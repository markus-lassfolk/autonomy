package notifications

import (
	"bytes"
	"context"
	"crypto/tls"
	"encoding/base64"
	"encoding/json"
	"encoding/xml"
	"fmt"
	"net/http"
	"strings"
	"text/template"
	"time"

	"github.com/markus-lassfolk/autonomy/pkg/logx"
)

// WebhookClient handles generic webhook notifications
type WebhookClient struct {
	config *WebhookConfig
	logger *logx.Logger
	client *http.Client
}

// WebhookPayload represents the payload sent to webhooks
type WebhookPayload struct {
	Type      string                 `json:"type"`
	Title     string                 `json:"title"`
	Message   string                 `json:"message"`
	Priority  int                    `json:"priority"`
	Timestamp string                 `json:"timestamp"`
	Context   map[string]interface{} `json:"context,omitempty"`

	// Additional metadata
	Source   string `json:"source"`
	Version  string `json:"version"`
	Hostname string `json:"hostname,omitempty"`
}

// NewWebhookClient creates a new webhook client
func NewWebhookClient(config *WebhookConfig, logger *logx.Logger, client *http.Client) *WebhookClient {
	// Create custom HTTP client if advanced settings are configured
	if config.Timeout > 0 || !config.VerifySSL || !config.FollowRedirects {
		transport := &http.Transport{
			TLSClientConfig: &tls.Config{ MinVersion: tls.VersionTLS13,
				InsecureSkipVerify: !config.VerifySSL,
			},
		}

		timeout := 30 * time.Second
		if config.Timeout > 0 {
			timeout = time.Duration(config.Timeout) * time.Second
		}

		client = &http.Client{
			Transport: transport,
			Timeout:   timeout,
		}

		if !config.FollowRedirects {
			client.CheckRedirect = func(req *http.Request, via []*http.Request) error {
				return http.ErrUseLastResponse
			}
		}
	}

	return &WebhookClient{
		config: config,
		logger: logger,
		client: client,
	}
}

// Send sends a notification to the configured webhook
func (wc *WebhookClient) Send(ctx context.Context, notification *Notification) error {
	if !wc.config.Enabled {
		return fmt.Errorf("webhook notifications are disabled")
	}

	if wc.config.URL == "" {
		return fmt.Errorf("webhook URL is required")
	}

	// Apply filters
	if !wc.shouldSendNotification(notification) {
		wc.logger.Debug("Notification filtered out by webhook configuration",
			"type", notification.Type,
			"priority", notification.Priority,
			"webhook", wc.config.Name)
		return nil
	}

	// Create webhook payload
	payload := wc.createWebhookPayload(notification)

	// Send webhook with retry logic
	return wc.sendWebhookWithRetry(ctx, payload)
}

// createWebhookPayload creates a webhook payload from notification
func (wc *WebhookClient) createWebhookPayload(notification *Notification) *WebhookPayload {
	payload := &WebhookPayload{
		Type:      string(notification.Type),
		Title:     notification.Title,
		Message:   notification.Message,
		Priority:  notification.Priority,
		Timestamp: notification.Timestamp.Format("2006-01-02T15:04:05Z"),
		Context:   notification.Context,
		Source:    "autonomy",
		Version:   "1.0.0",
	}

	// Add hostname if available
	// Note: In a real implementation, you might want to get this from system
	// payload.Hostname = getHostname()

	return payload
}

// sendWebhook sends the payload to the webhook URL
func (wc *WebhookClient) sendWebhook(ctx context.Context, payload *WebhookPayload) error {
	// Determine HTTP method
	method := wc.config.Method
	if method == "" {
		method = "POST"
	}
	method = strings.ToUpper(method)

	// Determine content type
	contentType := wc.config.ContentType
	if contentType == "" {
		contentType = "application/json"
	}

	// Prepare request body - use custom template if configured
	var requestBody []byte
	var err error

	if wc.config.Template != "" {
		// Use custom template
		notification := &Notification{
			Type:      NotificationType(payload.Type),
			Title:     payload.Title,
			Message:   payload.Message,
			Priority:  payload.Priority,
			Timestamp: time.Now(), // Will be overridden by template if needed
			Context:   payload.Context,
		}
		requestBody, err = wc.createCustomPayload(notification)
		if err != nil {
			return fmt.Errorf("failed to create custom payload: %w", err)
		}
	} else {
		// Use standard payload formatting
		switch strings.ToLower(contentType) {
		case "application/json":
			requestBody, err = json.Marshal(payload)
			if err != nil {
				return fmt.Errorf("failed to marshal JSON payload: %w", err)
			}
		case "application/x-www-form-urlencoded":
			// Convert payload to form data
			requestBody = []byte(wc.payloadToFormData(payload))
		default:
			// Default to JSON
			requestBody, err = json.Marshal(payload)
			if err != nil {
				return fmt.Errorf("failed to marshal JSON payload: %w", err)
			}
		}
	}

	// Create request
	req, err := http.NewRequestWithContext(ctx, method, wc.config.URL, bytes.NewBuffer(requestBody))
	if err != nil {
		return fmt.Errorf("failed to create request: %w", err)
	}

	// Set content type
	req.Header.Set("Content-Type", contentType)

	// Add authentication
	wc.addAuthentication(req)

	// Set custom headers (after auth to allow override)
	if wc.config.Headers != nil {
		for key, value := range wc.config.Headers {
			req.Header.Set(key, value)
		}
	}

	// Set default User-Agent if not specified
	if req.Header.Get("User-Agent") == "" {
		req.Header.Set("User-Agent", "autonomy/1.0.0")
	}

	// Send request
	resp, err := wc.client.Do(req)
	if err != nil {
		return fmt.Errorf("failed to send webhook request: %w", err)
	}
	defer resp.Body.Close()

	// Check response status
	if resp.StatusCode < 200 || resp.StatusCode >= 300 {
		return fmt.Errorf("webhook returned status %d", resp.StatusCode)
	}

	wc.logger.Debug("Webhook notification sent successfully",
		"url", wc.config.URL,
		"status", resp.StatusCode,
		"webhook", wc.config.Name)
	return nil
}

// payloadToFormData converts payload to form-encoded data
func (wc *WebhookClient) payloadToFormData(payload *WebhookPayload) string {
	data := make(map[string]string)

	data["type"] = payload.Type
	data["title"] = payload.Title
	data["message"] = payload.Message
	data["priority"] = fmt.Sprintf("%d", payload.Priority)
	data["timestamp"] = payload.Timestamp
	data["source"] = payload.Source
	data["version"] = payload.Version

	if payload.Hostname != "" {
		data["hostname"] = payload.Hostname
	}

	// Add context fields with prefix
	if payload.Context != nil {
		for key, value := range payload.Context {
			data[fmt.Sprintf("context_%s", key)] = fmt.Sprintf("%v", value)
		}
	}

	// Convert to form data
	var parts []string
	for key, value := range data {
		parts = append(parts, fmt.Sprintf("%s=%s", key, value))
	}

	return strings.Join(parts, "&")
}

// shouldSendNotification checks if notification should be sent based on filters
func (wc *WebhookClient) shouldSendNotification(notification *Notification) bool {
	// Check priority filter
	if len(wc.config.PriorityFilter) > 0 {
		priorityAllowed := false
		for _, allowedPriority := range wc.config.PriorityFilter {
			if notification.Priority == allowedPriority {
				priorityAllowed = true
				break
			}
		}
		if !priorityAllowed {
			return false
		}
	}

	// Check type filter
	if len(wc.config.TypeFilter) > 0 {
		typeAllowed := false
		for _, allowedType := range wc.config.TypeFilter {
			if string(notification.Type) == allowedType {
				typeAllowed = true
				break
			}
		}
		if !typeAllowed {
			return false
		}
	}

	return true
}

// sendWebhookWithRetry sends webhook with retry logic
func (wc *WebhookClient) sendWebhookWithRetry(ctx context.Context, payload *WebhookPayload) error {
	maxAttempts := wc.config.RetryAttempts
	if maxAttempts <= 0 {
		maxAttempts = 3 // Default to 3 attempts
	}

	retryDelay := time.Duration(wc.config.RetryDelay) * time.Second
	if retryDelay <= 0 {
		retryDelay = 5 * time.Second // Default to 5 seconds
	}

	var lastErr error
	for attempt := 1; attempt <= maxAttempts; attempt++ {
		err := wc.sendWebhook(ctx, payload)
		if err == nil {
			if attempt > 1 {
				wc.logger.Info("Webhook succeeded after retry",
					"attempt", attempt,
					"webhook", wc.config.Name)
			}
			return nil
		}

		lastErr = err
		wc.logger.Warn("Webhook attempt failed",
			"attempt", attempt,
			"max_attempts", maxAttempts,
			"error", err,
			"webhook", wc.config.Name)

		// Don't wait after the last attempt
		if attempt < maxAttempts {
			select {
			case <-ctx.Done():
				return ctx.Err()
			case <-time.After(retryDelay):
				// Continue to next attempt
			}
		}
	}

	return fmt.Errorf("webhook failed after %d attempts: %w", maxAttempts, lastErr)
}

// createCustomPayload creates payload using custom template if configured
func (wc *WebhookClient) createCustomPayload(notification *Notification) ([]byte, error) {
	if wc.config.Template == "" {
		// Use default payload
		payload := wc.createWebhookPayload(notification)
		return wc.formatPayload(payload)
	}

	// Parse template
	tmpl, err := template.New("webhook").Parse(wc.config.Template)
	if err != nil {
		return nil, fmt.Errorf("failed to parse template: %w", err)
	}

	// Create template data
	data := map[string]interface{}{
		"Type":      string(notification.Type),
		"Title":     notification.Title,
		"Message":   notification.Message,
		"Priority":  notification.Priority,
		"Timestamp": notification.Timestamp,
		"Context":   notification.Context,
		"Source":    "autonomy",
		"Version":   "1.0.0",
	}

	// Apply field mapping
	if wc.config.FieldMapping != nil {
		mappedData := make(map[string]interface{})
		for key, value := range data {
			if mappedKey, exists := wc.config.FieldMapping[key]; exists {
				mappedData[mappedKey] = value
			} else {
				mappedData[key] = value
			}
		}
		data = mappedData
	}

	// Remove excluded fields
	if wc.config.ExcludeFields != nil {
		for _, field := range wc.config.ExcludeFields {
			delete(data, field)
		}
	}

	// Include raw notification if requested
	if wc.config.IncludeRawData {
		data["RawNotification"] = notification
	}

	// Execute template
	var buf bytes.Buffer
	if err := tmpl.Execute(&buf, data); err != nil {
		return nil, fmt.Errorf("failed to execute template: %w", err)
	}

	return buf.Bytes(), nil
}

// formatPayload formats payload based on template format
func (wc *WebhookClient) formatPayload(payload *WebhookPayload) ([]byte, error) {
	format := strings.ToLower(wc.config.TemplateFormat)
	if format == "" {
		format = "json"
	}

	switch format {
	case "json":
		return json.Marshal(payload)
	case "xml":
		return xml.Marshal(payload)
	case "text":
		return []byte(fmt.Sprintf("Type: %s\nTitle: %s\nMessage: %s\nPriority: %d\nTimestamp: %s",
			payload.Type, payload.Title, payload.Message, payload.Priority, payload.Timestamp)), nil
	default:
		// Default to JSON for unknown formats
		return json.Marshal(payload)
	}
}

// addAuthentication adds authentication headers to the request
func (wc *WebhookClient) addAuthentication(req *http.Request) {
	switch strings.ToLower(wc.config.AuthType) {
	case "bearer":
		if wc.config.AuthToken != "" {
			req.Header.Set("Authorization", "Bearer "+wc.config.AuthToken)
		}
	case "basic":
		if wc.config.AuthUsername != "" && wc.config.AuthPassword != "" {
			auth := base64.StdEncoding.EncodeToString([]byte(wc.config.AuthUsername + ":" + wc.config.AuthPassword))
			req.Header.Set("Authorization", "Basic "+auth)
		}
	case "api_key":
		if wc.config.AuthToken != "" {
			headerName := wc.config.AuthHeader
			if headerName == "" {
				headerName = "X-API-Key"
			}
			req.Header.Set(headerName, wc.config.AuthToken)
		}
	case "custom":
		// Custom auth handled via Headers configuration
	}
}

// GetWebhookInfo returns information about the webhook configuration
func (wc *WebhookClient) GetWebhookInfo() map[string]interface{} {
	info := map[string]interface{}{
		"enabled":         wc.config.Enabled,
		"url":             wc.config.URL,
		"method":          wc.config.Method,
		"auth_type":       wc.config.AuthType,
		"has_template":    wc.config.Template != "",
		"template_format": wc.config.TemplateFormat,
	}

	if wc.config.Name != "" {
		info["name"] = wc.config.Name
	}
	if wc.config.Description != "" {
		info["description"] = wc.config.Description
	}
	if len(wc.config.Tags) > 0 {
		info["tags"] = wc.config.Tags
	}
	if len(wc.config.PriorityFilter) > 0 {
		info["priority_filter"] = wc.config.PriorityFilter
	}
	if len(wc.config.TypeFilter) > 0 {
		info["type_filter"] = wc.config.TypeFilter
	}

	return info
}
