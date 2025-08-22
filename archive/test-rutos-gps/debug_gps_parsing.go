package main

import (
	"fmt"
	"strconv"
	"strings"
)

// debugGPSParsing tests the GPS parsing with real data
func debugGPSParsing() {
	fmt.Println("🔍 GPS Parsing Debug Test")
	fmt.Println("========================")

	// Create SSH client
	client, err := createSSHClient()
	if err != nil {
		fmt.Printf("❌ Failed to create SSH client: %v\n", err)
		return
	}
	defer client.Close()

	// Get raw GPS data
	fmt.Println("📡 Getting raw GPS data...")
	output, err := executeCommand(client, "gsmctl -A 'AT+QGPSLOC=2'")
	if err != nil {
		fmt.Printf("❌ Failed to get GPS data: %v\n", err)
		return
	}

	fmt.Printf("📄 Raw output:\n%q\n", output)
	fmt.Printf("📄 Raw output (visible):\n%s\n", output)

	// Debug parsing step by step
	fmt.Println("\n🔍 Step-by-step parsing:")

	// Find the QGPSLOC line
	lines := strings.Split(output, "\n")
	fmt.Printf("📋 Found %d lines:\n", len(lines))
	for i, line := range lines {
		fmt.Printf("  Line %d: %q\n", i, line)
	}

	var qgpslocLine string
	for _, line := range lines {
		if strings.Contains(line, "+QGPSLOC:") {
			qgpslocLine = line
			break
		}
	}

	if qgpslocLine == "" {
		fmt.Println("❌ No QGPSLOC line found!")
		return
	}

	fmt.Printf("✅ QGPSLOC line: %q\n", qgpslocLine)

	// Remove the "+QGPSLOC: " prefix
	dataStr := strings.TrimPrefix(qgpslocLine, "+QGPSLOC: ")
	fmt.Printf("📊 Data string: %q\n", dataStr)

	parts := strings.Split(dataStr, ",")
	fmt.Printf("📋 Found %d parts:\n", len(parts))
	for i, part := range parts {
		fmt.Printf("  Part %d: %q\n", i, part)
	}

	if len(parts) < 11 {
		fmt.Printf("❌ Not enough parts! Expected 11, got %d\n", len(parts))
		return
	}

	// Parse satellites (part 10)
	fmt.Printf("\n🛰️  Parsing satellites from part 10: %q\n", parts[10])
	if sats, err := strconv.Atoi(parts[10]); err == nil {
		fmt.Printf("✅ Satellites parsed: %d\n", sats)
	} else {
		fmt.Printf("❌ Failed to parse satellites: %v\n", err)
	}

	// Test the existing parseQGPSLOC function
	fmt.Println("\n🧪 Testing existing parseQGPSLOC function:")
	gpsData := parseQGPSLOC(output)
	if gpsData != nil {
		fmt.Printf("✅ Parsed GPS data:\n")
		fmt.Printf("  Latitude: %.8f\n", gpsData.Latitude)
		fmt.Printf("  Longitude: %.8f\n", gpsData.Longitude)
		fmt.Printf("  HDOP: %.1f\n", gpsData.HDOP)
		fmt.Printf("  Satellites: %d\n", gpsData.Satellites)
		fmt.Printf("  Fix Type: %d\n", gpsData.FixType)
		fmt.Printf("  Time: %s\n", gpsData.Time)
		fmt.Printf("  Date: %s\n", gpsData.Date)
	} else {
		fmt.Println("❌ parseQGPSLOC returned nil")
	}
}

// testGPSParsingDebug runs the GPS parsing debug test
func testGPSParsingDebug() {
	debugGPSParsing()
}
