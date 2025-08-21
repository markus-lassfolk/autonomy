package collector

import (
	"context"
	"testing"
	"time"
)

func TestStarlinkCollector_New(t *testing.T) {
	config := map[string]interface{}{
		"starlink_host": "192.168.100.1",
		"starlink_port": 9200,
		"timeout":       10,
	}

	sc, err := NewStarlinkCollector(config)
	if err != nil {
		t.Fatalf("NewStarlinkCollector() error = %v", err)
	}

	if sc == nil {
		t.Fatal("NewStarlinkCollector() returned nil")
	}

	if sc.apiHost != "192.168.100.1" {
		t.Errorf("Expected host 192.168.100.1, got %s", sc.apiHost)
	}

	if sc.apiPort != 9200 {
		t.Errorf("Expected port 9200, got %d", sc.apiPort)
	}

	if sc.timeout != 10*time.Second {
		t.Errorf("Expected timeout 10s, got %v", sc.timeout)
	}

	if sc.starlinkClient == nil {
		t.Error("Expected starlinkClient to be initialized")
	}
}

func TestStarlinkCollector_DefaultConfig(t *testing.T) {
	config := map[string]interface{}{}

	sc, err := NewStarlinkCollector(config)
	if err != nil {
		t.Fatalf("NewStarlinkCollector() error = %v", err)
	}

	if sc.apiHost != "192.168.100.1" {
		t.Errorf("Expected default host 192.168.100.1, got %s", sc.apiHost)
	}

	if sc.apiPort != 9200 {
		t.Errorf("Expected default port 9200, got %d", sc.apiPort)
	}

	if sc.timeout != 30*time.Second {
		t.Errorf("Expected default timeout 30s, got %v", sc.timeout)
	}
}

func TestStarlinkCollector_TryStarlinkGRPC(t *testing.T) {
	config := map[string]interface{}{}
	sc, err := NewStarlinkCollector(config)
	if err != nil {
		t.Fatalf("NewStarlinkCollector() error = %v", err)
	}

	ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancel()

	// This test will only pass if there's an actual Starlink dish available
	response, err := sc.tryStarlinkGRPC(ctx)
	if err != nil {
		t.Logf("tryStarlinkGRPC() failed (expected if no Starlink dish): %v", err)
		return
	}

	if response == nil {
		t.Error("tryStarlinkGRPC() returned nil response")
		return
	}

	// Validate response structure
	if response.Status.DeviceInfo.ID == "" {
		t.Error("Expected device ID in response")
	}

	if response.Status.PopPingLatencyMs < 0 {
		t.Error("Expected non-negative latency")
	}
}

func TestStarlinkCollector_TestStarlinkMethod(t *testing.T) {
	config := map[string]interface{}{}
	sc, err := NewStarlinkCollector(config)
	if err != nil {
		t.Fatalf("NewStarlinkCollector() error = %v", err)
	}

	ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancel()

	// Test get_status method
	response, err := sc.TestStarlinkMethod(ctx, "get_status")
	if err != nil {
		t.Logf("TestStarlinkMethod(get_status) failed (expected if no Starlink dish): %v", err)
		return
	}

	if response == "" {
		t.Error("TestStarlinkMethod() returned empty response")
	}
}
