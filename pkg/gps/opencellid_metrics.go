package gps

import (
	"sync"
	"time"

	"github.com/markus-lassfolk/autonomy/pkg/logx"
)

// OpenCellIDMetricsCollector implements comprehensive metrics tracking (PM #10)
type OpenCellIDMetricsCollector struct {
	// Rate limiting metrics
	LookupsPerHour     int64   `json:"lookups_per_hour"`
	SubmissionsPerHour int64   `json:"submissions_per_hour"`
	CurrentRatio       float64 `json:"current_ratio"`
	DroppedByRatio     int64   `json:"dropped_by_ratio"`
	DroppedByCeilings  int64   `json:"dropped_by_ceilings"`

	// Cache metrics
	CacheHitRate         float64 `json:"cache_hit_rate"`
	NegativeCacheHitRate float64 `json:"negative_cache_hit_rate"`
	CacheSize            int     `json:"cache_size"`
	NegativeCacheSize    int     `json:"negative_cache_size"`

	// Submission reason breakdown (PM #10)
	NewCellSubmissions    int64 `json:"new_cell_submissions"`
	MovementSubmissions   int64 `json:"movement_submissions"`
	RSRPChangeSubmissions int64 `json:"rsrp_change_submissions"`
	ValidationTrickle     int64 `json:"validation_trickle"`

	// Queue and batch metrics
	QueueDepth       int     `json:"queue_depth"`
	AverageBatchSize float64 `json:"average_batch_size"`
	OfflineQueueSize int     `json:"offline_queue_size"`

	// API response metrics
	APISuccessRate      float64          `json:"api_success_rate"`
	APIErrorCodes       map[string]int64 `json:"api_error_codes"`
	AverageResponseTime float64          `json:"average_response_time_ms"`

	// Compliance metrics
	PolicyViolations int64   `json:"policy_violations"`
	ComplianceScore  float64 `json:"compliance_score"`

	// Deduplication metrics (PM #5)
	DuplicatesBlocked  int64 `json:"duplicates_blocked"`
	FingerprintsActive int   `json:"fingerprints_active"`

	// Stationary behavior metrics (PM #6)
	StationaryBlocked     int64 `json:"stationary_blocked"`
	StationarySubmissions int64 `json:"stationary_submissions"`

	// Burst smoothing metrics (PM #7)
	BurstBatchesProcessed int64 `json:"burst_batches_processed"`
	BurstSmoothingActive  bool  `json:"burst_smoothing_active"`

	// Clock sanity metrics (PM #8)
	TimestampsClamped int64 `json:"timestamps_clamped"`
	ClockSkewDetected int64 `json:"clock_skew_detected"`

	// Neighbor selection metrics (PM #9)
	BiasedSelectionAvoided  int64 `json:"biased_selection_avoided"`
	RandomNeighborsSelected int64 `json:"random_neighbors_selected"`

	// Internal tracking
	startTime time.Time
	lastReset time.Time
	logger    *logx.Logger
	mu        sync.RWMutex
}

// NewOpenCellIDMetricsCollector creates a new metrics collector
func NewOpenCellIDMetricsCollector(logger *logx.Logger) *OpenCellIDMetricsCollector {
	now := time.Now()
	return &OpenCellIDMetricsCollector{
		APIErrorCodes: make(map[string]int64),
		startTime:     now,
		lastReset:     now,
		logger:        logger,
	}
}

// RecordLookup records a lookup attempt
func (omc *OpenCellIDMetricsCollector) RecordLookup(success bool, responseTimeMs float64) {
	omc.mu.Lock()
	defer omc.mu.Unlock()

	omc.LookupsPerHour++

	if success {
		// Update response time (simple moving average)
		if omc.AverageResponseTime == 0 {
			omc.AverageResponseTime = responseTimeMs
		} else {
			omc.AverageResponseTime = (omc.AverageResponseTime * 0.9) + (responseTimeMs * 0.1)
		}
	}
}

// RecordSubmission records a submission with reason breakdown (PM #10)
func (omc *OpenCellIDMetricsCollector) RecordSubmission(reason string, success bool) {
	omc.mu.Lock()
	defer omc.mu.Unlock()

	omc.SubmissionsPerHour++

	// Track submission reasons (PM #10)
	switch reason {
	case "new_cell_observed", "new_cell":
		omc.NewCellSubmissions++
	case "movement":
		omc.MovementSubmissions++
	case "rsrp_change":
		omc.RSRPChangeSubmissions++
	case "validation_trickle", "trickle":
		omc.ValidationTrickle++
	}

	// Update current ratio
	if omc.SubmissionsPerHour > 0 {
		omc.CurrentRatio = float64(omc.LookupsPerHour) / float64(omc.SubmissionsPerHour)
	}
}

// RecordRateLimitDrop records when requests are dropped by rate limiting
func (omc *OpenCellIDMetricsCollector) RecordRateLimitDrop(reason string) {
	omc.mu.Lock()
	defer omc.mu.Unlock()

	switch reason {
	case "ratio":
		omc.DroppedByRatio++
	case "ceiling", "hourly_ceiling", "daily_ceiling":
		omc.DroppedByCeilings++
	}
}

// RecordCacheHit records cache performance
func (omc *OpenCellIDMetricsCollector) RecordCacheHit(isHit bool, isNegativeCache bool) {
	omc.mu.Lock()
	defer omc.mu.Unlock()

	// Simple hit rate calculation (would be more sophisticated in production)
	if isNegativeCache {
		if isHit {
			omc.NegativeCacheHitRate = (omc.NegativeCacheHitRate * 0.95) + (1.0 * 0.05)
		} else {
			omc.NegativeCacheHitRate = (omc.NegativeCacheHitRate * 0.95) + (0.0 * 0.05)
		}
	} else {
		if isHit {
			omc.CacheHitRate = (omc.CacheHitRate * 0.95) + (1.0 * 0.05)
		} else {
			omc.CacheHitRate = (omc.CacheHitRate * 0.95) + (0.0 * 0.05)
		}
	}
}

// RecordAPIError records API error codes for debugging (PM #10)
func (omc *OpenCellIDMetricsCollector) RecordAPIError(errorCode string) {
	omc.mu.Lock()
	defer omc.mu.Unlock()

	if omc.APIErrorCodes == nil {
		omc.APIErrorCodes = make(map[string]int64)
	}

	omc.APIErrorCodes[errorCode]++
}

// RecordDeduplication records deduplication events (PM #5)
func (omc *OpenCellIDMetricsCollector) RecordDeduplication(blocked bool, activeFingerprintCount int) {
	omc.mu.Lock()
	defer omc.mu.Unlock()

	if blocked {
		omc.DuplicatesBlocked++
	}
	omc.FingerprintsActive = activeFingerprintCount
}

// RecordStationaryBehavior records stationary submission behavior (PM #6)
func (omc *OpenCellIDMetricsCollector) RecordStationaryBehavior(blocked bool, submitted bool) {
	omc.mu.Lock()
	defer omc.mu.Unlock()

	if blocked {
		omc.StationaryBlocked++
	}
	if submitted {
		omc.StationarySubmissions++
	}
}

// RecordBurstSmoothing records burst smoothing activity (PM #7)
func (omc *OpenCellIDMetricsCollector) RecordBurstSmoothing(batchProcessed bool, isActive bool, queueSize int) {
	omc.mu.Lock()
	defer omc.mu.Unlock()

	if batchProcessed {
		omc.BurstBatchesProcessed++
	}
	omc.BurstSmoothingActive = isActive
	omc.OfflineQueueSize = queueSize
}

// RecordClockSanity records clock sanity check events (PM #8)
func (omc *OpenCellIDMetricsCollector) RecordClockSanity(timestampClamped bool, skewDetected bool) {
	omc.mu.Lock()
	defer omc.mu.Unlock()

	if timestampClamped {
		omc.TimestampsClamped++
	}
	if skewDetected {
		omc.ClockSkewDetected++
	}
}

// RecordNeighborSelection records neighbor selection bias avoidance (PM #9)
func (omc *OpenCellIDMetricsCollector) RecordNeighborSelection(biasAvoided bool, randomCount int) {
	omc.mu.Lock()
	defer omc.mu.Unlock()

	if biasAvoided {
		omc.BiasedSelectionAvoided++
	}
	omc.RandomNeighborsSelected += int64(randomCount)
}

// UpdateQueueMetrics updates queue-related metrics
func (omc *OpenCellIDMetricsCollector) UpdateQueueMetrics(queueDepth int, averageBatchSize float64) {
	omc.mu.Lock()
	defer omc.mu.Unlock()

	omc.QueueDepth = queueDepth
	omc.AverageBatchSize = averageBatchSize
}

// UpdateCacheSizes updates cache size metrics
func (omc *OpenCellIDMetricsCollector) UpdateCacheSizes(cacheSize, negativeCacheSize int) {
	omc.mu.Lock()
	defer omc.mu.Unlock()

	omc.CacheSize = cacheSize
	omc.NegativeCacheSize = negativeCacheSize
}

// CalculateComplianceScore calculates overall compliance score
func (omc *OpenCellIDMetricsCollector) CalculateComplianceScore() float64 {
	omc.mu.RLock()
	defer omc.mu.RUnlock()

	score := 100.0

	// Deduct points for policy violations
	if omc.CurrentRatio > 10.0 {
		score -= 20.0 // Major violation
	} else if omc.CurrentRatio > 8.0 {
		score -= 5.0 // Minor violation
	}

	// Deduct for excessive drops
	totalRequests := omc.LookupsPerHour + omc.SubmissionsPerHour
	if totalRequests > 0 {
		dropRate := float64(omc.DroppedByRatio+omc.DroppedByCeilings) / float64(totalRequests)
		if dropRate > 0.1 {
			score -= 10.0 // High drop rate
		}
	}

	// Bonus for good cache hit rate
	if omc.CacheHitRate > 0.8 {
		score += 5.0
	}

	// Ensure score is in valid range
	if score < 0 {
		score = 0
	}
	if score > 100 {
		score = 100
	}

	return score
}

// GetDetailedReport returns comprehensive metrics report (PM #10)
func (omc *OpenCellIDMetricsCollector) GetDetailedReport() map[string]interface{} {
	omc.mu.RLock()
	defer omc.mu.RUnlock()

	uptime := time.Since(omc.startTime)
	timeSinceReset := time.Since(omc.lastReset)

	return map[string]interface{}{
		"timestamp":    time.Now().Format(time.RFC3339),
		"uptime_hours": uptime.Hours(),
		"period_hours": timeSinceReset.Hours(),

		// Rate limiting compliance (PM #1, #2)
		"rate_limiting": map[string]interface{}{
			"lookups_per_hour":     omc.LookupsPerHour,
			"submissions_per_hour": omc.SubmissionsPerHour,
			"current_ratio":        omc.CurrentRatio,
			"dropped_by_ratio":     omc.DroppedByRatio,
			"dropped_by_ceilings":  omc.DroppedByCeilings,
		},

		// Cache performance
		"cache_performance": map[string]interface{}{
			"cache_hit_rate":          omc.CacheHitRate,
			"negative_cache_hit_rate": omc.NegativeCacheHitRate,
			"cache_size":              omc.CacheSize,
			"negative_cache_size":     omc.NegativeCacheSize,
		},

		// Submission breakdown (PM #10)
		"submission_reasons": map[string]interface{}{
			"new_cell_submissions":    omc.NewCellSubmissions,
			"movement_submissions":    omc.MovementSubmissions,
			"rsrp_change_submissions": omc.RSRPChangeSubmissions,
			"validation_trickle":      omc.ValidationTrickle,
		},

		// Queue metrics (PM #10)
		"queue_metrics": map[string]interface{}{
			"queue_depth":        omc.QueueDepth,
			"average_batch_size": omc.AverageBatchSize,
			"offline_queue_size": omc.OfflineQueueSize,
		},

		// API performance (PM #10)
		"api_performance": map[string]interface{}{
			"success_rate":        omc.APISuccessRate,
			"average_response_ms": omc.AverageResponseTime,
			"error_codes":         omc.APIErrorCodes,
		},

		// Enhanced features (PM feedback)
		"enhanced_features": map[string]interface{}{
			// Deduplication (PM #5)
			"deduplication": map[string]interface{}{
				"duplicates_blocked":  omc.DuplicatesBlocked,
				"fingerprints_active": omc.FingerprintsActive,
			},

			// Stationary behavior (PM #6)
			"stationary_behavior": map[string]interface{}{
				"stationary_blocked":     omc.StationaryBlocked,
				"stationary_submissions": omc.StationarySubmissions,
			},

			// Burst smoothing (PM #7)
			"burst_smoothing": map[string]interface{}{
				"batches_processed": omc.BurstBatchesProcessed,
				"smoothing_active":  omc.BurstSmoothingActive,
			},

			// Clock sanity (PM #8)
			"clock_sanity": map[string]interface{}{
				"timestamps_clamped":  omc.TimestampsClamped,
				"clock_skew_detected": omc.ClockSkewDetected,
			},

			// Neighbor selection (PM #9)
			"neighbor_selection": map[string]interface{}{
				"biased_selection_avoided":  omc.BiasedSelectionAvoided,
				"random_neighbors_selected": omc.RandomNeighborsSelected,
			},
		},

		// Compliance
		"compliance": map[string]interface{}{
			"policy_violations": omc.PolicyViolations,
			"compliance_score":  omc.CalculateComplianceScore(),
		},
	}
}

// ResetHourlyCounters resets hourly counters (called by scheduler)
func (omc *OpenCellIDMetricsCollector) ResetHourlyCounters() {
	omc.mu.Lock()
	defer omc.mu.Unlock()

	omc.logger.Info("opencellid_metrics_hourly_reset",
		"lookups_last_hour", omc.LookupsPerHour,
		"submissions_last_hour", omc.SubmissionsPerHour,
		"ratio_last_hour", omc.CurrentRatio,
	)

	omc.LookupsPerHour = 0
	omc.SubmissionsPerHour = 0
	omc.CurrentRatio = 0
	omc.lastReset = time.Now()
}

// LogComplianceReport logs a comprehensive compliance report
func (omc *OpenCellIDMetricsCollector) LogComplianceReport() {
	report := omc.GetDetailedReport()

	omc.logger.Info("opencellid_compliance_report",
		"report", report,
	)
}
