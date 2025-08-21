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

// SlackClient handles Slack notifications
type SlackClient struct {
	config *SlackConfig
	logger *logx.Logger
	client *http.Client
}

// SlackMessage represents a Slack webhook message
type SlackMessage struct {
	Text        string            `json:"text,omitempty"`
	Username    string            `json:"username,omitempty"`
	IconEmoji   string            `json:"icon_emoji,omitempty"`
	IconURL     string            `json:"icon_url,omitempty"`
	Channel     string            `json:"channel,omitempty"`
	Attachments []SlackAttachment `json:"attachments,omitempty"`
}

// SlackAttachment represents a Slack message attachment
type SlackAttachment struct {
	Color      string       `json:"color,omitempty"`
	Title      string       `json:"title,omitempty"`
	Text       string       `json:"text,omitempty"`
	Fields     []SlackField `json:"fields,omitempty"`
	Footer     string       `json:"footer,omitempty"`
	FooterIcon string       `json:"footer_icon,omitempty"`
	Timestamp  int64        `json:"ts,omitempty"`
}

// SlackField represents a field in a Slack attachment
type SlackField struct {
	Title string `json:"title"`
	Value string `json:"value"`
	Short bool   `json:"short"`
}

// NewSlackClient creates a new Slack client
func NewSlackClient(config *SlackConfig, logger *logx.Logger, client *http.Client) *SlackClient {
	return &SlackClient{
		config: config,
		logger: logger,
		client: client,
	}
}

// Send sends a notification to Slack
func (sc *SlackClient) Send(ctx context.Context, notification *Notification) error {
	if !sc.config.Enabled {
		return fmt.Errorf("Slack notifications are disabled")
	}

	if sc.config.WebhookURL == "" {
		return fmt.Errorf("Slack webhook URL is required")
	}

	// Create Slack message
	message := sc.createSlackMessage(notification)

	// Send message
	return sc.sendMessage(ctx, message)
}

// createSlackMessage creates a Slack message from notification
func (sc *SlackClient) createSlackMessage(notification *Notification) *SlackMessage {
	message := &SlackMessage{
		Username:  sc.config.Username,
		IconEmoji: sc.config.IconEmoji,
		IconURL:   sc.config.IconURL,
		Channel:   sc.config.Channel,
	}

	// Set default values if not configured
	if message.Username == "" {
		message.Username = "autonomy"
	}
	if message.IconEmoji == "" && message.IconURL == "" {
		message.IconEmoji = ":satellite:"
	}

	// Create attachment with rich formatting
	attachment := SlackAttachment{
		Color:     sc.getAttachmentColor(notification),
		Title:     notification.Title,
		Text:      notification.Message,
		Footer:    "autonomy Daemon",
		Timestamp: notification.Timestamp.Unix(),
	}

	// Add priority field
	priorityText := sc.getPriorityText(notification.Priority)
	attachment.Fields = append(attachment.Fields, SlackField{
		Title: "Priority",
		Value: priorityText,
		Short: true,
	})

	// Add notification type field
	attachment.Fields = append(attachment.Fields, SlackField{
		Title: "Type",
		Value: string(notification.Type),
		Short: true,
	})

	// Add context fields
	if notification.Context != nil {
		sc.addContextFields(&attachment, notification.Context)
	}

	message.Attachments = []SlackAttachment{attachment}

	return message
}

// addContextFields adds context information as Slack fields
func (sc *SlackClient) addContextFields(attachment *SlackAttachment, context map[string]interface{}) {
	for key, value := range context {
		if key == "test" {
			continue // Skip test flag
		}

		// Format key as title
		title := strings.Title(strings.ReplaceAll(key, "_", " "))

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

		// Determine if field should be short (side-by-side)
		short := len(valueStr) < 20

		attachment.Fields = append(attachment.Fields, SlackField{
			Title: title,
			Value: valueStr,
			Short: short,
		})
	}
}

// sendMessage sends the message to Slack webhook
func (sc *SlackClient) sendMessage(ctx context.Context, message *SlackMessage) error {
	// Marshal message to JSON
	jsonData, err := json.Marshal(message)
	if err != nil {
		return fmt.Errorf("failed to marshal message: %w", err)
	}

	// Create request
	req, err := http.NewRequestWithContext(ctx, "POST", sc.config.WebhookURL, bytes.NewBuffer(jsonData))
	if err != nil {
		return fmt.Errorf("failed to create request: %w", err)
	}

	req.Header.Set("Content-Type", "application/json")

	// Send request
	resp, err := sc.client.Do(req)
	if err != nil {
		return fmt.Errorf("failed to send request: %w", err)
	}
	defer resp.Body.Close()

	// Check response status
	if resp.StatusCode != http.StatusOK {
		return fmt.Errorf("Slack webhook returned status %d", resp.StatusCode)
	}

	sc.logger.Debug("Slack notification sent successfully")
	return nil
}

// getAttachmentColor returns color for attachment based on notification
func (sc *SlackClient) getAttachmentColor(notification *Notification) string {
	// Use custom color if specified
	if notification.SlackColor != "" {
		return notification.SlackColor
	}

	// Use priority-based colors
	switch notification.Priority {
	case PriorityEmergency:
		return "danger" // Red
	case PriorityHigh:
		return "warning" // Orange
	case PriorityNormal:
		return "good" // Green
	case PriorityLow:
		return "#36a64f" // Light green
	case PriorityLowest:
		return "#808080" // Gray
	default:
		return "good" // Green
	}
}

// getPriorityText returns formatted priority text for Slack
func (sc *SlackClient) getPriorityText(priority int) string {
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
