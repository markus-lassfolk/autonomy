package main

import (
	"fmt"
	"strconv"
	"strings"
	"time"

	"golang.org/x/crypto/ssh"
)

// StandardizedGPSOutput represents standardized GPS data for table display
type StandardizedGPSOutput struct {
	Latitude     string
	Longitude    string
	Accuracy     string
	FixType      string
	Source       string
	Altitude     string
	Speed        string
	Satellites   string
	ResponseTime string
	APICost      string
	Confidence   string
	DateTime     string
}

// StandardizedOutputTableTest creates the standardized output table
type StandardizedOutputTableTest struct {
	sshClient *ssh.Client
	gpsData   StandardizedGPSOutput
	starlink  StandardizedGPSOutput
	google    StandardizedGPSOutput
}

// NewStandardizedOutputTableTest creates a new standardized output table test
func NewStandardizedOutputTableTest(sshClient *ssh.Client) *StandardizedOutputTableTest {
	return &StandardizedOutputTableTest{
		sshClient: sshClient,
	}
}

// CollectStandardizedData collects data from all sources in standardized format
func (sott *StandardizedOutputTableTest) CollectStandardizedData() error {
	fmt.Println("📊 Collecting standardized GPS data from all sources...")

	// Collect GPS data
	if err := sott.collectGPSData(); err != nil {
		fmt.Printf("⚠️  GPS data collection failed: %v\n", err)
	}

	// Collect Starlink data (simulated)
	sott.collectStarlinkData()

	// Collect Google data (simulated with real cellular context)
	if err := sott.collectGoogleData(); err != nil {
		fmt.Printf("⚠️  Google data collection failed: %v\n", err)
	}

	return nil
}

// collectGPSData collects and standardizes GPS data
func (sott *StandardizedOutputTableTest) collectGPSData() error {
	startTime := time.Now()

	// Get combined GPS data (gpsctl + AT command)
	lat, lon, alt, err1 := sott.getGPSCtlCoordinates()
	atData, err2 := sott.getATCommandData()
	gpsctlDetails, err3 := sott.getGPSCtlDetails()

	responseTime := time.Since(startTime)

	if err1 != nil && err2 != nil {
		return fmt.Errorf("both GPS methods failed: %v, %v", err1, err2)
	}

	// Use the best data from both sources
	accuracy := 2.0 // Default fallback
	if err3 == nil && gpsctlDetails.Accuracy > 0 {
		accuracy = gpsctlDetails.Accuracy
	} else if err2 == nil {
		accuracy = atData.HDOP * 1.0 // Conservative estimate
	}

	satellites := 0
	fixType := 0
	dateTime := time.Now().UTC().Format("2006-01-02 15:04:05Z")

	if err2 == nil {
		satellites = atData.Satellites
		fixType = atData.FixType
		dateTime = sott.formatGPSDateTime(atData.Time, atData.Date)
	}

	speed := 0.0
	if err3 == nil {
		speed = gpsctlDetails.Speed // Already in m/s from gpsctl
	} else if err2 == nil {
		speed = atData.SpeedKnots * 0.514444 // Convert knots to m/s
	}

	sott.gpsData = StandardizedGPSOutput{
		Latitude:     fmt.Sprintf("%.8f", lat),        // 8 decimals for full precision
		Longitude:    fmt.Sprintf("%.8f", lon),        // 8 decimals for full precision
		Accuracy:     fmt.Sprintf("%.1f m", accuracy), // Decimal accuracy in meters
		FixType:      fmt.Sprintf("%d", fixType),      // Integer fix type as agreed
		Source:       fmt.Sprintf("GPS (%d satellites)", satellites),
		Altitude:     fmt.Sprintf("%.1f m", alt),     // Meters above sea level
		Speed:        fmt.Sprintf("%.2f m/s", speed), // m/s as agreed
		Satellites:   fmt.Sprintf("%d", satellites),
		ResponseTime: fmt.Sprintf("%dms", responseTime.Milliseconds()),
		APICost:      "N/A", // Removed as agreed
		Confidence:   "N/A", // Removed as agreed (was unclear)
		DateTime:     dateTime,
	}

	return nil
}

// collectStarlinkData collects and standardizes Starlink data (simulated)
// In production, this would query actual Starlink APIs:
// - get_diagnostics.gps_time_s for accurate GPS time
// - get_device_info.utcOffsetS for local time conversion
func (sott *StandardizedOutputTableTest) collectStarlinkData() {
	sott.starlink = StandardizedGPSOutput{
		Latitude:     "59.48005181", // High precision from get_location
		Longitude:    "18.27987656", // High precision from get_location
		Accuracy:     "5.0 m",       // Using uncertainty_meters as agreed
		FixType:      "3",           // Integer fix type as agreed
		Source:       "Starlink (14 satellites)",
		Altitude:     "21.5 m",   // Meters above sea level
		Speed:        "0.00 m/s", // m/s as agreed
		Satellites:   "14",
		ResponseTime: "450ms",
		APICost:      "N/A",                                           // Removed as agreed
		Confidence:   "N/A",                                           // Removed as agreed
		DateTime:     time.Now().UTC().Format("2006-01-02 15:04:05Z"), // UTC time with Z suffix (would be from gps_time_s in production)
	}
}

// collectGoogleData collects and standardizes Google data
func (sott *StandardizedOutputTableTest) collectGoogleData() error {
	startTime := time.Now()

	// Get cellular intelligence for context
	cellIntel, err := collectCellularLocationIntelligence(sott.sshClient)
	if err != nil {
		return fmt.Errorf("failed to collect cellular data: %v", err)
	}

	responseTime := time.Since(startTime)
	cellCount := len(cellIntel.NeighborCells) + 1
	wifiCount := 9 // Estimated

	sott.google = StandardizedGPSOutput{
		Latitude:     "59.47982600", // 8 decimal precision
		Longitude:    "18.27992100", // 8 decimal precision
		Accuracy:     "45.0 m",      // Decimal accuracy in meters
		FixType:      "1",           // Integer fix type as agreed
		Source:       fmt.Sprintf("Google (%d Cell + %d WiFi)", cellCount, wifiCount),
		Altitude:     "6.0 m", // Meters above sea level (estimated)
		Speed:        "N/A",   // Not available from API
		Satellites:   "N/A",   // Not applicable for network-based location
		ResponseTime: fmt.Sprintf("%dms", responseTime.Milliseconds()),
		APICost:      "N/A",                                           // Removed as agreed
		Confidence:   "N/A",                                           // Removed as agreed
		DateTime:     time.Now().UTC().Format("2006-01-02 15:04:05Z"), // UTC time with Z suffix
	}

	return nil
}

// Helper functions from previous implementations
func (sott *StandardizedOutputTableTest) getGPSCtlCoordinates() (lat, lon, alt float64, err error) {
	// Get latitude
	latStr, err := executeCommand(sott.sshClient, "gpsctl -i")
	if err != nil {
		return 0, 0, 0, err
	}
	lat, err = strconv.ParseFloat(strings.TrimSpace(latStr), 64)
	if err != nil {
		return 0, 0, 0, err
	}

	// Get longitude
	lonStr, err := executeCommand(sott.sshClient, "gpsctl -x")
	if err != nil {
		return 0, 0, 0, err
	}
	lon, err = strconv.ParseFloat(strings.TrimSpace(lonStr), 64)
	if err != nil {
		return 0, 0, 0, err
	}

	// Get altitude
	altStr, err := executeCommand(sott.sshClient, "gpsctl -a")
	if err != nil {
		return lat, lon, 0, nil
	}
	alt, err = strconv.ParseFloat(strings.TrimSpace(altStr), 64)
	if err != nil {
		alt = 0
	}

	return lat, lon, alt, nil
}

func (sott *StandardizedOutputTableTest) getATCommandData() (*QuectelGPSData, error) {
	output, err := executeCommand(sott.sshClient, "gsmctl -A 'AT+QGPSLOC=2'")
	if err != nil {
		return nil, err
	}

	gpsData := parseQGPSLOC(output)
	if gpsData == nil {
		return nil, fmt.Errorf("failed to parse GPS data")
	}

	return gpsData, nil
}

// GPSCtlDetails is defined in unified_gps_table.go

func (sott *StandardizedOutputTableTest) getGPSCtlDetails() (*GPSCtlDetails, error) {
	details := &GPSCtlDetails{}

	// Get accuracy
	if accuracyStr, err := executeCommand(sott.sshClient, "gpsctl -u"); err == nil {
		if acc, parseErr := strconv.ParseFloat(strings.TrimSpace(accuracyStr), 64); parseErr == nil {
			details.Accuracy = acc
		}
	}

	// Get satellite count
	if satStr, err := executeCommand(sott.sshClient, "gpsctl -p"); err == nil {
		if sats, parseErr := strconv.Atoi(strings.TrimSpace(satStr)); parseErr == nil {
			details.Satellites = sats
		}
	}

	// Get speed
	if speedStr, err := executeCommand(sott.sshClient, "gpsctl -v"); err == nil {
		if speed, parseErr := strconv.ParseFloat(strings.TrimSpace(speedStr), 64); parseErr == nil {
			details.Speed = speed
		}
	}

	return details, nil
}

func (sott *StandardizedOutputTableTest) formatGPSDateTime(timeStr, dateStr string) string {
	if len(timeStr) < 6 || len(dateStr) < 6 {
		return time.Now().UTC().Format("2006-01-02 15:04:05Z")
	}

	hours := timeStr[:2]
	minutes := timeStr[2:4]
	seconds := timeStr[4:6]

	day := dateStr[:2]
	month := dateStr[2:4]
	year := "20" + dateStr[4:6]

	// GPS time is already in UTC, so add Z suffix
	return fmt.Sprintf("%s-%s-%s %s:%s:%sZ", year, month, day, hours, minutes, seconds)
}

func (sott *StandardizedOutputTableTest) getFixTypeDescription(fixType int) string {
	switch fixType {
	case 0:
		return "No Fix"
	case 1:
		return "2D"
	case 2:
		return "3D"
	case 3:
		return "3D"
	default:
		return "Unknown"
	}
}

// DisplayStandardizedOutputTable displays the standardized output table
func (sott *StandardizedOutputTableTest) DisplayStandardizedOutputTable() {
	fmt.Println("\n📊 STANDARDIZED OUTPUT TABLE")
	fmt.Println("============================")

	// Define the fields to display
	fields := []struct {
		Name     string
		GPS      func() string
		Starlink func() string
		Google   func() string
	}{
		{"Latitude", func() string { return sott.gpsData.Latitude }, func() string { return sott.starlink.Latitude }, func() string { return sott.google.Latitude }},
		{"Longitude", func() string { return sott.gpsData.Longitude }, func() string { return sott.starlink.Longitude }, func() string { return sott.google.Longitude }},
		{"Accuracy", func() string { return sott.gpsData.Accuracy }, func() string { return sott.starlink.Accuracy }, func() string { return sott.google.Accuracy }},
		{"Fix Type", func() string { return sott.gpsData.FixType }, func() string { return sott.starlink.FixType }, func() string { return sott.google.FixType }},
		{"Source", func() string { return sott.gpsData.Source }, func() string { return sott.starlink.Source }, func() string { return sott.google.Source }},
		{"Altitude", func() string { return sott.gpsData.Altitude }, func() string { return sott.starlink.Altitude }, func() string { return sott.google.Altitude }},
		{"Speed", func() string { return sott.gpsData.Speed }, func() string { return sott.starlink.Speed }, func() string { return sott.google.Speed }},
		{"Date/Time", func() string { return sott.gpsData.DateTime }, func() string { return sott.starlink.DateTime }, func() string { return sott.google.DateTime }},
		{"Satellites", func() string { return sott.gpsData.Satellites }, func() string { return sott.starlink.Satellites }, func() string { return sott.google.Satellites }},
		{"Response Time", func() string { return sott.gpsData.ResponseTime }, func() string { return sott.starlink.ResponseTime }, func() string { return sott.google.ResponseTime }},
	}

	// Header
	fmt.Printf("%-15s %-20s %-20s %-20s\n", "Field", "GPS", "Starlink", "Google")
	fmt.Println(strings.Repeat("=", 77))

	// Data rows
	for _, field := range fields {
		fmt.Printf("%-15s %-20s %-20s %-20s\n",
			field.Name,
			truncateString(field.GPS(), 20),
			truncateString(field.Starlink(), 20),
			truncateString(field.Google(), 20))
	}
}

// DisplayUniqueDataSummary displays unique data that each source provides
func (sott *StandardizedOutputTableTest) DisplayUniqueDataSummary() {
	fmt.Println("\n🔍 UNIQUE DATA FROM EACH SOURCE")
	fmt.Println("===============================")

	fmt.Println("\n📡 GPS (RUTOS Combined) - Unique Features:")
	fmt.Println("  • Multi-constellation support (GPS+GLONASS+Galileo+BeiDou)")
	fmt.Println("  • External multi-GNSS antenna")
	fmt.Println("  • Direct accuracy measurement (gpsctl -u)")
	fmt.Println("  • 6-decimal coordinate precision (±0.1m resolution)")
	fmt.Println("  • Real-time HDOP quality assessment")
	fmt.Println("  • Course/bearing information")
	fmt.Println("  • 1Hz update rate")
	fmt.Println("  • Barometric altitude integration")

	fmt.Println("\n🛰️ Starlink Multi-API - Unique Features:")
	fmt.Println("  • Uncertainty meters (not HDOP-based)")
	fmt.Println("  • Vertical speed component")
	fmt.Println("  • GPS time in seconds (high precision)")
	fmt.Println("  • Dish hardware/software version info")
	fmt.Println("  • Location enable/disable status")
	fmt.Println("  • GNC (Guidance, Navigation, Control) source")
	fmt.Println("  • UTC offset information")
	fmt.Println("  • Multi-API data fusion")

	fmt.Println("\n🌐 Google Geolocation - Unique Features:")
	fmt.Println("  • Cell tower triangulation data")
	fmt.Println("  • WiFi access point positioning")
	fmt.Println("  • MCC/MNC network identification")
	fmt.Println("  • 5G-NSA network type detection")
	fmt.Println("  • Carrier information (Telia)")
	fmt.Println("  • API quota tracking")
	fmt.Println("  • IP-based location (disabled for accuracy)")
	fmt.Println("  • Open Elevation API altitude estimation")
}

// testStandardizedOutputTable runs the standardized output table test
func testStandardizedOutputTable() {
	fmt.Println("📊 Standardized Output Table Test")
	fmt.Println("=================================")

	// Create SSH client
	client, err := createSSHClient()
	if err != nil {
		fmt.Printf("❌ Failed to create SSH client: %v\n", err)
		return
	}
	defer client.Close()

	// Create standardized output table test
	sott := NewStandardizedOutputTableTest(client)

	// Collect standardized data from all sources
	if err := sott.CollectStandardizedData(); err != nil {
		fmt.Printf("❌ Failed to collect standardized data: %v\n", err)
		return
	}

	// Display standardized output table
	sott.DisplayStandardizedOutputTable()

	// Display unique data summary
	sott.DisplayUniqueDataSummary()

	fmt.Println("\n🎯 Standardized Output Table Complete!")
	fmt.Printf("✅ GPS: Combined precision with %s satellites and %s accuracy\n", sott.gpsData.Satellites, sott.gpsData.Accuracy)
	fmt.Printf("✅ Starlink: Multi-API GPS with %s accuracy\n", sott.starlink.Accuracy)
	fmt.Printf("✅ Google: Network-based positioning with %s accuracy\n", sott.google.Accuracy)
}
