package gps

import (
	"crypto/sha256"
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
	
	// Advanced features
	enablePredictiveLoading bool
	enableGeographicClustering bool
	clusterRadius            float64 // meters
	predictiveLoadThreshold  float64 // confidence threshold for predictive loading
}

// IntelligentCellCacheConfig holds configuration for the intelligent cache
type IntelligentCellCacheConfig struct {
	MaxCacheAge              time.Duration `json:"max_cache_age"`
	DebounceDelay            time.Duration `json:"debounce_delay"`
	TowerChangeThreshold     float64       `json:"tower_change_threshold"`
	TopTowersCount           int           `json:"top_towers_count"`
	EnablePredictiveLoading  bool          `json:"enable_predictive_loading"`
	EnableGeographicClustering bool        `json:"enable_geographic_clustering"`
	ClusterRadius            float64       `json:"cluster_radius"`
	PredictiveLoadThreshold  float64       `json:"predictive_load_threshold"`
}

// DefaultIntelligentCellCacheConfig returns default configuration
func DefaultIntelligentCellCacheConfig() *IntelligentCellCacheConfig {
	return &IntelligentCellCacheConfig{
		MaxCacheAge:              1 * time.Hour,
		DebounceDelay:            10 * time.Second,
		TowerChangeThreshold:     0.35, // 35%
		TopTowersCount:           5,
		EnablePredictiveLoading:  true,
		EnableGeographicClustering: true,
		ClusterRadius:            1000.0, // 1km
		PredictiveLoadThreshold:  0.7,    // 70% confidence
	}
}

// NewIntelligentCellCache creates a new intelligent cache with configuration
func NewIntelligentCellCache(config *IntelligentCellCacheConfig, logger *logx.Logger) *IntelligentCellCache {
	if config == nil {
		config = DefaultIntelligentCellCacheConfig()
	}

	return &IntelligentCellCache{
		logger:                    logger,
		maxCacheAge:               config.MaxCacheAge,
		debounceDelay:             config.DebounceDelay,
		towerChangeThreshold:      config.TowerChangeThreshold,
		topTowersCount:            config.TopTowersCount,
		enablePredictiveLoading:   config.EnablePredictiveLoading,
		enableGeographicClustering: config.EnableGeographicClustering,
		clusterRadius:             config.ClusterRadius,
		predictiveLoadThreshold:   config.PredictiveLoadThreshold,
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
			"percentage", changePercentage,
			"threshold", cache.towerChangeThreshold)
		cache.debounceTimer = now
		return true, fmt.Sprintf("tower_change_%.1f%%", changePercentage*100)
	}

	// Check if ≥2 of the top-5 strongest have changed
	topChanges := cache.countTopTowerChanges(currentEnv)
	if topChanges >= 2 {
		cache.logger.Info("Intelligent cache: top towers changed",
			"changes", topChanges,
			"threshold", 2)
		cache.debounceTimer = now
		return true, fmt.Sprintf("top_%d_towers_changed_%d", cache.topTowersCount, topChanges)
	}

	// Geographic clustering check
	if cache.enableGeographicClustering && cache.lastLocationResult != nil {
		if cache.shouldQueryForGeographicReason(currentEnv) {
			cache.logger.Info("Intelligent cache: geographic clustering triggered")
			cache.debounceTimer = now
			return true, "geographic_clustering"
		}
	}

	// Fallback: check if cache has expired (1 hour)
	if now.Sub(cache.lastLocationQuery) >= cache.maxCacheAge {
		cache.logger.Info("Intelligent cache: cache expired")
		return true, "cache_expired"
	}

	cache.logger.Debug("Intelligent cache: using cached location")
	return false, "using_cache"
}

// shouldQueryForGeographicReason checks if we should query based on geographic clustering
func (cache *IntelligentCellCache) shouldQueryForGeographicReason(currentEnv *CellEnvironment) bool {
	if cache.lastLocationResult == nil {
		return false
	}

	// Generate hash for current environment
	currentHash := cache.generateEnvironmentHash(currentEnv)
	
	// If environment hash is significantly different, consider geographic clustering
	if currentHash != cache.lastEnvironment.LocationHash {
		// Calculate distance-based clustering
		// This is a simplified version - in practice, you'd use actual GPS coordinates
		// For now, we'll use the hash difference as a proxy for geographic distance
		hashSimilarity := cache.calculateHashSimilarity(currentHash, cache.lastEnvironment.LocationHash)
		
		if hashSimilarity < 0.5 { // Less than 50% similarity
			return true
		}
	}

	return false
}

// calculateHashSimilarity calculates similarity between two environment hashes
func (cache *IntelligentCellCache) calculateHashSimilarity(hash1, hash2 string) float64 {
	if len(hash1) != len(hash2) {
		return 0.0
	}

	similarChars := 0
	for i := 0; i < len(hash1); i++ {
		if hash1[i] == hash2[i] {
			similarChars++
		}
	}

	return float64(similarChars) / float64(len(hash1))
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
	// Generate location hash for the environment
	env.LocationHash = cache.generateEnvironmentHash(env)
	
	cache.lastEnvironment = env
	cache.lastLocationResult = location
	cache.lastLocationQuery = time.Now()

	cache.logger.Debug("Intelligent cache: updated",
		"location_hash", env.LocationHash,
		"latitude", location.Latitude,
		"longitude", location.Longitude,
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

	// Create hash using SHA256 for better collision resistance
	data := strings.Join(parts, "|")
	hash := sha256.Sum256([]byte(data))
	return fmt.Sprintf("%x", hash)
}

// GetCacheStatus returns detailed cache status information
func (cache *IntelligentCellCache) GetCacheStatus(currentEnv *CellEnvironment) map[string]interface{} {
	status := map[string]interface{}{
		"has_previous_data": cache.lastEnvironment != nil,
		"cache_age":         time.Since(cache.lastLocationQuery).String(),
		"max_cache_age":     cache.maxCacheAge.String(),
		"debounce_delay":    cache.debounceDelay.String(),
		"tower_threshold":   cache.towerChangeThreshold,
		"top_towers_count":  cache.topTowersCount,
	}

	if cache.lastEnvironment != nil {
		status["last_serving_cell"] = cache.lastEnvironment.ServingCell.CellID
		status["last_neighbor_count"] = len(cache.lastEnvironment.NeighborCells)
		status["last_location_hash"] = cache.lastEnvironment.LocationHash
	}

	if currentEnv != nil {
		status["current_serving_cell"] = currentEnv.ServingCell.CellID
		status["current_neighbor_count"] = len(currentEnv.NeighborCells)
		
		// Calculate change metrics
		if cache.lastEnvironment != nil {
			changePercentage := cache.calculateTowerChangePercentage(currentEnv)
			topChanges := cache.countTopTowerChanges(currentEnv)
			
			status["tower_change_percentage"] = changePercentage
			status["top_tower_changes"] = topChanges
			status["serving_cell_changed"] = cache.lastEnvironment.ServingCell.CellID != currentEnv.ServingCell.CellID
		}
	}

	if cache.lastLocationResult != nil {
		status["cached_latitude"] = cache.lastLocationResult.Latitude
		status["cached_longitude"] = cache.lastLocationResult.Longitude
		status["cached_accuracy"] = cache.lastLocationResult.Accuracy
		status["cached_source"] = cache.lastLocationResult.Source
	}

	// Check debounce status
	debounceRemaining := cache.debounceDelay - time.Since(cache.debounceTimer)
	if debounceRemaining > 0 {
		status["debounce_remaining"] = debounceRemaining.String()
		status["debounce_active"] = true
	} else {
		status["debounce_active"] = false
	}

	return status
}

// ShouldPredictiveLoad determines if we should preemptively load location data
func (cache *IntelligentCellCache) ShouldPredictiveLoad(currentEnv *CellEnvironment) bool {
	if !cache.enablePredictiveLoading {
		return false
	}

	if cache.lastEnvironment == nil {
		return false
	}

	// Check if we're approaching a significant change
	changePercentage := cache.calculateTowerChangePercentage(currentEnv)
	if changePercentage > cache.towerChangeThreshold*0.8 { // 80% of threshold
		return true
	}

	// Check if top towers are changing rapidly
	topChanges := cache.countTopTowerChanges(currentEnv)
	if topChanges >= 1 { // At least one top tower changed
		return true
	}

	return false
}

// GetPredictiveLoadConfidence returns confidence score for predictive loading
func (cache *IntelligentCellCache) GetPredictiveLoadConfidence(currentEnv *CellEnvironment) float64 {
	if !cache.enablePredictiveLoading || cache.lastEnvironment == nil {
		return 0.0
	}

	confidence := 0.0

	// Base confidence on tower change percentage
	changePercentage := cache.calculateTowerChangePercentage(currentEnv)
	confidence += changePercentage * 0.5

	// Add confidence for top tower changes
	topChanges := cache.countTopTowerChanges(currentEnv)
	confidence += float64(topChanges) * 0.2

	// Add confidence for serving cell change
	if cache.lastEnvironment.ServingCell.CellID != currentEnv.ServingCell.CellID {
		confidence += 0.3
	}

	return confidence
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
		PCI:    intel.ServingCell.PCI,
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

	return env, nil
}

// ClearCache clears the cache
func (cache *IntelligentCellCache) ClearCache() {
	cache.lastEnvironment = nil
	cache.lastLocationResult = nil
	cache.lastLocationQuery = time.Time{}
	cache.debounceTimer = time.Time{}
	
	cache.logger.Info("Intelligent cache: cleared")
}

// GetCacheMetrics returns cache performance metrics
func (cache *IntelligentCellCache) GetCacheMetrics() map[string]interface{} {
	metrics := map[string]interface{}{
		"cache_hits":           0, // Would need to track this
		"cache_misses":         0, // Would need to track this
		"predictive_loads":     0, // Would need to track this
		"geographic_clusters":  0, // Would need to track this
		"average_cache_age":    time.Since(cache.lastLocationQuery).String(),
		"cache_efficiency":     0.0, // Would need to calculate this
	}

	return metrics
}
