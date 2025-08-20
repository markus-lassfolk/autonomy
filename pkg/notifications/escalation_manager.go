package notifications

import (
	"fmt"
	"sync"
	"time"

	"github.com/markus-lassfolk/autonomy/pkg/logx"
)

// EscalationManager handles notification escalation for critical situations
type EscalationManager struct {
	config *IntelligenceConfig
	logger *logx.Logger

	// Escalation tracking
	mu                sync.RWMutex
	activeEscalations map[string]*EscalationChain
	escalationHistory []EscalationRecord
}

// EscalationChain represents an active escalation sequence
type EscalationChain struct {
	ID             string                 `json:"id"`
	IncidentID     string                 `json:"incident_id"`
	AlertType      AlertType              `json:"alert_type"`
	StartTime      time.Time              `json:"start_time"`
	CurrentLevel   int                    `json:"current_level"`
	MaxLevel       int                    `json:"max_level"`
	NextEscalation time.Time              `json:"next_escalation"`
	Contacts       []EscalationContact    `json:"contacts"`
	Status         EscalationStatus       `json:"status"`
	Context        map[string]interface{} `json:"context"`
	Acknowledged   bool                   `json:"acknowledged"`
	AcknowledgedBy string                 `json:"acknowledged_by,omitempty"`
	AcknowledgedAt *time.Time             `json:"acknowledged_at,omitempty"`
}

// EscalationContact represents a contact in the escalation chain
type EscalationContact struct {
	Level           int                   `json:"level"`
	Name            string                `json:"name"`
	Channels        []NotificationChannel `json:"channels"`
	ResponseTimeout time.Duration         `json:"response_timeout"`
	Contacted       bool                  `json:"contacted"`
	ContactedAt     *time.Time            `json:"contacted_at,omitempty"`
	Responded       bool                  `json:"responded"`
	RespondedAt     *time.Time            `json:"responded_at,omitempty"`
}

// EscalationStatus represents the status of an escalation
type EscalationStatus string

const (
	EscalationActive    EscalationStatus = "active"
	EscalationPaused    EscalationStatus = "paused"
	EscalationCompleted EscalationStatus = "completed"
	EscalationCancelled EscalationStatus = "cancelled"
)

// EscalationRecord represents a historical escalation record
type EscalationRecord struct {
	ID              string           `json:"id"`
	IncidentID      string           `json:"incident_id"`
	AlertType       AlertType        `json:"alert_type"`
	StartTime       time.Time        `json:"start_time"`
	EndTime         time.Time        `json:"end_time"`
	Duration        time.Duration    `json:"duration"`
	MaxLevelReached int              `json:"max_level_reached"`
	TotalContacts   int              `json:"total_contacts"`
	ResponseTime    time.Duration    `json:"response_time"`
	Status          EscalationStatus `json:"status"`
	Effectiveness   float64          `json:"effectiveness"`
}

// NewEscalationManager creates a new escalation manager
func NewEscalationManager(config *IntelligenceConfig, logger *logx.Logger) *EscalationManager {
	em := &EscalationManager{
		config:            config,
		logger:            logger,
		activeEscalations: make(map[string]*EscalationChain),
		escalationHistory: make([]EscalationRecord, 0),
	}

	// Start escalation monitoring loop
	go em.escalationLoop()

	return em
}

// TriggerEmergencyEscalation triggers escalation for an emergency incident
func (em *EscalationManager) TriggerEmergencyEscalation(incident *ActiveIncident) error {
	if !em.config.EscalationEnabled {
		return nil
	}

	em.mu.Lock()
	defer em.mu.Unlock()

	// Check if escalation already exists for this incident
	if _, exists := em.activeEscalations[incident.ID]; exists {
		em.logger.Debug("Escalation already active for incident", "incident_id", incident.ID)
		return nil
	}

	// Create escalation chain
	escalationID := fmt.Sprintf("esc_%s_%d", incident.ID, time.Now().UnixNano())

	chain := &EscalationChain{
		ID:             escalationID,
		IncidentID:     incident.ID,
		AlertType:      AlertType(incident.Type),
		StartTime:      time.Now(),
		CurrentLevel:   1,
		MaxLevel:       em.config.MaxEscalationLevel,
		NextEscalation: time.Now().Add(5 * time.Minute), // First escalation in 5 minutes
		Contacts:       em.getEscalationContacts(AlertType(incident.Type), incident.Severity),
		Status:         EscalationActive,
		Context:        incident.Context,
		Acknowledged:   false,
	}

	em.activeEscalations[escalationID] = chain

	em.logger.Info("Emergency escalation triggered",
		"escalation_id", escalationID,
		"incident_id", incident.ID,
		"alert_type", incident.Type,
		"severity", incident.Severity,
		"contacts", len(chain.Contacts))

	// Send initial emergency notification
	go em.sendEscalationNotification(chain, 1)

	return nil
}

// getEscalationContacts returns escalation contacts based on alert type and severity
func (em *EscalationManager) getEscalationContacts(alertType AlertType, severity int) []EscalationContact {
	// In a real implementation, this would be configurable
	// For now, return a default escalation chain

	contacts := []EscalationContact{
		{
			Level:           1,
			Name:            "On-Call Engineer",
			Channels:        []NotificationChannel{ChannelPushover, ChannelSlack},
			ResponseTimeout: 5 * time.Minute,
		},
		{
			Level:           2,
			Name:            "Team Lead",
			Channels:        []NotificationChannel{ChannelPushover, ChannelEmail, ChannelSlack},
			ResponseTimeout: 10 * time.Minute,
		},
		{
			Level:           3,
			Name:            "Engineering Manager",
			Channels:        []NotificationChannel{ChannelPushover, ChannelEmail, ChannelSlack, ChannelTelegram},
			ResponseTimeout: 15 * time.Minute,
		},
	}

	// Adjust based on severity
	if severity >= int(EmergencyCritical) {
		// Add executive escalation for critical emergencies
		contacts = append(contacts, EscalationContact{
			Level:           4,
			Name:            "VP Engineering",
			Channels:        []NotificationChannel{ChannelPushover, ChannelEmail, ChannelTelegram},
			ResponseTimeout: 20 * time.Minute,
		})
	}

	return contacts
}

// sendEscalationNotification sends notification for a specific escalation level
func (em *EscalationManager) sendEscalationNotification(chain *EscalationChain, level int) {
	// Find contact for this level
	var contact *EscalationContact
	for i := range chain.Contacts {
		if chain.Contacts[i].Level == level {
			contact = &chain.Contacts[i]
			break
		}
	}

	if contact == nil {
		em.logger.Error("No contact found for escalation level", "level", level, "chain_id", chain.ID)
		return
	}

	// Mark as contacted
	now := time.Now()
	contact.Contacted = true
	contact.ContactedAt = &now

	// Create escalation notification
	notification := &Notification{
		Type:      NotificationType(chain.AlertType),
		Title:     fmt.Sprintf("🚨 ESCALATION LEVEL %d: %s", level, chain.AlertType),
		Message:   em.createEscalationMessage(chain, contact),
		Priority:  PriorityEmergency,
		Timestamp: time.Now(),
		Context: map[string]interface{}{
			"escalation_id":    chain.ID,
			"incident_id":      chain.IncidentID,
			"escalation_level": level,
			"contact_name":     contact.Name,
			"response_timeout": contact.ResponseTimeout.String(),
		},
	}

	em.logger.Info("Sending escalation notification",
		"escalation_id", chain.ID,
		"level", level,
		"contact", contact.Name,
		"channels", len(contact.Channels))

	// Send through each preferred channel for this contact
	// In a real implementation, this would integrate with the notification system
	// For now, log the escalation
	em.logger.Warn("ESCALATION NOTIFICATION",
		"level", level,
		"contact", contact.Name,
		"title", notification.Title,
		"message", notification.Message)
}

// createEscalationMessage creates the escalation notification message
func (em *EscalationManager) createEscalationMessage(chain *EscalationChain, contact *EscalationContact) string {
	duration := time.Since(chain.StartTime)

	message := fmt.Sprintf(`🚨 EMERGENCY ESCALATION - Level %d

📋 Incident Details:
• Incident ID: %s
• Alert Type: %s
• Duration: %s
• Escalation Level: %d/%d

👤 Escalated To: %s
⏰ Response Required Within: %s

🔍 Context:
• Started: %s
• Previous levels contacted: %d
• Acknowledgment required to stop escalation

⚡ Action Required:
1. Acknowledge this escalation immediately
2. Investigate the incident
3. Take corrective action
4. Update incident status

🆘 To acknowledge: Reply "ACK %s" or use the incident management system`,
		contact.Level,
		chain.IncidentID,
		chain.AlertType,
		duration.Round(time.Second),
		chain.CurrentLevel,
		chain.MaxLevel,
		contact.Name,
		contact.ResponseTimeout.String(),
		chain.StartTime.Format("2006-01-02 15:04:05 UTC"),
		contact.Level-1,
		chain.ID)

	// Add context information if available
	if len(chain.Context) > 0 {
		message += "\n\n📊 Additional Context:"
		for key, value := range chain.Context {
			message += fmt.Sprintf("\n• %s: %v", key, value)
		}
	}

	return message
}

// AcknowledgeEscalation acknowledges an escalation to stop further escalation
func (em *EscalationManager) AcknowledgeEscalation(escalationID, acknowledgedBy string) error {
	em.mu.Lock()
	defer em.mu.Unlock()

	chain, exists := em.activeEscalations[escalationID]
	if !exists {
		return fmt.Errorf("escalation not found: %s", escalationID)
	}

	if chain.Acknowledged {
		return fmt.Errorf("escalation already acknowledged")
	}

	// Mark as acknowledged
	now := time.Now()
	chain.Acknowledged = true
	chain.AcknowledgedBy = acknowledgedBy
	chain.AcknowledgedAt = &now
	chain.Status = EscalationPaused

	em.logger.Info("Escalation acknowledged",
		"escalation_id", escalationID,
		"acknowledged_by", acknowledgedBy,
		"duration", time.Since(chain.StartTime))

	// Record escalation completion
	em.recordEscalationCompletion(chain)

	return nil
}

// CancelEscalation cancels an active escalation
func (em *EscalationManager) CancelEscalation(escalationID, reason string) error {
	em.mu.Lock()
	defer em.mu.Unlock()

	chain, exists := em.activeEscalations[escalationID]
	if !exists {
		return fmt.Errorf("escalation not found: %s", escalationID)
	}

	chain.Status = EscalationCancelled

	em.logger.Info("Escalation cancelled",
		"escalation_id", escalationID,
		"reason", reason,
		"duration", time.Since(chain.StartTime))

	// Record escalation completion
	em.recordEscalationCompletion(chain)

	return nil
}

// recordEscalationCompletion records the completion of an escalation
func (em *EscalationManager) recordEscalationCompletion(chain *EscalationChain) {
	endTime := time.Now()
	duration := endTime.Sub(chain.StartTime)

	// Calculate response time (time to acknowledgment)
	responseTime := duration
	if chain.AcknowledgedAt != nil {
		responseTime = chain.AcknowledgedAt.Sub(chain.StartTime)
	}

	// Calculate effectiveness score
	effectiveness := em.calculateEscalationEffectiveness(chain, responseTime)

	record := EscalationRecord{
		ID:              chain.ID,
		IncidentID:      chain.IncidentID,
		AlertType:       chain.AlertType,
		StartTime:       chain.StartTime,
		EndTime:         endTime,
		Duration:        duration,
		MaxLevelReached: chain.CurrentLevel,
		TotalContacts:   len(chain.Contacts),
		ResponseTime:    responseTime,
		Status:          chain.Status,
		Effectiveness:   effectiveness,
	}

	em.escalationHistory = append(em.escalationHistory, record)

	// Keep only last 100 records
	if len(em.escalationHistory) > 100 {
		em.escalationHistory = em.escalationHistory[len(em.escalationHistory)-100:]
	}

	// Remove from active escalations
	delete(em.activeEscalations, chain.ID)
}

// calculateEscalationEffectiveness calculates how effective an escalation was
func (em *EscalationManager) calculateEscalationEffectiveness(chain *EscalationChain, responseTime time.Duration) float64 {
	effectiveness := 1.0

	// Reduce effectiveness for longer response times
	if responseTime > 30*time.Minute {
		effectiveness *= 0.3
	} else if responseTime > 15*time.Minute {
		effectiveness *= 0.6
	} else if responseTime > 5*time.Minute {
		effectiveness *= 0.8
	}

	// Reduce effectiveness if escalation was cancelled
	if chain.Status == EscalationCancelled {
		effectiveness *= 0.2
	}

	// Reduce effectiveness for higher escalation levels reached
	if chain.CurrentLevel > 2 {
		effectiveness *= 0.7
	}

	return effectiveness
}

// escalationLoop monitors active escalations and triggers next levels
func (em *EscalationManager) escalationLoop() {
	ticker := time.NewTicker(30 * time.Second)
	defer ticker.Stop()

	for {
		select {
		case <-ticker.C:
			em.processEscalations()
		}
	}
}

// processEscalations processes active escalations and triggers next levels
func (em *EscalationManager) processEscalations() {
	em.mu.Lock()
	defer em.mu.Unlock()

	now := time.Now()

	for _, chain := range em.activeEscalations {
		if chain.Status != EscalationActive {
			continue
		}

		if chain.Acknowledged {
			continue
		}

		// Check if it's time for next escalation
		if now.After(chain.NextEscalation) && chain.CurrentLevel < chain.MaxLevel {
			chain.CurrentLevel++

			// Calculate next escalation time
			if chain.CurrentLevel < chain.MaxLevel {
				// Exponential backoff: 5min, 10min, 20min, etc.
				nextDelay := time.Duration(5*chain.CurrentLevel) * time.Minute
				chain.NextEscalation = now.Add(nextDelay)
			}

			em.logger.Info("Escalating to next level",
				"escalation_id", chain.ID,
				"new_level", chain.CurrentLevel,
				"max_level", chain.MaxLevel)

			// Send notification for next level
			go em.sendEscalationNotification(chain, chain.CurrentLevel)
		}

		// Check for escalation timeout
		if chain.CurrentLevel >= chain.MaxLevel &&
			now.Sub(chain.StartTime) > em.config.EscalationCooldown {
			em.logger.Warn("Escalation reached maximum level and timed out",
				"escalation_id", chain.ID,
				"duration", time.Since(chain.StartTime))

			chain.Status = EscalationCompleted
			em.recordEscalationCompletion(chain)
		}
	}
}

// GetActiveEscalations returns all active escalations
func (em *EscalationManager) GetActiveEscalations() []EscalationChain {
	em.mu.RLock()
	defer em.mu.RUnlock()

	escalations := make([]EscalationChain, 0, len(em.activeEscalations))
	for _, chain := range em.activeEscalations {
		escalations = append(escalations, *chain)
	}

	return escalations
}

// GetEscalationHistory returns escalation history
func (em *EscalationManager) GetEscalationHistory(limit int) []EscalationRecord {
	em.mu.RLock()
	defer em.mu.RUnlock()

	if limit <= 0 || limit > len(em.escalationHistory) {
		limit = len(em.escalationHistory)
	}

	start := len(em.escalationHistory) - limit
	if start < 0 {
		start = 0
	}

	history := make([]EscalationRecord, limit)
	copy(history, em.escalationHistory[start:])

	return history
}

// GetEscalationStats returns escalation statistics
func (em *EscalationManager) GetEscalationStats() map[string]interface{} {
	em.mu.RLock()
	defer em.mu.RUnlock()

	totalEscalations := len(em.escalationHistory)
	if totalEscalations == 0 {
		return map[string]interface{}{
			"total_escalations":     0,
			"active_escalations":    len(em.activeEscalations),
			"average_response_time": "0s",
			"average_effectiveness": 0.0,
		}
	}

	// Calculate averages
	totalResponseTime := time.Duration(0)
	totalEffectiveness := 0.0

	for _, record := range em.escalationHistory {
		totalResponseTime += record.ResponseTime
		totalEffectiveness += record.Effectiveness
	}

	avgResponseTime := totalResponseTime / time.Duration(totalEscalations)
	avgEffectiveness := totalEffectiveness / float64(totalEscalations)

	return map[string]interface{}{
		"total_escalations":     totalEscalations,
		"active_escalations":    len(em.activeEscalations),
		"average_response_time": avgResponseTime.String(),
		"average_effectiveness": avgEffectiveness,
	}
}
