package main

import (
	"context"
	"encoding/json"
	"flag"
	"fmt"
	"os"
	"os/signal"
	"runtime"
	"syscall"
	"time"

	"github.com/markus-lassfolk/autonomy/pkg"

	"github.com/markus-lassfolk/autonomy/pkg/collector"
	"github.com/markus-lassfolk/autonomy/pkg/controller"
	"github.com/markus-lassfolk/autonomy/pkg/decision"
	"github.com/markus-lassfolk/autonomy/pkg/discovery"
	"github.com/markus-lassfolk/autonomy/pkg/gps"
	"github.com/markus-lassfolk/autonomy/pkg/health"
	"github.com/markus-lassfolk/autonomy/pkg/logx"
	"github.com/markus-lassfolk/autonomy/pkg/metered"
	"github.com/markus-lassfolk/autonomy/pkg/metrics"
	"github.com/markus-lassfolk/autonomy/pkg/mqtt"
	"github.com/markus-lassfolk/autonomy/pkg/performance"
	"github.com/markus-lassfolk/autonomy/pkg/pidfile"
	"github.com/markus-lassfolk/autonomy/pkg/security"
	"github.com/markus-lassfolk/autonomy/pkg/sysmgmt"
	"github.com/markus-lassfolk/autonomy/pkg/telem"
	"github.com/markus-lassfolk/autonomy/pkg/ubus"
	"github.com/markus-lassfolk/autonomy/pkg/uci"
)

var (
	configPath = flag.String("config", "/etc/config/autonomy", "Path to UCI configuration file")
	pidPath    = flag.String("pid-file", "/tmp/autonomy.pid", "Path to PID file")
	logLevel   = flag.String("log-level", "", "Override log level (debug|info|warn|error|trace)")
	version    = flag.Bool("version", false, "Show version information")
	profile    = flag.Bool("profile", false, "Enable performance profiling")
	audit      = flag.Bool("audit", false, "Enable security auditing")
	monitor    = flag.Bool("monitor", false, "Run in monitoring mode with verbose output")
	verbose    = flag.Bool("verbose", false, "Enable verbose logging (equivalent to trace level)")
	foreground = flag.Bool("foreground", false, "Run in foreground mode (don't daemonize)")
	dryRun     = flag.Bool("dry-run", false, "Dry run mode - don't make changes, only log intended actions")
	force      = flag.Bool("force", false, "Force start by removing stale PID file")

	// Health check options
	healthCheck        = flag.String("health-check", "", "Run specific health check and exit (starlink|uci|overlay|service|network|database|time|log|all)")
	healthCheckOnce    = flag.Bool("health-check-once", false, "Run all health checks once and exit")
	healthCheckVerbose = flag.Bool("health-check-verbose", false, "Enable verbose output for health checks")
)

const (
	AppName    = "autonomyd"
	AppVersion = "1.0.0"
)

// HeartbeatData represents the heartbeat information written to /tmp/autonomyd.health
type HeartbeatData struct {
	Timestamp      string  `json:"ts"`               // RFC3339Z timestamp
	UptimeS        int64   `json:"uptime_s"`         // Uptime in seconds
	Version        string  `json:"version"`          // Application version
	Status         string  `json:"status"`           // Current status (ok|degraded|cooldown)
	LastFailoverTS string  `json:"last_failover_ts"` // RFC3339Z timestamp of last failover
	MemMB          float64 `json:"mem_mb"`           // Memory usage in MB
	Goroutines     int     `json:"goroutines"`       // Number of goroutines
	DeviceID       string  `json:"device_id"`        // Device identifier
}

func main() {
	flag.Parse()

	if *version {
		fmt.Printf("%s version %s\n", AppName, AppVersion)
		os.Exit(0)
	}

	// Determine log level
	effectiveLogLevel := "info"
	if *logLevel != "" {
		effectiveLogLevel = *logLevel
	}
	if *verbose || *monitor {
		effectiveLogLevel = "trace"
	}

	// Initialize logger with component name
	logger := logx.NewLogger(effectiveLogLevel, "autonomyd")

	// Initialize PID file management
	pidFile := pidfile.New(*pidPath)

	// Check if another instance is already running
	running, existingPID, err := pidFile.CheckRunning()
	if err != nil {
		logger.Error("Failed to check for running instance", "error", err)
		os.Exit(1)
	}

	if running {
		if *force {
			logger.Warn("Another instance is running, but force flag specified", "existing_pid", existingPID)
			if err := pidFile.ForceRemove(); err != nil {
				logger.Error("Failed to remove existing PID file", "error", err)
				os.Exit(1)
			}
		} else {
			logger.Error("Another instance is already running", "existing_pid", existingPID, "pid_file", *pidPath)
			fmt.Fprintf(os.Stderr, "Error: %s is already running with PID %d\n", AppName, existingPID)
			fmt.Fprintf(os.Stderr, "Use --force to override, or stop the existing instance first\n")
			os.Exit(1)
		}
	}

	// Create PID file
	if err := pidFile.Create(); err != nil {
		logger.Error("Failed to create PID file", "error", err, "path", *pidPath)
		os.Exit(1)
	}

	// Ensure PID file is cleaned up on exit
	defer func() {
		if err := pidFile.Remove(); err != nil {
			logger.Error("Failed to remove PID file", "error", err)
		}
	}()

	logger.Info("Starting autonomy daemon", "version", AppVersion, "pid", os.Getpid(), "pid_file", *pidPath)

	// Load configuration
	cfg, err := uci.LoadConfig(*configPath)
	if err != nil {
		logger.Error("Failed to load configuration", "error", err, "path", *configPath)
		os.Exit(1)
	}

	// Initialize UCI client and config manager to ensure all required entries exist
	uciClient := uci.NewUCI(logger)
	configManager := uci.NewConfigManager(uciClient)
	if err := configManager.EnsureRequiredConfig(context.Background()); err != nil {
		logger.Error("Failed to ensure required configuration", "error", err)
		os.Exit(1)
	}

	// Commit any configuration changes
	if err := configManager.Commit(context.Background()); err != nil {
		logger.Error("Failed to commit configuration changes", "error", err)
		os.Exit(1)
	}

	// Apply log level override if specified
	if *logLevel != "" {
		cfg.LogLevel = *logLevel
		logger.SetLevel(cfg.LogLevel)
	}

	logger.Info("Configuration loaded", "predictive", cfg.Predictive, "use_mwan3", cfg.UseMWAN3)
	if *dryRun {
		logger.Info("Dry-run mode enabled: no system changes will be applied")
	}

	// Handle health check commands
	if *healthCheck != "" || *healthCheckOnce {
		runHealthCheckCommand(logger, cfg, *healthCheck, *healthCheckOnce, *healthCheckVerbose, *dryRun)
		return
	}

	// Log monitoring mode status
	if *monitor {
		logger.Info("Running in monitoring mode", "verbose_logging", true, "foreground", *foreground)
		logger.LogVerbose("monitoring_mode_enabled", map[string]interface{}{
			"log_level": effectiveLogLevel,
			"profile":   *profile,
			"audit":     *audit,
			"verbose":   *verbose,
		})
	}

	// Initialize telemetry store
	telemetry, err := telem.NewStore(cfg.RetentionHours, cfg.MaxRAMMB)
	if err != nil {
		logger.Error("Failed to initialize telemetry store", "error", err)
		os.Exit(1)
	}
	defer telemetry.Close()

	// Initialize performance profiler
	var profiler *performance.Profiler
	if *profile || cfg.PerformanceProfiling {
		profiler = performance.NewProfiler(true, 30*time.Second, 1000, logger)
		profiler.Start(context.Background())
		defer profiler.Stop()
		logger.Info("Performance profiler enabled")
	}

	// Initialize security auditor
	var auditor *security.Auditor
	if *audit || cfg.SecurityAuditing {
		auditConfig := &security.AuditConfig{
			Enabled:           true,
			LogLevel:          cfg.LogLevel,
			MaxEvents:         1000,
			RetentionDays:     30,
			FileIntegrity:     true,
			NetworkSecurity:   true,
			AccessControl:     true,
			ThreatDetection:   true,
			CriticalFiles:     []string{"/etc/config/autonomy", "/usr/sbin/autonomyd"},
			AllowedIPs:        cfg.AllowedIPs,
			BlockedIPs:        cfg.BlockedIPs,
			AllowedPorts:      []int{8080, 9090},
			BlockedPorts:      []int{22, 23, 25},
			MaxFailedAttempts: 5,
			BlockDuration:     24,
		}
		auditor = security.NewAuditor(auditConfig, logger)
		auditor.Start(context.Background())
		defer auditor.Stop()
		logger.Info("Security auditor enabled")
	}

	// Predictive configuration is handled internally by the decision engine

	// Initialize decision engine with predictive capabilities
	decisionEngine := decision.NewEngine(cfg, logger, telemetry)
	if cfg.Predictive {
		logger.Info("Predictive failover engine enabled via configuration")
	}

	// Initialize discovery system
	discoverer := discovery.NewDiscoverer(logger)

	// Discover initial members
	members, err := discoverer.DiscoverMembers()
	if err != nil {
		logger.Error("Failed to discover members", "error", err)
		os.Exit(1)
	}

	logger.Info("Initial member discovery completed", "count", len(members))

	// Initialize controller with discovered members
	ctrl, err := controller.NewController(cfg, logger)
	if err != nil {
		logger.Error("Failed to initialize controller", "error", err)
		os.Exit(1)
	}
	// Apply dry-run to controller
	ctrl.SetDryRun(*dryRun)

	// Set discovered members in controller
	if err := ctrl.SetMembers(members); err != nil {
		logger.Error("Failed to set members", "error", err)
		os.Exit(1)
	}

	// Initialize collector factory with UCI configuration
	collectorConfig := map[string]interface{}{
		"timeout":             time.Duration(cfg.StarlinkTimeout) * time.Second,
		"targets":             []string{"8.8.8.8", "1.1.1.1", "1.0.0.1"},
		"ubus_path":           "ubus",
		"starlink_api_host":   cfg.StarlinkAPIHost,
		"starlink_api_port":   cfg.StarlinkAPIPort,
		"starlink_timeout_s":  cfg.StarlinkTimeout,
		"starlink_grpc_first": cfg.StarlinkGRPCFirst,
		"starlink_http_first": cfg.StarlinkHTTPFirst,
	}
	collectorFactory := collector.NewCollectorFactory(collectorConfig)

	// Add discovered members to decision engine
	for _, member := range members {
		decisionEngine.AddMember(member)
		logger.Info("Added member to decision engine", "member", member.Name, "class", member.Class)
	}

	// Initialize decision engine with current active member to prevent unnecessary switches on startup
	if err := decisionEngine.Initialize(ctrl); err != nil {
		logger.Warn("Failed to initialize decision engine with current member", "error", err)
		// Don't exit, this is not critical
	}

	// Initialize and start metrics server if enabled
	var metricsServer *metrics.Server
	if cfg.MetricsListener {
		metricsServer = metrics.NewServer(ctrl, decisionEngine, telemetry, logger)
		if err := metricsServer.Start(cfg.MetricsPort); err != nil {
			logger.Error("Failed to start metrics server", "error", err)
			os.Exit(1)
		}
		defer metricsServer.Stop()
	}

	// Initialize and start health server if enabled
	var healthServer *health.Server
	if cfg.HealthListener {
		healthServer = health.NewServer(ctrl, decisionEngine, telemetry, logger)
		if err := healthServer.Start(cfg.HealthPort); err != nil {
			logger.Error("Failed to start health server", "error", err)
			os.Exit(1)
		}
		defer healthServer.Stop()
	}

	// Initialize MQTT client if enabled
	var mqttClient *mqtt.Client
	if cfg.MQTT.Enabled {
		// Convert UCI MQTT config to MQTT client config
		mqttConfig := &mqtt.Config{
			Broker:      cfg.MQTT.Broker,
			Port:        cfg.MQTT.Port,
			ClientID:    cfg.MQTT.ClientID,
			Username:    cfg.MQTT.Username,
			Password:    cfg.MQTT.Password,
			TopicPrefix: cfg.MQTT.TopicPrefix,
			QoS:         cfg.MQTT.QoS,
			Retain:      cfg.MQTT.Retain,
			Enabled:     cfg.MQTT.Enabled,
		}
		mqttClient = mqtt.NewClient(mqttConfig, logger)
		if err := mqttClient.Connect(); err != nil {
			logger.Error("Failed to connect to MQTT broker", "error", err)
			// Don't exit, MQTT is optional
		} else {
			defer mqttClient.Disconnect()
		}
	}

	// Initialize comprehensive GPS collector as core GPS component
	gpsConfig := &gps.ComprehensiveGPSConfig{
		Enabled:                  cfg.GPSEnabled,
		SourcePriority:           cfg.GPSSourcePriority,
		MovementThresholdM:       cfg.GPSMovementThresholdM,
		AccuracyThresholdM:       cfg.GPSAccuracyThresholdM,
		StalenessThresholdS:      cfg.GPSStalenessThresholdS,
		CollectionTimeoutS:       30, // Keep fixed for now
		RetryAttempts:            cfg.GPSRetryAttempts,
		RetryDelayS:              cfg.GPSRetryDelayS,
		GoogleAPIEnabled:         cfg.GPSGoogleAPIEnabled,
		GoogleAPIKey:             cfg.GPSGoogleAPIKey,
		NMEADevices:              []string{"/dev/ttyUSB1", "/dev/ttyUSB2", "/dev/ttyACM0"},
		PreferHighAccuracy:       true, // Keep fixed for now
		EnableMovementDetection:  cfg.GPSMovementDetection,
		EnableLocationClustering: cfg.GPSLocationClustering,

		// Hybrid Confidence-Based Prioritization
		EnableHybridPrioritization:  cfg.GPSHybridPrioritization,
		MinAcceptableConfidence:     cfg.GPSMinAcceptableConfidence,
		FallbackConfidenceThreshold: cfg.GPSFallbackConfidenceThreshold,
	}

	gpsCollector := gps.NewComprehensiveGPSCollector(gpsConfig, logger)
	if gpsCollector == nil {
		logger.Error("Failed to initialize comprehensive GPS collector")
		os.Exit(1)
	}

	// GPS API server skipped - using comprehensive GPS collector directly
	logger.Info("GPS API server disabled - using comprehensive GPS collector directly")

	logger.Info("Comprehensive GPS collector initialized as core component",
		"google_api_enabled", cfg.GPSGoogleAPIEnabled,
		"google_api_key_configured", cfg.GPSGoogleAPIKey != "",
		"sources", gpsConfig.SourcePriority,
		"accuracy_threshold", gpsConfig.AccuracyThresholdM)

	// Initialize ubus server with GPS support
	ubusServer := ubus.NewServer(ctrl, decisionEngine, telemetry, gpsCollector, logger)

	// Start ubus server
	if err := ubusServer.Start(context.Background()); err != nil {
		logger.Error("Failed to start ubus server", "error", err)
		os.Exit(1)
	}
	defer ubusServer.Stop()

	// Setup signal handling
	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()

	sigChan := make(chan os.Signal, 1)
	signal.Notify(sigChan, syscall.SIGINT, syscall.SIGTERM, syscall.SIGHUP)

	// Initialize system management with WiFi optimization
	sysmgmtManager, err := initializeSystemManagement(cfg, logger, false, gpsCollector)
	if err != nil {
		logger.Error("Failed to initialize system management", "error", err)
		// Continue without system management
		sysmgmtManager = nil
	}

	// Set WiFi manager in ubus server if system management is available
	if sysmgmtManager != nil {
		if wifiManager := sysmgmtManager.GetWiFiManager(); wifiManager != nil {
			ubusServer.SetWiFiManager(wifiManager)
			logger.Info("WiFi manager set in ubus server for API access")
		}
	}

	// Initialize metered mode manager
	meteredManager, err := metered.NewManager(cfg, logger)
	if err != nil {
		logger.Error("Failed to initialize metered mode manager", "error", err)
		// Continue without metered mode
		meteredManager = nil
	} else {
		logger.Info("Metered mode manager initialized")

		// Register failover callback with controller
		ctrl.AddFailoverCallback(meteredManager.OnFailover)

		// Set metered manager in ubus server for API access
		ubusServer.SetMeteredManager(meteredManager)
	}

	// Start heartbeat writer
	startTime := time.Now()
	heartbeatTicker := time.NewTicker(10 * time.Second)
	go writeHeartbeat(ctx, heartbeatTicker, startTime, logger, decisionEngine)

	// Start main loop
	go runMainLoop(ctx, cfg, decisionEngine, ctrl, logger, telemetry, discoverer, collectorFactory, metricsServer, healthServer, mqttClient, profiler, auditor, sysmgmtManager, meteredManager, gpsCollector)

	// Wait for shutdown signal
	sig := <-sigChan
	logger.Info("Received shutdown signal", "signal", sig)

	// Graceful shutdown
	shutdownCtx, shutdownCancel := context.WithTimeout(context.Background(), 10*time.Second)
	defer shutdownCancel()

	// Stop the main loop
	cancel()

	// Wait for shutdown or timeout
	select {
	case <-shutdownCtx.Done():
		logger.Warn("Shutdown timeout exceeded")
	case <-time.After(5 * time.Second):
		logger.Info("Graceful shutdown completed")
	}
}

func runMainLoop(ctx context.Context, cfg *uci.Config, engine *decision.Engine, ctrl *controller.Controller, logger *logx.Logger, telemetry *telem.Store, discoverer *discovery.Discoverer, collectorFactory *collector.CollectorFactory, metricsServer *metrics.Server, healthServer *health.Server, mqttClient *mqtt.Client, profiler *performance.Profiler, auditor *security.Auditor, sysmgmtManager *sysmgmt.Manager, meteredManager *metered.Manager, gpsCollector *gps.ComprehensiveGPSCollector) {
	// Set up event publishing callback if MQTT is enabled
	if mqttClient != nil && cfg.MQTT.Enabled {
		// Create a callback function for real-time event publishing
		eventPublisher := func(event *pkg.Event) {
			eventData := map[string]interface{}{
				"timestamp": event.Timestamp.Unix(),
				"type":      event.Type,
				"reason":    event.Reason,
				"member":    event.Member,
				"from":      event.From,
				"to":        event.To,
				"data":      event.Data,
			}

			if err := mqttClient.PublishEvent(eventData); err != nil {
				logger.Warn("Failed to publish real-time event to MQTT", "event_type", event.Type, "error", err)
			} else {
				logger.Debug("Published real-time event to MQTT", "event_type", event.Type)
			}
		}

		// Add the callback to telemetry store for immediate publishing
		telemetry.SetEventCallback(eventPublisher)
	}
	// Create tickers for different intervals
	decisionTicker := time.NewTicker(time.Duration(cfg.DecisionIntervalMS) * time.Millisecond)
	discoveryTicker := time.NewTicker(time.Duration(cfg.DiscoveryIntervalMS) * time.Millisecond)
	cleanupTicker := time.NewTicker(time.Duration(cfg.CleanupIntervalMS) * time.Millisecond)
	securityTicker := time.NewTicker(2 * time.Minute)
	performanceTicker := time.NewTicker(1 * time.Minute)
	mqttTicker := time.NewTicker(30 * time.Second)      // Publish telemetry every 30 seconds
	sysmgmtTicker := time.NewTicker(5 * time.Minute)    // System management checks every 5 minutes
	meteredTicker := time.NewTicker(5 * time.Minute)    // Metered mode checks every 5 minutes
	gpsTicker := time.NewTicker(60 * time.Second)       // GPS collection every 60 seconds
	gpsReevalTicker := time.NewTicker(10 * time.Minute) // GPS source re-evaluation every 10 minutes

	defer decisionTicker.Stop()
	defer discoveryTicker.Stop()
	defer cleanupTicker.Stop()
	defer securityTicker.Stop()
	defer performanceTicker.Stop()
	defer mqttTicker.Stop()
	defer sysmgmtTicker.Stop()
	defer meteredTicker.Stop()
	defer gpsTicker.Stop()
	defer gpsReevalTicker.Stop()

	logger.Info("Starting main loop", map[string]interface{}{
		"decision_interval_ms":  cfg.DecisionIntervalMS,
		"discovery_interval_ms": cfg.DiscoveryIntervalMS,
		"cleanup_interval_ms":   cfg.CleanupIntervalMS,
		"predictive":            cfg.Predictive,
		"profiling":             profiler != nil,
		"auditing":              auditor != nil,
	})

	for {
		select {
		case <-ctx.Done():
			logger.Info("Main loop stopped")
			return

		case <-decisionTicker.C:
			// Run decision engine tick
			if err := engine.Tick(ctrl); err != nil {
				logger.Error("Error in decision engine tick", map[string]interface{}{
					"error": err.Error(),
				})
			}

			// Update metrics if server is running
			if metricsServer != nil {
				metricsServer.UpdateMetrics()
			}

		case <-discoveryTicker.C:
			// Refresh member discovery
			currentMembers := ctrl.GetMembers()
			newMembers, err := discoverer.RefreshMembers(currentMembers)
			if err != nil {
				logger.Error("Error refreshing members", map[string]interface{}{
					"error": err.Error(),
				})
			} else {
				// Update controller with new members
				if err := ctrl.SetMembers(newMembers); err != nil {
					logger.Error("Failed to set members", map[string]interface{}{"error": err.Error()})
				} else {
					// Update decision engine with new members
					for _, member := range newMembers {
						engine.AddMember(member)
					}

					logger.Debug("Member discovery refreshed", map[string]interface{}{
						"member_count": len(newMembers),
					})
				}
			}

		case <-cleanupTicker.C:
			// Perform periodic cleanup
			telemetry.Cleanup()
			logger.Debug("Telemetry cleanup completed")

		case <-securityTicker.C:
			// Perform security checks
			if auditor != nil {
				// Check file integrity
				for _, filePath := range []string{"/etc/config/autonomy", "/usr/sbin/autonomyd"} {
					if _, err := auditor.ValidateFileIntegrity(filePath); err != nil {
						logger.Error("File integrity check failed", "file", filePath, "error", err)
					}
				}

				// Check network security
				for _, port := range []int{8080, 9090} {
					if _, err := auditor.CheckNetworkSecurity(port, "tcp"); err != nil {
						logger.Error("Network security check failed", "port", port, "error", err)
					}
				}
			}

		case <-performanceTicker.C:
			// Perform performance monitoring
			if profiler != nil {
				// Check memory usage
				memoryUsage := profiler.GetMemoryUsage()
				if memoryUsage > 100 { // 100MB threshold
					logger.Warn("High memory usage detected", "usage_mb", memoryUsage)
				}

				// Check goroutine count
				goroutineCount := profiler.GetGoroutineCount()
				if goroutineCount > 500 {
					logger.Warn("High goroutine count detected", "count", goroutineCount)
				}

				// Force GC if memory usage is high
				if memoryUsage > 200 { // 200MB threshold
					logger.Info("Forcing garbage collection due to high memory usage")
					profiler.ForceGC()
				}
			}

		case <-mqttTicker.C:
			// Publish telemetry data via MQTT
			if mqttClient != nil && cfg.MQTT.Enabled {
				publishTelemetryToMQTT(mqttClient, ctrl, telemetry, engine, logger)
			}

		case <-sysmgmtTicker.C:
			// Run system management checks
			if sysmgmtManager != nil {
				if err := sysmgmtManager.RunHealthCheck(ctx); err != nil {
					logger.Error("System management check failed", "error", err)
				}
			}

		case <-meteredTicker.C:
			// Process metered mode operations
			if meteredManager != nil {
				// Process any pending mode changes after stability delay
				if err := meteredManager.ProcessPendingChanges(); err != nil {
					logger.Error("Failed to process pending metered mode changes", "error", err)
				}

				// Monitor data usage for current member
				if currentMember, err := ctrl.GetCurrentMember(); err == nil && currentMember != nil {
					dataMonitor := metered.NewDataUsageMonitor(meteredManager)
					if err := dataMonitor.MonitorDataUsage(currentMember); err != nil {
						logger.Debug("Data usage monitoring failed", "member", currentMember.Name, "error", err)
					}
				}
			}

		case <-gpsTicker.C:
			// Collect GPS data using comprehensive GPS collector
			if gpsCollector != nil {
				gpsCtx, gpsCancel := context.WithTimeout(ctx, 30*time.Second)
				if gpsData, err := gpsCollector.CollectBestGPS(gpsCtx); err == nil {
					logger.Info("GPS data collected",
						"lat", gpsData.Latitude,
						"lon", gpsData.Longitude,
						"altitude", gpsData.Altitude,
						"accuracy", gpsData.Accuracy,
						"speed", gpsData.Speed,
						"satellites", gpsData.Satellites,
						"fix_type", gpsData.FixType,
						"fix_quality", gpsData.FixQuality,
						"source", gpsData.Source,
						"method", gpsData.Method,
						"confidence", gpsData.Confidence,
						"collection_time", gpsData.CollectionTime,
						"data_sources", gpsData.DataSources)
				} else {
					logger.Debug("GPS collection failed", "error", err)
				}
				gpsCancel()
			}

		case <-gpsReevalTicker.C:
			// Re-evaluate GPS source availability
			if gpsCollector != nil {
				reevalCtx, reevalCancel := context.WithTimeout(ctx, 30*time.Second)
				gpsCollector.ReEvaluateSourceAvailability(reevalCtx)
				reevalCancel()
			}
		}
	}
}

// publishTelemetryToMQTT publishes comprehensive telemetry data to MQTT
func publishTelemetryToMQTT(mqttClient *mqtt.Client, ctrl *controller.Controller, telemetry *telem.Store, engine *decision.Engine, logger *logx.Logger) {
	// Get current system status
	members := ctrl.GetMembers()
	currentMember, _ := ctrl.GetCurrentMember()

	// Create status payload
	status := map[string]interface{}{
		"timestamp":      time.Now().Unix(),
		"current_member": "",
		"total_members":  len(members),
		"active_members": 0,
		"daemon_uptime":  time.Since(time.Now().Add(-24 * time.Hour)).Seconds(), // Approximate
	}

	if currentMember != nil {
		status["current_member"] = currentMember.Name
	}

	// Count active members
	activeCount := 0
	for _, member := range members {
		if member.Eligible {
			activeCount++
		}
	}
	status["active_members"] = activeCount

	// Publish system status
	if err := mqttClient.PublishStatus(status); err != nil {
		logger.Warn("Failed to publish status to MQTT", "error", err)
	}

	// Publish member list
	memberData := make([]map[string]interface{}, len(members))
	for i, member := range members {
		memberData[i] = map[string]interface{}{
			"name":      member.Name,
			"class":     member.Class,
			"interface": member.Iface,
			"weight":    member.Weight,
			"eligible":  member.Eligible,
			"active":    currentMember != nil && currentMember.Name == member.Name,
		}
	}

	if err := mqttClient.PublishMemberList(memberData); err != nil {
		logger.Warn("Failed to publish member list to MQTT", "error", err)
	}

	// Publish recent samples for each member
	for _, member := range members {
		samples, err := telemetry.GetSamples(member.Name, time.Now().Add(-5*time.Minute))
		if err != nil || len(samples) == 0 {
			continue
		}

		// Get the latest sample
		latestSample := samples[len(samples)-1]
		sampleData := map[string]interface{}{
			"member":     member.Name,
			"timestamp":  latestSample.Timestamp.Unix(),
			"latency_ms": latestSample.Metrics.LatencyMS,
			"loss_pct":   latestSample.Metrics.LossPercent,
			"score":      latestSample.Score.Final,
		}

		// Add class-specific metrics
		if latestSample.Metrics.ObstructionPct != nil {
			sampleData["obstruction_pct"] = *latestSample.Metrics.ObstructionPct
		}
		if latestSample.Metrics.SignalStrength != nil {
			sampleData["signal_strength"] = *latestSample.Metrics.SignalStrength
		}

		if err := mqttClient.PublishSample(sampleData); err != nil {
			logger.Warn("Failed to publish sample to MQTT", "member", member.Name, "error", err)
		}
	}

	// Publish recent events
	events, err := telemetry.GetEvents(time.Now().Add(-10*time.Minute), 10)
	if err == nil && len(events) > 0 {
		for _, event := range events {
			eventData := map[string]interface{}{
				"timestamp": event.Timestamp.Unix(),
				"type":      event.Type,
				"reason":    event.Reason,
				"member":    event.Member,
				"from":      event.From,
				"to":        event.To,
				"data":      event.Data,
			}

			if err := mqttClient.PublishEvent(eventData); err != nil {
				logger.Warn("Failed to publish event to MQTT", "event_type", event.Type, "error", err)
			}
		}
	}

	// Publish health information
	healthData := map[string]interface{}{
		"timestamp":       time.Now().Unix(),
		"telemetry_usage": telemetry.GetMemoryUsage(),
		"components": map[string]string{
			"controller":      "healthy",
			"decision_engine": "healthy",
			"telemetry_store": "healthy",
		},
	}

	if err := mqttClient.PublishHealth(healthData); err != nil {
		logger.Warn("Failed to publish health to MQTT", "error", err)
	}

	logger.Debug("Successfully published telemetry to MQTT")
}

// initializeSystemManagement initializes the system management component
func initializeSystemManagement(cfg *uci.Config, logger *logx.Logger, dryRun bool, gpsCollector *gps.ComprehensiveGPSCollector) (*sysmgmt.Manager, error) {
	// Load system management configuration - use defaults since LoadConfig doesn't exist
	sysmgmtConfig := sysmgmt.DefaultConfig()

	// Copy WiFi optimization settings from main UCI config to sysmgmt config
	sysmgmtConfig.WiFiOptimizationEnabled = cfg.WiFiOptimizationEnabled
	sysmgmtConfig.WiFiMovementThreshold = cfg.WiFiMovementThreshold
	sysmgmtConfig.WiFiStationaryTime = cfg.WiFiStationaryTime
	sysmgmtConfig.WiFiNightlyOptimization = cfg.WiFiNightlyOptimization
	sysmgmtConfig.WiFiNightlyTime = cfg.WiFiNightlyTime
	sysmgmtConfig.WiFiNightlyWindow = cfg.WiFiNightlyWindow
	sysmgmtConfig.WiFiWeeklyOptimization = cfg.WiFiWeeklyOptimization
	sysmgmtConfig.WiFiWeeklyDays = cfg.WiFiWeeklyDays
	sysmgmtConfig.WiFiWeeklyTime = cfg.WiFiWeeklyTime
	sysmgmtConfig.WiFiWeeklyWindow = cfg.WiFiWeeklyWindow
	sysmgmtConfig.WiFiMinImprovement = cfg.WiFiMinImprovement
	sysmgmtConfig.WiFiDwellTime = cfg.WiFiDwellTime
	sysmgmtConfig.WiFiNoiseDefault = cfg.WiFiNoiseDefault
	sysmgmtConfig.WiFiVHT80Threshold = cfg.WiFiVHT80Threshold
	sysmgmtConfig.WiFiVHT40Threshold = cfg.WiFiVHT40Threshold
	sysmgmtConfig.WiFiUseDFS = cfg.WiFiUseDFS
	sysmgmtConfig.WiFiOptimizationCooldown = cfg.WiFiOptimizationCooldown
	sysmgmtConfig.WiFiGPSAccuracyThreshold = cfg.WiFiGPSAccuracyThreshold
	sysmgmtConfig.WiFiLocationLogging = cfg.WiFiLocationLogging
	sysmgmtConfig.WiFiSchedulerCheckInterval = cfg.WiFiSchedulerCheckInterval
	sysmgmtConfig.WiFiSkipIfRecent = cfg.WiFiSkipIfRecent
	sysmgmtConfig.WiFiRecentThreshold = cfg.WiFiRecentThreshold
	sysmgmtConfig.WiFiTimezone = cfg.WiFiTimezone

	// Create system management manager with comprehensive GPS collector
	var manager *sysmgmt.Manager
	if gpsCollector != nil {
		// Use comprehensive GPS collector for GPS functionality
		manager = sysmgmt.NewManager(sysmgmtConfig, logger, dryRun)
		logger.Info("System management initialized with comprehensive GPS collector")
	} else {
		// Fallback to basic manager
		manager = sysmgmt.NewManager(sysmgmtConfig, logger, dryRun)
		logger.Info("System management initialized without GPS functionality")
	}

	// Start system management if enabled
	if sysmgmtConfig.Enabled {
		if err := manager.Start(); err != nil {
			return nil, fmt.Errorf("failed to start system management: %w", err)
		}
		logger.Info("System management started",
			"check_interval", sysmgmtConfig.CheckInterval,
			"auto_fix", sysmgmtConfig.AutoFixEnabled,
			"dry_run", dryRun)
	} else {
		logger.Info("System management disabled in configuration")
	}

	return manager, nil
}

// runHealthCheckCommand runs specific health checks on demand
func runHealthCheckCommand(logger *logx.Logger, cfg *uci.Config, healthCheckType string, runAll bool, verbose bool, dryRun bool) {
	logger.Info("Running health check command", "type", healthCheckType, "run_all", runAll, "verbose", verbose, "dry_run", dryRun)

	// Set verbose logging if requested
	if verbose {
		logger.SetLevel("trace")
	}

	// Create system management configuration using defaults
	sysmgmtConfig := sysmgmt.DefaultConfig()

	// Override with command-specific settings
	sysmgmtConfig.AutoFixEnabled = !dryRun // Disable auto-fix in dry-run mode
	sysmgmtConfig.NotificationsEnabled = cfg.PushoverEnabled
	sysmgmtConfig.PushoverToken = cfg.PushoverToken
	sysmgmtConfig.PushoverUser = cfg.PushoverUser

	// Create system management manager
	manager := sysmgmt.NewManager(sysmgmtConfig, logger, dryRun)

	ctx, cancel := context.WithTimeout(context.Background(), time.Minute*2)
	defer cancel()

	if runAll || healthCheckType == "all" {
		logger.Info("Running all health checks")
		if err := manager.RunHealthCheck(ctx); err != nil {
			logger.Error("Health check failed", "error", err)
			os.Exit(1)
		}
		// Note: The manager.RunHealthCheck method now provides its own completion message
		// that accurately reflects whether issues were found and fixed
		return
	}

	// Run specific health check
	var err error
	switch healthCheckType {
	case "starlink":
		logger.Info("Running Starlink health check")
		err = runStarlinkHealthCheck(ctx, manager, logger)
	case "uci":
		logger.Info("Running UCI configuration health check")
		err = runUCIHealthCheck(ctx, manager, logger)
	case "overlay":
		logger.Info("Running overlay space health check")
		err = runOverlayHealthCheck(ctx, manager, logger)
	case "service":
		logger.Info("Running service watchdog health check")
		err = runServiceHealthCheck(ctx, manager, logger)
	case "network":
		logger.Info("Running network interface health check")
		err = runNetworkHealthCheck(ctx, manager, logger)
	case "database":
		logger.Info("Running database health check")
		err = runDatabaseHealthCheck(ctx, manager, logger)
	case "time":
		logger.Info("Running time drift health check")
		err = runTimeHealthCheck(ctx, manager, logger)
	case "log":
		logger.Info("Running log flood detection health check")
		err = runLogHealthCheck(ctx, manager, logger)
	default:
		logger.Error("Unknown health check type", "type", healthCheckType)
		fmt.Printf("Available health checks: starlink, uci, overlay, service, network, database, time, log, all\n")
		os.Exit(1)
	}

	if err != nil {
		logger.Error("Health check failed", "type", healthCheckType, "error", err)
		os.Exit(1)
	}

	logger.Info("Health check completed successfully", "type", healthCheckType)
}

// runStarlinkHealthCheck runs only the Starlink health check
func runStarlinkHealthCheck(ctx context.Context, manager *sysmgmt.Manager, logger *logx.Logger) error {
	logger.Info("🛰️ Starting comprehensive Starlink health monitoring")

	// Access the Starlink health manager through reflection or create a new one
	// For now, we'll create a temporary one with the same config
	sysmgmtConfig := &sysmgmt.Config{
		StarlinkScriptEnabled: true,
		StarlinkLogTimeout:    time.Minute * 10,
		NotificationsEnabled:  false, // Disable notifications for on-demand checks
	}

	starlinkMgr := sysmgmt.NewStarlinkHealthManager(sysmgmtConfig, logger, false)

	startTime := time.Now()
	err := starlinkMgr.Check(ctx)
	duration := time.Since(startTime)

	if err != nil {
		logger.Error("❌ Starlink health check failed", "error", err, "duration", duration)
		return err
	}

	logger.Info("✅ Starlink health check completed", "duration", duration)
	return nil
}

// runUCIHealthCheck runs only the UCI configuration health check
func runUCIHealthCheck(ctx context.Context, manager *sysmgmt.Manager, logger *logx.Logger) error {
	logger.Info("🔧 Starting UCI configuration health check")

	uciMgr := sysmgmt.NewUCIMaintenanceManager(logger)

	startTime := time.Now()

	// Always run UCI maintenance to check for unwanted files and other issues
	result, err := uciMgr.PerformUCIMaintenance()
	if err != nil {
		return fmt.Errorf("UCI maintenance failed: %w", err)
	}

	// Log results
	if len(result.IssuesFound) > 0 {
		logger.Info("UCI maintenance completed with issues",
			"issues_found", len(result.IssuesFound),
			"issues_fixed", len(result.IssuesFixed),
			"backup_created", result.BackupCreated,
			"success", result.Success)

		// Log details of found issues
		for _, issue := range result.IssuesFound {
			logger.Info("UCI issue detected",
				"type", issue.Type,
				"section", issue.Section,
				"description", issue.Description,
				"severity", issue.Severity)
		}
	} else {
		logger.Info("UCI maintenance completed - no issues found",
			"backup_created", result.BackupCreated,
			"success", result.Success)
	}

	duration := time.Since(startTime)
	logger.Info("✅ UCI health check completed", "duration", duration)
	return nil
}

// Placeholder functions for other health checks
func runOverlayHealthCheck(ctx context.Context, manager *sysmgmt.Manager, logger *logx.Logger) error {
	logger.Info("💾 Overlay space health check - creating temporary manager")
	sysmgmtConfig := &sysmgmt.Config{OverlaySpaceEnabled: true, OverlaySpaceThreshold: 80}
	overlayMgr := sysmgmt.NewOverlayManager(sysmgmtConfig, logger, false)
	return overlayMgr.Check(ctx)
}

func runServiceHealthCheck(ctx context.Context, manager *sysmgmt.Manager, logger *logx.Logger) error {
	logger.Info("🔄 Service watchdog health check - creating temporary manager")
	sysmgmtConfig := &sysmgmt.Config{ServiceWatchdogEnabled: true}
	serviceMgr := sysmgmt.NewServiceWatchdog(sysmgmtConfig, logger, false)
	return serviceMgr.Check(ctx)
}

func runNetworkHealthCheck(ctx context.Context, manager *sysmgmt.Manager, logger *logx.Logger) error {
	logger.Info("🌐 Network interface health check - creating temporary manager")
	sysmgmtConfig := &sysmgmt.Config{InterfaceFlappingEnabled: true, FlappingThreshold: 10}
	networkMgr := sysmgmt.NewNetworkManager(sysmgmtConfig, logger, false)
	return networkMgr.Check(ctx)
}

func runDatabaseHealthCheck(ctx context.Context, manager *sysmgmt.Manager, logger *logx.Logger) error {
	logger.Info("🗄️ Database health check - creating temporary manager")
	sysmgmtConfig := &sysmgmt.Config{DatabaseEnabled: true, DatabaseErrorThreshold: 5}
	dbMgr := sysmgmt.NewDatabaseManager(sysmgmtConfig, logger, false)
	return dbMgr.Check(ctx)
}

func runTimeHealthCheck(ctx context.Context, manager *sysmgmt.Manager, logger *logx.Logger) error {
	logger.Info("⏰ Time drift health check - creating temporary manager")
	sysmgmtConfig := &sysmgmt.Config{TimeDriftEnabled: true, TimeDriftThreshold: time.Minute * 5}
	timeMgr := sysmgmt.NewTimeManager(sysmgmtConfig, logger, false)
	return timeMgr.Check(ctx)
}

func runLogHealthCheck(ctx context.Context, manager *sysmgmt.Manager, logger *logx.Logger) error {
	logger.Info("📝 Log flood detection health check - creating temporary manager")
	sysmgmtConfig := &sysmgmt.Config{LogFloodEnabled: true, LogFloodThreshold: 100}
	logMgr := sysmgmt.NewLogFloodDetector(sysmgmtConfig, logger, false)
	return logMgr.Check(ctx)
}

// writeHeartbeat writes heartbeat data to /tmp/autonomyd.health every 10 seconds
func writeHeartbeat(ctx context.Context, ticker *time.Ticker, startTime time.Time, logger *logx.Logger, engine *decision.Engine) {
	const heartbeatFile = "/tmp/autonomyd.health"

	for {
		select {
		case <-ctx.Done():
			logger.Info("Heartbeat writer stopped")
			return
		case <-ticker.C:
			// Get current memory stats
			var memStats runtime.MemStats
			runtime.ReadMemStats(&memStats)

			// Get last failover timestamp from decision engine
			lastFailoverTS := ""
			if engine != nil {
				if lastFailover := engine.GetLastFailoverTime(); !lastFailover.IsZero() {
					lastFailoverTS = lastFailover.Format(time.RFC3339)
				}
			}

			// Create heartbeat data
			heartbeat := HeartbeatData{
				Timestamp:      time.Now().Format(time.RFC3339),
				UptimeS:        int64(time.Since(startTime).Seconds()),
				Version:        AppVersion,
				Status:         "ok", // TODO: Get actual status from decision engine
				LastFailoverTS: lastFailoverTS,
				MemMB:          float64(memStats.Alloc) / 1024 / 1024, // Convert bytes to MB
				Goroutines:     runtime.NumGoroutine(),
				DeviceID:       getDeviceID(),
			}

			// Marshal to JSON
			data, err := json.Marshal(heartbeat)
			if err != nil {
				logger.Error("Failed to marshal heartbeat data", "error", err)
				continue
			}

			// Write to file atomically (write to temp file, then rename)
			// Use os.CreateTemp for secure temporary file creation
			tempFile, err := os.CreateTemp("/tmp", "autonomyd-heartbeat-*.tmp")
			if err != nil {
				logger.Error("Failed to create temporary file", "error", err)
				continue
			}
			defer os.Remove(tempFile.Name()) // Clean up temp file
			
			if err := os.WriteFile(tempFile.Name(), data, 0o644); err != nil {
				logger.Error("Failed to write heartbeat file", "error", err, "file", tempFile.Name())
				continue
			}

			if err := os.Rename(tempFile.Name(), heartbeatFile); err != nil {
				logger.Error("Failed to rename heartbeat file", "error", err, "from", tempFile.Name(), "to", heartbeatFile)
				// Clean up temp file
				os.Remove(tempFile.Name())
				continue
			}

			logger.Debug("Heartbeat written", "file", heartbeatFile, "uptime_s", heartbeat.UptimeS, "mem_mb", heartbeat.MemMB, "goroutines", heartbeat.Goroutines)
		}
	}
}

// getDeviceID returns a device identifier for the heartbeat
func getDeviceID() string {
	// Try to get hostname first
	if hostname, err := os.Hostname(); err == nil && hostname != "" {
		return hostname
	}

	// Fallback to a generic identifier
	return "autonomy-device"
}
