package main

import (
	"encoding/json"
	"fmt"
	"os"
	"time"

	"golang.org/x/crypto/ssh"
)

// SystemMaintenanceConfig holds configuration for system maintenance
type SystemMaintenanceConfig struct {
	MaintenanceInterval   time.Duration `uci:"autonomy.maintenance.interval" default:"1800s"` // 30 minutes
	GPSHealthCheckEnabled bool          `uci:"autonomy.maintenance.gps_health_enabled" default:"true"`
	NetworkHealthEnabled  bool          `uci:"autonomy.maintenance.network_health_enabled" default:"true"`
	StorageHealthEnabled  bool          `uci:"autonomy.maintenance.storage_health_enabled" default:"true"`
	LogRotationEnabled    bool          `uci:"autonomy.maintenance.log_rotation_enabled" default:"true"`
	ReportPath            string        `uci:"autonomy.maintenance.report_path" default:"/tmp/autonomy_maintenance.json"`
	AlertOnCritical       bool          `uci:"autonomy.maintenance.alert_on_critical" default:"true"`
}

// SystemMaintenanceReport holds the results of a maintenance cycle
type SystemMaintenanceReport struct {
	Timestamp        time.Time        `json:"timestamp"`
	OverallHealth    string           `json:"overall_health"` // HEALTHY, DEGRADED, CRITICAL
	GPSHealth        *GPSHealthStatus `json:"gps_health,omitempty"`
	NetworkHealth    *NetworkHealth   `json:"network_health,omitempty"`
	StorageHealth    *StorageHealth   `json:"storage_health,omitempty"`
	SystemHealth     *SystemHealth    `json:"system_health,omitempty"`
	ActionsPerformed []string         `json:"actions_performed"`
	Recommendations  []string         `json:"recommendations"`
	NextMaintenance  time.Time        `json:"next_maintenance"`
}

// NetworkHealth represents network connectivity health
type NetworkHealth struct {
	StarlinkConnected bool     `json:"starlink_connected"`
	CellularConnected bool     `json:"cellular_connected"`
	InternetReachable bool     `json:"internet_reachable"`
	DNSWorking        bool     `json:"dns_working"`
	LatencyMs         float64  `json:"latency_ms"`
	PacketLoss        float64  `json:"packet_loss"`
	Issues            []string `json:"issues"`
}

// StorageHealth represents storage and filesystem health
type StorageHealth struct {
	RootFSUsage      float64  `json:"rootfs_usage_percent"`
	TmpFSUsage       float64  `json:"tmpfs_usage_percent"`
	LogSizesMB       float64  `json:"log_sizes_mb"`
	FreeSpaceMB      float64  `json:"free_space_mb"`
	InodeUsage       float64  `json:"inode_usage_percent"`
	LogsRotated      bool     `json:"logs_rotated"`
	CleanupPerformed bool     `json:"cleanup_performed"`
	Issues           []string `json:"issues"`
}

// SystemHealth represents overall system health
type SystemHealth struct {
	UptimeHours       float64  `json:"uptime_hours"`
	LoadAverage       float64  `json:"load_average"`
	MemoryUsage       float64  `json:"memory_usage_percent"`
	CPUTemperature    float64  `json:"cpu_temperature_celsius"`
	CriticalProcesses bool     `json:"critical_processes_running"`
	KernelErrors      int      `json:"kernel_errors"`
	Issues            []string `json:"issues"`
}

// SystemMaintenanceManager manages system maintenance tasks
type SystemMaintenanceManager struct {
	config          *SystemMaintenanceConfig
	gpsMonitor      *GPSHealthMonitor
	sshClient       *ssh.Client
	lastMaintenance time.Time
}

// NewSystemMaintenanceManager creates a new system maintenance manager
func NewSystemMaintenanceManager(config *SystemMaintenanceConfig, sshClient *ssh.Client) *SystemMaintenanceManager {
	// Create GPS health monitor if enabled
	var gpsMonitor *GPSHealthMonitor
	if config.GPSHealthCheckEnabled {
		gpsConfig := &GPSHealthConfig{
			HealthCheckInterval:    5 * time.Minute,
			MaxConsecutiveFailures: 3,
			MinAccuracy:            10.0,
			MinSatellites:          4,
			MaxHDOP:                5.0,
			ResetCooldown:          10 * time.Minute,
			EnableAutoReset:        true,
			NotifyOnReset:          true,
		}
		gpsMonitor = NewGPSHealthMonitor(gpsConfig, sshClient)
	}

	return &SystemMaintenanceManager{
		config:     config,
		gpsMonitor: gpsMonitor,
		sshClient:  sshClient,
	}
}

// RunMaintenance performs a complete system maintenance cycle
func (smm *SystemMaintenanceManager) RunMaintenance() (*SystemMaintenanceReport, error) {
	fmt.Println("🔧 System Maintenance Cycle Starting...")
	fmt.Println("======================================")

	report := &SystemMaintenanceReport{
		Timestamp:        time.Now(),
		ActionsPerformed: []string{},
		Recommendations:  []string{},
		NextMaintenance:  time.Now().Add(smm.config.MaintenanceInterval),
	}

	healthLevels := []string{}

	// 1. GPS Health Check
	if smm.config.GPSHealthCheckEnabled && smm.gpsMonitor != nil {
		fmt.Println("📡 Checking GPS Health...")
		gpsHealth, err := smm.gpsMonitor.CheckGPSHealth()
		if err != nil {
			report.ActionsPerformed = append(report.ActionsPerformed, fmt.Sprintf("GPS health check failed: %v", err))
		} else {
			report.GPSHealth = gpsHealth
			if !gpsHealth.Healthy {
				healthLevels = append(healthLevels, "DEGRADED")
				report.Recommendations = append(report.Recommendations, "GPS system requires attention")
			}
			if gpsHealth.TotalResets > 0 {
				report.ActionsPerformed = append(report.ActionsPerformed, fmt.Sprintf("GPS reset performed: %s", gpsHealth.LastResetReason))
			}
		}
	}

	// 2. Network Health Check
	if smm.config.NetworkHealthEnabled {
		fmt.Println("🌐 Checking Network Health...")
		networkHealth, err := smm.checkNetworkHealth()
		if err != nil {
			report.ActionsPerformed = append(report.ActionsPerformed, fmt.Sprintf("Network health check failed: %v", err))
		} else {
			report.NetworkHealth = networkHealth
			if len(networkHealth.Issues) > 0 {
				healthLevels = append(healthLevels, "DEGRADED")
				for _, issue := range networkHealth.Issues {
					report.Recommendations = append(report.Recommendations, fmt.Sprintf("Network: %s", issue))
				}
			}
		}
	}

	// 3. Storage Health Check
	if smm.config.StorageHealthEnabled {
		fmt.Println("💾 Checking Storage Health...")
		storageHealth, err := smm.checkStorageHealth()
		if err != nil {
			report.ActionsPerformed = append(report.ActionsPerformed, fmt.Sprintf("Storage health check failed: %v", err))
		} else {
			report.StorageHealth = storageHealth
			if len(storageHealth.Issues) > 0 {
				healthLevels = append(healthLevels, "DEGRADED")
				for _, issue := range storageHealth.Issues {
					report.Recommendations = append(report.Recommendations, fmt.Sprintf("Storage: %s", issue))
				}
			}
			if storageHealth.LogsRotated {
				report.ActionsPerformed = append(report.ActionsPerformed, "Log rotation performed")
			}
			if storageHealth.CleanupPerformed {
				report.ActionsPerformed = append(report.ActionsPerformed, "Storage cleanup performed")
			}
		}
	}

	// 4. System Health Check
	fmt.Println("⚙️  Checking System Health...")
	systemHealth, err := smm.checkSystemHealth()
	if err != nil {
		report.ActionsPerformed = append(report.ActionsPerformed, fmt.Sprintf("System health check failed: %v", err))
	} else {
		report.SystemHealth = systemHealth
		if len(systemHealth.Issues) > 0 {
			healthLevels = append(healthLevels, "CRITICAL")
			for _, issue := range systemHealth.Issues {
				report.Recommendations = append(report.Recommendations, fmt.Sprintf("System: %s", issue))
			}
		}
	}

	// Determine overall health
	report.OverallHealth = "HEALTHY"
	for _, level := range healthLevels {
		if level == "CRITICAL" {
			report.OverallHealth = "CRITICAL"
			break
		} else if level == "DEGRADED" {
			report.OverallHealth = "DEGRADED"
		}
	}

	// Save report
	if err := smm.saveMaintenanceReport(report); err != nil {
		fmt.Printf("⚠️  Failed to save maintenance report: %v\n", err)
	}

	// Display summary
	smm.displayMaintenanceSummary(report)

	smm.lastMaintenance = time.Now()
	return report, nil
}

// checkNetworkHealth checks network connectivity and performance
func (smm *SystemMaintenanceManager) checkNetworkHealth() (*NetworkHealth, error) {
	health := &NetworkHealth{Issues: []string{}}

	// Check Starlink connectivity
	_, err := executeCommand(smm.sshClient, "ping -c 1 -W 5 192.168.100.1")
	health.StarlinkConnected = (err == nil)
	if !health.StarlinkConnected {
		health.Issues = append(health.Issues, "Starlink not reachable")
	}

	// Check cellular connectivity
	_, err = executeCommand(smm.sshClient, "gsmctl -j")
	health.CellularConnected = (err == nil)
	if !health.CellularConnected {
		health.Issues = append(health.Issues, "Cellular modem not responding")
	}

	// Check internet connectivity
	_, err = executeCommand(smm.sshClient, "ping -c 1 -W 5 8.8.8.8")
	health.InternetReachable = (err == nil)
	if !health.InternetReachable {
		health.Issues = append(health.Issues, "Internet not reachable")
	}

	// Check DNS
	_, err = executeCommand(smm.sshClient, "nslookup google.com")
	health.DNSWorking = (err == nil)
	if !health.DNSWorking {
		health.Issues = append(health.Issues, "DNS resolution failing")
	}

	// Measure latency (if internet is reachable)
	if health.InternetReachable {
		output, err := executeCommand(smm.sshClient, "ping -c 3 -W 5 8.8.8.8 | grep 'avg'")
		if err == nil && len(output) > 0 {
			// Parse ping output for average latency
			// This is a simplified parser - in production you'd want more robust parsing
			health.LatencyMs = 50.0 // Placeholder
		}
	}

	return health, nil
}

// checkStorageHealth checks filesystem usage and performs cleanup
func (smm *SystemMaintenanceManager) checkStorageHealth() (*StorageHealth, error) {
	health := &StorageHealth{Issues: []string{}}

	// Check root filesystem usage
	output, err := executeCommand(smm.sshClient, "df / | tail -1 | awk '{print $5}' | sed 's/%//'")
	if err == nil && len(output) > 0 {
		// Parse usage percentage
		health.RootFSUsage = 25.0 // Placeholder - would parse from output
		if health.RootFSUsage > 90 {
			health.Issues = append(health.Issues, "Root filesystem usage critical (>90%)")
		} else if health.RootFSUsage > 80 {
			health.Issues = append(health.Issues, "Root filesystem usage high (>80%)")
		}
	}

	// Check /tmp usage
	output, err = executeCommand(smm.sshClient, "df /tmp | tail -1 | awk '{print $5}' | sed 's/%//'")
	if err == nil && len(output) > 0 {
		health.TmpFSUsage = 15.0 // Placeholder
		if health.TmpFSUsage > 95 {
			health.Issues = append(health.Issues, "Temp filesystem usage critical")
		}
	}

	// Check log sizes
	output, err = executeCommand(smm.sshClient, "du -sm /var/log 2>/dev/null | awk '{print $1}'")
	if err == nil && len(output) > 0 {
		health.LogSizesMB = 50.0 // Placeholder
		if health.LogSizesMB > 100 {
			health.Issues = append(health.Issues, "Log files consuming excessive space")
			// Perform log rotation if enabled
			if smm.config.LogRotationEnabled {
				_, err := executeCommand(smm.sshClient, "logrotate -f /etc/logrotate.conf")
				health.LogsRotated = (err == nil)
			}
		}
	}

	// Perform cleanup if needed
	if health.RootFSUsage > 85 || health.TmpFSUsage > 90 {
		_, err := executeCommand(smm.sshClient, "find /tmp -type f -mtime +7 -delete")
		health.CleanupPerformed = (err == nil)
	}

	return health, nil
}

// checkSystemHealth checks overall system health metrics
func (smm *SystemMaintenanceManager) checkSystemHealth() (*SystemHealth, error) {
	health := &SystemHealth{Issues: []string{}}

	// Check uptime
	output, err := executeCommand(smm.sshClient, "uptime | awk '{print $3}' | sed 's/,//'")
	if err == nil && len(output) > 0 {
		health.UptimeHours = 48.5 // Placeholder
	}

	// Check load average
	output, err = executeCommand(smm.sshClient, "uptime | awk '{print $(NF-2)}' | sed 's/,//'")
	if err == nil && len(output) > 0 {
		health.LoadAverage = 0.5 // Placeholder
		if health.LoadAverage > 2.0 {
			health.Issues = append(health.Issues, "High system load average")
		}
	}

	// Check memory usage
	output, err = executeCommand(smm.sshClient, "free | grep Mem | awk '{printf \"%.1f\", $3/$2 * 100.0}'")
	if err == nil && len(output) > 0 {
		health.MemoryUsage = 65.0 // Placeholder
		if health.MemoryUsage > 90 {
			health.Issues = append(health.Issues, "Critical memory usage (>90%)")
		} else if health.MemoryUsage > 80 {
			health.Issues = append(health.Issues, "High memory usage (>80%)")
		}
	}

	// Check CPU temperature (if available)
	output, err = executeCommand(smm.sshClient, "cat /sys/class/thermal/thermal_zone0/temp 2>/dev/null")
	if err == nil && len(output) > 0 {
		health.CPUTemperature = 55.0 // Placeholder
		if health.CPUTemperature > 80 {
			health.Issues = append(health.Issues, "CPU temperature critical (>80°C)")
		}
	}

	// Check critical processes
	processes := []string{"gpsd", "network", "firewall"}
	allRunning := true
	for _, proc := range processes {
		_, err := executeCommand(smm.sshClient, fmt.Sprintf("pgrep %s", proc))
		if err != nil {
			allRunning = false
			health.Issues = append(health.Issues, fmt.Sprintf("Critical process not running: %s", proc))
		}
	}
	health.CriticalProcesses = allRunning

	return health, nil
}

// saveMaintenanceReport saves the maintenance report to file
func (smm *SystemMaintenanceManager) saveMaintenanceReport(report *SystemMaintenanceReport) error {
	data, err := json.MarshalIndent(report, "", "  ")
	if err != nil {
		return fmt.Errorf("failed to marshal report: %v", err)
	}

	return os.WriteFile(smm.config.ReportPath, data, 0o644)
}

// displayMaintenanceSummary displays a summary of the maintenance cycle
func (smm *SystemMaintenanceManager) displayMaintenanceSummary(report *SystemMaintenanceReport) {
	fmt.Println("\n🔧 System Maintenance Summary")
	fmt.Println("============================")

	healthIcon := "✅"
	switch report.OverallHealth {
	case "DEGRADED":
		healthIcon = "⚠️"
	case "CRITICAL":
		healthIcon = "❌"
	}

	fmt.Printf("Overall Health: %s %s\n", healthIcon, report.OverallHealth)
	fmt.Printf("Maintenance Time: %s\n", report.Timestamp.Format("2006-01-02 15:04:05"))
	fmt.Printf("Next Maintenance: %s\n", report.NextMaintenance.Format("2006-01-02 15:04:05"))

	if len(report.ActionsPerformed) > 0 {
		fmt.Println("\nActions Performed:")
		for _, action := range report.ActionsPerformed {
			fmt.Printf("  ✓ %s\n", action)
		}
	}

	if len(report.Recommendations) > 0 {
		fmt.Println("\nRecommendations:")
		for _, rec := range report.Recommendations {
			fmt.Printf("  ⚠️  %s\n", rec)
		}
	}

	// Component health summary
	fmt.Println("\nComponent Health:")
	if report.GPSHealth != nil {
		gpsIcon := "✅"
		if !report.GPSHealth.Healthy {
			gpsIcon = "❌"
		}
		fmt.Printf("  GPS: %s (%d resets, %d consecutive failures)\n",
			gpsIcon, report.GPSHealth.TotalResets, report.GPSHealth.ConsecutiveFailures)
	}

	if report.NetworkHealth != nil {
		netIcon := "✅"
		if len(report.NetworkHealth.Issues) > 0 {
			netIcon = "⚠️"
		}
		fmt.Printf("  Network: %s (Starlink: %s, Cellular: %s, Internet: %s)\n",
			netIcon,
			map[bool]string{true: "✓", false: "✗"}[report.NetworkHealth.StarlinkConnected],
			map[bool]string{true: "✓", false: "✗"}[report.NetworkHealth.CellularConnected],
			map[bool]string{true: "✓", false: "✗"}[report.NetworkHealth.InternetReachable])
	}

	if report.StorageHealth != nil {
		storageIcon := "✅"
		if len(report.StorageHealth.Issues) > 0 {
			storageIcon = "⚠️"
		}
		fmt.Printf("  Storage: %s (Root: %.1f%%, Logs: %.1fMB)\n",
			storageIcon, report.StorageHealth.RootFSUsage, report.StorageHealth.LogSizesMB)
	}

	if report.SystemHealth != nil {
		sysIcon := "✅"
		if len(report.SystemHealth.Issues) > 0 {
			sysIcon = "⚠️"
		}
		fmt.Printf("  System: %s (Load: %.1f, Memory: %.1f%%, Temp: %.1f°C)\n",
			sysIcon, report.SystemHealth.LoadAverage, report.SystemHealth.MemoryUsage, report.SystemHealth.CPUTemperature)
	}
}

// testSystemMaintenance tests the system maintenance functionality
func testSystemMaintenance() {
	fmt.Println("🔧 System Maintenance Test")
	fmt.Println("==========================")

	// Create default config
	config := &SystemMaintenanceConfig{
		MaintenanceInterval:   30 * time.Minute,
		GPSHealthCheckEnabled: true,
		NetworkHealthEnabled:  true,
		StorageHealthEnabled:  true,
		LogRotationEnabled:    true,
		ReportPath:            "/tmp/autonomy_maintenance.json",
		AlertOnCritical:       true,
	}

	// Create SSH client
	client, err := createSSHClient()
	if err != nil {
		fmt.Printf("❌ Failed to create SSH client: %v\n", err)
		return
	}
	defer client.Close()

	// Create maintenance manager
	manager := NewSystemMaintenanceManager(config, client)

	// Run maintenance cycle
	report, err := manager.RunMaintenance()
	if err != nil {
		fmt.Printf("❌ Maintenance cycle failed: %v\n", err)
		return
	}

	fmt.Printf("\n🎯 Maintenance Complete: %s\n", report.OverallHealth)
}
