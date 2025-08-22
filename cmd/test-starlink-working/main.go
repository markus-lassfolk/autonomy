package main

import (
	"context"
	"encoding/json"
	"fmt"
	"strings"
	"time"

	"github.com/fullstorydev/grpcurl"
	"github.com/jhump/protoreflect/grpcreflect"
	"google.golang.org/grpc"
	"google.golang.org/grpc/credentials/insecure"
	"google.golang.org/grpc/reflection/grpc_reflection_v1alpha"
)

func main() {
	fmt.Println("🛰️  Working Starlink gRPC Client")
	fmt.Println("=" + fmt.Sprintf("%50s", "="))

	ctx, cancel := context.WithTimeout(context.Background(), 30*time.Second)
	defer cancel()

	// Connect to Starlink
	conn, err := grpc.DialContext(ctx, "192.168.100.1:9200",
		grpc.WithTransportCredentials(insecure.NewCredentials()),
		grpc.WithTimeout(10*time.Second))
	if err != nil {
		fmt.Printf("❌ Failed to connect: %v\n", err)
		return
	}
	defer conn.Close()

	fmt.Println("✅ Connected to Starlink gRPC API")

	// Create reflection client
	reflectionClient := grpcreflect.NewClient(ctx, grpc_reflection_v1alpha.NewServerReflectionClient(conn))
	descSource := grpcurl.DescriptorSourceFromServer(ctx, reflectionClient)

	// Test all Starlink API methods
	methods := []string{
		"get_status",
		"get_device_info",
		"get_location",
		"get_diagnostics",
		"get_history",
	}

	for _, method := range methods {
		fmt.Printf("\n🔍 Testing %s method:\n", method)
		fmt.Printf("=" + strings.Repeat("=", len(method)+15) + "\n")

		// Create the proper JSON request format
		requestJSON := fmt.Sprintf(`{"%s":{}}`, method)
		fmt.Printf("Request JSON: %s\n", requestJSON)

		// Create request parser
		requestReader := grpcurl.NewJSONRequestParser(strings.NewReader(requestJSON), grpcurl.AnyResolverFromDescriptorSource(descSource))

		// Create response handler
		var responseBuffer strings.Builder
		formatter := grpcurl.NewJSONFormatter(false, grpcurl.AnyResolverFromDescriptorSource(descSource))
		handler := &grpcurl.DefaultEventHandler{
			Out:            &responseBuffer,
			Formatter:      formatter,
			VerbosityLevel: 0,
		}

		// Invoke the RPC
		methodName := "SpaceX.API.Device.Device/Handle"
		err := grpcurl.InvokeRPC(ctx, descSource, conn, methodName, nil, handler, requestReader.Next)
		if err != nil {
			fmt.Printf("❌ %s failed: %v\n", method, err)
			continue
		}

		response := responseBuffer.String()
		fmt.Printf("✅ %s succeeded!\n", method)
		fmt.Printf("Response size: %d bytes\n", len(response))

		// Pretty print the response
		if len(response) > 0 {
			var prettyJSON map[string]interface{}
			if err := json.Unmarshal([]byte(response), &prettyJSON); err == nil {
				prettyBytes, _ := json.MarshalIndent(prettyJSON, "", "  ")
				fmt.Printf("Response:\n%s\n", string(prettyBytes))
			} else {
				fmt.Printf("Response: %s\n", response)
			}
		}
	}

	fmt.Println("\n" + fmt.Sprintf("%50s", "="))
	fmt.Println("🎯 Working Starlink gRPC test completed!")
}
