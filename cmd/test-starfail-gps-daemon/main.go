package main

import (
	"context"
	"flag"
	"fmt"
	"os"
	"os/signal"
	"syscall"
	"time"

	"github.com/markus-lassfolk/autonomy/pkg/gps"
	"github.com/markus-lassfolk/autonomy/pkg/logx"
)

var (
	logLevel          = flag.String("log-level", "info", "Log level (debug|info|warn|error|trace)")
	googleAPIKey      = flag.String("google-api-key", "", "Google Location API key (or path to key file)")
	pollInterval      = flag.Duration("poll-interval", 60*time.Second, "GPS polling interval")
	enableMovement    = flag.Bool("enable-movement", true, "Enable movement detection")
	enableCaching     = flag.Bool("enable-caching", true, "Enable location caching")
	accuracyThreshold = flag.Float64("accuracy-threshold", 100.0, "GPS accuracy threshold in meters")
	version           = flag.Bool("version", false, "Show version information")
)

const (
	AppName    = "autonomy-gps-daemon"
	AppVersion = "1.0.0"
)

func main() {
	flag.Parse()

	if *version {
		fmt.Printf("%s version %s\n", AppName, AppVersion)
		os.Exit(0)
	}

	// Initialize logger
	logger := logx.NewLogger(*logLevel, "autonomy-gps")

	logger.Info("Starting autonomy GPS Daemon",
		"version", AppVersion,
		"poll_interval", *pollInterval,
		"accuracy_threshold", *accuracyThreshold,
		"movement_detection", *enableMovement,
		"caching", *enableCaching)

	// Create GPS configuration
	gpsConfig := &gps.ComprehensiveGPSConfig{
		Enabled:                  true,
		SourcePriority:           []string{"rutos", "starlink", "google"},
		MovementThresholdM:       100.0, // 100 meters movement threshold
		AccuracyThresholdM:       *accuracyThreshold,
		StalenessThresholdS:      300, // 5 minutes staleness threshold
		CollectionTimeoutS:       30,  // 30 seconds timeout
		RetryAttempts:            3,
		RetryDelayS:              2,
		GoogleAPIEnabled:         *googleAPIKey != "",
		GoogleAPIKey:             loadAPIKey(*googleAPIKey),
		NMEADevices:              []string{"/dev/ttyUSB1", "/dev/ttyUSB2", "/dev/ttyACM0"},
		PreferHighAccuracy:       true,
		EnableMovementDetection:  *enableMovement,
		EnableLocationClustering: *enableCaching,
	}

	// Initialize comprehensive GPS collector
	gpsCollector := gps.NewComprehensiveGPSCollector(gpsConfig, logger)
	if gpsCollector == nil {
		logger.Error("Failed to initialize GPS collector")
		os.Exit(1)
	}

	logger.Info("GPS collector initialized",
		"sources", gpsConfig.SourcePriority,
		"google_api_enabled", gpsConfig.GoogleAPIEnabled,
		"movement_detection", gpsConfig.EnableMovementDetection,
		"location_clustering", gpsConfig.EnableLocationClustering)

	// Setup signal handling for graceful shutdown
	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()

	sigChan := make(chan os.Signal, 1)
	signal.Notify(sigChan, syscall.SIGINT, syscall.SIGTERM)

	// Start GPS monitoring loop
	go gpsMonitoringLoop(ctx, gpsCollector, logger, *pollInterval)

	// Start health monitoring loop
	go healthMonitoringLoop(ctx, gpsCollector, logger)

	logger.Info("autonomy GPS Daemon started successfully")
	logger.Info("Press Ctrl+C to stop...")

	// Wait for shutdown signal
	sig := <-sigChan
	logger.Info("Received shutdown signal", "signal", sig)

	// Graceful shutdown
	cancel()

	// Give goroutines time to finish
	time.Sleep(2 * time.Second)

	logger.Info("autonomy GPS Daemon stopped")
}

// gpsMonitoringLoop continuously monitors GPS data
func gpsMonitoringLoop(ctx context.Context, gpsCollector *gps.ComprehensiveGPSCollector, logger *logx.Logger, interval time.Duration) {
	ticker := time.NewTicker(interval)
	defer ticker.Stop()

	logger.Info("Starting GPS monitoring loop", "interval", interval)

	for {
		select {
		case <-ctx.Done():
			logger.Info("GPS monitoring loop stopped")
			return
		case <-ticker.C:
			collectAndLogGPSData(ctx, gpsCollector, logger)
		}
	}
}

// collectAndLogGPSData collects GPS data and logs comprehensive information
func collectAndLogGPSData(ctx context.Context, gpsCollector *gps.ComprehensiveGPSCollector, logger *logx.Logger) {
	start := time.Now()

	// Collect GPS data from best available source
	gpsData, err := gpsCollector.CollectBestGPS(ctx)
	if err != nil {
		logger.Error("GPS collection failed", "error", err, "collection_time", time.Since(start))
		return
	}

	// Log comprehensive GPS information
	logger.Info("GPS data collected",
		"latitude", fmt.Sprintf("%.6f", gpsData.Latitude),
		"longitude", fmt.Sprintf("%.6f", gpsData.Longitude),
		"altitude", fmt.Sprintf("%.1f", gpsData.Altitude),
		"accuracy", fmt.Sprintf("%.1f", gpsData.Accuracy),
		"speed", fmt.Sprintf("%.2f", gpsData.Speed),
		"course", fmt.Sprintf("%.1f", gpsData.Course),
		"satellites", gpsData.Satellites,
		"fix_type", gpsData.FixType,
		"fix_quality", gpsData.FixQuality,
		"confidence", fmt.Sprintf("%.2f", gpsData.Confidence),
		"source", gpsData.Source,
		"method", gpsData.Method,
		"valid", gpsData.Valid,
		"collection_time", gpsData.CollectionTime,
		"from_cache", gpsData.FromCache,
		"api_call_made", gpsData.APICallMade,
		"data_sources", gpsData.DataSources,
		"timestamp", gpsData.Timestamp.Format(time.RFC3339))

	// Log Google Maps link for easy visualization
	if gpsData.Valid {
		mapsURL := fmt.Sprintf("https://www.google.com/maps?q=%.6f,%.6f&z=15",
			gpsData.Latitude, gpsData.Longitude)
		logger.Info("Google Maps link", "url", mapsURL)
	}
}

// healthMonitoringLoop monitors GPS source health
func healthMonitoringLoop(ctx context.Context, gpsCollector *gps.ComprehensiveGPSCollector, logger *logx.Logger) {
	ticker := time.NewTicker(5 * time.Minute) // Check health every 5 minutes
	defer ticker.Stop()

	logger.Info("Starting GPS health monitoring loop")

	for {
		select {
		case <-ctx.Done():
			logger.Info("GPS health monitoring loop stopped")
			return
		case <-ticker.C:
			logGPSHealthStatus(gpsCollector, logger)
		}
	}
}

// logGPSHealthStatus logs comprehensive GPS source health information
func logGPSHealthStatus(gpsCollector *gps.ComprehensiveGPSCollector, logger *logx.Logger) {
	healthStatus := gpsCollector.GetSourceHealthStatus()

	logger.Info("GPS Source Health Status Report")

	for source, health := range healthStatus {
		logger.Info("GPS source status",
			"source", source,
			"available", health.Available,
			"success_rate", fmt.Sprintf("%.1f%%", health.SuccessRate*100),
			"avg_latency", fmt.Sprintf("%.1fms", health.AvgLatency),
			"success_count", health.SuccessCount,
			"error_count", health.ErrorCount,
			"last_error", health.LastError,
			"last_success", health.LastSuccess.Format(time.RFC3339))
	}

	// Get best available source
	ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancel()

	bestSource := gpsCollector.GetBestAvailableSource(ctx)
	logger.Info("Best available GPS source", "source", bestSource)
}

// loadAPIKey loads API key from file or returns the key directly
func loadAPIKey(keyOrPath string) string {
	if keyOrPath == "" {
		return ""
	}

	// If it looks like a file path, try to read it
	if len(keyOrPath) > 20 && (keyOrPath[0] == '/' || keyOrPath[1] == ':') {
		content, err := os.ReadFile(keyOrPath)
		if err != nil {
			return keyOrPath // Assume it's the key itself
		}
		return string(content)
	}

	// Otherwise, assume it's the key itself
	return keyOrPath
}
