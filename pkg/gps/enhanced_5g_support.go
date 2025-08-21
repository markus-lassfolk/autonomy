package gps

import (
	"context"
	"fmt"
	"os/exec"
	"strconv"
	"strings"
	"time"

	"github.com/markus-lassfolk/autonomy/pkg/logx"
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
	CollectedAt        time.Time                     `json:"collected_at"`
	Valid              bool                          `json:"valid"`
}

// Enhanced5GCollector collects 5G NR network information
type Enhanced5GCollector struct {
	logger *logx.Logger
	config *Enhanced5GConfig
}

// Enhanced5GConfig holds configuration for 5G collection
type Enhanced5GConfig struct {
	Enable5GCollection       bool          `json:"enable_5g_collection"`
	CollectionTimeout        time.Duration `json:"collection_timeout"`
	MaxNeighborNRCells       int           `json:"max_neighbor_nr_cells"`
	SignalThreshold          int           `json:"signal_threshold"`
	EnableCarrierAggregation bool          `json:"enable_carrier_aggregation"`
}

// NewEnhanced5GCollector creates a new 5G collector
func NewEnhanced5GCollector(config *Enhanced5GConfig, logger *logx.Logger) *Enhanced5GCollector {
	if config == nil {
		config = &Enhanced5GConfig{
			Enable5GCollection:       true,
			CollectionTimeout:        10 * time.Second,
			MaxNeighborNRCells:       8,
			SignalThreshold:          -120, // dBm
			EnableCarrierAggregation: true,
		}
	}

	return &Enhanced5GCollector{
		logger: logger,
		config: config,
	}
}

// Collect5GNetworkInfo collects comprehensive 5G network information
func (e5g *Enhanced5GCollector) Collect5GNetworkInfo(ctx context.Context) (*Enhanced5GNetworkInfo, error) {
	if !e5g.config.Enable5GCollection {
		return nil, fmt.Errorf("5G collection disabled")
	}

	e5g.logger.LogDebugVerbose("5g_collection_start", map[string]interface{}{
		"max_nr_cells":     e5g.config.MaxNeighborNRCells,
		"signal_threshold": e5g.config.SignalThreshold,
	})

	info := &Enhanced5GNetworkInfo{
		NRCells:     make([]Enhanced5GCellInfo, 0),
		CollectedAt: time.Now(),
	}

	// Get network mode
	if mode, err := e5g.executeATCommand(ctx, "AT+QNWINFO"); err == nil {
		info.Mode = e5g.parseNetworkMode(mode)
		e5g.logger.LogDebugVerbose("5g_network_mode", map[string]interface{}{
			"mode": info.Mode,
		})
	}

	// Get 5G registration status
	if reg, err := e5g.executeATCommand(ctx, "AT+C5GREG?"); err == nil {
		info.RegistrationStatus = e5g.parse5GRegistrationStatus(reg)
		e5g.logger.LogDebugVerbose("5g_registration_status", map[string]interface{}{
			"status": info.RegistrationStatus,
		})
	}

	// Collect 5G NR cells
	nrCells, err := e5g.collect5GNRCells(ctx)
	if err != nil {
		e5g.logger.LogDebugVerbose("5g_nr_cells_collection_failed", map[string]interface{}{
			"error": err.Error(),
		})
	} else {
		info.NRCells = nrCells
	}

	// Check for carrier aggregation
	if e5g.config.EnableCarrierAggregation {
		info.CarrierAggregation = e5g.detectCarrierAggregation(ctx)
	}

	// Collect LTE anchor information for NSA mode
	if strings.Contains(info.Mode, "NSA") {
		if lteAnchor, err := e5g.collectLTEAnchorInfo(ctx); err == nil {
			info.LTEAnchor = lteAnchor
		}
	}

	info.Valid = len(info.NRCells) > 0 || info.Mode != ""

	e5g.logger.Info("5g_network_info_collected",
		"mode", info.Mode,
		"nr_cells_count", len(info.NRCells),
		"carrier_aggregation", info.CarrierAggregation,
		"registration_status", info.RegistrationStatus,
	)

	return info, nil
}

// collect5GNRCells collects 5G NR cell information
func (e5g *Enhanced5GCollector) collect5GNRCells(ctx context.Context) ([]Enhanced5GCellInfo, error) {
	var nrCells []Enhanced5GCellInfo

	// Get serving NR cell
	if servingNR, err := e5g.executeATCommand(ctx, "AT+QENG=\"servingcell\""); err == nil {
		if cell := e5g.parseServingNRCell(servingNR); cell != nil {
			cell.CellType = "serving"
			nrCells = append(nrCells, *cell)
		}
	}

	// Get neighbor NR cells
	if neighborNR, err := e5g.executeATCommand(ctx, "AT+QENG=\"neighbourcell\""); err == nil {
		neighbors := e5g.parseNeighborNRCells(neighborNR)
		for i, neighbor := range neighbors {
			if i >= e5g.config.MaxNeighborNRCells {
				break
			}
			if neighbor.RSRP >= e5g.config.SignalThreshold {
				neighbor.CellType = "neighbor"
				nrCells = append(nrCells, neighbor)
			}
		}
	}

	e5g.logger.LogDebugVerbose("5g_nr_cells_collected", map[string]interface{}{
		"total_cells":    len(nrCells),
		"serving_cells":  countCellsByType(nrCells, "serving"),
		"neighbor_cells": countCellsByType(nrCells, "neighbor"),
	})

	return nrCells, nil
}

// executeATCommand executes an AT command
func (e5g *Enhanced5GCollector) executeATCommand(ctx context.Context, command string) (string, error) {
	// Create context with timeout
	cmdCtx, cancel := context.WithTimeout(ctx, e5g.config.CollectionTimeout)
	defer cancel()

	// Execute gsmctl command
	cmd := exec.CommandContext(cmdCtx, "gsmctl", "-A", command)
	output, err := cmd.Output()
	if err != nil {
		return "", fmt.Errorf("AT command failed: %w", err)
	}

	return strings.TrimSpace(string(output)), nil
}

// parseNetworkMode parses network mode from AT+QNWINFO response
func (e5g *Enhanced5GCollector) parseNetworkMode(response string) string {
	// Example: +QNWINFO: "5G","24001","NR5G-SA",2140
	lines := strings.Split(response, "\n")
	for _, line := range lines {
		if strings.Contains(line, "+QNWINFO:") {
			parts := strings.Split(line, ",")
			if len(parts) >= 3 {
				mode := strings.Trim(parts[2], "\"")
				return mode
			}
		}
	}
	return "unknown"
}

// parse5GRegistrationStatus parses 5G registration status
func (e5g *Enhanced5GCollector) parse5GRegistrationStatus(response string) string {
	// Example: +C5GREG: 0,1
	lines := strings.Split(response, "\n")
	for _, line := range lines {
		if strings.Contains(line, "+C5GREG:") {
			parts := strings.Split(strings.TrimSpace(line), ",")
			if len(parts) >= 2 {
				status := strings.TrimSpace(parts[1])
				switch status {
				case "0":
					return "not_registered"
				case "1":
					return "registered_home"
				case "2":
					return "searching"
				case "3":
					return "registration_denied"
				case "5":
					return "registered_roaming"
				default:
					return "unknown"
				}
			}
		}
	}
	return "unknown"
}

// parseServingNRCell parses serving NR cell information
func (e5g *Enhanced5GCollector) parseServingNRCell(response string) *Enhanced5GCellInfo {
	// Example: +QENG: "servingcell","NOCONN","NR5G-SA","FDD",240,01,7A52D01,466,2140,78,10,10,10,-44,-5,14
	lines := strings.Split(response, "\n")
	for _, line := range lines {
		if strings.Contains(line, "NR5G") {
			parts := strings.Split(line, ",")
			if len(parts) >= 16 {
				cell := &Enhanced5GCellInfo{}

				// Parse NCI (New Radio Cell Identity)
				if nci, err := strconv.ParseInt(strings.TrimSpace(parts[6]), 16, 64); err == nil {
					cell.NCI = int(nci)
				}

				// Parse GSCN
				if gscn, err := strconv.Atoi(strings.TrimSpace(parts[8])); err == nil {
					cell.GSCN = gscn
				}

				// Parse Band
				if band, err := strconv.Atoi(strings.TrimSpace(parts[9])); err == nil {
					cell.Band = fmt.Sprintf("N%d", band)
				}

				// Parse RSRP
				if rsrp, err := strconv.Atoi(strings.TrimSpace(parts[13])); err == nil {
					cell.RSRP = rsrp
				}

				// Parse RSRQ
				if rsrq, err := strconv.Atoi(strings.TrimSpace(parts[14])); err == nil {
					cell.RSRQ = rsrq
				}

				// Parse SINR
				if sinr, err := strconv.Atoi(strings.TrimSpace(parts[15])); err == nil {
					cell.SINR = sinr
				}

				return cell
			}
		}
	}
	return nil
}

// parseNeighborNRCells parses neighbor NR cells
func (e5g *Enhanced5GCollector) parseNeighborNRCells(response string) []Enhanced5GCellInfo {
	var neighbors []Enhanced5GCellInfo

	lines := strings.Split(response, "\n")
	for _, line := range lines {
		if strings.Contains(line, "NR5G") && strings.Contains(line, "neighbourcell") {
			parts := strings.Split(line, ",")
			if len(parts) >= 8 {
				cell := Enhanced5GCellInfo{}

				// Parse NCI
				if nci, err := strconv.ParseInt(strings.TrimSpace(parts[3]), 16, 64); err == nil {
					cell.NCI = int(nci)
				}

				// Parse RSRP
				if rsrp, err := strconv.Atoi(strings.TrimSpace(parts[5])); err == nil {
					cell.RSRP = rsrp
				}

				// Parse RSRQ
				if rsrq, err := strconv.Atoi(strings.TrimSpace(parts[6])); err == nil {
					cell.RSRQ = rsrq
				}

				// Parse SINR
				if sinr, err := strconv.Atoi(strings.TrimSpace(parts[7])); err == nil {
					cell.SINR = sinr
				}

				neighbors = append(neighbors, cell)
			}
		}
	}

	return neighbors
}

// detectCarrierAggregation detects if carrier aggregation is active
func (e5g *Enhanced5GCollector) detectCarrierAggregation(ctx context.Context) bool {
	// Check for carrier aggregation status
	if caStatus, err := e5g.executeATCommand(ctx, "AT+QCAINFO"); err == nil {
		return strings.Contains(caStatus, "PCC") && strings.Contains(caStatus, "SCC")
	}
	return false
}

// collectLTEAnchorInfo collects LTE anchor cell information for NSA mode
func (e5g *Enhanced5GCollector) collectLTEAnchorInfo(ctx context.Context) (*CellularLocationIntelligence, error) {
	// This would collect LTE anchor cell information
	// For now, return nil as this would require the cellular intelligence collector
	return nil, fmt.Errorf("LTE anchor collection not implemented")
}

// Helper functions

func countCellsByType(cells []Enhanced5GCellInfo, cellType string) int {
	count := 0
	for _, cell := range cells {
		if cell.CellType == cellType {
			count++
		}
	}
	return count
}

// Get5GCapabilities checks if the device supports 5G
func (e5g *Enhanced5GCollector) Get5GCapabilities(ctx context.Context) (map[string]interface{}, error) {
	capabilities := make(map[string]interface{})

	// Check modem capabilities
	if caps, err := e5g.executeATCommand(ctx, "AT+QGMR"); err == nil {
		capabilities["modem_info"] = caps
		capabilities["5g_capable"] = strings.Contains(caps, "5G") || strings.Contains(caps, "NR")
	}

	// Check supported bands
	if bands, err := e5g.executeATCommand(ctx, "AT+QNWPREFCFG=\"nr5g_band\""); err == nil {
		capabilities["supported_nr_bands"] = e5g.parseNRBands(bands)
	}

	// Check current network preference
	if pref, err := e5g.executeATCommand(ctx, "AT+QNWPREFCFG=\"mode_pref\""); err == nil {
		capabilities["mode_preference"] = pref
	}

	return capabilities, nil
}

// parseNRBands parses supported NR bands
func (e5g *Enhanced5GCollector) parseNRBands(response string) []string {
	var bands []string

	// Example: +QNWPREFCFG: "nr5g_band",1:3:5:7:8:20:28:38:40:41:77:78:79
	lines := strings.Split(response, "\n")
	for _, line := range lines {
		if strings.Contains(line, "nr5g_band") {
			parts := strings.Split(line, ",")
			if len(parts) >= 2 {
				bandStr := strings.Trim(parts[1], "\"")
				bandNumbers := strings.Split(bandStr, ":")
				for _, bandNum := range bandNumbers {
					if bandNum != "" {
						bands = append(bands, fmt.Sprintf("N%s", bandNum))
					}
				}
			}
		}
	}

	return bands
}

// IsAvailable checks if 5G collection is available
func (e5g *Enhanced5GCollector) IsAvailable(ctx context.Context) bool {
	if !e5g.config.Enable5GCollection {
		return false
	}

	// Check if gsmctl is available
	if _, err := exec.LookPath("gsmctl"); err != nil {
		return false
	}

	// Try a simple AT command to verify modem connectivity
	if _, err := e5g.executeATCommand(ctx, "AT"); err != nil {
		return false
	}

	return true
}
