package main

import (
	"fmt"
	"time"
)

// UCIAdaptiveConfig represents UCI configuration for adaptive location caching
type UCIAdaptiveConfig struct {
	// Trigger thresholds
	CellTopN             int     `uci:"autonomy.location.cell_top_n"`
	CellChangeThreshold  float64 `uci:"autonomy.location.cell_change_threshold"`
	CellTopStrongChanged int     `uci:"autonomy.location.cell_top_strong_changed"`

	WiFiTopK             int     `uci:"autonomy.location.wifi_top_k"`
	WiFiChangeThreshold  float64 `uci:"autonomy.location.wifi_change_threshold"`
	WiFiTopStrongChanged int     `uci:"autonomy.location.wifi_top_strong_changed"`

	// Timing controls (in seconds for UCI)
	DebounceTime          int `uci:"autonomy.location.debounce_time"`
	MinIntervalMoving     int `uci:"autonomy.location.min_interval_moving"`
	SoftTTL               int `uci:"autonomy.location.soft_ttl"`
	HardTTL               int `uci:"autonomy.location.hard_ttl"`
	StationaryBackoffTime int `uci:"autonomy.location.stationary_backoff_time"`

	// Stationary intervals (in seconds)
	StationaryInterval1 int `uci:"autonomy.location.stationary_interval_1"`
	StationaryInterval2 int `uci:"autonomy.location.stationary_interval_2"`
	StationaryInterval3 int `uci:"autonomy.location.stationary_interval_3"`
	StationaryInterval4 int `uci:"autonomy.location.stationary_interval_4"`

	// Quality gating
	AccuracyImprovement     float64 `uci:"autonomy.location.accuracy_improvement"`
	MinMovementDistance     float64 `uci:"autonomy.location.min_movement_distance"`
	MovementAccuracyFactor  float64 `uci:"autonomy.location.movement_accuracy_factor"`
	AccuracyRegressionLimit float64 `uci:"autonomy.location.accuracy_regression_limit"`
	ChiSquareThreshold      float64 `uci:"autonomy.location.chi_square_threshold"`

	// Budget management
	MonthlyQuota          int     `uci:"autonomy.location.monthly_quota"`
	DailyQuotaPercent     float64 `uci:"autonomy.location.daily_quota_percent"`
	QuotaExceededInterval int     `uci:"autonomy.location.quota_exceeded_interval"`

	// Smoothing
	BufferSize            int     `uci:"autonomy.location.buffer_size"`
	SmoothingWindowMoving int     `uci:"autonomy.location.smoothing_window_moving"`
	SmoothingWindowParked int     `uci:"autonomy.location.smoothing_window_parked"`
	EMAAlphaMin           float64 `uci:"autonomy.location.ema_alpha_min"`
	EMAAlphaMax           float64 `uci:"autonomy.location.ema_alpha_max"`
}

// GetDefaultAdaptiveConfig returns default configuration values
func GetDefaultAdaptiveConfig() *UCIAdaptiveConfig {
	return &UCIAdaptiveConfig{
		// Trigger thresholds
		CellTopN:             8,
		CellChangeThreshold:  0.35, // 35%
		CellTopStrongChanged: 2,

		WiFiTopK:             10,
		WiFiChangeThreshold:  0.40, // 40%
		WiFiTopStrongChanged: 3,

		// Timing controls (seconds)
		DebounceTime:          10,   // 10 seconds
		MinIntervalMoving:     300,  // 5 minutes
		SoftTTL:               900,  // 15 minutes
		HardTTL:               3600, // 60 minutes
		StationaryBackoffTime: 7200, // 2 hours

		// Stationary intervals (seconds)
		StationaryInterval1: 600,  // 10 minutes
		StationaryInterval2: 1200, // 20 minutes
		StationaryInterval3: 2400, // 40 minutes
		StationaryInterval4: 3600, // 60 minutes

		// Quality gating
		AccuracyImprovement:     0.8,   // Accept if 80% of old accuracy
		MinMovementDistance:     300.0, // 300 meters
		MovementAccuracyFactor:  1.5,   // 1.5x accuracy for movement detection
		AccuracyRegressionLimit: 1.2,   // Allow 20% accuracy loss on movement
		ChiSquareThreshold:      5.99,  // 95% confidence in 2D

		// Budget management
		MonthlyQuota:          10000, // 10k free requests
		DailyQuotaPercent:     0.5,   // 50% by midday
		QuotaExceededInterval: 900,   // 15 minutes

		// Smoothing
		BufferSize:            10,
		SmoothingWindowMoving: 5,
		SmoothingWindowParked: 10,
		EMAAlphaMin:           0.2,
		EMAAlphaMax:           0.5,
	}
}

// ToAdaptiveCacheConfig converts UCI config to internal config
func (uci *UCIAdaptiveConfig) ToAdaptiveCacheConfig() *AdaptiveCacheConfig {
	return &AdaptiveCacheConfig{
		// Trigger thresholds
		CellTopN:             uci.CellTopN,
		CellChangeThreshold:  uci.CellChangeThreshold,
		CellTopStrongChanged: uci.CellTopStrongChanged,

		WiFiTopK:             uci.WiFiTopK,
		WiFiChangeThreshold:  uci.WiFiChangeThreshold,
		WiFiTopStrongChanged: uci.WiFiTopStrongChanged,

		// Timing controls
		DebounceTime:          time.Duration(uci.DebounceTime) * time.Second,
		MinIntervalMoving:     time.Duration(uci.MinIntervalMoving) * time.Second,
		SoftTTL:               time.Duration(uci.SoftTTL) * time.Second,
		HardTTL:               time.Duration(uci.HardTTL) * time.Second,
		StationaryBackoffTime: time.Duration(uci.StationaryBackoffTime) * time.Second,

		// Stationary intervals
		StationaryIntervals: []time.Duration{
			time.Duration(uci.StationaryInterval1) * time.Second,
			time.Duration(uci.StationaryInterval2) * time.Second,
			time.Duration(uci.StationaryInterval3) * time.Second,
			time.Duration(uci.StationaryInterval4) * time.Second,
		},

		// Quality gating
		AccuracyImprovement:     uci.AccuracyImprovement,
		MinMovementDistance:     uci.MinMovementDistance,
		MovementAccuracyFactor:  uci.MovementAccuracyFactor,
		AccuracyRegressionLimit: uci.AccuracyRegressionLimit,
		ChiSquareThreshold:      uci.ChiSquareThreshold,

		// Budget management
		MonthlyQuota:          uci.MonthlyQuota,
		DailyQuotaPercent:     uci.DailyQuotaPercent,
		QuotaExceededInterval: time.Duration(uci.QuotaExceededInterval) * time.Second,

		// Smoothing
		BufferSize:            uci.BufferSize,
		SmoothingWindowMoving: uci.SmoothingWindowMoving,
		SmoothingWindowParked: uci.SmoothingWindowParked,
		EMAAlphaMin:           uci.EMAAlphaMin,
		EMAAlphaMax:           uci.EMAAlphaMax,
	}
}

// GenerateUCICommands generates UCI commands to set all configuration values
func (uci *UCIAdaptiveConfig) GenerateUCICommands() []string {
	commands := []string{
		"uci set autonomy.location=location",

		// Trigger thresholds
		fmt.Sprintf("uci set autonomy.location.cell_top_n='%d'", uci.CellTopN),
		fmt.Sprintf("uci set autonomy.location.cell_change_threshold='%.2f'", uci.CellChangeThreshold),
		fmt.Sprintf("uci set autonomy.location.cell_top_strong_changed='%d'", uci.CellTopStrongChanged),

		fmt.Sprintf("uci set autonomy.location.wifi_top_k='%d'", uci.WiFiTopK),
		fmt.Sprintf("uci set autonomy.location.wifi_change_threshold='%.2f'", uci.WiFiChangeThreshold),
		fmt.Sprintf("uci set autonomy.location.wifi_top_strong_changed='%d'", uci.WiFiTopStrongChanged),

		// Timing controls
		fmt.Sprintf("uci set autonomy.location.debounce_time='%d'", uci.DebounceTime),
		fmt.Sprintf("uci set autonomy.location.min_interval_moving='%d'", uci.MinIntervalMoving),
		fmt.Sprintf("uci set autonomy.location.soft_ttl='%d'", uci.SoftTTL),
		fmt.Sprintf("uci set autonomy.location.hard_ttl='%d'", uci.HardTTL),
		fmt.Sprintf("uci set autonomy.location.stationary_backoff_time='%d'", uci.StationaryBackoffTime),

		// Stationary intervals
		fmt.Sprintf("uci set autonomy.location.stationary_interval_1='%d'", uci.StationaryInterval1),
		fmt.Sprintf("uci set autonomy.location.stationary_interval_2='%d'", uci.StationaryInterval2),
		fmt.Sprintf("uci set autonomy.location.stationary_interval_3='%d'", uci.StationaryInterval3),
		fmt.Sprintf("uci set autonomy.location.stationary_interval_4='%d'", uci.StationaryInterval4),

		// Quality gating
		fmt.Sprintf("uci set autonomy.location.accuracy_improvement='%.2f'", uci.AccuracyImprovement),
		fmt.Sprintf("uci set autonomy.location.min_movement_distance='%.0f'", uci.MinMovementDistance),
		fmt.Sprintf("uci set autonomy.location.movement_accuracy_factor='%.1f'", uci.MovementAccuracyFactor),
		fmt.Sprintf("uci set autonomy.location.accuracy_regression_limit='%.1f'", uci.AccuracyRegressionLimit),
		fmt.Sprintf("uci set autonomy.location.chi_square_threshold='%.2f'", uci.ChiSquareThreshold),

		// Budget management
		fmt.Sprintf("uci set autonomy.location.monthly_quota='%d'", uci.MonthlyQuota),
		fmt.Sprintf("uci set autonomy.location.daily_quota_percent='%.2f'", uci.DailyQuotaPercent),
		fmt.Sprintf("uci set autonomy.location.quota_exceeded_interval='%d'", uci.QuotaExceededInterval),

		// Smoothing
		fmt.Sprintf("uci set autonomy.location.buffer_size='%d'", uci.BufferSize),
		fmt.Sprintf("uci set autonomy.location.smoothing_window_moving='%d'", uci.SmoothingWindowMoving),
		fmt.Sprintf("uci set autonomy.location.smoothing_window_parked='%d'", uci.SmoothingWindowParked),
		fmt.Sprintf("uci set autonomy.location.ema_alpha_min='%.1f'", uci.EMAAlphaMin),
		fmt.Sprintf("uci set autonomy.location.ema_alpha_max='%.1f'", uci.EMAAlphaMax),

		"uci commit autonomy",
	}

	return commands
}

// LoadFromUCI loads configuration from UCI (placeholder - would use actual UCI calls)
func LoadAdaptiveConfigFromUCI() (*AdaptiveCacheConfig, error) {
	// In a real implementation, this would execute UCI commands to read values
	// For now, return defaults
	uciConfig := GetDefaultAdaptiveConfig()
	return uciConfig.ToAdaptiveCacheConfig(), nil
}

// testAdaptiveUCIConfig demonstrates UCI configuration for adaptive caching
func testAdaptiveUCIConfig() {
	fmt.Println("⚙️  Adaptive Location Cache UCI Configuration")
	fmt.Println("============================================")

	// Get default configuration
	config := GetDefaultAdaptiveConfig()

	fmt.Println("📋 Default Configuration Values:")
	fmt.Println("================================")

	fmt.Printf("🔍 Trigger Thresholds:\n")
	fmt.Printf("  📱 Cell Top N: %d cells\n", config.CellTopN)
	fmt.Printf("  📱 Cell Change Threshold: %.0f%%\n", config.CellChangeThreshold*100)
	fmt.Printf("  📱 Cell Top Strong Changed: %d cells\n", config.CellTopStrongChanged)
	fmt.Printf("  📶 WiFi Top K: %d BSSIDs\n", config.WiFiTopK)
	fmt.Printf("  📶 WiFi Change Threshold: %.0f%%\n", config.WiFiChangeThreshold*100)
	fmt.Printf("  📶 WiFi Top Strong Changed: %d BSSIDs\n", config.WiFiTopStrongChanged)

	fmt.Printf("\n⏰ Timing Controls:\n")
	fmt.Printf("  🕐 Debounce Time: %d seconds\n", config.DebounceTime)
	fmt.Printf("  🚶 Min Interval Moving: %d seconds (%.0fm)\n", config.MinIntervalMoving, float64(config.MinIntervalMoving)/60)
	fmt.Printf("  🔄 Soft TTL: %d seconds (%.0fm)\n", config.SoftTTL, float64(config.SoftTTL)/60)
	fmt.Printf("  ⏰ Hard TTL: %d seconds (%.0fm)\n", config.HardTTL, float64(config.HardTTL)/60)
	fmt.Printf("  🏠 Stationary Backoff: %d seconds (%.0fh)\n", config.StationaryBackoffTime, float64(config.StationaryBackoffTime)/3600)

	fmt.Printf("\n🏠 Stationary Intervals:\n")
	fmt.Printf("  📅 Level 1: %d seconds (%.0fm)\n", config.StationaryInterval1, float64(config.StationaryInterval1)/60)
	fmt.Printf("  📅 Level 2: %d seconds (%.0fm)\n", config.StationaryInterval2, float64(config.StationaryInterval2)/60)
	fmt.Printf("  📅 Level 3: %d seconds (%.0fm)\n", config.StationaryInterval3, float64(config.StationaryInterval3)/60)
	fmt.Printf("  📅 Level 4: %d seconds (%.0fm)\n", config.StationaryInterval4, float64(config.StationaryInterval4)/60)

	fmt.Printf("\n🎯 Quality Gating:\n")
	fmt.Printf("  📊 Accuracy Improvement: %.0f%% threshold\n", config.AccuracyImprovement*100)
	fmt.Printf("  📏 Min Movement Distance: %.0fm\n", config.MinMovementDistance)
	fmt.Printf("  📐 Movement Accuracy Factor: %.1fx\n", config.MovementAccuracyFactor)
	fmt.Printf("  📈 Accuracy Regression Limit: %.0f%%\n", config.AccuracyRegressionLimit*100)
	fmt.Printf("  📊 Chi-Square Threshold: %.2f (95%% confidence)\n", config.ChiSquareThreshold)

	fmt.Printf("\n💰 Budget Management:\n")
	fmt.Printf("  📅 Monthly Quota: %d requests\n", config.MonthlyQuota)
	fmt.Printf("  📊 Daily Quota Percent: %.0f%% by midday\n", config.DailyQuotaPercent*100)
	fmt.Printf("  ⏰ Quota Exceeded Interval: %d seconds (%.0fm)\n", config.QuotaExceededInterval, float64(config.QuotaExceededInterval)/60)

	fmt.Printf("\n📊 Smoothing:\n")
	fmt.Printf("  📋 Buffer Size: %d fixes\n", config.BufferSize)
	fmt.Printf("  🚶 Smoothing Window Moving: %d fixes\n", config.SmoothingWindowMoving)
	fmt.Printf("  🏠 Smoothing Window Parked: %d fixes\n", config.SmoothingWindowParked)
	fmt.Printf("  📈 EMA Alpha Range: %.1f - %.1f\n", config.EMAAlphaMin, config.EMAAlphaMax)

	// Generate UCI commands
	fmt.Println("\n📝 Generated UCI Commands:")
	fmt.Println("==========================")
	commands := config.GenerateUCICommands()

	for i, cmd := range commands {
		if i < 5 || i >= len(commands)-2 {
			fmt.Printf("  %s\n", cmd)
		} else if i == 5 {
			fmt.Printf("  ... (%d more configuration commands) ...\n", len(commands)-7)
		}
	}

	fmt.Printf("\n💾 Total UCI Commands: %d\n", len(commands))
	fmt.Println("\n🎯 Usage in Production:")
	fmt.Println("  1. Copy commands to RutOS device")
	fmt.Println("  2. Execute to configure adaptive caching")
	fmt.Println("  3. Restart autonomy daemon to apply changes")
	fmt.Println("  4. Monitor /var/log/autonomy.log for behavior")
}
