package main

import (
	"context"
	"crypto/hmac"
	"crypto/sha256"
	"encoding/hex"
	"encoding/json"
	"fmt"
	"log"
	"net/http"
	"os"
	"strconv"
	"strings"
	"time"

	"github.com/google/go-github/v57/github"
	"golang.org/x/oauth2"
)

// WebhookPayload represents the payload from autonomy clients
type WebhookPayload struct {
	DeviceID   string   `json:"device_id"`
	Firmware   string   `json:"fw"`
	Severity   string   `json:"severity"`
	Scenario   string   `json:"scenario"`
	Note       string   `json:"note"`
	OverlayPct int      `json:"overlay_pct"`
	MemAvailMB int      `json:"mem_avail_mb"`
	Load1      float64  `json:"load1"`
	UbusOK     bool     `json:"ubus_ok"`
	Actions    []string `json:"actions"`
	Timestamp  int64    `json:"ts"`
	AlertID    string   `json:"alert_id,omitempty"`
	IssueKey   string   `json:"issue_key,omitempty"`
}

// Configuration holds server configuration
type Configuration struct {
	Port              string
	WebhookSecret     string
	GitHubToken       string
	GitHubOwner       string
	GitHubRepo        string
	SupportedVersions []string
	MinSeverity       string
	CopilotEnabled    bool
	AutoAssign        bool
	RateLimitPerMin   int
	RateLimitPerHour  int
}

// Server holds the webhook server state
type Server struct {
	config     *Configuration
	github     *github.Client
	rateLimits map[string][]time.Time
}

// NewServer creates a new webhook server
func NewServer(config *Configuration) *Server {
	ctx := context.Background()
	ts := oauth2.StaticTokenSource(
		&oauth2.Token{AccessToken: config.GitHubToken},
	)
	tc := oauth2.NewClient(ctx, ts)
	client := github.NewClient(tc)

	return &Server{
		config:     config,
		github:     client,
		rateLimits: make(map[string][]time.Time),
	}
}

// validateHMAC validates the HMAC signature
func (s *Server) validateHMAC(payload []byte, signature string) bool {
	if !strings.HasPrefix(signature, "sha256=") {
		return false
	}
	signature = signature[7:] // Remove "sha256=" prefix

	mac := hmac.New(sha256.New, []byte(s.config.WebhookSecret))
	mac.Write(payload)
	expected := hex.EncodeToString(mac.Sum(nil))

	return hmac.Equal([]byte(signature), []byte(expected))
}

// isSupportedVersion checks if the firmware version is supported
func (s *Server) isSupportedVersion(fw string) bool {
	for _, version := range s.config.SupportedVersions {
		if strings.HasPrefix(fw, version) {
			return true
		}
	}
	return false
}

// isCodeIssue determines if this is a code issue vs configuration issue
func (s *Server) isCodeIssue(scenario, note string) bool {
	configErrors := []string{
		"configuration_error",
		"user_misconfiguration",
		"network_setup_error",
		"config_error",
	}

	systemIssues := []string{
		"daemon_down",
		"daemon_hung",
		"crash_loop",
		"system_degraded",
		"performance_issue",
		"memory_leak",
	}

	// Check if it's a system-level issue
	for _, issue := range systemIssues {
		if strings.Contains(strings.ToLower(scenario), issue) {
			return true
		}
	}

	// Check if it's a configuration error
	for _, error := range configErrors {
		if strings.Contains(strings.ToLower(note), error) {
			return false
		}
	}

	return true
}

// generateIssueKey creates a unique key for deduplication
func (s *Server) generateIssueKey(payload *WebhookPayload) string {
	return fmt.Sprintf("%s-%s-%s", payload.DeviceID, payload.Scenario, payload.Firmware)
}

// searchExistingIssue searches for existing issues with the same key
func (s *Server) searchExistingIssue(issueKey string) (*github.Issue, error) {
	query := fmt.Sprintf("repo:%s/%s \"%s\" is:issue is:open", s.config.GitHubOwner, s.config.GitHubRepo, issueKey)

	ctx := context.Background()
	issues, _, err := s.github.Search.Issues(ctx, query, &github.SearchOptions{
		Sort:  "created",
		Order: "desc",
		ListOptions: github.ListOptions{
			PerPage: 10,
		},
	})

	if err != nil {
		return nil, err
	}

	if len(issues.Issues) > 0 {
		return &issues.Issues[0], nil
	}

	return nil, nil
}

// createGitHubIssue creates a new GitHub issue
func (s *Server) createGitHubIssue(payload *WebhookPayload) (*github.Issue, error) {
	title := fmt.Sprintf("[%s] %s - %s", strings.ToUpper(payload.Severity), payload.Scenario, payload.DeviceID)

	body := fmt.Sprintf(`## autonomy Alert Report

**Device**: %s
**Firmware**: %s
**Severity**: %s
**Scenario**: %s
**Timestamp**: %s

### System Status
- **Overlay Usage**: %d%%
- **Available Memory**: %d MB
- **Load Average**: %.2f
- **Ubus Status**: %t

### Actions Taken
%s

### Description
%s

### Diagnostic Information
This issue was automatically created by the autonomy webhook server.
- **Issue Key**: %s
- **Alert ID**: %s
- **Webhook Server**: Go-based server

### Next Steps
1. Review system logs for additional context
2. Check if this is a known issue
3. Verify system configuration
4. Consider firmware update if applicable

---
*This issue was automatically generated by the autonomy monitoring system.*`,
		payload.DeviceID,
		payload.Firmware,
		payload.Severity,
		payload.Scenario,
		time.Unix(payload.Timestamp, 0).Format(time.RFC3339),
		payload.OverlayPct,
		payload.MemAvailMB,
		payload.Load1,
		payload.UbusOK,
		strings.Join(payload.Actions, ", "),
		payload.Note,
		s.generateIssueKey(payload),
		payload.AlertID,
	)

	labels := []string{
		"autonomy-alert",
		"auto-generated",
		fmt.Sprintf("severity-%s", payload.Severity),
		fmt.Sprintf("scenario-%s", payload.Scenario),
	}

	if s.config.CopilotEnabled {
		labels = append(labels, "copilot-assign")
	}

	issue := &github.IssueRequest{
		Title:  &title,
		Body:   &body,
		Labels: &labels,
	}

	if s.config.AutoAssign {
		assignees := []string{"github-copilot"}
		issue.Assignees = &assignees
	}

	ctx := context.Background()
	return s.github.Issues.Create(ctx, s.config.GitHubOwner, s.config.GitHubRepo, issue)
}

// checkRateLimit checks if the request is within rate limits
func (s *Server) checkRateLimit(deviceID string) bool {
	now := time.Now()
	key := deviceID

	// Clean old entries
	if times, exists := s.rateLimits[key]; exists {
		var valid []time.Time
		for _, t := range times {
			if now.Sub(t) < time.Hour {
				valid = append(valid, t)
			}
		}
		s.rateLimits[key] = valid
	}

	// Check per-minute limit
	minuteAgo := now.Add(-time.Minute)
	minuteCount := 0
	for _, t := range s.rateLimits[key] {
		if t.After(minuteAgo) {
			minuteCount++
		}
	}

	if minuteCount >= s.config.RateLimitPerMin {
		return false
	}

	// Check per-hour limit
	hourAgo := now.Add(-time.Hour)
	hourCount := 0
	for _, t := range s.rateLimits[key] {
		if t.After(hourAgo) {
			hourCount++
		}
	}

	if hourCount >= s.config.RateLimitPerHour {
		return false
	}

	// Add current request
	s.rateLimits[key] = append(s.rateLimits[key], now)
	return true
}

// handleWebhook processes incoming webhook requests
func (s *Server) handleWebhook(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		http.Error(w, "Method not allowed", http.StatusMethodNotAllowed)
		return
	}

	// Read request body
	var payload WebhookPayload
	if err := json.NewDecoder(r.Body).Decode(&payload); err != nil {
		http.Error(w, "Invalid JSON", http.StatusBadRequest)
		return
	}

	// Validate HMAC signature
	signature := r.Header.Get("X-Starwatch-Signature")
	if !s.validateHMAC([]byte(fmt.Sprintf("%+v", payload)), signature) {
		http.Error(w, "Invalid signature", http.StatusUnauthorized)
		return
	}

	// Check rate limits
	if !s.checkRateLimit(payload.DeviceID) {
		http.Error(w, "Rate limit exceeded", http.StatusTooManyRequests)
		return
	}

	// Version filtering
	if !s.isSupportedVersion(payload.Firmware) {
		log.Printf("Skipping issue creation - unsupported firmware version: %s", payload.Firmware)
		w.WriteHeader(http.StatusOK)
		json.NewEncoder(w).Encode(map[string]string{"status": "skipped", "reason": "unsupported_version"})
		return
	}

	// Severity filtering
	severityLevels := map[string]int{"info": 1, "warn": 2, "critical": 3}
	minSeverityLevel := severityLevels[s.config.MinSeverity]
	if severityLevels[payload.Severity] < minSeverityLevel {
		log.Printf("Skipping issue creation - severity below threshold: %s", payload.Severity)
		w.WriteHeader(http.StatusOK)
		json.NewEncoder(w).Encode(map[string]string{"status": "skipped", "reason": "severity_filtered"})
		return
	}

	// Configuration vs code issue filtering
	if !s.isCodeIssue(payload.Scenario, payload.Note) {
		log.Printf("Skipping issue creation - not a code issue: %s", payload.Scenario)
		w.WriteHeader(http.StatusOK)
		json.NewEncoder(w).Encode(map[string]string{"status": "skipped", "reason": "not_code_issue"})
		return
	}

	// Check for existing issue
	issueKey := s.generateIssueKey(&payload)
	existingIssue, err := s.searchExistingIssue(issueKey)
	if err != nil {
		log.Printf("Error searching for existing issue: %v", err)
		http.Error(w, "Internal server error", http.StatusInternalServerError)
		return
	}

	if existingIssue != nil {
		log.Printf("Issue already exists: #%d", *existingIssue.Number)
		w.WriteHeader(http.StatusOK)
		json.NewEncoder(w).Encode(map[string]interface{}{
			"status":       "skipped",
			"reason":       "duplicate_issue",
			"issue_number": *existingIssue.Number,
		})
		return
	}

	// Create new issue
	issue, err := s.createGitHubIssue(&payload)
	if err != nil {
		log.Printf("Error creating GitHub issue: %v", err)
		http.Error(w, "Internal server error", http.StatusInternalServerError)
		return
	}

	log.Printf("Created issue #%d for device %s", *issue.Number, payload.DeviceID)
	w.WriteHeader(http.StatusOK)
	json.NewEncoder(w).Encode(map[string]interface{}{
		"status":       "created",
		"issue_number": *issue.Number,
		"issue_url":    *issue.HTMLURL,
	})
}

// healthCheck provides a health check endpoint
func (s *Server) healthCheck(w http.ResponseWriter, r *http.Request) {
	w.Header().Set("Content-Type", "application/json")
	json.NewEncoder(w).Encode(map[string]interface{}{
		"status":    "healthy",
		"timestamp": time.Now().Unix(),
		"version":   "1.0.0",
	})
}

func main() {
	// Load configuration from environment
	config := &Configuration{
		Port:              getEnv("PORT", "8080"),
		WebhookSecret:     getEnv("WEBHOOK_SECRET", ""),
		GitHubToken:       getEnv("GITHUB_TOKEN", ""),
		GitHubOwner:       getEnv("GITHUB_OWNER", ""),
		GitHubRepo:        getEnv("GITHUB_REPO", ""),
		SupportedVersions: strings.Split(getEnv("SUPPORTED_VERSIONS", "RUTX_R_00.07.17,RUTX_R_00.07.18,RUTX_R_00.08.00"), ","),
		MinSeverity:       getEnv("MIN_SEVERITY", "warn"),
		CopilotEnabled:    getEnv("COPILOT_ENABLED", "true") == "true",
		AutoAssign:        getEnv("AUTO_ASSIGN", "true") == "true",
		RateLimitPerMin:   getEnvAsInt("RATE_LIMIT_PER_MIN", 10),
		RateLimitPerHour:  getEnvAsInt("RATE_LIMIT_PER_HOUR", 100),
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
	if config.GitHubRepo == "" {
		log.Fatal("GITHUB_REPO environment variable is required")
	}

	// Create server
	server := NewServer(config)

	// Setup routes
	http.HandleFunc("/webhook/starwatch", server.handleWebhook)
	http.HandleFunc("/health", server.healthCheck)

	// Start server
	log.Printf("Starting webhook server on port %s", config.Port)
	log.Fatal(http.ListenAndServe(":"+config.Port, nil))
}

// Helper functions
func getEnv(key, defaultValue string) string {
	if value := os.Getenv(key); value != "" {
		return value
	}
	return defaultValue
}

func getEnvAsInt(key string, defaultValue int) int {
	if value := os.Getenv(key); value != "" {
		if intValue, err := strconv.Atoi(value); err == nil {
			return intValue
		}
	}
	return defaultValue
}
