package main

import (
	"context"
	"fmt"
	"log"
	"time"

	"github.com/markus-lassfolk/autonomy/pkg"
	"github.com/markus-lassfolk/autonomy/pkg/collector"
)

func main() {
	fmt.Println("🚀 Testing Starlink API Connection...")

	// Create a Starlink collector with default settings
	config := map[string]interface{}{
		"starlink_api_host":   "192.168.100.1",
		"starlink_api_port":   9200,
		"starlink_timeout_s":  30,
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

	fmt.Println("📡 Attempting to collect Starlink data...")

	// Try to collect data
	ctx, cancel := context.WithTimeout(context.Background(), 60*time.Second)
	defer cancel()

	metrics, err := collector.Collect(ctx, member)
	if err != nil {
		log.Printf("❌ Collection failed: %v", err)
		return
	}

	if metrics == nil {
		log.Println("❌ No metrics returned")
		return
	}

	// Display the results
	fmt.Println("✅ SUCCESS! Got Starlink data:")
	fmt.Printf("  Timestamp: %v\n", metrics.Timestamp)
	if metrics.LatencyMS != nil {
		fmt.Printf("  Latency: %.2f ms\n", *metrics.LatencyMS)
	}
	if metrics.LossPercent != nil {
		fmt.Printf("  Loss: %.2f%%\n", *metrics.LossPercent)
	}
	if metrics.JitterMS != nil {
		fmt.Printf("  Jitter: %.2f ms\n", *metrics.JitterMS)
	}

	if metrics.ObstructionPct != nil {
		fmt.Printf("  Obstruction: %.2f%%\n", *metrics.ObstructionPct*100)
	}

	if metrics.SNR != nil {
		fmt.Printf("  SNR: %d dB\n", *metrics.SNR)
	}

	if metrics.ThermalThrottle != nil {
		fmt.Printf("  Thermal Throttle: %v\n", *metrics.ThermalThrottle)
	}

	if metrics.Roaming != nil {
		fmt.Printf("  Roaming: %v\n", *metrics.Roaming)
	}

	fmt.Println("🎉 Starlink API test completed successfully!")
}
