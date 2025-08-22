package main

import (
	"bufio"
	"bytes"
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"os"
	"strconv"
	"strings"
	"time"

	"golang.org/x/crypto/ssh"
)

// CellTowerAccuracyTest represents a comprehensive test of cell tower location services
type CellTowerAccuracyTest struct {
	TestStarted      time.Time                     `json:"test_started"`
	GPSReference     *GPSCoordinate                `json:"gps_reference"`
	CellularData     *CellularLocationIntelligence `json:"cellular_data"`
	MozillaResult    *CellTowerLocationResult      `json:"mozilla_result"`
	OpenCellIDResult *CellTowerLocationResult      `json:"opencellid_result"`
	Comparison       *LocationServiceComparison    `json:"comparison"`
}

type CellTowerLocationResult struct {
	*CellTowerLocation
	TestDuration time.Duration `json:"test_duration"`
	Success      bool          `json:"success"`
	ErrorDetails string        `json:"error_details,omitempty"`
}

type LocationServiceComparison struct {
	MozillaAccuracy    float64 `json:"mozilla_accuracy_meters"`
	OpenCellIDAccuracy float64 `json:"opencellid_accuracy_meters"`
	BetterService      string  `json:"better_service"`
	AccuracyDifference float64 `json:"accuracy_difference_meters"`
	RecommendedService string  `json:"recommended_service"`
	Summary            string  `json:"summary"`
}

type GPSCoordinate struct {
	Latitude  float64 `json:"latitude"`
	Longitude float64 `json:"longitude"`
	Accuracy  float64 `json:"accuracy"`
	Source    string  `json:"source"`
}

// loadOpenCellIDToken loads the API token from the specified file
func loadOpenCellIDToken() (string, error) {
	tokenFile := `C:\Users\markusla\OneDrive\IT\RUTOS Keys\OpenCELLID.txt`

	file, err := os.Open(tokenFile)
	if err != nil {
		return "", fmt.Errorf("failed to open token file: %v", err)
	}
	defer file.Close()

	scanner := bufio.NewScanner(file)
	if scanner.Scan() {
		token := strings.TrimSpace(scanner.Text())
		if token == "" {
			return "", fmt.Errorf("token file is empty")
		}
		return token, nil
	}

	return "", fmt.Errorf("failed to read token from file")
}

// testCellTowerLocationAccuracy runs comprehensive cell tower location testing
func testCellTowerLocationAccuracy(client *ssh.Client) error {
	fmt.Println("🎯 COMPREHENSIVE CELL TOWER LOCATION ACCURACY TEST")
	fmt.Println("=" + strings.Repeat("=", 55))

	test := &CellTowerAccuracyTest{
		TestStarted: time.Now(),
	}

	// Step 1: Get super accurate GPS reference
	fmt.Println("\n📍 Step 1: Getting GPS Reference Location")
	fmt.Println("-" + strings.Repeat("-", 40))

	gpsRef, err := getGPSReference(client)
	if err != nil {
		return fmt.Errorf("failed to get GPS reference: %v", err)
	}
	test.GPSReference = gpsRef

	fmt.Printf("✅ GPS Reference: %.8f°, %.8f° (±%.1fm)\n",
		gpsRef.Latitude, gpsRef.Longitude, gpsRef.Accuracy)

	// Step 2: Collect comprehensive cellular data
	fmt.Println("\n📡 Step 2: Collecting Comprehensive Cellular Data")
	fmt.Println("-" + strings.Repeat("-", 45))

	cellData, err := collectEnhancedCellularData(client)
	if err != nil {
		return fmt.Errorf("failed to collect cellular data: %v", err)
	}
	test.CellularData = cellData

	// Step 3: Test Mozilla Location Service
	fmt.Println("\n🦊 Step 3: Testing Mozilla Location Service")
	fmt.Println("-" + strings.Repeat("-", 40))

	mozillaResult := testMozillaLocationService(cellData)
	test.MozillaResult = mozillaResult

	// Step 4: Test OpenCellID Service
	fmt.Println("\n🗼 Step 4: Testing OpenCellID Service")
	fmt.Println("-" + strings.Repeat("-", 35))

	openCellIDResult := testOpenCellIDService(cellData)
	test.OpenCellIDResult = openCellIDResult

	// Step 5: Compare results
	fmt.Println("\n📊 Step 5: Comparing Results")
	fmt.Println("-" + strings.Repeat("-", 30))

	comparison := compareLocationServices(test)
	test.Comparison = comparison

	// Step 6: Display comprehensive results
	displayComprehensiveResults(test)

	// Step 7: Save results to file
	saveTestResults(test)

	return nil
}

// getGPSReference gets the most accurate GPS coordinates available
func getGPSReference(client *ssh.Client) (*GPSCoordinate, error) {
	// Try Quectel GPS first (most accurate)
	if quectelData, err := testQuectelGPS(client); err == nil && quectelData.Valid {
		// Convert HDOP to approximate accuracy in meters (HDOP * 5 is a rough estimate)
		accuracy := quectelData.HDOP * 5.0
		if accuracy == 0 {
			accuracy = 1.0 // Default to 1m if HDOP is 0
		}
		return &GPSCoordinate{
			Latitude:  quectelData.Latitude,
			Longitude: quectelData.Longitude,
			Accuracy:  accuracy,
			Source:    "quectel_gsm_gps",
		}, nil
	}

	// Fallback to enhanced GPS data
	if enhancedData, err := collectEnhancedGPSData(client); err == nil && enhancedData.Valid {
		return &GPSCoordinate{
			Latitude:  enhancedData.Latitude,
			Longitude: enhancedData.Longitude,
			Accuracy:  enhancedData.Accuracy,
			Source:    "enhanced_gps",
		}, nil
	}

	return nil, fmt.Errorf("no GPS coordinates available")
}

// collectEnhancedCellularData collects all available cellular information
func collectEnhancedCellularData(client *ssh.Client) (*CellularLocationIntelligence, error) {
	intel, err := collectCellularLocationIntelligence(client)
	if err != nil {
		return nil, err
	}

	// Enhance with additional gsmctl data
	enhanceCellularDataWithGsmctl(client, intel)

	return intel, nil
}

// enhanceCellularDataWithGsmctl adds more cellular data using gsmctl commands
func enhanceCellularDataWithGsmctl(client *ssh.Client, intel *CellularLocationIntelligence) {
	fmt.Println("🔍 Enhancing cellular data with additional gsmctl commands...")

	// Get additional cell tower information
	commands := map[string]string{
		"cell_info":     "gsmctl -A 'AT+QENG=\"servingcell\"'",
		"neighbor_info": "gsmctl -A 'AT+QENG=\"neighbourcell\"'",
		"signal_info":   "gsmctl -q",
		"network_info":  "gsmctl -F",
		"operator_info": "gsmctl -o",
		"registration":  "gsmctl -A 'AT+CREG?'",
		"technology":    "gsmctl -A 'AT+QNWINFO'",
	}

	for name, cmd := range commands {
		if output, err := executeCommand(client, cmd); err == nil {
			fmt.Printf("  ✅ %s: %d bytes\n", name, len(output))
		} else {
			fmt.Printf("  ❌ %s: %v\n", name, err)
		}
	}
}

// testMozillaLocationService tests Mozilla Location Service
func testMozillaLocationService(intel *CellularLocationIntelligence) *CellTowerLocationResult {
	start := time.Now()
	result := &CellTowerLocationResult{}

	location, err := getMozillaLocationEnhanced(intel)
	if err != nil {
		result.Success = false
		result.ErrorDetails = err.Error()
		fmt.Printf("❌ Mozilla failed: %v\n", err)
	} else {
		result.CellTowerLocation = location
		result.Success = location.Valid
		if location.Valid {
			fmt.Printf("✅ Mozilla: %.6f°, %.6f° (±%.0fm) in %.1fms\n",
				location.Latitude, location.Longitude, location.Accuracy, location.ResponseTime)
		}
	}

	result.TestDuration = time.Since(start)
	return result
}

// testOpenCellIDService tests OpenCellID service
func testOpenCellIDService(intel *CellularLocationIntelligence) *CellTowerLocationResult {
	start := time.Now()
	result := &CellTowerLocationResult{}

	location, err := getOpenCellIDLocationEnhanced(intel)
	if err != nil {
		result.Success = false
		result.ErrorDetails = err.Error()
		fmt.Printf("❌ OpenCellID failed: %v\n", err)
	} else {
		result.CellTowerLocation = location
		result.Success = location.Valid
		if location.Valid {
			fmt.Printf("✅ OpenCellID: %.6f°, %.6f° (±%.0fm) in %.1fms\n",
				location.Latitude, location.Longitude, location.Accuracy, location.ResponseTime)
		}
	}

	result.TestDuration = time.Since(start)
	return result
}

// getMozillaLocationEnhanced gets location from Mozilla with enhanced data
func getMozillaLocationEnhanced(intel *CellularLocationIntelligence) (*CellTowerLocation, error) {
	start := time.Now()
	location := &CellTowerLocation{
		Source:      "mozilla_location_service",
		Method:      "enhanced_triangulation",
		CollectedAt: time.Now(),
	}

	// Parse cell data
	cellID, _ := strconv.Atoi(intel.ServingCell.CellID)
	mcc, _ := strconv.Atoi(intel.ServingCell.MCC)
	mnc, _ := strconv.Atoi(intel.ServingCell.MNC)
	lac, _ := strconv.Atoi(intel.ServingCell.TAC)

	// Prepare enhanced request with all available data
	request := MozillaLocationRequest{
		CellTowers: []MozillaCellTower{{
			RadioType:         "lte",
			MobileCountryCode: mcc,
			MobileNetworkCode: mnc,
			LocationAreaCode:  lac,
			CellID:            cellID,
			SignalStrength:    intel.SignalQuality.RSSI,
		}},
	}

	// Add ALL neighbor cells for maximum triangulation accuracy
	for _, neighbor := range intel.NeighborCells {
		if len(request.CellTowers) >= 10 { // Allow more cells for better accuracy
			break
		}
		request.CellTowers = append(request.CellTowers, MozillaCellTower{
			RadioType:         "lte",
			MobileCountryCode: mcc,
			MobileNetworkCode: mnc,
			LocationAreaCode:  lac,
			CellID:            neighbor.PCID,
			SignalStrength:    neighbor.RSSI,
		})
	}

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
	location.Confidence = 0.7

	if len(request.CellTowers) > 1 {
		location.Method = "multi_cell_triangulation"
		location.Confidence = 0.8 + (float64(len(request.CellTowers)) * 0.02) // Higher confidence with more cells
	}

	return location, nil
}

// getOpenCellIDLocationEnhanced gets location from OpenCellID with token
func getOpenCellIDLocationEnhanced(intel *CellularLocationIntelligence) (*CellTowerLocation, error) {
	start := time.Now()
	location := &CellTowerLocation{
		Source:      "opencellid",
		Method:      "enhanced_lookup",
		CollectedAt: time.Now(),
	}

	// Load API token
	apiKey, err := loadOpenCellIDToken()
	if err != nil {
		location.Error = fmt.Sprintf("Failed to load API key: %v", err)
		return location, err
	}

	// Parse cell data
	cellID, _ := strconv.Atoi(intel.ServingCell.CellID)
	mcc, _ := strconv.Atoi(intel.ServingCell.MCC)
	mnc, _ := strconv.Atoi(intel.ServingCell.MNC)
	lac, _ := strconv.Atoi(intel.ServingCell.TAC)

	// Prepare request
	request := OpenCellIDRequest{
		Token: apiKey,
		Radio: "LTE",
		MCC:   mcc,
		MNC:   mnc,
		Cells: []struct {
			LAC int `json:"lac"`
			CID int `json:"cid"`
		}{{
			LAC: lac,
			CID: cellID,
		}},
	}

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

// compareLocationServices compares the accuracy of both services
func compareLocationServices(test *CellTowerAccuracyTest) *LocationServiceComparison {
	comparison := &LocationServiceComparison{}

	gpsLat := test.GPSReference.Latitude
	gpsLon := test.GPSReference.Longitude

	// Calculate distances from GPS reference
	if test.MozillaResult.Success {
		comparison.MozillaAccuracy = calculateDistance(
			gpsLat, gpsLon,
			test.MozillaResult.Latitude, test.MozillaResult.Longitude,
		)
	} else {
		comparison.MozillaAccuracy = -1 // Indicate failure
	}

	if test.OpenCellIDResult.Success {
		comparison.OpenCellIDAccuracy = calculateDistance(
			gpsLat, gpsLon,
			test.OpenCellIDResult.Latitude, test.OpenCellIDResult.Longitude,
		)
	} else {
		comparison.OpenCellIDAccuracy = -1 // Indicate failure
	}

	// Determine better service
	if comparison.MozillaAccuracy >= 0 && comparison.OpenCellIDAccuracy >= 0 {
		if comparison.MozillaAccuracy < comparison.OpenCellIDAccuracy {
			comparison.BetterService = "Mozilla Location Service"
			comparison.AccuracyDifference = comparison.OpenCellIDAccuracy - comparison.MozillaAccuracy
		} else {
			comparison.BetterService = "OpenCellID"
			comparison.AccuracyDifference = comparison.MozillaAccuracy - comparison.OpenCellIDAccuracy
		}
	} else if comparison.MozillaAccuracy >= 0 {
		comparison.BetterService = "Mozilla Location Service"
		comparison.AccuracyDifference = 0
	} else if comparison.OpenCellIDAccuracy >= 0 {
		comparison.BetterService = "OpenCellID"
		comparison.AccuracyDifference = 0
	} else {
		comparison.BetterService = "Neither (both failed)"
		comparison.AccuracyDifference = 0
	}

	// Generate recommendation
	if comparison.MozillaAccuracy >= 0 && comparison.OpenCellIDAccuracy >= 0 {
		if comparison.MozillaAccuracy < 500 && comparison.OpenCellIDAccuracy < 500 {
			comparison.RecommendedService = "Both services are accurate enough for most use cases"
		} else if comparison.MozillaAccuracy < comparison.OpenCellIDAccuracy {
			comparison.RecommendedService = "Mozilla Location Service (more accurate)"
		} else {
			comparison.RecommendedService = "OpenCellID (more accurate)"
		}
	} else if comparison.MozillaAccuracy >= 0 {
		comparison.RecommendedService = "Mozilla Location Service (only working service)"
	} else if comparison.OpenCellIDAccuracy >= 0 {
		comparison.RecommendedService = "OpenCellID (only working service)"
	} else {
		comparison.RecommendedService = "Neither service is working properly"
	}

	// Generate summary
	comparison.Summary = generateComparisonSummary(comparison)

	return comparison
}

// generateComparisonSummary creates a human-readable summary
func generateComparisonSummary(comp *LocationServiceComparison) string {
	if comp.MozillaAccuracy < 0 && comp.OpenCellIDAccuracy < 0 {
		return "Both services failed to provide location data"
	}

	if comp.MozillaAccuracy < 0 {
		return fmt.Sprintf("Only OpenCellID worked (%.0fm accuracy)", comp.OpenCellIDAccuracy)
	}

	if comp.OpenCellIDAccuracy < 0 {
		return fmt.Sprintf("Only Mozilla worked (%.0fm accuracy)", comp.MozillaAccuracy)
	}

	return fmt.Sprintf("Mozilla: %.0fm, OpenCellID: %.0fm accuracy. %s is %.0fm more accurate.",
		comp.MozillaAccuracy, comp.OpenCellIDAccuracy, comp.BetterService, comp.AccuracyDifference)
}

// displayComprehensiveResults shows detailed test results
func displayComprehensiveResults(test *CellTowerAccuracyTest) {
	fmt.Println("\n" + strings.Repeat("=", 60))
	fmt.Println("🎯 COMPREHENSIVE CELL TOWER LOCATION TEST RESULTS")
	fmt.Println(strings.Repeat("=", 60))

	// GPS Reference
	fmt.Printf("\n📍 GPS Reference Location:\n")
	fmt.Printf("  Coordinates: %.8f°, %.8f°\n", test.GPSReference.Latitude, test.GPSReference.Longitude)
	fmt.Printf("  Accuracy: ±%.1f meters\n", test.GPSReference.Accuracy)
	fmt.Printf("  Source: %s\n", test.GPSReference.Source)

	// Cellular Data Summary
	fmt.Printf("\n📡 Cellular Data Summary:\n")
	fmt.Printf("  Cell ID: %s\n", test.CellularData.ServingCell.CellID)
	fmt.Printf("  Network: %s %s (MCC:%s, MNC:%s)\n",
		test.CellularData.ServingCell.Operator, test.CellularData.NetworkInfo.Technology,
		test.CellularData.ServingCell.MCC, test.CellularData.ServingCell.MNC)
	fmt.Printf("  Signal: RSSI %d, RSRP %d, RSRQ %d\n",
		test.CellularData.SignalQuality.RSSI, test.CellularData.SignalQuality.RSRP, test.CellularData.SignalQuality.RSRQ)
	fmt.Printf("  Neighbor Cells: %d detected\n", len(test.CellularData.NeighborCells))

	// Mozilla Results
	fmt.Printf("\n🦊 Mozilla Location Service Results:\n")
	if test.MozillaResult.Success {
		fmt.Printf("  ✅ SUCCESS\n")
		fmt.Printf("  Coordinates: %.6f°, %.6f°\n", test.MozillaResult.Latitude, test.MozillaResult.Longitude)
		fmt.Printf("  Claimed Accuracy: ±%.0f meters\n", test.MozillaResult.Accuracy)
		fmt.Printf("  Actual Accuracy: %.0f meters from GPS\n", test.Comparison.MozillaAccuracy)
		fmt.Printf("  Response Time: %.1f ms\n", test.MozillaResult.ResponseTime)
		fmt.Printf("  Method: %s\n", test.MozillaResult.Method)
	} else {
		fmt.Printf("  ❌ FAILED: %s\n", test.MozillaResult.ErrorDetails)
	}

	// OpenCellID Results
	fmt.Printf("\n🗼 OpenCellID Results:\n")
	if test.OpenCellIDResult.Success {
		fmt.Printf("  ✅ SUCCESS\n")
		fmt.Printf("  Coordinates: %.6f°, %.6f°\n", test.OpenCellIDResult.Latitude, test.OpenCellIDResult.Longitude)
		fmt.Printf("  Claimed Accuracy: ±%.0f meters\n", test.OpenCellIDResult.Accuracy)
		fmt.Printf("  Actual Accuracy: %.0f meters from GPS\n", test.Comparison.OpenCellIDAccuracy)
		fmt.Printf("  Response Time: %.1f ms\n", test.OpenCellIDResult.ResponseTime)
		fmt.Printf("  Method: %s\n", test.OpenCellIDResult.Method)
	} else {
		fmt.Printf("  ❌ FAILED: %s\n", test.OpenCellIDResult.ErrorDetails)
	}

	// Comparison
	fmt.Printf("\n📊 Service Comparison:\n")
	fmt.Printf("  Winner: %s\n", test.Comparison.BetterService)
	fmt.Printf("  Accuracy Difference: %.0f meters\n", test.Comparison.AccuracyDifference)
	fmt.Printf("  Recommendation: %s\n", test.Comparison.RecommendedService)
	fmt.Printf("  Summary: %s\n", test.Comparison.Summary)

	// Google Maps Links
	fmt.Printf("\n🗺️  Google Maps Links:\n")
	fmt.Printf("  GPS Reference: https://www.google.com/maps?q=%.8f,%.8f\n",
		test.GPSReference.Latitude, test.GPSReference.Longitude)
	if test.MozillaResult.Success {
		fmt.Printf("  Mozilla Result: https://www.google.com/maps?q=%.6f,%.6f\n",
			test.MozillaResult.Latitude, test.MozillaResult.Longitude)
	}
	if test.OpenCellIDResult.Success {
		fmt.Printf("  OpenCellID Result: https://www.google.com/maps?q=%.6f,%.6f\n",
			test.OpenCellIDResult.Latitude, test.OpenCellIDResult.Longitude)
	}

	fmt.Println("\n" + strings.Repeat("=", 60))
}

// saveTestResults saves the test results to a JSON file
func saveTestResults(test *CellTowerAccuracyTest) {
	filename := fmt.Sprintf("cell_tower_test_%s.json",
		test.TestStarted.Format("2006-01-02_15-04-05"))

	data, err := json.MarshalIndent(test, "", "  ")
	if err != nil {
		fmt.Printf("❌ Failed to marshal test results: %v\n", err)
		return
	}

	if err := os.WriteFile(filename, data, 0o644); err != nil {
		fmt.Printf("❌ Failed to save test results: %v\n", err)
		return
	}

	fmt.Printf("💾 Test results saved to: %s\n", filename)
}
