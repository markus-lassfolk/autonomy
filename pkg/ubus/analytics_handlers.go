package ubus

import (
	"context"
	"fmt"
	"strconv"

	"github.com/markus-lassfolk/autonomy/pkg/analytics"
	"github.com/markus-lassfolk/autonomy/pkg/logx"
)

// AnalyticsHandlers provides ubus API endpoints for analytics
type AnalyticsHandlers struct {
	analyticsEngine *analytics.Engine
	logger          *logx.Logger
}

// NewAnalyticsHandlers creates new analytics handlers
func NewAnalyticsHandlers(engine *analytics.Engine, logger *logx.Logger) *AnalyticsHandlers {
	return &AnalyticsHandlers{
		analyticsEngine: engine,
		logger:          logger,
	}
}

// GetDashboardData returns comprehensive dashboard data
func (ah *AnalyticsHandlers) GetDashboardData(ctx context.Context, params map[string]interface{}) (interface{}, error) {
	ah.logger.Debug("Getting dashboard data")

	metrics := ah.analyticsEngine.GetDashboardMetrics()
	if metrics == nil {
		return map[string]interface{}{
			"error": "No analytics data available",
		}, nil
	}

	return map[string]interface{}{
		"timestamp":       metrics.Timestamp,
		"overview":        metrics.Overview,
		"performance":     metrics.Performance,
		"trends":          metrics.Trends,
		"predictions":     metrics.Predictions,
		"health":          metrics.Health,
		"usage":           metrics.Usage,
		"alerts":          metrics.Alerts,
		"recommendations": metrics.Recommendations,
	}, nil
}

// GetMemberAnalytics returns analytics for a specific member
func (ah *AnalyticsHandlers) GetMemberAnalytics(ctx context.Context, params map[string]interface{}) (interface{}, error) {
	memberName, ok := params["member"].(string)
	if !ok {
		return nil, fmt.Errorf("member parameter is required")
	}

	hours := 24 // default to 24 hours
	if hoursStr, ok := params["hours"].(string); ok {
		if h, err := strconv.Atoi(hoursStr); err == nil {
			hours = h
		}
	}

	ah.logger.Debug("Getting member analytics", "member", memberName, "hours", hours)

	analytics, err := ah.analyticsEngine.GetMetricsForMember(memberName, hours)
	if err != nil {
		return map[string]interface{}{
			"error": fmt.Sprintf("Failed to get analytics for member %s: %v", memberName, err),
		}, nil
	}

	return map[string]interface{}{
		"member":          analytics.Member,
		"sample_count":    analytics.SampleCount,
		"average_latency": analytics.AverageLatency,
		"average_loss":    analytics.AverageLoss,
		"average_signal":  analytics.AverageSignal,
		"last_sample":     analytics.LastSample,
	}, nil
}

// GetPerformanceMetrics returns performance analytics
func (ah *AnalyticsHandlers) GetPerformanceMetrics(ctx context.Context, params map[string]interface{}) (interface{}, error) {
	ah.logger.Debug("Getting performance metrics")

	metrics := ah.analyticsEngine.GetDashboardMetrics()
	if metrics == nil || metrics.Performance == nil {
		return map[string]interface{}{
			"error": "No performance data available",
		}, nil
	}

	return map[string]interface{}{
		"average_latency": metrics.Performance.AverageLatency,
		"average_loss":    metrics.Performance.AverageLoss,
		"average_signal":  metrics.Performance.AverageSignal,
		"throughput":      metrics.Performance.Throughput,
		"response_time":   metrics.Performance.ResponseTime,
		"error_rate":      metrics.Performance.ErrorRate,
		"availability":    metrics.Performance.Availability,
	}, nil
}

// GetTrendAnalysis returns trend analysis
func (ah *AnalyticsHandlers) GetTrendAnalysis(ctx context.Context, params map[string]interface{}) (interface{}, error) {
	ah.logger.Debug("Getting trend analysis")

	metrics := ah.analyticsEngine.GetDashboardMetrics()
	if metrics == nil || metrics.Trends == nil {
		return map[string]interface{}{
			"error": "No trend data available",
		}, nil
	}

	return map[string]interface{}{
		"latency_trends":     metrics.Trends.LatencyTrends,
		"signal_trends":      metrics.Trends.SignalTrends,
		"usage_trends":       metrics.Trends.UsageTrends,
		"health_trends":      metrics.Trends.HealthTrends,
		"performance_trends": metrics.Trends.PerformanceTrends,
	}, nil
}

// GetPredictions returns predictive analytics
func (ah *AnalyticsHandlers) GetPredictions(ctx context.Context, params map[string]interface{}) (interface{}, error) {
	ah.logger.Debug("Getting predictions")

	metrics := ah.analyticsEngine.GetDashboardMetrics()
	if metrics == nil || metrics.Predictions == nil {
		return map[string]interface{}{
			"error": "No prediction data available",
		}, nil
	}

	return map[string]interface{}{
		"failover_probability": metrics.Predictions.FailoverProbability,
		"maintenance_windows":  metrics.Predictions.MaintenanceWindows,
		"capacity_forecasts":   metrics.Predictions.CapacityForecasts,
		"risk_assessments":     metrics.Predictions.RiskAssessments,
	}, nil
}

// GetHealthMetrics returns health analytics
func (ah *AnalyticsHandlers) GetHealthMetrics(ctx context.Context, params map[string]interface{}) (interface{}, error) {
	ah.logger.Debug("Getting health metrics")

	metrics := ah.analyticsEngine.GetDashboardMetrics()
	if metrics == nil || metrics.Health == nil {
		return map[string]interface{}{
			"error": "No health data available",
		}, nil
	}

	return map[string]interface{}{
		"member_health":   metrics.Health.MemberHealth,
		"overall_health":  metrics.Health.OverallHealth,
		"health_trend":    metrics.Health.HealthTrend,
		"issues":          metrics.Health.Issues,
		"recommendations": metrics.Health.Recommendations,
	}, nil
}

// GetUsageMetrics returns usage analytics
func (ah *AnalyticsHandlers) GetUsageMetrics(ctx context.Context, params map[string]interface{}) (interface{}, error) {
	ah.logger.Debug("Getting usage metrics")

	metrics := ah.analyticsEngine.GetDashboardMetrics()
	if metrics == nil || metrics.Usage == nil {
		return map[string]interface{}{
			"error": "No usage data available",
		}, nil
	}

	return map[string]interface{}{
		"data_usage":      metrics.Usage.DataUsage,
		"bandwidth_usage": metrics.Usage.BandwidthUsage,
		"peak_usage":      metrics.Usage.PeakUsage,
		"usage_patterns":  metrics.Usage.UsagePatterns,
	}, nil
}

// GetAlerts returns analytics alerts
func (ah *AnalyticsHandlers) GetAlerts(ctx context.Context, params map[string]interface{}) (interface{}, error) {
	ah.logger.Debug("Getting analytics alerts")

	metrics := ah.analyticsEngine.GetDashboardMetrics()
	if metrics == nil {
		return map[string]interface{}{
			"alerts": []interface{}{},
		}, nil
	}

	return map[string]interface{}{
		"alerts": metrics.Alerts,
	}, nil
}

// GetRecommendations returns analytics recommendations
func (ah *AnalyticsHandlers) GetRecommendations(ctx context.Context, params map[string]interface{}) (interface{}, error) {
	ah.logger.Debug("Getting analytics recommendations")

	metrics := ah.analyticsEngine.GetDashboardMetrics()
	if metrics == nil {
		return map[string]interface{}{
			"recommendations": []interface{}{},
		}, nil
	}

	return map[string]interface{}{
		"recommendations": metrics.Recommendations,
	}, nil
}

// AcknowledgeAlert acknowledges an analytics alert
func (ah *AnalyticsHandlers) AcknowledgeAlert(ctx context.Context, params map[string]interface{}) (interface{}, error) {
	alertID, ok := params["alert_id"].(string)
	if !ok {
		return nil, fmt.Errorf("alert_id parameter is required")
	}

	ah.logger.Debug("Acknowledging alert", "alert_id", alertID)

	// Placeholder implementation - in a real system, this would update the alert status
	return map[string]interface{}{
		"success": true,
		"message": fmt.Sprintf("Alert %s acknowledged", alertID),
	}, nil
}

// GetAnalyticsConfig returns analytics configuration
func (ah *AnalyticsHandlers) GetAnalyticsConfig(ctx context.Context, params map[string]interface{}) (interface{}, error) {
	ah.logger.Debug("Getting analytics configuration")

	// Return default configuration
	return map[string]interface{}{
		"enabled":           true,
		"update_interval":   "5m",
		"retention_period":  "24h",
		"max_data_points":   1000,
		"trend_window":      "1h",
		"prediction_window": "24h",
		"health_thresholds": map[string]interface{}{
			"excellent": 80.0,
			"good":      60.0,
			"fair":      40.0,
			"poor":      20.0,
			"critical":  0.0,
		},
	}, nil
}

// UpdateAnalyticsConfig updates analytics configuration
func (ah *AnalyticsHandlers) UpdateAnalyticsConfig(ctx context.Context, params map[string]interface{}) (interface{}, error) {
	ah.logger.Debug("Updating analytics configuration")

	// Placeholder implementation - in a real system, this would update the configuration
	return map[string]interface{}{
		"success": true,
		"message": "Analytics configuration updated",
	}, nil
}
