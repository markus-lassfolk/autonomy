package main

import (
	"encoding/json"
	"fmt"
	"net/http"
	"strings"
	"time"

	"googlemaps.github.io/maps"
)

// EnhancedStandardizedLocationResponse with your suggested improvements
type EnhancedStandardizedLocationResponse struct {
	// Core Location Data (always available)
	Latitude  float64   `json:"latitude"`  // Decimal degrees
	Longitude float64   `json:"longitude"` // Decimal degrees
	Accuracy  float64   `json:"accuracy"`  // Accuracy radius in meters (ALWAYS in meters)
	Timestamp time.Time `json:"timestamp"` // When location was determined

	// Enhanced GPS Data with compensation
	Altitude *float64 `json:"altitude,omitempty"` // Meters above sea level (compensated if needed)
	Speed    *float64 `json:"speed,omitempty"`    // Speed in km/h (nil if unavailable)
	Course   *float64 `json:"course,omitempty"`   // Bearing in degrees (nil if unavailable)

	// Enhanced Fix Information
	FixType    int    `json:"fix_type"`    // 0=No Fix, 1=2D Fix, 2=3D Fix, 3=DGPS Fix
	FixQuality string `json:"fix_quality"` // "excellent", "good", "fair", "poor"

	// Enhanced Source Information with details
	Source      string   `json:"source"`       // Detailed source description with satellite/data counts
	Method      string   `json:"method"`       // "gps", "starlink", "google_api", "fallback"
	DataSources []string `json:"data_sources"` // Specific APIs/sources used

	// Quality Indicators
	HDOP       *float64 `json:"hdop,omitempty"`       // Horizontal Dilution of Precision (GPS only)
	Satellites *int     `json:"satellites,omitempty"` // Number of satellites (GPS/Starlink only)

	// Metadata
	FromCache    bool          `json:"from_cache"`    // Whether from cache
	ResponseTime time.Duration `json:"response_time"` // Time to get location
	APICallMade  bool          `json:"api_call_made"` // Whether API was called
	APICost      float64       `json:"api_cost"`      // Cost of API call

	// Reliability indicators
	Valid      bool    `json:"valid"`      // Whether location is considered valid
	Confidence float64 `json:"confidence"` // Confidence score 0.0-1.0

	// Altitude compensation info
	AltitudeSource string `json:"altitude_source,omitempty"` // "gps", "starlink", "estimated", "api"
	AltitudeNote   string `json:"altitude_note,omitempty"`   // Note about altitude source
}

// GPS Fix Type constants
const (
	FixTypeNoFix = 0 // No GPS fix
	FixType2D    = 1 // 2D fix (lat/lon only)
	FixType3D    = 2 // 3D fix (lat/lon/alt)
	FixTypeDGPS  = 3 // Differential GPS fix (enhanced accuracy)
)

// ElevationAPI represents different elevation services
type ElevationAPI struct {
	Name      string
	URL       string
	Free      bool
	RateLimit string
}

// Available elevation APIs for altitude compensation
var ElevationAPIs = []ElevationAPI{
	{
		Name:      "Open Elevation",
		URL:       "https://api.open-elevation.com/api/v1/lookup",
		Free:      true,
		RateLimit: "No strict limit",
	},
	{
		Name:      "Google Elevation API",
		URL:       "https://maps.googleapis.com/maps/api/elevation/json",
		Free:      false,
		RateLimit: "2500 requests/day free",
	},
	{
		Name:      "MapBox Tilesets API",
		URL:       "https://api.mapbox.com/v4/mapbox.mapbox-terrain-v2/tilequery",
		Free:      false,
		RateLimit: "50,000 requests/month free",
	},
}

// CreateEnhancedLocationFromGPS converts GPS data to enhanced standardized format
func CreateEnhancedLocationFromGPS(gpsData *QuectelGPSData) *EnhancedStandardizedLocationResponse {
	// Calculate accuracy from HDOP
	accuracy := gpsData.HDOP * 5.0
	if accuracy < 2.0 {
		accuracy = 2.0
	}

	// Determine fix type based on GPS data
	fixType := FixTypeNoFix
	if gpsData.Latitude != 0 && gpsData.Longitude != 0 {
		if gpsData.Altitude != 0 {
			fixType = FixType3D // Has altitude
		} else {
			fixType = FixType2D // Only lat/lon
		}
	}

	// Determine fix quality based on accuracy and satellites
	quality := "poor"
	if accuracy <= 5.0 && gpsData.Satellites >= 8 {
		quality = "excellent"
	} else if accuracy <= 10.0 && gpsData.Satellites >= 6 {
		quality = "good"
	} else if accuracy <= 20.0 && gpsData.Satellites >= 4 {
		quality = "fair"
	}

	// Create enhanced source description
	source := fmt.Sprintf("GPS (%d satellites)", gpsData.Satellites)

	return &EnhancedStandardizedLocationResponse{
		// Core data
		Latitude:  gpsData.Latitude,
		Longitude: gpsData.Longitude,
		Accuracy:  accuracy, // Always in meters
		Timestamp: time.Now(),

		// Enhanced GPS data (all available)
		Altitude: &gpsData.Altitude,
		Speed:    &gpsData.SpeedKmh,
		Course:   &gpsData.Course,

		// Enhanced fix information
		FixType:    fixType,
		FixQuality: quality,

		// Enhanced source information
		Source:      source,
		Method:      "gps",
		DataSources: []string{"quectel_gnss"},

		// Quality indicators
		HDOP:       &gpsData.HDOP,
		Satellites: &gpsData.Satellites,

		// Metadata
		FromCache:    false,
		ResponseTime: 0,
		APICallMade:  false,
		APICost:      0.0,

		// Reliability
		Valid:      gpsData.Latitude != 0 && gpsData.Longitude != 0,
		Confidence: calculateGPSConfidence(gpsData),

		// Altitude info
		AltitudeSource: "gps",
		AltitudeNote:   "From GNSS receiver",
	}
}

// CreateEnhancedLocationFromStarlink converts Starlink data to enhanced standardized format
func CreateEnhancedLocationFromStarlink(starlinkData *ComprehensiveStarlinkGPS) *EnhancedStandardizedLocationResponse {
	// Determine fix type based on available data
	fixType := FixTypeNoFix
	if starlinkData.Latitude != 0 && starlinkData.Longitude != 0 {
		if starlinkData.Altitude != 0 {
			fixType = FixType3D // Has altitude
		} else {
			fixType = FixType2D // Only lat/lon
		}
	}

	// Create enhanced source description with satellite count if available
	var source string
	if starlinkData.GPSSatellites != nil {
		source = fmt.Sprintf("Starlink (%d satellites)", *starlinkData.GPSSatellites)
	} else {
		source = "Starlink (GPS)"
	}

	// Convert speed from m/s to km/h
	speedKmh := starlinkData.HorizontalSpeedMps * 3.6

	return &EnhancedStandardizedLocationResponse{
		// Core data
		Latitude:  starlinkData.Latitude,
		Longitude: starlinkData.Longitude,
		Accuracy:  starlinkData.Accuracy, // Always in meters
		Timestamp: starlinkData.CollectedAt,

		// Enhanced data
		Altitude: &starlinkData.Altitude,
		Speed:    &speedKmh,
		Course:   nil, // Not available from Starlink

		// Enhanced fix information
		FixType:    fixType,
		FixQuality: starlinkData.QualityScore,

		// Enhanced source information
		Source:      source,
		Method:      "starlink",
		DataSources: starlinkData.DataSources,

		// Quality indicators
		HDOP:       nil, // Not available, use accuracy instead
		Satellites: starlinkData.GPSSatellites,

		// Metadata
		FromCache:    false,
		ResponseTime: time.Duration(starlinkData.CollectionMs) * time.Millisecond,
		APICallMade:  true,
		APICost:      0.0, // Free Starlink APIs

		// Reliability
		Valid:      starlinkData.Valid,
		Confidence: starlinkData.Confidence,

		// Altitude info
		AltitudeSource: "starlink",
		AltitudeNote:   "From Starlink dish GPS",
	}
}

// CreateEnhancedLocationFromGoogle converts Google API response to enhanced standardized format
func CreateEnhancedLocationFromGoogle(resp *maps.GeolocationResult, wifiCount, cellCount int, cost float64) *EnhancedStandardizedLocationResponse {
	// Determine fix type based on accuracy (your suggestion)
	fixType := FixTypeNoFix
	if resp.Location.Lat != 0 && resp.Location.Lng != 0 {
		if resp.Accuracy < 2000.0 { // Your suggested threshold
			fixType = FixType2D // Good enough for 2D fix
		}
		// Note: Google API doesn't provide altitude, so never 3D fix
	}

	// Determine data sources used
	var dataSources []string
	if wifiCount > 0 {
		dataSources = append(dataSources, "wifi")
	}
	if cellCount > 0 {
		dataSources = append(dataSources, "cellular")
	}

	// Create enhanced source description (your suggestion)
	var sourceDetails []string
	if cellCount > 0 {
		sourceDetails = append(sourceDetails, fmt.Sprintf("%d Cell", cellCount))
	}
	if wifiCount > 0 {
		sourceDetails = append(sourceDetails, fmt.Sprintf("%d WiFi", wifiCount))
	}
	source := fmt.Sprintf("Google (%s)", strings.Join(sourceDetails, " + "))

	// Determine fix quality
	quality := "poor"
	if resp.Accuracy <= 50.0 {
		quality = "excellent"
	} else if resp.Accuracy <= 100.0 {
		quality = "good"
	} else if resp.Accuracy <= 500.0 {
		quality = "fair"
	}

	// Estimate altitude using elevation API (compensation strategy)
	estimatedAltitude, altitudeSource, altitudeNote := estimateAltitudeFromCoordinates(resp.Location.Lat, resp.Location.Lng)

	return &EnhancedStandardizedLocationResponse{
		// Core data (from Google API)
		Latitude:  resp.Location.Lat,
		Longitude: resp.Location.Lng,
		Accuracy:  resp.Accuracy, // Always in meters
		Timestamp: time.Now(),

		// Enhanced data (compensated)
		Altitude: estimatedAltitude, // Estimated from elevation API
		Speed:    nil,               // Not available from Google API
		Course:   nil,               // Not available from Google API

		// Enhanced fix information
		FixType:    fixType,
		FixQuality: quality,

		// Enhanced source information
		Source:      source,
		Method:      "google_api",
		DataSources: dataSources,

		// Quality indicators
		HDOP:       nil, // Not applicable for Google API
		Satellites: nil, // Not applicable for Google API

		// Metadata
		FromCache:    false,
		ResponseTime: 0,
		APICallMade:  true,
		APICost:      cost,

		// Reliability
		Valid:      resp.Location.Lat != 0 && resp.Location.Lng != 0,
		Confidence: calculateGoogleConfidence(resp.Accuracy, wifiCount, cellCount),

		// Altitude compensation info
		AltitudeSource: altitudeSource,
		AltitudeNote:   altitudeNote,
	}
}

// estimateAltitudeFromCoordinates uses elevation APIs to estimate altitude
func estimateAltitudeFromCoordinates(lat, lon float64) (*float64, string, string) {
	// Try Open Elevation API (free)
	if altitude, err := queryOpenElevationAPI(lat, lon); err == nil {
		return &altitude, "api", "Estimated from Open Elevation API"
	}

	// Fallback to rough estimation based on location
	// This is a very rough estimation - you could enhance this with local terrain data
	estimatedAlt := estimateAltitudeByRegion(lat, lon)
	return &estimatedAlt, "estimated", "Rough estimation based on geographic region"
}

// queryOpenElevationAPI queries the free Open Elevation API
func queryOpenElevationAPI(lat, lon float64) (float64, error) {
	url := fmt.Sprintf("https://api.open-elevation.com/api/v1/lookup?locations=%.6f,%.6f", lat, lon)

	client := &http.Client{Timeout: 10 * time.Second}
	resp, err := client.Get(url)
	if err != nil {
		return 0, err
	}
	defer resp.Body.Close()

	var result struct {
		Results []struct {
			Elevation float64 `json:"elevation"`
		} `json:"results"`
	}

	if err := json.NewDecoder(resp.Body).Decode(&result); err != nil {
		return 0, err
	}

	if len(result.Results) > 0 {
		return result.Results[0].Elevation, nil
	}

	return 0, fmt.Errorf("no elevation data found")
}

// estimateAltitudeByRegion provides rough altitude estimates by geographic region
func estimateAltitudeByRegion(lat, lon float64) float64 {
	// Very rough estimates based on major geographic regions
	// You could enhance this with more detailed terrain data

	// Scandinavia (where your coordinates are)
	if lat >= 55.0 && lat <= 71.0 && lon >= 4.0 && lon <= 31.0 {
		// Sweden/Norway - generally low to moderate elevation
		if lat >= 60.0 { // Northern Sweden/Norway - more mountainous
			return 200.0
		}
		return 50.0 // Southern Sweden - relatively flat
	}

	// Alps region
	if lat >= 45.0 && lat <= 48.0 && lon >= 6.0 && lon <= 17.0 {
		return 800.0 // Mountainous
	}

	// Netherlands/Denmark - very flat
	if lat >= 51.0 && lat <= 58.0 && lon >= 3.0 && lon <= 13.0 {
		return 10.0
	}

	// Default estimate for unknown regions
	return 100.0
}

// PrintEnhancedLocationResponse prints detailed enhanced location information
func PrintEnhancedLocationResponse(resp *EnhancedStandardizedLocationResponse) {
	fmt.Printf("📍 Location: %.6f°, %.6f° (±%.0fm)\n", resp.Latitude, resp.Longitude, resp.Accuracy)
	fmt.Printf("📡 Source: %s (%s)\n", resp.Source, resp.Method)
	fmt.Printf("⭐ Quality: %s (confidence: %.1f%%)\n", resp.FixQuality, resp.Confidence*100)

	// Enhanced fix type display
	fixTypeStr := map[int]string{
		FixTypeNoFix: "No Fix",
		FixType2D:    "2D Fix",
		FixType3D:    "3D Fix",
		FixTypeDGPS:  "DGPS Fix",
	}
	fmt.Printf("🎯 Fix Type: %s (%d)\n", fixTypeStr[resp.FixType], resp.FixType)

	// Altitude with source info
	if resp.Altitude != nil {
		fmt.Printf("🏔️  Altitude: %.1fm (%s)\n", *resp.Altitude, resp.AltitudeSource)
		if resp.AltitudeNote != "" {
			fmt.Printf("    Note: %s\n", resp.AltitudeNote)
		}
	} else {
		fmt.Printf("🏔️  Altitude: N/A\n")
	}

	// Enhanced satellite/data source info
	if resp.Satellites != nil {
		fmt.Printf("🛰️  Satellites: %d\n", *resp.Satellites)
	}
	if len(resp.DataSources) > 0 {
		fmt.Printf("📊 Data Sources: %v\n", resp.DataSources)
	}

	if resp.Speed != nil && *resp.Speed > 0 {
		fmt.Printf("🚗 Speed: %.1f km/h\n", *resp.Speed)
	}

	fmt.Printf("💾 Cached: %t\n", resp.FromCache)
	fmt.Printf("💰 API Cost: $%.3f\n", resp.APICost)
	fmt.Printf("⏱️  Response Time: %v\n", resp.ResponseTime)
	fmt.Printf("✅ Valid: %t\n", resp.Valid)
}

// testEnhancedStandardizedLocation demonstrates the enhanced standardized location system
func testEnhancedStandardizedLocation() {
	fmt.Println("🚀 Enhanced Standardized Location Response Test")
	fmt.Println("==============================================")

	fmt.Println("\n📊 Fix Type Standards:")
	fmt.Println("  0 = No Fix")
	fmt.Println("  1 = 2D Fix (lat/lon only)")
	fmt.Println("  2 = 3D Fix (lat/lon/alt)")
	fmt.Println("  3 = DGPS Fix (enhanced accuracy)")

	fmt.Println("\n🎯 Accuracy Standards:")
	fmt.Println("  Always presented in meters")
	fmt.Println("  Google API: <2000m = 2D Fix, ≥2000m = No Fix")

	fmt.Println("\n🛰️  Source Format Examples:")
	fmt.Println("  GPS (12 satellites)")
	fmt.Println("  Starlink (14 satellites)")
	fmt.Println("  Google (7 Cell + 9 WiFi)")

	fmt.Println("\n🏔️  Altitude Compensation Strategy:")
	fmt.Println("  1. GPS/Starlink altitude (preferred)")
	fmt.Println("  2. Open Elevation API (free)")
	fmt.Println("  3. Google Elevation API (paid)")
	fmt.Println("  4. Regional estimation (fallback)")

	// Example 1: GPS Response
	fmt.Println("\n📍 Example 1: GPS Response")
	fmt.Println("==========================")
	gpsData := &QuectelGPSData{
		Latitude:   59.480070,
		Longitude:  18.279850,
		Altitude:   25.5,
		SpeedKmh:   0.0,
		Course:     0.0,
		Satellites: 12,
		HDOP:       0.8,
	}

	gpsResponse := CreateEnhancedLocationFromGPS(gpsData)
	PrintEnhancedLocationResponse(gpsResponse)

	// Example 2: Starlink Response
	fmt.Println("\n🛰️  Example 2: Starlink Multi-API Response")
	fmt.Println("==========================================")
	starlinkData := &ComprehensiveStarlinkGPS{
		Latitude:               59.480051805924234,
		Longitude:              18.279876560548065,
		Altitude:               21.452765255424573,
		Accuracy:               5.0,
		HorizontalSpeedMps:     0.0,
		VerticalSpeedMps:       0.0,
		GPSSource:              "GNC_NO_ACCEL",
		GPSValid:               &[]bool{true}[0],
		GPSSatellites:          &[]int{14}[0],
		NoSatsAfterTTFF:        &[]bool{false}[0],
		InhibitGPS:             &[]bool{false}[0],
		LocationEnabled:        &[]bool{true}[0],
		UncertaintyMeters:      &[]float64{5.0}[0],
		UncertaintyMetersValid: &[]bool{true}[0],
		GPSTimeS:               &[]float64{1.4393847625804982e+09}[0],
		DataSources:            []string{"get_location", "get_status", "get_diagnostics"},
		CollectedAt:            time.Now(),
		CollectionMs:           450,
		Valid:                  true,
		Confidence:             0.9,
		QualityScore:           "excellent",
	}

	starlinkResponse := CreateEnhancedLocationFromStarlink(starlinkData)
	PrintEnhancedLocationResponse(starlinkResponse)

	// Example 3: Google API Response with altitude compensation
	fmt.Println("\n🌐 Example 3: Google API Response (with altitude compensation)")
	fmt.Println("============================================================")
	googleResp := &maps.GeolocationResult{
		Location: maps.LatLng{Lat: 59.479826, Lng: 18.279921},
		Accuracy: 45.0,
	}

	googleResponse := CreateEnhancedLocationFromGoogle(googleResp, 9, 7, 0.005)
	PrintEnhancedLocationResponse(googleResponse)

	fmt.Println("\n🎯 Key Enhancements:")
	fmt.Println("====================")
	fmt.Println("✅ Fix Type: 0-3 scale (No Fix, 2D, 3D, DGPS)")
	fmt.Println("✅ Source Details: Shows satellite/data counts")
	fmt.Println("✅ Accuracy: Always in meters")
	fmt.Println("✅ Altitude Compensation: APIs + regional estimation")
	fmt.Println("✅ Enhanced Metadata: Source info, collection time, costs")
	fmt.Println("✅ Google Fix Logic: <2000m accuracy = 2D Fix")
}
