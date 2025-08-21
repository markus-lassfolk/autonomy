package gps

// EnhancedOpenCellIDConfig incorporates all PM feedback for production robustness
type EnhancedOpenCellIDConfig struct {
	// Basic Configuration
	Enabled           bool   `json:"enabled"`
	APIKey            string `json:"api_key"`
	ContributeData    bool   `json:"contribute_data"`
	CacheSizeMB       int    `json:"cache_size_mb"`
	MaxCellsPerLookup int    `json:"max_cells_per_lookup"`

	// GPS and Movement Thresholds
	MinGPSAccuracyM       float64 `json:"min_gps_accuracy_m"`
	MovementThresholdM    float64 `json:"movement_threshold_m"`
	RSRPChangeThresholdDB float64 `json:"rsrp_change_threshold_db"`

	// Enhanced Rate Limiting (PM #1, #2)
	RatioLimit            float64 `json:"ratio_limit"`              // Configurable ratio (not hardcoded)
	RatioWindowHours      int     `json:"ratio_window_hours"`       // Rolling window in hours
	MaxLookupsPerHour     int     `json:"max_lookups_per_hour"`     // Hard ceiling per hour
	MaxSubmissionsPerHour int     `json:"max_submissions_per_hour"` // Hard ceiling per hour
	MaxSubmissionsPerDay  int     `json:"max_submissions_per_day"`  // Hard ceiling per day
	MinTricklePerHour     int     `json:"min_trickle_per_hour"`     // Minimum trickle submissions

	// Persistence (PM #3)
	PersistStatePath       string `json:"persist_state_path"`       // Path to persist rate limiter state
	PersistSubmissionQueue bool   `json:"persist_submission_queue"` // Persist submission queue

	// Negative Cache with Jitter (PM #4)
	NegativeCacheBaseTTLH   int `json:"negative_cache_base_ttl_h"`   // Base TTL for negative cache
	NegativeCacheJitterMinH int `json:"negative_cache_jitter_min_h"` // Min jitter hours (10h)
	NegativeCacheJitterMaxH int `json:"negative_cache_jitter_max_h"` // Max jitter hours (14h)

	// Submission Deduplication (PM #5)
	DedupeGridSizeM   float64 `json:"dedupe_grid_size_m"`   // Quantize location to ~50-100m grid
	DedupeTimeWindowH int     `json:"dedupe_time_window_h"` // Hour bucket for deduplication

	// Stationary Caps (PM #6)
	StationaryMaxIntervalH int `json:"stationary_max_interval_h"` // Max 1 per cell per 2-4h when stationary
	StationaryGlobalCapH   int `json:"stationary_global_cap_h"`   // Global cap per hour when stationary

	// Burst Smoothing (PM #7)
	BurstBatchSize      int `json:"burst_batch_size"`       // Small batches when reconnecting
	BurstDelayMs        int `json:"burst_delay_ms"`         // Sleep between batches
	OfflineQueueMaxSize int `json:"offline_queue_max_size"` // Max offline queue size

	// Clock Sanity (PM #8)
	MaxClockSkewMin int `json:"max_clock_skew_min"` // Max ±minutes from now

	// Neighbor Selection (PM #9)
	TopNeighborCount    int  `json:"top_neighbor_count"`    // Top neighbors by RSRP
	RandomNeighborCount int  `json:"random_neighbor_count"` // Random neighbors from next tier
	AvoidSectorBias     bool `json:"avoid_sector_bias"`     // Enable bias avoidance

	// Comprehensive Metrics (PM #10)
	EnableDetailedMetrics    bool `json:"enable_detailed_metrics"`    // Enable comprehensive metrics
	MetricsReportingInterval int  `json:"metrics_reporting_interval"` // Minutes between metric reports

	// Scheduler Configuration
	EnableScheduler             bool `json:"enable_scheduler"`              // Enable automated scheduling
	SchedulerMovingInterval     int  `json:"scheduler_moving_interval"`     // Minutes between scans when moving
	SchedulerStationaryInterval int  `json:"scheduler_stationary_interval"` // Minutes between scans when stationary
	SchedulerMaxScansPerHour    int  `json:"scheduler_max_scans_per_hour"`  // Maximum scans per hour

	// Fusion and Quality
	TimingAdvanceEnabled      bool    `json:"timing_advance_enabled"`
	FusionConfidenceThreshold float64 `json:"fusion_confidence_threshold"`
	HysteresisConsecutiveGood int     `json:"hysteresis_consecutive_good"`
	HysteresisConsecutiveBad  int     `json:"hysteresis_consecutive_bad"`
	MaxSpeedKmh               float64 `json:"max_speed_kmh"`
	StalenessThresholdS       int     `json:"staleness_threshold_s"`
	EMAAlpha                  float64 `json:"ema_alpha"`
	AccuracyStickinessRatio   float64 `json:"accuracy_stickiness_ratio"`
}

// DefaultEnhancedOpenCellIDConfig returns production-ready defaults incorporating PM feedback
func DefaultEnhancedOpenCellIDConfig() *EnhancedOpenCellIDConfig {
	return &EnhancedOpenCellIDConfig{
		// Basic Configuration
		Enabled:           true,
		APIKey:            "", // Must be configured
		ContributeData:    true,
		CacheSizeMB:       25,
		MaxCellsPerLookup: 5,

		// GPS and Movement Thresholds
		MinGPSAccuracyM:       20.0,
		MovementThresholdM:    250.0,
		RSRPChangeThresholdDB: 6.0,

		// Enhanced Rate Limiting (PM #1, #2)
		RatioLimit:            8.0, // Configurable 8:1 ratio (safety margin vs 10:1)
		RatioWindowHours:      48,  // 48-hour rolling window
		MaxLookupsPerHour:     30,  // Hard ceiling (PM #1)
		MaxSubmissionsPerHour: 6,   // Hard ceiling (PM #1)
		MaxSubmissionsPerDay:  50,  // Daily hard ceiling (PM #1)
		MinTricklePerHour:     1,   // Minimum trickle when moving (PM #1)

		// Persistence (PM #3)
		PersistStatePath:       "/overlay/autonomy/opencellid_state.json",
		PersistSubmissionQueue: true,

		// Negative Cache with Jitter (PM #4)
		NegativeCacheBaseTTLH:   12, // 12 hour base
		NegativeCacheJitterMinH: 10, // 10-14 hour jitter range (PM #4)
		NegativeCacheJitterMaxH: 14,

		// Submission Deduplication (PM #5)
		DedupeGridSizeM:   75.0, // ~75m grid for deduplication
		DedupeTimeWindowH: 1,    // 1-hour buckets

		// Stationary Caps (PM #6)
		StationaryMaxIntervalH: 3, // Max 1 per cell per 3h when stationary
		StationaryGlobalCapH:   2, // Max 2/hour when stationary

		// Burst Smoothing (PM #7)
		BurstBatchSize:      3,    // Small batches
		BurstDelayMs:        2000, // 2s between batches
		OfflineQueueMaxSize: 100,  // Max 100 offline submissions

		// Clock Sanity (PM #8)
		MaxClockSkewMin: 15, // ±15 minutes max

		// Neighbor Selection (PM #9)
		TopNeighborCount:    3,    // Top 3 by RSRP
		RandomNeighborCount: 2,    // 2 random from next tier
		AvoidSectorBias:     true, // Enable bias avoidance

		// Comprehensive Metrics (PM #10)
		EnableDetailedMetrics:    true,
		MetricsReportingInterval: 15, // Report every 15 minutes

		// Scheduler Configuration
		EnableScheduler:             true,
		SchedulerMovingInterval:     2,  // 2 minutes when moving
		SchedulerStationaryInterval: 10, // 10 minutes when stationary
		SchedulerMaxScansPerHour:    20, // Max 20 scans/hour

		// Fusion and Quality (existing)
		TimingAdvanceEnabled:      true,
		FusionConfidenceThreshold: 0.7,
		HysteresisConsecutiveGood: 3,
		HysteresisConsecutiveBad:  2,
		MaxSpeedKmh:               200.0,
		StalenessThresholdS:       300,
		EMAAlpha:                  0.3,
		AccuracyStickinessRatio:   1.5,
	}
}

// ValidateConfig validates the enhanced configuration
func (config *EnhancedOpenCellIDConfig) ValidateConfig() []string {
	var issues []string

	// Basic validation
	if config.APIKey == "" {
		issues = append(issues, "api_key is required")
	}

	// Rate limiting validation (PM #1, #2)
	if config.RatioLimit <= 0 {
		issues = append(issues, "ratio_limit must be positive")
	}
	if config.RatioLimit > 10.0 {
		issues = append(issues, "ratio_limit should not exceed 10.0 (OpenCellID limit)")
	}
	if config.MaxLookupsPerHour <= 0 {
		issues = append(issues, "max_lookups_per_hour must be positive")
	}
	if config.MaxSubmissionsPerHour <= 0 {
		issues = append(issues, "max_submissions_per_hour must be positive")
	}
	if config.MaxSubmissionsPerDay <= 0 {
		issues = append(issues, "max_submissions_per_day must be positive")
	}

	// Negative cache validation (PM #4)
	if config.NegativeCacheJitterMinH >= config.NegativeCacheJitterMaxH {
		issues = append(issues, "negative_cache_jitter_min_h must be less than jitter_max_h")
	}

	// Deduplication validation (PM #5)
	if config.DedupeGridSizeM <= 0 {
		issues = append(issues, "dedupe_grid_size_m must be positive")
	}

	// Burst smoothing validation (PM #7)
	if config.BurstBatchSize <= 0 {
		issues = append(issues, "burst_batch_size must be positive")
	}
	if config.BurstDelayMs < 0 {
		issues = append(issues, "burst_delay_ms must be non-negative")
	}

	// Clock sanity validation (PM #8)
	if config.MaxClockSkewMin <= 0 {
		issues = append(issues, "max_clock_skew_min must be positive")
	}

	// Neighbor selection validation (PM #9)
	if config.TopNeighborCount <= 0 {
		issues = append(issues, "top_neighbor_count must be positive")
	}
	if config.RandomNeighborCount < 0 {
		issues = append(issues, "random_neighbor_count must be non-negative")
	}

	return issues
}

// GetPolicyCompliantSettings returns settings that ensure OpenCellID policy compliance
func (config *EnhancedOpenCellIDConfig) GetPolicyCompliantSettings() map[string]interface{} {
	return map[string]interface{}{
		"policy_version": "2024-01-enhanced",
		"compliance_features": map[string]bool{
			"ratio_based_limiting":     true,
			"hard_ceilings":            true,
			"configurable_ratios":      true,
			"persistent_state":         config.PersistStatePath != "",
			"jittered_negative_cache":  config.NegativeCacheJitterMinH != config.NegativeCacheJitterMaxH,
			"submission_deduplication": config.DedupeGridSizeM > 0,
			"stationary_caps":          config.StationaryMaxIntervalH > 0,
			"burst_smoothing":          config.BurstBatchSize > 0,
			"clock_sanity_checks":      config.MaxClockSkewMin > 0,
			"bias_avoidance":           config.AvoidSectorBias,
			"comprehensive_metrics":    config.EnableDetailedMetrics,
		},
		"rate_limits": map[string]interface{}{
			"ratio_limit":              config.RatioLimit,
			"ratio_window_hours":       config.RatioWindowHours,
			"max_lookups_per_hour":     config.MaxLookupsPerHour,
			"max_submissions_per_hour": config.MaxSubmissionsPerHour,
			"max_submissions_per_day":  config.MaxSubmissionsPerDay,
			"min_trickle_per_hour":     config.MinTricklePerHour,
		},
		"quality_gates": map[string]interface{}{
			"min_gps_accuracy_m":       config.MinGPSAccuracyM,
			"movement_threshold_m":     config.MovementThresholdM,
			"rsrp_change_threshold_db": config.RSRPChangeThresholdDB,
			"dedupe_grid_size_m":       config.DedupeGridSizeM,
			"max_clock_skew_min":       config.MaxClockSkewMin,
		},
	}
}
