package main

import (
	"fmt"
	"strings"
)

// testBSSIDHardcodedLocation tests BSSID location with hardcoded WiFi access points
func testBSSIDHardcodedLocation() error {
	fmt.Println("📶 BSSID HARDCODED TEST")
	fmt.Println("=" + strings.Repeat("=", 23))

	// Create hardcoded WiFi access points for testing
	// These are example BSSIDs - in real use, these would be actual nearby WiFi networks
	accessPoints := []WiFiAccessPointInfo{
		{
			BSSID:          "00:1A:2B:3C:4D:5E",
			SSID:           "TestNetwork1",
			SignalStrength: -45,
			Frequency:      2437, // Channel 6
			Channel:        6,
			Security:       "WPA2",
			Valid:          true,
		},
		{
			BSSID:          "AA:BB:CC:DD:EE:FF",
			SSID:           "TestNetwork2",
			SignalStrength: -60,
			Frequency:      2462, // Channel 11
			Channel:        11,
			Security:       "WPA3",
			Valid:          true,
		},
		{
			BSSID:          "11:22:33:44:55:66",
			SSID:           "TestNetwork3",
			SignalStrength: -70,
			Frequency:      5180, // 5GHz
			Channel:        36,
			Security:       "WPA2",
			Valid:          true,
		},
	}

	fmt.Printf("📊 Using %d hardcoded WiFi access points:\n", len(accessPoints))
	for i, ap := range accessPoints {
		fmt.Printf("  %d. %s (%s) %d dBm, Ch %d\n",
			i+1, ap.BSSID, ap.SSID, ap.SignalStrength, ap.Channel)
	}

	// Test BSSID-only location
	fmt.Println("\n🎯 Testing Google Geolocation with hardcoded BSSIDs...")
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
