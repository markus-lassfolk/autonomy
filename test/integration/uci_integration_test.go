package integration

import (
	"context"
	"fmt"
	"os"
	"path/filepath"
	"testing"
	"time"

	"github.com/markus-lassfolk/autonomy/pkg/logx"
	"github.com/markus-lassfolk/autonomy/pkg/uci"
)

func TestUCIIntegration(t *testing.T) {
	// Create temporary test directory
	testDir := t.TempDir()

	// Create test logger
	logger := logx.NewLogger("debug", "test")

	// Create native UCI client
	client := uci.NewNativeUCI(testDir, logger)

	// Create config validator
	validator := uci.NewConfigValidator(logger)

	// Test basic UCI operations
	t.Run("BasicUCIOperations", func(t *testing.T) {
		testBasicUCIOperations(t, client)
	})

	// Test configuration validation
	t.Run("ConfigurationValidation", func(t *testing.T) {
		testConfigurationValidation(t, validator)
	})

	// Test performance
	t.Run("Performance", func(t *testing.T) {
		testUCIPerformance(t, client)
	})

	// Test error handling
	t.Run("ErrorHandling", func(t *testing.T) {
		testUCIErrorHandling(t, client, testDir)
	})

	// Test concurrent access
	t.Run("ConcurrentAccess", func(t *testing.T) {
		testUCIConcurrentAccess(t, client)
	})
}

func testBasicUCIOperations(t *testing.T, client *uci.NativeUCI) {
	ctx := context.Background()

	// Test setting and getting values
	t.Run("SetAndGet", func(t *testing.T) {
		// Set a value
		err := client.Set(ctx, "autonomy", "main", "enable", "1")
		if err != nil {
			t.Fatalf("Failed to set value: %v", err)
		}

		// Get the value
		value, err := client.Get(ctx, "autonomy", "main", "enable")
		if err != nil {
			t.Fatalf("Failed to get value: %v", err)
		}

		if value != "1" {
			t.Errorf("Expected value '1', got '%s'", value)
		}
	})

	// Test updating existing values
	t.Run("UpdateValue", func(t *testing.T) {
		// Set initial value
		err := client.Set(ctx, "autonomy", "main", "log_level", "info")
		if err != nil {
			t.Fatalf("Failed to set initial value: %v", err)
		}

		// Update value
		err = client.Set(ctx, "autonomy", "main", "log_level", "debug")
		if err != nil {
			t.Fatalf("Failed to update value: %v", err)
		}

		// Verify update
		value, err := client.Get(ctx, "autonomy", "main", "log_level")
		if err != nil {
			t.Fatalf("Failed to get updated value: %v", err)
		}

		if value != "debug" {
			t.Errorf("Expected value 'debug', got '%s'", value)
		}
	})

	// Test creating new sections
	t.Run("CreateSection", func(t *testing.T) {
		err := client.Set(ctx, "autonomy", "gps", "enabled", "1")
		if err != nil {
			t.Fatalf("Failed to create new section: %v", err)
		}

		value, err := client.Get(ctx, "autonomy", "gps", "enabled")
		if err != nil {
			t.Fatalf("Failed to get value from new section: %v", err)
		}

		if value != "1" {
			t.Errorf("Expected value '1', got '%s'", value)
		}
	})

	// Test commit operation
	t.Run("Commit", func(t *testing.T) {
		err := client.Commit(ctx, "autonomy")
		if err != nil {
			t.Fatalf("Failed to commit changes: %v", err)
		}
	})
}

func testConfigurationValidation(t *testing.T, validator *uci.ConfigValidator) {
	ctx := context.Background()

	// Test valid configuration
	t.Run("ValidConfiguration", func(t *testing.T) {
		config := createValidConfig()
		result := validator.ValidateConfiguration(ctx, config)

		if !result.Valid {
			t.Errorf("Expected valid configuration, got errors: %v", result.Errors)
		}

		if len(result.Warnings) > 0 {
			t.Logf("Configuration warnings: %v", result.Warnings)
		}
	})

	// Test invalid configuration
	t.Run("InvalidConfiguration", func(t *testing.T) {
		config := createInvalidConfig()
		result := validator.ValidateConfiguration(ctx, config)

		if result.Valid {
			t.Error("Expected invalid configuration, but validation passed")
		}

		if len(result.Errors) == 0 {
			t.Error("Expected validation errors, but none were found")
		}

		t.Logf("Validation errors: %v", result.Errors)
	})

	// Test configuration with warnings
	t.Run("ConfigurationWithWarnings", func(t *testing.T) {
		config := createConfigWithWarnings()
		result := validator.ValidateConfiguration(ctx, config)

		if len(result.Warnings) == 0 {
			t.Error("Expected configuration warnings, but none were found")
		}

		t.Logf("Configuration warnings: %v", result.Warnings)
	})
}

func testUCIPerformance(t *testing.T, client *uci.NativeUCI) {
	ctx := context.Background()

	// Test read performance
	t.Run("ReadPerformance", func(t *testing.T) {
		// Set up test data
		for i := 0; i < 100; i++ {
			err := client.Set(ctx, "autonomy", "main", fmt.Sprintf("test_option_%d", i), fmt.Sprintf("value_%d", i))
			if err != nil {
				t.Fatalf("Failed to set test data: %v", err)
			}
		}

		// Measure read performance
		start := time.Now()
		for i := 0; i < 1000; i++ {
			_, err := client.Get(ctx, "autonomy", "main", "enable")
			if err != nil {
				t.Fatalf("Failed to read value: %v", err)
			}
		}
		duration := time.Since(start)

		t.Logf("Read performance: %d reads in %v (%.2f reads/sec)",
			1000, duration, float64(1000)/duration.Seconds())

		// Performance should be reasonable (less than 1ms per read on average)
		avgReadTime := duration / 1000
		if avgReadTime > time.Millisecond {
			t.Errorf("Read performance too slow: %v average per read", avgReadTime)
		}
	})

	// Test write performance
	t.Run("WritePerformance", func(t *testing.T) {
		start := time.Now()
		for i := 0; i < 100; i++ {
			err := client.Set(ctx, "autonomy", "main", fmt.Sprintf("perf_test_%d", i), fmt.Sprintf("value_%d", i))
			if err != nil {
				t.Fatalf("Failed to write value: %v", err)
			}
		}
		duration := time.Since(start)

		t.Logf("Write performance: %d writes in %v (%.2f writes/sec)",
			100, duration, float64(100)/duration.Seconds())

		// Performance should be reasonable (less than 10ms per write on average)
		avgWriteTime := duration / 100
		if avgWriteTime > 10*time.Millisecond {
			t.Errorf("Write performance too slow: %v average per write", avgWriteTime)
		}
	})

	// Test cache performance
	t.Run("CachePerformance", func(t *testing.T) {
		// First read (cache miss)
		start := time.Now()
		_, err := client.Get(ctx, "autonomy", "main", "enable")
		if err != nil {
			t.Fatalf("Failed to read value: %v", err)
		}
		firstRead := time.Since(start)

		// Second read (cache hit)
		start = time.Now()
		_, err = client.Get(ctx, "autonomy", "main", "enable")
		if err != nil {
			t.Fatalf("Failed to read cached value: %v", err)
		}
		secondRead := time.Since(start)

		t.Logf("Cache performance: first read %v, cached read %v", firstRead, secondRead)

		// Cached read should be significantly faster
		if secondRead >= firstRead {
			t.Errorf("Cache not working: cached read (%v) not faster than first read (%v)",
				secondRead, firstRead)
		}
	})
}

func testUCIErrorHandling(t *testing.T, client *uci.NativeUCI, testDir string) {
	ctx := context.Background()

	// Test reading non-existent option
	t.Run("NonExistentOption", func(t *testing.T) {
		_, err := client.Get(ctx, "autonomy", "main", "non_existent_option")
		if err == nil {
			t.Error("Expected error when reading non-existent option")
		}

		t.Logf("Expected error for non-existent option: %v", err)
	})

	// Test reading from non-existent section
	t.Run("NonExistentSection", func(t *testing.T) {
		_, err := client.Get(ctx, "autonomy", "non_existent_section", "option")
		if err == nil {
			t.Error("Expected error when reading from non-existent section")
		}

		t.Logf("Expected error for non-existent section: %v", err)
	})

	// Test invalid config file
	t.Run("InvalidConfigFile", func(t *testing.T) {
		// Create invalid config file
		configPath := filepath.Join(testDir, "invalid")
		err := os.WriteFile(configPath, []byte("invalid config content"), 0o644)
		if err != nil {
			t.Fatalf("Failed to create invalid config file: %v", err)
		}

		// Try to read from invalid config
		_, err = client.Get(ctx, "invalid", "section", "option")
		if err == nil {
			t.Error("Expected error when reading from invalid config file")
		}

		t.Logf("Expected error for invalid config file: %v", err)
	})
}

func testUCIConcurrentAccess(t *testing.T, client *uci.NativeUCI) {
	ctx := context.Background()

	// Test concurrent reads
	t.Run("ConcurrentReads", func(t *testing.T) {
		const numGoroutines = 10
		const readsPerGoroutine = 100

		// Set up test data
		err := client.Set(ctx, "autonomy", "main", "concurrent_test", "value")
		if err != nil {
			t.Fatalf("Failed to set test data: %v", err)
		}

		// Start concurrent reads
		start := time.Now()
		done := make(chan bool, numGoroutines)

		for i := 0; i < numGoroutines; i++ {
			go func(id int) {
				for j := 0; j < readsPerGoroutine; j++ {
					_, err := client.Get(ctx, "autonomy", "main", "concurrent_test")
					if err != nil {
						t.Errorf("Goroutine %d failed to read: %v", id, err)
					}
				}
				done <- true
			}(i)
		}

		// Wait for all goroutines to complete
		for i := 0; i < numGoroutines; i++ {
			<-done
		}

		duration := time.Since(start)
		totalReads := numGoroutines * readsPerGoroutine

		t.Logf("Concurrent reads: %d reads in %v (%.2f reads/sec)",
			totalReads, duration, float64(totalReads)/duration.Seconds())
	})

	// Test concurrent writes
	t.Run("ConcurrentWrites", func(t *testing.T) {
		const numGoroutines = 5
		const writesPerGoroutine = 20

		// Start concurrent writes
		start := time.Now()
		done := make(chan bool, numGoroutines)

		for i := 0; i < numGoroutines; i++ {
			go func(id int) {
				for j := 0; j < writesPerGoroutine; j++ {
					err := client.Set(ctx, "autonomy", "main",
						fmt.Sprintf("concurrent_write_%d_%d", id, j),
						fmt.Sprintf("value_%d_%d", id, j))
					if err != nil {
						t.Errorf("Goroutine %d failed to write: %v", id, err)
					}
				}
				done <- true
			}(i)
		}

		// Wait for all goroutines to complete
		for i := 0; i < numGoroutines; i++ {
			<-done
		}

		duration := time.Since(start)
		totalWrites := numGoroutines * writesPerGoroutine

		t.Logf("Concurrent writes: %d writes in %v (%.2f writes/sec)",
			totalWrites, duration, float64(totalWrites)/duration.Seconds())

		// Verify all writes were successful
		for i := 0; i < numGoroutines; i++ {
			for j := 0; j < writesPerGoroutine; j++ {
				value, err := client.Get(ctx, "autonomy", "main",
					fmt.Sprintf("concurrent_write_%d_%d", i, j))
				if err != nil {
					t.Errorf("Failed to verify write %d_%d: %v", i, j, err)
				}
				expected := fmt.Sprintf("value_%d_%d", i, j)
				if value != expected {
					t.Errorf("Write verification failed for %d_%d: expected %s, got %s",
						i, j, expected, value)
				}
			}
		}
	})
}

// Helper functions to create test configurations
func createValidConfig() *uci.Config {
	return &uci.Config{
		Enable:                             true,
		UseMWAN3:                           true,
		PollIntervalMS:                     1500,
		DecisionIntervalMS:                 1000,
		DiscoveryIntervalMS:                5000,
		CleanupIntervalMS:                  10000,
		HistoryWindowS:                     600,
		RetentionHours:                     24,
		MaxRAMMB:                           16,
		DataCapMode:                        "balanced",
		Predictive:                         true,
		SwitchMargin:                       10,
		MinUptimeS:                         20,
		CooldownS:                          20,
		MetricsListener:                    false,
		HealthListener:                     true,
		LogLevel:                           "info",
		LogFile:                            "",
		PerformanceProfiling:               false,
		SecurityAuditing:                   true,
		ProfilingEnabled:                   false,
		AuditingEnabled:                    true,
		MLEnabled:                          true,
		MLModelPath:                        "/etc/autonomy/models.json",
		MLTraining:                         true,
		MLPrediction:                       true,
		StarlinkAPIHost:                    "192.168.100.1",
		StarlinkAPIPort:                    9200,
		StarlinkTimeout:                    10,
		StarlinkGRPCFirst:                  true,
		StarlinkHTTPFirst:                  false,
		RespectUserWeights:                 true,
		DynamicAdjustment:                  true,
		EmergencyOverride:                  true,
		OnlyEmergencyOverride:              false,
		RestoreTimeoutS:                    300,
		MinimalAdjustmentPoints:            5,
		TemporaryBoostPoints:               10,
		TemporaryAdjustmentDurationS:       300,
		EmergencyAdjustmentDurationS:       60,
		StarlinkObstructionThreshold:       0.1,
		CellularSignalThreshold:            -85.0,
		LatencyDegradationThreshold:        50.0,
		LossThreshold:                      5.0,
		AllowedIPs:                         []string{"127.0.0.1", "192.168.1.0/24"},
		BlockedIPs:                         []string{},
		AllowedPorts:                       []int{22, 80, 443},
		BlockedPorts:                       []int{},
		MaxFailedAttempts:                  5,
		BlockDuration:                      300,
		FailThresholdLoss:                  10,
		FailThresholdLatency:               100,
		FailMinDurationS:                   30,
		RestoreThresholdLoss:               5,
		RestoreThresholdLatency:            50,
		RestoreMinDurationS:                60,
		PriorityThreshold:                  "medium",
		AcknowledgmentTracking:             true,
		LocationEnabled:                    true,
		RichContextEnabled:                 true,
		NotifyOnFailover:                   true,
		NotifyOnFailback:                   true,
		NotifyOnMemberDown:                 true,
		NotifyOnMemberUp:                   true,
		NotifyOnPredictive:                 true,
		NotifyOnCritical:                   true,
		NotifyOnRecovery:                   true,
		NotificationCooldownS:              300,
		MaxNotificationsHour:               10,
		PriorityFailover:                   1,
		PriorityFailback:                   2,
		PriorityMemberDown:                 3,
		PriorityMemberUp:                   4,
		PriorityPredictive:                 5,
		PriorityCritical:                   1,
		PriorityRecovery:                   2,
		PushoverEnabled:                    false,
		PushoverToken:                      "",
		PushoverUser:                       "",
		PushoverDevice:                     "",
		EmailEnabled:                       false,
		EmailSMTPHost:                      "",
		EmailSMTPPort:                      587,
		EmailUsername:                      "",
		EmailPassword:                      "",
		EmailFrom:                          "",
		EmailTo:                            []string{},
		EmailUseTLS:                        true,
		EmailUseStartTLS:                   true,
		SlackEnabled:                       false,
		SlackWebhookURL:                    "",
		SlackChannel:                       "",
		SlackUsername:                      "",
		SlackIconEmoji:                     "",
		SlackIconURL:                       "",
		DiscordEnabled:                     false,
		DiscordWebhookURL:                  "",
		DiscordUsername:                    "",
		DiscordAvatarURL:                   "",
		TelegramEnabled:                    false,
		TelegramToken:                      "",
		TelegramChatID:                     "",
		WebhookEnabled:                     false,
		WebhookURL:                         "",
		WebhookMethod:                      "POST",
		WebhookContentType:                 "application/json",
		WebhookHeaders:                     map[string]string{},
		WebhookTemplate:                    "",
		WebhookTemplateFormat:              "json",
		WebhookAuthType:                    "none",
		WebhookAuthToken:                   "",
		WebhookAuthUsername:                "",
		WebhookAuthPassword:                "",
		WebhookAuthHeader:                  "",
		WebhookTimeout:                     30,
		WebhookRetryAttempts:               3,
		WebhookRetryDelay:                  5,
		WebhookVerifySSL:                   true,
		WebhookFollowRedirect:              true,
		WebhookPriorityFilter:              []int{},
		WebhookTypeFilter:                  []string{},
		WebhookName:                        "",
		WebhookDescription:                 "",
		MQTTBroker:                         "",
		MQTTTopic:                          "autonomy/status",
		MQTT:                               uci.MQTTConfig{},
		WiFiOptimizationEnabled:            false,
		WiFiMovementThreshold:              500.0,
		WiFiStationaryTime:                 300,
		WiFiNightlyOptimization:            false,
		WiFiNightlyTime:                    "02:00",
		WiFiNightlyWindow:                  60,
		WiFiWeeklyOptimization:             false,
		WiFiWeeklyDays:                     "0,6",
		WiFiWeeklyTime:                     "03:00",
		WiFiWeeklyWindow:                   120,
		WiFiMinImprovement:                 10,
		WiFiDwellTime:                      60,
		WiFiNoiseDefault:                   -90,
		WiFiVHT80Threshold:                 -50,
		WiFiVHT40Threshold:                 -60,
		WiFiUseDFS:                         false,
		WiFiOptimizationCooldown:           3600,
		WiFiGPSAccuracyThreshold:           50.0,
		WiFiLocationLogging:                false,
		WiFiSchedulerCheckInterval:         5,
		WiFiSkipIfRecent:                   true,
		WiFiRecentThreshold:                24,
		WiFiTimezone:                       "UTC",
		WiFiUseEnhancedScanner:             false,
		WiFiStrongRSSIThreshold:            -60,
		WiFiWeakRSSIThreshold:              -80,
		WiFiUtilizationWeight:              10,
		WiFiExcellentThreshold:             90,
		WiFiGoodThreshold:                  75,
		WiFiFairThreshold:                  50,
		WiFiPoorThreshold:                  25,
		WiFiOverlapPenaltyRatio:            0.5,
		MeteredModeEnabled:                 false,
		DataLimitWarningThreshold:          80,
		DataLimitCriticalThreshold:         95,
		DataUsageHysteresisMargin:          5,
		MeteredStabilityDelay:              300,
		MeteredClientReconnectMethod:       "gentle",
		MeteredModeDebug:                   false,
		GPSEnabled:                         true,
		GPSSourcePriority:                  []string{"rutos", "starlink", "google"},
		GPSMovementThresholdM:              500.0,
		GPSAccuracyThresholdM:              50.0,
		GPSStalenessThresholdS:             300,
		GPSCollectionIntervalS:             30,
		GPSMovementDetection:               true,
		GPSLocationClustering:              true,
		GPSRetryAttempts:                   3,
		GPSRetryDelayS:                     5,
		GPSGoogleAPIEnabled:                false,
		GPSGoogleAPIKey:                    "",
		GPSGoogleElevationAPIEnabled:       false,
		GPSHybridPrioritization:            true,
		GPSMinAcceptableConfidence:         0.5,
		GPSFallbackConfidenceThreshold:     0.7,
		GPSAPIServerEnabled:                false,
		GPSAPIServerPort:                   8080,
		GPSAPIServerHost:                   "localhost",
		GPSAPIServerAuthKey:                "",
		GPSCellTowerEnabled:                false,
		GPSMozillaEnabled:                  false,
		GPSOpenCellIDEnabled:               false,
		GPSOpenCellIDAPIKey:                "",
		GPSOpenCellIDContribute:            false,
		GPSCellTowerMaxCells:               10,
		GPSCellTowerTimeout:                30,
		GPSOpenCellIDCacheSizeMB:           10,
		GPSOpenCellIDMaxCellsPerLookup:     10,
		GPSOpenCellIDNegativeCacheTTLHours: 24,
		GPSOpenCellIDContributionInterval:  60,
		GPSOpenCellIDMinGPSAccuracy:        100.0,
		GPSOpenCellIDMovementThreshold:     100.0,
		GPSOpenCellIDRSRPChangeThreshold:   5.0,
		GPSOpenCellIDTimingAdvanceEnabled:  false,
		GPSOpenCellIDFusionConfidence:      0.8,
		GPSOpenCellIDRatioLimit:            8.0,
		GPSOpenCellIDRatioWindowHours:      48,
		GPSOpenCellIDSchedulerEnabled:      true,
		GPSOpenCellIDMovingInterval:        2,
		GPSOpenCellIDStationaryInterval:    10,
		GPSOpenCellIDMaxScansPerHour:       30,
		GPSLocalDBEnabled:                  false,
		GPSLocalDBPath:                     "/var/lib/autonomy/cells.db",
		GPSLocalDBMaxObservations:          10000,
		GPSLocalDBRetentionDays:            30,
		GPSLocalDBMinAccuracy:              100.0,
		GPS5GEnabled:                       false,
		GPS5GMaxNeighborCells:              10,
		GPS5GSignalThreshold:               -100,
		GPS5GCarrierAggregation:            false,
		GPS5GCollectionTimeout:             30,
		GPSCellularIntelEnabled:            false,
		GPSCellularMaxNeighbors:            10,
		GPSCellularSignalThreshold:         -100,
		GPSCellularFingerprinting:          false,
		GPSHealthEnabled:                   false,
		GPSHealthCheckInterval:             300,
		GPSHealthMaxFailures:               3,
		GPSHealthMinAccuracy:               100.0,
		GPSHealthMinSatellites:             4,
	}
}

func createInvalidConfig() *uci.Config {
	config := createValidConfig()
	// Make it invalid by setting impossible values
	config.PollIntervalMS = -1     // Invalid negative value
	config.LogLevel = "invalid"    // Invalid log level
	config.DataCapMode = "invalid" // Invalid data cap mode
	return config
}

func createConfigWithWarnings() *uci.Config {
	config := createValidConfig()
	// Set values that will generate warnings
	config.FailThresholdLoss = 5
	config.RestoreThresholdLoss = 10 // Warning: fail threshold should be higher than restore
	config.FailThresholdLatency = 50
	config.RestoreThresholdLatency = 100 // Warning: fail threshold should be higher than restore
	return config
}
