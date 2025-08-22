package main

import (
	"context"
	"fmt"
	"sync"
	"time"

	"golang.org/x/crypto/ssh"
	"googlemaps.github.io/maps"
)

// LocationManager implements the intelligent location strategy
type LocationManager struct {
	config           *LocationConfig
	sshClient        *ssh.Client
	googleClient     *maps.Client
	cache            *LocationCache
	intelligentCache *IntelligentLocationCache
	stats            *LocationStats
	mu               sync.RWMutex
}

// LocationConfig holds configuration for the location manager
type LocationConfig struct {
	// GPS settings
	GPSTimeout time.Duration
	GPSRetries int

	// WiFi settings
	WiFiTimeout       time.Duration
	WiFiCacheDuration time.Duration
	WiFiMinAPs        int

	// Cellular settings
	CellTimeout       time.Duration
	CellCacheDuration time.Duration

	// API settings
	GoogleAPIKey    string
	DailyQuotaLimit int
	BurstLimit      int

	// Daemon settings
	UpdateInterval  time.Duration
	MonitorInterval time.Duration
}

// LocationResult represents a location with metadata
type LocationResult struct {
	Location     *maps.LatLng  `json:"location"`
	Accuracy     float64       `json:"accuracy"`
	Source       string        `json:"source"`
	Timestamp    time.Time     `json:"timestamp"`
	ResponseTime time.Duration `json:"response_time"`
	Cached       bool          `json:"cached"`
	Cost         float64       `json:"cost"`
	Quality      string        `json:"quality"`
}

// LocationCache manages cached location data
type LocationCache struct {
	gpsCache      *CacheEntry
	wifiCache     *CacheEntry
	cellularCache *CacheEntry
	combinedCache *CacheEntry
	mu            sync.RWMutex
}

// CacheEntry represents a cached location
type CacheEntry struct {
	Result    *LocationResult
	ExpiresAt time.Time
}

// LocationStats tracks performance metrics
type LocationStats struct {
	GPSAttempts       int64
	GPSSuccesses      int64
	WiFiAttempts      int64
	WiFiSuccesses     int64
	CellularAttempts  int64
	CellularSuccesses int64
	APICallsToday     int64
	TotalCost         float64
	mu                sync.RWMutex
}

// NewLocationManager creates a new location manager
func NewLocationManager(config *LocationConfig) (*LocationManager, error) {
	// Create SSH client
	sshClient, err := createSSHClient()
	if err != nil {
		return nil, fmt.Errorf("failed to create SSH client: %w", err)
	}

	// Create Google Maps client
	googleClient, err := maps.NewClient(maps.WithAPIKey(config.GoogleAPIKey))
	if err != nil {
		return nil, fmt.Errorf("failed to create Google client: %w", err)
	}

	return &LocationManager{
		config:           config,
		sshClient:        sshClient,
		googleClient:     googleClient,
		cache:            &LocationCache{},
		intelligentCache: NewIntelligentLocationCache(),
		stats:            &LocationStats{},
	}, nil
}

// GetBestLocation implements the intelligent location hierarchy
func (lm *LocationManager) GetBestLocation(ctx context.Context) (*LocationResult, error) {
	startTime := time.Now()

	fmt.Println("🎯 Getting best location using intelligent hierarchy...")

	// 1. Always try GPS first (±2m accuracy, free, reliable)
	if gpsResult := lm.tryGPS(ctx); gpsResult != nil {
		gpsResult.ResponseTime = time.Since(startTime)
		fmt.Printf("✅ GPS success: %.6f°, %.6f° (±%.0fm) in %v\n",
			gpsResult.Location.Lat, gpsResult.Location.Lng,
			gpsResult.Accuracy, gpsResult.ResponseTime)
		return gpsResult, nil
	}

	// 2. Try enhanced WiFi fallback (±41m accuracy, rich data)
	if wifiResult := lm.tryEnhancedWiFi(ctx); wifiResult != nil {
		wifiResult.ResponseTime = time.Since(startTime)
		fmt.Printf("✅ Enhanced WiFi success: %.6f°, %.6f° (±%.0fm) in %v\n",
			wifiResult.Location.Lat, wifiResult.Location.Lng,
			wifiResult.Accuracy, wifiResult.ResponseTime)
		return wifiResult, nil
	}

	// 3. Try combined cellular + WiFi (±69m accuracy, comprehensive)
	if combinedResult := lm.tryCombined(ctx); combinedResult != nil {
		combinedResult.ResponseTime = time.Since(startTime)
		fmt.Printf("✅ Combined success: %.6f°, %.6f° (±%.0fm) in %v\n",
			combinedResult.Location.Lat, combinedResult.Location.Lng,
			combinedResult.Accuracy, combinedResult.ResponseTime)
		return combinedResult, nil
	}

	// 4. Last resort: cellular-only (±1334m accuracy, wide coverage)
	if cellularResult := lm.tryCellularOnly(ctx); cellularResult != nil {
		cellularResult.ResponseTime = time.Since(startTime)
		fmt.Printf("✅ Cellular-only success: %.6f°, %.6f° (±%.0fm) in %v\n",
			cellularResult.Location.Lat, cellularResult.Location.Lng,
			cellularResult.Accuracy, cellularResult.ResponseTime)
		return cellularResult, nil
	}

	return nil, fmt.Errorf("all location methods failed")
}

// tryGPS attempts to get GPS location (always first priority)
func (lm *LocationManager) tryGPS(ctx context.Context) *LocationResult {
	lm.stats.mu.Lock()
	lm.stats.GPSAttempts++
	lm.stats.mu.Unlock()

	// Check cache first (30 second cache for GPS)
	if cached := lm.getCachedGPS(); cached != nil {
		cached.Cached = true
		return cached
	}

	fmt.Println("  🛰️  Trying GPS (Quectel GNSS)...")

	// Try to get GPS data using existing Quectel method
	output, err := executeCommand(lm.sshClient, "gsmctl -A 'AT+QGPSLOC=2'")
	if err != nil {
		fmt.Printf("    ❌ GPS command failed: %v\n", err)
		return nil
	}

	// Parse Quectel GPS response using existing function
	gpsData := parseQGPSLOC(output)
	if gpsData == nil {
		fmt.Println("    ❌ GPS parsing failed")
		return nil
	}

	if gpsData.Latitude == 0 || gpsData.Longitude == 0 {
		fmt.Println("    ❌ GPS returned invalid coordinates")
		return nil
	}

	lm.stats.mu.Lock()
	lm.stats.GPSSuccesses++
	lm.stats.mu.Unlock()

	// Calculate accuracy from HDOP (Horizontal Dilution of Precision)
	accuracy := gpsData.HDOP * 5.0 // Approximate accuracy in meters
	if accuracy < 2.0 {
		accuracy = 2.0 // Minimum realistic GPS accuracy
	}

	result := &LocationResult{
		Location: &maps.LatLng{
			Lat: gpsData.Latitude,
			Lng: gpsData.Longitude,
		},
		Accuracy:  accuracy,
		Source:    fmt.Sprintf("GPS (Quectel GNSS, %d sats, HDOP %.1f)", gpsData.Satellites, gpsData.HDOP),
		Timestamp: time.Now(),
		Cost:      0.0, // GPS is free
		Quality:   "Excellent",
	}

	// Cache for 30 seconds
	lm.cacheGPS(result, 30*time.Second)
	return result
}

// tryEnhancedWiFi attempts enhanced ubus WiFi scanning
func (lm *LocationManager) tryEnhancedWiFi(ctx context.Context) *LocationResult {
	lm.stats.mu.Lock()
	lm.stats.WiFiAttempts++
	lm.stats.mu.Unlock()

	// Check intelligent cache first (10 minute max, 5 minute min, cell-change invalidation)
	if cached := lm.intelligentCache.GetCachedWiFiLocation(lm.sshClient); cached != nil {
		return cached
	}

	fmt.Println("  🚀 Trying Enhanced WiFi (ubus scan)...")

	// Check API quota
	if !lm.checkAPIQuota() {
		fmt.Println("    ⚠️  API quota exceeded, skipping WiFi")
		return nil
	}

	// Perform enhanced WiFi scan
	scanResult, err := performEnhancedUbusWiFiScan(lm.sshClient)
	if err != nil {
		fmt.Printf("    ❌ WiFi scan failed: %v\n", err)
		return nil
	}

	if len(scanResult.GoogleWiFiAPs) < lm.config.WiFiMinAPs {
		fmt.Printf("    ❌ Insufficient WiFi APs: %d < %d\n",
			len(scanResult.GoogleWiFiAPs), lm.config.WiFiMinAPs)
		return nil
	}

	// Make Google API call
	req := &maps.GeolocationRequest{
		WiFiAccessPoints: scanResult.GoogleWiFiAPs,
		ConsiderIP:       false,
	}

	resp, err := lm.googleClient.Geolocate(ctx, req)
	if err != nil {
		fmt.Printf("    ❌ Google WiFi API failed: %v\n", err)
		return nil
	}

	lm.stats.mu.Lock()
	lm.stats.WiFiSuccesses++
	lm.stats.APICallsToday++
	lm.stats.TotalCost += 0.005 // $0.005 per request
	lm.stats.mu.Unlock()

	result := &LocationResult{
		Location:  &resp.Location,
		Accuracy:  resp.Accuracy,
		Source:    fmt.Sprintf("Enhanced WiFi (%d APs)", len(scanResult.GoogleWiFiAPs)),
		Timestamp: time.Now(),
		Cost:      0.005,
		Quality:   lm.getQualityRating(resp.Accuracy),
	}

	// Cache with intelligent invalidation
	lm.intelligentCache.CacheWiFiLocation(lm.sshClient, result)
	return result
}

// tryCombined attempts combined cellular + WiFi location
func (lm *LocationManager) tryCombined(ctx context.Context) *LocationResult {
	// Check intelligent cache first (10 minute max, 5 minute min, cell-change invalidation)
	if cached := lm.intelligentCache.GetCachedCombinedLocation(lm.sshClient); cached != nil {
		return cached
	}

	fmt.Println("  📡 Trying Combined (Cellular + WiFi)...")

	// Check API quota
	if !lm.checkAPIQuota() {
		fmt.Println("    ⚠️  API quota exceeded, skipping combined")
		return nil
	}

	// This would call the existing testCombinedLocation logic
	// For brevity, returning nil here - implement based on existing code
	fmt.Println("    ⚠️  Combined method not implemented in this example")
	return nil
}

// tryCellularOnly attempts cellular-only location
func (lm *LocationManager) tryCellularOnly(ctx context.Context) *LocationResult {
	lm.stats.mu.Lock()
	lm.stats.CellularAttempts++
	lm.stats.mu.Unlock()

	// Check cache first (5 minute cache for cellular)
	if cached := lm.getCachedCellular(); cached != nil {
		cached.Cached = true
		return cached
	}

	fmt.Println("  📱 Trying Cellular-Only (last resort)...")

	// Check API quota
	if !lm.checkAPIQuota() {
		fmt.Println("    ⚠️  API quota exceeded, skipping cellular")
		return nil
	}

	// This would implement cellular-only location
	fmt.Println("    ⚠️  Cellular-only method not implemented in this example")
	return nil
}

// Cache management methods
func (lm *LocationManager) getCachedGPS() *LocationResult {
	lm.cache.mu.RLock()
	defer lm.cache.mu.RUnlock()

	if lm.cache.gpsCache != nil && time.Now().Before(lm.cache.gpsCache.ExpiresAt) {
		return lm.cache.gpsCache.Result
	}
	return nil
}

func (lm *LocationManager) cacheGPS(result *LocationResult, duration time.Duration) {
	lm.cache.mu.Lock()
	defer lm.cache.mu.Unlock()

	lm.cache.gpsCache = &CacheEntry{
		Result:    result,
		ExpiresAt: time.Now().Add(duration),
	}
}

func (lm *LocationManager) getCachedWiFi() *LocationResult {
	lm.cache.mu.RLock()
	defer lm.cache.mu.RUnlock()

	if lm.cache.wifiCache != nil && time.Now().Before(lm.cache.wifiCache.ExpiresAt) {
		return lm.cache.wifiCache.Result
	}
	return nil
}

func (lm *LocationManager) cacheWiFi(result *LocationResult, duration time.Duration) {
	lm.cache.mu.Lock()
	defer lm.cache.mu.Unlock()

	lm.cache.wifiCache = &CacheEntry{
		Result:    result,
		ExpiresAt: time.Now().Add(duration),
	}
}

func (lm *LocationManager) getCachedCombined() *LocationResult {
	lm.cache.mu.RLock()
	defer lm.cache.mu.RUnlock()

	if lm.cache.combinedCache != nil && time.Now().Before(lm.cache.combinedCache.ExpiresAt) {
		return lm.cache.combinedCache.Result
	}
	return nil
}

func (lm *LocationManager) getCachedCellular() *LocationResult {
	lm.cache.mu.RLock()
	defer lm.cache.mu.RUnlock()

	if lm.cache.cellularCache != nil && time.Now().Before(lm.cache.cellularCache.ExpiresAt) {
		return lm.cache.cellularCache.Result
	}
	return nil
}

// checkAPIQuota checks if we're within API usage limits
func (lm *LocationManager) checkAPIQuota() bool {
	lm.stats.mu.RLock()
	defer lm.stats.mu.RUnlock()

	return lm.stats.APICallsToday < int64(lm.config.DailyQuotaLimit)
}

// getQualityRating converts accuracy to quality rating
func (lm *LocationManager) getQualityRating(accuracy float64) string {
	switch {
	case accuracy <= 10:
		return "Excellent"
	case accuracy <= 50:
		return "Good"
	case accuracy <= 100:
		return "Fair"
	case accuracy <= 500:
		return "Poor"
	default:
		return "Very Poor"
	}
}

// GetStats returns current performance statistics
func (lm *LocationManager) GetStats() *LocationStats {
	lm.stats.mu.RLock()
	defer lm.stats.mu.RUnlock()

	// Return a copy to avoid race conditions
	return &LocationStats{
		GPSAttempts:       lm.stats.GPSAttempts,
		GPSSuccesses:      lm.stats.GPSSuccesses,
		WiFiAttempts:      lm.stats.WiFiAttempts,
		WiFiSuccesses:     lm.stats.WiFiSuccesses,
		CellularAttempts:  lm.stats.CellularAttempts,
		CellularSuccesses: lm.stats.CellularSuccesses,
		APICallsToday:     lm.stats.APICallsToday,
		TotalCost:         lm.stats.TotalCost,
	}
}

// PrintStats displays performance statistics
func (lm *LocationManager) PrintStats() {
	stats := lm.GetStats()

	fmt.Println("\n📊 Location Manager Statistics:")
	fmt.Println("===============================")

	if stats.GPSAttempts > 0 {
		gpsRate := float64(stats.GPSSuccesses) / float64(stats.GPSAttempts) * 100
		fmt.Printf("🛰️  GPS: %d/%d (%.1f%% success rate)\n",
			stats.GPSSuccesses, stats.GPSAttempts, gpsRate)
	}

	if stats.WiFiAttempts > 0 {
		wifiRate := float64(stats.WiFiSuccesses) / float64(stats.WiFiAttempts) * 100
		fmt.Printf("🚀 WiFi: %d/%d (%.1f%% success rate)\n",
			stats.WiFiSuccesses, stats.WiFiAttempts, wifiRate)
	}

	if stats.CellularAttempts > 0 {
		cellRate := float64(stats.CellularSuccesses) / float64(stats.CellularAttempts) * 100
		fmt.Printf("📱 Cellular: %d/%d (%.1f%% success rate)\n",
			stats.CellularSuccesses, stats.CellularAttempts, cellRate)
	}

	fmt.Printf("💰 API Calls Today: %d/%d\n", stats.APICallsToday, lm.config.DailyQuotaLimit)
	fmt.Printf("💵 Total Cost: $%.3f\n", stats.TotalCost)
}

// testLocationManager demonstrates the location manager
func testLocationManager() {
	fmt.Println("🎯 Location Manager Strategy Test")
	fmt.Println("=================================")

	// Load Google API key
	apiKey, err := LoadGoogleAPIKey()
	if err != nil {
		fmt.Printf("❌ Failed to load Google API key: %v\n", err)
		return
	}

	// Create configuration
	config := &LocationConfig{
		GPSTimeout:        5 * time.Second,
		GPSRetries:        1,
		WiFiTimeout:       10 * time.Second,
		WiFiCacheDuration: 2 * time.Minute,
		WiFiMinAPs:        2,
		CellTimeout:       15 * time.Second,
		CellCacheDuration: 5 * time.Minute,
		GoogleAPIKey:      apiKey,
		DailyQuotaLimit:   320, // ~10k/month = 333/day, use 320 for safety
		BurstLimit:        50,
		UpdateInterval:    5 * time.Minute,
		MonitorInterval:   30 * time.Second,
	}

	// Create location manager
	lm, err := NewLocationManager(config)
	if err != nil {
		fmt.Printf("❌ Failed to create location manager: %v\n", err)
		return
	}
	defer lm.sshClient.Close()

	// Test the intelligent hierarchy
	ctx := context.Background()

	fmt.Println("\n🔄 Testing location hierarchy (3 attempts)...")
	for i := 1; i <= 3; i++ {
		fmt.Printf("\n--- Attempt %d ---\n", i)

		result, err := lm.GetBestLocation(ctx)
		if err != nil {
			fmt.Printf("❌ Location failed: %v\n", err)
			continue
		}

		fmt.Printf("📍 Result: %.6f°, %.6f°\n", result.Location.Lat, result.Location.Lng)
		fmt.Printf("🎯 Accuracy: ±%.0fm\n", result.Accuracy)
		fmt.Printf("📡 Source: %s\n", result.Source)
		fmt.Printf("⏱️  Response: %v\n", result.ResponseTime)
		fmt.Printf("💰 Cost: $%.3f\n", result.Cost)
		fmt.Printf("⭐ Quality: %s\n", result.Quality)
		if result.Cached {
			fmt.Println("💾 (Cached)")
		}

		// Wait between attempts to test caching
		if i < 3 {
			fmt.Println("⏳ Waiting 10 seconds...")
			time.Sleep(10 * time.Second)
		}
	}

	// Display statistics
	lm.PrintStats()
}
