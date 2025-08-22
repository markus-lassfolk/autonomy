package main

import (
	"fmt"
	"math"
	"strconv"
	"strings"
	"time"

	"golang.org/x/crypto/ssh"
)

// GPSLocationData represents GPS data with accuracy for mapping
type GPSLocationData struct {
	Source     string
	Latitude   float64
	Longitude  float64
	Accuracy   float64 // meters
	Satellites int
	HDOP       float64
	FixType    int
	Timestamp  time.Time
	Notes      string
}

// GPSMapsComparison generates Google Maps links with accuracy circles
type GPSMapsComparison struct {
	sshClient *ssh.Client
	locations []GPSLocationData
}

// NewGPSMapsComparison creates a new GPS maps comparison
func NewGPSMapsComparison(sshClient *ssh.Client) *GPSMapsComparison {
	return &GPSMapsComparison{
		sshClient: sshClient,
		locations: []GPSLocationData{},
	}
}

// CollectAllGPSLocations collects GPS data from all available sources
func (gmc *GPSMapsComparison) CollectAllGPSLocations() error {
	fmt.Println("📍 Collecting GPS locations from all sources...")

	// Clear previous locations
	gmc.locations = []GPSLocationData{}

	// 1. Collect GPS data from gpsctl (highest precision coordinates)
	if gpsctlData, err := gmc.collectGPSCtlLocation(); err == nil {
		gmc.locations = append(gmc.locations, gpsctlData)
	} else {
		fmt.Printf("⚠️  gpsctl collection failed: %v\n", err)
	}

	// 2. Collect GPS data from AT command (comprehensive satellite data)
	if atData, err := gmc.collectATCommandLocation(); err == nil {
		gmc.locations = append(gmc.locations, atData)
	} else {
		fmt.Printf("⚠️  AT command collection failed: %v\n", err)
	}

	// 3. Simulate Starlink GPS data (would be from actual API in production)
	starlinkData := gmc.simulateStarlinkLocation()
	gmc.locations = append(gmc.locations, starlinkData)

	// 4. Simulate Google Geolocation API data
	if googleData, err := gmc.simulateGoogleLocation(); err == nil {
		gmc.locations = append(gmc.locations, googleData)
	} else {
		fmt.Printf("⚠️  Google location simulation failed: %v\n", err)
	}

	return nil
}

// collectGPSCtlLocation collects GPS data using gpsctl commands
func (gmc *GPSMapsComparison) collectGPSCtlLocation() (GPSLocationData, error) {
	// Get coordinates with highest precision
	latStr, err := executeCommand(gmc.sshClient, "gpsctl -i")
	if err != nil {
		return GPSLocationData{}, fmt.Errorf("failed to get latitude: %v", err)
	}
	lat, err := strconv.ParseFloat(strings.TrimSpace(latStr), 64)
	if err != nil {
		return GPSLocationData{}, fmt.Errorf("failed to parse latitude: %v", err)
	}

	lonStr, err := executeCommand(gmc.sshClient, "gpsctl -x")
	if err != nil {
		return GPSLocationData{}, fmt.Errorf("failed to get longitude: %v", err)
	}
	lon, err := strconv.ParseFloat(strings.TrimSpace(lonStr), 64)
	if err != nil {
		return GPSLocationData{}, fmt.Errorf("failed to parse longitude: %v", err)
	}

	// Get accuracy directly from gpsctl
	accuracyStr, err := executeCommand(gmc.sshClient, "gpsctl -u")
	accuracy := 5.0 // Default fallback
	if err == nil {
		if acc, parseErr := strconv.ParseFloat(strings.TrimSpace(accuracyStr), 64); parseErr == nil {
			accuracy = acc
		}
	}

	// Get satellite count
	satStr, err := executeCommand(gmc.sshClient, "gpsctl -p")
	satellites := 0
	if err == nil {
		if sats, parseErr := strconv.Atoi(strings.TrimSpace(satStr)); parseErr == nil {
			satellites = sats
		}
	}

	return GPSLocationData{
		Source:     "GPS (gpsctl)",
		Latitude:   lat,
		Longitude:  lon,
		Accuracy:   accuracy,
		Satellites: satellites,
		HDOP:       0.0, // Not available from gpsctl
		FixType:    2,   // Assume 3D fix
		Timestamp:  time.Now(),
		Notes:      fmt.Sprintf("6-decimal precision, direct accuracy measurement, %d satellites", satellites),
	}, nil
}

// collectATCommandLocation collects GPS data using AT commands
func (gmc *GPSMapsComparison) collectATCommandLocation() (GPSLocationData, error) {
	output, err := executeCommand(gmc.sshClient, "gsmctl -A 'AT+QGPSLOC=2'")
	if err != nil {
		return GPSLocationData{}, fmt.Errorf("AT command failed: %v", err)
	}

	gpsData := parseQGPSLOC(output)
	if gpsData == nil {
		return GPSLocationData{}, fmt.Errorf("failed to parse GPS data")
	}

	// Estimate accuracy from HDOP (HDOP × 1.0 for excellent conditions with 0.4 HDOP)
	estimatedAccuracy := gpsData.HDOP * 1.0
	if estimatedAccuracy < 0.5 {
		estimatedAccuracy = 0.5 // Minimum realistic accuracy
	}

	return GPSLocationData{
		Source:     "GPS (AT Command)",
		Latitude:   gpsData.Latitude,
		Longitude:  gpsData.Longitude,
		Accuracy:   estimatedAccuracy,
		Satellites: gpsData.Satellites,
		HDOP:       gpsData.HDOP,
		FixType:    gpsData.FixType,
		Timestamp:  time.Now(),
		Notes:      fmt.Sprintf("5-decimal precision, HDOP %.1f, %d satellites", gpsData.HDOP, gpsData.Satellites),
	}, nil
}

// simulateStarlinkLocation simulates Starlink GPS data
func (gmc *GPSMapsComparison) simulateStarlinkLocation() GPSLocationData {
	return GPSLocationData{
		Source:     "Starlink Multi-API",
		Latitude:   59.48005181,
		Longitude:  18.27987656,
		Accuracy:   5.0, // Using uncertainty_meters
		Satellites: 14,
		HDOP:       0.0, // Not provided by Starlink
		FixType:    3,   // 3D fix
		Timestamp:  time.Now(),
		Notes:      "High precision from get_location + get_status + get_diagnostics APIs",
	}
}

// simulateGoogleLocation simulates Google Geolocation API data
func (gmc *GPSMapsComparison) simulateGoogleLocation() (GPSLocationData, error) {
	// Get cellular data for context
	cellIntel, err := collectCellularLocationIntelligence(gmc.sshClient)
	if err != nil {
		return GPSLocationData{}, fmt.Errorf("failed to collect cellular data: %v", err)
	}

	return GPSLocationData{
		Source:     "Google Geolocation",
		Latitude:   59.47982600,
		Longitude:  18.27992100,
		Accuracy:   45.0, // Typical cellular + WiFi accuracy
		Satellites: 0,    // Not applicable
		HDOP:       0.0,  // Not applicable
		FixType:    1,    // 2D fix (no altitude)
		Timestamp:  time.Now(),
		Notes:      fmt.Sprintf("Cellular + WiFi triangulation, %d cell towers", len(cellIntel.NeighborCells)+1),
	}, nil
}

// GenerateGoogleMapsLinks creates Google Maps links with accuracy circles
func (gmc *GPSMapsComparison) GenerateGoogleMapsLinks() {
	fmt.Println("\n🗺️  Google Maps Links with Accuracy Circles")
	fmt.Println("============================================")

	for i, location := range gmc.locations {
		fmt.Printf("\n%d. %s\n", i+1, location.Source)
		fmt.Println(strings.Repeat("-", len(location.Source)+3))

		// Basic Google Maps link
		basicURL := fmt.Sprintf("https://www.google.com/maps?q=%.8f,%.8f",
			location.Latitude, location.Longitude)
		fmt.Printf("📍 Basic Link: %s\n", basicURL)

		// Google Maps link with marker and zoom level based on accuracy
		zoomLevel := gmc.calculateZoomLevel(location.Accuracy)
		markerURL := fmt.Sprintf("https://www.google.com/maps/@%.8f,%.8f,%dz",
			location.Latitude, location.Longitude, zoomLevel)
		fmt.Printf("🎯 Zoomed Link: %s\n", markerURL)

		// Create a custom Google Maps URL with accuracy circle visualization
		circleURL := gmc.createAccuracyCircleURL(location)
		fmt.Printf("⭕ Accuracy Circle: %s\n", circleURL)

		// Display location details
		fmt.Printf("📊 Details: %.8f°, %.8f° (±%.1fm, %d sats)\n",
			location.Latitude, location.Longitude, location.Accuracy, location.Satellites)
		fmt.Printf("📝 Notes: %s\n", location.Notes)
	}

	// Create comparison map with all locations
	comparisonURL := gmc.createComparisonMapURL()
	fmt.Printf("\n🗺️  All Locations Comparison Map:\n")
	fmt.Printf("📍 %s\n", comparisonURL)
}

// calculateZoomLevel determines appropriate zoom level based on accuracy
func (gmc *GPSMapsComparison) calculateZoomLevel(accuracyMeters float64) int {
	// Zoom levels for different accuracy ranges
	if accuracyMeters <= 1 {
		return 20 // Very high precision
	} else if accuracyMeters <= 5 {
		return 19 // High precision
	} else if accuracyMeters <= 10 {
		return 18 // Good precision
	} else if accuracyMeters <= 50 {
		return 17 // Moderate precision
	} else {
		return 16 // Lower precision
	}
}

// createAccuracyCircleURL creates a Google Maps URL with accuracy circle
func (gmc *GPSMapsComparison) createAccuracyCircleURL(location GPSLocationData) string {
	// Create a My Maps style URL with circle approximation using multiple points
	circlePoints := gmc.generateCirclePoints(location.Latitude, location.Longitude, location.Accuracy, 16)

	// Build URL with multiple markers to approximate a circle
	baseURL := "https://www.google.com/maps/dir/"

	// Add center point
	baseURL += fmt.Sprintf("%.8f,%.8f", location.Latitude, location.Longitude)

	// Add circle points (limited to avoid URL length issues)
	for i := 0; i < len(circlePoints) && i < 8; i += 2 {
		baseURL += fmt.Sprintf("/%.8f,%.8f", circlePoints[i].Lat, circlePoints[i].Lon)
	}

	return baseURL
}

// CirclePoint represents a point on the accuracy circle
type CirclePoint struct {
	Lat float64
	Lon float64
}

// generateCirclePoints generates points around the GPS location to represent accuracy circle
func (gmc *GPSMapsComparison) generateCirclePoints(centerLat, centerLon, radiusMeters float64, numPoints int) []CirclePoint {
	points := make([]CirclePoint, numPoints)

	// Earth's radius in meters
	earthRadius := 6371000.0

	// Convert radius to degrees (approximate)
	radiusLat := radiusMeters / earthRadius * (180.0 / math.Pi)
	radiusLon := radiusMeters / (earthRadius * math.Cos(centerLat*math.Pi/180.0)) * (180.0 / math.Pi)

	for i := 0; i < numPoints; i++ {
		angle := 2.0 * math.Pi * float64(i) / float64(numPoints)

		lat := centerLat + radiusLat*math.Sin(angle)
		lon := centerLon + radiusLon*math.Cos(angle)

		points[i] = CirclePoint{Lat: lat, Lon: lon}
	}

	return points
}

// createComparisonMapURL creates a single map showing all GPS locations
func (gmc *GPSMapsComparison) createComparisonMapURL() string {
	if len(gmc.locations) == 0 {
		return "No locations available"
	}

	// Calculate center point (average of all locations)
	var avgLat, avgLon float64
	for _, loc := range gmc.locations {
		avgLat += loc.Latitude
		avgLon += loc.Longitude
	}
	avgLat /= float64(len(gmc.locations))
	avgLon /= float64(len(gmc.locations))

	// Create Google Maps URL with multiple markers
	baseURL := "https://www.google.com/maps/dir/"

	for i, location := range gmc.locations {
		if i > 0 {
			baseURL += "/"
		}
		baseURL += fmt.Sprintf("%.8f,%.8f", location.Latitude, location.Longitude)
	}

	return baseURL
}

// CalculateDistances calculates distances between all GPS sources
func (gmc *GPSMapsComparison) CalculateDistances() {
	fmt.Println("\n📏 Distance Analysis Between GPS Sources")
	fmt.Println("=======================================")

	if len(gmc.locations) < 2 {
		fmt.Println("Need at least 2 locations for distance analysis")
		return
	}

	// Calculate distances between all pairs
	for i := 0; i < len(gmc.locations); i++ {
		for j := i + 1; j < len(gmc.locations); j++ {
			loc1 := gmc.locations[i]
			loc2 := gmc.locations[j]

			distance := calculateHaversineDistanceForMaps(
				loc1.Latitude, loc1.Longitude,
				loc2.Latitude, loc2.Longitude,
			)

			fmt.Printf("📍 %s ↔ %s: %.1f meters\n",
				loc1.Source, loc2.Source, distance)

			// Compare with combined accuracy
			combinedAccuracy := loc1.Accuracy + loc2.Accuracy
			if distance > combinedAccuracy {
				fmt.Printf("   ⚠️  Distance (%.1fm) exceeds combined accuracy (%.1fm)\n",
					distance, combinedAccuracy)
			} else {
				fmt.Printf("   ✅ Distance within combined accuracy range\n")
			}
		}
	}
}

// calculateHaversineDistanceForMaps calculates the distance between two GPS coordinates
func calculateHaversineDistanceForMaps(lat1, lon1, lat2, lon2 float64) float64 {
	const earthRadiusM = 6371000 // Earth's radius in meters

	lat1Rad := lat1 * math.Pi / 180
	lon1Rad := lon1 * math.Pi / 180
	lat2Rad := lat2 * math.Pi / 180
	lon2Rad := lon2 * math.Pi / 180

	deltaLat := lat2Rad - lat1Rad
	deltaLon := lon2Rad - lon1Rad

	a := math.Sin(deltaLat/2)*math.Sin(deltaLat/2) +
		math.Cos(lat1Rad)*math.Cos(lat2Rad)*
			math.Sin(deltaLon/2)*math.Sin(deltaLon/2)

	c := 2 * math.Atan2(math.Sqrt(a), math.Sqrt(1-a))

	return earthRadiusM * c
}

// DisplayLocationSummary displays a summary table of all locations
func (gmc *GPSMapsComparison) DisplayLocationSummary() {
	fmt.Println("\n📊 GPS Location Summary Table")
	fmt.Println("============================")

	// Header
	fmt.Printf("%-20s %-12s %-12s %-10s %-8s %-10s %s\n",
		"Source", "Latitude", "Longitude", "Accuracy", "Sats", "HDOP", "Notes")
	fmt.Println(strings.Repeat("=", 100))

	// Data rows
	for _, location := range gmc.locations {
		hdopStr := "N/A"
		if location.HDOP > 0 {
			hdopStr = fmt.Sprintf("%.1f", location.HDOP)
		}

		fmt.Printf("%-20s %-12.8f %-12.8f %-10.1f %-8d %-10s %s\n",
			truncateString(location.Source, 20),
			location.Latitude,
			location.Longitude,
			location.Accuracy,
			location.Satellites,
			hdopStr,
			truncateString(location.Notes, 40))
	}
}

// testGPSMapsComparison tests the GPS maps comparison functionality
func testGPSMapsComparison() {
	fmt.Println("🗺️  GPS Maps Comparison Test")
	fmt.Println("============================")

	// Check if we're running locally on RUTOS
	isLocal := isRunningOnRUTOS()
	var client *ssh.Client
	var err error

	if isLocal {
		fmt.Println("🏠 Running locally on RUTOS - using direct access")
		client = nil // No SSH client needed
	} else {
		// Create SSH client for remote access
		client, err = createSSHClient()
		if err != nil {
			fmt.Printf("❌ Failed to create SSH client: %v\n", err)
			return
		}
		defer client.Close()
	}

	// Create GPS maps comparison
	gmc := NewGPSMapsComparison(client)

	// Collect all GPS locations
	if err := gmc.CollectAllGPSLocations(); err != nil {
		fmt.Printf("❌ Failed to collect GPS locations: %v\n", err)
		return
	}

	// Display location summary
	gmc.DisplayLocationSummary()

	// Calculate distances between sources
	gmc.CalculateDistances()

	// Generate Google Maps links
	gmc.GenerateGoogleMapsLinks()

	fmt.Println("\n🎯 GPS Maps Comparison Complete!")
	fmt.Println("Click the links above to view each GPS source location with accuracy circles.")
}
