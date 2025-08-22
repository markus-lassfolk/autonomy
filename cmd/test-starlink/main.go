package main

import (
	"context"
	"encoding/json"
	"fmt"
	"log"
	"net"
	"strings"
	"time"

	"github.com/markus-lassfolk/autonomy/pkg"
	"github.com/markus-lassfolk/autonomy/pkg/collector"
	"google.golang.org/grpc"
	"google.golang.org/grpc/credentials/insecure"
)

func main() {
	fmt.Println("🛰️  Comprehensive Starlink gRPC API Testing...")
	fmt.Println(strings.Repeat("=", 60))

	// Create Starlink collector
	config := map[string]interface{}{
		"timeout":  15 * time.Second,
		"api_host": "192.168.100.1", // Starlink dish IP
	}

	starlinkCollector, err := collector.NewStarlinkCollector(config)
	if err != nil {
		log.Fatalf("❌ Failed to create Starlink collector: %v", err)
	}

	ctx := context.Background()

	// Test 1: Basic connectivity
	fmt.Println("\n🔌 Test 1: Testing connectivity to Starlink gRPC API...")
	_, err = starlinkCollector.TestStarlinkMethod(ctx, "get_status")
	if err != nil {
		log.Printf("❌ Starlink gRPC API connectivity test failed: %v", err)
		fmt.Println("\n💡 This is expected if:")
		fmt.Println("   - Starlink dish is not connected")
		fmt.Println("   - Dish is not in Bypass Mode")
		fmt.Println("   - Network routing to 192.168.100.1:9200 is blocked")
		fmt.Println("   - Dish is not powered on")

		// Try basic network connectivity
		fmt.Println("\n🏓 Testing basic network connectivity...")
		testBasicConnectivity("192.168.100.1")
	} else {
		fmt.Println("✅ Successfully connected to Starlink gRPC API!")
	}

	// Test 2: Raw gRPC calls to all API methods
	fmt.Println("\n📡 Test 2: Testing all gRPC API methods...")
	testAllGRPCMethods(ctx, "192.168.100.1")

	// Test 3: Collector metrics
	fmt.Println("\n📊 Test 3: Testing collector metrics extraction...")
	testCollectorMetrics(ctx, starlinkCollector)

	// Test 4: Comprehensive info
	fmt.Println("\n📋 Test 4: Getting comprehensive dish information...")
	testComprehensiveInfo(ctx, starlinkCollector)

	// Test 5: Hardware health assessment
	fmt.Println("\n🏥 Test 5: Hardware health assessment...")
	testHardwareHealth(ctx, starlinkCollector)

	fmt.Println("\n" + strings.Repeat("=", 60))
	fmt.Println("🎯 Starlink gRPC API comprehensive test completed!")
}

func testAllGRPCMethods(ctx context.Context, apiHost string) {
	// Connect directly to gRPC server
	conn, err := grpc.DialContext(ctx, fmt.Sprintf("%s:9200", apiHost),
		grpc.WithTransportCredentials(insecure.NewCredentials()),
		grpc.WithTimeout(10*time.Second))
	if err != nil {
		log.Printf("❌ Failed to connect to gRPC server: %v", err)
		return
	}
	defer conn.Close()

	methods := []string{
		"get_status",
		"get_history",
		"get_device_info",
		"get_location",
		"get_diagnostics",
	}

	for _, method := range methods {
		fmt.Printf("\n🔍 Testing method: %s\n", method)
		fmt.Println(strings.Repeat("─", 40))

		response, err := callStarlinkGRPCDirect(ctx, conn, method)
		if err != nil {
			fmt.Printf("❌ Error calling %s: %v\n", method, err)
			continue
		}

		// Pretty print JSON response
		var jsonData interface{}
		if err := json.Unmarshal(response, &jsonData); err != nil {
			fmt.Printf("⚠️  Raw response (not JSON): %s\n", string(response))
		} else {
			prettyJSON, _ := json.MarshalIndent(jsonData, "", "  ")
			fmt.Printf("✅ Response:\n%s\n", string(prettyJSON))
		}
	}
}

func callStarlinkGRPCDirect(ctx context.Context, conn *grpc.ClientConn, method string) ([]byte, error) {
	// Create the request payload based on the method
	var requestData string
	switch method {
	case "get_status":
		requestData = `{"get_status":{}}`
	case "get_history":
		requestData = `{"get_history":{}}`
	case "get_device_info":
		requestData = `{"get_device_info":{}}`
	case "get_location":
		requestData = `{"get_location":{}}`
	case "get_diagnostics":
		requestData = `{"get_diagnostics":{}}`
	default:
		return nil, fmt.Errorf("unknown method: %s", method)
	}

	// Use grpc.Invoke to make the raw call
	var response json.RawMessage
	err := conn.Invoke(ctx, "/SpaceX.API.Device.Device/Handle",
		json.RawMessage(requestData), &response)
	if err != nil {
		return nil, fmt.Errorf("gRPC invoke failed: %w", err)
	}

	return []byte(response), nil
}

func testCollectorMetrics(ctx context.Context, starlinkCollector *collector.StarlinkCollector) {
	// Create a mock member for testing
	member := &pkg.Member{
		Name:  "starlink",
		Iface: "wan_starlink",
		Class: pkg.ClassStarlink,
	}

	metrics, err := starlinkCollector.Collect(ctx, member)
	if err != nil {
		fmt.Printf("❌ Failed to collect metrics: %v\n", err)
		return
	}

	fmt.Println("✅ Collected metrics:")
	metricsJSON, _ := json.MarshalIndent(metrics, "", "  ")
	fmt.Printf("%s\n", string(metricsJSON))
}

func testComprehensiveInfo(ctx context.Context, starlinkCollector *collector.StarlinkCollector) {
	// Use TestStarlinkMethod to get comprehensive info
	info, err := starlinkCollector.TestStarlinkMethod(ctx, "get_device_info")
	if err != nil {
		fmt.Printf("❌ Failed to get Starlink info: %v\n", err)
		return
	}

	fmt.Println("✅ Comprehensive dish information:")
	fmt.Printf("   Raw response: %s\n", info)
}

func testHardwareHealth(ctx context.Context, starlinkCollector *collector.StarlinkCollector) {
	// Use TestStarlinkMethod to get diagnostics info
	health, err := starlinkCollector.TestStarlinkMethod(ctx, "get_diagnostics")
	if err != nil {
		fmt.Printf("❌ Failed to check hardware health: %v\n", err)
		return
	}

	fmt.Println("✅ Hardware health assessment:")
	fmt.Printf("   Raw diagnostics response: %s\n", health)
}

func testBasicConnectivity(host string) {
	// Test TCP connection to gRPC port
	conn, err := net.DialTimeout("tcp", fmt.Sprintf("%s:9200", host), 5*time.Second)
	if err != nil {
		fmt.Printf("❌ Cannot reach %s:9200 - %v\n", host, err)

		// Try to resolve hostname
		ips, err := net.LookupIP(host)
		if err != nil {
			fmt.Printf("❌ Cannot resolve %s - %v\n", host, err)
		} else {
			fmt.Printf("✅ %s resolves to: %v\n", host, ips)
		}
		return
	}
	conn.Close()

	fmt.Printf("✅ TCP connection to %s:9200 successful\n", host)
}
