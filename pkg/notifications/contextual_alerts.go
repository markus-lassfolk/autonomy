package notifications

import (
	"context"
	"fmt"
	"strings"
	"time"

	"github.com/markus-lassfolk/autonomy/pkg"
	"github.com/markus-lassfolk/autonomy/pkg/logx"
)

// ContextualAlertManager provides intelligent context-aware notifications
type ContextualAlertManager struct {
	logger       *logx.Logger
	smartManager *SmartNotificationManager

	// Context enrichment
	locationProvider LocationProvider
	metricsProvider  MetricsProvider
	systemProvider   SystemProvider

	// Alert templates and rules
	alertTemplates map[AlertType]*AlertTemplate
	contextRules   []ContextRule

	// State tracking for intelligent alerts
	lastKnownState map[string]interface{}
	alertHistory   []ContextualAlert
}

// LocationProvider interface for getting location context
type LocationProvider interface {
	GetCurrentLocation() (*LocationContext, error)
	GetLocationHistory(duration time.Duration) ([]LocationContext, error)
}

// MetricsProvider interface for getting performance metrics context
type MetricsProvider interface {
	GetCurrentMetrics(interfaceName string) (*pkg.Metrics, error)
	GetMetricsHistory(interfaceName string, duration time.Duration) ([]*pkg.Metrics, error)
	GetSystemLoad() (*SystemLoadMetrics, error)
}

// SystemProvider interface for getting system context
type SystemProvider interface {
	GetSystemInfo() (*SystemInfo, error)
	GetNetworkTopology() (*NetworkTopology, error)
	GetMaintenanceStatus() (*MaintenanceStatus, error)
}

// LocationContext provides location-based context for alerts
type LocationContext struct {
	Latitude     float64   `json:"latitude"`
	Longitude    float64   `json:"longitude"`
	Accuracy     float64   `json:"accuracy"`
	Address      string    `json:"address,omitempty"`
	Timezone     string    `json:"timezone"`
	Weather      *Weather  `json:"weather,omitempty"`
	MovementInfo *Movement `json:"movement,omitempty"`
	Timestamp    time.Time `json:"timestamp"`
}

// Weather provides weather context that might affect connectivity
type Weather struct {
	Condition     string  `json:"condition"`
	Temperature   float64 `json:"temperature"`
	Humidity      float64 `json:"humidity"`
	WindSpeed     float64 `json:"wind_speed"`
	Visibility    float64 `json:"visibility"`
	Precipitation float64 `json:"precipitation"`
}

// Movement provides movement context for mobile scenarios
type Movement struct {
	Speed          float64       `json:"speed"`          // m/s
	Direction      float64       `json:"direction"`      // degrees
	DistanceMoved  float64       `json:"distance_moved"` // meters since last check
	IsStationary   bool          `json:"is_stationary"`
	StationaryTime time.Duration `json:"stationary_time"`
}

// SystemLoadMetrics provides system performance context
type SystemLoadMetrics struct {
	CPUUsage    float64       `json:"cpu_usage"`
	MemoryUsage float64       `json:"memory_usage"`
	DiskUsage   float64       `json:"disk_usage"`
	Temperature float64       `json:"temperature"`
	Uptime      time.Duration `json:"uptime"`
}

// SystemInfo provides general system context
type SystemInfo struct {
	Hostname     string        `json:"hostname"`
	Model        string        `json:"model"`
	Firmware     string        `json:"firmware"`
	SerialNumber string        `json:"serial_number"`
	Uptime       time.Duration `json:"uptime"`
	LocalTime    time.Time     `json:"local_time"`
}

// NetworkTopology provides network context
type NetworkTopology struct {
	ActiveInterfaces []InterfaceInfo `json:"active_interfaces"`
	PrimaryInterface string          `json:"primary_interface"`
	BackupInterfaces []string        `json:"backup_interfaces"`
	TotalBandwidth   int64           `json:"total_bandwidth"`
	DataUsage        *DataUsageInfo  `json:"data_usage,omitempty"`
}

// InterfaceInfo provides detailed interface context
type InterfaceInfo struct {
	Name       string                 `json:"name"`
	Type       string                 `json:"type"`
	Status     string                 `json:"status"`
	Metrics    *pkg.Metrics           `json:"metrics,omitempty"`
	Provider   string                 `json:"provider,omitempty"`
	SignalInfo map[string]interface{} `json:"signal_info,omitempty"`
}

// DataUsageInfo provides data usage context
type DataUsageInfo struct {
	TotalUsed    int64     `json:"total_used"`
	TotalLimit   int64     `json:"total_limit"`
	UsagePercent float64   `json:"usage_percent"`
	ResetDate    time.Time `json:"reset_date"`
	DailyAverage int64     `json:"daily_average"`
}

// MaintenanceStatus provides maintenance context
type MaintenanceStatus struct {
	InMaintenance   bool      `json:"in_maintenance"`
	MaintenanceType string    `json:"maintenance_type,omitempty"`
	StartTime       time.Time `json:"start_time,omitempty"`
	EstimatedEnd    time.Time `json:"estimated_end,omitempty"`
	Description     string    `json:"description,omitempty"`
}

// AlertType defines different types of contextual alerts
type AlertType string

const (
	AlertFailover          AlertType = "failover"
	AlertFailback          AlertType = "failback"
	AlertInterfaceDown     AlertType = "interface_down"
	AlertInterfaceUp       AlertType = "interface_up"
	AlertPredictive        AlertType = "predictive"
	AlertPerformanceDeg    AlertType = "performance_degradation"
	AlertDataLimit         AlertType = "data_limit"
	AlertThermal           AlertType = "thermal"
	AlertObstruction       AlertType = "obstruction"
	AlertSignalLoss        AlertType = "signal_loss"
	AlertConnectivityIssue AlertType = "connectivity_issue"
	AlertSystemHealth      AlertType = "system_health"
	AlertMaintenance       AlertType = "maintenance"
	AlertSecurity          AlertType = "security"
)

// AlertTemplate defines how to format alerts for specific types
type AlertTemplate struct {
	Type            AlertType         `json:"type"`
	TitleTemplate   string            `json:"title_template"`
	MessageTemplate string            `json:"message_template"`
	Priority        int               `json:"priority"`
	RequiredContext []string          `json:"required_context"`
	Enrichers       []ContextEnricher `json:"enrichers"`
	Actions         []SuggestedAction `json:"actions"`
	Escalation      *EscalationRule   `json:"escalation,omitempty"`
}

// ContextEnricher defines how to enrich alerts with additional context
type ContextEnricher struct {
	Name       string              `json:"name"`
	Type       string              `json:"type"` // "location", "metrics", "system", "weather"
	Conditions []EnricherCondition `json:"conditions,omitempty"`
	Template   string              `json:"template"`
	Priority   int                 `json:"priority"`
}

// EnricherCondition defines when to apply context enrichment
type EnricherCondition struct {
	Field    string      `json:"field"`
	Operator string      `json:"operator"`
	Value    interface{} `json:"value"`
}

// SuggestedAction provides actionable recommendations
type SuggestedAction struct {
	Title       string `json:"title"`
	Description string `json:"description"`
	Command     string `json:"command,omitempty"`
	URL         string `json:"url,omitempty"`
	Priority    int    `json:"priority"`
	Automated   bool   `json:"automated"`
}

// EscalationRule defines when and how to escalate alerts
type EscalationRule struct {
	Threshold  int           `json:"threshold"` // Number of occurrences
	TimeWindow time.Duration `json:"time_window"`
	EscalateTo int           `json:"escalate_to"` // New priority level
	AddContext []string      `json:"add_context"`
}

// ContextRule defines rules for adding context based on conditions
type ContextRule struct {
	ID         string          `json:"id"`
	Name       string          `json:"name"`
	Conditions []RuleCondition `json:"conditions"`
	Actions    []ContextAction `json:"actions"`
	Priority   int             `json:"priority"`
	Enabled    bool            `json:"enabled"`
}

// RuleCondition defines conditions for context rules
type RuleCondition struct {
	Type     string      `json:"type"` // "alert_type", "location", "time", "metrics", "system"
	Field    string      `json:"field"`
	Operator string      `json:"operator"`
	Value    interface{} `json:"value"`
}

// ContextAction defines actions to take when context rules match
type ContextAction struct {
	Type       string                 `json:"type"` // "add_context", "modify_priority", "add_action", "suppress"
	Parameters map[string]interface{} `json:"parameters"`
}

// ContextualAlert represents an enriched alert with context
type ContextualAlert struct {
	BaseNotification *Notification       `json:"base_notification"`
	AlertType        AlertType           `json:"alert_type"`
	Context          *AlertContext       `json:"context"`
	SuggestedActions []SuggestedAction   `json:"suggested_actions"`
	Enrichments      []ContextEnrichment `json:"enrichments"`
	Timestamp        time.Time           `json:"timestamp"`
	ProcessingTime   time.Duration       `json:"processing_time"`
}

// AlertContext contains all contextual information for an alert
type AlertContext struct {
	Location      *LocationContext       `json:"location,omitempty"`
	SystemLoad    *SystemLoadMetrics     `json:"system_load,omitempty"`
	SystemInfo    *SystemInfo            `json:"system_info,omitempty"`
	NetworkTopo   *NetworkTopology       `json:"network_topology,omitempty"`
	Maintenance   *MaintenanceStatus     `json:"maintenance,omitempty"`
	InterfaceInfo *InterfaceInfo         `json:"interface_info,omitempty"`
	Metrics       *pkg.Metrics           `json:"metrics,omitempty"`
	Historical    *HistoricalContext     `json:"historical,omitempty"`
	Custom        map[string]interface{} `json:"custom,omitempty"`
}

// HistoricalContext provides historical context for pattern analysis
type HistoricalContext struct {
	SimilarEvents     []ContextualAlert `json:"similar_events"`
	PatternDetected   bool              `json:"pattern_detected"`
	PatternType       string            `json:"pattern_type,omitempty"`
	Frequency         float64           `json:"frequency"` // Events per day
	LastOccurrence    time.Time         `json:"last_occurrence"`
	AverageResolution time.Duration     `json:"average_resolution"`
}

// ContextEnrichment represents applied context enrichment
type ContextEnrichment struct {
	EnricherName string                 `json:"enricher_name"`
	Type         string                 `json:"type"`
	Content      string                 `json:"content"`
	Data         map[string]interface{} `json:"data,omitempty"`
	AppliedAt    time.Time              `json:"applied_at"`
}

// NewContextualAlertManager creates a new contextual alert manager
func NewContextualAlertManager(
	smartManager *SmartNotificationManager,
	locationProvider LocationProvider,
	metricsProvider MetricsProvider,
	systemProvider SystemProvider,
	logger *logx.Logger,
) *ContextualAlertManager {
	cam := &ContextualAlertManager{
		logger:           logger,
		smartManager:     smartManager,
		locationProvider: locationProvider,
		metricsProvider:  metricsProvider,
		systemProvider:   systemProvider,
		alertTemplates:   make(map[AlertType]*AlertTemplate),
		contextRules:     make([]ContextRule, 0),
		lastKnownState:   make(map[string]interface{}),
		alertHistory:     make([]ContextualAlert, 0),
	}

	// Initialize default alert templates
	cam.initializeDefaultTemplates()
	cam.initializeDefaultContextRules()

	return cam
}

// SendContextualAlert processes and sends a contextual alert
func (cam *ContextualAlertManager) SendContextualAlert(ctx context.Context, alertType AlertType, baseData map[string]interface{}) error {
	startTime := time.Now()

	// Get alert template
	template, exists := cam.alertTemplates[alertType]
	if !exists {
		return fmt.Errorf("no template found for alert type: %s", alertType)
	}

	// Gather context
	alertContext, err := cam.gatherContext(ctx, alertType, baseData)
	if err != nil {
		cam.logger.Warn("Failed to gather full context for alert", "error", err, "alert_type", alertType)
		// Continue with partial context
	}

	// Apply context rules
	cam.applyContextRules(alertType, alertContext, baseData)

	// Enrich with template enrichers
	enrichments := cam.applyEnrichments(template, alertContext, baseData)

	// Generate suggested actions
	actions := cam.generateSuggestedActions(template, alertContext, baseData)

	// Create base notification
	notification := cam.createBaseNotification(template, alertContext, baseData, enrichments)

	// Create contextual alert
	contextualAlert := &ContextualAlert{
		BaseNotification: notification,
		AlertType:        alertType,
		Context:          alertContext,
		SuggestedActions: actions,
		Enrichments:      enrichments,
		Timestamp:        time.Now(),
		ProcessingTime:   time.Since(startTime),
	}

	// Add to history
	cam.addToHistory(contextualAlert)

	// Send through smart manager
	err = cam.smartManager.SendNotification(ctx, notification)
	if err != nil {
		cam.logger.Error("Failed to send contextual alert", "error", err, "alert_type", alertType)
		return err
	}

	cam.logger.Info("Sent contextual alert",
		"alert_type", alertType,
		"processing_time", time.Since(startTime),
		"enrichments", len(enrichments),
		"actions", len(actions))

	return nil
}

// gatherContext collects all available context for the alert
func (cam *ContextualAlertManager) gatherContext(ctx context.Context, alertType AlertType, baseData map[string]interface{}) (*AlertContext, error) {
	alertContext := &AlertContext{
		Custom: make(map[string]interface{}),
	}

	// Gather location context
	if cam.locationProvider != nil {
		if location, err := cam.locationProvider.GetCurrentLocation(); err == nil {
			alertContext.Location = location
		}
	}

	// Gather system context
	if cam.systemProvider != nil {
		if systemInfo, err := cam.systemProvider.GetSystemInfo(); err == nil {
			alertContext.SystemInfo = systemInfo
		}

		if networkTopo, err := cam.systemProvider.GetNetworkTopology(); err == nil {
			alertContext.NetworkTopo = networkTopo
		}

		if maintenance, err := cam.systemProvider.GetMaintenanceStatus(); err == nil {
			alertContext.Maintenance = maintenance
		}
	}

	// Gather metrics context
	if cam.metricsProvider != nil {
		if systemLoad, err := cam.metricsProvider.GetSystemLoad(); err == nil {
			alertContext.SystemLoad = systemLoad
		}

		// Get interface-specific metrics if interface is specified
		if interfaceName, ok := baseData["interface"].(string); ok && interfaceName != "" {
			if metrics, err := cam.metricsProvider.GetCurrentMetrics(interfaceName); err == nil {
				alertContext.Metrics = metrics
			}
		}
	}

	// Gather historical context
	alertContext.Historical = cam.gatherHistoricalContext(alertType)

	// Add base data to custom context
	for key, value := range baseData {
		alertContext.Custom[key] = value
	}

	return alertContext, nil
}

// gatherHistoricalContext analyzes historical patterns for the alert type
func (cam *ContextualAlertManager) gatherHistoricalContext(alertType AlertType) *HistoricalContext {
	historical := &HistoricalContext{
		SimilarEvents: make([]ContextualAlert, 0),
	}

	// Look for similar events in the last 30 days
	cutoff := time.Now().Add(-30 * 24 * time.Hour)
	var similarEvents []ContextualAlert

	for _, alert := range cam.alertHistory {
		if alert.AlertType == alertType && alert.Timestamp.After(cutoff) {
			similarEvents = append(similarEvents, alert)
		}
	}

	if len(similarEvents) > 0 {
		historical.SimilarEvents = similarEvents
		historical.Frequency = float64(len(similarEvents)) / 30.0 // Events per day
		historical.LastOccurrence = similarEvents[len(similarEvents)-1].Timestamp

		// Detect patterns
		if len(similarEvents) >= 3 {
			historical.PatternDetected = true
			historical.PatternType = cam.detectPattern(similarEvents)
		}
	}

	return historical
}

// detectPattern analyzes similar events to detect patterns
func (cam *ContextualAlertManager) detectPattern(events []ContextualAlert) string {
	if len(events) < 3 {
		return "insufficient_data"
	}

	// Check for time-based patterns
	hours := make(map[int]int)
	days := make(map[time.Weekday]int)

	for _, event := range events {
		hours[event.Timestamp.Hour()]++
		days[event.Timestamp.Weekday()]++
	}

	// Check for hourly patterns
	maxHourCount := 0
	for _, count := range hours {
		if count > maxHourCount {
			maxHourCount = count
		}
	}

	if maxHourCount >= len(events)/2 {
		return "time_based"
	}

	// Check for daily patterns
	maxDayCount := 0
	for _, count := range days {
		if count > maxDayCount {
			maxDayCount = count
		}
	}

	if maxDayCount >= len(events)/2 {
		return "day_based"
	}

	// Check for frequency patterns
	intervals := make([]time.Duration, 0)
	for i := 1; i < len(events); i++ {
		interval := events[i].Timestamp.Sub(events[i-1].Timestamp)
		intervals = append(intervals, interval)
	}

	// Simple frequency analysis
	if len(intervals) > 0 {
		avgInterval := time.Duration(0)
		for _, interval := range intervals {
			avgInterval += interval
		}
		avgInterval /= time.Duration(len(intervals))

		if avgInterval < 1*time.Hour {
			return "high_frequency"
		} else if avgInterval < 24*time.Hour {
			return "regular_frequency"
		}
	}

	return "irregular"
}

// applyContextRules applies context rules to modify alert behavior
func (cam *ContextualAlertManager) applyContextRules(alertType AlertType, alertContext *AlertContext, baseData map[string]interface{}) {
	for _, rule := range cam.contextRules {
		if !rule.Enabled {
			continue
		}

		if cam.evaluateRuleConditions(rule.Conditions, alertType, alertContext, baseData) {
			cam.executeContextActions(rule.Actions, alertContext, baseData)
		}
	}
}

// evaluateRuleConditions checks if all conditions in a rule are met
func (cam *ContextualAlertManager) evaluateRuleConditions(conditions []RuleCondition, alertType AlertType, alertContext *AlertContext, baseData map[string]interface{}) bool {
	for _, condition := range conditions {
		if !cam.evaluateCondition(condition, alertType, alertContext, baseData) {
			return false
		}
	}
	return true
}

// evaluateCondition evaluates a single condition
func (cam *ContextualAlertManager) evaluateCondition(condition RuleCondition, alertType AlertType, alertContext *AlertContext, baseData map[string]interface{}) bool {
	var fieldValue interface{}

	switch condition.Type {
	case "alert_type":
		fieldValue = string(alertType)
	case "location":
		if alertContext.Location != nil {
			switch condition.Field {
			case "latitude":
				fieldValue = alertContext.Location.Latitude
			case "longitude":
				fieldValue = alertContext.Location.Longitude
			case "accuracy":
				fieldValue = alertContext.Location.Accuracy
			case "is_stationary":
				if alertContext.Location.MovementInfo != nil {
					fieldValue = alertContext.Location.MovementInfo.IsStationary
				}
			}
		}
	case "system":
		if alertContext.SystemLoad != nil {
			switch condition.Field {
			case "cpu_usage":
				fieldValue = alertContext.SystemLoad.CPUUsage
			case "memory_usage":
				fieldValue = alertContext.SystemLoad.MemoryUsage
			case "temperature":
				fieldValue = alertContext.SystemLoad.Temperature
			}
		}
	case "metrics":
		if alertContext.Metrics != nil {
			switch condition.Field {
			case "latency":
				if alertContext.Metrics.LatencyMS != nil {
					fieldValue = *alertContext.Metrics.LatencyMS
				}
			case "loss_percent":
				if alertContext.Metrics.LossPercent != nil {
					fieldValue = *alertContext.Metrics.LossPercent
				}
			}
		}
	case "time":
		now := time.Now()
		switch condition.Field {
		case "hour":
			fieldValue = now.Hour()
		case "weekday":
			fieldValue = now.Weekday().String()
		}
	default:
		// Check in baseData
		fieldValue = baseData[condition.Field]
	}

	return cam.compareValues(fieldValue, condition.Operator, condition.Value)
}

// compareValues compares two values using the specified operator
func (cam *ContextualAlertManager) compareValues(fieldValue interface{}, operator string, conditionValue interface{}) bool {
	switch operator {
	case "equals":
		return fmt.Sprintf("%v", fieldValue) == fmt.Sprintf("%v", conditionValue)
	case "not_equals":
		return fmt.Sprintf("%v", fieldValue) != fmt.Sprintf("%v", conditionValue)
	case "contains":
		fieldStr := strings.ToLower(fmt.Sprintf("%v", fieldValue))
		condStr := strings.ToLower(fmt.Sprintf("%v", conditionValue))
		return strings.Contains(fieldStr, condStr)
	case "gt":
		return cam.compareNumeric(fieldValue, conditionValue, ">")
	case "lt":
		return cam.compareNumeric(fieldValue, conditionValue, "<")
	case "gte":
		return cam.compareNumeric(fieldValue, conditionValue, ">=")
	case "lte":
		return cam.compareNumeric(fieldValue, conditionValue, "<=")
	}
	return false
}

// compareNumeric compares numeric values
func (cam *ContextualAlertManager) compareNumeric(fieldValue, conditionValue interface{}, operator string) bool {
	fv, fok := cam.toFloat64(fieldValue)
	cv, cok := cam.toFloat64(conditionValue)

	if !fok || !cok {
		return false
	}

	switch operator {
	case ">":
		return fv > cv
	case "<":
		return fv < cv
	case ">=":
		return fv >= cv
	case "<=":
		return fv <= cv
	}
	return false
}

// toFloat64 converts various numeric types to float64
func (cam *ContextualAlertManager) toFloat64(value interface{}) (float64, bool) {
	switch v := value.(type) {
	case float64:
		return v, true
	case float32:
		return float64(v), true
	case int:
		return float64(v), true
	case int64:
		return float64(v), true
	case int32:
		return float64(v), true
	}
	return 0, false
}

// executeContextActions executes actions from context rules
func (cam *ContextualAlertManager) executeContextActions(actions []ContextAction, alertContext *AlertContext, baseData map[string]interface{}) {
	for _, action := range actions {
		switch action.Type {
		case "add_context":
			if key, ok := action.Parameters["key"].(string); ok {
				if value, ok := action.Parameters["value"]; ok {
					alertContext.Custom[key] = value
				}
			}
		case "modify_priority":
			if priority, ok := action.Parameters["priority"].(int); ok {
				baseData["priority"] = priority
			}
		}
	}
}

// applyEnrichments applies template enrichers to add contextual information
func (cam *ContextualAlertManager) applyEnrichments(template *AlertTemplate, alertContext *AlertContext, baseData map[string]interface{}) []ContextEnrichment {
	var enrichments []ContextEnrichment

	for _, enricher := range template.Enrichers {
		// Check if enricher conditions are met
		shouldApply := true
		for _, condition := range enricher.Conditions {
			// Simplified condition evaluation for enrichers
			if !cam.evaluateEnricherCondition(condition, alertContext, baseData) {
				shouldApply = false
				break
			}
		}

		if shouldApply {
			content := cam.renderTemplate(enricher.Template, alertContext, baseData)
			enrichment := ContextEnrichment{
				EnricherName: enricher.Name,
				Type:         enricher.Type,
				Content:      content,
				AppliedAt:    time.Now(),
			}
			enrichments = append(enrichments, enrichment)
		}
	}

	return enrichments
}

// evaluateEnricherCondition evaluates conditions for enrichers
func (cam *ContextualAlertManager) evaluateEnricherCondition(condition EnricherCondition, alertContext *AlertContext, baseData map[string]interface{}) bool {
	// Simplified evaluation - can be expanded based on needs
	if value, exists := baseData[condition.Field]; exists {
		return cam.compareValues(value, condition.Operator, condition.Value)
	}
	return false
}

// generateSuggestedActions creates actionable recommendations based on context
func (cam *ContextualAlertManager) generateSuggestedActions(template *AlertTemplate, alertContext *AlertContext, baseData map[string]interface{}) []SuggestedAction {
	actions := make([]SuggestedAction, 0)

	// Add template actions
	actions = append(actions, template.Actions...)

	// Add context-specific actions based on alert type and context
	contextActions := cam.generateContextSpecificActions(template.Type, alertContext, baseData)
	actions = append(actions, contextActions...)

	return actions
}

// generateContextSpecificActions generates actions based on specific context
func (cam *ContextualAlertManager) generateContextSpecificActions(alertType AlertType, alertContext *AlertContext, baseData map[string]interface{}) []SuggestedAction {
	var actions []SuggestedAction

	switch alertType {
	case AlertFailover:
		if alertContext.NetworkTopo != nil {
			actions = append(actions, SuggestedAction{
				Title:       "Check backup interfaces",
				Description: fmt.Sprintf("Verify %d backup interfaces are functioning", len(alertContext.NetworkTopo.BackupInterfaces)),
				Command:     "ubus call autonomy members",
				Priority:    1,
			})
		}

	case AlertObstruction:
		if alertContext.Location != nil && alertContext.Location.MovementInfo != nil {
			if alertContext.Location.MovementInfo.IsStationary {
				actions = append(actions, SuggestedAction{
					Title:       "Clear obstruction map",
					Description: "Device is stationary - clear Starlink obstruction map to refresh",
					Command:     "ubus call starlink clear_obstruction_map",
					Priority:    2,
				})
			}
		}

	case AlertThermal:
		if alertContext.SystemLoad != nil && alertContext.SystemLoad.Temperature > 70 {
			actions = append(actions, SuggestedAction{
				Title:       "Check cooling system",
				Description: fmt.Sprintf("System temperature is %.1f°C - verify cooling", alertContext.SystemLoad.Temperature),
				Priority:    1,
			})
		}

	case AlertDataLimit:
		if alertContext.NetworkTopo != nil && alertContext.NetworkTopo.DataUsage != nil {
			if alertContext.NetworkTopo.DataUsage.UsagePercent > 90 {
				actions = append(actions, SuggestedAction{
					Title:       "Enable data saving mode",
					Description: "Data usage is critical - consider enabling data saving features",
					Command:     "uci set autonomy.general.data_saving=1 && uci commit",
					Priority:    1,
				})
			}
		}
	}

	return actions
}

// createBaseNotification creates the base notification with enriched content
func (cam *ContextualAlertManager) createBaseNotification(template *AlertTemplate, alertContext *AlertContext, baseData map[string]interface{}, enrichments []ContextEnrichment) *Notification {
	// Render title and message templates
	title := cam.renderTemplate(template.TitleTemplate, alertContext, baseData)
	message := cam.renderTemplate(template.MessageTemplate, alertContext, baseData)

	// Add enrichment content to message
	if len(enrichments) > 0 {
		message += "\n\n📋 Additional Context:"
		for _, enrichment := range enrichments {
			message += fmt.Sprintf("\n• %s: %s", enrichment.EnricherName, enrichment.Content)
		}
	}

	// Determine priority (can be overridden by context rules)
	priority := template.Priority
	if overridePriority, ok := baseData["priority"].(int); ok {
		priority = overridePriority
	}

	// Create context map for notification
	context := make(map[string]interface{})
	context["alert_type"] = string(template.Type)
	context["processing_time"] = time.Now()

	// Add key context information
	if alertContext.Location != nil {
		context["location"] = map[string]interface{}{
			"latitude":  alertContext.Location.Latitude,
			"longitude": alertContext.Location.Longitude,
			"accuracy":  alertContext.Location.Accuracy,
		}
	}

	if alertContext.SystemLoad != nil {
		context["system_load"] = map[string]interface{}{
			"cpu_usage":    alertContext.SystemLoad.CPUUsage,
			"memory_usage": alertContext.SystemLoad.MemoryUsage,
			"temperature":  alertContext.SystemLoad.Temperature,
		}
	}

	return &Notification{
		Type:      NotificationType(template.Type),
		Title:     title,
		Message:   message,
		Priority:  priority,
		Timestamp: time.Now(),
		Context:   context,
	}
}

// renderTemplate renders a template with context data
func (cam *ContextualAlertManager) renderTemplate(template string, alertContext *AlertContext, baseData map[string]interface{}) string {
	result := template

	// Simple template variable replacement
	// In a production system, you might want to use a proper template engine

	// Replace base data variables
	for key, value := range baseData {
		placeholder := fmt.Sprintf("{{.%s}}", key)
		result = strings.ReplaceAll(result, placeholder, fmt.Sprintf("%v", value))
	}

	// Replace context variables
	if alertContext.Location != nil {
		result = strings.ReplaceAll(result, "{{.location.latitude}}", fmt.Sprintf("%.6f", alertContext.Location.Latitude))
		result = strings.ReplaceAll(result, "{{.location.longitude}}", fmt.Sprintf("%.6f", alertContext.Location.Longitude))
		result = strings.ReplaceAll(result, "{{.location.accuracy}}", fmt.Sprintf("%.1f", alertContext.Location.Accuracy))
	}

	if alertContext.SystemLoad != nil {
		result = strings.ReplaceAll(result, "{{.system.cpu}}", fmt.Sprintf("%.1f", alertContext.SystemLoad.CPUUsage))
		result = strings.ReplaceAll(result, "{{.system.memory}}", fmt.Sprintf("%.1f", alertContext.SystemLoad.MemoryUsage))
		result = strings.ReplaceAll(result, "{{.system.temperature}}", fmt.Sprintf("%.1f", alertContext.SystemLoad.Temperature))
	}

	if alertContext.Metrics != nil {
		if alertContext.Metrics.LatencyMS != nil {
			result = strings.ReplaceAll(result, "{{.metrics.latency}}", fmt.Sprintf("%.1f", *alertContext.Metrics.LatencyMS))
		}
		if alertContext.Metrics.LossPercent != nil {
			result = strings.ReplaceAll(result, "{{.metrics.loss}}", fmt.Sprintf("%.2f", *alertContext.Metrics.LossPercent))
		}
	}

	// Add timestamp
	result = strings.ReplaceAll(result, "{{.timestamp}}", time.Now().Format("2006-01-02 15:04:05 UTC"))

	return result
}

// addToHistory adds a contextual alert to the history
func (cam *ContextualAlertManager) addToHistory(alert *ContextualAlert) {
	cam.alertHistory = append(cam.alertHistory, *alert)

	// Keep only last 1000 alerts
	if len(cam.alertHistory) > 1000 {
		cam.alertHistory = cam.alertHistory[len(cam.alertHistory)-1000:]
	}
}

// GetAlertHistory returns recent contextual alerts
func (cam *ContextualAlertManager) GetAlertHistory(limit int) []ContextualAlert {
	if limit <= 0 || limit > len(cam.alertHistory) {
		limit = len(cam.alertHistory)
	}

	start := len(cam.alertHistory) - limit
	if start < 0 {
		start = 0
	}

	return cam.alertHistory[start:]
}
