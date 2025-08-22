package main

import (
	"crypto/md5"
	"fmt"
	"sort"
	"strings"
	"time"
)

// CellTowerInfo represents a single cell tower with signal strength
type CellTowerInfo struct {
	CellID string
	RSRP   int    // Signal strength
	RSRQ   int    // Signal quality
	EARFCN int    // Frequency
	PCI    int    // Physical Cell ID
	Type   string // "intra" or "inter"
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
	LastEnvironment    *CellEnvironment
	LastLocationQuery  time.Time
	LastLocationResult *CellTowerLocation
	DebounceTimer      time.Time

	// Configuration
	MaxCacheAge          time.Duration // Fallback cache time (e.g., 1 hour)
	DebounceDelay        time.Duration // Debounce delay (e.g., 10 seconds)
	TowerChangeThreshold float64       // Percentage threshold for tower changes (e.g., 0.35 = 35%)
	TopTowersCount       int           // Number of top towers to monitor (e.g., 5)
}

// NewIntelligentCellCache creates a new intelligent cache with default settings
func NewIntelligentCellCache() *IntelligentCellCache {
	return &IntelligentCellCache{
		MaxCacheAge:          1 * time.Hour,
		DebounceDelay:        10 * time.Second,
		TowerChangeThreshold: 0.35, // 35%
		TopTowersCount:       5,
	}
}

// ShouldQueryLocation determines if we should query OpenCellID for a new location
func (cache *IntelligentCellCache) ShouldQueryLocation(currentEnv *CellEnvironment) (bool, string) {
	now := time.Now()

	// Always query if we have no previous data
	if cache.LastEnvironment == nil || cache.LastLocationResult == nil {
		return true, "no_previous_data"
	}

	// Check if we're still in debounce period
	if now.Sub(cache.DebounceTimer) < cache.DebounceDelay {
		return false, "debounce_active"
	}

	// Check if serving cell has changed
	if cache.LastEnvironment.ServingCell.CellID != currentEnv.ServingCell.CellID {
		cache.DebounceTimer = now
		return true, "serving_cell_changed"
	}

	// Check if ≥35% of towers differ from last fix
	changePercentage := cache.calculateTowerChangePercentage(currentEnv)
	if changePercentage >= cache.TowerChangeThreshold {
		cache.DebounceTimer = now
		return true, fmt.Sprintf("tower_change_%.1f%%", changePercentage*100)
	}

	// Check if ≥2 of the top-5 strongest have changed
	topChanges := cache.countTopTowerChanges(currentEnv)
	if topChanges >= 2 {
		cache.DebounceTimer = now
		return true, fmt.Sprintf("top_%d_towers_changed_%d", cache.TopTowersCount, topChanges)
	}

	// Fallback: check if cache has expired (1 hour)
	if now.Sub(cache.LastLocationQuery) >= cache.MaxCacheAge {
		return true, "cache_expired"
	}

	return false, "using_cache"
}

// calculateTowerChangePercentage calculates what percentage of towers have changed
func (cache *IntelligentCellCache) calculateTowerChangePercentage(currentEnv *CellEnvironment) float64 {
	if cache.LastEnvironment == nil {
		return 1.0 // 100% change if no previous data
	}

	// Create maps for easy lookup
	lastTowers := make(map[string]bool)
	for _, tower := range cache.LastEnvironment.NeighborCells {
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
	if cache.LastEnvironment == nil {
		return cache.TopTowersCount // All are "changed" if no previous data
	}

	// Get top N towers from last environment (sorted by RSRP - higher is better)
	lastTopTowers := cache.getTopTowers(cache.LastEnvironment.NeighborCells, cache.TopTowersCount)
	currentTopTowers := cache.getTopTowers(currentEnv.NeighborCells, cache.TopTowersCount)

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
	cache.LastEnvironment = env
	cache.LastLocationResult = location
	cache.LastLocationQuery = time.Now()
}

// GetCachedLocation returns the cached location if available
func (cache *IntelligentCellCache) GetCachedLocation() *CellTowerLocation {
	return cache.LastLocationResult
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
		RSRP:   intel.SignalQuality.RSRP,
		RSRQ:   intel.SignalQuality.RSRQ,
		EARFCN: intel.ServingCell.EARFCN,
		PCI:    intel.ServingCell.PCID,
		Type:   "serving",
	}

	// Parse neighbor cells
	for _, neighbor := range intel.NeighborCells {
		tower := CellTowerInfo{
			CellID: fmt.Sprintf("neighbor_%d", neighbor.PCID), // Use PCID as identifier
			RSRP:   neighbor.RSRP,
			RSRQ:   neighbor.RSRQ,
			EARFCN: neighbor.EARFCN,
			PCI:    neighbor.PCID,
			Type:   neighbor.CellType,
		}
		env.NeighborCells = append(env.NeighborCells, tower)
	}

	return env, nil
}

// PrintCacheStatus prints detailed information about the cache status
func (cache *IntelligentCellCache) PrintCacheStatus(currentEnv *CellEnvironment) {
	fmt.Println("\n=== Intelligent Cell Cache Status ===")

	if cache.LastEnvironment == nil {
		fmt.Println("Status: No previous environment data")
		return
	}

	now := time.Now()

	fmt.Printf("Last query: %s ago\n", now.Sub(cache.LastLocationQuery).Round(time.Second))
	fmt.Printf("Cache max age: %s\n", cache.MaxCacheAge)
	fmt.Printf("Debounce delay: %s\n", cache.DebounceDelay)
	fmt.Printf("Tower change threshold: %.1f%%\n", cache.TowerChangeThreshold*100)
	fmt.Printf("Top towers monitored: %d\n", cache.TopTowersCount)

	// Check serving cell change
	servingChanged := cache.LastEnvironment.ServingCell.CellID != currentEnv.ServingCell.CellID
	fmt.Printf("Serving cell changed: %v", servingChanged)
	if servingChanged {
		fmt.Printf(" (%s → %s)", cache.LastEnvironment.ServingCell.CellID, currentEnv.ServingCell.CellID)
	}
	fmt.Println()

	// Check tower change percentage
	changePercentage := cache.calculateTowerChangePercentage(currentEnv)
	fmt.Printf("Tower change percentage: %.1f%% (threshold: %.1f%%)\n",
		changePercentage*100, cache.TowerChangeThreshold*100)

	// Check top tower changes
	topChanges := cache.countTopTowerChanges(currentEnv)
	fmt.Printf("Top %d tower changes: %d (threshold: ≥2)\n", cache.TopTowersCount, topChanges)

	// Check debounce status
	debounceRemaining := cache.DebounceDelay - now.Sub(cache.DebounceTimer)
	if debounceRemaining > 0 {
		fmt.Printf("Debounce remaining: %s\n", debounceRemaining.Round(time.Second))
	} else {
		fmt.Println("Debounce: inactive")
	}

	// Overall decision
	shouldQuery, reason := cache.ShouldQueryLocation(currentEnv)
	fmt.Printf("Should query: %v (%s)\n", shouldQuery, reason)

	if cache.LastLocationResult != nil {
		fmt.Printf("Cached location: %.6f, %.6f (±%.0fm)\n",
			cache.LastLocationResult.Latitude,
			cache.LastLocationResult.Longitude,
			cache.LastLocationResult.Accuracy)
	}
}

// Example usage function
func testIntelligentCellCache() error {
	fmt.Println("=== Testing Intelligent Cell Cache ===")

	cache := NewIntelligentCellCache()

	// Simulate first environment
	env1 := &CellEnvironment{
		ServingCell: CellTowerInfo{CellID: "25939743", RSRP: -84, RSRQ: -8, PCI: 443},
		NeighborCells: []CellTowerInfo{
			{CellID: "25939744", RSRP: -90, RSRQ: -15, PCI: 263, Type: "intra"},
			{CellID: "25939745", RSRP: -102, RSRQ: -20, PCI: 60, Type: "intra"},
			{CellID: "25939746", RSRP: -95, RSRQ: -18, PCI: 100, Type: "inter"},
		},
		Timestamp: time.Now(),
	}

	// First query - should always query
	shouldQuery, reason := cache.ShouldQueryLocation(env1)
	fmt.Printf("First query: %v (%s)\n", shouldQuery, reason)

	// Simulate location result
	location1 := &CellTowerLocation{
		Latitude:  59.48007,
		Longitude: 18.27985,
		Accuracy:  500,
		Source:    "opencellid_area_search",
	}
	cache.UpdateCache(env1, location1)

	// Same environment - should use cache
	shouldQuery, reason = cache.ShouldQueryLocation(env1)
	fmt.Printf("Same environment: %v (%s)\n", shouldQuery, reason)

	// Simulate serving cell change
	env2 := &CellEnvironment{
		ServingCell:   CellTowerInfo{CellID: "25939999", RSRP: -85, RSRQ: -9, PCI: 500}, // Different cell
		NeighborCells: env1.NeighborCells,                                               // Same neighbors
		Timestamp:     time.Now(),
	}

	shouldQuery, reason = cache.ShouldQueryLocation(env2)
	fmt.Printf("Serving cell changed: %v (%s)\n", shouldQuery, reason)

	// Test debounce - immediate second query should be blocked
	shouldQuery, reason = cache.ShouldQueryLocation(env2)
	fmt.Printf("Immediate retry (debounce): %v (%s)\n", shouldQuery, reason)

	// Simulate significant neighbor change (>35% towers different)
	env3 := &CellEnvironment{
		ServingCell: env1.ServingCell, // Same serving cell
		NeighborCells: []CellTowerInfo{
			{CellID: "25939744", RSRP: -90, RSRQ: -15, PCI: 263, Type: "intra"}, // Same
			{CellID: "99999999", RSRP: -88, RSRQ: -12, PCI: 200, Type: "intra"}, // New
			{CellID: "88888888", RSRP: -92, RSRQ: -16, PCI: 300, Type: "inter"}, // New
			{CellID: "77777777", RSRP: -94, RSRQ: -18, PCI: 400, Type: "inter"}, // New
		},
		Timestamp: time.Now(),
	}

	// Wait for debounce to clear
	time.Sleep(1 * time.Second)
	cache.DebounceTimer = time.Now().Add(-15 * time.Second) // Simulate debounce cleared

	shouldQuery, reason = cache.ShouldQueryLocation(env3)
	fmt.Printf("Major neighbor change: %v (%s)\n", shouldQuery, reason)

	cache.PrintCacheStatus(env3)

	return nil
}
