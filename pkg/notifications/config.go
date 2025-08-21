package notifications

import (
	"time"
)

// UCIConfigProvider interface to avoid circular dependency
type UCIConfigProvider interface {
	GetPushoverConfig() (enabled bool, token, user, device string)
	GetEmailConfig() (enabled bool, smtpHost string, smtpPort int, username, password, from string, to []string, useTLS, useStartTLS bool)
	GetSlackConfig() (enabled bool, webhookURL, channel, username, iconEmoji, iconURL string)
	GetDiscordConfig() (enabled bool, webhookURL, username, avatarURL string)
	GetTelegramConfig() (enabled bool, token, chatID string)
	GetWebhookConfig() (enabled bool, url, method, contentType string, headers map[string]string, template, templateFormat, authType, authToken, authUsername, authPassword, authHeader string, timeout time.Duration, retryAttempts int, retryDelay time.Duration, verifySSL bool)
	GetSMSConfig() (enabled bool, provider, apiKey, from, to string, template string)
}

// ConfigFromUCI converts UCI configuration to multi-channel notification configuration
func ConfigFromUCI(uciConfig UCIConfigProvider) *ChannelConfig {
	if uciConfig == nil {
		return DefaultChannelConfig()
	}

	config := &ChannelConfig{}

	// Pushover Configuration
	enabled, token, user, device := uciConfig.GetPushoverConfig()
	if enabled && token != "" && user != "" {
		config.Pushover = &PushoverConfig{
			Enabled: true,
			Token:   token,
			User:    user,
			Device:  device,
		}
	}

	// Email Configuration
	emailEnabled, smtpHost, smtpPort, username, password, from, to, useTLS, useStartTLS := uciConfig.GetEmailConfig()
	if emailEnabled && smtpHost != "" && from != "" && len(to) > 0 {
		config.Email = &EmailConfig{
			Enabled:     true,
			SMTPHost:    smtpHost,
			SMTPPort:    smtpPort,
			Username:    username,
			Password:    password,
			From:        from,
			To:          to,
			UseTLS:      useTLS,
			UseStartTLS: useStartTLS,
		}

		// Set default SMTP port if not specified
		if config.Email.SMTPPort == 0 {
			if config.Email.UseTLS {
				config.Email.SMTPPort = 465
			} else {
				config.Email.SMTPPort = 587
			}
		}
	}

	// Slack Configuration
	slackEnabled, webhookURL, channel, username, iconEmoji, iconURL := uciConfig.GetSlackConfig()
	if slackEnabled && webhookURL != "" {
		config.Slack = &SlackConfig{
			Enabled:    true,
			WebhookURL: webhookURL,
			Channel:    channel,
			Username:   username,
			IconEmoji:  iconEmoji,
			IconURL:    iconURL,
		}
	}

	// Discord Configuration
	discordEnabled, discordWebhookURL, discordUsername, avatarURL := uciConfig.GetDiscordConfig()
	if discordEnabled && discordWebhookURL != "" {
		config.Discord = &DiscordConfig{
			Enabled:    true,
			WebhookURL: discordWebhookURL,
			Username:   discordUsername,
			AvatarURL:  avatarURL,
		}
	}

	// Telegram Configuration
	telegramEnabled, telegramToken, chatID := uciConfig.GetTelegramConfig()
	if telegramEnabled && telegramToken != "" && chatID != "" {
		config.Telegram = &TelegramConfig{
			Enabled: true,
			Token:   telegramToken,
			ChatID:  chatID,
		}
	}

	// Webhook Configuration
	webhookEnabled, webhookURL, method, contentType, headers, template, templateFormat, authType, authToken, authUsername, authPassword, authHeader, timeout, retryAttempts, retryDelay, verifySSL := uciConfig.GetWebhookConfig()
	if webhookEnabled && webhookURL != "" {
		config.Webhook = &WebhookConfig{
			Enabled:        true,
			URL:            webhookURL,
			Method:         method,
			ContentType:    contentType,
			Headers:        headers,
			Template:       template,
			TemplateFormat: templateFormat,
			AuthType:       authType,
			AuthToken:      authToken,
			AuthUsername:   authUsername,
			AuthPassword:   authPassword,
			AuthHeader:     authHeader,
			Timeout:        int(timeout.Seconds()),
			RetryAttempts:  int(retryAttempts),
			RetryDelay:     int(retryDelay.Seconds()),
			VerifySSL:      verifySSL,
		}
	}

	// SMS Configuration
	smsEnabled, provider, apiKey, _, smsTo, _ := uciConfig.GetSMSConfig()
	if smsEnabled && provider != "" && apiKey != "" && smsTo != "" {
		config.SMS = &SMSConfig{
			Enabled:     true,
			Provider:    provider,
			PhoneNumber: smsTo,
		}
	}

	return config
}

// DefaultChannelConfig returns default multi-channel configuration
func DefaultChannelConfig() *ChannelConfig {
	return &ChannelConfig{
		// All channels disabled by default
		Pushover: &PushoverConfig{Enabled: false},
		Email:    &EmailConfig{Enabled: false},
		Slack:    &SlackConfig{Enabled: false},
		Discord:  &DiscordConfig{Enabled: false},
		Telegram: &TelegramConfig{Enabled: false},
		Webhook:  &WebhookConfig{Enabled: false},
	}
}

// ValidateConfig validates notification configuration
func ValidateConfig(config *NotificationConfig) error {
	if config == nil {
		return nil
	}

	// Validate Pushover credentials if enabled
	if config.PushoverEnabled {
		if config.PushoverToken == "" {
			config.PushoverEnabled = false
		}
		if config.PushoverUser == "" {
			config.PushoverEnabled = false
		}
	}

	// Validate priorities are in valid range (-2 to 2)
	priorities := []*int{
		&config.PriorityFailover,
		&config.PriorityFailback,
		&config.PriorityMemberDown,
		&config.PriorityMemberUp,
		&config.PriorityPredictive,
		&config.PriorityCritical,
		&config.PriorityRecovery,
		&config.PriorityStatusUpdate,
	}

	for _, priority := range priorities {
		if *priority < -2 {
			*priority = -2
		}
		if *priority > 2 {
			*priority = 2
		}
	}

	// Validate timing settings
	if config.CooldownPeriod < 0 {
		config.CooldownPeriod = 5 * time.Minute
	}
	if config.EmergencyCooldown < 0 {
		config.EmergencyCooldown = 1 * time.Minute
	}
	if config.MaxNotificationsHour < 1 {
		config.MaxNotificationsHour = 20
	}
	if config.RetryAttempts < 0 {
		config.RetryAttempts = 3
	}
	if config.RetryDelay < time.Second {
		config.RetryDelay = 30 * time.Second
	}
	if config.HTTPTimeout < time.Second {
		config.HTTPTimeout = 10 * time.Second
	}

	return nil
}
