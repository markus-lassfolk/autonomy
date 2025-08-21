package main

import (
	"crypto/hmac"
	"crypto/sha256"
	"encoding/hex"
	"encoding/json"
	"fmt"
	"io"
	"log"
	"net/http"
	"os"
	"strings"
	"time"

	"github.com/gorilla/mux"
)

// WebhookPayload represents the structure of incoming webhook data
type WebhookPayload struct {
	Event     string                 `json:"event"`
	Timestamp time.Time              `json:"timestamp"`
	Source    string                 `json:"source"`
	Data      map[string]interface{} `json:"data"`
	Severity  string                 `json:"severity"`
	Message   string                 `json:"message"`
}

// GitHubIssue represents a GitHub issue to be created
type GitHubIssue struct {
	Title     string   `json:"title"`
	Body      string   `json:"body"`
	Labels    []string `json:"labels"`
	Assignees []string `json:"assignees"`
}

// Server configuration
type Config struct {
	Port         string
	WebhookSecret string
	GitHubToken   string
	GitHubRepo    string
	GitHubOwner   string
	RateLimit     int
	LogLevel      string
}

var config Config

func init() {
	// Load configuration from environment variables
	config = Config{
		Port:         getEnv("WEBHOOK_PORT", "8080"),
		WebhookSecret: getEnv("WEBHOOK_SECRET", ""),
		GitHubToken:   getEnv("GITHUB_TOKEN", ""),
		GitHubRepo:    getEnv("GITHUB_REPO", "autonomy"),
		GitHubOwner:   getEnv("GITHUB_OWNER", ""),
		RateLimit:     100, // requests per minute
		LogLevel:      getEnv("LOG_LEVEL", "info"),
	}

	// Validate required configuration
	if config.WebhookSecret == "" {
		log.Fatal("WEBHOOK_SECRET environment variable is required")
	}
	if config.GitHubToken == "" {
		log.Fatal("GITHUB_TOKEN environment variable is required")
	}
	if config.GitHubOwner == "" {
		log.Fatal("GITHUB_OWNER environment variable is required")
	}
}

func getEnv(key, defaultValue string) string {
	if value := os.Getenv(key); value != "" {
		return value
	}
	return defaultValue
}

// Rate limiting middleware
type RateLimiter struct {
	requests map[string][]time.Time
}

func NewRateLimiter() *RateLimiter {
	return &RateLimiter{
		requests: make(map[string][]time.Time),
	}
}

func (rl *RateLimiter) Middleware(next http.Handler) http.Handler {
	return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		clientIP := getClientIP(r)
		now := time.Now()
		
		// Clean old requests
		if requests, exists := rl.requests[clientIP]; exists {
			var validRequests []time.Time
			for _, reqTime := range requests {
				if now.Sub(reqTime) < time.Minute {
					validRequests = append(validRequests, reqTime)
				}
			}
			rl.requests[clientIP] = validRequests
		}

		// Check rate limit
		if len(rl.requests[clientIP]) >= config.RateLimit {
			http.Error(w, "Rate limit exceeded", http.StatusTooManyRequests)
			return
		}

		// Add current request
		rl.requests[clientIP] = append(rl.requests[clientIP], now)
		next.ServeHTTP(w, r)
	})
}

func getClientIP(r *http.Request) string {
	// Check for forwarded headers
	if forwarded := r.Header.Get("X-Forwarded-For"); forwarded != "" {
		return strings.Split(forwarded, ",")[0]
	}
	if realIP := r.Header.Get("X-Real-IP"); realIP != "" {
		return realIP
	}
	return r.RemoteAddr
}

// Verify webhook signature
func verifySignature(payload []byte, signature string) bool {
	if signature == "" {
		return false
	}

	// Remove "sha256=" prefix if present
	signature = strings.TrimPrefix(signature, "sha256=")

	// Create HMAC
	h := hmac.New(sha256.New, []byte(config.WebhookSecret))
	h.Write(payload)
	expectedSignature := hex.EncodeToString(h.Sum(nil))

	return hmac.Equal([]byte(signature), []byte(expectedSignature))
}

// Create GitHub issue
func createGitHubIssue(issue GitHubIssue) error {
	url := fmt.Sprintf("https://api.github.com/repos/%s/%s/issues", config.GitHubOwner, config.GitHubRepo)
	
	payload, err := json.Marshal(issue)
	if err != nil {
		return fmt.Errorf("failed to marshal issue: %w", err)
	}

	req, err := http.NewRequest("POST", url, strings.NewReader(string(payload)))
	if err != nil {
		return fmt.Errorf("failed to create request: %w", err)
	}

	req.Header.Set("Authorization", "token "+config.GitHubToken)
	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("User-Agent", "Autonomy-Webhook-Server")

	client := &http.Client{Timeout: 30 * time.Second}
	resp, err := client.Do(req)
	if err != nil {
		return fmt.Errorf("failed to send request: %w", err)
	}
	defer resp.Body.Close()

	if resp.StatusCode != http.StatusCreated {
		body, _ := io.ReadAll(resp.Body)
		return fmt.Errorf("GitHub API error: %d - %s", resp.StatusCode, string(body))
	}

	log.Printf("✅ Created GitHub issue: %s", issue.Title)
	return nil
}

// Webhook handler
func webhookHandler(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		http.Error(w, "Method not allowed", http.StatusMethodNotAllowed)
		return
	}

	// Read request body
	body, err := io.ReadAll(r.Body)
	if err != nil {
		log.Printf("❌ Failed to read request body: %v", err)
		http.Error(w, "Failed to read request body", http.StatusBadRequest)
		return
	}

	// Verify signature
	signature := r.Header.Get("X-Hub-Signature-256")
	if !verifySignature(body, signature) {
		log.Printf("❌ Invalid webhook signature from %s", getClientIP(r))
		http.Error(w, "Invalid signature", http.StatusUnauthorized)
		return
	}

	// Parse webhook payload
	var payload WebhookPayload
	if err := json.Unmarshal(body, &payload); err != nil {
		log.Printf("❌ Failed to parse webhook payload: %v", err)
		http.Error(w, "Invalid JSON payload", http.StatusBadRequest)
		return
	}

	log.Printf("📨 Received webhook: %s from %s", payload.Event, payload.Source)

	// Process webhook based on event type
	switch payload.Event {
	case "network_failure":
		handleNetworkFailure(payload)
	case "starlink_obstruction":
		handleStarlinkObstruction(payload)
	case "cellular_issue":
		handleCellularIssue(payload)
	case "system_alert":
		handleSystemAlert(payload)
	default:
		log.Printf("⚠️  Unknown event type: %s", payload.Event)
	}

	w.WriteHeader(http.StatusOK)
	w.Write([]byte(`{"status": "processed"}`))
}

func handleNetworkFailure(payload WebhookPayload) {
	issue := GitHubIssue{
		Title: fmt.Sprintf("🚨 Network Failure Detected - %s", payload.Source),
		Body: fmt.Sprintf(`## Network Failure Alert

**Source:** %s
**Severity:** %s
**Time:** %s

### Details
%s

### Data
%s

### Recommended Actions
1. Check network connectivity
2. Verify failover configuration
3. Review system logs
4. Test backup connections

---
*Auto-generated by Autonomy Webhook Server*`, 
			payload.Source, payload.Severity, payload.Timestamp.Format(time.RFC3339),
			payload.Message, formatData(payload.Data)),
		Labels: []string{"network-failure", "urgent", "autonomy"},
		Assignees: []string{},
	}

	if err := createGitHubIssue(issue); err != nil {
		log.Printf("❌ Failed to create GitHub issue: %v", err)
	}
}

func handleStarlinkObstruction(payload WebhookPayload) {
	issue := GitHubIssue{
		Title: fmt.Sprintf("🛰️ Starlink Obstruction Detected - %s", payload.Source),
		Body: fmt.Sprintf(`## Starlink Obstruction Alert

**Source:** %s
**Severity:** %s
**Time:** %s

### Details
%s

### Data
%s

### Recommended Actions
1. Check for physical obstructions
2. Verify dish positioning
3. Review obstruction history
4. Consider dish relocation

---
*Auto-generated by Autonomy Webhook Server*`,
			payload.Source, payload.Severity, payload.Timestamp.Format(time.RFC3339),
			payload.Message, formatData(payload.Data)),
		Labels: []string{"starlink", "obstruction", "autonomy"},
		Assignees: []string{},
	}

	if err := createGitHubIssue(issue); err != nil {
		log.Printf("❌ Failed to create GitHub issue: %v", err)
	}
}

func handleCellularIssue(payload WebhookPayload) {
	issue := GitHubIssue{
		Title: fmt.Sprintf("📱 Cellular Issue Detected - %s", payload.Source),
		Body: fmt.Sprintf(`## Cellular Issue Alert

**Source:** %s
**Severity:** %s
**Time:** %s

### Details
%s

### Data
%s

### Recommended Actions
1. Check SIM card status
2. Verify cellular signal strength
3. Review carrier settings
4. Test cellular connectivity

---
*Auto-generated by Autonomy Webhook Server*`,
			payload.Source, payload.Severity, payload.Timestamp.Format(time.RFC3339),
			payload.Message, formatData(payload.Data)),
		Labels: []string{"cellular", "autonomy"},
		Assignees: []string{},
	}

	if err := createGitHubIssue(issue); err != nil {
		log.Printf("❌ Failed to create GitHub issue: %v", err)
	}
}

func handleSystemAlert(payload WebhookPayload) {
	issue := GitHubIssue{
		Title: fmt.Sprintf("⚠️ System Alert - %s", payload.Source),
		Body: fmt.Sprintf(`## System Alert

**Source:** %s
**Severity:** %s
**Time:** %s

### Details
%s

### Data
%s

### Recommended Actions
1. Review system logs
2. Check resource usage
3. Verify configuration
4. Monitor system health

---
*Auto-generated by Autonomy Webhook Server*`,
			payload.Source, payload.Severity, payload.Timestamp.Format(time.RFC3339),
			payload.Message, formatData(payload.Data)),
		Labels: []string{"system-alert", "autonomy"},
		Assignees: []string{},
	}

	if err := createGitHubIssue(issue); err != nil {
		log.Printf("❌ Failed to create GitHub issue: %v", err)
	}
}

func formatData(data map[string]interface{}) string {
	if len(data) == 0 {
		return "No additional data"
	}

	formatted := "```json\n"
	jsonData, _ := json.MarshalIndent(data, "", "  ")
	formatted += string(jsonData)
	formatted += "\n```"
	return formatted
}

// Health check endpoint
func healthHandler(w http.ResponseWriter, r *http.Request) {
	w.Header().Set("Content-Type", "application/json")
	w.WriteHeader(http.StatusOK)
	w.Write([]byte(`{"status": "healthy", "timestamp": "` + time.Now().Format(time.RFC3339) + `"}`))
}

// Metrics endpoint
func metricsHandler(w http.ResponseWriter, r *http.Request) {
	w.Header().Set("Content-Type", "text/plain")
	w.WriteHeader(http.StatusOK)
	w.Write([]byte(`# HELP webhook_requests_total Total number of webhook requests
# TYPE webhook_requests_total counter
webhook_requests_total{status="processed"} 0
webhook_requests_total{status="failed"} 0
`))
}

func main() {
	log.Printf("🚀 Starting Autonomy Webhook Server on port %s", config.Port)
	log.Printf("📊 Rate limit: %d requests/minute", config.RateLimit)
	log.Printf("🔗 GitHub repo: %s/%s", config.GitHubOwner, config.GitHubRepo)

	// Create router
	r := mux.NewRouter()
	
	// Add rate limiting middleware
	rateLimiter := NewRateLimiter()
	r.Use(rateLimiter.Middleware)

	// Add routes
	r.HandleFunc("/webhook", webhookHandler).Methods("POST")
	r.HandleFunc("/health", healthHandler).Methods("GET")
	r.HandleFunc("/metrics", metricsHandler).Methods("GET")

	// Start server
	log.Printf("✅ Server ready to receive webhooks")
	log.Fatal(http.ListenAndServe(":"+config.Port, r))
}
