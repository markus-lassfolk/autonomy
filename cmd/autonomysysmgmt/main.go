package main

import (
	"context"
	"flag"
	"os"
	"os/signal"
	"syscall"
	"time"

	"github.com/markus-lassfolk/autonomy/pkg/logx"
	"github.com/markus-lassfolk/autonomy/pkg/sysmgmt"
)

var (
	configFile = flag.String("config", "/etc/config/autonomy", "Configuration file path")
	logLevel   = flag.String("log-level", "info", "Log level (debug|info|warn|error|trace)")
	dryRun     = flag.Bool("dry-run", false, "Dry run mode - don't make changes")
	checkOnly  = flag.Bool("check", false, "Check mode only - don't fix issues")
	interval   = flag.Duration("interval", 5*time.Minute, "Check interval when running as daemon")
	monitor    = flag.Bool("monitor", false, "Run in monitoring mode with verbose output")
	verbose    = flag.Bool("verbose", false, "Enable verbose logging (equivalent to trace level)")
	foreground = flag.Bool("foreground", false, "Run in foreground mode (don't daemonize)")
	runtime    = flag.Duration("runtime", 0, "Maximum runtime for testing (e.g., 50s, 5m)")
)

func main() {
	flag.Parse()

	// Determine log level
	effectiveLogLevel := *logLevel
	if *verbose || *monitor {
		effectiveLogLevel = "trace"
	}

	// Initialize logger
	logger := logx.NewLogger(effectiveLogLevel, "autonomysysmgmt")
	logger.Info("Starting autonomy System Management", "version", "1.0.0")

	// Log monitoring mode status
	if *monitor {
		logger.Info("Running in monitoring mode", "verbose_logging", true, "foreground", *foreground)
		logger.LogVerbose("monitoring_mode_enabled", map[string]interface{}{
			"log_level":  effectiveLogLevel,
			"dry_run":    *dryRun,
			"check_only": *checkOnly,
			"verbose":    *verbose,
		})
	}

	// Load configuration
	config, err := sysmgmt.LoadConfig(*configFile)
	if err != nil {
		logger.Error("Failed to load configuration", "error", err)
		os.Exit(1)
	}

	// Create system manager
	manager := sysmgmt.NewManager(config, logger, *dryRun)

	// Handle signals for graceful shutdown
	sigChan := make(chan os.Signal, 1)
	signal.Notify(sigChan, syscall.SIGINT, syscall.SIGTERM)

	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()

	// Run in daemon mode or single check mode
	if !*checkOnly {
		logger.Info("Running in daemon mode", "interval", *interval)

		// Set up runtime limit if specified
		if *runtime > 0 {
			logger.Info("Runtime limit set", "duration", *runtime)
			go func() {
				time.Sleep(*runtime)
				logger.Info("Runtime limit reached, shutting down")
				cancel()
			}()
		}

		// Run initial health check immediately
		logger.Info("Running initial health check")
		if err := manager.RunHealthCheck(ctx); err != nil {
			logger.Error("Initial health check failed", "error", err)
		}

		go func() {
			ticker := time.NewTicker(*interval)
			defer ticker.Stop()

			for {
				select {
				case <-ctx.Done():
					return
				case <-ticker.C:
					if err := manager.RunHealthCheck(ctx); err != nil {
						logger.Error("Health check failed", "error", err)
					}
				}
			}
		}()

		// Wait for shutdown signal or context cancellation
		select {
		case <-sigChan:
			logger.Info("Received shutdown signal")
		case <-ctx.Done():
			logger.Info("Context cancelled")
		}
		logger.Info("Shutting down system manager")
	} else {
		// Single check mode
		logger.Info("Running single health check")
		if err := manager.RunHealthCheck(ctx); err != nil {
			logger.Error("Health check failed", "error", err)
			os.Exit(1)
		}
		logger.Info("Health check completed successfully")
	}
}
