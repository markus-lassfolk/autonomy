//go:build !ci
// +build !ci

package security

import (
	"context"
	"os"
	"path/filepath"
	"testing"
	"time"

	"github.com/markus-lassfolk/autonomy/pkg/logx"
)

func TestAuditor_NewAuditor(t *testing.T) {
	tests := []struct {
		name   string
		config *AuditConfig
		logger *logx.Logger
	}{
		{
			name: "valid config",
			config: &AuditConfig{
				Enabled:         true,
				FileIntegrity:   true,
				NetworkSecurity: true,
				ThreatDetection: true,
				AccessControl:   true,
				RetentionDays:   30,
				MaxEvents:       1000,
				AllowedIPs:      []string{"127.0.0.1", "192.168.1.0/24"},
				AllowedPorts:    []int{22, 80, 443},
				BlockedPorts:    []int{23, 135, 139},
			},
			logger: logx.NewLogger("security_test", "debug"),
		},
		{
			name: "minimal config",
			config: &AuditConfig{
				Enabled: true,
			},
			logger: logx.NewLogger("security_test", "info"),
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			auditor := NewAuditor(tt.config, tt.logger)

			if auditor == nil {
				t.Error("NewAuditor() returned nil")
				return
			}

			if auditor.enabled != tt.config.Enabled {
				t.Error("NewAuditor() enabled flag not set correctly")
			}
		})
	}
}

func TestAuditor_LogSecurityEvent(t *testing.T) {
	config := &AuditConfig{
		Enabled:   true,
		MaxEvents: 100,
	}
	logger := logx.NewLogger("security_test", "debug")

	auditor := NewAuditor(config, logger)

	tests := []struct {
		name     string
		level    string
		category string
		source   string
		message  string
		details  map[string]interface{}
	}{
		{
			name:     "authentication event",
			level:    "warning",
			category: "authentication",
			source:   "ssh",
			message:  "Failed login attempt",
			details: map[string]interface{}{
				"ip_address": "192.168.1.100",
				"username":   "admin",
				"attempts":   3,
			},
		},
		{
			name:     "network event",
			level:    "error",
			category: "network",
			source:   "firewall",
			message:  "Blocked connection attempt",
			details: map[string]interface{}{
				"ip_address": "10.0.0.50",
				"port":       23,
				"protocol":   "tcp",
			},
		},
		{
			name:     "file integrity event",
			level:    "critical",
			category: "file_integrity",
			source:   "auditor",
			message:  "Critical file modified",
			details: map[string]interface{}{
				"file_path": "/etc/passwd",
				"old_hash":  "abc123",
				"new_hash":  "def456",
			},
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			// Log the event
			auditor.LogSecurityEvent(tt.level, tt.category, tt.source, tt.message, tt.details)

			// Verify event was stored
			events := auditor.GetSecurityEvents()
			if len(events) == 0 {
				t.Error("LogSecurityEvent() event not stored")
				return
			}

			// Find our event
			var found *SecurityEvent
			for _, event := range events {
				if event.Message == tt.message {
					found = event
					break
				}
			}

			if found == nil {
				t.Error("LogSecurityEvent() event not found in stored events")
				return
			}

			// Verify event details
			if found.Level != tt.level {
				t.Errorf("LogSecurityEvent() level = %v, want %v", found.Level, tt.level)
			}

			if found.Category != tt.category {
				t.Errorf("LogSecurityEvent() category = %v, want %v", found.Category, tt.category)
			}

			if found.Source != tt.source {
				t.Errorf("LogSecurityEvent() source = %v, want %v", found.Source, tt.source)
			}

			if found.Message != tt.message {
				t.Errorf("LogSecurityEvent() message = %v, want %v", found.Message, tt.message)
			}
		})
	}
}

func TestAuditor_CheckAccess(t *testing.T) {
	config := &AuditConfig{
		Enabled:       true,
		AccessControl: true,
		AllowedIPs:    []string{"127.0.0.1", "192.168.1.0/24"},
		BlockedPorts:  []int{23, 135},
	}
	logger := logx.NewLogger("security_test", "debug")

	auditor := NewAuditor(config, logger)

	tests := []struct {
		name        string
		ipAddress   string
		userAgent   string
		action      string
		resource    string
		wantAllowed bool
	}{
		{
			name:        "allowed IP",
			ipAddress:   "127.0.0.1",
			userAgent:   "test-client/1.0",
			action:      "read",
			resource:    "/api/status",
			wantAllowed: true,
		},
		{
			name:        "allowed IP range",
			ipAddress:   "192.168.1.100",
			userAgent:   "test-client/1.0",
			action:      "read",
			resource:    "/api/status",
			wantAllowed: true,
		},
		{
			name:        "blocked IP",
			ipAddress:   "10.0.0.1",
			userAgent:   "malicious-client/1.0",
			action:      "write",
			resource:    "/api/config",
			wantAllowed: false,
		},
		{
			name:        "empty IP",
			ipAddress:   "",
			userAgent:   "test-client/1.0",
			action:      "read",
			resource:    "/api/status",
			wantAllowed: false,
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			allowed := auditor.CheckAccess(tt.ipAddress, tt.userAgent, tt.action, tt.resource)

			if allowed != tt.wantAllowed {
				t.Errorf("CheckAccess() = %v, want %v", allowed, tt.wantAllowed)
			}
		})
	}
}

func TestAuditor_ValidateFileIntegrity(t *testing.T) {
	config := &AuditConfig{
		Enabled:       true,
		FileIntegrity: true,
	}
	logger := logx.NewLogger("security_test", "debug")

	auditor := NewAuditor(config, logger)

	// Create a temporary test file
	tempDir := t.TempDir()
	testFile := filepath.Join(tempDir, "test_file.txt")
	testContent := "This is a test file for integrity checking"

	if err := os.WriteFile(testFile, []byte(testContent), 0o644); err != nil {
		t.Fatalf("Failed to create test file: %v", err)
	}

	tests := []struct {
		name       string
		filePath   string
		wantErr    bool
		wantExists bool
	}{
		{
			name:       "existing file",
			filePath:   testFile,
			wantErr:    false,
			wantExists: true,
		},
		{
			name:       "non-existent file",
			filePath:   filepath.Join(tempDir, "nonexistent.txt"),
			wantErr:    true,
			wantExists: false,
		},
		{
			name:       "empty path",
			filePath:   "",
			wantErr:    true,
			wantExists: false,
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			result, err := auditor.ValidateFileIntegrity(tt.filePath)

			if (err != nil) != tt.wantErr {
				t.Errorf("ValidateFileIntegrity() error = %v, wantErr %v", err, tt.wantErr)
				return
			}

			if tt.wantExists && result == nil {
				t.Error("ValidateFileIntegrity() expected result but got nil")
				return
			}

			if !tt.wantExists && result != nil {
				t.Error("ValidateFileIntegrity() expected no result but got some")
				return
			}

			if tt.wantExists && result != nil {
				if result.FilePath != tt.filePath {
					t.Errorf("ValidateFileIntegrity() filePath = %v, want %v", result.FilePath, tt.filePath)
				}

				if result.Hash == "" {
					t.Error("ValidateFileIntegrity() hash is empty")
				}

				if result.Status == "" {
					t.Error("ValidateFileIntegrity() status should not be empty")
				}
			}
		})
	}
}

func TestAuditor_CheckNetworkSecurity(t *testing.T) {
	config := &AuditConfig{
		Enabled:         true,
		NetworkSecurity: true,
		AllowedPorts:    []int{22, 80, 443},
		BlockedPorts:    []int{23, 135, 139},
	}
	logger := logx.NewLogger("security_test", "debug")

	auditor := NewAuditor(config, logger)

	tests := []struct {
		name     string
		port     int
		protocol string
		wantErr  bool
	}{
		{
			name:     "allowed port SSH",
			port:     22,
			protocol: "tcp",
			wantErr:  false,
		},
		{
			name:     "allowed port HTTP",
			port:     80,
			protocol: "tcp",
			wantErr:  false,
		},
		{
			name:     "blocked port Telnet",
			port:     23,
			protocol: "tcp",
			wantErr:  false, // Not an error, just blocked
		},
		{
			name:     "unknown port",
			port:     9999,
			protocol: "tcp",
			wantErr:  false,
		},
		{
			name:     "invalid port",
			port:     -1,
			protocol: "tcp",
			wantErr:  true,
		},
		{
			name:     "port too high",
			port:     70000,
			protocol: "tcp",
			wantErr:  true,
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			result, err := auditor.CheckNetworkSecurity(tt.port, tt.protocol)

			if (err != nil) != tt.wantErr {
				t.Errorf("CheckNetworkSecurity() error = %v, wantErr %v", err, tt.wantErr)
				return
			}

			if !tt.wantErr && result == nil {
				t.Error("CheckNetworkSecurity() expected result but got nil")
				return
			}

			if tt.wantErr && result != nil {
				t.Error("CheckNetworkSecurity() expected no result but got some")
				return
			}

			if result != nil {
				if result.Port != tt.port {
					t.Errorf("CheckNetworkSecurity() port = %v, want %v", result.Port, tt.port)
				}

				if result.Protocol != tt.protocol {
					t.Errorf("CheckNetworkSecurity() protocol = %v, want %v", result.Protocol, tt.protocol)
				}
			}
		})
	}
}

func TestAuditor_ThreatDetection(t *testing.T) {
	config := &AuditConfig{
		Enabled:         true,
		ThreatDetection: true,
		MaxEvents:       1000,
	}
	logger := logx.NewLogger("security_test", "debug")

	auditor := NewAuditor(config, logger)

	// Simulate brute force attack
	t.Run("brute force detection", func(t *testing.T) {
		attackerIP := "10.0.0.100"

		// Generate multiple failed login events
		for i := 0; i < 12; i++ {
			auditor.LogSecurityEvent("error", "authentication", "ssh", "Failed login attempt", map[string]interface{}{
				"ip_address": attackerIP,
				"username":   "admin",
				"port":       22,
			})
		}

		// Run threat detection
		auditor.detectThreats()

		// Check if brute force was detected
		events := auditor.GetSecurityEvents()
		foundBruteForce := false
		for _, event := range events {
			if event.Category == "threat_detection" && event.Message == "Brute force attack detected" {
				if ipAddr, ok := event.Details["ip_address"].(string); ok && ipAddr == attackerIP {
					foundBruteForce = true
					break
				}
			}
		}

		if !foundBruteForce {
			t.Error("Brute force attack not detected")
		}
	})

	// Simulate port scanning
	t.Run("port scan detection", func(t *testing.T) {
		scannerIP := "10.0.0.200"

		// Generate port scan events
		ports := []int{22, 23, 80, 443, 8080, 9090}
		for _, port := range ports {
			auditor.LogSecurityEvent("warning", "network", "firewall", "Connection attempt", map[string]interface{}{
				"ip_address": scannerIP,
				"port":       port,
				"protocol":   "tcp",
			})
		}

		// Run threat detection
		auditor.detectThreats()

		// Check if port scan was detected
		events := auditor.GetSecurityEvents()
		foundPortScan := false
		for _, event := range events {
			if event.Category == "threat_detection" && event.Message == "Port scanning detected" {
				if ipAddr, ok := event.Details["ip_address"].(string); ok && ipAddr == scannerIP {
					foundPortScan = true
					break
				}
			}
		}

		if !foundPortScan {
			t.Error("Port scanning not detected")
		}
	})

	// Simulate DoS attack
	t.Run("DoS detection", func(t *testing.T) {
		// Clear previous events
		auditor.securityEvents = []*SecurityEvent{}

		// Generate high volume of events in short time
		for i := 0; i < 60; i++ {
			auditor.LogSecurityEvent("warning", "network", "firewall", "High traffic", map[string]interface{}{
				"ip_address": "10.0.0.300",
				"requests":   100,
			})
		}

		// Run threat detection
		auditor.detectThreats()

		// Check if DoS was detected
		events := auditor.GetSecurityEvents()
		foundDoS := false
		for _, event := range events {
			if event.Category == "threat_detection" && event.Message == "Potential DoS/DDoS attack detected" {
				foundDoS = true
				break
			}
		}

		if !foundDoS {
			t.Error("DoS attack not detected")
		}
	})
}

func TestAuditor_BlockIP(t *testing.T) {
	config := &AuditConfig{
		Enabled:       true,
		AccessControl: true,
	}
	logger := logx.NewLogger("security_test", "debug")

	auditor := NewAuditor(config, logger)

	testIP := "10.0.0.100"
	duration := time.Hour

	// Block IP
	auditor.BlockIP(testIP, duration)

	// Verify IP is blocked
	allowed := auditor.CheckAccess(testIP, "test-agent", "read", "/api/status")
	if allowed {
		t.Error("BlockIP() IP should be blocked but access was allowed")
	}

	// Unblock IP
	auditor.UnblockIP(testIP)

	// Verify IP is unblocked (should still be blocked due to not being in allowed list)
	// But the blocked list should be cleared
	auditor.allowedIPs[testIP] = true // Add to allowed list for test
	allowed = auditor.CheckAccess(testIP, "test-agent", "read", "/api/status")
	if !allowed {
		t.Error("UnblockIP() IP should be unblocked but access was denied")
	}
}

func TestAuditor_GetThreatLevel(t *testing.T) {
	config := &AuditConfig{
		Enabled: true,
	}
	logger := logx.NewLogger("security_test", "debug")

	auditor := NewAuditor(config, logger)

	// Initially should be low
	threatLevel := auditor.GetThreatLevel()
	if threatLevel == nil {
		t.Error("GetThreatLevel() returned nil")
		return
	}

	if threatLevel.Level != "low" {
		t.Errorf("GetThreatLevel() initial level = %v, want low", threatLevel.Level)
	}

	// Add some critical events
	for i := 0; i < 5; i++ {
		auditor.LogSecurityEvent("critical", "threat_detection", "auditor", "Critical threat detected", map[string]interface{}{
			"threat_type": "brute_force",
		})
	}

	// Check threat level again
	threatLevel = auditor.GetThreatLevel()
	if threatLevel.Level == "low" {
		t.Error("GetThreatLevel() should have increased after critical events")
	}
}

func TestAuditor_GenerateSecureToken(t *testing.T) {
	config := &AuditConfig{
		Enabled: true,
	}
	logger := logx.NewLogger("security_test", "debug")

	auditor := NewAuditor(config, logger)

	// Generate multiple tokens
	tokens := make([]string, 10)
	for i := 0; i < 10; i++ {
		token, err := auditor.GenerateSecureToken()
		if err != nil {
			t.Errorf("GenerateSecureToken() error = %v", err)
		}

		if len(token) != 64 { // 32 bytes = 64 hex characters
			t.Errorf("GenerateSecureToken() token length = %d, want 64", len(token))
		}

		tokens[i] = token
	}

	// Verify tokens are unique
	for i := 0; i < len(tokens); i++ {
		for j := i + 1; j < len(tokens); j++ {
			if tokens[i] == tokens[j] {
				t.Error("GenerateSecureToken() generated duplicate tokens")
			}
		}
	}

	// Test token validation
	for _, token := range tokens {
		if !auditor.ValidateSecureToken(token) {
			t.Errorf("ValidateSecureToken() failed for valid token: %s", token)
		}
	}

	// Test invalid tokens
	invalidTokens := []string{
		"",
		"short",
		"invalid_characters!@#$",
		"too_long_token_with_more_than_64_characters_should_fail_validation_test",
	}

	for _, token := range invalidTokens {
		if auditor.ValidateSecureToken(token) {
			t.Errorf("ValidateSecureToken() should have failed for invalid token: %s", token)
		}
	}
}

func TestAuditor_Integration_FullWorkflow(t *testing.T) {
	config := &AuditConfig{
		Enabled:         true,
		FileIntegrity:   true,
		NetworkSecurity: true,
		ThreatDetection: true,
		AccessControl:   true,
		MaxEvents:       1000,
		RetentionDays:   1,
		AllowedIPs:      []string{"127.0.0.1"},
		AllowedPorts:    []int{22, 80, 443},
		BlockedPorts:    []int{23, 135},
	}
	logger := logx.NewLogger("security_integration", "debug")

	auditor := NewAuditor(config, logger)

	ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancel()

	// Start the auditor
	auditor.Start(ctx)

	// Simulate real-world security events
	events := []struct {
		level    string
		category string
		source   string
		message  string
		details  map[string]interface{}
	}{
		{
			level:    "info",
			category: "authentication",
			source:   "ssh",
			message:  "Successful login",
			details:  map[string]interface{}{"ip_address": "127.0.0.1", "username": "admin"},
		},
		{
			level:    "warning",
			category: "authentication",
			source:   "ssh",
			message:  "Failed login attempt",
			details:  map[string]interface{}{"ip_address": "10.0.0.100", "username": "root"},
		},
		{
			level:    "error",
			category: "network",
			source:   "firewall",
			message:  "Blocked connection",
			details:  map[string]interface{}{"ip_address": "10.0.0.100", "port": 23},
		},
		{
			level:    "critical",
			category: "file_integrity",
			source:   "auditor",
			message:  "Critical file modified",
			details:  map[string]interface{}{"file_path": "/etc/passwd"},
		},
	}

	// Log events
	for _, event := range events {
		auditor.LogSecurityEvent(event.level, event.category, event.source, event.message, event.details)
	}

	// Test access control
	allowed := auditor.CheckAccess("127.0.0.1", "test-agent", "read", "/api/status")
	if !allowed {
		t.Error("Access should be allowed for whitelisted IP")
	}

	blocked := auditor.CheckAccess("10.0.0.100", "malicious-agent", "write", "/api/config")
	if blocked {
		t.Error("Access should be blocked for non-whitelisted IP")
	}

	// Get security events
	securityEvents := auditor.GetSecurityEvents()
	if len(securityEvents) < len(events) {
		t.Errorf("Expected at least %d security events, got %d", len(events), len(securityEvents))
	}

	// Get threat level
	threatLevel := auditor.GetThreatLevel()
	if threatLevel == nil {
		t.Error("GetThreatLevel() returned nil")
	}

	// Stop the auditor
	auditor.Stop()

	// Wait a moment for cleanup
	time.Sleep(100 * time.Millisecond)
}

// Benchmark tests
func BenchmarkAuditor_LogSecurityEvent(b *testing.B) {
	config := &AuditConfig{
		Enabled:   true,
		MaxEvents: 10000,
	}
	logger := logx.NewLogger("security_bench", "error")

	auditor := NewAuditor(config, logger)

	details := map[string]interface{}{
		"ip_address": "192.168.1.100",
		"username":   "testuser",
		"attempts":   3,
	}

	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		auditor.LogSecurityEvent("warning", "authentication", "ssh", "Failed login", details)
	}
}

func BenchmarkAuditor_CheckAccess(b *testing.B) {
	config := &AuditConfig{
		Enabled:       true,
		AccessControl: true,
		AllowedIPs:    []string{"127.0.0.1", "192.168.1.0/24"},
	}
	logger := logx.NewLogger("security_bench", "error")

	auditor := NewAuditor(config, logger)

	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		auditor.CheckAccess("192.168.1.100", "test-agent", "read", "/api/status")
	}
}

func BenchmarkAuditor_ThreatDetection(b *testing.B) {
	config := &AuditConfig{
		Enabled:         true,
		ThreatDetection: true,
		MaxEvents:       1000,
	}
	logger := logx.NewLogger("security_bench", "error")

	auditor := NewAuditor(config, logger)

	// Pre-populate with events
	for i := 0; i < 100; i++ {
		auditor.LogSecurityEvent("warning", "authentication", "ssh", "Failed login", map[string]interface{}{
			"ip_address": "10.0.0.100",
		})
	}

	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		auditor.detectThreats()
	}
}
