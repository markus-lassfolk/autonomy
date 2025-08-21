package notifications

import (
	"time"

	"github.com/markus-lassfolk/autonomy/pkg/logx"
)

// EmergencyDetector detects emergency conditions that require immediate attention
type EmergencyDetector struct {
	config *IntelligenceConfig
	logger *logx.Logger
}

// NewEmergencyDetector creates a new emergency detector
func NewEmergencyDetector(config *IntelligenceConfig, logger *logx.Logger) *EmergencyDetector {
	return &EmergencyDetector{
		config: config,
		logger: logger,
	}
}

// DetectEmergency analyzes system state and alert context to detect emergency conditions
func (ed *EmergencyDetector) DetectEmergency(
	systemState *SystemState,
	alertType AlertType,
	baseData map[string]interface{},
) EmergencyLevel {
	if !ed.config.EmergencyDetectionEnabled {
		return EmergencyNone
	}

	maxLevel := EmergencyNone

	// Check system health emergencies
	if systemLevel := ed.checkSystemHealthEmergency(systemState.SystemHealth); systemLevel > maxLevel {
		maxLevel = systemLevel
	}

	// Check network health emergencies
	if networkLevel := ed.checkNetworkHealthEmergency(systemState.NetworkHealth); networkLevel > maxLevel {
		maxLevel = networkLevel
	}

	// Check cascading failure patterns
	if cascadeLevel := ed.checkCascadingFailures(systemState.ActiveIncidents, systemState.RecentFailures); cascadeLevel > maxLevel {
		maxLevel = cascadeLevel
	}

	// Check alert-specific emergency conditions
	if alertLevel := ed.checkAlertSpecificEmergency(alertType, baseData); alertLevel > maxLevel {
		maxLevel = alertLevel
	}

	// Check temporal emergency patterns
	if temporalLevel := ed.checkTemporalEmergency(systemState); temporalLevel > maxLevel {
		maxLevel = temporalLevel
	}

	if maxLevel > EmergencyNone {
		ed.logger.Warn("Emergency condition detected",
			"level", maxLevel,
			"alert_type", alertType,
			"system_cpu", systemState.SystemHealth.CPUUsage,
			"system_memory", systemState.SystemHealth.MemoryUsage,
			"system_temperature", systemState.SystemHealth.Temperature)
	}

	return maxLevel
}

// checkSystemHealthEmergency checks for system health emergencies
func (ed *EmergencyDetector) checkSystemHealthEmergency(health *SystemHealthState) EmergencyLevel {
	if health == nil {
		return EmergencyNone
	}

	thresholds := ed.config.EmergencyThresholds

	// Critical system conditions
	if health.CPUUsage >= thresholds.CPUUsageEmergency ||
		health.MemoryUsage >= thresholds.MemoryUsageEmergency ||
		health.Temperature >= thresholds.TemperatureEmergency ||
		health.DiskUsage >= thresholds.DiskUsageEmergency {
		return EmergencyCritical
	}

	// High emergency conditions (80% of critical thresholds)
	if health.CPUUsage >= thresholds.CPUUsageEmergency*0.8 ||
		health.MemoryUsage >= thresholds.MemoryUsageEmergency*0.8 ||
		health.Temperature >= thresholds.TemperatureEmergency*0.8 ||
		health.DiskUsage >= thresholds.DiskUsageEmergency*0.8 {
		return EmergencyHigh
	}

	// Medium emergency conditions (60% of critical thresholds)
	if health.CPUUsage >= thresholds.CPUUsageEmergency*0.6 ||
		health.MemoryUsage >= thresholds.MemoryUsageEmergency*0.6 ||
		health.Temperature >= thresholds.TemperatureEmergency*0.6 ||
		health.DiskUsage >= thresholds.DiskUsageEmergency*0.6 {
		return EmergencyMedium
	}

	return EmergencyNone
}

// checkNetworkHealthEmergency checks for network health emergencies
func (ed *EmergencyDetector) checkNetworkHealthEmergency(health *NetworkHealthState) EmergencyLevel {
	if health == nil {
		return EmergencyNone
	}

	thresholds := ed.config.EmergencyThresholds

	// Critical network conditions
	if !health.PrimaryInterfaceUp && health.BackupInterfacesUp == 0 {
		return EmergencyCritical // Complete network failure
	}

	if health.AveragePacketLoss >= thresholds.PacketLossEmergency ||
		health.AverageLatency >= thresholds.LatencyEmergency {
		return EmergencyCritical
	}

	// High emergency conditions
	if !health.PrimaryInterfaceUp ||
		health.BackupInterfacesUp < health.TotalInterfaces/2 {
		return EmergencyHigh
	}

	if health.AveragePacketLoss >= thresholds.PacketLossEmergency*0.6 ||
		health.AverageLatency >= thresholds.LatencyEmergency*0.6 {
		return EmergencyHigh
	}

	// Medium emergency conditions
	if health.AveragePacketLoss >= thresholds.PacketLossEmergency*0.3 ||
		health.AverageLatency >= thresholds.LatencyEmergency*0.3 {
		return EmergencyMedium
	}

	return EmergencyNone
}

// checkCascadingFailures checks for cascading failure patterns
func (ed *EmergencyDetector) checkCascadingFailures(incidents []ActiveIncident, failures []FailureRecord) EmergencyLevel {
	thresholds := ed.config.EmergencyThresholds
	now := time.Now()

	// Count recent high-severity incidents
	recentHighSeverity := 0
	for _, incident := range incidents {
		if incident.Severity >= int(EmergencyHigh) &&
			now.Sub(incident.StartTime) < 10*time.Minute {
			recentHighSeverity++
		}
	}

	if recentHighSeverity >= thresholds.CascadingFailureCount {
		return EmergencyCritical
	}

	// Count failures in the last minute
	recentFailures := 0
	cutoff := now.Add(-1 * time.Minute)
	for _, failure := range failures {
		if failure.Timestamp.After(cutoff) {
			recentFailures++
		}
	}

	failureRate := float64(recentFailures)
	if failureRate >= thresholds.FailureRateEmergency {
		return EmergencyHigh
	}

	if failureRate >= thresholds.FailureRateEmergency*0.6 {
		return EmergencyMedium
	}

	return EmergencyNone
}

// checkAlertSpecificEmergency checks for alert-type specific emergency conditions
func (ed *EmergencyDetector) checkAlertSpecificEmergency(alertType AlertType, baseData map[string]interface{}) EmergencyLevel {
	switch alertType {
	case AlertThermal:
		if temp, ok := baseData["temperature"].(float64); ok {
			if temp >= ed.config.EmergencyThresholds.TemperatureEmergency {
				return EmergencyCritical
			}
			if temp >= ed.config.EmergencyThresholds.TemperatureEmergency*0.8 {
				return EmergencyHigh
			}
		}

	case AlertFailover:
		// Multiple rapid failovers indicate emergency
		if failoverCount, ok := baseData["recent_failover_count"].(int); ok {
			if failoverCount >= 3 {
				return EmergencyHigh
			}
		}

	case AlertConnectivityIssue:
		if loss, ok := baseData["packet_loss"].(float64); ok {
			if loss >= ed.config.EmergencyThresholds.PacketLossEmergency {
				return EmergencyCritical
			}
		}
		if latency, ok := baseData["latency"].(float64); ok {
			if latency >= ed.config.EmergencyThresholds.LatencyEmergency {
				return EmergencyHigh
			}
		}

	case AlertSystemHealth:
		if severity, ok := baseData["severity"].(string); ok {
			switch severity {
			case "critical":
				return EmergencyCritical
			case "high":
				return EmergencyHigh
			case "medium":
				return EmergencyMedium
			}
		}

	case AlertDataLimit:
		if usage, ok := baseData["usage_percent"].(float64); ok {
			if usage >= 100.0 {
				return EmergencyMedium // Data limit exceeded
			}
		}

	case AlertPredictive:
		if confidence, ok := baseData["confidence"].(float64); ok {
			if confidence >= 90.0 {
				return EmergencyHigh // High confidence prediction
			}
			if confidence >= 70.0 {
				return EmergencyMedium
			}
		}
	}

	// Check for explicit emergency flag
	if emergency, ok := baseData["emergency"].(bool); ok && emergency {
		return EmergencyHigh
	}

	return EmergencyNone
}

// checkTemporalEmergency checks for time-based emergency patterns
func (ed *EmergencyDetector) checkTemporalEmergency(systemState *SystemState) EmergencyLevel {
	now := time.Now()

	// Check for prolonged incidents
	for _, incident := range systemState.ActiveIncidents {
		duration := now.Sub(incident.StartTime)

		if duration >= ed.config.EmergencyThresholds.ServiceDowntimeEmergency {
			if incident.Severity >= int(EmergencyHigh) {
				return EmergencyCritical
			} else {
				return EmergencyHigh
			}
		}
	}

	// Check for repeated failures of the same component
	componentFailures := make(map[string]int)
	cutoff := now.Add(-1 * time.Hour)

	for _, failure := range systemState.RecentFailures {
		if failure.Timestamp.After(cutoff) {
			componentFailures[failure.Component]++
		}
	}

	for component, count := range componentFailures {
		if count >= 5 { // 5 failures in an hour
			ed.logger.Warn("Repeated component failures detected",
				"component", component,
				"failures", count)
			return EmergencyMedium
		}
	}

	return EmergencyNone
}

// IsEmergencyLevel checks if a given level qualifies as an emergency
func IsEmergencyLevel(level EmergencyLevel) bool {
	return level > EmergencyNone
}

// GetEmergencyLevelString returns string representation of emergency level
func GetEmergencyLevelString(level EmergencyLevel) string {
	switch level {
	case EmergencyNone:
		return "none"
	case EmergencyLow:
		return "low"
	case EmergencyMedium:
		return "medium"
	case EmergencyHigh:
		return "high"
	case EmergencyCritical:
		return "critical"
	default:
		return "unknown"
	}
}

// GetEmergencyLevelPriority returns notification priority for emergency level
func GetEmergencyLevelPriority(level EmergencyLevel) int {
	switch level {
	case EmergencyNone:
		return PriorityNormal
	case EmergencyLow:
		return PriorityNormal
	case EmergencyMedium:
		return PriorityHigh
	case EmergencyHigh:
		return PriorityEmergency
	case EmergencyCritical:
		return PriorityEmergency
	default:
		return PriorityNormal
	}
}
