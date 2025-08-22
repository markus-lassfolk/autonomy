package main

import (
	"fmt"
	"math"
	"strings"
	"time"

	"googlemaps.github.io/maps"
)

// ImprovedStandardizedLocationResponse with your requested changes
type ImprovedStandardizedLocationResponse struct {
	// Core Location Data (always available)
	Latitude  float64   `json:"latitude"`  // Full precision decimal degrees
	Longitude float64   `json:"longitude"` // Full precision decimal degrees
	Accuracy  float64   `json:"accuracy"`  // Accuracy radius in meters (decimal)
	Timestamp time.Time `json:"timestamp"` // When location was determined

	// Enhanced GPS Data with compensation
	Altitude *float64 `json:"altitude,omitempty"` // Meters above sea level (compensated if needed)
	Speed    *float64 `json:"speed,omitempty"`    // Speed in meters per second (m/s)
	Course   *float64 `json:"course,omitempty"`   // Bearing in degrees (nil if unavailable)

	// Simplified Fix Information
	FixType int `json:"fix_type"` // 0=No Fix, 1=2D Fix, 2=3D Fix, 3=DGPS Fix (integer only)

	// Enhanced Source Information with details
	Source      string   `json:"source"`       // Detailed source description with satellite/data counts
	Method      string   `json:"method"`       // "gps", "starlink", "google_api", "fallback"
	DataSources []string `json:"data_sources"` // Specific APIs/sources used

	// Quality Indicators
	HDOP       *float64 `json:"hdop,omitempty"`       // Horizontal Dilution of Precision (GPS only)
	Satellites *int     `json:"satellites,omitempty"` // Number of satellites (GPS/Starlink only)

	// Metadata (simplified)
	FromCache    bool          `json:"from_cache"`    // Whether from cache
	ResponseTime time.Duration `json:"response_time"` // Time to get location

	// Reliability indicators
	Valid bool `json:"valid"` // Whether location is considered valid

	// Altitude verification info
	AltitudeSource      string  `json:"altitude_source,omitempty"`       // "gps", "starlink", "estimated", "api"
	AltitudeNote        string  `json:"altitude_note,omitempty"`         // Note about altitude source
	AltitudeVerified    *bool   `json:"altitude_verified,omitempty"`     // Whether altitude was verified
	AltitudeVerifyError *string `json:"altitude_verify_error,omitempty"` // Verification error if any
}

// AltitudeVerificationResult contains verification data from multiple sources
type AltitudeVerificationResult struct {
	OpenElevationAPI   *float64 `json:"open_elevation_api,omitempty"`
	GoogleElevationAPI *float64 `json:"google_elevation_api,omitempty"`
	RegionalEstimate   float64  `json:"regional_estimate"`
	GPSAltitude        *float64 `json:"gps_altitude,omitempty"`
	StarlinkAltitude   *float64 `json:"starlink_altitude,omitempty"`
	Consensus          float64  `json:"consensus"`         // Best estimate from all sources
	Confidence         string   `json:"confidence"`        // "high", "medium", "low"
	VerificationNote   string   `json:"verification_note"` // Explanation of verification
}

// ConstellationInfo represents GPS constellation breakdown
type ConstellationInfo struct {
	GPS     int `json:"gps"`     // GPS satellites
	GLONASS int `json:"glonass"` // GLONASS satellites
	Galileo int `json:"galileo"` // Galileo satellites
	BeiDou  int `json:"beidou"`  // BeiDou satellites
	Total   int `json:"total"`   // Total satellites
}

// convertDMSToDecimal converts degrees/minutes format to decimal degrees
func convertDMSToDecimal(dms string, direction string) float64 {
	// Parse format like "5928.803965" (DDMM.MMMMMM)
	if len(dms) < 4 {
		return 0
	}

	// Extract degrees and minutes
	degrees := 0.0
	minutes := 0.0

	if len(dms) >= 4 {
		fmt.Sscanf(dms[:2], "%f", &degrees)
		fmt.Sscanf(dms[2:], "%f", &minutes)
	}

	decimal := degrees + (minutes / 60.0)

	// Apply direction
	if direction == "S" || direction == "W" {
		decimal = -decimal
	}

	return decimal
}

// verifyAltitudeFromMultipleSources verifies altitude using multiple elevation services
func verifyAltitudeFromMultipleSources(lat, lon float64, gpsAlt, starlinkAlt *float64) *AltitudeVerificationResult {
	result := &AltitudeVerificationResult{
		GPSAltitude:      gpsAlt,
		StarlinkAltitude: starlinkAlt,
	}

	// Try Open Elevation API
	if openElev, err := queryOpenElevationAPI(lat, lon); err == nil {
		result.OpenElevationAPI = &openElev
	}

	// Regional estimate
	result.RegionalEstimate = estimateAltitudeByRegion(lat, lon)

	// Calculate consensus
	var altitudes []float64
	var sources []string

	if result.OpenElevationAPI != nil {
		altitudes = append(altitudes, *result.OpenElevationAPI)
		sources = append(sources, "Open Elevation API")
	}
	if gpsAlt != nil {
		altitudes = append(altitudes, *gpsAlt)
		sources = append(sources, "GPS")
	}
	if starlinkAlt != nil {
		altitudes = append(altitudes, *starlinkAlt)
		sources = append(sources, "Starlink")
	}
	altitudes = append(altitudes, result.RegionalEstimate)
	sources = append(sources, "Regional Estimate")

	// Calculate consensus (median of available values)
	if len(altitudes) > 0 {
		// Sort altitudes
		for i := 0; i < len(altitudes)-1; i++ {
			for j := i + 1; j < len(altitudes); j++ {
				if altitudes[i] > altitudes[j] {
					altitudes[i], altitudes[j] = altitudes[j], altitudes[i]
				}
			}
		}

		// Use median
		if len(altitudes)%2 == 0 {
			result.Consensus = (altitudes[len(altitudes)/2-1] + altitudes[len(altitudes)/2]) / 2
		} else {
			result.Consensus = altitudes[len(altitudes)/2]
		}
	}

	// Determine confidence
	if len(altitudes) >= 3 {
		// Check if values are close (within 20m)
		maxDiff := 0.0
		for i := 0; i < len(altitudes); i++ {
			diff := math.Abs(altitudes[i] - result.Consensus)
			if diff > maxDiff {
				maxDiff = diff
			}
		}

		if maxDiff <= 10.0 {
			result.Confidence = "high"
			result.VerificationNote = fmt.Sprintf("Multiple sources agree within ±%.0fm", maxDiff)
		} else if maxDiff <= 20.0 {
			result.Confidence = "medium"
			result.VerificationNote = fmt.Sprintf("Sources vary by ±%.0fm", maxDiff)
		} else {
			result.Confidence = "low"
			result.VerificationNote = fmt.Sprintf("Sources disagree by ±%.0fm", maxDiff)
		}
	} else {
		result.Confidence = "low"
		result.VerificationNote = "Limited verification sources available"
	}

	return result
}

// CreateImprovedLocationFromGPS converts GPS data to improved standardized format
func CreateImprovedLocationFromGPS(gpsData *QuectelGPSData) *ImprovedStandardizedLocationResponse {
	// Calculate accuracy from HDOP (decimal value)
	accuracy := gpsData.HDOP * 5.0
	if accuracy < 2.0 {
		accuracy = 2.0
	}

	// Determine fix type (integer only)
	fixType := 0 // No Fix
	if gpsData.Latitude != 0 && gpsData.Longitude != 0 {
		if gpsData.Altitude != 0 {
			fixType = 2 // 3D Fix
		} else {
			fixType = 1 // 2D Fix
		}
	}

	// Convert speed to m/s (from km/h)
	speedMs := gpsData.SpeedKmh / 3.6

	// Verify altitude
	altVerification := verifyAltitudeFromMultipleSources(gpsData.Latitude, gpsData.Longitude, &gpsData.Altitude, nil)
	altitudeVerified := altVerification.Confidence == "high" || altVerification.Confidence == "medium"

	return &ImprovedStandardizedLocationResponse{
		// Core data (full precision)
		Latitude:  gpsData.Latitude,
		Longitude: gpsData.Longitude,
		Accuracy:  accuracy, // Decimal value in meters
		Timestamp: time.Now(),

		// Enhanced GPS data
		Altitude: &gpsData.Altitude,
		Speed:    &speedMs, // m/s instead of km/h
		Course:   &gpsData.Course,

		// Simplified fix information (integer only)
		FixType: fixType,

		// Enhanced source information
		Source:      fmt.Sprintf("GPS (%d satellites)", gpsData.Satellites),
		Method:      "gps",
		DataSources: []string{"quectel_gnss"},

		// Quality indicators
		HDOP:       &gpsData.HDOP,
		Satellites: &gpsData.Satellites,

		// Metadata (no API cost)
		FromCache:    false,
		ResponseTime: 0,

		// Reliability
		Valid: gpsData.Latitude != 0 && gpsData.Longitude != 0,

		// Altitude verification
		AltitudeSource:      "gps",
		AltitudeNote:        fmt.Sprintf("GPS altitude: %.1fm, Consensus: %.1fm (%s confidence)", gpsData.Altitude, altVerification.Consensus, altVerification.Confidence),
		AltitudeVerified:    &altitudeVerified,
		AltitudeVerifyError: nil,
	}
}

// CreateImprovedLocationFromGoogle converts Google API response to improved standardized format
func CreateImprovedLocationFromGoogle(resp *maps.GeolocationResult, wifiCount, cellCount int) *ImprovedStandardizedLocationResponse {
	// Determine fix type based on accuracy (integer only)
	fixType := 0 // No Fix
	if resp.Location.Lat != 0 && resp.Location.Lng != 0 {
		if resp.Accuracy < 2000.0 {
			fixType = 1 // 2D Fix (Google doesn't provide real altitude)
		}
	}

	// Create enhanced source description
	var sourceDetails []string
	if cellCount > 0 {
		sourceDetails = append(sourceDetails, fmt.Sprintf("%d Cell", cellCount))
	}
	if wifiCount > 0 {
		sourceDetails = append(sourceDetails, fmt.Sprintf("%d WiFi", wifiCount))
	}
	source := fmt.Sprintf("Google (%s)", strings.Join(sourceDetails, " + "))

	// Verify altitude (estimate from APIs)
	altVerification := verifyAltitudeFromMultipleSources(resp.Location.Lat, resp.Location.Lng, nil, nil)
	estimatedAltitude := altVerification.Consensus
	altitudeVerified := altVerification.Confidence == "high"

	// Determine data sources used
	var dataSources []string
	if wifiCount > 0 {
		dataSources = append(dataSources, "wifi")
	}
	if cellCount > 0 {
		dataSources = append(dataSources, "cellular")
	}

	return &ImprovedStandardizedLocationResponse{
		// Core data (full precision)
		Latitude:  resp.Location.Lat,
		Longitude: resp.Location.Lng,
		Accuracy:  resp.Accuracy, // Decimal value in meters
		Timestamp: time.Now(),

		// Enhanced data (compensated)
		Altitude: &estimatedAltitude, // Estimated from elevation APIs
		Speed:    nil,                // Not available from Google API
		Course:   nil,                // Not available from Google API

		// Simplified fix information (integer only)
		FixType: fixType,

		// Enhanced source information
		Source:      source,
		Method:      "google_api",
		DataSources: dataSources,

		// Quality indicators
		HDOP:       nil, // Not applicable for Google API
		Satellites: nil, // Not applicable for Google API

		// Metadata (no API cost shown)
		FromCache:    false,
		ResponseTime: 0,

		// Reliability
		Valid: resp.Location.Lat != 0 && resp.Location.Lng != 0,

		// Altitude verification
		AltitudeSource:      "api",
		AltitudeNote:        altVerification.VerificationNote,
		AltitudeVerified:    &altitudeVerified,
		AltitudeVerifyError: nil,
	}
}

// PrintImprovedLocationResponse prints detailed improved location information
func PrintImprovedLocationResponse(resp *ImprovedStandardizedLocationResponse) {
	fmt.Printf("📍 Location: %.6f°, %.6f° (±%.1fm)\n", resp.Latitude, resp.Longitude, resp.Accuracy)
	fmt.Printf("📡 Source: %s (%s)\n", resp.Source, resp.Method)
	fmt.Printf("🎯 Fix Type: %d\n", resp.FixType) // Integer only

	// Altitude with verification info
	if resp.Altitude != nil {
		fmt.Printf("🏔️  Altitude: %.1fm (%s)\n", *resp.Altitude, resp.AltitudeSource)
		if resp.AltitudeVerified != nil {
			if *resp.AltitudeVerified {
				fmt.Printf("    ✅ Verified: %s\n", resp.AltitudeNote)
			} else {
				fmt.Printf("    ⚠️  Unverified: %s\n", resp.AltitudeNote)
			}
		}
	} else {
		fmt.Printf("🏔️  Altitude: N/A\n")
	}

	// Speed in m/s
	if resp.Speed != nil {
		fmt.Printf("🚗 Speed: %.2f m/s (%.1f km/h)\n", *resp.Speed, *resp.Speed*3.6)
	}

	// Enhanced satellite/data source info
	if resp.Satellites != nil {
		fmt.Printf("🛰️  Satellites: %d\n", *resp.Satellites)
	}
	if resp.HDOP != nil {
		fmt.Printf("📊 HDOP: %.1f\n", *resp.HDOP)
	}
	if len(resp.DataSources) > 0 {
		fmt.Printf("📊 Data Sources: %v\n", resp.DataSources)
	}

	fmt.Printf("💾 Cached: %t\n", resp.FromCache)
	fmt.Printf("⏱️  Response Time: %v\n", resp.ResponseTime)
	fmt.Printf("✅ Valid: %t\n", resp.Valid)
}

// testImprovedStandardizedLocation demonstrates the improved standardized location system
func testImprovedStandardizedLocation() {
	fmt.Println("🚀 Improved Standardized Location Response Test")
	fmt.Println("==============================================")

	fmt.Println("\n📊 Key Improvements:")
	fmt.Println("  ✅ Fix Type: Integer only (0, 1, 2, 3)")
	fmt.Println("  ✅ Accuracy: Decimal value in meters")
	fmt.Println("  ✅ Speed: Displayed in m/s (with km/h conversion)")
	fmt.Println("  ✅ API Cost: Removed (not interesting)")
	fmt.Println("  ✅ Confidence: Removed (was unclear)")
	fmt.Println("  ✅ Coordinates: Full precision preserved")
	fmt.Println("  ✅ Altitude: Multi-source verification")

	// Example 1: GPS Response with altitude verification
	fmt.Println("\n📍 Example 1: GPS Response (with altitude verification)")
	fmt.Println("=====================================================")

	// Convert the DMS coordinates we got: 5928.803965,N,01816.791167,E
	lat := convertDMSToDecimal("5928.803965", "N")
	lon := convertDMSToDecimal("01816.791167", "E")

	gpsData := &QuectelGPSData{
		Latitude:   lat, // Full precision from DMS conversion
		Longitude:  lon, // Full precision from DMS conversion
		Altitude:   9.6, // From the QGPSLOC response
		SpeedKmh:   0.0,
		Course:     0.0,
		Satellites: 39,  // From the QGPSLOC response
		HDOP:       0.4, // From the QGPSLOC response
	}

	gpsResponse := CreateImprovedLocationFromGPS(gpsData)
	PrintImprovedLocationResponse(gpsResponse)

	// Example 2: Google API Response
	fmt.Println("\n🌐 Example 2: Google API Response")
	fmt.Println("=================================")
	googleResp := &maps.GeolocationResult{
		Location: maps.LatLng{Lat: 59.479826, Lng: 18.279921},
		Accuracy: 45.0,
	}

	googleResponse := CreateImprovedLocationFromGoogle(googleResp, 9, 7)
	PrintImprovedLocationResponse(googleResponse)

	fmt.Println("\n🏔️  Altitude Verification Explanation:")
	fmt.Println("=====================================")
	fmt.Println("GPS shows 9.6m altitude, which seems more reasonable than 25.5m")
	fmt.Println("This matches better with Stockholm archipelago sea level areas")
	fmt.Println("Open Elevation API will be used to verify against terrain data")

	fmt.Println("\n🎯 Data Type Changes:")
	fmt.Println("====================")
	fmt.Printf("Fix Type: %d (integer)\n", gpsResponse.FixType)
	fmt.Printf("Accuracy: %.1f (decimal meters)\n", gpsResponse.Accuracy)
	if gpsResponse.Speed != nil {
		fmt.Printf("Speed: %.2f m/s (was km/h)\n", *gpsResponse.Speed)
	}
	fmt.Println("API Cost: Removed")
	fmt.Println("Confidence: Removed (was unclear)")

	fmt.Println("\n🔍 Coordinate Precision:")
	fmt.Println("========================")
	fmt.Printf("GPS Decimal: %.8f°, %.8f°\n", lat, lon)
	fmt.Printf("DMS Source: 5928.803965,N,01816.791167,E\n")
	fmt.Println("✅ Full precision preserved from conversion")
}
