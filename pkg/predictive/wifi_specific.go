package predictive

import (
	"context"
	"fmt"
	"math"

	"github.com/markus-lassfolk/autonomy/pkg/logx"
)

// WiFiSpecificPredictor handles WiFi-specific predictive logic
type WiFiSpecificPredictor struct {
	logger  *logx.Logger
	samples []ConnectionSample
}

// WiFiAnalysis represents WiFi-specific analysis results
type WiFiAnalysis struct {
	SignalTrend        float64 `json:"signal_trend"`        // Rate of change in signal strength (dBm/sample)
	NoiseTrend         float64 `json:"noise_trend"`         // Rate of change in noise level (dBm/sample)
	QualityTrend       float64 `json:"quality_trend"`       // Rate of change in link quality (%/sample)
	PredictedSignal    float64 `json:"predicted_signal"`    // Predicted signal strength (dBm)
	PredictedNoise     float64 `json:"predicted_noise"`     // Predicted noise level (dBm)
	PredictedQuality   float64 `json:"predicted_quality"`   // Predicted link quality (%)
	InterferenceRisk   float64 `json:"interference_risk"`   // Risk of interference (0-1)
	DisconnectionRisk  float64 `json:"disconnection_risk"`  // Risk of disconnection (0-1)
	ChannelCongestion  float64 `json:"channel_congestion"`  // Channel congestion level (0-1)
	RoamingOpportunity float64 `json:"roaming_opportunity"` // Better AP availability (0-1)
	FrequencyStability float64 `json:"frequency_stability"` // Frequency/channel stability (0-1)
}

// NewWiFiSpecificPredictor creates a new WiFi-specific predictor
func NewWiFiSpecificPredictor(logger *logx.Logger) *WiFiSpecificPredictor {
	return &WiFiSpecificPredictor{
		logger:  logger,
		samples: make([]ConnectionSample, 0),
	}
}

// AddSample adds a new sample for WiFi-specific analysis
func (wsp *WiFiSpecificPredictor) AddSample(ctx context.Context, sample *ConnectionSample) error {
	wsp.samples = append(wsp.samples, *sample)

	// Keep only recent samples (last 300)
	if len(wsp.samples) > 300 {
		wsp.samples = wsp.samples[1:]
	}

	return nil
}

// GetAnalysis returns WiFi-specific analysis
func (wsp *WiFiSpecificPredictor) GetAnalysis(ctx context.Context) *WiFiAnalysis {
	analysis := &WiFiAnalysis{}

	if len(wsp.samples) == 0 {
		return analysis
	}

	// Calculate signal trends
	analysis.SignalTrend = wsp.calculateSignalTrend()
	analysis.NoiseTrend = wsp.calculateNoiseTrend()
	analysis.QualityTrend = wsp.calculateQualityTrend()

	// Predict future values
	analysis.PredictedSignal = wsp.predictFutureSignal()
	analysis.PredictedNoise = wsp.predictFutureNoise()
	analysis.PredictedQuality = wsp.predictFutureQuality()

	// Calculate risks and opportunities
	analysis.InterferenceRisk = wsp.calculateInterferenceRisk()
	analysis.DisconnectionRisk = wsp.calculateDisconnectionRisk()
	analysis.ChannelCongestion = wsp.calculateChannelCongestion()
	analysis.RoamingOpportunity = wsp.calculateRoamingOpportunity()
	analysis.FrequencyStability = wsp.calculateFrequencyStability()

	return analysis
}

// ShouldTriggerFailover checks WiFi-specific failover triggers
func (wsp *WiFiSpecificPredictor) ShouldTriggerFailover(ctx context.Context) (bool, string) {
	if len(wsp.samples) == 0 {
		return false, "no WiFi samples available"
	}

	latest := wsp.samples[len(wsp.samples)-1]
	if latest.Metrics == nil {
		return false, "no metrics in latest sample"
	}

	// Check for rapid signal degradation
	signalTrend := wsp.calculateSignalTrend()
	if signalTrend < -3.0 { // Losing more than 3 dBm per sample
		return true, fmt.Sprintf("rapid signal degradation: %.1f dBm/sample", signalTrend)
	}

	// Check predicted signal strength
	predictedSignal := wsp.predictFutureSignal()
	if predictedSignal < -80 { // Very weak signal predicted
		return true, fmt.Sprintf("predicted weak signal: %.1f dBm", predictedSignal)
	}

	// Check link quality degradation
	qualityTrend := wsp.calculateQualityTrend()
	if qualityTrend < -5.0 { // Losing more than 5% quality per sample
		return true, fmt.Sprintf("rapid quality degradation: %.1f%%/sample", qualityTrend)
	}

	// Check current link quality
	if latest.Metrics.Quality != nil && *latest.Metrics.Quality < 30 {
		return true, fmt.Sprintf("low link quality: %d%%", *latest.Metrics.Quality)
	}

	// Check high interference risk
	if wsp.calculateInterferenceRisk() > 0.8 {
		return true, "high interference risk detected"
	}

	// Check disconnection risk
	if wsp.calculateDisconnectionRisk() > 0.9 {
		return true, "imminent disconnection risk detected"
	}

	// Check for noise increase
	noiseTrend := wsp.calculateNoiseTrend()
	if noiseTrend > 2.0 { // Noise increasing by more than 2 dBm per sample
		return true, fmt.Sprintf("rapid noise increase: %.1f dBm/sample", noiseTrend)
	}

	// Check channel congestion
	if wsp.calculateChannelCongestion() > 0.9 {
		return true, "severe channel congestion detected"
	}

	return false, "no WiFi-specific triggers"
}

// calculateSignalTrend calculates the rate of change in signal strength
func (wsp *WiFiSpecificPredictor) calculateSignalTrend() float64 {
	if len(wsp.samples) < 3 {
		return 0
	}

	recentSamples := wsp.getRecentSamples(20) // Last 20 samples

	var sumX, sumY, sumXY, sumX2 float64
	validSamples := 0

	for i, sample := range recentSamples {
		if sample.Metrics != nil && sample.Metrics.SignalStrength != nil {
			x := float64(i)
			y := float64(*sample.Metrics.SignalStrength)
			sumX += x
			sumY += y
			sumXY += x * y
			sumX2 += x * x
			validSamples++
		}
	}

	if validSamples < 3 {
		return 0
	}

	n := float64(validSamples)
	slope := (n*sumXY - sumX*sumY) / (n*sumX2 - sumX*sumX)

	return slope
}

// calculateNoiseTrend calculates the rate of change in noise level
func (wsp *WiFiSpecificPredictor) calculateNoiseTrend() float64 {
	if len(wsp.samples) < 3 {
		return 0
	}

	recentSamples := wsp.getRecentSamples(20)

	var sumX, sumY, sumXY, sumX2 float64
	validSamples := 0

	for i, sample := range recentSamples {
		if sample.Metrics != nil && sample.Metrics.NoiseLevel != nil {
			x := float64(i)
			y := float64(*sample.Metrics.NoiseLevel)
			sumX += x
			sumY += y
			sumXY += x * y
			sumX2 += x * x
			validSamples++
		}
	}

	if validSamples < 3 {
		return 0
	}

	n := float64(validSamples)
	slope := (n*sumXY - sumX*sumY) / (n*sumX2 - sumX*sumX)

	return slope
}

// calculateQualityTrend calculates the rate of change in link quality
func (wsp *WiFiSpecificPredictor) calculateQualityTrend() float64 {
	if len(wsp.samples) < 3 {
		return 0
	}

	recentSamples := wsp.getRecentSamples(20)

	var sumX, sumY, sumXY, sumX2 float64
	validSamples := 0

	for i, sample := range recentSamples {
		if sample.Metrics != nil && sample.Metrics.Quality != nil {
			x := float64(i)
			y := float64(*sample.Metrics.Quality)
			sumX += x
			sumY += y
			sumXY += x * y
			sumX2 += x * x
			validSamples++
		}
	}

	if validSamples < 3 {
		return 0
	}

	n := float64(validSamples)
	slope := (n*sumXY - sumX*sumY) / (n*sumX2 - sumX*sumX)

	return slope
}

// predictFutureSignal predicts signal strength in the next sample
func (wsp *WiFiSpecificPredictor) predictFutureSignal() float64 {
	if len(wsp.samples) == 0 {
		return 0
	}

	latest := wsp.samples[len(wsp.samples)-1]
	if latest.Metrics == nil || latest.Metrics.SignalStrength == nil {
		return 0
	}

	current := float64(*latest.Metrics.SignalStrength)
	trend := wsp.calculateSignalTrend()

	predicted := current + trend

	// Clamp to reasonable signal strength range [-100, 0] dBm
	return math.Max(-100, math.Min(0, predicted))
}

// predictFutureNoise predicts noise level in the next sample
func (wsp *WiFiSpecificPredictor) predictFutureNoise() float64 {
	if len(wsp.samples) == 0 {
		return 0
	}

	latest := wsp.samples[len(wsp.samples)-1]
	if latest.Metrics == nil || latest.Metrics.NoiseLevel == nil {
		return 0
	}

	current := float64(*latest.Metrics.NoiseLevel)
	trend := wsp.calculateNoiseTrend()

	predicted := current + trend

	// Clamp to reasonable noise level range [-100, -30] dBm
	return math.Max(-100, math.Min(-30, predicted))
}

// predictFutureQuality predicts link quality in the next sample
func (wsp *WiFiSpecificPredictor) predictFutureQuality() float64 {
	if len(wsp.samples) == 0 {
		return 0
	}

	latest := wsp.samples[len(wsp.samples)-1]
	if latest.Metrics == nil || latest.Metrics.Quality == nil {
		return 0
	}

	current := float64(*latest.Metrics.Quality)
	trend := wsp.calculateQualityTrend()

	predicted := current + trend

	// Clamp to valid quality range [0, 100] %
	return math.Max(0, math.Min(100, predicted))
}

// calculateInterferenceRisk calculates the risk of interference
func (wsp *WiFiSpecificPredictor) calculateInterferenceRisk() float64 {
	if len(wsp.samples) == 0 {
		return 0
	}

	risk := 0.0

	// Check noise level increase
	noiseTrend := wsp.calculateNoiseTrend()
	if noiseTrend > 1.0 {
		noiseRisk := math.Min(noiseTrend/5.0, 0.8)
		risk = math.Max(risk, noiseRisk)
	}

	// Check current noise level
	latest := wsp.samples[len(wsp.samples)-1]
	if latest.Metrics != nil && latest.Metrics.NoiseLevel != nil {
		noiseLevel := float64(*latest.Metrics.NoiseLevel)
		if noiseLevel > -60 { // High noise level
			currentNoiseRisk := math.Min((noiseLevel+60)/20.0, 0.9)
			risk = math.Max(risk, currentNoiseRisk)
		}
	}

	// Check SNR degradation (signal to noise ratio)
	if latest.Metrics != nil && latest.Metrics.SignalStrength != nil && latest.Metrics.NoiseLevel != nil {
		snr := float64(*latest.Metrics.SignalStrength - *latest.Metrics.NoiseLevel)
		if snr < 20 { // Poor SNR
			snrRisk := math.Min((20-snr)/15.0, 0.8)
			risk = math.Max(risk, snrRisk)
		}
	}

	return risk
}

// calculateDisconnectionRisk calculates the risk of WiFi disconnection
func (wsp *WiFiSpecificPredictor) calculateDisconnectionRisk() float64 {
	if len(wsp.samples) == 0 {
		return 0
	}

	risk := 0.0
	latest := wsp.samples[len(wsp.samples)-1]

	if latest.Metrics == nil {
		return 0
	}

	// Check signal strength
	if latest.Metrics.SignalStrength != nil {
		signal := float64(*latest.Metrics.SignalStrength)
		if signal < -80 { // Weak signal
			signalRisk := math.Min((80+signal)/(-20), 0.9)
			risk = math.Max(risk, signalRisk)
		}
	}

	// Check link quality
	if latest.Metrics.Quality != nil {
		quality := float64(*latest.Metrics.Quality)
		if quality < 50 { // Poor quality
			qualityRisk := math.Min((50-quality)/50.0, 0.8)
			risk = math.Max(risk, qualityRisk)
		}
	}

	// Check signal degradation trend
	signalTrend := wsp.calculateSignalTrend()
	if signalTrend < -2.0 {
		trendRisk := math.Min(math.Abs(signalTrend)/10.0, 0.7)
		risk = math.Max(risk, trendRisk)
	}

	// Check quality degradation trend
	qualityTrend := wsp.calculateQualityTrend()
	if qualityTrend < -3.0 {
		qualityTrendRisk := math.Min(math.Abs(qualityTrend)/15.0, 0.6)
		risk = math.Max(risk, qualityTrendRisk)
	}

	return risk
}

// calculateChannelCongestion calculates channel congestion level
func (wsp *WiFiSpecificPredictor) calculateChannelCongestion() float64 {
	if len(wsp.samples) == 0 {
		return 0
	}

	// This is a simplified implementation
	// In a full implementation, this would analyze:
	// - Number of other APs on the same channel
	// - Channel utilization percentage
	// - Interference patterns

	congestion := 0.0

	// Check noise level as a proxy for congestion
	latest := wsp.samples[len(wsp.samples)-1]
	if latest.Metrics != nil && latest.Metrics.NoiseLevel != nil {
		noiseLevel := float64(*latest.Metrics.NoiseLevel)
		if noiseLevel > -70 { // High noise indicates congestion
			congestion = math.Min((noiseLevel+70)/20.0, 1.0)
		}
	}

	// Check throughput degradation as another indicator
	if latest.Metrics != nil && latest.Metrics.ThroughputKbps != nil {
		// If we have historical throughput data, compare current vs average
		if len(wsp.samples) >= 10 {
			recentSamples := wsp.getRecentSamples(10)
			var throughputSum float64
			validSamples := 0

			for _, sample := range recentSamples {
				if sample.Metrics != nil && sample.Metrics.ThroughputKbps != nil {
					throughputSum += *sample.Metrics.ThroughputKbps
					validSamples++
				}
			}

			if validSamples > 0 {
				avgThroughput := throughputSum / float64(validSamples)
				currentThroughput := *latest.Metrics.ThroughputKbps

				if avgThroughput > 0 && currentThroughput < avgThroughput*0.5 {
					throughputCongestion := 1.0 - (currentThroughput / avgThroughput)
					congestion = math.Max(congestion, throughputCongestion)
				}
			}
		}
	}

	return congestion
}

// calculateRoamingOpportunity calculates if there's a better AP available
func (wsp *WiFiSpecificPredictor) calculateRoamingOpportunity() float64 {
	// This is a placeholder implementation
	// In a full implementation, this would:
	// - Scan for other available APs
	// - Compare signal strengths
	// - Check if current AP is optimal
	// - Consider roaming thresholds and hysteresis

	// For now, return 0 (no roaming opportunity detected)
	return 0.0
}

// calculateFrequencyStability calculates frequency/channel stability
func (wsp *WiFiSpecificPredictor) calculateFrequencyStability() float64 {
	if len(wsp.samples) < 5 {
		return 1.0 // Assume stable if not enough data
	}

	stability := 1.0

	// Check for channel changes
	recentSamples := wsp.getRecentSamples(10)
	channelChanges := 0

	var lastChannel int
	channelSet := false

	for _, sample := range recentSamples {
		if sample.Metrics != nil && sample.Metrics.Channel != nil {
			currentChannel := *sample.Metrics.Channel
			if channelSet && currentChannel != lastChannel {
				channelChanges++
			}
			lastChannel = currentChannel
			channelSet = true
		}
	}

	if channelChanges > 0 {
		// Reduce stability based on channel changes
		channelStability := math.Max(0, 1.0-float64(channelChanges)/5.0)
		stability = math.Min(stability, channelStability)
	}

	// Check frequency variations (if available)
	frequencyChanges := 0
	var lastFrequency int
	frequencySet := false

	for _, sample := range recentSamples {
		if sample.Metrics != nil && sample.Metrics.Frequency != nil {
			currentFrequency := *sample.Metrics.Frequency
			if frequencySet && math.Abs(float64(currentFrequency-lastFrequency)) > 10 { // 10 MHz tolerance
				frequencyChanges++
			}
			lastFrequency = currentFrequency
			frequencySet = true
		}
	}

	if frequencyChanges > 0 {
		frequencyStability := math.Max(0, 1.0-float64(frequencyChanges)/3.0)
		stability = math.Min(stability, frequencyStability)
	}

	return stability
}

// Helper methods

func (wsp *WiFiSpecificPredictor) getRecentSamples(count int) []ConnectionSample {
	if count >= len(wsp.samples) {
		return wsp.samples
	}
	return wsp.samples[len(wsp.samples)-count:]
}

// GetStatus returns WiFi-specific predictor status
func (wsp *WiFiSpecificPredictor) GetStatus() map[string]interface{} {
	status := map[string]interface{}{
		"samples_count": len(wsp.samples),
	}

	if len(wsp.samples) > 0 {
		latest := wsp.samples[len(wsp.samples)-1]
		if latest.Metrics != nil {
			if latest.Metrics.SignalStrength != nil {
				status["latest_signal"] = *latest.Metrics.SignalStrength
			}
			if latest.Metrics.NoiseLevel != nil {
				status["latest_noise"] = *latest.Metrics.NoiseLevel
			}
			if latest.Metrics.Quality != nil {
				status["latest_quality"] = *latest.Metrics.Quality
			}
			if latest.Metrics.Channel != nil {
				status["latest_channel"] = *latest.Metrics.Channel
			}
		}

		// Add trend information
		status["signal_trend"] = wsp.calculateSignalTrend()
		status["noise_trend"] = wsp.calculateNoiseTrend()
		status["quality_trend"] = wsp.calculateQualityTrend()
		status["interference_risk"] = wsp.calculateInterferenceRisk()
		status["disconnection_risk"] = wsp.calculateDisconnectionRisk()
		status["channel_congestion"] = wsp.calculateChannelCongestion()
		status["frequency_stability"] = wsp.calculateFrequencyStability()
	}

	return status
}
