package uci

import (
	"context"
	"fmt"
	"os/exec"
	"strings"
	"time"

	"github.com/markus-lassfolk/autonomy/pkg/logx"
)

// UCI represents a UCI client
type UCI struct {
	logger *logx.Logger
}

// NewUCI creates a new UCI client
func NewUCI(logger *logx.Logger) *UCI {
	return &UCI{
		logger: logger,
	}
}

// LoadConfig loads the complete autonomy configuration from UCI
func (u *UCI) LoadConfig(ctx context.Context) (*Config, error) {
	cfg := &Config{
		Members: make(map[string]*MemberConfig),
	}

	// Set defaults first
	cfg.setDefaults()

	// Load main configuration
	if err := u.loadMainConfig(ctx, cfg); err != nil {
		return nil, fmt.Errorf("failed to load main config: %w", err)
	}

	// Load member configurations
	if err := u.loadMemberConfigs(ctx, cfg); err != nil {
		return nil, fmt.Errorf("failed to load member configs: %w", err)
	}

	// Validate configuration
	if err := cfg.validate(); err != nil {
		return nil, fmt.Errorf("configuration validation failed: %w", err)
	}

	return cfg, nil
}

// loadMainConfig loads the main autonomy configuration section
func (u *UCI) loadMainConfig(ctx context.Context, cfg *Config) error {
	// Get all options from autonomy.main
	output, err := u.execUCI(ctx, "get", "autonomy.main")
	if err != nil {
		// If autonomy.main doesn't exist, return with defaults
		return nil
	}

	lines := strings.Split(strings.TrimSpace(output), "\n")
	for _, line := range lines {
		if line == "" {
			continue
		}

		// Parse option=value format
		parts := strings.SplitN(line, "=", 2)
		if len(parts) != 2 {
			continue
		}

		option := parts[0]
		value := strings.Trim(parts[1], "'\"")

		// Parse the option
		cfg.parseMainOption(option, value)
	}

	return nil
}

// loadMemberConfigs loads all member configuration sections and other sections like GPS
func (u *UCI) loadMemberConfigs(ctx context.Context, cfg *Config) error {
	// Get all sections
	output, err := u.execUCI(ctx, "show", "autonomy")
	if err != nil {
		// If no autonomy config exists, return with defaults
		return nil
	}

	lines := strings.Split(strings.TrimSpace(output), "\n")
	var currentSection string
	var currentSectionType string

	for _, line := range lines {
		if line == "" {
			continue
		}

		// Require an '=' to process the line
		if !strings.Contains(line, "=") {
			continue
		}

		parts := strings.SplitN(line, "=", 2)
		left := parts[0]
		right := strings.Trim(parts[1], "'\"")
		leftParts := strings.Split(left, ".")

		// Section definition lines have exactly two parts: autonomy.@type[index]
		if len(leftParts) == 2 && strings.Contains(leftParts[1], "@") {
			// Extract section type from @type[index]
			typePart := strings.TrimPrefix(leftParts[1], "@")
			if bracketIndex := strings.Index(typePart, "["); bracketIndex != -1 {
				typePart = typePart[:bracketIndex]
			}
			currentSectionType = typePart
			currentSection = right // For member sections this is the name

			// Initialize member map entry when encountering a member definition
			if currentSectionType == "member" {
				cfg.Members[currentSection] = &MemberConfig{
					Detect:     "auto",
					Weight:     50,
					MinUptimeS: cfg.MinUptimeS,
					CooldownS:  cfg.CooldownS,
				}
			}
			continue
		}

		// Option lines have at least three parts: autonomy.@type[index].option
		if len(leftParts) >= 3 {
			optionName := leftParts[2]

			// Determine section type from @type[index]
			typePart := leftParts[1]
			if strings.HasPrefix(typePart, "@") {
				typePart = strings.TrimPrefix(typePart, "@")
				if bracketIndex := strings.Index(typePart, "["); bracketIndex != -1 {
					typePart = typePart[:bracketIndex]
				}
			}

			switch typePart {
			case "member":
				if currentSection != "" {
					cfg.parseMemberOption(currentSection, optionName, right)
				}
			case "gps":
				// Use a fixed section name for gps settings
				cfg.parseGPSOption("settings", optionName, right)
			default:
				// ignore other types for now
			}
		}
	}

	return nil
}

// SetOption sets a UCI option value
func (u *UCI) SetOption(ctx context.Context, section, option, value string) error {
	_, err := u.execUCI(ctx, "set", fmt.Sprintf("autonomy.%s.%s=%s", section, option, value))
	return err
}

// DeleteOption deletes a UCI option
func (u *UCI) DeleteOption(ctx context.Context, section, option string) error {
	_, err := u.execUCI(ctx, "delete", fmt.Sprintf("autonomy.%s.%s", section, option))
	return err
}

// Commit commits pending UCI changes
func (u *UCI) Commit(ctx context.Context) error {
	_, err := u.execUCI(ctx, "commit", "autonomy")
	return err
}

// Revert reverts pending UCI changes
func (u *UCI) Revert(ctx context.Context) error {
	_, err := u.execUCI(ctx, "revert", "autonomy")
	return err
}

// AddSection adds a new UCI section
func (u *UCI) AddSection(ctx context.Context, sectionType, sectionName string) error {
	_, err := u.execUCI(ctx, "add", "autonomy", sectionType)
	if err != nil {
		return err
	}

	// Set the section name if provided
	if sectionName != "" {
		return u.SetOption(ctx, sectionName, "name", sectionName)
	}

	return nil
}

// DeleteSection deletes a UCI section
func (u *UCI) DeleteSection(ctx context.Context, sectionName string) error {
	_, err := u.execUCI(ctx, "delete", fmt.Sprintf("autonomy.%s", sectionName))
	return err
}

// GetSections returns all sections of a given type
func (u *UCI) GetSections(ctx context.Context, sectionType string) ([]string, error) {
	output, err := u.execUCI(ctx, "show", "autonomy")
	if err != nil {
		return nil, err
	}

	var sections []string
	lines := strings.Split(strings.TrimSpace(output), "\n")
	for _, line := range lines {
		if strings.Contains(line, "="+sectionType) {
			parts := strings.Split(line, "=")
			if len(parts) >= 2 {
				sectionParts := strings.Split(parts[0], ".")
				if len(sectionParts) >= 2 {
					sections = append(sections, sectionParts[1])
				}
			}
		}
	}

	return sections, nil
}

// execUCI executes a UCI command
func (u *UCI) execUCI(ctx context.Context, args ...string) (string, error) {
	cmd := exec.CommandContext(ctx, "uci", args...)
	output, err := cmd.Output()
	if err != nil {
		if u.logger != nil {
			u.logger.Error("UCI command failed", "command", "uci "+strings.Join(args, " "), "error", err)
		}
		return "", fmt.Errorf("uci command failed: %w", err)
	}

	return string(output), nil
}

// ValidateUCI checks if UCI is available and working
func (u *UCI) ValidateUCI(ctx context.Context) error {
	_, err := u.execUCI(ctx, "version")
	if err != nil {
		return fmt.Errorf("UCI is not available: %w", err)
	}
	return nil
}

// BackupConfig creates a backup of the current configuration
func (u *UCI) BackupConfig(ctx context.Context) (string, error) {
	output, err := u.execUCI(ctx, "export", "autonomy")
	if err != nil {
		return "", fmt.Errorf("failed to export config: %w", err)
	}
	return output, nil
}

// RestoreConfig restores configuration from backup
func (u *UCI) RestoreConfig(ctx context.Context, backup string) error {
	// First revert any pending changes
	if err := u.Revert(ctx); err != nil {
		return fmt.Errorf("failed to revert before restore: %w", err)
	}

	// Import the backup
	_, err := u.execUCI(ctx, "import", backup)
	if err != nil {
		return fmt.Errorf("failed to import backup: %w", err)
	}

	// Commit the changes
	return u.Commit(ctx)
}

// GetConfigHash returns a hash of the current configuration for change detection
func (u *UCI) GetConfigHash(ctx context.Context) (string, error) {
	output, err := u.execUCI(ctx, "export", "autonomy")
	if err != nil {
		return "", err
	}

	// Simple hash - in production, use a proper hash function
	hash := fmt.Sprintf("%d", len(output))
	return hash, nil
}

// WatchConfig watches for configuration changes
func (u *UCI) WatchConfig(ctx context.Context, callback func()) error {
	initialHash, err := u.GetConfigHash(ctx)
	if err != nil {
		return err
	}

	ticker := time.NewTicker(5 * time.Second)
	defer ticker.Stop()

	for {
		select {
		case <-ctx.Done():
			return ctx.Err()
		case <-ticker.C:
			currentHash, err := u.GetConfigHash(ctx)
			if err != nil {
				if u.logger != nil {
					u.logger.Error("Failed to get config hash", "error", err)
				}
				continue
			}

			if currentHash != initialHash {
				if u.logger != nil {
					u.logger.Info("Configuration changed, triggering reload")
				}
				callback()
				initialHash = currentHash
			}
		}
	}
}
