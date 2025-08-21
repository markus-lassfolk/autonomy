package uci

import (
	"context"
	"fmt"
	"net"
	"os"
	"path/filepath"
	"strconv"
	"strings"

	"github.com/markus-lassfolk/autonomy/pkg/logx"
)

// ConfigValidator provides comprehensive configuration validation
type ConfigValidator struct {
	logger *logx.Logger
}

// NewConfigValidator creates a new configuration validator
func NewConfigValidator(logger *logx.Logger) *ConfigValidator {
	return &ConfigValidator{
		logger: logger,
	}
}

// ValidationResult represents the result of configuration validation
type ValidationResult struct {
	Valid    bool                `json:"valid"`
	Errors   []ValidationError   `json:"errors,omitempty"`
	Warnings []ValidationWarning `json:"warnings,omitempty"`
	Summary  ValidationSummary   `json:"summary"`
}

// ValidationError represents a configuration validation error
type ValidationError struct {
	Section string `json:"section"`
	Option  string `json:"option"`
	Value   string `json:"value"`
	Message string `json:"message"`
	Line    int    `json:"line,omitempty"`
}

// ValidationWarning represents a configuration validation warning
type ValidationWarning struct {
	Section string `json:"section"`
	Option  string `json:"option"`
	Value   string `json:"value"`
	Message string `json:"message"`
	Line    int    `json:"line,omitempty"`
}

// ValidationSummary provides a summary of validation results
type ValidationSummary struct {
	TotalErrors   int `json:"total_errors"`
	TotalWarnings int `json:"total_warnings"`
	TotalOptions  int `json:"total_options"`
	ValidOptions  int `json:"valid_options"`
}

// ValidateConfiguration validates the entire autonomy configuration
func (v *ConfigValidator) ValidateConfiguration(ctx context.Context, config *Config) ValidationResult {
	result := ValidationResult{
		Valid:    true,
		Errors:   []ValidationError{},
		Warnings: []ValidationWarning{},
	}

	// Validate main section
	v.validateMainSection(config, &result)

	// Validate GPS section
	v.validateGPSSection(config, &result)

	// Validate Starlink section
	v.validateStarlinkSection(config, &result)

	// Validate ML section
	v.validateMLSection(config, &result)

	// Validate monitoring section
	v.validateMonitoringSection(config, &result)

	// Validate metered section
	v.validateMeteredSection(config, &result)

	// Validate notifications section
	v.validateNotificationsSection(config, &result)

	// Validate security section
	v.validateSecuritySection(config, &result)

	// Validate thresholds section
	v.validateThresholdsSection(config, &result)

	// Calculate summary
	result.Summary = v.calculateSummary(result)
	result.Valid = len(result.Errors) == 0

	return result
}

// validateMainSection validates the main configuration section
func (v *ConfigValidator) validateMainSection(config *Config, result *ValidationResult) {
	section := "main"

	// Validate boolean options
	v.validateBooleanOption(section, "enable", config.Enable, result)
	v.validateBooleanOption(section, "use_mwan3", config.UseMWAN3, result)
	v.validateBooleanOption(section, "predictive", config.Predictive, result)
	v.validateBooleanOption(section, "metrics_listener", config.MetricsListener, result)
	v.validateBooleanOption(section, "health_listener", config.HealthListener, result)
	v.validateBooleanOption(section, "performance_profiling", config.PerformanceProfiling, result)
	v.validateBooleanOption(section, "security_auditing", config.SecurityAuditing, result)

	// Validate integer options
	v.validateIntegerRange(section, "poll_interval_ms", config.PollIntervalMS, 100, 10000, result)
	v.validateIntegerRange(section, "decision_interval_ms", config.DecisionIntervalMS, 100, 10000, result)
	v.validateIntegerRange(section, "discovery_interval_ms", config.DiscoveryIntervalMS, 1000, 60000, result)
	v.validateIntegerRange(section, "cleanup_interval_ms", config.CleanupIntervalMS, 1000, 60000, result)
	v.validateIntegerRange(section, "history_window_s", config.HistoryWindowS, 60, 3600, result)
	v.validateIntegerRange(section, "retention_hours", config.RetentionHours, 1, 168, result)
	v.validateIntegerRange(section, "max_ram_mb", config.MaxRAMMB, 8, 512, result)
	v.validateIntegerRange(section, "switch_margin", config.SwitchMargin, 0, 100, result)
	v.validateIntegerRange(section, "min_uptime_s", config.MinUptimeS, 5, 300, result)
	v.validateIntegerRange(section, "cooldown_s", config.CooldownS, 5, 300, result)

	// Validate string options
	v.validateLogLevel(section, "log_level", config.LogLevel, result)
	v.validateDataCapMode(section, "data_cap_mode", config.DataCapMode, result)

	// Validate file paths
	if config.LogFile != "" {
		v.validateFilePath(section, "log_file", config.LogFile, result)
	}
}

// validateGPSSection validates the GPS configuration section
func (v *ConfigValidator) validateGPSSection(config *Config, result *ValidationResult) {
	section := "gps"

	// Validate boolean options
	v.validateBooleanOption(section, "enabled", config.GPSEnabled, result)
	v.validateBooleanOption(section, "movement_detection", config.GPSMovementDetection, result)
	v.validateBooleanOption(section, "location_clustering", config.GPSLocationClustering, result)
	v.validateBooleanOption(section, "google_api_enabled", config.GPSGoogleAPIEnabled, result)
	v.validateBooleanOption(section, "google_elevation_api_enabled", config.GPSGoogleElevationAPIEnabled, result)
	v.validateBooleanOption(section, "hybrid_prioritization", config.GPSHybridPrioritization, result)

	// Validate float options
	v.validateFloatRange(section, "movement_threshold_m", config.GPSMovementThresholdM, 10.0, 10000.0, result)
	v.validateFloatRange(section, "accuracy_threshold_m", config.GPSAccuracyThresholdM, 1.0, 1000.0, result)
	v.validateIntegerRange(section, "staleness_threshold_s", int(config.GPSStalenessThresholdS), 30, 3600, result)
	v.validateIntegerRange(section, "collection_interval_s", config.GPSCollectionIntervalS, 5, 300, result)
	v.validateIntegerRange(section, "retry_attempts", config.GPSRetryAttempts, 1, 10, result)
	v.validateIntegerRange(section, "retry_delay_s", config.GPSRetryDelayS, 1, 60, result)

	// Validate float options
	v.validateFloatRange(section, "min_acceptable_confidence", config.GPSMinAcceptableConfidence, 0.0, 1.0, result)
	v.validateFloatRange(section, "fallback_confidence_threshold", config.GPSFallbackConfidenceThreshold, 0.0, 1.0, result)

	// Validate source priority
	if len(config.GPSSourcePriority) > 0 {
		v.validateSourcePriority(section, "source_priority", strings.Join(config.GPSSourcePriority, ","), result)
	}

	// Validate API key if Google API is enabled
	if config.GPSGoogleAPIEnabled && config.GPSGoogleAPIKey == "" {
		result.Errors = append(result.Errors, ValidationError{
			Section: section,
			Option:  "google_api_key",
			Value:   "",
			Message: "Google API key is required when Google API is enabled",
		})
	}
}

// validateStarlinkSection validates the Starlink configuration section
func (v *ConfigValidator) validateStarlinkSection(config *Config, result *ValidationResult) {
	section := "starlink"

	// Validate host
	v.validateHost(section, "host", config.StarlinkAPIHost, result)

	// Validate port
	v.validateIntegerRange(section, "port", config.StarlinkAPIPort, 1, 65535, result)

	// Validate timeout
	v.validateIntegerRange(section, "timeout_s", config.StarlinkTimeout, 1, 60, result)

	// Validate boolean options
	v.validateBooleanOption(section, "grpc_first", config.StarlinkGRPCFirst, result)
	v.validateBooleanOption(section, "http_first", config.StarlinkHTTPFirst, result)

	// Validate that only one protocol is set as first
	if config.StarlinkGRPCFirst && config.StarlinkHTTPFirst {
		result.Errors = append(result.Errors, ValidationError{
			Section: section,
			Option:  "grpc_first/http_first",
			Value:   "both true",
			Message: "Only one protocol can be set as first (grpc_first or http_first)",
		})
	}
}

// validateMLSection validates the ML configuration section
func (v *ConfigValidator) validateMLSection(config *Config, result *ValidationResult) {
	section := "ml"

	// Validate boolean options
	v.validateBooleanOption(section, "enabled", config.MLEnabled, result)
	v.validateBooleanOption(section, "training", config.MLTraining, result)
	v.validateBooleanOption(section, "prediction", config.MLPrediction, result)

	// Validate model path if ML is enabled
	if config.MLEnabled && config.MLModelPath != "" {
		v.validateFilePath(section, "model_path", config.MLModelPath, result)
	}
}

// validateMonitoringSection validates the monitoring configuration section
func (v *ConfigValidator) validateMonitoringSection(config *Config, result *ValidationResult) {
	section := "monitoring"

	// Validate MQTT broker if specified
	if config.MQTTBroker != "" {
		v.validateMQTTBroker(section, "broker", config.MQTTBroker, result)
	}

	// Validate topic
	if config.MQTTTopic != "" {
		v.validateMQTTTopic(section, "topic", config.MQTTTopic, result)
	}
}

// validateMeteredSection validates the metered configuration section
func (v *ConfigValidator) validateMeteredSection(config *Config, result *ValidationResult) {
	section := "metered"

	// Validate boolean options
	v.validateBooleanOption(section, "enabled", config.MeteredModeEnabled, result)
	v.validateBooleanOption(section, "debug", config.MeteredModeDebug, result)

	// Validate integer options
	v.validateIntegerRange(section, "warning_threshold", config.DataLimitWarningThreshold, 1, 100, result)
	v.validateIntegerRange(section, "critical_threshold", config.DataLimitCriticalThreshold, 1, 100, result)
	v.validateIntegerRange(section, "hysteresis_margin", config.DataUsageHysteresisMargin, 1, 20, result)
	v.validateIntegerRange(section, "stability_delay", config.MeteredStabilityDelay, 30, 3600, result)

	// Validate reconnect method
	v.validateReconnectMethod(section, "reconnect_method", config.MeteredClientReconnectMethod, result)

	// Validate threshold relationships
	if config.DataLimitWarningThreshold >= config.DataLimitCriticalThreshold {
		result.Errors = append(result.Errors, ValidationError{
			Section: section,
			Option:  "warning_threshold/critical_threshold",
			Value:   fmt.Sprintf("%d/%d", config.DataLimitWarningThreshold, config.DataLimitCriticalThreshold),
			Message: "Warning threshold must be less than critical threshold",
		})
	}
}

// validateNotificationsSection validates the notifications configuration section
func (v *ConfigValidator) validateNotificationsSection(config *Config, result *ValidationResult) {
	section := "notifications"

	// Validate boolean options
	v.validateBooleanOption(section, "acknowledgment_tracking", config.AcknowledgmentTracking, result)
	v.validateBooleanOption(section, "location_enabled", config.LocationEnabled, result)
	v.validateBooleanOption(section, "rich_context_enabled", config.RichContextEnabled, result)
	v.validateBooleanOption(section, "notify_on_failover", config.NotifyOnFailover, result)
	v.validateBooleanOption(section, "notify_on_failback", config.NotifyOnFailback, result)
	v.validateBooleanOption(section, "notify_on_member_down", config.NotifyOnMemberDown, result)
	v.validateBooleanOption(section, "notify_on_member_up", config.NotifyOnMemberUp, result)
	v.validateBooleanOption(section, "notify_on_predictive", config.NotifyOnPredictive, result)
	v.validateBooleanOption(section, "notify_on_critical", config.NotifyOnCritical, result)

	// Validate priority threshold
	v.validatePriorityThreshold(section, "priority_threshold", config.PriorityThreshold, result)
}

// validateSecuritySection validates the security configuration section
func (v *ConfigValidator) validateSecuritySection(config *Config, result *ValidationResult) {
	section := "security"

	// Validate integer options
	v.validateIntegerRange(section, "max_failed_attempts", config.MaxFailedAttempts, 1, 100, result)
	v.validateIntegerRange(section, "block_duration", config.BlockDuration, 60, 86400, result)

	// Validate IP addresses
	for i, ip := range config.AllowedIPs {
		v.validateIPAddress(section, fmt.Sprintf("allowed_ips[%d]", i), ip, result)
	}

	for i, ip := range config.BlockedIPs {
		v.validateIPAddress(section, fmt.Sprintf("blocked_ips[%d]", i), ip, result)
	}

	// Validate ports
	for i, port := range config.AllowedPorts {
		v.validateIntegerRange(section, fmt.Sprintf("allowed_ports[%d]", i), port, 1, 65535, result)
	}

	for i, port := range config.BlockedPorts {
		v.validateIntegerRange(section, fmt.Sprintf("blocked_ports[%d]", i), port, 1, 65535, result)
	}
}

// validateThresholdsSection validates the thresholds configuration section
func (v *ConfigValidator) validateThresholdsSection(config *Config, result *ValidationResult) {
	section := "thresholds"

	// Validate integer options
	v.validateIntegerRange(section, "fail_threshold_loss", config.FailThresholdLoss, 1, 100, result)
	v.validateIntegerRange(section, "fail_threshold_latency", config.FailThresholdLatency, 10, 10000, result)
	v.validateIntegerRange(section, "fail_min_duration_s", config.FailMinDurationS, 1, 300, result)
	v.validateIntegerRange(section, "restore_threshold_loss", config.RestoreThresholdLoss, 1, 100, result)
	v.validateIntegerRange(section, "restore_threshold_latency", config.RestoreThresholdLatency, 10, 10000, result)
	v.validateIntegerRange(section, "restore_min_duration_s", config.RestoreMinDurationS, 1, 300, result)

	// Validate threshold relationships
	if config.FailThresholdLoss <= config.RestoreThresholdLoss {
		result.Warnings = append(result.Warnings, ValidationWarning{
			Section: section,
			Option:  "fail_threshold_loss/restore_threshold_loss",
			Value:   fmt.Sprintf("%d/%d", config.FailThresholdLoss, config.RestoreThresholdLoss),
			Message: "Fail threshold should be higher than restore threshold for proper hysteresis",
		})
	}

	if config.FailThresholdLatency <= config.RestoreThresholdLatency {
		result.Warnings = append(result.Warnings, ValidationWarning{
			Section: section,
			Option:  "fail_threshold_latency/restore_threshold_latency",
			Value:   fmt.Sprintf("%d/%d", config.FailThresholdLatency, config.RestoreThresholdLatency),
			Message: "Fail threshold should be higher than restore threshold for proper hysteresis",
		})
	}
}

// Helper validation methods
func (v *ConfigValidator) validateBooleanOption(section, option string, value bool, result *ValidationResult) {
	// Boolean validation is handled by the type system, but we can add custom logic here
	result.Summary.TotalOptions++
	result.Summary.ValidOptions++
}

func (v *ConfigValidator) validateIntegerRange(section, option string, value int, min, max int, result *ValidationResult) {
	result.Summary.TotalOptions++
	if value < min || value > max {
		result.Errors = append(result.Errors, ValidationError{
			Section: section,
			Option:  option,
			Value:   strconv.Itoa(value),
			Message: fmt.Sprintf("Value must be between %d and %d", min, max),
		})
	} else {
		result.Summary.ValidOptions++
	}
}

func (v *ConfigValidator) validateFloatRange(section, option string, value float64, min, max float64, result *ValidationResult) {
	result.Summary.TotalOptions++
	if value < min || value > max {
		result.Errors = append(result.Errors, ValidationError{
			Section: section,
			Option:  option,
			Value:   fmt.Sprintf("%f", value),
			Message: fmt.Sprintf("Value must be between %f and %f", min, max),
		})
	} else {
		result.Summary.ValidOptions++
	}
}

func (v *ConfigValidator) validateLogLevel(section, option, value string, result *ValidationResult) {
	result.Summary.TotalOptions++
	validLevels := []string{"debug", "info", "warn", "error", "trace"}
	valid := false
	for _, level := range validLevels {
		if level == value {
			valid = true
			break
		}
	}
	if !valid {
		result.Errors = append(result.Errors, ValidationError{
			Section: section,
			Option:  option,
			Value:   value,
			Message: fmt.Sprintf("Log level must be one of %v", validLevels),
		})
	} else {
		result.Summary.ValidOptions++
	}
}

func (v *ConfigValidator) validateDataCapMode(section, option, value string, result *ValidationResult) {
	result.Summary.TotalOptions++
	validModes := []string{"balanced", "aggressive", "conservative"}
	valid := false
	for _, mode := range validModes {
		if mode == value {
			valid = true
			break
		}
	}
	if !valid {
		result.Errors = append(result.Errors, ValidationError{
			Section: section,
			Option:  option,
			Value:   value,
			Message: fmt.Sprintf("Data cap mode must be one of %v", validModes),
		})
	} else {
		result.Summary.ValidOptions++
	}
}

func (v *ConfigValidator) validateFilePath(section, option, value string, result *ValidationResult) {
	result.Summary.TotalOptions++
	if value == "" {
		result.Summary.ValidOptions++
		return
	}

	// Check if path is absolute
	if !filepath.IsAbs(value) {
		result.Errors = append(result.Errors, ValidationError{
			Section: section,
			Option:  option,
			Value:   value,
			Message: "File path must be absolute",
		})
		return
	}

	// Check if directory exists for new files
	dir := filepath.Dir(value)
	if _, err := os.Stat(dir); os.IsNotExist(err) {
		result.Warnings = append(result.Warnings, ValidationWarning{
			Section: section,
			Option:  option,
			Value:   value,
			Message: fmt.Sprintf("Directory does not exist: %s", dir),
		})
	}

	result.Summary.ValidOptions++
}

func (v *ConfigValidator) validateSourcePriority(section, option, value string, result *ValidationResult) {
	result.Summary.TotalOptions++
	if value == "" {
		result.Errors = append(result.Errors, ValidationError{
			Section: section,
			Option:  option,
			Value:   value,
			Message: "Source priority cannot be empty",
		})
		return
	}

	sources := strings.Split(value, ",")
	validSources := []string{"rutos", "starlink", "google", "opencellid"}

	for _, source := range sources {
		source = strings.TrimSpace(source)
		valid := false
		for _, validSource := range validSources {
			if source == validSource {
				valid = true
				break
			}
		}
		if !valid {
			result.Errors = append(result.Errors, ValidationError{
				Section: section,
				Option:  option,
				Value:   value,
				Message: fmt.Sprintf("Invalid source '%s'. Valid sources are: %v", source, validSources),
			})
			return
		}
	}

	result.Summary.ValidOptions++
}

func (v *ConfigValidator) validateHost(section, option, value string, result *ValidationResult) {
	result.Summary.TotalOptions++
	if value == "" {
		result.Errors = append(result.Errors, ValidationError{
			Section: section,
			Option:  option,
			Value:   value,
			Message: "Host cannot be empty",
		})
		return
	}

	// Try to resolve as IP address first
	if ip := net.ParseIP(value); ip != nil {
		result.Summary.ValidOptions++
		return
	}

	// Try to resolve as hostname
	if _, err := net.LookupHost(value); err != nil {
		result.Warnings = append(result.Warnings, ValidationWarning{
			Section: section,
			Option:  option,
			Value:   value,
			Message: fmt.Sprintf("Unable to resolve hostname: %s", err.Error()),
		})
	}

	result.Summary.ValidOptions++
}

func (v *ConfigValidator) validateIPAddress(section, option, value string, result *ValidationResult) {
	result.Summary.TotalOptions++
	if value == "" {
		result.Summary.ValidOptions++
		return
	}

	if ip := net.ParseIP(value); ip == nil {
		result.Errors = append(result.Errors, ValidationError{
			Section: section,
			Option:  option,
			Value:   value,
			Message: "Invalid IP address format",
		})
	} else {
		result.Summary.ValidOptions++
	}
}

func (v *ConfigValidator) validateMQTTBroker(section, option, value string, result *ValidationResult) {
	result.Summary.TotalOptions++
	if value == "" {
		result.Summary.ValidOptions++
		return
	}

	// Basic MQTT broker format validation
	if !strings.Contains(value, ":") {
		result.Errors = append(result.Errors, ValidationError{
			Section: section,
			Option:  option,
			Value:   value,
			Message: "MQTT broker must include port (e.g., localhost:1883)",
		})
	} else {
		result.Summary.ValidOptions++
	}
}

func (v *ConfigValidator) validateMQTTTopic(section, option, value string, result *ValidationResult) {
	result.Summary.TotalOptions++
	if value == "" {
		result.Summary.ValidOptions++
		return
	}

	// Basic MQTT topic validation
	if strings.Contains(value, "#") && !strings.HasSuffix(value, "#") {
		result.Errors = append(result.Errors, ValidationError{
			Section: section,
			Option:  option,
			Value:   value,
			Message: "MQTT wildcard '#' can only be used at the end of the topic",
		})
	} else {
		result.Summary.ValidOptions++
	}
}

func (v *ConfigValidator) validateReconnectMethod(section, option, value string, result *ValidationResult) {
	result.Summary.TotalOptions++
	validMethods := []string{"gentle", "aggressive", "immediate"}
	valid := false
	for _, method := range validMethods {
		if method == value {
			valid = true
			break
		}
	}
	if !valid {
		result.Errors = append(result.Errors, ValidationError{
			Section: section,
			Option:  option,
			Value:   value,
			Message: fmt.Sprintf("Reconnect method must be one of %v", validMethods),
		})
	} else {
		result.Summary.ValidOptions++
	}
}

func (v *ConfigValidator) validatePriorityThreshold(section, option, value string, result *ValidationResult) {
	result.Summary.TotalOptions++
	validThresholds := []string{"low", "medium", "high", "critical"}
	valid := false
	for _, threshold := range validThresholds {
		if threshold == value {
			valid = true
			break
		}
	}
	if !valid {
		result.Errors = append(result.Errors, ValidationError{
			Section: section,
			Option:  option,
			Value:   value,
			Message: fmt.Sprintf("Priority threshold must be one of %v", validThresholds),
		})
	} else {
		result.Summary.ValidOptions++
	}
}

func (v *ConfigValidator) calculateSummary(result ValidationResult) ValidationSummary {
	return ValidationSummary{
		TotalErrors:   len(result.Errors),
		TotalWarnings: len(result.Warnings),
		TotalOptions:  result.Summary.TotalOptions,
		ValidOptions:  result.Summary.ValidOptions,
	}
}
