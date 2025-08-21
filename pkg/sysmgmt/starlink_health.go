package sysmgmt

import (
	"context"
	"fmt"
	"net"
	"os/exec"
	"strconv"
	"strings"
	"time"

	"github.com/markus-lassfolk/autonomy/pkg/uci"
)

// StarlinkHealthData represents comprehensive Starlink health information
type StarlinkHealthData struct {
	// Connection status
	APIReachable    bool      `json:"api_reachable"`
	LastAPIResponse time.Time `json:"last_api_response"`
	ResponseTimeMs  float64   `json:"response_time_ms"`

	// Device status
	DeviceID         string `json:"device_id"`
	HardwareVersion  string `json:"hardware_version"`
	SoftwareVersion  string `json:"software_version"`
	UptimeSeconds    int64  `json:"uptime_seconds"`
	BootCount        int    `json:"boot_count"`
	GenerationNumber int32  `json:"generation_number"`

	// Signal quality
	SNR                float64 `json:"snr"`
	SNRAboveFloor      bool    `json:"snr_above_floor"`
	SNRPersistentlyLow bool    `json:"snr_persistently_low"`

	// Network performance
	LatencyMs      float64 `json:"latency_ms"`
	PacketLossRate float64 `json:"packet_loss_rate"`
	DownlinkMbps   float64 `json:"downlink_mbps"`
	UplinkMbps     float64 `json:"uplink_mbps"`

	// Obstruction data
	ObstructionPct   float64 `json:"obstruction_pct"`
	ObstructionValid bool    `json:"obstruction_valid"`

	// GPS status
	GPSValid      bool    `json:"gps_valid"`
	GPSSatellites int     `json:"gps_satellites"`
	Latitude      float64 `json:"latitude,omitempty"`
	Longitude     float64 `json:"longitude,omitempty"`

	// Enhanced Health alerts
	ThermalThrottle    bool `json:"thermal_throttle"`
	ThermalShutdown    bool `json:"thermal_shutdown"`
	Roaming            bool `json:"roaming"`
	UnexpectedLocation bool `json:"unexpected_location"`
	SlowEthernet       bool `json:"slow_ethernet"`
	LowPowerMode       bool `json:"low_power_mode"`
	MastNotVertical    bool `json:"mast_not_vertical"`

	// Motor and mechanical issues
	MotorError         bool `json:"motor_error"`
	MotorStuck         bool `json:"motor_stuck"`
	MotorCalibration   bool `json:"motor_calibration"`
	MotorOverheating   bool `json:"motor_overheating"`
	MotorCommunication bool `json:"motor_communication"`

	// Boot and startup issues
	BootFailure        bool `json:"boot_failure"`
	BootTimeout        bool `json:"boot_timeout"`
	BootCorruption     bool `json:"boot_corruption"`
	FirmwareCorruption bool `json:"firmware_corruption"`

	// Reboot tracking
	RebootCount24h  int        `json:"reboot_count_24h"`
	RebootCount7d   int        `json:"reboot_count_7d"`
	RebootFrequency float64    `json:"reboot_frequency"` // reboots per day
	LastRebootTime  *time.Time `json:"last_reboot_time,omitempty"`
	RebootReason    string     `json:"reboot_reason"`

	// Temperature monitoring
	TemperatureCPU      float64 `json:"temperature_cpu"`
	TemperatureMotor    float64 `json:"temperature_motor"`
	TemperatureAmbient  float64 `json:"temperature_ambient"`
	TemperatureCritical bool    `json:"temperature_critical"`

	// Software status
	SoftwareUpdatePending bool   `json:"software_update_pending"`
	SoftwareUpdateState   string `json:"software_update_state"`
	RebootReady           bool   `json:"reboot_ready"`

	// Mobility
	MobilityClass  string `json:"mobility_class"`
	ClassOfService string `json:"class_of_service"`

	// Ethernet
	EthernetSpeedMbps int `json:"ethernet_speed_mbps"`

	// Hardware self-test results
	SelfTestResults map[string]interface{} `json:"self_test_results"`
}

// StarlinkHealthIssue represents a detected health issue
type StarlinkHealthIssue struct {
	Severity    string    `json:"severity"`    // critical, warning, info
	Category    string    `json:"category"`    // signal, thermal, network, obstruction, gps, software
	Issue       string    `json:"issue"`       // Brief description
	Details     string    `json:"details"`     // Detailed explanation
	Remediation string    `json:"remediation"` // Suggested fix
	Timestamp   time.Time `json:"timestamp"`
}

// Check performs comprehensive Starlink health monitoring
func (shm *StarlinkHealthManager) Check(ctx context.Context) error {
	if !shm.config.StarlinkScriptEnabled {
		shm.logger.Debug("Starlink health monitoring disabled")
		return nil
	}

	shm.logger.Debug("Starting comprehensive Starlink health check")

	// Step 1: Check if we have any Starlink interfaces configured
	starlinkConfig, hasStarlink, err := shm.getStarlinkConfiguration(ctx)
	if err != nil {
		return fmt.Errorf("failed to get Starlink configuration: %w", err)
	}

	if !hasStarlink {
		shm.logger.Debug("No Starlink interfaces detected - skipping health check")
		return nil
	}

	shm.logger.Info("Starlink interface detected - performing health check",
		"host", starlinkConfig.Host, "port", starlinkConfig.Port)

	// Step 2: Test basic connectivity
	if !shm.testBasicConnectivity(starlinkConfig.Host, starlinkConfig.Port) {
		return shm.handleConnectivityFailure(ctx, starlinkConfig)
	}

	// Step 3: Collect comprehensive health data using centralized client
	healthData, err := shm.collectHealthDataCentralized(ctx)
	if err != nil {
		return fmt.Errorf("failed to collect Starlink health data: %w", err)
	}

	// Step 4: Analyze health data for issues
	issues := shm.analyzeHealthData(healthData)

	// Step 5: Take remediation actions
	if len(issues) > 0 {
		return shm.handleHealthIssues(ctx, starlinkConfig, healthData, issues)
	}

	shm.logger.Info("Starlink health check completed - all systems healthy",
		"snr", healthData.SNR,
		"latency_ms", healthData.LatencyMs,
		"obstruction_pct", healthData.ObstructionPct,
		"uptime_hours", float64(healthData.UptimeSeconds)/3600)

	return nil
}

// StarlinkConfig represents Starlink endpoint configuration
type StarlinkConfig struct {
	Host string
	Port int
}

// getStarlinkConfiguration determines if we have Starlink and gets its configuration
func (shm *StarlinkHealthManager) getStarlinkConfiguration(ctx context.Context) (*StarlinkConfig, bool, error) {
	// Method 1: Check daemon's UCI configuration for Starlink API settings
	uciClient := uci.NewUCI(shm.logger)
	config, err := uciClient.LoadConfig(ctx)
	if err == nil && config.StarlinkAPIHost != "" {
		shm.logger.Debug("Found Starlink config in daemon UCI",
			"host", config.StarlinkAPIHost, "port", config.StarlinkAPIPort)
		return &StarlinkConfig{
			Host: config.StarlinkAPIHost,
			Port: config.StarlinkAPIPort,
		}, true, nil
	}

	// Method 2: Check if we can discover Starlink by testing common endpoints
	commonConfigs := []*StarlinkConfig{
		{Host: "192.168.100.1", Port: 9200}, // Standard Starlink
		{Host: "192.168.1.1", Port: 9200},   // Alternative setup
	}

	for _, cfg := range commonConfigs {
		if shm.testBasicConnectivity(cfg.Host, cfg.Port) {
			shm.logger.Info("Discovered Starlink dish", "host", cfg.Host, "port", cfg.Port)
			return cfg, true, nil
		}
	}

	// Method 3: Check if any discovered members are classified as Starlink
	// This would require access to the discovery system, but for now we'll skip
	// as the daemon should have the config if Starlink is being monitored

	return nil, false, nil
}

// testBasicConnectivity tests if we can connect to the Starlink gRPC endpoint
func (shm *StarlinkHealthManager) testBasicConnectivity(host string, port int) bool {
	conn, err := net.DialTimeout("tcp", fmt.Sprintf("%s:%d", host, port), 5*time.Second)
	if err != nil {
		shm.logger.Debug("Starlink connectivity test failed", "host", host, "port", port, "error", err)
		return false
	}
	defer conn.Close()

	shm.logger.Debug("Starlink connectivity test passed", "host", host, "port", port)
	return true
}

// collectHealthDataCentralized collects comprehensive health data using centralized Starlink client
func (shm *StarlinkHealthManager) collectHealthDataCentralized(ctx context.Context) (*StarlinkHealthData, error) {
	startTime := time.Now()
	healthData := &StarlinkHealthData{
		LastAPIResponse: time.Now(),
	}

	// Use centralized Starlink client to get comprehensive health data
	starlinkHealth, err := shm.starlinkClient.GetHealthData(ctx)
	if err != nil {
		return nil, fmt.Errorf("failed to get Starlink health data: %w", err)
	}

	// Convert centralized health data to our format
	if starlinkHealth.Status != nil {
		status := starlinkHealth.Status.DishGetStatus

		// Device state
		healthData.UptimeSeconds = parseStringToInt64(status.DeviceState.UptimeS)

		// Device info
		healthData.DeviceID = status.DeviceInfo.ID
		healthData.HardwareVersion = status.DeviceInfo.HardwareVersion
		healthData.SoftwareVersion = status.DeviceInfo.SoftwareVersion

		// Signal quality
		healthData.SNR = status.SNR
		healthData.SNRAboveFloor = status.IsSnrAboveNoiseFloor
		healthData.SNRPersistentlyLow = status.IsSnrPersistentlyLow

		// Network performance
		healthData.LatencyMs = status.PopPingLatencyMs
		healthData.PacketLossRate = status.PopPingDropRate
		healthData.DownlinkMbps = status.DownlinkThroughputBps / 1000000
		healthData.UplinkMbps = status.UplinkThroughputBps / 1000000

		// Obstruction
		healthData.ObstructionPct = status.ObstructionStats.FractionObstructed * 100
		healthData.ObstructionValid = status.ObstructionStats.ValidS > 0

		// GPS
		healthData.GPSValid = status.GPSStats.GPSValid
		healthData.GPSSatellites = status.GPSStats.GPSSats

		// Ethernet
		healthData.EthernetSpeedMbps = int(status.EthSpeedMbps)

		// Mobility and service
		healthData.MobilityClass = status.MobilityClass
		healthData.ClassOfService = status.ClassOfService

		// Software update
		healthData.SoftwareUpdateState = status.SoftwareUpdateState
		healthData.SoftwareUpdatePending = status.SoftwareUpdateState != ""
		healthData.RebootReady = status.SwupdateRebootReady
	}

	// Add diagnostics data if available
	if starlinkHealth.Diagnostics != nil {
		diagnostics := starlinkHealth.Diagnostics.DishGetDiagnostics

		// Thermal status from diagnostics
		healthData.ThermalThrottle = diagnostics.ThermalThrottle
		healthData.ThermalShutdown = diagnostics.ThermalShutdown
	}

	// Get location data if available
	locationData, err := shm.starlinkClient.GetLocation(ctx)
	if err == nil {
		healthData.Latitude = locationData.Latitude
		healthData.Longitude = locationData.Longitude
	}

	healthData.APIReachable = true
	healthData.ResponseTimeMs = time.Since(startTime).Seconds() * 1000

	shm.logger.Info("Starlink health data collected using centralized client",
		"response_time_ms", healthData.ResponseTimeMs,
		"snr", healthData.SNR,
		"latency_ms", healthData.LatencyMs,
		"obstruction_pct", healthData.ObstructionPct)

	return healthData, nil
}

// parseStatusData parses get_status API response
func (shm *StarlinkHealthManager) parseStatusData(health *StarlinkHealthData, response map[string]interface{}) error {
	status, ok := response["dishGetStatus"].(map[string]interface{})
	if !ok {
		return fmt.Errorf("invalid status response format")
	}

	// Device state
	if deviceState, ok := status["deviceState"].(map[string]interface{}); ok {
		if uptime, ok := deviceState["uptimeS"].(float64); ok {
			health.UptimeSeconds = int64(uptime)
		}
	}

	// Signal quality
	if snr, ok := status["snr"].(float64); ok {
		health.SNR = snr
	}
	if snrAbove, ok := status["isSnrAboveNoiseFloor"].(bool); ok {
		health.SNRAboveFloor = snrAbove
	}
	if snrLow, ok := status["isSnrPersistentlyLow"].(bool); ok {
		health.SNRPersistentlyLow = snrLow
	}

	// Network performance
	if latency, ok := status["popPingLatencyMs"].(float64); ok {
		health.LatencyMs = latency
	}
	if loss, ok := status["popPingDropRate"].(float64); ok {
		health.PacketLossRate = loss
	}
	if downlink, ok := status["downlinkThroughputBps"].(float64); ok {
		health.DownlinkMbps = downlink / 1000000 // Convert to Mbps
	}
	if uplink, ok := status["uplinkThroughputBps"].(float64); ok {
		health.UplinkMbps = uplink / 1000000 // Convert to Mbps
	}

	// Obstruction
	if obstructionStats, ok := status["obstructionStats"].(map[string]interface{}); ok {
		if fraction, ok := obstructionStats["fractionObstructed"].(float64); ok {
			health.ObstructionPct = fraction * 100 // Convert to percentage
		}
		if valid, ok := obstructionStats["validS"].(float64); ok {
			health.ObstructionValid = valid > 0
		}
	}

	// GPS
	if gpsStats, ok := status["gpsStats"].(map[string]interface{}); ok {
		if valid, ok := gpsStats["gpsValid"].(bool); ok {
			health.GPSValid = valid
		}
		if sats, ok := gpsStats["gpsSats"].(float64); ok {
			health.GPSSatellites = int(sats)
		}
	}

	// Ethernet
	if ethSpeed, ok := status["ethSpeedMbps"].(float64); ok {
		health.EthernetSpeedMbps = int(ethSpeed)
	}

	// Mobility and service
	if mobility, ok := status["mobilityClass"].(string); ok {
		health.MobilityClass = mobility
	}
	if cos, ok := status["classOfService"].(string); ok {
		health.ClassOfService = cos
	}

	// Software update
	if updateState, ok := status["softwareUpdateState"].(string); ok {
		health.SoftwareUpdateState = updateState
		health.SoftwareUpdatePending = updateState != ""
	}
	if rebootReady, ok := status["swupdateRebootReady"].(bool); ok {
		health.RebootReady = rebootReady
	}

	return nil
}

// analyzeHealthData analyzes collected health data for issues
func (shm *StarlinkHealthManager) analyzeHealthData(health *StarlinkHealthData) []StarlinkHealthIssue {
	var issues []StarlinkHealthIssue
	now := time.Now()

	// Critical issues
	if health.ThermalShutdown {
		issues = append(issues, StarlinkHealthIssue{
			Severity:    "critical",
			Category:    "thermal",
			Issue:       "Thermal shutdown active",
			Details:     "Starlink dish has shut down due to overheating",
			Remediation: "Check dish ventilation, clean debris, ensure proper mounting",
			Timestamp:   now,
		})
	}

	if health.SNRPersistentlyLow {
		issues = append(issues, StarlinkHealthIssue{
			Severity:    "critical",
			Category:    "signal",
			Issue:       "Signal persistently low",
			Details:     fmt.Sprintf("SNR is persistently low (current: %.1f dB)", health.SNR),
			Remediation: "Check dish alignment, clear obstructions, verify mounting stability",
			Timestamp:   now,
		})
	}

	// Warning issues
	if health.ThermalThrottle {
		issues = append(issues, StarlinkHealthIssue{
			Severity:    "warning",
			Category:    "thermal",
			Issue:       "Thermal throttling active",
			Details:     "Dish is reducing performance due to high temperature",
			Remediation: "Improve ventilation, check for debris blocking airflow",
			Timestamp:   now,
		})
	}

	if health.ObstructionPct > 1.0 { // More than 1% obstruction
		issues = append(issues, StarlinkHealthIssue{
			Severity:    "warning",
			Category:    "obstruction",
			Issue:       "Sky view obstruction detected",
			Details:     fmt.Sprintf("%.1f%% of sky view is obstructed", health.ObstructionPct),
			Remediation: "Clear obstructions (trees, buildings) or relocate dish",
			Timestamp:   now,
		})
	}

	if health.LatencyMs > 150 {
		issues = append(issues, StarlinkHealthIssue{
			Severity:    "warning",
			Category:    "network",
			Issue:       "High latency detected",
			Details:     fmt.Sprintf("Current latency: %.1f ms (threshold: 150ms)", health.LatencyMs),
			Remediation: "Check for network congestion, verify dish alignment",
			Timestamp:   now,
		})
	}

	if health.PacketLossRate > 0.05 { // More than 5% packet loss
		issues = append(issues, StarlinkHealthIssue{
			Severity:    "warning",
			Category:    "network",
			Issue:       "High packet loss detected",
			Details:     fmt.Sprintf("Current packet loss: %.1f%% (threshold: 5%%)", health.PacketLossRate*100),
			Remediation: "Check signal quality, verify dish stability",
			Timestamp:   now,
		})
	}

	if health.SNR < 8.0 && health.SNR > 0 {
		issues = append(issues, StarlinkHealthIssue{
			Severity:    "warning",
			Category:    "signal",
			Issue:       "Low signal quality",
			Details:     fmt.Sprintf("SNR is low: %.1f dB (good: >10 dB)", health.SNR),
			Remediation: "Check dish alignment and clear any obstructions",
			Timestamp:   now,
		})
	}

	// Info issues
	if !health.GPSValid {
		issues = append(issues, StarlinkHealthIssue{
			Severity:    "info",
			Category:    "gps",
			Issue:       "GPS fix not available",
			Details:     fmt.Sprintf("GPS satellites: %d", health.GPSSatellites),
			Remediation: "Ensure clear sky view for GPS reception",
			Timestamp:   now,
		})
	}

	if health.SlowEthernet {
		issues = append(issues, StarlinkHealthIssue{
			Severity:    "info",
			Category:    "network",
			Issue:       "Slow Ethernet speeds detected",
			Details:     fmt.Sprintf("Ethernet speed: %d Mbps", health.EthernetSpeedMbps),
			Remediation: "Check Ethernet cable and port configuration",
			Timestamp:   now,
		})
	}

	if health.RebootReady {
		issues = append(issues, StarlinkHealthIssue{
			Severity:    "info",
			Category:    "software",
			Issue:       "Software update reboot pending",
			Details:     fmt.Sprintf("Update state: %s", health.SoftwareUpdateState),
			Remediation: "Schedule maintenance window for reboot",
			Timestamp:   now,
		})
	}

	return issues
}

// handleConnectivityFailure handles cases where Starlink API is not reachable
func (shm *StarlinkHealthManager) handleConnectivityFailure(ctx context.Context, config *StarlinkConfig) error {
	shm.logger.Error("Starlink API not reachable", "host", config.Host, "port", config.Port)

	if shm.dryRun {
		shm.logger.Info("DRY RUN: Would attempt Starlink connectivity remediation")
		return nil
	}

	// Note: grpcurl no longer needed - using centralized Starlink client

	// Try to restart network interface that might be connected to Starlink
	if err := shm.restartStarlinkInterface(ctx); err != nil {
		shm.logger.Error("Failed to restart Starlink interface", "error", err)
	}

	// Send notification about connectivity failure
	shm.sendConnectivityNotification(config)

	return fmt.Errorf("Starlink API connectivity failed - remediation attempted")
}

// handleHealthIssues handles detected health issues
func (shm *StarlinkHealthManager) handleHealthIssues(ctx context.Context, config *StarlinkConfig, health *StarlinkHealthData, issues []StarlinkHealthIssue) error {
	shm.logger.Warn("Starlink health issues detected", "issue_count", len(issues))

	// Log all issues
	for _, issue := range issues {
		shm.logger.Warn("Starlink health issue",
			"severity", issue.Severity,
			"category", issue.Category,
			"issue", issue.Issue,
			"details", issue.Details)
	}

	// Take remediation actions for critical issues
	criticalIssues := 0
	for _, issue := range issues {
		if issue.Severity == "critical" {
			criticalIssues++
			if err := shm.handleCriticalIssue(ctx, issue, health); err != nil {
				shm.logger.Error("Failed to handle critical Starlink issue", "issue", issue.Issue, "error", err)
			}
		}
	}

	// Send notification about health issues
	shm.sendHealthNotification(health, issues)

	if criticalIssues > 0 {
		return fmt.Errorf("critical Starlink health issues detected: %d", criticalIssues)
	}

	return nil
}

// handleCriticalIssue handles a specific critical issue
func (shm *StarlinkHealthManager) handleCriticalIssue(ctx context.Context, issue StarlinkHealthIssue, health *StarlinkHealthData) error {
	if shm.dryRun {
		shm.logger.Info("DRY RUN: Would handle critical Starlink issue", "issue", issue.Issue)
		return nil
	}

	switch issue.Category {
	case "thermal":
		// For thermal issues, we can't do much automatically
		shm.logger.Info("Critical thermal issue detected - manual intervention required", "issue", issue.Issue)

	case "signal":
		// For signal issues, try restarting the interface
		if err := shm.restartStarlinkInterface(ctx); err != nil {
			return fmt.Errorf("failed to restart Starlink interface: %w", err)
		}

	default:
		shm.logger.Info("Critical issue detected - no automatic remediation available", "category", issue.Category)
	}

	return nil
}

// restartStarlinkInterface attempts to restart the network interface connected to Starlink
func (shm *StarlinkHealthManager) restartStarlinkInterface(ctx context.Context) error {
	// Try to find the interface that can reach Starlink
	interfaces := []string{"wan", "eth1", "eth0"}

	for _, iface := range interfaces {
		// Check if this interface exists and is up
		cmd := exec.Command("ip", "link", "show", iface)
		if err := cmd.Run(); err != nil {
			continue // Interface doesn't exist
		}

		shm.logger.Info("Restarting network interface", "interface", iface)

		// Bring interface down and up
		if err := exec.Command("ifdown", iface).Run(); err != nil {
			shm.logger.Debug("Failed to bring interface down", "interface", iface, "error", err)
		}
		time.Sleep(2 * time.Second)
		if err := exec.Command("ifup", iface).Run(); err != nil {
			shm.logger.Debug("Failed to bring interface up", "interface", iface, "error", err)
		}

		// Wait a moment for interface to come up
		time.Sleep(5 * time.Second)

		// Test if Starlink is reachable now
		if shm.testBasicConnectivity("192.168.100.1", 9200) {
			shm.logger.Info("Starlink connectivity restored", "interface", iface)
			return nil
		}
	}

	return fmt.Errorf("failed to restore Starlink connectivity")
}

// sendConnectivityNotification sends notification about connectivity issues
func (shm *StarlinkHealthManager) sendConnectivityNotification(config *StarlinkConfig) {
	// This would integrate with the notification system
	shm.logger.Warn("Starlink connectivity notification",
		"title", "🛰️ Starlink API Unreachable",
		"message", fmt.Sprintf("Cannot connect to Starlink dish at %s:%d", config.Host, config.Port))
}

// sendHealthNotification sends notification about health issues
func (shm *StarlinkHealthManager) sendHealthNotification(health *StarlinkHealthData, issues []StarlinkHealthIssue) {
	criticalCount := 0
	warningCount := 0

	for _, issue := range issues {
		switch issue.Severity {
		case "critical":
			criticalCount++
		case "warning":
			warningCount++
		}
	}

	title := "🛰️ Starlink Health Alert"
	if criticalCount > 0 {
		title = "🚨 Critical Starlink Issues"
	}

	message := "Health Check Results:\n"
	message += fmt.Sprintf("• SNR: %.1f dB\n", health.SNR)
	message += fmt.Sprintf("• Latency: %.1f ms\n", health.LatencyMs)
	message += fmt.Sprintf("• Obstruction: %.1f%%\n", health.ObstructionPct)
	message += fmt.Sprintf("• Issues: %d critical, %d warning", criticalCount, warningCount)

	shm.logger.Warn("Starlink health notification", "title", title, "message", message)
}

// detectMotorIssues detects motor-related problems
func (shm *StarlinkHealthManager) detectMotorIssues(health *StarlinkHealthData) []StarlinkHealthIssue {
	var issues []StarlinkHealthIssue

	// Check for motor errors in self-test results
	if selfTestResults, ok := health.SelfTestResults["motor"]; ok {
		if motorMap, ok := selfTestResults.(map[string]interface{}); ok {
			if status, ok := motorMap["status"].(string); ok && status != "pass" {
				issues = append(issues, StarlinkHealthIssue{
					Severity:    "critical",
					Category:    "motor",
					Issue:       "Motor self-test failed",
					Details:     fmt.Sprintf("Motor self-test status: %s", status),
					Remediation: "Check motor connections and power supply. Contact support if persistent.",
					Timestamp:   time.Now(),
				})
				health.MotorError = true
			}
		}
	}

	// Check for motor overheating
	if health.TemperatureMotor > 80.0 {
		issues = append(issues, StarlinkHealthIssue{
			Severity:    "warning",
			Category:    "motor",
			Issue:       "Motor overheating detected",
			Details:     fmt.Sprintf("Motor temperature: %.1f°C", health.TemperatureMotor),
			Remediation: "Check motor ventilation and ambient temperature. Reduce load if necessary.",
			Timestamp:   time.Now(),
		})
		health.MotorOverheating = true
	}

	// Check for motor communication issues
	if health.UptimeSeconds < 300 && health.BootCount > 0 { // Recent boot
		issues = append(issues, StarlinkHealthIssue{
			Severity:    "warning",
			Category:    "motor",
			Issue:       "Potential motor communication issue",
			Details:     "Recent reboot detected - may indicate motor communication problems",
			Remediation: "Monitor for repeated reboots. Check motor cable connections.",
			Timestamp:   time.Now(),
		})
		health.MotorCommunication = true
	}

	return issues
}

// detectBootIssues detects boot and startup problems
func (shm *StarlinkHealthManager) detectBootIssues(health *StarlinkHealthData) []StarlinkHealthIssue {
	var issues []StarlinkHealthIssue

	// Check for boot failures
	if health.BootCount > 0 && health.UptimeSeconds < 60 {
		issues = append(issues, StarlinkHealthIssue{
			Severity:    "critical",
			Category:    "boot",
			Issue:       "Boot failure detected",
			Details:     fmt.Sprintf("Device booted but uptime only %d seconds", health.UptimeSeconds),
			Remediation: "Check power supply and hardware connections. Contact support if persistent.",
			Timestamp:   time.Now(),
		})
		health.BootFailure = true
	}

	// Check for boot timeouts
	if health.UptimeSeconds < 300 { // Less than 5 minutes uptime
		issues = append(issues, StarlinkHealthIssue{
			Severity:    "warning",
			Category:    "boot",
			Issue:       "Boot timeout detected",
			Details:     fmt.Sprintf("Device uptime only %d seconds - may indicate boot issues", health.UptimeSeconds),
			Remediation: "Monitor boot process. Check for firmware corruption.",
			Timestamp:   time.Now(),
		})
		health.BootTimeout = true
	}

	// Check for firmware corruption
	if health.SoftwareVersion == "" || health.HardwareVersion == "" {
		issues = append(issues, StarlinkHealthIssue{
			Severity:    "critical",
			Category:    "boot",
			Issue:       "Firmware corruption suspected",
			Details:     "Missing version information - may indicate firmware corruption",
			Remediation: "Contact support for firmware recovery procedures.",
			Timestamp:   time.Now(),
		})
		health.FirmwareCorruption = true
	}

	return issues
}

// detectRebootPatterns detects excessive reboots and patterns
func (shm *StarlinkHealthManager) detectRebootPatterns(health *StarlinkHealthData) []StarlinkHealthIssue {
	var issues []StarlinkHealthIssue

	// Check for excessive reboots in 24 hours
	if health.RebootCount24h > 3 {
		issues = append(issues, StarlinkHealthIssue{
			Severity:    "critical",
			Category:    "reboot",
			Issue:       "Excessive reboots detected",
			Details:     fmt.Sprintf("%d reboots in 24 hours (%.1f per day)", health.RebootCount24h, health.RebootFrequency),
			Remediation: "Check power supply, temperature, and hardware connections. Contact support if persistent.",
			Timestamp:   time.Now(),
		})
	}

	// Check for high reboot frequency
	if health.RebootFrequency > 2.0 { // More than 2 reboots per day average
		issues = append(issues, StarlinkHealthIssue{
			Severity:    "warning",
			Category:    "reboot",
			Issue:       "High reboot frequency",
			Details:     fmt.Sprintf("Average %.1f reboots per day over 7 days", health.RebootFrequency),
			Remediation: "Monitor system stability. Check for environmental issues.",
			Timestamp:   time.Now(),
		})
	}

	// Check for specific reboot reasons
	if health.RebootReason != "" {
		severity := "info"
		if strings.Contains(strings.ToLower(health.RebootReason), "thermal") ||
			strings.Contains(strings.ToLower(health.RebootReason), "panic") ||
			strings.Contains(strings.ToLower(health.RebootReason), "error") {
			severity = "warning"
		}

		issues = append(issues, StarlinkHealthIssue{
			Severity:    severity,
			Category:    "reboot",
			Issue:       "Reboot reason detected",
			Details:     fmt.Sprintf("Last reboot reason: %s", health.RebootReason),
			Remediation: "Monitor for pattern in reboot reasons.",
			Timestamp:   time.Now(),
		})
	}

	return issues
}

// detectTemperatureIssues detects temperature-related problems
func (shm *StarlinkHealthManager) detectTemperatureIssues(health *StarlinkHealthData) []StarlinkHealthIssue {
	var issues []StarlinkHealthIssue

	// Check CPU temperature
	if health.TemperatureCPU > 85.0 {
		issues = append(issues, StarlinkHealthIssue{
			Severity:    "critical",
			Category:    "thermal",
			Issue:       "CPU overheating",
			Details:     fmt.Sprintf("CPU temperature: %.1f°C", health.TemperatureCPU),
			Remediation: "Check ventilation and ambient temperature. Reduce load if necessary.",
			Timestamp:   time.Now(),
		})
		health.TemperatureCritical = true
	} else if health.TemperatureCPU > 75.0 {
		issues = append(issues, StarlinkHealthIssue{
			Severity:    "warning",
			Category:    "thermal",
			Issue:       "High CPU temperature",
			Details:     fmt.Sprintf("CPU temperature: %.1f°C", health.TemperatureCPU),
			Remediation: "Monitor temperature trend. Check ventilation.",
			Timestamp:   time.Now(),
		})
	}

	// Check ambient temperature
	if health.TemperatureAmbient > 50.0 {
		issues = append(issues, StarlinkHealthIssue{
			Severity:    "warning",
			Category:    "thermal",
			Issue:       "High ambient temperature",
			Details:     fmt.Sprintf("Ambient temperature: %.1f°C", health.TemperatureAmbient),
			Remediation: "Improve ventilation and reduce ambient temperature.",
			Timestamp:   time.Now(),
		})
	}

	// Check for thermal throttling
	if health.ThermalThrottle {
		issues = append(issues, StarlinkHealthIssue{
			Severity:    "warning",
			Category:    "thermal",
			Issue:       "Thermal throttling active",
			Details:     "System is throttling performance due to high temperature",
			Remediation: "Check ventilation and reduce ambient temperature.",
			Timestamp:   time.Now(),
		})
	}

	return issues
}

// performEnhancedHealthCheck performs comprehensive health monitoring
func (shm *StarlinkHealthManager) performEnhancedHealthCheck(ctx context.Context, health *StarlinkHealthData) []StarlinkHealthIssue {
	var allIssues []StarlinkHealthIssue

	// Detect motor issues
	motorIssues := shm.detectMotorIssues(health)
	allIssues = append(allIssues, motorIssues...)

	// Detect boot issues
	bootIssues := shm.detectBootIssues(health)
	allIssues = append(allIssues, bootIssues...)

	// Detect reboot patterns
	rebootIssues := shm.detectRebootPatterns(health)
	allIssues = append(allIssues, rebootIssues...)

	// Detect temperature issues
	tempIssues := shm.detectTemperatureIssues(health)
	allIssues = append(allIssues, tempIssues...)

	// Log comprehensive health status
	shm.logger.Info("Enhanced Starlink health check completed",
		"total_issues", len(allIssues),
		"motor_issues", len(motorIssues),
		"boot_issues", len(bootIssues),
		"reboot_issues", len(rebootIssues),
		"temperature_issues", len(tempIssues),
		"cpu_temp", health.TemperatureCPU,
		"motor_temp", health.TemperatureMotor,
		"reboot_count_24h", health.RebootCount24h,
		"reboot_frequency", health.RebootFrequency)

	return allIssues
}

// parseStringToInt64 converts a string to int64, returns 0 if parsing fails
func parseStringToInt64(s string) int64 {
	if val, err := strconv.ParseInt(s, 10, 64); err == nil {
		return val
	}
	return 0
}
