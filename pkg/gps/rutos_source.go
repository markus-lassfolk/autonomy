package gps

import (
	"context"
	"encoding/json"
	"fmt"
	"math"
	"os/exec"
	"strconv"
	"strings"
	"time"

	"github.com/markus-lassfolk/autonomy/pkg/logx"
)

// RUTOSGPSSource implements GPS collection from RUTOS/OpenWrt systems
type RUTOSGPSSource struct {
	logger       *logx.Logger
	priority     int
	health       GPSSourceHealth
	devices      []string
	lastSuccess  time.Time
	errorCount   int
	successCount int
}

// NMEAData represents parsed NMEA GPS data
type NMEAData struct {
	Latitude   float64
	Longitude  float64
	Altitude   float64
	Speed      float64
	Course     float64
	HDOP       float64
	VDOP       float64
	Satellites int
	FixQuality int // NMEA fix quality (0-8 scale)
	FixType    int // Derived fix type (0-3 scale)
	Timestamp  time.Time
	Valid      bool
	Source     string
}

// NewRUTOSGPSSource creates a new RUTOS GPS source
func NewRUTOSGPSSource(priority int, logger *logx.Logger) *RUTOSGPSSource {
	return &RUTOSGPSSource{
		logger:   logger,
		priority: priority,
		health: GPSSourceHealth{
			Available:    false,
			LastSuccess:  time.Time{},
			LastError:    "",
			SuccessRate:  0.0,
			AvgLatency:   0.0,
			ErrorCount:   0,
			SuccessCount: 0,
		},
		devices: []string{"/dev/ttyUSB1", "/dev/ttyUSB2", "/dev/ttyACM0"},
	}
}

// GetName returns the source name
func (rs *RUTOSGPSSource) GetName() string {
	return "rutos"
}

// GetPriority returns the source priority
func (rs *RUTOSGPSSource) GetPriority() int {
	return rs.priority
}

// GetHealthStatus returns the current health status
func (rs *RUTOSGPSSource) GetHealthStatus() GPSSourceHealth {
	total := rs.errorCount + rs.successCount
	if total > 0 {
		rs.health.SuccessRate = float64(rs.successCount) / float64(total)
	}
	return rs.health
}

// IsAvailable checks if RUTOS GPS is available
func (rs *RUTOSGPSSource) IsAvailable(ctx context.Context) bool {
	availabilityReasons := []string{}

	// Check if gpsctl command is available
	if cmd := exec.CommandContext(ctx, "which", "gpsctl"); cmd.Run() == nil {
		rs.logger.LogDebugVerbose("rutos_gps_method_available", map[string]interface{}{
			"method": "gpsctl",
			"status": "available",
		})
		availabilityReasons = append(availabilityReasons, "gpsctl")
	}

	// Check if gsmctl command is available (for AT commands)
	if cmd := exec.CommandContext(ctx, "which", "gsmctl"); cmd.Run() == nil {
		rs.logger.LogDebugVerbose("rutos_gps_method_available", map[string]interface{}{
			"method": "gsmctl",
			"status": "available",
		})
		availabilityReasons = append(availabilityReasons, "gsmctl")
	}

	// Check if ubus is available (for GPS data via ubus)
	if cmd := exec.CommandContext(ctx, "which", "ubus"); cmd.Run() == nil {
		rs.logger.LogDebugVerbose("rutos_gps_method_available", map[string]interface{}{
			"method": "ubus",
			"status": "available",
		})
		availabilityReasons = append(availabilityReasons, "ubus")
	}

	// Check if any NMEA devices are available
	availableDevices := []string{}
	for _, device := range rs.devices {
		if cmd := exec.CommandContext(ctx, "test", "-c", device); cmd.Run() == nil {
			availableDevices = append(availableDevices, device)
		}
	}

	if len(availableDevices) > 0 {
		rs.logger.LogDebugVerbose("rutos_gps_nmea_devices_available", map[string]interface{}{
			"devices": availableDevices,
			"count":   len(availableDevices),
		})
		availabilityReasons = append(availabilityReasons, "nmea_devices")
	}

	// Determine overall availability
	isAvailable := len(availabilityReasons) > 0
	rs.health.Available = isAvailable

	if isAvailable {
		rs.logger.LogDebugVerbose("rutos_gps_availability_check", map[string]interface{}{
			"available":         true,
			"available_methods": availabilityReasons,
			"method_count":      len(availabilityReasons),
		})
	} else {
		rs.logger.LogDebugVerbose("rutos_gps_availability_check", map[string]interface{}{
			"available":       false,
			"checked_methods": []string{"gpsctl", "gsmctl", "ubus", "nmea_devices"},
			"reason":          "no_gps_methods_or_hardware_detected",
		})
	}

	return isAvailable
}

// CollectGPS collects GPS data from RUTOS using multiple methods
func (rs *RUTOSGPSSource) CollectGPS(ctx context.Context) (*StandardizedGPSData, error) {
	start := time.Now()

	// Try multiple collection methods in order of preference
	methods := []func(context.Context) (*StandardizedGPSData, error){
		rs.collectFromGpsctl,
		rs.collectFromNMEADirect,
		rs.collectFromATCommand,
		rs.collectFromGsmctl,
		rs.collectFromUbus,
	}

	var lastError error
	for _, method := range methods {
		gpsData, err := method(ctx)
		if err != nil {
			lastError = err
			continue
		}

		if gpsData != nil && gpsData.Valid {
			// Update health metrics
			rs.successCount++
			rs.lastSuccess = time.Now()
			rs.health.LastSuccess = rs.lastSuccess
			rs.health.AvgLatency = (rs.health.AvgLatency*float64(rs.successCount-1) + float64(time.Since(start).Milliseconds())) / float64(rs.successCount)

			gpsData.CollectionTime = time.Since(start)
			return gpsData, nil
		}
	}

	// Update error metrics
	rs.errorCount++
	if lastError != nil {
		rs.health.LastError = lastError.Error()
	}

	return nil, fmt.Errorf("failed to collect GPS data from RUTOS: %w", lastError)
}

// collectFromGpsctl collects GPS data using gpsctl command (highest accuracy)
func (rs *RUTOSGPSSource) collectFromGpsctl(ctx context.Context) (*StandardizedGPSData, error) {
	// Check GPS status first
	statusCmd := exec.CommandContext(ctx, "gpsctl", "-s")
	statusOutput, err := statusCmd.Output()
	if err != nil {
		return nil, fmt.Errorf("gpsctl status check failed: %w", err)
	}

	status := strings.TrimSpace(string(statusOutput))
	if status != "1" {
		return nil, fmt.Errorf("GPS not active, status: %s", status)
	}

	// Collect GPS data using multiple gpsctl commands
	data := &StandardizedGPSData{
		Source:      "RUTOS Combined",
		Method:      "gpsctl",
		DataSources: []string{"gpsctl"},
		Timestamp:   time.Now(),
		Valid:       false,
	}

	// Get coordinates
	if lat, err := rs.getGpsctlValue(ctx, "-i"); err == nil {
		data.Latitude = lat
	}
	if lon, err := rs.getGpsctlValue(ctx, "-x"); err == nil {
		data.Longitude = lon
	}
	if alt, err := rs.getGpsctlValue(ctx, "-a"); err == nil {
		data.Altitude = alt
	}
	if acc, err := rs.getGpsctlValue(ctx, "-u"); err == nil {
		data.Accuracy = acc
	}
	if sats, err := rs.getGpsctlValueInt(ctx, "-p"); err == nil {
		data.Satellites = sats
	}
	if speed, err := rs.getGpsctlValue(ctx, "-v"); err == nil {
		data.Speed = speed
	}
	if course, err := rs.getGpsctlValue(ctx, "-c"); err == nil {
		data.Course = course
	}
	if hdop, err := rs.getGpsctlValue(ctx, "-h"); err == nil {
		data.HDOP = hdop
	}

	// Validate coordinates
	if data.Latitude != 0 && data.Longitude != 0 {
		data.Valid = true
		data.FixType = rs.determineFixType(data.Accuracy, data.Satellites)
		data.FixQuality = rs.determineFixQuality(data.Accuracy, data.Satellites)
		data.Confidence = rs.calculateConfidence(data.Accuracy, data.Satellites)
	}

	if !data.Valid {
		return nil, fmt.Errorf("invalid GPS coordinates from gpsctl")
	}

	return data, nil
}

// collectFromNMEADirect collects GPS data directly from NMEA devices
func (rs *RUTOSGPSSource) collectFromNMEADirect(ctx context.Context) (*StandardizedGPSData, error) {
	for _, device := range rs.devices {
		// Check if device exists
		if cmd := exec.CommandContext(ctx, "test", "-c", device); cmd.Run() != nil {
			continue
		}

		// Read NMEA data with timeout
		timeoutCtx, cancel := context.WithTimeout(ctx, 5*time.Second)
		cmd := exec.CommandContext(timeoutCtx, "timeout", "3", "cat", device)
		output, err := cmd.Output()
		cancel()

		if err != nil {
			continue
		}

		// Parse NMEA data
		nmeaData := rs.parseNMEAData(string(output))
		if nmeaData != nil && nmeaData.Valid {
			return rs.convertNMEAToStandardized(nmeaData), nil
		}
	}

	return nil, fmt.Errorf("no valid NMEA data from any device")
}

// collectFromATCommand collects GPS data using AT commands
func (rs *RUTOSGPSSource) collectFromATCommand(ctx context.Context) (*StandardizedGPSData, error) {
	// Try comprehensive AT command for GPS info
	cmd := exec.CommandContext(ctx, "gsmctl", "-A", "AT+QGPSLOC=2")
	output, err := cmd.Output()
	if err != nil {
		return nil, fmt.Errorf("AT command failed: %w", err)
	}

	return rs.parseATGPSResponse(string(output))
}

// collectFromGsmctl collects GPS data using gsmctl GPS info
func (rs *RUTOSGPSSource) collectFromGsmctl(ctx context.Context) (*StandardizedGPSData, error) {
	cmd := exec.CommandContext(ctx, "gsmctl", "-A", "AT+CGPSINFO")
	output, err := cmd.Output()
	if err != nil {
		return nil, fmt.Errorf("gsmctl GPS command failed: %w", err)
	}

	return rs.parseGsmctlGPSOutput(string(output))
}

// collectFromUbus collects GPS data using ubus
func (rs *RUTOSGPSSource) collectFromUbus(ctx context.Context) (*StandardizedGPSData, error) {
	cmd := exec.CommandContext(ctx, "ubus", "call", "gps", "info")
	output, err := cmd.Output()
	if err != nil {
		return nil, fmt.Errorf("ubus GPS call failed: %w", err)
	}

	var ubusResp map[string]interface{}
	if err := json.Unmarshal(output, &ubusResp); err != nil {
		return nil, fmt.Errorf("failed to parse ubus GPS response: %w", err)
	}

	return rs.parseUbusGPSResponse(ubusResp)
}

// Helper functions for gpsctl value extraction
func (rs *RUTOSGPSSource) getGpsctlValue(ctx context.Context, flag string) (float64, error) {
	cmd := exec.CommandContext(ctx, "gpsctl", flag)
	output, err := cmd.Output()
	if err != nil {
		return 0, err
	}
	return strconv.ParseFloat(strings.TrimSpace(string(output)), 64)
}

func (rs *RUTOSGPSSource) getGpsctlValueInt(ctx context.Context, flag string) (int, error) {
	cmd := exec.CommandContext(ctx, "gpsctl", flag)
	output, err := cmd.Output()
	if err != nil {
		return 0, err
	}
	return strconv.Atoi(strings.TrimSpace(string(output)))
}

// parseNMEAData parses NMEA sentences and extracts GPS data
func (rs *RUTOSGPSSource) parseNMEAData(nmeaText string) *NMEAData {
	lines := strings.Split(nmeaText, "\n")

	var gga, rmc *NMEAData

	for _, line := range lines {
		line = strings.TrimSpace(line)
		if strings.HasPrefix(line, "$GPGGA") || strings.HasPrefix(line, "$GNGGA") {
			gga = rs.parseGGA(line)
		} else if strings.HasPrefix(line, "$GPRMC") || strings.HasPrefix(line, "$GNRMC") {
			rmc = rs.parseRMC(line)
		}
	}

	// Combine GGA and RMC data for most complete information
	if gga != nil && gga.Valid {
		if rmc != nil && rmc.Valid {
			// Merge RMC data into GGA
			gga.Speed = rmc.Speed
			gga.Course = rmc.Course
		}
		return gga
	} else if rmc != nil && rmc.Valid {
		return rmc
	}

	return nil
}

// parseGGA parses GPGGA NMEA sentence
func (rs *RUTOSGPSSource) parseGGA(sentence string) *NMEAData {
	parts := strings.Split(sentence, ",")
	if len(parts) < 15 {
		return nil
	}

	data := &NMEAData{Source: "GPGGA"}

	// Parse fix quality
	if quality, err := strconv.Atoi(parts[6]); err == nil {
		data.FixQuality = quality
		data.Valid = quality > 0
		// Convert NMEA fix quality (0-8) to standard fix type (0-3)
		data.FixType = rs.convertNMEAFixQualityToFixType(quality)
	}

	if !data.Valid {
		return data
	}

	// Parse coordinates
	if lat := rs.parseCoordinate(parts[2], parts[3]); lat != 0 {
		data.Latitude = lat
	}
	if lon := rs.parseCoordinate(parts[4], parts[5]); lon != 0 {
		data.Longitude = lon
	}

	// Parse satellites
	if sats, err := strconv.Atoi(parts[7]); err == nil {
		data.Satellites = sats
	}

	// Parse HDOP
	if hdop, err := strconv.ParseFloat(parts[8], 64); err == nil {
		data.HDOP = hdop
	}

	// Parse altitude
	if alt, err := strconv.ParseFloat(parts[9], 64); err == nil {
		data.Altitude = alt
	}

	// Parse time
	if timeStr := parts[1]; len(timeStr) >= 6 {
		data.Timestamp = rs.parseNMEATime(timeStr)
	} else {
		data.Timestamp = time.Now()
	}

	return data
}

// parseRMC parses GPRMC NMEA sentence
func (rs *RUTOSGPSSource) parseRMC(sentence string) *NMEAData {
	parts := strings.Split(sentence, ",")
	if len(parts) < 12 {
		return nil
	}

	data := &NMEAData{Source: "GPRMC"}

	// Check validity
	data.Valid = parts[2] == "A"
	if !data.Valid {
		return data
	}

	// Parse coordinates
	if lat := rs.parseCoordinate(parts[3], parts[4]); lat != 0 {
		data.Latitude = lat
	}
	if lon := rs.parseCoordinate(parts[5], parts[6]); lon != 0 {
		data.Longitude = lon
	}

	// Parse speed (knots to m/s)
	if speed, err := strconv.ParseFloat(parts[7], 64); err == nil {
		data.Speed = speed * 0.514444 // Convert knots to m/s
	}

	// Parse course
	if course, err := strconv.ParseFloat(parts[8], 64); err == nil {
		data.Course = course
	}

	// Parse time
	if timeStr := parts[1]; len(timeStr) >= 6 {
		data.Timestamp = rs.parseNMEATime(timeStr)
	} else {
		data.Timestamp = time.Now()
	}

	return data
}

// parseCoordinate converts NMEA coordinate format to decimal degrees
func (rs *RUTOSGPSSource) parseCoordinate(coordStr, dirStr string) float64 {
	if coordStr == "" || dirStr == "" {
		return 0
	}

	coord, err := strconv.ParseFloat(coordStr, 64)
	if err != nil {
		return 0
	}

	// Convert DDMM.MMMM to decimal degrees
	degrees := math.Floor(coord / 100)
	minutes := coord - (degrees * 100)
	decimal := degrees + (minutes / 60)

	// Apply direction
	if dirStr == "S" || dirStr == "W" {
		decimal = -decimal
	}

	return decimal
}

// parseNMEATime parses NMEA time format HHMMSS
func (rs *RUTOSGPSSource) parseNMEATime(timeStr string) time.Time {
	if len(timeStr) < 6 {
		return time.Now()
	}

	hour, _ := strconv.Atoi(timeStr[0:2])
	minute, _ := strconv.Atoi(timeStr[2:4])
	second, _ := strconv.Atoi(timeStr[4:6])

	now := time.Now()
	return time.Date(now.Year(), now.Month(), now.Day(), hour, minute, second, 0, time.UTC)
}

// convertNMEAToStandardized converts NMEA data to standardized format
func (rs *RUTOSGPSSource) convertNMEAToStandardized(nmea *NMEAData) *StandardizedGPSData {
	data := &StandardizedGPSData{
		Latitude:    nmea.Latitude,
		Longitude:   nmea.Longitude,
		Altitude:    nmea.Altitude,
		Speed:       nmea.Speed,
		Course:      nmea.Course,
		HDOP:        nmea.HDOP,
		VDOP:        nmea.VDOP,
		Satellites:  nmea.Satellites,
		Source:      "RUTOS NMEA",
		Method:      "nmea_direct",
		DataSources: []string{nmea.Source},
		Valid:       nmea.Valid,
		Timestamp:   nmea.Timestamp,
	}

	// Calculate accuracy from HDOP
	if nmea.HDOP > 0 {
		data.Accuracy = nmea.HDOP * 5 // Rough conversion: HDOP * 5 = accuracy in meters
	} else {
		data.Accuracy = 10.0 // Default accuracy
	}

	// Use actual fix type from NMEA data instead of calculating
	data.FixType = nmea.FixType
	data.FixQuality = rs.determineFixQuality(data.Accuracy, data.Satellites)
	data.Confidence = rs.calculateConfidence(data.Accuracy, data.Satellites)

	return data
}

// parseATGPSResponse parses AT command GPS response
func (rs *RUTOSGPSSource) parseATGPSResponse(output string) (*StandardizedGPSData, error) {
	lines := strings.Split(output, "\n")
	for _, line := range lines {
		if strings.Contains(line, "+QGPSLOC:") {
			// Parse QGPSLOC response
			// Format: +QGPSLOC: <UTC>,<latitude>,<longitude>,<hdop>,<altitude>,<fix>,<cog>,<spkm>,<spkn>,<date>,<nsat>
			parts := strings.Split(strings.TrimPrefix(line, "+QGPSLOC: "), ",")
			if len(parts) >= 11 {
				lat, _ := strconv.ParseFloat(parts[1], 64)
				lon, _ := strconv.ParseFloat(parts[2], 64)
				hdop, _ := strconv.ParseFloat(parts[3], 64)
				alt, _ := strconv.ParseFloat(parts[4], 64)
				fix, _ := strconv.Atoi(parts[5])
				course, _ := strconv.ParseFloat(parts[6], 64)
				speedKmh, _ := strconv.ParseFloat(parts[7], 64)
				sats, _ := strconv.Atoi(parts[10])

				if lat != 0 && lon != 0 && fix > 0 {
					data := &StandardizedGPSData{
						Latitude:    lat,
						Longitude:   lon,
						Altitude:    alt,
						Speed:       speedKmh / 3.6, // Convert km/h to m/s
						Course:      course,
						HDOP:        hdop,
						Satellites:  sats,
						Source:      "RUTOS AT",
						Method:      "at_command",
						DataSources: []string{"AT+QGPSLOC"},
						Valid:       true,
						Timestamp:   time.Now(),
					}

					data.Accuracy = hdop * 5 // Convert HDOP to accuracy estimate
					// Use actual fix type from AT command response (already 0-3 scale)
					data.FixType = fix
					data.FixQuality = rs.determineFixQuality(data.Accuracy, data.Satellites)
					data.Confidence = rs.calculateConfidence(data.Accuracy, data.Satellites)

					return data, nil
				}
			}
		}
	}

	return nil, fmt.Errorf("no valid GPS data in AT response")
}

// parseGsmctlGPSOutput parses gsmctl GPS output
func (rs *RUTOSGPSSource) parseGsmctlGPSOutput(output string) (*StandardizedGPSData, error) {
	lines := strings.Split(output, "\n")
	for _, line := range lines {
		if strings.Contains(line, "+CGPSINFO:") {
			// Parse CGPSINFO response
			parts := strings.Split(strings.TrimPrefix(line, "+CGPSINFO: "), ",")
			if len(parts) >= 9 {
				lat, _ := strconv.ParseFloat(parts[0], 64)
				lon, _ := strconv.ParseFloat(parts[2], 64)
				alt, _ := strconv.ParseFloat(parts[6], 64)

				if lat != 0 && lon != 0 {
					// Convert from DDMM.MMMM to decimal degrees
					lat = rs.convertToDecimalDegrees(lat)
					lon = rs.convertToDecimalDegrees(lon)

					data := &StandardizedGPSData{
						Latitude:    lat,
						Longitude:   lon,
						Altitude:    alt,
						Accuracy:    10.0, // Assume 10m accuracy
						Source:      "RUTOS GSM",
						Method:      "gsmctl",
						DataSources: []string{"AT+CGPSINFO"},
						Valid:       true,
						Timestamp:   time.Now(),
					}

					data.FixType = rs.determineFixType(data.Accuracy, data.Satellites)
					data.FixQuality = rs.determineFixQuality(data.Accuracy, data.Satellites)
					data.Confidence = rs.calculateConfidence(data.Accuracy, data.Satellites)

					return data, nil
				}
			}
		}
	}

	return nil, fmt.Errorf("no valid GPS data in gsmctl output")
}

// parseUbusGPSResponse parses ubus GPS response
func (rs *RUTOSGPSSource) parseUbusGPSResponse(response map[string]interface{}) (*StandardizedGPSData, error) {
	data := &StandardizedGPSData{
		Source:      "RUTOS ubus",
		Method:      "ubus",
		DataSources: []string{"ubus"},
		Timestamp:   time.Now(),
	}

	if lat, ok := response["latitude"].(float64); ok {
		data.Latitude = lat
	}
	if lon, ok := response["longitude"].(float64); ok {
		data.Longitude = lon
	}
	if alt, ok := response["altitude"].(float64); ok {
		data.Altitude = alt
	}
	if acc, ok := response["accuracy"].(float64); ok {
		data.Accuracy = acc
	}
	if sats, ok := response["satellites"].(float64); ok {
		data.Satellites = int(sats)
	}

	data.Valid = data.Latitude != 0 && data.Longitude != 0

	if data.Valid {
		data.FixType = rs.determineFixType(data.Accuracy, data.Satellites)
		data.FixQuality = rs.determineFixQuality(data.Accuracy, data.Satellites)
		data.Confidence = rs.calculateConfidence(data.Accuracy, data.Satellites)
		return data, nil
	}

	return nil, fmt.Errorf("invalid GPS data from ubus")
}

// convertNMEAFixQualityToFixType converts NMEA fix quality (0-8) to standard fix type (0-3)
func (rs *RUTOSGPSSource) convertNMEAFixQualityToFixType(nmeaQuality int) int {
	switch nmeaQuality {
	case 0:
		return 0 // Invalid/No fix
	case 1:
		return 2 // GPS fix (assume 3D)
	case 2:
		return 3 // DGPS fix
	case 3:
		return 3 // PPS fix (high accuracy)
	case 4, 5:
		return 3 // RTK fixes (highest accuracy)
	case 6:
		return 1 // Estimated (lower quality, assume 2D)
	case 7, 8:
		return 1 // Manual/Simulation (lower quality)
	default:
		return 0 // Unknown
	}
}

// Helper functions for GPS quality assessment
func (rs *RUTOSGPSSource) determineFixType(accuracy float64, satellites int) int {
	if satellites >= 4 && accuracy <= 5 {
		return 3 // DGPS fix
	} else if satellites >= 4 && accuracy <= 15 {
		return 2 // 3D fix
	} else if satellites >= 3 {
		return 1 // 2D fix
	}
	return 0 // No fix
}

func (rs *RUTOSGPSSource) determineFixQuality(accuracy float64, satellites int) string {
	if accuracy <= 2 && satellites >= 8 {
		return "excellent"
	} else if accuracy <= 5 && satellites >= 6 {
		return "good"
	} else if accuracy <= 15 && satellites >= 4 {
		return "fair"
	}
	return "poor"
}

func (rs *RUTOSGPSSource) calculateConfidence(accuracy float64, satellites int) float64 {
	// Base confidence on accuracy and satellite count
	accuracyScore := math.Max(0, 1.0-(accuracy/50.0))         // 0-1 scale, 50m = 0 confidence
	satelliteScore := math.Min(1.0, float64(satellites)/12.0) // 0-1 scale, 12+ sats = full confidence

	return (accuracyScore + satelliteScore) / 2.0
}

// convertToDecimalDegrees converts DDMM.MMMM to decimal degrees
func (rs *RUTOSGPSSource) convertToDecimalDegrees(coord float64) float64 {
	degrees := math.Floor(coord / 100)
	minutes := coord - (degrees * 100)
	return degrees + (minutes / 60)
}
