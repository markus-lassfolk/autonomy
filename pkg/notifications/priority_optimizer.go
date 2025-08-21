package notifications

import (
	"math"
	"time"

	"github.com/markus-lassfolk/autonomy/pkg/logx"
)

// PriorityOptimizer optimizes notification priorities based on context and learning
type PriorityOptimizer struct {
	config *IntelligenceConfig
	logger *logx.Logger
}

// NewPriorityOptimizer creates a new priority optimizer
func NewPriorityOptimizer(config *IntelligenceConfig, logger *logx.Logger) *PriorityOptimizer {
	return &PriorityOptimizer{
		config: config,
		logger: logger,
	}
}

// OptimizePriority determines the optimal priority for a notification
func (po *PriorityOptimizer) OptimizePriority(
	alertType AlertType,
	baseData map[string]interface{},
	systemState *SystemState,
	learningData *LearningData,
) int {
	// Start with base priority from data or default
	basePriority := PriorityNormal
	if priority, ok := baseData["priority"].(int); ok {
		basePriority = priority
	}

	// Calculate context-based adjustments
	contextScore := po.calculateContextScore(alertType, baseData, systemState)

	// Apply learning-based adjustments
	learningScore := po.calculateLearningScore(alertType, baseData, learningData)

	// Calculate urgency score
	urgencyScore := po.calculateUrgencyScore(alertType, baseData, systemState)

	// Calculate business impact score
	businessScore := po.calculateBusinessImpactScore(alertType, baseData, systemState)

	// Combine scores to determine optimal priority
	optimizedPriority := po.combinePriorityScores(
		basePriority,
		contextScore,
		learningScore,
		urgencyScore,
		businessScore,
	)

	po.logger.Debug("Priority optimization",
		"alert_type", alertType,
		"base_priority", basePriority,
		"context_score", contextScore,
		"learning_score", learningScore,
		"urgency_score", urgencyScore,
		"business_score", businessScore,
		"optimized_priority", optimizedPriority)

	return optimizedPriority
}

// calculateContextScore calculates priority adjustment based on current context
func (po *PriorityOptimizer) calculateContextScore(
	alertType AlertType,
	baseData map[string]interface{},
	systemState *SystemState,
) float64 {
	score := 0.0

	// System health context
	if systemState.SystemHealth != nil {
		health := systemState.SystemHealth

		// High CPU/Memory usage increases priority
		if health.CPUUsage > 80 {
			score += 0.3
		} else if health.CPUUsage > 60 {
			score += 0.1
		}

		if health.MemoryUsage > 90 {
			score += 0.4
		} else if health.MemoryUsage > 70 {
			score += 0.2
		}

		// High temperature increases priority
		if health.Temperature > 70 {
			score += 0.3
		} else if health.Temperature > 60 {
			score += 0.1
		}
	}

	// Network health context
	if systemState.NetworkHealth != nil {
		network := systemState.NetworkHealth

		// Primary interface down significantly increases priority
		if !network.PrimaryInterfaceUp {
			score += 0.5
		}

		// Few backup interfaces increases priority
		if network.BackupInterfacesUp < 2 {
			score += 0.2
		}

		// High packet loss increases priority
		if network.AveragePacketLoss > 10 {
			score += 0.3
		} else if network.AveragePacketLoss > 5 {
			score += 0.1
		}
	}

	// Active incidents context
	if len(systemState.ActiveIncidents) > 0 {
		// Multiple incidents increase priority
		score += float64(len(systemState.ActiveIncidents)) * 0.1

		// High severity incidents increase priority more
		for _, incident := range systemState.ActiveIncidents {
			if incident.Severity >= int(EmergencyHigh) {
				score += 0.2
			}
		}
	}

	// Time-based context
	if !systemState.BusinessHours {
		// Non-business hours slightly reduce priority for non-critical alerts
		if alertType != AlertFailover && alertType != AlertSystemHealth {
			score -= 0.1
		}
	}

	// Maintenance mode context
	if systemState.MaintenanceMode {
		// Reduce priority during maintenance unless it's critical
		if alertType != AlertSystemHealth && alertType != AlertThermal {
			score -= 0.3
		}
	}

	return score
}

// calculateLearningScore calculates priority adjustment based on learned patterns
func (po *PriorityOptimizer) calculateLearningScore(
	alertType AlertType,
	baseData map[string]interface{},
	learningData *LearningData,
) float64 {
	if !po.config.LearningEnabled {
		return 0.0
	}

	learningData.mu.RLock()
	defer learningData.mu.RUnlock()

	score := 0.0

	// Check learned optimal priorities
	if optimalPriority, exists := learningData.OptimalPriorities[string(alertType)]; exists {
		// Adjust towards learned optimal priority
		currentPriority := PriorityNormal
		if priority, ok := baseData["priority"].(int); ok {
			currentPriority = priority
		}

		diff := optimalPriority - currentPriority
		score += float64(diff) * 0.2 // 20% weight for learned priorities
	}

	// Check historical patterns
	for _, pattern := range learningData.NotificationPatterns {
		if pattern.AlertType == alertType {
			// If this pattern was effective, slightly increase priority
			if pattern.EffectivenessScore > 0.8 {
				score += 0.1
			} else if pattern.EffectivenessScore < 0.3 {
				score -= 0.1
			}
		}
	}

	// Check confidence scores
	if confidence, exists := learningData.ConfidenceScores[string(alertType)]; exists {
		if confidence > po.config.ConfidenceThreshold {
			// High confidence in learned behavior
			score *= 1.2
		} else {
			// Low confidence, reduce learning influence
			score *= 0.5
		}
	}

	return score
}

// calculateUrgencyScore calculates priority adjustment based on urgency indicators
func (po *PriorityOptimizer) calculateUrgencyScore(
	alertType AlertType,
	baseData map[string]interface{},
	systemState *SystemState,
) float64 {
	score := 0.0

	// Alert type specific urgency
	switch alertType {
	case AlertFailover:
		score += 0.4 // Failovers are inherently urgent

		// Multiple recent failovers increase urgency
		if count, ok := baseData["recent_failover_count"].(int); ok {
			score += float64(count) * 0.1
		}

	case AlertSystemHealth:
		score += 0.3 // System health issues are urgent

		// Critical severity increases urgency
		if severity, ok := baseData["severity"].(string); ok {
			switch severity {
			case "critical":
				score += 0.5
			case "high":
				score += 0.3
			case "medium":
				score += 0.1
			}
		}

	case AlertThermal:
		score += 0.4 // Thermal issues can cause damage

		// High temperature increases urgency
		if temp, ok := baseData["temperature"].(float64); ok {
			if temp > 80 {
				score += 0.4
			} else if temp > 70 {
				score += 0.2
			}
		}

	case AlertDataLimit:
		// Data limit urgency depends on usage
		if usage, ok := baseData["usage_percent"].(float64); ok {
			if usage > 95 {
				score += 0.3
			} else if usage > 85 {
				score += 0.1
			}
		}

	case AlertPredictive:
		// Predictive alerts urgency depends on confidence and time
		if confidence, ok := baseData["confidence"].(float64); ok {
			score += confidence / 100.0 * 0.3 // Max 0.3 for 100% confidence
		}

		if timeToFailure, ok := baseData["time_to_failure"].(time.Duration); ok {
			// More urgent if failure is imminent
			if timeToFailure < 5*time.Minute {
				score += 0.4
			} else if timeToFailure < 15*time.Minute {
				score += 0.2
			} else if timeToFailure < 1*time.Hour {
				score += 0.1
			}
		}
	}

	// Recent failure history increases urgency
	recentFailures := len(systemState.RecentFailures)
	if recentFailures > 5 {
		score += 0.2
	} else if recentFailures > 2 {
		score += 0.1
	}

	// Duration of ongoing issues increases urgency
	if duration, ok := baseData["duration"].(time.Duration); ok {
		if duration > 30*time.Minute {
			score += 0.3
		} else if duration > 10*time.Minute {
			score += 0.1
		}
	}

	return score
}

// calculateBusinessImpactScore calculates priority adjustment based on business impact
func (po *PriorityOptimizer) calculateBusinessImpactScore(
	alertType AlertType,
	baseData map[string]interface{},
	systemState *SystemState,
) float64 {
	score := 0.0

	// Business hours increase priority for business-critical alerts
	if systemState.BusinessHours {
		switch alertType {
		case AlertFailover, AlertConnectivityIssue:
			score += 0.2 // Network issues during business hours
		case AlertSystemHealth:
			score += 0.1 // System issues during business hours
		}
	}

	// Multiple affected systems increase business impact
	if affectedSystems, ok := baseData["affected_systems"].([]string); ok {
		impact := float64(len(affectedSystems)) * 0.05
		score += math.Min(impact, 0.3) // Cap at 0.3
	}

	// Service availability impact
	if availability, ok := baseData["service_availability"].(float64); ok {
		if availability < 0.9 { // Less than 90% availability
			score += (0.9 - availability) * 2.0 // Scale impact
		}
	}

	// User impact
	if userCount, ok := baseData["affected_users"].(int); ok {
		if userCount > 100 {
			score += 0.3
		} else if userCount > 10 {
			score += 0.1
		}
	}

	// Financial impact
	if cost, ok := baseData["estimated_cost"].(float64); ok {
		if cost > 1000 {
			score += 0.3
		} else if cost > 100 {
			score += 0.1
		}
	}

	return score
}

// combinePriorityScores combines all priority scores to determine final priority
func (po *PriorityOptimizer) combinePriorityScores(
	basePriority int,
	contextScore, learningScore, urgencyScore, businessScore float64,
) int {
	// Combine scores with weights
	totalAdjustment := contextScore*0.3 + learningScore*0.2 + urgencyScore*0.4 + businessScore*0.1

	// Apply adaptation rate
	totalAdjustment *= po.config.AdaptationRate

	// Convert to priority adjustment
	priorityAdjustment := int(math.Round(totalAdjustment * 2)) // Scale to priority levels

	// Calculate final priority
	finalPriority := basePriority + priorityAdjustment

	// Clamp to valid priority range
	if finalPriority < PriorityLowest {
		finalPriority = PriorityLowest
	} else if finalPriority > PriorityEmergency {
		finalPriority = PriorityEmergency
	}

	return finalPriority
}

// GetOptimizationStats returns priority optimization statistics
func (po *PriorityOptimizer) GetOptimizationStats() map[string]interface{} {
	return map[string]interface{}{
		"optimization_enabled": po.config.PriorityOptimizationEnabled,
		"learning_enabled":     po.config.LearningEnabled,
		"adaptation_rate":      po.config.AdaptationRate,
		"confidence_threshold": po.config.ConfidenceThreshold,
	}
}
