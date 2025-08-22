package main

import (
	"encoding/json"
	"fmt"
)

// testAPIEndpoints demonstrates all API endpoints without starting the server
func testAPIEndpoints() {
	fmt.Println("🌐 autonomy GPS API Endpoints Test")
	fmt.Println("==================================")

	// Create SSH connection
	sshClient, err := createSSHClient()
	if err != nil {
		fmt.Printf("❌ SSH connection failed: %v\n", err)
		return
	}
	defer sshClient.Close()

	// Create API server instance
	server := NewautonomyAPIServer(sshClient)

	fmt.Println("📡 Testing Individual GPS Sources:")
	fmt.Println("==================================")

	// Test RUTOS data collection
	fmt.Println("\n🛰️ RUTOS GPS Data:")
	rutosData := server.collectRutosData()
	printGPSData(rutosData)

	// Test Starlink data collection
	fmt.Println("\n🛰️ Starlink GPS Data:")
	starlinkData := server.collectStarlinkData()
	printGPSData(starlinkData)

	// Test Google data collection
	fmt.Println("\n🌐 Google GPS Data:")
	googleData := server.collectGoogleData()
	printGPSData(googleData)

	// Test best source selection
	fmt.Println("\n🏆 Best GPS Source Selection:")
	fmt.Println("=============================")
	bestData := server.selectBestGPSSource(rutosData, starlinkData, googleData)
	printGPSData(bestData)

	// Show API endpoints
	fmt.Println("\n🔧 Available API Endpoints:")
	fmt.Println("===========================")
	fmt.Println("📍 Main endpoint (drop-in replacement):")
	fmt.Println("   GET /api/gps/position/status")
	fmt.Println("   → Returns best GPS source in RUTOS format")
	fmt.Println("")
	fmt.Println("🔧 Individual source endpoints:")
	fmt.Println("   GET /api/gps/rutos     → RUTOS GPS only")
	fmt.Println("   GET /api/gps/starlink  → Starlink GPS only")
	fmt.Println("   GET /api/gps/google    → Google GPS only")
	fmt.Println("")
	fmt.Println("📊 Utility endpoints:")
	fmt.Println("   GET /api/gps/all       → All sources combined")
	fmt.Println("   GET /api/health        → Server health status")

	// Show Node-Red configuration
	fmt.Println("\n📝 Node-Red Configuration:")
	fmt.Println("==========================")
	fmt.Println("🔄 Change your Node-Red HTTP request node:")
	fmt.Println("   FROM: https://192.168.80.1/api/gps/position/status")
	fmt.Println("   TO:   http://localhost:8080/api/gps/position/status")
	fmt.Println("")
	fmt.Println("✅ Your existing Node-Red function will work unchanged!")
	fmt.Println("🚀 The autonomy daemon will automatically select the best GPS source")

	// Show sample responses
	fmt.Println("\n📋 Sample API Responses:")
	fmt.Println("========================")

	// Best GPS response
	bestResponse := RutosGPSResponse{Data: bestData}
	bestJSON, _ := json.MarshalIndent(bestResponse, "", "  ")
	fmt.Printf("\n🏆 GET /api/gps/position/status:\n%s\n", bestJSON)

	// All sources response
	allSources := map[string]RutosGPSData{
		"rutos":    rutosData,
		"starlink": starlinkData,
		"google":   googleData,
	}
	allJSON, _ := json.MarshalIndent(allSources, "", "  ")
	fmt.Printf("\n📊 GET /api/gps/all:\n%s\n", allJSON)

	fmt.Println("\n🎯 API Endpoints Test Complete!")
	fmt.Println("💡 To start the actual server, run: go run . -start-api-server")
}

// printGPSData prints GPS data in a readable format
func printGPSData(data RutosGPSData) {
	fmt.Printf("  Source: %s\n", data.Source)
	if data.Latitude != nil && data.Longitude != nil {
		fmt.Printf("  Position: %.6f, %.6f\n", *data.Latitude, *data.Longitude)
	}
	if data.Altitude != nil {
		fmt.Printf("  Altitude: %.1f m\n", *data.Altitude)
	}
	fmt.Printf("  Fix Status: %s\n", data.FixStatus)
	if data.Satellites != nil {
		fmt.Printf("  Satellites: %d\n", *data.Satellites)
	}
	if data.Accuracy != nil {
		fmt.Printf("  Accuracy: %.1f m\n", *data.Accuracy)
	}
	if data.Speed != nil {
		fmt.Printf("  Speed: %.2f km/h\n", *data.Speed)
	}
	fmt.Printf("  DateTime: %s\n", data.DateTime)
}
