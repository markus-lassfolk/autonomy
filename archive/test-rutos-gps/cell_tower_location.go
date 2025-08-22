package main

import (
	"bytes"
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"strconv"
	"strings"
	"time"
)

// CellTowerLocation represents location data from cell tower databases
type CellTowerLocation struct {
	Latitude     float64   `json:"latitude"`
	Longitude    float64   `json:"longitude"`
	Accuracy     float64   `json:"accuracy"`   // Accuracy radius in meters
	Source       string    `json:"source"`     // "opencellid", "mozilla", "google"
	Method       string    `json:"method"`     // "single_cell", "triangulation"
	Confidence   float64   `json:"confidence"` // 0.0-1.0
	Valid        bool      `json:"valid"`
	Error        string    `json:"error,omitempty"`
	ResponseTime float64   `json:"response_time_ms"`
	CollectedAt  time.Time `json:"collected_at"`
}

// OpenCellIDRequest represents request to OpenCellID API
type OpenCellIDRequest struct {
	Token string `json:"token"`
	Radio string `json:"radio"`
	MCC   int    `json:"mcc"`
	MNC   int    `json:"mnc"`
	Cells []struct {
		LAC int `json:"lac"`
		CID int `json:"cid"`
	} `json:"cells"`
}

// OpenCellIDResponse represents response from OpenCellID API
type OpenCellIDResponse struct {
	Status   string  `json:"status"`
	Message  string  `json:"message,omitempty"`
	Balance  int     `json:"balance,omitempty"`
	Lat      float64 `json:"lat"`
	Lon      float64 `json:"lon"`
	Accuracy float64 `json:"accuracy"`
	Address  string  `json:"address,omitempty"`
}

// MozillaLocationRequest represents request to Mozilla Location Service
type MozillaLocationRequest struct {
	CellTowers []MozillaCellTower `json:"cellTowers"`
}

type MozillaCellTower struct {
	RadioType         string `json:"radioType"`
	MobileCountryCode int    `json:"mobileCountryCode"`
	MobileNetworkCode int    `json:"mobileNetworkCode"`
	LocationAreaCode  int    `json:"locationAreaCode"`
	CellID            int    `json:"cellId"`
	SignalStrength    int    `json:"signalStrength,omitempty"`
}

// MozillaLocationResponse represents response from Mozilla Location Service
type MozillaLocationResponse struct {
	Location struct {
		Lat float64 `json:"lat"`
		Lng float64 `json:"lng"`
	} `json:"location"`
	Accuracy float64 `json:"accuracy"`
}

// getCellTowerLocation gets location from cell tower databases
func getCellTowerLocation(intel *CellularLocationIntelligence) (*CellTowerLocation, error) {
	fmt.Println("🗼 Getting Location from Cell Tower Databases")
	fmt.Println("=" + strings.Repeat("=", 45))

	// Try multiple services in order of preference
	services := []func(*CellularLocationIntelligence) (*CellTowerLocation, error){
		getMozillaLocation,    // Free, no API key required
		getOpenCellIDLocation, // Free with registration
		// getGoogleLocation,   // Paid service (commented out)
	}

	var lastError error
	for _, service := range services {
		if location, err := service(intel); err == nil && location.Valid {
			fmt.Printf("✅ Location found via %s\n", location.Source)
			displayCellTowerLocation(location)
			return location, nil
		} else {
			lastError = err
			if location != nil {
				fmt.Printf("❌ %s failed: %s\n", location.Source, location.Error)
			}
		}
	}

	return nil, fmt.Errorf("all cell tower location services failed: %v", lastError)
}

// getMozillaLocation gets location from Mozilla Location Service
func getMozillaLocation(intel *CellularLocationIntelligence) (*CellTowerLocation, error) {
	start := time.Now()
	location := &CellTowerLocation{
		Source:      "mozilla_location_service",
		Method:      "single_cell",
		CollectedAt: time.Now(),
	}

	// Parse cell data
	cellID, _ := strconv.Atoi(intel.ServingCell.CellID)
	mcc, _ := strconv.Atoi(intel.ServingCell.MCC)
	mnc, _ := strconv.Atoi(intel.ServingCell.MNC)
	lac, _ := strconv.Atoi(intel.ServingCell.TAC)

	// Prepare request
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

	// Add neighbor cells for better triangulation
	for _, neighbor := range intel.NeighborCells {
		if len(request.CellTowers) >= 6 { // Limit to 6 cells
			break
		}
		request.CellTowers = append(request.CellTowers, MozillaCellTower{
			RadioType:         "lte",
			MobileCountryCode: mcc,
			MobileNetworkCode: mnc,
			LocationAreaCode:  lac,
			CellID:            neighbor.PCID, // Use PCID as cell ID for neighbors
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
	location.Confidence = 0.7 // Medium confidence for cell tower location

	if len(request.CellTowers) > 1 {
		location.Method = "triangulation"
		location.Confidence = 0.8 // Higher confidence with multiple cells
	}

	return location, nil
}

// getOpenCellIDLocation gets location from OpenCellID (requires API key)
func getOpenCellIDLocation(intel *CellularLocationIntelligence) (*CellTowerLocation, error) {
	start := time.Now()
	location := &CellTowerLocation{
		Source:      "opencellid",
		Method:      "single_cell",
		CollectedAt: time.Now(),
	}

	// Note: This requires an API key from opencellid.org
	apiKey := "YOUR_OPENCELLID_API_KEY" // Replace with actual API key
	if apiKey == "YOUR_OPENCELLID_API_KEY" {
		location.Error = "OpenCellID API key not configured"
		return location, fmt.Errorf("API key required")
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
	location.Confidence = 0.8 // Good confidence for OpenCellID

	return location, nil
}

// displayCellTowerLocation displays cell tower location results
func displayCellTowerLocation(location *CellTowerLocation) {
	fmt.Printf("  📍 Coordinates: %.6f°, %.6f°\n", location.Latitude, location.Longitude)
	fmt.Printf("  🎯 Accuracy: %.0f meters\n", location.Accuracy)
	fmt.Printf("  📡 Source: %s\n", location.Source)
	fmt.Printf("  🔧 Method: %s\n", location.Method)
	fmt.Printf("  📊 Confidence: %.1f%%\n", location.Confidence*100)
	fmt.Printf("  ⏱️  Response Time: %.1f ms\n", location.ResponseTime)

	// Create Google Maps link
	mapsLink := fmt.Sprintf("https://www.google.com/maps?q=%.6f,%.6f", location.Latitude, location.Longitude)
	fmt.Printf("  🗺️  Maps Link: %s\n", mapsLink)
}

// compareCellTowerWithGPS compares cell tower location with GPS coordinates
func compareCellTowerWithGPS(cellLocation *CellTowerLocation, gpsLat, gpsLon float64) {
	if !cellLocation.Valid {
		fmt.Println("  ❌ Cannot compare - cell tower location invalid")
		return
	}

	distance := calculateDistance(cellLocation.Latitude, cellLocation.Longitude, gpsLat, gpsLon)

	fmt.Printf("\n📏 Comparison with GPS:\n")
	fmt.Printf("  GPS Location: %.6f°, %.6f°\n", gpsLat, gpsLon)
	fmt.Printf("  Cell Location: %.6f°, %.6f°\n", cellLocation.Latitude, cellLocation.Longitude)
	fmt.Printf("  Distance: %.0f meters\n", distance)

	if distance < cellLocation.Accuracy {
		fmt.Printf("  ✅ EXCELLENT: Within expected accuracy (%.0fm)\n", cellLocation.Accuracy)
	} else if distance < cellLocation.Accuracy*2 {
		fmt.Printf("  ✅ GOOD: Close to expected accuracy\n")
	} else {
		fmt.Printf("  ⚠️  FAIR: Outside expected accuracy range\n")
	}
}

// Enhanced location collection with cell tower fallback
func getLocationWithCellTowerFallback(intel *CellularLocationIntelligence) (*CellTowerLocation, error) {
	fmt.Println("🎯 Enhanced Location Collection with Cell Tower Fallback")
	fmt.Println("=" + strings.Repeat("=", 58))

	// Try to get cell tower location
	cellLocation, err := getCellTowerLocation(intel)
	if err != nil {
		return nil, fmt.Errorf("cell tower location failed: %v", err)
	}

	// Compare with known GPS coordinates (from your previous tests)
	knownGPSLat := 59.48007000 // From Quectel GPS
	knownGPSLon := 18.27985000

	compareCellTowerWithGPS(cellLocation, knownGPSLat, knownGPSLon)

	return cellLocation, nil
}
