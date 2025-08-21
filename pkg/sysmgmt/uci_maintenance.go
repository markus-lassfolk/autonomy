package sysmgmt

import (
	"fmt"
	"os/exec"
	"regexp"
	"strings"
	"time"

	"github.com/markus-lassfolk/autonomy/pkg/logx"
)

// UCIMaintenanceManager handles UCI configuration validation and repair
type UCIMaintenanceManager struct {
	logger *logx.Logger
}

// UCIIssue represents a UCI configuration issue
type UCIIssue struct {
	Type        string    `json:"type"`         // "parse_error", "corruption", "missing_section", "unwanted_file"
	Section     string    `json:"section"`      // UCI section (e.g., "mwan3.rule1") or file path
	Description string    `json:"description"`  // Human-readable description
	Severity    string    `json:"severity"`     // "critical", "warning", "info"
	CanAutoFix  bool      `json:"can_auto_fix"` // Whether we can automatically fix this
	Timestamp   time.Time `json:"timestamp"`    // When the issue was detected
}

// UCIMaintenanceResult represents the result of UCI maintenance
type UCIMaintenanceResult struct {
	IssuesFound   []UCIIssue `json:"issues_found"`
	IssuesFixed   []UCIIssue `json:"issues_fixed"`
	BackupCreated bool       `json:"backup_created"`
	BackupPath    string     `json:"backup_path"`
	SystemRestart bool       `json:"system_restart_needed"`
	Success       bool       `json:"success"`
	ErrorMessage  string     `json:"error_message,omitempty"`
}

// NewUCIMaintenanceManager creates a new UCI maintenance manager
func NewUCIMaintenanceManager(logger *logx.Logger) *UCIMaintenanceManager {
	return &UCIMaintenanceManager{
		logger: logger,
	}
}

// PerformUCIMaintenance performs comprehensive UCI maintenance
func (umm *UCIMaintenanceManager) PerformUCIMaintenance() (*UCIMaintenanceResult, error) {
	result := &UCIMaintenanceResult{
		IssuesFound: make([]UCIIssue, 0),
		IssuesFixed: make([]UCIIssue, 0),
	}

	umm.logger.Info("Starting UCI maintenance check")

	// Step 1: Create backup (disabled - using external backup system)
	// Backup functionality disabled to avoid filling up storage
	// External backup via Teltonika RMS is used instead
	umm.logger.Debug("UCI backup skipped - using external backup system")

	// Step 2: Check for parse errors
	if err := umm.checkParseErrors(result); err != nil {
		umm.logger.Error("Failed to check UCI parse errors", "error", err)
	}

	// Step 3: Validate critical sections
	if err := umm.validateCriticalSections(result); err != nil {
		umm.logger.Error("Failed to validate critical sections", "error", err)
	}

	// Step 4: Check for corruption
	if err := umm.checkUCICorruption(result); err != nil {
		umm.logger.Error("Failed to check UCI corruption", "error", err)
	}

	// Step 4.5: Check for unwanted files in /etc/config/
	if err := umm.checkUnwantedConfigFiles(result); err != nil {
		umm.logger.Error("Failed to check unwanted config files", "error", err)
	}

	// Step 5: Attempt to fix issues
	if err := umm.fixIssues(result); err != nil {
		umm.logger.Error("Failed to fix UCI issues", "error", err)
	}

	// Step 6: Verify fixes
	if err := umm.verifyFixes(result); err != nil {
		umm.logger.Error("Failed to verify UCI fixes", "error", err)
	}

	result.Success = len(result.IssuesFound) == 0 || len(result.IssuesFixed) > 0

	umm.logger.Info("UCI maintenance completed",
		"issues_found", len(result.IssuesFound),
		"issues_fixed", len(result.IssuesFixed),
		"success", result.Success)

	return result, nil
}

// createUCIBackup creates a backup of the current UCI configuration
func (umm *UCIMaintenanceManager) createUCIBackup(result *UCIMaintenanceResult) error {
	timestamp := time.Now().Format("20060102_150405")
	backupPath := fmt.Sprintf("/tmp/uci_backup_%s.tar.gz", timestamp)

	cmd := exec.Command("tar", "-czf", backupPath, "/etc/config/")
	if err := cmd.Run(); err != nil {
		return fmt.Errorf("failed to create UCI backup: %w", err)
	}

	result.BackupCreated = true
	result.BackupPath = backupPath

	umm.logger.Info("Created UCI backup", "path", backupPath)
	return nil
}

// checkParseErrors checks for UCI parse errors
func (umm *UCIMaintenanceManager) checkParseErrors(result *UCIMaintenanceResult) error {
	cmd := exec.Command("uci", "show")
	output, _ := cmd.CombinedOutput()

	outputStr := string(output)

	// Check for parse errors in output
	if strings.Contains(outputStr, "Parse error") {
		lines := strings.Split(outputStr, "\n")
		for i, line := range lines {
			if strings.Contains(line, "Parse error") {
				// Try to identify the problematic section
				section := "unknown"
				if i > 0 {
					prevLine := lines[i-1]
					if parts := strings.Split(prevLine, "="); len(parts) > 0 {
						section = strings.Split(parts[0], ".")[0]
					}
				}

				issue := UCIIssue{
					Type:        "parse_error",
					Section:     section,
					Description: fmt.Sprintf("UCI parse error detected near line: %s", line),
					Severity:    "critical",
					CanAutoFix:  true,
					Timestamp:   time.Now(),
				}
				result.IssuesFound = append(result.IssuesFound, issue)

				umm.logger.Error("UCI parse error detected",
					"section", section,
					"line", line)
			}
		}
	}

	return nil
}

// validateCriticalSections validates critical UCI sections
func (umm *UCIMaintenanceManager) validateCriticalSections(result *UCIMaintenanceResult) error {
	criticalSections := []string{"network", "mwan3", "system", "firewall"}

	for _, section := range criticalSections {
		cmd := exec.Command("uci", "show", section)
		if err := cmd.Run(); err != nil {
			issue := UCIIssue{
				Type:        "missing_section",
				Section:     section,
				Description: fmt.Sprintf("Critical UCI section '%s' is missing or corrupted", section),
				Severity:    "critical",
				CanAutoFix:  false,
				Timestamp:   time.Now(),
			}
			result.IssuesFound = append(result.IssuesFound, issue)

			umm.logger.Error("Critical UCI section missing", "section", section)
		}
	}

	return nil
}

// checkUCICorruption checks for UCI database corruption
func (umm *UCIMaintenanceManager) checkUCICorruption(result *UCIMaintenanceResult) error {
	// Check UCI database files for corruption
	configFiles := []string{
		"/etc/config/network",
		"/etc/config/mwan3",
		"/etc/config/system",
		"/etc/config/firewall",
	}

	for _, configFile := range configFiles {
		// Check if file exists and is readable
		cmd := exec.Command("test", "-r", configFile)
		if err := cmd.Run(); err != nil {
			issue := UCIIssue{
				Type:        "corruption",
				Section:     strings.TrimPrefix(configFile, "/etc/config/"),
				Description: fmt.Sprintf("UCI config file '%s' is not readable or missing", configFile),
				Severity:    "critical",
				CanAutoFix:  false,
				Timestamp:   time.Now(),
			}
			result.IssuesFound = append(result.IssuesFound, issue)
			continue
		}

		// Check for binary data or corruption
		cmd = exec.Command("file", configFile)
		output, err := cmd.Output()
		if err == nil && !strings.Contains(string(output), "text") {
			issue := UCIIssue{
				Type:        "corruption",
				Section:     strings.TrimPrefix(configFile, "/etc/config/"),
				Description: fmt.Sprintf("UCI config file '%s' appears to be corrupted (not text)", configFile),
				Severity:    "critical",
				CanAutoFix:  false,
				Timestamp:   time.Now(),
			}
			result.IssuesFound = append(result.IssuesFound, issue)
		}
	}

	return nil
}

// checkUnwantedConfigFiles checks for backup, temp, and other unwanted files in /etc/config/
func (umm *UCIMaintenanceManager) checkUnwantedConfigFiles(result *UCIMaintenanceResult) error {
	cmd := exec.Command("ls", "-la", "/etc/config/")
	output, err := cmd.Output()
	if err != nil {
		return fmt.Errorf("failed to list /etc/config/ directory: %w", err)
	}

	lines := strings.Split(string(output), "\n")
	unwantedPatterns := []string{
		".backup", // backup files like mwan3.backup.
		".bak",    // backup files
		".tmp",    // temporary files
		".temp",   // temporary files
		".old",    // old files
		".orig",   // original files
		".save",   // saved files
		"~",       // editor backup files
		".swp",    // vim swap files
		".swo",    // vim swap files
		"#",       // editor temp files
	}

	for _, line := range lines {
		if line == "" || strings.HasPrefix(line, "total") {
			continue
		}

		// Parse ls -la output to get filename
		fields := strings.Fields(line)
		if len(fields) < 9 {
			continue
		}
		filename := fields[8]

		// Skip directories and special entries
		if filename == "." || filename == ".." || strings.HasPrefix(line, "d") {
			continue
		}

		// Check if file matches unwanted patterns
		for _, pattern := range unwantedPatterns {
			if strings.Contains(filename, pattern) {
				issue := UCIIssue{
					Type:        "unwanted_file",
					Section:     "/etc/config/" + filename,
					Description: fmt.Sprintf("Unwanted file '%s' in UCI config directory (contains '%s')", filename, pattern),
					Severity:    "warning",
					CanAutoFix:  true,
					Timestamp:   time.Now(),
				}
				result.IssuesFound = append(result.IssuesFound, issue)

				umm.logger.Warn("Found unwanted file in UCI config directory",
					"file", filename,
					"pattern", pattern,
					"path", "/etc/config/"+filename)
				break
			}
		}

		// Also check for files that don't look like valid UCI config names
		// Valid UCI configs are typically alphanumeric with underscores
		if !isValidUCIConfigName(filename) {
			issue := UCIIssue{
				Type:        "unwanted_file",
				Section:     "/etc/config/" + filename,
				Description: fmt.Sprintf("File '%s' has invalid UCI config name format", filename),
				Severity:    "info",
				CanAutoFix:  true,
				Timestamp:   time.Now(),
			}
			result.IssuesFound = append(result.IssuesFound, issue)

			umm.logger.Info("Found file with invalid UCI config name",
				"file", filename,
				"path", "/etc/config/"+filename)
		}
	}

	return nil
}

// isValidUCIConfigName checks if a filename is a valid UCI config name
func isValidUCIConfigName(filename string) bool {
	// Valid UCI config names are typically:
	// - alphanumeric characters
	// - underscores
	// - no dots (except for some system files we'll allow)
	// - no spaces or special characters

	// Allow some known system files
	knownSystemFiles := []string{
		"openssl", // SSL config
	}
	for _, known := range knownSystemFiles {
		if filename == known {
			return true
		}
	}

	// Check for invalid characters
	for _, char := range filename {
		if !((char >= 'a' && char <= 'z') ||
			(char >= 'A' && char <= 'Z') ||
			(char >= '0' && char <= '9') ||
			char == '_') {
			return false
		}
	}

	return len(filename) > 0
}

// fixIssues attempts to fix detected UCI issues
func (umm *UCIMaintenanceManager) fixIssues(result *UCIMaintenanceResult) error {
	for _, issue := range result.IssuesFound {
		if !issue.CanAutoFix {
			umm.logger.Warn("Cannot auto-fix UCI issue",
				"type", issue.Type,
				"section", issue.Section,
				"description", issue.Description)
			continue
		}

		switch issue.Type {
		case "parse_error":
			if err := umm.fixParseError(issue); err != nil {
				umm.logger.Error("Failed to fix parse error", "error", err, "section", issue.Section)
			} else {
				result.IssuesFixed = append(result.IssuesFixed, issue)
				umm.logger.Info("Fixed UCI parse error", "section", issue.Section)
			}
		case "unwanted_file":
			if err := umm.fixUnwantedFile(issue); err != nil {
				umm.logger.Error("Failed to fix unwanted file", "error", err, "file", issue.Section)
			} else {
				result.IssuesFixed = append(result.IssuesFixed, issue)
				umm.logger.Info("Fixed unwanted file", "file", issue.Section)
			}
		}
	}

	return nil
}

// fixParseError attempts to fix UCI parse errors
func (umm *UCIMaintenanceManager) fixParseError(issue UCIIssue) error {
	// Strategy 1: Try to reload the specific section
	if issue.Section != "unknown" {
		umm.logger.Info("Attempting to reload UCI section", "section", issue.Section)

		// First, try to commit any pending changes
		cmd := exec.Command("uci", "commit", issue.Section)
		if err := cmd.Run(); err != nil {
			umm.logger.Debug("UCI commit failed, trying revert", "section", issue.Section, "error", err)

			// If commit fails, try to revert
			cmd = exec.Command("uci", "revert", issue.Section)
			if err := cmd.Run(); err != nil {
				return fmt.Errorf("failed to revert UCI section %s: %w", issue.Section, err)
			}
		}
	}

	// Strategy 2: Try to reload UCI entirely
	cmd := exec.Command("uci", "reload")
	if err := cmd.Run(); err != nil {
		return fmt.Errorf("failed to reload UCI: %w", err)
	}

	return nil
}

// fixUnwantedFile moves unwanted files from /etc/config/ to a backup location
func (umm *UCIMaintenanceManager) fixUnwantedFile(issue UCIIssue) error {
	filePath := issue.Section // Section contains the full file path

	// Create backup directory if it doesn't exist
	backupDir := "/tmp/uci_unwanted_files"
	if err := exec.Command("mkdir", "-p", backupDir).Run(); err != nil {
		return fmt.Errorf("failed to create backup directory: %w", err)
	}

	// Generate backup filename with timestamp
	timestamp := time.Now().Format("20060102_150405")
	filename := strings.TrimPrefix(filePath, "/etc/config/")
	backupPath := fmt.Sprintf("%s/%s_%s", backupDir, filename, timestamp)

	// Move the unwanted file to backup location
	cmd := exec.Command("mv", filePath, backupPath)
	if err := cmd.Run(); err != nil {
		return fmt.Errorf("failed to move unwanted file %s to %s: %w", filePath, backupPath, err)
	}

	umm.logger.Info("Moved unwanted UCI file to backup",
		"original", filePath,
		"backup", backupPath)

	return nil
}

// verifyFixes verifies that UCI issues have been resolved
func (umm *UCIMaintenanceManager) verifyFixes(result *UCIMaintenanceResult) error {
	// Re-run parse error check
	cmd := exec.Command("uci", "show")
	output, err := cmd.CombinedOutput()
	if err != nil {
		return fmt.Errorf("UCI still has errors after fix attempt: %w", err)
	}

	if strings.Contains(string(output), "Parse error") {
		return fmt.Errorf("UCI parse errors still present after fix attempt")
	}

	umm.logger.Info("UCI verification passed - no parse errors detected")
	return nil
}

// GetUCIHealth returns the current health status of UCI configuration
func (umm *UCIMaintenanceManager) GetUCIHealth() map[string]interface{} {
	health := map[string]interface{}{
		"timestamp": time.Now(),
		"status":    "unknown",
		"errors":    []string{},
		"warnings":  []string{},
	}

	// Quick UCI health check
	cmd := exec.Command("uci", "show")
	output, err := cmd.CombinedOutput()
	if err != nil {
		health["status"] = "error"
		health["errors"] = append(health["errors"].([]string), fmt.Sprintf("UCI command failed: %v", err))
		return health
	}

	outputStr := string(output)
	if strings.Contains(outputStr, "Parse error") {
		health["status"] = "error"
		health["errors"] = append(health["errors"].([]string), "UCI parse errors detected")
	} else {
		health["status"] = "healthy"
	}

	// Count configuration sections
	sections := make(map[string]int)
	re := regexp.MustCompile(`^([^.]+)\.`)
	lines := strings.Split(outputStr, "\n")
	for _, line := range lines {
		if matches := re.FindStringSubmatch(line); len(matches) > 1 {
			sections[matches[1]]++
		}
	}
	health["sections"] = sections

	return health
}

// ScheduleUCIMaintenance schedules regular UCI maintenance checks
func (umm *UCIMaintenanceManager) ScheduleUCIMaintenance(interval time.Duration) {
	ticker := time.NewTicker(interval)
	go func() {
		for range ticker.C {
			result, err := umm.PerformUCIMaintenance()
			if err != nil {
				umm.logger.Error("Scheduled UCI maintenance failed", "error", err)
			} else if len(result.IssuesFound) > 0 {
				umm.logger.Warn("UCI maintenance found issues",
					"issues_found", len(result.IssuesFound),
					"issues_fixed", len(result.IssuesFixed))
			}
		}
	}()

	umm.logger.Info("Scheduled UCI maintenance", "interval", interval)
}
