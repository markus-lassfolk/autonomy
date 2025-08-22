package main

import (
	"fmt"
	"os/exec"
	"strconv"
	"strings"
	"time"
)

// UCIUnwiredLabsConfig manages UCI configuration for UnwiredLabs LocationAPI
type UCIUnwiredLabsConfig struct {
	configSection string // UCI section name, e.g., "autonomy.unwiredlabs"
}

// UnwiredLabsConfig represents the UCI configuration structure for UnwiredLabs
type UnwiredLabsConfig struct {
	// API Configuration
	Enabled  bool   `uci:"enabled"`
	APIToken string `uci:"api_token"`
	Region   string `uci:"region"` // "eu1", "us1", "us2", "ap1"

	// Request Parameters
	MaxCells       int  `uci:"max_cells"`       // Maximum cells per request (1-7)
	MaxWiFiAPs     int  `uci:"max_wifi_aps"`    // Maximum WiFi APs per request (2-15)
	IncludeAddress bool `uci:"include_address"` // Include address in response

	// Credit Management
	MinCredits           int `uci:"min_credits"`            // Minimum credits before alerts
	CreditAlertThreshold int `uci:"credit_alert_threshold"` // Alert when credits below this

	// Fallback Options
	EnableLACFallback bool `uci:"enable_lac_fallback"` // Location Area Code fallback
	EnableSCFallback  bool `uci:"enable_sc_fallback"`  // Serving Cell fallback
	EnableIPFallback  bool `uci:"enable_ip_fallback"`  // IP-based fallback

	// Rate Limiting
	MaxRequestsPerHour  int `uci:"max_requests_per_hour"`    // Maximum requests per hour
	RequestIntervalSecs int `uci:"request_interval_seconds"` // Minimum interval between requests

	// GPS Integration
	EnableFallback    bool `uci:"enable_fallback"`    // Use as GPS fallback
	FallbackPriority  int  `uci:"fallback_priority"`  // Priority in GPS source list (1-10)
	AccuracyThreshold int  `uci:"accuracy_threshold"` // Minimum accuracy to accept (meters)

	// Caching and Intelligence
	EnableCaching       bool `uci:"enable_caching"`        // Enable intelligent caching
	CacheMaxAge         int  `uci:"cache_max_age_minutes"` // Cache expiration in minutes
	CellChangeThreshold int  `uci:"cell_change_threshold"` // Percentage change to trigger new request
	WiFiChangeThreshold int  `uci:"wifi_change_threshold"` // Percentage change to trigger new request

	// Logging and Debug
	LogLevel           string `uci:"log_level"`            // debug, info, warn, error
	EnableDetailedLogs bool   `uci:"enable_detailed_logs"` // Log all requests and responses
	LogCreditsUsage    bool   `uci:"log_credits_usage"`    // Log credit consumption

	// Monitoring and Alerts
	EnableBalanceAlerts bool   `uci:"enable_balance_alerts"`       // Enable balance monitoring
	AlertWebhookURL     string `uci:"alert_webhook_url"`           // Webhook for alerts
	MonitoringInterval  int    `uci:"monitoring_interval_minutes"` // Balance check interval
}

// NewUCIUnwiredLabsConfig creates a new UCI configuration manager for UnwiredLabs
func NewUCIUnwiredLabsConfig(section string) *UCIUnwiredLabsConfig {
	return &UCIUnwiredLabsConfig{
		configSection: section,
	}
}

// LoadConfig loads UnwiredLabs configuration from UCI
func (uci *UCIUnwiredLabsConfig) LoadConfig() (*UnwiredLabsConfig, error) {
	config := &UnwiredLabsConfig{}

	var err error

	// API Configuration
	config.Enabled, err = uci.getBool("enabled", true)
	if err != nil {
		return nil, fmt.Errorf("failed to load enabled: %w", err)
	}

	config.APIToken, err = uci.getString("api_token", "")
	if err != nil {
		return nil, fmt.Errorf("failed to load api_token: %w", err)
	}

	config.Region, err = uci.getString("region", "eu1")
	if err != nil {
		return nil, fmt.Errorf("failed to load region: %w", err)
	}

	// Request Parameters
	config.MaxCells, err = uci.getInt("max_cells", 7)
	if err != nil {
		return nil, fmt.Errorf("failed to load max_cells: %w", err)
	}

	config.MaxWiFiAPs, err = uci.getInt("max_wifi_aps", 15)
	if err != nil {
		return nil, fmt.Errorf("failed to load max_wifi_aps: %w", err)
	}

	config.IncludeAddress, err = uci.getBool("include_address", true)
	if err != nil {
		return nil, fmt.Errorf("failed to load include_address: %w", err)
	}

	// Credit Management
	config.MinCredits, err = uci.getInt("min_credits", 50)
	if err != nil {
		return nil, fmt.Errorf("failed to load min_credits: %w", err)
	}

	config.CreditAlertThreshold, err = uci.getInt("credit_alert_threshold", 100)
	if err != nil {
		return nil, fmt.Errorf("failed to load credit_alert_threshold: %w", err)
	}

	// Fallback Options
	config.EnableLACFallback, err = uci.getBool("enable_lac_fallback", true)
	if err != nil {
		return nil, fmt.Errorf("failed to load enable_lac_fallback: %w", err)
	}

	config.EnableSCFallback, err = uci.getBool("enable_sc_fallback", true)
	if err != nil {
		return nil, fmt.Errorf("failed to load enable_sc_fallback: %w", err)
	}

	config.EnableIPFallback, err = uci.getBool("enable_ip_fallback", false)
	if err != nil {
		return nil, fmt.Errorf("failed to load enable_ip_fallback: %w", err)
	}

	// Rate Limiting
	config.MaxRequestsPerHour, err = uci.getInt("max_requests_per_hour", 100)
	if err != nil {
		return nil, fmt.Errorf("failed to load max_requests_per_hour: %w", err)
	}

	config.RequestIntervalSecs, err = uci.getInt("request_interval_seconds", 30)
	if err != nil {
		return nil, fmt.Errorf("failed to load request_interval_seconds: %w", err)
	}

	// GPS Integration
	config.EnableFallback, err = uci.getBool("enable_fallback", true)
	if err != nil {
		return nil, fmt.Errorf("failed to load enable_fallback: %w", err)
	}

	config.FallbackPriority, err = uci.getInt("fallback_priority", 5)
	if err != nil {
		return nil, fmt.Errorf("failed to load fallback_priority: %w", err)
	}

	config.AccuracyThreshold, err = uci.getInt("accuracy_threshold", 1000)
	if err != nil {
		return nil, fmt.Errorf("failed to load accuracy_threshold: %w", err)
	}

	// Caching and Intelligence
	config.EnableCaching, err = uci.getBool("enable_caching", true)
	if err != nil {
		return nil, fmt.Errorf("failed to load enable_caching: %w", err)
	}

	config.CacheMaxAge, err = uci.getInt("cache_max_age_minutes", 30)
	if err != nil {
		return nil, fmt.Errorf("failed to load cache_max_age_minutes: %w", err)
	}

	config.CellChangeThreshold, err = uci.getInt("cell_change_threshold", 30)
	if err != nil {
		return nil, fmt.Errorf("failed to load cell_change_threshold: %w", err)
	}

	config.WiFiChangeThreshold, err = uci.getInt("wifi_change_threshold", 40)
	if err != nil {
		return nil, fmt.Errorf("failed to load wifi_change_threshold: %w", err)
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

	config.LogCreditsUsage, err = uci.getBool("log_credits_usage", true)
	if err != nil {
		return nil, fmt.Errorf("failed to load log_credits_usage: %w", err)
	}

	// Monitoring and Alerts
	config.EnableBalanceAlerts, err = uci.getBool("enable_balance_alerts", true)
	if err != nil {
		return nil, fmt.Errorf("failed to load enable_balance_alerts: %w", err)
	}

	config.AlertWebhookURL, err = uci.getString("alert_webhook_url", "")
	if err != nil {
		return nil, fmt.Errorf("failed to load alert_webhook_url: %w", err)
	}

	config.MonitoringInterval, err = uci.getInt("monitoring_interval_minutes", 60)
	if err != nil {
		return nil, fmt.Errorf("failed to load monitoring_interval_minutes: %w", err)
	}

	return config, nil
}

// SaveConfig saves UnwiredLabs configuration to UCI
func (uci *UCIUnwiredLabsConfig) SaveConfig(config *UnwiredLabsConfig) error {
	var err error

	// API Configuration
	err = uci.setBool("enabled", config.Enabled)
	if err != nil {
		return fmt.Errorf("failed to save enabled: %w", err)
	}

	err = uci.setString("api_token", config.APIToken)
	if err != nil {
		return fmt.Errorf("failed to save api_token: %w", err)
	}

	err = uci.setString("region", config.Region)
	if err != nil {
		return fmt.Errorf("failed to save region: %w", err)
	}

	// Request Parameters
	err = uci.setInt("max_cells", config.MaxCells)
	if err != nil {
		return fmt.Errorf("failed to save max_cells: %w", err)
	}

	err = uci.setInt("max_wifi_aps", config.MaxWiFiAPs)
	if err != nil {
		return fmt.Errorf("failed to save max_wifi_aps: %w", err)
	}

	err = uci.setBool("include_address", config.IncludeAddress)
	if err != nil {
		return fmt.Errorf("failed to save include_address: %w", err)
	}

	// Credit Management
	err = uci.setInt("min_credits", config.MinCredits)
	if err != nil {
		return fmt.Errorf("failed to save min_credits: %w", err)
	}

	err = uci.setInt("credit_alert_threshold", config.CreditAlertThreshold)
	if err != nil {
		return fmt.Errorf("failed to save credit_alert_threshold: %w", err)
	}

	// Fallback Options
	err = uci.setBool("enable_lac_fallback", config.EnableLACFallback)
	if err != nil {
		return fmt.Errorf("failed to save enable_lac_fallback: %w", err)
	}

	err = uci.setBool("enable_sc_fallback", config.EnableSCFallback)
	if err != nil {
		return fmt.Errorf("failed to save enable_sc_fallback: %w", err)
	}

	err = uci.setBool("enable_ip_fallback", config.EnableIPFallback)
	if err != nil {
		return fmt.Errorf("failed to save enable_ip_fallback: %w", err)
	}

	// Rate Limiting
	err = uci.setInt("max_requests_per_hour", config.MaxRequestsPerHour)
	if err != nil {
		return fmt.Errorf("failed to save max_requests_per_hour: %w", err)
	}

	err = uci.setInt("request_interval_seconds", config.RequestIntervalSecs)
	if err != nil {
		return fmt.Errorf("failed to save request_interval_seconds: %w", err)
	}

	// GPS Integration
	err = uci.setBool("enable_fallback", config.EnableFallback)
	if err != nil {
		return fmt.Errorf("failed to save enable_fallback: %w", err)
	}

	err = uci.setInt("fallback_priority", config.FallbackPriority)
	if err != nil {
		return fmt.Errorf("failed to save fallback_priority: %w", err)
	}

	err = uci.setInt("accuracy_threshold", config.AccuracyThreshold)
	if err != nil {
		return fmt.Errorf("failed to save accuracy_threshold: %w", err)
	}

	// Caching and Intelligence
	err = uci.setBool("enable_caching", config.EnableCaching)
	if err != nil {
		return fmt.Errorf("failed to save enable_caching: %w", err)
	}

	err = uci.setInt("cache_max_age_minutes", config.CacheMaxAge)
	if err != nil {
		return fmt.Errorf("failed to save cache_max_age_minutes: %w", err)
	}

	err = uci.setInt("cell_change_threshold", config.CellChangeThreshold)
	if err != nil {
		return fmt.Errorf("failed to save cell_change_threshold: %w", err)
	}

	err = uci.setInt("wifi_change_threshold", config.WiFiChangeThreshold)
	if err != nil {
		return fmt.Errorf("failed to save wifi_change_threshold: %w", err)
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

	err = uci.setBool("log_credits_usage", config.LogCreditsUsage)
	if err != nil {
		return fmt.Errorf("failed to save log_credits_usage: %w", err)
	}

	// Monitoring and Alerts
	err = uci.setBool("enable_balance_alerts", config.EnableBalanceAlerts)
	if err != nil {
		return fmt.Errorf("failed to save enable_balance_alerts: %w", err)
	}

	err = uci.setString("alert_webhook_url", config.AlertWebhookURL)
	if err != nil {
		return fmt.Errorf("failed to save alert_webhook_url: %w", err)
	}

	err = uci.setInt("monitoring_interval_minutes", config.MonitoringInterval)
	if err != nil {
		return fmt.Errorf("failed to save monitoring_interval_minutes: %w", err)
	}

	// Commit changes
	return uci.commit()
}

// Helper methods for UCI operations (reuse from previous implementation)
func (uci *UCIUnwiredLabsConfig) getString(key, defaultValue string) (string, error) {
	cmd := exec.Command("uci", "get", fmt.Sprintf("%s.%s", uci.configSection, key))
	output, err := cmd.Output()
	if err != nil {
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

func (uci *UCIUnwiredLabsConfig) setString(key, value string) error {
	cmd := exec.Command("uci", "set", fmt.Sprintf("%s.%s=%s", uci.configSection, key, value))
	return cmd.Run()
}

func (uci *UCIUnwiredLabsConfig) getInt(key string, defaultValue int) (int, error) {
	strValue, err := uci.getString(key, strconv.Itoa(defaultValue))
	if err != nil {
		return defaultValue, err
	}
	return strconv.Atoi(strValue)
}

func (uci *UCIUnwiredLabsConfig) setInt(key string, value int) error {
	return uci.setString(key, strconv.Itoa(value))
}

func (uci *UCIUnwiredLabsConfig) getBool(key string, defaultValue bool) (bool, error) {
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

func (uci *UCIUnwiredLabsConfig) setBool(key string, value bool) error {
	strValue := "0"
	if value {
		strValue = "1"
	}
	return uci.setString(key, strValue)
}

func (uci *UCIUnwiredLabsConfig) commit() error {
	cmd := exec.Command("uci", "commit", strings.Split(uci.configSection, ".")[0])
	return cmd.Run()
}

// BuildFallbacksFromConfig converts UCI config to API fallback array
func (config *UnwiredLabsConfig) BuildFallbacksFromConfig() []string {
	var fallbacks []string

	if config.EnableLACFallback {
		fallbacks = append(fallbacks, "lacf")
	}
	if config.EnableSCFallback {
		fallbacks = append(fallbacks, "scf")
	}
	if config.EnableIPFallback {
		fallbacks = append(fallbacks, "ipf")
	}

	return fallbacks
}

// ConvertToUnwiredLabsAPI converts UCI config to UnwiredLabsLocationAPI
func (config *UnwiredLabsConfig) ConvertToUnwiredLabsAPI() *UnwiredLabsLocationAPI {
	api := NewUnwiredLabsLocationAPI(config.APIToken, config.Region)
	api.timeout = time.Duration(config.RequestIntervalSecs) * time.Second
	return api
}

// PrintConfig displays the current UnwiredLabs configuration
func (config *UnwiredLabsConfig) PrintConfig() {
	fmt.Println("📋 UnwiredLabs LocationAPI Configuration")
	fmt.Println("=" + strings.Repeat("=", 40))

	fmt.Printf("🔧 API Configuration:\n")
	fmt.Printf("  Enabled: %v\n", config.Enabled)
	if config.APIToken != "" {
		fmt.Printf("  API Token: %s...%s\n",
			config.APIToken[:8],
			config.APIToken[len(config.APIToken)-8:])
	} else {
		fmt.Println("  API Token: NOT SET")
	}
	fmt.Printf("  Region: %s\n", config.Region)

	fmt.Printf("\n📡 Request Parameters:\n")
	fmt.Printf("  Max Cells: %d\n", config.MaxCells)
	fmt.Printf("  Max WiFi APs: %d\n", config.MaxWiFiAPs)
	fmt.Printf("  Include Address: %v\n", config.IncludeAddress)

	fmt.Printf("\n💰 Credit Management:\n")
	fmt.Printf("  Min Credits: %d\n", config.MinCredits)
	fmt.Printf("  Alert Threshold: %d\n", config.CreditAlertThreshold)

	fmt.Printf("\n🔄 Fallback Options:\n")
	fmt.Printf("  LAC Fallback: %v\n", config.EnableLACFallback)
	fmt.Printf("  SC Fallback: %v\n", config.EnableSCFallback)
	fmt.Printf("  IP Fallback: %v\n", config.EnableIPFallback)

	fmt.Printf("\n⏱️  Rate Limiting:\n")
	fmt.Printf("  Max Requests/Hour: %d\n", config.MaxRequestsPerHour)
	fmt.Printf("  Request Interval: %d seconds\n", config.RequestIntervalSecs)

	fmt.Printf("\n🎯 GPS Integration:\n")
	fmt.Printf("  Enable Fallback: %v\n", config.EnableFallback)
	fmt.Printf("  Fallback Priority: %d\n", config.FallbackPriority)
	fmt.Printf("  Accuracy Threshold: %d meters\n", config.AccuracyThreshold)

	fmt.Printf("\n🧠 Caching & Intelligence:\n")
	fmt.Printf("  Enable Caching: %v\n", config.EnableCaching)
	fmt.Printf("  Cache Max Age: %d minutes\n", config.CacheMaxAge)
	fmt.Printf("  Cell Change Threshold: %d%%\n", config.CellChangeThreshold)
	fmt.Printf("  WiFi Change Threshold: %d%%\n", config.WiFiChangeThreshold)

	fmt.Printf("\n📊 Logging & Debug:\n")
	fmt.Printf("  Log Level: %s\n", config.LogLevel)
	fmt.Printf("  Detailed Logs: %v\n", config.EnableDetailedLogs)
	fmt.Printf("  Log Credits Usage: %v\n", config.LogCreditsUsage)

	fmt.Printf("\n🔔 Monitoring & Alerts:\n")
	fmt.Printf("  Balance Alerts: %v\n", config.EnableBalanceAlerts)
	fmt.Printf("  Monitoring Interval: %d minutes\n", config.MonitoringInterval)
	if config.AlertWebhookURL != "" {
		fmt.Printf("  Webhook URL: %s\n", config.AlertWebhookURL)
	}
}

// ValidateConfig validates the UnwiredLabs configuration values
func (config *UnwiredLabsConfig) ValidateConfig() error {
	if config.Enabled && config.APIToken == "" {
		return fmt.Errorf("UnwiredLabs is enabled but no API token is configured")
	}

	validRegions := []string{"eu1", "us1", "us2", "ap1"}
	found := false
	for _, region := range validRegions {
		if config.Region == region {
			found = true
			break
		}
	}
	if !found {
		return fmt.Errorf("region must be one of: %s", strings.Join(validRegions, ", "))
	}

	if config.MaxCells < 1 || config.MaxCells > 7 {
		return fmt.Errorf("max_cells must be between 1 and 7")
	}

	if config.MaxWiFiAPs < 2 || config.MaxWiFiAPs > 15 {
		return fmt.Errorf("max_wifi_aps must be between 2 and 15")
	}

	if config.MinCredits < 0 {
		return fmt.Errorf("min_credits must be non-negative")
	}

	if config.FallbackPriority < 1 || config.FallbackPriority > 10 {
		return fmt.Errorf("fallback_priority must be between 1 and 10")
	}

	if config.AccuracyThreshold < 10 || config.AccuracyThreshold > 10000 {
		return fmt.Errorf("accuracy_threshold must be between 10 and 10000 meters")
	}

	validLogLevels := []string{"debug", "info", "warn", "error"}
	found = false
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

// testUCIUnwiredLabsConfig demonstrates UCI configuration management for UnwiredLabs
func testUCIUnwiredLabsConfig() error {
	fmt.Println("⚙️  TESTING UCI UNWIREDLABS CONFIGURATION")
	fmt.Println("=" + strings.Repeat("=", 42))

	// Create UCI config manager
	uciConfig := NewUCIUnwiredLabsConfig("autonomy.unwiredlabs")

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
		config.Enabled = true
		config.APIToken = "your-unwiredlabs-token-here"
		config.Region = "eu1"
		config.MaxCells = 7
		config.MaxWiFiAPs = 15
		config.IncludeAddress = true
		config.MinCredits = 50
		config.CreditAlertThreshold = 100
		config.EnableLACFallback = true
		config.EnableSCFallback = true
		config.EnableIPFallback = false
		config.MaxRequestsPerHour = 100
		config.RequestIntervalSecs = 30
		config.EnableFallback = true
		config.FallbackPriority = 5
		config.AccuracyThreshold = 1000
		config.EnableCaching = true
		config.CacheMaxAge = 30
		config.CellChangeThreshold = 30
		config.WiFiChangeThreshold = 40
		config.LogLevel = "info"
		config.EnableDetailedLogs = false
		config.LogCreditsUsage = true
		config.EnableBalanceAlerts = true
		config.MonitoringInterval = 60

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

	// Test API client creation
	fmt.Println("\n🔧 Testing API client creation...")
	api := config.ConvertToUnwiredLabsAPI()
	fmt.Printf("API Base URL: %s\n", api.baseURL)
	fmt.Printf("API Timeout: %v\n", api.timeout)

	// Test fallbacks configuration
	fmt.Println("\n🔄 Testing fallbacks configuration...")
	fallbacks := config.BuildFallbacksFromConfig()
	fmt.Printf("Enabled Fallbacks: %v\n", fallbacks)

	fmt.Println("\n✅ UCI UnwiredLabs Configuration Test Complete!")
	return nil
}
