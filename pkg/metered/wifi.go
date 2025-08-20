package metered

import (
	"fmt"
	"os/exec"
	"strings"
	"time"
)

// applyVendorElements applies vendor elements to WiFi interfaces
func (m *Manager) applyVendorElements(elements []VendorElement) error {
	// Get list of AP interfaces
	apSections, err := m.getAPSections()
	if err != nil {
		return fmt.Errorf("failed to get AP sections: %w", err)
	}

	if len(apSections) == 0 {
		m.logger.Info("No AP interfaces found, skipping vendor element application")
		return nil
	}

	// Apply vendor elements to each AP interface
	for _, section := range apSections {
		if err := m.applyVendorElementsToSection(section, elements); err != nil {
			m.logger.Warn("Failed to apply vendor elements to section",
				"section", section, "error", err)
			// Continue with other sections
		}
	}

	// Commit wireless configuration and reload WiFi
	if err := m.reloadWiFi(); err != nil {
		return fmt.Errorf("failed to reload WiFi: %w", err)
	}

	m.logger.Info("Successfully applied vendor elements to WiFi interfaces",
		"sections", len(apSections),
		"elements", len(elements))

	return nil
}

// getAPSections returns list of WiFi AP section names
func (m *Manager) getAPSections() ([]string, error) {
	cmd := exec.Command("uci", "show", "wireless")
	output, err := cmd.Output()
	if err != nil {
		return nil, fmt.Errorf("failed to query wireless config: %w", err)
	}

	var apSections []string
	lines := strings.Split(string(output), "\n")

	for _, line := range lines {
		// Look for wifi-iface sections
		if strings.Contains(line, "=wifi-iface") {
			parts := strings.Split(line, "=")
			if len(parts) > 0 {
				section := parts[0]

				// Check if this section is in AP mode
				if m.isAPSection(section) {
					apSections = append(apSections, section)
				}
			}
		}
	}

	return apSections, nil
}

// isAPSection checks if a wireless section is in AP mode
func (m *Manager) isAPSection(section string) bool {
	cmd := exec.Command("uci", "-q", "get", section+".mode")
	output, err := cmd.Output()
	if err != nil {
		return false
	}

	mode := strings.TrimSpace(string(output))
	return mode == "ap"
}

// applyVendorElementsToSection applies vendor elements to a specific wireless section
func (m *Manager) applyVendorElementsToSection(section string, elements []VendorElement) error {
	// Clear existing hostapd_options to avoid accumulation
	if err := m.clearHostapdOptions(section); err != nil {
		return fmt.Errorf("failed to clear hostapd options: %w", err)
	}

	// Add vendor elements
	for _, element := range elements {
		if err := m.addVendorElement(section, element); err != nil {
			return fmt.Errorf("failed to add vendor element %s: %w", element.Name, err)
		}

		if m.debugEnabled {
			m.logger.Debug("Added vendor element to section",
				"section", section,
				"element", element.Name,
				"data", element.Data)
		}
	}

	return nil
}

// clearHostapdOptions clears all hostapd_options for a section
func (m *Manager) clearHostapdOptions(section string) error {
	cmd := exec.Command("uci", "-q", "delete", section+".hostapd_options")
	// Ignore error if option doesn't exist
	_ = cmd.Run()
	return nil
}

// addVendorElement adds a vendor element to a wireless section
func (m *Manager) addVendorElement(section string, element VendorElement) error {
	vendorElementOption := fmt.Sprintf("vendor_elements=%s", strings.ToUpper(element.Data))

	cmd := exec.Command("uci", "add_list", section+".hostapd_options="+vendorElementOption)
	if err := cmd.Run(); err != nil {
		return fmt.Errorf("failed to add vendor element: %w", err)
	}

	return nil
}

// updateDHCPConfiguration updates DHCP configuration for Android metered signaling
func (m *Manager) updateDHCPConfiguration(mode Mode) error {
	// Remove any existing android tag sections
	if err := m.removeAndroidDHCPTags(); err != nil {
		m.logger.Warn("Failed to remove existing android DHCP tags", "error", err)
	}

	// Add android metered DHCP option for metered modes
	if mode != ModeOff && mode != ModeTetheredNoLimit {
		if err := m.enableAndroidMeteredDHCP(); err != nil {
			return fmt.Errorf("failed to enable android metered DHCP: %w", err)
		}
	}

	// Commit DHCP configuration and restart dnsmasq
	if err := m.reloadDHCP(); err != nil {
		return fmt.Errorf("failed to reload DHCP: %w", err)
	}

	return nil
}

// removeAndroidDHCPTags removes existing android DHCP tag sections
func (m *Manager) removeAndroidDHCPTags() error {
	// Get list of DHCP tag sections
	cmd := exec.Command("uci", "show", "dhcp")
	output, err := cmd.Output()
	if err != nil {
		return fmt.Errorf("failed to query DHCP config: %w", err)
	}

	lines := strings.Split(string(output), "\n")
	for _, line := range lines {
		if strings.Contains(line, "dhcp.@tag[") && strings.Contains(line, ".name='android'") {
			// Extract tag index
			parts := strings.Split(line, ".")
			if len(parts) >= 2 {
				tagSection := parts[0] + "." + parts[1]
				cmd := exec.Command("uci", "-q", "delete", tagSection)
				_ = cmd.Run() // Ignore errors
			}
		}
	}

	// Also remove android tag references from lan interface
	cmd = exec.Command("uci", "-q", "del_list", "dhcp.lan.tag=android")
	_ = cmd.Run() // Ignore errors

	return nil
}

// enableAndroidMeteredDHCP enables Android metered DHCP signaling
func (m *Manager) enableAndroidMeteredDHCP() error {
	// Add new android tag section
	cmd := exec.Command("uci", "add", "dhcp", "tag")
	if err := cmd.Run(); err != nil {
		return fmt.Errorf("failed to add DHCP tag section: %w", err)
	}

	// Configure the tag
	commands := [][]string{
		{"uci", "set", "dhcp.@tag[-1].name=android"},
		{"uci", "set", "dhcp.@tag[-1].vendorid=Android"},
		{"uci", "add_list", "dhcp.@tag[-1].dhcp_option=43,ANDROID_METERED"},
		{"uci", "add_list", "dhcp.lan.tag=android"},
	}

	for _, cmdArgs := range commands {
		cmd := exec.Command(cmdArgs[0], cmdArgs[1:]...)
		if err := cmd.Run(); err != nil {
			return fmt.Errorf("failed to configure android DHCP tag: %w", err)
		}
	}

	return nil
}

// triggerClientReconnection triggers client reconnection based on configured method
func (m *Manager) triggerClientReconnection() error {
	switch m.clientReconnectMethod {
	case "force":
		return m.forceClientReconnection()
	case "gentle":
		return m.gentleClientReconnection()
	default:
		m.logger.Warn("Unknown client reconnection method", "method", m.clientReconnectMethod)
		return m.gentleClientReconnection()
	}
}

// gentleClientReconnection triggers gentle client reconnection via beacon updates
func (m *Manager) gentleClientReconnection() error {
	// Gentle reconnection relies on beacon updates after WiFi reload
	// Clients will see the new vendor elements in subsequent beacons
	m.logger.Info("Using gentle client reconnection (beacon updates)")
	return nil
}

// forceClientReconnection forces client reconnection via deauthentication
func (m *Manager) forceClientReconnection() error {
	// Get list of WiFi interfaces
	interfaces, err := m.getWiFiInterfaces()
	if err != nil {
		return fmt.Errorf("failed to get WiFi interfaces: %w", err)
	}

	for _, iface := range interfaces {
		if err := m.deauthenticateClients(iface); err != nil {
			m.logger.Warn("Failed to deauthenticate clients on interface",
				"interface", iface, "error", err)
			// Continue with other interfaces
		}
	}

	m.logger.Info("Forced client reconnection via deauthentication",
		"interfaces", len(interfaces))

	return nil
}

// getWiFiInterfaces gets list of active WiFi interfaces
func (m *Manager) getWiFiInterfaces() ([]string, error) {
	cmd := exec.Command("iw", "dev")
	output, err := cmd.Output()
	if err != nil {
		return nil, fmt.Errorf("failed to get WiFi interfaces: %w", err)
	}

	var interfaces []string
	lines := strings.Split(string(output), "\n")

	for _, line := range lines {
		line = strings.TrimSpace(line)
		if strings.HasPrefix(line, "Interface ") {
			parts := strings.Fields(line)
			if len(parts) >= 2 {
				interfaces = append(interfaces, parts[1])
			}
		}
	}

	return interfaces, nil
}

// deauthenticateClients deauthenticates all clients on an interface
func (m *Manager) deauthenticateClients(iface string) error {
	// Use hostapd_cli to deauthenticate all clients
	cmd := exec.Command("hostapd_cli", "-i", iface, "deauthenticate", "ff:ff:ff:ff:ff:ff")
	if err := cmd.Run(); err != nil {
		return fmt.Errorf("failed to deauthenticate clients: %w", err)
	}

	return nil
}

// reloadWiFi commits wireless configuration and reloads WiFi
func (m *Manager) reloadWiFi() error {
	// Commit wireless configuration
	cmd := exec.Command("uci", "commit", "wireless")
	if err := cmd.Run(); err != nil {
		return fmt.Errorf("failed to commit wireless config: %w", err)
	}

	// Reload WiFi
	cmd = exec.Command("wifi", "reload")
	if err := cmd.Run(); err != nil {
		return fmt.Errorf("failed to reload WiFi: %w", err)
	}

	// Give WiFi time to reload
	time.Sleep(2 * time.Second)

	return nil
}

// reloadDHCP commits DHCP configuration and restarts dnsmasq
func (m *Manager) reloadDHCP() error {
	// Commit DHCP configuration
	cmd := exec.Command("uci", "commit", "dhcp")
	if err := cmd.Run(); err != nil {
		return fmt.Errorf("failed to commit DHCP config: %w", err)
	}

	// Restart dnsmasq
	cmd = exec.Command("/etc/init.d/dnsmasq", "restart")
	if err := cmd.Run(); err != nil {
		return fmt.Errorf("failed to restart dnsmasq: %w", err)
	}

	return nil
}

// GetVendorElementStatus returns current vendor element status
func (m *Manager) GetVendorElementStatus() (map[string]interface{}, error) {
	status := make(map[string]interface{})

	// Get WiFi interfaces and their vendor elements
	interfaces, err := m.getWiFiInterfaces()
	if err != nil {
		return nil, fmt.Errorf("failed to get WiFi interfaces: %w", err)
	}

	for _, iface := range interfaces {
		elements, err := m.getInterfaceVendorElements(iface)
		if err != nil {
			m.logger.Warn("Failed to get vendor elements for interface",
				"interface", iface, "error", err)
			continue
		}

		status[iface] = elements
	}

	// Get DHCP android tag status
	androidDHCP, err := m.getAndroidDHCPStatus()
	if err != nil {
		m.logger.Warn("Failed to get Android DHCP status", "error", err)
	} else {
		status["android_dhcp"] = androidDHCP
	}

	return status, nil
}

// getInterfaceVendorElements gets vendor elements for a specific interface
func (m *Manager) getInterfaceVendorElements(iface string) ([]string, error) {
	cmd := exec.Command("hostapd_cli", "-i", iface, "get_config")
	output, err := cmd.Output()
	if err != nil {
		return nil, fmt.Errorf("failed to get hostapd config: %w", err)
	}

	var elements []string
	lines := strings.Split(string(output), "\n")

	for _, line := range lines {
		if strings.HasPrefix(line, "vendor_elements=") {
			element := strings.TrimPrefix(line, "vendor_elements=")
			elements = append(elements, element)
		}
	}

	return elements, nil
}

// getAndroidDHCPStatus gets Android DHCP tag status
func (m *Manager) getAndroidDHCPStatus() (map[string]interface{}, error) {
	status := make(map[string]interface{})

	// Check for android tags
	cmd := exec.Command("uci", "show", "dhcp")
	output, err := cmd.Output()
	if err != nil {
		return nil, fmt.Errorf("failed to query DHCP config: %w", err)
	}

	hasAndroidTag := false
	lines := strings.Split(string(output), "\n")

	for _, line := range lines {
		if strings.Contains(line, ".name='android'") {
			hasAndroidTag = true
			break
		}
	}

	status["enabled"] = hasAndroidTag

	// Check if lan interface has android tag
	cmd = exec.Command("uci", "show", "dhcp.lan")
	output, err = cmd.Output()
	if err == nil {
		lanConfig := string(output)
		status["lan_configured"] = strings.Contains(lanConfig, "tag='android'")
	}

	return status, nil
}
