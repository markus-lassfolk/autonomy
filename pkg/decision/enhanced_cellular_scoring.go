package decision

import (
	"math"
	"strings"

	"github.com/markus-lassfolk/autonomy/pkg"
	"github.com/markus-lassfolk/autonomy/pkg/logx"
)

// EnhancedCellularScorer provides comprehensive cellular scoring
type EnhancedCellularScorer struct {
	logger *logx.Logger
	config *EnhancedScoringConfig
}

// EnhancedScoringConfig holds configuration for enhanced cellular scoring
type EnhancedScoringConfig struct {
	// Signal Strength Weights (Total: 35%)
	RSRPWeight float64 `json:"rsrp_weight"` // 0.15 - Reference Signal Received Power
	RSRQWeight float64 `json:"rsrq_weight"` // 0.10 - Reference Signal Received Quality
	SINRWeight float64 `json:"sinr_weight"` // 0.10 - Signal-to-Interference-plus-Noise Ratio

	// Connectivity Weights (Total: 40%)
	LatencyWeight float64 `json:"latency_weight"` // 0.20 - Network latency
	LossWeight    float64 `json:"loss_weight"`    // 0.15 - Packet loss
	JitterWeight  float64 `json:"jitter_weight"`  // 0.05 - Network jitter

	// Stability Weights (Total: 15%)
	VarianceWeight   float64 `json:"variance_weight"`    // 0.08 - Signal variance penalty
	CellChangeWeight float64 `json:"cell_change_weight"` // 0.04 - Cell handoff penalty
	ThroughputWeight float64 `json:"throughput_weight"`  // 0.03 - Throughput performance

	// Quality Factors (Total: 10%)
	NetworkTypeWeight float64 `json:"network_type_weight"` // 0.05 - 5G > LTE > 3G
	BandQualityWeight float64 `json:"band_quality_weight"` // 0.03 - Band performance
	ModemHealthWeight float64 `json:"modem_health_weight"` // 0.02 - Modem status

	// Signal Strength Thresholds
	RSRPExcellent float64 `json:"rsrp_excellent"` // -80 dBm
	RSRPGood      float64 `json:"rsrp_good"`      // -90 dBm
	RSRPFair      float64 `json:"rsrp_fair"`      // -100 dBm
	RSRPPoor      float64 `json:"rsrp_poor"`      // -110 dBm
	RSRPUnusable  float64 `json:"rsrp_unusable"`  // -120 dBm

	RSRQExcellent float64 `json:"rsrq_excellent"` // -8 dB
	RSRQGood      float64 `json:"rsrq_good"`      // -12 dB
	RSRQFair      float64 `json:"rsrq_fair"`      // -15 dB
	RSRQPoor      float64 `json:"rsrq_poor"`      // -18 dB
	RSRQUnusable  float64 `json:"rsrq_unusable"`  // -25 dB

	SINRExcellent float64 `json:"sinr_excellent"` // 20 dB
	SINRGood      float64 `json:"sinr_good"`      // 10 dB
	SINRFair      float64 `json:"sinr_fair"`      // 5 dB
	SINRPoor      float64 `json:"sinr_poor"`      // 0 dB
	SINRUnusable  float64 `json:"sinr_unusable"`  // -5 dB

	// Connectivity Thresholds
	LatencyExcellent float64 `json:"latency_excellent"` // 50 ms
	LatencyGood      float64 `json:"latency_good"`      // 100 ms
	LatencyFair      float64 `json:"latency_fair"`      // 200 ms
	LatencyPoor      float64 `json:"latency_poor"`      // 500 ms
	LatencyUnusable  float64 `json:"latency_unusable"`  // 1000 ms

	LossExcellent float64 `json:"loss_excellent"` // 0%
	LossGood      float64 `json:"loss_good"`      // 1%
	LossFair      float64 `json:"loss_fair"`      // 3%
	LossPoor      float64 `json:"loss_poor"`      // 7%
	LossUnusable  float64 `json:"loss_unusable"`  // 15%

	JitterExcellent float64 `json:"jitter_excellent"` // 5 ms
	JitterGood      float64 `json:"jitter_good"`      // 15 ms
	JitterFair      float64 `json:"jitter_fair"`      // 30 ms
	JitterPoor      float64 `json:"jitter_poor"`      // 60 ms
	JitterUnusable  float64 `json:"jitter_unusable"`  // 120 ms

	// Stability Thresholds
	VarianceThreshold   float64 `json:"variance_threshold"`    // 8.0 dB
	CellChangeThreshold int     `json:"cell_change_threshold"` // 3 changes per window
	ThroughputThreshold float64 `json:"throughput_threshold"`  // 500 Kbps minimum

	// Network Type Bonuses
	FiveGBonus  float64 `json:"five_g_bonus"`  // 15 points for 5G
	LTEBonus    float64 `json:"lte_bonus"`     // 10 points for LTE
	ThreeGBonus float64 `json:"three_g_bonus"` // 0 points for 3G (baseline)
	TwoGPenalty float64 `json:"two_g_penalty"` // -20 points for 2G
}

// CellularScoreBreakdown provides detailed scoring breakdown
type CellularScoreBreakdown struct {
	TotalScore        float64            `json:"total_score"`
	Components        map[string]float64 `json:"components"`
	SignalScore       float64            `json:"signal_score"`
	ConnectivityScore float64            `json:"connectivity_score"`
	StabilityScore    float64            `json:"stability_score"`
	QualityScore      float64            `json:"quality_score"`
	Bonuses           map[string]float64 `json:"bonuses"`
	Penalties         map[string]float64 `json:"penalties"`
	Grade             string             `json:"grade"`          // A+, A, B+, B, C+, C, D, F
	Recommendation    string             `json:"recommendation"` // Action recommendation
}

// NewEnhancedCellularScorer creates a new enhanced cellular scorer
func NewEnhancedCellularScorer(logger *logx.Logger) *EnhancedCellularScorer {
	config := &EnhancedScoringConfig{
		// Signal Strength Weights (35% total)
		RSRPWeight: 0.15,
		RSRQWeight: 0.10,
		SINRWeight: 0.10,

		// Connectivity Weights (40% total)
		LatencyWeight: 0.20,
		LossWeight:    0.15,
		JitterWeight:  0.05,

		// Stability Weights (15% total)
		VarianceWeight:   0.08,
		CellChangeWeight: 0.04,
		ThroughputWeight: 0.03,

		// Quality Factors (10% total)
		NetworkTypeWeight: 0.05,
		BandQualityWeight: 0.03,
		ModemHealthWeight: 0.02,

		// RSRP Thresholds (dBm)
		RSRPExcellent: -80,
		RSRPGood:      -90,
		RSRPFair:      -100,
		RSRPPoor:      -110,
		RSRPUnusable:  -120,

		// RSRQ Thresholds (dB)
		RSRQExcellent: -8,
		RSRQGood:      -12,
		RSRQFair:      -15,
		RSRQPoor:      -18,
		RSRQUnusable:  -25,

		// SINR Thresholds (dB)
		SINRExcellent: 20,
		SINRGood:      10,
		SINRFair:      5,
		SINRPoor:      0,
		SINRUnusable:  -5,

		// Latency Thresholds (ms)
		LatencyExcellent: 50,
		LatencyGood:      100,
		LatencyFair:      200,
		LatencyPoor:      500,
		LatencyUnusable:  1000,

		// Loss Thresholds (%)
		LossExcellent: 0,
		LossGood:      1,
		LossFair:      3,
		LossPoor:      7,
		LossUnusable:  15,

		// Jitter Thresholds (ms)
		JitterExcellent: 5,
		JitterGood:      15,
		JitterFair:      30,
		JitterPoor:      60,
		JitterUnusable:  120,

		// Stability Thresholds
		VarianceThreshold:   8.0,
		CellChangeThreshold: 3,
		ThroughputThreshold: 500,

		// Network Type Bonuses
		FiveGBonus:  15,
		LTEBonus:    10,
		ThreeGBonus: 0,
		TwoGPenalty: -20,
	}

	return &EnhancedCellularScorer{
		logger: logger,
		config: config,
	}
}

// CalculateEnhancedScore calculates comprehensive cellular score
func (ecs *EnhancedCellularScorer) CalculateEnhancedScore(metrics *pkg.Metrics) *CellularScoreBreakdown {
	breakdown := &CellularScoreBreakdown{
		Components: make(map[string]float64),
		Bonuses:    make(map[string]float64),
		Penalties:  make(map[string]float64),
	}

	// 1. Signal Strength Score (35%)
	signalScore := ecs.calculateSignalScore(metrics, breakdown)
	breakdown.SignalScore = signalScore

	// 2. Connectivity Score (40%)
	connectivityScore := ecs.calculateConnectivityScore(metrics, breakdown)
	breakdown.ConnectivityScore = connectivityScore

	// 3. Stability Score (15%)
	stabilityScore := ecs.calculateStabilityScore(metrics, breakdown)
	breakdown.StabilityScore = stabilityScore

	// 4. Quality Score (10%)
	qualityScore := ecs.calculateQualityScore(metrics, breakdown)
	breakdown.QualityScore = qualityScore

	// Calculate total score
	totalScore := signalScore + connectivityScore + stabilityScore + qualityScore

	// Apply bonuses and penalties
	for _, bonus := range breakdown.Bonuses {
		totalScore += bonus
	}
	for _, penalty := range breakdown.Penalties {
		totalScore += penalty // penalties are negative
	}

	// Clamp to 0-100 range
	totalScore = math.Max(0, math.Min(100, totalScore))
	breakdown.TotalScore = totalScore

	// Assign grade and recommendation
	breakdown.Grade = ecs.assignGrade(totalScore)
	breakdown.Recommendation = ecs.getRecommendation(totalScore, breakdown)

	return breakdown
}

// calculateSignalScore calculates signal strength component score
func (ecs *EnhancedCellularScorer) calculateSignalScore(metrics *pkg.Metrics, breakdown *CellularScoreBreakdown) float64 {
	var totalScore float64

	// RSRP Score (15% of total)
	if metrics.RSRP != nil {
		rsrpScore := ecs.scoreRSRP(*metrics.RSRP) * ecs.config.RSRPWeight * 100
		breakdown.Components["rsrp"] = rsrpScore
		totalScore += rsrpScore
	}

	// RSRQ Score (10% of total)
	if metrics.RSRQ != nil {
		rsrqScore := ecs.scoreRSRQ(*metrics.RSRQ) * ecs.config.RSRQWeight * 100
		breakdown.Components["rsrq"] = rsrqScore
		totalScore += rsrqScore
	}

	// SINR Score (10% of total)
	if metrics.SINR != nil {
		sinrScore := ecs.scoreSINR(*metrics.SINR) * ecs.config.SINRWeight * 100
		breakdown.Components["sinr"] = sinrScore
		totalScore += sinrScore
	}

	return totalScore
}

// calculateConnectivityScore calculates connectivity component score
func (ecs *EnhancedCellularScorer) calculateConnectivityScore(metrics *pkg.Metrics, breakdown *CellularScoreBreakdown) float64 {
	var totalScore float64

	// Latency Score (20% of total)
	if metrics.LatencyMS != nil {
		latencyScore := ecs.scoreLatency(*metrics.LatencyMS) * ecs.config.LatencyWeight * 100
		breakdown.Components["latency"] = latencyScore
		totalScore += latencyScore
	}

	// Loss Score (15% of total)
	if metrics.LossPercent != nil {
		lossScore := ecs.scoreLoss(*metrics.LossPercent) * ecs.config.LossWeight * 100
		breakdown.Components["loss"] = lossScore
		totalScore += lossScore
	}

	// Jitter Score (5% of total)
	if metrics.JitterMS != nil {
		jitterScore := ecs.scoreJitter(*metrics.JitterMS) * ecs.config.JitterWeight * 100
		breakdown.Components["jitter"] = jitterScore
		totalScore += jitterScore
	}

	return totalScore
}

// calculateStabilityScore calculates stability component score
func (ecs *EnhancedCellularScorer) calculateStabilityScore(metrics *pkg.Metrics, breakdown *CellularScoreBreakdown) float64 {
	var totalScore float64

	// Signal Variance Penalty (8% of total)
	if metrics.SignalVariance != nil {
		variancePenalty := ecs.scoreVariance(*metrics.SignalVariance) * ecs.config.VarianceWeight * 100
		breakdown.Components["variance"] = variancePenalty
		totalScore += variancePenalty

		if *metrics.SignalVariance > ecs.config.VarianceThreshold {
			breakdown.Penalties["high_variance"] = -5
		}
	}

	// Cell Change Penalty (4% of total)
	if metrics.CellChanges != nil {
		cellChangePenalty := ecs.scoreCellChanges(*metrics.CellChanges) * ecs.config.CellChangeWeight * 100
		breakdown.Components["cell_changes"] = cellChangePenalty
		totalScore += cellChangePenalty

		if *metrics.CellChanges > ecs.config.CellChangeThreshold {
			breakdown.Penalties["frequent_handoffs"] = -3
		}
	}

	// Throughput Score (3% of total)
	if metrics.ThroughputKbps != nil {
		throughputScore := ecs.scoreThroughput(*metrics.ThroughputKbps) * ecs.config.ThroughputWeight * 100
		breakdown.Components["throughput"] = throughputScore
		totalScore += throughputScore

		if *metrics.ThroughputKbps < ecs.config.ThroughputThreshold {
			breakdown.Penalties["low_throughput"] = -2
		}
	}

	return totalScore
}

// calculateQualityScore calculates quality factor score
func (ecs *EnhancedCellularScorer) calculateQualityScore(metrics *pkg.Metrics, breakdown *CellularScoreBreakdown) float64 {
	var totalScore float64

	// Network Type Bonus (5% of total)
	if metrics.NetworkType != nil {
		networkBonus := ecs.scoreNetworkType(*metrics.NetworkType) * ecs.config.NetworkTypeWeight * 100
		breakdown.Components["network_type"] = networkBonus
		totalScore += networkBonus

		// Apply network type bonuses
		switch *metrics.NetworkType {
		case "5G NSA", "5G SA", "NR":
			breakdown.Bonuses["5g_network"] = ecs.config.FiveGBonus
		case "LTE", "LTE-A":
			breakdown.Bonuses["lte_network"] = ecs.config.LTEBonus
		case "UMTS", "HSPA", "HSPA+":
			breakdown.Bonuses["3g_network"] = ecs.config.ThreeGBonus
		case "GSM", "EDGE", "GPRS":
			breakdown.Penalties["2g_network"] = ecs.config.TwoGPenalty
		}
	}

	// Band Quality (3% of total)
	if metrics.Band != nil {
		bandScore := ecs.scoreBand(*metrics.Band) * ecs.config.BandQualityWeight * 100
		breakdown.Components["band"] = bandScore
		totalScore += bandScore
	}

	// Modem Health (2% of total) - placeholder for future modem health metrics
	modemScore := 1.0 * ecs.config.ModemHealthWeight * 100 // Assume healthy for now
	breakdown.Components["modem_health"] = modemScore
	totalScore += modemScore

	return totalScore
}

// Signal strength scoring functions
func (ecs *EnhancedCellularScorer) scoreRSRP(rsrp float64) float64 {
	if rsrp >= ecs.config.RSRPExcellent {
		return 1.0
	} else if rsrp >= ecs.config.RSRPGood {
		return 0.8 + 0.2*(rsrp-ecs.config.RSRPGood)/(ecs.config.RSRPExcellent-ecs.config.RSRPGood)
	} else if rsrp >= ecs.config.RSRPFair {
		return 0.6 + 0.2*(rsrp-ecs.config.RSRPFair)/(ecs.config.RSRPGood-ecs.config.RSRPFair)
	} else if rsrp >= ecs.config.RSRPPoor {
		return 0.3 + 0.3*(rsrp-ecs.config.RSRPPoor)/(ecs.config.RSRPFair-ecs.config.RSRPPoor)
	} else if rsrp >= ecs.config.RSRPUnusable {
		return 0.1 + 0.2*(rsrp-ecs.config.RSRPUnusable)/(ecs.config.RSRPPoor-ecs.config.RSRPUnusable)
	} else {
		return 0.0
	}
}

func (ecs *EnhancedCellularScorer) scoreRSRQ(rsrq float64) float64 {
	if rsrq >= ecs.config.RSRQExcellent {
		return 1.0
	} else if rsrq >= ecs.config.RSRQGood {
		return 0.8 + 0.2*(rsrq-ecs.config.RSRQGood)/(ecs.config.RSRQExcellent-ecs.config.RSRQGood)
	} else if rsrq >= ecs.config.RSRQFair {
		return 0.6 + 0.2*(rsrq-ecs.config.RSRQFair)/(ecs.config.RSRQGood-ecs.config.RSRQFair)
	} else if rsrq >= ecs.config.RSRQPoor {
		return 0.3 + 0.3*(rsrq-ecs.config.RSRQPoor)/(ecs.config.RSRQFair-ecs.config.RSRQPoor)
	} else if rsrq >= ecs.config.RSRQUnusable {
		return 0.1 + 0.2*(rsrq-ecs.config.RSRQUnusable)/(ecs.config.RSRQPoor-ecs.config.RSRQUnusable)
	} else {
		return 0.0
	}
}

func (ecs *EnhancedCellularScorer) scoreSINR(sinr float64) float64 {
	if sinr >= ecs.config.SINRExcellent {
		return 1.0
	} else if sinr >= ecs.config.SINRGood {
		return 0.8 + 0.2*(sinr-ecs.config.SINRGood)/(ecs.config.SINRExcellent-ecs.config.SINRGood)
	} else if sinr >= ecs.config.SINRFair {
		return 0.6 + 0.2*(sinr-ecs.config.SINRFair)/(ecs.config.SINRGood-ecs.config.SINRFair)
	} else if sinr >= ecs.config.SINRPoor {
		return 0.3 + 0.3*(sinr-ecs.config.SINRPoor)/(ecs.config.SINRFair-ecs.config.SINRPoor)
	} else if sinr >= ecs.config.SINRUnusable {
		return 0.1 + 0.2*(sinr-ecs.config.SINRUnusable)/(ecs.config.SINRPoor-ecs.config.SINRUnusable)
	} else {
		return 0.0
	}
}

// Connectivity scoring functions
func (ecs *EnhancedCellularScorer) scoreLatency(latency float64) float64 {
	if latency <= ecs.config.LatencyExcellent {
		return 1.0
	} else if latency <= ecs.config.LatencyGood {
		return 0.8 + 0.2*(ecs.config.LatencyGood-latency)/(ecs.config.LatencyGood-ecs.config.LatencyExcellent)
	} else if latency <= ecs.config.LatencyFair {
		return 0.6 + 0.2*(ecs.config.LatencyFair-latency)/(ecs.config.LatencyFair-ecs.config.LatencyGood)
	} else if latency <= ecs.config.LatencyPoor {
		return 0.3 + 0.3*(ecs.config.LatencyPoor-latency)/(ecs.config.LatencyPoor-ecs.config.LatencyFair)
	} else if latency <= ecs.config.LatencyUnusable {
		return 0.1 + 0.2*(ecs.config.LatencyUnusable-latency)/(ecs.config.LatencyUnusable-ecs.config.LatencyPoor)
	} else {
		return 0.0
	}
}

func (ecs *EnhancedCellularScorer) scoreLoss(loss float64) float64 {
	if loss <= ecs.config.LossExcellent {
		return 1.0
	} else if loss <= ecs.config.LossGood {
		return 0.8 + 0.2*(ecs.config.LossGood-loss)/(ecs.config.LossGood-ecs.config.LossExcellent)
	} else if loss <= ecs.config.LossFair {
		return 0.6 + 0.2*(ecs.config.LossFair-loss)/(ecs.config.LossFair-ecs.config.LossGood)
	} else if loss <= ecs.config.LossPoor {
		return 0.3 + 0.3*(ecs.config.LossPoor-loss)/(ecs.config.LossPoor-ecs.config.LossFair)
	} else if loss <= ecs.config.LossUnusable {
		return 0.1 + 0.2*(ecs.config.LossUnusable-loss)/(ecs.config.LossUnusable-ecs.config.LossPoor)
	} else {
		return 0.0
	}
}

func (ecs *EnhancedCellularScorer) scoreJitter(jitter float64) float64 {
	if jitter <= ecs.config.JitterExcellent {
		return 1.0
	} else if jitter <= ecs.config.JitterGood {
		return 0.8 + 0.2*(ecs.config.JitterGood-jitter)/(ecs.config.JitterGood-ecs.config.JitterExcellent)
	} else if jitter <= ecs.config.JitterFair {
		return 0.6 + 0.2*(ecs.config.JitterFair-jitter)/(ecs.config.JitterFair-ecs.config.JitterGood)
	} else if jitter <= ecs.config.JitterPoor {
		return 0.3 + 0.3*(ecs.config.JitterPoor-jitter)/(ecs.config.JitterPoor-ecs.config.JitterFair)
	} else if jitter <= ecs.config.JitterUnusable {
		return 0.1 + 0.2*(ecs.config.JitterUnusable-jitter)/(ecs.config.JitterUnusable-ecs.config.JitterPoor)
	} else {
		return 0.0
	}
}

// Stability scoring functions
func (ecs *EnhancedCellularScorer) scoreVariance(variance float64) float64 {
	// Lower variance is better (more stable signal)
	if variance <= 2.0 {
		return 1.0 // Excellent stability
	} else if variance <= 4.0 {
		return 0.8 // Good stability
	} else if variance <= 6.0 {
		return 0.6 // Fair stability
	} else if variance <= 8.0 {
		return 0.3 // Poor stability
	} else {
		return 0.1 // Very unstable
	}
}

func (ecs *EnhancedCellularScorer) scoreCellChanges(changes int) float64 {
	// Fewer cell changes is better
	if changes == 0 {
		return 1.0 // No handoffs
	} else if changes == 1 {
		return 0.8 // Single handoff
	} else if changes <= 2 {
		return 0.6 // Few handoffs
	} else if changes <= 4 {
		return 0.3 // Many handoffs
	} else {
		return 0.1 // Excessive handoffs
	}
}

func (ecs *EnhancedCellularScorer) scoreThroughput(throughput float64) float64 {
	if throughput >= 5000 { // 5 Mbps
		return 1.0
	} else if throughput >= 2000 { // 2 Mbps
		return 0.8
	} else if throughput >= 1000 { // 1 Mbps
		return 0.6
	} else if throughput >= 500 { // 500 Kbps
		return 0.3
	} else {
		return 0.1
	}
}

// Quality scoring functions
func (ecs *EnhancedCellularScorer) scoreNetworkType(networkType string) float64 {
	switch networkType {
	case "5G NSA", "5G SA", "NR":
		return 1.0
	case "LTE", "LTE-A":
		return 0.8
	case "UMTS", "HSPA", "HSPA+":
		return 0.5
	case "GSM", "EDGE", "GPRS":
		return 0.2
	default:
		return 0.6 // Unknown, assume reasonable
	}
}

func (ecs *EnhancedCellularScorer) scoreBand(band string) float64 {
	// This is a simplified band scoring - could be enhanced with more specific band performance data
	switch {
	case strings.Contains(band, "B3"), strings.Contains(band, "B7"), strings.Contains(band, "B20"):
		return 1.0 // Excellent bands
	case strings.Contains(band, "B1"), strings.Contains(band, "B8"):
		return 0.8 // Good bands
	default:
		return 0.6 // Average band
	}
}

// assignGrade assigns a letter grade based on score
func (ecs *EnhancedCellularScorer) assignGrade(score float64) string {
	if score >= 95 {
		return "A+"
	} else if score >= 90 {
		return "A"
	} else if score >= 85 {
		return "B+"
	} else if score >= 80 {
		return "B"
	} else if score >= 75 {
		return "C+"
	} else if score >= 70 {
		return "C"
	} else if score >= 60 {
		return "D"
	} else {
		return "F"
	}
}

// getRecommendation provides action recommendation based on score
func (ecs *EnhancedCellularScorer) getRecommendation(score float64, breakdown *CellularScoreBreakdown) string {
	if score >= 85 {
		return "Excellent - No action required"
	} else if score >= 75 {
		return "Good - Monitor for changes"
	} else if score >= 65 {
		return "Fair - Consider optimization"
	} else if score >= 50 {
		return "Poor - Optimization recommended"
	} else if score >= 30 {
		return "Bad - Immediate attention required"
	} else {
		return "Critical - Consider failover"
	}
}

// Note: Using standard library strings.Contains function
