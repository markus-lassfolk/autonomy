package gps

import (
	"bytes"
	"context"
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"net/url"
	"strconv"
	"time"

	"github.com/markus-lassfolk/autonomy/pkg/logx"
)

// OpenCellIDProvider provides location services using OpenCellID API
type OpenCellIDProvider struct {
	logger     *logx.Logger
	httpClient *http.Client
	config     *OpenCellIDConfig
}

// OpenCellIDConfig holds configuration for OpenCellID API
type OpenCellIDConfig struct {
	APIKey         string        `json:"api_key"`
	BaseURL        string        `json:"base_url"`
	Timeout        time.Duration `json:"timeout"`
	ContributeData bool          `json:"contribute_data"`
	MaxRetries     int           `json:"max_retries"`
	RateLimitDelay time.Duration `json:"rate_limit_delay"`
}

// OpenCellIDResponse represents the OpenCellID API response format
type OpenCellIDResponse struct {
	Lat     float64 `json:"lat"`
	Lon     float64 `json:"lon"`
	MCC     int     `json:"mcc"`
	MNC     int     `json:"mnc"`
	LAC     int     `json:"lac"`
	CellID  int     `json:"cellid"`
	Range   int     `json:"range"`
	Samples int     `json:"samples"`
	Radio   string  `json:"radio"`
	Address string  `json:"address,omitempty"`
	Error   string  `json:"error,omitempty"`
	Message string  `json:"message,omitempty"`
}

// OpenCellIDContributionRequest represents a data contribution request
type OpenCellIDContributionRequest struct {
	Token string                       `json:"token"`
	Cells []OpenCellIDContributionCell `json:"cells"`
}

type OpenCellIDContributionCell struct {
	MCC    int     `json:"mcc"`
	MNC    int     `json:"mnc"`
	LAC    int     `json:"lac"`
	CellID int     `json:"cellid"`
	Lat    float64 `json:"lat"`
	Lon    float64 `json:"lon"`
	Radio  string  `json:"radio"`
	Range  int     `json:"range,omitempty"`
}

// NewOpenCellIDProvider creates a new OpenCellID provider
func NewOpenCellIDProvider(config *OpenCellIDConfig, logger *logx.Logger) *OpenCellIDProvider {
	if config == nil {
		config = &OpenCellIDConfig{
			BaseURL:        "https://us1.unwiredlabs.com/v2/process.php",
			Timeout:        30 * time.Second,
			ContributeData: false,
			MaxRetries:     3,
			RateLimitDelay: 1 * time.Second,
		}
	}

	return &OpenCellIDProvider{
		logger: logger,
		config: config,
		httpClient: &http.Client{
			Timeout: config.Timeout,
		},
	}
}

// GetLocationFromCell gets location for a specific cell from OpenCellID
func (ocp *OpenCellIDProvider) GetLocationFromCell(ctx context.Context, servingCell *ServingCellInfo) (*CellTowerLocation, error) {
	if ocp.config.APIKey == "" {
		return nil, fmt.Errorf("OpenCellID API key not configured")
	}

	if servingCell == nil {
		return nil, fmt.Errorf("no serving cell information available")
	}

	start := time.Now()
	location := &CellTowerLocation{
		Source:      "opencellid",
		Method:      "get_cell_position",
		CollectedAt: time.Now(),
	}

	// Parse cell data
	cellID, _ := strconv.Atoi(servingCell.CellID)
	mcc, _ := strconv.Atoi(servingCell.MCC)
	mnc, _ := strconv.Atoi(servingCell.MNC)
	lac, _ := strconv.Atoi(servingCell.TAC)

	// Build request URL with parameters
	params := url.Values{}
	params.Set("key", ocp.config.APIKey)
	params.Set("mcc", strconv.Itoa(mcc))
	params.Set("mnc", strconv.Itoa(mnc))
	params.Set("lac", strconv.Itoa(lac))
	params.Set("cellid", strconv.Itoa(cellID))
	params.Set("format", "json")

	requestURL := fmt.Sprintf("https://opencellid.org/cell/get?%s", params.Encode())

	ocp.logger.LogDebugVerbose("opencellid_request", map[string]interface{}{
		"mcc":    mcc,
		"mnc":    mnc,
		"lac":    lac,
		"cellid": cellID,
		"url":    requestURL,
	})

	// Make API request with retries
	var resp *http.Response
	var lastErr error
	for attempt := 0; attempt < ocp.config.MaxRetries; attempt++ {
		if attempt > 0 {
			time.Sleep(ocp.config.RateLimitDelay)
		}

		req, err := http.NewRequestWithContext(ctx, "GET", requestURL, nil)
		if err != nil {
			location.Error = fmt.Sprintf("Failed to create request: %v", err)
			return location, err
		}

		resp, lastErr = ocp.httpClient.Do(req)
		if lastErr == nil {
			break
		}

		ocp.logger.LogDebugVerbose("opencellid_retry", map[string]interface{}{
			"attempt": attempt + 1,
			"error":   lastErr.Error(),
		})
	}

	if lastErr != nil {
		location.Error = fmt.Sprintf("HTTP request failed after %d attempts: %v", ocp.config.MaxRetries, lastErr)
		return location, lastErr
	}
	defer resp.Body.Close()

	location.ResponseTime = float64(time.Since(start).Nanoseconds()) / 1e6

	// Check HTTP status
	if resp.StatusCode != 200 {
		body, _ := io.ReadAll(resp.Body)
		location.Error = fmt.Sprintf("HTTP error %d: %s", resp.StatusCode, string(body))
		return location, fmt.Errorf("HTTP error %d", resp.StatusCode)
	}

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

	// Check for API errors
	if response.Error != "" {
		location.Error = fmt.Sprintf("OpenCellID API error: %s", response.Error)
		return location, fmt.Errorf("API error: %s", response.Error)
	}

	if response.Message != "" && response.Lat == 0 && response.Lon == 0 {
		location.Error = fmt.Sprintf("No location data: %s", response.Message)
		return location, fmt.Errorf("no location data: %s", response.Message)
	}

	// Extract location data
	location.Latitude = response.Lat
	location.Longitude = response.Lon
	location.Accuracy = float64(response.Range)
	location.Valid = location.Latitude != 0 && location.Longitude != 0
	location.CellCount = 1

	// Calculate confidence based on sample count and range
	if response.Samples > 0 {
		// Higher confidence with more samples and smaller range
		sampleFactor := float64(response.Samples) / 100.0 // Normalize to 0-1
		if sampleFactor > 1.0 {
			sampleFactor = 1.0
		}

		rangeFactor := 1.0 - (float64(response.Range) / 10000.0) // Normalize to 0-1
		if rangeFactor < 0 {
			rangeFactor = 0
		}

		location.Confidence = (sampleFactor + rangeFactor) / 2.0
	} else {
		location.Confidence = 0.5 // Default confidence
	}

	ocp.logger.Info("opencellid_location_success",
		"latitude", location.Latitude,
		"longitude", location.Longitude,
		"accuracy", location.Accuracy,
		"confidence", location.Confidence,
		"samples", response.Samples,
		"response_time", location.ResponseTime,
	)

	return location, nil
}

// ContributeObservation contributes a GPS observation to OpenCellID
func (ocp *OpenCellIDProvider) ContributeObservation(ctx context.Context, observation *CellTowerObservation) error {
	if !ocp.config.ContributeData || ocp.config.APIKey == "" {
		return fmt.Errorf("contribution not enabled or API key not configured")
	}

	// Prepare contribution request
	request := OpenCellIDContributionRequest{
		Token: ocp.config.APIKey,
		Cells: []OpenCellIDContributionCell{{
			MCC:    observation.Cell_MCC,
			MNC:    observation.Cell_MNC,
			LAC:    observation.Cell_LAC,
			CellID: observation.Cell_ID,
			Lat:    observation.GPS_Latitude,
			Lon:    observation.GPS_Longitude,
			Radio:  observation.Cell_Technology,
			Range:  int(observation.GPS_Accuracy),
		}},
	}

	jsonData, err := json.Marshal(request)
	if err != nil {
		return fmt.Errorf("failed to marshal contribution request: %w", err)
	}

	// Make contribution request
	req, err := http.NewRequestWithContext(ctx, "POST",
		"https://opencellid.org/measure/add",
		bytes.NewBuffer(jsonData))
	if err != nil {
		return fmt.Errorf("failed to create contribution request: %w", err)
	}
	req.Header.Set("Content-Type", "application/json")

	resp, err := ocp.httpClient.Do(req)
	if err != nil {
		return fmt.Errorf("contribution request failed: %w", err)
	}
	defer resp.Body.Close()

	// Check response
	if resp.StatusCode != 200 {
		body, _ := io.ReadAll(resp.Body)
		return fmt.Errorf("contribution failed with HTTP %d: %s", resp.StatusCode, string(body))
	}

	ocp.logger.Info("opencellid_contribution_success",
		"cell_id", observation.Cell_ID,
		"mcc", observation.Cell_MCC,
		"mnc", observation.Cell_MNC,
		"lac", observation.Cell_LAC,
		"accuracy", observation.GPS_Accuracy,
	)

	return nil
}

// ContributeObservations contributes multiple observations in batch
func (ocp *OpenCellIDProvider) ContributeObservations(ctx context.Context, observations []CellTowerObservation) error {
	if !ocp.config.ContributeData || ocp.config.APIKey == "" {
		return fmt.Errorf("contribution not enabled or API key not configured")
	}

	if len(observations) == 0 {
		return nil
	}

	// Prepare batch contribution request
	cells := make([]OpenCellIDContributionCell, len(observations))
	for i, obs := range observations {
		cells[i] = OpenCellIDContributionCell{
			MCC:    obs.Cell_MCC,
			MNC:    obs.Cell_MNC,
			LAC:    obs.Cell_LAC,
			CellID: obs.Cell_ID,
			Lat:    obs.GPS_Latitude,
			Lon:    obs.GPS_Longitude,
			Radio:  obs.Cell_Technology,
			Range:  int(obs.GPS_Accuracy),
		}
	}

	request := OpenCellIDContributionRequest{
		Token: ocp.config.APIKey,
		Cells: cells,
	}

	jsonData, err := json.Marshal(request)
	if err != nil {
		return fmt.Errorf("failed to marshal batch contribution request: %w", err)
	}

	// Make batch contribution request
	req, err := http.NewRequestWithContext(ctx, "POST",
		"https://opencellid.org/measure/add",
		bytes.NewBuffer(jsonData))
	if err != nil {
		return fmt.Errorf("failed to create batch contribution request: %w", err)
	}
	req.Header.Set("Content-Type", "application/json")

	resp, err := ocp.httpClient.Do(req)
	if err != nil {
		return fmt.Errorf("batch contribution request failed: %w", err)
	}
	defer resp.Body.Close()

	// Check response
	if resp.StatusCode != 200 {
		body, _ := io.ReadAll(resp.Body)
		return fmt.Errorf("batch contribution failed with HTTP %d: %s", resp.StatusCode, string(body))
	}

	ocp.logger.Info("opencellid_batch_contribution_success",
		"observation_count", len(observations),
		"unique_cells", countUniqueCells(observations),
	)

	return nil
}

// GetAPIStatus checks the OpenCellID API status and remaining quota
func (ocp *OpenCellIDProvider) GetAPIStatus(ctx context.Context) (map[string]interface{}, error) {
	if ocp.config.APIKey == "" {
		return nil, fmt.Errorf("API key not configured")
	}

	// Make a simple query to check status
	params := url.Values{}
	params.Set("key", ocp.config.APIKey)
	params.Set("mcc", "240") // Sweden MCC for testing
	params.Set("mnc", "1")   // Telia MNC for testing
	params.Set("lac", "1")
	params.Set("cellid", "1")
	params.Set("format", "json")

	requestURL := fmt.Sprintf("https://opencellid.org/cell/get?%s", params.Encode())

	req, err := http.NewRequestWithContext(ctx, "GET", requestURL, nil)
	if err != nil {
		return nil, fmt.Errorf("failed to create status request: %w", err)
	}

	resp, err := ocp.httpClient.Do(req)
	if err != nil {
		return nil, fmt.Errorf("status request failed: %w", err)
	}
	defer resp.Body.Close()

	status := map[string]interface{}{
		"api_key_configured": true,
		"http_status":        resp.StatusCode,
		"api_accessible":     resp.StatusCode == 200 || resp.StatusCode == 404, // 404 is OK for non-existent cell
	}

	// Try to get quota information from headers
	if quota := resp.Header.Get("X-RateLimit-Remaining"); quota != "" {
		status["remaining_quota"] = quota
	}

	return status, nil
}

// Helper functions

func countUniqueCells(observations []CellTowerObservation) int {
	cellSet := make(map[string]bool)
	for _, obs := range observations {
		cellKey := fmt.Sprintf("%d-%d-%d-%d", obs.Cell_MCC, obs.Cell_MNC, obs.Cell_LAC, obs.Cell_ID)
		cellSet[cellKey] = true
	}
	return len(cellSet)
}
