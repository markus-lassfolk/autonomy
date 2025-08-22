package notifications

import (
	"context"
	"crypto/tls"
	"fmt"
	"net/smtp"
	"strings"
	"time"

	"github.com/markus-lassfolk/autonomy/pkg/logx"
)

// EmailClient handles email notifications
type EmailClient struct {
	config *EmailConfig
	logger *logx.Logger
}

// NewEmailClient creates a new email client
func NewEmailClient(config *EmailConfig, logger *logx.Logger) *EmailClient {
	return &EmailClient{
		config: config,
		logger: logger,
	}
}

// Send sends a notification via email
func (ec *EmailClient) Send(ctx context.Context, notification *Notification) error {
	if !ec.config.Enabled {
		return fmt.Errorf("email notifications are disabled")
	}

	if ec.config.SMTPHost == "" || ec.config.From == "" || len(ec.config.To) == 0 {
		return fmt.Errorf("email configuration incomplete")
	}

	// Prepare email content
	subject := notification.Title
	if notification.EmailSubject != "" {
		subject = notification.EmailSubject
	}

	body := ec.formatEmailBody(notification)

	// Send email to each recipient
	var errors []string
	for _, recipient := range ec.config.To {
		if err := ec.sendEmail(ctx, recipient, subject, body); err != nil {
			errors = append(errors, fmt.Sprintf("%s: %v", recipient, err))
			ec.logger.Warn("Failed to send email", "recipient", recipient, "error", err)
		} else {
			ec.logger.Debug("Email sent successfully", "recipient", recipient)
		}
	}

	if len(errors) > 0 {
		return fmt.Errorf("email sending failed: %s", strings.Join(errors, "; "))
	}

	return nil
}

// sendEmail sends an email to a single recipient
func (ec *EmailClient) sendEmail(ctx context.Context, to, subject, body string) error {
	// Create message
	message := ec.createMessage(to, subject, body)

	// Setup authentication
	var auth smtp.Auth
	if ec.config.Username != "" && ec.config.Password != "" {
		auth = smtp.PlainAuth("", ec.config.Username, ec.config.Password, ec.config.SMTPHost)
	}

	// Determine server address
	serverAddr := fmt.Sprintf("%s:%d", ec.config.SMTPHost, ec.config.SMTPPort)

	// Send email based on TLS configuration
	if ec.config.UseTLS {
		return ec.sendEmailTLS(serverAddr, auth, ec.config.From, []string{to}, message)
	} else if ec.config.UseStartTLS {
		return ec.sendEmailStartTLS(serverAddr, auth, ec.config.From, []string{to}, message)
	} else {
		return smtp.SendMail(serverAddr, auth, ec.config.From, []string{to}, message)
	}
}

// sendEmailTLS sends email using TLS connection
func (ec *EmailClient) sendEmailTLS(addr string, auth smtp.Auth, from string, to []string, msg []byte) error {
	// Create TLS connection
	tlsConfig := &tls.Config{
		MinVersion: tls.VersionTLS13,
		ServerName: ec.config.SMTPHost,
	}

	conn, err := tls.Dial("tcp", addr, tlsConfig)
	if err != nil {
		return fmt.Errorf("failed to create TLS connection: %w", err)
	}
	defer conn.Close()

	// Create SMTP client
	client, err := smtp.NewClient(conn, ec.config.SMTPHost)
	if err != nil {
		return fmt.Errorf("failed to create SMTP client: %w", err)
	}
	defer client.Quit()

	// Authenticate if credentials provided
	if auth != nil {
		if err := client.Auth(auth); err != nil {
			return fmt.Errorf("SMTP authentication failed: %w", err)
		}
	}

	// Send email
	if err := client.Mail(from); err != nil {
		return fmt.Errorf("failed to set sender: %w", err)
	}

	for _, recipient := range to {
		if err := client.Rcpt(recipient); err != nil {
			return fmt.Errorf("failed to set recipient %s: %w", recipient, err)
		}
	}

	writer, err := client.Data()
	if err != nil {
		return fmt.Errorf("failed to get data writer: %w", err)
	}

	_, err = writer.Write(msg)
	if err != nil {
		return fmt.Errorf("failed to write message: %w", err)
	}

	return writer.Close()
}

// sendEmailStartTLS sends email using STARTTLS
func (ec *EmailClient) sendEmailStartTLS(addr string, auth smtp.Auth, from string, to []string, msg []byte) error {
	// Create plain connection
	client, err := smtp.Dial(addr)
	if err != nil {
		return fmt.Errorf("failed to connect to SMTP server: %w", err)
	}
	defer client.Quit()

	// Start TLS if supported
	if ok, _ := client.Extension("STARTTLS"); ok {
		tlsConfig := &tls.Config{
			MinVersion: tls.VersionTLS13,
			ServerName: ec.config.SMTPHost,
		}
		if err := client.StartTLS(tlsConfig); err != nil {
			return fmt.Errorf("failed to start TLS: %w", err)
		}
	}

	// Authenticate if credentials provided
	if auth != nil {
		if err := client.Auth(auth); err != nil {
			return fmt.Errorf("SMTP authentication failed: %w", err)
		}
	}

	// Send email
	if err := client.Mail(from); err != nil {
		return fmt.Errorf("failed to set sender: %w", err)
	}

	for _, recipient := range to {
		if err := client.Rcpt(recipient); err != nil {
			return fmt.Errorf("failed to set recipient %s: %w", recipient, err)
		}
	}

	writer, err := client.Data()
	if err != nil {
		return fmt.Errorf("failed to get data writer: %w", err)
	}

	_, err = writer.Write(msg)
	if err != nil {
		return fmt.Errorf("failed to write message: %w", err)
	}

	return writer.Close()
}

// createMessage creates the email message
func (ec *EmailClient) createMessage(to, subject, body string) []byte {
	headers := make(map[string]string)
	headers["From"] = ec.config.From
	headers["To"] = to
	headers["Subject"] = subject
	headers["Date"] = time.Now().Format(time.RFC1123Z)
	headers["Content-Type"] = "text/html; charset=UTF-8"
	headers["MIME-Version"] = "1.0"

	message := ""
	for key, value := range headers {
		message += fmt.Sprintf("%s: %s\r\n", key, value)
	}
	message += "\r\n" + body

	return []byte(message)
}

// formatEmailBody formats the notification as HTML email body
func (ec *EmailClient) formatEmailBody(notification *Notification) string {
	priorityColor := ec.getPriorityColor(notification.Priority)
	priorityText := ec.getPriorityText(notification.Priority)

	html := fmt.Sprintf(`
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <title>%s</title>
    <style>
        body { font-family: Arial, sans-serif; margin: 0; padding: 20px; background-color: #f5f5f5; }
        .container { max-width: 600px; margin: 0 auto; background-color: white; border-radius: 8px; overflow: hidden; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }
        .header { background-color: %s; color: white; padding: 20px; text-align: center; }
        .header h1 { margin: 0; font-size: 24px; }
        .priority { background-color: rgba(255,255,255,0.2); padding: 5px 10px; border-radius: 15px; font-size: 12px; margin-top: 10px; display: inline-block; }
        .content { padding: 20px; }
        .message { font-size: 16px; line-height: 1.6; margin-bottom: 20px; }
        .details { background-color: #f8f9fa; padding: 15px; border-radius: 5px; margin-top: 20px; }
        .details h3 { margin-top: 0; color: #333; }
        .details table { width: 100%%; border-collapse: collapse; }
        .details td { padding: 8px 0; border-bottom: 1px solid #eee; }
        .details td:first-child { font-weight: bold; width: 30%%; }
        .footer { background-color: #f8f9fa; padding: 15px 20px; text-align: center; font-size: 12px; color: #666; }
        .timestamp { color: #888; font-size: 14px; }
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <h1>🛰️ autonomy Alert</h1>
            <div class="priority">%s Priority</div>
        </div>
        <div class="content">
            <h2>%s</h2>
            <div class="message">%s</div>
            <div class="timestamp">⏰ %s</div>
            %s
        </div>
        <div class="footer">
            <p>This notification was sent by autonomy Daemon</p>
            <p>Notification Type: %s</p>
        </div>
    </div>
</body>
</html>`,
		notification.Title,
		priorityColor,
		priorityText,
		notification.Title,
		strings.ReplaceAll(notification.Message, "\n", "<br>"),
		notification.Timestamp.Format("2006-01-02 15:04:05 UTC"),
		ec.formatContext(notification.Context),
		notification.Type,
	)

	return html
}

// formatContext formats the notification context as HTML
func (ec *EmailClient) formatContext(context map[string]interface{}) string {
	if len(context) == 0 {
		return ""
	}

	html := `<div class="details"><h3>📊 Additional Details</h3><table>`

	for key, value := range context {
		if key == "test" {
			continue // Skip test flag
		}
		html += fmt.Sprintf("<tr><td>%s:</td><td>%v</td></tr>",
			strings.Title(strings.ReplaceAll(key, "_", " ")), value)
	}

	html += "</table></div>"
	return html
}

// getPriorityColor returns color for priority level
func (ec *EmailClient) getPriorityColor(priority int) string {
	switch priority {
	case PriorityEmergency:
		return "#dc3545" // Red
	case PriorityHigh:
		return "#fd7e14" // Orange
	case PriorityNormal:
		return "#007bff" // Blue
	case PriorityLow:
		return "#28a745" // Green
	case PriorityLowest:
		return "#6c757d" // Gray
	default:
		return "#007bff" // Blue
	}
}

// getPriorityText returns text for priority level
func (ec *EmailClient) getPriorityText(priority int) string {
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
