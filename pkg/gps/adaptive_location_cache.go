package gps

import (
	"fmt"
	"math"
	"sync"
	"time"

	"github.com/markus-lassfolk/autonomy/pkg/logx"
)

// AdaptiveLocationCache implements intelligent caching with movement detection and quality gating
type AdaptiveLocationCache struct {
	// Configuration
	config *AdaptiveCacheConfig

	// State tracking
	currentState     *LocationState
	fixBuffer        []*LocationFix
	lastTriggerTime  time.Time
	stationaryStart  time.Time
	movementDetected bool

	// Statistics
	stats *AdaptiveCacheStats

	// Thread safety
	mu sync.RWMutex

	// Logger
	logger *logx.Logger
}

// AdaptiveCacheConfig holds all configurable parameters
type AdaptiveCacheConfig struct {
	// Trigger thresholds
	CellTopN             int     `json:"cell_top_n" default:"8"`               // Top N cells to track
	CellChangeThreshold  float64 `json:"cell_change_threshold" default:"0.35"` // 35% change threshold
	CellTopStrongChanged int     `json:"cell_top_strong_changed" default:"2"`  // Top 2 strongest changed

	WiFiTopK             int     `json:"wifi_top_k" default:"10"`              // Top K BSSIDs to track
	WiFiChangeThreshold  float64 `json:"wifi_change_threshold" default:"0.40"` // 40% change threshold
	WiFiTopStrongChanged int     `json:"wifi_top_strong_changed" default:"3"`  // Top 3 strongest changed

	// Timing controls
	DebounceTime          time.Duration `json:"debounce_time" default:"10s"`          // Change persistence required
	MinIntervalMoving     time.Duration `json:"min_interval_moving" default:"5m"`     // Hard floor when moving
	SoftTTL               time.Duration `json:"soft_ttl" default:"15m"`               // Refresh if no change
	HardTTL               time.Duration `json:"hard_ttl" default:"60m"`               // Force refresh max age
	StationaryBackoffTime time.Duration `json:"stationary_backoff_time" default:"2h"` // When to start backoff

	// Adaptive intervals when stationary
	StationaryIntervals []time.Duration `json:"stationary_intervals"` // [10m, 20m, 40m, 60m]

	// Quality gating
	AccuracyImprovement     float64 `json:"accuracy_improvement" default:"0.8"`      // Accept if 80% of old accuracy
	MinMovementDistance     float64 `json:"min_movement_distance" default:"300"`     // Minimum movement in meters
	MovementAccuracyFactor  float64 `json:"movement_accuracy_factor" default:"1.5"`  // Movement = 1.5 × accuracy
	AccuracyRegressionLimit float64 `json:"accuracy_regression_limit" default:"1.2"` // Allow 20% accuracy loss on movement
	ChiSquareThreshold      float64 `json:"chi_square_threshold" default:"5.99"`     // 95% confidence in 2D

	// Budget management
	MonthlyQuota          int           `json:"monthly_quota" default:"10000"`         // 10k free requests
	DailyQuotaPercent     float64       `json:"daily_quota_percent" default:"0.5"`     // 50% by midday
	QuotaExceededInterval time.Duration `json:"quota_exceeded_interval" default:"15m"` // Fallback interval

	// Smoothing
	BufferSize            int     `json:"buffer_size" default:"10"`             // Rolling buffer size
	SmoothingWindowMoving int     `json:"smoothing_window_moving" default:"5"`  // Fixes to smooth when moving
	SmoothingWindowParked int     `json:"smoothing_window_parked" default:"10"` // Fixes to smooth when parked
	EMAAlphaMin           float64 `json:"ema_alpha_min" default:"0.2"`          // Minimum EMA alpha
	EMAAlphaMax           float64 `json:"ema_alpha_max" default:"0.5"`          // Maximum EMA alpha
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

// AdaptiveCacheStats tracks cache performance statistics
type AdaptiveCacheStats struct {
	TotalQueries         int64         `json:"total_queries"`
	CacheHits            int64         `json:"cache_hits"`
	CacheMisses          int64         `json:"cache_misses"`
	EnvironmentChanges   int64         `json:"environment_changes"`
	DebouncedChanges     int64         `json:"debounced_changes"`
	VerifiedChanges      int64         `json:"verified_changes"`
	QualityRejections    int64         `json:"quality_rejections"`
	AcceptedFixes        int64         `json:"accepted_fixes"`
	BigMoveAcceptances   int64         `json:"big_move_acceptances"`
	StationaryDetections int64         `json:"stationary_detections"`
	AverageResponseTime  time.Duration `json:"average_response_time"`
	LastResetDate        time.Time     `json:"last_reset_date"`
}

// NewAdaptiveLocationCache creates a new adaptive cache with default configuration
func NewAdaptiveLocationCache(logger *logx.Logger) *AdaptiveLocationCache {
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
		MinMovementDistance:     300,
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
		config: config,
		stats:  &AdaptiveCacheStats{LastResetDate: time.Now()},
		logger: logger,
	}
}

// NewAdaptiveLocationCacheWithConfig creates a new adaptive cache with custom configuration
func NewAdaptiveLocationCacheWithConfig(config *AdaptiveCacheConfig, logger *logx.Logger) *AdaptiveLocationCache {
	if config == nil {
		config = &AdaptiveCacheConfig{}
	}

	return &AdaptiveLocationCache{
		config: config,
		stats:  &AdaptiveCacheStats{LastResetDate: time.Now()},
		logger: logger,
	}
}

// ShouldQuery determines if a new location query should be performed
func (alc *AdaptiveLocationCache) ShouldQuery() (bool, string) {
	alc.mu.RLock()
	defer alc.mu.RUnlock()

	alc.stats.TotalQueries++

	now := time.Now()

	// Check if we have a current state
	if alc.currentState == nil {
		alc.stats.CacheMisses++
		return true, "no_cached_location"
	}

	// Check hard TTL (force refresh)
	if now.Sub(alc.currentState.Timestamp) > alc.config.HardTTL {
		alc.stats.CacheMisses++
		return true, "hard_ttl_expired"
	}

	// Check if we're currently moving
	if alc.movementDetected {
		// When moving, use minimum interval
		if now.Sub(alc.lastTriggerTime) < alc.config.MinIntervalMoving {
			alc.stats.CacheHits++
			return false, "moving_min_interval"
		}
	} else {
		// When stationary, use adaptive intervals
		interval := alc.getMinInterval()
		if now.Sub(alc.lastTriggerTime) < interval {
			alc.stats.CacheHits++
			return false, "stationary_adaptive_interval"
		}
	}

	// Check soft TTL (refresh if no environment change)
	if now.Sub(alc.currentState.Timestamp) > alc.config.SoftTTL {
		alc.stats.CacheMisses++
		return true, "soft_ttl_expired"
	}

	// Check quota limits
	if alc.isQuotaExceeded() {
		// Use longer interval when quota exceeded
		if now.Sub(alc.lastTriggerTime) < alc.config.QuotaExceededInterval {
			alc.stats.CacheHits++
			return false, "quota_exceeded"
		}
	}

	alc.stats.CacheHits++
	return false, "cache_valid"
}

// getMinInterval returns the minimum query interval based on stationary state
func (alc *AdaptiveLocationCache) getMinInterval() time.Duration {
	if alc.currentState == nil || !alc.currentState.IsStationary {
		return alc.config.MinIntervalMoving
	}

	// Calculate how long we've been stationary
	stationaryDuration := time.Since(alc.stationaryStart)

	// Find appropriate interval based on stationary duration
	for i, interval := range alc.config.StationaryIntervals {
		if stationaryDuration < interval {
			if i > 0 {
				return alc.config.StationaryIntervals[i-1]
			}
			return alc.config.MinIntervalMoving
		}
	}

	// If we've been stationary longer than all intervals, use the longest
	return alc.config.StationaryIntervals[len(alc.config.StationaryIntervals)-1]
}

// isQuotaExceeded checks if we've exceeded the daily quota
func (alc *AdaptiveLocationCache) isQuotaExceeded() bool {
	// Simple quota check - in a real implementation, this would track actual API calls
	today := time.Now().Truncate(24 * time.Hour)
	if today.After(alc.stats.LastResetDate) {
		// Reset daily quota
		alc.stats.LastResetDate = today
		return false
	}

	// Check if we've used more than the daily percentage
	dailyQuota := int(float64(alc.config.MonthlyQuota) * alc.config.DailyQuotaPercent / 30)
	return alc.stats.TotalQueries > int64(dailyQuota)
}

// ProcessLocationFix processes a new location fix with quality gating
func (alc *AdaptiveLocationCache) ProcessLocationFix(newFix *LocationFix) *LocationState {
	alc.mu.Lock()
	defer alc.mu.Unlock()

	// Apply quality gates
	accepted, reason := alc.applyQualityGates(newFix)
	if !accepted {
		newFix.Accepted = false
		newFix.RejectReason = reason
		alc.stats.QualityRejections++
		alc.logger.Debug("Location fix rejected by quality gates", "reason", reason)
		return alc.currentState
	}

	newFix.Accepted = true
	alc.stats.AcceptedFixes++

	// Add to buffer for smoothing
	alc.addToBuffer(newFix)

	// Apply smoothing filter
	smoothedState := alc.applySmoothingFilter()

	// Update movement state
	alc.updateMovementState()

	// Update current state
	alc.currentState = smoothedState
	alc.lastTriggerTime = time.Now()

	alc.logger.Debug("Location fix processed",
		"accepted", accepted,
		"source", newFix.Source,
		"accuracy", newFix.Accuracy,
		"distance", newFix.Distance)

	return alc.currentState
}

// applyQualityGates applies quality validation to location fixes
func (alc *AdaptiveLocationCache) applyQualityGates(newFix *LocationFix) (bool, string) {
	if alc.currentState == nil {
		return true, "" // Accept first fix
	}

	// Calculate distance from current location
	distance := alc.calculateDistance(alc.currentState.Latitude, alc.currentState.Longitude, newFix.Latitude, newFix.Longitude)
	newFix.Distance = distance

	// Check for big moves (accept immediately)
	if distance > alc.config.MinMovementDistance {
		alc.stats.BigMoveAcceptances++
		alc.logger.Info("Big move detected, accepting location", "distance", distance)
		return true, ""
	}

	// Check accuracy regression
	accuracyRatio := newFix.Accuracy / alc.currentState.Accuracy
	if accuracyRatio > alc.config.AccuracyRegressionLimit {
		return false, fmt.Sprintf("accuracy_regression_too_high: %.2f > %.2f", accuracyRatio, alc.config.AccuracyRegressionLimit)
	}

	// Check for reasonable accuracy improvement
	if accuracyRatio < alc.config.AccuracyImprovement {
		return false, fmt.Sprintf("insufficient_accuracy_improvement: %.2f < %.2f", accuracyRatio, alc.config.AccuracyImprovement)
	}

	// Check Chi-square test for statistical consistency
	if alc.currentState != nil {
		chiSquare := alc.calculateChiSquare(newFix)
		newFix.ChiSquare = chiSquare
		if chiSquare > alc.config.ChiSquareThreshold {
			return false, fmt.Sprintf("chi_square_test_failed: %.2f > %.2f", chiSquare, alc.config.ChiSquareThreshold)
		}
	}

	return true, ""
}

// addToBuffer adds a fix to the rolling buffer
func (alc *AdaptiveLocationCache) addToBuffer(fix *LocationFix) {
	alc.fixBuffer = append(alc.fixBuffer, fix)
	if len(alc.fixBuffer) > alc.config.BufferSize {
		alc.fixBuffer = alc.fixBuffer[1:] // Remove oldest
	}
}

// applySmoothingFilter applies smoothing to the location data
func (alc *AdaptiveLocationCache) applySmoothingFilter() *LocationState {
	if len(alc.fixBuffer) == 0 {
		return nil
	}

	// Use different smoothing windows based on movement state
	windowSize := alc.config.SmoothingWindowParked
	if alc.movementDetected {
		windowSize = alc.config.SmoothingWindowMoving
	}

	// Limit window size to available data
	if windowSize > len(alc.fixBuffer) {
		windowSize = len(alc.fixBuffer)
	}

	// Get recent fixes for smoothing
	recentFixes := alc.fixBuffer[len(alc.fixBuffer)-windowSize:]

	// Calculate weighted average
	var totalWeight float64
	var weightedLat, weightedLon, weightedAcc float64

	for _, fix := range recentFixes {
		if !fix.Accepted {
			continue
		}

		// Weight based on accuracy (better accuracy = higher weight)
		weight := 1.0 / (fix.Accuracy + 1.0) // Avoid division by zero
		totalWeight += weight

		weightedLat += fix.Latitude * weight
		weightedLon += fix.Longitude * weight
		weightedAcc += fix.Accuracy * weight
	}

	if totalWeight == 0 {
		// Fallback to simple average
		var sumLat, sumLon, sumAcc float64
		var count int
		for _, fix := range recentFixes {
			if fix.Accepted {
				sumLat += fix.Latitude
				sumLon += fix.Longitude
				sumAcc += fix.Accuracy
				count++
			}
		}
		if count == 0 {
			return nil
		}
		weightedLat = sumLat / float64(count)
		weightedLon = sumLon / float64(count)
		weightedAcc = sumAcc / float64(count)
	} else {
		weightedLat /= totalWeight
		weightedLon /= totalWeight
		weightedAcc /= totalWeight
	}

	// Calculate confidence based on consistency
	confidence := alc.calculateConfidence(recentFixes)

	return &LocationState{
		Latitude:     weightedLat,
		Longitude:    weightedLon,
		Accuracy:     weightedAcc,
		Timestamp:    time.Now(),
		Source:       recentFixes[len(recentFixes)-1].Source,
		IsStationary: !alc.movementDetected,
		Confidence:   confidence,
	}
}

// updateMovementState updates the movement detection state
func (alc *AdaptiveLocationCache) updateMovementState() {
	if len(alc.fixBuffer) < 2 {
		return
	}

	// Check recent fixes for movement
	var totalDistance float64
	var movementCount int

	for i := 1; i < len(alc.fixBuffer); i++ {
		prev := alc.fixBuffer[i-1]
		curr := alc.fixBuffer[i]

		if prev.Accepted && curr.Accepted {
			distance := alc.calculateDistance(prev.Latitude, prev.Longitude, curr.Latitude, curr.Longitude)
			totalDistance += distance
			movementCount++
		}
	}

	if movementCount > 0 {
		avgDistance := totalDistance / float64(movementCount)
		wasMoving := alc.movementDetected
		alc.movementDetected = avgDistance > 10 // 10 meters threshold

		if !wasMoving && alc.movementDetected {
			alc.logger.Info("Movement detected")
		} else if wasMoving && !alc.movementDetected {
			alc.stationaryStart = time.Now()
			alc.stats.StationaryDetections++
			alc.logger.Info("Stationary state detected")
		}
	}
}

// calculateConfidence calculates confidence based on location consistency
func (alc *AdaptiveLocationCache) calculateConfidence(fixes []*LocationFix) float64 {
	if len(fixes) < 2 {
		return 1.0
	}

	// Calculate standard deviation of recent fixes
	var lats, lons []float64
	for _, fix := range fixes {
		if fix.Accepted {
			lats = append(lats, fix.Latitude)
			lons = append(lons, fix.Longitude)
		}
	}

	if len(lats) < 2 {
		return 1.0
	}

	// Calculate standard deviation
	latStdDev := alc.calculateStandardDeviation(lats)
	lonStdDev := alc.calculateStandardDeviation(lons)

	// Convert to meters (approximate)
	latMeters := latStdDev * 111000 // 1 degree ≈ 111km
	lonMeters := lonStdDev * 111000 * math.Cos(lats[0]*math.Pi/180)

	// Calculate confidence based on consistency
	avgAccuracy := 0.0
	for _, fix := range fixes {
		if fix.Accepted {
			avgAccuracy += fix.Accuracy
		}
	}
	avgAccuracy /= float64(len(fixes))

	// Confidence decreases with higher standard deviation relative to accuracy
	consistency := math.Max(0, 1-(latMeters+lonMeters)/(avgAccuracy*2))
	return math.Min(1.0, consistency)
}

// calculateStandardDeviation calculates standard deviation of a slice of values
func (alc *AdaptiveLocationCache) calculateStandardDeviation(values []float64) float64 {
	if len(values) < 2 {
		return 0
	}

	// Calculate mean
	sum := 0.0
	for _, v := range values {
		sum += v
	}
	mean := sum / float64(len(values))

	// Calculate variance
	variance := 0.0
	for _, v := range values {
		variance += math.Pow(v-mean, 2)
	}
	variance /= float64(len(values) - 1)

	return math.Sqrt(variance)
}

// calculateDistance calculates distance between two points
func (alc *AdaptiveLocationCache) calculateDistance(lat1, lon1, lat2, lon2 float64) float64 {
	const earthRadius = 6371000 // meters

	lat1Rad := lat1 * math.Pi / 180
	lat2Rad := lat2 * math.Pi / 180
	deltaLat := (lat2 - lat1) * math.Pi / 180
	deltaLon := (lon2 - lon1) * math.Pi / 180

	a := math.Sin(deltaLat/2)*math.Sin(deltaLat/2) +
		math.Cos(lat1Rad)*math.Cos(lat2Rad)*
			math.Sin(deltaLon/2)*math.Sin(deltaLon/2)
	c := 2 * math.Atan2(math.Sqrt(a), math.Sqrt(1-a))

	return earthRadius * c
}

// calculateChiSquare calculates Chi-square statistic for location consistency
func (alc *AdaptiveLocationCache) calculateChiSquare(newFix *LocationFix) float64 {
	if alc.currentState == nil {
		return 0
	}

	// Calculate expected location (current state)
	expectedLat := alc.currentState.Latitude
	expectedLon := alc.currentState.Longitude

	// Calculate observed location (new fix)
	observedLat := newFix.Latitude
	observedLon := newFix.Longitude

	// Calculate Chi-square statistic
	latDiff := (observedLat - expectedLat) / (newFix.Accuracy / 111000) // Convert accuracy to degrees
	lonDiff := (observedLon - expectedLon) / (newFix.Accuracy / 111000)

	return latDiff*latDiff + lonDiff*lonDiff
}

// GetCurrentLocation returns the current cached location state
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

// GetStats returns the current cache statistics
func (alc *AdaptiveLocationCache) GetStats() *AdaptiveCacheStats {
	alc.mu.RLock()
	defer alc.mu.RUnlock()

	stats := *alc.stats // Copy to avoid race conditions
	return &stats
}

// PrintAdaptiveStats prints adaptive cache statistics
func (alc *AdaptiveLocationCache) PrintAdaptiveStats() {
	stats := alc.GetStats()

	alc.logger.Info("Adaptive Location Cache Statistics",
		"total_queries", stats.TotalQueries,
		"cache_hits", stats.CacheHits,
		"cache_misses", stats.CacheMisses,
		"environment_changes", stats.EnvironmentChanges,
		"debounced_changes", stats.DebouncedChanges,
		"verified_changes", stats.VerifiedChanges,
		"quality_rejections", stats.QualityRejections,
		"accepted_fixes", stats.AcceptedFixes,
		"big_move_acceptances", stats.BigMoveAcceptances,
		"stationary_detections", stats.StationaryDetections,
		"average_response_time", stats.AverageResponseTime)
}

// Close gracefully shuts down the adaptive location cache
func (alc *AdaptiveLocationCache) Close() error {
	alc.logger.Info("Adaptive location cache shut down")
	return nil
}
