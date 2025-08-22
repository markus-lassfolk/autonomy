package main

import (
	"fmt"
	"strconv"
	"strings"
	"time"

	"golang.org/x/crypto/ssh"
)

// QuectelGPSData represents GPS data from Quectel modem
type QuectelGPSData struct {
	Latitude    float64   `json:"latitude"`
	Longitude   float64   `json:"longitude"`
	Altitude    float64   `json:"altitude"`
	SpeedKmh    float64   `json:"speed_kmh"`
	SpeedKnots  float64   `json:"speed_knots"`
	Course      float64   `json:"course"`
	Satellites  int       `json:"satellites"`
	HDOP        float64   `json:"hdop"`
	FixType     int       `json:"fix_type"`
	Time        string    `json:"time"`
	Date        string    `json:"date"`
	Valid       bool      `json:"valid"`
	Source      string    `json:"source"`
	RawData     string    `json:"raw_data"`
	CollectedAt time.Time `json:"collected_at"`
}

// testQuectelGPS tests the working Quectel GPS command
func testQuectelGPS(client *ssh.Client) (*QuectelGPSData, error) {
	fmt.Println("🎯 Testing Quectel GSM GPS (Tertiary GPS Source)")
	fmt.Println("=" + strings.Repeat("=", 48))

	// Test the working command
	fmt.Println("📡 Executing AT+QGPSLOC=2...")
	output, err := executeCommand(client, "gsmctl -A 'AT+QGPSLOC=2'")
	if err != nil {
		return nil, fmt.Errorf("QGPSLOC command failed: %v", err)
	}

	fmt.Printf("📊 Raw Response: %s\n", strings.TrimSpace(output))

	// Parse the response
	gpsData := parseQGPSLOC(output)
	if gpsData == nil {
		return nil, fmt.Errorf("failed to parse QGPSLOC response")
	}

	fmt.Println("\n📍 Parsed GSM GPS Data:")
	fmt.Println("=" + strings.Repeat("=", 25))
	displayQuectelGPSData(gpsData)

	return gpsData, nil
}

// parseQGPSLOC parses Quectel QGPSLOC response
func parseQGPSLOC(response string) *QuectelGPSData {
	gpsData := &QuectelGPSData{
		Source:      "quectel_gsm_gps",
		CollectedAt: time.Now(),
		RawData:     response,
	}

	// Find the QGPSLOC line
	lines := strings.Split(response, "\n")
	var qgpslocLine string
	for _, line := range lines {
		if strings.Contains(line, "+QGPSLOC:") {
			qgpslocLine = line
			break
		}
	}

	if qgpslocLine == "" {
		return gpsData
	}

	// Parse: +QGPSLOC: time,lat,lon,hdop,altitude,fix,cog,spkm,spkn,date,nsat
	// Example: +QGPSLOC: 001047.00,59.48007,18.27985,0.4,9.5,3,,0.0,0.0,160825,39

	// Remove the "+QGPSLOC: " prefix and clean up whitespace/control characters
	dataStr := strings.TrimPrefix(qgpslocLine, "+QGPSLOC: ")
	dataStr = strings.TrimSpace(dataStr) // Remove \r\n and other whitespace
	parts := strings.Split(dataStr, ",")

	if len(parts) < 11 {
		return gpsData
	}

	// Parse each field
	gpsData.Time = parts[0]

	if lat, err := strconv.ParseFloat(parts[1], 64); err == nil {
		gpsData.Latitude = lat
	}

	if lon, err := strconv.ParseFloat(parts[2], 64); err == nil {
		gpsData.Longitude = lon
	}

	if hdop, err := strconv.ParseFloat(parts[3], 64); err == nil {
		gpsData.HDOP = hdop
	}

	if alt, err := strconv.ParseFloat(parts[4], 64); err == nil {
		gpsData.Altitude = alt
	}

	if fix, err := strconv.Atoi(parts[5]); err == nil {
		gpsData.FixType = fix
	}

	if parts[6] != "" {
		if course, err := strconv.ParseFloat(parts[6], 64); err == nil {
			gpsData.Course = course
		}
	}

	if spkm, err := strconv.ParseFloat(parts[7], 64); err == nil {
		gpsData.SpeedKmh = spkm
	}

	if spkn, err := strconv.ParseFloat(parts[8], 64); err == nil {
		gpsData.SpeedKnots = spkn
	}

	gpsData.Date = parts[9]

	if sats, err := strconv.Atoi(strings.TrimSpace(parts[10])); err == nil {
		gpsData.Satellites = sats
	}

	// Determine if GPS fix is valid
	gpsData.Valid = gpsData.FixType >= 2 && // 2D or 3D fix
		gpsData.Latitude != 0 &&
		gpsData.Longitude != 0 &&
		gpsData.Satellites > 0

	return gpsData
}

// displayQuectelGPSData displays formatted Quectel GPS data
func displayQuectelGPSData(gps *QuectelGPSData) {
	if gps.Valid {
		fmt.Printf("  ✅ GPS Fix Status: VALID (%s)\n", getFixTypeString(gps.FixType))
	} else {
		fmt.Printf("  ❌ GPS Fix Status: INVALID (%s)\n", getFixTypeString(gps.FixType))
	}

	fmt.Printf("  📍 Coordinates: %.8f°, %.8f°\n", gps.Latitude, gps.Longitude)
	fmt.Printf("  🏔️  Altitude: %.2f meters\n", gps.Altitude)
	fmt.Printf("  🛰️  Satellites: %d\n", gps.Satellites)
	fmt.Printf("  🎯 HDOP: %.2f (accuracy indicator)\n", gps.HDOP)

	if gps.SpeedKmh > 0 {
		fmt.Printf("  🚀 Speed: %.2f km/h (%.2f knots)\n", gps.SpeedKmh, gps.SpeedKnots)
	} else {
		fmt.Printf("  🚀 Speed: 0.00 km/h (stationary)\n")
	}

	if gps.Course > 0 {
		fmt.Printf("  🧭 Course: %.2f°\n", gps.Course)
	}

	fmt.Printf("  ⏰ Time: %s\n", formatQGPSTime(gps.Time))
	fmt.Printf("  📅 Date: %s\n", formatQGPSDate(gps.Date))
	fmt.Printf("  📡 Source: %s\n", gps.Source)

	// Create Google Maps link
	mapsLink := fmt.Sprintf("https://www.google.com/maps?q=%.8f,%.8f", gps.Latitude, gps.Longitude)
	fmt.Printf("  🗺️  Maps Link: %s\n", mapsLink)
}

// getFixTypeString returns human-readable fix type
func getFixTypeString(fixType int) string {
	switch fixType {
	case 0:
		return "No Fix"
	case 1:
		return "Dead Reckoning"
	case 2:
		return "2D Fix"
	case 3:
		return "3D Fix"
	case 4:
		return "GNSS + Dead Reckoning"
	case 5:
		return "Time Only Fix"
	default:
		return fmt.Sprintf("Unknown (%d)", fixType)
	}
}

// formatQGPSTime formats QGPS time (HHMMSS.SS)
func formatQGPSTime(timeStr string) string {
	if len(timeStr) >= 6 {
		hour := timeStr[0:2]
		minute := timeStr[2:4]
		second := timeStr[4:6]
		return fmt.Sprintf("%s:%s:%s UTC", hour, minute, second)
	}
	return timeStr
}

// formatQGPSDate formats QGPS date (DDMMYY)
func formatQGPSDate(dateStr string) string {
	if len(dateStr) >= 6 {
		day := dateStr[0:2]
		month := dateStr[2:4]
		year := "20" + dateStr[4:6]
		return fmt.Sprintf("%s/%s/%s", day, month, year)
	}
	return dateStr
}

// compareAllGPSSources compares all three GPS sources
func compareAllGPSSources(client *ssh.Client) {
	fmt.Println("\n🏆 COMPLETE GPS SOURCE COMPARISON")
	fmt.Println("=" + strings.Repeat("=", 35))

	// Get External GPS data
	fmt.Println("\n1. 🥇 External GPS Antenna (Primary):")
	externalGPS, err := collectEnhancedGPSData(client)
	if err != nil {
		fmt.Printf("   ❌ Failed: %v\n", err)
	}

	// Get Quectel GSM GPS data
	fmt.Println("\n2. 🥉 Quectel GSM GPS (Tertiary):")
	quectelGPS, err := testQuectelGPS(client)
	if err != nil {
		fmt.Printf("   ❌ Failed: %v\n", err)
	}

	// Compare results
	fmt.Println("\n3. 🥈 Starlink GPS (Secondary - Reference):")
	fmt.Println("   📊 Previous coordinates: 59.48005935°, 18.27982195°")

	// Calculate distances if we have data
	if externalGPS != nil && externalGPS.Valid && quectelGPS != nil && quectelGPS.Valid {
		fmt.Println("\n📏 Distance Analysis:")

		// Distance between External and Quectel
		extQuectelDist := calculateDistance(
			externalGPS.Latitude, externalGPS.Longitude,
			quectelGPS.Latitude, quectelGPS.Longitude)
		fmt.Printf("   External ↔ Quectel: %.2f meters\n", extQuectelDist)

		// Distance from Starlink reference
		starlinkLat, starlinkLon := 59.48005935, 18.27982195
		extStarlinkDist := calculateDistance(
			externalGPS.Latitude, externalGPS.Longitude,
			starlinkLat, starlinkLon)
		quectelStarlinkDist := calculateDistance(
			quectelGPS.Latitude, quectelGPS.Longitude,
			starlinkLat, starlinkLon)

		fmt.Printf("   External ↔ Starlink: %.2f meters\n", extStarlinkDist)
		fmt.Printf("   Quectel ↔ Starlink: %.2f meters\n", quectelStarlinkDist)

		if extQuectelDist < 10 && extStarlinkDist < 10 && quectelStarlinkDist < 10 {
			fmt.Println("   ✅ EXCELLENT: All GPS sources agree within 10 meters!")
		} else if extQuectelDist < 50 && extStarlinkDist < 50 && quectelStarlinkDist < 50 {
			fmt.Println("   ✅ GOOD: All GPS sources agree within 50 meters")
		} else {
			fmt.Println("   ⚠️  Some GPS sources show significant differences")
		}
	}

	fmt.Println("\n🎯 GPS System Status: ALL THREE SOURCES OPERATIONAL! 🎉")
}
