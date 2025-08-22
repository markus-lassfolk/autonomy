package main

import (
	"fmt"
	"os/exec"
	"strconv"
	"strings"
	"time"
)

// UCICellConfig manages UCI configuration for cell location services
type UCICellConfig struct {
	configSection string // UCI section name, e.g., "autonomy.cell_location"
}

// CellLocationConfig represents the UCI configuration structure
type CellLocationConfig struct {
	// OpenCellID Configuration
	OpenCellIDEnabled bool   `uci:"opencellid_enabled"`
	OpenCellIDToken   string `uci:"opencellid_token"`

	// Intelligent Caching Configuration
	MaxCacheAge          int     `uci:"max_cache_age_minutes"`  // Cache expiration in minutes
	DebounceDelay        int     `uci:"debounce_delay_seconds"` // Debounce delay in seconds
	TowerChangeThreshold float64 `uci:"tower_change_threshold"` // Percentage (0.0-1.0)
	TopTowersCount       int     `uci:"top_towers_count"`       // Number of top towers to monitor

	// Query Limits
	MaxDailyQueries      int `uci:"max_daily_queries"`      // Maximum queries per day
	QueryIntervalMinutes int `uci:"query_interval_minutes"` // Minimum interval between queries

	// Contribution Settings
	ContributionEnabled  bool `uci:"contribution_enabled"`        // Enable contributing data back
	ContributionInterval int  `uci:"contribution_interval_hours"` // Hours between contributions
	MinGPSAccuracy       int  `uci:"min_gps_accuracy_meters"`     // Minimum GPS accuracy for contribution

	// Fallback Settings
	EnableFallback   bool `uci:"enable_fallback"`   // Use cell location as GPS fallback
	FallbackPriority int  `uci:"fallback_priority"` // Priority in GPS source list (1-10)

	// Logging and Debug
	LogLevel           string `uci:"log_level"`            // debug, info, warn, error
	EnableDetailedLogs bool   `uci:"enable_detailed_logs"` // Log all queries and responses
}

// NewUCICellConfig creates a new UCI configuration manager
func NewUCICellConfig(section string) *UCICellConfig {
	return &UCICellConfig{
		configSection: section,
	}
}

// LoadConfig loads configuration from UCI
func (uci *UCICellConfig) LoadConfig() (*CellLocationConfig, error) {
	config := &CellLocationConfig{}

	// Load each configuration value
	var err error

	// OpenCellID Configuration
	config.OpenCellIDEnabled, err = uci.getBool("opencellid_enabled", true)
	if err != nil {
		return nil, fmt.Errorf("failed to load opencellid_enabled: %w", err)
	}

	config.OpenCellIDToken, err = uci.getString("opencellid_token", "")
	if err != nil {
		return nil, fmt.Errorf("failed to load opencellid_token: %w", err)
	}

	// Intelligent Caching Configuration
	config.MaxCacheAge, err = uci.getInt("max_cache_age_minutes", 60) // 1 hour default
	if err != nil {
		return nil, fmt.Errorf("failed to load max_cache_age_minutes: %w", err)
	}

	config.DebounceDelay, err = uci.getInt("debounce_delay_seconds", 10)
	if err != nil {
		return nil, fmt.Errorf("failed to load debounce_delay_seconds: %w", err)
	}

	config.TowerChangeThreshold, err = uci.getFloat("tower_change_threshold", 0.35) // 35% default
	if err != nil {
		return nil, fmt.Errorf("failed to load tower_change_threshold: %w", err)
	}

	config.TopTowersCount, err = uci.getInt("top_towers_count", 5)
	if err != nil {
		return nil, fmt.Errorf("failed to load top_towers_count: %w", err)
	}

	// Query Limits
	config.MaxDailyQueries, err = uci.getInt("max_daily_queries", 4800) // 96% of 5000 limit
	if err != nil {
		return nil, fmt.Errorf("failed to load max_daily_queries: %w", err)
	}

	config.QueryIntervalMinutes, err = uci.getInt("query_interval_minutes", 5)
	if err != nil {
		return nil, fmt.Errorf("failed to load query_interval_minutes: %w", err)
	}

	// Contribution Settings
	config.ContributionEnabled, err = uci.getBool("contribution_enabled", true)
	if err != nil {
		return nil, fmt.Errorf("failed to load contribution_enabled: %w", err)
	}

	config.ContributionInterval, err = uci.getInt("contribution_interval_hours", 24) // Daily
	if err != nil {
		return nil, fmt.Errorf("failed to load contribution_interval_hours: %w", err)
	}

	config.MinGPSAccuracy, err = uci.getInt("min_gps_accuracy_meters", 10)
	if err != nil {
		return nil, fmt.Errorf("failed to load min_gps_accuracy_meters: %w", err)
	}

	// Fallback Settings
	config.EnableFallback, err = uci.getBool("enable_fallback", true)
	if err != nil {
		return nil, fmt.Errorf("failed to load enable_fallback: %w", err)
	}

	config.FallbackPriority, err = uci.getInt("fallback_priority", 4) // 4th priority after GPS sources
	if err != nil {
		return nil, fmt.Errorf("failed to load fallback_priority: %w", err)
	}

	// Logging and Debug
	config.LogLevel, err = uci.getString("log_level", "info")
	if err != nil {
		return nil, fmt.Errorf("failed to load log_level: %w", err)
	}

	config.EnableDetailedLogs, err = uci.getBool("enable_detailed_logs", false)
	if err != nil {
		return nil, fmt.Errorf("failed to load enable_detailed_logs: %w", err)
	}

	return config, nil
}

// SaveConfig saves configuration to UCI
func (uci *UCICellConfig) SaveConfig(config *CellLocationConfig) error {
	// Save each configuration value
	var err error

	// OpenCellID Configuration
	err = uci.setBool("opencellid_enabled", config.OpenCellIDEnabled)
	if err != nil {
		return fmt.Errorf("failed to save opencellid_enabled: %w", err)
	}

	err = uci.setString("opencellid_token", config.OpenCellIDToken)
	if err != nil {
		return fmt.Errorf("failed to save opencellid_token: %w", err)
	}

	// Intelligent Caching Configuration
	err = uci.setInt("max_cache_age_minutes", config.MaxCacheAge)
	if err != nil {
		return fmt.Errorf("failed to save max_cache_age_minutes: %w", err)
	}

	err = uci.setInt("debounce_delay_seconds", config.DebounceDelay)
	if err != nil {
		return fmt.Errorf("failed to save debounce_delay_seconds: %w", err)
	}

	err = uci.setFloat("tower_change_threshold", config.TowerChangeThreshold)
	if err != nil {
		return fmt.Errorf("failed to save tower_change_threshold: %w", err)
	}

	err = uci.setInt("top_towers_count", config.TopTowersCount)
	if err != nil {
		return fmt.Errorf("failed to save top_towers_count: %w", err)
	}

	// Query Limits
	err = uci.setInt("max_daily_queries", config.MaxDailyQueries)
	if err != nil {
		return fmt.Errorf("failed to save max_daily_queries: %w", err)
	}

	err = uci.setInt("query_interval_minutes", config.QueryIntervalMinutes)
	if err != nil {
		return fmt.Errorf("failed to save query_interval_minutes: %w", err)
	}

	// Contribution Settings
	err = uci.setBool("contribution_enabled", config.ContributionEnabled)
	if err != nil {
		return fmt.Errorf("failed to save contribution_enabled: %w", err)
	}

	err = uci.setInt("contribution_interval_hours", config.ContributionInterval)
	if err != nil {
		return fmt.Errorf("failed to save contribution_interval_hours: %w", err)
	}

	err = uci.setInt("min_gps_accuracy_meters", config.MinGPSAccuracy)
	if err != nil {
		return fmt.Errorf("failed to save min_gps_accuracy_meters: %w", err)
	}

	// Fallback Settings
	err = uci.setBool("enable_fallback", config.EnableFallback)
	if err != nil {
		return fmt.Errorf("failed to save enable_fallback: %w", err)
	}

	err = uci.setInt("fallback_priority", config.FallbackPriority)
	if err != nil {
		return fmt.Errorf("failed to save fallback_priority: %w", err)
	}

	// Logging and Debug
	err = uci.setString("log_level", config.LogLevel)
	if err != nil {
		return fmt.Errorf("failed to save log_level: %w", err)
	}

	err = uci.setBool("enable_detailed_logs", config.EnableDetailedLogs)
	if err != nil {
		return fmt.Errorf("failed to save enable_detailed_logs: %w", err)
	}

	// Commit changes
	return uci.commit()
}

// Helper methods for UCI operations
func (uci *UCICellConfig) getString(key, defaultValue string) (string, error) {
	cmd := exec.Command("uci", "get", fmt.Sprintf("%s.%s", uci.configSection, key))
	output, err := cmd.Output()
	if err != nil {
		// If key doesn't exist, return default and set it
		if strings.Contains(err.Error(), "not found") {
			err = uci.setString(key, defaultValue)
			if err != nil {
				return defaultValue, err
			}
			return defaultValue, nil
		}
		return defaultValue, err
	}
	return strings.TrimSpace(string(output)), nil
}

func (uci *UCICellConfig) setString(key, value string) error {
	cmd := exec.Command("uci", "set", fmt.Sprintf("%s.%s=%s", uci.configSection, key, value))
	return cmd.Run()
}

func (uci *UCICellConfig) getInt(key string, defaultValue int) (int, error) {
	strValue, err := uci.getString(key, strconv.Itoa(defaultValue))
	if err != nil {
		return defaultValue, err
	}
	return strconv.Atoi(strValue)
}

func (uci *UCICellConfig) setInt(key string, value int) error {
	return uci.setString(key, strconv.Itoa(value))
}

func (uci *UCICellConfig) getFloat(key string, defaultValue float64) (float64, error) {
	strValue, err := uci.getString(key, fmt.Sprintf("%.3f", defaultValue))
	if err != nil {
		return defaultValue, err
	}
	return strconv.ParseFloat(strValue, 64)
}

func (uci *UCICellConfig) setFloat(key string, value float64) error {
	return uci.setString(key, fmt.Sprintf("%.3f", value))
}

func (uci *UCICellConfig) getBool(key string, defaultValue bool) (bool, error) {
	defaultStr := "0"
	if defaultValue {
		defaultStr = "1"
	}
	strValue, err := uci.getString(key, defaultStr)
	if err != nil {
		return defaultValue, err
	}
	return strValue == "1" || strings.ToLower(strValue) == "true", nil
}

func (uci *UCICellConfig) setBool(key string, value bool) error {
	strValue := "0"
	if value {
		strValue = "1"
	}
	return uci.setString(key, strValue)
}

func (uci *UCICellConfig) commit() error {
	cmd := exec.Command("uci", "commit", strings.Split(uci.configSection, ".")[0])
	return cmd.Run()
}

// ConvertToIntelligentCacheConfig converts UCI config to IntelligentCellCache config
func (config *CellLocationConfig) ConvertToIntelligentCacheConfig() *IntelligentCellCache {
	cache := NewIntelligentCellCache()

	// Convert UCI values to cache configuration
	cache.MaxCacheAge = time.Duration(config.MaxCacheAge) * time.Minute
	cache.DebounceDelay = time.Duration(config.DebounceDelay) * time.Second
	cache.TowerChangeThreshold = config.TowerChangeThreshold
	cache.TopTowersCount = config.TopTowersCount

	return cache
}

// PrintConfig displays the current configuration
func (config *CellLocationConfig) PrintConfig() {
	fmt.Println("📋 Cell Location Configuration")
	fmt.Println("=" + strings.Repeat("=", 30))

	fmt.Printf("OpenCellID Enabled: %v\n", config.OpenCellIDEnabled)
	if config.OpenCellIDToken != "" {
		fmt.Printf("OpenCellID Token: %s...%s\n",
			config.OpenCellIDToken[:8],
			config.OpenCellIDToken[len(config.OpenCellIDToken)-8:])
	} else {
		fmt.Println("OpenCellID Token: NOT SET")
	}

	fmt.Printf("Cache Max Age: %d minutes\n", config.MaxCacheAge)
	fmt.Printf("Debounce Delay: %d seconds\n", config.DebounceDelay)
	fmt.Printf("Tower Change Threshold: %.1f%%\n", config.TowerChangeThreshold*100)
	fmt.Printf("Top Towers Monitored: %d\n", config.TopTowersCount)

	fmt.Printf("Max Daily Queries: %d\n", config.MaxDailyQueries)
	fmt.Printf("Query Interval: %d minutes\n", config.QueryIntervalMinutes)

	fmt.Printf("Contribution Enabled: %v\n", config.ContributionEnabled)
	fmt.Printf("Contribution Interval: %d hours\n", config.ContributionInterval)
	fmt.Printf("Min GPS Accuracy: %d meters\n", config.MinGPSAccuracy)

	fmt.Printf("Enable Fallback: %v\n", config.EnableFallback)
	fmt.Printf("Fallback Priority: %d\n", config.FallbackPriority)

	fmt.Printf("Log Level: %s\n", config.LogLevel)
	fmt.Printf("Detailed Logs: %v\n", config.EnableDetailedLogs)
}

// ValidateConfig validates the configuration values
func (config *CellLocationConfig) ValidateConfig() error {
	if config.OpenCellIDEnabled && config.OpenCellIDToken == "" {
		return fmt.Errorf("OpenCellID is enabled but no token is configured")
	}

	if config.MaxCacheAge < 1 {
		return fmt.Errorf("max_cache_age_minutes must be at least 1")
	}

	if config.DebounceDelay < 1 {
		return fmt.Errorf("debounce_delay_seconds must be at least 1")
	}

	if config.TowerChangeThreshold < 0.1 || config.TowerChangeThreshold > 1.0 {
		return fmt.Errorf("tower_change_threshold must be between 0.1 and 1.0")
	}

	if config.TopTowersCount < 1 || config.TopTowersCount > 10 {
		return fmt.Errorf("top_towers_count must be between 1 and 10")
	}

	if config.MaxDailyQueries < 100 || config.MaxDailyQueries > 5000 {
		return fmt.Errorf("max_daily_queries must be between 100 and 5000")
	}

	if config.FallbackPriority < 1 || config.FallbackPriority > 10 {
		return fmt.Errorf("fallback_priority must be between 1 and 10")
	}

	validLogLevels := []string{"debug", "info", "warn", "error"}
	found := false
	for _, level := range validLogLevels {
		if config.LogLevel == level {
			found = true
			break
		}
	}
	if !found {
		return fmt.Errorf("log_level must be one of: %s", strings.Join(validLogLevels, ", "))
	}

	return nil
}

// testUCICellConfig demonstrates UCI configuration management
func testUCICellConfig() error {
	fmt.Println("⚙️  TESTING UCI CELL CONFIGURATION")
	fmt.Println("=" + strings.Repeat("=", 35))

	// Create UCI config manager
	uciConfig := NewUCICellConfig("autonomy.cell_location")

	// Load current configuration
	fmt.Println("📖 Loading configuration from UCI...")
	config, err := uciConfig.LoadConfig()
	if err != nil {
		return fmt.Errorf("failed to load UCI config: %w", err)
	}

	// Display current configuration
	config.PrintConfig()

	// Validate configuration
	fmt.Println("\n🔍 Validating configuration...")
	err = config.ValidateConfig()
	if err != nil {
		fmt.Printf("⚠️  Configuration validation failed: %v\n", err)

		// Set default values for invalid config
		fmt.Println("🔧 Setting default values...")
		config.OpenCellIDEnabled = true
		config.OpenCellIDToken = "your-token-here" // User needs to set this
		config.MaxCacheAge = 60
		config.DebounceDelay = 10
		config.TowerChangeThreshold = 0.35
		config.TopTowersCount = 5
		config.MaxDailyQueries = 4800
		config.QueryIntervalMinutes = 5
		config.ContributionEnabled = true
		config.ContributionInterval = 24
		config.MinGPSAccuracy = 10
		config.EnableFallback = true
		config.FallbackPriority = 4
		config.LogLevel = "info"
		config.EnableDetailedLogs = false

		// Save corrected configuration
		fmt.Println("💾 Saving corrected configuration...")
		err = uciConfig.SaveConfig(config)
		if err != nil {
			return fmt.Errorf("failed to save UCI config: %w", err)
		}

		fmt.Println("✅ Configuration saved successfully!")
	} else {
		fmt.Println("✅ Configuration is valid!")
	}

	// Convert to intelligent cache configuration
	fmt.Println("\n🧠 Converting to intelligent cache configuration...")
	cacheConfig := config.ConvertToIntelligentCacheConfig()

	fmt.Printf("Cache Max Age: %v\n", cacheConfig.MaxCacheAge)
	fmt.Printf("Debounce Delay: %v\n", cacheConfig.DebounceDelay)
	fmt.Printf("Tower Change Threshold: %.1f%%\n", cacheConfig.TowerChangeThreshold*100)
	fmt.Printf("Top Towers Count: %d\n", cacheConfig.TopTowersCount)

	fmt.Println("\n✅ UCI Cell Configuration Test Complete!")
	return nil
}
