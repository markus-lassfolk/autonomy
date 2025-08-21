package notifications

import (
	"fmt"
	"sort"
	"time"

	"github.com/markus-lassfolk/autonomy/pkg/logx"
)

// ChannelIntelligence provides intelligent channel selection based on context and effectiveness
type ChannelIntelligence struct {
	config *IntelligenceConfig
	logger *logx.Logger
}

// ChannelScore represents the effectiveness score for a notification channel
type ChannelScore struct {
	Channel       NotificationChannel `json:"channel"`
	Score         float64             `json:"score"`
	Reason        string              `json:"reason"`
	Effectiveness float64             `json:"effectiveness"`
	ResponseTime  time.Duration       `json:"response_time"`
}

// NewChannelIntelligence creates a new channel intelligence system
func NewChannelIntelligence(config *IntelligenceConfig, logger *logx.Logger) *ChannelIntelligence {
	return &ChannelIntelligence{
		config: config,
		logger: logger,
	}
}

// SelectOptimalChannels selects the best channels for a notification based on context
func (ci *ChannelIntelligence) SelectOptimalChannels(
	alertType AlertType,
	baseData map[string]interface{},
	systemState *SystemState,
	learningData *LearningData,
) []NotificationChannel {
	// Score all available channels
	channelScores := ci.scoreAllChannels(alertType, baseData, systemState, learningData)

	// Sort by score (highest first)
	sort.Slice(channelScores, func(i, j int) bool {
		return channelScores[i].Score > channelScores[j].Score
	})

	// Select optimal channels based on priority and context
	selectedChannels := ci.selectChannelsFromScores(channelScores, alertType, baseData, systemState)

	ci.logger.Debug("Channel selection completed",
		"alert_type", alertType,
		"total_channels", len(channelScores),
		"selected_channels", len(selectedChannels),
		"top_channel", channelScores[0].Channel,
		"top_score", channelScores[0].Score)

	return selectedChannels
}

// scoreAllChannels scores all available notification channels
func (ci *ChannelIntelligence) scoreAllChannels(
	alertType AlertType,
	baseData map[string]interface{},
	systemState *SystemState,
	learningData *LearningData,
) []ChannelScore {
	channels := []NotificationChannel{
		ChannelPushover,
		ChannelEmail,
		ChannelSlack,
		ChannelDiscord,
		ChannelTelegram,
		ChannelWebhook,
	}

	scores := make([]ChannelScore, 0, len(channels))

	for _, channel := range channels {
		score := ci.calculateChannelScore(channel, alertType, baseData, systemState, learningData)
		scores = append(scores, score)
	}

	return scores
}

// calculateChannelScore calculates the effectiveness score for a specific channel
func (ci *ChannelIntelligence) calculateChannelScore(
	channel NotificationChannel,
	alertType AlertType,
	baseData map[string]interface{},
	systemState *SystemState,
	learningData *LearningData,
) ChannelScore {
	score := &ChannelScore{
		Channel:       channel,
		Score:         0.0,
		Effectiveness: ci.getChannelEffectiveness(channel, learningData),
		ResponseTime:  ci.getChannelResponseTime(channel, learningData),
	}

	// Base effectiveness score
	score.Score = score.Effectiveness

	// Context-based adjustments
	contextAdjustment := ci.calculateContextAdjustment(channel, alertType, baseData, systemState)
	score.Score += contextAdjustment

	// Learning-based adjustments
	learningAdjustment := ci.calculateLearningAdjustment(channel, alertType, learningData)
	score.Score += learningAdjustment

	// Time-based adjustments
	timeAdjustment := ci.calculateTimeAdjustment(channel, systemState)
	score.Score += timeAdjustment

	// Priority-based adjustments
	priorityAdjustment := ci.calculatePriorityAdjustment(channel, baseData)
	score.Score += priorityAdjustment

	// User preference adjustments
	userAdjustment := ci.calculateUserPreferenceAdjustment(channel, systemState)
	score.Score += userAdjustment

	// Generate reason for the score
	score.Reason = ci.generateScoreReason(channel, score.Score, contextAdjustment, learningAdjustment, timeAdjustment)

	// Clamp score to valid range
	if score.Score < 0 {
		score.Score = 0
	} else if score.Score > 1 {
		score.Score = 1
	}

	return *score
}

// getChannelEffectiveness gets the base effectiveness for a channel
func (ci *ChannelIntelligence) getChannelEffectiveness(channel NotificationChannel, learningData *LearningData) float64 {
	// Default effectiveness scores (would be learned from actual data)
	defaultEffectiveness := map[NotificationChannel]float64{
		ChannelPushover: 0.95, // Highest for mobile alerts
		ChannelTelegram: 0.92, // High for instant messaging
		ChannelSlack:    0.90, // High for team communication
		ChannelDiscord:  0.88, // Good for community alerts
		ChannelEmail:    0.85, // Good for detailed notifications
		ChannelWebhook:  0.80, // Variable depending on integration
	}

	// Use learned effectiveness if available
	learningData.mu.RLock()
	defer learningData.mu.RUnlock()

	// Check for learned effectiveness (would be populated by actual usage data)
	if effectiveness, exists := defaultEffectiveness[channel]; exists {
		return effectiveness
	}

	return 0.5 // Default moderate effectiveness
}

// getChannelResponseTime gets the typical response time for a channel
func (ci *ChannelIntelligence) getChannelResponseTime(channel NotificationChannel, learningData *LearningData) time.Duration {
	// Default response times (would be learned from actual data)
	defaultResponseTimes := map[NotificationChannel]time.Duration{
		ChannelPushover: 30 * time.Second, // Very fast mobile notifications
		ChannelTelegram: 45 * time.Second, // Fast instant messaging
		ChannelSlack:    1 * time.Minute,  // Fast team communication
		ChannelDiscord:  1 * time.Minute,  // Fast community alerts
		ChannelEmail:    5 * time.Minute,  // Slower email checking
		ChannelWebhook:  2 * time.Minute,  // Variable webhook processing
	}

	if responseTime, exists := defaultResponseTimes[channel]; exists {
		return responseTime
	}

	return 2 * time.Minute // Default response time
}

// calculateContextAdjustment calculates context-based score adjustments
func (ci *ChannelIntelligence) calculateContextAdjustment(
	channel NotificationChannel,
	alertType AlertType,
	baseData map[string]interface{},
	systemState *SystemState,
) float64 {
	adjustment := 0.0

	// Alert type specific preferences
	switch alertType {
	case AlertFailover, AlertSystemHealth:
		// Critical alerts prefer fast channels
		switch channel {
		case ChannelPushover, ChannelTelegram:
			adjustment += 0.2 // Prefer instant notifications
		case ChannelSlack, ChannelDiscord:
			adjustment += 0.1 // Good for team alerts
		case ChannelEmail:
			adjustment -= 0.1 // Less preferred for urgent alerts
		}

	case AlertDataLimit, AlertObstruction:
		// Informational alerts can use any channel
		switch channel {
		case ChannelEmail:
			adjustment += 0.1 // Good for detailed information
		case ChannelSlack:
			adjustment += 0.05 // Good for team awareness
		}

	case AlertPredictive:
		// Predictive alerts prefer channels that support rich content
		switch channel {
		case ChannelEmail, ChannelSlack:
			adjustment += 0.15 // Good for detailed predictions
		case ChannelWebhook:
			adjustment += 0.1 // Good for integration with monitoring systems
		}
	}

	// System state adjustments
	if systemState.MaintenanceMode {
		// During maintenance, prefer less intrusive channels
		switch channel {
		case ChannelPushover, ChannelTelegram:
			adjustment -= 0.2 // Reduce mobile notifications
		case ChannelEmail, ChannelSlack:
			adjustment += 0.1 // Prefer less intrusive channels
		}
	}

	// Business hours adjustments
	if !systemState.BusinessHours {
		// Outside business hours, prefer personal channels
		switch channel {
		case ChannelPushover, ChannelTelegram:
			adjustment += 0.1 // Personal mobile notifications
		case ChannelSlack, ChannelDiscord:
			adjustment -= 0.1 // Team channels less relevant
		}
	}

	return adjustment
}

// calculateLearningAdjustment calculates learning-based score adjustments
func (ci *ChannelIntelligence) calculateLearningAdjustment(
	channel NotificationChannel,
	alertType AlertType,
	learningData *LearningData,
) float64 {
	if !ci.config.LearningEnabled {
		return 0.0
	}

	learningData.mu.RLock()
	defer learningData.mu.RUnlock()

	adjustment := 0.0

	// Check learned optimal channels for this alert type
	if optimalChannels, exists := learningData.OptimalChannels[alertType]; exists {
		for i, optimalChannel := range optimalChannels {
			if optimalChannel == channel {
				// Higher adjustment for higher-ranked channels
				adjustment += 0.3 * (1.0 - float64(i)*0.1)
				break
			}
		}
	}

	// Check historical patterns
	for _, pattern := range learningData.NotificationPatterns {
		if pattern.AlertType == alertType {
			for _, patternChannel := range pattern.OptimalChannels {
				if patternChannel == channel {
					// Adjust based on pattern effectiveness
					adjustment += pattern.EffectivenessScore * 0.1
				}
			}
		}
	}

	return adjustment
}

// calculateTimeAdjustment calculates time-based score adjustments
func (ci *ChannelIntelligence) calculateTimeAdjustment(
	channel NotificationChannel,
	systemState *SystemState,
) float64 {
	adjustment := 0.0
	now := time.Now()
	hour := now.Hour()

	// Time of day preferences
	switch {
	case hour >= 22 || hour <= 6: // Night time
		switch channel {
		case ChannelPushover, ChannelTelegram:
			adjustment -= 0.2 // Reduce intrusive notifications at night
		case ChannelEmail:
			adjustment += 0.1 // Email is less intrusive
		}

	case hour >= 9 && hour <= 17: // Business hours
		switch channel {
		case ChannelSlack, ChannelDiscord:
			adjustment += 0.1 // Team channels more relevant
		case ChannelEmail:
			adjustment += 0.05 // Professional communication
		}

	default: // Evening hours
		// Normal preferences apply
	}

	// User presence adjustments
	if systemState.UserPresence != nil {
		if !systemState.UserPresence.IsActive {
			// User not active, prefer persistent channels
			switch channel {
			case ChannelEmail:
				adjustment += 0.15 // Email persists until read
			case ChannelPushover:
				adjustment += 0.1 // Push notifications persist
			}
		}

		if systemState.UserPresence.QuietHoursActive {
			// Quiet hours active, reduce intrusive channels
			switch channel {
			case ChannelPushover, ChannelTelegram:
				adjustment -= 0.3
			case ChannelEmail:
				adjustment += 0.2
			}
		}
	}

	return adjustment
}

// calculatePriorityAdjustment calculates priority-based score adjustments
func (ci *ChannelIntelligence) calculatePriorityAdjustment(
	channel NotificationChannel,
	baseData map[string]interface{},
) float64 {
	priority := PriorityNormal
	if p, ok := baseData["priority"].(int); ok {
		priority = p
	}

	adjustment := 0.0

	switch priority {
	case PriorityEmergency:
		// Emergency: prefer all fast, reliable channels
		switch channel {
		case ChannelPushover, ChannelTelegram:
			adjustment += 0.3 // Highest preference for instant channels
		case ChannelSlack, ChannelDiscord:
			adjustment += 0.2 // High preference for team channels
		case ChannelEmail, ChannelWebhook:
			adjustment += 0.1 // Include all channels for emergency
		}

	case PriorityHigh:
		// High priority: prefer fast channels
		switch channel {
		case ChannelPushover, ChannelTelegram:
			adjustment += 0.2
		case ChannelSlack:
			adjustment += 0.1
		}

	case PriorityLow, PriorityLowest:
		// Low priority: prefer less intrusive channels
		switch channel {
		case ChannelEmail:
			adjustment += 0.1
		case ChannelPushover, ChannelTelegram:
			adjustment -= 0.1
		}
	}

	return adjustment
}

// calculateUserPreferenceAdjustment calculates user preference-based adjustments
func (ci *ChannelIntelligence) calculateUserPreferenceAdjustment(
	channel NotificationChannel,
	systemState *SystemState,
) float64 {
	if systemState.UserPresence == nil {
		return 0.0
	}

	adjustment := 0.0

	// Check preferred channels
	for i, preferredChannel := range systemState.UserPresence.PreferredChannels {
		if preferredChannel == channel {
			// Higher adjustment for higher-ranked preferences
			adjustment += 0.2 * (1.0 - float64(i)*0.05)
			break
		}
	}

	// Check response history
	if systemState.UserPresence.ResponseHistory != nil {
		responseRate, exists := systemState.UserPresence.ResponseHistory.ResponseRate[channel]
		if exists {
			// Prefer channels with higher response rates
			adjustment += responseRate * 0.1
		}

		responseTime, exists := systemState.UserPresence.ResponseHistory.AverageResponseTime[channel]
		if exists {
			// Prefer channels with faster response times
			if responseTime < 1*time.Minute {
				adjustment += 0.1
			} else if responseTime > 10*time.Minute {
				adjustment -= 0.1
			}
		}
	}

	return adjustment
}

// generateScoreReason generates a human-readable reason for the channel score
func (ci *ChannelIntelligence) generateScoreReason(
	channel NotificationChannel,
	finalScore float64,
	contextAdjustment, learningAdjustment, timeAdjustment float64,
) string {
	reason := fmt.Sprintf("Base effectiveness: %.2f", finalScore-contextAdjustment-learningAdjustment-timeAdjustment)

	if contextAdjustment != 0 {
		reason += fmt.Sprintf(", Context: %+.2f", contextAdjustment)
	}

	if learningAdjustment != 0 {
		reason += fmt.Sprintf(", Learning: %+.2f", learningAdjustment)
	}

	if timeAdjustment != 0 {
		reason += fmt.Sprintf(", Time: %+.2f", timeAdjustment)
	}

	return reason
}

// selectChannelsFromScores selects the optimal channels from scored channels
func (ci *ChannelIntelligence) selectChannelsFromScores(
	channelScores []ChannelScore,
	alertType AlertType,
	baseData map[string]interface{},
	systemState *SystemState,
) []NotificationChannel {
	if len(channelScores) == 0 {
		return []NotificationChannel{ChannelPushover} // Fallback
	}

	// Check for forced channel selection
	if forceAll, ok := baseData["force_all_channels"].(bool); ok && forceAll {
		channels := make([]NotificationChannel, len(channelScores))
		for i, score := range channelScores {
			channels[i] = score.Channel
		}
		return channels
	}

	// Check for preferred channels override
	if preferredChannels, ok := baseData["preferred_channels"].([]NotificationChannel); ok {
		return preferredChannels
	}

	priority := PriorityNormal
	if p, ok := baseData["priority"].(int); ok {
		priority = p
	}

	// Select channels based on priority and scores
	var selectedChannels []NotificationChannel

	switch priority {
	case PriorityEmergency:
		// Emergency: use top 3 channels or all channels with score > 0.6
		for i, score := range channelScores {
			if i < 3 || score.Score > 0.6 {
				selectedChannels = append(selectedChannels, score.Channel)
			}
		}

	case PriorityHigh:
		// High: use top 2 channels or channels with score > 0.7
		for i, score := range channelScores {
			if i < 2 || score.Score > 0.7 {
				selectedChannels = append(selectedChannels, score.Channel)
			}
		}

	case PriorityNormal:
		// Normal: use top channel or channels with score > 0.8
		for i, score := range channelScores {
			if i < 1 || score.Score > 0.8 {
				selectedChannels = append(selectedChannels, score.Channel)
			}
		}

	case PriorityLow, PriorityLowest:
		// Low: use only the top channel if score > 0.5
		if channelScores[0].Score > 0.5 {
			selectedChannels = append(selectedChannels, channelScores[0].Channel)
		}
	}

	// Ensure at least one channel is selected
	if len(selectedChannels) == 0 {
		selectedChannels = append(selectedChannels, channelScores[0].Channel)
	}

	return selectedChannels
}

// GetChannelStats returns channel intelligence statistics
func (ci *ChannelIntelligence) GetChannelStats() map[string]interface{} {
	return map[string]interface{}{
		"intelligence_enabled": ci.config.ChannelIntelligenceEnabled,
		"learning_enabled":     ci.config.LearningEnabled,
	}
}
