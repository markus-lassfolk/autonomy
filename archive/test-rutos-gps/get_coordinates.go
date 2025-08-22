package main

import (
	"fmt"
	"strconv"
	"strings"
	"time"

	"golang.org/x/crypto/ssh"
)

// getGPSCoordinates reads GPS coordinates from RutOS device
func getGPSCoordinates(client *ssh.Client) {
	fmt.Println("🎯 Getting Real GPS Coordinates from RutOS")
	fmt.Println("=" + strings.Repeat("=", 42))

	// Read more NMEA data to find coordinate sentences
	fmt.Println("📡 Reading NMEA data for coordinates...")

	// Try to get GPGGA and GPRMC sentences which contain coordinates
	cmd := "timeout 30 cat /dev/ttyUSB1 | grep -E '\\$(GP|GN)(GGA|RMC)' | head -10"
	output, err := executeCommand(client, cmd)
	if err != nil {
		fmt.Printf("❌ Failed to read GPS coordinates: %v\n", err)
		return
	}

	if strings.TrimSpace(output) == "" {
		fmt.Println("⏳ No coordinate sentences found, trying longer read...")
		// Try reading raw data for 30 seconds
		cmd = "timeout 30 cat /dev/ttyUSB1 | head -50"
		output, err = executeCommand(client, cmd)
		if err != nil {
			fmt.Printf("❌ Failed to read raw GPS data: %v\n", err)
			return
		}
	}

	fmt.Println("📊 Raw NMEA Data:")
	fmt.Println(strings.Repeat("-", 60))
	fmt.Println(output)
	fmt.Println(strings.Repeat("-", 60))

	// Parse the NMEA data
	coordinates := parseNMEACoordinates(output)

	if len(coordinates) > 0 {
		fmt.Println("🎉 GPS Coordinates Found!")
		for i, coord := range coordinates {
			fmt.Printf("\n📍 Fix #%d:\n", i+1)
			fmt.Printf("   Latitude:  %.8f°\n", coord.Latitude)
			fmt.Printf("   Longitude: %.8f°\n", coord.Longitude)
			if coord.Altitude != 0 {
				fmt.Printf("   Altitude:  %.2f m\n", coord.Altitude)
			}
			fmt.Printf("   Quality:   %d\n", coord.Quality)
			fmt.Printf("   Satellites: %d\n", coord.Satellites)
			fmt.Printf("   HDOP:      %.2f\n", coord.HDOP)
			fmt.Printf("   Time:      %s\n", coord.Timestamp.Format("15:04:05"))
			fmt.Printf("   Valid:     %t\n", coord.Valid)
			fmt.Printf("   Source:    %s\n", coord.Source)
		}
	} else {
		fmt.Println("❌ No valid GPS coordinates found in NMEA data")
		fmt.Println("💡 This might mean:")
		fmt.Println("   - GPS is still acquiring satellites")
		fmt.Println("   - GPS antenna not connected")
		fmt.Println("   - GPS disabled in modem configuration")
	}
}

// parseNMEACoordinates parses NMEA sentences for GPS coordinates
func parseNMEACoordinates(nmeaData string) []GPSCoordinates {
	var coordinates []GPSCoordinates

	lines := strings.Split(nmeaData, "\n")
	for _, line := range lines {
		line = strings.TrimSpace(line)

		// Parse GPGGA (Global Positioning System Fix Data)
		if strings.HasPrefix(line, "$GPGGA") || strings.HasPrefix(line, "$GNGGA") {
			coord := parseGPGGA(line)
			if coord.Valid {
				coordinates = append(coordinates, coord)
			}
		}

		// Parse GPRMC (Recommended Minimum Course)
		if strings.HasPrefix(line, "$GPRMC") || strings.HasPrefix(line, "$GNRMC") {
			coord := parseGPRMC(line)
			if coord.Valid {
				coordinates = append(coordinates, coord)
			}
		}
	}

	return coordinates
}

// parseGPGGA parses GPGGA NMEA sentence
func parseGPGGA(sentence string) GPSCoordinates {
	coord := GPSCoordinates{
		Source:    "GPGGA",
		Timestamp: time.Now(),
	}

	parts := strings.Split(sentence, ",")
	if len(parts) < 15 {
		return coord
	}

	// GPGGA format: $GPGGA,time,lat,N/S,lon,E/W,quality,satellites,hdop,altitude,M,geoid,M,dgps_time,dgps_id*checksum

	// Parse time
	if parts[1] != "" {
		coord.Timestamp = parseNMEATime(parts[1])
	}

	// Parse latitude
	if parts[2] != "" && parts[3] != "" {
		lat := parseNMEACoordinate(parts[2])
		if parts[3] == "S" {
			lat = -lat
		}
		coord.Latitude = lat
	}

	// Parse longitude
	if parts[4] != "" && parts[5] != "" {
		lon := parseNMEACoordinate(parts[4])
		if parts[5] == "W" {
			lon = -lon
		}
		coord.Longitude = lon
	}

	// Parse quality
	if parts[6] != "" {
		coord.Quality, _ = strconv.Atoi(parts[6])
	}

	// Parse satellites
	if parts[7] != "" {
		coord.Satellites, _ = strconv.Atoi(parts[7])
	}

	// Parse HDOP
	if parts[8] != "" {
		coord.HDOP, _ = strconv.ParseFloat(parts[8], 64)
	}

	// Parse altitude
	if parts[9] != "" {
		coord.Altitude, _ = strconv.ParseFloat(parts[9], 64)
	}

	// Check if fix is valid
	coord.Valid = coord.Quality > 0 && coord.Latitude != 0 && coord.Longitude != 0

	return coord
}

// parseGPRMC parses GPRMC NMEA sentence
func parseGPRMC(sentence string) GPSCoordinates {
	coord := GPSCoordinates{
		Source:    "GPRMC",
		Timestamp: time.Now(),
	}

	parts := strings.Split(sentence, ",")
	if len(parts) < 12 {
		return coord
	}

	// GPRMC format: $GPRMC,time,status,lat,N/S,lon,E/W,speed,course,date,magnetic_variation,E/W*checksum

	// Check status (A = Active, V = Void)
	if parts[2] != "A" {
		return coord
	}

	// Parse time
	if parts[1] != "" {
		coord.Timestamp = parseNMEATime(parts[1])
	}

	// Parse latitude
	if parts[3] != "" && parts[4] != "" {
		lat := parseNMEACoordinate(parts[3])
		if parts[4] == "S" {
			lat = -lat
		}
		coord.Latitude = lat
	}

	// Parse longitude
	if parts[5] != "" && parts[6] != "" {
		lon := parseNMEACoordinate(parts[5])
		if parts[6] == "W" {
			lon = -lon
		}
		coord.Longitude = lon
	}

	// Parse speed (knots)
	if parts[7] != "" {
		coord.Speed, _ = strconv.ParseFloat(parts[7], 64)
	}

	// Parse course
	if parts[8] != "" {
		coord.Course, _ = strconv.ParseFloat(parts[8], 64)
	}

	coord.Valid = coord.Latitude != 0 && coord.Longitude != 0

	return coord
}

// parseNMEACoordinate converts NMEA coordinate format (DDMM.MMMM) to decimal degrees
func parseNMEACoordinate(nmeaCoord string) float64 {
	if nmeaCoord == "" {
		return 0
	}

	coord, err := strconv.ParseFloat(nmeaCoord, 64)
	if err != nil {
		return 0
	}

	// Convert DDMM.MMMM to decimal degrees
	degrees := int(coord / 100)
	minutes := coord - float64(degrees*100)

	return float64(degrees) + minutes/60.0
}

// parseNMEATime parses NMEA time format (HHMMSS.SSS)
func parseNMEATime(nmeaTime string) time.Time {
	if len(nmeaTime) < 6 {
		return time.Now()
	}

	hour, _ := strconv.Atoi(nmeaTime[0:2])
	minute, _ := strconv.Atoi(nmeaTime[2:4])
	second, _ := strconv.Atoi(nmeaTime[4:6])

	now := time.Now()
	return time.Date(now.Year(), now.Month(), now.Day(), hour, minute, second, 0, time.UTC)
}
