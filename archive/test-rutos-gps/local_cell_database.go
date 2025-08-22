package main

import (
	"database/sql"
	"encoding/json"
	"fmt"
	"os"
	"strconv"
	"strings"
	"time"

	_ "github.com/mattn/go-sqlite3"
)

// LocalCellDatabase manages local storage of GPS + cell tower data
type LocalCellDatabase struct {
	db       *sql.DB
	dbPath   string
	lastSync time.Time
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
	TotalObservations  int     `json:"total_observations"`
	UniqueCells        int     `json:"unique_cells"`
	AverageGPSAccuracy float64 `json:"average_gps_accuracy"`
	BestGPSAccuracy    float64 `json:"best_gps_accuracy"`
	TimeSpan           string  `json:"time_span"`
	QualityScore       float64 `json:"quality_score"`
}

// NewLocalCellDatabase creates a new local cell database
func NewLocalCellDatabase(dbPath string) (*LocalCellDatabase, error) {
	db, err := sql.Open("sqlite3", dbPath)
	if err != nil {
		return nil, fmt.Errorf("failed to open database: %v", err)
	}

	lcd := &LocalCellDatabase{
		db:     db,
		dbPath: dbPath,
	}

	if err := lcd.initializeSchema(); err != nil {
		return nil, fmt.Errorf("failed to initialize schema: %v", err)
	}

	return lcd, nil
}

// initializeSchema creates the database tables
func (lcd *LocalCellDatabase) initializeSchema() error {
	schema := `
	CREATE TABLE IF NOT EXISTS cell_observations (
		id INTEGER PRIMARY KEY AUTOINCREMENT,
		timestamp DATETIME NOT NULL,
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
		contributed_at DATETIME,
		created_at DATETIME DEFAULT CURRENT_TIMESTAMP
	);
	
	CREATE INDEX IF NOT EXISTS idx_timestamp ON cell_observations(timestamp);
	CREATE INDEX IF NOT EXISTS idx_cell_id ON cell_observations(cell_id);
	CREATE INDEX IF NOT EXISTS idx_contributed ON cell_observations(contributed);
	CREATE INDEX IF NOT EXISTS idx_gps_accuracy ON cell_observations(gps_accuracy);
	
	CREATE TABLE IF NOT EXISTS contribution_log (
		id INTEGER PRIMARY KEY AUTOINCREMENT,
		date TEXT NOT NULL,
		observations_count INTEGER NOT NULL,
		unique_cells INTEGER NOT NULL,
		api_response TEXT,
		success BOOLEAN NOT NULL,
		created_at DATETIME DEFAULT CURRENT_TIMESTAMP
	);
	`

	_, err := lcd.db.Exec(schema)
	return err
}

// RecordObservation stores a new GPS + cell tower observation
func (lcd *LocalCellDatabase) RecordObservation(gps *GPSCoordinate, cell *CellularLocationIntelligence) error {
	// Only record high-quality observations
	if gps.Accuracy > 10.0 {
		return fmt.Errorf("GPS accuracy too poor (%.1fm) - not recording", gps.Accuracy)
	}

	if cell.SignalQuality.RSSI < -100 {
		return fmt.Errorf("signal too weak (%d dBm) - not recording", cell.SignalQuality.RSSI)
	}

	cellID, _ := parseIntFromString(cell.ServingCell.CellID)
	mcc, _ := parseIntFromString(cell.ServingCell.MCC)
	mnc, _ := parseIntFromString(cell.ServingCell.MNC)
	lac, _ := parseIntFromString(cell.ServingCell.TAC)

	query := `
	INSERT INTO cell_observations (
		timestamp, gps_latitude, gps_longitude, gps_accuracy, gps_source,
		cell_id, cell_mcc, cell_mnc, cell_lac, cell_technology,
		signal_rssi, signal_rsrp, signal_rsrq, signal_sinr
	) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
	`

	_, err := lcd.db.Exec(query,
		time.Now(), gps.Latitude, gps.Longitude, gps.Accuracy, gps.Source,
		cellID, mcc, mnc, lac, cell.NetworkInfo.Technology,
		cell.SignalQuality.RSSI, cell.SignalQuality.RSRP,
		cell.SignalQuality.RSRQ, cell.SignalQuality.SINR,
	)

	if err == nil {
		fmt.Printf("📊 Recorded observation: Cell %d at %.6f°,%.6f° (±%.1fm, %d dBm)\n",
			cellID, gps.Latitude, gps.Longitude, gps.Accuracy, cell.SignalQuality.RSSI)
	}

	return err
}

// GetDailyContributionBatch gets observations ready for contribution
func (lcd *LocalCellDatabase) GetDailyContributionBatch() (*DailyContributionBatch, error) {
	// Get uncontributed observations from the last 24 hours with good quality
	query := `
	SELECT id, timestamp, gps_latitude, gps_longitude, gps_accuracy, gps_source,
		   cell_id, cell_mcc, cell_mnc, cell_lac, cell_technology,
		   signal_rssi, signal_rsrp, signal_rsrq, signal_sinr
	FROM cell_observations 
	WHERE contributed = FALSE 
	  AND gps_accuracy <= 5.0 
	  AND signal_rssi > -95
	  AND timestamp >= datetime('now', '-24 hours')
	ORDER BY gps_accuracy ASC, signal_rssi DESC
	LIMIT 100
	`

	rows, err := lcd.db.Query(query)
	if err != nil {
		return nil, err
	}
	defer rows.Close()

	var observations []CellTowerObservation
	for rows.Next() {
		var obs CellTowerObservation
		err := rows.Scan(
			&obs.ID, &obs.Timestamp, &obs.GPS_Latitude, &obs.GPS_Longitude,
			&obs.GPS_Accuracy, &obs.GPS_Source, &obs.Cell_ID, &obs.Cell_MCC,
			&obs.Cell_MNC, &obs.Cell_LAC, &obs.Cell_Technology,
			&obs.Signal_RSSI, &obs.Signal_RSRP, &obs.Signal_RSRQ, &obs.Signal_SINR,
		)
		if err != nil {
			return nil, err
		}
		observations = append(observations, obs)
	}

	if len(observations) == 0 {
		return nil, fmt.Errorf("no observations ready for contribution")
	}

	// Create batch with summary
	batch := &DailyContributionBatch{
		Date:         time.Now().Format("2006-01-02"),
		Observations: observations,
		Summary:      lcd.calculateSummary(observations),
	}

	return batch, nil
}

// calculateSummary generates statistics for the batch
func (lcd *LocalCellDatabase) calculateSummary(observations []CellTowerObservation) ContributionSummary {
	if len(observations) == 0 {
		return ContributionSummary{}
	}

	uniqueCells := make(map[int]bool)
	totalAccuracy := 0.0
	bestAccuracy := observations[0].GPS_Accuracy
	var firstTime, lastTime time.Time

	for i, obs := range observations {
		uniqueCells[obs.Cell_ID] = true
		totalAccuracy += obs.GPS_Accuracy

		if obs.GPS_Accuracy < bestAccuracy {
			bestAccuracy = obs.GPS_Accuracy
		}

		if i == 0 {
			firstTime = obs.Timestamp
			lastTime = obs.Timestamp
		} else {
			if obs.Timestamp.Before(firstTime) {
				firstTime = obs.Timestamp
			}
			if obs.Timestamp.After(lastTime) {
				lastTime = obs.Timestamp
			}
		}
	}

	avgAccuracy := totalAccuracy / float64(len(observations))
	timeSpan := lastTime.Sub(firstTime).String()

	// Quality score: higher is better (based on accuracy and signal strength)
	qualityScore := (5.0 - avgAccuracy) / 5.0 * 100 // 0-100 scale
	if qualityScore < 0 {
		qualityScore = 0
	}

	return ContributionSummary{
		TotalObservations:  len(observations),
		UniqueCells:        len(uniqueCells),
		AverageGPSAccuracy: avgAccuracy,
		BestGPSAccuracy:    bestAccuracy,
		TimeSpan:           timeSpan,
		QualityScore:       qualityScore,
	}
}

// MarkAsContributed marks observations as contributed
func (lcd *LocalCellDatabase) MarkAsContributed(observationIDs []int) error {
	if len(observationIDs) == 0 {
		return nil
	}

	// Create placeholders for IN clause
	placeholders := strings.Repeat("?,", len(observationIDs)-1) + "?"
	query := fmt.Sprintf(`
		UPDATE cell_observations 
		SET contributed = TRUE, contributed_at = CURRENT_TIMESTAMP 
		WHERE id IN (%s)
	`, placeholders)

	// Convert IDs to interface{} slice
	args := make([]interface{}, len(observationIDs))
	for i, id := range observationIDs {
		args[i] = id
	}

	_, err := lcd.db.Exec(query, args...)
	return err
}

// LogContribution records a contribution attempt
func (lcd *LocalCellDatabase) LogContribution(batch *DailyContributionBatch, success bool, apiResponse string) error {
	query := `
	INSERT INTO contribution_log (date, observations_count, unique_cells, api_response, success)
	VALUES (?, ?, ?, ?, ?)
	`

	_, err := lcd.db.Exec(query, batch.Date, batch.Summary.TotalObservations,
		batch.Summary.UniqueCells, apiResponse, success)
	return err
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

	// Pending observations
	var pendingObs int
	err = lcd.db.QueryRow("SELECT COUNT(*) FROM cell_observations WHERE contributed = FALSE").Scan(&pendingObs)
	if err != nil {
		return nil, err
	}
	stats["pending_observations"] = pendingObs

	// Unique cells observed
	var uniqueCells int
	err = lcd.db.QueryRow("SELECT COUNT(DISTINCT cell_id) FROM cell_observations").Scan(&uniqueCells)
	if err != nil {
		return nil, err
	}
	stats["unique_cells"] = uniqueCells

	// Average GPS accuracy
	var avgAccuracy float64
	err = lcd.db.QueryRow("SELECT AVG(gps_accuracy) FROM cell_observations").Scan(&avgAccuracy)
	if err != nil {
		return nil, err
	}
	stats["avg_gps_accuracy"] = avgAccuracy

	// Best GPS accuracy
	var bestAccuracy float64
	err = lcd.db.QueryRow("SELECT MIN(gps_accuracy) FROM cell_observations").Scan(&bestAccuracy)
	if err != nil {
		return nil, err
	}
	stats["best_gps_accuracy"] = bestAccuracy

	// Date range
	var firstDate, lastDate string
	err = lcd.db.QueryRow("SELECT MIN(timestamp), MAX(timestamp) FROM cell_observations").Scan(&firstDate, &lastDate)
	if err == nil {
		stats["date_range"] = fmt.Sprintf("%s to %s", firstDate, lastDate)
	}

	// Successful contributions
	var successfulContrib int
	err = lcd.db.QueryRow("SELECT COUNT(*) FROM contribution_log WHERE success = TRUE").Scan(&successfulContrib)
	if err != nil {
		return nil, err
	}
	stats["successful_contributions"] = successfulContrib

	return stats, nil
}

// CleanupOldData removes old observations to keep database size manageable
func (lcd *LocalCellDatabase) CleanupOldData(daysToKeep int) error {
	query := `
	DELETE FROM cell_observations 
	WHERE contributed = TRUE 
	  AND timestamp < datetime('now', '-' || ? || ' days')
	`

	result, err := lcd.db.Exec(query, daysToKeep)
	if err != nil {
		return err
	}

	rowsAffected, _ := result.RowsAffected()
	if rowsAffected > 0 {
		fmt.Printf("🧹 Cleaned up %d old observations (>%d days)\n", rowsAffected, daysToKeep)
	}

	return nil
}

// Close closes the database connection
func (lcd *LocalCellDatabase) Close() error {
	return lcd.db.Close()
}

// parseIntFromString safely parses integer from string
func parseIntFromString(s string) (int, error) {
	if s == "" {
		return 0, fmt.Errorf("empty string")
	}

	// Handle potential hex values
	if strings.HasPrefix(s, "0x") || len(s) > 6 {
		// Try parsing as hex first
		if val, err := parseHexToInt(s); err == nil {
			return val, nil
		}
	}

	// Parse as decimal
	return parseIntLocal(s)
}

// Helper functions (you'd implement these based on your existing parsing code)
func parseHexToInt(s string) (int, error) {
	// Implementation depends on your existing hex parsing
	return 0, fmt.Errorf("not implemented")
}

func parseIntLocal(s string) (int, error) {
	// Use standard library for parsing
	return strconv.Atoi(s)
}

// testLocalCellDatabase demonstrates the local database functionality
func testLocalCellDatabase() error {
	fmt.Println("🗄️  TESTING LOCAL CELL TOWER DATABASE")
	fmt.Println("=" + strings.Repeat("=", 40))

	// Create test database
	dbPath := "test_cell_observations.db"
	defer os.Remove(dbPath) // Clean up after test

	db, err := NewLocalCellDatabase(dbPath)
	if err != nil {
		return fmt.Errorf("failed to create database: %v", err)
	}
	defer db.Close()

	fmt.Println("✅ Database created and initialized")

	// Create test data
	gps := &GPSCoordinate{
		Latitude:  59.48007000,
		Longitude: 18.27985000,
		Accuracy:  0.4,
		Source:    "quectel_multi_gnss",
	}

	cell := createHardcodedCellularData() // Use your existing function

	// Record some observations
	fmt.Println("\n📊 Recording test observations...")
	for i := 0; i < 5; i++ {
		// Slightly vary the GPS coordinates to simulate movement
		testGPS := *gps
		testGPS.Latitude += float64(i) * 0.0001
		testGPS.Longitude += float64(i) * 0.0001

		if err := db.RecordObservation(&testGPS, cell); err != nil {
			fmt.Printf("❌ Failed to record observation %d: %v\n", i+1, err)
		}
	}

	// Get statistics
	fmt.Println("\n📊 Database Statistics:")
	stats, err := db.GetStatistics()
	if err != nil {
		return fmt.Errorf("failed to get statistics: %v", err)
	}

	for key, value := range stats {
		fmt.Printf("  %s: %v\n", key, value)
	}

	// Get contribution batch
	fmt.Println("\n📤 Getting contribution batch...")
	batch, err := db.GetDailyContributionBatch()
	if err != nil {
		fmt.Printf("❌ No batch ready: %v\n", err)
	} else {
		fmt.Printf("✅ Batch ready: %d observations, %d unique cells\n",
			batch.Summary.TotalObservations, batch.Summary.UniqueCells)
		fmt.Printf("   📊 Quality Score: %.1f/100\n", batch.Summary.QualityScore)
		fmt.Printf("   🎯 Best GPS Accuracy: %.1fm\n", batch.Summary.BestGPSAccuracy)

		// Save batch to JSON for inspection
		batchJSON, _ := json.MarshalIndent(batch, "", "  ")
		filename := fmt.Sprintf("contribution_batch_%s.json", batch.Date)
		if err := os.WriteFile(filename, batchJSON, 0o644); err == nil {
			fmt.Printf("   💾 Batch saved to: %s\n", filename)
		}
	}

	fmt.Println("\n💡 LOCAL DATABASE BENEFITS:")
	fmt.Println("   📊 Efficient data collection (continuous recording)")
	fmt.Println("   🎯 Quality filtering (only high-accuracy data)")
	fmt.Println("   📤 Batch contributions (once daily)")
	fmt.Println("   💰 Minimal API usage (1 request/day for contribution)")
	fmt.Println("   📈 Historical tracking (contribution success/failure)")
	fmt.Println("   🧹 Automatic cleanup (old data removal)")

	return nil
}
