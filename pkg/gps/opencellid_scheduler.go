package gps

import (
	"context"
	"fmt"
	"sync"
	"time"

	"github.com/markus-lassfolk/autonomy/pkg/logx"
)

// OpenCellIDScheduler manages automated OpenCellID operations
type OpenCellIDScheduler struct {
	logger           *logx.Logger
	config           *OpenCellIDSchedulerConfig
	source           *OpenCellIDGPSSource
	cellCollector    CellularDataCollector
	lastGPSLocation  *GPSObservation
	lastCellScan     time.Time
	lastContribution time.Time
	isMoving         bool
	movementDetector *MovementDetector

	// Control channels
	stopCh   chan struct{}
	pauseCh  chan bool
	isPaused bool

	// Statistics
	stats *SchedulerStats
	mu    sync.RWMutex
}

// OpenCellIDSchedulerConfig defines scheduling behavior
type OpenCellIDSchedulerConfig struct {
	// Base intervals
	StationaryInterval time.Duration `json:"stationary_interval"`  // When not moving (default: 10m)
	MovingInterval     time.Duration `json:"moving_interval"`      // When moving (default: 2m)
	FastMovingInterval time.Duration `json:"fast_moving_interval"` // When moving fast (default: 1m)

	// GPS quality thresholds
	GoodGPSAccuracy      float64 `json:"good_gps_accuracy"`      // Accuracy threshold for "good" GPS (default: 20m)
	ExcellentGPSAccuracy float64 `json:"excellent_gps_accuracy"` // Accuracy for frequent scanning (default: 10m)

	// Movement detection
	MovementThreshold float64       `json:"movement_threshold"`  // Distance to consider "moving" (default: 50m)
	FastMovementSpeed float64       `json:"fast_movement_speed"` // Speed for "fast moving" (default: 15 m/s = 54 km/h)
	MovementWindow    time.Duration `json:"movement_window"`     // Time window for movement detection (default: 5m)

	// Contribution scheduling
	ContributionInterval    time.Duration `json:"contribution_interval"`     // Base contribution interval (default: 10m)
	MovingContributionBoost float64       `json:"moving_contribution_boost"` // Multiplier when moving (default: 0.5 = 2x faster)
	ExcellentGPSBoost       float64       `json:"excellent_gps_boost"`       // Multiplier for excellent GPS (default: 0.7 = 1.4x faster)

	// Operational limits
	MaxScansPerHour         int           `json:"max_scans_per_hour"`         // Rate limit (default: 30)
	MaxContributionsPerHour int           `json:"max_contributions_per_hour"` // Rate limit (default: 6)
	MinCellScanInterval     time.Duration `json:"min_cell_scan_interval"`     // Absolute minimum (default: 30s)

	// Adaptive behavior
	EnableAdaptiveScheduling bool `json:"enable_adaptive_scheduling"` // Enable smart scheduling (default: true)
	BackoffOnRateLimit       bool `json:"backoff_on_rate_limit"`      // Slow down when rate limited (default: true)
}

// SchedulerStats tracks scheduler performance
type SchedulerStats struct {
	TotalScans            int64         `json:"total_scans"`
	TotalContributions    int64         `json:"total_contributions"`
	ScansThisHour         int           `json:"scans_this_hour"`
	ContributionsThisHour int           `json:"contributions_this_hour"`
	LastScanTime          time.Time     `json:"last_scan_time"`
	LastContributionTime  time.Time     `json:"last_contribution_time"`
	CurrentInterval       time.Duration `json:"current_interval"`
	IsMoving              bool          `json:"is_moving"`
	GPSAccuracy           float64       `json:"gps_accuracy"`
	RateLimitHits         int64         `json:"rate_limit_hits"`
	SchedulingErrors      int64         `json:"scheduling_errors"`
}

// DefaultOpenCellIDSchedulerConfig returns default configuration
func DefaultOpenCellIDSchedulerConfig() *OpenCellIDSchedulerConfig {
	return &OpenCellIDSchedulerConfig{
		StationaryInterval:       10 * time.Minute,
		MovingInterval:           2 * time.Minute,
		FastMovingInterval:       1 * time.Minute,
		GoodGPSAccuracy:          20.0,
		ExcellentGPSAccuracy:     10.0,
		MovementThreshold:        50.0,
		FastMovementSpeed:        15.0, // 54 km/h
		MovementWindow:           5 * time.Minute,
		ContributionInterval:     10 * time.Minute,
		MovingContributionBoost:  0.5, // 2x faster when moving
		ExcellentGPSBoost:        0.7, // 1.4x faster with excellent GPS
		MaxScansPerHour:          30,
		MaxContributionsPerHour:  6,
		MinCellScanInterval:      30 * time.Second,
		EnableAdaptiveScheduling: true,
		BackoffOnRateLimit:       true,
	}
}

// NewOpenCellIDScheduler creates a new scheduler
func NewOpenCellIDScheduler(source *OpenCellIDGPSSource, cellCollector CellularDataCollector, config *OpenCellIDSchedulerConfig, logger *logx.Logger) *OpenCellIDScheduler {
	if config == nil {
		config = DefaultOpenCellIDSchedulerConfig()
	}

	return &OpenCellIDScheduler{
		logger:           logger,
		config:           config,
		source:           source,
		cellCollector:    cellCollector,
		movementDetector: NewMovementDetector(config.MovementThreshold),
		stopCh:           make(chan struct{}),
		pauseCh:          make(chan bool, 1),
		stats:            &SchedulerStats{},
	}
}

// Start begins the automated scheduling
func (ocs *OpenCellIDScheduler) Start(ctx context.Context) error {
	ocs.logger.Info("opencellid_scheduler_starting",
		"stationary_interval", ocs.config.StationaryInterval,
		"moving_interval", ocs.config.MovingInterval,
		"max_scans_per_hour", ocs.config.MaxScansPerHour,
	)

	go ocs.schedulingLoop(ctx)
	go ocs.contributionLoop(ctx)
	go ocs.hourlyStatsReset()

	return nil
}

// Stop halts the scheduler
func (ocs *OpenCellIDScheduler) Stop() {
	ocs.logger.Info("opencellid_scheduler_stopping")
	close(ocs.stopCh)
}

// Pause temporarily stops scheduling
func (ocs *OpenCellIDScheduler) Pause() {
	ocs.mu.Lock()
	defer ocs.mu.Unlock()

	if !ocs.isPaused {
		ocs.isPaused = true
		select {
		case ocs.pauseCh <- true:
		default:
		}
		ocs.logger.Info("opencellid_scheduler_paused")
	}
}

// Resume restarts scheduling
func (ocs *OpenCellIDScheduler) Resume() {
	ocs.mu.Lock()
	defer ocs.mu.Unlock()

	if ocs.isPaused {
		ocs.isPaused = false
		select {
		case ocs.pauseCh <- false:
		default:
		}
		ocs.logger.Info("opencellid_scheduler_resumed")
	}
}

// schedulingLoop runs the main cell scanning loop
func (ocs *OpenCellIDScheduler) schedulingLoop(ctx context.Context) {
	ticker := time.NewTicker(ocs.config.MovingInterval) // Start with moving interval
	defer ticker.Stop()

	for {
		select {
		case <-ctx.Done():
			return
		case <-ocs.stopCh:
			return
		case paused := <-ocs.pauseCh:
			if paused {
				// Wait for resume
				for {
					select {
					case <-ctx.Done():
						return
					case <-ocs.stopCh:
						return
					case resumed := <-ocs.pauseCh:
						if !resumed {
							goto resumed
						}
					}
				}
			resumed:
			}
		case <-ticker.C:
			if err := ocs.performCellScan(ctx); err != nil {
				ocs.mu.Lock()
				ocs.stats.SchedulingErrors++
				ocs.mu.Unlock()

				ocs.logger.Warn("opencellid_scan_failed",
					"error", err.Error(),
				)
			}

			// Update ticker interval based on current conditions
			newInterval := ocs.calculateNextInterval()
			if newInterval != ocs.stats.CurrentInterval {
				ticker.Reset(newInterval)
				ocs.mu.Lock()
				ocs.stats.CurrentInterval = newInterval
				ocs.mu.Unlock()

				ocs.logger.Debug("opencellid_interval_adjusted",
					"new_interval", newInterval,
					"is_moving", ocs.isMoving,
					"gps_accuracy", ocs.stats.GPSAccuracy,
				)
			}
		}
	}
}

// contributionLoop manages periodic contributions
func (ocs *OpenCellIDScheduler) contributionLoop(ctx context.Context) {
	ticker := time.NewTicker(ocs.config.ContributionInterval)
	defer ticker.Stop()

	for {
		select {
		case <-ctx.Done():
			return
		case <-ocs.stopCh:
			return
		case <-ticker.C:
			if ocs.shouldContribute() {
				if err := ocs.performContribution(ctx); err != nil {
					ocs.logger.Warn("opencellid_contribution_failed",
						"error", err.Error(),
					)
				}
			}

			// Adjust contribution interval
			newInterval := ocs.calculateContributionInterval()
			ticker.Reset(newInterval)
		}
	}
}

// performCellScan executes a cell tower scan and lookup
func (ocs *OpenCellIDScheduler) performCellScan(ctx context.Context) error {
	// Check rate limits
	ocs.mu.RLock()
	if ocs.stats.ScansThisHour >= ocs.config.MaxScansPerHour {
		ocs.mu.RUnlock()
		ocs.mu.Lock()
		ocs.stats.RateLimitHits++
		ocs.mu.Unlock()
		return fmt.Errorf("hourly scan limit exceeded (%d)", ocs.config.MaxScansPerHour)
	}
	ocs.mu.RUnlock()

	// Check minimum interval
	if time.Since(ocs.lastCellScan) < ocs.config.MinCellScanInterval {
		return fmt.Errorf("minimum scan interval not met")
	}

	// Get current GPS location
	gpsData, err := ocs.source.CollectGPS(ctx)
	if err != nil {
		return fmt.Errorf("failed to get GPS location: %w", err)
	}

	// Update movement detection
	gpsObs := GPSObservation{
		Latitude:  gpsData.Latitude,
		Longitude: gpsData.Longitude,
		Accuracy:  gpsData.Accuracy,
		Speed:     gpsData.Speed,
		Heading:   gpsData.Course,
		Timestamp: gpsData.Timestamp,
	}

	moved, distance := ocs.movementDetector.DetectMovement(gpsObs)
	ocs.isMoving = moved || gpsData.Speed > 1.0 // 1 m/s threshold

	// Update stats
	ocs.mu.Lock()
	ocs.stats.TotalScans++
	ocs.stats.ScansThisHour++
	ocs.stats.LastScanTime = time.Now()
	ocs.stats.IsMoving = ocs.isMoving
	ocs.stats.GPSAccuracy = gpsData.Accuracy
	ocs.lastGPSLocation = &gpsObs
	ocs.lastCellScan = time.Now()
	ocs.mu.Unlock()

	ocs.logger.Debug("opencellid_scan_performed",
		"gps_accuracy", gpsData.Accuracy,
		"is_moving", ocs.isMoving,
		"distance_moved", distance,
		"scans_this_hour", ocs.stats.ScansThisHour,
	)

	return nil
}

// performContribution submits observations to OpenCellID
func (ocs *OpenCellIDScheduler) performContribution(ctx context.Context) error {
	// Check rate limits
	ocs.mu.RLock()
	if ocs.stats.ContributionsThisHour >= ocs.config.MaxContributionsPerHour {
		ocs.mu.RUnlock()
		return fmt.Errorf("hourly contribution limit exceeded (%d)", ocs.config.MaxContributionsPerHour)
	}
	ocs.mu.RUnlock()

	// Get cellular data
	servingCell, err := ocs.cellCollector.GetServingCell(ctx)
	if err != nil {
		return fmt.Errorf("failed to get serving cell: %w", err)
	}

	neighborCells, err := ocs.cellCollector.GetNeighborCells(ctx)
	if err != nil {
		ocs.logger.Warn("failed_to_get_neighbor_cells", "error", err.Error())
		neighborCells = []NeighborCellInfo{} // Continue with just serving cell
	}

	// Check if we have good GPS
	if ocs.lastGPSLocation == nil || ocs.lastGPSLocation.Accuracy > ocs.config.GoodGPSAccuracy {
		return fmt.Errorf("GPS accuracy insufficient for contribution: %.1fm", ocs.lastGPSLocation.Accuracy)
	}

	// Create observation
	observation := CellObservation{
		GPS:         *ocs.lastGPSLocation,
		ServingCell: *servingCell,
		Neighbors:   neighborCells,
		ObservedAt:  time.Now(),
	}

	// Check if we should submit this observation
	if ocs.source.contributionMgr != nil {
		decision := ocs.source.contributionMgr.ShouldSubmitObservation(observation)
		if !decision.ShouldSubmit {
			ocs.logger.Debug("contribution_skipped", "reason", decision.Reason)
			return nil
		}

		// Queue the observation
		ocs.source.contributionMgr.QueueObservation(&observation)

		// Try to flush immediately if conditions are good
		if err := ocs.source.contributionMgr.FlushPendingContributions(ctx); err != nil {
			ocs.logger.Debug("contribution_flush_deferred", "reason", err.Error())
		}
	}

	// Update stats
	ocs.mu.Lock()
	ocs.stats.TotalContributions++
	ocs.stats.ContributionsThisHour++
	ocs.stats.LastContributionTime = time.Now()
	ocs.lastContribution = time.Now()
	ocs.mu.Unlock()

	ocs.logger.Info("opencellid_contribution_queued",
		"gps_accuracy", ocs.lastGPSLocation.Accuracy,
		"serving_cell", servingCell.CellID,
		"neighbor_count", len(neighborCells),
		"contributions_this_hour", ocs.stats.ContributionsThisHour,
	)

	return nil
}

// calculateNextInterval determines the next scan interval based on conditions
func (ocs *OpenCellIDScheduler) calculateNextInterval() time.Duration {
	if !ocs.config.EnableAdaptiveScheduling {
		return ocs.config.MovingInterval
	}

	baseInterval := ocs.config.StationaryInterval

	// Adjust for movement
	if ocs.isMoving {
		if ocs.lastGPSLocation != nil && ocs.lastGPSLocation.Speed > ocs.config.FastMovementSpeed {
			baseInterval = ocs.config.FastMovingInterval
		} else {
			baseInterval = ocs.config.MovingInterval
		}
	}

	// Adjust for GPS quality
	if ocs.lastGPSLocation != nil && ocs.lastGPSLocation.Accuracy <= ocs.config.ExcellentGPSAccuracy {
		baseInterval = time.Duration(float64(baseInterval) * 0.8) // 20% faster with excellent GPS
	}

	// Backoff on rate limits
	if ocs.config.BackoffOnRateLimit && ocs.stats.ScansThisHour >= ocs.config.MaxScansPerHour-5 {
		baseInterval = time.Duration(float64(baseInterval) * 1.5) // Slow down near limit
	}

	// Enforce minimum
	if baseInterval < ocs.config.MinCellScanInterval {
		baseInterval = ocs.config.MinCellScanInterval
	}

	return baseInterval
}

// calculateContributionInterval determines the next contribution interval
func (ocs *OpenCellIDScheduler) calculateContributionInterval() time.Duration {
	baseInterval := ocs.config.ContributionInterval

	// Boost when moving
	if ocs.isMoving {
		baseInterval = time.Duration(float64(baseInterval) * ocs.config.MovingContributionBoost)
	}

	// Boost with excellent GPS
	if ocs.lastGPSLocation != nil && ocs.lastGPSLocation.Accuracy <= ocs.config.ExcellentGPSAccuracy {
		baseInterval = time.Duration(float64(baseInterval) * ocs.config.ExcellentGPSBoost)
	}

	// Minimum 2 minutes
	if baseInterval < 2*time.Minute {
		baseInterval = 2 * time.Minute
	}

	return baseInterval
}

// shouldContribute checks if conditions are right for contribution
func (ocs *OpenCellIDScheduler) shouldContribute() bool {
	// Check if we have recent GPS
	if ocs.lastGPSLocation == nil {
		return false
	}

	// Check GPS age (max 5 minutes old)
	if time.Since(ocs.lastGPSLocation.Timestamp) > 5*time.Minute {
		return false
	}

	// Check GPS accuracy
	if ocs.lastGPSLocation.Accuracy > ocs.config.GoodGPSAccuracy {
		return false
	}

	// Check rate limits
	ocs.mu.RLock()
	defer ocs.mu.RUnlock()

	return ocs.stats.ContributionsThisHour < ocs.config.MaxContributionsPerHour
}

// hourlyStatsReset resets hourly counters
func (ocs *OpenCellIDScheduler) hourlyStatsReset() {
	ticker := time.NewTicker(time.Hour)
	defer ticker.Stop()

	for {
		select {
		case <-ocs.stopCh:
			return
		case <-ticker.C:
			ocs.mu.Lock()
			ocs.stats.ScansThisHour = 0
			ocs.stats.ContributionsThisHour = 0
			ocs.mu.Unlock()

			ocs.logger.Debug("opencellid_hourly_stats_reset")
		}
	}
}

// GetStats returns current scheduler statistics
func (ocs *OpenCellIDScheduler) GetStats() SchedulerStats {
	ocs.mu.RLock()
	defer ocs.mu.RUnlock()

	return *ocs.stats
}
