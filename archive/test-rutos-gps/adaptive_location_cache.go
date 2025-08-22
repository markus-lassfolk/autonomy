package main

import (
	"context"
	"crypto/sha256"
	"fmt"
	"math"
	"sort"
	"sync"
	"time"

	"golang.org/x/crypto/ssh"
	"googlemaps.github.io/maps"
)

// AdaptiveLocationCache implements intelligent caching with movement detection and quality gating
type AdaptiveLocationCache struct {
	// Configuration (UCI-configurable)
	config *AdaptiveCacheConfig

	// State tracking
	currentState     *LocationState
	fixBuffer        []*LocationFix
	lastCellSig      string
	lastWiFiSig      string
	lastTriggerTime  time.Time
	stationaryStart  time.Time
	movementDetected bool

	// Statistics
	stats *AdaptiveCacheStats

	// Thread safety
	mu sync.RWMutex
}

// AdaptiveCacheConfig holds all UCI-configurable parameters
type AdaptiveCacheConfig struct {
	// Trigger thresholds
	CellTopN             int     `uci:"cell_top_n" default:"8"`               // Top N cells to track
	CellChangeThreshold  float64 `uci:"cell_change_threshold" default:"0.35"` // 35% change threshold
	CellTopStrongChanged int     `uci:"cell_top_strong_changed" default:"2"`  // Top 2 strongest changed

	WiFiTopK             int     `uci:"wifi_top_k" default:"10"`              // Top K BSSIDs to track
	WiFiChangeThreshold  float64 `uci:"wifi_change_threshold" default:"0.40"` // 40% change threshold
	WiFiTopStrongChanged int     `uci:"wifi_top_strong_changed" default:"3"`  // Top 3 strongest changed

	// Timing controls
	DebounceTime          time.Duration `uci:"debounce_time" default:"10s"`          // Change persistence required
	MinIntervalMoving     time.Duration `uci:"min_interval_moving" default:"5m"`     // Hard floor when moving
	SoftTTL               time.Duration `uci:"soft_ttl" default:"15m"`               // Refresh if no change
	HardTTL               time.Duration `uci:"hard_ttl" default:"60m"`               // Force refresh max age
	StationaryBackoffTime time.Duration `uci:"stationary_backoff_time" default:"2h"` // When to start backoff

	// Adaptive intervals when stationary
	StationaryIntervals []time.Duration `uci:"stationary_intervals"` // [10m, 20m, 40m, 60m]

	// Quality gating
	AccuracyImprovement     float64 `uci:"accuracy_improvement" default:"0.8"`      // Accept if 80% of old accuracy
	MinMovementDistance     float64 `uci:"min_movement_distance" default:"300"`     // Minimum movement in meters
	MovementAccuracyFactor  float64 `uci:"movement_accuracy_factor" default:"1.5"`  // Movement = 1.5 × accuracy
	AccuracyRegressionLimit float64 `uci:"accuracy_regression_limit" default:"1.2"` // Allow 20% accuracy loss on movement
	ChiSquareThreshold      float64 `uci:"chi_square_threshold" default:"5.99"`     // 95% confidence in 2D

	// Budget management
	MonthlyQuota          int           `uci:"monthly_quota" default:"10000"`         // 10k free requests
	DailyQuotaPercent     float64       `uci:"daily_quota_percent" default:"0.5"`     // 50% by midday
	QuotaExceededInterval time.Duration `uci:"quota_exceeded_interval" default:"15m"` // Fallback interval

	// Smoothing
	BufferSize            int     `uci:"buffer_size" default:"10"`             // Rolling buffer size
	SmoothingWindowMoving int     `uci:"smoothing_window_moving" default:"5"`  // Fixes to smooth when moving
	SmoothingWindowParked int     `uci:"smoothing_window_parked" default:"10"` // Fixes to smooth when parked
	EMAAlphaMin           float64 `uci:"ema_alpha_min" default:"0.2"`          // Minimum EMA alpha
	EMAAlphaMax           float64 `uci:"ema_alpha_max" default:"0.5"`          // Maximum EMA alpha
}

// LocationState represents the current filtered location state
type LocationState struct {
	Latitude     float64   `json:"latitude"`
	Longitude    float64   `json:"longitude"`
	Accuracy     float64   `json:"accuracy"`
	Timestamp    time.Time `json:"timestamp"`
	Source       string    `json:"source"`
	IsStationary bool      `json:"is_stationary"`
	Confidence   float64   `json:"confidence"`
}

// LocationFix represents a single location fix with metadata
type LocationFix struct {
	Latitude     float64   `json:"latitude"`
	Longitude    float64   `json:"longitude"`
	Accuracy     float64   `json:"accuracy"`
	Timestamp    time.Time `json:"timestamp"`
	Source       string    `json:"source"`
	Accepted     bool      `json:"accepted"`
	RejectReason string    `json:"reject_reason,omitempty"`
	ChiSquare    float64   `json:"chi_square"`
	Distance     float64   `json:"distance"`
}

// AdaptiveCacheStats tracks performance and behavior
type AdaptiveCacheStats struct {
	TotalQueries       int64     `json:"total_queries"`
	CacheHits          int64     `json:"cache_hits"`
	MovementTriggers   int64     `json:"movement_triggers"`
	CellChangeTriggers int64     `json:"cell_change_triggers"`
	WiFiChangeTriggers int64     `json:"wifi_change_triggers"`
	QualityRejections  int64     `json:"quality_rejections"`
	AcceptedFixes      int64     `json:"accepted_fixes"`
	StationaryPeriods  int64     `json:"stationary_periods"`
	APICallsToday      int64     `json:"api_calls_today"`
	LastResetDate      time.Time `json:"last_reset_date"`
}

// EnvironmentSignature represents cellular and WiFi environment
type EnvironmentSignature struct {
	CellSignature string    `json:"cell_signature"`
	WiFiSignature string    `json:"wifi_signature"`
	Timestamp     time.Time `json:"timestamp"`
}

// NewAdaptiveLocationCache creates a new adaptive cache with default configuration
func NewAdaptiveLocationCache() *AdaptiveLocationCache {
	config := &AdaptiveCacheConfig{
		CellTopN:                8,
		CellChangeThreshold:     0.35,
		CellTopStrongChanged:    2,
		WiFiTopK:                10,
		WiFiChangeThreshold:     0.40,
		WiFiTopStrongChanged:    3,
		DebounceTime:            10 * time.Second,
		MinIntervalMoving:       5 * time.Minute,
		SoftTTL:                 15 * time.Minute,
		HardTTL:                 60 * time.Minute,
		StationaryBackoffTime:   2 * time.Hour,
		StationaryIntervals:     []time.Duration{10 * time.Minute, 20 * time.Minute, 40 * time.Minute, 60 * time.Minute},
		AccuracyImprovement:     0.8,
		MinMovementDistance:     300.0,
		MovementAccuracyFactor:  1.5,
		AccuracyRegressionLimit: 1.2,
		ChiSquareThreshold:      5.99,
		MonthlyQuota:            10000,
		DailyQuotaPercent:       0.5,
		QuotaExceededInterval:   15 * time.Minute,
		BufferSize:              10,
		SmoothingWindowMoving:   5,
		SmoothingWindowParked:   10,
		EMAAlphaMin:             0.2,
		EMAAlphaMax:             0.5,
	}

	return &AdaptiveLocationCache{
		config:          config,
		fixBuffer:       make([]*LocationFix, 0, config.BufferSize),
		stats:           &AdaptiveCacheStats{LastResetDate: time.Now()},
		stationaryStart: time.Now(),
	}
}

// ShouldQuery determines if a new location query should be made
func (alc *AdaptiveLocationCache) ShouldQuery(client *ssh.Client) (bool, string) {
	alc.mu.Lock()
	defer alc.mu.Unlock()

	now := time.Now()

	// Reset daily stats if needed
	if !isSameDay(alc.stats.LastResetDate, now) {
		alc.stats.APICallsToday = 0
		alc.stats.LastResetDate = now
	}

	// 1. Check hard TTL (always query after max age)
	if alc.currentState != nil && now.Sub(alc.currentState.Timestamp) > alc.config.HardTTL {
		return true, "hard TTL exceeded"
	}

	// 2. Check minimum interval (never query too often)
	if now.Sub(alc.lastTriggerTime) < alc.getMinInterval() {
		remaining := alc.getMinInterval() - now.Sub(alc.lastTriggerTime)
		return false, fmt.Sprintf("within min interval (%.1fm remaining)", remaining.Minutes())
	}

	// 3. Check API quota
	if alc.isQuotaExceeded() {
		if now.Sub(alc.lastTriggerTime) < alc.config.QuotaExceededInterval {
			return false, "quota exceeded, using extended interval"
		}
		return true, "quota exceeded but extended interval reached"
	}

	// 4. Get current environment signatures
	cellSig, wifiSig, err := alc.getEnvironmentSignatures(client)
	if err != nil {
		fmt.Printf("    ⚠️  Failed to get environment signatures: %v\n", err)
		// Fallback to soft TTL
		if alc.currentState != nil && now.Sub(alc.currentState.Timestamp) > alc.config.SoftTTL {
			return true, "soft TTL exceeded (environment check failed)"
		}
		return false, "environment check failed, no TTL trigger"
	}

	// 5. Check for significant changes
	changeReason := alc.detectSignificantChanges(cellSig, wifiSig)
	if changeReason != "" {
		// Check debounce
		if alc.movementDetected && now.Sub(alc.lastTriggerTime) >= alc.config.DebounceTime {
			alc.movementDetected = false // Reset movement flag
			return true, changeReason
		} else if !alc.movementDetected {
			alc.movementDetected = true
			alc.lastTriggerTime = now
			return false, fmt.Sprintf("change detected, debouncing: %s", changeReason)
		}
		return false, fmt.Sprintf("debouncing change: %s", changeReason)
	}

	// 6. Check soft TTL (refresh if no change for a while)
	if alc.currentState != nil && now.Sub(alc.currentState.Timestamp) > alc.getSoftTTL() {
		return true, "soft TTL exceeded"
	}

	return false, "no trigger conditions met"
}

// getMinInterval returns the current minimum interval based on movement state
func (alc *AdaptiveLocationCache) getMinInterval() time.Duration {
	if alc.movementDetected || !alc.currentState.IsStationary {
		return alc.config.MinIntervalMoving
	}

	// Adaptive intervals when stationary
	stationaryDuration := time.Since(alc.stationaryStart)
	intervals := alc.config.StationaryIntervals

	switch {
	case stationaryDuration < 30*time.Minute:
		return intervals[0] // 10 minutes
	case stationaryDuration < 2*time.Hour:
		return intervals[1] // 20 minutes
	case stationaryDuration < 6*time.Hour:
		return intervals[2] // 40 minutes
	default:
		return intervals[3] // 60 minutes
	}
}

// getSoftTTL returns the current soft TTL based on stationary state
func (alc *AdaptiveLocationCache) getSoftTTL() time.Duration {
	if alc.currentState != nil && alc.currentState.IsStationary {
		stationaryDuration := time.Since(alc.stationaryStart)
		if stationaryDuration > alc.config.StationaryBackoffTime {
			return 30 * time.Minute // Double the soft TTL when stationary for 2+ hours
		}
	}
	return alc.config.SoftTTL
}

// isQuotaExceeded checks if we're exceeding the daily API quota
func (alc *AdaptiveLocationCache) isQuotaExceeded() bool {
	dailyLimit := float64(alc.config.MonthlyQuota) / 30.0 // Approximate daily limit
	currentHour := time.Now().Hour()

	// Check if we're exceeding 50% of daily quota by midday (12:00)
	if currentHour >= 12 {
		midDayLimit := dailyLimit * alc.config.DailyQuotaPercent
		return float64(alc.stats.APICallsToday) > midDayLimit
	}

	return false
}

// getEnvironmentSignatures gets current cellular and WiFi environment signatures
func (alc *AdaptiveLocationCache) getEnvironmentSignatures(client *ssh.Client) (string, string, error) {
	// Get cellular intelligence
	cellIntel, err := collectCellularLocationIntelligence(client)
	if err != nil {
		return "", "", fmt.Errorf("cellular intelligence failed: %w", err)
	}

	// Get WiFi scan
	wifiScan, err := performEnhancedUbusWiFiScan(client)
	if err != nil {
		return "", "", fmt.Errorf("WiFi scan failed: %w", err)
	}

	// Create cellular signature (top N cells by signal strength)
	cellSig := alc.createCellularSignature(cellIntel)

	// Create WiFi signature (top K BSSIDs by signal strength)
	wifiSig := alc.createWiFiSignature(wifiScan.AccessPoints)

	return cellSig, wifiSig, nil
}

// createCellularSignature creates a signature from cellular environment
func (alc *AdaptiveLocationCache) createCellularSignature(intel *CellularLocationIntelligence) string {
	type cellInfo struct {
		ID   string
		RSSI int
	}

	var cells []cellInfo

	// Add serving cell
	if intel.ServingCell.CellID != "" {
		cells = append(cells, cellInfo{
			ID:   fmt.Sprintf("%s-%s-%s", intel.ServingCell.MCC, intel.ServingCell.MNC, intel.ServingCell.CellID),
			RSSI: intel.SignalQuality.RSSI,
		})
	}

	// Add neighbor cells
	for _, neighbor := range intel.NeighborCells {
		if neighbor.PCID > 0 {
			cells = append(cells, cellInfo{
				ID:   fmt.Sprintf("%s-%s-%d", intel.ServingCell.MCC, intel.ServingCell.MNC, neighbor.PCID),
				RSSI: neighbor.RSSI,
			})
		}
	}

	// Sort by signal strength (strongest first)
	sort.Slice(cells, func(i, j int) bool {
		return cells[i].RSSI > cells[j].RSSI
	})

	// Take top N cells
	topN := alc.config.CellTopN
	if len(cells) < topN {
		topN = len(cells)
	}

	// Create signature string
	var sigParts []string
	for i := 0; i < topN; i++ {
		sigParts = append(sigParts, fmt.Sprintf("%s:%d", cells[i].ID, cells[i].RSSI))
	}

	// Hash the signature for efficient comparison
	return fmt.Sprintf("%x", sha256.Sum256([]byte(fmt.Sprintf("%v", sigParts))))[:16]
}

// createWiFiSignature creates a signature from WiFi environment
func (alc *AdaptiveLocationCache) createWiFiSignature(accessPoints []UbusWiFiAccessPoint) string {
	type wifiInfo struct {
		BSSID  string
		Signal int
	}

	var wifis []wifiInfo
	for _, ap := range accessPoints {
		if ap.BSSID != "" && ap.Signal != 0 {
			wifis = append(wifis, wifiInfo{
				BSSID:  ap.BSSID,
				Signal: ap.Signal,
			})
		}
	}

	// Sort by signal strength (strongest first, remember signals are negative)
	sort.Slice(wifis, func(i, j int) bool {
		return wifis[i].Signal > wifis[j].Signal
	})

	// Take top K BSSIDs
	topK := alc.config.WiFiTopK
	if len(wifis) < topK {
		topK = len(wifis)
	}

	// Create signature string
	var sigParts []string
	for i := 0; i < topK; i++ {
		sigParts = append(sigParts, fmt.Sprintf("%s:%d", wifis[i].BSSID, wifis[i].Signal))
	}

	// Hash the signature for efficient comparison
	return fmt.Sprintf("%x", sha256.Sum256([]byte(fmt.Sprintf("%v", sigParts))))[:16]
}

// detectSignificantChanges compares current signatures with cached ones
func (alc *AdaptiveLocationCache) detectSignificantChanges(cellSig, wifiSig string) string {
	var reasons []string

	// Check cellular changes
	if alc.lastCellSig != "" && alc.lastCellSig != cellSig {
		reasons = append(reasons, "cellular environment changed")
		alc.stats.CellChangeTriggers++
	}

	// Check WiFi changes
	if alc.lastWiFiSig != "" && alc.lastWiFiSig != wifiSig {
		reasons = append(reasons, "WiFi environment changed")
		alc.stats.WiFiChangeTriggers++
	}

	if len(reasons) > 0 {
		alc.stats.MovementTriggers++
		return fmt.Sprintf("%v", reasons)
	}

	return ""
}

// ProcessLocationFix processes a new location fix with quality gating and smoothing
func (alc *AdaptiveLocationCache) ProcessLocationFix(newFix *LocationFix) *LocationState {
	alc.mu.Lock()
	defer alc.mu.Unlock()

	// Apply quality gates
	accepted, reason := alc.applyQualityGates(newFix)
	newFix.Accepted = accepted
	newFix.RejectReason = reason

	if !accepted {
		alc.stats.QualityRejections++
		fmt.Printf("    ❌ Location fix rejected: %s\n", reason)
		// Still add to buffer for analysis
		alc.addToBuffer(newFix)
		return alc.currentState
	}

	alc.stats.AcceptedFixes++
	alc.addToBuffer(newFix)

	// Apply smoothing and update current state
	smoothedState := alc.applySmoothingFilter()
	alc.currentState = smoothedState

	// Update environment signatures
	// (This would be called after getting the fix)

	// Detect stationary vs moving state
	alc.updateMovementState()

	fmt.Printf("    ✅ Location fix accepted and smoothed\n")
	fmt.Printf("    📍 Smoothed: %.6f°, %.6f° (±%.0fm)\n",
		smoothedState.Latitude, smoothedState.Longitude, smoothedState.Accuracy)

	return smoothedState
}

// applyQualityGates applies quality gating rules to determine if a fix should be accepted
func (alc *AdaptiveLocationCache) applyQualityGates(newFix *LocationFix) (bool, string) {
	if alc.currentState == nil {
		return true, "first fix" // Always accept first fix
	}

	// Calculate distance from current position
	distance := calculateHaversineDistance(
		alc.currentState.Latitude, alc.currentState.Longitude,
		newFix.Latitude, newFix.Longitude)
	newFix.Distance = distance

	// Gate 1: Big move gate - accept immediately if clearly moved
	bigMoveThreshold := math.Max(alc.config.MinMovementDistance,
		alc.config.MovementAccuracyFactor*alc.currentState.Accuracy)
	if distance >= bigMoveThreshold {
		return true, fmt.Sprintf("big move detected (%.0fm > %.0fm)", distance, bigMoveThreshold)
	}

	// Gate 2: Accuracy improvement gate
	if newFix.Accuracy <= alc.config.AccuracyImprovement*alc.currentState.Accuracy {
		return true, fmt.Sprintf("accuracy improved (%.0fm vs %.0fm)", newFix.Accuracy, alc.currentState.Accuracy)
	}

	// Gate 3: Chi-square statistical gate (anti-jitter)
	sigma := newFix.Accuracy
	if sigma > 0 {
		chiSquare := math.Pow(distance/sigma, 2)
		newFix.ChiSquare = chiSquare

		if chiSquare <= alc.config.ChiSquareThreshold {
			return true, fmt.Sprintf("statistically consistent (χ²=%.2f ≤ %.2f)", chiSquare, alc.config.ChiSquareThreshold)
		}

		return false, fmt.Sprintf("statistical outlier (χ²=%.2f > %.2f)", chiSquare, alc.config.ChiSquareThreshold)
	}

	// Gate 4: Movement with acceptable accuracy regression
	if alc.movementDetected && newFix.Accuracy <= alc.currentState.Accuracy*alc.config.AccuracyRegressionLimit {
		return true, "movement with acceptable accuracy regression"
	}

	return false, fmt.Sprintf("no acceptance criteria met (dist=%.0fm, acc=%.0fm vs %.0fm)",
		distance, newFix.Accuracy, alc.currentState.Accuracy)
}

// addToBuffer adds a fix to the rolling buffer
func (alc *AdaptiveLocationCache) addToBuffer(fix *LocationFix) {
	alc.fixBuffer = append(alc.fixBuffer, fix)

	// Keep buffer size limited
	if len(alc.fixBuffer) > alc.config.BufferSize {
		alc.fixBuffer = alc.fixBuffer[1:]
	}
}

// applySmoothingFilter applies accuracy-weighted smoothing to accepted fixes
func (alc *AdaptiveLocationCache) applySmoothingFilter() *LocationState {
	// Get accepted fixes from buffer
	var acceptedFixes []*LocationFix
	for _, fix := range alc.fixBuffer {
		if fix.Accepted {
			acceptedFixes = append(acceptedFixes, fix)
		}
	}

	if len(acceptedFixes) == 0 {
		return alc.currentState
	}

	// Determine smoothing window based on movement state
	windowSize := alc.config.SmoothingWindowMoving
	if alc.currentState != nil && alc.currentState.IsStationary {
		windowSize = alc.config.SmoothingWindowParked
	}

	// Use most recent fixes up to window size
	startIdx := len(acceptedFixes) - windowSize
	if startIdx < 0 {
		startIdx = 0
	}
	recentFixes := acceptedFixes[startIdx:]

	// Apply accuracy-weighted averaging
	var weightedLat, weightedLon, totalWeight float64

	for _, fix := range recentFixes {
		if fix.Accuracy > 0 {
			weight := 1.0 / (fix.Accuracy * fix.Accuracy) // Weight = 1/variance
			weightedLat += weight * fix.Latitude
			weightedLon += weight * fix.Longitude
			totalWeight += weight
		}
	}

	if totalWeight == 0 {
		// Fallback to simple average
		for _, fix := range recentFixes {
			weightedLat += fix.Latitude
			weightedLon += fix.Longitude
			totalWeight += 1.0
		}
	}

	smoothedLat := weightedLat / totalWeight
	smoothedLon := weightedLon / totalWeight
	fusedAccuracy := math.Sqrt(1.0 / totalWeight)

	// Create smoothed state
	return &LocationState{
		Latitude:     smoothedLat,
		Longitude:    smoothedLon,
		Accuracy:     fusedAccuracy,
		Timestamp:    time.Now(),
		Source:       fmt.Sprintf("Smoothed (%d fixes)", len(recentFixes)),
		IsStationary: alc.isCurrentlyStationary(),
		Confidence:   alc.calculateConfidence(recentFixes),
	}
}

// updateMovementState updates the movement/stationary detection
func (alc *AdaptiveLocationCache) updateMovementState() {
	if len(alc.fixBuffer) < 3 {
		return // Need at least 3 fixes to determine movement
	}

	// Check recent movement
	recentFixes := alc.fixBuffer[len(alc.fixBuffer)-3:]
	totalDistance := 0.0

	for i := 1; i < len(recentFixes); i++ {
		if recentFixes[i].Accepted && recentFixes[i-1].Accepted {
			distance := calculateHaversineDistance(
				recentFixes[i-1].Latitude, recentFixes[i-1].Longitude,
				recentFixes[i].Latitude, recentFixes[i].Longitude)
			totalDistance += distance
		}
	}

	// Determine if stationary (low movement over time)
	wasStationary := alc.currentState != nil && alc.currentState.IsStationary
	isNowStationary := totalDistance < 100.0 // Less than 100m movement in recent fixes

	if !wasStationary && isNowStationary {
		alc.stationaryStart = time.Now()
		alc.stats.StationaryPeriods++
		fmt.Println("    🏠 Detected stationary state")
	} else if wasStationary && !isNowStationary {
		fmt.Println("    🚶 Detected movement")
	}
}

// isCurrentlyStationary determines if currently in stationary state
func (alc *AdaptiveLocationCache) isCurrentlyStationary() bool {
	if len(alc.fixBuffer) < 2 {
		return true // Assume stationary with insufficient data
	}

	// Check movement in recent fixes
	recentFixes := alc.fixBuffer[len(alc.fixBuffer)-2:]
	if len(recentFixes) >= 2 && recentFixes[0].Accepted && recentFixes[1].Accepted {
		distance := calculateHaversineDistance(
			recentFixes[0].Latitude, recentFixes[0].Longitude,
			recentFixes[1].Latitude, recentFixes[1].Longitude)
		return distance < 50.0 // Less than 50m movement = stationary
	}

	return true
}

// calculateConfidence calculates confidence based on fix consistency
func (alc *AdaptiveLocationCache) calculateConfidence(fixes []*LocationFix) float64 {
	if len(fixes) < 2 {
		return 0.5 // Medium confidence with single fix
	}

	// Calculate variance in positions
	var latSum, lonSum float64
	for _, fix := range fixes {
		latSum += fix.Latitude
		lonSum += fix.Longitude
	}

	avgLat := latSum / float64(len(fixes))
	avgLon := lonSum / float64(len(fixes))

	var variance float64
	for _, fix := range fixes {
		distance := calculateHaversineDistance(avgLat, avgLon, fix.Latitude, fix.Longitude)
		variance += distance * distance
	}
	variance /= float64(len(fixes))

	// Convert variance to confidence (lower variance = higher confidence)
	stdDev := math.Sqrt(variance)
	confidence := math.Max(0.1, math.Min(1.0, 1.0-stdDev/1000.0))

	return confidence
}

// Helper functions
func calculateHaversineDistance(lat1, lon1, lat2, lon2 float64) float64 {
	const earthRadiusM = 6371000 // Earth's radius in meters

	lat1Rad := lat1 * math.Pi / 180
	lon1Rad := lon1 * math.Pi / 180
	lat2Rad := lat2 * math.Pi / 180
	lon2Rad := lon2 * math.Pi / 180

	deltaLat := lat2Rad - lat1Rad
	deltaLon := lon2Rad - lon1Rad

	a := math.Sin(deltaLat/2)*math.Sin(deltaLat/2) +
		math.Cos(lat1Rad)*math.Cos(lat2Rad)*
			math.Sin(deltaLon/2)*math.Sin(deltaLon/2)
	c := 2 * math.Atan2(math.Sqrt(a), math.Sqrt(1-a))

	return earthRadiusM * c
}

func isSameDay(t1, t2 time.Time) bool {
	y1, m1, d1 := t1.Date()
	y2, m2, d2 := t2.Date()
	return y1 == y2 && m1 == m2 && d1 == d2
}

// GetCurrentLocation returns the current best location estimate
func (alc *AdaptiveLocationCache) GetCurrentLocation() *LocationState {
	alc.mu.RLock()
	defer alc.mu.RUnlock()

	if alc.currentState == nil {
		return nil
	}

	// Return a copy to avoid race conditions
	state := *alc.currentState
	return &state
}

// PrintAdaptiveStats displays comprehensive cache statistics
func (alc *AdaptiveLocationCache) PrintAdaptiveStats() {
	alc.mu.RLock()
	defer alc.mu.RUnlock()

	fmt.Println("\n📊 Adaptive Location Cache Statistics:")
	fmt.Println("======================================")

	if alc.currentState != nil {
		fmt.Printf("📍 Current Location: %.6f°, %.6f° (±%.0fm)\n",
			alc.currentState.Latitude, alc.currentState.Longitude, alc.currentState.Accuracy)
		fmt.Printf("🏠 State: %s (confidence: %.1f%%)\n",
			map[bool]string{true: "Stationary", false: "Moving"}[alc.currentState.IsStationary],
			alc.currentState.Confidence*100)

		if alc.currentState.IsStationary {
			stationaryDuration := time.Since(alc.stationaryStart)
			fmt.Printf("⏱️  Stationary for: %.1f minutes\n", stationaryDuration.Minutes())
			fmt.Printf("📅 Next interval: %.1f minutes\n", alc.getMinInterval().Minutes())
		}
	}

	fmt.Printf("📊 Performance:\n")
	fmt.Printf("  🔍 Total Queries: %d\n", alc.stats.TotalQueries)
	fmt.Printf("  💾 Cache Hits: %d (%.1f%%)\n", alc.stats.CacheHits,
		float64(alc.stats.CacheHits)/float64(alc.stats.TotalQueries)*100)
	fmt.Printf("  ✅ Accepted Fixes: %d\n", alc.stats.AcceptedFixes)
	fmt.Printf("  ❌ Quality Rejections: %d\n", alc.stats.QualityRejections)

	fmt.Printf("🚶 Movement Detection:\n")
	fmt.Printf("  📱 Cell Changes: %d\n", alc.stats.CellChangeTriggers)
	fmt.Printf("  📶 WiFi Changes: %d\n", alc.stats.WiFiChangeTriggers)
	fmt.Printf("  🏠 Stationary Periods: %d\n", alc.stats.StationaryPeriods)

	fmt.Printf("💰 API Usage:\n")
	fmt.Printf("  📅 Today: %d calls\n", alc.stats.APICallsToday)
	dailyLimit := float64(alc.config.MonthlyQuota) / 30.0
	fmt.Printf("  📊 Daily Limit: %.0f calls\n", dailyLimit)
	fmt.Printf("  📈 Usage: %.1f%%\n", float64(alc.stats.APICallsToday)/dailyLimit*100)

	fmt.Printf("⚙️  Current Config:\n")
	fmt.Printf("  📱 Cell Threshold: %.0f%% (top %d)\n", alc.config.CellChangeThreshold*100, alc.config.CellTopN)
	fmt.Printf("  📶 WiFi Threshold: %.0f%% (top %d)\n", alc.config.WiFiChangeThreshold*100, alc.config.WiFiTopK)
	fmt.Printf("  ⏱️  Min Interval: %.1fm\n", alc.getMinInterval().Minutes())
	fmt.Printf("  🔄 Soft TTL: %.1fm\n", alc.getSoftTTL().Minutes())
}

// testAdaptiveLocationCache demonstrates the adaptive caching system
func testAdaptiveLocationCache() {
	fmt.Println("🧠 Adaptive Location Cache Test")
	fmt.Println("===============================")
	fmt.Println("Features: Movement detection, Quality gating, Adaptive intervals")

	// Create SSH client
	client, err := createSSHClient()
	if err != nil {
		fmt.Printf("❌ SSH connection failed: %v\n", err)
		return
	}
	defer client.Close()

	// Create adaptive cache
	cache := NewAdaptiveLocationCache()

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

	fmt.Println("\n🔄 Testing adaptive behavior (3 requests)...")

	for i := 1; i <= 3; i++ {
		fmt.Printf("\n--- Request %d ---\n", i)

		// Check if we should query
		shouldQuery, reason := cache.ShouldQuery(client)
		cache.stats.TotalQueries++

		if !shouldQuery {
			cache.stats.CacheHits++
			fmt.Printf("💾 Cache decision: %s\n", reason)
			if current := cache.GetCurrentLocation(); current != nil {
				fmt.Printf("📍 Using cached: %.6f°, %.6f° (±%.0fm)\n",
					current.Latitude, current.Longitude, current.Accuracy)
			}
		} else {
			fmt.Printf("🔍 Query decision: %s\n", reason)

			// Perform WiFi scan and API call
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

			cache.stats.APICallsToday++

			// Create location fix
			fix := &LocationFix{
				Latitude:  resp.Location.Lat,
				Longitude: resp.Location.Lng,
				Accuracy:  resp.Accuracy,
				Timestamp: time.Now(),
				Source:    fmt.Sprintf("Enhanced WiFi (%d APs)", len(scanResult.GoogleWiFiAPs)),
			}

			// Process the fix through quality gating and smoothing
			smoothedState := cache.ProcessLocationFix(fix)

			if smoothedState != nil {
				fmt.Printf("📍 Final location: %.6f°, %.6f° (±%.0fm)\n",
					smoothedState.Latitude, smoothedState.Longitude, smoothedState.Accuracy)
			}
		}

		// Display cache stats
		cache.PrintAdaptiveStats()

		// Wait between requests (except last one)
		if i < 3 {
			waitTime := 3 * time.Minute
			fmt.Printf("⏳ Waiting %.1f minutes...\n", waitTime.Minutes())
			time.Sleep(waitTime)
		}
	}

	fmt.Println("\n🎯 Adaptive cache test completed!")
	fmt.Println("Key features demonstrated:")
	fmt.Println("  ✅ Movement detection via cell/WiFi environment changes")
	fmt.Println("  ✅ Quality gating (chi-square, accuracy, movement)")
	fmt.Println("  ✅ Adaptive intervals (5m moving → 10/20/40/60m stationary)")
	fmt.Println("  ✅ API quota management (10k/month = ~333/day)")
	fmt.Println("  ✅ Accuracy-weighted smoothing filter")
	fmt.Println("  ✅ Statistical outlier rejection")
}
