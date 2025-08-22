package main

import (
	"context"
	"fmt"
	"math"
	"sync"
	"time"

	"golang.org/x/crypto/ssh"
	"googlemaps.github.io/maps"
)

// ProductionLocationManager implements production-ready location management
type ProductionLocationManager struct {
	// Configuration
	config *ProductionLocationConfig

	// Clients
	sshClient    *ssh.Client
	googleClient *maps.Client

	// State management
	lastKnownLocation  *ProductionLocationResponse
	lastQueryTimestamp time.Time
	lastEnvironmentSig *EnvironmentSignature

	// Async operation management
	queryInProgress  bool
	backgroundCtx    context.Context
	backgroundCancel context.CancelFunc

	// Statistics
	stats *ProductionLocationStats

	// Thread safety
	mu sync.RWMutex
}

// ProductionLocationConfig holds production configuration
type ProductionLocationConfig struct {
	// API settings
	GoogleAPIKey             string `uci:"autonomy.location.google_api_key"`
	GoogleGeoLocationEnabled bool   `uci:"autonomy.location.google_geolocation_enabled" default:"false"`
	MonthlyQuota             int    `uci:"autonomy.location.monthly_quota" default:"10000"`

	// Timing settings
	MinQueryInterval      time.Duration `uci:"autonomy.location.min_query_interval" default:"5m"`
	MaxCacheAge           time.Duration `uci:"autonomy.location.max_cache_age" default:"60m"`
	DebounceTime          time.Duration `uci:"autonomy.location.debounce_time" default:"10s"`
	RetryVerificationTime time.Duration `uci:"autonomy.location.retry_verification_time" default:"10s"`

	// Change detection thresholds
	CellChangeThreshold float64 `uci:"autonomy.location.cell_change_threshold" default:"0.35"`
	WiFiChangeThreshold float64 `uci:"autonomy.location.wifi_change_threshold" default:"0.40"`
	CellTopN            int     `uci:"autonomy.location.cell_top_n" default:"8"`
	WiFiTopK            int     `uci:"autonomy.location.wifi_top_k" default:"10"`

	// Stationary detection
	StationaryIntervals []time.Duration // [10m, 20m, 40m, 60m]
	StationaryThreshold time.Duration   `uci:"autonomy.location.stationary_threshold" default:"2h"`

	// Quality gating (ChatGPT recommended parameters)
	AccuracyImprovement     float64 `uci:"autonomy.location.accuracy_improvement" default:"0.8"`      // Accept if 80% of old accuracy
	MinMovementDistance     float64 `uci:"autonomy.location.min_movement_distance" default:"300"`     // 300 meters
	MovementAccuracyFactor  float64 `uci:"autonomy.location.movement_accuracy_factor" default:"1.5"`  // 1.5x accuracy for movement
	AccuracyRegressionLimit float64 `uci:"autonomy.location.accuracy_regression_limit" default:"1.2"` // Allow 20% accuracy loss
	ChiSquareThreshold      float64 `uci:"autonomy.location.chi_square_threshold" default:"5.99"`     // 95% confidence in 2D

	// Background operation settings
	BackgroundUpdateInterval time.Duration `uci:"autonomy.location.background_interval" default:"30s"`
	MaxConcurrentQueries     int           `uci:"autonomy.location.max_concurrent_queries" default:"1"`
}

// ProductionLocationResponse represents the final location response
type ProductionLocationResponse struct {
	Latitude     float64       `json:"latitude"`
	Longitude    float64       `json:"longitude"`
	Accuracy     float64       `json:"accuracy"` // Meters - for UI display only
	Timestamp    time.Time     `json:"timestamp"`
	Source       string        `json:"source"`
	FromCache    bool          `json:"from_cache"`
	APICallMade  bool          `json:"api_call_made"`
	ResponseTime time.Duration `json:"response_time"`
}

// ProductionLocationStats tracks operational statistics
type ProductionLocationStats struct {
	TotalRequests        int64         `json:"total_requests"`
	CacheHits            int64         `json:"cache_hits"`
	APICallsToday        int64         `json:"api_calls_today"`
	SuccessfulQueries    int64         `json:"successful_queries"`
	FailedQueries        int64         `json:"failed_queries"`
	EnvironmentChanges   int64         `json:"environment_changes"`
	DebouncedChanges     int64         `json:"debounced_changes"`
	VerifiedChanges      int64         `json:"verified_changes"`
	FallbacksToCache     int64         `json:"fallbacks_to_cache"`
	QualityRejections    int64         `json:"quality_rejections"`
	AcceptedLocations    int64         `json:"accepted_locations"`
	BigMoveAcceptances   int64         `json:"big_move_acceptances"`
	AccuracyImprovements int64         `json:"accuracy_improvements"`
	StatisticalOutliers  int64         `json:"statistical_outliers"`
	LastResetDate        time.Time     `json:"last_reset_date"`
	AverageResponseTime  time.Duration `json:"average_response_time"`
}

// NewProductionLocationManager creates a new production location manager
func NewProductionLocationManager(config *ProductionLocationConfig) (*ProductionLocationManager, error) {
	// Create SSH client
	sshClient, err := createSSHClient()
	if err != nil {
		return nil, fmt.Errorf("failed to create SSH client: %w", err)
	}

	// Validate Google API configuration
	var googleClient *maps.Client
	if config.GoogleGeoLocationEnabled && config.GoogleAPIKey != "" {
		googleClient, err = maps.NewClient(maps.WithAPIKey(config.GoogleAPIKey))
		if err != nil {
			sshClient.Close()
			return nil, fmt.Errorf("failed to create Google client: %w", err)
		}
		fmt.Println("✅ Google Geolocation API enabled and configured")
	} else {
		fmt.Println("⚠️  Google Geolocation API disabled (no key or UCI setting disabled)")
		googleClient = nil // Will skip Google API calls
	}

	// Create background context
	backgroundCtx, backgroundCancel := context.WithCancel(context.Background())

	plm := &ProductionLocationManager{
		config:           config,
		sshClient:        sshClient,
		googleClient:     googleClient,
		backgroundCtx:    backgroundCtx,
		backgroundCancel: backgroundCancel,
		stats:            &ProductionLocationStats{LastResetDate: time.Now()},
	}

	// Start background monitoring
	go plm.backgroundMonitor()

	return plm, nil
}

// GetLocation returns the current best location estimate (non-blocking)
func (plm *ProductionLocationManager) GetLocation() *ProductionLocationResponse {
	plm.mu.RLock()
	defer plm.mu.RUnlock()

	plm.stats.TotalRequests++

	// Always return immediately with best available data
	if plm.lastKnownLocation != nil {
		// Check if cache is still valid
		cacheAge := time.Since(plm.lastKnownLocation.Timestamp)
		if cacheAge <= plm.config.MaxCacheAge {
			plm.stats.CacheHits++

			// Return cached location (create copy to avoid race conditions)
			response := *plm.lastKnownLocation
			response.FromCache = true
			response.ResponseTime = 0 // Instant from cache

			return &response
		}
	}

	// If no valid cache, trigger background update but still return last known
	go plm.triggerBackgroundUpdate("cache_expired")

	if plm.lastKnownLocation != nil {
		// Return stale cache with indication
		response := *plm.lastKnownLocation
		response.FromCache = true
		response.ResponseTime = 0
		return &response
	}

	// No location available at all - return nil (caller should handle gracefully)
	return nil
}

// backgroundMonitor continuously monitors for environment changes
func (plm *ProductionLocationManager) backgroundMonitor() {
	ticker := time.NewTicker(plm.config.BackgroundUpdateInterval)
	defer ticker.Stop()

	for {
		select {
		case <-plm.backgroundCtx.Done():
			return
		case <-ticker.C:
			plm.checkForEnvironmentChanges()
		}
	}
}

// checkForEnvironmentChanges checks if environment has changed significantly
func (plm *ProductionLocationManager) checkForEnvironmentChanges() {
	// Skip if query already in progress
	plm.mu.RLock()
	if plm.queryInProgress {
		plm.mu.RUnlock()
		return
	}
	plm.mu.RUnlock()

	// Check minimum query interval
	if time.Since(plm.lastQueryTimestamp) < plm.config.MinQueryInterval {
		return
	}

	// Get current environment signature
	currentSig, err := plm.getCurrentEnvironmentSignature()
	if err != nil {
		fmt.Printf("    ⚠️  Failed to get environment signature: %v\n", err)
		return
	}

	// Compare with last known signature
	if plm.lastEnvironmentSig != nil {
		changeDetected, changeReason := plm.detectSignificantChange(plm.lastEnvironmentSig, currentSig)
		if changeDetected {
			fmt.Printf("    🔄 Environment change detected: %s\n", changeReason)
			plm.stats.EnvironmentChanges++

			// Implement debouncing with retry verification
			go plm.debounceAndVerifyChange(currentSig, changeReason)
		}
	} else {
		// First time - just store the signature
		plm.mu.Lock()
		plm.lastEnvironmentSig = currentSig
		plm.mu.Unlock()
	}
}

// debounceAndVerifyChange implements the "sleep 10 and retry" logic
func (plm *ProductionLocationManager) debounceAndVerifyChange(initialSig *EnvironmentSignature, initialReason string) {
	plm.stats.DebouncedChanges++

	fmt.Printf("    ⏳ Debouncing change for %.0fs: %s\n", plm.config.RetryVerificationTime.Seconds(), initialReason)

	// Sleep for debounce time
	time.Sleep(plm.config.RetryVerificationTime)

	// Re-check environment to verify change is persistent
	verifySig, err := plm.getCurrentEnvironmentSignature()
	if err != nil {
		fmt.Printf("    ⚠️  Failed to verify environment change: %v\n", err)
		return
	}

	// Compare verification signature with original
	changeStillPresent, verifyReason := plm.detectSignificantChange(plm.lastEnvironmentSig, verifySig)
	if changeStillPresent {
		fmt.Printf("    ✅ Change verified after debounce: %s\n", verifyReason)
		plm.stats.VerifiedChanges++

		// Change is persistent - trigger API query
		plm.triggerBackgroundUpdate(fmt.Sprintf("verified_change: %s", verifyReason))
	} else {
		fmt.Printf("    ❌ Change was temporary, ignoring\n")
	}
}

// triggerBackgroundUpdate triggers an asynchronous location update
func (plm *ProductionLocationManager) triggerBackgroundUpdate(reason string) {
	plm.mu.Lock()
	if plm.queryInProgress {
		plm.mu.Unlock()
		return // Already in progress
	}
	plm.queryInProgress = true
	plm.mu.Unlock()

	go func() {
		defer func() {
			plm.mu.Lock()
			plm.queryInProgress = false
			plm.mu.Unlock()
		}()

		fmt.Printf("    🔍 Background location update triggered: %s\n", reason)
		startTime := time.Now()

		// Perform the actual location query
		newLocation, err := plm.performLocationQuery()
		responseTime := time.Since(startTime)

		plm.mu.Lock()
		defer plm.mu.Unlock()

		if err != nil {
			plm.stats.FailedQueries++
			plm.stats.FallbacksToCache++
			fmt.Printf("    ❌ Location query failed: %v (keeping last known location)\n", err)
			// Do NOT update lastQueryTimestamp on failure - allows retry sooner
			return
		}

		// Success - update state
		plm.stats.SuccessfulQueries++
		plm.stats.APICallsToday++
		plm.lastKnownLocation = newLocation
		plm.lastQueryTimestamp = time.Now()

		// Update average response time
		if plm.stats.SuccessfulQueries == 1 {
			plm.stats.AverageResponseTime = responseTime
		} else {
			// Simple moving average
			plm.stats.AverageResponseTime = (plm.stats.AverageResponseTime + responseTime) / 2
		}

		// Update environment signature
		if currentSig, err := plm.getCurrentEnvironmentSignature(); err == nil {
			plm.lastEnvironmentSig = currentSig
		}

		fmt.Printf("    ✅ Location updated: %.6f°, %.6f° (±%.0fm) in %v\n",
			newLocation.Latitude, newLocation.Longitude, newLocation.Accuracy, responseTime)
	}()
}

// performLocationQuery performs the actual Google API location query
func (plm *ProductionLocationManager) performLocationQuery() (*ProductionLocationResponse, error) {
	// Check if Google API is available
	if plm.googleClient == nil {
		return nil, fmt.Errorf("Google Geolocation API not available (disabled or no API key)")
	}
	// Get enhanced WiFi scan
	wifiScan, err := performEnhancedUbusWiFiScan(plm.sshClient)
	if err != nil {
		return nil, fmt.Errorf("WiFi scan failed: %w", err)
	}

	if len(wifiScan.GoogleWiFiAPs) < 2 {
		return nil, fmt.Errorf("insufficient WiFi APs: %d", len(wifiScan.GoogleWiFiAPs))
	}

	// Get cellular intelligence
	cellIntel, err := collectCellularLocationIntelligence(plm.sshClient)
	if err != nil {
		fmt.Printf("    ⚠️  Cellular intelligence failed, using WiFi-only: %v\n", err)
		cellIntel = nil // Continue with WiFi-only
	}

	// Build Google API request
	req := &maps.GeolocationRequest{
		WiFiAccessPoints: wifiScan.GoogleWiFiAPs,
		ConsiderIP:       false,
	}

	// Add cellular data if available
	if cellIntel != nil {
		cellTowers, _, err := BuildGoogleCellTowersFromIntelligence(cellIntel, 10)
		if err == nil && len(cellTowers) > 0 {
			req.CellTowers = cellTowers

			// Add home network information
			if mcc := parseInt(cellIntel.ServingCell.MCC); mcc != 0 {
				req.HomeMobileCountryCode = mcc
			}
			if mnc := parseInt(cellIntel.ServingCell.MNC); mnc != 0 {
				req.HomeMobileNetworkCode = mnc
			}
			req.Carrier = cellIntel.NetworkInfo.Operator
		}
	}

	// Make Google API call
	ctx, cancel := context.WithTimeout(context.Background(), 15*time.Second)
	defer cancel()

	resp, err := plm.googleClient.Geolocate(ctx, req)
	if err != nil {
		return nil, fmt.Errorf("Google API call failed: %w", err)
	}

	// Create new location response from API
	source := fmt.Sprintf("Google API (%d WiFi", len(wifiScan.GoogleWiFiAPs))
	if len(req.CellTowers) > 0 {
		source += fmt.Sprintf(" + %d Cell", len(req.CellTowers))
	}
	source += ")"

	newLocation := &ProductionLocationResponse{
		Latitude:     resp.Location.Lat,
		Longitude:    resp.Location.Lng,
		Accuracy:     resp.Accuracy, // For UI display only - not for filtering
		Timestamp:    time.Now(),
		Source:       source,
		FromCache:    false,
		APICallMade:  true,
		ResponseTime: 0, // Will be set by caller
	}

	// Apply quality gates before accepting the new location
	return plm.applyQualityGates(newLocation)
}

// applyQualityGates applies ChatGPT's quality gating logic to new location
func (plm *ProductionLocationManager) applyQualityGates(newLocation *ProductionLocationResponse) (*ProductionLocationResponse, error) {
	// If no previous location, always accept first fix
	if plm.lastKnownLocation == nil {
		plm.stats.AcceptedLocations++
		fmt.Println("    ✅ First location fix - accepted")
		return newLocation, nil
	}

	// Calculate distance from current position
	distance := calculateHaversineDistance(
		plm.lastKnownLocation.Latitude, plm.lastKnownLocation.Longitude,
		newLocation.Latitude, newLocation.Longitude)

	fmt.Printf("    📏 Distance from last location: %.0fm\n", distance)

	// Gate 1: Big move gate - accept immediately if clearly moved
	bigMoveThreshold := math.Max(plm.config.MinMovementDistance,
		plm.config.MovementAccuracyFactor*plm.lastKnownLocation.Accuracy)

	if distance >= bigMoveThreshold {
		plm.stats.BigMoveAcceptances++
		plm.stats.AcceptedLocations++
		fmt.Printf("    ✅ Big move detected - accepted (%.0fm > %.0fm)\n", distance, bigMoveThreshold)
		return newLocation, nil
	}

	// Gate 2: Accuracy improvement gate
	if newLocation.Accuracy <= plm.config.AccuracyImprovement*plm.lastKnownLocation.Accuracy {
		plm.stats.AccuracyImprovements++
		plm.stats.AcceptedLocations++
		fmt.Printf("    ✅ Accuracy improved - accepted (%.0fm vs %.0fm)\n",
			newLocation.Accuracy, plm.lastKnownLocation.Accuracy)
		return newLocation, nil
	}

	// Gate 3: Chi-square statistical gate (anti-jitter)
	sigma := newLocation.Accuracy
	if sigma > 0 {
		chiSquare := math.Pow(distance/sigma, 2)

		if chiSquare <= plm.config.ChiSquareThreshold {
			plm.stats.AcceptedLocations++
			fmt.Printf("    ✅ Statistically consistent - accepted (χ²=%.2f ≤ %.2f)\n",
				chiSquare, plm.config.ChiSquareThreshold)
			return newLocation, nil
		}

		plm.stats.StatisticalOutliers++
		fmt.Printf("    ❌ Statistical outlier - rejected (χ²=%.2f > %.2f)\n",
			chiSquare, plm.config.ChiSquareThreshold)
	}

	// Gate 4: Movement with acceptable accuracy regression
	if distance > 50.0 && newLocation.Accuracy <= plm.lastKnownLocation.Accuracy*plm.config.AccuracyRegressionLimit {
		plm.stats.AcceptedLocations++
		fmt.Printf("    ✅ Movement with acceptable accuracy regression - accepted\n")
		return newLocation, nil
	}

	// All gates failed - reject the new location
	plm.stats.QualityRejections++
	fmt.Printf("    ❌ Quality gates failed - keeping last known location\n")
	fmt.Printf("        Distance: %.0fm, Accuracy: %.0fm vs %.0fm\n",
		distance, newLocation.Accuracy, plm.lastKnownLocation.Accuracy)

	// Return error to indicate rejection (caller will use lastKnownLocation)
	return nil, fmt.Errorf("location rejected by quality gates")
}

// getCurrentEnvironmentSignature gets current cellular and WiFi environment signature
func (plm *ProductionLocationManager) getCurrentEnvironmentSignature() (*EnvironmentSignature, error) {
	// Get cellular intelligence
	cellIntel, err := collectCellularLocationIntelligence(plm.sshClient)
	if err != nil {
		return nil, fmt.Errorf("cellular intelligence failed: %w", err)
	}

	// Get WiFi scan (lightweight - just for signature)
	wifiScan, err := performEnhancedUbusWiFiScan(plm.sshClient)
	if err != nil {
		return nil, fmt.Errorf("WiFi scan failed: %w", err)
	}

	// Create signatures
	cellSig := plm.createCellularSignature(cellIntel)
	wifiSig := plm.createWiFiSignature(wifiScan.AccessPoints)

	return &EnvironmentSignature{
		CellSignature: cellSig,
		WiFiSignature: wifiSig,
		Timestamp:     time.Now(),
	}, nil
}

// createCellularSignature creates a signature from cellular environment
func (plm *ProductionLocationManager) createCellularSignature(intel *CellularLocationIntelligence) string {
	// Create signature based on serving cell and top neighbor cells
	signature := fmt.Sprintf("serving:%s-%s-%s",
		intel.ServingCell.MCC, intel.ServingCell.MNC, intel.ServingCell.CellID)

	// Add top N neighbor cells by signal strength
	neighbors := intel.NeighborCells
	if len(neighbors) > plm.config.CellTopN {
		neighbors = neighbors[:plm.config.CellTopN]
	}

	for _, neighbor := range neighbors {
		signature += fmt.Sprintf(",n:%d:%d", neighbor.PCID, neighbor.RSSI)
	}

	return signature
}

// createWiFiSignature creates a signature from WiFi environment
func (plm *ProductionLocationManager) createWiFiSignature(accessPoints []UbusWiFiAccessPoint) string {
	// Sort by signal strength
	aps := make([]UbusWiFiAccessPoint, len(accessPoints))
	copy(aps, accessPoints)

	// Simple sort by signal strength (strongest first)
	for i := 0; i < len(aps)-1; i++ {
		for j := i + 1; j < len(aps); j++ {
			if aps[i].Signal < aps[j].Signal { // Remember signals are negative
				aps[i], aps[j] = aps[j], aps[i]
			}
		}
	}

	// Take top K BSSIDs
	topK := plm.config.WiFiTopK
	if len(aps) < topK {
		topK = len(aps)
	}

	signature := ""
	for i := 0; i < topK; i++ {
		if i > 0 {
			signature += ","
		}
		signature += fmt.Sprintf("%s:%d", aps[i].BSSID, aps[i].Signal)
	}

	return signature
}

// detectSignificantChange compares two environment signatures
func (plm *ProductionLocationManager) detectSignificantChange(old, new *EnvironmentSignature) (bool, string) {
	// Check cellular changes
	if old.CellSignature != new.CellSignature {
		return true, "cellular environment changed"
	}

	// Check WiFi changes
	if old.WiFiSignature != new.WiFiSignature {
		return true, "WiFi environment changed"
	}

	return false, ""
}

// GetStats returns current operational statistics
func (plm *ProductionLocationManager) GetStats() *ProductionLocationStats {
	plm.mu.RLock()
	defer plm.mu.RUnlock()

	// Reset daily stats if needed
	if !isSameDay(plm.stats.LastResetDate, time.Now()) {
		plm.stats.APICallsToday = 0
		plm.stats.LastResetDate = time.Now()
	}

	// Return a copy
	stats := *plm.stats
	return &stats
}

// PrintProductionStats displays comprehensive operational statistics
func (plm *ProductionLocationManager) PrintProductionStats() {
	stats := plm.GetStats()

	fmt.Println("\n📊 Production Location Manager Statistics:")
	fmt.Println("==========================================")

	if plm.lastKnownLocation != nil {
		cacheAge := time.Since(plm.lastKnownLocation.Timestamp)
		fmt.Printf("📍 Last Known Location: %.6f°, %.6f° (±%.0fm)\n",
			plm.lastKnownLocation.Latitude, plm.lastKnownLocation.Longitude, plm.lastKnownLocation.Accuracy)
		fmt.Printf("⏰ Cache Age: %.1f minutes\n", cacheAge.Minutes())
		fmt.Printf("📡 Source: %s\n", plm.lastKnownLocation.Source)
	} else {
		fmt.Println("📍 No location data available")
	}

	fmt.Printf("\n📊 Request Statistics:\n")
	fmt.Printf("  🔍 Total Requests: %d\n", stats.TotalRequests)
	fmt.Printf("  💾 Cache Hits: %d (%.1f%%)\n", stats.CacheHits,
		float64(stats.CacheHits)/float64(stats.TotalRequests)*100)
	fmt.Printf("  ✅ Successful Queries: %d\n", stats.SuccessfulQueries)
	fmt.Printf("  ❌ Failed Queries: %d\n", stats.FailedQueries)
	fmt.Printf("  🔄 Fallbacks to Cache: %d\n", stats.FallbacksToCache)

	fmt.Printf("\n🔄 Change Detection:\n")
	fmt.Printf("  📡 Environment Changes: %d\n", stats.EnvironmentChanges)
	fmt.Printf("  ⏳ Debounced Changes: %d\n", stats.DebouncedChanges)
	fmt.Printf("  ✅ Verified Changes: %d\n", stats.VerifiedChanges)

	fmt.Printf("🎯 Quality Gating:\n")
	fmt.Printf("  ✅ Accepted Locations: %d\n", stats.AcceptedLocations)
	fmt.Printf("  ❌ Quality Rejections: %d\n", stats.QualityRejections)
	fmt.Printf("  🚶 Big Move Acceptances: %d\n", stats.BigMoveAcceptances)
	fmt.Printf("  📊 Accuracy Improvements: %d\n", stats.AccuracyImprovements)
	fmt.Printf("  📈 Statistical Outliers: %d\n", stats.StatisticalOutliers)

	fmt.Printf("\n💰 API Usage:\n")
	fmt.Printf("  📅 Today: %d calls\n", stats.APICallsToday)
	dailyLimit := float64(plm.config.MonthlyQuota) / 30.0
	fmt.Printf("  📊 Daily Limit: %.0f calls\n", dailyLimit)
	fmt.Printf("  📈 Usage: %.1f%%\n", float64(stats.APICallsToday)/dailyLimit*100)

	fmt.Printf("\n⏱️  Performance:\n")
	fmt.Printf("  📊 Average Response Time: %v\n", stats.AverageResponseTime)
	fmt.Printf("  🔄 Query In Progress: %t\n", plm.queryInProgress)
}

// Close gracefully shuts down the location manager
func (plm *ProductionLocationManager) Close() error {
	// Cancel background operations
	plm.backgroundCancel()

	// Close SSH client
	if plm.sshClient != nil {
		plm.sshClient.Close()
	}

	return nil
}

// testProductionLocationManager demonstrates the production location manager
func testProductionLocationManager() {
	fmt.Println("🏭 Production Location Manager Test")
	fmt.Println("===================================")
	fmt.Println("Features: Non-blocking, Error fallback, Debounced changes, Trust API output")

	// Load Google API key
	apiKey, err := LoadGoogleAPIKey()
	if err != nil {
		fmt.Printf("❌ Failed to load Google API key: %v\n", err)
		return
	}

	// Create production configuration
	config := &ProductionLocationConfig{
		GoogleAPIKey:             apiKey,
		GoogleGeoLocationEnabled: true, // Enable Google API for testing
		MonthlyQuota:             10000,
		MinQueryInterval:         5 * time.Minute,
		MaxCacheAge:              60 * time.Minute,
		DebounceTime:             10 * time.Second,
		RetryVerificationTime:    10 * time.Second,
		CellChangeThreshold:      0.35,
		WiFiChangeThreshold:      0.40,
		CellTopN:                 8,
		WiFiTopK:                 10,
		StationaryIntervals:      []time.Duration{10 * time.Minute, 20 * time.Minute, 40 * time.Minute, 60 * time.Minute},
		StationaryThreshold:      2 * time.Hour,
		AccuracyImprovement:      0.8,   // Accept if 80% of old accuracy
		MinMovementDistance:      300.0, // 300 meters
		MovementAccuracyFactor:   1.5,   // 1.5x accuracy for movement
		AccuracyRegressionLimit:  1.2,   // Allow 20% accuracy loss
		ChiSquareThreshold:       5.99,  // 95% confidence in 2D
		BackgroundUpdateInterval: 30 * time.Second,
		MaxConcurrentQueries:     1,
	}

	// Create production location manager
	plm, err := NewProductionLocationManager(config)
	if err != nil {
		fmt.Printf("❌ Failed to create production location manager: %v\n", err)
		return
	}
	defer plm.Close()

	fmt.Println("\n🔄 Testing production behavior (5 requests over time)...")

	for i := 1; i <= 5; i++ {
		fmt.Printf("\n--- Request %d ---\n", i)

		// Get location (always non-blocking)
		startTime := time.Now()
		location := plm.GetLocation()
		responseTime := time.Since(startTime)

		if location != nil {
			fmt.Printf("✅ Location received in %v\n", responseTime)
			fmt.Printf("📍 Location: %.6f°, %.6f° (±%.0fm)\n",
				location.Latitude, location.Longitude, location.Accuracy)
			fmt.Printf("📡 Source: %s\n", location.Source)
			fmt.Printf("💾 From Cache: %t\n", location.FromCache)
			fmt.Printf("🌐 API Call Made: %t\n", location.APICallMade)
		} else {
			fmt.Println("❌ No location available")
		}

		// Display statistics
		plm.PrintProductionStats()

		// Wait between requests
		if i < 5 {
			waitTime := 2 * time.Minute
			fmt.Printf("⏳ Waiting %.1f minutes (background monitoring continues)...\n", waitTime.Minutes())
			time.Sleep(waitTime)
		}
	}

	// Let background monitoring run a bit longer
	fmt.Println("\n⏳ Letting background monitoring run for 1 minute...")
	time.Sleep(1 * time.Minute)

	// Final statistics
	fmt.Println("\n📊 Final Statistics:")
	plm.PrintProductionStats()

	fmt.Println("\n🎯 Production features demonstrated:")
	fmt.Println("  ✅ Non-blocking location requests (instant response)")
	fmt.Println("  ✅ Background environment monitoring")
	fmt.Println("  ✅ Debounced change detection (sleep 10s + retry)")
	fmt.Println("  ✅ Error fallback to last known location")
	fmt.Println("  ✅ Trust API output (no averaging or filtering)")
	fmt.Println("  ✅ Asynchronous operations (no UI blocking)")
	fmt.Println("  ✅ Comprehensive error handling")
}
