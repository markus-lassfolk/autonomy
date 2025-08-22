package main

import (
	"context"
	"fmt"
	"os/exec"
	"regexp"
	"strconv"
	"strings"
	"time"

	"golang.org/x/crypto/ssh"
	"googlemaps.github.io/maps"
)

// WiFiAccessPointInfo represents a WiFi access point for location
type WiFiAccessPointInfo struct {
	BSSID          string `json:"bssid"`           // MAC address
	SSID           string `json:"ssid"`            // Network name
	SignalStrength int    `json:"signal_strength"` // Signal strength in dBm
	Frequency      int    `json:"frequency"`       // Frequency in MHz
	Channel        int    `json:"channel"`         // WiFi channel
	Security       string `json:"security"`        // Security type
	Valid          bool   `json:"valid"`           // Whether BSSID is valid for location
}

// BSSIDLocationResult represents the result of BSSID-only location
type BSSIDLocationResult struct {
	Success      bool                  `json:"success"`
	Location     *maps.LatLng          `json:"location"`
	Accuracy     float64               `json:"accuracy"`
	AccessPoints []WiFiAccessPointInfo `json:"access_points"`
	APsUsed      int                   `json:"aps_used"`
	ResponseTime time.Duration         `json:"response_time"`
	ErrorMessage string                `json:"error_message"`
	RequestTime  time.Time             `json:"request_time"`
}

// collectWiFiAccessPoints collects WiFi access points from RutOS using dynamic interface discovery
func collectWiFiAccessPoints(client *ssh.Client) ([]WiFiAccessPointInfo, error) {
	fmt.Println("📶 Collecting WiFi Access Points...")

	// First, discover available WiFi interfaces dynamically
	wifiInterfaces, err := discoverWiFiInterfaces(client)
	if err != nil {
		fmt.Printf("  ⚠️  Failed to discover WiFi interfaces: %v\n", err)
		fmt.Println("  🔄 Falling back to common interface names...")
		// Fallback to common interface names
		wifiInterfaces = []string{"wlan0-1", "wlan0", "wlan1", "wlan2"}
	}

	if len(wifiInterfaces) == 0 {
		fmt.Println("  ❌ No WiFi interfaces found")
		return nil, fmt.Errorf("no WiFi interfaces available")
	}

	fmt.Printf("  📡 Discovered WiFi interfaces: %v\n", wifiInterfaces)

	var accessPoints []WiFiAccessPointInfo

	// Try each discovered WiFi interface
	for _, iface := range wifiInterfaces {
		// Enhanced scan to get frequency/channel information
		cmd := fmt.Sprintf("iw dev %s scan | grep -E \"BSS|SSID|signal|freq\"", iface)
		fmt.Printf("  🔍 Scanning interface %s (enhanced)...\n", iface)

		output, err := executeCommand(client, cmd)
		if err != nil {
			fmt.Printf("    ❌ Failed: %v\n", err)
			continue
		}

		if strings.TrimSpace(output) == "" {
			fmt.Printf("    📭 No scan results\n")
			continue
		}

		// Parse the enhanced iw scan output
		aps := parseEnhancedIwScan(output)
		if len(aps) > 0 {
			fmt.Printf("    ✅ Found %d access points on %s\n", len(aps), iface)
			accessPoints = append(accessPoints, aps...)
			// Continue scanning other interfaces to get more APs
		} else {
			fmt.Printf("    📭 No access points parsed from %s\n", iface)
		}
	}

	// If no WiFi interfaces worked, try fallback methods
	if len(accessPoints) == 0 {
		fmt.Println("  🔍 Trying fallback WiFi scanning methods...")
		accessPoints = tryFallbackWiFiScan(client)
	}

	// Remove duplicates (same BSSID from different interfaces)
	accessPoints = removeDuplicateAccessPoints(accessPoints)

	// Filter and validate access points
	validAPs := filterValidAccessPoints(accessPoints)

	fmt.Printf("📊 WiFi Access Points Summary:\n")
	fmt.Printf("  📶 Total Detected: %d\n", len(accessPoints))
	fmt.Printf("  ✅ Valid for Location: %d\n", len(validAPs))

	if len(validAPs) > 0 {
		fmt.Println("  📋 Valid Access Points:")
		for i, ap := range validAPs {
			if i >= 5 { // Show first 5
				fmt.Printf("    ... and %d more\n", len(validAPs)-5)
				break
			}
			fmt.Printf("    - %s (%s) %d dBm\n", ap.BSSID, ap.SSID, ap.SignalStrength)
		}
	}

	return validAPs, nil
}

// discoverWiFiInterfaces dynamically discovers available WiFi interfaces on the system
func discoverWiFiInterfaces(client *ssh.Client) ([]string, error) {
	var interfaces []string

	// Method 1: Use 'iw dev' to list all wireless interfaces
	fmt.Println("  🔍 Discovering WiFi interfaces with 'iw dev'...")
	output, err := executeCommand(client, "iw dev")
	if err == nil && strings.TrimSpace(output) != "" {
		interfaces = parseIwDevOutput(output)
		if len(interfaces) > 0 {
			fmt.Printf("    ✅ Found interfaces via 'iw dev': %v\n", interfaces)
			return interfaces, nil
		}
	}

	// Method 2: Check /sys/class/net for wireless interfaces
	fmt.Println("  🔍 Checking /sys/class/net for wireless interfaces...")
	output, err = executeCommand(client, "ls /sys/class/net/*/wireless 2>/dev/null | cut -d'/' -f5")
	if err == nil && strings.TrimSpace(output) != "" {
		lines := strings.Split(strings.TrimSpace(output), "\n")
		for _, line := range lines {
			line = strings.TrimSpace(line)
			if line != "" {
				interfaces = append(interfaces, line)
			}
		}
		if len(interfaces) > 0 {
			fmt.Printf("    ✅ Found interfaces via /sys/class/net: %v\n", interfaces)
			return interfaces, nil
		}
	}

	// Method 3: Use 'iwconfig' if available
	fmt.Println("  🔍 Trying 'iwconfig' to find wireless interfaces...")
	output, err = executeCommand(client, "iwconfig 2>/dev/null | grep -o '^[a-zA-Z0-9-]*' | head -10")
	if err == nil && strings.TrimSpace(output) != "" {
		lines := strings.Split(strings.TrimSpace(output), "\n")
		for _, line := range lines {
			line = strings.TrimSpace(line)
			if line != "" && line != "lo" { // Skip loopback
				interfaces = append(interfaces, line)
			}
		}
		if len(interfaces) > 0 {
			fmt.Printf("    ✅ Found interfaces via 'iwconfig': %v\n", interfaces)
			return interfaces, nil
		}
	}

	// Method 4: Parse /proc/net/wireless
	fmt.Println("  🔍 Checking /proc/net/wireless...")
	output, err = executeCommand(client, "cat /proc/net/wireless 2>/dev/null | tail -n +3 | awk '{print $1}' | sed 's/://'")
	if err == nil && strings.TrimSpace(output) != "" {
		lines := strings.Split(strings.TrimSpace(output), "\n")
		for _, line := range lines {
			line = strings.TrimSpace(line)
			if line != "" {
				interfaces = append(interfaces, line)
			}
		}
		if len(interfaces) > 0 {
			fmt.Printf("    ✅ Found interfaces via /proc/net/wireless: %v\n", interfaces)
			return interfaces, nil
		}
	}

	fmt.Println("    ❌ No WiFi interfaces discovered through any method")
	return interfaces, fmt.Errorf("no WiFi interfaces found")
}

// parseIwDevOutput parses the output of 'iw dev' command to extract interface names
func parseIwDevOutput(output string) []string {
	var interfaces []string

	lines := strings.Split(output, "\n")
	for _, line := range lines {
		line = strings.TrimSpace(line)
		// Look for lines like "Interface wlan0-1"
		if strings.HasPrefix(line, "Interface ") {
			parts := strings.Fields(line)
			if len(parts) >= 2 {
				iface := parts[1]
				interfaces = append(interfaces, iface)
			}
		}
	}

	return interfaces
}

// removeDuplicateAccessPoints removes duplicate access points (same BSSID)
func removeDuplicateAccessPoints(aps []WiFiAccessPointInfo) []WiFiAccessPointInfo {
	seen := make(map[string]bool)
	var unique []WiFiAccessPointInfo

	for _, ap := range aps {
		if !seen[ap.BSSID] {
			seen[ap.BSSID] = true
			unique = append(unique, ap)
		}
	}

	return unique
}

// parseEnhancedIwScan parses the output from "iw dev wlan0-1 scan | grep -E "BSS|SSID|signal|freq""
func parseEnhancedIwScan(output string) []WiFiAccessPointInfo {
	var accessPoints []WiFiAccessPointInfo
	var currentAP *WiFiAccessPointInfo

	lines := strings.Split(output, "\n")

	for _, line := range lines {
		line = strings.TrimSpace(line)
		if line == "" {
			continue
		}

		// New BSS entry (access point)
		if strings.HasPrefix(line, "BSS ") {
			// Save previous AP if valid
			if currentAP != nil && currentAP.BSSID != "" {
				accessPoints = append(accessPoints, *currentAP)
			}

			// Start new AP
			currentAP = &WiFiAccessPointInfo{}

			// Extract BSSID from BSS line
			// Format: "BSS aa:bb:cc:dd:ee:ff(on wlan0-1)"
			re := regexp.MustCompile(`BSS\s+([0-9a-fA-F:]{17})`)
			if matches := re.FindStringSubmatch(line); len(matches) > 1 {
				currentAP.BSSID = strings.ToLower(matches[1])
			}
		}

		// SSID line
		if strings.HasPrefix(line, "SSID:") && currentAP != nil {
			ssid := strings.TrimSpace(strings.TrimPrefix(line, "SSID:"))
			if ssid != "" {
				currentAP.SSID = ssid
			}
		}

		// Signal strength line
		if strings.Contains(line, "signal:") && currentAP != nil {
			// Format: "signal: -45.00 dBm"
			re := regexp.MustCompile(`signal:\s*(-?\d+\.?\d*)\s*dBm`)
			if matches := re.FindStringSubmatch(line); len(matches) > 1 {
				if signal, err := strconv.ParseFloat(matches[1], 64); err == nil {
					currentAP.SignalStrength = int(signal)
				}
			}
		}

		// Frequency (if present in filtered output)
		if strings.Contains(line, "freq:") && currentAP != nil {
			re := regexp.MustCompile(`freq:\s*(\d+)`)
			if matches := re.FindStringSubmatch(line); len(matches) > 1 {
				if freq, err := strconv.Atoi(matches[1]); err == nil {
					currentAP.Frequency = freq
					// Convert frequency to channel for Google API
					currentAP.Channel = frequencyToChannel(freq)
				}
			}
		}
	}

	// Add last AP if valid
	if currentAP != nil && currentAP.BSSID != "" {
		accessPoints = append(accessPoints, *currentAP)
	}

	return accessPoints
}

// frequencyToChannel converts WiFi frequency (MHz) to channel number
func frequencyToChannel(freq int) int {
	// 2.4 GHz band (channels 1-14)
	if freq >= 2412 && freq <= 2484 {
		if freq == 2484 {
			return 14 // Special case for channel 14
		}
		return (freq-2412)/5 + 1
	}

	// 5 GHz band (channels 36-165)
	if freq >= 5170 && freq <= 5825 {
		return (freq - 5000) / 5
	}

	// 6 GHz band (channels 1-233) - WiFi 6E
	if freq >= 5955 && freq <= 7115 {
		return (freq - 5950) / 5
	}

	// Unknown frequency
	return 0
}

// tryFallbackWiFiScan tries fallback WiFi scanning methods
func tryFallbackWiFiScan(client *ssh.Client) []WiFiAccessPointInfo {
	var accessPoints []WiFiAccessPointInfo

	// Try full iw scan without grep filter
	fallbackCommands := []string{
		"iw dev wlan0-1 scan",
		"iw dev wlan0 scan",
		"iwlist scan",
	}

	for _, cmd := range fallbackCommands {
		fmt.Printf("    🔄 Fallback: %s\n", cmd)

		output, err := executeCommand(client, cmd)
		if err != nil {
			fmt.Printf("      ❌ Failed: %v\n", err)
			continue
		}

		if strings.TrimSpace(output) == "" {
			fmt.Printf("      📭 No output\n")
			continue
		}

		// Parse based on command type
		var aps []WiFiAccessPointInfo
		if strings.Contains(cmd, "iw dev") {
			aps = parseIwScan(output)
		} else if strings.Contains(cmd, "iwlist") {
			aps = parseIwlistScan(output)
		}

		if len(aps) > 0 {
			fmt.Printf("      ✅ Fallback found %d access points\n", len(aps))
			accessPoints = aps
			break
		}
	}

	return accessPoints
}

// parseIwlistScan parses iwlist scan output
func parseIwlistScan(output string) []WiFiAccessPointInfo {
	var accessPoints []WiFiAccessPointInfo

	// Split by Cell entries
	cells := strings.Split(output, "Cell ")
	for _, cell := range cells[1:] { // Skip first empty split
		ap := WiFiAccessPointInfo{}

		lines := strings.Split(cell, "\n")
		for _, line := range lines {
			line = strings.TrimSpace(line)

			// Extract BSSID
			if strings.Contains(line, "Address:") {
				parts := strings.Split(line, "Address:")
				if len(parts) > 1 {
					ap.BSSID = strings.TrimSpace(parts[1])
				}
			}

			// Extract SSID
			if strings.Contains(line, "ESSID:") {
				re := regexp.MustCompile(`ESSID:"([^"]*)"`)
				if matches := re.FindStringSubmatch(line); len(matches) > 1 {
					ap.SSID = matches[1]
				}
			}

			// Extract signal strength
			if strings.Contains(line, "Signal level=") {
				re := regexp.MustCompile(`Signal level=(-?\d+)`)
				if matches := re.FindStringSubmatch(line); len(matches) > 1 {
					if signal, err := strconv.Atoi(matches[1]); err == nil {
						ap.SignalStrength = signal
					}
				}
			}

			// Extract frequency
			if strings.Contains(line, "Frequency:") {
				re := regexp.MustCompile(`Frequency:(\d+\.?\d*)`)
				if matches := re.FindStringSubmatch(line); len(matches) > 1 {
					if freq, err := strconv.ParseFloat(matches[1], 64); err == nil {
						ap.Frequency = int(freq * 1000) // Convert GHz to MHz
					}
				}
			}
		}

		if ap.BSSID != "" {
			accessPoints = append(accessPoints, ap)
		}
	}

	return accessPoints
}

// parseIwScan parses iw scan output
func parseIwScan(output string) []WiFiAccessPointInfo {
	var accessPoints []WiFiAccessPointInfo

	// Split by BSS entries
	bssEntries := strings.Split(output, "BSS ")
	for _, entry := range bssEntries[1:] { // Skip first empty split
		ap := WiFiAccessPointInfo{}

		lines := strings.Split(entry, "\n")
		if len(lines) > 0 {
			// First line contains BSSID
			firstLine := strings.TrimSpace(lines[0])
			if bssidMatch := regexp.MustCompile(`([0-9a-fA-F:]{17})`).FindString(firstLine); bssidMatch != "" {
				ap.BSSID = bssidMatch
			}
		}

		for _, line := range lines {
			line = strings.TrimSpace(line)

			// Extract SSID
			if strings.HasPrefix(line, "SSID:") {
				ap.SSID = strings.TrimSpace(strings.TrimPrefix(line, "SSID:"))
			}

			// Extract signal strength
			if strings.Contains(line, "signal:") {
				re := regexp.MustCompile(`signal:\s*(-?\d+\.?\d*)\s*dBm`)
				if matches := re.FindStringSubmatch(line); len(matches) > 1 {
					if signal, err := strconv.ParseFloat(matches[1], 64); err == nil {
						ap.SignalStrength = int(signal)
					}
				}
			}

			// Extract frequency
			if strings.Contains(line, "freq:") {
				re := regexp.MustCompile(`freq:\s*(\d+)`)
				if matches := re.FindStringSubmatch(line); len(matches) > 1 {
					if freq, err := strconv.Atoi(matches[1]); err == nil {
						ap.Frequency = freq
					}
				}
			}
		}

		if ap.BSSID != "" {
			accessPoints = append(accessPoints, ap)
		}
	}

	return accessPoints
}

// parseUbusScan parses ubus WiFi scan output (JSON format)
func parseUbusScan(output string) []WiFiAccessPointInfo {
	// This would parse JSON output from ubus
	// For now, return empty slice - implement if ubus method works
	return []WiFiAccessPointInfo{}
}

// scanLocalWiFi attempts to scan WiFi locally (Windows/Linux)
func scanLocalWiFi() []WiFiAccessPointInfo {
	var accessPoints []WiFiAccessPointInfo

	// Try Windows netsh command
	if cmd := exec.Command("netsh", "wlan", "show", "profiles"); cmd.Run() == nil {
		// Windows system detected, try WiFi scan
		if output, err := exec.Command("netsh", "wlan", "show", "profiles").Output(); err == nil {
			fmt.Printf("    📱 Windows WiFi scan: %d bytes output\n", len(output))
			// Parse Windows WiFi output (simplified)
			return parseWindowsWiFi(string(output))
		}
	}

	// Try Linux iwlist (if available)
	if output, err := exec.Command("iwlist", "scan").Output(); err == nil {
		fmt.Printf("    🐧 Linux WiFi scan: %d bytes output\n", len(output))
		return parseIwlistScan(string(output))
	}

	return accessPoints
}

// parseWindowsWiFi parses Windows netsh WiFi output
func parseWindowsWiFi(output string) []WiFiAccessPointInfo {
	var accessPoints []WiFiAccessPointInfo

	// Try to get detailed WiFi information with netsh
	if detailedOutput, err := exec.Command("netsh", "wlan", "show", "profiles").Output(); err == nil {
		fmt.Printf("    📊 Detailed scan: %d bytes\n", len(detailedOutput))

		// Get available networks
		if networksOutput, err := exec.Command("netsh", "wlan", "show", "networks", "mode=bssid").Output(); err == nil {
			fmt.Printf("    🔍 Networks with BSSID: %d bytes\n", len(networksOutput))
			return parseNetshNetworks(string(networksOutput))
		}
	}

	return accessPoints
}

// parseNetshNetworks parses netsh wlan show networks mode=bssid output
func parseNetshNetworks(output string) []WiFiAccessPointInfo {
	var accessPoints []WiFiAccessPointInfo

	lines := strings.Split(output, "\n")
	var currentAP *WiFiAccessPointInfo

	for _, line := range lines {
		line = strings.TrimSpace(line)

		// New network entry
		if strings.HasPrefix(line, "SSID ") {
			if currentAP != nil && currentAP.BSSID != "" {
				accessPoints = append(accessPoints, *currentAP)
			}
			currentAP = &WiFiAccessPointInfo{}

			// Extract SSID
			parts := strings.Split(line, ":")
			if len(parts) > 1 {
				currentAP.SSID = strings.TrimSpace(parts[1])
			}
		}

		// Network type
		if strings.Contains(line, "Network type") && currentAP != nil {
			if strings.Contains(line, "Infrastructure") {
				// This is a regular WiFi network
			}
		}

		// Authentication
		if strings.Contains(line, "Authentication") && currentAP != nil {
			parts := strings.Split(line, ":")
			if len(parts) > 1 {
				currentAP.Security = strings.TrimSpace(parts[1])
			}
		}

		// BSSID
		if strings.Contains(line, "BSSID ") && currentAP != nil {
			parts := strings.Split(line, ":")
			if len(parts) >= 6 { // MAC address has 6 parts
				// Reconstruct MAC address
				macParts := make([]string, 6)
				for i := 1; i < 7 && i < len(parts); i++ {
					macParts[i-1] = strings.TrimSpace(parts[i])
				}
				currentAP.BSSID = strings.Join(macParts, ":")
			}
		}

		// Signal strength
		if strings.Contains(line, "Signal") && currentAP != nil {
			re := regexp.MustCompile(`(\d+)%`)
			if matches := re.FindStringSubmatch(line); len(matches) > 1 {
				if percent, err := strconv.Atoi(matches[1]); err == nil {
					// Convert percentage to approximate dBm
					// 100% ≈ -30dBm, 0% ≈ -100dBm
					currentAP.SignalStrength = -100 + (percent * 70 / 100)
				}
			}
		}

		// Channel
		if strings.Contains(line, "Channel") && currentAP != nil {
			re := regexp.MustCompile(`Channel\s*:\s*(\d+)`)
			if matches := re.FindStringSubmatch(line); len(matches) > 1 {
				if channel, err := strconv.Atoi(matches[1]); err == nil {
					currentAP.Channel = channel
				}
			}
		}
	}

	// Add last AP if valid
	if currentAP != nil && currentAP.BSSID != "" {
		accessPoints = append(accessPoints, *currentAP)
	}

	return accessPoints
}

// filterValidAccessPoints filters access points for location services
func filterValidAccessPoints(aps []WiFiAccessPointInfo) []WiFiAccessPointInfo {
	var validAPs []WiFiAccessPointInfo

	for _, ap := range aps {
		// Validate BSSID format
		if !isValidMACAddress(ap.BSSID) {
			continue
		}

		// Require minimum signal strength
		if ap.SignalStrength == 0 || ap.SignalStrength < -100 {
			continue
		}

		// Skip hidden networks for location (optional)
		if ap.SSID == "" || ap.SSID == "<hidden>" {
			// Still include for location if BSSID is valid
		}

		ap.Valid = true
		validAPs = append(validAPs, ap)
	}

	return validAPs
}

// testBSSIDOnlyLocation tests BSSID-only location with Google Geolocation API
func testBSSIDOnlyLocation() error {
	fmt.Println("📶 BSSID-ONLY LOCATION TEST")
	fmt.Println("=" + strings.Repeat("=", 27))

	// Connect to RutOS
	client, err := createSSHClient()
	if err != nil {
		return fmt.Errorf("failed to connect to RutOS: %w", err)
	}
	defer client.Close()

	// Collect WiFi access points
	accessPoints, err := collectWiFiAccessPoints(client)
	if err != nil {
		return fmt.Errorf("failed to collect WiFi access points: %w", err)
	}

	if len(accessPoints) < 2 {
		return fmt.Errorf("insufficient WiFi access points for location (need minimum 2, found %d)", len(accessPoints))
	}

	// Test BSSID-only location
	result, err := getBSSIDOnlyLocation(accessPoints)
	if err != nil {
		return fmt.Errorf("BSSID location failed: %w", err)
	}

	// Display results
	result.PrintBSSIDResult()

	// Compare with GPS if available
	fmt.Println("\n🎯 Comparing with GPS reference...")
	result.CompareWithGPS(59.48007, 18.27985) // Known GPS coordinates

	return nil
}

// getBSSIDOnlyLocation gets location using only WiFi BSSIDs
func getBSSIDOnlyLocation(accessPoints []WiFiAccessPointInfo) (*BSSIDLocationResult, error) {
	start := time.Now()

	result := &BSSIDLocationResult{
		AccessPoints: accessPoints,
		RequestTime:  start,
	}

	// Load Google API key
	apiKey, err := LoadGoogleAPIKey()
	if err != nil {
		result.ErrorMessage = fmt.Sprintf("Failed to load API key: %v", err)
		return result, err
	}

	// Create Google Maps client
	client, err := maps.NewClient(maps.WithAPIKey(apiKey))
	if err != nil {
		result.ErrorMessage = fmt.Sprintf("Failed to create Google client: %v", err)
		return result, err
	}

	// Build WiFi access points for Google API
	var wifiAPs []maps.WiFiAccessPoint
	maxAPs := 15 // Google limit
	if len(accessPoints) > maxAPs {
		accessPoints = accessPoints[:maxAPs]
	}

	for _, ap := range accessPoints {
		wifiAP := maps.WiFiAccessPoint{
			MACAddress:     ap.BSSID,
			SignalStrength: float64(ap.SignalStrength),
		}
		wifiAPs = append(wifiAPs, wifiAP)
	}

	result.APsUsed = len(wifiAPs)

	// Create geolocation request (BSSID only) with all available data
	req := &maps.GeolocationRequest{
		WiFiAccessPoints: wifiAPs,
		ConsiderIP:       false, // Don't use IP - we need precise location, not ISP location
	}

	fmt.Printf("📡 Google Geolocation Request (BSSID Only):\n")
	fmt.Printf("  📶 WiFi APs: %d\n", len(wifiAPs))
	fmt.Printf("  🌐 Consider IP: false\n")
	fmt.Printf("  📋 BSSID Details:\n")
	for i, ap := range wifiAPs {
		if i >= 5 { // Show first 5
			fmt.Printf("    ... and %d more\n", len(wifiAPs)-5)
			break
		}
		fmt.Printf("    %d. BSSID: %s, Signal: %.0f dBm\n", i+1, ap.MACAddress, ap.SignalStrength)
	}

	// Make the request
	fmt.Println("🎯 Requesting location from Google (BSSID only)...")
	resp, err := client.Geolocate(context.Background(), req)
	if err != nil {
		result.ErrorMessage = fmt.Sprintf("Google API error: %v", err)
		return result, err
	}

	result.ResponseTime = time.Since(start)
	result.Success = true
	result.Location = &resp.Location
	result.Accuracy = resp.Accuracy

	return result, nil
}

// PrintBSSIDResult prints the BSSID location result
func (result *BSSIDLocationResult) PrintBSSIDResult() {
	fmt.Printf("\n📊 BSSID Location Response:\n")
	fmt.Println("=" + strings.Repeat("=", 30))

	if result.Success {
		fmt.Printf("✅ SUCCESS: bssid_only_location\n")
		fmt.Printf("📍 Location: %.6f°, %.6f°\n", result.Location.Lat, result.Location.Lng)
		fmt.Printf("🎯 Accuracy: ±%.0f meters\n", result.Accuracy)
		fmt.Printf("📶 WiFi APs Used: %d\n", result.APsUsed)
		fmt.Printf("🌐 IP Considered: false\n")
		fmt.Printf("🗺️  Maps Link: https://www.google.com/maps?q=%.6f,%.6f\n",
			result.Location.Lat, result.Location.Lng)
		fmt.Printf("⏱️  Response Time: %.1f ms\n", float64(result.ResponseTime.Nanoseconds())/1e6)
		fmt.Printf("⏰ Request Time: %s\n", result.RequestTime.Format("2006-01-02 15:04:05"))
	} else {
		fmt.Printf("❌ FAILED: bssid_only_location\n")
		fmt.Printf("💥 Error: %s\n", result.ErrorMessage)
	}
}

// CompareWithGPS compares BSSID location result with GPS coordinates
func (result *BSSIDLocationResult) CompareWithGPS(gpsLat, gpsLon float64) {
	if !result.Success {
		fmt.Println("❌ Cannot compare - BSSID location failed")
		return
	}

	// Calculate distance between BSSID result and GPS
	distance := calculateDistance(
		result.Location.Lat, result.Location.Lng,
		gpsLat, gpsLon,
	)

	fmt.Printf("\n🎯 Accuracy Comparison with GPS:\n")
	fmt.Printf("  📍 BSSID: %.6f°, %.6f° (±%.0fm)\n",
		result.Location.Lat, result.Location.Lng, result.Accuracy)
	fmt.Printf("  🛰️  GPS: %.6f°, %.6f°\n", gpsLat, gpsLon)
	fmt.Printf("  📏 Distance: %.1f meters\n", distance)

	if distance <= result.Accuracy {
		fmt.Printf("  ✅ EXCELLENT: Within accuracy range!\n")
	} else if distance <= result.Accuracy*2 {
		fmt.Printf("  ✅ GOOD: Within 2x accuracy range\n")
	} else {
		fmt.Printf("  ⚠️  FAIR: Outside accuracy range but reasonable\n")
	}
}
