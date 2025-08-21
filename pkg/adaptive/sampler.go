package adaptive

import (
	"context"
	"fmt"
	"sync"
	"time"

	"github.com/markus-lassfolk/autonomy/pkg/logx"
)

// SamplingMode represents different sampling modes
type SamplingMode string

const (
	SamplingModeUnlimited    SamplingMode = "unlimited"    // 1s intervals
	SamplingModeMetered      SamplingMode = "metered"      // 60s intervals
	SamplingModeConservative SamplingMode = "conservative" // 30s intervals
	SamplingModeAggressive   SamplingMode = "aggressive"   // 5s intervals
	SamplingModeBattery      SamplingMode = "battery"      // 120s intervals for battery optimization
)

// ConnectionType represents different connection types
type ConnectionType string

const (
	ConnectionTypeStarlink ConnectionType = "starlink"
	ConnectionTypeCellular ConnectionType = "cellular"
	ConnectionTypeWiFi     ConnectionType = "wifi"
	ConnectionTypeLAN      ConnectionType = "lan"
	ConnectionTypeUnknown  ConnectionType = "unknown"
)

// SamplingConfig holds adaptive sampling configuration
type SamplingConfig struct {
	Enabled              bool                             `json:"enabled"`
	BaseInterval         time.Duration                    `json:"base_interval"`   // Base sampling interval (1s)
	MaxInterval          time.Duration                    `json:"max_interval"`    // Maximum sampling interval (120s)
	MinInterval          time.Duration                    `json:"min_interval"`    // Minimum sampling interval (1s)
	AdaptationRate       float64                          `json:"adaptation_rate"` // How quickly to adapt (0.1-1.0)
	ConnectionTypeRules  map[ConnectionType]time.Duration `json:"connection_type_rules"`
	DataLimitThreshold   float64                          `json:"data_limit_threshold"`  // Data usage threshold for metered mode
	BatteryThreshold     float64                          `json:"battery_threshold"`     // Battery level threshold
	PerformanceThreshold float64                          `json:"performance_threshold"` // CPU/memory threshold
	FallBehindThreshold  int                              `json:"fall_behind_threshold"` // Max samples to fall behind
	MaxSamplesPerRun     int                              `json:"max_samples_per_run"`   // Max samples to process per run
}

// SamplingState represents the current sampling state
type SamplingState struct {
	CurrentInterval  time.Duration  `json:"current_interval"`
	CurrentMode      SamplingMode   `json:"current_mode"`
	ConnectionType   ConnectionType `json:"connection_type"`
	DataUsagePercent float64        `json:"data_usage_percent"`
	BatteryLevel     float64        `json:"battery_level"`
	PerformanceScore float64        `json:"performance_score"`
	LastAdaptation   time.Time      `json:"last_adaptation"`
	AdaptationCount  int            `json:"adaptation_count"`
	FallBehindCount  int            `json:"fall_behind_count"`
	ProcessedSamples int            `json:"processed_samples"`
	TotalSamples     int            `json:"total_samples"`
}

// AdaptiveSampler manages adaptive sampling rates
type AdaptiveSampler struct {
	config *SamplingConfig
	logger *logx.Logger
	mu     sync.RWMutex

	state  *SamplingState
	ticker *time.Ticker
	ctx    context.Context
	cancel context.CancelFunc

	// Callbacks
	onSample func(ctx context.Context) error
	onAdapt  func(oldInterval, newInterval time.Duration, reason string)
}

// NewAdaptiveSampler creates a new adaptive sampler
func NewAdaptiveSampler(config *SamplingConfig, logger *logx.Logger) *AdaptiveSampler {
	if config == nil {
		config = &SamplingConfig{
			Enabled:              true,
			BaseInterval:         1 * time.Second,
			MaxInterval:          120 * time.Second,
			MinInterval:          1 * time.Second,
			AdaptationRate:       0.2,
			ConnectionTypeRules:  make(map[ConnectionType]time.Duration),
			DataLimitThreshold:   80.0, // 80% data usage
			BatteryThreshold:     20.0, // 20% battery
			PerformanceThreshold: 80.0, // 80% CPU/memory usage
			FallBehindThreshold:  10,
			MaxSamplesPerRun:     5,
		}
	}

	// Set default connection type rules
	if len(config.ConnectionTypeRules) == 0 {
		config.ConnectionTypeRules = map[ConnectionType]time.Duration{
			ConnectionTypeStarlink: 1 * time.Second,
			ConnectionTypeCellular: 5 * time.Second,
			ConnectionTypeWiFi:     3 * time.Second,
			ConnectionTypeLAN:      2 * time.Second,
			ConnectionTypeUnknown:  10 * time.Second,
		}
	}

	ctx, cancel := context.WithCancel(context.Background())

	as := &AdaptiveSampler{
		config: config,
		logger: logger,
		state: &SamplingState{
			CurrentInterval: config.BaseInterval,
			CurrentMode:     SamplingModeUnlimited,
			ConnectionType:  ConnectionTypeUnknown,
			LastAdaptation:  time.Now(),
		},
		ctx:    ctx,
		cancel: cancel,
	}

	return as
}

// Start begins adaptive sampling
func (as *AdaptiveSampler) Start(onSample func(ctx context.Context) error) error {
	if !as.config.Enabled {
		return fmt.Errorf("adaptive sampling is disabled")
	}

	as.mu.Lock()
	defer as.mu.Unlock()

	if as.ticker != nil {
		return fmt.Errorf("adaptive sampler is already running")
	}

	as.onSample = onSample
	as.ticker = time.NewTicker(as.state.CurrentInterval)

	go as.samplingLoop()

	as.logger.Info("Adaptive sampler started",
		"interval", as.state.CurrentInterval,
		"mode", as.state.CurrentMode)

	return nil
}

// Stop stops adaptive sampling
func (as *AdaptiveSampler) Stop() {
	as.mu.Lock()
	defer as.mu.Unlock()

	if as.ticker != nil {
		as.ticker.Stop()
		as.ticker = nil
	}

	as.cancel()

	as.logger.Info("Adaptive sampler stopped")
}

// UpdateConnectionType updates the connection type and adapts sampling
func (as *AdaptiveSampler) UpdateConnectionType(connectionType ConnectionType) {
	as.mu.Lock()
	defer as.mu.Unlock()

	if as.state.ConnectionType != connectionType {
		oldType := as.state.ConnectionType
		as.state.ConnectionType = connectionType

		as.adaptToConnectionType()

		as.logger.Info("Connection type updated",
			"old_type", oldType,
			"new_type", connectionType,
			"new_interval", as.state.CurrentInterval)
	}
}

// UpdateDataUsage updates data usage and adapts sampling
func (as *AdaptiveSampler) UpdateDataUsage(usagePercent float64) {
	as.mu.Lock()
	defer as.mu.Unlock()

	as.state.DataUsagePercent = usagePercent

	// Adapt if data usage is high
	if usagePercent > as.config.DataLimitThreshold {
		as.adaptToDataLimit()
	}
}

// UpdateBatteryLevel updates battery level and adapts sampling
func (as *AdaptiveSampler) UpdateBatteryLevel(batteryLevel float64) {
	as.mu.Lock()
	defer as.mu.Unlock()

	as.state.BatteryLevel = batteryLevel

	// Adapt if battery is low
	if batteryLevel < as.config.BatteryThreshold {
		as.adaptToBatteryLevel()
	}
}

// UpdatePerformance updates performance metrics and adapts sampling
func (as *AdaptiveSampler) UpdatePerformance(performanceScore float64) {
	as.mu.Lock()
	defer as.mu.Unlock()

	as.state.PerformanceScore = performanceScore

	// Adapt if performance is poor
	if performanceScore > as.config.PerformanceThreshold {
		as.adaptToPerformance()
	}
}

// GetCurrentState returns the current sampling state
func (as *AdaptiveSampler) GetCurrentState() *SamplingState {
	as.mu.RLock()
	defer as.mu.RUnlock()

	// Return a copy to avoid race conditions
	state := *as.state
	return &state
}

// SetAdaptationCallback sets a callback for when adaptation occurs
func (as *AdaptiveSampler) SetAdaptationCallback(callback func(oldInterval, newInterval time.Duration, reason string)) {
	as.mu.Lock()
	defer as.mu.Unlock()
	as.onAdapt = callback
}

// samplingLoop runs the main sampling loop
func (as *AdaptiveSampler) samplingLoop() {
	for {
		select {
		case <-as.ctx.Done():
			return
		case <-as.ticker.C:
			as.processSample()
		}
	}
}

// processSample processes a single sample
func (as *AdaptiveSampler) processSample() {
	as.mu.Lock()
	defer as.mu.Unlock()

	as.state.TotalSamples++

	if as.onSample != nil {
		if err := as.onSample(as.ctx); err != nil {
			as.logger.Warn("Sample processing failed", "error", err)
			as.state.FallBehindCount++
		} else {
			as.state.ProcessedSamples++
			as.state.FallBehindCount = 0
		}
	}

	// Check if we need to adapt
	as.checkAdaptation()
}

// checkAdaptation checks if adaptation is needed
func (as *AdaptiveSampler) checkAdaptation() {
	// Check fall behind threshold
	if as.state.FallBehindCount > as.config.FallBehindThreshold {
		as.adaptInterval(as.state.CurrentInterval/2, "fall_behind")
		return
	}

	// Check data usage
	if as.state.DataUsagePercent > as.config.DataLimitThreshold {
		as.adaptToDataLimit()
		return
	}

	// Check battery level
	if as.state.BatteryLevel > 0 && as.state.BatteryLevel < as.config.BatteryThreshold {
		as.adaptToBatteryLevel()
		return
	}

	// Check performance
	if as.state.PerformanceScore > as.config.PerformanceThreshold {
		as.adaptToPerformance()
		return
	}

	// Gradual adaptation back to optimal
	if as.state.CurrentInterval > as.getOptimalInterval() {
		as.adaptInterval(as.state.CurrentInterval-time.Duration(float64(as.state.CurrentInterval)*as.config.AdaptationRate), "gradual_optimization")
	}
}

// adaptInterval changes the sampling interval
func (as *AdaptiveSampler) adaptInterval(newInterval time.Duration, reason string) {
	// Clamp to min/max bounds
	if newInterval < as.config.MinInterval {
		newInterval = as.config.MinInterval
	}
	if newInterval > as.config.MaxInterval {
		newInterval = as.config.MaxInterval
	}

	// Only adapt if there's a significant change
	if absDuration(newInterval-as.state.CurrentInterval) < time.Second {
		return
	}

	oldInterval := as.state.CurrentInterval
	as.state.CurrentInterval = newInterval
	as.state.LastAdaptation = time.Now()
	as.state.AdaptationCount++

	// Update ticker
	if as.ticker != nil {
		as.ticker.Reset(newInterval)
	}

	// Update mode
	as.state.CurrentMode = as.getModeForInterval(newInterval)

	// Call adaptation callback
	if as.onAdapt != nil {
		as.onAdapt(oldInterval, newInterval, reason)
	}

	as.logger.Info("Sampling interval adapted",
		"old_interval", oldInterval,
		"new_interval", newInterval,
		"reason", reason,
		"mode", as.state.CurrentMode)
}

// adaptToConnectionType adapts based on connection type
func (as *AdaptiveSampler) adaptToConnectionType() {
	optimalInterval := as.getOptimalInterval()
	as.adaptInterval(optimalInterval, "connection_type_change")
}

// adaptToDataLimit adapts for high data usage
func (as *AdaptiveSampler) adaptToDataLimit() {
	newInterval := as.state.CurrentInterval * 2
	if newInterval > 60*time.Second {
		newInterval = 60 * time.Second
	}
	as.adaptInterval(newInterval, "data_limit")
}

// adaptToBatteryLevel adapts for low battery
func (as *AdaptiveSampler) adaptToBatteryLevel() {
	newInterval := as.state.CurrentInterval * 3
	if newInterval > 120*time.Second {
		newInterval = 120 * time.Second
	}
	as.adaptInterval(newInterval, "battery_level")
}

// adaptToPerformance adapts for poor performance
func (as *AdaptiveSampler) adaptToPerformance() {
	newInterval := as.state.CurrentInterval * 2
	if newInterval > 30*time.Second {
		newInterval = 30 * time.Second
	}
	as.adaptInterval(newInterval, "performance")
}

// getOptimalInterval returns the optimal interval for current connection type
func (as *AdaptiveSampler) getOptimalInterval() time.Duration {
	if interval, exists := as.config.ConnectionTypeRules[as.state.ConnectionType]; exists {
		return interval
	}
	return as.config.BaseInterval
}

// getModeForInterval determines the sampling mode for a given interval
func (as *AdaptiveSampler) getModeForInterval(interval time.Duration) SamplingMode {
	switch {
	case interval <= 1*time.Second:
		return SamplingModeUnlimited
	case interval <= 5*time.Second:
		return SamplingModeAggressive
	case interval <= 30*time.Second:
		return SamplingModeConservative
	case interval <= 60*time.Second:
		return SamplingModeMetered
	default:
		return SamplingModeBattery
	}
}

// UpdateConfig updates the sampling configuration
func (as *AdaptiveSampler) UpdateConfig(config *SamplingConfig) {
	as.mu.Lock()
	defer as.mu.Unlock()

	if config == nil {
		return
	}

	as.config = config
	as.logger.Info("Adaptive sampling configuration updated")
}

// Helper function to get absolute duration
func absDuration(d time.Duration) time.Duration {
	if d < 0 {
		return -d
	}
	return d
}
