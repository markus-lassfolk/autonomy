package gps

import (
	"context"
	"fmt"
	"math"
	"time"

	"github.com/markus-lassfolk/autonomy/pkg"
	"github.com/markus-lassfolk/autonomy/pkg/logx"
	"github.com/markus-lassfolk/autonomy/pkg/starlink"
)

// ComprehensiveGPSCollector implements advanced GPS collection with multiple sources
type ComprehensiveGPSCollector struct {
	logger         *logx.Logger
	config         *ComprehensiveGPSConfig
	starlinkClient *starlink.Client
	lastKnown      *StandardizedGPSData
	sources        []GPSSourceProvider
}

// ComprehensiveGPSConfig represents comprehensive GPS configuration
type ComprehensiveGPSConfig struct {
	Enabled                   bool     `json:"enabled"`
	SourcePriority            []string `json:"source_priority"`              // ["rutos", "starlink", "quectel", "google"]
	MovementThresholdM        float64  `json:"movement_threshold_m"`         // Movement detection threshold
	AccuracyThresholdM        float64  `json:"accuracy_threshold_m"`         // Minimum accuracy required
	StalenessThresholdS       int64    `json:"staleness_threshold_s"`        // Maximum age for GPS data
	CollectionTimeoutS        int      `json:"collection_timeout_s"`         // Collection timeout
	RetryAttempts             int      `json:"retry_attempts"`               // Number of retry attempts
	RetryDelayS               int      `json:"retry_delay_s"`                // Delay between retries
	GoogleAPIEnabled          bool     `json:"google_api_enabled"`           // Enable Google API
	GoogleAPIKey              string   `json:"google_api_key"`               // Google API key
	GoogleElevationAPIEnabled bool     `json:"google_elevation_api_enabled"` // Enable Google Maps Elevation API
	NMEADevices               []string `json:"nmea_devices"`                 // NMEA device paths
	PreferHighAccuracy        bool     `json:"prefer_high_accuracy"`         // Prefer high accuracy sources
	EnableMovementDetection   bool     `json:"enable_movement_detection"`    // Enable movement detection
	EnableLocationClustering  bool     `json:"enable_location_clustering"`   // Enable location clustering

	// Hybrid Confidence-Based Prioritization
	EnableHybridPrioritization  bool    `json:"enable_hybrid_prioritization"`  // Enable confidence-based fallback
	MinAcceptableConfidence     float64 `json:"min_acceptable_confidence"`     // Minimum confidence to accept (0.0-1.0)
	FallbackConfidenceThreshold float64 `json:"fallback_confidence_threshold"` // Threshold to try next source (0.0-1.0)

	// OpenCellID Configuration
	OpenCellIDEnabled    bool   `json:"opencellid_enabled"`    // Enable OpenCellID GPS source
	OpenCellIDAPIKey     string `json:"opencellid_api_key"`    // OpenCellID API key
	OpenCellIDContribute bool   `json:"opencellid_contribute"` // Enable data contribution to OpenCellID
}

// StandardizedGPSData represents GPS data in standardized format
type StandardizedGPSData struct {
	// Core Location Data
	Latitude  float64   `json:"latitude"`  // Decimal degrees
	Longitude float64   `json:"longitude"` // Decimal degrees
	Altitude  float64   `json:"altitude"`  // Meters above sea level
	Accuracy  float64   `json:"accuracy"`  // Accuracy radius in meters
	Timestamp time.Time `json:"timestamp"` // When location was determined

	// Enhanced GPS Data
	Speed  float64 `json:"speed"`  // Speed in m/s
	Course float64 `json:"course"` // Bearing in degrees
	HDOP   float64 `json:"hdop"`   // Horizontal Dilution of Precision
	VDOP   float64 `json:"vdop"`   // Vertical Dilution of Precision

	// Fix Information
	FixType    int    `json:"fix_type"`    // 0=No Fix, 1=2D Fix, 2=3D Fix, 3=DGPS Fix
	FixQuality string `json:"fix_quality"` // "excellent", "good", "fair", "poor"
	Satellites int    `json:"satellites"`  // Number of satellites

	// Source Information
	Source      string   `json:"source"`       // Primary source name
	Method      string   `json:"method"`       // Collection method
	DataSources []string `json:"data_sources"` // All sources used

	// Quality Indicators
	Valid      bool    `json:"valid"`      // Whether location is valid
	Confidence float64 `json:"confidence"` // Confidence score 0.0-1.0

	// Collection Metadata
	CollectionTime time.Duration `json:"collection_time"` // Time to collect
	FromCache      bool          `json:"from_cache"`      // Whether from cache
	APICallMade    bool          `json:"api_call_made"`   // Whether API was called
	APICost        float64       `json:"api_cost"`        // Cost of API call
}

// GPSSourceProvider interface for GPS data sources
type GPSSourceProvider interface {
	GetName() string
	GetPriority() int
	IsAvailable(ctx context.Context) bool
	CollectGPS(ctx context.Context) (*StandardizedGPSData, error)
	GetHealthStatus() GPSSourceHealth
}

// GPSSourceHealth represents health status of a GPS source
type GPSSourceHealth struct {
	Available    bool      `json:"available"`
	LastSuccess  time.Time `json:"last_success"`
	LastError    string    `json:"last_error"`
	SuccessRate  float64   `json:"success_rate"`
	AvgLatency   float64   `json:"avg_latency_ms"`
	ErrorCount   int       `json:"error_count"`
	SuccessCount int       `json:"success_count"`
}

// NewComprehensiveGPSCollector creates a new comprehensive GPS collector
func NewComprehensiveGPSCollector(config *ComprehensiveGPSConfig, logger *logx.Logger) *ComprehensiveGPSCollector {
	if config == nil {
		config = DefaultComprehensiveGPSConfig()
	}

	collector := &ComprehensiveGPSCollector{
		logger:         logger,
		config:         config,
		starlinkClient: starlink.DefaultClient(logger),
		sources:        []GPSSourceProvider{},
	}

	// Initialize GPS sources based on priority with availability checking
	ctx := context.Background()
	availableSources := 0

	for _, sourceName := range config.SourcePriority {
		switch sourceName {
		case "rutos":
			source := NewRUTOSGPSSource(availableSources, logger)
			if source.IsAvailable(ctx) {
				collector.sources = append(collector.sources, source)
				availableSources++
				logger.Info("gps_source_initialized",
					"source", sourceName,
					"priority", availableSources,
					"status", "available",
				)
			} else {
				logger.Warn("gps_source_skipped",
					"source", sourceName,
					"reason", "no_rutos_gps_hardware_detected",
				)
			}
		case "starlink":
			source := NewStarlinkGPSSource(availableSources, collector.starlinkClient, logger)
			if source.IsAvailable(ctx) {
				collector.sources = append(collector.sources, source)
				availableSources++
				logger.Info("gps_source_initialized",
					"source", sourceName,
					"priority", availableSources,
					"status", "available",
				)
			} else {
				logger.Warn("gps_source_skipped",
					"source", sourceName,
					"reason", "starlink_not_available",
				)
			}
		case "quectel":
			source := NewQuectelGPSSource(DefaultQuectelGPSConfig(), logger)
			if source.IsAvailable(ctx) {
				collector.sources = append(collector.sources, &QuectelSourceAdapter{source: source})
				availableSources++
				logger.Info("gps_source_initialized",
					"source", sourceName,
					"priority", availableSources,
					"status", "available",
				)
			} else {
				logger.Warn("gps_source_skipped",
					"source", sourceName,
					"reason", "quectel_modem_not_available",
				)
			}
		case "google":
			if config.GoogleAPIEnabled && config.GoogleAPIKey != "" {
				source := NewGoogleLocationSource(availableSources, config.GoogleAPIKey, config.GoogleElevationAPIEnabled, logger)
				if source.IsAvailable(ctx) {
					collector.sources = append(collector.sources, source)
					availableSources++
					logger.Info("gps_source_initialized",
						"source", sourceName,
						"priority", availableSources,
						"status", "available",
					)
				} else {
					logger.Warn("gps_source_skipped",
						"source", sourceName,
						"reason", "no_cellular_or_wifi_data_available",
					)
				}
			} else {
				logger.Warn("gps_source_skipped",
					"source", sourceName,
					"reason", "google_api_disabled_or_no_key",
				)
			}
		case "opencellid":
			// Check if OpenCellID is configured in the main config
			if config.OpenCellIDEnabled && config.OpenCellIDAPIKey != "" {
				opencellConfig := &OpenCellIDGPSConfig{
					Enabled:                   true,
					APIKey:                    config.OpenCellIDAPIKey,
					ContributeData:            config.OpenCellIDContribute,
					CacheSizeMB:               25,
					MaxCellsPerLookup:         5,
					NegativeCacheTTLHours:     12,
					ContributionIntervalMin:   10,
					MinGPSAccuracyM:           20.0,
					MovementThresholdM:        250.0,
					RSRPChangeThresholdDB:     6.0,
					TimingAdvanceEnabled:      true,
					FusionConfidenceThreshold: 0.5,
					RatioLimit:                8.0, // 8:1 lookup:submission ratio
					RatioWindowHours:          48,  // 48-hour rolling window

					// Scheduler configuration - use defaults since not in config
					EnableScheduler:             false, // Default disabled
					SchedulerMovingInterval:     5,     // 5 minutes
					SchedulerStationaryInterval: 30,    // 30 minutes
					SchedulerMaxScansPerHour:    12,
				}

				// Create a cellular data collector (this would need to be implemented)
				cellCollector := NewCellularDataCollectorFromConfig(logger)

				source := NewOpenCellIDGPSSource(availableSources, opencellConfig, cellCollector, logger)
				if source.IsAvailable(ctx) {
					collector.sources = append(collector.sources, source)
					availableSources++

					// Start scheduler if enabled
					if opencellConfig.EnableScheduler {
						if err := source.StartScheduler(ctx); err != nil {
							logger.Warn("opencellid_scheduler_start_failed",
								"error", err.Error(),
							)
						} else {
							logger.Info("opencellid_scheduler_started",
								"moving_interval", opencellConfig.SchedulerMovingInterval,
								"stationary_interval", opencellConfig.SchedulerStationaryInterval,
								"max_scans_per_hour", opencellConfig.SchedulerMaxScansPerHour,
							)
						}
					}

					logger.Info("gps_source_initialized",
						"source", sourceName,
						"priority", availableSources,
						"status", "available",
						"contribute_data", opencellConfig.ContributeData,
						"scheduler_enabled", opencellConfig.EnableScheduler,
					)
				} else {
					logger.Warn("gps_source_skipped",
						"source", sourceName,
						"reason", "no_cellular_data_available",
					)
				}
			} else {
				logger.Warn("gps_source_skipped",
					"source", sourceName,
					"reason", "opencellid_not_configured",
				)
			}
		default:
			logger.Warn("gps_source_unknown",
				"source", sourceName,
				"reason", "unknown_source_type",
			)
		}
	}

	logger.Info("gps_collector_initialized",
		"total_sources", len(collector.sources),
		"available_sources", availableSources,
		"configured_sources", len(config.SourcePriority),
	)

	// Warn if no GPS sources are available
	if len(collector.sources) == 0 {
		logger.Warn("gps_no_sources_available",
			"configured_sources", len(config.SourcePriority),
			"warning", "GPS functionality will be disabled",
		)
	}

	return collector
}

// ReEvaluateSourceAvailability re-evaluates GPS source availability and updates the source list
func (gc *ComprehensiveGPSCollector) ReEvaluateSourceAvailability(ctx context.Context) {
	gc.logger.LogDebugVerbose("gps_source_reevaluation_start", map[string]interface{}{
		"current_sources": len(gc.sources),
	})

	// Check current sources for availability changes
	availableSources := []GPSSourceProvider{}
	unavailableSources := []string{}

	for _, source := range gc.sources {
		if source.IsAvailable(ctx) {
			availableSources = append(availableSources, source)
		} else {
			unavailableSources = append(unavailableSources, source.GetName())
			gc.logger.Warn("gps_source_became_unavailable",
				"source", source.GetName(),
				"reason", "availability_check_failed",
			)
		}
	}

	// Check if any previously unavailable sources are now available
	newSources := []GPSSourceProvider{}
	availableSourceNames := make(map[string]bool)
	for _, source := range availableSources {
		availableSourceNames[source.GetName()] = true
	}

	// Re-check all configured sources
	for _, sourceName := range gc.config.SourcePriority {
		if availableSourceNames[sourceName] {
			continue // Already available
		}

		// Try to initialize this source again
		switch sourceName {
		case "rutos":
			source := NewRUTOSGPSSource(len(availableSources)+len(newSources), gc.logger)
			if source.IsAvailable(ctx) {
				newSources = append(newSources, source)
				gc.logger.Info("gps_source_became_available",
					"source", sourceName,
					"priority", len(availableSources)+len(newSources),
				)
			}
		case "starlink":
			source := NewStarlinkGPSSource(len(availableSources)+len(newSources), gc.starlinkClient, gc.logger)
			if source.IsAvailable(ctx) {
				newSources = append(newSources, source)
				gc.logger.Info("gps_source_became_available",
					"source", sourceName,
					"priority", len(availableSources)+len(newSources),
				)
			}
		case "quectel":
			source := NewQuectelGPSSource(DefaultQuectelGPSConfig(), gc.logger)
			if source.IsAvailable(ctx) {
				newSources = append(newSources, &QuectelSourceAdapter{source: source})
				gc.logger.Info("gps_source_became_available",
					"source", sourceName,
					"priority", len(availableSources)+len(newSources),
				)
			}
		case "google":
			if gc.config.GoogleAPIEnabled && gc.config.GoogleAPIKey != "" {
				source := NewGoogleLocationSource(len(availableSources)+len(newSources), gc.config.GoogleAPIKey, gc.config.GoogleElevationAPIEnabled, gc.logger)
				if source.IsAvailable(ctx) {
					newSources = append(newSources, source)
					gc.logger.Info("gps_source_became_available",
						"source", sourceName,
						"priority", len(availableSources)+len(newSources),
					)
				}
			}
		}
	}

	// Update the sources list
	gc.sources = append(availableSources, newSources...)

	gc.logger.Info("gps_source_reevaluation_complete",
		"total_sources", len(gc.sources),
		"available_sources", len(availableSources),
		"new_sources", len(newSources),
		"unavailable_sources", len(unavailableSources),
		"unavailable_list", unavailableSources,
	)
}

// DefaultComprehensiveGPSConfig returns default comprehensive GPS configuration
func DefaultComprehensiveGPSConfig() *ComprehensiveGPSConfig {
	return &ComprehensiveGPSConfig{
		Enabled:                   true,
		SourcePriority:            []string{"rutos", "starlink", "quectel", "opencellid", "google"},
		MovementThresholdM:        100.0, // 100 meters movement threshold
		AccuracyThresholdM:        100.0, // 100 meters accuracy threshold
		StalenessThresholdS:       300,   // 5 minutes staleness threshold
		CollectionTimeoutS:        30,    // 30 seconds timeout
		RetryAttempts:             3,
		RetryDelayS:               2,
		GoogleAPIEnabled:          false,
		GoogleAPIKey:              "",
		GoogleElevationAPIEnabled: false,
		NMEADevices:               []string{"/dev/ttyUSB1", "/dev/ttyUSB2", "/dev/ttyACM0"},
		PreferHighAccuracy:        true,
		EnableMovementDetection:   true,
		EnableLocationClustering:  true,

		// Hybrid Confidence-Based Prioritization Defaults
		EnableHybridPrioritization:  true, // Enable intelligent source selection
		MinAcceptableConfidence:     0.5,  // 50% minimum confidence to accept any source
		FallbackConfidenceThreshold: 0.7,  // 70% threshold to try next source

		// OpenCellID Defaults
		OpenCellIDEnabled:    false, // Requires API key configuration
		OpenCellIDAPIKey:     "",    // Must be configured by user
		OpenCellIDContribute: true,  // Enable contribution by default when API key is set
	}
}

// CollectBestGPS collects GPS data using hybrid confidence-based prioritization
func (gc *ComprehensiveGPSCollector) CollectBestGPS(ctx context.Context) (*StandardizedGPSData, error) {
	if !gc.config.Enabled {
		return nil, fmt.Errorf("GPS collection is disabled")
	}

	// Check if any GPS sources are available
	if len(gc.sources) == 0 {
		return nil, fmt.Errorf("no GPS sources are available - all sources were skipped during initialization")
	}

	start := time.Now()

	// Use hybrid prioritization if enabled
	if gc.config.EnableHybridPrioritization {
		return gc.collectWithHybridPrioritization(ctx, start)
	}

	// Fall back to traditional priority-based collection
	return gc.collectWithTraditionalPriority(ctx, start)
}

// collectWithHybridPrioritization implements the intelligent confidence-based source selection
func (gc *ComprehensiveGPSCollector) collectWithHybridPrioritization(ctx context.Context, start time.Time) (*StandardizedGPSData, error) {
	var allSourceData []*StandardizedGPSData

	gc.logger.LogDebugVerbose("hybrid_gps_collection_start", map[string]interface{}{
		"min_confidence":     gc.config.MinAcceptableConfidence,
		"fallback_threshold": gc.config.FallbackConfidenceThreshold,
		"source_count":       len(gc.sources),
	})

	// Step 1: Try each source in priority order, but collect confidence data
	for i, source := range gc.sources {
		if !source.IsAvailable(ctx) {
			gc.logger.LogDebugVerbose("gps_source_unavailable", map[string]interface{}{
				"source": source.GetName(),
				"step":   "hybrid_collection",
			})
			continue
		}

		gpsData := gc.collectFromSource(ctx, source, start)
		if gpsData == nil {
			continue // Source failed, try next
		}

		allSourceData = append(allSourceData, gpsData)

		gc.logger.LogDebugVerbose("gps_source_collected", map[string]interface{}{
			"source":     source.GetName(),
			"confidence": gpsData.Confidence,
			"accuracy":   gpsData.Accuracy,
			"priority":   i + 1,
		})

		// Step 2: Apply hybrid logic
		if i == 0 { // RUTOS (highest priority)
			if gpsData.Confidence >= gc.config.FallbackConfidenceThreshold {
				gc.logger.Info("gps_source_selected",
					"source", source.GetName(),
					"reason", "high_confidence_primary",
					"confidence", gpsData.Confidence,
				)
				return gc.finalizeGPSData(gpsData, start), nil
			}
			gc.logger.LogDebugVerbose("gps_primary_low_confidence", map[string]interface{}{
				"source":     source.GetName(),
				"confidence": gpsData.Confidence,
				"threshold":  gc.config.FallbackConfidenceThreshold,
			})
			// Continue to check other sources
		} else if i == 1 { // Starlink (second priority)
			if gpsData.Confidence >= gc.config.FallbackConfidenceThreshold {
				gc.logger.Info("gps_source_selected",
					"source", source.GetName(),
					"reason", "high_confidence_secondary",
					"confidence", gpsData.Confidence,
				)
				return gc.finalizeGPSData(gpsData, start), nil
			}
			// Continue to Google if available
		} else { // Google or other sources
			if gpsData.Confidence >= gc.config.FallbackConfidenceThreshold {
				gc.logger.Info("gps_source_selected",
					"source", source.GetName(),
					"reason", "high_confidence_fallback",
					"confidence", gpsData.Confidence,
				)
				return gc.finalizeGPSData(gpsData, start), nil
			}
		}
	}

	// Step 3: No source met the high confidence threshold, pick the best available
	if len(allSourceData) == 0 {
		return nil, fmt.Errorf("no GPS sources provided data")
	}

	// Find the source with highest confidence that meets minimum requirements
	var bestData *StandardizedGPSData
	for _, data := range allSourceData {
		if data.Confidence >= gc.config.MinAcceptableConfidence {
			if bestData == nil || data.Confidence > bestData.Confidence {
				bestData = data
			}
		}
	}

	// If no source meets minimum confidence, pick the best available anyway
	if bestData == nil {
		for _, data := range allSourceData {
			if bestData == nil || data.Confidence > bestData.Confidence {
				bestData = data
			}
		}
		gc.logger.Warn("gps_low_confidence_selected",
			"source", bestData.Source,
			"confidence", bestData.Confidence,
			"min_required", gc.config.MinAcceptableConfidence,
			"reason", "best_available_below_threshold",
		)
	} else {
		gc.logger.Info("gps_source_selected",
			"source", bestData.Source,
			"reason", "best_confidence_above_minimum",
			"confidence", bestData.Confidence,
		)
	}

	return gc.finalizeGPSData(bestData, start), nil
}

// collectWithTraditionalPriority implements the original priority-based collection
func (gc *ComprehensiveGPSCollector) collectWithTraditionalPriority(ctx context.Context, start time.Time) (*StandardizedGPSData, error) {
	// Try each source in priority order (original logic)
	for _, source := range gc.sources {
		if !source.IsAvailable(ctx) {
			gc.logger.LogDebugVerbose("gps_source_unavailable", map[string]interface{}{
				"source": source.GetName(),
			})
			continue
		}

		gpsData := gc.collectFromSource(ctx, source, start)
		if gpsData != nil {
			return gc.finalizeGPSData(gpsData, start), nil
		}
	}

	return nil, fmt.Errorf("no GPS sources provided data")
}

// collectFromSource attempts to collect GPS data from a specific source
func (gc *ComprehensiveGPSCollector) collectFromSource(ctx context.Context, source GPSSourceProvider, start time.Time) *StandardizedGPSData {
	// Create timeout context for this source
	sourceCtx, cancel := context.WithTimeout(ctx, time.Duration(gc.config.CollectionTimeoutS)*time.Second)
	defer cancel()

	// Attempt to collect GPS data with retries
	for attempt := 0; attempt < gc.config.RetryAttempts; attempt++ {
		gpsData, err := source.CollectGPS(sourceCtx)
		if err != nil {
			gc.logger.LogDebugVerbose("gps_collection_attempt_failed", map[string]interface{}{
				"source":  source.GetName(),
				"attempt": attempt + 1,
				"error":   err.Error(),
			})

			if attempt < gc.config.RetryAttempts-1 {
				time.Sleep(time.Duration(gc.config.RetryDelayS) * time.Second)
			}
			continue
		}

		// Validate GPS data quality
		if err := gc.ValidateGPSData(gpsData); err != nil {
			gc.logger.LogDebugVerbose("gps_validation_failed", map[string]interface{}{
				"source": source.GetName(),
				"error":  err.Error(),
			})
			continue
		}

		// Set collection time
		gpsData.CollectionTime = time.Since(start)
		return gpsData
	}

	return nil // Failed to collect from this source
}

// finalizeGPSData performs final processing on selected GPS data
func (gc *ComprehensiveGPSCollector) finalizeGPSData(gpsData *StandardizedGPSData, start time.Time) *StandardizedGPSData {
	// Check for movement if enabled
	if gc.config.EnableMovementDetection && gc.lastKnown != nil {
		distance := gc.calculateDistance(gc.lastKnown, gpsData)
		if distance > gc.config.MovementThresholdM {
			gc.logger.LogStateChange("gps_collector", "stationary", "moving", "movement_detected", map[string]interface{}{
				"distance_m":    distance,
				"threshold_m":   gc.config.MovementThresholdM,
				"from_lat":      gc.lastKnown.Latitude,
				"from_lon":      gc.lastKnown.Longitude,
				"to_lat":        gpsData.Latitude,
				"to_lon":        gpsData.Longitude,
				"movement_time": time.Since(gc.lastKnown.Timestamp).Seconds(),
			})
		}
	}

	// Update last known position
	gc.lastKnown = gpsData

	gc.logger.LogVerbose("gps_collection_success", map[string]interface{}{
		"source":          gpsData.Source,
		"latitude":        gpsData.Latitude,
		"longitude":       gpsData.Longitude,
		"accuracy":        gpsData.Accuracy,
		"satellites":      gpsData.Satellites,
		"fix_quality":     gpsData.FixQuality,
		"fix_type":        gpsData.FixType,
		"confidence":      gpsData.Confidence,
		"collection_time": gpsData.CollectionTime.Milliseconds(),
	})

	return gpsData
}

// CollectAllSources collects GPS data from all available sources for comparison
func (gc *ComprehensiveGPSCollector) CollectAllSources(ctx context.Context) (map[string]*StandardizedGPSData, error) {
	results := make(map[string]*StandardizedGPSData)

	for _, source := range gc.sources {
		if !source.IsAvailable(ctx) {
			continue
		}

		sourceCtx, cancel := context.WithTimeout(ctx, time.Duration(gc.config.CollectionTimeoutS)*time.Second)
		gpsData, err := source.CollectGPS(sourceCtx)
		cancel()

		if err != nil {
			gc.logger.LogDebugVerbose("source_collection_failed", map[string]interface{}{
				"source": source.GetName(),
				"error":  err.Error(),
			})
			continue
		}

		if err := gc.ValidateGPSData(gpsData); err == nil {
			results[source.GetName()] = gpsData
		}
	}

	if len(results) == 0 {
		return nil, fmt.Errorf("no sources returned valid GPS data")
	}

	return results, nil
}

// ValidateGPSData validates GPS data quality
func (gc *ComprehensiveGPSCollector) ValidateGPSData(gps *StandardizedGPSData) error {
	if gps == nil {
		return fmt.Errorf("GPS data is nil")
	}

	if !gps.Valid {
		return fmt.Errorf("GPS data is marked as invalid")
	}

	// Check coordinate bounds
	if gps.Latitude < -90 || gps.Latitude > 90 {
		return fmt.Errorf("invalid latitude: %f", gps.Latitude)
	}
	if gps.Longitude < -180 || gps.Longitude > 180 {
		return fmt.Errorf("invalid longitude: %f", gps.Longitude)
	}

	// Check accuracy threshold
	if gps.Accuracy > gc.config.AccuracyThresholdM {
		return fmt.Errorf("GPS accuracy too low: %f > %f", gps.Accuracy, gc.config.AccuracyThresholdM)
	}

	// Check staleness
	if time.Since(gps.Timestamp).Seconds() > float64(gc.config.StalenessThresholdS) {
		return fmt.Errorf("GPS data too stale: %v", time.Since(gps.Timestamp))
	}

	return nil
}

// GetBestAvailableSource returns the name of the best available GPS source
func (gc *ComprehensiveGPSCollector) GetBestAvailableSource(ctx context.Context) string {
	for _, source := range gc.sources {
		if source.IsAvailable(ctx) {
			return source.GetName()
		}
	}
	return "none"
}

// GetSourceHealthStatus returns health status of all GPS sources
func (gc *ComprehensiveGPSCollector) GetSourceHealthStatus() map[string]GPSSourceHealth {
	status := make(map[string]GPSSourceHealth)

	for _, source := range gc.sources {
		status[source.GetName()] = source.GetHealthStatus()
	}

	return status
}

// calculateDistance calculates the distance between two GPS coordinates using Haversine formula
func (gc *ComprehensiveGPSCollector) calculateDistance(from, to *StandardizedGPSData) float64 {
	const earthRadiusM = 6371000 // Earth's radius in meters

	lat1Rad := from.Latitude * math.Pi / 180
	lat2Rad := to.Latitude * math.Pi / 180
	deltaLatRad := (to.Latitude - from.Latitude) * math.Pi / 180
	deltaLonRad := (to.Longitude - from.Longitude) * math.Pi / 180

	a := math.Sin(deltaLatRad/2)*math.Sin(deltaLatRad/2) +
		math.Cos(lat1Rad)*math.Cos(lat2Rad)*
			math.Sin(deltaLonRad/2)*math.Sin(deltaLonRad/2)
	c := 2 * math.Atan2(math.Sqrt(a), math.Sqrt(1-a))

	return earthRadiusM * c
}

// ConvertToLegacyFormat converts StandardizedGPSData to legacy pkg.GPSData format
func (gps *StandardizedGPSData) ConvertToLegacyFormat() *pkg.GPSData {
	return &pkg.GPSData{
		Latitude:   gps.Latitude,
		Longitude:  gps.Longitude,
		Altitude:   gps.Altitude,
		Accuracy:   gps.Accuracy,
		Source:     gps.Source,
		Satellites: gps.Satellites,
		Valid:      gps.Valid,
		Timestamp:  gps.Timestamp,
	}
}

// CreateStandardizedFromLegacy converts legacy pkg.GPSData to StandardizedGPSData
func CreateStandardizedFromLegacy(legacy *pkg.GPSData) *StandardizedGPSData {
	if legacy == nil {
		return nil
	}

	quality := "poor"
	confidence := 0.3
	fixType := 0

	if legacy.Valid {
		if legacy.Accuracy <= 5 {
			quality = "excellent"
			confidence = 0.95
			fixType = 3
		} else if legacy.Accuracy <= 15 {
			quality = "good"
			confidence = 0.8
			fixType = 2
		} else if legacy.Accuracy <= 50 {
			quality = "fair"
			confidence = 0.6
			fixType = 1
		}
	}

	return &StandardizedGPSData{
		Latitude:    legacy.Latitude,
		Longitude:   legacy.Longitude,
		Altitude:    legacy.Altitude,
		Accuracy:    legacy.Accuracy,
		Timestamp:   legacy.Timestamp,
		FixType:     fixType,
		FixQuality:  quality,
		Satellites:  legacy.Satellites,
		Source:      legacy.Source,
		Method:      "legacy",
		DataSources: []string{legacy.Source},
		Valid:       legacy.Valid,
		Confidence:  confidence,
	}
}
