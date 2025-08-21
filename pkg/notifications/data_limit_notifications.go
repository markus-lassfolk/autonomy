package notifications

import (
	"context"
	"fmt"
	"time"

	"github.com/markus-lassfolk/autonomy/pkg/discovery"
	"github.com/markus-lassfolk/autonomy/pkg/logx"
)

// DataLimitNotificationManager handles data limit related notifications
type DataLimitNotificationManager struct {
	logger            *logx.Logger
	notificationMgr   *Manager
	lastNotifications map[string]time.Time // Track last notification times to avoid spam
	dailyUsageTracker map[string]*DailyUsageTracker
}

// DailyUsageTracker tracks daily data usage patterns
type DailyUsageTracker struct {
	InterfaceName       string    `json:"interface_name"`
	LastResetDate       time.Time `json:"last_reset_date"`
	DailyAllowanceMB    float64   `json:"daily_allowance_mb"`
	TodayUsageMB        float64   `json:"today_usage_mb"`
	YesterdayUsageMB    float64   `json:"yesterday_usage_mb"`
	LastUsageCheckMB    float64   `json:"last_usage_check_mb"`
	LastUsageCheckTime  time.Time `json:"last_usage_check_time"`
	DailyWarning80Sent  bool      `json:"daily_warning_80_sent"`
	DailyWarning100Sent bool      `json:"daily_warning_100_sent"`
}

// Data limit notification types (using existing NotificationType from manager.go)
const (
	NotifyFailoverToLimited    = NotificationDataLimitFailover
	NotifyFailbackFromLimited  = NotificationDataLimitFailback
	NotifyDailyUsage80         = NotificationDataLimitDaily80
	NotifyDailyUsage100        = NotificationDataLimitDaily100
	NotifyMonthlyUsage80       = NotificationDataLimitMonthly80
	NotifyMonthlyUsage95       = NotificationDataLimitMonthly95
	NotifyMonthlyUsageExceeded = NotificationDataLimitExceeded
	NotifyDataLimitReset       = NotificationDataLimitReset
	NotifyUnexpectedUsageSpike = NotificationDataUsageSpike
	NotifyLowDataRemaining     = NotificationDataLimitExceeded // Reuse existing type
)

// NewDataLimitNotificationManager creates a new data limit notification manager
func NewDataLimitNotificationManager(logger *logx.Logger, notificationMgr *Manager) *DataLimitNotificationManager {
	return &DataLimitNotificationManager{
		logger:            logger,
		notificationMgr:   notificationMgr,
		lastNotifications: make(map[string]time.Time),
		dailyUsageTracker: make(map[string]*DailyUsageTracker),
	}
}

// NotifyFailoverToLimited sends notification when failing over to a limited connection
func (dlnm *DataLimitNotificationManager) NotifyFailoverToLimited(fromInterface, toInterface string, dataLimit *discovery.DataLimitConfig) error {
	if !dlnm.shouldSendNotification(string(NotifyFailoverToLimited), 5*time.Minute) {
		return nil // Avoid spam
	}

	remainingMB := float64(dataLimit.DataLimitMB) - dataLimit.CurrentUsageMB
	remainingGB := remainingMB / 1024

	title := "🔄 Failover to Data-Limited Connection"
	message := fmt.Sprintf(
		"Switched from %s to %s\n\n"+
			"📊 Data Status:\n"+
			"• Remaining: %.2f GB (%.1f%%)\n"+
			"• Used: %.2f GB of %d GB\n"+
			"• Resets in: %d days\n\n"+
			"⚠️ Monitor usage carefully!",
		fromInterface, toInterface,
		remainingGB, 100-dataLimit.UsagePercentage,
		dataLimit.CurrentUsageMB/1024, dataLimit.DataLimitMB/1024,
		dataLimit.DaysUntilReset,
	)

	priority := PriorityNormal
	if dataLimit.UsagePercentage > 80 {
		priority = PriorityHigh
		title = "⚠️ " + title
	}

	return dlnm.notificationMgr.SendNotification(context.Background(), &NotificationEvent{
		Type:      NotifyFailoverToLimited,
		Title:     title,
		Message:   message,
		Priority:  priority,
		Timestamp: time.Now(),
	})
}

// NotifyFailbackFromLimited sends notification when failing back from a limited connection
func (dlnm *DataLimitNotificationManager) NotifyFailbackFromLimited(fromInterface, toInterface string, dataLimit *discovery.DataLimitConfig) error {
	if !dlnm.shouldSendNotification(string(NotifyFailbackFromLimited), 5*time.Minute) {
		return nil
	}

	remainingMB := float64(dataLimit.DataLimitMB) - dataLimit.CurrentUsageMB
	remainingGB := remainingMB / 1024

	title := "✅ Failback to Unlimited Connection"
	message := fmt.Sprintf(
		"Switched back from %s to %s\n\n"+
			"📊 Final Data Usage:\n"+
			"• Remaining: %.2f GB (%.1f%%)\n"+
			"• Used during outage: [calculated]\n"+
			"• Total used: %.2f GB of %d GB\n"+
			"• Resets in: %d days\n\n"+
			"🎉 Back to unlimited connectivity!",
		fromInterface, toInterface,
		remainingGB, 100-dataLimit.UsagePercentage,
		dataLimit.CurrentUsageMB/1024, dataLimit.DataLimitMB/1024,
		dataLimit.DaysUntilReset,
	)

	return dlnm.notificationMgr.SendNotification(context.Background(), &NotificationEvent{
		Type:      NotifyFailbackFromLimited,
		Title:     title,
		Message:   message,
		Priority:  PriorityNormal,
		Timestamp: time.Now(),
	})
}

// NotifyDailyUsageThreshold sends notification when daily usage threshold is reached
func (dlnm *DataLimitNotificationManager) NotifyDailyUsageThreshold(interfaceName string, dataLimit *discovery.DataLimitConfig, percentage int) error {
	tracker := dlnm.getDailyUsageTracker(interfaceName, dataLimit)

	notifyType := NotifyDailyUsage80
	if percentage >= 100 {
		notifyType = NotifyDailyUsage100
		if tracker.DailyWarning100Sent {
			return nil // Already sent
		}
		tracker.DailyWarning100Sent = true
	} else {
		if tracker.DailyWarning80Sent {
			return nil // Already sent
		}
		tracker.DailyWarning80Sent = true
	}

	title := fmt.Sprintf("📊 Daily Data Usage: %d%%", percentage)
	if percentage >= 100 {
		title = "🚨 Daily Data Limit Exceeded!"
	}

	dailyUsagePercent := (tracker.TodayUsageMB / tracker.DailyAllowanceMB) * 100
	remainingDaily := tracker.DailyAllowanceMB - tracker.TodayUsageMB

	message := fmt.Sprintf(
		"Interface: %s\n\n"+
			"📅 Today's Usage:\n"+
			"• Used: %.2f MB (%.1f%%)\n"+
			"• Daily allowance: %.2f MB\n"+
			"• Remaining today: %.2f MB\n\n"+
			"📊 Monthly Status:\n"+
			"• Used: %.2f GB of %d GB\n"+
			"• Remaining: %.2f GB\n"+
			"• Resets in: %d days",
		interfaceName,
		tracker.TodayUsageMB, dailyUsagePercent,
		tracker.DailyAllowanceMB,
		remainingDaily,
		dataLimit.CurrentUsageMB/1024, dataLimit.DataLimitMB/1024,
		(float64(dataLimit.DataLimitMB)-dataLimit.CurrentUsageMB)/1024,
		dataLimit.DaysUntilReset,
	)

	priority := PriorityNormal
	if percentage >= 100 {
		priority = PriorityHigh
	}

	return dlnm.notificationMgr.SendNotification(context.Background(), &NotificationEvent{
		Type:      notifyType,
		Title:     title,
		Message:   message,
		Priority:  priority,
		Timestamp: time.Now(),
	})
}

// NotifyMonthlyUsageThreshold sends notification for monthly usage milestones
func (dlnm *DataLimitNotificationManager) NotifyMonthlyUsageThreshold(interfaceName string, dataLimit *discovery.DataLimitConfig) error {
	var notifyType NotificationType
	var title, emoji string

	if dataLimit.UsagePercentage >= 100 {
		notifyType = NotifyMonthlyUsageExceeded
		title = "🚨 Monthly Data Limit EXCEEDED!"
		emoji = "🚨"
	} else if dataLimit.UsagePercentage >= 95 {
		notifyType = NotifyMonthlyUsage95
		title = "⚠️ Monthly Data Limit: 95% Used"
		emoji = "⚠️"
	} else if dataLimit.UsagePercentage >= 80 {
		notifyType = NotifyMonthlyUsage80
		title = "📊 Monthly Data Limit: 80% Used"
		emoji = "📊"
	} else {
		return nil // No notification needed
	}

	if !dlnm.shouldSendNotification(string(notifyType)+interfaceName, 24*time.Hour) {
		return nil // Avoid daily spam for same threshold
	}

	remainingMB := float64(dataLimit.DataLimitMB) - dataLimit.CurrentUsageMB
	remainingGB := remainingMB / 1024
	tracker := dlnm.getDailyUsageTracker(interfaceName, dataLimit)

	message := fmt.Sprintf(
		"Interface: %s\n\n"+
			"%s Monthly Status:\n"+
			"• Used: %.2f GB of %d GB (%.1f%%)\n"+
			"• Remaining: %.2f GB\n"+
			"• Resets in: %d days\n\n"+
			"📅 Daily Allowance:\n"+
			"• Recommended: %.2f MB/day\n"+
			"• Today used: %.2f MB\n\n"+
			"💡 Consider switching to unlimited connection!",
		interfaceName, emoji,
		dataLimit.CurrentUsageMB/1024, dataLimit.DataLimitMB/1024, dataLimit.UsagePercentage,
		remainingGB, dataLimit.DaysUntilReset,
		tracker.DailyAllowanceMB, tracker.TodayUsageMB,
	)

	priority := PriorityNormal
	if dataLimit.UsagePercentage >= 95 {
		priority = PriorityHigh
	}

	return dlnm.notificationMgr.SendNotification(context.Background(), &NotificationEvent{
		Type:      notifyType,
		Title:     title,
		Message:   message,
		Priority:  priority,
		Timestamp: time.Now(),
	})
}

// NotifyUnexpectedUsageSpike detects and notifies about unusual usage patterns
func (dlnm *DataLimitNotificationManager) NotifyUnexpectedUsageSpike(interfaceName string, dataLimit *discovery.DataLimitConfig) error {
	tracker := dlnm.getDailyUsageTracker(interfaceName, dataLimit)

	// Calculate usage since last check
	usageSinceLastCheck := dataLimit.CurrentUsageMB - tracker.LastUsageCheckMB
	timeSinceLastCheck := time.Since(tracker.LastUsageCheckTime)

	if timeSinceLastCheck < time.Hour {
		return nil // Too soon to check
	}

	// Calculate hourly usage rate
	hourlyUsageMB := usageSinceLastCheck / timeSinceLastCheck.Hours()

	// If hourly usage is more than 3x the normal daily allowance per hour, it's a spike
	normalHourlyUsage := tracker.DailyAllowanceMB / 24
	if hourlyUsageMB > normalHourlyUsage*3 && usageSinceLastCheck > 50 { // At least 50MB spike

		if !dlnm.shouldSendNotification(string(NotifyUnexpectedUsageSpike)+interfaceName, 2*time.Hour) {
			return nil
		}

		title := "📈 Unusual Data Usage Detected"
		message := fmt.Sprintf(
			"Interface: %s\n\n"+
				"🚨 High usage detected:\n"+
				"• Last %.1f hours: %.2f MB\n"+
				"• Rate: %.2f MB/hour\n"+
				"• Normal rate: %.2f MB/hour\n\n"+
				"📊 Current Status:\n"+
				"• Monthly used: %.1f%%\n"+
				"• Remaining: %.2f GB\n"+
				"• Resets in: %d days\n\n"+
				"💡 Check for background downloads or streaming!",
			interfaceName, timeSinceLastCheck.Hours(), usageSinceLastCheck,
			hourlyUsageMB, normalHourlyUsage,
			dataLimit.UsagePercentage,
			(float64(dataLimit.DataLimitMB)-dataLimit.CurrentUsageMB)/1024,
			dataLimit.DaysUntilReset,
		)

		return dlnm.notificationMgr.SendNotification(context.Background(), &NotificationEvent{
			Type:      NotifyUnexpectedUsageSpike,
			Title:     title,
			Message:   message,
			Priority:  PriorityHigh,
			Timestamp: time.Now(),
		})
	}

	// Update tracking
	tracker.LastUsageCheckMB = dataLimit.CurrentUsageMB
	tracker.LastUsageCheckTime = time.Now()

	return nil
}

// NotifyDataLimitReset sends notification when data limits reset
func (dlnm *DataLimitNotificationManager) NotifyDataLimitReset(interfaceName string, dataLimit *discovery.DataLimitConfig) error {
	title := "🔄 Data Limit Reset"
	message := fmt.Sprintf(
		"Interface: %s\n\n"+
			"✅ Monthly data limit has reset!\n\n"+
			"📊 New Month:\n"+
			"• Limit: %d GB\n"+
			"• Used: %.2f MB\n"+
			"• Available: %.2f GB\n\n"+
			"📅 Daily Allowance:\n"+
			"• Recommended: %.2f MB/day\n\n"+
			"🎉 Fresh start with full data allowance!",
		interfaceName,
		dataLimit.DataLimitMB/1024,
		dataLimit.CurrentUsageMB,
		float64(dataLimit.DataLimitMB)/1024,
		float64(dataLimit.DataLimitMB)/float64(dataLimit.DaysUntilReset),
	)

	// Reset daily tracking
	if tracker, exists := dlnm.dailyUsageTracker[interfaceName]; exists {
		tracker.LastResetDate = time.Now()
		tracker.DailyWarning80Sent = false
		tracker.DailyWarning100Sent = false
		tracker.TodayUsageMB = 0
		tracker.YesterdayUsageMB = 0
	}

	return dlnm.notificationMgr.SendNotification(context.Background(), &NotificationEvent{
		Type:      NotifyDataLimitReset,
		Title:     title,
		Message:   message,
		Priority:  PriorityNormal,
		Timestamp: time.Now(),
	})
}

// UpdateUsageTracking updates daily usage tracking for an interface
func (dlnm *DataLimitNotificationManager) UpdateUsageTracking(interfaceName string, dataLimit *discovery.DataLimitConfig) {
	tracker := dlnm.getDailyUsageTracker(interfaceName, dataLimit)

	now := time.Now()
	today := now.Format("2006-01-02")
	lastCheckDay := tracker.LastUsageCheckTime.Format("2006-01-02")

	// If it's a new day, reset daily counters
	if today != lastCheckDay {
		tracker.YesterdayUsageMB = tracker.TodayUsageMB
		tracker.TodayUsageMB = 0
		tracker.DailyWarning80Sent = false
		tracker.DailyWarning100Sent = false
	}

	// Calculate today's usage
	if tracker.LastUsageCheckMB > 0 {
		usageIncrease := dataLimit.CurrentUsageMB - tracker.LastUsageCheckMB
		if usageIncrease > 0 {
			tracker.TodayUsageMB += usageIncrease
		}
	}

	tracker.LastUsageCheckMB = dataLimit.CurrentUsageMB
	tracker.LastUsageCheckTime = now
}

// getDailyUsageTracker gets or creates a daily usage tracker for an interface
func (dlnm *DataLimitNotificationManager) getDailyUsageTracker(interfaceName string, dataLimit *discovery.DataLimitConfig) *DailyUsageTracker {
	if tracker, exists := dlnm.dailyUsageTracker[interfaceName]; exists {
		return tracker
	}

	// Calculate daily allowance (remaining data / remaining days)
	remainingMB := float64(dataLimit.DataLimitMB) - dataLimit.CurrentUsageMB
	dailyAllowance := remainingMB / float64(dataLimit.DaysUntilReset)
	if dailyAllowance < 0 {
		dailyAllowance = 0
	}

	tracker := &DailyUsageTracker{
		InterfaceName:      interfaceName,
		LastResetDate:      time.Now().AddDate(0, 0, -30), // Assume last reset was ~30 days ago
		DailyAllowanceMB:   dailyAllowance,
		TodayUsageMB:       0,
		YesterdayUsageMB:   0,
		LastUsageCheckMB:   dataLimit.CurrentUsageMB,
		LastUsageCheckTime: time.Now(),
	}

	dlnm.dailyUsageTracker[interfaceName] = tracker
	return tracker
}

// shouldSendNotification checks if enough time has passed since last notification of this type
func (dlnm *DataLimitNotificationManager) shouldSendNotification(notificationKey string, cooldown time.Duration) bool {
	if lastTime, exists := dlnm.lastNotifications[notificationKey]; exists {
		if time.Since(lastTime) < cooldown {
			return false
		}
	}

	dlnm.lastNotifications[notificationKey] = time.Now()
	return true
}

// CheckAllDataLimitNotifications performs comprehensive data limit monitoring
func (dlnm *DataLimitNotificationManager) CheckAllDataLimitNotifications(dataLimits map[string]*discovery.DataLimitConfig) error {
	for interfaceName, dataLimit := range dataLimits {
		if !dataLimit.Enabled {
			continue
		}

		// Update usage tracking
		dlnm.UpdateUsageTracking(interfaceName, dataLimit)

		// Check monthly thresholds
		if err := dlnm.NotifyMonthlyUsageThreshold(interfaceName, dataLimit); err != nil {
			dlnm.logger.Error("Failed to send monthly usage notification", "interface", interfaceName, "error", err)
		}

		// Check for usage spikes
		if err := dlnm.NotifyUnexpectedUsageSpike(interfaceName, dataLimit); err != nil {
			dlnm.logger.Error("Failed to send usage spike notification", "interface", interfaceName, "error", err)
		}

		// Check daily thresholds
		tracker := dlnm.getDailyUsageTracker(interfaceName, dataLimit)
		dailyUsagePercent := (tracker.TodayUsageMB / tracker.DailyAllowanceMB) * 100

		if dailyUsagePercent >= 100 && !tracker.DailyWarning100Sent {
			if err := dlnm.NotifyDailyUsageThreshold(interfaceName, dataLimit, 100); err != nil {
				dlnm.logger.Error("Failed to send daily 100% notification", "interface", interfaceName, "error", err)
			}
		} else if dailyUsagePercent >= 80 && !tracker.DailyWarning80Sent {
			if err := dlnm.NotifyDailyUsageThreshold(interfaceName, dataLimit, 80); err != nil {
				dlnm.logger.Error("Failed to send daily 80% notification", "interface", interfaceName, "error", err)
			}
		}
	}

	return nil
}
