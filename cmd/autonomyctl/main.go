package main

import (
	"context"
	"encoding/csv"
	"encoding/json"
	"flag"
	"fmt"
	"os"
	"os/exec"
	"strings"
	"time"

	"github.com/markus-lassfolk/autonomy/pkg/gps"
	"github.com/markus-lassfolk/autonomy/pkg/logx"
)

// Command line flags
var (
	// GPS Data Retrieval Commands
	getBestGPS     = flag.Bool("get-best-gps", false, "Get best available GPS data")
	getRutosGPS    = flag.Bool("get-rutos-gps", false, "Get RUTOS GPS data only")
	getStarlinkGPS = flag.Bool("get-starlink-gps", false, "Get Starlink GPS data only")
	getGoogleGPS   = flag.Bool("get-google-gps", false, "Get Google Location API data only")
	getAllGPS      = flag.Bool("get-all-gps", false, "Get all GPS sources in comparison format")

	// Output Format Options
	outputFormat = flag.String("format", "standard", "Output format: standard, json, csv, minimal")

	// Configuration Options
	googleAPIKey = flag.String("google-api-key", "", "Google Location API key (or path to key file)")
	logLevel     = flag.String("log-level", "info", "Log level (debug|info|warn|error|trace)")
	timeout      = flag.Duration("timeout", 30*time.Second, "Operation timeout")

	// Google Maps Integration
	generateMaps = flag.Bool("generate-maps", false, "Generate Google Maps links for GPS sources")
	mapZoom      = flag.Int("map-zoom", 15, "Zoom level for Google Maps links")

	// Health and Status
	healthCheck = flag.Bool("health", false, "Show GPS source health status")
	version     = flag.Bool("version", false, "Show version information")

	// Advanced GPS Testing
	testCellTower     = flag.Bool("test-cell-tower", false, "Test cell tower location services")
	testOpenCellID    = flag.Bool("test-opencellid", false, "Test OpenCellID API")
	testLocalDB       = flag.Bool("test-local-db", false, "Test local cell database")
	test5G            = flag.Bool("test-5g", false, "Test 5G network collection")
	testCellularIntel = flag.Bool("test-cellular-intel", false, "Test cellular intelligence")
	testGPSHealth     = flag.Bool("test-gps-health", false, "Test GPS health monitoring")
	testAdaptiveCache = flag.Bool("test-adaptive-cache", false, "Test adaptive location cache")

	// Cell Tower Options
	cellTowerProvider = flag.String("cell-provider", "mozilla", "Cell tower provider (mozilla|opencellid)")
	openCellIDKey     = flag.String("opencellid-key", "", "OpenCellID API key")

	// Database Options
	dbPath = flag.String("db-path", "/tmp/autonomy_test.db", "Local database path for testing")

	// Debug Options
	debugCellular = flag.Bool("debug-cellular", false, "Enable cellular debug output")
	debug5G       = flag.Bool("debug-5g", false, "Enable 5G debug output")
	debugHealth   = flag.Bool("debug-health", false, "Enable health debug output")

	// Contribution Options
	contributeData = flag.Bool("contribute", false, "Contribute data to OpenCellID")
	showStats      = flag.Bool("stats", false, "Show GPS statistics and performance data")
)

const (
	AppName    = "autonomyctl"
	AppVersion = "1.0.0"
)

func main() {
	flag.Parse()

	if *version {
		fmt.Printf("%s version %s\n", AppName, AppVersion)
		os.Exit(0)
	}

	// Initialize logger
	logger := logx.NewLogger(*logLevel, "autonomyctl")

	// Create context with timeout
	ctx, cancel := context.WithTimeout(context.Background(), *timeout)
	defer cancel()

	// Handle GPS data retrieval commands
	if *getBestGPS || *getRutosGPS || *getStarlinkGPS || *getGoogleGPS || *getAllGPS {
		if err := handleGPSDataRetrieval(ctx, logger); err != nil {
			fmt.Fprintf(os.Stderr, "Error: %v\n", err)
			os.Exit(1)
		}
		return
	}

	// Handle health check
	if *healthCheck {
		if err := handleHealthCheck(ctx, logger); err != nil {
			fmt.Fprintf(os.Stderr, "Error: %v\n", err)
			os.Exit(1)
		}
		return
	}

	// Handle advanced GPS testing commands
	if *testCellTower {
		if err := handleCellTowerTest(ctx, logger); err != nil {
			fmt.Fprintf(os.Stderr, "Error: %v\n", err)
			os.Exit(1)
		}
		return
	}

	if *testOpenCellID {
		if err := handleOpenCellIDTest(ctx, logger); err != nil {
			fmt.Fprintf(os.Stderr, "Error: %v\n", err)
			os.Exit(1)
		}
		return
	}

	if *testLocalDB {
		if err := handleLocalDBTest(ctx, logger); err != nil {
			fmt.Fprintf(os.Stderr, "Error: %v\n", err)
			os.Exit(1)
		}
		return
	}

	if *test5G {
		if err := handle5GTest(ctx, logger); err != nil {
			fmt.Fprintf(os.Stderr, "Error: %v\n", err)
			os.Exit(1)
		}
		return
	}

	if *testCellularIntel {
		if err := handleCellularIntelTest(ctx, logger); err != nil {
			fmt.Fprintf(os.Stderr, "Error: %v\n", err)
			os.Exit(1)
		}
		return
	}

	if *testGPSHealth {
		if err := handleGPSHealthTest(ctx, logger); err != nil {
			fmt.Fprintf(os.Stderr, "Error: %v\n", err)
			os.Exit(1)
		}
		return
	}

	if *testAdaptiveCache {
		if err := handleAdaptiveCacheTest(ctx, logger); err != nil {
			fmt.Fprintf(os.Stderr, "Error: %v\n", err)
			os.Exit(1)
		}
		return
	}

	if *showStats {
		if err := handleStatsDisplay(ctx, logger); err != nil {
			fmt.Fprintf(os.Stderr, "Error: %v\n", err)
			os.Exit(1)
		}
		return
	}

	if *contributeData {
		if err := handleDataContribution(ctx, logger); err != nil {
			fmt.Fprintf(os.Stderr, "Error: %v\n", err)
			os.Exit(1)
		}
		return
	}

	// If no specific command, show usage
	showUsage()
}

// handleGPSDataRetrieval handles GPS data retrieval commands
func handleGPSDataRetrieval(ctx context.Context, logger *logx.Logger) error {
	// Initialize GPS collector with configuration
	config := gps.DefaultComprehensiveGPSConfig()

	// Configure Google API if key provided
	if *googleAPIKey != "" {
		apiKey, err := loadAPIKey(*googleAPIKey)
		if err != nil {
			return fmt.Errorf("failed to load Google API key: %w", err)
		}
		config.GoogleAPIEnabled = true
		config.GoogleAPIKey = apiKey
	}

	collector := gps.NewComprehensiveGPSCollector(config, logger)

	// Handle different GPS retrieval commands
	switch {
	case *getBestGPS:
		return handleBestGPS(ctx, collector)
	case *getRutosGPS:
		return handleSpecificSource(ctx, collector, "rutos")
	case *getStarlinkGPS:
		return handleSpecificSource(ctx, collector, "starlink")
	case *getGoogleGPS:
		return handleSpecificSource(ctx, collector, "google")
	case *getAllGPS:
		return handleAllGPS(ctx, collector)
	}

	return nil
}

// handleBestGPS gets the best available GPS data
func handleBestGPS(ctx context.Context, collector *gps.ComprehensiveGPSCollector) error {
	gpsData, err := collector.CollectBestGPS(ctx)
	if err != nil {
		return fmt.Errorf("failed to collect best GPS data: %w", err)
	}

	return outputGPSData(gpsData, "Best GPS")
}

// handleSpecificSource gets GPS data from a specific source
func handleSpecificSource(ctx context.Context, collector *gps.ComprehensiveGPSCollector, sourceName string) error {
	// Collect all sources and filter for the requested one
	allSources, err := collector.CollectAllSources(ctx)
	if err != nil {
		return fmt.Errorf("failed to collect GPS data: %w", err)
	}

	gpsData, exists := allSources[sourceName]
	if !exists {
		return fmt.Errorf("GPS source '%s' not available", sourceName)
	}

	return outputGPSData(gpsData, fmt.Sprintf("%s GPS", strings.Title(sourceName)))
}

// handleAllGPS gets GPS data from all available sources
func handleAllGPS(ctx context.Context, collector *gps.ComprehensiveGPSCollector) error {
	allSources, err := collector.CollectAllSources(ctx)
	if err != nil {
		return fmt.Errorf("failed to collect GPS data from all sources: %w", err)
	}

	return outputAllGPSData(allSources)
}

// handleHealthCheck shows GPS source health status
func handleHealthCheck(ctx context.Context, logger *logx.Logger) error {
	config := gps.DefaultComprehensiveGPSConfig()
	if *googleAPIKey != "" {
		apiKey, err := loadAPIKey(*googleAPIKey)
		if err != nil {
			return fmt.Errorf("failed to load Google API key: %w", err)
		}
		config.GoogleAPIEnabled = true
		config.GoogleAPIKey = apiKey
	}

	collector := gps.NewComprehensiveGPSCollector(config, logger)
	healthStatus := collector.GetSourceHealthStatus()

	switch *outputFormat {
	case "json":
		return json.NewEncoder(os.Stdout).Encode(healthStatus)
	default:
		fmt.Println("GPS Source Health Status:")
		fmt.Println("========================")
		for source, health := range healthStatus {
			fmt.Printf("\n%s:\n", strings.Title(source))
			fmt.Printf("  Available: %t\n", health.Available)
			fmt.Printf("  Success Rate: %.1f%%\n", health.SuccessRate*100)
			fmt.Printf("  Avg Latency: %.1fms\n", health.AvgLatency)
			fmt.Printf("  Success Count: %d\n", health.SuccessCount)
			fmt.Printf("  Error Count: %d\n", health.ErrorCount)
			if health.LastError != "" {
				fmt.Printf("  Last Error: %s\n", health.LastError)
			}
			if !health.LastSuccess.IsZero() {
				fmt.Printf("  Last Success: %s\n", health.LastSuccess.Format(time.RFC3339))
			}
		}
	}

	return nil
}

// outputGPSData outputs GPS data in the specified format
func outputGPSData(gpsData *gps.StandardizedGPSData, title string) error {
	switch *outputFormat {
	case "json":
		return outputJSON(gpsData)
	case "csv":
		return outputCSV([]*gps.StandardizedGPSData{gpsData})
	case "minimal":
		return outputMinimal(gpsData)
	default:
		return outputStandard(gpsData, title)
	}
}

// outputAllGPSData outputs all GPS sources data
func outputAllGPSData(allSources map[string]*gps.StandardizedGPSData) error {
	switch *outputFormat {
	case "json":
		return json.NewEncoder(os.Stdout).Encode(allSources)
	case "csv":
		var dataList []*gps.StandardizedGPSData
		for _, data := range allSources {
			dataList = append(dataList, data)
		}
		return outputCSV(dataList)
	default:
		fmt.Println("GPS Source Comparison:")
		fmt.Println("=====================")

		for source, data := range allSources {
			fmt.Printf("\n%s:\n", strings.Title(source))
			if err := outputStandard(data, ""); err != nil {
				return err
			}
		}

		// Generate Google Maps links if requested
		if *generateMaps {
			fmt.Println("\nGoogle Maps Links:")
			fmt.Println("==================")
			generateGoogleMapsLinks(allSources)
		}
	}

	return nil
}

// outputJSON outputs GPS data in JSON format
func outputJSON(gpsData *gps.StandardizedGPSData) error {
	encoder := json.NewEncoder(os.Stdout)
	encoder.SetIndent("", "  ")
	return encoder.Encode(gpsData)
}

// outputCSV outputs GPS data in CSV format
func outputCSV(dataList []*gps.StandardizedGPSData) error {
	writer := csv.NewWriter(os.Stdout)
	defer writer.Flush()

	// Write header
	header := []string{
		"Source", "Latitude", "Longitude", "Altitude", "Accuracy",
		"Speed", "Course", "Satellites", "FixType", "FixQuality",
		"Confidence", "Timestamp", "Valid",
	}
	if err := writer.Write(header); err != nil {
		return err
	}

	// Write data rows
	for _, data := range dataList {
		row := []string{
			data.Source,
			fmt.Sprintf("%.6f", data.Latitude),
			fmt.Sprintf("%.6f", data.Longitude),
			fmt.Sprintf("%.1f", data.Altitude),
			fmt.Sprintf("%.1f", data.Accuracy),
			fmt.Sprintf("%.2f", data.Speed),
			fmt.Sprintf("%.1f", data.Course),
			fmt.Sprintf("%d", data.Satellites),
			fmt.Sprintf("%d", data.FixType),
			data.FixQuality,
			fmt.Sprintf("%.2f", data.Confidence),
			data.Timestamp.Format(time.RFC3339),
			fmt.Sprintf("%t", data.Valid),
		}
		if err := writer.Write(row); err != nil {
			return err
		}
	}

	return nil
}

// outputMinimal outputs GPS data in minimal format (coordinates only)
func outputMinimal(gpsData *gps.StandardizedGPSData) error {
	fmt.Printf("%.6f,%.6f\n", gpsData.Latitude, gpsData.Longitude)
	return nil
}

// outputStandard outputs GPS data in human-readable format
func outputStandard(gpsData *gps.StandardizedGPSData, title string) error {
	if title != "" {
		fmt.Printf("%s:\n", title)
	}

	fmt.Printf("  Location: %.6f°, %.6f°\n", gpsData.Latitude, gpsData.Longitude)
	fmt.Printf("  Altitude: %.1f m\n", gpsData.Altitude)
	fmt.Printf("  Accuracy: %.1f m\n", gpsData.Accuracy)
	fmt.Printf("  Speed: %.2f m/s\n", gpsData.Speed)
	fmt.Printf("  Course: %.1f°\n", gpsData.Course)
	fmt.Printf("  Satellites: %d\n", gpsData.Satellites)
	fmt.Printf("  Fix Type: %d (%s)\n", gpsData.FixType, gpsData.FixQuality)
	fmt.Printf("  Confidence: %.2f\n", gpsData.Confidence)
	fmt.Printf("  Source: %s\n", gpsData.Source)
	fmt.Printf("  Method: %s\n", gpsData.Method)
	fmt.Printf("  Valid: %t\n", gpsData.Valid)
	fmt.Printf("  Timestamp: %s\n", gpsData.Timestamp.Format(time.RFC3339))

	if gpsData.CollectionTime > 0 {
		fmt.Printf("  Collection Time: %v\n", gpsData.CollectionTime)
	}

	if len(gpsData.DataSources) > 0 {
		fmt.Printf("  Data Sources: %s\n", strings.Join(gpsData.DataSources, ", "))
	}

	return nil
}

// generateGoogleMapsLinks generates Google Maps links for GPS sources
func generateGoogleMapsLinks(allSources map[string]*gps.StandardizedGPSData) {
	for source, data := range allSources {
		if !data.Valid {
			continue
		}

		// Basic map link
		mapURL := fmt.Sprintf("https://www.google.com/maps?q=%.6f,%.6f&z=%d",
			data.Latitude, data.Longitude, *mapZoom)
		fmt.Printf("  %s: %s\n", source, mapURL)

		// Accuracy circle link
		if data.Accuracy > 0 {
			circleURL := createAccuracyCircleURL(data.Latitude, data.Longitude, data.Accuracy)
			fmt.Printf("  %s (with accuracy): %s\n", source, circleURL)
		}
	}

	// Comparison map with all sources
	if len(allSources) > 1 {
		comparisonURL := createComparisonMapURL(allSources)
		fmt.Printf("  All Sources Comparison: %s\n", comparisonURL)
	}
}

// createAccuracyCircleURL creates a Google Maps URL with accuracy circle
func createAccuracyCircleURL(lat, lng, accuracy float64) string {
	// Calculate zoom level based on accuracy
	zoom := calculateZoomLevel(accuracy)

	// Create URL with marker and approximate accuracy representation
	return fmt.Sprintf("https://www.google.com/maps/@%.6f,%.6f,%dz/data=!3m1!1e3",
		lat, lng, zoom)
}

// createComparisonMapURL creates a Google Maps URL showing all GPS sources
func createComparisonMapURL(allSources map[string]*gps.StandardizedGPSData) string {
	var coords []string
	for _, data := range allSources {
		if data.Valid {
			coords = append(coords, fmt.Sprintf("%.6f,%.6f", data.Latitude, data.Longitude))
		}
	}

	if len(coords) == 0 {
		return ""
	}

	// Create URL with multiple markers
	baseURL := "https://www.google.com/maps/dir/"
	return baseURL + strings.Join(coords, "/")
}

// calculateZoomLevel calculates appropriate zoom level based on accuracy
func calculateZoomLevel(accuracyMeters float64) int {
	if accuracyMeters <= 5 {
		return 20 // Very high accuracy
	} else if accuracyMeters <= 15 {
		return 18 // High accuracy
	} else if accuracyMeters <= 50 {
		return 16 // Medium accuracy
	} else if accuracyMeters <= 200 {
		return 14 // Low accuracy
	}
	return 12 // Very low accuracy
}

// loadAPIKey loads API key from file or returns the key directly
func loadAPIKey(keyOrPath string) (string, error) {
	// If it looks like a file path, try to read it
	if strings.Contains(keyOrPath, "/") || strings.Contains(keyOrPath, "\\") {
		content, err := os.ReadFile(keyOrPath)
		if err != nil {
			return "", fmt.Errorf("failed to read API key file: %w", err)
		}
		return strings.TrimSpace(string(content)), nil
	}

	// Otherwise, assume it's the key itself
	return keyOrPath, nil
}

// isRunningOnRUTOS detects if the binary is running on a RUTOS device
func isRunningOnRUTOS() bool {
	// Check for RUTOS-specific commands and files
	checks := [][]string{
		{"which", "gpsctl"},
		{"which", "gsmctl"},
		{"which", "ubus"},
		{"test", "-f", "/etc/config/system"},
	}

	for _, check := range checks {
		cmd := exec.Command(check[0], check[1:]...)
		if cmd.Run() == nil {
			return true
		}
	}

	return false
}

// showUsage displays usage information
func showUsage() {
	fmt.Printf("%s - autonomy Control Tool\n", AppName)
	fmt.Printf("Version: %s\n\n", AppVersion)

	fmt.Println("GPS Data Retrieval Commands:")
	fmt.Println("  -get-best-gps      Get best available GPS data")
	fmt.Println("  -get-rutos-gps     Get RUTOS GPS data only")
	fmt.Println("  -get-starlink-gps  Get Starlink GPS data only")
	fmt.Println("  -get-google-gps    Get Google Location API data only")
	fmt.Println("  -get-all-gps       Get all GPS sources in comparison format")
	fmt.Println()

	fmt.Println("Advanced GPS Testing:")
	fmt.Println("  -test-cell-tower     Test cell tower location services")
	fmt.Println("  -test-opencellid     Test OpenCellID API")
	fmt.Println("  -test-local-db       Test local cell database")
	fmt.Println("  -test-5g             Test 5G network collection")
	fmt.Println("  -test-cellular-intel Test cellular intelligence")
	fmt.Println("  -test-gps-health     Test GPS health monitoring")
	fmt.Println("  -test-adaptive-cache Test adaptive location cache")
	fmt.Println()

	fmt.Println("Output Format Options:")
	fmt.Println("  -format string     Output format: standard, json, csv, minimal (default \"standard\")")
	fmt.Println()

	fmt.Println("Configuration Options:")
	fmt.Println("  -google-api-key      Google Location API key (or path to key file)")
	fmt.Println("  -opencellid-key      OpenCellID API key")
	fmt.Println("  -cell-provider       Cell tower provider (mozilla|opencellid) (default \"mozilla\")")
	fmt.Println("  -db-path             Local database path for testing (default \"/tmp/autonomy_test.db\")")
	fmt.Println("  -log-level           Log level: debug, info, warn, error, trace (default \"info\")")
	fmt.Println("  -timeout             Operation timeout (default 30s)")
	fmt.Println()

	fmt.Println("Debug Options:")
	fmt.Println("  -debug-cellular      Enable cellular debug output")
	fmt.Println("  -debug-5g           Enable 5G debug output")
	fmt.Println("  -debug-health       Enable health debug output")
	fmt.Println()

	fmt.Println("Data Management:")
	fmt.Println("  -contribute         Contribute data to OpenCellID")
	fmt.Println("  -stats              Show GPS statistics and performance data")
	fmt.Println()

	fmt.Println("Google Maps Integration:")
	fmt.Println("  -generate-maps     Generate Google Maps links for GPS sources")
	fmt.Println("  -map-zoom int      Zoom level for Google Maps links (default 15)")
	fmt.Println()

	fmt.Println("Health and Status:")
	fmt.Println("  -health           Show GPS source health status")
	fmt.Println("  -version          Show version information")
	fmt.Println()

	fmt.Println("Examples:")
	fmt.Println("  autonomyctl -get-best-gps")
	fmt.Println("  autonomyctl -get-all-gps -format json")
	fmt.Println("  autonomyctl -test-cell-tower -cell-provider mozilla")
	fmt.Println("  autonomyctl -test-opencellid -opencellid-key YOUR_KEY")
	fmt.Println("  autonomyctl -test-5g -debug-5g")
	fmt.Println("  autonomyctl -contribute -opencellid-key YOUR_KEY")
	fmt.Println("  autonomyctl -health")
}

// Advanced GPS Testing Handler Stubs
// These would be fully implemented with the actual GPS functionality

func handleCellTowerTest(ctx context.Context, logger *logx.Logger) error {
	fmt.Println("🗼 Cell Tower Location Test")
	fmt.Println("This feature requires the full GPS system integration.")
	fmt.Println("Use the main autonomy daemon for complete functionality.")
	return nil
}

func handleOpenCellIDTest(ctx context.Context, logger *logx.Logger) error {
	fmt.Println("📡 OpenCellID API Test")
	fmt.Println("This feature requires the full GPS system integration.")
	fmt.Println("Use the main autonomy daemon for complete functionality.")
	return nil
}

func handleLocalDBTest(ctx context.Context, logger *logx.Logger) error {
	fmt.Println("💾 Local Cell Database Test")
	fmt.Println("This feature requires the full GPS system integration.")
	fmt.Println("Use the main autonomy daemon for complete functionality.")
	return nil
}

func handle5GTest(ctx context.Context, logger *logx.Logger) error {
	fmt.Println("📶 5G Network Collection Test")
	fmt.Println("This feature requires the full GPS system integration.")
	fmt.Println("Use the main autonomy daemon for complete functionality.")
	return nil
}

func handleCellularIntelTest(ctx context.Context, logger *logx.Logger) error {
	fmt.Println("🧠 Cellular Intelligence Test")
	fmt.Println("This feature requires the full GPS system integration.")
	fmt.Println("Use the main autonomy daemon for complete functionality.")
	return nil
}

func handleGPSHealthTest(ctx context.Context, logger *logx.Logger) error {
	fmt.Println("🏥 GPS Health Monitoring Test")
	fmt.Println("This feature requires the full GPS system integration.")
	fmt.Println("Use the main autonomy daemon for complete functionality.")
	return nil
}

func handleAdaptiveCacheTest(ctx context.Context, logger *logx.Logger) error {
	fmt.Println("🧠 Adaptive Location Cache Test")
	fmt.Println("This feature requires the full GPS system integration.")
	fmt.Println("Use the main autonomy daemon for complete functionality.")
	return nil
}

func handleStatsDisplay(ctx context.Context, logger *logx.Logger) error {
	fmt.Println("📊 GPS Statistics Display")
	fmt.Println("This feature requires connection to the running autonomy daemon.")
	fmt.Println("Ensure autonomyd is running to get live statistics.")
	return nil
}

func handleDataContribution(ctx context.Context, logger *logx.Logger) error {
	fmt.Println("🤝 Data Contribution to OpenCellID")
	fmt.Println("This feature requires the full GPS system integration.")
	fmt.Println("Use the main autonomy daemon for complete functionality.")
	return nil
}
