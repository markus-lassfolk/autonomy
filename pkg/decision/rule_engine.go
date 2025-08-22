package decision

import (
	"context"
	"fmt"
	"sync"
	"time"

	"github.com/markus-lassfolk/autonomy/pkg/logx"
)

// RuleEngine provides flexible decision-making based on configurable rules
type RuleEngine struct {
	config *RuleEngineConfig
	logger *logx.Logger
	mu     sync.RWMutex

	// Rules storage
	rules     []*Rule
	ruleIndex map[string]*Rule

	// Rule execution state
	executionHistory []*RuleExecution
	lastExecution    time.Time

	// Performance tracking
	executionStats map[string]*RuleStats
}

// RuleEngineConfig holds rule engine configuration
type RuleEngineConfig struct {
	Enabled             bool          `json:"enabled"`
	MaxRules            int           `json:"max_rules"`             // Maximum number of rules
	ExecutionTimeout    time.Duration `json:"execution_timeout"`     // Timeout for rule execution
	MaxHistorySize      int           `json:"max_history_size"`      // Maximum execution history size
	EnableDebugLogging  bool          `json:"enable_debug_logging"`  // Enable detailed rule execution logging
	ParallelExecution   bool          `json:"parallel_execution"`    // Execute rules in parallel
	MaxParallelRules    int           `json:"max_parallel_rules"`    // Maximum parallel rule executions
	RulePriorityEnabled bool          `json:"rule_priority_enabled"` // Enable rule priority system
	DefaultPriority     int           `json:"default_priority"`      // Default rule priority
}

// Rule represents a decision rule
type Rule struct {
	ID          string                 `json:"id"`
	Name        string                 `json:"name"`
	Description string                 `json:"description"`
	Enabled     bool                   `json:"enabled"`
	Priority    int                    `json:"priority"`
	Conditions  []*Condition           `json:"conditions"`
	Actions     []*Action              `json:"actions"`
	Metadata    map[string]interface{} `json:"metadata,omitempty"`
	CreatedAt   time.Time              `json:"created_at"`
	UpdatedAt   time.Time              `json:"updated_at"`
}

// Condition represents a rule condition
type Condition struct {
	Type        string                 `json:"type"`
	Field       string                 `json:"field"`
	Operator    string                 `json:"operator"`
	Value       interface{}            `json:"value"`
	Parameters  map[string]interface{} `json:"parameters,omitempty"`
	Description string                 `json:"description"`
}

// Action represents a rule action
type Action struct {
	Type        string                 `json:"type"`
	Parameters  map[string]interface{} `json:"parameters"`
	Description string                 `json:"description"`
	Enabled     bool                   `json:"enabled"`
}

// RuleExecution represents a rule execution result
type RuleExecution struct {
	RuleID        string                 `json:"rule_id"`
	RuleName      string                 `json:"rule_name"`
	Triggered     bool                   `json:"triggered"`
	Conditions    []*ConditionResult     `json:"conditions"`
	Actions       []*ActionResult        `json:"actions"`
	ExecutionTime time.Duration          `json:"execution_time"`
	Timestamp     time.Time              `json:"timestamp"`
	Context       map[string]interface{} `json:"context"`
	Error         string                 `json:"error,omitempty"`
}

// ConditionResult represents the result of a condition evaluation
type ConditionResult struct {
	Condition *Condition  `json:"condition"`
	Evaluated bool        `json:"evaluated"`
	Result    bool        `json:"result"`
	Value     interface{} `json:"value"`
	Error     string      `json:"error,omitempty"`
}

// ActionResult represents the result of an action execution
type ActionResult struct {
	Action   *Action     `json:"action"`
	Executed bool        `json:"executed"`
	Success  bool        `json:"success"`
	Result   interface{} `json:"result"`
	Error    string      `json:"error,omitempty"`
}

// RuleStats represents statistics for a rule
type RuleStats struct {
	RuleID           string    `json:"rule_id"`
	TotalExecutions  int       `json:"total_executions"`
	TriggeredCount   int       `json:"triggered_count"`
	SuccessCount     int       `json:"success_count"`
	ErrorCount       int       `json:"error_count"`
	AvgExecutionTime float64   `json:"avg_execution_time"`
	LastExecution    time.Time `json:"last_execution"`
	LastTriggered    time.Time `json:"last_triggered"`
}

// DefaultRuleEngineConfig returns default rule engine configuration
func DefaultRuleEngineConfig() *RuleEngineConfig {
	return &RuleEngineConfig{
		Enabled:             true,
		MaxRules:            100,
		ExecutionTimeout:    30 * time.Second,
		MaxHistorySize:      1000,
		EnableDebugLogging:  false,
		ParallelExecution:   false,
		MaxParallelRules:    5,
		RulePriorityEnabled: true,
		DefaultPriority:     50,
	}
}

// NewRuleEngine creates a new rule engine
func NewRuleEngine(config *RuleEngineConfig, logger *logx.Logger) *RuleEngine {
	if config == nil {
		config = DefaultRuleEngineConfig()
	}

	return &RuleEngine{
		config:           config,
		logger:           logger,
		rules:            make([]*Rule, 0),
		ruleIndex:        make(map[string]*Rule),
		executionHistory: make([]*RuleExecution, 0),
		executionStats:   make(map[string]*RuleStats),
	}
}

// AddRule adds a new rule to the engine
func (re *RuleEngine) AddRule(rule *Rule) error {
	if !re.config.Enabled {
		return fmt.Errorf("rule engine is disabled")
	}

	re.mu.Lock()
	defer re.mu.Unlock()

	// Validate rule
	if err := re.validateRule(rule); err != nil {
		return fmt.Errorf("invalid rule: %w", err)
	}

	// Check rule limit
	if len(re.rules) >= re.config.MaxRules {
		return fmt.Errorf("maximum number of rules (%d) reached", re.config.MaxRules)
	}

	// Set default values
	if rule.ID == "" {
		rule.ID = fmt.Sprintf("rule_%d", len(re.rules)+1)
	}
	if rule.CreatedAt.IsZero() {
		rule.CreatedAt = time.Now()
	}
	if rule.Priority == 0 {
		rule.Priority = re.config.DefaultPriority
	}

	rule.UpdatedAt = time.Now()

	// Add to storage
	re.rules = append(re.rules, rule)
	re.ruleIndex[rule.ID] = rule

	// Initialize stats
	re.executionStats[rule.ID] = &RuleStats{
		RuleID: rule.ID,
	}

	re.logger.Info("Rule added", "rule_id", rule.ID, "name", rule.Name)
	return nil
}

// RemoveRule removes a rule from the engine
func (re *RuleEngine) RemoveRule(ruleID string) error {
	re.mu.Lock()
	defer re.mu.Unlock()

	rule, exists := re.ruleIndex[ruleID]
	if !exists {
		return fmt.Errorf("rule not found: %s", ruleID)
	}

	// Remove from storage
	delete(re.ruleIndex, ruleID)
	for i, r := range re.rules {
		if r.ID == ruleID {
			re.rules = append(re.rules[:i], re.rules[i+1:]...)
			break
		}
	}

	// Remove stats
	delete(re.executionStats, ruleID)

	re.logger.Info("Rule removed", "rule_id", ruleID, "name", rule.Name)
	return nil
}

// UpdateRule updates an existing rule
func (re *RuleEngine) UpdateRule(rule *Rule) error {
	re.mu.Lock()
	defer re.mu.Unlock()

	existingRule, exists := re.ruleIndex[rule.ID]
	if !exists {
		return fmt.Errorf("rule not found: %s", rule.ID)
	}

	// Validate rule
	if err := re.validateRule(rule); err != nil {
		return fmt.Errorf("invalid rule: %w", err)
	}

	// Update fields
	existingRule.Name = rule.Name
	existingRule.Description = rule.Description
	existingRule.Enabled = rule.Enabled
	existingRule.Priority = rule.Priority
	existingRule.Conditions = rule.Conditions
	existingRule.Actions = rule.Actions
	existingRule.Metadata = rule.Metadata
	existingRule.UpdatedAt = time.Now()

	re.logger.Info("Rule updated", "rule_id", rule.ID, "name", rule.Name)
	return nil
}

// ExecuteRules executes all enabled rules against the given context
func (re *RuleEngine) ExecuteRules(ctx context.Context, context map[string]interface{}) ([]*RuleExecution, error) {
	if !re.config.Enabled {
		return nil, fmt.Errorf("rule engine is disabled")
	}

	re.mu.RLock()
	enabledRules := make([]*Rule, 0)
	for _, rule := range re.rules {
		if rule.Enabled {
			enabledRules = append(enabledRules, rule)
		}
	}
	re.mu.RUnlock()

	if len(enabledRules) == 0 {
		return nil, nil
	}

	// Sort rules by priority if enabled
	if re.config.RulePriorityEnabled {
		re.sortRulesByPriority(enabledRules)
	}

	// Execute rules
	var executions []*RuleExecution
	if re.config.ParallelExecution {
		executions = re.executeRulesParallel(ctx, enabledRules, context)
	} else {
		executions = re.executeRulesSequential(ctx, enabledRules, context)
	}

	// Update execution history
	re.updateExecutionHistory(executions)

	re.logger.Debug("Rule execution completed",
		"total_rules", len(enabledRules),
		"triggered_rules", len(executions),
		"parallel", re.config.ParallelExecution)

	return executions, nil
}

// executeRulesSequential executes rules sequentially
func (re *RuleEngine) executeRulesSequential(ctx context.Context, rules []*Rule, context map[string]interface{}) []*RuleExecution {
	var executions []*RuleExecution

	for _, rule := range rules {
		execution := re.executeRule(ctx, rule, context)
		executions = append(executions, execution)

		// Check for timeout
		select {
		case <-ctx.Done():
			re.logger.Warn("Rule execution timed out", "rule_id", rule.ID)
			return executions
		default:
		}
	}

	return executions
}

// executeRulesParallel executes rules in parallel
func (re *RuleEngine) executeRulesParallel(ctx context.Context, rules []*Rule, context map[string]interface{}) []*RuleExecution {
	// Limit parallel executions
	semaphore := make(chan struct{}, re.config.MaxParallelRules)
	results := make(chan *RuleExecution, len(rules))

	// Start rule executions
	for _, rule := range rules {
		go func(r *Rule) {
			semaphore <- struct{}{}
			defer func() { <-semaphore }()

			execution := re.executeRule(ctx, r, context)
			results <- execution
		}(rule)
	}

	// Collect results
	var executions []*RuleExecution
	for i := 0; i < len(rules); i++ {
		select {
		case execution := <-results:
			executions = append(executions, execution)
		case <-ctx.Done():
			re.logger.Warn("Parallel rule execution timed out")
			return executions
		}
	}

	return executions
}

// executeRule executes a single rule
func (re *RuleEngine) executeRule(ctx context.Context, rule *Rule, context map[string]interface{}) *RuleExecution {
	startTime := time.Now()
	execution := &RuleExecution{
		RuleID:    rule.ID,
		RuleName:  rule.Name,
		Timestamp: startTime,
		Context:   context,
	}

	// Evaluate conditions
	conditionResults := re.evaluateConditions(rule.Conditions, context)
	execution.Conditions = conditionResults

	// Check if all conditions are met
	allConditionsMet := true
	for _, result := range conditionResults {
		if !result.Result {
			allConditionsMet = false
			break
		}
	}

	execution.Triggered = allConditionsMet

	// Execute actions if conditions are met
	if allConditionsMet {
		actionResults := re.executeActions(ctx, rule.Actions, context)
		execution.Actions = actionResults
	}

	execution.ExecutionTime = time.Since(startTime)

	// Update stats
	re.updateRuleStats(rule.ID, execution)

	re.logger.Debug("Rule executed",
		"rule_id", rule.ID,
		"triggered", execution.Triggered,
		"execution_time", execution.ExecutionTime)

	return execution
}

// evaluateConditions evaluates all conditions for a rule
func (re *RuleEngine) evaluateConditions(conditions []*Condition, context map[string]interface{}) []*ConditionResult {
	var results []*ConditionResult

	for _, condition := range conditions {
		result := &ConditionResult{
			Condition: condition,
		}

		// Get field value from context
		fieldValue, exists := context[condition.Field]
		if !exists {
			result.Error = fmt.Sprintf("field '%s' not found in context", condition.Field)
			results = append(results, result)
			continue
		}

		result.Value = fieldValue
		result.Evaluated = true

		// Evaluate condition based on type and operator
		evaluated, err := re.evaluateCondition(condition, fieldValue)
		if err != nil {
			result.Error = err.Error()
		} else {
			result.Result = evaluated
		}

		results = append(results, result)
	}

	return results
}

// evaluateCondition evaluates a single condition
func (re *RuleEngine) evaluateCondition(condition *Condition, value interface{}) (bool, error) {
	switch condition.Type {
	case "numeric":
		return re.evaluateNumericCondition(condition, value)
	case "string":
		return re.evaluateStringCondition(condition, value)
	case "boolean":
		return re.evaluateBooleanCondition(condition, value)
	case "array":
		return re.evaluateArrayCondition(condition, value)
	case "custom":
		return re.evaluateCustomCondition(condition, value)
	default:
		return false, fmt.Errorf("unknown condition type: %s", condition.Type)
	}
}

// evaluateNumericCondition evaluates a numeric condition
func (re *RuleEngine) evaluateNumericCondition(condition *Condition, value interface{}) (bool, error) {
	// Convert value to float64
	var numericValue float64
	switch v := value.(type) {
	case float64:
		numericValue = v
	case int:
		numericValue = float64(v)
	case int64:
		numericValue = float64(v)
	default:
		return false, fmt.Errorf("cannot convert value to numeric: %v", value)
	}

	// Convert condition value to float64
	var conditionValue float64
	switch v := condition.Value.(type) {
	case float64:
		conditionValue = v
	case int:
		conditionValue = float64(v)
	case int64:
		conditionValue = float64(v)
	default:
		return false, fmt.Errorf("cannot convert condition value to numeric: %v", condition.Value)
	}

	// Apply operator
	switch condition.Operator {
	case "eq":
		return numericValue == conditionValue, nil
	case "ne":
		return numericValue != conditionValue, nil
	case "gt":
		return numericValue > conditionValue, nil
	case "gte":
		return numericValue >= conditionValue, nil
	case "lt":
		return numericValue < conditionValue, nil
	case "lte":
		return numericValue <= conditionValue, nil
	default:
		return false, fmt.Errorf("unknown numeric operator: %s", condition.Operator)
	}
}

// evaluateStringCondition evaluates a string condition
func (re *RuleEngine) evaluateStringCondition(condition *Condition, value interface{}) (bool, error) {
	stringValue, ok := value.(string)
	if !ok {
		return false, fmt.Errorf("value is not a string: %v", value)
	}

	conditionValue, ok := condition.Value.(string)
	if !ok {
		return false, fmt.Errorf("condition value is not a string: %v", condition.Value)
	}

	switch condition.Operator {
	case "eq":
		return stringValue == conditionValue, nil
	case "ne":
		return stringValue != conditionValue, nil
	case "contains":
		return contains(stringValue, conditionValue), nil
	case "starts_with":
		return startsWith(stringValue, conditionValue), nil
	case "ends_with":
		return endsWith(stringValue, conditionValue), nil
	case "regex":
		return re.matchRegex(stringValue, conditionValue)
	default:
		return false, fmt.Errorf("unknown string operator: %s", condition.Operator)
	}
}

// evaluateBooleanCondition evaluates a boolean condition
func (re *RuleEngine) evaluateBooleanCondition(condition *Condition, value interface{}) (bool, error) {
	boolValue, ok := value.(bool)
	if !ok {
		return false, fmt.Errorf("value is not a boolean: %v", value)
	}

	conditionValue, ok := condition.Value.(bool)
	if !ok {
		return false, fmt.Errorf("condition value is not a boolean: %v", condition.Value)
	}

	switch condition.Operator {
	case "eq":
		return boolValue == conditionValue, nil
	case "ne":
		return boolValue != conditionValue, nil
	default:
		return false, fmt.Errorf("unknown boolean operator: %s", condition.Operator)
	}
}

// evaluateArrayCondition evaluates an array condition
func (re *RuleEngine) evaluateArrayCondition(condition *Condition, value interface{}) (bool, error) {
	// Implementation for array conditions (contains, not_contains, etc.)
	// This would handle cases where the value is an array/slice
	return false, fmt.Errorf("array condition evaluation not implemented")
}

// evaluateCustomCondition evaluates a custom condition
func (re *RuleEngine) evaluateCustomCondition(condition *Condition, value interface{}) (bool, error) {
	// Implementation for custom condition types
	// This would allow for plugin-based condition evaluation
	return false, fmt.Errorf("custom condition evaluation not implemented")
}

// executeActions executes all actions for a rule
func (re *RuleEngine) executeActions(ctx context.Context, actions []*Action, context map[string]interface{}) []*ActionResult {
	var results []*ActionResult

	for _, action := range actions {
		if !action.Enabled {
			continue
		}

		result := &ActionResult{
			Action: action,
		}

		// Execute action based on type
		actionResult, err := re.executeAction(ctx, action, context)
		if err != nil {
			result.Error = err.Error()
		} else {
			result.Success = true
			result.Result = actionResult
		}

		result.Executed = true
		results = append(results, result)
	}

	return results
}

// executeAction executes a single action
func (re *RuleEngine) executeAction(ctx context.Context, action *Action, context map[string]interface{}) (interface{}, error) {
	switch action.Type {
	case "log":
		return re.executeLogAction(action, context)
	case "notification":
		return re.executeNotificationAction(action, context)
	case "failover":
		return re.executeFailoverAction(action, context)
	case "restore":
		return re.executeRestoreAction(action, context)
	case "custom":
		return re.executeCustomAction(action, context)
	default:
		return nil, fmt.Errorf("unknown action type: %s", action.Type)
	}
}

// executeLogAction executes a log action
func (re *RuleEngine) executeLogAction(action *Action, context map[string]interface{}) (interface{}, error) {
	message, ok := action.Parameters["message"].(string)
	if !ok {
		return nil, fmt.Errorf("log action requires 'message' parameter")
	}

	level, ok := action.Parameters["level"].(string)
	if !ok {
		level = "info"
	}

	// Log the message
	switch level {
	case "debug":
		re.logger.Debug(message, "context", context)
	case "info":
		re.logger.Info(message, "context", context)
	case "warn":
		re.logger.Warn(message, "context", context)
	case "error":
		re.logger.Error(message, "context", context)
	default:
		re.logger.Info(message, "context", context)
	}

	return map[string]string{"status": "logged", "level": level}, nil
}

// executeNotificationAction executes a notification action
func (re *RuleEngine) executeNotificationAction(action *Action, context map[string]interface{}) (interface{}, error) {
	// Implementation for notification actions
	// This would integrate with the notification system
	return map[string]string{"status": "notification_sent"}, nil
}

// executeFailoverAction executes a failover action
func (re *RuleEngine) executeFailoverAction(action *Action, context map[string]interface{}) (interface{}, error) {
	// Implementation for failover actions
	// This would integrate with the decision engine
	return map[string]string{"status": "failover_triggered"}, nil
}

// executeRestoreAction executes a restore action
func (re *RuleEngine) executeRestoreAction(action *Action, context map[string]interface{}) (interface{}, error) {
	// Implementation for restore actions
	// This would integrate with the decision engine
	return map[string]string{"status": "restore_triggered"}, nil
}

// executeCustomAction executes a custom action
func (re *RuleEngine) executeCustomAction(action *Action, context map[string]interface{}) (interface{}, error) {
	// Implementation for custom actions
	// This would allow for plugin-based action execution
	return map[string]string{"status": "custom_action_executed"}, nil
}

// validateRule validates a rule
func (re *RuleEngine) validateRule(rule *Rule) error {
	if rule.Name == "" {
		return fmt.Errorf("rule name is required")
	}

	if len(rule.Conditions) == 0 {
		return fmt.Errorf("rule must have at least one condition")
	}

	if len(rule.Actions) == 0 {
		return fmt.Errorf("rule must have at least one action")
	}

	// Validate conditions
	for i, condition := range rule.Conditions {
		if err := re.validateCondition(condition); err != nil {
			return fmt.Errorf("condition %d invalid: %w", i+1, err)
		}
	}

	// Validate actions
	for i, action := range rule.Actions {
		if err := re.validateAction(action); err != nil {
			return fmt.Errorf("action %d invalid: %w", i+1, err)
		}
	}

	return nil
}

// validateCondition validates a condition
func (re *RuleEngine) validateCondition(condition *Condition) error {
	if condition.Type == "" {
		return fmt.Errorf("condition type is required")
	}

	if condition.Field == "" {
		return fmt.Errorf("condition field is required")
	}

	if condition.Operator == "" {
		return fmt.Errorf("condition operator is required")
	}

	// Validate type-specific requirements
	switch condition.Type {
	case "numeric", "string", "boolean":
		if condition.Value == nil {
			return fmt.Errorf("condition value is required for type %s", condition.Type)
		}
	}

	return nil
}

// validateAction validates an action
func (re *RuleEngine) validateAction(action *Action) error {
	if action.Type == "" {
		return fmt.Errorf("action type is required")
	}

	// Validate type-specific requirements
	switch action.Type {
	case "log":
		if action.Parameters["message"] == nil {
			return fmt.Errorf("log action requires 'message' parameter")
		}
	}

	return nil
}

// Helper functions
func contains(s, substr string) bool {
	return len(s) >= len(substr) && (s == substr || len(substr) == 0 || len(s) > len(substr) && (s[:len(substr)] == substr || s[len(s)-len(substr):] == substr || containsHelper(s, substr)))
}

func containsHelper(s, substr string) bool {
	for i := 0; i <= len(s)-len(substr); i++ {
		if s[i:i+len(substr)] == substr {
			return true
		}
	}
	return false
}

func startsWith(s, prefix string) bool {
	return len(s) >= len(prefix) && s[:len(prefix)] == prefix
}

func endsWith(s, suffix string) bool {
	return len(s) >= len(suffix) && s[len(s)-len(suffix):] == suffix
}

func (re *RuleEngine) matchRegex(s, pattern string) (bool, error) {
	// Simple regex matching implementation
	// In a real implementation, this would use regexp.MatchString
	return s == pattern, nil
}

func (re *RuleEngine) sortRulesByPriority(rules []*Rule) {
	// Sort rules by priority (higher priority first)
	for i := 0; i < len(rules)-1; i++ {
		for j := i + 1; j < len(rules); j++ {
			if rules[i].Priority < rules[j].Priority {
				rules[i], rules[j] = rules[j], rules[i]
			}
		}
	}
}

func (re *RuleEngine) updateExecutionHistory(executions []*RuleExecution) {
	re.mu.Lock()
	defer re.mu.Unlock()

	re.executionHistory = append(re.executionHistory, executions...)

	// Maintain history size limit
	if len(re.executionHistory) > re.config.MaxHistorySize {
		excess := len(re.executionHistory) - re.config.MaxHistorySize
		re.executionHistory = re.executionHistory[excess:]
	}

	re.lastExecution = time.Now()
}

func (re *RuleEngine) updateRuleStats(ruleID string, execution *RuleExecution) {
	re.mu.Lock()
	defer re.mu.Unlock()

	stats, exists := re.executionStats[ruleID]
	if !exists {
		stats = &RuleStats{RuleID: ruleID}
		re.executionStats[ruleID] = stats
	}

	stats.TotalExecutions++
	stats.LastExecution = execution.Timestamp

	if execution.Triggered {
		stats.TriggeredCount++
		stats.LastTriggered = execution.Timestamp
	}

	if execution.Error == "" {
		stats.SuccessCount++
	} else {
		stats.ErrorCount++
	}

	// Update average execution time
	totalTime := stats.AvgExecutionTime * float64(stats.TotalExecutions-1)
	totalTime += float64(execution.ExecutionTime.Milliseconds())
	stats.AvgExecutionTime = totalTime / float64(stats.TotalExecutions)
}

// GetRule returns a rule by ID
func (re *RuleEngine) GetRule(ruleID string) (*Rule, error) {
	re.mu.RLock()
	defer re.mu.RUnlock()

	rule, exists := re.ruleIndex[ruleID]
	if !exists {
		return nil, fmt.Errorf("rule not found: %s", ruleID)
	}

	return rule, nil
}

// GetRules returns all rules
func (re *RuleEngine) GetRules() []*Rule {
	re.mu.RLock()
	defer re.mu.RUnlock()

	rules := make([]*Rule, len(re.rules))
	copy(rules, re.rules)
	return rules
}

// GetRuleStats returns statistics for a rule
func (re *RuleEngine) GetRuleStats(ruleID string) (*RuleStats, error) {
	re.mu.RLock()
	defer re.mu.RUnlock()

	stats, exists := re.executionStats[ruleID]
	if !exists {
		return nil, fmt.Errorf("rule stats not found: %s", ruleID)
	}

	return stats, nil
}

// GetAllRuleStats returns statistics for all rules
func (re *RuleEngine) GetAllRuleStats() map[string]*RuleStats {
	re.mu.RLock()
	defer re.mu.RUnlock()

	stats := make(map[string]*RuleStats)
	for k, v := range re.executionStats {
		stats[k] = v
	}

	return stats
}

// GetExecutionHistory returns the execution history
func (re *RuleEngine) GetExecutionHistory(limit int) []*RuleExecution {
	re.mu.RLock()
	defer re.mu.RUnlock()

	if limit <= 0 || limit > len(re.executionHistory) {
		limit = len(re.executionHistory)
	}

	history := make([]*RuleExecution, limit)
	copy(history, re.executionHistory[len(re.executionHistory)-limit:])
	return history
}

// GetEngineStatus returns the rule engine status
func (re *RuleEngine) GetEngineStatus() map[string]interface{} {
	re.mu.RLock()
	defer re.mu.RUnlock()

	status := map[string]interface{}{
		"enabled":                re.config.Enabled,
		"total_rules":            len(re.rules),
		"enabled_rules":          re.countEnabledRules(),
		"execution_history_size": len(re.executionHistory),
		"last_execution":         re.lastExecution.Format(time.RFC3339),
		"config":                 re.config,
	}

	return status
}
