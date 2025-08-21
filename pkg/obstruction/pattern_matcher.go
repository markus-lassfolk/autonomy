package obstruction

import (
	"context"
	"fmt"
	"math"
	"sort"
	"sync"
	"time"

	"github.com/markus-lassfolk/autonomy/pkg/logx"
)

// PatternMatcher matches current conditions against learned patterns
type PatternMatcher struct {
	mu     sync.RWMutex
	logger *logx.Logger
	config *PatternMatcherConfig

	// Matching state
	activeMatches map[string]*ActiveMatch
	matchHistory  []*MatchResult
	lastMatchTime time.Time
}

// PatternMatcherConfig holds configuration for pattern matching
type PatternMatcherConfig struct {
	MatchThreshold       float64 `json:"match_threshold"`        // Minimum similarity for a match
	MaxActiveMatches     int     `json:"max_active_matches"`     // Maximum concurrent active matches
	MatchTimeoutMinutes  int     `json:"match_timeout_minutes"`  // Timeout for active matches
	HistorySize          int     `json:"history_size"`           // Number of match results to keep
	RequireLocationMatch bool    `json:"require_location_match"` // Require location similarity for matches
	RequireTimeMatch     bool    `json:"require_time_match"`     // Require time similarity for matches
	MinConfidenceLevel   float64 `json:"min_confidence_level"`   // Minimum pattern confidence to consider
}

// DefaultPatternMatcherConfig returns default configuration
func DefaultPatternMatcherConfig() *PatternMatcherConfig {
	return &PatternMatcherConfig{
		MatchThreshold:       0.7,
		MaxActiveMatches:     5,
		MatchTimeoutMinutes:  30,
		HistorySize:          100,
		RequireLocationMatch: false, // Allow matches without location
		RequireTimeMatch:     false, // Allow matches without time constraints
		MinConfidenceLevel:   0.5,
	}
}

// ActiveMatch represents an ongoing pattern match
type ActiveMatch struct {
	PatternID   string                   `json:"pattern_id"`
	PatternName string                   `json:"pattern_name"`
	StartTime   time.Time                `json:"start_time"`
	LastUpdate  time.Time                `json:"last_update"`
	Similarity  float64                  `json:"similarity"`
	Confidence  float64                  `json:"confidence"`
	Status      string                   `json:"status"` // "matching", "confirmed", "failed"
	SampleCount int                      `json:"sample_count"`
	Predictions []*ObstructionPrediction `json:"predictions"`
	Metadata    map[string]interface{}   `json:"metadata"`
}

// MatchResult represents the result of a pattern matching attempt
type MatchResult struct {
	Timestamp   time.Time              `json:"timestamp"`
	PatternID   string                 `json:"pattern_id"`
	PatternName string                 `json:"pattern_name"`
	Similarity  float64                `json:"similarity"`
	Confidence  float64                `json:"confidence"`
	Success     bool                   `json:"success"`
	Reason      string                 `json:"reason"`
	Duration    time.Duration          `json:"duration"`
	SampleCount int                    `json:"sample_count"`
	Metadata    map[string]interface{} `json:"metadata"`
}

// MatchCandidate represents a potential pattern match
type MatchCandidate struct {
	Pattern        *EnvironmentalPattern `json:"pattern"`
	Similarity     float64               `json:"similarity"`
	LocationScore  float64               `json:"location_score"`
	TimeScore      float64               `json:"time_score"`
	SignatureScore float64               `json:"signature_score"`
	OverallScore   float64               `json:"overall_score"`
	Reason         string                `json:"reason"`
}

// NewPatternMatcher creates a new pattern matcher
func NewPatternMatcher(logger *logx.Logger) *PatternMatcher {
	config := DefaultPatternMatcherConfig()

	return &PatternMatcher{
		logger:        logger,
		config:        config,
		activeMatches: make(map[string]*ActiveMatch),
		matchHistory:  make([]*MatchResult, 0, config.HistorySize),
	}
}

// FindMatches finds patterns that match current conditions
func (pm *PatternMatcher) FindMatches(ctx context.Context, patterns map[string]*EnvironmentalPattern,
	currentLocation *LocationInfo, currentTime time.Time, currentSignature *ObstructionSignature,
) ([]*MatchCandidate, error) {
	pm.mu.RLock()
	defer pm.mu.RUnlock()

	var candidates []*MatchCandidate

	for _, pattern := range patterns {
		// Skip patterns with insufficient confidence
		if pattern.Confidence < pm.config.MinConfidenceLevel {
			continue
		}

		candidate := &MatchCandidate{
			Pattern: pattern,
		}

		// Calculate location similarity
		if currentLocation != nil && pattern.Location != nil {
			candidate.LocationScore = pm.calculateLocationSimilarity(pattern.Location, currentLocation)
		} else if pm.config.RequireLocationMatch {
			continue // Skip if location match is required but not available
		} else {
			candidate.LocationScore = 1.0 // Neutral score if location not available
		}

		// Calculate time similarity
		if pattern.TimePattern != nil {
			candidate.TimeScore = pm.calculateTimeSimilarity(pattern.TimePattern, currentTime)
		} else if pm.config.RequireTimeMatch {
			continue // Skip if time match is required but pattern has no time data
		} else {
			candidate.TimeScore = 1.0 // Neutral score if time pattern not available
		}

		// Calculate signature similarity
		if currentSignature != nil && pattern.ObstructionData != nil {
			candidate.SignatureScore = pm.calculateSignatureSimilarity(pattern.ObstructionData, currentSignature)
		} else {
			candidate.SignatureScore = 0.5 // Low score if signature comparison not possible
		}

		// Calculate overall similarity
		candidate.Similarity = pm.calculateOverallSimilarity(candidate)
		candidate.OverallScore = candidate.Similarity * pattern.Confidence

		// Generate reasoning
		candidate.Reason = pm.generateMatchReason(candidate)

		// Only include candidates above threshold
		if candidate.OverallScore >= pm.config.MatchThreshold {
			candidates = append(candidates, candidate)
		}
	}

	// Sort by overall score (highest first)
	sort.Slice(candidates, func(i, j int) bool {
		return candidates[i].OverallScore > candidates[j].OverallScore
	})

	pm.logger.Debug("Found pattern match candidates",
		"total_patterns", len(patterns),
		"candidates", len(candidates),
		"threshold", pm.config.MatchThreshold)

	return candidates, nil
}

// calculateLocationSimilarity calculates similarity between two locations
func (pm *PatternMatcher) calculateLocationSimilarity(loc1, loc2 *LocationInfo) float64 {
	// Calculate distance using Haversine formula
	distance := pm.haversineDistance(loc1.Latitude, loc1.Longitude, loc2.Latitude, loc2.Longitude)

	// Convert distance to similarity score (closer = more similar)
	// Use exponential decay for similarity
	maxDistance := 1000.0 // 1km maximum useful distance
	similarity := math.Exp(-distance / maxDistance)

	return similarity
}

// calculateTimeSimilarity calculates similarity between a time pattern and current time
func (pm *PatternMatcher) calculateTimeSimilarity(pattern *TimePattern, currentTime time.Time) float64 {
	switch pattern.Type {
	case "daily":
		return pm.calculateDailyTimeSimilarity(pattern, currentTime)
	case "weekly":
		return pm.calculateWeeklyTimeSimilarity(pattern, currentTime)
	case "seasonal":
		return pm.calculateSeasonalTimeSimilarity(pattern, currentTime)
	default:
		return 0.5 // Neutral score for unknown pattern types
	}
}

// calculateDailyTimeSimilarity calculates similarity for daily time patterns
func (pm *PatternMatcher) calculateDailyTimeSimilarity(pattern *TimePattern, currentTime time.Time) float64 {
	patternMinutes := pattern.StartTime.Hour()*60 + pattern.StartTime.Minute()
	currentMinutes := currentTime.Hour()*60 + currentTime.Minute()

	timeDiff := math.Abs(float64(patternMinutes - currentMinutes))

	// Handle wrap-around (e.g., 23:30 vs 00:30)
	if timeDiff > 720 { // 12 hours
		timeDiff = 1440 - timeDiff // 24 hours - diff
	}

	// Convert to similarity (closer times = higher similarity)
	maxDiff := 360.0 // 6 hours maximum useful difference
	similarity := math.Max(0, 1.0-timeDiff/maxDiff)

	return similarity
}

// calculateWeeklyTimeSimilarity calculates similarity for weekly time patterns
func (pm *PatternMatcher) calculateWeeklyTimeSimilarity(pattern *TimePattern, currentTime time.Time) float64 {
	currentWeekday := int(currentTime.Weekday())

	// Check if current day is in the pattern
	if pattern.DayOfWeek != nil {
		for _, day := range pattern.DayOfWeek {
			if day == currentWeekday {
				// Also check time of day if available
				if !pattern.StartTime.IsZero() {
					return pm.calculateDailyTimeSimilarity(pattern, currentTime)
				}
				return 1.0
			}
		}
	}

	return 0.0 // No match for day of week
}

// calculateSeasonalTimeSimilarity calculates similarity for seasonal time patterns
func (pm *PatternMatcher) calculateSeasonalTimeSimilarity(pattern *TimePattern, currentTime time.Time) float64 {
	currentMonth := int(currentTime.Month())

	// Check if current month is in the pattern
	if pattern.MonthOfYear != nil {
		for _, month := range pattern.MonthOfYear {
			if month == currentMonth {
				return 1.0
			}
		}
	}

	return 0.0 // No match for month
}

// calculateSignatureSimilarity calculates similarity between obstruction signatures
func (pm *PatternMatcher) calculateSignatureSimilarity(sig1, sig2 *ObstructionSignature) float64 {
	if sig1 == nil || sig2 == nil {
		return 0
	}

	var totalScore float64
	var components int

	// Compare typical obstruction (40% weight)
	obstructionDiff := math.Abs(sig1.TypicalObstruction - sig2.TypicalObstruction)
	obstructionScore := math.Max(0, 1.0-obstructionDiff*2) // Scale factor
	totalScore += obstructionScore * 0.4
	components++

	// Compare typical SNR (30% weight)
	snrDiff := math.Abs(sig1.TypicalSNR - sig2.TypicalSNR)
	snrScore := math.Max(0, 1.0-snrDiff/10.0) // Scale factor (10 dB range)
	totalScore += snrScore * 0.3
	components++

	// Compare severity (20% weight)
	severityScore := 0.0
	if sig1.Severity == sig2.Severity {
		severityScore = 1.0
	} else {
		// Partial score for similar severities
		severityMap := map[string]int{"minor": 1, "moderate": 2, "severe": 3}
		diff := math.Abs(float64(severityMap[sig1.Severity] - severityMap[sig2.Severity]))
		severityScore = math.Max(0, 1.0-diff/2.0)
	}
	totalScore += severityScore * 0.2

	// Compare predictability (10% weight)
	if sig1.Predictability > 0 && sig2.Predictability > 0 {
		predictabilityDiff := math.Abs(sig1.Predictability - sig2.Predictability)
		predictabilityScore := math.Max(0, 1.0-predictabilityDiff)
		totalScore += predictabilityScore * 0.1
	}

	return totalScore
}

// calculateOverallSimilarity calculates the overall similarity score
func (pm *PatternMatcher) calculateOverallSimilarity(candidate *MatchCandidate) float64 {
	// Weighted combination of different similarity scores
	locationWeight := 0.3
	timeWeight := 0.3
	signatureWeight := 0.4

	// Adjust weights based on available data
	if candidate.LocationScore == 1.0 && candidate.Pattern.Location == nil {
		// No location data, redistribute weight
		timeWeight += locationWeight / 2
		signatureWeight += locationWeight / 2
		locationWeight = 0
	}

	if candidate.TimeScore == 1.0 && candidate.Pattern.TimePattern == nil {
		// No time pattern, redistribute weight
		locationWeight += timeWeight / 2
		signatureWeight += timeWeight / 2
		timeWeight = 0
	}

	similarity := candidate.LocationScore*locationWeight +
		candidate.TimeScore*timeWeight +
		candidate.SignatureScore*signatureWeight

	return similarity
}

// generateMatchReason generates a human-readable reason for the match
func (pm *PatternMatcher) generateMatchReason(candidate *MatchCandidate) string {
	reasons := []string{}

	if candidate.LocationScore > 0.8 {
		reasons = append(reasons, "strong location match")
	} else if candidate.LocationScore > 0.5 {
		reasons = append(reasons, "moderate location match")
	}

	if candidate.TimeScore > 0.8 {
		reasons = append(reasons, "strong time pattern match")
	} else if candidate.TimeScore > 0.5 {
		reasons = append(reasons, "moderate time pattern match")
	}

	if candidate.SignatureScore > 0.8 {
		reasons = append(reasons, "strong signature match")
	} else if candidate.SignatureScore > 0.5 {
		reasons = append(reasons, "moderate signature match")
	}

	if len(reasons) == 0 {
		return "weak overall match"
	}

	if len(reasons) == 1 {
		return reasons[0]
	}

	// Join multiple reasons
	result := ""
	for i, reason := range reasons {
		if i == 0 {
			result = reason
		} else if i == len(reasons)-1 {
			result += " and " + reason
		} else {
			result += ", " + reason
		}
	}

	return result
}

// StartActiveMatch starts tracking an active pattern match
func (pm *PatternMatcher) StartActiveMatch(ctx context.Context, candidate *MatchCandidate) (*ActiveMatch, error) {
	pm.mu.Lock()
	defer pm.mu.Unlock()

	// Check if we already have an active match for this pattern
	if existing, exists := pm.activeMatches[candidate.Pattern.ID]; exists {
		return existing, nil
	}

	// Check active match limit
	if len(pm.activeMatches) >= pm.config.MaxActiveMatches {
		// Remove oldest active match
		pm.removeOldestActiveMatch()
	}

	match := &ActiveMatch{
		PatternID:   candidate.Pattern.ID,
		PatternName: candidate.Pattern.Name,
		StartTime:   time.Now(),
		LastUpdate:  time.Now(),
		Similarity:  candidate.Similarity,
		Confidence:  candidate.Pattern.Confidence,
		Status:      "matching",
		SampleCount: 0,
		Predictions: make([]*ObstructionPrediction, 0),
		Metadata:    make(map[string]interface{}),
	}

	// Store match context
	match.Metadata["initial_similarity"] = candidate.Similarity
	match.Metadata["location_score"] = candidate.LocationScore
	match.Metadata["time_score"] = candidate.TimeScore
	match.Metadata["signature_score"] = candidate.SignatureScore
	match.Metadata["reason"] = candidate.Reason

	pm.activeMatches[candidate.Pattern.ID] = match

	pm.logger.Info("Started active pattern match",
		"pattern_id", candidate.Pattern.ID,
		"pattern_name", candidate.Pattern.Name,
		"similarity", candidate.Similarity,
		"reason", candidate.Reason)

	return match, nil
}

// UpdateActiveMatch updates an active pattern match with new data
func (pm *PatternMatcher) UpdateActiveMatch(ctx context.Context, patternID string,
	currentSignature *ObstructionSignature, similarity float64,
) error {
	pm.mu.Lock()
	defer pm.mu.Unlock()

	match, exists := pm.activeMatches[patternID]
	if !exists {
		return fmt.Errorf("no active match found for pattern %s", patternID)
	}

	match.LastUpdate = time.Now()
	match.SampleCount++

	// Update similarity with exponential moving average
	alpha := 0.3 // Smoothing factor
	match.Similarity = match.Similarity*(1-alpha) + similarity*alpha

	// Check for match confirmation or failure
	if match.SampleCount >= 5 { // Need at least 5 samples to confirm
		if match.Similarity >= pm.config.MatchThreshold {
			match.Status = "confirmed"
		} else if match.Similarity < pm.config.MatchThreshold*0.7 {
			match.Status = "failed"
		}
	}

	// Check for timeout
	if time.Since(match.StartTime) > time.Duration(pm.config.MatchTimeoutMinutes)*time.Minute {
		if match.Status == "matching" {
			match.Status = "failed"
			match.Metadata["failure_reason"] = "timeout"
		}
	}

	pm.logger.Debug("Updated active pattern match",
		"pattern_id", patternID,
		"similarity", match.Similarity,
		"status", match.Status,
		"samples", match.SampleCount)

	return nil
}

// EndActiveMatch ends an active pattern match and records the result
func (pm *PatternMatcher) EndActiveMatch(ctx context.Context, patternID string, success bool, reason string) error {
	pm.mu.Lock()
	defer pm.mu.Unlock()

	match, exists := pm.activeMatches[patternID]
	if !exists {
		return fmt.Errorf("no active match found for pattern %s", patternID)
	}

	// Create match result
	result := &MatchResult{
		Timestamp:   time.Now(),
		PatternID:   match.PatternID,
		PatternName: match.PatternName,
		Similarity:  match.Similarity,
		Confidence:  match.Confidence,
		Success:     success,
		Reason:      reason,
		Duration:    time.Since(match.StartTime),
		SampleCount: match.SampleCount,
		Metadata:    match.Metadata,
	}

	// Add to history
	pm.matchHistory = append(pm.matchHistory, result)
	if len(pm.matchHistory) > pm.config.HistorySize {
		pm.matchHistory = pm.matchHistory[1:]
	}

	// Remove from active matches
	delete(pm.activeMatches, patternID)

	pm.logger.Info("Ended active pattern match",
		"pattern_id", patternID,
		"success", success,
		"reason", reason,
		"duration", result.Duration,
		"samples", result.SampleCount)

	return nil
}

// removeOldestActiveMatch removes the oldest active match
func (pm *PatternMatcher) removeOldestActiveMatch() {
	var oldestID string
	var oldestTime time.Time

	for id, match := range pm.activeMatches {
		if oldestID == "" || match.StartTime.Before(oldestTime) {
			oldestID = id
			oldestTime = match.StartTime
		}
	}

	if oldestID != "" {
		if err := pm.EndActiveMatch(context.Background(), oldestID, false, "removed due to limit"); err != nil {
			pm.logger.Warn("Failed to end active match", "match_id", oldestID, "error", err)
		}
	}
}

// GetActiveMatches returns all current active matches
func (pm *PatternMatcher) GetActiveMatches() map[string]*ActiveMatch {
	pm.mu.RLock()
	defer pm.mu.RUnlock()

	// Return a copy to prevent external modification
	matches := make(map[string]*ActiveMatch)
	for id, match := range pm.activeMatches {
		matches[id] = match
	}

	return matches
}

// GetMatchHistory returns recent match history
func (pm *PatternMatcher) GetMatchHistory(maxResults int) []*MatchResult {
	pm.mu.RLock()
	defer pm.mu.RUnlock()

	if maxResults <= 0 || maxResults >= len(pm.matchHistory) {
		// Return copy of all history
		history := make([]*MatchResult, len(pm.matchHistory))
		copy(history, pm.matchHistory)
		return history
	}

	// Return copy of recent history
	start := len(pm.matchHistory) - maxResults
	history := make([]*MatchResult, maxResults)
	copy(history, pm.matchHistory[start:])
	return history
}

// CleanupExpiredMatches removes expired active matches
func (pm *PatternMatcher) CleanupExpiredMatches(ctx context.Context) {
	pm.mu.Lock()
	defer pm.mu.Unlock()

	timeout := time.Duration(pm.config.MatchTimeoutMinutes) * time.Minute
	now := time.Now()

	var expiredIDs []string
	for id, match := range pm.activeMatches {
		if now.Sub(match.StartTime) > timeout {
			expiredIDs = append(expiredIDs, id)
		}
	}

	for _, id := range expiredIDs {
		match := pm.activeMatches[id]

		// Create expired match result
		result := &MatchResult{
			Timestamp:   now,
			PatternID:   match.PatternID,
			PatternName: match.PatternName,
			Similarity:  match.Similarity,
			Confidence:  match.Confidence,
			Success:     false,
			Reason:      "expired due to timeout",
			Duration:    now.Sub(match.StartTime),
			SampleCount: match.SampleCount,
			Metadata:    match.Metadata,
		}

		pm.matchHistory = append(pm.matchHistory, result)
		delete(pm.activeMatches, id)

		pm.logger.Info("Cleaned up expired pattern match",
			"pattern_id", id,
			"duration", result.Duration)
	}

	// Trim history if needed
	if len(pm.matchHistory) > pm.config.HistorySize {
		pm.matchHistory = pm.matchHistory[len(pm.matchHistory)-pm.config.HistorySize:]
	}
}

// haversineDistance calculates the distance between two points on Earth
func (pm *PatternMatcher) haversineDistance(lat1, lon1, lat2, lon2 float64) float64 {
	const R = 6371000 // Earth's radius in meters

	lat1Rad := lat1 * math.Pi / 180
	lat2Rad := lat2 * math.Pi / 180
	deltaLatRad := (lat2 - lat1) * math.Pi / 180
	deltaLonRad := (lon2 - lon1) * math.Pi / 180

	a := math.Sin(deltaLatRad/2)*math.Sin(deltaLatRad/2) +
		math.Cos(lat1Rad)*math.Cos(lat2Rad)*
			math.Sin(deltaLonRad/2)*math.Sin(deltaLonRad/2)
	c := 2 * math.Atan2(math.Sqrt(a), math.Sqrt(1-a))

	return R * c
}

// GetStatus returns current matcher status
func (pm *PatternMatcher) GetStatus() map[string]interface{} {
	pm.mu.RLock()
	defer pm.mu.RUnlock()

	status := map[string]interface{}{
		"active_matches_count": len(pm.activeMatches),
		"match_history_count":  len(pm.matchHistory),
		"last_match_time":      pm.lastMatchTime,
		"config":               pm.config,
	}

	// Active matches summary
	if len(pm.activeMatches) > 0 {
		var confirmedCount, matchingCount, failedCount int
		for _, match := range pm.activeMatches {
			switch match.Status {
			case "confirmed":
				confirmedCount++
			case "matching":
				matchingCount++
			case "failed":
				failedCount++
			}
		}

		status["active_matches_summary"] = map[string]interface{}{
			"confirmed": confirmedCount,
			"matching":  matchingCount,
			"failed":    failedCount,
		}
	}

	// Recent match success rate
	if len(pm.matchHistory) > 0 {
		recentCount := 10
		if recentCount > len(pm.matchHistory) {
			recentCount = len(pm.matchHistory)
		}

		successCount := 0
		for i := len(pm.matchHistory) - recentCount; i < len(pm.matchHistory); i++ {
			if pm.matchHistory[i].Success {
				successCount++
			}
		}

		status["recent_success_rate"] = float64(successCount) / float64(recentCount)
	}

	return status
}
