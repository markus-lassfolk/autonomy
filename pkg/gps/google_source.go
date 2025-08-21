package gps

import (
	"bytes"
	"context"
	"encoding/json"
	"fmt"
	"io"
	"math"
	"net/http"
	"os/exec"
	"strconv"
	"strings"
	"time"

	"github.com/markus-lassfolk/autonomy/pkg/logx"
)

// GoogleLocationSource implements Google Geolocation API GPS collection
type GoogleLocationSource struct {
	logger             *logx.Logger
	priority           int
	apiKey             string
	useGoogleElevation bool // New: Configure whether to use Google Elevation API
	health             GPSSourceHealth
	errorCount         int
	successCount       int
	httpClient         *http.Client
	lastNotification   time.Time
}

// GoogleLocationRequest represents the request to Google Geolocation API
type GoogleLocationRequest struct {
	HomeMobileCountryCode int                     `json:"homeMobileCountryCode,omitempty"`
	HomeMobileNetworkCode int                     `json:"homeMobileNetworkCode,omitempty"`
	RadioType             string                  `json:"radioType,omitempty"`
	Carrier               string                  `json:"carrier,omitempty"`
	ConsiderIp            bool                    `json:"considerIp"`
	CellTowers            []GoogleCellTower       `json:"cellTowers,omitempty"`
	WifiAccessPoints      []GoogleWifiAccessPoint `json:"wifiAccessPoints,omitempty"`
}

// GoogleCellTower represents a cell tower for Google API
type GoogleCellTower struct {
	CellId            int `json:"cellId"`
	LocationAreaCode  int `json:"locationAreaCode"`
	MobileCountryCode int `json:"mobileCountryCode"`
	MobileNetworkCode int `json:"mobileNetworkCode"`
	Age               int `json:"age,omitempty"`
	SignalStrength    int `json:"signalStrength,omitempty"`
	TimingAdvance     int `json:"timingAdvance,omitempty"`
}

// GoogleWifiAccessPoint represents a WiFi access point for Google API
type GoogleWifiAccessPoint struct {
	MacAddress         string `json:"macAddress"`
	SignalStrength     int    `json:"signalStrength,omitempty"`
	Age                int    `json:"age,omitempty"`
	Channel            int    `json:"channel,omitempty"`
	SignalToNoiseRatio int    `json:"signalToNoiseRatio,omitempty"`
}

// GoogleLocationResponse represents the response from Google Geolocation API
type GoogleLocationResponse struct {
	Location struct {
		Lat float64 `json:"lat"`
		Lng float64 `json:"lng"`
	} `json:"location"`
	Accuracy float64 `json:"accuracy"`
	Error    struct {
		Code    int    `json:"code"`
		Message string `json:"message"`
	} `json:"error,omitempty"`
}

// OpenElevationResponse represents response from Open Elevation API
type OpenElevationResponse struct {
	Results []struct {
		Latitude  float64 `json:"latitude"`
		Longitude float64 `json:"longitude"`
		Elevation float64 `json:"elevation"`
	} `json:"results"`
}

// GoogleMapsElevationResponse represents response from Google Maps Elevation API
type GoogleMapsElevationResponse struct {
	Results []struct {
		Elevation float64 `json:"elevation"`
		Location  struct {
			Lat float64 `json:"lat"`
			Lng float64 `json:"lng"`
		} `json:"location"`
		Resolution float64 `json:"resolution"`
	} `json:"results"`
	Status string `json:"status"`
}

// NewGoogleLocationSource creates a new Google Location API source
func NewGoogleLocationSource(priority int, apiKey string, useGoogleElevation bool, logger *logx.Logger) *GoogleLocationSource {
	return &GoogleLocationSource{
		logger:             logger,
		priority:           priority,
		apiKey:             apiKey,
		useGoogleElevation: useGoogleElevation,
		health: GPSSourceHealth{
			Available:    false,
			LastSuccess:  time.Time{},
			LastError:    "",
			SuccessRate:  0.0,
			AvgLatency:   0.0,
			ErrorCount:   0,
			SuccessCount: 0,
		},
		httpClient: &http.Client{
			Timeout: 30 * time.Second,
		},
	}
}

// GetName returns the source name
func (gs *GoogleLocationSource) GetName() string {
	return "google"
}

// GetPriority returns the source priority
func (gs *GoogleLocationSource) GetPriority() int {
	return gs.priority
}

// GetHealthStatus returns the current health status
func (gs *GoogleLocationSource) GetHealthStatus() GPSSourceHealth {
	total := gs.errorCount + gs.successCount
	if total > 0 {
		gs.health.SuccessRate = float64(gs.successCount) / float64(total)
	}
	return gs.health
}

// IsAvailable checks if Google Location API is available
func (gs *GoogleLocationSource) IsAvailable(ctx context.Context) bool {
	// Check if API key is configured and valid
	if gs.apiKey == "" || len(gs.apiKey) < 10 {
		gs.logger.LogDebugVerbose("google_location_availability_check", map[string]interface{}{
			"available":  false,
			"reason":     "no_valid_api_key_configured",
			"key_length": len(gs.apiKey),
		})
		gs.health.Available = false
		return false
	}

	// Check if we can collect cellular or WiFi data
	hasCellular := gs.canCollectCellularData(ctx)
	hasWiFi := gs.canCollectWiFiData(ctx)

	availableDataSources := []string{}
	if hasCellular {
		availableDataSources = append(availableDataSources, "cellular")
	}
	if hasWiFi {
		availableDataSources = append(availableDataSources, "wifi")
	}

	isAvailable := hasCellular || hasWiFi
	gs.health.Available = isAvailable

	gs.logger.LogDebugVerbose("google_location_availability_check", map[string]interface{}{
		"available":          isAvailable,
		"has_api_key":        true,
		"cellular_available": hasCellular,
		"wifi_available":     hasWiFi,
		"data_sources":       availableDataSources,
		"data_source_count":  len(availableDataSources),
	})

	if !isAvailable {
		gs.logger.LogDebugVerbose("google_location_unavailable", map[string]interface{}{
			"reason": "no_cellular_modem_or_wifi_interface_detected",
			"note":   "Google Location API requires cellular towers or WiFi access points for triangulation",
		})
	}

	return isAvailable
}

// CollectGPS collects GPS data using Google Geolocation API
func (gs *GoogleLocationSource) CollectGPS(ctx context.Context) (*StandardizedGPSData, error) {
	start := time.Now()

	// Collect cellular and WiFi data
	cellTowers, err := gs.collectCellularData(ctx)
	if err != nil {
		gs.logger.LogDebugVerbose("google_cellular_collection_failed", map[string]interface{}{
			"error": err.Error(),
		})
	}

	wifiAPs, err := gs.collectWiFiData(ctx)
	if err != nil {
		gs.logger.LogDebugVerbose("google_wifi_collection_failed", map[string]interface{}{
			"error": err.Error(),
		})
	}

	// Check if we have any data to send
	if len(cellTowers) == 0 && len(wifiAPs) == 0 {
		gs.errorCount++
		gs.health.LastError = "no cellular or WiFi data available"
		return nil, fmt.Errorf("no cellular or WiFi data available for Google API")
	}

	// Query Google Location API
	location, err := gs.queryGoogleLocationAPI(ctx, cellTowers, wifiAPs)
	if err != nil {
		gs.errorCount++
		gs.health.LastError = err.Error()
		return nil, fmt.Errorf("Google Location API failed: %w", err)
	}

	// Get altitude estimation
	altitude := gs.estimateAltitude(ctx, location.Location.Lat, location.Location.Lng)

	// Convert to standardized format
	standardized := &StandardizedGPSData{
		Latitude:       location.Location.Lat,
		Longitude:      location.Location.Lng,
		Altitude:       altitude,
		Accuracy:       location.Accuracy,
		FixType:        1, // Network-based fix
		FixQuality:     gs.determineQualityFromAccuracy(location.Accuracy),
		Source:         "Google Geolocation",
		Method:         "google_api",
		DataSources:    gs.buildDataSources(cellTowers, wifiAPs),
		Valid:          true,
		Confidence:     gs.calculateConfidence(location.Accuracy, len(cellTowers), len(wifiAPs)),
		Timestamp:      time.Now(),
		CollectionTime: time.Since(start),
		APICallMade:    true,
		APICost:        0.005, // Approximate cost per request
	}

	// Update health metrics
	gs.successCount++
	gs.health.LastSuccess = time.Now()
	gs.health.AvgLatency = (gs.health.AvgLatency*float64(gs.successCount-1) + float64(standardized.CollectionTime.Milliseconds())) / float64(gs.successCount)

	return standardized, nil
}

// canCollectCellularData checks if cellular data collection is possible
func (gs *GoogleLocationSource) canCollectCellularData(ctx context.Context) bool {
	cmd := exec.CommandContext(ctx, "which", "gsmctl")
	return cmd.Run() == nil
}

// canCollectWiFiData checks if WiFi data collection is possible
func (gs *GoogleLocationSource) canCollectWiFiData(ctx context.Context) bool {
	cmd := exec.CommandContext(ctx, "which", "ubus")
	return cmd.Run() == nil
}

// collectCellularData collects cellular tower information
func (gs *GoogleLocationSource) collectCellularData(ctx context.Context) ([]GoogleCellTower, error) {
	var towers []GoogleCellTower

	// Get serving cell information
	cmd := exec.CommandContext(ctx, "gsmctl", "-A", "AT+QENG=\"servingcell\"")
	output, err := cmd.Output()
	if err != nil {
		return nil, fmt.Errorf("failed to get serving cell info: %w", err)
	}

	lines := strings.Split(string(output), "\n")
	for _, line := range lines {
		if strings.Contains(line, "+QENG:") && strings.Contains(line, "\"LTE\"") {
			tower := gs.parseServingCell(line)
			if tower != nil {
				towers = append(towers, *tower)
			}
		}
	}

	// Get neighbor cells
	cmd = exec.CommandContext(ctx, "gsmctl", "-A", "AT+QENG=\"neighbourcell\"")
	output, err = cmd.Output()
	if err == nil {
		lines = strings.Split(string(output), "\n")
		for _, line := range lines {
			if strings.Contains(line, "+QENG:") {
				tower := gs.parseNeighborCell(line)
				if tower != nil {
					towers = append(towers, *tower)
				}
			}
		}
	}

	return towers, nil
}

// collectWiFiData collects WiFi access point information
func (gs *GoogleLocationSource) collectWiFiData(ctx context.Context) ([]GoogleWifiAccessPoint, error) {
	var accessPoints []GoogleWifiAccessPoint

	cmd := exec.CommandContext(ctx, "ubus", "call", "iwinfo", "scan", `{"device":"wlan0"}`)
	output, err := cmd.Output()
	if err != nil {
		return nil, fmt.Errorf("failed to scan WiFi: %w", err)
	}

	var scanResult struct {
		Results []struct {
			SSID    string `json:"ssid"`
			BSSID   string `json:"bssid"`
			Signal  int    `json:"signal"`
			Channel int    `json:"channel"`
		} `json:"results"`
	}

	if err := json.Unmarshal(output, &scanResult); err != nil {
		return nil, fmt.Errorf("failed to parse WiFi scan results: %w", err)
	}

	for _, result := range scanResult.Results {
		if result.BSSID != "" {
			accessPoints = append(accessPoints, GoogleWifiAccessPoint{
				MacAddress:     result.BSSID,
				SignalStrength: result.Signal,
				Channel:        result.Channel,
			})
		}
	}

	return accessPoints, nil
}

// parseServingCell parses serving cell information from AT command response
func (gs *GoogleLocationSource) parseServingCell(line string) *GoogleCellTower {
	parts := strings.Split(line, ",")
	if len(parts) < 10 {
		return nil
	}

	mcc, err := strconv.Atoi(strings.Trim(parts[4], "\""))
	if err != nil {
		return nil
	}

	mnc, err := strconv.Atoi(strings.Trim(parts[5], "\""))
	if err != nil {
		return nil
	}

	cellIdHex := strings.Trim(parts[6], "\"")
	cellId, err := strconv.ParseInt(cellIdHex, 16, 64)
	if err != nil {
		return nil
	}

	// Check bounds before converting to int
	if cellId < 0 || cellId > int64(^uint(0)>>1) {
		return nil
	}

	lac, err := strconv.Atoi(strings.Trim(parts[7], "\""))
	if err != nil {
		return nil
	}

	signal := -100
	if len(parts) > 12 {
		if rsrp, err := strconv.Atoi(strings.Trim(parts[12], "\"")); err == nil {
			signal = rsrp
		}
	}

	return &GoogleCellTower{
		CellId:            int(cellId),
		LocationAreaCode:  lac,
		MobileCountryCode: mcc,
		MobileNetworkCode: mnc,
		SignalStrength:    signal,
	}
}

// parseNeighborCell parses neighbor cell information
func (gs *GoogleLocationSource) parseNeighborCell(line string) *GoogleCellTower {
	// Similar parsing logic for neighbor cells
	// Implementation would depend on the specific AT command format
	return nil
}

// queryGoogleLocationAPI sends request to Google Geolocation API
func (gs *GoogleLocationSource) queryGoogleLocationAPI(ctx context.Context, cellTowers []GoogleCellTower, wifiAPs []GoogleWifiAccessPoint) (*GoogleLocationResponse, error) {
	url := fmt.Sprintf("https://www.googleapis.com/geolocation/v1/geolocate?key=%s", gs.apiKey)

	request := GoogleLocationRequest{
		ConsiderIp:       true,
		CellTowers:       cellTowers,
		WifiAccessPoints: wifiAPs,
	}

	jsonData, err := json.Marshal(request)
	if err != nil {
		return nil, fmt.Errorf("failed to marshal request: %w", err)
	}

	req, err := http.NewRequestWithContext(ctx, "POST", url, bytes.NewBuffer(jsonData))
	if err != nil {
		return nil, fmt.Errorf("failed to create request: %w", err)
	}

	req.Header.Set("Content-Type", "application/json")

	resp, err := gs.httpClient.Do(req)
	if err != nil {
		apiErr := fmt.Errorf("HTTP request failed: %w", err)
		gs.handleGoogleAPIError(apiErr, "http_request")
		return nil, apiErr
	}
	defer resp.Body.Close()

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		apiErr := fmt.Errorf("failed to read response: %w", err)
		gs.handleGoogleAPIError(apiErr, "response_read")
		return nil, apiErr
	}

	// Check HTTP status code
	if resp.StatusCode != 200 {
		apiErr := fmt.Errorf("Google API HTTP error %d: %s", resp.StatusCode, string(body))
		gs.handleGoogleAPIError(apiErr, "http_status")
		return nil, apiErr
	}

	var response GoogleLocationResponse
	if err := json.Unmarshal(body, &response); err != nil {
		apiErr := fmt.Errorf("failed to parse response: %w", err)
		gs.handleGoogleAPIError(apiErr, "json_parse")
		return nil, apiErr
	}

	if response.Error.Code != 0 {
		apiErr := fmt.Errorf("Google API error %d: %s", response.Error.Code, response.Error.Message)
		gs.handleGoogleAPIError(apiErr, "api_error")
		return nil, apiErr
	}

	return &response, nil
}

// estimateAltitude estimates altitude using Google Maps Elevation API (if enabled) or Open Elevation API
func (gs *GoogleLocationSource) estimateAltitude(ctx context.Context, lat, lng float64) float64 {
	// Strategy 1: Use Google Maps Elevation API if enabled and API key is available
	if gs.useGoogleElevation && gs.apiKey != "" {
		if altitude, err := gs.queryGoogleMapsElevationAPI(ctx, lat, lng); err == nil {
			gs.logger.LogDebugVerbose("elevation_api_success", map[string]interface{}{
				"api":       "google_maps_elevation",
				"latitude":  lat,
				"longitude": lng,
				"altitude":  altitude,
			})
			return altitude
		} else {
			gs.logger.LogDebugVerbose("elevation_api_fallback", map[string]interface{}{
				"api":         "google_maps_elevation",
				"fallback_to": "open_elevation",
				"error":       err.Error(),
				"latitude":    lat,
				"longitude":   lng,
			})
		}
	}

	// Strategy 2: Use Open Elevation API as primary (when Google Elevation not enabled) or fallback
	if altitude, err := gs.queryOpenElevationAPI(ctx, lat, lng); err == nil {
		gs.logger.LogDebugVerbose("elevation_api_success", map[string]interface{}{
			"api":       "open_elevation",
			"latitude":  lat,
			"longitude": lng,
			"altitude":  altitude,
		})
		return altitude
	} else {
		gs.logger.LogDebugVerbose("elevation_api_fallback", map[string]interface{}{
			"api":         "open_elevation",
			"fallback_to": "regional_estimation",
			"error":       err.Error(),
			"latitude":    lat,
			"longitude":   lng,
		})
	}

	// Strategy 3: Fall back to regional estimation
	estimatedAltitude := gs.estimateAltitudeByRegion(lat, lng)
	gs.logger.LogDebugVerbose("elevation_regional_estimation", map[string]interface{}{
		"latitude":  lat,
		"longitude": lng,
		"altitude":  estimatedAltitude,
		"method":    "regional_estimation",
	})
	return estimatedAltitude
}

// queryOpenElevationAPI queries the Open Elevation API for altitude
func (gs *GoogleLocationSource) queryOpenElevationAPI(ctx context.Context, lat, lng float64) (float64, error) {
	url := fmt.Sprintf("https://api.open-elevation.com/api/v1/lookup?locations=%.6f,%.6f", lat, lng)

	req, err := http.NewRequestWithContext(ctx, "GET", url, nil)
	if err != nil {
		return 0, err
	}

	client := &http.Client{Timeout: 15 * time.Second}
	resp, err := client.Do(req)
	if err != nil {
		return 0, err
	}
	defer resp.Body.Close()

	var response OpenElevationResponse
	if err := json.NewDecoder(resp.Body).Decode(&response); err != nil {
		return 0, err
	}

	if len(response.Results) > 0 {
		return response.Results[0].Elevation, nil
	}

	return 0, fmt.Errorf("no elevation data available")
}

// queryGoogleMapsElevationAPI queries the Google Maps Elevation API for altitude
func (gs *GoogleLocationSource) queryGoogleMapsElevationAPI(ctx context.Context, lat, lng float64) (float64, error) {
	url := fmt.Sprintf("https://maps.googleapis.com/maps/api/elevation/json?locations=%.6f,%.6f&key=%s", lat, lng, gs.apiKey)

	req, err := http.NewRequestWithContext(ctx, "GET", url, nil)
	if err != nil {
		return 0, err
	}

	client := &http.Client{Timeout: 15 * time.Second}
	resp, err := client.Do(req)
	if err != nil {
		return 0, err
	}
	defer resp.Body.Close()

	var response GoogleMapsElevationResponse
	if err := json.NewDecoder(resp.Body).Decode(&response); err != nil {
		return 0, err
	}

	if response.Status == "OK" && len(response.Results) > 0 {
		return response.Results[0].Elevation, nil
	}

	return 0, fmt.Errorf("Google Maps Elevation API failed or no data available")
}

// estimateAltitudeByRegion provides rough altitude estimation based on geographic region
func (gs *GoogleLocationSource) estimateAltitudeByRegion(lat, lng float64) float64 {
	// Stockholm area (where the RUTX50 is typically located)
	if lat >= 59.0 && lat <= 60.0 && lng >= 17.0 && lng <= 19.0 {
		return 25.0 // Average elevation in Stockholm area
	}

	// Europe general
	if lat >= 35.0 && lat <= 70.0 && lng >= -10.0 && lng <= 40.0 {
		return 200.0 // Average European elevation
	}

	// Default sea level
	return 0.0
}

// Helper functions for quality assessment
func (gs *GoogleLocationSource) determineQualityFromAccuracy(accuracy float64) string {
	if accuracy <= 50 {
		return "good"
	} else if accuracy <= 200 {
		return "fair"
	}
	return "poor"
}

func (gs *GoogleLocationSource) calculateConfidence(accuracy float64, cellCount, wifiCount int) float64 {
	// Base confidence on accuracy
	accuracyScore := math.Max(0, 1.0-(accuracy/1000.0)) // 0-1 scale, 1000m = 0 confidence

	// Bonus for more data sources
	dataScore := math.Min(1.0, float64(cellCount+wifiCount)/10.0) // 0-1 scale, 10+ sources = full bonus

	return (accuracyScore + dataScore) / 2.0
}

func (gs *GoogleLocationSource) buildDataSources(cellTowers []GoogleCellTower, wifiAPs []GoogleWifiAccessPoint) []string {
	var sources []string

	if len(cellTowers) > 0 {
		sources = append(sources, fmt.Sprintf("cellular_%d_towers", len(cellTowers)))
	}
	if len(wifiAPs) > 0 {
		sources = append(sources, fmt.Sprintf("wifi_%d_aps", len(wifiAPs)))
	}

	return sources
}

// handleGoogleAPIError handles Google API errors and sends notifications if needed
func (gs *GoogleLocationSource) handleGoogleAPIError(err error, context string) {
	gs.errorCount++
	gs.health.LastError = err.Error()

	// Log the error
	gs.logger.Error("Google Location API error",
		"context", context,
		"error", err.Error(),
		"error_count", gs.errorCount,
	)

	// Send notification for critical errors (every 10 errors or quota issues)
	shouldNotify := false
	notificationTitle := "GPS Google API Error"
	notificationMessage := ""

	if strings.Contains(err.Error(), "quota") || strings.Contains(err.Error(), "QUOTA") {
		shouldNotify = true
		notificationTitle = "GPS Google API Quota Exceeded"
		notificationMessage = fmt.Sprintf("Google Location API quota exceeded. Error: %s", err.Error())
	} else if strings.Contains(err.Error(), "API key") || strings.Contains(err.Error(), "INVALID_KEY") {
		shouldNotify = true
		notificationTitle = "GPS Google API Key Invalid"
		notificationMessage = fmt.Sprintf("Google Location API key is invalid. Error: %s", err.Error())
	} else if gs.errorCount%10 == 0 { // Every 10 errors
		shouldNotify = true
		notificationMessage = fmt.Sprintf("Google Location API has failed %d times. Latest error: %s", gs.errorCount, err.Error())
	}

	// Rate limit notifications (max once per hour)
	if shouldNotify && time.Since(gs.lastNotification) > time.Hour {
		gs.sendNotification(notificationTitle, notificationMessage)
		gs.lastNotification = time.Now()
	}
}

// sendNotification sends a Pushover notification (simplified implementation)
func (gs *GoogleLocationSource) sendNotification(title, message string) {
	// Create a simple notification using system logger
	gs.logger.Warn("GPS_API_NOTIFICATION",
		"title", title,
		"message", message,
		"component", "google_location_api",
		"notification_type", "pushover_recommended",
	)

	// TODO: Integrate with proper notification system when available
	// For now, this creates a log entry that can be monitored
}
