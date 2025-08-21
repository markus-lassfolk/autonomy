package predictive

import (
	"context"
	"fmt"
	"math"
	"sync"
	"time"

	"github.com/markus-lassfolk/autonomy/pkg"
	"github.com/markus-lassfolk/autonomy/pkg/logx"
)

// ConnectionSample represents a generic connection measurement sample
type ConnectionSample struct {
	Timestamp     time.Time              `json:"timestamp"`
	InterfaceType string                 `json:"interface_type"` // "starlink", "cellular", "wifi", "lan"
	Metrics       *pkg.Metrics           `json:"metrics"`
	Quality       float64                `json:"quality"`  // Data quality score (0-1)
	Metadata      map[string]interface{} `json:"metadata"` // Interface-specific data
}

// PredictionTrend represents trend analysis results for any connection type
type PredictionTrend struct {
	InterfaceType     string                 `json:"interface_type"`
	LatencyTrend      float64                `json:"latency_trend"`      // Rate of change in latency (ms/sample)
	LossTrend         float64                `json:"loss_trend"`         // Rate of change in packet loss (%/sample)
	SignalTrend       float64                `json:"signal_trend"`       // Rate of change in signal strength
	ThroughputTrend   float64                `json:"throughput_trend"`   // Rate of change in throughput
	PredictedLatency  float64                `json:"predicted_latency"`  // Predicted latency in next sample
	PredictedLoss     float64                `json:"predicted_loss"`     // Predicted loss in next sample
	PredictedSignal   float64                `json:"predicted_signal"`   // Predicted signal strength
	Confidence        float64                `json:"confidence"`         // Confidence in prediction (0-1)
	TrendDirection    string                 `json:"trend_direction"`    // "improving", "degrading", "stable"
	TimeToFailure     *time.Duration         `json:"time_to_failure"`    // Estimated time until failure
	FailureRisk       float64                `json:"failure_risk"`       // Risk of failure (0-1)
	InterfaceSpecific map[string]interface{} `json:"interface_specific"` // Interface-specific predictions
}

// GenericPredictor provides predictive analysis for any connection type
type GenericPredictor struct {
	mu                    sync.RWMutex
	logger                *logx.Logger
	interfaceType         string
	samples               []ConnectionSample
	maxSamples            int
	minSamplesForAnalysis int

	// Thresholds (interface-agnostic)
	criticalLatencyThreshold float64 // Latency that triggers concern (ms)
	criticalLossThreshold    float64 // Loss percentage that triggers concern
	degradationThreshold     float64 // Rate of degradation that triggers early warning

	// Interface-specific predictors
	starlinkPredictor *StarlinkSpecificPredictor
	cellularPredictor *CellularSpecificPredictor
	wifiPredictor     *WiFiSpecificPredictor

	// Configuration
	config *GenericPredictorConfig
}

// GenericPredictorConfig holds configuration for the generic predictor
type GenericPredictorConfig struct {
	MaxSamples               int           `json:"max_samples"`
	MinSamplesForAnalysis    int           `json:"min_samples_for_analysis"`
	CriticalLatencyThreshold float64       `json:"critical_latency_threshold"` // ms
	CriticalLossThreshold    float64       `json:"critical_loss_threshold"`    // percentage
	DegradationThreshold     float64       `json:"degradation_threshold"`      // rate of change
	PredictionWindow         time.Duration `json:"prediction_window"`
	ConfidenceThreshold      float64       `json:"confidence_threshold"`
	EnableInterfaceSpecific  bool          `json:"enable_interface_specific"`
}

// DefaultGenericPredictorConfig returns default configuration
func DefaultGenericPredictorConfig() *GenericPredictorConfig {
	return &GenericPredictorConfig{
		MaxSamples:               300, // 5 minutes at 1s intervals
		MinSamplesForAnalysis:    10,  // Need at least 10 samples for trends
		CriticalLatencyThreshold: 500, // 500ms latency is concerning
		CriticalLossThreshold:    5.0, // 5% loss is concerning
		DegradationThreshold:     0.1, // 10% degradation per sample
		PredictionWindow:         30 * time.Second,
		ConfidenceThreshold:      0.7, // 70% confidence required
		EnableInterfaceSpecific:  true,
	}
}

// NewGenericPredictor creates a new generic connection predictor
func NewGenericPredictor(logger *logx.Logger, interfaceType string, config *GenericPredictorConfig) *GenericPredictor {
	if config == nil {
		config = DefaultGenericPredictorConfig()
	}

	gp := &GenericPredictor{
		logger:                   logger,
		interfaceType:            interfaceType,
		samples:                  make([]ConnectionSample, 0, config.MaxSamples),
		maxSamples:               config.MaxSamples,
		minSamplesForAnalysis:    config.MinSamplesForAnalysis,
		criticalLatencyThreshold: config.CriticalLatencyThreshold,
		criticalLossThreshold:    config.CriticalLossThreshold,
		degradationThreshold:     config.DegradationThreshold,
		config:                   config,
	}

	// Initialize interface-specific predictors if enabled
	if config.EnableInterfaceSpecific {
		switch interfaceType {
		case "starlink":
			gp.starlinkPredictor = NewStarlinkSpecificPredictor(logger)
		case "cellular":
			gp.cellularPredictor = NewCellularSpecificPredictor(logger)
		case "wifi":
			gp.wifiPredictor = NewWiFiSpecificPredictor(logger)
		}
	}

	return gp
}

// AddSample adds a new connection sample for analysis
func (gp *GenericPredictor) AddSample(ctx context.Context, metrics *pkg.Metrics, quality float64) error {
	gp.mu.Lock()
	defer gp.mu.Unlock()

	sample := ConnectionSample{
		Timestamp:     time.Now(),
		InterfaceType: gp.interfaceType,
		Metrics:       metrics,
		Quality:       quality,
		Metadata:      make(map[string]interface{}),
	}

	// Add interface-specific metadata
	gp.addInterfaceSpecificMetadata(&sample)

	// Add sample to ring buffer
	gp.samples = append(gp.samples, sample)
	if len(gp.samples) > gp.maxSamples {
		gp.samples = gp.samples[1:] // Remove oldest sample
	}

	// Update interface-specific predictors
	if gp.config.EnableInterfaceSpecific {
		gp.updateInterfaceSpecificPredictors(ctx, &sample)
	}

	gp.logger.Debug("Added connection sample",
		"interface_type", gp.interfaceType,
		"timestamp", sample.Timestamp,
		"quality", quality,
		"samples_count", len(gp.samples))

	return nil
}

// addInterfaceSpecificMetadata adds interface-specific data to the sample
func (gp *GenericPredictor) addInterfaceSpecificMetadata(sample *ConnectionSample) {
	switch gp.interfaceType {
	case "starlink":
		if sample.Metrics.ObstructionPct != nil {
			sample.Metadata["obstruction_pct"] = *sample.Metrics.ObstructionPct
		}
		if sample.Metrics.SNR != nil {
			sample.Metadata["snr"] = *sample.Metrics.SNR
		}

	case "cellular":
		if sample.Metrics.RSRP != nil {
			sample.Metadata["rsrp"] = *sample.Metrics.RSRP
		}
		if sample.Metrics.RSRQ != nil {
			sample.Metadata["rsrq"] = *sample.Metrics.RSRQ
		}
		if sample.Metrics.SINR != nil {
			sample.Metadata["sinr"] = *sample.Metrics.SINR
		}
		if sample.Metrics.StabilityScore != nil {
			sample.Metadata["stability_score"] = *sample.Metrics.StabilityScore
		}

	case "wifi":
		if sample.Metrics.SignalStrength != nil {
			sample.Metadata["signal_strength"] = *sample.Metrics.SignalStrength
		}
		if sample.Metrics.NoiseLevel != nil {
			sample.Metadata["noise_level"] = *sample.Metrics.NoiseLevel
		}
		if sample.Metrics.Quality != nil {
			sample.Metadata["quality"] = *sample.Metrics.Quality
		}
		if sample.Metrics.Channel != nil {
			sample.Metadata["channel"] = *sample.Metrics.Channel
		}
	}
}

// updateInterfaceSpecificPredictors updates specialized predictors
func (gp *GenericPredictor) updateInterfaceSpecificPredictors(ctx context.Context, sample *ConnectionSample) {
	switch gp.interfaceType {
	case "starlink":
		if gp.starlinkPredictor != nil {
			if err := gp.starlinkPredictor.AddSample(ctx, sample); err != nil {
				gp.logger.Warn("Failed to add sample to starlink predictor", "error", err)
			}
		}
	case "cellular":
		if gp.cellularPredictor != nil {
			if err := gp.cellularPredictor.AddSample(ctx, sample); err != nil {
				gp.logger.Warn("Failed to add sample to cellular predictor", "error", err)
			}
		}
	case "wifi":
		if gp.wifiPredictor != nil {
			if err := gp.wifiPredictor.AddSample(ctx, sample); err != nil {
				gp.logger.Warn("Failed to add sample to wifi predictor", "error", err)
			}
		}
	}
}

// AnalyzeTrends performs comprehensive trend analysis
func (gp *GenericPredictor) AnalyzeTrends(ctx context.Context) (*PredictionTrend, error) {
	gp.mu.RLock()
	defer gp.mu.RUnlock()

	if len(gp.samples) < gp.minSamplesForAnalysis {
		return nil, fmt.Errorf("insufficient samples for analysis: have %d, need %d",
			len(gp.samples), gp.minSamplesForAnalysis)
	}

	trend := &PredictionTrend{
		InterfaceType:     gp.interfaceType,
		InterfaceSpecific: make(map[string]interface{}),
	}

	// Calculate generic trends
	trend.LatencyTrend = gp.calculateLatencyTrend()
	trend.LossTrend = gp.calculateLossTrend()
	trend.SignalTrend = gp.calculateSignalTrend()
	trend.ThroughputTrend = gp.calculateThroughputTrend()

	// Predict future values
	trend.PredictedLatency = gp.predictFutureLatency()
	trend.PredictedLoss = gp.predictFutureLoss()
	trend.PredictedSignal = gp.predictFutureSignal()

	// Calculate confidence and risk
	trend.Confidence = gp.calculateConfidence()
	trend.FailureRisk = gp.calculateFailureRisk()

	// Determine trend direction
	trend.TrendDirection = gp.determineTrendDirection(trend)

	// Estimate time to failure
	trend.TimeToFailure = gp.estimateTimeToFailure(trend)

	// Add interface-specific analysis
	if gp.config.EnableInterfaceSpecific {
		gp.addInterfaceSpecificAnalysis(ctx, trend)
	}

	gp.logger.Debug("Analyzed connection trends",
		"interface_type", gp.interfaceType,
		"latency_trend", trend.LatencyTrend,
		"loss_trend", trend.LossTrend,
		"confidence", trend.Confidence,
		"failure_risk", trend.FailureRisk,
		"direction", trend.TrendDirection)

	return trend, nil
}

// calculateLatencyTrend calculates the rate of change in latency
func (gp *GenericPredictor) calculateLatencyTrend() float64 {
	if len(gp.samples) < 3 {
		return 0
	}

	// Use linear regression on recent samples
	recentSamples := gp.getRecentSamples(20) // Last 20 samples

	var sumX, sumY, sumXY, sumX2 float64
	validSamples := 0

	for i, sample := range recentSamples {
		if sample.Metrics.LatencyMS != nil {
			x := float64(i)
			y := *sample.Metrics.LatencyMS
			sumX += x
			sumY += y
			sumXY += x * y
			sumX2 += x * x
			validSamples++
		}
	}

	if validSamples < 3 {
		return 0
	}

	n := float64(validSamples)
	slope := (n*sumXY - sumX*sumY) / (n*sumX2 - sumX*sumX)

	return slope
}

// calculateLossTrend calculates the rate of change in packet loss
func (gp *GenericPredictor) calculateLossTrend() float64 {
	if len(gp.samples) < 3 {
		return 0
	}

	recentSamples := gp.getRecentSamples(20)

	var sumX, sumY, sumXY, sumX2 float64
	validSamples := 0

	for i, sample := range recentSamples {
		if sample.Metrics.LossPercent != nil {
			x := float64(i)
			y := *sample.Metrics.LossPercent
			sumX += x
			sumY += y
			sumXY += x * y
			sumX2 += x * x
			validSamples++
		}
	}

	if validSamples < 3 {
		return 0
	}

	n := float64(validSamples)
	slope := (n*sumXY - sumX*sumY) / (n*sumX2 - sumX*sumX)

	return slope
}

// calculateSignalTrend calculates the rate of change in signal strength (interface-specific)
func (gp *GenericPredictor) calculateSignalTrend() float64 {
	if len(gp.samples) < 3 {
		return 0
	}

	recentSamples := gp.getRecentSamples(20)

	var sumX, sumY, sumXY, sumX2 float64
	validSamples := 0

	for i, sample := range recentSamples {
		var signalValue float64
		hasSignal := false

		// Extract signal value based on interface type
		switch gp.interfaceType {
		case "starlink":
			if sample.Metrics.SNR != nil {
				signalValue = float64(*sample.Metrics.SNR)
				hasSignal = true
			}
		case "cellular":
			if sample.Metrics.RSRP != nil {
				signalValue = *sample.Metrics.RSRP
				hasSignal = true
			}
		case "wifi":
			if sample.Metrics.SignalStrength != nil {
				signalValue = float64(*sample.Metrics.SignalStrength)
				hasSignal = true
			}
		}

		if hasSignal {
			x := float64(i)
			y := signalValue
			sumX += x
			sumY += y
			sumXY += x * y
			sumX2 += x * x
			validSamples++
		}
	}

	if validSamples < 3 {
		return 0
	}

	n := float64(validSamples)
	slope := (n*sumXY - sumX*sumY) / (n*sumX2 - sumX*sumX)

	return slope
}

// calculateThroughputTrend calculates the rate of change in throughput
func (gp *GenericPredictor) calculateThroughputTrend() float64 {
	if len(gp.samples) < 3 {
		return 0
	}

	recentSamples := gp.getRecentSamples(20)

	var sumX, sumY, sumXY, sumX2 float64
	validSamples := 0

	for i, sample := range recentSamples {
		if sample.Metrics.ThroughputKbps != nil {
			x := float64(i)
			y := *sample.Metrics.ThroughputKbps
			sumX += x
			sumY += y
			sumXY += x * y
			sumX2 += x * x
			validSamples++
		}
	}

	if validSamples < 3 {
		return 0
	}

	n := float64(validSamples)
	slope := (n*sumXY - sumX*sumY) / (n*sumX2 - sumX*sumX)

	return slope
}

// predictFutureLatency predicts latency in the next sample
func (gp *GenericPredictor) predictFutureLatency() float64 {
	if len(gp.samples) == 0 {
		return 0
	}

	latest := gp.samples[len(gp.samples)-1]
	if latest.Metrics.LatencyMS == nil {
		return 0
	}

	current := *latest.Metrics.LatencyMS
	trend := gp.calculateLatencyTrend()

	predicted := current + trend

	// Clamp to reasonable range [0, 10000ms]
	return math.Max(0, math.Min(10000, predicted))
}

// predictFutureLoss predicts packet loss in the next sample
func (gp *GenericPredictor) predictFutureLoss() float64 {
	if len(gp.samples) == 0 {
		return 0
	}

	latest := gp.samples[len(gp.samples)-1]
	if latest.Metrics.LossPercent == nil {
		return 0
	}

	current := *latest.Metrics.LossPercent
	trend := gp.calculateLossTrend()

	predicted := current + trend

	// Clamp to valid range [0, 100%]
	return math.Max(0, math.Min(100, predicted))
}

// predictFutureSignal predicts signal strength in the next sample
func (gp *GenericPredictor) predictFutureSignal() float64 {
	if len(gp.samples) == 0 {
		return 0
	}

	latest := gp.samples[len(gp.samples)-1]
	var current float64

	// Get current signal value based on interface type
	switch gp.interfaceType {
	case "starlink":
		if latest.Metrics.SNR != nil {
			current = float64(*latest.Metrics.SNR)
		}
	case "cellular":
		if latest.Metrics.RSRP != nil {
			current = *latest.Metrics.RSRP
		}
	case "wifi":
		if latest.Metrics.SignalStrength != nil {
			current = float64(*latest.Metrics.SignalStrength)
		}
	}

	if current == 0 {
		return 0
	}

	trend := gp.calculateSignalTrend()
	predicted := current + trend

	// Interface-specific clamping
	switch gp.interfaceType {
	case "starlink":
		return math.Max(0, math.Min(30, predicted)) // SNR range 0-30 dB
	case "cellular":
		return math.Max(-140, math.Min(-40, predicted)) // RSRP range -140 to -40 dBm
	case "wifi":
		return math.Max(-100, math.Min(0, predicted)) // Signal strength range -100 to 0 dBm
	default:
		return predicted
	}
}

// calculateConfidence calculates confidence in the predictions
func (gp *GenericPredictor) calculateConfidence() float64 {
	if len(gp.samples) < gp.minSamplesForAnalysis {
		return 0
	}

	confidence := 0.0

	// Factor 1: Sample count (30% weight)
	sampleConfidence := math.Min(float64(len(gp.samples))/float64(gp.maxSamples), 1.0)
	confidence += sampleConfidence * 0.3

	// Factor 2: Data quality (40% weight)
	avgQuality := gp.calculateAverageQuality()
	confidence += avgQuality * 0.4

	// Factor 3: Trend consistency (30% weight)
	trendConsistency := gp.calculateTrendConsistency()
	confidence += trendConsistency * 0.3

	return math.Min(confidence, 1.0)
}

// calculateFailureRisk calculates the risk of connection failure
func (gp *GenericPredictor) calculateFailureRisk() float64 {
	if len(gp.samples) == 0 {
		return 0
	}

	risk := 0.0

	// Latency risk
	predictedLatency := gp.predictFutureLatency()
	if predictedLatency > gp.criticalLatencyThreshold {
		latencyRisk := math.Min((predictedLatency-gp.criticalLatencyThreshold)/gp.criticalLatencyThreshold, 1.0)
		risk = math.Max(risk, latencyRisk)
	}

	// Loss risk
	predictedLoss := gp.predictFutureLoss()
	if predictedLoss > gp.criticalLossThreshold {
		lossRisk := math.Min((predictedLoss-gp.criticalLossThreshold)/(100-gp.criticalLossThreshold), 1.0)
		risk = math.Max(risk, lossRisk)
	}

	// Signal degradation risk (interface-specific)
	signalRisk := gp.calculateSignalRisk()
	risk = math.Max(risk, signalRisk)

	return risk
}

// calculateSignalRisk calculates risk based on signal degradation
func (gp *GenericPredictor) calculateSignalRisk() float64 {
	signalTrend := gp.calculateSignalTrend()

	// Interface-specific risk calculation
	switch gp.interfaceType {
	case "starlink":
		// SNR degradation risk
		if signalTrend < -0.5 { // Losing 0.5 dB per sample
			return math.Min(math.Abs(signalTrend)/2.0, 1.0)
		}
	case "cellular":
		// RSRP degradation risk
		if signalTrend < -1.0 { // Losing 1 dBm per sample
			return math.Min(math.Abs(signalTrend)/10.0, 1.0)
		}
	case "wifi":
		// Signal strength degradation risk
		if signalTrend < -2.0 { // Losing 2 dBm per sample
			return math.Min(math.Abs(signalTrend)/20.0, 1.0)
		}
	}

	return 0
}

// ShouldTriggerFailover determines if predictive failover should be triggered
func (gp *GenericPredictor) ShouldTriggerFailover(ctx context.Context) (bool, string, error) {
	trend, err := gp.AnalyzeTrends(ctx)
	if err != nil {
		return false, "", err
	}

	// Check if confidence is sufficient
	if trend.Confidence < gp.config.ConfidenceThreshold {
		return false, fmt.Sprintf("insufficient confidence: %.2f < %.2f",
			trend.Confidence, gp.config.ConfidenceThreshold), nil
	}

	// Check for rapid latency increase
	if trend.LatencyTrend > gp.degradationThreshold*gp.criticalLatencyThreshold {
		return true, fmt.Sprintf("rapid latency increase detected: %.2f ms/sample",
			trend.LatencyTrend), nil
	}

	// Check if predicted latency exceeds threshold
	if trend.PredictedLatency > gp.criticalLatencyThreshold {
		return true, fmt.Sprintf("predicted latency %.1f ms > %.1f ms threshold",
			trend.PredictedLatency, gp.criticalLatencyThreshold), nil
	}

	// Check for rapid loss increase
	if trend.LossTrend > gp.degradationThreshold*gp.criticalLossThreshold {
		return true, fmt.Sprintf("rapid packet loss increase detected: %.2f%%/sample",
			trend.LossTrend), nil
	}

	// Check if predicted loss exceeds threshold
	if trend.PredictedLoss > gp.criticalLossThreshold {
		return true, fmt.Sprintf("predicted loss %.1f%% > %.1f%% threshold",
			trend.PredictedLoss, gp.criticalLossThreshold), nil
	}

	// Check overall failure risk
	if trend.FailureRisk > 0.8 {
		return true, fmt.Sprintf("high failure risk detected: %.2f", trend.FailureRisk), nil
	}

	// Check if time to failure is within prediction window
	if trend.TimeToFailure != nil && *trend.TimeToFailure < gp.config.PredictionWindow {
		return true, fmt.Sprintf("predicted failure in %v < %v window",
			*trend.TimeToFailure, gp.config.PredictionWindow), nil
	}

	// Check interface-specific triggers
	if gp.config.EnableInterfaceSpecific {
		shouldTrigger, reason := gp.checkInterfaceSpecificTriggers(ctx, trend)
		if shouldTrigger {
			return true, reason, nil
		}
	}

	return false, "no predictive failover triggers detected", nil
}

// Helper methods

func (gp *GenericPredictor) getRecentSamples(count int) []ConnectionSample {
	if count >= len(gp.samples) {
		return gp.samples
	}
	return gp.samples[len(gp.samples)-count:]
}

func (gp *GenericPredictor) calculateAverageQuality() float64 {
	if len(gp.samples) == 0 {
		return 0
	}

	var sum float64
	for _, sample := range gp.samples {
		sum += sample.Quality
	}
	return sum / float64(len(gp.samples))
}

func (gp *GenericPredictor) calculateTrendConsistency() float64 {
	if len(gp.samples) < 5 {
		return 0
	}

	// Calculate variance in trend changes
	var changes []float64
	recentSamples := gp.getRecentSamples(10)

	for i := 1; i < len(recentSamples); i++ {
		if recentSamples[i].Metrics.LatencyMS != nil && recentSamples[i-1].Metrics.LatencyMS != nil {
			change := *recentSamples[i].Metrics.LatencyMS - *recentSamples[i-1].Metrics.LatencyMS
			changes = append(changes, change)
		}
	}

	if len(changes) == 0 {
		return 0
	}

	// Calculate variance
	var sum, sumSquares float64
	for _, change := range changes {
		sum += change
		sumSquares += change * change
	}

	mean := sum / float64(len(changes))
	variance := (sumSquares - sum*mean) / float64(len(changes))

	// Lower variance = higher consistency
	consistency := math.Exp(-variance / 100) // Scale factor for sensitivity

	return math.Min(consistency, 1.0)
}

func (gp *GenericPredictor) determineTrendDirection(trend *PredictionTrend) string {
	// Combine multiple trend indicators
	degradationScore := 0.0

	if trend.LatencyTrend > 1.0 {
		degradationScore += 1.0
	} else if trend.LatencyTrend < -1.0 {
		degradationScore -= 1.0
	}

	if trend.LossTrend > 0.1 {
		degradationScore += 1.0
	} else if trend.LossTrend < -0.1 {
		degradationScore -= 1.0
	}

	if trend.SignalTrend < -0.5 {
		degradationScore += 1.0
	} else if trend.SignalTrend > 0.5 {
		degradationScore -= 1.0
	}

	if degradationScore > 0.5 {
		return "degrading"
	} else if degradationScore < -0.5 {
		return "improving"
	}

	return "stable"
}

func (gp *GenericPredictor) estimateTimeToFailure(trend *PredictionTrend) *time.Duration {
	if len(gp.samples) == 0 {
		return nil
	}

	latest := gp.samples[len(gp.samples)-1]

	// Estimate based on latency trend
	if trend.LatencyTrend > 0 && latest.Metrics.LatencyMS != nil {
		remainingLatency := gp.criticalLatencyThreshold - *latest.Metrics.LatencyMS
		if remainingLatency > 0 {
			samplesUntilFailure := remainingLatency / trend.LatencyTrend
			duration := time.Duration(samplesUntilFailure) * time.Second
			return &duration
		}
	}

	// Estimate based on loss trend
	if trend.LossTrend > 0 && latest.Metrics.LossPercent != nil {
		remainingLoss := gp.criticalLossThreshold - *latest.Metrics.LossPercent
		if remainingLoss > 0 {
			samplesUntilFailure := remainingLoss / trend.LossTrend
			duration := time.Duration(samplesUntilFailure) * time.Second
			return &duration
		}
	}

	return nil
}

func (gp *GenericPredictor) addInterfaceSpecificAnalysis(ctx context.Context, trend *PredictionTrend) {
	switch gp.interfaceType {
	case "starlink":
		if gp.starlinkPredictor != nil {
			analysis := gp.starlinkPredictor.GetAnalysis(ctx)
			trend.InterfaceSpecific["starlink"] = analysis
		}
	case "cellular":
		if gp.cellularPredictor != nil {
			analysis := gp.cellularPredictor.GetAnalysis(ctx)
			trend.InterfaceSpecific["cellular"] = analysis
		}
	case "wifi":
		if gp.wifiPredictor != nil {
			analysis := gp.wifiPredictor.GetAnalysis(ctx)
			trend.InterfaceSpecific["wifi"] = analysis
		}
	}
}

func (gp *GenericPredictor) checkInterfaceSpecificTriggers(ctx context.Context, trend *PredictionTrend) (bool, string) {
	switch gp.interfaceType {
	case "starlink":
		if gp.starlinkPredictor != nil {
			return gp.starlinkPredictor.ShouldTriggerFailover(ctx)
		}
	case "cellular":
		if gp.cellularPredictor != nil {
			return gp.cellularPredictor.ShouldTriggerFailover(ctx)
		}
	case "wifi":
		if gp.wifiPredictor != nil {
			return gp.wifiPredictor.ShouldTriggerFailover(ctx)
		}
	}

	return false, "no interface-specific triggers"
}

// GetStatus returns current predictor status
func (gp *GenericPredictor) GetStatus() map[string]interface{} {
	gp.mu.RLock()
	defer gp.mu.RUnlock()

	status := map[string]interface{}{
		"interface_type":             gp.interfaceType,
		"samples_count":              len(gp.samples),
		"max_samples":                gp.maxSamples,
		"min_samples_for_analysis":   gp.minSamplesForAnalysis,
		"critical_latency_threshold": gp.criticalLatencyThreshold,
		"critical_loss_threshold":    gp.criticalLossThreshold,
		"degradation_threshold":      gp.degradationThreshold,
	}

	if len(gp.samples) > 0 {
		latest := gp.samples[len(gp.samples)-1]
		status["latest_sample"] = map[string]interface{}{
			"timestamp": latest.Timestamp,
			"quality":   latest.Quality,
		}

		if latest.Metrics.LatencyMS != nil {
			status["latest_latency"] = *latest.Metrics.LatencyMS
		}
		if latest.Metrics.LossPercent != nil {
			status["latest_loss"] = *latest.Metrics.LossPercent
		}
	}

	// Add interface-specific status
	if gp.config.EnableInterfaceSpecific {
		switch gp.interfaceType {
		case "starlink":
			if gp.starlinkPredictor != nil {
				status["starlink_specific"] = gp.starlinkPredictor.GetStatus()
			}
		case "cellular":
			if gp.cellularPredictor != nil {
				status["cellular_specific"] = gp.cellularPredictor.GetStatus()
			}
		case "wifi":
			if gp.wifiPredictor != nil {
				status["wifi_specific"] = gp.wifiPredictor.GetStatus()
			}
		}
	}

	return status
}
