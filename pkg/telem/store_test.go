package telem

import (
	"testing"
	"time"

	"github.com/markus-lassfolk/autonomy/pkg"
)

func TestNewStore(t *testing.T) {
	// Test valid parameters
	store, err := NewStore(24, 50)
	if err != nil {
		t.Fatalf("NewStore should not error with valid parameters: %v", err)
	}
	if store == nil {
		t.Fatal("Store should not be nil")
	}

	// Test invalid retention hours
	_, err = NewStore(-1, 50)
	if err == nil {
		t.Error("NewStore should error with negative retention hours")
	}

	// Test invalid max RAM
	_, err = NewStore(24, -1)
	if err == nil {
		t.Error("NewStore should error with negative max RAM")
	}
}

func TestStoreAddSample(t *testing.T) {
	store, err := NewStore(24, 50)
	if err != nil {
		t.Fatalf("Failed to create store: %v", err)
	}

	memberName := "test-member"
	obstruction := 5.0
	outages := 0
	latency := 50.0
	loss := 0.1
	jitter := 5.0
	metrics := &pkg.Metrics{
		Timestamp:      time.Now(),
		LatencyMS:      &latency,
		LossPercent:    &loss,
		JitterMS:       &jitter,
		ObstructionPct: &obstruction,
		Outages:        &outages,
	}
	score := &pkg.Score{
		Instant:   85.0,
		EWMA:      85.0,
		Final:     85.0,
		UpdatedAt: time.Now(),
	}

	// Add sample
	err = store.AddSample(memberName, metrics, score)
	if err != nil {
		t.Fatalf("AddSample should not error: %v", err)
	}

	// Verify sample was added
	samples, err := store.GetSamples(memberName, time.Now().Add(-time.Hour))
	if err != nil {
		t.Fatalf("GetSamples should not error: %v", err)
	}
	if len(samples) != 1 {
		t.Errorf("Should have one sample, got %d", len(samples))
	}
	if samples[0].Member != memberName {
		t.Errorf("Sample member name should match, got %s", samples[0].Member)
	}
}

func TestStoreAddEvent(t *testing.T) {
	store, err := NewStore(24, 50)
	if err != nil {
		t.Fatalf("Failed to create store: %v", err)
	}

	event := &pkg.Event{
		Type:      "switch",
		Timestamp: time.Now(),
		Data:      map[string]interface{}{"test": "data"},
	}

	// Add event
	err = store.AddEvent(event)
	if err != nil {
		t.Fatalf("AddEvent should not error: %v", err)
	}

	// Verify event was added
	events, err := store.GetEvents(time.Now().Add(-time.Hour), 10)
	if err != nil {
		t.Fatalf("GetEvents should not error: %v", err)
	}
	if len(events) != 1 {
		t.Errorf("Should have one event, got %d", len(events))
	}
	if events[0].Type != event.Type {
		t.Errorf("Event type should match, got %v", events[0].Type)
	}
}

func TestStoreRetentionPolicy(t *testing.T) {
	// Create store with short retention
	store, err := NewStore(1, 50) // 1 hour retention
	if err != nil {
		t.Fatalf("Failed to create store: %v", err)
	}

	memberName := "test-member"
	obstruction := 5.0
	outages := 0
	latency := 50.0
	loss := 0.1
	jitter := 5.0
	metrics := &pkg.Metrics{
		Timestamp:      time.Now(),
		LatencyMS:      &latency,
		LossPercent:    &loss,
		JitterMS:       &jitter,
		ObstructionPct: &obstruction,
		Outages:        &outages,
	}
	score := &pkg.Score{
		Instant:   85.0,
		EWMA:      85.0,
		Final:     85.0,
		UpdatedAt: time.Now(),
	}

	// Add sample
	err = store.AddSample(memberName, metrics, score)
	if err != nil {
		t.Fatalf("AddSample should not error: %v", err)
	}

	// Verify sample exists
	samples, err := store.GetSamples(memberName, time.Now().Add(-time.Hour))
	if err != nil {
		t.Fatalf("GetSamples should not error: %v", err)
	}
	if len(samples) != 1 {
		t.Errorf("Should have one sample, got %d", len(samples))
	}

	// Add multiple samples
	for i := 0; i < 5; i++ {
		metrics.Timestamp = time.Now()
		err = store.AddSample(memberName, metrics, score)
		if err != nil {
			t.Fatalf("AddSample should not error: %v", err)
		}
	}

	// Verify we can retrieve samples
	samples, err = store.GetSamples(memberName, time.Now().Add(-time.Hour))
	if err != nil {
		t.Fatalf("GetSamples should not error: %v", err)
	}
	if len(samples) == 0 {
		t.Error("Should have samples")
	}
}

func TestStoreMemoryLimit(t *testing.T) {
	// Create store with small memory limit
	store, err := NewStore(24, 1) // 1MB limit
	if err != nil {
		t.Fatalf("Failed to create store: %v", err)
	}

	memberName := "test-member"
	obstruction := 5.0
	outages := 0
	latency := 50.0
	loss := 0.1
	jitter := 5.0
	metrics := &pkg.Metrics{
		Timestamp:      time.Now(),
		LatencyMS:      &latency,
		LossPercent:    &loss,
		JitterMS:       &jitter,
		ObstructionPct: &obstruction,
		Outages:        &outages,
	}
	score := &pkg.Score{
		Instant:   85.0,
		EWMA:      85.0,
		Final:     85.0,
		UpdatedAt: time.Now(),
	}

	// Add many samples to trigger memory pressure
	for i := 0; i < 100; i++ {
		metrics.Timestamp = time.Now()
		err = store.AddSample(memberName, metrics, score)
		if err != nil {
			t.Fatalf("AddSample should not error: %v", err)
		}
	}

	// Verify store is still functional
	samples, err := store.GetSamples(memberName, time.Now().Add(-time.Hour))
	if err != nil {
		t.Fatalf("GetSamples should not error: %v", err)
	}
	if len(samples) == 0 {
		t.Error("Should have samples even with memory pressure")
	}
}

func TestStoreGetMembers(t *testing.T) {
	store, err := NewStore(24, 50)
	if err != nil {
		t.Fatalf("Failed to create store: %v", err)
	}

	// Add samples for multiple members
	members := []string{"member1", "member2", "member3"}
	obstruction := 5.0
	outages := 0
	latency := 50.0
	loss := 0.1
	jitter := 5.0
	metrics := &pkg.Metrics{
		Timestamp:      time.Now(),
		LatencyMS:      &latency,
		LossPercent:    &loss,
		JitterMS:       &jitter,
		ObstructionPct: &obstruction,
		Outages:        &outages,
	}
	score := &pkg.Score{
		Instant:   85.0,
		EWMA:      85.0,
		Final:     85.0,
		UpdatedAt: time.Now(),
	}

	for _, member := range members {
		err = store.AddSample(member, metrics, score)
		if err != nil {
			t.Fatalf("AddSample should not error: %v", err)
		}
	}

	// Get all members
	retrievedMembers := store.GetMembers()
	if len(retrievedMembers) != len(members) {
		t.Errorf("Should have %d members, got %d", len(members), len(retrievedMembers))
	}

	// Verify all expected members are present
	for _, expectedMember := range members {
		found := false
		for _, retrievedMember := range retrievedMembers {
			if retrievedMember == expectedMember {
				found = true
				break
			}
		}
		if !found {
			t.Errorf("Member %s not found in retrieved members", expectedMember)
		}
	}
}

func TestStoreCleanup(t *testing.T) {
	// Create store with very short retention
	store, err := NewStore(1, 50) // 1 hour retention
	if err != nil {
		t.Fatalf("Failed to create store: %v", err)
	}

	memberName := "test-member"
	obstruction := 5.0
	outages := 0
	latency := 50.0
	loss := 0.1
	jitter := 5.0
	metrics := &pkg.Metrics{
		Timestamp:      time.Now(),
		LatencyMS:      &latency,
		LossPercent:    &loss,
		JitterMS:       &jitter,
		ObstructionPct: &obstruction,
		Outages:        &outages,
	}
	score := &pkg.Score{
		Instant:   85.0,
		EWMA:      85.0,
		Final:     85.0,
		UpdatedAt: time.Now(),
	}

	// Add sample
	err = store.AddSample(memberName, metrics, score)
	if err != nil {
		t.Fatalf("AddSample should not error: %v", err)
	}

	// Verify sample exists
	samples, err := store.GetSamples(memberName, time.Now().Add(-time.Hour))
	if err != nil {
		t.Fatalf("GetSamples should not error: %v", err)
	}
	if len(samples) != 1 {
		t.Errorf("Should have one sample, got %d", len(samples))
	}

	// Run cleanup
	store.Cleanup()

	// Verify sample still exists (not old enough to be cleaned up)
	samples, err = store.GetSamples(memberName, time.Now().Add(-time.Hour))
	if err != nil {
		t.Fatalf("GetSamples should not error: %v", err)
	}
	if len(samples) != 1 {
		t.Errorf("Should still have one sample after cleanup, got %d", len(samples))
	}
}
