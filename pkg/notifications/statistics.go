package notifications

import (
	"time"
)

// NewNotificationStats creates a new notification statistics tracker
func NewNotificationStats() *NotificationStats {
	return &NotificationStats{
		ByPriority:  make(map[int]int64),
		ByType:      make(map[NotificationType]int64),
		ByChannel:   make(map[NotificationChannel]int64),
		LastUpdated: time.Now(),
	}
}

// IncrementSent increments the total sent counter
func (ns *NotificationStats) IncrementSent() {
	ns.mu.Lock()
	defer ns.mu.Unlock()
	ns.TotalSent++
	ns.LastUpdated = time.Now()
}

// IncrementSuppressed increments the total suppressed counter
func (ns *NotificationStats) IncrementSuppressed() {
	ns.mu.Lock()
	defer ns.mu.Unlock()
	ns.TotalSuppressed++
	ns.LastUpdated = time.Now()
}

// IncrementFailed increments the total failed counter
func (ns *NotificationStats) IncrementFailed() {
	ns.mu.Lock()
	defer ns.mu.Unlock()
	ns.TotalFailed++
	ns.LastUpdated = time.Now()
}

// IncrementDeduped increments the total deduplicated counter
func (ns *NotificationStats) IncrementDeduped() {
	ns.mu.Lock()
	defer ns.mu.Unlock()
	ns.TotalDeduped++
	ns.LastUpdated = time.Now()
}

// IncrementRateLimited increments the rate limited counter
func (ns *NotificationStats) IncrementRateLimited() {
	ns.mu.Lock()
	defer ns.mu.Unlock()
	ns.RateLimited++
	ns.LastUpdated = time.Now()
}

// IncrementByPriority increments the counter for a specific priority
func (ns *NotificationStats) IncrementByPriority(priority int) {
	ns.mu.Lock()
	defer ns.mu.Unlock()
	ns.ByPriority[priority]++
	ns.LastUpdated = time.Now()
}

// IncrementByType increments the counter for a specific notification type
func (ns *NotificationStats) IncrementByType(notificationType NotificationType) {
	ns.mu.Lock()
	defer ns.mu.Unlock()
	ns.ByType[notificationType]++
	ns.LastUpdated = time.Now()
}

// IncrementByChannel increments the counter for a specific channel
func (ns *NotificationStats) IncrementByChannel(channel NotificationChannel) {
	ns.mu.Lock()
	defer ns.mu.Unlock()
	ns.ByChannel[channel]++
	ns.LastUpdated = time.Now()
}

// UpdateLatency updates the latency statistics
func (ns *NotificationStats) UpdateLatency(latency time.Duration) {
	ns.mu.Lock()
	defer ns.mu.Unlock()

	// Update max latency
	if latency > ns.MaxLatency {
		ns.MaxLatency = latency
	}

	// Update average latency (simple moving average)
	if ns.AverageLatency == 0 {
		ns.AverageLatency = latency
	} else {
		// Weighted average with more weight on recent measurements
		ns.AverageLatency = time.Duration(float64(ns.AverageLatency)*0.9 + float64(latency)*0.1)
	}

	ns.LastUpdated = time.Now()
}

// UpdateTimeBasedStats updates time-based statistics from notification history
func (ns *NotificationStats) UpdateTimeBasedStats(history []NotificationRecord) {
	ns.mu.Lock()
	defer ns.mu.Unlock()

	now := time.Now()
	oneHourAgo := now.Add(-1 * time.Hour)
	oneDayAgo := now.Add(-24 * time.Hour)

	var lastHour, lastDay int64

	for _, record := range history {
		if record.Success {
			if record.Timestamp.After(oneHourAgo) {
				lastHour++
			}
			if record.Timestamp.After(oneDayAgo) {
				lastDay++
			}
		}
	}

	ns.LastHour = lastHour
	ns.LastDay = lastDay
	ns.LastUpdated = now
}

// GetSummary returns a summary of notification statistics
func (ns *NotificationStats) GetSummary() map[string]interface{} {
	ns.mu.RLock()
	defer ns.mu.RUnlock()

	total := ns.TotalSent + ns.TotalFailed
	successRate := float64(0)
	if total > 0 {
		successRate = float64(ns.TotalSent) / float64(total)
	}

	return map[string]interface{}{
		"total_sent":       ns.TotalSent,
		"total_failed":     ns.TotalFailed,
		"total_suppressed": ns.TotalSuppressed,
		"total_deduped":    ns.TotalDeduped,
		"rate_limited":     ns.RateLimited,
		"success_rate":     successRate,
		"last_hour":        ns.LastHour,
		"last_day":         ns.LastDay,
		"average_latency":  ns.AverageLatency.String(),
		"max_latency":      ns.MaxLatency.String(),
		"last_updated":     ns.LastUpdated,
		"by_priority":      ns.copyPriorityMap(),
		"by_type":          ns.copyTypeMap(),
		"by_channel":       ns.copyChannelMap(),
	}
}

// copyPriorityMap creates a copy of the priority map for safe access
func (ns *NotificationStats) copyPriorityMap() map[string]int64 {
	result := make(map[string]int64)
	for priority, count := range ns.ByPriority {
		switch priority {
		case PriorityEmergency:
			result["emergency"] = count
		case PriorityHigh:
			result["high"] = count
		case PriorityNormal:
			result["normal"] = count
		case PriorityLow:
			result["low"] = count
		case PriorityLowest:
			result["lowest"] = count
		default:
			result["unknown"] = count
		}
	}
	return result
}

// copyTypeMap creates a copy of the type map for safe access
func (ns *NotificationStats) copyTypeMap() map[string]int64 {
	result := make(map[string]int64)
	for notificationType, count := range ns.ByType {
		result[string(notificationType)] = count
	}
	return result
}

// copyChannelMap creates a copy of the channel map for safe access
func (ns *NotificationStats) copyChannelMap() map[string]int64 {
	result := make(map[string]int64)
	for channel, count := range ns.ByChannel {
		result[string(channel)] = count
	}
	return result
}

// Reset resets all statistics
func (ns *NotificationStats) Reset() {
	ns.mu.Lock()
	defer ns.mu.Unlock()

	ns.TotalSent = 0
	ns.TotalSuppressed = 0
	ns.TotalFailed = 0
	ns.TotalDeduped = 0
	ns.RateLimited = 0
	ns.AdaptiveAdjustments = 0
	ns.LastHour = 0
	ns.LastDay = 0
	ns.AverageLatency = 0
	ns.MaxLatency = 0

	ns.ByPriority = make(map[int]int64)
	ns.ByType = make(map[NotificationType]int64)
	ns.ByChannel = make(map[NotificationChannel]int64)

	ns.LastUpdated = time.Now()
}
