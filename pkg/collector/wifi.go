package collector

import (
	"context"
	"encoding/json"
	"fmt"
	"os"
	"os/exec"
	"strconv"
	"strings"
	"time"

	"github.com/markus-lassfolk/autonomy/pkg"
	"github.com/markus-lassfolk/autonomy/pkg/logx"
)

// WiFiCollector collects metrics from WiFi interfaces
type WiFiCollector struct {
	*BaseCollector
	ubusPath string
	timeout  time.Duration
}

// WiFiInfo represents WiFi connection information
type WiFiInfo struct {
	SignalStrength *int    `json:"signal,omitempty"`
	NoiseLevel     *int    `json:"noise,omitempty"`
	SNR            *int    `json:"snr,omitempty"`
	Bitrate        *int    `json:"bitrate,omitempty"`
	SSID           *string `json:"ssid,omitempty"`
	Channel        *int    `json:"channel,omitempty"`
	Mode           *string `json:"mode,omitempty"`
	Frequency      *int    `json:"frequency,omitempty"`
	Quality        *int    `json:"quality,omitempty"`
	LinkQuality    *int    `json:"link_quality,omitempty"`
	TxPower        *int    `json:"tx_power,omitempty"`
	Encryption     *string `json:"encryption,omitempty"`
	Country        *string `json:"country,omitempty"`
	TetheringMode  *bool   `json:"tethering_mode,omitempty"`
}

// NewWiFiCollector creates a new WiFi collector
func NewWiFiCollector(config map[string]interface{}) (*WiFiCollector, error) {
	timeout := 5 * time.Second
	if t, ok := config["timeout"].(time.Duration); ok {
		timeout = t
	}

	ubusPath := "ubus"
	if p, ok := config["ubus_path"].(string); ok {
		ubusPath = p
	}

	targets := []string{"8.8.8.8", "1.1.1.1"}
	if t, ok := config["targets"].([]string); ok {
		targets = t
	}

	logger := &logx.Logger{} // Default logger for now
	collector := &WiFiCollector{
		BaseCollector: NewBaseCollector(timeout, targets, logger),
		ubusPath:      ubusPath,
		timeout:       timeout,
	}

	// Enable predictive analysis for WiFi interfaces
	predictiveEnabled := true
	if p, ok := config["predictive_enabled"].(bool); ok {
		predictiveEnabled = p
	}

	if predictiveEnabled {
		collector.BaseCollector.EnablePredictive("wifi")
	}

	return collector, nil
}

// Collect collects metrics from WiFi interface
func (wc *WiFiCollector) Collect(ctx context.Context, member *pkg.Member) (*pkg.Metrics, error) {
	if err := wc.Validate(member); err != nil {
		return nil, err
	}

	// Start with common metrics
	metrics, err := wc.CollectCommonMetrics(ctx, member)
	if err != nil {
		return nil, err
	}

	// Collect WiFi-specific metrics
	wifiInfo, err := wc.collectWiFiInfo(ctx, member)
	if err != nil {
		// Log error but don't fail - continue with common metrics
		fmt.Printf("Warning: Failed to collect WiFi metrics for %s: %v\n", member.Name, err)
	} else {
		// Merge WiFi metrics
		metrics.SignalStrength = wifiInfo.SignalStrength
		metrics.NoiseLevel = wifiInfo.NoiseLevel
		metrics.SNR = wifiInfo.SNR
		metrics.Bitrate = wifiInfo.Bitrate

		// Add extended WiFi metrics to the metrics structure
		if wifiInfo.Quality != nil {
			metrics.Quality = wifiInfo.Quality
		}
		if wifiInfo.LinkQuality != nil {
			metrics.LinkQuality = wifiInfo.LinkQuality
		}
		if wifiInfo.TxPower != nil {
			metrics.TxPower = wifiInfo.TxPower
		}
		if wifiInfo.Frequency != nil {
			metrics.Frequency = wifiInfo.Frequency
		}
		if wifiInfo.Channel != nil {
			metrics.Channel = wifiInfo.Channel
		}
		if wifiInfo.TetheringMode != nil {
			metrics.TetheringMode = wifiInfo.TetheringMode
		}
	}

	// Perform predictive analysis
	if err := wc.BaseCollector.PerformPredictiveAnalysis(ctx, metrics); err != nil {
		wc.BaseCollector.logger.Warn("Predictive analysis failed", "error", err)
	}

	return metrics, nil
}

// collectWiFiInfo collects WiFi-specific information via ubus iwinfo
func (wc *WiFiCollector) collectWiFiInfo(ctx context.Context, member *pkg.Member) (*WiFiInfo, error) {
	info := &WiFiInfo{}

	// Try to get WiFi info via ubus iwinfo
	if wifiData, err := wc.queryIwinfo(ctx, member.Iface); err == nil {
		if err := wc.parseWiFiData(wifiData, info); err == nil {
			return info, nil
		}
	}

	// Fallback: try to get basic info from /proc/net/wireless
	if err := wc.collectFallbackInfo(ctx, member, info); err != nil {
		return nil, fmt.Errorf("failed to collect WiFi info: %w", err)
	}

	return info, nil
}

// queryIwinfo queries WiFi information via ubus iwinfo
func (wc *WiFiCollector) queryIwinfo(ctx context.Context, iface string) (map[string]interface{}, error) {
	// Create context with timeout
	ctx, cancel := context.WithTimeout(ctx, wc.timeout)
	defer cancel()

	// Try to call ubus iwinfo
	cmd := exec.CommandContext(ctx, wc.ubusPath, "call", "iwinfo", "info", fmt.Sprintf(`{"device":"%s"}`, iface))
	output, err := cmd.Output()
	if err != nil {
		return nil, fmt.Errorf("ubus iwinfo call failed: %w", err)
	}

	// Parse JSON response
	var result map[string]interface{}
	if err := json.Unmarshal(output, &result); err != nil {
		return nil, fmt.Errorf("failed to parse iwinfo response: %w", err)
	}

	return result, nil
}

// parseWiFiData parses WiFi data from ubus response
func (wc *WiFiCollector) parseWiFiData(data map[string]interface{}, info *WiFiInfo) error {
	// Try to extract signal strength
	if signal, ok := data["signal"].(float64); ok {
		signalInt := int(signal)
		info.SignalStrength = &signalInt
	}

	// Try to extract noise level
	if noise, ok := data["noise"].(float64); ok {
		noiseInt := int(noise)
		info.NoiseLevel = &noiseInt
	}

	// Try to extract bitrate
	if bitrate, ok := data["bitrate"].(float64); ok {
		bitrateInt := int(bitrate)
		info.Bitrate = &bitrateInt
	}

	// Try to extract SSID
	if ssid, ok := data["ssid"].(string); ok {
		info.SSID = &ssid
	}

	// Try to extract channel
	if channel, ok := data["channel"].(float64); ok {
		channelInt := int(channel)
		info.Channel = &channelInt
	}

	// Try to extract mode
	if mode, ok := data["mode"].(string); ok {
		info.Mode = &mode
	}

	// Try to extract frequency
	if frequency, ok := data["frequency"].(float64); ok {
		frequencyInt := int(frequency)
		info.Frequency = &frequencyInt
	}

	// Try to extract quality
	if quality, ok := data["quality"].(float64); ok {
		qualityInt := int(quality)
		info.Quality = &qualityInt
	}

	// Try to extract link quality
	if linkQuality, ok := data["quality_max"].(float64); ok {
		linkQualityInt := int(linkQuality)
		info.LinkQuality = &linkQualityInt
	}

	// Try to extract TX power
	if txPower, ok := data["txpower"].(float64); ok {
		txPowerInt := int(txPower)
		info.TxPower = &txPowerInt
	}

	// Try to extract encryption
	if encryption, ok := data["encryption"].(map[string]interface{}); ok {
		if encType, ok := encryption["enabled"].(bool); ok && encType {
			if ciphers, ok := encryption["ciphers"].([]interface{}); ok && len(ciphers) > 0 {
				if cipher, ok := ciphers[0].(string); ok {
					info.Encryption = &cipher
				}
			} else {
				encStr := "enabled"
				info.Encryption = &encStr
			}
		} else {
			encStr := "none"
			info.Encryption = &encStr
		}
	}

	// Try to extract country code
	if country, ok := data["country"].(string); ok {
		info.Country = &country
	}

	// Detect tethering mode (AP mode typically indicates tethering)
	if info.Mode != nil {
		isAP := strings.ToLower(*info.Mode) == "ap" || strings.ToLower(*info.Mode) == "master"
		info.TetheringMode = &isAP
	}

	// Calculate SNR if we have both signal and noise
	if info.SignalStrength != nil && info.NoiseLevel != nil {
		snr := *info.SignalStrength - *info.NoiseLevel
		info.SNR = &snr
	}

	// Calculate quality percentage if we have quality and max quality
	if info.Quality != nil && info.LinkQuality != nil && *info.LinkQuality > 0 {
		qualityPct := (*info.Quality * 100) / *info.LinkQuality
		info.Quality = &qualityPct
	}

	return nil
}

// collectFallbackInfo collects basic WiFi info from /proc/net/wireless
func (wc *WiFiCollector) collectFallbackInfo(ctx context.Context, member *pkg.Member, info *WiFiInfo) error {
	// Try to read from /proc/net/wireless
	if wirelessData, err := wc.readWirelessFile(); err == nil {
		if wifiData := wc.parseWirelessFile(wirelessData, member.Iface); wifiData != nil {
			info.SignalStrength = wifiData.SignalStrength
			info.NoiseLevel = wifiData.NoiseLevel
			info.SNR = wifiData.SNR
			info.Bitrate = wifiData.Bitrate
		} else {
			fmt.Printf("Debug: no wireless stats found for %s\n", member.Iface)
		}
	} else {
		fmt.Printf("Warning: failed to read /proc/net/wireless: %v\n", err)
	}

	return nil
}

// readWirelessFile reads /proc/net/wireless
func (wc *WiFiCollector) readWirelessFile() (string, error) {
	data, err := os.ReadFile("/proc/net/wireless")
	if err != nil {
		return "", fmt.Errorf("read /proc/net/wireless: %w", err)
	}
	return string(data), nil
}

// parseWirelessFile parses /proc/net/wireless format
func (wc *WiFiCollector) parseWirelessFile(data, iface string) *WiFiInfo {
	lines := strings.Split(data, "\n")
	for _, line := range lines {
		line = strings.TrimSpace(line)
		if !strings.HasPrefix(line, iface) {
			continue
		}

		fields := strings.Fields(line)
		// Expected format: iface: status quality level noise nwid crypt frag retry misc beacon
		if len(fields) < 5 {
			return nil
		}

		info := &WiFiInfo{}

		// Parse quality (field 2)
		if len(fields) > 2 {
			qualityStr := strings.TrimSuffix(fields[2], ".")
			if qualityF, err := strconv.ParseFloat(qualityStr, 64); err == nil {
				quality := int(qualityF)
				info.Quality = &quality
			}
		}

		// Parse signal level (field 3)
		if len(fields) > 3 {
			levelStr := strings.TrimSuffix(fields[3], ".")
			if levelF, err := strconv.ParseFloat(levelStr, 64); err == nil {
				level := int(levelF)
				info.SignalStrength = &level
			}
		}

		// Parse noise level (field 4)
		if len(fields) > 4 {
			noiseStr := strings.TrimSuffix(fields[4], ".")
			if noiseF, err := strconv.ParseFloat(noiseStr, 64); err == nil {
				noise := int(noiseF)
				info.NoiseLevel = &noise
			}
		}

		// Calculate SNR if we have both signal and noise
		if info.SignalStrength != nil && info.NoiseLevel != nil {
			snr := *info.SignalStrength - *info.NoiseLevel
			info.SNR = &snr
		}

		// Try to get additional info from iwconfig as fallback
		if info.Bitrate == nil {
			if bitrate := wc.getIwconfigBitrate(iface); bitrate != nil {
				info.Bitrate = bitrate
			}
		}

		return info
	}

	return nil
}

// getIwconfigBitrate tries to get bitrate from iwconfig command
func (wc *WiFiCollector) getIwconfigBitrate(iface string) *int {
	// Validate iface to prevent command injection
	if strings.ContainsAny(iface, ";&|`$(){}[]<>\"'\\") {
		return nil
	}

	cmd := exec.Command("iwconfig", iface)
	output, err := cmd.Output()
	if err != nil {
		return nil
	}

	// Parse iwconfig output for bitrate
	lines := strings.Split(string(output), "\n")
	for _, line := range lines {
		if strings.Contains(line, "Bit Rate") {
			// Look for pattern like "Bit Rate=54 Mb/s"
			parts := strings.Split(line, "Bit Rate=")
			if len(parts) > 1 {
				ratePart := strings.Fields(parts[1])[0]
				if rateF, err := strconv.ParseFloat(ratePart, 64); err == nil {
					// Convert Mb/s to bps
					rateBps := int(rateF * 1000000)
					return &rateBps
				}
			}
		}
	}

	return nil
}

// Validate validates a member for the WiFi collector
func (wc *WiFiCollector) Validate(member *pkg.Member) error {
	if err := wc.BaseCollector.Validate(member); err != nil {
		return err
	}

	// Additional WiFi-specific validation
	if member.Class != pkg.ClassWiFi {
		return fmt.Errorf("member class must be wifi, got %s", member.Class)
	}

	return nil
}

// TestWiFiConnectivity tests if we can get WiFi information
func (wc *WiFiCollector) TestWiFiConnectivity(ctx context.Context, member *pkg.Member) error {
	// Try to get basic WiFi info
	info, err := wc.collectWiFiInfo(ctx, member)
	if err != nil {
		return fmt.Errorf("failed to collect WiFi info: %w", err)
	}

	// Check if we got any meaningful data
	if info.SignalStrength == nil && info.SNR == nil {
		return fmt.Errorf("no WiFi metrics available")
	}

	return nil
}

// GetWiFiInfo returns detailed WiFi information
func (wc *WiFiCollector) GetWiFiInfo(ctx context.Context, member *pkg.Member) (map[string]interface{}, error) {
	info, err := wc.collectWiFiInfo(ctx, member)
	if err != nil {
		return nil, err
	}

	result := make(map[string]interface{})

	if info.SignalStrength != nil {
		result["signal"] = *info.SignalStrength
	}
	if info.NoiseLevel != nil {
		result["noise"] = *info.NoiseLevel
	}
	if info.SNR != nil {
		result["snr"] = *info.SNR
	}
	if info.Bitrate != nil {
		result["bitrate"] = *info.Bitrate
	}
	if info.SSID != nil {
		result["ssid"] = *info.SSID
	}
	if info.Channel != nil {
		result["channel"] = *info.Channel
	}
	if info.Mode != nil {
		result["mode"] = *info.Mode
	}

	return result, nil
}

// GetSignalQuality returns a signal quality score based on WiFi metrics
func (wc *WiFiCollector) GetSignalQuality(signal, noise, snr *int) float64 {
	// Calculate signal quality score (0-100)
	score := 50.0 // Base score

	if signal != nil {
		// WiFi signal strength typically ranges from -100 to -30 dBm
		// Convert to 0-100 scale
		signalScore := float64(*signal+100) / 70.0 * 100
		if signalScore > 100 {
			signalScore = 100
		} else if signalScore < 0 {
			signalScore = 0
		}
		score = score*0.3 + signalScore*0.7
	}

	if snr != nil {
		// SNR typically ranges from 0 to 40 dB
		// Convert to 0-100 scale
		snrScore := float64(*snr) / 40.0 * 100
		if snrScore > 100 {
			snrScore = 100
		} else if snrScore < 0 {
			snrScore = 0
		}
		score = score*0.6 + snrScore*0.4
	}

	return score
}

// GetBitrateQuality returns a bitrate quality score
func (wc *WiFiCollector) GetBitrateQuality(bitrate *int) float64 {
	if bitrate == nil {
		return 50.0 // Default score
	}

	// Bitrate quality based on typical WiFi speeds
	// Assuming 802.11n/g speeds
	bitrateMbps := float64(*bitrate) / 1000000 // Convert to Mbps

	if bitrateMbps >= 100 {
		return 100.0 // Excellent
	} else if bitrateMbps >= 50 {
		return 80.0 // Good
	} else if bitrateMbps >= 25 {
		return 60.0 // Fair
	} else if bitrateMbps >= 10 {
		return 40.0 // Poor
	} else {
		return 20.0 // Very poor
	}
}

// IsWiFiInterface checks if an interface is a WiFi interface
func (wc *WiFiCollector) IsWiFiInterface(iface string) bool {
	// Check if interface name matches WiFi patterns
	wifiPatterns := []string{"wlan", "wifi", "ath", "radio"}

	ifaceLower := strings.ToLower(iface)
	for _, pattern := range wifiPatterns {
		if strings.Contains(ifaceLower, pattern) {
			return true
		}
	}

	return false
}

// GetWiFiChannels returns available WiFi channels for the interface
func (wc *WiFiCollector) GetWiFiChannels(ctx context.Context, iface string) ([]int, error) {
	// Try to get channel information via ubus
	if wifiData, err := wc.queryIwinfo(ctx, iface); err == nil {
		if channels, ok := wifiData["channels"].([]interface{}); ok {
			result := make([]int, 0, len(channels))
			for _, ch := range channels {
				if channel, ok := ch.(float64); ok {
					result = append(result, int(channel))
				}
			}
			return result, nil
		}
	}

	return nil, fmt.Errorf("failed to get WiFi channels")
}

// GetWiFiModes returns available WiFi modes for the interface
func (wc *WiFiCollector) GetWiFiModes(ctx context.Context, iface string) ([]string, error) {
	// Try to get mode information via ubus
	if wifiData, err := wc.queryIwinfo(ctx, iface); err == nil {
		if modes, ok := wifiData["modes"].([]interface{}); ok {
			result := make([]string, 0, len(modes))
			for _, m := range modes {
				if mode, ok := m.(string); ok {
					result = append(result, mode)
				}
			}
			return result, nil
		}
	}

	return nil, fmt.Errorf("failed to get WiFi modes")
}

// DetectTetheringMode detects if the WiFi interface is in tethering/AP mode
func (wc *WiFiCollector) DetectTetheringMode(ctx context.Context, member *pkg.Member) (bool, error) {
	info, err := wc.collectWiFiInfo(ctx, member)
	if err != nil {
		return false, err
	}

	// Check if we already detected tethering mode
	if info.TetheringMode != nil {
		return *info.TetheringMode, nil
	}

	// Additional checks for tethering detection
	// 1. Check interface mode
	if info.Mode != nil {
		mode := strings.ToLower(*info.Mode)
		if mode == "ap" || mode == "master" || mode == "hostap" {
			return true, nil
		}
	}

	// 2. Check if interface has DHCP server running
	if wc.hasActiveDHCPServer(member.Iface) {
		return true, nil
	}

	// 3. Check if interface is in bridge mode
	if wc.isInBridgeMode(member.Iface) {
		return true, nil
	}

	return false, nil
}

// hasActiveDHCPServer checks if there's an active DHCP server on the interface
func (wc *WiFiCollector) hasActiveDHCPServer(iface string) bool {
	// Check if dnsmasq is running with this interface
	cmd := exec.Command("ps", "aux")
	output, err := cmd.Output()
	if err != nil {
		return false
	}

	lines := strings.Split(string(output), "\n")
	for _, line := range lines {
		if strings.Contains(line, "dnsmasq") && strings.Contains(line, iface) {
			return true
		}
	}

	// Check if hostapd is running for this interface
	cmd = exec.Command("ps", "aux")
	output, err = cmd.Output()
	if err != nil {
		return false
	}

	lines = strings.Split(string(output), "\n")
	for _, line := range lines {
		if strings.Contains(line, "hostapd") && strings.Contains(line, iface) {
			return true
		}
	}

	return false
}

// isInBridgeMode checks if the interface is part of a bridge
func (wc *WiFiCollector) isInBridgeMode(iface string) bool {
	// Check if interface is in a bridge
	bridgeFile := fmt.Sprintf("/sys/class/net/%s/brport/bridge", iface)
	if _, err := os.Stat(bridgeFile); err == nil {
		return true
	}

	// Check if interface is a bridge itself
	bridgeDir := fmt.Sprintf("/sys/class/net/%s/bridge", iface)
	if _, err := os.Stat(bridgeDir); err == nil {
		return true
	}

	return false
}

// AnalyzeSignalTrend analyzes signal strength trends over time
func (wc *WiFiCollector) AnalyzeSignalTrend(recentMetrics []*pkg.Metrics) map[string]interface{} {
	if len(recentMetrics) < 3 {
		return map[string]interface{}{
			"trend":      "insufficient_data",
			"confidence": 0.0,
		}
	}

	// Calculate signal strength trend
	var signalValues []float64
	for _, metric := range recentMetrics {
		if metric.SignalStrength != nil {
			signalValues = append(signalValues, float64(*metric.SignalStrength))
		}
	}

	if len(signalValues) < 3 {
		return map[string]interface{}{
			"trend":      "no_signal_data",
			"confidence": 0.0,
		}
	}

	// Simple linear trend calculation
	n := float64(len(signalValues))
	sumX := n * (n - 1) / 2 // Sum of indices 0, 1, 2, ...
	sumY := 0.0
	sumXY := 0.0
	sumXX := (n - 1) * n * (2*n - 1) / 6

	for i, y := range signalValues {
		sumY += y
		sumXY += float64(i) * y
	}

	// Calculate slope (trend)
	slope := (n*sumXY - sumX*sumY) / (n*sumXX - sumX*sumX)

	// Determine trend direction and confidence
	trend := "stable"
	confidence := 0.5

	if slope > 1.0 {
		trend = "improving"
		confidence = 0.8
	} else if slope < -1.0 {
		trend = "degrading"
		confidence = 0.8
	}

	// Calculate signal quality assessment
	avgSignal := sumY / n
	var quality string

	if avgSignal > -50 {
		quality = "excellent"
	} else if avgSignal > -60 {
		quality = "good"
	} else if avgSignal > -70 {
		quality = "fair"
	} else if avgSignal > -80 {
		quality = "poor"
	} else {
		quality = "very_poor"
	}

	return map[string]interface{}{
		"trend":          trend,
		"confidence":     confidence,
		"slope":          slope,
		"average_signal": avgSignal,
		"signal_quality": quality,
		"sample_count":   len(signalValues),
	}
}

// GetAdvancedWiFiMetrics returns comprehensive WiFi analysis
func (wc *WiFiCollector) GetAdvancedWiFiMetrics(ctx context.Context, member *pkg.Member, recentMetrics []*pkg.Metrics) (map[string]interface{}, error) {
	// Get basic WiFi info
	info, err := wc.collectWiFiInfo(ctx, member)
	if err != nil {
		return nil, err
	}

	// Get detailed info
	detailedInfo, _ := wc.GetWiFiInfo(ctx, member)

	// Analyze signal trends
	trendAnalysis := wc.AnalyzeSignalTrend(recentMetrics)

	// Detect tethering mode
	isTetheringMode, _ := wc.DetectTetheringMode(ctx, member)

	// Calculate comprehensive quality score
	signalQuality := wc.GetSignalQuality(info.SignalStrength, info.NoiseLevel, info.SNR)
	bitrateQuality := wc.GetBitrateQuality(info.Bitrate)

	overallQuality := (signalQuality*0.6 + bitrateQuality*0.4)

	result := map[string]interface{}{
		"basic_info":      detailedInfo,
		"trend_analysis":  trendAnalysis,
		"tethering_mode":  isTetheringMode,
		"signal_quality":  signalQuality,
		"bitrate_quality": bitrateQuality,
		"overall_quality": overallQuality,
		"timestamp":       time.Now().Format(time.RFC3339),
	}

	return result, nil
}
