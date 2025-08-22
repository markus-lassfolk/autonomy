package main

import (
	"fmt"
	"os/exec"
	"strconv"
	"strings"
	"time"
)

// UCIGoogleConfig manages UCI configuration for Google Geolocation API
type UCIGoogleConfig struct {
	configSection string // UCI section name, e.g., "autonomy.google_geo"
}

// GoogleGeolocationConfig represents the UCI configuration structure for Google
type GoogleGeolocationConfig struct {
	// API Configuration
	Enabled bool   `uci:"enabled"`
	APIKey  string `uci:"api_key"`

	// Request Parameters
	MaxCells   int  `uci:"max_cells"`    // Maximum cells per request
	MaxWiFiAPs int  `uci:"max_wifi_aps"` // Maximum WiFi APs per request
	ConsiderIP bool `uci:"consider_ip"`  // Include IP-based location

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

	// Quality Control
	MinCellsRequired    int `uci:"min_cells_required"`    // Minimum cells needed for request
	MinWiFiAPsRequired  int `uci:"min_wifi_aps_required"` // Minimum WiFi APs needed for request
	MaxAccuracyAccepted int `uci:"max_accuracy_accepted"` // Maximum accuracy to accept (meters)

	// Logging and Debug
	LogLevel           string `uci:"log_level"`            // debug, info, warn, error
	EnableDetailedLogs bool   `uci:"enable_detailed_logs"` // Log all requests and responses
	LogAPIUsage        bool   `uci:"log_api_usage"`        // Log API usage and costs

	// Monitoring and Alerts
	EnableUsageAlerts  bool   `uci:"enable_usage_alerts"`         // Enable usage monitoring
	AlertWebhookURL    string `uci:"alert_webhook_url"`           // Webhook for alerts
	MonitoringInterval int    `uci:"monitoring_interval_minutes"` // Usage check interval

	// Cost Management
	EstimatedCostPerRequest float64 `uci:"estimated_cost_per_request"` // Estimated cost per API call
	MaxDailyCost            float64 `uci:"max_daily_cost"`             // Maximum daily cost limit
	CostAlertThreshold      float64 `uci:"cost_alert_threshold"`       // Alert when cost exceeds this
}

// NewUCIGoogleConfig creates a new UCI configuration manager for Google Geolocation
func NewUCIGoogleConfig(section string) *UCIGoogleConfig {
	return &UCIGoogleConfig{
		configSection: section,
	}
}

// LoadConfig loads Google Geolocation configuration from UCI
func (uci *UCIGoogleConfig) LoadConfig() (*GoogleGeolocationConfig, error) {
	config := &GoogleGeolocationConfig{}

	var err error

	// API Configuration
	config.Enabled, err = uci.getBool("enabled", true)
	if err != nil {
		return nil, fmt.Errorf("failed to load enabled: %w", err)
	}

	config.APIKey, err = uci.getString("api_key", "")
	if err != nil {
		return nil, fmt.Errorf("failed to load api_key: %w", err)
	}

	// Request Parameters
	config.MaxCells, err = uci.getInt("max_cells", 20)
	if err != nil {
		return nil, fmt.Errorf("failed to load max_cells: %w", err)
	}

	config.MaxWiFiAPs, err = uci.getInt("max_wifi_aps", 50)
	if err != nil {
		return nil, fmt.Errorf("failed to load max_wifi_aps: %w", err)
	}

	config.ConsiderIP, err = uci.getBool("consider_ip", true)
	if err != nil {
		return nil, fmt.Errorf("failed to load consider_ip: %w", err)
	}

	// Rate Limiting
	config.MaxRequestsPerHour, err = uci.getInt("max_requests_per_hour", 100)
	if err != nil {
		return nil, fmt.Errorf("failed to load max_requests_per_hour: %w", err)
	}

	config.RequestIntervalSecs, err = uci.getInt("request_interval_seconds", 60)
	if err != nil {
		return nil, fmt.Errorf("failed to load request_interval_seconds: %w", err)
	}

	// GPS Integration
	config.EnableFallback, err = uci.getBool("enable_fallback", true)
	if err != nil {
		return nil, fmt.Errorf("failed to load enable_fallback: %w", err)
	}

	config.FallbackPriority, err = uci.getInt("fallback_priority", 3)
	if err != nil {
		return nil, fmt.Errorf("failed to load fallback_priority: %w", err)
	}

	config.AccuracyThreshold, err = uci.getInt("accuracy_threshold", 500)
	if err != nil {
		return nil, fmt.Errorf("failed to load accuracy_threshold: %w", err)
	}

	// Caching and Intelligence
	config.EnableCaching, err = uci.getBool("enable_caching", true)
	if err != nil {
		return nil, fmt.Errorf("failed to load enable_caching: %w", err)
	}

	config.CacheMaxAge, err = uci.getInt("cache_max_age_minutes", 15)
	if err != nil {
		return nil, fmt.Errorf("failed to load cache_max_age_minutes: %w", err)
	}

	config.CellChangeThreshold, err = uci.getInt("cell_change_threshold", 25)
	if err != nil {
		return nil, fmt.Errorf("failed to load cell_change_threshold: %w", err)
	}

	config.WiFiChangeThreshold, err = uci.getInt("wifi_change_threshold", 30)
	if err != nil {
		return nil, fmt.Errorf("failed to load wifi_change_threshold: %w", err)
	}

	// Quality Control
	config.MinCellsRequired, err = uci.getInt("min_cells_required", 1)
	if err != nil {
		return nil, fmt.Errorf("failed to load min_cells_required: %w", err)
	}

	config.MinWiFiAPsRequired, err = uci.getInt("min_wifi_aps_required", 2)
	if err != nil {
		return nil, fmt.Errorf("failed to load min_wifi_aps_required: %w", err)
	}

	config.MaxAccuracyAccepted, err = uci.getInt("max_accuracy_accepted", 2000)
	if err != nil {
		return nil, fmt.Errorf("failed to load max_accuracy_accepted: %w", err)
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

	config.LogAPIUsage, err = uci.getBool("log_api_usage", true)
	if err != nil {
		return nil, fmt.Errorf("failed to load log_api_usage: %w", err)
	}

	// Monitoring and Alerts
	config.EnableUsageAlerts, err = uci.getBool("enable_usage_alerts", true)
	if err != nil {
		return nil, fmt.Errorf("failed to load enable_usage_alerts: %w", err)
	}

	config.AlertWebhookURL, err = uci.getString("alert_webhook_url", "")
	if err != nil {
		return nil, fmt.Errorf("failed to load alert_webhook_url: %w", err)
	}

	config.MonitoringInterval, err = uci.getInt("monitoring_interval_minutes", 60)
	if err != nil {
		return nil, fmt.Errorf("failed to load monitoring_interval_minutes: %w", err)
	}

	// Cost Management
	config.EstimatedCostPerRequest, err = uci.getFloat("estimated_cost_per_request", 0.005)
	if err != nil {
		return nil, fmt.Errorf("failed to load estimated_cost_per_request: %w", err)
	}

	config.MaxDailyCost, err = uci.getFloat("max_daily_cost", 1.0)
	if err != nil {
		return nil, fmt.Errorf("failed to load max_daily_cost: %w", err)
	}

	config.CostAlertThreshold, err = uci.getFloat("cost_alert_threshold", 0.5)
	if err != nil {
		return nil, fmt.Errorf("failed to load cost_alert_threshold: %w", err)
	}

	return config, nil
}

// SaveConfig saves Google Geolocation configuration to UCI
func (uci *UCIGoogleConfig) SaveConfig(config *GoogleGeolocationConfig) error {
	var err error

	// API Configuration
	err = uci.setBool("enabled", config.Enabled)
	if err != nil {
		return fmt.Errorf("failed to save enabled: %w", err)
	}

	err = uci.setString("api_key", config.APIKey)
	if err != nil {
		return fmt.Errorf("failed to save api_key: %w", err)
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

	err = uci.setBool("consider_ip", config.ConsiderIP)
	if err != nil {
		return fmt.Errorf("failed to save consider_ip: %w", err)
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

	// Quality Control
	err = uci.setInt("min_cells_required", config.MinCellsRequired)
	if err != nil {
		return fmt.Errorf("failed to save min_cells_required: %w", err)
	}

	err = uci.setInt("min_wifi_aps_required", config.MinWiFiAPsRequired)
	if err != nil {
		return fmt.Errorf("failed to save min_wifi_aps_required: %w", err)
	}

	err = uci.setInt("max_accuracy_accepted", config.MaxAccuracyAccepted)
	if err != nil {
		return fmt.Errorf("failed to save max_accuracy_accepted: %w", err)
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

	err = uci.setBool("log_api_usage", config.LogAPIUsage)
	if err != nil {
		return fmt.Errorf("failed to save log_api_usage: %w", err)
	}

	// Monitoring and Alerts
	err = uci.setBool("enable_usage_alerts", config.EnableUsageAlerts)
	if err != nil {
		return fmt.Errorf("failed to save enable_usage_alerts: %w", err)
	}

	err = uci.setString("alert_webhook_url", config.AlertWebhookURL)
	if err != nil {
		return fmt.Errorf("failed to save alert_webhook_url: %w", err)
	}

	err = uci.setInt("monitoring_interval_minutes", config.MonitoringInterval)
	if err != nil {
		return fmt.Errorf("failed to save monitoring_interval_minutes: %w", err)
	}

	// Cost Management
	err = uci.setFloat("estimated_cost_per_request", config.EstimatedCostPerRequest)
	if err != nil {
		return fmt.Errorf("failed to save estimated_cost_per_request: %w", err)
	}

	err = uci.setFloat("max_daily_cost", config.MaxDailyCost)
	if err != nil {
		return fmt.Errorf("failed to save max_daily_cost: %w", err)
	}

	err = uci.setFloat("cost_alert_threshold", config.CostAlertThreshold)
	if err != nil {
		return fmt.Errorf("failed to save cost_alert_threshold: %w", err)
	}

	// Commit changes
	return uci.commit()
}

// Helper methods for UCI operations (reuse from previous implementations)
func (uci *UCIGoogleConfig) getString(key, defaultValue string) (string, error) {
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

func (uci *UCIGoogleConfig) setString(key, value string) error {
	cmd := exec.Command("uci", "set", fmt.Sprintf("%s.%s=%s", uci.configSection, key, value))
	return cmd.Run()
}

func (uci *UCIGoogleConfig) getInt(key string, defaultValue int) (int, error) {
	strValue, err := uci.getString(key, strconv.Itoa(defaultValue))
	if err != nil {
		return defaultValue, err
	}
	return strconv.Atoi(strValue)
}

func (uci *UCIGoogleConfig) setInt(key string, value int) error {
	return uci.setString(key, strconv.Itoa(value))
}

func (uci *UCIGoogleConfig) getFloat(key string, defaultValue float64) (float64, error) {
	strValue, err := uci.getString(key, fmt.Sprintf("%.6f", defaultValue))
	if err != nil {
		return defaultValue, err
	}
	return strconv.ParseFloat(strValue, 64)
}

func (uci *UCIGoogleConfig) setFloat(key string, value float64) error {
	return uci.setString(key, fmt.Sprintf("%.6f", value))
}

func (uci *UCIGoogleConfig) getBool(key string, defaultValue bool) (bool, error) {
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

func (uci *UCIGoogleConfig) setBool(key string, value bool) error {
	strValue := "0"
	if value {
		strValue = "1"
	}
	return uci.setString(key, strValue)
}

func (uci *UCIGoogleConfig) commit() error {
	cmd := exec.Command("uci", "commit", strings.Split(uci.configSection, ".")[0])
	return cmd.Run()
}

// ConvertToGoogleService converts UCI config to GoogleGeolocationService
func (config *GoogleGeolocationConfig) ConvertToGoogleService() (*GoogleGeolocationService, error) {
	service, err := NewGoogleGeolocationService(config.APIKey)
	if err != nil {
		return nil, err
	}

	service.timeout = time.Duration(config.RequestIntervalSecs) * time.Second
	return service, nil
}

// PrintConfig displays the current Google Geolocation configuration
func (config *GoogleGeolocationConfig) PrintConfig() {
	fmt.Println("📋 Google Geolocation API Configuration")
	fmt.Println("=" + strings.Repeat("=", 40))

	fmt.Printf("🔧 API Configuration:\n")
	fmt.Printf("  Enabled: %v\n", config.Enabled)
	if config.APIKey != "" {
		fmt.Printf("  API Key: %s...%s\n",
			config.APIKey[:8],
			config.APIKey[len(config.APIKey)-8:])
	} else {
		fmt.Println("  API Key: NOT SET")
	}

	fmt.Printf("\n📡 Request Parameters:\n")
	fmt.Printf("  Max Cells: %d\n", config.MaxCells)
	fmt.Printf("  Max WiFi APs: %d\n", config.MaxWiFiAPs)
	fmt.Printf("  Consider IP: %v\n", config.ConsiderIP)

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

	fmt.Printf("\n🔍 Quality Control:\n")
	fmt.Printf("  Min Cells Required: %d\n", config.MinCellsRequired)
	fmt.Printf("  Min WiFi APs Required: %d\n", config.MinWiFiAPsRequired)
	fmt.Printf("  Max Accuracy Accepted: %d meters\n", config.MaxAccuracyAccepted)

	fmt.Printf("\n📊 Logging & Debug:\n")
	fmt.Printf("  Log Level: %s\n", config.LogLevel)
	fmt.Printf("  Detailed Logs: %v\n", config.EnableDetailedLogs)
	fmt.Printf("  Log API Usage: %v\n", config.LogAPIUsage)

	fmt.Printf("\n🔔 Monitoring & Alerts:\n")
	fmt.Printf("  Usage Alerts: %v\n", config.EnableUsageAlerts)
	fmt.Printf("  Monitoring Interval: %d minutes\n", config.MonitoringInterval)
	if config.AlertWebhookURL != "" {
		fmt.Printf("  Webhook URL: %s\n", config.AlertWebhookURL)
	}

	fmt.Printf("\n💰 Cost Management:\n")
	fmt.Printf("  Cost Per Request: $%.4f\n", config.EstimatedCostPerRequest)
	fmt.Printf("  Max Daily Cost: $%.2f\n", config.MaxDailyCost)
	fmt.Printf("  Cost Alert Threshold: $%.2f\n", config.CostAlertThreshold)
}

// ValidateConfig validates the Google Geolocation configuration values
func (config *GoogleGeolocationConfig) ValidateConfig() error {
	if config.Enabled && config.APIKey == "" {
		return fmt.Errorf("Google Geolocation is enabled but no API key is configured")
	}

	if config.MaxCells < 1 || config.MaxCells > 100 {
		return fmt.Errorf("max_cells must be between 1 and 100")
	}

	if config.MaxWiFiAPs < 1 || config.MaxWiFiAPs > 100 {
		return fmt.Errorf("max_wifi_aps must be between 1 and 100")
	}

	if config.FallbackPriority < 1 || config.FallbackPriority > 10 {
		return fmt.Errorf("fallback_priority must be between 1 and 10")
	}

	if config.AccuracyThreshold < 10 || config.AccuracyThreshold > 10000 {
		return fmt.Errorf("accuracy_threshold must be between 10 and 10000 meters")
	}

	if config.MinCellsRequired < 0 || config.MinCellsRequired > config.MaxCells {
		return fmt.Errorf("min_cells_required must be between 0 and max_cells")
	}

	if config.MinWiFiAPsRequired < 0 || config.MinWiFiAPsRequired > config.MaxWiFiAPs {
		return fmt.Errorf("min_wifi_aps_required must be between 0 and max_wifi_aps")
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

	if config.EstimatedCostPerRequest < 0 {
		return fmt.Errorf("estimated_cost_per_request must be non-negative")
	}

	if config.MaxDailyCost < 0 {
		return fmt.Errorf("max_daily_cost must be non-negative")
	}

	return nil
}

// testUCIGoogleConfig demonstrates UCI configuration management for Google Geolocation
func testUCIGoogleConfig() error {
	fmt.Println("⚙️  TESTING UCI GOOGLE GEOLOCATION CONFIGURATION")
	fmt.Println("=" + strings.Repeat("=", 48))

	// Create UCI config manager
	uciConfig := NewUCIGoogleConfig("autonomy.google_geo")

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
		config.APIKey = "your-google-api-key-here"
		config.MaxCells = 20
		config.MaxWiFiAPs = 50
		config.ConsiderIP = true
		config.MaxRequestsPerHour = 100
		config.RequestIntervalSecs = 60
		config.EnableFallback = true
		config.FallbackPriority = 3
		config.AccuracyThreshold = 500
		config.EnableCaching = true
		config.CacheMaxAge = 15
		config.CellChangeThreshold = 25
		config.WiFiChangeThreshold = 30
		config.MinCellsRequired = 1
		config.MinWiFiAPsRequired = 2
		config.MaxAccuracyAccepted = 2000
		config.LogLevel = "info"
		config.EnableDetailedLogs = false
		config.LogAPIUsage = true
		config.EnableUsageAlerts = true
		config.MonitoringInterval = 60
		config.EstimatedCostPerRequest = 0.005
		config.MaxDailyCost = 1.0
		config.CostAlertThreshold = 0.5

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

	// Test service creation
	fmt.Println("\n🔧 Testing service creation...")
	if config.APIKey != "your-google-api-key-here" && config.APIKey != "" {
		service, err := config.ConvertToGoogleService()
		if err != nil {
			fmt.Printf("⚠️  Service creation failed: %v\n", err)
		} else {
			fmt.Printf("✅ Google Geolocation service created successfully\n")
			fmt.Printf("Service timeout: %v\n", service.timeout)
		}
	} else {
		fmt.Println("⚠️  Skipping service creation - no valid API key")
	}

	fmt.Println("\n✅ UCI Google Geolocation Configuration Test Complete!")
	return nil
}
