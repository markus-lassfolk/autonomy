package collector

import (
	"context"
	"encoding/json"
	"fmt"
	"math"
	"os/exec"
	"strconv"
	"strings"
	"sync"
	"time"

	"github.com/markus-lassfolk/autonomy/pkg"
	"github.com/markus-lassfolk/autonomy/pkg/logx"
)

// CellularStabilityCollector provides enhanced cellular stability monitoring
type CellularStabilityCollector struct {
	logger                *logx.Logger
	config                *CellularStabilityConfig
	ringBuffer            *CellularRingBuffer
	lastMetrics           *pkg.Metrics
	connectivityCollector *EnhancedConnectivityCollector
	mu                    sync.RWMutex
	stabilityHistory      map[string]*StabilityWindow // keyed by interface name
}

// CellularStabilityConfig holds configuration for stability monitoring
type CellularStabilityConfig struct {
	WindowDurationMinutes       int     `json:"window_duration_minutes"`        // Rolling window size (default: 10)
	SampleIntervalSeconds       int     `json:"sample_interval_seconds"`        // How often to sample (default: 5)
	RSRPHealthyThreshold        float64 `json:"rsrp_healthy_threshold"`         // -90 dBm
	RSRPUnhealthyThreshold      float64 `json:"rsrp_unhealthy_threshold"`       // -110 dBm
	RSRQHealthyThreshold        float64 `json:"rsrq_healthy_threshold"`         // -12 dB
	RSRQUnhealthyThreshold      float64 `json:"rsrq_unhealthy_threshold"`       // -16 dB
	SINRHealthyThreshold        float64 `json:"sinr_healthy_threshold"`         // 5 dB
	SINRUnhealthyThreshold      float64 `json:"sinr_unhealthy_threshold"`       // 0 dB
	VariancePenaltyThreshold    float64 `json:"variance_penalty_threshold"`     // 6 dB stddev
	CellChangesPenaltyThreshold int     `json:"cell_changes_penalty_threshold"` // 3 changes
	ThroughputMinKbps           float64 `json:"throughput_min_kbps"`            // 100 Kbps minimum
	HysteresisGoodSeconds       int     `json:"hysteresis_good_seconds"`        // 60s above threshold
	HysteresisBadSeconds        int     `json:"hysteresis_bad_seconds"`         // 30s below threshold
}

// CellularSample represents a single cellular signal measurement
type CellularSample struct {
	Timestamp      time.Time `json:"timestamp"`
	RSRP           float64   `json:"rsrp"`            // Reference Signal Received Power (dBm)
	RSRQ           float64   `json:"rsrq"`            // Reference Signal Received Quality (dB)
	SINR           float64   `json:"sinr"`            // Signal-to-Interference-plus-Noise Ratio (dB)
	CellID         string    `json:"cell_id"`         // Cell identifier (TAC-CID or similar)
	Band           string    `json:"band"`            // LTE/5G band
	NetworkType    string    `json:"network_type"`    // LTE, 5G NSA, 5G SA
	ThroughputKbps float64   `json:"throughput_kbps"` // Calculated throughput
	PCI            int       `json:"pci"`             // Physical Cell ID
	EARFCN         int       `json:"earfcn"`          // E-UTRA Absolute Radio Frequency Channel Number
}

// CellularRingBuffer implements a circular buffer for cellular samples
type CellularRingBuffer struct {
	samples []CellularSample
	index   int
	size    int
	full    bool
	mu      sync.RWMutex
}

// StabilityWindow tracks stability metrics over time
type StabilityWindow struct {
	CurrentScore      int       `json:"current_score"`      // 0-100 stability score
	LastUpdate        time.Time `json:"last_update"`        // When last updated
	Status            string    `json:"status"`             // "healthy", "degraded", "unhealthy"
	ConsecutiveGood   int       `json:"consecutive_good"`   // Seconds in good state
	ConsecutiveBad    int       `json:"consecutive_bad"`    // Seconds in bad state
	CellChanges       int       `json:"cell_changes"`       // Cell changes in window
	VariancePenalty   float64   `json:"variance_penalty"`   // Signal variance penalty
	ThroughputPenalty float64   `json:"throughput_penalty"` // Throughput penalty
	PredictiveRisk    float64   `json:"predictive_risk"`    // 0-1 risk of failure
}

// NewCellularStabilityCollector creates a new enhanced cellular collector
func NewCellularStabilityCollector(logger *logx.Logger) *CellularStabilityCollector {
	config := &CellularStabilityConfig{
		WindowDurationMinutes:       10,
		SampleIntervalSeconds:       5,
		RSRPHealthyThreshold:        -90.0,
		RSRPUnhealthyThreshold:      -110.0,
		RSRQHealthyThreshold:        -12.0,
		RSRQUnhealthyThreshold:      -16.0,
		SINRHealthyThreshold:        5.0,
		SINRUnhealthyThreshold:      0.0,
		VariancePenaltyThreshold:    6.0,
		CellChangesPenaltyThreshold: 3,
		ThroughputMinKbps:           100.0,
		HysteresisGoodSeconds:       60,
		HysteresisBadSeconds:        30,
	}

	// Calculate ring buffer size based on window and sampling interval
	bufferSize := (config.WindowDurationMinutes * 60) / config.SampleIntervalSeconds

	return &CellularStabilityCollector{
		logger:                logger,
		config:                config,
		ringBuffer:            NewCellularRingBuffer(bufferSize),
		connectivityCollector: NewEnhancedConnectivityCollector(logger),
		stabilityHistory:      make(map[string]*StabilityWindow),
	}
}

// NewCellularRingBuffer creates a new ring buffer
func NewCellularRingBuffer(size int) *CellularRingBuffer {
	return &CellularRingBuffer{
		samples: make([]CellularSample, size),
		size:    size,
	}
}

// Add adds a sample to the ring buffer
func (rb *CellularRingBuffer) Add(sample CellularSample) {
	rb.mu.Lock()
	defer rb.mu.Unlock()

	rb.samples[rb.index] = sample
	rb.index = (rb.index + 1) % rb.size
	if !rb.full && rb.index == 0 {
		rb.full = true
	}
}

// GetLastN returns the last N samples within the specified duration
func (rb *CellularRingBuffer) GetLastN(duration time.Duration) []CellularSample {
	rb.mu.RLock()
	defer rb.mu.RUnlock()

	cutoff := time.Now().Add(-duration)
	var result []CellularSample

	count := rb.size
	if !rb.full {
		count = rb.index
	}

	for i := 0; i < count; i++ {
		idx := (rb.index - 1 - i + rb.size) % rb.size
		sample := rb.samples[idx]
		if sample.Timestamp.After(cutoff) {
			result = append(result, sample)
		} else {
			break // Samples are ordered by time, so we can break
		}
	}

	return result
}

// CollectEnhanced collects enhanced cellular metrics with stability analysis
func (c *CellularStabilityCollector) CollectEnhanced(ctx context.Context, member *pkg.Member) (*pkg.Metrics, *CellularSample, error) {
	c.mu.Lock()
	defer c.mu.Unlock()

	// Get basic cellular sample (signal strength data)
	sample, err := c.collectCellularSample(ctx, member)
	if err != nil {
		return nil, nil, fmt.Errorf("failed to collect cellular sample: %w", err)
	}

	// Get enhanced connectivity data (latency, loss, jitter)
	// Determine if this interface is currently active for adaptive monitoring
	isActive := c.isInterfaceActive(member)
	connectivityMetrics, err := c.connectivityCollector.CollectEnhancedConnectivity(ctx, member, isActive)
	if err != nil {
		c.logger.Debug("Enhanced connectivity collection failed, using basic connectivity",
			"interface", member.Iface, "error", err)
		// Continue with just signal strength data
	}

	// Add to ring buffer
	c.ringBuffer.Add(*sample)

	// Convert to standard metrics format and merge connectivity data
	metrics := c.convertToMetrics(sample)
	if connectivityMetrics != nil {
		c.mergeConnectivityMetrics(metrics, connectivityMetrics)
	}

	// Update stability analysis
	c.updateStabilityAnalysis(member.Iface, sample)

	c.lastMetrics = metrics
	return metrics, sample, nil
}

// collectCellularSample collects a comprehensive cellular signal sample
func (c *CellularStabilityCollector) collectCellularSample(ctx context.Context, member *pkg.Member) (*CellularSample, error) {
	sample := &CellularSample{
		Timestamp: time.Now(),
	}

	// Method 1: Try RUTOS native mobiled ubus service
	if err := c.collectViaMobiled(ctx, member, sample); err == nil {
		c.logger.Debug("Collected cellular sample via mobiled ubus",
			"interface", member.Iface, "rsrp", sample.RSRP, "rsrq", sample.RSRQ, "sinr", sample.SINR)

		// Get throughput data
		c.collectThroughputData(ctx, member, sample)
		return sample, nil
	}

	// Method 2: Try QMI fallback
	if err := c.collectViaQMI(ctx, member, sample); err == nil {
		c.logger.Debug("Collected cellular sample via QMI",
			"interface", member.Iface, "rsrp", sample.RSRP)

		c.collectThroughputData(ctx, member, sample)
		return sample, nil
	}

	// Method 3: Try AT commands fallback
	if err := c.collectViaAT(ctx, member, sample); err == nil {
		c.logger.Debug("Collected cellular sample via AT commands",
			"interface", member.Iface, "rsrp", sample.RSRP)

		c.collectThroughputData(ctx, member, sample)
		return sample, nil
	}

	return nil, fmt.Errorf("all cellular collection methods failed for interface %s", member.Iface)
}

// collectViaMobiled collects data using RUTOS mobiled ubus service
func (c *CellularStabilityCollector) collectViaMobiled(ctx context.Context, member *pkg.Member, sample *CellularSample) error {
	// Try to get signal information
	cmd := exec.CommandContext(ctx, "ubus", "-S", "call", "mobiled", "signal", "{}")
	output, err := cmd.Output()
	if err != nil {
		return fmt.Errorf("mobiled signal call failed: %w", err)
	}

	var signalData struct {
		RSRP        float64 `json:"rsrp"`
		RSRQ        float64 `json:"rsrq"`
		SINR        float64 `json:"sinr"`
		NetworkType string  `json:"network_type"`
		Band        string  `json:"band"`
	}

	if err := json.Unmarshal(output, &signalData); err != nil {
		return fmt.Errorf("failed to parse mobiled signal response: %w", err)
	}

	sample.RSRP = signalData.RSRP
	sample.RSRQ = signalData.RSRQ
	sample.SINR = signalData.SINR
	sample.NetworkType = signalData.NetworkType
	sample.Band = signalData.Band

	// Try to get cell information
	cmd = exec.CommandContext(ctx, "ubus", "-S", "call", "mobiled", "cell_info", "{}")
	if output, err := cmd.Output(); err == nil {
		var cellData struct {
			CellID string `json:"cell_id"`
			TAC    string `json:"tac"`
			PCI    int    `json:"pci"`
			EARFCN int    `json:"earfcn"`
		}
		if json.Unmarshal(output, &cellData) == nil {
			sample.CellID = fmt.Sprintf("%s-%s", cellData.TAC, cellData.CellID)
			sample.PCI = cellData.PCI
			sample.EARFCN = cellData.EARFCN
		}
	}

	return nil
}

// collectViaQMI collects data using QMI interface
func (c *CellularStabilityCollector) collectViaQMI(ctx context.Context, member *pkg.Member, sample *CellularSample) error {
	// Try to find QMI device
	qmiDevice := "/dev/cdc-wdm0" // Default, could be dynamic

	cmd := exec.CommandContext(ctx, "uqmi", "-d", qmiDevice, "--get-signal-info")
	output, err := cmd.Output()
	if err != nil {
		return fmt.Errorf("QMI signal info failed: %w", err)
	}

	// Parse QMI response (format varies by modem)
	lines := strings.Split(string(output), "\n")
	for _, line := range lines {
		if strings.Contains(line, "rsrp") {
			if val, err := c.extractNumericValue(line); err == nil {
				sample.RSRP = val
			}
		} else if strings.Contains(line, "rsrq") {
			if val, err := c.extractNumericValue(line); err == nil {
				sample.RSRQ = val
			}
		} else if strings.Contains(line, "sinr") {
			if val, err := c.extractNumericValue(line); err == nil {
				sample.SINR = val
			}
		}
	}

	return nil
}

// collectViaAT collects data using AT commands
func (c *CellularStabilityCollector) collectViaAT(ctx context.Context, member *pkg.Member, sample *CellularSample) error {
	// Try common AT ports
	atPorts := []string{"/dev/ttyUSB2", "/dev/ttyUSB1", "/dev/ttyUSB0"}

	for _, port := range atPorts {
		if err := c.tryATPort(ctx, port, sample); err == nil {
			return nil
		}
	}

	return fmt.Errorf("no working AT port found")
}

// tryATPort tries to collect data from a specific AT port
func (c *CellularStabilityCollector) tryATPort(ctx context.Context, port string, sample *CellularSample) error {
	// Send AT+QCSQ command for Quectel modems
	cmd := exec.CommandContext(ctx, "sh", "-c",
		fmt.Sprintf(`echo -e 'AT+QCSQ\r' > %s; sleep 1; timeout 2 cat %s`, port, port))
	output, err := cmd.Output()
	if err != nil {
		return fmt.Errorf("AT command failed on %s: %w", port, err)
	}

	// Parse AT response
	response := string(output)
	if strings.Contains(response, "+QCSQ:") {
		return c.parseQCSQResponse(response, sample)
	}

	return fmt.Errorf("no valid AT response from %s", port)
}

// parseQCSQResponse parses Quectel QCSQ response
func (c *CellularStabilityCollector) parseQCSQResponse(response string, sample *CellularSample) error {
	lines := strings.Split(response, "\n")
	for _, line := range lines {
		if strings.Contains(line, "+QCSQ:") {
			// Example: +QCSQ: "LTE",-95,-12,15,-
			parts := strings.Split(line, ",")
			if len(parts) >= 4 {
				if rsrp, err := strconv.ParseFloat(strings.TrimSpace(parts[1]), 64); err == nil {
					sample.RSRP = rsrp
				}
				if rsrq, err := strconv.ParseFloat(strings.TrimSpace(parts[2]), 64); err == nil {
					sample.RSRQ = rsrq
				}
				if sinr, err := strconv.ParseFloat(strings.TrimSpace(parts[3]), 64); err == nil {
					sample.SINR = sinr
				}
				sample.NetworkType = strings.Trim(parts[0], `"`)
			}
			return nil
		}
	}
	return fmt.Errorf("failed to parse QCSQ response")
}

// collectThroughputData collects throughput information
func (c *CellularStabilityCollector) collectThroughputData(ctx context.Context, member *pkg.Member, sample *CellularSample) {
	cmd := exec.CommandContext(ctx, "ubus", "-S", "call",
		fmt.Sprintf("network.interface.%s", member.Iface), "status")
	output, err := cmd.Output()
	if err != nil {
		return
	}

	var ifaceData struct {
		Statistics struct {
			RXBytes uint64 `json:"rx_bytes"`
			TXBytes uint64 `json:"tx_bytes"`
		} `json:"statistics"`
	}

	if json.Unmarshal(output, &ifaceData) == nil {
		// Calculate throughput if we have previous data
		if c.lastMetrics != nil && c.lastMetrics.RXBytes != nil && c.lastMetrics.TXBytes != nil {
			timeDiff := sample.Timestamp.Sub(time.Now().Add(-time.Duration(c.config.SampleIntervalSeconds) * time.Second))
			if timeDiff.Seconds() > 0 {
				bytesDiff := (ifaceData.Statistics.RXBytes + ifaceData.Statistics.TXBytes) -
					(uint64(*c.lastMetrics.RXBytes) + uint64(*c.lastMetrics.TXBytes))
				sample.ThroughputKbps = float64(bytesDiff*8) / (timeDiff.Seconds() * 1000) // Convert to Kbps
			}
		}
	}
}

// convertToMetrics converts cellular sample to standard metrics format
func (c *CellularStabilityCollector) convertToMetrics(sample *CellularSample) *pkg.Metrics {
	rsrp := sample.RSRP
	rsrq := sample.RSRQ
	sinr := sample.SINR

	return &pkg.Metrics{
		RSRP:           &rsrp,
		RSRQ:           &rsrq,
		SINR:           &sinr,
		NetworkType:    &sample.NetworkType,
		Band:           &sample.Band,
		CellID:         &sample.CellID,
		CollectedAt:    sample.Timestamp,
		ThroughputKbps: &sample.ThroughputKbps,
	}
}

// updateStabilityAnalysis updates the stability analysis for an interface
func (c *CellularStabilityCollector) updateStabilityAnalysis(ifname string, sample *CellularSample) {
	window, exists := c.stabilityHistory[ifname]
	if !exists {
		window = &StabilityWindow{
			Status:     "unknown",
			LastUpdate: sample.Timestamp,
		}
		c.stabilityHistory[ifname] = window
	}

	// Get samples from the rolling window
	windowDuration := time.Duration(c.config.WindowDurationMinutes) * time.Minute
	samples := c.ringBuffer.GetLastN(windowDuration)

	if len(samples) < 3 {
		// Not enough data for analysis
		return
	}

	// Calculate stability score
	levelScore := c.calculateLevelScore(samples)
	stabilityScore := c.calculateStabilityScore(samples)

	// Combine scores (60% level, 40% stability)
	finalScore := int(0.6*levelScore + 0.4*stabilityScore)

	window.CurrentScore = finalScore
	window.LastUpdate = sample.Timestamp

	// Update status with hysteresis
	c.updateStatusWithHysteresis(window, finalScore)

	// Calculate predictive risk
	window.PredictiveRisk = c.calculatePredictiveRisk(samples)

	c.logger.Debug("Updated cellular stability analysis",
		"interface", ifname,
		"score", finalScore,
		"status", window.Status,
		"predictive_risk", window.PredictiveRisk,
		"samples_count", len(samples))
}

// calculateLevelScore calculates signal level score (0-100)
func (c *CellularStabilityCollector) calculateLevelScore(samples []CellularSample) float64 {
	if len(samples) == 0 {
		return 0
	}

	var rsrpSum, rsrqSum, sinrSum float64
	count := float64(len(samples))

	for _, sample := range samples {
		// Map RSRP from [-130..-60] to [0..100]
		rsrpScore := c.mapToScore(sample.RSRP, -130, -60)
		rsrpSum += rsrpScore

		// Map RSRQ from [-20..-6] to [0..100]
		rsrqScore := c.mapToScore(sample.RSRQ, -20, -6)
		rsrqSum += rsrqScore

		// Map SINR from [-5..20] to [0..100]
		sinrScore := c.mapToScore(sample.SINR, -5, 20)
		sinrSum += sinrScore
	}

	// Average the three metrics
	return (rsrpSum + rsrqSum + sinrSum) / (3 * count)
}

// calculateStabilityScore calculates signal stability score (0-100)
func (c *CellularStabilityCollector) calculateStabilityScore(samples []CellularSample) float64 {
	if len(samples) < 3 {
		return 0
	}

	// Calculate variance penalty
	variancePenalty := c.calculateVariancePenalty(samples)

	// Calculate cell change penalty
	cellChangePenalty := c.calculateCellChangePenalty(samples)

	// Calculate below-threshold penalty
	belowThresholdPenalty := c.calculateBelowThresholdPenalty(samples)

	// Combine penalties
	totalPenalty := 0.5*variancePenalty + 0.3*cellChangePenalty + 0.2*belowThresholdPenalty

	return math.Max(0, 100*(1-totalPenalty))
}

// calculateVariancePenalty calculates penalty based on signal variance
func (c *CellularStabilityCollector) calculateVariancePenalty(samples []CellularSample) float64 {
	if len(samples) < 2 {
		return 0
	}

	// Calculate RSRP standard deviation
	var sum, sumSq float64
	count := float64(len(samples))

	for _, sample := range samples {
		sum += sample.RSRP
		sumSq += sample.RSRP * sample.RSRP
	}

	mean := sum / count
	variance := (sumSq / count) - (mean * mean)
	stddev := math.Sqrt(variance)

	// Penalty increases as stddev approaches threshold
	return math.Min(1.0, stddev/c.config.VariancePenaltyThreshold)
}

// calculateCellChangePenalty calculates penalty based on cell changes
func (c *CellularStabilityCollector) calculateCellChangePenalty(samples []CellularSample) float64 {
	if len(samples) < 2 {
		return 0
	}

	cellChanges := 0
	lastCellID := samples[0].CellID

	for i := 1; i < len(samples); i++ {
		if samples[i].CellID != "" && samples[i].CellID != lastCellID {
			cellChanges++
			lastCellID = samples[i].CellID
		}
	}

	return math.Min(1.0, float64(cellChanges)/float64(c.config.CellChangesPenaltyThreshold))
}

// calculateBelowThresholdPenalty calculates penalty for time below thresholds
func (c *CellularStabilityCollector) calculateBelowThresholdPenalty(samples []CellularSample) float64 {
	if len(samples) == 0 {
		return 0
	}

	belowCount := 0
	for _, sample := range samples {
		if sample.RSRP < c.config.RSRPUnhealthyThreshold ||
			sample.RSRQ < c.config.RSRQUnhealthyThreshold ||
			sample.SINR < c.config.SINRUnhealthyThreshold {
			belowCount++
		}
	}

	return float64(belowCount) / float64(len(samples))
}

// calculatePredictiveRisk calculates the risk of impending failure (0-1)
func (c *CellularStabilityCollector) calculatePredictiveRisk(samples []CellularSample) float64 {
	if len(samples) < 5 {
		return 0
	}

	// Look for trends in the most recent samples
	recentSamples := samples[len(samples)-5:]

	// Calculate RSRP trend (negative slope indicates degradation)
	rsrpTrend := c.calculateTrend(recentSamples, func(s CellularSample) float64 { return s.RSRP })

	// Calculate variance in recent samples
	recentVariance := c.calculateVariancePenalty(recentSamples)

	// Risk increases with negative trend and high variance
	trendRisk := math.Max(0, -rsrpTrend/10.0) // 10 dBm/sample = high risk
	varianceRisk := recentVariance

	return math.Min(1.0, 0.7*trendRisk+0.3*varianceRisk)
}

// calculateTrend calculates the linear trend of a metric over samples
func (c *CellularStabilityCollector) calculateTrend(samples []CellularSample, extractor func(CellularSample) float64) float64 {
	if len(samples) < 2 {
		return 0
	}

	n := float64(len(samples))
	var sumX, sumY, sumXY, sumX2 float64

	for i, sample := range samples {
		x := float64(i)
		y := extractor(sample)
		sumX += x
		sumY += y
		sumXY += x * y
		sumX2 += x * x
	}

	// Calculate slope using linear regression
	denominator := n*sumX2 - sumX*sumX
	if denominator == 0 {
		return 0
	}

	slope := (n*sumXY - sumX*sumY) / denominator
	return slope
}

// updateStatusWithHysteresis updates status with hysteresis to prevent flapping
func (c *CellularStabilityCollector) updateStatusWithHysteresis(window *StabilityWindow, score int) {
	now := time.Now()
	timeSinceUpdate := int(now.Sub(window.LastUpdate).Seconds())

	if score >= 75 {
		// Good signal
		window.ConsecutiveGood += timeSinceUpdate
		window.ConsecutiveBad = 0

		if window.ConsecutiveGood >= c.config.HysteresisGoodSeconds && window.Status != "healthy" {
			window.Status = "healthy"
			c.logger.Info("Cellular status changed to healthy",
				"score", score, "consecutive_good_seconds", window.ConsecutiveGood)
		}
	} else if score < 50 {
		// Bad signal
		window.ConsecutiveBad += timeSinceUpdate
		window.ConsecutiveGood = 0

		if window.ConsecutiveBad >= c.config.HysteresisBadSeconds && window.Status != "unhealthy" {
			window.Status = "unhealthy"
			c.logger.Warn("Cellular status changed to unhealthy",
				"score", score, "consecutive_bad_seconds", window.ConsecutiveBad)
		}
	} else {
		// Degraded signal
		window.ConsecutiveGood = 0
		window.ConsecutiveBad = 0
		if window.Status == "healthy" || window.Status == "unknown" {
			window.Status = "degraded"
		}
	}
}

// mapToScore maps a value from [min, max] to [0, 100]
func (c *CellularStabilityCollector) mapToScore(value, min, max float64) float64 {
	if value <= min {
		return 0
	}
	if value >= max {
		return 100
	}
	return 100 * (value - min) / (max - min)
}

// extractNumericValue extracts a numeric value from a text line
func (c *CellularStabilityCollector) extractNumericValue(line string) (float64, error) {
	// Look for numbers (including negative) in the line
	parts := strings.Fields(line)
	for _, part := range parts {
		if val, err := strconv.ParseFloat(part, 64); err == nil {
			return val, nil
		}
	}
	return 0, fmt.Errorf("no numeric value found in: %s", line)
}

// GetStabilityStatus returns the current stability status for an interface
func (c *CellularStabilityCollector) GetStabilityStatus(ifname string) *StabilityWindow {
	c.mu.RLock()
	defer c.mu.RUnlock()

	if window, exists := c.stabilityHistory[ifname]; exists {
		return window
	}
	return nil
}

// GetRecentSamples returns recent samples for an interface
func (c *CellularStabilityCollector) GetRecentSamples(duration time.Duration) []CellularSample {
	return c.ringBuffer.GetLastN(duration)
}

// isInterfaceActive determines if an interface is currently the active/primary interface
func (c *CellularStabilityCollector) isInterfaceActive(member *pkg.Member) bool {
	// This is a simplified check - in practice, you'd check against the current active member
	// For now, assume cellular interfaces are active if they have recent good metrics
	if c.lastMetrics != nil && c.lastMetrics.RSRP != nil {
		return *c.lastMetrics.RSRP > -110.0 // Reasonable signal strength
	}
	return false
}

// mergeConnectivityMetrics merges connectivity metrics into the main metrics
func (c *CellularStabilityCollector) mergeConnectivityMetrics(cellular *pkg.Metrics, connectivity *pkg.Metrics) {
	// Merge latency, loss, and jitter from connectivity monitoring
	if connectivity.LatencyMS != nil {
		cellular.LatencyMS = connectivity.LatencyMS
	}
	if connectivity.LossPercent != nil {
		cellular.LossPercent = connectivity.LossPercent
	}
	if connectivity.JitterMS != nil {
		cellular.JitterMS = connectivity.JitterMS
	}
	if connectivity.ProbeMethod != nil {
		cellular.ProbeMethod = connectivity.ProbeMethod
	}
	if connectivity.TargetHost != nil {
		cellular.TargetHost = connectivity.TargetHost
	}
}
