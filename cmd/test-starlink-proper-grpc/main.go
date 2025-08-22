package main

import (
	"context"
	"fmt"
	"time"

	"google.golang.org/grpc"
	"google.golang.org/grpc/reflection/grpc_reflection_v1alpha"
	"google.golang.org/protobuf/proto"
	"google.golang.org/protobuf/types/known/emptypb"
)

func main() {
	fmt.Println("🛰️  Starlink Proper gRPC Testing...")
	fmt.Println("=" + fmt.Sprintf("%60s", "="))

	ctx, cancel := context.WithTimeout(context.Background(), 30*time.Second)
	defer cancel()

	// Connect to Starlink
	conn, err := grpc.DialContext(ctx, "192.168.100.1:9200",
		grpc.WithInsecure(),
		grpc.WithTimeout(10*time.Second))
	if err != nil {
		fmt.Printf("❌ Failed to connect: %v\n", err)
		return
	}
	defer conn.Close()

	fmt.Println("✅ Connected to Starlink gRPC API")

	// Test 1: Use reflection to get service definitions
	fmt.Println("\n🔍 Test 1: Getting service definitions...")

	reflectionClient := grpc_reflection_v1alpha.NewServerReflectionClient(conn)
	stream, err := reflectionClient.ServerReflectionInfo(ctx)
	if err != nil {
		fmt.Printf("❌ Failed to create reflection stream: %v\n", err)
		return
	}
	defer stream.CloseSend()

	// Get file descriptor for Device service
	err = stream.Send(&grpc_reflection_v1alpha.ServerReflectionRequest{
		MessageRequest: &grpc_reflection_v1alpha.ServerReflectionRequest_FileContainingSymbol{
			FileContainingSymbol: "SpaceX.API.Device.Device",
		},
	})
	if err != nil {
		fmt.Printf("❌ Failed to send service description request: %v\n", err)
		return
	}

	resp, err := stream.Recv()
	if err != nil {
		fmt.Printf("❌ Failed to receive service description: %v\n", err)
		return
	}

	if fileResp := resp.GetFileDescriptorResponse(); fileResp == nil {
		fmt.Println("❌ No file descriptor response")
		return
	}

	fmt.Println("✅ Got service definitions")

	// Test 2: Try simple approach with empty message
	fmt.Println("\n📡 Test 2: Testing with empty protobuf message...")

	// Create an empty protobuf message
	emptyMsg := &emptypb.Empty{}
	var response proto.Message

	err = conn.Invoke(ctx, "/SpaceX.API.Device.Device/Handle", emptyMsg, &response)
	if err != nil {
		fmt.Printf("❌ Empty message call failed: %v\n", err)
	} else {
		fmt.Println("✅ Empty message call succeeded!")
		fmt.Printf("   Response type: %T\n", response)
	}

	// Test 3: Try creating a basic request message dynamically
	fmt.Println("\n🔧 Test 3: Testing with dynamic protobuf message...")

	// This is a more complex approach - we'd need to parse the file descriptors
	// and create proper request messages. For now, let's try a simpler approach.

	// Test 4: Use our existing collector but with debugging
	fmt.Println("\n🛠️  Test 4: Testing our collector with detailed debugging...")

	// Let's manually test what our collector does
	testCollectorApproach(ctx, conn)

	fmt.Println("\n" + fmt.Sprintf("%60s", "="))
	fmt.Println("🎯 Starlink proper gRPC test completed!")
}

func testCollectorApproach(ctx context.Context, conn *grpc.ClientConn) {
	fmt.Println("   🔍 Testing collector's approach...")

	// This simulates what our collector does
	// Try to call the Handle method with different approaches

	// Approach 1: Try with nil
	fmt.Println("   📞 Trying with nil request...")
	var nilResponse interface{}
	err := conn.Invoke(ctx, "/SpaceX.API.Device.Device/Handle", nil, &nilResponse)
	if err != nil {
		fmt.Printf("   ❌ Nil request failed: %v\n", err)
	} else {
		fmt.Println("   ✅ Nil request succeeded!")
	}

	// Approach 2: Try with empty proto message
	fmt.Println("   📞 Trying with empty proto message...")
	empty := &emptypb.Empty{}
	var emptyResponse interface{}
	err = conn.Invoke(ctx, "/SpaceX.API.Device.Device/Handle", empty, &emptyResponse)
	if err != nil {
		fmt.Printf("   ❌ Empty proto failed: %v\n", err)
	} else {
		fmt.Println("   ✅ Empty proto succeeded!")
		fmt.Printf("   Response: %+v\n", emptyResponse)
	}

	// Approach 3: Try to create a minimal Request message
	fmt.Println("   📞 Trying with minimal request structure...")

	// We know from reflection that there's a Request message
	// Let's try to create a basic one
	fmt.Println("   ⚠️  Need proper protobuf definitions to continue...")
	fmt.Println("   💡 Recommendation: Use grpcurl or generate proper .proto files")
}
