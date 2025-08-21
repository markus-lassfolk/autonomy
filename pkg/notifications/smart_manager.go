package notifications

import (
	"context"
	"crypto/sha256"
	"fmt"
	"sort"
	"strings"
	"sync"
	"time"

	"github.com/markus-lassfolk/autonomy/pkg/logx"
)

// SmartNotificationManager provides intelligent notification management with advanced rate limiting
type SmartNotificationManager struct {
	multiChannel *MultiChannelNotifier
	config       *SmartManagerConfig
	logger       *logx.Logger

	// Rate limiting and tracking
	rateLimiter   *AdaptiveRateLimiter
	deduplicator  *NotificationDeduplicator
	priorityQueue *PriorityNotificationQueue

	// State management
	mu                  sync.RWMutex
	lastNotification    map[NotificationType]time.Time
	notificationHistory []NotificationRecord
	suppressionRules    []SuppressionRule

	// Statistics
	stats *NotificationStats
}

// SmartManagerConfig holds configuration for the smart notification manager
type SmartManagerConfig struct {
	// Rate limiting configuration
	MaxNotificationsPerHour   int `json:"max_notifications_per_hour"`
	MaxNotificationsPerMinute int `json:"max_notifications_per_minute"`
	BurstLimit                int `json:"burst_limit"`

	// Priority-based rate limiting
	EmergencyRateLimit int `json:"emergency_rate_limit"` // Per hour
	HighRateLimit      int `json:"high_rate_limit"`      // Per hour
	NormalRateLimit    int `json:"normal_rate_limit"`    // Per hour
	LowRateLimit       int `json:"low_rate_limit"`       // Per hour

	// Cooldown periods by priority
	EmergencyCooldown time.Duration `json:"emergency_cooldown"`
	HighCooldown      time.Duration `json:"high_cooldown"`
	NormalCooldown    time.Duration `json:"normal_cooldown"`
	LowCooldown       time.Duration `json:"low_cooldown"`

	// Deduplication settings
	DeduplicationEnabled bool          `json:"deduplication_enabled"`
	DeduplicationWindow  time.Duration `json:"deduplication_window"`
	SimilarityThreshold  float64       `json:"similarity_threshold"`

	// Intelligent features
	AdaptiveRateLimiting bool          `json:"adaptive_rate_limiting"`
	PriorityEscalation   bool          `json:"priority_escalation"`
	EscalationThreshold  int           `json:"escalation_threshold"`
	EscalationDelay      time.Duration `json:"escalation_delay"`

	// Suppression rules
	QuietHours              bool     `json:"quiet_hours"`
	QuietHoursStart         string   `json:"quiet_hours_start"` // "22:00"
	QuietHoursEnd           string   `json:"quiet_hours_end"`   // "08:00"
	QuietHoursTimezone      string   `json:"quiet_hours_timezone"`
	SuppressLowPriorityDays []string `json:"suppress_low_priority_days"` // ["saturday", "sunday"]

	// Advanced settings
	HistoryRetention time.Duration `json:"history_retention"`
	MaxHistorySize   int           `json:"max_history_size"`
	EnableStatistics bool          `json:"enable_statistics"`
}

// NotificationRecord represents a historical notification record
type NotificationRecord struct {
	ID          string                 `json:"id"`
	Type        NotificationType       `json:"type"`
	Priority    int                    `json:"priority"`
	Title       string                 `json:"title"`
	Message     string                 `json:"message"`
	Timestamp   time.Time              `json:"timestamp"`
	Channels    []NotificationChannel  `json:"channels"`
	Success     bool                   `json:"success"`
	Error       string                 `json:"error,omitempty"`
	Context     map[string]interface{} `json:"context,omitempty"`
	Fingerprint string                 `json:"fingerprint"`
	Suppressed  bool                   `json:"suppressed"`
	Escalated   bool                   `json:"escalated"`
}

// SuppressionRule defines conditions for suppressing notifications
type SuppressionRule struct {
	ID         string                 `json:"id"`
	Name       string                 `json:"name"`
	Enabled    bool                   `json:"enabled"`
	Priority   []int                  `json:"priority"`    // Priorities to suppress
	Types      []NotificationType     `json:"types"`       // Types to suppress
	Channels   []NotificationChannel  `json:"channels"`    // Channels to suppress
	TimeRanges []TimeRange            `json:"time_ranges"` // Time-based suppression
	Conditions []SuppressionCondition `json:"conditions"`  // Advanced conditions
	Duration   time.Duration          `json:"duration"`    // How long to suppress
	CreatedAt  time.Time              `json:"created_at"`
	ExpiresAt  *time.Time             `json:"expires_at,omitempty"`
}

// TimeRange represents a time range for suppression
type TimeRange struct {
	Start    string   `json:"start"` // "HH:MM"
	End      string   `json:"end"`   // "HH:MM"
	Days     []string `json:"days"`  // ["monday", "tuesday", ...]
	Timezone string   `json:"timezone"`
}

// SuppressionCondition represents advanced suppression conditions
type SuppressionCondition struct {
	Field    string      `json:"field"`    // "message", "title", "context.key"
	Operator string      `json:"operator"` // "contains", "equals", "regex", "gt", "lt"
	Value    interface{} `json:"value"`
}

// NotificationStats tracks notification statistics
type NotificationStats struct {
	mu sync.RWMutex

	// Counters
	TotalSent       int64 `json:"total_sent"`
	TotalSuppressed int64 `json:"total_suppressed"`
	TotalFailed     int64 `json:"total_failed"`
	TotalDeduped    int64 `json:"total_deduped"`

	// By priority
	ByPriority map[int]int64 `json:"by_priority"`

	// By type
	ByType map[NotificationType]int64 `json:"by_type"`

	// By channel
	ByChannel map[NotificationChannel]int64 `json:"by_channel"`

	// Rate limiting stats
	RateLimited         int64 `json:"rate_limited"`
	AdaptiveAdjustments int64 `json:"adaptive_adjustments"`

	// Time-based stats
	LastHour int64 `json:"last_hour"`
	LastDay  int64 `json:"last_day"`

	// Performance
	AverageLatency time.Duration `json:"average_latency"`
	MaxLatency     time.Duration `json:"max_latency"`

	// Updated timestamp
	LastUpdated time.Time `json:"last_updated"`
}

// NewSmartNotificationManager creates a new smart notification manager
func NewSmartNotificationManager(multiChannel *MultiChannelNotifier, config *SmartManagerConfig, logger *logx.Logger) *SmartNotificationManager {
	if config == nil {
		config = DefaultSmartManagerConfig()
	}

	snm := &SmartNotificationManager{
		multiChannel:        multiChannel,
		config:              config,
		logger:              logger,
		lastNotification:    make(map[NotificationType]time.Time),
		notificationHistory: make([]NotificationRecord, 0),
		suppressionRules:    make([]SuppressionRule, 0),
		stats:               NewNotificationStats(),
	}

	// Initialize components
	snm.rateLimiter = NewAdaptiveRateLimiter(config, logger)
	snm.deduplicator = NewNotificationDeduplicator(config, logger)
	snm.priorityQueue = NewPriorityNotificationQueue(logger)

	// Start background tasks
	go snm.backgroundTasks()

	return snm
}

// SendNotification sends a notification through the smart manager
func (snm *SmartNotificationManager) SendNotification(ctx context.Context, notification *Notification) error {
	startTime := time.Now()

	// Generate fingerprint for deduplication
	fingerprint := snm.generateFingerprint(notification)

	// Create notification record
	record := &NotificationRecord{
		ID:          snm.generateID(),
		Type:        notification.Type,
		Priority:    notification.Priority,
		Title:       notification.Title,
		Message:     notification.Message,
		Timestamp:   notification.Timestamp,
		Context:     notification.Context,
		Fingerprint: fingerprint,
	}

	snm.mu.Lock()
	defer snm.mu.Unlock()

	// Check suppression rules
	if snm.isSuppressed(notification) {
		record.Suppressed = true
		snm.addToHistory(record)
		snm.stats.IncrementSuppressed()
		snm.logger.Debug("Notification suppressed by rules",
			"type", notification.Type,
			"priority", notification.Priority)
		return nil
	}

	// Check deduplication
	if snm.config.DeduplicationEnabled && snm.deduplicator.IsDuplicate(notification, fingerprint) {
		snm.stats.IncrementDeduped()
		snm.logger.Debug("Notification deduplicated",
			"fingerprint", fingerprint[:8])
		return nil
	}

	// Check rate limiting
	if !snm.rateLimiter.Allow(notification.Priority) {
		// Add to priority queue for later processing
		snm.priorityQueue.Enqueue(notification)
		snm.stats.IncrementRateLimited()
		snm.logger.Debug("Notification rate limited, queued for later",
			"priority", notification.Priority)
		return nil
	}

	// Send notification
	err := snm.multiChannel.SendNotification(ctx, notification)

	// Update record
	record.Success = err == nil
	if err != nil {
		record.Error = err.Error()
		snm.stats.IncrementFailed()
	} else {
		snm.stats.IncrementSent()
		snm.stats.IncrementByPriority(notification.Priority)
		snm.stats.IncrementByType(notification.Type)

		// Track channels used
		channels := snm.multiChannel.GetEnabledChannels()
		record.Channels = channels
		for _, channel := range channels {
			snm.stats.IncrementByChannel(channel)
		}
	}

	// Update performance stats
	latency := time.Since(startTime)
	snm.stats.UpdateLatency(latency)

	// Add to history
	snm.addToHistory(record)

	// Update last notification time
	snm.lastNotification[notification.Type] = time.Now()

	// Check for priority escalation
	if snm.config.PriorityEscalation && err != nil {
		snm.checkPriorityEscalation(notification)
	}

	return err
}

// generateFingerprint creates a fingerprint for deduplication
func (snm *SmartNotificationManager) generateFingerprint(notification *Notification) string {
	// Create a hash based on type, title, and key message content
	content := fmt.Sprintf("%s|%s|%s", notification.Type, notification.Title, notification.Message)
	hash := sha256.Sum256([]byte(content))
	return fmt.Sprintf("%x", hash)
}

// generateID generates a unique ID for notification records
func (snm *SmartNotificationManager) generateID() string {
	return fmt.Sprintf("notif_%d", time.Now().UnixNano())
}

// isSuppressed checks if a notification should be suppressed
func (snm *SmartNotificationManager) isSuppressed(notification *Notification) bool {
	now := time.Now()

	// Check quiet hours
	if snm.config.QuietHours && snm.isQuietHours(now) {
		// Allow emergency notifications during quiet hours
		if notification.Priority < PriorityEmergency {
			return true
		}
	}

	// Check day-based suppression for low priority
	if snm.isLowPrioritySuppressedDay(now, notification.Priority) {
		return true
	}

	// Check custom suppression rules
	for _, rule := range snm.suppressionRules {
		if !rule.Enabled {
			continue
		}

		if snm.matchesSuppressionRule(notification, &rule) {
			return true
		}
	}

	return false
}

// isQuietHours checks if current time is within quiet hours
func (snm *SmartNotificationManager) isQuietHours(now time.Time) bool {
	if !snm.config.QuietHours {
		return false
	}

	// Parse quiet hours (simplified - assumes same timezone)
	start, err := time.Parse("15:04", snm.config.QuietHoursStart)
	if err != nil {
		return false
	}

	end, err := time.Parse("15:04", snm.config.QuietHoursEnd)
	if err != nil {
		return false
	}

	currentTime := time.Date(0, 1, 1, now.Hour(), now.Minute(), 0, 0, time.UTC)
	startTime := time.Date(0, 1, 1, start.Hour(), start.Minute(), 0, 0, time.UTC)
	endTime := time.Date(0, 1, 1, end.Hour(), end.Minute(), 0, 0, time.UTC)

	// Handle overnight quiet hours (e.g., 22:00 to 08:00)
	if startTime.After(endTime) {
		return currentTime.After(startTime) || currentTime.Before(endTime)
	}

	return currentTime.After(startTime) && currentTime.Before(endTime)
}

// isLowPrioritySuppressedDay checks if low priority notifications are suppressed today
func (snm *SmartNotificationManager) isLowPrioritySuppressedDay(now time.Time, priority int) bool {
	if priority >= PriorityNormal {
		return false // Only suppress low priority
	}

	dayName := strings.ToLower(now.Weekday().String())
	for _, suppressedDay := range snm.config.SuppressLowPriorityDays {
		if strings.ToLower(suppressedDay) == dayName {
			return true
		}
	}

	return false
}

// matchesSuppressionRule checks if notification matches a suppression rule
func (snm *SmartNotificationManager) matchesSuppressionRule(notification *Notification, rule *SuppressionRule) bool {
	// Check if rule has expired
	if rule.ExpiresAt != nil && time.Now().After(*rule.ExpiresAt) {
		return false
	}

	// Check priority
	if len(rule.Priority) > 0 {
		found := false
		for _, p := range rule.Priority {
			if p == notification.Priority {
				found = true
				break
			}
		}
		if !found {
			return false
		}
	}

	// Check type
	if len(rule.Types) > 0 {
		found := false
		for _, t := range rule.Types {
			if t == notification.Type {
				found = true
				break
			}
		}
		if !found {
			return false
		}
	}

	// Check time ranges
	if len(rule.TimeRanges) > 0 {
		inTimeRange := false
		for _, tr := range rule.TimeRanges {
			if snm.isInTimeRange(time.Now(), &tr) {
				inTimeRange = true
				break
			}
		}
		if !inTimeRange {
			return false
		}
	}

	// Check conditions
	for _, condition := range rule.Conditions {
		if !snm.matchesCondition(notification, &condition) {
			return false
		}
	}

	return true
}

// isInTimeRange checks if current time is within a time range
func (snm *SmartNotificationManager) isInTimeRange(now time.Time, tr *TimeRange) bool {
	// Check day of week
	if len(tr.Days) > 0 {
		dayName := strings.ToLower(now.Weekday().String())
		found := false
		for _, day := range tr.Days {
			if strings.ToLower(day) == dayName {
				found = true
				break
			}
		}
		if !found {
			return false
		}
	}

	// Check time range (simplified)
	start, err := time.Parse("15:04", tr.Start)
	if err != nil {
		return false
	}

	end, err := time.Parse("15:04", tr.End)
	if err != nil {
		return false
	}

	currentTime := time.Date(0, 1, 1, now.Hour(), now.Minute(), 0, 0, time.UTC)
	startTime := time.Date(0, 1, 1, start.Hour(), start.Minute(), 0, 0, time.UTC)
	endTime := time.Date(0, 1, 1, end.Hour(), end.Minute(), 0, 0, time.UTC)

	if startTime.After(endTime) {
		return currentTime.After(startTime) || currentTime.Before(endTime)
	}

	return currentTime.After(startTime) && currentTime.Before(endTime)
}

// matchesCondition checks if notification matches a condition
func (snm *SmartNotificationManager) matchesCondition(notification *Notification, condition *SuppressionCondition) bool {
	var fieldValue interface{}

	// Extract field value
	switch condition.Field {
	case "title":
		fieldValue = notification.Title
	case "message":
		fieldValue = notification.Message
	case "type":
		fieldValue = string(notification.Type)
	case "priority":
		fieldValue = notification.Priority
	default:
		// Handle context fields (e.g., "context.key")
		if strings.HasPrefix(condition.Field, "context.") && notification.Context != nil {
			key := strings.TrimPrefix(condition.Field, "context.")
			fieldValue = notification.Context[key]
		}
	}

	if fieldValue == nil {
		return false
	}

	// Apply operator
	switch condition.Operator {
	case "equals":
		return fmt.Sprintf("%v", fieldValue) == fmt.Sprintf("%v", condition.Value)
	case "contains":
		return strings.Contains(strings.ToLower(fmt.Sprintf("%v", fieldValue)),
			strings.ToLower(fmt.Sprintf("%v", condition.Value)))
	case "gt":
		if fv, ok := fieldValue.(float64); ok {
			if cv, ok := condition.Value.(float64); ok {
				return fv > cv
			}
		}
		if iv, ok := fieldValue.(int); ok {
			if cv, ok := condition.Value.(int); ok {
				return iv > cv
			}
		}
	case "lt":
		if fv, ok := fieldValue.(float64); ok {
			if cv, ok := condition.Value.(float64); ok {
				return fv < cv
			}
		}
		if iv, ok := fieldValue.(int); ok {
			if cv, ok := condition.Value.(int); ok {
				return iv < cv
			}
		}
	}

	return false
}

// checkPriorityEscalation checks if notification should be escalated
func (snm *SmartNotificationManager) checkPriorityEscalation(notification *Notification) {
	if notification.Priority >= PriorityEmergency {
		return // Already at highest priority
	}

	// Count recent failures of this type
	failureCount := 0
	cutoff := time.Now().Add(-snm.config.EscalationDelay)

	for _, record := range snm.notificationHistory {
		if record.Type == notification.Type &&
			record.Timestamp.After(cutoff) &&
			!record.Success {
			failureCount++
		}
	}

	if failureCount >= snm.config.EscalationThreshold {
		// Schedule escalated notification
		escalated := &Notification{
			Type:      notification.Type,
			Title:     fmt.Sprintf("🚨 ESCALATED: %s", notification.Title),
			Message:   fmt.Sprintf("Multiple failures detected. Original: %s", notification.Message),
			Priority:  PriorityEmergency,
			Timestamp: time.Now(),
			Context:   notification.Context,
		}

		// Send escalated notification after delay
		go func() {
			time.Sleep(snm.config.EscalationDelay)
			ctx, cancel := context.WithTimeout(context.Background(), 30*time.Second)
			defer cancel()

			snm.logger.Warn("Escalating notification due to repeated failures",
				"type", notification.Type,
				"failure_count", failureCount)

			if err := snm.multiChannel.SendNotification(ctx, escalated); err != nil {
				snm.logger.Error("Failed to send escalated notification", "error", err)
			}
		}()
	}
}

// addToHistory adds a notification record to history
func (snm *SmartNotificationManager) addToHistory(record *NotificationRecord) {
	snm.notificationHistory = append(snm.notificationHistory, *record)

	// Trim history if too large
	if len(snm.notificationHistory) > snm.config.MaxHistorySize {
		snm.notificationHistory = snm.notificationHistory[len(snm.notificationHistory)-snm.config.MaxHistorySize:]
	}
}

// backgroundTasks runs background maintenance tasks
func (snm *SmartNotificationManager) backgroundTasks() {
	ticker := time.NewTicker(1 * time.Minute)
	defer ticker.Stop()

	for {
		select {
		case <-ticker.C:
			snm.performMaintenance()
		}
	}
}

// performMaintenance performs periodic maintenance tasks
func (snm *SmartNotificationManager) performMaintenance() {
	snm.mu.Lock()
	defer snm.mu.Unlock()

	now := time.Now()

	// Clean old history
	if snm.config.HistoryRetention > 0 {
		cutoff := now.Add(-snm.config.HistoryRetention)
		newHistory := make([]NotificationRecord, 0)

		for _, record := range snm.notificationHistory {
			if record.Timestamp.After(cutoff) {
				newHistory = append(newHistory, record)
			}
		}

		snm.notificationHistory = newHistory
	}

	// Process queued notifications
	snm.processQueuedNotifications()

	// Update statistics
	snm.stats.UpdateTimeBasedStats(snm.notificationHistory)

	// Clean expired suppression rules
	snm.cleanExpiredSuppressionRules()
}

// processQueuedNotifications processes notifications in the priority queue
func (snm *SmartNotificationManager) processQueuedNotifications() {
	for {
		notification := snm.priorityQueue.Dequeue()
		if notification == nil {
			break
		}

		// Check if rate limit now allows this notification
		if snm.rateLimiter.Allow(notification.Priority) {
			ctx, cancel := context.WithTimeout(context.Background(), 30*time.Second)

			err := snm.multiChannel.SendNotification(ctx, notification)
			if err != nil {
				snm.logger.Warn("Failed to send queued notification", "error", err)
			} else {
				snm.logger.Debug("Successfully sent queued notification",
					"type", notification.Type,
					"priority", notification.Priority)
			}

			cancel()
		} else {
			// Put it back in queue
			snm.priorityQueue.Enqueue(notification)
			break
		}
	}
}

// cleanExpiredSuppressionRules removes expired suppression rules
func (snm *SmartNotificationManager) cleanExpiredSuppressionRules() {
	now := time.Now()
	newRules := make([]SuppressionRule, 0)

	for _, rule := range snm.suppressionRules {
		if rule.ExpiresAt == nil || now.Before(*rule.ExpiresAt) {
			newRules = append(newRules, rule)
		}
	}

	snm.suppressionRules = newRules
}

// GetStats returns current notification statistics
func (snm *SmartNotificationManager) GetStats() *NotificationStats {
	return snm.stats
}

// GetHistory returns notification history
func (snm *SmartNotificationManager) GetHistory(limit int) []NotificationRecord {
	snm.mu.RLock()
	defer snm.mu.RUnlock()

	if limit <= 0 || limit > len(snm.notificationHistory) {
		limit = len(snm.notificationHistory)
	}

	// Return most recent records
	start := len(snm.notificationHistory) - limit
	if start < 0 {
		start = 0
	}

	history := make([]NotificationRecord, limit)
	copy(history, snm.notificationHistory[start:])

	// Sort by timestamp (newest first)
	sort.Slice(history, func(i, j int) bool {
		return history[i].Timestamp.After(history[j].Timestamp)
	})

	return history
}

// AddSuppressionRule adds a new suppression rule
func (snm *SmartNotificationManager) AddSuppressionRule(rule SuppressionRule) {
	snm.mu.Lock()
	defer snm.mu.Unlock()

	rule.ID = snm.generateID()
	rule.CreatedAt = time.Now()

	if rule.Duration > 0 {
		expiresAt := time.Now().Add(rule.Duration)
		rule.ExpiresAt = &expiresAt
	}

	snm.suppressionRules = append(snm.suppressionRules, rule)

	snm.logger.Info("Added suppression rule",
		"id", rule.ID,
		"name", rule.Name,
		"duration", rule.Duration)
}

// RemoveSuppressionRule removes a suppression rule by ID
func (snm *SmartNotificationManager) RemoveSuppressionRule(id string) bool {
	snm.mu.Lock()
	defer snm.mu.Unlock()

	for i, rule := range snm.suppressionRules {
		if rule.ID == id {
			snm.suppressionRules = append(snm.suppressionRules[:i], snm.suppressionRules[i+1:]...)
			snm.logger.Info("Removed suppression rule", "id", id, "name", rule.Name)
			return true
		}
	}

	return false
}

// GetSuppressionRules returns all active suppression rules
func (snm *SmartNotificationManager) GetSuppressionRules() []SuppressionRule {
	snm.mu.RLock()
	defer snm.mu.RUnlock()

	rules := make([]SuppressionRule, len(snm.suppressionRules))
	copy(rules, snm.suppressionRules)
	return rules
}

// DefaultSmartManagerConfig returns default configuration
func DefaultSmartManagerConfig() *SmartManagerConfig {
	return &SmartManagerConfig{
		MaxNotificationsPerHour:   60,
		MaxNotificationsPerMinute: 10,
		BurstLimit:                5,

		EmergencyRateLimit: 100, // No real limit for emergencies
		HighRateLimit:      30,
		NormalRateLimit:    20,
		LowRateLimit:       10,

		EmergencyCooldown: 30 * time.Second,
		HighCooldown:      2 * time.Minute,
		NormalCooldown:    5 * time.Minute,
		LowCooldown:       15 * time.Minute,

		DeduplicationEnabled: true,
		DeduplicationWindow:  10 * time.Minute,
		SimilarityThreshold:  0.8,

		AdaptiveRateLimiting: true,
		PriorityEscalation:   true,
		EscalationThreshold:  3,
		EscalationDelay:      5 * time.Minute,

		QuietHours:              false,
		QuietHoursStart:         "22:00",
		QuietHoursEnd:           "08:00",
		SuppressLowPriorityDays: []string{"saturday", "sunday"},

		HistoryRetention: 7 * 24 * time.Hour, // 7 days
		MaxHistorySize:   1000,
		EnableStatistics: true,
	}
}
