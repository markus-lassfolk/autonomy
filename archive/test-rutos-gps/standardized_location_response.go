package main

import (
	"fmt"
	"strings"
	"time"

	"googlemaps.github.io/maps"
)

// StandardizedLocationResponse provides a unified interface for all location sources
type StandardizedLocationResponse struct {
	// Core Location Data (always available)
	Latitude  float64   `json:"latitude"`  // Decimal degrees
	Longitude float64   `json:"longitude"` // Decimal degrees
	Accuracy  float64   `json:"accuracy"`  // Accuracy radius in meters
	Timestamp time.Time `json:"timestamp"` // When location was determined

	// Extended GPS Data (available from GPS sources, simulated for others)
	Altitude   *float64 `json:"altitude,omitempty"`   // Meters above sea level (nil if unavailable)
	Speed      *float64 `json:"speed,omitempty"`      // Speed in km/h (nil if unavailable)
	Course     *float64 `json:"course,omitempty"`     // Bearing in degrees (nil if unavailable)
	Satellites *int     `json:"satellites,omitempty"` // Number of satellites (nil if not GPS)

	// Quality Indicators
	FixType    string   `json:"fix_type"`       // "gps", "cellular", "wifi", "combined"
	FixQuality string   `json:"fix_quality"`    // "excellent", "good", "fair", "poor"
	HDOP       *float64 `json:"hdop,omitempty"` // Horizontal Dilution of Precision (GPS only)

	// Source Information
	Source       string        `json:"source"`        // Detailed source description
	Method       string        `json:"method"`        // "gps", "google_api", "fallback"
	FromCache    bool          `json:"from_cache"`    // Whether from cache
	ResponseTime time.Duration `json:"response_time"` // Time to get location

	// API-specific data (for Google API responses)
	APICallMade bool     `json:"api_call_made"` // Whether API was called
	APICost     float64  `json:"api_cost"`      // Cost of API call
	DataSources []string `json:"data_sources"`  // ["wifi", "cellular", "ip"]

	// Reliability indicators
	Valid      bool    `json:"valid"`      // Whether location is considered valid
	Confidence float64 `json:"confidence"` // Confidence score 0.0-1.0
}

// LocationSourceCapabilities defines what each source can provide
type LocationSourceCapabilities struct {
	ProvidesAltitude   bool
	ProvidesSpeed      bool
	ProvidesCourse     bool
	ProvidesSatellites bool
	ProvidesHDOP       bool
	TypicalAccuracy    string
	MaxAccuracy        float64
	MinAccuracy        float64
}

// GetSourceCapabilities returns capabilities for each location source
func GetSourceCapabilities() map[string]LocationSourceCapabilities {
	return map[string]LocationSourceCapabilities{
		"gps": {
			ProvidesAltitude:   true,
			ProvidesSpeed:      true,
			ProvidesCourse:     true,
			ProvidesSatellites: true,
			ProvidesHDOP:       true,
			TypicalAccuracy:    "2-5 meters",
			MaxAccuracy:        1.0,
			MinAccuracy:        50.0,
		},
		"google_wifi": {
			ProvidesAltitude:   false,
			ProvidesSpeed:      false,
			ProvidesCourse:     false,
			ProvidesSatellites: false,
			ProvidesHDOP:       false,
			TypicalAccuracy:    "10-100 meters",
			MaxAccuracy:        10.0,
			MinAccuracy:        500.0,
		},
		"google_cellular": {
			ProvidesAltitude:   false,
			ProvidesSpeed:      false,
			ProvidesCourse:     false,
			ProvidesSatellites: false,
			ProvidesHDOP:       false,
			TypicalAccuracy:    "100-1000 meters",
			MaxAccuracy:        50.0,
			MinAccuracy:        5000.0,
		},
		"google_combined": {
			ProvidesAltitude:   false,
			ProvidesSpeed:      false,
			ProvidesCourse:     false,
			ProvidesSatellites: false,
			ProvidesHDOP:       false,
			TypicalAccuracy:    "20-200 meters",
			MaxAccuracy:        10.0,
			MinAccuracy:        1000.0,
		},
	}
}

// CreateStandardizedLocationFromGPS converts GPS data to standardized format
func CreateStandardizedLocationFromGPS(gpsData *QuectelGPSData) *StandardizedLocationResponse {
	// Calculate accuracy from HDOP
	accuracy := gpsData.HDOP * 5.0
	if accuracy < 2.0 {
		accuracy = 2.0
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

	return &StandardizedLocationResponse{
		// Core data
		Latitude:  gpsData.Latitude,
		Longitude: gpsData.Longitude,
		Accuracy:  accuracy,
		Timestamp: time.Now(),

		// Extended GPS data (all available)
		Altitude:   &gpsData.Altitude,
		Speed:      &gpsData.SpeedKmh,
		Course:     &gpsData.Course,
		Satellites: &gpsData.Satellites,

		// Quality indicators
		FixType:    "gps",
		FixQuality: quality,
		HDOP:       &gpsData.HDOP,

		// Source information
		Source:       fmt.Sprintf("Quectel GNSS (%d satellites, HDOP %.1f)", gpsData.Satellites, gpsData.HDOP),
		Method:       "gps",
		FromCache:    false,
		ResponseTime: 0,

		// API data (not applicable)
		APICallMade: false,
		APICost:     0.0,
		DataSources: []string{"gnss"},

		// Reliability
		Valid:      gpsData.Latitude != 0 && gpsData.Longitude != 0,
		Confidence: calculateGPSConfidence(gpsData),
	}
}

// CreateStandardizedLocationFromGoogle converts Google API response to standardized format
func CreateStandardizedLocationFromGoogle(resp *maps.GeolocationResult, wifiCount, cellCount int, cost float64) *StandardizedLocationResponse {
	// Determine data sources used
	var dataSources []string
	if wifiCount > 0 {
		dataSources = append(dataSources, "wifi")
	}
	if cellCount > 0 {
		dataSources = append(dataSources, "cellular")
	}

	// Determine fix type and quality
	fixType := "wifi"
	if cellCount > 0 && wifiCount > 0 {
		fixType = "combined"
	} else if cellCount > 0 {
		fixType = "cellular"
	}

	quality := "poor"
	if resp.Accuracy <= 50.0 {
		quality = "excellent"
	} else if resp.Accuracy <= 100.0 {
		quality = "good"
	} else if resp.Accuracy <= 500.0 {
		quality = "fair"
	}

	// Simulate missing GPS data (set to nil since Google API doesn't provide these)
	return &StandardizedLocationResponse{
		// Core data (from Google API)
		Latitude:  resp.Location.Lat,
		Longitude: resp.Location.Lng,
		Accuracy:  resp.Accuracy,
		Timestamp: time.Now(),

		// Extended GPS data (not available from Google API)
		Altitude:   nil, // Google API doesn't provide altitude
		Speed:      nil, // Google API doesn't provide speed
		Course:     nil, // Google API doesn't provide course
		Satellites: nil, // Google API doesn't use satellites

		// Quality indicators
		FixType:    fixType,
		FixQuality: quality,
		HDOP:       nil, // Not applicable for Google API

		// Source information
		Source:       fmt.Sprintf("Google API (%d WiFi + %d Cell)", wifiCount, cellCount),
		Method:       "google_api",
		FromCache:    false,
		ResponseTime: 0,

		// API data
		APICallMade: true,
		APICost:     cost,
		DataSources: dataSources,

		// Reliability
		Valid:      resp.Location.Lat != 0 && resp.Location.Lng != 0,
		Confidence: calculateGoogleConfidence(resp.Accuracy, wifiCount, cellCount),
	}
}

// CreateStandardizedLocationFromCache creates a cached location response
func CreateStandardizedLocationFromCache(cached *StandardizedLocationResponse) *StandardizedLocationResponse {
	// Create a copy with updated cache status
	response := *cached
	response.FromCache = true
	response.ResponseTime = 0       // Instant from cache
	response.Timestamp = time.Now() // Update access time
	response.APICallMade = false    // No new API call
	response.APICost = 0.0          // No cost for cache

	return &response
}

// SimulateMissingGPSFields adds simulated GPS fields for non-GPS sources
func (slr *StandardizedLocationResponse) SimulateMissingGPSFields() {
	// Only simulate if this is not a GPS source
	if slr.Method == "gps" {
		return
	}

	// Simulate altitude based on location (very rough estimation)
	if slr.Altitude == nil {
		// Default sea level for most locations
		estimatedAltitude := 50.0 // 50 meters above sea level as default
		slr.Altitude = &estimatedAltitude
	}

	// Speed and course remain nil (cannot be estimated from single location)
	// Satellites remain nil (not applicable for non-GPS sources)
	// HDOP remains nil (not applicable for non-GPS sources)
}

// GetMissingFields returns a list of fields that are nil/unavailable
func (slr *StandardizedLocationResponse) GetMissingFields() []string {
	var missing []string

	if slr.Altitude == nil {
		missing = append(missing, "altitude")
	}
	if slr.Speed == nil {
		missing = append(missing, "speed")
	}
	if slr.Course == nil {
		missing = append(missing, "course")
	}
	if slr.Satellites == nil {
		missing = append(missing, "satellites")
	}
	if slr.HDOP == nil {
		missing = append(missing, "hdop")
	}

	return missing
}

// IsEquivalentToGPS checks if this response has GPS-equivalent data
func (slr *StandardizedLocationResponse) IsEquivalentToGPS() bool {
	missing := slr.GetMissingFields()
	// Consider equivalent if only missing speed/course (which require movement)
	criticalMissing := 0
	for _, field := range missing {
		if field != "speed" && field != "course" {
			criticalMissing++
		}
	}
	return criticalMissing <= 1 // Allow 1 critical field missing
}

// Helper functions for confidence calculation
func calculateGPSConfidence(gpsData *QuectelGPSData) float64 {
	confidence := 0.5 // Base confidence

	// Boost confidence based on satellite count
	if gpsData.Satellites >= 8 {
		confidence += 0.3
	} else if gpsData.Satellites >= 6 {
		confidence += 0.2
	} else if gpsData.Satellites >= 4 {
		confidence += 0.1
	}

	// Boost confidence based on HDOP (lower is better)
	if gpsData.HDOP <= 1.0 {
		confidence += 0.2
	} else if gpsData.HDOP <= 2.0 {
		confidence += 0.1
	}

	// Cap at 1.0
	if confidence > 1.0 {
		confidence = 1.0
	}

	return confidence
}

func calculateGoogleConfidence(accuracy float64, wifiCount, cellCount int) float64 {
	confidence := 0.3 // Base confidence

	// Boost confidence based on accuracy
	if accuracy <= 50.0 {
		confidence += 0.4
	} else if accuracy <= 100.0 {
		confidence += 0.3
	} else if accuracy <= 500.0 {
		confidence += 0.2
	}

	// Boost confidence based on data sources
	if wifiCount >= 5 {
		confidence += 0.2
	} else if wifiCount >= 3 {
		confidence += 0.1
	}

	if cellCount >= 3 {
		confidence += 0.1
	}

	// Cap at 1.0
	if confidence > 1.0 {
		confidence = 1.0
	}

	return confidence
}

// PrintLocationComparison prints a detailed comparison of location sources
func PrintLocationComparison() {
	fmt.Println("📊 Location Source Comparison")
	fmt.Println("=============================")

	capabilities := GetSourceCapabilities()

	fmt.Printf("%-15s %-10s %-8s %-8s %-8s %-12s %-6s %-15s\n",
		"Source", "Altitude", "Speed", "Course", "Sats", "HDOP", "Acc", "Typical Range")
	fmt.Println(strings.Repeat("-", 90))

	for source, caps := range capabilities {
		fmt.Printf("%-15s %-10s %-8s %-8s %-8s %-12s %-6s %-15s\n",
			source,
			boolToYesNo(caps.ProvidesAltitude),
			boolToYesNo(caps.ProvidesSpeed),
			boolToYesNo(caps.ProvidesCourse),
			boolToYesNo(caps.ProvidesSatellites),
			boolToYesNo(caps.ProvidesHDOP),
			fmt.Sprintf("%.0f-%.0fm", caps.MaxAccuracy, caps.MinAccuracy),
			caps.TypicalAccuracy)
	}

	fmt.Println("\n🎯 Field Availability Summary:")
	fmt.Println("  ✅ Always Available: Latitude, Longitude, Accuracy, Timestamp")
	fmt.Println("  📍 GPS Only: Altitude, Speed, Course, Satellites, HDOP")
	fmt.Println("  🌐 API Sources: Limited to location + accuracy only")
	fmt.Println("  💡 Simulation: Can estimate altitude, but not speed/course/satellites")
}

func boolToYesNo(b bool) string {
	if b {
		return "Yes"
	}
	return "No"
}

// testStandardizedLocationResponse demonstrates the standardized response system
func testStandardizedLocationResponse() {
	fmt.Println("📊 Standardized Location Response Test")
	fmt.Println("======================================")

	// Print comparison table
	PrintLocationComparison()

	fmt.Println("\n🧪 Testing Response Creation:")

	// Example 1: GPS Response
	gpsData := &QuectelGPSData{
		Latitude:   59.480070,
		Longitude:  18.279850,
		Altitude:   25.5,
		SpeedKmh:   0.0,
		Course:     0.0,
		Satellites: 12,
		HDOP:       0.8,
	}

	gpsResponse := CreateStandardizedLocationFromGPS(gpsData)
	fmt.Println("\n📍 GPS Response:")
	printLocationResponse(gpsResponse)

	// Example 2: Google API Response (simulated)
	googleResp := &maps.GeolocationResult{
		Location: maps.LatLng{Lat: 59.479826, Lng: 18.279921},
		Accuracy: 45.0,
	}

	googleResponse := CreateStandardizedLocationFromGoogle(googleResp, 8, 3, 0.005)
	fmt.Println("\n🌐 Google API Response:")
	printLocationResponse(googleResponse)

	// Example 3: Cached Response
	cachedResponse := CreateStandardizedLocationFromCache(googleResponse)
	fmt.Println("\n💾 Cached Response:")
	printLocationResponse(cachedResponse)

	// Example 4: Simulated fields
	googleResponse.SimulateMissingGPSFields()
	fmt.Println("\n🎭 Google API with Simulated Fields:")
	printLocationResponse(googleResponse)

	fmt.Println("\n🎯 Key Benefits:")
	fmt.Println("  ✅ Unified interface for all location sources")
	fmt.Println("  ✅ Clear indication of available vs missing fields")
	fmt.Println("  ✅ Confidence scoring for reliability assessment")
	fmt.Println("  ✅ Source capability documentation")
	fmt.Println("  ✅ Optional field simulation for compatibility")
}

func printLocationResponse(resp *StandardizedLocationResponse) {
	fmt.Printf("  📍 Location: %.6f°, %.6f° (±%.0fm)\n", resp.Latitude, resp.Longitude, resp.Accuracy)
	fmt.Printf("  📡 Source: %s (%s)\n", resp.Source, resp.Method)
	fmt.Printf("  ⭐ Quality: %s (confidence: %.1f%%)\n", resp.FixQuality, resp.Confidence*100)

	if resp.Altitude != nil {
		fmt.Printf("  🏔️  Altitude: %.1fm\n", *resp.Altitude)
	} else {
		fmt.Printf("  🏔️  Altitude: N/A\n")
	}

	if resp.Satellites != nil {
		fmt.Printf("  🛰️  Satellites: %d\n", *resp.Satellites)
	} else {
		fmt.Printf("  🛰️  Satellites: N/A\n")
	}

	if resp.Speed != nil && *resp.Speed > 0 {
		fmt.Printf("  🚗 Speed: %.1f km/h\n", *resp.Speed)
	}

	fmt.Printf("  💾 Cached: %t\n", resp.FromCache)
	fmt.Printf("  💰 API Cost: $%.3f\n", resp.APICost)

	missing := resp.GetMissingFields()
	if len(missing) > 0 {
		fmt.Printf("  ❌ Missing: %v\n", missing)
	}

	fmt.Printf("  🎯 GPS Equivalent: %t\n", resp.IsEquivalentToGPS())
}
