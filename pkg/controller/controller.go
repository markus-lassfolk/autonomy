package controller

import (
	"context"
	"encoding/json"
	"fmt"
	"os/exec"
	"strconv"
	"strings"
	"sync"
	"time"

	"github.com/markus-lassfolk/autonomy/pkg"
	"github.com/markus-lassfolk/autonomy/pkg/logx"
	"github.com/markus-lassfolk/autonomy/pkg/uci"
)

// FailoverCallback is called when a failover occurs
type FailoverCallback func(from, to *pkg.Member) error

// Controller implements network interface control
type Controller struct {
	config *uci.Config
	logger *logx.Logger

	// Current state
	currentMember *pkg.Member

	// Member list
	members   []*pkg.Member
	membersMu sync.RWMutex

	// MWAN3 integration
	mwan3Enabled bool
	mwan3Path    string

	// Netifd integration
	ubusPath string

	// Behavior
	dryRun bool

	// Callbacks
	failoverCallbacks []FailoverCallback
	callbacksMu       sync.RWMutex
}

// NewController creates a new controller
func NewController(config *uci.Config, logger *logx.Logger) (*Controller, error) {
	ctrl := &Controller{
		config:       config,
		logger:       logger,
		mwan3Path:    "mwan3",
		ubusPath:     "ubus",
		mwan3Enabled: config.UseMWAN3,
	}

	// Test if mwan3 is available
	if config.UseMWAN3 {
		if err := ctrl.testMWAN3(); err != nil {
			logger.Warn("MWAN3 not available, falling back to netifd", "error", err)
			ctrl.mwan3Enabled = false
		}
	}

	return ctrl, nil
}

// Switch switches from one member to another
func (c *Controller) Switch(from, to *pkg.Member) error {
	if to == nil {
		return fmt.Errorf("target member cannot be nil")
	}

	c.logger.Info("Switching network interface", "from", func() string {
		if from != nil {
			return from.Name
		}
		return "none"
	}(), "to", to.Name, "dry_run", c.dryRun)

	// Validate target member
	if err := c.Validate(to); err != nil {
		return fmt.Errorf("invalid target member: %w", err)
	}

	// Perform the switch based on available methods
	if c.mwan3Enabled {
		return c.switchMWAN3(from, to)
	} else {
		return c.switchNetifd(from, to)
	}
}

// GetCurrentMember returns the current active member
func (c *Controller) GetCurrentMember() (*pkg.Member, error) {
	if c.currentMember != nil {
		return c.currentMember, nil
	}

	// Try to determine current member from mwan3 or netifd
	if c.mwan3Enabled {
		return c.getCurrentMemberMWAN3()
	} else {
		return c.getCurrentMemberNetifd()
	}
}

// Validate validates a member
func (c *Controller) Validate(member *pkg.Member) error {
	if member == nil {
		return fmt.Errorf("member cannot be nil")
	}
	if member.Name == "" {
		return fmt.Errorf("member name cannot be empty")
	}
	if member.Iface == "" {
		return fmt.Errorf("member interface cannot be empty")
	}
	return nil
}

// testMWAN3 tests if mwan3 is available
func (c *Controller) testMWAN3() error {
	ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancel()

	cmd := exec.CommandContext(ctx, c.mwan3Path, "status")
	return cmd.Run()
}

// switchMWAN3 switches using mwan3
func (c *Controller) switchMWAN3(from, to *pkg.Member) error {
	c.logger.LogMWAN3("switch", map[string]interface{}{
		"from": func() string {
			if from != nil {
				return from.Name
			}
			return "none"
		}(),
		"to":      to.Name,
		"dry_run": c.dryRun,
	})

	// Get current mwan3 status
	status, err := c.getMWAN3Status()
	if err != nil {
		return fmt.Errorf("failed to get mwan3 status: %w", err)
	}

	// Check if we need to make changes
	if c.isMWAN3SwitchNeeded(from, to, status) {
		if c.dryRun {
			c.logger.Info("DRY RUN: Updating MWAN3 policy (config changes allowed)", "target", to.Name, "iface", to.Iface)
			// Update mwan3 policy (allowed in dry-run for testing)
			if err := c.updateMWAN3Policy(to); err != nil {
				return fmt.Errorf("failed to update mwan3 policy: %w", err)
			}
			c.logger.Info("DRY RUN: Would reload MWAN3 (skipped to prevent network changes)")
		} else {
			// Update mwan3 policy
			if err := c.updateMWAN3Policy(to); err != nil {
				return fmt.Errorf("failed to update mwan3 policy: %w", err)
			}

			// Reload mwan3
			if err := c.reloadMWAN3(); err != nil {
				return fmt.Errorf("failed to reload mwan3: %w", err)
			}

			c.logger.LogMWAN3("reload", map[string]interface{}{
				"reason": "policy_change",
			})
		}
	} else {
		c.logger.LogMWAN3("unchanged", map[string]interface{}{
			"reason": "no_change_needed",
		})
	}

	// Update current member (allowed in dry-run for testing)
	oldMember := c.currentMember
	c.currentMember = to
	if c.dryRun {
		c.logger.Info("DRY RUN: Updated current member tracking", "member", to.Name)
	}

	// Call failover callbacks
	c.callFailoverCallbacks(oldMember, to)

	return nil
}

// switchNetifd switches using netifd/route metrics
func (c *Controller) switchNetifd(from, to *pkg.Member) error {
	c.logger.Info("Switching via netifd", "from", func() string {
		if from != nil {
			return from.Name
		}
		return "none"
	}(), "to", to.Name, "dry_run", c.dryRun)

	if c.dryRun {
		c.logger.Info("DRY RUN: Would update route metrics via netifd", "target", to.Name, "iface", to.Iface)
	} else {
		// Update route metrics to prefer the target interface
		if err := c.updateRouteMetrics(to); err != nil {
			return fmt.Errorf("failed to update route metrics: %w", err)
		}
	}

	// Update current member (allowed in dry-run for testing)
	oldMember := c.currentMember
	c.currentMember = to
	if c.dryRun {
		c.logger.Info("DRY RUN: Updated current member tracking", "member", to.Name)
	}

	// Call failover callbacks
	c.callFailoverCallbacks(oldMember, to)

	return nil
}

// getMWAN3Status gets the current mwan3 status
func (c *Controller) getMWAN3Status() (map[string]interface{}, error) {
	ctx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
	defer cancel()

	// Prefer JSON output via CLI (both variants), then fall back to ubus, then synthesize
	tryCommands := [][]string{
		{c.mwan3Path, "status", "json"},
		{c.mwan3Path, "status", "--json"},
	}
	for _, args := range tryCommands {
		cmd := exec.CommandContext(ctx, args[0], args[1:]...)
		if output, err := cmd.Output(); err == nil {
			var status map[string]interface{}
			if err := json.Unmarshal(output, &status); err == nil {
				return status, nil
			}
		}
	}

	if c.ubusPath != "" {
		if out, err := exec.CommandContext(ctx, c.ubusPath, "call", "mwan3", "status").Output(); err == nil {
			var status map[string]interface{}
			if err := json.Unmarshal(out, &status); err == nil {
				return status, nil
			}
		}
	}

	// Synthesize minimal status from netifd for known interfaces
	type ifaceInfo struct{ name string }
	interfaces := []ifaceInfo{}
	c.membersMu.RLock()
	for _, m := range c.members {
		if m != nil && m.Iface != "" {
			interfaces = append(interfaces, ifaceInfo{name: m.Iface})
		}
	}
	c.membersMu.RUnlock()
	if len(interfaces) == 0 {
		interfaces = append(interfaces, ifaceInfo{name: "wan"}, ifaceInfo{name: "mob1s1a1"})
	}

	ifaceMap := map[string]interface{}{}
	for _, it := range interfaces {
		if it.name == "" {
			continue
		}
		out, err := exec.CommandContext(ctx, c.ubusPath, "call", "network.interface."+it.name, "status").Output()
		if err != nil {
			ifaceMap[it.name] = map[string]interface{}{"status": "offline"}
			continue
		}
		var st map[string]interface{}
		if err := json.Unmarshal(out, &st); err != nil {
			ifaceMap[it.name] = map[string]interface{}{"status": "offline"}
			continue
		}
		if up, ok := st["up"].(bool); ok && up {
			ifaceMap[it.name] = map[string]interface{}{"status": "online"}
		} else {
			ifaceMap[it.name] = map[string]interface{}{"status": "offline"}
		}
	}
	return map[string]interface{}{"interfaces": ifaceMap}, nil
}

// isMWAN3SwitchNeeded checks if a mwan3 switch is needed
func (c *Controller) isMWAN3SwitchNeeded(from, to *pkg.Member, status map[string]interface{}) bool {
	// Check current policy
	if interfaces, ok := status["interfaces"].(map[string]interface{}); ok {
		for ifaceName, ifaceData := range interfaces {
			if ifaceName == to.Iface {
				if ifaceMap, ok := ifaceData.(map[string]interface{}); ok {
					if status, ok := ifaceMap["status"].(string); ok {
						// Check if this interface is already the primary
						return status != "online" || (from != nil && from.Iface == to.Iface)
					}
				}
			}
		}
	}

	return true
}

// updateMWAN3Policy updates the mwan3 policy to prefer the target member
func (c *Controller) updateMWAN3Policy(to *pkg.Member) error {
	if to == nil {
		return fmt.Errorf("target member cannot be nil")
	}

	c.logger.LogMWAN3("policy_update_start", map[string]interface{}{
		"target": to.Name,
		"iface":  to.Iface,
	})

	// Read current mwan3 configuration from UCI
	config, err := c.readMWAN3Config()
	if err != nil {
		return fmt.Errorf("failed to read mwan3 config: %w", err)
	}

	// Update member weights to prefer the target interface
	updated, err := c.updateMemberWeights(config, to)
	if err != nil {
		return fmt.Errorf("failed to update member weights: %w", err)
	}

	if !updated {
		c.logger.LogMWAN3("policy_unchanged", map[string]interface{}{
			"target": to.Name,
			"reason": "weights_already_optimal",
		})
		return nil
	}

	// Write the updated configuration back
	if err := c.writeMWAN3Config(config); err != nil {
		return fmt.Errorf("failed to write mwan3 config: %w", err)
	}

	c.logger.LogMWAN3("policy_update_complete", map[string]interface{}{
		"target":          to.Name,
		"iface":           to.Iface,
		"weights_updated": true,
	})

	return nil
}

// reloadMWAN3 reloads mwan3 configuration
func (c *Controller) reloadMWAN3() error {
	if c.dryRun {
		c.logger.Info("DRY RUN: Would reload MWAN3 configuration")
		return nil
	}
	ctx, cancel := context.WithTimeout(context.Background(), 30*time.Second)
	defer cancel()

	cmd := exec.CommandContext(ctx, c.mwan3Path, "reload")
	return cmd.Run()
}

// MWAN3Config represents the mwan3 configuration structure
type MWAN3Config struct {
	Members  []*MWAN3Member `json:"members"`
	Policies []*MWAN3Policy `json:"policies"`
}

type MWAN3Member struct {
	Name    string `json:"name"`
	UCIName string `json:"uci_name"` // UCI section name (e.g., "member1")
	Iface   string `json:"interface"`
	Weight  int    `json:"weight"`
	Metric  int    `json:"metric"`
	Enabled bool   `json:"enabled"`
}

type MWAN3Policy struct {
	Name    string   `json:"name"`
	Members []string `json:"members"`
}

// readMWAN3Config reads the current mwan3 configuration from UCI
func (c *Controller) readMWAN3Config() (*MWAN3Config, error) {
	ctx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
	defer cancel()

	// Read mwan3 member configuration
	cmd := exec.CommandContext(ctx, "uci", "show", "mwan3")
	output, err := cmd.Output()
	if err != nil {
		return nil, fmt.Errorf("failed to read mwan3 config: %w", err)
	}

	config := &MWAN3Config{
		Members:  []*MWAN3Member{},
		Policies: []*MWAN3Policy{},
	}

	// Parse UCI output
	lines := strings.Split(string(output), "\n")
	memberMap := make(map[string]*MWAN3Member)
	policyMap := make(map[string]*MWAN3Policy)

	for _, line := range lines {
		line = strings.TrimSpace(line)
		if line == "" {
			continue
		}

		// Parse member configurations
		if strings.Contains(line, "mwan3.") && strings.Contains(line, "=member") {
			parts := strings.Split(line, ".")
			if len(parts) >= 2 {
				memberNamePart := parts[1]
				memberName := strings.Split(memberNamePart, "=")[0] // Extract name before =member
				if _, exists := memberMap[memberName]; !exists {
					memberMap[memberName] = &MWAN3Member{
						Name:    memberName, // Will be overridden if there's a name field
						UCIName: memberName, // UCI section name
						Weight:  1,
						Metric:  1,
						Enabled: true,
					}
				}
			}
		}

		// Parse member properties
		if strings.Contains(line, ".interface=") {
			parts := strings.Split(line, "=")
			if len(parts) == 2 {
				memberName := strings.Split(strings.Split(line, ".")[1], ".interface")[0]
				if member, exists := memberMap[memberName]; exists {
					member.Iface = strings.Trim(parts[1], "'\"")
				}
			}
		}

		if strings.Contains(line, ".weight=") {
			parts := strings.Split(line, "=")
			if len(parts) == 2 {
				memberName := strings.Split(strings.Split(line, ".")[1], ".weight")[0]
				if member, exists := memberMap[memberName]; exists {
					if weight, err := strconv.Atoi(strings.Trim(parts[1], "'\"")); err == nil {
						member.Weight = weight
					}
				}
			}
		}

		if strings.Contains(line, ".metric=") {
			parts := strings.Split(line, "=")
			if len(parts) == 2 {
				memberName := strings.Split(strings.Split(line, ".")[1], ".metric")[0]
				if member, exists := memberMap[memberName]; exists {
					if metric, err := strconv.Atoi(strings.Trim(parts[1], "'\"")); err == nil {
						member.Metric = metric
					}
				}
			}
		}

		if strings.Contains(line, ".name=") {
			parts := strings.Split(line, "=")
			if len(parts) == 2 {
				memberName := strings.Split(strings.Split(line, ".")[1], ".name")[0]
				if member, exists := memberMap[memberName]; exists {
					member.Name = strings.Trim(parts[1], "'\"")
				}
			}
		}

		// Parse policies
		if strings.Contains(line, "=policy") {
			parts := strings.Split(line, ".")
			if len(parts) >= 2 {
				policyName := parts[1]
				if _, exists := policyMap[policyName]; !exists {
					policyMap[policyName] = &MWAN3Policy{
						Name:    policyName,
						Members: []string{},
					}
				}
			}
		}
	}

	// Convert maps to slices
	for _, member := range memberMap {
		config.Members = append(config.Members, member)
	}
	for _, policy := range policyMap {
		config.Policies = append(config.Policies, policy)
	}

	return config, nil
}

// updateMemberWeights updates member weights to prefer the target
func (c *Controller) updateMemberWeights(config *MWAN3Config, target *pkg.Member) (bool, error) {
	updated := false
	targetFound := false

	for _, member := range config.Members {
		// Match by UCI section name (member.UCIName) or interface name
		if member.Iface == target.Iface || member.UCIName == target.Name || member.Name == target.Name {
			targetFound = true // Set this regardless of weight change
			// Set target member to high priority
			if member.Weight != 100 {
				member.Weight = 100
				updated = true
			}
		} else {
			// Set other members to low priority
			if member.Weight != 10 {
				member.Weight = 10
				updated = true
			}
		}
	}

	if !targetFound {
		return false, fmt.Errorf("target member %s (interface %s) not found in mwan3 config. Available members: %v",
			target.Name, target.Iface, func() []string {
				var names []string
				for _, m := range config.Members {
					names = append(names, fmt.Sprintf("%s(uci:%s,iface:%s)", m.Name, m.UCIName, m.Iface))
				}
				return names
			}())
	}

	return updated, nil
}

// writeMWAN3Config writes the mwan3 configuration back to UCI
func (c *Controller) writeMWAN3Config(config *MWAN3Config) error {
	if c.dryRun {
		c.logger.Info("DRY RUN: Writing MWAN3 config (allowed in dry-run for testing)")
	}
	ctx, cancel := context.WithTimeout(context.Background(), 15*time.Second)
	defer cancel()

	// Update each member's weight using UCI section name
	for _, member := range config.Members {
		uciName := member.UCIName
		if uciName == "" {
			uciName = member.Name // fallback to name if UCIName not set
		}
		cmd := exec.CommandContext(ctx, "uci", "set",
			fmt.Sprintf("mwan3.%s.weight=%d", uciName, member.Weight))
		if err := cmd.Run(); err != nil {
			return fmt.Errorf("failed to set weight for member %s (uci:%s): %w", member.Name, uciName, err)
		}
	}

	// Commit changes
	cmd := exec.CommandContext(ctx, "uci", "commit", "mwan3")
	if err := cmd.Run(); err != nil {
		return fmt.Errorf("failed to commit mwan3 config: %w", err)
	}

	return nil
}

// updateRouteMetrics updates route metrics to prefer the target interface
func (c *Controller) updateRouteMetrics(to *pkg.Member) error {
	c.logger.Info("Updating route metrics via netifd", "target", to.Name, "iface", to.Iface, "dry_run", c.dryRun)
	if c.dryRun {
		c.logger.Info("DRY RUN: Would change default route metric", "iface", to.Iface)
		return nil
	}

	ctx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
	defer cancel()

	// Get current interface information
	cmd := exec.CommandContext(ctx, c.ubusPath, "call", "network.interface."+to.Iface, "status")
	output, err := cmd.Output()
	if err != nil {
		return fmt.Errorf("failed to get interface status for %s: %w", to.Iface, err)
	}

	var ifaceStatus map[string]interface{}
	if err := json.Unmarshal(output, &ifaceStatus); err != nil {
		return fmt.Errorf("failed to parse interface status: %w", err)
	}

	// Check if interface is up
	if up, ok := ifaceStatus["up"].(bool); !ok || !up {
		return fmt.Errorf("interface %s is not up", to.Iface)
	}

	// Update route metrics by setting interface as preferred
	// This is done by lowering the metric for this interface's routes
	cmd = exec.CommandContext(ctx, "ip", "route", "change", "default",
		"dev", to.Iface, "metric", "1")
	if err := cmd.Run(); err != nil {
		c.logger.Warn("Failed to update default route metric", "iface", to.Iface, "error", err)
		// Try alternative approach - bring interface up with higher priority
		cmd = exec.CommandContext(ctx, c.ubusPath, "call", "network.interface."+to.Iface, "up")
		if err := cmd.Run(); err != nil {
			return fmt.Errorf("failed to bring up interface %s: %w", to.Iface, err)
		}
	}

	c.logger.Info("Route metrics updated successfully", "target", to.Name, "iface", to.Iface)
	return nil
}

// getCurrentMemberMWAN3 gets the current member from mwan3
func (c *Controller) getCurrentMemberMWAN3() (*pkg.Member, error) {
	status, err := c.getMWAN3Status()
	if err != nil {
		return nil, err
	}

	// Find the primary interface
	if interfaces, ok := status["interfaces"].(map[string]interface{}); ok {
		for ifaceName, ifaceData := range interfaces {
			if ifaceMap, ok := ifaceData.(map[string]interface{}); ok {
				if status, ok := ifaceMap["status"].(string); ok {
					if status == "online" {
						// This is a simplified approach - in reality you'd need to
						// map the interface back to a member
						return &pkg.Member{
							Name:  ifaceName,
							Iface: ifaceName,
							Class: pkg.ClassOther,
						}, nil
					}
				}
			}
		}
	}

	return nil, fmt.Errorf("no active interface found")
}

// getCurrentMemberNetifd gets the current member from netifd
func (c *Controller) getCurrentMemberNetifd() (*pkg.Member, error) {
	// Query netifd via ubus to determine which interface is currently active
	// This method works by checking interface status and routing table

	// First, try to get the default route to determine active interface
	activeInterface, err := c.getActiveInterfaceFromRoutes()
	if err != nil {
		c.logger.Debug("Failed to get active interface from routes", "error", err)
		// Fallback: check interface status via ubus
		activeInterface, err = c.getActiveInterfaceFromNetifd()
		if err != nil {
			return nil, fmt.Errorf("failed to determine active interface: %w", err)
		}
	}

	// Find the member that corresponds to this interface
	for _, member := range c.members {
		if member.Iface == activeInterface {
			c.logger.Debug("Found active member via netifd",
				"member", member.Name,
				"interface", activeInterface)
			return member, nil
		}
	}

	return nil, fmt.Errorf("no member found for active interface: %s", activeInterface)
}

// getActiveInterfaceFromRoutes determines the active interface from routing table
func (c *Controller) getActiveInterfaceFromRoutes() (string, error) {
	// Execute 'ip route show default' to get default route
	cmd := exec.Command("ip", "route", "show", "default")
	output, err := cmd.Output()
	if err != nil {
		return "", fmt.Errorf("failed to get default route: %w", err)
	}

	// Parse output to extract interface name
	// Expected format: "default via 192.168.1.1 dev eth0 metric 100"
	lines := strings.Split(string(output), "\n")
	for _, line := range lines {
		if strings.Contains(line, "default") && strings.Contains(line, "dev") {
			parts := strings.Fields(line)
			for i, part := range parts {
				if part == "dev" && i+1 < len(parts) {
					iface := parts[i+1]
					c.logger.Debug("Found default route interface", "interface", iface)
					return iface, nil
				}
			}
		}
	}

	return "", fmt.Errorf("no default route found")
}

// getActiveInterfaceFromNetifd queries netifd via ubus for interface status
func (c *Controller) getActiveInterfaceFromNetifd() (string, error) {
	// Query network.interface status via ubus
	cmd := exec.Command("ubus", "call", "network.interface", "dump")
	output, err := cmd.Output()
	if err != nil {
		return "", fmt.Errorf("failed to query network interfaces: %w", err)
	}

	// Parse JSON response
	var response struct {
		Interface []struct {
			Interface string `json:"interface"`
			Up        bool   `json:"up"`
			Available bool   `json:"available"`
			Device    string `json:"device"`
			Route     []struct {
				Target  string `json:"target"`
				Mask    int    `json:"mask"`
				Nexthop string `json:"nexthop"`
				Metric  int    `json:"metric"`
			} `json:"route"`
		} `json:"interface"`
	}

	if err := json.Unmarshal(output, &response); err != nil {
		return "", fmt.Errorf("failed to parse network interface response: %w", err)
	}

	// Find the interface with the default route (0.0.0.0/0)
	var bestInterface string
	bestMetric := int(^uint(0) >> 1) // Max int value

	for _, iface := range response.Interface {
		if !iface.Up || !iface.Available {
			continue
		}

		// Check if this interface has a default route
		for _, route := range iface.Route {
			if route.Target == "0.0.0.0" && route.Mask == 0 {
				// This is a default route
				if route.Metric < bestMetric {
					bestMetric = route.Metric
					bestInterface = iface.Device
					if bestInterface == "" {
						bestInterface = iface.Interface
					}
				}
			}
		}
	}

	if bestInterface != "" {
		c.logger.Debug("Found active interface via netifd",
			"interface", bestInterface,
			"metric", bestMetric)
		return bestInterface, nil
	}

	// Fallback: return the first up and available interface
	for _, iface := range response.Interface {
		if iface.Up && iface.Available && iface.Device != "" {
			c.logger.Debug("Using first available interface as fallback",
				"interface", iface.Device)
			return iface.Device, nil
		}
	}

	return "", fmt.Errorf("no active interface found via netifd")
}

// GetMWAN3Info returns detailed mwan3 information
func (c *Controller) GetMWAN3Info() (map[string]interface{}, error) {
	if !c.mwan3Enabled {
		return nil, fmt.Errorf("mwan3 not enabled")
	}

	status, err := c.getMWAN3Status()
	if err != nil {
		return nil, err
	}

	// Extract relevant information
	info := make(map[string]interface{})

	if interfaces, ok := status["interfaces"].(map[string]interface{}); ok {
		interfaceInfo := make(map[string]interface{})
		for ifaceName, ifaceData := range interfaces {
			if ifaceMap, ok := ifaceData.(map[string]interface{}); ok {
				interfaceInfo[ifaceName] = ifaceMap
			}
		}
		info["interfaces"] = interfaceInfo
	}

	if policies, ok := status["policies"].(map[string]interface{}); ok {
		info["policies"] = policies
	}

	return info, nil
}

// GetNetifdInfo returns detailed netifd information
func (c *Controller) GetNetifdInfo() (map[string]interface{}, error) {
	// Query netifd via ubus
	ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancel()

	cmd := exec.CommandContext(ctx, c.ubusPath, "call", "network", "dump")
	output, err := cmd.Output()
	if err != nil {
		return nil, fmt.Errorf("ubus network dump failed: %w", err)
	}

	var info map[string]interface{}
	if err := json.Unmarshal(output, &info); err != nil {
		return nil, fmt.Errorf("failed to parse netifd info: %w", err)
	}

	return info, nil
}

// TestInterface tests if an interface is reachable
func (c *Controller) TestInterface(member *pkg.Member) error {
	if member == nil {
		return fmt.Errorf("member cannot be nil")
	}

	// Test interface connectivity
	ctx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
	defer cancel()

	// Try to ping through the interface
	cmd := exec.CommandContext(ctx, "ping", "-c", "3", "-I", member.Iface, "8.8.8.8")
	if err := cmd.Run(); err != nil {
		return fmt.Errorf("interface test failed: %w", err)
	}

	return nil
}

// GetInterfaceStats returns interface statistics
func (c *Controller) GetInterfaceStats(member *pkg.Member) (map[string]interface{}, error) {
	if member == nil {
		return nil, fmt.Errorf("member cannot be nil")
	}

	// Read interface statistics from /proc/net/dev
	ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancel()

	cmd := exec.CommandContext(ctx, "cat", "/proc/net/dev")
	output, err := cmd.Output()
	if err != nil {
		return nil, fmt.Errorf("failed to read interface stats: %w", err)
	}

	// Parse the output to find stats for the target interface
	lines := strings.Split(string(output), "\n")
	for _, line := range lines {
		if strings.Contains(line, member.Iface) {
			// Parse interface statistics
			// Format: interface: rx_bytes rx_packets rx_errs rx_drop ... tx_bytes tx_packets ...
			fields := strings.Fields(line)
			if len(fields) >= 17 {
				stats := map[string]interface{}{
					"rx_bytes":   fields[1],
					"rx_packets": fields[2],
					"rx_errors":  fields[3],
					"rx_dropped": fields[4],
					"tx_bytes":   fields[9],
					"tx_packets": fields[10],
					"tx_errors":  fields[11],
					"tx_dropped": fields[12],
				}
				return stats, nil
			}
		}
	}

	return nil, fmt.Errorf("interface stats not found")
}

// IsMWAN3Enabled returns whether mwan3 is enabled
func (c *Controller) IsMWAN3Enabled() bool {
	return c.mwan3Enabled
}

// SetMembers stores the list of active members after validating them
func (c *Controller) SetMembers(members []*pkg.Member) error {
	if members == nil {
		members = []*pkg.Member{}
	}

	for i, m := range members {
		if err := c.Validate(m); err != nil {
			return fmt.Errorf("invalid member at index %d: %w", i, err)
		}
	}

	c.membersMu.Lock()
	defer c.membersMu.Unlock()
	c.members = make([]*pkg.Member, len(members))
	copy(c.members, members)
	return nil
}

// GetMembers returns all available members
func (c *Controller) GetMembers() []*pkg.Member {
	c.membersMu.RLock()
	defer c.membersMu.RUnlock()

	members := make([]*pkg.Member, len(c.members))
	copy(members, c.members)
	return members
}

// GetActiveMember returns the currently active member (alias for GetCurrentMember)
func (c *Controller) GetActiveMember() (*pkg.Member, error) {
	return c.GetCurrentMember()
}

// GetControllerInfo returns general controller information
func (c *Controller) GetControllerInfo() map[string]interface{} {
	return map[string]interface{}{
		"mwan3_enabled": c.mwan3Enabled,
		"mwan3_path":    c.mwan3Path,
		"ubus_path":     c.ubusPath,
		"dry_run":       c.dryRun,
		"current_member": func() string {
			if c.currentMember != nil {
				return c.currentMember.Name
			}
			return "none"
		}(),
	}
}

// SetDryRun enables or disables dry-run mode for the controller
func (c *Controller) SetDryRun(enabled bool) {
	c.dryRun = enabled
}

// AddFailoverCallback adds a callback to be called on failover events
func (c *Controller) AddFailoverCallback(callback FailoverCallback) {
	c.callbacksMu.Lock()
	defer c.callbacksMu.Unlock()
	c.failoverCallbacks = append(c.failoverCallbacks, callback)
}

// callFailoverCallbacks calls all registered failover callbacks
func (c *Controller) callFailoverCallbacks(from, to *pkg.Member) {
	c.callbacksMu.RLock()
	callbacks := make([]FailoverCallback, len(c.failoverCallbacks))
	copy(callbacks, c.failoverCallbacks)
	c.callbacksMu.RUnlock()

	for _, callback := range callbacks {
		if err := callback(from, to); err != nil {
			c.logger.Warn("Failover callback failed", "error", err)
		}
	}
}
