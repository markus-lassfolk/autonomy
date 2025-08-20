package starlink

import (
	"context"
	"encoding/json"
	"fmt"
	"os/exec"
	"strings"
	"time"

	"github.com/fullstorydev/grpcurl"
	"github.com/jhump/protoreflect/grpcreflect"
	"github.com/markus-lassfolk/autonomy/pkg"
	"github.com/markus-lassfolk/autonomy/pkg/logx"
	"google.golang.org/grpc"
	"google.golang.org/grpc/credentials/insecure"
	"google.golang.org/grpc/reflection/grpc_reflection_v1alpha"
)

// Client provides centralized access to Starlink gRPC API
type Client struct {
	host    string
	port    int
	timeout time.Duration
	logger  *logx.Logger
}

// NewClient creates a new Starlink API client
func NewClient(host string, port int, timeout time.Duration, logger *logx.Logger) *Client {
	return &Client{
		host:    host,
		port:    port,
		timeout: timeout,
		logger:  logger,
	}
}

// DefaultClient creates a client with default Starlink settings
func DefaultClient(logger *logx.Logger) *Client {
	return NewClient("192.168.100.1", 9200, 10*time.Second, logger)
}

// APIMethod represents available Starlink API methods
type APIMethod string

const (
	MethodGetStatus      APIMethod = "get_status"
	MethodGetHistory     APIMethod = "get_history"
	MethodGetDeviceInfo  APIMethod = "get_device_info"
	MethodGetDiagnostics APIMethod = "get_diagnostics"
	MethodGetLocation    APIMethod = "get_location"
)

// CallMethod calls a Starlink gRPC method and returns the JSON response
func (c *Client) CallMethod(ctx context.Context, method APIMethod) (string, error) {
	// Use native gRPC implementation only
	return c.callNativeGRPC(ctx, method)
}

// callNativeGRPC calls the Starlink API using dynamic protobuf reflection (like grpcurl)
func (c *Client) callNativeGRPC(ctx context.Context, method APIMethod) (string, error) {
	// Create gRPC connection
	conn, err := grpc.DialContext(ctx, fmt.Sprintf("%s:%d", c.host, c.port),
		grpc.WithTransportCredentials(insecure.NewCredentials()),
		grpc.WithTimeout(c.timeout))
	if err != nil {
		return "", fmt.Errorf("failed to connect to Starlink API: %w", err)
	}
	defer conn.Close()

	// Create reflection client for dynamic protobuf discovery
	reflectionClient := grpcreflect.NewClient(ctx, grpc_reflection_v1alpha.NewServerReflectionClient(conn))
	descSource := grpcurl.DescriptorSourceFromServer(ctx, reflectionClient)

	// Create the proper JSON request format
	requestJSON := fmt.Sprintf(`{"%s":{}}`, string(method))

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

	// Invoke the RPC using the Handle method
	methodName := "SpaceX.API.Device.Device/Handle"
	err = grpcurl.InvokeRPC(ctx, descSource, conn, methodName, nil, handler, requestReader.Next)
	if err != nil {
		return "", fmt.Errorf("gRPC call failed: %w", err)
	}

	return responseBuffer.String(), nil
}

// GetStatus retrieves current Starlink status
func (c *Client) GetStatus(ctx context.Context) (*StatusResponse, error) {
	response, err := c.CallMethod(ctx, MethodGetStatus)
	if err != nil {
		return nil, err
	}

	var status StatusResponse
	if err := json.Unmarshal([]byte(response), &status); err != nil {
		return nil, fmt.Errorf("failed to parse status response: %w", err)
	}

	return &status, nil
}

// GetLocation retrieves GPS location from Starlink
func (c *Client) GetLocation(ctx context.Context) (*pkg.GPSData, error) {
	response, err := c.CallMethod(ctx, MethodGetLocation)
	if err != nil {
		return nil, err
	}

	// Parse the location response
	var locationResp LocationResponse
	if err := json.Unmarshal([]byte(response), &locationResp); err != nil {
		return nil, fmt.Errorf("failed to parse location response: %w", err)
	}

	// Convert to standard GPS data format
	gpsData := &pkg.GPSData{
		Latitude:   locationResp.GetLocation.LLA.Lat,
		Longitude:  locationResp.GetLocation.LLA.Lon,
		Altitude:   locationResp.GetLocation.LLA.Alt,
		Accuracy:   locationResp.GetLocation.SigmaM,
		Source:     "starlink",
		Valid:      locationResp.GetLocation.LLA.Lat != 0 && locationResp.GetLocation.LLA.Lon != 0,
		Timestamp:  time.Now(),
		Satellites: 0, // Starlink doesn't provide satellite count in location response
	}

	return gpsData, nil
}

// GetDiagnostics retrieves comprehensive diagnostic information
func (c *Client) GetDiagnostics(ctx context.Context) (*DiagnosticsResponse, error) {
	response, err := c.CallMethod(ctx, MethodGetDiagnostics)
	if err != nil {
		return nil, err
	}

	// Log the raw response for debugging
	if c.logger != nil {
		c.logger.Debug("Raw diagnostics response", "response", response)
	}

	var diagnostics DiagnosticsResponse
	if err := json.Unmarshal([]byte(response), &diagnostics); err != nil {
		return nil, fmt.Errorf("failed to parse diagnostics response: %w", err)
	}

	// Handle alerts field - it can be either an empty object {} or an array []
	// We've already successfully parsed it as json.RawMessage, so no additional processing needed

	return &diagnostics, nil
}

// GetMetrics extracts key metrics from Starlink status
func (c *Client) GetMetrics(ctx context.Context) (*pkg.Metrics, error) {
	status, err := c.GetStatus(ctx)
	if err != nil {
		return nil, err
	}

	// Extract metrics from status response
	latency := status.DishGetStatus.PopPingLatencyMs
	lossPercent := status.DishGetStatus.PopPingDropRate * 100 // Convert to percentage
	metrics := &pkg.Metrics{
		LatencyMS:   &latency,
		LossPercent: &lossPercent,
		Timestamp:   time.Now(),
	}

	// Add obstruction percentage if available
	if status.DishGetStatus.ObstructionStats.FractionObstructed > 0 {
		obstructionPct := status.DishGetStatus.ObstructionStats.FractionObstructed * 100
		metrics.ObstructionPct = &obstructionPct
	}

	// Add SNR if available
	if status.DishGetStatus.SNR > 0 {
		snrInt := int(status.DishGetStatus.SNR)
		metrics.SignalStrength = &snrInt
	}

	return metrics, nil
}

// IsAvailable checks if the Starlink API is accessible
func (c *Client) IsAvailable(ctx context.Context) bool {
	// Simple connectivity test
	cmd := exec.CommandContext(ctx, "ping", "-c", "1", "-W", "2", c.host)
	return cmd.Run() == nil
}

// TestMethod tests a specific API method and returns raw response
func (c *Client) TestMethod(ctx context.Context, method APIMethod) (string, error) {
	if c.logger != nil {
		c.logger.LogDebugVerbose("testing_starlink_method", map[string]interface{}{
			"method": string(method),
			"host":   c.host,
			"port":   c.port,
		})
	}

	response, err := c.CallMethod(ctx, method)
	if err != nil {
		if c.logger != nil {
			c.logger.LogDebugVerbose("starlink_method_failed", map[string]interface{}{
				"method": string(method),
				"error":  err.Error(),
			})
		}
		return "", err
	}

	if c.logger != nil {
		c.logger.LogDebugVerbose("starlink_method_success", map[string]interface{}{
			"method":        string(method),
			"response_size": len(response),
		})
	}

	return response, nil
}

// GetHealthData retrieves comprehensive health information
func (c *Client) GetHealthData(ctx context.Context) (*HealthData, error) {
	// Get status for basic health metrics
	status, err := c.GetStatus(ctx)
	if err != nil {
		return nil, fmt.Errorf("failed to get status: %w", err)
	}

	// Get diagnostics for detailed health information
	diagnostics, err := c.GetDiagnostics(ctx)
	if err != nil {
		c.logger.Warn("Failed to get diagnostics, using status only", "error", err)
		diagnostics = nil
	}

	// Compile health data
	health := &HealthData{
		Timestamp:   time.Now(),
		Status:      status,
		Diagnostics: diagnostics,

		// Extract key health indicators
		IsHealthy: c.evaluateHealth(status),
		Issues:    c.identifyIssues(status),
	}

	return health, nil
}

// evaluateHealth determines if Starlink is in a healthy state
func (c *Client) evaluateHealth(status *StatusResponse) bool {
	if status == nil {
		return false
	}

	// Check key health indicators
	if status.DishGetStatus.PopPingLatencyMs > 1000 { // High latency
		return false
	}

	if status.DishGetStatus.PopPingDropRate > 0.05 { // >5% packet loss
		return false
	}

	if status.DishGetStatus.ObstructionStats.FractionObstructed > 0.15 { // >15% obstruction
		return false
	}

	return true
}

// identifyIssues identifies specific health issues
func (c *Client) identifyIssues(status *StatusResponse) []string {
	var issues []string

	if status == nil {
		return []string{"Unable to retrieve status"}
	}

	if status.DishGetStatus.PopPingLatencyMs > 1000 {
		issues = append(issues, fmt.Sprintf("High latency: %.1fms", status.DishGetStatus.PopPingLatencyMs))
	}

	if status.DishGetStatus.PopPingDropRate > 0.05 {
		issues = append(issues, fmt.Sprintf("High packet loss: %.1f%%", status.DishGetStatus.PopPingDropRate*100))
	}

	if status.DishGetStatus.ObstructionStats.FractionObstructed > 0.15 {
		issues = append(issues, fmt.Sprintf("High obstruction: %.1f%%", status.DishGetStatus.ObstructionStats.FractionObstructed*100))
	}

	if status.DishGetStatus.ObstructionStats.CurrentlyObstructed {
		issues = append(issues, "Currently obstructed")
	}

	if !status.DishGetStatus.IsSnrAboveNoiseFloor {
		issues = append(issues, "SNR below noise floor")
	}

	return issues
}
