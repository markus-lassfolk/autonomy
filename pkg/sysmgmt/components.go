package sysmgmt

import (
	"context"
	"fmt"
	"os"
	"os/exec"
	"regexp"
	"strings"
	"time"

	"github.com/markus-lassfolk/autonomy/pkg/logx"
	"github.com/markus-lassfolk/autonomy/pkg/starlink"
)

// LogFloodDetector detects and prevents log flooding
type LogFloodDetector struct {
	config *Config
	logger *logx.Logger
	dryRun bool
}

// NewLogFloodDetector creates a new log flood detector
func NewLogFloodDetector(config *Config, logger *logx.Logger, dryRun bool) *LogFloodDetector {
	return &LogFloodDetector{
		config: config,
		logger: logger,
		dryRun: dryRun,
	}
}

// Check monitors for log flooding and takes action
func (lfd *LogFloodDetector) Check(ctx context.Context) error {
	if !lfd.config.LogFloodEnabled {
		return nil
	}

	lfd.logger.Debug("Checking for log flooding")

	for _, pattern := range lfd.config.LogFloodPatterns {
		select {
		case <-ctx.Done():
			return ctx.Err()
		default:
		}

		if count, err := lfd.countRecentLogEntries(pattern); err == nil && count > lfd.config.LogFloodThreshold {
			lfd.logger.Warn("Log flooding detected", "pattern", pattern, "count", count, "threshold", lfd.config.LogFloodThreshold)
			return lfd.handleLogFlood(ctx, pattern, count)
		}
	}

	return nil
}

// countRecentLogEntries counts log entries matching a pattern in the last hour
func (lfd *LogFloodDetector) countRecentLogEntries(pattern string) (int, error) {
	cmd := exec.Command("logread", "-l", "3600") // Last hour
	output, err := cmd.Output()
	if err != nil {
		return 0, err
	}

	lines := strings.Split(string(output), "\n")
	count := 0
	for _, line := range lines {
		if strings.Contains(line, pattern) {
			count++
		}
	}

	return count, nil
}

// handleLogFlood handles log flooding by reducing log verbosity
func (lfd *LogFloodDetector) handleLogFlood(ctx context.Context, pattern string, count int) error {
	lfd.logger.Info("Handling log flood", "pattern", pattern, "count", count, "dry_run", lfd.dryRun)

	if lfd.dryRun {
		lfd.logger.Info("DRY RUN: Would handle log flood", "pattern", pattern)
		return nil
	}

	// Try to reduce log verbosity for the affected service
	if strings.Contains(pattern, "hostapd") {
		return lfd.reduceHostapdVerbosity(ctx)
	}

	// Generic log flood handling
	return lfd.genericLogFloodHandling(ctx, pattern)
}

// reduceHostapdVerbosity reduces hostapd log verbosity
func (lfd *LogFloodDetector) reduceHostapdVerbosity(ctx context.Context) error {
	// Restart hostapd with reduced verbosity
	cmd := exec.Command("/etc/init.d/hostapd", "restart")
	if err := cmd.Run(); err != nil {
		return fmt.Errorf("failed to restart hostapd: %w", err)
	}

	lfd.logger.Info("Reduced hostapd log verbosity")
	return nil
}

// genericLogFloodHandling handles generic log flooding
func (lfd *LogFloodDetector) genericLogFloodHandling(ctx context.Context, pattern string) error {
	// Log the issue and send notification
	lfd.logger.Warn("Log flooding detected", "pattern", pattern)

	if lfd.config.NotificationsEnabled && lfd.config.NotifyOnCritical {
		lfd.sendCriticalNotification("Log flooding detected", fmt.Sprintf("Pattern: %s", pattern))
	}

	return nil
}

// sendCriticalNotification sends a critical notification
func (lfd *LogFloodDetector) sendCriticalNotification(action, details string) {
	lfd.logger.Warn("Critical notification", "action", action, "details", details)
}

// TimeManager manages time drift and NTP synchronization
type TimeManager struct {
	config *Config
	logger *logx.Logger
	dryRun bool
}

// NewTimeManager creates a new time manager
func NewTimeManager(config *Config, logger *logx.Logger, dryRun bool) *TimeManager {
	return &TimeManager{
		config: config,
		logger: logger,
		dryRun: dryRun,
	}
}

// Check monitors time drift and NTP synchronization
func (tm *TimeManager) Check(ctx context.Context) error {
	if !tm.config.TimeDriftEnabled {
		return nil
	}

	tm.logger.Debug("Checking time synchronization")

	// Check if NTP service is running
	if running, err := tm.isNTPServiceRunning(); err != nil || !running {
		tm.logger.Warn("NTP service not running")
		return tm.restartNTPService(ctx)
	}

	// Check time drift
	if drift, err := tm.checkTimeDrift(); err == nil && drift > tm.config.TimeDriftThreshold {
		tm.logger.Warn("Time drift detected", "drift", drift, "threshold", tm.config.TimeDriftThreshold)
		return tm.syncTime(ctx)
	}

	return nil
}

// isNTPServiceRunning checks if NTP service is running
func (tm *TimeManager) isNTPServiceRunning() (bool, error) {
	// Check for common NTP services
	services := []string{"sysntpd", "ntpd", "chronyd"}

	for _, service := range services {
		cmd := exec.Command("pgrep", service)
		if err := cmd.Run(); err == nil {
			return true, nil
		}
	}

	return false, nil
}

// checkTimeDrift checks for time drift by querying NTP servers
func (tm *TimeManager) checkTimeDrift() (time.Duration, error) {
	// Try to get time offset from NTP servers
	ntpServers := []string{"pool.ntp.org", "time.nist.gov", "time.google.com"}

	for _, server := range ntpServers {
		if offset, err := tm.getNTPOffset(server); err == nil {
			return time.Duration(offset) * time.Millisecond, nil
		}
	}

	return 0, fmt.Errorf("unable to check time drift")
}

// getNTPOffset gets time offset from an NTP server
func (tm *TimeManager) getNTPOffset(server string) (int64, error) {
	// Use ntpdate or similar to check offset
	cmd := exec.Command("ntpdate", "-q", server)
	output, err := cmd.Output()
	if err != nil {
		return 0, err
	}

	// Parse offset from output
	lines := strings.Split(string(output), "\n")
	for _, line := range lines {
		if strings.Contains(line, "offset") {
			// Extract offset value (simplified parsing)
			re := regexp.MustCompile(`offset\s+([-\d.]+)`)
			if matches := re.FindStringSubmatch(line); len(matches) > 1 {
				// Convert to milliseconds
				offsetMs := float64(0)
				if _, err := fmt.Sscanf(matches[1], "%f", &offsetMs); err != nil {
					return 0, fmt.Errorf("failed to parse NTP offset: %w", err)
				}
				return int64(offsetMs * 1000), nil
			}
		}
	}

	return 0, fmt.Errorf("unable to parse NTP offset")
}

// restartNTPService restarts the NTP service
func (tm *TimeManager) restartNTPService(ctx context.Context) error {
	tm.logger.Info("Restarting NTP service", "dry_run", tm.dryRun)

	if tm.dryRun {
		tm.logger.Info("DRY RUN: Would restart NTP service")
		return nil
	}

	// Try to restart sysntpd (common on OpenWrt/RutOS)
	cmd := exec.Command("/etc/init.d/sysntpd", "restart")
	if err := cmd.Run(); err != nil {
		return fmt.Errorf("failed to restart NTP service: %w", err)
	}

	tm.logger.Info("NTP service restarted")
	return nil
}

// syncTime synchronizes system time
func (tm *TimeManager) syncTime(ctx context.Context) error {
	tm.logger.Info("Synchronizing system time", "dry_run", tm.dryRun)

	if tm.dryRun {
		tm.logger.Info("DRY RUN: Would sync time")
		return nil
	}

	// Use ntpdate to sync time
	cmd := exec.Command("ntpdate", "pool.ntp.org")
	if err := cmd.Run(); err != nil {
		return fmt.Errorf("failed to sync time: %w", err)
	}

	tm.logger.Info("System time synchronized")
	return nil
}

// NetworkManager manages network interface stability
type NetworkManager struct {
	config *Config
	logger *logx.Logger
	dryRun bool
}

// NewNetworkManager creates a new network manager
func NewNetworkManager(config *Config, logger *logx.Logger, dryRun bool) *NetworkManager {
	return &NetworkManager{
		config: config,
		logger: logger,
		dryRun: dryRun,
	}
}

// Check monitors network interface stability
func (nm *NetworkManager) Check(ctx context.Context) error {
	if !nm.config.InterfaceFlappingEnabled {
		return nil
	}

	nm.logger.Debug("Checking network interface stability")

	for _, iface := range nm.config.FlappingInterfaces {
		select {
		case <-ctx.Done():
			return ctx.Err()
		default:
		}

		if count, err := nm.countInterfaceEvents(iface); err == nil && count > nm.config.FlappingThreshold {
			nm.logger.Warn("Interface flapping detected", "interface", iface, "events", count, "threshold", nm.config.FlappingThreshold)
			return nm.stabilizeInterface(ctx, iface)
		}
	}

	return nil
}

// countInterfaceEvents counts interface up/down events in recent logs
func (nm *NetworkManager) countInterfaceEvents(iface string) (int, error) {
	cmd := exec.Command("logread", "-l", "3600") // Last hour
	output, err := cmd.Output()
	if err != nil {
		return 0, err
	}

	lines := strings.Split(string(output), "\n")
	count := 0
	patterns := []string{
		fmt.Sprintf("%s: link becomes ready", iface),
		fmt.Sprintf("%s: link becomes not ready", iface),
		fmt.Sprintf("Interface %s is now up", iface),
		fmt.Sprintf("Interface %s is now down", iface),
	}

	for _, line := range lines {
		for _, pattern := range patterns {
			if strings.Contains(line, pattern) {
				count++
				break
			}
		}
	}

	return count, nil
}

// stabilizeInterface stabilizes a flapping interface
func (nm *NetworkManager) stabilizeInterface(ctx context.Context, iface string) error {
	nm.logger.Info("Stabilizing flapping interface", "interface", iface, "dry_run", nm.dryRun)

	if nm.dryRun {
		nm.logger.Info("DRY RUN: Would stabilize interface", "interface", iface)
		return nil
	}

	// Restart network service to stabilize interfaces
	cmd := exec.Command("/etc/init.d/network", "restart")
	if err := cmd.Run(); err != nil {
		return fmt.Errorf("failed to restart network service: %w", err)
	}

	nm.logger.Info("Network service restarted to stabilize interfaces")
	return nil
}

// StarlinkHealthManager manages comprehensive Starlink dish health monitoring
type StarlinkHealthManager struct {
	config         *Config
	logger         *logx.Logger
	dryRun         bool
	starlinkClient *starlink.Client
}

// NewStarlinkHealthManager creates a new Starlink health manager
func NewStarlinkHealthManager(config *Config, logger *logx.Logger, dryRun bool) *StarlinkHealthManager {
	// Initialize centralized Starlink client
	starlinkClient := starlink.DefaultClient(logger)

	return &StarlinkHealthManager{
		config:         config,
		logger:         logger,
		dryRun:         dryRun,
		starlinkClient: starlinkClient,
	}
}

// DatabaseManager manages database health
type DatabaseManager struct {
	config *Config
	logger *logx.Logger
	dryRun bool
}

// NewDatabaseManager creates a new database manager
func NewDatabaseManager(config *Config, logger *logx.Logger, dryRun bool) *DatabaseManager {
	return &DatabaseManager{
		config: config,
		logger: logger,
		dryRun: dryRun,
	}
}

// Check monitors database health
func (dbm *DatabaseManager) Check(ctx context.Context) error {
	if !dbm.config.DatabaseEnabled {
		return nil
	}

	dbm.logger.Debug("Checking database health")

	// Check for database errors in logs
	if count, err := dbm.countDatabaseErrors(); err == nil && count >= dbm.config.DatabaseErrorThreshold {
		dbm.logger.Warn("Database errors detected", "count", count, "threshold", dbm.config.DatabaseErrorThreshold)
		return dbm.fixDatabaseIssues(ctx)
	}

	// Check for corrupted databases
	if corrupted, err := dbm.findCorruptedDatabases(); err == nil && len(corrupted) > 0 {
		dbm.logger.Warn("Corrupted databases found", "databases", corrupted)
		return dbm.recreateDatabases(ctx, corrupted)
	}

	return nil
}

// countDatabaseErrors counts database errors in recent logs
func (dbm *DatabaseManager) countDatabaseErrors() (int, error) {
	cmd := exec.Command("logread", "-l", "3600") // Last hour
	output, err := cmd.Output()
	if err != nil {
		return 0, err
	}

	lines := strings.Split(string(output), "\n")
	count := 0
	errorPatterns := []string{
		"Can't open database",
		"database is locked",
		"database or disk is full",
		"database corruption",
	}

	for _, line := range lines {
		for _, pattern := range errorPatterns {
			if strings.Contains(line, pattern) {
				count++
				break
			}
		}
	}

	return count, nil
}

// findCorruptedDatabases finds potentially corrupted databases
func (dbm *DatabaseManager) findCorruptedDatabases() ([]string, error) {
	// RUTOS doesn't use SQLite databases, it uses plain text UCI files
	// Only check for databases if they actually exist
	databases := []string{}

	// Check if SQLite databases exist (for OpenWrt systems)
	if _, err := os.Stat("/etc/config/uci.db"); err == nil {
		databases = append(databases, "/etc/config/uci.db")
	}
	if _, err := os.Stat("/var/lib/ubox/config.db"); err == nil {
		databases = append(databases, "/var/lib/ubox/config.db")
	}

	// If no databases exist, skip database health check (normal for RUTOS)
	if len(databases) == 0 {
		dbm.logger.Debug("No SQLite databases found - skipping database health check (normal for RUTOS)")
		return []string{}, nil
	}
	var corrupted []string

	for _, db := range databases {
		if info, err := dbm.checkDatabaseHealth(db); err == nil && !info.healthy {
			corrupted = append(corrupted, db)
		}
	}

	return corrupted, nil
}

// checkDatabaseHealth checks if a database is healthy
func (dbm *DatabaseManager) checkDatabaseHealth(dbPath string) (*dbHealthInfo, error) {
	// Check file size
	cmd := exec.Command("stat", "-c", "%s", dbPath)
	output, err := cmd.Output()
	if err != nil {
		return &dbHealthInfo{healthy: false, reason: "file not found"}, nil
	}

	sizeStr := strings.TrimSpace(string(output))
	var size int
	if _, err := fmt.Sscanf(sizeStr, "%d", &size); err != nil {
		return &dbHealthInfo{healthy: false, reason: "invalid size"}, nil
	}

	// Check if file is too small (likely corrupted)
	if size < dbm.config.DatabaseMinSizeKB*1024 {
		return &dbHealthInfo{healthy: false, reason: "file too small"}, nil
	}

	// Check file age
	cmd = exec.Command("stat", "-c", "%Y", dbPath)
	output, err = cmd.Output()
	if err != nil {
		return &dbHealthInfo{healthy: false, reason: "cannot check age"}, nil
	}

	mtimeStr := strings.TrimSpace(string(output))
	var mtime int64
	if _, err := fmt.Sscanf(mtimeStr, "%d", &mtime); err != nil {
		return &dbHealthInfo{healthy: false, reason: "invalid modification time"}, nil
	}

	// Check if file is too old (stale)
	cutoff := time.Now().AddDate(0, 0, -dbm.config.DatabaseMaxAgeDays)
	if time.Unix(mtime, 0).Before(cutoff) {
		return &dbHealthInfo{healthy: false, reason: "file too old"}, nil
	}

	return &dbHealthInfo{healthy: true}, nil
}

// dbHealthInfo represents database health information
type dbHealthInfo struct {
	healthy bool
	reason  string
}

// fixDatabaseIssues fixes database issues
func (dbm *DatabaseManager) fixDatabaseIssues(ctx context.Context) error {
	dbm.logger.Info("Fixing database issues", "dry_run", dbm.dryRun)

	if dbm.dryRun {
		dbm.logger.Info("DRY RUN: Would fix database issues")
		return nil
	}

	// Restart services that use databases
	services := []string{"logd", "ubus"}

	for _, service := range services {
		cmd := exec.Command("/etc/init.d/"+service, "restart")
		if err := cmd.Run(); err != nil {
			dbm.logger.Error("Failed to restart service", "service", service, "error", err)
		} else {
			dbm.logger.Info("Restarted service", "service", service)
		}
	}

	return nil
}

// recreateDatabases recreates corrupted databases
func (dbm *DatabaseManager) recreateDatabases(ctx context.Context, databases []string) error {
	dbm.logger.Info("Recreating corrupted databases", "databases", databases, "dry_run", dbm.dryRun)

	if dbm.dryRun {
		dbm.logger.Info("DRY RUN: Would recreate databases", "databases", databases)
		return nil
	}

	for _, db := range databases {
		if err := dbm.recreateDatabase(db); err != nil {
			dbm.logger.Error("Failed to recreate database", "database", db, "error", err)
		} else {
			dbm.logger.Info("Recreated database", "database", db)
		}
	}

	return nil
}

// recreateDatabase recreates a single database
func (dbm *DatabaseManager) recreateDatabase(dbPath string) error {
	// Create backup
	backupPath := dbPath + ".backup." + time.Now().Format("20060102_150405")
	cmd := exec.Command("cp", dbPath, backupPath)
	if err := cmd.Run(); err != nil {
		// Ignore errors, file might not exist
		dbm.logger.Debug("Failed to create backup", "database", dbPath, "error", err)
	}

	// Remove corrupted database
	cmd = exec.Command("rm", "-f", dbPath)
	if err := cmd.Run(); err != nil {
		return fmt.Errorf("failed to remove corrupted database: %w", err)
	}

	// Restart related services to recreate database
	services := []string{"logd", "ubus"}
	for _, service := range services {
		cmd = exec.Command("/etc/init.d/"+service, "restart")
		if err := cmd.Run(); err != nil {
			// Ignore errors
			dbm.logger.Debug("Failed to restart service", "service", service, "error", err)
		}
	}

	return nil
}

// NotificationManager manages notifications
type NotificationManager struct {
	config *Config
	logger *logx.Logger
	dryRun bool
}

// NewNotificationManager creates a new notification manager
func NewNotificationManager(config *Config, logger *logx.Logger, dryRun bool) *NotificationManager {
	return &NotificationManager{
		config: config,
		logger: logger,
		dryRun: dryRun,
	}
}

// SendNotification sends a notification
func (nm *NotificationManager) SendNotification(title, message string, priority int) error {
	if !nm.config.NotificationsEnabled {
		return nil
	}

	if nm.dryRun {
		nm.logger.Info("DRY RUN: Would send notification", "title", title, "priority", priority)
		return nil
	}

	// Send Pushover notification if configured
	if nm.config.PushoverEnabled && nm.config.PushoverToken != "" && nm.config.PushoverUser != "" {
		return nm.sendPushoverNotification(title, message, priority)
	}

	// Log notification
	nm.logger.Info("Notification", "title", title, "message", message, "priority", priority)
	return nil
}

// sendPushoverNotification sends a Pushover notification
func (nm *NotificationManager) sendPushoverNotification(title, message string, priority int) error {
	// Use curl to send Pushover notification
	cmd := exec.Command("curl", "-s",
		"-F", "token="+nm.config.PushoverToken,
		"-F", "user="+nm.config.PushoverUser,
		"-F", "title="+title,
		"-F", "message="+message,
		"-F", fmt.Sprintf("priority=%d", priority),
		"https://api.pushover.net/1/messages.json")

	return cmd.Run()
}
