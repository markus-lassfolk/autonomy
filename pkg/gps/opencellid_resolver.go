package gps

import (
	"context"
	"encoding/json"
	"fmt"
	"net/http"
	"net/url"
	"strconv"
	"strings"
	"sync"
	"time"

	"github.com/markus-lassfolk/autonomy/pkg/logx"
)

// OpenCellIDResolver resolves cell tower locations using OpenCellID API with intelligent caching
type OpenCellIDResolver struct {
	logger           *logx.Logger
	httpClient       *http.Client
	config           *OpenCellIDGPSConfig
	cache            *EnhancedIntelligentCellCache
	apiStats         *APIStats
	rateLimiter      *RateLimiter           // Legacy rate limiter for backward compatibility
	dualRateLimiter  *DualRateLimiter       // Legacy dual rate limiter
	ratioRateLimiter *RatioBasedRateLimiter // New ratio-based rate limiter (preferred)
}

// CellIdentifier represents a unique cell tower identifier
type CellIdentifier struct {
	MCC    string `json:"mcc"`    // Mobile Country Code
	MNC    string `json:"mnc"`    // Mobile Network Code
	LAC    string `json:"lac"`    // Location Area Code / Tracking Area Code
	CellID string `json:"cellid"` // Cell ID / eNodeB ID / gNodeB ID
	Radio  string `json:"radio"`  // GSM, UMTS, LTE, NR
}

// TowerLocation represents a resolved cell tower location
type TowerLocation struct {
	CellID      CellIdentifier `json:"cell_id"`
	Latitude    float64        `json:"latitude"`
	Longitude   float64        `json:"longitude"`
	Range       float64        `json:"range"`      // Accuracy radius in meters
	Samples     int            `json:"samples"`    // Number of measurements
	Confidence  float64        `json:"confidence"` // 0.0-1.0
	Source      string         `json:"source"`     // "cache", "opencellid"
	Changeable  bool           `json:"changeable"` // Whether position can be updated
	LastUpdated time.Time      `json:"last_updated"`
}

// APIStats tracks OpenCellID API usage statistics
type APIStats struct {
	mu              sync.RWMutex
	TotalRequests   int           `json:"total_requests"`
	CacheHits       int           `json:"cache_hits"`
	CacheMisses     int           `json:"cache_misses"`
	APIErrors       int           `json:"api_errors"`
	RateLimitHits   int           `json:"rate_limit_hits"`
	AvgResponseTime time.Duration `json:"avg_response_time"`
	LastAPICall     time.Time     `json:"last_api_call"`
	QuotaRemaining  int           `json:"quota_remaining"`
	QuotaResetTime  time.Time     `json:"quota_reset_time"`
}

// RateLimiter implements exponential backoff for API requests
type RateLimiter struct {
	mu           sync.Mutex
	lastRequest  time.Time
	backoffDelay time.Duration
	maxDelay     time.Duration
}

// TokenBucket implements a token bucket rate limiter
type TokenBucket struct {
	Tokens     int           `json:"tokens"`      // Current token count
	Capacity   int           `json:"capacity"`    // Maximum tokens
	RefillRate time.Duration `json:"refill_rate"` // Time between token refills
	LastRefill time.Time     `json:"last_refill"` // Last refill time
	mu         sync.Mutex    `json:"-"`           // Mutex for thread safety
}

// NewTokenBucket creates a new token bucket
func NewTokenBucket(capacity int, refillRate time.Duration) *TokenBucket {
	return &TokenBucket{
		Tokens:     capacity,
		Capacity:   capacity,
		RefillRate: refillRate,
		LastRefill: time.Now(),
	}
}

// TryConsume attempts to consume a token, returns true if successful
func (tb *TokenBucket) TryConsume() bool {
	tb.mu.Lock()
	defer tb.mu.Unlock()

	// Refill tokens based on elapsed time
	now := time.Now()
	elapsed := now.Sub(tb.LastRefill)
	tokensToAdd := int(elapsed / tb.RefillRate)

	if tokensToAdd > 0 {
		tb.Tokens = minInt(tb.Capacity, tb.Tokens+tokensToAdd)
		tb.LastRefill = now
	}

	// Try to consume a token
	if tb.Tokens > 0 {
		tb.Tokens--
		return true
	}

	return false
}

// GetStatus returns current bucket status
func (tb *TokenBucket) GetStatus() (current, capacity int, nextRefill time.Duration) {
	tb.mu.Lock()
	defer tb.mu.Unlock()

	now := time.Now()
	elapsed := now.Sub(tb.LastRefill)
	timeToNext := tb.RefillRate - elapsed

	if timeToNext < 0 {
		timeToNext = 0
	}

	return tb.Tokens, tb.Capacity, timeToNext
}

// minInt returns the minimum of two integers
func minInt(a, b int) int {
	if a < b {
		return a
	}
	return b
}

// RatioBasedRateLimiter manages rate limits based on lookup:submission ratio over rolling window
type RatioBasedRateLimiter struct {
	MaxRatio          float64      `json:"max_ratio"`          // Max lookups per submission (e.g., 8.0)
	WindowHours       int          `json:"window_hours"`       // Rolling window in hours (e.g., 48)
	LookupHistory     []time.Time  `json:"lookup_history"`     // Timestamps of recent lookups
	SubmissionHistory []time.Time  `json:"submission_history"` // Timestamps of recent submissions
	logger            *logx.Logger `json:"-"`
	mu                sync.RWMutex `json:"-"`
}

// NewRatioBasedRateLimiter creates a new ratio-based rate limiter
func NewRatioBasedRateLimiter(maxRatio float64, windowHours int, logger *logx.Logger) *RatioBasedRateLimiter {
	return &RatioBasedRateLimiter{
		MaxRatio:          maxRatio,
		WindowHours:       windowHours,
		LookupHistory:     make([]time.Time, 0),
		SubmissionHistory: make([]time.Time, 0),
		logger:            logger,
	}
}

// cleanupOldEntries removes entries outside the rolling window
func (rrl *RatioBasedRateLimiter) cleanupOldEntries() {
	cutoff := time.Now().Add(-time.Duration(rrl.WindowHours) * time.Hour)

	// Clean lookup history
	i := 0
	for _, timestamp := range rrl.LookupHistory {
		if timestamp.After(cutoff) {
			rrl.LookupHistory[i] = timestamp
			i++
		}
	}
	rrl.LookupHistory = rrl.LookupHistory[:i]

	// Clean submission history
	i = 0
	for _, timestamp := range rrl.SubmissionHistory {
		if timestamp.After(cutoff) {
			rrl.SubmissionHistory[i] = timestamp
			i++
		}
	}
	rrl.SubmissionHistory = rrl.SubmissionHistory[:i]
}

// TryLookup attempts to perform a lookup, checking ratio constraints
func (rrl *RatioBasedRateLimiter) TryLookup() bool {
	rrl.mu.Lock()
	defer rrl.mu.Unlock()

	rrl.cleanupOldEntries()

	currentLookups := len(rrl.LookupHistory)
	currentSubmissions := len(rrl.SubmissionHistory)

	// If no submissions yet, allow some initial lookups (bootstrap case)
	if currentSubmissions == 0 {
		if currentLookups < 10 { // Allow up to 10 initial lookups
			rrl.LookupHistory = append(rrl.LookupHistory, time.Now())
			rrl.logger.Debug("lookup_allowed_bootstrap",
				"current_lookups", currentLookups+1,
				"reason", "no_submissions_yet")
			return true
		}
		rrl.logger.Warn("lookup_rate_limit_exceeded",
			"reason", "bootstrap_limit_reached",
			"current_lookups", currentLookups,
			"max_bootstrap", 10)
		return false
	}

	// Check if adding this lookup would exceed the ratio
	projectedRatio := float64(currentLookups+1) / float64(currentSubmissions)
	if projectedRatio > rrl.MaxRatio {
		rrl.logger.Warn("lookup_rate_limit_exceeded",
			"current_lookups", currentLookups,
			"current_submissions", currentSubmissions,
			"current_ratio", float64(currentLookups)/float64(currentSubmissions),
			"projected_ratio", projectedRatio,
			"max_ratio", rrl.MaxRatio,
			"window_hours", rrl.WindowHours)
		return false
	}

	// Allow the lookup
	rrl.LookupHistory = append(rrl.LookupHistory, time.Now())
	rrl.logger.Debug("lookup_allowed",
		"current_lookups", currentLookups+1,
		"current_submissions", currentSubmissions,
		"current_ratio", float64(currentLookups+1)/float64(currentSubmissions),
		"max_ratio", rrl.MaxRatio)
	return true
}

// TrySubmission attempts to perform a submission (always allowed, improves ratio)
func (rrl *RatioBasedRateLimiter) TrySubmission() bool {
	rrl.mu.Lock()
	defer rrl.mu.Unlock()

	rrl.cleanupOldEntries()

	// Submissions are always allowed as they improve the ratio
	rrl.SubmissionHistory = append(rrl.SubmissionHistory, time.Now())

	currentLookups := len(rrl.LookupHistory)
	currentSubmissions := len(rrl.SubmissionHistory)

	rrl.logger.Debug("submission_allowed",
		"current_lookups", currentLookups,
		"current_submissions", currentSubmissions,
		"current_ratio", float64(currentLookups)/float64(currentSubmissions),
		"max_ratio", rrl.MaxRatio)

	return true
}

// GetStats returns current rate limiter statistics
func (rrl *RatioBasedRateLimiter) GetStats() map[string]interface{} {
	rrl.mu.RLock()
	defer rrl.mu.RUnlock()

	rrl.cleanupOldEntries()

	currentLookups := len(rrl.LookupHistory)
	currentSubmissions := len(rrl.SubmissionHistory)

	var currentRatio float64
	if currentSubmissions > 0 {
		currentRatio = float64(currentLookups) / float64(currentSubmissions)
	} else {
		currentRatio = float64(currentLookups) // When no submissions, ratio is just lookup count
	}

	// Calculate remaining lookup capacity
	var remainingLookups int
	if currentSubmissions > 0 {
		maxAllowedLookups := int(float64(currentSubmissions) * rrl.MaxRatio)
		remainingLookups = maxAllowedLookups - currentLookups
		if remainingLookups < 0 {
			remainingLookups = 0
		}
	} else {
		remainingLookups = 10 - currentLookups // Bootstrap limit
		if remainingLookups < 0 {
			remainingLookups = 0
		}
	}

	return map[string]interface{}{
		"current_lookups":     currentLookups,
		"current_submissions": currentSubmissions,
		"current_ratio":       currentRatio,
		"max_ratio":           rrl.MaxRatio,
		"remaining_lookups":   remainingLookups,
		"window_hours":        rrl.WindowHours,
		"window_start":        time.Now().Add(-time.Duration(rrl.WindowHours) * time.Hour).Format(time.RFC3339),
	}
}

// DualRateLimiter manages separate rate limits for lookups and submissions (legacy)
type DualRateLimiter struct {
	LookupBucket     *TokenBucket `json:"lookup_bucket"`
	SubmissionBucket *TokenBucket `json:"submission_bucket"`
	logger           *logx.Logger `json:"-"`
}

// NewDualRateLimiter creates a new dual rate limiter
func NewDualRateLimiter(lookupsPerDay, submissionsPerDay int, logger *logx.Logger) *DualRateLimiter {
	// Calculate refill rates (tokens per day -> duration per token)
	lookupRefillRate := 24 * time.Hour / time.Duration(lookupsPerDay)
	submissionRefillRate := 24 * time.Hour / time.Duration(submissionsPerDay)

	return &DualRateLimiter{
		LookupBucket:     NewTokenBucket(lookupsPerDay, lookupRefillRate),
		SubmissionBucket: NewTokenBucket(submissionsPerDay, submissionRefillRate),
		logger:           logger,
	}
}

// TryLookup attempts to consume a lookup token
func (drl *DualRateLimiter) TryLookup() bool {
	allowed := drl.LookupBucket.TryConsume()
	if !allowed {
		current, capacity, nextRefill := drl.LookupBucket.GetStatus()
		drl.logger.Warn("lookup_rate_limit_exceeded",
			"current_tokens", current,
			"capacity", capacity,
			"next_refill_in", nextRefill.String(),
		)
	}
	return allowed
}

// TrySubmission attempts to consume a submission token
func (drl *DualRateLimiter) TrySubmission() bool {
	allowed := drl.SubmissionBucket.TryConsume()
	if !allowed {
		current, capacity, nextRefill := drl.SubmissionBucket.GetStatus()
		drl.logger.Warn("submission_rate_limit_exceeded",
			"current_tokens", current,
			"capacity", capacity,
			"next_refill_in", nextRefill.String(),
		)
	}
	return allowed
}

// GetStats returns rate limiter statistics
func (drl *DualRateLimiter) GetStats() map[string]interface{} {
	lookupCurrent, lookupCapacity, lookupNext := drl.LookupBucket.GetStatus()
	submissionCurrent, submissionCapacity, submissionNext := drl.SubmissionBucket.GetStatus()

	return map[string]interface{}{
		"lookup_tokens":          lookupCurrent,
		"lookup_capacity":        lookupCapacity,
		"lookup_next_refill":     lookupNext.String(),
		"submission_tokens":      submissionCurrent,
		"submission_capacity":    submissionCapacity,
		"submission_next_refill": submissionNext.String(),
	}
}

// OpenCellIDAPIResponse represents the API response structure
type OpenCellIDAPIResponse struct {
	Lat                   float64 `json:"lat"`
	Lon                   float64 `json:"lon"`
	MCC                   int     `json:"mcc"`
	MNC                   int     `json:"mnc"`
	LAC                   int     `json:"lac"`
	CellID                int     `json:"cellid"`
	AverageSignalStrength int     `json:"averageSignalStrength"`
	Range                 int     `json:"range"`
	Samples               int     `json:"samples"`
	Changeable            bool    `json:"changeable"`
	Radio                 string  `json:"radio"`
	Error                 string  `json:"error,omitempty"`
	Message               string  `json:"message,omitempty"`
}

// NewOpenCellIDResolver creates a new OpenCellID resolver
func NewOpenCellIDResolver(config *OpenCellIDGPSConfig, logger *logx.Logger) *OpenCellIDResolver {
	resolver := &OpenCellIDResolver{
		logger: logger,
		config: config,
		httpClient: &http.Client{
			Timeout: 30 * time.Second,
		},
		apiStats: &APIStats{},
		rateLimiter: &RateLimiter{
			maxDelay: 5 * time.Minute,
		},
		dualRateLimiter:  NewDualRateLimiter(120, 12, logger),                                          // Legacy: 120 lookups/day, 12 submissions/day
		ratioRateLimiter: NewRatioBasedRateLimiter(config.RatioLimit, config.RatioWindowHours, logger), // Configurable ratio-based limiting
	}

	// Initialize enhanced intelligent cache
	cacheConfig := &EnhancedCellCacheConfig{
		MaxSizeMB:           config.CacheSizeMB,
		NegativeTTLHours:    config.NegativeCacheTTLHours,
		CompressionEnabled:  true,
		EvictionPolicy:      "lru",
		PersistencePath:     "/overlay/autonomy/opencellid_cache.db",
		SyncIntervalSeconds: 300,
		MaxEntriesPerBucket: 10000,
	}

	var err error
	resolver.cache, err = NewEnhancedIntelligentCellCache(cacheConfig, logger)
	if err != nil {
		logger.Error("failed_to_initialize_opencellid_cache",
			"error", err.Error(),
			"fallback", "nil_cache",
		)
		// Set cache to nil - we'll handle this gracefully
		resolver.cache = nil
	}

	logger.Info("opencellid_resolver_initialized",
		"cache_size_mb", config.CacheSizeMB,
		"max_cells_per_lookup", config.MaxCellsPerLookup,
		"negative_ttl_hours", config.NegativeCacheTTLHours,
	)

	return resolver
}

// ResolveCells resolves multiple cell tower locations in batch
func (ocr *OpenCellIDResolver) ResolveCells(ctx context.Context, cells []CellIdentifier) ([]TowerLocation, error) {
	if len(cells) == 0 {
		return nil, fmt.Errorf("no cells to resolve")
	}

	// Limit the number of cells per lookup
	if len(cells) > ocr.config.MaxCellsPerLookup {
		cells = cells[:ocr.config.MaxCellsPerLookup]
		ocr.logger.LogDebugVerbose("opencellid_cells_limited", map[string]interface{}{
			"requested":  len(cells),
			"limited_to": ocr.config.MaxCellsPerLookup,
		})
	}

	var results []TowerLocation
	var cellsToResolve []CellIdentifier

	// Check cache first
	for _, cell := range cells {
		if cached := ocr.checkCache(cell); cached != nil {
			results = append(results, *cached)
			ocr.updateStats(true, false, 0) // Cache hit
		} else {
			cellsToResolve = append(cellsToResolve, cell)
		}
	}

	// Resolve remaining cells via API
	if len(cellsToResolve) > 0 {
		apiResults, err := ocr.resolveViaAPI(ctx, cellsToResolve)
		if err != nil {
			ocr.logger.Warn("opencellid_api_partial_failure",
				"error", err.Error(),
				"cache_results", len(results),
				"failed_cells", len(cellsToResolve),
			)
			// Continue with cache results even if API fails
		} else {
			results = append(results, apiResults...)
		}
	}

	ocr.logger.LogDebugVerbose("opencellid_resolution_complete", map[string]interface{}{
		"requested_cells": len(cells),
		"resolved_cells":  len(results),
		"cache_hits":      len(results) - len(cellsToResolve),
		"api_calls":       len(cellsToResolve),
	})

	return results, nil
}

// checkCache checks if a cell location is available in cache
func (ocr *OpenCellIDResolver) checkCache(cell CellIdentifier) *TowerLocation {
	if ocr.cache == nil {
		return nil // No cache available
	}

	cached, err := ocr.cache.Get(cell)
	if err != nil {
		ocr.logger.LogDebugVerbose("opencellid_cache_error", map[string]interface{}{
			"cell":  cell,
			"error": err.Error(),
		})
		return nil
	}

	if cached == nil {
		return nil
	}

	// Check if cached entry is still valid
	if cached.IsNegative && time.Since(cached.CachedAt) > time.Duration(ocr.config.NegativeCacheTTLHours)*time.Hour {
		// Negative cache entry expired
		if err := ocr.cache.Delete(cell); err != nil {
			ocr.logger.Warn("Failed to delete expired negative cache entry", "error", err)
		}
		return nil
	}

	if cached.IsNegative {
		// Valid negative cache entry
		return nil
	}

	// Convert cached entry to TowerLocation
	return &TowerLocation{
		CellID:      cell,
		Latitude:    cached.Latitude,
		Longitude:   cached.Longitude,
		Range:       cached.Range,
		Samples:     cached.Samples,
		Confidence:  cached.Confidence,
		Source:      "cache",
		Changeable:  cached.Changeable,
		LastUpdated: cached.CachedAt,
	}
}

// resolveViaAPI resolves cells using OpenCellID API
func (ocr *OpenCellIDResolver) resolveViaAPI(ctx context.Context, cells []CellIdentifier) ([]TowerLocation, error) {
	if ocr.config.APIKey == "" {
		return nil, fmt.Errorf("OpenCellID API key not configured")
	}

	// Check ratio-based rate limit (preferred)
	if ocr.ratioRateLimiter != nil && !ocr.ratioRateLimiter.TryLookup() {
		ocr.logger.Debug("lookup_rate_limited", "reason", "ratio_limit_exceeded")
		return nil, fmt.Errorf("lookup rate limit exceeded (ratio-based)")
	}

	// Apply legacy rate limiting (for backward compatibility)
	if err := ocr.rateLimiter.Wait(ctx); err != nil {
		return nil, fmt.Errorf("rate limiter error: %w", err)
	}

	var results []TowerLocation

	// Resolve cells individually (OpenCellID doesn't support batch lookup)
	for _, cell := range cells {
		location, err := ocr.resolveSingleCell(ctx, cell)
		if err != nil {
			ocr.logger.LogDebugVerbose("opencellid_single_cell_failed", map[string]interface{}{
				"cell":  cell,
				"error": err.Error(),
			})

			// Cache negative result
			ocr.cacheNegativeResult(cell)
			ocr.updateStats(false, true, 0) // API error
			continue
		}

		results = append(results, *location)

		// Cache positive result
		ocr.cachePositiveResult(cell, location)
		ocr.updateStats(false, false, 1) // API success
	}

	return results, nil
}

// resolveSingleCell resolves a single cell via OpenCellID API
func (ocr *OpenCellIDResolver) resolveSingleCell(ctx context.Context, cell CellIdentifier) (*TowerLocation, error) {
	start := time.Now()

	// Parse cell identifiers
	mcc, err := strconv.Atoi(cell.MCC)
	if err != nil {
		return nil, fmt.Errorf("invalid MCC: %s", cell.MCC)
	}

	mnc, err := strconv.Atoi(cell.MNC)
	if err != nil {
		return nil, fmt.Errorf("invalid MNC: %s", cell.MNC)
	}

	lac, err := strconv.Atoi(cell.LAC)
	if err != nil {
		return nil, fmt.Errorf("invalid LAC: %s", cell.LAC)
	}

	cellID, err := strconv.Atoi(cell.CellID)
	if err != nil {
		return nil, fmt.Errorf("invalid CellID: %s", cell.CellID)
	}

	// Build request URL
	params := url.Values{}
	params.Set("key", ocr.config.APIKey)
	params.Set("mcc", strconv.Itoa(mcc))
	params.Set("mnc", strconv.Itoa(mnc))
	params.Set("lac", strconv.Itoa(lac))
	params.Set("cellid", strconv.Itoa(cellID))
	params.Set("format", "json")

	if cell.Radio != "" {
		params.Set("radio", strings.ToUpper(cell.Radio))
	}

	requestURL := fmt.Sprintf("https://opencellid.org/cell/get?%s", params.Encode())

	ocr.logger.LogDebugVerbose("opencellid_api_request", map[string]interface{}{
		"mcc":    mcc,
		"mnc":    mnc,
		"lac":    lac,
		"cellid": cellID,
		"radio":  cell.Radio,
	})

	// Make HTTP request
	req, err := http.NewRequestWithContext(ctx, "GET", requestURL, nil)
	if err != nil {
		return nil, fmt.Errorf("failed to create request: %w", err)
	}

	resp, err := ocr.httpClient.Do(req)
	if err != nil {
		return nil, fmt.Errorf("HTTP request failed: %w", err)
	}
	defer resp.Body.Close()

	responseTime := time.Since(start)
	ocr.updateResponseTime(responseTime)

	// Check HTTP status
	if resp.StatusCode == 429 {
		ocr.rateLimiter.BackOff()
		ocr.apiStats.mu.Lock()
		ocr.apiStats.RateLimitHits++
		ocr.apiStats.mu.Unlock()
		return nil, fmt.Errorf("rate limited by API")
	}

	if resp.StatusCode != 200 {
		return nil, fmt.Errorf("HTTP error %d", resp.StatusCode)
	}

	// Parse response
	var apiResp OpenCellIDAPIResponse
	if err := json.NewDecoder(resp.Body).Decode(&apiResp); err != nil {
		return nil, fmt.Errorf("failed to parse response: %w", err)
	}

	// Check for API errors
	if apiResp.Error != "" {
		return nil, fmt.Errorf("API error: %s", apiResp.Error)
	}

	if apiResp.Message != "" && apiResp.Lat == 0 && apiResp.Lon == 0 {
		return nil, fmt.Errorf("no location data: %s", apiResp.Message)
	}

	// Validate coordinates
	if apiResp.Lat == 0 && apiResp.Lon == 0 {
		return nil, fmt.Errorf("invalid coordinates")
	}

	// Calculate confidence based on samples and range
	confidence := ocr.calculateConfidence(apiResp.Samples, apiResp.Range)

	location := &TowerLocation{
		CellID:      cell,
		Latitude:    apiResp.Lat,
		Longitude:   apiResp.Lon,
		Range:       float64(apiResp.Range),
		Samples:     apiResp.Samples,
		Confidence:  confidence,
		Source:      "opencellid",
		Changeable:  apiResp.Changeable,
		LastUpdated: time.Now(),
	}

	ocr.logger.LogDebugVerbose("opencellid_api_success", map[string]interface{}{
		"cell":             cell,
		"latitude":         location.Latitude,
		"longitude":        location.Longitude,
		"range":            location.Range,
		"samples":          location.Samples,
		"confidence":       location.Confidence,
		"response_time_ms": responseTime.Milliseconds(),
	})

	return location, nil
}

// calculateConfidence calculates confidence score based on samples and range
func (ocr *OpenCellIDResolver) calculateConfidence(samples, rangeM int) float64 {
	if samples <= 0 {
		return 0.3 // Low confidence for no samples
	}

	// Sample factor: more samples = higher confidence
	sampleFactor := float64(samples) / 100.0
	if sampleFactor > 1.0 {
		sampleFactor = 1.0
	}

	// Range factor: smaller range = higher confidence
	rangeFactor := 1.0 - (float64(rangeM) / 10000.0)
	if rangeFactor < 0 {
		rangeFactor = 0
	}

	// Combined confidence (weighted average)
	confidence := 0.6*sampleFactor + 0.4*rangeFactor

	// Ensure minimum confidence
	if confidence < 0.1 {
		confidence = 0.1
	}

	// Ensure maximum confidence
	if confidence > 0.95 {
		confidence = 0.95
	}

	return confidence
}

// cachePositiveResult caches a successful API result
func (ocr *OpenCellIDResolver) cachePositiveResult(cell CellIdentifier, location *TowerLocation) {
	if ocr.cache == nil {
		return // No cache available
	}

	cached := &CachedCellLocation{
		CellID:     cell,
		Latitude:   location.Latitude,
		Longitude:  location.Longitude,
		Range:      location.Range,
		Samples:    location.Samples,
		Confidence: location.Confidence,
		Changeable: location.Changeable,
		Source:     "opencellid",
		CachedAt:   time.Now(),
		IsNegative: false,
	}

	if err := ocr.cache.Set(cell, cached); err != nil {
		ocr.logger.Warn("opencellid_cache_set_failed",
			"cell", cell,
			"error", err.Error(),
		)
	}
}

// cacheNegativeResult caches a failed API result
func (ocr *OpenCellIDResolver) cacheNegativeResult(cell CellIdentifier) {
	if ocr.cache == nil {
		return // No cache available
	}

	cached := &CachedCellLocation{
		CellID:     cell,
		CachedAt:   time.Now(),
		IsNegative: true,
		Source:     "opencellid_negative",
	}

	if err := ocr.cache.Set(cell, cached); err != nil {
		ocr.logger.Warn("opencellid_negative_cache_failed",
			"cell", cell,
			"error", err.Error(),
		)
	}
}

// updateStats updates API usage statistics
func (ocr *OpenCellIDResolver) updateStats(cacheHit, apiError bool, apiCalls int) {
	ocr.apiStats.mu.Lock()
	defer ocr.apiStats.mu.Unlock()

	ocr.apiStats.TotalRequests++

	if cacheHit {
		ocr.apiStats.CacheHits++
	} else {
		ocr.apiStats.CacheMisses++
	}

	if apiError {
		ocr.apiStats.APIErrors++
	}

	if apiCalls > 0 {
		ocr.apiStats.LastAPICall = time.Now()
	}
}

// updateResponseTime updates average response time using EMA
func (ocr *OpenCellIDResolver) updateResponseTime(responseTime time.Duration) {
	ocr.apiStats.mu.Lock()
	defer ocr.apiStats.mu.Unlock()

	if ocr.apiStats.AvgResponseTime == 0 {
		ocr.apiStats.AvgResponseTime = responseTime
	} else {
		// EMA with alpha = 0.1
		alpha := 0.1
		ocr.apiStats.AvgResponseTime = time.Duration(
			alpha*float64(responseTime) + (1-alpha)*float64(ocr.apiStats.AvgResponseTime),
		)
	}
}

// GetStats returns current API usage statistics
func (ocr *OpenCellIDResolver) GetStats() APIStats {
	ocr.apiStats.mu.RLock()
	defer ocr.apiStats.mu.RUnlock()

	// Create a copy without the mutex
	return APIStats{
		TotalRequests:   ocr.apiStats.TotalRequests,
		CacheHits:       ocr.apiStats.CacheHits,
		CacheMisses:     ocr.apiStats.CacheMisses,
		APIErrors:       ocr.apiStats.APIErrors,
		RateLimitHits:   ocr.apiStats.RateLimitHits,
		AvgResponseTime: ocr.apiStats.AvgResponseTime,
		LastAPICall:     ocr.apiStats.LastAPICall,
		QuotaRemaining:  ocr.apiStats.QuotaRemaining,
		QuotaResetTime:  ocr.apiStats.QuotaResetTime,
	}
}

// Wait implements rate limiting with exponential backoff
func (rl *RateLimiter) Wait(ctx context.Context) error {
	rl.mu.Lock()
	defer rl.mu.Unlock()

	if rl.backoffDelay > 0 {
		select {
		case <-time.After(rl.backoffDelay):
			// Reduce backoff after successful wait
			rl.backoffDelay = rl.backoffDelay / 2
			if rl.backoffDelay < time.Second {
				rl.backoffDelay = 0
			}
		case <-ctx.Done():
			return ctx.Err()
		}
	}

	// Minimum delay between requests
	minDelay := 200 * time.Millisecond
	if elapsed := time.Since(rl.lastRequest); elapsed < minDelay {
		select {
		case <-time.After(minDelay - elapsed):
		case <-ctx.Done():
			return ctx.Err()
		}
	}

	rl.lastRequest = time.Now()
	return nil
}

// BackOff increases the backoff delay exponentially
func (rl *RateLimiter) BackOff() {
	rl.mu.Lock()
	defer rl.mu.Unlock()

	if rl.backoffDelay == 0 {
		rl.backoffDelay = time.Second
	} else {
		rl.backoffDelay *= 2
	}

	if rl.backoffDelay > rl.maxDelay {
		rl.backoffDelay = rl.maxDelay
	}
}
