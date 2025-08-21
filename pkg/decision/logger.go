package decision

import (
	"encoding/csv"
	"encoding/json"
	"fmt"
	"os"
	"path/filepath"
	"sort"
	"strconv"
	"strings"
	"sync"
	"time"

	"github.com/markus-lassfolk/autonomy/pkg"
	"github.com/markus-lassfolk/autonomy/pkg/logx"
)

// DecisionLoggerImpl implements the DecisionLogger interface
type DecisionLoggerImpl struct {
	logger           *logx.Logger
	config           *DecisionLogConfig
	decisions        []*pkg.Decision
	decisionsMutex   sync.RWMutex
	csvFile          *os.File
	csvWriter        *csv.Writer
	jsonFile         *os.File
	currentLogDate   string
	logRotationMutex sync.Mutex
}

// DecisionLogConfig represents decision logging configuration
type DecisionLogConfig struct {
	Enabled                bool   `json:"enabled"`
	LogDirectory           string `json:"log_directory"`
	CSVLoggingEnabled      bool   `json:"csv_logging_enabled"`
	JSONLoggingEnabled     bool   `json:"json_logging_enabled"`
	InMemoryHistorySize    int    `json:"in_memory_history_size"`
	LogRotationEnabled     bool   `json:"log_rotation_enabled"`
	MaxLogFileSizeMB       int    `json:"max_log_file_size_mb"`
	MaxLogFileAgeDays      int    `json:"max_log_file_age_days"`
	CompressionEnabled     bool   `json:"compression_enabled"`
	DetailedLoggingEnabled bool   `json:"detailed_logging_enabled"`
	QualityFactorLogging   bool   `json:"quality_factor_logging"`
	LocationDataLogging    bool   `json:"location_data_logging"`
	PerformanceMetrics     bool   `json:"performance_metrics"`
}

// NewDecisionLogger creates a new decision logger
func NewDecisionLogger(config *DecisionLogConfig, logger *logx.Logger) (*DecisionLoggerImpl, error) {
	if config == nil {
		config = DefaultDecisionLogConfig()
	}

	dl := &DecisionLoggerImpl{
		logger:    logger,
		config:    config,
		decisions: make([]*pkg.Decision, 0, config.InMemoryHistorySize),
	}

	if config.Enabled {
		if err := dl.initializeLogFiles(); err != nil {
			return nil, fmt.Errorf("failed to initialize log files: %w", err)
		}
	}

	return dl, nil
}

// DefaultDecisionLogConfig returns default decision logging configuration
func DefaultDecisionLogConfig() *DecisionLogConfig {
	return &DecisionLogConfig{
		Enabled:                true,
		LogDirectory:           "/tmp/autonomy/decisions",
		CSVLoggingEnabled:      true,
		JSONLoggingEnabled:     true,
		InMemoryHistorySize:    1000,
		LogRotationEnabled:     true,
		MaxLogFileSizeMB:       10,
		MaxLogFileAgeDays:      30,
		CompressionEnabled:     false,
		DetailedLoggingEnabled: true,
		QualityFactorLogging:   true,
		LocationDataLogging:    true,
		PerformanceMetrics:     true,
	}
}

// LogDecision logs a decision with comprehensive details
func (dl *DecisionLoggerImpl) LogDecision(decision *pkg.Decision) error {
	if !dl.config.Enabled {
		return nil
	}

	dl.decisionsMutex.Lock()
	defer dl.decisionsMutex.Unlock()

	// Add to in-memory history
	dl.decisions = append(dl.decisions, decision)

	// Maintain history size limit
	if len(dl.decisions) > dl.config.InMemoryHistorySize {
		dl.decisions = dl.decisions[1:]
	}

	// Check for log rotation
	if dl.config.LogRotationEnabled {
		if err := dl.checkLogRotation(); err != nil {
			dl.logger.LogVerbose("log_rotation_error", map[string]interface{}{
				"error": err.Error(),
			})
		}
	}

	// Log to CSV
	if dl.config.CSVLoggingEnabled && dl.csvWriter != nil {
		if err := dl.logToCSV(decision); err != nil {
			dl.logger.LogVerbose("csv_logging_error", map[string]interface{}{
				"error": err.Error(),
			})
		}
	}

	// Log to JSON
	if dl.config.JSONLoggingEnabled && dl.jsonFile != nil {
		if err := dl.logToJSON(decision); err != nil {
			dl.logger.LogVerbose("json_logging_error", map[string]interface{}{
				"error": err.Error(),
			})
		}
	}

	// Log structured decision details
	dl.logger.LogVerbose("decision_logged", map[string]interface{}{
		"decision_id": decision.ID,
		"type":        decision.Type,
		"from":        decision.From,
		"to":          decision.To,
		"reason":      decision.Reason,
		"trigger":     decision.TriggerReason,
		"success":     decision.Success,
		"predictive":  decision.Predictive,
		"duration_ms": decision.Duration.Milliseconds(),
	})

	return nil
}

// GetDecisions retrieves decisions based on query parameters
func (dl *DecisionLoggerImpl) GetDecisions(since time.Time, limit int) ([]*pkg.Decision, error) {
	dl.decisionsMutex.RLock()
	defer dl.decisionsMutex.RUnlock()

	var filtered []*pkg.Decision

	for _, decision := range dl.decisions {
		if decision.Timestamp.After(since) {
			filtered = append(filtered, decision)
		}
	}

	// Sort by timestamp (newest first)
	sort.Slice(filtered, func(i, j int) bool {
		return filtered[i].Timestamp.After(filtered[j].Timestamp)
	})

	// Apply limit
	if limit > 0 && len(filtered) > limit {
		filtered = filtered[:limit]
	}

	return filtered, nil
}

// GetDecisionStats returns comprehensive decision statistics
func (dl *DecisionLoggerImpl) GetDecisionStats(since time.Time) (map[string]interface{}, error) {
	dl.decisionsMutex.RLock()
	defer dl.decisionsMutex.RUnlock()

	decisionsByType := make(map[string]int)
	decisionsByReason := make(map[string]int)

	var totalDecisions int
	var successCount int
	var predictiveDecisions int
	var totalDuration time.Duration

	for _, decision := range dl.decisions {
		if decision.Timestamp.Before(since) {
			continue
		}

		totalDecisions++
		decisionsByType[decision.Type]++
		decisionsByReason[decision.Reason]++

		if decision.Success {
			successCount++
		}

		if decision.Predictive {
			predictiveDecisions++
		}

		totalDuration += decision.Duration
	}

	// Calculate derived statistics
	successRate := 0.0
	avgDecisionTime := 0.0

	if totalDecisions > 0 {
		successRate = float64(successCount) / float64(totalDecisions) * 100
		avgDecisionTime = float64(totalDuration.Milliseconds()) / float64(totalDecisions)
	}

	return map[string]interface{}{
		"total_decisions":      totalDecisions,
		"decisions_by_type":    decisionsByType,
		"decisions_by_reason":  decisionsByReason,
		"success_rate":         successRate,
		"avg_decision_time_ms": avgDecisionTime,
		"predictive_decisions": predictiveDecisions,
	}, nil
}

// initializeLogFiles initializes log files and directories
func (dl *DecisionLoggerImpl) initializeLogFiles() error {
	// Create log directory if it doesn't exist
	if err := os.MkdirAll(dl.config.LogDirectory, 0o755); err != nil {
		return fmt.Errorf("failed to create log directory: %w", err)
	}

	// Initialize current log date
	dl.currentLogDate = time.Now().Format("2006-01-02")

	// Initialize CSV logging
	if dl.config.CSVLoggingEnabled {
		if err := dl.initializeCSVLog(); err != nil {
			return fmt.Errorf("failed to initialize CSV log: %w", err)
		}
	}

	// Initialize JSON logging
	if dl.config.JSONLoggingEnabled {
		if err := dl.initializeJSONLog(); err != nil {
			return fmt.Errorf("failed to initialize JSON log: %w", err)
		}
	}

	dl.logger.LogVerbose("decision_logger_initialized", map[string]interface{}{
		"log_directory":    dl.config.LogDirectory,
		"csv_enabled":      dl.config.CSVLoggingEnabled,
		"json_enabled":     dl.config.JSONLoggingEnabled,
		"history_size":     dl.config.InMemoryHistorySize,
		"rotation_enabled": dl.config.LogRotationEnabled,
	})

	return nil
}

// initializeCSVLog initializes CSV logging
func (dl *DecisionLoggerImpl) initializeCSVLog() error {
	csvPath := filepath.Join(dl.config.LogDirectory, fmt.Sprintf("decisions_%s.csv", dl.currentLogDate))

	// Check if file exists to determine if we need headers
	fileExists := false
	if _, err := os.Stat(csvPath); err == nil {
		fileExists = true
	}

	file, err := os.OpenFile(csvPath, os.O_CREATE|os.O_WRONLY|os.O_APPEND, 0o644)
	if err != nil {
		return fmt.Errorf("failed to open CSV file: %w", err)
	}

	dl.csvFile = file
	dl.csvWriter = csv.NewWriter(file)

	// Write CSV headers if this is a new file
	if !fileExists {
		headers := dl.getCSVHeaders()
		if err := dl.csvWriter.Write(headers); err != nil {
			return fmt.Errorf("failed to write CSV headers: %w", err)
		}
		dl.csvWriter.Flush()
	}

	return nil
}

// initializeJSONLog initializes JSON logging
func (dl *DecisionLoggerImpl) initializeJSONLog() error {
	jsonPath := filepath.Join(dl.config.LogDirectory, fmt.Sprintf("decisions_%s.jsonl", dl.currentLogDate))

	file, err := os.OpenFile(jsonPath, os.O_CREATE|os.O_WRONLY|os.O_APPEND, 0o644)
	if err != nil {
		return fmt.Errorf("failed to open JSON file: %w", err)
	}

	dl.jsonFile = file
	return nil
}

// logToCSV logs decision to CSV file
func (dl *DecisionLoggerImpl) logToCSV(decision *pkg.Decision) error {
	record := dl.decisionToCSVRecord(decision)
	if err := dl.csvWriter.Write(record); err != nil {
		return err
	}
	dl.csvWriter.Flush()
	return nil
}

// logToJSON logs decision to JSON file
func (dl *DecisionLoggerImpl) logToJSON(decision *pkg.Decision) error {
	jsonData, err := json.Marshal(decision)
	if err != nil {
		return err
	}

	_, err = dl.jsonFile.WriteString(string(jsonData) + "\n")
	if err != nil {
		return err
	}

	return dl.jsonFile.Sync()
}

// getCSVHeaders returns CSV column headers
func (dl *DecisionLoggerImpl) getCSVHeaders() []string {
	headers := []string{
		"timestamp",
		"decision_id",
		"type",
		"member",
		"from",
		"to",
		"reason",
		"trigger_reason",
		"success",
		"error",
		"duration_ms",
		"predictive",
	}

	if dl.config.QualityFactorLogging {
		headers = append(headers, []string{
			"quality_latency",
			"quality_loss",
			"quality_jitter",
			"quality_obstruction",
			"quality_signal",
		}...)
	}

	if dl.config.LocationDataLogging {
		headers = append(headers, []string{
			"location_latitude",
			"location_longitude",
			"location_accuracy",
			"location_source",
		}...)
	}

	return headers
}

// decisionToCSVRecord converts a decision to CSV record
func (dl *DecisionLoggerImpl) decisionToCSVRecord(decision *pkg.Decision) []string {
	record := []string{
		decision.Timestamp.Format(time.RFC3339),
		decision.ID,
		decision.Type,
		decision.Member,
		decision.From,
		decision.To,
		decision.Reason,
		decision.TriggerReason,
		strconv.FormatBool(decision.Success),
		decision.Error,
		strconv.FormatInt(decision.Duration.Milliseconds(), 10),
		strconv.FormatBool(decision.Predictive),
	}

	if dl.config.QualityFactorLogging {
		record = append(record, []string{
			dl.getQualityFactor(decision.QualityFactors, "latency"),
			dl.getQualityFactor(decision.QualityFactors, "loss"),
			dl.getQualityFactor(decision.QualityFactors, "jitter"),
			dl.getQualityFactor(decision.QualityFactors, "obstruction"),
			dl.getQualityFactor(decision.QualityFactors, "signal"),
		}...)
	}

	if dl.config.LocationDataLogging && decision.LocationData != nil {
		record = append(record, []string{
			strconv.FormatFloat(decision.LocationData.Latitude, 'f', 6, 64),
			strconv.FormatFloat(decision.LocationData.Longitude, 'f', 6, 64),
			strconv.FormatFloat(decision.LocationData.Accuracy, 'f', 2, 64),
			decision.LocationData.Source,
		}...)
	} else if dl.config.LocationDataLogging {
		record = append(record, []string{"", "", "", ""}...)
	}

	return record
}

// getQualityFactor extracts quality factor value as string
func (dl *DecisionLoggerImpl) getQualityFactor(factors map[string]float64, key string) string {
	if factors == nil {
		return ""
	}
	if val, exists := factors[key]; exists {
		return strconv.FormatFloat(val, 'f', 2, 64)
	}
	return ""
}

// checkLogRotation checks if log rotation is needed
func (dl *DecisionLoggerImpl) checkLogRotation() error {
	dl.logRotationMutex.Lock()
	defer dl.logRotationMutex.Unlock()

	currentDate := time.Now().Format("2006-01-02")
	if currentDate != dl.currentLogDate {
		// Close current files
		if dl.csvFile != nil {
			dl.csvFile.Close()
		}
		if dl.jsonFile != nil {
			dl.jsonFile.Close()
		}

		// Update current date
		dl.currentLogDate = currentDate

		// Initialize new files
		if dl.config.CSVLoggingEnabled {
			if err := dl.initializeCSVLog(); err != nil {
				return err
			}
		}
		if dl.config.JSONLoggingEnabled {
			if err := dl.initializeJSONLog(); err != nil {
				return err
			}
		}

		dl.logger.LogVerbose("log_rotation_completed", map[string]interface{}{
			"new_date": currentDate,
		})
	}

	return nil
}

// Close closes the decision logger and flushes any pending data
func (dl *DecisionLoggerImpl) Close() error {
	dl.logRotationMutex.Lock()
	defer dl.logRotationMutex.Unlock()

	var errors []string

	if dl.csvWriter != nil {
		dl.csvWriter.Flush()
	}
	if dl.csvFile != nil {
		if err := dl.csvFile.Close(); err != nil {
			errors = append(errors, fmt.Sprintf("CSV file close error: %v", err))
		}
	}
	if dl.jsonFile != nil {
		if err := dl.jsonFile.Close(); err != nil {
			errors = append(errors, fmt.Sprintf("JSON file close error: %v", err))
		}
	}

	if len(errors) > 0 {
		return fmt.Errorf("errors during close: %s", strings.Join(errors, "; "))
	}

	return nil
}
