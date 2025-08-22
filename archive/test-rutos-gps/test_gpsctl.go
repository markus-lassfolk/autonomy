package main

import (
	"fmt"
	"strconv"
	"strings"

	"golang.org/x/crypto/ssh"
)

// testGPSCTL tests the gpsctl command approach
func testGPSCTL(client *ssh.Client) {
	fmt.Println("🎯 Testing gpsctl Command Approach")
	fmt.Println("=" + strings.Repeat("=", 35))

	// First, check if gpsctl exists
	fmt.Println("📋 1. Checking gpsctl availability:")
	output, err := executeCommand(client, "which gpsctl")
	if err != nil {
		fmt.Printf("   ❌ gpsctl not found: %v\n", err)
		fmt.Println("   💡 Checking alternative commands...")
		checkAlternativeGPSCommands(client)
		return
	}
	fmt.Printf("   ✅ gpsctl found at: %s\n", strings.TrimSpace(output))

	// Check gpsctl help/usage
	fmt.Println("\n📋 2. gpsctl command options:")
	output, err = executeCommand(client, "gpsctl -h 2>&1 || gpsctl --help 2>&1 || gpsctl 2>&1")
	if err == nil {
		fmt.Printf("   %s\n", output)
	}

	// Test the specific commands from the script
	fmt.Println("\n📍 3. Testing GPS coordinate retrieval:")

	// Test latitude command
	fmt.Println("   Testing latitude (gpsctl -i):")
	latOutput, latErr := executeCommand(client, "gpsctl -i")
	if latErr != nil {
		fmt.Printf("   ❌ Latitude command failed: %v\n", latErr)
	} else {
		fmt.Printf("   📊 Raw output: '%s'\n", strings.TrimSpace(latOutput))
		if lat := parseCoordinate(latOutput); lat != 0 {
			fmt.Printf("   ✅ Parsed latitude: %.8f°\n", lat)
		} else {
			fmt.Printf("   ⚠️  Could not parse latitude from output\n")
		}
	}

	// Test longitude command
	fmt.Println("\n   Testing longitude (gpsctl -x):")
	lonOutput, lonErr := executeCommand(client, "gpsctl -x")
	if lonErr != nil {
		fmt.Printf("   ❌ Longitude command failed: %v\n", lonErr)
	} else {
		fmt.Printf("   📊 Raw output: '%s'\n", strings.TrimSpace(lonOutput))
		if lon := parseCoordinate(lonOutput); lon != 0 {
			fmt.Printf("   ✅ Parsed longitude: %.8f°\n", lon)
		} else {
			fmt.Printf("   ⚠️  Could not parse longitude from output\n")
		}
	}

	// Test the complete script logic
	fmt.Println("\n🔄 4. Testing complete script logic:")
	testCompleteGPSScript(client)

	// Test other gpsctl options
	fmt.Println("\n🔍 5. Exploring other gpsctl options:")
	testOtherGPSCTLOptions(client)
}

func checkAlternativeGPSCommands(client *ssh.Client) {
	alternatives := []string{
		"gpsd_client",
		"cgps",
		"gpsmon",
		"gpspipe",
		"gpsctl",
		"gps",
		"loc",
		"location",
	}

	fmt.Println("   🔍 Searching for alternative GPS commands:")
	for _, cmd := range alternatives {
		output, err := executeCommand(client, fmt.Sprintf("which %s 2>/dev/null", cmd))
		if err == nil && strings.TrimSpace(output) != "" {
			fmt.Printf("   ✅ Found: %s at %s\n", cmd, strings.TrimSpace(output))
		}
	}

	// Check for GPS-related binaries
	fmt.Println("\n   🔍 Searching for GPS-related binaries:")
	output, err := executeCommand(client, "find /usr/bin /usr/sbin /bin /sbin -name '*gps*' 2>/dev/null")
	if err == nil {
		lines := strings.Split(strings.TrimSpace(output), "\n")
		for _, line := range lines {
			if strings.TrimSpace(line) != "" {
				fmt.Printf("   📁 %s\n", line)
			}
		}
	}
}

func testCompleteGPSScript(client *ssh.Client) {
	// Implement the exact script logic
	script := `#!/bin/sh
# Get latitude
LATITUDE=$(gpsctl -i)
# Get longitude  
LONGITUDE=$(gpsctl -x)
# Check if both latitude and longitude are available
if [ -n "$LATITUDE" ] && [ -n "$LONGITUDE" ]; then
    GPS_COORDINATES="$LATITUDE,$LONGITUDE"
    MAPS_LINK="https://www.google.com/maps?q=$GPS_COORDINATES"
    echo "SUCCESS: GPS Coordinates: $GPS_COORDINATES"
    echo "Maps Link: $MAPS_LINK"
else
    echo "ERROR: GPS coordinates not available."
    echo "LATITUDE='$LATITUDE'"
    echo "LONGITUDE='$LONGITUDE'"
fi`

	fmt.Println("   📝 Running complete GPS script:")
	output, err := executeCommand(client, script)
	if err != nil {
		fmt.Printf("   ❌ Script execution failed: %v\n", err)
	} else {
		lines := strings.Split(output, "\n")
		for _, line := range lines {
			if strings.TrimSpace(line) != "" {
				if strings.Contains(line, "SUCCESS") {
					fmt.Printf("   ✅ %s\n", line)
				} else if strings.Contains(line, "ERROR") {
					fmt.Printf("   ❌ %s\n", line)
				} else {
					fmt.Printf("   📊 %s\n", line)
				}
			}
		}
	}
}

func testOtherGPSCTLOptions(client *ssh.Client) {
	options := []string{
		"-h", "--help",
		"-v", "--version",
		"-s", "-a", "-t", "-d",
		"-l", "-p", "-c", "-n",
	}

	for _, opt := range options {
		fmt.Printf("   Testing gpsctl %s: ", opt)
		output, err := executeCommand(client, fmt.Sprintf("gpsctl %s 2>&1", opt))
		if err != nil {
			fmt.Printf("❌ Failed\n")
		} else {
			output = strings.TrimSpace(output)
			if output == "" {
				fmt.Printf("⚪ No output\n")
			} else if len(output) > 100 {
				fmt.Printf("✅ %s...\n", output[:100])
			} else {
				fmt.Printf("✅ %s\n", output)
			}
		}
	}
}

func parseCoordinate(output string) float64 {
	// Try to parse coordinate from various formats
	output = strings.TrimSpace(output)

	// Try direct float parsing
	if coord, err := strconv.ParseFloat(output, 64); err == nil {
		return coord
	}

	// Try parsing from lines (in case there's extra text)
	lines := strings.Split(output, "\n")
	for _, line := range lines {
		line = strings.TrimSpace(line)
		if coord, err := strconv.ParseFloat(line, 64); err == nil {
			return coord
		}
	}

	return 0
}
