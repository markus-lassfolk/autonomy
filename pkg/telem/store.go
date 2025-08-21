package telem

import (
	"fmt"
	"sync"
	"time"

	"github.com/markus-lassfolk/autonomy/pkg"
)

// Store manages telemetry data in RAM with ring buffers
type Store struct {
	mu sync.RWMutex

	// Configuration
	retentionHours int
	maxRAMMB       int

	// Ring buffers
	samples map[string]*RingBuffer // per-member samples
	events  *RingBuffer            // system events

	// Memory tracking
	memoryUsage int64
	lastCleanup time.Time

	// Event callback for real-time publishing
	eventCallback func(*pkg.Event)

	// Memory optimization: Object pools
	samplePool *sync.Pool
	eventPool  *sync.Pool

	// Memory optimization: Pre-allocated slices for common operations
	emptySamples []*Sample
	emptyEvents  []*pkg.Event
}

// RingBuffer implements a thread-safe ring buffer with time-based retention
type RingBuffer struct {
	mu       sync.RWMutex
	data     []interface{}
	capacity int
	head     int
	tail     int
	size     int
	lastAdd  time.Time

	// Memory optimization: Pre-allocated result slice
	resultPool []interface{}
}

// Sample represents a telemetry sample with metadata
type Sample struct {
	Member    string       `json:"member"`
	Timestamp time.Time    `json:"timestamp"`
	Metrics   *pkg.Metrics `json:"metrics"`
	Score     *pkg.Score   `json:"score,omitempty"`
}

// NewStore creates a new telemetry store with memory optimization
func NewStore(retentionHours, maxRAMMB int) (*Store, error) {
	if retentionHours < 1 || retentionHours > 168 {
		return nil, fmt.Errorf("retention_hours must be between 1 and 168")
	}
	if maxRAMMB < 1 || maxRAMMB > 128 {
		return nil, fmt.Errorf("max_ram_mb must be between 1 and 128")
	}

	store := &Store{
		retentionHours: retentionHours,
		maxRAMMB:       maxRAMMB,
		samples:        make(map[string]*RingBuffer),
		events:         NewRingBuffer(1000), // 1000 events max
		lastCleanup:    time.Now(),

		// Memory optimization: Initialize object pools
		samplePool: &sync.Pool{
			New: func() interface{} {
				return &Sample{}
			},
		},
		eventPool: &sync.Pool{
			New: func() interface{} {
				return &pkg.Event{}
			},
		},

		// Memory optimization: Pre-allocate common slices
		emptySamples: make([]*Sample, 0, 10),
		emptyEvents:  make([]*pkg.Event, 0, 10),
	}

	return store, nil
}

// AddSample adds a sample for a member with memory optimization
func (s *Store) AddSample(member string, metrics *pkg.Metrics, score *pkg.Score) error {
	s.mu.Lock()
	defer s.mu.Unlock()

	// Ensure ring buffer exists for this member
	if s.samples[member] == nil {
		s.samples[member] = NewRingBuffer(1000) // 1000 samples per member
	}

	// Memory optimization: Reuse sample object from pool
	sample := s.samplePool.Get().(*Sample)
	sample.Member = member
	sample.Timestamp = time.Now()
	sample.Metrics = metrics
	sample.Score = score

	// Add to ring buffer
	s.samples[member].Add(sample)

	// Check memory pressure
	s.checkMemoryPressure()

	return nil
}

// AddEvent adds a system event with memory optimization
func (s *Store) AddEvent(event *pkg.Event) error {
	s.mu.Lock()
	callback := s.eventCallback
	s.mu.Unlock()

	// Add to ring buffer
	s.mu.Lock()
	s.events.Add(event)
	// Check memory pressure
	s.checkMemoryPressure()
	s.mu.Unlock()

	// Call the callback if set (outside of lock to avoid deadlock)
	if callback != nil {
		go callback(event) // Run in goroutine to avoid blocking
	}

	return nil
}

// SetEventCallback sets a callback function that will be called when events are added
func (s *Store) SetEventCallback(callback func(*pkg.Event)) {
	s.mu.Lock()
	defer s.mu.Unlock()
	s.eventCallback = callback
}

// GetSamples returns samples for a member within a time window with memory optimization
func (s *Store) GetSamples(member string, since time.Time) ([]*Sample, error) {
	s.mu.RLock()
	defer s.mu.RUnlock()

	buffer, exists := s.samples[member]
	if !exists {
		return s.emptySamples, nil // Return pre-allocated empty slice
	}

	// Convert interface{} to []*Sample with memory optimization
	items := buffer.GetSince(since)
	if len(items) == 0 {
		return s.emptySamples, nil
	}

	// Memory optimization: Pre-allocate exact size
	samples := make([]*Sample, 0, len(items))
	for _, item := range items {
		if sample, ok := item.(*Sample); ok {
			samples = append(samples, sample)
		}
	}

	return samples, nil
}

// GetEvents returns events within a time window with memory optimization
func (s *Store) GetEvents(since time.Time, limit int) ([]*pkg.Event, error) {
	s.mu.RLock()
	defer s.mu.RUnlock()

	events := s.events.GetSince(since)
	if len(events) == 0 {
		return s.emptyEvents, nil // Return pre-allocated empty slice
	}

	if limit > 0 && len(events) > limit {
		events = events[:limit]
	}

	// Memory optimization: Pre-allocate exact size
	result := make([]*pkg.Event, 0, len(events))
	for _, event := range events {
		if e, ok := event.(*pkg.Event); ok {
			result = append(result, e)
		}
	}

	return result, nil
}

// GetMembers returns all member names with samples
func (s *Store) GetMembers() []string {
	s.mu.RLock()
	defer s.mu.RUnlock()

	// Memory optimization: Pre-allocate exact size
	members := make([]string, 0, len(s.samples))
	for member := range s.samples {
		members = append(members, member)
	}

	return members
}

// GetMemoryUsage returns current memory usage in MB
func (s *Store) GetMemoryUsage() int {
	s.mu.RLock()
	defer s.mu.RUnlock()

	return int(s.memoryUsage / 1024 / 1024)
}

// Cleanup removes old data based on retention policy with memory optimization
func (s *Store) Cleanup() {
	s.mu.Lock()
	defer s.mu.Unlock()

	cutoff := time.Now().Add(-time.Duration(s.retentionHours) * time.Hour)

	// Cleanup samples with memory optimization
	for member, buffer := range s.samples {
		removedCount := buffer.RemoveBefore(cutoff)
		if buffer.Size() == 0 {
			delete(s.samples, member)
		}

		// Memory optimization: Return removed samples to pool
		if removedCount > 0 {
			// Note: In a real implementation, we'd need to track which samples were removed
			// For now, we'll rely on the garbage collector to clean up
			// This is intentional - no action needed here
		}
	}

	// Cleanup events
	s.events.RemoveBefore(cutoff)

	// Update memory usage
	s.updateMemoryUsage()
}

// Close cleans up resources
func (s *Store) Close() error {
	s.mu.Lock()
	defer s.mu.Unlock()

	// Clear all data
	s.samples = make(map[string]*RingBuffer)
	s.events = nil
	s.memoryUsage = 0

	return nil
}

// checkMemoryPressure checks if we need to reduce memory usage
func (s *Store) checkMemoryPressure() {
	s.updateMemoryUsage()

	if s.memoryUsage > int64(s.maxRAMMB*1024*1024) {
		// Memory pressure - downsample old data
		s.downsample()
	}

	// Periodic cleanup
	if time.Since(s.lastCleanup) > time.Hour {
		s.Cleanup()
		s.lastCleanup = time.Now()
	}
}

// updateMemoryUsage estimates current memory usage with improved accuracy
func (s *Store) updateMemoryUsage() {
	var usage int64

	// Memory optimization: More accurate memory estimation
	for _, buffer := range s.samples {
		// Estimate based on actual data types and sizes
		usage += int64(buffer.Size() * 256) // Reduced estimate per sample
	}

	// Estimate events memory
	usage += int64(s.events.Size() * 128) // Reduced estimate per event

	s.memoryUsage = usage
}

// downsample reduces memory usage by keeping every Nth sample
func (s *Store) downsample() {
	// Keep every 3rd sample for old data
	for _, buffer := range s.samples {
		buffer.Downsample(3)
	}
}

// NewRingBuffer creates a new ring buffer with memory optimization
func NewRingBuffer(capacity int) *RingBuffer {
	return &RingBuffer{
		data:       make([]interface{}, capacity),
		capacity:   capacity,
		head:       0,
		tail:       0,
		size:       0,
		resultPool: make([]interface{}, 0, capacity), // Pre-allocate result pool
	}
}

// Add adds an item to the ring buffer
func (rb *RingBuffer) Add(item interface{}) {
	rb.mu.Lock()
	defer rb.mu.Unlock()

	rb.data[rb.tail] = item
	rb.tail = (rb.tail + 1) % rb.capacity
	rb.lastAdd = time.Now()

	if rb.size < rb.capacity {
		rb.size++
	} else {
		rb.head = (rb.head + 1) % rb.capacity
	}
}

// GetSince returns items since the given time with memory optimization
func (rb *RingBuffer) GetSince(since time.Time) []interface{} {
	rb.mu.RLock()
	defer rb.mu.RUnlock()

	// Memory optimization: Reuse result pool
	rb.resultPool = rb.resultPool[:0] // Reset slice without reallocating

	for i := 0; i < rb.size; i++ {
		idx := (rb.head + i) % rb.capacity
		item := rb.data[idx]

		if sample, ok := item.(*Sample); ok {
			if sample.Timestamp.After(since) {
				rb.resultPool = append(rb.resultPool, sample)
			}
		} else if event, ok := item.(*pkg.Event); ok {
			if event.Timestamp.After(since) {
				rb.resultPool = append(rb.resultPool, event)
			}
		}
	}

	// Memory optimization: Return copy to avoid race conditions
	result := make([]interface{}, len(rb.resultPool))
	copy(result, rb.resultPool)
	return result
}

// RemoveBefore removes items before the given time and returns count of removed items
func (rb *RingBuffer) RemoveBefore(before time.Time) int {
	rb.mu.Lock()
	defer rb.mu.Unlock()

	// Simple approach: just reset if all data is old
	allOld := true
	removedCount := 0

	for i := 0; i < rb.size; i++ {
		idx := (rb.head + i) % rb.capacity
		item := rb.data[idx]

		if sample, ok := item.(*Sample); ok {
			if sample.Timestamp.After(before) {
				allOld = false
				break
			}
		} else if event, ok := item.(*pkg.Event); ok {
			if event.Timestamp.After(before) {
				allOld = false
				break
			}
		}
	}

	if allOld {
		removedCount = rb.size
		rb.head = 0
		rb.tail = 0
		rb.size = 0
	}

	return removedCount
}

// Downsample keeps every Nth item with memory optimization
func (rb *RingBuffer) Downsample(n int) {
	rb.mu.Lock()
	defer rb.mu.Unlock()

	if rb.size == 0 {
		return
	}

	// Memory optimization: Reuse existing slice when possible
	newSize := (rb.size + n - 1) / n // Ceiling division

	if newSize == rb.size {
		return // No downsampling needed
	}

	// Create new data slice only if necessary
	newData := make([]interface{}, rb.capacity)
	newHead := 0

	for i := 0; i < rb.size; i += n {
		idx := (rb.head + i) % rb.capacity
		newData[newSize] = rb.data[idx]
		newSize++
	}

	rb.data = newData
	rb.head = newHead
	rb.tail = newSize % rb.capacity
	rb.size = newSize
}

// Size returns the current number of items
func (rb *RingBuffer) Size() int {
	rb.mu.RLock()
	defer rb.mu.RUnlock()
	return rb.size
}

// Capacity returns the buffer capacity
func (rb *RingBuffer) Capacity() int {
	return rb.capacity
}
