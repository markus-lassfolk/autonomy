package gps

import (
	"context"
	"fmt"
	"os/exec"
	"strconv"
	"strings"
	"time"

	"github.com/markus-lassfolk/autonomy/pkg/logx"
)

// QuectelGPSSource implements GPS data collection from Quectel modems
type QuectelGPSSource struct {
	logger *logx.Logger
	config *QuectelGPSConfig
}

// QuectelGPSConfig holds configuration for Quectel GPS
type QuectelGPSConfig struct {
	Enabled        bool          `json:"enabled"`
	Timeout        time.Duration `json:"timeout"`
	RetryAttempts  int           `json:"retry_attempts"`
	RetryDelay     time.Duration `json:"retry_delay"`
	MinSatellites  int           `json:"min_satellites"`
	MaxHDOP        float64       `json:"max_hdop"`
	ValidateCoords bool          `json:"validate_coords"`
}

// QuectelGPSData represents GPS data from Quectel modem
type QuectelGPSData struct {
	Latitude    float64   `json:"latitude"`
	Longitude   float64   `json:"longitude"`
	Altitude    float64   `json:"altitude"`
	SpeedKmh    float64   `json:"speed_kmh"`
	SpeedKnots  float64   `json:"speed_knots"`
	SpeedMs     float64   `json:"speed_ms"` // Speed in m/s
	Course      float64   `json:"course"`
	Satellites  int       `json:"satellites"`
	HDOP        float64   `json:"hdop"`
	FixType     int       `json:"fix_type"`
	FixQuality  string    `json:"fix_quality"`
	Time        string    `json:"time"`
	Date        string    `json:"date"`
	Valid       bool      `json:"valid"`
	Source      string    `json:"source"`
	RawData     string    `json:"raw_data"`
	CollectedAt time.Time `json:"collected_at"`
	Confidence  float64   `json:"confidence"`
}

// DefaultQuectelGPSConfig returns default configuration
func DefaultQuectelGPSConfig() *QuectelGPSConfig {
	return &QuectelGPSConfig{
		Enabled:        true,
		Timeout:        10 * time.Second,
		RetryAttempts:  3,
		RetryDelay:     2 * time.Second,
		MinSatellites:  4,
		MaxHDOP:        5.0,
		ValidateCoords: true,
	}
}

// NewQuectelGPSSource creates a new Quectel GPS source
func NewQuectelGPSSource(config *QuectelGPSConfig, logger *logx.Logger) *QuectelGPSSource {
	if config == nil {
		config = DefaultQuectelGPSConfig()
	}

	return &QuectelGPSSource{
		logger: logger,
		config: config,
	}
}

// IsAvailable checks if Quectel GPS is available
func (q *QuectelGPSSource) IsAvailable(ctx context.Context) bool {
	if !q.config.Enabled {
		q.logger.Debug("Quectel GPS disabled in configuration")
		return false
	}

	// Check if gsmctl command is available
	if _, err := exec.LookPath("gsmctl"); err != nil {
		q.logger.Debug("Quectel GPS not available: gsmctl command not found")
		return false
	}

	// Test basic AT command
	ctx, cancel := context.WithTimeout(ctx, 5*time.Second)
	defer cancel()

	cmd := exec.CommandContext(ctx, "gsmctl", "-A", "AT")
	if err := cmd.Run(); err != nil {
		q.logger.Debug("Quectel GPS not available: AT command failed", "error", err)
		return false
	}

	q.logger.Debug("Quectel GPS source is available")
	return true
}

// CollectGPS collects GPS data from Quectel modem
func (q *QuectelGPSSource) CollectGPS(ctx context.Context) (*StandardizedGPSData, error) {
	if !q.IsAvailable(ctx) {
		return nil, fmt.Errorf("Quectel GPS source not available")
	}

	var lastErr error
	for attempt := 1; attempt <= q.config.RetryAttempts; attempt++ {
		q.logger.Debug("Collecting Quectel GPS data", "attempt", attempt)

		quectelData, err := q.collectQuectelGPSData(ctx)
		if err != nil {
			lastErr = err
			q.logger.Warn("Quectel GPS collection failed", "attempt", attempt, "error", err)

			if attempt < q.config.RetryAttempts {
				select {
				case <-ctx.Done():
					return nil, ctx.Err()
				case <-time.After(q.config.RetryDelay):
					continue
				}
			}
			continue
		}

		// Convert to standardized format
		standardized := q.convertToStandardized(quectelData)

		// Validate the data
		if err := q.validateGPSData(standardized); err != nil {
			lastErr = err
			q.logger.Warn("Quectel GPS data validation failed", "attempt", attempt, "error", err)
			continue
		}

		q.logger.Info("Quectel GPS data collected successfully",
			"latitude", standardized.Latitude,
			"longitude", standardized.Longitude,
			"satellites", standardized.Satellites,
			"fix_type", standardized.FixType,
			"confidence", standardized.Confidence)

		return standardized, nil
	}

	return nil, fmt.Errorf("failed to collect valid Quectel GPS data after %d attempts: %v", q.config.RetryAttempts, lastErr)
}

// collectQuectelGPSData executes the Quectel GPS command and parses the response
func (q *QuectelGPSSource) collectQuectelGPSData(ctx context.Context) (*QuectelGPSData, error) {
	ctx, cancel := context.WithTimeout(ctx, q.config.Timeout)
	defer cancel()

	// Execute AT+QGPSLOC=2 command
	cmd := exec.CommandContext(ctx, "gsmctl", "-A", "AT+QGPSLOC=2")
	output, err := cmd.Output()
	if err != nil {
		return nil, fmt.Errorf("QGPSLOC command failed: %w", err)
	}

	outputStr := string(output)
	q.logger.Debug("Quectel GPS raw response", "output", strings.TrimSpace(outputStr))

	// Parse the response
	gpsData := q.parseQGPSLOC(outputStr)
	if gpsData == nil {
		return nil, fmt.Errorf("failed to parse QGPSLOC response")
	}

	return gpsData, nil
}

// parseQGPSLOC parses Quectel QGPSLOC response
func (q *QuectelGPSSource) parseQGPSLOC(response string) *QuectelGPSData {
	gpsData := &QuectelGPSData{
		Source:      "quectel_gsm_gps",
		CollectedAt: time.Now(),
		RawData:     response,
	}

	// Find the QGPSLOC line
	lines := strings.Split(response, "\n")
	var qgpslocLine string
	for _, line := range lines {
		if strings.Contains(line, "+QGPSLOC:") {
			qgpslocLine = line
			break
		}
	}

	if qgpslocLine == "" {
		q.logger.Debug("No QGPSLOC line found in response")
		return gpsData
	}

	// Parse: +QGPSLOC: time,lat,lon,hdop,altitude,fix,cog,spkm,spkn,date,nsat
	// Example: +QGPSLOC: 001047.00,59.48007,18.27985,0.4,9.5,3,,0.0,0.0,160825,39

	// Remove the "+QGPSLOC: " prefix and clean up whitespace/control characters
	dataStr := strings.TrimPrefix(qgpslocLine, "+QGPSLOC: ")
	dataStr = strings.TrimSpace(dataStr) // Remove \r\n and other whitespace
	parts := strings.Split(dataStr, ",")

	if len(parts) < 11 {
		q.logger.Debug("Insufficient QGPSLOC data fields", "fields", len(parts), "expected", 11)
		return gpsData
	}

	// Parse each field
	gpsData.Time = parts[0]

	if lat, err := strconv.ParseFloat(parts[1], 64); err == nil {
		gpsData.Latitude = lat
	}

	if lon, err := strconv.ParseFloat(parts[2], 64); err == nil {
		gpsData.Longitude = lon
	}

	if hdop, err := strconv.ParseFloat(parts[3], 64); err == nil {
		gpsData.HDOP = hdop
	}

	if alt, err := strconv.ParseFloat(parts[4], 64); err == nil {
		gpsData.Altitude = alt
	}

	if fix, err := strconv.Atoi(parts[5]); err == nil {
		gpsData.FixType = fix
		gpsData.FixQuality = q.getFixTypeString(fix)
	}

	if parts[6] != "" {
		if course, err := strconv.ParseFloat(parts[6], 64); err == nil {
			gpsData.Course = course
		}
	}

	if spkm, err := strconv.ParseFloat(parts[7], 64); err == nil {
		gpsData.SpeedKmh = spkm
		gpsData.SpeedMs = spkm / 3.6 // Convert km/h to m/s
	}

	if spkn, err := strconv.ParseFloat(parts[8], 64); err == nil {
		gpsData.SpeedKnots = spkn
	}

	gpsData.Date = parts[9]

	if sats, err := strconv.Atoi(strings.TrimSpace(parts[10])); err == nil {
		gpsData.Satellites = sats
	}

	// Determine if GPS fix is valid
	gpsData.Valid = gpsData.FixType >= 2 && // 2D or 3D fix
		gpsData.Latitude != 0 &&
		gpsData.Longitude != 0 &&
		gpsData.Satellites > 0

	// Calculate confidence
	gpsData.Confidence = q.calculateConfidence(gpsData)

	return gpsData
}

// convertToStandardized converts QuectelGPSData to StandardizedGPSData
func (q *QuectelGPSSource) convertToStandardized(quectel *QuectelGPSData) *StandardizedGPSData {
	// Convert Quectel fix type to standard fix type (0-3)
	standardFixType := 0
	if quectel.FixType >= 2 {
		if quectel.FixType == 2 {
			standardFixType = 2 // 2D fix
		} else if quectel.FixType >= 3 {
			standardFixType = 3 // 3D fix
		}
	}

	return &StandardizedGPSData{
		Latitude:   quectel.Latitude,
		Longitude:  quectel.Longitude,
		Altitude:   quectel.Altitude,
		Accuracy:   q.calculateAccuracy(quectel.HDOP),
		Speed:      quectel.SpeedMs, // Already in m/s
		Course:     quectel.Course,
		Satellites: quectel.Satellites,
		HDOP:       quectel.HDOP,
		FixType:    standardFixType,
		FixQuality: quectel.FixQuality,
		Source:     "quectel",
		Timestamp:  quectel.CollectedAt,
		Valid:      quectel.Valid,
		Confidence: quectel.Confidence,
	}
}

// calculateAccuracy estimates accuracy from HDOP
func (q *QuectelGPSSource) calculateAccuracy(hdop float64) float64 {
	if hdop <= 0 {
		return 50.0 // Default accuracy
	}
	// Rough estimate: accuracy = HDOP * 5 meters
	accuracy := hdop * 5.0
	if accuracy < 1.0 {
		accuracy = 1.0
	}
	if accuracy > 100.0 {
		accuracy = 100.0
	}
	return accuracy
}

// calculateConfidence calculates confidence score for Quectel GPS data
func (q *QuectelGPSSource) calculateConfidence(data *QuectelGPSData) float64 {
	if !data.Valid {
		return 0.0
	}

	confidence := 0.0

	// Base confidence from fix type
	switch data.FixType {
	case 0, 1:
		confidence = 0.0 // No fix or dead reckoning
	case 2:
		confidence = 0.6 // 2D fix
	case 3:
		confidence = 0.8 // 3D fix
	case 4:
		confidence = 0.9 // GNSS + Dead Reckoning
	case 5:
		confidence = 0.3 // Time only fix
	default:
		confidence = 0.5 // Unknown fix type
	}

	// Adjust based on satellite count
	if data.Satellites >= 8 {
		confidence += 0.1
	} else if data.Satellites >= 6 {
		confidence += 0.05
	} else if data.Satellites < 4 {
		confidence -= 0.2
	}

	// Adjust based on HDOP (lower is better)
	if data.HDOP > 0 {
		if data.HDOP <= 1.0 {
			confidence += 0.1
		} else if data.HDOP <= 2.0 {
			confidence += 0.05
		} else if data.HDOP > 5.0 {
			confidence -= 0.2
		}
	}

	// Ensure confidence is within bounds
	if confidence < 0.0 {
		confidence = 0.0
	}
	if confidence > 1.0 {
		confidence = 1.0
	}

	return confidence
}

// validateGPSData validates the collected GPS data
func (q *QuectelGPSSource) validateGPSData(data *StandardizedGPSData) error {
	if !data.Valid {
		return fmt.Errorf("GPS data marked as invalid")
	}

	if q.config.ValidateCoords {
		if data.Latitude == 0 && data.Longitude == 0 {
			return fmt.Errorf("invalid coordinates (0,0)")
		}

		if data.Latitude < -90 || data.Latitude > 90 {
			return fmt.Errorf("invalid latitude: %f", data.Latitude)
		}

		if data.Longitude < -180 || data.Longitude > 180 {
			return fmt.Errorf("invalid longitude: %f", data.Longitude)
		}
	}

	if data.Satellites < q.config.MinSatellites {
		return fmt.Errorf("insufficient satellites: %d < %d", data.Satellites, q.config.MinSatellites)
	}

	if data.HDOP > q.config.MaxHDOP {
		return fmt.Errorf("HDOP too high: %f > %f", data.HDOP, q.config.MaxHDOP)
	}

	return nil
}

// getFixTypeString returns human-readable fix type
func (q *QuectelGPSSource) getFixTypeString(fixType int) string {
	switch fixType {
	case 0:
		return "No Fix"
	case 1:
		return "Dead Reckoning"
	case 2:
		return "2D Fix"
	case 3:
		return "3D Fix"
	case 4:
		return "GNSS + Dead Reckoning"
	case 5:
		return "Time Only Fix"
	default:
		return fmt.Sprintf("Unknown (%d)", fixType)
	}
}

// GetSourceInfo returns information about the Quectel GPS source
func (q *QuectelGPSSource) GetSourceInfo() map[string]interface{} {
	return map[string]interface{}{
		"source":          "quectel",
		"type":            "gsm_gps",
		"enabled":         q.config.Enabled,
		"timeout":         q.config.Timeout.String(),
		"retry_attempts":  q.config.RetryAttempts,
		"min_satellites":  q.config.MinSatellites,
		"max_hdop":        q.config.MaxHDOP,
		"validate_coords": q.config.ValidateCoords,
	}
}

// GetHealthStatus returns the current health status of the Quectel GPS source
func (q *QuectelGPSSource) GetHealthStatus() GPSSourceHealth {
	return GPSSourceHealth{
		Available:    q.IsAvailable(context.Background()),
		LastSuccess:  time.Now(),
		LastError:    "",
		SuccessRate:  1.0, // TODO: implement proper success rate tracking
		AvgLatency:   0.0, // TODO: implement latency tracking
		ErrorCount:   0,   // TODO: implement error counting
		SuccessCount: 1,   // TODO: implement success counting
	}
}

// GetName returns the name of this GPS source
func (q *QuectelGPSSource) GetName() string {
	return "quectel"
}

// GetPriority returns the priority of this GPS source
func (q *QuectelGPSSource) GetPriority() int {
	return 50 // Medium priority, lower than built-in GPS but higher than network sources
}

// QuectelSourceAdapter adapts QuectelGPSSource to GPSSourceProvider interface
type QuectelSourceAdapter struct {
	source *QuectelGPSSource
}

// CollectGPS implements GPSSourceProvider interface
func (a *QuectelSourceAdapter) CollectGPS(ctx context.Context) (*StandardizedGPSData, error) {
	return a.source.CollectGPS(ctx)
}

// IsAvailable implements GPSSourceProvider interface
func (a *QuectelSourceAdapter) IsAvailable(ctx context.Context) bool {
	return a.source.IsAvailable(ctx)
}

// GetHealthStatus implements GPSSourceProvider interface
func (a *QuectelSourceAdapter) GetHealthStatus() GPSSourceHealth {
	return a.source.GetHealthStatus()
}

// GetName implements GPSSourceProvider interface
func (a *QuectelSourceAdapter) GetName() string {
	return a.source.GetName()
}

// GetPriority implements GPSSourceProvider interface
func (a *QuectelSourceAdapter) GetPriority() int {
	return a.source.GetPriority()
}
