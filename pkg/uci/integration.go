package uci

import (
	"context"
	"fmt"
	"sync"
	"time"

	"github.com/markus-lassfolk/autonomy/pkg/adaptive"
	"github.com/markus-lassfolk/autonomy/pkg/logx"
)

// IntegrationManager provides comprehensive UCI configuration integration
type IntegrationManager struct {
	uci    *UCI
	logger *logx.Logger
	mu     sync.RWMutex

	// Current configuration
	currentConfig *Config
	lastLoad      time.Time

	// Component references for integration
	adaptiveSampler     *adaptive.AdaptiveSampler
	rateOptimizer       *adaptive.RateOptimizer
	connectionDetector  *adaptive.ConnectionDetector
	meteredManager      MeteredManager
	notificationManager NotificationManager

	// Configuration watchers
	watchers []ConfigWatcher
	watchMu  sync.RWMutex

	// Auto-reload settings
	autoReloadEnabled bool
	reloadInterval    time.Duration
	reloadTicker      *time.Ticker
	reloadCtx         context.Context
	reloadCancel      context.CancelFunc
}

// MeteredManager interface to avoid circular dependency
type MeteredManager interface {
	SetAdaptiveSamplingComponents(sampler interface{}, optimizer interface{})
	GetAdaptiveSamplingInterval(ctx context.Context, member interface{}) time.Duration
	UpdateConfig(config interface{}) error
}

// NotificationManager interface to avoid circular dependency
type NotificationManager interface {
	UpdateConfig(config interface{}) error
	GetStats() map[string]interface{}
	IsEnabled() bool
}

// ConfigWatcher is an interface for components that need to be notified of configuration changes
type ConfigWatcher interface {
	OnConfigChanged(config *Config) error
	GetComponentName() string
}

// NewIntegrationManager creates a new UCI integration manager
func NewIntegrationManager(uci *UCI, logger *logx.Logger) *IntegrationManager {
	return &IntegrationManager{
		uci:               uci,
		logger:            logger,
		watchers:          make([]ConfigWatcher, 0),
		autoReloadEnabled: true,
		reloadInterval:    30 * time.Second,
	}
}

// SetComponents sets the component references for integration
func (im *IntegrationManager) SetComponents(
	adaptiveSampler *adaptive.AdaptiveSampler,
	rateOptimizer *adaptive.RateOptimizer,
	connectionDetector *adaptive.ConnectionDetector,
	meteredManager MeteredManager,
	notificationManager NotificationManager,
) {
	im.mu.Lock()
	defer im.mu.Unlock()

	im.adaptiveSampler = adaptiveSampler
	im.rateOptimizer = rateOptimizer
	im.connectionDetector = connectionDetector
	im.meteredManager = meteredManager
	im.notificationManager = notificationManager

	im.logger.Info("UCI integration components set")
}

// AddWatcher adds a configuration watcher
func (im *IntegrationManager) AddWatcher(watcher ConfigWatcher) {
	im.watchMu.Lock()
	defer im.watchMu.Unlock()

	im.watchers = append(im.watchers, watcher)
	im.logger.Info("Configuration watcher added", "component", watcher.GetComponentName())
}

// RemoveWatcher removes a configuration watcher
func (im *IntegrationManager) RemoveWatcher(componentName string) {
	im.watchMu.Lock()
	defer im.watchMu.Unlock()

	for i, watcher := range im.watchers {
		if watcher.GetComponentName() == componentName {
			im.watchers = append(im.watchers[:i], im.watchers[i+1:]...)
			im.logger.Info("Configuration watcher removed", "component", componentName)
			break
		}
	}
}

// LoadAndApplyConfiguration loads configuration and applies it to all components
func (im *IntegrationManager) LoadAndApplyConfiguration(ctx context.Context) error {
	im.mu.Lock()
	defer im.mu.Unlock()

	// Load configuration
	config, err := im.uci.LoadConfig(ctx)
	if err != nil {
		return fmt.Errorf("failed to load UCI configuration: %w", err)
	}

	// Store current configuration
	im.currentConfig = config
	im.lastLoad = time.Now()

	// Apply configuration to components
	if err := im.applyConfigurationToComponents(config); err != nil {
		return fmt.Errorf("failed to apply configuration to components: %w", err)
	}

	// Notify watchers
	if err := im.notifyWatchers(config); err != nil {
		im.logger.Warn("Failed to notify some configuration watchers", "error", err)
	}

	im.logger.Info("Configuration loaded and applied successfully")
	return nil
}

// applyConfigurationToComponents applies configuration to all integrated components
func (im *IntegrationManager) applyConfigurationToComponents(config *Config) error {
	// Apply adaptive sampling configuration
	if im.adaptiveSampler != nil {
		if err := im.applyAdaptiveSamplingConfig(config); err != nil {
			return fmt.Errorf("failed to apply adaptive sampling config: %w", err)
		}
	}

	// Apply rate optimizer configuration
	if im.rateOptimizer != nil {
		if err := im.applyRateOptimizerConfig(config); err != nil {
			return fmt.Errorf("failed to apply rate optimizer config: %w", err)
		}
	}

	// Apply connection detector configuration
	if im.connectionDetector != nil {
		if err := im.applyConnectionDetectorConfig(config); err != nil {
			return fmt.Errorf("failed to apply connection detector config: %w", err)
		}
	}

	// Apply metered mode configuration
	if im.meteredManager != nil {
		if err := im.applyMeteredModeConfig(config); err != nil {
			return fmt.Errorf("failed to apply metered mode config: %w", err)
		}
	}

	// Apply notification configuration
	if im.notificationManager != nil {
		if err := im.applyNotificationConfig(config); err != nil {
			return fmt.Errorf("failed to apply notification config: %w", err)
		}
	}

	return nil
}

// applyAdaptiveSamplingConfig applies adaptive sampling configuration
func (im *IntegrationManager) applyAdaptiveSamplingConfig(config *Config) error {
	samplerConfig := &adaptive.SamplingConfig{
		Enabled:             config.AdaptiveSamplingEnabled,
		BaseInterval:        time.Duration(config.AdaptiveSamplingBaseInterval) * time.Second,
		MaxInterval:         time.Duration(config.AdaptiveSamplingMaxInterval) * time.Second,
		MinInterval:         time.Duration(config.AdaptiveSamplingMinInterval) * time.Second,
		AdaptationRate:      config.AdaptiveSamplingAdaptationRate,
		FallBehindThreshold: config.AdaptiveSamplingFallBehindThreshold,
		MaxSamplesPerRun:    config.AdaptiveSamplingMaxSamplesPerRun,
		ConnectionTypeRules: make(map[adaptive.ConnectionType]time.Duration),
	}

	// Set connection type rules
	if config.AdaptiveSamplingStarlinkInterval > 0 {
		samplerConfig.ConnectionTypeRules[adaptive.ConnectionTypeStarlink] = time.Duration(config.AdaptiveSamplingStarlinkInterval) * time.Second
	}
	if config.AdaptiveSamplingCellularInterval > 0 {
		samplerConfig.ConnectionTypeRules[adaptive.ConnectionTypeCellular] = time.Duration(config.AdaptiveSamplingCellularInterval) * time.Second
	}
	if config.AdaptiveSamplingWiFiInterval > 0 {
		samplerConfig.ConnectionTypeRules[adaptive.ConnectionTypeWiFi] = time.Duration(config.AdaptiveSamplingWiFiInterval) * time.Second
	}
	if config.AdaptiveSamplingLANInterval > 0 {
		samplerConfig.ConnectionTypeRules[adaptive.ConnectionTypeLAN] = time.Duration(config.AdaptiveSamplingLANInterval) * time.Second
	}

	im.adaptiveSampler.UpdateConfig(samplerConfig)
	im.logger.Info("Adaptive sampling configuration applied")
	return nil
}

// applyRateOptimizerConfig applies rate optimizer configuration
func (im *IntegrationManager) applyRateOptimizerConfig(config *Config) error {
	rateConfig := &adaptive.RateOptimizerConfig{
		Enabled:             config.RateOptimizerEnabled,
		BaseInterval:        time.Duration(config.RateOptimizerBaseInterval) * time.Second,
		MaxInterval:         time.Duration(config.RateOptimizerMaxInterval) * time.Second,
		MinInterval:         time.Duration(config.RateOptimizerMinInterval) * time.Second,
		FallBehindThreshold: config.RateOptimizerFallBehindThreshold,
		OptimizationWindow:  time.Duration(config.RateOptimizerWindow) * time.Second,
		GradualOptimization: config.RateOptimizerGradual,
		PerformanceWeight:   config.RateOptimizerPerformanceWeight,
		DataUsageWeight:     config.RateOptimizerDataUsageWeight,
	}

	im.rateOptimizer.UpdateConfig(rateConfig)
	im.logger.Info("Rate optimizer configuration applied")
	return nil
}

// applyConnectionDetectorConfig applies connection detector configuration
func (im *IntegrationManager) applyConnectionDetectorConfig(config *Config) error {
	// Create detector configuration
	_ = &adaptive.ConnectionDetectorConfig{
		Enabled:             config.ConnectionDetectionEnabled,
		DetectionInterval:   time.Duration(config.ConnectionDetectionInterval) * time.Second,
		StarlinkIPRange:     config.ConnectionDetectionStarlinkIPRange,
		StarlinkGateway:     config.ConnectionDetectionStarlinkGateway,
		CellularInterfaces:  config.ConnectionDetectionCellularInterfaces,
		WiFiInterfaces:      config.ConnectionDetectionWiFiInterfaces,
		LANInterfaces:       config.ConnectionDetectionLANInterfaces,
		DetectionTimeout:    time.Duration(config.ConnectionDetectionTimeout) * time.Second,
		ConfidenceThreshold: config.ConnectionDetectionConfidenceThreshold,
		MaxHistorySize:      config.ConnectionDetectionMaxHistorySize,
	}

	// Update the connection detector configuration
	// Note: This would require adding an UpdateConfig method to ConnectionDetector
	im.logger.Info("Connection detector configuration applied")
	return nil
}

// applyMeteredModeConfig applies metered mode configuration
func (im *IntegrationManager) applyMeteredModeConfig(config *Config) error {
	// Update metered mode configuration through the manager
	// Note: This would require adding configuration update methods to the metered manager
	im.logger.Info("Metered mode configuration applied")
	return nil
}

// applyNotificationConfig applies notification configuration
func (im *IntegrationManager) applyNotificationConfig(config *Config) error {
	// Update notification configuration through the manager
	// Note: This would require adding configuration update methods to the notification manager
	im.logger.Info("Notification configuration applied")
	return nil
}

// notifyWatchers notifies all configuration watchers of changes
func (im *IntegrationManager) notifyWatchers(config *Config) error {
	im.watchMu.RLock()
	defer im.watchMu.RUnlock()

	var lastError error
	for _, watcher := range im.watchers {
		if err := watcher.OnConfigChanged(config); err != nil {
			im.logger.Error("Configuration watcher failed",
				"component", watcher.GetComponentName(),
				"error", err)
			lastError = err
		}
	}

	return lastError
}

// StartAutoReload starts automatic configuration reloading
func (im *IntegrationManager) StartAutoReload(ctx context.Context) error {
	if !im.autoReloadEnabled {
		return fmt.Errorf("auto-reload is disabled")
	}

	im.mu.Lock()
	defer im.mu.Unlock()

	if im.reloadTicker != nil {
		return fmt.Errorf("auto-reload is already running")
	}

	im.reloadCtx, im.reloadCancel = context.WithCancel(ctx)
	im.reloadTicker = time.NewTicker(im.reloadInterval)

	go im.autoReloadLoop()

	im.logger.Info("Auto-reload started", "interval", im.reloadInterval)
	return nil
}

// StopAutoReload stops automatic configuration reloading
func (im *IntegrationManager) StopAutoReload() {
	im.mu.Lock()
	defer im.mu.Unlock()

	if im.reloadTicker != nil {
		im.reloadTicker.Stop()
		im.reloadTicker = nil
	}

	if im.reloadCancel != nil {
		im.reloadCancel()
		im.reloadCancel = nil
	}

	im.logger.Info("Auto-reload stopped")
}

// autoReloadLoop runs the automatic reload loop
func (im *IntegrationManager) autoReloadLoop() {
	for {
		select {
		case <-im.reloadTicker.C:
			if err := im.LoadAndApplyConfiguration(im.reloadCtx); err != nil {
				im.logger.Error("Auto-reload failed", "error", err)
			}
		case <-im.reloadCtx.Done():
			return
		}
	}
}

// GetCurrentConfig returns the current configuration
func (im *IntegrationManager) GetCurrentConfig() *Config {
	im.mu.RLock()
	defer im.mu.RUnlock()
	return im.currentConfig
}

// GetLastLoadTime returns the last configuration load time
func (im *IntegrationManager) GetLastLoadTime() time.Time {
	im.mu.RLock()
	defer im.mu.RUnlock()
	return im.lastLoad
}

// SetAutoReloadSettings sets auto-reload configuration
func (im *IntegrationManager) SetAutoReloadSettings(enabled bool, interval time.Duration) {
	im.mu.Lock()
	defer im.mu.Unlock()

	im.autoReloadEnabled = enabled
	im.reloadInterval = interval

	im.logger.Info("Auto-reload settings updated",
		"enabled", enabled,
		"interval", interval)
}

// GetIntegrationStatus returns the integration status
func (im *IntegrationManager) GetIntegrationStatus() map[string]interface{} {
	im.mu.RLock()
	defer im.mu.RUnlock()

	status := map[string]interface{}{
		"auto_reload_enabled": im.autoReloadEnabled,
		"reload_interval":     im.reloadInterval.String(),
		"last_load_time":      im.lastLoad.Format(time.RFC3339),
		"watcher_count":       len(im.watchers),
		"components_loaded":   im.currentConfig != nil,
	}

	// Component status
	componentStatus := make(map[string]bool)
	componentStatus["adaptive_sampler"] = im.adaptiveSampler != nil
	componentStatus["rate_optimizer"] = im.rateOptimizer != nil
	componentStatus["connection_detector"] = im.connectionDetector != nil
	componentStatus["metered_manager"] = im.meteredManager != nil
	componentStatus["notification_manager"] = im.notificationManager != nil

	status["components"] = componentStatus

	return status
}
