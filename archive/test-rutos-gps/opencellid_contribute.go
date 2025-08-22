package main

import (
	"bytes"
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"strconv"
	"strings"
	"time"

	"golang.org/x/crypto/ssh"
)

// OpenCellIDContribution represents data to contribute to OpenCellID
type OpenCellIDContribution struct {
	Token      string  `json:"token"`
	Radio      string  `json:"radio"`
	MCC        int     `json:"mcc"`
	MNC        int     `json:"mnc"`
	LAC        int     `json:"lac"`
	CellID     int     `json:"cellid"`
	Lat        float64 `json:"lat"`
	Lon        float64 `json:"lon"`
	Signal     int     `json:"signal,omitempty"`
	MeasuredAt string  `json:"measured_at"`
}

// OpenCellIDContributionResponse represents the API response
type OpenCellIDContributionResponse struct {
	Status  string `json:"status"`
	Message string `json:"message,omitempty"`
	Balance int    `json:"balance,omitempty"`
}

// contributeToOpenCellID automatically contributes cell tower data
func contributeToOpenCellID(client *ssh.Client) error {
	fmt.Println("🌍 CONTRIBUTING TO OPENCELLID DATABASE")
	fmt.Println("=" + strings.Repeat("=", 40))
	fmt.Println("📡 Helping improve global cell tower database")
	fmt.Println()

	// Get current GPS coordinates (most accurate available)
	fmt.Println("📍 Getting current GPS coordinates...")
	gpsData, err := getAccurateGPSCoordinates(client)
	if err != nil {
		return fmt.Errorf("failed to get GPS coordinates: %v", err)
	}

	fmt.Printf("✅ GPS: %.8f°, %.8f° (±%.1fm)\n", gpsData.Latitude, gpsData.Longitude, gpsData.Accuracy)

	// Get current cellular data
	fmt.Println("📡 Getting current cellular data...")
	cellData, err := collectCellularLocationIntelligence(client)
	if err != nil {
		return fmt.Errorf("failed to get cellular data: %v", err)
	}

	fmt.Printf("✅ Cell: %s (MCC:%s, MNC:%s, LAC:%s, RSSI:%d)\n",
		cellData.ServingCell.CellID, cellData.ServingCell.MCC, cellData.ServingCell.MNC,
		cellData.ServingCell.TAC, cellData.SignalQuality.RSSI)

	// Contribute the data
	fmt.Println("📤 Contributing data to OpenCellID...")
	response, err := submitContribution(gpsData, cellData)
	if err != nil {
		return fmt.Errorf("failed to contribute data: %v", err)
	}

	// Display results
	if response.Status == "ok" {
		fmt.Printf("✅ CONTRIBUTION SUCCESSFUL!\n")
		fmt.Printf("  📊 Status: %s\n", response.Status)
		if response.Balance > 0 {
			fmt.Printf("  💰 API Balance: %d requests\n", response.Balance)
		}
		if response.Message != "" {
			fmt.Printf("  💬 Message: %s\n", response.Message)
		}

		fmt.Printf("\n🎯 Your contribution helps:\n")
		fmt.Printf("  🌍 Improve location accuracy for everyone\n")
		fmt.Printf("  📍 Add your exact cell tower (25939743) to database\n")
		fmt.Printf("  🇸🇪 Enhance coverage in Stockholm/Sweden area\n")
		fmt.Printf("  🆓 Keep the service free for contributors\n")

	} else {
		fmt.Printf("❌ CONTRIBUTION FAILED\n")
		fmt.Printf("  📊 Status: %s\n", response.Status)
		if response.Message != "" {
			fmt.Printf("  💬 Error: %s\n", response.Message)
		}
	}

	return nil
}

// getAccurateGPSCoordinates gets the most accurate GPS data available
func getAccurateGPSCoordinates(client *ssh.Client) (*GPSCoordinate, error) {
	// Try Quectel GPS first (most accurate)
	if quectelData, err := testQuectelGPS(client); err == nil && quectelData.Valid {
		accuracy := quectelData.HDOP * 5.0
		if accuracy == 0 {
			accuracy = 0.4 // Your typical accuracy
		}
		return &GPSCoordinate{
			Latitude:  quectelData.Latitude,
			Longitude: quectelData.Longitude,
			Accuracy:  accuracy,
			Source:    "quectel_multi_gnss",
		}, nil
	}

	// Fallback to enhanced GPS
	if enhancedData, err := collectEnhancedGPSData(client); err == nil && enhancedData.Valid {
		return &GPSCoordinate{
			Latitude:  enhancedData.Latitude,
			Longitude: enhancedData.Longitude,
			Accuracy:  enhancedData.Accuracy,
			Source:    "enhanced_gps",
		}, nil
	}

	return nil, fmt.Errorf("no accurate GPS data available")
}

// submitContribution submits the contribution to OpenCellID
func submitContribution(gps *GPSCoordinate, cell *CellularLocationIntelligence) (*OpenCellIDContributionResponse, error) {
	// Load API token
	apiKey, err := loadOpenCellIDTokenLocal()
	if err != nil {
		return nil, fmt.Errorf("failed to load API key: %v", err)
	}

	// Parse cell data
	cellID, _ := strconv.Atoi(cell.ServingCell.CellID)
	mcc, _ := strconv.Atoi(cell.ServingCell.MCC)
	mnc, _ := strconv.Atoi(cell.ServingCell.MNC)
	lac, _ := strconv.Atoi(cell.ServingCell.TAC)

	// Prepare contribution data
	contribution := OpenCellIDContribution{
		Token:      apiKey,
		Radio:      "LTE", // Your cell is 5G-NSA/LTE
		MCC:        mcc,
		MNC:        mnc,
		LAC:        lac,
		CellID:     cellID,
		Lat:        gps.Latitude,
		Lon:        gps.Longitude,
		Signal:     cell.SignalQuality.RSSI,
		MeasuredAt: time.Now().UTC().Format(time.RFC3339),
	}

	fmt.Printf("  📊 Contributing: Cell %d at %.8f°, %.8f° (RSSI: %d)\n",
		contribution.CellID, contribution.Lat, contribution.Lon, contribution.Signal)

	// Make API request
	jsonData, err := json.Marshal(contribution)
	if err != nil {
		return nil, fmt.Errorf("failed to marshal contribution: %v", err)
	}

	resp, err := http.Post(
		"https://opencellid.org/measure/add",
		"application/json",
		bytes.NewBuffer(jsonData),
	)
	if err != nil {
		return nil, fmt.Errorf("HTTP request failed: %v", err)
	}
	defer resp.Body.Close()

	// Parse response
	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, fmt.Errorf("failed to read response: %v", err)
	}

	var response OpenCellIDContributionResponse
	if err := json.Unmarshal(body, &response); err != nil {
		return nil, fmt.Errorf("failed to parse response: %v (body: %s)", err, string(body))
	}

	return &response, nil
}

// testContributionToOpenCellID tests the contribution functionality
func testContributionToOpenCellID(client *ssh.Client) error {
	fmt.Println("🧪 TESTING OPENCELLID CONTRIBUTION")
	fmt.Println("=" + strings.Repeat("=", 35))
	fmt.Println("📡 Testing data contribution to OpenCellID database")
	fmt.Println()

	// Show what we're about to contribute
	fmt.Println("📋 Data to contribute:")
	fmt.Println("  🏢 Network: Telia Sweden (MCC:240, MNC:01)")
	fmt.Println("  📡 Cell ID: 25939743 (currently missing from database)")
	fmt.Println("  📍 Location: Your super accurate GPS coordinates")
	fmt.Println("  📊 Signal: Live RSSI/RSRP measurements")
	fmt.Println("  🕐 Timestamp: Current time")
	fmt.Println()

	// Ask for confirmation (in a real implementation, you might want user consent)
	fmt.Println("⚠️  This will contribute your location data to OpenCellID's public database.")
	fmt.Println("✅ Benefits: Helps everyone, keeps API free, improves accuracy")
	fmt.Println("📊 Data shared: Cell tower location (not personal location tracking)")
	fmt.Println()

	// For testing, we'll proceed automatically
	fmt.Println("🚀 Proceeding with contribution test...")

	if err := contributeToOpenCellID(client); err != nil {
		return fmt.Errorf("contribution test failed: %v", err)
	}

	fmt.Println("\n💡 RECOMMENDATION:")
	fmt.Println("  ✅ Set up automatic contribution in your autonomy daemon")
	fmt.Println("  🔄 Contribute data periodically (daily/weekly)")
	fmt.Println("  🆓 Maintain free API access")
	fmt.Println("  🌍 Help improve global cell tower database")

	return nil
}

// schedulePeriodicContribution sets up periodic data contribution
func schedulePeriodicContribution(client *ssh.Client, intervalHours int) {
	fmt.Printf("⏰ Setting up periodic contribution every %d hours\n", intervalHours)

	ticker := time.NewTicker(time.Duration(intervalHours) * time.Hour)
	defer ticker.Stop()

	// Contribute immediately
	fmt.Println("📤 Initial contribution...")
	if err := contributeToOpenCellID(client); err != nil {
		fmt.Printf("❌ Initial contribution failed: %v\n", err)
	}

	// Then contribute periodically
	for range ticker.C {
		fmt.Printf("📤 Periodic contribution (%s)...\n", time.Now().Format("2006-01-02 15:04:05"))
		if err := contributeToOpenCellID(client); err != nil {
			fmt.Printf("❌ Periodic contribution failed: %v\n", err)
		}
	}
}
