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
	"github.com/markus-lassfolk/autonomy/pkg/logx"
)

// EnhancedRutosDataLimitDetector provides native RUTOS data limit detection
type EnhancedRutosDataLimitDetector struct {
	logger         *logx.Logger
	dataLimitObj   string // Cached ubus data limit object name
	lastDiscovery  time.Time
	discoveryCache map[string]*RutosDataLimitRule
}

// RutosDataLimitRule represents a RUTOS data limit rule
type RutosDataLimitRule struct {
	Ifname       string    `json:"ifname"`        // e.g., mob1s1a1, mob1s2a1
	Enabled      bool      `json:"enabled"`       // true/false
	Period       string    `json:"period"`        // day|week|month|custom
	LimitMB      int64     `json:"limit_mb"`      // Limit in MB
	LimitBytes   int64     `json:"limit_bytes"`   // Limit in bytes (calculated)
	UsedMB       int64     `json:"used_mb"`       // Used data in MB
	UsedBytes    int64     `json:"used_bytes"`    // Used data in bytes
	ClearDue     string    `json:"clear_due"`     // Next reset timestamp
	SMSWarning   bool      `json:"sms_warning"`   // SMS warning enabled
	ResetDate    time.Time `json:"reset_date"`    // Parsed reset date
	UsagePercent float64   `json:"usage_percent"` // Calculated usage percentage
}

// RutosInterfaceStats represents network interface statistics from RUTOS
type RutosInterfaceStats struct {
	Interface string `json:"interface"`
	RXBytes   uint64 `json:"rx_bytes"`
	TXBytes   uint64 `json:"tx_bytes"`
	TotalMB   uint64 `json:"total_mb"`
}

// NewEnhancedRutosDataLimitDetector creates a new enhanced RUTOS data limit detector
func NewEnhancedRutosDataLimitDetector(logger *logx.Logger) *EnhancedRutosDataLimitDetector {
	return &EnhancedRutosDataLimitDetector{
		logger:         logger,
		discoveryCache: make(map[string]*RutosDataLimitRule),
	}
}

// GetDataLimitInfo gets comprehensive data limit information using native RUTOS methods
func (e *EnhancedRutosDataLimitDetector) GetDataLimitInfo(ctx context.Context, member *pkg.Member) (*DataUsageInfo, error) {
	// First try the preferred native ubus data_limit object approach
	if rule, err := e.getDataLimitViaUbusService(ctx, member.Iface); err == nil && rule != nil {
		e.logger.Debug("Retrieved data limit via native ubus service",
			"interface", member.Iface,
			"enabled", rule.Enabled,
			"limit_mb", rule.LimitMB,
			"used_mb", rule.UsedMB,
			"usage_percent", rule.UsagePercent)

		return e.convertRuleToDataUsageInfo(rule), nil
	}

	// Fallback to portable UCI + runtime counters approach
	if rule, err := e.getDataLimitViaUCIFallback(ctx, member.Iface); err == nil && rule != nil {
		e.logger.Debug("Retrieved data limit via UCI fallback",
			"interface", member.Iface,
			"enabled", rule.Enabled,
			"limit_mb", rule.LimitMB,
			"used_mb", rule.UsedMB,
			"usage_percent", rule.UsagePercent)

		return e.convertRuleToDataUsageInfo(rule), nil
	}

	e.logger.Debug("No data limits configured for interface", "interface", member.Iface)
	return nil, nil // No limits found
}

// getDataLimitViaUbusService gets data limits using the native RUTOS ubus data_limit service
func (e *EnhancedRutosDataLimitDetector) getDataLimitViaUbusService(ctx context.Context, ifname string) (*RutosDataLimitRule, error) {
	// Discover data limit ubus object if not cached or cache is old
	if e.dataLimitObj == "" || time.Since(e.lastDiscovery) > 5*time.Minute {
		if err := e.discoverDataLimitObject(ctx); err != nil {
			return nil, fmt.Errorf("failed to discover data limit ubus object: %w", err)
		}
	}

	if e.dataLimitObj == "" {
		return nil, fmt.Errorf("no data limit ubus object available")
	}

	// Call the data limit service
	cmd := exec.CommandContext(ctx, "ubus", "-S", "call", e.dataLimitObj, "status", "{}")
	output, err := cmd.Output()
	if err != nil {
		return nil, fmt.Errorf("failed to call data limit service: %w", err)
	}

	// Parse the response
	var response struct {
		Rules []RutosDataLimitRule `json:"rules"`
	}
	if err := json.Unmarshal(output, &response); err != nil {
		return nil, fmt.Errorf("failed to parse data limit response: %w", err)
	}

	// Find the rule for our interface
	for _, rule := range response.Rules {
		if rule.Ifname == ifname {
			// Calculate additional fields
			rule.LimitBytes = rule.LimitMB * 1024 * 1024
			rule.UsedBytes = rule.UsedMB * 1024 * 1024
			if rule.LimitMB > 0 {
				rule.UsagePercent = (float64(rule.UsedMB) / float64(rule.LimitMB)) * 100.0
			}

			// Parse reset date
			if rule.ClearDue != "" {
				if resetDate, err := time.Parse(time.RFC3339, rule.ClearDue); err == nil {
					rule.ResetDate = resetDate
				}
			}

			return &rule, nil
		}
	}

	return nil, fmt.Errorf("no data limit rule found for interface %s", ifname)
}

// getDataLimitViaUCIFallback gets data limits using the portable UCI + runtime counters approach
func (e *EnhancedRutosDataLimitDetector) getDataLimitViaUCIFallback(ctx context.Context, ifname string) (*RutosDataLimitRule, error) {
	// Get configured limits from UCI
	limitConfig, err := e.getDataLimitConfigFromUCI(ctx)
	if err != nil {
		return nil, fmt.Errorf("failed to get UCI data limit config: %w", err)
	}

	// Find configuration for our interface
	var rule *RutosDataLimitRule
	for _, config := range limitConfig {
		if config.Ifname == ifname {
			rule = &config
			break
		}
	}

	if rule == nil {
		return nil, fmt.Errorf("no data limit configuration found for interface %s", ifname)
	}

	// Get current usage from interface statistics
	stats, err := e.getInterfaceStatistics(ctx, ifname)
	if err != nil {
		e.logger.Warn("Failed to get interface statistics, using configured values",
			"interface", ifname, "error", err)
	} else {
		rule.UsedBytes = int64(stats.RXBytes + stats.TXBytes)
		rule.UsedMB = int64(stats.TotalMB)
	}

	// Calculate usage percentage
	if rule.LimitMB > 0 {
		rule.UsagePercent = (float64(rule.UsedMB) / float64(rule.LimitMB)) * 100.0
	}

	return rule, nil
}

// discoverDataLimitObject discovers the ubus data limit service object
func (e *EnhancedRutosDataLimitDetector) discoverDataLimitObject(ctx context.Context) error {
	// Use the shell command from your example to find the data limit object
	cmd := exec.CommandContext(ctx, "sh", "-c", `ubus list | grep -E '(^|\.)(data_?limit)$' | head -n1`)
	output, err := cmd.Output()
	if err != nil {
		return fmt.Errorf("failed to discover data limit object: %w", err)
	}

	e.dataLimitObj = strings.TrimSpace(string(output))
	e.lastDiscovery = time.Now()

	if e.dataLimitObj != "" {
		e.logger.Info("Discovered native RUTOS data limit service", "object", e.dataLimitObj)
	} else {
		e.logger.Debug("No native data limit ubus object found, will use fallback method")
	}

	return nil
}

// getDataLimitConfigFromUCI gets data limit configuration from UCI
func (e *EnhancedRutosDataLimitDetector) getDataLimitConfigFromUCI(ctx context.Context) ([]RutosDataLimitRule, error) {
	// Get UCI data_limit configuration
	cmd := exec.CommandContext(ctx, "ubus", "-S", "call", "uci", "get", `{"config":"data_limit"}`)
	output, err := cmd.Output()
	if err != nil {
		return nil, fmt.Errorf("failed to get UCI data_limit config: %w", err)
	}

	var uciResponse struct {
		Values map[string]map[string]interface{} `json:"values"`
	}
	if err := json.Unmarshal(output, &uciResponse); err != nil {
		return nil, fmt.Errorf("failed to parse UCI response: %w", err)
	}

	var rules []RutosDataLimitRule
	for _, config := range uciResponse.Values {
		rule := RutosDataLimitRule{}

		// Extract interface name
		if ifname, ok := config["ifname"].(string); ok {
			rule.Ifname = ifname
		}

		// Extract enabled status
		if enabled, ok := config["enabled"].(string); ok {
			rule.Enabled = enabled == "1" || enabled == "true"
		}

		// Extract period
		if period, ok := config["period"].(string); ok {
			rule.Period = period
		}

		// Extract limit (handle both "limit" and "limit_mb" keys)
		if limitStr, ok := config["limit"].(string); ok {
			if limit, err := strconv.ParseInt(limitStr, 10, 64); err == nil {
				rule.LimitMB = limit
			}
		} else if limitStr, ok := config["limit_mb"].(string); ok {
			if limit, err := strconv.ParseInt(limitStr, 10, 64); err == nil {
				rule.LimitMB = limit
			}
		}

		// Extract clear_due timestamp
		if clearDue, ok := config["clear_due"].(string); ok {
			rule.ClearDue = clearDue
			if resetDate, err := time.Parse("2006-01-02 15:04:05", clearDue); err == nil {
				rule.ResetDate = resetDate
			}
		}

		// Extract SMS warning
		if smsWarn, ok := config["sms_warning"].(string); ok {
			rule.SMSWarning = smsWarn == "1" || smsWarn == "true"
		}

		// Calculate limit in bytes
		rule.LimitBytes = rule.LimitMB * 1024 * 1024

		// Only add rules that have an interface name and are for mobile interfaces
		if rule.Ifname != "" && strings.Contains(rule.Ifname, "mob") {
			rules = append(rules, rule)
		}
	}

	return rules, nil
}

// getInterfaceStatistics gets current interface usage statistics
func (e *EnhancedRutosDataLimitDetector) getInterfaceStatistics(ctx context.Context, ifname string) (*RutosInterfaceStats, error) {
	// Get interface statistics via ubus
	cmd := exec.CommandContext(ctx, "ubus", "-S", "call",
		fmt.Sprintf("network.interface.%s", ifname), "status")
	output, err := cmd.Output()
	if err != nil {
		return nil, fmt.Errorf("failed to get interface statistics: %w", err)
	}

	var response struct {
		Statistics struct {
			RXBytes uint64 `json:"rx_bytes"`
			TXBytes uint64 `json:"tx_bytes"`
		} `json:"statistics"`
	}
	if err := json.Unmarshal(output, &response); err != nil {
		return nil, fmt.Errorf("failed to parse interface statistics: %w", err)
	}

	stats := &RutosInterfaceStats{
		Interface: ifname,
		RXBytes:   response.Statistics.RXBytes,
		TXBytes:   response.Statistics.TXBytes,
		TotalMB:   (response.Statistics.RXBytes + response.Statistics.TXBytes) / (1024 * 1024),
	}

	return stats, nil
}

// convertRuleToDataUsageInfo converts a RutosDataLimitRule to DataUsageInfo
func (e *EnhancedRutosDataLimitDetector) convertRuleToDataUsageInfo(rule *RutosDataLimitRule) *DataUsageInfo {
	return &DataUsageInfo{
		Interface:    rule.Ifname,
		TotalBytes:   rule.UsedBytes,
		LimitBytes:   rule.LimitBytes,
		UsagePercent: rule.UsagePercent,
		Period:       rule.Period,
		ResetDate:    rule.ResetDate,
		LastUpdated:  time.Now(),
	}
}

// GetAllMobileInterfaces returns all mobile interfaces with data limit information
func (e *EnhancedRutosDataLimitDetector) GetAllMobileInterfaces(ctx context.Context) (map[string]*RutosDataLimitRule, error) {
	// Try native ubus service first
	if e.dataLimitObj != "" {
		if rules, err := e.getAllRulesViaUbusService(ctx); err == nil {
			return rules, nil
		}
	}

	// Fallback to UCI method
	return e.getAllRulesViaUCIFallback(ctx)
}

// getAllRulesViaUbusService gets all data limit rules via native ubus service
func (e *EnhancedRutosDataLimitDetector) getAllRulesViaUbusService(ctx context.Context) (map[string]*RutosDataLimitRule, error) {
	cmd := exec.CommandContext(ctx, "ubus", "-S", "call", e.dataLimitObj, "status", "{}")
	output, err := cmd.Output()
	if err != nil {
		return nil, fmt.Errorf("failed to call data limit service: %w", err)
	}

	var response struct {
		Rules []RutosDataLimitRule `json:"rules"`
	}
	if err := json.Unmarshal(output, &response); err != nil {
		return nil, fmt.Errorf("failed to parse data limit response: %w", err)
	}

	rules := make(map[string]*RutosDataLimitRule)
	for _, rule := range response.Rules {
		// Calculate additional fields
		rule.LimitBytes = rule.LimitMB * 1024 * 1024
		rule.UsedBytes = rule.UsedMB * 1024 * 1024
		if rule.LimitMB > 0 {
			rule.UsagePercent = (float64(rule.UsedMB) / float64(rule.LimitMB)) * 100.0
		}

		// Parse reset date
		if rule.ClearDue != "" {
			if resetDate, err := time.Parse(time.RFC3339, rule.ClearDue); err == nil {
				rule.ResetDate = resetDate
			}
		}

		rules[rule.Ifname] = &rule
	}

	return rules, nil
}

// getAllRulesViaUCIFallback gets all data limit rules via UCI fallback
func (e *EnhancedRutosDataLimitDetector) getAllRulesViaUCIFallback(ctx context.Context) (map[string]*RutosDataLimitRule, error) {
	uciRules, err := e.getDataLimitConfigFromUCI(ctx)
	if err != nil {
		return nil, err
	}

	rules := make(map[string]*RutosDataLimitRule)
	for _, rule := range uciRules {
		// Get current usage statistics
		if stats, err := e.getInterfaceStatistics(ctx, rule.Ifname); err == nil {
			rule.UsedBytes = int64(stats.RXBytes + stats.TXBytes)
			rule.UsedMB = int64(stats.TotalMB)
		}

		// Calculate usage percentage
		if rule.LimitMB > 0 {
			rule.UsagePercent = (float64(rule.UsedMB) / float64(rule.LimitMB)) * 100.0
		}

		rules[rule.Ifname] = &rule
	}

	return rules, nil
}

// PrintDataLimitTable prints a formatted table of all mobile interfaces (for debugging)
func (e *EnhancedRutosDataLimitDetector) PrintDataLimitTable(ctx context.Context) error {
	rules, err := e.GetAllMobileInterfaces(ctx)
	if err != nil {
		return err
	}

	e.logger.Info("Mobile Interface Data Limits:")
	e.logger.Info("Interface  | Enabled | Period | Limit(MB) | Used(MB) | Usage% | Clear Due")
	e.logger.Info("-----------|---------|--------|-----------|----------|--------|----------")

	for ifname, rule := range rules {
		enabledStr := "No"
		if rule.Enabled {
			enabledStr = "Yes"
		}

		e.logger.Info(fmt.Sprintf("%-10s | %-7s | %-6s | %9d | %8d | %5.1f%% | %s",
			ifname, enabledStr, rule.Period, rule.LimitMB, rule.UsedMB,
			rule.UsagePercent, rule.ClearDue))
	}

	return nil
}
