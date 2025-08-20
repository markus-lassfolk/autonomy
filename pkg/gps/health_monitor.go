package gps

import (
	"context"
	"fmt"
	"os/exec"
	"strconv"
	"strings"
	"sync"
	"time"

	"github.com/markus-lassfolk/autonomy/pkg/logx"
	"github.com/markus-lassfolk/autonomy/pkg/notifications"
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
	HealthCheckInterval    time.Duration `json:"health_check_interval"`    // 5 minutes
	MaxConsecutiveFailures int           `json:"max_consecutive_failures"` // 3 failures before reset
	MinAccuracy            float64       `json:"min_accuracy"`             // 10m minimum accuracy
	MinSatellites          int           `json:"min_satellites"`           // 4 satellites minimum
	MaxHDOP                float64       `json:"max_hdop"`                 // HDOP threshold
	ResetCooldown          time.Duration `json:"reset_cooldown"`           // 10 minutes between resets
	EnableAutoReset        bool          `json:"enable_auto_reset"`        // Enable automatic GPS reset
	NotifyOnReset          bool          `json:"notify_on_reset"`          // Send notifications on reset
}

// GPSHealthMonitor manages GPS health monitoring and recovery
type GPSHealthMonitor struct {
	config          *GPSHealthConfig
	status          *GPSHealthStatus
	logger          *logx.Logger
	notificationMgr *notifications.Manager
	mu              sync.RWMutex
	stopChan        chan struct{}
	running         bool
}

// NewGPSHealthMonitor creates a new GPS health monitor
func NewGPSHealthMonitor(config *GPSHealthConfig, logger *logx.Logger, notificationMgr *notifications.Manager) *GPSHealthMonitor {
	if config == nil {
		config = &GPSHealthConfig{
			HealthCheckInterval:    5 * time.Minute,
			MaxConsecutiveFailures: 3,
			MinAccuracy:            10.0,
			MinSatellites:          4,
			MaxHDOP:                5.0,
			ResetCooldown:          10 * time.Minute,
			EnableAutoReset:        true,
			NotifyOnReset:          true,
		}
	}

	status := &GPSHealthStatus{
		Healthy:             true,
		LastSuccessfulFix:   time.Now(),
		ConsecutiveFailures: 0,
		TotalResets:         0,
		LastHealthCheck:     time.Now(),
		HealthCheckInterval: config.HealthCheckInterval,
		Issues:              make([]string, 0),
	}

	return &GPSHealthMonitor{
		config:          config,
		status:          status,
		logger:          logger,
		notificationMgr: notificationMgr,
		stopChan:        make(chan struct{}),
	}
}

// Start starts the GPS health monitoring
func (ghm *GPSHealthMonitor) Start(ctx context.Context) error {
	ghm.mu.Lock()
	if ghm.running {
		ghm.mu.Unlock()
		return fmt.Errorf("GPS health monitor already running")
	}
	ghm.running = true
	ghm.mu.Unlock()

	ghm.logger.Info("gps_health_monitor_started",
		"check_interval", ghm.config.HealthCheckInterval,
		"max_failures", ghm.config.MaxConsecutiveFailures,
		"auto_reset", ghm.config.EnableAutoReset,
	)

	// Start monitoring loop
	go ghm.monitoringLoop(ctx)

	return nil
}

// Stop stops the GPS health monitoring
func (ghm *GPSHealthMonitor) Stop() {
	ghm.mu.Lock()
	defer ghm.mu.Unlock()

	if !ghm.running {
		return
	}

	close(ghm.stopChan)
	ghm.running = false
	ghm.logger.Info("gps_health_monitor_stopped")
}

// monitoringLoop runs the continuous health monitoring
func (ghm *GPSHealthMonitor) monitoringLoop(ctx context.Context) {
	ticker := time.NewTicker(ghm.config.HealthCheckInterval)
	defer ticker.Stop()

	for {
		select {
		case <-ctx.Done():
			return
		case <-ghm.stopChan:
			return
		case <-ticker.C:
			ghm.performHealthCheck(ctx)
		}
	}
}

// performHealthCheck performs a comprehensive GPS health check
func (ghm *GPSHealthMonitor) performHealthCheck(ctx context.Context) {
	ghm.mu.Lock()
	defer ghm.mu.Unlock()

	ghm.status.LastHealthCheck = time.Now()
	ghm.status.Issues = make([]string, 0) // Clear previous issues

	ghm.logger.LogDebugVerbose("gps_health_check_start", map[string]interface{}{
		"consecutive_failures": ghm.status.ConsecutiveFailures,
		"last_successful_fix":  ghm.status.LastSuccessfulFix,
	})

	// Check GPS daemon status
	ghm.checkGPSDaemonStatus(ctx)

	// Check GPS session status
	ghm.checkGPSSessionStatus(ctx)

	// Check GPS fix quality
	ghm.checkGPSFixQuality(ctx)

	// Check GPS hardware status
	ghm.checkGPSHardwareStatus(ctx)

	// Determine overall health
	ghm.determineOverallHealth()

	// Take corrective action if needed
	if !ghm.status.Healthy && ghm.config.EnableAutoReset {
		ghm.attemptGPSRecovery(ctx)
	}

	ghm.logger.Info("gps_health_check_completed",
		"healthy", ghm.status.Healthy,
		"consecutive_failures", ghm.status.ConsecutiveFailures,
		"issues_count", len(ghm.status.Issues),
		"current_accuracy", ghm.status.CurrentAccuracy,
		"current_satellites", ghm.status.CurrentSatellites,
	)
}

// checkGPSDaemonStatus checks if GPS daemon is running
func (ghm *GPSHealthMonitor) checkGPSDaemonStatus(ctx context.Context) {
	// Check if gpsd is running
	cmd := exec.CommandContext(ctx, "pgrep", "gpsd")
	err := cmd.Run()
	ghm.status.GPSDaemonRunning = err == nil

	if !ghm.status.GPSDaemonRunning {
		ghm.status.Issues = append(ghm.status.Issues, "gpsd_daemon_not_running")
		ghm.logger.LogDebugVerbose("gps_daemon_check", map[string]interface{}{
			"running": false,
		})
	}
}

// checkGPSSessionStatus checks GPS session status
func (ghm *GPSHealthMonitor) checkGPSSessionStatus(ctx context.Context) {
	// Check gpsctl status
	cmd := exec.CommandContext(ctx, "gpsctl", "-s")
	output, err := cmd.Output()
	if err != nil {
		ghm.status.GPSSessionActive = false
		ghm.status.Issues = append(ghm.status.Issues, "gps_session_check_failed")
		return
	}

	status := strings.TrimSpace(string(output))
	ghm.status.GPSSessionActive = strings.Contains(status, "enabled") || strings.Contains(status, "active")

	if !ghm.status.GPSSessionActive {
		ghm.status.Issues = append(ghm.status.Issues, "gps_session_inactive")
	}

	ghm.logger.LogDebugVerbose("gps_session_check", map[string]interface{}{
		"active": ghm.status.GPSSessionActive,
		"status": status,
	})
}

// checkGPSFixQuality checks the quality of the current GPS fix
func (ghm *GPSHealthMonitor) checkGPSFixQuality(ctx context.Context) {
	// Get current GPS data
	cmd := exec.CommandContext(ctx, "gpsctl", "-i")
	output, err := cmd.Output()
	if err != nil {
		ghm.status.Issues = append(ghm.status.Issues, "gps_fix_data_unavailable")
		ghm.status.ConsecutiveFailures++
		return
	}

	// Parse GPS data
	gpsData := strings.TrimSpace(string(output))
	ghm.parseGPSFixData(gpsData)

	// Check fix quality criteria
	hasValidFix := false

	if ghm.status.CurrentFixType >= 2 { // 2D or 3D fix
		hasValidFix = true

		// Check accuracy
		if ghm.status.CurrentAccuracy > ghm.config.MinAccuracy {
			ghm.status.Issues = append(ghm.status.Issues, "gps_accuracy_poor")
		}

		// Check satellite count
		if ghm.status.CurrentSatellites < ghm.config.MinSatellites {
			ghm.status.Issues = append(ghm.status.Issues, "gps_satellites_insufficient")
		}

		// Check HDOP
		if ghm.status.CurrentHDOP > ghm.config.MaxHDOP {
			ghm.status.Issues = append(ghm.status.Issues, "gps_hdop_poor")
		}
	} else {
		ghm.status.Issues = append(ghm.status.Issues, "gps_no_fix")
	}

	if hasValidFix {
		ghm.status.LastSuccessfulFix = time.Now()
		ghm.status.ConsecutiveFailures = 0
	} else {
		ghm.status.ConsecutiveFailures++
	}

	ghm.logger.LogDebugVerbose("gps_fix_quality_check", map[string]interface{}{
		"fix_type":   ghm.status.CurrentFixType,
		"accuracy":   ghm.status.CurrentAccuracy,
		"satellites": ghm.status.CurrentSatellites,
		"hdop":       ghm.status.CurrentHDOP,
		"valid_fix":  hasValidFix,
	})
}

// parseGPSFixData parses GPS fix data from gpsctl output
func (ghm *GPSHealthMonitor) parseGPSFixData(data string) {
	lines := strings.Split(data, "\n")
	for _, line := range lines {
		line = strings.TrimSpace(line)

		if strings.Contains(line, "Accuracy:") {
			parts := strings.Fields(line)
			if len(parts) >= 2 {
				if acc, err := strconv.ParseFloat(parts[1], 64); err == nil {
					ghm.status.CurrentAccuracy = acc
				}
			}
		}

		if strings.Contains(line, "Satellites:") {
			parts := strings.Fields(line)
			if len(parts) >= 2 {
				if sats, err := strconv.Atoi(parts[1]); err == nil {
					ghm.status.CurrentSatellites = sats
				}
			}
		}

		if strings.Contains(line, "HDOP:") {
			parts := strings.Fields(line)
			if len(parts) >= 2 {
				if hdop, err := strconv.ParseFloat(parts[1], 64); err == nil {
					ghm.status.CurrentHDOP = hdop
				}
			}
		}

		if strings.Contains(line, "Fix:") {
			parts := strings.Fields(line)
			if len(parts) >= 2 {
				switch strings.ToLower(parts[1]) {
				case "none", "no":
					ghm.status.CurrentFixType = 0
				case "2d":
					ghm.status.CurrentFixType = 2
				case "3d":
					ghm.status.CurrentFixType = 3
				default:
					ghm.status.CurrentFixType = 1
				}
			}
		}
	}
}

// checkGPSHardwareStatus checks GPS hardware status
func (ghm *GPSHealthMonitor) checkGPSHardwareStatus(ctx context.Context) {
	// Check if GPS device is accessible
	devices := []string{"/dev/ttyUSB1", "/dev/ttyUSB2", "/dev/ttyACM0"}
	deviceFound := false

	for _, device := range devices {
		cmd := exec.CommandContext(ctx, "test", "-c", device)
		if cmd.Run() == nil {
			deviceFound = true
			break
		}
	}

	if !deviceFound {
		ghm.status.Issues = append(ghm.status.Issues, "gps_hardware_not_accessible")
	}

	ghm.logger.LogDebugVerbose("gps_hardware_check", map[string]interface{}{
		"device_found": deviceFound,
	})
}

// determineOverallHealth determines the overall GPS health status
func (ghm *GPSHealthMonitor) determineOverallHealth() {
	previousHealth := ghm.status.Healthy

	// GPS is healthy if:
	// 1. No critical issues
	// 2. Consecutive failures below threshold
	// 3. Recent successful fix

	criticalIssues := []string{"gps_hardware_not_accessible", "gpsd_daemon_not_running"}
	hasCriticalIssue := false

	for _, issue := range ghm.status.Issues {
		for _, critical := range criticalIssues {
			if issue == critical {
				hasCriticalIssue = true
				break
			}
		}
		if hasCriticalIssue {
			break
		}
	}

	timeSinceLastFix := time.Since(ghm.status.LastSuccessfulFix)

	ghm.status.Healthy = !hasCriticalIssue &&
		ghm.status.ConsecutiveFailures < ghm.config.MaxConsecutiveFailures &&
		timeSinceLastFix < ghm.config.HealthCheckInterval*3 // Allow 3 check intervals

	// Log health status change
	if previousHealth != ghm.status.Healthy {
		ghm.logger.Info("gps_health_status_changed",
			"previous_healthy", previousHealth,
			"current_healthy", ghm.status.Healthy,
			"consecutive_failures", ghm.status.ConsecutiveFailures,
			"time_since_last_fix", timeSinceLastFix,
			"issues", ghm.status.Issues,
		)
	}
}

// attemptGPSRecovery attempts to recover GPS functionality
func (ghm *GPSHealthMonitor) attemptGPSRecovery(ctx context.Context) {
	// Check reset cooldown
	if time.Since(ghm.status.LastResetTime) < ghm.config.ResetCooldown {
		ghm.logger.LogDebugVerbose("gps_reset_skipped_cooldown", map[string]interface{}{
			"time_since_last_reset": time.Since(ghm.status.LastResetTime),
			"cooldown_period":       ghm.config.ResetCooldown,
		})
		return
	}

	ghm.logger.Info("gps_recovery_attempt_start",
		"consecutive_failures", ghm.status.ConsecutiveFailures,
		"issues", ghm.status.Issues,
	)

	var resetReason string
	var resetSuccess bool

	// Try different recovery methods
	if ghm.containsIssue("gpsd_daemon_not_running") {
		resetReason = "gpsd_daemon_restart"
		resetSuccess = ghm.restartGPSDaemon(ctx)
	} else if ghm.containsIssue("gps_session_inactive") {
		resetReason = "gps_session_restart"
		resetSuccess = ghm.restartGPSSession(ctx)
	} else if ghm.containsIssue("gps_no_fix") || ghm.containsIssue("gps_accuracy_poor") {
		resetReason = "gps_cold_restart"
		resetSuccess = ghm.performGPSColdRestart(ctx)
	} else {
		resetReason = "gps_full_reset"
		resetSuccess = ghm.performFullGPSReset(ctx)
	}

	// Update reset statistics
	ghm.status.LastResetTime = time.Now()
	ghm.status.LastResetReason = resetReason
	ghm.status.TotalResets++

	if resetSuccess {
		ghm.logger.Info("gps_recovery_successful",
			"reset_reason", resetReason,
			"total_resets", ghm.status.TotalResets,
		)

		// Send notification if enabled
		if ghm.config.NotifyOnReset && ghm.notificationMgr != nil {
			ghm.sendResetNotification(resetReason, true)
		}
	} else {
		ghm.logger.Error("gps_recovery_failed",
			"reset_reason", resetReason,
			"total_resets", ghm.status.TotalResets,
		)

		// Send failure notification
		if ghm.config.NotifyOnReset && ghm.notificationMgr != nil {
			ghm.sendResetNotification(resetReason, false)
		}
	}
}

// Recovery methods

func (ghm *GPSHealthMonitor) restartGPSDaemon(ctx context.Context) bool {
	cmd := exec.CommandContext(ctx, "/etc/init.d/gpsd", "restart")
	err := cmd.Run()
	return err == nil
}

func (ghm *GPSHealthMonitor) restartGPSSession(ctx context.Context) bool {
	// Stop GPS session
	cmd1 := exec.CommandContext(ctx, "gpsctl", "-x")
	if err := cmd1.Run(); err != nil {
		ghm.logger.Warn("Failed to stop GPS session", "error", err)
	}

	time.Sleep(2 * time.Second)

	// Start GPS session
	cmd2 := exec.CommandContext(ctx, "gpsctl", "-e")
	err := cmd2.Run()
	return err == nil
}

func (ghm *GPSHealthMonitor) performGPSColdRestart(ctx context.Context) bool {
	// Cold restart via AT command
	cmd := exec.CommandContext(ctx, "gsmctl", "-A", "AT+QGPSCFG=\"gpsnmeatype\",31")
	if err := cmd.Run(); err != nil {
		ghm.logger.Warn("Failed to configure GPS NMEA type", "error", err)
	}

	time.Sleep(1 * time.Second)

	cmd2 := exec.CommandContext(ctx, "gsmctl", "-A", "AT+QGPS=2")
	err := cmd2.Run()
	return err == nil
}

func (ghm *GPSHealthMonitor) performFullGPSReset(ctx context.Context) bool {
	// Full GPS reset sequence
	success := true

	// Stop GPS
	cmd1 := exec.CommandContext(ctx, "gpsctl", "-x")
	if cmd1.Run() != nil {
		success = false
	}

	// Reset via AT command
	cmd2 := exec.CommandContext(ctx, "gsmctl", "-A", "AT+QGPSEND")
	if err := cmd2.Run(); err != nil {
		ghm.logger.Warn("Failed to end GPS session", "error", err)
	}

	time.Sleep(5 * time.Second)

	// Restart GPS
	cmd3 := exec.CommandContext(ctx, "gpsctl", "-e")
	if cmd3.Run() != nil {
		success = false
	}

	return success
}

// Helper methods

func (ghm *GPSHealthMonitor) containsIssue(issue string) bool {
	for _, existingIssue := range ghm.status.Issues {
		if existingIssue == issue {
			return true
		}
	}
	return false
}

func (ghm *GPSHealthMonitor) sendResetNotification(reason string, success bool) {
	status := "successful"
	if !success {
		status = "failed"
	}

	title := fmt.Sprintf("GPS Reset %s", strings.ToUpper(status[:1])+strings.ToLower(status[1:]))
	message := fmt.Sprintf("GPS reset attempt (%s) was %s. Total resets: %d",
		reason, status, ghm.status.TotalResets)

	// TODO: Implement full notification sending
	ghm.logger.Info("gps_reset_notification",
		"title", title,
		"message", message,
		"reason", reason,
		"success", success,
	)
}

// GetHealthStatus returns the current GPS health status
func (ghm *GPSHealthMonitor) GetHealthStatus() *GPSHealthStatus {
	ghm.mu.RLock()
	defer ghm.mu.RUnlock()

	// Return a copy to avoid race conditions
	statusCopy := *ghm.status
	statusCopy.Issues = make([]string, len(ghm.status.Issues))
	copy(statusCopy.Issues, ghm.status.Issues)

	return &statusCopy
}

// UpdateConfig updates the health monitor configuration
func (ghm *GPSHealthMonitor) UpdateConfig(config *GPSHealthConfig) {
	ghm.mu.Lock()
	defer ghm.mu.Unlock()

	ghm.config = config
	ghm.status.HealthCheckInterval = config.HealthCheckInterval

	ghm.logger.Info("gps_health_config_updated",
		"check_interval", config.HealthCheckInterval,
		"max_failures", config.MaxConsecutiveFailures,
		"auto_reset", config.EnableAutoReset,
	)
}
