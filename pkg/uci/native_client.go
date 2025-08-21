package uci

import (
	"context"
	"fmt"
	"os"
	"path/filepath"
	"strconv"
	"strings"
	"sync"
	"time"

	"github.com/markus-lassfolk/autonomy/pkg/logx"
)

// NativeUCI provides direct UCI configuration access without exec calls
type NativeUCI struct {
	configPath string
	logger     *logx.Logger
	cache      map[string]interface{}
	cacheMutex sync.RWMutex
	cacheTTL   time.Duration
}

// NewNativeUCI creates a new native UCI client
func NewNativeUCI(configPath string, logger *logx.Logger) *NativeUCI {
	return &NativeUCI{
		configPath: configPath,
		logger:     logger,
		cache:      make(map[string]interface{}),
		cacheTTL:   30 * time.Second, // Cache for 30 seconds
	}
}

// Get retrieves a UCI configuration value with caching
func (n *NativeUCI) Get(ctx context.Context, config, section, option string) (string, error) {
	cacheKey := fmt.Sprintf("%s.%s.%s", config, section, option)

	// Check cache first
	n.cacheMutex.RLock()
	if cached, exists := n.cache[cacheKey]; exists {
		if cachedValue, ok := cached.(cachedValue); ok && time.Since(cachedValue.timestamp) < n.cacheTTL {
			n.cacheMutex.RUnlock()
			return cachedValue.value, nil
		}
	}
	n.cacheMutex.RUnlock()

	// Read from file
	value, err := n.readConfigValue(config, section, option)
	if err != nil {
		return "", fmt.Errorf("failed to read config %s.%s.%s: %w", config, section, option, err)
	}

	// Cache the result
	n.cacheMutex.Lock()
	n.cache[cacheKey] = cachedValue{
		value:     value,
		timestamp: time.Now(),
	}
	n.cacheMutex.Unlock()

	return value, nil
}

// Set sets a UCI configuration value
func (n *NativeUCI) Set(ctx context.Context, config, section, option, value string) error {
	// Invalidate cache for this key
	cacheKey := fmt.Sprintf("%s.%s.%s", config, section, option)
	n.cacheMutex.Lock()
	delete(n.cache, cacheKey)
	n.cacheMutex.Unlock()

	// Write to file
	if err := n.writeConfigValue(config, section, option, value); err != nil {
		return fmt.Errorf("failed to write config %s.%s.%s: %w", config, section, option, err)
	}

	n.logger.Debug("UCI config set", "config", config, "section", section, "option", option, "value", value)
	return nil
}

// Commit commits pending UCI changes
func (n *NativeUCI) Commit(ctx context.Context, config string) error {
	// Clear cache for this config
	n.cacheMutex.Lock()
	for key := range n.cache {
		if strings.HasPrefix(key, config+".") {
			delete(n.cache, key)
		}
	}
	n.cacheMutex.Unlock()

	n.logger.Debug("UCI config committed", "config", config)
	return nil
}

// readConfigValue reads a configuration value directly from the UCI config file
func (n *NativeUCI) readConfigValue(config, section, option string) (string, error) {
	configFile := filepath.Join(n.configPath, config)

	data, err := os.ReadFile(configFile)
	if err != nil {
		return "", fmt.Errorf("failed to read config file %s: %w", configFile, err)
	}

	lines := strings.Split(string(data), "\n")
	var currentSection string
	var inTargetSection bool

	for _, line := range lines {
		line = strings.TrimSpace(line)

		// Skip comments and empty lines
		if line == "" || strings.HasPrefix(line, "#") {
			continue
		}

		// Check for section definition
		if strings.HasPrefix(line, "config ") {
			parts := strings.Fields(line)
			if len(parts) >= 2 {
				currentSection = parts[1]
				inTargetSection = currentSection == section
			}
			continue
		}

		// Check for option in target section
		if inTargetSection && strings.HasPrefix(line, "option "+option+" ") {
			parts := strings.Fields(line)
			if len(parts) >= 3 {
				return strings.Trim(parts[2], `"'`), nil
			}
		}
	}

	return "", fmt.Errorf("option %s not found in section %s", option, section)
}

// writeConfigValue writes a configuration value directly to the UCI config file
func (n *NativeUCI) writeConfigValue(config, section, option, value string) error {
	configFile := filepath.Join(n.configPath, config)

	// Ensure config directory exists
	if err := os.MkdirAll(filepath.Dir(configFile), 0o755); err != nil {
		return fmt.Errorf("failed to create config directory: %w", err)
	}

	data, err := os.ReadFile(configFile)
	if err != nil && !os.IsNotExist(err) {
		return fmt.Errorf("failed to read config file %s: %w", configFile, err)
	}

	lines := strings.Split(string(data), "\n")
	var currentSection string
	var inTargetSection bool
	var optionFound bool
	var newLines []string

	for _, line := range lines {
		trimmedLine := strings.TrimSpace(line)

		// Check for section definition
		if strings.HasPrefix(trimmedLine, "config ") {
			parts := strings.Fields(trimmedLine)
			if len(parts) >= 2 {
				currentSection = parts[1]
				inTargetSection = currentSection == section
			}
			newLines = append(newLines, line)
			continue
		}

		// Check for option in target section
		if inTargetSection && strings.HasPrefix(trimmedLine, "option "+option+" ") {
			optionFound = true
			newLines = append(newLines, fmt.Sprintf("option %s '%s'", option, value))
			continue
		}

		newLines = append(newLines, line)
	}

	// If section doesn't exist, create it
	if !inTargetSection {
		newLines = append(newLines, "")
		newLines = append(newLines, fmt.Sprintf("config %s", section))
		newLines = append(newLines, fmt.Sprintf("option %s '%s'", option, value))
	} else if !optionFound {
		// If section exists but option doesn't, add it
		for i, line := range newLines {
			if strings.TrimSpace(line) == fmt.Sprintf("config %s", section) {
				// Insert option after section definition
				newLines = append(newLines[:i+1], append([]string{fmt.Sprintf("option %s '%s'", option, value)}, newLines[i+1:]...)...)
				break
			}
		}
	}

	// Write back to file
	content := strings.Join(newLines, "\n")
	if err := os.WriteFile(configFile, []byte(content), 0o644); err != nil {
		return fmt.Errorf("failed to write config file %s: %w", configFile, err)
	}

	return nil
}

// ValidateConfig validates UCI configuration structure
func (n *NativeUCI) ValidateConfig(ctx context.Context, config string) error {
	configFile := filepath.Join(n.configPath, config)

	data, err := os.ReadFile(configFile)
	if err != nil {
		return fmt.Errorf("failed to read config file %s: %w", configFile, err)
	}

	lines := strings.Split(string(data), "\n")
	var currentSection string
	var sectionOptions []string

	for i, line := range lines {
		lineNum := i + 1
		trimmedLine := strings.TrimSpace(line)

		// Skip comments and empty lines
		if trimmedLine == "" || strings.HasPrefix(trimmedLine, "#") {
			continue
		}

		// Validate section definition
		if strings.HasPrefix(trimmedLine, "config ") {
			parts := strings.Fields(trimmedLine)
			if len(parts) < 2 {
				return fmt.Errorf("invalid section definition at line %d: %s", lineNum, line)
			}
			currentSection = parts[1]
			sectionOptions = []string{}
			continue
		}

		// Validate option definition
		if strings.HasPrefix(trimmedLine, "option ") {
			parts := strings.Fields(trimmedLine)
			if len(parts) < 3 {
				return fmt.Errorf("invalid option definition at line %d: %s", lineNum, line)
			}

			optionName := parts[1]
			optionValue := strings.Trim(strings.Join(parts[2:], " "), `"'`)

			// Check for duplicate options in same section
			for _, existing := range sectionOptions {
				if existing == optionName {
					return fmt.Errorf("duplicate option '%s' in section '%s' at line %d", optionName, currentSection, lineNum)
				}
			}
			sectionOptions = append(sectionOptions, optionName)

			// Validate option value format
			if err := n.validateOptionValue(optionName, optionValue); err != nil {
				return fmt.Errorf("invalid value for option '%s' in section '%s' at line %d: %w", optionName, currentSection, lineNum, err)
			}
		}
	}

	return nil
}

// validateOptionValue validates individual option values
func (n *NativeUCI) validateOptionValue(option, value string) error {
	switch option {
	case "enable", "use_mwan3", "predictive", "metrics_listener", "health_listener":
		if value != "0" && value != "1" {
			return fmt.Errorf("boolean option must be 0 or 1, got: %s", value)
		}
	case "poll_interval_ms", "decision_interval_ms", "discovery_interval_ms", "cleanup_interval_ms":
		if _, err := strconv.Atoi(value); err != nil {
			return fmt.Errorf("integer option must be a number, got: %s", value)
		}
	case "log_level":
		validLevels := []string{"debug", "info", "warn", "error", "trace"}
		valid := false
		for _, level := range validLevels {
			if level == value {
				valid = true
				break
			}
		}
		if !valid {
			return fmt.Errorf("log_level must be one of %v, got: %s", validLevels, value)
		}
	}
	return nil
}

// ClearCache clears the configuration cache
func (n *NativeUCI) ClearCache() {
	n.cacheMutex.Lock()
	n.cache = make(map[string]interface{})
	n.cacheMutex.Unlock()
}

// cachedValue represents a cached configuration value
type cachedValue struct {
	value     string
	timestamp time.Time
}
