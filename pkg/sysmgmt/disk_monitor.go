package sysmgmt

import (
	"fmt"
	"os/exec"
	"strconv"
	"strings"
	"time"

	"github.com/markus-lassfolk/autonomy/pkg/logx"
)

// DiskSpaceInfo represents disk space information
type DiskSpaceInfo struct {
	Path         string  `json:"path"`
	TotalGB      float64 `json:"total_gb"`
	UsedGB       float64 `json:"used_gb"`
	AvailableGB  float64 `json:"available_gb"`
	UsagePercent float64 `json:"usage_percent"`
	InodesTotal  int64   `json:"inodes_total"`
	InodesUsed   int64   `json:"inodes_used"`
	InodesFree   int64   `json:"inodes_free"`
}

// DiskMonitor manages disk space monitoring and cleanup
type DiskMonitor struct {
	logger *logx.Logger
	config *DiskMonitorConfig
}

// DiskMonitorConfig holds configuration for disk monitoring
type DiskMonitorConfig struct {
	CriticalThresholdGB float64  `json:"critical_threshold_gb"` // Critical disk space threshold
	WarningThresholdGB  float64  `json:"warning_threshold_gb"`  // Warning disk space threshold
	CleanupThresholdGB  float64  `json:"cleanup_threshold_gb"`  // Threshold to trigger cleanup
	MonitorPaths        []string `json:"monitor_paths"`         // Paths to monitor
	MaxLogSizeMB        int      `json:"max_log_size_mb"`       // Maximum log file size
	MaxTempAgeHours     int      `json:"max_temp_age_hours"`    // Maximum age for temp files
}

// NewDiskMonitor creates a new disk space monitor
func NewDiskMonitor(logger *logx.Logger, config *DiskMonitorConfig) *DiskMonitor {
	if config == nil {
		config = &DiskMonitorConfig{
			CriticalThresholdGB: 1.0, // 1GB critical
			WarningThresholdGB:  2.0, // 2GB warning
			CleanupThresholdGB:  3.0, // 3GB cleanup trigger
			MonitorPaths:        []string{"/tmp", "/var/tmp", "/root/tmp"},
			MaxLogSizeMB:        100, // 100MB max log size
			MaxTempAgeHours:     24,  // 24 hours max temp file age
		}
	}

	return &DiskMonitor{
		logger: logger,
		config: config,
	}
}

// CheckDiskSpace checks disk space for monitored paths
func (dm *DiskMonitor) CheckDiskSpace() (map[string]*DiskSpaceInfo, error) {
	results := make(map[string]*DiskSpaceInfo)

	for _, path := range dm.config.MonitorPaths {
		info, err := dm.getDiskSpaceInfo(path)
		if err != nil {
			dm.logger.Warn("Failed to get disk space info", "path", path, "error", err)
			continue
		}
		results[path] = info
	}

	return results, nil
}

// getDiskSpaceInfo gets disk space information for a specific path
func (dm *DiskMonitor) getDiskSpaceInfo(path string) (*DiskSpaceInfo, error) {
	cmd := exec.Command("df", "-h", path)
	output, err := cmd.Output()
	if err != nil {
		return nil, fmt.Errorf("failed to get disk space for %s: %w", path, err)
	}

	lines := strings.Split(string(output), "\n")
	if len(lines) < 2 {
		return nil, fmt.Errorf("unexpected df output format for %s", path)
	}

	// Parse df output (Filesystem Size Used Avail Use% Mounted on)
	fields := strings.Fields(lines[1])
	if len(fields) < 5 {
		return nil, fmt.Errorf("unexpected df output format for %s: %s", path, lines[1])
	}

	// Parse size values (remove 'G' suffix and convert to float)
	totalStr := strings.TrimSuffix(fields[1], "G")
	usedStr := strings.TrimSuffix(fields[2], "G")
	availStr := strings.TrimSuffix(fields[3], "G")
	usageStr := strings.TrimSuffix(fields[4], "%")

	totalGB, _ := strconv.ParseFloat(totalStr, 64)
	usedGB, _ := strconv.ParseFloat(usedStr, 64)
	availGB, _ := strconv.ParseFloat(availStr, 64)
	usagePercent, _ := strconv.ParseFloat(usageStr, 64)

	// Get inode information
	inodes, err := dm.getInodeInfo(path)
	if err != nil {
		dm.logger.Debug("Failed to get inode info", "path", path, "error", err)
	}

	return &DiskSpaceInfo{
		Path:         path,
		TotalGB:      totalGB,
		UsedGB:       usedGB,
		AvailableGB:  availGB,
		UsagePercent: usagePercent,
		InodesTotal:  inodes.Total,
		InodesUsed:   inodes.Used,
		InodesFree:   inodes.Free,
	}, nil
}

// InodeInfo represents inode information
type InodeInfo struct {
	Total int64
	Used  int64
	Free  int64
}

// getInodeInfo gets inode information for a path
func (dm *DiskMonitor) getInodeInfo(path string) (*InodeInfo, error) {
	cmd := exec.Command("df", "-i", path)
	output, err := cmd.Output()
	if err != nil {
		return nil, err
	}

	lines := strings.Split(string(output), "\n")
	if len(lines) < 2 {
		return nil, fmt.Errorf("unexpected df -i output format")
	}

	fields := strings.Fields(lines[1])
	if len(fields) < 4 {
		return nil, fmt.Errorf("unexpected df -i output format: %s", lines[1])
	}

	total, _ := strconv.ParseInt(fields[1], 10, 64)
	used, _ := strconv.ParseInt(fields[2], 10, 64)
	free, _ := strconv.ParseInt(fields[3], 10, 64)

	return &InodeInfo{
		Total: total,
		Used:  used,
		Free:  free,
	}, nil
}

// CheckDiskSpaceStatus checks if disk space is critical or needs cleanup
func (dm *DiskMonitor) CheckDiskSpaceStatus() (*DiskSpaceStatus, error) {
	diskInfo, err := dm.CheckDiskSpace()
	if err != nil {
		return nil, err
	}

	status := &DiskSpaceStatus{
		Timestamp: time.Now(),
		Status:    "healthy",
		Warnings:  []string{},
		Errors:    []string{},
		DiskInfo:  diskInfo,
	}

	// Check each monitored path
	for path, info := range diskInfo {
		if info.AvailableGB <= dm.config.CriticalThresholdGB {
			status.Status = "critical"
			status.Errors = append(status.Errors,
				fmt.Sprintf("Critical disk space on %s: %.2f GB available", path, info.AvailableGB))
		} else if info.AvailableGB <= dm.config.WarningThresholdGB {
			if status.Status != "critical" {
				status.Status = "warning"
			}
			status.Warnings = append(status.Warnings,
				fmt.Sprintf("Low disk space on %s: %.2f GB available", path, info.AvailableGB))
		}

		// Check if cleanup is needed
		if info.AvailableGB <= dm.config.CleanupThresholdGB {
			status.CleanupNeeded = true
			status.CleanupPaths = append(status.CleanupPaths, path)
		}
	}

	return status, nil
}

// DiskSpaceStatus represents the overall disk space status
type DiskSpaceStatus struct {
	Timestamp     time.Time                 `json:"timestamp"`
	Status        string                    `json:"status"` // healthy, warning, critical
	Warnings      []string                  `json:"warnings"`
	Errors        []string                  `json:"errors"`
	DiskInfo      map[string]*DiskSpaceInfo `json:"disk_info"`
	CleanupNeeded bool                      `json:"cleanup_needed"`
	CleanupPaths  []string                  `json:"cleanup_paths"`
}

// PerformCleanup performs disk space cleanup
func (dm *DiskMonitor) PerformCleanup() (*CleanupResult, error) {
	result := &CleanupResult{
		Timestamp: time.Now(),
		Actions:   []CleanupAction{},
		Errors:    []string{},
	}

	// Check current disk space status
	status, err := dm.CheckDiskSpaceStatus()
	if err != nil {
		return nil, err
	}

	if !status.CleanupNeeded {
		result.Message = "No cleanup needed - sufficient disk space available"
		return result, nil
	}

	// Perform cleanup actions
	for _, path := range status.CleanupPaths {
		actions, err := dm.cleanupPath(path)
		if err != nil {
			result.Errors = append(result.Errors, fmt.Sprintf("Failed to cleanup %s: %v", path, err))
			continue
		}
		result.Actions = append(result.Actions, actions...)
	}

	// Check disk space after cleanup
	afterStatus, err := dm.CheckDiskSpaceStatus()
	if err == nil {
		result.AfterCleanup = afterStatus
	}

	return result, nil
}

// CleanupResult represents the result of a cleanup operation
type CleanupResult struct {
	Timestamp    time.Time        `json:"timestamp"`
	Message      string           `json:"message"`
	Actions      []CleanupAction  `json:"actions"`
	Errors       []string         `json:"errors"`
	AfterCleanup *DiskSpaceStatus `json:"after_cleanup"`
}

// CleanupAction represents a single cleanup action
type CleanupAction struct {
	Type    string `json:"type"`    // file_removed, log_rotated, temp_cleaned
	Path    string `json:"path"`    // Path where action was performed
	Details string `json:"details"` // Details about the action
	SizeMB  int64  `json:"size_mb"` // Size freed in MB
}

// cleanupPath performs cleanup for a specific path
func (dm *DiskMonitor) cleanupPath(path string) ([]CleanupAction, error) {
	var actions []CleanupAction

	// Clean old temporary files
	tempActions, err := dm.cleanupTempFiles(path)
	if err != nil {
		dm.logger.Warn("Failed to cleanup temp files", "path", path, "error", err)
	} else {
		actions = append(actions, tempActions...)
	}

	// Clean old log files
	logActions, err := dm.cleanupLogFiles(path)
	if err != nil {
		dm.logger.Warn("Failed to cleanup log files", "path", path, "error", err)
	} else {
		actions = append(actions, logActions...)
	}

	// Clean core dumps and crash files
	crashActions, err := dm.cleanupCrashFiles(path)
	if err != nil {
		dm.logger.Warn("Failed to cleanup crash files", "path", path, "error", err)
	} else {
		actions = append(actions, crashActions...)
	}

	return actions, nil
}

// cleanupTempFiles removes old temporary files
func (dm *DiskMonitor) cleanupTempFiles(path string) ([]CleanupAction, error) {
	var actions []CleanupAction

	// Find files older than MaxTempAgeHours
	cmd := exec.Command("find", path, "-type", "f", "-mtime", "+1", "-name", "*.tmp", "-o", "-name", "*.temp", "-o", "-name", "*~")
	output, err := cmd.Output()
	if err != nil {
		return actions, err
	}

	files := strings.Split(strings.TrimSpace(string(output)), "\n")
	for _, file := range files {
		if file == "" {
			continue
		}

		// Get file size before removal
		sizeCmd := exec.Command("stat", "-c", "%s", file)
		sizeOutput, err := sizeCmd.Output()
		if err != nil {
			continue
		}
		sizeBytes, _ := strconv.ParseInt(strings.TrimSpace(string(sizeOutput)), 10, 64)
		sizeMB := sizeBytes / (1024 * 1024)

		// Remove the file
		if err := exec.Command("rm", "-f", file).Run(); err != nil {
			continue
		}

		actions = append(actions, CleanupAction{
			Type:    "file_removed",
			Path:    file,
			Details: "Removed old temporary file",
			SizeMB:  sizeMB,
		})
	}

	return actions, nil
}

// cleanupLogFiles rotates or removes old log files
func (dm *DiskMonitor) cleanupLogFiles(path string) ([]CleanupAction, error) {
	var actions []CleanupAction

	// Find large log files
	cmd := exec.Command("find", path, "-type", "f", "-name", "*.log", "-size", "+100M")
	output, err := cmd.Output()
	if err != nil {
		return actions, err
	}

	files := strings.Split(strings.TrimSpace(string(output)), "\n")
	for _, file := range files {
		if file == "" {
			continue
		}

		// Get file size
		sizeCmd := exec.Command("stat", "-c", "%s", file)
		sizeOutput, err := sizeCmd.Output()
		if err != nil {
			continue
		}
		sizeBytes, _ := strconv.ParseInt(strings.TrimSpace(string(sizeOutput)), 10, 64)
		sizeMB := sizeBytes / (1024 * 1024)

		// Truncate the log file
		if err := exec.Command("truncate", "-s", "0", file).Run(); err != nil {
			continue
		}

		actions = append(actions, CleanupAction{
			Type:    "log_rotated",
			Path:    file,
			Details: "Truncated large log file",
			SizeMB:  sizeMB,
		})
	}

	return actions, nil
}

// cleanupCrashFiles removes core dumps and crash files
func (dm *DiskMonitor) cleanupCrashFiles(path string) ([]CleanupAction, error) {
	var actions []CleanupAction

	// Find core dumps and crash files
	cmd := exec.Command("find", path, "-type", "f", "(", "-name", "core*", "-o", "-name", "*.crash", "-o", "-name", "*.dump", ")", "-mtime", "+1")
	output, err := cmd.Output()
	if err != nil {
		return actions, err
	}

	files := strings.Split(strings.TrimSpace(string(output)), "\n")
	for _, file := range files {
		if file == "" {
			continue
		}

		// Get file size before removal
		sizeCmd := exec.Command("stat", "-c", "%s", file)
		sizeOutput, err := sizeCmd.Output()
		if err != nil {
			continue
		}
		sizeBytes, _ := strconv.ParseInt(strings.TrimSpace(string(sizeOutput)), 10, 64)
		sizeMB := sizeBytes / (1024 * 1024)

		// Remove the file
		if err := exec.Command("rm", "-f", file).Run(); err != nil {
			continue
		}

		actions = append(actions, CleanupAction{
			Type:    "file_removed",
			Path:    file,
			Details: "Removed old crash/core dump file",
			SizeMB:  sizeMB,
		})
	}

	return actions, nil
}
