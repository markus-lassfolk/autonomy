package analytics

import (
	"context"
	"sync"
	"time"

	"github.com/markus-lassfolk/autonomy/pkg/logx"
	"github.com/markus-lassfolk/autonomy/pkg/telem"
)

// Engine provides comprehensive analytics and dashboard functionality
type Engine struct {
	mu sync.RWMutex

	// Dependencies
	store  *telem.Store
	logger *logx.Logger

	// Analytics components
	performanceAnalyzer *PerformanceAnalyzer
	trendAnalyzer       *TrendAnalyzer
	predictiveAnalyzer  *PredictiveAnalyzer
	healthAnalyzer      *HealthAnalyzer
	usageAnalyzer       *UsageAnalyzer

	// Configuration
	config *Config

	// State
	lastUpdate time.Time
	metrics    *DashboardMetrics
}

// Config represents analytics configuration
type Config struct {
	Enabled          bool             `json:"enabled"`
	UpdateInterval   time.Duration    `json:"update_interval"`
	RetentionPeriod  time.Duration    `json:"retention_period"`
	MaxDataPoints    int              `json:"max_data_points"`
	TrendWindow      time.Duration    `json:"trend_window"`
	PredictionWindow time.Duration    `json:"prediction_window"`
	HealthThresholds HealthThresholds `json:"health_thresholds"`
}

// HealthThresholds defines health assessment thresholds
type HealthThresholds struct {
	Excellent float64 `json:"excellent"` // > 80
	Good      float64 `json:"good"`      // 60-80
	Fair      float64 `json:"fair"`      // 40-60
	Poor      float64 `json:"poor"`      // 20-40
	Critical  float64 `json:"critical"`  // < 20
}

// DashboardMetrics represents comprehensive dashboard metrics
type DashboardMetrics struct {
	Timestamp       time.Time                  `json:"timestamp"`
	Overview        *SystemOverview            `json:"overview"`
	Performance     *PerformanceMetrics        `json:"performance"`
	Trends          *TrendMetrics              `json:"trends"`
	Predictions     *PredictionMetrics         `json:"predictions"`
	Health          *HealthMetrics             `json:"health"`
	Usage           *UsageMetrics              `json:"usage"`
	Alerts          []*AnalyticsAlert          `json:"alerts"`
	Recommendations []*AnalyticsRecommendation `json:"recommendations"`
}

// SystemOverview provides high-level system status
type SystemOverview struct {
	TotalMembers  int        `json:"total_members"`
	ActiveMembers int        `json:"active_members"`
	OverallHealth float64    `json:"overall_health"`
	Uptime        string     `json:"uptime"`
	LastFailover  *time.Time `json:"last_failover,omitempty"`
	FailoverCount int        `json:"failover_count"`
	SuccessRate   float64    `json:"success_rate"`
}

// PerformanceMetrics provides performance analytics
type PerformanceMetrics struct {
	AverageLatency map[string]float64 `json:"average_latency"`
	AverageLoss    map[string]float64 `json:"average_loss"`
	AverageSignal  map[string]float64 `json:"average_signal"`
	Throughput     map[string]float64 `json:"throughput"`
	ResponseTime   map[string]float64 `json:"response_time"`
	ErrorRate      map[string]float64 `json:"error_rate"`
	Availability   map[string]float64 `json:"availability"`
}

// TrendMetrics provides trend analysis
type TrendMetrics struct {
	LatencyTrends     map[string]*Trend `json:"latency_trends"`
	SignalTrends      map[string]*Trend `json:"signal_trends"`
	UsageTrends       map[string]*Trend `json:"usage_trends"`
	HealthTrends      map[string]*Trend `json:"health_trends"`
	PerformanceTrends map[string]*Trend `json:"performance_trends"`
}

// Trend represents a trend analysis result
type Trend struct {
	Direction  string   `json:"direction"` // improving, stable, degrading
	Slope      float64  `json:"slope"`
	Confidence float64  `json:"confidence"`
	Magnitude  string   `json:"magnitude"` // small, medium, large
	Duration   string   `json:"duration"`  // short, medium, long
	Prediction *float64 `json:"prediction,omitempty"`
}

// PredictionMetrics provides predictive analytics
type PredictionMetrics struct {
	FailoverProbability map[string]float64           `json:"failover_probability"`
	MaintenanceWindows  []*MaintenanceWindow         `json:"maintenance_windows"`
	CapacityForecasts   map[string]*CapacityForecast `json:"capacity_forecasts"`
	RiskAssessments     map[string]*RiskAssessment   `json:"risk_assessments"`
}

// MaintenanceWindow represents a predicted maintenance window
type MaintenanceWindow struct {
	StartTime   time.Time `json:"start_time"`
	EndTime     time.Time `json:"end_time"`
	Type        string    `json:"type"`
	Probability float64   `json:"probability"`
	Description string    `json:"description"`
}

// CapacityForecast represents capacity prediction
type CapacityForecast struct {
	CurrentUsage   float64 `json:"current_usage"`
	PredictedUsage float64 `json:"predicted_usage"`
	TimeToLimit    string  `json:"time_to_limit"`
	Recommendation string  `json:"recommendation"`
}

// RiskAssessment represents risk analysis
type RiskAssessment struct {
	RiskLevel   string   `json:"risk_level"` // low, medium, high, critical
	RiskScore   float64  `json:"risk_score"`
	RiskFactors []string `json:"risk_factors"`
	Mitigation  string   `json:"mitigation"`
}

// HealthMetrics provides health analytics
type HealthMetrics struct {
	MemberHealth    map[string]*MemberHealth `json:"member_health"`
	OverallHealth   float64                  `json:"overall_health"`
	HealthTrend     *Trend                   `json:"health_trend"`
	Issues          []*HealthIssue           `json:"issues"`
	Recommendations []string                 `json:"recommendations"`
}

// MemberHealth represents individual member health
type MemberHealth struct {
	Score     float64   `json:"score"`
	Status    string    `json:"status"`
	Issues    []string  `json:"issues"`
	LastCheck time.Time `json:"last_check"`
	Trend     *Trend    `json:"trend"`
}

// HealthIssue represents a health issue
type HealthIssue struct {
	Member      string     `json:"member"`
	Type        string     `json:"type"`
	Severity    string     `json:"severity"`
	Description string     `json:"description"`
	DetectedAt  time.Time  `json:"detected_at"`
	ResolvedAt  *time.Time `json:"resolved_at,omitempty"`
}

// UsageMetrics provides usage analytics
type UsageMetrics struct {
	DataUsage      map[string]*DataUsage      `json:"data_usage"`
	BandwidthUsage map[string]*BandwidthUsage `json:"bandwidth_usage"`
	PeakUsage      map[string]*PeakUsage      `json:"peak_usage"`
	UsagePatterns  map[string]*UsagePattern   `json:"usage_patterns"`
}

// DataUsage represents data usage statistics
type DataUsage struct {
	Current    uint64  `json:"current"`
	Limit      uint64  `json:"limit"`
	Percentage float64 `json:"percentage"`
	Trend      *Trend  `json:"trend"`
	Projection uint64  `json:"projection"`
}

// BandwidthUsage represents bandwidth usage
type BandwidthUsage struct {
	Current     float64 `json:"current"`
	Average     float64 `json:"average"`
	Peak        float64 `json:"peak"`
	Utilization float64 `json:"utilization"`
}

// PeakUsage represents peak usage information
type PeakUsage struct {
	Value     float64   `json:"value"`
	Timestamp time.Time `json:"timestamp"`
	Duration  string    `json:"duration"`
}

// UsagePattern represents usage pattern analysis
type UsagePattern struct {
	Pattern     string  `json:"pattern"`
	Confidence  float64 `json:"confidence"`
	Description string  `json:"description"`
}

// AnalyticsAlert represents an analytics alert
type AnalyticsAlert struct {
	ID           string    `json:"id"`
	Type         string    `json:"type"`
	Severity     string    `json:"severity"`
	Title        string    `json:"title"`
	Message      string    `json:"message"`
	Timestamp    time.Time `json:"timestamp"`
	Acknowledged bool      `json:"acknowledged"`
}

// AnalyticsRecommendation represents an analytics recommendation
type AnalyticsRecommendation struct {
	ID          string    `json:"id"`
	Type        string    `json:"type"`
	Priority    string    `json:"priority"`
	Title       string    `json:"title"`
	Description string    `json:"description"`
	Impact      string    `json:"impact"`
	Effort      string    `json:"effort"`
	Timestamp   time.Time `json:"timestamp"`
}

// NewEngine creates a new analytics engine
func NewEngine(store *telem.Store, logger *logx.Logger, config *Config) *Engine {
	if config == nil {
		config = &Config{
			Enabled:          true,
			UpdateInterval:   5 * time.Minute,
			RetentionPeriod:  24 * time.Hour,
			MaxDataPoints:    1000,
			TrendWindow:      1 * time.Hour,
			PredictionWindow: 24 * time.Hour,
			HealthThresholds: HealthThresholds{
				Excellent: 80.0,
				Good:      60.0,
				Fair:      40.0,
				Poor:      20.0,
				Critical:  0.0,
			},
		}
	}

	engine := &Engine{
		store:  store,
		logger: logger,
		config: config,
	}

	// Initialize analytics components
	engine.performanceAnalyzer = NewPerformanceAnalyzer(store, logger)
	engine.trendAnalyzer = NewTrendAnalyzer(store, logger, config.TrendWindow)
	engine.predictiveAnalyzer = NewPredictiveAnalyzer(store, logger, config.PredictionWindow)
	engine.healthAnalyzer = NewHealthAnalyzer(store, logger, config.HealthThresholds)
	engine.usageAnalyzer = NewUsageAnalyzer(store, logger)

	return engine
}

// Start starts the analytics engine
func (e *Engine) Start(ctx context.Context) error {
	e.logger.Info("Starting analytics engine")

	// Initial metrics calculation
	if err := e.updateMetrics(); err != nil {
		e.logger.Error("Failed to calculate initial metrics", "error", err)
	}

	// Start periodic updates
	go e.updateLoop(ctx)

	return nil
}

// updateLoop runs the periodic metrics update loop
func (e *Engine) updateLoop(ctx context.Context) {
	ticker := time.NewTicker(e.config.UpdateInterval)
	defer ticker.Stop()

	for {
		select {
		case <-ctx.Done():
			e.logger.Info("Analytics engine stopped")
			return
		case <-ticker.C:
			if err := e.updateMetrics(); err != nil {
				e.logger.Error("Failed to update metrics", "error", err)
			}
		}
	}
}

// updateMetrics updates all dashboard metrics
func (e *Engine) updateMetrics() error {
	e.mu.Lock()
	defer e.mu.Unlock()

	now := time.Now()
	e.logger.Debug("Updating analytics metrics")

	// Calculate all metrics
	overview, err := e.calculateOverview()
	if err != nil {
		return err
	}

	performance, err := e.performanceAnalyzer.Analyze()
	if err != nil {
		return err
	}

	trends, err := e.trendAnalyzer.Analyze()
	if err != nil {
		return err
	}

	predictions, err := e.predictiveAnalyzer.Analyze()
	if err != nil {
		return err
	}

	health, err := e.healthAnalyzer.Analyze()
	if err != nil {
		return err
	}

	usage, err := e.usageAnalyzer.Analyze()
	if err != nil {
		return err
	}

	alerts, err := e.generateAlerts()
	if err != nil {
		return err
	}

	recommendations, err := e.generateRecommendations()
	if err != nil {
		return err
	}

	// Update metrics
	e.metrics = &DashboardMetrics{
		Timestamp:       now,
		Overview:        overview,
		Performance:     performance,
		Trends:          trends,
		Predictions:     predictions,
		Health:          health,
		Usage:           usage,
		Alerts:          alerts,
		Recommendations: recommendations,
	}

	e.lastUpdate = now
	e.logger.Debug("Analytics metrics updated successfully")
	return nil
}

// GetDashboardMetrics returns the current dashboard metrics
func (e *Engine) GetDashboardMetrics() *DashboardMetrics {
	e.mu.RLock()
	defer e.mu.RUnlock()

	if e.metrics == nil {
		return &DashboardMetrics{
			Timestamp: time.Now(),
		}
	}

	return e.metrics
}

// GetMetricsForMember returns analytics metrics for a specific member
func (e *Engine) GetMetricsForMember(memberName string, hours int) (*MemberAnalytics, error) {
	e.mu.RLock()
	defer e.mu.RUnlock()

	since := time.Now().Add(-time.Duration(hours) * time.Hour)
	samples, err := e.store.GetSamples(memberName, since)
	if err != nil {
		return nil, err
	}

	return e.calculateMemberAnalytics(memberName, samples)
}

// calculateOverview calculates system overview metrics
func (e *Engine) calculateOverview() (*SystemOverview, error) {
	// This would integrate with the controller to get member information
	// For now, return a placeholder
	return &SystemOverview{
		TotalMembers:  0,
		ActiveMembers: 0,
		OverallHealth: 0.0,
		Uptime:        "0h 0m",
		FailoverCount: 0,
		SuccessRate:   0.0,
	}, nil
}

// calculateMemberAnalytics calculates analytics for a specific member
func (e *Engine) calculateMemberAnalytics(memberName string, samples []*telem.Sample) (*MemberAnalytics, error) {
	if len(samples) == 0 {
		return &MemberAnalytics{
			Member: memberName,
		}, nil
	}

	// Calculate basic statistics
	var totalLatency, totalLoss, totalSignal float64
	var latencyCount, lossCount, signalCount int

	for _, sample := range samples {
		if sample.Metrics.LatencyMS != nil {
			totalLatency += *sample.Metrics.LatencyMS
			latencyCount++
		}
		if sample.Metrics.LossPercent != nil {
			totalLoss += *sample.Metrics.LossPercent
			lossCount++
		}
		if sample.Metrics.SignalStrength != nil {
			totalSignal += float64(*sample.Metrics.SignalStrength)
			signalCount++
		}
	}

	avgLatency := 0.0
	if latencyCount > 0 {
		avgLatency = totalLatency / float64(latencyCount)
	}

	avgLoss := 0.0
	if lossCount > 0 {
		avgLoss = totalLoss / float64(lossCount)
	}

	avgSignal := 0.0
	if signalCount > 0 {
		avgSignal = totalSignal / float64(signalCount)
	}

	return &MemberAnalytics{
		Member:         memberName,
		SampleCount:    len(samples),
		AverageLatency: avgLatency,
		AverageLoss:    avgLoss,
		AverageSignal:  avgSignal,
		LastSample:     samples[len(samples)-1],
	}, nil
}

// generateAlerts generates analytics alerts
func (e *Engine) generateAlerts() ([]*AnalyticsAlert, error) {
	// Placeholder implementation
	return []*AnalyticsAlert{}, nil
}

// generateRecommendations generates analytics recommendations
func (e *Engine) generateRecommendations() ([]*AnalyticsRecommendation, error) {
	// Placeholder implementation
	return []*AnalyticsRecommendation{}, nil
}

// MemberAnalytics represents analytics for a specific member
type MemberAnalytics struct {
	Member         string        `json:"member"`
	SampleCount    int           `json:"sample_count"`
	AverageLatency float64       `json:"average_latency"`
	AverageLoss    float64       `json:"average_loss"`
	AverageSignal  float64       `json:"average_signal"`
	LastSample     *telem.Sample `json:"last_sample,omitempty"`
}
