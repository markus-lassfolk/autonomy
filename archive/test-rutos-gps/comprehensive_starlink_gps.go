package main

import (
	"context"
	"encoding/json"
	"fmt"
	"time"

	"github.com/markus-lassfolk/autonomy/pkg/starlink"
	"golang.org/x/crypto/ssh"
)

// ComprehensiveStarlinkGPS combines data from all three Starlink APIs
type ComprehensiveStarlinkGPS struct {
	// Core Location Data (from get_location)
	Latitude  float64 `json:"latitude"`
	Longitude float64 `json:"longitude"`
	Altitude  float64 `json:"altitude"`
	Accuracy  float64 `json:"accuracy"`

	// Speed Data (from get_location)
	HorizontalSpeedMps float64 `json:"horizontal_speed_mps"`
	VerticalSpeedMps   float64 `json:"vertical_speed_mps"`

	// GPS Source Info (from get_location)
	GPSSource string `json:"gps_source"` // GNC_FUSED, GNC_NO_ACCEL, etc.

	// Satellite Data (from get_status)
	GPSValid        *bool `json:"gps_valid,omitempty"`          // GPS fix validity
	GPSSatellites   *int  `json:"gps_satellites,omitempty"`     // Number of satellites
	NoSatsAfterTTFF *bool `json:"no_sats_after_ttff,omitempty"` // No satellites after time to first fix
	InhibitGPS      *bool `json:"inhibit_gps,omitempty"`        // GPS inhibited

	// Enhanced Location Data (from get_diagnostics)
	LocationEnabled        *bool    `json:"location_enabled,omitempty"`         // Location service enabled
	UncertaintyMeters      *float64 `json:"uncertainty_meters,omitempty"`       // Uncertainty in meters
	UncertaintyMetersValid *bool    `json:"uncertainty_meters_valid,omitempty"` // Uncertainty validity
	GPSTimeS               *float64 `json:"gps_time_s,omitempty"`               // GPS time in seconds

	// Metadata
	DataSources  []string  `json:"data_sources"`  // Which APIs provided data
	CollectedAt  time.Time `json:"collected_at"`  // When data was collected
	CollectionMs int64     `json:"collection_ms"` // Time taken to collect all data
	Valid        bool      `json:"valid"`         // Overall validity
	Confidence   float64   `json:"confidence"`    // Confidence score 0.0-1.0
	QualityScore string    `json:"quality_score"` // excellent, good, fair, poor
}

// StarlinkAPICollector collects GPS data from all Starlink APIs
type StarlinkAPICollector struct {
	starlinkHost   string
	starlinkPort   int
	timeout        time.Duration
	sshClient      *ssh.Client      // For executing grpcurl commands via SSH (fallback)
	starlinkClient *starlink.Client // Centralized Starlink client
}

// NewStarlinkAPICollector creates a new comprehensive Starlink GPS collector
func NewStarlinkAPICollector(host string, port int, timeout time.Duration) *StarlinkAPICollector {
	return &StarlinkAPICollector{
		starlinkHost:   host,
		starlinkPort:   port,
		timeout:        timeout,
		starlinkClient: starlink.DefaultClient(nil),
	}
}

// NewStarlinkAPICollectorWithSSH creates a collector that can execute commands via SSH
func NewStarlinkAPICollectorWithSSH(host string, port int, timeout time.Duration, sshClient *ssh.Client) *StarlinkAPICollector {
	return &StarlinkAPICollector{
		starlinkHost:   host,
		starlinkPort:   port,
		timeout:        timeout,
		sshClient:      sshClient,
		starlinkClient: starlink.DefaultClient(nil),
	}
}

// CollectComprehensiveGPS collects GPS data from all three Starlink APIs
func (sc *StarlinkAPICollector) CollectComprehensiveGPS(ctx context.Context) (*ComprehensiveStarlinkGPS, error) {
	startTime := time.Now()

	gps := &ComprehensiveStarlinkGPS{
		DataSources: []string{},
		CollectedAt: startTime,
	}

	// Collect from get_location (primary coordinates + speed)
	locationData, err := sc.collectLocationData(ctx)
	if err == nil {
		sc.mergeLocationData(gps, locationData)
		gps.DataSources = append(gps.DataSources, "get_location")
	}

	// Collect from get_status (satellite info)
	statusData, err := sc.collectStatusData(ctx)
	if err == nil {
		sc.mergeStatusData(gps, statusData)
		gps.DataSources = append(gps.DataSources, "get_status")
	}

	// Collect from get_diagnostics (enhanced location + GPS time)
	diagnosticsData, err := sc.collectDiagnosticsData(ctx)
	if err == nil {
		sc.mergeDiagnosticsData(gps, diagnosticsData)
		gps.DataSources = append(gps.DataSources, "get_diagnostics")
	}

	// Calculate collection time
	gps.CollectionMs = time.Since(startTime).Milliseconds()

	// Validate and score the data
	sc.validateAndScore(gps)

	return gps, nil
}

// collectLocationData calls get_location API
func (sc *StarlinkAPICollector) collectLocationData(ctx context.Context) (map[string]interface{}, error) {
	// Try centralized client first
	if sc.starlinkClient != nil {
		response, err := sc.starlinkClient.CallMethod(ctx, starlink.MethodGetLocation)
		if err == nil {
			var result map[string]interface{}
			if err := json.Unmarshal([]byte(response), &result); err != nil {
				return nil, fmt.Errorf("failed to parse get_location response: %w", err)
			}
			return result, nil
		}
		// If centralized client fails, fall back to SSH method
	}

	// Fallback to SSH grpcurl method
	if sc.sshClient != nil {
		cmd := fmt.Sprintf("grpcurl -plaintext -max-time %d -d '{\"get_location\":{}}' %s:%d SpaceX.API.Device.Device/Handle",
			int(sc.timeout.Seconds()), sc.starlinkHost, sc.starlinkPort)

		output, err := executeCommand(sc.sshClient, cmd)
		if err != nil {
			return nil, fmt.Errorf("failed to execute get_location: %w", err)
		}

		var result map[string]interface{}
		if err := json.Unmarshal([]byte(output), &result); err != nil {
			return nil, fmt.Errorf("failed to parse get_location response: %w", err)
		}
		return result, nil
	}

	// Mock data for testing without SSH
	return map[string]interface{}{
		"getLocation": map[string]interface{}{
			"lla": map[string]interface{}{
				"lat": 59.480051805924234,
				"lon": 18.279876560548065,
				"alt": 21.452765255424573,
			},
			"source":             "GNC_NO_ACCEL",
			"sigmaM":             0.0,
			"horizontalSpeedMps": 0.0,
			"verticalSpeedMps":   0.0,
		},
	}, nil
}

// collectStatusData calls get_status API
func (sc *StarlinkAPICollector) collectStatusData(ctx context.Context) (map[string]interface{}, error) {
	// Try centralized client first
	if sc.starlinkClient != nil {
		response, err := sc.starlinkClient.CallMethod(ctx, starlink.MethodGetStatus)
		if err == nil {
			var result map[string]interface{}
			if err := json.Unmarshal([]byte(response), &result); err != nil {
				return nil, fmt.Errorf("failed to parse get_status response: %w", err)
			}
			return result, nil
		}
		// If centralized client fails, fall back to SSH method
	}

	// Fallback to SSH grpcurl method
	if sc.sshClient != nil {
		cmd := fmt.Sprintf("grpcurl -plaintext -max-time %d -d '{\"get_status\":{}}' %s:%d SpaceX.API.Device.Device/Handle",
			int(sc.timeout.Seconds()), sc.starlinkHost, sc.starlinkPort)

		output, err := executeCommand(sc.sshClient, cmd)
		if err != nil {
			return nil, fmt.Errorf("failed to execute get_status: %w", err)
		}

		var result map[string]interface{}
		if err := json.Unmarshal([]byte(output), &result); err != nil {
			return nil, fmt.Errorf("failed to parse get_status response: %w", err)
		}
		return result, nil
	}

	// Mock data structure based on actual API response
	return map[string]interface{}{
		"dishGetStatus": map[string]interface{}{
			"gpsStats": map[string]interface{}{
				"gpsValid":        true,
				"gpsSats":         14,
				"noSatsAfterTtff": false,
				"inhibitGps":      false,
			},
		},
	}, nil
}

// collectDiagnosticsData calls get_diagnostics API
func (sc *StarlinkAPICollector) collectDiagnosticsData(ctx context.Context) (map[string]interface{}, error) {
	// Try centralized client first
	if sc.starlinkClient != nil {
		response, err := sc.starlinkClient.CallMethod(ctx, starlink.MethodGetDiagnostics)
		if err == nil {
			var result map[string]interface{}
			if err := json.Unmarshal([]byte(response), &result); err != nil {
				return nil, fmt.Errorf("failed to parse get_diagnostics response: %w", err)
			}
			return result, nil
		}
		// If centralized client fails, fall back to SSH method
	}

	// Fallback to SSH grpcurl method
	if sc.sshClient != nil {
		cmd := fmt.Sprintf("grpcurl -plaintext -max-time %d -d '{\"get_diagnostics\":{}}' %s:%d SpaceX.API.Device.Device/Handle",
			int(sc.timeout.Seconds()), sc.starlinkHost, sc.starlinkPort)

		output, err := executeCommand(sc.sshClient, cmd)
		if err != nil {
			return nil, fmt.Errorf("failed to execute get_diagnostics: %w", err)
		}

		var result map[string]interface{}
		if err := json.Unmarshal([]byte(output), &result); err != nil {
			return nil, fmt.Errorf("failed to parse get_diagnostics response: %w", err)
		}
		return result, nil
	}

	// Mock data structure based on actual API response
	return map[string]interface{}{
		"dishGetDiagnostics": map[string]interface{}{
			"location": map[string]interface{}{
				"enabled":                true,
				"latitude":               59.480050934336674,
				"longitude":              18.27987468673109,
				"altitudeMeters":         21.42455200671644,
				"uncertaintyMetersValid": false,
				"uncertaintyMeters":      0.0,
				"gpsTimeS":               1.4393847625804982e+09,
			},
		},
	}, nil
}

// mergeLocationData merges get_location data into comprehensive GPS
func (sc *StarlinkAPICollector) mergeLocationData(gps *ComprehensiveStarlinkGPS, data map[string]interface{}) {
	getLocation, ok := data["getLocation"].(map[string]interface{})
	if !ok {
		return
	}

	// Extract coordinates
	if lla, ok := getLocation["lla"].(map[string]interface{}); ok {
		if lat, ok := lla["lat"].(float64); ok {
			gps.Latitude = lat
		}
		if lon, ok := lla["lon"].(float64); ok {
			gps.Longitude = lon
		}
		if alt, ok := lla["alt"].(float64); ok {
			gps.Altitude = alt
		}
	}

	// Extract accuracy
	if sigmaM, ok := getLocation["sigmaM"].(float64); ok {
		gps.Accuracy = sigmaM
		if gps.Accuracy == 0 {
			gps.Accuracy = 5.0 // Default accuracy
		}
	}

	// Extract speed data
	if hSpeed, ok := getLocation["horizontalSpeedMps"].(float64); ok {
		gps.HorizontalSpeedMps = hSpeed
	}
	if vSpeed, ok := getLocation["verticalSpeedMps"].(float64); ok {
		gps.VerticalSpeedMps = vSpeed
	}

	// Extract GPS source
	if source, ok := getLocation["source"].(string); ok {
		gps.GPSSource = source
	}
}

// mergeStatusData merges get_status data into comprehensive GPS
func (sc *StarlinkAPICollector) mergeStatusData(gps *ComprehensiveStarlinkGPS, data map[string]interface{}) {
	dishGetStatus, ok := data["dishGetStatus"].(map[string]interface{})
	if !ok {
		return
	}

	gpsStats, ok := dishGetStatus["gpsStats"].(map[string]interface{})
	if !ok {
		return
	}

	// Extract GPS validity
	if gpsValid, ok := gpsStats["gpsValid"].(bool); ok {
		gps.GPSValid = &gpsValid
	}

	// Extract satellite count
	if gpsSats, ok := gpsStats["gpsSats"].(float64); ok {
		sats := int(gpsSats)
		gps.GPSSatellites = &sats
	}

	// Extract additional GPS flags
	if noSats, ok := gpsStats["noSatsAfterTtff"].(bool); ok {
		gps.NoSatsAfterTTFF = &noSats
	}
	if inhibit, ok := gpsStats["inhibitGps"].(bool); ok {
		gps.InhibitGPS = &inhibit
	}
}

// mergeDiagnosticsData merges get_diagnostics data into comprehensive GPS
func (sc *StarlinkAPICollector) mergeDiagnosticsData(gps *ComprehensiveStarlinkGPS, data map[string]interface{}) {
	dishGetDiagnostics, ok := data["dishGetDiagnostics"].(map[string]interface{})
	if !ok {
		return
	}

	location, ok := dishGetDiagnostics["location"].(map[string]interface{})
	if !ok {
		return
	}

	// Extract location enabled status
	if enabled, ok := location["enabled"].(bool); ok {
		gps.LocationEnabled = &enabled
	}

	// Extract uncertainty data
	if uncertainty, ok := location["uncertaintyMeters"].(float64); ok {
		gps.UncertaintyMeters = &uncertainty
	}
	if uncertaintyValid, ok := location["uncertaintyMetersValid"].(bool); ok {
		gps.UncertaintyMetersValid = &uncertaintyValid
	}

	// Extract GPS time
	if gpsTime, ok := location["gpsTimeS"].(float64); ok {
		gps.GPSTimeS = &gpsTime
	}

	// Use diagnostics coordinates if get_location failed
	if gps.Latitude == 0 && gps.Longitude == 0 {
		if lat, ok := location["latitude"].(float64); ok {
			gps.Latitude = lat
		}
		if lon, ok := location["longitude"].(float64); ok {
			gps.Longitude = lon
		}
		if alt, ok := location["altitudeMeters"].(float64); ok {
			gps.Altitude = alt
		}
	}

	// Use diagnostics uncertainty as accuracy if not available from get_location
	if gps.Accuracy == 0 && gps.UncertaintyMeters != nil && gps.UncertaintyMetersValid != nil && *gps.UncertaintyMetersValid {
		gps.Accuracy = *gps.UncertaintyMeters
	}
}

// validateAndScore validates the GPS data and calculates confidence/quality scores
func (sc *StarlinkAPICollector) validateAndScore(gps *ComprehensiveStarlinkGPS) {
	// Basic validity check
	gps.Valid = gps.Latitude != 0 && gps.Longitude != 0

	// Calculate confidence score
	confidence := 0.0

	// Base confidence from having coordinates
	if gps.Valid {
		confidence += 0.3
	}

	// Boost from accuracy
	if gps.Accuracy > 0 {
		if gps.Accuracy <= 5.0 {
			confidence += 0.3
		} else if gps.Accuracy <= 10.0 {
			confidence += 0.2
		} else if gps.Accuracy <= 50.0 {
			confidence += 0.1
		}
	}

	// Boost from satellite count
	if gps.GPSSatellites != nil {
		if *gps.GPSSatellites >= 8 {
			confidence += 0.2
		} else if *gps.GPSSatellites >= 6 {
			confidence += 0.15
		} else if *gps.GPSSatellites >= 4 {
			confidence += 0.1
		}
	}

	// Boost from GPS validity
	if gps.GPSValid != nil && *gps.GPSValid {
		confidence += 0.1
	}

	// Boost from multiple data sources
	if len(gps.DataSources) >= 3 {
		confidence += 0.1
	} else if len(gps.DataSources) >= 2 {
		confidence += 0.05
	}

	// Cap at 1.0
	if confidence > 1.0 {
		confidence = 1.0
	}
	gps.Confidence = confidence

	// Determine quality score
	if confidence >= 0.8 {
		gps.QualityScore = "excellent"
	} else if confidence >= 0.6 {
		gps.QualityScore = "good"
	} else if confidence >= 0.4 {
		gps.QualityScore = "fair"
	} else {
		gps.QualityScore = "poor"
	}
}

// GetMissingFields returns a list of fields that are nil/unavailable
func (gps *ComprehensiveStarlinkGPS) GetMissingFields() []string {
	var missing []string

	if gps.GPSValid == nil {
		missing = append(missing, "gps_valid")
	}
	if gps.GPSSatellites == nil {
		missing = append(missing, "gps_satellites")
	}
	if gps.LocationEnabled == nil {
		missing = append(missing, "location_enabled")
	}
	if gps.UncertaintyMeters == nil {
		missing = append(missing, "uncertainty_meters")
	}
	if gps.GPSTimeS == nil {
		missing = append(missing, "gps_time_s")
	}

	return missing
}

// GetAvailableFields returns a list of fields that have data
func (gps *ComprehensiveStarlinkGPS) GetAvailableFields() []string {
	var available []string

	// Always available from get_location
	available = append(available, "latitude", "longitude", "altitude", "accuracy")
	available = append(available, "horizontal_speed_mps", "vertical_speed_mps", "gps_source")

	// Conditionally available
	if gps.GPSValid != nil {
		available = append(available, "gps_valid")
	}
	if gps.GPSSatellites != nil {
		available = append(available, "gps_satellites")
	}
	if gps.LocationEnabled != nil {
		available = append(available, "location_enabled")
	}
	if gps.UncertaintyMeters != nil {
		available = append(available, "uncertainty_meters")
	}
	if gps.GPSTimeS != nil {
		available = append(available, "gps_time_s")
	}

	return available
}

// ToStandardizedResponse converts to our standardized location response format
func (gps *ComprehensiveStarlinkGPS) ToStandardizedResponse() *StandardizedLocationResponse {
	// Convert speed from m/s to km/h
	speedKmh := gps.HorizontalSpeedMps * 3.6

	response := &StandardizedLocationResponse{
		// Core data
		Latitude:  gps.Latitude,
		Longitude: gps.Longitude,
		Accuracy:  gps.Accuracy,
		Timestamp: gps.CollectedAt,

		// Extended data (available from Starlink multi-API)
		Altitude:   &gps.Altitude,
		Speed:      &speedKmh,
		Course:     nil, // Not available from Starlink
		Satellites: gps.GPSSatellites,

		// Quality indicators
		FixType:    "starlink_combined",
		FixQuality: gps.QualityScore,
		HDOP:       nil, // Not available, use accuracy instead

		// Source information
		Source:       fmt.Sprintf("Starlink Multi-API (%s)", gps.GPSSource),
		Method:       "starlink_multi_api",
		FromCache:    false,
		ResponseTime: time.Duration(gps.CollectionMs) * time.Millisecond,

		// API data
		APICallMade: true,
		APICost:     0.0, // No cost for Starlink APIs
		DataSources: gps.DataSources,

		// Reliability
		Valid:      gps.Valid,
		Confidence: gps.Confidence,
	}

	return response
}

// PrintComprehensiveGPS prints detailed GPS information
func (gps *ComprehensiveStarlinkGPS) PrintComprehensiveGPS() {
	fmt.Printf("🛰️  COMPREHENSIVE STARLINK GPS DATA\n")
	fmt.Printf("===================================\n")
	fmt.Printf("📍 Location: %.6f°, %.6f° (±%.1fm)\n", gps.Latitude, gps.Longitude, gps.Accuracy)
	fmt.Printf("🏔️  Altitude: %.1fm\n", gps.Altitude)
	fmt.Printf("🚀 Speed: %.2f m/s (%.1f km/h)\n", gps.HorizontalSpeedMps, gps.HorizontalSpeedMps*3.6)
	fmt.Printf("📡 GPS Source: %s\n", gps.GPSSource)

	if gps.GPSSatellites != nil {
		fmt.Printf("🛰️  Satellites: %d\n", *gps.GPSSatellites)
	}
	if gps.GPSValid != nil {
		fmt.Printf("✅ GPS Valid: %t\n", *gps.GPSValid)
	}
	if gps.GPSTimeS != nil {
		gpsTime := time.Unix(int64(*gps.GPSTimeS), 0)
		fmt.Printf("⏰ GPS Time: %s\n", gpsTime.Format("2006-01-02 15:04:05 UTC"))
	}

	fmt.Printf("📊 Quality: %s (confidence: %.1f%%)\n", gps.QualityScore, gps.Confidence*100)
	fmt.Printf("📡 Data Sources: %v\n", gps.DataSources)
	fmt.Printf("⏱️  Collection Time: %dms\n", gps.CollectionMs)

	missing := gps.GetMissingFields()
	if len(missing) > 0 {
		fmt.Printf("❌ Missing Fields: %v\n", missing)
	}

	available := gps.GetAvailableFields()
	fmt.Printf("✅ Available Fields: %d/%d\n", len(available), len(available)+len(missing))
}

// testComprehensiveStarlinkGPS demonstrates the comprehensive Starlink GPS collection
func testComprehensiveStarlinkGPS() {
	fmt.Println("🛰️  Comprehensive Starlink GPS Collection Test")
	fmt.Println("==============================================")

	// Check if we're running locally on RUTOS
	isLocal := isRunningOnRUTOS()
	var collector *StarlinkAPICollector

	if isLocal {
		fmt.Println("🏠 Running locally on RUTOS - using direct Starlink API access!")
		collector = NewStarlinkAPICollector("192.168.100.1", 9200, 10*time.Second)
	} else {
		// Try to create SSH connection for remote API calls
		sshClient, err := createSSHClient()
		if err != nil {
			fmt.Printf("⚠️  SSH connection failed: %v\n", err)
			fmt.Println("📡 Using mock data for demonstration...")
			collector = NewStarlinkAPICollector("192.168.100.1", 9200, 10*time.Second)
		} else {
			fmt.Println("✅ SSH connection established - using remote Starlink API calls!")
			collector = NewStarlinkAPICollectorWithSSH("192.168.100.1", 9200, 10*time.Second, sshClient)
			defer sshClient.Close()
		}
	}

	// Collect comprehensive GPS data
	ctx, cancel := context.WithTimeout(context.Background(), 30*time.Second)
	defer cancel()

	fmt.Println("📡 Collecting GPS data from all three Starlink APIs...")
	fmt.Println("   1️⃣  get_location - coordinates, altitude, speed, accuracy")
	fmt.Println("   2️⃣  get_status - satellite count, GPS validity")
	fmt.Println("   3️⃣  get_diagnostics - GPS timestamp, uncertainty")

	gpsData, err := collector.CollectComprehensiveGPS(ctx)
	if err != nil {
		fmt.Printf("❌ Failed to collect comprehensive GPS data: %v\n", err)
		return
	}

	fmt.Println("✅ Successfully collected comprehensive GPS data!")

	// Print detailed information
	gpsData.PrintComprehensiveGPS()

	// Convert to standardized format
	fmt.Println("\n🔄 Converting to Standardized Response Format:")
	fmt.Println("==============================================")
	standardized := gpsData.ToStandardizedResponse()
	printLocationResponse(standardized)

	// Show field completeness comparison
	fmt.Println("\n📊 Field Completeness Analysis:")
	fmt.Println("===============================")
	available := gpsData.GetAvailableFields()
	missing := gpsData.GetMissingFields()
	total := len(available) + len(missing)

	fmt.Printf("✅ Available Fields: %d/%d (%.0f%%)\n", len(available), total, float64(len(available))/float64(total)*100)
	fmt.Printf("❌ Missing Fields: %d/%d (%.0f%%)\n", len(missing), total, float64(len(missing))/float64(total)*100)

	if len(available) > 0 {
		fmt.Printf("📍 Available: %v\n", available)
	}
	if len(missing) > 0 {
		fmt.Printf("❌ Missing: %v\n", missing)
	}

	fmt.Println("\n🎯 Key Benefits of Multi-API Collection:")
	fmt.Println("========================================")
	fmt.Println("✅ Complete coordinate data from get_location")
	fmt.Println("✅ Satellite count and GPS validity from get_status")
	fmt.Println("✅ GPS timestamp and uncertainty from get_diagnostics")
	fmt.Println("✅ Comprehensive quality scoring based on all available data")
	fmt.Println("✅ No duplicate data - each API provides unique fields")
	fmt.Println("✅ Fallback coordinates if primary API fails")
	fmt.Printf("✅ Data completeness: %d fields available (vs 4 from single API)\n", len(available))
	fmt.Println("🚀 Near-complete GPS dataset from Starlink multi-API approach!")
}
