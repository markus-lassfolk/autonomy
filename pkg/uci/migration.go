package uci

import (
	"context"
	"fmt"
	"os"
	"os/exec"
	"path/filepath"
	"strings"
	"time"

	"github.com/markus-lassfolk/autonomy/pkg/logx"
)

// MigrationManager handles configuration migrations between versions
type MigrationManager struct {
	logger *logx.Logger
	client *NativeUCI
}

// MigrationResult represents the result of a migration operation
type MigrationResult struct {
	Success     bool               `json:"success"`
	FromVersion string             `json:"from_version"`
	ToVersion   string             `json:"to_version"`
	Changes     []MigrationChange  `json:"changes,omitempty"`
	Errors      []MigrationError   `json:"errors,omitempty"`
	Warnings    []MigrationWarning `json:"warnings,omitempty"`
	BackupPath  string             `json:"backup_path,omitempty"`
}

// MigrationChange represents a single configuration change
type MigrationChange struct {
	Section  string `json:"section"`
	Option   string `json:"option"`
	OldValue string `json:"old_value"`
	NewValue string `json:"new_value"`
	Reason   string `json:"reason"`
}

// MigrationError represents a migration error
type MigrationError struct {
	Section string `json:"section"`
	Option  string `json:"option"`
	Message string `json:"message"`
}

// MigrationWarning represents a migration warning
type MigrationWarning struct {
	Section string `json:"section"`
	Option  string `json:"option"`
	Message string `json:"message"`
}

// NewMigrationManager creates a new migration manager
func NewMigrationManager(logger *logx.Logger, client *NativeUCI) *MigrationManager {
	return &MigrationManager{
		logger: logger,
		client: client,
	}
}

// MigrateConfiguration migrates configuration to the latest version
func (mm *MigrationManager) MigrateConfiguration(ctx context.Context) MigrationResult {
	result := MigrationResult{
		Success:  true,
		Changes:  []MigrationChange{},
		Errors:   []MigrationError{},
		Warnings: []MigrationWarning{},
	}

	// Get current version
	currentVersion, err := mm.getCurrentVersion(ctx)
	if err != nil {
		result.Success = false
		result.Errors = append(result.Errors, MigrationError{
			Message: fmt.Sprintf("Failed to get current version: %v", err),
		})
		return result
	}

	result.FromVersion = currentVersion
	result.ToVersion = "2.0.0" // Latest version

	// Create backup before migration
	backupPath, err := mm.createBackup(ctx)
	if err != nil {
		result.Success = false
		result.Errors = append(result.Errors, MigrationError{
			Message: fmt.Sprintf("Failed to create backup: %v", err),
		})
		return result
	}
	result.BackupPath = backupPath

	// Perform migrations based on current version
	switch currentVersion {
	case "1.0.0":
		mm.migrateFromV1ToV2(ctx, &result)
	case "1.1.0":
		mm.migrateFromV11ToV2(ctx, &result)
	case "1.2.0":
		mm.migrateFromV12ToV2(ctx, &result)
	default:
		// Already at latest version or unknown version
		if currentVersion != "2.0.0" {
			result.Warnings = append(result.Warnings, MigrationWarning{
				Message: fmt.Sprintf("Unknown version %s, attempting migration to 2.0.0", currentVersion),
			})
		}
	}

	// Update version to latest
	if result.Success {
		err = mm.setVersion(ctx, "2.0.0")
		if err != nil {
			result.Success = false
			result.Errors = append(result.Errors, MigrationError{
				Message: fmt.Sprintf("Failed to update version: %v", err),
			})
		}
	}

	return result
}

// migrateFromV1ToV2 migrates from version 1.0.0 to 2.0.0
func (mm *MigrationManager) migrateFromV1ToV2(ctx context.Context, result *MigrationResult) {
	mm.logger.Info("Migrating from version 1.0.0 to 2.0.0")

	// Add new required options with defaults
	newOptions := map[string]map[string]string{
		"main": {
			"performance_profiling": "0",
			"security_auditing":     "1",
			"profiling_enabled":     "0",
			"auditing_enabled":      "1",
			"max_failed_attempts":   "5",
			"block_duration":        "300",
		},
		"gps": {
			"movement_detection":            "1",
			"location_clustering":           "1",
			"google_elevation_api_enabled":  "1",
			"hybrid_prioritization":         "1",
			"min_acceptable_confidence":     "0.5",
			"fallback_confidence_threshold": "0.7",
		},
		"ml": {
			"enabled":    "1",
			"model_path": "/etc/autonomy/models.json",
			"training":   "1",
			"prediction": "1",
		},
		"monitoring": {
			"broker": "",
			"topic":  "autonomy/status",
		},
		"metered": {
			"enabled":            "0",
			"warning_threshold":  "80",
			"critical_threshold": "95",
			"hysteresis_margin":  "5",
			"stability_delay":    "300",
			"reconnect_method":   "gentle",
			"debug":              "0",
		},
	}

	for section, options := range newOptions {
		for option, defaultValue := range options {
			// Check if option already exists
			_, err := mm.client.Get(ctx, "autonomy", section, option)
			if err != nil {
				// Option doesn't exist, add it
				err = mm.client.Set(ctx, "autonomy", section, option, defaultValue)
				if err != nil {
					result.Errors = append(result.Errors, MigrationError{
						Section: section,
						Option:  option,
						Message: fmt.Sprintf("Failed to add option: %v", err),
					})
					result.Success = false
				} else {
					result.Changes = append(result.Changes, MigrationChange{
						Section:  section,
						Option:   option,
						OldValue: "",
						NewValue: defaultValue,
						Reason:   "Added new required option",
					})
				}
			}
		}
	}

	// Update existing options that have changed
	optionUpdates := map[string]map[string]string{
		"main": {
			"log_level": "info", // Ensure log level is set
		},
		"starlink": {
			"grpc_first": "1", // Default to gRPC first
			"http_first": "0",
		},
	}

	for section, options := range optionUpdates {
		for option, newValue := range options {
			oldValue, err := mm.client.Get(ctx, "autonomy", section, option)
			if err == nil && oldValue != newValue {
				err = mm.client.Set(ctx, "autonomy", section, option, newValue)
				if err != nil {
					result.Errors = append(result.Errors, MigrationError{
						Section: section,
						Option:  option,
						Message: fmt.Sprintf("Failed to update option: %v", err),
					})
					result.Success = false
				} else {
					result.Changes = append(result.Changes, MigrationChange{
						Section:  section,
						Option:   option,
						OldValue: oldValue,
						NewValue: newValue,
						Reason:   "Updated default value",
					})
				}
			}
		}
	}

	// Commit changes
	if result.Success {
		err := mm.client.Commit(ctx, "autonomy")
		if err != nil {
			result.Errors = append(result.Errors, MigrationError{
				Message: fmt.Sprintf("Failed to commit changes: %v", err),
			})
			result.Success = false
		}
	}
}

// migrateFromV11ToV2 migrates from version 1.1.0 to 2.0.0
func (mm *MigrationManager) migrateFromV11ToV2(ctx context.Context, result *MigrationResult) {
	mm.logger.Info("Migrating from version 1.1.0 to 2.0.0")

	// Add missing options from v1.1.0 to v2.0.0
	newOptions := map[string]map[string]string{
		"gps": {
			"google_elevation_api_enabled":  "1",
			"hybrid_prioritization":         "1",
			"min_acceptable_confidence":     "0.5",
			"fallback_confidence_threshold": "0.7",
		},
		"ml": {
			"enabled":    "1",
			"model_path": "/etc/autonomy/models.json",
			"training":   "1",
			"prediction": "1",
		},
	}

	for section, options := range newOptions {
		for option, defaultValue := range options {
			_, err := mm.client.Get(ctx, "autonomy", section, option)
			if err != nil {
				err = mm.client.Set(ctx, "autonomy", section, option, defaultValue)
				if err != nil {
					result.Errors = append(result.Errors, MigrationError{
						Section: section,
						Option:  option,
						Message: fmt.Sprintf("Failed to add option: %v", err),
					})
					result.Success = false
				} else {
					result.Changes = append(result.Changes, MigrationChange{
						Section:  section,
						Option:   option,
						OldValue: "",
						NewValue: defaultValue,
						Reason:   "Added new required option",
					})
				}
			}
		}
	}

	// Commit changes
	if result.Success {
		err := mm.client.Commit(ctx, "autonomy")
		if err != nil {
			result.Errors = append(result.Errors, MigrationError{
				Message: fmt.Sprintf("Failed to commit changes: %v", err),
			})
			result.Success = false
		}
	}
}

// migrateFromV12ToV2 migrates from version 1.2.0 to 2.0.0
func (mm *MigrationManager) migrateFromV12ToV2(ctx context.Context, result *MigrationResult) {
	mm.logger.Info("Migrating from version 1.2.0 to 2.0.0")

	// Add missing options from v1.2.0 to v2.0.0
	newOptions := map[string]map[string]string{
		"gps": {
			"hybrid_prioritization":         "1",
			"min_acceptable_confidence":     "0.5",
			"fallback_confidence_threshold": "0.7",
		},
	}

	for section, options := range newOptions {
		for option, defaultValue := range options {
			_, err := mm.client.Get(ctx, "autonomy", section, option)
			if err != nil {
				err = mm.client.Set(ctx, "autonomy", section, option, defaultValue)
				if err != nil {
					result.Errors = append(result.Errors, MigrationError{
						Section: section,
						Option:  option,
						Message: fmt.Sprintf("Failed to add option: %v", err),
					})
					result.Success = false
				} else {
					result.Changes = append(result.Changes, MigrationChange{
						Section:  section,
						Option:   option,
						OldValue: "",
						NewValue: defaultValue,
						Reason:   "Added new required option",
					})
				}
			}
		}
	}

	// Commit changes
	if result.Success {
		err := mm.client.Commit(ctx, "autonomy")
		if err != nil {
			result.Errors = append(result.Errors, MigrationError{
				Message: fmt.Sprintf("Failed to commit changes: %v", err),
			})
			result.Success = false
		}
	}
}

// getCurrentVersion gets the current configuration version
func (mm *MigrationManager) getCurrentVersion(ctx context.Context) (string, error) {
	version, err := mm.client.Get(ctx, "autonomy", "main", "version")
	if err != nil {
		// If version doesn't exist, assume it's v1.0.0
		return "1.0.0", nil
	}
	return version, nil
}

// setVersion sets the configuration version
func (mm *MigrationManager) setVersion(ctx context.Context, version string) error {
	return mm.client.Set(ctx, "autonomy", "main", "version", version)
}

// createBackup creates a backup of the current configuration
func (mm *MigrationManager) createBackup(ctx context.Context) (string, error) {
	backupDir := "/etc/autonomy/backups"
	if err := os.MkdirAll(backupDir, 0o755); err != nil {
		return "", fmt.Errorf("failed to create backup directory: %w", err)
	}

	timestamp := time.Now().Format("20060102_150405")
	backupPath := filepath.Join(backupDir, fmt.Sprintf("autonomy_backup_%s.tar.gz", timestamp))

	// Create backup using tar
	cmd := exec.Command("tar", "-czf", backupPath, "-C", "/etc/config", "autonomy")
	if err := cmd.Run(); err != nil {
		return "", fmt.Errorf("failed to create backup: %w", err)
	}

	mm.logger.Info("Configuration backup created", "path", backupPath)
	return backupPath, nil
}

// RestoreBackup restores configuration from a backup
func (mm *MigrationManager) RestoreBackup(ctx context.Context, backupPath string) error {
	if _, err := os.Stat(backupPath); os.IsNotExist(err) {
		return fmt.Errorf("backup file does not exist: %s", backupPath)
	}

	// Extract backup
	cmd := exec.Command("tar", "-xzf", backupPath, "-C", "/etc/config")
	if err := cmd.Run(); err != nil {
		return fmt.Errorf("failed to restore backup: %w", err)
	}

	mm.logger.Info("Configuration restored from backup", "path", backupPath)
	return nil
}

// ValidateMigration validates that a migration can be performed
func (mm *MigrationManager) ValidateMigration(ctx context.Context) (bool, []string) {
	var issues []string

	// Check if configuration exists
	_, err := mm.client.Get(ctx, "autonomy", "main", "enable")
	if err != nil {
		issues = append(issues, "Configuration file does not exist")
		return false, issues
	}

	// Check for required sections
	requiredSections := []string{"main", "starlink"}
	for _, section := range requiredSections {
		_, err := mm.client.Get(ctx, "autonomy", section, "enable")
		if err != nil {
			issues = append(issues, fmt.Sprintf("Required section '%s' is missing", section))
		}
	}

	// Check for critical options
	criticalOptions := map[string]string{
		"main.enable":   "1",
		"starlink.host": "192.168.100.1",
		"starlink.port": "9200",
	}

	for option, expectedValue := range criticalOptions {
		parts := strings.Split(option, ".")
		if len(parts) != 2 {
			continue
		}
		section, opt := parts[0], parts[1]

		value, err := mm.client.Get(ctx, "autonomy", section, opt)
		if err != nil {
			issues = append(issues, fmt.Sprintf("Critical option '%s' is missing", option))
		} else if value != expectedValue {
			issues = append(issues, fmt.Sprintf("Critical option '%s' has unexpected value: %s (expected: %s)",
				option, value, expectedValue))
		}
	}

	return len(issues) == 0, issues
}

// RollbackMigration rolls back a failed migration
func (mm *MigrationManager) RollbackMigration(ctx context.Context, backupPath string) error {
	if backupPath == "" {
		return fmt.Errorf("no backup path provided for rollback")
	}

	mm.logger.Warn("Rolling back failed migration", "backup", backupPath)
	return mm.RestoreBackup(ctx, backupPath)
}
