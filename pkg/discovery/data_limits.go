package discovery

import (
	"fmt"
	"os/exec"
	"strconv"
	"strings"
	"time"

	"github.com/markus-lassfolk/autonomy/pkg/logx"
)

// DataLimitConfig represents data usage limits for an interface
type DataLimitConfig struct {
	Enabled          bool    `json:"enabled"`            // Data limits enabled
	DataLimitMB      int     `json:"data_limit_mb"`      // Monthly data limit in MB
	Period           int     `json:"period"`             // Period type (1=monthly)
	RateLimitEnabled bool    `json:"rate_limit_enabled"` // Rate limiting enabled
	RateLimitRxKbps  int     `json:"rate_limit_rx_kbps"` // Download rate limit in Kbps
	RateLimitTxKbps  int     `json:"rate_limit_tx_kbps"` // Upload rate limit in Kbps
	ResetHour        int     `json:"reset_hour"`         // Hour when usage resets
	WarningEnabled   bool    `json:"warning_enabled"`    // Warning notifications enabled
	CurrentUsageMB   float64 `json:"current_usage_mb"`   // Current usage in MB
	UsagePercentage  float64 `json:"usage_percentage"`   // Percentage of limit used
	DaysUntilReset   int     `json:"days_until_reset"`   // Days until usage resets
}

// DataLimitStatus represents the current status of data limits
type DataLimitStatus int

const (
	DataLimitOK       DataLimitStatus = iota
	DataLimitWarning                  // 80-95% used
	DataLimitCritical                 // 95-100% used
	DataLimitExceeded                 // Over 100% used
	DataLimitDisabled                 // No limits configured
)

func (d DataLimitStatus) String() string {
	switch d {
	case DataLimitOK:
		return "ok"
	case DataLimitWarning:
		return "warning"
	case DataLimitCritical:
		return "critical"
	case DataLimitExceeded:
		return "exceeded"
	case DataLimitDisabled:
		return "disabled"
	default:
		return "unknown"
	}
}

// DataLimitManager handles data limit discovery and monitoring
type DataLimitManager struct {
	logger *logx.Logger
}

// NewDataLimitManager creates a new data limit manager
func NewDataLimitManager(logger *logx.Logger) *DataLimitManager {
	return &DataLimitManager{
		logger: logger,
	}
}

// DiscoverDataLimits discovers data limit configurations for all interfaces
func (dlm *DataLimitManager) DiscoverDataLimits() (map[string]*DataLimitConfig, error) {
	limits := make(map[string]*DataLimitConfig)

	// Get quota_limit configuration
	cmd := exec.Command("uci", "show", "quota_limit")
	output, err := cmd.Output()
	if err != nil {
		dlm.logger.Debug("No quota_limit configuration found", "error", err)
		return limits, nil
	}

	interfaces := make(map[string]*DataLimitConfig)
	lines := strings.Split(string(output), "\n")

	for _, line := range lines {
		line = strings.TrimSpace(line)
		if line == "" {
			continue
		}

		// Parse interface definitions
		if strings.Contains(line, "=interface") {
			parts := strings.Split(line, ".")
			if len(parts) >= 2 {
				ifaceName := strings.Split(parts[1], "=")[0]
				interfaces[ifaceName] = &DataLimitConfig{}
			}
		}

		// Parse interface properties
		for ifaceName := range interfaces {
			prefix := fmt.Sprintf("quota_limit.%s.", ifaceName)
			if strings.HasPrefix(line, prefix) {
				parts := strings.Split(line, "=")
				if len(parts) == 2 {
					key := strings.TrimPrefix(parts[0], prefix)
					value := strings.Trim(parts[1], "'\"")

					switch key {
					case "enabled":
						interfaces[ifaceName].Enabled = (value == "1")
					case "data_limit":
						if limit, err := strconv.Atoi(value); err == nil {
							interfaces[ifaceName].DataLimitMB = limit
						}
					case "period":
						if period, err := strconv.Atoi(value); err == nil {
							interfaces[ifaceName].Period = period
						}
					case "enable_rate_limit":
						interfaces[ifaceName].RateLimitEnabled = (value == "1")
					case "rate_limit_rx":
						if rate, err := strconv.Atoi(value); err == nil {
							interfaces[ifaceName].RateLimitRxKbps = rate
						}
					case "rate_limit_tx":
						if rate, err := strconv.Atoi(value); err == nil {
							interfaces[ifaceName].RateLimitTxKbps = rate
						}
					case "reset_hour":
						if hour, err := strconv.Atoi(value); err == nil {
							interfaces[ifaceName].ResetHour = hour
						}
					case "enable_warning":
						interfaces[ifaceName].WarningEnabled = (value == "1")
					}
				}
			}
		}
	}

	// Get current usage for each interface
	for ifaceName, config := range interfaces {
		if config.Enabled {
			usage, err := dlm.getCurrentUsage(ifaceName)
			if err != nil {
				dlm.logger.Debug("Failed to get current usage", "interface", ifaceName, "error", err)
			} else {
				config.CurrentUsageMB = usage
				if config.DataLimitMB > 0 {
					config.UsagePercentage = (usage / float64(config.DataLimitMB)) * 100
				}
				config.DaysUntilReset = dlm.getDaysUntilReset(config.ResetHour)
			}
		}
		limits[ifaceName] = config
	}

	return limits, nil
}

// getCurrentUsage gets current data usage for an interface in MB
func (dlm *DataLimitManager) getCurrentUsage(ifaceName string) (float64, error) {
	// Try to get usage from /proc/net/dev
	cmd := exec.Command("cat", "/proc/net/dev")
	output, err := cmd.Output()
	if err != nil {
		return 0, fmt.Errorf("failed to read /proc/net/dev: %w", err)
	}

	// Look for the physical interface (qmimux0 for mob1s1a1, etc.)
	var physicalIface string
	switch ifaceName {
	case "mob1s1a1":
		physicalIface = "qmimux0"
	case "mob1s2a1":
		physicalIface = "qmimux1"
	default:
		return 0, fmt.Errorf("unknown cellular interface: %s", ifaceName)
	}

	lines := strings.Split(string(output), "\n")
	for _, line := range lines {
		if strings.Contains(line, physicalIface+":") {
			fields := strings.Fields(line)
			if len(fields) >= 10 {
				// bytes received (field 1) + bytes transmitted (field 9)
				rxBytes, err1 := strconv.ParseFloat(fields[1], 64)
				txBytes, err2 := strconv.ParseFloat(fields[9], 64)
				if err1 == nil && err2 == nil {
					totalBytes := rxBytes + txBytes
					totalMB := totalBytes / (1024 * 1024) // Convert to MB
					return totalMB, nil
				}
			}
		}
	}

	return 0, fmt.Errorf("interface %s not found in /proc/net/dev", physicalIface)
}

// getDaysUntilReset calculates days until monthly reset
func (dlm *DataLimitManager) getDaysUntilReset(resetHour int) int {
	now := time.Now()

	// Calculate next reset time (next month at reset hour)
	nextMonth := now.AddDate(0, 1, 0)
	nextReset := time.Date(nextMonth.Year(), nextMonth.Month(), 1, resetHour, 0, 0, 0, now.Location())

	// If we haven't passed this month's reset time yet, use this month
	thisMonthReset := time.Date(now.Year(), now.Month(), 1, resetHour, 0, 0, 0, now.Location())
	if now.Before(thisMonthReset) {
		nextReset = thisMonthReset
	}

	duration := nextReset.Sub(now)
	return int(duration.Hours() / 24)
}

// GetDataLimitStatus determines the status of data limits for an interface
func (dlm *DataLimitManager) GetDataLimitStatus(config *DataLimitConfig) DataLimitStatus {
	if !config.Enabled || config.DataLimitMB == 0 {
		return DataLimitDisabled
	}

	if config.UsagePercentage >= 100 {
		return DataLimitExceeded
	} else if config.UsagePercentage >= 95 {
		return DataLimitCritical
	} else if config.UsagePercentage >= 80 {
		return DataLimitWarning
	}

	return DataLimitOK
}

// ShouldAvoidInterface determines if an interface should be avoided due to data limits
func (dlm *DataLimitManager) ShouldAvoidInterface(config *DataLimitConfig, urgency string) bool {
	if !config.Enabled {
		return false // No limits, no restrictions
	}

	status := dlm.GetDataLimitStatus(config)

	switch urgency {
	case "emergency":
		// In emergency, only avoid if completely exceeded
		return status == DataLimitExceeded
	case "high":
		// High priority traffic, avoid if critical or exceeded
		return status == DataLimitCritical || status == DataLimitExceeded
	case "normal":
		// Normal traffic, avoid if warning, critical, or exceeded
		return status == DataLimitWarning || status == DataLimitCritical || status == DataLimitExceeded
	default:
		// Conservative approach
		return status != DataLimitOK && status != DataLimitDisabled
	}
}

// GetFailoverPriority calculates failover priority based on data limits
// Lower numbers = higher priority
func (dlm *DataLimitManager) GetFailoverPriority(config *DataLimitConfig) int {
	if !config.Enabled {
		return 1 // No limits = high priority
	}

	status := dlm.GetDataLimitStatus(config)

	switch status {
	case DataLimitOK:
		return 1 // High priority
	case DataLimitWarning:
		return 3 // Medium priority
	case DataLimitCritical:
		return 5 // Low priority
	case DataLimitExceeded:
		return 10 // Very low priority
	default:
		return 2 // Default priority
	}
}
