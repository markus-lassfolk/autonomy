package gps

import (
	"crypto/rand"
	"encoding/binary"
	"sync"
	"time"

	"github.com/markus-lassfolk/autonomy/pkg/logx"
)

// EnhancedNegativeCache implements PM feedback #4: jittered TTL to avoid synchronized queries
type EnhancedNegativeCache struct {
	entries   map[string]*NegativeCacheEntry
	baseTTL   time.Duration // Base TTL (e.g., 12 hours)
	jitterMin time.Duration // Minimum jitter (e.g., 10 hours)
	jitterMax time.Duration // Maximum jitter (e.g., 14 hours)
	logger    *logx.Logger
	mu        sync.RWMutex
}

// NegativeCacheEntry represents a negative cache entry with jittered expiry
type NegativeCacheEntry struct {
	CellKey      string    `json:"cell_key"`
	CachedAt     time.Time `json:"cached_at"`
	ExpiresAt    time.Time `json:"expires_at"`    // Jittered expiry time
	Reason       string    `json:"reason"`        // Why it was cached (404, timeout, etc.)
	AttemptCount int       `json:"attempt_count"` // Number of failed attempts
}

// NegativeCacheConfig defines negative cache behavior
type NegativeCacheConfig struct {
	BaseTTLHours   int `json:"base_ttl_hours"`   // Base TTL in hours (e.g., 12)
	JitterMinHours int `json:"jitter_min_hours"` // Minimum TTL with jitter (e.g., 10)
	JitterMaxHours int `json:"jitter_max_hours"` // Maximum TTL with jitter (e.g., 14)
}

// DefaultNegativeCacheConfig returns safe defaults with jitter (PM #4)
func DefaultNegativeCacheConfig() *NegativeCacheConfig {
	return &NegativeCacheConfig{
		BaseTTLHours:   12, // 12 hour base
		JitterMinHours: 10, // 10-14 hour range (PM #4)
		JitterMaxHours: 14,
	}
}

// NewEnhancedNegativeCache creates a new negative cache with jitter
func NewEnhancedNegativeCache(config *NegativeCacheConfig, logger *logx.Logger) *EnhancedNegativeCache {
	if config == nil {
		config = DefaultNegativeCacheConfig()
	}

	return &EnhancedNegativeCache{
		entries:   make(map[string]*NegativeCacheEntry),
		baseTTL:   time.Duration(config.BaseTTLHours) * time.Hour,
		jitterMin: time.Duration(config.JitterMinHours) * time.Hour,
		jitterMax: time.Duration(config.JitterMaxHours) * time.Hour,
		logger:    logger,
	}
}

// CacheNegativeResult caches a negative result with jittered TTL (PM #4)
func (enc *EnhancedNegativeCache) CacheNegativeResult(cellKey, reason string) {
	enc.mu.Lock()
	defer enc.mu.Unlock()

	now := time.Now()

	// Calculate jittered expiry time (PM #4)
	jitterRange := enc.jitterMax - enc.jitterMin
	jitter := time.Duration(secureRandomInt64(int64(jitterRange)))
	expiresAt := now.Add(enc.jitterMin + jitter)

	// Update or create entry
	if existing, exists := enc.entries[cellKey]; exists {
		existing.AttemptCount++
		existing.CachedAt = now
		existing.ExpiresAt = expiresAt
		existing.Reason = reason
	} else {
		enc.entries[cellKey] = &NegativeCacheEntry{
			CellKey:      cellKey,
			CachedAt:     now,
			ExpiresAt:    expiresAt,
			Reason:       reason,
			AttemptCount: 1,
		}
	}

	enc.logger.Debug("negative_cache_entry_added",
		"cell_key", cellKey,
		"reason", reason,
		"expires_at", expiresAt.Format(time.RFC3339),
		"ttl_hours", expiresAt.Sub(now).Hours(),
	)

	// Periodic cleanup (1% chance)
	if secureRandomFloat32() < 0.01 {
		enc.cleanupExpiredEntries()
	}
}

// IsNegativelyCached checks if a cell is negatively cached
func (enc *EnhancedNegativeCache) IsNegativelyCached(cellKey string) bool {
	enc.mu.RLock()
	defer enc.mu.RUnlock()

	entry, exists := enc.entries[cellKey]
	if !exists {
		return false
	}

	// Check if expired
	if time.Now().After(entry.ExpiresAt) {
		// Remove expired entry
		enc.mu.RUnlock()
		enc.mu.Lock()
		delete(enc.entries, cellKey)
		enc.mu.Unlock()
		enc.mu.RLock()

		enc.logger.Debug("negative_cache_entry_expired",
			"cell_key", cellKey,
			"was_cached_for", time.Since(entry.CachedAt),
		)
		return false
	}

	enc.logger.Debug("negative_cache_hit",
		"cell_key", cellKey,
		"reason", entry.Reason,
		"attempt_count", entry.AttemptCount,
		"expires_in", time.Until(entry.ExpiresAt),
	)

	return true
}

// GetNegativeEntry returns the negative cache entry if it exists and is valid
func (enc *EnhancedNegativeCache) GetNegativeEntry(cellKey string) *NegativeCacheEntry {
	enc.mu.RLock()
	defer enc.mu.RUnlock()

	entry, exists := enc.entries[cellKey]
	if !exists {
		return nil
	}

	if time.Now().After(entry.ExpiresAt) {
		return nil
	}

	return entry
}

// RemoveNegativeEntry removes a negative cache entry (e.g., when we get a positive result)
func (enc *EnhancedNegativeCache) RemoveNegativeEntry(cellKey string) {
	enc.mu.Lock()
	defer enc.mu.Unlock()

	if _, exists := enc.entries[cellKey]; exists {
		delete(enc.entries, cellKey)
		enc.logger.Debug("negative_cache_entry_removed",
			"cell_key", cellKey,
			"reason", "positive_result_received",
		)
	}
}

// cleanupExpiredEntries removes expired entries
func (enc *EnhancedNegativeCache) cleanupExpiredEntries() {
	now := time.Now()
	expiredCount := 0

	for cellKey, entry := range enc.entries {
		if now.After(entry.ExpiresAt) {
			delete(enc.entries, cellKey)
			expiredCount++
		}
	}

	if expiredCount > 0 {
		enc.logger.Debug("negative_cache_cleanup",
			"expired_entries", expiredCount,
			"remaining_entries", len(enc.entries),
		)
	}
}

// GetStats returns negative cache statistics
func (enc *EnhancedNegativeCache) GetStats() map[string]interface{} {
	enc.mu.RLock()
	defer enc.mu.RUnlock()

	now := time.Now()
	activeEntries := 0
	expiredEntries := 0
	totalAttempts := 0

	for _, entry := range enc.entries {
		totalAttempts += entry.AttemptCount
		if now.After(entry.ExpiresAt) {
			expiredEntries++
		} else {
			activeEntries++
		}
	}

	return map[string]interface{}{
		"total_entries":    len(enc.entries),
		"active_entries":   activeEntries,
		"expired_entries":  expiredEntries,
		"total_attempts":   totalAttempts,
		"base_ttl_hours":   enc.baseTTL.Hours(),
		"jitter_min_hours": enc.jitterMin.Hours(),
		"jitter_max_hours": enc.jitterMax.Hours(),
	}
}

// ForceCleanup forces cleanup of all expired entries
func (enc *EnhancedNegativeCache) ForceCleanup() int {
	enc.mu.Lock()
	defer enc.mu.Unlock()

	now := time.Now()
	expiredCount := 0

	for cellKey, entry := range enc.entries {
		if now.After(entry.ExpiresAt) {
			delete(enc.entries, cellKey)
			expiredCount++
		}
	}

	enc.logger.Info("negative_cache_force_cleanup",
		"expired_entries", expiredCount,
		"remaining_entries", len(enc.entries),
	)

	return expiredCount
}

// secureRandomInt64 generates a cryptographically secure random int64 in range [0, max)
func secureRandomInt64(max int64) int64 {
	if max <= 0 {
		return 0
	}
	
	var buf [8]byte
	_, err := rand.Read(buf[:])
	if err != nil {
		// Fallback to time-based seed if crypto/rand fails
		return time.Now().UnixNano() % max
	}
	
	val := binary.BigEndian.Uint64(buf[:])
	return int64(val % uint64(max))
}

// secureRandomFloat32 generates a cryptographically secure random float32 in range [0, 1)
func secureRandomFloat32() float32 {
	var buf [4]byte
	_, err := rand.Read(buf[:])
	if err != nil {
		// Fallback to time-based value if crypto/rand fails
		return float32(time.Now().UnixNano()%1000) / 1000.0
	}
	
	val := binary.BigEndian.Uint32(buf[:])
	return float32(val) / float32(^uint32(0))
}
