package notifications

import (
	"bytes"
	"context"
	"encoding/json"
	"fmt"
	"net/http"
	"strings"
	"time"

	"github.com/markus-lassfolk/autonomy/pkg/logx"
)

// DiscordClient handles Discord notifications
type DiscordClient struct {
	config *DiscordConfig
	logger *logx.Logger
	client *http.Client
}

// DiscordMessage represents a Discord webhook message
type DiscordMessage struct {
	Content   string         `json:"content,omitempty"`
	Username  string         `json:"username,omitempty"`
	AvatarURL string         `json:"avatar_url,omitempty"`
	Embeds    []DiscordEmbed `json:"embeds,omitempty"`
}

// DiscordEmbed represents a Discord embed
type DiscordEmbed struct {
	Title       string              `json:"title,omitempty"`
	Description string              `json:"description,omitempty"`
	Color       int                 `json:"color,omitempty"`
	Fields      []DiscordEmbedField `json:"fields,omitempty"`
	Footer      *DiscordEmbedFooter `json:"footer,omitempty"`
	Timestamp   string              `json:"timestamp,omitempty"`
}

// DiscordEmbedField represents a field in a Discord embed
type DiscordEmbedField struct {
	Name   string `json:"name"`
	Value  string `json:"value"`
	Inline bool   `json:"inline"`
}

// DiscordEmbedFooter represents footer in a Discord embed
type DiscordEmbedFooter struct {
	Text    string `json:"text"`
	IconURL string `json:"icon_url,omitempty"`
}

// NewDiscordClient creates a new Discord client
func NewDiscordClient(config *DiscordConfig, logger *logx.Logger, client *http.Client) *DiscordClient {
	return &DiscordClient{
		config: config,
		logger: logger,
		client: client,
	}
}

// Send sends a notification to Discord
func (dc *DiscordClient) Send(ctx context.Context, notification *Notification) error {
	if !dc.config.Enabled {
		return fmt.Errorf("Discord notifications are disabled")
	}

	if dc.config.WebhookURL == "" {
		return fmt.Errorf("Discord webhook URL is required")
	}

	// Create Discord message
	message := dc.createDiscordMessage(notification)

	// Send message
	return dc.sendMessage(ctx, message)
}

// createDiscordMessage creates a Discord message from notification
func (dc *DiscordClient) createDiscordMessage(notification *Notification) *DiscordMessage {
	message := &DiscordMessage{
		Username:  dc.config.Username,
		AvatarURL: dc.config.AvatarURL,
	}

	// Set default values if not configured
	if message.Username == "" {
		message.Username = "autonomy"
	}
	if message.AvatarURL == "" {
		message.AvatarURL = "https://cdn.discordapp.com/attachments/placeholder/satellite.png"
	}

	// Create embed with rich formatting
	embed := DiscordEmbed{
		Title:       notification.Title,
		Description: notification.Message,
		Color:       dc.getEmbedColor(notification),
		Timestamp:   notification.Timestamp.Format(time.RFC3339),
		Footer: &DiscordEmbedFooter{
			Text: "autonomy Daemon",
		},
	}

	// Add priority field
	priorityText := dc.getPriorityText(notification.Priority)
	embed.Fields = append(embed.Fields, DiscordEmbedField{
		Name:   "Priority",
		Value:  priorityText,
		Inline: true,
	})

	// Add notification type field
	embed.Fields = append(embed.Fields, DiscordEmbedField{
		Name:   "Type",
		Value:  string(notification.Type),
		Inline: true,
	})

	// Add context fields
	if notification.Context != nil {
		dc.addContextFields(&embed, notification.Context)
	}

	message.Embeds = []DiscordEmbed{embed}

	return message
}

// addContextFields adds context information as Discord embed fields
func (dc *DiscordClient) addContextFields(embed *DiscordEmbed, context map[string]interface{}) {
	for key, value := range context {
		if key == "test" {
			continue // Skip test flag
		}

		// Format key as title
		name := strings.Title(strings.ReplaceAll(key, "_", " "))

		// Format value based on type
		var valueStr string
		switch v := value.(type) {
		case string:
			valueStr = v
		case float64:
			if key == "latency_ms" {
				valueStr = fmt.Sprintf("%.1f ms", v)
			} else if key == "loss_percent" {
				valueStr = fmt.Sprintf("%.1f%%", v)
			} else {
				valueStr = fmt.Sprintf("%.2f", v)
			}
		case int:
			valueStr = fmt.Sprintf("%d", v)
		case bool:
			if v {
				valueStr = "✅ Yes"
			} else {
				valueStr = "❌ No"
			}
		case time.Time:
			valueStr = v.Format("2006-01-02 15:04:05 UTC")
		default:
			valueStr = fmt.Sprintf("%v", v)
		}

		// Determine if field should be inline (side-by-side)
		inline := len(valueStr) < 20

		embed.Fields = append(embed.Fields, DiscordEmbedField{
			Name:   name,
			Value:  valueStr,
			Inline: inline,
		})
	}
}

// sendMessage sends the message to Discord webhook
func (dc *DiscordClient) sendMessage(ctx context.Context, message *DiscordMessage) error {
	// Marshal message to JSON
	jsonData, err := json.Marshal(message)
	if err != nil {
		return fmt.Errorf("failed to marshal message: %w", err)
	}

	// Create request
	req, err := http.NewRequestWithContext(ctx, "POST", dc.config.WebhookURL, bytes.NewBuffer(jsonData))
	if err != nil {
		return fmt.Errorf("failed to create request: %w", err)
	}

	req.Header.Set("Content-Type", "application/json")

	// Send request
	resp, err := dc.client.Do(req)
	if err != nil {
		return fmt.Errorf("failed to send request: %w", err)
	}
	defer resp.Body.Close()

	// Check response status
	if resp.StatusCode < 200 || resp.StatusCode >= 300 {
		return fmt.Errorf("Discord webhook returned status %d", resp.StatusCode)
	}

	dc.logger.Debug("Discord notification sent successfully")
	return nil
}

// getEmbedColor returns color for embed based on notification
func (dc *DiscordClient) getEmbedColor(notification *Notification) int {
	// Use custom color if specified
	if notification.DiscordColor != 0 {
		return notification.DiscordColor
	}

	// Use priority-based colors (Discord uses decimal color values)
	switch notification.Priority {
	case PriorityEmergency:
		return 0xFF0000 // Red
	case PriorityHigh:
		return 0xFF8C00 // Orange
	case PriorityNormal:
		return 0x007BFF // Blue
	case PriorityLow:
		return 0x28A745 // Green
	case PriorityLowest:
		return 0x6C757D // Gray
	default:
		return 0x007BFF // Blue
	}
}

// getPriorityText returns formatted priority text for Discord
func (dc *DiscordClient) getPriorityText(priority int) string {
	switch priority {
	case PriorityEmergency:
		return "🚨 Emergency"
	case PriorityHigh:
		return "⚠️ High"
	case PriorityNormal:
		return "ℹ️ Normal"
	case PriorityLow:
		return "📝 Low"
	case PriorityLowest:
		return "💬 Lowest"
	default:
		return "ℹ️ Normal"
	}
}
