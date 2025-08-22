package main

import (
	"context"
	"fmt"
	"time"

	"google.golang.org/grpc"
	"google.golang.org/grpc/reflection/grpc_reflection_v1alpha"
)

func main() {
	fmt.Println("🛰️  Starlink gRPC Debug Testing...")
	fmt.Println("=" + fmt.Sprintf("%60s", "="))

	// Test 1: Basic gRPC connection
	fmt.Println("\n🔌 Test 1: Testing gRPC connection...")

	ctx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
	defer cancel()

	conn, err := grpc.DialContext(ctx, "192.168.100.1:9200",
		grpc.WithInsecure(),
		grpc.WithTimeout(10*time.Second))
	if err != nil {
		fmt.Printf("❌ Failed to connect: %v\n", err)
		return
	}
	defer conn.Close()

	fmt.Println("✅ gRPC connection established")

	// Test 2: Test gRPC reflection
	fmt.Println("\n🔍 Test 2: Testing gRPC reflection...")

	reflectionClient := grpc_reflection_v1alpha.NewServerReflectionClient(conn)
	stream, err := reflectionClient.ServerReflectionInfo(ctx)
	if err != nil {
		fmt.Printf("❌ Failed to create reflection stream: %v\n", err)
		return
	}

	// Request service list
	err = stream.Send(&grpc_reflection_v1alpha.ServerReflectionRequest{
		MessageRequest: &grpc_reflection_v1alpha.ServerReflectionRequest_ListServices{
			ListServices: "*",
		},
	})
	if err != nil {
		fmt.Printf("❌ Failed to send list services request: %v\n", err)
		return
	}

	resp, err := stream.Recv()
	if err != nil {
		fmt.Printf("❌ Failed to receive services: %v\n", err)
		return
	}

	if listResp := resp.GetListServicesResponse(); listResp != nil {
		fmt.Println("✅ Available gRPC services:")
		for _, service := range listResp.Service {
			fmt.Printf("   📡 %s\n", service.Name)
		}
	}

	// Test 3: Get service description for Device service
	fmt.Println("\n📋 Test 3: Getting Device service description...")

	err = stream.Send(&grpc_reflection_v1alpha.ServerReflectionRequest{
		MessageRequest: &grpc_reflection_v1alpha.ServerReflectionRequest_FileContainingSymbol{
			FileContainingSymbol: "SpaceX.API.Device.Device",
		},
	})
	if err != nil {
		fmt.Printf("❌ Failed to send service description request: %v\n", err)
		return
	}

	resp, err = stream.Recv()
	if err != nil {
		fmt.Printf("❌ Failed to receive service description: %v\n", err)
		return
	}

	if fileResp := resp.GetFileDescriptorResponse(); fileResp != nil {
		fmt.Printf("✅ Got file descriptor response with %d files\n", len(fileResp.FileDescriptorProto))
		for i, file := range fileResp.FileDescriptorProto {
			fmt.Printf("   📄 File %d: %d bytes\n", i+1, len(file))
		}
	}

	// Test 4: Try raw protobuf call
	fmt.Println("\n🔧 Test 4: Testing raw protobuf call...")

	// Create a simple protobuf request for get_status
	// This is field 1 with an empty get_status message
	request := []byte{
		0x0A, 0x00, // Field 1 (get_status), length 0
	}

	var response []byte
	err = conn.Invoke(ctx, "/SpaceX.API.Device.Device/Handle", request, &response)
	if err != nil {
		fmt.Printf("❌ gRPC call failed: %v\n", err)

		// Let's try different approach - using grpc.CallOption
		fmt.Println("\n🔄 Trying alternative approach...")

		// Try with proper protobuf message structure
		alternateRequest := []byte{
			0x0A, 0x02, // Field 1, length 2
			0x08, 0x01, // Nested field 1, value 1
		}

		err = conn.Invoke(ctx, "/SpaceX.API.Device.Device/Handle", alternateRequest, &response)
		if err != nil {
			fmt.Printf("❌ Alternative approach also failed: %v\n", err)
		} else {
			fmt.Printf("✅ Alternative approach succeeded! Response length: %d bytes\n", len(response))
			if len(response) > 0 && len(response) < 1000 {
				fmt.Printf("   Response (hex): %x\n", response[:min(len(response), 100)])
			}
		}
	} else {
		fmt.Printf("✅ gRPC call succeeded! Response length: %d bytes\n", len(response))
		if len(response) > 0 && len(response) < 1000 {
			fmt.Printf("   Response (hex): %x\n", response[:min(len(response), 100)])
		}
	}

	if err := stream.CloseSend(); err != nil {
		fmt.Printf("❌ Failed to close stream: %v\n", err)
	}

	fmt.Println("\n" + fmt.Sprintf("%60s", "="))
	fmt.Println("🎯 Starlink gRPC debug test completed!")
}

func min(a, b int) int {
	if a < b {
		return a
	}
	return b
}
