package gps

import (
	"context"
	"encoding/json"
	"fmt"
	"time"

	"github.com/markus-lassfolk/autonomy/pkg/logx"
	"github.com/markus-lassfolk/autonomy/pkg/starlink"
)

// StarlinkGPSSource implements comprehensive Starlink GPS collection
type StarlinkGPSSource struct {
	logger       *logx.Logger
	priority     int
	client       *starlink.Client
	health       GPSSourceHealth
	errorCount   int
	successCount int
}

// Note: ComprehensiveStarlinkGPS type is defined in comprehensive_starlink_gps.go

// NewStarlinkGPSSource creates a new Starlink GPS source
func NewStarlinkGPSSource(priority int, client *starlink.Client, logger *logx.Logger) *StarlinkGPSSource {
	return &StarlinkGPSSource{
		logger:   logger,
		priority: priority,
		client:   client,
		health: GPSSourceHealth{
			Available:    false,
			LastSuccess:  time.Time{},
			LastError:    "",
			SuccessRate:  0.0,
			AvgLatency:   0.0,
			ErrorCount:   0,
			SuccessCount: 0,
		},
	}
}

// GetName returns the source name
func (ss *StarlinkGPSSource) GetName() string {
	return "starlink"
}

// GetPriority returns the source priority
func (ss *StarlinkGPSSource) GetPriority() int {
	return ss.priority
}

// GetHealthStatus returns the current health status
func (ss *StarlinkGPSSource) GetHealthStatus() GPSSourceHealth {
	total := ss.errorCount + ss.successCount
	if total > 0 {
		ss.health.SuccessRate = float64(ss.successCount) / float64(total)
	}
	return ss.health
}

// IsAvailable checks if Starlink GPS is available
func (ss *StarlinkGPSSource) IsAvailable(ctx context.Context) bool {
	available := ss.client.IsAvailable(ctx)
	ss.health.Available = available
	return available
}

// CollectGPS collects comprehensive GPS data from all Starlink APIs
func (ss *StarlinkGPSSource) CollectGPS(ctx context.Context) (*StandardizedGPSData, error) {
	start := time.Now()

	// Collect comprehensive GPS data from all three APIs
	comprehensive, err := ss.collectComprehensiveGPS(ctx)
	if err != nil {
		ss.errorCount++
		ss.health.LastError = err.Error()
		return nil, fmt.Errorf("failed to collect Starlink GPS data: %w", err)
	}

	// Convert to standardized format
	standardized := ss.convertToStandardized(comprehensive)
	standardized.CollectionTime = time.Since(start)

	// Update health metrics
	ss.successCount++
	ss.health.LastSuccess = time.Now()
	ss.health.AvgLatency = (ss.health.AvgLatency*float64(ss.successCount-1) + float64(standardized.CollectionTime.Milliseconds())) / float64(ss.successCount)

	return standardized, nil
}

// collectComprehensiveGPS collects GPS data from all three Starlink APIs
func (ss *StarlinkGPSSource) collectComprehensiveGPS(ctx context.Context) (*ComprehensiveStarlinkGPS, error) {
	start := time.Now()

	gps := &ComprehensiveStarlinkGPS{
		DataSources: []string{},
	}

	// Collect from get_location (primary coordinates)
	if err := ss.collectLocationData(ctx, gps); err != nil {
		ss.logger.LogDebugVerbose("starlink_location_failed", map[string]interface{}{
			"error": err.Error(),
		})
	}

	// Collect from get_status (satellite count and GPS validity)
	if err := ss.collectStatusData(ctx, gps); err != nil {
		ss.logger.LogDebugVerbose("starlink_status_failed", map[string]interface{}{
			"error": err.Error(),
		})
	}

	// Collect from get_diagnostics (enhanced location + GPS time)
	if err := ss.collectDiagnosticsData(ctx, gps); err != nil {
		ss.logger.LogDebugVerbose("starlink_diagnostics_failed", map[string]interface{}{
			"error": err.Error(),
		})
	}

	gps.CollectionMs = time.Since(start).Milliseconds()

	// Validate and score the collected data
	ss.validateAndScore(gps)

	if len(gps.DataSources) == 0 {
		return nil, fmt.Errorf("no Starlink APIs returned data")
	}

	if gps.Latitude == 0 && gps.Longitude == 0 {
		return nil, fmt.Errorf("no valid coordinates from Starlink APIs")
	}

	return gps, nil
}

// collectLocationData collects data from get_location API
func (ss *StarlinkGPSSource) collectLocationData(ctx context.Context, gps *ComprehensiveStarlinkGPS) error {
	response, err := ss.client.CallMethod(ctx, starlink.MethodGetLocation)
	if err != nil {
		return fmt.Errorf("get_location API call failed: %w", err)
	}

	var result map[string]interface{}
	if err := json.Unmarshal([]byte(response), &result); err != nil {
		return fmt.Errorf("failed to parse get_location response: %w", err)
	}

	ss.mergeLocationData(gps, result)
	gps.DataSources = append(gps.DataSources, "get_location")
	return nil
}

// collectStatusData collects data from get_status API
func (ss *StarlinkGPSSource) collectStatusData(ctx context.Context, gps *ComprehensiveStarlinkGPS) error {
	response, err := ss.client.CallMethod(ctx, starlink.MethodGetStatus)
	if err != nil {
		return fmt.Errorf("get_status API call failed: %w", err)
	}

	var result map[string]interface{}
	if err := json.Unmarshal([]byte(response), &result); err != nil {
		return fmt.Errorf("failed to parse get_status response: %w", err)
	}

	ss.mergeStatusData(gps, result)
	gps.DataSources = append(gps.DataSources, "get_status")
	return nil
}

// collectDiagnosticsData collects data from get_diagnostics API
func (ss *StarlinkGPSSource) collectDiagnosticsData(ctx context.Context, gps *ComprehensiveStarlinkGPS) error {
	response, err := ss.client.CallMethod(ctx, starlink.MethodGetDiagnostics)
	if err != nil {
		return fmt.Errorf("get_diagnostics API call failed: %w", err)
	}

	var result map[string]interface{}
	if err := json.Unmarshal([]byte(response), &result); err != nil {
		return fmt.Errorf("failed to parse get_diagnostics response: %w", err)
	}

	ss.mergeDiagnosticsData(gps, result)
	gps.DataSources = append(gps.DataSources, "get_diagnostics")
	return nil
}

// mergeLocationData merges get_location data into comprehensive GPS
func (ss *StarlinkGPSSource) mergeLocationData(gps *ComprehensiveStarlinkGPS, data map[string]interface{}) {
	if getLocation, ok := data["getLocation"].(map[string]interface{}); ok {
		if lla, ok := getLocation["lla"].(map[string]interface{}); ok {
			if lat, ok := lla["lat"].(float64); ok {
				gps.Latitude = lat
			}
			if lng, ok := lla["lng"].(float64); ok {
				gps.Longitude = lng
			}
			if alt, ok := lla["alt"].(float64); ok {
				gps.Altitude = alt
			}
		}
		if source, ok := getLocation["source"].(string); ok {
			gps.GPSSource = source
		}
		if speed, ok := getLocation["horizontalSpeedMps"].(float64); ok {
			gps.HorizontalSpeedMps = speed
		}
	}
}

// mergeStatusData merges get_status data into comprehensive GPS
func (ss *StarlinkGPSSource) mergeStatusData(gps *ComprehensiveStarlinkGPS, data map[string]interface{}) {
	if dishGetStatus, ok := data["dishGetStatus"].(map[string]interface{}); ok {
		if gpsStats, ok := dishGetStatus["gpsStats"].(map[string]interface{}); ok {
					if gpsValid, ok := gpsStats["gpsValid"].(bool); ok {
			gps.GPSValid = &gpsValid
		}
		if gpsSats, ok := gpsStats["gpsSats"].(float64); ok {
			gpsSatsInt := int(gpsSats)
			gps.GPSSatellites = &gpsSatsInt
		}
		}
	}
}

// mergeDiagnosticsData merges get_diagnostics data into comprehensive GPS
func (ss *StarlinkGPSSource) mergeDiagnosticsData(gps *ComprehensiveStarlinkGPS, data map[string]interface{}) {
	if dishGetDiagnostics, ok := data["dishGetDiagnostics"].(map[string]interface{}); ok {
		if location, ok := dishGetDiagnostics["location"].(map[string]interface{}); ok {
					if enabled, ok := location["enabled"].(bool); ok {
			gps.LocationEnabled = &enabled
		}
		if gpsTimeS, ok := location["gpsTimeS"].(float64); ok {
			gps.GPSTimeS = &gpsTimeS
		}
		if uncertaintyMeters, ok := location["uncertaintyMeters"].(float64); ok {
			gps.UncertaintyMeters = &uncertaintyMeters
		}

			// Use diagnostics coordinates if primary coordinates are missing
			if gps.Latitude == 0 && gps.Longitude == 0 {
				if lat, ok := location["latitude"].(float64); ok {
					gps.Latitude = lat
				}
				if lng, ok := location["longitude"].(float64); ok {
					gps.Longitude = lng
				}
				if alt, ok := location["altitudeMeters"].(float64); ok {
					gps.Altitude = alt
				}
			}
		}
	}
}

// validateAndScore validates and scores the comprehensive GPS data
func (ss *StarlinkGPSSource) validateAndScore(gps *ComprehensiveStarlinkGPS) {
	score := 0.0
	confidence := 0.0

	// Score based on data completeness
	if gps.Latitude != 0 && gps.Longitude != 0 {
		score += 30.0 // Base score for coordinates
		confidence += 0.3
	}
	if gps.GPSValid != nil && *gps.GPSValid {
		score += 20.0 // GPS validity
		confidence += 0.2
	}
	if gps.GPSSatellites != nil && *gps.GPSSatellites > 0 {
		score += float64(*gps.GPSSatellites) * 2.0       // Satellite count
		confidence += float64(*gps.GPSSatellites) / 50.0 // Max 0.3 for 15+ satellites
	}
	if gps.LocationEnabled != nil && *gps.LocationEnabled {
		score += 10.0 // Location service enabled
		confidence += 0.1
	}
	if gps.UncertaintyMeters != nil && *gps.UncertaintyMeters > 0 {
		score += 10.0 // Uncertainty data available
		confidence += 0.1
		// Better confidence for lower uncertainty
		if *gps.UncertaintyMeters <= 5 {
			confidence += 0.2
		} else if *gps.UncertaintyMeters <= 15 {
			confidence += 0.1
		}
	}

	// Bonus for multiple data sources
	score += float64(len(gps.DataSources)) * 5.0
	confidence += float64(len(gps.DataSources)) * 0.05

	// Normalize confidence to 0-1 range
	if confidence > 1.0 {
		confidence = 1.0
	}

	// Convert score to quality string
	if score >= 80 {
		gps.QualityScore = "excellent"
	} else if score >= 60 {
		gps.QualityScore = "good"
	} else if score >= 40 {
		gps.QualityScore = "fair"
	} else {
		gps.QualityScore = "poor"
	}
	gps.Confidence = confidence
}

// convertToStandardized converts comprehensive Starlink GPS to standardized format
func (ss *StarlinkGPSSource) convertToStandardized(comprehensive *ComprehensiveStarlinkGPS) *StandardizedGPSData {
	// Determine accuracy from uncertainty or estimate
	accuracy := 5.0 // Default accuracy for Starlink
	if comprehensive.UncertaintyMeters != nil && *comprehensive.UncertaintyMeters > 0 {
		accuracy = *comprehensive.UncertaintyMeters
	}

	// Determine fix type based on GPS validity and satellite count
	fixType := 0
	gpsValid := comprehensive.GPSValid != nil && *comprehensive.GPSValid
	gpsSats := 0
	if comprehensive.GPSSatellites != nil {
		gpsSats = *comprehensive.GPSSatellites
	}
	
	if gpsValid {
		if gpsSats >= 6 {
			fixType = 3 // DGPS fix
		} else if gpsSats >= 4 {
			fixType = 2 // 3D fix
		} else {
			fixType = 1 // 2D fix
		}
	}

	// Determine fix quality
	fixQuality := "poor"
	if gpsValid && gpsSats >= 8 && accuracy <= 5 {
		fixQuality = "excellent"
	} else if gpsValid && gpsSats >= 6 && accuracy <= 15 {
		fixQuality = "good"
	} else if gpsValid && gpsSats >= 4 {
		fixQuality = "fair"
	}

	return &StandardizedGPSData{
		Latitude:    comprehensive.Latitude,
		Longitude:   comprehensive.Longitude,
		Altitude:    comprehensive.Altitude,
		Accuracy:    accuracy,
		Speed:       comprehensive.HorizontalSpeedMps,
		FixType:     fixType,
		FixQuality:  fixQuality,
		Satellites:  gpsSats,
		Source:      "Starlink Multi-API",
		Method:      "starlink_comprehensive",
		DataSources: comprehensive.DataSources,
		Valid:       gpsValid && comprehensive.Latitude != 0 && comprehensive.Longitude != 0,
		Confidence:  comprehensive.Confidence,
		Timestamp:   time.Now(),
	}
}
