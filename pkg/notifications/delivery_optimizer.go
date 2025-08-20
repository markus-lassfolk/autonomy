package notifications

import (
	"fmt"
	"time"

	"github.com/markus-lassfolk/autonomy/pkg/logx"
)

// DeliveryOptimizer optimizes notification delivery timing based on context and user behavior
type DeliveryOptimizer struct {
	config *IntelligenceConfig
	logger *logx.Logger
}

// DeliveryPlan represents an optimized delivery plan for a notification
type DeliveryPlan struct {
	DelayDelivery   bool          `json:"delay_delivery"`
	OptimalTime     time.Time     `json:"optimal_time"`
	Reason          string        `json:"reason"`
	Confidence      float64       `json:"confidence"`
	EstimatedDelay  time.Duration `json:"estimated_delay"`
	AlternativeTime *time.Time    `json:"alternative_time,omitempty"`
}

// NewDeliveryOptimizer creates a new delivery optimizer
func NewDeliveryOptimizer(config *IntelligenceConfig, logger *logx.Logger) *DeliveryOptimizer {
	return &DeliveryOptimizer{
		config: config,
		logger: logger,
	}
}

// OptimizeDelivery determines the optimal delivery timing for a notification
func (do *DeliveryOptimizer) OptimizeDelivery(
	alertType AlertType,
	baseData map[string]interface{},
	systemState *SystemState,
	learningData *LearningData,
) *DeliveryPlan {
	now := time.Now()

	plan := &DeliveryPlan{
		DelayDelivery:  false,
		OptimalTime:    now,
		Confidence:     1.0,
		EstimatedDelay: 0,
	}

	// Never delay emergency notifications
	priority := PriorityNormal
	if p, ok := baseData["priority"].(int); ok {
		priority = p
	}

	if priority >= PriorityEmergency {
		plan.Reason = "Emergency priority - immediate delivery"
		return plan
	}

	// Check for bypass flags
	if bypass, ok := baseData["bypass_delivery_optimization"].(bool); ok && bypass {
		plan.Reason = "Delivery optimization bypassed"
		return plan
	}

	// Calculate optimal delivery time based on various factors
	optimalTime := do.calculateOptimalDeliveryTime(alertType, baseData, systemState, learningData)

	// Determine if we should delay delivery
	if optimalTime.After(now) {
		delay := optimalTime.Sub(now)

		// Only delay if the improvement is significant and delay is reasonable
		if do.shouldDelayDelivery(alertType, priority, delay, systemState) {
			plan.DelayDelivery = true
			plan.OptimalTime = optimalTime
			plan.EstimatedDelay = delay
			plan.Confidence = do.calculateDeliveryConfidence(alertType, systemState, learningData)
			plan.Reason = do.generateDelayReason(alertType, delay, systemState)

			// Calculate alternative time if delay is too long
			if delay > 2*time.Hour {
				alternativeTime := do.calculateAlternativeDeliveryTime(now, optimalTime)
				plan.AlternativeTime = &alternativeTime
			}
		} else {
			plan.Reason = "Optimal time calculated but delay not justified"
		}
	} else {
		plan.Reason = "Current time is optimal for delivery"
	}

	do.logger.Debug("Delivery optimization completed",
		"alert_type", alertType,
		"priority", priority,
		"delay_delivery", plan.DelayDelivery,
		"estimated_delay", plan.EstimatedDelay,
		"confidence", plan.Confidence)

	return plan
}

// calculateOptimalDeliveryTime calculates the optimal time to deliver a notification
func (do *DeliveryOptimizer) calculateOptimalDeliveryTime(
	alertType AlertType,
	baseData map[string]interface{},
	systemState *SystemState,
	learningData *LearningData,
) time.Time {
	now := time.Now()

	// Start with current time as baseline
	optimalTime := now

	// User behavior optimization
	if userOptimalTime := do.calculateUserOptimalTime(systemState, learningData); !userOptimalTime.IsZero() {
		optimalTime = userOptimalTime
	}

	// Business hours optimization
	if businessOptimalTime := do.calculateBusinessHoursOptimalTime(alertType, now); !businessOptimalTime.IsZero() {
		// Choose the later of user optimal time and business optimal time
		if businessOptimalTime.After(optimalTime) {
			optimalTime = businessOptimalTime
		}
	}

	// Quiet hours avoidance
	if quietOptimalTime := do.calculateQuietHoursOptimalTime(now, systemState); !quietOptimalTime.IsZero() {
		// Choose the later of current optimal time and quiet hours end
		if quietOptimalTime.After(optimalTime) {
			optimalTime = quietOptimalTime
		}
	}

	// Alert type specific optimization
	if typeOptimalTime := do.calculateAlertTypeOptimalTime(alertType, now, learningData); !typeOptimalTime.IsZero() {
		// For non-urgent alerts, prefer type-specific optimal times
		priority := PriorityNormal
		if p, ok := baseData["priority"].(int); ok {
			priority = p
		}

		if priority <= PriorityNormal && typeOptimalTime.After(optimalTime) {
			optimalTime = typeOptimalTime
		}
	}

	// Maintenance window avoidance
	if maintenanceOptimalTime := do.calculateMaintenanceOptimalTime(now, systemState); !maintenanceOptimalTime.IsZero() {
		if maintenanceOptimalTime.After(optimalTime) {
			optimalTime = maintenanceOptimalTime
		}
	}

	return optimalTime
}

// calculateUserOptimalTime calculates optimal delivery time based on user behavior patterns
func (do *DeliveryOptimizer) calculateUserOptimalTime(
	systemState *SystemState,
	learningData *LearningData,
) time.Time {
	if systemState.UserPresence == nil || systemState.UserPresence.ResponseHistory == nil {
		return time.Time{}
	}

	now := time.Now()

	// Check learned user behavior patterns
	learningData.mu.RLock()
	defer learningData.mu.RUnlock()

	currentWeekday := int(now.Weekday())

	// Find the best time window for the user
	var bestWindow *TimeWindow
	var bestScore float64

	for _, pattern := range learningData.UserBehaviorPatterns {
		// Check if this pattern matches current day
		dayMatches := false
		for _, day := range []int{currentWeekday} { // Could expand to check multiple days
			if pattern.DayOfWeek == day {
				dayMatches = true
				break
			}
		}

		if dayMatches && pattern.Confidence > 0.5 {
			// Calculate score based on activity level and response time
			score := pattern.ActivityLevel * pattern.Confidence
			if pattern.AverageResponseTime < 5*time.Minute {
				score += 0.2 // Bonus for fast response
			}

			if score > bestScore {
				bestScore = score
				// Convert pattern to time window (simplified)
				bestWindow = &TimeWindow{
					Start:    fmt.Sprintf("%02d:00", pattern.TimeOfDay),
					End:      fmt.Sprintf("%02d:59", pattern.TimeOfDay),
					Days:     []int{pattern.DayOfWeek},
					Priority: int(score * 10),
				}
			}
		}
	}

	if bestWindow != nil {
		// Calculate next occurrence of this time window
		return do.calculateNextTimeWindow(now, bestWindow)
	}

	return time.Time{}
}

// calculateBusinessHoursOptimalTime calculates optimal time based on business hours
func (do *DeliveryOptimizer) calculateBusinessHoursOptimalTime(alertType AlertType, now time.Time) time.Time {
	// For business-relevant alerts, prefer business hours
	businessRelevant := map[AlertType]bool{
		AlertFailover:          true,
		AlertSystemHealth:      true,
		AlertConnectivityIssue: true,
		AlertDataLimit:         false, // Can be delivered anytime
		AlertObstruction:       false, // Can be delivered anytime
		AlertThermal:           true,  // Important for business operations
		AlertPredictive:        true,  // Business planning relevant
	}

	if !businessRelevant[alertType] {
		return time.Time{}
	}

	// If currently in business hours, deliver now
	hour := now.Hour()
	weekday := now.Weekday()

	if weekday >= time.Monday && weekday <= time.Friday && hour >= 9 && hour < 17 {
		return now // Already in business hours
	}

	// Calculate next business hours
	nextBusinessDay := now

	// Find next weekday
	for nextBusinessDay.Weekday() < time.Monday || nextBusinessDay.Weekday() > time.Friday {
		nextBusinessDay = nextBusinessDay.Add(24 * time.Hour)
	}

	// Set to 9 AM
	nextBusinessHours := time.Date(
		nextBusinessDay.Year(),
		nextBusinessDay.Month(),
		nextBusinessDay.Day(),
		9, 0, 0, 0,
		nextBusinessDay.Location(),
	)

	// If it's the same day but before 9 AM, use 9 AM today
	if nextBusinessDay.Day() == now.Day() && hour < 9 {
		return nextBusinessHours
	}

	// If it's the same day but after 5 PM, use 9 AM next business day
	if nextBusinessDay.Day() == now.Day() && hour >= 17 {
		nextBusinessHours = nextBusinessHours.Add(24 * time.Hour)
		// Ensure it's still a weekday
		for nextBusinessHours.Weekday() < time.Monday || nextBusinessHours.Weekday() > time.Friday {
			nextBusinessHours = nextBusinessHours.Add(24 * time.Hour)
		}
	}

	return nextBusinessHours
}

// calculateQuietHoursOptimalTime calculates optimal time avoiding quiet hours
func (do *DeliveryOptimizer) calculateQuietHoursOptimalTime(now time.Time, systemState *SystemState) time.Time {
	if systemState.UserPresence == nil || !systemState.UserPresence.QuietHoursActive {
		return time.Time{}
	}

	// Assume quiet hours are 22:00 to 08:00 (would be configurable in real implementation)
	hour := now.Hour()

	if hour >= 22 || hour < 8 {
		// We're in quiet hours, calculate when they end
		endOfQuietHours := time.Date(
			now.Year(),
			now.Month(),
			now.Day(),
			8, 0, 0, 0,
			now.Location(),
		)

		// If it's past 22:00, quiet hours end at 8 AM next day
		if hour >= 22 {
			endOfQuietHours = endOfQuietHours.Add(24 * time.Hour)
		}

		return endOfQuietHours
	}

	return time.Time{} // Not in quiet hours
}

// calculateAlertTypeOptimalTime calculates optimal time based on alert type patterns
func (do *DeliveryOptimizer) calculateAlertTypeOptimalTime(
	alertType AlertType,
	now time.Time,
	learningData *LearningData,
) time.Time {
	learningData.mu.RLock()
	defer learningData.mu.RUnlock()

	// Check learned optimal timing for this alert type
	if optimalTiming, exists := learningData.OptimalTiming[alertType]; exists {
		// Find the next optimal time window
		for _, window := range optimalTiming {
			nextTime := do.calculateNextTimeWindow(now, &window)
			if !nextTime.IsZero() && nextTime.After(now) {
				return nextTime
			}
		}
	}

	// Default alert type preferences (would be learned from data)
	defaultOptimalHours := map[AlertType][]int{
		AlertDataLimit:   {9, 10, 14, 15}, // Business hours, not too early or late
		AlertObstruction: {8, 9, 16, 17},  // When user might be able to reposition
		AlertPredictive:  {9, 10, 13, 14}, // When user can take preventive action
	}

	if hours, exists := defaultOptimalHours[alertType]; exists {
		currentHour := now.Hour()

		// Find next optimal hour
		for _, hour := range hours {
			if hour > currentHour {
				return time.Date(now.Year(), now.Month(), now.Day(), hour, 0, 0, 0, now.Location())
			}
		}

		// If no hour today, use first hour tomorrow
		if len(hours) > 0 {
			return time.Date(now.Year(), now.Month(), now.Day()+1, hours[0], 0, 0, 0, now.Location())
		}
	}

	return time.Time{}
}

// calculateMaintenanceOptimalTime calculates optimal time avoiding maintenance windows
func (do *DeliveryOptimizer) calculateMaintenanceOptimalTime(now time.Time, systemState *SystemState) time.Time {
	if systemState.MaintenanceMode {
		// If in maintenance mode, delay until maintenance is expected to end
		// This would integrate with actual maintenance scheduling system

		// For now, assume maintenance lasts 2 hours
		return now.Add(2 * time.Hour)
	}

	return time.Time{}
}

// calculateNextTimeWindow calculates the next occurrence of a time window
func (do *DeliveryOptimizer) calculateNextTimeWindow(now time.Time, window *TimeWindow) time.Time {
	// Parse start time
	var startHour, startMinute int
	if _, err := fmt.Sscanf(window.Start, "%d:%d", &startHour, &startMinute); err != nil {
		return time.Time{}
	}

	// Check if any of the window days match today or future days
	currentWeekday := int(now.Weekday())

	for daysAhead := 0; daysAhead < 7; daysAhead++ {
		checkDay := (currentWeekday + daysAhead) % 7

		for _, windowDay := range window.Days {
			if windowDay == checkDay {
				targetTime := time.Date(
					now.Year(),
					now.Month(),
					now.Day()+daysAhead,
					startHour,
					startMinute,
					0, 0,
					now.Location(),
				)

				// If it's today, make sure the time hasn't passed
				if daysAhead == 0 && targetTime.Before(now) {
					continue
				}

				return targetTime
			}
		}
	}

	return time.Time{}
}

// shouldDelayDelivery determines if delivery should be delayed based on various factors
func (do *DeliveryOptimizer) shouldDelayDelivery(
	alertType AlertType,
	priority int,
	delay time.Duration,
	systemState *SystemState,
) bool {
	// Never delay high priority or emergency notifications
	if priority >= PriorityHigh {
		return false
	}

	// Don't delay if system is in critical state
	if systemState.EmergencyLevel >= EmergencyHigh {
		return false
	}

	// Don't delay if there are active high-severity incidents
	for _, incident := range systemState.ActiveIncidents {
		if incident.Severity >= int(EmergencyHigh) {
			return false
		}
	}

	// Limit maximum delay based on alert type
	maxDelay := map[AlertType]time.Duration{
		AlertDataLimit:   4 * time.Hour,    // Can wait for business hours
		AlertObstruction: 2 * time.Hour,    // Moderate delay acceptable
		AlertPredictive:  6 * time.Hour,    // Predictive can wait longer
		AlertFailback:    30 * time.Minute, // Positive news can wait a bit
	}

	if maxAllowed, exists := maxDelay[alertType]; exists {
		if delay > maxAllowed {
			return false
		}
	} else {
		// Default maximum delay for unknown alert types
		if delay > 1*time.Hour {
			return false
		}
	}

	// Delay is reasonable
	return true
}

// calculateDeliveryConfidence calculates confidence in the delivery optimization
func (do *DeliveryOptimizer) calculateDeliveryConfidence(
	alertType AlertType,
	systemState *SystemState,
	learningData *LearningData,
) float64 {
	confidence := 0.5 // Base confidence

	// Increase confidence if we have user behavior data
	if systemState.UserPresence != nil && systemState.UserPresence.ResponseHistory != nil {
		confidence += 0.2
	}

	// Increase confidence if we have learned patterns for this alert type
	learningData.mu.RLock()
	if _, exists := learningData.OptimalTiming[alertType]; exists {
		confidence += 0.2
	}
	learningData.mu.RUnlock()

	// Increase confidence during stable system conditions
	if systemState.EmergencyLevel == EmergencyNone && len(systemState.ActiveIncidents) == 0 {
		confidence += 0.1
	}

	return confidence
}

// generateDelayReason generates a human-readable reason for delaying delivery
func (do *DeliveryOptimizer) generateDelayReason(
	alertType AlertType,
	delay time.Duration,
	systemState *SystemState,
) string {
	reasons := []string{}

	if systemState.UserPresence != nil && systemState.UserPresence.QuietHoursActive {
		reasons = append(reasons, "avoiding quiet hours")
	}

	if !systemState.BusinessHours {
		switch alertType {
		case AlertFailover, AlertSystemHealth:
			reasons = append(reasons, "waiting for business hours")
		}
	}

	if len(reasons) == 0 {
		reasons = append(reasons, "optimizing for user availability")
	}

	return fmt.Sprintf("Delaying %v for %s", delay.Round(time.Minute),
		joinReasons(reasons))
}

// calculateAlternativeDeliveryTime calculates an alternative delivery time if optimal time is too far
func (do *DeliveryOptimizer) calculateAlternativeDeliveryTime(now, optimalTime time.Time) time.Time {
	// If optimal time is more than 2 hours away, find a reasonable middle ground
	delay := optimalTime.Sub(now)

	if delay > 2*time.Hour {
		// Deliver in 1 hour as a compromise
		return now.Add(1 * time.Hour)
	}

	return optimalTime
}

// joinReasons joins multiple reasons with proper grammar
func joinReasons(reasons []string) string {
	if len(reasons) == 0 {
		return ""
	}
	if len(reasons) == 1 {
		return reasons[0]
	}
	if len(reasons) == 2 {
		return reasons[0] + " and " + reasons[1]
	}

	result := ""
	for i, reason := range reasons {
		if i == len(reasons)-1 {
			result += "and " + reason
		} else if i == 0 {
			result += reason
		} else {
			result += ", " + reason
		}
	}

	return result
}

// GetDeliveryStats returns delivery optimization statistics
func (do *DeliveryOptimizer) GetDeliveryStats() map[string]interface{} {
	return map[string]interface{}{
		"optimization_enabled": do.config.DeliveryOptimizationEnabled,
		"learning_enabled":     do.config.LearningEnabled,
	}
}
