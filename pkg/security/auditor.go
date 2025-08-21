package security

import (
	"context"
	"crypto/rand"
	"crypto/sha256"
	"encoding/hex"
	"fmt"
	"net"
	"os"
	"sync"
	"time"

	"github.com/markus-lassfolk/autonomy/pkg/logx"
)

// Auditor provides security auditing and hardening
type Auditor struct {
	mu sync.RWMutex

	// Configuration
	enabled    bool
	auditLevel string

	// Dependencies
	logger *logx.Logger

	// Security state
	securityEvents []*SecurityEvent
	threatLevel    string
	lastAudit      time.Time

	// Access control
	allowedIPs     map[string]bool
	allowedUsers   map[string]bool
	blockedIPs     map[string]time.Time
	failedAttempts map[string]int

	// Request tracking for suspicious activity detection
	accessAttempts map[string][]time.Time

	// File integrity
	fileHashes    map[string]string
	criticalFiles []string

	// Network security
	allowedPorts map[int]bool
	blockedPorts map[int]bool

	// Audit configuration
	auditConfig *AuditConfig
}

// SecurityEvent represents a security event
type SecurityEvent struct {
	ID           string                 `json:"id"`
	Timestamp    time.Time              `json:"timestamp"`
	Level        string                 `json:"level"` // info, warning, error, critical
	Category     string                 `json:"category"`
	Source       string                 `json:"source"`
	Message      string                 `json:"message"`
	Details      map[string]interface{} `json:"details,omitempty"`
	IPAddress    string                 `json:"ip_address,omitempty"`
	UserAgent    string                 `json:"user_agent,omitempty"`
	Action       string                 `json:"action,omitempty"`
	Resource     string                 `json:"resource,omitempty"`
	RiskScore    int                    `json:"risk_score"`
	Acknowledged bool                   `json:"acknowledged"`
}

// AuditConfig represents audit configuration
type AuditConfig struct {
	Enabled           bool     `json:"enabled"`
	LogLevel          string   `json:"log_level"`
	MaxEvents         int      `json:"max_events"`
	RetentionDays     int      `json:"retention_days"`
	FileIntegrity     bool     `json:"file_integrity"`
	NetworkSecurity   bool     `json:"network_security"`
	AccessControl     bool     `json:"access_control"`
	ThreatDetection   bool     `json:"threat_detection"`
	CriticalFiles     []string `json:"critical_files"`
	AllowedIPs        []string `json:"allowed_ips"`
	BlockedIPs        []string `json:"blocked_ips"`
	AllowedPorts      []int    `json:"allowed_ports"`
	BlockedPorts      []int    `json:"blocked_ports"`
	MaxFailedAttempts int      `json:"max_failed_attempts"`
	BlockDuration     int      `json:"block_duration"`
}

// ThreatLevel represents the current threat level
type ThreatLevel struct {
	Level       string    `json:"level"` // low, medium, high, critical
	Score       int       `json:"score"`
	LastUpdate  time.Time `json:"last_update"`
	Description string    `json:"description"`
	Mitigations []string  `json:"mitigations"`
}

// FileIntegrityCheck represents a file integrity check
type FileIntegrityCheck struct {
	FilePath    string    `json:"file_path"`
	Hash        string    `json:"hash"`
	LastCheck   time.Time `json:"last_check"`
	Status      string    `json:"status"` // valid, modified, missing, error
	Description string    `json:"description"`
}

// NetworkSecurityCheck represents a network security check
type NetworkSecurityCheck struct {
	Port        int       `json:"port"`
	Protocol    string    `json:"protocol"`
	Status      string    `json:"status"` // open, closed, filtered, error
	Service     string    `json:"service,omitempty"`
	LastCheck   time.Time `json:"last_check"`
	RiskLevel   string    `json:"risk_level"`
	Description string    `json:"description"`
}

// NewAuditor creates a new security auditor
func NewAuditor(config *AuditConfig, logger *logx.Logger) *Auditor {
	auditor := &Auditor{
		enabled:        config.Enabled,
		auditLevel:     config.LogLevel,
		logger:         logger,
		securityEvents: make([]*SecurityEvent, 0),
		threatLevel:    "low",
		allowedIPs:     make(map[string]bool),
		allowedUsers:   make(map[string]bool),
		blockedIPs:     make(map[string]time.Time),
		failedAttempts: make(map[string]int),
		fileHashes:     make(map[string]string),
		criticalFiles:  config.CriticalFiles,
		allowedPorts:   make(map[int]bool),
		blockedPorts:   make(map[int]bool),
		auditConfig:    config,
		accessAttempts: make(map[string][]time.Time),
	}

	// Initialize access control lists
	auditor.initializeAccessControl(config)

	// Initialize file integrity monitoring
	if config.FileIntegrity {
		auditor.initializeFileIntegrity()
	}

	// Initialize network security
	if config.NetworkSecurity {
		auditor.initializeNetworkSecurity(config)
	}

	return auditor
}

// Start starts the security auditor
func (a *Auditor) Start(ctx context.Context) {
	if !a.enabled {
		return
	}

	a.logger.Info("Starting security auditor")

	// Start file integrity monitoring
	if a.auditConfig.FileIntegrity {
		go a.fileIntegrityLoop(ctx)
	}

	// Start network security monitoring
	if a.auditConfig.NetworkSecurity {
		go a.networkSecurityLoop(ctx)
	}

	// Start threat detection
	if a.auditConfig.ThreatDetection {
		go a.threatDetectionLoop(ctx)
	}

	// Start audit cleanup
	go a.auditCleanupLoop(ctx)
}

// Stop stops the security auditor
func (a *Auditor) Stop() {
	if !a.enabled {
		return
	}

	a.logger.Info("Stopping security auditor")
}

// LogSecurityEvent logs a security event
func (a *Auditor) LogSecurityEvent(level, category, source, message string, details map[string]interface{}) {
	if !a.enabled {
		return
	}

	event := &SecurityEvent{
		ID:           a.generateEventID(),
		Timestamp:    time.Now(),
		Level:        level,
		Category:     category,
		Source:       source,
		Message:      message,
		Details:      details,
		RiskScore:    a.calculateRiskScore(level, category, details),
		Acknowledged: false,
	}

	a.mu.Lock()
	a.securityEvents = append(a.securityEvents, event)
	a.mu.Unlock()

	// Log to logger
	switch level {
	case "critical":
		a.logger.Error("Security event", "event", event)
	case "error":
		a.logger.Error("Security event", "event", event)
	case "warning":
		a.logger.Warn("Security event", "event", event)
	default:
		a.logger.Info("Security event", "event", event)
	}

	// Update threat level
	a.updateThreatLevel(event)
}

// CheckAccess checks if access is allowed
func (a *Auditor) CheckAccess(ipAddress, userAgent, action, resource string) bool {
	if !a.enabled {
		return true
	}

	// Check if IP is blocked
	if a.isIPBlocked(ipAddress) {
		a.LogSecurityEvent("warning", "access_control", "auditor",
			"Blocked IP attempted access", map[string]interface{}{
				"ip_address": ipAddress,
				"action":     action,
				"resource":   resource,
			})
		return false
	}

	// Check if IP is allowed
	if !a.isIPAllowed(ipAddress) {
		a.LogSecurityEvent("error", "access_control", "auditor",
			"Unauthorized IP attempted access", map[string]interface{}{
				"ip_address": ipAddress,
				"action":     action,
				"resource":   resource,
			})
		a.recordFailedAttempt(ipAddress)
		return false
	}

	// Record access attempt for threat detection
	a.recordAccessAttempt(ipAddress)

	// Check for suspicious activity
	if a.isSuspiciousActivity(ipAddress, action, resource) {
		a.LogSecurityEvent("warning", "threat_detection", "auditor",
			"Suspicious activity detected", map[string]interface{}{
				"ip_address": ipAddress,
				"action":     action,
				"resource":   resource,
				"user_agent": userAgent,
			})
	}

	return true
}

// ValidateFileIntegrity validates file integrity
func (a *Auditor) ValidateFileIntegrity(filePath string) (*FileIntegrityCheck, error) {
	if !a.enabled || !a.auditConfig.FileIntegrity {
		return nil, fmt.Errorf("file integrity monitoring is disabled")
	}

	check := &FileIntegrityCheck{
		FilePath:  filePath,
		LastCheck: time.Now(),
	}

	// Check if file exists
	if _, err := os.Stat(filePath); os.IsNotExist(err) {
		check.Status = "missing"
		check.Description = "File does not exist"
		return check, nil
	}

	// Calculate file hash
	hash, err := a.calculateFileHash(filePath)
	if err != nil {
		check.Status = "error"
		check.Description = fmt.Sprintf("Failed to calculate hash: %v", err)
		return check, err
	}

	check.Hash = hash

	// Check against stored hash
	a.mu.RLock()
	storedHash, exists := a.fileHashes[filePath]
	a.mu.RUnlock()

	if !exists {
		// First time checking this file
		a.mu.Lock()
		a.fileHashes[filePath] = hash
		a.mu.Unlock()
		check.Status = "valid"
		check.Description = "File hash recorded for first time"
	} else if hash == storedHash {
		check.Status = "valid"
		check.Description = "File integrity verified"
	} else {
		check.Status = "modified"
		check.Description = "File has been modified"

		// Log security event
		a.LogSecurityEvent("error", "file_integrity", "auditor",
			"File integrity check failed", map[string]interface{}{
				"file_path": filePath,
				"expected":  storedHash,
				"actual":    hash,
			})
	}

	return check, nil
}

// CheckNetworkSecurity checks network security
func (a *Auditor) CheckNetworkSecurity(port int, protocol string) (*NetworkSecurityCheck, error) {
	if !a.enabled || !a.auditConfig.NetworkSecurity {
		return nil, fmt.Errorf("network security monitoring is disabled")
	}

	check := &NetworkSecurityCheck{
		Port:      port,
		Protocol:  protocol,
		LastCheck: time.Now(),
	}

	// Check if port is blocked
	if a.isPortBlocked(port) {
		check.Status = "blocked"
		check.RiskLevel = "high"
		check.Description = "Port is blocked by security policy"
		return check, nil
	}

	// Check if port is allowed
	if !a.isPortAllowed(port) {
		check.Status = "unauthorized"
		check.RiskLevel = "high"
		check.Description = "Port is not in allowed list"

		// Log security event
		a.LogSecurityEvent("warning", "network_security", "auditor",
			"Unauthorized port access", map[string]interface{}{
				"port":     port,
				"protocol": protocol,
			})
		return check, nil
	}

	// Test port connectivity
	conn, err := net.DialTimeout(protocol, fmt.Sprintf("localhost:%d", port), 5*time.Second)
	if err != nil {
		check.Status = "closed"
		check.RiskLevel = "low"
		check.Description = "Port is closed"
	} else {
		conn.Close()
		check.Status = "open"
		check.RiskLevel = "medium"
		check.Description = "Port is open"
		check.Service = a.identifyService(port)
	}

	return check, nil
}

// GetSecurityEvents returns security events
func (a *Auditor) GetSecurityEvents() []*SecurityEvent {
	a.mu.RLock()
	defer a.mu.RUnlock()

	events := make([]*SecurityEvent, len(a.securityEvents))
	copy(events, a.securityEvents)

	return events
}

// GetThreatLevel returns current threat level
func (a *Auditor) GetThreatLevel() *ThreatLevel {
	a.mu.RLock()
	defer a.mu.RUnlock()

	return &ThreatLevel{
		Level:       a.threatLevel,
		Score:       a.calculateThreatScore(),
		LastUpdate:  a.lastAudit,
		Description: a.getThreatDescription(),
		Mitigations: a.getThreatMitigations(),
	}
}

// AcknowledgeEvent acknowledges a security event
func (a *Auditor) AcknowledgeEvent(eventID string) error {
	a.mu.Lock()
	defer a.mu.Unlock()

	for _, event := range a.securityEvents {
		if event.ID == eventID {
			event.Acknowledged = true
			return nil
		}
	}

	return fmt.Errorf("event not found: %s", eventID)
}

// BlockIP blocks an IP address
func (a *Auditor) BlockIP(ipAddress string, duration time.Duration) {
	a.mu.Lock()
	defer a.mu.Unlock()

	a.blockedIPs[ipAddress] = time.Now().Add(duration)

	a.LogSecurityEvent("info", "access_control", "auditor",
		"IP address blocked", map[string]interface{}{
			"ip_address": ipAddress,
			"duration":   duration.String(),
		})
}

// UnblockIP unblocks an IP address
func (a *Auditor) UnblockIP(ipAddress string) {
	a.mu.Lock()
	defer a.mu.Unlock()

	delete(a.blockedIPs, ipAddress)

	a.LogSecurityEvent("info", "access_control", "auditor",
		"IP address unblocked", map[string]interface{}{
			"ip_address": ipAddress,
		})
}

// GenerateSecureToken generates a secure token
func (a *Auditor) GenerateSecureToken() (string, error) {
	bytes := make([]byte, 32)
	if _, err := rand.Read(bytes); err != nil {
		return "", err
	}
	return hex.EncodeToString(bytes), nil
}

// ValidateSecureToken validates a secure token
func (a *Auditor) ValidateSecureToken(token string) bool {
	// Basic token validation
	if len(token) != 64 { // 32 bytes = 64 hex characters
		return false
	}

	// Check if token contains only hex characters
	for _, char := range token {
		if !((char >= '0' && char <= '9') || (char >= 'a' && char <= 'f') || (char >= 'A' && char <= 'F')) {
			return false
		}
	}

	return true
}

// fileIntegrityLoop runs the file integrity monitoring loop
func (a *Auditor) fileIntegrityLoop(ctx context.Context) {
	ticker := time.NewTicker(5 * time.Minute)
	defer ticker.Stop()

	for {
		select {
		case <-ctx.Done():
			return
		case <-ticker.C:
			a.checkFileIntegrity()
		}
	}
}

// networkSecurityLoop runs the network security monitoring loop
func (a *Auditor) networkSecurityLoop(ctx context.Context) {
	ticker := time.NewTicker(10 * time.Minute)
	defer ticker.Stop()

	for {
		select {
		case <-ctx.Done():
			return
		case <-ticker.C:
			a.checkNetworkSecurity()
		}
	}
}

// threatDetectionLoop runs the threat detection loop
func (a *Auditor) threatDetectionLoop(ctx context.Context) {
	ticker := time.NewTicker(1 * time.Minute)
	defer ticker.Stop()

	for {
		select {
		case <-ctx.Done():
			return
		case <-ticker.C:
			a.detectThreats()
		}
	}
}

// auditCleanupLoop runs the audit cleanup loop
func (a *Auditor) auditCleanupLoop(ctx context.Context) {
	ticker := time.NewTicker(1 * time.Hour)
	defer ticker.Stop()

	for {
		select {
		case <-ctx.Done():
			return
		case <-ticker.C:
			a.cleanupAuditLogs()
		}
	}
}

// initializeAccessControl initializes access control lists
func (a *Auditor) initializeAccessControl(config *AuditConfig) {
	for _, ip := range config.AllowedIPs {
		a.allowedIPs[ip] = true
	}

	for _, ip := range config.BlockedIPs {
		a.blockedIPs[ip] = time.Now().Add(24 * time.Hour)
	}
}

// initializeFileIntegrity initializes file integrity monitoring
func (a *Auditor) initializeFileIntegrity() {
	for _, filePath := range a.criticalFiles {
		if hash, err := a.calculateFileHash(filePath); err == nil {
			a.fileHashes[filePath] = hash
		}
	}
}

// initializeNetworkSecurity initializes network security monitoring
func (a *Auditor) initializeNetworkSecurity(config *AuditConfig) {
	for _, port := range config.AllowedPorts {
		a.allowedPorts[port] = true
	}

	for _, port := range config.BlockedPorts {
		a.blockedPorts[port] = true
	}
}

// generateEventID generates a unique event ID
func (a *Auditor) generateEventID() string {
	bytes := make([]byte, 16)
	if _, err := rand.Read(bytes); err != nil {
		// Fallback to timestamp-based ID if random generation fails
		return fmt.Sprintf("event_%d", time.Now().UnixNano())
	}
	return hex.EncodeToString(bytes)
}

// calculateRiskScore calculates the risk score for an event
func (a *Auditor) calculateRiskScore(level, category string, details map[string]interface{}) int {
	score := 0

	// Base score by level
	switch level {
	case "critical":
		score += 100
	case "error":
		score += 75
	case "warning":
		score += 50
	case "info":
		score += 25
	}

	// Category modifiers
	switch category {
	case "access_control":
		score += 25
	case "file_integrity":
		score += 30
	case "network_security":
		score += 20
	case "threat_detection":
		score += 35
	}

	return score
}

// updateThreatLevel updates the threat level based on events
func (a *Auditor) updateThreatLevel(event *SecurityEvent) {
	a.mu.Lock()
	defer a.mu.Unlock()

	// Calculate new threat level based on recent events
	recentEvents := 0
	highRiskEvents := 0

	for _, e := range a.securityEvents {
		if time.Since(e.Timestamp) < time.Hour {
			recentEvents++
			if e.RiskScore > 75 {
				highRiskEvents++
			}
		}
	}

	// Update threat level
	if highRiskEvents > 5 {
		a.threatLevel = "critical"
	} else if highRiskEvents > 2 {
		a.threatLevel = "high"
	} else if recentEvents > 10 {
		a.threatLevel = "medium"
	} else {
		a.threatLevel = "low"
	}

	a.lastAudit = time.Now()
}

// isIPBlocked checks if an IP is blocked
func (a *Auditor) isIPBlocked(ipAddress string) bool {
	a.mu.RLock()
	defer a.mu.RUnlock()

	blockTime, exists := a.blockedIPs[ipAddress]
	if !exists {
		return false
	}

	// Check if block has expired
	if time.Now().After(blockTime) {
		delete(a.blockedIPs, ipAddress)
		return false
	}

	return true
}

// isIPAllowed checks if an IP is allowed
func (a *Auditor) isIPAllowed(ipAddress string) bool {
	a.mu.RLock()
	defer a.mu.RUnlock()

	// If no allowed IPs are configured, allow all
	if len(a.allowedIPs) == 0 {
		return true
	}

	return a.allowedIPs[ipAddress]
}

// isPortBlocked checks if a port is blocked
func (a *Auditor) isPortBlocked(port int) bool {
	a.mu.RLock()
	defer a.mu.RUnlock()

	return a.blockedPorts[port]
}

// isPortAllowed checks if a port is allowed
func (a *Auditor) isPortAllowed(port int) bool {
	a.mu.RLock()
	defer a.mu.RUnlock()

	// If no allowed ports are configured, allow all
	if len(a.allowedPorts) == 0 {
		return true
	}

	return a.allowedPorts[port]
}

// recordFailedAttempt records a failed access attempt
func (a *Auditor) recordFailedAttempt(ipAddress string) {
	a.mu.Lock()
	defer a.mu.Unlock()

	a.failedAttempts[ipAddress]++

	// Block IP if too many failed attempts
	if a.failedAttempts[ipAddress] >= a.auditConfig.MaxFailedAttempts {
		a.blockedIPs[ipAddress] = time.Now().Add(time.Duration(a.auditConfig.BlockDuration) * time.Hour)

		a.LogSecurityEvent("error", "access_control", "auditor",
			"IP blocked due to failed attempts", map[string]interface{}{
				"ip_address": ipAddress,
				"attempts":   a.failedAttempts[ipAddress],
			})
	}
}

// isSuspiciousActivity checks for suspicious activity
func (a *Auditor) isSuspiciousActivity(ipAddress, action, resource string) bool {
	a.mu.RLock()
	attempts := a.accessAttempts[ipAddress]
	a.mu.RUnlock()

	cutoff := time.Now().Add(-1 * time.Minute)
	count := 0
	for i := len(attempts) - 1; i >= 0; i-- {
		if attempts[i].After(cutoff) {
			count++
		} else {
			break
		}
	}

	// Consider more than 20 requests in a minute suspicious
	return count > 20
}

// recordAccessAttempt records an access attempt for threat detection
func (a *Auditor) recordAccessAttempt(ipAddress string) {
	a.mu.Lock()
	defer a.mu.Unlock()

	now := time.Now()
	attempts := append(a.accessAttempts[ipAddress], now)

	cutoff := now.Add(-1 * time.Minute)
	idx := 0
	for ; idx < len(attempts); idx++ {
		if attempts[idx].After(cutoff) {
			break
		}
	}
	a.accessAttempts[ipAddress] = attempts[idx:]
}

// calculateFileHash calculates the SHA256 hash of a file
func (a *Auditor) calculateFileHash(filePath string) (string, error) {
	data, err := os.ReadFile(filePath)
	if err != nil {
		return "", err
	}

	hash := sha256.Sum256(data)
	return hex.EncodeToString(hash[:]), nil
}

// identifyService identifies the service running on a port
func (a *Auditor) identifyService(port int) string {
	services := map[int]string{
		22:   "SSH",
		23:   "Telnet",
		25:   "SMTP",
		53:   "DNS",
		80:   "HTTP",
		443:  "HTTPS",
		8080: "HTTP-Alt",
		9090: "HTTP-Alt",
	}

	if service, exists := services[port]; exists {
		return service
	}

	return "Unknown"
}

// checkFileIntegrity checks integrity of critical files
func (a *Auditor) checkFileIntegrity() {
	for _, filePath := range a.criticalFiles {
		if _, err := a.ValidateFileIntegrity(filePath); err != nil {
			a.logger.Error("File integrity check failed", "file", filePath, "error", err)
		}
	}
}

// checkNetworkSecurity checks network security
func (a *Auditor) checkNetworkSecurity() {
	// Check common ports
	commonPorts := []int{22, 23, 25, 53, 80, 443, 8080, 9090}

	for _, port := range commonPorts {
		if _, err := a.CheckNetworkSecurity(port, "tcp"); err != nil {
			a.logger.Error("Network security check failed", "port", port, "error", err)
		}
	}
}

// detectThreats detects potential threats using comprehensive analysis
func (a *Auditor) detectThreats() {
	a.mu.RLock()
	events := make([]*SecurityEvent, len(a.securityEvents))
	copy(events, a.securityEvents)
	a.mu.RUnlock()

	// Time windows for analysis
	lastHour := time.Now().Add(-time.Hour)
	last24Hours := time.Now().Add(-24 * time.Hour)
	lastWeek := time.Now().Add(-7 * 24 * time.Hour)

	// Counters for different threat types
	recentEvents := 0
	failedLogins := 0
	suspiciousNetworkActivity := 0
	fileIntegrityViolations := 0
	bruteForceAttempts := make(map[string]int)
	suspiciousIPs := make(map[string]int)
	portScans := make(map[string][]int)

	// Analyze security events
	for _, event := range events {
		if event.Timestamp.After(lastHour) {
			recentEvents++
		}

		if event.Timestamp.After(last24Hours) {
			switch event.Category {
			case "authentication":
				if event.Level == "error" || event.Level == "warning" {
					failedLogins++
					if event.IPAddress != "" {
						bruteForceAttempts[event.IPAddress]++
					}
				}
			case "network":
				if event.Level == "warning" || event.Level == "error" {
					suspiciousNetworkActivity++
					if event.IPAddress != "" {
						suspiciousIPs[event.IPAddress]++
					}

					// Detect port scanning patterns
					if port, ok := event.Details["port"].(int); ok {
						portScans[event.IPAddress] = append(portScans[event.IPAddress], port)
					}
				}
			case "file_integrity":
				if event.Level == "error" || event.Level == "critical" {
					fileIntegrityViolations++
				}
			}
		}
	}

	// Detect brute force attacks
	for ip, attempts := range bruteForceAttempts {
		if attempts > 10 { // More than 10 failed attempts in 24 hours
			a.LogSecurityEvent("critical", "threat_detection", "auditor",
				"Brute force attack detected", map[string]interface{}{
					"ip_address":      ip,
					"failed_attempts": attempts,
					"detection_type":  "brute_force",
				})

			// Auto-block IP for brute force
			a.BlockIP(ip, 24*time.Hour)
		} else if attempts > 5 {
			a.LogSecurityEvent("warning", "threat_detection", "auditor",
				"Suspicious login activity detected", map[string]interface{}{
					"ip_address":      ip,
					"failed_attempts": attempts,
					"detection_type":  "suspicious_login",
				})
		}
	}

	// Detect port scanning
	for ip, ports := range portScans {
		if len(ports) > 5 { // Scanning more than 5 ports
			a.LogSecurityEvent("warning", "threat_detection", "auditor",
				"Port scanning detected", map[string]interface{}{
					"ip_address":     ip,
					"scanned_ports":  ports,
					"port_count":     len(ports),
					"detection_type": "port_scan",
				})
		}
	}

	// Detect DoS/DDoS patterns
	if recentEvents > 50 { // More than 50 events in last hour
		a.LogSecurityEvent("critical", "threat_detection", "auditor",
			"Potential DoS/DDoS attack detected", map[string]interface{}{
				"event_count":    recentEvents,
				"time_window":    "1_hour",
				"detection_type": "dos_attack",
			})
	} else if recentEvents > 20 {
		a.LogSecurityEvent("warning", "threat_detection", "auditor",
			"High security event rate detected", map[string]interface{}{
				"event_count":    recentEvents,
				"time_window":    "1_hour",
				"detection_type": "high_activity",
			})
	}

	// Detect coordinated attacks (multiple suspicious IPs)
	if len(suspiciousIPs) > 3 {
		suspiciousIPList := make([]string, 0, len(suspiciousIPs))
		for ip := range suspiciousIPs {
			suspiciousIPList = append(suspiciousIPList, ip)
		}

		a.LogSecurityEvent("critical", "threat_detection", "auditor",
			"Coordinated attack detected", map[string]interface{}{
				"suspicious_ips": suspiciousIPList,
				"ip_count":       len(suspiciousIPs),
				"detection_type": "coordinated_attack",
			})
	}

	// File integrity threat detection
	if fileIntegrityViolations > 0 {
		level := "warning"
		if fileIntegrityViolations > 3 {
			level = "critical"
		}

		a.LogSecurityEvent(level, "threat_detection", "auditor",
			"File integrity violations detected", map[string]interface{}{
				"violation_count": fileIntegrityViolations,
				"detection_type":  "file_integrity_threat",
			})
	}

	// Detect unusual network patterns
	if suspiciousNetworkActivity > 10 {
		a.LogSecurityEvent("warning", "threat_detection", "auditor",
			"Unusual network activity detected", map[string]interface{}{
				"activity_count": suspiciousNetworkActivity,
				"detection_type": "network_anomaly",
			})
	}

	// Advanced pattern analysis
	a.detectAdvancedThreats(events, last24Hours, lastWeek)
}

// detectAdvancedThreats performs advanced threat pattern analysis
func (a *Auditor) detectAdvancedThreats(events []*SecurityEvent, last24Hours, lastWeek time.Time) {
	// Time-based pattern analysis
	hourlyEventCounts := make(map[int]int)
	categoryPatterns := make(map[string][]time.Time)

	for _, event := range events {
		if event.Timestamp.After(last24Hours) {
			hour := event.Timestamp.Hour()
			hourlyEventCounts[hour]++
			categoryPatterns[event.Category] = append(categoryPatterns[event.Category], event.Timestamp)
		}
	}

	// Detect unusual time patterns (e.g., activity during off-hours)
	offHourEvents := hourlyEventCounts[0] + hourlyEventCounts[1] + hourlyEventCounts[2] +
		hourlyEventCounts[3] + hourlyEventCounts[4] + hourlyEventCounts[5]

	if offHourEvents > 20 { // High activity during 00:00-06:00
		a.LogSecurityEvent("warning", "threat_detection", "auditor",
			"Unusual off-hours activity detected", map[string]interface{}{
				"off_hour_events": offHourEvents,
				"detection_type":  "temporal_anomaly",
			})
	}

	// Detect rapid-fire events (potential automation)
	for category, timestamps := range categoryPatterns {
		if len(timestamps) > 10 {
			// Check for events within very short time windows
			rapidEvents := 0
			for i := 1; i < len(timestamps); i++ {
				if timestamps[i].Sub(timestamps[i-1]) < 5*time.Second {
					rapidEvents++
				}
			}

			if rapidEvents > 5 {
				a.LogSecurityEvent("warning", "threat_detection", "auditor",
					"Automated attack pattern detected", map[string]interface{}{
						"category":       category,
						"rapid_events":   rapidEvents,
						"total_events":   len(timestamps),
						"detection_type": "automation_pattern",
					})
			}
		}
	}

	// Baseline comparison (compare with previous week)
	weeklyBaseline := 0
	for _, event := range events {
		if event.Timestamp.After(lastWeek) && event.Timestamp.Before(last24Hours) {
			weeklyBaseline++
		}
	}

	currentDayEvents := 0
	for _, event := range events {
		if event.Timestamp.After(last24Hours) {
			currentDayEvents++
		}
	}

	// If current day events are significantly higher than baseline
	if weeklyBaseline > 0 && float64(currentDayEvents) > float64(weeklyBaseline)*2.5 {
		a.LogSecurityEvent("warning", "threat_detection", "auditor",
			"Anomalous activity spike detected", map[string]interface{}{
				"current_events":  currentDayEvents,
				"baseline_events": weeklyBaseline / 7, // Daily average
				"spike_ratio":     float64(currentDayEvents) / (float64(weeklyBaseline) / 7),
				"detection_type":  "activity_spike",
			})
	}
}

// cleanupAuditLogs cleans up old audit logs
func (a *Auditor) cleanupAuditLogs() {
	a.mu.Lock()
	defer a.mu.Unlock()

	cutoff := time.Now().Add(-time.Duration(a.auditConfig.RetentionDays) * 24 * time.Hour)

	var newEvents []*SecurityEvent
	for _, event := range a.securityEvents {
		if event.Timestamp.After(cutoff) {
			newEvents = append(newEvents, event)
		}
	}

	a.securityEvents = newEvents

	// Trim to max events
	if len(a.securityEvents) > a.auditConfig.MaxEvents {
		a.securityEvents = a.securityEvents[len(a.securityEvents)-a.auditConfig.MaxEvents:]
	}
}

// calculateThreatScore calculates the current threat score
func (a *Auditor) calculateThreatScore() int {
	score := 0

	for _, event := range a.securityEvents {
		if time.Since(event.Timestamp) < 24*time.Hour {
			score += event.RiskScore
		}
	}

	return score
}

// getThreatDescription gets the threat level description
func (a *Auditor) getThreatDescription() string {
	switch a.threatLevel {
	case "critical":
		return "Critical security threats detected. Immediate action required."
	case "high":
		return "High security threats detected. Enhanced monitoring recommended."
	case "medium":
		return "Medium security threats detected. Standard monitoring active."
	default:
		return "Low security threats. Normal operation."
	}
}

// getThreatMitigations gets recommended mitigations
func (a *Auditor) getThreatMitigations() []string {
	var mitigations []string

	switch a.threatLevel {
	case "critical":
		mitigations = append(mitigations,
			"Immediately review all security events",
			"Block suspicious IP addresses",
			"Check file integrity",
			"Review network access logs",
			"Consider system lockdown")
	case "high":
		mitigations = append(mitigations,
			"Review recent security events",
			"Monitor network traffic",
			"Check for unauthorized access",
			"Update security policies")
	case "medium":
		mitigations = append(mitigations,
			"Monitor security events",
			"Review access logs",
			"Check system integrity")
	default:
		mitigations = append(mitigations,
			"Continue normal monitoring",
			"Maintain security policies")
	}

	return mitigations
}
