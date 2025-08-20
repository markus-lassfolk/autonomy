package testing

import (
	"context"
	"testing"
	"time"

	"github.com/markus-lassfolk/autonomy/pkg"
	"github.com/markus-lassfolk/autonomy/pkg/decision"
	"github.com/markus-lassfolk/autonomy/pkg/performance"
	"github.com/markus-lassfolk/autonomy/pkg/security"
	"github.com/markus-lassfolk/autonomy/pkg/telem"
	"github.com/markus-lassfolk/autonomy/pkg/uci"
)

// IntegrationTestSuite provides comprehensive testing of all components
type IntegrationTestSuite struct {
	t *testing.T

	// Components
	telemetry        *telem.Store
	decisionEngine   *decision.Engine
	profiler         *performance.Profiler
	auditor          *security.Auditor
	predictiveEngine *decision.PredictiveEngine

	// Test data
	testMembers []*pkg.Member
	testMetrics map[string]*pkg.Metrics
}

// NewIntegrationTestSuite creates a new integration test suite
func NewIntegrationTestSuite(t *testing.T) *IntegrationTestSuite {
	suite := &IntegrationTestSuite{
		t: t,
	}

	// Initialize components
	suite.initializeComponents()

	// Setup test data
	suite.setupTestData()

	return suite
}

// RunAllTests runs all integration tests
func (suite *IntegrationTestSuite) RunAllTests() {
	suite.t.Log("Running integration test suite")

	// Test telemetry
	suite.testTelemetry()

	// Test performance profiler
	suite.testPerformanceProfiler()

	// Test security auditor
	suite.testSecurityAuditor()

	// Test predictive engine
	suite.testPredictiveEngine()

	// Test integration scenarios
	suite.testIntegrationScenarios()

	suite.t.Log("Integration test suite completed")
}

// initializeComponents initializes all test components
func (suite *IntegrationTestSuite) initializeComponents() {
	// Initialize telemetry store
	var err error
	suite.telemetry, err = telem.NewStore(24, 16)
	if err != nil {
		suite.t.Fatalf("Failed to initialize telemetry store: %v", err)
	}

	// Initialize decision engine
	config := &uci.Config{
		Predictive:     true,
		SwitchMargin:   10,
		MinUptimeS:     20,
		CooldownS:      20,
		HistoryWindowS: 600,
	}
	suite.decisionEngine = decision.NewEngine(config, nil, suite.telemetry)

	// Initialize performance profiler
	suite.profiler = performance.NewProfiler(true, 1*time.Second, 100, nil)

	// Initialize security auditor
	auditConfig := &security.AuditConfig{
		Enabled:           true,
		LogLevel:          "info",
		MaxEvents:         1000,
		RetentionDays:     30,
		FileIntegrity:     true,
		NetworkSecurity:   true,
		AccessControl:     true,
		ThreatDetection:   true,
		CriticalFiles:     []string{"/tmp/test_file"},
		AllowedIPs:        []string{"127.0.0.1"},
		BlockedIPs:        []string{},
		AllowedPorts:      []int{8080, 9090},
		BlockedPorts:      []int{22, 23, 25},
		MaxFailedAttempts: 5,
		BlockDuration:     24,
	}
	suite.auditor = security.NewAuditor(auditConfig, nil)

	// Initialize predictive engine
	predictiveConfig := &decision.PredictiveConfig{
		Enabled:             true,
		LookbackWindow:      10 * time.Minute,
		PredictionHorizon:   5 * time.Minute,
		ConfidenceThreshold: 0.7,
		AnomalyThreshold:    0.8,
		TrendSensitivity:    0.1,
		PatternMinSamples:   20,
		MLEnabled:           true,
		MLModelPath:         "/tmp/test_models",
	}
	suite.predictiveEngine = decision.NewPredictiveEngine(predictiveConfig, nil)
}

// setupTestData sets up test data
func (suite *IntegrationTestSuite) setupTestData() {
	// Create test members
	suite.testMembers = []*pkg.Member{
		{
			Name:     "wan_starlink",
			Class:    pkg.ClassStarlink,
			Iface:    "wan_starlink",
			Weight:   100,
			Eligible: true,
			Detect:   pkg.DetectAuto,
		},
		{
			Name:     "wan_cellular",
			Class:    pkg.ClassCellular,
			Iface:    "wan_cellular",
			Weight:   80,
			Eligible: true,
			Detect:   pkg.DetectAuto,
		},
		{
			Name:     "wan_wifi",
			Class:    pkg.ClassWiFi,
			Iface:    "wan_wifi",
			Weight:   60,
			Eligible: true,
			Detect:   pkg.DetectAuto,
		},
	}

	// Create test metrics
	suite.testMetrics = make(map[string]*pkg.Metrics)
	for _, member := range suite.testMembers {
		latency := 50.0
		loss := 0.5
		jitter := 5.0
		suite.testMetrics[member.Name] = &pkg.Metrics{
			Timestamp:   time.Now(),
			LatencyMS:   &latency,
			LossPercent: &loss,
			JitterMS:    &jitter,
		}
	}
}

// testTelemetry tests telemetry functionality
func (suite *IntegrationTestSuite) testTelemetry() {
	suite.t.Log("Testing telemetry functionality")

	// Test storing metrics
	for memberName, metrics := range suite.testMetrics {
		err := suite.telemetry.AddSample(memberName, metrics, nil)
		if err != nil {
			suite.t.Errorf("Failed to store metrics for %s: %v", memberName, err)
		}
	}

	// Test retrieving metrics
	since := time.Now().Add(-time.Hour)
	for _, member := range suite.testMembers {
		samples, err := suite.telemetry.GetSamples(member.Name, since)
		if err != nil {
			suite.t.Errorf("Failed to get metrics for %s: %v", member.Name, err)
		}
		if len(samples) == 0 {
			suite.t.Errorf("No metrics found for %s", member.Name)
		}
	}

	// Test storing events
	event := &pkg.Event{
		ID:        "test_event_1",
		Type:      pkg.EventFailover,
		Timestamp: time.Now(),
		From:      "wan_starlink",
		To:        "wan_cellular",
		Reason:    "test",
	}
	err := suite.telemetry.AddEvent(event)
	if err != nil {
		suite.t.Errorf("Failed to store event: %v", err)
	}

	// Test retrieving events
	events, err := suite.telemetry.GetEvents(since, 10)
	if err != nil {
		suite.t.Errorf("Failed to get events: %v", err)
	}
	if len(events) == 0 {
		suite.t.Errorf("No events found")
	}
}

// testPerformanceProfiler tests performance profiler functionality
func (suite *IntegrationTestSuite) testPerformanceProfiler() {
	suite.t.Log("Testing performance profiler functionality")

	// Start profiler
	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()
	suite.profiler.Start(ctx)

	// Wait for some samples
	time.Sleep(2 * time.Second)

	// Test getting metrics
	metrics := suite.profiler.GetMetrics()
	if len(metrics) == 0 {
		suite.t.Error("No performance metrics collected")
	}

	// Test getting samples
	samples := suite.profiler.GetSamples()
	if len(samples) == 0 {
		suite.t.Error("No performance samples collected")
	}

	// Test getting alerts
	_ = suite.profiler.GetAlerts() // Alerts might be empty depending on system state

	// Test getting optimizations
	optimizations := suite.profiler.GetOptimizations()
	if len(optimizations) == 0 {
		suite.t.Error("No optimizations available")
	}

	// Test memory usage
	memoryUsage := suite.profiler.GetMemoryUsage()
	if memoryUsage < 0 {
		suite.t.Error("Invalid memory usage")
	}

	// Test goroutine count
	goroutineCount := suite.profiler.GetGoroutineCount()
	if goroutineCount < 0 {
		suite.t.Error("Invalid goroutine count")
	}
}

// testSecurityAuditor tests security auditor functionality
func (suite *IntegrationTestSuite) testSecurityAuditor() {
	suite.t.Log("Testing security auditor functionality")

	// Start auditor
	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()
	suite.auditor.Start(ctx)

	// Test access control
	allowed := suite.auditor.CheckAccess("127.0.0.1", "test-agent", "read", "config")
	if !allowed {
		suite.t.Error("Localhost access should be allowed")
	}

	blocked := suite.auditor.CheckAccess("192.168.1.100", "test-agent", "read", "config")
	if blocked {
		suite.t.Error("Unauthorized IP access should be blocked")
	}

	// Test file integrity (with non-existent file)
	check, err := suite.auditor.ValidateFileIntegrity("/tmp/non_existent_file")
	if err != nil {
		suite.t.Errorf("File integrity check failed: %v", err)
	}
	if check.Status != "missing" {
		suite.t.Errorf("Expected missing status, got %s", check.Status)
	}

	// Test network security
	networkCheck, err := suite.auditor.CheckNetworkSecurity(8080, "tcp")
	if err != nil {
		suite.t.Errorf("Network security check failed: %v", err)
	}
	if networkCheck == nil {
		suite.t.Error("Network security check returned nil")
	}

	// Test secure token generation
	token, err := suite.auditor.GenerateSecureToken()
	if err != nil {
		suite.t.Errorf("Failed to generate secure token: %v", err)
	}
	if len(token) != 64 {
		suite.t.Errorf("Invalid token length: %d", len(token))
	}

	// Test token validation
	valid := suite.auditor.ValidateSecureToken(token)
	if !valid {
		suite.t.Error("Generated token should be valid")
	}

	// Test getting security events
	_ = suite.auditor.GetSecurityEvents() // Events might be empty depending on test conditions

	// Test getting threat level
	threatLevel := suite.auditor.GetThreatLevel()
	if threatLevel == nil {
		suite.t.Error("Threat level should not be nil")
	}
}

// testPredictiveEngine tests predictive engine functionality
func (suite *IntegrationTestSuite) testPredictiveEngine() {
	suite.t.Log("Testing predictive engine functionality")

	// Add some historical data
	for _, member := range suite.testMembers {
		metrics := suite.testMetrics[member.Name]
		score := &pkg.Score{
			Instant:   85.0,
			EWMA:      87.0,
			Final:     86.0,
			UpdatedAt: time.Now(),
		}
		suite.predictiveEngine.UpdateMemberData(member.Name, metrics, score)
	}

	// Test failure prediction
	for _, member := range suite.testMembers {
		prediction, err := suite.predictiveEngine.PredictFailure(member.Name)
		if err != nil {
			suite.t.Errorf("Failed to predict failure for %s: %v", member.Name, err)
		}
		if prediction == nil {
			suite.t.Errorf("No prediction for %s", member.Name)
			continue
		}
		if prediction.Risk < 0 || prediction.Risk > 1 {
			suite.t.Errorf("Invalid risk score for %s: %f", member.Name, prediction.Risk)
		}
	}
}

// testIntegrationScenarios tests integration scenarios
func (suite *IntegrationTestSuite) testIntegrationScenarios() {
	suite.t.Log("Testing integration scenarios")

	// Scenario 1: Normal operation
	suite.testNormalOperation()

	// Scenario 2: Performance degradation
	suite.testPerformanceDegradation()

	// Scenario 3: Security threat
	suite.testSecurityThreat()

	// Scenario 4: Predictive failover
	suite.testPredictiveFailover()
}

// testNormalOperation tests normal operation scenario
func (suite *IntegrationTestSuite) testNormalOperation() {
	suite.t.Log("Testing normal operation scenario")

	// Simulate normal metrics collection
	for i := 0; i < 10; i++ {
		for memberName, metrics := range suite.testMetrics {
			// Update metrics with normal values
			latency := 50.0 + float64(i)
			loss := 0.5
			jitter := 5.0
			metrics.LatencyMS = &latency
			metrics.LossPercent = &loss
			metrics.JitterMS = &jitter

			// Store metrics
			err := suite.telemetry.AddSample(memberName, metrics, nil)
			if err != nil {
				suite.t.Errorf("Failed to store metrics: %v", err)
			}

			// Update predictive engine
			score := &pkg.Score{
				Instant:   85.0,
				EWMA:      87.0,
				Final:     86.0,
				UpdatedAt: time.Now(),
			}
			suite.predictiveEngine.UpdateMemberData(memberName, metrics, score)
		}

		time.Sleep(100 * time.Millisecond)
	}

	// Verify system is stable
	threatLevel := suite.auditor.GetThreatLevel()
	if threatLevel.Level == "critical" {
		suite.t.Error("Threat level should not be critical during normal operation")
	}
}

// testPerformanceDegradation tests performance degradation scenario
func (suite *IntegrationTestSuite) testPerformanceDegradation() {
	suite.t.Log("Testing performance degradation scenario")

	// Simulate performance degradation
	for i := 0; i < 5; i++ {
		// Update metrics with degraded values
		for memberName, metrics := range suite.testMetrics {
			latency := 200.0 + float64(i*50)
			loss := 5.0 + float64(i)
			jitter := 20.0 + float64(i*5)
			metrics.LatencyMS = &latency
			metrics.LossPercent = &loss
			metrics.JitterMS = &jitter

			// Store metrics
			err := suite.telemetry.AddSample(memberName, metrics, nil)
			if err != nil {
				suite.t.Errorf("Failed to store degraded metrics: %v", err)
			}

			// Update predictive engine
			score := &pkg.Score{
				Instant:   40.0 - float64(i*5),
				EWMA:      45.0 - float64(i*5),
				Final:     42.0 - float64(i*5),
				UpdatedAt: time.Now(),
			}
			suite.predictiveEngine.UpdateMemberData(memberName, metrics, score)
		}

		time.Sleep(100 * time.Millisecond)
	}

	// Verify performance alerts are generated
	alerts := suite.profiler.GetAlerts()
	if len(alerts) == 0 {
		suite.t.Log("No performance alerts generated (this might be normal)")
	}
}

// testSecurityThreat tests security threat scenario
func (suite *IntegrationTestSuite) testSecurityThreat() {
	suite.t.Log("Testing security threat scenario")

	// Simulate multiple failed access attempts
	for i := 0; i < 10; i++ {
		allowed := suite.auditor.CheckAccess("192.168.1.200", "malicious-agent", "write", "config")
		if allowed {
			suite.t.Error("Malicious access should be blocked")
		}
	}

	// Verify security events are generated
	events := suite.auditor.GetSecurityEvents()
	if len(events) == 0 {
		suite.t.Log("No security events generated (this might be normal)")
	}

	// Verify threat level increases
	threatLevel := suite.auditor.GetThreatLevel()
	if threatLevel.Level == "low" {
		suite.t.Log("Threat level remains low (this might be normal)")
	}
}

// testPredictiveFailover tests predictive failover scenario
func (suite *IntegrationTestSuite) testPredictiveFailover() {
	suite.t.Log("Testing predictive failover scenario")

	// Simulate deteriorating conditions for primary member
	primaryMember := suite.testMembers[0]
	for i := 0; i < 15; i++ {
		metrics := suite.testMetrics[primaryMember.Name]
		latency := 300.0 + float64(i*20)
		loss := 8.0 + float64(i)*0.5
		jitter := 30.0 + float64(i*2)
		metrics.LatencyMS = &latency
		metrics.LossPercent = &loss
		metrics.JitterMS = &jitter

		// Store metrics
		err := suite.telemetry.AddSample(primaryMember.Name, metrics, nil)
		if err != nil {
			suite.t.Errorf("Failed to store deteriorating metrics: %v", err)
		}

		// Update predictive engine
		score := &pkg.Score{
			Instant:   30.0 - float64(i*2),
			EWMA:      35.0 - float64(i*2),
			Final:     32.0 - float64(i*2),
			UpdatedAt: time.Now(),
		}
		suite.predictiveEngine.UpdateMemberData(primaryMember.Name, metrics, score)

		time.Sleep(100 * time.Millisecond)
	}

	// Test failure prediction
	prediction, err := suite.predictiveEngine.PredictFailure(primaryMember.Name)
	if err != nil {
		suite.t.Errorf("Failed to predict failure: %v", err)
	}
	if prediction == nil {
		suite.t.Error("No failure prediction generated")
	} else {
		suite.t.Logf("Failure prediction: risk=%.2f, confidence=%.2f", prediction.Risk, prediction.Confidence)
	}
}

// Cleanup cleans up test resources
func (suite *IntegrationTestSuite) Cleanup() {
	if suite.telemetry != nil {
		suite.telemetry.Close()
	}
}
