package notifications

import (
	"context"
	"fmt"
	"sync"
	"time"

	"github.com/markus-lassfolk/autonomy/pkg/logx"
)

// AcknowledgmentStatus represents the status of an acknowledgment
type AcknowledgmentStatus string

const (
	AcknowledgmentStatusPending      AcknowledgmentStatus = "pending"
	AcknowledgmentStatusAcknowledged AcknowledgmentStatus = "acknowledged"
	AcknowledgmentStatusExpired      AcknowledgmentStatus = "expired"
	AcknowledgmentStatusResolved     AcknowledgmentStatus = "resolved"
)

// Acknowledgment represents an acknowledgment record
type Acknowledgment struct {
	ID              string                 `json:"id"`
	NotificationID  string                 `json:"notification_id"`
	Type            string                 `json:"type"`
	Message         string                 `json:"message"`
	Priority        int                    `json:"priority"`
	Status          AcknowledgmentStatus   `json:"status"`
	CreatedAt       time.Time              `json:"created_at"`
	AcknowledgedAt  *time.Time             `json:"acknowledged_at,omitempty"`
	AcknowledgedBy  string                 `json:"acknowledged_by,omitempty"`
	ExpiresAt       time.Time              `json:"expires_at"`
	ResolvedAt      *time.Time             `json:"resolved_at,omitempty"`
	Context         map[string]interface{} `json:"context,omitempty"`
	Channels        []NotificationChannel  `json:"channels"`
	AutoResolve     bool                   `json:"auto_resolve"`
	AutoResolveTime time.Duration          `json:"auto_resolve_time"`
}

// AcknowledgmentTracker manages acknowledgment tracking
type AcknowledgmentTracker struct {
	config *AcknowledgmentConfig
	logger *logx.Logger
	mu     sync.RWMutex

	// In-memory storage (in production, this would be persistent)
	acknowledgments map[string]*Acknowledgment

	// Indexes for efficient lookups
	byType     map[string][]string               // notification type -> acknowledgment IDs
	byStatus   map[AcknowledgmentStatus][]string // status -> acknowledgment IDs
	byPriority map[int][]string                  // priority -> acknowledgment IDs
}

// AcknowledgmentConfig holds acknowledgment tracking configuration
type AcknowledgmentConfig struct {
	Enabled               bool          `json:"enabled"`
	DefaultExpiry         time.Duration `json:"default_expiry"`           // Default time before acknowledgment expires
	AutoResolveEnabled    bool          `json:"auto_resolve_enabled"`     // Auto-resolve acknowledgments after resolution
	AutoResolveTime       time.Duration `json:"auto_resolve_time"`        // Time to wait before auto-resolving
	MaxPendingPerType     int           `json:"max_pending_per_type"`     // Max pending acknowledgments per type
	MaxPendingPerPriority int           `json:"max_pending_per_priority"` // Max pending acknowledgments per priority
	CleanupInterval       time.Duration `json:"cleanup_interval"`         // How often to cleanup expired acknowledgments
	RequireAcknowledgment []string      `json:"require_acknowledgment"`   // Notification types that require acknowledgment
}

// NewAcknowledgmentTracker creates a new acknowledgment tracker
func NewAcknowledgmentTracker(config *AcknowledgmentConfig, logger *logx.Logger) *AcknowledgmentTracker {
	if config == nil {
		config = &AcknowledgmentConfig{
			Enabled:               true,
			DefaultExpiry:         24 * time.Hour,
			AutoResolveEnabled:    true,
			AutoResolveTime:       1 * time.Hour,
			MaxPendingPerType:     5,
			MaxPendingPerPriority: 10,
			CleanupInterval:       1 * time.Hour,
			RequireAcknowledgment: []string{"critical", "emergency", "system_failure"},
		}
	}

	at := &AcknowledgmentTracker{
		config:          config,
		logger:          logger,
		acknowledgments: make(map[string]*Acknowledgment),
		byType:          make(map[string][]string),
		byStatus:        make(map[AcknowledgmentStatus][]string),
		byPriority:      make(map[int][]string),
	}

	// Start cleanup goroutine
	if config.Enabled {
		go at.cleanupRoutine()
	}

	return at
}

// CreateAcknowledgment creates a new acknowledgment for a notification
func (at *AcknowledgmentTracker) CreateAcknowledgment(ctx context.Context, notification *Notification, channels []NotificationChannel) (*Acknowledgment, error) {
	if !at.config.Enabled {
		return nil, nil // Acknowledgment tracking disabled
	}

	at.mu.Lock()
	defer at.mu.Unlock()

	// Check if acknowledgment is required for this notification type
	if !at.isAcknowledgmentRequired(string(notification.Type)) {
		return nil, nil // No acknowledgment required
	}

	// Check limits
	if err := at.checkLimits(string(notification.Type), notification.Priority); err != nil {
		return nil, fmt.Errorf("acknowledgment limit exceeded: %w", err)
	}

	// Create acknowledgment
	ack := &Acknowledgment{
		ID:              at.generateID(notification),
		NotificationID:  fmt.Sprintf("notif_%s_%d", notification.Type, notification.Timestamp.UnixNano()),
		Type:            string(notification.Type),
		Message:         notification.Message,
		Priority:        notification.Priority,
		Status:          AcknowledgmentStatusPending,
		CreatedAt:       time.Now(),
		ExpiresAt:       time.Now().Add(at.config.DefaultExpiry),
		Context:         notification.Context,
		Channels:        channels,
		AutoResolve:     at.config.AutoResolveEnabled,
		AutoResolveTime: at.config.AutoResolveTime,
	}

	// Store acknowledgment
	at.acknowledgments[ack.ID] = ack
	at.addToIndexes(ack)

	at.logger.Info("Acknowledgment created",
		"id", ack.ID,
		"type", ack.Type,
		"priority", ack.Priority,
		"expires_at", ack.ExpiresAt)

	return ack, nil
}

// Acknowledge marks an acknowledgment as acknowledged
func (at *AcknowledgmentTracker) Acknowledge(ctx context.Context, acknowledgmentID string, acknowledgedBy string) error {
	if !at.config.Enabled {
		return fmt.Errorf("acknowledgment tracking is disabled")
	}

	at.mu.Lock()
	defer at.mu.Unlock()

	ack, exists := at.acknowledgments[acknowledgmentID]
	if !exists {
		return fmt.Errorf("acknowledgment not found: %s", acknowledgmentID)
	}

	if ack.Status != AcknowledgmentStatusPending {
		return fmt.Errorf("acknowledgment is not pending: %s", ack.Status)
	}

	now := time.Now()
	ack.Status = AcknowledgmentStatusAcknowledged
	ack.AcknowledgedAt = &now
	ack.AcknowledgedBy = acknowledgedBy

	// Update indexes
	at.removeFromIndexes(ack)
	at.addToIndexes(ack)

	at.logger.Info("Acknowledgment acknowledged",
		"id", acknowledgmentID,
		"acknowledged_by", acknowledgedBy)

	return nil
}

// Resolve marks an acknowledgment as resolved
func (at *AcknowledgmentTracker) Resolve(ctx context.Context, acknowledgmentID string) error {
	if !at.config.Enabled {
		return fmt.Errorf("acknowledgment tracking is disabled")
	}

	at.mu.Lock()
	defer at.mu.Unlock()

	ack, exists := at.acknowledgments[acknowledgmentID]
	if !exists {
		return fmt.Errorf("acknowledgment not found: %s", acknowledgmentID)
	}

	if ack.Status == AcknowledgmentStatusResolved {
		return fmt.Errorf("acknowledgment already resolved")
	}

	now := time.Now()
	ack.Status = AcknowledgmentStatusResolved
	ack.ResolvedAt = &now

	// Update indexes
	at.removeFromIndexes(ack)
	at.addToIndexes(ack)

	at.logger.Info("Acknowledgment resolved", "id", acknowledgmentID)

	return nil
}

// GetAcknowledgment retrieves an acknowledgment by ID
func (at *AcknowledgmentTracker) GetAcknowledgment(ctx context.Context, acknowledgmentID string) (*Acknowledgment, error) {
	if !at.config.Enabled {
		return nil, fmt.Errorf("acknowledgment tracking is disabled")
	}

	at.mu.RLock()
	defer at.mu.RUnlock()

	ack, exists := at.acknowledgments[acknowledgmentID]
	if !exists {
		return nil, fmt.Errorf("acknowledgment not found: %s", acknowledgmentID)
	}

	return ack, nil
}

// GetPendingAcknowledgment checks if there's a pending acknowledgment for a notification type
func (at *AcknowledgmentTracker) GetPendingAcknowledgment(ctx context.Context, notificationType string) (*Acknowledgment, error) {
	if !at.config.Enabled {
		return nil, nil
	}

	at.mu.RLock()
	defer at.mu.RUnlock()

	// Check for pending acknowledgments of this type
	for _, id := range at.byType[notificationType] {
		ack := at.acknowledgments[id]
		if ack.Status == AcknowledgmentStatusPending && ack.ExpiresAt.After(time.Now()) {
			return ack, nil
		}
	}

	return nil, nil
}

// ShouldSendNotification checks if a notification should be sent based on acknowledgment status
func (at *AcknowledgmentTracker) ShouldSendNotification(ctx context.Context, notification *Notification) (bool, error) {
	if !at.config.Enabled {
		return true, nil // Always send if tracking is disabled
	}

	// Check if acknowledgment is required
	if !at.isAcknowledgmentRequired(string(notification.Type)) {
		return true, nil // No acknowledgment required
	}

	// Check for pending acknowledgment
	pending, err := at.GetPendingAcknowledgment(ctx, string(notification.Type))
	if err != nil {
		return true, err // Send on error
	}

	if pending != nil {
		at.logger.Debug("Notification suppressed due to pending acknowledgment",
			"notification_type", notification.Type,
			"acknowledgment_id", pending.ID)
		return false, nil // Don't send if there's a pending acknowledgment
	}

	return true, nil
}

// GetAcknowledgmentStats returns statistics about acknowledgments
func (at *AcknowledgmentTracker) GetAcknowledgmentStats(ctx context.Context) map[string]interface{} {
	if !at.config.Enabled {
		return map[string]interface{}{
			"enabled": false,
		}
	}

	at.mu.RLock()
	defer at.mu.RUnlock()

	stats := map[string]interface{}{
		"enabled": true,
		"total":   len(at.acknowledgments),
		"by_status": map[string]int{
			"pending":      len(at.byStatus[AcknowledgmentStatusPending]),
			"acknowledged": len(at.byStatus[AcknowledgmentStatusAcknowledged]),
			"expired":      len(at.byStatus[AcknowledgmentStatusExpired]),
			"resolved":     len(at.byStatus[AcknowledgmentStatusResolved]),
		},
		"by_priority": make(map[string]int),
		"by_type":     make(map[string]int),
	}

	// Count by priority
	for priority, ids := range at.byPriority {
		stats["by_priority"].(map[string]int)[fmt.Sprintf("priority_%d", priority)] = len(ids)
	}

	// Count by type
	for notificationType, ids := range at.byType {
		stats["by_type"].(map[string]int)[notificationType] = len(ids)
	}

	return stats
}

// ListAcknowledgment returns a list of acknowledgments with optional filtering
func (at *AcknowledgmentTracker) ListAcknowledgment(ctx context.Context, filters map[string]interface{}) ([]*Acknowledgment, error) {
	if !at.config.Enabled {
		return nil, fmt.Errorf("acknowledgment tracking is disabled")
	}

	at.mu.RLock()
	defer at.mu.RUnlock()

	var results []*Acknowledgment

	for _, ack := range at.acknowledgments {
		if at.matchesFilters(ack, filters) {
			results = append(results, ack)
		}
	}

	return results, nil
}

// cleanupRoutine periodically cleans up expired acknowledgments
func (at *AcknowledgmentTracker) cleanupRoutine() {
	ticker := time.NewTicker(at.config.CleanupInterval)
	defer ticker.Stop()

	for range ticker.C {
		at.cleanupExpired()
	}
}

// cleanupExpired removes expired acknowledgments
func (at *AcknowledgmentTracker) cleanupExpired() {
	at.mu.Lock()
	defer at.mu.Unlock()

	now := time.Now()
	var expiredIDs []string

	for id, ack := range at.acknowledgments {
		if ack.ExpiresAt.Before(now) && ack.Status == AcknowledgmentStatusPending {
			expiredIDs = append(expiredIDs, id)
		}
	}

	for _, id := range expiredIDs {
		ack := at.acknowledgments[id]
		ack.Status = AcknowledgmentStatusExpired
		at.removeFromIndexes(ack)
		at.addToIndexes(ack)

		at.logger.Info("Acknowledgment expired", "id", id, "type", ack.Type)
	}
}

// Helper methods

func (at *AcknowledgmentTracker) isAcknowledgmentRequired(notificationType string) bool {
	for _, requiredType := range at.config.RequireAcknowledgment {
		if requiredType == notificationType {
			return true
		}
	}
	return false
}

func (at *AcknowledgmentTracker) checkLimits(notificationType string, priority int) error {
	// Check type limit
	if len(at.byType[notificationType]) >= at.config.MaxPendingPerType {
		return fmt.Errorf("max pending acknowledgments per type exceeded: %d", at.config.MaxPendingPerType)
	}

	// Check priority limit
	if len(at.byPriority[priority]) >= at.config.MaxPendingPerPriority {
		return fmt.Errorf("max pending acknowledgments per priority exceeded: %d", at.config.MaxPendingPerPriority)
	}

	return nil
}

func (at *AcknowledgmentTracker) generateID(notification *Notification) string {
	return fmt.Sprintf("ack_%s_%d", notification.Type, time.Now().UnixNano())
}

func (at *AcknowledgmentTracker) addToIndexes(ack *Acknowledgment) {
	// Add to type index
	at.byType[ack.Type] = append(at.byType[ack.Type], ack.ID)

	// Add to status index
	at.byStatus[ack.Status] = append(at.byStatus[ack.Status], ack.ID)

	// Add to priority index
	at.byPriority[ack.Priority] = append(at.byPriority[ack.Priority], ack.ID)
}

func (at *AcknowledgmentTracker) removeFromIndexes(ack *Acknowledgment) {
	// Remove from type index
	if slice, exists := at.byType[ack.Type]; exists {
		at.byType[ack.Type] = at.removeFromSlice(slice, ack.ID)
	}

	// Remove from status index
	if slice, exists := at.byStatus[ack.Status]; exists {
		at.byStatus[ack.Status] = at.removeFromSlice(slice, ack.ID)
	}

	// Remove from priority index
	if slice, exists := at.byPriority[ack.Priority]; exists {
		at.byPriority[ack.Priority] = at.removeFromSlice(slice, ack.ID)
	}
}

func (at *AcknowledgmentTracker) removeFromSlice(slice []string, item string) []string {
	for i, id := range slice {
		if id == item {
			return append(slice[:i], slice[i+1:]...)
		}
	}
	return slice
}

func (at *AcknowledgmentTracker) matchesFilters(ack *Acknowledgment, filters map[string]interface{}) bool {
	for key, value := range filters {
		switch key {
		case "status":
			if status, ok := value.(AcknowledgmentStatus); ok && ack.Status != status {
				return false
			}
		case "type":
			if notificationType, ok := value.(string); ok && ack.Type != notificationType {
				return false
			}
		case "priority":
			if priority, ok := value.(int); ok && ack.Priority != priority {
				return false
			}
		case "acknowledged_by":
			if acknowledgedBy, ok := value.(string); ok && ack.AcknowledgedBy != acknowledgedBy {
				return false
			}
		}
	}
	return true
}
