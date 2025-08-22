package main

import (
	"fmt"
	"strconv"
	"strings"

	"golang.org/x/crypto/ssh"
)

// Enhanced5GCellInfo represents 5G NR cell information
type Enhanced5GCellInfo struct {
	NCI      int    `json:"nci"`       // New Radio Cell Identity
	GSCN     int    `json:"gscn"`      // Global Synchronization Channel Number
	RSRP     int    `json:"rsrp"`      // Reference Signal Received Power
	RSRQ     int    `json:"rsrq"`      // Reference Signal Received Quality
	SINR     int    `json:"sinr"`      // Signal-to-Interference-plus-Noise Ratio
	Band     string `json:"band"`      // 5G NR Band (e.g., "N78", "N1")
	CellType string `json:"cell_type"` // "serving" or "neighbor"
}

// Enhanced5GNetworkInfo represents comprehensive 5G network information
type Enhanced5GNetworkInfo struct {
	Mode               string                        `json:"mode"`                // "5G-SA", "5G-NSA", "LTE"
	LTEAnchor          *CellularLocationIntelligence `json:"lte_anchor"`          // LTE anchor cell (for NSA)
	NRCells            []Enhanced5GCellInfo          `json:"nr_cells"`            // 5G NR cells
	CarrierAggregation bool                          `json:"carrier_aggregation"` // CA active
	RegistrationStatus string                        `json:"registration_status"` // 5G registration status
}

// collect5GNetworkInfo collects comprehensive 5G network information
func collect5GNetworkInfo(client *ssh.Client) (*Enhanced5GNetworkInfo, error) {
	info := &Enhanced5GNetworkInfo{
		NRCells: make([]Enhanced5GCellInfo, 0),
	}

	fmt.Println("📡 Collecting Enhanced 5G Network Information...")

	// Get network mode
	if mode, err := executeCommand(client, "gsmctl -F"); err == nil {
		info.Mode = strings.TrimSpace(mode)
		fmt.Printf("  🌐 Network Mode: %s\n", info.Mode)
	}

	// Get 5G registration status
	if reg, err := executeCommand(client, "gsmctl -A 'AT+C5GREG?'"); err == nil {
		info.RegistrationStatus = strings.TrimSpace(reg)
		fmt.Printf("  📋 5G Registration: %s\n", info.RegistrationStatus)
	}

	// Check carrier aggregation
	if ca, err := executeCommand(client, "gsmctl -G"); err == nil {
		info.CarrierAggregation = strings.Contains(ca, "CA 1") || strings.Contains(ca, "Secondary:")
		fmt.Printf("  🔗 Carrier Aggregation: %t\n", info.CarrierAggregation)
	}

	// Try to get LTE anchor information (for NSA mode)
	if strings.Contains(info.Mode, "5G-NSA") {
		fmt.Println("  📡 5G-NSA Mode: Collecting LTE anchor information...")
		if lteInfo, err := collectCellularLocationIntelligence(client); err == nil {
			info.LTEAnchor = lteInfo
			fmt.Printf("  ⚓ LTE Anchor: Cell %s, Band %s\n",
				lteInfo.ServingCell.CellID, lteInfo.NetworkInfo.Band)
		}
	}

	// Try alternative 5G NR AT commands
	nrCommands := []string{
		"AT+QENG=\"NR5G\"",
		"AT+QNWINFO",
		"AT+QCSQ",
		"AT+QRSRP",
		"AT+QSINR",
	}

	fmt.Println("  🔍 Attempting 5G NR data collection...")
	for _, cmd := range nrCommands {
		if output, err := executeCommand(client, fmt.Sprintf("gsmctl -A '%s'", cmd)); err == nil {
			output = strings.TrimSpace(output)
			if output != "" && !strings.Contains(output, "ERROR") {
				fmt.Printf("    ✅ %s: %s\n", cmd, output)
				// Parse 5G NR data if available
				if nrCells := parse5GNRData(output, cmd); len(nrCells) > 0 {
					info.NRCells = append(info.NRCells, nrCells...)
				}
			} else {
				fmt.Printf("    ❌ %s: No data\n", cmd)
			}
		}
	}

	return info, nil
}

// parse5GNRData parses 5G NR cell data from AT command responses
func parse5GNRData(output, command string) []Enhanced5GCellInfo {
	var cells []Enhanced5GCellInfo

	lines := strings.Split(output, "\n")
	for _, line := range lines {
		line = strings.TrimSpace(line)

		// Parse different 5G NR response formats
		if strings.Contains(command, "QNWINFO") && strings.Contains(line, "NR5G") {
			// Example: +QNWINFO: "NR5G","24001","NR5G BAND 78",3600
			if cell := parseQNWINFO(line); cell != nil {
				cells = append(cells, *cell)
			}
		} else if strings.Contains(command, "QCSQ") && strings.Contains(line, "NR5G") {
			// Example: +QCSQ: "NR5G",-85,-12,30,-
			if cell := parseQCSQ(line); cell != nil {
				cells = append(cells, *cell)
			}
		}
		// Add more parsers as needed
	}

	return cells
}

// parseQNWINFO parses +QNWINFO response for 5G NR information
func parseQNWINFO(line string) *Enhanced5GCellInfo {
	// +QNWINFO: "NR5G","24001","NR5G BAND 78",3600
	if !strings.Contains(line, "+QNWINFO:") || !strings.Contains(line, "NR5G") {
		return nil
	}

	parts := strings.Split(line, ",")
	if len(parts) < 4 {
		return nil
	}

	cell := &Enhanced5GCellInfo{
		CellType: "serving",
	}

	// Extract band information
	if len(parts) >= 3 {
		bandStr := strings.Trim(parts[2], "\"")
		if strings.Contains(bandStr, "BAND") {
			// Extract band number (e.g., "NR5G BAND 78" -> "N78")
			bandParts := strings.Fields(bandStr)
			if len(bandParts) >= 3 {
				cell.Band = "N" + bandParts[2]
			}
		}
	}

	// Extract frequency if available
	if len(parts) >= 4 {
		if freq, err := strconv.Atoi(strings.TrimSpace(parts[3])); err == nil {
			cell.GSCN = freq
		}
	}

	return cell
}

// parseQCSQ parses +QCSQ response for 5G NR signal quality
func parseQCSQ(line string) *Enhanced5GCellInfo {
	// +QCSQ: "NR5G",-85,-12,30,-
	if !strings.Contains(line, "+QCSQ:") || !strings.Contains(line, "NR5G") {
		return nil
	}

	parts := strings.Split(line, ",")
	if len(parts) < 4 {
		return nil
	}

	cell := &Enhanced5GCellInfo{
		CellType: "serving",
	}

	// Parse signal values
	if len(parts) >= 2 {
		if rsrp, err := strconv.Atoi(strings.TrimSpace(parts[1])); err == nil {
			cell.RSRP = rsrp
		}
	}
	if len(parts) >= 3 {
		if rsrq, err := strconv.Atoi(strings.TrimSpace(parts[2])); err == nil {
			cell.RSRQ = rsrq
		}
	}
	if len(parts) >= 4 {
		if sinr, err := strconv.Atoi(strings.TrimSpace(parts[3])); err == nil {
			cell.SINR = sinr
		}
	}

	return cell
}

// test5GEnhancedCollection tests the enhanced 5G data collection
func test5GEnhancedCollection() error {
	fmt.Println("📡 ENHANCED 5G NETWORK ANALYSIS")
	fmt.Println("=" + strings.Repeat("=", 30))

	// Connect to RutOS
	client, err := createSSHClient()
	if err != nil {
		return fmt.Errorf("failed to connect to RutOS: %w", err)
	}
	defer client.Close()

	// Collect enhanced 5G information
	info, err := collect5GNetworkInfo(client)
	if err != nil {
		return fmt.Errorf("failed to collect 5G info: %w", err)
	}

	// Display results
	fmt.Printf("\n📊 Enhanced 5G Network Summary:\n")
	fmt.Printf("  🌐 Mode: %s\n", info.Mode)
	fmt.Printf("  📋 5G Registration: %s\n", info.RegistrationStatus)
	fmt.Printf("  🔗 Carrier Aggregation: %t\n", info.CarrierAggregation)
	fmt.Printf("  📡 5G NR Cells: %d detected\n", len(info.NRCells))

	if info.LTEAnchor != nil {
		fmt.Printf("  ⚓ LTE Anchor: Cell %s (PCID %d, Band %s)\n",
			info.LTEAnchor.ServingCell.CellID,
			info.LTEAnchor.ServingCell.PCID,
			info.LTEAnchor.NetworkInfo.Band)
	}

	// Display 5G NR cells if any
	if len(info.NRCells) > 0 {
		fmt.Println("  📋 5G NR Cells:")
		for i, cell := range info.NRCells {
			fmt.Printf("    %d. NCI: %d, Band: %s, GSCN: %d, RSRP: %d dBm\n",
				i+1, cell.NCI, cell.Band, cell.GSCN, cell.RSRP)
		}
	} else {
		fmt.Println("  📭 No 5G NR cells detected (LTE anchor mode)")
	}

	return nil
}
