package metered

import (
	"context"
	"fmt"
	"time"

	"github.com/markus-lassfolk/autonomy/pkg/logx"
)

// DataLimitUbusAPI provides ubus API integration for data limit monitoring
type DataLimitUbusAPI struct {
	detector *EnhancedRutosDataLimitDetector
	logger   *logx.Logger
}

// NewDataLimitUbusAPI creates a new data limit ubus API
func NewDataLimitUbusAPI(logger *logx.Logger) *DataLimitUbusAPI {
	return &DataLimitUbusAPI{
		detector: NewEnhancedRutosDataLimitDetector(logger),
		logger:   logger,
	}
}

// DataLimitStatusResponse represents the response for data limit status
type DataLimitStatusResponse struct {
	Success    bool                               `json:"success"`
	Timestamp  string                             `json:"timestamp"`
	Interfaces map[string]*DataLimitInterfaceInfo `json:"interfaces"`
	Summary    *DataLimitSummary                  `json:"summary"`
}

// DataLimitInterfaceInfo represents data limit information for a single interface
type DataLimitInterfaceInfo struct {
	Interface      string  `json:"interface"`
	Enabled        bool    `json:"enabled"`
	Period         string  `json:"period"`
	LimitMB        int64   `json:"limit_mb"`
	UsedMB         int64   `json:"used_mb"`
	UsagePercent   float64 `json:"usage_percent"`
	Status         string  `json:"status"` // "ok", "warning", "critical", "over_limit"
	ClearDue       string  `json:"clear_due"`
	SMSWarning     bool    `json:"sms_warning"`
	DaysUntilReset int     `json:"days_until_reset"`
}

// DataLimitSummary provides a summary of all interfaces
type DataLimitSummary struct {
	TotalInterfaces     int     `json:"total_interfaces"`
	EnabledInterfaces   int     `json:"enabled_interfaces"`
	WarningInterfaces   int     `json:"warning_interfaces"`
	CriticalInterfaces  int     `json:"critical_interfaces"`
	OverLimitInterfaces int     `json:"over_limit_interfaces"`
	TotalUsageMB        int64   `json:"total_usage_mb"`
	TotalLimitMB        int64   `json:"total_limit_mb"`
	AverageUsagePercent float64 `json:"average_usage_percent"`
}

// GetDataLimitStatus gets comprehensive data limit status for all mobile interfaces
func (api *DataLimitUbusAPI) GetDataLimitStatus(ctx context.Context) (*DataLimitStatusResponse, error) {
	// Get all mobile interfaces with data limits
	rules, err := api.detector.GetAllMobileInterfaces(ctx)
	if err != nil {
		return &DataLimitStatusResponse{
			Success:   false,
			Timestamp: time.Now().UTC().Format(time.RFC3339),
		}, fmt.Errorf("failed to get mobile interfaces: %w", err)
	}

	interfaces := make(map[string]*DataLimitInterfaceInfo)
	summary := &DataLimitSummary{}

	for ifname, rule := range rules {
		info := &DataLimitInterfaceInfo{
			Interface:      rule.Ifname,
			Enabled:        rule.Enabled,
			Period:         rule.Period,
			LimitMB:        rule.LimitMB,
			UsedMB:         rule.UsedMB,
			UsagePercent:   rule.UsagePercent,
			Status:         api.calculateStatus(rule.UsagePercent, rule.Enabled),
			ClearDue:       rule.ClearDue,
			SMSWarning:     rule.SMSWarning,
			DaysUntilReset: api.calculateDaysUntilReset(rule.ResetDate),
		}

		interfaces[ifname] = info

		// Update summary
		summary.TotalInterfaces++
		if rule.Enabled {
			summary.EnabledInterfaces++
			summary.TotalUsageMB += rule.UsedMB
			summary.TotalLimitMB += rule.LimitMB

			switch info.Status {
			case "warning":
				summary.WarningInterfaces++
			case "critical":
				summary.CriticalInterfaces++
			case "over_limit":
				summary.OverLimitInterfaces++
			}
		}
	}

	// Calculate average usage percentage
	if summary.EnabledInterfaces > 0 && summary.TotalLimitMB > 0 {
		summary.AverageUsagePercent = (float64(summary.TotalUsageMB) / float64(summary.TotalLimitMB)) * 100.0
	}

	return &DataLimitStatusResponse{
		Success:    true,
		Timestamp:  time.Now().UTC().Format(time.RFC3339),
		Interfaces: interfaces,
		Summary:    summary,
	}, nil
}

// calculateStatus determines the status based on usage percentage
func (api *DataLimitUbusAPI) calculateStatus(usagePercent float64, enabled bool) string {
	if !enabled {
		return "disabled"
	}

	if usagePercent >= 100.0 {
		return "over_limit"
	} else if usagePercent >= 90.0 {
		return "critical"
	} else if usagePercent >= 75.0 {
		return "warning"
	}

	return "ok"
}

// calculateDaysUntilReset calculates days until the data usage resets
func (api *DataLimitUbusAPI) calculateDaysUntilReset(resetDate time.Time) int {
	if resetDate.IsZero() {
		return -1 // Unknown
	}

	now := time.Now()
	if resetDate.Before(now) {
		return 0 // Already passed
	}

	duration := resetDate.Sub(now)
	return int(duration.Hours() / 24)
}

// GetInterfaceDataLimit gets data limit information for a specific interface
func (api *DataLimitUbusAPI) GetInterfaceDataLimit(ctx context.Context, ifname string) (*DataLimitInterfaceInfo, error) {
	rule, err := api.detector.getDataLimitViaUbusService(ctx, ifname)
	if err != nil {
		// Try fallback method
		if rule, err = api.detector.getDataLimitViaUCIFallback(ctx, ifname); err != nil {
			return nil, fmt.Errorf("failed to get data limit for interface %s: %w", ifname, err)
		}
	}

	return &DataLimitInterfaceInfo{
		Interface:      rule.Ifname,
		Enabled:        rule.Enabled,
		Period:         rule.Period,
		LimitMB:        rule.LimitMB,
		UsedMB:         rule.UsedMB,
		UsagePercent:   rule.UsagePercent,
		Status:         api.calculateStatus(rule.UsagePercent, rule.Enabled),
		ClearDue:       rule.ClearDue,
		SMSWarning:     rule.SMSWarning,
		DaysUntilReset: api.calculateDaysUntilReset(rule.ResetDate),
	}, nil
}

// GenerateDataLimitReport generates a formatted report for logging or display
func (api *DataLimitUbusAPI) GenerateDataLimitReport(ctx context.Context) (string, error) {
	status, err := api.GetDataLimitStatus(ctx)
	if err != nil {
		return "", err
	}

	if !status.Success {
		return "Failed to retrieve data limit status", nil
	}

	report := fmt.Sprintf("📊 Data Limit Status Report - %s\n", status.Timestamp)
	report += "═══════════════════════════════════════\n"
	report += fmt.Sprintf("Summary: %d total, %d enabled, %d warnings, %d critical, %d over limit\n",
		status.Summary.TotalInterfaces,
		status.Summary.EnabledInterfaces,
		status.Summary.WarningInterfaces,
		status.Summary.CriticalInterfaces,
		status.Summary.OverLimitInterfaces)

	if status.Summary.TotalLimitMB > 0 {
		report += fmt.Sprintf("Total Usage: %.1f%% (%d MB / %d MB)\n",
			status.Summary.AverageUsagePercent,
			status.Summary.TotalUsageMB,
			status.Summary.TotalLimitMB)
	}

	report += "───────────────────────────────────────\n"

	for ifname, info := range status.Interfaces {
		statusIcon := "✅"
		switch info.Status {
		case "warning":
			statusIcon = "⚠️"
		case "critical":
			statusIcon = "🚨"
		case "over_limit":
			statusIcon = "🚫"
		case "disabled":
			statusIcon = "⏸️"
		}

		resetInfo := "Unknown"
		if info.DaysUntilReset >= 0 {
			resetInfo = fmt.Sprintf("%d days", info.DaysUntilReset)
		}

		report += fmt.Sprintf("%s %s: %.1f%% (%d/%d MB) - %s period, resets in %s\n",
			statusIcon, ifname, info.UsagePercent, info.UsedMB, info.LimitMB,
			info.Period, resetInfo)
	}

	return report, nil
}
