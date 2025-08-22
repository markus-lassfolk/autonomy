package main

import (
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"net/url"
	"strconv"
	"strings"
	"time"
)

// OpenCellIDCorrectResponse represents the correct OpenCellID API response format
type OpenCellIDCorrectResponse struct {
	Lat     float64 `json:"lat"`
	Lon     float64 `json:"lon"`
	MCC     int     `json:"mcc"`
	MNC     int     `json:"mnc"`
	LAC     int     `json:"lac"`
	CellID  int     `json:"cellid"`
	Range   int     `json:"range"`
	Samples int     `json:"samples"`
	Radio   string  `json:"radio"`
	Address string  `json:"address,omitempty"`
	Error   string  `json:"error,omitempty"`
	Message string  `json:"message,omitempty"`
}

// getOpenCellIDLocationCorrect uses the correct OpenCellID API format
func getOpenCellIDLocationCorrect(cellData *CellularLocationIntelligence) (*CellTowerLocation, error) {
	start := time.Now()
	location := &CellTowerLocation{
		Source:      "opencellid_correct_api",
		Method:      "get_cell_position",
		CollectedAt: time.Now(),
	}

	// Load API token
	apiKey, err := loadOpenCellIDTokenLocal()
	if err != nil {
		location.Error = fmt.Sprintf("Failed to load API key: %v", err)
		return location, err
	}

	// Parse cell data
	cellID, _ := strconv.Atoi(cellData.ServingCell.CellID)
	mcc, _ := strconv.Atoi(cellData.ServingCell.MCC)
	mnc, _ := strconv.Atoi(cellData.ServingCell.MNC)
	lac, _ := strconv.Atoi(cellData.ServingCell.TAC)

	// Build the correct OpenCellID API URL according to documentation
	// GET https://opencellid.org/cell/get?key=<apiKey>&mcc=<mcc>&mnc=<mnc>&lac=<lac>&cellid=<cellid>&format=json
	baseURL := "https://opencellid.org/cell/get"
	params := url.Values{}
	params.Add("key", apiKey)
	params.Add("mcc", strconv.Itoa(mcc))
	params.Add("mnc", strconv.Itoa(mnc))
	params.Add("lac", strconv.Itoa(lac))
	params.Add("cellid", strconv.Itoa(cellID))
	params.Add("format", "json")

	fullURL := baseURL + "?" + params.Encode()

	fmt.Printf("    📡 OpenCellID URL: %s\n", fullURL)

	// Make GET request (not POST as we were doing before)
	resp, err := http.Get(fullURL)
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

	var response OpenCellIDCorrectResponse
	if err := json.Unmarshal(body, &response); err != nil {
		location.Error = fmt.Sprintf("Failed to parse response: %v", err)
		return location, err
	}

	// Check for API errors
	if response.Error != "" {
		location.Error = fmt.Sprintf("API error: %s", response.Error)
		return location, fmt.Errorf("API error: %s", response.Error)
	}

	if response.Message != "" {
		location.Error = fmt.Sprintf("API message: %s", response.Message)
		return location, fmt.Errorf("API message: %s", response.Message)
	}

	// Extract location data
	location.Latitude = response.Lat
	location.Longitude = response.Lon
	location.Accuracy = float64(response.Range) // Range is the accuracy radius
	location.Valid = location.Latitude != 0 && location.Longitude != 0
	location.Confidence = 0.85 // High confidence for OpenCellID

	return location, nil
}

// testOpenCellIDCorrectAPI tests the corrected OpenCellID API
func testOpenCellIDCorrectAPI() error {
	fmt.Println("🗼 Testing CORRECTED OpenCellID API")
	fmt.Println("=" + strings.Repeat("=", 35))

	// Create test cellular data
	cellData := createHardcodedCellularData()

	fmt.Printf("📡 Testing Cell Tower: %s (MCC:%s, MNC:%s, LAC:%s)\n",
		cellData.ServingCell.CellID, cellData.ServingCell.MCC,
		cellData.ServingCell.MNC, cellData.ServingCell.TAC)

	// Test the corrected API
	location, err := getOpenCellIDLocationCorrect(cellData)
	if err != nil {
		fmt.Printf("❌ OpenCellID failed: %v\n", err)
		return err
	}

	if location.Valid {
		fmt.Printf("✅ OpenCellID SUCCESS: %.6f°, %.6f° (±%.0fm) in %.1fms\n",
			location.Latitude, location.Longitude, location.Accuracy, location.ResponseTime)

		// Calculate distance from known GPS
		gpsLat := 59.48007000
		gpsLon := 18.27985000
		distance := calculateDistance(gpsLat, gpsLon, location.Latitude, location.Longitude)
		fmt.Printf("📏 Distance from GPS: %.0fm\n", distance)

		// Create Google Maps link
		mapsLink := fmt.Sprintf("https://www.google.com/maps?q=%.6f,%.6f", location.Latitude, location.Longitude)
		fmt.Printf("🗺️  Maps Link: %s\n", mapsLink)

		// Accuracy assessment
		if distance < 500 {
			fmt.Printf("🎯 EXCELLENT: Very accurate for cell tower location\n")
		} else if distance < 1000 {
			fmt.Printf("✅ GOOD: Acceptable accuracy for area detection\n")
		} else if distance < 5000 {
			fmt.Printf("⚠️  FAIR: Rough area location only\n")
		} else {
			fmt.Printf("❌ POOR: Location may be inaccurate\n")
		}
	} else {
		fmt.Printf("❌ OpenCellID returned invalid location\n")
	}

	return nil
}

// testAlternativeOpenCellIDQueries tests different query approaches
func testAlternativeOpenCellIDQueries() error {
	fmt.Println("\n🔍 Testing Alternative OpenCellID Query Methods")
	fmt.Println("=" + strings.Repeat("=", 50))

	apiKey, err := loadOpenCellIDTokenLocal()
	if err != nil {
		return fmt.Errorf("failed to load API key: %v", err)
	}

	// Test different combinations based on your cell tower data
	testCases := []struct {
		name string
		mcc  int
		mnc  int
		lac  int
		cid  int
	}{
		{"Your Exact Cell", 240, 1, 23, 25939743},
		{"Alternative LAC", 240, 1, 1, 25939743},
		{"Hex LAC", 240, 1, 0x17, 25939743},             // 23 in hex
		{"Different CID format", 240, 1, 23, 0x18BCF1F}, // Original hex
		{"Telia Common", 240, 1, 100, 25939743},
	}

	for _, tc := range testCases {
		fmt.Printf("\n🧪 Testing %s (MCC:%d, MNC:%d, LAC:%d, CID:%d):\n",
			tc.name, tc.mcc, tc.mnc, tc.lac, tc.cid)

		baseURL := "https://opencellid.org/cell/get"
		params := url.Values{}
		params.Add("key", apiKey)
		params.Add("mcc", strconv.Itoa(tc.mcc))
		params.Add("mnc", strconv.Itoa(tc.mnc))
		params.Add("lac", strconv.Itoa(tc.lac))
		params.Add("cellid", strconv.Itoa(tc.cid))
		params.Add("format", "json")

		fullURL := baseURL + "?" + params.Encode()

		resp, err := http.Get(fullURL)
		if err != nil {
			fmt.Printf("  ❌ HTTP Error: %v\n", err)
			continue
		}
		defer resp.Body.Close()

		body, err := io.ReadAll(resp.Body)
		if err != nil {
			fmt.Printf("  ❌ Read Error: %v\n", err)
			continue
		}

		fmt.Printf("  📡 Response: %s\n", string(body))

		var response OpenCellIDCorrectResponse
		if err := json.Unmarshal(body, &response); err != nil {
			fmt.Printf("  ❌ JSON Error: %v\n", err)
			continue
		}

		if response.Lat != 0 && response.Lon != 0 {
			distance := calculateDistance(59.48007000, 18.27985000, response.Lat, response.Lon)
			fmt.Printf("  ✅ SUCCESS: %.6f°, %.6f° (±%dm, %d samples, %.0fm from GPS)\n",
				response.Lat, response.Lon, response.Range, response.Samples, distance)
		} else if response.Error != "" {
			fmt.Printf("  ❌ API Error: %s\n", response.Error)
		} else if response.Message != "" {
			fmt.Printf("  ❌ API Message: %s\n", response.Message)
		} else {
			fmt.Printf("  ❌ No location data returned\n")
		}
	}

	return nil
}

// testOpenCellIDAreaSearch tests the area search functionality
func testOpenCellIDAreaSearch() error {
	fmt.Println("\n🗺️  Testing OpenCellID Area Search")
	fmt.Println("=" + strings.Repeat("=", 35))

	apiKey, err := loadOpenCellIDTokenLocal()
	if err != nil {
		return fmt.Errorf("failed to load API key: %v", err)
	}

	// Search for cells in the Stockholm area around your GPS coordinates
	gpsLat := 59.48007000
	gpsLon := 18.27985000

	// Create a bounding box around your location (±0.01 degrees ≈ ±1km)
	latMin := gpsLat - 0.01
	latMax := gpsLat + 0.01
	lonMin := gpsLon - 0.01
	lonMax := gpsLon + 0.01

	baseURL := "https://opencellid.org/cell/getInArea"
	params := url.Values{}
	params.Add("key", apiKey)
	params.Add("BBOX", fmt.Sprintf("%.6f,%.6f,%.6f,%.6f", latMin, lonMin, latMax, lonMax))
	params.Add("mcc", "240") // Sweden
	params.Add("mnc", "1")   // Telia
	params.Add("format", "json")
	params.Add("limit", "10") // Limit results

	fullURL := baseURL + "?" + params.Encode()
	fmt.Printf("📡 Area Search URL: %s\n", fullURL)

	resp, err := http.Get(fullURL)
	if err != nil {
		return fmt.Errorf("HTTP request failed: %v", err)
	}
	defer resp.Body.Close()

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return fmt.Errorf("failed to read response: %v", err)
	}

	fmt.Printf("📡 Response: %s\n", string(body))

	// Parse area search response
	var areaResponse struct {
		Count int `json:"count"`
		Cells []struct {
			Lat     float64 `json:"lat"`
			Lon     float64 `json:"lon"`
			MCC     int     `json:"mcc"`
			MNC     int     `json:"mnc"`
			LAC     int     `json:"lac"`
			CellID  int     `json:"cellid"`
			Range   int     `json:"range"`
			Samples int     `json:"samples"`
			Radio   string  `json:"radio"`
		} `json:"cells"`
	}

	if err := json.Unmarshal(body, &areaResponse); err != nil {
		return fmt.Errorf("failed to parse area response: %v", err)
	}

	fmt.Printf("📊 Found %d cells in the area:\n", areaResponse.Count)
	for i, cell := range areaResponse.Cells {
		distance := calculateDistance(gpsLat, gpsLon, cell.Lat, cell.Lon)
		fmt.Printf("  %d. Cell %d: %.6f°, %.6f° (%s, ±%dm, %d samples, %.0fm away)\n",
			i+1, cell.CellID, cell.Lat, cell.Lon, cell.Radio, cell.Range, cell.Samples, distance)

		// Check if this is your cell tower
		if cell.CellID == 25939743 {
			fmt.Printf("     🎯 THIS IS YOUR CELL TOWER!\n")
		}
	}

	return nil
}
