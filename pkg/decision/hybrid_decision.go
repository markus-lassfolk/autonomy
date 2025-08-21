package decision

import (
	"fmt"
	"time"

	"github.com/markus-lassfolk/autonomy/pkg"
	"github.com/markus-lassfolk/autonomy/pkg/discovery"
	"github.com/markus-lassfolk/autonomy/pkg/logx"
)

// HybridDecisionEngine extends the decision engine with intelligent weight adjustments
type HybridDecisionEngine struct {
	*Engine
	hybridWeightMgr *discovery.HybridWeightManager
	logger          *logx.Logger

	// Intelligent adjustment thresholds
	starlinkObstructionThreshold float64
	cellularSignalThreshold      float64
	latencyDegradationThreshold  float64
	lossThreshold                float64

	// Adjustment durations
	temporaryAdjustmentDuration time.Duration
	emergencyAdjustmentDuration time.Duration
}

// NewHybridDecisionEngine creates a new hybrid decision engine
func NewHybridDecisionEngine(baseEngine *Engine, hybridWeightMgr *discovery.HybridWeightManager, logger *logx.Logger) *HybridDecisionEngine {
	return &HybridDecisionEngine{
		Engine:                       baseEngine,
		hybridWeightMgr:              hybridWeightMgr,
		logger:                       logger,
		starlinkObstructionThreshold: 10.0,   // 10% obstruction triggers adjustment
		cellularSignalThreshold:      -110.0, // dBm
		latencyDegradationThreshold:  500.0,  // ms
		lossThreshold:                5.0,    // 5% packet loss
		temporaryAdjustmentDuration:  5 * time.Minute,
		emergencyAdjustmentDuration:  15 * time.Minute,
	}
}

// MakeIntelligentDecision makes failover decisions with intelligent weight adjustments
func (hde *HybridDecisionEngine) MakeIntelligentDecision(controller pkg.Controller) error {
	// First, analyze conditions and apply intelligent adjustments
	if err := hde.analyzeAndAdjustWeights(); err != nil {
		hde.logger.Error("Failed to analyze and adjust weights", "error", err)
	}

	// Then make the standard decision using adjusted weights
	return hde.Engine.makeDecision(controller)
}

// analyzeAndAdjustWeights analyzes current conditions and applies intelligent weight adjustments
func (hde *HybridDecisionEngine) analyzeAndAdjustWeights() error {
	now := time.Now()

	for name, member := range hde.members {
		// Get recent metrics for analysis
		samples, err := hde.telemetry.GetSamples(name, now.Add(-2*time.Minute))
		if err != nil || len(samples) == 0 {
			continue
		}

		latestSample := samples[len(samples)-1]
		if latestSample.Metrics == nil {
			continue
		}

		// Analyze member-specific conditions
		switch member.Class {
		case pkg.MemberClassStarlink:
			hde.analyzeStarlinkConditions(member, latestSample.Metrics)
		case pkg.MemberClassCellular:
			hde.analyzeCellularConditions(member, latestSample.Metrics)
		case pkg.MemberClassWiFi:
			hde.analyzeWiFiConditions(member, latestSample.Metrics)
		case pkg.MemberClassLAN:
			hde.analyzeLANConditions(member, latestSample.Metrics)
		}

		// Apply general performance-based adjustments
		hde.analyzeGeneralPerformance(member, latestSample.Metrics)
	}

	return nil
}

// analyzeStarlinkConditions analyzes Starlink-specific conditions
func (hde *HybridDecisionEngine) analyzeStarlinkConditions(member *pkg.Member, metrics *pkg.Metrics) {
	// Check for obstructions
	if metrics.ObstructionPct != nil && *metrics.ObstructionPct > hde.starlinkObstructionThreshold {
		reason := fmt.Sprintf("Starlink obstruction detected: %.1f%% > %.1f%%",
			*metrics.ObstructionPct, hde.starlinkObstructionThreshold)

		// Apply penalty to reduce Starlink priority temporarily
		originalWeight := hde.hybridWeightMgr.GetOriginalWeight(member.Name)
		penaltyWeight := originalWeight - 20 // Reduce by 20 points
		if penaltyWeight < 10 {
			penaltyWeight = 10 // Minimum weight
		}

		if err := hde.hybridWeightMgr.ApplyTemporaryAdjustment(
			member.Name,
			penaltyWeight,
			reason,
			discovery.AdjustmentTypePenalty,
			hde.temporaryAdjustmentDuration,
		); err != nil {
			hde.logger.Warn("Failed to apply temporary adjustment", "error", err, "member", member.Name)
		}

		hde.logger.Info("Applied Starlink obstruction penalty",
			"member", member.Name,
			"obstruction_percent", *metrics.ObstructionPct,
			"original_weight", originalWeight,
			"penalty_weight", penaltyWeight)
	}

	// Check for outages
	if metrics.Outages != nil && *metrics.Outages > 3 {
		reason := fmt.Sprintf("Starlink frequent outages: %d in recent period", *metrics.Outages)

		originalWeight := hde.hybridWeightMgr.GetOriginalWeight(member.Name)
		penaltyWeight := originalWeight - 30 // Larger penalty for outages
		if penaltyWeight < 5 {
			penaltyWeight = 5
		}

		if err := hde.hybridWeightMgr.ApplyTemporaryAdjustment(
			member.Name,
			penaltyWeight,
			reason,
			discovery.AdjustmentTypePenalty,
			hde.emergencyAdjustmentDuration, // Longer duration for outages
		); err != nil {
			hde.logger.Warn("Failed to apply temporary adjustment", "error", err, "member", member.Name)
		}
	}

	// Check for dish thermal issues
	if (metrics.ThermalThrottle != nil && *metrics.ThermalThrottle) || (metrics.ThermalShutdown != nil && *metrics.ThermalShutdown) {
		reason := "Starlink dish thermal management active"

		originalWeight := hde.hybridWeightMgr.GetOriginalWeight(member.Name)
		penaltyWeight := originalWeight - 10 // Small penalty for thermal issues
		if penaltyWeight < 20 {
			penaltyWeight = 20
		}

		if err := hde.hybridWeightMgr.ApplyTemporaryAdjustment(
			member.Name,
			penaltyWeight,
			reason,
			discovery.AdjustmentTypePenalty,
			hde.temporaryAdjustmentDuration,
		); err != nil {
			hde.logger.Warn("Failed to apply temporary adjustment", "error", err, "member", member.Name)
		}
	}
}

// analyzeCellularConditions analyzes cellular-specific conditions
func (hde *HybridDecisionEngine) analyzeCellularConditions(member *pkg.Member, metrics *pkg.Metrics) {
	// Check signal strength (using RSRP as signal strength indicator)
	if metrics.RSRP != nil && float64(*metrics.RSRP) < hde.cellularSignalThreshold {
		reason := fmt.Sprintf("Cellular signal weak: %.1f dBm < %.1f dBm",
			*metrics.RSRP, hde.cellularSignalThreshold)

		originalWeight := hde.hybridWeightMgr.GetOriginalWeight(member.Name)
		penaltyWeight := originalWeight - 15 // Penalty for weak signal
		if penaltyWeight < 10 {
			penaltyWeight = 10
		}

		if err := hde.hybridWeightMgr.ApplyTemporaryAdjustment(
			member.Name,
			penaltyWeight,
			reason,
			discovery.AdjustmentTypePenalty,
			hde.temporaryAdjustmentDuration,
		); err != nil {
			hde.logger.Warn("Failed to apply temporary adjustment", "error", err, "member", member.Name)
		}
	}

	// Check for roaming
	if metrics.Roaming != nil && *metrics.Roaming {
		reason := "Cellular roaming detected - may incur extra costs"

		originalWeight := hde.hybridWeightMgr.GetOriginalWeight(member.Name)
		penaltyWeight := originalWeight - 25 // Larger penalty for roaming
		if penaltyWeight < 5 {
			penaltyWeight = 5
		}

		if err := hde.hybridWeightMgr.ApplyTemporaryAdjustment(
			member.Name,
			penaltyWeight,
			reason,
			discovery.AdjustmentTypePenalty,
			hde.emergencyAdjustmentDuration, // Longer duration for cost concerns
		); err != nil {
			hde.logger.Warn("Failed to apply temporary adjustment", "error", err, "member", member.Name)
		}
	}

	// Boost cellular if it has excellent signal and Starlink is having issues
	if metrics.RSRP != nil && *metrics.RSRP > -70 {
		// Check if Starlink is having problems
		starlinkHasIssues := hde.checkStarlinkIssues()
		if starlinkHasIssues {
			reason := fmt.Sprintf("Boosting cellular due to excellent signal (%.1f dBm) while Starlink has issues",
				*metrics.RSRP)

			originalWeight := hde.hybridWeightMgr.GetOriginalWeight(member.Name)
			boostWeight := originalWeight + 15 // Boost for excellent signal
			if boostWeight > 95 {              // Don't exceed Starlink's typical weight
				boostWeight = 95
			}

			if err := hde.hybridWeightMgr.ApplyTemporaryAdjustment(
				member.Name,
				boostWeight,
				reason,
				discovery.AdjustmentTypeBoost,
				hde.temporaryAdjustmentDuration,
			); err != nil {
				hde.logger.Warn("Failed to apply temporary adjustment", "error", err, "member", member.Name)
			}
		}
	}
}

// analyzeWiFiConditions analyzes WiFi-specific conditions
func (hde *HybridDecisionEngine) analyzeWiFiConditions(member *pkg.Member, metrics *pkg.Metrics) {
	// Check WiFi signal strength
	if metrics.SignalStrength != nil && *metrics.SignalStrength < -80 {
		reason := fmt.Sprintf("WiFi signal weak: %d dBm", *metrics.SignalStrength)

		originalWeight := hde.hybridWeightMgr.GetOriginalWeight(member.Name)
		penaltyWeight := originalWeight - 10
		if penaltyWeight < 5 {
			penaltyWeight = 5
		}

		if err := hde.hybridWeightMgr.ApplyTemporaryAdjustment(
			member.Name,
			penaltyWeight,
			reason,
			discovery.AdjustmentTypePenalty,
			hde.temporaryAdjustmentDuration,
		); err != nil {
			hde.logger.Warn("Failed to apply temporary adjustment", "error", err, "member", member.Name)
		}
	}
}

// analyzeLANConditions analyzes LAN-specific conditions
func (hde *HybridDecisionEngine) analyzeLANConditions(member *pkg.Member, metrics *pkg.Metrics) {
	// LAN should have very low latency and no loss
	if metrics.LatencyMS != nil && *metrics.LatencyMS > 50.0 {
		reason := fmt.Sprintf("LAN latency unusually high: %.1f ms", *metrics.LatencyMS)

		originalWeight := hde.hybridWeightMgr.GetOriginalWeight(member.Name)
		penaltyWeight := originalWeight - 20 // Significant penalty for LAN issues
		if penaltyWeight < 20 {
			penaltyWeight = 20
		}

		if err := hde.hybridWeightMgr.ApplyTemporaryAdjustment(
			member.Name,
			penaltyWeight,
			reason,
			discovery.AdjustmentTypePenalty,
			hde.temporaryAdjustmentDuration,
		); err != nil {
			hde.logger.Warn("Failed to apply temporary adjustment", "error", err, "member", member.Name)
		}
	}
}

// analyzeGeneralPerformance applies general performance-based adjustments
func (hde *HybridDecisionEngine) analyzeGeneralPerformance(member *pkg.Member, metrics *pkg.Metrics) {
	// Check for high latency
	if metrics.LatencyMS != nil && *metrics.LatencyMS > hde.latencyDegradationThreshold {
		reason := fmt.Sprintf("High latency detected: %.1f ms > %.1f ms",
			*metrics.LatencyMS, hde.latencyDegradationThreshold)

		originalWeight := hde.hybridWeightMgr.GetOriginalWeight(member.Name)
		penaltyWeight := originalWeight - 15
		if penaltyWeight < 10 {
			penaltyWeight = 10
		}

		hde.hybridWeightMgr.ApplyTemporaryAdjustment(
			member.Name,
			penaltyWeight,
			reason,
			discovery.AdjustmentTypePenalty,
			hde.temporaryAdjustmentDuration,
		)
	}

	// Check for packet loss
	if metrics.LossPercent != nil && *metrics.LossPercent > hde.lossThreshold {
		reason := fmt.Sprintf("High packet loss: %.1f%% > %.1f%%",
			*metrics.LossPercent, hde.lossThreshold)

		originalWeight := hde.hybridWeightMgr.GetOriginalWeight(member.Name)
		penaltyWeight := originalWeight - 20 // Significant penalty for loss
		if penaltyWeight < 5 {
			penaltyWeight = 5
		}

		if err := hde.hybridWeightMgr.ApplyTemporaryAdjustment(
			member.Name,
			penaltyWeight,
			reason,
			discovery.AdjustmentTypePenalty,
			hde.emergencyAdjustmentDuration, // Longer duration for loss issues
		); err != nil {
			hde.logger.Warn("Failed to apply temporary adjustment", "error", err, "member", member.Name)
		}
	}
}

// checkStarlinkIssues checks if Starlink is currently having issues
func (hde *HybridDecisionEngine) checkStarlinkIssues() bool {
	now := time.Now()

	for name, member := range hde.members {
		if member.Class != pkg.MemberClassStarlink {
			continue
		}

		// Check if Starlink has active adjustments (penalties)
		adjustments := hde.hybridWeightMgr.GetActiveAdjustments()
		if adj, exists := adjustments[name]; exists && adj.Type == discovery.AdjustmentTypePenalty {
			return true
		}

		// Check recent metrics
		samples, err := hde.telemetry.GetSamples(name, now.Add(-2*time.Minute))
		if err != nil || len(samples) == 0 {
			continue
		}

		latestSample := samples[len(samples)-1]
		if latestSample.Metrics == nil {
			continue
		}

		// Check for current issues
		if latestSample.Metrics != nil {
			if (latestSample.Metrics.ObstructionPct != nil && *latestSample.Metrics.ObstructionPct > hde.starlinkObstructionThreshold) ||
				(latestSample.Metrics.Outages != nil && *latestSample.Metrics.Outages > 2) ||
				(latestSample.Metrics.LatencyMS != nil && *latestSample.Metrics.LatencyMS > 1000.0) ||
				(latestSample.Metrics.LossPercent != nil && *latestSample.Metrics.LossPercent > 3.0) {
				return true
			}
		}
	}

	return false
}

// GetWeightAdjustmentSummary returns a summary of current weight adjustments
func (hde *HybridDecisionEngine) GetWeightAdjustmentSummary() map[string]interface{} {
	return hde.hybridWeightMgr.GetWeightSummary()
}

// SetAdjustmentThresholds allows configuration of adjustment thresholds
func (hde *HybridDecisionEngine) SetAdjustmentThresholds(
	starlinkObstruction, cellularSignal, latencyDegradation, loss float64,
	temporaryDuration, emergencyDuration time.Duration,
) {
	hde.starlinkObstructionThreshold = starlinkObstruction
	hde.cellularSignalThreshold = cellularSignal
	hde.latencyDegradationThreshold = latencyDegradation
	hde.lossThreshold = loss
	hde.temporaryAdjustmentDuration = temporaryDuration
	hde.emergencyAdjustmentDuration = emergencyDuration

	hde.logger.Info("Updated intelligent adjustment thresholds",
		"starlink_obstruction", starlinkObstruction,
		"cellular_signal", cellularSignal,
		"latency_degradation", latencyDegradation,
		"loss", loss,
		"temporary_duration", temporaryDuration,
		"emergency_duration", emergencyDuration)
}
