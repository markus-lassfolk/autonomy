package integration

import (
	"context"
	"testing"
	"time"

	"github.com/markus-lassfolk/autonomy/pkg"
	"github.com/markus-lassfolk/autonomy/pkg/collector"
	"github.com/markus-lassfolk/autonomy/pkg/controller"
	"github.com/markus-lassfolk/autonomy/pkg/decision"
	"github.com/markus-lassfolk/autonomy/pkg/discovery"
	"github.com/markus-lassfolk/autonomy/pkg/gps"
	"github.com/markus-lassfolk/autonomy/pkg/health"
	"github.com/markus-lassfolk/autonomy/pkg/logx"
	"github.com/markus-lassfolk/autonomy/pkg/metrics"
	"github.com/markus-lassfolk/autonomy/pkg/mqtt"
	"github.com/markus-lassfolk/autonomy/pkg/performance"
	"github.com/markus-lassfolk/autonomy/pkg/security"
	"github.com/markus-lassfolk/autonomy/pkg/telem"
	"github.com/markus-lassfolk/autonomy/pkg/ubus"
	"github.com/markus-lassfolk/autonomy/pkg/uci"
)

// TestSystemIntegration tests the end-to-end system integration
func TestSystemIntegration(t *testing.T) {
	_, cancel := context.WithTimeout(context.Background(), 30*time.Second)
	defer cancel()

	// Initialize logger
	logger := logx.NewLogger("integration_test", "info")

	// Initialize UCI configuration
	cfg := &uci.Config{
		LogLevel:            "info",
		DecisionIntervalMS:  1000,
		DiscoveryIntervalMS: 30000,
		CleanupIntervalMS:   60000,
		StarlinkAPIHost:     "192.168.100.1",
		StarlinkAPIPort:     9200,
		StarlinkTimeout:     10,
		StarlinkGRPCFirst:   true,
		StarlinkHTTPFirst:   false,
	}

	// Test telemetry store
	store, err := telem.NewStore(24, 64) // 24 hours retention, 64MB max
	if err != nil {
		t.Fatalf("Failed to create telemetry store: %v", err)
	}

	// Test discovery
	discoverer := discovery.NewDiscoverer(logger)
	if discoverer == nil {
		t.Fatal("Failed to create discoverer")
	}

	// Test decision engine
	engine := decision.NewEngine(cfg, logger, store)

	// Test controller
	controller, err := controller.NewController(cfg, logger)
	if err != nil {
		t.Fatalf("Failed to create controller: %v", err)
	}

	// Test collector factory
	collectorConfig := map[string]interface{}{
		"timeout":             time.Duration(cfg.StarlinkTimeout) * time.Second,
		"targets":             []string{"8.8.8.8", "1.1.1.1"},
		"starlink_api_host":   cfg.StarlinkAPIHost,
		"starlink_api_port":   cfg.StarlinkAPIPort,
		"starlink_timeout_s":  cfg.StarlinkTimeout,
		"starlink_grpc_first": cfg.StarlinkGRPCFirst,
		"starlink_http_first": cfg.StarlinkHTTPFirst,
	}
	collectorFactory := collector.NewCollectorFactory(collectorConfig)

	// Test security auditor
	auditConfig := &security.AuditConfig{
		Enabled:         true,
		FileIntegrity:   true,
		NetworkSecurity: true,
		ThreatDetection: true,
		AccessControl:   true,
		RetentionDays:   30,
		MaxEvents:       1000,
	}
	auditor := security.NewAuditor(auditConfig, logger)

	// Test performance profiler
	profiler := performance.NewProfiler(true, 30*time.Second, 1000, logger)

	// Test metrics server
	metricsServer := metrics.NewServer(controller, engine, store, logger)

	// Test health server
	healthServer := health.NewServer(controller, engine, store, logger)

	// Test ubus server - create a mock GPS collector
	gpsCollector := gps.NewGPSCollector(logger)
	ubusServer := ubus.NewServer(controller, engine, store, gpsCollector, logger)

	// Test MQTT client
	mqttConfig := &mqtt.Config{
		Enabled: false, // Disabled for testing
	}
	mqttClient := mqtt.NewClient(mqttConfig, logger)

	// Verify all components are created
	if store == nil {
		t.Error("Telemetry store not created")
	}
	if discoverer == nil {
		t.Error("Discoverer not created")
	}
	if engine == nil {
		t.Error("Decision engine not created")
	}
	if controller == nil {
		t.Error("Controller not created")
	}
	if collectorFactory == nil {
		t.Error("Collector factory not created")
	}
	if auditor == nil {
		t.Error("Security auditor not created")
	}
	if profiler == nil {
		t.Error("Performance profiler not created")
	}
	if metricsServer == nil {
		t.Error("Metrics server not created")
	}
	if healthServer == nil {
		t.Error("Health server not created")
	}
	if ubusServer == nil {
		t.Error("ubus server not created")
	}
	if mqttClient == nil {
		t.Error("MQTT client not created")
	}

	// Test basic functionality
	t.Run("TelemetryStore", func(t *testing.T) {
		testTelemetryStore(t, store)
	})

	t.Run("Controller", func(t *testing.T) {
		testController(t, controller)
	})

	t.Run("DecisionEngine", func(t *testing.T) {
		testDecisionEngine(t, engine, controller)
	})

	t.Run("CollectorFactory", func(t *testing.T) {
		testCollectorFactory(t, collectorFactory)
	})

	t.Run("SecurityAuditor", func(t *testing.T) {
		testSecurityAuditor(t, auditor)
	})

	t.Run("UbusServer", func(t *testing.T) {
		testUbusServer(t, ubusServer)
	})
}

func testTelemetryStore(t *testing.T, store *telem.Store) {
	// Test adding sample
	latency := 50.0
	loss := 1.0
	jitter := 5.0
	metrics := &pkg.Metrics{
		Timestamp:   time.Now(),
		LatencyMS:   &latency,
		LossPercent: &loss,
		JitterMS:    &jitter,
	}
	score := &pkg.Score{
		Final: 85.0,
	}

	store.AddSample("test_member", metrics, score)

	// Test getting samples
	samples, err := store.GetSamples("test_member", time.Now().Add(-time.Hour))
	if err != nil {
		t.Errorf("GetSamples error: %v", err)
	}

	if len(samples) == 0 {
		t.Error("Expected at least one sample")
	}
}

func testController(t *testing.T, controller *controller.Controller) {
	// Test getting members (should be empty initially)
	members := controller.GetMembers()
	if members == nil {
		t.Error("GetMembers returned nil")
	}

	// Test getting current member
	currentMember, err := controller.GetCurrentMember()
	if err != nil {
		t.Logf("GetCurrentMember error (expected for empty controller): %v", err)
	}
	if currentMember != nil {
		t.Logf("Current member: %s", currentMember.Name)
	}
}

func testDecisionEngine(t *testing.T, engine *decision.Engine, controller *controller.Controller) {
	// Test tick operation
	err := engine.Tick(controller)
	if err != nil {
		t.Errorf("Engine.Tick error: %v", err)
	}
}

func testCollectorFactory(t *testing.T, factory *collector.CollectorFactory) {
	// Test creating collectors with mock members
	starlinkMember := &pkg.Member{
		Name:  "test_starlink",
		Class: pkg.ClassStarlink,
		Iface: "eth0",
	}
	starlinkCollector, err := factory.CreateCollector(starlinkMember)
	if err != nil {
		t.Logf("CreateCollector(starlink) error (expected without hardware): %v", err)
	} else if starlinkCollector == nil {
		t.Error("CreateCollector(starlink) returned nil without error")
	}

	cellularMember := &pkg.Member{
		Name:  "test_cellular",
		Class: pkg.ClassCellular,
		Iface: "wwan0",
	}
	cellularCollector, err := factory.CreateCollector(cellularMember)
	if err != nil {
		t.Logf("CreateCollector(cellular) error (expected without hardware): %v", err)
	} else if cellularCollector == nil {
		t.Error("CreateCollector(cellular) returned nil without error")
	}
}

func testSecurityAuditor(t *testing.T, auditor *security.Auditor) {
	// Test file integrity check (using public method)
	// Note: CheckFileIntegrity is private, so we'll test indirectly
	events := auditor.GetSecurityEvents()
	if events == nil {
		t.Error("GetSecurityEvents should not return nil")
	}

	// Test access control
	allowed := auditor.CheckAccess("127.0.0.1", "test_user", "read", "/test")
	if !allowed {
		t.Log("Access denied (expected for test)")
	}
}

func testUbusServer(t *testing.T, server *ubus.Server) {
	// Test status
	status, err := server.GetStatus()
	if err != nil {
		t.Errorf("GetStatus error: %v", err)
	}
	if status == nil {
		t.Error("GetStatus returned nil")
	}

	// Test info
	info, err := server.GetInfo()
	if err != nil {
		t.Errorf("GetInfo error: %v", err)
	}
	if info == nil {
		t.Error("GetInfo returned nil")
	}

	// Test config
	config, err := server.GetConfig()
	if err != nil {
		t.Errorf("GetConfig error: %v", err)
	}
	if config == nil {
		t.Error("GetConfig returned nil")
	}
}
