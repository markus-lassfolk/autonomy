package notifications

import (
	"context"
	"fmt"
	"net/http"
	"strings"
	"time"

	"github.com/markus-lassfolk/autonomy/pkg/logx"
)

// NotificationChannel represents different notification channels
type NotificationChannel string

const (
	ChannelPushover NotificationChannel = "pushover"
	ChannelEmail    NotificationChannel = "email"
	ChannelSlack    NotificationChannel = "slack"
	ChannelDiscord  NotificationChannel = "discord"
	ChannelTelegram NotificationChannel = "telegram"
	ChannelWebhook  NotificationChannel = "webhook"
	ChannelSMS      NotificationChannel = "sms"
)

// ChannelConfig holds configuration for different notification channels
type ChannelConfig struct {
	// Pushover configuration
	Pushover *PushoverConfig `json:"pushover,omitempty"`

	// Email configuration
	Email *EmailConfig `json:"email,omitempty"`

	// Slack configuration
	Slack *SlackConfig `json:"slack,omitempty"`

	// Discord configuration
	Discord *DiscordConfig `json:"discord,omitempty"`

	// Telegram configuration
	Telegram *TelegramConfig `json:"telegram,omitempty"`

	// Generic webhook configuration
	Webhook *WebhookConfig `json:"webhook,omitempty"`

	// SMS configuration
	SMS *SMSConfig `json:"sms,omitempty"`
}

// PushoverConfig holds Pushover-specific configuration
type PushoverConfig struct {
	Enabled bool   `json:"enabled"`
	Token   string `json:"token"`
	User    string `json:"user"`
	Device  string `json:"device,omitempty"`
}

// EmailConfig holds email-specific configuration
type EmailConfig struct {
	Enabled     bool     `json:"enabled"`
	SMTPHost    string   `json:"smtp_host"`
	SMTPPort    int      `json:"smtp_port"`
	Username    string   `json:"username"`
	Password    string   `json:"password"`
	From        string   `json:"from"`
	To          []string `json:"to"`
	UseTLS      bool     `json:"use_tls"`
	UseStartTLS bool     `json:"use_starttls"`
}

// SlackConfig holds Slack-specific configuration
type SlackConfig struct {
	Enabled    bool   `json:"enabled"`
	WebhookURL string `json:"webhook_url"`
	Channel    string `json:"channel,omitempty"`
	Username   string `json:"username,omitempty"`
	IconEmoji  string `json:"icon_emoji,omitempty"`
	IconURL    string `json:"icon_url,omitempty"`
}

// DiscordConfig holds Discord-specific configuration
type DiscordConfig struct {
	Enabled    bool   `json:"enabled"`
	WebhookURL string `json:"webhook_url"`
	Username   string `json:"username,omitempty"`
	AvatarURL  string `json:"avatar_url,omitempty"`
}

// TelegramConfig holds Telegram-specific configuration
type TelegramConfig struct {
	Enabled bool   `json:"enabled"`
	Token   string `json:"token"`
	ChatID  string `json:"chat_id"`
}

// WebhookConfig holds generic webhook configuration
type WebhookConfig struct {
	Enabled     bool              `json:"enabled"`
	URL         string            `json:"url"`
	Method      string            `json:"method"`
	Headers     map[string]string `json:"headers,omitempty"`
	ContentType string            `json:"content_type"`

	// Advanced configuration for custom integrations
	Template        string `json:"template,omitempty"`        // Custom payload template (Go template syntax)
	TemplateFormat  string `json:"template_format,omitempty"` // "json", "xml", "text", "custom"
	AuthType        string `json:"auth_type,omitempty"`       // "bearer", "basic", "api_key", "custom"
	AuthToken       string `json:"auth_token,omitempty"`      // Token for bearer/api_key auth
	AuthUsername    string `json:"auth_username,omitempty"`   // Username for basic auth
	AuthPassword    string `json:"auth_password,omitempty"`   // Password for basic auth
	AuthHeader      string `json:"auth_header,omitempty"`     // Custom auth header name (for api_key)
	Timeout         int    `json:"timeout,omitempty"`         // Request timeout in seconds (default: 30)
	RetryAttempts   int    `json:"retry_attempts,omitempty"`  // Number of retry attempts (default: 3)
	RetryDelay      int    `json:"retry_delay,omitempty"`     // Delay between retries in seconds (default: 5)
	VerifySSL       bool   `json:"verify_ssl"`                // Verify SSL certificates (default: true)
	FollowRedirects bool   `json:"follow_redirects"`          // Follow HTTP redirects (default: true)

	// Filtering and transformation
	PriorityFilter []int             `json:"priority_filter,omitempty"` // Only send specific priorities (empty = all)
	TypeFilter     []string          `json:"type_filter,omitempty"`     // Only send specific types (empty = all)
	FieldMapping   map[string]string `json:"field_mapping,omitempty"`   // Map fields to different names
	ExcludeFields  []string          `json:"exclude_fields,omitempty"`  // Fields to exclude from payload
	IncludeRawData bool              `json:"include_raw_data"`          // Include raw notification object

	// Integration-specific settings
	Name        string   `json:"name,omitempty"`        // Human-readable name for this webhook
	Description string   `json:"description,omitempty"` // Description of the integration
	Tags        []string `json:"tags,omitempty"`        // Tags for categorization
}

// MultiChannelNotifier handles notifications across multiple channels
type MultiChannelNotifier struct {
	config *ChannelConfig
	logger *logx.Logger
	client *http.Client

	// Channel-specific clients
	pushoverClient *PushoverClient
	emailClient    *EmailClient
	slackClient    *SlackClient
	discordClient  *DiscordClient
	telegramClient *TelegramClient
	webhookClient  *WebhookClient
	smsClient      *SMSClient
}

// NewMultiChannelNotifier creates a new multi-channel notifier
func NewMultiChannelNotifier(config *ChannelConfig, logger *logx.Logger) *MultiChannelNotifier {
	client := &http.Client{
		Timeout: 30 * time.Second,
	}

	mcn := &MultiChannelNotifier{
		config: config,
		logger: logger,
		client: client,
	}

	// Initialize channel-specific clients
	if config.Pushover != nil && config.Pushover.Enabled {
		mcn.pushoverClient = NewPushoverClient(config.Pushover, logger, client)
	}

	if config.Email != nil && config.Email.Enabled {
		mcn.emailClient = NewEmailClient(config.Email, logger)
	}

	if config.Slack != nil && config.Slack.Enabled {
		mcn.slackClient = NewSlackClient(config.Slack, logger, client)
	}

	if config.Discord != nil && config.Discord.Enabled {
		mcn.discordClient = NewDiscordClient(config.Discord, logger, client)
	}

	if config.Telegram != nil && config.Telegram.Enabled {
		mcn.telegramClient = NewTelegramClient(config.Telegram, logger, client)
	}

	if config.Webhook != nil && config.Webhook.Enabled {
		mcn.webhookClient = NewWebhookClient(config.Webhook, logger, client)
	}

	if config.SMS != nil && config.SMS.Enabled {
		mcn.smsClient = NewSMSClient(config.SMS, logger)
	}

	return mcn
}

// SendNotification sends a notification to all enabled channels
func (mcn *MultiChannelNotifier) SendNotification(ctx context.Context, notification *Notification) error {
	var errors []string
	successCount := 0

	// Send to Pushover
	if mcn.pushoverClient != nil {
		if err := mcn.pushoverClient.Send(ctx, notification); err != nil {
			errors = append(errors, fmt.Sprintf("Pushover: %v", err))
			mcn.logger.Warn("Pushover notification failed", "error", err)
		} else {
			successCount++
			mcn.logger.Debug("Pushover notification sent successfully")
		}
	}

	// Send to Email
	if mcn.emailClient != nil {
		if err := mcn.emailClient.Send(ctx, notification); err != nil {
			errors = append(errors, fmt.Sprintf("Email: %v", err))
			mcn.logger.Warn("Email notification failed", "error", err)
		} else {
			successCount++
			mcn.logger.Debug("Email notification sent successfully")
		}
	}

	// Send to Slack
	if mcn.slackClient != nil {
		if err := mcn.slackClient.Send(ctx, notification); err != nil {
			errors = append(errors, fmt.Sprintf("Slack: %v", err))
			mcn.logger.Warn("Slack notification failed", "error", err)
		} else {
			successCount++
			mcn.logger.Debug("Slack notification sent successfully")
		}
	}

	// Send to Discord
	if mcn.discordClient != nil {
		if err := mcn.discordClient.Send(ctx, notification); err != nil {
			errors = append(errors, fmt.Sprintf("Discord: %v", err))
			mcn.logger.Warn("Discord notification failed", "error", err)
		} else {
			successCount++
			mcn.logger.Debug("Discord notification sent successfully")
		}
	}

	// Send to Telegram
	if mcn.telegramClient != nil {
		if err := mcn.telegramClient.Send(ctx, notification); err != nil {
			errors = append(errors, fmt.Sprintf("Telegram: %v", err))
			mcn.logger.Warn("Telegram notification failed", "error", err)
		} else {
			successCount++
			mcn.logger.Debug("Telegram notification sent successfully")
		}
	}

	// Send to Webhook
	if mcn.webhookClient != nil {
		if err := mcn.webhookClient.Send(ctx, notification); err != nil {
			errors = append(errors, fmt.Sprintf("Webhook: %v", err))
			mcn.logger.Warn("Webhook notification failed", "error", err)
		} else {
			successCount++
			mcn.logger.Debug("Webhook notification sent successfully")
		}
	}

	// Send to SMS
	if mcn.smsClient != nil {
		if err := mcn.smsClient.Send(ctx, notification.Message, notification.Priority); err != nil {
			errors = append(errors, fmt.Sprintf("SMS: %v", err))
			mcn.logger.Warn("SMS notification failed", "error", err)
		} else {
			successCount++
			mcn.logger.Debug("SMS notification sent successfully")
		}
	}

	// Log summary
	mcn.logger.Info("Multi-channel notification completed",
		"success_count", successCount,
		"error_count", len(errors),
		"notification_type", notification.Type)

	// Return error if all channels failed
	if successCount == 0 && len(errors) > 0 {
		return fmt.Errorf("all notification channels failed: %s", strings.Join(errors, "; "))
	}

	// Return partial error if some channels failed
	if len(errors) > 0 {
		mcn.logger.Warn("Some notification channels failed", "errors", errors)
	}

	return nil
}

// GetEnabledChannels returns a list of enabled notification channels
func (mcn *MultiChannelNotifier) GetEnabledChannels() []NotificationChannel {
	var channels []NotificationChannel

	if mcn.pushoverClient != nil {
		channels = append(channels, ChannelPushover)
	}
	if mcn.emailClient != nil {
		channels = append(channels, ChannelEmail)
	}
	if mcn.slackClient != nil {
		channels = append(channels, ChannelSlack)
	}
	if mcn.discordClient != nil {
		channels = append(channels, ChannelDiscord)
	}
	if mcn.telegramClient != nil {
		channels = append(channels, ChannelTelegram)
	}
	if mcn.webhookClient != nil {
		channels = append(channels, ChannelWebhook)
	}
	if mcn.smsClient != nil {
		channels = append(channels, ChannelSMS)
	}

	return channels
}

// TestChannels tests all enabled notification channels
func (mcn *MultiChannelNotifier) TestChannels(ctx context.Context) map[NotificationChannel]error {
	results := make(map[NotificationChannel]error)

	// Create test notification
	testNotification := &Notification{
		Type:      NotificationStatusUpdate,
		Title:     "🧪 autonomy Notification Test",
		Message:   "This is a test notification to verify channel configuration.",
		Priority:  PriorityLow,
		Timestamp: time.Now(),
		Context: map[string]interface{}{
			"test": true,
		},
	}

	// Test each channel
	if mcn.pushoverClient != nil {
		results[ChannelPushover] = mcn.pushoverClient.Send(ctx, testNotification)
	}
	if mcn.emailClient != nil {
		results[ChannelEmail] = mcn.emailClient.Send(ctx, testNotification)
	}
	if mcn.slackClient != nil {
		results[ChannelSlack] = mcn.slackClient.Send(ctx, testNotification)
	}
	if mcn.discordClient != nil {
		results[ChannelDiscord] = mcn.discordClient.Send(ctx, testNotification)
	}
	if mcn.telegramClient != nil {
		results[ChannelTelegram] = mcn.telegramClient.Send(ctx, testNotification)
	}
	if mcn.webhookClient != nil {
		results[ChannelWebhook] = mcn.webhookClient.Send(ctx, testNotification)
	}

	return results
}

// GetChannelStatus returns status information for all channels
func (mcn *MultiChannelNotifier) GetChannelStatus() map[NotificationChannel]map[string]interface{} {
	status := make(map[NotificationChannel]map[string]interface{})

	if mcn.pushoverClient != nil {
		status[ChannelPushover] = map[string]interface{}{
			"enabled": true,
			"config":  "configured",
		}
	}
	if mcn.emailClient != nil {
		status[ChannelEmail] = map[string]interface{}{
			"enabled": true,
			"config":  "configured",
		}
	}
	if mcn.slackClient != nil {
		status[ChannelSlack] = map[string]interface{}{
			"enabled": true,
			"config":  "configured",
		}
	}
	if mcn.discordClient != nil {
		status[ChannelDiscord] = map[string]interface{}{
			"enabled": true,
			"config":  "configured",
		}
	}
	if mcn.telegramClient != nil {
		status[ChannelTelegram] = map[string]interface{}{
			"enabled": true,
			"config":  "configured",
		}
	}
	if mcn.webhookClient != nil {
		status[ChannelWebhook] = map[string]interface{}{
			"enabled": true,
			"config":  "configured",
		}
	}

	return status
}

// Notification represents a notification to be sent
type Notification struct {
	Type      NotificationType       `json:"type"`
	Title     string                 `json:"title"`
	Message   string                 `json:"message"`
	Priority  int                    `json:"priority"`
	Timestamp time.Time              `json:"timestamp"`
	Context   map[string]interface{} `json:"context,omitempty"`

	// Channel-specific overrides
	PushoverSound string `json:"pushover_sound,omitempty"`
	EmailSubject  string `json:"email_subject,omitempty"`
	SlackColor    string `json:"slack_color,omitempty"`
	DiscordColor  int    `json:"discord_color,omitempty"`
}
