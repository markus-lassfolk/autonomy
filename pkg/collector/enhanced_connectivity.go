package collector

import (
	"context"
	"fmt"
	"math"
	"net"
	"os/exec"
	"strconv"
	"strings"
	"sync"
	"time"

	"github.com/markus-lassfolk/autonomy/pkg"
	"github.com/markus-lassfolk/autonomy/pkg/logx"
)

// EnhancedConnectivityCollector provides advanced connectivity monitoring
// that works for both active and standby interfaces
type EnhancedConnectivityCollector struct {
	logger              *logx.Logger
	config              *ConnectivityConfig
	mwan3TrackingCache  map[string]*MWAN3TrackStatus
	connectivityHistory map[string]*ConnectivityHistory
	mu                  sync.RWMutex
}

// ConnectivityConfig holds configuration for connectivity monitoring
type ConnectivityConfig struct {
	// Probe targets and methods
	ProbeTargets   []string      `json:"probe_targets"`    // ["8.8.8.8", "1.1.1.1", "208.67.222.222"]
	ProbeInterval  time.Duration `json:"probe_interval"`   // 5s - How often to probe
	ProbeTimeout   time.Duration `json:"probe_timeout"`    // 3s - Timeout per probe
	ProbesPerCycle int           `json:"probes_per_cycle"` // 3 - Probes per target per cycle

	// Interface-specific probing
	UseInterfaceRouting bool `json:"use_interface_routing"` // true - Route probes through specific interface
	BindToInterface     bool `json:"bind_to_interface"`     // true - Bind sockets to interface

	// mwan3 integration
	UseMWAN3Tracking   bool          `json:"use_mwan3_tracking"`   // true - Leverage mwan3 track data
	MWAN3TrackInterval time.Duration `json:"mwan3_track_interval"` // 10s - How often to read mwan3 track

	// Advanced probing methods
	UseICMPPing bool  `json:"use_icmp_ping"` // true - Try ICMP ping first
	UseTCPProbe bool  `json:"use_tcp_probe"` // true - TCP connect as fallback
	UseUDPProbe bool  `json:"use_udp_probe"` // false - UDP probe (less reliable)
	TCPPorts    []int `json:"tcp_ports"`     // [80, 443, 53] - Ports for TCP probing

	// Quality calculation
	HistorySize           int           `json:"history_size"`            // 20 - Number of samples to keep
	JitterCalculation     string        `json:"jitter_calculation"`      // "stddev" - Method for jitter calculation
	LossCalculationWindow time.Duration `json:"loss_calculation_window"` // 60s - Window for loss calculation

	// Standby interface monitoring
	StandbyProbeInterval time.Duration `json:"standby_probe_interval"` // 30s - Slower probing for standby
	StandbyProbeTargets  []string      `json:"standby_probe_targets"`  // Fewer targets for standby

	// Adaptive monitoring
	AdaptiveMonitoring bool          `json:"adaptive_monitoring"` // true - Adjust based on performance
	HealthyInterval    time.Duration `json:"healthy_interval"`    // 10s - Interval when healthy
	DegradedInterval   time.Duration `json:"degraded_interval"`   // 3s - Interval when degraded
	CriticalInterval   time.Duration `json:"critical_interval"`   // 1s - Interval when critical
}

// MWAN3TrackStatus represents mwan3 tracking status for an interface
type MWAN3TrackStatus struct {
	Interface    string    `json:"interface"`
	Status       string    `json:"status"`        // "online", "offline", "unknown"
	Latency      float64   `json:"latency"`       // Average latency in ms
	PacketLoss   float64   `json:"packet_loss"`   // Packet loss percentage
	TrackTargets []string  `json:"track_targets"` // Targets being tracked
	LastUpdate   time.Time `json:"last_update"`
	Online       bool      `json:"online"`
	Reliability  float64   `json:"reliability"` // 0-100 reliability score
}

// ConnectivityHistory maintains history for connectivity calculations
type ConnectivityHistory struct {
	LatencyHistory []float64   `json:"latency_history"`
	LossHistory    []bool      `json:"loss_history"`
	Timestamps     []time.Time `json:"timestamps"`
	LastProbe      time.Time   `json:"last_probe"`
}

// ConnectivityProbeResult represents result of a single probe
type ConnectivityProbeResult struct {
	Target    string        `json:"target"`
	Method    string        `json:"method"` // "icmp", "tcp", "udp"
	Success   bool          `json:"success"`
	Latency   time.Duration `json:"latency"`
	Error     string        `json:"error,omitempty"`
	Timestamp time.Time     `json:"timestamp"`
}

// NewEnhancedConnectivityCollector creates a new enhanced connectivity collector
func NewEnhancedConnectivityCollector(logger *logx.Logger) *EnhancedConnectivityCollector {
	config := &ConnectivityConfig{
		ProbeTargets:          []string{"8.8.8.8", "1.1.1.1", "208.67.222.222", "9.9.9.9"},
		ProbeInterval:         5 * time.Second,
		ProbeTimeout:          3 * time.Second,
		ProbesPerCycle:        3,
		UseInterfaceRouting:   true,
		BindToInterface:       true,
		UseMWAN3Tracking:      true,
		MWAN3TrackInterval:    10 * time.Second,
		UseICMPPing:           true,
		UseTCPProbe:           true,
		UseUDPProbe:           false,
		TCPPorts:              []int{80, 443, 53},
		HistorySize:           20,
		JitterCalculation:     "stddev",
		LossCalculationWindow: 60 * time.Second,
		StandbyProbeInterval:  30 * time.Second,
		StandbyProbeTargets:   []string{"8.8.8.8", "1.1.1.1"},
		AdaptiveMonitoring:    true,
		HealthyInterval:       10 * time.Second,
		DegradedInterval:      3 * time.Second,
		CriticalInterval:      1 * time.Second,
	}

	return &EnhancedConnectivityCollector{
		logger:              logger,
		config:              config,
		mwan3TrackingCache:  make(map[string]*MWAN3TrackStatus),
		connectivityHistory: make(map[string]*ConnectivityHistory),
	}
}

// CollectEnhancedConnectivity collects comprehensive connectivity metrics
func (ecc *EnhancedConnectivityCollector) CollectEnhancedConnectivity(ctx context.Context, member *pkg.Member, isActive bool) (*pkg.Metrics, error) {
	ecc.mu.Lock()
	defer ecc.mu.Unlock()

	metrics := &pkg.Metrics{
		Timestamp:   time.Now(),
		CollectedAt: time.Now(),
	}

	// Method 1: Try to get mwan3 tracking data first (if available and interface is tracked)
	if ecc.config.UseMWAN3Tracking {
		if mwan3Data, err := ecc.getMWAN3TrackingData(ctx, member); err == nil && mwan3Data != nil {
			metrics.LatencyMS = &mwan3Data.Latency
			metrics.LossPercent = &mwan3Data.PacketLoss

			// Calculate jitter from mwan3 data if we have history
			if history := ecc.connectivityHistory[member.Iface]; history != nil {
				jitter := ecc.calculateJitter(history.LatencyHistory)
				metrics.JitterMS = &jitter
			}

			ecc.logger.Debug("Using mwan3 tracking data",
				"interface", member.Iface,
				"latency", mwan3Data.Latency,
				"loss", mwan3Data.PacketLoss,
				"reliability", mwan3Data.Reliability)

			return metrics, nil
		}
	}

	// Method 2: Perform our own connectivity probing
	probeResults, err := ecc.performConnectivityProbes(ctx, member, isActive)
	if err != nil {
		return nil, fmt.Errorf("connectivity probing failed: %w", err)
	}

	// Process probe results into metrics
	latency, loss, jitter := ecc.processProbeResults(member.Iface, probeResults)

	metrics.LatencyMS = &latency
	metrics.LossPercent = &loss
	metrics.JitterMS = &jitter

	// Set probe method for tracking
	probeMethod := "mixed"
	if len(probeResults) > 0 {
		probeMethod = probeResults[0].Method
	}
	metrics.ProbeMethod = &probeMethod

	ecc.logger.Debug("Enhanced connectivity metrics collected",
		"interface", member.Iface,
		"active", isActive,
		"latency", latency,
		"loss", loss,
		"jitter", jitter,
		"probe_count", len(probeResults))

	return metrics, nil
}

// getMWAN3TrackingData retrieves mwan3 tracking data for an interface
func (ecc *EnhancedConnectivityCollector) getMWAN3TrackingData(ctx context.Context, member *pkg.Member) (*MWAN3TrackStatus, error) {
	// Check cache first
	if cached, exists := ecc.mwan3TrackingCache[member.Iface]; exists {
		if time.Since(cached.LastUpdate) < ecc.config.MWAN3TrackInterval {
			return cached, nil
		}
	}

	// Query mwan3 status
	cmd := exec.CommandContext(ctx, "mwan3", "status")
	output, err := cmd.Output()
	if err != nil {
		return nil, fmt.Errorf("mwan3 status failed: %w", err)
	}

	// Parse mwan3 output for our interface
	trackStatus, err := ecc.parseMWAN3Status(string(output), member.Iface)
	if err != nil {
		return nil, fmt.Errorf("failed to parse mwan3 status: %w", err)
	}

	if trackStatus != nil {
		trackStatus.LastUpdate = time.Now()
		ecc.mwan3TrackingCache[member.Iface] = trackStatus
	}

	return trackStatus, nil
}

// parseMWAN3Status parses mwan3 status output for interface tracking data
func (ecc *EnhancedConnectivityCollector) parseMWAN3Status(output, iface string) (*MWAN3TrackStatus, error) {
	lines := strings.Split(output, "\n")

	for _, line := range lines {
		line = strings.TrimSpace(line)
		if line == "" {
			continue
		}

		// Look for interface tracking information
		// Example: "interface wan_m1 is online and tracking is active"
		// Example: "interface wan_m1 (mob1s1a1) is online, uptime: 00h:12m:34s, tracking is active"
		if strings.Contains(line, iface) || strings.Contains(line, fmt.Sprintf("(%s)", iface)) {
			status := &MWAN3TrackStatus{
				Interface: iface,
				Status:    "unknown",
				Online:    false,
			}

			// Parse status
			if strings.Contains(line, "is online") {
				status.Status = "online"
				status.Online = true
			} else if strings.Contains(line, "is offline") {
				status.Status = "offline"
				status.Online = false
			}

			// Try to extract more detailed tracking info
			if trackInfo, err := ecc.getMWAN3DetailedTracking(iface); err == nil {
				status.Latency = trackInfo.Latency
				status.PacketLoss = trackInfo.PacketLoss
				status.Reliability = trackInfo.Reliability
				status.TrackTargets = trackInfo.TrackTargets
			}

			return status, nil
		}
	}

	return nil, fmt.Errorf("interface %s not found in mwan3 status", iface)
}

// getMWAN3DetailedTracking gets detailed tracking information for an interface
func (ecc *EnhancedConnectivityCollector) getMWAN3DetailedTracking(iface string) (*MWAN3TrackStatus, error) {
	// Try to get detailed tracking info via mwan3 interfaces command
	ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancel()

	cmd := exec.CommandContext(ctx, "mwan3", "interfaces")
	output, err := cmd.Output()
	if err != nil {
		return nil, fmt.Errorf("mwan3 interfaces failed: %w", err)
	}

	// Parse interface details
	// Example output format varies, but typically includes latency and loss info
	lines := strings.Split(string(output), "\n")

	for i, line := range lines {
		if strings.Contains(line, iface) {
			// Look for tracking details in surrounding lines
			trackInfo := &MWAN3TrackStatus{
				Interface:    iface,
				Latency:      0,
				PacketLoss:   0,
				Reliability:  100,
				TrackTargets: []string{},
			}

			// Parse next few lines for tracking details
			for j := i + 1; j < len(lines) && j < i+5; j++ {
				detailLine := strings.TrimSpace(lines[j])

				// Look for latency info
				if strings.Contains(detailLine, "latency") {
					if lat := ecc.extractFloatFromLine(detailLine, "latency"); lat > 0 {
						trackInfo.Latency = lat
					}
				}

				// Look for loss info
				if strings.Contains(detailLine, "loss") || strings.Contains(detailLine, "packet") {
					if loss := ecc.extractFloatFromLine(detailLine, "loss"); loss >= 0 {
						trackInfo.PacketLoss = loss
					}
				}

				// Look for reliability
				if strings.Contains(detailLine, "reliability") {
					if rel := ecc.extractFloatFromLine(detailLine, "reliability"); rel >= 0 {
						trackInfo.Reliability = rel
					}
				}
			}

			return trackInfo, nil
		}
	}

	return nil, fmt.Errorf("detailed tracking info not found for %s", iface)
}

// extractFloatFromLine extracts a float value from a text line
func (ecc *EnhancedConnectivityCollector) extractFloatFromLine(line, keyword string) float64 {
	// Look for patterns like "latency: 45ms" or "loss: 2.5%"
	parts := strings.Fields(line)
	for i, part := range parts {
		if strings.Contains(strings.ToLower(part), keyword) && i+1 < len(parts) {
			valueStr := strings.TrimSuffix(strings.TrimSuffix(parts[i+1], "ms"), "%")
			if val, err := strconv.ParseFloat(valueStr, 64); err == nil {
				return val
			}
		}
	}
	return 0
}

// performConnectivityProbes performs our own connectivity probing
func (ecc *EnhancedConnectivityCollector) performConnectivityProbes(ctx context.Context, member *pkg.Member, isActive bool) ([]ConnectivityProbeResult, error) {
	var results []ConnectivityProbeResult

	// Choose targets and interval based on whether interface is active
	targets := ecc.config.ProbeTargets
	if !isActive {
		targets = ecc.config.StandbyProbeTargets
	}

	for _, target := range targets {
		for i := 0; i < ecc.config.ProbesPerCycle; i++ {
			result := ecc.probeTarget(ctx, target, member)
			results = append(results, result)

			// Small delay between probes to the same target
			if i < ecc.config.ProbesPerCycle-1 {
				time.Sleep(100 * time.Millisecond)
			}
		}
	}

	return results, nil
}

// probeTarget probes a single target using multiple methods
func (ecc *EnhancedConnectivityCollector) probeTarget(ctx context.Context, target string, member *pkg.Member) ConnectivityProbeResult {
	result := ConnectivityProbeResult{
		Target:    target,
		Timestamp: time.Now(),
	}

	// Method 1: Try ICMP ping first (if enabled and available)
	if ecc.config.UseICMPPing {
		if latency, err := ecc.icmpPing(ctx, target, member.Iface); err == nil {
			result.Method = "icmp"
			result.Success = true
			result.Latency = latency
			return result
		}
	}

	// Method 2: Try TCP connect to common ports
	if ecc.config.UseTCPProbe {
		for _, port := range ecc.config.TCPPorts {
			if latency, err := ecc.tcpProbe(ctx, target, port, member.Iface); err == nil {
				result.Method = "tcp"
				result.Success = true
				result.Latency = latency
				return result
			}
		}
	}

	// Method 3: UDP probe (if enabled)
	if ecc.config.UseUDPProbe {
		if latency, err := ecc.udpProbe(ctx, target, member.Iface); err == nil {
			result.Method = "udp"
			result.Success = true
			result.Latency = latency
			return result
		}
	}

	// All methods failed
	result.Method = "failed"
	result.Success = false
	result.Error = "all probe methods failed"

	return result
}

// icmpPing performs ICMP ping through specific interface
func (ecc *EnhancedConnectivityCollector) icmpPing(ctx context.Context, target, iface string) (time.Duration, error) {
	// Use ping command with interface binding
	args := []string{"-c", "1", "-W", "3"}

	// Add interface specification if supported
	if ecc.config.UseInterfaceRouting {
		args = append(args, "-I", iface)
	}

	args = append(args, target)

	start := time.Now()
	cmd := exec.CommandContext(ctx, "ping", args...)
	err := cmd.Run()
	latency := time.Since(start)

	if err != nil {
		return 0, err
	}

	return latency, nil
}

// tcpProbe performs TCP connect probe through specific interface
func (ecc *EnhancedConnectivityCollector) tcpProbe(ctx context.Context, target string, port int, iface string) (time.Duration, error) {
	start := time.Now()

	// Create connection with interface binding if possible
	dialer := &net.Dialer{
		Timeout: ecc.config.ProbeTimeout,
	}

	// Try to bind to specific interface (Linux-specific)
	if ecc.config.BindToInterface {
		// This would require platform-specific code
		// For now, use standard dialer
		ecc.logger.Debug("Interface binding requested but not implemented", "interface", iface)
	}

	conn, err := dialer.DialContext(ctx, "tcp", fmt.Sprintf("%s:%d", target, port))
	latency := time.Since(start)

	if err != nil {
		return 0, err
	}
	defer conn.Close()

	return latency, nil
}

// udpProbe performs UDP probe through specific interface
func (ecc *EnhancedConnectivityCollector) udpProbe(ctx context.Context, target, iface string) (time.Duration, error) {
	start := time.Now()

	// UDP probe is less reliable but can be useful
	conn, err := net.DialTimeout("udp", target+":53", ecc.config.ProbeTimeout)
	latency := time.Since(start)

	if err != nil {
		return 0, err
	}
	defer conn.Close()

	// Send a simple DNS query to test connectivity
	_, err = conn.Write([]byte{0x12, 0x34, 0x01, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01})
	if err != nil {
		return 0, err
	}

	return latency, nil
}

// processProbeResults processes probe results into latency, loss, and jitter metrics
func (ecc *EnhancedConnectivityCollector) processProbeResults(iface string, results []ConnectivityProbeResult) (latency, loss, jitter float64) {
	if len(results) == 0 {
		return 0, 100, 0 // No results = 100% loss
	}

	// Calculate metrics from probe results
	var totalLatency time.Duration
	var successCount int
	var latencies []float64

	for _, result := range results {
		if result.Success {
			totalLatency += result.Latency
			latencies = append(latencies, float64(result.Latency.Milliseconds()))
			successCount++
		}
	}

	// Calculate loss percentage
	loss = float64(len(results)-successCount) / float64(len(results)) * 100

	// Calculate average latency (only from successful probes)
	if successCount > 0 {
		latency = float64(totalLatency.Milliseconds()) / float64(successCount)
	}

	// Update history and calculate jitter
	history := ecc.getOrCreateHistory(iface)
	history.LastProbe = time.Now()

	// Add latencies to history
	for _, lat := range latencies {
		history.LatencyHistory = append(history.LatencyHistory, lat)
		if len(history.LatencyHistory) > ecc.config.HistorySize {
			history.LatencyHistory = history.LatencyHistory[1:]
		}
	}

	// Add loss results to history
	for _, result := range results {
		history.LossHistory = append(history.LossHistory, !result.Success)
		history.Timestamps = append(history.Timestamps, result.Timestamp)
		if len(history.LossHistory) > ecc.config.HistorySize {
			history.LossHistory = history.LossHistory[1:]
			history.Timestamps = history.Timestamps[1:]
		}
	}

	// Calculate jitter
	jitter = ecc.calculateJitter(history.LatencyHistory)

	return latency, loss, jitter
}

// calculateJitter calculates jitter from latency history
func (ecc *EnhancedConnectivityCollector) calculateJitter(latencies []float64) float64 {
	if len(latencies) < 2 {
		return 0
	}

	switch ecc.config.JitterCalculation {
	case "stddev":
		return ecc.calculateStandardDeviation(latencies)
	case "mad":
		return ecc.calculateMAD(latencies)
	default:
		return ecc.calculateStandardDeviation(latencies)
	}
}

// calculateStandardDeviation calculates standard deviation of latencies
func (ecc *EnhancedConnectivityCollector) calculateStandardDeviation(values []float64) float64 {
	if len(values) < 2 {
		return 0
	}

	var sum float64
	for _, v := range values {
		sum += v
	}
	mean := sum / float64(len(values))

	var variance float64
	for _, v := range values {
		diff := v - mean
		variance += diff * diff
	}
	variance /= float64(len(values))

	return math.Sqrt(variance)
}

// calculateMAD calculates Mean Absolute Deviation
func (ecc *EnhancedConnectivityCollector) calculateMAD(values []float64) float64 {
	if len(values) < 2 {
		return 0
	}

	var sum float64
	for _, v := range values {
		sum += v
	}
	mean := sum / float64(len(values))

	var madSum float64
	for _, v := range values {
		madSum += math.Abs(v - mean)
	}

	return madSum / float64(len(values))
}

// getOrCreateHistory gets or creates connectivity history for an interface
func (ecc *EnhancedConnectivityCollector) getOrCreateHistory(iface string) *ConnectivityHistory {
	if history, exists := ecc.connectivityHistory[iface]; exists {
		return history
	}

	history := &ConnectivityHistory{
		LatencyHistory: make([]float64, 0, ecc.config.HistorySize),
		LossHistory:    make([]bool, 0, ecc.config.HistorySize),
		Timestamps:     make([]time.Time, 0, ecc.config.HistorySize),
	}

	ecc.connectivityHistory[iface] = history
	return history
}

// GetAdaptiveInterval returns the appropriate monitoring interval based on interface health
func (ecc *EnhancedConnectivityCollector) GetAdaptiveInterval(latency, loss float64) time.Duration {
	if !ecc.config.AdaptiveMonitoring {
		return ecc.config.ProbeInterval
	}

	// Determine health status and return appropriate interval
	if loss > 10 || latency > 1000 {
		return ecc.config.CriticalInterval // Critical - monitor very frequently
	} else if loss > 2 || latency > 500 {
		return ecc.config.DegradedInterval // Degraded - monitor more frequently
	} else {
		return ecc.config.HealthyInterval // Healthy - normal interval
	}
}

// GetConnectivityHistory returns connectivity history for an interface
func (ecc *EnhancedConnectivityCollector) GetConnectivityHistory(iface string) *ConnectivityHistory {
	ecc.mu.RLock()
	defer ecc.mu.RUnlock()

	if history, exists := ecc.connectivityHistory[iface]; exists {
		return history
	}
	return nil
}

// GetMWAN3TrackingStatus returns mwan3 tracking status for an interface
func (ecc *EnhancedConnectivityCollector) GetMWAN3TrackingStatus(iface string) *MWAN3TrackStatus {
	ecc.mu.RLock()
	defer ecc.mu.RUnlock()

	if status, exists := ecc.mwan3TrackingCache[iface]; exists {
		return status
	}
	return nil
}
