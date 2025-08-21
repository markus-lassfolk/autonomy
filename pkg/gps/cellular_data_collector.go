package gps

import (
	"context"
	"encoding/json"
	"fmt"
	"os/exec"

	"github.com/markus-lassfolk/autonomy/pkg/logx"
)

// CellularDataCollectorImpl implements CellularDataCollector interface
type CellularDataCollectorImpl struct {
	logger *logx.Logger
}

// NewCellularDataCollectorFromConfig creates a new cellular data collector
func NewCellularDataCollectorFromConfig(logger *logx.Logger) CellularDataCollector {
	return &CellularDataCollectorImpl{
		logger: logger,
	}
}

// GetServingCell returns information about the serving cell
func (cdc *CellularDataCollectorImpl) GetServingCell(ctx context.Context) (*ServingCellInfo, error) {
	// Try multiple methods to get serving cell information

	// Method 1: Try ubus mobiled (RutOS)
	if cell, err := cdc.getServingCellViaUbusMobiled(ctx); err == nil {
		return cell, nil
	}

	// Method 2: Try ubus gsm (Alternative RutOS)
	if cell, err := cdc.getServingCellViaUbusGSM(ctx); err == nil {
		return cell, nil
	}

	// Method 3: Try AT commands (if modem supports it)
	if cell, err := cdc.getServingCellViaAT(ctx); err == nil {
		return cell, nil
	}

	// Method 4: Try parsing from /proc or /sys
	if cell, err := cdc.getServingCellViaSysfs(ctx); err == nil {
		return cell, nil
	}

	return nil, fmt.Errorf("no serving cell information available")
}

// GetNeighborCells returns information about neighbor cells
func (cdc *CellularDataCollectorImpl) GetNeighborCells(ctx context.Context) ([]NeighborCellInfo, error) {
	// Try to get neighbor cell information

	// Method 1: Try ubus mobiled
	if neighbors, err := cdc.getNeighborCellsViaUbusMobiled(ctx); err == nil {
		return neighbors, nil
	}

	// Method 2: Try AT commands
	if neighbors, err := cdc.getNeighborCellsViaAT(ctx); err == nil {
		return neighbors, nil
	}

	// Return empty slice if no neighbors available (not an error)
	cdc.logger.LogDebugVerbose("no_neighbor_cells_available", map[string]interface{}{
		"reason": "no_supported_method_available",
	})

	return []NeighborCellInfo{}, nil
}

// GetCellularMetrics returns detailed cellular metrics
func (cdc *CellularDataCollectorImpl) GetCellularMetrics(ctx context.Context) (*CellularMetrics, error) {
	metrics := &CellularMetrics{}

	// Try to get metrics via ubus mobiled
	if err := cdc.getCellularMetricsViaUbusMobiled(ctx, metrics); err != nil {
		cdc.logger.LogDebugVerbose("cellular_metrics_ubus_failed", map[string]interface{}{
			"error": err.Error(),
		})
	}

	// Try to get additional metrics via AT commands
	if err := cdc.getCellularMetricsViaAT(ctx, metrics); err != nil {
		cdc.logger.LogDebugVerbose("cellular_metrics_at_failed", map[string]interface{}{
			"error": err.Error(),
		})
	}

	return metrics, nil
}

// getServingCellViaUbusMobiled gets serving cell info via ubus mobiled
func (cdc *CellularDataCollectorImpl) getServingCellViaUbusMobiled(ctx context.Context) (*ServingCellInfo, error) {
	cmd := exec.CommandContext(ctx, "ubus", "call", "mobiled", "status")
	output, err := cmd.Output()
	if err != nil {
		return nil, fmt.Errorf("ubus mobiled call failed: %w", err)
	}

	var response map[string]interface{}
	if err := json.Unmarshal(output, &response); err != nil {
		return nil, fmt.Errorf("failed to parse mobiled response: %w", err)
	}

	// Extract serving cell information from mobiled response
	cell := &ServingCellInfo{}

	if device, ok := response["device"].(map[string]interface{}); ok {
		if network, ok := device["network"].(map[string]interface{}); ok {
			if mcc, ok := network["mcc"].(string); ok {
				cell.MCC = mcc
			}
			if mnc, ok := network["mnc"].(string); ok {
				cell.MNC = mnc
			}
			if lac, ok := network["lac"].(string); ok {
				cell.TAC = lac
			}
			if cellid, ok := network["cellid"].(string); ok {
				cell.CellID = cellid
			}
			if tech, ok := network["technology"].(string); ok {
				cell.Technology = tech
			}
		}

		if signal, ok := device["signal"].(map[string]interface{}); ok {
			if rsrp, ok := signal["rsrp"].(float64); ok {
				cell.RSRP = int(rsrp)
			}
			if rsrq, ok := signal["rsrq"].(float64); ok {
				cell.RSRQ = int(rsrq)
			}
			if sinr, ok := signal["sinr"].(float64); ok {
				cell.SINR = int(sinr)
			}
		}
	}

	// Validate that we have minimum required information
	if cell.MCC == "" || cell.MNC == "" || cell.CellID == "" {
		return nil, fmt.Errorf("incomplete serving cell information")
	}

	return cell, nil
}

// getServingCellViaUbusGSM gets serving cell info via ubus gsm
func (cdc *CellularDataCollectorImpl) getServingCellViaUbusGSM(ctx context.Context) (*ServingCellInfo, error) {
	cmd := exec.CommandContext(ctx, "ubus", "call", "gsm", "info")
	output, err := cmd.Output()
	if err != nil {
		return nil, fmt.Errorf("ubus gsm call failed: %w", err)
	}

	var response map[string]interface{}
	if err := json.Unmarshal(output, &response); err != nil {
		return nil, fmt.Errorf("failed to parse gsm response: %w", err)
	}

	// Parse GSM response format (implementation depends on actual format)
	cell := &ServingCellInfo{}

	// This would need to be adapted based on actual GSM ubus response format
	if mcc, ok := response["mcc"].(string); ok {
		cell.MCC = mcc
	}
	if mnc, ok := response["mnc"].(string); ok {
		cell.MNC = mnc
	}
	if lac, ok := response["lac"].(string); ok {
		cell.TAC = lac
	}
	if cellid, ok := response["cellid"].(string); ok {
		cell.CellID = cellid
	}

	if cell.MCC == "" || cell.MNC == "" || cell.CellID == "" {
		return nil, fmt.Errorf("incomplete serving cell information from gsm")
	}

	return cell, nil
}

// getServingCellViaAT gets serving cell info via AT commands
func (cdc *CellularDataCollectorImpl) getServingCellViaAT(ctx context.Context) (*ServingCellInfo, error) {
	// This would require implementing AT command communication
	// For now, return not implemented
	return nil, fmt.Errorf("AT commands not implemented")
}

// getServingCellViaSysfs gets serving cell info via /sys filesystem
func (cdc *CellularDataCollectorImpl) getServingCellViaSysfs(ctx context.Context) (*ServingCellInfo, error) {
	// Try to read cell information from /sys/class/net interfaces
	// This is a fallback method and may not provide complete information
	return nil, fmt.Errorf("sysfs method not implemented")
}

// getNeighborCellsViaUbusMobiled gets neighbor cells via ubus mobiled
func (cdc *CellularDataCollectorImpl) getNeighborCellsViaUbusMobiled(ctx context.Context) ([]NeighborCellInfo, error) {
	cmd := exec.CommandContext(ctx, "ubus", "call", "mobiled", "neighbors")
	output, err := cmd.Output()
	if err != nil {
		return nil, fmt.Errorf("ubus mobiled neighbors call failed: %w", err)
	}

	var response map[string]interface{}
	if err := json.Unmarshal(output, &response); err != nil {
		return nil, fmt.Errorf("failed to parse neighbors response: %w", err)
	}

	var neighbors []NeighborCellInfo

	// Parse neighbor cells from response
	if cells, ok := response["neighbors"].([]interface{}); ok {
		for _, cellData := range cells {
			if cell, ok := cellData.(map[string]interface{}); ok {
				neighbor := NeighborCellInfo{}

				if pcid, ok := cell["pcid"].(float64); ok {
					neighbor.PCID = int(pcid)
				}
				if rssi, ok := cell["rssi"].(float64); ok {
					neighbor.RSSI = int(rssi)
				}
				if rsrp, ok := cell["rsrp"].(float64); ok {
					neighbor.RSRP = int(rsrp)
				}
				if rsrq, ok := cell["rsrq"].(float64); ok {
					neighbor.RSRQ = int(rsrq)
				}
				if sinr, ok := cell["sinr"].(float64); ok {
					neighbor.SINR = int(sinr)
				}

				// Only add if we have minimum required info (PCID is required)
				if neighbor.PCID != 0 {
					neighbors = append(neighbors, neighbor)
				}
			}
		}
	}

	return neighbors, nil
}

// getNeighborCellsViaAT gets neighbor cells via AT commands
func (cdc *CellularDataCollectorImpl) getNeighborCellsViaAT(ctx context.Context) ([]NeighborCellInfo, error) {
	// AT command implementation would go here
	return nil, fmt.Errorf("AT commands not implemented")
}

// getCellularMetricsViaUbusMobiled gets cellular metrics via ubus mobiled
func (cdc *CellularDataCollectorImpl) getCellularMetricsViaUbusMobiled(ctx context.Context, metrics *CellularMetrics) error {
	cmd := exec.CommandContext(ctx, "ubus", "call", "mobiled", "status")
	output, err := cmd.Output()
	if err != nil {
		return fmt.Errorf("ubus mobiled call failed: %w", err)
	}

	var response map[string]interface{}
	if err := json.Unmarshal(output, &response); err != nil {
		return fmt.Errorf("failed to parse mobiled response: %w", err)
	}

	// Extract metrics from mobiled response
	if device, ok := response["device"].(map[string]interface{}); ok {
		if signal, ok := device["signal"].(map[string]interface{}); ok {
			if rsrp, ok := signal["rsrp"].(float64); ok {
				rsrpInt := int(rsrp)
				metrics.RSRP = &rsrpInt
				metrics.ServingRSRP = &rsrpInt
			}
			if rsrq, ok := signal["rsrq"].(float64); ok {
				rsrqInt := int(rsrq)
				metrics.RSRQ = &rsrqInt
			}
			if sinr, ok := signal["sinr"].(float64); ok {
				sinrInt := int(sinr)
				metrics.SINR = &sinrInt
			}
		}

		// Try to get timing advance if available
		if network, ok := device["network"].(map[string]interface{}); ok {
			if ta, ok := network["timing_advance"].(float64); ok {
				taInt := int(ta)
				metrics.TimingAdvance = &taInt
			}
		}

		// Try to get temperature if available
		if temp, ok := device["temperature"].(float64); ok {
			metrics.Temperature = &temp
		}
	}

	return nil
}

// getCellularMetricsViaAT gets cellular metrics via AT commands
func (cdc *CellularDataCollectorImpl) getCellularMetricsViaAT(ctx context.Context, metrics *CellularMetrics) error {
	// AT command implementation would go here
	// For now, just return without error (no additional metrics)
	return nil
}
