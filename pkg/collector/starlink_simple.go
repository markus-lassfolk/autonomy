package collector

import (
	"context"
	"fmt"
	"time"

	"github.com/markus-lassfolk/autonomy/pkg"
	"github.com/markus-lassfolk/autonomy/pkg/logx"
	"github.com/markus-lassfolk/autonomy/pkg/starlink"
)

// SimpleStarlinkCollector uses centralized Starlink client to collect metrics
type SimpleStarlinkCollector struct {
	*BaseCollector
	apiHost        string
	apiPort        int
	timeout        time.Duration
	starlinkClient *starlink.Client
}

// StarlinkResponse represents the simplified response from Starlink API
type StarlinkResponse struct {
	DishGetStatus struct {
		DeviceInfo struct {
			ID              string `json:"id"`
			HardwareVersion string `json:"hardwareVersion"`
			SoftwareVersion string `json:"softwareVersion"`
		} `json:"deviceInfo"`
		DeviceState struct {
			UptimeS uint64 `json:"uptimeS"`
		} `json:"deviceState"`
		ObstructionStats struct {
			FractionObstructed  float64 `json:"fractionObstructed"`
			CurrentlyObstructed bool    `json:"currentlyObstructed"`
			ValidS              int     `json:"validS"`
		} `json:"obstructionStats"`
		PopPingLatencyMs      float64 `json:"popPingLatencyMs"`
		PopPingDropRate       float64 `json:"popPingDropRate"`
		DownlinkThroughputBps float64 `json:"downlinkThroughputBps"`
		UplinkThroughputBps   float64 `json:"uplinkThroughputBps"`
		SNR                   float64 `json:"snr"`
		IsSnrAboveNoiseFloor  bool    `json:"isSnrAboveNoiseFloor"`
		IsSnrPersistentlyLow  bool    `json:"isSnrPersistentlyLow"`
	} `json:"dishGetStatus"`
}

// NewSimpleStarlinkCollector creates a new simple Starlink collector
func NewSimpleStarlinkCollector(config map[string]interface{}) (*SimpleStarlinkCollector, error) {
	timeout := 10 * time.Second
	if timeoutVal, ok := config["timeout"].(time.Duration); ok {
		timeout = timeoutVal
	}

	targets := []string{"8.8.8.8", "1.1.1.1"}
	if targetsVal, ok := config["targets"].([]string); ok {
		targets = targetsVal
	}

	logger := &logx.Logger{} // Default logger for now
	base := NewBaseCollector(timeout, targets, logger)

	apiHost := "192.168.100.1"
	if host, ok := config["starlink_api_host"].(string); ok && host != "" {
		apiHost = host
	}

	apiPort := 9200
	if port, ok := config["starlink_api_port"].(int); ok && port > 0 {
		apiPort = port
	}

	// Initialize centralized Starlink client
	starlinkClient := starlink.NewClient(apiHost, apiPort, timeout, nil)

	return &SimpleStarlinkCollector{
		BaseCollector:  base,
		apiHost:        apiHost,
		apiPort:        apiPort,
		timeout:        timeout,
		starlinkClient: starlinkClient,
	}, nil
}

// Collect collects metrics from Starlink dish using grpcurl
func (sc *SimpleStarlinkCollector) Collect(ctx context.Context, member *pkg.Member) (*pkg.Metrics, error) {
	// Use centralized Starlink client (preferred method)
	metrics, err := sc.starlinkClient.GetMetrics(ctx)
	if err == nil {
		return metrics, nil
	}

	// Fallback to basic connectivity test
	return sc.collectFallback(ctx, member)
}

// collectFallback provides basic connectivity metrics when API fails
func (sc *SimpleStarlinkCollector) collectFallback(ctx context.Context, member *pkg.Member) (*pkg.Metrics, error) {
	// Use base collector for basic ping/latency
	return sc.BaseCollector.CollectCommonMetrics(ctx, member)
}

// convertToMetrics converts Starlink API response to standard metrics
func (sc *SimpleStarlinkCollector) convertToMetrics(response *StarlinkResponse, member *pkg.Member) (*pkg.Metrics, error) {
	status := &response.DishGetStatus

	metrics := &pkg.Metrics{
		Timestamp: time.Now(),
	}

	// Network performance metrics
	if status.PopPingLatencyMs > 0 {
		latency := status.PopPingLatencyMs
		metrics.LatencyMS = &latency
	}
	if status.PopPingDropRate >= 0 {
		lossPercent := status.PopPingDropRate * 100 // Convert to percentage
		metrics.LossPercent = &lossPercent
	}

	// Starlink-specific metrics
	if status.ObstructionStats.FractionObstructed >= 0 {
		obstructionPct := status.ObstructionStats.FractionObstructed * 100
		metrics.ObstructionPct = &obstructionPct
	}

	if status.SNR > 0 {
		snrInt := int(status.SNR)
		metrics.SNR = &snrInt
	}

	// Signal quality indicators
	metrics.IsSNRAboveNoiseFloor = &status.IsSnrAboveNoiseFloor
	metrics.IsSNRPersistentlyLow = &status.IsSnrPersistentlyLow

	// Device state
	if status.DeviceState.UptimeS > 0 {
		uptimeInt := int64(status.DeviceState.UptimeS)
		metrics.UptimeS = &uptimeInt
	}

	// Calculate jitter (estimate based on latency stability)
	if metrics.LatencyMS != nil && *metrics.LatencyMS > 0 {
		// Simple jitter estimation - in real implementation this would be calculated from multiple samples
		jitter := *metrics.LatencyMS * 0.1 // 10% of latency as jitter estimate
		metrics.JitterMS = &jitter
	}

	return metrics, nil
}

// GetClass returns the collector class
func (sc *SimpleStarlinkCollector) GetClass() string {
	return string(pkg.ClassStarlink)
}

// GetName returns the collector name
func (sc *SimpleStarlinkCollector) GetName() string {
	return "simple_starlink"
}

// String returns a string representation
func (sc *SimpleStarlinkCollector) String() string {
	return fmt.Sprintf("SimpleStarlinkCollector{host=%s, port=%d, timeout=%v}",
		sc.apiHost, sc.apiPort, sc.timeout)
}
