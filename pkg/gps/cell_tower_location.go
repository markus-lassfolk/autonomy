package gps

import (
	"bytes"
	"context"
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"strconv"
	"time"

	"github.com/markus-lassfolk/autonomy/pkg/logx"
)

// CellTowerLocationProvider provides location services using cell tower databases
type CellTowerLocationProvider struct {
	logger     *logx.Logger
	httpClient *http.Client
	config     *CellTowerConfig
}

// CellTowerConfig holds configuration for cell tower location services
type CellTowerConfig struct {
	OpenCellIDAPIKey  string        `json:"opencellid_api_key"`
	MozillaEnabled    bool          `json:"mozilla_enabled"`
	OpenCellIDEnabled bool          `json:"opencellid_enabled"`
	Timeout           time.Duration `json:"timeout"`
	MaxCells          int           `json:"max_cells"`
}

// CellTowerLocation represents location data from cell tower databases
type CellTowerLocation struct {
	Latitude     float64   `json:"latitude"`
	Longitude    float64   `json:"longitude"`
	Accuracy     float64   `json:"accuracy"`   // Accuracy radius in meters
	Source       string    `json:"source"`     // "mozilla", "opencellid"
	Method       string    `json:"method"`     // "single_cell", "triangulation"
	Confidence   float64   `json:"confidence"` // 0.0-1.0
	Valid        bool      `json:"valid"`
	Error        string    `json:"error,omitempty"`
	ResponseTime float64   `json:"response_time_ms"`
	CollectedAt  time.Time `json:"collected_at"`
	CellCount    int       `json:"cell_count"`
}

// MozillaLocationRequest represents request to Mozilla Location Service
type MozillaLocationRequest struct {
	CellTowers []MozillaCellTower `json:"cellTowers"`
}

type MozillaCellTower struct {
	RadioType         string `json:"radioType"`
	MobileCountryCode int    `json:"mobileCountryCode"`
	MobileNetworkCode int    `json:"mobileNetworkCode"`
	LocationAreaCode  int    `json:"locationAreaCode"`
	CellID            int    `json:"cellId"`
	SignalStrength    int    `json:"signalStrength,omitempty"`
}

// MozillaLocationResponse represents response from Mozilla Location Service
type MozillaLocationResponse struct {
	Location struct {
		Lat float64 `json:"lat"`
		Lng float64 `json:"lng"`
	} `json:"location"`
	Accuracy float64 `json:"accuracy"`
}

// OpenCellIDRequest represents request to OpenCellID API
type OpenCellIDRequest struct {
	Token string `json:"token"`
	Radio string `json:"radio"`
	MCC   int    `json:"mcc"`
	MNC   int    `json:"mnc"`
	Cells []struct {
		LAC int `json:"lac"`
		CID int `json:"cid"`
	} `json:"cells"`
}

// Note: OpenCellIDResponse is defined in opencellid_api.go

// NewCellTowerLocationProvider creates a new cell tower location provider
func NewCellTowerLocationProvider(config *CellTowerConfig, logger *logx.Logger) *CellTowerLocationProvider {
	if config == nil {
		config = &CellTowerConfig{
			MozillaEnabled:    true,
			OpenCellIDEnabled: false, // Requires API key
			Timeout:           30 * time.Second,
			MaxCells:          6,
		}
	}

	return &CellTowerLocationProvider{
		logger: logger,
		config: config,
		httpClient: &http.Client{
			Timeout: config.Timeout,
		},
	}
}

// GetLocationFromCellTowers gets location using cell tower data
func (ctp *CellTowerLocationProvider) GetLocationFromCellTowers(ctx context.Context, servingCell *ServingCellInfo, neighborCells []NeighborCellInfo) (*CellTowerLocation, error) {
	ctp.logger.LogDebugVerbose("cell_tower_location_start", map[string]interface{}{
		"serving_cell":   servingCell != nil,
		"neighbor_count": len(neighborCells),
	})

	if servingCell == nil {
		return nil, fmt.Errorf("no serving cell information available")
	}

	// Try multiple services in order of preference
	services := []func(context.Context, *ServingCellInfo, []NeighborCellInfo) (*CellTowerLocation, error){
		ctp.getMozillaLocation,    // Free, no API key required
		ctp.getOpenCellIDLocation, // Free with registration
	}

	var lastError error
	for _, service := range services {
		if location, err := service(ctx, servingCell, neighborCells); err == nil && location.Valid {
			ctp.logger.Info("cell_tower_location_success",
				"source", location.Source,
				"method", location.Method,
				"accuracy", location.Accuracy,
				"confidence", location.Confidence,
				"response_time", location.ResponseTime,
			)
			return location, nil
		} else {
			lastError = err
			if location != nil {
				ctp.logger.LogDebugVerbose("cell_tower_service_failed", map[string]interface{}{
					"source": location.Source,
					"error":  location.Error,
				})
			}
		}
	}

	return nil, fmt.Errorf("all cell tower location services failed: %v", lastError)
}

// getMozillaLocation gets location from Mozilla Location Service
func (ctp *CellTowerLocationProvider) getMozillaLocation(ctx context.Context, servingCell *ServingCellInfo, neighborCells []NeighborCellInfo) (*CellTowerLocation, error) {
	if !ctp.config.MozillaEnabled {
		return &CellTowerLocation{
			Source: "mozilla",
			Error:  "Mozilla Location Service disabled",
		}, fmt.Errorf("service disabled")
	}

	start := time.Now()
	location := &CellTowerLocation{
		Source:      "mozilla",
		Method:      "single_cell",
		CollectedAt: time.Now(),
	}

	// Parse cell data
	cellID, _ := strconv.Atoi(servingCell.CellID)
	mcc, _ := strconv.Atoi(servingCell.MCC)
	mnc, _ := strconv.Atoi(servingCell.MNC)
	lac, _ := strconv.Atoi(servingCell.TAC)

	// Prepare request with serving cell
	request := MozillaLocationRequest{
		CellTowers: []MozillaCellTower{{
			RadioType:         "lte",
			MobileCountryCode: mcc,
			MobileNetworkCode: mnc,
			LocationAreaCode:  lac,
			CellID:            cellID,
			SignalStrength:    servingCell.RSSI,
		}},
	}

	// Add neighbor cells for better triangulation
	cellCount := 1
	for _, neighbor := range neighborCells {
		if cellCount >= ctp.config.MaxCells {
			break
		}
		request.CellTowers = append(request.CellTowers, MozillaCellTower{
			RadioType:         "lte",
			MobileCountryCode: mcc,
			MobileNetworkCode: mnc,
			LocationAreaCode:  lac,
			CellID:            neighbor.PCID,
			SignalStrength:    neighbor.RSSI,
		})
		cellCount++
	}

	location.CellCount = cellCount

	// Make API request
	jsonData, _ := json.Marshal(request)
	req, err := http.NewRequestWithContext(ctx, "POST",
		"https://location.services.mozilla.com/v1/geolocate?key=test",
		bytes.NewBuffer(jsonData))
	if err != nil {
		location.Error = fmt.Sprintf("Failed to create request: %v", err)
		return location, err
	}
	req.Header.Set("Content-Type", "application/json")

	resp, err := ctp.httpClient.Do(req)
	if err != nil {
		location.Error = fmt.Sprintf("HTTP request failed: %v", err)
		return location, err
	}
	defer resp.Body.Close()

	location.ResponseTime = float64(time.Since(start).Nanoseconds()) / 1e6

	// Check HTTP status
	if resp.StatusCode != 200 {
		location.Error = fmt.Sprintf("HTTP error %d", resp.StatusCode)
		return location, fmt.Errorf("HTTP error %d", resp.StatusCode)
	}

	// Parse response
	body, err := io.ReadAll(resp.Body)
	if err != nil {
		location.Error = fmt.Sprintf("Failed to read response: %v", err)
		return location, err
	}

	var response MozillaLocationResponse
	if err := json.Unmarshal(body, &response); err != nil {
		location.Error = fmt.Sprintf("Failed to parse response: %v", err)
		return location, err
	}

	// Extract location data
	location.Latitude = response.Location.Lat
	location.Longitude = response.Location.Lng
	location.Accuracy = response.Accuracy
	location.Valid = location.Latitude != 0 && location.Longitude != 0
	location.Confidence = 0.7 // Medium confidence for cell tower location

	if cellCount > 1 {
		location.Method = "triangulation"
		location.Confidence = 0.8 // Higher confidence with multiple cells
	}

	return location, nil
}

// getOpenCellIDLocation gets location from OpenCellID
func (ctp *CellTowerLocationProvider) getOpenCellIDLocation(ctx context.Context, servingCell *ServingCellInfo, neighborCells []NeighborCellInfo) (*CellTowerLocation, error) {
	if !ctp.config.OpenCellIDEnabled || ctp.config.OpenCellIDAPIKey == "" {
		return &CellTowerLocation{
			Source: "opencellid",
			Error:  "OpenCellID API key not configured",
		}, fmt.Errorf("API key required")
	}

	start := time.Now()
	location := &CellTowerLocation{
		Source:      "opencellid",
		Method:      "single_cell",
		CollectedAt: time.Now(),
	}

	// Parse cell data
	cellID, _ := strconv.Atoi(servingCell.CellID)
	mcc, _ := strconv.Atoi(servingCell.MCC)
	mnc, _ := strconv.Atoi(servingCell.MNC)
	lac, _ := strconv.Atoi(servingCell.TAC)

	// Prepare request
	request := OpenCellIDRequest{
		Token: ctp.config.OpenCellIDAPIKey,
		Radio: "LTE",
		MCC:   mcc,
		MNC:   mnc,
		Cells: []struct {
			LAC int `json:"lac"`
			CID int `json:"cid"`
		}{{
			LAC: lac,
			CID: cellID,
		}},
	}

	location.CellCount = 1

	// Make API request
	jsonData, _ := json.Marshal(request)
	req, err := http.NewRequestWithContext(ctx, "POST",
		"https://us1.unwiredlabs.com/v2/process.php",
		bytes.NewBuffer(jsonData))
	if err != nil {
		location.Error = fmt.Sprintf("Failed to create request: %v", err)
		return location, err
	}
	req.Header.Set("Content-Type", "application/json")

	resp, err := ctp.httpClient.Do(req)
	if err != nil {
		location.Error = fmt.Sprintf("HTTP request failed: %v", err)
		return location, err
	}
	defer resp.Body.Close()

	location.ResponseTime = float64(time.Since(start).Nanoseconds()) / 1e6

	// Parse response
	body, err := io.ReadAll(resp.Body)
	if err != nil {
		location.Error = fmt.Sprintf("Failed to read response: %v", err)
		return location, err
	}

	var response OpenCellIDResponse
	if err := json.Unmarshal(body, &response); err != nil {
		location.Error = fmt.Sprintf("Failed to parse response: %v", err)
		return location, err
	}

	if response.Error != "" {
		location.Error = fmt.Sprintf("API error: %s", response.Error)
		return location, fmt.Errorf("API error: %s", response.Error)
	}

	// Extract location data
	location.Latitude = response.Lat
	location.Longitude = response.Lon
	location.Accuracy = float64(response.Range)
	location.Valid = location.Latitude != 0 && location.Longitude != 0
	location.Confidence = 0.85 // High confidence for OpenCellID

	return location, nil
}

// CalculateDistance calculates the distance between two GPS coordinates in meters
func (ctp *CellTowerLocationProvider) CalculateDistance(lat1, lon1, lat2, lon2 float64) float64 {
	const R = 6371000 // Earth's radius in meters

	lat1Rad := lat1 * (3.14159265359 / 180)
	lat2Rad := lat2 * (3.14159265359 / 180)
	deltaLatRad := (lat2 - lat1) * (3.14159265359 / 180)
	deltaLonRad := (lon2 - lon1) * (3.14159265359 / 180)

	a := (deltaLatRad/2)*(deltaLatRad/2) +
		(deltaLonRad/2)*(deltaLonRad/2)*
			(lat1Rad)*(lat2Rad)
	c := 2 * (a * (1 - a))

	return R * c
}

// CompareWithGPS compares cell tower location with GPS coordinates
func (ctp *CellTowerLocationProvider) CompareWithGPS(cellLocation *CellTowerLocation, gpsLat, gpsLon float64) map[string]interface{} {
	if !cellLocation.Valid {
		return map[string]interface{}{
			"valid":  false,
			"reason": "cell_location_invalid",
		}
	}

	distance := ctp.CalculateDistance(cellLocation.Latitude, cellLocation.Longitude, gpsLat, gpsLon)

	accuracy := "poor"
	if distance < cellLocation.Accuracy {
		accuracy = "excellent"
	} else if distance < cellLocation.Accuracy*2 {
		accuracy = "good"
	} else if distance < cellLocation.Accuracy*3 {
		accuracy = "fair"
	}

	return map[string]interface{}{
		"valid":             true,
		"gps_latitude":      gpsLat,
		"gps_longitude":     gpsLon,
		"cell_latitude":     cellLocation.Latitude,
		"cell_longitude":    cellLocation.Longitude,
		"distance_meters":   distance,
		"expected_accuracy": cellLocation.Accuracy,
		"accuracy_rating":   accuracy,
		"within_expected":   distance < cellLocation.Accuracy,
	}
}
