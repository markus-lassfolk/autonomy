package predictive

import (
	"context"
	"fmt"
	"math"

	"github.com/markus-lassfolk/autonomy/pkg/logx"
	"github.com/markus-lassfolk/autonomy/pkg/obstruction"
)

// StarlinkSpecificPredictor handles Starlink-specific predictive logic
type StarlinkSpecificPredictor struct {
	logger               *logx.Logger
	obstructionPredictor *obstruction.ObstructionPredictor
	trendAnalyzer        *obstruction.TrendAnalyzer
	patternLearner       *obstruction.PatternLearner
	samples              []ConnectionSample
}

// StarlinkAnalysis represents Starlink-specific analysis results
type StarlinkAnalysis struct {
	ObstructionTrend     float64 `json:"obstruction_trend"`     // Rate of change in obstruction %
	SNRTrend             float64 `json:"snr_trend"`             // Rate of change in SNR
	PredictedObstruction float64 `json:"predicted_obstruction"` // Predicted obstruction %
	PredictedSNR         float64 `json:"predicted_snr"`         // Predicted SNR
	ThermalRisk          float64 `json:"thermal_risk"`          // Risk of thermal issues (0-1)
	RebootRisk           float64 `json:"reboot_risk"`           // Risk of pending reboot (0-1)
	ObstructionPatterns  int     `json:"obstruction_patterns"`  // Number of learned patterns
}

// NewStarlinkSpecificPredictor creates a new Starlink-specific predictor
func NewStarlinkSpecificPredictor(logger *logx.Logger) *StarlinkSpecificPredictor {
	return &StarlinkSpecificPredictor{
		logger:               logger,
		obstructionPredictor: obstruction.NewObstructionPredictor(logger, nil),
		trendAnalyzer:        obstruction.NewTrendAnalyzer(logger, nil),
		patternLearner:       obstruction.NewPatternLearner(logger, nil),
		samples:              make([]ConnectionSample, 0),
	}
}

// AddSample adds a new sample for Starlink-specific analysis
func (ssp *StarlinkSpecificPredictor) AddSample(ctx context.Context, sample *ConnectionSample) error {
	ssp.samples = append(ssp.samples, *sample)

	// Keep only recent samples (last 300)
	if len(ssp.samples) > 300 {
		ssp.samples = ssp.samples[1:]
	}

	// Add to obstruction predictor if we have obstruction data
	if sample.Metrics != nil {
		if err := ssp.obstructionPredictor.AddSample(ctx, sample.Metrics); err != nil {
			ssp.logger.Warn("Failed to add sample to obstruction predictor", "error", err)
		}

		// Add to trend analyzer
		if sample.Metrics.ObstructionPct != nil {
			ssp.trendAnalyzer.AddObstructionPoint(sample.Timestamp, *sample.Metrics.ObstructionPct/100.0, sample.Quality)
		}
		if sample.Metrics.SNR != nil {
			ssp.trendAnalyzer.AddSNRPoint(sample.Timestamp, float64(*sample.Metrics.SNR), sample.Quality)
		}
		if sample.Metrics.LatencyMS != nil {
			ssp.trendAnalyzer.AddLatencyPoint(sample.Timestamp, *sample.Metrics.LatencyMS, sample.Quality)
		}

		// Add to pattern learner if we have significant obstruction
		if sample.Metrics.ObstructionPct != nil && *sample.Metrics.ObstructionPct > 5.0 {
			obstructionSample := obstruction.ObstructionSample{
				Timestamp:           sample.Timestamp,
				CurrentlyObstructed: *sample.Metrics.ObstructionPct > 0,
				FractionObstructed:  *sample.Metrics.ObstructionPct / 100.0,
				SNR:                 0,
				TimeObstructed:      0,
				ValidS:              0,
				PatchesValid:        0,
			}

			if sample.Metrics.SNR != nil {
				obstructionSample.SNR = float64(*sample.Metrics.SNR)
			}
			if sample.Metrics.ObstructionTimePct != nil {
				obstructionSample.TimeObstructed = *sample.Metrics.ObstructionTimePct
			}
			if sample.Metrics.ObstructionValidS != nil {
				obstructionSample.ValidS = int(*sample.Metrics.ObstructionValidS)
			}
			if sample.Metrics.ObstructionPatchesValid != nil {
				obstructionSample.PatchesValid = *sample.Metrics.ObstructionPatchesValid
			}

			if err := ssp.patternLearner.AddObservation(ctx, obstructionSample); err != nil {
				ssp.logger.Warn("Failed to add observation to pattern learner", "error", err)
			}
		}
	}

	return nil
}

// GetAnalysis returns Starlink-specific analysis
func (ssp *StarlinkSpecificPredictor) GetAnalysis(ctx context.Context) *StarlinkAnalysis {
	analysis := &StarlinkAnalysis{}

	// Get obstruction trend analysis
	if obstructionTrend, err := ssp.trendAnalyzer.AnalyzeObstructionTrend(ctx); err == nil {
		analysis.ObstructionTrend = obstructionTrend.Slope
		if obstructionTrend.Prediction != nil {
			analysis.PredictedObstruction = obstructionTrend.Prediction.PredictedValue * 100 // Convert to percentage
		}
	}

	// Get SNR trend analysis
	if snrTrend, err := ssp.trendAnalyzer.AnalyzeSNRTrend(ctx); err == nil {
		analysis.SNRTrend = snrTrend.Slope
		if snrTrend.Prediction != nil {
			analysis.PredictedSNR = snrTrend.Prediction.PredictedValue
		}
	}

	// Calculate thermal risk
	analysis.ThermalRisk = ssp.calculateThermalRisk()

	// Calculate reboot risk
	analysis.RebootRisk = ssp.calculateRebootRisk()

	// Get pattern count
	patterns := ssp.patternLearner.GetPatterns()
	analysis.ObstructionPatterns = len(patterns)

	return analysis
}

// ShouldTriggerFailover checks Starlink-specific failover triggers
func (ssp *StarlinkSpecificPredictor) ShouldTriggerFailover(ctx context.Context) (bool, string) {
	// Check obstruction predictor
	if shouldTrigger, reason, err := ssp.obstructionPredictor.ShouldTriggerFailover(ctx); err == nil && shouldTrigger {
		return true, fmt.Sprintf("obstruction prediction: %s", reason)
	}

	// Check thermal issues
	if ssp.calculateThermalRisk() > 0.8 {
		return true, "high thermal risk detected"
	}

	// Check pending reboot
	if ssp.calculateRebootRisk() > 0.9 {
		return true, "imminent reboot detected"
	}

	// Check for rapid SNR degradation
	if len(ssp.samples) >= 3 {
		recent := ssp.samples[len(ssp.samples)-3:]
		snrDegradation := ssp.calculateSNRDegradation(recent)
		if snrDegradation > 2.0 { // Losing more than 2 dB per sample
			return true, fmt.Sprintf("rapid SNR degradation: %.1f dB/sample", snrDegradation)
		}
	}

	// Check for obstruction acceleration
	if len(ssp.samples) >= 3 {
		recent := ssp.samples[len(ssp.samples)-3:]
		obstructionAccel := ssp.calculateObstructionAcceleration(recent)
		if obstructionAccel > 5.0 { // More than 5% increase per sample
			return true, fmt.Sprintf("rapid obstruction increase: %.1f%%/sample", obstructionAccel)
		}
	}

	return false, "no Starlink-specific triggers"
}

// calculateThermalRisk calculates risk based on thermal conditions
func (ssp *StarlinkSpecificPredictor) calculateThermalRisk() float64 {
	if len(ssp.samples) == 0 {
		return 0
	}

	latest := ssp.samples[len(ssp.samples)-1]
	if latest.Metrics == nil {
		return 0
	}

	risk := 0.0

	// Check thermal throttling
	if latest.Metrics.ThermalThrottle != nil && *latest.Metrics.ThermalThrottle {
		risk = math.Max(risk, 0.7) // High risk if already throttling
	}

	// Check thermal shutdown warning
	if latest.Metrics.ThermalShutdown != nil && *latest.Metrics.ThermalShutdown {
		risk = math.Max(risk, 0.95) // Very high risk if shutdown imminent
	}

	return risk
}

// calculateRebootRisk calculates risk based on pending reboots
func (ssp *StarlinkSpecificPredictor) calculateRebootRisk() float64 {
	if len(ssp.samples) == 0 {
		return 0
	}

	latest := ssp.samples[len(ssp.samples)-1]
	if latest.Metrics == nil {
		return 0
	}

	risk := 0.0

	// Check software update reboot ready
	if latest.Metrics.SwupdateRebootReady != nil && *latest.Metrics.SwupdateRebootReady {
		risk = math.Max(risk, 0.9) // Very high risk if reboot is ready
	}

	// Check scheduled reboot
	if latest.Metrics.RebootScheduledUTC != nil && *latest.Metrics.RebootScheduledUTC != "" {
		risk = math.Max(risk, 0.8) // High risk if reboot is scheduled
	}

	return risk
}

// calculateSNRDegradation calculates the rate of SNR degradation
func (ssp *StarlinkSpecificPredictor) calculateSNRDegradation(samples []ConnectionSample) float64 {
	if len(samples) < 2 {
		return 0
	}

	var snrValues []float64
	for _, sample := range samples {
		if sample.Metrics != nil && sample.Metrics.SNR != nil {
			snrValues = append(snrValues, float64(*sample.Metrics.SNR))
		}
	}

	if len(snrValues) < 2 {
		return 0
	}

	// Calculate average rate of change
	totalChange := 0.0
	for i := 1; i < len(snrValues); i++ {
		change := snrValues[i] - snrValues[i-1]
		totalChange += change
	}

	return -totalChange / float64(len(snrValues)-1) // Negative because degradation is negative change
}

// calculateObstructionAcceleration calculates the rate of obstruction increase
func (ssp *StarlinkSpecificPredictor) calculateObstructionAcceleration(samples []ConnectionSample) float64 {
	if len(samples) < 2 {
		return 0
	}

	var obstructionValues []float64
	for _, sample := range samples {
		if sample.Metrics != nil && sample.Metrics.ObstructionPct != nil {
			obstructionValues = append(obstructionValues, *sample.Metrics.ObstructionPct)
		}
	}

	if len(obstructionValues) < 2 {
		return 0
	}

	// Calculate average rate of change
	totalChange := 0.0
	for i := 1; i < len(obstructionValues); i++ {
		change := obstructionValues[i] - obstructionValues[i-1]
		if change > 0 { // Only count increases
			totalChange += change
		}
	}

	return totalChange / float64(len(obstructionValues)-1)
}

// GetStatus returns Starlink-specific predictor status
func (ssp *StarlinkSpecificPredictor) GetStatus() map[string]interface{} {
	status := map[string]interface{}{
		"samples_count": len(ssp.samples),
	}

	if ssp.obstructionPredictor != nil {
		status["obstruction_predictor"] = ssp.obstructionPredictor.GetStatus()
	}

	if ssp.trendAnalyzer != nil {
		status["trend_analyzer"] = ssp.trendAnalyzer.GetStatus()
	}

	if ssp.patternLearner != nil {
		status["pattern_learner"] = ssp.patternLearner.GetStatus()
	}

	return status
}
