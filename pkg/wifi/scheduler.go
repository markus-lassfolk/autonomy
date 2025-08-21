package wifi

import (
	"context"
	"fmt"
	"strings"
	"sync"
	"time"

	"github.com/markus-lassfolk/autonomy/pkg/logx"
)

// WiFiScheduler manages scheduled WiFi optimization tasks
type WiFiScheduler struct {
	optimizer *WiFiOptimizer
	logger    *logx.Logger
	config    *SchedulerConfig

	// State tracking
	running     bool
	lastNightly time.Time
	lastWeekly  time.Time
	nextNightly time.Time
	nextWeekly  time.Time
	mu          sync.RWMutex
	stopCh      chan struct{}
}

// SchedulerConfig represents scheduler configuration
type SchedulerConfig struct {
	// Nightly optimization
	NightlyEnabled   bool   `json:"nightly_enabled"`
	NightlyTime      string `json:"nightly_time"`       // HH:MM format
	NightlyWindowMin int    `json:"nightly_window_min"` // Minutes window for execution

	// Weekly optimization
	WeeklyEnabled   bool     `json:"weekly_enabled"`
	WeeklyDays      []string `json:"weekly_days"`       // ["monday", "wednesday", "friday"]
	WeeklyTime      string   `json:"weekly_time"`       // HH:MM format
	WeeklyWindowMin int      `json:"weekly_window_min"` // Minutes window for execution

	// General settings
	CheckIntervalMin int  `json:"check_interval_min"` // How often to check for scheduled tasks
	SkipIfRecent     bool `json:"skip_if_recent"`     // Skip if optimization happened recently
	RecentThresholdH int  `json:"recent_threshold_h"` // Hours threshold for "recent"

	// Timezone
	Timezone string `json:"timezone"` // Timezone for scheduling (default: local)
}

// ScheduleType represents the type of scheduled optimization
type ScheduleType string

const (
	ScheduleTypeNightly ScheduleType = "nightly"
	ScheduleTypeWeekly  ScheduleType = "weekly"
	ScheduleTypeManual  ScheduleType = "manual"
)

// ScheduledTask represents a scheduled optimization task
type ScheduledTask struct {
	Type        ScheduleType `json:"type"`
	ScheduledAt time.Time    `json:"scheduled_at"`
	ExecutedAt  time.Time    `json:"executed_at"`
	Success     bool         `json:"success"`
	Error       string       `json:"error,omitempty"`
	Trigger     string       `json:"trigger"`
}

// NewWiFiScheduler creates a new WiFi scheduler
func NewWiFiScheduler(optimizer *WiFiOptimizer, logger *logx.Logger, config *SchedulerConfig) *WiFiScheduler {
	if config == nil {
		config = DefaultSchedulerConfig()
	}

	return &WiFiScheduler{
		optimizer: optimizer,
		logger:    logger,
		config:    config,
		stopCh:    make(chan struct{}),
	}
}

// DefaultSchedulerConfig returns default scheduler configuration
func DefaultSchedulerConfig() *SchedulerConfig {
	return &SchedulerConfig{
		NightlyEnabled:   true,
		NightlyTime:      "03:00",
		NightlyWindowMin: 60, // 1 hour window

		WeeklyEnabled:   false,              // Disabled by default
		WeeklyDays:      []string{"sunday"}, // Sunday only by default
		WeeklyTime:      "02:00",
		WeeklyWindowMin: 120, // 2 hour window

		CheckIntervalMin: 10, // Check every 10 minutes
		SkipIfRecent:     true,
		RecentThresholdH: 6, // Skip if optimized in last 6 hours

		Timezone: "Local", // Use local timezone
	}
}

// Start begins the WiFi scheduler
func (ws *WiFiScheduler) Start(ctx context.Context) error {
	ws.mu.Lock()
	defer ws.mu.Unlock()

	if ws.running {
		return fmt.Errorf("WiFi scheduler is already running")
	}

	ws.running = true
	ws.calculateNextSchedules()

	ws.logger.Info("Starting WiFi scheduler",
		"nightly_enabled", ws.config.NightlyEnabled,
		"nightly_time", ws.config.NightlyTime,
		"weekly_enabled", ws.config.WeeklyEnabled,
		"weekly_days", ws.config.WeeklyDays,
		"weekly_time", ws.config.WeeklyTime,
		"check_interval_min", ws.config.CheckIntervalMin)

	// Start scheduler loop
	go ws.schedulerLoop(ctx)

	return nil
}

// Stop stops the WiFi scheduler
func (ws *WiFiScheduler) Stop() {
	ws.mu.Lock()
	defer ws.mu.Unlock()

	if !ws.running {
		return
	}

	ws.running = false
	close(ws.stopCh)
	ws.logger.Info("WiFi scheduler stopped")
}

// schedulerLoop runs the main scheduler loop
func (ws *WiFiScheduler) schedulerLoop(ctx context.Context) {
	checkInterval := time.Duration(ws.config.CheckIntervalMin) * time.Minute
	ticker := time.NewTicker(checkInterval)
	defer ticker.Stop()

	ws.logger.Info("WiFi scheduler loop started", "check_interval", checkInterval)

	for {
		select {
		case <-ctx.Done():
			ws.logger.Info("WiFi scheduler loop stopped (context cancelled)")
			return
		case <-ws.stopCh:
			ws.logger.Info("WiFi scheduler loop stopped (stop signal)")
			return
		case <-ticker.C:
			if err := ws.checkAndExecuteScheduledTasks(ctx); err != nil {
				ws.logger.Error("Error in scheduled task execution", "error", err)
			}
		}
	}
}

// checkAndExecuteScheduledTasks checks for and executes scheduled tasks
func (ws *WiFiScheduler) checkAndExecuteScheduledTasks(ctx context.Context) error {
	ws.mu.Lock()
	defer ws.mu.Unlock()

	now := time.Now()

	// Check nightly schedule
	if ws.config.NightlyEnabled && ws.shouldExecuteNightly(now) {
		task := &ScheduledTask{
			Type:        ScheduleTypeNightly,
			ScheduledAt: ws.nextNightly,
			Trigger:     "scheduled_nightly",
		}

		if err := ws.executeOptimization(ctx, task); err != nil {
			ws.logger.Error("Nightly WiFi optimization failed", "error", err)
		} else {
			ws.lastNightly = now
			ws.logger.Info("Nightly WiFi optimization completed successfully")
		}

		ws.calculateNextNightly()
	}

	// Check weekly schedule
	if ws.config.WeeklyEnabled && ws.shouldExecuteWeekly(now) {
		task := &ScheduledTask{
			Type:        ScheduleTypeWeekly,
			ScheduledAt: ws.nextWeekly,
			Trigger:     "scheduled_weekly",
		}

		if err := ws.executeOptimization(ctx, task); err != nil {
			ws.logger.Error("Weekly WiFi optimization failed", "error", err)
		} else {
			ws.lastWeekly = now
			ws.logger.Info("Weekly WiFi optimization completed successfully")
		}

		ws.calculateNextWeekly()
	}

	return nil
}

// shouldExecuteNightly checks if nightly optimization should execute
func (ws *WiFiScheduler) shouldExecuteNightly(now time.Time) bool {
	if ws.nextNightly.IsZero() {
		return false
	}

	// Check if we're within the execution window
	windowStart := ws.nextNightly
	windowEnd := ws.nextNightly.Add(time.Duration(ws.config.NightlyWindowMin) * time.Minute)

	if now.Before(windowStart) || now.After(windowEnd) {
		return false
	}

	// Check if already executed today
	if ws.lastNightly.Year() == now.Year() &&
		ws.lastNightly.YearDay() == now.YearDay() {
		return false
	}

	// Check if recent optimization should skip this
	if ws.config.SkipIfRecent && ws.wasOptimizedRecently(now) {
		ws.logger.Info("Skipping nightly optimization due to recent optimization",
			"last_optimization", ws.getLastOptimizationTime(),
			"threshold_hours", ws.config.RecentThresholdH)
		return false
	}

	return true
}

// shouldExecuteWeekly checks if weekly optimization should execute
func (ws *WiFiScheduler) shouldExecuteWeekly(now time.Time) bool {
	if ws.nextWeekly.IsZero() {
		return false
	}

	// Check if we're within the execution window
	windowStart := ws.nextWeekly
	windowEnd := ws.nextWeekly.Add(time.Duration(ws.config.WeeklyWindowMin) * time.Minute)

	if now.Before(windowStart) || now.After(windowEnd) {
		return false
	}

	// Check if already executed this week
	_, thisWeek := now.ISOWeek()
	_, lastWeek := ws.lastWeekly.ISOWeek()
	if ws.lastWeekly.Year() == now.Year() && lastWeek == thisWeek {
		return false
	}

	// Check if recent optimization should skip this
	if ws.config.SkipIfRecent && ws.wasOptimizedRecently(now) {
		ws.logger.Info("Skipping weekly optimization due to recent optimization",
			"last_optimization", ws.getLastOptimizationTime(),
			"threshold_hours", ws.config.RecentThresholdH)
		return false
	}

	return true
}

// executeOptimization executes a scheduled optimization
func (ws *WiFiScheduler) executeOptimization(ctx context.Context, task *ScheduledTask) error {
	ws.logger.Info("Executing scheduled WiFi optimization",
		"type", task.Type,
		"scheduled_at", task.ScheduledAt,
		"trigger", task.Trigger)

	task.ExecutedAt = time.Now()

	// Set location trigger for scheduled optimization
	ws.optimizer.SetLocationTrigger(true)

	// Execute optimization
	err := ws.optimizer.OptimizeChannels(ctx, task.Trigger)
	if err != nil {
		task.Success = false
		task.Error = err.Error()
		return err
	}

	task.Success = true
	ws.optimizer.SetLocationTrigger(false)

	// Log success with scheduling context
	ws.logger.LogStateChange("wifi_scheduler", "idle", "optimized", "scheduled_optimization", map[string]interface{}{
		"schedule_type":     task.Type,
		"scheduled_at":      task.ScheduledAt.UTC().Format(time.RFC3339),
		"executed_at":       task.ExecutedAt.UTC().Format(time.RFC3339),
		"optimization_time": time.Now().UTC().Format(time.RFC3339),
		"trigger":           task.Trigger,
	})

	return nil
}

// calculateNextSchedules calculates next nightly and weekly schedules
func (ws *WiFiScheduler) calculateNextSchedules() {
	ws.calculateNextNightly()
	ws.calculateNextWeekly()
}

// calculateNextNightly calculates the next nightly optimization time
func (ws *WiFiScheduler) calculateNextNightly() {
	if !ws.config.NightlyEnabled {
		ws.nextNightly = time.Time{}
		return
	}

	now := time.Now()
	targetTime, err := time.Parse("15:04", ws.config.NightlyTime)
	if err != nil {
		ws.logger.Error("Invalid nightly time format", "time", ws.config.NightlyTime, "error", err)
		ws.nextNightly = time.Time{}
		return
	}

	// Calculate next nightly time
	next := time.Date(now.Year(), now.Month(), now.Day(),
		targetTime.Hour(), targetTime.Minute(), 0, 0, now.Location())

	// If today's time has passed, schedule for tomorrow
	if now.After(next) {
		next = next.Add(24 * time.Hour)
	}

	ws.nextNightly = next
	ws.logger.Debug("Next nightly optimization scheduled",
		"time", ws.nextNightly.Format(time.RFC3339))
}

// calculateNextWeekly calculates the next weekly optimization time
func (ws *WiFiScheduler) calculateNextWeekly() {
	if !ws.config.WeeklyEnabled || len(ws.config.WeeklyDays) == 0 {
		ws.nextWeekly = time.Time{}
		return
	}

	now := time.Now()
	targetTime, err := time.Parse("15:04", ws.config.WeeklyTime)
	if err != nil {
		ws.logger.Error("Invalid weekly time format", "time", ws.config.WeeklyTime, "error", err)
		ws.nextWeekly = time.Time{}
		return
	}

	// Find next matching weekday
	var nextWeekly time.Time
	for i := 0; i < 7; i++ {
		candidate := now.Add(time.Duration(i) * 24 * time.Hour)
		weekday := strings.ToLower(candidate.Weekday().String())

		if ws.isWeekdayEnabled(weekday) {
			candidateTime := time.Date(candidate.Year(), candidate.Month(), candidate.Day(),
				targetTime.Hour(), targetTime.Minute(), 0, 0, candidate.Location())

			// If it's today but time has passed, skip to next occurrence
			if i == 0 && now.After(candidateTime) {
				continue
			}

			nextWeekly = candidateTime
			break
		}
	}

	ws.nextWeekly = nextWeekly
	if !ws.nextWeekly.IsZero() {
		ws.logger.Debug("Next weekly optimization scheduled",
			"time", ws.nextWeekly.Format(time.RFC3339),
			"weekday", ws.nextWeekly.Weekday().String())
	}
}

// isWeekdayEnabled checks if a weekday is enabled for weekly optimization
func (ws *WiFiScheduler) isWeekdayEnabled(weekday string) bool {
	for _, day := range ws.config.WeeklyDays {
		if strings.ToLower(day) == weekday {
			return true
		}
	}
	return false
}

// wasOptimizedRecently checks if optimization happened recently
func (ws *WiFiScheduler) wasOptimizedRecently(now time.Time) bool {
	lastOptimization := ws.getLastOptimizationTime()
	if lastOptimization.IsZero() {
		return false
	}

	threshold := time.Duration(ws.config.RecentThresholdH) * time.Hour
	return now.Sub(lastOptimization) < threshold
}

// getLastOptimizationTime gets the last optimization time from the optimizer
func (ws *WiFiScheduler) getLastOptimizationTime() time.Time {
	status := ws.optimizer.GetStatus()
	if lastOptimized, ok := status["last_optimized"].(time.Time); ok {
		return lastOptimized
	}
	return time.Time{}
}

// ForceNightly manually triggers nightly optimization
func (ws *WiFiScheduler) ForceNightly(ctx context.Context) error {
	ws.mu.Lock()
	defer ws.mu.Unlock()

	ws.logger.Info("Manually triggering nightly WiFi optimization")

	task := &ScheduledTask{
		Type:        ScheduleTypeNightly,
		ScheduledAt: time.Now(),
		Trigger:     "manual_nightly",
	}

	if err := ws.executeOptimization(ctx, task); err != nil {
		return fmt.Errorf("manual nightly optimization failed: %w", err)
	}

	ws.lastNightly = time.Now()
	return nil
}

// ForceWeekly manually triggers weekly optimization
func (ws *WiFiScheduler) ForceWeekly(ctx context.Context) error {
	ws.mu.Lock()
	defer ws.mu.Unlock()

	ws.logger.Info("Manually triggering weekly WiFi optimization")

	task := &ScheduledTask{
		Type:        ScheduleTypeWeekly,
		ScheduledAt: time.Now(),
		Trigger:     "manual_weekly",
	}

	if err := ws.executeOptimization(ctx, task); err != nil {
		return fmt.Errorf("manual weekly optimization failed: %w", err)
	}

	ws.lastWeekly = time.Now()
	return nil
}

// UpdateConfig updates the scheduler configuration
func (ws *WiFiScheduler) UpdateConfig(config *SchedulerConfig) {
	ws.mu.Lock()
	defer ws.mu.Unlock()

	ws.config = config
	ws.calculateNextSchedules()

	ws.logger.Info("WiFi scheduler configuration updated",
		"nightly_enabled", config.NightlyEnabled,
		"nightly_time", config.NightlyTime,
		"weekly_enabled", config.WeeklyEnabled,
		"weekly_days", config.WeeklyDays,
		"weekly_time", config.WeeklyTime)
}

// GetStatus returns scheduler status
func (ws *WiFiScheduler) GetStatus() map[string]interface{} {
	ws.mu.RLock()
	defer ws.mu.RUnlock()

	status := map[string]interface{}{
		"running":            ws.running,
		"nightly_enabled":    ws.config.NightlyEnabled,
		"nightly_time":       ws.config.NightlyTime,
		"weekly_enabled":     ws.config.WeeklyEnabled,
		"weekly_days":        ws.config.WeeklyDays,
		"weekly_time":        ws.config.WeeklyTime,
		"check_interval_min": ws.config.CheckIntervalMin,
		"last_nightly":       ws.lastNightly,
		"last_weekly":        ws.lastWeekly,
	}

	if !ws.nextNightly.IsZero() {
		status["next_nightly"] = ws.nextNightly
		status["next_nightly_in_hours"] = time.Until(ws.nextNightly).Hours()
	}

	if !ws.nextWeekly.IsZero() {
		status["next_weekly"] = ws.nextWeekly
		status["next_weekly_in_hours"] = time.Until(ws.nextWeekly).Hours()
	}

	return status
}

// GetNextSchedules returns upcoming scheduled optimizations
func (ws *WiFiScheduler) GetNextSchedules() []map[string]interface{} {
	ws.mu.RLock()
	defer ws.mu.RUnlock()

	var schedules []map[string]interface{}

	if !ws.nextNightly.IsZero() {
		schedules = append(schedules, map[string]interface{}{
			"type":         "nightly",
			"scheduled_at": ws.nextNightly,
			"in_hours":     time.Until(ws.nextNightly).Hours(),
		})
	}

	if !ws.nextWeekly.IsZero() {
		schedules = append(schedules, map[string]interface{}{
			"type":         "weekly",
			"scheduled_at": ws.nextWeekly,
			"in_hours":     time.Until(ws.nextWeekly).Hours(),
		})
	}

	return schedules
}
