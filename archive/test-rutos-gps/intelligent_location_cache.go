package main

import (
	"context"
	"fmt"
	"sync"
	"time"

	"golang.org/x/crypto/ssh"
	"googlemaps.github.io/maps"
)

// IntelligentLocationCache manages location data with smart invalidation
type IntelligentLocationCache struct {
	// Cache entries
	wifiCache     *SmartCacheEntry
	combinedCache *SmartCacheEntry
	cellularCache *SmartCacheEntry

	// Cellular environment tracking
	lastCellEnvironment *CellEnvironmentSnapshot

	// Configuration
	config *IntelligentCacheConfig

	// Thread safety
	mu sync.RWMutex
}

// SmartCacheEntry represents a cached location with environment context
type SmartCacheEntry struct {
	Result            *LocationResult
	CachedAt          time.Time
	ExpiresAt         time.Time
	CellEnvironment   *CellEnvironmentSnapshot
	InvalidationCount int
}

// CellEnvironmentSnapshot captures the cellular environment at cache time
type CellEnvironmentSnapshot struct {
	ServingCellID   string
	ServingCellRSSI int
	TopCells        []CellTowerSnapshot
	CapturedAt      time.Time
}

// CellTowerSnapshot represents a cell tower at a point in time
type CellTowerSnapshot struct {
	CellID int
	RSSI   int
	RSRP   int
	PCID   int
}

// IntelligentCacheConfig holds configuration for smart caching
type IntelligentCacheConfig struct {
	// Time-based limits
	MinCacheTime time.Duration // Never query more often than this
	MaxCacheTime time.Duration // Always expire after this

	// Change detection thresholds
	TopCellsToTrack     int     // Number of top cells to monitor
	CellChangeThreshold float64 // Percentage change to trigger invalidation
	ServingCellRequired bool    // Require serving cell change for invalidation

	// Debouncing
	DebounceTime time.Duration // Wait time after detecting changes

	// API quota management
	MonthlyQuotaLimit int // 10,000 free requests per month
	DailyQuotaLimit   int // Calculated daily limit
}

// NewIntelligentLocationCache creates a new intelligent cache
func NewIntelligentLocationCache() *IntelligentLocationCache {
	config := &IntelligentCacheConfig{
		MinCacheTime:        5 * time.Minute,  // Never query more often than 5 minutes
		MaxCacheTime:        10 * time.Minute, // Always expire after 10 minutes
		TopCellsToTrack:     5,                // Monitor top 5 cells
		CellChangeThreshold: 0.33,             // 33% change threshold
		ServingCellRequired: false,            // Don't require serving cell change
		DebounceTime:        10 * time.Second, // 10 second debounce
		MonthlyQuotaLimit:   10000,            // 10k free requests/month
		DailyQuotaLimit:     333,              // ~10k/30 days
	}

	return &IntelligentLocationCache{
		config: config,
	}
}

// ShouldInvalidateCache determines if cache should be invalidated based on cellular changes
func (ilc *IntelligentLocationCache) ShouldInvalidateCache(client *ssh.Client, cacheEntry *SmartCacheEntry) (bool, string) {
	ilc.mu.RLock()
	defer ilc.mu.RUnlock()

	if cacheEntry == nil {
		return true, "no cache entry"
	}

	now := time.Now()

	// 1. Check minimum cache time (never query more often than 5 minutes)
	if now.Sub(cacheEntry.CachedAt) < ilc.config.MinCacheTime {
		remaining := ilc.config.MinCacheTime - now.Sub(cacheEntry.CachedAt)
		return false, fmt.Sprintf("within min cache time (%.1f min remaining)", remaining.Minutes())
	}

	// 2. Check maximum cache time (always expire after 10 minutes)
	if now.After(cacheEntry.ExpiresAt) {
		return true, "max cache time exceeded"
	}

	// 3. Get current cellular environment
	currentEnv, err := ilc.captureCellEnvironment(client)
	if err != nil {
		fmt.Printf("    ⚠️  Failed to capture cell environment: %v\n", err)
		return false, "failed to get current cell environment"
	}

	// 4. Compare with cached environment
	if cacheEntry.CellEnvironment == nil {
		return true, "no cached cell environment"
	}

	// 5. Check for significant changes
	changeReason := ilc.detectSignificantChanges(cacheEntry.CellEnvironment, currentEnv)
	if changeReason != "" {
		return true, changeReason
	}

	return false, "no significant changes detected"
}

// captureCellEnvironment captures the current cellular environment
func (ilc *IntelligentLocationCache) captureCellEnvironment(client *ssh.Client) (*CellEnvironmentSnapshot, error) {
	// Get cellular intelligence data
	intel, err := collectCellularLocationIntelligence(client)
	if err != nil {
		return nil, fmt.Errorf("failed to get cellular intelligence: %w", err)
	}

	// Create snapshot
	snapshot := &CellEnvironmentSnapshot{
		ServingCellID:   intel.ServingCell.CellID,
		ServingCellRSSI: intel.SignalQuality.RSSI,
		CapturedAt:      time.Now(),
	}

	// Capture top N cells by signal strength
	topCells := make([]CellTowerSnapshot, 0, ilc.config.TopCellsToTrack)

	// Add serving cell first
	if intel.ServingCell.CellID != "" {
		topCells = append(topCells, CellTowerSnapshot{
			CellID: parseIntSafe(intel.ServingCell.CellID),
			RSSI:   intel.SignalQuality.RSSI,
			RSRP:   intel.SignalQuality.RSRP,
			PCID:   intel.ServingCell.PCID,
		})
	}

	// Add top neighbor cells
	for i, neighbor := range intel.NeighborCells {
		if len(topCells) >= ilc.config.TopCellsToTrack {
			break
		}
		if i < ilc.config.TopCellsToTrack-1 { // Reserve one slot for serving cell
			topCells = append(topCells, CellTowerSnapshot{
				CellID: neighbor.PCID, // Use PCID as identifier
				RSSI:   neighbor.RSSI,
				RSRP:   neighbor.RSRP,
				PCID:   neighbor.PCID,
			})
		}
	}

	snapshot.TopCells = topCells
	return snapshot, nil
}

// detectSignificantChanges compares two cell environments for significant changes
func (ilc *IntelligentLocationCache) detectSignificantChanges(cached, current *CellEnvironmentSnapshot) string {
	// 1. Check serving cell change
	if cached.ServingCellID != current.ServingCellID {
		return fmt.Sprintf("serving cell changed: %s → %s", cached.ServingCellID, current.ServingCellID)
	}

	// 2. Check if we have enough data to compare
	if len(cached.TopCells) == 0 || len(current.TopCells) == 0 {
		return "insufficient cell data for comparison"
	}

	// 3. Create maps for easier comparison
	cachedCells := make(map[int]CellTowerSnapshot)
	for _, cell := range cached.TopCells {
		cachedCells[cell.CellID] = cell
	}

	currentCells := make(map[int]CellTowerSnapshot)
	for _, cell := range current.TopCells {
		currentCells[cell.CellID] = cell
	}

	// 4. Calculate percentage of cells that have changed
	totalCells := len(cached.TopCells)
	changedCells := 0

	for cellID := range cachedCells {
		if _, exists := currentCells[cellID]; !exists {
			changedCells++
		}
	}

	for cellID := range currentCells {
		if _, exists := cachedCells[cellID]; !exists {
			changedCells++
		}
	}

	changePercentage := float64(changedCells) / float64(totalCells)

	// 5. Check if change exceeds threshold
	if changePercentage >= ilc.config.CellChangeThreshold {
		return fmt.Sprintf("%.1f%% of top cells changed (threshold: %.1f%%)",
			changePercentage*100, ilc.config.CellChangeThreshold*100)
	}

	// 6. Check for significant signal strength changes in common cells
	significantSignalChanges := 0
	for cellID, cachedCell := range cachedCells {
		if currentCell, exists := currentCells[cellID]; exists {
			rssiDiff := absInt(currentCell.RSSI - cachedCell.RSSI)
			if rssiDiff > 10 { // More than 10 dBm change
				significantSignalChanges++
			}
		}
	}

	if significantSignalChanges >= 2 {
		return fmt.Sprintf("%d cells have significant signal changes (>10 dBm)", significantSignalChanges)
	}

	return "" // No significant changes
}

// GetCachedWiFiLocation gets cached WiFi location with intelligent invalidation
func (ilc *IntelligentLocationCache) GetCachedWiFiLocation(client *ssh.Client) *LocationResult {
	ilc.mu.RLock()
	defer ilc.mu.RUnlock()

	if ilc.wifiCache == nil {
		return nil
	}

	shouldInvalidate, reason := ilc.ShouldInvalidateCache(client, ilc.wifiCache)
	if shouldInvalidate {
		fmt.Printf("    🔄 Cache invalidated: %s\n", reason)
		return nil
	}

	fmt.Printf("    💾 Using cached WiFi location (%s)\n", reason)
	result := *ilc.wifiCache.Result // Copy the result
	result.Cached = true
	return &result
}

// CacheWiFiLocation caches WiFi location with cellular environment context
func (ilc *IntelligentLocationCache) CacheWiFiLocation(client *ssh.Client, result *LocationResult) {
	ilc.mu.Lock()
	defer ilc.mu.Unlock()

	// Capture current cellular environment
	cellEnv, err := ilc.captureCellEnvironment(client)
	if err != nil {
		fmt.Printf("    ⚠️  Failed to capture cell environment for cache: %v\n", err)
		cellEnv = nil // Cache without environment context
	}

	now := time.Now()
	ilc.wifiCache = &SmartCacheEntry{
		Result:          result,
		CachedAt:        now,
		ExpiresAt:       now.Add(ilc.config.MaxCacheTime),
		CellEnvironment: cellEnv,
	}

	fmt.Printf("    💾 Cached WiFi location for %.1f minutes (with cell environment)\n",
		ilc.config.MaxCacheTime.Minutes())
}

// Similar methods for combined and cellular caches...
func (ilc *IntelligentLocationCache) GetCachedCombinedLocation(client *ssh.Client) *LocationResult {
	ilc.mu.RLock()
	defer ilc.mu.RUnlock()

	if ilc.combinedCache == nil {
		return nil
	}

	shouldInvalidate, reason := ilc.ShouldInvalidateCache(client, ilc.combinedCache)
	if shouldInvalidate {
		fmt.Printf("    🔄 Cache invalidated: %s\n", reason)
		return nil
	}

	fmt.Printf("    💾 Using cached combined location (%s)\n", reason)
	result := *ilc.combinedCache.Result
	result.Cached = true
	return &result
}

func (ilc *IntelligentLocationCache) CacheCombinedLocation(client *ssh.Client, result *LocationResult) {
	ilc.mu.Lock()
	defer ilc.mu.Unlock()

	cellEnv, err := ilc.captureCellEnvironment(client)
	if err != nil {
		fmt.Printf("    ⚠️  Failed to capture cell environment for cache: %v\n", err)
		cellEnv = nil
	}

	now := time.Now()
	ilc.combinedCache = &SmartCacheEntry{
		Result:          result,
		CachedAt:        now,
		ExpiresAt:       now.Add(ilc.config.MaxCacheTime),
		CellEnvironment: cellEnv,
	}

	fmt.Printf("    💾 Cached combined location for %.1f minutes (with cell environment)\n",
		ilc.config.MaxCacheTime.Minutes())
}

// PrintCacheStats displays cache statistics and status
func (ilc *IntelligentLocationCache) PrintCacheStats() {
	ilc.mu.RLock()
	defer ilc.mu.RUnlock()

	fmt.Println("\n📊 Intelligent Cache Statistics:")
	fmt.Println("=================================")

	now := time.Now()

	if ilc.wifiCache != nil {
		age := now.Sub(ilc.wifiCache.CachedAt)
		remaining := ilc.wifiCache.ExpiresAt.Sub(now)
		fmt.Printf("🚀 WiFi Cache: %.1fm old, %.1fm remaining\n", age.Minutes(), remaining.Minutes())
		if ilc.wifiCache.CellEnvironment != nil {
			fmt.Printf("   📡 Serving Cell: %s (%d dBm)\n",
				ilc.wifiCache.CellEnvironment.ServingCellID,
				ilc.wifiCache.CellEnvironment.ServingCellRSSI)
			fmt.Printf("   🗼 Top Cells: %d tracked\n", len(ilc.wifiCache.CellEnvironment.TopCells))
		}
	} else {
		fmt.Println("🚀 WiFi Cache: Empty")
	}

	if ilc.combinedCache != nil {
		age := now.Sub(ilc.combinedCache.CachedAt)
		remaining := ilc.combinedCache.ExpiresAt.Sub(now)
		fmt.Printf("📡 Combined Cache: %.1fm old, %.1fm remaining\n", age.Minutes(), remaining.Minutes())
	} else {
		fmt.Println("📡 Combined Cache: Empty")
	}

	fmt.Printf("⚙️  Config: Min=%.1fm, Max=%.1fm, Threshold=%.0f%%, TopCells=%d\n",
		ilc.config.MinCacheTime.Minutes(),
		ilc.config.MaxCacheTime.Minutes(),
		ilc.config.CellChangeThreshold*100,
		ilc.config.TopCellsToTrack)
}

// Helper functions
func parseIntSafe(s string) int {
	if val := parseInt(s); val != 0 {
		return val
	}
	return 0
}

func absInt(x int) int {
	if x < 0 {
		return -x
	}
	return x
}

// testIntelligentLocationCache demonstrates the intelligent caching system
func testIntelligentLocationCache() {
	fmt.Println("🧠 Intelligent Location Cache Test")
	fmt.Println("==================================")

	// Create SSH client
	client, err := createSSHClient()
	if err != nil {
		fmt.Printf("❌ SSH connection failed: %v\n", err)
		return
	}
	defer client.Close()

	// Create intelligent cache
	cache := NewIntelligentLocationCache()

	// Load Google API key
	apiKey, err := LoadGoogleAPIKey()
	if err != nil {
		fmt.Printf("❌ Failed to load Google API key: %v\n", err)
		return
	}

	// Create Google client
	googleClient, err := maps.NewClient(maps.WithAPIKey(apiKey))
	if err != nil {
		fmt.Printf("❌ Failed to create Google client: %v\n", err)
		return
	}

	fmt.Println("\n🔄 Testing intelligent cache behavior (5 requests over time)...")

	for i := 1; i <= 5; i++ {
		fmt.Printf("\n--- Request %d ---\n", i)

		// Check cache first
		if cached := cache.GetCachedWiFiLocation(client); cached != nil {
			fmt.Printf("✅ Cache hit: %.6f°, %.6f° (±%.0fm)\n",
				cached.Location.Lat, cached.Location.Lng, cached.Accuracy)
			fmt.Printf("📡 Source: %s (cached)\n", cached.Source)
		} else {
			fmt.Println("🔍 Cache miss - performing fresh WiFi scan...")

			// Perform enhanced WiFi scan
			scanResult, err := performEnhancedUbusWiFiScan(client)
			if err != nil {
				fmt.Printf("❌ WiFi scan failed: %v\n", err)
				continue
			}

			if len(scanResult.GoogleWiFiAPs) < 2 {
				fmt.Printf("❌ Insufficient WiFi APs: %d\n", len(scanResult.GoogleWiFiAPs))
				continue
			}

			// Make Google API call
			req := &maps.GeolocationRequest{
				WiFiAccessPoints: scanResult.GoogleWiFiAPs,
				ConsiderIP:       false,
			}

			ctx := context.Background()
			resp, err := googleClient.Geolocate(ctx, req)
			if err != nil {
				fmt.Printf("❌ Google API failed: %v\n", err)
				continue
			}

			// Create result
			result := &LocationResult{
				Location:  &resp.Location,
				Accuracy:  resp.Accuracy,
				Source:    fmt.Sprintf("Enhanced WiFi (%d APs)", len(scanResult.GoogleWiFiAPs)),
				Timestamp: time.Now(),
				Cost:      0.005,
				Quality:   "Good",
			}

			fmt.Printf("✅ Fresh location: %.6f°, %.6f° (±%.0fm)\n",
				result.Location.Lat, result.Location.Lng, result.Accuracy)

			// Cache the result
			cache.CacheWiFiLocation(client, result)
		}

		// Display cache stats
		cache.PrintCacheStats()

		// Wait between requests (except last one)
		if i < 5 {
			waitTime := 2 * time.Minute
			if i == 3 {
				waitTime = 6 * time.Minute // Test max cache expiration
			}
			fmt.Printf("⏳ Waiting %.1f minutes...\n", waitTime.Minutes())
			time.Sleep(waitTime)
		}
	}

	fmt.Println("\n🎯 Intelligent cache test completed!")
	fmt.Println("Key benefits demonstrated:")
	fmt.Println("  ✅ 10-minute max cache (API quota conservation)")
	fmt.Println("  ✅ 5-minute min cache (prevents excessive queries)")
	fmt.Println("  ✅ Cell environment tracking (smart invalidation)")
	fmt.Println("  ✅ 33% change threshold (movement detection)")
	fmt.Println("  ✅ Stable location when stationary")
}
