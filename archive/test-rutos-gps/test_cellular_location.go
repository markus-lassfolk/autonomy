package main

import (
	"fmt"
	"regexp"
	"strconv"
	"strings"
	"time"

	"golang.org/x/crypto/ssh"
)

// CellularLocationData represents location data from cellular network
type CellularLocationData struct {
	Latitude       float64   `json:"latitude"`
	Longitude      float64   `json:"longitude"`
	Accuracy       float64   `json:"accuracy"`        // Accuracy in meters
	LocationType   int       `json:"location_type"`   // Type of location service
	Method         string    `json:"method"`          // Which AT command was used
	CellID         string    `json:"cell_id"`         // Cell tower ID
	LAC            string    `json:"lac"`             // Location Area Code
	MCC            string    `json:"mcc"`             // Mobile Country Code
	MNC            string    `json:"mnc"`             // Mobile Network Code
	SignalStrength int       `json:"signal_strength"` // Signal strength
	Valid          bool      `json:"valid"`
	Source         string    `json:"source"`
	RawData        string    `json:"raw_data"`
	CollectedAt    time.Time `json:"collected_at"`
	ResponseTime   float64   `json:"response_time_ms"`
	Error          string    `json:"error,omitempty"`
}

// testCellularLocation tests all cellular location methods
func testCellularLocation(client *ssh.Client) {
	fmt.Println("🗼 Testing Cellular Network Location Services")
	fmt.Println("=" + strings.Repeat("=", 45))

	// 1. Test basic location services
	fmt.Println("\n📡 1. Basic Location Services (CLBS):")
	testCLBSLocation(client)

	// 2. Test IP-based location services
	fmt.Println("\n🌐 2. IP-Based Location Services (CIPGSMLOC):")
	testCIPGSMLOCLocation(client)

	// 3. Test Quectel-specific location services
	fmt.Println("\n📱 3. Quectel Location Services (QLBS):")
	testQLBSLocation(client)

	// 4. Test cell-based location
	fmt.Println("\n🗼 4. Cell-Based Location (QCELLLOC):")
	testQCELLLOCLocation(client)

	// 5. Get cell tower information
	fmt.Println("\n📊 5. Cell Tower Information:")
	getCellTowerInfo(client)

	// 6. Summary and recommendations
	fmt.Println("\n📋 6. Summary & Recommendations:")
	provideCellularLocationSummary()
}

func testCLBSLocation(client *ssh.Client) {
	commands := []string{
		"AT+CLBS=4,1", // Get current location
		"AT+CLBS=2,1", // Alternative method
		"AT+CLBS=1,1", // Enable location services
	}

	for _, cmd := range commands {
		fmt.Printf("  Testing %s: ", cmd)
		start := time.Now()
		output, err := executeCommand(client, fmt.Sprintf("gsmctl -A '%s'", cmd))
		duration := time.Since(start)

		if err != nil {
			fmt.Printf("❌ Failed: %v\n", err)
			continue
		}

		if strings.Contains(output, "+CLBS:") {
			fmt.Printf("🎉 SUCCESS! (%v)\n", duration)
			location := parseCLBSResponse(output, cmd)
			if location != nil && location.Valid {
				displayCellularLocation(location)
			}
		} else if strings.Contains(output, "ERROR") {
			fmt.Printf("❌ ERROR: %s\n", strings.TrimSpace(output))
		} else {
			fmt.Printf("📊 Response: %s (%v)\n", strings.TrimSpace(output), duration)
		}
	}
}

func testCIPGSMLOCLocation(client *ssh.Client) {
	commands := []string{
		"AT+CIPGSMLOC=1,1", // GSM location
		"AT+CIPGSMLOC=2,1", // GPS location via network
		"AT+CIPGSMLOC=3,1", // Network location
	}

	for _, cmd := range commands {
		fmt.Printf("  Testing %s: ", cmd)
		start := time.Now()
		output, err := executeCommand(client, fmt.Sprintf("gsmctl -A '%s'", cmd))
		duration := time.Since(start)

		if err != nil {
			fmt.Printf("❌ Failed: %v\n", err)
			continue
		}

		if strings.Contains(output, "+CIPGSMLOC:") {
			fmt.Printf("🎉 SUCCESS! (%v)\n", duration)
			location := parseCIPGSMLOCResponse(output, cmd)
			if location != nil && location.Valid {
				displayCellularLocation(location)
			}
		} else if strings.Contains(output, "ERROR") {
			fmt.Printf("❌ ERROR: %s\n", strings.TrimSpace(output))
		} else {
			fmt.Printf("📊 Response: %s (%v)\n", strings.TrimSpace(output), duration)
		}
	}
}

func testQLBSLocation(client *ssh.Client) {
	commands := []string{
		"AT+QLBS=2,1", // Get location via LBS
		"AT+QLBS=1",   // Enable LBS
		"AT+QLBSCFG?", // Check LBS configuration
	}

	for _, cmd := range commands {
		fmt.Printf("  Testing %s: ", cmd)
		start := time.Now()
		output, err := executeCommand(client, fmt.Sprintf("gsmctl -A '%s'", cmd))
		duration := time.Since(start)

		if err != nil {
			fmt.Printf("❌ Failed: %v\n", err)
			continue
		}

		if strings.Contains(output, "+QLBS:") {
			fmt.Printf("🎉 SUCCESS! (%v)\n", duration)
			// Parse QLBS response if needed
		} else if strings.Contains(output, "ERROR") {
			fmt.Printf("❌ ERROR: %s\n", strings.TrimSpace(output))
		} else {
			fmt.Printf("📊 Response: %s (%v)\n", strings.TrimSpace(output), duration)
		}
	}
}

func testQCELLLOCLocation(client *ssh.Client) {
	commands := []string{
		"AT+QCELLLOC=1,1", // Cell location
		"AT+QCELLLOC=2,1", // Enhanced cell location
	}

	for _, cmd := range commands {
		fmt.Printf("  Testing %s: ", cmd)
		start := time.Now()
		output, err := executeCommand(client, fmt.Sprintf("gsmctl -A '%s'", cmd))
		duration := time.Since(start)

		if err != nil {
			fmt.Printf("❌ Failed: %v\n", err)
			continue
		}

		if strings.Contains(output, "+QCELLLOC:") {
			fmt.Printf("🎉 SUCCESS! (%v)\n", duration)
			// Parse QCELLLOC response if needed
		} else if strings.Contains(output, "ERROR") {
			fmt.Printf("❌ ERROR: %s\n", strings.TrimSpace(output))
		} else {
			fmt.Printf("📊 Response: %s (%v)\n", strings.TrimSpace(output), duration)
		}
	}
}

func getCellTowerInfo(client *ssh.Client) {
	commands := map[string]string{
		"Cell ID":        "gsmctl -C",
		"Network Info":   "gsmctl -F",
		"Serving Cell":   "gsmctl -A 'AT+QENG=\"servingcell\"'",
		"Neighbor Cells": "gsmctl -A 'AT+QENG=\"neighbourcell\"'",
		"Signal Quality": "gsmctl -q",
	}

	for name, cmd := range commands {
		fmt.Printf("  %s: ", name)
		output, err := executeCommand(client, cmd)
		if err != nil {
			fmt.Printf("❌ Failed: %v\n", err)
		} else {
			output = strings.TrimSpace(output)
			if len(output) > 80 {
				fmt.Printf("📊 %s...\n", output[:80])
			} else {
				fmt.Printf("📊 %s\n", output)
			}
		}
	}
}

func parseCLBSResponse(response, command string) *CellularLocationData {
	location := &CellularLocationData{
		Source:      "cellular_clbs",
		Method:      command,
		CollectedAt: time.Now(),
		RawData:     response,
	}

	// Parse +CLBS: location_type,longitude,latitude,accuracy,date,time
	re := regexp.MustCompile(`\+CLBS:\s*(\d+),([^,]+),([^,]+),([^,]+),([^,]+),([^,]+)`)
	matches := re.FindStringSubmatch(response)

	if len(matches) >= 7 {
		if locType, err := strconv.Atoi(matches[1]); err == nil {
			location.LocationType = locType
		}

		if lon, err := strconv.ParseFloat(matches[2], 64); err == nil {
			location.Longitude = lon
		}

		if lat, err := strconv.ParseFloat(matches[3], 64); err == nil {
			location.Latitude = lat
		}

		if acc, err := strconv.ParseFloat(matches[4], 64); err == nil {
			location.Accuracy = acc
		}

		location.Valid = location.Latitude != 0 && location.Longitude != 0
	}

	return location
}

func parseCIPGSMLOCResponse(response, command string) *CellularLocationData {
	location := &CellularLocationData{
		Source:      "cellular_cipgsmloc",
		Method:      command,
		CollectedAt: time.Now(),
		RawData:     response,
	}

	// Parse +CIPGSMLOC: longitude,latitude,accuracy,date,time
	re := regexp.MustCompile(`\+CIPGSMLOC:\s*([^,]+),([^,]+),([^,]+),([^,]+),([^,]+)`)
	matches := re.FindStringSubmatch(response)

	if len(matches) >= 6 {
		if lon, err := strconv.ParseFloat(matches[1], 64); err == nil {
			location.Longitude = lon
		}

		if lat, err := strconv.ParseFloat(matches[2], 64); err == nil {
			location.Latitude = lat
		}

		if acc, err := strconv.ParseFloat(matches[3], 64); err == nil {
			location.Accuracy = acc
		}

		location.Valid = location.Latitude != 0 && location.Longitude != 0
	}

	return location
}

func displayCellularLocation(location *CellularLocationData) {
	fmt.Printf("    📍 Coordinates: %.6f°, %.6f°\n", location.Latitude, location.Longitude)
	fmt.Printf("    🎯 Accuracy: %.0f meters\n", location.Accuracy)
	if location.LocationType > 0 {
		fmt.Printf("    📡 Location Type: %d\n", location.LocationType)
	}
	fmt.Printf("    📱 Method: %s\n", location.Method)

	// Create Google Maps link
	mapsLink := fmt.Sprintf("https://www.google.com/maps?q=%.6f,%.6f", location.Latitude, location.Longitude)
	fmt.Printf("    🗺️  Maps: %s\n", mapsLink)
}

func provideCellularLocationSummary() {
	fmt.Println("  📊 Cellular Location Summary:")
	fmt.Println("    • CLBS: Basic location services (cell tower triangulation)")
	fmt.Println("    • CIPGSMLOC: IP-based location services")
	fmt.Println("    • QLBS: Quectel-specific location services")
	fmt.Println("    • QCELLLOC: Enhanced cell-based location")
	fmt.Println()
	fmt.Println("  🎯 Expected Accuracy:")
	fmt.Println("    • Cell Triangulation: 50-500 meters")
	fmt.Println("    • Enhanced Cell ID: 10-100 meters")
	fmt.Println("    • Network-Assisted: 5-50 meters")
	fmt.Println()
	fmt.Println("  💡 Use Cases:")
	fmt.Println("    • Indoor positioning when GPS fails")
	fmt.Println("    • Emergency fallback location")
	fmt.Println("    • Rough location for geofencing")
	fmt.Println("    • Cross-validation of GPS data")
}
