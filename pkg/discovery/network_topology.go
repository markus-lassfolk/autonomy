package discovery

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

// NetworkTopology represents the complete network configuration
type NetworkTopology struct {
	PhysicalInterfaces map[string]*PhysicalInterface `json:"physical_interfaces"`
	LogicalInterfaces  map[string]*LogicalInterface  `json:"logical_interfaces"`
	MWAN3Members       map[string]*MWAN3Member       `json:"mwan3_members"`
	DataLimits         map[string]*DataLimitConfig   `json:"data_limits"`
	logger             *logx.Logger
	dataLimitManager   *DataLimitManager
}

// PhysicalInterface represents a physical network device
type PhysicalInterface struct {
	Name   string `json:"name"`
	Type   string `json:"type"`   // ethernet, cellular, wireless, bridge
	State  string `json:"state"`  // UP, DOWN, UNKNOWN
	Master string `json:"master"` // bridge master if applicable
}

// LogicalInterface represents a network interface configuration
type LogicalInterface struct {
	Name        string            `json:"name"`
	Protocol    string            `json:"protocol"`     // dhcp, wwan, static, etc.
	Device      string            `json:"device"`       // physical device
	AreaType    string            `json:"area_type"`    // wan, lan
	AutoConnect bool              `json:"auto_connect"` // auto connection enabled
	Status      *InterfaceStatus  `json:"status"`       // runtime status
	Config      map[string]string `json:"config"`       // additional config
}

// InterfaceStatus represents runtime interface status
type InterfaceStatus struct {
	Up        bool     `json:"up"`
	Available bool     `json:"available"`
	HasIP     bool     `json:"has_ip"`
	IPAddress string   `json:"ip_address"`
	Device    string   `json:"device"`
	Uptime    int      `json:"uptime"`
	Routes    []string `json:"routes"`
}

// MWAN3Member represents an mwan3 member configuration
type MWAN3Member struct {
	Name      string `json:"name"`
	Interface string `json:"interface"` // logical interface name
	Weight    int    `json:"weight"`
	Metric    int    `json:"metric"`
	Enabled   bool   `json:"enabled"`
}

// NewNetworkTopologyDiscoverer creates a new topology-based discoverer
func NewNetworkTopologyDiscoverer(logger *logx.Logger) *NetworkTopology {
	return &NetworkTopology{
		PhysicalInterfaces: make(map[string]*PhysicalInterface),
		LogicalInterfaces:  make(map[string]*LogicalInterface),
		MWAN3Members:       make(map[string]*MWAN3Member),
		DataLimits:         make(map[string]*DataLimitConfig),
		logger:             logger,
		dataLimitManager:   NewDataLimitManager(logger),
	}
}

// DiscoverTopology performs complete network topology discovery
func (nt *NetworkTopology) DiscoverTopology(ctx context.Context) error {
	nt.logger.Info("Starting comprehensive network topology discovery")

	// Step 1: Discover physical interfaces
	if err := nt.discoverPhysicalInterfaces(); err != nil {
		return fmt.Errorf("failed to discover physical interfaces: %w", err)
	}

	// Step 2: Discover logical network interfaces
	if err := nt.discoverLogicalInterfaces(); err != nil {
		return fmt.Errorf("failed to discover logical interfaces: %w", err)
	}

	// Step 3: Get runtime status for logical interfaces
	if err := nt.discoverInterfaceStatus(); err != nil {
		return fmt.Errorf("failed to discover interface status: %w", err)
	}

	// Step 4: Discover MWAN3 members
	if err := nt.discoverMWAN3Members(); err != nil {
		return fmt.Errorf("failed to discover MWAN3 members: %w", err)
	}

	// Step 5: Discover data limits
	if err := nt.discoverDataLimits(); err != nil {
		return fmt.Errorf("failed to discover data limits: %w", err)
	}

	nt.logger.Info("Network topology discovery completed",
		"physical_interfaces", len(nt.PhysicalInterfaces),
		"logical_interfaces", len(nt.LogicalInterfaces),
		"mwan3_members", len(nt.MWAN3Members),
		"data_limits", len(nt.DataLimits))

	return nil
}

// GetViableMembers returns only members that should be monitored
func (nt *NetworkTopology) GetViableMembers() ([]*pkg.Member, error) {
	var members []*pkg.Member

	for memberName, mwan3Member := range nt.MWAN3Members {
		// Get the logical interface
		logicalIface, exists := nt.LogicalInterfaces[mwan3Member.Interface]
		if !exists {
			nt.logger.Warn("MWAN3 member references non-existent interface",
				"member", memberName, "interface", mwan3Member.Interface)
			continue
		}

		// Check if interface is viable for monitoring
		if !nt.isInterfaceViable(logicalIface) {
			nt.logger.Info("Skipping non-viable interface",
				"member", memberName,
				"interface", mwan3Member.Interface,
				"reason", nt.getViabilityReason(logicalIface))
			continue
		}

		// Create member
		member := &pkg.Member{
			Name:      memberName,
			Iface:     mwan3Member.Interface,
			Weight:    mwan3Member.Weight,
			Class:     pkg.InterfaceClass(nt.classifyInterface(logicalIface)),
			Eligible:  true,
			CreatedAt: time.Now(),
			LastSeen:  time.Now(),
			Detect:    "topology",
			Config:    make(map[string]string),
		}

		// Add configuration details
		member.Config["metric"] = strconv.Itoa(mwan3Member.Metric)
		member.Config["protocol"] = logicalIface.Protocol
		member.Config["device"] = logicalIface.Device
		if logicalIface.Status != nil {
			member.Config["ip_address"] = logicalIface.Status.IPAddress
		}

		// Add data limit information for cellular interfaces
		if dataLimit, exists := nt.DataLimits[mwan3Member.Interface]; exists && dataLimit.Enabled {
			// Set the DataLimitConfig field for adaptive monitoring
			member.DataLimitConfig = dataLimit

			// Also store in Config map for backward compatibility and logging
			member.Config["data_limit_mb"] = strconv.Itoa(dataLimit.DataLimitMB)
			member.Config["data_usage_mb"] = fmt.Sprintf("%.2f", dataLimit.CurrentUsageMB)
			member.Config["data_usage_percent"] = fmt.Sprintf("%.1f", dataLimit.UsagePercentage)
			member.Config["data_limit_status"] = nt.dataLimitManager.GetDataLimitStatus(dataLimit).String()

			// Adjust weight based on data limit status
			priority := nt.dataLimitManager.GetFailoverPriority(dataLimit)
			member.Config["data_limit_priority"] = strconv.Itoa(priority)

			nt.logger.Debug("Added data limit info to member",
				"member", memberName,
				"limit_mb", dataLimit.DataLimitMB,
				"usage_mb", dataLimit.CurrentUsageMB,
				"usage_percent", dataLimit.UsagePercentage,
				"status", nt.dataLimitManager.GetDataLimitStatus(dataLimit).String(),
				"priority", priority)
		}

		members = append(members, member)
		nt.logger.Info("Added viable member",
			"member", memberName,
			"interface", mwan3Member.Interface,
			"class", member.Class,
			"weight", member.Weight)
	}

	return members, nil
}

// isInterfaceViable checks if an interface should be monitored
func (nt *NetworkTopology) isInterfaceViable(iface *LogicalInterface) bool {
	// Must be WAN area type
	if iface.AreaType != "wan" {
		return false
	}

	// Must be up and available
	if iface.Status == nil || !iface.Status.Up || !iface.Status.Available {
		return false
	}

	// Must have IP address for actual connectivity
	if !iface.Status.HasIP || iface.Status.IPAddress == "" {
		return false
	}

	// For cellular interfaces, prefer those with auto-connect enabled, but allow manual ones if they have IP
	if strings.Contains(iface.Name, "mob") && !iface.AutoConnect {
		nt.logger.Debug("Cellular interface has auto-connect disabled but has IP",
			"interface", iface.Name, "ip", iface.Status.IPAddress)
		// Allow it since it has connectivity
	}

	return true
}

// getViabilityReason returns human-readable reason why interface is not viable
func (nt *NetworkTopology) getViabilityReason(iface *LogicalInterface) string {
	if iface.AreaType != "wan" {
		return "not_wan_interface"
	}
	if strings.Contains(iface.Name, "mob") && !iface.AutoConnect {
		return "auto_connect_disabled"
	}
	if iface.Status == nil {
		return "no_status_info"
	}
	if !iface.Status.Up {
		return "interface_down"
	}
	if !iface.Status.Available {
		return "interface_not_available"
	}
	if !iface.Status.HasIP {
		return "no_ip_address"
	}
	return "unknown"
}

// classifyInterface determines the interface class based on protocol and device
func (nt *NetworkTopology) classifyInterface(iface *LogicalInterface) string {
	switch iface.Protocol {
	case "wwan":
		return string(pkg.ClassCellular)
	case "dhcp":
		// Check if it's Starlink based on IP range or device
		if iface.Status != nil && strings.HasPrefix(iface.Status.IPAddress, "100.") {
			return string(pkg.ClassStarlink) // CGNAT range used by Starlink
		}
		if strings.Contains(iface.Device, "eth") {
			return string(pkg.ClassStarlink) // Ethernet-based WAN is likely Starlink
		}
		return string(pkg.ClassLAN)
	case "static":
		return string(pkg.ClassLAN)
	default:
		return string(pkg.ClassOther)
	}
}

// discoverPhysicalInterfaces discovers physical network devices
func (nt *NetworkTopology) discoverPhysicalInterfaces() error {
	cmd := exec.Command("ip", "link", "show")
	output, err := cmd.Output()
	if err != nil {
		return fmt.Errorf("failed to get physical interfaces: %w", err)
	}

	lines := strings.Split(string(output), "\n")
	for _, line := range lines {
		if strings.Contains(line, ":") && !strings.HasPrefix(line, " ") {
			parts := strings.Fields(line)
			if len(parts) >= 2 {
				name := strings.TrimSuffix(parts[1], ":")
				if name == "lo" {
					continue // Skip loopback
				}

				iface := &PhysicalInterface{
					Name:  name,
					Type:  nt.classifyPhysicalInterface(name, line),
					State: nt.extractState(line),
				}

				// Extract master if bridged
				if strings.Contains(line, "master") {
					masterIdx := strings.Index(line, "master")
					remaining := line[masterIdx:]
					parts := strings.Fields(remaining)
					if len(parts) >= 2 {
						iface.Master = parts[1]
					}
				}

				nt.PhysicalInterfaces[name] = iface
			}
		}
	}

	return nil
}

// discoverLogicalInterfaces discovers UCI network interface configurations
func (nt *NetworkTopology) discoverLogicalInterfaces() error {
	cmd := exec.Command("uci", "show", "network")
	output, err := cmd.Output()
	if err != nil {
		return fmt.Errorf("failed to get network config: %w", err)
	}

	interfaces := make(map[string]*LogicalInterface)
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
				ifaceName := parts[1]
				if strings.Contains(ifaceName, "=") {
					ifaceName = strings.Split(ifaceName, "=")[0]
				}

				interfaces[ifaceName] = &LogicalInterface{
					Name:   ifaceName,
					Config: make(map[string]string),
				}
			}
		}

		// Parse interface properties
		for ifaceName := range interfaces {
			prefix := fmt.Sprintf("network.%s.", ifaceName)
			if strings.HasPrefix(line, prefix) {
				parts := strings.Split(line, "=")
				if len(parts) == 2 {
					key := strings.TrimPrefix(parts[0], prefix)
					value := strings.Trim(parts[1], "'\"")

					switch key {
					case "proto":
						interfaces[ifaceName].Protocol = value
					case "device":
						interfaces[ifaceName].Device = value
					case "area_type":
						interfaces[ifaceName].AreaType = value
					case "auto":
						interfaces[ifaceName].AutoConnect = (value == "1")
					default:
						interfaces[ifaceName].Config[key] = value
					}
				}
			}
		}
	}

	// Filter to only WAN interfaces
	for name, iface := range interfaces {
		if iface.AreaType == "wan" {
			nt.LogicalInterfaces[name] = iface
		}
	}

	return nil
}

// discoverInterfaceStatus gets runtime status for logical interfaces
func (nt *NetworkTopology) discoverInterfaceStatus() error {
	for name, iface := range nt.LogicalInterfaces {
		cmd := exec.Command("ubus", "call", fmt.Sprintf("network.interface.%s", name), "status")
		output, err := cmd.Output()
		if err != nil {
			nt.logger.Debug("Failed to get interface status", "interface", name, "error", err)
			continue
		}

		var status map[string]interface{}
		if err := json.Unmarshal(output, &status); err != nil {
			nt.logger.Debug("Failed to parse interface status", "interface", name, "error", err)
			continue
		}

		ifaceStatus := &InterfaceStatus{}

		// Parse basic status
		if up, ok := status["up"].(bool); ok {
			ifaceStatus.Up = up
		}
		if available, ok := status["available"].(bool); ok {
			ifaceStatus.Available = available
		}
		if device, ok := status["l3_device"].(string); ok {
			ifaceStatus.Device = device
		}
		if uptime, ok := status["uptime"].(float64); ok {
			ifaceStatus.Uptime = int(uptime)
		}

		// Check for IP addresses in ubus status
		if ipv4Addrs, ok := status["ipv4-address"].([]interface{}); ok && len(ipv4Addrs) > 0 {
			if addr, ok := ipv4Addrs[0].(map[string]interface{}); ok {
				if ip, ok := addr["address"].(string); ok {
					ifaceStatus.HasIP = true
					ifaceStatus.IPAddress = ip
				}
			}
		}

		// For cellular interfaces, also check physical qmimux interfaces
		if !ifaceStatus.HasIP && strings.Contains(name, "mob") {
			ifaceStatus.HasIP, ifaceStatus.IPAddress = nt.checkPhysicalInterfaceIP(name)
		}

		// Parse routes
		if routes, ok := status["route"].([]interface{}); ok {
			for _, route := range routes {
				if routeMap, ok := route.(map[string]interface{}); ok {
					if target, ok := routeMap["target"].(string); ok {
						ifaceStatus.Routes = append(ifaceStatus.Routes, target)
					}
				}
			}
		}

		iface.Status = ifaceStatus
	}

	return nil
}

// checkPhysicalInterfaceIP checks if physical cellular interfaces have IP addresses
func (nt *NetworkTopology) checkPhysicalInterfaceIP(logicalName string) (bool, string) {
	// Map logical cellular interfaces to physical qmimux interfaces
	var physicalIface string
	switch logicalName {
	case "mob1s1a1":
		physicalIface = "qmimux0"
	case "mob1s2a1":
		physicalIface = "qmimux1"
	default:
		return false, ""
	}

	// Check if physical interface has IP
	cmd := exec.Command("ip", "addr", "show", physicalIface)
	output, err := cmd.Output()
	if err != nil {
		nt.logger.Debug("Failed to check physical interface", "interface", physicalIface, "error", err)
		return false, ""
	}

	lines := strings.Split(string(output), "\n")
	for _, line := range lines {
		if strings.Contains(line, "inet ") && !strings.Contains(line, "127.0.0.1") {
			parts := strings.Fields(line)
			for i, part := range parts {
				if part == "inet" && i+1 < len(parts) {
					ipCidr := parts[i+1]
					ip := strings.Split(ipCidr, "/")[0]
					if !strings.HasPrefix(ip, "169.254.") && ip != "0.0.0.0" {
						nt.logger.Debug("Found IP on physical interface",
							"logical", logicalName, "physical", physicalIface, "ip", ip)
						return true, ip
					}
				}
			}
		}
	}

	return false, ""
}

// discoverMWAN3Members discovers MWAN3 member configurations
func (nt *NetworkTopology) discoverMWAN3Members() error {
	cmd := exec.Command("uci", "show", "mwan3")
	output, err := cmd.Output()
	if err != nil {
		return fmt.Errorf("failed to get mwan3 config: %w", err)
	}

	members := make(map[string]*MWAN3Member)
	lines := strings.Split(string(output), "\n")

	for _, line := range lines {
		line = strings.TrimSpace(line)
		if line == "" {
			continue
		}

		// Parse member definitions
		if strings.Contains(line, "=member") {
			parts := strings.Split(line, ".")
			if len(parts) >= 2 {
				memberName := strings.Split(parts[1], "=")[0]
				members[memberName] = &MWAN3Member{
					Name:    memberName,
					Enabled: true, // Default to enabled
				}
			}
		}

		// Parse member properties
		for memberName := range members {
			prefix := fmt.Sprintf("mwan3.%s.", memberName)
			if strings.HasPrefix(line, prefix) {
				parts := strings.Split(line, "=")
				if len(parts) == 2 {
					key := strings.TrimPrefix(parts[0], prefix)
					value := strings.Trim(parts[1], "'\"")

					switch key {
					case "interface":
						members[memberName].Interface = value
					case "weight":
						if w, err := strconv.Atoi(value); err == nil {
							members[memberName].Weight = w
						}
					case "metric":
						if m, err := strconv.Atoi(value); err == nil {
							members[memberName].Metric = m
						}
					}
				}
			}
		}
	}

	nt.MWAN3Members = members
	return nil
}

// discoverDataLimits discovers data limit configurations
func (nt *NetworkTopology) discoverDataLimits() error {
	limits, err := nt.dataLimitManager.DiscoverDataLimits()
	if err != nil {
		return fmt.Errorf("failed to discover data limits: %w", err)
	}

	nt.DataLimits = limits

	// Log data limit status for each interface
	for ifaceName, limit := range limits {
		if limit.Enabled {
			status := nt.dataLimitManager.GetDataLimitStatus(limit)
			nt.logger.Info("Discovered data limits",
				"interface", ifaceName,
				"limit_mb", limit.DataLimitMB,
				"usage_mb", limit.CurrentUsageMB,
				"usage_percent", limit.UsagePercentage,
				"status", status.String(),
				"days_until_reset", limit.DaysUntilReset)
		} else {
			nt.logger.Debug("Data limits disabled", "interface", ifaceName)
		}
	}

	return nil
}

// Helper methods
func (nt *NetworkTopology) classifyPhysicalInterface(name, line string) string {
	if strings.HasPrefix(name, "eth") {
		return "ethernet"
	}
	if strings.HasPrefix(name, "wwan") || strings.HasPrefix(name, "qmimux") {
		return "cellular"
	}
	if strings.HasPrefix(name, "wlan") {
		return "wireless"
	}
	if strings.HasPrefix(name, "br-") {
		return "bridge"
	}
	return "other"
}

func (nt *NetworkTopology) extractState(line string) string {
	if strings.Contains(line, "state UP") {
		return "UP"
	}
	if strings.Contains(line, "state DOWN") {
		return "DOWN"
	}
	return "UNKNOWN"
}
