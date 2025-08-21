package notifications

import (
	"context"
	"fmt"
	"math"
	"sync"
	"time"

	"github.com/markus-lassfolk/autonomy/pkg/logx"
)

// NotificationIntelligenceEngine provides advanced notification intelligence and emergency handling
type NotificationIntelligenceEngine struct {
	logger           *logx.Logger
	smartManager     *SmartNotificationManager
	contextualAlerts *ContextualAlertManager

	// Intelligence configuration
	config *IntelligenceConfig

	// Emergency handling
	emergencyDetector *EmergencyDetector
	escalationManager *EscalationManager
	priorityOptimizer *PriorityOptimizer

	// Intelligent routing
	channelIntelligence *ChannelIntelligence
	deliveryOptimizer   *DeliveryOptimizer

	// State tracking
	mu                  sync.RWMutex
	systemState         *SystemState
	notificationMetrics *IntelligenceMetrics
	learningData        *LearningData
}

// IntelligenceConfig holds configuration for the intelligence engine
type IntelligenceConfig struct {
	// Emergency detection
	EmergencyDetectionEnabled bool                 `json:"emergency_detection_enabled"`
	EmergencyThresholds       *EmergencyThresholds `json:"emergency_thresholds"`

	// Priority optimization
	PriorityOptimizationEnabled bool    `json:"priority_optimization_enabled"`
	LearningEnabled             bool    `json:"learning_enabled"`
	AdaptationRate              float64 `json:"adaptation_rate"`

	// Channel intelligence
	ChannelIntelligenceEnabled  bool `json:"channel_intelligence_enabled"`
	DeliveryOptimizationEnabled bool `json:"delivery_optimization_enabled"`

	// Escalation settings
	EscalationEnabled  bool          `json:"escalation_enabled"`
	MaxEscalationLevel int           `json:"max_escalation_level"`
	EscalationCooldown time.Duration `json:"escalation_cooldown"`

	// Learning parameters
	LearningWindow        time.Duration `json:"learning_window"`
	MinSamplesForLearning int           `json:"min_samples_for_learning"`
	ConfidenceThreshold   float64       `json:"confidence_threshold"`
}

// EmergencyThresholds defines thresholds for emergency detection
type EmergencyThresholds struct {
	// System metrics thresholds
	CPUUsageEmergency    float64 `json:"cpu_usage_emergency"`    // 90%
	MemoryUsageEmergency float64 `json:"memory_usage_emergency"` // 95%
	TemperatureEmergency float64 `json:"temperature_emergency"`  // 85°C
	DiskUsageEmergency   float64 `json:"disk_usage_emergency"`   // 95%

	// Network metrics thresholds
	PacketLossEmergency float64 `json:"packet_loss_emergency"` // 50%
	LatencyEmergency    float64 `json:"latency_emergency"`     // 5000ms

	// Failure pattern thresholds
	FailureRateEmergency  float64 `json:"failure_rate_emergency"`  // 5 failures/minute
	CascadingFailureCount int     `json:"cascading_failure_count"` // 3 simultaneous failures

	// Time-based thresholds
	ServiceDowntimeEmergency time.Duration `json:"service_downtime_emergency"` // 5 minutes
	RecoveryTimeEmergency    time.Duration `json:"recovery_time_emergency"`    // 15 minutes
}

// SystemState tracks current system conditions for intelligent decisions
type SystemState struct {
	Timestamp       time.Time           `json:"timestamp"`
	EmergencyLevel  EmergencyLevel      `json:"emergency_level"`
	SystemHealth    *SystemHealthState  `json:"system_health"`
	NetworkHealth   *NetworkHealthState `json:"network_health"`
	ActiveIncidents []ActiveIncident    `json:"active_incidents"`
	RecentFailures  []FailureRecord     `json:"recent_failures"`
	UserPresence    *UserPresenceState  `json:"user_presence"`
	MaintenanceMode bool                `json:"maintenance_mode"`
	BusinessHours   bool                `json:"business_hours"`
}

// EmergencyLevel defines different levels of emergency
type EmergencyLevel int

const (
	EmergencyNone EmergencyLevel = iota
	EmergencyLow
	EmergencyMedium
	EmergencyHigh
	EmergencyCritical
)

// SystemHealthState tracks system health metrics
type SystemHealthState struct {
	CPUUsage        float64   `json:"cpu_usage"`
	MemoryUsage     float64   `json:"memory_usage"`
	DiskUsage       float64   `json:"disk_usage"`
	Temperature     float64   `json:"temperature"`
	LoadAverage     float64   `json:"load_average"`
	UptimeSeconds   int64     `json:"uptime_seconds"`
	LastHealthCheck time.Time `json:"last_health_check"`
}

// NetworkHealthState tracks network health metrics
type NetworkHealthState struct {
	PrimaryInterfaceUp    bool      `json:"primary_interface_up"`
	BackupInterfacesUp    int       `json:"backup_interfaces_up"`
	TotalInterfaces       int       `json:"total_interfaces"`
	AverageLatency        float64   `json:"average_latency"`
	AveragePacketLoss     float64   `json:"average_packet_loss"`
	ThroughputMbps        float64   `json:"throughput_mbps"`
	LastConnectivityCheck time.Time `json:"last_connectivity_check"`
}

// ActiveIncident represents an ongoing system incident
type ActiveIncident struct {
	ID              string                 `json:"id"`
	Type            string                 `json:"type"`
	Severity        int                    `json:"severity"`
	StartTime       time.Time              `json:"start_time"`
	Description     string                 `json:"description"`
	AffectedSystems []string               `json:"affected_systems"`
	Escalated       bool                   `json:"escalated"`
	Context         map[string]interface{} `json:"context"`
}

// FailureRecord tracks recent system failures
type FailureRecord struct {
	Timestamp   time.Time              `json:"timestamp"`
	Component   string                 `json:"component"`
	FailureType string                 `json:"failure_type"`
	Severity    int                    `json:"severity"`
	Duration    time.Duration          `json:"duration"`
	Resolved    bool                   `json:"resolved"`
	Context     map[string]interface{} `json:"context"`
}

// UserPresenceState tracks user availability for notifications
type UserPresenceState struct {
	IsActive          bool                  `json:"is_active"`
	LastActivity      time.Time             `json:"last_activity"`
	PreferredChannels []NotificationChannel `json:"preferred_channels"`
	TimeZone          string                `json:"time_zone"`
	QuietHoursActive  bool                  `json:"quiet_hours_active"`
	ResponseHistory   *ResponseHistory      `json:"response_history"`
}

// ResponseHistory tracks user response patterns
type ResponseHistory struct {
	AverageResponseTime map[NotificationChannel]time.Duration `json:"average_response_time"`
	ResponseRate        map[NotificationChannel]float64       `json:"response_rate"`
	PreferredTimes      []TimeWindow                          `json:"preferred_times"`
	LastResponse        time.Time                             `json:"last_response"`
}

// TimeWindow represents a time window for user preferences
type TimeWindow struct {
	Start    string `json:"start"`    // "HH:MM"
	End      string `json:"end"`      // "HH:MM"
	Days     []int  `json:"days"`     // 0=Sunday, 1=Monday, etc.
	Priority int    `json:"priority"` // Higher = more preferred
}

// IntelligenceMetrics tracks intelligence engine performance
type IntelligenceMetrics struct {
	mu sync.RWMutex

	// Emergency detection metrics
	EmergenciesDetected   int64         `json:"emergencies_detected"`
	FalsePositives        int64         `json:"false_positives"`
	EmergencyResponseTime time.Duration `json:"emergency_response_time"`

	// Escalation metrics
	EscalationsTriggered  int64   `json:"escalations_triggered"`
	EscalationSuccessRate float64 `json:"escalation_success_rate"`

	// Priority optimization metrics
	PriorityAdjustments  int64   `json:"priority_adjustments"`
	OptimizationAccuracy float64 `json:"optimization_accuracy"`

	// Channel intelligence metrics
	ChannelEffectiveness  map[NotificationChannel]float64 `json:"channel_effectiveness"`
	DeliveryOptimizations int64                           `json:"delivery_optimizations"`

	// Learning metrics
	LearningIterations   int64   `json:"learning_iterations"`
	ModelAccuracy        float64 `json:"model_accuracy"`
	PredictionConfidence float64 `json:"prediction_confidence"`

	LastUpdated time.Time `json:"last_updated"`
}

// LearningData stores machine learning data for intelligent decisions
type LearningData struct {
	mu sync.RWMutex

	// Historical patterns
	NotificationPatterns   []NotificationPattern   `json:"notification_patterns"`
	UserBehaviorPatterns   []UserBehaviorPattern   `json:"user_behavior_patterns"`
	SystemBehaviorPatterns []SystemBehaviorPattern `json:"system_behavior_patterns"`

	// Learned preferences
	OptimalChannels   map[AlertType][]NotificationChannel `json:"optimal_channels"`
	OptimalTiming     map[AlertType][]TimeWindow          `json:"optimal_timing"`
	OptimalPriorities map[string]int                      `json:"optimal_priorities"`

	// Model parameters
	WeightMatrix     map[string]float64 `json:"weight_matrix"`
	ConfidenceScores map[string]float64 `json:"confidence_scores"`
	LastTraining     time.Time          `json:"last_training"`
}

// NotificationPattern represents learned notification patterns
type NotificationPattern struct {
	AlertType          AlertType              `json:"alert_type"`
	Context            map[string]interface{} `json:"context"`
	OptimalPriority    int                    `json:"optimal_priority"`
	OptimalChannels    []NotificationChannel  `json:"optimal_channels"`
	UserResponseTime   time.Duration          `json:"user_response_time"`
	EffectivenessScore float64                `json:"effectiveness_score"`
	Frequency          int                    `json:"frequency"`
	LastSeen           time.Time              `json:"last_seen"`
}

// UserBehaviorPattern represents learned user behavior
type UserBehaviorPattern struct {
	TimeOfDay           int                   `json:"time_of_day"`
	DayOfWeek           int                   `json:"day_of_week"`
	PreferredChannels   []NotificationChannel `json:"preferred_channels"`
	AverageResponseTime time.Duration         `json:"average_response_time"`
	ActivityLevel       float64               `json:"activity_level"`
	Confidence          float64               `json:"confidence"`
}

// SystemBehaviorPattern represents learned system behavior
type SystemBehaviorPattern struct {
	Conditions         map[string]interface{} `json:"conditions"`
	FailureProbability float64                `json:"failure_probability"`
	EscalationNeeded   bool                   `json:"escalation_needed"`
	OptimalResponse    string                 `json:"optimal_response"`
	Confidence         float64                `json:"confidence"`
}

// NewNotificationIntelligenceEngine creates a new intelligence engine
func NewNotificationIntelligenceEngine(
	smartManager *SmartNotificationManager,
	contextualAlerts *ContextualAlertManager,
	config *IntelligenceConfig,
	logger *logx.Logger,
) *NotificationIntelligenceEngine {
	if config == nil {
		config = DefaultIntelligenceConfig()
	}

	nie := &NotificationIntelligenceEngine{
		logger:              logger,
		smartManager:        smartManager,
		contextualAlerts:    contextualAlerts,
		config:              config,
		systemState:         &SystemState{},
		notificationMetrics: NewIntelligenceMetrics(),
		learningData:        NewLearningData(),
	}

	// Initialize components
	nie.emergencyDetector = NewEmergencyDetector(config, logger)
	nie.escalationManager = NewEscalationManager(config, logger)
	nie.priorityOptimizer = NewPriorityOptimizer(config, logger)
	nie.channelIntelligence = NewChannelIntelligence(config, logger)
	nie.deliveryOptimizer = NewDeliveryOptimizer(config, logger)

	// Start background intelligence tasks
	go nie.intelligenceLoop()

	return nie
}

// ProcessIntelligentNotification processes a notification with full intelligence
func (nie *NotificationIntelligenceEngine) ProcessIntelligentNotification(
	ctx context.Context,
	alertType AlertType,
	baseData map[string]interface{},
) error {
	startTime := time.Now()

	// Update system state
	nie.updateSystemState()

	// Detect emergency conditions
	emergencyLevel := nie.emergencyDetector.DetectEmergency(nie.systemState, alertType, baseData)

	// Apply emergency handling if needed
	if emergencyLevel > EmergencyNone {
		return nie.handleEmergencyNotification(ctx, alertType, baseData, emergencyLevel)
	}

	// Optimize priority based on intelligence
	if nie.config.PriorityOptimizationEnabled {
		optimizedPriority := nie.priorityOptimizer.OptimizePriority(alertType, baseData, nie.systemState, nie.learningData)
		baseData["priority"] = optimizedPriority
	}

	// Select optimal channels based on intelligence
	if nie.config.ChannelIntelligenceEnabled {
		optimalChannels := nie.channelIntelligence.SelectOptimalChannels(alertType, baseData, nie.systemState, nie.learningData)
		baseData["preferred_channels"] = optimalChannels
	}

	// Optimize delivery timing
	if nie.config.DeliveryOptimizationEnabled {
		deliveryPlan := nie.deliveryOptimizer.OptimizeDelivery(alertType, baseData, nie.systemState, nie.learningData)
		if deliveryPlan.DelayDelivery {
			// Schedule for later delivery
			go nie.scheduleDelayedDelivery(ctx, alertType, baseData, deliveryPlan.OptimalTime)
			return nil
		}
	}

	// Send through contextual alerts system
	err := nie.contextualAlerts.SendContextualAlert(ctx, alertType, baseData)

	// Learn from the notification
	if nie.config.LearningEnabled {
		nie.learnFromNotification(alertType, baseData, err, time.Since(startTime))
	}

	// Update metrics
	nie.updateIntelligenceMetrics(alertType, err, time.Since(startTime))

	return err
}

// handleEmergencyNotification handles emergency-level notifications with special processing
func (nie *NotificationIntelligenceEngine) handleEmergencyNotification(
	ctx context.Context,
	alertType AlertType,
	baseData map[string]interface{},
	emergencyLevel EmergencyLevel,
) error {
	nie.logger.Warn("Emergency notification detected",
		"alert_type", alertType,
		"emergency_level", emergencyLevel)

	// Force emergency priority
	baseData["priority"] = PriorityEmergency

	// Add emergency context
	baseData["emergency_level"] = emergencyLevel
	baseData["emergency_detected_at"] = time.Now()

	// Use all available channels for emergency
	baseData["force_all_channels"] = true

	// Bypass rate limiting for emergencies
	baseData["bypass_rate_limiting"] = true

	// Create emergency incident record
	incident := &ActiveIncident{
		ID:              fmt.Sprintf("emergency_%d", time.Now().UnixNano()),
		Type:            string(alertType),
		Severity:        int(emergencyLevel),
		StartTime:       time.Now(),
		Description:     fmt.Sprintf("Emergency %s detected", alertType),
		AffectedSystems: []string{"notification_system"},
		Escalated:       false,
		Context:         baseData,
	}

	nie.mu.Lock()
	nie.systemState.ActiveIncidents = append(nie.systemState.ActiveIncidents, *incident)
	nie.mu.Unlock()

	// Send emergency notification
	err := nie.contextualAlerts.SendContextualAlert(ctx, alertType, baseData)

	// Trigger escalation if configured
	if nie.config.EscalationEnabled && err == nil {
		go nie.escalationManager.TriggerEmergencyEscalation(incident)
	}

	// Update emergency metrics
	nie.notificationMetrics.IncrementEmergenciesDetected()

	return err
}

// updateSystemState updates the current system state for intelligent decisions
func (nie *NotificationIntelligenceEngine) updateSystemState() {
	nie.mu.Lock()
	defer nie.mu.Unlock()

	nie.systemState.Timestamp = time.Now()

	// Update system health (would integrate with actual system monitoring)
	nie.systemState.SystemHealth = &SystemHealthState{
		CPUUsage:        nie.getCurrentCPUUsage(),
		MemoryUsage:     nie.getCurrentMemoryUsage(),
		Temperature:     nie.getCurrentTemperature(),
		LastHealthCheck: time.Now(),
	}

	// Update network health (would integrate with actual network monitoring)
	nie.systemState.NetworkHealth = &NetworkHealthState{
		PrimaryInterfaceUp:    true, // Would check actual status
		BackupInterfacesUp:    2,    // Would check actual count
		TotalInterfaces:       3,
		LastConnectivityCheck: time.Now(),
	}

	// Update business hours
	now := time.Now()
	hour := now.Hour()
	weekday := now.Weekday()
	nie.systemState.BusinessHours = hour >= 9 && hour < 17 && weekday >= time.Monday && weekday <= time.Friday

	// Clean up old incidents and failures
	nie.cleanupOldRecords()
}

// getCurrentCPUUsage gets current CPU usage (mock implementation)
func (nie *NotificationIntelligenceEngine) getCurrentCPUUsage() float64 {
	// In a real implementation, this would read from /proc/stat or similar
	return 25.0 // Mock value
}

// getCurrentMemoryUsage gets current memory usage (mock implementation)
func (nie *NotificationIntelligenceEngine) getCurrentMemoryUsage() float64 {
	// In a real implementation, this would read from /proc/meminfo or similar
	return 45.0 // Mock value
}

// getCurrentTemperature gets current system temperature (mock implementation)
func (nie *NotificationIntelligenceEngine) getCurrentTemperature() float64 {
	// In a real implementation, this would read from thermal sensors
	return 45.0 // Mock value
}

// cleanupOldRecords removes old incidents and failure records
func (nie *NotificationIntelligenceEngine) cleanupOldRecords() {
	cutoff := time.Now().Add(-24 * time.Hour)

	// Clean old incidents
	activeIncidents := make([]ActiveIncident, 0)
	for _, incident := range nie.systemState.ActiveIncidents {
		if incident.StartTime.After(cutoff) {
			activeIncidents = append(activeIncidents, incident)
		}
	}
	nie.systemState.ActiveIncidents = activeIncidents

	// Clean old failures
	recentFailures := make([]FailureRecord, 0)
	for _, failure := range nie.systemState.RecentFailures {
		if failure.Timestamp.After(cutoff) {
			recentFailures = append(recentFailures, failure)
		}
	}
	nie.systemState.RecentFailures = recentFailures
}

// scheduleDelayedDelivery schedules a notification for later delivery
func (nie *NotificationIntelligenceEngine) scheduleDelayedDelivery(
	ctx context.Context,
	alertType AlertType,
	baseData map[string]interface{},
	deliveryTime time.Time,
) {
	delay := time.Until(deliveryTime)
	if delay > 0 {
		nie.logger.Info("Scheduling delayed notification delivery",
			"alert_type", alertType,
			"delay", delay,
			"delivery_time", deliveryTime)

		time.Sleep(delay)
	}

	// Send the notification at the optimal time
	err := nie.contextualAlerts.SendContextualAlert(ctx, alertType, baseData)
	if err != nil {
		nie.logger.Error("Failed to send delayed notification", "error", err)
	}
}

// learnFromNotification updates learning data based on notification results
func (nie *NotificationIntelligenceEngine) learnFromNotification(
	alertType AlertType,
	baseData map[string]interface{},
	err error,
	processingTime time.Duration,
) {
	nie.learningData.mu.Lock()
	defer nie.learningData.mu.Unlock()

	// Create or update notification pattern
	pattern := &NotificationPattern{
		AlertType:          alertType,
		Context:            baseData,
		EffectivenessScore: nie.calculateEffectivenessScore(err, processingTime),
		LastSeen:           time.Now(),
	}

	// Add to learning data
	nie.learningData.NotificationPatterns = append(nie.learningData.NotificationPatterns, *pattern)

	// Keep only recent patterns (last 1000)
	if len(nie.learningData.NotificationPatterns) > 1000 {
		nie.learningData.NotificationPatterns = nie.learningData.NotificationPatterns[len(nie.learningData.NotificationPatterns)-1000:]
	}

	// Update model weights if enough data
	if len(nie.learningData.NotificationPatterns) >= nie.config.MinSamplesForLearning {
		nie.updateModelWeights()
	}
}

// calculateEffectivenessScore calculates how effective a notification was
func (nie *NotificationIntelligenceEngine) calculateEffectivenessScore(err error, processingTime time.Duration) float64 {
	score := 1.0

	// Reduce score for errors
	if err != nil {
		score *= 0.1
	}

	// Reduce score for slow processing
	if processingTime > 5*time.Second {
		score *= 0.8
	} else if processingTime > 1*time.Second {
		score *= 0.9
	}

	return score
}

// updateModelWeights updates machine learning model weights
func (nie *NotificationIntelligenceEngine) updateModelWeights() {
	// Simple learning algorithm - in production, this could be more sophisticated
	patterns := nie.learningData.NotificationPatterns

	// Calculate average effectiveness by alert type
	typeEffectiveness := make(map[AlertType][]float64)
	for _, pattern := range patterns {
		typeEffectiveness[pattern.AlertType] = append(typeEffectiveness[pattern.AlertType], pattern.EffectivenessScore)
	}

	// Update weights based on effectiveness
	for alertType, scores := range typeEffectiveness {
		if len(scores) > 0 {
			avgScore := nie.calculateAverage(scores)
			nie.learningData.WeightMatrix[string(alertType)] = avgScore
			nie.learningData.ConfidenceScores[string(alertType)] = nie.calculateConfidence(scores)
		}
	}

	nie.learningData.LastTraining = time.Now()
}

// calculateAverage calculates the average of a slice of float64 values
func (nie *NotificationIntelligenceEngine) calculateAverage(values []float64) float64 {
	if len(values) == 0 {
		return 0.0
	}

	sum := 0.0
	for _, value := range values {
		sum += value
	}

	return sum / float64(len(values))
}

// calculateConfidence calculates confidence score based on variance
func (nie *NotificationIntelligenceEngine) calculateConfidence(values []float64) float64 {
	if len(values) < 2 {
		return 0.5 // Low confidence with insufficient data
	}

	avg := nie.calculateAverage(values)
	variance := 0.0

	for _, value := range values {
		variance += math.Pow(value-avg, 2)
	}
	variance /= float64(len(values) - 1)

	// Convert variance to confidence (lower variance = higher confidence)
	confidence := 1.0 / (1.0 + variance)
	return math.Min(confidence, 1.0)
}

// updateIntelligenceMetrics updates intelligence engine metrics
func (nie *NotificationIntelligenceEngine) updateIntelligenceMetrics(alertType AlertType, err error, processingTime time.Duration) {
	nie.notificationMetrics.mu.Lock()
	defer nie.notificationMetrics.mu.Unlock()

	if err == nil {
		// Update success metrics
	} else {
		// Update failure metrics
	}

	nie.notificationMetrics.LastUpdated = time.Now()
}

// intelligenceLoop runs background intelligence tasks
func (nie *NotificationIntelligenceEngine) intelligenceLoop() {
	ticker := time.NewTicker(1 * time.Minute)
	defer ticker.Stop()

	for {
		select {
		case <-ticker.C:
			nie.performIntelligenceTasks()
		}
	}
}

// performIntelligenceTasks performs periodic intelligence tasks
func (nie *NotificationIntelligenceEngine) performIntelligenceTasks() {
	// Update system state
	nie.updateSystemState()

	// Perform learning if enabled
	if nie.config.LearningEnabled {
		nie.performLearningTasks()
	}

	// Update channel effectiveness
	if nie.config.ChannelIntelligenceEnabled {
		nie.updateChannelEffectiveness()
	}

	// Check for pattern anomalies
	nie.checkForAnomalies()
}

// performLearningTasks performs machine learning tasks
func (nie *NotificationIntelligenceEngine) performLearningTasks() {
	nie.learningData.mu.Lock()
	defer nie.learningData.mu.Unlock()

	// Retrain model if enough new data
	if time.Since(nie.learningData.LastTraining) > nie.config.LearningWindow {
		nie.updateModelWeights()
		nie.notificationMetrics.IncrementLearningIterations()
	}
}

// updateChannelEffectiveness updates channel effectiveness metrics
func (nie *NotificationIntelligenceEngine) updateChannelEffectiveness() {
	// This would integrate with actual delivery statistics
	// For now, use mock effectiveness scores
	nie.notificationMetrics.mu.Lock()
	defer nie.notificationMetrics.mu.Unlock()

	if nie.notificationMetrics.ChannelEffectiveness == nil {
		nie.notificationMetrics.ChannelEffectiveness = make(map[NotificationChannel]float64)
	}

	// Mock effectiveness scores (would be calculated from actual delivery data)
	nie.notificationMetrics.ChannelEffectiveness[ChannelPushover] = 0.95
	nie.notificationMetrics.ChannelEffectiveness[ChannelEmail] = 0.85
	nie.notificationMetrics.ChannelEffectiveness[ChannelSlack] = 0.90
	nie.notificationMetrics.ChannelEffectiveness[ChannelDiscord] = 0.88
	nie.notificationMetrics.ChannelEffectiveness[ChannelTelegram] = 0.92
	nie.notificationMetrics.ChannelEffectiveness[ChannelWebhook] = 0.80
}

// checkForAnomalies checks for unusual patterns that might indicate issues
func (nie *NotificationIntelligenceEngine) checkForAnomalies() {
	// Check for unusual failure rates
	recentFailures := len(nie.systemState.RecentFailures)
	if recentFailures > 10 { // Threshold for anomaly
		nie.logger.Warn("Anomaly detected: High failure rate", "failures", recentFailures)
	}

	// Check for emergency escalation patterns
	activeEmergencies := 0
	for _, incident := range nie.systemState.ActiveIncidents {
		if incident.Severity >= int(EmergencyHigh) {
			activeEmergencies++
		}
	}

	if activeEmergencies > 3 {
		nie.logger.Warn("Anomaly detected: Multiple active emergencies", "count", activeEmergencies)
	}
}

// GetIntelligenceMetrics returns current intelligence metrics
func (nie *NotificationIntelligenceEngine) GetIntelligenceMetrics() *IntelligenceMetrics {
	return nie.notificationMetrics
}

// GetSystemState returns current system state
func (nie *NotificationIntelligenceEngine) GetSystemState() *SystemState {
	nie.mu.RLock()
	defer nie.mu.RUnlock()

	// Return a copy
	state := *nie.systemState
	return &state
}

// GetLearningData returns current learning data
func (nie *NotificationIntelligenceEngine) GetLearningData() *LearningData {
	return nie.learningData
}

// Helper functions for metrics

// IncrementEmergenciesDetected increments the emergencies detected counter
func (im *IntelligenceMetrics) IncrementEmergenciesDetected() {
	im.mu.Lock()
	defer im.mu.Unlock()
	im.EmergenciesDetected++
	im.LastUpdated = time.Now()
}

// IncrementLearningIterations increments the learning iterations counter
func (im *IntelligenceMetrics) IncrementLearningIterations() {
	im.mu.Lock()
	defer im.mu.Unlock()
	im.LearningIterations++
	im.LastUpdated = time.Now()
}

// NewIntelligenceMetrics creates new intelligence metrics
func NewIntelligenceMetrics() *IntelligenceMetrics {
	return &IntelligenceMetrics{
		ChannelEffectiveness: make(map[NotificationChannel]float64),
		LastUpdated:          time.Now(),
	}
}

// NewLearningData creates new learning data
func NewLearningData() *LearningData {
	return &LearningData{
		NotificationPatterns:   make([]NotificationPattern, 0),
		UserBehaviorPatterns:   make([]UserBehaviorPattern, 0),
		SystemBehaviorPatterns: make([]SystemBehaviorPattern, 0),
		OptimalChannels:        make(map[AlertType][]NotificationChannel),
		OptimalTiming:          make(map[AlertType][]TimeWindow),
		OptimalPriorities:      make(map[string]int),
		WeightMatrix:           make(map[string]float64),
		ConfidenceScores:       make(map[string]float64),
		LastTraining:           time.Now(),
	}
}

// DefaultIntelligenceConfig returns default intelligence configuration
func DefaultIntelligenceConfig() *IntelligenceConfig {
	return &IntelligenceConfig{
		EmergencyDetectionEnabled:   true,
		PriorityOptimizationEnabled: true,
		LearningEnabled:             true,
		AdaptationRate:              0.1,
		ChannelIntelligenceEnabled:  true,
		DeliveryOptimizationEnabled: true,
		EscalationEnabled:           true,
		MaxEscalationLevel:          3,
		EscalationCooldown:          15 * time.Minute,
		LearningWindow:              1 * time.Hour,
		MinSamplesForLearning:       10,
		ConfidenceThreshold:         0.7,
		EmergencyThresholds: &EmergencyThresholds{
			CPUUsageEmergency:        90.0,
			MemoryUsageEmergency:     95.0,
			TemperatureEmergency:     85.0,
			DiskUsageEmergency:       95.0,
			PacketLossEmergency:      50.0,
			LatencyEmergency:         5000.0,
			FailureRateEmergency:     5.0,
			CascadingFailureCount:    3,
			ServiceDowntimeEmergency: 5 * time.Minute,
			RecoveryTimeEmergency:    15 * time.Minute,
		},
	}
}
