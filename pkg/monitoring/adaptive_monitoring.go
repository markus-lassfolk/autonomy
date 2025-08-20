package monitoring

import (
	"os/exec"
	"strconv"
	"strings"
	"time"

	"github.com/markus-lassfolk/autonomy/pkg"
	"github.com/markus-lassfolk/autonomy/pkg/discovery"
	"github.com/markus-lassfolk/autonomy/pkg/logx"
)

// MonitoringMode defines different monitoring intensity levels
type MonitoringMode int

const (
	MonitoringActive    MonitoringMode = iota // Primary interface - full monitoring
	MonitoringStandby                         // Backup interface - reduced monitoring
	MonitoringEmergency                       // Data limit critical - minimal monitoring
	MonitoringDisabled                        // Data limit exceeded - no monitoring
)

// MonitoringConfig holds adaptive monitoring configuration
type MonitoringConfig struct {
	// Intervals for different modes
	ActiveInterval    time.Duration `json:"active_interval"`    // 5s - full monitoring
	StandbyInterval   time.Duration `json:"standby_interval"`   // 60s - reduced monitoring
	EmergencyInterval time.Duration `json:"emergency_interval"` // 300s - minimal monitoring

	// Data usage thresholds
	StandbyThreshold   float64 `json:"standby_threshold"`   // 50% - switch to standby
	EmergencyThreshold float64 `json:"emergency_threshold"` // 85% - switch to emergency
	DisabledThreshold  float64 `json:"disabled_threshold"`  // 95% - disable monitoring

	// Ping optimization
	OptimizedPingSize int  `json:"optimized_ping_size"` // 8 bytes instead of 64
	EnablePingOptim   bool `json:"enable_ping_optim"`   // Enable smaller pings

	// API call optimization
	ReducedAPIFreq    bool `json:"reduced_api_freq"`    // Reduce Starlink API frequency
	MinimalATCommands bool `json:"minimal_at_commands"` // Reduce cellular AT commands
}

// AdaptiveMonitoringManager manages monitoring intensity based on data limits
type AdaptiveMonitoringManager struct {
	logger *logx.Logger
	config *MonitoringConfig
}

// NewAdaptiveMonitoringManager creates a new adaptive monitoring manager
func NewAdaptiveMonitoringManager(logger *logx.Logger) *AdaptiveMonitoringManager {
	// Load configuration from UCI, with defaults as fallback
	config := loadAdaptiveMonitoringConfig(logger)

	return &AdaptiveMonitoringManager{
		logger: logger,
		config: config,
	}
}

// loadAdaptiveMonitoringConfig loads adaptive monitoring configuration from UCI
func loadAdaptiveMonitoringConfig(logger *logx.Logger) *MonitoringConfig {
	// Default configuration
	config := &MonitoringConfig{
		ActiveInterval:     5 * time.Second,
		StandbyInterval:    60 * time.Second,
		EmergencyInterval:  300 * time.Second,
		StandbyThreshold:   50.0,
		EmergencyThreshold: 85.0,
		DisabledThreshold:  95.0,
		OptimizedPingSize:  8,
		EnablePingOptim:    true,
		ReducedAPIFreq:     true,
		MinimalATCommands:  true,
	}

	// Try to load from UCI autonomy.adaptive_monitoring
	cmd := exec.Command("uci", "show", "autonomy.adaptive_monitoring")
	output, err := cmd.Output()
	if err != nil {
		logger.Debug("No adaptive monitoring UCI config found, using defaults", "error", err)
		return config
	}

	lines := strings.Split(string(output), "\n")
	for _, line := range lines {
		line = strings.TrimSpace(line)
		if line == "" {
			continue
		}

		parts := strings.Split(line, "=")
		if len(parts) != 2 {
			continue
		}

		key := strings.TrimPrefix(parts[0], "autonomy.adaptive_monitoring.")
		value := strings.Trim(parts[1], "'\"")

		switch key {
		case "active_interval":
			if interval, err := strconv.Atoi(value); err == nil {
				config.ActiveInterval = time.Duration(interval) * time.Second
			}
		case "standby_interval":
			if interval, err := strconv.Atoi(value); err == nil {
				config.StandbyInterval = time.Duration(interval) * time.Second
			}
		case "emergency_interval":
			if interval, err := strconv.Atoi(value); err == nil {
				config.EmergencyInterval = time.Duration(interval) * time.Second
			}
		case "standby_threshold":
			if threshold, err := strconv.ParseFloat(value, 64); err == nil {
				config.StandbyThreshold = threshold
			}
		case "emergency_threshold":
			if threshold, err := strconv.ParseFloat(value, 64); err == nil {
				config.EmergencyThreshold = threshold
			}
		case "disabled_threshold":
			if threshold, err := strconv.ParseFloat(value, 64); err == nil {
				config.DisabledThreshold = threshold
			}
		case "optimized_ping_size":
			if size, err := strconv.Atoi(value); err == nil {
				config.OptimizedPingSize = size
			}
		case "enable_ping_optim":
			config.EnablePingOptim = (value == "1")
		case "reduced_api_freq":
			config.ReducedAPIFreq = (value == "1")
		case "minimal_at_commands":
			config.MinimalATCommands = (value == "1")
		}
	}

	logger.Debug("Loaded adaptive monitoring config from UCI",
		"active_interval", config.ActiveInterval,
		"standby_interval", config.StandbyInterval,
		"emergency_interval", config.EmergencyInterval,
		"standby_threshold", config.StandbyThreshold,
		"emergency_threshold", config.EmergencyThreshold,
		"disabled_threshold", config.DisabledThreshold)

	return config
}

// GetMonitoringMode determines the appropriate monitoring mode for a member
func (amm *AdaptiveMonitoringManager) GetMonitoringMode(member *pkg.Member) MonitoringMode {
	// If no data limit, use active monitoring
	if member.DataLimitConfig == nil {
		return MonitoringActive
	}

	// Cast to DataLimitConfig
	dataLimit, ok := member.DataLimitConfig.(*discovery.DataLimitConfig)
	if !ok || dataLimit == nil {
		return MonitoringActive
	}

	usage := dataLimit.UsagePercentage

	// Check thresholds
	switch {
	case usage >= amm.config.DisabledThreshold:
		amm.logger.Info("Disabling monitoring due to data limit",
			"member", member.Name,
			"usage_percent", usage,
			"threshold", amm.config.DisabledThreshold)
		return MonitoringDisabled

	case usage >= amm.config.EmergencyThreshold:
		amm.logger.Info("Emergency monitoring mode due to data limit",
			"member", member.Name,
			"usage_percent", usage,
			"threshold", amm.config.EmergencyThreshold)
		return MonitoringEmergency

	case usage >= amm.config.StandbyThreshold && !member.IsPrimary:
		amm.logger.Debug("Standby monitoring mode due to data limit",
			"member", member.Name,
			"usage_percent", usage,
			"threshold", amm.config.StandbyThreshold)
		return MonitoringStandby

	default:
		// Primary interface or low usage - full monitoring
		return MonitoringActive
	}
}

// GetMonitoringInterval returns the monitoring interval for a given mode
func (amm *AdaptiveMonitoringManager) GetMonitoringInterval(mode MonitoringMode) time.Duration {
	switch mode {
	case MonitoringActive:
		return amm.config.ActiveInterval
	case MonitoringStandby:
		return amm.config.StandbyInterval
	case MonitoringEmergency:
		return amm.config.EmergencyInterval
	case MonitoringDisabled:
		return 0 // No monitoring
	default:
		return amm.config.ActiveInterval
	}
}

// GetPingConfig returns optimized ping configuration based on monitoring mode
func (amm *AdaptiveMonitoringManager) GetPingConfig(mode MonitoringMode, dataLimit *discovery.DataLimitConfig) PingConfig {
	config := PingConfig{
		Count:    1,
		Timeout:  5 * time.Second,
		Size:     64, // Default ping size
		Interval: 1 * time.Second,
	}

	// Optimize ping size for data-limited connections
	if dataLimit != nil && amm.config.EnablePingOptim {
		config.Size = amm.config.OptimizedPingSize // 8 bytes instead of 64
		amm.logger.Debug("Using optimized ping size",
			"size", config.Size,
			"data_savings", "87.5%")
	}

	// Adjust ping frequency based on monitoring mode
	switch mode {
	case MonitoringActive:
		config.Interval = 1 * time.Second
	case MonitoringStandby:
		config.Interval = 60 * time.Second
		amm.logger.Debug("Reduced ping frequency for standby mode",
			"interval", config.Interval,
			"data_savings", "98.3%")
	case MonitoringEmergency:
		config.Interval = 300 * time.Second
		amm.logger.Debug("Minimal ping frequency for emergency mode",
			"interval", config.Interval,
			"data_savings", "99.7%")
	case MonitoringDisabled:
		config.Count = 0 // No pings
	}

	return config
}

// GetAPICallConfig returns optimized API call configuration
func (amm *AdaptiveMonitoringManager) GetAPICallConfig(mode MonitoringMode, interfaceType pkg.InterfaceClass) APICallConfig {
	config := APICallConfig{
		EnableFullMetrics: true,
		EnableLocationAPI: true,
		EnableStatusAPI:   true,
		CallFrequency:     5 * time.Second,
	}

	switch mode {
	case MonitoringActive:
		// Full API calls
		config.EnableFullMetrics = true
		config.CallFrequency = 5 * time.Second

	case MonitoringStandby:
		// Reduced API frequency
		config.EnableFullMetrics = false
		config.EnableLocationAPI = false // Skip GPS calls
		config.CallFrequency = 60 * time.Second

	case MonitoringEmergency:
		// Minimal API calls
		config.EnableFullMetrics = false
		config.EnableLocationAPI = false
		config.CallFrequency = 300 * time.Second

	case MonitoringDisabled:
		// No API calls
		config.EnableFullMetrics = false
		config.EnableLocationAPI = false
		config.EnableStatusAPI = false
		config.CallFrequency = 0
	}

	return config
}

// CalculateDataUsageSavings estimates data usage savings from adaptive monitoring
func (amm *AdaptiveMonitoringManager) CalculateDataUsageSavings(mode MonitoringMode, interfaceType pkg.InterfaceClass) DataUsageSavings {
	savings := DataUsageSavings{
		PingSavingsPercent:  0,
		APISavingsPercent:   0,
		TotalSavingsPercent: 0,
	}

	switch mode {
	case MonitoringActive:
		// No savings - full monitoring
		return savings

	case MonitoringStandby:
		savings.PingSavingsPercent = 98.3 // 60s interval vs 1s
		if interfaceType == pkg.ClassStarlink {
			savings.APISavingsPercent = 92.0 // Reduced API calls
		} else if interfaceType == pkg.ClassCellular {
			savings.APISavingsPercent = 92.0 // Reduced AT commands
		}
		savings.TotalSavingsPercent = 92.0

	case MonitoringEmergency:
		savings.PingSavingsPercent = 99.7 // 300s interval vs 1s
		if interfaceType == pkg.ClassStarlink {
			savings.APISavingsPercent = 98.3 // Minimal API calls
		} else if interfaceType == pkg.ClassCellular {
			savings.APISavingsPercent = 98.3 // Minimal AT commands
		}
		savings.TotalSavingsPercent = 98.5

	case MonitoringDisabled:
		savings.PingSavingsPercent = 100.0
		savings.APISavingsPercent = 100.0
		savings.TotalSavingsPercent = 100.0
	}

	return savings
}

// EstimateMonthlyDataUsage calculates estimated monthly data usage for monitoring
func (amm *AdaptiveMonitoringManager) EstimateMonthlyDataUsage(mode MonitoringMode, interfaceType pkg.InterfaceClass) MonthlyDataUsage {
	usage := MonthlyDataUsage{}

	// Base usage (full monitoring)
	basePingMB := 315.0 // 315 MB/month for 1s ping interval
	baseAPIMB := 777.0  // 777 MB/month for cellular AT commands
	if interfaceType == pkg.ClassStarlink {
		baseAPIMB = 2070.0 // 2.07 GB/month for Starlink gRPC
	}

	savings := amm.CalculateDataUsageSavings(mode, interfaceType)

	// Calculate actual usage with savings
	usage.PingUsageMB = basePingMB * (100 - savings.PingSavingsPercent) / 100
	usage.APIUsageMB = baseAPIMB * (100 - savings.APISavingsPercent) / 100
	usage.TotalUsageMB = usage.PingUsageMB + usage.APIUsageMB
	usage.TotalUsageGB = usage.TotalUsageMB / 1024

	return usage
}

// Supporting types
type PingConfig struct {
	Count    int
	Timeout  time.Duration
	Size     int           // Ping packet size in bytes
	Interval time.Duration // Interval between pings
}

type APICallConfig struct {
	EnableFullMetrics bool
	EnableLocationAPI bool
	EnableStatusAPI   bool
	CallFrequency     time.Duration
}

type DataUsageSavings struct {
	PingSavingsPercent  float64
	APISavingsPercent   float64
	TotalSavingsPercent float64
}

type MonthlyDataUsage struct {
	PingUsageMB  float64
	APIUsageMB   float64
	TotalUsageMB float64
	TotalUsageGB float64
}

// String methods for logging
func (m MonitoringMode) String() string {
	switch m {
	case MonitoringActive:
		return "active"
	case MonitoringStandby:
		return "standby"
	case MonitoringEmergency:
		return "emergency"
	case MonitoringDisabled:
		return "disabled"
	default:
		return "unknown"
	}
}
