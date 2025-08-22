package main

import (
	"context"
	"fmt"
	"time"

	"github.com/markus-lassfolk/autonomy/pkg"
	"github.com/markus-lassfolk/autonomy/pkg/logx"
	"github.com/markus-lassfolk/autonomy/pkg/notifications"
)

func main() {
	fmt.Println("🧪 Testing Pushover Integration with Real API Keys")
	fmt.Println("=" + repeatString("=", 60))

	// Create logger
	logger := logx.NewLogger("debug", "pushover-test")

	// Create notification config with real API keys
	config := &notifications.NotificationConfig{
		PushoverEnabled: true,
		PushoverToken:   "aczm9pre8oowwpxmte92utk5gbyub7", // Your app key
		PushoverUser:    "uXLTS5NjcBSj5v6xi7uB8VH4khD6dK", // Your user key
		PushoverDevice:  "",                               // Optional

		// Advanced features
		PriorityThreshold:      "info", // Send all notifications
		AcknowledgmentTracking: true,
		LocationEnabled:        true,
		RichContextEnabled:     true,

		// Notification types
		NotifyOnFailover:   true,
		NotifyOnFailback:   true,
		NotifyOnMemberDown: true,
		NotifyOnPredictive: true,
		NotifyOnCritical:   true,
		NotifyOnRecovery:   true,

		// Rate limiting (relaxed for testing)
		CooldownPeriod:       1 * time.Second,
		MaxNotificationsHour: 60,
		EmergencyCooldown:    1 * time.Second,

		// Priorities
		PriorityFailover:   notifications.PriorityHigh,
		PriorityFailback:   notifications.PriorityNormal,
		PriorityMemberDown: notifications.PriorityHigh,
		PriorityPredictive: notifications.PriorityNormal,
		PriorityCritical:   notifications.PriorityEmergency,
		PriorityRecovery:   notifications.PriorityNormal,

		// Advanced settings
		RetryAttempts:    3,
		RetryDelay:       5 * time.Second,
		HTTPTimeout:      10 * time.Second,
		IncludeHostname:  true,
		IncludeTimestamp: true,
	}

	// Create notification manager
	manager := notifications.NewManager(config, logger)
	builder := notifications.NewEventBuilder(manager)

	fmt.Printf("✅ Notification manager created\n")
	fmt.Printf("📱 Pushover enabled: %v\n", manager.IsEnabled())

	if !manager.IsEnabled() {
		fmt.Printf("❌ Pushover not enabled - check credentials\n")
		return
	}

	// Test 1: Simple test notification
	fmt.Println("\n🧪 Test 1: Simple test notification")
	testEvent := builder.TestEvent()
	if err := manager.SendNotification(context.Background(), testEvent); err != nil {
		fmt.Printf("❌ Test notification failed: %v\n", err)
	} else {
		fmt.Printf("✅ Test notification sent successfully\n")
	}

	time.Sleep(2 * time.Second)

	// Test 2: Failover event with metrics
	fmt.Println("\n🧪 Test 2: Failover event with rich metrics")

	starlinkMember := &pkg.Member{
		Name:  "starlink_primary",
		Class: "starlink",
		Iface: "wan",
	}

	cellularMember := &pkg.Member{
		Name:  "cellular_backup",
		Class: "cellular",
		Iface: "wwan0",
	}

	latency := 450.2
	loss := 3.5
	jitter := 25.8
	metrics := &pkg.Metrics{
		LatencyMS:      &latency,
		LossPercent:    &loss,
		JitterMS:       &jitter,
		ObstructionPct: func() *float64 { v := 12.5; return &v }(),
		SNR:            func() *int { v := -8; return &v }(),
		Timestamp:      time.Now(),
	}

	failoverEvent := builder.FailoverEvent(starlinkMember, cellularMember, "predictive", metrics)
	if err := manager.SendNotification(context.Background(), failoverEvent); err != nil {
		fmt.Printf("❌ Failover notification failed: %v\n", err)
	} else {
		fmt.Printf("✅ Failover notification sent successfully\n")
	}

	time.Sleep(2 * time.Second)

	// Test 3: Member down event with cellular metrics
	fmt.Println("\n🧪 Test 3: Member down event with cellular metrics")

	cellLatency := 200.0
	cellLoss := 8.0
	rsrp := -95.0
	rsrq := -12.0
	sinr := 8.0
	cellularMetrics := &pkg.Metrics{
		LatencyMS:   &cellLatency,
		LossPercent: &cellLoss,
		RSRP:        &rsrp,
		RSRQ:        &rsrq,
		SINR:        &sinr,
		Timestamp:   time.Now(),
	}

	memberDownEvent := builder.MemberDownEvent(cellularMember, "signal_degradation", cellularMetrics)
	if err := manager.SendNotification(context.Background(), memberDownEvent); err != nil {
		fmt.Printf("❌ Member down notification failed: %v\n", err)
	} else {
		fmt.Printf("✅ Member down notification sent successfully\n")
	}

	time.Sleep(2 * time.Second)

	// Test 4: Predictive warning
	fmt.Println("\n🧪 Test 4: Predictive warning event")

	predictiveEvent := builder.PredictiveEvent(
		starlinkMember,
		"Obstruction trending upward rapidly",
		0.87,
		metrics,
	)
	if err := manager.SendNotification(context.Background(), predictiveEvent); err != nil {
		fmt.Printf("❌ Predictive notification failed: %v\n", err)
	} else {
		fmt.Printf("✅ Predictive notification sent successfully\n")
	}

	time.Sleep(2 * time.Second)

	// Test 5: Critical error (Emergency priority)
	fmt.Println("\n🧪 Test 5: Critical error event (Emergency priority)")

	criticalEvent := builder.CriticalErrorEvent(
		"Decision Engine",
		fmt.Errorf("database connection lost"),
		map[string]interface{}{
			"component":     "telemetry_store",
			"error_count":   5,
			"last_success":  time.Now().Add(-10 * time.Minute).Format(time.RFC3339),
			"recovery_time": "unknown",
		},
	)
	if err := manager.SendNotification(context.Background(), criticalEvent); err != nil {
		fmt.Printf("❌ Critical notification failed: %v\n", err)
	} else {
		fmt.Printf("✅ Critical notification sent successfully\n")
	}

	time.Sleep(2 * time.Second)

	// Test 6: Recovery event
	fmt.Println("\n🧪 Test 6: Recovery event")

	recoveryEvent := builder.RecoveryEvent(
		"Decision Engine",
		map[string]interface{}{
			"recovery_time": "45 seconds",
			"method":        "automatic_restart",
			"status":        "fully_operational",
			"checks_passed": 12,
			"performance":   "normal",
		},
	)
	if err := manager.SendNotification(context.Background(), recoveryEvent); err != nil {
		fmt.Printf("❌ Recovery notification failed: %v\n", err)
	} else {
		fmt.Printf("✅ Recovery notification sent successfully\n")
	}

	// Test 7: Rate limiting
	fmt.Println("\n🧪 Test 7: Rate limiting test")

	for i := 1; i <= 3; i++ {
		testEvent := builder.CustomEvent(
			notifications.NotificationStatusUpdate,
			fmt.Sprintf("Rate Limit Test %d", i),
			fmt.Sprintf("This is rate limit test message %d", i),
			notifications.PriorityNormal,
		)

		if err := manager.SendNotification(context.Background(), testEvent); err != nil {
			fmt.Printf("❌ Rate limit test %d failed: %v\n", i, err)
		} else {
			fmt.Printf("✅ Rate limit test %d sent\n", i)
		}

		time.Sleep(500 * time.Millisecond)
	}

	// Display statistics
	fmt.Println("\n📊 Notification Statistics:")
	stats := manager.GetStats()
	fmt.Printf("• Total sent: %v\n", stats["total_sent"])
	fmt.Printf("• Total failed: %v\n", stats["total_failed"])
	fmt.Printf("• Rate limited: %v\n", stats["rate_limited"])
	fmt.Printf("• Last sent: %v\n", stats["last_sent_time"])

	if stats["total_failed"].(int64) > 0 {
		fmt.Printf("• Last failure: %v\n", stats["last_failure_time"])
	}

	fmt.Println("\n🎉 Pushover integration test completed!")
	fmt.Println("Check your Pushover app for the test notifications.")
	fmt.Println("=" + repeatString("=", 60))
}

func repeatString(s string, count int) string {
	result := ""
	for i := 0; i < count; i++ {
		result += s
	}
	return result
}
