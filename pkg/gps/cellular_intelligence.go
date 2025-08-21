package gps

import (
	"context"
	"fmt"
	"strconv"
	"strings"
	"time"

	"github.com/markus-lassfolk/autonomy/pkg/logx"
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
	Source      string    `json:"source"`
	Valid       bool      `json:"valid"`
}

// ServingCellInfo represents the primary serving cell
type ServingCellInfo struct {
	CellID     string `json:"cell_id"`
	MCC        string `json:"mcc"`        // Mobile Country Code
	MNC        string `json:"mnc"`        // Mobile Network Code
	TAC        string `json:"tac"`        // Tracking Area Code (LTE) / LAC (GSM)
	Technology string `json:"technology"` // "GSM", "UMTS", "LTE", "NR"
	Band       string `json:"band"`
	EARFCN     int    `json:"earfcn"` // E-UTRA Absolute Radio Frequency Channel Number
	PCI        int    `json:"pci"`    // Physical Cell ID

	// Signal measurements
	RSSI int `json:"rssi"` // Received Signal Strength Indicator
	RSRP int `json:"rsrp"` // Reference Signal Received Power
	RSRQ int `json:"rsrq"` // Reference Signal Received Quality
	SINR int `json:"sinr"` // Signal to Interference plus Noise Ratio
	CQI  int `json:"cqi"`  // Channel Quality Indicator
}

// NeighborCellInfo represents a neighbor cell
type NeighborCellInfo struct {
	PCID int `json:"pcid"` // Physical Cell ID
	RSSI int `json:"rssi"`
	RSRP int `json:"rsrp"`
	RSRQ int `json:"rsrq"`
	SINR int `json:"sinr"`
}

// LocationFingerprint represents a unique cellular environment signature
type LocationFingerprint struct {
	PrimaryCellID    string   `json:"primary_cell_id"`
	NeighborCellIDs  []int    `json:"neighbor_cell_ids"`
	SignalPattern    string   `json:"signal_pattern"`
	FingerprintHash  string   `json:"fingerprint_hash"`
	Confidence       float64  `json:"confidence"`
	SimilarLocations []string `json:"similar_locations,omitempty"`
}

// SignalQuality represents overall signal quality metrics
type SignalQuality struct {
	OverallRSSI    int     `json:"overall_rssi"`
	OverallRSRP    int     `json:"overall_rsrp"`
	OverallRSRQ    int     `json:"overall_rsrq"`
	OverallSINR    int     `json:"overall_sinr"`
	QualityScore   float64 `json:"quality_score"`   // 0.0-1.0
	StabilityScore float64 `json:"stability_score"` // 0.0-1.0
}

// NetworkInfo represents network-level information
type NetworkInfo struct {
	OperatorName    string `json:"operator_name"`
	NetworkType     string `json:"network_type"`
	RoamingStatus   string `json:"roaming_status"`
	ConnectionState string `json:"connection_state"`
}

// CellularIntelligenceCollector collects and analyzes cellular location data
type CellularIntelligenceCollector struct {
	logger *logx.Logger
	config *CellularIntelligenceConfig
}

// CellularIntelligenceConfig holds configuration for cellular intelligence
type CellularIntelligenceConfig struct {
	MaxNeighborCells     int           `json:"max_neighbor_cells"`
	SignalThreshold      int           `json:"signal_threshold"`
	CollectionTimeout    time.Duration `json:"collection_timeout"`
	EnableFingerprinting bool          `json:"enable_fingerprinting"`
	Enable5GSupport      bool          `json:"enable_5g_support"`
}

// NewCellularIntelligenceCollector creates a new cellular intelligence collector
func NewCellularIntelligenceCollector(config *CellularIntelligenceConfig, logger *logx.Logger) *CellularIntelligenceCollector {
	if config == nil {
		config = &CellularIntelligenceConfig{
			MaxNeighborCells:     8,
			SignalThreshold:      -120, // dBm
			CollectionTimeout:    10 * time.Second,
			EnableFingerprinting: true,
			Enable5GSupport:      true,
		}
	}

	return &CellularIntelligenceCollector{
		logger: logger,
		config: config,
	}
}

// CollectCellularIntelligence collects comprehensive cellular location intelligence
func (cic *CellularIntelligenceCollector) CollectCellularIntelligence(ctx context.Context) (*CellularLocationIntelligence, error) {
	cic.logger.LogDebugVerbose("cellular_intelligence_collection_start", map[string]interface{}{
		"max_neighbors": cic.config.MaxNeighborCells,
		"5g_support":    cic.config.Enable5GSupport,
	})

	intelligence := &CellularLocationIntelligence{
		CollectedAt: time.Now(),
		Timestamp:   time.Now().Unix(),
		Source:      "cellular_intelligence_collector",
	}

	// Collect serving cell information
	servingCell, err := cic.collectServingCellInfo(ctx)
	if err != nil {
		return nil, fmt.Errorf("failed to collect serving cell info: %w", err)
	}
	intelligence.ServingCell = *servingCell

	// Collect neighbor cells
	neighborCells, err := cic.collectNeighborCells(ctx)
	if err != nil {
		cic.logger.LogDebugVerbose("neighbor_cell_collection_failed", map[string]interface{}{
			"error": err.Error(),
		})
		// Continue without neighbor cells
	} else {
		intelligence.NeighborCells = neighborCells
	}

	// Calculate signal quality metrics
	intelligence.SignalQuality = cic.calculateSignalQuality(servingCell, neighborCells)

	// Collect network information
	networkInfo, err := cic.collectNetworkInfo(ctx)
	if err != nil {
		cic.logger.LogDebugVerbose("network_info_collection_failed", map[string]interface{}{
			"error": err.Error(),
		})
	} else {
		intelligence.NetworkInfo = *networkInfo
	}

	// Generate location fingerprint if enabled
	if cic.config.EnableFingerprinting {
		intelligence.LocationFingerprint = cic.generateLocationFingerprint(servingCell, neighborCells)
	}

	intelligence.Valid = true

	cic.logger.Info("cellular_intelligence_collected",
		"serving_cell_id", servingCell.CellID,
		"neighbor_count", len(neighborCells),
		"signal_quality", intelligence.SignalQuality.QualityScore,
		"technology", servingCell.Technology,
	)

	return intelligence, nil
}

// collectServingCellInfo collects information about the serving cell
func (cic *CellularIntelligenceCollector) collectServingCellInfo(ctx context.Context) (*ServingCellInfo, error) {
	// This would typically use AT commands or system APIs
	// For now, we'll simulate the collection

	// In a real implementation, this would execute commands like:
	// AT+QENG="servingcell" for Quectel modems
	// or use system APIs to get cellular information

	servingCell := &ServingCellInfo{
		CellID:     "12345",
		MCC:        "240", // Sweden
		MNC:        "1",   // Telia
		TAC:        "54321",
		Technology: "LTE",
		Band:       "20",
		EARFCN:     6300,
		PCI:        123,
		RSSI:       -65,
		RSRP:       -85,
		RSRQ:       -10,
		SINR:       15,
		CQI:        12,
	}

	cic.logger.LogDebugVerbose("serving_cell_collected", map[string]interface{}{
		"cell_id":    servingCell.CellID,
		"technology": servingCell.Technology,
		"rssi":       servingCell.RSSI,
		"rsrp":       servingCell.RSRP,
	})

	return servingCell, nil
}

// collectNeighborCells collects information about neighbor cells
func (cic *CellularIntelligenceCollector) collectNeighborCells(ctx context.Context) ([]NeighborCellInfo, error) {
	// This would typically use AT commands to get neighbor cell information
	// For now, we'll simulate some neighbor cells

	neighbors := []NeighborCellInfo{
		{PCID: 124, RSSI: -75, RSRP: -95, RSRQ: -12, SINR: 10},
		{PCID: 125, RSSI: -80, RSRP: -100, RSRQ: -15, SINR: 8},
		{PCID: 126, RSSI: -85, RSRP: -105, RSRQ: -18, SINR: 5},
	}

	// Filter neighbors based on signal threshold
	var filteredNeighbors []NeighborCellInfo
	for _, neighbor := range neighbors {
		if neighbor.RSSI >= cic.config.SignalThreshold && len(filteredNeighbors) < cic.config.MaxNeighborCells {
			filteredNeighbors = append(filteredNeighbors, neighbor)
		}
	}

	cic.logger.LogDebugVerbose("neighbor_cells_collected", map[string]interface{}{
		"total_neighbors":    len(neighbors),
		"filtered_neighbors": len(filteredNeighbors),
		"signal_threshold":   cic.config.SignalThreshold,
	})

	return filteredNeighbors, nil
}

// collectNetworkInfo collects network-level information
func (cic *CellularIntelligenceCollector) collectNetworkInfo(ctx context.Context) (*NetworkInfo, error) {
	// This would typically query network status
	// For now, we'll provide simulated data

	networkInfo := &NetworkInfo{
		OperatorName:    "Telia",
		NetworkType:     "LTE",
		RoamingStatus:   "home",
		ConnectionState: "connected",
	}

	return networkInfo, nil
}

// calculateSignalQuality calculates overall signal quality metrics
func (cic *CellularIntelligenceCollector) calculateSignalQuality(servingCell *ServingCellInfo, neighbors []NeighborCellInfo) SignalQuality {
	quality := SignalQuality{
		OverallRSSI: servingCell.RSSI,
		OverallRSRP: servingCell.RSRP,
		OverallRSRQ: servingCell.RSRQ,
		OverallSINR: servingCell.SINR,
	}

	// Calculate quality score based on signal strength
	// RSSI: -50 to -120 dBm (higher is better)
	rssiScore := float64(servingCell.RSSI+120) / 70.0
	if rssiScore > 1.0 {
		rssiScore = 1.0
	}
	if rssiScore < 0.0 {
		rssiScore = 0.0
	}

	// SINR: 0 to 30 dB (higher is better)
	sinrScore := float64(servingCell.SINR) / 30.0
	if sinrScore > 1.0 {
		sinrScore = 1.0
	}
	if sinrScore < 0.0 {
		sinrScore = 0.0
	}

	quality.QualityScore = (rssiScore + sinrScore) / 2.0

	// Calculate stability score based on neighbor cell diversity
	neighborCount := len(neighbors)
	if neighborCount > 0 {
		quality.StabilityScore = 1.0 - (float64(neighborCount) / 10.0) // More neighbors = less stable
		if quality.StabilityScore < 0.3 {
			quality.StabilityScore = 0.3 // Minimum stability
		}
	} else {
		quality.StabilityScore = 0.5 // Default stability
	}

	return quality
}

// generateLocationFingerprint generates a unique fingerprint for the cellular environment
func (cic *CellularIntelligenceCollector) generateLocationFingerprint(servingCell *ServingCellInfo, neighbors []NeighborCellInfo) LocationFingerprint {
	fingerprint := LocationFingerprint{
		PrimaryCellID: servingCell.CellID,
	}

	// Collect neighbor PCIDs
	for _, neighbor := range neighbors {
		fingerprint.NeighborCellIDs = append(fingerprint.NeighborCellIDs, neighbor.PCID)
	}

	// Generate signal pattern string
	signalPattern := fmt.Sprintf("RSSI:%d,RSRP:%d,RSRQ:%d,SINR:%d",
		servingCell.RSSI, servingCell.RSRP, servingCell.RSRQ, servingCell.SINR)

	for _, neighbor := range neighbors {
		signalPattern += fmt.Sprintf(",N%d:%d", neighbor.PCID, neighbor.RSSI)
	}
	fingerprint.SignalPattern = signalPattern

	// Generate hash of the fingerprint
	fingerprintData := fmt.Sprintf("%s-%s-%s", servingCell.CellID,
		strings.Join(intSliceToStringSlice(fingerprint.NeighborCellIDs), ","),
		signalPattern)

	// Simple hash (in production, use a proper hash function)
	hash := fmt.Sprintf("%x", len(fingerprintData)*7919) // Simple hash
	fingerprint.FingerprintHash = hash

	// Calculate confidence based on signal quality and neighbor count
	baseConfidence := 0.5
	if len(neighbors) > 2 {
		baseConfidence += 0.3 // More neighbors = higher confidence
	}
	if servingCell.RSSI > -80 {
		baseConfidence += 0.2 // Strong signal = higher confidence
	}
	fingerprint.Confidence = baseConfidence

	return fingerprint
}

// Helper functions

func intSliceToStringSlice(ints []int) []string {
	strings := make([]string, len(ints))
	for i, v := range ints {
		strings[i] = strconv.Itoa(v)
	}
	return strings
}

// CompareCellularEnvironments compares two cellular environments for similarity
func (cic *CellularIntelligenceCollector) CompareCellularEnvironments(env1, env2 *CellularLocationIntelligence) float64 {
	if env1 == nil || env2 == nil {
		return 0.0
	}

	similarity := 0.0

	// Compare serving cells
	if env1.ServingCell.CellID == env2.ServingCell.CellID {
		similarity += 0.5 // 50% weight for same serving cell
	}

	// Compare neighbor cells
	commonNeighbors := 0
	for _, n1 := range env1.NeighborCells {
		for _, n2 := range env2.NeighborCells {
			if n1.PCID == n2.PCID {
				commonNeighbors++
				break
			}
		}
	}

	if len(env1.NeighborCells) > 0 || len(env2.NeighborCells) > 0 {
		maxNeighbors := len(env1.NeighborCells)
		if len(env2.NeighborCells) > maxNeighbors {
			maxNeighbors = len(env2.NeighborCells)
		}
		neighborSimilarity := float64(commonNeighbors) / float64(maxNeighbors)
		similarity += neighborSimilarity * 0.3 // 30% weight for neighbor similarity
	}

	// Compare signal patterns
	signalDiff := abs(env1.SignalQuality.OverallRSSI - env2.SignalQuality.OverallRSSI)
	signalSimilarity := 1.0 - (float64(signalDiff) / 50.0) // Normalize to 50dB range
	if signalSimilarity < 0 {
		signalSimilarity = 0
	}
	similarity += signalSimilarity * 0.2 // 20% weight for signal similarity

	return similarity
}

func abs(x int) int {
	if x < 0 {
		return -x
	}
	return x
}
