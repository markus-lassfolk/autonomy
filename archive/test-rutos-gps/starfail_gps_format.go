package main

import (
	"encoding/json"
	"fmt"
	"time"
)

// autonomyGPSData represents the standardized GPS format for autonomy daemon
type autonomyGPSData struct {
	// Core GPS Data
	Latitude   float64 `json:"latitude"`   // Decimal degrees
	Longitude  float64 `json:"longitude"`  // Decimal degrees
	Altitude   float64 `json:"altitude"`   // Meters above sea level
	Accuracy   float64 `json:"accuracy"`   // Accuracy in meters (or HDOP)
	Speed      float64 `json:"speed"`      // Speed in km/h
	Course     float64 `json:"course"`     // Course/bearing in degrees
	Satellites int     `json:"satellites"` // Number of satellites

	// Quality Indicators
	Valid      bool   `json:"valid"`       // GPS fix is valid
	FixType    string `json:"fix_type"`    // "no_fix", "2d", "3d", "dgps"
	FixQuality int    `json:"fix_quality"` // 0=invalid, 1=gps, 2=dgps, etc.

	// Source Information
	Source   string `json:"source"`   // "quectel_gnss", "starlink", "basic_gps"
	Priority int    `json:"priority"` // 1=primary, 2=secondary, 3=tertiary
	Method   string `json:"method"`   // "AT+QGPSLOC=2", "starlink_api", "gpsctl"

	// Timing
	Timestamp int64   `json:"timestamp"`   // Unix timestamp
	DateTime  string  `json:"datetime"`    // Human-readable time
	Age       float64 `json:"age_seconds"` // Age of GPS fix in seconds

	// Additional Metadata
	HDOP float64 `json:"hdop,omitempty"` // Horizontal dilution of precision
	VDOP float64 `json:"vdop,omitempty"` // Vertical dilution of precision
	PDOP float64 `json:"pdop,omitempty"` // Position dilution of precision

	// System Status
	CollectedAt  time.Time `json:"collected_at"`     // When data was collected
	ResponseTime float64   `json:"response_time_ms"` // Collection time in milliseconds
	Error        string    `json:"error,omitempty"`  // Error message if any

	// Raw Data (for debugging)
	RawData string `json:"raw_data,omitempty"` // Original response
}

// autonomyGPSResponse represents the complete GPS response for autonomy
type autonomyGPSResponse struct {
	Status    string           `json:"status"`              // "success", "partial", "failed"
	Message   string           `json:"message"`             // Human-readable status
	Primary   *autonomyGPSData `json:"primary"`             // Primary GPS source data
	Secondary *autonomyGPSData `json:"secondary,omitempty"` // Secondary GPS for verification
	Tertiary  *autonomyGPSData `json:"tertiary,omitempty"`  // Tertiary GPS for fallback

	// Summary
	BestSource string `json:"best_source"` // Which source is being used
	Confidence string `json:"confidence"`  // "high", "medium", "low"

	// Cross-validation
	SourceAgreement bool    `json:"source_agreement"` // Do sources agree?
	MaxDistance     float64 `json:"max_distance_m"`   // Max distance between sources

	// System metadata
	CollectionTime float64 `json:"collection_time_ms"` // Total collection time
	Timestamp      int64   `json:"timestamp"`          // Response timestamp
}

// Example output formats for different scenarios
func demonstrateGPSFormats() {
	fmt.Println("🎯 autonomy GPS Output Formats")
	fmt.Println("=" + fmt.Sprintf("%30s", "=============================="))

	// Scenario 1: All sources working
	fmt.Println("\n📊 Scenario 1: All GPS Sources Working")
	fmt.Println("=" + fmt.Sprintf("%38s", "======================================"))

	allWorking := autonomyGPSResponse{
		Status:  "success",
		Message: "All GPS sources operational",
		Primary: &autonomyGPSData{
			Latitude:     59.48007000,
			Longitude:    18.27985000,
			Altitude:     9.5,
			Accuracy:     0.4, // HDOP
			Speed:        0.0,
			Course:       0.0,
			Satellites:   37,
			Valid:        true,
			FixType:      "3d",
			FixQuality:   3,
			Source:       "quectel_gnss",
			Priority:     1,
			Method:       "AT+QGPSLOC=2",
			Timestamp:    time.Now().Unix(),
			DateTime:     time.Now().Format("2006-01-02 15:04:05 UTC"),
			Age:          0.1,
			HDOP:         0.4,
			CollectedAt:  time.Now(),
			ResponseTime: 245.5,
		},
		Secondary: &autonomyGPSData{
			Latitude:     59.48005935,
			Longitude:    18.27982195,
			Altitude:     28.4,
			Accuracy:     3.0,
			Satellites:   12,
			Valid:        true,
			FixType:      "3d",
			FixQuality:   1,
			Source:       "starlink",
			Priority:     2,
			Method:       "starlink_api",
			Timestamp:    time.Now().Unix(),
			DateTime:     time.Now().Format("2006-01-02 15:04:05 UTC"),
			Age:          1.2,
			CollectedAt:  time.Now(),
			ResponseTime: 156.3,
		},
		BestSource:      "quectel_gnss",
		Confidence:      "high",
		SourceAgreement: true,
		MaxDistance:     3.2,
		CollectionTime:  401.8,
		Timestamp:       time.Now().Unix(),
	}

	printJSON("All Sources Working", allWorking)

	// Scenario 2: Primary source only
	fmt.Println("\n📊 Scenario 2: Primary Source Only")
	fmt.Println("=" + fmt.Sprintf("%35s", "==================================="))

	primaryOnly := autonomyGPSResponse{
		Status:  "success",
		Message: "Primary GPS source operational",
		Primary: &autonomyGPSData{
			Latitude:     59.48007000,
			Longitude:    18.27985000,
			Altitude:     9.5,
			Accuracy:     0.4,
			Satellites:   37,
			Valid:        true,
			FixType:      "3d",
			Source:       "quectel_gnss",
			Priority:     1,
			Method:       "AT+QGPSLOC=2",
			Timestamp:    time.Now().Unix(),
			DateTime:     time.Now().Format("2006-01-02 15:04:05 UTC"),
			CollectedAt:  time.Now(),
			ResponseTime: 245.5,
		},
		BestSource:     "quectel_gnss",
		Confidence:     "high",
		CollectionTime: 245.5,
		Timestamp:      time.Now().Unix(),
	}

	printJSON("Primary Only", primaryOnly)

	// Scenario 3: Fallback scenario
	fmt.Println("\n📊 Scenario 3: Fallback to Tertiary")
	fmt.Println("=" + fmt.Sprintf("%36s", "===================================="))

	fallback := autonomyGPSResponse{
		Status:  "partial",
		Message: "Using tertiary GPS source",
		Primary: &autonomyGPSData{
			Valid:        false,
			Source:       "quectel_gnss",
			Priority:     1,
			Method:       "AT+QGPSLOC=2",
			Error:        "GPS not ready",
			CollectedAt:  time.Now(),
			ResponseTime: 5000.0, // Timeout
		},
		Tertiary: &autonomyGPSData{
			Latitude:     59.48006800,
			Longitude:    18.27985400,
			Altitude:     9.6,
			Accuracy:     0.5,
			Satellites:   10,
			Valid:        true,
			FixType:      "3d",
			Source:       "basic_gps",
			Priority:     3,
			Method:       "gpsctl",
			Timestamp:    time.Now().Unix(),
			DateTime:     time.Now().Format("2006-01-02 15:04:05 UTC"),
			CollectedAt:  time.Now(),
			ResponseTime: 89.2,
		},
		BestSource:     "basic_gps",
		Confidence:     "medium",
		CollectionTime: 5089.2,
		Timestamp:      time.Now().Unix(),
	}

	printJSON("Fallback Scenario", fallback)

	// Scenario 4: All sources failed
	fmt.Println("\n📊 Scenario 4: All Sources Failed")
	fmt.Println("=" + fmt.Sprintf("%34s", "=================================="))

	allFailed := autonomyGPSResponse{
		Status:  "failed",
		Message: "No GPS sources available",
		Primary: &autonomyGPSData{
			Valid:        false,
			Source:       "quectel_gnss",
			Priority:     1,
			Error:        "GPS timeout",
			CollectedAt:  time.Now(),
			ResponseTime: 5000.0,
		},
		Secondary: &autonomyGPSData{
			Valid:        false,
			Source:       "starlink",
			Priority:     2,
			Error:        "Starlink offline",
			CollectedAt:  time.Now(),
			ResponseTime: 5000.0,
		},
		Tertiary: &autonomyGPSData{
			Valid:        false,
			Source:       "basic_gps",
			Priority:     3,
			Error:        "No GPS fix",
			CollectedAt:  time.Now(),
			ResponseTime: 1000.0,
		},
		BestSource:     "none",
		Confidence:     "none",
		CollectionTime: 11000.0,
		Timestamp:      time.Now().Unix(),
	}

	printJSON("All Failed", allFailed)
}

func printJSON(title string, data interface{}) {
	jsonData, _ := json.MarshalIndent(data, "", "  ")
	fmt.Printf("### %s:\n```json\n%s\n```\n", title, string(jsonData))
}

// Command-line output formats
func demonstrateCommandLineFormats() {
	fmt.Println("\n🖥️  Command Line Output Formats")
	fmt.Println("=" + fmt.Sprintf("%32s", "================================"))

	fmt.Println("\n📋 Format 1: Compact (for scripts)")
	fmt.Println("SUCCESS|quectel_gnss|59.48007000,18.27985000|9.5|0.4|37|3d")

	fmt.Println("\n📋 Format 2: Detailed (for humans)")
	fmt.Println("✅ GPS Status: SUCCESS")
	fmt.Println("📍 Location: 59.48007000°, 18.27985000°")
	fmt.Println("🏔️  Altitude: 9.5 meters")
	fmt.Println("🎯 Accuracy: 0.4 HDOP (excellent)")
	fmt.Println("🛰️  Satellites: 37 (multi-constellation)")
	fmt.Println("📡 Source: Quectel Multi-GNSS (primary)")
	fmt.Println("⏰ Time: 2025-08-16 01:42:53 UTC")
	fmt.Println("🗺️  Maps: https://www.google.com/maps?q=59.48007000,18.27985000")

	fmt.Println("\n📋 Format 3: CSV (for logging)")
	fmt.Println("timestamp,status,source,latitude,longitude,altitude,accuracy,satellites,fix_type")
	fmt.Println("1755308573,success,quectel_gnss,59.48007000,18.27985000,9.5,0.4,37,3d")
}

// Commented out main function - use main.go instead
// func main() {
// 	demonstrateGPSFormats()
// 	demonstrateCommandLineFormats()
// }
