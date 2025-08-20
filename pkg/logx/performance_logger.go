package logx

import (
	"context"
	"fmt"
	"sync"
	"time"
)

// PerformanceLogger provides structured logging for performance metrics
type PerformanceLogger struct {
	logger       *Logger
	metrics      map[string]*PerformanceMetric
	metricsMutex sync.RWMutex
}

// PerformanceMetric tracks performance data for a specific operation
type PerformanceMetric struct {
	Name          string        `json:"name"`
	Count         int64         `json:"count"`
	TotalDuration time.Duration `json:"total_duration"`
	MinDuration   time.Duration `json:"min_duration"`
	MaxDuration   time.Duration `json:"max_duration"`
	AvgDuration   time.Duration `json:"avg_duration"`
	LastExecuted  time.Time     `json:"last_executed"`
	ErrorCount    int64         `json:"error_count"`
	SuccessRate   float64       `json:"success_rate"`
	ConcurrentOps int64         `json:"concurrent_ops"`
	MaxConcurrent int64         `json:"max_concurrent"`
}

// PerformanceContext provides context for performance tracking
type PerformanceContext struct {
	metricName string
	startTime  time.Time
	logger     *PerformanceLogger
	ctx        context.Context
}

// NewPerformanceLogger creates a new performance logger
func NewPerformanceLogger(logger *Logger) *PerformanceLogger {
	return &PerformanceLogger{
		logger:  logger,
		metrics: make(map[string]*PerformanceMetric),
	}
}

// StartOperation starts tracking a performance operation
func (pl *PerformanceLogger) StartOperation(ctx context.Context, metricName string) *PerformanceContext {
	pl.metricsMutex.Lock()
	defer pl.metricsMutex.Unlock()

	// Get or create metric
	metric, exists := pl.metrics[metricName]
	if !exists {
		metric = &PerformanceMetric{
			Name:         metricName,
			MinDuration:  time.Hour, // Start with a high value
			LastExecuted: time.Now(),
		}
		pl.metrics[metricName] = metric
	}

	// Update concurrent operations
	metric.ConcurrentOps++
	if metric.ConcurrentOps > metric.MaxConcurrent {
		metric.MaxConcurrent = metric.ConcurrentOps
	}

	return &PerformanceContext{
		metricName: metricName,
		startTime:  time.Now(),
		logger:     pl,
		ctx:        ctx,
	}
}

// Complete marks an operation as completed and logs performance data
func (pc *PerformanceContext) Complete(err error) {
	duration := time.Since(pc.startTime)

	pc.logger.metricsMutex.Lock()
	defer pc.logger.metricsMutex.Unlock()

	metric := pc.logger.metrics[pc.metricName]
	metric.Count++
	metric.TotalDuration += duration
	metric.LastExecuted = time.Now()
	metric.ConcurrentOps--

	// Update min/max durations
	if duration < metric.MinDuration {
		metric.MinDuration = duration
	}
	if duration > metric.MaxDuration {
		metric.MaxDuration = duration
	}

	// Calculate average
	metric.AvgDuration = metric.TotalDuration / time.Duration(metric.Count)

	// Track errors
	if err != nil {
		metric.ErrorCount++
		metric.SuccessRate = float64(metric.Count-metric.ErrorCount) / float64(metric.Count) * 100

		pc.logger.logger.Error("Performance operation failed",
			"metric", pc.metricName,
			"duration", duration.String(),
			"error", err.Error(),
			"success_rate", fmt.Sprintf("%.2f%%", metric.SuccessRate),
		)
	} else {
		metric.SuccessRate = float64(metric.Count-metric.ErrorCount) / float64(metric.Count) * 100

		// Log performance data for slow operations or periodically
		if duration > 100*time.Millisecond || metric.Count%100 == 0 {
			pc.logger.logger.Info("Performance operation completed",
				"metric", pc.metricName,
				"duration", duration.String(),
				"avg_duration", metric.AvgDuration.String(),
				"success_rate", fmt.Sprintf("%.2f%%", metric.SuccessRate),
				"total_operations", metric.Count,
			)
		}
	}
}

// LogMetrics logs all current performance metrics
func (pl *PerformanceLogger) LogMetrics() {
	pl.metricsMutex.RLock()
	defer pl.metricsMutex.RUnlock()

	for name, metric := range pl.metrics {
		pl.logger.Info("Performance metric summary",
			"metric", name,
			"total_operations", metric.Count,
			"avg_duration", metric.AvgDuration.String(),
			"min_duration", metric.MinDuration.String(),
			"max_duration", metric.MaxDuration.String(),
			"success_rate", fmt.Sprintf("%.2f%%", metric.SuccessRate),
			"error_count", metric.ErrorCount,
			"max_concurrent", metric.MaxConcurrent,
			"last_executed", metric.LastExecuted.Format(time.RFC3339),
		)
	}
}

// GetMetric returns a specific performance metric
func (pl *PerformanceLogger) GetMetric(name string) *PerformanceMetric {
	pl.metricsMutex.RLock()
	defer pl.metricsMutex.RUnlock()

	if metric, exists := pl.metrics[name]; exists {
		// Return a copy to avoid race conditions
		return &PerformanceMetric{
			Name:          metric.Name,
			Count:         metric.Count,
			TotalDuration: metric.TotalDuration,
			MinDuration:   metric.MinDuration,
			MaxDuration:   metric.MaxDuration,
			AvgDuration:   metric.AvgDuration,
			LastExecuted:  metric.LastExecuted,
			ErrorCount:    metric.ErrorCount,
			SuccessRate:   metric.SuccessRate,
			ConcurrentOps: metric.ConcurrentOps,
			MaxConcurrent: metric.MaxConcurrent,
		}
	}
	return nil
}

// GetAllMetrics returns all performance metrics
func (pl *PerformanceLogger) GetAllMetrics() map[string]*PerformanceMetric {
	pl.metricsMutex.RLock()
	defer pl.metricsMutex.RUnlock()

	result := make(map[string]*PerformanceMetric)
	for name, metric := range pl.metrics {
		result[name] = &PerformanceMetric{
			Name:          metric.Name,
			Count:         metric.Count,
			TotalDuration: metric.TotalDuration,
			MinDuration:   metric.MinDuration,
			MaxDuration:   metric.MaxDuration,
			AvgDuration:   metric.AvgDuration,
			LastExecuted:  metric.LastExecuted,
			ErrorCount:    metric.ErrorCount,
			SuccessRate:   metric.SuccessRate,
			ConcurrentOps: metric.ConcurrentOps,
			MaxConcurrent: metric.MaxConcurrent,
		}
	}
	return result
}

// ResetMetrics resets all performance metrics
func (pl *PerformanceLogger) ResetMetrics() {
	pl.metricsMutex.Lock()
	defer pl.metricsMutex.Unlock()

	pl.metrics = make(map[string]*PerformanceMetric)
	pl.logger.Info("Performance metrics reset")
}

// LogSlowOperations logs operations that exceed a threshold
func (pl *PerformanceLogger) LogSlowOperations(threshold time.Duration) {
	pl.metricsMutex.RLock()
	defer pl.metricsMutex.RUnlock()

	for name, metric := range pl.metrics {
		if metric.AvgDuration > threshold {
			pl.logger.Warn("Slow operation detected",
				"metric", name,
				"avg_duration", metric.AvgDuration.String(),
				"threshold", threshold.String(),
				"total_operations", metric.Count,
				"max_duration", metric.MaxDuration.String(),
			)
		}
	}
}

// LogHighErrorRates logs operations with high error rates
func (pl *PerformanceLogger) LogHighErrorRates(threshold float64) {
	pl.metricsMutex.RLock()
	defer pl.metricsMutex.RUnlock()

	for name, metric := range pl.metrics {
		if metric.SuccessRate < threshold && metric.Count > 10 {
			pl.logger.Error("High error rate detected",
				"metric", name,
				"success_rate", fmt.Sprintf("%.2f%%", metric.SuccessRate),
				"threshold", fmt.Sprintf("%.2f%%", threshold),
				"error_count", metric.ErrorCount,
				"total_operations", metric.Count,
			)
		}
	}
}

// LogResourceUsage logs resource usage metrics
func (pl *PerformanceLogger) LogResourceUsage(memoryMB float64, cpuPercent float64, goroutines int) {
	pl.logger.Info("Resource usage",
		"memory_mb", memoryMB,
		"cpu_percent", cpuPercent,
		"goroutines", goroutines,
	)
}

// LogDatabasePerformance logs database performance metrics
func (pl *PerformanceLogger) LogDatabasePerformance(operation string, duration time.Duration, rowsAffected int, err error) {
	fields := map[string]interface{}{
		"operation":     operation,
		"duration":      duration.String(),
		"rows_affected": rowsAffected,
	}

	if err != nil {
		fields["error"] = err.Error()
		pl.logger.Error("Database operation failed", fields)
	} else {
		pl.logger.Debug("Database operation completed", fields)
	}
}

// LogNetworkPerformance logs network performance metrics
func (pl *PerformanceLogger) LogNetworkPerformance(operation string, duration time.Duration, bytesSent int, bytesReceived int, err error) {
	fields := map[string]interface{}{
		"operation":      operation,
		"duration":       duration.String(),
		"bytes_sent":     bytesSent,
		"bytes_received": bytesReceived,
	}

	if err != nil {
		fields["error"] = err.Error()
		pl.logger.Error("Network operation failed", fields)
	} else {
		pl.logger.Debug("Network operation completed", fields)
	}
}

// LogAPIPerformance logs API performance metrics
func (pl *PerformanceLogger) LogAPIPerformance(endpoint string, method string, duration time.Duration, statusCode int, err error) {
	fields := map[string]interface{}{
		"endpoint":    endpoint,
		"method":      method,
		"duration":    duration.String(),
		"status_code": statusCode,
	}

	if err != nil {
		fields["error"] = err.Error()
		pl.logger.Error("API call failed", fields)
	} else if statusCode >= 400 {
		pl.logger.Warn("API call returned error status", fields)
	} else {
		pl.logger.Debug("API call completed", fields)
	}
}

// LogCachePerformance logs cache performance metrics
func (pl *PerformanceLogger) LogCachePerformance(operation string, key string, hit bool, duration time.Duration, err error) {
	fields := map[string]interface{}{
		"operation": operation,
		"key":       key,
		"hit":       hit,
		"duration":  duration.String(),
	}

	if err != nil {
		fields["error"] = err.Error()
		pl.logger.Error("Cache operation failed", fields)
	} else {
		pl.logger.Debug("Cache operation completed", fields)
	}
}
