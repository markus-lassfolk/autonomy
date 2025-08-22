package main

import (
	"context"
	"fmt"
	"log"
	"strings"
	"time"

	"github.com/markus-lassfolk/autonomy/pkg"
	"github.com/markus-lassfolk/autonomy/pkg/collector"
)

func main() {
	fmt.Println("🚀 Testing All Starlink API Methods...")

	// Create a Starlink collector with default settings
	config := map[string]interface{}{
		"starlink_api_host":   "192.168.100.1",
		"starlink_api_port":   9200,
		"starlink_timeout_s":  10,
		"starlink_grpc_first": true,
		"starlink_http_first": false,
	}

	collector, err := collector.NewStarlinkCollector(config)
	if err != nil {
		log.Fatalf("Failed to create Starlink collector: %v", err)
	}

	// Create a test member
	member := &pkg.Member{
		Name:  "starlink_test",
		Class: "starlink",
		Iface: "wan",
	}

	ctx, cancel := context.WithTimeout(context.Background(), 30*time.Second)
	defer cancel()

	// Test all API methods
	apiMethods := []string{
		"get_status",
		"get_history",
		"get_device_info",
		"get_location",
		"get_diagnostics",
	}

	fmt.Println("📡 Testing individual API methods...")

	for _, method := range apiMethods {
		fmt.Printf("\n🔍 Testing %s method:\n", method)
		fmt.Printf("=" + strings.Repeat("=", len(method)+15) + "\n")

		response, err := testStarlinkMethod(ctx, collector, method)
		if err != nil {
			fmt.Printf("❌ %s failed: %v\n", method, err)
			continue
		}

		fmt.Printf("✅ %s succeeded! Got %d bytes\n", method, len(response))

		// Print first 500 characters of response for inspection
		if len(response) > 500 {
			fmt.Printf("📄 Response preview: %s...\n", response[:500])
		} else {
			fmt.Printf("📄 Full response: %s\n", response)
		}
	}

	fmt.Println("\n🧪 Testing main Collect method (should use get_status):")
	fmt.Println("=================================================")

	apiResponse, err := collector.Collect(ctx, member)
	if err != nil {
		fmt.Printf("❌ Main Collect failed: %v\n", err)
	} else {
		fmt.Printf("✅ Main Collect succeeded!\n")
		fmt.Printf("  Timestamp: %v\n", apiResponse.Timestamp)
		if apiResponse.LatencyMS != nil {
			fmt.Printf("  Latency: %.2f ms\n", *apiResponse.LatencyMS)
		}
		if apiResponse.LossPercent != nil {
			fmt.Printf("  Loss: %.2f%%\n", *apiResponse.LossPercent)
		}
		if apiResponse.ObstructionPct != nil {
			fmt.Printf("  Obstruction: %.2f%%\n", *apiResponse.ObstructionPct)
		}
		if apiResponse.SNR != nil {
			fmt.Printf("  SNR: %d dB\n", *apiResponse.SNR)
		}
		if apiResponse.UptimeS != nil {
			fmt.Printf("  Uptime: %d seconds (%.1f hours)\n", *apiResponse.UptimeS, float64(*apiResponse.UptimeS)/3600.0)
		}
		if apiResponse.GPSValid != nil && *apiResponse.GPSValid {
			fmt.Printf("  GPS: Valid with %d satellites\n", *apiResponse.GPSSatellites)
		}
	}

	fmt.Println("\n🎉 All Starlink API tests completed!")
}

// testStarlinkMethod tests a specific Starlink API method
func testStarlinkMethod(ctx context.Context, sc *collector.StarlinkCollector, method string) (string, error) {
	// Use the public TestStarlinkMethod from the collector
	return sc.TestStarlinkMethod(ctx, method)
}
