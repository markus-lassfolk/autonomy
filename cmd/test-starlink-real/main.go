package main

import (
	"context"
	"encoding/json"
	"fmt"
	"time"

	"github.com/markus-lassfolk/autonomy/pkg"
	"github.com/markus-lassfolk/autonomy/pkg/collector"
)

func main() {
	fmt.Println("🛰️  Real Starlink API Testing...")
	fmt.Println("=" + fmt.Sprintf("%60s", "="))

	// Test 1: Create Starlink collector
	fmt.Println("\n🔧 Test 1: Creating Starlink collector...")
	config := map[string]interface{}{
		"api_host":            "192.168.100.1",
		"api_port":            9200,
		"timeout":             10 * time.Second,
		"starlink_grpc_first": true,
		"starlink_http_first": false,
	}

	collector, err := collector.NewStarlinkCollector(config)
	if err != nil {
		fmt.Printf("❌ Failed to create Starlink collector: %v\n", err)
		return
	}
	fmt.Println("✅ Starlink collector created successfully")

	// Test 2: Test with mock member
	fmt.Println("\n📡 Test 2: Testing metrics collection...")
	member := &pkg.Member{
		Name:  "starlink_test",
		Class: pkg.ClassStarlink,
		Iface: "eth0",
	}

	ctx, cancel := context.WithTimeout(context.Background(), 15*time.Second)
	defer cancel()

	metrics, err := collector.Collect(ctx, member)
	if err != nil {
		fmt.Printf("❌ Failed to collect metrics: %v\n", err)
		return
	}

	fmt.Println("✅ Metrics collected successfully!")

	// Pretty print the metrics
	metricsJSON, err := json.MarshalIndent(metrics, "", "  ")
	if err != nil {
		fmt.Printf("❌ Failed to marshal metrics: %v\n", err)
		return
	}

	fmt.Printf("📊 Collected Metrics:\n%s\n", string(metricsJSON))

	// Test 3: Test specific Starlink fields
	fmt.Println("\n🔍 Test 3: Verifying Starlink-specific data...")

	if metrics.ObstructionPct != nil {
		fmt.Printf("   🛡️  Obstruction: %.2f%%\n", *metrics.ObstructionPct)
	} else {
		fmt.Println("   ⚠️  Obstruction data not available")
	}

	if metrics.LatencyMS != nil {
		fmt.Printf("   📶 Latency: %.1f ms\n", *metrics.LatencyMS)
	}
	if metrics.LossPercent != nil {
		fmt.Printf("   📉 Loss: %.2f%%\n", *metrics.LossPercent)
	}
	if metrics.JitterMS != nil {
		fmt.Printf("   📊 Jitter: %.1f ms\n", *metrics.JitterMS)
	}

	// Additional Starlink-specific fields
	if metrics.GPSValid != nil && *metrics.GPSValid {
		fmt.Printf("   🛰️  GPS: %.6f, %.6f\n", *metrics.GPSLatitude, *metrics.GPSLongitude)
	}

	// Test 4: Test multiple collections
	fmt.Println("\n🔄 Test 4: Testing multiple collections...")
	for i := 0; i < 3; i++ {
		fmt.Printf("   Collection %d...", i+1)

		ctx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
		metrics, err := collector.Collect(ctx, member)
		cancel()

		if err != nil {
			fmt.Printf(" ❌ Error: %v\n", err)
		} else {
			latencyStr := "N/A"
			lossStr := "N/A"
			if metrics.LatencyMS != nil {
				latencyStr = fmt.Sprintf("%.1fms", *metrics.LatencyMS)
			}
			if metrics.LossPercent != nil {
				lossStr = fmt.Sprintf("%.2f%%", *metrics.LossPercent)
			}
			fmt.Printf(" ✅ Latency: %s, Loss: %s\n", latencyStr, lossStr)
		}

		time.Sleep(2 * time.Second)
	}

	fmt.Println("\n" + fmt.Sprintf("%60s", "="))
	fmt.Println("🎯 Real Starlink API test completed!")
}
