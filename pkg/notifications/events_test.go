package notifications

import (
	"errors"
	"strings"
	"testing"

	"github.com/markus-lassfolk/autonomy/pkg"
	"github.com/markus-lassfolk/autonomy/pkg/logx"
)

// Create a mock logger for testing
func newTestLogger() *logx.Logger {
	return logx.NewLogger("debug", "test")
}

func TestEventBuilder_FailoverEvent(t *testing.T) {
	logger := newTestLogger()
	manager := NewManager(nil, logger)
	builder := NewEventBuilder(manager)

	fromMember := &pkg.Member{
		Name:  "starlink",
		Class: "starlink",
		Iface: "wan",
	}

	toMember := &pkg.Member{
		Name:  "cellular",
		Class: "cellular",
		Iface: "wwan0",
	}

	latency := 100.5
	loss := 2.5
	jitter := 10.2
	metrics := &pkg.Metrics{
		LatencyMS:      &latency,
		LossPercent:    &loss,
		JitterMS:       &jitter,
		ObstructionPct: func() *float64 { v := 5.5; return &v }(),
	}

	tests := []struct {
		name       string
		reason     string
		wantEmoji  string
		wantReason string
	}{
		{
			name:       "predictive failover",
			reason:     "predictive",
			wantEmoji:  "🔮",
			wantReason: "Predictive failover triggered",
		},
		{
			name:       "quality failover",
			reason:     "quality",
			wantEmoji:  "📶",
			wantReason: "Signal quality degraded",
		},
		{
			name:       "latency failover",
			reason:     "latency",
			wantEmoji:  "🐌",
			wantReason: "High latency detected",
		},
		{
			name:       "loss failover",
			reason:     "loss",
			wantEmoji:  "📉",
			wantReason: "Packet loss detected",
		},
		{
			name:       "manual failover",
			reason:     "manual",
			wantEmoji:  "👤",
			wantReason: "Manual failover requested",
		},
		{
			name:       "unknown reason",
			reason:     "unknown",
			wantEmoji:  "🔄",
			wantReason: "Failover triggered",
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			event := builder.FailoverEvent(fromMember, toMember, tt.reason, metrics)

			// Check event type
			if event.Type != NotificationFailover {
				t.Errorf("Expected type %s, got %s", NotificationFailover, event.Type)
			}

			// Check title contains emoji
			if !strings.Contains(event.Title, tt.wantEmoji) {
				t.Errorf("Expected title to contain %s, got %s", tt.wantEmoji, event.Title)
			}

			// Check message contains reason
			if !strings.Contains(event.Message, tt.wantReason) {
				t.Errorf("Expected message to contain '%s', got %s", tt.wantReason, event.Message)
			}

			// Check message contains member info
			if !strings.Contains(event.Message, "From: starlink") {
				t.Errorf("Expected message to contain 'From: starlink', got %s", event.Message)
			}

			if !strings.Contains(event.Message, "To: cellular") {
				t.Errorf("Expected message to contain 'To: cellular', got %s", event.Message)
			}

			// Check message contains metrics
			if !strings.Contains(event.Message, "Latency: 100.5 ms") {
				t.Errorf("Expected message to contain latency, got %s", event.Message)
			}

			if !strings.Contains(event.Message, "Loss: 2.5%") {
				t.Errorf("Expected message to contain loss, got %s", event.Message)
			}

			if !strings.Contains(event.Message, "Obstruction: 5.5%") {
				t.Errorf("Expected message to contain obstruction, got %s", event.Message)
			}

			// Check event has proper context
			if event.FromMember != fromMember {
				t.Error("Expected FromMember to be set")
			}

			if event.ToMember != toMember {
				t.Error("Expected ToMember to be set")
			}

			if event.Metrics != metrics {
				t.Error("Expected Metrics to be set")
			}

			if event.Details["reason"] != tt.reason {
				t.Errorf("Expected reason in details to be '%s', got '%v'", tt.reason, event.Details["reason"])
			}
		})
	}
}

func TestEventBuilder_FailbackEvent(t *testing.T) {
	logger := newTestLogger()
	manager := NewManager(nil, logger)
	builder := NewEventBuilder(manager)

	fromMember := &pkg.Member{
		Name:  "cellular",
		Class: "cellular",
		Iface: "wwan0",
	}

	toMember := &pkg.Member{
		Name:  "starlink",
		Class: "starlink",
		Iface: "wan",
	}

	latency := 45.2
	loss := 0.1
	metrics := &pkg.Metrics{
		LatencyMS:      &latency,
		LossPercent:    &loss,
		ObstructionPct: func() *float64 { v := 1.2; return &v }(),
	}

	event := builder.FailbackEvent(fromMember, toMember, metrics)

	// Check event type
	if event.Type != NotificationFailback {
		t.Errorf("Expected type %s, got %s", NotificationFailback, event.Type)
	}

	// Check title
	if !strings.Contains(event.Title, "✅") {
		t.Errorf("Expected title to contain success emoji, got %s", event.Title)
	}

	if !strings.Contains(event.Title, "Network Restored") {
		t.Errorf("Expected title to contain 'Network Restored', got %s", event.Title)
	}

	// Check message contains restoration info
	if !strings.Contains(event.Message, "Primary connection restored") {
		t.Errorf("Expected message to contain restoration info, got %s", event.Message)
	}

	// Check message contains member info
	if !strings.Contains(event.Message, "From: cellular") {
		t.Errorf("Expected message to contain 'From: cellular', got %s", event.Message)
	}

	if !strings.Contains(event.Message, "To: starlink") {
		t.Errorf("Expected message to contain 'To: starlink', got %s", event.Message)
	}

	// Check message contains metrics for starlink
	if !strings.Contains(event.Message, "Restored Connection Quality") {
		t.Errorf("Expected message to contain quality info, got %s", event.Message)
	}

	if !strings.Contains(event.Message, "Obstruction: 1.2%") {
		t.Errorf("Expected message to contain obstruction for starlink, got %s", event.Message)
	}
}

func TestEventBuilder_MemberDownEvent(t *testing.T) {
	logger := newTestLogger()
	manager := NewManager(nil, logger)
	builder := NewEventBuilder(manager)

	tests := []struct {
		name      string
		member    *pkg.Member
		wantEmoji string
	}{
		{
			name: "starlink down",
			member: &pkg.Member{
				Name:  "starlink",
				Class: "starlink",
				Iface: "wan",
			},
			wantEmoji: "🛰️",
		},
		{
			name: "cellular down",
			member: &pkg.Member{
				Name:  "cellular",
				Class: "cellular",
				Iface: "wwan0",
			},
			wantEmoji: "📱",
		},
		{
			name: "wifi down",
			member: &pkg.Member{
				Name:  "wifi",
				Class: "wifi",
				Iface: "wlan0",
			},
			wantEmoji: "📶",
		},
		{
			name: "lan down",
			member: &pkg.Member{
				Name:  "lan",
				Class: "lan",
				Iface: "eth1",
			},
			wantEmoji: "🌐",
		},
		{
			name: "unknown down",
			member: &pkg.Member{
				Name:  "unknown",
				Class: "unknown",
				Iface: "unknown0",
			},
			wantEmoji: "⚠️",
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			latency := 200.0
			loss := 10.0
			metrics := &pkg.Metrics{
				LatencyMS:   &latency,
				LossPercent: &loss,
			}

			// Add class-specific metrics
			if tt.member.Class == "starlink" {
				metrics.ObstructionPct = func() *float64 { v := 15.0; return &v }()
			} else if tt.member.Class == "cellular" {
				rsrp := -95.0
				rsrq := -10.0
				metrics.RSRP = &rsrp
				metrics.RSRQ = &rsrq
			}

			event := builder.MemberDownEvent(tt.member, "connection_timeout", metrics)

			// Check event type
			if event.Type != NotificationMemberDown {
				t.Errorf("Expected type %s, got %s", NotificationMemberDown, event.Type)
			}

			// Check title contains correct emoji
			if !strings.Contains(event.Title, tt.wantEmoji) {
				t.Errorf("Expected title to contain %s, got %s", tt.wantEmoji, event.Title)
			}

			// Check title contains class name
			expectedClass := strings.Title(string(tt.member.Class))
			if !strings.Contains(event.Title, expectedClass) {
				t.Errorf("Expected title to contain '%s', got %s", expectedClass, event.Title)
			}

			// Check message contains member info
			if !strings.Contains(event.Message, tt.member.Name) {
				t.Errorf("Expected message to contain member name '%s', got %s", tt.member.Name, event.Message)
			}

			if !strings.Contains(event.Message, tt.member.Iface) {
				t.Errorf("Expected message to contain interface '%s', got %s", tt.member.Iface, event.Message)
			}

			// Check message contains reason
			if !strings.Contains(event.Message, "connection_timeout") {
				t.Errorf("Expected message to contain reason, got %s", event.Message)
			}

			// Check class-specific metrics
			if tt.member.Class == "starlink" {
				if !strings.Contains(event.Message, "Obstruction: 15.0%") {
					t.Errorf("Expected message to contain obstruction for starlink, got %s", event.Message)
				}
			} else if tt.member.Class == "cellular" {
				if !strings.Contains(event.Message, "RSRP: -95.0 dBm") {
					t.Errorf("Expected message to contain RSRP for cellular, got %s", event.Message)
				}
				if !strings.Contains(event.Message, "RSRQ: -10.0 dB") {
					t.Errorf("Expected message to contain RSRQ for cellular, got %s", event.Message)
				}
			}
		})
	}
}

func TestEventBuilder_MemberUpEvent(t *testing.T) {
	logger := newTestLogger()
	manager := NewManager(nil, logger)
	builder := NewEventBuilder(manager)

	member := &pkg.Member{
		Name:  "backup_cellular",
		Class: "cellular",
		Iface: "wwan1",
	}

	latency := 75.0
	loss := 0.5
	rsrp := -85.0
	rsrq := -8.0
	metrics := &pkg.Metrics{
		LatencyMS:   &latency,
		LossPercent: &loss,
		RSRP:        &rsrp,
		RSRQ:        &rsrq,
	}

	event := builder.MemberUpEvent(member, metrics)

	// Check event type
	if event.Type != NotificationMemberUp {
		t.Errorf("Expected type %s, got %s", NotificationMemberUp, event.Type)
	}

	// Check title
	if !strings.Contains(event.Title, "📱") {
		t.Errorf("Expected title to contain cellular emoji, got %s", event.Title)
	}

	if !strings.Contains(event.Title, "Connection Restored") {
		t.Errorf("Expected title to contain 'Connection Restored', got %s", event.Title)
	}

	// Check message contains restoration info
	if !strings.Contains(event.Message, "connection is back online") {
		t.Errorf("Expected message to contain restoration info, got %s", event.Message)
	}

	// Check message contains member info
	if !strings.Contains(event.Message, member.Name) {
		t.Errorf("Expected message to contain member name, got %s", event.Message)
	}

	// Check message contains quality metrics
	if !strings.Contains(event.Message, "Current Quality") {
		t.Errorf("Expected message to contain quality section, got %s", event.Message)
	}

	if !strings.Contains(event.Message, "RSRP: -85.0 dBm") {
		t.Errorf("Expected message to contain RSRP, got %s", event.Message)
	}
}

func TestEventBuilder_PredictiveEvent(t *testing.T) {
	logger := newTestLogger()
	manager := NewManager(nil, logger)
	builder := NewEventBuilder(manager)

	member := &pkg.Member{
		Name:  "starlink",
		Class: "starlink",
		Iface: "wan",
	}

	latency := 120.0
	loss := 1.5
	metrics := &pkg.Metrics{
		LatencyMS:      &latency,
		LossPercent:    &loss,
		ObstructionPct: func() *float64 { v := 8.5; return &v }(),
	}

	event := builder.PredictiveEvent(member, "Obstruction increasing rapidly", 0.85, metrics)

	// Check event type
	if event.Type != NotificationPredictive {
		t.Errorf("Expected type %s, got %s", NotificationPredictive, event.Type)
	}

	// Check title
	if !strings.Contains(event.Title, "🔮") {
		t.Errorf("Expected title to contain predictive emoji, got %s", event.Title)
	}

	if !strings.Contains(event.Title, "Predictive Warning") {
		t.Errorf("Expected title to contain 'Predictive Warning', got %s", event.Title)
	}

	// Check message contains prediction info
	if !strings.Contains(event.Message, "Potential issue predicted") {
		t.Errorf("Expected message to contain prediction info, got %s", event.Message)
	}

	if !strings.Contains(event.Message, "Obstruction increasing rapidly") {
		t.Errorf("Expected message to contain prediction text, got %s", event.Message)
	}

	if !strings.Contains(event.Message, "Confidence: 85.0%") {
		t.Errorf("Expected message to contain confidence, got %s", event.Message)
	}

	// Check message contains recommendation
	if !strings.Contains(event.Message, "Recommendation") {
		t.Errorf("Expected message to contain recommendation, got %s", event.Message)
	}

	// Check details
	if event.Details["prediction"] != "Obstruction increasing rapidly" {
		t.Errorf("Expected prediction in details, got %v", event.Details["prediction"])
	}

	if event.Details["confidence"] != 0.85 {
		t.Errorf("Expected confidence in details, got %v", event.Details["confidence"])
	}
}

func TestEventBuilder_CriticalErrorEvent(t *testing.T) {
	logger := newTestLogger()
	manager := NewManager(nil, logger)
	builder := NewEventBuilder(manager)

	testErr := errors.New("Database connection failed")
	details := map[string]interface{}{
		"component":    "telemetry_store",
		"error_count":  5,
		"last_success": "2025-01-15T10:30:00Z",
	}

	event := builder.CriticalErrorEvent("Database", testErr, details)

	// Check event type
	if event.Type != NotificationCriticalError {
		t.Errorf("Expected type %s, got %s", NotificationCriticalError, event.Type)
	}

	// Check priority is emergency
	if event.Priority != PriorityEmergency {
		t.Errorf("Expected priority %d, got %d", PriorityEmergency, event.Priority)
	}

	// Check title
	if !strings.Contains(event.Title, "🚨") {
		t.Errorf("Expected title to contain critical emoji, got %s", event.Title)
	}

	if !strings.Contains(event.Title, "CRITICAL") {
		t.Errorf("Expected title to contain 'CRITICAL', got %s", event.Title)
	}

	// Check message contains error info
	if !strings.Contains(event.Message, "Critical error in Database") {
		t.Errorf("Expected message to contain component info, got %s", event.Message)
	}

	if !strings.Contains(event.Message, "Database connection failed") {
		t.Errorf("Expected message to contain error text, got %s", event.Message)
	}

	// Check message contains details
	if !strings.Contains(event.Message, "component: telemetry_store") {
		t.Errorf("Expected message to contain details, got %s", event.Message)
	}

	// Check message contains urgency
	if !strings.Contains(event.Message, "Immediate attention required") {
		t.Errorf("Expected message to contain urgency, got %s", event.Message)
	}
}

func TestEventBuilder_RecoveryEvent(t *testing.T) {
	logger := newTestLogger()
	manager := NewManager(nil, logger)
	builder := NewEventBuilder(manager)

	details := map[string]interface{}{
		"recovery_time": "30 seconds",
		"method":        "automatic_restart",
		"status":        "fully_operational",
	}

	event := builder.RecoveryEvent("Network Controller", details)

	// Check event type
	if event.Type != NotificationRecovery {
		t.Errorf("Expected type %s, got %s", NotificationRecovery, event.Type)
	}

	// Check title
	if !strings.Contains(event.Title, "✅") {
		t.Errorf("Expected title to contain success emoji, got %s", event.Title)
	}

	if !strings.Contains(event.Title, "System Recovered") {
		t.Errorf("Expected title to contain 'System Recovered', got %s", event.Title)
	}

	// Check message contains recovery info
	if !strings.Contains(event.Message, "Network Controller has recovered") {
		t.Errorf("Expected message to contain component recovery info, got %s", event.Message)
	}

	// Check message contains details
	if !strings.Contains(event.Message, "recovery_time: 30 seconds") {
		t.Errorf("Expected message to contain recovery details, got %s", event.Message)
	}

	// Check message contains status
	if !strings.Contains(event.Message, "operating normally") {
		t.Errorf("Expected message to contain status info, got %s", event.Message)
	}
}

func TestEventBuilder_TestEvent(t *testing.T) {
	logger := newTestLogger()
	manager := NewManager(nil, logger)
	builder := NewEventBuilder(manager)

	event := builder.TestEvent()

	// Check event type
	if event.Type != NotificationStatusUpdate {
		t.Errorf("Expected type %s, got %s", NotificationStatusUpdate, event.Type)
	}

	// Check priority is normal
	if event.Priority != PriorityNormal {
		t.Errorf("Expected priority %d, got %d", PriorityNormal, event.Priority)
	}

	// Check sound is set
	if event.Sound != "pushover" {
		t.Errorf("Expected sound 'pushover', got '%s'", event.Sound)
	}

	// Check title
	if !strings.Contains(event.Title, "🧪") {
		t.Errorf("Expected title to contain test emoji, got %s", event.Title)
	}

	if !strings.Contains(event.Title, "Test Notification") {
		t.Errorf("Expected title to contain 'Test Notification', got %s", event.Title)
	}

	// Check message
	if !strings.Contains(event.Message, "test notification") {
		t.Errorf("Expected message to contain test info, got %s", event.Message)
	}

	if !strings.Contains(event.Message, "notifications are working correctly") {
		t.Errorf("Expected message to contain success info, got %s", event.Message)
	}
}

func TestEventBuilder_CustomEvent(t *testing.T) {
	logger := newTestLogger()
	manager := NewManager(nil, logger)
	builder := NewEventBuilder(manager)

	event := builder.CustomEvent(
		NotificationStatusUpdate,
		"Custom Title",
		"Custom Message",
		PriorityHigh,
	)

	// Check all fields are set correctly
	if event.Type != NotificationStatusUpdate {
		t.Errorf("Expected type %s, got %s", NotificationStatusUpdate, event.Type)
	}

	if event.Title != "Custom Title" {
		t.Errorf("Expected title 'Custom Title', got '%s'", event.Title)
	}

	if event.Message != "Custom Message" {
		t.Errorf("Expected message 'Custom Message', got '%s'", event.Message)
	}

	if event.Priority != PriorityHigh {
		t.Errorf("Expected priority %d, got %d", PriorityHigh, event.Priority)
	}

	if event.Timestamp.IsZero() {
		t.Error("Expected timestamp to be set")
	}
}

// Benchmark tests
func BenchmarkEventBuilder_FailoverEvent(b *testing.B) {
	logger := newTestLogger()
	manager := NewManager(nil, logger)
	builder := NewEventBuilder(manager)

	fromMember := &pkg.Member{Name: "starlink", Class: "starlink", Iface: "wan"}
	toMember := &pkg.Member{Name: "cellular", Class: "cellular", Iface: "wwan0"}
	latency := 100.0
	loss := 2.0
	metrics := &pkg.Metrics{
		LatencyMS:      &latency,
		LossPercent:    &loss,
		ObstructionPct: func() *float64 { v := 5.0; return &v }(),
	}

	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		builder.FailoverEvent(fromMember, toMember, "predictive", metrics)
	}
}

func BenchmarkEventBuilder_MemberDownEvent(b *testing.B) {
	logger := newTestLogger()
	manager := NewManager(nil, logger)
	builder := NewEventBuilder(manager)

	member := &pkg.Member{Name: "cellular", Class: "cellular", Iface: "wwan0"}
	latency2 := 200.0
	loss2 := 10.0
	rsrp := -95.0
	rsrq := -10.0
	metrics := &pkg.Metrics{
		LatencyMS:   &latency2,
		LossPercent: &loss2,
		RSRP:        &rsrp,
		RSRQ:        &rsrq,
	}

	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		builder.MemberDownEvent(member, "timeout", metrics)
	}
}
