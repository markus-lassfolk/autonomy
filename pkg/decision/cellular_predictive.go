package decision

import (
	"context"
	"fmt"
	"time"

	"github.com/markus-lassfolk/autonomy/pkg"
	"github.com/markus-lassfolk/autonomy/pkg/collector"
	"github.com/markus-lassfolk/autonomy/pkg/logx"
)

// CellularPredictiveAnalyzer provides predictive analysis for cellular stability
type CellularPredictiveAnalyzer struct {
	logger *logx.Logger
	config *CellularPredictiveConfig
}

// CellularPredictiveConfig holds configuration for cellular predictive analysis
type CellularPredictiveConfig struct {
	// Stability thresholds
	HealthyStabilityScore   int     `json:"healthy_stability_score"`   // 75 - score above which cellular is healthy
	UnhealthyStabilityScore int     `json:"unhealthy_stability_score"` // 50 - score below which cellular is unhealthy
	PredictiveRiskThreshold float64 `json:"predictive_risk_threshold"` // 0.7 - risk above which to trigger predictive failover

	// Hysteresis settings
	PredictiveFailoverCooldownSeconds int `json:"predictive_failover_cooldown_seconds"` // 120 - cooldown after predictive failover
	StabilityWindowSeconds            int `json:"stability_window_seconds"`             // 30 - time window for stability assessment

	// Signal degradation thresholds
	RSRPDegradationThreshold float64 `json:"rsrp_degradation_threshold"`  // -10 dBm - significant RSRP drop
	RSRQDegradationThreshold float64 `json:"rsrq_degradation_threshold"`  // -3 dB - significant RSRQ drop
	SINRDegradationThreshold float64 `json:"sinr_degradation_threshold"`  // -5 dB - significant SINR drop
	VarianceAlarmThreshold   float64 `json:"variance_alarm_threshold"`    // 8.0 dB - high variance alarm
	CellChangeAlarmThreshold int     `json:"cell_change_alarm_threshold"` // 2 - cell changes in window

	// Predictive scoring weights
	StabilityScoreWeight    float64 `json:"stability_score_weight"`    // 0.4 - weight for stability score
	PredictiveRiskWeight    float64 `json:"predictive_risk_weight"`    // 0.3 - weight for predictive risk
	SignalDegradationWeight float64 `json:"signal_degradation_weight"` // 0.3 - weight for signal degradation
}

// CellularStabilityAssessment represents a stability assessment for cellular
type CellularStabilityAssessment struct {
	Score              int       `json:"score"`               // Overall stability score (0-100)
	Status             string    `json:"status"`              // "healthy", "degraded", "unhealthy", "critical"
	PredictiveRisk     float64   `json:"predictive_risk"`     // Risk of impending failure (0-1)
	RecommendAction    string    `json:"recommend_action"`    // "none", "monitor", "prepare_failover", "failover_now"
	Reasoning          []string  `json:"reasoning"`           // Human-readable reasoning
	LastUpdate         time.Time `json:"last_update"`         // When assessment was made
	SignalDegradation  float64   `json:"signal_degradation"`  // Signal degradation score (0-1)
	VarianceAlarm      bool      `json:"variance_alarm"`      // High variance detected
	CellChangeAlarm    bool      `json:"cell_change_alarm"`   // Frequent cell changes
	ThroughputDegraded bool      `json:"throughput_degraded"` // Throughput below threshold
}

// NewCellularPredictiveAnalyzer creates a new cellular predictive analyzer
func NewCellularPredictiveAnalyzer(logger *logx.Logger) *CellularPredictiveAnalyzer {
	config := &CellularPredictiveConfig{
		HealthyStabilityScore:             75,
		UnhealthyStabilityScore:           50,
		PredictiveRiskThreshold:           0.7,
		PredictiveFailoverCooldownSeconds: 120,
		StabilityWindowSeconds:            30,
		RSRPDegradationThreshold:          -10.0,
		RSRQDegradationThreshold:          -3.0,
		SINRDegradationThreshold:          -5.0,
		VarianceAlarmThreshold:            8.0,
		CellChangeAlarmThreshold:          2,
		StabilityScoreWeight:              0.4,
		PredictiveRiskWeight:              0.3,
		SignalDegradationWeight:           0.3,
	}

	return &CellularPredictiveAnalyzer{
		logger: logger,
		config: config,
	}
}

// AnalyzeCellularStability analyzes cellular stability and provides predictive insights
func (cpa *CellularPredictiveAnalyzer) AnalyzeCellularStability(ctx context.Context, member *pkg.Member, metrics *pkg.Metrics, stabilityCollector *collector.CellularStabilityCollector) *CellularStabilityAssessment {
	assessment := &CellularStabilityAssessment{
		LastUpdate: time.Now(),
		Reasoning:  make([]string, 0),
	}

	// Get stability information from collector if available
	if stabilityCollector != nil {
		if stability := stabilityCollector.GetStabilityStatus(member.Iface); stability != nil {
			assessment.Score = stability.CurrentScore
			assessment.Status = stability.Status
			assessment.PredictiveRisk = stability.PredictiveRisk
		}
	}

	// Analyze signal degradation if we have cellular metrics
	if metrics != nil {
		assessment.SignalDegradation = cpa.calculateSignalDegradation(metrics)
		assessment.VarianceAlarm = cpa.detectVarianceAlarm(metrics)
		assessment.CellChangeAlarm = cpa.detectCellChangeAlarm(metrics)
		assessment.ThroughputDegraded = cpa.detectThroughputDegradation(metrics)
	}

	// Determine overall assessment and recommendation
	cpa.determineRecommendation(assessment)

	// Log assessment if significant
	if assessment.RecommendAction != "none" {
		cpa.logger.Info("Cellular stability assessment",
			"interface", member.Iface,
			"score", assessment.Score,
			"status", assessment.Status,
			"predictive_risk", assessment.PredictiveRisk,
			"action", assessment.RecommendAction,
			"reasoning", assessment.Reasoning)
	}

	return assessment
}

// calculateSignalDegradation calculates signal degradation score based on recent trends
func (cpa *CellularPredictiveAnalyzer) calculateSignalDegradation(metrics *pkg.Metrics) float64 {
	degradation := 0.0

	// Check RSRP degradation
	if metrics.RSRP != nil && *metrics.RSRP < -100.0 {
		rsrpDegradation := (*metrics.RSRP + 100.0) / cpa.config.RSRPDegradationThreshold
		if rsrpDegradation < 0 {
			degradation += 0.4 * (-rsrpDegradation) // 40% weight for RSRP
		}
	}

	// Check RSRQ degradation
	if metrics.RSRQ != nil && *metrics.RSRQ < -12.0 {
		rsrqDegradation := (*metrics.RSRQ + 12.0) / cpa.config.RSRQDegradationThreshold
		if rsrqDegradation < 0 {
			degradation += 0.3 * (-rsrqDegradation) // 30% weight for RSRQ
		}
	}

	// Check SINR degradation
	if metrics.SINR != nil && *metrics.SINR < 5.0 {
		sinrDegradation := (*metrics.SINR - 5.0) / cpa.config.SINRDegradationThreshold
		if sinrDegradation < 0 {
			degradation += 0.3 * (-sinrDegradation) // 30% weight for SINR
		}
	}

	return clamp(degradation, 0.0, 1.0)
}

// detectVarianceAlarm detects high signal variance indicating instability
func (cpa *CellularPredictiveAnalyzer) detectVarianceAlarm(metrics *pkg.Metrics) bool {
	if metrics.SignalVariance != nil {
		return *metrics.SignalVariance > cpa.config.VarianceAlarmThreshold
	}
	return false
}

// detectCellChangeAlarm detects frequent cell changes indicating instability
func (cpa *CellularPredictiveAnalyzer) detectCellChangeAlarm(metrics *pkg.Metrics) bool {
	if metrics.CellChanges != nil {
		return *metrics.CellChanges >= cpa.config.CellChangeAlarmThreshold
	}
	return false
}

// detectThroughputDegradation detects throughput degradation
func (cpa *CellularPredictiveAnalyzer) detectThroughputDegradation(metrics *pkg.Metrics) bool {
	if metrics.ThroughputKbps != nil {
		// Consider throughput degraded if below 50 Kbps
		return *metrics.ThroughputKbps < 50.0
	}
	return false
}

// determineRecommendation determines the recommended action based on assessment
func (cpa *CellularPredictiveAnalyzer) determineRecommendation(assessment *CellularStabilityAssessment) {
	// Critical conditions - immediate failover
	if assessment.Score < 30 || assessment.PredictiveRisk > 0.9 {
		assessment.RecommendAction = "failover_now"
		assessment.Status = "critical"
		assessment.Reasoning = append(assessment.Reasoning, "Critical cellular conditions detected")
		return
	}

	// Unhealthy conditions - prepare for failover
	if assessment.Score < cpa.config.UnhealthyStabilityScore {
		assessment.RecommendAction = "prepare_failover"
		assessment.Reasoning = append(assessment.Reasoning, "Stability score below unhealthy threshold")
	}

	// High predictive risk - prepare for failover
	if assessment.PredictiveRisk > cpa.config.PredictiveRiskThreshold {
		assessment.RecommendAction = "prepare_failover"
		assessment.Reasoning = append(assessment.Reasoning, "High predictive risk detected")
	}

	// Signal degradation - monitor closely
	if assessment.SignalDegradation > 0.6 {
		if assessment.RecommendAction == "" {
			assessment.RecommendAction = "monitor"
		}
		assessment.Reasoning = append(assessment.Reasoning, "Significant signal degradation detected")
	}

	// Variance alarm - monitor
	if assessment.VarianceAlarm {
		if assessment.RecommendAction == "" {
			assessment.RecommendAction = "monitor"
		}
		assessment.Reasoning = append(assessment.Reasoning, "High signal variance detected")
	}

	// Cell change alarm - monitor
	if assessment.CellChangeAlarm {
		if assessment.RecommendAction == "" {
			assessment.RecommendAction = "monitor"
		}
		assessment.Reasoning = append(assessment.Reasoning, "Frequent cell changes detected")
	}

	// Throughput degraded - monitor
	if assessment.ThroughputDegraded {
		if assessment.RecommendAction == "" {
			assessment.RecommendAction = "monitor"
		}
		assessment.Reasoning = append(assessment.Reasoning, "Throughput degradation detected")
	}

	// Default to no action if nothing significant detected
	if assessment.RecommendAction == "" {
		assessment.RecommendAction = "none"
	}
}

// ShouldTriggerPredictiveFailover determines if predictive failover should be triggered
func (cpa *CellularPredictiveAnalyzer) ShouldTriggerPredictiveFailover(assessment *CellularStabilityAssessment, member *pkg.Member) (bool, string) {
	if assessment.RecommendAction == "failover_now" {
		return true, "Critical cellular stability conditions detected"
	}

	// Combine multiple factors for predictive failover decision
	if assessment.RecommendAction == "prepare_failover" {
		// Calculate combined risk score
		combinedRisk := 0.0

		// Stability score component (inverted - lower score = higher risk)
		stabilityRisk := float64(100-assessment.Score) / 100.0
		combinedRisk += cpa.config.StabilityScoreWeight * stabilityRisk

		// Predictive risk component
		combinedRisk += cpa.config.PredictiveRiskWeight * assessment.PredictiveRisk

		// Signal degradation component
		combinedRisk += cpa.config.SignalDegradationWeight * assessment.SignalDegradation

		// Additional penalties for alarms
		if assessment.VarianceAlarm {
			combinedRisk += 0.1
		}
		if assessment.CellChangeAlarm {
			combinedRisk += 0.1
		}
		if assessment.ThroughputDegraded {
			combinedRisk += 0.1
		}

		// Trigger predictive failover if combined risk is high
		if combinedRisk > 0.8 {
			reason := "Predictive failover triggered: combined risk score " +
				formatFloat(combinedRisk, 2) + " exceeds threshold 0.8"
			return true, reason
		}
	}

	return false, ""
}

// GetStabilityTrend analyzes stability trend over recent samples
func (cpa *CellularPredictiveAnalyzer) GetStabilityTrend(stabilityCollector *collector.CellularStabilityCollector) string {
	if stabilityCollector == nil {
		return "unknown"
	}

	// Get recent samples for trend analysis
	samples := stabilityCollector.GetRecentSamples(5 * time.Minute)
	if len(samples) < 5 {
		return "insufficient_data"
	}

	// Calculate RSRP trend over recent samples
	recentSamples := samples[len(samples)-5:]
	firstRSRP := recentSamples[0].RSRP
	lastRSRP := recentSamples[len(recentSamples)-1].RSRP

	rsrpChange := lastRSRP - firstRSRP

	if rsrpChange < -5.0 {
		return "degrading"
	} else if rsrpChange > 3.0 {
		return "improving"
	} else {
		return "stable"
	}
}

// clamp clamps a value between min and max
func clamp(value, min, max float64) float64 {
	if value < min {
		return min
	}
	if value > max {
		return max
	}
	return value
}

// formatFloat formats a float64 to specified decimal places
func formatFloat(value float64, decimals int) string {
	format := "%." + string(rune('0'+decimals)) + "f"
	return fmt.Sprintf(format, value)
}
