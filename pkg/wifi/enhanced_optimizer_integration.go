package wifi

import (
	"context"
	"fmt"
	"time"

	"github.com/markus-lassfolk/autonomy/pkg/logx"
	"github.com/markus-lassfolk/autonomy/pkg/uci"
)

// EnhancedOptimizer integrates the sophisticated RUTOS-native scanning with our existing optimizer
type EnhancedOptimizer struct {
	*WiFiOptimizer // Embed existing optimizer
	scanner        *EnhancedWiFiScanner
}

// NewEnhancedOptimizer creates an enhanced optimizer that uses RUTOS built-in scanning
func NewEnhancedOptimizer(config *Config, logger *logx.Logger, uciClient *uci.UCI) *EnhancedOptimizer {
	baseOptimizer := NewWiFiOptimizer(config, logger, uciClient)
	scanner := NewEnhancedWiFiScanner(logger)

	return &EnhancedOptimizer{
		WiFiOptimizer: baseOptimizer,
		scanner:       scanner,
	}
}

// OptimizeChannelsEnhanced performs optimization using the enhanced scanning approach
func (eo *EnhancedOptimizer) OptimizeChannelsEnhanced(ctx context.Context, trigger string) error {
	eo.mu.Lock()
	defer eo.mu.Unlock()

	if !eo.config.Enabled {
		eo.logger.Debug("WiFi optimization disabled", "trigger", trigger)
		return nil
	}

	eo.logger.Info("Starting enhanced WiFi channel optimization",
		"trigger", trigger,
		"dry_run", eo.config.DryRun,
		"min_improvement", eo.config.MinImprovement,
		"method", "enhanced_rutos_native")

	// Detect WiFi interfaces
	interfaces, err := eo.detectInterfaces(ctx)
	if err != nil {
		return fmt.Errorf("failed to detect WiFi interfaces: %w", err)
	}

	if len(interfaces) < 2 {
		eo.logger.Warn("Insufficient WiFi interfaces for optimization", "count", len(interfaces))
		return fmt.Errorf("need at least 2 WiFi interfaces (2.4GHz and 5GHz)")
	}

	// Get regulatory domain information
	country, regDomain, err := eo.getRegDomainInfo(ctx)
	if err != nil {
		eo.logger.Warn("Failed to get regulatory domain info, using defaults", "error", err)
		country = "US"
		regDomain = "FCC"
	}

	// Analyze channels using enhanced scanner
	plan, err := eo.findOptimalChannelsEnhanced(ctx, interfaces, country, regDomain)
	if err != nil {
		return fmt.Errorf("failed to find optimal channels: %w", err)
	}

	// Get current plan for comparison
	currentPlan, err := eo.getCurrentPlan(ctx)
	if err != nil {
		eo.logger.Warn("Failed to get current channel plan", "error", err)
		currentPlan = &ChannelPlan{}
	}

	// Check if optimization is worthwhile
	improvement := plan.TotalScore - currentPlan.TotalScore
	if improvement < eo.config.MinImprovement {
		eo.logger.Info("Current plan is close enough, skipping optimization",
			"current_score", currentPlan.TotalScore,
			"optimal_score", plan.TotalScore,
			"improvement", improvement,
			"min_improvement", eo.config.MinImprovement)
		return nil
	}

	// Apply the optimal plan
	if err := eo.applyChannelPlan(ctx, plan); err != nil {
		return fmt.Errorf("failed to apply channel plan: %w", err)
	}

	eo.currentPlan = plan
	eo.lastOptimized = time.Now()

	eo.logger.Info("Enhanced WiFi channel optimization completed successfully",
		"trigger", trigger,
		"channel_24", plan.Channel24,
		"channel_5", plan.Channel5,
		"width_5", plan.Width5,
		"score_improvement", improvement,
		"country", country,
		"domain", regDomain,
		"method", "enhanced_rutos_native")

	return nil
}

// findOptimalChannelsEnhanced uses the enhanced scanner for channel selection
func (eo *EnhancedOptimizer) findOptimalChannelsEnhanced(ctx context.Context, interfaces []WiFiInterface, country, regDomain string) (*ChannelPlan, error) {
	eo.logger.Debug("Finding optimal channels using enhanced scanner",
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

	// Analyze 2.4GHz channels
	scores24, err := eo.scanner.ScanAndRateChannels(ctx, device24)
	if err != nil {
		return nil, fmt.Errorf("failed to analyze 2.4GHz channels: %w", err)
	}

	// Analyze 5GHz channels
	scores5, err := eo.scanner.ScanAndRateChannels(ctx, device5)
	if err != nil {
		return nil, fmt.Errorf("failed to analyze 5GHz channels: %w", err)
	}

	// Filter channels by regulatory domain
	validChannels24 := eo.filterByRegDomain(scores24, regDomain, "2.4GHz")
	validChannels5 := eo.filterByRegDomain(scores5, regDomain, "5GHz")

	if len(validChannels24) == 0 || len(validChannels5) == 0 {
		return nil, fmt.Errorf("no valid channels found for regulatory domain %s", regDomain)
	}

	// Select best channels
	best24 := validChannels24[0] // Already sorted by score
	best5 := validChannels5[0]

	// Determine optimal 5GHz width based on score and interference
	width5 := eo.determineOptimalWidthEnhanced(best5)

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

	eo.logger.Info("Enhanced optimal channel plan determined",
		"channel_24", best24.Channel,
		"score_24", best24.RawScore,
		"stars_24", best24.Stars,
		"aps_24", best24.APCount,
		"channel_5", best5.Channel,
		"score_5", best5.RawScore,
		"stars_5", best5.Stars,
		"aps_5", best5.APCount,
		"width_5", width5,
		"total_score", plan.TotalScore,
		"utilization_24", best24.Utilization,
		"utilization_5", best5.Utilization)

	return plan, nil
}

// filterByRegDomain filters channel scores by regulatory domain
func (eo *EnhancedOptimizer) filterByRegDomain(scores []EnhancedChannelScore, regDomain, band string) []EnhancedChannelScore {
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
			if eo.config.UseDFS {
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

	return filtered
}

// determineOptimalWidthEnhanced determines channel width based on enhanced scoring
func (eo *EnhancedOptimizer) determineOptimalWidthEnhanced(score EnhancedChannelScore) string {
	// Use enhanced scoring to determine width
	if score.RawScore >= 90 && score.Utilization < 0.3 && score.StrongInterferers == 0 {
		return "VHT80" // Excellent conditions - use 80MHz
	} else if score.RawScore >= 70 && score.Utilization < 0.5 && score.StrongInterferers <= 1 {
		return "VHT40" // Good conditions - use 40MHz
	} else {
		return "HT20" // Crowded conditions - stick to 20MHz
	}
}

// GetChannelAnalysis returns detailed channel analysis for monitoring/debugging
func (eo *EnhancedOptimizer) GetChannelAnalysis(ctx context.Context) (*ChannelAnalysisReport, error) {
	interfaces, err := eo.detectInterfaces(ctx)
	if err != nil {
		return nil, fmt.Errorf("failed to detect interfaces: %w", err)
	}

	report := &ChannelAnalysisReport{
		Timestamp: time.Now(),
		Bands:     make(map[string][]EnhancedChannelScore),
	}

	for _, iface := range interfaces {
		scores, err := eo.scanner.ScanAndRateChannels(ctx, iface.Name)
		if err != nil {
			eo.logger.Warn("Failed to analyze interface", "interface", iface.Name, "error", err)
			continue
		}
		report.Bands[iface.Band] = scores
	}

	return report, nil
}

// ChannelAnalysisReport contains comprehensive channel analysis
type ChannelAnalysisReport struct {
	Timestamp time.Time                         `json:"timestamp"`
	Bands     map[string][]EnhancedChannelScore `json:"bands"` // "2.4GHz" -> scores, "5GHz" -> scores
}

// GetBestChannelRecommendations returns top channel recommendations for each band
func (car *ChannelAnalysisReport) GetBestChannelRecommendations() map[string]EnhancedChannelScore {
	recommendations := make(map[string]EnhancedChannelScore)

	for band, scores := range car.Bands {
		if len(scores) > 0 {
			recommendations[band] = scores[0] // Already sorted by score
		}
	}

	return recommendations
}
