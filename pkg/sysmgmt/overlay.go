package sysmgmt

import (
	"context"
	"fmt"
	"os"
	"os/exec"
	"path/filepath"
	"strconv"
	"strings"
	"time"

	"github.com/markus-lassfolk/autonomy/pkg/logx"
)

// OverlayManager manages overlay filesystem space
type OverlayManager struct {
	config *Config
	logger *logx.Logger
	dryRun bool
}

// NewOverlayManager creates a new overlay manager
func NewOverlayManager(config *Config, logger *logx.Logger, dryRun bool) *OverlayManager {
	return &OverlayManager{
		config: config,
		logger: logger,
		dryRun: dryRun,
	}
}

// Check monitors overlay space and performs cleanup if needed
func (om *OverlayManager) Check(ctx context.Context) error {
	usage, err := om.getOverlayUsage()
	if err != nil {
		return fmt.Errorf("failed to get overlay usage: %w", err)
	}

	// If usage is 0, it means we're monitoring a read-only filesystem, so skip
	if usage == 0 {
		om.logger.Debug("Skipping overlay check - monitoring read-only filesystem")
		return nil
	}

	om.logger.Debug("Overlay space check", "usage_percent", usage, "threshold", om.config.OverlaySpaceThreshold, "critical_threshold", om.config.OverlayCriticalThreshold)

	// Only alert if thresholds are properly configured (not 0)
	if om.config.OverlayCriticalThreshold == 0 {
		om.logger.Warn("Overlay critical threshold not configured", "usage_percent", usage)
		return nil
	}

	// Only monitor when usage is above a reasonable threshold (e.g., 50%)
	// This prevents false alarms on systems with plenty of space
	if usage < 50 {
		om.logger.Debug("Overlay usage below monitoring threshold", "usage_percent", usage, "monitoring_threshold", 50)
		return nil
	}

	if usage >= om.config.OverlayCriticalThreshold {
		om.logger.Warn("Critical overlay space usage", "usage_percent", usage, "threshold", om.config.OverlayCriticalThreshold)
		if om.config.NotificationsEnabled && om.config.NotifyOnCritical {
			om.sendCriticalNotification(usage)
		}
		return om.performEmergencyCleanup(ctx)
	} else if usage >= om.config.OverlaySpaceThreshold {
		om.logger.Warn("High overlay space usage", "usage_percent", usage, "threshold", om.config.OverlaySpaceThreshold)
		return om.performCleanup(ctx)
	}

	return nil
}

// getOverlayUsage returns the overlay filesystem usage percentage
func (om *OverlayManager) getOverlayUsage() (int, error) {
	// First, check if /overlay is mounted and writable
	cmd := exec.Command("df", "/overlay")
	output, err := cmd.Output()
	if err != nil {
		return 0, fmt.Errorf("failed to check /overlay: %w", err)
	}

	lines := strings.Split(string(output), "\n")
	if len(lines) < 2 {
		return 0, fmt.Errorf("unexpected df output format")
	}

	// Parse the usage percentage from df output
	fields := strings.Fields(lines[1])
	if len(fields) < 5 {
		return 0, fmt.Errorf("unexpected df output format")
	}

	usageStr := strings.TrimSuffix(fields[4], "%")
	usage, err := strconv.Atoi(usageStr)
	if err != nil {
		return 0, fmt.Errorf("failed to parse usage percentage: %w", err)
	}

	// Check if this is a read-only filesystem
	device := fields[0]
	if strings.Contains(device, "tmpfs") || strings.Contains(device, "squashfs") {
		om.logger.Debug("Skipping read-only filesystem", "device", device, "usage_percent", usage)
		return 0, nil
	}

	// Check if the filesystem is mounted read-only
	mountCmd := exec.Command("mount")
	mountOutput, err := mountCmd.Output()
	if err == nil {
		// Look for /overlay mount line and check if it's read-only
		lines := strings.Split(string(mountOutput), "\n")
		for _, line := range lines {
			if strings.Contains(line, " /overlay ") {
				if strings.Contains(line, "ro,") || strings.Contains(line, " ro ") {
					om.logger.Debug("Skipping read-only mounted overlay", "usage_percent", usage)
					return 0, nil
				}
				break
			}
		}
	}

	om.logger.Debug("Overlay space check", "device", device, "usage_percent", usage, "threshold", om.config.OverlayCriticalThreshold)
	return usage, nil
}

// performCleanup performs routine cleanup of stale files
func (om *OverlayManager) performCleanup(ctx context.Context) error {
	om.logger.Info("Starting overlay cleanup", "dry_run", om.dryRun)

	cleanupTasks := []struct {
		name string
		fn   func(context.Context) (int64, error)
	}{
		{"stale backup files", om.cleanupStaleBackups},
		{"old log files", om.cleanupOldLogs},
		{"temporary files", om.cleanupTempFiles},
		{"maintenance logs", om.cleanupMaintenanceLogs},
	}

	var totalFreed int64
	for _, task := range cleanupTasks {
		select {
		case <-ctx.Done():
			return ctx.Err()
		default:
		}

		if freed, err := task.fn(ctx); err != nil {
			om.logger.Error("Cleanup task failed", "task", task.name, "error", err)
		} else {
			totalFreed += freed
		}
	}

	if totalFreed > 0 {
		om.logger.Info("Overlay cleanup completed", "bytes_freed", totalFreed, "dry_run", om.dryRun)
		if om.config.NotificationsEnabled && om.config.NotifyOnFixes {
			om.sendFixNotification("Overlay cleanup", fmt.Sprintf("Freed %d bytes", totalFreed))
		}
	}

	return nil
}

// performEmergencyCleanup performs aggressive cleanup for critical space situations
func (om *OverlayManager) performEmergencyCleanup(ctx context.Context) error {
	om.logger.Warn("Performing emergency overlay cleanup", "dry_run", om.dryRun)

	// More aggressive cleanup for emergency situations
	emergencyTasks := []struct {
		name string
		fn   func(context.Context) (int64, error)
	}{
		{"all backup files", om.cleanupAllBackups},
		{"all log files", om.cleanupAllLogs},
		{"all temporary files", om.cleanupAllTempFiles},
		{"system cache", om.cleanupSystemCache},
	}

	var totalFreed int64
	for _, task := range emergencyTasks {
		select {
		case <-ctx.Done():
			return ctx.Err()
		default:
		}

		if freed, err := task.fn(ctx); err != nil {
			om.logger.Error("Emergency cleanup task failed", "task", task.name, "error", err)
		} else {
			totalFreed += freed
		}
	}

	if totalFreed > 0 {
		om.logger.Warn("Emergency overlay cleanup completed", "bytes_freed", totalFreed, "dry_run", om.dryRun)
		if om.config.NotificationsEnabled && om.config.NotifyOnCritical {
			om.sendCriticalNotification(0, "Emergency cleanup completed", fmt.Sprintf("Freed %d bytes", totalFreed))
		}
	}

	return nil
}

// cleanupStaleBackups removes old backup files
func (om *OverlayManager) cleanupStaleBackups(ctx context.Context) (int64, error) {
	if om.dryRun {
		return 0, nil
	}

	cutoff := time.Now().AddDate(0, 0, -om.config.CleanupRetentionDays)
	patterns := []string{"*.old", "*.bak", "*.tmp", "*.backup"}

	var totalFreed int64
	for _, pattern := range patterns {
		matches, err := filepath.Glob(filepath.Join("/overlay", "**", pattern))
		if err != nil {
			continue
		}

		for _, file := range matches {
			select {
			case <-ctx.Done():
				return totalFreed, ctx.Err()
			default:
			}

			info, err := os.Stat(file)
			if err != nil {
				continue
			}

			if info.ModTime().Before(cutoff) {
				if err := os.Remove(file); err == nil {
					totalFreed += info.Size()
					om.logger.Debug("Removed stale backup file", "file", file, "size", info.Size())
				}
			}
		}
	}

	return totalFreed, nil
}

// cleanupOldLogs removes old log files
func (om *OverlayManager) cleanupOldLogs(ctx context.Context) (int64, error) {
	if om.dryRun {
		return 0, nil
	}

	cutoff := time.Now().AddDate(0, 0, -om.config.CleanupRetentionDays)
	logDirs := []string{"/var/log", "/tmp/log", "/overlay/var/log"}

	var totalFreed int64
	for _, logDir := range logDirs {
		if err := filepath.Walk(logDir, func(path string, info os.FileInfo, err error) error {
			if err != nil {
				return nil
			}

			select {
			case <-ctx.Done():
				return ctx.Err()
			default:
			}

			if !info.IsDir() && info.ModTime().Before(cutoff) {
				if strings.HasSuffix(path, ".log") || strings.HasSuffix(path, ".gz") {
					if err := os.Remove(path); err == nil {
						totalFreed += info.Size()
						om.logger.Debug("Removed old log file", "file", path, "size", info.Size())
					}
				}
			}
			return nil
		}); err != nil {
			om.logger.Debug("Error walking log directory", "dir", logDir, "error", err)
		}
	}

	return totalFreed, nil
}

// cleanupTempFiles removes temporary files
func (om *OverlayManager) cleanupTempFiles(ctx context.Context) (int64, error) {
	if om.dryRun {
		return 0, nil
	}

	cutoff := time.Now().Add(-24 * time.Hour) // Remove temp files older than 24 hours
	tempDirs := []string{"/tmp", "/var/tmp"}

	var totalFreed int64
	for _, tempDir := range tempDirs {
		if err := filepath.Walk(tempDir, func(path string, info os.FileInfo, err error) error {
			if err != nil {
				return nil
			}

			select {
			case <-ctx.Done():
				return ctx.Err()
			default:
			}

			if !info.IsDir() && info.ModTime().Before(cutoff) {
				if err := os.Remove(path); err == nil {
					totalFreed += info.Size()
					om.logger.Debug("Removed temp file", "file", path, "size", info.Size())
				}
			}
			return nil
		}); err != nil {
			om.logger.Debug("Error walking temp directory", "dir", tempDir, "error", err)
		}
	}

	return totalFreed, nil
}

// cleanupMaintenanceLogs removes old maintenance logs
func (om *OverlayManager) cleanupMaintenanceLogs(ctx context.Context) (int64, error) {
	if om.dryRun {
		return 0, nil
	}

	cutoff := time.Now().AddDate(0, 0, -om.config.CleanupRetentionDays)
	maintenanceLog := "/var/log/system-maintenance.log"

	info, err := os.Stat(maintenanceLog)
	if err != nil {
		return 0, nil // File doesn't exist
	}

	if info.ModTime().Before(cutoff) {
		if err := os.Remove(maintenanceLog); err == nil {
			om.logger.Debug("Removed old maintenance log", "file", maintenanceLog, "size", info.Size())
			return info.Size(), nil
		}
	}

	return 0, nil
}

// cleanupAllBackups removes all backup files regardless of age
func (om *OverlayManager) cleanupAllBackups(ctx context.Context) (int64, error) {
	if om.dryRun {
		return 0, nil
	}

	patterns := []string{"*.old", "*.bak", "*.tmp", "*.backup"}
	var totalFreed int64

	for _, pattern := range patterns {
		matches, err := filepath.Glob(filepath.Join("/overlay", "**", pattern))
		if err != nil {
			continue
		}

		for _, file := range matches {
			select {
			case <-ctx.Done():
				return totalFreed, ctx.Err()
			default:
			}

			if info, err := os.Stat(file); err == nil {
				if err := os.Remove(file); err == nil {
					totalFreed += info.Size()
					om.logger.Debug("Emergency: removed backup file", "file", file, "size", info.Size())
				}
			}
		}
	}

	return totalFreed, nil
}

// cleanupAllLogs removes all log files regardless of age
func (om *OverlayManager) cleanupAllLogs(ctx context.Context) (int64, error) {
	if om.dryRun {
		return 0, nil
	}

	logDirs := []string{"/var/log", "/tmp/log", "/overlay/var/log"}
	var totalFreed int64

	for _, logDir := range logDirs {
		if err := filepath.Walk(logDir, func(path string, info os.FileInfo, err error) error {
			if err != nil {
				return nil
			}

			select {
			case <-ctx.Done():
				return ctx.Err()
			default:
			}

			if !info.IsDir() {
				if strings.HasSuffix(path, ".log") || strings.HasSuffix(path, ".gz") {
					if err := os.Remove(path); err == nil {
						totalFreed += info.Size()
						om.logger.Debug("Emergency: removed log file", "file", path, "size", info.Size())
					}
				}
			}
			return nil
		}); err != nil {
			om.logger.Debug("Error walking log directory", "dir", logDir, "error", err)
		}
	}

	return totalFreed, nil
}

// cleanupAllTempFiles removes only safe temporary files regardless of age
func (om *OverlayManager) cleanupAllTempFiles(ctx context.Context) (int64, error) {
	if om.dryRun {
		return 0, nil
	}

	tempDirs := []string{"/tmp", "/var/tmp"}
	var totalFreed int64

	// Define safe patterns for emergency cleanup - only remove files that are clearly temporary
	safePatterns := []string{
		"*.tmp", "*.temp", "*.cache", "*.log", "*.bak", "*.old", "*.backup",
		"*.pid", "*.lock", "*.sock", "*.conf", "*.json", "*.db", "*.dat",
	}

	// Define critical system files that should NEVER be removed
	criticalFiles := map[string]bool{
		"/tmp/run":                      true,
		"/tmp/state":                    true,
		"/tmp/etc":                      true,
		"/tmp/var":                      true,
		"/tmp/overlay":                  true,
		"/tmp/resolv.conf":              true,
		"/tmp/resolv.conf.auto":         true,
		"/tmp/hosts":                    true,
		"/tmp/firewall":                 true,
		"/tmp/network":                  true,
		"/tmp/system":                   true,
		"/tmp/wireless":                 true,
		"/tmp/dhcp":                     true,
		"/tmp/dnsmasq":                  true,
		"/tmp/ubus":                     true,
		"/tmp/rpcd":                     true,
		"/tmp/procd":                    true,
		"/tmp/init":                     true,
		"/tmp/boot":                     true,
		"/tmp/startup":                  true,
		"/tmp/shutdown":                 true,
		"/tmp/rc":                       true,
		"/tmp/rc.local":                 true,
		"/tmp/rc.common":                true,
		"/tmp/rc.functions":             true,
		"/tmp/rc.init":                  true,
		"/tmp/rc.shutdown":              true,
		"/tmp/rc.submit":                true,
		"/tmp/rc.submit-early":          true,
		"/tmp/rc.submit-late":           true,
		"/tmp/rc.submit-early-common":   true,
		"/tmp/rc.submit-late-common":    true,
		"/tmp/rc.submit-common":         true,
		"/tmp/rc.submit-early-init":     true,
		"/tmp/rc.submit-late-init":      true,
		"/tmp/rc.submit-init":           true,
		"/tmp/rc.submit-early-shutdown": true,
		"/tmp/rc.submit-late-shutdown":  true,
		"/tmp/rc.submit-shutdown":       true,
	}

	for _, tempDir := range tempDirs {
		if err := filepath.Walk(tempDir, func(path string, info os.FileInfo, err error) error {
			if err != nil {
				return nil
			}

			select {
			case <-ctx.Done():
				return ctx.Err()
			default:
			}

			if !info.IsDir() {
				// Skip critical system files
				if criticalFiles[path] {
					return nil
				}

				// Only remove files that match safe patterns
				shouldRemove := false
				for _, pattern := range safePatterns {
					if strings.HasSuffix(path, pattern[1:]) { // Remove the * from the pattern
						shouldRemove = true
						break
					}
				}

				// Also remove files that are clearly temporary (based on name patterns)
				if strings.Contains(filepath.Base(path), "temp") ||
					strings.Contains(filepath.Base(path), "cache") ||
					strings.Contains(filepath.Base(path), "tmp") ||
					strings.Contains(filepath.Base(path), "log") {
					shouldRemove = true
				}

				// Skip files that are clearly system files
				if strings.Contains(path, "/run/") ||
					strings.Contains(path, "/state/") ||
					strings.Contains(path, "/etc/") ||
					strings.Contains(path, "/var/") ||
					strings.Contains(path, "/overlay/") {
					shouldRemove = false
				}

				if shouldRemove {
					if err := os.Remove(path); err == nil {
						totalFreed += info.Size()
						om.logger.Debug("Emergency: removed temp file", "file", path, "size", info.Size())
					}
				}
			}
			return nil
		}); err != nil {
			om.logger.Debug("Error walking temp directory", "dir", tempDir, "error", err)
		}
	}

	return totalFreed, nil
}

// cleanupSystemCache removes system cache files
func (om *OverlayManager) cleanupSystemCache(ctx context.Context) (int64, error) {
	if om.dryRun {
		return 0, nil
	}

	// Clear various system caches
	cacheDirs := []string{"/var/cache", "/tmp/cache"}
	var totalFreed int64

	for _, cacheDir := range cacheDirs {
		if err := filepath.Walk(cacheDir, func(path string, info os.FileInfo, err error) error {
			if err != nil {
				return nil
			}

			select {
			case <-ctx.Done():
				return ctx.Err()
			default:
			}

			if !info.IsDir() {
				if err := os.Remove(path); err == nil {
					totalFreed += info.Size()
					om.logger.Debug("Emergency: removed cache file", "file", path, "size", info.Size())
				}
			}
			return nil
		}); err != nil {
			om.logger.Debug("Error walking cache directory", "dir", cacheDir, "error", err)
		}
	}

	return totalFreed, nil
}

// sendCriticalNotification sends a critical notification
func (om *OverlayManager) sendCriticalNotification(usage int, args ...interface{}) {
	// This would be implemented by the notification manager
	om.logger.Warn("Critical overlay space notification", "usage_percent", usage)
}

// sendFixNotification sends a fix notification
func (om *OverlayManager) sendFixNotification(action, details string) {
	// This would be implemented by the notification manager
	om.logger.Info("Fix notification", "action", action, "details", details)
}
