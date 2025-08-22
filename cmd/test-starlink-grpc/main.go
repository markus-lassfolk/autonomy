package main

import (
	"context"
	"fmt"
	"strings"
	"time"

	"google.golang.org/grpc"
	"google.golang.org/grpc/credentials/insecure"
	"google.golang.org/protobuf/encoding/protojson"
	"google.golang.org/protobuf/types/known/structpb"
)

func main() {
	fmt.Println("🛰️  Testing Starlink gRPC API with proper protobuf...")
	fmt.Println(strings.Repeat("=", 60))

	// Test connectivity
	fmt.Println("\n🔌 Connecting to Starlink gRPC server...")

	ctx, cancel := context.WithTimeout(context.Background(), 15*time.Second)
	defer cancel()

	conn, err := grpc.DialContext(ctx, "192.168.100.1:9200",
		grpc.WithTransportCredentials(insecure.NewCredentials()),
		grpc.WithBlock())
	if err != nil {
		fmt.Printf("❌ Failed to connect: %v\n", err)
		return
	}
	defer conn.Close()

	fmt.Println("✅ Connected to gRPC server!")

	// Test different methods
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

		err := testGRPCMethod(ctx, conn, method)
		if err != nil {
			fmt.Printf("❌ Error: %v\n", err)
		}
	}

	fmt.Println("\n" + strings.Repeat("=", 60))
	fmt.Println("🎯 Starlink gRPC API test completed!")
}

func testGRPCMethod(ctx context.Context, conn *grpc.ClientConn, method string) error {
	// Create request using structpb for dynamic message creation
	requestFields := map[string]*structpb.Value{
		method: structpb.NewStructValue(&structpb.Struct{
			Fields: map[string]*structpb.Value{},
		}),
	}

	request := &structpb.Struct{
		Fields: requestFields,
	}

	var response structpb.Struct

	// Make the gRPC call
	err := conn.Invoke(ctx, "/SpaceX.API.Device.Device/Handle", request, &response)
	if err != nil {
		return fmt.Errorf("gRPC call failed: %w", err)
	}

	// Convert response to JSON for display
	jsonBytes, err := protojson.Marshal(&response)
	if err != nil {
		return fmt.Errorf("failed to marshal response: %w", err)
	}

	fmt.Printf("✅ Response:\n%s\n", string(jsonBytes))
	return nil
}

// Alternative approach using raw bytes
func testRawGRPCMethod(ctx context.Context, conn *grpc.ClientConn, method string) error {
	// Create a minimal protobuf message
	// This is a simplified approach that may work with some gRPC services

	// For get_status, we need to send: {"get_status": {}}
	// In protobuf, this would be a message with field number 1 (get_status) containing an empty message

	var requestBytes []byte
	var responseBytes []byte

	switch method {
	case "get_status":
		// Construct minimal protobuf for get_status request
		// Field 1 (get_status) = empty message
		requestBytes = []byte{0x0a, 0x00} // tag 1, wire type 2 (length-delimited), length 0
	case "get_device_info":
		// Field 2 (get_device_info) = empty message
		requestBytes = []byte{0x12, 0x00} // tag 2, wire type 2, length 0
	case "get_location":
		// Field 4 (get_location) = empty message
		requestBytes = []byte{0x22, 0x00} // tag 4, wire type 2, length 0
	case "get_history":
		// Field 3 (get_history) = empty message
		requestBytes = []byte{0x1a, 0x00} // tag 3, wire type 2, length 0
	case "get_diagnostics":
		// Field 5 (get_diagnostics) = empty message
		requestBytes = []byte{0x2a, 0x00} // tag 5, wire type 2, length 0
	default:
		return fmt.Errorf("unknown method: %s", method)
	}

	// Make raw gRPC call using byte slices
	err := conn.Invoke(ctx, "/SpaceX.API.Device.Device/Handle",
		requestBytes,
		&responseBytes)
	if err != nil {
		return fmt.Errorf("raw gRPC call failed: %w", err)
	}

	fmt.Printf("✅ Raw response (%d bytes): %x\n", len(responseBytes), responseBytes)
	return nil
}
