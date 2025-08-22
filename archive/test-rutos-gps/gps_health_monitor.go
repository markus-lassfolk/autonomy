package main

import (
	"fmt"
	"strconv"
	"strings"
	"time"

	"golang.org/x/crypto/ssh"
)

// GPSHealthStatus represents the health status of the GPS system
type GPSHealthStatus struct {
	Healthy             bool          `json:"healthy"`
	LastSuccessfulFix   time.Time     `json:"last_successful_fix"`
	ConsecutiveFailures int           `json:"consecutive_failures"`
	TotalResets         int           `json:"total_resets"`
	LastResetTime       time.Time     `json:"last_reset_time"`
	LastResetReason     string        `json:"last_reset_reason"`
	CurrentAccuracy     float64       `json:"current_accuracy"`
	CurrentSatellites   int           `json:"current_satellites"`
	CurrentHDOP         float64       `json:"current_hdop"`
	CurrentFixType      int           `json:"current_fix_type"`
	GPSSessionActive    bool          `json:"gps_session_active"`
	GPSDaemonRunning    bool          `json:"gpsd_daemon_running"`
	LastHealthCheck     time.Time     `json:"last_health_check"`
	HealthCheckInterval time.Duration `json:"health_check_interval"`
	Issues              []string      `json:"issues"`
}

// GPSHealthConfig holds configuration for GPS health monitoring
type GPSHealthConfig struct {
	HealthCheckInterval    time.Duration `uci:"autonomy.gps.health_check_interval" default:"300s"` // 5 minutes
	MaxConsecutiveFailures int           `uci:"autonomy.gps.max_consecutive_failures" default:"3"` // 3 failures before reset
	MinAccuracy            float64       `uci:"autonomy.gps.min_accuracy" default:"10.0"`          // 10m minimum accuracy
	MinSatellites          int           `uci:"autonomy.gps.min_satellites" default:"4"`           // 4 satellites minimum
	MaxHDOP                float64       `uci:"autonomy.gps.max_hdop" default:"5.0"`               // HDOP threshold
	ResetCooldown          time.Duration `uci:"autonomy.gps.reset_cooldown" default:"600s"`        // 10 minutes between resets
	EnableAutoReset        bool          `uci:"autonomy.gps.enable_auto_reset" default:"true"`     // Enable automatic GPS reset
	NotifyOnReset          bool          `uci:"autonomy.gps.notify_on_reset" default:"true"`       // Send notifications on reset
}

// GPSHealthMonitor manages GPS health monitoring and recovery
type GPSHealthMonitor struct {
	config    *GPSHealthConfig
	status    *GPSHealthStatus
	sshClient *ssh.Client
}

// NewGPSHealthMonitor creates a new GPS health monitor
func NewGPSHealthMonitor(config *GPSHealthConfig, sshClient *ssh.Client) *GPSHealthMonitor {
	return &GPSHealthMonitor{
		config:    config,
		sshClient: sshClient,
		status: &GPSHealthStatus{
			Healthy:             true,
			LastSuccessfulFix:   time.Now(),
			ConsecutiveFailures: 0,
			TotalResets:         0,
			HealthCheckInterval: config.HealthCheckInterval,
			Issues:              []string{},
		},
	}
}

// CheckGPSHealth performs a comprehensive GPS health check
func (ghm *GPSHealthMonitor) CheckGPSHealth() (*GPSHealthStatus, error) {
	fmt.Println("🔍 GPS Health Check Starting...")
	ghm.status.LastHealthCheck = time.Now()
	ghm.status.Issues = []string{} // Clear previous issues

	// Check 1: Verify GPS session is active
	sessionActive, err := ghm.checkGPSSession()
	if err != nil {
		ghm.status.Issues = append(ghm.status.Issues, fmt.Sprintf("GPS session check failed: %v", err))
	}
	ghm.status.GPSSessionActive = sessionActive

	// Check 2: Verify gpsd daemon is running
	daemonRunning, err := ghm.checkGPSDaemon()
	if err != nil {
		ghm.status.Issues = append(ghm.status.Issues, fmt.Sprintf("GPSD daemon check failed: %v", err))
	}
	ghm.status.GPSDaemonRunning = daemonRunning

	// Check 3: Test GPS data quality
	gpsHealthy, err := ghm.checkGPSDataQuality()
	if err != nil {
		ghm.status.Issues = append(ghm.status.Issues, fmt.Sprintf("GPS data quality check failed: %v", err))
		ghm.status.ConsecutiveFailures++
	} else if gpsHealthy {
		ghm.status.LastSuccessfulFix = time.Now()
		ghm.status.ConsecutiveFailures = 0
	} else {
		ghm.status.ConsecutiveFailures++
	}

	// Determine overall health
	ghm.status.Healthy = sessionActive && daemonRunning && gpsHealthy && len(ghm.status.Issues) == 0

	// Check if reset is needed
	if ghm.shouldResetGPS() {
		if ghm.config.EnableAutoReset {
			fmt.Println("⚠️  GPS health degraded, attempting automatic reset...")
			resetReason := ghm.determineResetReason()
			err := ghm.ResetGPS(resetReason)
			if err != nil {
				ghm.status.Issues = append(ghm.status.Issues, fmt.Sprintf("GPS reset failed: %v", err))
			}
		} else {
			ghm.status.Issues = append(ghm.status.Issues, "GPS reset needed but auto-reset disabled")
		}
	}

	ghm.displayHealthStatus()
	return ghm.status, nil
}

// checkGPSSession verifies that GPS session is active
func (ghm *GPSHealthMonitor) checkGPSSession() (bool, error) {
	fmt.Println("  📡 Checking GPS session status...")

	// Check GPS status via AT command
	output, err := executeCommand(ghm.sshClient, "gsmctl -A 'AT+QGPS?'")
	if err != nil {
		return false, fmt.Errorf("failed to check GPS status: %v", err)
	}

	// Expected response: +QGPS: 1 (GPS enabled)
	if strings.Contains(output, "+QGPS: 1") {
		fmt.Println("    ✅ GPS session is active")
		return true, nil
	} else if strings.Contains(output, "+QGPS: 0") {
		fmt.Println("    ❌ GPS session is inactive")
		return false, nil
	}

	return false, fmt.Errorf("unexpected GPS status response: %s", output)
}

// checkGPSDaemon verifies that gpsd daemon is running
func (ghm *GPSHealthMonitor) checkGPSDaemon() (bool, error) {
	fmt.Println("  🔧 Checking GPSD daemon status...")

	output, err := executeCommand(ghm.sshClient, "ps | grep gpsd | grep -v grep")
	if err != nil {
		return false, fmt.Errorf("failed to check gpsd process: %v", err)
	}

	if strings.Contains(output, "/usr/sbin/gpsd") {
		fmt.Println("    ✅ GPSD daemon is running")
		return true, nil
	}

	fmt.Println("    ❌ GPSD daemon is not running")
	return false, nil
}

// checkGPSDataQuality verifies GPS data quality meets thresholds
func (ghm *GPSHealthMonitor) checkGPSDataQuality() (bool, error) {
	fmt.Println("  📊 Checking GPS data quality...")

	// Get GPS data from AT command
	gpsData, err := ghm.getGPSData()
	if err != nil {
		return false, fmt.Errorf("failed to get GPS data: %v", err)
	}

	// Update current status
	ghm.status.CurrentAccuracy = gpsData.Accuracy
	ghm.status.CurrentSatellites = gpsData.Satellites
	ghm.status.CurrentHDOP = gpsData.HDOP
	ghm.status.CurrentFixType = gpsData.FixType

	issues := []string{}

	// Check fix type
	if gpsData.FixType < 1 {
		issues = append(issues, "No GPS fix")
	}

	// Check accuracy (if available from gpsctl)
	if gpsData.Accuracy > ghm.config.MinAccuracy {
		issues = append(issues, fmt.Sprintf("Poor accuracy: %.1fm (threshold: %.1fm)",
			gpsData.Accuracy, ghm.config.MinAccuracy))
	}

	// Check satellite count
	if gpsData.Satellites < ghm.config.MinSatellites {
		issues = append(issues, fmt.Sprintf("Low satellite count: %d (minimum: %d)",
			gpsData.Satellites, ghm.config.MinSatellites))
	}

	// Check HDOP
	if gpsData.HDOP > ghm.config.MaxHDOP {
		issues = append(issues, fmt.Sprintf("Poor HDOP: %.1f (maximum: %.1f)",
			gpsData.HDOP, ghm.config.MaxHDOP))
	}

	// Check coordinates validity
	if gpsData.Latitude == 0 && gpsData.Longitude == 0 {
		issues = append(issues, "Invalid coordinates (0,0)")
	}

	if len(issues) > 0 {
		fmt.Printf("    ❌ GPS data quality issues: %s\n", strings.Join(issues, ", "))
		ghm.status.Issues = append(ghm.status.Issues, issues...)
		return false, nil
	}

	fmt.Printf("    ✅ GPS data quality good: %d sats, %.1fm accuracy, %.1f HDOP\n",
		gpsData.Satellites, gpsData.Accuracy, gpsData.HDOP)
	return true, nil
}

// GPSData holds GPS data for health checking
type GPSData struct {
	Latitude   float64
	Longitude  float64
	Accuracy   float64
	Satellites int
	HDOP       float64
	FixType    int
}

// getGPSData retrieves current GPS data for health assessment
func (ghm *GPSHealthMonitor) getGPSData() (*GPSData, error) {
	// Get data from AT command
	atOutput, err := executeCommand(ghm.sshClient, "gsmctl -A 'AT+QGPSLOC=2'")
	if err != nil {
		return nil, fmt.Errorf("AT command failed: %v", err)
	}

	gpsData := parseQGPSLOC(atOutput)
	if gpsData == nil {
		return nil, fmt.Errorf("failed to parse GPS data")
	}

	// Try to get accuracy from gpsctl
	accuracy := 10.0 // Default fallback
	if accuracyStr, err := executeCommand(ghm.sshClient, "gpsctl -u"); err == nil {
		if acc, parseErr := strconv.ParseFloat(strings.TrimSpace(accuracyStr), 64); parseErr == nil {
			accuracy = acc
		}
	}

	return &GPSData{
		Latitude:   gpsData.Latitude,
		Longitude:  gpsData.Longitude,
		Accuracy:   accuracy,
		Satellites: gpsData.Satellites,
		HDOP:       gpsData.HDOP,
		FixType:    gpsData.FixType,
	}, nil
}

// shouldResetGPS determines if GPS reset is needed
func (ghm *GPSHealthMonitor) shouldResetGPS() bool {
	// Don't reset if we're in cooldown period
	if time.Since(ghm.status.LastResetTime) < ghm.config.ResetCooldown {
		return false
	}

	// Reset if we have too many consecutive failures
	if ghm.status.ConsecutiveFailures >= ghm.config.MaxConsecutiveFailures {
		return true
	}

	// Reset if GPS session is not active
	if !ghm.status.GPSSessionActive {
		return true
	}

	// Reset if GPSD daemon is not running
	if !ghm.status.GPSDaemonRunning {
		return true
	}

	return false
}

// determineResetReason determines the reason for GPS reset
func (ghm *GPSHealthMonitor) determineResetReason() string {
	if !ghm.status.GPSSessionActive {
		return "GPS session inactive"
	}
	if !ghm.status.GPSDaemonRunning {
		return "GPSD daemon not running"
	}
	if ghm.status.ConsecutiveFailures >= ghm.config.MaxConsecutiveFailures {
		return fmt.Sprintf("Too many consecutive failures (%d)", ghm.status.ConsecutiveFailures)
	}
	return "GPS health degraded"
}

// ResetGPS performs GPS reset with multiple strategies
func (ghm *GPSHealthMonitor) ResetGPS(reason string) error {
	fmt.Printf("🔄 Initiating GPS reset (reason: %s)...\n", reason)

	ghm.status.LastResetTime = time.Now()
	ghm.status.LastResetReason = reason
	ghm.status.TotalResets++

	// Strategy 1: Soft reset - restart GPS session
	fmt.Println("  📡 Attempting soft reset (GPS session restart)...")
	if err := ghm.softResetGPS(); err == nil {
		fmt.Println("    ✅ Soft reset successful")
		ghm.status.ConsecutiveFailures = 0
		return nil
	} else {
		fmt.Printf("    ❌ Soft reset failed: %v\n", err)
	}

	// Strategy 2: Hard reset - restart GPSD daemon
	fmt.Println("  🔧 Attempting hard reset (GPSD daemon restart)...")
	if err := ghm.hardResetGPS(); err == nil {
		fmt.Println("    ✅ Hard reset successful")
		ghm.status.ConsecutiveFailures = 0
		return nil
	} else {
		fmt.Printf("    ❌ Hard reset failed: %v\n", err)
	}

	// Strategy 3: Full reset - restart modem GPS
	fmt.Println("  ⚡ Attempting full reset (modem GPS restart)...")
	if err := ghm.fullResetGPS(); err == nil {
		fmt.Println("    ✅ Full reset successful")
		ghm.status.ConsecutiveFailures = 0
		return nil
	} else {
		fmt.Printf("    ❌ Full reset failed: %v\n", err)
		return fmt.Errorf("all GPS reset strategies failed")
	}
}

// softResetGPS performs a soft GPS reset by restarting the GPS session
func (ghm *GPSHealthMonitor) softResetGPS() error {
	// Stop GPS
	_, err := executeCommand(ghm.sshClient, "gsmctl -A 'AT+QGPSEND'")
	if err != nil {
		return fmt.Errorf("failed to stop GPS: %v", err)
	}

	// Wait a moment
	time.Sleep(2 * time.Second)

	// Start GPS
	_, err = executeCommand(ghm.sshClient, "gsmctl -A 'AT+QGPS=1'")
	if err != nil {
		return fmt.Errorf("failed to start GPS: %v", err)
	}

	// Wait for GPS to initialize
	time.Sleep(5 * time.Second)

	// Verify GPS is working
	return ghm.verifyGPSWorking()
}

// hardResetGPS performs a hard GPS reset by restarting GPSD daemon
func (ghm *GPSHealthMonitor) hardResetGPS() error {
	// Stop GPSD daemon
	_, err := executeCommand(ghm.sshClient, "/etc/init.d/gpsd stop")
	if err != nil {
		return fmt.Errorf("failed to stop GPSD: %v", err)
	}

	// Wait a moment
	time.Sleep(3 * time.Second)

	// Start GPSD daemon
	_, err = executeCommand(ghm.sshClient, "/etc/init.d/gpsd start")
	if err != nil {
		return fmt.Errorf("failed to start GPSD: %v", err)
	}

	// Wait for daemon to initialize
	time.Sleep(10 * time.Second)

	// Restart GPS session
	return ghm.softResetGPS()
}

// fullResetGPS performs a full GPS reset by restarting the modem GPS subsystem
func (ghm *GPSHealthMonitor) fullResetGPS() error {
	// This is the most aggressive reset - restart the entire modem GPS
	_, err := executeCommand(ghm.sshClient, "gsmctl -A 'AT+CFUN=0'") // Disable modem
	if err != nil {
		return fmt.Errorf("failed to disable modem: %v", err)
	}

	time.Sleep(5 * time.Second)

	_, err = executeCommand(ghm.sshClient, "gsmctl -A 'AT+CFUN=1'") // Enable modem
	if err != nil {
		return fmt.Errorf("failed to enable modem: %v", err)
	}

	// Wait for modem to fully initialize
	time.Sleep(15 * time.Second)

	// Restart GPS
	return ghm.softResetGPS()
}

// verifyGPSWorking verifies that GPS is working after reset
func (ghm *GPSHealthMonitor) verifyGPSWorking() error {
	// Check GPS status
	output, err := executeCommand(ghm.sshClient, "gsmctl -A 'AT+QGPS?'")
	if err != nil {
		return fmt.Errorf("failed to check GPS status: %v", err)
	}

	if !strings.Contains(output, "+QGPS: 1") {
		return fmt.Errorf("GPS session not active after reset")
	}

	// Wait a bit for GPS to get a fix
	time.Sleep(10 * time.Second)

	// Try to get GPS data
	_, err = ghm.getGPSData()
	if err != nil {
		return fmt.Errorf("GPS data not available after reset: %v", err)
	}

	return nil
}

// displayHealthStatus displays the current GPS health status
func (ghm *GPSHealthMonitor) displayHealthStatus() {
	fmt.Println("\n📊 GPS Health Status Summary:")
	fmt.Println("=============================")

	healthIcon := "✅"
	if !ghm.status.Healthy {
		healthIcon = "❌"
	}

	fmt.Printf("Overall Health: %s %s\n", healthIcon, map[bool]string{true: "HEALTHY", false: "UNHEALTHY"}[ghm.status.Healthy])
	fmt.Printf("Last Successful Fix: %s\n", ghm.status.LastSuccessfulFix.Format("2006-01-02 15:04:05"))
	fmt.Printf("Consecutive Failures: %d\n", ghm.status.ConsecutiveFailures)
	fmt.Printf("Total Resets: %d\n", ghm.status.TotalResets)

	if !ghm.status.LastResetTime.IsZero() {
		fmt.Printf("Last Reset: %s (%s)\n", ghm.status.LastResetTime.Format("2006-01-02 15:04:05"), ghm.status.LastResetReason)
	}

	fmt.Printf("GPS Session Active: %s\n", map[bool]string{true: "✅ Yes", false: "❌ No"}[ghm.status.GPSSessionActive])
	fmt.Printf("GPSD Daemon Running: %s\n", map[bool]string{true: "✅ Yes", false: "❌ No"}[ghm.status.GPSDaemonRunning])

	if ghm.status.CurrentSatellites > 0 {
		fmt.Printf("Current GPS Data: %d sats, %.1fm accuracy, %.1f HDOP, fix type %d\n",
			ghm.status.CurrentSatellites, ghm.status.CurrentAccuracy, ghm.status.CurrentHDOP, ghm.status.CurrentFixType)
	}

	if len(ghm.status.Issues) > 0 {
		fmt.Println("Issues:")
		for _, issue := range ghm.status.Issues {
			fmt.Printf("  ⚠️  %s\n", issue)
		}
	}
}

// GetHealthStatus returns the current health status
func (ghm *GPSHealthMonitor) GetHealthStatus() *GPSHealthStatus {
	return ghm.status
}

// testGPSHealthMonitor tests the GPS health monitoring system
func testGPSHealthMonitor() {
	fmt.Println("🔍 GPS Health Monitor Test")
	fmt.Println("==========================")

	// Create default config
	config := &GPSHealthConfig{
		HealthCheckInterval:    5 * time.Minute,
		MaxConsecutiveFailures: 3,
		MinAccuracy:            10.0,
		MinSatellites:          4,
		MaxHDOP:                5.0,
		ResetCooldown:          10 * time.Minute,
		EnableAutoReset:        true,
		NotifyOnReset:          true,
	}

	// Create SSH client
	client, err := createSSHClient()
	if err != nil {
		fmt.Printf("❌ Failed to create SSH client: %v\n", err)
		return
	}
	defer client.Close()

	// Create health monitor
	monitor := NewGPSHealthMonitor(config, client)

	// Perform health check
	status, err := monitor.CheckGPSHealth()
	if err != nil {
		fmt.Printf("❌ Health check failed: %v\n", err)
		return
	}

	fmt.Printf("\n🎯 Health Check Complete: %s\n", map[bool]string{true: "HEALTHY", false: "NEEDS ATTENTION"}[status.Healthy])
}
