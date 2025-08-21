package notifications

import (
	"fmt"
	"os"
	"strings"
	"time"

	"github.com/markus-lassfolk/autonomy/pkg"
	"github.com/markus-lassfolk/autonomy/pkg/logx"
)

// DefaultLocationProvider provides basic location context
type DefaultLocationProvider struct {
	logger       *logx.Logger
	lastLocation *LocationContext
}

// DefaultMetricsProvider provides basic metrics context
type DefaultMetricsProvider struct {
	logger *logx.Logger
}

// DefaultSystemProvider provides basic system context
type DefaultSystemProvider struct {
	logger *logx.Logger
}

// NewDefaultLocationProvider creates a new default location provider
func NewDefaultLocationProvider(logger *logx.Logger) *DefaultLocationProvider {
	return &DefaultLocationProvider{
		logger: logger,
	}
}

// NewDefaultMetricsProvider creates a new default metrics provider
func NewDefaultMetricsProvider(logger *logx.Logger) *DefaultMetricsProvider {
	return &DefaultMetricsProvider{
		logger: logger,
	}
}

// NewDefaultSystemProvider creates a new default system provider
func NewDefaultSystemProvider(logger *logx.Logger) *DefaultSystemProvider {
	return &DefaultSystemProvider{
		logger: logger,
	}
}

// GetCurrentLocation returns current location context
func (dlp *DefaultLocationProvider) GetCurrentLocation() (*LocationContext, error) {
	// In a real implementation, this would integrate with the GPS system
	// For now, return a mock location or the last known location

	if dlp.lastLocation != nil {
		// Update timestamp
		dlp.lastLocation.Timestamp = time.Now()
		return dlp.lastLocation, nil
	}

	// Return a default location (can be configured)
	return &LocationContext{
		Latitude:  0.0,
		Longitude: 0.0,
		Accuracy:  1000.0, // Low accuracy indicates this is not real GPS data
		Address:   "Unknown Location",
		Timezone:  "UTC",
		Timestamp: time.Now(),
		MovementInfo: &Movement{
			Speed:          0.0,
			Direction:      0.0,
			DistanceMoved:  0.0,
			IsStationary:   true,
			StationaryTime: 24 * time.Hour, // Assume stationary
		},
	}, nil
}

// GetLocationHistory returns location history
func (dlp *DefaultLocationProvider) GetLocationHistory(duration time.Duration) ([]LocationContext, error) {
	// In a real implementation, this would query the GPS history
	current, err := dlp.GetCurrentLocation()
	if err != nil {
		return nil, err
	}

	// Return just the current location for now
	return []LocationContext{*current}, nil
}

// SetLocation allows setting a location for testing purposes
func (dlp *DefaultLocationProvider) SetLocation(lat, lon, accuracy float64) {
	dlp.lastLocation = &LocationContext{
		Latitude:  lat,
		Longitude: lon,
		Accuracy:  accuracy,
		Timestamp: time.Now(),
		MovementInfo: &Movement{
			Speed:          0.0,
			IsStationary:   true,
			StationaryTime: time.Hour,
		},
	}
}

// GetCurrentMetrics returns current metrics for an interface
func (dmp *DefaultMetricsProvider) GetCurrentMetrics(interfaceName string) (*pkg.Metrics, error) {
	// In a real implementation, this would query the actual metrics collector
	// For now, return mock metrics

	latency := 50.0
	loss := 0.5
	jitter := 2.0

	return &pkg.Metrics{
		LatencyMS:   &latency,
		LossPercent: &loss,
		JitterMS:    &jitter,
		Timestamp:   time.Now(),
		CollectedAt: time.Now(),
	}, nil
}

// GetMetricsHistory returns metrics history for an interface
func (dmp *DefaultMetricsProvider) GetMetricsHistory(interfaceName string, duration time.Duration) ([]*pkg.Metrics, error) {
	// In a real implementation, this would query the metrics store
	current, err := dmp.GetCurrentMetrics(interfaceName)
	if err != nil {
		return nil, err
	}

	return []*pkg.Metrics{current}, nil
}

// GetSystemLoad returns current system load metrics
func (dmp *DefaultMetricsProvider) GetSystemLoad() (*SystemLoadMetrics, error) {
	// In a real implementation, this would read actual system metrics
	// For now, return mock data

	return &SystemLoadMetrics{
		CPUUsage:    25.5,
		MemoryUsage: 45.2,
		DiskUsage:   60.1,
		Temperature: 45.0,
		Uptime:      24 * time.Hour,
	}, nil
}

// GetSystemInfo returns general system information
func (dsp *DefaultSystemProvider) GetSystemInfo() (*SystemInfo, error) {
	hostname, _ := os.Hostname()

	return &SystemInfo{
		Hostname:     hostname,
		Model:        "RUTX50", // Default model
		Firmware:     "RUTOS_7.06.0",
		SerialNumber: "Unknown",
		Uptime:       24 * time.Hour,
		LocalTime:    time.Now(),
	}, nil
}

// GetNetworkTopology returns current network topology
func (dsp *DefaultSystemProvider) GetNetworkTopology() (*NetworkTopology, error) {
	// In a real implementation, this would query the actual network state
	// For now, return mock topology

	return &NetworkTopology{
		ActiveInterfaces: []InterfaceInfo{
			{
				Name:     "mob1s1a1",
				Type:     "cellular",
				Status:   "up",
				Provider: "Verizon",
			},
			{
				Name:     "wlan0",
				Type:     "wifi",
				Status:   "up",
				Provider: "Local WiFi",
			},
		},
		PrimaryInterface: "mob1s1a1",
		BackupInterfaces: []string{"wlan0"},
		TotalBandwidth:   100000000, // 100 Mbps
		DataUsage: &DataUsageInfo{
			TotalUsed:    15000000000, // 15 GB
			TotalLimit:   50000000000, // 50 GB
			UsagePercent: 30.0,
			ResetDate:    time.Now().AddDate(0, 1, -time.Now().Day()+1),
			DailyAverage: 500000000, // 500 MB
		},
	}, nil
}

// GetMaintenanceStatus returns current maintenance status
func (dsp *DefaultSystemProvider) GetMaintenanceStatus() (*MaintenanceStatus, error) {
	// Check if we're in a maintenance window (simple file-based check)
	if _, err := os.Stat("/tmp/maintenance_mode"); err == nil {
		return &MaintenanceStatus{
			InMaintenance:   true,
			MaintenanceType: "scheduled",
			StartTime:       time.Now().Add(-30 * time.Minute),
			EstimatedEnd:    time.Now().Add(30 * time.Minute),
			Description:     "Scheduled system maintenance",
		}, nil
	}

	return &MaintenanceStatus{
		InMaintenance: false,
	}, nil
}

// ContextualAlertFactory provides a factory for creating contextual alert managers with default providers
type ContextualAlertFactory struct {
	logger *logx.Logger
}

// NewContextualAlertFactory creates a new factory
func NewContextualAlertFactory(logger *logx.Logger) *ContextualAlertFactory {
	return &ContextualAlertFactory{
		logger: logger,
	}
}

// CreateWithDefaultProviders creates a contextual alert manager with default providers
func (caf *ContextualAlertFactory) CreateWithDefaultProviders(smartManager *SmartNotificationManager) *ContextualAlertManager {
	locationProvider := NewDefaultLocationProvider(caf.logger)
	metricsProvider := NewDefaultMetricsProvider(caf.logger)
	systemProvider := NewDefaultSystemProvider(caf.logger)

	return NewContextualAlertManager(
		smartManager,
		locationProvider,
		metricsProvider,
		systemProvider,
		caf.logger,
	)
}

// CreateWithCustomProviders creates a contextual alert manager with custom providers
func (caf *ContextualAlertFactory) CreateWithCustomProviders(
	smartManager *SmartNotificationManager,
	locationProvider LocationProvider,
	metricsProvider MetricsProvider,
	systemProvider SystemProvider,
) *ContextualAlertManager {
	return NewContextualAlertManager(
		smartManager,
		locationProvider,
		metricsProvider,
		systemProvider,
		caf.logger,
	)
}

// Helper functions for context providers

// ParseWeatherCondition converts weather data to a standardized format
func ParseWeatherCondition(condition string) string {
	condition = strings.ToLower(strings.TrimSpace(condition))

	switch {
	case strings.Contains(condition, "clear"):
		return "clear"
	case strings.Contains(condition, "cloud"):
		return "cloudy"
	case strings.Contains(condition, "rain"):
		return "rainy"
	case strings.Contains(condition, "snow"):
		return "snowy"
	case strings.Contains(condition, "storm"):
		return "stormy"
	case strings.Contains(condition, "fog"):
		return "foggy"
	default:
		return "unknown"
	}
}

// CalculateMovementSpeed calculates speed from distance and time
func CalculateMovementSpeed(distanceMeters float64, timeDuration time.Duration) float64 {
	if timeDuration <= 0 {
		return 0.0
	}

	return distanceMeters / timeDuration.Seconds() // m/s
}

// IsHighSpeedMovement determines if the device is moving at high speed
func IsHighSpeedMovement(speedMS float64) bool {
	// Consider > 5 m/s (18 km/h) as high speed movement
	return speedMS > 5.0
}

// FormatDuration formats a duration in a human-readable way
func FormatDuration(d time.Duration) string {
	if d < time.Minute {
		return fmt.Sprintf("%.0fs", d.Seconds())
	} else if d < time.Hour {
		return fmt.Sprintf("%.1fm", d.Minutes())
	} else if d < 24*time.Hour {
		return fmt.Sprintf("%.1fh", d.Hours())
	} else {
		return fmt.Sprintf("%.1fd", d.Hours()/24)
	}
}

// FormatBytes formats bytes in a human-readable way
func FormatBytes(bytes int64) string {
	const unit = 1024
	if bytes < unit {
		return fmt.Sprintf("%d B", bytes)
	}

	div, exp := int64(unit), 0
	for n := bytes / unit; n >= unit; n /= unit {
		div *= unit
		exp++
	}

	units := []string{"KB", "MB", "GB", "TB"}
	return fmt.Sprintf("%.1f %s", float64(bytes)/float64(div), units[exp])
}

// CalculateDataUsagePercent calculates usage percentage
func CalculateDataUsagePercent(used, limit int64) float64 {
	if limit <= 0 {
		return 0.0
	}
	return (float64(used) / float64(limit)) * 100.0
}
