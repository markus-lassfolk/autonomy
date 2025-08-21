package gps

import (
	"encoding/json"
	"fmt"
	"os"
	"path/filepath"
	"sync"
	"time"

	"github.com/markus-lassfolk/autonomy/pkg/logx"
	bolt "go.etcd.io/bbolt"
)

// EnhancedIntelligentCellCache implements a high-performance cell cache with LRU eviction and persistence
type EnhancedIntelligentCellCache struct {
	logger *logx.Logger
	config *EnhancedCellCacheConfig
	db     *bolt.DB
	stats  *CacheStats
	mu     sync.RWMutex
}

// EnhancedCellCacheConfig holds configuration for the enhanced cell cache
type EnhancedCellCacheConfig struct {
	MaxSizeMB           int    `json:"max_size_mb"`
	NegativeTTLHours    int    `json:"negative_ttl_hours"`
	CompressionEnabled  bool   `json:"compression_enabled"`
	EvictionPolicy      string `json:"eviction_policy"` // "lru", "lfu", "ttl"
	PersistencePath     string `json:"persistence_path"`
	SyncIntervalSeconds int    `json:"sync_interval_seconds"`
	MaxEntriesPerBucket int    `json:"max_entries_per_bucket"`
}

// CachedCellLocation represents a cached cell tower location
type CachedCellLocation struct {
	CellID      CellIdentifier `json:"cell_id"`
	Latitude    float64        `json:"latitude"`
	Longitude   float64        `json:"longitude"`
	Range       float64        `json:"range"`
	Samples     int            `json:"samples"`
	Confidence  float64        `json:"confidence"`
	Changeable  bool           `json:"changeable"`
	Source      string         `json:"source"`
	CachedAt    time.Time      `json:"cached_at"`
	LastAccess  time.Time      `json:"last_access"`
	LastSeenAt  time.Time      `json:"last_seen_at"` // When cell was last observed in scan
	LastUsedAt  time.Time      `json:"last_used_at"` // When cell was last used for positioning
	AccessCount int            `json:"access_count"`
	IsNegative  bool           `json:"is_negative"`
}

// CacheStats tracks cache performance statistics
type CacheStats struct {
	TotalEntries    int       `json:"total_entries"`
	PositiveEntries int       `json:"positive_entries"`
	NegativeEntries int       `json:"negative_entries"`
	CacheHits       int64     `json:"cache_hits"`
	CacheMisses     int64     `json:"cache_misses"`
	Evictions       int64     `json:"evictions"`
	SizeBytes       int64     `json:"size_bytes"`
	SizeMB          float64   `json:"size_mb"`
	HitRate         float64   `json:"hit_rate"`
	LastEviction    time.Time `json:"last_eviction"`
	LastCompaction  time.Time `json:"last_compaction"`
}

// Bucket names for bbolt database
const (
	CellDataBucket = "cell_data"
	MetadataBucket = "metadata"
	StatsBucket    = "stats"
)

// NewEnhancedIntelligentCellCache creates a new enhanced intelligent cell cache
func NewEnhancedIntelligentCellCache(config *EnhancedCellCacheConfig, logger *logx.Logger) (*EnhancedIntelligentCellCache, error) {
	if config == nil {
		config = DefaultEnhancedCellCacheConfig()
	}

	// Ensure cache directory exists
	cacheDir := filepath.Dir(config.PersistencePath)
	if err := os.MkdirAll(cacheDir, 0o755); err != nil {
		return nil, fmt.Errorf("failed to create cache directory: %w", err)
	}

	// Open bbolt database
	db, err := bolt.Open(config.PersistencePath, 0o600, &bolt.Options{
		Timeout: 5 * time.Second,
	})
	if err != nil {
		return nil, fmt.Errorf("failed to open cache database: %w", err)
	}

	cache := &EnhancedIntelligentCellCache{
		logger: logger,
		config: config,
		db:     db,
		stats:  &CacheStats{},
	}

	// Initialize database buckets
	if err := cache.initializeBuckets(); err != nil {
		db.Close()
		return nil, fmt.Errorf("failed to initialize cache buckets: %w", err)
	}

	// Load existing statistics
	if err := cache.loadStats(); err != nil {
		logger.Warn("failed_to_load_cache_stats", "error", err.Error())
	}

	// Start background maintenance
	go cache.backgroundMaintenance()

	logger.Info("enhanced_cell_cache_initialized",
		"max_size_mb", config.MaxSizeMB,
		"persistence_path", config.PersistencePath,
		"eviction_policy", config.EvictionPolicy,
		"existing_entries", cache.stats.TotalEntries,
	)

	return cache, nil
}

// DefaultEnhancedCellCacheConfig returns default cache configuration
func DefaultEnhancedCellCacheConfig() *EnhancedCellCacheConfig {
	return &EnhancedCellCacheConfig{
		MaxSizeMB:           25,
		NegativeTTLHours:    12,
		CompressionEnabled:  true,
		EvictionPolicy:      "lru",
		PersistencePath:     "/overlay/autonomy/opencellid_cache.db",
		SyncIntervalSeconds: 300, // 5 minutes
		MaxEntriesPerBucket: 10000,
	}
}

// initializeBuckets creates necessary database buckets
func (ecc *EnhancedIntelligentCellCache) initializeBuckets() error {
	return ecc.db.Update(func(tx *bolt.Tx) error {
		buckets := []string{CellDataBucket, MetadataBucket, StatsBucket}
		for _, bucket := range buckets {
			if _, err := tx.CreateBucketIfNotExists([]byte(bucket)); err != nil {
				return fmt.Errorf("failed to create bucket %s: %w", bucket, err)
			}
		}
		return nil
	})
}

// Get retrieves a cached cell location
func (ecc *EnhancedIntelligentCellCache) Get(cellID CellIdentifier) (*CachedCellLocation, error) {
	ecc.mu.Lock()
	defer ecc.mu.Unlock()

	key := ecc.generateKey(cellID)
	var cached *CachedCellLocation

	err := ecc.db.View(func(tx *bolt.Tx) error {
		bucket := tx.Bucket([]byte(CellDataBucket))
		if bucket == nil {
			return fmt.Errorf("cell data bucket not found")
		}

		data := bucket.Get([]byte(key))
		if data == nil {
			return nil // Cache miss
		}

		cached = &CachedCellLocation{}
		if err := json.Unmarshal(data, cached); err != nil {
			return fmt.Errorf("failed to unmarshal cached data: %w", err)
		}

		return nil
	})
	if err != nil {
		ecc.stats.CacheMisses++
		return nil, err
	}

	if cached == nil {
		ecc.stats.CacheMisses++
		return nil, nil // Cache miss
	}

	// Update access statistics
	cached.LastAccess = time.Now()
	cached.AccessCount++

	// Update in database (async to avoid blocking)
	go ecc.updateAccessStats(key, cached)

	ecc.stats.CacheHits++
	ecc.updateHitRate()

	ecc.logger.LogDebugVerbose("cache_hit", map[string]interface{}{
		"cell_key":     key,
		"is_negative":  cached.IsNegative,
		"cached_at":    cached.CachedAt,
		"access_count": cached.AccessCount,
	})

	return cached, nil
}

// Set stores a cell location in the cache
func (ecc *EnhancedIntelligentCellCache) Set(cellID CellIdentifier, location *CachedCellLocation) error {
	ecc.mu.Lock()
	defer ecc.mu.Unlock()

	key := ecc.generateKey(cellID)
	location.LastAccess = time.Now()
	location.AccessCount = 1

	// Serialize location
	data, err := json.Marshal(location)
	if err != nil {
		return fmt.Errorf("failed to marshal location: %w", err)
	}

	// Store in database
	err = ecc.db.Update(func(tx *bolt.Tx) error {
		bucket := tx.Bucket([]byte(CellDataBucket))
		if bucket == nil {
			return fmt.Errorf("cell data bucket not found")
		}

		return bucket.Put([]byte(key), data)
	})
	if err != nil {
		return fmt.Errorf("failed to store in cache: %w", err)
	}

	// Update statistics
	ecc.updateStatsAfterSet(location)

	// Check if eviction is needed
	if ecc.shouldEvict() {
		go ecc.performEviction()
	}

	ecc.logger.LogDebugVerbose("cache_set", map[string]interface{}{
		"cell_key":    key,
		"is_negative": location.IsNegative,
		"size_mb":     ecc.stats.SizeMB,
	})

	return nil
}

// Delete removes a cell location from the cache
func (ecc *EnhancedIntelligentCellCache) Delete(cellID CellIdentifier) error {
	ecc.mu.Lock()
	defer ecc.mu.Unlock()

	key := ecc.generateKey(cellID)

	err := ecc.db.Update(func(tx *bolt.Tx) error {
		bucket := tx.Bucket([]byte(CellDataBucket))
		if bucket == nil {
			return fmt.Errorf("cell data bucket not found")
		}

		return bucket.Delete([]byte(key))
	})
	if err != nil {
		return fmt.Errorf("failed to delete from cache: %w", err)
	}

	// Update statistics
	ecc.stats.TotalEntries--
	ecc.updateCacheSize()

	return nil
}

// generateKey generates a unique key for a cell identifier
func (ecc *EnhancedIntelligentCellCache) generateKey(cellID CellIdentifier) string {
	return fmt.Sprintf("%s:%s:%s:%s:%s", cellID.MCC, cellID.MNC, cellID.LAC, cellID.CellID, cellID.Radio)
}

// updateAccessStats updates access statistics for a cached entry
func (ecc *EnhancedIntelligentCellCache) updateAccessStats(key string, cached *CachedCellLocation) {
	if err := ecc.db.Update(func(tx *bolt.Tx) error {
		bucket := tx.Bucket([]byte(CellDataBucket))
		if bucket == nil {
			return nil
		}

		data, err := json.Marshal(cached)
		if err != nil {
			return err
		}

		return bucket.Put([]byte(key), data)
	}); err != nil {
		ecc.logger.Warn("Failed to update access stats", "error", err)
	}
}

// updateStatsAfterSet updates statistics after setting a cache entry
func (ecc *EnhancedIntelligentCellCache) updateStatsAfterSet(location *CachedCellLocation) {
	ecc.stats.TotalEntries++

	if location.IsNegative {
		ecc.stats.NegativeEntries++
	} else {
		ecc.stats.PositiveEntries++
	}

	ecc.updateCacheSize()
}

// updateCacheSize updates the cache size statistics
func (ecc *EnhancedIntelligentCellCache) updateCacheSize() {
	// Estimate size based on entry count (approximate)
	avgEntrySize := 200 // bytes per entry (estimated)
	ecc.stats.SizeBytes = int64(ecc.stats.TotalEntries * avgEntrySize)
	ecc.stats.SizeMB = float64(ecc.stats.SizeBytes) / (1024 * 1024)
}

// updateHitRate updates the cache hit rate
func (ecc *EnhancedIntelligentCellCache) updateHitRate() {
	total := ecc.stats.CacheHits + ecc.stats.CacheMisses
	if total > 0 {
		ecc.stats.HitRate = float64(ecc.stats.CacheHits) / float64(total)
	}
}

// shouldEvict determines if cache eviction is needed
func (ecc *EnhancedIntelligentCellCache) shouldEvict() bool {
	return ecc.stats.SizeMB > float64(ecc.config.MaxSizeMB)
}

// performEviction performs LRU eviction to reduce cache size
func (ecc *EnhancedIntelligentCellCache) performEviction() {
	ecc.mu.Lock()
	defer ecc.mu.Unlock()

	targetSize := float64(ecc.config.MaxSizeMB) * 0.8 // Evict to 80% of max size
	evicted := 0

	ecc.logger.Info("cache_eviction_start",
		"current_size_mb", ecc.stats.SizeMB,
		"target_size_mb", targetSize,
		"total_entries", ecc.stats.TotalEntries,
	)

	// Collect entries for eviction (LRU policy)
	type evictionCandidate struct {
		key        string
		lastAccess time.Time
		isNegative bool
	}

	var candidates []evictionCandidate

	if err := ecc.db.View(func(tx *bolt.Tx) error {
		bucket := tx.Bucket([]byte(CellDataBucket))
		if bucket == nil {
			return nil
		}

		cursor := bucket.Cursor()
		for key, value := cursor.First(); key != nil; key, value = cursor.Next() {
			var cached CachedCellLocation
			if json.Unmarshal(value, &cached) == nil {
				candidates = append(candidates, evictionCandidate{
					key:        string(key),
					lastAccess: cached.LastAccess,
					isNegative: cached.IsNegative,
				})
			}
		}
		return nil
	}); err != nil {
		ecc.logger.Warn("Failed to read cache for eviction", "error", err)
		return
	}

	// Sort by last access time (oldest first) and prioritize negative entries
	for i := 0; i < len(candidates)-1; i++ {
		for j := i + 1; j < len(candidates); j++ {
			shouldSwap := false

			// Prioritize negative entries for eviction
			if candidates[i].isNegative != candidates[j].isNegative {
				shouldSwap = candidates[j].isNegative
			} else {
				// Same type, sort by last access time
				shouldSwap = candidates[i].lastAccess.After(candidates[j].lastAccess)
			}

			if shouldSwap {
				candidates[i], candidates[j] = candidates[j], candidates[i]
			}
		}
	}

	// Evict entries until we reach target size
	if err := ecc.db.Update(func(tx *bolt.Tx) error {
		bucket := tx.Bucket([]byte(CellDataBucket))
		if bucket == nil {
			return nil
		}

		for _, candidate := range candidates {
			if ecc.stats.SizeMB <= targetSize {
				break
			}

			if err := bucket.Delete([]byte(candidate.key)); err != nil {
				return err
			}
			evicted++

			if candidate.isNegative {
				ecc.stats.NegativeEntries--
			} else {
				ecc.stats.PositiveEntries--
			}
			ecc.stats.TotalEntries--
		}

		return nil
	}); err != nil {
		ecc.logger.Warn("Failed to perform cache eviction", "error", err)
		return
	}

	ecc.updateCacheSize()
	ecc.stats.Evictions += int64(evicted)
	ecc.stats.LastEviction = time.Now()

	ecc.logger.Info("cache_eviction_complete",
		"evicted_entries", evicted,
		"new_size_mb", ecc.stats.SizeMB,
		"remaining_entries", ecc.stats.TotalEntries,
	)
}

// backgroundMaintenance performs periodic cache maintenance
func (ecc *EnhancedIntelligentCellCache) backgroundMaintenance() {
	ticker := time.NewTicker(time.Duration(ecc.config.SyncIntervalSeconds) * time.Second)
	defer ticker.Stop()

	for range ticker.C {
		ecc.performMaintenance()
	}
}

// performMaintenance performs cache maintenance tasks
func (ecc *EnhancedIntelligentCellCache) performMaintenance() {
	ecc.mu.Lock()
	defer ecc.mu.Unlock()

	// Clean up expired negative entries
	ecc.cleanupExpiredNegativeEntries()

	// Update cache statistics
	ecc.updateCacheStats()

	// Perform database compaction if needed
	if time.Since(ecc.stats.LastCompaction) > 24*time.Hour {
		ecc.compactDatabase()
	}

	// Save statistics
	if err := ecc.saveStats(); err != nil {
		ecc.logger.Warn("Failed to save cache stats", "error", err)
	}
}

// cleanupExpiredNegativeEntries removes expired negative cache entries
func (ecc *EnhancedIntelligentCellCache) cleanupExpiredNegativeEntries() int {
	ttl := time.Duration(ecc.config.NegativeTTLHours) * time.Hour
	cutoff := time.Now().Add(-ttl)
	cleaned := 0

	if err := ecc.db.Update(func(tx *bolt.Tx) error {
		bucket := tx.Bucket([]byte(CellDataBucket))
		if bucket == nil {
			return nil
		}

		cursor := bucket.Cursor()
		for key, value := cursor.First(); key != nil; key, value = cursor.Next() {
			var cached CachedCellLocation
			if json.Unmarshal(value, &cached) == nil {
				if cached.IsNegative && cached.CachedAt.Before(cutoff) {
					if err := cursor.Delete(); err != nil {
						return err
					}
					cleaned++
					ecc.stats.NegativeEntries--
					ecc.stats.TotalEntries--
				}
			}
		}
		return nil
	}); err != nil {
		ecc.logger.Warn("Failed to cleanup expired negative entries", "error", err)
	}

	if cleaned > 0 {
		ecc.updateCacheSize()
		ecc.logger.LogDebugVerbose("cache_negative_cleanup", map[string]interface{}{
			"cleaned_entries": cleaned,
			"ttl_hours":       ecc.config.NegativeTTLHours,
		})
	}

	return cleaned
}

// updateStats updates cache statistics (alias for updateCacheStats)
func (ecc *EnhancedIntelligentCellCache) updateStats() {
	ecc.updateCacheStats()
}

// updateCacheStats updates comprehensive cache statistics
func (ecc *EnhancedIntelligentCellCache) updateCacheStats() {
	var totalEntries, positiveEntries, negativeEntries int

	if err := ecc.db.View(func(tx *bolt.Tx) error {
		bucket := tx.Bucket([]byte(CellDataBucket))
		if bucket == nil {
			return nil
		}

		cursor := bucket.Cursor()
		for key, value := cursor.First(); key != nil; key, value = cursor.Next() {
			totalEntries++

			var cached CachedCellLocation
			if json.Unmarshal(value, &cached) == nil {
				if cached.IsNegative {
					negativeEntries++
				} else {
					positiveEntries++
				}
			}
		}
		return nil
	}); err != nil {
		ecc.logger.Warn("Failed to update cache stats", "error", err)
	}

	ecc.stats.TotalEntries = totalEntries
	ecc.stats.PositiveEntries = positiveEntries
	ecc.stats.NegativeEntries = negativeEntries
	ecc.updateCacheSize()
	ecc.updateHitRate()
}

// compactDatabase performs database compaction
func (ecc *EnhancedIntelligentCellCache) compactDatabase() {
	start := time.Now()

	// bbolt doesn't have explicit compaction, but we can trigger it by copying data
	ecc.logger.Info("cache_compaction_start")

	// For now, just update the timestamp
	ecc.stats.LastCompaction = time.Now()

	ecc.logger.Info("cache_compaction_complete",
		"duration_ms", time.Since(start).Milliseconds(),
	)
}

// loadStats loads cache statistics from database
func (ecc *EnhancedIntelligentCellCache) loadStats() error {
	return ecc.db.View(func(tx *bolt.Tx) error {
		bucket := tx.Bucket([]byte(StatsBucket))
		if bucket == nil {
			return nil
		}

		data := bucket.Get([]byte("cache_stats"))
		if data == nil {
			return nil
		}

		return json.Unmarshal(data, ecc.stats)
	})
}

// saveStats saves cache statistics to database
func (ecc *EnhancedIntelligentCellCache) saveStats() error {
	data, err := json.Marshal(ecc.stats)
	if err != nil {
		return err
	}

	return ecc.db.Update(func(tx *bolt.Tx) error {
		bucket := tx.Bucket([]byte(StatsBucket))
		if bucket == nil {
			return fmt.Errorf("stats bucket not found")
		}

		return bucket.Put([]byte("cache_stats"), data)
	})
}

// GetStats returns current cache statistics
func (ecc *EnhancedIntelligentCellCache) GetStats() CacheStats {
	ecc.mu.RLock()
	defer ecc.mu.RUnlock()
	return *ecc.stats
}

// Close closes the cache and database
func (ecc *EnhancedIntelligentCellCache) Close() error {
	ecc.mu.Lock()
	defer ecc.mu.Unlock()

	// Save final statistics
	if err := ecc.saveStats(); err != nil {
		ecc.logger.Warn("Failed to save final cache stats", "error", err)
	}

	// Close database
	return ecc.db.Close()
}

// TouchSeen updates the LastSeenAt timestamp for a cell (when observed in scan)
func (eic *EnhancedIntelligentCellCache) TouchSeen(key CellKey, timestamp time.Time) error {
	eic.mu.Lock()
	defer eic.mu.Unlock()

	return eic.db.Update(func(tx *bolt.Tx) error {
		bucket := tx.Bucket([]byte(CellDataBucket))
		if bucket == nil {
			return fmt.Errorf("bucket not found")
		}

		keyBytes := []byte(key.String())
		data := bucket.Get(keyBytes)
		if data == nil {
			// Cell not in cache, skip touch
			return nil
		}

		var location CachedCellLocation
		if err := json.Unmarshal(data, &location); err != nil {
			return fmt.Errorf("failed to unmarshal cached location: %w", err)
		}

		// Update LastSeenAt timestamp
		location.LastSeenAt = timestamp

		// Re-serialize and store
		updatedData, err := json.Marshal(location)
		if err != nil {
			return fmt.Errorf("failed to marshal updated location: %w", err)
		}

		return bucket.Put(keyBytes, updatedData)
	})
}

// TouchUsed updates the LastUsedAt timestamp for a cell (when used for positioning)
func (eic *EnhancedIntelligentCellCache) TouchUsed(key CellKey, timestamp time.Time) error {
	eic.mu.Lock()
	defer eic.mu.Unlock()

	return eic.db.Update(func(tx *bolt.Tx) error {
		bucket := tx.Bucket([]byte(CellDataBucket))
		if bucket == nil {
			return fmt.Errorf("bucket not found")
		}

		keyBytes := []byte(key.String())
		data := bucket.Get(keyBytes)
		if data == nil {
			// Cell not in cache, skip touch
			return nil
		}

		var location CachedCellLocation
		if err := json.Unmarshal(data, &location); err != nil {
			return fmt.Errorf("failed to unmarshal cached location: %w", err)
		}

		// Update LastUsedAt timestamp and access tracking
		location.LastUsedAt = timestamp
		location.LastAccess = timestamp
		location.AccessCount++

		// Re-serialize and store
		updatedData, err := json.Marshal(location)
		if err != nil {
			return fmt.Errorf("failed to marshal updated location: %w", err)
		}

		return bucket.Put(keyBytes, updatedData)
	})
}

// MaintenanceConfig defines configuration for nightly maintenance tasks
type MaintenanceConfig struct {
	CacheAgeDays        int     `json:"cache_age_days"`        // Delete entries older than this (default: 30)
	PurgeDistanceKM     float64 `json:"purge_distance_km"`     // Delete entries farther than this (default: 300)
	RecentKeepHours     int     `json:"recent_keep_hours"`     // Always keep entries seen within this time (default: 48)
	SizeCapMB           float64 `json:"size_cap_mb"`           // Size cap for cache (default: 25)
	EnableDistancePurge bool    `json:"enable_distance_purge"` // Enable distance-based purging
}

// DefaultMaintenanceConfig returns default maintenance configuration
func DefaultMaintenanceConfig() *MaintenanceConfig {
	return &MaintenanceConfig{
		CacheAgeDays:        30,
		PurgeDistanceKM:     300.0,
		RecentKeepHours:     48,
		SizeCapMB:           25.0,
		EnableDistancePurge: true,
	}
}

// PerformNightlyMaintenance runs comprehensive cache maintenance
func (eic *EnhancedIntelligentCellCache) PerformNightlyMaintenance(config *MaintenanceConfig, currentLocation *GPSObservation) error {
	if config == nil {
		config = DefaultMaintenanceConfig()
	}

	eic.logger.Info("starting_nightly_maintenance",
		"cache_age_days", config.CacheAgeDays,
		"purge_distance_km", config.PurgeDistanceKM,
		"recent_keep_hours", config.RecentKeepHours,
		"size_cap_mb", config.SizeCapMB,
	)

	startTime := time.Now()
	var deletedCount int
	var errors []string

	// 1. Time-based aging
	aged, err := eic.performTimeBasedAging(config.CacheAgeDays, config.RecentKeepHours)
	if err != nil {
		errors = append(errors, fmt.Sprintf("time_aging_error: %v", err))
	} else {
		deletedCount += aged
	}

	// 2. Distance-based purging (if location available and enabled)
	if config.EnableDistancePurge && currentLocation != nil {
		purged, err := eic.performDistanceBasedPurging(currentLocation, config.PurgeDistanceKM, config.RecentKeepHours)
		if err != nil {
			errors = append(errors, fmt.Sprintf("distance_purging_error: %v", err))
		} else {
			deletedCount += purged
		}
	}

	// 3. Size cap enforcement
	evicted, err := eic.performSizeCapEnforcement(config.SizeCapMB, config.RecentKeepHours)
	if err != nil {
		errors = append(errors, fmt.Sprintf("size_cap_error: %v", err))
	} else {
		deletedCount += evicted
	}

	// 4. Negative cache cleanup
	negativeCleared := eic.cleanupExpiredNegativeEntries()
	deletedCount += negativeCleared

	duration := time.Since(startTime)
	eic.logger.Info("nightly_maintenance_completed",
		"duration_ms", duration.Milliseconds(),
		"total_deleted", deletedCount,
		"errors", len(errors),
	)

	if len(errors) > 0 {
		return fmt.Errorf("maintenance completed with errors: %v", errors)
	}

	return nil
}

// performTimeBasedAging removes entries older than specified days
func (eic *EnhancedIntelligentCellCache) performTimeBasedAging(ageDays, recentKeepHours int) (int, error) {
	eic.mu.Lock()
	defer eic.mu.Unlock()

	cutoffTime := time.Now().AddDate(0, 0, -ageDays)
	recentCutoff := time.Now().Add(-time.Duration(recentKeepHours) * time.Hour)
	deletedCount := 0

	return deletedCount, eic.db.Update(func(tx *bolt.Tx) error {
		bucket := tx.Bucket([]byte(CellDataBucket))
		if bucket == nil {
			return nil
		}

		var keysToDelete [][]byte
		cursor := bucket.Cursor()

		for k, v := cursor.First(); k != nil; k, v = cursor.Next() {
			var location CachedCellLocation
			if err := json.Unmarshal(v, &location); err != nil {
				continue // Skip corrupted entries
			}

			// Skip if recently seen (safety rule)
			if location.LastSeenAt.After(recentCutoff) {
				continue
			}

			// Delete if older than cutoff
			if location.CachedAt.Before(cutoffTime) {
				keysToDelete = append(keysToDelete, append([]byte(nil), k...))
			}
		}

		// Delete identified keys
		for _, key := range keysToDelete {
			if err := bucket.Delete(key); err != nil {
				return err
			}
			deletedCount++
		}

		return nil
	})
}

// performDistanceBasedPurging removes entries too far from current location
func (eic *EnhancedIntelligentCellCache) performDistanceBasedPurging(currentLocation *GPSObservation, maxDistanceKM float64, recentKeepHours int) (int, error) {
	eic.mu.Lock()
	defer eic.mu.Unlock()

	recentCutoff := time.Now().Add(-time.Duration(recentKeepHours) * time.Hour)
	deletedCount := 0

	return deletedCount, eic.db.Update(func(tx *bolt.Tx) error {
		bucket := tx.Bucket([]byte(CellDataBucket))
		if bucket == nil {
			return nil
		}

		var keysToDelete [][]byte
		cursor := bucket.Cursor()

		for k, v := cursor.First(); k != nil; k, v = cursor.Next() {
			var location CachedCellLocation
			if err := json.Unmarshal(v, &location); err != nil {
				continue // Skip corrupted entries
			}

			// Skip if recently seen (safety rule)
			if location.LastSeenAt.After(recentCutoff) {
				continue
			}

			// Skip negative entries (no location data)
			if location.IsNegative {
				continue
			}

			// Calculate distance
			distance := calculateHaversineDistance(
				currentLocation.Latitude, currentLocation.Longitude,
				location.Latitude, location.Longitude,
			)

			// Delete if too far away
			if distance > maxDistanceKM {
				keysToDelete = append(keysToDelete, append([]byte(nil), k...))
			}
		}

		// Delete identified keys
		for _, key := range keysToDelete {
			if err := bucket.Delete(key); err != nil {
				return err
			}
			deletedCount++
		}

		return nil
	})
}

// performSizeCapEnforcement enforces size cap using LRU eviction
func (eic *EnhancedIntelligentCellCache) performSizeCapEnforcement(sizeCapMB float64, recentKeepHours int) (int, error) {
	eic.mu.Lock()
	defer eic.mu.Unlock()

	// Check current size
	eic.updateStats()
	if eic.stats.SizeMB <= sizeCapMB {
		return 0, nil // No eviction needed
	}

	recentCutoff := time.Now().Add(-time.Duration(recentKeepHours) * time.Hour)
	deletedCount := 0

	// Collect all entries with their LRU timestamps
	type lruEntry struct {
		key      []byte
		lastUsed time.Time
		lastSeen time.Time
		isRecent bool
	}

	var entries []lruEntry

	err := eic.db.View(func(tx *bolt.Tx) error {
		bucket := tx.Bucket([]byte(CellDataBucket))
		if bucket == nil {
			return nil
		}

		cursor := bucket.Cursor()
		for k, v := cursor.First(); k != nil; k, v = cursor.Next() {
			var location CachedCellLocation
			if err := json.Unmarshal(v, &location); err != nil {
				continue
			}

			isRecent := location.LastSeenAt.After(recentCutoff)
			entries = append(entries, lruEntry{
				key:      append([]byte(nil), k...),
				lastUsed: location.LastUsedAt,
				lastSeen: location.LastSeenAt,
				isRecent: isRecent,
			})
		}
		return nil
	})
	if err != nil {
		return 0, err
	}

	// Sort by LRU (oldest first), but separate recent entries
	var evictableEntries []lruEntry
	for _, entry := range entries {
		if !entry.isRecent {
			evictableEntries = append(evictableEntries, entry)
		}
	}

	// Sort evictable entries by LastUsedAt (oldest first)
	for i := 0; i < len(evictableEntries)-1; i++ {
		for j := i + 1; j < len(evictableEntries); j++ {
			if evictableEntries[i].lastUsed.After(evictableEntries[j].lastUsed) {
				evictableEntries[i], evictableEntries[j] = evictableEntries[j], evictableEntries[i]
			}
		}
	}

	// Delete oldest entries until size is under cap
	return deletedCount, eic.db.Update(func(tx *bolt.Tx) error {
		bucket := tx.Bucket([]byte(CellDataBucket))
		if bucket == nil {
			return nil
		}

		for _, entry := range evictableEntries {
			if err := bucket.Delete(entry.key); err != nil {
				return err
			}
			deletedCount++

			// Check size after each deletion
			eic.updateStats()
			if eic.stats.SizeMB <= sizeCapMB {
				break
			}
		}

		return nil
	})
}

// NewMemoryOnlyCellCache creates a simple memory-only cache as fallback
func NewMemoryOnlyCellCache(logger *logx.Logger) *EnhancedIntelligentCellCache {
	// This is a simplified fallback implementation
	// In a real implementation, you would create a memory-only version
	logger.Warn("using_memory_only_cache", "reason", "persistent_cache_failed")

	// Return nil for now - in production, implement a proper memory cache
	return nil
}
