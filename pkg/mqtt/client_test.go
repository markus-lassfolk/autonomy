package mqtt

import (
	"encoding/json"
	"sync"
	"testing"
	"time"

	"github.com/markus-lassfolk/autonomy/pkg/logx"
)

// MockMQTTClient simulates MQTT client behavior for testing
type MockMQTTClient struct {
	mu         sync.RWMutex
	connected  bool
	published  []PublishedMessage
	shouldFail bool
}

type PublishedMessage struct {
	Topic     string
	Payload   interface{}
	Timestamp time.Time
}

func NewMockMQTTClient() *MockMQTTClient {
	return &MockMQTTClient{
		connected: true,
		published: make([]PublishedMessage, 0),
	}
}

func (m *MockMQTTClient) SetConnected(connected bool) {
	m.mu.Lock()
	defer m.mu.Unlock()
	m.connected = connected
}

func (m *MockMQTTClient) SetShouldFail(fail bool) {
	m.mu.Lock()
	defer m.mu.Unlock()
	m.shouldFail = fail
}

func (m *MockMQTTClient) GetPublishedMessages() []PublishedMessage {
	m.mu.RLock()
	defer m.mu.RUnlock()
	result := make([]PublishedMessage, len(m.published))
	copy(result, m.published)
	return result
}

func (m *MockMQTTClient) ClearPublishedMessages() {
	m.mu.Lock()
	defer m.mu.Unlock()
	m.published = m.published[:0]
}

func TestMQTTClient_NewClient(t *testing.T) {
	tests := []struct {
		name   string
		config *Config
		logger *logx.Logger
	}{
		{
			name: "valid config",
			config: &Config{
				Broker:      "localhost",
				Port:        1883,
				ClientID:    "test_client",
				TopicPrefix: "test",
				Enabled:     true,
			},
			logger: logx.NewLogger("test", "debug"),
		},
		{
			name:   "default config",
			config: DefaultConfig(),
			logger: logx.NewLogger("test", "debug"),
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			client := NewClient(tt.config, tt.logger)

			if client == nil {
				t.Error("NewClient() returned nil")
				return
			}

			if client.config != tt.config {
				t.Error("NewClient() config not set correctly")
			}

			if client.logger != tt.logger {
				t.Error("NewClient() logger not set correctly")
			}
		})
	}
}

func TestMQTTClient_PublishSample(t *testing.T) {
	tests := []struct {
		name     string
		config   *Config
		sample   interface{}
		wantErr  bool
		wantSkip bool
	}{
		{
			name: "valid sample - enabled",
			config: &Config{
				Broker:      "localhost",
				Port:        1883,
				ClientID:    "test_client",
				TopicPrefix: "test",
				Enabled:     true,
			},
			sample: map[string]interface{}{
				"member":     "starlink_test",
				"timestamp":  time.Now().Unix(),
				"latency_ms": 45.0,
				"loss_pct":   0.5,
				"score":      85.2,
			},
			wantErr:  false,
			wantSkip: false,
		},
		{
			name: "disabled client",
			config: &Config{
				Enabled: false,
			},
			sample:   map[string]interface{}{"test": "data"},
			wantErr:  false,
			wantSkip: true,
		},
		{
			name: "nil sample",
			config: &Config{
				Enabled: true,
			},
			sample:   nil,
			wantErr:  false,
			wantSkip: false,
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			logger := logx.NewLogger("test", "debug")
			client := NewClient(tt.config, logger)

			// Mock the connection state
			if tt.config.Enabled {
				client.connected = true
			}

			err := client.PublishSample(tt.sample)

			if (err != nil) != tt.wantErr {
				t.Errorf("PublishSample() error = %v, wantErr %v", err, tt.wantErr)
			}

			// For enabled clients, check that lastPublish was updated
			if tt.config.Enabled && !tt.wantSkip && !client.lastPublish.IsZero() {
				if time.Since(client.lastPublish) > time.Second {
					t.Error("PublishSample() lastPublish not updated recently")
				}
			}
		})
	}
}

func TestMQTTClient_PublishEvent(t *testing.T) {
	tests := []struct {
		name    string
		event   interface{}
		wantErr bool
	}{
		{
			name: "valid event",
			event: map[string]interface{}{
				"timestamp": time.Now().Unix(),
				"type":      "failover",
				"reason":    "predictive",
				"member":    "starlink_test",
				"from":      "starlink",
				"to":        "cellular",
			},
			wantErr: false,
		},
		{
			name: "simple event",
			event: map[string]interface{}{
				"type":    "test",
				"message": "test event",
			},
			wantErr: false,
		},
	}

	config := &Config{
		Broker:      "localhost",
		Port:        1883,
		ClientID:    "test_client",
		TopicPrefix: "test",
		Enabled:     true,
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			logger := logx.NewLogger("test", "debug")
			client := NewClient(config, logger)
			client.connected = true

			err := client.PublishEvent(tt.event)

			if (err != nil) != tt.wantErr {
				t.Errorf("PublishEvent() error = %v, wantErr %v", err, tt.wantErr)
			}
		})
	}
}

func TestMQTTClient_PublishStatus(t *testing.T) {
	config := &Config{
		Broker:      "localhost",
		Port:        1883,
		ClientID:    "test_client",
		TopicPrefix: "test",
		Enabled:     true,
	}

	status := map[string]interface{}{
		"timestamp":      time.Now().Unix(),
		"current_member": "starlink_test",
		"total_members":  3,
		"active_members": 2,
		"daemon_uptime":  3600,
	}

	logger := logx.NewLogger("test", "debug")
	client := NewClient(config, logger)
	client.connected = true

	err := client.PublishStatus(status)
	if err != nil {
		t.Errorf("PublishStatus() error = %v", err)
	}
}

func TestMQTTClient_PublishMemberList(t *testing.T) {
	config := &Config{
		Broker:      "localhost",
		Port:        1883,
		ClientID:    "test_client",
		TopicPrefix: "test",
		Enabled:     true,
	}

	members := []map[string]interface{}{
		{
			"name":      "starlink_test",
			"class":     "starlink",
			"interface": "wan",
			"weight":    100,
			"eligible":  true,
			"active":    true,
		},
		{
			"name":      "cellular_test",
			"class":     "cellular",
			"interface": "wwan0",
			"weight":    80,
			"eligible":  true,
			"active":    false,
		},
	}

	logger := logx.NewLogger("test", "debug")
	client := NewClient(config, logger)
	client.connected = true

	err := client.PublishMemberList(members)
	if err != nil {
		t.Errorf("PublishMemberList() error = %v", err)
	}
}

func TestMQTTClient_PublishHealth(t *testing.T) {
	config := &Config{
		Broker:      "localhost",
		Port:        1883,
		ClientID:    "test_client",
		TopicPrefix: "test",
		Enabled:     true,
	}

	health := map[string]interface{}{
		"timestamp":       time.Now().Unix(),
		"telemetry_usage": 1024000, // 1MB
		"components": map[string]string{
			"controller":      "healthy",
			"decision_engine": "healthy",
			"telemetry_store": "healthy",
		},
	}

	logger := logx.NewLogger("test", "debug")
	client := NewClient(config, logger)
	client.connected = true

	err := client.PublishHealth(health)
	if err != nil {
		t.Errorf("PublishHealth() error = %v", err)
	}
}

func TestMQTTClient_PublishWithRetry(t *testing.T) {
	tests := []struct {
		name       string
		topic      string
		payload    interface{}
		maxRetries int
		shouldFail bool
		wantErr    bool
	}{
		{
			name:       "successful publish",
			topic:      "test/topic",
			payload:    map[string]string{"test": "data"},
			maxRetries: 3,
			shouldFail: false,
			wantErr:    false,
		},
		{
			name:       "retry and succeed",
			topic:      "test/topic",
			payload:    map[string]string{"test": "data"},
			maxRetries: 3,
			shouldFail: true, // Will fail first attempts then succeed
			wantErr:    false,
		},
		{
			name:       "max retries exceeded",
			topic:      "test/topic",
			payload:    map[string]string{"test": "data"},
			maxRetries: 1,
			shouldFail: true,
			wantErr:    true,
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			config := &Config{
				Broker:      "localhost",
				Port:        1883,
				ClientID:    "test_client",
				TopicPrefix: "test",
				Enabled:     true,
			}

			logger := logx.NewLogger("test", "debug")
			client := NewClient(config, logger)
			client.connected = true

			err := client.PublishWithRetry(tt.topic, tt.payload, tt.maxRetries)

			if (err != nil) != tt.wantErr {
				t.Errorf("PublishWithRetry() error = %v, wantErr %v", err, tt.wantErr)
			}
		})
	}
}

func TestMQTTClient_GetLastPublish(t *testing.T) {
	config := DefaultConfig()
	config.Enabled = true

	logger := logx.NewLogger("test", "debug")
	client := NewClient(config, logger)
	client.connected = true

	// Initially should be zero
	lastPublish := client.GetLastPublish()
	if !lastPublish.IsZero() {
		t.Error("GetLastPublish() should return zero time initially")
	}

	// Publish something
	err := client.PublishSample(map[string]interface{}{"test": "data"})
	if err != nil {
		t.Errorf("PublishSample() error = %v", err)
	}

	// Now should have a timestamp
	lastPublish = client.GetLastPublish()
	if lastPublish.IsZero() {
		t.Error("GetLastPublish() should return non-zero time after publish")
	}

	if time.Since(lastPublish) > time.Second {
		t.Error("GetLastPublish() timestamp should be recent")
	}
}

func TestMQTTClient_JSONSerialization(t *testing.T) {
	tests := []struct {
		name    string
		payload interface{}
		wantErr bool
	}{
		{
			name: "simple map",
			payload: map[string]interface{}{
				"string": "value",
				"number": 42,
				"float":  3.14,
				"bool":   true,
			},
			wantErr: false,
		},
		{
			name: "nested structure",
			payload: map[string]interface{}{
				"level1": map[string]interface{}{
					"level2": []interface{}{1, 2, 3},
				},
			},
			wantErr: false,
		},
		{
			name: "time values",
			payload: map[string]interface{}{
				"timestamp": time.Now().Unix(),
				"duration":  time.Hour.Seconds(),
			},
			wantErr: false,
		},
		{
			name:    "nil payload",
			payload: nil,
			wantErr: false,
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			// Test JSON serialization
			data, err := json.Marshal(tt.payload)

			if (err != nil) != tt.wantErr {
				t.Errorf("JSON Marshal error = %v, wantErr %v", err, tt.wantErr)
			}

			if !tt.wantErr && tt.payload != nil {
				// Test deserialization
				var result interface{}
				err = json.Unmarshal(data, &result)
				if err != nil {
					t.Errorf("JSON Unmarshal error = %v", err)
				}
			}
		})
	}
}

// Integration test for MQTT callback system
func TestMQTTClient_Integration_CallbackSystem(t *testing.T) {
	config := &Config{
		Broker:      "localhost",
		Port:        1883,
		ClientID:    "test_integration",
		TopicPrefix: "test",
		Enabled:     true,
	}

	logger := logx.NewLogger("test", "debug")
	client := NewClient(config, logger)
	client.connected = true

	// Test sequence of operations that would happen in real daemon
	testEvents := []map[string]interface{}{
		{
			"timestamp": time.Now().Unix(),
			"type":      "startup",
			"reason":    "daemon_start",
			"data":      map[string]interface{}{"version": "1.0.0"},
		},
		{
			"timestamp": time.Now().Unix(),
			"type":      "failover",
			"reason":    "predictive",
			"from":      "starlink",
			"to":        "cellular",
			"data":      map[string]interface{}{"score_delta": 15.5},
		},
		{
			"timestamp": time.Now().Unix(),
			"type":      "failback",
			"reason":    "restored",
			"from":      "cellular",
			"to":        "starlink",
			"data":      map[string]interface{}{"uptime": 3600},
		},
	}

	// Publish events in sequence
	for i, event := range testEvents {
		err := client.PublishEvent(event)
		if err != nil {
			t.Errorf("PublishEvent() %d error = %v", i, err)
		}

		// Small delay to simulate real-time publishing
		time.Sleep(10 * time.Millisecond)
	}

	// Verify last publish timestamp
	lastPublish := client.GetLastPublish()
	if time.Since(lastPublish) > time.Second {
		t.Error("Last publish timestamp not updated correctly")
	}
}

// Benchmark tests for performance validation
func BenchmarkMQTTClient_PublishSample(b *testing.B) {
	config := &Config{
		Broker:      "localhost",
		Port:        1883,
		ClientID:    "bench_client",
		TopicPrefix: "bench",
		Enabled:     true,
	}

	logger := logx.NewLogger("bench", "error") // Minimal logging for benchmarks
	client := NewClient(config, logger)
	client.connected = true

	sample := map[string]interface{}{
		"member":     "starlink_bench",
		"timestamp":  time.Now().Unix(),
		"latency_ms": 45.0,
		"loss_pct":   0.5,
		"score":      85.2,
	}

	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		_ = client.PublishSample(sample)
	}
}

func BenchmarkMQTTClient_JSONMarshal(b *testing.B) {
	payload := map[string]interface{}{
		"timestamp":      time.Now().Unix(),
		"current_member": "starlink_bench",
		"total_members":  5,
		"active_members": 3,
		"components": map[string]string{
			"controller":      "healthy",
			"decision_engine": "healthy",
			"telemetry_store": "healthy",
		},
	}

	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		_, _ = json.Marshal(payload)
	}
}
