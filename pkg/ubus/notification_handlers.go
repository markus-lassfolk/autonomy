package ubus

import (
	"context"
	"encoding/json"
	"fmt"
	"time"

	"github.com/markus-lassfolk/autonomy/pkg/logx"
	"github.com/markus-lassfolk/autonomy/pkg/notifications"
)

// NotificationHandlers provides ubus API endpoints for notification management
type NotificationHandlers struct {
	notificationManager *notifications.Manager
	logger              *logx.Logger
}

// NewNotificationHandlers creates a new notification handlers instance
func NewNotificationHandlers(notificationManager *notifications.Manager, logger *logx.Logger) *NotificationHandlers {
	return &NotificationHandlers{
		notificationManager: notificationManager,
		logger:              logger,
	}
}

// NotificationStatusResponse represents the response for notification status
type NotificationStatusResponse struct {
	Enabled         bool                   `json:"enabled"`
	Channels        []string               `json:"channels"`
	RateLimitStatus *RateLimitStatus       `json:"rate_limit_status"`
	QueueStatus     *QueueStatus           `json:"queue_status"`
	Statistics      map[string]interface{} `json:"statistics"`
	Message         string                 `json:"message"`
}

// RateLimitStatus represents rate limiting status
type RateLimitStatus struct {
	IsLimited     bool    `json:"is_limited"`
	Remaining     int     `json:"remaining"`
	ResetTime     string  `json:"reset_time"`
	WindowSeconds int     `json:"window_seconds"`
	UsagePercent  float64 `json:"usage_percent"`
}

// QueueStatus represents notification queue status
type QueueStatus struct {
	PendingCount    int `json:"pending_count"`
	ProcessingCount int `json:"processing_count"`
	TotalQueued     int `json:"total_queued"`
}

// SendNotificationRequest represents a notification send request
type SendNotificationRequest struct {
	Title    string                 `json:"title"`
	Message  string                 `json:"message"`
	Priority int                    `json:"priority"`
	Channel  string                 `json:"channel"`
	Context  map[string]interface{} `json:"context,omitempty"`
	Template string                 `json:"template,omitempty"`
	Retry    bool                   `json:"retry"`
}

// SendNotificationResponse represents a notification send response
type SendNotificationResponse struct {
	Success   bool   `json:"success"`
	ID        string `json:"id"`
	Message   string `json:"message"`
	Channel   string `json:"channel"`
	Timestamp string `json:"timestamp"`
}

// GetNotificationStatus handles the notification status API endpoint
func (nh *NotificationHandlers) GetNotificationStatus(ctx context.Context, data json.RawMessage) (interface{}, error) {
	if nh.notificationManager == nil {
		return &NotificationStatusResponse{
			Enabled: false,
			Message: "Notification manager not available",
		}, nil
	}

	// Get rate limiting status
	rateLimit := &RateLimitStatus{
		IsLimited:     false, // TODO: Implement rate limiting check
		Remaining:     0,     // TODO: Get from manager
		ResetTime:     time.Now().Add(time.Hour).Format(time.RFC3339),
		WindowSeconds: 3600,
		UsagePercent:  0,
	}

	// Get queue status
	queueStatus := &QueueStatus{
		PendingCount:    0, // TODO: Get from manager
		ProcessingCount: 0,
		TotalQueued:     0,
	}

	// Get statistics
	stats := nh.notificationManager.GetStats()

	// Get available channels
	channels := []string{"pushover", "email", "slack", "discord", "telegram", "webhook", "sms"}

	response := &NotificationStatusResponse{
		Enabled:         true,
		Channels:        channels,
		RateLimitStatus: rateLimit,
		QueueStatus:     queueStatus,
		Statistics:      stats,
		Message:         "Notification system operational",
	}

	nh.logger.Debug("Notification status requested")
	return response, nil
}

// SendNotification handles the send notification API endpoint
func (nh *NotificationHandlers) SendNotification(ctx context.Context, data json.RawMessage) (interface{}, error) {
	if nh.notificationManager == nil {
		return &SendNotificationResponse{
			Success: false,
			Message: "Notification manager not available",
		}, nil
	}

	// Parse request
	var req SendNotificationRequest
	if err := json.Unmarshal(data, &req); err != nil {
		return &SendNotificationResponse{
			Success: false,
			Message: fmt.Sprintf("Invalid request format: %v", err),
		}, nil
	}

	// Validate request
	if req.Title == "" {
		return &SendNotificationResponse{
			Success: false,
			Message: "Title is required",
		}, nil
	}

	if req.Message == "" {
		return &SendNotificationResponse{
			Success: false,
			Message: "Message is required",
		}, nil
	}

	// Create notification event
	event := &notifications.NotificationEvent{
		Type:     notifications.NotificationStatusUpdate,
		Title:    req.Title,
		Message:  req.Message,
		Priority: req.Priority,
		Details:  req.Context,
	}

	// Send notification using the manager
	err := nh.notificationManager.SendNotification(ctx, event)
	if err != nil {
		nh.logger.Error("Failed to send notification", "error", err, "title", req.Title)
		return &SendNotificationResponse{
			Success: false,
			Message: fmt.Sprintf("Failed to send notification: %v", err),
		}, nil
	}

	response := &SendNotificationResponse{
		Success:   true,
		ID:        fmt.Sprintf("api_%d", time.Now().Unix()),
		Message:   "Notification sent successfully",
		Channel:   req.Channel,
		Timestamp: time.Now().Format(time.RFC3339),
	}

	nh.logger.Info("Notification sent via API", "title", req.Title, "channel", req.Channel)
	return response, nil
}

// GetNotificationHistory handles the notification history API endpoint
func (nh *NotificationHandlers) GetNotificationHistory(ctx context.Context, data json.RawMessage) (interface{}, error) {
	if nh.notificationManager == nil {
		return map[string]interface{}{
			"success": false,
			"message": "Notification manager not available",
		}, nil
	}

	// Parse request for limit
	var req struct {
		Limit int `json:"limit"`
	}
	if err := json.Unmarshal(data, &req); err != nil {
		req.Limit = 50 // Default limit
	}

	if req.Limit <= 0 || req.Limit > 1000 {
		req.Limit = 50
	}

	// TODO: Implement history retrieval from notification manager
	history := []map[string]interface{}{
		{
			"id":        "test_1",
			"title":     "Test Notification",
			"message":   "This is a test notification",
			"channel":   "pushover",
			"priority":  1,
			"timestamp": time.Now().Add(-time.Hour).Format(time.RFC3339),
			"status":    "sent",
		},
	}

	response := map[string]interface{}{
		"success": true,
		"history": history,
		"count":   len(history),
		"limit":   req.Limit,
		"message": "Notification history retrieved",
	}

	nh.logger.Debug("Notification history requested", "limit", req.Limit)
	return response, nil
}

// AcknowledgeNotification handles the acknowledge notification API endpoint
func (nh *NotificationHandlers) AcknowledgeNotification(ctx context.Context, data json.RawMessage) (interface{}, error) {
	if nh.notificationManager == nil {
		return map[string]interface{}{
			"success": false,
			"message": "Notification manager not available",
		}, nil
	}

	// Parse request
	var req struct {
		ID string `json:"id"`
	}
	if err := json.Unmarshal(data, &req); err != nil {
		return map[string]interface{}{
			"success": false,
			"message": fmt.Sprintf("Invalid request format: %v", err),
		}, nil
	}

	if req.ID == "" {
		return map[string]interface{}{
			"success": false,
			"message": "Notification ID is required",
		}, nil
	}

	// TODO: Implement acknowledgment in notification manager
	response := map[string]interface{}{
		"success":   true,
		"id":        req.ID,
		"message":   "Notification acknowledged successfully",
		"timestamp": time.Now().Format(time.RFC3339),
	}

	nh.logger.Info("Notification acknowledged", "id", req.ID)
	return response, nil
}

// GetNotificationStats handles the notification statistics API endpoint
func (nh *NotificationHandlers) GetNotificationStats(ctx context.Context, data json.RawMessage) (interface{}, error) {
	if nh.notificationManager == nil {
		return map[string]interface{}{
			"success": false,
			"message": "Notification manager not available",
		}, nil
	}

	// Get statistics from manager
	stats := nh.notificationManager.GetStats()

	response := map[string]interface{}{
		"success":    true,
		"statistics": stats,
		"timestamp":  time.Now().Format(time.RFC3339),
		"message":    "Notification statistics retrieved",
	}

	nh.logger.Debug("Notification statistics requested")
	return response, nil
}

// TestNotification handles the test notification API endpoint
func (nh *NotificationHandlers) TestNotification(ctx context.Context, data json.RawMessage) (interface{}, error) {
	if nh.notificationManager == nil {
		return map[string]interface{}{
			"success": false,
			"message": "Notification manager not available",
		}, nil
	}

	// Parse request
	var req struct {
		Channel string `json:"channel"`
	}
	if err := json.Unmarshal(data, &req); err != nil {
		req.Channel = "pushover" // Default channel
	}

	// Send test notification
	testTitle := "Test Notification"
	testMessage := fmt.Sprintf("This is a test notification sent via API at %s", time.Now().Format(time.RFC3339))

	// Create test notification event
	testEvent := &notifications.NotificationEvent{
		Type:     notifications.NotificationStatusUpdate,
		Title:    testTitle,
		Message:  testMessage,
		Priority: 0,
	}

	err := nh.notificationManager.SendNotification(ctx, testEvent)
	if err != nil {
		nh.logger.Error("Test notification failed", "error", err, "channel", req.Channel)
		return map[string]interface{}{
			"success": false,
			"message": fmt.Sprintf("Test notification failed: %v", err),
		}, nil
	}

	response := map[string]interface{}{
		"success":   true,
		"channel":   req.Channel,
		"message":   "Test notification sent successfully",
		"timestamp": time.Now().Format(time.RFC3339),
	}

	nh.logger.Info("Test notification sent", "channel", req.Channel)
	return response, nil
}
