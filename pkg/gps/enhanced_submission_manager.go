package gps

import (
	"crypto/rand"
	"crypto/sha256"
	"encoding/binary"
	"fmt"
	"math"
	"sync"
	"time"

	"github.com/markus-lassfolk/autonomy/pkg/logx"
)

// EnhancedSubmissionManager implements PM feedback for robust submissions
type EnhancedSubmissionManager struct {
	logger      *logx.Logger
	config      *EnhancedSubmissionConfig
	rateLimiter *EnhancedRateLimiter

	// Deduplication (PM #5)
	submissionFingerprints map[string]time.Time // fingerprint -> timestamp

	// Burst smoothing (PM #7)
	offlineQueue  []CellObservation
	burstSmoother *BurstSmoother

	// Stationary caps (PM #6)
	stationarySubmissions map[string]time.Time // cellKey -> last submission

	// Clock sanity (PM #8)
	clockSkewDetector *ClockSkewDetector

	// Metrics (PM #10)
	stats *SubmissionStats

	mu sync.RWMutex
}

// EnhancedSubmissionConfig defines enhanced submission behavior
type EnhancedSubmissionConfig struct {
	// Existing config
	MinGPSAccuracy       float64       `json:"min_gps_accuracy"`
	MovementThreshold    float64       `json:"movement_threshold"`
	RSRPChangeThreshold  float64       `json:"rsrp_change_threshold"`
	ContributionInterval time.Duration `json:"contribution_interval"`

	// Deduplication (PM #5)
	DedupeGridSizeM   float64 `json:"dedupe_grid_size_m"`   // Quantize location to ~50-100m
	DedupeTimeWindowH int     `json:"dedupe_time_window_h"` // Hour bucket for deduplication

	// Stationary caps (PM #6)
	StationaryMaxInterval time.Duration `json:"stationary_max_interval"` // Max 1 per cell per 2-4h when stationary
	StationaryGlobalCap   int           `json:"stationary_global_cap"`   // Global cap per hour when stationary

	// Burst smoothing (PM #7)
	BurstBatchSize int `json:"burst_batch_size"` // Small batches when reconnecting
	BurstDelayMs   int `json:"burst_delay_ms"`   // Sleep between batches

	// Clock sanity (PM #8)
	MaxClockSkewMin int `json:"max_clock_skew_min"` // Max ±minutes from now

	// Neighbor selection (PM #9)
	RandomNeighborCount int `json:"random_neighbor_count"` // Random neighbors from next tier
	TopNeighborCount    int `json:"top_neighbor_count"`    // Top neighbors by RSRP
}

// BurstSmoother handles offline queue burst smoothing (PM #7)
type BurstSmoother struct {
	batchSize int
	delayMs   int
	logger    *logx.Logger
}

// ClockSkewDetector handles timestamp sanity (PM #8)
type ClockSkewDetector struct {
	maxSkew time.Duration
	logger  *logx.Logger
}

// SubmissionStats tracks detailed metrics (PM #10)
type SubmissionStats struct {
	// Submission reasons breakdown (PM #10)
	NewCellSubmissions    int64 `json:"new_cell_submissions"`
	MovementSubmissions   int64 `json:"movement_submissions"`
	RSRPChangeSubmissions int64 `json:"rsrp_change_submissions"`
	ValidationTrickle     int64 `json:"validation_trickle"`

	// Deduplication stats
	DuplicatesBlocked  int64 `json:"duplicates_blocked"`
	FingerprintsActive int   `json:"fingerprints_active"`

	// Stationary caps
	StationaryBlocked     int64 `json:"stationary_blocked"`
	StationarySubmissions int64 `json:"stationary_submissions"`

	// Burst smoothing
	BurstBatchesProcessed int64 `json:"burst_batches_processed"`
	OfflineQueueSize      int   `json:"offline_queue_size"`

	// Clock sanity
	TimestampsClamped int64 `json:"timestamps_clamped"`
	ClockSkewDetected int64 `json:"clock_skew_detected"`

	// Neighbor selection
	BiasedSelectionAvoided  int64 `json:"biased_selection_avoided"`
	RandomNeighborsSelected int64 `json:"random_neighbors_selected"`
}

// DefaultEnhancedSubmissionConfig returns safe defaults
func DefaultEnhancedSubmissionConfig() *EnhancedSubmissionConfig {
	return &EnhancedSubmissionConfig{
		MinGPSAccuracy:       20.0,
		MovementThreshold:    250.0,
		RSRPChangeThreshold:  6.0,
		ContributionInterval: 10 * time.Minute,

		// PM feedback defaults
		DedupeGridSizeM:       75.0,          // ~75m grid for deduplication
		DedupeTimeWindowH:     1,             // 1-hour buckets
		StationaryMaxInterval: 3 * time.Hour, // Max 1 per cell per 3h when stationary
		StationaryGlobalCap:   2,             // Max 2/hour when stationary
		BurstBatchSize:        3,             // Small batches
		BurstDelayMs:          2000,          // 2s between batches
		MaxClockSkewMin:       15,            // ±15 minutes max
		RandomNeighborCount:   2,             // 2 random from next tier
		TopNeighborCount:      3,             // Top 3 by RSRP
	}
}

// NewEnhancedSubmissionManager creates enhanced submission manager
func NewEnhancedSubmissionManager(config *EnhancedSubmissionConfig, rateLimiter *EnhancedRateLimiter, logger *logx.Logger) *EnhancedSubmissionManager {
	if config == nil {
		config = DefaultEnhancedSubmissionConfig()
	}

	return &EnhancedSubmissionManager{
		logger:                 logger,
		config:                 config,
		rateLimiter:            rateLimiter,
		submissionFingerprints: make(map[string]time.Time),
		offlineQueue:           make([]CellObservation, 0),
		stationarySubmissions:  make(map[string]time.Time),
		burstSmoother: &BurstSmoother{
			batchSize: config.BurstBatchSize,
			delayMs:   config.BurstDelayMs,
			logger:    logger,
		},
		clockSkewDetector: &ClockSkewDetector{
			maxSkew: time.Duration(config.MaxClockSkewMin) * time.Minute,
			logger:  logger,
		},
		stats: &SubmissionStats{},
	}
}

// ShouldSubmitObservation enhanced submission decision with PM feedback
func (esm *EnhancedSubmissionManager) ShouldSubmitObservation(observation CellObservation, isMoving bool) SubmissionDecision {
	esm.mu.Lock()
	defer esm.mu.Unlock()

	// Clock sanity check (PM #8)
	observation.GPS.Timestamp = esm.clockSkewDetector.SanitizeTimestamp(observation.GPS.Timestamp)

	// GPS accuracy validation
	if observation.GPS.Accuracy > esm.config.MinGPSAccuracy {
		return SubmissionDecision{
			ShouldSubmit: false,
			Reason: fmt.Sprintf("gps_accuracy_insufficient: %.1fm > %.1fm",
				observation.GPS.Accuracy, esm.config.MinGPSAccuracy),
		}
	}

	cellKey := esm.generateCellKey(observation.ServingCell)

	// Deduplication check (PM #5)
	fingerprint := esm.generateSubmissionFingerprint(observation)
	if lastSeen, exists := esm.submissionFingerprints[fingerprint]; exists {
		if time.Since(lastSeen) < time.Hour { // Within same hour bucket
			esm.stats.DuplicatesBlocked++
			return SubmissionDecision{
				ShouldSubmit: false,
				Reason:       "duplicate_fingerprint",
				CellKey:      cellKey,
			}
		}
	}

	// Stationary caps (PM #6)
	if !isMoving {
		if lastSubmission, exists := esm.stationarySubmissions[cellKey]; exists {
			if time.Since(lastSubmission) < esm.config.StationaryMaxInterval {
				esm.stats.StationaryBlocked++
				return SubmissionDecision{
					ShouldSubmit: false,
					Reason: fmt.Sprintf("stationary_cell_interval: %v < %v",
						time.Since(lastSubmission), esm.config.StationaryMaxInterval),
					CellKey: cellKey,
				}
			}
		}

		// Global stationary cap
		stationaryThisHour := esm.countStationarySubmissionsThisHour()
		if stationaryThisHour >= esm.config.StationaryGlobalCap {
			esm.stats.StationaryBlocked++
			return SubmissionDecision{
				ShouldSubmit: false,
				Reason: fmt.Sprintf("stationary_global_cap: %d >= %d",
					stationaryThisHour, esm.config.StationaryGlobalCap),
				CellKey: cellKey,
			}
		}
	}

	// Check minimum trickle requirement (PM #1)
	if esm.rateLimiter.ShouldTrickleSubmit(isMoving, observation.GPS.Accuracy <= esm.config.MinGPSAccuracy) {
		esm.recordSubmission(observation, fingerprint, cellKey, isMoving, "validation_trickle")
		esm.stats.ValidationTrickle++
		return SubmissionDecision{
			ShouldSubmit: true,
			Reason:       "validation_trickle",
			CellKey:      cellKey,
			Triggers:     []string{"trickle"},
		}
	}

	// Standard submission logic with enhanced tracking
	decision := esm.evaluateStandardTriggers(observation, cellKey, isMoving)

	if decision.ShouldSubmit {
		esm.recordSubmission(observation, fingerprint, cellKey, isMoving, decision.Reason)
	}

	return decision
}

// generateSubmissionFingerprint creates deduplication fingerprint (PM #5)
func (esm *EnhancedSubmissionManager) generateSubmissionFingerprint(observation CellObservation) string {
	// Quantize location to grid (PM #5)
	gridLat := math.Floor(observation.GPS.Latitude*1000/esm.config.DedupeGridSizeM) * esm.config.DedupeGridSizeM / 1000
	gridLon := math.Floor(observation.GPS.Longitude*1000/esm.config.DedupeGridSizeM) * esm.config.DedupeGridSizeM / 1000

	// Hour bucket
	hourBucket := observation.GPS.Timestamp.Truncate(time.Hour).Unix()

	// Create fingerprint
	data := fmt.Sprintf("%s-%.6f-%.6f-%d",
		esm.generateCellKey(observation.ServingCell),
		gridLat, gridLon, hourBucket)

	return fmt.Sprintf("%x", sha256.Sum256([]byte(data)))[:16] // 16-char fingerprint
}

// generateCellKey creates consistent cell identifier
func (esm *EnhancedSubmissionManager) generateCellKey(cell ServingCellInfo) string {
	return fmt.Sprintf("%s-%s-%s-%s-%s",
		cell.MCC, cell.MNC, cell.TAC, cell.CellID, cell.Technology)
}

// evaluateStandardTriggers checks standard submission triggers
func (esm *EnhancedSubmissionManager) evaluateStandardTriggers(observation CellObservation, cellKey string, isMoving bool) SubmissionDecision {
	// Implementation would check:
	// - New cell trigger
	// - Movement threshold
	// - RSRP change threshold
	// - Time-based intervals

	// For now, simplified logic
	decision := SubmissionDecision{
		CellKey:  cellKey,
		Triggers: make([]string, 0),
	}

	// Example: always submit new cells
	if _, exists := esm.stationarySubmissions[cellKey]; !exists {
		decision.ShouldSubmit = true
		decision.Reason = "new_cell_observed"
		decision.Triggers = append(decision.Triggers, "new_cell")
		esm.stats.NewCellSubmissions++
		return decision
	}

	decision.ShouldSubmit = false
	decision.Reason = "no_triggers_met"
	return decision
}

// recordSubmission records submission with comprehensive tracking
func (esm *EnhancedSubmissionManager) recordSubmission(observation CellObservation, fingerprint, cellKey string, isMoving bool, reason string) {
	now := time.Now()

	// Record fingerprint
	esm.submissionFingerprints[fingerprint] = now

	// Record stationary submission
	if !isMoving {
		esm.stationarySubmissions[cellKey] = now
		esm.stats.StationarySubmissions++
	}

	// Update reason-specific stats
	switch reason {
	case "new_cell_observed":
		esm.stats.NewCellSubmissions++
	case "movement":
		esm.stats.MovementSubmissions++
	case "rsrp_change":
		esm.stats.RSRPChangeSubmissions++
	case "validation_trickle":
		esm.stats.ValidationTrickle++
	}

	// Cleanup old fingerprints periodically
	if secureRandomFloat32() < 0.01 { // 1% chance
		esm.cleanupOldFingerprints()
	}
}

// SelectNeighborsWithBiasAvoidance implements PM #9
func (esm *EnhancedSubmissionManager) SelectNeighborsWithBiasAvoidance(neighbors []NeighborCellInfo, maxNeighbors int) []NeighborCellInfo {
	if len(neighbors) <= maxNeighbors {
		return neighbors
	}

	// Sort by RSRP (strongest first)
	sortedNeighbors := make([]NeighborCellInfo, len(neighbors))
	copy(sortedNeighbors, neighbors)

	// Simple bubble sort by RSRP (descending)
	for i := 0; i < len(sortedNeighbors)-1; i++ {
		for j := i + 1; j < len(sortedNeighbors); j++ {
			if sortedNeighbors[i].RSRP < sortedNeighbors[j].RSRP {
				sortedNeighbors[i], sortedNeighbors[j] = sortedNeighbors[j], sortedNeighbors[i]
			}
		}
	}

	selected := make([]NeighborCellInfo, 0, maxNeighbors)

	// Take top N neighbors
	topCount := esm.config.TopNeighborCount
	if topCount > len(sortedNeighbors) {
		topCount = len(sortedNeighbors)
	}
	if topCount > maxNeighbors {
		topCount = maxNeighbors
	}

	for i := 0; i < topCount; i++ {
		selected = append(selected, sortedNeighbors[i])
	}

	// Randomly select from remaining neighbors (PM #9)
	remaining := maxNeighbors - topCount
	if remaining > 0 && len(sortedNeighbors) > topCount {
		randomCount := esm.config.RandomNeighborCount
		if randomCount > remaining {
			randomCount = remaining
		}
		if randomCount > len(sortedNeighbors)-topCount {
			randomCount = len(sortedNeighbors) - topCount
		}

		// Random selection from next tier
		for i := 0; i < randomCount; i++ {
			randomIdx := topCount + secureRandomInt(len(sortedNeighbors)-topCount-i)
			selected = append(selected, sortedNeighbors[randomIdx])
			// Remove selected item
			sortedNeighbors[randomIdx] = sortedNeighbors[len(sortedNeighbors)-1-i]
		}

		esm.stats.BiasedSelectionAvoided++
		esm.stats.RandomNeighborsSelected += int64(randomCount)
	}

	return selected
}

// SanitizeTimestamp implements clock sanity checking (PM #8)
func (csd *ClockSkewDetector) SanitizeTimestamp(timestamp time.Time) time.Time {
	now := time.Now()
	skew := timestamp.Sub(now)

	if skew > csd.maxSkew || skew < -csd.maxSkew {
		csd.logger.Warn("clock_skew_detected",
			"timestamp", timestamp.Format(time.RFC3339),
			"now", now.Format(time.RFC3339),
			"skew_minutes", skew.Minutes(),
			"max_skew_minutes", csd.maxSkew.Minutes(),
		)

		// Clamp to acceptable range
		if skew > csd.maxSkew {
			return now.Add(csd.maxSkew)
		} else {
			return now.Add(-csd.maxSkew)
		}
	}

	return timestamp
}

// countStationarySubmissionsThisHour counts stationary submissions in current hour
func (esm *EnhancedSubmissionManager) countStationarySubmissionsThisHour() int {
	hourAgo := time.Now().Add(-time.Hour)
	count := 0

	for _, timestamp := range esm.stationarySubmissions {
		if timestamp.After(hourAgo) {
			count++
		}
	}

	return count
}

// cleanupOldFingerprints removes expired fingerprints
func (esm *EnhancedSubmissionManager) cleanupOldFingerprints() {
	cutoff := time.Now().Add(-time.Duration(esm.config.DedupeTimeWindowH) * time.Hour)

	for fingerprint, timestamp := range esm.submissionFingerprints {
		if timestamp.Before(cutoff) {
			delete(esm.submissionFingerprints, fingerprint)
		}
	}

	esm.stats.FingerprintsActive = len(esm.submissionFingerprints)
}

// GetStats returns comprehensive submission statistics (PM #10)
func (esm *EnhancedSubmissionManager) GetStats() SubmissionStats {
	esm.mu.RLock()
	defer esm.mu.RUnlock()

	esm.stats.FingerprintsActive = len(esm.submissionFingerprints)
	esm.stats.OfflineQueueSize = len(esm.offlineQueue)

	return *esm.stats
}

// secureRandomInt generates a cryptographically secure random int in range [0, max)
func secureRandomInt(max int) int {
	if max <= 0 {
		return 0
	}

	var buf [8]byte
	_, err := rand.Read(buf[:])
	if err != nil {
		// Fallback to time-based seed if crypto/rand fails
		return int(time.Now().UnixNano() % int64(max))
	}

	val := binary.BigEndian.Uint64(buf[:])
	return int(val % uint64(max))
}
