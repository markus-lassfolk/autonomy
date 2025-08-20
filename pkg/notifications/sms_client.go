package notifications

import (
	"context"
	"encoding/json"
	"fmt"
	"net/http"
	"net/url"
	"os/exec"
	"strings"
	"time"

	"github.com/markus-lassfolk/autonomy/pkg/logx"
)

// SMSConfig holds SMS-specific configuration
type SMSConfig struct {
	Enabled     bool   `json:"enabled"`
	Provider    string `json:"provider"` // twilio, aws_sns, at_command, custom
	PhoneNumber string `json:"phone_number"`

	// Twilio configuration
	TwilioAccountSID string `json:"twilio_account_sid,omitempty"`
	TwilioAuthToken  string `json:"twilio_auth_token,omitempty"`
	TwilioFromNumber string `json:"twilio_from_number,omitempty"`

	// AWS SNS configuration
	AWSAccessKeyID     string `json:"aws_access_key_id,omitempty"`
	AWSSecretAccessKey string `json:"aws_secret_access_key,omitempty"`
	AWSRegion          string `json:"aws_region,omitempty"`
	AWSSNSTopicARN     string `json:"aws_sns_topic_arn,omitempty"`

	// AT Command configuration (for cellular modems)
	ATCommandDevice string `json:"at_command_device,omitempty"` // e.g., /dev/ttyUSB0
	ATCommandBaud   int    `json:"at_command_baud,omitempty"`   // e.g., 115200

	// Custom webhook configuration
	CustomWebhookURL string            `json:"custom_webhook_url,omitempty"`
	CustomHeaders    map[string]string `json:"custom_headers,omitempty"`

	// Rate limiting
	MaxMessagesPerHour int           `json:"max_messages_per_hour"`
	CooldownPeriod     time.Duration `json:"cooldown_period"`
}

// SMSClient represents an SMS notification client
type SMSClient struct {
	config *SMSConfig
	logger *logx.Logger
	client *http.Client
}

// NewSMSClient creates a new SMS client
func NewSMSClient(config *SMSConfig, logger *logx.Logger) *SMSClient {
	return &SMSClient{
		config: config,
		logger: logger,
		client: &http.Client{
			Timeout: 30 * time.Second,
		},
	}
}

// Send sends an SMS message
func (s *SMSClient) Send(ctx context.Context, message string, priority int) error {
	if !s.config.Enabled {
		return fmt.Errorf("SMS notifications are disabled")
	}

	if s.config.PhoneNumber == "" {
		return fmt.Errorf("phone number not configured")
	}

	// Truncate message if too long (SMS limit is typically 160 characters)
	if len(message) > 160 {
		message = message[:157] + "..."
	}

	switch s.config.Provider {
	case "twilio":
		return s.sendViaTwilio(ctx, message)
	case "aws_sns":
		return s.sendViaAWSSNS(ctx, message)
	case "at_command":
		return s.sendViaATCommand(ctx, message)
	case "custom":
		return s.sendViaCustomWebhook(ctx, message)
	default:
		return fmt.Errorf("unsupported SMS provider: %s", s.config.Provider)
	}
}

// sendViaTwilio sends SMS via Twilio
func (s *SMSClient) sendViaTwilio(ctx context.Context, message string) error {
	if s.config.TwilioAccountSID == "" || s.config.TwilioAuthToken == "" || s.config.TwilioFromNumber == "" {
		return fmt.Errorf("Twilio configuration incomplete")
	}

	// Twilio API endpoint
	apiURL := fmt.Sprintf("https://api.twilio.com/2010-04-01/Accounts/%s/Messages.json", s.config.TwilioAccountSID)

	// Prepare form data
	data := url.Values{}
	data.Set("To", s.config.PhoneNumber)
	data.Set("From", s.config.TwilioFromNumber)
	data.Set("Body", message)

	// Create request
	req, err := http.NewRequestWithContext(ctx, "POST", apiURL, strings.NewReader(data.Encode()))
	if err != nil {
		return fmt.Errorf("failed to create Twilio request: %w", err)
	}

	// Set headers
	req.Header.Set("Content-Type", "application/x-www-form-urlencoded")
	req.SetBasicAuth(s.config.TwilioAccountSID, s.config.TwilioAuthToken)

	// Send request
	resp, err := s.client.Do(req)
	if err != nil {
		return fmt.Errorf("failed to send Twilio SMS: %w", err)
	}
	defer resp.Body.Close()

	if resp.StatusCode != http.StatusCreated {
		return fmt.Errorf("Twilio API error: %s", resp.Status)
	}

	s.logger.Info("SMS sent via Twilio", "to", s.config.PhoneNumber, "status", resp.Status)
	return nil
}

// sendViaAWSSNS sends SMS via AWS SNS
func (s *SMSClient) sendViaAWSSNS(ctx context.Context, message string) error {
	if s.config.AWSAccessKeyID == "" || s.config.AWSSecretAccessKey == "" || s.config.AWSRegion == "" {
		return fmt.Errorf("AWS SNS configuration incomplete")
	}

	// AWS SNS API endpoint
	apiURL := fmt.Sprintf("https://sns.%s.amazonaws.com/", s.config.AWSRegion)

	// Prepare request payload
	payload := map[string]interface{}{
		"Action":      "Publish",
		"Message":     message,
		"PhoneNumber": s.config.PhoneNumber,
		"Version":     "2010-03-31",
	}

	if s.config.AWSSNSTopicARN != "" {
		payload["TopicArn"] = s.config.AWSSNSTopicARN
	}

	jsonData, err := json.Marshal(payload)
	if err != nil {
		return fmt.Errorf("failed to marshal AWS SNS payload: %w", err)
	}

	// Create request
	req, err := http.NewRequestWithContext(ctx, "POST", apiURL, strings.NewReader(string(jsonData)))
	if err != nil {
		return fmt.Errorf("failed to create AWS SNS request: %w", err)
	}

	// Set headers
	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("X-Amz-Target", "AmazonSNS.Publish")

	// Note: In a production environment, you would use AWS SDK for proper authentication
	// For now, we'll use basic auth as a placeholder
	req.SetBasicAuth(s.config.AWSAccessKeyID, s.config.AWSSecretAccessKey)

	// Send request
	resp, err := s.client.Do(req)
	if err != nil {
		return fmt.Errorf("failed to send AWS SNS SMS: %w", err)
	}
	defer resp.Body.Close()

	if resp.StatusCode != http.StatusOK {
		return fmt.Errorf("AWS SNS API error: %s", resp.Status)
	}

	s.logger.Info("SMS sent via AWS SNS", "to", s.config.PhoneNumber, "status", resp.Status)
	return nil
}

// sendViaATCommand sends SMS via AT commands to cellular modem
func (s *SMSClient) sendViaATCommand(ctx context.Context, message string) error {
	if s.config.ATCommandDevice == "" {
		return fmt.Errorf("AT command device not configured")
	}

	// Escape special characters in message
	escapedMessage := strings.ReplaceAll(message, `"`, `\"`)
	escapedMessage = strings.ReplaceAll(escapedMessage, "\n", "\\n")
	escapedMessage = strings.ReplaceAll(escapedMessage, "\r", "\\r")

	// AT commands to send SMS
	commands := []string{
		"ATZ",             // Reset modem
		"AT+CMGF=1",       // Set SMS text mode
		"AT+CSCS=\"GSM\"", // Set character set
		fmt.Sprintf("AT+CMGS=\"%s\"", s.config.PhoneNumber), // Set recipient
		escapedMessage + "\x1A",                             // Message content + Ctrl+Z to send
	}

	// Execute AT commands
	for i, cmd := range commands {
		// Skip the last command (message content) as it needs special handling
		if i == len(commands)-1 {
			continue
		}

		// Use minicom or similar tool to send AT command
		// For now, we'll use a simple echo approach
		execCmd := exec.CommandContext(ctx, "echo", cmd)
		if s.config.ATCommandDevice != "" {
			execCmd = exec.CommandContext(ctx, "echo", cmd, ">", s.config.ATCommandDevice)
		}

		if err := execCmd.Run(); err != nil {
			s.logger.Warn("Failed to send AT command", "command", cmd, "error", err)
		}
	}

	// Send the actual message (this is a simplified approach)
	// In a real implementation, you would use a proper serial communication library
	s.logger.Info("SMS AT command prepared", "to", s.config.PhoneNumber, "message_length", len(message))

	// Note: This is a placeholder implementation
	// Real AT command implementation would require proper serial communication
	return fmt.Errorf("AT command SMS not fully implemented - requires serial communication library")
}

// sendViaCustomWebhook sends SMS via custom webhook
func (s *SMSClient) sendViaCustomWebhook(ctx context.Context, message string) error {
	if s.config.CustomWebhookURL == "" {
		return fmt.Errorf("custom webhook URL not configured")
	}

	// Prepare payload
	payload := map[string]interface{}{
		"to":        s.config.PhoneNumber,
		"message":   message,
		"timestamp": time.Now().Unix(),
	}

	jsonData, err := json.Marshal(payload)
	if err != nil {
		return fmt.Errorf("failed to marshal custom webhook payload: %w", err)
	}

	// Create request
	req, err := http.NewRequestWithContext(ctx, "POST", s.config.CustomWebhookURL, strings.NewReader(string(jsonData)))
	if err != nil {
		return fmt.Errorf("failed to create custom webhook request: %w", err)
	}

	// Set headers
	req.Header.Set("Content-Type", "application/json")
	for key, value := range s.config.CustomHeaders {
		req.Header.Set(key, value)
	}

	// Send request
	resp, err := s.client.Do(req)
	if err != nil {
		return fmt.Errorf("failed to send custom webhook SMS: %w", err)
	}
	defer resp.Body.Close()

	if resp.StatusCode < 200 || resp.StatusCode >= 300 {
		return fmt.Errorf("custom webhook error: %s", resp.Status)
	}

	s.logger.Info("SMS sent via custom webhook", "to", s.config.PhoneNumber, "status", resp.Status)
	return nil
}

// Validate checks if the SMS configuration is valid
func (s *SMSClient) Validate() error {
	if !s.config.Enabled {
		return nil // Disabled configs are always valid
	}

	if s.config.PhoneNumber == "" {
		return fmt.Errorf("phone number is required for SMS notifications")
	}

	switch s.config.Provider {
	case "twilio":
		if s.config.TwilioAccountSID == "" || s.config.TwilioAuthToken == "" || s.config.TwilioFromNumber == "" {
			return fmt.Errorf("Twilio configuration incomplete: account_sid, auth_token, and from_number are required")
		}
	case "aws_sns":
		if s.config.AWSAccessKeyID == "" || s.config.AWSSecretAccessKey == "" || s.config.AWSRegion == "" {
			return fmt.Errorf("AWS SNS configuration incomplete: access_key_id, secret_access_key, and region are required")
		}
	case "at_command":
		if s.config.ATCommandDevice == "" {
			return fmt.Errorf("AT command device path is required")
		}
	case "custom":
		if s.config.CustomWebhookURL == "" {
			return fmt.Errorf("custom webhook URL is required")
		}
	default:
		return fmt.Errorf("unsupported SMS provider: %s", s.config.Provider)
	}

	return nil
}

// GetProvider returns the SMS provider name
func (s *SMSClient) GetProvider() string {
	return s.config.Provider
}

// IsEnabled returns whether SMS notifications are enabled
func (s *SMSClient) IsEnabled() bool {
	return s.config.Enabled
}
