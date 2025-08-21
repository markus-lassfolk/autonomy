package sysmgmt

import (
	"os"
	"strings"
	"time"
)

// Config represents the system management configuration
type Config struct {
	// General settings
	Enabled               bool          `json:"enabled"`
	CheckInterval         time.Duration `json:"check_interval"`
	MaxExecutionTime      time.Duration `json:"max_execution_time"`
	AutoFixEnabled        bool          `json:"auto_fix_enabled"`
	ServiceRestartEnabled bool          `json:"service_restart_enabled"`

	// Overlay space management
	OverlaySpaceEnabled      bool `json:"overlay_space_enabled"`
	OverlaySpaceThreshold    int  `json:"overlay_space_threshold"`    // Percentage
	OverlayCriticalThreshold int  `json:"overlay_critical_threshold"` // Percentage
	CleanupRetentionDays     int  `json:"cleanup_retention_days"`

	// Service watchdog
	ServiceWatchdogEnabled bool          `json:"service_watchdog_enabled"`
	ServiceTimeout         time.Duration `json:"service_timeout"`
	ServicesToMonitor      []string      `json:"services_to_monitor"`

	// Log flood detection
	LogFloodEnabled   bool     `json:"log_flood_enabled"`
	LogFloodThreshold int      `json:"log_flood_threshold"` // Entries per hour
	LogFloodPatterns  []string `json:"log_flood_patterns"`

	// Time drift correction
	TimeDriftEnabled   bool          `json:"time_drift_enabled"`
	TimeDriftThreshold time.Duration `json:"time_drift_threshold"`
	NTPTimeout         time.Duration `json:"ntp_timeout"`

	// Network interface stabilization
	InterfaceFlappingEnabled bool     `json:"interface_flapping_enabled"`
	FlappingThreshold        int      `json:"flapping_threshold"` // Events per hour
	FlappingInterfaces       []string `json:"flapping_interfaces"`

	// Starlink script health
	StarlinkScriptEnabled bool          `json:"starlink_script_enabled"`
	StarlinkLogTimeout    time.Duration `json:"starlink_log_timeout"`

	// Database management
	DatabaseEnabled        bool `json:"database_enabled"`
	DatabaseErrorThreshold int  `json:"database_error_threshold"`
	DatabaseMinSizeKB      int  `json:"database_min_size_kb"`
	DatabaseMaxAgeDays     int  `json:"database_max_age_days"`

	// Notifications
	NotificationsEnabled   bool          `json:"notifications_enabled"`
	NotifyOnFixes          bool          `json:"notify_on_fixes"`
	NotifyOnFailures       bool          `json:"notify_on_failures"`
	NotifyOnCritical       bool          `json:"notify_on_critical"`
	NotificationCooldown   time.Duration `json:"notification_cooldown"`
	MaxNotificationsPerRun int           `json:"max_notifications_per_run"`

	// Pushover settings
	PushoverEnabled          bool   `json:"pushover_enabled"`
	PushoverToken            string `json:"pushover_token"`
	PushoverUser             string `json:"pushover_user"`
	PushoverPriorityFixed    int    `json:"pushover_priority_fixed"`
	PushoverPriorityFailed   int    `json:"pushover_priority_failed"`
	PushoverPriorityCritical int    `json:"pushover_priority_critical"`

	// WiFi optimization settings
	WiFiOptimizationEnabled    bool    `json:"wifi_optimization_enabled"`
	WiFiMovementThreshold      float64 `json:"wifi_movement_threshold"`    // meters
	WiFiStationaryTime         int     `json:"wifi_stationary_time"`       // seconds
	WiFiOptimizationCooldown   int     `json:"wifi_optimization_cooldown"` // seconds
	WiFiNightlyOptimization    bool    `json:"wifi_nightly_optimization"`
	WiFiNightlyTime            string  `json:"wifi_nightly_time"`   // HH:MM format
	WiFiNightlyWindow          int     `json:"wifi_nightly_window"` // hours
	WiFiWeeklyOptimization     bool    `json:"wifi_weekly_optimization"`
	WiFiWeeklyDays             string  `json:"wifi_weekly_days"`   // comma-separated
	WiFiWeeklyTime             string  `json:"wifi_weekly_time"`   // HH:MM format
	WiFiWeeklyWindow           int     `json:"wifi_weekly_window"` // hours
	WiFiMinImprovement         int     `json:"wifi_min_improvement"`
	WiFiDwellTime              int     `json:"wifi_dwell_time"`    // seconds
	WiFiNoiseDefault           int     `json:"wifi_noise_default"` // dBm
	WiFiVHT80Threshold         int     `json:"wifi_vht80_threshold"`
	WiFiVHT40Threshold         int     `json:"wifi_vht40_threshold"`
	WiFiUseDFS                 bool    `json:"wifi_use_dfs"`
	WiFiGPSAccuracyThreshold   float64 `json:"wifi_gps_accuracy_threshold"` // meters
	WiFiLocationLogging        bool    `json:"wifi_location_logging"`
	WiFiSchedulerCheckInterval int     `json:"wifi_scheduler_check_interval"` // minutes
	WiFiSkipIfRecent           bool    `json:"wifi_skip_if_recent"`
	WiFiRecentThreshold        int     `json:"wifi_recent_threshold"` // hours
	WiFiTimezone               string  `json:"wifi_timezone"`

	// Enhanced WiFi Scanning Configuration
	WiFiUseEnhancedScanner  bool    `json:"wifi_use_enhanced_scanner"`  // Use RUTOS-native enhanced scanning
	WiFiStrongRSSIThreshold int     `json:"wifi_strong_rssi_threshold"` // Strong interferer threshold (-60dBm)
	WiFiWeakRSSIThreshold   int     `json:"wifi_weak_rssi_threshold"`   // Weak interferer threshold (-80dBm)
	WiFiUtilizationWeight   int     `json:"wifi_utilization_weight"`    // Channel utilization penalty weight
	WiFiExcellentThreshold  int     `json:"wifi_excellent_threshold"`   // 5-star threshold (90)
	WiFiGoodThreshold       int     `json:"wifi_good_threshold"`        // 4-star threshold (75)
	WiFiFairThreshold       int     `json:"wifi_fair_threshold"`        // 3-star threshold (50)
	WiFiPoorThreshold       int     `json:"wifi_poor_threshold"`        // 2-star threshold (25)
	WiFiOverlapPenaltyRatio float64 `json:"wifi_overlap_penalty_ratio"` // Overlap penalty ratio (0.5)

	// Ubus health monitoring
	UbusMonitorEnabled      bool          `json:"ubus_monitor_enabled"`
	UbusMonitorInterval     time.Duration `json:"ubus_monitor_interval"`
	UbusMaxFixAttempts      int           `json:"ubus_max_fix_attempts"`
	UbusAutoFix             bool          `json:"ubus_auto_fix"`
	UbusRestartTimeout      time.Duration `json:"ubus_restart_timeout"`
	UbusMinServicesExpected int           `json:"ubus_min_services_expected"`
	UbusCriticalServices    []string      `json:"ubus_critical_services"`
}

// DefaultConfig returns a default configuration
func DefaultConfig() *Config {
	return &Config{
		Enabled:               true,
		CheckInterval:         5 * time.Minute,
		MaxExecutionTime:      30 * time.Second,
		AutoFixEnabled:        true,
		ServiceRestartEnabled: true,

		OverlaySpaceThreshold:    80,
		OverlayCriticalThreshold: 90,
		CleanupRetentionDays:     7,

		ServiceWatchdogEnabled: true,
		ServiceTimeout:         30 * time.Minute, // Increased from 5 minutes to 30 minutes
		ServicesToMonitor:      []string{"nlbwmon", "mdcollectd", "connchecker", "network"},

		LogFloodEnabled:   true,
		LogFloodThreshold: 100,
		LogFloodPatterns:  []string{"STA-OPMODE-SMPS-MODE-CHANGED", "CTRL-EVENT-", "WPS-"},

		TimeDriftEnabled:   true,
		TimeDriftThreshold: 30 * time.Second,
		NTPTimeout:         10 * time.Second,

		InterfaceFlappingEnabled: true,
		FlappingThreshold:        5,
		FlappingInterfaces:       []string{"wan", "wwan", "mob"},

		StarlinkScriptEnabled: true,
		StarlinkLogTimeout:    10 * time.Minute,

		DatabaseEnabled:        true,
		DatabaseErrorThreshold: 5,
		DatabaseMinSizeKB:      1,
		DatabaseMaxAgeDays:     7,

		NotificationsEnabled:   true,
		NotifyOnFixes:          true,
		NotifyOnFailures:       true,
		NotifyOnCritical:       true,
		NotificationCooldown:   30 * time.Minute,
		MaxNotificationsPerRun: 10,

		PushoverEnabled:          false,
		PushoverToken:            "",
		PushoverUser:             "",
		PushoverPriorityFixed:    0,
		PushoverPriorityFailed:   1,
		PushoverPriorityCritical: 2,

		// WiFi optimization defaults
		WiFiOptimizationEnabled:    false, // Disabled by default
		WiFiMovementThreshold:      100.0, // 100 meters
		WiFiStationaryTime:         1800,  // 30 minutes
		WiFiOptimizationCooldown:   7200,  // 2 hours
		WiFiNightlyOptimization:    true,
		WiFiNightlyTime:            "03:00",
		WiFiNightlyWindow:          1, // 1 hour window
		WiFiWeeklyOptimization:     false,
		WiFiWeeklyDays:             "sunday",
		WiFiWeeklyTime:             "02:00",
		WiFiWeeklyWindow:           2, // 2 hour window
		WiFiMinImprovement:         15,
		WiFiDwellTime:              30, // 30 seconds
		WiFiNoiseDefault:           -95,
		WiFiVHT80Threshold:         80,
		WiFiVHT40Threshold:         60,
		WiFiUseDFS:                 true,
		WiFiGPSAccuracyThreshold:   50.0, // 50 meters
		WiFiLocationLogging:        true,
		WiFiSchedulerCheckInterval: 15, // 15 minutes
		WiFiSkipIfRecent:           true,
		WiFiRecentThreshold:        6, // 6 hours
		WiFiTimezone:               "UTC",

		// Enhanced WiFi Scanning defaults
		WiFiUseEnhancedScanner:  true, // Enable enhanced scanning by default
		WiFiStrongRSSIThreshold: -60,  // Strong interferer threshold (-60dBm)
		WiFiWeakRSSIThreshold:   -80,  // Weak interferer threshold (-80dBm)
		WiFiUtilizationWeight:   100,  // Full weight for utilization penalty
		WiFiExcellentThreshold:  90,   // 5 stars threshold (90 points)
		WiFiGoodThreshold:       75,   // 4 stars threshold (75 points)
		WiFiFairThreshold:       50,   // 3 stars threshold (50 points)
		WiFiPoorThreshold:       25,   // 2 stars threshold (25 points)
		WiFiOverlapPenaltyRatio: 0.5,  // Overlap penalty is 50% of co-channel

		// Ubus monitoring defaults
		UbusMonitorEnabled:      true,
		UbusMonitorInterval:     5 * time.Minute,
		UbusMaxFixAttempts:      3,
		UbusAutoFix:             true,
		UbusRestartTimeout:      30 * time.Second,
		UbusMinServicesExpected: 20,
		UbusCriticalServices:    []string{"system", "uci", "network", "service"},
	}
}

// LoadConfig loads configuration from UCI
func LoadConfig(configPath string) (*Config, error) {
	// Load configuration from UCI file or use defaults
	config := DefaultConfig()

	// Try to load from UCI file if it exists
	if configPath != "" {
		if data, err := os.ReadFile(configPath); err == nil {
			// Parse basic UCI options for system management
			lines := strings.Split(string(data), "\n")
			for _, line := range lines {
				line = strings.TrimSpace(line)
				if strings.HasPrefix(line, "option ") {
					parts := strings.Fields(line)
					if len(parts) >= 3 {
						key := parts[1]
						value := strings.Trim(parts[2], "'\"")

						// Parse relevant system management options
						switch key {
						case "sysmgmt_enabled":
							config.Enabled = value == "1"
						case "sysmgmt_check_interval":
							if duration, err := time.ParseDuration(value); err == nil {
								config.CheckInterval = duration
							}
						case "sysmgmt_auto_fix":
							config.AutoFixEnabled = value == "1"
						}
					}
				}
			}
		}
	}

	return config, nil
}
