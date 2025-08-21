package collector

import (
	"math"
	"testing"
)

func TestCalculateJitter(t *testing.T) {
	bc := NewBaseCollector(0, nil, nil)
	member := "test-member"

	if j := bc.calculateJitter(member, 100); j != 0 {
		t.Fatalf("expected jitter 0 with single sample, got %f", j)
	}

	if j := bc.calculateJitter(member, 110); math.Abs(j-5) > 0.0001 {
		t.Fatalf("expected jitter ~5, got %f", j)
	}

	if j := bc.calculateJitter(member, 90); math.Abs(j-math.Sqrt(200.0/3.0)) > 0.0001 {
		t.Fatalf("expected jitter %.4f, got %f", math.Sqrt(200.0/3.0), j)
	}
}

func TestJitterHistoryLimit(t *testing.T) {
	bc := NewBaseCollector(0, nil, nil)
	bc.historySize = 2
	member := "test-member"

	bc.calculateJitter(member, 100)
	bc.calculateJitter(member, 110)
	if j := bc.calculateJitter(member, 150); math.Abs(j-20) > 0.0001 {
		t.Fatalf("expected jitter 20 with limited history, got %f", j)
	}

	if len(bc.latencyHistory[member]) != 2 {
		t.Fatalf("expected history size 2, got %d", len(bc.latencyHistory[member]))
	}
}
