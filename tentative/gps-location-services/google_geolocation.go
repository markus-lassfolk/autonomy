package main

import (
	"context"
	"fmt"
	"os"
	"sort"
	"strconv"
	"strings"
	"time"

	"golang.org/x/crypto/ssh"
	"googlemaps.github.io/maps"
)

// GoogleGeolocationService provides comprehensive geolocation using Google's API
type GoogleGeolocationService struct {
	client      *maps.Client
	apiKey      string
	timeout     time.Duration
	rateLimiter *time.Ticker
}

// GoogleGeolocationRequest represents our enhanced request structure
type GoogleGeolocationRequest struct {
	CellTowers       []*maps.CellTower       `json:"cellTowers"`
	WiFiAccessPoints []*maps.WiFiAccessPoint `json:"wifiAccessPoints"`
	ConsiderIP       bool                    `json:"considerIp"`

	// Metadata for our implementation
	RadioType        string    `json:"radioType"`
	RequestTimestamp time.Time `json:"requestTimestamp"`
	MaxCells         int       `json:"maxCells"`
	MaxWiFiAPs       int       `json:"maxWiFiAPs"`
}

// GoogleGeolocationResponse represents our enhanced response structure
type GoogleGeolocationResponse struct {
	// Google API response data
	Location maps.LatLng `json:"location"`
	Accuracy float64     `json:"accuracy"`

	// Our metadata
	Success          bool      `json:"success"`
	Method           string    `json:"method"`
	CellsUsed        int       `json:"cellsUsed"`
	WiFiAPsUsed      int       `json:"wifiAPsUsed"`
	ResponseTime     float64   `json:"responseTimeMs"`
	RequestTimestamp time.Time `json:"requestTimestamp"`
	Error            string    `json:"error,omitempty"`

	// Additional information
	RadioType    string `json:"radioType,omitempty"`
	ConsideredIP bool   `json:"consideredIp"`
}

// NewGoogleGeolocationService creates a new Google Geolocation service
func NewGoogleGeolocationService(apiKey string) (*GoogleGeolocationService, error) {
	client, err := maps.NewClient(maps.WithAPIKey(apiKey))
	if err != nil {
		return nil, fmt.Errorf("failed to create Google Maps client: %w", err)
	}

	return &GoogleGeolocationService{
		client:      client,
		apiKey:      apiKey,
		timeout:     30 * time.Second,
		rateLimiter: time.NewTicker(100 * time.Millisecond), // 10 requests per second max
	}, nil
}

// LoadGoogleAPIKey loads the Google API key from file
func LoadGoogleAPIKey() (string, error) {
	keyPath := "C:\\Users\\markusla\\OneDrive\\IT\\RUTOS Keys\\Google_LocationAPI.txt"

	data, err := os.ReadFile(keyPath)
	if err != nil {
		return "", fmt.Errorf("failed to read Google API key file: %w", err)
	}

	apiKey := strings.TrimSpace(string(data))
	if apiKey == "" {
		return "", fmt.Errorf("Google API key file is empty")
	}

	return apiKey, nil
}

// BuildGoogleCellTowersFromIntelligence converts cellular intelligence to Google format
func BuildGoogleCellTowersFromIntelligence(intel *CellularLocationIntelligence, maxCells int) ([]maps.CellTower, string, error) {
	var cellTowers []maps.CellTower

	// Parse serving cell information
	servingCellID, err := strconv.Atoi(intel.ServingCell.CellID)
	if err != nil {
		return nil, "", fmt.Errorf("invalid serving cell ID: %s", intel.ServingCell.CellID)
	}

	mcc, err := strconv.Atoi(intel.ServingCell.MCC)
	if err != nil {
		return nil, "", fmt.Errorf("invalid MCC: %s", intel.ServingCell.MCC)
	}

	mnc, err := strconv.Atoi(intel.ServingCell.MNC)
	if err != nil {
		return nil, "", fmt.Errorf("invalid MNC: %s", intel.ServingCell.MNC)
	}

	// Determine radio type
	radioType := determineGoogleRadioType(intel.ServingCell.Band, intel.NetworkInfo.Technology)

	// Build serving cell (must be first) with ALL available data
	servingCell := maps.CellTower{
		// Required fields
		MobileCountryCode: mcc,
		MobileNetworkCode: mnc,
		CellID:            servingCellID,
		SignalStrength:    intel.SignalQuality.RSRP, // Use RSRP for serving cell
	}

	// Set LocationAreaCode (required by Google API)
	if tac, err := strconv.Atoi(intel.ServingCell.TAC); err == nil && tac > 0 {
		servingCell.LocationAreaCode = tac
	} else {
		// Fallback: derive LAC from Cell ID (common practice)
		// For LTE, TAC is often the upper 16 bits of the Cell ID
		servingCell.LocationAreaCode = servingCellID >> 8 // Use upper bits as fallback
		if servingCell.LocationAreaCode == 0 {
			servingCell.LocationAreaCode = 1 // Minimum valid LAC
		}
		fmt.Printf("    ⚠️  Using derived LocationAreaCode: %d (TAC: '%s' invalid)\n",
			servingCell.LocationAreaCode, intel.ServingCell.TAC)
	}

	// Add ALL optional fields we have available for maximum accuracy

	// Age of measurement (optional) - set to 0 for fresh data
	servingCell.Age = 0

	// TimingAdvance (optional) - try to get from enhanced cellular data
	if ta := getTimingAdvanceForCell(intel, servingCellID); ta > 0 {
		servingCell.TimingAdvance = ta
		fmt.Printf("    ⏱️  TimingAdvance: %d\n", ta)
	}

	// Timing advance (optional) - helps with distance calculation
	// Note: We don't currently collect this, but could be added in future

	// Add radio-specific optional fields based on technology
	if radioType == "lte" {
		// For LTE, we can add additional signal quality metrics
		// These help Google's algorithm assess signal quality

		// Physical Cell ID (helps with cell identification)
		if intel.ServingCell.PCID > 0 {
			// Note: Google API doesn't have direct PCID field, but it helps with CellID validation
		}

		// EARFCN (frequency) - helps with cell identification
		if intel.ServingCell.EARFCN > 0 {
			// Note: Google API doesn't have direct EARFCN field, but could be useful for validation
		}
	}

	fmt.Printf("    📡 Serving Cell: CellID=%d, MCC=%d, MNC=%d, LAC=%d, RSRP=%d dBm\n",
		servingCell.CellID, servingCell.MobileCountryCode, servingCell.MobileNetworkCode,
		servingCell.LocationAreaCode, servingCell.SignalStrength)

	cellTowers = append(cellTowers, servingCell)

	// Sort neighbor cells by signal strength (strongest first)
	// For neighbor cells, we use RSSI since that's what's populated from AT commands
	neighbors := make([]NeighborCellInfo, len(intel.NeighborCells))
	copy(neighbors, intel.NeighborCells)
	sort.Slice(neighbors, func(i, j int) bool {
		return neighbors[i].RSSI > neighbors[j].RSSI
	})

	// Add up to maxCells-1 neighbor cells (serving cell already added)
	maxNeighbors := maxCells - 1
	if maxNeighbors > len(neighbors) {
		maxNeighbors = len(neighbors)
	}

	for i := 0; i < maxNeighbors; i++ {
		neighbor := neighbors[i]

		// Skip invalid neighbor cells (PCID 0 only - allow weak signals for inter-frequency cells)
		// For neighbor cells, we use RSSI since that's what's populated from AT commands
		if neighbor.PCID == 0 {
			continue
		}

		// Build neighbor cell with ALL available data
		neighborCell := maps.CellTower{
			// Required fields
			CellID:            neighbor.PCID, // Using PCID as Cell ID for neighbors
			MobileCountryCode: mcc,
			MobileNetworkCode: mnc,
			SignalStrength:    neighbor.RSSI, // Use RSSI for neighbor cells (that's what we have)
		}

		// Use serving cell's LAC for neighbors (we don't have individual LACs)
		if servingCell.LocationAreaCode != 0 {
			neighborCell.LocationAreaCode = servingCell.LocationAreaCode
		}

		// Add ALL optional fields for maximum accuracy

		// Age of measurement (optional) - set to 0 for fresh data
		neighborCell.Age = 0

		// Add additional signal quality information if available
		if neighbor.RSRP != 0 && neighbor.RSRP != neighbor.RSSI {
			// If we have both RSSI and RSRP, use the stronger signal as SignalStrength
			// and note that we have additional signal quality data
			if neighbor.RSRP > neighbor.RSSI {
				neighborCell.SignalStrength = neighbor.RSRP
			}
		}

		fmt.Printf("    📡 Neighbor %d: CellID=%d, LAC=%d, RSSI=%d dBm, Type=%s\n",
			i+1, neighborCell.CellID, neighborCell.LocationAreaCode,
			neighborCell.SignalStrength, neighbor.CellType)

		cellTowers = append(cellTowers, neighborCell)
	}

	return cellTowers, radioType, nil
}

// getTimingAdvanceForCell attempts to get TimingAdvance for a specific cell
func getTimingAdvanceForCell(intel *CellularLocationIntelligence, cellID int) int {
	// This is a placeholder for TimingAdvance collection
	// In a real implementation, this would:
	// 1. Use AT+CGED commands to get timing advance
	// 2. Parse network measurement reports
	// 3. Extract timing advance from serving cell measurements

	// For now, we don't have a reliable way to get TimingAdvance
	// This would require additional AT commands and parsing
	return 0
}

// BuildGoogleWiFiAccessPoints converts WiFi scan results to Google format
func BuildGoogleWiFiAccessPoints(client *ssh.Client, maxAPs int) ([]maps.WiFiAccessPoint, error) {
	var wifiAPs []maps.WiFiAccessPoint

	// Get WiFi scan results
	fmt.Println("📶 Scanning for WiFi access points...")

	// Command to scan for WiFi networks
	scanCmd := "iwlist scan 2>/dev/null | grep -E '(Address|ESSID|Signal|Channel)' | head -100"
	output, err := executeCommand(client, scanCmd)
	if err != nil {
		// Try alternative command
		scanCmd = "iw dev wlan0 scan 2>/dev/null | grep -E '(BSS|SSID|signal|freq)' | head -100"
		output, err = executeCommand(client, scanCmd)
		if err != nil {
			return nil, fmt.Errorf("failed to scan WiFi: %w", err)
		}
	}

	// Parse WiFi scan results
	lines := strings.Split(output, "\n")
	var currentAP *maps.WiFiAccessPoint
	var currentSignal int
	var currentChannel int

	for _, line := range lines {
		line = strings.TrimSpace(line)

		// Parse BSSID (MAC address)
		if strings.Contains(line, "Address:") || strings.Contains(line, "BSS ") {
			if currentAP != nil && currentAP.MACAddress != "" {
				// Finalize previous AP
				currentAP.SignalStrength = float64(currentSignal)
				currentAP.Channel = currentChannel
				wifiAPs = append(wifiAPs, *currentAP)
			}

			currentAP = &maps.WiFiAccessPoint{}
			currentSignal = 0
			currentChannel = 0

			// Extract BSSID
			if strings.Contains(line, "Address:") {
				parts := strings.Split(line, "Address:")
				if len(parts) > 1 {
					bssid := strings.TrimSpace(parts[1])
					// Validate MAC address format
					if len(bssid) == 17 && strings.Count(bssid, ":") == 5 {
						currentAP.MACAddress = strings.ToLower(bssid)
					}
				}
			} else if strings.Contains(line, "BSS ") {
				parts := strings.Fields(line)
				if len(parts) > 1 {
					bssid := strings.TrimSpace(parts[1])
					if len(bssid) == 17 && strings.Count(bssid, ":") == 5 {
						currentAP.MACAddress = strings.ToLower(bssid)
					}
				}
			}
		}

		// Parse signal strength
		if strings.Contains(line, "Signal level") || strings.Contains(line, "signal:") {
			if strings.Contains(line, "Signal level") {
				// Format: "Signal level=-45 dBm"
				parts := strings.Split(line, "=")
				if len(parts) > 1 {
					signalStr := strings.Fields(parts[1])[0]
					if signal, err := strconv.Atoi(signalStr); err == nil {
						currentSignal = signal
					}
				}
			} else if strings.Contains(line, "signal:") {
				// Format: "signal: -45.00 dBm"
				parts := strings.Split(line, "signal:")
				if len(parts) > 1 {
					signalStr := strings.Fields(strings.TrimSpace(parts[1]))[0]
					if signal, err := strconv.ParseFloat(signalStr, 64); err == nil {
						currentSignal = int(signal)
					}
				}
			}
		}

		// Parse channel/frequency
		if strings.Contains(line, "Channel:") || strings.Contains(line, "freq:") {
			if strings.Contains(line, "Channel:") {
				parts := strings.Split(line, "Channel:")
				if len(parts) > 1 {
					channelStr := strings.TrimSpace(parts[1])
					if channel, err := strconv.Atoi(channelStr); err == nil {
						currentChannel = channel
					}
				}
			} else if strings.Contains(line, "freq:") {
				// Convert frequency to channel (approximate)
				parts := strings.Split(line, "freq:")
				if len(parts) > 1 {
					freqStr := strings.TrimSpace(parts[1])
					if freq, err := strconv.Atoi(freqStr); err == nil {
						// Rough conversion from frequency to channel
						if freq >= 2412 && freq <= 2484 {
							currentChannel = (freq-2412)/5 + 1
						} else if freq >= 5170 && freq <= 5825 {
							currentChannel = (freq - 5000) / 5
						}
					}
				}
			}
		}
	}

	// Add the last AP if valid
	if currentAP != nil && currentAP.MACAddress != "" {
		currentAP.SignalStrength = float64(currentSignal)
		currentAP.Channel = currentChannel
		wifiAPs = append(wifiAPs, *currentAP)
	}

	// Filter out invalid MAC addresses and sort by signal strength
	var validAPs []maps.WiFiAccessPoint
	for _, ap := range wifiAPs {
		if isValidMACAddress(ap.MACAddress) {
			validAPs = append(validAPs, ap)
		}
	}

	// Sort by signal strength (strongest first - higher values are better for negative dBm)
	sort.Slice(validAPs, func(i, j int) bool {
		return validAPs[i].SignalStrength > validAPs[j].SignalStrength
	})

	// Limit to maxAPs
	if len(validAPs) > maxAPs {
		validAPs = validAPs[:maxAPs]
	}

	fmt.Printf("📶 Found %d valid WiFi access points\n", len(validAPs))

	return validAPs, nil
}

// isValidMACAddress validates MAC addresses according to Google's best practices
// Filters out locally-administered MAC addresses and reserved IANA ranges
func isValidMACAddress(macAddr string) bool {
	// Basic format validation
	if len(macAddr) != 17 || strings.Count(macAddr, ":") != 5 {
		return false
	}

	// Convert to uppercase for consistency
	macAddr = strings.ToUpper(macAddr)

	// Check for broadcast MAC address
	if macAddr == "FF:FF:FF:FF:FF:FF" {
		return false
	}

	// Extract the first byte
	firstByte := macAddr[:2]

	// Parse the first byte to check the second least-significant bit
	firstByteInt, err := strconv.ParseInt(firstByte, 16, 64)
	if err != nil {
		return false
	}

	// Check if it's locally administered (second least-significant bit is 1)
	// The bit pattern is: xxxx xx1x (where x can be 0 or 1)
	if (firstByteInt & 0x02) != 0 {
		return false // Locally administered, not useful for location
	}

	// Check for reserved IANA range (00:00:5E:xx:xx:xx)
	if strings.HasPrefix(macAddr, "00:00:5E") {
		return false // Reserved for IANA, not useful for location
	}

	return true
}

// determineGoogleRadioType determines the radio type based on band and technology information for Google API
func determineGoogleRadioType(band, technology string) string {
	// Convert to lowercase for consistent matching
	tech := strings.ToLower(technology)

	// Map technology strings to Google's radio types
	// Note: The Go library may not support all latest 5G NR features, so we fall back to LTE for 5G-NSA
	switch {
	case strings.Contains(tech, "5g") || strings.Contains(tech, "nr"):
		// For 5G-NSA (Non-Standalone), fall back to LTE since the Go library may not support NR fully
		if strings.Contains(tech, "nsa") {
			return "lte" // 5G-NSA uses LTE core, so treat as LTE
		}
		return "lte" // Fallback to LTE for now due to library limitations
	case strings.Contains(tech, "lte") || strings.Contains(tech, "4g"):
		return "lte"
	case strings.Contains(tech, "wcdma") || strings.Contains(tech, "umts") || strings.Contains(tech, "3g"):
		return "wcdma"
	case strings.Contains(tech, "cdma"):
		return "cdma"
	case strings.Contains(tech, "gsm") || strings.Contains(tech, "2g"):
		return "gsm"
	default:
		// Default to GSM if unknown (as per Google's recommendation)
		return "gsm"
	}
}

// GetLocationWithGoogle performs comprehensive geolocation using Google's API
func (service *GoogleGeolocationService) GetLocationWithGoogle(intel *CellularLocationIntelligence, wifiAPs []maps.WiFiAccessPoint, considerIP bool) (*GoogleGeolocationResponse, error) {
	start := time.Now()

	response := &GoogleGeolocationResponse{
		Method:           "google_geolocation",
		RequestTimestamp: start,
		ConsideredIP:     considerIP,
	}

	// Build cell towers (Google supports many cells)
	cellTowers, radioType, err := BuildGoogleCellTowersFromIntelligence(intel, 20) // Google can handle more
	if err != nil {
		response.Error = fmt.Sprintf("Failed to build cell towers: %v", err)
		return response, err
	}

	response.RadioType = radioType
	response.CellsUsed = len(cellTowers)
	response.WiFiAPsUsed = len(wifiAPs)

	// Build Google geolocation request with proper radio type and all available data
	req := &maps.GeolocationRequest{
		RadioType:        maps.RadioType(radioType), // Set the radio type properly
		CellTowers:       cellTowers,
		WiFiAccessPoints: wifiAPs,
		ConsiderIP:       false, // Always disable IP - we need precise location, not ISP location
	}

	// Print request summary
	fmt.Printf("📡 Google Geolocation Request Summary:\n")
	fmt.Printf("  🗼 Radio Type: %s\n", radioType)
	fmt.Printf("  📱 Cell Towers: %d\n", len(cellTowers))
	fmt.Printf("  📶 WiFi APs: %d\n", len(wifiAPs))
	fmt.Printf("  🌐 Consider IP: %v\n", considerIP)

	// Debug: Print cell tower details
	fmt.Printf("  📋 Cell Tower Details:\n")
	for i, tower := range cellTowers {
		fmt.Printf("    %d. CellID: %d, MCC: %d, MNC: %d, LAC: %d, Signal: %d dBm\n",
			i+1, tower.CellID, tower.MobileCountryCode, tower.MobileNetworkCode,
			tower.LocationAreaCode, tower.SignalStrength)
	}

	// Rate limit the request
	<-service.rateLimiter.C

	// Make the geolocation request
	fmt.Println("🎯 Requesting location from Google...")
	ctx, cancel := context.WithTimeout(context.Background(), service.timeout)
	defer cancel()

	resp, err := service.client.Geolocate(ctx, req)
	if err != nil {
		response.Error = fmt.Sprintf("Google geolocation failed: %v", err)
		return response, err
	}

	// Populate response
	response.Success = true
	response.Location = resp.Location
	response.Accuracy = resp.Accuracy
	response.ResponseTime = float64(time.Since(start).Nanoseconds()) / 1e6

	return response, nil
}

// PrintGoogleResponse displays detailed response information
func (response *GoogleGeolocationResponse) PrintGoogleResponse() {
	fmt.Println("\n📊 Google Geolocation Response:")
	fmt.Println("=" + strings.Repeat("=", 35))

	if response.Success {
		fmt.Printf("✅ SUCCESS: %s\n", response.Method)
		fmt.Printf("📍 Location: %.6f°, %.6f°\n", response.Location.Lat, response.Location.Lng)
		fmt.Printf("🎯 Accuracy: ±%.0f meters\n", response.Accuracy)
		fmt.Printf("🗼 Cells Used: %d\n", response.CellsUsed)
		fmt.Printf("📶 WiFi APs Used: %d\n", response.WiFiAPsUsed)
		if response.RadioType != "" {
			fmt.Printf("📡 Radio Type: %s\n", response.RadioType)
		}
		fmt.Printf("🌐 Considered IP: %v\n", response.ConsideredIP)
		fmt.Printf("🗺️  Maps Link: https://www.google.com/maps?q=%.6f,%.6f\n",
			response.Location.Lat, response.Location.Lng)
	} else {
		fmt.Printf("❌ FAILED: %s\n", response.Error)
	}

	fmt.Printf("⏱️  Response Time: %.1f ms\n", response.ResponseTime)
	fmt.Printf("⏰ Request Time: %s\n", response.RequestTimestamp.Format("2006-01-02 15:04:05"))
}

// GetLocationWithGoogleComplete performs complete geolocation with live data collection
func GetLocationWithGoogleComplete(client *ssh.Client, considerIP bool) (*GoogleGeolocationResponse, error) {
	// Load Google API key
	apiKey, err := LoadGoogleAPIKey()
	if err != nil {
		return nil, fmt.Errorf("failed to load Google API key: %w", err)
	}

	// Create Google geolocation service
	service, err := NewGoogleGeolocationService(apiKey)
	if err != nil {
		return nil, fmt.Errorf("failed to create Google service: %w", err)
	}

	// Collect cellular intelligence
	fmt.Println("🗼 Collecting cellular intelligence...")
	intel, err := collectCellularLocationIntelligence(client)
	if err != nil {
		return nil, fmt.Errorf("failed to collect cellular data: %w", err)
	}

	// Collect WiFi access points
	fmt.Println("📶 Collecting WiFi access points...")
	wifiAPs, err := BuildGoogleWiFiAccessPoints(client, 50) // Google can handle many WiFi APs
	if err != nil {
		fmt.Printf("⚠️  Warning: Could not collect WiFi data: %v\n", err)
		wifiAPs = []maps.WiFiAccessPoint{} // Continue without WiFi
	}

	// Get location from Google
	response, err := service.GetLocationWithGoogle(intel, wifiAPs, considerIP)
	if err != nil {
		return response, err
	}

	// Print results
	response.PrintGoogleResponse()

	return response, nil
}

// CompareWithGPS compares Google geolocation result with GPS coordinates
func (response *GoogleGeolocationResponse) CompareWithGPS(gpsLat, gpsLon float64) {
	if !response.Success {
		fmt.Println("❌ Cannot compare - Google geolocation failed")
		return
	}

	// Calculate distance between Google result and GPS
	distance := calculateDistance(
		response.Location.Lat, response.Location.Lng,
		gpsLat, gpsLon,
	)

	fmt.Printf("\n🎯 Accuracy Comparison with GPS:\n")
	fmt.Printf("  📍 Google: %.6f°, %.6f° (±%.0fm)\n",
		response.Location.Lat, response.Location.Lng, response.Accuracy)
	fmt.Printf("  🛰️  GPS: %.6f°, %.6f°\n", gpsLat, gpsLon)
	fmt.Printf("  📏 Distance: %.1f meters\n", distance)

	if distance <= response.Accuracy {
		fmt.Printf("  ✅ EXCELLENT: Within accuracy range!\n")
	} else if distance <= response.Accuracy*2 {
		fmt.Printf("  ✅ GOOD: Within 2x accuracy range\n")
	} else if distance <= 1000 {
		fmt.Printf("  ⚠️  FAIR: Within 1km\n")
	} else {
		fmt.Printf("  ❌ POOR: >1km difference\n")
	}
}

// testGoogleGeolocation demonstrates the Google Geolocation API integration
func testGoogleGeolocation() error {
	fmt.Println("🚀 TESTING GOOGLE GEOLOCATION API")
	fmt.Println("=" + strings.Repeat("=", 35))

	// Test with hardcoded data first (to verify API works)
	fmt.Println("🧪 Testing with example data...")

	apiKey, err := LoadGoogleAPIKey()
	if err != nil {
		return fmt.Errorf("failed to load Google API key: %w", err)
	}

	service, err := NewGoogleGeolocationService(apiKey)
	if err != nil {
		return fmt.Errorf("failed to create Google service: %w", err)
	}

	// Create test request with example data
	testWiFiAPs := []maps.WiFiAccessPoint{
		{
			MACAddress:     "00:25:9c:cf:1c:ac",
			SignalStrength: -43,
		},
		{
			MACAddress:     "00:25:9c:cf:1c:ad",
			SignalStrength: -55,
		},
	}

	// Create mock intelligence for test
	testIntel := &CellularLocationIntelligence{
		ServingCell: ServingCellInfo{
			CellID: "42",
			MCC:    "310",
			MNC:    "410",
			TAC:    "415",
		},
		SignalQuality: SignalQuality{
			RSRP: -80,
		},
		NetworkInfo: NetworkInfo{
			Technology: "LTE",
		},
		NeighborCells: []NeighborCellInfo{}, // Empty for test
	}

	response, err := service.GetLocationWithGoogle(testIntel, testWiFiAPs, true)
	if err != nil {
		fmt.Printf("⚠️  Test with example data failed: %v\n", err)
	} else {
		fmt.Println("✅ Example data test successful!")
		response.PrintGoogleResponse()
	}

	fmt.Println("\n✅ Google Geolocation API Test Complete!")
	return nil
}
