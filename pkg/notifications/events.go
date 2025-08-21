package notifications

import (
	"fmt"
	"strings"
	"time"

	"github.com/markus-lassfolk/autonomy/pkg"
)

// EventBuilder provides convenient methods to create notification events
type EventBuilder struct {
	manager *Manager
}

// NewEventBuilder creates a new event builder
func NewEventBuilder(manager *Manager) *EventBuilder {
	return &EventBuilder{
		manager: manager,
	}
}

// FailoverEvent creates a failover notification event
func (eb *EventBuilder) FailoverEvent(from, to *pkg.Member, reason string, metrics *pkg.Metrics) *NotificationEvent {
	var reasonEmoji string
	var reasonText string

	switch strings.ToLower(reason) {
	case "predictive":
		reasonEmoji = "🔮"
		reasonText = "Predictive failover triggered"
	case "quality":
		reasonEmoji = "📶"
		reasonText = "Signal quality degraded"
	case "latency":
		reasonEmoji = "🐌"
		reasonText = "High latency detected"
	case "loss":
		reasonEmoji = "📉"
		reasonText = "Packet loss detected"
	case "manual":
		reasonEmoji = "👤"
		reasonText = "Manual failover requested"
	default:
		reasonEmoji = "🔄"
		reasonText = "Failover triggered"
	}

	title := fmt.Sprintf("%s Network Failover", reasonEmoji)

	var message strings.Builder
	message.WriteString(fmt.Sprintf("%s\n\n", reasonText))

	if from != nil {
		message.WriteString(fmt.Sprintf("From: %s (%s)\n", from.Name, from.Class))
	}
	if to != nil {
		message.WriteString(fmt.Sprintf("To: %s (%s)\n", to.Name, to.Class))
	}

	if metrics != nil {
		message.WriteString("\n📊 Current Metrics:\n")
		if metrics.LatencyMS != nil && *metrics.LatencyMS > 0 {
			latencyIcon := "🟢"
			if *metrics.LatencyMS > 200 {
				latencyIcon = "🟡"
			}
			if *metrics.LatencyMS > 500 {
				latencyIcon = "🔴"
			}
			message.WriteString(fmt.Sprintf("%s Latency: %.1f ms\n", latencyIcon, *metrics.LatencyMS))
		}
		if metrics.LossPercent != nil && *metrics.LossPercent > 0 {
			lossIcon := "🟢"
			if *metrics.LossPercent > 1 {
				lossIcon = "🟡"
			}
			if *metrics.LossPercent > 5 {
				lossIcon = "🔴"
			}
			message.WriteString(fmt.Sprintf("%s Loss: %.1f%%\n", lossIcon, *metrics.LossPercent))
		}
		if metrics.JitterMS != nil && *metrics.JitterMS > 0 {
			message.WriteString(fmt.Sprintf("📈 Jitter: %.1f ms\n", *metrics.JitterMS))
		}
		if metrics.ObstructionPct != nil && *metrics.ObstructionPct > 0 {
			obstructionIcon := "🟢"
			if *metrics.ObstructionPct > 5 {
				obstructionIcon = "🟡"
			}
			if *metrics.ObstructionPct > 15 {
				obstructionIcon = "🔴"
			}
			message.WriteString(fmt.Sprintf("%s Obstruction: %.1f%%\n", obstructionIcon, *metrics.ObstructionPct))
		}
	}

	return &NotificationEvent{
		Type:       NotificationFailover,
		Title:      title,
		Message:    message.String(),
		Timestamp:  time.Now(),
		FromMember: from,
		ToMember:   to,
		Metrics:    metrics,
		Details: map[string]interface{}{
			"reason": reason,
		},
	}
}

// FailbackEvent creates a failback notification event
func (eb *EventBuilder) FailbackEvent(from, to *pkg.Member, metrics *pkg.Metrics) *NotificationEvent {
	title := "✅ Network Restored"

	var message strings.Builder
	message.WriteString("Primary connection restored\n\n")

	if from != nil {
		message.WriteString(fmt.Sprintf("From: %s (%s)\n", from.Name, from.Class))
	}
	if to != nil {
		message.WriteString(fmt.Sprintf("To: %s (%s)\n", to.Name, to.Class))
	}

	if metrics != nil && to != nil {
		message.WriteString("\nRestored Connection Quality:\n")
		if metrics.LatencyMS != nil && *metrics.LatencyMS > 0 {
			message.WriteString(fmt.Sprintf("• Latency: %.1f ms\n", *metrics.LatencyMS))
		}
		if metrics.LossPercent != nil && *metrics.LossPercent >= 0 {
			message.WriteString(fmt.Sprintf("• Loss: %.1f%%\n", *metrics.LossPercent))
		}
		if to.Class == "starlink" && metrics.ObstructionPct != nil && *metrics.ObstructionPct >= 0 {
			message.WriteString(fmt.Sprintf("• Obstruction: %.1f%%\n", *metrics.ObstructionPct))
		}
	}

	return &NotificationEvent{
		Type:       NotificationFailback,
		Title:      title,
		Message:    message.String(),
		Timestamp:  time.Now(),
		FromMember: from,
		ToMember:   to,
		Metrics:    metrics,
	}
}

// MemberDownEvent creates a member down notification event
func (eb *EventBuilder) MemberDownEvent(member *pkg.Member, reason string, metrics *pkg.Metrics) *NotificationEvent {
	var emoji string
	switch member.Class {
	case "starlink":
		emoji = "🛰️"
	case "cellular":
		emoji = "📱"
	case "wifi":
		emoji = "📶"
	case "lan":
		emoji = "🌐"
	default:
		emoji = "⚠️"
	}

	title := fmt.Sprintf("%s %s Connection Down", emoji, strings.Title(string(member.Class)))

	var message strings.Builder
	message.WriteString(fmt.Sprintf("%s connection has failed\n\n", strings.Title(string(member.Class))))
	message.WriteString(fmt.Sprintf("Member: %s\n", member.Name))
	message.WriteString(fmt.Sprintf("Interface: %s\n", member.Iface))

	if reason != "" {
		message.WriteString(fmt.Sprintf("Reason: %s\n", reason))
	}

	if metrics != nil {
		message.WriteString("\nLast Known Metrics:\n")
		if metrics.LatencyMS != nil && *metrics.LatencyMS > 0 {
			message.WriteString(fmt.Sprintf("• Latency: %.1f ms\n", *metrics.LatencyMS))
		}
		if metrics.LossPercent != nil && *metrics.LossPercent > 0 {
			message.WriteString(fmt.Sprintf("• Loss: %.1f%%\n", *metrics.LossPercent))
		}
		if member.Class == "starlink" && metrics.ObstructionPct != nil && *metrics.ObstructionPct > 0 {
			message.WriteString(fmt.Sprintf("• Obstruction: %.1f%%\n", *metrics.ObstructionPct))
		}
		if member.Class == "cellular" {
			if metrics.RSRP != nil && *metrics.RSRP != 0 {
				message.WriteString(fmt.Sprintf("• RSRP: %.1f dBm\n", float64(*metrics.RSRP)))
			}
			if metrics.RSRQ != nil && *metrics.RSRQ != 0 {
				message.WriteString(fmt.Sprintf("• RSRQ: %.1f dB\n", float64(*metrics.RSRQ)))
			}
		}
	}

	return &NotificationEvent{
		Type:      NotificationMemberDown,
		Title:     title,
		Message:   message.String(),
		Timestamp: time.Now(),
		Member:    member,
		Metrics:   metrics,
		Details: map[string]interface{}{
			"reason": reason,
		},
	}
}

// MemberUpEvent creates a member up notification event
func (eb *EventBuilder) MemberUpEvent(member *pkg.Member, metrics *pkg.Metrics) *NotificationEvent {
	var emoji string
	switch member.Class {
	case "starlink":
		emoji = "🛰️"
	case "cellular":
		emoji = "📱"
	case "wifi":
		emoji = "📶"
	case "lan":
		emoji = "🌐"
	default:
		emoji = "✅"
	}

	title := fmt.Sprintf("%s %s Connection Restored", emoji, strings.Title(string(member.Class)))

	var message strings.Builder
	message.WriteString(fmt.Sprintf("%s connection is back online\n\n", strings.Title(string(member.Class))))
	message.WriteString(fmt.Sprintf("Member: %s\n", member.Name))
	message.WriteString(fmt.Sprintf("Interface: %s\n", member.Iface))

	if metrics != nil {
		message.WriteString("\nCurrent Quality:\n")
		if metrics.LatencyMS != nil && *metrics.LatencyMS > 0 {
			message.WriteString(fmt.Sprintf("• Latency: %.1f ms\n", *metrics.LatencyMS))
		}
		if metrics.LossPercent != nil && *metrics.LossPercent >= 0 {
			message.WriteString(fmt.Sprintf("• Loss: %.1f%%\n", *metrics.LossPercent))
		}
		if member.Class == "starlink" && metrics.ObstructionPct != nil && *metrics.ObstructionPct >= 0 {
			message.WriteString(fmt.Sprintf("• Obstruction: %.1f%%\n", *metrics.ObstructionPct))
		}
		if member.Class == "cellular" {
			if metrics.RSRP != nil && *metrics.RSRP != 0 {
				message.WriteString(fmt.Sprintf("• RSRP: %.1f dBm\n", float64(*metrics.RSRP)))
			}
			if metrics.RSRQ != nil && *metrics.RSRQ != 0 {
				message.WriteString(fmt.Sprintf("• RSRQ: %.1f dB\n", float64(*metrics.RSRQ)))
			}
		}
	}

	return &NotificationEvent{
		Type:      NotificationMemberUp,
		Title:     title,
		Message:   message.String(),
		Timestamp: time.Now(),
		Member:    member,
		Metrics:   metrics,
	}
}

// PredictiveEvent creates a predictive warning notification event
func (eb *EventBuilder) PredictiveEvent(member *pkg.Member, prediction string, confidence float64, metrics *pkg.Metrics) *NotificationEvent {
	title := "🔮 Predictive Warning"

	var message strings.Builder
	message.WriteString(fmt.Sprintf("Potential issue predicted for %s\n\n", member.Name))
	message.WriteString(fmt.Sprintf("Prediction: %s\n", prediction))
	message.WriteString(fmt.Sprintf("Confidence: %.1f%%\n", confidence*100))
	message.WriteString(fmt.Sprintf("Member: %s (%s)\n", member.Name, member.Class))

	if metrics != nil {
		message.WriteString("\nCurrent Metrics:\n")
		if metrics.LatencyMS != nil && *metrics.LatencyMS > 0 {
			message.WriteString(fmt.Sprintf("• Latency: %.1f ms\n", *metrics.LatencyMS))
		}
		if metrics.LossPercent != nil && *metrics.LossPercent > 0 {
			message.WriteString(fmt.Sprintf("• Loss: %.1f%%\n", *metrics.LossPercent))
		}
		if member.Class == "starlink" && metrics.ObstructionPct != nil && *metrics.ObstructionPct > 0 {
			message.WriteString(fmt.Sprintf("• Obstruction: %.1f%% (trending up)\n", *metrics.ObstructionPct))
		}
	}

	message.WriteString("\nRecommendation: Monitor connection closely")

	return &NotificationEvent{
		Type:      NotificationPredictive,
		Title:     title,
		Message:   message.String(),
		Timestamp: time.Now(),
		Member:    member,
		Metrics:   metrics,
		Details: map[string]interface{}{
			"prediction": prediction,
			"confidence": confidence,
		},
	}
}

// CriticalErrorEvent creates a critical error notification event
func (eb *EventBuilder) CriticalErrorEvent(component string, error error, details map[string]interface{}) *NotificationEvent {
	title := "🚨 CRITICAL: System Error"

	var message strings.Builder
	message.WriteString(fmt.Sprintf("Critical error in %s\n\n", component))
	message.WriteString(fmt.Sprintf("Error: %s\n", error.Error()))

	if details != nil {
		message.WriteString("\nDetails:\n")
		for key, value := range details {
			message.WriteString(fmt.Sprintf("• %s: %v\n", key, value))
		}
	}

	message.WriteString("\nImmediate attention required!")

	return &NotificationEvent{
		Type:      NotificationCriticalError,
		Title:     title,
		Message:   message.String(),
		Priority:  PriorityEmergency,
		Timestamp: time.Now(),
		Error:     error,
		Details:   details,
	}
}

// RecoveryEvent creates a system recovery notification event
func (eb *EventBuilder) RecoveryEvent(component string, details map[string]interface{}) *NotificationEvent {
	title := "✅ System Recovered"

	var message strings.Builder
	message.WriteString(fmt.Sprintf("%s has recovered\n\n", component))

	if details != nil {
		message.WriteString("Recovery Details:\n")
		for key, value := range details {
			message.WriteString(fmt.Sprintf("• %s: %v\n", key, value))
		}
	}

	message.WriteString("\nSystem is operating normally")

	return &NotificationEvent{
		Type:      NotificationRecovery,
		Title:     title,
		Message:   message.String(),
		Timestamp: time.Now(),
		Details:   details,
	}
}

// StatusUpdateEvent creates a status update notification event
func (eb *EventBuilder) StatusUpdateEvent(summary string, stats map[string]interface{}) *NotificationEvent {
	title := "📊 Status Update"

	var message strings.Builder
	message.WriteString(fmt.Sprintf("%s\n\n", summary))

	if stats != nil {
		message.WriteString("Current Status:\n")
		for key, value := range stats {
			message.WriteString(fmt.Sprintf("• %s: %v\n", key, value))
		}
	}

	return &NotificationEvent{
		Type:      NotificationStatusUpdate,
		Title:     title,
		Message:   message.String(),
		Priority:  PriorityLow,
		Timestamp: time.Now(),
		Details:   stats,
	}
}

// SummaryEvent creates a periodic summary notification event
func (eb *EventBuilder) SummaryEvent(period string, stats map[string]interface{}) *NotificationEvent {
	title := fmt.Sprintf("📈 %s Summary", period)

	var message strings.Builder
	message.WriteString(fmt.Sprintf("Network activity summary for %s\n\n", strings.ToLower(period)))

	for key, value := range stats {
		message.WriteString(fmt.Sprintf("• %s: %v\n", key, value))
	}

	return &NotificationEvent{
		Type:      NotificationSummary,
		Title:     title,
		Message:   message.String(),
		Priority:  PriorityLow,
		Timestamp: time.Now(),
		Details:   stats,
	}
}

// TestEvent creates a test notification event
func (eb *EventBuilder) TestEvent() *NotificationEvent {
	return &NotificationEvent{
		Type:      NotificationStatusUpdate,
		Title:     "🧪 Test Notification",
		Message:   "This is a test notification from autonomy.\n\nIf you receive this, notifications are working correctly!",
		Priority:  PriorityNormal,
		Sound:     "pushover",
		Timestamp: time.Now(),
	}
}

// CustomEvent creates a custom notification event
func (eb *EventBuilder) CustomEvent(notType NotificationType, title, message string, priority int) *NotificationEvent {
	return &NotificationEvent{
		Type:      notType,
		Title:     title,
		Message:   message,
		Priority:  priority,
		Timestamp: time.Now(),
	}
}
