package main

import (
	"fmt"
	"strings"
)

// testQuectelParsing tests the parsing with your actual data
func testQuectelParsing() {
	fmt.Println("🧪 Testing Quectel GPS Parsing")
	fmt.Println("=" + strings.Repeat("=", 30))

	// Your actual response data
	testResponse := "+QGPSLOC: 001047.00,59.48007,18.27985,0.4,9.5,3,,0.0,0.0,160825,39"

	fmt.Printf("📊 Test Data: %s\n", testResponse)

	// Parse the response
	gpsData := parseQGPSLOC(testResponse)

	if gpsData == nil {
		fmt.Println("❌ Parsing failed!")
		return
	}

	fmt.Println("\n📍 Parsed Results:")
	fmt.Println("=" + strings.Repeat("=", 18))
	displayQuectelGPSData(gpsData)

	// Verify against expected values
	fmt.Println("\n✅ Verification:")
	fmt.Printf("  Expected Lat: 59.48007° | Parsed: %.5f° | ✅ %s\n",
		gpsData.Latitude, checkMatch(59.48007, gpsData.Latitude))
	fmt.Printf("  Expected Lon: 18.27985° | Parsed: %.5f° | ✅ %s\n",
		gpsData.Longitude, checkMatch(18.27985, gpsData.Longitude))
	fmt.Printf("  Expected Alt: 9.5m | Parsed: %.1fm | ✅ %s\n",
		gpsData.Altitude, checkMatch(9.5, gpsData.Altitude))
	fmt.Printf("  Expected Sats: 39 | Parsed: %d | ✅ %s\n",
		gpsData.Satellites, checkMatchInt(39, gpsData.Satellites))
	fmt.Printf("  Expected HDOP: 0.4 | Parsed: %.1f | ✅ %s\n",
		gpsData.HDOP, checkMatch(0.4, gpsData.HDOP))
	fmt.Printf("  Expected Fix: 3 (3D) | Parsed: %d (%s) | ✅ %s\n",
		gpsData.FixType, getFixTypeString(gpsData.FixType), checkMatchInt(3, gpsData.FixType))

	if gpsData.Valid {
		fmt.Println("\n🎉 SUCCESS: Quectel GPS parsing is working perfectly!")
	} else {
		fmt.Println("\n❌ ISSUE: GPS data marked as invalid")
	}
}

func checkMatch(expected, actual float64) string {
	if abs(expected-actual) < 0.00001 {
		return "MATCH"
	}
	return "MISMATCH"
}

func checkMatchInt(expected, actual int) string {
	if expected == actual {
		return "MATCH"
	}
	return "MISMATCH"
}

func abs(x float64) float64 {
	if x < 0 {
		return -x
	}
	return x
}

// Commented out main function - use main.go instead
// func main() {
// 	testQuectelParsing()
// }
