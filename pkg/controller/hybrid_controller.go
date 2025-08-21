package controller

import (
	"context"
	"fmt"
	"os/exec"
	"time"

	"github.com/markus-lassfolk/autonomy/pkg"
	"github.com/markus-lassfolk/autonomy/pkg/discovery"
	"github.com/markus-lassfolk/autonomy/pkg/logx"
)

// HybridController implements the new hybrid approach that respects MWAN3 weights
type HybridController struct {
	*Controller
	hybridWeightMgr *discovery.HybridWeightManager
	logger          *logx.Logger

	// Configuration
	respectUserWeights    bool
	onlyEmergencyOverride bool
}

// NewHybridController creates a new hybrid controller
func NewHybridController(baseController *Controller, hybridWeightMgr *discovery.HybridWeightManager, logger *logx.Logger) *HybridController {
	return &HybridController{
		Controller:            baseController,
		hybridWeightMgr:       hybridWeightMgr,
		logger:                logger,
		respectUserWeights:    true, // Default: respect user weights
		onlyEmergencyOverride: true, // Default: only override in emergencies
	}
}

// Switch switches from one member to another using the hybrid approach
func (hc *HybridController) Switch(from, to *pkg.Member) error {
	if to == nil {
		return fmt.Errorf("target member cannot be nil")
	}

	hc.logger.Info("Hybrid switch initiated",
		"from", func() string {
			if from != nil {
				return from.Name
			}
			return "none"
		}(),
		"to", to.Name,
		"dry_run", hc.dryRun,
		"respect_user_weights", hc.respectUserWeights)

	// Validate target member
	if err := hc.Validate(to); err != nil {
		return fmt.Errorf("invalid target member: %w", err)
	}

	// Check if we need to make any MWAN3 changes
	needsChange, reason := hc.assessMWAN3ChangeNeed(from, to)

	if !needsChange {
		hc.logger.Info("No MWAN3 changes needed - relying on existing weights and health checks",
			"target", to.Name,
			"reason", reason)

		// Update current member tracking only
		hc.currentMember = to
		return nil
	}

	// Perform the switch based on available methods
	if hc.mwan3Enabled {
		return hc.hybridSwitchMWAN3(from, to, reason)
	} else {
		return hc.switchNetifd(from, to)
	}
}

// assessMWAN3ChangeNeed determines if MWAN3 configuration changes are needed
func (hc *HybridController) assessMWAN3ChangeNeed(from, to *pkg.Member) (bool, string) {
	// If we're not respecting user weights, use the old behavior
	if !hc.respectUserWeights {
		return true, "user_weights_disabled"
	}

	// Get current MWAN3 status
	status, err := hc.getMWAN3Status()
	if err != nil {
		hc.logger.Warn("Failed to get MWAN3 status, assuming change needed", "error", err)
		return true, "status_check_failed"
	}

	// Check if target interface is already online and healthy
	if hc.isInterfaceHealthyInMWAN3(to.Iface, status) {
		// Check if it has reasonable weight compared to others
		if hc.hasReasonableWeight(to) {
			return false, "target_already_healthy_with_good_weight"
		}
		return true, "target_healthy_but_weight_needs_adjustment"
	}

	// Check if target interface is offline/unhealthy
	if !hc.isInterfaceOnlineInMWAN3(to.Iface, status) {
		return true, "target_interface_offline"
	}

	// Emergency situations that require intervention
	if hc.isEmergencySituation(to, status) {
		return true, "emergency_intervention_required"
	}

	// Default: let MWAN3 handle it with existing weights
	return false, "mwan3_can_handle_with_existing_weights"
}

// hybridSwitchMWAN3 performs a hybrid MWAN3 switch that respects user weights
func (hc *HybridController) hybridSwitchMWAN3(from, to *pkg.Member, reason string) error {
	hc.logger.LogMWAN3("hybrid_switch", map[string]interface{}{
		"from": func() string {
			if from != nil {
				return from.Name
			}
			return "none"
		}(),
		"to":      to.Name,
		"reason":  reason,
		"dry_run": hc.dryRun,
	})

	// Get current mwan3 status
	status, err := hc.getMWAN3Status()
	if err != nil {
		return fmt.Errorf("failed to get mwan3 status: %w", err)
	}

	// Determine the type of intervention needed
	interventionType := hc.determineInterventionType(to, reason)

	switch interventionType {
	case "minimal_adjustment":
		return hc.applyMinimalAdjustment(to, status)
	case "temporary_boost":
		return hc.applyTemporaryBoost(to, status)
	case "emergency_override":
		return hc.applyEmergencyOverride(to, status)
	case "interface_enable":
		return hc.enableInterface(to, status)
	default:
		hc.logger.Info("No intervention needed, letting MWAN3 handle naturally",
			"target", to.Name,
			"intervention_type", interventionType)
		hc.currentMember = to
		return nil
	}
}

// determineInterventionType determines what type of intervention is needed
func (hc *HybridController) determineInterventionType(to *pkg.Member, reason string) string {
	switch reason {
	case "target_interface_offline":
		return "interface_enable"
	case "emergency_intervention_required":
		return "emergency_override"
	case "target_healthy_but_weight_needs_adjustment":
		return "minimal_adjustment"
	case "user_weights_disabled":
		return "emergency_override" // Fall back to old behavior
	default:
		return "no_intervention"
	}
}

// applyMinimalAdjustment applies a minimal weight adjustment that respects user preferences
func (hc *HybridController) applyMinimalAdjustment(to *pkg.Member, status map[string]interface{}) error {
	hc.logger.Info("Applying minimal weight adjustment", "target", to.Name)

	// Instead of overriding all weights, just ensure the target has a slight boost
	originalWeight := hc.hybridWeightMgr.GetOriginalWeight(to.Name)
	if originalWeight == 0 {
		originalWeight = to.Weight
	}

	// Apply a small temporary boost (5-10 points) rather than setting to 100
	boostWeight := originalWeight + 10
	if boostWeight > 100 {
		boostWeight = 100
	}

	// Use the hybrid weight manager to apply temporary adjustment
	err := hc.hybridWeightMgr.ApplyTemporaryAdjustment(
		to.Name,
		boostWeight,
		"Minimal boost to encourage failover",
		discovery.AdjustmentTypeBoost,
		5*time.Minute, // Short duration
	)
	if err != nil {
		return fmt.Errorf("failed to apply minimal adjustment: %w", err)
	}

	// Update MWAN3 with the new weight
	return hc.updateSingleMemberWeight(to.Name, boostWeight)
}

// applyTemporaryBoost applies a temporary boost without affecting other members
func (hc *HybridController) applyTemporaryBoost(to *pkg.Member, status map[string]interface{}) error {
	hc.logger.Info("Applying temporary boost", "target", to.Name)

	originalWeight := hc.hybridWeightMgr.GetOriginalWeight(to.Name)
	if originalWeight == 0 {
		originalWeight = to.Weight
	}

	// Apply a moderate boost (15-20 points)
	boostWeight := originalWeight + 20
	if boostWeight > 100 {
		boostWeight = 100
	}

	err := hc.hybridWeightMgr.ApplyTemporaryAdjustment(
		to.Name,
		boostWeight,
		"Temporary boost for failover",
		discovery.AdjustmentTypeBoost,
		10*time.Minute,
	)
	if err != nil {
		return fmt.Errorf("failed to apply temporary boost: %w", err)
	}

	return hc.updateSingleMemberWeight(to.Name, boostWeight)
}

// applyEmergencyOverride applies emergency override (similar to old behavior)
func (hc *HybridController) applyEmergencyOverride(to *pkg.Member, status map[string]interface{}) error {
	hc.logger.Warn("Applying emergency override - this will temporarily override user weights",
		"target", to.Name)

	if !hc.onlyEmergencyOverride {
		hc.logger.Info("Emergency override disabled, applying temporary boost instead")
		return hc.applyTemporaryBoost(to, status)
	}

	// This is the old behavior - set target to 100, others to 10
	// But mark it as temporary emergency adjustment
	err := hc.hybridWeightMgr.ApplyTemporaryAdjustment(
		to.Name,
		100,
		"Emergency override - critical situation",
		discovery.AdjustmentTypeEmergency,
		15*time.Minute,
	)
	if err != nil {
		return fmt.Errorf("failed to apply emergency override: %w", err)
	}

	// Apply the old updateMWAN3Policy logic for emergency situations
	return hc.updateMWAN3Policy(to)
}

// enableInterface enables a disabled interface
func (hc *HybridController) enableInterface(to *pkg.Member, status map[string]interface{}) error {
	hc.logger.Info("Enabling interface", "target", to.Name, "interface", to.Iface)

	if hc.dryRun {
		hc.logger.Info("DRY RUN: Would enable interface", "interface", to.Iface)
		hc.currentMember = to
		return nil
	}

	// Enable the interface via ubus
	ctx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
	defer cancel()

	cmd := exec.CommandContext(ctx, "ubus", "call", "network.interface."+to.Iface, "up")
	if err := cmd.Run(); err != nil {
		hc.logger.Warn("Failed to enable interface via ubus", "interface", to.Iface, "error", err)
	}

	// Update current member
	hc.currentMember = to
	return nil
}

// updateSingleMemberWeight updates the weight of a single member in MWAN3
func (hc *HybridController) updateSingleMemberWeight(memberName string, weight int) error {
	if hc.dryRun {
		hc.logger.Info("DRY RUN: Would update single member weight",
			"member", memberName,
			"weight", weight)
		return nil
	}

	ctx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
	defer cancel()

	// Update the specific member's weight
	cmd := exec.CommandContext(ctx, "uci", "set",
		fmt.Sprintf("mwan3.%s.weight=%d", memberName, weight))
	if err := cmd.Run(); err != nil {
		return fmt.Errorf("failed to set member weight: %w", err)
	}

	// Commit the change
	cmd = exec.CommandContext(ctx, "uci", "commit", "mwan3")
	if err := cmd.Run(); err != nil {
		return fmt.Errorf("failed to commit mwan3 config: %w", err)
	}

	// Reload mwan3
	cmd = exec.CommandContext(ctx, "mwan3", "reload")
	if err := cmd.Run(); err != nil {
		return fmt.Errorf("failed to reload mwan3: %w", err)
	}

	hc.logger.Info("Updated single member weight",
		"member", memberName,
		"weight", weight)

	return nil
}

// isInterfaceHealthyInMWAN3 checks if an interface is healthy in MWAN3
func (hc *HybridController) isInterfaceHealthyInMWAN3(iface string, status map[string]interface{}) bool {
	if interfaces, ok := status["interfaces"].(map[string]interface{}); ok {
		if ifaceData, ok := interfaces[iface].(map[string]interface{}); ok {
			if ifaceStatus, ok := ifaceData["status"].(string); ok {
				return ifaceStatus == "online"
			}
		}
	}
	return false
}

// isInterfaceOnlineInMWAN3 checks if an interface is online in MWAN3
func (hc *HybridController) isInterfaceOnlineInMWAN3(iface string, status map[string]interface{}) bool {
	if interfaces, ok := status["interfaces"].(map[string]interface{}); ok {
		if ifaceData, ok := interfaces[iface].(map[string]interface{}); ok {
			if ifaceStatus, ok := ifaceData["status"].(string); ok {
				return ifaceStatus == "online" || ifaceStatus == "tracking"
			}
		}
	}
	return false
}

// hasReasonableWeight checks if a member has a reasonable weight for its class
func (hc *HybridController) hasReasonableWeight(member *pkg.Member) bool {
	currentWeight := hc.hybridWeightMgr.GetEffectiveWeight(member.Name)
	originalWeight := hc.hybridWeightMgr.GetOriginalWeight(member.Name)

	// If it has its original weight or better, it's reasonable
	if currentWeight >= originalWeight {
		return true
	}

	// Check if it's significantly lower than it should be
	if originalWeight > 0 && currentWeight < (originalWeight/2) {
		return false
	}

	return true
}

// isEmergencySituation determines if this is an emergency that requires intervention
func (hc *HybridController) isEmergencySituation(member *pkg.Member, status map[string]interface{}) bool {
	// Check if all other interfaces are down
	onlineCount := 0
	totalCount := 0

	if interfaces, ok := status["interfaces"].(map[string]interface{}); ok {
		for _, ifaceData := range interfaces {
			totalCount++
			if ifaceMap, ok := ifaceData.(map[string]interface{}); ok {
				if ifaceStatus, ok := ifaceMap["status"].(string); ok {
					if ifaceStatus == "online" {
						onlineCount++
					}
				}
			}
		}
	}

	// Emergency if less than 50% of interfaces are online
	if totalCount > 0 && float64(onlineCount)/float64(totalCount) < 0.5 {
		return true
	}

	// Emergency if no interfaces are online
	if onlineCount == 0 {
		return true
	}

	return false
}

// SetConfiguration updates the hybrid controller configuration
func (hc *HybridController) SetConfiguration(respectUserWeights, onlyEmergencyOverride bool) {
	hc.respectUserWeights = respectUserWeights
	hc.onlyEmergencyOverride = onlyEmergencyOverride

	hc.logger.Info("Updated hybrid controller configuration",
		"respect_user_weights", respectUserWeights,
		"only_emergency_override", onlyEmergencyOverride)
}

// GetConfiguration returns the current configuration
func (hc *HybridController) GetConfiguration() (bool, bool) {
	return hc.respectUserWeights, hc.onlyEmergencyOverride
}

// RestoreUserWeights restores all members to their original user-configured weights
func (hc *HybridController) RestoreUserWeights() error {
	hc.logger.Info("Restoring all members to original user weights")

	// Get all active adjustments
	adjustments := hc.hybridWeightMgr.GetActiveAdjustments()

	if len(adjustments) == 0 {
		hc.logger.Info("No active weight adjustments to restore")
		return nil
	}

	// Restore each member to its original weight
	for memberName, adjustment := range adjustments {
		if hc.dryRun {
			hc.logger.Info("DRY RUN: Would restore member weight",
				"member", memberName,
				"from", adjustment.AdjustedWeight,
				"to", adjustment.OriginalWeight)
			continue
		}

		// Update MWAN3 with original weight
		if err := hc.updateSingleMemberWeight(memberName, adjustment.OriginalWeight); err != nil {
			hc.logger.Error("Failed to restore member weight",
				"member", memberName,
				"error", err)
			continue
		}
	}

	// Clear all adjustments from the hybrid weight manager
	hc.hybridWeightMgr.RestoreAllOriginalWeights()

	hc.logger.Info("Completed restoration of user weights",
		"restored_count", len(adjustments))

	return nil
}
