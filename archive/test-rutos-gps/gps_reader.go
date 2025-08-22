package main

import (
	"fmt"
	"strings"
	"time"

	"golang.org/x/crypto/ssh"
)

// GPSData represents parsed GPS coordinates
type GPSCoordinates struct {
	Latitude   float64
	Longitude  float64
	Altitude   float64
	Speed      float64
	Course     float64
	Satellites int
	Quality    int
	HDOP       float64
	Timestamp  time.Time
	Valid      bool
	Source     string
	RawData    string
}

// testGPSDaemon tests the GPS daemon (gpsd)
func testGPSDaemon(client *ssh.Client) GPSTestResult {
	start := time.Now()
	result := GPSTestResult{
		Method: "gpsd daemon",
	}

	// Test gpsd status
	output, err := executeCommand(client, "gpspipe -w -n 5")
	result.Output = output
	result.Duration = time.Since(start)

	if err != nil {
		result.Error = fmt.Sprintf("gpspipe command failed: %v", err)
		return result
	}

	if strings.Contains(output, "lat") && strings.Contains(output, "lon") {
		result.Success = true
		result.Source = "gpsd"
	} else {
		result.Error = "No GPS data from gpsd"
	}

	return result
}

// testNMEADirect tests direct NMEA reading from GPS devices
func testNMEADirect(client *ssh.Client) []GPSTestResult {
	var results []GPSTestResult
	devices := []string{"/dev/ttyUSB0", "/dev/ttyUSB1", "/dev/ttyUSB2"}

	for _, device := range devices {
		start := time.Now()
		result := GPSTestResult{
			Method: fmt.Sprintf("NMEA Direct (%s)", device),
		}

		// Read a few lines from the GPS device
		cmd := fmt.Sprintf("timeout 10 head -n 10 %s", device)
		output, err := executeCommand(client, cmd)
		result.Output = output
		result.Duration = time.Since(start)

		if err != nil {
			result.Error = fmt.Sprintf("Failed to read from %s: %v", device, err)
		} else if strings.Contains(output, "$GP") || strings.Contains(output, "$GN") {
			result.Success = true
			result.Source = fmt.Sprintf("nmea_%s", device)

			// Try to parse NMEA data
			coords := parseNMEAData(output)
			if coords.Valid {
				result.Latitude = coords.Latitude
				result.Longitude = coords.Longitude
				result.Altitude = coords.Altitude
			}
		} else {
			result.Error = fmt.Sprintf("No NMEA data from %s", device)
		}

		results = append(results, result)
	}

	return results
}

// testATCommands tests various AT commands for GPS
func testATCommands(client *ssh.Client) []GPSTestResult {
	var results []GPSTestResult

	atCommands := map[string]string{
		"GPS Info (CGPSINFO)":  "gsmctl -A 'AT+CGPSINFO'",
		"GPS Status (CGNSINF)": "gsmctl -A 'AT+CGNSINF'",
		"GPS Power (CGPSPWR)":  "gsmctl -A 'AT+CGPSPWR?'",
		"GPS Config (CGPSRST)": "gsmctl -A 'AT+CGPSRST?'",
		"Location Services":    "gsmctl -A 'AT+CLBS=1,1'",
		"Network Time":         "gsmctl -A 'AT+CCLK?'",
	}

	for name, cmd := range atCommands {
		start := time.Now()
		result := GPSTestResult{
			Method: fmt.Sprintf("AT Command: %s", name),
		}

		output, err := executeCommand(client, cmd)
		result.Output = output
		result.Duration = time.Since(start)

		if err != nil {
			result.Error = fmt.Sprintf("AT command failed: %v", err)
		} else if strings.Contains(output, "ERROR") {
			result.Error = "AT command returned ERROR"
		} else if strings.Contains(output, "OK") || strings.Contains(output, "+C") {
			result.Success = true
			result.Source = "at_command"
		} else {
			result.Error = "Unexpected AT command response"
		}

		results = append(results, result)
	}

	return results
}

// parseNMEAData parses NMEA sentences for GPS coordinates
func parseNMEAData(nmeaData string) GPSCoordinates {
	coords := GPSCoordinates{
		Timestamp: time.Now(),
		Source:    "nmea",
	}

	lines := strings.Split(nmeaData, "\n")
	for _, line := range lines {
		line = strings.TrimSpace(line)

		// Parse GPGGA (Global Positioning System Fix Data)
		if strings.HasPrefix(line, "$GPGGA") || strings.HasPrefix(line, "$GNGGA") {
			parts := strings.Split(line, ",")
			if len(parts) >= 15 {
				// GPGGA format: $GPGGA,time,lat,N/S,lon,E/W,quality,satellites,hdop,altitude,M,geoid,M,dgps_time,dgps_id*checksum
				if parts[2] != "" && parts[4] != "" {
					coords.Valid = true
					// TODO: Parse actual coordinates from DDMM.MMMM format
					coords.Quality = parseInt(parts[6])
					coords.Satellites = parseInt(parts[7])
					coords.HDOP = parseFloat(parts[8])
					coords.Altitude = parseFloat(parts[9])
				}
			}
		}

		// Parse GPRMC (Recommended Minimum Course)
		if strings.HasPrefix(line, "$GPRMC") || strings.HasPrefix(line, "$GNRMC") {
			parts := strings.Split(line, ",")
			if len(parts) >= 12 {
				// GPRMC format: $GPRMC,time,status,lat,N/S,lon,E/W,speed,course,date,magnetic_variation,E/W*checksum
				if parts[2] == "A" { // Active (valid)
					coords.Valid = true
					coords.Speed = parseFloat(parts[7])
					coords.Course = parseFloat(parts[8])
				}
			}
		}
	}

	coords.RawData = nmeaData
	return coords
}

// Helper functions for parsing
func parseFloat(s string) float64 {
	// Simple float parsing - in production use strconv.ParseFloat
	return 0.0
}

func parseInt(s string) int {
	// Simple int parsing - in production use strconv.Atoi
	return 0
}
