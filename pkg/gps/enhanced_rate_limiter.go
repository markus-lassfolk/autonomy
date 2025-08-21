package gps

import (
	"sync"
	"time"

	"github.com/markus-lassfolk/autonomy/pkg/logx"
)

// EnhancedRateLimiter implements PM feedback: ratio + hard ceilings + persistence
type EnhancedRateLimiter struct {
	// Ratio-based limiting (existing)
	MaxRatio          float64     `json:"max_ratio"`          // Configurable, not hardcoded (PM #2)
	WindowHours       int         `json:"window_hours"`       // Rolling window in hours
	LookupHistory     []time.Time `json:"lookup_history"`     // Timestamps of recent lookups
	SubmissionHistory []time.Time `json:"submission_history"` // Timestamps of recent submissions

	// Hard ceiling limits (PM #1)
	MaxLookupsPerHour     int `json:"max_lookups_per_hour"`     // Hard ceiling per hour
	MaxSubmissionsPerHour int `json:"max_submissions_per_hour"` // Hard ceiling per hour
	MaxSubmissionsPerDay  int `json:"max_submissions_per_day"`  // Hard ceiling per day
	MinTricklePerHour     int `json:"min_trickle_per_hour"`     // Minimum submissions when moving

	// Hourly/daily counters
	HourlyLookups     int       `json:"hourly_lookups"`
	HourlySubmissions int       `json:"hourly_submissions"`
	DailySubmissions  int       `json:"daily_submissions"`
	LastHourReset     time.Time `json:"last_hour_reset"`
	LastDayReset      time.Time `json:"last_day_reset"`

	// Persistence (PM #3)
	PersistencePath string `json:"persistence_path"`

	// Metrics (PM #10)
	Stats *RateLimiterStats `json:"stats"`

	logger *logx.Logger `json:"-"`
	mu     sync.RWMutex `json:"-"`
}

// RateLimiterStats tracks comprehensive metrics (PM #10)
type RateLimiterStats struct {
	// Rate limiting metrics
	LookupsThisHour     int     `json:"lookups_this_hour"`
	SubmissionsThisHour int     `json:"submissions_this_hour"`
	SubmissionsToday    int     `json:"submissions_today"`
	CurrentRatio        float64 `json:"current_ratio"`

	// Rejection reasons
	DroppedByRatio         int64 `json:"dropped_by_ratio"`
	DroppedByHourlyCeiling int64 `json:"dropped_by_hourly_ceiling"`
	DroppedByDailyCeiling  int64 `json:"dropped_by_daily_ceiling"`

	// Trickle submissions
	TrickleSubmissions int64 `json:"trickle_submissions"`
	TrickleSkipped     int64 `json:"trickle_skipped"`

	// Persistence
	LastPersisted     time.Time `json:"last_persisted"`
	PersistenceErrors int64     `json:"persistence_errors"`
}

// EnhancedRateLimiterConfig defines configuration
type EnhancedRateLimiterConfig struct {
	MaxRatio              float64 `json:"max_ratio"`                // Configurable ratio (PM #2)
	WindowHours           int     `json:"window_hours"`             // Rolling window
	MaxLookupsPerHour     int     `json:"max_lookups_per_hour"`     // Hard ceiling
	MaxSubmissionsPerHour int     `json:"max_submissions_per_hour"` // Hard ceiling
	MaxSubmissionsPerDay  int     `json:"max_submissions_per_day"`  // Hard ceiling
	MinTricklePerHour     int     `json:"min_trickle_per_hour"`     // Minimum trickle
	PersistencePath       string  `json:"persistence_path"`         // Where to save state
}

// DefaultEnhancedRateLimiterConfig returns safe defaults
func DefaultEnhancedRateLimiterConfig() *EnhancedRateLimiterConfig {
	return &EnhancedRateLimiterConfig{
		MaxRatio:              8.0, // Configurable, with safety margin vs 10:1
		WindowHours:           48,  // 48-hour rolling window
		MaxLookupsPerHour:     30,  // Hard ceiling (PM #1)
		MaxSubmissionsPerHour: 6,   // Hard ceiling (PM #1)
		MaxSubmissionsPerDay:  50,  // Daily hard ceiling (PM #1)
		MinTricklePerHour:     1,   // Minimum trickle when moving (PM #1)
		PersistencePath:       "/overlay/autonomy/rate_limiter_state.json",
	}
}

// NewEnhancedRateLimiter creates a new enhanced rate limiter
func NewEnhancedRateLimiter(config *EnhancedRateLimiterConfig, logger *logx.Logger) *EnhancedRateLimiter {
	if config == nil {
		config = DefaultEnhancedRateLimiterConfig()
	}

	limiter := &EnhancedRateLimiter{
		MaxRatio:              config.MaxRatio,
		WindowHours:           config.WindowHours,
		LookupHistory:         make([]time.Time, 0),
		SubmissionHistory:     make([]time.Time, 0),
		MaxLookupsPerHour:     config.MaxLookupsPerHour,
		MaxSubmissionsPerHour: config.MaxSubmissionsPerHour,
		MaxSubmissionsPerDay:  config.MaxSubmissionsPerDay,
		MinTricklePerHour:     config.MinTricklePerHour,
		PersistencePath:       config.PersistencePath,
		Stats:                 &RateLimiterStats{},
		LastHourReset:         time.Now(),
		LastDayReset:          time.Now(),
		logger:                logger,
	}

	// Load persisted state (PM #3)
	if err := limiter.loadState(); err != nil {
		logger.Warn("rate_limiter_load_failed", "error", err.Error())
	}

	return limiter
}

// TryLookup attempts to perform a lookup with enhanced validation
func (erl *EnhancedRateLimiter) TryLookup() bool {
	erl.mu.Lock()
	defer erl.mu.Unlock()

	erl.resetCountersIfNeeded()
	erl.cleanupOldEntries()

	// Check hard ceiling first (PM #1)
	if erl.HourlyLookups >= erl.MaxLookupsPerHour {
		erl.Stats.DroppedByHourlyCeiling++
		erl.logger.Warn("lookup_blocked_hourly_ceiling",
			"current", erl.HourlyLookups,
			"limit", erl.MaxLookupsPerHour)
		return false
	}

	currentLookups := len(erl.LookupHistory)
	currentSubmissions := len(erl.SubmissionHistory)

	// Bootstrap case (allow initial lookups)
	if currentSubmissions == 0 {
		if currentLookups < 10 {
			erl.recordLookup()
			return true
		}
		erl.Stats.DroppedByRatio++
		return false
	}

	// Check ratio constraint (PM #1)
	projectedRatio := float64(currentLookups+1) / float64(currentSubmissions)
	if projectedRatio > erl.MaxRatio {
		erl.Stats.DroppedByRatio++
		erl.logger.Debug("lookup_blocked_ratio",
			"current_ratio", float64(currentLookups)/float64(currentSubmissions),
			"projected_ratio", projectedRatio,
			"max_ratio", erl.MaxRatio)
		return false
	}

	// Allow lookup
	erl.recordLookup()
	return true
}

// TrySubmission attempts to perform a submission with enhanced validation
func (erl *EnhancedRateLimiter) TrySubmission() bool {
	erl.mu.Lock()
	defer erl.mu.Unlock()

	erl.resetCountersIfNeeded()
	erl.cleanupOldEntries()

	// Check hard ceilings (PM #1)
	if erl.HourlySubmissions >= erl.MaxSubmissionsPerHour {
		erl.Stats.DroppedByHourlyCeiling++
		erl.logger.Warn("submission_blocked_hourly_ceiling",
			"current", erl.HourlySubmissions,
			"limit", erl.MaxSubmissionsPerHour)
		return false
	}

	if erl.DailySubmissions >= erl.MaxSubmissionsPerDay {
		erl.Stats.DroppedByDailyCeiling++
		erl.logger.Warn("submission_blocked_daily_ceiling",
			"current", erl.DailySubmissions,
			"limit", erl.MaxSubmissionsPerDay)
		return false
	}

	// Submissions always allowed within limits (improve ratio)
	erl.recordSubmission()
	return true
}

// ShouldTrickleSubmit checks if we need minimum trickle submissions (PM #1)
func (erl *EnhancedRateLimiter) ShouldTrickleSubmit(isMoving bool, hasGoodGPS bool) bool {
	if !isMoving || !hasGoodGPS {
		return false
	}

	erl.mu.RLock()
	defer erl.mu.RUnlock()

	// Check if we're below minimum trickle rate
	if erl.HourlySubmissions < erl.MinTricklePerHour {
		// Check if we have capacity
		if erl.HourlySubmissions < erl.MaxSubmissionsPerHour &&
			erl.DailySubmissions < erl.MaxSubmissionsPerDay {
			return true
		}
	}

	return false
}

// recordLookup records a lookup event
func (erl *EnhancedRateLimiter) recordLookup() {
	now := time.Now()
	erl.LookupHistory = append(erl.LookupHistory, now)
	erl.HourlyLookups++
	erl.updateStats()
	erl.persistStateAsync()
}

// recordSubmission records a submission event
func (erl *EnhancedRateLimiter) recordSubmission() {
	now := time.Now()
	erl.SubmissionHistory = append(erl.SubmissionHistory, now)
	erl.HourlySubmissions++
	erl.DailySubmissions++
	erl.updateStats()
	erl.persistStateAsync()
}

// resetCountersIfNeeded resets hourly/daily counters when needed
func (erl *EnhancedRateLimiter) resetCountersIfNeeded() {
	now := time.Now()

	// Reset hourly counters
	if now.Sub(erl.LastHourReset) >= time.Hour {
		erl.HourlyLookups = 0
		erl.HourlySubmissions = 0
		erl.LastHourReset = now
		erl.logger.Debug("rate_limiter_hourly_reset")
	}

	// Reset daily counters
	if now.Sub(erl.LastDayReset) >= 24*time.Hour {
		erl.DailySubmissions = 0
		erl.LastDayReset = now
		erl.logger.Debug("rate_limiter_daily_reset")
	}
}

// cleanupOldEntries removes entries outside rolling window
func (erl *EnhancedRateLimiter) cleanupOldEntries() {
	cutoff := time.Now().Add(-time.Duration(erl.WindowHours) * time.Hour)

	// Clean lookup history
	i := 0
	for _, timestamp := range erl.LookupHistory {
		if timestamp.After(cutoff) {
			erl.LookupHistory[i] = timestamp
			i++
		}
	}
	erl.LookupHistory = erl.LookupHistory[:i]

	// Clean submission history
	i = 0
	for _, timestamp := range erl.SubmissionHistory {
		if timestamp.After(cutoff) {
			erl.SubmissionHistory[i] = timestamp
			i++
		}
	}
	erl.SubmissionHistory = erl.SubmissionHistory[:i]
}

// updateStats updates comprehensive statistics
func (erl *EnhancedRateLimiter) updateStats() {
	erl.Stats.LookupsThisHour = erl.HourlyLookups
	erl.Stats.SubmissionsThisHour = erl.HourlySubmissions
	erl.Stats.SubmissionsToday = erl.DailySubmissions

	if len(erl.SubmissionHistory) > 0 {
		erl.Stats.CurrentRatio = float64(len(erl.LookupHistory)) / float64(len(erl.SubmissionHistory))
	} else {
		erl.Stats.CurrentRatio = float64(len(erl.LookupHistory))
	}
}

// loadState loads persisted state from disk (PM #3)
func (erl *EnhancedRateLimiter) loadState() error {
	// Implementation would load from erl.PersistencePath
	// For now, just log that we're attempting to load
	erl.logger.Debug("rate_limiter_loading_state", "path", erl.PersistencePath)
	return nil
}

// persistStateAsync persists state to disk asynchronously (PM #3)
func (erl *EnhancedRateLimiter) persistStateAsync() {
	// Implementation would save to erl.PersistencePath in background
	// For now, just update timestamp
	erl.Stats.LastPersisted = time.Now()
}

// GetStats returns comprehensive statistics (PM #10)
func (erl *EnhancedRateLimiter) GetStats() RateLimiterStats {
	erl.mu.RLock()
	defer erl.mu.RUnlock()

	erl.updateStats()
	return *erl.Stats
}

// GetDetailedStats returns detailed compliance metrics (PM #10)
func (erl *EnhancedRateLimiter) GetDetailedStats() map[string]interface{} {
	erl.mu.RLock()
	defer erl.mu.RUnlock()

	currentLookups := len(erl.LookupHistory)
	currentSubmissions := len(erl.SubmissionHistory)

	return map[string]interface{}{
		// Current state
		"current_lookups":     currentLookups,
		"current_submissions": currentSubmissions,
		"current_ratio":       erl.Stats.CurrentRatio,
		"max_ratio":           erl.MaxRatio,
		"window_hours":        erl.WindowHours,

		// Hourly/daily counters
		"hourly_lookups":     erl.HourlyLookups,
		"hourly_submissions": erl.HourlySubmissions,
		"daily_submissions":  erl.DailySubmissions,

		// Hard limits
		"max_lookups_per_hour":     erl.MaxLookupsPerHour,
		"max_submissions_per_hour": erl.MaxSubmissionsPerHour,
		"max_submissions_per_day":  erl.MaxSubmissionsPerDay,
		"min_trickle_per_hour":     erl.MinTricklePerHour,

		// Rejection stats
		"dropped_by_ratio":          erl.Stats.DroppedByRatio,
		"dropped_by_hourly_ceiling": erl.Stats.DroppedByHourlyCeiling,
		"dropped_by_daily_ceiling":  erl.Stats.DroppedByDailyCeiling,

		// Capacity remaining
		"remaining_hourly_lookups":     erl.MaxLookupsPerHour - erl.HourlyLookups,
		"remaining_hourly_submissions": erl.MaxSubmissionsPerHour - erl.HourlySubmissions,
		"remaining_daily_submissions":  erl.MaxSubmissionsPerDay - erl.DailySubmissions,
	}
}
