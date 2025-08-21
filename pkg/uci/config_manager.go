package uci

import (
	"context"
	"fmt"
	"log"
)

// ConfigManager handles UCI configuration validation and repair
type ConfigManager struct {
	client *UCI
}

// NewConfigManager creates a new config manager
func NewConfigManager(client *UCI) *ConfigManager {
	return &ConfigManager{
		client: client,
	}
}

// EnsureRequiredConfig ensures all required configuration sections and options exist
func (cm *ConfigManager) EnsureRequiredConfig(ctx context.Context) error {
	log.Println("Ensuring required autonomy configuration...")

	// First, ensure the config file exists
	if err := cm.ensureConfigFileExists(); err != nil {
		return fmt.Errorf("failed to ensure config file exists: %w", err)
	}

	// Define required sections and their options
	requiredSections := map[string]map[string]string{
		"main": {
			"enable":                "1",
			"use_mwan3":             "1",
			"log_level":             "info",
			"log_file":              "",
			"poll_interval_ms":      "1500",
			"history_window_s":      "600",
			"min_uptime_s":          "20",
			"cooldown_s":            "20",
			"retention_hours":       "24",
			"max_ram_mb":            "16",
			"predictive":            "1",
			"switch_margin":         "10",
			"data_cap_mode":         "balanced",
			"metrics_listener":      "0",
			"health_listener":       "1",
			"performance_profiling": "0",
			"security_auditing":     "1",
			"profiling_enabled":     "0",
			"auditing_enabled":      "1",
			"max_failed_attempts":   "5",
			"block_duration":        "300",
		},
		"gps": {
			"enabled":                       "1",
			"source_priority":               "rutos,starlink,google",
			"movement_threshold_m":          "500",
			"accuracy_threshold_m":          "50",
			"staleness_threshold_s":         "300",
			"collection_interval_s":         "30",
			"movement_detection":            "1",
			"location_clustering":           "1",
			"retry_attempts":                "3",
			"retry_delay_s":                 "5",
			"google_api_enabled":            "1",
			"google_api_key":                "",  // Must be configured by user
			"google_elevation_api_enabled":  "1", // Enable Google Maps Elevation API when using Google Location API
			"hybrid_prioritization":         "1",
			"min_acceptable_confidence":     "0.5",
			"fallback_confidence_threshold": "0.7",
		},
		"starlink": {
			"host":       "192.168.100.1",
			"port":       "9200",
			"timeout_s":  "10",
			"grpc_first": "1",
			"http_first": "0",
		},
		"ml": {
			"enabled":    "1",
			"model_path": "/etc/autonomy/models.json",
			"training":   "1",
			"prediction": "1",
		},
		"monitoring": {
			"broker": "",
			"topic":  "autonomy/status",
		},
		"metered": {
			"enabled":            "0",
			"warning_threshold":  "80",
			"critical_threshold": "95",
			"hysteresis_margin":  "5",
			"stability_delay":    "300",
			"reconnect_method":   "gentle",
			"debug":              "0",
		},
		"wifi": {
			"enabled":                "0",
			"movement_threshold":     "500",
			"gps_accuracy_threshold": "50",
			"channel_optimization":   "1",
			"power_optimization":     "1",
			"interference_detection": "1",
		},
		"notifications": {
			"pushover_enabled":        "0",
			"pushover_token":          "",
			"pushover_user":           "",
			"pushover_device":         "",
			"threshold":               "warning",
			"acknowledgment_tracking": "1",
			"location_enabled":        "1",
			"rich_context_enabled":    "1",
			"cooldown_s":              "300",
			"max_per_hour":            "20",
			"failover":                "1",
			"failback":                "1",
			"member_down":             "1",
			"member_up":               "0",
			"predictive":              "1",
			"critical":                "1",
			"recovery":                "1",
			"failover_priority":       "1",
			"failback_priority":       "0",
			"member_down_priority":    "1",
			"member_up_priority":      "-1",
			"predictive_priority":     "0",
			"critical_priority":       "2",
			"recovery_priority":       "0",
		},
		"maintenance": {
			"enabled":                       "1",
			"check_interval":                "300",
			"max_execution_time":            "30",
			"auto_fix_enabled":              "1",
			"service_restart_enabled":       "1",
			"overlay_space_enabled":         "1",
			"overlay_space_threshold":       "80",
			"overlay_critical_threshold":    "90",
			"cleanup_retention_days":        "7",
			"service_watchdog_enabled":      "1",
			"service_timeout":               "300",
			"services_to_monitor":           "nlbwmon,mdcollectd,connchecker,hostapd,network",
			"log_flood_enabled":             "1",
			"log_flood_threshold":           "100",
			"log_flood_patterns":            "STA-OPMODE-SMPS-MODE-CHANGED,CTRL-EVENT-,WPS-",
			"time_drift_enabled":            "1",
			"time_drift_threshold":          "30",
			"ntp_timeout":                   "10",
			"interface_flapping_enabled":    "1",
			"flapping_threshold":            "5",
			"flapping_interfaces":           "wan,wwan,mob",
			"starlink_script_enabled":       "1",
			"starlink_log_timeout":          "600",
			"database_enabled":              "1",
			"database_cleanup_enabled":      "1",
			"database_optimization_enabled": "1",
		},
	}

	// Define required member configurations (commented out for now)
	/*
		requiredMembers := map[string]map[string]string{
			"starlink_any": {
				"detect":       "auto",
				"class":        "starlink",
				"weight":       "100",
				"min_uptime_s": "30",
				"cooldown_s":   "20",
			},
			"cellular_any": {
				"detect":         "auto",
				"class":          "cellular",
				"weight":         "80",
				"prefer_roaming": "0",
				"metered":        "1",
				"min_uptime_s":   "20",
				"cooldown_s":     "20",
			},
			"wifi_any": {
				"detect":       "auto",
				"class":        "wifi",
				"weight":       "60",
				"min_uptime_s": "15",
				"cooldown_s":   "15",
			},
			"lan_any": {
				"detect":       "auto",
				"class":        "lan",
				"weight":       "40",
				"min_uptime_s": "10",
				"cooldown_s":   "10",
			},
		}
	*/

	// Ensure main sections exist
	for sectionType, options := range requiredSections {
		if err := cm.ensureSection(ctx, sectionType, options); err != nil {
			return fmt.Errorf("failed to ensure section %s: %w", sectionType, err)
		}
	}

	// Ensure member sections exist (skip for now to avoid UCI issues)
	log.Println("Skipping member section creation to avoid UCI parsing issues")
	/*
		for memberName, options := range requiredMembers {
			if err := cm.ensureMemberSection(ctx, memberName, options); err != nil {
				return fmt.Errorf("failed to ensure member %s: %w", memberName, err)
			}
		}
	*/

	log.Println("✅ Required autonomy configuration ensured")
	return nil
}

// ensureSection ensures a section exists with all required options
func (cm *ConfigManager) ensureSection(ctx context.Context, sectionType string, options map[string]string) error {
	// Check if section exists
	_, err := cm.client.execUCI(ctx, "show", fmt.Sprintf("autonomy.@%s[0]", sectionType))
	if err != nil {
		// Section doesn't exist, create it
		log.Printf("Creating missing section: %s", sectionType)
		if err := cm.createSection(ctx, sectionType, options); err != nil {
			return err
		}
	} else {
		// Section exists, ensure all options are set
		log.Printf("Ensuring options for section: %s", sectionType)
		if err := cm.ensureOptions(ctx, sectionType, options); err != nil {
			return err
		}
	}
	return nil
}

// createSection creates a new section with options
func (cm *ConfigManager) createSection(ctx context.Context, sectionType string, options map[string]string) error {
	// Add the section
	if _, err := cm.client.execUCI(ctx, "add", "autonomy", sectionType); err != nil {
		return fmt.Errorf("failed to add section %s: %w", sectionType, err)
	}

	// Set all options
	for option, value := range options {
		if err := cm.setOption(ctx, sectionType, 0, option, value); err != nil {
			return fmt.Errorf("failed to set option %s.%s: %w", sectionType, option, err)
		}
	}

	return nil
}

// ensureOptions ensures all required options exist in a section
func (cm *ConfigManager) ensureOptions(ctx context.Context, sectionType string, options map[string]string) error {
	for option, defaultValue := range options {
		// Check if option exists
		_, err := cm.client.execUCI(ctx, "get", fmt.Sprintf("autonomy.@%s[0].%s", sectionType, option))
		if err != nil {
			// Option doesn't exist, set it
			log.Printf("Setting missing option: %s.%s = %s", sectionType, option, defaultValue)
			if err := cm.setOption(ctx, sectionType, 0, option, defaultValue); err != nil {
				return fmt.Errorf("failed to set option %s.%s: %w", sectionType, option, err)
			}
		}
	}
	return nil
}

// setOption sets a single option in a section
func (cm *ConfigManager) setOption(ctx context.Context, sectionType string, index int, option, value string) error {
	_, err := cm.client.execUCI(ctx, "set", fmt.Sprintf("autonomy.@%s[%d].%s=%s", sectionType, index, option, value))
	return err
}

// ensureConfigFileExists ensures the autonomy config file exists
func (cm *ConfigManager) ensureConfigFileExists() error {
	// Check if the config file exists
	_, err := cm.client.execUCI(context.Background(), "show", "autonomy")
	if err != nil {
		// File doesn't exist, create it
		log.Println("Creating autonomy config file...")
		_, err := cm.client.execUCI(context.Background(), "add", "autonomy", "main")
		if err != nil {
			return fmt.Errorf("failed to create initial config file: %w", err)
		}
		// Remove the temporary main section
		_, err = cm.client.execUCI(context.Background(), "delete", "autonomy.@main[0]")
		if err != nil {
			log.Printf("Warning: failed to remove temporary main section: %v", err)
		}
		log.Println("✅ autonomy config file created")
	}
	return nil
}

// Commit commits the configuration changes
func (cm *ConfigManager) Commit(ctx context.Context) error {
	_, err := cm.client.execUCI(ctx, "commit", "autonomy")
	if err != nil {
		return fmt.Errorf("failed to commit configuration: %w", err)
	}
	log.Println("✅ Configuration committed successfully")
	return nil
}
