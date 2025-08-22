package main

import (
	"context"
	"encoding/json"
	"fmt"
	"strings"
	"time"

	"golang.org/x/crypto/ssh"
	"googlemaps.github.io/maps"
)

// UbusWiFiScanResult represents the complete ubus iwinfo scan response
type UbusWiFiScanResult struct {
	Results []UbusWiFiAccessPoint `json:"results"`
}

// UbusWiFiAccessPoint represents a single WiFi access point from ubus scan
type UbusWiFiAccessPoint struct {
	SSID         string            `json:"ssid,omitempty"`
	BSSID        string            `json:"bssid"`
	Mode         string            `json:"mode"`
	Channel      int               `json:"channel"`
	Signal       int               `json:"signal"`      // dBm
	Quality      int               `json:"quality"`     // SNR-like quality metric
	QualityMax   int               `json:"quality_max"` // Maximum quality value
	HTOperation  *HTOperationInfo  `json:"ht_operation,omitempty"`
	VHTOperation *VHTOperationInfo `json:"vht_operation,omitempty"`
	Encryption   EncryptionInfo    `json:"encryption"`
}

// HTOperationInfo represents HT (802.11n) operation parameters
type HTOperationInfo struct {
	PrimaryChannel         int    `json:"primary_channel"`
	SecondaryChannelOffset string `json:"secondary_channel_offset"`
	ChannelWidth           int    `json:"channel_width"`
}

// VHTOperationInfo represents VHT (802.11ac) operation parameters
type VHTOperationInfo struct {
	ChannelWidth int `json:"channel_width"`
	CenterFreq1  int `json:"center_freq_1"`
	CenterFreq2  int `json:"center_freq_2"`
}

// EncryptionInfo represents WiFi encryption details
type EncryptionInfo struct {
	Enabled        bool     `json:"enabled"`
	WPA            []int    `json:"wpa,omitempty"`
	Authentication []string `json:"authentication,omitempty"`
	Ciphers        []string `json:"ciphers,omitempty"`
}

// EnhancedWiFiScanResult represents the result of enhanced WiFi scanning
type EnhancedWiFiScanResult struct {
	Success        bool                   `json:"success"`
	AccessPoints   []UbusWiFiAccessPoint  `json:"access_points"`
	GoogleWiFiAPs  []maps.WiFiAccessPoint `json:"google_wifi_aps"`
	TotalFound     int                    `json:"total_found"`
	UniqueFound    int                    `json:"unique_found"`
	QualityRange   string                 `json:"quality_range"`
	ChannelSpread  []int                  `json:"channel_spread"`
	ScanInterfaces []string               `json:"scan_interfaces"`
	ScanDuration   time.Duration          `json:"scan_duration"`
	ErrorMessage   string                 `json:"error_message,omitempty"`
}

// testEnhancedUbusWiFiScan demonstrates the enhanced ubus-based WiFi scanning
func testEnhancedUbusWiFiScan() {
	fmt.Println("🚀 Enhanced ubus WiFi Scanning Test")
	fmt.Println("=====================================")

	// Create SSH client
	client, err := createSSHClient()
	if err != nil {
		fmt.Printf("❌ SSH connection failed: %v\n", err)
		return
	}
	defer client.Close()

	// Perform enhanced WiFi scan
	result, err := performEnhancedUbusWiFiScan(client)
	if err != nil {
		fmt.Printf("❌ Enhanced WiFi scan failed: %v\n", err)
		return
	}

	// Display comprehensive results
	displayEnhancedWiFiResults(result)

	// Test Google Geolocation with enhanced data
	if len(result.GoogleWiFiAPs) >= 2 {
		fmt.Println("\n🎯 Testing Google Geolocation with Enhanced WiFi Data...")
		testGoogleLocationWithEnhancedWiFi(result.GoogleWiFiAPs)
	} else {
		fmt.Printf("⚠️  Need minimum 2 WiFi APs for Google location (found %d)\n", len(result.GoogleWiFiAPs))
	}
}

// performEnhancedUbusWiFiScan performs comprehensive WiFi scanning using ubus
func performEnhancedUbusWiFiScan(client *ssh.Client) (*EnhancedWiFiScanResult, error) {
	startTime := time.Now()
	result := &EnhancedWiFiScanResult{
		Success:        false,
		AccessPoints:   []UbusWiFiAccessPoint{},
		GoogleWiFiAPs:  []maps.WiFiAccessPoint{},
		ScanInterfaces: []string{},
		ChannelSpread:  []int{},
	}

	// Get available WiFi interfaces from network.wireless status
	interfaces, err := getWiFiInterfacesFromUbus(client)
	if err != nil {
		return result, fmt.Errorf("failed to get WiFi interfaces: %w", err)
	}
	result.ScanInterfaces = interfaces

	fmt.Printf("📡 Discovered WiFi interfaces: %v\n", interfaces)

	var allAccessPoints []UbusWiFiAccessPoint
	seenBSSIDs := make(map[string]bool)

	// Scan each interface
	for _, iface := range interfaces {
		fmt.Printf("🔍 Scanning interface %s...\n", iface)

		aps, err := scanInterfaceWithUbus(client, iface)
		if err != nil {
			fmt.Printf("  ⚠️  Interface %s scan failed: %v\n", iface, err)
			continue
		}

		fmt.Printf("  ✅ Found %d access points on %s\n", len(aps), iface)

		// Add unique access points
		for _, ap := range aps {
			if !seenBSSIDs[ap.BSSID] {
				allAccessPoints = append(allAccessPoints, ap)
				seenBSSIDs[ap.BSSID] = true
			}
		}
	}

	result.AccessPoints = allAccessPoints
	result.TotalFound = len(allAccessPoints)
	result.UniqueFound = len(seenBSSIDs)
	result.ScanDuration = time.Since(startTime)

	// Convert to Google WiFi format
	result.GoogleWiFiAPs = convertUbusToGoogleWiFi(allAccessPoints)

	// Calculate quality statistics
	result.QualityRange = calculateQualityRange(allAccessPoints)
	result.ChannelSpread = calculateChannelSpread(allAccessPoints)

	result.Success = len(allAccessPoints) > 0
	return result, nil
}

// getWiFiInterfacesFromUbus gets WiFi interfaces from network.wireless ubus service
func getWiFiInterfacesFromUbus(client *ssh.Client) ([]string, error) {
	cmd := "ubus call network.wireless status"
	output, err := executeCommand(client, cmd)
	if err != nil {
		return nil, err
	}

	// Parse the network.wireless status to extract interface names
	var status map[string]interface{}
	if err := json.Unmarshal([]byte(output), &status); err != nil {
		return nil, fmt.Errorf("failed to parse wireless status: %w", err)
	}

	var interfaces []string
	for _, radioData := range status {
		if radioMap, ok := radioData.(map[string]interface{}); ok {
			if interfacesData, ok := radioMap["interfaces"].([]interface{}); ok {
				for _, ifaceData := range interfacesData {
					if ifaceMap, ok := ifaceData.(map[string]interface{}); ok {
						if ifname, ok := ifaceMap["ifname"].(string); ok {
							interfaces = append(interfaces, ifname)
						}
					}
				}
			}
		}
	}

	return interfaces, nil
}

// scanInterfaceWithUbus scans a specific WiFi interface using ubus iwinfo
func scanInterfaceWithUbus(client *ssh.Client, iface string) ([]UbusWiFiAccessPoint, error) {
	cmd := fmt.Sprintf("ubus call iwinfo scan '{\"device\":\"%s\"}'", iface)
	output, err := executeCommand(client, cmd)
	if err != nil {
		return nil, err
	}

	var scanResult UbusWiFiScanResult
	if err := json.Unmarshal([]byte(output), &scanResult); err != nil {
		return nil, fmt.Errorf("failed to parse scan result: %w", err)
	}

	return scanResult.Results, nil
}

// convertUbusToGoogleWiFi converts ubus WiFi data to Google Maps format
func convertUbusToGoogleWiFi(ubusAPs []UbusWiFiAccessPoint) []maps.WiFiAccessPoint {
	var googleAPs []maps.WiFiAccessPoint

	for _, ap := range ubusAPs {
		// Filter out invalid BSSIDs
		if !isValidMACAddress(ap.BSSID) {
			continue
		}

		googleAP := maps.WiFiAccessPoint{
			MACAddress:     ap.BSSID,
			SignalStrength: float64(ap.Signal),
			Age:            0, // Fresh scan
		}

		// Add channel if available
		if ap.Channel > 0 {
			googleAP.Channel = ap.Channel
		}

		// Add SNR if we can calculate it from quality
		if ap.Quality > 0 && ap.QualityMax > 0 {
			// Convert quality to approximate SNR
			snr := float64(ap.Quality) / float64(ap.QualityMax) * 40.0 // Scale to ~40dB max
			googleAP.SignalToNoiseRatio = snr
		}

		googleAPs = append(googleAPs, googleAP)
	}

	return googleAPs
}

// calculateQualityRange calculates the quality range of access points
func calculateQualityRange(aps []UbusWiFiAccessPoint) string {
	if len(aps) == 0 {
		return "N/A"
	}

	minQuality, maxQuality := aps[0].Quality, aps[0].Quality
	minSignal, maxSignal := aps[0].Signal, aps[0].Signal

	for _, ap := range aps {
		if ap.Quality < minQuality {
			minQuality = ap.Quality
		}
		if ap.Quality > maxQuality {
			maxQuality = ap.Quality
		}
		if ap.Signal < minSignal {
			minSignal = ap.Signal
		}
		if ap.Signal > maxSignal {
			maxSignal = ap.Signal
		}
	}

	return fmt.Sprintf("Quality: %d-%d/70, Signal: %d to %d dBm",
		minQuality, maxQuality, minSignal, maxSignal)
}

// calculateChannelSpread calculates the distribution of channels
func calculateChannelSpread(aps []UbusWiFiAccessPoint) []int {
	channelCount := make(map[int]int)

	for _, ap := range aps {
		if ap.Channel > 0 {
			channelCount[ap.Channel]++
		}
	}

	var channels []int
	for channel := range channelCount {
		channels = append(channels, channel)
	}

	return channels
}

// displayEnhancedWiFiResults displays comprehensive WiFi scan results
func displayEnhancedWiFiResults(result *EnhancedWiFiScanResult) {
	fmt.Printf("\n📊 Enhanced WiFi Scan Results:\n")
	fmt.Printf("==============================\n")
	fmt.Printf("✅ Success: %t\n", result.Success)
	fmt.Printf("📡 Interfaces Scanned: %v\n", result.ScanInterfaces)
	fmt.Printf("🔍 Total APs Found: %d\n", result.TotalFound)
	fmt.Printf("🎯 Unique APs: %d\n", result.UniqueFound)
	fmt.Printf("📊 %s\n", result.QualityRange)
	fmt.Printf("📻 Channels: %v\n", result.ChannelSpread)
	fmt.Printf("⏱️  Scan Duration: %v\n", result.ScanDuration)
	fmt.Printf("🎯 Google-Ready APs: %d\n", len(result.GoogleWiFiAPs))

	if len(result.AccessPoints) > 0 {
		fmt.Printf("\n📋 Top 10 Access Points (by signal strength):\n")
		fmt.Printf("%-18s %-20s %-3s %-6s %-7s %-10s %-15s\n",
			"BSSID", "SSID", "Ch", "Signal", "Quality", "Width", "Security")
		fmt.Printf("%s\n", strings.Repeat("-", 85))

		// Sort by signal strength (strongest first)
		sortedAPs := make([]UbusWiFiAccessPoint, len(result.AccessPoints))
		copy(sortedAPs, result.AccessPoints)

		// Simple bubble sort by signal (strongest = highest number, e.g., -50 > -80)
		for i := 0; i < len(sortedAPs)-1; i++ {
			for j := 0; j < len(sortedAPs)-i-1; j++ {
				if sortedAPs[j].Signal < sortedAPs[j+1].Signal {
					sortedAPs[j], sortedAPs[j+1] = sortedAPs[j+1], sortedAPs[j]
				}
			}
		}

		// Display top 10
		displayCount := len(sortedAPs)
		if displayCount > 10 {
			displayCount = 10
		}

		for i := 0; i < displayCount; i++ {
			ap := sortedAPs[i]
			ssid := ap.SSID
			if ssid == "" {
				ssid = "[Hidden]"
			}
			if len(ssid) > 20 {
				ssid = ssid[:17] + "..."
			}

			// Determine channel width
			width := "20MHz"
			if ap.HTOperation != nil {
				if ap.HTOperation.ChannelWidth == 2040 {
					width = "40MHz"
				}
			}
			if ap.VHTOperation != nil {
				width = fmt.Sprintf("%dMHz", ap.VHTOperation.ChannelWidth)
			}

			// Determine security
			security := "Open"
			if ap.Encryption.Enabled {
				if len(ap.Encryption.WPA) > 0 {
					wpaVersions := make([]string, len(ap.Encryption.WPA))
					for j, version := range ap.Encryption.WPA {
						wpaVersions[j] = fmt.Sprintf("WPA%d", version)
					}
					security = strings.Join(wpaVersions, "/")
				}
			}

			fmt.Printf("%-18s %-20s %-3d %-6d %-7s %-10s %-15s\n",
				ap.BSSID, ssid, ap.Channel, ap.Signal,
				fmt.Sprintf("%d/%d", ap.Quality, ap.QualityMax),
				width, security)
		}
	}
}

// testGoogleLocationWithEnhancedWiFi tests Google Geolocation with enhanced WiFi data
func testGoogleLocationWithEnhancedWiFi(wifiAPs []maps.WiFiAccessPoint) {
	// Load Google API key
	apiKey, err := LoadGoogleAPIKey()
	if err != nil {
		fmt.Printf("❌ Failed to load Google API key: %v\n", err)
		return
	}

	// Create Google Maps client
	client, err := maps.NewClient(maps.WithAPIKey(apiKey))
	if err != nil {
		fmt.Printf("❌ Failed to create Google client: %v\n", err)
		return
	}

	// Limit to top 15 APs (Google's limit)
	maxAPs := 15
	if len(wifiAPs) > maxAPs {
		wifiAPs = wifiAPs[:maxAPs]
	}

	// Create enhanced geolocation request
	req := &maps.GeolocationRequest{
		WiFiAccessPoints: wifiAPs,
		ConsiderIP:       false, // Disable IP-based location
	}

	fmt.Printf("📡 Enhanced Google Geolocation Request:\n")
	fmt.Printf("  📶 WiFi APs: %d\n", len(wifiAPs))
	fmt.Printf("  🌐 Consider IP: false\n")

	// Make the request
	ctx := context.Background()
	resp, err := client.Geolocate(ctx, req)
	if err != nil {
		fmt.Printf("❌ Google Geolocation failed: %v\n", err)
		return
	}

	fmt.Printf("\n🎯 Enhanced WiFi Location Response:\n")
	fmt.Printf("==================================\n")
	fmt.Printf("📍 Location: %.6f°, %.6f°\n", resp.Location.Lat, resp.Location.Lng)
	fmt.Printf("🎯 Accuracy: ±%.0f meters\n", resp.Accuracy)
	fmt.Printf("🗺️  Maps Link: https://www.google.com/maps?q=%.6f,%.6f\n",
		resp.Location.Lat, resp.Location.Lng)
}
