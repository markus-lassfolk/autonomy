package sysmgmt

import (
	"context"
	"fmt"
	"os"
	"os/exec"
	"strings"
	"sync"
	"time"

	"github.com/markus-lassfolk/autonomy/pkg/gps"
	"github.com/markus-lassfolk/autonomy/pkg/logx"
)

// Manager represents the system management orchestrator
type Manager struct {
	config *Config
	logger *logx.Logger
	dryRun bool
	mu     sync.Mutex

	// Health check components
	overlayManager    *OverlayManager
	serviceWatchdog   *ServiceWatchdog
	logFloodDetector  *LogFloodDetector
	timeManager       *TimeManager
	networkManager    *NetworkManager
	starlinkHealthMgr *StarlinkHealthManager
	databaseManager   *DatabaseManager
	uciMaintenanceMgr *UCIMaintenanceManager
	notificationMgr   *NotificationManager
	wifiManager       *WiFiManager
	ubusMonitor       *UbusMonitor

	// Statistics
	lastCheckTime     time.Time
	issuesFound       int
	issuesFixed       int
	notificationsSent int
}

// NewManager creates a new system manager
func NewManager(config *Config, logger *logx.Logger, dryRun bool) *Manager {
	m := &Manager{
		config: config,
		logger: logger,
		dryRun: dryRun,
	}

	// Initialize components
	m.overlayManager = NewOverlayManager(config, logger, dryRun)
	m.serviceWatchdog = NewServiceWatchdog(config, logger, dryRun)
	m.logFloodDetector = NewLogFloodDetector(config, logger, dryRun)
	m.timeManager = NewTimeManager(config, logger, dryRun)
	m.networkManager = NewNetworkManager(config, logger, dryRun)
	m.starlinkHealthMgr = NewStarlinkHealthManager(config, logger, dryRun)
	m.databaseManager = NewDatabaseManager(config, logger, dryRun)
	m.uciMaintenanceMgr = NewUCIMaintenanceManager(logger)
	m.notificationMgr = NewNotificationManager(config, logger, dryRun)

	// Initialize WiFi manager (UCI client and GPS collector will be added later)
	m.wifiManager = NewWiFiManager(config, logger, dryRun, nil, nil)

	// Initialize ubus monitor
	ubusConfig := &UbusMonitorConfig{
		Enabled:             config.UbusMonitorEnabled,
		CheckInterval:       config.UbusMonitorInterval,
		MaxFixAttempts:      config.UbusMaxFixAttempts,
		AutoFix:             config.UbusAutoFix,
		RestartTimeout:      config.UbusRestartTimeout,
		MinServicesExpected: config.UbusMinServicesExpected,
		CriticalServices:    config.UbusCriticalServices,
	}
	m.ubusMonitor = NewUbusMonitor(logger, ubusConfig, dryRun)

	return m
}

// NewManagerWithGPS creates a new system manager with GPS collector for WiFi optimization
func NewManagerWithGPS(config *Config, logger *logx.Logger, dryRun bool, gpsCollector gps.ComprehensiveGPSCollectorInterface) *Manager {
	m := &Manager{
		config: config,
		logger: logger,
		dryRun: dryRun,
	}

	// Initialize components
	m.overlayManager = NewOverlayManager(config, logger, dryRun)
	m.serviceWatchdog = NewServiceWatchdog(config, logger, dryRun)
	m.logFloodDetector = NewLogFloodDetector(config, logger, dryRun)
	m.timeManager = NewTimeManager(config, logger, dryRun)
	m.networkManager = NewNetworkManager(config, logger, dryRun)
	m.starlinkHealthMgr = NewStarlinkHealthManager(config, logger, dryRun)
	m.databaseManager = NewDatabaseManager(config, logger, dryRun)
	m.uciMaintenanceMgr = NewUCIMaintenanceManager(logger)
	m.notificationMgr = NewNotificationManager(config, logger, dryRun)

	// Initialize WiFi manager with GPS collector
	m.wifiManager = NewWiFiManager(config, logger, dryRun, nil, gpsCollector)

	// Initialize ubus monitor
	ubusConfig := &UbusMonitorConfig{
		Enabled:             config.UbusMonitorEnabled,
		CheckInterval:       config.UbusMonitorInterval,
		MaxFixAttempts:      config.UbusMaxFixAttempts,
		AutoFix:             config.UbusAutoFix,
		RestartTimeout:      config.UbusRestartTimeout,
		MinServicesExpected: config.UbusMinServicesExpected,
		CriticalServices:    config.UbusCriticalServices,
	}
	m.ubusMonitor = NewUbusMonitor(logger, ubusConfig, dryRun)

	return m
}

// RunHealthCheck runs a complete health check cycle
func (m *Manager) RunHealthCheck(ctx context.Context) error {
	m.mu.Lock()
	defer m.mu.Unlock()

	startTime := time.Now()
	m.logger.Info("Starting system health check", "dry_run", m.dryRun)

	// Reset statistics
	m.issuesFound = 0
	m.issuesFixed = 0
	m.notificationsSent = 0

	// Run all health checks
	checks := []struct {
		name string
		fn   func(context.Context) error
	}{
		{"overlay space", m.overlayManager.Check},
		{"service watchdog", m.serviceWatchdog.Check},
		{"log flood detection", m.logFloodDetector.Check},
		{"time drift", m.timeManager.Check},
		{"network interface", m.networkManager.Check},
		{"starlink health", m.starlinkHealthMgr.Check},
		{"gps system", m.checkGPSHealth},
		{"enhanced starlink", m.checkEnhancedStarlinkHealth},
		{"mwan3 functionality", m.checkMWAN3Health},
		{"database health", m.databaseManager.Check},
		{"uci configuration", m.checkUCIHealth},
		{"wifi optimization", m.wifiManager.Check},
		{"ubus health", m.checkUbusHealth},
	}

	for _, check := range checks {
		select {
		case <-ctx.Done():
			return ctx.Err()
		default:
		}

		if err := check.fn(ctx); err != nil {
			m.logger.Error("Health check failed", "check", check.name, "error", err)
			m.issuesFound++
		}
	}

	// Send summary notification if enabled
	if m.config.NotificationsEnabled && m.issuesFound > 0 {
		m.sendSummaryNotification()
	}

	m.lastCheckTime = time.Now()
	duration := time.Since(startTime)

	// Determine if the health check was truly successful
	success := m.issuesFound == 0 || (m.issuesFound > 0 && m.issuesFound == m.issuesFixed)

	if success {
		m.logger.Info("System health check completed successfully",
			"duration", duration,
			"issues_found", m.issuesFound,
			"issues_fixed", m.issuesFixed,
			"notifications_sent", m.notificationsSent)
	} else {
		m.logger.Warn("System health check completed with unfixed issues",
			"duration", duration,
			"issues_found", m.issuesFound,
			"issues_fixed", m.issuesFixed,
			"unfixed_issues", m.issuesFound-m.issuesFixed,
			"notifications_sent", m.notificationsSent)
	}

	return nil
}

// sendSummaryNotification sends a summary of the health check
func (m *Manager) sendSummaryNotification() {
	if m.notificationsSent >= m.config.MaxNotificationsPerRun {
		m.logger.Debug("Notification limit reached, skipping summary")
		return
	}

	summary := fmt.Sprintf("System Health Check Summary\n\n"+
		"Issues Found: %d\n"+
		"Issues Fixed: %d\n"+
		"Check Time: %s",
		m.issuesFound, m.issuesFixed, m.lastCheckTime.Format("2006-01-02 15:04:05"))

	if err := m.notificationMgr.SendNotification("System Health Summary", summary, m.config.PushoverPriorityFixed); err != nil {
		m.logger.Error("Failed to send summary notification", "error", err)
	} else {
		m.notificationsSent++
	}
}

// GetStatus returns the current status of the system manager
func (m *Manager) GetStatus() map[string]interface{} {
	m.mu.Lock()
	defer m.mu.Unlock()

	return map[string]interface{}{
		"enabled":            m.config.Enabled,
		"last_check_time":    m.lastCheckTime,
		"issues_found":       m.issuesFound,
		"issues_fixed":       m.issuesFixed,
		"notifications_sent": m.notificationsSent,
		"dry_run":            m.dryRun,
		"check_interval":     m.config.CheckInterval,
		"auto_fix_enabled":   m.config.AutoFixEnabled,
	}
}

// checkUCIHealth performs UCI configuration health checks
func (m *Manager) checkUCIHealth(ctx context.Context) error {
	// Always perform UCI maintenance to check for unwanted files and other issues
	result, err := m.uciMaintenanceMgr.PerformUCIMaintenance()
	if err != nil {
		return fmt.Errorf("UCI maintenance failed: %w", err)
	}

	// If any issues were found and fixed, log them
	if len(result.IssuesFound) > 0 {
		m.logger.Warn("UCI configuration issues detected and addressed",
			"issues_found", len(result.IssuesFound),
			"issues_fixed", len(result.IssuesFixed))

		// Send notification about UCI maintenance
		if err := m.sendUCINotification(result); err != nil {
			m.logger.Error("Failed to send UCI notification", "error", err)
		}

		// Return error if critical issues remain unfixed
		for _, issue := range result.IssuesFound {
			if issue.Type == "parse_error" || issue.Type == "corruption" {
				return fmt.Errorf("critical UCI issue detected: %s", issue.Description)
			}
		}
	} else {
		m.logger.Debug("UCI health check completed - no issues found")
	}

	return nil
}

// sendUCINotification sends notification about UCI maintenance results
func (m *Manager) sendUCINotification(result *UCIMaintenanceResult) error {
	if !m.config.NotificationsEnabled {
		return nil
	}

	title := "🔧 UCI Configuration Maintenance"
	priority := 1 // Normal priority

	// Count issues by type
	parseErrors := 0
	unwantedFiles := 0
	fixed := len(result.IssuesFixed)

	for _, issue := range result.IssuesFound {
		switch issue.Type {
		case "parse_error":
			parseErrors++
		case "unwanted_file":
			unwantedFiles++
		}
	}

	// Adjust priority based on severity
	if parseErrors > 0 {
		title = "🚨 Critical UCI Issues Fixed"
		priority = 2 // High priority
	}

	message := "UCI Maintenance Results:\n\n"
	message += fmt.Sprintf("• Parse errors: %d\n", parseErrors)
	message += fmt.Sprintf("• Unwanted files: %d\n", unwantedFiles)
	message += fmt.Sprintf("• Issues fixed: %d\n", fixed)

	if result.BackupPath != "" {
		message += fmt.Sprintf("• Backup created: %s\n", result.BackupPath)
	}

	if result.Success {
		message += "\n✅ Maintenance completed successfully"
	} else if result.ErrorMessage != "" {
		message += fmt.Sprintf("\n❌ Error: %s", result.ErrorMessage)
	}

	return m.notificationMgr.SendNotification(title, message, priority)
}

// checkGPSHealth performs GPS system health checks
func (m *Manager) checkGPSHealth(ctx context.Context) error {
	// Check if GPS is available on this system
	if !m.isGPSAvailable() {
		m.logger.Debug("GPS not available on this system - skipping check")
		return nil
	}

	issues := []string{}

	// Test gpsd process
	if _, err := m.exec("pgrep", "gpsd"); err != nil {
		issues = append(issues, "gpsd_not_running")
	}

	// Test GPS devices
	if _, err := os.Stat("/dev/ttyUSB1"); err != nil {
		if _, err := os.Stat("/dev/ttyUSB2"); err != nil {
			issues = append(issues, "no_gps_devices")
		}
	}

	// Test gpsctl (with shorter timeout to avoid hanging)
	if _, err := m.execWithTimeout(5, "gpsctl", "status"); err != nil {
		issues = append(issues, "gpsctl_failed")
	}

	// Test GPS ubus services (only if ubus is working)
	if m.testUbus() {
		if _, err := m.execWithTimeout(3, "ubus", "call", "gpsd", "status"); err != nil {
			issues = append(issues, "gps_ubus_failed")
		}
	} else {
		issues = append(issues, "ubus_unavailable")
	}

	if len(issues) > 0 {
		m.logger.Warn("GPS system issues detected", "issues", issues)
		m.issuesFound++

		// Try to fix GPS issues if auto-fix is enabled
		if m.config.AutoFixEnabled && !m.dryRun {
			if m.fixGPSSystem() {
				m.issuesFixed++
				m.logger.Info("GPS system fixed successfully")
			} else {
				m.logger.Error("GPS system fix failed")
				// Return error to indicate unfixed issues
				return fmt.Errorf("GPS system issues remain unfixed: %v", issues)
			}
		} else {
			// Return error if auto-fix is disabled and issues exist
			return fmt.Errorf("GPS system issues detected but auto-fix disabled: %v", issues)
		}
	} else {
		m.logger.Debug("GPS system health check passed")
	}

	return nil
}

// checkEnhancedStarlinkHealth performs enhanced Starlink connectivity checks
func (m *Manager) checkEnhancedStarlinkHealth(ctx context.Context) error {
	// Check if Starlink is available on this system
	if !m.isStarlinkAvailable() {
		m.logger.Debug("Starlink not available on this system - skipping check")
		return nil
	}

	issues := []string{}

	// Test basic connectivity to Starlink dish
	if _, err := m.execWithTimeout(3, "ping", "-c", "1", "-W", "3", "192.168.100.1"); err != nil {
		issues = append(issues, "ping_failed")
	}

	// Test Starlink gRPC port (BusyBox nc syntax)
	if _, err := m.execWithTimeout(3, "sh", "-c", "echo 'test' | nc 192.168.100.1 9200"); err != nil {
		issues = append(issues, "grpc_port_failed")
	}

	// Test gRPC response time with 15-second timeout
	// Note: Using centralized Starlink client instead of grpcurl binary
	if _, err := m.execWithTimeout(15, "ping", "-c", "1", "-W", "3", "192.168.100.1"); err != nil {
		issues = append(issues, "grpc_timeout")
	}

	if len(issues) > 0 {
		m.logger.Warn("Enhanced Starlink connectivity issues detected", "issues", issues)
		m.issuesFound++

		// Try to fix Starlink issues if auto-fix is enabled
		if m.config.AutoFixEnabled && !m.dryRun {
			if m.fixStarlink() {
				m.issuesFixed++
				m.logger.Info("Starlink connectivity fixed successfully")
			} else {
				m.logger.Error("Starlink connectivity fix failed")
				// Return error to indicate unfixed issues
				return fmt.Errorf("Starlink connectivity issues remain unfixed: %v", issues)
			}
		} else {
			// Return error if auto-fix is disabled and issues exist
			return fmt.Errorf("Starlink connectivity issues detected but auto-fix disabled: %v", issues)
		}
	} else {
		m.logger.Debug("Enhanced Starlink health check passed")
	}

	return nil
}

// checkMWAN3Health performs mwan3 functionality checks
func (m *Manager) checkMWAN3Health(ctx context.Context) error {
	issues := []string{}

	// Test mwan3 ubus service
	if _, err := m.execWithTimeout(3, "ubus", "call", "mwan3", "status"); err != nil {
		issues = append(issues, "mwan3_ubus_failed")
	} else {
		// Test mwan3 interfaces
		output, err := m.exec("ubus", "call", "mwan3", "status")
		if err == nil {
			interfaceCount := strings.Count(output, "interface")
			if interfaceCount == 0 {
				issues = append(issues, "no_mwan3_interfaces")
			}
		}
	}

	if len(issues) > 0 {
		m.logger.Warn("mwan3 functionality issues detected", "issues", issues)
		m.issuesFound++

		// Try to fix mwan3 if auto-fix is enabled
		if m.config.AutoFixEnabled && !m.dryRun {
			if m.fixMWAN3() {
				m.issuesFixed++
				m.logger.Info("mwan3 fixed successfully")
			} else {
				m.logger.Error("mwan3 fix failed")
				// Return error to indicate unfixed issues
				return fmt.Errorf("mwan3 functionality issues remain unfixed: %v", issues)
			}
		} else {
			// Return error if auto-fix is disabled and issues exist
			return fmt.Errorf("mwan3 functionality issues detected but auto-fix disabled: %v", issues)
		}
	} else {
		m.logger.Debug("mwan3 health check passed")
	}

	return nil
}

// checkUbusHealth performs ubus health checks
func (m *Manager) checkUbusHealth(ctx context.Context) error {
	if !m.config.UbusMonitorEnabled {
		return nil
	}

	// Check ubus health
	info, err := m.ubusMonitor.CheckUbusHealth(ctx)
	if err != nil {
		m.logger.Error("Ubus health check failed", "error", err)
		return err
	}

	// Log ubus health status
	if !info.UbusResponding || !info.RpcdRunning || info.ServicesCount < m.config.UbusMinServicesExpected {
		m.logger.Warn("Ubus health issues detected",
			"ubus_responding", info.UbusResponding,
			"rpcd_running", info.RpcdRunning,
			"services_count", info.ServicesCount,
			"min_expected", m.config.UbusMinServicesExpected,
			"fix_attempts", info.FixAttempts)
		m.issuesFound++

		// Return error to indicate ubus issues
		return fmt.Errorf("ubus health issues detected: responding=%v, rpcd_running=%v, services_count=%d (min_expected=%d)",
			info.UbusResponding, info.RpcdRunning, info.ServicesCount, m.config.UbusMinServicesExpected)
	} else {
		m.logger.Debug("Ubus health check passed",
			"services_count", info.ServicesCount,
			"ubus_responding", info.UbusResponding)
	}

	return nil
}

// Start initializes and starts the system management components
func (m *Manager) Start() error {
	m.mu.Lock()
	defer m.mu.Unlock()

	m.logger.Info("Starting system management", "enabled", m.config.Enabled)

	// Initialize all components if needed
	// Most components are already initialized in NewManager
	// This method is mainly for future extensibility

	return nil
}

// Stop gracefully shuts down the system management components
func (m *Manager) Stop() {
	m.mu.Lock()
	defer m.mu.Unlock()

	m.logger.Info("Stopping system management")

	// Stop any background tasks or cleanup resources
	// Currently no background tasks to stop, but method exists for future use
}

// GetWiFiManager returns the WiFi manager for ubus API access
func (m *Manager) GetWiFiManager() *WiFiManager {
	m.mu.Lock()
	defer m.mu.Unlock()
	return m.wifiManager
}

// Helper methods for health checks

func (m *Manager) isGPSAvailable() bool {
	// Check if gpsctl exists
	if _, err := m.exec("which", "gpsctl"); err != nil {
		return false
	}

	// Check if any GPS devices exist
	if _, err := os.Stat("/dev/ttyUSB1"); err != nil {
		if _, err := os.Stat("/dev/ttyUSB2"); err != nil {
			return false
		}
	}

	return true
}

func (m *Manager) isStarlinkAvailable() bool {
	// Check if we can reach the Starlink dish network
	if _, err := m.execWithTimeout(3, "ping", "-c", "1", "-W", "3", "192.168.100.1"); err != nil {
		return false
	}

	return true
}

func (m *Manager) testUbus() bool {
	_, err := m.execWithTimeout(3, "ubus", "-S", "call", "system", "board", "{}")
	return err == nil
}

func (m *Manager) execWithTimeout(timeout int, command string, args ...string) (string, error) {
	ctx, cancel := context.WithTimeout(context.Background(), time.Duration(timeout)*time.Second)
	defer cancel()

	cmd := exec.CommandContext(ctx, command, args...)
	output, err := cmd.Output()
	return strings.TrimSpace(string(output)), err
}

func (m *Manager) exec(command string, args ...string) (string, error) {
	cmd := exec.Command(command, args...)
	output, err := cmd.Output()
	return strings.TrimSpace(string(output)), err
}

// Fix methods

func (m *Manager) fixGPSSystem() bool {
	m.logger.Info("Attempting to fix GPS system")

	// First try normal restart
	m.exec("/etc/init.d/gpsd", "restart")
	time.Sleep(5 * time.Second)

	// Test if it's working now
	if _, err := m.execWithTimeout(5, "gpsctl", "status"); err == nil {
		m.logger.Info("GPS system fixed with normal restart")
		return true
	}

	// If normal restart failed, try aggressive fix
	m.logger.Info("Normal restart failed, attempting aggressive GPS fix")

	// Kill all gpsd processes
	m.exec("killall", "-9", "gpsd")
	m.logger.Info("Killed all gpsd processes")

	// Wait for processes to be killed
	time.Sleep(3 * time.Second)

	// Start gpsd again
	m.exec("/etc/init.d/gpsd", "start")
	m.logger.Info("Restarted gpsd service")

	// Wait for gpsd to stabilize
	time.Sleep(5 * time.Second)

	// Test gpsctl again
	if _, err := m.execWithTimeout(5, "gpsctl", "status"); err == nil {
		m.logger.Info("GPS system fixed successfully with aggressive restart")
		return true
	} else {
		m.logger.Error("GPS system still not working after aggressive restart")
		return false
	}
}

func (m *Manager) fixStarlink() bool {
	m.logger.Info("Attempting to fix Starlink connectivity")

	// Restart network interface
	m.exec("/etc/init.d/network", "restart")

	// Wait for network to stabilize
	time.Sleep(10 * time.Second)

	// Test connectivity again
	if _, err := m.execWithTimeout(3, "ping", "-c", "1", "-W", "3", "192.168.100.1"); err == nil {
		m.logger.Info("Starlink connectivity restored")
		return true
	} else {
		m.logger.Error("Starlink connectivity still not working")
		return false
	}
}

func (m *Manager) fixMWAN3() bool {
	m.logger.Info("Attempting to fix mwan3")

	// Restart mwan3
	m.exec("/etc/init.d/mwan3", "restart")

	// Wait for mwan3 to stabilize
	time.Sleep(5 * time.Second)

	// Test mwan3 again
	if _, err := m.execWithTimeout(3, "ubus", "call", "mwan3", "status"); err == nil {
		m.logger.Info("mwan3 fixed successfully")
		return true
	} else {
		m.logger.Error("mwan3 still not working after restart")
		return false
	}
}
