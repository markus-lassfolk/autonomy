package main

import (
	"bytes"
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"strings"
)

// debugCellTowerAPIs tests the APIs with debug information
func debugCellTowerAPIs() error {
	fmt.Println("🔍 DEBUG: Cell Tower API Testing")
	fmt.Println("=" + strings.Repeat("=", 35))

	// Test Mozilla with minimal data first
	fmt.Println("\n🦊 Testing Mozilla Location Service (Debug Mode)")
	fmt.Println("-" + strings.Repeat("-", 45))

	testMozillaDebug()

	// Test OpenCellID with debug
	fmt.Println("\n🗼 Testing OpenCellID (Debug Mode)")
	fmt.Println("-" + strings.Repeat("-", 35))

	testOpenCellIDDebug()

	// Try with alternative cell tower data
	fmt.Println("\n🔄 Testing with Alternative Cell Tower Data")
	fmt.Println("-" + strings.Repeat("-", 45))

	testWithAlternativeData()

	// Test corrected OpenCellID API
	fmt.Println("\n🔧 Testing CORRECTED OpenCellID API")
	fmt.Println("-" + strings.Repeat("-", 40))

	if err := testOpenCellIDCorrectAPI(); err != nil {
		fmt.Printf("❌ Corrected OpenCellID test failed: %v\n", err)
	}

	// Test alternative queries
	if err := testAlternativeOpenCellIDQueries(); err != nil {
		fmt.Printf("❌ Alternative queries failed: %v\n", err)
	}

	// Test area search
	if err := testOpenCellIDAreaSearch(); err != nil {
		fmt.Printf("❌ Area search failed: %v\n", err)
	}

	return nil
}

func testMozillaDebug() {
	fmt.Println("  📡 Testing Mozilla with minimal request...")

	// Minimal request - just one cell tower
	request := map[string]interface{}{
		"cellTowers": []map[string]interface{}{{
			"radioType":         "lte",
			"mobileCountryCode": 240,
			"mobileNetworkCode": 1,
			"locationAreaCode":  23,
			"cellId":            25939743,
			"signalStrength":    -53,
		}},
	}

	jsonData, _ := json.MarshalIndent(request, "", "  ")
	fmt.Printf("  📊 Request JSON:\n%s\n", string(jsonData))

	resp, err := http.Post(
		"https://location.services.mozilla.com/v1/geolocate?key=test",
		"application/json",
		bytes.NewBuffer(jsonData),
	)
	if err != nil {
		fmt.Printf("  ❌ HTTP Error: %v\n", err)
		return
	}
	defer resp.Body.Close()

	fmt.Printf("  📊 Response Status: %s\n", resp.Status)
	fmt.Printf("  📊 Response Headers:\n")
	for key, values := range resp.Header {
		fmt.Printf("    %s: %s\n", key, strings.Join(values, ", "))
	}

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		fmt.Printf("  ❌ Read Error: %v\n", err)
		return
	}

	fmt.Printf("  📊 Response Body (%d bytes): %s\n", len(body), string(body))

	if len(body) > 0 {
		var response map[string]interface{}
		if err := json.Unmarshal(body, &response); err != nil {
			fmt.Printf("  ❌ JSON Parse Error: %v\n", err)
		} else {
			fmt.Printf("  ✅ Parsed Response: %+v\n", response)
		}
	}
}

func testOpenCellIDDebug() {
	// Load API token
	apiKey, err := loadOpenCellIDTokenLocal()
	if err != nil {
		fmt.Printf("  ❌ Token Error: %v\n", err)
		return
	}

	fmt.Printf("  🔑 API Key loaded: %s...\n", apiKey[:min(len(apiKey), 10)])

	// Test request
	request := map[string]interface{}{
		"token": apiKey,
		"radio": "LTE",
		"mcc":   240,
		"mnc":   1,
		"cells": []map[string]interface{}{{
			"lac": 23,
			"cid": 25939743,
		}},
	}

	jsonData, _ := json.MarshalIndent(request, "", "  ")
	fmt.Printf("  📊 Request JSON:\n%s\n", string(jsonData))

	resp, err := http.Post(
		"https://us1.unwiredlabs.com/v2/process.php",
		"application/json",
		bytes.NewBuffer(jsonData),
	)
	if err != nil {
		fmt.Printf("  ❌ HTTP Error: %v\n", err)
		return
	}
	defer resp.Body.Close()

	fmt.Printf("  📊 Response Status: %s\n", resp.Status)

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		fmt.Printf("  ❌ Read Error: %v\n", err)
		return
	}

	fmt.Printf("  📊 Response Body: %s\n", string(body))

	var response map[string]interface{}
	if err := json.Unmarshal(body, &response); err != nil {
		fmt.Printf("  ❌ JSON Parse Error: %v\n", err)
	} else {
		fmt.Printf("  ✅ Parsed Response: %+v\n", response)
	}
}

func testWithAlternativeData() {
	fmt.Println("  🔄 Trying with different cell tower configurations...")

	// Try with different LAC values (common alternatives)
	alternatives := []struct {
		name string
		lac  int
		cid  int
	}{
		{"Original", 23, 25939743},
		{"LAC as hex", 0x17, 25939743}, // 23 in hex
		{"CID as hex", 23, 0x18BCF1F},  // Original hex value
		{"Common Stockholm LAC", 1, 25939743},
		{"Alternative LAC", 100, 25939743},
	}

	for _, alt := range alternatives {
		fmt.Printf("\n  🧪 Testing %s (LAC:%d, CID:%d):\n", alt.name, alt.lac, alt.cid)
		testMozillaWithData(alt.lac, alt.cid)
	}
}

func testMozillaWithData(lac, cid int) {
	request := map[string]interface{}{
		"cellTowers": []map[string]interface{}{{
			"radioType":         "lte",
			"mobileCountryCode": 240,
			"mobileNetworkCode": 1,
			"locationAreaCode":  lac,
			"cellId":            cid,
		}},
	}

	jsonData, _ := json.Marshal(request)
	resp, err := http.Post(
		"https://location.services.mozilla.com/v1/geolocate?key=test",
		"application/json",
		bytes.NewBuffer(jsonData),
	)
	if err != nil {
		fmt.Printf("    ❌ HTTP Error: %v\n", err)
		return
	}
	defer resp.Body.Close()

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		fmt.Printf("    ❌ Read Error: %v\n", err)
		return
	}

	if len(body) == 0 {
		fmt.Printf("    ❌ Empty response\n")
		return
	}

	var response map[string]interface{}
	if err := json.Unmarshal(body, &response); err != nil {
		fmt.Printf("    ❌ JSON Error: %v (Body: %s)\n", err, string(body))
		return
	}

	if location, ok := response["location"].(map[string]interface{}); ok {
		lat := location["lat"].(float64)
		lng := location["lng"].(float64)
		accuracy := response["accuracy"].(float64)
		fmt.Printf("    ✅ SUCCESS: %.6f°, %.6f° (±%.0fm)\n", lat, lng, accuracy)

		// Calculate distance from known GPS
		distance := calculateDistance(59.48007000, 18.27985000, lat, lng)
		fmt.Printf("    📏 Distance from GPS: %.0fm\n", distance)
	} else {
		fmt.Printf("    ❌ No location in response: %+v\n", response)
	}
}

func min(a, b int) int {
	if a < b {
		return a
	}
	return b
}
