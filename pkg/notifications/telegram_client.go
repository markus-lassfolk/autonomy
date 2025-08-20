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

// TelegramClient handles Telegram notifications
type TelegramClient struct {
	config *TelegramConfig
	logger *logx.Logger
	client *http.Client
}

// TelegramMessage represents a Telegram bot message
type TelegramMessage struct {
	ChatID    string `json:"chat_id"`
	Text      string `json:"text"`
	ParseMode string `json:"parse_mode,omitempty"`
}

// TelegramResponse represents the Telegram API response
type TelegramResponse struct {
	OK          bool   `json:"ok"`
	Description string `json:"description,omitempty"`
	ErrorCode   int    `json:"error_code,omitempty"`
}

// NewTelegramClient creates a new Telegram client
func NewTelegramClient(config *TelegramConfig, logger *logx.Logger, client *http.Client) *TelegramClient {
	return &TelegramClient{
		config: config,
		logger: logger,
		client: client,
	}
}

// Send sends a notification to Telegram
func (tc *TelegramClient) Send(ctx context.Context, notification *Notification) error {
	if !tc.config.Enabled {
		return fmt.Errorf("Telegram notifications are disabled")
	}

	if tc.config.Token == "" || tc.config.ChatID == "" {
		return fmt.Errorf("Telegram token and chat ID are required")
	}

	// Create Telegram message
	message := tc.createTelegramMessage(notification)

	// Send message
	return tc.sendMessage(ctx, message)
}

// createTelegramMessage creates a Telegram message from notification
func (tc *TelegramClient) createTelegramMessage(notification *Notification) *TelegramMessage {
	// Format message with Markdown
	text := tc.formatMessage(notification)

	return &TelegramMessage{
		ChatID:    tc.config.ChatID,
		Text:      text,
		ParseMode: "Markdown",
	}
}

// formatMessage formats the notification as Telegram Markdown
func (tc *TelegramClient) formatMessage(notification *Notification) string {
	var builder strings.Builder

	// Title with emoji based on priority
	priorityEmoji := tc.getPriorityEmoji(notification.Priority)
	builder.WriteString(fmt.Sprintf("%s *%s*\n\n", priorityEmoji, tc.escapeMarkdown(notification.Title)))

	// Message content
	builder.WriteString(fmt.Sprintf("%s\n\n", tc.escapeMarkdown(notification.Message)))

	// Priority and type info
	priorityText := tc.getPriorityText(notification.Priority)
	builder.WriteString(fmt.Sprintf("🏷️ *Priority:* %s\n", priorityText))
	builder.WriteString(fmt.Sprintf("📋 *Type:* %s\n", notification.Type))
	builder.WriteString(fmt.Sprintf("⏰ *Time:* %s\n", notification.Timestamp.Format("2006-01-02 15:04:05 UTC")))

	// Add context information
	if len(notification.Context) > 0 {
		builder.WriteString("\n📊 *Details:*\n")
		tc.addContextInfo(&builder, notification.Context)
	}

	// Footer
	builder.WriteString("\n🛰️ _autonomy Daemon_")

	return builder.String()
}

// addContextInfo adds context information to the message
func (tc *TelegramClient) addContextInfo(builder *strings.Builder, context map[string]interface{}) {
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
			valueStr = tc.escapeMarkdown(v)
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
			valueStr = tc.escapeMarkdown(fmt.Sprintf("%v", v))
		}

		builder.WriteString(fmt.Sprintf("• *%s:* %s\n", title, valueStr))
	}
}

// sendMessage sends the message to Telegram Bot API
func (tc *TelegramClient) sendMessage(ctx context.Context, message *TelegramMessage) error {
	// Construct API URL
	apiURL := fmt.Sprintf("https://api.telegram.org/bot%s/sendMessage", tc.config.Token)

	// Marshal message to JSON
	jsonData, err := json.Marshal(message)
	if err != nil {
		return fmt.Errorf("failed to marshal message: %w", err)
	}

	// Create request
	req, err := http.NewRequestWithContext(ctx, "POST", apiURL, bytes.NewBuffer(jsonData))
	if err != nil {
		return fmt.Errorf("failed to create request: %w", err)
	}

	req.Header.Set("Content-Type", "application/json")

	// Send request
	resp, err := tc.client.Do(req)
	if err != nil {
		return fmt.Errorf("failed to send request: %w", err)
	}
	defer resp.Body.Close()

	// Parse response
	var telegramResp TelegramResponse
	if err := json.NewDecoder(resp.Body).Decode(&telegramResp); err != nil {
		return fmt.Errorf("failed to parse response: %w", err)
	}

	// Check for errors
	if !telegramResp.OK {
		return fmt.Errorf("Telegram API error (code %d): %s", telegramResp.ErrorCode, telegramResp.Description)
	}

	tc.logger.Debug("Telegram notification sent successfully")
	return nil
}

// escapeMarkdown escapes special Markdown characters for Telegram
func (tc *TelegramClient) escapeMarkdown(text string) string {
	// Telegram Markdown special characters that need escaping
	replacer := strings.NewReplacer(
		"*", "\\*",
		"_", "\\_",
		"`", "\\`",
		"[", "\\[",
		"]", "\\]",
		"(", "\\(",
		")", "\\)",
	)
	return replacer.Replace(text)
}

// getPriorityEmoji returns emoji for priority level
func (tc *TelegramClient) getPriorityEmoji(priority int) string {
	switch priority {
	case PriorityEmergency:
		return "🚨"
	case PriorityHigh:
		return "⚠️"
	case PriorityNormal:
		return "ℹ️"
	case PriorityLow:
		return "📝"
	case PriorityLowest:
		return "💬"
	default:
		return "ℹ️"
	}
}

// getPriorityText returns formatted priority text for Telegram
func (tc *TelegramClient) getPriorityText(priority int) string {
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
