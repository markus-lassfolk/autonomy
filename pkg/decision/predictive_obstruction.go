package decision

import (
	"fmt"
	"math"
	"sort"
	"time"

	"github.com/markus-lassfolk/autonomy/pkg/logx"
)

// PredictiveObstructionManager handles predictive obstruction detection and management
type PredictiveObstructionManager struct {
	logger             *logx.Logger
	config             *PredictiveObstructionConfig
	obstructionHistory []*ObstructionSample
	snrHistory         []*SNRSample
	lastPrediction     *ObstructionPrediction
}

// PredictiveObstructionConfig represents predictive obstruction configuration
type PredictiveObstructionConfig struct {
	Enabled                          bool    `json:"enabled"`
	HistoryWindowMinutes             int     `json:"history_window_minutes"`             // History window for analysis
	MinSamplesForPrediction          int     `json:"min_samples_for_prediction"`         // Minimum samples needed
	ObstructionAccelerationThreshold float64 `json:"obstruction_acceleration_threshold"` // Acceleration threshold
	SNRTrendThreshold                float64 `json:"snr_trend_threshold"`                // SNR decline threshold
	PredictionConfidenceThreshold    float64 `json:"prediction_confidence_threshold"`    // Minimum confidence for action
	ProactiveFailoverEnabled         bool    `json:"proactive_failover_enabled"`         // Enable proactive failover
	FalsePositiveReduction           bool    `json:"false_positive_reduction"`           // Enable false positive reduction
	DataQualityValidation            bool    `json:"data_quality_validation"`            // Enable data quality checks
	EnvironmentalLearning            bool    `json:"environmental_learning"`             // Enable environmental pattern learning
	MovementTriggeredReset           bool    `json:"movement_triggered_reset"`           // Reset on movement
	PredictionUpdateIntervalS        int     `json:"prediction_update_interval_s"`       // How often to update predictions
}

// ObstructionSample represents an obstruction measurement sample
type ObstructionSample struct {
	Timestamp           time.Time `json:"timestamp"`
	FractionObstructed  float64   `json:"fraction_obstructed"`
	TimeObstructed      float64   `json:"time_obstructed"`
	ValidDurationS      int64     `json:"valid_duration_s"`
	PatchesValid        int       `json:"patches_valid"`
	CurrentlyObstructed bool      `json:"currently_obstructed"`
	DataQuality         string    `json:"data_quality"` // good|poor|insufficient
	ProlongedIntervalS  float64   `json:"prolonged_interval_s"`
}

// SNRSample represents a signal-to-noise ratio sample
type SNRSample struct {
	Timestamp           time.Time `json:"timestamp"`
	SNR                 float64   `json:"snr"`
	IsAboveNoiseFloor   bool      `json:"is_above_noise_floor"`
	IsPersistentlyLow   bool      `json:"is_persistently_low"`
	SecondsSinceLastSNR int       `json:"seconds_since_last_snr"`
}

// ObstructionPrediction represents a prediction about future obstruction
type ObstructionPrediction struct {
	Timestamp               time.Time `json:"timestamp"`
	PredictedIssue          string    `json:"predicted_issue"`          // Type of predicted issue
	TimeToIssue             int       `json:"time_to_issue_seconds"`    // Seconds until predicted issue
	Confidence              float64   `json:"confidence"`               // Confidence level (0-1)
	ObstructionSlope        float64   `json:"obstruction_slope"`        // Rate of obstruction change
	ObstructionAcceleration float64   `json:"obstruction_acceleration"` // Acceleration of obstruction
	SNRTrend                float64   `json:"snr_trend"`                // SNR trend
	TriggerReasons          []string  `json:"trigger_reasons"`          // Reasons for prediction
	RecommendedAction       string    `json:"recommended_action"`       // Recommended action
	FalsePositiveRisk       float64   `json:"false_positive_risk"`      // Risk of false positive
	DataQualityScore        float64   `json:"data_quality_score"`       // Quality of underlying data
}

// NewPredictiveObstructionManager creates a new predictive obstruction manager
func NewPredictiveObstructionManager(config *PredictiveObstructionConfig, logger *logx.Logger) *PredictiveObstructionManager {
	if config == nil {
		config = DefaultPredictiveObstructionConfig()
	}

	return &PredictiveObstructionManager{
		logger:             logger,
		config:             config,
		obstructionHistory: make([]*ObstructionSample, 0),
		snrHistory:         make([]*SNRSample, 0),
	}
}

// DefaultPredictiveObstructionConfig returns default predictive obstruction configuration
func DefaultPredictiveObstructionConfig() *PredictiveObstructionConfig {
	return &PredictiveObstructionConfig{
		Enabled:                          true,
		HistoryWindowMinutes:             30,   // 30 minutes of history
		MinSamplesForPrediction:          10,   // At least 10 samples
		ObstructionAccelerationThreshold: 0.5,  // 0.5% per minute acceleration
		SNRTrendThreshold:                -1.0, // 1 dB per minute decline
		PredictionConfidenceThreshold:    0.7,  // 70% confidence threshold
		ProactiveFailoverEnabled:         true,
		FalsePositiveReduction:           true,
		DataQualityValidation:            true,
		EnvironmentalLearning:            true,
		MovementTriggeredReset:           true,
		PredictionUpdateIntervalS:        60, // Update every minute
	}
}

// AddObstructionSample adds a new obstruction sample for analysis
func (pom *PredictiveObstructionManager) AddObstructionSample(sample *ObstructionSample) error {
	if !pom.config.Enabled {
		return nil
	}

	// Validate sample
	if err := pom.validateObstructionSample(sample); err != nil {
		return fmt.Errorf("invalid obstruction sample: %w", err)
	}

	// Add to history
	pom.obstructionHistory = append(pom.obstructionHistory, sample)

	// Maintain history window
	pom.maintainHistoryWindow()

	// Log sample addition
	pom.logger.LogDataFlow("predictive_obstruction", "sample_added", "obstruction", 1, map[string]interface{}{
		"fraction_obstructed":  sample.FractionObstructed,
		"time_obstructed":      sample.TimeObstructed,
		"currently_obstructed": sample.CurrentlyObstructed,
		"data_quality":         sample.DataQuality,
		"prolonged_interval_s": sample.ProlongedIntervalS,
		"patches_valid":        sample.PatchesValid,
	})

	return nil
}

// AddSNRSample adds a new SNR sample for analysis
func (pom *PredictiveObstructionManager) AddSNRSample(sample *SNRSample) error {
	if !pom.config.Enabled {
		return nil
	}

	// Add to history
	pom.snrHistory = append(pom.snrHistory, sample)

	// Maintain history window
	pom.maintainSNRHistoryWindow()

	// Log sample addition
	pom.logger.LogDataFlow("predictive_obstruction", "sample_added", "snr", 1, map[string]interface{}{
		"snr":                    sample.SNR,
		"is_above_noise_floor":   sample.IsAboveNoiseFloor,
		"is_persistently_low":    sample.IsPersistentlyLow,
		"seconds_since_last_snr": sample.SecondsSinceLastSNR,
	})

	return nil
}

// AnalyzePredictiveObstruction performs comprehensive predictive analysis
func (pom *PredictiveObstructionManager) AnalyzePredictiveObstruction() (*ObstructionPrediction, error) {
	if !pom.config.Enabled {
		return nil, nil
	}

	if len(pom.obstructionHistory) < pom.config.MinSamplesForPrediction {
		return nil, fmt.Errorf("insufficient samples for prediction: %d < %d",
			len(pom.obstructionHistory), pom.config.MinSamplesForPrediction)
	}

	prediction := &ObstructionPrediction{
		Timestamp:         time.Now(),
		TriggerReasons:    []string{},
		RecommendedAction: "monitor",
	}

	// Calculate obstruction trends
	obstructionSlope, obstructionAcceleration := pom.calculateObstructionTrends()
	prediction.ObstructionSlope = obstructionSlope
	prediction.ObstructionAcceleration = obstructionAcceleration

	// Calculate SNR trends
	snrTrend := pom.calculateSNRTrend()
	prediction.SNRTrend = snrTrend

	// Analyze for different types of predicted issues
	pom.analyzeObstructionAcceleration(prediction)
	pom.analyzeSNRDegradation(prediction)
	pom.analyzeDataQuality(prediction)
	pom.analyzeFalsePositiveRisk(prediction)

	// Apply environmental learning patterns
	if pom.config.EnvironmentalLearning {
		pom.applyEnvironmentalPatterns(prediction)
	}

	// Calculate overall confidence and recommendation
	pom.calculateOverallPrediction(prediction)

	// Store prediction
	pom.lastPrediction = prediction

	// Log prediction
	pom.logger.LogVerbose("obstruction_prediction_generated", map[string]interface{}{
		"predicted_issue":          prediction.PredictedIssue,
		"time_to_issue_seconds":    prediction.TimeToIssue,
		"confidence":               prediction.Confidence,
		"obstruction_slope":        prediction.ObstructionSlope,
		"obstruction_acceleration": prediction.ObstructionAcceleration,
		"snr_trend":                prediction.SNRTrend,
		"trigger_reasons":          prediction.TriggerReasons,
		"recommended_action":       prediction.RecommendedAction,
		"false_positive_risk":      prediction.FalsePositiveRisk,
		"data_quality_score":       prediction.DataQualityScore,
	})

	return prediction, nil
}

// ShouldTriggerProactiveFailover determines if proactive failover should be triggered
func (pom *PredictiveObstructionManager) ShouldTriggerProactiveFailover() (bool, *ObstructionPrediction, string) {
	if !pom.config.Enabled || !pom.config.ProactiveFailoverEnabled {
		return false, nil, "disabled"
	}

	if pom.lastPrediction == nil {
		return false, nil, "no_prediction"
	}

	// Check confidence threshold
	if pom.lastPrediction.Confidence < pom.config.PredictionConfidenceThreshold {
		return false, pom.lastPrediction, "confidence_too_low"
	}

	// Check false positive risk
	if pom.config.FalsePositiveReduction && pom.lastPrediction.FalsePositiveRisk > 0.3 {
		return false, pom.lastPrediction, "high_false_positive_risk"
	}

	// Check for critical predictions
	criticalIssues := []string{"rapid_obstruction_increase", "snr_critical_decline", "imminent_signal_loss"}
	for _, issue := range criticalIssues {
		if pom.lastPrediction.PredictedIssue == issue {
			return true, pom.lastPrediction, "critical_issue_predicted"
		}
	}

	// Check time to issue threshold
	if pom.lastPrediction.TimeToIssue > 0 && pom.lastPrediction.TimeToIssue < 300 { // 5 minutes
		return true, pom.lastPrediction, "imminent_issue"
	}

	return false, pom.lastPrediction, "no_trigger_conditions_met"
}

// validateObstructionSample validates an obstruction sample
func (pom *PredictiveObstructionManager) validateObstructionSample(sample *ObstructionSample) error {
	if sample == nil {
		return fmt.Errorf("sample is nil")
	}

	if sample.FractionObstructed < 0 || sample.FractionObstructed > 1 {
		return fmt.Errorf("invalid fraction obstructed: %f", sample.FractionObstructed)
	}

	if pom.config.DataQualityValidation {
		// Check data quality indicators
		if sample.ValidDurationS < 30 { // Less than 30 seconds of valid data
			sample.DataQuality = "insufficient"
		} else if sample.PatchesValid < 10 { // Less than 10 valid patches
			sample.DataQuality = "poor"
		} else {
			sample.DataQuality = "good"
		}
	}

	return nil
}

// maintainHistoryWindow maintains the obstruction history window
func (pom *PredictiveObstructionManager) maintainHistoryWindow() {
	cutoff := time.Now().Add(-time.Duration(pom.config.HistoryWindowMinutes) * time.Minute)

	var validSamples []*ObstructionSample
	for _, sample := range pom.obstructionHistory {
		if sample.Timestamp.After(cutoff) {
			validSamples = append(validSamples, sample)
		}
	}

	pom.obstructionHistory = validSamples
}

// maintainSNRHistoryWindow maintains the SNR history window
func (pom *PredictiveObstructionManager) maintainSNRHistoryWindow() {
	cutoff := time.Now().Add(-time.Duration(pom.config.HistoryWindowMinutes) * time.Minute)

	var validSamples []*SNRSample
	for _, sample := range pom.snrHistory {
		if sample.Timestamp.After(cutoff) {
			validSamples = append(validSamples, sample)
		}
	}

	pom.snrHistory = validSamples
}

// calculateObstructionTrends calculates obstruction slope and acceleration
func (pom *PredictiveObstructionManager) calculateObstructionTrends() (slope, acceleration float64) {
	if len(pom.obstructionHistory) < 3 {
		return 0, 0
	}

	// Sort by timestamp
	samples := make([]*ObstructionSample, len(pom.obstructionHistory))
	copy(samples, pom.obstructionHistory)
	sort.Slice(samples, func(i, j int) bool {
		return samples[i].Timestamp.Before(samples[j].Timestamp)
	})

	// Calculate linear regression for slope
	n := float64(len(samples))
	var sumX, sumY, sumXY, sumX2 float64

	for i, sample := range samples {
		x := float64(i)
		y := sample.FractionObstructed * 100 // Convert to percentage

		sumX += x
		sumY += y
		sumXY += x * y
		sumX2 += x * x
	}

	// Linear regression slope calculation
	slope = (n*sumXY - sumX*sumY) / (n*sumX2 - sumX*sumX)

	// Calculate acceleration (change in slope over time)
	if len(samples) >= 6 { // Need at least 6 points for acceleration
		mid := len(samples) / 2

		// Calculate slope for first half
		firstHalf := samples[:mid]
		firstSlope := pom.calculateSlopeForSamples(firstHalf)

		// Calculate slope for second half
		secondHalf := samples[mid:]
		secondSlope := pom.calculateSlopeForSamples(secondHalf)

		// Acceleration is change in slope over time
		timeSpan := samples[len(samples)-1].Timestamp.Sub(samples[0].Timestamp).Minutes()
		if timeSpan > 0 {
			acceleration = (secondSlope - firstSlope) / timeSpan
		}
	}

	return slope, acceleration
}

// calculateSlopeForSamples calculates slope for a subset of samples
func (pom *PredictiveObstructionManager) calculateSlopeForSamples(samples []*ObstructionSample) float64 {
	if len(samples) < 2 {
		return 0
	}

	n := float64(len(samples))
	var sumX, sumY, sumXY, sumX2 float64

	for i, sample := range samples {
		x := float64(i)
		y := sample.FractionObstructed * 100

		sumX += x
		sumY += y
		sumXY += x * y
		sumX2 += x * x
	}

	return (n*sumXY - sumX*sumY) / (n*sumX2 - sumX*sumX)
}

// calculateSNRTrend calculates SNR trend
func (pom *PredictiveObstructionManager) calculateSNRTrend() float64 {
	if len(pom.snrHistory) < 3 {
		return 0
	}

	// Sort by timestamp
	samples := make([]*SNRSample, len(pom.snrHistory))
	copy(samples, pom.snrHistory)
	sort.Slice(samples, func(i, j int) bool {
		return samples[i].Timestamp.Before(samples[j].Timestamp)
	})

	// Calculate linear regression for SNR trend
	n := float64(len(samples))
	var sumX, sumY, sumXY, sumX2 float64

	for i, sample := range samples {
		x := float64(i)
		y := sample.SNR

		sumX += x
		sumY += y
		sumXY += x * y
		sumX2 += x * x
	}

	return (n*sumXY - sumX*sumY) / (n*sumX2 - sumX*sumX)
}

// analyzeObstructionAcceleration analyzes obstruction acceleration patterns
func (pom *PredictiveObstructionManager) analyzeObstructionAcceleration(prediction *ObstructionPrediction) {
	if math.Abs(prediction.ObstructionAcceleration) > pom.config.ObstructionAccelerationThreshold {
		if prediction.ObstructionAcceleration > 0 {
			prediction.PredictedIssue = "rapid_obstruction_increase"
			prediction.TriggerReasons = append(prediction.TriggerReasons, "obstruction_acceleration_detected")

			// Estimate time to critical obstruction (>50%)
			currentObstruction := pom.getCurrentObstructionLevel()
			if prediction.ObstructionSlope > 0 {
				timeToFiftyPercent := (50 - currentObstruction) / prediction.ObstructionSlope
				if timeToFiftyPercent > 0 {
					prediction.TimeToIssue = int(timeToFiftyPercent * 60) // Convert minutes to seconds
				}
			}

			prediction.RecommendedAction = "proactive_failover"
		}
	}
}

// analyzeSNRDegradation analyzes SNR degradation patterns
func (pom *PredictiveObstructionManager) analyzeSNRDegradation(prediction *ObstructionPrediction) {
	if prediction.SNRTrend < pom.config.SNRTrendThreshold {
		prediction.PredictedIssue = "snr_critical_decline"
		prediction.TriggerReasons = append(prediction.TriggerReasons, "snr_degradation_trend")

		// Check for persistently low SNR in recent samples
		recentLowSNRCount := 0
		for i := len(pom.snrHistory) - 1; i >= 0 && i >= len(pom.snrHistory)-5; i-- {
			if pom.snrHistory[i].IsPersistentlyLow || !pom.snrHistory[i].IsAboveNoiseFloor {
				recentLowSNRCount++
			}
		}

		if recentLowSNRCount >= 3 {
			prediction.PredictedIssue = "imminent_signal_loss"
			prediction.TimeToIssue = 120 // 2 minutes
			prediction.RecommendedAction = "immediate_failover"
		}
	}
}

// analyzeDataQuality analyzes data quality for prediction reliability
func (pom *PredictiveObstructionManager) analyzeDataQuality(prediction *ObstructionPrediction) {
	if !pom.config.DataQualityValidation {
		prediction.DataQualityScore = 1.0
		return
	}

	goodSamples := 0
	totalSamples := len(pom.obstructionHistory)

	for _, sample := range pom.obstructionHistory {
		if sample.DataQuality == "good" {
			goodSamples++
		}
	}

	if totalSamples > 0 {
		prediction.DataQualityScore = float64(goodSamples) / float64(totalSamples)
	}

	// Adjust confidence based on data quality
	if prediction.DataQualityScore < 0.5 {
		prediction.TriggerReasons = append(prediction.TriggerReasons, "poor_data_quality")
	}
}

// analyzeFalsePositiveRisk analyzes risk of false positive predictions
func (pom *PredictiveObstructionManager) analyzeFalsePositiveRisk(prediction *ObstructionPrediction) {
	if !pom.config.FalsePositiveReduction {
		prediction.FalsePositiveRisk = 0.0
		return
	}

	riskFactors := 0.0

	// Check for rapid changes that might be temporary
	if len(pom.obstructionHistory) >= 3 {
		recent := pom.obstructionHistory[len(pom.obstructionHistory)-3:]
		variance := pom.calculateVariance(recent)
		if variance > 0.1 { // High variance indicates instability
			riskFactors += 0.3
		}
	}

	// Check time-of-day patterns (morning/evening obstructions are often temporary)
	hour := time.Now().Hour()
	if hour >= 6 && hour <= 9 || hour >= 17 && hour <= 20 {
		riskFactors += 0.2
	}

	// Check for very short duration predictions
	if prediction.TimeToIssue > 0 && prediction.TimeToIssue < 60 {
		riskFactors += 0.2
	}

	prediction.FalsePositiveRisk = math.Min(riskFactors, 1.0)
}

// applyEnvironmentalPatterns applies environmental learning patterns
func (pom *PredictiveObstructionManager) applyEnvironmentalPatterns(prediction *ObstructionPrediction) {
	// This is a simplified version - in production, this would use ML models
	// trained on historical data to recognize environmental patterns

	// Check for weather-related patterns (simplified)
	hour := time.Now().Hour()
	if hour >= 6 && hour <= 8 { // Morning sun angle
		prediction.TriggerReasons = append(prediction.TriggerReasons, "morning_sun_pattern")
	}

	// Check for seasonal patterns (simplified)
	month := time.Now().Month()
	if month >= 11 || month <= 2 { // Winter months
		prediction.TriggerReasons = append(prediction.TriggerReasons, "winter_weather_pattern")
	}
}

// calculateOverallPrediction calculates overall prediction confidence and recommendation
func (pom *PredictiveObstructionManager) calculateOverallPrediction(prediction *ObstructionPrediction) {
	// Base confidence on number of trigger reasons
	baseConfidence := float64(len(prediction.TriggerReasons)) * 0.2

	// Adjust for data quality
	confidence := baseConfidence * prediction.DataQualityScore

	// Reduce confidence for high false positive risk
	confidence = confidence * (1.0 - prediction.FalsePositiveRisk)

	// Boost confidence for multiple concurrent indicators
	if prediction.ObstructionAcceleration > pom.config.ObstructionAccelerationThreshold &&
		prediction.SNRTrend < pom.config.SNRTrendThreshold {
		confidence += 0.3
	}

	prediction.Confidence = math.Min(confidence, 1.0)

	// Set final recommendation based on confidence and issue severity
	if prediction.Confidence >= pom.config.PredictionConfidenceThreshold {
		if prediction.PredictedIssue == "imminent_signal_loss" {
			prediction.RecommendedAction = "immediate_failover"
		} else if prediction.PredictedIssue == "rapid_obstruction_increase" ||
			prediction.PredictedIssue == "snr_critical_decline" {
			prediction.RecommendedAction = "proactive_failover"
		}
	}
}

// getCurrentObstructionLevel gets the current obstruction level
func (pom *PredictiveObstructionManager) getCurrentObstructionLevel() float64 {
	if len(pom.obstructionHistory) == 0 {
		return 0
	}

	// Get most recent sample
	latest := pom.obstructionHistory[len(pom.obstructionHistory)-1]
	return latest.FractionObstructed * 100
}

// calculateVariance calculates variance in obstruction samples
func (pom *PredictiveObstructionManager) calculateVariance(samples []*ObstructionSample) float64 {
	if len(samples) < 2 {
		return 0
	}

	// Calculate mean
	var sum float64
	for _, sample := range samples {
		sum += sample.FractionObstructed
	}
	mean := sum / float64(len(samples))

	// Calculate variance
	var variance float64
	for _, sample := range samples {
		diff := sample.FractionObstructed - mean
		variance += diff * diff
	}

	return variance / float64(len(samples))
}

// GetPredictionStatus returns the current prediction status
func (pom *PredictiveObstructionManager) GetPredictionStatus() map[string]interface{} {
	status := map[string]interface{}{
		"enabled":              pom.config.Enabled,
		"obstruction_samples":  len(pom.obstructionHistory),
		"snr_samples":          len(pom.snrHistory),
		"min_samples_required": pom.config.MinSamplesForPrediction,
		"ready_for_prediction": len(pom.obstructionHistory) >= pom.config.MinSamplesForPrediction,
	}

	if pom.lastPrediction != nil {
		status["last_prediction"] = map[string]interface{}{
			"timestamp":             pom.lastPrediction.Timestamp,
			"predicted_issue":       pom.lastPrediction.PredictedIssue,
			"confidence":            pom.lastPrediction.Confidence,
			"time_to_issue_seconds": pom.lastPrediction.TimeToIssue,
			"recommended_action":    pom.lastPrediction.RecommendedAction,
			"false_positive_risk":   pom.lastPrediction.FalsePositiveRisk,
		}
	}

	return status
}

// ResetOnMovement resets obstruction data when movement is detected
func (pom *PredictiveObstructionManager) ResetOnMovement() {
	if pom.config.MovementTriggeredReset {
		pom.obstructionHistory = make([]*ObstructionSample, 0)
		pom.lastPrediction = nil

		pom.logger.LogStateChange("predictive_obstruction", "active", "reset", "movement_detected", map[string]interface{}{
			"reason": "movement_triggered_reset",
		})
	}
}
