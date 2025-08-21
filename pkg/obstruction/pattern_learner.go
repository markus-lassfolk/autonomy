package obstruction

import (
	"context"
	"encoding/json"
	"fmt"
	"math"
	"sync"
	"time"

	"github.com/markus-lassfolk/autonomy/pkg/logx"
)

// EnvironmentalPattern represents a learned environmental pattern
type EnvironmentalPattern struct {
	ID              string                 `json:"id"`
	Name            string                 `json:"name"`
	Description     string                 `json:"description"`
	Location        *LocationInfo          `json:"location,omitempty"`
	TimePattern     *TimePattern           `json:"time_pattern,omitempty"`
	WeatherPattern  *WeatherPattern        `json:"weather_pattern,omitempty"`
	ObstructionData *ObstructionSignature  `json:"obstruction_data"`
	Confidence      float64                `json:"confidence"`
	SampleCount     int                    `json:"sample_count"`
	FirstSeen       time.Time              `json:"first_seen"`
	LastSeen        time.Time              `json:"last_seen"`
	Metadata        map[string]interface{} `json:"metadata,omitempty"`
}

// LocationInfo represents location-based pattern information
type LocationInfo struct {
	Latitude    float64 `json:"latitude"`
	Longitude   float64 `json:"longitude"`
	Accuracy    float64 `json:"accuracy"`
	Radius      float64 `json:"radius"`      // Pattern applies within this radius (meters)
	Elevation   float64 `json:"elevation"`   // Elevation in meters
	Environment string  `json:"environment"` // "urban", "rural", "forest", "mountain", etc.
}

// TimePattern represents time-based patterns
type TimePattern struct {
	Type        string        `json:"type"`                    // "daily", "weekly", "seasonal"
	StartTime   time.Time     `json:"start_time"`              // When pattern typically starts
	EndTime     time.Time     `json:"end_time"`                // When pattern typically ends
	Duration    time.Duration `json:"duration"`                // Typical duration
	Frequency   float64       `json:"frequency"`               // How often it occurs (0-1)
	DayOfWeek   []int         `json:"day_of_week,omitempty"`   // Days of week (0=Sunday)
	MonthOfYear []int         `json:"month_of_year,omitempty"` // Months of year (1=January)
}

// WeatherPattern represents weather-related patterns
type WeatherPattern struct {
	Conditions    []string `json:"conditions"`              // Weather conditions that trigger pattern
	Temperature   *Range   `json:"temperature,omitempty"`   // Temperature range
	Humidity      *Range   `json:"humidity,omitempty"`      // Humidity range
	WindSpeed     *Range   `json:"wind_speed,omitempty"`    // Wind speed range
	Precipitation *Range   `json:"precipitation,omitempty"` // Precipitation range
	Pressure      *Range   `json:"pressure,omitempty"`      // Atmospheric pressure range
}

// Range represents a numeric range
type Range struct {
	Min float64 `json:"min"`
	Max float64 `json:"max"`
}

// ObstructionSignature represents the obstruction characteristics of a pattern
type ObstructionSignature struct {
	TypicalObstruction float64       `json:"typical_obstruction"`     // Typical obstruction percentage
	ObstructionRange   *Range        `json:"obstruction_range"`       // Range of obstruction values
	TypicalSNR         float64       `json:"typical_snr"`             // Typical SNR value
	SNRRange           *Range        `json:"snr_range"`               // Range of SNR values
	ObstructionProfile []float64     `json:"obstruction_profile"`     // Obstruction over time profile
	RecoveryTime       time.Duration `json:"recovery_time"`           // Typical recovery time
	Severity           string        `json:"severity"`                // "minor", "moderate", "severe"
	Predictability     float64       `json:"predictability"`          // How predictable this pattern is (0-1)
	WedgePattern       []float64     `json:"wedge_pattern,omitempty"` // Typical wedge obstruction pattern
}

// PatternLearner learns and manages environmental obstruction patterns
type PatternLearner struct {
	mu       sync.RWMutex
	logger   *logx.Logger
	config   *PatternLearnerConfig
	patterns map[string]*EnvironmentalPattern

	// Learning state
	currentObservation *ObservationSession
	observations       []*ObservationSession

	// Pattern matching
	matcher *PatternMatcher
}

// PatternLearnerConfig holds configuration for pattern learning
type PatternLearnerConfig struct {
	MaxPatterns                int           `json:"max_patterns"`
	MinObservationsToLearn     int           `json:"min_observations_to_learn"`
	PatternSimilarityThreshold float64       `json:"pattern_similarity_threshold"`
	LocationRadiusMeters       float64       `json:"location_radius_meters"`
	TimeToleranceMinutes       int           `json:"time_tolerance_minutes"`
	ConfidenceThreshold        float64       `json:"confidence_threshold"`
	ObservationTimeout         time.Duration `json:"observation_timeout"`
	PatternExpiryDays          int           `json:"pattern_expiry_days"`
	EnableLocationLearning     bool          `json:"enable_location_learning"`
	EnableTimeLearning         bool          `json:"enable_time_learning"`
	EnableWeatherLearning      bool          `json:"enable_weather_learning"`
}

// DefaultPatternLearnerConfig returns default configuration
func DefaultPatternLearnerConfig() *PatternLearnerConfig {
	return &PatternLearnerConfig{
		MaxPatterns:                100,
		MinObservationsToLearn:     5,
		PatternSimilarityThreshold: 0.8,
		LocationRadiusMeters:       100, // 100 meter radius for location patterns
		TimeToleranceMinutes:       30,  // 30 minute tolerance for time patterns
		ConfidenceThreshold:        0.7,
		ObservationTimeout:         30 * time.Minute,
		PatternExpiryDays:          30, // Patterns expire after 30 days without reinforcement
		EnableLocationLearning:     true,
		EnableTimeLearning:         true,
		EnableWeatherLearning:      false, // Disabled by default (requires weather API)
	}
}

// ObservationSession represents an ongoing observation of obstruction patterns
type ObservationSession struct {
	ID           string                 `json:"id"`
	StartTime    time.Time              `json:"start_time"`
	EndTime      *time.Time             `json:"end_time,omitempty"`
	Location     *LocationInfo          `json:"location,omitempty"`
	Weather      *WeatherPattern        `json:"weather,omitempty"`
	Samples      []ObstructionSample    `json:"samples"`
	Summary      *ObstructionSignature  `json:"summary,omitempty"`
	PatternMatch *string                `json:"pattern_match,omitempty"` // ID of matched pattern
	Metadata     map[string]interface{} `json:"metadata,omitempty"`
}

// NewPatternLearner creates a new pattern learner
func NewPatternLearner(logger *logx.Logger, config *PatternLearnerConfig) *PatternLearner {
	if config == nil {
		config = DefaultPatternLearnerConfig()
	}

	pl := &PatternLearner{
		logger:       logger,
		config:       config,
		patterns:     make(map[string]*EnvironmentalPattern),
		observations: make([]*ObservationSession, 0),
		matcher:      NewPatternMatcher(logger),
	}

	return pl
}

// StartObservation begins a new observation session
func (pl *PatternLearner) StartObservation(ctx context.Context, location *LocationInfo) (*ObservationSession, error) {
	pl.mu.Lock()
	defer pl.mu.Unlock()

	// End any existing observation
	if pl.currentObservation != nil {
		if err := pl.endCurrentObservation(); err != nil {
			pl.logger.Warn("Failed to end current observation", "error", err)
		}
	}

	session := &ObservationSession{
		ID:        fmt.Sprintf("obs_%d", time.Now().Unix()),
		StartTime: time.Now(),
		Location:  location,
		Samples:   make([]ObstructionSample, 0),
		Metadata:  make(map[string]interface{}),
	}

	pl.currentObservation = session

	pl.logger.Info("Started new observation session",
		"session_id", session.ID,
		"location", location)

	return session, nil
}

// AddObservation adds an obstruction sample to the current observation
func (pl *PatternLearner) AddObservation(ctx context.Context, sample ObstructionSample) error {
	pl.mu.Lock()
	defer pl.mu.Unlock()

	if pl.currentObservation == nil {
		// Auto-start observation if none exists
		_, err := pl.StartObservation(ctx, nil)
		if err != nil {
			return fmt.Errorf("failed to auto-start observation: %w", err)
		}
	}

	pl.currentObservation.Samples = append(pl.currentObservation.Samples, sample)

	pl.logger.Debug("Added observation sample",
		"session_id", pl.currentObservation.ID,
		"sample_count", len(pl.currentObservation.Samples),
		"obstruction", sample.FractionObstructed,
		"snr", sample.SNR)

	// Check if observation should be ended (timeout or pattern completion)
	if pl.shouldEndObservation() {
		return pl.endCurrentObservation()
	}

	return nil
}

// EndObservation manually ends the current observation session
func (pl *PatternLearner) EndObservation(ctx context.Context) error {
	pl.mu.Lock()
	defer pl.mu.Unlock()

	if pl.currentObservation == nil {
		return fmt.Errorf("no active observation session")
	}

	return pl.endCurrentObservation()
}

// endCurrentObservation ends the current observation and processes it for learning
func (pl *PatternLearner) endCurrentObservation() error {
	if pl.currentObservation == nil {
		return nil
	}

	now := time.Now()
	pl.currentObservation.EndTime = &now

	// Generate summary
	summary, err := pl.generateObservationSummary(pl.currentObservation)
	if err != nil {
		pl.logger.Warn("Failed to generate observation summary", "error", err)
	} else {
		pl.currentObservation.Summary = summary
	}

	// Try to match with existing patterns
	matchedPattern := pl.findMatchingPattern(pl.currentObservation)
	if matchedPattern != nil {
		pl.currentObservation.PatternMatch = &matchedPattern.ID
		pl.reinforcePattern(matchedPattern, pl.currentObservation)
		pl.logger.Info("Observation matched existing pattern",
			"session_id", pl.currentObservation.ID,
			"pattern_id", matchedPattern.ID,
			"pattern_name", matchedPattern.Name)
	} else {
		// Try to learn new pattern
		newPattern, err := pl.learnNewPattern(pl.currentObservation)
		if err != nil {
			pl.logger.Debug("Could not learn new pattern from observation", "error", err)
		} else if newPattern != nil {
			pl.patterns[newPattern.ID] = newPattern
			pl.currentObservation.PatternMatch = &newPattern.ID
			pl.logger.Info("Learned new pattern from observation",
				"session_id", pl.currentObservation.ID,
				"pattern_id", newPattern.ID,
				"pattern_name", newPattern.Name)
		}
	}

	// Store observation
	pl.observations = append(pl.observations, pl.currentObservation)

	// Limit observation history
	if len(pl.observations) > pl.config.MaxPatterns*2 {
		pl.observations = pl.observations[len(pl.observations)-pl.config.MaxPatterns:]
	}

	pl.logger.Info("Ended observation session",
		"session_id", pl.currentObservation.ID,
		"duration", now.Sub(pl.currentObservation.StartTime),
		"samples", len(pl.currentObservation.Samples),
		"pattern_match", pl.currentObservation.PatternMatch)

	pl.currentObservation = nil
	return nil
}

// shouldEndObservation determines if the current observation should be ended
func (pl *PatternLearner) shouldEndObservation() bool {
	if pl.currentObservation == nil {
		return false
	}

	// End on timeout
	if time.Since(pl.currentObservation.StartTime) > pl.config.ObservationTimeout {
		return true
	}

	// End if obstruction has returned to normal for a while
	if len(pl.currentObservation.Samples) > 10 {
		recentSamples := pl.currentObservation.Samples[len(pl.currentObservation.Samples)-10:]
		allNormal := true
		for _, sample := range recentSamples {
			if sample.FractionObstructed > 0.05 || sample.CurrentlyObstructed {
				allNormal = false
				break
			}
		}
		if allNormal {
			return true
		}
	}

	return false
}

// generateObservationSummary creates a summary of an observation session
func (pl *PatternLearner) generateObservationSummary(session *ObservationSession) (*ObstructionSignature, error) {
	if len(session.Samples) == 0 {
		return nil, fmt.Errorf("no samples in observation")
	}

	signature := &ObstructionSignature{
		ObstructionProfile: make([]float64, 0),
		WedgePattern:       make([]float64, 0),
	}

	// Calculate statistics
	var obstructionSum, snrSum float64
	var obstructionMin, obstructionMax, snrMin, snrMax float64
	var obstructedCount int

	obstructionMin = session.Samples[0].FractionObstructed
	obstructionMax = session.Samples[0].FractionObstructed
	snrMin = session.Samples[0].SNR
	snrMax = session.Samples[0].SNR

	for _, sample := range session.Samples {
		obstructionSum += sample.FractionObstructed
		snrSum += sample.SNR

		if sample.FractionObstructed < obstructionMin {
			obstructionMin = sample.FractionObstructed
		}
		if sample.FractionObstructed > obstructionMax {
			obstructionMax = sample.FractionObstructed
		}
		if sample.SNR < snrMin {
			snrMin = sample.SNR
		}
		if sample.SNR > snrMax {
			snrMax = sample.SNR
		}

		if sample.CurrentlyObstructed {
			obstructedCount++
		}

		signature.ObstructionProfile = append(signature.ObstructionProfile, sample.FractionObstructed)
	}

	sampleCount := len(session.Samples)
	signature.TypicalObstruction = obstructionSum / float64(sampleCount)
	signature.TypicalSNR = snrSum / float64(sampleCount)
	signature.ObstructionRange = &Range{Min: obstructionMin, Max: obstructionMax}
	signature.SNRRange = &Range{Min: snrMin, Max: snrMax}

	// Calculate recovery time
	if session.EndTime != nil {
		signature.RecoveryTime = session.EndTime.Sub(session.StartTime)
	}

	// Determine severity
	if signature.TypicalObstruction < 0.1 {
		signature.Severity = "minor"
	} else if signature.TypicalObstruction < 0.3 {
		signature.Severity = "moderate"
	} else {
		signature.Severity = "severe"
	}

	// Calculate predictability based on pattern consistency
	signature.Predictability = pl.calculatePredictability(signature.ObstructionProfile)

	return signature, nil
}

// calculatePredictability measures how predictable/consistent a pattern is
func (pl *PatternLearner) calculatePredictability(profile []float64) float64 {
	if len(profile) < 3 {
		return 0
	}

	// Calculate variance - lower variance means higher predictability
	var sum, sumSquares float64
	for _, value := range profile {
		sum += value
		sumSquares += value * value
	}

	mean := sum / float64(len(profile))
	variance := (sumSquares - sum*mean) / float64(len(profile))

	// Convert variance to predictability score (0-1)
	// Lower variance = higher predictability
	predictability := math.Exp(-variance * 10) // Scale factor for sensitivity

	return math.Min(predictability, 1.0)
}

// findMatchingPattern finds an existing pattern that matches the observation
func (pl *PatternLearner) findMatchingPattern(session *ObservationSession) *EnvironmentalPattern {
	if session.Summary == nil {
		return nil
	}

	bestMatch := (*EnvironmentalPattern)(nil)
	bestScore := 0.0

	for _, pattern := range pl.patterns {
		score := pl.calculatePatternSimilarity(pattern, session)
		if score > bestScore && score >= pl.config.PatternSimilarityThreshold {
			bestScore = score
			bestMatch = pattern
		}
	}

	return bestMatch
}

// calculatePatternSimilarity calculates similarity between a pattern and observation
func (pl *PatternLearner) calculatePatternSimilarity(pattern *EnvironmentalPattern, session *ObservationSession) float64 {
	if session.Summary == nil {
		return 0
	}

	var totalScore, weightSum float64

	// Location similarity (if both have location data)
	if pl.config.EnableLocationLearning && pattern.Location != nil && session.Location != nil {
		locationScore := pl.calculateLocationSimilarity(pattern.Location, session.Location)
		totalScore += locationScore * 0.4 // 40% weight for location
		weightSum += 0.4
	}

	// Time similarity (if pattern has time data)
	if pl.config.EnableTimeLearning && pattern.TimePattern != nil {
		timeScore := pl.calculateTimeSimilarity(pattern.TimePattern, session.StartTime)
		totalScore += timeScore * 0.3 // 30% weight for time
		weightSum += 0.3
	}

	// Obstruction signature similarity
	signatureScore := pl.calculateSignatureSimilarity(pattern.ObstructionData, session.Summary)
	totalScore += signatureScore * 0.3 // 30% weight for signature
	weightSum += 0.3

	if weightSum == 0 {
		return 0
	}

	return totalScore / weightSum
}

// calculateLocationSimilarity calculates similarity between two locations
func (pl *PatternLearner) calculateLocationSimilarity(loc1, loc2 *LocationInfo) float64 {
	// Calculate distance using Haversine formula
	distance := pl.haversineDistance(loc1.Latitude, loc1.Longitude, loc2.Latitude, loc2.Longitude)

	// Convert distance to similarity score
	if distance <= pl.config.LocationRadiusMeters {
		return 1.0 - (distance / pl.config.LocationRadiusMeters)
	}

	return 0
}

// haversineDistance calculates the distance between two points on Earth
func (pl *PatternLearner) haversineDistance(lat1, lon1, lat2, lon2 float64) float64 {
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

// calculateTimeSimilarity calculates similarity between a time pattern and a timestamp
func (pl *PatternLearner) calculateTimeSimilarity(pattern *TimePattern, timestamp time.Time) float64 {
	// This is a simplified implementation
	// Production code might consider more sophisticated time pattern matching

	switch pattern.Type {
	case "daily":
		// Compare time of day
		patternTime := pattern.StartTime.Hour()*60 + pattern.StartTime.Minute()
		observationTime := timestamp.Hour()*60 + timestamp.Minute()
		timeDiff := math.Abs(float64(patternTime - observationTime))

		// Handle wrap-around (e.g., 23:30 vs 00:30)
		if timeDiff > 720 { // 12 hours
			timeDiff = 1440 - timeDiff // 24 hours - diff
		}

		tolerance := float64(pl.config.TimeToleranceMinutes)
		if timeDiff <= tolerance {
			return 1.0 - (timeDiff / tolerance)
		}

	case "weekly":
		// Compare day of week
		if pattern.DayOfWeek != nil {
			weekday := int(timestamp.Weekday())
			for _, day := range pattern.DayOfWeek {
				if day == weekday {
					return 1.0
				}
			}
		}

	case "seasonal":
		// Compare month
		if pattern.MonthOfYear != nil {
			month := int(timestamp.Month())
			for _, m := range pattern.MonthOfYear {
				if m == month {
					return 1.0
				}
			}
		}
	}

	return 0
}

// calculateSignatureSimilarity calculates similarity between obstruction signatures
func (pl *PatternLearner) calculateSignatureSimilarity(sig1, sig2 *ObstructionSignature) float64 {
	if sig1 == nil || sig2 == nil {
		return 0
	}

	var totalScore float64
	var components int

	// Compare typical obstruction
	obstructionDiff := math.Abs(sig1.TypicalObstruction - sig2.TypicalObstruction)
	obstructionScore := math.Max(0, 1.0-obstructionDiff*2) // Scale factor
	totalScore += obstructionScore
	components++

	// Compare typical SNR
	snrDiff := math.Abs(sig1.TypicalSNR - sig2.TypicalSNR)
	snrScore := math.Max(0, 1.0-snrDiff/10.0) // Scale factor (10 dB range)
	totalScore += snrScore
	components++

	// Compare severity
	if sig1.Severity == sig2.Severity {
		totalScore += 1.0
	}
	components++

	if components == 0 {
		return 0
	}

	return totalScore / float64(components)
}

// reinforcePattern updates an existing pattern with new observation data
func (pl *PatternLearner) reinforcePattern(pattern *EnvironmentalPattern, session *ObservationSession) {
	pattern.SampleCount++
	pattern.LastSeen = time.Now()

	// Update confidence (more samples = higher confidence, up to a limit)
	pattern.Confidence = math.Min(0.95, 0.5+float64(pattern.SampleCount)*0.05)

	// Update obstruction data (weighted average with existing data)
	if session.Summary != nil && pattern.ObstructionData != nil {
		weight := 1.0 / float64(pattern.SampleCount) // Weight of new observation

		// Update typical values
		pattern.ObstructionData.TypicalObstruction = pattern.ObstructionData.TypicalObstruction*(1-weight) +
			session.Summary.TypicalObstruction*weight

		pattern.ObstructionData.TypicalSNR = pattern.ObstructionData.TypicalSNR*(1-weight) +
			session.Summary.TypicalSNR*weight

		// Update ranges
		if pattern.ObstructionData.ObstructionRange != nil && session.Summary.ObstructionRange != nil {
			pattern.ObstructionData.ObstructionRange.Min = math.Min(
				pattern.ObstructionData.ObstructionRange.Min,
				session.Summary.ObstructionRange.Min)
			pattern.ObstructionData.ObstructionRange.Max = math.Max(
				pattern.ObstructionData.ObstructionRange.Max,
				session.Summary.ObstructionRange.Max)
		}
	}
}

// learnNewPattern attempts to create a new pattern from an observation
func (pl *PatternLearner) learnNewPattern(session *ObservationSession) (*EnvironmentalPattern, error) {
	if session.Summary == nil {
		return nil, fmt.Errorf("no summary available for pattern learning")
	}

	// Check if we have enough similar observations to learn a pattern
	similarObservations := pl.findSimilarObservations(session)
	if len(similarObservations) < pl.config.MinObservationsToLearn {
		return nil, fmt.Errorf("insufficient similar observations: have %d, need %d",
			len(similarObservations), pl.config.MinObservationsToLearn)
	}

	// Check pattern limit
	if len(pl.patterns) >= pl.config.MaxPatterns {
		// Remove oldest or least confident pattern
		pl.removeWeakestPattern()
	}

	pattern := &EnvironmentalPattern{
		ID:              fmt.Sprintf("pattern_%d", time.Now().Unix()),
		Name:            pl.generatePatternName(session),
		Description:     pl.generatePatternDescription(session),
		Location:        session.Location,
		ObstructionData: session.Summary,
		Confidence:      0.6, // Initial confidence for new patterns
		SampleCount:     1,
		FirstSeen:       session.StartTime,
		LastSeen:        time.Now(),
		Metadata:        make(map[string]interface{}),
	}

	// Generate time pattern if enabled
	if pl.config.EnableTimeLearning {
		pattern.TimePattern = pl.generateTimePattern(similarObservations)
	}

	return pattern, nil
}

// findSimilarObservations finds observations similar to the given session
func (pl *PatternLearner) findSimilarObservations(session *ObservationSession) []*ObservationSession {
	var similar []*ObservationSession

	for _, obs := range pl.observations {
		if obs.Summary != nil {
			similarity := pl.calculateSignatureSimilarity(session.Summary, obs.Summary)
			if similarity >= pl.config.PatternSimilarityThreshold {
				similar = append(similar, obs)
			}
		}
	}

	return similar
}

// removeWeakestPattern removes the pattern with lowest confidence or oldest
func (pl *PatternLearner) removeWeakestPattern() {
	if len(pl.patterns) == 0 {
		return
	}

	var weakestID string
	var lowestScore float64 = 2.0 // Higher than any possible confidence

	for id, pattern := range pl.patterns {
		// Score based on confidence and recency
		daysSinceLastSeen := time.Since(pattern.LastSeen).Hours() / 24
		ageScore := math.Max(0, 1.0-daysSinceLastSeen/float64(pl.config.PatternExpiryDays))
		score := pattern.Confidence * ageScore

		if score < lowestScore {
			lowestScore = score
			weakestID = id
		}
	}

	if weakestID != "" {
		delete(pl.patterns, weakestID)
		pl.logger.Info("Removed weakest pattern", "pattern_id", weakestID, "score", lowestScore)
	}
}

// generatePatternName generates a descriptive name for a pattern
func (pl *PatternLearner) generatePatternName(session *ObservationSession) string {
	if session.Summary == nil {
		return "Unknown Pattern"
	}

	severity := session.Summary.Severity
	timeStr := session.StartTime.Format("15:04")

	if session.Location != nil {
		return fmt.Sprintf("%s obstruction at %.4f,%.4f around %s",
			severity, session.Location.Latitude, session.Location.Longitude, timeStr)
	}

	return fmt.Sprintf("%s obstruction around %s", severity, timeStr)
}

// generatePatternDescription generates a detailed description for a pattern
func (pl *PatternLearner) generatePatternDescription(session *ObservationSession) string {
	if session.Summary == nil {
		return "Obstruction pattern with unknown characteristics"
	}

	return fmt.Sprintf("Obstruction pattern with %.1f%% typical obstruction, %.1f dB typical SNR, %s severity, recovery time %v",
		session.Summary.TypicalObstruction*100,
		session.Summary.TypicalSNR,
		session.Summary.Severity,
		session.Summary.RecoveryTime)
}

// generateTimePattern generates a time pattern from similar observations
func (pl *PatternLearner) generateTimePattern(observations []*ObservationSession) *TimePattern {
	if len(observations) < 2 {
		return nil
	}

	// Analyze time patterns in the observations
	hourCounts := make(map[int]int)
	dayOfWeekCounts := make(map[int]int)

	for _, obs := range observations {
		hour := obs.StartTime.Hour()
		dayOfWeek := int(obs.StartTime.Weekday())

		hourCounts[hour]++
		dayOfWeekCounts[dayOfWeek]++
	}

	// Find most common hour
	var mostCommonHour int
	var maxHourCount int
	for hour, count := range hourCounts {
		if count > maxHourCount {
			maxHourCount = count
			mostCommonHour = hour
		}
	}

	// Find common days of week
	var commonDays []int
	threshold := len(observations) / 3 // Must occur in at least 1/3 of observations
	for day, count := range dayOfWeekCounts {
		if count >= threshold {
			commonDays = append(commonDays, day)
		}
	}

	pattern := &TimePattern{
		Type:      "daily",
		StartTime: time.Date(0, 1, 1, mostCommonHour, 0, 0, 0, time.UTC),
		Frequency: float64(maxHourCount) / float64(len(observations)),
	}

	if len(commonDays) > 0 && len(commonDays) < 7 {
		pattern.Type = "weekly"
		pattern.DayOfWeek = commonDays
	}

	return pattern
}

// GetPatterns returns all learned patterns
func (pl *PatternLearner) GetPatterns() map[string]*EnvironmentalPattern {
	pl.mu.RLock()
	defer pl.mu.RUnlock()

	// Return a copy to prevent external modification
	patterns := make(map[string]*EnvironmentalPattern)
	for id, pattern := range pl.patterns {
		patterns[id] = pattern
	}

	return patterns
}

// GetPattern returns a specific pattern by ID
func (pl *PatternLearner) GetPattern(id string) (*EnvironmentalPattern, bool) {
	pl.mu.RLock()
	defer pl.mu.RUnlock()

	pattern, exists := pl.patterns[id]
	return pattern, exists
}

// PredictObstruction predicts obstruction based on learned patterns
func (pl *PatternLearner) PredictObstruction(ctx context.Context, location *LocationInfo, targetTime time.Time) (*ObstructionPrediction, error) {
	pl.mu.RLock()
	defer pl.mu.RUnlock()

	var applicablePatterns []*EnvironmentalPattern

	// Find patterns that might apply to the given location and time
	for _, pattern := range pl.patterns {
		if pl.isPatternApplicable(pattern, location, targetTime) {
			applicablePatterns = append(applicablePatterns, pattern)
		}
	}

	if len(applicablePatterns) == 0 {
		return nil, fmt.Errorf("no applicable patterns found for prediction")
	}

	// Generate prediction based on applicable patterns
	return pl.generatePrediction(applicablePatterns, location, targetTime)
}

// ObstructionPrediction represents a prediction of future obstruction
type ObstructionPrediction struct {
	TargetTime           time.Time `json:"target_time"`
	PredictedObstruction float64   `json:"predicted_obstruction"`
	PredictedSNR         float64   `json:"predicted_snr"`
	Confidence           float64   `json:"confidence"`
	ApplicablePatterns   []string  `json:"applicable_patterns"`
	Reasoning            string    `json:"reasoning"`
}

// isPatternApplicable checks if a pattern applies to the given context
func (pl *PatternLearner) isPatternApplicable(pattern *EnvironmentalPattern, location *LocationInfo, targetTime time.Time) bool {
	// Check location if both pattern and query have location data
	if pattern.Location != nil && location != nil {
		similarity := pl.calculateLocationSimilarity(pattern.Location, location)
		if similarity < 0.5 { // Require at least 50% location similarity
			return false
		}
	}

	// Check time pattern if it exists
	if pattern.TimePattern != nil {
		similarity := pl.calculateTimeSimilarity(pattern.TimePattern, targetTime)
		if similarity < 0.3 { // Require at least 30% time similarity
			return false
		}
	}

	// Check pattern confidence and recency
	if pattern.Confidence < pl.config.ConfidenceThreshold {
		return false
	}

	daysSinceLastSeen := time.Since(pattern.LastSeen).Hours() / 24
	return daysSinceLastSeen <= float64(pl.config.PatternExpiryDays)
}

// generatePrediction generates a prediction from applicable patterns
func (pl *PatternLearner) generatePrediction(patterns []*EnvironmentalPattern, location *LocationInfo, targetTime time.Time) (*ObstructionPrediction, error) {
	if len(patterns) == 0 {
		return nil, fmt.Errorf("no patterns provided for prediction")
	}

	var weightedObstruction, weightedSNR, totalWeight float64
	var patternIDs []string

	for _, pattern := range patterns {
		if pattern.ObstructionData == nil {
			continue
		}

		// Calculate weight based on confidence and applicability
		weight := pattern.Confidence

		// Adjust weight based on location similarity if available
		if pattern.Location != nil && location != nil {
			locationSim := pl.calculateLocationSimilarity(pattern.Location, location)
			weight *= locationSim
		}

		// Adjust weight based on time similarity if available
		if pattern.TimePattern != nil {
			timeSim := pl.calculateTimeSimilarity(pattern.TimePattern, targetTime)
			weight *= timeSim
		}

		weightedObstruction += pattern.ObstructionData.TypicalObstruction * weight
		weightedSNR += pattern.ObstructionData.TypicalSNR * weight
		totalWeight += weight
		patternIDs = append(patternIDs, pattern.ID)
	}

	if totalWeight == 0 {
		return nil, fmt.Errorf("no valid patterns for prediction")
	}

	prediction := &ObstructionPrediction{
		TargetTime:           targetTime,
		PredictedObstruction: weightedObstruction / totalWeight,
		PredictedSNR:         weightedSNR / totalWeight,
		Confidence:           math.Min(totalWeight/float64(len(patterns)), 1.0),
		ApplicablePatterns:   patternIDs,
		Reasoning:            fmt.Sprintf("Prediction based on %d applicable patterns", len(patterns)),
	}

	return prediction, nil
}

// GetStatus returns current learner status
func (pl *PatternLearner) GetStatus() map[string]interface{} {
	pl.mu.RLock()
	defer pl.mu.RUnlock()

	status := map[string]interface{}{
		"patterns_count":     len(pl.patterns),
		"observations_count": len(pl.observations),
		"max_patterns":       pl.config.MaxPatterns,
		"config":             pl.config,
	}

	if pl.currentObservation != nil {
		status["current_observation"] = map[string]interface{}{
			"id":         pl.currentObservation.ID,
			"start_time": pl.currentObservation.StartTime,
			"samples":    len(pl.currentObservation.Samples),
			"duration":   time.Since(pl.currentObservation.StartTime).String(),
		}
	}

	// Pattern statistics
	if len(pl.patterns) > 0 {
		var totalConfidence float64
		var totalSamples int
		for _, pattern := range pl.patterns {
			totalConfidence += pattern.Confidence
			totalSamples += pattern.SampleCount
		}
		status["average_confidence"] = totalConfidence / float64(len(pl.patterns))
		status["total_samples"] = totalSamples
	}

	return status
}

// SavePatterns saves patterns to JSON (for persistence)
func (pl *PatternLearner) SavePatterns() ([]byte, error) {
	pl.mu.RLock()
	defer pl.mu.RUnlock()

	return json.Marshal(pl.patterns)
}

// LoadPatterns loads patterns from JSON (for persistence)
func (pl *PatternLearner) LoadPatterns(data []byte) error {
	pl.mu.Lock()
	defer pl.mu.Unlock()

	var patterns map[string]*EnvironmentalPattern
	if err := json.Unmarshal(data, &patterns); err != nil {
		return fmt.Errorf("failed to unmarshal patterns: %w", err)
	}

	pl.patterns = patterns
	pl.logger.Info("Loaded patterns from data", "count", len(patterns))

	return nil
}
