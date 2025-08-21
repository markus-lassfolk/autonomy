package wifi

import (
	"context"
	"fmt"
	"math"
	"os/exec"
	"regexp"
	"sort"
	"strconv"
	"strings"
	"sync"
	"time"

	"github.com/markus-lassfolk/autonomy/pkg"
	"github.com/markus-lassfolk/autonomy/pkg/logx"
	"github.com/markus-lassfolk/autonomy/pkg/uci"
)

// WiFiOptimizer manages intelligent WiFi channel optimization
type WiFiOptimizer struct {
	config          *Config
	logger          *logx.Logger
	uci             *uci.UCI
	enhancedScanner *EnhancedWiFiScanner
	lastOptimized   time.Time
	currentPlan     *ChannelPlan
	locationTrigger bool
	mu              sync.RWMutex
}

// Config represents WiFi optimizer configuration
type Config struct {
	Enabled             bool          `json:"enabled"`
	MovementThreshold   float64       `json:"movement_threshold_m"` // meters
	StationaryTime      time.Duration `json:"stationary_time"`      // time to be stationary before optimization
	NightlyOptimization bool          `json:"nightly_optimization"`
	NightlyTime         string        `json:"nightly_time"`    // HH:MM format
	MinImprovement      int           `json:"min_improvement"` // minimum score improvement to apply changes
	DwellTime           time.Duration `json:"dwell_time"`      // wait time after applying changes
	NoiseDefault        int           `json:"noise_default"`   // default noise floor
	VHT80Threshold      int           `json:"vht80_threshold"` // threshold for VHT80 selection
	VHT40Threshold      int           `json:"vht40_threshold"` // threshold for VHT40 selection
	UseDFS              bool          `json:"use_dfs"`         // allow DFS channels
	DryRun              bool          `json:"dry_run"`         // test mode

	// Enhanced scanning options
	UseEnhancedScanner  bool    `json:"use_enhanced_scanner"`  // Use RUTOS-native enhanced scanning
	StrongRSSIThreshold int     `json:"strong_rssi_threshold"` // RSSI threshold for strong interferers (-60dBm)
	WeakRSSIThreshold   int     `json:"weak_rssi_threshold"`   // RSSI threshold for weak interferers (-80dBm)
	UtilizationWeight   int     `json:"utilization_weight"`    // Weight for channel utilization penalty (100)
	ExcellentThreshold  int     `json:"excellent_threshold"`   // Score threshold for 5 stars (90)
	GoodThreshold       int     `json:"good_threshold"`        // Score threshold for 4 stars (75)
	FairThreshold       int     `json:"fair_threshold"`        // Score threshold for 3 stars (50)
	PoorThreshold       int     `json:"poor_threshold"`        // Score threshold for 2 stars (25)
	OverlapPenaltyRatio float64 `json:"overlap_penalty_ratio"` // Overlap penalty as ratio of co-channel (0.5)
}

// ChannelPlan represents a WiFi channel configuration
type ChannelPlan struct {
	Channel24  int       `json:"channel_24"`
	Channel5   int       `json:"channel_5"`
	Width5     string    `json:"width_5"`
	Score24    int       `json:"score_24"`
	Score5     int       `json:"score_5"`
	TotalScore int       `json:"total_score"`
	AppliedAt  time.Time `json:"applied_at"`
	Country    string    `json:"country"`
	RegDomain  string    `json:"reg_domain"`
}

// WiFiInterface represents a WiFi interface
type WiFiInterface struct {
	Name      string `json:"name"`
	Band      string `json:"band"` // "2.4" or "5"
	Frequency string `json:"frequency"`
}

// ChannelScore represents a channel with its interference score
type ChannelScore struct {
	Channel int `json:"channel"`
	Score   int `json:"score"` // lower is better
	BSS     int `json:"bss_count"`
	Noise   int `json:"noise"`
	AvgRSSI int `json:"avg_rssi"`
}

// ScanResult represents WiFi scan results
type ScanResult struct {
	BSSCount      int `json:"bss_count"`
	AvgStrongRSSI int `json:"avg_strong_rssi"`
}

// RegDomainChannels represents channel sets per regulatory domain
type RegDomainChannels struct {
	Band24 []int `json:"band_24"`
	Band5  []int `json:"band_5"`
}

// NewWiFiOptimizer creates a new WiFi optimizer
func NewWiFiOptimizer(config *Config, logger *logx.Logger, uciClient *uci.UCI) *WiFiOptimizer {
	if config == nil {
		config = DefaultConfig()
	}

	optimizer := &WiFiOptimizer{
		config: config,
		logger: logger,
		uci:    uciClient,
	}

	// Initialize enhanced scanner
	optimizer.enhancedScanner = NewEnhancedWiFiScanner(logger)

	logger.Info("WiFi optimizer initialized",
		"enhanced_scanner", config.UseEnhancedScanner,
		"movement_threshold", config.MovementThreshold,
		"min_improvement", config.MinImprovement)

	return optimizer
}

// DefaultConfig returns default WiFi optimizer configuration
func DefaultConfig() *Config {
	return &Config{
		Enabled:             true,
		MovementThreshold:   100.0, // 100 meters
		StationaryTime:      45 * time.Minute,
		NightlyOptimization: true,
		NightlyTime:         "03:00",
		MinImprovement:      15,
		DwellTime:           2 * time.Second,
		NoiseDefault:        -95,
		VHT80Threshold:      60,
		VHT40Threshold:      120,
		UseDFS:              true,
		DryRun:              false,

		// Enhanced scanning defaults
		UseEnhancedScanner:  true, // Enable enhanced scanning by default
		StrongRSSIThreshold: -60,  // Strong interferer threshold
		WeakRSSIThreshold:   -80,  // Weak interferer threshold
		UtilizationWeight:   100,  // Full weight for utilization
		ExcellentThreshold:  90,   // 5 stars
		GoodThreshold:       75,   // 4 stars
		FairThreshold:       50,   // 3 stars
		PoorThreshold:       25,   // 2 stars
		OverlapPenaltyRatio: 0.5,  // 50% of co-channel penalty
	}
}

// OptimizeChannels performs intelligent WiFi channel optimization
func (wo *WiFiOptimizer) OptimizeChannels(ctx context.Context, trigger string) error {
	wo.mu.Lock()
	defer wo.mu.Unlock()

	if !wo.config.Enabled {
		wo.logger.Debug("WiFi optimization disabled", "trigger", trigger)
		return nil
	}

	wo.logger.Info("Starting WiFi channel optimization",
		"trigger", trigger,
		"dry_run", wo.config.DryRun,
		"min_improvement", wo.config.MinImprovement)

	// Detect WiFi interfaces
	interfaces, err := wo.detectInterfaces(ctx)
	if err != nil {
		return fmt.Errorf("failed to detect WiFi interfaces: %w", err)
	}

	if len(interfaces) < 2 {
		wo.logger.Warn("Insufficient WiFi interfaces for optimization", "count", len(interfaces))
		return fmt.Errorf("need at least 2 WiFi interfaces (2.4GHz and 5GHz)")
	}

	// Get regulatory domain information
	country, regDomain, err := wo.getRegDomainInfo(ctx)
	if err != nil {
		wo.logger.Warn("Failed to get regulatory domain info, using defaults", "error", err)
		country = "US"
		regDomain = "FCC"
	}

	wo.logger.Info("Regulatory domain detected",
		"country", country,
		"domain", regDomain)

	// Get current channel plan for comparison
	currentPlan, err := wo.getCurrentPlan(ctx)
	if err != nil {
		wo.logger.Warn("Failed to get current channel plan", "error", err)
		currentPlan = &ChannelPlan{}
	}

	// Find optimal channels using enhanced or legacy method
	var optimalPlan *ChannelPlan

	if wo.config.UseEnhancedScanner {
		wo.logger.Info("Using enhanced RUTOS-native scanning", "trigger", trigger)
		optimalPlan, err = wo.findOptimalChannelsEnhanced(ctx, interfaces, country, regDomain)
	} else {
		wo.logger.Info("Using legacy scanning method", "trigger", trigger)
		optimalPlan, err = wo.findOptimalChannels(ctx, interfaces, country, regDomain)
	}

	if err != nil {
		return fmt.Errorf("failed to find optimal channels: %w", err)
	}

	// Check if optimization is worthwhile
	improvement := currentPlan.TotalScore - optimalPlan.TotalScore
	if improvement < wo.config.MinImprovement {
		wo.logger.Info("Current plan is close enough, skipping optimization",
			"current_score", currentPlan.TotalScore,
			"optimal_score", optimalPlan.TotalScore,
			"improvement", improvement,
			"min_improvement", wo.config.MinImprovement)
		return nil
	}

	// Apply the optimal plan
	if err := wo.applyChannelPlan(ctx, optimalPlan); err != nil {
		return fmt.Errorf("failed to apply channel plan: %w", err)
	}

	// Check for DFS radar detection and fallback if needed
	if wo.config.UseDFS && wo.isDFSChannel(optimalPlan.Channel5) {
		if err := wo.checkDFSRadar(ctx, optimalPlan); err != nil {
			wo.logger.Warn("DFS radar detected, applying fallback", "error", err)
			if err := wo.applyDFSFallback(ctx, regDomain); err != nil {
				wo.logger.Error("Failed to apply DFS fallback", "error", err)
			}
		}
	}

	wo.currentPlan = optimalPlan
	wo.lastOptimized = time.Now()

	wo.logger.Info("WiFi channel optimization completed successfully",
		"trigger", trigger,
		"channel_24", optimalPlan.Channel24,
		"channel_5", optimalPlan.Channel5,
		"width_5", optimalPlan.Width5,
		"score_improvement", improvement,
		"country", country,
		"domain", regDomain)

	return nil
}

// detectInterfaces detects available WiFi interfaces
func (wo *WiFiOptimizer) detectInterfaces(ctx context.Context) ([]WiFiInterface, error) {
	wo.logger.Debug("Detecting WiFi interfaces")

	// Get interface list from iw dev
	cmd := exec.CommandContext(ctx, "iw", "dev")
	output, err := cmd.Output()
	if err != nil {
		return nil, fmt.Errorf("failed to get interface list: %w", err)
	}

	var interfaces []WiFiInterface
	lines := strings.Split(string(output), "\n")

	for _, line := range lines {
		line = strings.TrimSpace(line)
		if strings.HasPrefix(line, "Interface ") {
			ifaceName := strings.TrimPrefix(line, "Interface ")

			// Get frequency information for this interface
			freq, err := wo.getInterfaceFrequency(ctx, ifaceName)
			if err != nil {
				wo.logger.Debug("Failed to get frequency for interface",
					"interface", ifaceName, "error", err)
				continue
			}

			band := "unknown"
			if strings.HasPrefix(freq, "2.") {
				band = "2.4"
			} else if strings.HasPrefix(freq, "5.") || strings.HasPrefix(freq, "6.") {
				band = "5"
			}

			interfaces = append(interfaces, WiFiInterface{
				Name:      ifaceName,
				Band:      band,
				Frequency: freq,
			})

			wo.logger.Debug("Detected WiFi interface",
				"name", ifaceName,
				"band", band,
				"frequency", freq)
		}
	}

	// Fallback: try common interface names if detection failed
	if len(interfaces) == 0 {
		commonNames := []string{"wlan0", "wlan1", "radio0", "radio1"}
		for _, name := range commonNames {
			if freq, err := wo.getInterfaceFrequency(ctx, name); err == nil {
				band := "unknown"
				if strings.HasPrefix(freq, "2.") {
					band = "2.4"
				} else if strings.HasPrefix(freq, "5.") || strings.HasPrefix(freq, "6.") {
					band = "5"
				}

				interfaces = append(interfaces, WiFiInterface{
					Name:      name,
					Band:      band,
					Frequency: freq,
				})

				wo.logger.Debug("Fallback detected WiFi interface",
					"name", name,
					"band", band,
					"frequency", freq)
			}
		}
	}

	wo.logger.Info("WiFi interface detection completed",
		"count", len(interfaces),
		"interfaces", interfaces)

	return interfaces, nil
}

// getInterfaceFrequency gets the current frequency of a WiFi interface
func (wo *WiFiOptimizer) getInterfaceFrequency(ctx context.Context, iface string) (string, error) {
	// Try iwinfo first
	cmd := exec.CommandContext(ctx, "iwinfo", iface, "info")
	if output, err := cmd.Output(); err == nil {
		re := regexp.MustCompile(`Frequency: (\d+\.\d+) GHz`)
		if matches := re.FindStringSubmatch(string(output)); len(matches) > 1 {
			return matches[1], nil
		}
	}

	// Fallback to iw
	cmd = exec.CommandContext(ctx, "iw", "dev", iface, "info")
	if output, err := cmd.Output(); err == nil {
		re := regexp.MustCompile(`channel \d+ \((\d+) MHz\)`)
		if matches := re.FindStringSubmatch(string(output)); len(matches) > 1 {
			freq, _ := strconv.Atoi(matches[1])
			return fmt.Sprintf("%.1f", float64(freq)/1000), nil
		}
	}

	return "", fmt.Errorf("could not determine frequency for interface %s", iface)
}

// getRegDomainInfo gets regulatory domain information
func (wo *WiFiOptimizer) getRegDomainInfo(ctx context.Context) (country, domain string, err error) {
	// Get country code from UCI first
	country, err = wo.getUCICountry()
	if err != nil {
		wo.logger.Debug("Failed to get country from UCI", "error", err)
	}

	// Get regulatory domain from kernel
	cmd := exec.CommandContext(ctx, "iw", "reg", "get")
	output, err := cmd.Output()
	if err != nil {
		return country, "OTHER", fmt.Errorf("failed to get regulatory domain: %w", err)
	}

	lines := strings.Split(string(output), "\n")
	for _, line := range lines {
		line = strings.ToUpper(strings.TrimSpace(line))
		if strings.HasPrefix(line, "COUNTRY ") {
			// Extract country if not already set
			if country == "" {
				parts := strings.Fields(line)
				if len(parts) > 1 {
					country = strings.TrimSuffix(parts[1], ":")
				}
			}

			// Determine regulatory domain
			if strings.Contains(line, "DFS-ETSI") {
				domain = "ETSI"
			} else if strings.Contains(line, "DFS-FCC") {
				domain = "FCC"
			} else {
				domain = "OTHER"
			}
			break
		}
	}

	if country == "" {
		country = "US"
	}
	if domain == "" {
		domain = "OTHER"
	}

	return country, domain, nil
}

// getUCICountry gets country code from UCI wireless configuration
func (wo *WiFiOptimizer) getUCICountry() (string, error) {
	// Try radio0 first
	if country, err := wo.getUCIValue("wireless.radio0.country"); err == nil && country != "" {
		return strings.ToUpper(country), nil
	}

	// Try radio1
	if country, err := wo.getUCIValue("wireless.radio1.country"); err == nil && country != "" {
		return strings.ToUpper(country), nil
	}

	return "", fmt.Errorf("no country code found in UCI")
}

// findOptimalChannels finds the optimal channel configuration
func (wo *WiFiOptimizer) findOptimalChannels(ctx context.Context, interfaces []WiFiInterface, country, regDomain string) (*ChannelPlan, error) {
	wo.logger.Debug("Finding optimal channels",
		"country", country,
		"domain", regDomain)

	// Get channel candidates based on regulatory domain
	channels := wo.getRegDomainChannels(regDomain)

	// Find 2.4GHz interface
	var iface24 string
	for _, iface := range interfaces {
		if iface.Band == "2.4" {
			iface24 = iface.Name
			break
		}
	}

	// Find 5GHz interface
	var iface5 string
	for _, iface := range interfaces {
		if iface.Band == "5" {
			iface5 = iface.Name
			break
		}
	}

	if iface24 == "" || iface5 == "" {
		return nil, fmt.Errorf("missing required interfaces: 2.4GHz=%s, 5GHz=%s", iface24, iface5)
	}

	// Find optimal 2.4GHz channel
	best24, score24, err := wo.findBestChannel(ctx, iface24, channels.Band24)
	if err != nil {
		return nil, fmt.Errorf("failed to find optimal 2.4GHz channel: %w", err)
	}

	// Find optimal 5GHz channel
	best5, score5, err := wo.findBestChannel(ctx, iface5, channels.Band5)
	if err != nil {
		return nil, fmt.Errorf("failed to find optimal 5GHz channel: %w", err)
	}

	// Determine optimal 5GHz width based on interference
	width5 := wo.determineOptimalWidth(score5)

	plan := &ChannelPlan{
		Channel24:  best24,
		Channel5:   best5,
		Width5:     width5,
		Score24:    score24,
		Score5:     score5,
		TotalScore: score24 + score5,
		AppliedAt:  time.Now(),
		Country:    country,
		RegDomain:  regDomain,
	}

	wo.logger.Info("Optimal channel plan determined",
		"channel_24", best24,
		"score_24", score24,
		"channel_5", best5,
		"score_5", score5,
		"width_5", width5,
		"total_score", plan.TotalScore)

	return plan, nil
}

// getRegDomainChannels returns channel sets for regulatory domains
func (wo *WiFiOptimizer) getRegDomainChannels(regDomain string) RegDomainChannels {
	switch regDomain {
	case "ETSI":
		return RegDomainChannels{
			Band24: []int{1, 5, 9, 13},
			Band5:  []int{100, 104, 108, 112, 116, 120, 124, 128, 132, 136, 140}, // DFS preferred
		}
	case "FCC":
		channels5 := []int{36, 40, 44, 48, 149, 153, 157, 161}
		if wo.config.UseDFS {
			// Add DFS channels for FCC
			channels5 = append(channels5, 52, 56, 60, 64, 100, 104, 108, 112, 116, 120, 124, 128, 132, 136, 140)
		}
		// Add channel 165 if supported
		channels5 = append(channels5, 165)
		return RegDomainChannels{
			Band24: []int{1, 6, 11},
			Band5:  channels5,
		}
	default:
		return RegDomainChannels{
			Band24: []int{1, 6, 11},
			Band5:  []int{36, 40, 44, 48}, // Conservative set
		}
	}
}

// findBestChannel finds the best channel from candidates
func (wo *WiFiOptimizer) findBestChannel(ctx context.Context, iface string, candidates []int) (int, int, error) {
	wo.logger.Debug("Finding best channel",
		"interface", iface,
		"candidates", candidates)

	bestChannel := candidates[0]
	bestScore := 999999

	for _, channel := range candidates {
		// Apply channel temporarily for testing
		if err := wo.applyTestChannel(ctx, iface, channel); err != nil {
			wo.logger.Warn("Failed to apply test channel",
				"interface", iface,
				"channel", channel,
				"error", err)
			continue
		}

		// Wait for channel to settle
		time.Sleep(wo.config.DwellTime)

		// Scan and score the channel
		score, err := wo.scoreChannel(ctx, iface)
		if err != nil {
			wo.logger.Warn("Failed to score channel",
				"interface", iface,
				"channel", channel,
				"error", err)
			continue
		}

		wo.logger.Debug("Channel scored",
			"interface", iface,
			"channel", channel,
			"score", score)

		if score < bestScore {
			bestScore = score
			bestChannel = channel
		}
	}

	wo.logger.Info("Best channel found",
		"interface", iface,
		"channel", bestChannel,
		"score", bestScore)

	return bestChannel, bestScore, nil
}

// applyTestChannel applies a channel for testing purposes
func (wo *WiFiOptimizer) applyTestChannel(ctx context.Context, iface string, channel int) error {
	// Determine radio name from interface
	radio := wo.getRadioFromInterface(iface)
	if radio == "" {
		return fmt.Errorf("could not determine radio for interface %s", iface)
	}

	// Set channel via UCI
	channelStr := strconv.Itoa(channel)
	if err := wo.setUCIValue(fmt.Sprintf("wireless.%s.channel", radio), channelStr); err != nil {
		return fmt.Errorf("failed to set channel in UCI: %w", err)
	}

	// Apply changes
	if wo.config.DryRun {
		wo.logger.Info("DRY RUN: Would apply test channel",
			"radio", radio,
			"channel", channel)
		return nil
	}

	if err := wo.commitUCI("wireless"); err != nil {
		return fmt.Errorf("failed to commit UCI changes: %w", err)
	}

	// Reload WiFi
	cmd := exec.CommandContext(ctx, "wifi", "reload")
	if err := cmd.Run(); err != nil {
		return fmt.Errorf("failed to reload WiFi: %w", err)
	}

	return nil
}

// scoreChannel calculates interference score for a channel
func (wo *WiFiOptimizer) scoreChannel(ctx context.Context, iface string) (int, error) {
	// Get noise floor
	noise := wo.getNoise(ctx, iface)
	if noise == 0 {
		noise = wo.config.NoiseDefault
	}

	// Perform scan
	scanResult, err := wo.scanInterface(ctx, iface)
	if err != nil {
		return 999999, fmt.Errorf("failed to scan interface: %w", err)
	}

	// Calculate score: (BSS * 10) + (100 + noise) + RSSI_penalty
	// Lower score is better
	score := (scanResult.BSSCount * 10) + (100 + noise) + scanResult.AvgStrongRSSI

	wo.logger.Debug("Channel score calculated",
		"interface", iface,
		"bss_count", scanResult.BSSCount,
		"noise", noise,
		"avg_rssi", scanResult.AvgStrongRSSI,
		"score", score)

	return score, nil
}

// getNoise gets noise floor for an interface
func (wo *WiFiOptimizer) getNoise(ctx context.Context, iface string) int {
	// Try iwinfo first
	cmd := exec.CommandContext(ctx, "iwinfo", iface, "info")
	if output, err := cmd.Output(); err == nil {
		re := regexp.MustCompile(`Noise: (-?\d+) dBm`)
		if matches := re.FindStringSubmatch(string(output)); len(matches) > 1 {
			if noise, err := strconv.Atoi(matches[1]); err == nil {
				return noise
			}
		}
	}

	wo.logger.Debug("Could not get noise floor, using default",
		"interface", iface,
		"default", wo.config.NoiseDefault)

	return wo.config.NoiseDefault
}

// scanInterface performs WiFi scan on interface
func (wo *WiFiOptimizer) scanInterface(ctx context.Context, iface string) (*ScanResult, error) {
	// Use iwinfo scan
	cmd := exec.CommandContext(ctx, "iwinfo", iface, "scan")
	output, err := cmd.Output()
	if err != nil {
		return &ScanResult{}, fmt.Errorf("scan failed: %w", err)
	}

	result := &ScanResult{}
	lines := strings.Split(string(output), "\n")
	var signals []int

	for _, line := range lines {
		line = strings.TrimSpace(line)

		// Count access points
		if strings.Contains(line, "Address:") {
			result.BSSCount++
		}

		// Extract signal strengths
		if strings.Contains(line, "Signal:") {
			re := regexp.MustCompile(`Signal: (-?\d+) dBm`)
			if matches := re.FindStringSubmatch(line); len(matches) > 1 {
				if signal, err := strconv.Atoi(matches[1]); err == nil {
					signals = append(signals, -signal) // Convert to positive for easier math
				}
			}
		}
	}

	// Calculate average of top 3 strongest signals
	if len(signals) > 0 {
		sort.Ints(signals) // Sort ascending (strongest first after negation)

		count := len(signals)
		if count > 3 {
			count = 3
		}

		sum := 0
		for i := 0; i < count; i++ {
			sum += signals[i]
		}
		result.AvgStrongRSSI = sum / count

		// Cap at 40 to prevent excessive penalty
		if result.AvgStrongRSSI > 40 {
			result.AvgStrongRSSI = 40
		}
	}

	wo.logger.Debug("Scan completed",
		"interface", iface,
		"bss_count", result.BSSCount,
		"signals_found", len(signals),
		"avg_strong_rssi", result.AvgStrongRSSI)

	return result, nil
}

// determineOptimalWidth determines optimal channel width based on interference
func (wo *WiFiOptimizer) determineOptimalWidth(score int) string {
	if score <= wo.config.VHT80Threshold {
		return "VHT80"
	} else if score <= wo.config.VHT40Threshold {
		return "VHT40"
	}
	return "VHT20"
}

// getCurrentPlan gets the current channel configuration
func (wo *WiFiOptimizer) getCurrentPlan(ctx context.Context) (*ChannelPlan, error) {
	plan := &ChannelPlan{}

	// Get 2.4GHz channel
	if ch24, err := wo.getUCIValue("wireless.radio0.channel"); err == nil {
		if channel, err := strconv.Atoi(ch24); err == nil {
			plan.Channel24 = channel
		}
	}

	// Get 5GHz channel and width
	if ch5, err := wo.getUCIValue("wireless.radio1.channel"); err == nil {
		if channel, err := strconv.Atoi(ch5); err == nil {
			plan.Channel5 = channel
		}
	}

	if width, err := wo.getUCIValue("wireless.radio1.htmode"); err == nil {
		plan.Width5 = width
	}

	// Score current plan
	if plan.Channel24 > 0 && plan.Channel5 > 0 {
		// This is a simplified scoring - in practice you'd need to scan current channels
		plan.Score24 = 100 // Placeholder
		plan.Score5 = 100  // Placeholder
		plan.TotalScore = plan.Score24 + plan.Score5
	}

	return plan, nil
}

// applyChannelPlan applies the optimal channel configuration
func (wo *WiFiOptimizer) applyChannelPlan(ctx context.Context, plan *ChannelPlan) error {
	wo.logger.Info("Applying channel plan",
		"channel_24", plan.Channel24,
		"channel_5", plan.Channel5,
		"width_5", plan.Width5,
		"dry_run", wo.config.DryRun)

	if wo.config.DryRun {
		wo.logger.Info("DRY RUN: Would apply channel plan", "plan", plan)
		return nil
	}

	// Apply 2.4GHz settings
	if err := wo.setUCIValue("wireless.radio0.channel", strconv.Itoa(plan.Channel24)); err != nil {
		return fmt.Errorf("failed to set 2.4GHz channel: %w", err)
	}
	if err := wo.setUCIValue("wireless.radio0.hwmode", "11g"); err != nil {
		return fmt.Errorf("failed to set 2.4GHz hwmode: %w", err)
	}
	if err := wo.setUCIValue("wireless.radio0.htmode", "HT20"); err != nil {
		return fmt.Errorf("failed to set 2.4GHz htmode: %w", err)
	}

	// Apply 5GHz settings
	if err := wo.setUCIValue("wireless.radio1.channel", strconv.Itoa(plan.Channel5)); err != nil {
		return fmt.Errorf("failed to set 5GHz channel: %w", err)
	}
	if err := wo.setUCIValue("wireless.radio1.hwmode", "11a"); err != nil {
		return fmt.Errorf("failed to set 5GHz hwmode: %w", err)
	}
	if err := wo.setUCIValue("wireless.radio1.htmode", plan.Width5); err != nil {
		return fmt.Errorf("failed to set 5GHz htmode: %w", err)
	}

	// Commit changes
	if err := wo.commitUCI("wireless"); err != nil {
		return fmt.Errorf("failed to commit wireless changes: %w", err)
	}

	// Reload WiFi
	cmd := exec.CommandContext(ctx, "wifi", "reload")
	if err := cmd.Run(); err != nil {
		return fmt.Errorf("failed to reload WiFi: %w", err)
	}

	wo.logger.Info("Channel plan applied successfully")
	return nil
}

// Helper functions

// getRadioFromInterface maps interface name to radio name
func (wo *WiFiOptimizer) getRadioFromInterface(iface string) string {
	// Common mappings
	switch iface {
	case "wlan0":
		return "radio0"
	case "wlan1":
		return "radio1"
	default:
		// Try to extract from interface name
		if strings.Contains(iface, "0") {
			return "radio0"
		} else if strings.Contains(iface, "1") {
			return "radio1"
		}
	}
	return ""
}

// isDFSChannel checks if a channel is a DFS channel
func (wo *WiFiOptimizer) isDFSChannel(channel int) bool {
	dfsChannels := []int{52, 56, 60, 64, 100, 104, 108, 112, 116, 120, 124, 128, 132, 136, 140}
	for _, dfs := range dfsChannels {
		if channel == dfs {
			return true
		}
	}
	return false
}

// checkDFSRadar checks for DFS radar detection
func (wo *WiFiOptimizer) checkDFSRadar(ctx context.Context, plan *ChannelPlan) error {
	// Wait a moment for potential radar detection
	time.Sleep(2 * time.Second)

	// Check system logs for radar detection
	cmd := exec.CommandContext(ctx, "logread")
	output, err := cmd.Output()
	if err != nil {
		return fmt.Errorf("failed to read system log: %w", err)
	}

	if strings.Contains(strings.ToLower(string(output)), "radar detected") {
		return fmt.Errorf("DFS radar detected on channel %d", plan.Channel5)
	}

	return nil
}

// applyDFSFallback applies fallback configuration when DFS radar is detected
func (wo *WiFiOptimizer) applyDFSFallback(ctx context.Context, regDomain string) error {
	var fallbackChannel int

	switch regDomain {
	case "ETSI":
		fallbackChannel = 36
	case "FCC":
		fallbackChannel = 36
	default:
		fallbackChannel = 36
	}

	wo.logger.Info("Applying DFS fallback",
		"fallback_channel", fallbackChannel,
		"width", "VHT40")

	if wo.config.DryRun {
		wo.logger.Info("DRY RUN: Would apply DFS fallback",
			"channel", fallbackChannel)
		return nil
	}

	// Apply fallback settings
	if err := wo.setUCIValue("wireless.radio1.channel", strconv.Itoa(fallbackChannel)); err != nil {
		return fmt.Errorf("failed to set fallback channel: %w", err)
	}
	if err := wo.setUCIValue("wireless.radio1.htmode", "VHT40"); err != nil {
		return fmt.Errorf("failed to set fallback htmode: %w", err)
	}

	// Commit and reload
	if err := wo.commitUCI("wireless"); err != nil {
		return fmt.Errorf("failed to commit fallback changes: %w", err)
	}

	cmd := exec.CommandContext(ctx, "wifi", "reload")
	if err := cmd.Run(); err != nil {
		return fmt.Errorf("failed to reload WiFi for fallback: %w", err)
	}

	return nil
}

// ShouldOptimize checks if optimization should be triggered
func (wo *WiFiOptimizer) ShouldOptimize(trigger string) bool {
	wo.mu.RLock()
	defer wo.mu.RUnlock()

	if !wo.config.Enabled {
		return false
	}

	switch trigger {
	case "location_change":
		return wo.locationTrigger
	case "nightly":
		return wo.config.NightlyOptimization && wo.isNightlyTime()
	case "manual":
		return true
	default:
		return false
	}
}

// SetLocationTrigger sets the location-based trigger
func (wo *WiFiOptimizer) SetLocationTrigger(triggered bool) {
	wo.mu.Lock()
	defer wo.mu.Unlock()
	wo.locationTrigger = triggered
}

// isNightlyTime checks if it's time for nightly optimization
func (wo *WiFiOptimizer) isNightlyTime() bool {
	now := time.Now()
	targetTime, err := time.Parse("15:04", wo.config.NightlyTime)
	if err != nil {
		return false
	}

	// Check if we're within 1 hour of the target time
	target := time.Date(now.Year(), now.Month(), now.Day(),
		targetTime.Hour(), targetTime.Minute(), 0, 0, now.Location())

	diff := now.Sub(target)
	return diff >= 0 && diff < time.Hour
}

// GetStatus returns current WiFi optimizer status
func (wo *WiFiOptimizer) GetStatus() map[string]interface{} {
	wo.mu.RLock()
	defer wo.mu.RUnlock()

	status := map[string]interface{}{
		"enabled":          wo.config.Enabled,
		"last_optimized":   wo.lastOptimized,
		"location_trigger": wo.locationTrigger,
		"dry_run":          wo.config.DryRun,
	}

	if wo.currentPlan != nil {
		status["current_plan"] = wo.currentPlan
	}

	return status
}

// calculateDistance calculates the distance between two GPS coordinates using Haversine formula
func (wo *WiFiOptimizer) calculateDistance(from, to *pkg.GPSData) float64 {
	const earthRadiusM = 6371000 // Earth's radius in meters

	lat1Rad := from.Latitude * math.Pi / 180
	lat2Rad := to.Latitude * math.Pi / 180
	deltaLatRad := (to.Latitude - from.Latitude) * math.Pi / 180
	deltaLonRad := (to.Longitude - from.Longitude) * math.Pi / 180

	a := math.Sin(deltaLatRad/2)*math.Sin(deltaLatRad/2) +
		math.Cos(lat1Rad)*math.Cos(lat2Rad)*
			math.Sin(deltaLonRad/2)*math.Sin(deltaLonRad/2)
	c := 2 * math.Atan2(math.Sqrt(a), math.Sqrt(1-a))

	return earthRadiusM * c
}

// findOptimalChannelsEnhanced uses the enhanced scanner for sophisticated channel selection
func (wo *WiFiOptimizer) findOptimalChannelsEnhanced(ctx context.Context, interfaces []WiFiInterface, country, regDomain string) (*ChannelPlan, error) {
	wo.logger.Debug("Finding optimal channels using enhanced RUTOS-native scanner",
		"country", country,
		"domain", regDomain)

	// Find 2.4GHz and 5GHz interfaces
	var device24, device5 string
	for _, iface := range interfaces {
		if iface.Band == "2.4" {
			device24 = iface.Name
		} else if iface.Band == "5" {
			device5 = iface.Name
		}
	}

	if device24 == "" || device5 == "" {
		return nil, fmt.Errorf("missing required interfaces: 2.4GHz=%s, 5GHz=%s", device24, device5)
	}

	// Analyze 2.4GHz channels using enhanced scanner
	scores24, err := wo.enhancedScanner.ScanAndRateChannels(ctx, device24)
	if err != nil {
		return nil, fmt.Errorf("failed to analyze 2.4GHz channels: %w", err)
	}

	// Analyze 5GHz channels using enhanced scanner
	scores5, err := wo.enhancedScanner.ScanAndRateChannels(ctx, device5)
	if err != nil {
		return nil, fmt.Errorf("failed to analyze 5GHz channels: %w", err)
	}

	// Filter channels by regulatory domain
	validChannels24 := wo.filterChannelsByRegDomain(scores24, regDomain, "2.4GHz")
	validChannels5 := wo.filterChannelsByRegDomain(scores5, regDomain, "5GHz")

	if len(validChannels24) == 0 || len(validChannels5) == 0 {
		return nil, fmt.Errorf("no valid channels found for regulatory domain %s", regDomain)
	}

	// Select best channels (already sorted by score)
	best24 := validChannels24[0]
	best5 := validChannels5[0]

	// Determine optimal 5GHz width based on enhanced scoring
	width5 := wo.determineOptimalWidthEnhanced(best5)

	plan := &ChannelPlan{
		Channel24:  best24.Channel,
		Channel5:   best5.Channel,
		Width5:     width5,
		Score24:    best24.RawScore,
		Score5:     best5.RawScore,
		TotalScore: best24.RawScore + best5.RawScore,
		AppliedAt:  time.Now(),
		Country:    country,
		RegDomain:  regDomain,
	}

	wo.logger.Info("Enhanced optimal channel plan determined",
		"method", "rutos_native_enhanced",
		"channel_24", best24.Channel,
		"score_24", best24.RawScore,
		"stars_24", best24.Stars,
		"aps_24", best24.APCount,
		"strong_interferers_24", best24.StrongInterferers,
		"utilization_24", best24.Utilization,
		"recommendation_24", best24.Recommendation,
		"channel_5", best5.Channel,
		"score_5", best5.RawScore,
		"stars_5", best5.Stars,
		"aps_5", best5.APCount,
		"strong_interferers_5", best5.StrongInterferers,
		"utilization_5", best5.Utilization,
		"recommendation_5", best5.Recommendation,
		"width_5", width5,
		"total_score", plan.TotalScore)

	return plan, nil
}

// filterChannelsByRegDomain filters enhanced channel scores by regulatory domain
func (wo *WiFiOptimizer) filterChannelsByRegDomain(scores []EnhancedChannelScore, regDomain, band string) []EnhancedChannelScore {
	var validChannels []int

	switch regDomain {
	case "ETSI":
		if band == "2.4GHz" {
			validChannels = []int{1, 5, 9, 13}
		} else {
			validChannels = []int{100, 104, 108, 112, 116, 120, 124, 128, 132, 136, 140}
		}
	case "FCC":
		if band == "2.4GHz" {
			validChannels = []int{1, 6, 11}
		} else {
			validChannels = []int{36, 40, 44, 48, 149, 153, 157, 161}
			if wo.config.UseDFS {
				validChannels = append(validChannels, 52, 56, 60, 64, 100, 104, 108, 112, 116, 120, 124, 128, 132, 136, 140)
			}
		}
	default:
		if band == "2.4GHz" {
			validChannels = []int{1, 6, 11}
		} else {
			validChannels = []int{36, 40, 44, 48}
		}
	}

	// Filter scores to only include valid channels
	var filtered []EnhancedChannelScore
	for _, score := range scores {
		for _, validChan := range validChannels {
			if score.Channel == validChan {
				filtered = append(filtered, score)
				break
			}
		}
	}

	wo.logger.Debug("Filtered channels by regulatory domain",
		"domain", regDomain,
		"band", band,
		"total_channels", len(scores),
		"valid_channels", len(filtered))

	return filtered
}

// determineOptimalWidthEnhanced determines channel width based on enhanced scoring
func (wo *WiFiOptimizer) determineOptimalWidthEnhanced(score EnhancedChannelScore) string {
	// Use enhanced scoring metrics to make intelligent width decisions

	// Excellent conditions: high score, low utilization, no strong interferers
	if score.RawScore >= wo.config.ExcellentThreshold &&
		score.Utilization < 0.3 &&
		score.StrongInterferers == 0 {
		wo.logger.Debug("Selecting VHT80 for excellent conditions",
			"score", score.RawScore,
			"utilization", score.Utilization,
			"strong_interferers", score.StrongInterferers)
		return "VHT80" // 80MHz for excellent conditions
	}

	// Good conditions: good score, moderate utilization, few strong interferers
	if score.RawScore >= wo.config.GoodThreshold &&
		score.Utilization < 0.5 &&
		score.StrongInterferers <= 1 {
		wo.logger.Debug("Selecting VHT40 for good conditions",
			"score", score.RawScore,
			"utilization", score.Utilization,
			"strong_interferers", score.StrongInterferers)
		return "VHT40" // 40MHz for good conditions
	}

	// Crowded conditions: stick to 20MHz for reliability
	wo.logger.Debug("Selecting HT20 for crowded conditions",
		"score", score.RawScore,
		"utilization", score.Utilization,
		"strong_interferers", score.StrongInterferers)
	return "HT20" // 20MHz for crowded/poor conditions
}

// GetChannelAnalysis returns detailed channel analysis for monitoring/debugging
func (wo *WiFiOptimizer) GetChannelAnalysis(ctx context.Context) (*ChannelAnalysisReport, error) {
	if !wo.config.UseEnhancedScanner {
		return nil, fmt.Errorf("enhanced scanner not enabled")
	}

	interfaces, err := wo.detectInterfaces(ctx)
	if err != nil {
		return nil, fmt.Errorf("failed to detect interfaces: %w", err)
	}

	report := &ChannelAnalysisReport{
		Timestamp: time.Now(),
		Bands:     make(map[string][]EnhancedChannelScore),
	}

	for _, iface := range interfaces {
		scores, err := wo.enhancedScanner.ScanAndRateChannels(ctx, iface.Name)
		if err != nil {
			wo.logger.Warn("Failed to analyze interface", "interface", iface.Name, "error", err)
			continue
		}
		report.Bands[iface.Band] = scores
	}

	wo.logger.Debug("Channel analysis completed",
		"bands_analyzed", len(report.Bands),
		"timestamp", report.Timestamp)

	return report, nil
}

// UCI helper methods that adapt to the main daemon's UCI interface

// getUCIValue gets a UCI value using direct exec (same as main daemon)
func (wo *WiFiOptimizer) getUCIValue(key string) (string, error) {
	ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancel()

	cmd := exec.CommandContext(ctx, "uci", "get", key)
	output, err := cmd.Output()
	if err != nil {
		return "", fmt.Errorf("failed to get UCI value %s: %w", key, err)
	}

	return strings.TrimSpace(string(output)), nil
}

// setUCIValue sets a UCI value using direct exec (same as main daemon)
func (wo *WiFiOptimizer) setUCIValue(key, value string) error {
	ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancel()

	cmd := exec.CommandContext(ctx, "uci", "set", fmt.Sprintf("%s=%s", key, value))
	return cmd.Run()
}

// commitUCI commits UCI changes using direct exec (same as main daemon)
func (wo *WiFiOptimizer) commitUCI(config string) error {
	ctx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
	defer cancel()

	cmd := exec.CommandContext(ctx, "uci", "commit", config)
	return cmd.Run()
}
