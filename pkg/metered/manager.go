package metered

import (
	"context"
	"fmt"
	"os/exec"
	"strings"
	"sync"
	"time"

	"github.com/markus-lassfolk/autonomy/pkg"
	"github.com/markus-lassfolk/autonomy/pkg/adaptive"
	"github.com/markus-lassfolk/autonomy/pkg/logx"
)

// Mode represents the metered connection mode
type Mode string

const (
	ModeOff             Mode = "off"
	ModeRestricted      Mode = "restricted"
	ModeNearCap         Mode = "near-cap"
	ModeOverCap         Mode = "over-cap"
	ModeTetheredNoLimit Mode = "tethered-no-limit"
)

// VendorElement represents a WiFi vendor-specific information element
type VendorElement struct {
	Name string
	Data string
}

// ConfigProvider interface to avoid circular dependency
type ConfigProvider interface {
	GetMeteredConfig() map[string]interface{}
	GetMeteredModeEnabled() bool
	GetDataLimitWarningThreshold() int
	GetDataLimitCriticalThreshold() int
	GetDataUsageHysteresisMargin() int
	GetMeteredStabilityDelay() int
	GetMeteredClientReconnectMethod() string
	GetMeteredModeDebug() bool
}

// Manager handles metered connection signaling
type Manager struct {
	config ConfigProvider
	logger *logx.Logger

	// Current state
	currentMode    Mode
	currentMember  *pkg.Member
	lastModeChange time.Time
	stabilityDelay time.Duration

	// Configuration
	enabled                    bool
	dataLimitWarningThreshold  int    // Percentage
	dataLimitCriticalThreshold int    // Percentage
	hysteresisMargin           int    // Percentage
	clientReconnectMethod      string // "gentle" or "force"
	debugEnabled               bool

	// State tracking
	mu                sync.RWMutex
	pendingModeChange *pendingChange

	// Adaptive sampling integration
	adaptiveSampler *adaptive.AdaptiveSampler
	rateOptimizer   *adaptive.RateOptimizer
}

type pendingChange struct {
	mode      Mode
	member    *pkg.Member
	reason    string
	timestamp time.Time
}

// NewManager creates a new metered mode manager
func NewManager(config ConfigProvider, logger *logx.Logger) (*Manager, error) {
	m := &Manager{
		config:                     config,
		logger:                     logger,
		currentMode:                ModeOff,
		stabilityDelay:             5 * time.Minute,
		dataLimitWarningThreshold:  80,
		dataLimitCriticalThreshold: 95,
		hysteresisMargin:           5,
		clientReconnectMethod:      "gentle",
	}

	// Load configuration from UCI
	if err := m.loadConfig(); err != nil {
		return nil, fmt.Errorf("failed to load metered mode config: %w", err)
	}

	return m, nil
}

// SetAdaptiveSamplingComponents sets the adaptive sampling components for integration
func (m *Manager) SetAdaptiveSamplingComponents(sampler *adaptive.AdaptiveSampler, optimizer *adaptive.RateOptimizer) {
	m.adaptiveSampler = sampler
	m.rateOptimizer = optimizer
	m.logger.Info("Adaptive sampling components set for metered mode integration")
}

// GetAdaptiveSamplingInterval returns the optimal sampling interval considering metered mode
func (m *Manager) GetAdaptiveSamplingInterval(ctx context.Context, member *pkg.Member) time.Duration {
	if m.rateOptimizer == nil {
		return 1 * time.Second // Default interval
	}

	// Get base connection type
	connectionType := m.getConnectionTypeForMember(member)

	// Adjust for metered mode
	if m.isMeteredMode() {
		// Use more conservative sampling for metered connections
		connectionType = adaptive.ConnectionTypeCellular // Treat as cellular for sampling purposes
	}

	// Get data usage
	dataUsage := m.getDataUsageForMember(member)

	// Get optimal rate
	optimalRate := m.rateOptimizer.GetOptimalRate(ctx, connectionType, dataUsage)

	// Apply metered mode adjustments
	if m.isMeteredMode() {
		// Increase interval for metered connections
		optimalRate = optimalRate * 2
	}

	m.logger.Debug("Metered mode adaptive sampling interval calculated",
		"member", member.Name,
		"metered_mode", m.isMeteredMode(),
		"connection_type", connectionType,
		"data_usage", dataUsage,
		"optimal_rate", optimalRate)

	return optimalRate
}

// isMeteredMode returns whether metered mode is currently active
func (m *Manager) isMeteredMode() bool {
	m.mu.RLock()
	defer m.mu.RUnlock()
	return m.currentMode != ModeOff
}

// getConnectionTypeForMember determines the connection type for a member
func (m *Manager) getConnectionTypeForMember(member *pkg.Member) adaptive.ConnectionType {
	switch member.Class {
	case pkg.ClassStarlink:
		return adaptive.ConnectionTypeStarlink
	case pkg.ClassCellular:
		return adaptive.ConnectionTypeCellular
	case pkg.ClassWiFi:
		return adaptive.ConnectionTypeWiFi
	case pkg.ClassLAN:
		return adaptive.ConnectionTypeLAN
	default:
		return adaptive.ConnectionTypeUnknown
	}
}

// getDataUsageForMember gets the data usage for a member
func (m *Manager) getDataUsageForMember(member *pkg.Member) float64 {
	// TODO: Implement actual data usage tracking from data limit detection
	// For now, return a conservative estimate
	return 10.0 // 10 MB per hour
}

// loadConfig loads configuration from UCI
func (m *Manager) loadConfig() error {
	// Load metered mode configuration from the config struct
	m.enabled = m.config.GetMeteredModeEnabled()
	m.dataLimitWarningThreshold = m.config.GetDataLimitWarningThreshold()
	m.dataLimitCriticalThreshold = m.config.GetDataLimitCriticalThreshold()
	m.hysteresisMargin = m.config.GetDataUsageHysteresisMargin()
	m.stabilityDelay = time.Duration(m.config.GetMeteredStabilityDelay()) * time.Second
	m.clientReconnectMethod = m.config.GetMeteredClientReconnectMethod()
	m.debugEnabled = m.config.GetMeteredModeDebug()

	m.logger.Info("Loaded metered mode configuration",
		"enabled", m.enabled,
		"warning_threshold", m.dataLimitWarningThreshold,
		"critical_threshold", m.dataLimitCriticalThreshold,
		"hysteresis_margin", m.hysteresisMargin,
		"stability_delay", m.stabilityDelay,
		"reconnect_method", m.clientReconnectMethod,
		"debug", m.debugEnabled)

	return nil
}

// OnFailover handles mwan3 failover events
func (m *Manager) OnFailover(from, to *pkg.Member) error {
	if !m.enabled {
		return nil
	}

	m.mu.Lock()
	defer m.mu.Unlock()

	m.logger.Info("Processing failover for metered mode",
		"from", func() string {
			if from != nil {
				return from.Name
			}
			return "none"
		}(),
		"to", to.Name,
		"class", to.Class)

	// Determine appropriate mode based on new interface
	newMode, reason := m.determineModeForMember(to)

	// Schedule mode change with stability delay
	m.pendingModeChange = &pendingChange{
		mode:      newMode,
		member:    to,
		reason:    reason,
		timestamp: time.Now(),
	}

	m.currentMember = to

	m.logger.Info("Scheduled metered mode change",
		"mode", newMode,
		"reason", reason,
		"delay", m.stabilityDelay)

	return nil
}

// OnDataUsageUpdate handles data usage threshold updates
func (m *Manager) OnDataUsageUpdate(member *pkg.Member, usagePercent float64) error {
	if !m.enabled || member != m.currentMember {
		return nil
	}

	m.mu.Lock()
	defer m.mu.Unlock()

	// Determine mode based on usage with hysteresis
	newMode := m.determineModeFromUsage(usagePercent)

	if newMode != m.currentMode {
		reason := fmt.Sprintf("data_usage_%.1f%%", usagePercent)

		m.logger.Info("Data usage triggered mode change",
			"member", member.Name,
			"usage_percent", usagePercent,
			"current_mode", m.currentMode,
			"new_mode", newMode,
			"reason", reason)

		// Apply mode change immediately for data usage updates
		return m.applyModeChange(newMode, member, reason)
	}

	return nil
}

// ProcessPendingChanges processes any pending mode changes after stability delay
func (m *Manager) ProcessPendingChanges() error {
	if !m.enabled {
		return nil
	}

	m.mu.Lock()
	defer m.mu.Unlock()

	if m.pendingModeChange == nil {
		return nil
	}

	// Check if stability delay has passed
	if time.Since(m.pendingModeChange.timestamp) < m.stabilityDelay {
		return nil
	}

	// Apply the pending change
	change := m.pendingModeChange
	m.pendingModeChange = nil

	m.logger.Info("Applying pending metered mode change",
		"mode", change.mode,
		"member", change.member.Name,
		"reason", change.reason,
		"delay_elapsed", time.Since(change.timestamp))

	return m.applyModeChange(change.mode, change.member, change.reason)
}

// determineModeForMember determines the appropriate mode for a member
func (m *Manager) determineModeForMember(member *pkg.Member) (Mode, string) {
	// Check if member is WiFi STA (tethering mode)
	if member.Class == "wifi" && m.isWiFiSTAMode(member) {
		return ModeTetheredNoLimit, "wifi_sta_tethering"
	}

	// Check if member has data limits
	if m.hasDataLimits(member) {
		// Get current usage to determine initial mode
		if usagePercent, err := m.getDataUsagePercent(member); err == nil {
			return m.determineModeFromUsage(usagePercent), fmt.Sprintf("data_limited_%.1f%%", usagePercent)
		}
		return ModeRestricted, "data_limited_unknown_usage"
	}

	// No limits - turn off metered signaling
	return ModeOff, "no_data_limits"
}

// determineModeFromUsage determines mode based on data usage percentage
func (m *Manager) determineModeFromUsage(usagePercent float64) Mode {
	// Apply hysteresis to prevent flapping
	warningThreshold := float64(m.dataLimitWarningThreshold)
	criticalThreshold := float64(m.dataLimitCriticalThreshold)
	hysteresis := float64(m.hysteresisMargin)

	// Adjust thresholds based on current mode to add hysteresis
	switch m.currentMode {
	case ModeOverCap:
		// Need to drop below critical - hysteresis to go back to near-cap
		if usagePercent < criticalThreshold-hysteresis {
			if usagePercent < warningThreshold-hysteresis {
				return ModeRestricted
			}
			return ModeNearCap
		}
		return ModeOverCap

	case ModeNearCap:
		// Check for escalation to over-cap or de-escalation to restricted
		if usagePercent >= criticalThreshold {
			return ModeOverCap
		}
		if usagePercent < warningThreshold-hysteresis {
			return ModeRestricted
		}
		return ModeNearCap

	default:
		// From restricted or off, use normal thresholds
		if usagePercent >= criticalThreshold {
			return ModeOverCap
		}
		if usagePercent >= warningThreshold {
			return ModeNearCap
		}
		return ModeRestricted
	}
}

// applyModeChange applies a metered mode change
func (m *Manager) applyModeChange(mode Mode, member *pkg.Member, reason string) error {
	if mode == m.currentMode {
		return nil
	}

	oldMode := m.currentMode
	m.currentMode = mode
	m.lastModeChange = time.Now()

	m.logger.Info("Applying metered mode change",
		"old_mode", oldMode,
		"new_mode", mode,
		"member", member.Name,
		"reason", reason)

	// Generate vendor elements for the new mode
	elements := m.generateVendorElements(mode, member)

	// Apply vendor elements to WiFi interfaces
	if err := m.applyVendorElements(elements); err != nil {
		return fmt.Errorf("failed to apply vendor elements: %w", err)
	}

	// Update DHCP configuration for Android
	if err := m.updateDHCPConfiguration(mode); err != nil {
		return fmt.Errorf("failed to update DHCP configuration: %w", err)
	}

	// Trigger client reconnection if configured
	if err := m.triggerClientReconnection(); err != nil {
		m.logger.Warn("Failed to trigger client reconnection", "error", err)
		// Don't fail the mode change for reconnection issues
	}

	m.logger.Info("Successfully applied metered mode change",
		"mode", mode,
		"member", member.Name,
		"reason", reason)

	return nil
}

// generateVendorElements generates vendor elements for the specified mode
func (m *Manager) generateVendorElements(mode Mode, member *pkg.Member) []VendorElement {
	var elements []VendorElement

	switch mode {
	case ModeRestricted:
		// Microsoft Network Cost IE: Fixed cost, no flags
		elements = append(elements, VendorElement{
			Name: "Microsoft Network Cost IE (Restricted)",
			Data: "DD080050F211020000",
		})

	case ModeNearCap:
		// Microsoft Network Cost IE: Fixed cost + ApproachingDataLimit
		elements = append(elements, VendorElement{
			Name: "Microsoft Network Cost IE (Near Cap)",
			Data: "DD080050F211020800",
		})

	case ModeOverCap:
		// Microsoft Network Cost IE: Fixed cost + OverDataLimit
		elements = append(elements, VendorElement{
			Name: "Microsoft Network Cost IE (Over Cap)",
			Data: "DD080050F211020100",
		})

	case ModeTetheredNoLimit:
		// Microsoft Tethering Identifier IE with AP MAC
		if tetherIE := m.generateTetherIE(member); tetherIE != "" {
			elements = append(elements, VendorElement{
				Name: "Microsoft Tethering Identifier IE",
				Data: tetherIE,
			})
		}
	}

	// Add Apple vendor element for all metered modes (except off)
	if mode != ModeOff {
		elements = append(elements, VendorElement{
			Name: "Apple Vendor IE",
			Data: "DD0A0017F206010103010000",
		})
	}

	return elements
}

// generateTetherIE generates Microsoft Tethering Identifier IE with AP MAC
func (m *Manager) generateTetherIE(member *pkg.Member) string {
	// Get MAC address for the interface
	mac, err := m.getInterfaceMAC(member.Iface)
	if err != nil {
		m.logger.Warn("Failed to get interface MAC for tethering IE",
			"interface", member.Iface, "error", err)
		return ""
	}

	// Format: DD 0E 00 50 F2 12 00 2B 00 06 <AP_MAC(6 bytes)>
	macHex := strings.ReplaceAll(strings.ToUpper(mac), ":", "")
	return fmt.Sprintf("DD0E0050F212002B0006%s", macHex)
}

// getInterfaceMAC gets the MAC address for an interface
func (m *Manager) getInterfaceMAC(iface string) (string, error) {
	macFile := fmt.Sprintf("/sys/class/net/%s/address", iface)
	cmd := exec.Command("cat", macFile)
	output, err := cmd.Output()
	if err != nil {
		return "", fmt.Errorf("failed to read MAC address: %w", err)
	}

	return strings.TrimSpace(string(output)), nil
}

// isWiFiSTAMode checks if a WiFi interface is in STA mode
func (m *Manager) isWiFiSTAMode(member *pkg.Member) bool {
	// Check UCI wireless configuration for STA mode
	cmd := exec.Command("uci", "show", "wireless")
	output, err := cmd.Output()
	if err != nil {
		return false
	}

	lines := strings.Split(string(output), "\n")
	for _, line := range lines {
		if strings.Contains(line, member.Iface) && strings.Contains(line, "mode='sta'") {
			return true
		}
	}

	return false
}

// hasDataLimits checks if a member has data limits configured
func (m *Manager) hasDataLimits(member *pkg.Member) bool {
	// Check for data limits in network configuration
	limitKeys := []string{
		fmt.Sprintf("network.%s.data_limit", member.Iface),
		fmt.Sprintf("network.%s.data_limit_daily_bytes", member.Iface),
		fmt.Sprintf("network.%s.data_limit_monthly_bytes", member.Iface),
	}

	for _, key := range limitKeys {
		cmd := exec.Command("uci", "-q", "get", key)
		if err := cmd.Run(); err == nil {
			return true
		}
	}

	// Check for cellular interfaces (typically have data limits)
	if member.Class == "cellular" {
		return true
	}

	return false
}

// getDataUsagePercent gets current data usage percentage for a member
func (m *Manager) getDataUsagePercent(member *pkg.Member) (float64, error) {
	// Use the proper DataUsageMonitor implementation
	monitor := NewDataUsageMonitor(m)
	usageInfo, err := monitor.GetDataUsageInfo(member)
	if err != nil {
		return 0.0, fmt.Errorf("failed to get data usage info: %w", err)
	}

	if usageInfo == nil {
		// No data limits configured
		return 0.0, nil
	}

	return usageInfo.UsagePercent, nil
}

// GetCurrentMode returns the current metered mode
func (m *Manager) GetCurrentMode() Mode {
	m.mu.RLock()
	defer m.mu.RUnlock()
	return m.currentMode
}

// GetStatus returns the current status
func (m *Manager) GetStatus() map[string]interface{} {
	m.mu.RLock()
	defer m.mu.RUnlock()

	status := map[string]interface{}{
		"enabled":      m.enabled,
		"current_mode": string(m.currentMode),
		"last_change":  m.lastModeChange.Format(time.RFC3339),
	}

	if m.currentMember != nil {
		status["current_member"] = m.currentMember.Name
	}

	if m.pendingModeChange != nil {
		status["pending_change"] = map[string]interface{}{
			"mode":      string(m.pendingModeChange.mode),
			"reason":    m.pendingModeChange.reason,
			"scheduled": m.pendingModeChange.timestamp.Format(time.RFC3339),
			"remaining": m.stabilityDelay - time.Since(m.pendingModeChange.timestamp),
		}
	}

	return status
}
