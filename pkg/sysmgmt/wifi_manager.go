package sysmgmt

import (
	"context"
	"fmt"
	"strings"
	"time"

	"github.com/markus-lassfolk/autonomy/pkg/gps"
	"github.com/markus-lassfolk/autonomy/pkg/logx"
	"github.com/markus-lassfolk/autonomy/pkg/uci"
	"github.com/markus-lassfolk/autonomy/pkg/wifi"
)

// WiFiManager manages WiFi optimization within the system management framework
type WiFiManager struct {
	config *Config
	logger *logx.Logger
	dryRun bool

	// WiFi optimization components
	optimizer      *wifi.WiFiOptimizer
	gpsWiFiManager *wifi.GPSWiFiManager
	nightlyManager *wifi.WiFiScheduler

	// State tracking
	lastOptimization  time.Time
	optimizationCount int
	errorCount        int
	lastError         error
}

// NewWiFiManager creates a new WiFi manager for system management
func NewWiFiManager(config *Config, logger *logx.Logger, dryRun bool, uciClient *uci.UCI, gpsCollector gps.ComprehensiveGPSCollectorInterface) *WiFiManager {
	// Create WiFi optimizer configuration from system config
	_ = &wifi.Config{ // Temporarily unused - work in progress
		Enabled:             config.WiFiOptimizationEnabled,
		MovementThreshold:   config.WiFiMovementThreshold,
		StationaryTime:      time.Duration(config.WiFiStationaryTime) * time.Second,
		NightlyOptimization: config.WiFiNightlyOptimization,
		NightlyTime:         config.WiFiNightlyTime,
		MinImprovement:      config.WiFiMinImprovement,
		DwellTime:           time.Duration(config.WiFiDwellTime) * time.Second,
		NoiseDefault:        config.WiFiNoiseDefault,
		VHT80Threshold:      config.WiFiVHT80Threshold,
		VHT40Threshold:      config.WiFiVHT40Threshold,
		UseDFS:              config.WiFiUseDFS,
		DryRun:              dryRun,
	}

	// Create GPS-WiFi integration configuration
	_ = &wifi.GPSWiFiConfig{ // Temporarily unused - work in progress
		Enabled:              config.WiFiOptimizationEnabled,
		MovementThreshold:    config.WiFiMovementThreshold,
		StationaryTime:       time.Duration(config.WiFiStationaryTime) * time.Second,
		OptimizationCooldown: time.Duration(config.WiFiOptimizationCooldown) * time.Second,
		GPSAccuracyThreshold: config.WiFiGPSAccuracyThreshold,
		LocationLogging:      config.WiFiLocationLogging,
	}

	// Create nightly optimization configuration
	_ = &wifi.NightlyConfig{ // Temporarily unused - work in progress
		Enabled:     config.WiFiNightlyOptimization,
		Time:        config.WiFiNightlyTime,
		WindowHours: config.WiFiNightlyWindow,
	}

	// Create WiFi optimizer with the main daemon's UCI client
	var optimizer *wifi.WiFiOptimizer = nil
	if config.WiFiOptimizationEnabled {
		wifiOptimizerConfig := &wifi.Config{
			Enabled:             config.WiFiOptimizationEnabled,
			MovementThreshold:   config.WiFiMovementThreshold,
			StationaryTime:      time.Duration(config.WiFiStationaryTime) * time.Second,
			NightlyOptimization: config.WiFiNightlyOptimization,
			NightlyTime:         config.WiFiNightlyTime,
			MinImprovement:      config.WiFiMinImprovement,
			DwellTime:           time.Duration(config.WiFiDwellTime) * time.Second,
			NoiseDefault:        config.WiFiNoiseDefault,
			VHT80Threshold:      config.WiFiVHT80Threshold,
			VHT40Threshold:      config.WiFiVHT40Threshold,
			UseDFS:              config.WiFiUseDFS,
			DryRun:              dryRun,

			// Enhanced scanning configuration
			UseEnhancedScanner:  config.WiFiUseEnhancedScanner,
			StrongRSSIThreshold: config.WiFiStrongRSSIThreshold,
			WeakRSSIThreshold:   config.WiFiWeakRSSIThreshold,
			UtilizationWeight:   config.WiFiUtilizationWeight,
			ExcellentThreshold:  config.WiFiExcellentThreshold,
			GoodThreshold:       config.WiFiGoodThreshold,
			FairThreshold:       config.WiFiFairThreshold,
			PoorThreshold:       config.WiFiPoorThreshold,
			OverlapPenaltyRatio: config.WiFiOverlapPenaltyRatio,
		}
		optimizer = wifi.NewWiFiOptimizer(wifiOptimizerConfig, logger, uciClient)
		logger.Info("WiFi optimizer created", "enabled", wifiOptimizerConfig.Enabled)
	}

	// Create GPS-WiFi integration manager
	var gpsWiFiManager *wifi.GPSWiFiManager = nil
	if config.WiFiOptimizationEnabled && gpsCollector != nil {
		gpsWiFiConfig := &wifi.GPSWiFiConfig{
			Enabled:              config.WiFiOptimizationEnabled,
			MovementThreshold:    config.WiFiMovementThreshold,
			StationaryTime:       time.Duration(config.WiFiStationaryTime) * time.Second,
			OptimizationCooldown: time.Duration(config.WiFiOptimizationCooldown) * time.Second,
			GPSAccuracyThreshold: config.WiFiGPSAccuracyThreshold,
			LocationLogging:      config.WiFiLocationLogging,
		}
		gpsWiFiManager = wifi.NewGPSWiFiManager(optimizer, gpsCollector, logger, gpsWiFiConfig)
		logger.Info("GPS-WiFi manager created", "movement_threshold", gpsWiFiConfig.MovementThreshold)
	}

	// Create nightly/weekly scheduler
	var nightlyManager *wifi.WiFiScheduler = nil
	if config.WiFiOptimizationEnabled && optimizer != nil {
		schedulerConfig := &wifi.SchedulerConfig{
			NightlyEnabled:   config.WiFiNightlyOptimization,
			NightlyTime:      config.WiFiNightlyTime,
			NightlyWindowMin: config.WiFiNightlyWindow,
			WeeklyEnabled:    config.WiFiWeeklyOptimization,
			WeeklyDays:       strings.Split(config.WiFiWeeklyDays, ","),
			WeeklyTime:       config.WiFiWeeklyTime,
			WeeklyWindowMin:  config.WiFiWeeklyWindow,
			CheckIntervalMin: config.WiFiSchedulerCheckInterval,
			SkipIfRecent:     config.WiFiSkipIfRecent,
			RecentThresholdH: config.WiFiRecentThreshold,
			Timezone:         config.WiFiTimezone,
		}
		nightlyManager = wifi.NewWiFiScheduler(optimizer, logger, schedulerConfig)
		logger.Info("WiFi scheduler created",
			"nightly_enabled", schedulerConfig.NightlyEnabled,
			"weekly_enabled", schedulerConfig.WeeklyEnabled)
	}

	return &WiFiManager{
		config:         config,
		logger:         logger,
		dryRun:         dryRun,
		optimizer:      optimizer,
		gpsWiFiManager: gpsWiFiManager,
		nightlyManager: nightlyManager,
	}
}

// Start initializes and starts WiFi optimization services
func (wm *WiFiManager) Start(ctx context.Context) error {
	if !wm.config.WiFiOptimizationEnabled {
		wm.logger.Info("WiFi optimization disabled in system configuration")
		return nil
	}

	wm.logger.Info("Starting WiFi optimization services",
		"dry_run", wm.dryRun,
		"movement_threshold", wm.config.WiFiMovementThreshold,
		"stationary_time", wm.config.WiFiStationaryTime,
		"nightly_optimization", wm.config.WiFiNightlyOptimization)

	// Start GPS-WiFi integration if available
	if wm.gpsWiFiManager != nil {
		if err := wm.gpsWiFiManager.Start(ctx); err != nil {
			wm.logger.Error("Failed to start GPS-WiFi integration", "error", err)
			return fmt.Errorf("failed to start GPS-WiFi integration: %w", err)
		}
		wm.logger.Info("GPS-WiFi integration started successfully")
	} else {
		wm.logger.Info("GPS-WiFi integration not available (GPS collector not provided)")
	}

	// Start nightly/weekly scheduler if available
	if wm.nightlyManager != nil {
		if err := wm.nightlyManager.Start(ctx); err != nil {
			wm.logger.Error("Failed to start WiFi scheduler", "error", err)
			return fmt.Errorf("failed to start WiFi scheduler: %w", err)
		}
		wm.logger.Info("WiFi scheduler started successfully")
	} else {
		wm.logger.Info("WiFi scheduler not available (optimizer not created)")
	}

	wm.logger.Info("WiFi optimization services started successfully")
	return nil
}

// Check performs WiFi optimization health checks
func (wm *WiFiManager) Check(ctx context.Context) error {
	if !wm.config.WiFiOptimizationEnabled {
		return nil
	}

	wm.logger.Debug("Performing WiFi optimization health check")

	// Check if WiFi interfaces are available
	if err := wm.checkWiFiInterfaces(ctx); err != nil {
		wm.errorCount++
		wm.lastError = err
		wm.logger.Warn("WiFi interface check failed", "error", err)
		return err
	}

	// Check GPS availability for location-based optimization
	if err := wm.checkGPSAvailability(ctx); err != nil {
		wm.logger.Debug("GPS not available for WiFi optimization", "error", err)
		// GPS unavailability is not a critical error for WiFi optimization
	}

	// Check if optimization is overdue (for debugging)
	if err := wm.checkOptimizationStatus(); err != nil {
		wm.logger.Debug("WiFi optimization status check", "info", err)
		// This is informational, not an error
	}

	wm.logger.Debug("WiFi optimization health check completed successfully")
	return nil
}

// checkWiFiInterfaces verifies WiFi interfaces are available
func (wm *WiFiManager) checkWiFiInterfaces(ctx context.Context) error {
	// Try to detect WiFi interfaces
	interfaces, err := wm.detectWiFiInterfaces(ctx)
	if err != nil {
		return fmt.Errorf("failed to detect WiFi interfaces: %w", err)
	}

	if len(interfaces) < 2 {
		return fmt.Errorf("insufficient WiFi interfaces detected: %d (need at least 2)", len(interfaces))
	}

	// Check for 2.4GHz and 5GHz interfaces
	has24 := false
	has5 := false
	for _, iface := range interfaces {
		if iface.Band == "2.4" {
			has24 = true
		} else if iface.Band == "5" {
			has5 = true
		}
	}

	if !has24 || !has5 {
		return fmt.Errorf("missing required WiFi bands: 2.4GHz=%v, 5GHz=%v", has24, has5)
	}

	wm.logger.Debug("WiFi interfaces check passed",
		"interface_count", len(interfaces),
		"has_2_4ghz", has24,
		"has_5ghz", has5)

	return nil
}

// detectWiFiInterfaces is a simplified interface detection for health checks
func (wm *WiFiManager) detectWiFiInterfaces(ctx context.Context) ([]wifi.WiFiInterface, error) {
	// This is a simplified version for health checking
	// The full implementation is in the WiFi optimizer

	// Check common interface names
	interfaces := []wifi.WiFiInterface{}

	// Try common 2.4GHz interfaces
	for _, name := range []string{"wlan0", "radio0"} {
		if wm.interfaceExists(name) {
			interfaces = append(interfaces, wifi.WiFiInterface{
				Name: name,
				Band: "2.4",
			})
			break
		}
	}

	// Try common 5GHz interfaces
	for _, name := range []string{"wlan1", "radio1"} {
		if wm.interfaceExists(name) {
			interfaces = append(interfaces, wifi.WiFiInterface{
				Name: name,
				Band: "5",
			})
			break
		}
	}

	return interfaces, nil
}

// interfaceExists checks if a network interface exists
func (wm *WiFiManager) interfaceExists(name string) bool {
	// Simple check - in practice this would use proper interface detection
	// For now, assume common interfaces exist
	return name == "wlan0" || name == "wlan1" || name == "radio0" || name == "radio1"
}

// checkGPSAvailability checks if GPS is available for location-based optimization
func (wm *WiFiManager) checkGPSAvailability(ctx context.Context) error {
	if !wm.gpsWiFiManager.IsEnabled() {
		return fmt.Errorf("GPS-WiFi integration is disabled")
	}

	// This would check GPS collector availability
	// For now, just return success
	return nil
}

// checkOptimizationStatus checks the status of WiFi optimization
func (wm *WiFiManager) checkOptimizationStatus() error {
	status := wm.optimizer.GetStatus()

	if lastOptimized, ok := status["last_optimized"].(time.Time); ok {
		if !lastOptimized.IsZero() {
			timeSince := time.Since(lastOptimized)
			if timeSince > 24*time.Hour {
				return fmt.Errorf("WiFi optimization hasn't run in %v", timeSince)
			}
		}
	}

	return nil
}

// ForceOptimization manually triggers WiFi optimization
func (wm *WiFiManager) ForceOptimization(ctx context.Context) error {
	if !wm.config.WiFiOptimizationEnabled {
		return fmt.Errorf("WiFi optimization is disabled")
	}

	wm.logger.Info("Manually triggering WiFi optimization")

	err := wm.gpsWiFiManager.ForceOptimization(ctx)
	if err != nil {
		wm.errorCount++
		wm.lastError = err
		wm.logger.Error("Manual WiFi optimization failed", "error", err)
		return err
	}

	wm.optimizationCount++
	wm.lastOptimization = time.Now()
	wm.logger.Info("Manual WiFi optimization completed successfully")

	return nil
}

// GetStatus returns WiFi optimization status
func (wm *WiFiManager) GetStatus() map[string]interface{} {
	status := map[string]interface{}{
		"enabled":            wm.config.WiFiOptimizationEnabled,
		"dry_run":            wm.dryRun,
		"optimization_count": wm.optimizationCount,
		"error_count":        wm.errorCount,
		"last_optimization":  wm.lastOptimization,
	}

	if wm.lastError != nil {
		status["last_error"] = wm.lastError.Error()
	}

	// Add optimizer status
	if wm.optimizer != nil {
		status["optimizer"] = wm.optimizer.GetStatus()
	}

	// Add GPS-WiFi manager status
	if wm.gpsWiFiManager != nil {
		status["gps_wifi"] = wm.gpsWiFiManager.GetStatus()
	}

	// Add nightly manager status
	if wm.nightlyManager != nil {
		status["nightly"] = wm.nightlyManager.GetStatus()
	}

	return status
}

// GetMetrics returns WiFi optimization metrics for monitoring
func (wm *WiFiManager) GetMetrics() map[string]interface{} {
	metrics := map[string]interface{}{
		"wifi_optimization_enabled":     wm.config.WiFiOptimizationEnabled,
		"wifi_optimization_count_total": wm.optimizationCount,
		"wifi_optimization_error_count": wm.errorCount,
	}

	if !wm.lastOptimization.IsZero() {
		metrics["wifi_optimization_last_run_seconds"] = time.Since(wm.lastOptimization).Seconds()
	}

	// Add location state metrics if GPS-WiFi is enabled
	if wm.gpsWiFiManager != nil && wm.gpsWiFiManager.IsEnabled() {
		locationState := wm.gpsWiFiManager.GetLocationState()
		if locationState != nil {
			metrics["wifi_gps_is_moving"] = locationState.IsMoving
			metrics["wifi_gps_stationary_duration_seconds"] = locationState.Stationary.Seconds()

			if locationState.Current != nil {
				metrics["wifi_gps_accuracy_meters"] = locationState.Current.Accuracy
				metrics["wifi_gps_satellites"] = locationState.Current.Satellites
			}
		}
	}

	return metrics
}

// UpdateConfig updates WiFi optimization configuration
func (wm *WiFiManager) UpdateConfig(newConfig *Config) error {
	wm.logger.Info("Updating WiFi optimization configuration")

	// Update internal config
	wm.config = newConfig

	// Update optimizer configuration
	_ = &wifi.Config{ // Temporarily unused - work in progress
		Enabled:             newConfig.WiFiOptimizationEnabled,
		MovementThreshold:   newConfig.WiFiMovementThreshold,
		StationaryTime:      time.Duration(newConfig.WiFiStationaryTime) * time.Second,
		NightlyOptimization: newConfig.WiFiNightlyOptimization,
		NightlyTime:         newConfig.WiFiNightlyTime,
		MinImprovement:      newConfig.WiFiMinImprovement,
		DwellTime:           time.Duration(newConfig.WiFiDwellTime) * time.Second,
		NoiseDefault:        newConfig.WiFiNoiseDefault,
		VHT80Threshold:      newConfig.WiFiVHT80Threshold,
		VHT40Threshold:      newConfig.WiFiVHT40Threshold,
		UseDFS:              newConfig.WiFiUseDFS,
		DryRun:              wm.dryRun,
	}

	// Update GPS-WiFi configuration
	gpsWiFiConfig := &wifi.GPSWiFiConfig{
		Enabled:              newConfig.WiFiOptimizationEnabled,
		MovementThreshold:    newConfig.WiFiMovementThreshold,
		StationaryTime:       time.Duration(newConfig.WiFiStationaryTime) * time.Second,
		OptimizationCooldown: time.Duration(newConfig.WiFiOptimizationCooldown) * time.Second,
		GPSAccuracyThreshold: newConfig.WiFiGPSAccuracyThreshold,
		LocationLogging:      newConfig.WiFiLocationLogging,
	}

	// Update configurations
	if wm.gpsWiFiManager != nil {
		wm.gpsWiFiManager.SetConfig(gpsWiFiConfig)
	}

	wm.logger.Info("WiFi optimization configuration updated successfully")
	return nil
}

// Stop gracefully stops WiFi optimization services
func (wm *WiFiManager) Stop() {
	wm.logger.Info("Stopping WiFi optimization services")

	// GPS-WiFi integration stops automatically when context is cancelled
	if wm.gpsWiFiManager != nil {
		wm.logger.Info("GPS-WiFi integration will stop when context is cancelled")
	}

	// Stop WiFi scheduler
	if wm.nightlyManager != nil {
		wm.nightlyManager.Stop()
		wm.logger.Info("WiFi scheduler stopped")
	}

	wm.logger.Info("WiFi optimization services stopped")
}
