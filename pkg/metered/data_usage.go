package metered

import (
	"context"
	"encoding/json"
	"fmt"
	"os/exec"
	"strconv"
	"strings"
	"time"

	"github.com/markus-lassfolk/autonomy/pkg"
)

// DataUsageMonitor monitors data usage for interfaces with limits
type DataUsageMonitor struct {
	manager          *Manager
	enhancedDetector *EnhancedRutosDataLimitDetector // Enhanced RUTOS-native detection
}

// DataUsageInfo represents data usage information for an interface
type DataUsageInfo struct {
	Interface    string    `json:"interface"`
	TotalBytes   int64     `json:"total_bytes"`
	LimitBytes   int64     `json:"limit_bytes"`
	UsagePercent float64   `json:"usage_percent"`
	Period       string    `json:"period"` // "daily", "monthly", "billing_cycle"
	ResetDate    time.Time `json:"reset_date"`
	LastUpdated  time.Time `json:"last_updated"`
}

// NewDataUsageMonitor creates a new data usage monitor
func NewDataUsageMonitor(manager *Manager) *DataUsageMonitor {
	return &DataUsageMonitor{
		manager:          manager,
		enhancedDetector: NewEnhancedRutosDataLimitDetector(manager.logger),
	}
}

// MonitorDataUsage monitors data usage for the current member
func (d *DataUsageMonitor) MonitorDataUsage(member *pkg.Member) error {
	if member == nil {
		return fmt.Errorf("member cannot be nil")
	}

	// Get data usage information
	usageInfo, err := d.GetDataUsageInfo(member)
	if err != nil {
		return fmt.Errorf("failed to get data usage info: %w", err)
	}

	if usageInfo == nil {
		// No data limits configured for this interface
		return nil
	}

	d.manager.logger.Debug("Monitoring data usage",
		"interface", member.Iface,
		"usage_percent", usageInfo.UsagePercent,
		"total_bytes", usageInfo.TotalBytes,
		"limit_bytes", usageInfo.LimitBytes)

	// Trigger metered mode update based on usage
	return d.manager.OnDataUsageUpdate(member, usageInfo.UsagePercent)
}

// GetDataUsageInfo gets comprehensive data usage information for an interface
func (d *DataUsageMonitor) GetDataUsageInfo(member *pkg.Member) (*DataUsageInfo, error) {
	ctx := context.Background()

	// First try enhanced RUTOS-native detection
	if usageInfo, err := d.enhancedDetector.GetDataLimitInfo(ctx, member); err == nil && usageInfo != nil {
		d.manager.logger.Debug("Retrieved data usage info via enhanced RUTOS detection",
			"interface", member.Iface,
			"usage_percent", usageInfo.UsagePercent,
			"limit_mb", usageInfo.LimitBytes/(1024*1024))
		return usageInfo, nil
	}

	// Fallback to legacy method
	d.manager.logger.Debug("Falling back to legacy data limit detection", "interface", member.Iface)

	// Check if interface has data limits configured
	limitInfo, err := d.getDataLimitInfo(member)
	if err != nil || limitInfo == nil {
		return nil, nil // No limits configured
	}

	// Get current usage
	currentUsage, err := d.getCurrentUsage(member)
	if err != nil {
		return nil, fmt.Errorf("failed to get current usage: %w", err)
	}

	// Calculate usage percentage
	usagePercent := 0.0
	if limitInfo.LimitBytes > 0 {
		usagePercent = (float64(currentUsage) / float64(limitInfo.LimitBytes)) * 100.0
	}

	return &DataUsageInfo{
		Interface:    member.Iface,
		TotalBytes:   currentUsage,
		LimitBytes:   limitInfo.LimitBytes,
		UsagePercent: usagePercent,
		Period:       limitInfo.Period,
		ResetDate:    limitInfo.ResetDate,
		LastUpdated:  time.Now(),
	}, nil
}

// DataLimitInfo represents data limit configuration
type DataLimitInfo struct {
	LimitBytes int64     `json:"limit_bytes"`
	Period     string    `json:"period"`
	ResetDate  time.Time `json:"reset_date"`
}

// getDataLimitInfo gets data limit configuration for an interface
func (d *DataUsageMonitor) getDataLimitInfo(member *pkg.Member) (*DataLimitInfo, error) {
	// Try different UCI keys for data limits
	limitKeys := []struct {
		key    string
		period string
	}{
		{fmt.Sprintf("network.%s.data_limit_daily_bytes", member.Iface), "daily"},
		{fmt.Sprintf("network.%s.data_limit_monthly_bytes", member.Iface), "monthly"},
		{fmt.Sprintf("network.%s.data_limit", member.Iface), "monthly"}, // Generic limit
	}

	for _, limitKey := range limitKeys {
		cmd := exec.Command("uci", "-q", "get", limitKey.key)
		output, err := cmd.Output()
		if err != nil {
			continue // Try next key
		}

		limitStr := strings.TrimSpace(string(output))
		if limitStr == "" {
			continue
		}

		// Parse limit value (could be in bytes or with units like "10GB")
		limitBytes, err := d.parseLimitValue(limitStr)
		if err != nil {
			d.manager.logger.Warn("Failed to parse data limit value",
				"key", limitKey.key, "value", limitStr, "error", err)
			continue
		}

		// Calculate reset date based on period
		resetDate := d.calculateResetDate(limitKey.period)

		return &DataLimitInfo{
			LimitBytes: limitBytes,
			Period:     limitKey.period,
			ResetDate:  resetDate,
		}, nil
	}

	return nil, nil // No limits found
}

// parseLimitValue parses a data limit value (supports units like GB, MB, etc.)
func (d *DataUsageMonitor) parseLimitValue(value string) (int64, error) {
	value = strings.ToUpper(strings.TrimSpace(value))

	// If it's just a number, assume bytes
	if bytes, err := strconv.ParseInt(value, 10, 64); err == nil {
		return bytes, nil
	}

	// Parse with units
	multipliers := map[string]int64{
		"B":  1,
		"KB": 1024,
		"MB": 1024 * 1024,
		"GB": 1024 * 1024 * 1024,
		"TB": 1024 * 1024 * 1024 * 1024,
	}

	for unit, multiplier := range multipliers {
		if strings.HasSuffix(value, unit) {
			numStr := strings.TrimSuffix(value, unit)
			if num, err := strconv.ParseFloat(numStr, 64); err == nil {
				return int64(num * float64(multiplier)), nil
			}
		}
	}

	return 0, fmt.Errorf("invalid limit value format: %s", value)
}

// calculateResetDate calculates when the data usage resets based on period
func (d *DataUsageMonitor) calculateResetDate(period string) time.Time {
	now := time.Now()

	switch period {
	case "daily":
		// Reset at midnight
		return time.Date(now.Year(), now.Month(), now.Day()+1, 0, 0, 0, 0, now.Location())

	case "monthly":
		// Reset on first day of next month
		if now.Month() == 12 {
			return time.Date(now.Year()+1, 1, 1, 0, 0, 0, 0, now.Location())
		}
		return time.Date(now.Year(), now.Month()+1, 1, 0, 0, 0, 0, now.Location())

	default:
		// Default to monthly
		if now.Month() == 12 {
			return time.Date(now.Year()+1, 1, 1, 0, 0, 0, 0, now.Location())
		}
		return time.Date(now.Year(), now.Month()+1, 1, 0, 0, 0, 0, now.Location())
	}
}

// getCurrentUsage gets current data usage for an interface
func (d *DataUsageMonitor) getCurrentUsage(member *pkg.Member) (int64, error) {
	// Try multiple methods to get usage data

	// Method 1: Try nlbw (Netlink Bandwidth Monitor)
	if usage, err := d.getNlbwUsage(member); err == nil {
		return usage, nil
	}

	// Method 2: Try vnstat
	if usage, err := d.getVnstatUsage(member); err == nil {
		return usage, nil
	}

	// Method 3: Try /proc/net/dev
	if usage, err := d.getProcNetDevUsage(member); err == nil {
		return usage, nil
	}

	// Method 4: Try ubus network.interface statistics
	if usage, err := d.getUbusInterfaceUsage(member); err == nil {
		return usage, nil
	}

	return 0, fmt.Errorf("no data usage monitoring method available for interface %s", member.Iface)
}

// getNlbwUsage gets usage from nlbw (Netlink Bandwidth Monitor)
func (d *DataUsageMonitor) getNlbwUsage(member *pkg.Member) (int64, error) {
	// Check if nlbw is available
	if _, err := exec.LookPath("nlbw"); err != nil {
		return 0, fmt.Errorf("nlbw not available")
	}

	// Get current month's usage
	cmd := exec.Command("nlbw", "-c", "show", "-i", member.Iface)
	output, err := cmd.Output()
	if err != nil {
		return 0, fmt.Errorf("nlbw command failed: %w", err)
	}

	// Parse nlbw output (format varies, this is a basic implementation)
	lines := strings.Split(string(output), "\n")
	for _, line := range lines {
		if strings.Contains(line, member.Iface) {
			fields := strings.Fields(line)
			if len(fields) >= 3 {
				// Assuming format: interface rx_bytes tx_bytes
				rxBytes, err1 := strconv.ParseInt(fields[1], 10, 64)
				txBytes, err2 := strconv.ParseInt(fields[2], 10, 64)
				if err1 == nil && err2 == nil {
					return rxBytes + txBytes, nil
				}
			}
		}
	}

	return 0, fmt.Errorf("failed to parse nlbw output")
}

// getVnstatUsage gets usage from vnstat
func (d *DataUsageMonitor) getVnstatUsage(member *pkg.Member) (int64, error) {
	// Check if vnstat is available
	if _, err := exec.LookPath("vnstat"); err != nil {
		return 0, fmt.Errorf("vnstat not available")
	}

	// Get current month's usage in JSON format
	cmd := exec.Command("vnstat", "-i", member.Iface, "--json", "m")
	output, err := cmd.Output()
	if err != nil {
		return 0, fmt.Errorf("vnstat command failed: %w", err)
	}

	// Parse vnstat JSON output
	var vnstatData map[string]interface{}
	if err := json.Unmarshal(output, &vnstatData); err != nil {
		return 0, fmt.Errorf("failed to parse vnstat JSON: %w", err)
	}

	// Extract current month's data
	if interfaces, ok := vnstatData["interfaces"].([]interface{}); ok && len(interfaces) > 0 {
		if iface, ok := interfaces[0].(map[string]interface{}); ok {
			if traffic, ok := iface["traffic"].(map[string]interface{}); ok {
				if months, ok := traffic["month"].([]interface{}); ok && len(months) > 0 {
					if currentMonth, ok := months[len(months)-1].(map[string]interface{}); ok {
						rx, _ := currentMonth["rx"].(float64)
						tx, _ := currentMonth["tx"].(float64)
						return int64(rx + tx), nil
					}
				}
			}
		}
	}

	return 0, fmt.Errorf("failed to extract usage from vnstat data")
}

// getProcNetDevUsage gets usage from /proc/net/dev
func (d *DataUsageMonitor) getProcNetDevUsage(member *pkg.Member) (int64, error) {
	cmd := exec.Command("cat", "/proc/net/dev")
	output, err := cmd.Output()
	if err != nil {
		return 0, fmt.Errorf("failed to read /proc/net/dev: %w", err)
	}

	lines := strings.Split(string(output), "\n")
	for _, line := range lines {
		if strings.Contains(line, member.Iface+":") {
			// Parse /proc/net/dev format
			parts := strings.Fields(strings.TrimSpace(line))
			if len(parts) >= 10 {
				// Format: interface: rx_bytes rx_packets ... tx_bytes tx_packets ...
				rxBytes, err1 := strconv.ParseInt(parts[1], 10, 64)
				txBytes, err2 := strconv.ParseInt(parts[9], 10, 64)
				if err1 == nil && err2 == nil {
					return rxBytes + txBytes, nil
				}
			}
		}
	}

	return 0, fmt.Errorf("interface %s not found in /proc/net/dev", member.Iface)
}

// getUbusInterfaceUsage gets usage from ubus network.interface statistics
func (d *DataUsageMonitor) getUbusInterfaceUsage(member *pkg.Member) (int64, error) {
	cmd := exec.Command("ubus", "call", "network.interface."+member.Iface, "status")
	output, err := cmd.Output()
	if err != nil {
		return 0, fmt.Errorf("ubus interface status failed: %w", err)
	}

	var status map[string]interface{}
	if err := json.Unmarshal(output, &status); err != nil {
		return 0, fmt.Errorf("failed to parse ubus status: %w", err)
	}

	// Extract statistics if available
	if statistics, ok := status["statistics"].(map[string]interface{}); ok {
		rxBytes, _ := statistics["rx_bytes"].(float64)
		txBytes, _ := statistics["tx_bytes"].(float64)
		return int64(rxBytes + txBytes), nil
	}

	return 0, fmt.Errorf("no statistics available in ubus interface status")
}

// GetAllInterfaceUsage gets data usage for all interfaces with limits
func (d *DataUsageMonitor) GetAllInterfaceUsage() (map[string]*DataUsageInfo, error) {
	usage := make(map[string]*DataUsageInfo)

	// Get list of network interfaces from UCI
	cmd := exec.Command("uci", "show", "network")
	output, err := cmd.Output()
	if err != nil {
		return nil, fmt.Errorf("failed to get network interfaces: %w", err)
	}

	// Parse network interfaces and check for data limits
	interfaces := make(map[string]bool)
	lines := strings.Split(string(output), "\n")

	for _, line := range lines {
		if strings.Contains(line, "=interface") {
			parts := strings.Split(line, ".")
			if len(parts) >= 2 {
				ifaceName := parts[1]
				interfaces[ifaceName] = true
			}
		}
	}

	// Check each interface for data limits and usage
	for ifaceName := range interfaces {
		member := &pkg.Member{
			Name:  ifaceName,
			Iface: ifaceName,
		}

		if usageInfo, err := d.GetDataUsageInfo(member); err == nil && usageInfo != nil {
			usage[ifaceName] = usageInfo
		}
	}

	return usage, nil
}

// ResetUsageCounters resets usage counters for an interface (if supported)
func (d *DataUsageMonitor) ResetUsageCounters(member *pkg.Member) error {
	// This would depend on the monitoring system in use
	// For nlbw, vnstat, etc., reset commands vary

	d.manager.logger.Info("Resetting usage counters", "interface", member.Iface)

	// Try to reset vnstat if available
	if _, err := exec.LookPath("vnstat"); err == nil {
		cmd := exec.Command("vnstat", "-i", member.Iface, "--delete")
		if err := cmd.Run(); err != nil {
			d.manager.logger.Warn("Failed to reset vnstat counters",
				"interface", member.Iface, "error", err)
		}
	}

	// Note: /proc/net/dev counters reset on interface restart
	// nlbw counters may need specific reset commands

	return nil
}
