package wifi

import (
	"context"
	"sync"
	"time"

	"github.com/markus-lassfolk/autonomy/pkg"
	"github.com/markus-lassfolk/autonomy/pkg/logx"
)

// GPSHook integrates WiFi optimization with the existing GPS system
// This hooks into the main GPS movement detection rather than running separately
type GPSHook struct {
	optimizer *WiFiOptimizer
	logger    *logx.Logger
	config    *GPSHookConfig

	// State tracking
	lastOptimizedLocation *pkg.GPSData
	lastOptimized         time.Time
	stationaryStart       time.Time
	isStationary          bool
	mu                    sync.RWMutex
}

// GPSHookConfig represents GPS hook configuration
type GPSHookConfig struct {
	Enabled                    bool          `json:"enabled"`
	WiFiMovementThreshold      float64       `json:"wifi_movement_threshold"`      // meters - more sensitive than main GPS
	UseMainGPSStationary       bool          `json:"use_main_gps_stationary"`      // use main GPS stationary detection
	OptimizationCooldown       time.Duration `json:"optimization_cooldown"`        // minimum time between optimizations
	RequireAccuracyImprovement bool          `json:"require_accuracy_improvement"` // only optimize if GPS accuracy improved
}

// NewGPSHook creates a new GPS hook for WiFi optimization
func NewGPSHook(optimizer *WiFiOptimizer, logger *logx.Logger, config *GPSHookConfig) *GPSHook {
	if config == nil {
		config = DefaultGPSHookConfig()
	}

	return &GPSHook{
		optimizer: optimizer,
		logger:    logger,
		config:    config,
	}
}

// DefaultGPSHookConfig returns default GPS hook configuration
func DefaultGPSHookConfig() *GPSHookConfig {
	return &GPSHookConfig{
		Enabled:                    true,
		WiFiMovementThreshold:      100.0, // 100m - more sensitive than main GPS (500m)
		UseMainGPSStationary:       true,  // leverage existing stationary detection
		OptimizationCooldown:       2 * time.Hour,
		RequireAccuracyImprovement: false, // optimize even if accuracy is same
	}
}

// OnMovementDetected is called by the main GPS system when movement is detected
// This allows us to piggyback on the existing GPS infrastructure
func (gh *GPSHook) OnMovementDetected(ctx context.Context, oldLocation, newLocation *pkg.GPSData, distance float64) error {
	if !gh.config.Enabled {
		return nil
	}

	gh.mu.Lock()
	defer gh.mu.Unlock()

	gh.logger.LogVerbose("gps_movement_detected_wifi_hook", map[string]interface{}{
		"distance_m":         distance,
		"main_gps_threshold": 500.0, // Main GPS system threshold
		"wifi_threshold":     gh.config.WiFiMovementThreshold,
		"old_lat":            oldLocation.Latitude,
		"old_lon":            oldLocation.Longitude,
		"new_lat":            newLocation.Latitude,
		"new_lon":            newLocation.Longitude,
	})

	// Reset stationary state since we moved
	gh.isStationary = false
	gh.stationaryStart = time.Now()

	// Check if we should also consider this movement for WiFi optimization
	// This handles the case where main GPS detected movement (>500m) but we want
	// to be more sensitive for WiFi (>100m)
	if gh.lastOptimizedLocation != nil {
		wifiDistance := gh.calculateDistance(gh.lastOptimizedLocation, newLocation)

		gh.logger.Debug("Checking WiFi-specific movement threshold",
			"wifi_distance", wifiDistance,
			"wifi_threshold", gh.config.WiFiMovementThreshold,
			"main_distance", distance)

		if wifiDistance > gh.config.WiFiMovementThreshold {
			// Significant movement for WiFi optimization purposes
			gh.logger.Info("Significant movement detected for WiFi optimization",
				"wifi_distance", wifiDistance,
				"main_distance", distance,
				"wifi_threshold", gh.config.WiFiMovementThreshold)

			// Reset optimization location so we'll optimize when stationary
			gh.lastOptimizedLocation = nil
		}
	}

	return nil
}

// OnStationaryDetected is called by the main GPS system when stationary period is detected
func (gh *GPSHook) OnStationaryDetected(ctx context.Context, currentLocation *pkg.GPSData, stationaryDuration time.Duration) error {
	if !gh.config.Enabled {
		return nil
	}

	gh.mu.Lock()
	defer gh.mu.Unlock()

	gh.isStationary = true

	gh.logger.LogVerbose("gps_stationary_detected_wifi_hook", map[string]interface{}{
		"stationary_duration_s": stationaryDuration.Seconds(),
		"latitude":              currentLocation.Latitude,
		"longitude":             currentLocation.Longitude,
		"accuracy":              currentLocation.Accuracy,
		"source":                currentLocation.Source,
	})

	// Check if we should trigger WiFi optimization
	shouldOptimize := false
	reason := ""

	// Check if we've never optimized
	if gh.lastOptimizedLocation == nil {
		shouldOptimize = true
		reason = "first_optimization"
	} else {
		// Check if we've moved significantly since last optimization
		distance := gh.calculateDistance(gh.lastOptimizedLocation, currentLocation)
		if distance > gh.config.WiFiMovementThreshold {
			shouldOptimize = true
			reason = "movement_threshold_exceeded"
		}
	}

	// Check optimization cooldown
	if shouldOptimize {
		timeSinceLastOptimization := time.Since(gh.lastOptimized)
		if timeSinceLastOptimization < gh.config.OptimizationCooldown {
			shouldOptimize = false
			reason = "cooldown_active"

			gh.logger.Debug("WiFi optimization on cooldown",
				"time_since_last", timeSinceLastOptimization,
				"cooldown_period", gh.config.OptimizationCooldown,
				"remaining", gh.config.OptimizationCooldown-timeSinceLastOptimization)
		}
	}

	if shouldOptimize {
		if err := gh.triggerOptimization(ctx, currentLocation, reason, stationaryDuration); err != nil {
			gh.logger.Error("Failed to trigger WiFi optimization from GPS hook", "error", err)
			return err
		}
	} else {
		gh.logger.Debug("WiFi optimization not triggered",
			"reason", reason,
			"stationary_duration", stationaryDuration)
	}

	return nil
}

// triggerOptimization triggers WiFi channel optimization
func (gh *GPSHook) triggerOptimization(ctx context.Context, currentLocation *pkg.GPSData, reason string, stationaryDuration time.Duration) error {
	gh.logger.Info("Triggering GPS-hooked WiFi optimization",
		"reason", reason,
		"latitude", currentLocation.Latitude,
		"longitude", currentLocation.Longitude,
		"stationary_duration", stationaryDuration,
		"gps_source", currentLocation.Source,
		"gps_accuracy", currentLocation.Accuracy)

	// Set location trigger
	gh.optimizer.SetLocationTrigger(true)

	// Trigger optimization
	err := gh.optimizer.OptimizeChannels(ctx, "gps_hook_"+reason)
	if err != nil {
		gh.logger.Error("GPS-hooked WiFi optimization failed", "error", err)
		return err
	}

	// Update state
	gh.lastOptimized = time.Now()
	gh.lastOptimizedLocation = currentLocation
	gh.optimizer.SetLocationTrigger(false) // Reset trigger after use

	// Log success with location context
	gh.logger.LogStateChange("wifi_optimizer", "idle", "optimized", "gps_hook_trigger", map[string]interface{}{
		"trigger_type":        "gps_hook",
		"trigger_reason":      reason,
		"latitude":            currentLocation.Latitude,
		"longitude":           currentLocation.Longitude,
		"stationary_duration": stationaryDuration.Seconds(),
		"gps_source":          currentLocation.Source,
		"gps_accuracy":        currentLocation.Accuracy,
		"optimization_time":   time.Now().UTC().Format(time.RFC3339),
	})

	return nil
}

// calculateDistance calculates the distance between two GPS coordinates using Haversine formula
func (gh *GPSHook) calculateDistance(from, to *pkg.GPSData) float64 {
	// Use the same calculation as the main GPS system for consistency
	return gh.optimizer.calculateDistance(from, to)
}

// OnLocationUpdate is called for every GPS update (optional, for fine-grained control)
func (gh *GPSHook) OnLocationUpdate(ctx context.Context, location *pkg.GPSData) error {
	if !gh.config.Enabled {
		return nil
	}

	// This could be used for additional logic if needed
	// For now, we rely on the movement and stationary callbacks
	return nil
}

// GetStatus returns GPS hook status
func (gh *GPSHook) GetStatus() map[string]interface{} {
	gh.mu.RLock()
	defer gh.mu.RUnlock()

	status := map[string]interface{}{
		"enabled":                 gh.config.Enabled,
		"wifi_movement_threshold": gh.config.WiFiMovementThreshold,
		"optimization_cooldown":   gh.config.OptimizationCooldown,
		"is_stationary":           gh.isStationary,
		"last_optimized":          gh.lastOptimized,
	}

	if gh.lastOptimizedLocation != nil {
		status["last_optimized_location"] = map[string]interface{}{
			"latitude":  gh.lastOptimizedLocation.Latitude,
			"longitude": gh.lastOptimizedLocation.Longitude,
			"accuracy":  gh.lastOptimizedLocation.Accuracy,
			"source":    gh.lastOptimizedLocation.Source,
		}
	}

	// Add cooldown information
	timeSinceLastOptimization := time.Since(gh.lastOptimized)
	status["time_since_last_optimization"] = timeSinceLastOptimization.Seconds()
	if timeSinceLastOptimization < gh.config.OptimizationCooldown {
		status["cooldown_remaining"] = (gh.config.OptimizationCooldown - timeSinceLastOptimization).Seconds()
	}

	return status
}

// ForceOptimization manually triggers WiFi optimization
func (gh *GPSHook) ForceOptimization(ctx context.Context) error {
	gh.mu.Lock()
	defer gh.mu.Unlock()

	gh.logger.Info("Forcing WiFi optimization via GPS hook")

	// Set location trigger temporarily
	gh.optimizer.SetLocationTrigger(true)

	// Trigger optimization
	err := gh.optimizer.OptimizeChannels(ctx, "manual_gps_hook")
	if err != nil {
		gh.logger.Error("Manual WiFi optimization via GPS hook failed", "error", err)
		return err
	}

	// Update state
	gh.lastOptimized = time.Now()
	gh.optimizer.SetLocationTrigger(false)

	gh.logger.Info("Manual WiFi optimization via GPS hook completed successfully")
	return nil
}
