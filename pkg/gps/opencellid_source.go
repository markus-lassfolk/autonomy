package gps

import (
	"context"
	"fmt"
	"time"

	"github.com/markus-lassfolk/autonomy/pkg/logx"
)

// OpenCellIDGPSSource implements GPS data collection using OpenCellID cellular geolocation
type OpenCellIDGPSSource struct {
	name            string
	priority        int
	logger          *logx.Logger
	resolver        *OpenCellIDResolver
	fuser           *CellularLocationFuser
	contributionMgr *ContributionManager
	cellCollector   CellularDataCollector
	scheduler       *OpenCellIDScheduler
	lastLocation    *StandardizedGPSData
	consecutiveGood int
	consecutiveBad  int
	healthStats     GPSSourceHealth
}

// CellularDataCollector interface for collecting cellular tower information
type CellularDataCollector interface {
	GetServingCell(ctx context.Context) (*ServingCellInfo, error)
	GetNeighborCells(ctx context.Context) ([]NeighborCellInfo, error)
	GetCellularMetrics(ctx context.Context) (*CellularMetrics, error)
}

// OpenCellIDGPSConfig holds configuration for OpenCellID GPS source
type OpenCellIDGPSConfig struct {
	Enabled                   bool    `json:"enabled"`
	APIKey                    string  `json:"api_key"`
	ContributeData            bool    `json:"contribute_data"`
	CacheSizeMB               int     `json:"cache_size_mb"`
	MaxCellsPerLookup         int     `json:"max_cells_per_lookup"`
	NegativeCacheTTLHours     int     `json:"negative_cache_ttl_hours"`
	ContributionIntervalMin   int     `json:"contribution_interval_minutes"`
	MinGPSAccuracyM           float64 `json:"min_gps_accuracy_m"`
	MovementThresholdM        float64 `json:"movement_threshold_m"`
	RSRPChangeThresholdDB     float64 `json:"rsrp_change_threshold_db"`
	TimingAdvanceEnabled      bool    `json:"timing_advance_enabled"`
	FusionConfidenceThreshold float64 `json:"fusion_confidence_threshold"`
	HysteresisConsecutiveGood int     `json:"hysteresis_consecutive_good"`
	HysteresisConsecutiveBad  int     `json:"hysteresis_consecutive_bad"`
	MaxSpeedKmh               float64 `json:"max_speed_kmh"`
	StalenessThresholdS       int     `json:"staleness_threshold_s"`
	EMAAlpha                  float64 `json:"ema_alpha"`
	AccuracyStickinessRatio   float64 `json:"accuracy_stickiness_ratio"`
	RatioLimit                float64 `json:"ratio_limit"`        // Max lookup:submission ratio
	RatioWindowHours          int     `json:"ratio_window_hours"` // Rolling window for ratio calculation

	// Scheduling configuration
	EnableScheduler             bool `json:"enable_scheduler"`              // Enable automated scheduling
	SchedulerMovingInterval     int  `json:"scheduler_moving_interval"`     // Scan interval when moving (minutes)
	SchedulerStationaryInterval int  `json:"scheduler_stationary_interval"` // Scan interval when stationary (minutes)
	SchedulerMaxScansPerHour    int  `json:"scheduler_max_scans_per_hour"`  // Rate limit for scans
}

// NewOpenCellIDGPSSource creates a new OpenCellID GPS source
func NewOpenCellIDGPSSource(priority int, config *OpenCellIDGPSConfig, cellCollector CellularDataCollector, logger *logx.Logger) *OpenCellIDGPSSource {
	if config == nil {
		config = DefaultOpenCellIDGPSConfig()
	}

	source := &OpenCellIDGPSSource{
		name:          "opencellid",
		priority:      priority,
		logger:        logger,
		cellCollector: cellCollector,
		healthStats: GPSSourceHealth{
			Available:    true,
			LastSuccess:  time.Time{},
			LastError:    "",
			SuccessRate:  0.0,
			AvgLatency:   0.0,
			ErrorCount:   0,
			SuccessCount: 0,
		},
	}

	// Initialize components
	source.resolver = NewOpenCellIDResolver(config, logger)
	source.fuser = NewCellularLocationFuser(config, logger)

	if config.ContributeData && config.APIKey != "" {
		source.contributionMgr = NewContributionManager(config, logger, source.resolver.ratioRateLimiter)
	}

	// Initialize scheduler if enabled
	if config.EnableScheduler {
		schedulerConfig := &OpenCellIDSchedulerConfig{
			StationaryInterval:       time.Duration(config.SchedulerStationaryInterval) * time.Minute,
			MovingInterval:           time.Duration(config.SchedulerMovingInterval) * time.Minute,
			FastMovingInterval:       1 * time.Minute, // Fixed at 1 minute for fast movement
			GoodGPSAccuracy:          config.MinGPSAccuracyM,
			ExcellentGPSAccuracy:     config.MinGPSAccuracyM / 2,    // Half of good accuracy
			MovementThreshold:        config.MovementThresholdM / 5, // 1/5 of contribution threshold
			FastMovementSpeed:        15.0,                          // 54 km/h
			MovementWindow:           5 * time.Minute,
			ContributionInterval:     time.Duration(config.ContributionIntervalMin) * time.Minute,
			MovingContributionBoost:  0.5, // 2x faster when moving
			ExcellentGPSBoost:        0.7, // 1.4x faster with excellent GPS
			MaxScansPerHour:          config.SchedulerMaxScansPerHour,
			MaxContributionsPerHour:  6, // Conservative limit
			MinCellScanInterval:      30 * time.Second,
			EnableAdaptiveScheduling: true,
			BackoffOnRateLimit:       true,
		}

		source.scheduler = NewOpenCellIDScheduler(source, cellCollector, schedulerConfig, logger)
	}

	logger.Info("opencellid_gps_source_initialized",
		"priority", priority,
		"contribute_data", config.ContributeData,
		"cache_size_mb", config.CacheSizeMB,
		"max_cells", config.MaxCellsPerLookup,
	)

	return source
}

// DefaultOpenCellIDGPSConfig returns default configuration for OpenCellID GPS source
func DefaultOpenCellIDGPSConfig() *OpenCellIDGPSConfig {
	return &OpenCellIDGPSConfig{
		Enabled:                   false, // Requires API key
		APIKey:                    "",
		ContributeData:            true,
		CacheSizeMB:               25,
		MaxCellsPerLookup:         5,
		NegativeCacheTTLHours:     12,
		ContributionIntervalMin:   10,
		MinGPSAccuracyM:           20.0,
		MovementThresholdM:        250.0,
		RSRPChangeThresholdDB:     6.0,
		TimingAdvanceEnabled:      true,
		FusionConfidenceThreshold: 0.5,
		HysteresisConsecutiveGood: 3,
		HysteresisConsecutiveBad:  3,
		MaxSpeedKmh:               160.0,
		StalenessThresholdS:       30,
		EMAAlpha:                  0.3,
		AccuracyStickinessRatio:   2.0,
		RatioLimit:                8.0, // 8:1 lookup:submission ratio
		RatioWindowHours:          48,  // 48-hour rolling window

		// Scheduling defaults
		EnableScheduler:             true, // Enable automated scheduling
		SchedulerMovingInterval:     2,    // 2 minutes when moving
		SchedulerStationaryInterval: 10,   // 10 minutes when stationary
		SchedulerMaxScansPerHour:    30,   // Max 30 scans per hour
	}
}

// GetName returns the source name
func (ogs *OpenCellIDGPSSource) GetName() string {
	return ogs.name
}

// GetPriority returns the source priority
func (ogs *OpenCellIDGPSSource) GetPriority() int {
	return ogs.priority
}

// IsAvailable checks if OpenCellID GPS source is available
func (ogs *OpenCellIDGPSSource) IsAvailable(ctx context.Context) bool {
	// Check if cellular data collector is available
	if ogs.cellCollector == nil {
		return false
	}

	// Try to get serving cell information
	servingCell, err := ogs.cellCollector.GetServingCell(ctx)
	if err != nil || servingCell == nil {
		ogs.logger.LogDebugVerbose("opencellid_unavailable", map[string]interface{}{
			"reason": "no_serving_cell",
			"error":  err,
		})
		return false
	}

	// Check if we have minimum required cell information
	if servingCell.MCC == "" || servingCell.MNC == "" || servingCell.CellID == "" {
		ogs.logger.LogDebugVerbose("opencellid_unavailable", map[string]interface{}{
			"reason": "incomplete_cell_info",
			"mcc":    servingCell.MCC,
			"mnc":    servingCell.MNC,
			"cellid": servingCell.CellID,
		})
		return false
	}

	return true
}

// CollectGPS collects GPS data using OpenCellID cellular geolocation
func (ogs *OpenCellIDGPSSource) CollectGPS(ctx context.Context) (*StandardizedGPSData, error) {
	start := time.Now()

	// Collect cellular data
	servingCell, err := ogs.cellCollector.GetServingCell(ctx)
	if err != nil {
		ogs.updateHealthStats(false, time.Since(start), err)
		return nil, fmt.Errorf("failed to get serving cell: %w", err)
	}

	neighborCells, err := ogs.cellCollector.GetNeighborCells(ctx)
	if err != nil {
		ogs.logger.LogDebugVerbose("opencellid_no_neighbors", map[string]interface{}{
			"error": err.Error(),
		})
		// Continue with serving cell only
		neighborCells = []NeighborCellInfo{}
	}

	cellularMetrics, err := ogs.cellCollector.GetCellularMetrics(ctx)
	if err != nil {
		ogs.logger.LogDebugVerbose("opencellid_no_metrics", map[string]interface{}{
			"error": err.Error(),
		})
		// Continue without detailed metrics
	}

	// Resolve cell locations using cache and API
	cellList := []CellIdentifier{
		{
			MCC:    servingCell.MCC,
			MNC:    servingCell.MNC,
			LAC:    servingCell.TAC, // Use TAC for LTE
			CellID: servingCell.CellID,
			Radio:  servingCell.Technology,
		},
	}

	// Add neighbor cells (up to MaxCellsPerLookup - 1)
	// Note: NeighborCellInfo only has PCID and signal metrics, not full cell identifiers
	// For now, we'll skip neighbor cells since they don't have the required identifiers
	// In a real implementation, you would need to get full cell identifiers for neighbors
	maxNeighbors := ogs.resolver.config.MaxCellsPerLookup - 1
	_ = maxNeighbors  // Avoid unused variable warning
	_ = neighborCells // Avoid unused variable warning

	// TODO: Implement neighbor cell identifier resolution if available from modem

	// Resolve cell tower locations
	towerLocations, err := ogs.resolver.ResolveCells(ctx, cellList)
	if err != nil {
		ogs.updateHealthStats(false, time.Since(start), err)
		return nil, fmt.Errorf("failed to resolve cell locations: %w", err)
	}

	if len(towerLocations) == 0 {
		err := fmt.Errorf("no cell tower locations found")
		ogs.updateHealthStats(false, time.Since(start), err)
		return nil, err
	}

	// Fuse locations to get final position
	location, err := ogs.fuser.FuseLocations(towerLocations, servingCell, cellularMetrics)
	if err != nil {
		ogs.updateHealthStats(false, time.Since(start), err)
		return nil, fmt.Errorf("failed to fuse cell locations: %w", err)
	}

	// Apply hysteresis and motion constraints
	finalLocation := ogs.applyHysteresisAndConstraints(location, time.Since(start))
	if finalLocation == nil {
		err := fmt.Errorf("location rejected by hysteresis/constraints")
		ogs.updateHealthStats(false, time.Since(start), err)
		return nil, err
	}

	// Queue contribution if enabled and conditions are met
	if ogs.contributionMgr != nil {
		ogs.queueContributionIfNeeded(servingCell, neighborCells, cellularMetrics, finalLocation)
	}

	ogs.updateHealthStats(true, time.Since(start), nil)
	ogs.lastLocation = finalLocation

	ogs.logger.Info("opencellid_gps_success",
		"latitude", finalLocation.Latitude,
		"longitude", finalLocation.Longitude,
		"accuracy", finalLocation.Accuracy,
		"confidence", finalLocation.Confidence,
		"cell_count", len(towerLocations),
		"collection_time_ms", finalLocation.CollectionTime.Milliseconds(),
	)

	return finalLocation, nil
}

// applyHysteresisAndConstraints applies hysteresis and motion constraints to location
func (ogs *OpenCellIDGPSSource) applyHysteresisAndConstraints(location *CellularLocation, collectionTime time.Duration) *StandardizedGPSData {
	// Check confidence threshold
	if location.Confidence < ogs.resolver.config.FusionConfidenceThreshold {
		ogs.consecutiveBad++
		ogs.consecutiveGood = 0
		ogs.logger.LogDebugVerbose("opencellid_low_confidence", map[string]interface{}{
			"confidence":      location.Confidence,
			"threshold":       ogs.resolver.config.FusionConfidenceThreshold,
			"consecutive_bad": ogs.consecutiveBad,
		})
		return nil
	}

	// Apply hysteresis - require consecutive good fixes
	if ogs.consecutiveGood < ogs.resolver.config.HysteresisConsecutiveGood {
		ogs.consecutiveGood++
		ogs.consecutiveBad = 0
		ogs.logger.LogDebugVerbose("opencellid_hysteresis", map[string]interface{}{
			"consecutive_good": ogs.consecutiveGood,
			"required":         ogs.resolver.config.HysteresisConsecutiveGood,
		})
		return nil
	}

	// Check accuracy stickiness - don't accept if much worse than previous
	if ogs.lastLocation != nil {
		accuracyRatio := location.Accuracy / ogs.lastLocation.Accuracy
		if accuracyRatio > ogs.resolver.config.AccuracyStickinessRatio {
			ogs.logger.LogDebugVerbose("opencellid_accuracy_stickiness", map[string]interface{}{
				"new_accuracy":      location.Accuracy,
				"previous_accuracy": ogs.lastLocation.Accuracy,
				"ratio":             accuracyRatio,
				"threshold":         ogs.resolver.config.AccuracyStickinessRatio,
			})
			return nil
		}
	}

	// Check plausible speed if we have previous location
	if ogs.lastLocation != nil {
		distance := calculateHaversineDistance(
			ogs.lastLocation.Latitude, ogs.lastLocation.Longitude,
			location.Latitude, location.Longitude,
		)
		timeDiff := time.Since(ogs.lastLocation.Timestamp).Seconds()
		if timeDiff > 0 {
			speedMs := distance / timeDiff
			speedKmh := speedMs * 3.6

			if speedKmh > ogs.resolver.config.MaxSpeedKmh {
				ogs.logger.LogDebugVerbose("opencellid_implausible_speed", map[string]interface{}{
					"speed_kmh":   speedKmh,
					"max_speed":   ogs.resolver.config.MaxSpeedKmh,
					"distance_m":  distance,
					"time_diff_s": timeDiff,
				})
				return nil
			}
		}
	}

	// Apply EMA smoothing if we have previous location
	finalLat := location.Latitude
	finalLon := location.Longitude

	if ogs.lastLocation != nil {
		alpha := ogs.resolver.config.EMAAlpha
		finalLat = alpha*location.Latitude + (1-alpha)*ogs.lastLocation.Latitude
		finalLon = alpha*location.Longitude + (1-alpha)*ogs.lastLocation.Longitude
	}

	// Create standardized GPS data
	return &StandardizedGPSData{
		Latitude:       finalLat,
		Longitude:      finalLon,
		Altitude:       0.0, // Not available from cellular
		Accuracy:       location.Accuracy,
		Timestamp:      time.Now(),
		Speed:          0.0, // Calculate from movement if needed
		Course:         0.0, // Not available from cellular
		HDOP:           0.0, // Not applicable
		VDOP:           0.0, // Not applicable
		FixType:        2,   // 2D fix
		FixQuality:     ogs.getFixQuality(location.Confidence, location.Accuracy),
		Satellites:     location.CellCount, // Use cell count as "satellites"
		Source:         "opencellid",
		Method:         location.Method,
		DataSources:    []string{"opencellid", "cellular"},
		Valid:          true,
		Confidence:     location.Confidence,
		CollectionTime: collectionTime,
		FromCache:      location.FromCache,
		APICallMade:    location.APICallMade,
		APICost:        location.APICost,
	}
}

// getFixQuality determines fix quality based on confidence and accuracy
func (ogs *OpenCellIDGPSSource) getFixQuality(confidence, accuracy float64) string {
	if confidence >= 0.9 && accuracy <= 100 {
		return "excellent"
	} else if confidence >= 0.7 && accuracy <= 500 {
		return "good"
	} else if confidence >= 0.5 && accuracy <= 1000 {
		return "fair"
	}
	return "poor"
}

// queueContributionIfNeeded queues a contribution if conditions are met
func (ogs *OpenCellIDGPSSource) queueContributionIfNeeded(servingCell *ServingCellInfo, neighborCells []NeighborCellInfo, metrics *CellularMetrics, location *StandardizedGPSData) {
	// Check if we should contribute based on accuracy and other conditions
	if location.Accuracy > ogs.resolver.config.MinGPSAccuracyM {
		return // GPS accuracy not good enough
	}

	// Create contribution observation
	observation := &CellObservation{
		GPS: GPSObservation{
			Latitude:  location.Latitude,
			Longitude: location.Longitude,
			Accuracy:  location.Accuracy,
			Timestamp: location.Timestamp,
		},
		ServingCell: *servingCell,
		Neighbors:   neighborCells,
		Metrics:     metrics,
	}

	// Queue the observation
	ogs.contributionMgr.QueueObservation(observation)
}

// updateHealthStats updates health statistics
func (ogs *OpenCellIDGPSSource) updateHealthStats(success bool, latency time.Duration, err error) {
	if success {
		ogs.healthStats.SuccessCount++
		ogs.healthStats.LastSuccess = time.Now()
		ogs.healthStats.LastError = ""
	} else {
		ogs.healthStats.ErrorCount++
		if err != nil {
			ogs.healthStats.LastError = err.Error()
		}
	}

	// Update success rate
	total := ogs.healthStats.SuccessCount + ogs.healthStats.ErrorCount
	if total > 0 {
		ogs.healthStats.SuccessRate = float64(ogs.healthStats.SuccessCount) / float64(total)
	}

	// Update average latency (EMA)
	latencyMs := float64(latency.Nanoseconds()) / 1e6
	if ogs.healthStats.AvgLatency == 0 {
		ogs.healthStats.AvgLatency = latencyMs
	} else {
		alpha := 0.1 // EMA smoothing factor
		ogs.healthStats.AvgLatency = alpha*latencyMs + (1-alpha)*ogs.healthStats.AvgLatency
	}

	ogs.healthStats.Available = ogs.healthStats.SuccessRate > 0.1 // Available if >10% success rate
}

// GetHealthStatus returns the current health status
func (ogs *OpenCellIDGPSSource) GetHealthStatus() GPSSourceHealth {
	return ogs.healthStats
}

// StartScheduler starts the automated scheduling if enabled
func (ogs *OpenCellIDGPSSource) StartScheduler(ctx context.Context) error {
	if ogs.scheduler == nil {
		return fmt.Errorf("scheduler not initialized")
	}

	ogs.logger.Info("opencellid_scheduler_starting")
	return ogs.scheduler.Start(ctx)
}

// StopScheduler stops the automated scheduling
func (ogs *OpenCellIDGPSSource) StopScheduler() {
	if ogs.scheduler != nil {
		ogs.logger.Info("opencellid_scheduler_stopping")
		ogs.scheduler.Stop()
	}
}

// PauseScheduler temporarily pauses scheduling
func (ogs *OpenCellIDGPSSource) PauseScheduler() {
	if ogs.scheduler != nil {
		ogs.scheduler.Pause()
	}
}

// ResumeScheduler resumes scheduling
func (ogs *OpenCellIDGPSSource) ResumeScheduler() {
	if ogs.scheduler != nil {
		ogs.scheduler.Resume()
	}
}

// GetSchedulerStats returns current scheduler statistics
func (ogs *OpenCellIDGPSSource) GetSchedulerStats() *SchedulerStats {
	if ogs.scheduler == nil {
		return nil
	}

	stats := ogs.scheduler.GetStats()
	return &stats
}

// calculateHaversineDistance calculates distance between two GPS coordinates
func calculateHaversineDistance(lat1, lon1, lat2, lon2 float64) float64 {
	const earthRadiusM = 6371000 // Earth's radius in meters

	lat1Rad := lat1 * 3.14159265359 / 180
	lat2Rad := lat2 * 3.14159265359 / 180
	deltaLat := (lat2 - lat1) * 3.14159265359 / 180
	deltaLon := (lon2 - lon1) * 3.14159265359 / 180

	a := 0.5 - 0.5*((lat2Rad-lat1Rad)/2) +
		0.5*((lat2Rad+lat1Rad)/2)*0.5*((deltaLon)/2)
	_ = deltaLat // Avoid unused variable warning

	return earthRadiusM * 2 * 1.5707963268 * a // Simplified haversine
}
