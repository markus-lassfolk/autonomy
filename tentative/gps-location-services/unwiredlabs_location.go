package main

import (
	"bytes"
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"os"
	"sort"
	"strconv"
	"strings"
	"time"

	"golang.org/x/crypto/ssh"
)

// UnwiredLabsLocationAPI provides comprehensive geolocation services
type UnwiredLabsLocationAPI struct {
	apiKey  string
	baseURL string // Region-specific URL
	timeout time.Duration
	client  *http.Client
}

// LocationRequest represents the UnwiredLabs LocationAPI request structure
type LocationRequest struct {
	Token     string      `json:"token"`
	Radio     string      `json:"radio,omitempty"`     // "gsm", "cdma", "umts", "lte"
	MCC       int         `json:"mcc,omitempty"`       // Mobile Country Code
	MNC       int         `json:"mnc,omitempty"`       // Mobile Network Code
	Cells     []CellTower `json:"cells,omitempty"`     // Up to 7 cell towers
	WiFi      []WiFiAP    `json:"wifi,omitempty"`      // 2-15 WiFi access points
	Address   int         `json:"address,omitempty"`   // Include address in response (0/1)
	Fallbacks []string    `json:"fallbacks,omitempty"` // ["lacf", "scf", "ipf"]
}

// CellTower represents a single cell tower for location request
type CellTower struct {
	// Common fields for all radio types
	CID int `json:"cid"`           // Cell ID (required)
	LAC int `json:"lac,omitempty"` // Location Area Code

	// LTE specific fields
	TAC    int `json:"tac,omitempty"`    // Tracking Area Code (LTE)
	PCI    int `json:"pci,omitempty"`    // Physical Cell ID (LTE)
	EARFCN int `json:"earfcn,omitempty"` // E-UTRA Absolute Radio Frequency Channel Number

	// UMTS specific fields
	UC     int `json:"uc,omitempty"`     // UMTS Cell ID
	PSC    int `json:"psc,omitempty"`    // Primary Scrambling Code
	UARFCN int `json:"uarfcn,omitempty"` // UTRA Absolute Radio Frequency Channel Number

	// CDMA specific fields
	SID int `json:"sid,omitempty"` // System ID
	NID int `json:"nid,omitempty"` // Network ID
	BID int `json:"bid,omitempty"` // Base Station ID

	// Signal strength (optional for all types)
	Signal int `json:"signal,omitempty"` // Signal strength in dBm
	TA     int `json:"ta,omitempty"`     // Timing Advance
}

// WiFiAP represents a WiFi Access Point for location request
type WiFiAP struct {
	BSSID   string `json:"bssid"`             // MAC address (required)
	Channel int    `json:"channel,omitempty"` // WiFi channel
	Signal  int    `json:"signal,omitempty"`  // Signal strength in dBm
	SSID    string `json:"ssid,omitempty"`    // Network name (optional)
}

// LocationResponse represents the UnwiredLabs LocationAPI response
type LocationResponse struct {
	Status   string  `json:"status"`             // "ok" or "error"
	Message  string  `json:"message,omitempty"`  // Error message if status is "error"
	Balance  int     `json:"balance,omitempty"`  // Remaining API credits
	Lat      float64 `json:"lat,omitempty"`      // Latitude
	Lon      float64 `json:"lon,omitempty"`      // Longitude
	Accuracy int     `json:"accuracy,omitempty"` // Accuracy in meters
	Address  string  `json:"address,omitempty"`  // Human-readable address
	Fallback string  `json:"fallback,omitempty"` // Fallback method used
}

// BalanceResponse represents the balance API response
type BalanceResponse struct {
	Status  string `json:"status"`            // "ok" or "error"
	Message string `json:"message,omitempty"` // Error message if status is "error"
	Balance int    `json:"balance"`           // Remaining API credits
}

// ErrorSchema represents detailed error information
type ErrorSchema struct {
	Code        int    `json:"code"`
	Message     string `json:"message"`
	Description string `json:"description"`
	Suggestion  string `json:"suggestion,omitempty"`
}

// Common error codes from UnwiredLabs API
var UnwiredLabsErrors = map[int]ErrorSchema{
	200: {200, "OK", "Request successful", ""},
	400: {400, "Bad Request", "Invalid request format or parameters", "Check request structure and parameters"},
	401: {401, "Unauthorized", "Invalid or missing API token", "Verify your API token"},
	402: {402, "Payment Required", "Insufficient credits", "Add credits to your account"},
	403: {403, "Forbidden", "Access denied or rate limited", "Check rate limits and permissions"},
	404: {404, "Not Found", "Location not found", "Try with more cell towers or WiFi access points"},
	429: {429, "Too Many Requests", "Rate limit exceeded", "Implement request throttling"},
	500: {500, "Internal Server Error", "Server error", "Try again later or contact support"},
	503: {503, "Service Unavailable", "Service temporarily unavailable", "Try again later"},
}

// Regional endpoints for UnwiredLabs LocationAPI
var UnwiredLabsRegions = map[string]string{
	"us1": "https://us1.unwiredlabs.com/v2", // US East
	"us2": "https://us2.unwiredlabs.com/v2", // US West
	"eu1": "https://eu1.unwiredlabs.com/v2", // Europe (France)
	"ap1": "https://ap1.unwiredlabs.com/v2", // Asia Pacific
}

// NewUnwiredLabsLocationAPI creates a new UnwiredLabs LocationAPI client
func NewUnwiredLabsLocationAPI(apiKey, region string) *UnwiredLabsLocationAPI {
	baseURL, exists := UnwiredLabsRegions[region]
	if !exists {
		baseURL = UnwiredLabsRegions["eu1"] // Default to Europe
	}

	return &UnwiredLabsLocationAPI{
		apiKey:  apiKey,
		baseURL: baseURL,
		timeout: 30 * time.Second,
		client: &http.Client{
			Timeout: 30 * time.Second,
		},
	}
}

// LoadUnwiredLabsToken loads the API token from file
func LoadUnwiredLabsToken() (string, error) {
	tokenPath := "C:\\Users\\markusla\\OneDrive\\IT\\RUTOS Keys\\UniWiredLabs.txt"

	data, err := os.ReadFile(tokenPath)
	if err != nil {
		return "", fmt.Errorf("failed to read token file: %w", err)
	}

	token := strings.TrimSpace(string(data))
	if token == "" {
		return "", fmt.Errorf("token file is empty")
	}

	return token, nil
}

// GetBalance checks the remaining API credits
func (api *UnwiredLabsLocationAPI) GetBalance() (*BalanceResponse, error) {
	url := fmt.Sprintf("%s/balance?token=%s", api.baseURL, api.apiKey)

	resp, err := api.client.Get(url)
	if err != nil {
		return nil, fmt.Errorf("balance request failed: %w", err)
	}
	defer resp.Body.Close()

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, fmt.Errorf("failed to read balance response: %w", err)
	}

	var balance BalanceResponse
	if err := json.Unmarshal(body, &balance); err != nil {
		return nil, fmt.Errorf("failed to parse balance response: %w", err)
	}

	return &balance, nil
}

// GetLocation performs geolocation using cell towers and WiFi access points
func (api *UnwiredLabsLocationAPI) GetLocation(request *LocationRequest) (*LocationResponse, error) {
	// Set the API token
	request.Token = api.apiKey

	// Serialize request to JSON
	jsonData, err := json.Marshal(request)
	if err != nil {
		return nil, fmt.Errorf("failed to marshal request: %w", err)
	}

	// Make POST request to location API
	url := fmt.Sprintf("%s/process", api.baseURL)
	resp, err := api.client.Post(url, "application/json", bytes.NewBuffer(jsonData))
	if err != nil {
		return nil, fmt.Errorf("location request failed: %w", err)
	}
	defer resp.Body.Close()

	// Read response
	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, fmt.Errorf("failed to read location response: %w", err)
	}

	// Parse response
	var locationResp LocationResponse
	if err := json.Unmarshal(body, &locationResp); err != nil {
		return nil, fmt.Errorf("failed to parse location response: %w", err)
	}

	// Handle API errors
	if locationResp.Status == "error" {
		errorInfo := api.getErrorInfo(resp.StatusCode, locationResp.Message)
		return &locationResp, fmt.Errorf("API error (%d): %s - %s",
			errorInfo.Code, errorInfo.Message, errorInfo.Description)
	}

	return &locationResp, nil
}

// getErrorInfo returns detailed error information
func (api *UnwiredLabsLocationAPI) getErrorInfo(statusCode int, message string) ErrorSchema {
	if errorInfo, exists := UnwiredLabsErrors[statusCode]; exists {
		errorInfo.Message = message // Override with actual API message
		return errorInfo
	}

	return ErrorSchema{
		Code:        statusCode,
		Message:     message,
		Description: "Unknown error",
		Suggestion:  "Check API documentation",
	}
}

// BuildCellTowersFromIntelligence converts cellular intelligence to UnwiredLabs format
func BuildCellTowersFromIntelligence(intel *CellularLocationIntelligence, maxCells int) ([]CellTower, string, error) {
	var cells []CellTower
	var radioType string

	// Parse serving cell information
	servingCellID, err := strconv.Atoi(intel.ServingCell.CellID)
	if err != nil {
		return nil, "", fmt.Errorf("invalid serving cell ID: %s", intel.ServingCell.CellID)
	}

	_, err = strconv.Atoi(intel.ServingCell.MCC)
	if err != nil {
		return nil, "", fmt.Errorf("invalid MCC: %s", intel.ServingCell.MCC)
	}

	_, err = strconv.Atoi(intel.ServingCell.MNC)
	if err != nil {
		return nil, "", fmt.Errorf("invalid MNC: %s", intel.ServingCell.MNC)
	}

	tac, err := strconv.Atoi(intel.ServingCell.TAC)
	if err != nil {
		return nil, "", fmt.Errorf("invalid TAC: %s", intel.ServingCell.TAC)
	}

	// Determine radio type from band information
	radioType = determineRadioType(intel.ServingCell.Band, intel.NetworkInfo.Technology)

	// Build serving cell (must be first)
	servingCell := CellTower{
		CID:    servingCellID,
		Signal: intel.SignalQuality.RSRP,
	}

	// Add radio-specific fields
	switch radioType {
	case "lte":
		servingCell.TAC = tac
		servingCell.PCI = intel.ServingCell.PCID
		servingCell.EARFCN = intel.ServingCell.EARFCN
	case "umts":
		servingCell.LAC = tac // Use TAC as LAC for UMTS
		servingCell.UC = servingCellID
		servingCell.PSC = intel.ServingCell.PCID
	case "gsm":
		servingCell.LAC = tac
	}

	cells = append(cells, servingCell)

	// Sort neighbor cells by signal strength (strongest first)
	neighbors := make([]NeighborCellInfo, len(intel.NeighborCells))
	copy(neighbors, intel.NeighborCells)
	sort.Slice(neighbors, func(i, j int) bool {
		return neighbors[i].RSRP > neighbors[j].RSRP
	})

	// Add up to maxCells-1 neighbor cells (serving cell already added)
	maxNeighbors := maxCells - 1
	if maxNeighbors > len(neighbors) {
		maxNeighbors = len(neighbors)
	}

	for i := 0; i < maxNeighbors; i++ {
		neighbor := neighbors[i]

		// For neighbors, we don't have actual Cell IDs, so we'll use PCID
		// This might not work perfectly, but it's the best we can do
		neighborCell := CellTower{
			CID:    neighbor.PCID, // Using PCID as Cell ID
			Signal: neighbor.RSRP,
		}

		// Add radio-specific fields for neighbors
		switch radioType {
		case "lte":
			neighborCell.PCI = neighbor.PCID
			neighborCell.EARFCN = neighbor.EARFCN
			// We don't have TAC for neighbors, use serving cell's TAC
			neighborCell.TAC = tac
		case "umts":
			neighborCell.PSC = neighbor.PCID
			neighborCell.UARFCN = neighbor.EARFCN
			neighborCell.LAC = tac
		case "gsm":
			neighborCell.LAC = tac
		}

		cells = append(cells, neighborCell)
	}

	return cells, radioType, nil
}

// determineRadioType determines the radio type from band and technology information
func determineRadioType(band, technology string) string {
	// Convert to lowercase for comparison
	band = strings.ToLower(band)
	technology = strings.ToLower(technology)

	// Check for LTE indicators
	if strings.Contains(band, "lte") || strings.Contains(band, "b") ||
		strings.Contains(technology, "lte") || strings.Contains(technology, "5g") {
		return "lte"
	}

	// Check for UMTS indicators
	if strings.Contains(technology, "umts") || strings.Contains(technology, "3g") ||
		strings.Contains(band, "wcdma") {
		return "umts"
	}

	// Check for GSM indicators
	if strings.Contains(technology, "gsm") || strings.Contains(technology, "2g") {
		return "gsm"
	}

	// Default to LTE for modern networks
	return "lte"
}

// CollectWiFiAccessPoints collects WiFi access points from RutOS
func CollectWiFiAccessPoints(client *ssh.Client, maxAPs int) ([]WiFiAP, error) {
	var wifiAPs []WiFiAP

	// Get WiFi scan results
	fmt.Println("📶 Scanning for WiFi access points...")

	// Command to scan for WiFi networks
	scanCmd := "iwlist scan 2>/dev/null | grep -E '(Address|ESSID|Signal|Channel)' | head -50"
	output, err := executeCommand(client, scanCmd)
	if err != nil {
		// Try alternative command
		scanCmd = "iw dev wlan0 scan 2>/dev/null | grep -E '(BSS|SSID|signal|freq)' | head -50"
		output, err = executeCommand(client, scanCmd)
		if err != nil {
			return nil, fmt.Errorf("failed to scan WiFi: %w", err)
		}
	}

	// Parse WiFi scan results
	lines := strings.Split(output, "\n")
	var currentAP *WiFiAP

	for _, line := range lines {
		line = strings.TrimSpace(line)

		// Parse BSSID (MAC address)
		if strings.Contains(line, "Address:") || strings.Contains(line, "BSS ") {
			if currentAP != nil && currentAP.BSSID != "" {
				wifiAPs = append(wifiAPs, *currentAP)
			}
			currentAP = &WiFiAP{}

			// Extract BSSID
			if strings.Contains(line, "Address:") {
				parts := strings.Split(line, "Address:")
				if len(parts) > 1 {
					currentAP.BSSID = strings.TrimSpace(parts[1])
				}
			} else if strings.Contains(line, "BSS ") {
				parts := strings.Fields(line)
				if len(parts) > 1 {
					currentAP.BSSID = strings.TrimSpace(parts[1])
				}
			}
		}

		// Parse SSID
		if strings.Contains(line, "ESSID:") || strings.Contains(line, "SSID:") {
			if currentAP != nil {
				if strings.Contains(line, "ESSID:") {
					parts := strings.Split(line, "ESSID:")
					if len(parts) > 1 {
						ssid := strings.Trim(strings.TrimSpace(parts[1]), "\"")
						if ssid != "" && ssid != "<hidden>" {
							currentAP.SSID = ssid
						}
					}
				} else if strings.Contains(line, "SSID:") {
					parts := strings.Split(line, "SSID:")
					if len(parts) > 1 {
						ssid := strings.TrimSpace(parts[1])
						if ssid != "" && ssid != "<hidden>" {
							currentAP.SSID = ssid
						}
					}
				}
			}
		}

		// Parse signal strength
		if strings.Contains(line, "Signal level") || strings.Contains(line, "signal:") {
			if currentAP != nil {
				if strings.Contains(line, "Signal level") {
					// Format: "Signal level=-45 dBm"
					parts := strings.Split(line, "=")
					if len(parts) > 1 {
						signalStr := strings.Fields(parts[1])[0]
						if signal, err := strconv.Atoi(signalStr); err == nil {
							currentAP.Signal = signal
						}
					}
				} else if strings.Contains(line, "signal:") {
					// Format: "signal: -45.00 dBm"
					parts := strings.Split(line, "signal:")
					if len(parts) > 1 {
						signalStr := strings.Fields(strings.TrimSpace(parts[1]))[0]
						if signal, err := strconv.ParseFloat(signalStr, 64); err == nil {
							currentAP.Signal = int(signal)
						}
					}
				}
			}
		}

		// Parse channel/frequency
		if strings.Contains(line, "Channel:") || strings.Contains(line, "freq:") {
			if currentAP != nil {
				if strings.Contains(line, "Channel:") {
					parts := strings.Split(line, "Channel:")
					if len(parts) > 1 {
						channelStr := strings.TrimSpace(parts[1])
						if channel, err := strconv.Atoi(channelStr); err == nil {
							currentAP.Channel = channel
						}
					}
				} else if strings.Contains(line, "freq:") {
					// Convert frequency to channel (approximate)
					parts := strings.Split(line, "freq:")
					if len(parts) > 1 {
						freqStr := strings.TrimSpace(parts[1])
						if freq, err := strconv.Atoi(freqStr); err == nil {
							// Rough conversion from frequency to channel
							if freq >= 2412 && freq <= 2484 {
								currentAP.Channel = (freq-2412)/5 + 1
							} else if freq >= 5170 && freq <= 5825 {
								currentAP.Channel = (freq - 5000) / 5
							}
						}
					}
				}
			}
		}
	}

	// Add the last AP if valid
	if currentAP != nil && currentAP.BSSID != "" {
		wifiAPs = append(wifiAPs, *currentAP)
	}

	// Sort by signal strength (strongest first) and limit to maxAPs
	sort.Slice(wifiAPs, func(i, j int) bool {
		return wifiAPs[i].Signal > wifiAPs[j].Signal
	})

	if len(wifiAPs) > maxAPs {
		wifiAPs = wifiAPs[:maxAPs]
	}

	fmt.Printf("📶 Found %d WiFi access points\n", len(wifiAPs))

	return wifiAPs, nil
}

// GetLocationWithUnwiredLabs performs comprehensive geolocation using UnwiredLabs
func GetLocationWithUnwiredLabs(client *ssh.Client, region string) (*LocationResponse, error) {
	start := time.Now()

	// Load API token
	apiKey, err := LoadUnwiredLabsToken()
	if err != nil {
		return nil, fmt.Errorf("failed to load UnwiredLabs token: %w", err)
	}

	// Create API client
	api := NewUnwiredLabsLocationAPI(apiKey, region)

	// Check balance first
	fmt.Println("💰 Checking API balance...")
	balance, err := api.GetBalance()
	if err != nil {
		fmt.Printf("⚠️  Warning: Could not check balance: %v\n", err)
	} else {
		fmt.Printf("💰 Remaining credits: %d\n", balance.Balance)
		if balance.Balance < 10 {
			return nil, fmt.Errorf("insufficient credits: %d remaining", balance.Balance)
		}
	}

	// Collect cellular intelligence
	fmt.Println("🗼 Collecting cellular intelligence...")
	intel, err := collectCellularLocationIntelligence(client)
	if err != nil {
		return nil, fmt.Errorf("failed to collect cellular data: %w", err)
	}

	// Build cell towers (up to 7)
	cells, radioType, err := BuildCellTowersFromIntelligence(intel, 7)
	if err != nil {
		return nil, fmt.Errorf("failed to build cell towers: %w", err)
	}

	// Collect WiFi access points (2-15)
	fmt.Println("📶 Collecting WiFi access points...")
	wifiAPs, err := CollectWiFiAccessPoints(client, 15)
	if err != nil {
		fmt.Printf("⚠️  Warning: Could not collect WiFi data: %v\n", err)
		wifiAPs = []WiFiAP{} // Continue without WiFi
	}

	// Build location request
	request := &LocationRequest{
		Radio:     radioType,
		MCC:       240, // Sweden
		MNC:       1,   // Telia
		Cells:     cells,
		WiFi:      wifiAPs,
		Address:   1,                       // Include address in response
		Fallbacks: []string{"lacf", "scf"}, // Location Area Code fallback, Serving Cell fallback
	}

	// Print request summary
	fmt.Printf("📡 UnwiredLabs Request Summary:\n")
	fmt.Printf("  🗼 Radio Type: %s\n", radioType)
	fmt.Printf("  📱 Cell Towers: %d (serving + %d neighbors)\n", len(cells), len(cells)-1)
	fmt.Printf("  📶 WiFi APs: %d\n", len(wifiAPs))
	fmt.Printf("  🌍 Region: %s (%s)\n", region, api.baseURL)

	// Make location request
	fmt.Println("🎯 Requesting location...")
	response, err := api.GetLocation(request)
	if err != nil {
		return nil, fmt.Errorf("location request failed: %w", err)
	}

	// Print results
	fmt.Printf("✅ Location Response:\n")
	fmt.Printf("  📍 Coordinates: %.6f°, %.6f°\n", response.Lat, response.Lon)
	fmt.Printf("  🎯 Accuracy: ±%d meters\n", response.Accuracy)
	if response.Address != "" {
		fmt.Printf("  🏠 Address: %s\n", response.Address)
	}
	if response.Fallback != "" {
		fmt.Printf("  🔄 Fallback Used: %s\n", response.Fallback)
	}
	fmt.Printf("  💰 Remaining Credits: %d\n", response.Balance)
	fmt.Printf("  ⏱️  Response Time: %.1f ms\n", float64(time.Since(start).Nanoseconds())/1e6)

	return response, nil
}

// testUnwiredLabsLocation demonstrates the UnwiredLabs LocationAPI integration
func testUnwiredLabsLocation() error {
	fmt.Println("🚀 TESTING UNWIREDLABS LOCATIONAPI")
	fmt.Println("=" + strings.Repeat("=", 35))

	// Test with different regions
	regions := []string{"eu1", "us1", "ap1"}

	for _, region := range regions {
		fmt.Printf("\n🌍 Testing region: %s\n", region)
		fmt.Println("-" + strings.Repeat("-", 25))

		// Load API token
		apiKey, err := LoadUnwiredLabsToken()
		if err != nil {
			fmt.Printf("❌ Failed to load token: %v\n", err)
			continue
		}

		// Create API client
		api := NewUnwiredLabsLocationAPI(apiKey, region)

		// Check balance
		balance, err := api.GetBalance()
		if err != nil {
			fmt.Printf("❌ Balance check failed: %v\n", err)
			continue
		}

		fmt.Printf("💰 Balance for %s: %d credits\n", region, balance.Balance)

		// Only test location with EU region to save credits
		if region == "eu1" {
			// Create test request with hardcoded data
			request := &LocationRequest{
				Radio: "lte",
				MCC:   240,
				MNC:   1,
				Cells: []CellTower{
					{
						CID:    25939743,
						TAC:    23,
						PCI:    443,
						EARFCN: 1300,
						Signal: -84,
					},
				},
				Address:   1,
				Fallbacks: []string{"lacf", "scf"},
			}

			fmt.Println("🎯 Testing location request...")
			response, err := api.GetLocation(request)
			if err != nil {
				fmt.Printf("❌ Location request failed: %v\n", err)
			} else {
				fmt.Printf("✅ Location: %.6f°, %.6f° (±%dm)\n",
					response.Lat, response.Lon, response.Accuracy)
				if response.Address != "" {
					fmt.Printf("🏠 Address: %s\n", response.Address)
				}
			}
		}
	}

	fmt.Println("\n✅ UnwiredLabs LocationAPI Test Complete!")
	return nil
}
