package audit

import (
	"fmt"
	"math"
	"sort"
	"time"

	"github.com/markus-lassfolk/autonomy/pkg/logx"
)

// PatternType represents different types of patterns that can be detected
type PatternType string

const (
	PatternTypeCyclic        PatternType = "cyclic"
	PatternTypeDeteriorating PatternType = "deteriorating"
	PatternTypeImproving     PatternType = "improving"
	PatternTypeSpike         PatternType = "spike"
	PatternTypeTrend         PatternType = "trend"
	PatternTypeAnomaly       PatternType = "anomaly"
)

// Pattern represents a detected pattern in decision data
type Pattern struct {
	Type            PatternType            `json:"type"`
	Confidence      float64                `json:"confidence"` // 0.0-1.0 confidence in pattern
	StartTime       time.Time              `json:"start_time"`
	EndTime         time.Time              `json:"end_time"`
	Duration        time.Duration          `json:"duration"`
	Description     string                 `json:"description"`
	Severity        string                 `json:"severity"` // low, medium, high, critical
	Metrics         map[string]interface{} `json:"metrics"`
	Recommendations []string               `json:"recommendations"`
}

// PatternAnalyzer analyzes decision patterns and trends
type PatternAnalyzer struct {
	logger *logx.Logger
}

// NewPatternAnalyzer creates a new pattern analyzer
func NewPatternAnalyzer(logger *logx.Logger) *PatternAnalyzer {
	return &PatternAnalyzer{
		logger: logger,
	}
}

// AnalyzePatterns analyzes decision records for patterns
func (pa *PatternAnalyzer) AnalyzePatterns(records []*DecisionRecord, window time.Duration) []*Pattern {
	var patterns []*Pattern

	if len(records) < 3 {
		return patterns // Need at least 3 records for pattern analysis
	}

	// Sort records by timestamp
	sort.Slice(records, func(i, j int) bool {
		return records[i].Timestamp.Before(records[j].Timestamp)
	})

	// Analyze different pattern types
	patterns = append(patterns, pa.detectCyclicPatterns(records, window)...)
	patterns = append(patterns, pa.detectTrendPatterns(records, window)...)
	patterns = append(patterns, pa.detectAnomalyPatterns(records)...)
	patterns = append(patterns, pa.detectSpikePatterns(records)...)

	// Sort patterns by confidence (highest first)
	sort.Slice(patterns, func(i, j int) bool {
		return patterns[i].Confidence > patterns[j].Confidence
	})

	return patterns
}

// detectCyclicPatterns detects cyclic patterns in decision data
func (pa *PatternAnalyzer) detectCyclicPatterns(records []*DecisionRecord, window time.Duration) []*Pattern {
	var patterns []*Pattern

	// Group decisions by type
	decisionTypes := make(map[string][]*DecisionRecord)
	for _, record := range records {
		decisionTypes[record.DecisionType] = append(decisionTypes[record.DecisionType], record)
	}

	for decisionType, typeRecords := range decisionTypes {
		if len(typeRecords) < 4 {
			continue // Need at least 4 records for cyclic detection
		}

		// Calculate intervals between decisions
		intervals := make([]time.Duration, len(typeRecords)-1)
		for i := 0; i < len(typeRecords)-1; i++ {
			intervals[i] = typeRecords[i+1].Timestamp.Sub(typeRecords[i].Timestamp)
		}

		// Check for regularity in intervals
		if pa.isCyclic(intervals) {
			avgInterval := pa.calculateAverageInterval(intervals)
			confidence := pa.calculateCyclicConfidence(intervals)

			pattern := &Pattern{
				Type:       PatternTypeCyclic,
				Confidence: confidence,
				StartTime:  typeRecords[0].Timestamp,
				EndTime:    typeRecords[len(typeRecords)-1].Timestamp,
				Duration:   typeRecords[len(typeRecords)-1].Timestamp.Sub(typeRecords[0].Timestamp),
				Description: fmt.Sprintf("Cyclic %s decisions every %v (confidence: %.2f)",
					decisionType, avgInterval, confidence),
				Severity: pa.calculateSeverity(confidence),
				Metrics: map[string]interface{}{
					"decision_type":  decisionType,
					"avg_interval":   avgInterval.String(),
					"interval_count": len(intervals),
				},
				Recommendations: []string{
					"Consider adjusting decision thresholds to reduce frequency",
					"Monitor for underlying issues causing regular failures",
				},
			}
			patterns = append(patterns, pattern)
		}
	}

	return patterns
}

// detectTrendPatterns detects improving or deteriorating trends
func (pa *PatternAnalyzer) detectTrendPatterns(records []*DecisionRecord, window time.Duration) []*Pattern {
	var patterns []*Pattern

	// Analyze confidence trends
	confidenceTrend := pa.analyzeConfidenceTrend(records)
	if confidenceTrend != nil {
		patterns = append(patterns, confidenceTrend)
	}

	// Analyze success rate trends
	successTrend := pa.analyzeSuccessTrend(records)
	if successTrend != nil {
		patterns = append(patterns, successTrend)
	}

	// Analyze execution time trends
	executionTrend := pa.analyzeExecutionTimeTrend(records)
	if executionTrend != nil {
		patterns = append(patterns, executionTrend)
	}

	return patterns
}

// detectAnomalyPatterns detects anomalous patterns
func (pa *PatternAnalyzer) detectAnomalyPatterns(records []*DecisionRecord) []*Pattern {
	var patterns []*Pattern

	if len(records) < 5 {
		return patterns // Need sufficient data for anomaly detection
	}

	// Calculate baseline statistics
	confidences := make([]float64, len(records))
	executionTimes := make([]float64, len(records))

	for i, record := range records {
		confidences[i] = record.Confidence
		executionTimes[i] = float64(record.ExecutionTime.Milliseconds())
	}

	confMean, confStd := pa.calculateMeanStd(confidences)
	execMean, execStd := pa.calculateMeanStd(executionTimes)

	// Detect anomalies (values > 2 standard deviations from mean)
	for _, record := range records {
		confZScore := math.Abs((record.Confidence - confMean) / confStd)
		execZScore := math.Abs((float64(record.ExecutionTime.Milliseconds()) - execMean) / execStd)

		if confZScore > 2.0 || execZScore > 2.0 {
			pattern := &Pattern{
				Type:       PatternTypeAnomaly,
				Confidence: math.Min(confZScore/4.0, 1.0), // Normalize to 0-1
				StartTime:  record.Timestamp,
				EndTime:    record.Timestamp,
				Duration:   0,
				Description: fmt.Sprintf("Anomalous decision: confidence=%.2f (z-score=%.2f), execution_time=%v (z-score=%.2f)",
					record.Confidence, confZScore, record.ExecutionTime, execZScore),
				Severity: "high",
				Metrics: map[string]interface{}{
					"decision_id":       record.DecisionID,
					"confidence_zscore": confZScore,
					"execution_zscore":  execZScore,
				},
				Recommendations: []string{
					"Investigate root cause of anomalous decision",
					"Check for system issues or configuration problems",
				},
			}
			patterns = append(patterns, pattern)
		}
	}

	return patterns
}

// detectSpikePatterns detects sudden spikes in decision frequency
func (pa *PatternAnalyzer) detectSpikePatterns(records []*DecisionRecord) []*Pattern {
	var patterns []*Pattern

	if len(records) < 10 {
		return patterns // Need sufficient data for spike detection
	}

	// Group records by time windows (e.g., 1-hour windows)
	windowSize := time.Hour
	windows := make(map[time.Time]int)

	for _, record := range records {
		window := record.Timestamp.Truncate(windowSize)
		windows[window]++
	}

	// Calculate average decisions per window
	var totalDecisions int
	for _, count := range windows {
		totalDecisions += count
	}
	avgPerWindow := float64(totalDecisions) / float64(len(windows))

	// Detect spikes (>2x average)
	for window, count := range windows {
		if float64(count) > avgPerWindow*2.0 {
			pattern := &Pattern{
				Type:       PatternTypeSpike,
				Confidence: math.Min(float64(count)/avgPerWindow/4.0, 1.0),
				StartTime:  window,
				EndTime:    window.Add(windowSize),
				Duration:   windowSize,
				Description: fmt.Sprintf("Decision spike: %d decisions in %v window (avg: %.1f)",
					count, windowSize, avgPerWindow),
				Severity: "medium",
				Metrics: map[string]interface{}{
					"decisions_in_window": count,
					"average_per_window":  avgPerWindow,
					"spike_ratio":         float64(count) / avgPerWindow,
				},
				Recommendations: []string{
					"Investigate what caused the decision spike",
					"Check for network issues or configuration changes",
				},
			}
			patterns = append(patterns, pattern)
		}
	}

	return patterns
}

// analyzeConfidenceTrend analyzes trends in decision confidence
func (pa *PatternAnalyzer) analyzeConfidenceTrend(records []*DecisionRecord) *Pattern {
	if len(records) < 5 {
		return nil
	}

	// Calculate linear regression for confidence over time
	slope, rSquared := pa.calculateLinearRegression(records, func(r *DecisionRecord) float64 {
		return r.Confidence
	})

	if math.Abs(slope) < 0.01 || rSquared < 0.3 {
		return nil // No significant trend
	}

	var patternType PatternType
	var description string
	var recommendations []string

	if slope > 0 {
		patternType = PatternTypeImproving
		description = fmt.Sprintf("Improving confidence trend: slope=%.3f, R²=%.3f", slope, rSquared)
		recommendations = []string{
			"System performance is improving",
			"Consider optimizing decision thresholds",
		}
	} else {
		patternType = PatternTypeDeteriorating
		description = fmt.Sprintf("Deteriorating confidence trend: slope=%.3f, R²=%.3f", slope, rSquared)
		recommendations = []string{
			"Investigate declining system performance",
			"Check for hardware or configuration issues",
		}
	}

	return &Pattern{
		Type:        patternType,
		Confidence:  rSquared,
		StartTime:   records[0].Timestamp,
		EndTime:     records[len(records)-1].Timestamp,
		Duration:    records[len(records)-1].Timestamp.Sub(records[0].Timestamp),
		Description: description,
		Severity:    pa.calculateSeverity(rSquared),
		Metrics: map[string]interface{}{
			"slope":       slope,
			"r_squared":   rSquared,
			"data_points": len(records),
		},
		Recommendations: recommendations,
	}
}

// analyzeSuccessTrend analyzes trends in decision success rates
func (pa *PatternAnalyzer) analyzeSuccessTrend(records []*DecisionRecord) *Pattern {
	if len(records) < 5 {
		return nil
	}

	// Group records into time windows and calculate success rates
	windowSize := time.Hour
	windows := make(map[time.Time][]bool)

	for _, record := range records {
		window := record.Timestamp.Truncate(windowSize)
		windows[window] = append(windows[window], record.Success)
	}

	// Calculate success rate trend
	var windowTimes []time.Time
	var successRates []float64

	for window, successes := range windows {
		windowTimes = append(windowTimes, window)
		successCount := 0
		for _, success := range successes {
			if success {
				successCount++
			}
		}
		successRate := float64(successCount) / float64(len(successes))
		successRates = append(successRates, successRate)
	}

	if len(successRates) < 3 {
		return nil
	}

	// Calculate linear regression
	slope, rSquared := pa.calculateLinearRegressionForWindows(windowTimes, successRates)

	if math.Abs(slope) < 0.01 || rSquared < 0.3 {
		return nil // No significant trend
	}

	var patternType PatternType
	var description string
	var recommendations []string

	if slope > 0 {
		patternType = PatternTypeImproving
		description = fmt.Sprintf("Improving success rate trend: slope=%.3f, R²=%.3f", slope, rSquared)
		recommendations = []string{
			"Decision success rate is improving",
			"System reliability is increasing",
		}
	} else {
		patternType = PatternTypeDeteriorating
		description = fmt.Sprintf("Deteriorating success rate trend: slope=%.3f, R²=%.3f", slope, rSquared)
		recommendations = []string{
			"Decision success rate is declining",
			"Investigate system reliability issues",
		}
	}

	return &Pattern{
		Type:        patternType,
		Confidence:  rSquared,
		StartTime:   windowTimes[0],
		EndTime:     windowTimes[len(windowTimes)-1].Add(windowSize),
		Duration:    windowTimes[len(windowTimes)-1].Sub(windowTimes[0]) + windowSize,
		Description: description,
		Severity:    pa.calculateSeverity(rSquared),
		Metrics: map[string]interface{}{
			"slope":            slope,
			"r_squared":        rSquared,
			"windows_analyzed": len(windowTimes),
		},
		Recommendations: recommendations,
	}
}

// analyzeExecutionTimeTrend analyzes trends in decision execution times
func (pa *PatternAnalyzer) analyzeExecutionTimeTrend(records []*DecisionRecord) *Pattern {
	if len(records) < 5 {
		return nil
	}

	// Calculate linear regression for execution time over time
	slope, rSquared := pa.calculateLinearRegression(records, func(r *DecisionRecord) float64 {
		return float64(r.ExecutionTime.Milliseconds())
	})

	if math.Abs(slope) < 1.0 || rSquared < 0.3 {
		return nil // No significant trend
	}

	var patternType PatternType
	var description string
	var recommendations []string

	if slope < 0 {
		patternType = PatternTypeImproving
		description = fmt.Sprintf("Improving execution time trend: slope=%.1f ms/sample, R²=%.3f", slope, rSquared)
		recommendations = []string{
			"Decision execution is getting faster",
			"System performance is improving",
		}
	} else {
		patternType = PatternTypeDeteriorating
		description = fmt.Sprintf("Deteriorating execution time trend: slope=%.1f ms/sample, R²=%.3f", slope, rSquared)
		recommendations = []string{
			"Decision execution is getting slower",
			"Investigate system performance degradation",
		}
	}

	return &Pattern{
		Type:        patternType,
		Confidence:  rSquared,
		StartTime:   records[0].Timestamp,
		EndTime:     records[len(records)-1].Timestamp,
		Duration:    records[len(records)-1].Timestamp.Sub(records[0].Timestamp),
		Description: description,
		Severity:    pa.calculateSeverity(rSquared),
		Metrics: map[string]interface{}{
			"slope_ms_per_sample": slope,
			"r_squared":           rSquared,
			"data_points":         len(records),
		},
		Recommendations: recommendations,
	}
}

// Helper methods

func (pa *PatternAnalyzer) isCyclic(intervals []time.Duration) bool {
	if len(intervals) < 3 {
		return false
	}

	// Calculate coefficient of variation (CV = std/mean)
	mean := pa.calculateAverageInterval(intervals)
	var sumSquaredDiff float64

	for _, interval := range intervals {
		diff := float64(interval - mean)
		sumSquaredDiff += diff * diff
	}

	std := time.Duration(math.Sqrt(sumSquaredDiff / float64(len(intervals))))
	cv := float64(std) / float64(mean)

	// If CV < 0.3, consider it cyclic (regular intervals)
	return cv < 0.3
}

func (pa *PatternAnalyzer) calculateAverageInterval(intervals []time.Duration) time.Duration {
	var total time.Duration
	for _, interval := range intervals {
		total += interval
	}
	return total / time.Duration(len(intervals))
}

func (pa *PatternAnalyzer) calculateCyclicConfidence(intervals []time.Duration) float64 {
	mean := pa.calculateAverageInterval(intervals)
	var sumSquaredDiff float64

	for _, interval := range intervals {
		diff := float64(interval - mean)
		sumSquaredDiff += diff * diff
	}

	std := time.Duration(math.Sqrt(sumSquaredDiff / float64(len(intervals))))
	cv := float64(std) / float64(mean)

	// Convert CV to confidence (lower CV = higher confidence)
	confidence := 1.0 - math.Min(cv, 1.0)
	return confidence
}

func (pa *PatternAnalyzer) calculateLinearRegression(records []*DecisionRecord, valueFunc func(*DecisionRecord) float64) (slope, rSquared float64) {
	n := len(records)
	if n < 2 {
		return 0, 0
	}

	// Convert timestamps to float64 (seconds since first record)
	startTime := records[0].Timestamp
	var x, y []float64

	for _, record := range records {
		x = append(x, float64(record.Timestamp.Sub(startTime).Seconds()))
		y = append(y, valueFunc(record))
	}

	return pa.calculateLinearRegressionForPoints(x, y)
}

func (pa *PatternAnalyzer) calculateLinearRegressionForWindows(times []time.Time, values []float64) (slope, rSquared float64) {
	if len(times) != len(values) || len(times) < 2 {
		return 0, 0
	}

	// Convert times to float64 (seconds since first time)
	startTime := times[0]
	var x []float64

	for _, t := range times {
		x = append(x, float64(t.Sub(startTime).Seconds()))
	}

	return pa.calculateLinearRegressionForPoints(x, values)
}

func (pa *PatternAnalyzer) calculateLinearRegressionForPoints(x, y []float64) (slope, rSquared float64) {
	n := len(x)
	if n != len(y) || n < 2 {
		return 0, 0
	}

	// Calculate means
	var sumX, sumY, sumXY, sumX2, sumY2 float64
	for i := 0; i < n; i++ {
		sumX += x[i]
		sumY += y[i]
		sumXY += x[i] * y[i]
		sumX2 += x[i] * x[i]
		sumY2 += y[i] * y[i]
	}

	meanX := sumX / float64(n)
	meanY := sumY / float64(n)

	// Calculate slope
	numerator := sumXY - float64(n)*meanX*meanY
	denominator := sumX2 - float64(n)*meanX*meanX

	if denominator == 0 {
		return 0, 0
	}

	slope = numerator / denominator

	// Calculate R-squared
	var ssRes, ssTot float64
	for i := 0; i < n; i++ {
		predicted := slope*x[i] + (meanY - slope*meanX)
		ssRes += (y[i] - predicted) * (y[i] - predicted)
		ssTot += (y[i] - meanY) * (y[i] - meanY)
	}

	if ssTot == 0 {
		rSquared = 0
	} else {
		rSquared = 1 - (ssRes / ssTot)
	}

	return slope, rSquared
}

func (pa *PatternAnalyzer) calculateMeanStd(values []float64) (mean, std float64) {
	if len(values) == 0 {
		return 0, 0
	}

	// Calculate mean
	var sum float64
	for _, v := range values {
		sum += v
	}
	mean = sum / float64(len(values))

	// Calculate standard deviation
	var sumSquaredDiff float64
	for _, v := range values {
		diff := v - mean
		sumSquaredDiff += diff * diff
	}
	std = math.Sqrt(sumSquaredDiff / float64(len(values)))

	return mean, std
}

func (pa *PatternAnalyzer) calculateSeverity(confidence float64) string {
	switch {
	case confidence >= 0.8:
		return "critical"
	case confidence >= 0.6:
		return "high"
	case confidence >= 0.4:
		return "medium"
	default:
		return "low"
	}
}
