package gps

import (
	"context"
	"fmt"
	"time"

	"github.com/markus-lassfolk/autonomy/pkg"
	"github.com/markus-lassfolk/autonomy/pkg/logx"
	"github.com/markus-lassfolk/autonomy/pkg/starlink"
)

// ComprehensiveStarlinkGPS combines data from all three Starlink APIs
type ComprehensiveStarlinkGPS struct {
	// Core Location Data (from get_location)
	Latitude  float64 `json:"latitude"`
	Longitude float64 `json:"longitude"`
	Altitude  float64 `json:"altitude"`
	Accuracy  float64 `json:"accuracy"`

	// Speed Data (from get_location)
	HorizontalSpeedMps float64 `json:"horizontal_speed_mps"`
	VerticalSpeedMps   float64 `json:"vertical_speed_mps"`

	// GPS Source Info (from get_location)
	GPSSource string `json:"gps_source"` // GNC_FUSED, GNC_NO_ACCEL, etc.

	// Satellite Data (from get_status)
	GPSValid        *bool `json:"gps_valid,omitempty"`          // GPS fix validity
	GPSSatellites   *int  `json:"gps_satellites,omitempty"`     // Number of satellites
	NoSatsAfterTTFF *bool `json:"no_sats_after_ttff,omitempty"` // No satellites after time to first fix
	InhibitGPS      *bool `json:"inhibit_gps,omitempty"`        // GPS inhibited

	// Enhanced Location Data (from get_diagnostics)
	LocationEnabled        *bool    `json:"location_enabled,omitempty"`         // Location service enabled
	UncertaintyMeters      *float64 `json:"uncertainty_meters,omitempty"`       // Uncertainty in meters
	UncertaintyMetersValid *bool    `json:"uncertainty_meters_valid,omitempty"` // Uncertainty validity
	GPSTimeS               *float64 `json:"gps_time_s,omitempty"`               // GPS time in seconds

	// Metadata
	DataSources  []string  `json:"data_sources"`  // Which APIs provided data
	CollectedAt  time.Time `json:"collected_at"`  // When data was collected
	CollectionMs int64     `json:"collection_ms"` // Time taken to collect all data
	Valid        bool      `json:"valid"`         // Overall validity
	Confidence   float64   `json:"confidence"`    // Confidence score 0.0-1.0
	QualityScore string    `json:"quality_score"` // excellent, good, fair, poor
}

// StarlinkAPICollector collects GPS data from all Starlink APIs
type StarlinkAPICollector struct {
	starlinkHost   string
	starlinkPort   int
	timeout        time.Duration
	starlinkClient *starlink.Client // Centralized Starlink client
	logger         *logx.Logger
}

// StarlinkAPICollectorConfig holds configuration for the collector
type StarlinkAPICollectorConfig struct {
	Host                  string        `json:"host"`
	Port                  int           `json:"port"`
	Timeout               time.Duration `json:"timeout"`
	EnableAllAPIs         bool          `json:"enable_all_apis"`
	EnableLocationAPI     bool          `json:"enable_location_api"`
	EnableStatusAPI       bool          `json:"enable_status_api"`
	EnableDiagnosticsAPI  bool          `json:"enable_diagnostics_api"`
	RetryAttempts         int           `json:"retry_attempts"`
	ConfidenceThreshold   float64       `json:"confidence_threshold"`
	QualityScoreThreshold float64       `json:"quality_score_threshold"`
}

// DefaultStarlinkAPICollectorConfig returns default configuration
func DefaultStarlinkAPICollectorConfig() *StarlinkAPICollectorConfig {
	return &StarlinkAPICollectorConfig{
		Host:                  "192.168.100.1",
		Port:                  9200,
		Timeout:               10 * time.Second,
		EnableAllAPIs:         true,
		EnableLocationAPI:     true,
		EnableStatusAPI:       true,
		EnableDiagnosticsAPI:  true,
		RetryAttempts:         3,
		ConfidenceThreshold:   0.3,
		QualityScoreThreshold: 0.5,
	}
}

// NewStarlinkAPICollector creates a new comprehensive Starlink GPS collector
func NewStarlinkAPICollector(config *StarlinkAPICollectorConfig, logger *logx.Logger) *StarlinkAPICollector {
	if config == nil {
		config = DefaultStarlinkAPICollectorConfig()
	}

	return &StarlinkAPICollector{
		starlinkHost:   config.Host,
		starlinkPort:   config.Port,
		timeout:        config.Timeout,
		starlinkClient: starlink.DefaultClient(nil),
		logger:         logger,
	}
}

// CollectComprehensiveGPS collects GPS data from all three Starlink APIs
func (sc *StarlinkAPICollector) CollectComprehensiveGPS(ctx context.Context) (*ComprehensiveStarlinkGPS, error) {
	startTime := time.Now()

	gps := &ComprehensiveStarlinkGPS{
		DataSources: []string{},
		CollectedAt: startTime,
	}

	sc.logger.LogDebugVerbose("starlink_comprehensive_collection_start", map[string]interface{}{
		"host": sc.starlinkHost,
		"port": sc.starlinkPort,
	})

	// Collect from get_location (primary coordinates + speed)
	if sc.shouldCollectLocation() {
		locationData, err := sc.collectLocationData(ctx)
		if err == nil {
			sc.mergeLocationData(gps, locationData)
			gps.DataSources = append(gps.DataSources, "get_location")
			sc.logger.LogDebugVerbose("starlink_location_collected", map[string]interface{}{
				"latitude":  gps.Latitude,
				"longitude": gps.Longitude,
				"accuracy":  gps.Accuracy,
			})
		} else {
			sc.logger.LogDebugVerbose("starlink_location_failed", map[string]interface{}{
				"error": err.Error(),
			})
		}
	}

	// Collect from get_status (satellite info)
	if sc.shouldCollectStatus() {
		statusData, err := sc.collectStatusData(ctx)
		if err == nil {
			sc.mergeStatusData(gps, statusData)
			gps.DataSources = append(gps.DataSources, "get_status")
			sc.logger.LogDebugVerbose("starlink_status_collected", map[string]interface{}{
				"gps_valid":      gps.GPSValid,
				"gps_satellites": gps.GPSSatellites,
			})
		} else {
			sc.logger.LogDebugVerbose("starlink_status_failed", map[string]interface{}{
				"error": err.Error(),
			})
		}
	}

	// Collect from get_diagnostics (enhanced location data)
	if sc.shouldCollectDiagnostics() {
		diagnosticsData, err := sc.collectDiagnosticsData(ctx)
		if err == nil {
			sc.mergeDiagnosticsData(gps, diagnosticsData)
			gps.DataSources = append(gps.DataSources, "get_diagnostics")
			sc.logger.LogDebugVerbose("starlink_diagnostics_collected", map[string]interface{}{
				"uncertainty_meters": gps.UncertaintyMeters,
				"location_enabled":   gps.LocationEnabled,
			})
		} else {
			sc.logger.LogDebugVerbose("starlink_diagnostics_failed", map[string]interface{}{
				"error": err.Error(),
			})
		}
	}

	// Calculate collection time
	gps.CollectionMs = time.Since(startTime).Milliseconds()

	// Calculate confidence and quality scores
	gps.Confidence = sc.calculateConfidence(gps)
	gps.QualityScore = sc.calculateQualityScore(gps)
	gps.Valid = gps.Confidence > 0.3

	sc.logger.LogDebugVerbose("starlink_comprehensive_collection_complete", map[string]interface{}{
		"data_sources":  gps.DataSources,
		"collection_ms": gps.CollectionMs,
		"confidence":    gps.Confidence,
		"quality_score": gps.QualityScore,
		"valid":         gps.Valid,
	})

	return gps, nil
}

// shouldCollectLocation determines if we should collect location data
func (sc *StarlinkAPICollector) shouldCollectLocation() bool {
	return true // Always collect location data
}

// shouldCollectStatus determines if we should collect status data
func (sc *StarlinkAPICollector) shouldCollectStatus() bool {
	return true // Always collect status data
}

// shouldCollectDiagnostics determines if we should collect diagnostics data
func (sc *StarlinkAPICollector) shouldCollectDiagnostics() bool {
	return true // Always collect diagnostics data
}

// collectLocationData collects data from get_location API
func (sc *StarlinkAPICollector) collectLocationData(ctx context.Context) (map[string]interface{}, error) {
	// Use the existing Starlink client to get location data
	location, err := sc.starlinkClient.GetLocation(ctx)
	if err != nil {
		return nil, fmt.Errorf("failed to get location: %w", err)
	}

	// Convert to map for merging
	data := map[string]interface{}{
		"latitude":   location.Latitude,
		"longitude":  location.Longitude,
		"altitude":   location.Altitude,
		"accuracy":   location.Accuracy,
		"gps_source": location.Source,
	}

	return data, nil
}

// collectStatusData collects data from get_status API
func (sc *StarlinkAPICollector) collectStatusData(ctx context.Context) (map[string]interface{}, error) {
	// Use the existing Starlink client to get status data
	status, err := sc.starlinkClient.GetStatus(ctx)
	if err != nil {
		return nil, fmt.Errorf("failed to get status: %w", err)
	}

	// Convert to map for merging
	data := map[string]interface{}{
		"gps_valid":          status.DishGetStatus.GPSStats.GPSValid,
		"gps_satellites":     status.DishGetStatus.GPSStats.GPSSats,
		"no_sats_after_ttff": status.DishGetStatus.GPSStats.NoSatsAfterTtff,
		"inhibit_gps":        status.DishGetStatus.GPSStats.InhibitGPS,
	}

	return data, nil
}

// collectDiagnosticsData collects data from get_diagnostics API
func (sc *StarlinkAPICollector) collectDiagnosticsData(ctx context.Context) (map[string]interface{}, error) {
	// Use the existing Starlink client to get diagnostics data
	_, err := sc.starlinkClient.GetDiagnostics(ctx)
	if err != nil {
		return nil, fmt.Errorf("failed to get diagnostics: %w", err)
	}

	// Convert to map for merging
	data := map[string]interface{}{
		// Note: DiagnosticsResponse doesn't have GPS-specific fields
		// These would need to be implemented based on actual Starlink API
	}

	return data, nil
}

// mergeLocationData merges location data into the comprehensive GPS struct
func (sc *StarlinkAPICollector) mergeLocationData(gps *ComprehensiveStarlinkGPS, data map[string]interface{}) {
	if val, ok := data["latitude"].(float64); ok {
		gps.Latitude = val
	}
	if val, ok := data["longitude"].(float64); ok {
		gps.Longitude = val
	}
	if val, ok := data["altitude"].(float64); ok {
		gps.Altitude = val
	}
	if val, ok := data["accuracy"].(float64); ok {
		gps.Accuracy = val
	}
	if val, ok := data["horizontal_speed_mps"].(float64); ok {
		gps.HorizontalSpeedMps = val
	}
	if val, ok := data["vertical_speed_mps"].(float64); ok {
		gps.VerticalSpeedMps = val
	}
	if val, ok := data["gps_source"].(string); ok {
		gps.GPSSource = val
	}
}

// mergeStatusData merges status data into the comprehensive GPS struct
func (sc *StarlinkAPICollector) mergeStatusData(gps *ComprehensiveStarlinkGPS, data map[string]interface{}) {
	if val, ok := data["gps_valid"].(*bool); ok {
		gps.GPSValid = val
	}
	if val, ok := data["gps_satellites"].(*int); ok {
		gps.GPSSatellites = val
	}
	if val, ok := data["no_sats_after_ttff"].(*bool); ok {
		gps.NoSatsAfterTTFF = val
	}
	if val, ok := data["inhibit_gps"].(*bool); ok {
		gps.InhibitGPS = val
	}
}

// mergeDiagnosticsData merges diagnostics data into the comprehensive GPS struct
func (sc *StarlinkAPICollector) mergeDiagnosticsData(gps *ComprehensiveStarlinkGPS, data map[string]interface{}) {
	if val, ok := data["location_enabled"].(*bool); ok {
		gps.LocationEnabled = val
	}
	if val, ok := data["uncertainty_meters"].(*float64); ok {
		gps.UncertaintyMeters = val
	}
	if val, ok := data["uncertainty_meters_valid"].(*bool); ok {
		gps.UncertaintyMetersValid = val
	}
	if val, ok := data["gps_time_s"].(*float64); ok {
		gps.GPSTimeS = val
	}
}

// calculateConfidence calculates confidence score for the GPS data
func (sc *StarlinkAPICollector) calculateConfidence(gps *ComprehensiveStarlinkGPS) float64 {
	confidence := 0.0

	// Base confidence for having location data
	if gps.Latitude != 0 && gps.Longitude != 0 {
		confidence += 0.4
	}

	// GPS validity confidence
	if gps.GPSValid != nil && *gps.GPSValid {
		confidence += 0.2
	}

	// Satellite count confidence
	if gps.GPSSatellites != nil && *gps.GPSSatellites > 0 {
		confidence += 0.1
		if *gps.GPSSatellites >= 6 {
			confidence += 0.1
		}
	}

	// Accuracy confidence
	if gps.Accuracy > 0 && gps.Accuracy < 100 {
		confidence += 0.1
	}

	// Uncertainty confidence
	if gps.UncertaintyMeters != nil && gps.UncertaintyMetersValid != nil && *gps.UncertaintyMetersValid {
		if *gps.UncertaintyMeters < 50 {
			confidence += 0.1
		}
	}

	// Data source confidence
	if len(gps.DataSources) >= 2 {
		confidence += 0.1
	}

	return confidence
}

// calculateQualityScore calculates quality score for the GPS data
func (sc *StarlinkAPICollector) calculateQualityScore(gps *ComprehensiveStarlinkGPS) string {
	score := 0.0

	// GPS validity
	if gps.GPSValid != nil && *gps.GPSValid {
		score += 0.3
	}

	// Satellite count
	if gps.GPSSatellites != nil {
		if *gps.GPSSatellites >= 8 {
			score += 0.3
		} else if *gps.GPSSatellites >= 6 {
			score += 0.2
		} else if *gps.GPSSatellites >= 4 {
			score += 0.1
		}
	}

	// Accuracy
	if gps.Accuracy > 0 {
		if gps.Accuracy < 10 {
			score += 0.2
		} else if gps.Accuracy < 50 {
			score += 0.1
		}
	}

	// Uncertainty
	if gps.UncertaintyMeters != nil && gps.UncertaintyMetersValid != nil && *gps.UncertaintyMetersValid {
		if *gps.UncertaintyMeters < 10 {
			score += 0.2
		} else if *gps.UncertaintyMeters < 50 {
			score += 0.1
		}
	}

	// Determine quality score string
	if score >= 0.8 {
		return "excellent"
	} else if score >= 0.6 {
		return "good"
	} else if score >= 0.4 {
		return "fair"
	} else {
		return "poor"
	}
}

// GetGPSLocation returns a simplified location from the comprehensive data
func (sc *StarlinkAPICollector) GetGPSLocation(ctx context.Context) (*pkg.GPSData, error) {
	comprehensive, err := sc.CollectComprehensiveGPS(ctx)
	if err != nil {
		return nil, err
	}

	location := &pkg.GPSData{
		Latitude:  comprehensive.Latitude,
		Longitude: comprehensive.Longitude,
		Altitude:  comprehensive.Altitude,
		Accuracy:  comprehensive.Accuracy,
		Source:    "starlink_comprehensive",
		Timestamp: comprehensive.CollectedAt,
		Valid:     comprehensive.Valid,
	}

	return location, nil
}

// GetGPSStatus returns GPS status information
func (sc *StarlinkAPICollector) GetGPSStatus(ctx context.Context) (map[string]interface{}, error) {
	comprehensive, err := sc.CollectComprehensiveGPS(ctx)
	if err != nil {
		return nil, err
	}

	status := map[string]interface{}{
		"valid":            comprehensive.Valid,
		"confidence":       comprehensive.Confidence,
		"quality_score":    comprehensive.QualityScore,
		"data_sources":     comprehensive.DataSources,
		"collection_ms":    comprehensive.CollectionMs,
		"gps_valid":        comprehensive.GPSValid,
		"gps_satellites":   comprehensive.GPSSatellites,
		"gps_source":       comprehensive.GPSSource,
		"accuracy":         comprehensive.Accuracy,
		"uncertainty":      comprehensive.UncertaintyMeters,
		"location_enabled": comprehensive.LocationEnabled,
	}

	return status, nil
}

// IsAvailable checks if Starlink GPS is available
func (sc *StarlinkAPICollector) IsAvailable(ctx context.Context) bool {
	// Try to get a simple status to check availability
	_, err := sc.starlinkClient.GetStatus(ctx)
	return err == nil
}

// GetComprehensiveGPSMetrics returns detailed metrics about the GPS collection
func (sc *StarlinkAPICollector) GetComprehensiveGPSMetrics(ctx context.Context) (map[string]interface{}, error) {
	comprehensive, err := sc.CollectComprehensiveGPS(ctx)
	if err != nil {
		return nil, err
	}

	metrics := map[string]interface{}{
		"collection_time_ms": comprehensive.CollectionMs,
		"data_sources_count": len(comprehensive.DataSources),
		"confidence":         comprehensive.Confidence,
		"quality_score":      comprehensive.QualityScore,
		"valid":              comprehensive.Valid,
		"latitude":           comprehensive.Latitude,
		"longitude":          comprehensive.Longitude,
		"altitude":           comprehensive.Altitude,
		"accuracy":           comprehensive.Accuracy,
		"horizontal_speed":   comprehensive.HorizontalSpeedMps,
		"vertical_speed":     comprehensive.VerticalSpeedMps,
		"gps_source":         comprehensive.GPSSource,
		"gps_valid":          comprehensive.GPSValid,
		"gps_satellites":     comprehensive.GPSSatellites,
		"uncertainty_meters": comprehensive.UncertaintyMeters,
		"location_enabled":   comprehensive.LocationEnabled,
		"collected_at":       comprehensive.CollectedAt,
	}

	return metrics, nil
}
