package main

import (
	"fmt"
	"regexp"
	"strconv"
	"strings"
	"time"

	"golang.org/x/crypto/ssh"
)

// GSMGPSData represents GPS data from cellular modem
type GSMGPSData struct {
	Latitude    float64   `json:"latitude"`
	Longitude   float64   `json:"longitude"`
	Altitude    float64   `json:"altitude"`
	Speed       float64   `json:"speed"`
	Course      float64   `json:"course"`
	Satellites  int       `json:"satellites"`
	HDOP        float64   `json:"hdop"`
	FixStatus   int       `json:"fix_status"`
	Valid       bool      `json:"valid"`
	Source      string    `json:"source"`
	RawData     string    `json:"raw_data"`
	CollectedAt time.Time `json:"collected_at"`
}

// testGSMGPS tests comprehensive GSM GPS functionality
func testGSMGPS(client *ssh.Client) {
	fmt.Println("🎯 Testing GSM GPS (Tertiary GPS Source)")
	fmt.Println("=" + strings.Repeat("=", 40))

	// 1. Check modem info and GPS capabilities
	fmt.Println("\n📱 1. Modem Information & GPS Capabilities:")
	checkModemGPSCapabilities(client)

	// 2. Test GPS power and initialization
	fmt.Println("\n🔋 2. GPS Power & Initialization:")
	testGPSPowerCommands(client)

	// 3. Test various GPS AT commands
	fmt.Println("\n📡 3. GPS Data Retrieval Commands:")
	testGPSDataCommands(client)

	// 4. Test location services
	fmt.Println("\n🌐 4. Location Services:")
	testLocationServices(client)

	// 5. Try alternative GPS commands
	fmt.Println("\n🔍 5. Alternative GPS Commands:")
	testAlternativeGPSCommands(client)
}

func checkModemGPSCapabilities(client *ssh.Client) {
	commands := map[string]string{
		"Modem Model":           "gsmctl -m",
		"Modem Manufacturer":    "gsmctl -w",
		"Firmware Version":      "gsmctl -y",
		"IMEI":                  "gsmctl -i",
		"Available AT Commands": "gsmctl -A 'AT+CLAC' | grep -i gps",
		"GPS Module Check":      "gsmctl -A 'AT+CGMM'",
	}

	for name, cmd := range commands {
		fmt.Printf("  %s: ", name)
		output, err := executeCommand(client, cmd)
		if err != nil {
			fmt.Printf("❌ Failed: %v\n", err)
		} else {
			output = strings.TrimSpace(output)
			if output == "" {
				fmt.Printf("⚪ No output\n")
			} else {
				// Clean up output for display
				lines := strings.Split(output, "\n")
				if len(lines) == 1 {
					fmt.Printf("✅ %s\n", output)
				} else {
					fmt.Printf("✅ %s... (%d lines)\n", lines[0], len(lines))
				}
			}
		}
	}
}

func testGPSPowerCommands(client *ssh.Client) {
	powerCommands := []string{
		"AT+CGPS?",     // Check GPS power status
		"AT+CGPSPWR?",  // Check GPS power
		"AT+CGPS=1",    // Turn on GPS
		"AT+CGPSPWR=1", // Power on GPS
		"AT+CGPSRST=0", // GPS cold start
		"AT+CGPSRST=1", // GPS hot start
	}

	for _, cmd := range powerCommands {
		fmt.Printf("  Testing %s: ", cmd)
		output, err := executeCommand(client, fmt.Sprintf("gsmctl -A '%s'", cmd))
		if err != nil {
			fmt.Printf("❌ Command failed: %v\n", err)
		} else {
			output = strings.TrimSpace(output)
			if strings.Contains(output, "OK") {
				fmt.Printf("✅ %s\n", strings.ReplaceAll(output, "\n", " "))
			} else if strings.Contains(output, "ERROR") {
				fmt.Printf("❌ %s\n", output)
			} else {
				fmt.Printf("📊 %s\n", output)
			}
		}
	}
}

func testGPSDataCommands(client *ssh.Client) {
	gpsCommands := []string{
		"AT+CGPSINFO",   // GPS information
		"AT+CGNSINF",    // GNSS information
		"AT+CGPSOUT=32", // NMEA output
		"AT+CGPSOUT=2",  // Location output
		"AT+CGPSOUT=1",  // GPS output
		"AT+CLBS=1,1",   // Location services
		"AT+CLBS=4,1",   // Get location
		"AT+CGPSINF=32", // GPS info alternative
		"AT+CGPSINF=0",  // GPS info basic
	}

	for _, cmd := range gpsCommands {
		fmt.Printf("  Testing %s: ", cmd)
		output, err := executeCommand(client, fmt.Sprintf("gsmctl -A '%s'", cmd))
		if err != nil {
			fmt.Printf("❌ Command failed: %v\n", err)
		} else {
			output = strings.TrimSpace(output)
			if strings.Contains(output, "OK") && len(output) > 10 {
				// Try to parse GPS data
				if gpsData := parseGSMGPSResponse(output, cmd); gpsData != nil && gpsData.Valid {
					fmt.Printf("🎉 GPS DATA FOUND!\n")
					displayGSMGPSData(gpsData)
				} else {
					fmt.Printf("✅ %s\n", truncateOutput(output, 80))
				}
			} else if strings.Contains(output, "ERROR") {
				fmt.Printf("❌ %s\n", output)
			} else {
				fmt.Printf("📊 %s\n", truncateOutput(output, 80))
			}
		}
	}
}

func testLocationServices(client *ssh.Client) {
	locationCommands := []string{
		"AT+CLBS=1,1",      // Enable location services
		"AT+CLBS=4,1",      // Get current location
		"AT+CLBS=2,1",      // Location method 2
		"AT+CLBS=3,1",      // Location method 3
		"AT+CIPGSMLOC=1,1", // GSM location
		"AT+CIPGSMLOC=2,1", // GPS location
	}

	for _, cmd := range locationCommands {
		fmt.Printf("  Testing %s: ", cmd)
		output, err := executeCommand(client, fmt.Sprintf("gsmctl -A '%s'", cmd))
		if err != nil {
			fmt.Printf("❌ Command failed: %v\n", err)
		} else {
			output = strings.TrimSpace(output)
			if strings.Contains(output, "+CLBS:") || strings.Contains(output, "+CIPGSMLOC:") {
				fmt.Printf("🎉 LOCATION DATA!\n")
				fmt.Printf("    📊 %s\n", output)
			} else if strings.Contains(output, "OK") {
				fmt.Printf("✅ %s\n", truncateOutput(output, 60))
			} else if strings.Contains(output, "ERROR") {
				fmt.Printf("❌ %s\n", output)
			} else {
				fmt.Printf("📊 %s\n", truncateOutput(output, 60))
			}
		}
	}
}

func testAlternativeGPSCommands(client *ssh.Client) {
	// Try different modem-specific commands
	altCommands := []string{
		"AT+QGPS=1",      // Quectel GPS on
		"AT+QGPSGNMEA=1", // Quectel NMEA
		"AT+QGPSLOC=2",   // Quectel location
		"AT+QGPSEND",     // Quectel GPS end
		"AT+UGPS=1,1,1",  // u-blox GPS
		"AT+UGGGA?",      // u-blox GGA
		"AT+UGRMC?",      // u-blox RMC
	}

	for _, cmd := range altCommands {
		fmt.Printf("  Testing %s: ", cmd)
		output, err := executeCommand(client, fmt.Sprintf("gsmctl -A '%s'", cmd))
		if err != nil {
			fmt.Printf("❌ Command failed: %v\n", err)
		} else {
			output = strings.TrimSpace(output)
			if strings.Contains(output, "OK") && len(output) > 5 {
				fmt.Printf("✅ %s\n", truncateOutput(output, 60))
			} else if strings.Contains(output, "ERROR") {
				fmt.Printf("❌ %s\n", output)
			} else {
				fmt.Printf("📊 %s\n", truncateOutput(output, 60))
			}
		}
	}
}

func parseGSMGPSResponse(response, command string) *GSMGPSData {
	gpsData := &GSMGPSData{
		Source:      "gsm_cellular",
		CollectedAt: time.Now(),
		RawData:     response,
	}

	// Parse different response formats
	if strings.Contains(response, "+CGPSINFO:") {
		return parseCGPSINFO(response, gpsData)
	} else if strings.Contains(response, "+CGNSINF:") {
		return parseCGNSINF(response, gpsData)
	} else if strings.Contains(response, "+CLBS:") {
		return parseCLBS(response, gpsData)
	} else if strings.Contains(response, "+CIPGSMLOC:") {
		return parseCIPGSMLOC(response, gpsData)
	}

	return gpsData
}

func parseCGPSINFO(response string, gpsData *GSMGPSData) *GSMGPSData {
	// Parse +CGPSINFO: lat,N,lon,E,date,time,alt,speed,course
	re := regexp.MustCompile(`\+CGPSINFO:\s*([^,]+),([NS]),([^,]+),([EW]),([^,]+),([^,]+),([^,]+),([^,]+),([^,]+)`)
	matches := re.FindStringSubmatch(response)

	if len(matches) >= 10 {
		if lat, err := strconv.ParseFloat(matches[1], 64); err == nil && lat != 0 {
			gpsData.Latitude = convertDDMMtoDecimal(lat)
			if matches[2] == "S" {
				gpsData.Latitude = -gpsData.Latitude
			}
		}
		if lon, err := strconv.ParseFloat(matches[3], 64); err == nil && lon != 0 {
			gpsData.Longitude = convertDDMMtoDecimal(lon)
			if matches[4] == "W" {
				gpsData.Longitude = -gpsData.Longitude
			}
		}
		if alt, err := strconv.ParseFloat(matches[7], 64); err == nil {
			gpsData.Altitude = alt
		}
		if speed, err := strconv.ParseFloat(matches[8], 64); err == nil {
			gpsData.Speed = speed
		}
		if course, err := strconv.ParseFloat(matches[9], 64); err == nil {
			gpsData.Course = course
		}

		gpsData.Valid = gpsData.Latitude != 0 && gpsData.Longitude != 0
	}

	return gpsData
}

func parseCGNSINF(response string, gpsData *GSMGPSData) *GSMGPSData {
	// Parse +CGNSINF: status,lat,lon,alt,speed,course,fix_mode,reserved1,HDOP,PDOP,VDOP,reserved2,satellites,reserved3,reserved4
	parts := strings.Split(response, ",")
	if len(parts) >= 15 {
		if status := parts[1]; status == "1" { // GPS fix available
			if lat, err := strconv.ParseFloat(parts[2], 64); err == nil {
				gpsData.Latitude = lat
			}
			if lon, err := strconv.ParseFloat(parts[3], 64); err == nil {
				gpsData.Longitude = lon
			}
			if alt, err := strconv.ParseFloat(parts[4], 64); err == nil {
				gpsData.Altitude = alt
			}
			if speed, err := strconv.ParseFloat(parts[5], 64); err == nil {
				gpsData.Speed = speed
			}
			if course, err := strconv.ParseFloat(parts[6], 64); err == nil {
				gpsData.Course = course
			}
			if hdop, err := strconv.ParseFloat(parts[8], 64); err == nil {
				gpsData.HDOP = hdop
			}
			if sats, err := strconv.Atoi(parts[12]); err == nil {
				gpsData.Satellites = sats
			}

			gpsData.Valid = gpsData.Latitude != 0 && gpsData.Longitude != 0
			gpsData.FixStatus = 1
		}
	}

	return gpsData
}

func parseCLBS(response string, gpsData *GSMGPSData) *GSMGPSData {
	// Parse +CLBS: location_type,longitude,latitude,accuracy,date,time
	re := regexp.MustCompile(`\+CLBS:\s*(\d+),([^,]+),([^,]+),([^,]+),([^,]+),([^,]+)`)
	matches := re.FindStringSubmatch(response)

	if len(matches) >= 7 {
		if lon, err := strconv.ParseFloat(matches[2], 64); err == nil {
			gpsData.Longitude = lon
		}
		if lat, err := strconv.ParseFloat(matches[3], 64); err == nil {
			gpsData.Latitude = lat
		}

		gpsData.Valid = gpsData.Latitude != 0 && gpsData.Longitude != 0
		gpsData.Source = "gsm_location_service"
	}

	return gpsData
}

func parseCIPGSMLOC(response string, gpsData *GSMGPSData) *GSMGPSData {
	// Parse +CIPGSMLOC: longitude,latitude,accuracy,date,time
	re := regexp.MustCompile(`\+CIPGSMLOC:\s*([^,]+),([^,]+),([^,]+),([^,]+),([^,]+)`)
	matches := re.FindStringSubmatch(response)

	if len(matches) >= 6 {
		if lon, err := strconv.ParseFloat(matches[1], 64); err == nil {
			gpsData.Longitude = lon
		}
		if lat, err := strconv.ParseFloat(matches[2], 64); err == nil {
			gpsData.Latitude = lat
		}

		gpsData.Valid = gpsData.Latitude != 0 && gpsData.Longitude != 0
		gpsData.Source = "gsm_ip_location"
	}

	return gpsData
}

func convertDDMMtoDecimal(ddmm float64) float64 {
	degrees := int(ddmm / 100)
	minutes := ddmm - float64(degrees*100)
	return float64(degrees) + minutes/60.0
}

func displayGSMGPSData(gps *GSMGPSData) {
	fmt.Printf("    📍 Coordinates: %.8f°, %.8f°\n", gps.Latitude, gps.Longitude)
	if gps.Altitude != 0 {
		fmt.Printf("    🏔️  Altitude: %.2f meters\n", gps.Altitude)
	}
	if gps.Satellites > 0 {
		fmt.Printf("    🛰️  Satellites: %d\n", gps.Satellites)
	}
	if gps.Speed != 0 {
		fmt.Printf("    🚀 Speed: %.2f knots\n", gps.Speed)
	}
	if gps.Course != 0 {
		fmt.Printf("    🧭 Course: %.2f°\n", gps.Course)
	}
	fmt.Printf("    📡 Source: %s\n", gps.Source)

	// Create Google Maps link
	mapsLink := fmt.Sprintf("https://www.google.com/maps?q=%.8f,%.8f", gps.Latitude, gps.Longitude)
	fmt.Printf("    🗺️  Maps Link: %s\n", mapsLink)
}

func truncateOutput(output string, maxLen int) string {
	if len(output) <= maxLen {
		return output
	}
	return output[:maxLen] + "..."
}
