package wifi

import (
	"context"
	"encoding/json"
	"fmt"
	"math"
	"os/exec"
	"sort"

	"github.com/markus-lassfolk/autonomy/pkg/logx"
)

// EnhancedWiFiScanner leverages RUTOS built-in iwinfo for sophisticated channel analysis
type EnhancedWiFiScanner struct {
	logger *logx.Logger
}

// UbusWiFiAccessPoint represents a WiFi AP from ubus iwinfo scan
type UbusWiFiAccessPoint struct {
	SSID        string `json:"ssid"`
	BSSID       string `json:"bssid"`
	Channel     int    `json:"channel"`
	Signal      int    `json:"signal"` // dBm (negative)
	HTMode      string `json:"htmode"` // "HT20","HT40","VHT80","HE80"...
	Bandwidth   int    `json:"bandwidth,omitempty"`
	Frequency   int64  `json:"frequency"`
	CenterChan1 int    `json:"center_chan1,omitempty"`
	CenterChan2 int    `json:"center_chan2,omitempty"`
}

// UbusScanResult contains scan results from ubus
type UbusScanResult struct {
	Results []UbusWiFiAccessPoint `json:"results"`
}

// UbusSurveyItem represents channel utilization data
type UbusSurveyItem struct {
	Frequency  int64 `json:"frequency"`
	Channel    int   `json:"channel"`
	Noise      int   `json:"noise"`
	ActiveTime int64 `json:"active_time"`
	BusyTime   int64 `json:"busy_time"`
	RxTime     int64 `json:"rx_time"`
	TxTime     int64 `json:"tx_time"`
}

// UbusSurveyResult contains survey results from ubus
type UbusSurveyResult struct {
	Survey []UbusSurveyItem `json:"survey"`
}

// EnhancedChannelScore represents comprehensive channel analysis
type EnhancedChannelScore struct {
	Band               string  `json:"band"`
	Channel            int     `json:"channel"`
	APCount            int     `json:"ap_count"`
	CoChannelPenalty   int     `json:"co_channel_penalty"`
	OverlapPenalty     int     `json:"overlap_penalty"`
	UtilizationPenalty int     `json:"utilization_penalty"`
	Utilization        float64 `json:"utilization"` // 0-1
	RawScore           int     `json:"raw_score"`   // 0-100
	Stars              int     `json:"stars"`       // 1-5
	Recommendation     string  `json:"recommendation"`
	StrongInterferers  int     `json:"strong_interferers"` // RSSI >= -60dBm
	WeakInterferers    int     `json:"weak_interferers"`   // RSSI < -70dBm
}

// NewEnhancedWiFiScanner creates a new enhanced scanner
func NewEnhancedWiFiScanner(logger *logx.Logger) *EnhancedWiFiScanner {
	return &EnhancedWiFiScanner{
		logger: logger,
	}
}

// ScanAndRateChannels performs comprehensive channel analysis using RUTOS built-in tools
func (s *EnhancedWiFiScanner) ScanAndRateChannels(ctx context.Context, device string) ([]EnhancedChannelScore, error) {
	s.logger.Info("Starting enhanced WiFi channel analysis", "device", device)

	// Step 1: Perform scan using ubus iwinfo
	scanResult, err := s.performUbusScan(ctx, device)
	if err != nil {
		return nil, fmt.Errorf("failed to perform ubus scan: %w", err)
	}

	// Step 2: Get channel utilization survey (if available)
	utilization, err := s.getChannelUtilization(ctx, device)
	if err != nil {
		s.logger.Warn("Channel utilization not available, using AP-based scoring only", "error", err)
		utilization = make(map[int]float64)
	}

	// Step 3: Analyze and score channels
	scores := s.analyzeChannels(scanResult.Results, utilization, device)

	// Step 4: Sort by score (highest first)
	sort.Slice(scores, func(i, j int) bool {
		return scores[i].RawScore > scores[j].RawScore
	})

	s.logger.Info("Enhanced channel analysis completed",
		"device", device,
		"channels_analyzed", len(scores),
		"best_channel", scores[0].Channel,
		"best_score", scores[0].RawScore,
		"best_stars", scores[0].Stars)

	return scores, nil
}

// performUbusScan executes ubus iwinfo scan
func (s *EnhancedWiFiScanner) performUbusScan(ctx context.Context, device string) (*UbusScanResult, error) {
	cmd := exec.CommandContext(ctx, "ubus", "-S", "-t", "30", "call", "iwinfo", "scan",
		fmt.Sprintf(`{"device":"%s"}`, device))

	output, err := cmd.Output()
	if err != nil {
		return nil, fmt.Errorf("ubus scan failed: %w", err)
	}

	var result UbusScanResult
	if err := json.Unmarshal(output, &result); err != nil {
		return nil, fmt.Errorf("failed to parse scan results: %w", err)
	}

	s.logger.Debug("Ubus scan completed",
		"device", device,
		"aps_found", len(result.Results))

	return &result, nil
}

// getChannelUtilization gets channel utilization via ubus survey
func (s *EnhancedWiFiScanner) getChannelUtilization(ctx context.Context, device string) (map[int]float64, error) {
	cmd := exec.CommandContext(ctx, "ubus", "-S", "-t", "10", "call", "iwinfo", "survey",
		fmt.Sprintf(`{"device":"%s"}`, device))

	output, err := cmd.Output()
	if err != nil {
		return nil, fmt.Errorf("ubus survey failed: %w", err)
	}

	var result UbusSurveyResult
	if err := json.Unmarshal(output, &result); err != nil {
		return nil, fmt.Errorf("failed to parse survey results: %w", err)
	}

	utilization := make(map[int]float64)
	for _, item := range result.Survey {
		if item.ActiveTime > 0 {
			utilization[item.Channel] = float64(item.BusyTime) / float64(item.ActiveTime)
		}
	}

	s.logger.Debug("Channel utilization survey completed",
		"device", device,
		"channels_with_utilization", len(utilization))

	return utilization, nil
}

// analyzeChannels performs sophisticated channel analysis
func (s *EnhancedWiFiScanner) analyzeChannels(aps []UbusWiFiAccessPoint, utilization map[int]float64, device string) []EnhancedChannelScore {
	// Group APs by channel
	apsByChannel := make(map[int][]UbusWiFiAccessPoint)
	for _, ap := range aps {
		apsByChannel[ap.Channel] = append(apsByChannel[ap.Channel], ap)
	}

	var scores []EnhancedChannelScore

	// Analyze each channel that has APs (could extend to all available channels)
	for channel, channelAPs := range apsByChannel {
		score := s.scoreChannel(channel, channelAPs, apsByChannel, utilization, device)
		scores = append(scores, score)
	}

	return scores
}

// scoreChannel implements the enhanced scoring algorithm from the document
func (s *EnhancedWiFiScanner) scoreChannel(channel int, channelAPs []UbusWiFiAccessPoint,
	allAPs map[int][]UbusWiFiAccessPoint, utilization map[int]float64, device string,
) EnhancedChannelScore {
	// Calculate co-channel penalty (P_co)
	coChannelPenalty := 0
	strongInterferers := 0
	weakInterferers := 0

	for _, ap := range channelAPs {
		weight := s.calculateRSSIWeight(ap.Signal)
		coChannelPenalty += weight

		if ap.Signal >= -60 {
			strongInterferers++
		} else if ap.Signal < -70 {
			weakInterferers++
		}
	}

	// Calculate overlap penalty (P_ol)
	overlapPenalty := 0
	for otherChannel, otherAPs := range allAPs {
		if otherChannel == channel {
			continue
		}

		for _, ap := range otherAPs {
			if s.channelsOverlap(channel, otherChannel, ap, device) {
				weight := s.calculateRSSIWeight(ap.Signal)
				overlapPenalty += weight / 2 // 50% of co-channel penalty
			}
		}
	}

	// Calculate utilization penalty (P_util)
	utilizationPenalty := 0
	channelUtilization := utilization[channel]
	if channelUtilization > 0 {
		utilizationPenalty = int(100 * channelUtilization)
	}

	// Calculate raw score: S = 100 - (P_co + P_ol + P_util)
	rawScore := 100 - (coChannelPenalty + overlapPenalty + utilizationPenalty)
	if rawScore < 0 {
		rawScore = 0
	}
	if rawScore > 100 {
		rawScore = 100
	}

	// Convert to star rating
	stars := s.scoreToStars(rawScore)

	// Generate recommendation
	recommendation := s.generateRecommendation(rawScore, len(channelAPs), channelUtilization, strongInterferers)

	return EnhancedChannelScore{
		Band:               s.detectBand(device),
		Channel:            channel,
		APCount:            len(channelAPs),
		CoChannelPenalty:   coChannelPenalty,
		OverlapPenalty:     overlapPenalty,
		UtilizationPenalty: utilizationPenalty,
		Utilization:        channelUtilization,
		RawScore:           rawScore,
		Stars:              stars,
		Recommendation:     recommendation,
		StrongInterferers:  strongInterferers,
		WeakInterferers:    weakInterferers,
	}
}

// calculateRSSIWeight implements the RSSI weighting from the document
func (s *EnhancedWiFiScanner) calculateRSSIWeight(rssi int) int {
	switch {
	case rssi >= -60:
		return 30 // Very strong interferer
	case rssi >= -70:
		return 20 // Strong interferer
	case rssi >= -80:
		return 10 // Moderate interferer
	default:
		return 5 // Weak interferer
	}
}

// channelsOverlap determines if channels overlap based on band and bandwidth
func (s *EnhancedWiFiScanner) channelsOverlap(channel1, channel2 int, ap UbusWiFiAccessPoint, device string) bool {
	if s.detectBand(device) == "2.4GHz" {
		return s.channels2GOverlap(channel1, channel2, s.getChannelWidth(ap))
	} else {
		return s.channels5GOverlap(channel1, channel2, s.getChannelWidth(ap))
	}
}

// channels2GOverlap checks 2.4GHz channel overlap
func (s *EnhancedWiFiScanner) channels2GOverlap(chan1, chan2, width int) bool {
	delta := 2 // Default for 20MHz
	if width >= 40 {
		delta = 4 // 40MHz channels
	}
	return int(math.Abs(float64(chan1-chan2))) <= delta
}

// channels5GOverlap checks 5GHz channel overlap (simplified)
func (s *EnhancedWiFiScanner) channels5GOverlap(chan1, chan2, width int) bool {
	// Simplified 5GHz overlap detection
	// In reality, would need center frequency calculation
	switch width {
	case 80:
		return int(math.Abs(float64(chan1-chan2))) <= 12 // 80MHz span
	case 40:
		return int(math.Abs(float64(chan1-chan2))) <= 6 // 40MHz span
	default:
		return int(math.Abs(float64(chan1-chan2))) <= 2 // 20MHz span
	}
}

// getChannelWidth extracts channel width from AP info
func (s *EnhancedWiFiScanner) getChannelWidth(ap UbusWiFiAccessPoint) int {
	if ap.Bandwidth != 0 {
		return ap.Bandwidth
	}

	switch ap.HTMode {
	case "HT40", "VHT40", "HE40":
		return 40
	case "VHT80", "HE80":
		return 80
	case "VHT160", "HE160":
		return 160
	default:
		return 20
	}
}

// scoreToStars converts raw score to 1-5 star rating
func (s *EnhancedWiFiScanner) scoreToStars(score int) int {
	switch {
	case score >= 90:
		return 5 // Excellent
	case score >= 75:
		return 4 // Good
	case score >= 50:
		return 3 // Fair
	case score >= 25:
		return 2 // Poor
	default:
		return 1 // Very Poor
	}
}

// detectBand determines if device is 2.4GHz or 5GHz
func (s *EnhancedWiFiScanner) detectBand(device string) string {
	// Simple heuristic - could be enhanced with actual frequency detection
	if device == "wlan0" {
		return "2.4GHz"
	}
	return "5GHz"
}

// generateRecommendation provides human-readable channel recommendation
func (s *EnhancedWiFiScanner) generateRecommendation(score int, apCount int, utilization float64, strongInterferers int) string {
	switch {
	case score >= 90:
		return "Excellent choice - minimal interference"
	case score >= 75:
		return "Good choice - acceptable interference levels"
	case score >= 50:
		if strongInterferers > 2 {
			return "Fair - consider if strong interferers move"
		}
		return "Fair - moderate interference present"
	case score >= 25:
		return "Poor - high interference, consider alternatives"
	default:
		return "Avoid - severe interference detected"
	}
}
