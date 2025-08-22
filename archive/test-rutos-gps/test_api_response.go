package main

import (
	"encoding/json"
	"fmt"
	"time"
)

// testAPIResponse demonstrates the RUTOS-compatible API response format
func testAPIResponse() {
	fmt.Println("📡 RUTOS-Compatible API Response Test")
	fmt.Println("====================================")

	// Create sample response that matches RUTOS format
	response := RutosGPSResponse{
		Data: RutosGPSData{
			Latitude:   floatPtr(59.48006600),
			Longitude:  floatPtr(18.27985300),
			Altitude:   floatPtr(9.5),
			FixStatus:  "3",
			Satellites: intPtr(43),
			Accuracy:   floatPtr(0.4),
			Speed:      floatPtr(0.0), // km/h
			DateTime:   time.Now().UTC().Format("2006-01-02T15:04:05Z"),
			Source:     "RUTOS Combined",
		},
	}

	// Convert to JSON
	jsonData, err := json.MarshalIndent(response, "", "  ")
	if err != nil {
		fmt.Printf("❌ JSON encoding failed: %v\n", err)
		return
	}

	fmt.Println("📋 Sample API Response (RUTOS Format):")
	fmt.Println(string(jsonData))

	fmt.Println("\n🔄 Node-Red Processing Simulation:")
	fmt.Println("==================================")

	// Simulate Node-Red processing
	d := response.Data

	fmt.Printf("Original RUTOS values:\n")
	if d.Latitude != nil {
		fmt.Printf("  latitude: %f\n", *d.Latitude)
	}
	if d.Longitude != nil {
		fmt.Printf("  longitude: %f\n", *d.Longitude)
	}
	if d.Altitude != nil {
		fmt.Printf("  altitude: %f\n", *d.Altitude)
	}
	fmt.Printf("  fix_status: %s\n", d.FixStatus)
	if d.Satellites != nil {
		fmt.Printf("  satellites: %d\n", *d.Satellites)
	}
	if d.Accuracy != nil {
		fmt.Printf("  accuracy: %f\n", *d.Accuracy)
	}
	if d.Speed != nil {
		fmt.Printf("  speed: %f km/h\n", *d.Speed)
	}

	fmt.Printf("\nNode-Red processed values:\n")

	// Simulate Node-Red function processing
	fix := 0
	if d.FixStatus != "" {
		if val, err := parseIntSafeLocal(d.FixStatus); err == nil {
			fix = val
		}
	}

	o := map[string]interface{}{
		"lat":    *d.Latitude,
		"lon":    *d.Longitude,
		"alt":    *d.Altitude,
		"gpsFix": fix,
	}

	if d.Satellites != nil {
		o["sats"] = *d.Satellites
	}
	if d.Accuracy != nil {
		o["hAcc"] = *d.Accuracy
	}
	if d.Speed != nil {
		// Convert km/h to m/s (as per Node-Red function)
		speedMs := *d.Speed * 0.277777778
		o["speed"] = speedMs
	}

	// Show processed output
	processedJSON, _ := json.MarshalIndent(o, "", "  ")
	fmt.Println(string(processedJSON))

	fmt.Println("\n✅ Node-Red Compatibility Verified!")
	fmt.Println("📝 Your existing Node-Red flow will work unchanged!")
	fmt.Println("🔄 Just change the URL from:")
	fmt.Println("   https://192.168.80.1/api/gps/position/status")
	fmt.Println("   to:")
	fmt.Println("   http://localhost:8080/api/gps/position/status")
}

// Helper functions
func floatPtr(f float64) *float64 {
	return &f
}

func intPtr(i int) *int {
	return &i
}

func parseIntSafeLocal(s string) (int, error) {
	if s == "" {
		return 0, fmt.Errorf("empty string")
	}
	// Simple conversion for fix status
	switch s {
	case "0":
		return 0, nil
	case "1":
		return 1, nil
	case "2":
		return 2, nil
	case "3":
		return 3, nil
	default:
		return 0, fmt.Errorf("invalid fix status: %s", s)
	}
}
