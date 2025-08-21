package notifications

import (
	"bytes"
	"context"
	"encoding/json"
	"fmt"
	"net/http"
	"net/url"
	"strconv"

	"github.com/markus-lassfolk/autonomy/pkg/logx"
)

// PushoverClient handles Pushover notifications
type PushoverClient struct {
	config *PushoverConfig
	logger *logx.Logger
	client *http.Client
}

// PushoverMessage represents a Pushover API message
type PushoverMessage struct {
	Token     string `json:"token"`
	User      string `json:"user"`
	Message   string `json:"message"`
	Title     string `json:"title,omitempty"`
	Priority  int    `json:"priority,omitempty"`
	Sound     string `json:"sound,omitempty"`
	Device    string `json:"device,omitempty"`
	Timestamp int64  `json:"timestamp,omitempty"`
	URL       string `json:"url,omitempty"`
	URLTitle  string `json:"url_title,omitempty"`
	HTML      int    `json:"html,omitempty"`

	// Emergency priority fields
	Retry  int `json:"retry,omitempty"`
	Expire int `json:"expire,omitempty"`
}

// PushoverResponse represents the Pushover API response
type PushoverResponse struct {
	Status  int      `json:"status"`
	Request string   `json:"request"`
	Errors  []string `json:"errors,omitempty"`
}

// NewPushoverClient creates a new Pushover client
func NewPushoverClient(config *PushoverConfig, logger *logx.Logger, client *http.Client) *PushoverClient {
	return &PushoverClient{
		config: config,
		logger: logger,
		client: client,
	}
}

// Send sends a notification via Pushover
func (pc *PushoverClient) Send(ctx context.Context, notification *Notification) error {
	if !pc.config.Enabled {
		return fmt.Errorf("Pushover is disabled")
	}

	if pc.config.Token == "" || pc.config.User == "" {
		return fmt.Errorf("Pushover token and user are required")
	}

	// Create Pushover message
	message := &PushoverMessage{
		Token:     pc.config.Token,
		User:      pc.config.User,
		Message:   notification.Message,
		Title:     notification.Title,
		Priority:  notification.Priority,
		Device:    pc.config.Device,
		Timestamp: notification.Timestamp.Unix(),
		HTML:      1, // Enable HTML formatting
	}

	// Set sound if specified
	if notification.PushoverSound != "" {
		message.Sound = notification.PushoverSound
	} else {
		message.Sound = pc.getPrioritySound(notification.Priority)
	}

	// Set emergency priority parameters
	if notification.Priority == PriorityEmergency {
		message.Retry = 60    // Retry every 60 seconds
		message.Expire = 3600 // Expire after 1 hour
	}

	// Add context URL if available
	if notification.Context != nil {
		if dashboardURL, ok := notification.Context["dashboard_url"].(string); ok {
			message.URL = dashboardURL
			message.URLTitle = "View Dashboard"
		}
	}

	// Send the message
	return pc.sendMessage(ctx, message)
}

// sendMessage sends a message to the Pushover API
func (pc *PushoverClient) sendMessage(ctx context.Context, message *PushoverMessage) error {
	// Prepare form data
	data := url.Values{}
	data.Set("token", message.Token)
	data.Set("user", message.User)
	data.Set("message", message.Message)

	if message.Title != "" {
		data.Set("title", message.Title)
	}
	if message.Priority != 0 {
		data.Set("priority", strconv.Itoa(message.Priority))
	}
	if message.Sound != "" {
		data.Set("sound", message.Sound)
	}
	if message.Device != "" {
		data.Set("device", message.Device)
	}
	if message.Timestamp > 0 {
		data.Set("timestamp", strconv.FormatInt(message.Timestamp, 10))
	}
	if message.URL != "" {
		data.Set("url", message.URL)
	}
	if message.URLTitle != "" {
		data.Set("url_title", message.URLTitle)
	}
	if message.HTML > 0 {
		data.Set("html", strconv.Itoa(message.HTML))
	}

	// Emergency priority parameters
	if message.Priority == PriorityEmergency {
		if message.Retry > 0 {
			data.Set("retry", strconv.Itoa(message.Retry))
		}
		if message.Expire > 0 {
			data.Set("expire", strconv.Itoa(message.Expire))
		}
	}

	// Create request
	req, err := http.NewRequestWithContext(ctx, "POST", "https://api.pushover.net/1/messages.json",
		bytes.NewBufferString(data.Encode()))
	if err != nil {
		return fmt.Errorf("failed to create request: %w", err)
	}

	req.Header.Set("Content-Type", "application/x-www-form-urlencoded")

	// Send request
	resp, err := pc.client.Do(req)
	if err != nil {
		return fmt.Errorf("failed to send request: %w", err)
	}
	defer resp.Body.Close()

	// Parse response
	var pushoverResp PushoverResponse
	if err := json.NewDecoder(resp.Body).Decode(&pushoverResp); err != nil {
		return fmt.Errorf("failed to parse response: %w", err)
	}

	// Check for errors
	if pushoverResp.Status != 1 {
		if len(pushoverResp.Errors) > 0 {
			return fmt.Errorf("Pushover API error: %v", pushoverResp.Errors)
		}
		return fmt.Errorf("Pushover API returned status %d", pushoverResp.Status)
	}

	pc.logger.Debug("Pushover notification sent successfully", "request_id", pushoverResp.Request)
	return nil
}

// getPrioritySound returns appropriate sound for priority level
func (pc *PushoverClient) getPrioritySound(priority int) string {
	switch priority {
	case PriorityEmergency:
		return "siren"
	case PriorityHigh:
		return "updown"
	case PriorityNormal:
		return "pushover"
	case PriorityLow:
		return "none"
	case PriorityLowest:
		return "none"
	default:
		return "pushover"
	}
}
