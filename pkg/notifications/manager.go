package notifications

import (
	"bytes"
	"context"
	"encoding/json"
	"fmt"
	"net/http"
	"net/url"
	"os"
	"strings"
	"sync"
	"time"

	"github.com/markus-lassfolk/autonomy/pkg"
	"github.com/markus-lassfolk/autonomy/pkg/logx"
)

// NotificationType represents different types of notifications
type NotificationType string

const (
	// Failover and network events
	NotificationFailover   NotificationType = "failover"
	NotificationFailback   NotificationType = "failback"
	NotificationMemberDown NotificationType = "member_down"
	NotificationMemberUp   NotificationType = "member_up"
	NotificationPredictive NotificationType = "predictive"

	// System events
	NotificationCriticalError NotificationType = "critical_error"
	NotificationSystemHealth  NotificationType = "system_health"
	NotificationRecovery      NotificationType = "recovery"

	// Status updates
	NotificationStatusUpdate NotificationType = "status_update"
	NotificationSummary      NotificationType = "summary"

	// Data limit notifications
	NotificationDataLimitFailover  NotificationType = "data_limit_failover"
	NotificationDataLimitFailback  NotificationType = "data_limit_failback"
	NotificationDataLimitDaily80   NotificationType = "data_limit_daily_80"
	NotificationDataLimitDaily100  NotificationType = "data_limit_daily_100"
	NotificationDataLimitMonthly80 NotificationType = "data_limit_monthly_80"
	NotificationDataLimitMonthly95 NotificationType = "data_limit_monthly_95"
	NotificationDataLimitExceeded  NotificationType = "data_limit_exceeded"
	NotificationDataLimitReset     NotificationType = "data_limit_reset"
	NotificationDataUsageSpike     NotificationType = "data_usage_spike"
)

// Priority levels matching Pushover API
const (
	PriorityLowest    = -2 // No notification/sound
	PriorityLow       = -1 // Quiet notification
	PriorityNormal    = 0  // Normal notification
	PriorityHigh      = 1  // High-priority notification
	PriorityEmergency = 2  // Emergency notification with retry
)

// NotificationConfig holds configuration for the notification manager
type NotificationConfig struct {
	// Pushover settings
	PushoverEnabled bool   `json:"pushover_enabled"`
	PushoverToken   string `json:"pushover_token"`
	PushoverUser    string `json:"pushover_user"`
	PushoverDevice  string `json:"pushover_device,omitempty"`

	// Advanced Pushover features
	PriorityThreshold      string `json:"priority_threshold"`      // "info", "warning", "critical", "emergency"
	AcknowledgmentTracking bool   `json:"acknowledgment_tracking"` // Track message acknowledgments
	LocationEnabled        bool   `json:"location_enabled"`        // Include GPS coordinates
	RichContextEnabled     bool   `json:"rich_context_enabled"`    // Include detailed metrics

	// Notification control
	NotifyOnFailover     bool `json:"notify_on_failover"`
	NotifyOnFailback     bool `json:"notify_on_failback"`
	NotifyOnMemberDown   bool `json:"notify_on_member_down"`
	NotifyOnMemberUp     bool `json:"notify_on_member_up"`
	NotifyOnPredictive   bool `json:"notify_on_predictive"`
	NotifyOnCritical     bool `json:"notify_on_critical"`
	NotifyOnRecovery     bool `json:"notify_on_recovery"`
	NotifyOnStatusUpdate bool `json:"notify_on_status_update"`

	// Timing and rate limiting
	CooldownPeriod       time.Duration `json:"cooldown_period"`
	MaxNotificationsHour int           `json:"max_notifications_hour"`
	EmergencyCooldown    time.Duration `json:"emergency_cooldown"`

	// Priority settings
	PriorityFailover     int `json:"priority_failover"`
	PriorityFailback     int `json:"priority_failback"`
	PriorityMemberDown   int `json:"priority_member_down"`
	PriorityMemberUp     int `json:"priority_member_up"`
	PriorityPredictive   int `json:"priority_predictive"`
	PriorityCritical     int `json:"priority_critical"`
	PriorityRecovery     int `json:"priority_recovery"`
	PriorityStatusUpdate int `json:"priority_status_update"`

	// Advanced settings
	RetryAttempts    int           `json:"retry_attempts"`
	RetryDelay       time.Duration `json:"retry_delay"`
	HTTPTimeout      time.Duration `json:"http_timeout"`
	IncludeHostname  bool          `json:"include_hostname"`
	IncludeTimestamp bool          `json:"include_timestamp"`

	// Smart rate limiting (priority-based cooldowns)
	InfoCooldown           time.Duration `json:"info_cooldown"`            // 6 hours for info messages
	WarningCooldown        time.Duration `json:"warning_cooldown"`         // 1 hour for warnings
	CriticalCooldown       time.Duration `json:"critical_cooldown"`        // 5 minutes for critical
	EmergencyRetryInterval time.Duration `json:"emergency_retry_interval"` // 60 seconds for emergency retry
}

// DefaultNotificationConfig returns default notification configuration
func DefaultNotificationConfig() *NotificationConfig {
	return &NotificationConfig{
		// Pushover settings
		PushoverEnabled: false,
		PushoverToken:   "",
		PushoverUser:    "",
		PushoverDevice:  "",

		// Advanced Pushover features
		PriorityThreshold:      "warning", // Only send warning+ by default
		AcknowledgmentTracking: true,      // Track acknowledgments
		LocationEnabled:        true,      // Include location data
		RichContextEnabled:     true,      // Include rich metrics

		// Notification control - conservative defaults
		NotifyOnFailover:     true,
		NotifyOnFailback:     true,
		NotifyOnMemberDown:   true,
		NotifyOnMemberUp:     false, // Reduce noise
		NotifyOnPredictive:   true,
		NotifyOnCritical:     true,
		NotifyOnRecovery:     true,
		NotifyOnStatusUpdate: false, // Reduce noise

		// Timing and rate limiting
		CooldownPeriod:       5 * time.Minute,
		MaxNotificationsHour: 20,
		EmergencyCooldown:    1 * time.Minute,

		// Priority settings - based on legacy system
		PriorityFailover:     PriorityHigh,      // High priority - network down
		PriorityFailback:     PriorityNormal,    // Normal - network restored
		PriorityMemberDown:   PriorityHigh,      // High priority - backup lost
		PriorityMemberUp:     PriorityLow,       // Low priority - backup restored
		PriorityPredictive:   PriorityNormal,    // Normal - early warning
		PriorityCritical:     PriorityEmergency, // Emergency - system failure
		PriorityRecovery:     PriorityNormal,    // Normal - system recovered
		PriorityStatusUpdate: PriorityLow,       // Low priority - status info

		// Advanced settings
		RetryAttempts:    3,
		RetryDelay:       30 * time.Second,
		HTTPTimeout:      10 * time.Second,
		IncludeHostname:  true,
		IncludeTimestamp: true,

		// Smart rate limiting (priority-based cooldowns)
		InfoCooldown:           6 * time.Hour,    // 6 hours for info messages
		WarningCooldown:        1 * time.Hour,    // 1 hour for warnings
		CriticalCooldown:       5 * time.Minute,  // 5 minutes for critical
		EmergencyRetryInterval: 60 * time.Second, // 60 seconds for emergency retry
	}
}

// LocationData represents GPS coordinates and location context
type LocationData struct {
	Latitude  float64 `json:"latitude"`
	Longitude float64 `json:"longitude"`
	Address   string  `json:"address,omitempty"`
	Source    string  `json:"source"` // "starlink", "rutos", "manual"
}

// NotificationEvent represents a notification event
type NotificationEvent struct {
	Type      NotificationType `json:"type"`
	Title     string           `json:"title"`
	Message   string           `json:"message"`
	Priority  int              `json:"priority"`
	Sound     string           `json:"sound,omitempty"`
	URL       string           `json:"url,omitempty"`
	URLTitle  string           `json:"url_title,omitempty"`
	Timestamp time.Time        `json:"timestamp"`

	// Enhanced context data
	Member     *pkg.Member            `json:"member,omitempty"`
	FromMember *pkg.Member            `json:"from_member,omitempty"`
	ToMember   *pkg.Member            `json:"to_member,omitempty"`
	Metrics    *pkg.Metrics           `json:"metrics,omitempty"`
	Error      error                  `json:"error,omitempty"`
	Details    map[string]interface{} `json:"details,omitempty"`

	// Rich context features
	Location     *LocationData `json:"location,omitempty"`
	Duration     time.Duration `json:"duration,omitempty"`   // How long the issue lasted
	Acknowledged bool          `json:"acknowledged"`         // Has user acknowledged this?
	MessageID    string        `json:"message_id,omitempty"` // Pushover message ID for tracking

	// Performance context
	PerformanceMetrics map[string]float64   `json:"performance_metrics,omitempty"`
	TrendData          map[string][]float64 `json:"trend_data,omitempty"`
}

// Manager handles all notification operations
type Manager struct {
	config   *NotificationConfig
	logger   *logx.Logger
	hostname string

	// Rate limiting
	mu                sync.Mutex
	lastNotification  map[NotificationType]time.Time
	notificationCount map[string]int // hour-based counting
	lastEmergency     time.Time

	// HTTP client for API calls
	httpClient *http.Client

	// Statistics
	stats struct {
		TotalSent       int64     `json:"total_sent"`
		TotalFailed     int64     `json:"total_failed"`
		RateLimited     int64     `json:"rate_limited"`
		LastSentTime    time.Time `json:"last_sent_time"`
		LastFailureTime time.Time `json:"last_failure_time"`
	}
}

// NewManager creates a new notification manager
func NewManager(config *NotificationConfig, logger *logx.Logger) *Manager {
	if config == nil {
		config = DefaultNotificationConfig()
	}

	// Get hostname for notifications
	hostname := "autonomy-router"
	if h, err := os.Hostname(); err == nil && h != "" {
		hostname = h
	}

	return &Manager{
		config:            config,
		logger:            logger,
		hostname:          hostname,
		lastNotification:  make(map[NotificationType]time.Time),
		notificationCount: make(map[string]int),
		httpClient: &http.Client{
			Timeout: config.HTTPTimeout,
		},
	}
}

// IsEnabled returns whether notifications are enabled
func (m *Manager) IsEnabled() bool {
	return m.config.PushoverEnabled &&
		m.config.PushoverToken != "" &&
		m.config.PushoverUser != ""
}

// SendNotification sends a notification event
func (m *Manager) SendNotification(ctx context.Context, event *NotificationEvent) error {
	if !m.IsEnabled() {
		m.logger.Debug("Notifications disabled, skipping", "type", event.Type)
		return nil
	}

	// Check if this notification type is enabled
	if !m.isNotificationTypeEnabled(event.Type) {
		m.logger.Debug("Notification type disabled", "type", event.Type)
		return nil
	}

	// Check priority threshold
	if !m.meetsPriorityThreshold(event) {
		m.logger.Debug("Notification below priority threshold", "type", event.Type, "priority", event.Priority)
		return nil
	}

	// Apply smart rate limiting (priority-based)
	if !m.shouldSendSmart(event) {
		m.logger.Debug("Notification rate limited", "type", event.Type)
		m.stats.RateLimited++
		return nil
	}

	// Enrich with location and context data
	m.enrichNotification(event)

	// Format the notification
	m.formatNotification(event)

	// Send the notification with retries
	return m.sendWithRetry(ctx, event)
}

// isNotificationTypeEnabled checks if a notification type is enabled
func (m *Manager) isNotificationTypeEnabled(notType NotificationType) bool {
	switch notType {
	case NotificationFailover:
		return m.config.NotifyOnFailover
	case NotificationFailback:
		return m.config.NotifyOnFailback
	case NotificationMemberDown:
		return m.config.NotifyOnMemberDown
	case NotificationMemberUp:
		return m.config.NotifyOnMemberUp
	case NotificationPredictive:
		return m.config.NotifyOnPredictive
	case NotificationCriticalError:
		return m.config.NotifyOnCritical
	case NotificationRecovery:
		return m.config.NotifyOnRecovery
	case NotificationStatusUpdate:
		return m.config.NotifyOnStatusUpdate
	default:
		return true // Default to enabled for unknown types
	}
}

// shouldSend determines if a notification should be sent based on rate limiting
func (m *Manager) shouldSend(event *NotificationEvent) bool {
	m.mu.Lock()
	defer m.mu.Unlock()

	now := time.Now()

	// Emergency notifications bypass most rate limiting
	if event.Priority == PriorityEmergency {
		if now.Sub(m.lastEmergency) < m.config.EmergencyCooldown {
			return false
		}
		m.lastEmergency = now
		return true
	}

	// Check type-specific cooldown
	if lastSent, exists := m.lastNotification[event.Type]; exists {
		if now.Sub(lastSent) < m.config.CooldownPeriod {
			return false
		}
	}

	// Check hourly rate limiting
	hourKey := now.Format("2006-01-02-15")
	if m.notificationCount[hourKey] >= m.config.MaxNotificationsHour {
		return false
	}

	// Update tracking
	m.lastNotification[event.Type] = now
	m.notificationCount[hourKey]++

	// Cleanup old hour counters (keep last 24 hours)
	for key := range m.notificationCount {
		if len(key) == 13 { // Format: "2006-01-02-15"
			if keyTime, err := time.Parse("2006-01-02-15", key); err == nil {
				if now.Sub(keyTime) > 24*time.Hour {
					delete(m.notificationCount, key)
				}
			}
		}
	}

	return true
}

// formatNotification formats the notification message based on type and context
func (m *Manager) formatNotification(event *NotificationEvent) {
	// Set default priority if not set
	if event.Priority == 0 {
		event.Priority = m.getPriorityForType(event.Type)
	}

	// Set sound based on priority if not set
	if event.Sound == "" {
		event.Sound = m.getSoundForPriority(event.Priority)
	}

	// Add hostname prefix if enabled
	if m.config.IncludeHostname {
		event.Title = fmt.Sprintf("%s - %s", m.hostname, event.Title)
	}

	// Add timestamp if enabled and not already formatted
	if m.config.IncludeTimestamp && !strings.Contains(event.Message, "Time:") {
		timeStr := event.Timestamp.Format("2006-01-02 15:04:05")
		event.Message = fmt.Sprintf("%s\n\nTime: %s", event.Message, timeStr)
	}
}

// getPriorityForType returns the configured priority for a notification type
func (m *Manager) getPriorityForType(notType NotificationType) int {
	switch notType {
	case NotificationFailover:
		return m.config.PriorityFailover
	case NotificationFailback:
		return m.config.PriorityFailback
	case NotificationMemberDown:
		return m.config.PriorityMemberDown
	case NotificationMemberUp:
		return m.config.PriorityMemberUp
	case NotificationPredictive:
		return m.config.PriorityPredictive
	case NotificationCriticalError:
		return m.config.PriorityCritical
	case NotificationRecovery:
		return m.config.PriorityRecovery
	case NotificationStatusUpdate:
		return m.config.PriorityStatusUpdate
	default:
		return PriorityNormal
	}
}

// getSoundForPriority returns appropriate sound for priority level
func (m *Manager) getSoundForPriority(priority int) string {
	switch priority {
	case PriorityEmergency:
		return "alien" // Emergency - immediate attention
	case PriorityHigh:
		return "siren" // High - requires attention
	case PriorityNormal:
		return "magic" // Normal - standard notification
	case PriorityLow:
		return "pushover" // Low - quiet notification
	case PriorityLowest:
		return "none" // Lowest - no sound
	default:
		return "pushover" // Default sound
	}
}

// sendWithRetry sends notification with retry logic
func (m *Manager) sendWithRetry(ctx context.Context, event *NotificationEvent) error {
	var lastErr error

	for attempt := 0; attempt <= m.config.RetryAttempts; attempt++ {
		if attempt > 0 {
			// Wait before retry
			select {
			case <-time.After(m.config.RetryDelay):
			case <-ctx.Done():
				return ctx.Err()
			}

			m.logger.Debug("Retrying notification",
				"type", event.Type,
				"attempt", attempt+1,
				"max_attempts", m.config.RetryAttempts+1)
		}

		err := m.sendPushoverNotification(ctx, event)
		if err == nil {
			m.stats.TotalSent++
			m.stats.LastSentTime = time.Now()
			m.logger.Info("Notification sent successfully",
				"type", event.Type,
				"priority", event.Priority,
				"attempt", attempt+1)
			return nil
		}

		lastErr = err
		m.logger.Warn("Notification send failed",
			"type", event.Type,
			"attempt", attempt+1,
			"error", err)
	}

	m.stats.TotalFailed++
	m.stats.LastFailureTime = time.Now()
	m.logger.Error("Notification failed after all retries",
		"type", event.Type,
		"attempts", m.config.RetryAttempts+1,
		"error", lastErr)

	return fmt.Errorf("notification failed after %d attempts: %w",
		m.config.RetryAttempts+1, lastErr)
}

// sendPushoverNotification sends notification via Pushover API
func (m *Manager) sendPushoverNotification(ctx context.Context, event *NotificationEvent) error {
	// Prepare form data
	data := url.Values{}
	data.Set("token", m.config.PushoverToken)
	data.Set("user", m.config.PushoverUser)
	data.Set("title", event.Title)
	data.Set("message", event.Message)
	data.Set("priority", fmt.Sprintf("%d", event.Priority))

	if event.Sound != "" {
		data.Set("sound", event.Sound)
	}

	if m.config.PushoverDevice != "" {
		data.Set("device", m.config.PushoverDevice)
	}

	if event.URL != "" {
		data.Set("url", event.URL)
		if event.URLTitle != "" {
			data.Set("url_title", event.URLTitle)
		}
	}

	// Handle emergency notifications
	if event.Priority == PriorityEmergency {
		data.Set("retry", "60")    // Retry every 60 seconds
		data.Set("expire", "3600") // Expire after 1 hour
	}

	// Add location data if available
	if event.Location != nil {
		// Pushover supports location data
		data.Set("location", fmt.Sprintf("%.6f,%.6f", event.Location.Latitude, event.Location.Longitude))
		if event.Location.Address != "" {
			data.Set("location_name", event.Location.Address)
		}
	}

	// Add rich context as supplementary URL if available
	if m.config.RichContextEnabled && len(event.PerformanceMetrics) > 0 {
		// Could generate a URL to a dashboard or metrics page
		// For now, we'll include key metrics in the message itself
	}

	// Create request
	req, err := http.NewRequestWithContext(ctx, "POST",
		"https://api.pushover.net/1/messages.json",
		strings.NewReader(data.Encode()))
	if err != nil {
		return fmt.Errorf("failed to create request: %w", err)
	}

	req.Header.Set("Content-Type", "application/x-www-form-urlencoded")
	req.Header.Set("User-Agent", "autonomy/1.0")

	// Send request
	resp, err := m.httpClient.Do(req)
	if err != nil {
		return fmt.Errorf("HTTP request failed: %w", err)
	}
	defer resp.Body.Close()

	// Check response
	if resp.StatusCode != http.StatusOK {
		var buf bytes.Buffer
		buf.ReadFrom(resp.Body)
		return fmt.Errorf("Pushover API error: %d %s - %s",
			resp.StatusCode, resp.Status, buf.String())
	}

	// Parse response for any errors
	var pushoverResp struct {
		Status  int      `json:"status"`
		Request string   `json:"request"`
		Errors  []string `json:"errors,omitempty"`
	}

	if err := json.NewDecoder(resp.Body).Decode(&pushoverResp); err != nil {
		// Response parsing failed, but HTTP was OK, so consider it success
		return nil
	}

	if pushoverResp.Status != 1 {
		return fmt.Errorf("Pushover API returned status %d: %v",
			pushoverResp.Status, pushoverResp.Errors)
	}

	return nil
}

// GetStats returns notification statistics
func (m *Manager) GetStats() map[string]interface{} {
	m.mu.Lock()
	defer m.mu.Unlock()

	return map[string]interface{}{
		"total_sent":        m.stats.TotalSent,
		"total_failed":      m.stats.TotalFailed,
		"rate_limited":      m.stats.RateLimited,
		"last_sent_time":    m.stats.LastSentTime,
		"last_failure_time": m.stats.LastFailureTime,
		"enabled":           m.IsEnabled(),
		"config": map[string]interface{}{
			"cooldown_period":        m.config.CooldownPeriod,
			"max_notifications_hour": m.config.MaxNotificationsHour,
			"retry_attempts":         m.config.RetryAttempts,
		},
	}
}

// UpdateConfig updates the notification configuration
func (m *Manager) UpdateConfig(config *NotificationConfig) {
	m.mu.Lock()
	defer m.mu.Unlock()

	m.config = config
	m.httpClient.Timeout = config.HTTPTimeout

	m.logger.Info("Notification configuration updated",
		"enabled", config.PushoverEnabled,
		"cooldown", config.CooldownPeriod,
		"max_per_hour", config.MaxNotificationsHour)
}

// Close cleans up the notification manager
func (m *Manager) Close() error {
	// Close HTTP client if it has a custom transport
	if transport, ok := m.httpClient.Transport.(*http.Transport); ok {
		transport.CloseIdleConnections()
	}

	m.logger.Info("Notification manager closed")
	return nil
}

// meetsPriorityThreshold checks if notification meets the configured priority threshold
func (m *Manager) meetsPriorityThreshold(event *NotificationEvent) bool {
	if event.Priority == 0 {
		event.Priority = m.getPriorityForType(event.Type)
	}

	thresholdPriority := m.getThresholdPriority()
	return event.Priority >= thresholdPriority
}

// getThresholdPriority converts string threshold to numeric priority
func (m *Manager) getThresholdPriority() int {
	switch strings.ToLower(m.config.PriorityThreshold) {
	case "emergency":
		return PriorityEmergency
	case "critical":
		return PriorityHigh
	case "warning":
		return PriorityNormal
	case "info":
		return PriorityLow
	default:
		return PriorityNormal // Default to normal
	}
}

// shouldSendSmart implements priority-based rate limiting
func (m *Manager) shouldSendSmart(event *NotificationEvent) bool {
	m.mu.Lock()
	defer m.mu.Unlock()

	now := time.Now()

	// Set priority if not already set
	if event.Priority == 0 {
		event.Priority = m.getPriorityForType(event.Type)
	}

	// Emergency notifications bypass most rate limiting but have their own retry interval
	if event.Priority == PriorityEmergency {
		if now.Sub(m.lastEmergency) < m.config.EmergencyRetryInterval {
			return false
		}
		m.lastEmergency = now
		return true
	}

	// Get priority-based cooldown
	cooldown := m.getPriorityCooldown(event.Priority)

	// Check type-specific cooldown with priority override
	if lastSent, exists := m.lastNotification[event.Type]; exists {
		if now.Sub(lastSent) < cooldown {
			return false
		}
	}

	// Check hourly rate limiting (less restrictive for higher priorities)
	hourKey := now.Format("2006-01-02-15")
	maxPerHour := m.getMaxNotificationsForPriority(event.Priority)
	if m.notificationCount[hourKey] >= maxPerHour {
		return false
	}

	// Update tracking
	m.lastNotification[event.Type] = now
	m.notificationCount[hourKey]++

	// Cleanup old hour counters
	for key := range m.notificationCount {
		if len(key) == 13 { // Format: "2006-01-02-15"
			if keyTime, err := time.Parse("2006-01-02-15", key); err == nil {
				if now.Sub(keyTime) > 24*time.Hour {
					delete(m.notificationCount, key)
				}
			}
		}
	}

	return true
}

// getPriorityCooldown returns cooldown period based on priority
func (m *Manager) getPriorityCooldown(priority int) time.Duration {
	switch priority {
	case PriorityEmergency:
		return m.config.EmergencyRetryInterval
	case PriorityHigh:
		return m.config.CriticalCooldown
	case PriorityNormal:
		return m.config.WarningCooldown
	case PriorityLow, PriorityLowest:
		return m.config.InfoCooldown
	default:
		return m.config.CooldownPeriod
	}
}

// getMaxNotificationsForPriority returns max notifications per hour based on priority
func (m *Manager) getMaxNotificationsForPriority(priority int) int {
	switch priority {
	case PriorityEmergency:
		return 60 // Emergency can send every minute
	case PriorityHigh:
		return 20 // Critical gets normal limit
	case PriorityNormal:
		return 10 // Warning gets reduced limit
	case PriorityLow, PriorityLowest:
		return 4 // Info gets very limited
	default:
		return m.config.MaxNotificationsHour
	}
}

// enrichNotification adds location data and rich context
func (m *Manager) enrichNotification(event *NotificationEvent) {
	// Add location data if enabled
	if m.config.LocationEnabled {
		if location := m.getLocationData(); location != nil {
			event.Location = location
		}
	}

	// Add rich context if enabled
	if m.config.RichContextEnabled {
		m.addRichContext(event)
	}
}

// getLocationData retrieves current location from available sources
func (m *Manager) getLocationData() *LocationData {
	// Try to get location from Starlink API first
	if location := m.getStarlinkLocation(); location != nil {
		return location
	}

	// Try to get location from RUTOS GPS
	if location := m.getRutosLocation(); location != nil {
		return location
	}

	// Could add manual/configured location as fallback
	return nil
}

// getStarlinkLocation gets GPS coordinates from Starlink API
func (m *Manager) getStarlinkLocation() *LocationData {
	// This would integrate with the Starlink collector to get GPS data
	// For now, return nil - this would be implemented when we integrate
	// with the actual Starlink collector
	return nil
}

// getRutosLocation gets GPS coordinates from RUTOS GPS system
func (m *Manager) getRutosLocation() *LocationData {
	// This would integrate with RUTOS GPS via ubus
	// For now, return nil - this would be implemented for actual RUTOS integration
	return nil
}

// addRichContext adds performance metrics and trend data
func (m *Manager) addRichContext(event *NotificationEvent) {
	if event.Metrics != nil {
		// Add performance metrics summary
		event.PerformanceMetrics = make(map[string]float64)
		if event.Metrics.LatencyMS != nil {
			event.PerformanceMetrics["latency_ms"] = *event.Metrics.LatencyMS
		}
		if event.Metrics.LossPercent != nil {
			event.PerformanceMetrics["loss_percent"] = *event.Metrics.LossPercent
		}
		if event.Metrics.JitterMS != nil {
			event.PerformanceMetrics["jitter_ms"] = *event.Metrics.JitterMS
		}

		// Add class-specific metrics
		if event.Member != nil {
			switch event.Member.Class {
			case "starlink":
				if event.Metrics.ObstructionPct != nil {
					event.PerformanceMetrics["obstruction_percent"] = *event.Metrics.ObstructionPct
				}
				if event.Metrics.SNR != nil {
					event.PerformanceMetrics["snr_db"] = float64(*event.Metrics.SNR)
				}
			case "cellular":
				if event.Metrics.RSRP != nil {
					event.PerformanceMetrics["rsrp_dbm"] = float64(*event.Metrics.RSRP)
				}
				if event.Metrics.RSRQ != nil {
					event.PerformanceMetrics["rsrq_db"] = float64(*event.Metrics.RSRQ)
				}
				if event.Metrics.SINR != nil {
					event.PerformanceMetrics["sinr_db"] = float64(*event.Metrics.SINR)
				}
			case "wifi":
				if event.Metrics.SignalStrength != nil {
					event.PerformanceMetrics["signal_strength_dbm"] = float64(*event.Metrics.SignalStrength)
				}
				if event.Metrics.LinkQuality != nil {
					event.PerformanceMetrics["link_quality_percent"] = float64(*event.Metrics.LinkQuality)
				}
			}
		}
	}

	// Could add trend data from telemetry store
	// This would require integration with the telemetry system
}
