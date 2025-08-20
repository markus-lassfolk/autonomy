package sysmgmt

import (
	"context"
	"fmt"
	"os/exec"
	"strings"
	"time"

	"github.com/markus-lassfolk/autonomy/pkg/logx"
)

// ServiceWatchdog monitors and restarts hung services
type ServiceWatchdog struct {
	config *Config
	logger *logx.Logger
	dryRun bool
}

// NewServiceWatchdog creates a new service watchdog
func NewServiceWatchdog(config *Config, logger *logx.Logger, dryRun bool) *ServiceWatchdog {
	return &ServiceWatchdog{
		config: config,
		logger: logger,
		dryRun: dryRun,
	}
}

// Check monitors services and restarts hung ones
func (sw *ServiceWatchdog) Check(ctx context.Context) error {
	if !sw.config.ServiceWatchdogEnabled {
		return nil
	}

	sw.logger.Debug("Checking service health")

	for _, service := range sw.config.ServicesToMonitor {
		select {
		case <-ctx.Done():
			return ctx.Err()
		default:
		}

		if err := sw.checkService(ctx, service); err != nil {
			sw.logger.Error("Service check failed", "service", service, "error", err)
		}
	}

	return nil
}

// checkService checks if a specific service is healthy
func (sw *ServiceWatchdog) checkService(ctx context.Context, service string) error {
	// Check if service is running
	running, err := sw.isServiceRunning(service)
	if err != nil {
		return fmt.Errorf("failed to check service status: %w", err)
	}

	if !running {
		sw.logger.Warn("Service not running", "service", service)
		return sw.restartService(ctx, service, "not running")
	}

	// Check if service has recent log activity
	active, err := sw.hasRecentActivity(service)
	if err != nil {
		return fmt.Errorf("failed to check service activity: %w", err)
	}

	if !active {
		sw.logger.Warn("Service appears hung (no recent activity)", "service", service)
		return sw.restartService(ctx, service, "no recent activity")
	}

	sw.logger.Debug("Service healthy", "service", service)
	return nil
}

// isServiceRunning checks if a service is currently running
func (sw *ServiceWatchdog) isServiceRunning(service string) (bool, error) {
	// Try different methods to check service status
	methods := []func(string) (bool, error){
		sw.checkServiceStatus,
		sw.checkProcessRunning,
		sw.checkInitScript,
	}

	for _, method := range methods {
		if running, err := method(service); err == nil {
			return running, nil
		}
	}

	// If all methods fail, assume service is not running
	return false, nil
}

// checkServiceStatus checks service status using systemctl or init.d
func (sw *ServiceWatchdog) checkServiceStatus(service string) (bool, error) {
	// Try systemctl first
	cmd := exec.Command("systemctl", "is-active", service)
	if err := cmd.Run(); err == nil {
		return true, nil
	}

	// Try init.d script
	cmd = exec.Command("/etc/init.d/"+service, "status")
	if err := cmd.Run(); err == nil {
		return true, nil
	}

	return false, nil
}

// checkProcessRunning checks if a process is running by name
func (sw *ServiceWatchdog) checkProcessRunning(service string) (bool, error) {
	cmd := exec.Command("pgrep", service)
	if err := cmd.Run(); err == nil {
		return true, nil
	}

	// Try with full path
	cmd = exec.Command("pgrep", "-f", service)
	if err := cmd.Run(); err == nil {
		return true, nil
	}

	return false, nil
}

// checkInitScript checks service status using init script
func (sw *ServiceWatchdog) checkInitScript(service string) (bool, error) {
	scriptPath := "/etc/init.d/" + service

	cmd := exec.Command("sh", scriptPath, "status")
	output, err := cmd.Output()
	if err != nil {
		return false, err
	}

	outputStr := strings.ToLower(string(output))
	return strings.Contains(outputStr, "running") || strings.Contains(outputStr, "active"), nil
}

// hasRecentActivity checks if a service has recent log activity
func (sw *ServiceWatchdog) hasRecentActivity(service string) (bool, error) {
	cutoff := time.Now().Add(-sw.config.ServiceTimeout)

	// Check system logs for recent activity (RUTOS uses logread)
	logSources := []string{
		"logread", // Use logread command on RUTOS
	}

	for _, logSource := range logSources {
		if active, err := sw.checkLogActivity(logSource, service, cutoff); err == nil && active {
			return true, nil
		}
	}

	// If no log activity found, check if service is actually running
	// Some services don't log frequently but are still healthy
	running, err := sw.isServiceRunning(service)
	if err != nil {
		return false, err
	}

	// If service is running but no recent logs, assume it's healthy
	// This prevents false positives for services that don't log frequently
	return running, nil
}

// checkLogActivity checks if a log source has recent entries for a service
func (sw *ServiceWatchdog) checkLogActivity(logSource, service string, cutoff time.Time) (bool, error) {
	var cmd *exec.Cmd

	// Handle different log sources
	if logSource == "logread" {
		// Use logread command on RUTOS
		cmd = exec.Command("logread")
	} else {
		// Fallback to tail for file-based logs
		cmd = exec.Command("tail", "-n", "100", logSource)
	}

	output, err := cmd.Output()
	if err != nil {
		return false, err
	}

	lines := strings.Split(string(output), "\n")
	for _, line := range lines {
		if strings.Contains(line, service) {
			// Try to parse timestamp from log line
			if timestamp, err := sw.parseLogTimestamp(line); err == nil {
				if timestamp.After(cutoff) {
					return true, nil
				}
			}
		}
	}

	return false, nil
}

// parseLogTimestamp attempts to parse timestamp from log line
func (sw *ServiceWatchdog) parseLogTimestamp(line string) (time.Time, error) {
	// Common log timestamp formats
	formats := []string{
		"2006-01-02 15:04:05",
		"Jan 2 15:04:05",
		"2006/01/02 15:04:05",
	}

	// Extract timestamp from beginning of line
	fields := strings.Fields(line)
	if len(fields) < 2 {
		return time.Time{}, fmt.Errorf("insufficient fields in log line")
	}

	// Try different timestamp formats
	timestampStr := fields[0] + " " + fields[1]
	for _, format := range formats {
		if timestamp, err := time.Parse(format, timestampStr); err == nil {
			return timestamp, nil
		}
	}

	return time.Time{}, fmt.Errorf("unable to parse timestamp")
}

// restartService restarts a service
func (sw *ServiceWatchdog) restartService(ctx context.Context, service, reason string) error {
	sw.logger.Info("Restarting service", "service", service, "reason", reason, "dry_run", sw.dryRun)

	if sw.dryRun {
		sw.logger.Info("DRY RUN: Would restart service", "service", service, "reason", reason)
		return nil
	}

	if !sw.config.ServiceRestartEnabled {
		sw.logger.Warn("Service restart disabled, skipping", "service", service)
		return nil
	}

	// Try different restart methods
	restartMethods := []func(string) error{
		sw.restartWithSystemctl,
		sw.restartWithInitScript,
		sw.restartWithKill,
	}

	var lastErr error
	for _, method := range restartMethods {
		if err := method(service); err == nil {
			sw.logger.Info("Service restarted successfully", "service", service, "method", "unknown")

			// Send notification
			if sw.config.NotificationsEnabled && sw.config.NotifyOnFixes {
				sw.sendFixNotification("Service restart", fmt.Sprintf("Restarted %s (%s)", service, reason))
			}

			return nil
		} else {
			lastErr = err
		}
	}

	sw.logger.Error("Failed to restart service", "service", service, "error", lastErr)

	// Send failure notification
	if sw.config.NotificationsEnabled && sw.config.NotifyOnFailures {
		sw.sendFailureNotification("Service restart failed", fmt.Sprintf("Failed to restart %s: %v", service, lastErr))
	}

	return lastErr
}

// restartWithSystemctl restarts service using systemctl
func (sw *ServiceWatchdog) restartWithSystemctl(service string) error {
	cmd := exec.Command("systemctl", "restart", service)
	return cmd.Run()
}

// restartWithInitScript restarts service using init script
func (sw *ServiceWatchdog) restartWithInitScript(service string) error {
	cmd := exec.Command("/etc/init.d/"+service, "restart")
	return cmd.Run()
}

// restartWithKill restarts service by killing and restarting process
func (sw *ServiceWatchdog) restartWithKill(service string) error {
	// Kill existing process using killall (available on RUTOS)
	cmd := exec.Command("killall", service)
	if err := cmd.Run(); err != nil {
		// Ignore errors, process might not exist
		sw.logger.Debug("Failed to kill process", "service", service, "error", err)
	}

	// Wait a moment
	time.Sleep(2 * time.Second)

	// Try to start service
	cmd = exec.Command("/etc/init.d/"+service, "start")
	return cmd.Run()
}

// sendFixNotification sends a fix notification
func (sw *ServiceWatchdog) sendFixNotification(action, details string) {
	// This would be implemented by the notification manager
	sw.logger.Info("Fix notification", "action", action, "details", details)
}

// sendFailureNotification sends a failure notification
func (sw *ServiceWatchdog) sendFailureNotification(action, details string) {
	// This would be implemented by the notification manager
	sw.logger.Error("Failure notification", "action", action, "details", details)
}
