package sysmgmt

import (
	"context"
	"fmt"
	"os/exec"
	"strings"
	"time"

	"github.com/markus-lassfolk/autonomy/pkg/logx"
)

// UbusHealthInfo represents ubus health status
type UbusHealthInfo struct {
	UbusResponding   bool   `json:"ubus_responding"`
	RpcdRunning      bool   `json:"rpcd_running"`
	UbusSocketExists bool   `json:"ubus_socket_exists"`
	ServicesCount    int    `json:"services_count"`
	LastError        string `json:"last_error,omitempty"`
	LastCheckTime    string `json:"last_check_time"`
	FixAttempts      int    `json:"fix_attempts"`
	LastFixTime      string `json:"last_fix_time,omitempty"`
}

// UbusMonitor manages ubus health monitoring and troubleshooting
type UbusMonitor struct {
	logger      *logx.Logger
	config      *UbusMonitorConfig
	dryRun      bool
	fixAttempts int
	lastFixTime time.Time
}

// UbusMonitorConfig holds configuration for ubus monitoring
type UbusMonitorConfig struct {
	Enabled             bool          `json:"enabled"`               // Enable ubus monitoring
	CheckInterval       time.Duration `json:"check_interval"`        // How often to check ubus health
	MaxFixAttempts      int           `json:"max_fix_attempts"`      // Maximum fix attempts per hour
	AutoFix             bool          `json:"auto_fix"`              // Automatically fix ubus issues
	RestartTimeout      time.Duration `json:"restart_timeout"`       // Timeout for service restarts
	MinServicesExpected int           `json:"min_services_expected"` // Minimum expected ubus services
	CriticalServices    []string      `json:"critical_services"`     // Critical services that must be available
}

// NewUbusMonitor creates a new ubus health monitor
func NewUbusMonitor(logger *logx.Logger, config *UbusMonitorConfig, dryRun bool) *UbusMonitor {
	if config == nil {
		config = &UbusMonitorConfig{
			Enabled:             true,
			CheckInterval:       5 * time.Minute,
			MaxFixAttempts:      3,
			AutoFix:             true,
			RestartTimeout:      30 * time.Second,
			MinServicesExpected: 20,
			CriticalServices:    []string{"system", "uci", "network", "service"},
		}
	}

	return &UbusMonitor{
		logger:      logger,
		config:      config,
		dryRun:      dryRun,
		fixAttempts: 0,
	}
}

// CheckUbusHealth checks the overall health of ubus and rpcd
func (um *UbusMonitor) CheckUbusHealth(ctx context.Context) (*UbusHealthInfo, error) {
	info := &UbusHealthInfo{
		LastCheckTime: time.Now().UTC().Format(time.RFC3339),
		FixAttempts:   um.fixAttempts,
	}

	if um.lastFixTime.After(time.Time{}) {
		info.LastFixTime = um.lastFixTime.UTC().Format(time.RFC3339)
	}

	// Check if ubus is responding
	ubusResponding, err := um.testUbusResponse()
	info.UbusResponding = ubusResponding
	if err != nil {
		info.LastError = err.Error()
	}

	// Check if rpcd is running
	rpcdRunning, err := um.checkRpcdStatus()
	info.RpcdRunning = rpcdRunning
	if err != nil {
		um.logger.Debug("Failed to check rpcd status", "error", err)
	}

	// Check if ubus socket exists
	socketExists, err := um.checkUbusSocket()
	info.UbusSocketExists = socketExists
	if err != nil {
		um.logger.Debug("Failed to check ubus socket", "error", err)
	}

	// Count available services
	servicesCount, err := um.countUbusServices()
	info.ServicesCount = servicesCount
	if err != nil {
		um.logger.Debug("Failed to count ubus services", "error", err)
	}

	// Check critical services
	criticalServicesMissing := um.checkCriticalServices()

	// Determine if intervention is needed
	if !ubusResponding || !rpcdRunning || servicesCount < um.config.MinServicesExpected || len(criticalServicesMissing) > 0 {
		um.logger.Warn("Ubus health issues detected",
			"ubus_responding", ubusResponding,
			"rpcd_running", rpcdRunning,
			"services_count", servicesCount,
			"critical_missing", criticalServicesMissing,
			"fix_attempts", um.fixAttempts)

		if um.config.AutoFix && um.canAttemptFix() {
			return um.attemptUbusFix(ctx, info)
		}
	} else {
		um.logger.Debug("Ubus health check passed",
			"services_count", servicesCount,
			"ubus_responding", ubusResponding)
	}

	return info, nil
}

// testUbusResponse tests if ubus is responding to commands
func (um *UbusMonitor) testUbusResponse() (bool, error) {
	ctx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
	defer cancel()

	cmd := exec.CommandContext(ctx, "ubus", "list")
	output, err := cmd.Output()
	if err != nil {
		return false, fmt.Errorf("ubus list failed: %w", err)
	}

	// Check if output contains services (not just error messages)
	lines := strings.Split(string(output), "\n")
	serviceCount := 0
	for _, line := range lines {
		line = strings.TrimSpace(line)
		if line != "" && !strings.Contains(line, "Failed to connect") {
			serviceCount++
		}
	}

	return serviceCount > 0, nil
}

// checkRpcdStatus checks if rpcd service is running
func (um *UbusMonitor) checkRpcdStatus() (bool, error) {
	cmd := exec.Command("pgrep", "rpcd")
	err := cmd.Run()
	return err == nil, nil
}

// checkUbusSocket checks if ubus socket exists and is accessible
func (um *UbusMonitor) checkUbusSocket() (bool, error) {
	cmd := exec.Command("test", "-S", "/var/run/ubus/ubus.sock")
	err := cmd.Run()
	return err == nil, nil
}

// countUbusServices counts the number of available ubus services
func (um *UbusMonitor) countUbusServices() (int, error) {
	ctx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
	defer cancel()

	cmd := exec.CommandContext(ctx, "ubus", "list")
	output, err := cmd.Output()
	if err != nil {
		return 0, err
	}

	lines := strings.Split(string(output), "\n")
	count := 0
	for _, line := range lines {
		line = strings.TrimSpace(line)
		if line != "" && !strings.Contains(line, "Failed to connect") {
			count++
		}
	}

	return count, nil
}

// checkCriticalServices checks if critical services are available
func (um *UbusMonitor) checkCriticalServices() []string {
	ctx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
	defer cancel()

	cmd := exec.CommandContext(ctx, "ubus", "list")
	output, err := cmd.Output()
	if err != nil {
		return um.config.CriticalServices // Assume all critical services are missing
	}

	availableServices := strings.Split(string(output), "\n")
	missing := []string{}

	for _, critical := range um.config.CriticalServices {
		found := false
		for _, available := range availableServices {
			if strings.TrimSpace(available) == critical {
				found = true
				break
			}
		}
		if !found {
			missing = append(missing, critical)
		}
	}

	return missing
}

// canAttemptFix checks if we can attempt to fix ubus issues
func (um *UbusMonitor) canAttemptFix() bool {
	// Reset fix attempts if more than an hour has passed
	if time.Since(um.lastFixTime) > time.Hour {
		um.fixAttempts = 0
	}

	return um.fixAttempts < um.config.MaxFixAttempts
}

// attemptUbusFix attempts to fix ubus issues
func (um *UbusMonitor) attemptUbusFix(ctx context.Context, info *UbusHealthInfo) (*UbusHealthInfo, error) {
	um.logger.Info("Attempting to fix ubus issues", "dry_run", um.dryRun, "fix_attempt", um.fixAttempts+1)

	if um.dryRun {
		um.logger.Info("DRY RUN: Would attempt ubus fix")
		return info, nil
	}

	um.fixAttempts++
	um.lastFixTime = time.Now()

	// Step 1: Try restarting rpcd service
	if um.restartRpcdService(ctx) {
		time.Sleep(5 * time.Second) // Wait for service to stabilize
		if healthy, _ := um.testUbusResponse(); healthy {
			um.logger.Info("Ubus fixed by restarting rpcd service")
			return um.CheckUbusHealth(ctx)
		}
	}

	// Step 2: Try manual ubus restart
	if um.restartUbusManually(ctx) {
		time.Sleep(3 * time.Second) // Wait for ubus to start
		if healthy, _ := um.testUbusResponse(); healthy {
			um.logger.Info("Ubus fixed by manual restart")
			return um.CheckUbusHealth(ctx)
		}
	}

	// Step 3: Try restarting rpcd again with longer timeout
	if um.restartRpcdServiceWithTimeout(ctx) {
		time.Sleep(10 * time.Second) // Longer wait
		if healthy, _ := um.testUbusResponse(); healthy {
			um.logger.Info("Ubus fixed by rpcd restart with timeout")
			return um.CheckUbusHealth(ctx)
		}
	}

	// Step 4: Try aggressive fix (kill all processes and restart)
	if um.attemptAggressiveFix(ctx) {
		time.Sleep(5 * time.Second) // Wait for services to stabilize
		if healthy, _ := um.testUbusResponse(); healthy {
			um.logger.Info("Ubus fixed by aggressive restart")
			return um.CheckUbusHealth(ctx)
		}
	}

	um.logger.Error("Failed to fix ubus issues after all attempts")
	return info, fmt.Errorf("failed to fix ubus issues after %d attempts", um.fixAttempts)
}

// restartRpcdService restarts the rpcd service
func (um *UbusMonitor) restartRpcdService(ctx context.Context) bool {
	um.logger.Debug("Restarting rpcd service")

	// Check if rpcd init script exists
	cmd := exec.CommandContext(ctx, "test", "-f", "/etc/init.d/rpcd")
	if err := cmd.Run(); err != nil {
		um.logger.Debug("rpcd init script not found")
		return false
	}

	// Restart rpcd
	cmd = exec.CommandContext(ctx, "/etc/init.d/rpcd", "restart")
	if err := cmd.Run(); err != nil {
		um.logger.Warn("Failed to restart rpcd service", "error", err)
		return false
	}

	return true
}

// restartRpcdServiceWithTimeout restarts rpcd with a timeout
func (um *UbusMonitor) restartRpcdServiceWithTimeout(ctx context.Context) bool {
	um.logger.Debug("Restarting rpcd service with timeout")

	timeoutCtx, cancel := context.WithTimeout(ctx, um.config.RestartTimeout)
	defer cancel()

	cmd := exec.CommandContext(timeoutCtx, "/etc/init.d/rpcd", "restart")
	if err := cmd.Run(); err != nil {
		um.logger.Warn("Failed to restart rpcd service with timeout", "error", err)
		return false
	}

	return true
}

// restartUbusManually manually restarts ubus daemon
func (um *UbusMonitor) restartUbusManually(ctx context.Context) bool {
	um.logger.Debug("Manually restarting ubus daemon")

	// Kill existing ubusd processes
	cmd := exec.CommandContext(ctx, "killall", "ubusd")
	cmd.Run() // Ignore errors, process might not exist

	time.Sleep(2 * time.Second)

	// Start ubusd manually
	cmd = exec.CommandContext(ctx, "/sbin/ubusd")
	if err := cmd.Start(); err != nil {
		um.logger.Warn("Failed to start ubusd manually", "error", err)
		return false
	}

	return true
}

// GetUbusLogs retrieves recent ubus-related logs
func (um *UbusMonitor) GetUbusLogs() ([]string, error) {
	ctx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
	defer cancel()

	cmd := exec.CommandContext(ctx, "logread")
	output, err := cmd.Output()
	if err != nil {
		return nil, fmt.Errorf("failed to get logs: %w", err)
	}

	lines := strings.Split(string(output), "\n")
	ubusLogs := []string{}

	for _, line := range lines {
		if strings.Contains(strings.ToLower(line), "ubus") {
			ubusLogs = append(ubusLogs, line)
		}
	}

	// Return last 10 ubus logs
	if len(ubusLogs) > 10 {
		return ubusLogs[len(ubusLogs)-10:], nil
	}

	return ubusLogs, nil
}

// ResetFixAttempts resets the fix attempt counter
func (um *UbusMonitor) ResetFixAttempts() {
	um.fixAttempts = 0
	um.lastFixTime = time.Time{}
}

// GetStatus returns the current status of the ubus monitor
func (um *UbusMonitor) GetStatus() map[string]interface{} {
	return map[string]interface{}{
		"enabled":        um.config.Enabled,
		"auto_fix":       um.config.AutoFix,
		"fix_attempts":   um.fixAttempts,
		"max_attempts":   um.config.MaxFixAttempts,
		"last_fix_time":  um.lastFixTime,
		"check_interval": um.config.CheckInterval.String(),
	}
}

// attemptAggressiveFix attempts an aggressive fix by killing all processes and restarting
func (um *UbusMonitor) attemptAggressiveFix(ctx context.Context) bool {
	um.logger.Info("Attempting aggressive ubus/rpcd fix")

	// Kill all ubus and rpcd processes
	cmd := exec.CommandContext(ctx, "killall", "-9", "ubusd")
	cmd.Run() // Ignore errors, process might not exist

	cmd = exec.CommandContext(ctx, "killall", "-9", "rpcd")
	cmd.Run() // Ignore errors, process might not exist

	um.logger.Info("Killed all ubusd and rpcd processes")

	// Wait for processes to be killed
	time.Sleep(3 * time.Second)

	// Start them again
	cmd = exec.CommandContext(ctx, "/etc/init.d/ubus", "start")
	if err := cmd.Run(); err != nil {
		um.logger.Warn("Failed to start ubus service", "error", err)
	}

	cmd = exec.CommandContext(ctx, "/etc/init.d/rpcd", "start")
	if err := cmd.Run(); err != nil {
		um.logger.Warn("Failed to start rpcd service", "error", err)
	}

	um.logger.Info("Restarted ubus and rpcd services")

	return true
}
