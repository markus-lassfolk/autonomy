package main

import (
	"bytes"
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"os"
	"strings"
	"time"
)

// LocalCellTowerTest runs cell tower location tests with hardcoded data
func runLocalCellTowerTest() error {
	fmt.Println("🎯 LOCAL CELL TOWER LOCATION ACCURACY TEST")
	fmt.Println("=" + strings.Repeat("=", 45))
	fmt.Println("📡 Using hardcoded cell tower data from your RutOS device")
	fmt.Println()

	// Hardcoded GPS reference (your super accurate Quectel GPS data)
	gpsReference := &GPSCoordinate{
		Latitude:  59.48007000,
		Longitude: 18.27985000,
		Accuracy:  0.4,
		Source:    "quectel_multi_gnss_reference",
	}

	// Hardcoded cellular data from your RutOS device
	cellularData := createHardcodedCellularData()

	fmt.Printf("📍 GPS Reference: %.8f°, %.8f° (±%.1fm)\n",
		gpsReference.Latitude, gpsReference.Longitude, gpsReference.Accuracy)
	fmt.Printf("📡 Cell Tower: %s (Telia Sweden, MCC:%s, MNC:%s)\n",
		cellularData.ServingCell.CellID, cellularData.ServingCell.MCC, cellularData.ServingCell.MNC)
	fmt.Printf("📊 Signal: RSSI %d, RSRP %d, RSRQ %d\n",
		cellularData.SignalQuality.RSSI, cellularData.SignalQuality.RSRP, cellularData.SignalQuality.RSRQ)

	// Test Mozilla Location Service
	fmt.Println("\n🦊 Testing Mozilla Location Service...")
	fmt.Println("-" + strings.Repeat("-", 35))

	mozillaResult := testMozillaLocationLocal(cellularData)

	// Test OpenCellID Service
	fmt.Println("\n🗼 Testing OpenCellID Service...")
	fmt.Println("-" + strings.Repeat("-", 30))

	openCellIDResult := testOpenCellIDLocal(cellularData)

	// Compare results
	fmt.Println("\n📊 COMPARISON RESULTS")
	fmt.Println("=" + strings.Repeat("=", 25))

	compareLocalResults(gpsReference, mozillaResult, openCellIDResult)

	// Save results
	saveLocalTestResults(gpsReference, cellularData, mozillaResult, openCellIDResult)

	return nil
}

// createHardcodedCellularData creates cellular data based on your RutOS device
func createHardcodedCellularData() *CellularLocationIntelligence {
	return &CellularLocationIntelligence{
		ServingCell: ServingCellInfo{
			CellID:   "25939743", // Your actual cell ID (hex 18BCF1F converted to decimal)
			TAC:      "23",       // Tracking Area Code
			PCID:     443,        // Physical Cell ID
			EARFCN:   1300,       // Frequency
			Band:     "B3",       // LTE Band 3
			MCC:      "240",      // Sweden
			MNC:      "01",       // Telia
			Operator: "Telia",
		},
		SignalQuality: SignalQuality{
			RSSI: -53, // Excellent signal
			RSRP: -84, // Reference Signal Received Power
			RSRQ: -8,  // Reference Signal Received Quality
			SINR: 17,  // Signal-to-Interference-plus-Noise Ratio
		},
		NetworkInfo: NetworkInfo{
			Operator:   "Telia",
			Technology: "5G-NSA",
			Band:       "B3",
			Registered: true,
		},
		// Add some neighbor cells for better triangulation
		NeighborCells: []NeighborCellInfo{
			{PCID: 444, EARFCN: 1300, RSSI: -67, CellType: "intra"},
			{PCID: 445, EARFCN: 1300, RSSI: -72, CellType: "intra"},
			{PCID: 446, EARFCN: 1300, RSSI: -78, CellType: "intra"},
			{PCID: 447, EARFCN: 1275, RSSI: -81, CellType: "inter"},
		},
		LocationFingerprint: LocationFingerprint{
			PrimaryCellID: "25939743",
			NeighborPCIDs: []int{444, 445, 446, 447},
			SignalPattern: "RSSI:-53,PCID:443,NEIGHBORS:4",
			LocationName:  "stockholm_area",
			Confidence:    0.9,
		},
		Timestamp:   time.Now().Unix(),
		CollectedAt: time.Now(),
		Valid:       true,
	}
}

// testMozillaLocationLocal tests Mozilla Location Service locally
func testMozillaLocationLocal(cellData *CellularLocationIntelligence) *CellTowerLocationResult {
	start := time.Now()
	result := &CellTowerLocationResult{}

	fmt.Println("  📡 Sending request to Mozilla Location Service...")

	location, err := getMozillaLocationLocal(cellData)
	if err != nil {
		result.Success = false
		result.ErrorDetails = err.Error()
		fmt.Printf("  ❌ Mozilla failed: %v\n", err)
	} else {
		result.CellTowerLocation = location
		result.Success = location.Valid
		if location.Valid {
			fmt.Printf("  ✅ Mozilla SUCCESS: %.6f°, %.6f° (±%.0fm) in %.1fms\n",
				location.Latitude, location.Longitude, location.Accuracy, location.ResponseTime)

			// Create Google Maps link
			mapsLink := fmt.Sprintf("https://www.google.com/maps?q=%.6f,%.6f", location.Latitude, location.Longitude)
			fmt.Printf("  🗺️  Mozilla Maps: %s\n", mapsLink)
		} else {
			fmt.Printf("  ❌ Mozilla returned invalid location\n")
		}
	}

	result.TestDuration = time.Since(start)
	return result
}

// testOpenCellIDLocal tests OpenCellID service locally
func testOpenCellIDLocal(cellData *CellularLocationIntelligence) *CellTowerLocationResult {
	start := time.Now()
	result := &CellTowerLocationResult{}

	fmt.Println("  📡 Sending request to OpenCellID...")

	location, err := getOpenCellIDLocationLocal(cellData)
	if err != nil {
		result.Success = false
		result.ErrorDetails = err.Error()
		fmt.Printf("  ❌ OpenCellID failed: %v\n", err)
	} else {
		result.CellTowerLocation = location
		result.Success = location.Valid
		if location.Valid {
			fmt.Printf("  ✅ OpenCellID SUCCESS: %.6f°, %.6f° (±%.0fm) in %.1fms\n",
				location.Latitude, location.Longitude, location.Accuracy, location.ResponseTime)

			// Create Google Maps link
			mapsLink := fmt.Sprintf("https://www.google.com/maps?q=%.6f,%.6f", location.Latitude, location.Longitude)
			fmt.Printf("  🗺️  OpenCellID Maps: %s\n", mapsLink)
		} else {
			fmt.Printf("  ❌ OpenCellID returned invalid location\n")
		}
	}

	result.TestDuration = time.Since(start)
	return result
}

// getMozillaLocationLocal gets location from Mozilla Location Service
func getMozillaLocationLocal(cellData *CellularLocationIntelligence) (*CellTowerLocation, error) {
	start := time.Now()
	location := &CellTowerLocation{
		Source:      "mozilla_location_service",
		Method:      "multi_cell_triangulation",
		CollectedAt: time.Now(),
	}

	// Prepare request with serving cell + neighbors
	request := MozillaLocationRequest{
		CellTowers: []MozillaCellTower{{
			RadioType:         "lte",
			MobileCountryCode: 240,
			MobileNetworkCode: 1,
			LocationAreaCode:  23,
			CellID:            25939743,
			SignalStrength:    cellData.SignalQuality.RSSI,
		}},
	}

	// Add neighbor cells for better triangulation
	for _, neighbor := range cellData.NeighborCells {
		request.CellTowers = append(request.CellTowers, MozillaCellTower{
			RadioType:         "lte",
			MobileCountryCode: 240,
			MobileNetworkCode: 1,
			LocationAreaCode:  23,
			CellID:            neighbor.PCID,
			SignalStrength:    neighbor.RSSI,
		})
	}

	fmt.Printf("    📊 Using %d cell towers for triangulation\n", len(request.CellTowers))

	// Make API request
	jsonData, _ := json.Marshal(request)
	resp, err := http.Post(
		"https://location.services.mozilla.com/v1/geolocate?key=test",
		"application/json",
		bytes.NewBuffer(jsonData),
	)
	if err != nil {
		location.Error = fmt.Sprintf("HTTP request failed: %v", err)
		return location, err
	}
	defer resp.Body.Close()

	location.ResponseTime = float64(time.Since(start).Nanoseconds()) / 1e6

	// Parse response
	body, err := io.ReadAll(resp.Body)
	if err != nil {
		location.Error = fmt.Sprintf("Failed to read response: %v", err)
		return location, err
	}

	fmt.Printf("    📡 Response: %s\n", string(body))

	var response MozillaLocationResponse
	if err := json.Unmarshal(body, &response); err != nil {
		location.Error = fmt.Sprintf("Failed to parse response: %v", err)
		return location, err
	}

	// Extract location data
	location.Latitude = response.Location.Lat
	location.Longitude = response.Location.Lng
	location.Accuracy = response.Accuracy
	location.Valid = location.Latitude != 0 && location.Longitude != 0
	location.Confidence = 0.8 // High confidence with multiple cells

	return location, nil
}

// getOpenCellIDLocationLocal gets location from OpenCellID
func getOpenCellIDLocationLocal(cellData *CellularLocationIntelligence) (*CellTowerLocation, error) {
	start := time.Now()
	location := &CellTowerLocation{
		Source:      "opencellid",
		Method:      "database_lookup",
		CollectedAt: time.Now(),
	}

	// Load API token
	apiKey, err := loadOpenCellIDTokenLocal()
	if err != nil {
		location.Error = fmt.Sprintf("Failed to load API key: %v", err)
		return location, err
	}

	// Prepare request
	request := OpenCellIDRequest{
		Token: apiKey,
		Radio: "LTE",
		MCC:   240,
		MNC:   1,
		Cells: []struct {
			LAC int `json:"lac"`
			CID int `json:"cid"`
		}{{
			LAC: 23,
			CID: 25939743,
		}},
	}

	fmt.Printf("    📊 Looking up Cell ID: %d (MCC:240, MNC:1, LAC:23)\n", 25939743)

	// Make API request
	jsonData, _ := json.Marshal(request)
	resp, err := http.Post(
		"https://us1.unwiredlabs.com/v2/process.php",
		"application/json",
		bytes.NewBuffer(jsonData),
	)
	if err != nil {
		location.Error = fmt.Sprintf("HTTP request failed: %v", err)
		return location, err
	}
	defer resp.Body.Close()

	location.ResponseTime = float64(time.Since(start).Nanoseconds()) / 1e6

	// Parse response
	body, err := io.ReadAll(resp.Body)
	if err != nil {
		location.Error = fmt.Sprintf("Failed to read response: %v", err)
		return location, err
	}

	fmt.Printf("    📡 Response: %s\n", string(body))

	var response OpenCellIDResponse
	if err := json.Unmarshal(body, &response); err != nil {
		location.Error = fmt.Sprintf("Failed to parse response: %v", err)
		return location, err
	}

	if response.Status != "ok" {
		location.Error = fmt.Sprintf("API error: %s", response.Message)
		return location, fmt.Errorf("API error: %s", response.Message)
	}

	// Extract location data
	location.Latitude = response.Lat
	location.Longitude = response.Lon
	location.Accuracy = response.Accuracy
	location.Valid = location.Latitude != 0 && location.Longitude != 0
	location.Confidence = 0.85 // High confidence for OpenCellID

	return location, nil
}

// loadOpenCellIDTokenLocal loads the API token from file
func loadOpenCellIDTokenLocal() (string, error) {
	tokenFile := `C:\Users\markusla\OneDrive\IT\RUTOS Keys\OpenCELLID.txt`

	data, err := os.ReadFile(tokenFile)
	if err != nil {
		return "", fmt.Errorf("failed to read token file: %v", err)
	}

	token := strings.TrimSpace(string(data))
	if token == "" {
		return "", fmt.Errorf("token file is empty")
	}

	return token, nil
}

// compareLocalResults compares the results from both services
func compareLocalResults(gpsRef *GPSCoordinate, mozilla, openCellID *CellTowerLocationResult) {
	fmt.Printf("\n📍 GPS Reference: %.8f°, %.8f° (±%.1fm)\n",
		gpsRef.Latitude, gpsRef.Longitude, gpsRef.Accuracy)

	var mozillaDistance, openCellIDDistance float64 = -1, -1

	if mozilla.Success {
		mozillaDistance = calculateDistance(gpsRef.Latitude, gpsRef.Longitude,
			mozilla.Latitude, mozilla.Longitude)
		fmt.Printf("🦊 Mozilla: %.6f°, %.6f° → %.0fm from GPS\n",
			mozilla.Latitude, mozilla.Longitude, mozillaDistance)
	} else {
		fmt.Printf("🦊 Mozilla: FAILED - %s\n", mozilla.ErrorDetails)
	}

	if openCellID.Success {
		openCellIDDistance = calculateDistance(gpsRef.Latitude, gpsRef.Longitude,
			openCellID.Latitude, openCellID.Longitude)
		fmt.Printf("🗼 OpenCellID: %.6f°, %.6f° → %.0fm from GPS\n",
			openCellID.Latitude, openCellID.Longitude, openCellIDDistance)
	} else {
		fmt.Printf("🗼 OpenCellID: FAILED - %s\n", openCellID.ErrorDetails)
	}

	// Determine winner
	fmt.Println("\n🏆 WINNER:")
	if mozillaDistance >= 0 && openCellIDDistance >= 0 {
		if mozillaDistance < openCellIDDistance {
			fmt.Printf("   🦊 Mozilla Location Service (%.0fm more accurate)\n",
				openCellIDDistance-mozillaDistance)
		} else if openCellIDDistance < mozillaDistance {
			fmt.Printf("   🗼 OpenCellID (%.0fm more accurate)\n",
				mozillaDistance-openCellIDDistance)
		} else {
			fmt.Printf("   🤝 TIE - Both services equally accurate\n")
		}
	} else if mozillaDistance >= 0 {
		fmt.Printf("   🦊 Mozilla Location Service (only working service)\n")
	} else if openCellIDDistance >= 0 {
		fmt.Printf("   🗼 OpenCellID (only working service)\n")
	} else {
		fmt.Printf("   ❌ Neither service worked\n")
	}

	// Recommendation
	fmt.Println("\n💡 RECOMMENDATION:")
	if mozillaDistance >= 0 && openCellIDDistance >= 0 {
		if mozillaDistance < 1000 && openCellIDDistance < 1000 {
			fmt.Printf("   ✅ Both services are accurate enough for location fallback\n")
			fmt.Printf("   🆓 Use Mozilla for free production deployment\n")
			fmt.Printf("   🎯 Use OpenCellID for higher accuracy needs\n")
		} else {
			fmt.Printf("   ⚠️  Cell tower location may not be accurate enough for your area\n")
		}
	} else {
		fmt.Printf("   ❌ Cell tower location services not reliable for your area\n")
	}

	// Google Maps comparison link
	if mozillaDistance >= 0 && openCellIDDistance >= 0 {
		fmt.Printf("\n🗺️  Compare all locations on map:\n")
		fmt.Printf("   GPS: https://www.google.com/maps?q=%.8f,%.8f\n", gpsRef.Latitude, gpsRef.Longitude)
		fmt.Printf("   Mozilla: https://www.google.com/maps?q=%.6f,%.6f\n", mozilla.Latitude, mozilla.Longitude)
		fmt.Printf("   OpenCellID: https://www.google.com/maps?q=%.6f,%.6f\n", openCellID.Latitude, openCellID.Longitude)
	}
}

// saveLocalTestResults saves the test results to a JSON file
func saveLocalTestResults(gpsRef *GPSCoordinate, cellData *CellularLocationIntelligence,
	mozilla, openCellID *CellTowerLocationResult,
) {
	results := map[string]interface{}{
		"test_type":         "local_cell_tower_accuracy",
		"timestamp":         time.Now().Format(time.RFC3339),
		"gps_reference":     gpsRef,
		"cellular_data":     cellData,
		"mozilla_result":    mozilla,
		"opencellid_result": openCellID,
	}

	filename := fmt.Sprintf("local_cell_tower_test_%s.json",
		time.Now().Format("2006-01-02_15-04-05"))

	data, err := json.MarshalIndent(results, "", "  ")
	if err != nil {
		fmt.Printf("❌ Failed to marshal results: %v\n", err)
		return
	}

	if err := os.WriteFile(filename, data, 0o644); err != nil {
		fmt.Printf("❌ Failed to save results: %v\n", err)
		return
	}

	fmt.Printf("\n💾 Results saved to: %s\n", filename)
}
