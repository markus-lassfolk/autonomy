package obstruction

import (
	"context"
	"fmt"
	"math"
	"sync"
	"time"

	"github.com/markus-lassfolk/autonomy/pkg/logx"
)

// TrendPoint represents a single point in trend analysis
type TrendPoint struct {
	Timestamp time.Time
	Value     float64
	Quality   float64 // Data quality score (0-1)
}

// TrendAnalysis represents comprehensive trend analysis results
type TrendAnalysis struct {
	Metric          string        `json:"metric"`
	WindowStart     time.Time     `json:"window_start"`
	WindowEnd       time.Time     `json:"window_end"`
	SampleCount     int           `json:"sample_count"`
	Slope           float64       `json:"slope"`            // Rate of change per second
	Intercept       float64       `json:"intercept"`        // Y-intercept of trend line
	RSquared        float64       `json:"r_squared"`        // Correlation coefficient squared
	Variance        float64       `json:"variance"`         // Data variance
	StandardError   float64       `json:"standard_error"`   // Standard error of slope
	ConfidenceLevel float64       `json:"confidence_level"` // Statistical confidence (0-1)
	TrendStrength   string        `json:"trend_strength"`   // "weak", "moderate", "strong"
	TrendDirection  string        `json:"trend_direction"`  // "increasing", "decreasing", "stable"
	Prediction      *Prediction   `json:"prediction"`       // Future value prediction
	Anomalies       []Anomaly     `json:"anomalies"`        // Detected anomalies
	SeasonalPattern *SeasonalInfo `json:"seasonal_pattern"` // Seasonal pattern if detected
}

// Prediction represents a future value prediction
type Prediction struct {
	TargetTime         time.Time `json:"target_time"`
	PredictedValue     float64   `json:"predicted_value"`
	ConfidenceInterval struct {
		Lower float64 `json:"lower"`
		Upper float64 `json:"upper"`
	} `json:"confidence_interval"`
	Confidence float64 `json:"confidence"`
}

// Anomaly represents a detected anomaly in the data
type Anomaly struct {
	Timestamp     time.Time `json:"timestamp"`
	Value         float64   `json:"value"`
	ExpectedValue float64   `json:"expected_value"`
	Deviation     float64   `json:"deviation"`
	Severity      string    `json:"severity"` // "minor", "moderate", "severe"
}

// SeasonalInfo represents detected seasonal patterns
type SeasonalInfo struct {
	Period      time.Duration `json:"period"`
	Amplitude   float64       `json:"amplitude"`
	Phase       float64       `json:"phase"`
	Confidence  float64       `json:"confidence"`
	Description string        `json:"description"`
}

// TrendAnalyzer provides advanced trend analysis capabilities
type TrendAnalyzer struct {
	mu     sync.RWMutex
	logger *logx.Logger
	config *TrendAnalyzerConfig

	// Historical data storage
	obstructionHistory []TrendPoint
	snrHistory         []TrendPoint
	latencyHistory     []TrendPoint

	// Analysis cache
	lastAnalysis map[string]*TrendAnalysis
	lastUpdate   time.Time
}

// TrendAnalyzerConfig holds configuration for trend analysis
type TrendAnalyzerConfig struct {
	MaxHistoryPoints     int           `json:"max_history_points"`
	MinPointsForAnalysis int           `json:"min_points_for_analysis"`
	AnalysisWindow       time.Duration `json:"analysis_window"`
	PredictionHorizon    time.Duration `json:"prediction_horizon"`
	AnomalyThreshold     float64       `json:"anomaly_threshold"`   // Standard deviations for anomaly detection
	SeasonalMinPeriod    time.Duration `json:"seasonal_min_period"` // Minimum period for seasonal detection
	SeasonalMaxPeriod    time.Duration `json:"seasonal_max_period"` // Maximum period for seasonal detection
	CacheTimeout         time.Duration `json:"cache_timeout"`       // How long to cache analysis results
}

// DefaultTrendAnalyzerConfig returns default configuration
func DefaultTrendAnalyzerConfig() *TrendAnalyzerConfig {
	return &TrendAnalyzerConfig{
		MaxHistoryPoints:     1440, // 24 hours at 1-minute intervals
		MinPointsForAnalysis: 10,   // Minimum points needed for analysis
		AnalysisWindow:       30 * time.Minute,
		PredictionHorizon:    5 * time.Minute,
		AnomalyThreshold:     2.0, // 2 standard deviations
		SeasonalMinPeriod:    10 * time.Minute,
		SeasonalMaxPeriod:    24 * time.Hour,
		CacheTimeout:         30 * time.Second,
	}
}

// NewTrendAnalyzer creates a new trend analyzer
func NewTrendAnalyzer(logger *logx.Logger, config *TrendAnalyzerConfig) *TrendAnalyzer {
	if config == nil {
		config = DefaultTrendAnalyzerConfig()
	}

	return &TrendAnalyzer{
		logger:             logger,
		config:             config,
		obstructionHistory: make([]TrendPoint, 0, config.MaxHistoryPoints),
		snrHistory:         make([]TrendPoint, 0, config.MaxHistoryPoints),
		latencyHistory:     make([]TrendPoint, 0, config.MaxHistoryPoints),
		lastAnalysis:       make(map[string]*TrendAnalysis),
	}
}

// AddObstructionPoint adds a new obstruction data point
func (ta *TrendAnalyzer) AddObstructionPoint(timestamp time.Time, value, quality float64) {
	ta.mu.Lock()
	defer ta.mu.Unlock()

	point := TrendPoint{
		Timestamp: timestamp,
		Value:     value,
		Quality:   quality,
	}

	ta.obstructionHistory = append(ta.obstructionHistory, point)
	if len(ta.obstructionHistory) > ta.config.MaxHistoryPoints {
		ta.obstructionHistory = ta.obstructionHistory[1:]
	}

	ta.logger.Debug("Added obstruction trend point",
		"timestamp", timestamp,
		"value", value,
		"quality", quality,
		"history_size", len(ta.obstructionHistory))
}

// AddSNRPoint adds a new SNR data point
func (ta *TrendAnalyzer) AddSNRPoint(timestamp time.Time, value, quality float64) {
	ta.mu.Lock()
	defer ta.mu.Unlock()

	point := TrendPoint{
		Timestamp: timestamp,
		Value:     value,
		Quality:   quality,
	}

	ta.snrHistory = append(ta.snrHistory, point)
	if len(ta.snrHistory) > ta.config.MaxHistoryPoints {
		ta.snrHistory = ta.snrHistory[1:]
	}

	ta.logger.Debug("Added SNR trend point",
		"timestamp", timestamp,
		"value", value,
		"quality", quality,
		"history_size", len(ta.snrHistory))
}

// AddLatencyPoint adds a new latency data point
func (ta *TrendAnalyzer) AddLatencyPoint(timestamp time.Time, value, quality float64) {
	ta.mu.Lock()
	defer ta.mu.Unlock()

	point := TrendPoint{
		Timestamp: timestamp,
		Value:     value,
		Quality:   quality,
	}

	ta.latencyHistory = append(ta.latencyHistory, point)
	if len(ta.latencyHistory) > ta.config.MaxHistoryPoints {
		ta.latencyHistory = ta.latencyHistory[1:]
	}

	ta.logger.Debug("Added latency trend point",
		"timestamp", timestamp,
		"value", value,
		"quality", quality,
		"history_size", len(ta.latencyHistory))
}

// AnalyzeObstructionTrend analyzes obstruction trends
func (ta *TrendAnalyzer) AnalyzeObstructionTrend(ctx context.Context) (*TrendAnalysis, error) {
	return ta.analyzeTrend(ctx, "obstruction", ta.obstructionHistory)
}

// AnalyzeSNRTrend analyzes SNR trends
func (ta *TrendAnalyzer) AnalyzeSNRTrend(ctx context.Context) (*TrendAnalysis, error) {
	return ta.analyzeTrend(ctx, "snr", ta.snrHistory)
}

// AnalyzeLatencyTrend analyzes latency trends
func (ta *TrendAnalyzer) AnalyzeLatencyTrend(ctx context.Context) (*TrendAnalysis, error) {
	return ta.analyzeTrend(ctx, "latency", ta.latencyHistory)
}

// analyzeTrend performs comprehensive trend analysis on a data series
func (ta *TrendAnalyzer) analyzeTrend(ctx context.Context, metric string, history []TrendPoint) (*TrendAnalysis, error) {
	ta.mu.RLock()
	defer ta.mu.RUnlock()

	// Check cache first
	if cached, exists := ta.lastAnalysis[metric]; exists {
		if time.Since(ta.lastUpdate) < ta.config.CacheTimeout {
			return cached, nil
		}
	}

	// Filter data to analysis window
	now := time.Now()
	windowStart := now.Add(-ta.config.AnalysisWindow)
	var windowData []TrendPoint

	for _, point := range history {
		if point.Timestamp.After(windowStart) {
			windowData = append(windowData, point)
		}
	}

	if len(windowData) < ta.config.MinPointsForAnalysis {
		return nil, fmt.Errorf("insufficient data points for %s analysis: have %d, need %d",
			metric, len(windowData), ta.config.MinPointsForAnalysis)
	}

	analysis := &TrendAnalysis{
		Metric:      metric,
		WindowStart: windowStart,
		WindowEnd:   now,
		SampleCount: len(windowData),
	}

	// Perform linear regression
	ta.performLinearRegression(windowData, analysis)

	// Calculate statistical measures
	ta.calculateStatistics(windowData, analysis)

	// Determine trend strength and direction
	ta.classifyTrend(analysis)

	// Generate prediction
	analysis.Prediction = ta.generatePrediction(windowData, analysis)

	// Detect anomalies
	analysis.Anomalies = ta.detectAnomalies(windowData, analysis)

	// Detect seasonal patterns (if enough data)
	if len(history) > 100 { // Need substantial data for seasonal analysis
		analysis.SeasonalPattern = ta.detectSeasonalPattern(history)
	}

	// Cache the result
	ta.lastAnalysis[metric] = analysis
	ta.lastUpdate = now

	ta.logger.Debug("Completed trend analysis",
		"metric", metric,
		"samples", len(windowData),
		"slope", analysis.Slope,
		"r_squared", analysis.RSquared,
		"trend_direction", analysis.TrendDirection,
		"trend_strength", analysis.TrendStrength)

	return analysis, nil
}

// performLinearRegression calculates linear regression parameters
func (ta *TrendAnalyzer) performLinearRegression(data []TrendPoint, analysis *TrendAnalysis) {
	if len(data) < 2 {
		return
	}

	// Convert timestamps to seconds since first point for regression
	baseTime := data[0].Timestamp
	var sumX, sumY, sumXY, sumX2, sumY2 float64
	var weightSum float64

	for _, point := range data {
		x := point.Timestamp.Sub(baseTime).Seconds()
		y := point.Value
		w := point.Quality // Use quality as weight

		sumX += x * w
		sumY += y * w
		sumXY += x * y * w
		sumX2 += x * x * w
		sumY2 += y * y * w
		weightSum += w
	}

	if weightSum == 0 || sumX2*weightSum == sumX*sumX {
		return // Avoid division by zero
	}

	// Weighted least squares regression
	analysis.Slope = (weightSum*sumXY - sumX*sumY) / (weightSum*sumX2 - sumX*sumX)
	analysis.Intercept = (sumY - analysis.Slope*sumX) / weightSum

	// Calculate R-squared
	meanY := sumY / weightSum
	var ssRes, ssTot float64

	for _, point := range data {
		x := point.Timestamp.Sub(baseTime).Seconds()
		y := point.Value
		w := point.Quality

		predicted := analysis.Intercept + analysis.Slope*x
		ssRes += w * (y - predicted) * (y - predicted)
		ssTot += w * (y - meanY) * (y - meanY)
	}

	if ssTot > 0 {
		analysis.RSquared = 1 - (ssRes / ssTot)
	}
}

// calculateStatistics calculates additional statistical measures
func (ta *TrendAnalyzer) calculateStatistics(data []TrendPoint, analysis *TrendAnalysis) {
	if len(data) < 2 {
		return
	}

	// Calculate variance and standard error
	var sumSquaredResiduals float64
	var weightSum float64
	baseTime := data[0].Timestamp

	for _, point := range data {
		x := point.Timestamp.Sub(baseTime).Seconds()
		y := point.Value
		w := point.Quality

		predicted := analysis.Intercept + analysis.Slope*x
		residual := y - predicted
		sumSquaredResiduals += w * residual * residual
		weightSum += w
	}

	if weightSum > 2 {
		analysis.Variance = sumSquaredResiduals / (weightSum - 2)
		analysis.StandardError = math.Sqrt(analysis.Variance)
	}

	// Calculate confidence level based on R-squared and sample size
	analysis.ConfidenceLevel = analysis.RSquared * math.Min(float64(len(data))/50.0, 1.0)
}

// classifyTrend determines trend strength and direction
func (ta *TrendAnalyzer) classifyTrend(analysis *TrendAnalysis) {
	// Determine direction
	if math.Abs(analysis.Slope) < 1e-6 {
		analysis.TrendDirection = "stable"
	} else if analysis.Slope > 0 {
		analysis.TrendDirection = "increasing"
	} else {
		analysis.TrendDirection = "decreasing"
	}

	// Determine strength based on R-squared and confidence
	strength := analysis.RSquared * analysis.ConfidenceLevel

	if strength < 0.3 {
		analysis.TrendStrength = "weak"
	} else if strength < 0.7 {
		analysis.TrendStrength = "moderate"
	} else {
		analysis.TrendStrength = "strong"
	}
}

// generatePrediction creates a future value prediction
func (ta *TrendAnalyzer) generatePrediction(data []TrendPoint, analysis *TrendAnalysis) *Prediction {
	if len(data) < 2 || analysis.StandardError == 0 {
		return nil
	}

	baseTime := data[0].Timestamp
	targetTime := time.Now().Add(ta.config.PredictionHorizon)
	x := targetTime.Sub(baseTime).Seconds()

	predictedValue := analysis.Intercept + analysis.Slope*x

	// Calculate confidence interval (95% confidence)
	tValue := 1.96 // Approximate t-value for 95% confidence
	margin := tValue * analysis.StandardError * math.Sqrt(1+1/float64(len(data)))

	prediction := &Prediction{
		TargetTime:     targetTime,
		PredictedValue: predictedValue,
		Confidence:     analysis.ConfidenceLevel,
	}

	prediction.ConfidenceInterval.Lower = predictedValue - margin
	prediction.ConfidenceInterval.Upper = predictedValue + margin

	return prediction
}

// detectAnomalies identifies anomalous data points
func (ta *TrendAnalyzer) detectAnomalies(data []TrendPoint, analysis *TrendAnalysis) []Anomaly {
	if analysis.StandardError == 0 {
		return nil
	}

	var anomalies []Anomaly
	baseTime := data[0].Timestamp
	threshold := ta.config.AnomalyThreshold * analysis.StandardError

	for _, point := range data {
		x := point.Timestamp.Sub(baseTime).Seconds()
		expected := analysis.Intercept + analysis.Slope*x
		deviation := math.Abs(point.Value - expected)

		if deviation > threshold {
			severity := "minor"
			if deviation > threshold*2 {
				severity = "moderate"
			}
			if deviation > threshold*3 {
				severity = "severe"
			}

			anomaly := Anomaly{
				Timestamp:     point.Timestamp,
				Value:         point.Value,
				ExpectedValue: expected,
				Deviation:     deviation,
				Severity:      severity,
			}

			anomalies = append(anomalies, anomaly)
		}
	}

	return anomalies
}

// detectSeasonalPattern attempts to detect seasonal patterns in the data
func (ta *TrendAnalyzer) detectSeasonalPattern(history []TrendPoint) *SeasonalInfo {
	if len(history) < 50 {
		return nil // Need substantial data for seasonal analysis
	}

	// Simple seasonal detection using autocorrelation
	// This is a simplified implementation - production code might use FFT or more sophisticated methods

	maxCorrelation := 0.0
	bestPeriod := time.Duration(0)

	// Test different periods
	minPeriodSamples := int(ta.config.SeasonalMinPeriod.Seconds())
	maxPeriodSamples := int(ta.config.SeasonalMaxPeriod.Seconds())

	for period := minPeriodSamples; period <= maxPeriodSamples && period < len(history)/2; period += 60 { // Test every minute
		correlation := ta.calculateAutocorrelation(history, period)
		if correlation > maxCorrelation {
			maxCorrelation = correlation
			bestPeriod = time.Duration(period) * time.Second
		}
	}

	// Only return seasonal info if correlation is strong enough
	if maxCorrelation > 0.5 {
		return &SeasonalInfo{
			Period:      bestPeriod,
			Amplitude:   ta.calculateSeasonalAmplitude(history, bestPeriod),
			Confidence:  maxCorrelation,
			Description: fmt.Sprintf("Seasonal pattern detected with period %v", bestPeriod),
		}
	}

	return nil
}

// calculateAutocorrelation calculates autocorrelation for a given lag
func (ta *TrendAnalyzer) calculateAutocorrelation(data []TrendPoint, lag int) float64 {
	if lag >= len(data) {
		return 0
	}

	// Calculate means
	var sum1, sum2 float64
	n := len(data) - lag

	for i := 0; i < n; i++ {
		sum1 += data[i].Value
		sum2 += data[i+lag].Value
	}

	mean1 := sum1 / float64(n)
	mean2 := sum2 / float64(n)

	// Calculate correlation
	var numerator, denom1, denom2 float64

	for i := 0; i < n; i++ {
		diff1 := data[i].Value - mean1
		diff2 := data[i+lag].Value - mean2

		numerator += diff1 * diff2
		denom1 += diff1 * diff1
		denom2 += diff2 * diff2
	}

	if denom1 == 0 || denom2 == 0 {
		return 0
	}

	return numerator / math.Sqrt(denom1*denom2)
}

// calculateSeasonalAmplitude calculates the amplitude of seasonal variation
func (ta *TrendAnalyzer) calculateSeasonalAmplitude(data []TrendPoint, period time.Duration) float64 {
	if len(data) < 10 {
		return 0
	}

	// Simple amplitude calculation as range of values
	var min, max float64
	min = data[0].Value
	max = data[0].Value

	for _, point := range data {
		if point.Value < min {
			min = point.Value
		}
		if point.Value > max {
			max = point.Value
		}
	}

	return (max - min) / 2.0
}

// GetStatus returns current analyzer status
func (ta *TrendAnalyzer) GetStatus() map[string]interface{} {
	ta.mu.RLock()
	defer ta.mu.RUnlock()

	return map[string]interface{}{
		"obstruction_history_size": len(ta.obstructionHistory),
		"snr_history_size":         len(ta.snrHistory),
		"latency_history_size":     len(ta.latencyHistory),
		"max_history_points":       ta.config.MaxHistoryPoints,
		"analysis_window":          ta.config.AnalysisWindow.String(),
		"prediction_horizon":       ta.config.PredictionHorizon.String(),
		"last_analysis_count":      len(ta.lastAnalysis),
		"last_update":              ta.lastUpdate,
	}
}
