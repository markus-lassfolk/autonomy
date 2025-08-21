package audit

import (
	"fmt"
	"time"

	"github.com/markus-lassfolk/autonomy/pkg/logx"
)

// RootCause represents a root cause analysis result
type RootCause struct {
	ID              string                 `json:"id"`
	Timestamp       time.Time              `json:"timestamp"`
	DecisionID      string                 `json:"decision_id"`
	Category        string                 `json:"category"`
	Description     string                 `json:"description"`
	Confidence      float64                `json:"confidence"` // 0.0-1.0 confidence in analysis
	Evidence        []string               `json:"evidence"`
	Impact          string                 `json:"impact"` // low, medium, high, critical
	Recommendations []string               `json:"recommendations"`
	Metrics         map[string]interface{} `json:"metrics"`
}

// RootCauseAnalyzer performs automated root cause analysis
type RootCauseAnalyzer struct {
	logger *logx.Logger
}

// NewRootCauseAnalyzer creates a new root cause analyzer
func NewRootCauseAnalyzer(logger *logx.Logger) *RootCauseAnalyzer {
	return &RootCauseAnalyzer{
		logger: logger,
	}
}

// AnalyzeRootCause performs root cause analysis on a decision record
func (rca *RootCauseAnalyzer) AnalyzeRootCause(record *DecisionRecord, relatedRecords []*DecisionRecord) *RootCause {
	if record == nil {
		return nil
	}

	// Analyze based on decision type
	switch record.DecisionType {
	case "failover":
		return rca.analyzeFailoverRootCause(record, relatedRecords)
	case "restore":
		return rca.analyzeRestoreRootCause(record, relatedRecords)
	case "recheck":
		return rca.analyzeRecheckRootCause(record, relatedRecords)
	default:
		return rca.analyzeGenericRootCause(record, relatedRecords)
	}
}

// analyzeFailoverRootCause analyzes root causes for failover decisions
func (rca *RootCauseAnalyzer) analyzeFailoverRootCause(record *DecisionRecord, relatedRecords []*DecisionRecord) *RootCause {
	var evidence []string
	var recommendations []string
	var metrics map[string]interface{}
	confidence := 0.0
	category := "unknown"
	impact := "medium"

	// Analyze metrics for clues
	if record.Metrics != nil {
		metrics = make(map[string]interface{})

		// Check for high latency
		if record.Metrics.LatencyMS != nil && *record.Metrics.LatencyMS > 1000 {
			evidence = append(evidence, fmt.Sprintf("High latency detected: %.1f ms", *record.Metrics.LatencyMS))
			confidence += 0.3
			category = "network_performance"
			impact = "high"
			recommendations = append(recommendations, "Check network connectivity and routing")
		}

		// Check for packet loss
		if record.Metrics.LossPercent != nil && *record.Metrics.LossPercent > 5 {
			evidence = append(evidence, fmt.Sprintf("High packet loss detected: %.1f%%", *record.Metrics.LossPercent))
			confidence += 0.4
			category = "network_reliability"
			impact = "critical"
			recommendations = append(recommendations, "Investigate network infrastructure issues")
		}

		// Check for Starlink-specific issues
		if record.FromMember != nil && record.FromMember.Class == "starlink" {
			if record.Metrics.ObstructionPct != nil && *record.Metrics.ObstructionPct > 10 {
				evidence = append(evidence, fmt.Sprintf("High obstruction detected: %.1f%%", *record.Metrics.ObstructionPct))
				confidence += 0.5
				category = "starlink_obstruction"
				impact = "high"
				recommendations = append(recommendations, "Check for physical obstructions around Starlink dish")
			}

			if record.Metrics.SNR != nil && *record.Metrics.SNR < 5 {
				evidence = append(evidence, fmt.Sprintf("Low SNR detected: %d dB", *record.Metrics.SNR))
				confidence += 0.4
				category = "starlink_signal"
				impact = "high"
				recommendations = append(recommendations, "Check Starlink dish alignment and positioning")
			}
		}

		// Check for cellular-specific issues
		if record.FromMember != nil && record.FromMember.Class == "cellular" {
			if record.Metrics.RSRP != nil && *record.Metrics.RSRP < -110 {
				evidence = append(evidence, fmt.Sprintf("Poor cellular signal: %.1f dBm", *record.Metrics.RSRP))
				confidence += 0.4
				category = "cellular_signal"
				impact = "high"
				recommendations = append(recommendations, "Check cellular antenna positioning and signal strength")
			}
		}

		// Store metrics for reference
		metrics["latency_ms"] = record.Metrics.LatencyMS
		metrics["loss_percent"] = record.Metrics.LossPercent
		metrics["obstruction_pct"] = record.Metrics.ObstructionPct
		metrics["snr"] = record.Metrics.SNR
		metrics["rsrp"] = record.Metrics.RSRP
	}

	// Analyze decision confidence
	if record.Confidence < 0.5 {
		evidence = append(evidence, fmt.Sprintf("Low decision confidence: %.2f", record.Confidence))
		confidence += 0.2
		recommendations = append(recommendations, "Review decision thresholds and scoring logic")
	}

	// Analyze execution time
	if record.ExecutionTime > 5*time.Second {
		evidence = append(evidence, fmt.Sprintf("Slow decision execution: %v", record.ExecutionTime))
		confidence += 0.2
		category = "system_performance"
		recommendations = append(recommendations, "Check system performance and resource usage")
	}

	// Analyze related records for patterns
	if len(relatedRecords) > 0 {
		pattern := rca.analyzeRelatedRecordsPattern(record, relatedRecords)
		if pattern != "" {
			evidence = append(evidence, fmt.Sprintf("Pattern detected: %s", pattern))
			confidence += 0.3
			recommendations = append(recommendations, "Investigate recurring issues and consider preventive measures")
		}
	}

	// Analyze trigger
	if record.Trigger != "" {
		evidence = append(evidence, fmt.Sprintf("Trigger: %s", record.Trigger))
		confidence += 0.1
	}

	// Cap confidence at 1.0
	if confidence > 1.0 {
		confidence = 1.0
	}

	// If no specific evidence found, provide generic analysis
	if len(evidence) == 0 {
		evidence = append(evidence, "No specific indicators found in metrics")
		confidence = 0.1
		category = "unknown"
		recommendations = append(recommendations, "Review system logs for additional clues")
	}

	return &RootCause{
		ID:              fmt.Sprintf("rc_%s_%d", record.DecisionID, time.Now().Unix()),
		Timestamp:       time.Now(),
		DecisionID:      record.DecisionID,
		Category:        category,
		Description:     rca.generateRootCauseDescription(category, evidence),
		Confidence:      confidence,
		Evidence:        evidence,
		Impact:          impact,
		Recommendations: recommendations,
		Metrics:         metrics,
	}
}

// analyzeRestoreRootCause analyzes root causes for restore decisions
func (rca *RootCauseAnalyzer) analyzeRestoreRootCause(record *DecisionRecord, relatedRecords []*DecisionRecord) *RootCause {
	var evidence []string
	var recommendations []string
	confidence := 0.0
	category := "recovery"
	impact := "low"

	// Restore decisions are usually positive
	evidence = append(evidence, "System successfully restored to primary connection")
	confidence = 0.8

	// Check if restore was quick (good) or slow (potential issue)
	if record.ExecutionTime < 2*time.Second {
		evidence = append(evidence, "Quick restoration indicates good system health")
		confidence += 0.1
	} else {
		evidence = append(evidence, fmt.Sprintf("Slow restoration: %v", record.ExecutionTime))
		confidence -= 0.1
		recommendations = append(recommendations, "Investigate why restoration was slow")
	}

	// Check confidence
	if record.Confidence > 0.8 {
		evidence = append(evidence, "High confidence in restoration decision")
		confidence += 0.1
	}

	recommendations = append(recommendations, "Monitor system stability after restoration")

	return &RootCause{
		ID:              fmt.Sprintf("rc_%s_%d", record.DecisionID, time.Now().Unix()),
		Timestamp:       time.Now(),
		DecisionID:      record.DecisionID,
		Category:        category,
		Description:     "System successfully restored to primary connection",
		Confidence:      confidence,
		Evidence:        evidence,
		Impact:          impact,
		Recommendations: recommendations,
		Metrics:         make(map[string]interface{}),
	}
}

// analyzeRecheckRootCause analyzes root causes for recheck decisions
func (rca *RootCauseAnalyzer) analyzeRecheckRootCause(record *DecisionRecord, relatedRecords []*DecisionRecord) *RootCause {
	var evidence []string
	var recommendations []string
	confidence := 0.0
	category := "verification"
	impact := "low"

	evidence = append(evidence, "System performing verification check")
	confidence = 0.6

	// Check if recheck was triggered by specific conditions
	if record.Trigger != "" {
		evidence = append(evidence, fmt.Sprintf("Recheck triggered by: %s", record.Trigger))
		confidence += 0.2
	}

	// Check execution time
	if record.ExecutionTime > 10*time.Second {
		evidence = append(evidence, fmt.Sprintf("Slow recheck execution: %v", record.ExecutionTime))
		confidence -= 0.1
		recommendations = append(recommendations, "Investigate slow recheck performance")
	}

	recommendations = append(recommendations, "Monitor recheck frequency and performance")

	return &RootCause{
		ID:              fmt.Sprintf("rc_%s_%d", record.DecisionID, time.Now().Unix()),
		Timestamp:       time.Now(),
		DecisionID:      record.DecisionID,
		Category:        category,
		Description:     "System verification check performed",
		Confidence:      confidence,
		Evidence:        evidence,
		Impact:          impact,
		Recommendations: recommendations,
		Metrics:         make(map[string]interface{}),
	}
}

// analyzeGenericRootCause analyzes root causes for generic decisions
func (rca *RootCauseAnalyzer) analyzeGenericRootCause(record *DecisionRecord, relatedRecords []*DecisionRecord) *RootCause {
	var evidence []string
	var recommendations []string
	confidence := 0.0
	category := "general"
	impact := "medium"

	evidence = append(evidence, fmt.Sprintf("Generic decision analysis for type: %s", record.DecisionType))
	confidence = 0.3

	// Analyze basic metrics
	if record.Metrics != nil {
		if record.Metrics.LatencyMS != nil {
			evidence = append(evidence, fmt.Sprintf("Latency: %.1f ms", *record.Metrics.LatencyMS))
		}
		if record.Metrics.LossPercent != nil {
			evidence = append(evidence, fmt.Sprintf("Packet loss: %.1f%%", *record.Metrics.LossPercent))
		}
	}

	// Check execution time
	if record.ExecutionTime > 5*time.Second {
		evidence = append(evidence, fmt.Sprintf("Slow execution: %v", record.ExecutionTime))
		confidence += 0.2
		recommendations = append(recommendations, "Investigate system performance")
	}

	recommendations = append(recommendations, "Review decision logic and thresholds")

	return &RootCause{
		ID:              fmt.Sprintf("rc_%s_%d", record.DecisionID, time.Now().Unix()),
		Timestamp:       time.Now(),
		DecisionID:      record.DecisionID,
		Category:        category,
		Description:     fmt.Sprintf("Generic analysis for %s decision", record.DecisionType),
		Confidence:      confidence,
		Evidence:        evidence,
		Impact:          impact,
		Recommendations: recommendations,
		Metrics:         make(map[string]interface{}),
	}
}

// analyzeRelatedRecordsPattern analyzes patterns in related records
func (rca *RootCauseAnalyzer) analyzeRelatedRecordsPattern(record *DecisionRecord, relatedRecords []*DecisionRecord) string {
	if len(relatedRecords) < 3 {
		return ""
	}

	// Count recent failures
	failureCount := 0
	recentWindow := 1 * time.Hour

	for _, related := range relatedRecords {
		if !related.Success &&
			related.Timestamp.After(record.Timestamp.Add(-recentWindow)) &&
			related.DecisionType == record.DecisionType {
			failureCount++
		}
	}

	if failureCount >= 3 {
		return fmt.Sprintf("Multiple failures in last hour (%d failures)", failureCount)
	}

	// Check for same member failures
	if record.FromMember != nil {
		memberFailureCount := 0
		for _, related := range relatedRecords {
			if related.FromMember != nil &&
				related.FromMember.Name == record.FromMember.Name &&
				!related.Success &&
				related.Timestamp.After(record.Timestamp.Add(-recentWindow)) {
				memberFailureCount++
			}
		}

		if memberFailureCount >= 2 {
			return fmt.Sprintf("Recurring failures on member %s (%d failures)", record.FromMember.Name, memberFailureCount)
		}
	}

	return ""
}

// generateRootCauseDescription generates a human-readable description
func (rca *RootCauseAnalyzer) generateRootCauseDescription(category string, evidence []string) string {
	switch category {
	case "network_performance":
		return "Network performance degradation causing connectivity issues"
	case "network_reliability":
		return "Network reliability issues with packet loss or connectivity problems"
	case "starlink_obstruction":
		return "Physical obstructions affecting Starlink signal quality"
	case "starlink_signal":
		return "Poor Starlink signal quality due to alignment or environmental factors"
	case "cellular_signal":
		return "Poor cellular signal strength affecting connectivity"
	case "system_performance":
		return "System performance issues affecting decision execution"
	case "recovery":
		return "System successfully recovered from previous issues"
	case "verification":
		return "System verification check performed"
	default:
		if len(evidence) > 0 {
			return fmt.Sprintf("Issue detected: %s", evidence[0])
		}
		return "Unknown root cause"
	}
}

// GetRootCauseByCategory returns root causes by category
func (rca *RootCauseAnalyzer) GetRootCauseByCategory(records []*DecisionRecord, category string) []*RootCause {
	var results []*RootCause

	for _, record := range records {
		rootCause := rca.AnalyzeRootCause(record, nil)
		if rootCause != nil && rootCause.Category == category {
			results = append(results, rootCause)
		}
	}

	return results
}

// GetRootCauseStats returns statistics about root causes
func (rca *RootCauseAnalyzer) GetRootCauseStats(records []*DecisionRecord) map[string]interface{} {
	stats := make(map[string]interface{})
	categories := make(map[string]int)
	impacts := make(map[string]int)
	var totalConfidence float64
	validRootCauses := 0

	for _, record := range records {
		rootCause := rca.AnalyzeRootCause(record, nil)
		if rootCause != nil {
			categories[rootCause.Category]++
			impacts[rootCause.Impact]++
			totalConfidence += rootCause.Confidence
			validRootCauses++
		}
	}

	stats["total_analyzed"] = len(records)
	stats["valid_root_causes"] = validRootCauses
	stats["categories"] = categories
	stats["impacts"] = impacts

	if validRootCauses > 0 {
		stats["average_confidence"] = totalConfidence / float64(validRootCauses)
	} else {
		stats["average_confidence"] = 0.0
	}

	return stats
}
