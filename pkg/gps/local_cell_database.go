package gps

import (
	"database/sql"
	"fmt"
	"os"
	"path/filepath"
	"strconv"
	"time"

	_ "github.com/mattn/go-sqlite3"
	"github.com/markus-lassfolk/autonomy/pkg/logx"
)

// LocalCellDatabase manages local storage of GPS + cell tower data
type LocalCellDatabase struct {
	db     *sql.DB
	dbPath string
	logger *logx.Logger
	config *LocalCellDatabaseConfig
}

// LocalCellDatabaseConfig holds configuration for local cell database
type LocalCellDatabaseConfig struct {
	DatabasePath         string        `json:"database_path"`
	MaxObservations      int           `json:"max_observations"`
	RetentionDays        int           `json:"retention_days"`
	AutoContribute       bool          `json:"auto_contribute"`
	ContributionInterval time.Duration `json:"contribution_interval"`
	MinAccuracy          float64       `json:"min_accuracy"`
}

// CellTowerObservation represents a single GPS + cell tower measurement
type CellTowerObservation struct {
	ID              int        `json:"id"`
	Timestamp       time.Time  `json:"timestamp"`
	GPS_Latitude    float64    `json:"gps_latitude"`
	GPS_Longitude   float64    `json:"gps_longitude"`
	GPS_Accuracy    float64    `json:"gps_accuracy"`
	GPS_Source      string     `json:"gps_source"`
	Cell_ID         int        `json:"cell_id"`
	Cell_MCC        int        `json:"cell_mcc"`
	Cell_MNC        int        `json:"cell_mnc"`
	Cell_LAC        int        `json:"cell_lac"`
	Cell_Technology string     `json:"cell_technology"`
	Signal_RSSI     int        `json:"signal_rssi"`
	Signal_RSRP     int        `json:"signal_rsrp"`
	Signal_RSRQ     int        `json:"signal_rsrq"`
	Signal_SINR     int        `json:"signal_sinr"`
	Contributed     bool       `json:"contributed"`
	ContributedAt   *time.Time `json:"contributed_at,omitempty"`
}

// DailyContributionBatch represents a batch of observations to contribute
type DailyContributionBatch struct {
	Date         string                 `json:"date"`
	Observations []CellTowerObservation `json:"observations"`
	Summary      ContributionSummary    `json:"summary"`
}

// ContributionSummary provides statistics about the batch
type ContributionSummary struct {
	TotalObservations int     `json:"total_observations"`
	UniqueCells       int     `json:"unique_cells"`
	AverageAccuracy   float64 `json:"average_accuracy"`
	DateRange         string  `json:"date_range"`
}

// NewLocalCellDatabase creates a new local cell database
func NewLocalCellDatabase(config *LocalCellDatabaseConfig, logger *logx.Logger) (*LocalCellDatabase, error) {
	if config == nil {
		config = &LocalCellDatabaseConfig{
			DatabasePath:         "/tmp/autonomy_cell_observations.db",
			MaxObservations:      10000,
			RetentionDays:        30,
			AutoContribute:       false,
			ContributionInterval: 24 * time.Hour,
			MinAccuracy:          100.0, // Only store observations with accuracy better than 100m
		}
	}

	// Ensure directory exists
	dir := filepath.Dir(config.DatabasePath)
	if err := os.MkdirAll(dir, 0o755); err != nil {
		return nil, fmt.Errorf("failed to create database directory: %w", err)
	}

	db, err := sql.Open("sqlite3", config.DatabasePath)
	if err != nil {
		return nil, fmt.Errorf("failed to open database: %w", err)
	}

	lcd := &LocalCellDatabase{
		db:     db,
		dbPath: config.DatabasePath,
		logger: logger,
		config: config,
	}

	if err := lcd.initializeDatabase(); err != nil {
		return nil, fmt.Errorf("failed to initialize database: %w", err)
	}

	logger.Info("local_cell_database_initialized",
		"database_path", config.DatabasePath,
		"max_observations", config.MaxObservations,
		"retention_days", config.RetentionDays,
	)

	return lcd, nil
}

// initializeDatabase creates the necessary tables
func (lcd *LocalCellDatabase) initializeDatabase() error {
	createTableSQL := `
	CREATE TABLE IF NOT EXISTS cell_observations (
		id INTEGER PRIMARY KEY AUTOINCREMENT,
		timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
		gps_latitude REAL NOT NULL,
		gps_longitude REAL NOT NULL,
		gps_accuracy REAL NOT NULL,
		gps_source TEXT NOT NULL,
		cell_id INTEGER NOT NULL,
		cell_mcc INTEGER NOT NULL,
		cell_mnc INTEGER NOT NULL,
		cell_lac INTEGER NOT NULL,
		cell_technology TEXT NOT NULL,
		signal_rssi INTEGER,
		signal_rsrp INTEGER,
		signal_rsrq INTEGER,
		signal_sinr INTEGER,
		contributed BOOLEAN DEFAULT FALSE,
		contributed_at DATETIME
	);

	CREATE INDEX IF NOT EXISTS idx_cell_observations_timestamp ON cell_observations(timestamp);
	CREATE INDEX IF NOT EXISTS idx_cell_observations_cell ON cell_observations(cell_id, cell_mcc, cell_mnc, cell_lac);
	CREATE INDEX IF NOT EXISTS idx_cell_observations_contributed ON cell_observations(contributed);
	`

	_, err := lcd.db.Exec(createTableSQL)
	return err
}

// StoreObservation stores a GPS + cell tower observation
func (lcd *LocalCellDatabase) StoreObservation(gpsData *StandardizedGPSData, servingCell *ServingCellInfo) error {
	// Check if GPS data meets quality requirements
	if gpsData.Accuracy > lcd.config.MinAccuracy {
		lcd.logger.LogDebugVerbose("cell_observation_skipped", map[string]interface{}{
			"reason":       "accuracy_too_low",
			"accuracy":     gpsData.Accuracy,
			"min_required": lcd.config.MinAccuracy,
		})
		return nil
	}

	if servingCell == nil {
		return fmt.Errorf("no serving cell information available")
	}

	// Parse cell data
	cellID, _ := strconv.Atoi(servingCell.CellID)
	mcc, _ := strconv.Atoi(servingCell.MCC)
	mnc, _ := strconv.Atoi(servingCell.MNC)
	lac, _ := strconv.Atoi(servingCell.TAC)

	insertSQL := `
	INSERT INTO cell_observations (
		gps_latitude, gps_longitude, gps_accuracy, gps_source,
		cell_id, cell_mcc, cell_mnc, cell_lac, cell_technology,
		signal_rssi, signal_rsrp, signal_rsrq, signal_sinr
	) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
	`

	_, err := lcd.db.Exec(insertSQL,
		gpsData.Latitude, gpsData.Longitude, gpsData.Accuracy, gpsData.Source,
		cellID, mcc, mnc, lac, servingCell.Technology,
		servingCell.RSSI, servingCell.RSRP, servingCell.RSRQ, servingCell.SINR,
	)
	if err != nil {
		lcd.logger.Error("failed to store cell observation", "error", err)
		return err
	}

	lcd.logger.LogDebugVerbose("cell_observation_stored", map[string]interface{}{
		"gps_accuracy": gpsData.Accuracy,
		"gps_source":   gpsData.Source,
		"cell_id":      cellID,
		"mcc":          mcc,
		"mnc":          mnc,
	})

	// Perform maintenance if needed
	go lcd.performMaintenance()

	return nil
}

// GetObservationsForCell retrieves observations for a specific cell
func (lcd *LocalCellDatabase) GetObservationsForCell(cellID, mcc, mnc, lac int) ([]CellTowerObservation, error) {
	query := `
	SELECT id, timestamp, gps_latitude, gps_longitude, gps_accuracy, gps_source,
		   cell_id, cell_mcc, cell_mnc, cell_lac, cell_technology,
		   signal_rssi, signal_rsrp, signal_rsrq, signal_sinr,
		   contributed, contributed_at
	FROM cell_observations 
	WHERE cell_id = ? AND cell_mcc = ? AND cell_mnc = ? AND cell_lac = ?
	ORDER BY timestamp DESC
	LIMIT 100
	`

	rows, err := lcd.db.Query(query, cellID, mcc, mnc, lac)
	if err != nil {
		return nil, err
	}
	defer rows.Close()

	var observations []CellTowerObservation
	for rows.Next() {
		var obs CellTowerObservation
		var contributedAt sql.NullTime

		err := rows.Scan(
			&obs.ID, &obs.Timestamp, &obs.GPS_Latitude, &obs.GPS_Longitude,
			&obs.GPS_Accuracy, &obs.GPS_Source, &obs.Cell_ID, &obs.Cell_MCC,
			&obs.Cell_MNC, &obs.Cell_LAC, &obs.Cell_Technology,
			&obs.Signal_RSSI, &obs.Signal_RSRP, &obs.Signal_RSRQ, &obs.Signal_SINR,
			&obs.Contributed, &contributedAt,
		)
		if err != nil {
			continue
		}

		if contributedAt.Valid {
			obs.ContributedAt = &contributedAt.Time
		}

		observations = append(observations, obs)
	}

	return observations, nil
}

// EstimateLocationFromCell estimates location based on stored observations for a cell
func (lcd *LocalCellDatabase) EstimateLocationFromCell(cellID, mcc, mnc, lac int) (*CellTowerLocation, error) {
	observations, err := lcd.GetObservationsForCell(cellID, mcc, mnc, lac)
	if err != nil {
		return nil, err
	}

	if len(observations) == 0 {
		return nil, fmt.Errorf("no observations found for cell %d", cellID)
	}

	// Calculate weighted average location based on GPS accuracy
	var totalWeight float64
	var weightedLat, weightedLon float64

	for _, obs := range observations {
		// Weight inversely proportional to accuracy (better accuracy = higher weight)
		weight := 1.0 / (obs.GPS_Accuracy + 1.0)
		totalWeight += weight
		weightedLat += obs.GPS_Latitude * weight
		weightedLon += obs.GPS_Longitude * weight
	}

	avgLat := weightedLat / totalWeight
	avgLon := weightedLon / totalWeight

	// Calculate estimated accuracy as standard deviation
	var variance float64
	for _, obs := range observations {
		distance := calculateDistance(avgLat, avgLon, obs.GPS_Latitude, obs.GPS_Longitude)
		variance += distance * distance
	}
	estimatedAccuracy := (variance / float64(len(observations)))

	return &CellTowerLocation{
		Latitude:    avgLat,
		Longitude:   avgLon,
		Accuracy:    estimatedAccuracy,
		Source:      "local_database",
		Method:      "weighted_average",
		Confidence:  calculateConfidenceFromObservations(observations),
		Valid:       true,
		CollectedAt: time.Now(),
		CellCount:   1,
	}, nil
}

// GetUncontributedObservations gets observations that haven't been contributed yet
func (lcd *LocalCellDatabase) GetUncontributedObservations(limit int) ([]CellTowerObservation, error) {
	query := `
	SELECT id, timestamp, gps_latitude, gps_longitude, gps_accuracy, gps_source,
		   cell_id, cell_mcc, cell_mnc, cell_lac, cell_technology,
		   signal_rssi, signal_rsrp, signal_rsrq, signal_sinr,
		   contributed, contributed_at
	FROM cell_observations 
	WHERE contributed = FALSE
	ORDER BY timestamp ASC
	LIMIT ?
	`

	rows, err := lcd.db.Query(query, limit)
	if err != nil {
		return nil, err
	}
	defer rows.Close()

	var observations []CellTowerObservation
	for rows.Next() {
		var obs CellTowerObservation
		var contributedAt sql.NullTime

		err := rows.Scan(
			&obs.ID, &obs.Timestamp, &obs.GPS_Latitude, &obs.GPS_Longitude,
			&obs.GPS_Accuracy, &obs.GPS_Source, &obs.Cell_ID, &obs.Cell_MCC,
			&obs.Cell_MNC, &obs.Cell_LAC, &obs.Cell_Technology,
			&obs.Signal_RSSI, &obs.Signal_RSRP, &obs.Signal_RSRQ, &obs.Signal_SINR,
			&obs.Contributed, &contributedAt,
		)
		if err != nil {
			continue
		}

		observations = append(observations, obs)
	}

	return observations, nil
}

// MarkAsContributed marks observations as contributed
func (lcd *LocalCellDatabase) MarkAsContributed(observationIDs []int) error {
	if len(observationIDs) == 0 {
		return nil
	}

	// Build placeholders for IN clause
	placeholders := make([]string, len(observationIDs))
	args := make([]interface{}, len(observationIDs))
	for i, id := range observationIDs {
		placeholders[i] = "?"
		args[i] = id
	}

	query := fmt.Sprintf(`
	UPDATE cell_observations 
	SET contributed = TRUE, contributed_at = CURRENT_TIMESTAMP 
	WHERE id IN (%s)
	`, fmt.Sprintf("%s", placeholders))

	_, err := lcd.db.Exec(query, args...)
	if err != nil {
		lcd.logger.Error("failed to mark observations as contributed", "error", err)
		return err
	}

	lcd.logger.Info("marked_observations_as_contributed", "count", len(observationIDs))
	return nil
}

// GetStatistics returns database statistics
func (lcd *LocalCellDatabase) GetStatistics() (map[string]interface{}, error) {
	stats := make(map[string]interface{})

	// Total observations
	var totalObs int
	err := lcd.db.QueryRow("SELECT COUNT(*) FROM cell_observations").Scan(&totalObs)
	if err != nil {
		return nil, err
	}
	stats["total_observations"] = totalObs

	// Contributed observations
	var contributedObs int
	err = lcd.db.QueryRow("SELECT COUNT(*) FROM cell_observations WHERE contributed = TRUE").Scan(&contributedObs)
	if err != nil {
		return nil, err
	}
	stats["contributed_observations"] = contributedObs

	// Unique cells
	var uniqueCells int
	err = lcd.db.QueryRow("SELECT COUNT(DISTINCT cell_id || '-' || cell_mcc || '-' || cell_mnc || '-' || cell_lac) FROM cell_observations").Scan(&uniqueCells)
	if err != nil {
		return nil, err
	}
	stats["unique_cells"] = uniqueCells

	// Average accuracy
	var avgAccuracy float64
	err = lcd.db.QueryRow("SELECT AVG(gps_accuracy) FROM cell_observations").Scan(&avgAccuracy)
	if err != nil {
		return nil, err
	}
	stats["average_accuracy"] = avgAccuracy

	// Date range
	var oldestDate, newestDate string
	err = lcd.db.QueryRow("SELECT MIN(timestamp), MAX(timestamp) FROM cell_observations").Scan(&oldestDate, &newestDate)
	if err != nil {
		return nil, err
	}
	stats["date_range"] = fmt.Sprintf("%s to %s", oldestDate, newestDate)

	return stats, nil
}

// performMaintenance performs database maintenance tasks
func (lcd *LocalCellDatabase) performMaintenance() {
	// Clean old observations if over limit
	var count int
	if err := lcd.db.QueryRow("SELECT COUNT(*) FROM cell_observations").Scan(&count); err != nil {
		lcd.logger.Warn("Failed to count observations for maintenance", "error", err)
		return
	}

	if count > lcd.config.MaxObservations {
		deleteCount := count - lcd.config.MaxObservations
		_, err := lcd.db.Exec(`
			DELETE FROM cell_observations 
			WHERE id IN (
				SELECT id FROM cell_observations 
				ORDER BY timestamp ASC 
				LIMIT ?
			)
		`, deleteCount)

		if err == nil {
			lcd.logger.Info("database_maintenance_cleanup",
				"deleted_observations", deleteCount,
				"remaining_observations", lcd.config.MaxObservations,
			)
		}
	}

	// Clean observations older than retention period
	cutoffDate := time.Now().AddDate(0, 0, -lcd.config.RetentionDays)
	result, err := lcd.db.Exec("DELETE FROM cell_observations WHERE timestamp < ?", cutoffDate)
	if err == nil {
		if rowsAffected, _ := result.RowsAffected(); rowsAffected > 0 {
			lcd.logger.Info("database_maintenance_retention",
				"deleted_old_observations", rowsAffected,
				"cutoff_date", cutoffDate,
			)
		}
	}
}

// Close closes the database connection
func (lcd *LocalCellDatabase) Close() error {
	if lcd.db != nil {
		return lcd.db.Close()
	}
	return nil
}

// Helper functions

func calculateDistance(lat1, lon1, lat2, lon2 float64) float64 {
	const R = 6371000 // Earth's radius in meters

	lat1Rad := lat1 * (3.14159265359 / 180)
	lat2Rad := lat2 * (3.14159265359 / 180)
	deltaLatRad := (lat2 - lat1) * (3.14159265359 / 180)
	deltaLonRad := (lon2 - lon1) * (3.14159265359 / 180)

	a := (deltaLatRad/2)*(deltaLatRad/2) +
		(deltaLonRad/2)*(deltaLonRad/2)*
			(lat1Rad)*(lat2Rad)
	c := 2 * (a * (1 - a))

	return R * c
}

func calculateConfidenceFromObservations(observations []CellTowerObservation) float64 {
	if len(observations) == 0 {
		return 0.0
	}

	// Base confidence on number of observations and their accuracy
	obsCount := float64(len(observations))

	// Calculate average accuracy
	var totalAccuracy float64
	for _, obs := range observations {
		totalAccuracy += obs.GPS_Accuracy
	}
	avgAccuracy := totalAccuracy / obsCount

	// Confidence increases with more observations and better accuracy
	countFactor := 1.0 - (1.0 / (1.0 + obsCount/10.0)) // Asymptotic to 1.0
	accuracyFactor := 1.0 - (avgAccuracy / 1000.0)     // Decreases with worse accuracy

	if accuracyFactor < 0 {
		accuracyFactor = 0
	}

	confidence := (countFactor + accuracyFactor) / 2.0
	if confidence > 1.0 {
		confidence = 1.0
	}

	return confidence
}
