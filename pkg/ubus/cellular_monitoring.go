package ubus

import (
	"context"
	"fmt"
	"time"

	"github.com/markus-lassfolk/autonomy/pkg"
	"github.com/markus-lassfolk/autonomy/pkg/collector"
	"github.com/markus-lassfolk/autonomy/pkg/decision"
)

// CellularMonitoringAPI provides ubus API for cellular stability monitoring
type CellularMonitoringAPI struct {
	stabilityCollector *collector.CellularStabilityCollector
	predictiveAnalyzer *decision.CellularPredictiveAnalyzer
	members            []*pkg.Member // Available cellular members
}

// CellularStatusResponse represents the response for cellular status
type CellularStatusResponse struct {
	Timestamp time.Time                     `json:"timestamp"`
	Cellular  map[string]*CellularInterface `json:"cellular"`
	Summary   *CellularSummary              `json:"summary"`
}

// CellularInterface represents status for a single cellular interface
type CellularInterface struct {
	Interface      string                                `json:"interface"`
	Status         string                                `json:"status"`          // "healthy", "degraded", "unhealthy", "critical"
	StabilityScore int                                   `json:"stability_score"` // 0-100
	PredictiveRisk float64                               `json:"predictive_risk"` // 0-1
	CurrentSignal  *CellularSignalInfo                   `json:"current_signal"`
	RecentSamples  []collector.CellularSample            `json:"recent_samples,omitempty"`
	Assessment     *decision.CellularStabilityAssessment `json:"assessment"`
	Recommendation string                                `json:"recommendation"`
	LastUpdate     time.Time                             `json:"last_update"`
}

// CellularSignalInfo represents current signal information
type CellularSignalInfo struct {
	RSRP        float64 `json:"rsrp"`         // dBm
	RSRQ        float64 `json:"rsrq"`         // dB
	SINR        float64 `json:"sinr"`         // dB
	NetworkType string  `json:"network_type"` // LTE, 5G NSA, 5G SA
	Band        string  `json:"band"`         // LTE/5G band
	CellID      string  `json:"cell_id"`      // Cell identifier
	Throughput  float64 `json:"throughput"`   // Kbps
}

// CellularSummary provides overall cellular health summary
type CellularSummary struct {
	TotalInterfaces   int     `json:"total_interfaces"`
	HealthyCount      int     `json:"healthy_count"`
	DegradedCount     int     `json:"degraded_count"`
	UnhealthyCount    int     `json:"unhealthy_count"`
	CriticalCount     int     `json:"critical_count"`
	OverallStatus     string  `json:"overall_status"`
	HighestRisk       float64 `json:"highest_risk"`
	LowestScore       int     `json:"lowest_score"`
	RecommendedAction string  `json:"recommended_action"`
}

// CellularAnalysisResponse represents detailed analysis response
type CellularAnalysisResponse struct {
	Interface       string                                `json:"interface"`
	WindowMinutes   int                                   `json:"window_minutes"`
	SampleCount     int                                   `json:"sample_count"`
	Samples         []collector.CellularSample            `json:"samples"`
	Statistics      *CellularStatistics                   `json:"statistics"`
	Assessment      *decision.CellularStabilityAssessment `json:"assessment"`
	Trends          *CellularTrends                       `json:"trends"`
	Recommendations []string                              `json:"recommendations"`
}

// CellularStatistics provides statistical analysis of cellular data
type CellularStatistics struct {
	RSRP               *SignalStatistics `json:"rsrp"`
	RSRQ               *SignalStatistics `json:"rsrq"`
	SINR               *SignalStatistics `json:"sinr"`
	Throughput         *SignalStatistics `json:"throughput"`
	CellChanges        int               `json:"cell_changes"`
	TimeInHealthyState float64           `json:"time_in_healthy_state"` // percentage
}

// SignalStatistics provides statistical measures for a signal metric
type SignalStatistics struct {
	Min      float64 `json:"min"`
	Max      float64 `json:"max"`
	Mean     float64 `json:"mean"`
	Median   float64 `json:"median"`
	StdDev   float64 `json:"std_dev"`
	Variance float64 `json:"variance"`
	P95      float64 `json:"p95"`
	P99      float64 `json:"p99"`
}

// CellularTrends provides trend analysis
type CellularTrends struct {
	RSRPTrend        string  `json:"rsrp_trend"` // "improving", "stable", "degrading"
	RSRQTrend        string  `json:"rsrq_trend"`
	SINRTrend        string  `json:"sinr_trend"`
	ThroughputTrend  string  `json:"throughput_trend"`
	StabilityTrend   string  `json:"stability_trend"`
	OverallDirection string  `json:"overall_direction"` // "improving", "stable", "degrading"
	Confidence       float64 `json:"confidence"`        // 0-1 confidence in trend analysis
}

// NewCellularMonitoringAPI creates a new cellular monitoring API
func NewCellularMonitoringAPI(stabilityCollector *collector.CellularStabilityCollector,
	predictiveAnalyzer *decision.CellularPredictiveAnalyzer,
) *CellularMonitoringAPI {
	return &CellularMonitoringAPI{
		stabilityCollector: stabilityCollector,
		predictiveAnalyzer: predictiveAnalyzer,
		members:            make([]*pkg.Member, 0),
	}
}

// SetMembers updates the list of available cellular members
func (cma *CellularMonitoringAPI) SetMembers(members []*pkg.Member) {
	cellularMembers := make([]*pkg.Member, 0)
	for _, member := range members {
		if member.Class == "cellular" {
			cellularMembers = append(cellularMembers, member)
		}
	}
	cma.members = cellularMembers
}

// GetCellularStatus returns comprehensive cellular status for all interfaces
func (cma *CellularMonitoringAPI) GetCellularStatus(ctx context.Context) (*CellularStatusResponse, error) {
	response := &CellularStatusResponse{
		Timestamp: time.Now(),
		Cellular:  make(map[string]*CellularInterface),
		Summary:   &CellularSummary{},
	}

	// Collect status for each cellular interface
	for _, member := range cma.members {
		ifaceStatus := cma.getCellularInterfaceStatus(ctx, member)
		response.Cellular[member.Iface] = ifaceStatus

		// Update summary counters
		response.Summary.TotalInterfaces++
		switch ifaceStatus.Status {
		case "healthy":
			response.Summary.HealthyCount++
		case "degraded":
			response.Summary.DegradedCount++
		case "unhealthy":
			response.Summary.UnhealthyCount++
		case "critical":
			response.Summary.CriticalCount++
		}

		// Track highest risk and lowest score
		if ifaceStatus.PredictiveRisk > response.Summary.HighestRisk {
			response.Summary.HighestRisk = ifaceStatus.PredictiveRisk
		}
		if response.Summary.LowestScore == 0 || ifaceStatus.StabilityScore < response.Summary.LowestScore {
			response.Summary.LowestScore = ifaceStatus.StabilityScore
		}
	}

	// Determine overall status and recommended action
	cma.calculateOverallStatus(response.Summary)

	return response, nil
}

// getCellularInterfaceStatus gets status for a single cellular interface
func (cma *CellularMonitoringAPI) getCellularInterfaceStatus(ctx context.Context, member *pkg.Member) *CellularInterface {
	iface := &CellularInterface{
		Interface:     member.Iface,
		Status:        "unknown",
		LastUpdate:    time.Now(),
		CurrentSignal: &CellularSignalInfo{},
	}

	// Get stability information
	if cma.stabilityCollector != nil {
		if stability := cma.stabilityCollector.GetStabilityStatus(member.Iface); stability != nil {
			iface.Status = stability.Status
			iface.StabilityScore = stability.CurrentScore
			iface.PredictiveRisk = stability.PredictiveRisk
			iface.LastUpdate = stability.LastUpdate
		}

		// Get recent samples
		samples := cma.stabilityCollector.GetRecentSamples(5 * time.Minute)
		if len(samples) > 0 {
			// Get most recent sample for current signal info
			latest := samples[len(samples)-1]
			iface.CurrentSignal = &CellularSignalInfo{
				RSRP:        latest.RSRP,
				RSRQ:        latest.RSRQ,
				SINR:        latest.SINR,
				NetworkType: latest.NetworkType,
				Band:        latest.Band,
				CellID:      latest.CellID,
				Throughput:  latest.ThroughputKbps,
			}

			// Include recent samples (last 10 for brevity)
			if len(samples) > 10 {
				iface.RecentSamples = samples[len(samples)-10:]
			} else {
				iface.RecentSamples = samples
			}
		}
	}

	// Get predictive assessment
	if cma.predictiveAnalyzer != nil {
		// Create a basic metrics object from current signal
		metrics := &pkg.Metrics{
			RSRP:           &iface.CurrentSignal.RSRP,
			RSRQ:           &iface.CurrentSignal.RSRQ,
			SINR:           &iface.CurrentSignal.SINR,
			ThroughputKbps: &iface.CurrentSignal.Throughput,
		}

		assessment := cma.predictiveAnalyzer.AnalyzeCellularStability(ctx, member, metrics, cma.stabilityCollector)
		iface.Assessment = assessment
		iface.Recommendation = assessment.RecommendAction

		// Override status if assessment is more critical
		if assessment.Status == "critical" {
			iface.Status = "critical"
		}
	}

	return iface
}

// calculateOverallStatus determines overall cellular health status
func (cma *CellularMonitoringAPI) calculateOverallStatus(summary *CellularSummary) {
	if summary.TotalInterfaces == 0 {
		summary.OverallStatus = "no_interfaces"
		summary.RecommendedAction = "none"
		return
	}

	// Determine overall status based on interface health
	if summary.CriticalCount > 0 {
		summary.OverallStatus = "critical"
		summary.RecommendedAction = "immediate_attention"
	} else if summary.UnhealthyCount > summary.HealthyCount {
		summary.OverallStatus = "unhealthy"
		summary.RecommendedAction = "investigate"
	} else if summary.DegradedCount > 0 {
		summary.OverallStatus = "degraded"
		summary.RecommendedAction = "monitor"
	} else {
		summary.OverallStatus = "healthy"
		summary.RecommendedAction = "none"
	}

	// Escalate based on high predictive risk
	if summary.HighestRisk > 0.8 {
		summary.OverallStatus = "critical"
		summary.RecommendedAction = "prepare_failover"
	}
}

// GetCellularAnalysis returns detailed analysis for a specific interface
func (cma *CellularMonitoringAPI) GetCellularAnalysis(ctx context.Context, interfaceName string, windowMinutes int) (*CellularAnalysisResponse, error) {
	if cma.stabilityCollector == nil {
		return nil, fmt.Errorf("stability collector not available")
	}

	// Find the member for this interface
	var member *pkg.Member
	for _, m := range cma.members {
		if m.Iface == interfaceName {
			member = m
			break
		}
	}
	if member == nil {
		return nil, fmt.Errorf("interface %s not found", interfaceName)
	}

	// Get samples for the specified window
	windowDuration := time.Duration(windowMinutes) * time.Minute
	samples := cma.stabilityCollector.GetRecentSamples(windowDuration)

	response := &CellularAnalysisResponse{
		Interface:     interfaceName,
		WindowMinutes: windowMinutes,
		SampleCount:   len(samples),
		Samples:       samples,
	}

	if len(samples) > 0 {
		// Calculate statistics
		response.Statistics = cma.calculateStatistics(samples)

		// Calculate trends
		response.Trends = cma.calculateTrends(samples)

		// Get predictive assessment
		if cma.predictiveAnalyzer != nil {
			// Use latest sample for metrics
			latest := samples[len(samples)-1]
			metrics := &pkg.Metrics{
				RSRP:           &latest.RSRP,
				RSRQ:           &latest.RSRQ,
				SINR:           &latest.SINR,
				ThroughputKbps: &latest.ThroughputKbps,
			}
			response.Assessment = cma.predictiveAnalyzer.AnalyzeCellularStability(ctx, member, metrics, cma.stabilityCollector)
		}

		// Generate recommendations
		response.Recommendations = cma.generateRecommendations(response)
	}

	return response, nil
}

// calculateStatistics calculates statistical measures for samples
func (cma *CellularMonitoringAPI) calculateStatistics(samples []collector.CellularSample) *CellularStatistics {
	if len(samples) == 0 {
		return nil
	}

	stats := &CellularStatistics{
		RSRP:       cma.calculateSignalStatistics(samples, func(s collector.CellularSample) float64 { return s.RSRP }),
		RSRQ:       cma.calculateSignalStatistics(samples, func(s collector.CellularSample) float64 { return s.RSRQ }),
		SINR:       cma.calculateSignalStatistics(samples, func(s collector.CellularSample) float64 { return s.SINR }),
		Throughput: cma.calculateSignalStatistics(samples, func(s collector.CellularSample) float64 { return s.ThroughputKbps }),
	}

	// Count cell changes
	cellChanges := 0
	lastCellID := ""
	for _, sample := range samples {
		if sample.CellID != "" && sample.CellID != lastCellID {
			if lastCellID != "" {
				cellChanges++
			}
			lastCellID = sample.CellID
		}
	}
	stats.CellChanges = cellChanges

	// Calculate time in healthy state (RSRP > -100, RSRQ > -14, SINR > 0)
	healthyCount := 0
	for _, sample := range samples {
		if sample.RSRP > -100.0 && sample.RSRQ > -14.0 && sample.SINR > 0.0 {
			healthyCount++
		}
	}
	stats.TimeInHealthyState = float64(healthyCount) / float64(len(samples)) * 100.0

	return stats
}

// calculateSignalStatistics calculates statistics for a specific signal metric
func (cma *CellularMonitoringAPI) calculateSignalStatistics(samples []collector.CellularSample, extractor func(collector.CellularSample) float64) *SignalStatistics {
	if len(samples) == 0 {
		return nil
	}

	values := make([]float64, len(samples))
	for i, sample := range samples {
		values[i] = extractor(sample)
	}

	// Sort for percentile calculations
	sortedValues := make([]float64, len(values))
	copy(sortedValues, values)
	// Simple bubble sort for small datasets
	for i := 0; i < len(sortedValues); i++ {
		for j := i + 1; j < len(sortedValues); j++ {
			if sortedValues[i] > sortedValues[j] {
				sortedValues[i], sortedValues[j] = sortedValues[j], sortedValues[i]
			}
		}
	}

	stats := &SignalStatistics{
		Min: sortedValues[0],
		Max: sortedValues[len(sortedValues)-1],
	}

	// Calculate mean
	sum := 0.0
	for _, v := range values {
		sum += v
	}
	stats.Mean = sum / float64(len(values))

	// Calculate median
	mid := len(sortedValues) / 2
	if len(sortedValues)%2 == 0 {
		stats.Median = (sortedValues[mid-1] + sortedValues[mid]) / 2
	} else {
		stats.Median = sortedValues[mid]
	}

	// Calculate variance and standard deviation
	sumSq := 0.0
	for _, v := range values {
		diff := v - stats.Mean
		sumSq += diff * diff
	}
	stats.Variance = sumSq / float64(len(values))
	stats.StdDev = sqrt(stats.Variance)

	// Calculate percentiles
	p95Index := int(float64(len(sortedValues)) * 0.95)
	if p95Index >= len(sortedValues) {
		p95Index = len(sortedValues) - 1
	}
	stats.P95 = sortedValues[p95Index]

	p99Index := int(float64(len(sortedValues)) * 0.99)
	if p99Index >= len(sortedValues) {
		p99Index = len(sortedValues) - 1
	}
	stats.P99 = sortedValues[p99Index]

	return stats
}

// calculateTrends calculates trend analysis for samples
func (cma *CellularMonitoringAPI) calculateTrends(samples []collector.CellularSample) *CellularTrends {
	if len(samples) < 3 {
		return &CellularTrends{
			RSRPTrend:        "insufficient_data",
			RSRQTrend:        "insufficient_data",
			SINRTrend:        "insufficient_data",
			ThroughputTrend:  "insufficient_data",
			StabilityTrend:   "insufficient_data",
			OverallDirection: "insufficient_data",
			Confidence:       0.0,
		}
	}

	trends := &CellularTrends{
		RSRPTrend:       cma.calculateTrend(samples, func(s collector.CellularSample) float64 { return s.RSRP }),
		RSRQTrend:       cma.calculateTrend(samples, func(s collector.CellularSample) float64 { return s.RSRQ }),
		SINRTrend:       cma.calculateTrend(samples, func(s collector.CellularSample) float64 { return s.SINR }),
		ThroughputTrend: cma.calculateTrend(samples, func(s collector.CellularSample) float64 { return s.ThroughputKbps }),
	}

	// Determine overall direction
	improvingCount := 0
	degradingCount := 0

	trends_list := []string{trends.RSRPTrend, trends.RSRQTrend, trends.SINRTrend, trends.ThroughputTrend}
	for _, trend := range trends_list {
		switch trend {
		case "improving":
			improvingCount++
		case "degrading":
			degradingCount++
		}
	}

	if improvingCount > degradingCount {
		trends.OverallDirection = "improving"
		trends.Confidence = float64(improvingCount) / float64(len(trends_list))
	} else if degradingCount > improvingCount {
		trends.OverallDirection = "degrading"
		trends.Confidence = float64(degradingCount) / float64(len(trends_list))
	} else {
		trends.OverallDirection = "stable"
		trends.Confidence = 0.5
	}

	return trends
}

// calculateTrend calculates trend direction for a specific metric
func (cma *CellularMonitoringAPI) calculateTrend(samples []collector.CellularSample, extractor func(collector.CellularSample) float64) string {
	if len(samples) < 3 {
		return "insufficient_data"
	}

	// Compare first third with last third
	thirdSize := len(samples) / 3
	firstThird := samples[:thirdSize]
	lastThird := samples[len(samples)-thirdSize:]

	firstAvg := 0.0
	for _, sample := range firstThird {
		firstAvg += extractor(sample)
	}
	firstAvg /= float64(len(firstThird))

	lastAvg := 0.0
	for _, sample := range lastThird {
		lastAvg += extractor(sample)
	}
	lastAvg /= float64(len(lastThird))

	change := lastAvg - firstAvg
	changePercent := (change / firstAvg) * 100

	if changePercent > 5 {
		return "improving"
	} else if changePercent < -5 {
		return "degrading"
	} else {
		return "stable"
	}
}

// generateRecommendations generates actionable recommendations based on analysis
func (cma *CellularMonitoringAPI) generateRecommendations(analysis *CellularAnalysisResponse) []string {
	recommendations := make([]string, 0)

	if analysis.Statistics == nil || analysis.Trends == nil {
		return recommendations
	}

	// Signal strength recommendations
	if analysis.Statistics.RSRP.Mean < -110 {
		recommendations = append(recommendations, "Poor RSRP signal strength - consider relocating device or using external antenna")
	}

	// Signal quality recommendations
	if analysis.Statistics.RSRQ.Mean < -15 {
		recommendations = append(recommendations, "Poor RSRQ signal quality - interference may be present")
	}

	// SINR recommendations
	if analysis.Statistics.SINR.Mean < 0 {
		recommendations = append(recommendations, "Low SINR indicates high interference - check for nearby signal sources")
	}

	// Stability recommendations
	if analysis.Statistics.RSRP.StdDev > 8 {
		recommendations = append(recommendations, "High RSRP variance detected - signal is unstable")
	}

	// Cell change recommendations
	if analysis.Statistics.CellChanges > 3 {
		recommendations = append(recommendations, "Frequent cell changes detected - device may be in poor coverage area")
	}

	// Throughput recommendations
	if analysis.Statistics.Throughput.Mean < 100 {
		recommendations = append(recommendations, "Low throughput detected - network congestion or poor signal quality")
	}

	// Trend-based recommendations
	if analysis.Trends.OverallDirection == "degrading" && analysis.Trends.Confidence > 0.7 {
		recommendations = append(recommendations, "Signal quality is degrading - monitor closely and prepare for potential failover")
	}

	// Assessment-based recommendations
	if analysis.Assessment != nil {
		if analysis.Assessment.RecommendAction == "failover_now" {
			recommendations = append(recommendations, "CRITICAL: Immediate failover recommended due to severe signal degradation")
		} else if analysis.Assessment.RecommendAction == "prepare_failover" {
			recommendations = append(recommendations, "Prepare for failover - signal conditions are deteriorating")
		}
	}

	return recommendations
}

// sqrt calculates square root (simple implementation)
func sqrt(x float64) float64 {
	if x == 0 {
		return 0
	}

	// Newton's method for square root
	guess := x / 2
	for i := 0; i < 10; i++ {
		guess = (guess + x/guess) / 2
	}
	return guess
}
