package obstruction

import (
	"context"
	"fmt"
	"math"
	"sync"
	"time"

	"github.com/markus-lassfolk/autonomy/pkg"
	"github.com/markus-lassfolk/autonomy/pkg/logx"
)

// ObstructionSample represents a single obstruction measurement
type ObstructionSample struct {
	Timestamp                        time.Time
	CurrentlyObstructed              bool
	FractionObstructed               float64
	TimeObstructed                   float64
	SNR                              float64
	AvgProlongedObstructionIntervalS float64
	ValidS                           int
	PatchesValid                     int
	WedgeFractionObstructed          []float64
	WedgeAbsFractionObstructed       []float64
}

// ObstructionTrend represents trend analysis results
type ObstructionTrend struct {
	ObstructionAcceleration float64        // Rate of change in obstruction percentage
	SNRTrend                float64        // Rate of change in SNR
	PredictedObstruction    float64        // Predicted obstruction in next sample
	PredictedSNR            float64        // Predicted SNR in next sample
	Confidence              float64        // Confidence in prediction (0-1)
	TrendDirection          string         // "improving", "degrading", "stable"
	TimeToFailure           *time.Duration // Estimated time until complete failure
}

// ObstructionPredictor provides predictive obstruction analysis
type ObstructionPredictor struct {
	mu                    sync.RWMutex
	logger                *logx.Logger
	samples               []ObstructionSample
	maxSamples            int
	minSamplesForAnalysis int

	// Thresholds for prediction
	criticalObstructionThreshold float64 // Obstruction % that triggers failover
	criticalSNRThreshold         float64 // SNR level that triggers failover
	accelerationThreshold        float64 // Rate of change that triggers early warning

	// Pattern learning
	environmentalPatterns map[string]*EnvironmentalPattern
	movementDetector      *MovementDetector

	// Configuration
	config *PredictorConfig
}

// PredictorConfig holds configuration for the obstruction predictor
type PredictorConfig struct {
	MaxSamples                   int           `json:"max_samples"`
	MinSamplesForAnalysis        int           `json:"min_samples_for_analysis"`
	CriticalObstructionThreshold float64       `json:"critical_obstruction_threshold"`
	CriticalSNRThreshold         float64       `json:"critical_snr_threshold"`
	AccelerationThreshold        float64       `json:"acceleration_threshold"`
	PredictionWindow             time.Duration `json:"prediction_window"`
	ConfidenceThreshold          float64       `json:"confidence_threshold"`
	EnablePatternLearning        bool          `json:"enable_pattern_learning"`
	EnableMovementDetection      bool          `json:"enable_movement_detection"`
}

// DefaultPredictorConfig returns default configuration
func DefaultPredictorConfig() *PredictorConfig {
	return &PredictorConfig{
		MaxSamples:                   300,  // 5 minutes at 1s intervals
		MinSamplesForAnalysis:        10,   // Need at least 10 samples for trends
		CriticalObstructionThreshold: 0.15, // 15% obstruction triggers concern
		CriticalSNRThreshold:         8.0,  // SNR below 8dB is concerning
		AccelerationThreshold:        0.02, // 2% increase per sample is rapid
		PredictionWindow:             30 * time.Second,
		ConfidenceThreshold:          0.7, // 70% confidence required
		EnablePatternLearning:        true,
		EnableMovementDetection:      true,
	}
}

// NewObstructionPredictor creates a new obstruction predictor
func NewObstructionPredictor(logger *logx.Logger, config *PredictorConfig) *ObstructionPredictor {
	if config == nil {
		config = DefaultPredictorConfig()
	}

	return &ObstructionPredictor{
		logger:                       logger,
		samples:                      make([]ObstructionSample, 0, config.MaxSamples),
		maxSamples:                   config.MaxSamples,
		minSamplesForAnalysis:        config.MinSamplesForAnalysis,
		criticalObstructionThreshold: config.CriticalObstructionThreshold,
		criticalSNRThreshold:         config.CriticalSNRThreshold,
		accelerationThreshold:        config.AccelerationThreshold,
		environmentalPatterns:        make(map[string]*EnvironmentalPattern),
		movementDetector:             NewMovementDetector(logger),
		config:                       config,
	}
}

// AddSample adds a new obstruction sample for analysis
func (op *ObstructionPredictor) AddSample(ctx context.Context, metrics *pkg.Metrics) error {
	op.mu.Lock()
	defer op.mu.Unlock()

	// Extract obstruction data from metrics
	sample := ObstructionSample{
		Timestamp:           time.Now(),
		CurrentlyObstructed: false, // Default to false, will be set based on obstruction percentage
		FractionObstructed:  0,
		TimeObstructed:      0,
		SNR:                 0,
		ValidS:              0,
		PatchesValid:        0,
	}

	// Safely extract pointer values
	if metrics.ObstructionPct != nil {
		sample.FractionObstructed = *metrics.ObstructionPct / 100.0 // Convert percentage to fraction
		sample.CurrentlyObstructed = *metrics.ObstructionPct > 0    // Consider obstructed if > 0%
	}
	if metrics.SNR != nil {
		sample.SNR = float64(*metrics.SNR) // Convert int to float64
	}
	if metrics.ObstructionTimePct != nil {
		sample.TimeObstructed = *metrics.ObstructionTimePct
	}
	if metrics.ObstructionValidS != nil {
		sample.ValidS = int(*metrics.ObstructionValidS) // Convert int64 to int
	}
	if metrics.ObstructionPatchesValid != nil {
		sample.PatchesValid = *metrics.ObstructionPatchesValid
	}
	if metrics.ObstructionAvgProlonged != nil {
		sample.AvgProlongedObstructionIntervalS = *metrics.ObstructionAvgProlonged
	}

	// Add sample to ring buffer
	op.samples = append(op.samples, sample)
	if len(op.samples) > op.maxSamples {
		op.samples = op.samples[1:] // Remove oldest sample
	}

	op.logger.Debug("Added obstruction sample",
		"timestamp", sample.Timestamp,
		"obstructed", sample.CurrentlyObstructed,
		"fraction", sample.FractionObstructed,
		"snr", sample.SNR,
		"samples_count", len(op.samples))

	return nil
}

// AnalyzeTrends performs trend analysis on recent samples
func (op *ObstructionPredictor) AnalyzeTrends(ctx context.Context) (*ObstructionTrend, error) {
	op.mu.RLock()
	defer op.mu.RUnlock()

	if len(op.samples) < op.minSamplesForAnalysis {
		return nil, fmt.Errorf("insufficient samples for analysis: have %d, need %d",
			len(op.samples), op.minSamplesForAnalysis)
	}

	trend := &ObstructionTrend{}

	// Calculate obstruction acceleration (rate of change)
	trend.ObstructionAcceleration = op.calculateObstructionAcceleration()

	// Calculate SNR trend
	trend.SNRTrend = op.calculateSNRTrend()

	// Predict future values
	trend.PredictedObstruction = op.predictFutureObstruction()
	trend.PredictedSNR = op.predictFutureSNR()

	// Calculate confidence based on data quality and trend consistency
	trend.Confidence = op.calculateConfidence()

	// Determine trend direction
	trend.TrendDirection = op.determineTrendDirection(trend.ObstructionAcceleration, trend.SNRTrend)

	// Estimate time to failure if trend continues
	trend.TimeToFailure = op.estimateTimeToFailure(trend.ObstructionAcceleration, trend.SNRTrend)

	op.logger.Debug("Analyzed obstruction trends",
		"obstruction_accel", trend.ObstructionAcceleration,
		"snr_trend", trend.SNRTrend,
		"predicted_obstruction", trend.PredictedObstruction,
		"predicted_snr", trend.PredictedSNR,
		"confidence", trend.Confidence,
		"direction", trend.TrendDirection)

	return trend, nil
}

// ShouldTriggerFailover determines if predictive failover should be triggered
func (op *ObstructionPredictor) ShouldTriggerFailover(ctx context.Context) (bool, string, error) {
	trend, err := op.AnalyzeTrends(ctx)
	if err != nil {
		return false, "", err
	}

	// Check if confidence is sufficient
	if trend.Confidence < op.config.ConfidenceThreshold {
		return false, fmt.Sprintf("insufficient confidence: %.2f < %.2f",
			trend.Confidence, op.config.ConfidenceThreshold), nil
	}

	// Check for rapid obstruction increase
	if trend.ObstructionAcceleration > op.accelerationThreshold {
		return true, fmt.Sprintf("rapid obstruction increase detected: %.4f/sample > %.4f threshold",
			trend.ObstructionAcceleration, op.accelerationThreshold), nil
	}

	// Check if predicted obstruction exceeds threshold
	if trend.PredictedObstruction > op.criticalObstructionThreshold {
		return true, fmt.Sprintf("predicted obstruction %.2f%% > %.2f%% threshold",
			trend.PredictedObstruction*100, op.criticalObstructionThreshold*100), nil
	}

	// Check if predicted SNR drops below threshold
	if trend.PredictedSNR < op.criticalSNRThreshold {
		return true, fmt.Sprintf("predicted SNR %.1f dB < %.1f dB threshold",
			trend.PredictedSNR, op.criticalSNRThreshold), nil
	}

	// Check if time to failure is within prediction window
	if trend.TimeToFailure != nil && *trend.TimeToFailure < op.config.PredictionWindow {
		return true, fmt.Sprintf("predicted failure in %v < %v window",
			*trend.TimeToFailure, op.config.PredictionWindow), nil
	}

	return false, "no predictive failover triggers detected", nil
}

// calculateObstructionAcceleration calculates the rate of change in obstruction
func (op *ObstructionPredictor) calculateObstructionAcceleration() float64 {
	if len(op.samples) < 3 {
		return 0
	}

	// Use linear regression to find slope of obstruction over time
	n := len(op.samples)
	recentSamples := op.samples[max(0, n-20):] // Use last 20 samples for acceleration

	if len(recentSamples) < 3 {
		return 0
	}

	// Calculate slope using least squares
	var sumX, sumY, sumXY, sumX2 float64
	for i, sample := range recentSamples {
		x := float64(i)
		y := sample.FractionObstructed
		sumX += x
		sumY += y
		sumXY += x * y
		sumX2 += x * x
	}

	n64 := float64(len(recentSamples))
	slope := (n64*sumXY - sumX*sumY) / (n64*sumX2 - sumX*sumX)

	return slope
}

// calculateSNRTrend calculates the rate of change in SNR
func (op *ObstructionPredictor) calculateSNRTrend() float64 {
	if len(op.samples) < 3 {
		return 0
	}

	// Use linear regression to find slope of SNR over time
	n := len(op.samples)
	recentSamples := op.samples[max(0, n-20):] // Use last 20 samples for trend

	if len(recentSamples) < 3 {
		return 0
	}

	// Calculate slope using least squares
	var sumX, sumY, sumXY, sumX2 float64
	for i, sample := range recentSamples {
		x := float64(i)
		y := sample.SNR
		sumX += x
		sumY += y
		sumXY += x * y
		sumX2 += x * x
	}

	n64 := float64(len(recentSamples))
	slope := (n64*sumXY - sumX*sumY) / (n64*sumX2 - sumX*sumX)

	return slope
}

// predictFutureObstruction predicts obstruction in the next sample
func (op *ObstructionPredictor) predictFutureObstruction() float64 {
	if len(op.samples) == 0 {
		return 0
	}

	current := op.samples[len(op.samples)-1].FractionObstructed
	acceleration := op.calculateObstructionAcceleration()

	// Simple linear prediction
	predicted := current + acceleration

	// Clamp to valid range [0, 1]
	if predicted < 0 {
		predicted = 0
	}
	if predicted > 1 {
		predicted = 1
	}

	return predicted
}

// predictFutureSNR predicts SNR in the next sample
func (op *ObstructionPredictor) predictFutureSNR() float64 {
	if len(op.samples) == 0 {
		return 0
	}

	current := op.samples[len(op.samples)-1].SNR
	trend := op.calculateSNRTrend()

	// Simple linear prediction
	predicted := current + trend

	// Clamp to reasonable range [0, 30] dB
	if predicted < 0 {
		predicted = 0
	}
	if predicted > 30 {
		predicted = 30
	}

	return predicted
}

// calculateConfidence calculates confidence in the prediction
func (op *ObstructionPredictor) calculateConfidence() float64 {
	if len(op.samples) < op.minSamplesForAnalysis {
		return 0
	}

	confidence := 0.0

	// Factor 1: Data quality (ValidS and PatchesValid)
	recent := op.samples[len(op.samples)-1]
	if recent.ValidS > 0 && recent.PatchesValid > 0 {
		dataQuality := math.Min(float64(recent.ValidS)/300.0, 1.0) * // Normalize to 5 minutes
			math.Min(float64(recent.PatchesValid)/100.0, 1.0) // Normalize patches
		confidence += dataQuality * 0.4 // 40% weight for data quality
	}

	// Factor 2: Sample count (more samples = higher confidence)
	sampleConfidence := math.Min(float64(len(op.samples))/float64(op.maxSamples), 1.0)
	confidence += sampleConfidence * 0.3 // 30% weight for sample count

	// Factor 3: Trend consistency (lower variance = higher confidence)
	trendConsistency := op.calculateTrendConsistency()
	confidence += trendConsistency * 0.3 // 30% weight for trend consistency

	return math.Min(confidence, 1.0)
}

// calculateTrendConsistency measures how consistent the trend is
func (op *ObstructionPredictor) calculateTrendConsistency() float64 {
	if len(op.samples) < 5 {
		return 0
	}

	// Calculate variance in obstruction changes
	var changes []float64
	for i := 1; i < len(op.samples); i++ {
		change := op.samples[i].FractionObstructed - op.samples[i-1].FractionObstructed
		changes = append(changes, change)
	}

	if len(changes) == 0 {
		return 0
	}

	// Calculate mean and variance
	var sum float64
	for _, change := range changes {
		sum += change
	}
	mean := sum / float64(len(changes))

	var variance float64
	for _, change := range changes {
		variance += (change - mean) * (change - mean)
	}
	variance /= float64(len(changes))

	// Lower variance = higher consistency
	// Use inverse exponential to convert variance to consistency score
	consistency := math.Exp(-variance * 100) // Scale factor for sensitivity

	return consistency
}

// determineTrendDirection determines if the trend is improving, degrading, or stable
func (op *ObstructionPredictor) determineTrendDirection(obstructionAccel, snrTrend float64) string {
	// Thresholds for determining trend direction
	const stableThreshold = 0.001

	if obstructionAccel > stableThreshold || snrTrend < -stableThreshold {
		return "degrading"
	} else if obstructionAccel < -stableThreshold || snrTrend > stableThreshold {
		return "improving"
	}

	return "stable"
}

// estimateTimeToFailure estimates time until complete failure if trend continues
func (op *ObstructionPredictor) estimateTimeToFailure(obstructionAccel, snrTrend float64) *time.Duration {
	if len(op.samples) == 0 {
		return nil
	}

	current := op.samples[len(op.samples)-1]

	// Estimate time to reach critical thresholds
	var timeToObstructionFailure, timeToSNRFailure *time.Duration

	// Time to critical obstruction
	if obstructionAccel > 0 {
		remainingObstruction := op.criticalObstructionThreshold - current.FractionObstructed
		if remainingObstruction > 0 {
			samplesUntilFailure := remainingObstruction / obstructionAccel
			duration := time.Duration(samplesUntilFailure) * time.Second // Assuming 1s intervals
			timeToObstructionFailure = &duration
		}
	}

	// Time to critical SNR
	if snrTrend < 0 {
		remainingSNR := current.SNR - op.criticalSNRThreshold
		if remainingSNR > 0 {
			samplesUntilFailure := remainingSNR / (-snrTrend)
			duration := time.Duration(samplesUntilFailure) * time.Second // Assuming 1s intervals
			timeToSNRFailure = &duration
		}
	}

	// Return the shorter time to failure
	if timeToObstructionFailure != nil && timeToSNRFailure != nil {
		if *timeToObstructionFailure < *timeToSNRFailure {
			return timeToObstructionFailure
		}
		return timeToSNRFailure
	} else if timeToObstructionFailure != nil {
		return timeToObstructionFailure
	} else if timeToSNRFailure != nil {
		return timeToSNRFailure
	}

	return nil
}

// GetStatus returns current predictor status
func (op *ObstructionPredictor) GetStatus() map[string]interface{} {
	op.mu.RLock()
	defer op.mu.RUnlock()

	status := map[string]interface{}{
		"samples_count":                  len(op.samples),
		"max_samples":                    op.maxSamples,
		"min_samples_for_analysis":       op.minSamplesForAnalysis,
		"critical_obstruction_threshold": op.criticalObstructionThreshold,
		"critical_snr_threshold":         op.criticalSNRThreshold,
		"acceleration_threshold":         op.accelerationThreshold,
	}

	if len(op.samples) > 0 {
		latest := op.samples[len(op.samples)-1]
		status["latest_sample"] = map[string]interface{}{
			"timestamp":            latest.Timestamp,
			"currently_obstructed": latest.CurrentlyObstructed,
			"fraction_obstructed":  latest.FractionObstructed,
			"snr":                  latest.SNR,
			"time_obstructed":      latest.TimeObstructed,
		}
	}

	return status
}

// Helper function for max
func max(a, b int) int {
	if a > b {
		return a
	}
	return b
}
