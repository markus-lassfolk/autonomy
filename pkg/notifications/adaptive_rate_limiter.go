package notifications

import (
	"fmt"
	"sync"
	"time"

	"github.com/markus-lassfolk/autonomy/pkg/logx"
)

// AdaptiveRateLimiter provides intelligent rate limiting based on priority and system load
type AdaptiveRateLimiter struct {
	config *SmartManagerConfig
	logger *logx.Logger

	// Rate limiting buckets by priority
	buckets map[int]*TokenBucket

	// Adaptive adjustment tracking
	mu                 sync.RWMutex
	systemLoad         float64
	recentFailures     int
	lastAdjustment     time.Time
	adaptiveMultiplier float64

	// Statistics
	allowedCount map[int]int64
	deniedCount  map[int]int64
	lastReset    time.Time
}

// TokenBucket implements a token bucket rate limiter
type TokenBucket struct {
	mu         sync.Mutex
	capacity   int
	tokens     int
	refillRate int // tokens per hour
	lastRefill time.Time
	cooldown   time.Duration
	lastUsed   time.Time
}

// NewAdaptiveRateLimiter creates a new adaptive rate limiter
func NewAdaptiveRateLimiter(config *SmartManagerConfig, logger *logx.Logger) *AdaptiveRateLimiter {
	arl := &AdaptiveRateLimiter{
		config:             config,
		logger:             logger,
		buckets:            make(map[int]*TokenBucket),
		allowedCount:       make(map[int]int64),
		deniedCount:        make(map[int]int64),
		lastReset:          time.Now(),
		adaptiveMultiplier: 1.0,
		systemLoad:         0.0,
	}

	// Initialize buckets for each priority level
	arl.buckets[PriorityEmergency] = NewTokenBucket(config.EmergencyRateLimit, config.EmergencyRateLimit, config.EmergencyCooldown)
	arl.buckets[PriorityHigh] = NewTokenBucket(config.HighRateLimit, config.HighRateLimit, config.HighCooldown)
	arl.buckets[PriorityNormal] = NewTokenBucket(config.NormalRateLimit, config.NormalRateLimit, config.NormalCooldown)
	arl.buckets[PriorityLow] = NewTokenBucket(config.LowRateLimit, config.LowRateLimit, config.LowCooldown)
	arl.buckets[PriorityLowest] = NewTokenBucket(config.LowRateLimit/2, config.LowRateLimit/2, config.LowCooldown*2)

	// Start background adaptive adjustment
	if config.AdaptiveRateLimiting {
		go arl.adaptiveAdjustmentLoop()
	}

	return arl
}

// NewTokenBucket creates a new token bucket
func NewTokenBucket(capacity, refillRate int, cooldown time.Duration) *TokenBucket {
	return &TokenBucket{
		capacity:   capacity,
		tokens:     capacity,
		refillRate: refillRate,
		lastRefill: time.Now(),
		cooldown:   cooldown,
		lastUsed:   time.Time{},
	}
}

// Allow checks if a notification with given priority should be allowed
func (arl *AdaptiveRateLimiter) Allow(priority int) bool {
	bucket, exists := arl.buckets[priority]
	if !exists {
		// Default to normal priority bucket for unknown priorities
		bucket = arl.buckets[PriorityNormal]
	}

	// Check cooldown period
	if !bucket.lastUsed.IsZero() && time.Since(bucket.lastUsed) < bucket.cooldown {
		arl.incrementDenied(priority)
		arl.logger.Debug("Rate limited by cooldown",
			"priority", priority,
			"cooldown", bucket.cooldown,
			"time_since_last", time.Since(bucket.lastUsed))
		return false
	}

	// Check token bucket
	if bucket.TakeToken() {
		arl.incrementAllowed(priority)
		return true
	}

	arl.incrementDenied(priority)
	arl.logger.Debug("Rate limited by token bucket",
		"priority", priority,
		"tokens", bucket.tokens,
		"capacity", bucket.capacity)
	return false
}

// TakeToken attempts to take a token from the bucket
func (tb *TokenBucket) TakeToken() bool {
	tb.mu.Lock()
	defer tb.mu.Unlock()

	// Refill tokens based on time elapsed
	tb.refill()

	if tb.tokens > 0 {
		tb.tokens--
		tb.lastUsed = time.Now()
		return true
	}

	return false
}

// refill adds tokens to the bucket based on elapsed time
func (tb *TokenBucket) refill() {
	now := time.Now()
	elapsed := now.Sub(tb.lastRefill)

	if elapsed < time.Minute {
		return // Don't refill too frequently
	}

	// Calculate tokens to add (refillRate is per hour)
	tokensToAdd := int(elapsed.Hours() * float64(tb.refillRate))
	if tokensToAdd > 0 {
		tb.tokens += tokensToAdd
		if tb.tokens > tb.capacity {
			tb.tokens = tb.capacity
		}
		tb.lastRefill = now
	}
}

// GetTokens returns current token count (for monitoring)
func (tb *TokenBucket) GetTokens() int {
	tb.mu.Lock()
	defer tb.mu.Unlock()
	tb.refill()
	return tb.tokens
}

// incrementAllowed increments the allowed counter for a priority
func (arl *AdaptiveRateLimiter) incrementAllowed(priority int) {
	arl.mu.Lock()
	defer arl.mu.Unlock()
	arl.allowedCount[priority]++
}

// incrementDenied increments the denied counter for a priority
func (arl *AdaptiveRateLimiter) incrementDenied(priority int) {
	arl.mu.Lock()
	defer arl.mu.Unlock()
	arl.deniedCount[priority]++
}

// adaptiveAdjustmentLoop runs the adaptive adjustment algorithm
func (arl *AdaptiveRateLimiter) adaptiveAdjustmentLoop() {
	ticker := time.NewTicker(5 * time.Minute)
	defer ticker.Stop()

	for {
		select {
		case <-ticker.C:
			arl.performAdaptiveAdjustment()
		}
	}
}

// performAdaptiveAdjustment adjusts rate limits based on system conditions
func (arl *AdaptiveRateLimiter) performAdaptiveAdjustment() {
	arl.mu.Lock()
	defer arl.mu.Unlock()

	now := time.Now()
	timeSinceLastAdjustment := now.Sub(arl.lastAdjustment)

	// Don't adjust too frequently
	if timeSinceLastAdjustment < 5*time.Minute {
		return
	}

	// Calculate system metrics
	totalAllowed := int64(0)
	totalDenied := int64(0)

	for _, count := range arl.allowedCount {
		totalAllowed += count
	}
	for _, count := range arl.deniedCount {
		totalDenied += count
	}

	// Calculate denial rate
	denialRate := float64(0)
	if totalAllowed+totalDenied > 0 {
		denialRate = float64(totalDenied) / float64(totalAllowed+totalDenied)
	}

	// Determine adjustment needed
	var adjustment float64

	if denialRate > 0.3 { // High denial rate - increase limits
		adjustment = 1.2
		arl.logger.Info("Increasing rate limits due to high denial rate",
			"denial_rate", denialRate,
			"adjustment", adjustment)
	} else if denialRate < 0.05 { // Very low denial rate - decrease limits to save resources
		adjustment = 0.9
		arl.logger.Info("Decreasing rate limits due to low denial rate",
			"denial_rate", denialRate,
			"adjustment", adjustment)
	} else {
		// No adjustment needed
		arl.lastAdjustment = now
		return
	}

	// Apply adjustment to all buckets except emergency
	for priority, bucket := range arl.buckets {
		if priority == PriorityEmergency {
			continue // Never limit emergency notifications
		}

		bucket.mu.Lock()
		newCapacity := int(float64(bucket.capacity) * adjustment)
		newRefillRate := int(float64(bucket.refillRate) * adjustment)

		// Apply reasonable bounds
		if newCapacity < 1 {
			newCapacity = 1
		}
		if newCapacity > arl.config.MaxNotificationsPerHour {
			newCapacity = arl.config.MaxNotificationsPerHour
		}
		if newRefillRate < 1 {
			newRefillRate = 1
		}

		bucket.capacity = newCapacity
		bucket.refillRate = newRefillRate

		// Adjust current tokens proportionally
		if bucket.tokens > newCapacity {
			bucket.tokens = newCapacity
		}

		bucket.mu.Unlock()

		arl.logger.Debug("Adjusted rate limit for priority",
			"priority", priority,
			"new_capacity", newCapacity,
			"new_refill_rate", newRefillRate)
	}

	arl.adaptiveMultiplier *= adjustment
	arl.lastAdjustment = now

	// Reset counters for next period
	arl.allowedCount = make(map[int]int64)
	arl.deniedCount = make(map[int]int64)
}

// GetStats returns rate limiter statistics
func (arl *AdaptiveRateLimiter) GetStats() map[string]interface{} {
	arl.mu.RLock()
	defer arl.mu.RUnlock()

	stats := map[string]interface{}{
		"adaptive_multiplier": arl.adaptiveMultiplier,
		"system_load":         arl.systemLoad,
		"recent_failures":     arl.recentFailures,
		"last_adjustment":     arl.lastAdjustment,
		"buckets":             make(map[string]interface{}),
		"allowed_count":       make(map[string]int64),
		"denied_count":        make(map[string]int64),
	}

	// Add bucket stats
	bucketStats := make(map[string]interface{})
	for priority, bucket := range arl.buckets {
		bucketStats[fmt.Sprintf("priority_%d", priority)] = map[string]interface{}{
			"capacity":    bucket.capacity,
			"tokens":      bucket.GetTokens(),
			"refill_rate": bucket.refillRate,
			"cooldown":    bucket.cooldown.String(),
			"last_used":   bucket.lastUsed,
		}
	}
	stats["buckets"] = bucketStats

	// Add counters
	allowedStats := make(map[string]int64)
	deniedStats := make(map[string]int64)

	for priority, count := range arl.allowedCount {
		allowedStats[fmt.Sprintf("priority_%d", priority)] = count
	}
	for priority, count := range arl.deniedCount {
		deniedStats[fmt.Sprintf("priority_%d", priority)] = count
	}

	stats["allowed_count"] = allowedStats
	stats["denied_count"] = deniedStats

	return stats
}

// UpdateSystemLoad updates the system load metric for adaptive adjustment
func (arl *AdaptiveRateLimiter) UpdateSystemLoad(load float64) {
	arl.mu.Lock()
	defer arl.mu.Unlock()
	arl.systemLoad = load
}

// ReportFailure reports a notification failure for adaptive adjustment
func (arl *AdaptiveRateLimiter) ReportFailure() {
	arl.mu.Lock()
	defer arl.mu.Unlock()
	arl.recentFailures++
}

// ResetFailures resets the failure counter
func (arl *AdaptiveRateLimiter) ResetFailures() {
	arl.mu.Lock()
	defer arl.mu.Unlock()
	arl.recentFailures = 0
}
