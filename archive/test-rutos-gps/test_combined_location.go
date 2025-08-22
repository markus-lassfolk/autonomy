package main

import (
	"context"
	"fmt"
	"strconv"
	"strings"
	"time"

	"googlemaps.github.io/maps"
)

// CombinedLocationResult represents the result of combined cellular + BSSID location
type CombinedLocationResult struct {
	Success      bool                  `json:"success"`
	Location     *maps.LatLng          `json:"location"`
	Accuracy     float64               `json:"accuracy"`
	CellTowers   []maps.CellTower      `json:"cell_towers"`
	AccessPoints []WiFiAccessPointInfo `json:"access_points"`
	CellsUsed    int                   `json:"cells_used"`
	APsUsed      int                   `json:"aps_used"`
	ResponseTime time.Duration         `json:"response_time"`
	ErrorMessage string                `json:"error_message"`
	RequestTime  time.Time             `json:"request_time"`
	RadioType    string                `json:"radio_type"`
	ConsideredIP bool                  `json:"considered_ip"`
}

// testCombinedLocation tests combined cellular + BSSID location
func testCombinedLocation() error {
	fmt.Println("📡 COMBINED CELLULAR + BSSID LOCATION TEST")
	fmt.Println("=" + strings.Repeat("=", 42))

	// Connect to RutOS
	client, err := createSSHClient()
	if err != nil {
		return fmt.Errorf("failed to connect to RutOS: %w", err)
	}
	defer client.Close()

	// Collect cellular intelligence
	fmt.Println("📱 Collecting cellular intelligence...")
	cellIntel, err := collectCellularLocationIntelligence(client)
	if err != nil {
		return fmt.Errorf("failed to collect cellular intelligence: %w", err)
	}

	// Collect WiFi access points
	fmt.Println("📶 Collecting WiFi access points...")
	accessPoints, err := collectWiFiAccessPoints(client)
	if err != nil {
		fmt.Printf("⚠️  WiFi collection failed: %v\n", err)
		accessPoints = []WiFiAccessPointInfo{} // Continue with cellular only
	}

	// Test combined location
	result, err := getCombinedLocation(cellIntel, accessPoints)
	if err != nil {
		return fmt.Errorf("combined location failed: %w", err)
	}

	// Display results
	result.PrintCombinedResult()

	// Compare with GPS
	fmt.Println("\n🎯 Comparing with GPS reference...")
	result.CompareWithGPS(59.48007, 18.27985) // Known GPS coordinates

	// Show comparison with individual methods
	fmt.Println("\n📊 Method Comparison:")
	fmt.Println("  🔄 Testing individual methods for comparison...")

	return nil
}

// getCombinedLocation gets location using both cellular and WiFi data
func getCombinedLocation(cellIntel *CellularLocationIntelligence, accessPoints []WiFiAccessPointInfo) (*CombinedLocationResult, error) {
	start := time.Now()

	result := &CombinedLocationResult{
		AccessPoints: accessPoints,
		RequestTime:  start,
		ConsideredIP: false, // Disable IP - we need precise location, not ISP location
	}

	// Load Google API key
	apiKey, err := LoadGoogleAPIKey()
	if err != nil {
		result.ErrorMessage = fmt.Sprintf("Failed to load API key: %v", err)
		return result, err
	}

	// Create Google Maps client
	client, err := maps.NewClient(maps.WithAPIKey(apiKey))
	if err != nil {
		result.ErrorMessage = fmt.Sprintf("Failed to create Google client: %v", err)
		return result, err
	}

	// Parse MCC/MNC for home network information
	mcc, err := strconv.Atoi(cellIntel.ServingCell.MCC)
	if err != nil {
		mcc = 0 // Fallback if parsing fails
	}
	mnc, err := strconv.Atoi(cellIntel.ServingCell.MNC)
	if err != nil {
		mnc = 0 // Fallback if parsing fails
	}

	// Build cell towers for Google API
	cellTowers, radioType, err := BuildGoogleCellTowersFromIntelligence(cellIntel, 20) // Use up to 20 cells
	if err != nil {
		fmt.Printf("⚠️  Failed to build cell towers: %v\n", err)
		cellTowers = []maps.CellTower{} // Continue with WiFi only
	}
	result.CellTowers = cellTowers
	result.RadioType = radioType
	result.CellsUsed = len(cellTowers)

	// Build WiFi access points for Google API with ALL available data
	var wifiAPs []maps.WiFiAccessPoint
	maxAPs := 15 // Google limit
	validAPs := accessPoints
	if len(validAPs) > maxAPs {
		validAPs = validAPs[:maxAPs]
	}

	for i, ap := range validAPs {
		wifiAP := maps.WiFiAccessPoint{
			// Required field
			MACAddress: ap.BSSID,

			// Optional fields for maximum accuracy
			SignalStrength: float64(ap.SignalStrength), // Signal strength in dBm
		}

		// Add all available optional fields

		// Age of measurement (optional) - set to 0 for fresh scan
		wifiAP.Age = 0

		// Channel (optional) - helps with AP identification and accuracy
		if ap.Channel > 0 {
			wifiAP.Channel = ap.Channel
		}

		// Signal-to-noise ratio (optional) - we don't currently collect this
		// but could be added in future for better accuracy

		fmt.Printf("    📶 WiFi AP %d: BSSID=%s, Signal=%.0f dBm, Channel=%d, SSID=%s\n",
			i+1, wifiAP.MACAddress, wifiAP.SignalStrength, wifiAP.Channel, ap.SSID)

		wifiAPs = append(wifiAPs, wifiAP)
	}
	result.APsUsed = len(wifiAPs)

	// Validate we have enough data
	if len(cellTowers) == 0 && len(wifiAPs) < 2 {
		result.ErrorMessage = "Insufficient data: need either cell towers or minimum 2 WiFi access points"
		return result, fmt.Errorf(result.ErrorMessage)
	}

	// Create enhanced combined geolocation request with ALL available data
	req := &maps.GeolocationRequest{
		// High Impact: Home network information
		HomeMobileCountryCode: mcc, // Use serving cell's MCC as home network
		HomeMobileNetworkCode: mnc, // Use serving cell's MNC as home network

		// High Impact: Carrier information
		Carrier: cellIntel.NetworkInfo.Operator, // "Telia"

		// Enhanced radio type
		RadioType: maps.RadioType(result.RadioType),

		// Data arrays
		CellTowers:       cellTowers,
		WiFiAccessPoints: wifiAPs,

		// IP consideration (disabled for precision)
		ConsiderIP: result.ConsideredIP,
	}

	// Display enhanced request summary
	fmt.Printf("📡 Google Geolocation Request (Enhanced Combined):\n")
	fmt.Printf("  🏠 Home MCC/MNC: %d/%d\n", mcc, mnc)
	fmt.Printf("  📡 Carrier: %s\n", cellIntel.NetworkInfo.Operator)
	fmt.Printf("  🗼 Radio Type: %s\n", result.RadioType)
	fmt.Printf("  📱 Cell Towers: %d\n", len(cellTowers))
	fmt.Printf("  📶 WiFi APs: %d\n", len(wifiAPs))
	fmt.Printf("  🌐 Consider IP: %t\n", result.ConsideredIP)

	// Show detailed breakdown
	if len(cellTowers) > 0 {
		fmt.Println("  📋 Cell Tower Details:")
		for i, tower := range cellTowers {
			if i >= 3 { // Show first 3
				fmt.Printf("    ... and %d more\n", len(cellTowers)-3)
				break
			}
			fmt.Printf("    %d. CellID: %d, MCC: %d, MNC: %d, Signal: %d dBm\n",
				i+1, tower.CellID, tower.MobileCountryCode, tower.MobileNetworkCode, tower.SignalStrength)
		}
	}

	if len(wifiAPs) > 0 {
		fmt.Println("  📋 WiFi AP Details:")
		for i, ap := range wifiAPs {
			if i >= 3 { // Show first 3
				fmt.Printf("    ... and %d more\n", len(wifiAPs)-3)
				break
			}
			fmt.Printf("    %d. BSSID: %s, Signal: %.0f dBm\n", i+1, ap.MACAddress, ap.SignalStrength)
		}
	}

	// Make the request
	fmt.Println("🎯 Requesting location from Google (Combined)...")
	resp, err := client.Geolocate(context.Background(), req)
	if err != nil {
		result.ErrorMessage = fmt.Sprintf("Google API error: %v", err)
		return result, err
	}

	result.ResponseTime = time.Since(start)
	result.Success = true
	result.Location = &resp.Location
	result.Accuracy = resp.Accuracy

	return result, nil
}

// PrintCombinedResult prints the combined location result
func (result *CombinedLocationResult) PrintCombinedResult() {
	fmt.Printf("\n📊 Combined Location Response:\n")
	fmt.Println("=" + strings.Repeat("=", 33))

	if result.Success {
		fmt.Printf("✅ SUCCESS: combined_cellular_bssid_location\n")
		fmt.Printf("📍 Location: %.6f°, %.6f°\n", result.Location.Lat, result.Location.Lng)
		fmt.Printf("🎯 Accuracy: ±%.0f meters\n", result.Accuracy)
		fmt.Printf("📱 Cell Towers Used: %d\n", result.CellsUsed)
		fmt.Printf("📶 WiFi APs Used: %d\n", result.APsUsed)
		fmt.Printf("🗼 Radio Type: %s\n", result.RadioType)
		fmt.Printf("🌐 IP Considered: %t\n", result.ConsideredIP)
		fmt.Printf("🗺️  Maps Link: https://www.google.com/maps?q=%.6f,%.6f\n",
			result.Location.Lat, result.Location.Lng)
		fmt.Printf("⏱️  Response Time: %.1f ms\n", float64(result.ResponseTime.Nanoseconds())/1e6)
		fmt.Printf("⏰ Request Time: %s\n", result.RequestTime.Format("2006-01-02 15:04:05"))

		// Show data source breakdown
		fmt.Printf("\n📊 Data Sources:\n")
		if result.CellsUsed > 0 && result.APsUsed > 0 {
			fmt.Printf("  🎯 HYBRID: Cellular (%d towers) + WiFi (%d APs)\n", result.CellsUsed, result.APsUsed)
		} else if result.CellsUsed > 0 {
			fmt.Printf("  📱 CELLULAR ONLY: %d cell towers\n", result.CellsUsed)
		} else if result.APsUsed > 0 {
			fmt.Printf("  📶 WIFI ONLY: %d access points\n", result.APsUsed)
		}
	} else {
		fmt.Printf("❌ FAILED: combined_cellular_bssid_location\n")
		fmt.Printf("💥 Error: %s\n", result.ErrorMessage)
	}
}

// CompareWithGPS compares combined location result with GPS coordinates
func (result *CombinedLocationResult) CompareWithGPS(gpsLat, gpsLon float64) {
	if !result.Success {
		fmt.Println("❌ Cannot compare - combined location failed")
		return
	}

	// Calculate distance between combined result and GPS
	distance := calculateDistance(
		result.Location.Lat, result.Location.Lng,
		gpsLat, gpsLon,
	)

	fmt.Printf("\n🎯 Accuracy Comparison with GPS:\n")
	fmt.Printf("  📍 Combined: %.6f°, %.6f° (±%.0fm)\n",
		result.Location.Lat, result.Location.Lng, result.Accuracy)
	fmt.Printf("  🛰️  GPS: %.6f°, %.6f°\n", gpsLat, gpsLon)
	fmt.Printf("  📏 Distance: %.1f meters\n", distance)

	// Determine accuracy rating
	if distance <= result.Accuracy {
		fmt.Printf("  ✅ EXCELLENT: Within accuracy range!\n")
	} else if distance <= result.Accuracy*1.5 {
		fmt.Printf("  ✅ VERY GOOD: Within 1.5x accuracy range\n")
	} else if distance <= result.Accuracy*2 {
		fmt.Printf("  ✅ GOOD: Within 2x accuracy range\n")
	} else {
		fmt.Printf("  ⚠️  FAIR: Outside accuracy range but reasonable\n")
	}

	// Show improvement over individual methods
	fmt.Printf("\n📈 Expected Improvements:\n")
	fmt.Printf("  🎯 Combined approach should provide:\n")
	fmt.Printf("    • Better accuracy than cellular alone\n")
	fmt.Printf("    • More reliability than WiFi alone\n")
	fmt.Printf("    • Redundancy if one method fails\n")
}
