package main

import (
	"context"
	"fmt"
	"os/exec"
	"strings"
	"time"
)

func main() {
	fmt.Println("🛰️  Testing Starlink API using external grpcurl...")
	fmt.Println(strings.Repeat("=", 60))

	// Test basic connectivity first
	fmt.Println("\n🔌 Testing TCP connectivity to 192.168.100.1:9200...")
	if !testTCPConnectivity("192.168.100.1", "9200") {
		fmt.Println("❌ Cannot reach Starlink device")
		return
	}
	fmt.Println("✅ TCP connection successful")

	// Test all gRPC methods using grpcurl
	methods := []string{
		"get_status",
		"get_device_info",
		"get_location",
		"get_history",
		"get_diagnostics",
	}

	for _, method := range methods {
		fmt.Printf("\n📡 Testing %s...\n", method)
		fmt.Println(strings.Repeat("-", 40))

		output, err := callStarlinkAPI(method)
		if err != nil {
			fmt.Printf("❌ Error: %v\n", err)
			continue
		}

		fmt.Printf("✅ Response:\n%s\n", output)
	}

	fmt.Println("\n" + strings.Repeat("=", 60))
	fmt.Println("🎯 Starlink API test completed!")
}

func testTCPConnectivity(host, port string) bool {
	ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancel()

	cmd := exec.CommandContext(ctx, "powershell", "-Command",
		fmt.Sprintf("Test-NetConnection -ComputerName %s -Port %s -InformationLevel Quiet", host, port))

	err := cmd.Run()
	return err == nil
}

func callStarlinkAPI(method string) (string, error) {
	ctx, cancel := context.WithTimeout(context.Background(), 15*time.Second)
	defer cancel()

	// Try to use grpcurl if available
	cmd := exec.CommandContext(ctx, "grpcurl",
		"-plaintext",
		"-d", fmt.Sprintf(`{"%s":{}}`, method),
		"192.168.100.1:9200",
		"SpaceX.API.Device.Device/Handle")

	output, err := cmd.CombinedOutput()
	if err != nil {
		// If grpcurl is not available, try PowerShell approach
		return callWithPowerShell(ctx, method)
	}

	return string(output), nil
}

func callWithPowerShell(ctx context.Context, method string) (string, error) {
	// Try to make a simple HTTP request to see if there's an HTTP interface
	cmd := exec.CommandContext(ctx, "powershell", "-Command",
		`
			try {
				$response = Invoke-WebRequest -Uri "http://192.168.100.1/api/v1/status" -TimeoutSec 10
				$response.Content
			} catch {
				"HTTP Error: " + $_.Exception.Message
			}
		`)

	output, err := cmd.CombinedOutput()
	if err != nil {
		return "", fmt.Errorf("PowerShell request failed: %w", err)
	}

	return string(output), nil
}
