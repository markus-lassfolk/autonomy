package discovery

import (
	"context"
	"fmt"
	"os/exec"
	"strconv"
	"strings"
	"time"

	"github.com/markus-lassfolk/autonomy/pkg"
	"github.com/markus-lassfolk/autonomy/pkg/logx"
)

// HybridWeightManager manages the hybrid weight system that respects MWAN3 weights
// while adding intelligent adjustments
type HybridWeightManager struct {
	logger *logx.Logger

	// Original MWAN3 weights (user-configured)
	originalWeights map[string]int

	// Current effective weights (with adjustments)
	effectiveWeights map[string]int

	// Temporary adjustments
	adjustments map[string]*WeightAdjustment

	// Configuration
	respectUserWeights bool
	dynamicAdjustment  bool
	restoreTimeout     time.Duration
	emergencyOverride  bool
}

// WeightAdjustment represents a temporary weight adjustment
type WeightAdjustment struct {
	MemberName     string
	OriginalWeight int
	AdjustedWeight int
	Reason         string
	AppliedAt      time.Time
	ExpiresAt      time.Time
	Type           AdjustmentType
}

// AdjustmentType defines the type of weight adjustment
type AdjustmentType string

const (
	AdjustmentTypeBoost     AdjustmentType = "boost"     // Temporary increase
	AdjustmentTypePenalty   AdjustmentType = "penalty"   // Temporary decrease
	AdjustmentTypeEmergency AdjustmentType = "emergency" // Emergency override
)

// NewHybridWeightManager creates a new hybrid weight manager
func NewHybridWeightManager(logger *logx.Logger) *HybridWeightManager {
	return &HybridWeightManager{
		logger:             logger,
		originalWeights:    make(map[string]int),
		effectiveWeights:   make(map[string]int),
		adjustments:        make(map[string]*WeightAdjustment),
		respectUserWeights: true,            // Default: respect user weights
		dynamicAdjustment:  true,            // Default: allow dynamic adjustments
		restoreTimeout:     5 * time.Minute, // Default: restore after 5 minutes
		emergencyOverride:  true,            // Default: allow emergency overrides
	}
}

// LoadOriginalWeights loads the original MWAN3 weights from UCI configuration
func (hwm *HybridWeightManager) LoadOriginalWeights() error {
	ctx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
	defer cancel()

	// Read mwan3 configuration
	cmd := exec.CommandContext(ctx, "uci", "show", "mwan3")
	output, err := cmd.Output()
	if err != nil {
		return fmt.Errorf("failed to read mwan3 config: %w", err)
	}

	hwm.originalWeights = make(map[string]int)
	hwm.effectiveWeights = make(map[string]int)

	lines := strings.Split(string(output), "\n")
	for _, line := range lines {
		line = strings.TrimSpace(line)
		if line == "" {
			continue
		}

		// Parse weight configurations
		if strings.Contains(line, ".weight=") {
			parts := strings.Split(line, "=")
			if len(parts) == 2 {
				memberName := strings.Split(strings.Split(line, ".")[1], ".weight")[0]
				if weight, err := strconv.Atoi(strings.Trim(parts[1], "'\"")); err == nil {
					hwm.originalWeights[memberName] = weight
					hwm.effectiveWeights[memberName] = weight
					hwm.logger.Info("Loaded original MWAN3 weight",
						"member", memberName,
						"weight", weight)
				}
			}
		}
	}

	hwm.logger.Info("Loaded original MWAN3 weights",
		"count", len(hwm.originalWeights),
		"weights", hwm.originalWeights)

	return nil
}

// GetEffectiveWeight returns the current effective weight for a member
func (hwm *HybridWeightManager) GetEffectiveWeight(memberName string) int {
	// Clean up expired adjustments first
	hwm.cleanupExpiredAdjustments()

	if weight, exists := hwm.effectiveWeights[memberName]; exists {
		return weight
	}

	// Fallback to class-based defaults if not in MWAN3 config
	return hwm.getClassBasedWeight(memberName)
}

// GetOriginalWeight returns the original MWAN3 weight for a member
func (hwm *HybridWeightManager) GetOriginalWeight(memberName string) int {
	if weight, exists := hwm.originalWeights[memberName]; exists {
		return weight
	}
	return 0
}

// ApplyTemporaryAdjustment applies a temporary weight adjustment
func (hwm *HybridWeightManager) ApplyTemporaryAdjustment(memberName string, newWeight int, reason string, adjustmentType AdjustmentType, duration time.Duration) error {
	if !hwm.dynamicAdjustment && adjustmentType != AdjustmentTypeEmergency {
		hwm.logger.Info("Dynamic adjustment disabled, skipping",
			"member", memberName,
			"reason", reason)
		return nil
	}

	if !hwm.emergencyOverride && adjustmentType == AdjustmentTypeEmergency {
		hwm.logger.Info("Emergency override disabled, skipping",
			"member", memberName,
			"reason", reason)
		return nil
	}

	originalWeight := hwm.GetOriginalWeight(memberName)
	if originalWeight == 0 {
		originalWeight = hwm.effectiveWeights[memberName]
	}

	adjustment := &WeightAdjustment{
		MemberName:     memberName,
		OriginalWeight: originalWeight,
		AdjustedWeight: newWeight,
		Reason:         reason,
		AppliedAt:      time.Now(),
		ExpiresAt:      time.Now().Add(duration),
		Type:           adjustmentType,
	}

	hwm.adjustments[memberName] = adjustment
	hwm.effectiveWeights[memberName] = newWeight

	hwm.logger.Info("Applied temporary weight adjustment",
		"member", memberName,
		"original_weight", originalWeight,
		"new_weight", newWeight,
		"reason", reason,
		"type", string(adjustmentType),
		"duration", duration,
		"expires_at", adjustment.ExpiresAt)

	return nil
}

// RestoreOriginalWeight restores the original MWAN3 weight for a member
func (hwm *HybridWeightManager) RestoreOriginalWeight(memberName string) {
	if adjustment, exists := hwm.adjustments[memberName]; exists {
		hwm.effectiveWeights[memberName] = adjustment.OriginalWeight
		delete(hwm.adjustments, memberName)

		hwm.logger.Info("Restored original weight",
			"member", memberName,
			"weight", adjustment.OriginalWeight,
			"was_adjusted_for", adjustment.Reason)
	}
}

// RestoreAllOriginalWeights restores all members to their original MWAN3 weights
func (hwm *HybridWeightManager) RestoreAllOriginalWeights() {
	for memberName := range hwm.adjustments {
		hwm.RestoreOriginalWeight(memberName)
	}
	hwm.logger.Info("Restored all members to original MWAN3 weights")
}

// cleanupExpiredAdjustments removes expired weight adjustments
func (hwm *HybridWeightManager) cleanupExpiredAdjustments() {
	now := time.Now()
	for memberName, adjustment := range hwm.adjustments {
		if now.After(adjustment.ExpiresAt) {
			hwm.RestoreOriginalWeight(memberName)
			hwm.logger.Info("Weight adjustment expired, restored original",
				"member", memberName,
				"original_weight", adjustment.OriginalWeight,
				"was_reason", adjustment.Reason)
		}
	}
}

// GetActiveAdjustments returns all currently active weight adjustments
func (hwm *HybridWeightManager) GetActiveAdjustments() map[string]*WeightAdjustment {
	hwm.cleanupExpiredAdjustments()

	active := make(map[string]*WeightAdjustment)
	for name, adj := range hwm.adjustments {
		active[name] = adj
	}
	return active
}

// UpdateMemberWithHybridWeight updates a member with the hybrid weight system
func (hwm *HybridWeightManager) UpdateMemberWithHybridWeight(member *pkg.Member) {
	// Get effective weight (respects MWAN3 + adjustments)
	effectiveWeight := hwm.GetEffectiveWeight(member.Name)
	originalWeight := hwm.GetOriginalWeight(member.Name)

	// Update member weight
	member.Weight = effectiveWeight

	// Add metadata about weight source
	if member.Config == nil {
		member.Config = make(map[string]string)
	}

	member.Config["weight_source"] = "hybrid"
	member.Config["original_mwan3_weight"] = fmt.Sprintf("%d", originalWeight)
	member.Config["effective_weight"] = fmt.Sprintf("%d", effectiveWeight)

	// Add adjustment info if active
	if adjustment, exists := hwm.adjustments[member.Name]; exists {
		member.Config["weight_adjusted"] = "true"
		member.Config["adjustment_reason"] = adjustment.Reason
		member.Config["adjustment_type"] = string(adjustment.Type)
		member.Config["adjustment_expires"] = adjustment.ExpiresAt.Format(time.RFC3339)
	} else {
		member.Config["weight_adjusted"] = "false"
	}

	hwm.logger.Debug("Updated member with hybrid weight",
		"member", member.Name,
		"original_weight", originalWeight,
		"effective_weight", effectiveWeight,
		"has_adjustment", member.Config["weight_adjusted"])
}

// getClassBasedWeight returns a fallback weight based on member class
func (hwm *HybridWeightManager) getClassBasedWeight(memberName string) int {
	// This is a fallback for members not in MWAN3 config
	// In practice, this should rarely be used since we prioritize MWAN3 discovery
	return 50 // Neutral default weight
}

// SetConfiguration updates the hybrid weight manager configuration
func (hwm *HybridWeightManager) SetConfiguration(respectUserWeights, dynamicAdjustment, emergencyOverride bool, restoreTimeout time.Duration) {
	hwm.respectUserWeights = respectUserWeights
	hwm.dynamicAdjustment = dynamicAdjustment
	hwm.emergencyOverride = emergencyOverride
	hwm.restoreTimeout = restoreTimeout

	hwm.logger.Info("Updated hybrid weight configuration",
		"respect_user_weights", respectUserWeights,
		"dynamic_adjustment", dynamicAdjustment,
		"emergency_override", emergencyOverride,
		"restore_timeout", restoreTimeout)
}

// GetConfiguration returns the current configuration
func (hwm *HybridWeightManager) GetConfiguration() (bool, bool, bool, time.Duration) {
	return hwm.respectUserWeights, hwm.dynamicAdjustment, hwm.emergencyOverride, hwm.restoreTimeout
}

// ShouldUseHybridWeights determines if hybrid weights should be used
func (hwm *HybridWeightManager) ShouldUseHybridWeights() bool {
	return hwm.respectUserWeights && len(hwm.originalWeights) > 0
}

// SetOriginalWeightForTesting sets an original weight for testing purposes
func (hwm *HybridWeightManager) SetOriginalWeightForTesting(memberName string, weight int) {
	hwm.originalWeights[memberName] = weight
	hwm.effectiveWeights[memberName] = weight
}

// GetWeightSummary returns a summary of all weights for logging/debugging
func (hwm *HybridWeightManager) GetWeightSummary() map[string]interface{} {
	hwm.cleanupExpiredAdjustments()

	summary := map[string]interface{}{
		"respect_user_weights": hwm.respectUserWeights,
		"dynamic_adjustment":   hwm.dynamicAdjustment,
		"emergency_override":   hwm.emergencyOverride,
		"restore_timeout":      hwm.restoreTimeout.String(),
		"original_weights":     hwm.originalWeights,
		"effective_weights":    hwm.effectiveWeights,
		"active_adjustments":   len(hwm.adjustments),
	}

	if len(hwm.adjustments) > 0 {
		adjustmentSummary := make(map[string]interface{})
		for name, adj := range hwm.adjustments {
			adjustmentSummary[name] = map[string]interface{}{
				"original": adj.OriginalWeight,
				"adjusted": adj.AdjustedWeight,
				"reason":   adj.Reason,
				"type":     string(adj.Type),
				"expires":  adj.ExpiresAt.Format(time.RFC3339),
			}
		}
		summary["adjustments"] = adjustmentSummary
	}

	return summary
}
