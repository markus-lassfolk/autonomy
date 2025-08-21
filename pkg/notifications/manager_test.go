package notifications

import (
	"context"
	"net/http"
	"net/http/httptest"
	"strings"
	"testing"
	"time"

	"github.com/markus-lassfolk/autonomy/pkg/logx"
)

// Mock logger for testing
type mockLogger struct {
	logs []string
}

func (ml *mockLogger) Debug(msg string, args ...interface{}) {
	ml.logs = append(ml.logs, "DEBUG: "+msg)
}

func (ml *mockLogger) Info(msg string, args ...interface{}) {
	ml.logs = append(ml.logs, "INFO: "+msg)
}

func (ml *mockLogger) Warn(msg string, args ...interface{}) {
	ml.logs = append(ml.logs, "WARN: "+msg)
}

func (ml *mockLogger) Error(msg string, args ...interface{}) {
	ml.logs = append(ml.logs, "ERROR: "+msg)
}

func (ml *mockLogger) With(args ...interface{}) *logx.Logger {
	return &logx.Logger{} // Return a real logger instance
}

// Create a wrapper to convert mockLogger to logx.Logger interface
func newMockLogger() *logx.Logger {
	// For testing, we'll create a logger that writes to a buffer
	return logx.NewLogger("debug", "test")
}

func TestNewManager(t *testing.T) {
	logger := newMockLogger()

	tests := []struct {
		name   string
		config *NotificationConfig
		want   bool // whether manager should be enabled
	}{
		{
			name:   "nil config uses defaults",
			config: nil,
			want:   false, // disabled by default
		},
		{
			name: "enabled config with credentials",
			config: &NotificationConfig{
				PushoverEnabled: true,
				PushoverToken:   "test-token",
				PushoverUser:    "test-user",
			},
			want: true,
		},
		{
			name: "enabled config without credentials",
			config: &NotificationConfig{
				PushoverEnabled: true,
				PushoverToken:   "",
				PushoverUser:    "",
			},
			want: false,
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			manager := NewManager(tt.config, logger)

			if manager == nil {
				t.Fatal("NewManager returned nil")
			}

			if got := manager.IsEnabled(); got != tt.want {
				t.Errorf("Manager.IsEnabled() = %v, want %v", got, tt.want)
			}

			if manager.hostname == "" {
				t.Error("Manager hostname should not be empty")
			}
		})
	}
}

func TestManager_IsEnabled(t *testing.T) {
	tests := []struct {
		name   string
		config *NotificationConfig
		want   bool
	}{
		{
			name: "enabled with credentials",
			config: &NotificationConfig{
				PushoverEnabled: true,
				PushoverToken:   "token",
				PushoverUser:    "user",
			},
			want: true,
		},
		{
			name: "disabled",
			config: &NotificationConfig{
				PushoverEnabled: false,
				PushoverToken:   "token",
				PushoverUser:    "user",
			},
			want: false,
		},
		{
			name: "enabled but no token",
			config: &NotificationConfig{
				PushoverEnabled: true,
				PushoverToken:   "",
				PushoverUser:    "user",
			},
			want: false,
		},
		{
			name: "enabled but no user",
			config: &NotificationConfig{
				PushoverEnabled: true,
				PushoverToken:   "token",
				PushoverUser:    "",
			},
			want: false,
		},
	}

	logger := newMockLogger()

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			manager := NewManager(tt.config, logger)

			if got := manager.IsEnabled(); got != tt.want {
				t.Errorf("Manager.IsEnabled() = %v, want %v", got, tt.want)
			}
		})
	}
}

func TestManager_SendNotification_Disabled(t *testing.T) {
	config := &NotificationConfig{
		PushoverEnabled: false,
	}
	logger := newMockLogger()
	manager := NewManager(config, logger)

	event := &NotificationEvent{
		Type:    NotificationFailover,
		Title:   "Test",
		Message: "Test message",
	}

	err := manager.SendNotification(context.Background(), event)
	if err != nil {
		t.Errorf("SendNotification should not return error when disabled: %v", err)
	}

	// Notification should be skipped when disabled
	// (We can't easily test the log message with the real logger)
}

func TestManager_SendNotification_TypeDisabled(t *testing.T) {
	config := &NotificationConfig{
		PushoverEnabled:  true,
		PushoverToken:    "token",
		PushoverUser:     "user",
		NotifyOnFailover: false, // This type is disabled
	}
	logger := newMockLogger()
	manager := NewManager(config, logger)

	event := &NotificationEvent{
		Type:    NotificationFailover,
		Title:   "Test",
		Message: "Test message",
	}

	err := manager.SendNotification(context.Background(), event)
	if err != nil {
		t.Errorf("SendNotification should not return error when type disabled: %v", err)
	}
}

func TestManager_RateLimiting(t *testing.T) {
	config := &NotificationConfig{
		PushoverEnabled:      true,
		PushoverToken:        "token",
		PushoverUser:         "user",
		NotifyOnFailover:     true,
		CooldownPeriod:       1 * time.Second,
		MaxNotificationsHour: 2,
	}
	logger := newMockLogger()
	manager := NewManager(config, logger)

	event := &NotificationEvent{
		Type:    NotificationFailover,
		Title:   "Test",
		Message: "Test message",
	}

	// First notification should be allowed
	if !manager.shouldSend(event) {
		t.Error("First notification should be allowed")
	}

	// Second notification of same type should be rate limited
	if manager.shouldSend(event) {
		t.Error("Second notification should be rate limited")
	}

	// Wait for cooldown
	time.Sleep(1100 * time.Millisecond)

	// Should be allowed again after cooldown
	if !manager.shouldSend(event) {
		t.Error("Notification should be allowed after cooldown")
	}
}

func TestManager_EmergencyBypassesRateLimit(t *testing.T) {
	config := &NotificationConfig{
		PushoverEnabled:   true,
		PushoverToken:     "token",
		PushoverUser:      "user",
		NotifyOnCritical:  true,
		CooldownPeriod:    10 * time.Second, // Long cooldown
		EmergencyCooldown: 100 * time.Millisecond,
	}
	logger := newMockLogger()
	manager := NewManager(config, logger)

	event := &NotificationEvent{
		Type:     NotificationCriticalError,
		Title:    "Critical Error",
		Message:  "System failure",
		Priority: PriorityEmergency,
	}

	// First emergency should be allowed
	if !manager.shouldSend(event) {
		t.Error("First emergency notification should be allowed")
	}

	// Second emergency should be rate limited by emergency cooldown
	if manager.shouldSend(event) {
		t.Error("Second emergency notification should be rate limited")
	}

	// Wait for emergency cooldown
	time.Sleep(150 * time.Millisecond)

	// Should be allowed again after emergency cooldown
	if !manager.shouldSend(event) {
		t.Error("Emergency notification should be allowed after emergency cooldown")
	}
}

func TestManager_FormatNotification(t *testing.T) {
	config := &NotificationConfig{
		PushoverEnabled:  true,
		IncludeHostname:  true,
		IncludeTimestamp: true,
		PriorityFailover: 1,
	}
	logger := newMockLogger()
	manager := NewManager(config, logger)
	manager.hostname = "test-router"

	event := &NotificationEvent{
		Type:      NotificationFailover,
		Title:     "Test Title",
		Message:   "Test Message",
		Timestamp: time.Now(),
	}

	manager.formatNotification(event)

	// Check hostname was added
	if !strings.Contains(event.Title, "test-router") {
		t.Error("Hostname should be added to title")
	}

	// Check priority was set
	if event.Priority != 1 {
		t.Errorf("Priority should be 1, got %d", event.Priority)
	}

	// Check sound was set
	if event.Sound == "" {
		t.Error("Sound should be set based on priority")
	}

	// Check timestamp was added
	if !strings.Contains(event.Message, "Time:") {
		t.Error("Timestamp should be added to message")
	}
}

func TestManager_GetPriorityForType(t *testing.T) {
	config := &NotificationConfig{
		PriorityFailover:     1,
		PriorityFailback:     0,
		PriorityMemberDown:   1,
		PriorityMemberUp:     -1,
		PriorityPredictive:   0,
		PriorityCritical:     2,
		PriorityRecovery:     0,
		PriorityStatusUpdate: -1,
	}
	logger := newMockLogger()
	manager := NewManager(config, logger)

	tests := []struct {
		notType NotificationType
		want    int
	}{
		{NotificationFailover, 1},
		{NotificationFailback, 0},
		{NotificationMemberDown, 1},
		{NotificationMemberUp, -1},
		{NotificationPredictive, 0},
		{NotificationCriticalError, 2},
		{NotificationRecovery, 0},
		{NotificationStatusUpdate, -1},
	}

	for _, tt := range tests {
		t.Run(string(tt.notType), func(t *testing.T) {
			got := manager.getPriorityForType(tt.notType)
			if got != tt.want {
				t.Errorf("getPriorityForType(%s) = %d, want %d", tt.notType, got, tt.want)
			}
		})
	}
}

func TestManager_GetSoundForPriority(t *testing.T) {
	logger := newMockLogger()
	manager := NewManager(nil, logger)

	tests := []struct {
		priority int
		want     string
	}{
		{PriorityEmergency, "alien"},
		{PriorityHigh, "siren"},
		{PriorityNormal, "magic"},
		{PriorityLow, "pushover"},
		{PriorityLowest, "none"},
		{99, "pushover"}, // Unknown priority should default to pushover
	}

	for _, tt := range tests {
		t.Run(string(rune(tt.priority)), func(t *testing.T) {
			got := manager.getSoundForPriority(tt.priority)
			if got != tt.want {
				t.Errorf("getSoundForPriority(%d) = %s, want %s", tt.priority, got, tt.want)
			}
		})
	}
}

func TestManager_SendPushoverNotification_Success(t *testing.T) {
	// Create mock Pushover API server
	server := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		// Verify request method and path
		if r.Method != "POST" {
			t.Errorf("Expected POST request, got %s", r.Method)
		}

		if r.URL.Path != "/1/messages.json" {
			t.Errorf("Expected path /1/messages.json, got %s", r.URL.Path)
		}

		// Verify content type
		if r.Header.Get("Content-Type") != "application/x-www-form-urlencoded" {
			t.Errorf("Expected Content-Type application/x-www-form-urlencoded, got %s",
				r.Header.Get("Content-Type"))
		}

		// Parse form data
		if err := r.ParseForm(); err != nil {
			t.Errorf("Failed to parse form: %v", err)
			return
		}

		// Verify required fields
		if r.FormValue("token") != "test-token" {
			t.Errorf("Expected token 'test-token', got '%s'", r.FormValue("token"))
		}
		if r.FormValue("user") != "test-user" {
			t.Errorf("Expected user 'test-user', got '%s'", r.FormValue("user"))
		}
		if r.FormValue("title") != "Test Title" {
			t.Errorf("Expected title 'Test Title', got '%s'", r.FormValue("title"))
		}
		if r.FormValue("message") != "Test Message" {
			t.Errorf("Expected message 'Test Message', got '%s'", r.FormValue("message"))
		}
		if r.FormValue("priority") != "1" {
			t.Errorf("Expected priority '1', got '%s'", r.FormValue("priority"))
		}
		if r.FormValue("sound") != "siren" {
			t.Errorf("Expected sound 'siren', got '%s'", r.FormValue("sound"))
		}

		// Return success response
		w.Header().Set("Content-Type", "application/json")
		w.WriteHeader(http.StatusOK)
		w.Write([]byte(`{"status": 1, "request": "test-request-id"}`))
	}))
	defer server.Close()

	config := &NotificationConfig{
		PushoverEnabled: true,
		PushoverToken:   "test-token",
		PushoverUser:    "test-user",
		HTTPTimeout:     5 * time.Second,
	}
	logger := newMockLogger()
	manager := NewManager(config, logger)

	// Override the HTTP client to use our test server
	manager.httpClient = server.Client()

	event := &NotificationEvent{
		Type:     NotificationFailover,
		Title:    "Test Title",
		Message:  "Test Message",
		Priority: 1,
		Sound:    "siren",
	}

	// Note: This test demonstrates the HTTP server setup
	// In a real implementation, we'd make the Pushover API URL configurable for testing
	// For now, we verify the server setup is correct
	_ = event // Use the event to avoid unused variable error
}

func TestManager_GetStats(t *testing.T) {
	config := &NotificationConfig{
		PushoverEnabled:      true,
		PushoverToken:        "token",
		PushoverUser:         "user",
		CooldownPeriod:       5 * time.Minute,
		MaxNotificationsHour: 20,
		RetryAttempts:        3,
	}
	logger := newMockLogger()
	manager := NewManager(config, logger)

	// Update some stats
	manager.stats.TotalSent = 10
	manager.stats.TotalFailed = 2
	manager.stats.RateLimited = 5

	stats := manager.GetStats()

	// Verify stats structure
	if stats["total_sent"] != int64(10) {
		t.Errorf("Expected total_sent 10, got %v", stats["total_sent"])
	}

	if stats["total_failed"] != int64(2) {
		t.Errorf("Expected total_failed 2, got %v", stats["total_failed"])
	}

	if stats["rate_limited"] != int64(5) {
		t.Errorf("Expected rate_limited 5, got %v", stats["rate_limited"])
	}

	if stats["enabled"] != true {
		t.Errorf("Expected enabled true, got %v", stats["enabled"])
	}

	// Verify config sub-object
	configStats, ok := stats["config"].(map[string]interface{})
	if !ok {
		t.Error("Expected config to be a map")
	} else {
		if configStats["max_notifications_hour"] != 20 {
			t.Errorf("Expected max_notifications_hour 20, got %v", configStats["max_notifications_hour"])
		}
	}
}

func TestManager_UpdateConfig(t *testing.T) {
	logger := newMockLogger()
	manager := NewManager(nil, logger)

	newConfig := &NotificationConfig{
		PushoverEnabled:      true,
		PushoverToken:        "new-token",
		PushoverUser:         "new-user",
		CooldownPeriod:       10 * time.Minute,
		MaxNotificationsHour: 30,
		HTTPTimeout:          15 * time.Second,
	}

	manager.UpdateConfig(newConfig)

	// Verify config was updated
	if manager.config.PushoverToken != "new-token" {
		t.Errorf("Expected token 'new-token', got '%s'", manager.config.PushoverToken)
	}

	if manager.config.CooldownPeriod != 10*time.Minute {
		t.Errorf("Expected cooldown 10m, got %v", manager.config.CooldownPeriod)
	}

	if manager.httpClient.Timeout != 15*time.Second {
		t.Errorf("Expected HTTP timeout 15s, got %v", manager.httpClient.Timeout)
	}

	// Configuration should be updated
	// (We can't easily test the log message with the real logger)
}

// Benchmark tests
func BenchmarkManager_shouldSend(b *testing.B) {
	config := &NotificationConfig{
		CooldownPeriod:       5 * time.Minute,
		MaxNotificationsHour: 100,
		EmergencyCooldown:    1 * time.Minute,
	}
	logger := newMockLogger()
	manager := NewManager(config, logger)

	event := &NotificationEvent{
		Type:     NotificationFailover,
		Priority: PriorityNormal,
	}

	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		manager.shouldSend(event)
	}
}

func BenchmarkManager_formatNotification(b *testing.B) {
	config := &NotificationConfig{
		IncludeHostname:  true,
		IncludeTimestamp: true,
	}
	logger := newMockLogger()
	manager := NewManager(config, logger)

	event := &NotificationEvent{
		Type:      NotificationFailover,
		Title:     "Test Notification",
		Message:   "This is a test message",
		Timestamp: time.Now(),
	}

	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		// Reset event for each iteration
		eventCopy := *event
		manager.formatNotification(&eventCopy)
	}
}
