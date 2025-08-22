package main

import (
	"fmt"
	"strconv"
	"strings"
	"time"

	"golang.org/x/crypto/ssh"
)

// CellularLocationIntelligence represents cellular-based location data
type CellularLocationIntelligence struct {
	// Primary serving cell
	ServingCell ServingCellInfo `json:"serving_cell"`

	// Neighbor cells for fingerprinting
	NeighborCells []NeighborCellInfo `json:"neighbor_cells"`

	// Location fingerprint
	LocationFingerprint LocationFingerprint `json:"location_fingerprint"`

	// Signal quality metrics
	SignalQuality SignalQuality `json:"signal_quality"`

	// Network information
	NetworkInfo NetworkInfo `json:"network_info"`

	// Metadata
	Timestamp   int64     `json:"timestamp"`
	CollectedAt time.Time `json:"collected_at"`
	Valid       bool      `json:"valid"`
}

type ServingCellInfo struct {
	CellID   string `json:"cell_id"`
	TAC      string `json:"tac"`      // Tracking Area Code
	PCID     int    `json:"pcid"`     // Physical Cell ID
	EARFCN   int    `json:"earfcn"`   // Frequency
	Band     string `json:"band"`     // LTE Band
	MCC      string `json:"mcc"`      // Mobile Country Code
	MNC      string `json:"mnc"`      // Mobile Network Code
	Operator string `json:"operator"` // Network operator
}

type NeighborCellInfo struct {
	PCID     int    `json:"pcid"`
	EARFCN   int    `json:"earfcn"`
	RSSI     int    `json:"rssi"`
	RSRP     int    `json:"rsrp"`
	RSRQ     int    `json:"rsrq"`
	CellType string `json:"cell_type"` // "intra" or "inter"
}

type LocationFingerprint struct {
	PrimaryCellID string  `json:"primary_cell_id"`
	NeighborPCIDs []int   `json:"neighbor_pcids"`
	SignalPattern string  `json:"signal_pattern"`
	LocationName  string  `json:"location_name"` // "home", "away", "unknown"
	Confidence    float64 `json:"confidence"`    // 0.0-1.0
}

type SignalQuality struct {
	RSSI int `json:"rssi"` // Received Signal Strength Indicator
	RSRP int `json:"rsrp"` // Reference Signal Received Power
	RSRQ int `json:"rsrq"` // Reference Signal Received Quality
	SINR int `json:"sinr"` // Signal-to-Interference-plus-Noise Ratio
}

type NetworkInfo struct {
	Operator   string `json:"operator"`
	Technology string `json:"technology"` // "5G-NSA", "LTE", etc.
	Band       string `json:"band"`
	Bandwidth  string `json:"bandwidth"`
	Registered bool   `json:"registered"`
}

// collectCellularLocationIntelligence gathers comprehensive cellular data
func collectCellularLocationIntelligence(client *ssh.Client) (*CellularLocationIntelligence, error) {
	fmt.Println("🗼 Collecting Cellular Location Intelligence")
	fmt.Println("=" + strings.Repeat("=", 42))

	intel := &CellularLocationIntelligence{
		CollectedAt: time.Now(),
		Timestamp:   time.Now().Unix(),
	}

	// Get serving cell information
	fmt.Println("📡 Getting serving cell information...")
	if err := getServingCellInfo(client, intel); err != nil {
		return nil, fmt.Errorf("failed to get serving cell info: %v", err)
	}

	// Get neighbor cells
	fmt.Println("🗼 Getting neighbor cell information...")
	if err := getNeighborCellInfo(client, intel); err != nil {
		fmt.Printf("Warning: failed to get neighbor cell info: %v\n", err)
		// Continue even if neighbor cells fail
	}

	// Get signal quality
	fmt.Println("📊 Getting signal quality metrics...")
	if err := getSignalQuality(client, intel); err != nil {
		fmt.Printf("Warning: failed to get signal quality: %v\n", err)
	}

	// Get network information
	fmt.Println("🌐 Getting network information...")
	if err := getNetworkInfo(client, intel); err != nil {
		fmt.Printf("Warning: failed to get network info: %v\n", err)
	}

	// Generate location fingerprint
	generateLocationFingerprint(intel)

	// Validate data
	intel.Valid = intel.ServingCell.CellID != ""

	fmt.Println("\n📊 Cellular Location Intelligence Summary:")
	displayCellularIntelligence(intel)

	return intel, nil
}

func getServingCellInfo(client *ssh.Client, intel *CellularLocationIntelligence) error {
	// Get detailed serving cell info
	output, err := executeCommand(client, "gsmctl -A 'AT+QENG=\"servingcell\"'")
	if err != nil {
		return err
	}

	// Parse: +QENG: "servingcell","NOCONN","LTE","FDD",240,01,18BCF1F,443,1300,3,5,5,17,-84,-8,-53,17,0,-,43
	if strings.Contains(output, "+QENG:") {
		parts := strings.Split(output, ",")
		if len(parts) >= 20 {
			intel.ServingCell.MCC = strings.Trim(parts[4], " \"")
			intel.ServingCell.MNC = strings.Trim(parts[5], " \"")

			// Convert hex cell ID to decimal
			if cellIDHex := strings.Trim(parts[6], " \""); cellIDHex != "" {
				if cellID, err := strconv.ParseInt(cellIDHex, 16, 64); err == nil {
					intel.ServingCell.CellID = fmt.Sprintf("%d", cellID)
				}
			}

			if pcid, err := strconv.Atoi(strings.TrimSpace(parts[7])); err == nil {
				intel.ServingCell.PCID = pcid
			}

			if earfcn, err := strconv.Atoi(strings.TrimSpace(parts[8])); err == nil {
				intel.ServingCell.EARFCN = earfcn
			}
		}
	}

	// Get operator name
	if opOutput, err := executeCommand(client, "gsmctl -o"); err == nil {
		intel.ServingCell.Operator = strings.TrimSpace(opOutput)
	}

	// Get network info for band
	if netOutput, err := executeCommand(client, "gsmctl -F"); err == nil {
		parts := strings.Split(netOutput, " | ")
		if len(parts) >= 2 {
			intel.ServingCell.Band = strings.TrimSpace(parts[1])
		}
	}

	// Enhanced: Try to get real TAC from network registration
	err = getEnhancedTAC(client, intel)
	if err != nil {
		fmt.Printf("    ⚠️  Enhanced TAC collection failed: %v\n", err)
		// Derive TAC from Cell ID as fallback
		if intel.ServingCell.CellID != "" {
			if cellID, err := strconv.ParseInt(intel.ServingCell.CellID, 10, 64); err == nil {
				tac := cellID >> 8 // Extract TAC from upper bits
				intel.ServingCell.TAC = fmt.Sprintf("%d", tac)
				fmt.Printf("    📡 Derived TAC: %s from CellID: %s\n", intel.ServingCell.TAC, intel.ServingCell.CellID)
			}
		}
	}

	return nil
}

// getEnhancedTAC attempts to get real TAC from network registration
func getEnhancedTAC(client *ssh.Client, intel *CellularLocationIntelligence) error {
	// Method 1: Try AT+CREG=2 for network registration info with TAC
	_, err := executeCommand(client, "gsmctl -A 'AT+CREG=2'")
	if err == nil {
		if output, err := executeCommand(client, "gsmctl -A 'AT+CREG?'"); err == nil {
			if strings.Contains(output, "+CREG:") {
				// Format: +CREG: 2,1,"0017","18BCF22",7
				parts := strings.Split(output, ",")
				if len(parts) >= 4 {
					// TAC is usually the 3rd parameter (index 2)
					tacHex := strings.Trim(parts[2], "\" ")
					if tac, err := strconv.ParseInt(tacHex, 16, 64); err == nil {
						intel.ServingCell.TAC = strconv.FormatInt(tac, 10)
						fmt.Printf("    ✅ Real TAC from CREG: %s (hex: %s)\n", intel.ServingCell.TAC, tacHex)
						return nil
					}
				}
			}
		}
	}

	return fmt.Errorf("could not get real TAC")
}

func getNeighborCellInfo(client *ssh.Client, intel *CellularLocationIntelligence) error {
	output, err := executeCommand(client, "gsmctl -A 'AT+QENG=\"neighbourcell\"'")
	if err != nil {
		return err
	}

	lines := strings.Split(output, "\n")
	for _, line := range lines {
		if strings.Contains(line, "+QENG:") && strings.Contains(line, "neighbourcell") {
			// Parse neighbor cell data
			parts := strings.Split(line, ",")
			if len(parts) >= 12 {
				neighbor := NeighborCellInfo{}

				// Determine cell type
				if strings.Contains(line, "intra") {
					neighbor.CellType = "intra"
				} else {
					neighbor.CellType = "inter"
				}

				// Parse EARFCN (always available)
				if earfcn, err := strconv.Atoi(strings.TrimSpace(parts[2])); err == nil {
					neighbor.EARFCN = earfcn
				}

				// Parse PCID (may be "-" for inter-frequency cells)
				pcidStr := strings.TrimSpace(parts[3])
				if pcidStr != "-" {
					if pcid, err := strconv.Atoi(pcidStr); err == nil {
						neighbor.PCID = pcid
					}
				} else {
					// For inter-frequency cells without PCID, use EARFCN as identifier
					neighbor.PCID = neighbor.EARFCN
				}

				// Parse signal values (if available)
				if len(parts) >= 7 && parts[6] != "-" {
					if rssi, err := strconv.Atoi(strings.TrimSpace(parts[6])); err == nil {
						neighbor.RSSI = rssi
					}
				} else {
					// For inter-frequency cells, we don't have signal strength
					// Set a default weak signal value so they're still considered but with low priority
					neighbor.RSSI = -100 // Weak signal for inter-frequency cells
				}

				intel.NeighborCells = append(intel.NeighborCells, neighbor)
			}
		}
	}

	return nil
}

func getSignalQuality(client *ssh.Client, intel *CellularLocationIntelligence) error {
	output, err := executeCommand(client, "gsmctl -q")
	if err != nil {
		return err
	}

	lines := strings.Split(output, "\n")
	for _, line := range lines {
		line = strings.TrimSpace(line)
		if strings.HasPrefix(line, "RSSI:") {
			if val, err := strconv.Atoi(strings.TrimSpace(strings.TrimPrefix(line, "RSSI:"))); err == nil {
				intel.SignalQuality.RSSI = val
			}
		} else if strings.HasPrefix(line, "RSRP:") {
			if val, err := strconv.Atoi(strings.TrimSpace(strings.TrimPrefix(line, "RSRP:"))); err == nil {
				intel.SignalQuality.RSRP = val
			}
		} else if strings.HasPrefix(line, "RSRQ:") {
			if val, err := strconv.Atoi(strings.TrimSpace(strings.TrimPrefix(line, "RSRQ:"))); err == nil {
				intel.SignalQuality.RSRQ = val
			}
		} else if strings.HasPrefix(line, "SINR:") {
			if val, err := strconv.Atoi(strings.TrimSpace(strings.TrimPrefix(line, "SINR:"))); err == nil {
				intel.SignalQuality.SINR = val
			}
		}
	}

	return nil
}

func getNetworkInfo(client *ssh.Client, intel *CellularLocationIntelligence) error {
	// Get network technology
	if output, err := executeCommand(client, "gsmctl -F"); err == nil {
		parts := strings.Split(output, " | ")
		if len(parts) >= 1 {
			intel.NetworkInfo.Technology = strings.TrimSpace(parts[0])
		}
		if len(parts) >= 2 {
			intel.NetworkInfo.Band = strings.TrimSpace(parts[1])
		}
	}

	intel.NetworkInfo.Operator = intel.ServingCell.Operator
	intel.NetworkInfo.Registered = intel.ServingCell.CellID != ""

	return nil
}

func generateLocationFingerprint(intel *CellularLocationIntelligence) {
	fingerprint := &intel.LocationFingerprint

	fingerprint.PrimaryCellID = intel.ServingCell.CellID

	// Collect neighbor PCIDs
	for _, neighbor := range intel.NeighborCells {
		fingerprint.NeighborPCIDs = append(fingerprint.NeighborPCIDs, neighbor.PCID)
	}

	// Generate signal pattern
	fingerprint.SignalPattern = fmt.Sprintf("RSSI:%d,PCID:%d,NEIGHBORS:%d",
		intel.SignalQuality.RSSI, intel.ServingCell.PCID, len(intel.NeighborCells))

	// For now, assume this is "home" location (can be enhanced with database)
	fingerprint.LocationName = "home"
	fingerprint.Confidence = 0.8 // High confidence for known cell pattern
}

func displayCellularIntelligence(intel *CellularLocationIntelligence) {
	fmt.Printf("  📡 Serving Cell: %s (PCID: %d)\n", intel.ServingCell.CellID, intel.ServingCell.PCID)
	fmt.Printf("  🌐 Network: %s %s (%s)\n", intel.NetworkInfo.Operator, intel.NetworkInfo.Technology, intel.NetworkInfo.Band)
	fmt.Printf("  📊 Signal: RSSI %d, RSRP %d, RSRQ %d, SINR %d\n",
		intel.SignalQuality.RSSI, intel.SignalQuality.RSRP, intel.SignalQuality.RSRQ, intel.SignalQuality.SINR)
	fmt.Printf("  🗼 Neighbors: %d cells detected\n", len(intel.NeighborCells))
	fmt.Printf("  🏠 Location: %s (%.1f%% confidence)\n", intel.LocationFingerprint.LocationName, intel.LocationFingerprint.Confidence*100)

	// Display neighbor cells
	if len(intel.NeighborCells) > 0 {
		fmt.Println("  📋 Neighbor Cells:")
		for i, neighbor := range intel.NeighborCells {
			if i >= 5 { // Limit display to first 5
				fmt.Printf("    ... and %d more\n", len(intel.NeighborCells)-5)
				break
			}
			fmt.Printf("    - PCID %d (%s, RSSI %d)\n", neighbor.PCID, neighbor.CellType, neighbor.RSSI)
		}
	}
}

// compareCellularFingerprints compares current cellular data with known locations
func compareCellularFingerprints(current *CellularLocationIntelligence, known map[string]*CellularLocationIntelligence) string {
	bestMatch := "unknown"
	bestScore := 0.0

	for locationName, knownData := range known {
		score := calculateFingerprintSimilarity(current, knownData)
		if score > bestScore {
			bestScore = score
			bestMatch = locationName
		}
	}

	if bestScore > 0.7 { // 70% similarity threshold
		return bestMatch
	}

	return "unknown"
}

func calculateFingerprintSimilarity(current, known *CellularLocationIntelligence) float64 {
	score := 0.0

	// Primary cell match (50% weight)
	if current.ServingCell.CellID == known.ServingCell.CellID {
		score += 0.5
	}

	// PCID match (30% weight)
	if current.ServingCell.PCID == known.ServingCell.PCID {
		score += 0.3
	}

	// Neighbor similarity (20% weight)
	neighborScore := calculateNeighborSimilarity(current.NeighborCells, known.NeighborCells)
	score += 0.2 * neighborScore

	return score
}

func calculateNeighborSimilarity(current, known []NeighborCellInfo) float64 {
	if len(current) == 0 && len(known) == 0 {
		return 1.0
	}

	if len(current) == 0 || len(known) == 0 {
		return 0.0
	}

	matches := 0
	for _, currentCell := range current {
		for _, knownCell := range known {
			if currentCell.PCID == knownCell.PCID {
				matches++
				break
			}
		}
	}

	// Calculate Jaccard similarity
	union := len(current) + len(known) - matches
	if union == 0 {
		return 1.0
	}

	return float64(matches) / float64(union)
}
