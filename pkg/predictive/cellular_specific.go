package predictive

import (
	"context"
	"fmt"
	"math"

	"github.com/markus-lassfolk/autonomy/pkg/logx"
)

// CellularSpecificPredictor handles cellular-specific predictive logic
type CellularSpecificPredictor struct {
	logger  *logx.Logger
	samples []ConnectionSample
}

// CellularAnalysis represents cellular-specific analysis results
type CellularAnalysis struct {
	RSRPTrend      float64 `json:"rsrp_trend"`      // Rate of change in RSRP (dBm/sample)
	RSRQTrend      float64 `json:"rsrq_trend"`      // Rate of change in RSRQ (dB/sample)
	SINRTrend      float64 `json:"sinr_trend"`      // Rate of change in SINR (dB/sample)
	PredictedRSRP  float64 `json:"predicted_rsrp"`  // Predicted RSRP (dBm)
	PredictedRSRQ  float64 `json:"predicted_rsrq"`  // Predicted RSRQ (dB)
	PredictedSINR  float64 `json:"predicted_sinr"`  // Predicted SINR (dB)
	HandoverRisk   float64 `json:"handover_risk"`   // Risk of cell handover (0-1)
	StabilityScore float64 `json:"stability_score"` // Current stability score (0-100)
	CellChanges    int     `json:"cell_changes"`    // Recent cell changes
	SignalVariance float64 `json:"signal_variance"` // Signal strength variance
	RoamingRisk    float64 `json:"roaming_risk"`    // Risk of entering roaming (0-1)
}

// NewCellularSpecificPredictor creates a new cellular-specific predictor
func NewCellularSpecificPredictor(logger *logx.Logger) *CellularSpecificPredictor {
	return &CellularSpecificPredictor{
		logger:  logger,
		samples: make([]ConnectionSample, 0),
	}
}

// AddSample adds a new sample for cellular-specific analysis
func (csp *CellularSpecificPredictor) AddSample(ctx context.Context, sample *ConnectionSample) error {
	csp.samples = append(csp.samples, *sample)

	// Keep only recent samples (last 300)
	if len(csp.samples) > 300 {
		csp.samples = csp.samples[1:]
	}

	return nil
}

// GetAnalysis returns cellular-specific analysis
func (csp *CellularSpecificPredictor) GetAnalysis(ctx context.Context) *CellularAnalysis {
	analysis := &CellularAnalysis{}

	if len(csp.samples) == 0 {
		return analysis
	}

	// Calculate signal trends
	analysis.RSRPTrend = csp.calculateRSRPTrend()
	analysis.RSRQTrend = csp.calculateRSRQTrend()
	analysis.SINRTrend = csp.calculateSINRTrend()

	// Predict future values
	analysis.PredictedRSRP = csp.predictFutureRSRP()
	analysis.PredictedRSRQ = csp.predictFutureRSRQ()
	analysis.PredictedSINR = csp.predictFutureSINR()

	// Calculate risks
	analysis.HandoverRisk = csp.calculateHandoverRisk()
	analysis.RoamingRisk = csp.calculateRoamingRisk()

	// Get current metrics
	latest := csp.samples[len(csp.samples)-1]
	if latest.Metrics != nil {
		if latest.Metrics.StabilityScore != nil {
			analysis.StabilityScore = *latest.Metrics.StabilityScore
		}
		if latest.Metrics.CellChanges != nil {
			analysis.CellChanges = *latest.Metrics.CellChanges
		}
		if latest.Metrics.SignalVariance != nil {
			analysis.SignalVariance = *latest.Metrics.SignalVariance
		}
	}

	return analysis
}

// ShouldTriggerFailover checks cellular-specific failover triggers
func (csp *CellularSpecificPredictor) ShouldTriggerFailover(ctx context.Context) (bool, string) {
	if len(csp.samples) == 0 {
		return false, "no cellular samples available"
	}

	latest := csp.samples[len(csp.samples)-1]
	if latest.Metrics == nil {
		return false, "no metrics in latest sample"
	}

	// Check stability score
	if latest.Metrics.StabilityScore != nil && *latest.Metrics.StabilityScore < 30 {
		return true, fmt.Sprintf("low stability score: %.1f", *latest.Metrics.StabilityScore)
	}

	// Check for rapid RSRP degradation
	rsrpTrend := csp.calculateRSRPTrend()
	if rsrpTrend < -2.0 { // Losing more than 2 dBm per sample
		return true, fmt.Sprintf("rapid RSRP degradation: %.1f dBm/sample", rsrpTrend)
	}

	// Check predicted RSRP
	predictedRSRP := csp.predictFutureRSRP()
	if predictedRSRP < -120 { // Very weak signal predicted
		return true, fmt.Sprintf("predicted weak RSRP: %.1f dBm", predictedRSRP)
	}

	// Check for frequent cell changes
	if latest.Metrics.CellChanges != nil && *latest.Metrics.CellChanges > 3 {
		return true, fmt.Sprintf("frequent cell changes: %d", *latest.Metrics.CellChanges)
	}

	// Check high signal variance (instability)
	if latest.Metrics.SignalVariance != nil && *latest.Metrics.SignalVariance > 10.0 {
		return true, fmt.Sprintf("high signal variance: %.1f", *latest.Metrics.SignalVariance)
	}

	// Check roaming risk
	if csp.calculateRoamingRisk() > 0.8 {
		return true, "high roaming risk detected"
	}

	// Check handover risk
	if csp.calculateHandoverRisk() > 0.9 {
		return true, "imminent handover detected"
	}

	// Check for SINR degradation
	sinrTrend := csp.calculateSINRTrend()
	if sinrTrend < -1.0 { // Losing more than 1 dB SINR per sample
		return true, fmt.Sprintf("rapid SINR degradation: %.1f dB/sample", sinrTrend)
	}

	return false, "no cellular-specific triggers"
}

// calculateRSRPTrend calculates the rate of change in RSRP
func (csp *CellularSpecificPredictor) calculateRSRPTrend() float64 {
	if len(csp.samples) < 3 {
		return 0
	}

	recentSamples := csp.getRecentSamples(20) // Last 20 samples

	var sumX, sumY, sumXY, sumX2 float64
	validSamples := 0

	for i, sample := range recentSamples {
		if sample.Metrics != nil && sample.Metrics.RSRP != nil {
			x := float64(i)
			y := *sample.Metrics.RSRP
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

// calculateRSRQTrend calculates the rate of change in RSRQ
func (csp *CellularSpecificPredictor) calculateRSRQTrend() float64 {
	if len(csp.samples) < 3 {
		return 0
	}

	recentSamples := csp.getRecentSamples(20)

	var sumX, sumY, sumXY, sumX2 float64
	validSamples := 0

	for i, sample := range recentSamples {
		if sample.Metrics != nil && sample.Metrics.RSRQ != nil {
			x := float64(i)
			y := *sample.Metrics.RSRQ
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

// calculateSINRTrend calculates the rate of change in SINR
func (csp *CellularSpecificPredictor) calculateSINRTrend() float64 {
	if len(csp.samples) < 3 {
		return 0
	}

	recentSamples := csp.getRecentSamples(20)

	var sumX, sumY, sumXY, sumX2 float64
	validSamples := 0

	for i, sample := range recentSamples {
		if sample.Metrics != nil && sample.Metrics.SINR != nil {
			x := float64(i)
			y := *sample.Metrics.SINR
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

// predictFutureRSRP predicts RSRP in the next sample
func (csp *CellularSpecificPredictor) predictFutureRSRP() float64 {
	if len(csp.samples) == 0 {
		return 0
	}

	latest := csp.samples[len(csp.samples)-1]
	if latest.Metrics == nil || latest.Metrics.RSRP == nil {
		return 0
	}

	current := *latest.Metrics.RSRP
	trend := csp.calculateRSRPTrend()

	predicted := current + trend

	// Clamp to reasonable RSRP range [-140, -40] dBm
	return math.Max(-140, math.Min(-40, predicted))
}

// predictFutureRSRQ predicts RSRQ in the next sample
func (csp *CellularSpecificPredictor) predictFutureRSRQ() float64 {
	if len(csp.samples) == 0 {
		return 0
	}

	latest := csp.samples[len(csp.samples)-1]
	if latest.Metrics == nil || latest.Metrics.RSRQ == nil {
		return 0
	}

	current := *latest.Metrics.RSRQ
	trend := csp.calculateRSRQTrend()

	predicted := current + trend

	// Clamp to reasonable RSRQ range [-20, 0] dB
	return math.Max(-20, math.Min(0, predicted))
}

// predictFutureSINR predicts SINR in the next sample
func (csp *CellularSpecificPredictor) predictFutureSINR() float64 {
	if len(csp.samples) == 0 {
		return 0
	}

	latest := csp.samples[len(csp.samples)-1]
	if latest.Metrics == nil || latest.Metrics.SINR == nil {
		return 0
	}

	current := *latest.Metrics.SINR
	trend := csp.calculateSINRTrend()

	predicted := current + trend

	// Clamp to reasonable SINR range [-10, 30] dB
	return math.Max(-10, math.Min(30, predicted))
}

// calculateHandoverRisk calculates the risk of an imminent cell handover
func (csp *CellularSpecificPredictor) calculateHandoverRisk() float64 {
	if len(csp.samples) < 5 {
		return 0
	}

	risk := 0.0

	// Check recent cell changes
	recentSamples := csp.getRecentSamples(10)
	cellChanges := 0

	var lastCellID string
	for _, sample := range recentSamples {
		if sample.Metrics != nil && sample.Metrics.CellID != nil {
			currentCellID := *sample.Metrics.CellID
			if lastCellID != "" && currentCellID != lastCellID {
				cellChanges++
			}
			lastCellID = currentCellID
		}
	}

	if cellChanges > 0 {
		risk = math.Min(float64(cellChanges)/3.0, 1.0) // Risk increases with cell changes
	}

	// Check RSRP degradation
	rsrpTrend := csp.calculateRSRPTrend()
	if rsrpTrend < -1.0 {
		degradationRisk := math.Min(math.Abs(rsrpTrend)/5.0, 0.8)
		risk = math.Max(risk, degradationRisk)
	}

	// Check signal variance
	latest := csp.samples[len(csp.samples)-1]
	if latest.Metrics != nil && latest.Metrics.SignalVariance != nil {
		varianceRisk := math.Min(*latest.Metrics.SignalVariance/20.0, 0.7)
		risk = math.Max(risk, varianceRisk)
	}

	return risk
}

// calculateRoamingRisk calculates the risk of entering roaming
func (csp *CellularSpecificPredictor) calculateRoamingRisk() float64 {
	if len(csp.samples) == 0 {
		return 0
	}

	latest := csp.samples[len(csp.samples)-1]
	if latest.Metrics == nil {
		return 0
	}

	risk := 0.0

	// Check if already roaming
	if latest.Metrics.Roaming != nil && *latest.Metrics.Roaming {
		risk = 0.9 // High risk if already roaming
	}

	// Check operator changes (indicator of potential roaming)
	if len(csp.samples) >= 5 {
		recentSamples := csp.getRecentSamples(5)
		operatorChanges := 0

		var lastOperator string
		for _, sample := range recentSamples {
			if sample.Metrics != nil && sample.Metrics.Operator != nil {
				currentOperator := *sample.Metrics.Operator
				if lastOperator != "" && currentOperator != lastOperator {
					operatorChanges++
				}
				lastOperator = currentOperator
			}
		}

		if operatorChanges > 0 {
			operatorRisk := math.Min(float64(operatorChanges)/2.0, 0.8)
			risk = math.Max(risk, operatorRisk)
		}
	}

	// Check very weak signal (might force roaming)
	if latest.Metrics.RSRP != nil && *latest.Metrics.RSRP < -130 {
		weakSignalRisk := math.Min((130+*latest.Metrics.RSRP)/(-10), 0.7)
		risk = math.Max(risk, weakSignalRisk)
	}

	return risk
}

// Helper methods

func (csp *CellularSpecificPredictor) getRecentSamples(count int) []ConnectionSample {
	if count >= len(csp.samples) {
		return csp.samples
	}
	return csp.samples[len(csp.samples)-count:]
}

// GetStatus returns cellular-specific predictor status
func (csp *CellularSpecificPredictor) GetStatus() map[string]interface{} {
	status := map[string]interface{}{
		"samples_count": len(csp.samples),
	}

	if len(csp.samples) > 0 {
		latest := csp.samples[len(csp.samples)-1]
		if latest.Metrics != nil {
			if latest.Metrics.RSRP != nil {
				status["latest_rsrp"] = *latest.Metrics.RSRP
			}
			if latest.Metrics.RSRQ != nil {
				status["latest_rsrq"] = *latest.Metrics.RSRQ
			}
			if latest.Metrics.SINR != nil {
				status["latest_sinr"] = *latest.Metrics.SINR
			}
			if latest.Metrics.StabilityScore != nil {
				status["stability_score"] = *latest.Metrics.StabilityScore
			}
		}

		// Add trend information
		status["rsrp_trend"] = csp.calculateRSRPTrend()
		status["rsrq_trend"] = csp.calculateRSRQTrend()
		status["sinr_trend"] = csp.calculateSINRTrend()
		status["handover_risk"] = csp.calculateHandoverRisk()
		status["roaming_risk"] = csp.calculateRoamingRisk()
	}

	return status
}
