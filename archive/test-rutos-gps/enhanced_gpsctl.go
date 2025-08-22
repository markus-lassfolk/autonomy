package main

import (
	"fmt"
	"math"
	"strconv"
	"strings"
	"time"

	"golang.org/x/crypto/ssh"
)

// EnhancedGPSData represents complete GPS data from gpsctl
type EnhancedGPSData struct {
	Latitude    float64   `json:"latitude"`
	Longitude   float64   `json:"longitude"`
	Altitude    float64   `json:"altitude"`
	Speed       float64   `json:"speed"`
	Satellites  int       `json:"satellites"`
	Accuracy    float64   `json:"accuracy"`
	FixStatus   int       `json:"fix_status"`
	Timestamp   int64     `json:"timestamp"`
	DateTime    string    `json:"datetime"`
	Valid       bool      `json:"valid"`
	Source      string    `json:"source"`
	CollectedAt time.Time `json:"collected_at"`
}

// collectEnhancedGPSData collects comprehensive GPS data using gpsctl
func collectEnhancedGPSData(client *ssh.Client) (*EnhancedGPSData, error) {
	fmt.Println("🎯 Collecting Enhanced GPS Data from External Antenna")
	fmt.Println("=" + strings.Repeat("=", 52))

	gpsData := &EnhancedGPSData{
		Source:      "external_gps_antenna",
		CollectedAt: time.Now(),
	}

	// Collect all GPS parameters
	gpsCommands := map[string]string{
		"latitude":   "gpsctl -i",
		"longitude":  "gpsctl -x",
		"altitude":   "gpsctl -a",
		"speed":      "gpsctl -v",
		"satellites": "gpsctl -p",
		"accuracy":   "gpsctl -u",
		"status":     "gpsctl -s",
		"timestamp":  "gpsctl -t",
		"datetime":   "gpsctl -e",
	}

	results := make(map[string]string)
	errors := make(map[string]error)

	// Execute all commands
	for param, cmd := range gpsCommands {
		fmt.Printf("  📡 Getting %s: ", param)
		output, err := executeCommand(client, cmd)
		if err != nil {
			fmt.Printf("❌ Failed: %v\n", err)
			errors[param] = err
		} else {
			output = strings.TrimSpace(output)
			results[param] = output
			fmt.Printf("✅ %s\n", output)
		}
	}

	// Parse results
	if lat, err := strconv.ParseFloat(results["latitude"], 64); err == nil {
		gpsData.Latitude = lat
	}
	if lon, err := strconv.ParseFloat(results["longitude"], 64); err == nil {
		gpsData.Longitude = lon
	}
	if alt, err := strconv.ParseFloat(results["altitude"], 64); err == nil {
		gpsData.Altitude = alt
	}
	if speed, err := strconv.ParseFloat(results["speed"], 64); err == nil {
		gpsData.Speed = speed
	}
	if sats, err := strconv.Atoi(results["satellites"]); err == nil {
		gpsData.Satellites = sats
	}
	if acc, err := strconv.ParseFloat(results["accuracy"], 64); err == nil {
		gpsData.Accuracy = acc
	}
	if status, err := strconv.Atoi(results["status"]); err == nil {
		gpsData.FixStatus = status
	}
	if ts, err := strconv.ParseInt(results["timestamp"], 10, 64); err == nil {
		gpsData.Timestamp = ts
	}
	gpsData.DateTime = results["datetime"]

	// Determine if GPS fix is valid
	gpsData.Valid = gpsData.FixStatus > 0 &&
		gpsData.Latitude != 0 &&
		gpsData.Longitude != 0 &&
		gpsData.Satellites > 0

	fmt.Println("\n📊 Enhanced GPS Data Summary:")
	fmt.Println("=" + strings.Repeat("=", 30))
	displayEnhancedGPSData(gpsData)

	if len(errors) > 0 {
		fmt.Println("\n⚠️  Errors encountered:")
		for param, err := range errors {
			fmt.Printf("  %s: %v\n", param, err)
		}
	}

	return gpsData, nil
}

func displayEnhancedGPSData(gps *EnhancedGPSData) {
	if gps.Valid {
		fmt.Printf("  ✅ GPS Fix Status: VALID (%d)\n", gps.FixStatus)
	} else {
		fmt.Printf("  ❌ GPS Fix Status: INVALID (%d)\n", gps.FixStatus)
	}

	fmt.Printf("  📍 Coordinates: %.8f°, %.8f°\n", gps.Latitude, gps.Longitude)
	fmt.Printf("  🏔️  Altitude: %.2f meters\n", gps.Altitude)
	fmt.Printf("  🛰️  Satellites: %d\n", gps.Satellites)
	fmt.Printf("  🎯 Accuracy: %.2f meters\n", gps.Accuracy)
	fmt.Printf("  🚀 Speed: %.2f knots\n", gps.Speed)
	fmt.Printf("  ⏰ Timestamp: %d (%s)\n", gps.Timestamp, time.Unix(gps.Timestamp, 0).Format("2006-01-02 15:04:05"))
	fmt.Printf("  📅 DateTime: %s\n", gps.DateTime)
	fmt.Printf("  📡 Source: %s\n", gps.Source)

	// Create Google Maps link
	mapsLink := fmt.Sprintf("https://www.google.com/maps?q=%.8f,%.8f", gps.Latitude, gps.Longitude)
	fmt.Printf("  🗺️  Maps Link: %s\n", mapsLink)
}

// compareGPSSources compares GPS data from different sources
func compareGPSSources(client *ssh.Client) {
	fmt.Println("\n🔍 GPS Source Comparison")
	fmt.Println("=" + strings.Repeat("=", 25))

	// Get Enhanced GPS data (external antenna)
	fmt.Println("\n1. 🏆 External GPS Antenna (Most Accurate):")
	externalGPS, err := collectEnhancedGPSData(client)
	if err != nil {
		fmt.Printf("   ❌ Failed to get external GPS: %v\n", err)
	}

	// Get Starlink GPS data (would need to call Starlink API)
	fmt.Println("\n2. 🥈 Starlink GPS (Reference):")
	fmt.Println("   📊 Previous Starlink coordinates: 59.48005935°, 18.27982195°")

	// Calculate distance if we have both
	if externalGPS != nil && externalGPS.Valid {
		starlinkLat := 59.48005935
		starlinkLon := 18.27982195

		distance := calculateDistance(externalGPS.Latitude, externalGPS.Longitude, starlinkLat, starlinkLon)
		fmt.Printf("   📏 Distance from Starlink GPS: %.2f meters\n", distance)

		if distance < 10 {
			fmt.Println("   ✅ Excellent agreement between sources!")
		} else if distance < 50 {
			fmt.Println("   ✅ Good agreement between sources")
		} else {
			fmt.Println("   ⚠️  Significant difference - investigate further")
		}
	}
}

// calculateDistance calculates distance between two GPS coordinates (simplified for small distances)
func calculateDistance(lat1, lon1, lat2, lon2 float64) float64 {
	const earthRadiusM = 6371000 // Earth's radius in meters

	// Convert degrees to radians
	lat1Rad := lat1 * math.Pi / 180
	lon1Rad := lon1 * math.Pi / 180
	lat2Rad := lat2 * math.Pi / 180
	lon2Rad := lon2 * math.Pi / 180

	// Haversine formula
	deltaLat := lat2Rad - lat1Rad
	deltaLon := lon2Rad - lon1Rad

	a := math.Sin(deltaLat/2)*math.Sin(deltaLat/2) +
		math.Cos(lat1Rad)*math.Cos(lat2Rad)*
			math.Sin(deltaLon/2)*math.Sin(deltaLon/2)

	c := 2 * math.Atan2(math.Sqrt(a), math.Sqrt(1-a))

	return earthRadiusM * c
}
