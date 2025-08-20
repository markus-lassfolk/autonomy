package adaptive

import (
	"context"
	"sync"
	"time"

	"github.com/markus-lassfolk/autonomy/pkg/logx"
)

// RateOptimizer provides intelligent sampling rate optimization based on system performance and data usage
type RateOptimizer struct {
	config *RateOptimizerConfig
	logger *logx.Logger
	mu     sync.RWMutex

	// Performance tracking
	performanceHistory []PerformanceMetric
	fallBehindCount    int
	lastOptimization   time.Time

	// Rate optimization state
	currentRate       time.Duration
	optimizationLevel int // 0-5, where 0 is most aggressive, 5 is most conservative
}

// RateOptimizerConfig holds configuration for the rate optimizer
type RateOptimizerConfig struct {
	Enabled             bool          `json:"enabled"`
	BaseInterval        time.Duration `json:"base_interval"`         // Base sampling interval (1s)
	MaxInterval         time.Duration `json:"max_interval"`          // Maximum sampling interval (120s)
	MinInterval         time.Duration `json:"min_interval"`          // Minimum sampling interval (1s)
	FallBehindThreshold int           `json:"fall_behind_threshold"` // Number of samples behind before optimization
	OptimizationWindow  time.Duration `json:"optimization_window"`   // Time window for optimization decisions
	GradualOptimization bool          `json:"gradual_optimization"`  // Use gradual vs immediate optimization
	PerformanceWeight   float64       `json:"performance_weight"`    // Weight for performance vs data usage
	DataUsageWeight     float64       `json:"data_usage_weight"`     // Weight for data usage vs performance
}

// PerformanceMetric represents a performance measurement
type PerformanceMetric struct {
	Timestamp      time.Time     `json:"timestamp"`
	ProcessingTime time.Duration `json:"processing_time"`
	MemoryUsage    int64         `json:"memory_usage"`
	CPUUsage       float64       `json:"cpu_usage"`
	QueueDepth     int           `json:"queue_depth"`
	DataUsage      float64       `json:"data_usage"` // MB per hour
}

// DefaultRateOptimizerConfig returns default rate optimizer configuration
func DefaultRateOptimizerConfig() *RateOptimizerConfig {
	return &RateOptimizerConfig{
		Enabled:             true,
		BaseInterval:        1 * time.Second,
		MaxInterval:         120 * time.Second,
		MinInterval:         1 * time.Second,
		FallBehindThreshold: 3,
		OptimizationWindow:  5 * time.Minute,
		GradualOptimization: true,
		PerformanceWeight:   0.6,
		DataUsageWeight:     0.4,
	}
}

// NewRateOptimizer creates a new rate optimizer
func NewRateOptimizer(config *RateOptimizerConfig, logger *logx.Logger) *RateOptimizer {
	if config == nil {
		config = DefaultRateOptimizerConfig()
	}

	return &RateOptimizer{
		config:             config,
		logger:             logger,
		performanceHistory: make([]PerformanceMetric, 0),
		currentRate:        config.BaseInterval,
		optimizationLevel:  2, // Start at moderate optimization
	}
}

// AddPerformanceMetric adds a new performance metric for analysis
func (ro *RateOptimizer) AddPerformanceMetric(metric *PerformanceMetric) error {
	if !ro.config.Enabled {
		return nil
	}

	ro.mu.Lock()
	defer ro.mu.Unlock()

	// Add to history
	ro.performanceHistory = append(ro.performanceHistory, *metric)

	// Maintain history window
	ro.maintainHistoryWindow()

	// Check for fall-behind condition
	if metric.QueueDepth > ro.config.FallBehindThreshold {
		ro.fallBehindCount++
		ro.logger.Warn("Fall-behind detected", "queue_depth", metric.QueueDepth, "threshold", ro.config.FallBehindThreshold)
	} else {
		ro.fallBehindCount = 0
	}

	// Log performance metric
	ro.logger.LogDataFlow("adaptive_sampling", "performance_metric", "rate_optimizer", 1, map[string]interface{}{
		"processing_time": metric.ProcessingTime.Milliseconds(),
		"memory_usage":    metric.MemoryUsage,
		"cpu_usage":       metric.CPUUsage,
		"queue_depth":     metric.QueueDepth,
		"data_usage":      metric.DataUsage,
	})

	return nil
}

// GetOptimalRate returns the optimal sampling rate based on current performance and data usage
func (ro *RateOptimizer) GetOptimalRate(ctx context.Context, connectionType ConnectionType, dataUsage float64) time.Duration {
	if !ro.config.Enabled {
		return ro.config.BaseInterval
	}

	ro.mu.RLock()
	defer ro.mu.RUnlock()

	// Start with base rate for connection type
	baseRate := ro.getBaseRateForConnectionType(connectionType)

	// Apply performance-based optimization
	performanceRate := ro.calculatePerformanceBasedRate()

	// Apply data usage optimization
	dataUsageRate := ro.calculateDataUsageBasedRate(dataUsage)

	// Combine optimizations with weights
	optimalRate := ro.combineRates(performanceRate, dataUsageRate)

	// Ensure rate is within bounds
	optimalRate = ro.clampRate(optimalRate)

	// Apply gradual optimization if enabled
	if ro.config.GradualOptimization {
		optimalRate = ro.applyGradualOptimization(optimalRate)
	}

	// Update current rate
	ro.currentRate = optimalRate

	ro.logger.Debug("Rate optimization calculated",
		"connection_type", connectionType,
		"base_rate", baseRate,
		"performance_rate", performanceRate,
		"data_usage_rate", dataUsageRate,
		"optimal_rate", optimalRate,
		"optimization_level", ro.optimizationLevel)

	return optimalRate
}

// getBaseRateForConnectionType returns the base rate for a connection type
func (ro *RateOptimizer) getBaseRateForConnectionType(connectionType ConnectionType) time.Duration {
	switch connectionType {
	case ConnectionTypeStarlink:
		return 5 * time.Second
	case ConnectionTypeCellular:
		return 30 * time.Second
	case ConnectionTypeWiFi:
		return 10 * time.Second
	case ConnectionTypeLAN:
		return 5 * time.Second
	case ConnectionTypeUnknown:
		return 10 * time.Second
	default:
		return ro.config.BaseInterval
	}
}

// calculatePerformanceBasedRate calculates sampling rate based on performance metrics
func (ro *RateOptimizer) calculatePerformanceBasedRate() time.Duration {
	if len(ro.performanceHistory) == 0 {
		return ro.config.BaseInterval
	}

	// Calculate average performance metrics
	var avgProcessingTime time.Duration
	var avgMemoryUsage int64
	var avgCPUUsage float64
	var avgQueueDepth int

	for _, metric := range ro.performanceHistory {
		avgProcessingTime += metric.ProcessingTime
		avgMemoryUsage += metric.MemoryUsage
		avgCPUUsage += metric.CPUUsage
		avgQueueDepth += metric.QueueDepth
	}

	count := len(ro.performanceHistory)
	avgProcessingTime /= time.Duration(count)
	avgMemoryUsage /= int64(count)
	avgCPUUsage /= float64(count)
	avgQueueDepth /= count

	// Determine performance-based rate adjustment
	rateMultiplier := 1.0

	// Adjust based on processing time
	if avgProcessingTime > 500*time.Millisecond {
		rateMultiplier *= 1.5 // Slow down if processing is slow
	} else if avgProcessingTime < 100*time.Millisecond {
		rateMultiplier *= 0.8 // Speed up if processing is fast
	}

	// Adjust based on memory usage
	if avgMemoryUsage > 50*1024*1024 { // 50MB
		rateMultiplier *= 1.3 // Slow down if memory usage is high
	}

	// Adjust based on CPU usage
	if avgCPUUsage > 80.0 {
		rateMultiplier *= 1.4 // Slow down if CPU usage is high
	} else if avgCPUUsage < 20.0 {
		rateMultiplier *= 0.9 // Speed up if CPU usage is low
	}

	// Adjust based on queue depth
	if avgQueueDepth > ro.config.FallBehindThreshold {
		rateMultiplier *= 2.0 // Significantly slow down if falling behind
	}

	// Apply fall-behind penalty
	if ro.fallBehindCount > 0 {
		rateMultiplier *= float64(ro.fallBehindCount + 1)
	}

	return time.Duration(float64(ro.config.BaseInterval) * rateMultiplier)
}

// calculateDataUsageBasedRate calculates sampling rate based on data usage
func (ro *RateOptimizer) calculateDataUsageBasedRate(dataUsage float64) time.Duration {
	// Data usage is in MB per hour
	rateMultiplier := 1.0

	if dataUsage > 100 { // High data usage (>100MB/hour)
		rateMultiplier *= 2.0 // Slow down significantly
	} else if dataUsage > 50 { // Moderate data usage (50-100MB/hour)
		rateMultiplier *= 1.5 // Slow down moderately
	} else if dataUsage < 10 { // Low data usage (<10MB/hour)
		rateMultiplier *= 0.8 // Speed up slightly
	}

	return time.Duration(float64(ro.config.BaseInterval) * rateMultiplier)
}

// combineRates combines performance and data usage rates with weights
func (ro *RateOptimizer) combineRates(performanceRate, dataUsageRate time.Duration) time.Duration {
	performanceWeight := ro.config.PerformanceWeight
	dataUsageWeight := ro.config.DataUsageWeight

	// Normalize weights
	totalWeight := performanceWeight + dataUsageWeight
	performanceWeight /= totalWeight
	dataUsageWeight /= totalWeight

	// Calculate weighted average
	combinedRate := time.Duration(
		float64(performanceRate)*performanceWeight +
			float64(dataUsageRate)*dataUsageWeight,
	)

	return combinedRate
}

// clampRate ensures the rate is within configured bounds
func (ro *RateOptimizer) clampRate(rate time.Duration) time.Duration {
	if rate < ro.config.MinInterval {
		return ro.config.MinInterval
	}
	if rate > ro.config.MaxInterval {
		return ro.config.MaxInterval
	}
	return rate
}

// applyGradualOptimization applies gradual rate changes to avoid sudden spikes
func (ro *RateOptimizer) applyGradualOptimization(targetRate time.Duration) time.Duration {
	// Calculate maximum change per optimization cycle
	maxChange := ro.config.BaseInterval * 2 // Allow doubling/halving at most

	currentRate := ro.currentRate
	if currentRate == 0 {
		return targetRate
	}

	// Calculate the difference
	diff := targetRate - currentRate

	// Limit the change
	if diff > maxChange {
		targetRate = currentRate + maxChange
	} else if diff < -maxChange {
		targetRate = currentRate - maxChange
	}

	return targetRate
}

// maintainHistoryWindow maintains the performance history within the optimization window
func (ro *RateOptimizer) maintainHistoryWindow() {
	cutoff := time.Now().Add(-ro.config.OptimizationWindow)

	// Find the first metric within the window
	validIndex := 0
	for i, metric := range ro.performanceHistory {
		if metric.Timestamp.After(cutoff) {
			validIndex = i
			break
		}
	}

	// Remove old metrics
	if validIndex > 0 {
		ro.performanceHistory = ro.performanceHistory[validIndex:]
	}

	// Limit history size to prevent memory growth
	maxHistorySize := 1000
	if len(ro.performanceHistory) > maxHistorySize {
		ro.performanceHistory = ro.performanceHistory[len(ro.performanceHistory)-maxHistorySize:]
	}
}

// GetOptimizationStats returns optimization statistics
func (ro *RateOptimizer) GetOptimizationStats() map[string]interface{} {
	ro.mu.RLock()
	defer ro.mu.RUnlock()

	stats := map[string]interface{}{
		"enabled":            ro.config.Enabled,
		"current_rate":       ro.currentRate.String(),
		"optimization_level": ro.optimizationLevel,
		"fall_behind_count":  ro.fallBehindCount,
		"history_size":       len(ro.performanceHistory),
		"last_optimization":  ro.lastOptimization.Format(time.RFC3339),
	}

	// Add performance metrics if available
	if len(ro.performanceHistory) > 0 {
		latest := ro.performanceHistory[len(ro.performanceHistory)-1]
		stats["latest_processing_time"] = latest.ProcessingTime.Milliseconds()
		stats["latest_memory_usage"] = latest.MemoryUsage
		stats["latest_cpu_usage"] = latest.CPUUsage
		stats["latest_queue_depth"] = latest.QueueDepth
		stats["latest_data_usage"] = latest.DataUsage
	}

	return stats
}

// ResetOptimization resets the optimization state
func (ro *RateOptimizer) ResetOptimization() {
	ro.mu.Lock()
	defer ro.mu.Unlock()

	ro.performanceHistory = make([]PerformanceMetric, 0)
	ro.fallBehindCount = 0
	ro.currentRate = ro.config.BaseInterval
	ro.optimizationLevel = 2
	ro.lastOptimization = time.Now()

	ro.logger.Info("Rate optimization reset")
}

// UpdateConfig updates the rate optimizer configuration
func (ro *RateOptimizer) UpdateConfig(config *RateOptimizerConfig) {
	ro.mu.Lock()
	defer ro.mu.Unlock()

	ro.config = config
	ro.logger.Info("Rate optimizer configuration updated")
}
