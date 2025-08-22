package main

import (
	"context"
	"flag"
	"fmt"
	"os"
	"time"

	"github.com/markus-lassfolk/autonomy/pkg"
	"github.com/markus-lassfolk/autonomy/pkg/gps"
	"github.com/markus-lassfolk/autonomy/pkg/logx"
)

var (
	verbose    = flag.Bool("verbose", false, "Enable verbose logging")
	continuous = flag.Bool("continuous", false, "Continuously collect GPS data")
	interval   = flag.Duration("interval", 10*time.Second, "Collection interval for continuous mode")
	source     = flag.String("source", "", "Specific GPS source to test (rutos|starlink|cellular)")
	timeout    = flag.Duration("timeout", 30*time.Second, "Timeout for GPS collection")
)

func main() {
	flag.Parse()

	// Initialize logger
	logLevel := "info"
	if *verbose {
		logLevel = "debug"
	}
	logger := logx.NewLogger(logLevel, "test-gps")

	fmt.Println("GPS Coordinate Testing Tool")
	fmt.Println("==========================")

	// Create GPS collector configuration
	config := gps.DefaultComprehensiveGPSConfig()
	if *source != "" {
		// Test specific source only
		config.SourcePriority = []string{*source}
		fmt.Printf("Testing specific GPS source: %s\n", *source)
	} else {
		fmt.Printf("Testing all GPS sources in priority order: %v\n", config.SourcePriority)
	}

	// Create GPS collector
	collector := gps.NewGPSCollectorWithConfig(config, logger)

	if *continuous {
		fmt.Printf("Starting continuous GPS collection (interval: %v)\n", *interval)
		fmt.Println("Press Ctrl+C to stop...")
		runContinuous(collector, logger)
	} else {
		fmt.Println("Performing single GPS collection test...")
		runSingle(collector, logger)
	}
}

func runSingle(collector gps.ComprehensiveGPSCollectorInterface, logger *logx.Logger) {
	ctx, cancel := context.WithTimeout(context.Background(), *timeout)
	defer cancel()

	fmt.Printf("Collecting GPS data (timeout: %v)...\n", *timeout)

	gpsData, err := collector.CollectGPS(ctx)
	if err != nil {
		fmt.Printf("❌ GPS collection failed: %v\n", err)

		// Try to get the best available source info
		bestSource := collector.GetBestSource()
		fmt.Printf("Best available GPS source: %s\n", bestSource)

		os.Exit(1)
	}

	displayGPSData(gpsData)

	// Show best source
	bestSource := collector.GetBestSource()
	fmt.Printf("Best available GPS source: %s\n", bestSource)
}

func runContinuous(collector gps.ComprehensiveGPSCollectorInterface, logger *logx.Logger) {
	ticker := time.NewTicker(*interval)
	defer ticker.Stop()

	count := 0
	for {
		count++
		fmt.Printf("\n--- GPS Collection #%d ---\n", count)

		ctx, cancel := context.WithTimeout(context.Background(), *timeout)

		gpsData, err := collector.CollectGPS(ctx)
		if err != nil {
			fmt.Printf("❌ GPS collection failed: %v\n", err)
		} else {
			displayGPSData(gpsData)
		}

		cancel()

		// Wait for next interval
		<-ticker.C
	}
}

func displayGPSData(gps *pkg.GPSData) {
	if gps == nil {
		fmt.Println("❌ No GPS data received")
		return
	}

	fmt.Println("✅ GPS Data Successfully Collected:")
	fmt.Printf("   📍 Coordinates: %.6f, %.6f\n", gps.Latitude, gps.Longitude)
	fmt.Printf("   🏔️  Altitude: %.1f meters\n", gps.Altitude)
	fmt.Printf("   🎯 Accuracy: %.1f meters\n", gps.Accuracy)
	fmt.Printf("   🛰️  Satellites: %d\n", gps.Satellites)
	fmt.Printf("   📡 Source: %s\n", gps.Source)
	fmt.Printf("   ✅ Valid: %t\n", gps.Valid)
	fmt.Printf("   ⏰ Timestamp: %s\n", gps.Timestamp.Format("2006-01-02 15:04:05"))

	if gps.UncertaintyMeters > 0 {
		fmt.Printf("   ❓ Uncertainty: %.1f meters\n", gps.UncertaintyMeters)
	}

	// Display Google Maps link for convenience
	if gps.Valid && gps.Latitude != 0 && gps.Longitude != 0 {
		fmt.Printf("   🗺️  Google Maps: https://maps.google.com/?q=%.6f,%.6f\n", gps.Latitude, gps.Longitude)
	}
}
