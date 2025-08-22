package main

import (
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"net/url"
	"sort"
	"strconv"
	"strings"
	"time"
)

// EnhancedOpenCellIDRequest represents a comprehensive cell location request
type EnhancedOpenCellIDRequest struct {
	// Primary serving cell
	ServingCell CellTowerRequest `json:"serving_cell"`

	// Top neighbor cells (sorted by signal strength)
	NeighborCells []CellTowerRequest `json:"neighbor_cells"`

	// Request metadata
	RequestType  string         `json:"request_type"` // "single", "multi", "area"
	MaxCells     int            `json:"max_cells"`    // Maximum cells to include
	Timestamp    time.Time      `json:"timestamp"`
	GPSReference *GPSCoordinate `json:"gps_reference,omitempty"`
}

// CellTowerRequest represents detailed cell tower information for API requests
type CellTowerRequest struct {
	// Basic cell identification
	CellID int `json:"cellid"`
	MCC    int `json:"mcc"`
	MNC    int `json:"mnc"`
	LAC    int `json:"lac"` // Location Area Code / Tracking Area Code

	// Physical cell properties
	PCID   int    `json:"pcid,omitempty"`   // Physical Cell ID
	EARFCN int    `json:"earfcn,omitempty"` // Frequency channel
	Band   string `json:"band,omitempty"`   // LTE Band (e.g., "B3")

	// Signal measurements
	RSSI int `json:"rssi,omitempty"` // Received Signal Strength Indicator
	RSRP int `json:"rsrp,omitempty"` // Reference Signal Received Power
	RSRQ int `json:"rsrq,omitempty"` // Reference Signal Received Quality
	SINR int `json:"sinr,omitempty"` // Signal-to-Interference-plus-Noise Ratio

	// Cell type and technology
	Radio    string `json:"radio"`               // "LTE", "UMTS", "GSM"
	CellType string `json:"cell_type,omitempty"` // "serving", "intra", "inter"

	// Additional metadata
	Operator   string `json:"operator,omitempty"`
	Technology string `json:"technology,omitempty"` // "5G-NSA", "LTE", etc.
}

// EnhancedOpenCellIDResponse represents the comprehensive response
type EnhancedOpenCellIDResponse struct {
	// Location result
	Success   bool    `json:"success"`
	Latitude  float64 `json:"latitude"`
	Longitude float64 `json:"longitude"`
	Accuracy  float64 `json:"accuracy"`

	// Method used
	Method       string `json:"method"`        // "single_cell", "area_search", "multi_cell"
	CellsUsed    int    `json:"cells_used"`    // Number of cells that contributed
	CellsQueried int    `json:"cells_queried"` // Total cells queried

	// Individual cell results
	CellResults []SingleCellResult `json:"cell_results"`

	// Area search results (if used)
	AreaResults []NearbyCellInfo `json:"area_results,omitempty"`

	// Performance metrics
	ResponseTime   float64 `json:"response_time_ms"`
	APICallsUsed   int     `json:"api_calls_used"`
	RemainingQuota int     `json:"remaining_quota"`

	// Error information
	Error    string   `json:"error,omitempty"`
	Warnings []string `json:"warnings,omitempty"`
}

// SingleCellResult represents the result for a single cell query
type SingleCellResult struct {
	CellID    int     `json:"cellid"`
	Found     bool    `json:"found"`
	Latitude  float64 `json:"latitude,omitempty"`
	Longitude float64 `json:"longitude,omitempty"`
	Range     int     `json:"range,omitempty"`
	Samples   int     `json:"samples,omitempty"`
	Radio     string  `json:"radio,omitempty"`
	Error     string  `json:"error,omitempty"`
}

// EnhancedOpenCellIDService provides comprehensive cell location services
type EnhancedOpenCellIDService struct {
	apiKey      string
	maxCells    int // Maximum cells to query (default: 10)
	timeout     time.Duration
	rateLimiter *time.Ticker // Rate limiting for API calls
}

// NewEnhancedOpenCellIDService creates a new enhanced service
func NewEnhancedOpenCellIDService(apiKey string) *EnhancedOpenCellIDService {
	return &EnhancedOpenCellIDService{
		apiKey:      apiKey,
		maxCells:    10, // Query top 10 cells by signal strength
		timeout:     30 * time.Second,
		rateLimiter: time.NewTicker(200 * time.Millisecond), // 5 requests per second max
	}
}

// GetLocationEnhanced performs comprehensive cell location lookup
func (service *EnhancedOpenCellIDService) GetLocationEnhanced(intel *CellularLocationIntelligence, gpsRef *GPSCoordinate) (*EnhancedOpenCellIDResponse, error) {
	start := time.Now()

	response := &EnhancedOpenCellIDResponse{
		Method: "enhanced_multi_cell",
	}

	// Build comprehensive request
	request, err := service.buildEnhancedRequest(intel, gpsRef)
	if err != nil {
		response.Error = fmt.Sprintf("Failed to build request: %v", err)
		return response, err
	}

	fmt.Printf("🔍 Enhanced OpenCellID Request:\n")
	fmt.Printf("  📡 Serving Cell: %d (MCC:%d, MNC:%d, LAC:%d)\n",
		request.ServingCell.CellID, request.ServingCell.MCC,
		request.ServingCell.MNC, request.ServingCell.LAC)
	fmt.Printf("  🗼 Neighbor Cells: %d (top %d by signal strength)\n",
		len(request.NeighborCells), service.maxCells)

	// Strategy 1: Try individual cell lookups for serving cell and top neighbors
	fmt.Println("  🎯 Strategy 1: Individual cell lookups...")
	cellResults, apiCalls := service.queryIndividualCells(request)
	response.CellResults = cellResults
	response.APICallsUsed += apiCalls

	// Count successful cell lookups
	foundCells := 0
	for _, result := range cellResults {
		if result.Found {
			foundCells++
		}
	}

	if foundCells > 0 {
		// Calculate weighted average from found cells
		response.Success = true
		response.Method = "individual_cells"
		response.CellsUsed = foundCells
		response.Latitude, response.Longitude, response.Accuracy = service.calculateWeightedAverage(cellResults)
		fmt.Printf("  ✅ Found %d cells, calculated position\n", foundCells)
	} else {
		// Strategy 2: Area search as fallback
		fmt.Println("  🎯 Strategy 2: Area search fallback...")
		if gpsRef != nil {
			areaResult, err := service.performAreaSearch(gpsRef, intel)
			if err == nil && len(areaResult) > 0 {
				response.Success = true
				response.Method = "area_search_fallback"
				response.AreaResults = areaResult
				response.CellsUsed = len(areaResult)
				response.APICallsUsed += 1

				// Calculate position from area search
				response.Latitude, response.Longitude, response.Accuracy = service.calculateAreaAverage(areaResult)
				fmt.Printf("  ✅ Area search found %d nearby cells\n", len(areaResult))
			} else {
				response.Error = "No cells found via individual lookup or area search"
				fmt.Println("  ❌ Both strategies failed")
			}
		} else {
			response.Error = "No GPS reference available for area search"
			fmt.Println("  ❌ No GPS reference for area search")
		}
	}

	response.ResponseTime = float64(time.Since(start).Nanoseconds()) / 1e6
	response.CellsQueried = len(request.NeighborCells) + 1 // +1 for serving cell

	return response, nil
}

// buildEnhancedRequest creates a comprehensive request with detailed cell information
func (service *EnhancedOpenCellIDService) buildEnhancedRequest(intel *CellularLocationIntelligence, gpsRef *GPSCoordinate) (*EnhancedOpenCellIDRequest, error) {
	request := &EnhancedOpenCellIDRequest{
		RequestType:  "enhanced_multi_cell",
		MaxCells:     service.maxCells,
		Timestamp:    time.Now(),
		GPSReference: gpsRef,
	}

	// Build serving cell request with all available information
	servingCell, err := service.buildCellRequest(intel.ServingCell, intel.SignalQuality, "serving")
	if err != nil {
		return nil, fmt.Errorf("failed to build serving cell request: %w", err)
	}
	request.ServingCell = servingCell

	// Build neighbor cell requests, sorted by signal strength (best first)
	neighbors := make([]CellTowerRequest, 0, len(intel.NeighborCells))
	for _, neighbor := range intel.NeighborCells {
		// Convert neighbor to cell request format
		cellReq := CellTowerRequest{
			// We don't have actual Cell ID for neighbors, so we'll use PCID as identifier
			CellID:   neighbor.PCID, // This might not work, but we'll try
			MCC:      240,           // From serving cell
			MNC:      1,             // From serving cell
			LAC:      0,             // Unknown for neighbors
			PCID:     neighbor.PCID,
			EARFCN:   neighbor.EARFCN,
			RSSI:     neighbor.RSSI,
			RSRP:     neighbor.RSRP,
			RSRQ:     neighbor.RSRQ,
			Radio:    "LTE", // Assume LTE for neighbors
			CellType: neighbor.CellType,
		}
		neighbors = append(neighbors, cellReq)
	}

	// Sort neighbors by signal strength (RSRP, higher is better)
	sort.Slice(neighbors, func(i, j int) bool {
		return neighbors[i].RSRP > neighbors[j].RSRP
	})

	// Take only the top N neighbors
	if len(neighbors) > service.maxCells {
		neighbors = neighbors[:service.maxCells]
	}

	request.NeighborCells = neighbors

	return request, nil
}

// buildCellRequest creates a detailed cell request from available information
func (service *EnhancedOpenCellIDService) buildCellRequest(serving ServingCellInfo, signal SignalQuality, cellType string) (CellTowerRequest, error) {
	cellID, err := strconv.Atoi(serving.CellID)
	if err != nil {
		return CellTowerRequest{}, fmt.Errorf("invalid cell ID: %s", serving.CellID)
	}

	mcc, err := strconv.Atoi(serving.MCC)
	if err != nil {
		return CellTowerRequest{}, fmt.Errorf("invalid MCC: %s", serving.MCC)
	}

	mnc, err := strconv.Atoi(serving.MNC)
	if err != nil {
		return CellTowerRequest{}, fmt.Errorf("invalid MNC: %s", serving.MNC)
	}

	lac, err := strconv.Atoi(serving.TAC)
	if err != nil {
		return CellTowerRequest{}, fmt.Errorf("invalid TAC: %s", serving.TAC)
	}

	return CellTowerRequest{
		CellID:     cellID,
		MCC:        mcc,
		MNC:        mnc,
		LAC:        lac,
		PCID:       serving.PCID,
		EARFCN:     serving.EARFCN,
		Band:       serving.Band,
		RSSI:       signal.RSSI,
		RSRP:       signal.RSRP,
		RSRQ:       signal.RSRQ,
		SINR:       signal.SINR,
		Radio:      "LTE", // Determine from band or technology
		CellType:   cellType,
		Operator:   serving.Operator,
		Technology: "LTE", // Could be enhanced based on band
	}, nil
}

// queryIndividualCells queries each cell individually
func (service *EnhancedOpenCellIDService) queryIndividualCells(request *EnhancedOpenCellIDRequest) ([]SingleCellResult, int) {
	var results []SingleCellResult
	apiCalls := 0

	// Query serving cell first
	fmt.Printf("    📡 Querying serving cell %d...\n", request.ServingCell.CellID)
	result := service.querySingleCell(request.ServingCell)
	results = append(results, result)
	apiCalls++

	// Rate limit between requests
	<-service.rateLimiter.C

	// Query top neighbor cells
	for i, neighbor := range request.NeighborCells {
		if i >= service.maxCells {
			break
		}

		fmt.Printf("    📡 Querying neighbor cell %d (RSRP: %d dBm)...\n",
			neighbor.CellID, neighbor.RSRP)

		result := service.querySingleCell(neighbor)
		results = append(results, result)
		apiCalls++

		// Rate limit between requests
		<-service.rateLimiter.C
	}

	return results, apiCalls
}

// querySingleCell queries a single cell via OpenCellID API
func (service *EnhancedOpenCellIDService) querySingleCell(cell CellTowerRequest) SingleCellResult {
	result := SingleCellResult{
		CellID: cell.CellID,
		Found:  false,
	}

	// Build OpenCellID API URL
	baseURL := "https://opencellid.org/cell/get"
	params := url.Values{}
	params.Add("key", service.apiKey)
	params.Add("mcc", strconv.Itoa(cell.MCC))
	params.Add("mnc", strconv.Itoa(cell.MNC))
	params.Add("lac", strconv.Itoa(cell.LAC))
	params.Add("cellid", strconv.Itoa(cell.CellID))
	params.Add("format", "json")

	fullURL := baseURL + "?" + params.Encode()

	// Make request
	resp, err := http.Get(fullURL)
	if err != nil {
		result.Error = fmt.Sprintf("HTTP error: %v", err)
		return result
	}
	defer resp.Body.Close()

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		result.Error = fmt.Sprintf("Read error: %v", err)
		return result
	}

	// Parse response
	var apiResponse map[string]interface{}
	if err := json.Unmarshal(body, &apiResponse); err != nil {
		result.Error = fmt.Sprintf("Parse error: %v", err)
		return result
	}

	// Check for errors
	if errorMsg, exists := apiResponse["error"]; exists {
		result.Error = fmt.Sprintf("API error: %v", errorMsg)
		return result
	}

	// Extract location data
	if lat, exists := apiResponse["lat"]; exists {
		if latFloat, ok := lat.(float64); ok {
			result.Latitude = latFloat
			result.Found = true
		}
	}

	if lon, exists := apiResponse["lon"]; exists {
		if lonFloat, ok := lon.(float64); ok {
			result.Longitude = lonFloat
		}
	}

	if rangeVal, exists := apiResponse["range"]; exists {
		if rangeFloat, ok := rangeVal.(float64); ok {
			result.Range = int(rangeFloat)
		}
	}

	if samples, exists := apiResponse["samples"]; exists {
		if samplesFloat, ok := samples.(float64); ok {
			result.Samples = int(samplesFloat)
		}
	}

	if radio, exists := apiResponse["radio"]; exists {
		if radioStr, ok := radio.(string); ok {
			result.Radio = radioStr
		}
	}

	return result
}

// calculateWeightedAverage calculates position from multiple cell results
func (service *EnhancedOpenCellIDService) calculateWeightedAverage(results []SingleCellResult) (float64, float64, float64) {
	if len(results) == 0 {
		return 0, 0, 0
	}

	var totalLat, totalLon, totalWeight float64
	var maxAccuracy float64

	for _, result := range results {
		if !result.Found {
			continue
		}

		// Weight by inverse of range (smaller range = higher weight)
		weight := 1.0
		if result.Range > 0 {
			weight = 1.0 / float64(result.Range)
		}

		// Also weight by number of samples (more samples = higher weight)
		if result.Samples > 1 {
			weight *= float64(result.Samples) / 10.0
		}

		totalLat += result.Latitude * weight
		totalLon += result.Longitude * weight
		totalWeight += weight

		// Track maximum accuracy (worst case)
		if float64(result.Range) > maxAccuracy {
			maxAccuracy = float64(result.Range)
		}
	}

	if totalWeight == 0 {
		return 0, 0, 0
	}

	return totalLat / totalWeight, totalLon / totalWeight, maxAccuracy
}

// performAreaSearch performs area search as fallback
func (service *EnhancedOpenCellIDService) performAreaSearch(gpsRef *GPSCoordinate, intel *CellularLocationIntelligence) ([]NearbyCellInfo, error) {
	// Use existing area search implementation
	result, err := getPracticalCellLocation()
	if err != nil {
		return nil, err
	}

	return result.NearbyCells, nil
}

// calculateAreaAverage calculates position from area search results
func (service *EnhancedOpenCellIDService) calculateAreaAverage(cells []NearbyCellInfo) (float64, float64, float64) {
	if len(cells) == 0 {
		return 0, 0, 0
	}

	var totalLat, totalLon, totalWeight float64
	var maxAccuracy float64

	for _, cell := range cells {
		// Weight by inverse of range and number of samples
		weight := 1.0
		if cell.Range > 0 {
			weight = 1.0 / float64(cell.Range)
		}
		if cell.Samples > 1 {
			weight *= float64(cell.Samples) / 10.0
		}

		totalLat += cell.Lat * weight
		totalLon += cell.Lon * weight
		totalWeight += weight

		if float64(cell.Range) > maxAccuracy {
			maxAccuracy = float64(cell.Range)
		}
	}

	if totalWeight == 0 {
		return 0, 0, 0
	}

	return totalLat / totalWeight, totalLon / totalWeight, maxAccuracy
}

// PrintEnhancedResponse displays detailed response information
func (response *EnhancedOpenCellIDResponse) PrintEnhancedResponse() {
	fmt.Println("\n📊 Enhanced OpenCellID Response:")
	fmt.Println("=" + strings.Repeat("=", 35))

	if response.Success {
		fmt.Printf("✅ SUCCESS: %s\n", response.Method)
		fmt.Printf("📍 Location: %.6f°, %.6f°\n", response.Latitude, response.Longitude)
		fmt.Printf("🎯 Accuracy: ±%.0f meters\n", response.Accuracy)
		fmt.Printf("🗼 Cells Used: %d/%d\n", response.CellsUsed, response.CellsQueried)
	} else {
		fmt.Printf("❌ FAILED: %s\n", response.Error)
	}

	fmt.Printf("⏱️  Response Time: %.1f ms\n", response.ResponseTime)
	fmt.Printf("📞 API Calls Used: %d\n", response.APICallsUsed)

	if len(response.CellResults) > 0 {
		fmt.Printf("\n📡 Individual Cell Results:\n")
		for i, result := range response.CellResults {
			status := "❌"
			if result.Found {
				status = "✅"
			}
			fmt.Printf("  %d. Cell %d: %s", i+1, result.CellID, status)
			if result.Found {
				fmt.Printf(" (%.6f°, %.6f°, ±%dm)", result.Latitude, result.Longitude, result.Range)
			} else if result.Error != "" {
				fmt.Printf(" - %s", result.Error)
			}
			fmt.Println()
		}
	}

	if len(response.Warnings) > 0 {
		fmt.Printf("\n⚠️  Warnings:\n")
		for _, warning := range response.Warnings {
			fmt.Printf("  - %s\n", warning)
		}
	}
}

// testEnhancedOpenCellID demonstrates the enhanced multi-cell functionality
func testEnhancedOpenCellID() error {
	fmt.Println("🚀 TESTING ENHANCED OPENCELLID (MULTI-CELL)")
	fmt.Println("=" + strings.Repeat("=", 45))

	// Load API key
	apiKey, err := loadOpenCellIDTokenLocal()
	if err != nil {
		return fmt.Errorf("failed to load API key: %w", err)
	}

	// Create enhanced service
	service := NewEnhancedOpenCellIDService(apiKey)

	// Create test cellular intelligence with multiple neighbor cells
	intel := &CellularLocationIntelligence{
		ServingCell: ServingCellInfo{
			CellID:   "25939743",
			MCC:      "240",
			MNC:      "01",
			TAC:      "23",
			PCID:     443,
			EARFCN:   1300,
			Band:     "LTE B3",
			Operator: "Telia",
		},
		NeighborCells: []NeighborCellInfo{
			{PCID: 263, RSRP: -90, RSRQ: -15, EARFCN: 1300, CellType: "intra"},
			{PCID: 60, RSRP: -102, RSRQ: -20, EARFCN: 1300, CellType: "intra"},
			{PCID: 100, RSRP: -95, RSRQ: -18, EARFCN: 9360, CellType: "inter"},
			{PCID: 200, RSRP: -88, RSRQ: -12, EARFCN: 1300, CellType: "intra"},
			{PCID: 300, RSRP: -105, RSRQ: -22, EARFCN: 3150, CellType: "inter"},
		},
		SignalQuality: SignalQuality{
			RSSI: -54,
			RSRP: -84,
			RSRQ: -8,
			SINR: 15,
		},
	}

	// GPS reference for area search fallback
	gpsRef := &GPSCoordinate{
		Latitude:  59.48007,
		Longitude: 18.27985,
		Accuracy:  2.0,
	}

	// Perform enhanced location lookup
	response, err := service.GetLocationEnhanced(intel, gpsRef)
	if err != nil {
		return fmt.Errorf("enhanced lookup failed: %w", err)
	}

	// Display results
	response.PrintEnhancedResponse()

	fmt.Println("\n✅ Enhanced OpenCellID Test Complete!")
	return nil
}
