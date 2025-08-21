package audit

import (
	"context"
	"encoding/csv"
	"fmt"
	"os"
	"path/filepath"
	"sync"
	"time"

	"github.com/markus-lassfolk/autonomy/pkg"
	"github.com/markus-lassfolk/autonomy/pkg/logx"
)

// DecisionRecord represents a single decision made by the failover system
type DecisionRecord struct {
	Timestamp       time.Time              `json:"timestamp"`
	DecisionID      string                 `json:"decision_id"`
	DecisionType    string                 `json:"decision_type"` // failover, restore, recheck, etc.
	Trigger         string                 `json:"trigger"`       // what triggered the decision
	FromMember      *pkg.Member            `json:"from_member,omitempty"`
	ToMember        *pkg.Member            `json:"to_member,omitempty"`
	Reasoning       string                 `json:"reasoning"`
	Confidence      float64                `json:"confidence"` // 0.0-1.0 confidence in decision
	Metrics         *pkg.Metrics           `json:"metrics,omitempty"`
	Score           *pkg.Score             `json:"score,omitempty"`
	Context         map[string]interface{} `json:"context"` // additional context
	RootCause       string                 `json:"root_cause,omitempty"`
	Recommendations []string               `json:"recommendations,omitempty"`
	ExecutionTime   time.Duration          `json:"execution_time"`
	Success         bool                   `json:"success"`
	Error           string                 `json:"error,omitempty"`
}

// DecisionLogger manages the audit trail for all failover decisions
type DecisionLogger struct {
	logger     *logx.Logger
	mu         sync.RWMutex
	records    []*DecisionRecord
	maxRecords int
	logFile    string
	csvFile    string
	enabled    bool
}

// NewDecisionLogger creates a new decision logger instance
func NewDecisionLogger(logger *logx.Logger, maxRecords int, logDir string) *DecisionLogger {
	if maxRecords <= 0 {
		maxRecords = 1000 // Default to 1000 records
	}

	if logDir == "" {
		logDir = "/var/log/autonomy"
	}

	// Ensure log directory exists
	if err := os.MkdirAll(logDir, 0o755); err != nil {
		logger.Error("Failed to create audit log directory", "error", err, "path", logDir)
	}

	return &DecisionLogger{
		logger:     logger,
		records:    make([]*DecisionRecord, 0, maxRecords),
		maxRecords: maxRecords,
		logFile:    filepath.Join(logDir, "decision_audit.log"),
		csvFile:    filepath.Join(logDir, "decision_audit.csv"),
		enabled:    true,
	}
}

// LogDecision records a decision in the audit trail
func (dl *DecisionLogger) LogDecision(ctx context.Context, record *DecisionRecord) error {
	if !dl.enabled {
		return nil
	}

	dl.mu.Lock()
	defer dl.mu.Unlock()

	// Add to in-memory records
	dl.records = append(dl.records, record)

	// Maintain max records limit
	if len(dl.records) > dl.maxRecords {
		dl.records = dl.records[len(dl.records)-dl.maxRecords:]
	}

	// Log to file
	if err := dl.writeToLogFile(record); err != nil {
		dl.logger.Error("Failed to write decision to log file", "error", err, "decision_id", record.DecisionID)
	}

	// Write to CSV
	if err := dl.writeToCSV(record); err != nil {
		dl.logger.Error("Failed to write decision to CSV", "error", err, "decision_id", record.DecisionID)
	}

	// Log to structured logger
	dl.logger.Info("Decision recorded",
		"decision_id", record.DecisionID,
		"type", record.DecisionType,
		"trigger", record.Trigger,
		"confidence", record.Confidence,
		"success", record.Success,
		"execution_time", record.ExecutionTime,
	)

	return nil
}

// GetRecentDecisions returns recent decisions within the specified time window
func (dl *DecisionLogger) GetRecentDecisions(since time.Time, limit int) []*DecisionRecord {
	dl.mu.RLock()
	defer dl.mu.RUnlock()

	if limit <= 0 {
		limit = 100
	}

	var recent []*DecisionRecord
	count := 0

	// Iterate backwards through records (most recent first)
	for i := len(dl.records) - 1; i >= 0 && count < limit; i-- {
		record := dl.records[i]
		if record.Timestamp.After(since) {
			recent = append([]*DecisionRecord{record}, recent...)
			count++
		}
	}

	return recent
}

// GetDecisionsByType returns decisions of a specific type
func (dl *DecisionLogger) GetDecisionsByType(decisionType string, limit int) []*DecisionRecord {
	dl.mu.RLock()
	defer dl.mu.RUnlock()

	if limit <= 0 {
		limit = 100
	}

	var filtered []*DecisionRecord
	count := 0

	// Iterate backwards through records (most recent first)
	for i := len(dl.records) - 1; i >= 0 && count < limit; i-- {
		record := dl.records[i]
		if record.DecisionType == decisionType {
			filtered = append([]*DecisionRecord{record}, filtered...)
			count++
		}
	}

	return filtered
}

// GetDecisionByID returns a specific decision by ID
func (dl *DecisionLogger) GetDecisionByID(decisionID string) *DecisionRecord {
	dl.mu.RLock()
	defer dl.mu.RUnlock()

	for _, record := range dl.records {
		if record.DecisionID == decisionID {
			return record
		}
	}

	return nil
}

// GetDecisionStats returns statistics about decisions
func (dl *DecisionLogger) GetDecisionStats(since time.Time) *DecisionStats {
	dl.mu.RLock()
	defer dl.mu.RUnlock()

	stats := &DecisionStats{
		TotalDecisions:       0,
		SuccessfulDecisions:  0,
		FailedDecisions:      0,
		AverageConfidence:    0.0,
		AverageExecutionTime: 0,
		DecisionTypes:        make(map[string]int),
		Triggers:             make(map[string]int),
		RootCauses:           make(map[string]int),
	}

	var totalConfidence float64
	var totalExecutionTime time.Duration
	validDecisions := 0

	for _, record := range dl.records {
		if record.Timestamp.After(since) {
			stats.TotalDecisions++

			if record.Success {
				stats.SuccessfulDecisions++
			} else {
				stats.FailedDecisions++
			}

			stats.DecisionTypes[record.DecisionType]++
			stats.Triggers[record.Trigger]++

			if record.RootCause != "" {
				stats.RootCauses[record.RootCause]++
			}

			totalConfidence += record.Confidence
			totalExecutionTime += record.ExecutionTime
			validDecisions++
		}
	}

	if validDecisions > 0 {
		stats.AverageConfidence = totalConfidence / float64(validDecisions)
		stats.AverageExecutionTime = totalExecutionTime / time.Duration(validDecisions)
	}

	return stats
}

// DecisionStats represents statistics about decisions
type DecisionStats struct {
	TotalDecisions       int            `json:"total_decisions"`
	SuccessfulDecisions  int            `json:"successful_decisions"`
	FailedDecisions      int            `json:"failed_decisions"`
	AverageConfidence    float64        `json:"average_confidence"`
	AverageExecutionTime time.Duration  `json:"average_execution_time"`
	DecisionTypes        map[string]int `json:"decision_types"`
	Triggers             map[string]int `json:"triggers"`
	RootCauses           map[string]int `json:"root_causes"`
}

// writeToLogFile writes a decision record to the log file
func (dl *DecisionLogger) writeToLogFile(record *DecisionRecord) error {
	file, err := os.OpenFile(dl.logFile, os.O_APPEND|os.O_CREATE|os.O_WRONLY, 0o644)
	if err != nil {
		return fmt.Errorf("failed to open log file: %w", err)
	}
	defer file.Close()

	// Write structured log entry
	logEntry := fmt.Sprintf("[%s] %s | %s | %s | %.2f | %v | %v\n",
		record.Timestamp.Format(time.RFC3339),
		record.DecisionID,
		record.DecisionType,
		record.Trigger,
		record.Confidence,
		record.Success,
		record.ExecutionTime,
	)

	_, err = file.WriteString(logEntry)
	return err
}

// writeToCSV writes a decision record to the CSV file
func (dl *DecisionLogger) writeToCSV(record *DecisionRecord) error {
	// Create CSV file if it doesn't exist
	if _, err := os.Stat(dl.csvFile); os.IsNotExist(err) {
		if err := dl.createCSVHeader(); err != nil {
			return err
		}
	}

	file, err := os.OpenFile(dl.csvFile, os.O_APPEND|os.O_CREATE|os.O_WRONLY, 0o644)
	if err != nil {
		return fmt.Errorf("failed to open CSV file: %w", err)
	}
	defer file.Close()

	writer := csv.NewWriter(file)
	defer writer.Flush()

	// Prepare CSV row
	row := []string{
		record.Timestamp.Format(time.RFC3339),
		record.DecisionID,
		record.DecisionType,
		record.Trigger,
		fmt.Sprintf("%.2f", record.Confidence),
		fmt.Sprintf("%v", record.Success),
		record.ExecutionTime.String(),
	}

	if record.FromMember != nil {
		row = append(row, record.FromMember.Name)
	} else {
		row = append(row, "")
	}

	if record.ToMember != nil {
		row = append(row, record.ToMember.Name)
	} else {
		row = append(row, "")
	}

	row = append(row, record.Reasoning)
	row = append(row, record.RootCause)
	row = append(row, record.Error)

	return writer.Write(row)
}

// createCSVHeader creates the CSV file with headers
func (dl *DecisionLogger) createCSVHeader() error {
	file, err := os.Create(dl.csvFile)
	if err != nil {
		return fmt.Errorf("failed to create CSV file: %w", err)
	}
	defer file.Close()

	writer := csv.NewWriter(file)
	defer writer.Flush()

	headers := []string{
		"Timestamp",
		"DecisionID",
		"DecisionType",
		"Trigger",
		"Confidence",
		"Success",
		"ExecutionTime",
		"FromMember",
		"ToMember",
		"Reasoning",
		"RootCause",
		"Error",
	}

	return writer.Write(headers)
}

// Enable/Disable logging
func (dl *DecisionLogger) Enable() {
	dl.mu.Lock()
	defer dl.mu.Unlock()
	dl.enabled = true
	dl.logger.Info("Decision audit logging enabled")
}

func (dl *DecisionLogger) Disable() {
	dl.mu.Lock()
	defer dl.mu.Unlock()
	dl.enabled = false
	dl.logger.Info("Decision audit logging disabled")
}

func (dl *DecisionLogger) IsEnabled() bool {
	dl.mu.RLock()
	defer dl.mu.RUnlock()
	return dl.enabled
}

// Clear clears all stored decisions
func (dl *DecisionLogger) Clear() {
	dl.mu.Lock()
	defer dl.mu.Unlock()
	dl.records = make([]*DecisionRecord, 0, dl.maxRecords)
	dl.logger.Info("Decision audit trail cleared")
}

// GetRecordCount returns the current number of stored records
func (dl *DecisionLogger) GetRecordCount() int {
	dl.mu.RLock()
	defer dl.mu.RUnlock()
	return len(dl.records)
}
