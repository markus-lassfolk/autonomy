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
	PCI      int    `json:"pci"`       // Physical Cell ID
	EARFCN   int    `json:"earfcn"`    // Frequency
}

// Enhanced5GNetworkInfo represents comprehensive 5G network information
type Enhanced5GNetworkInfo struct {
	Mode               string                        `json:"mode"`                // "5G-SA", "5G-NSA", "LTE"
	LTEAnchor          *CellularLocationIntelligence `json:"lte_anchor"`          // LTE anchor cell (for NSA)
	NRCells            []Enhanced5GCellInfo          `json:"nr_cells"`            // 5G NR cells
	CarrierAggregation bool                          `json:"carrier_aggregation"` // CA active
	RegistrationStatus string                        `json:"registration_status"` // 5G registration status
	NetworkOperator    string                        `json:"network_operator"`    // Network operator
	Technology         string                        `json:"technology"`          // Current technology
	CollectedAt        time.Time                     `json:"collected_at"`
	Valid              bool                          `json:"valid"`
	Confidence         float64                       `json:"confidence"` // 0.0-1.0
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
	EnableAdvancedParsing    bool          `json:"enable_advanced_parsing"`
	RetryAttempts            int           `json:"retry_attempts"`
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
			EnableAdvancedParsing:    true,
			RetryAttempts:            3,
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

	// Get network mode with enhanced parsing
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

	// Enhanced carrier aggregation detection
	if e5g.config.EnableCarrierAggregation {
		info.CarrierAggregation = e5g.detectCarrierAggregation(ctx)
		e5g.logger.LogDebugVerbose("5g_carrier_aggregation", map[string]interface{}{
			"enabled": info.CarrierAggregation,
		})
	}

	// Get network operator information
	if operator, err := e5g.executeATCommand(ctx, "AT+COPS?"); err == nil {
		info.NetworkOperator = e5g.parseNetworkOperator(operator)
		e5g.logger.LogDebugVerbose("5g_network_operator", map[string]interface{}{
			"operator": info.NetworkOperator,
		})
	}

	// Enhanced 5G NR data collection with multiple AT commands
	if e5g.config.EnableAdvancedParsing {
		e5g.collectAdvanced5GNRData(ctx, info)
	} else {
		e5g.collectBasic5GNRData(ctx, info)
	}

	// Calculate confidence score
	info.Confidence = e5g.calculateConfidence(info)
	info.Valid = info.Confidence > 0.3

	e5g.logger.LogDebugVerbose("5g_collection_complete", map[string]interface{}{
		"nr_cells":   len(info.NRCells),
		"confidence": info.Confidence,
		"valid":      info.Valid,
	})

	return info, nil
}

// collectAdvanced5GNRData collects 5G NR data using multiple AT commands
func (e5g *Enhanced5GCollector) collectAdvanced5GNRData(ctx context.Context, info *Enhanced5GNetworkInfo) {
	nrCommands := []string{
		"AT+QENG=\"NR5G\"",
		"AT+QNWINFO",
		"AT+QCSQ",
		"AT+QRSRP",
		"AT+QSINR",
		"AT+QENG=\"SERVINGCELL\"",
	}

	e5g.logger.LogDebugVerbose("5g_advanced_collection_start", map[string]interface{}{
		"commands": len(nrCommands),
	})

	for _, cmd := range nrCommands {
		if output, err := e5g.executeATCommand(ctx, cmd); err == nil {
			output = strings.TrimSpace(output)
			if output != "" && !strings.Contains(output, "ERROR") {
				e5g.logger.LogDebugVerbose("5g_command_success", map[string]interface{}{
					"command": cmd,
					"output":  output,
				})

				// Parse 5G NR data if available
				if nrCells := e5g.parse5GNRData(output, cmd); len(nrCells) > 0 {
					info.NRCells = append(info.NRCells, nrCells...)
				}
			} else {
				e5g.logger.LogDebugVerbose("5g_command_no_data", map[string]interface{}{
					"command": cmd,
				})
			}
		}
	}
}

// collectBasic5GNRData collects basic 5G NR data
func (e5g *Enhanced5GCollector) collectBasic5GNRData(ctx context.Context, info *Enhanced5GNetworkInfo) {
	// Basic collection using QNWINFO
	if output, err := e5g.executeATCommand(ctx, "AT+QNWINFO"); err == nil {
		if nrCells := e5g.parse5GNRData(output, "AT+QNWINFO"); len(nrCells) > 0 {
			info.NRCells = append(info.NRCells, nrCells...)
		}
	}
}

// parse5GNRData parses 5G NR cell data from AT command responses
func (e5g *Enhanced5GCollector) parse5GNRData(output, command string) []Enhanced5GCellInfo {
	var cells []Enhanced5GCellInfo

	lines := strings.Split(output, "\n")
	for _, line := range lines {
		line = strings.TrimSpace(line)

		// Parse different 5G NR response formats
		if strings.Contains(command, "QNWINFO") && strings.Contains(line, "NR5G") {
			if cell := e5g.parseQNWINFO(line); cell != nil {
				cells = append(cells, *cell)
			}
		} else if strings.Contains(command, "QCSQ") && strings.Contains(line, "NR5G") {
			if cell := e5g.parseQCSQ(line); cell != nil {
				cells = append(cells, *cell)
			}
		} else if strings.Contains(command, "QENG") && strings.Contains(line, "NR5G") {
			if cell := e5g.parseQENG(line); cell != nil {
				cells = append(cells, *cell)
			}
		}
	}

	return cells
}

// parseQNWINFO parses +QNWINFO response for 5G NR information
func (e5g *Enhanced5GCollector) parseQNWINFO(line string) *Enhanced5GCellInfo {
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

	// Extract NCI if available
	if len(parts) >= 2 {
		if nci, err := strconv.ParseInt(strings.TrimSpace(parts[1]), 16, 64); err == nil {
			if nci >= 0 && nci <= int64(^uint(0)>>1) {
				cell.NCI = int(nci)
			} else {
				e5g.logger.Warn("NCI value out of range for int conversion", "nci", nci)
			}
		}
	}

	return cell
}

// parseQCSQ parses +QCSQ response for 5G NR signal quality
func (e5g *Enhanced5GCollector) parseQCSQ(line string) *Enhanced5GCellInfo {
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

// parseQENG parses +QENG response for 5G NR information
func (e5g *Enhanced5GCollector) parseQENG(line string) *Enhanced5GCellInfo {
	// +QENG: "NR5G","LTE",1,24001,0x12345678,78,3600,-85,-12,30
	if !strings.Contains(line, "+QENG:") || !strings.Contains(line, "NR5G") {
		return nil
	}

	parts := strings.Split(line, ",")
	if len(parts) < 10 {
		return nil
	}

	cell := &Enhanced5GCellInfo{
		CellType: "serving",
	}

	// Parse NCI (hex format)
	if len(parts) >= 5 {
		if nci, err := strconv.ParseInt(strings.TrimPrefix(strings.TrimSpace(parts[4]), "0x"), 16, 64); err == nil {
			if nci >= 0 && nci <= int64(^uint(0)>>1) {
				cell.NCI = int(nci)
			} else {
				e5g.logger.Warn("NCI value out of range for int conversion", "nci", nci)
			}
		}
	}

	// Parse band
	if len(parts) >= 6 {
		if band, err := strconv.Atoi(strings.TrimSpace(parts[5])); err == nil {
			cell.Band = fmt.Sprintf("N%d", band)
		}
	}

	// Parse frequency
	if len(parts) >= 7 {
		if freq, err := strconv.Atoi(strings.TrimSpace(parts[6])); err == nil {
			cell.GSCN = freq
		}
	}

	// Parse signal values
	if len(parts) >= 8 {
		if rsrp, err := strconv.Atoi(strings.TrimSpace(parts[7])); err == nil {
			cell.RSRP = rsrp
		}
	}
	if len(parts) >= 9 {
		if rsrq, err := strconv.Atoi(strings.TrimSpace(parts[8])); err == nil {
			cell.RSRQ = rsrq
		}
	}
	if len(parts) >= 10 {
		if sinr, err := strconv.Atoi(strings.TrimSpace(parts[9])); err == nil {
			cell.SINR = sinr
		}
	}

	return cell
}

// detectCarrierAggregation detects if carrier aggregation is active
func (e5g *Enhanced5GCollector) detectCarrierAggregation(ctx context.Context) bool {
	// Try multiple commands to detect carrier aggregation
	caCommands := []string{
		"AT+QENG=\"SERVINGCELL\"",
		"AT+QNWINFO",
		"AT+QCSQ",
	}

	for _, cmd := range caCommands {
		if output, err := e5g.executeATCommand(ctx, cmd); err == nil {
			output = strings.ToUpper(output)
			// Look for carrier aggregation indicators
			if strings.Contains(output, "CA 1") ||
				strings.Contains(output, "SECONDARY:") ||
				strings.Contains(output, "CARRIER AGGREGATION") ||
				strings.Contains(output, "MULTIPLE CELLS") {
				return true
			}
		}
	}

	return false
}

// parseNetworkOperator parses network operator information
func (e5g *Enhanced5GCollector) parseNetworkOperator(output string) string {
	// +COPS: 0,0,"Telia",7
	if strings.Contains(output, "+COPS:") {
		parts := strings.Split(output, ",")
		if len(parts) >= 3 {
			operator := strings.Trim(parts[2], "\"")
			return operator
		}
	}
	return ""
}

// calculateConfidence calculates confidence score for 5G data
func (e5g *Enhanced5GCollector) calculateConfidence(info *Enhanced5GNetworkInfo) float64 {
	confidence := 0.0

	// Base confidence for having any data
	if info.Mode != "" {
		confidence += 0.2
	}

	// Registration status confidence
	if info.RegistrationStatus != "" {
		confidence += 0.2
	}

	// NR cells confidence
	if len(info.NRCells) > 0 {
		confidence += 0.3
		// Additional confidence for multiple cells
		if len(info.NRCells) > 1 {
			confidence += 0.1
		}
	}

	// Signal quality confidence
	for _, cell := range info.NRCells {
		if cell.RSRP != 0 && cell.RSRP > e5g.config.SignalThreshold {
			confidence += 0.1
			break
		}
	}

	// Carrier aggregation confidence
	if info.CarrierAggregation {
		confidence += 0.1
	}

	return confidence
}

// executeATCommand executes an AT command with retry logic
func (e5g *Enhanced5GCollector) executeATCommand(ctx context.Context, command string) (string, error) {
	var lastErr error

	for attempt := 0; attempt < e5g.config.RetryAttempts; attempt++ {
		select {
		case <-ctx.Done():
			return "", ctx.Err()
		default:
		}

		cmd := exec.CommandContext(ctx, "gsmctl", "-A", command)
		output, err := cmd.Output()

		if err == nil {
			return string(output), nil
		}

		lastErr = err
		e5g.logger.LogDebugVerbose("5g_command_retry", map[string]interface{}{
			"command": command,
			"attempt": attempt + 1,
			"error":   err.Error(),
		})

		// Wait before retry
		time.Sleep(100 * time.Millisecond)
	}

	return "", fmt.Errorf("failed after %d attempts: %w", e5g.config.RetryAttempts, lastErr)
}

// parseNetworkMode parses network mode from AT command response
func (e5g *Enhanced5GCollector) parseNetworkMode(output string) string {
	// Parse various network mode formats
	output = strings.ToUpper(output)

	if strings.Contains(output, "5G-SA") {
		return "5G-SA"
	} else if strings.Contains(output, "5G-NSA") {
		return "5G-NSA"
	} else if strings.Contains(output, "LTE") {
		return "LTE"
	} else if strings.Contains(output, "NR5G") {
		return "5G-NSA" // Assume NSA if NR5G is mentioned
	}

	return "UNKNOWN"
}

// parse5GRegistrationStatus parses 5G registration status
func (e5G *Enhanced5GCollector) parse5GRegistrationStatus(output string) string {
	// Parse registration status from various formats
	if strings.Contains(output, "+C5GREG:") {
		parts := strings.Split(output, ",")
		if len(parts) >= 2 {
			status := strings.TrimSpace(parts[1])
			switch status {
			case "1":
				return "REGISTERED"
			case "2":
				return "SEARCHING"
			case "3":
				return "REGISTRATION_DENIED"
			case "4":
				return "UNKNOWN"
			case "5":
				return "REGISTERED_ROAMING"
			}
		}
	}

	return "UNKNOWN"
}

// Get5GNetworkSummary returns a summary of 5G network status
func (e5g *Enhanced5GCollector) Get5GNetworkSummary(ctx context.Context) (map[string]interface{}, error) {
	info, err := e5g.Collect5GNetworkInfo(ctx)
	if err != nil {
		return nil, err
	}

	summary := map[string]interface{}{
		"mode":                info.Mode,
		"registration_status": info.RegistrationStatus,
		"carrier_aggregation": info.CarrierAggregation,
		"network_operator":    info.NetworkOperator,
		"nr_cells_count":      len(info.NRCells),
		"confidence":          info.Confidence,
		"valid":               info.Valid,
		"collected_at":        info.CollectedAt,
	}

	// Add signal quality summary if cells are available
	if len(info.NRCells) > 0 {
		var totalRSRP, totalRSRQ, totalSINR int
		validSignals := 0

		for _, cell := range info.NRCells {
			if cell.RSRP != 0 {
				totalRSRP += cell.RSRP
				validSignals++
			}
			if cell.RSRQ != 0 {
				totalRSRQ += cell.RSRQ
			}
			if cell.SINR != 0 {
				totalSINR += cell.SINR
			}
		}

		if validSignals > 0 {
			summary["average_rsrp"] = totalRSRP / validSignals
			summary["average_rsrq"] = totalRSRQ / validSignals
			summary["average_sinr"] = totalSINR / validSignals
		}
	}

	return summary, nil
}
