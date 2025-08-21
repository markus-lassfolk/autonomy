package testing

import (
	"context"
	"testing"
	"time"

	"github.com/markus-lassfolk/autonomy/pkg"
	"github.com/markus-lassfolk/autonomy/pkg/controller"
	"github.com/markus-lassfolk/autonomy/pkg/decision"
	"github.com/markus-lassfolk/autonomy/pkg/discovery"
	"github.com/markus-lassfolk/autonomy/pkg/logx"
	"github.com/markus-lassfolk/autonomy/pkg/telem"
	"github.com/markus-lassfolk/autonomy/pkg/uci"
)

// TestFramework provides utilities for testing autonomyd components
type TestFramework struct {
	t *testing.T
}

// NewTestFramework creates a new test framework instance
func NewTestFramework(t *testing.T) *TestFramework {
	return &TestFramework{t: t}
}

// MockConfig creates a test configuration
func (tf *TestFramework) MockConfig() *uci.Config {
	return &uci.Config{
		LogLevel:            "debug",
		PollIntervalMS:      1000,
		DecisionIntervalMS:  5000,
		DiscoveryIntervalMS: 30000,
		CleanupIntervalMS:   60000,
		RetentionHours:      24,
		MaxRAMMB:            50,
		Predictive:          false,
		UseMWAN3:            true,
		MetricsListener:     false,
		HealthListener:      false,
		MetricsPort:         9090,
		HealthPort:          8080,
		Members: map[string]*uci.MemberConfig{
			"starlink": {
				Class:  string(pkg.ClassStarlink),
				Weight: 100,
			},
			"cellular": {
				Class:  string(pkg.ClassCellular),
				Weight: 80,
			},
		},
	}
}

// MockMembers creates test member data
func (tf *TestFramework) MockMembers() []*pkg.Member {
	return []*pkg.Member{
		{
			Name:      "starlink",
			Iface:     "wan",
			Class:     pkg.MemberClassStarlink,
			Eligible:  true,
			Weight:    100,
			CreatedAt: time.Now(),
		},
		{
			Name:      "cellular",
			Iface:     "wwan0",
			Class:     pkg.MemberClassCellular,
			Eligible:  true,
			Weight:    80,
			CreatedAt: time.Now(),
		},
		{
			Name:      "wifi",
			Iface:     "wlan0",
			Class:     pkg.MemberClassWiFi,
			Eligible:  true,
			Weight:    60,
			CreatedAt: time.Now(),
		},
	}
}

// MockMetrics creates test metrics data
func (tf *TestFramework) MockMetrics() pkg.Metrics {
	latency := 50.0
	loss := 0.1
	jitter := 5.0
	return pkg.Metrics{
		Timestamp:   time.Now(),
		LatencyMS:   &latency,
		LossPercent: &loss,
		JitterMS:    &jitter,
	}
}

// MockScore creates test score data
func (tf *TestFramework) MockScore() pkg.Score {
	return pkg.Score{
		Instant:   85.0,
		EWMA:      82.0,
		Final:     83.0,
		UpdatedAt: time.Now(),
	}
}

// MockEvent creates test event data
func (tf *TestFramework) MockEvent(eventType string) pkg.Event {
	return pkg.Event{
		ID:        "test-event",
		Type:      eventType,
		Timestamp: time.Now(),
		Member:    "starlink",
		Reason:    "Test event",
		Data: map[string]interface{}{
			"test": "data",
		},
	}
}

// MockTelemetryStore creates a test telemetry store
func (tf *TestFramework) MockTelemetryStore() *telem.Store {
	store, err := telem.NewStore(24, 50)
	if err != nil {
		tf.t.Fatalf("Failed to create telemetry store: %v", err)
	}
	return store
}

// MockLogger creates a test logger
func (tf *TestFramework) MockLogger() *logx.Logger {
	return logx.NewLogger("debug", "test")
}

// MockController creates a test controller
func (tf *TestFramework) MockController() *controller.Controller {
	cfg := tf.MockConfig()
	logger := tf.MockLogger()
	ctrl, err := controller.NewController(cfg, logger)
	if err != nil {
		tf.t.Fatalf("Failed to create controller: %v", err)
	}

	if err := ctrl.SetMembers(tf.MockMembers()); err != nil {
		tf.t.Fatalf("Failed to set members: %v", err)
	}

	return ctrl
}

// MockDecisionEngine creates a test decision engine
func (tf *TestFramework) MockDecisionEngine() *decision.Engine {
	cfg := tf.MockConfig()
	logger := tf.MockLogger()
	store := tf.MockTelemetryStore()
	return decision.NewEngine(cfg, logger, store)
}

// MockDiscoverer creates a test discoverer
func (tf *TestFramework) MockDiscoverer() *discovery.Discoverer {
	logger := tf.MockLogger()
	return discovery.NewDiscoverer(logger)
}

// AssertEqual compares two values and fails if they're not equal
func (tf *TestFramework) AssertEqual(expected, actual interface{}, message string) {
	if expected != actual {
		tf.t.Errorf("%s: expected %v, got %v", message, expected, actual)
	}
}

// AssertNotNil checks if a value is not nil
func (tf *TestFramework) AssertNotNil(value interface{}, message string) {
	if value == nil {
		tf.t.Errorf("%s: value is nil", message)
	}
}

// AssertTrue checks if a boolean is true
func (tf *TestFramework) AssertTrue(value bool, message string) {
	if !value {
		tf.t.Errorf("%s: expected true, got false", message)
	}
}

// AssertFalse checks if a boolean is false
func (tf *TestFramework) AssertFalse(value bool, message string) {
	if value {
		tf.t.Errorf("%s: expected false, got true", message)
	}
}

// AssertError checks if an error is not nil
func (tf *TestFramework) AssertError(err error, message string) {
	if err == nil {
		tf.t.Errorf("%s: expected error, got nil", message)
	}
}

// AssertNoError checks if an error is nil
func (tf *TestFramework) AssertNoError(err error, message string) {
	if err != nil {
		tf.t.Errorf("%s: unexpected error: %v", message, err)
	}
}

// WaitForCondition waits for a condition to be true
func (tf *TestFramework) WaitForCondition(condition func() bool, timeout time.Duration, message string) {
	ctx, cancel := context.WithTimeout(context.Background(), timeout)
	defer cancel()

	ticker := time.NewTicker(100 * time.Millisecond)
	defer ticker.Stop()

	for {
		select {
		case <-ctx.Done():
			tf.t.Errorf("%s: timeout waiting for condition", message)
			return
		case <-ticker.C:
			if condition() {
				return
			}
		}
	}
}

// RunWithTimeout runs a function with a timeout
func (tf *TestFramework) RunWithTimeout(fn func() error, timeout time.Duration, message string) {
	ctx, cancel := context.WithTimeout(context.Background(), timeout)
	defer cancel()

	done := make(chan error, 1)
	go func() {
		done <- fn()
	}()

	select {
	case <-ctx.Done():
		tf.t.Errorf("%s: timeout", message)
	case err := <-done:
		if err != nil {
			tf.t.Errorf("%s: %v", message, err)
		}
	}
}
