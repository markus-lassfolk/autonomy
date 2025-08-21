package gps

import (
	"crypto/md5"
	"fmt"
	"sort"
	"strings"
	"time"

	"github.com/markus-lassfolk/autonomy/pkg/logx"
)

// CellTowerInfo represents a single cell tower with signal strength
type CellTowerInfo struct {
	CellID string
	RSRP   int    // Signal strength
	RSRQ   int    // Signal quality
	EARFCN int    // Frequency
	PCI    int    // Physical Cell ID
	Type   string // "intra", "inter", or "serving"
}

// CellEnvironment represents the current cellular environment
type CellEnvironment struct {
	ServingCell   CellTowerInfo
	NeighborCells []CellTowerInfo
	Timestamp     time.Time
	LocationHash  string // Hash of the cellular environment for comparison
}

// IntelligentCellCache manages smart caching of cell tower location data
type IntelligentCellCache struct {
	logger             *logx.Logger
	lastEnvironment    *CellEnvironment
	lastLocationQuery  time.Time
	lastLocationResult *CellTowerLocation
	debounceTimer      time.Time

	// Configuration
	maxCacheAge          time.Duration // Fallback cache time (e.g., 1 hour)
	debounceDelay        time.Duration // Debounce delay (e.g., 10 seconds)
	towerChangeThreshold float64       // Percentage threshold for tower changes (e.g., 0.35 = 35%)
	topTowersCount       int           // Number of top towers to monitor (e.g., 5)
}

// IntelligentCellCacheConfig holds configuration for the intelligent cache
type IntelligentCellCacheConfig struct {
	MaxCacheAge          time.Duration `json:"max_cache_age"`
	DebounceDelay        time.Duration `json:"debounce_delay"`
	TowerChangeThreshold float64       `json:"tower_change_threshold"`
	TopTowersCount       int           `json:"top_towers_count"`
}

// DefaultIntelligentCellCacheConfig returns default configuration
func DefaultIntelligentCellCacheConfig() *IntelligentCellCacheConfig {
	return &IntelligentCellCacheConfig{
		MaxCacheAge:          1 * time.Hour,
		DebounceDelay:        10 * time.Second,
		TowerChangeThreshold: 0.35, // 35%
		TopTowersCount:       5,
	}
}

// NewIntelligentCellCache creates a new intelligent cache with configuration
func NewIntelligentCellCache(config *IntelligentCellCacheConfig, logger *logx.Logger) *IntelligentCellCache {
	if config == nil {
		config = DefaultIntelligentCellCacheConfig()
	}

	return &IntelligentCellCache{
		logger:               logger,
		maxCacheAge:          config.MaxCacheAge,
		debounceDelay:        config.DebounceDelay,
		towerChangeThreshold: config.TowerChangeThreshold,
		topTowersCount:       config.TopTowersCount,
	}
}

// ShouldQueryLocation determines if we should query for a new location
func (cache *IntelligentCellCache) ShouldQueryLocation(currentEnv *CellEnvironment) (bool, string) {
	now := time.Now()

	// Always query if we have no previous data
	if cache.lastEnvironment == nil || cache.lastLocationResult == nil {
		cache.logger.Debug("Intelligent cache: no previous data, querying")
		return true, "no_previous_data"
	}

	// Check if we're still in debounce period
	if now.Sub(cache.debounceTimer) < cache.debounceDelay {
		cache.logger.Debug("Intelligent cache: debounce active", "remaining", cache.debounceDelay-now.Sub(cache.debounceTimer))
		return false, "debounce_active"
	}

	// Check if serving cell has changed
	if cache.lastEnvironment.ServingCell.CellID != currentEnv.ServingCell.CellID {
		cache.logger.Info("Intelligent cache: serving cell changed",
			"old", cache.lastEnvironment.ServingCell.CellID,
			"new", currentEnv.ServingCell.CellID)
		cache.debounceTimer = now
		return true, "serving_cell_changed"
	}

	// Check if ≥35% of towers differ from last fix
	changePercentage := cache.calculateTowerChangePercentage(currentEnv)
	if changePercentage >= cache.towerChangeThreshold {
		cache.logger.Info("Intelligent cache: significant tower change",
			"percentage", fmt.Sprintf("%.1f%%", changePercentage*100),
			"threshold", fmt.Sprintf("%.1f%%", cache.towerChangeThreshold*100))
		cache.debounceTimer = now
		return true, fmt.Sprintf("tower_change_%.1f%%", changePercentage*100)
	}

	// Check if ≥2 of the top-5 strongest have changed
	topChanges := cache.countTopTowerChanges(currentEnv)
	if topChanges >= 2 {
		cache.logger.Info("Intelligent cache: top towers changed",
			"changes", topChanges,
			"monitored", cache.topTowersCount)
		cache.debounceTimer = now
		return true, fmt.Sprintf("top_%d_towers_changed_%d", cache.topTowersCount, topChanges)
	}

	// Fallback: check if cache has expired
	if now.Sub(cache.lastLocationQuery) >= cache.maxCacheAge {
		cache.logger.Info("Intelligent cache: cache expired",
			"age", now.Sub(cache.lastLocationQuery),
			"max_age", cache.maxCacheAge)
		return true, "cache_expired"
	}

	cache.logger.Debug("Intelligent cache: using cached location")
	return false, "using_cache"
}

// calculateTowerChangePercentage calculates what percentage of towers have changed
func (cache *IntelligentCellCache) calculateTowerChangePercentage(currentEnv *CellEnvironment) float64 {
	if cache.lastEnvironment == nil {
		return 1.0 // 100% change if no previous data
	}

	// Create maps for easy lookup
	lastTowers := make(map[string]bool)
	for _, tower := range cache.lastEnvironment.NeighborCells {
		lastTowers[tower.CellID] = true
	}

	currentTowers := make(map[string]bool)
	for _, tower := range currentEnv.NeighborCells {
		currentTowers[tower.CellID] = true
	}

	// Count total unique towers (union)
	allTowers := make(map[string]bool)
	for cellID := range lastTowers {
		allTowers[cellID] = true
	}
	for cellID := range currentTowers {
		allTowers[cellID] = true
	}

	if len(allTowers) == 0 {
		return 0.0
	}

	// Count towers that are different (not in intersection)
	intersection := 0
	for cellID := range currentTowers {
		if lastTowers[cellID] {
			intersection++
		}
	}

	// Calculate change percentage
	totalTowers := len(allTowers)
	unchangedTowers := intersection
	changedTowers := totalTowers - unchangedTowers

	return float64(changedTowers) / float64(totalTowers)
}

// countTopTowerChanges counts how many of the top N strongest towers have changed
func (cache *IntelligentCellCache) countTopTowerChanges(currentEnv *CellEnvironment) int {
	if cache.lastEnvironment == nil {
		return cache.topTowersCount // All are "changed" if no previous data
	}

	// Get top N towers from last environment (sorted by RSRP - higher is better)
	lastTopTowers := cache.getTopTowers(cache.lastEnvironment.NeighborCells, cache.topTowersCount)
	currentTopTowers := cache.getTopTowers(currentEnv.NeighborCells, cache.topTowersCount)

	// Count how many of the current top towers were not in the last top towers
	lastTopMap := make(map[string]bool)
	for _, tower := range lastTopTowers {
		lastTopMap[tower.CellID] = true
	}

	changes := 0
	for _, tower := range currentTopTowers {
		if !lastTopMap[tower.CellID] {
			changes++
		}
	}

	return changes
}

// getTopTowers returns the top N towers sorted by signal strength (RSRP)
func (cache *IntelligentCellCache) getTopTowers(towers []CellTowerInfo, count int) []CellTowerInfo {
	// Create a copy to avoid modifying the original slice
	sortedTowers := make([]CellTowerInfo, len(towers))
	copy(sortedTowers, towers)

	// Sort by RSRP (higher is better, so reverse sort)
	sort.Slice(sortedTowers, func(i, j int) bool {
		return sortedTowers[i].RSRP > sortedTowers[j].RSRP
	})

	// Return top N towers
	if len(sortedTowers) < count {
		return sortedTowers
	}
	return sortedTowers[:count]
}

// UpdateCache updates the cache with new environment and location data
func (cache *IntelligentCellCache) UpdateCache(env *CellEnvironment, location *CellTowerLocation) {
	cache.lastEnvironment = env
	cache.lastLocationResult = location
	cache.lastLocationQuery = time.Now()

	cache.logger.Debug("Intelligent cache updated",
		"serving_cell", env.ServingCell.CellID,
		"neighbor_count", len(env.NeighborCells),
		"location", fmt.Sprintf("%.6f,%.6f", location.Latitude, location.Longitude),
		"accuracy", location.Accuracy)
}

// GetCachedLocation returns the cached location if available
func (cache *IntelligentCellCache) GetCachedLocation() *CellTowerLocation {
	return cache.lastLocationResult
}

// generateEnvironmentHash creates a hash of the cellular environment for comparison
func (cache *IntelligentCellCache) generateEnvironmentHash(env *CellEnvironment) string {
	var parts []string

	// Add serving cell
	parts = append(parts, fmt.Sprintf("serving:%s:%d", env.ServingCell.CellID, env.ServingCell.RSRP))

	// Add neighbor cells (sorted for consistent hash)
	var neighbors []string
	for _, tower := range env.NeighborCells {
		neighbors = append(neighbors, fmt.Sprintf("%s:%d", tower.CellID, tower.RSRP))
	}
	sort.Strings(neighbors)
	parts = append(parts, neighbors...)

	// Create hash
	data := strings.Join(parts, "|")
	hash := md5.Sum([]byte(data))
	return fmt.Sprintf("%x", hash)
}

// ParseCellEnvironmentFromIntelligence converts CellularLocationIntelligence to CellEnvironment
func ParseCellEnvironmentFromIntelligence(intel *CellularLocationIntelligence) (*CellEnvironment, error) {
	env := &CellEnvironment{
		Timestamp: time.Now(),
	}

	// Parse serving cell from ServingCell info
	env.ServingCell = CellTowerInfo{
		CellID: intel.ServingCell.CellID,
		RSRP:   intel.SignalQuality.OverallRSRP,
		RSRQ:   intel.SignalQuality.OverallRSRQ,
		EARFCN: intel.ServingCell.EARFCN,
		PCI:    intel.ServingCell.PCI, // Assuming PCI not PCID
		Type:   "serving",
	}

	// Parse neighbor cells
	for _, neighbor := range intel.NeighborCells {
		tower := CellTowerInfo{
			CellID: fmt.Sprintf("neighbor_%d", neighbor.PCID), // Use PCID as identifier
			RSRP:   neighbor.RSRP,
			RSRQ:   neighbor.RSRQ,
			EARFCN: 0, // Not available in neighbor info
			PCI:    neighbor.PCID,
			Type:   "neighbor", // Default type since not in struct
		}
		env.NeighborCells = append(env.NeighborCells, tower)
	}

	// Generate environment hash
	cache := &IntelligentCellCache{} // Temporary instance for hash generation
	env.LocationHash = cache.generateEnvironmentHash(env)

	return env, nil
}

// GetCacheStats returns statistics about the cache usage
func (cache *IntelligentCellCache) GetCacheStats() map[string]interface{} {
	stats := make(map[string]interface{})

	if cache.lastEnvironment != nil {
		stats["last_serving_cell"] = cache.lastEnvironment.ServingCell.CellID
		stats["last_neighbor_count"] = len(cache.lastEnvironment.NeighborCells)
		stats["last_query_age"] = time.Since(cache.lastLocationQuery).String()
	}

	if cache.lastLocationResult != nil {
		stats["cached_latitude"] = cache.lastLocationResult.Latitude
		stats["cached_longitude"] = cache.lastLocationResult.Longitude
		stats["cached_accuracy"] = cache.lastLocationResult.Accuracy
		stats["cached_source"] = cache.lastLocationResult.Source
	}

	stats["max_cache_age"] = cache.maxCacheAge.String()
	stats["debounce_delay"] = cache.debounceDelay.String()
	stats["tower_change_threshold"] = fmt.Sprintf("%.1f%%", cache.towerChangeThreshold*100)
	stats["top_towers_count"] = cache.topTowersCount

	return stats
}

// Reset clears all cached data
func (cache *IntelligentCellCache) Reset() {
	cache.lastEnvironment = nil
	cache.lastLocationResult = nil
	cache.lastLocationQuery = time.Time{}
	cache.debounceTimer = time.Time{}

	cache.logger.Info("Intelligent cell cache reset")
}
