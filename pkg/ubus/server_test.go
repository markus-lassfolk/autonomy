package ubus

import (
	"testing"
	"time"

	"github.com/markus-lassfolk/autonomy/pkg"
	"github.com/markus-lassfolk/autonomy/pkg/gps"
	"github.com/markus-lassfolk/autonomy/pkg/telem"
	testutil "github.com/markus-lassfolk/autonomy/pkg/testing"
)

// MockController for testing
type MockController struct{}

func NewMockController() *MockController {
	return &MockController{}
}

func (m *MockController) GetMembers() []*pkg.Member {
	return []*pkg.Member{
		{
			Name:      "test_member_1",
			Class:     "cellular",
			Iface:     "wwan0",
			Weight:    100,
			Eligible:  true,
			CreatedAt: time.Now(),
			LastSeen:  time.Now(),
		},
		{
			Name:      "test_member_2",
			Class:     "starlink",
			Iface:     "eth0",
			Weight:    200,
			Eligible:  true,
			CreatedAt: time.Now(),
			LastSeen:  time.Now(),
		},
	}
}

func (m *MockController) GetCurrentMember() (*pkg.Member, error) {
	members := m.GetMembers()
	if len(members) > 0 {
		return members[0], nil
	}
	return nil, nil
}

func (m *MockController) Switch(from, to *pkg.Member) error {
	return nil
}

func (m *MockController) SetMembers(members []*pkg.Member) {
	// Mock implementation
}

// MockTelemetryStore for testing
type MockTelemetryStore struct {
	samples     map[string][]*telem.Sample
	events      []*pkg.Event
	memoryUsage int
}

func NewMockTelemetryStore() *MockTelemetryStore {
	return &MockTelemetryStore{
		samples:     make(map[string][]*telem.Sample),
		events:      []*pkg.Event{},
		memoryUsage: 1024 * 1024, // 1MB
	}
}

func (m *MockTelemetryStore) AddSample(member string, metrics *pkg.Metrics, score *pkg.Score) {
	// Mock implementation
}

func (m *MockTelemetryStore) GetSamples(member string, since time.Time) ([]*telem.Sample, error) {
	samples, exists := m.samples[member]
	if !exists {
		// Return mock samples
		return []*telem.Sample{
			{
				Timestamp: time.Now(),
				Member:    member,
				Metrics: &pkg.Metrics{
					Timestamp:   time.Now(),
					LatencyMS:   &[]float64{50.0}[0],
					LossPercent: &[]float64{1.0}[0],
					JitterMS:    &[]float64{5.0}[0],
				},
				Score: &pkg.Score{
					Final: 85.0,
				},
			},
		}, nil
	}

	var result []*telem.Sample
	for _, sample := range samples {
		if sample.Timestamp.After(since) {
			result = append(result, sample)
		}
	}
	return result, nil
}

func (m *MockTelemetryStore) GetEvents(since time.Time, limit int) ([]*pkg.Event, error) {
	var result []*pkg.Event
	count := 0
	for _, event := range m.events {
		if event.Timestamp.After(since) && count < limit {
			result = append(result, event)
			count++
		}
	}
	return result, nil
}

func (m *MockTelemetryStore) GetMemoryUsage() int {
	return m.memoryUsage
}

// MockDecisionEngine for testing
type MockDecisionEngine struct {
	available bool
}

func NewMockDecisionEngine() *MockDecisionEngine {
	return &MockDecisionEngine{available: true}
}

func (m *MockDecisionEngine) Tick(controller pkg.Controller) error {
	return nil
}

func (m *MockDecisionEngine) GetMemberState(memberName string) (*pkg.MemberState, error) {
	return &pkg.MemberState{
		Status: "active",
	}, nil
}

func TestServer_NewServer(t *testing.T) {
	tf := testutil.NewTestFramework(t)
	controller := tf.MockController()
	store := tf.MockTelemetryStore()
	decision := tf.MockDecisionEngine()
	logger := tf.MockLogger()

	gpsCollector := gps.NewGPSCollector(logger)
	server := NewServer(controller, decision, store, gpsCollector, logger)

	if server == nil {
		t.Error("NewServer() returned nil")
	}

	if server.logger != logger {
		t.Error("NewServer() logger not set correctly")
	}
}

func TestServer_GetStatus(t *testing.T) {
	tf := testutil.NewTestFramework(t)
	controller := tf.MockController()
	store := tf.MockTelemetryStore()
	decision := tf.MockDecisionEngine()
	logger := tf.MockLogger()

	gpsCollector := gps.NewGPSCollector(logger)
	server := NewServer(controller, decision, store, gpsCollector, logger)

	response, err := server.GetStatus()
	if err != nil {
		t.Errorf("GetStatus() error = %v", err)
	}

	if response == nil {
		t.Error("GetStatus() returned nil response")
		return
	}

	// Verify response structure
	if response.Members == nil {
		t.Error("GetStatus() members should not be nil")
	}

	if response.Health == nil {
		t.Error("GetStatus() health should not be nil")
	}
}

func TestServer_GetInfo(t *testing.T) {
	tf := testutil.NewTestFramework(t)
	controller := tf.MockController()
	store := tf.MockTelemetryStore()
	decision := tf.MockDecisionEngine()
	logger := tf.MockLogger()

	gpsCollector := gps.NewGPSCollector(logger)
	server := NewServer(controller, decision, store, gpsCollector, logger)

	response, err := server.GetInfo()
	if err != nil {
		t.Errorf("GetInfo() error = %v", err)
	}

	if response == nil {
		t.Error("GetInfo() returned nil response")
		return
	}

	// Verify response structure
	if response.Version == "" {
		t.Error("GetInfo() version should not be empty")
	}

	if response.Platform == "" {
		t.Error("GetInfo() platform should not be empty")
	}

	if response.Uptime < 0 {
		t.Error("GetInfo() uptime should not be negative")
	}

	if response.MemoryUsage == nil {
		t.Error("GetInfo() memory usage should not be nil")
	}

	if response.Stats == nil {
		t.Error("GetInfo() stats should not be nil")
	}
}

func TestServer_GetConfig(t *testing.T) {
	tf := testutil.NewTestFramework(t)
	controller := tf.MockController()
	store := tf.MockTelemetryStore()
	decision := tf.MockDecisionEngine()
	logger := tf.MockLogger()

	gpsCollector := gps.NewGPSCollector(logger)
	server := NewServer(controller, decision, store, gpsCollector, logger)

	response, err := server.GetConfig()
	if err != nil {
		t.Errorf("GetConfig() error = %v", err)
	}

	if response == nil {
		t.Error("GetConfig() returned nil response")
		return
	}

	// Verify response structure
	if response.Config == nil {
		t.Error("GetConfig() config should not be nil")
	}

	// Verify essential config sections exist
	config := response.Config
	if _, ok := config["members"]; !ok {
		t.Error("GetConfig() should have members section")
	}

	if _, ok := config["system"]; !ok {
		t.Error("GetConfig() should have system section")
	}
}

func TestServer_GetMetrics(t *testing.T) {
	tf := testutil.NewTestFramework(t)
	controller := tf.MockController()
	store := tf.MockTelemetryStore()
	decision := tf.MockDecisionEngine()
	logger := tf.MockLogger()

	gpsCollector := gps.NewGPSCollector(logger)
	server := NewServer(controller, decision, store, gpsCollector, logger)

	response, err := server.GetMetrics("test_member_1", 1)
	if err != nil {
		t.Errorf("GetMetrics() error = %v", err)
	}

	if response == nil {
		t.Error("GetMetrics() returned nil response")
		return
	}

	// Verify response structure
	if response.Member != "test_member_1" {
		t.Errorf("GetMetrics() member = %v, want test_member_1", response.Member)
	}

	if response.Samples == nil {
		t.Error("GetMetrics() samples should not be nil")
	}

	if response.Period <= 0 {
		t.Error("GetMetrics() period should be positive")
	}
}

func TestServer_GetTelemetry(t *testing.T) {
	tf := testutil.NewTestFramework(t)
	controller := tf.MockController()
	store := tf.MockTelemetryStore()
	decision := tf.MockDecisionEngine()
	logger := tf.MockLogger()

	gpsCollector := gps.NewGPSCollector(logger)
	server := NewServer(controller, decision, store, gpsCollector, logger)

	response, err := server.GetTelemetry()
	if err != nil {
		t.Errorf("GetTelemetry() error = %v", err)
	}

	if response == nil {
		t.Error("GetTelemetry() returned nil response")
		return
	}

	// Verify response structure by type assertion
	telemetryResponse, ok := response.(*TelemetryResponse)
	if !ok {
		t.Error("GetTelemetry() response is not of type *TelemetryResponse")
		return
	}

	if telemetryResponse.Members == nil {
		t.Error("GetTelemetry() members should not be nil")
	}

	if telemetryResponse.Events == nil {
		t.Error("GetTelemetry() events should not be nil")
	}

	if telemetryResponse.MemoryUsage == nil {
		t.Error("GetTelemetry() memory usage should not be nil")
	}
}
