package wifi

import (
	"context"
	"math"
	"sync"
	"time"

	"github.com/markus-lassfolk/autonomy/pkg"
	"github.com/markus-lassfolk/autonomy/pkg/gps"
	"github.com/markus-lassfolk/autonomy/pkg/logx"
)

// GPSWiFiManager manages GPS-based WiFi optimization triggers
type GPSWiFiManager struct {
	optimizer    *WiFiOptimizer
	gpsCollector gps.ComprehensiveGPSCollectorInterface
	logger       *logx.Logger
	config       *GPSWiFiConfig

	// State tracking
	lastLocation    *pkg.GPSData
	lastOptimized   time.Time
	stationaryStart time.Time
	isStationary    bool
	mu              sync.RWMutex
}

// GPSWiFiConfig represents GPS-WiFi integration configuration
type GPSWiFiConfig struct {
	Enabled              bool          `json:"enabled"`
	MovementThreshold    float64       `json:"movement_threshold_m"`   // meters
	StationaryTime       time.Duration `json:"stationary_time"`        // time to be stationary before optimization
	OptimizationCooldown time.Duration `json:"optimization_cooldown"`  // minimum time between optimizations
	GPSAccuracyThreshold float64       `json:"gps_accuracy_threshold"` // minimum GPS accuracy required
	LocationLogging      bool          `json:"location_logging"`       // enable detailed location logging
}

// LocationState represents the current location state
type LocationState struct {
	Current       *pkg.GPSData  `json:"current"`
	Previous      *pkg.GPSData  `json:"previous"`
	Distance      float64       `json:"distance_m"`
	IsMoving      bool          `json:"is_moving"`
	Stationary    time.Duration `json:"stationary_duration"`
	LastOptimized time.Time     `json:"last_optimized"`
}

// NewGPSWiFiManager creates a new GPS-WiFi integration manager
func NewGPSWiFiManager(optimizer *WiFiOptimizer, gpsCollector gps.ComprehensiveGPSCollectorInterface, logger *logx.Logger, config *GPSWiFiConfig) *GPSWiFiManager {
	if config == nil {
		config = DefaultGPSWiFiConfig()
	}

	return &GPSWiFiManager{
		optimizer:    optimizer,
		gpsCollector: gpsCollector,
		logger:       logger,
		config:       config,
	}
}

// DefaultGPSWiFiConfig returns default GPS-WiFi integration configuration
func DefaultGPSWiFiConfig() *GPSWiFiConfig {
	return &GPSWiFiConfig{
		Enabled:              true,
		MovementThreshold:    100.0,            // 100 meters - more sensitive than main GPS system (500m)
		StationaryTime:       30 * time.Minute, // Use existing GPS system default
		OptimizationCooldown: 2 * time.Hour,
		GPSAccuracyThreshold: 50.0, // 50 meters
		LocationLogging:      true,
	}
}

// Start begins GPS monitoring for WiFi optimization triggers
func (gwm *GPSWiFiManager) Start(ctx context.Context) error {
	if !gwm.config.Enabled {
		gwm.logger.Info("GPS-WiFi integration disabled")
		return nil
	}

	gwm.logger.Info("Starting GPS-WiFi integration manager",
		"movement_threshold", gwm.config.MovementThreshold,
		"stationary_time", gwm.config.StationaryTime,
		"optimization_cooldown", gwm.config.OptimizationCooldown)

	// Start GPS monitoring loop
	go gwm.monitoringLoop(ctx)

	return nil
}

// monitoringLoop runs the main GPS monitoring loop
func (gwm *GPSWiFiManager) monitoringLoop(ctx context.Context) {
	ticker := time.NewTicker(30 * time.Second) // Check GPS every 30 seconds
	defer ticker.Stop()

	for {
		select {
		case <-ctx.Done():
			gwm.logger.Info("GPS-WiFi monitoring loop stopped")
			return
		case <-ticker.C:
			if err := gwm.checkLocationAndOptimize(ctx); err != nil {
				gwm.logger.Error("Error in GPS-WiFi monitoring", "error", err)
			}
		}
	}
}

// checkLocationAndOptimize checks current location and triggers optimization if needed
func (gwm *GPSWiFiManager) checkLocationAndOptimize(ctx context.Context) error {
	// Get current GPS position
	currentGPS, err := gwm.gpsCollector.CollectGPS(ctx)
	if err != nil {
		gwm.logger.Debug("Failed to collect GPS data", "error", err)
		return nil // Don't treat GPS failures as critical errors
	}

	// Validate GPS accuracy
	if currentGPS.Accuracy > gwm.config.GPSAccuracyThreshold {
		gwm.logger.Debug("GPS accuracy too low for WiFi optimization",
			"accuracy", currentGPS.Accuracy,
			"threshold", gwm.config.GPSAccuracyThreshold)
		return nil
	}

	gwm.mu.Lock()
	defer gwm.mu.Unlock()

	// Log location if enabled
	if gwm.config.LocationLogging {
		gwm.logger.LogVerbose("gps_location_update", map[string]interface{}{
			"latitude":   currentGPS.Latitude,
			"longitude":  currentGPS.Longitude,
			"accuracy":   currentGPS.Accuracy,
			"source":     currentGPS.Source,
			"satellites": currentGPS.Satellites,
		})
	}

	// Check if this is the first location reading
	if gwm.lastLocation == nil {
		gwm.lastLocation = currentGPS
		gwm.stationaryStart = time.Now()
		gwm.isStationary = true

		gwm.logger.Info("Initial GPS location recorded for WiFi optimization",
			"latitude", currentGPS.Latitude,
			"longitude", currentGPS.Longitude,
			"accuracy", currentGPS.Accuracy,
			"source", currentGPS.Source)
		return nil
	}

	// Calculate distance from last position
	distance := gwm.calculateDistance(gwm.lastLocation, currentGPS)

	// Determine if we've moved significantly
	hasMoved := distance > gwm.config.MovementThreshold

	if hasMoved {
		// Movement detected
		if gwm.isStationary {
			gwm.logger.Info("Movement detected, resetting stationary timer",
				"distance", distance,
				"threshold", gwm.config.MovementThreshold,
				"from_lat", gwm.lastLocation.Latitude,
				"from_lon", gwm.lastLocation.Longitude,
				"to_lat", currentGPS.Latitude,
				"to_lon", currentGPS.Longitude)
		}

		gwm.isStationary = false
		gwm.stationaryStart = time.Now()
		gwm.lastLocation = currentGPS

		// Clear location trigger since we're moving
		gwm.optimizer.SetLocationTrigger(false)

	} else {
		// No significant movement
		if !gwm.isStationary {
			// Just became stationary
			gwm.logger.Info("Became stationary, starting timer for WiFi optimization",
				"stationary_time_required", gwm.config.StationaryTime)
			gwm.isStationary = true
			gwm.stationaryStart = time.Now()
		}

		// Check if we've been stationary long enough
		stationaryDuration := time.Since(gwm.stationaryStart)
		if stationaryDuration >= gwm.config.StationaryTime {
			// Check optimization cooldown
			timeSinceLastOptimization := time.Since(gwm.lastOptimized)
			if timeSinceLastOptimization >= gwm.config.OptimizationCooldown {
				// Trigger WiFi optimization
				if err := gwm.triggerOptimization(ctx, currentGPS, distance, stationaryDuration); err != nil {
					gwm.logger.Error("Failed to trigger WiFi optimization", "error", err)
					return err
				}
			} else {
				gwm.logger.Debug("WiFi optimization on cooldown",
					"time_since_last", timeSinceLastOptimization,
					"cooldown_period", gwm.config.OptimizationCooldown)
			}
		} else {
			gwm.logger.Debug("Waiting for stationary period to complete",
				"stationary_duration", stationaryDuration,
				"required_duration", gwm.config.StationaryTime)
		}
	}

	return nil
}

// triggerOptimization triggers WiFi channel optimization
func (gwm *GPSWiFiManager) triggerOptimization(ctx context.Context, currentGPS *pkg.GPSData, distance float64, stationaryDuration time.Duration) error {
	gwm.logger.Info("Triggering GPS-based WiFi optimization",
		"latitude", currentGPS.Latitude,
		"longitude", currentGPS.Longitude,
		"distance_moved", distance,
		"stationary_duration", stationaryDuration,
		"gps_source", currentGPS.Source,
		"gps_accuracy", currentGPS.Accuracy)

	// Set location trigger
	gwm.optimizer.SetLocationTrigger(true)

	// Trigger optimization
	err := gwm.optimizer.OptimizeChannels(ctx, "location_change")
	if err != nil {
		gwm.logger.Error("GPS-triggered WiFi optimization failed", "error", err)
		return err
	}

	// Update state
	gwm.lastOptimized = time.Now()
	gwm.optimizer.SetLocationTrigger(false) // Reset trigger after use

	// Log success with location context
	gwm.logger.LogStateChange("wifi_optimizer", "idle", "optimized", "gps_location_trigger", map[string]interface{}{
		"trigger_type":        "location_change",
		"latitude":            currentGPS.Latitude,
		"longitude":           currentGPS.Longitude,
		"distance_moved":      distance,
		"stationary_duration": stationaryDuration.Seconds(),
		"gps_source":          currentGPS.Source,
		"gps_accuracy":        currentGPS.Accuracy,
		"optimization_time":   time.Now().UTC().Format(time.RFC3339),
	})

	return nil
}

// calculateDistance calculates the distance between two GPS coordinates using Haversine formula
func (gwm *GPSWiFiManager) calculateDistance(from, to *pkg.GPSData) float64 {
	const earthRadiusM = 6371000 // Earth's radius in meters

	lat1Rad := from.Latitude * math.Pi / 180
	lat2Rad := to.Latitude * math.Pi / 180
	deltaLatRad := (to.Latitude - from.Latitude) * math.Pi / 180
	deltaLonRad := (to.Longitude - from.Longitude) * math.Pi / 180

	a := math.Sin(deltaLatRad/2)*math.Sin(deltaLatRad/2) +
		math.Cos(lat1Rad)*math.Cos(lat2Rad)*
			math.Sin(deltaLonRad/2)*math.Sin(deltaLonRad/2)
	c := 2 * math.Atan2(math.Sqrt(a), math.Sqrt(1-a))

	return earthRadiusM * c
}

// GetLocationState returns current location state
func (gwm *GPSWiFiManager) GetLocationState() *LocationState {
	gwm.mu.RLock()
	defer gwm.mu.RUnlock()

	var distance float64
	if gwm.lastLocation != nil {
		// For current state, distance is 0 since we're comparing to self
		distance = 0
	}

	var stationaryDuration time.Duration
	if gwm.isStationary {
		stationaryDuration = time.Since(gwm.stationaryStart)
	}

	return &LocationState{
		Current:       gwm.lastLocation,
		Previous:      gwm.lastLocation, // Simplified for status
		Distance:      distance,
		IsMoving:      !gwm.isStationary,
		Stationary:    stationaryDuration,
		LastOptimized: gwm.lastOptimized,
	}
}

// ForceOptimization forces WiFi optimization regardless of location state
func (gwm *GPSWiFiManager) ForceOptimization(ctx context.Context) error {
	gwm.mu.Lock()
	defer gwm.mu.Unlock()

	gwm.logger.Info("Forcing WiFi optimization (manual trigger)")

	// Set location trigger temporarily
	gwm.optimizer.SetLocationTrigger(true)

	// Trigger optimization
	err := gwm.optimizer.OptimizeChannels(ctx, "manual")
	if err != nil {
		gwm.logger.Error("Manual WiFi optimization failed", "error", err)
		return err
	}

	// Update state
	gwm.lastOptimized = time.Now()
	gwm.optimizer.SetLocationTrigger(false)

	gwm.logger.Info("Manual WiFi optimization completed successfully")
	return nil
}

// SetConfig updates the GPS-WiFi configuration
func (gwm *GPSWiFiManager) SetConfig(config *GPSWiFiConfig) {
	gwm.mu.Lock()
	defer gwm.mu.Unlock()

	gwm.config = config
	gwm.logger.Info("GPS-WiFi configuration updated",
		"enabled", config.Enabled,
		"movement_threshold", config.MovementThreshold,
		"stationary_time", config.StationaryTime)
}

// GetConfig returns current GPS-WiFi configuration
func (gwm *GPSWiFiManager) GetConfig() *GPSWiFiConfig {
	gwm.mu.RLock()
	defer gwm.mu.RUnlock()

	// Return a copy to prevent external modification
	configCopy := *gwm.config
	return &configCopy
}

// IsEnabled returns whether GPS-WiFi integration is enabled
func (gwm *GPSWiFiManager) IsEnabled() bool {
	gwm.mu.RLock()
	defer gwm.mu.RUnlock()
	return gwm.config.Enabled
}

// GetStatus returns comprehensive status information
func (gwm *GPSWiFiManager) GetStatus() map[string]interface{} {
	gwm.mu.RLock()
	defer gwm.mu.RUnlock()

	status := map[string]interface{}{
		"enabled":               gwm.config.Enabled,
		"movement_threshold":    gwm.config.MovementThreshold,
		"stationary_time":       gwm.config.StationaryTime,
		"optimization_cooldown": gwm.config.OptimizationCooldown,
		"is_stationary":         gwm.isStationary,
		"last_optimized":        gwm.lastOptimized,
	}

	if gwm.lastLocation != nil {
		status["current_location"] = map[string]interface{}{
			"latitude":  gwm.lastLocation.Latitude,
			"longitude": gwm.lastLocation.Longitude,
			"accuracy":  gwm.lastLocation.Accuracy,
			"source":    gwm.lastLocation.Source,
			"timestamp": gwm.lastLocation.Timestamp,
		}
	}

	if gwm.isStationary {
		status["stationary_duration"] = time.Since(gwm.stationaryStart).Seconds()
		status["stationary_remaining"] = (gwm.config.StationaryTime - time.Since(gwm.stationaryStart)).Seconds()
	}

	// Add cooldown information
	timeSinceLastOptimization := time.Since(gwm.lastOptimized)
	status["time_since_last_optimization"] = timeSinceLastOptimization.Seconds()
	if timeSinceLastOptimization < gwm.config.OptimizationCooldown {
		status["cooldown_remaining"] = (gwm.config.OptimizationCooldown - timeSinceLastOptimization).Seconds()
	}

	return status
}

// NightlyOptimizationManager handles scheduled nightly WiFi optimization
type NightlyOptimizationManager struct {
	optimizer *WiFiOptimizer
	logger    *logx.Logger
	config    *NightlyConfig
	lastRun   time.Time
	mu        sync.RWMutex
}

// NightlyConfig represents nightly optimization configuration
type NightlyConfig struct {
	Enabled     bool   `json:"enabled"`
	Time        string `json:"time"`         // HH:MM format
	WindowHours int    `json:"window_hours"` // Hours window for execution
}

// NewNightlyOptimizationManager creates a new nightly optimization manager
func NewNightlyOptimizationManager(optimizer *WiFiOptimizer, logger *logx.Logger, config *NightlyConfig) *NightlyOptimizationManager {
	if config == nil {
		config = &NightlyConfig{
			Enabled:     true,
			Time:        "03:00",
			WindowHours: 1,
		}
	}

	return &NightlyOptimizationManager{
		optimizer: optimizer,
		logger:    logger,
		config:    config,
	}
}

// Start begins nightly optimization monitoring
func (nom *NightlyOptimizationManager) Start(ctx context.Context) error {
	if !nom.config.Enabled {
		nom.logger.Info("Nightly WiFi optimization disabled")
		return nil
	}

	nom.logger.Info("Starting nightly WiFi optimization manager",
		"time", nom.config.Time,
		"window_hours", nom.config.WindowHours)

	// Start monitoring loop
	go nom.monitoringLoop(ctx)

	return nil
}

// monitoringLoop runs the nightly optimization monitoring loop
func (nom *NightlyOptimizationManager) monitoringLoop(ctx context.Context) {
	ticker := time.NewTicker(10 * time.Minute) // Check every 10 minutes
	defer ticker.Stop()

	for {
		select {
		case <-ctx.Done():
			nom.logger.Info("Nightly WiFi optimization monitoring stopped")
			return
		case <-ticker.C:
			if err := nom.checkAndRunNightlyOptimization(ctx); err != nil {
				nom.logger.Error("Error in nightly WiFi optimization", "error", err)
			}
		}
	}
}

// checkAndRunNightlyOptimization checks if it's time for nightly optimization
func (nom *NightlyOptimizationManager) checkAndRunNightlyOptimization(ctx context.Context) error {
	nom.mu.Lock()
	defer nom.mu.Unlock()

	now := time.Now()

	// Parse target time
	targetTime, err := time.Parse("15:04", nom.config.Time)
	if err != nil {
		nom.logger.Error("Invalid nightly optimization time format",
			"time", nom.config.Time, "error", err)
		return err
	}

	// Create target time for today
	target := time.Date(now.Year(), now.Month(), now.Day(),
		targetTime.Hour(), targetTime.Minute(), 0, 0, now.Location())

	// Check if we're within the execution window
	windowDuration := time.Duration(nom.config.WindowHours) * time.Hour
	if now.Before(target) || now.After(target.Add(windowDuration)) {
		return nil // Not in execution window
	}

	// Check if we've already run today
	if nom.lastRun.Year() == now.Year() &&
		nom.lastRun.YearDay() == now.YearDay() {
		return nil // Already ran today
	}

	// Run nightly optimization
	nom.logger.Info("Running nightly WiFi optimization",
		"target_time", nom.config.Time,
		"actual_time", now.Format("15:04"))

	err = nom.optimizer.OptimizeChannels(ctx, "nightly")
	if err != nil {
		nom.logger.Error("Nightly WiFi optimization failed", "error", err)
		return err
	}

	nom.lastRun = now
	nom.logger.Info("Nightly WiFi optimization completed successfully")

	return nil
}

// GetStatus returns nightly optimization status
func (nom *NightlyOptimizationManager) GetStatus() map[string]interface{} {
	nom.mu.RLock()
	defer nom.mu.RUnlock()

	status := map[string]interface{}{
		"enabled":      nom.config.Enabled,
		"time":         nom.config.Time,
		"window_hours": nom.config.WindowHours,
		"last_run":     nom.lastRun,
	}

	// Calculate next run time
	now := time.Now()
	targetTime, err := time.Parse("15:04", nom.config.Time)
	if err == nil {
		nextRun := time.Date(now.Year(), now.Month(), now.Day(),
			targetTime.Hour(), targetTime.Minute(), 0, 0, now.Location())

		// If today's time has passed, schedule for tomorrow
		if now.After(nextRun) {
			nextRun = nextRun.Add(24 * time.Hour)
		}

		status["next_run"] = nextRun
		status["time_until_next_run"] = nextRun.Sub(now).Seconds()
	}

	return status
}
