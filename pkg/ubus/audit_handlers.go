package ubus

import (
	"context"
	"encoding/json"
	"fmt"
	"time"

	"github.com/markus-lassfolk/autonomy/pkg/audit"
)

// handleAuditDecisionsWrapper wraps the audit decisions handler for ubus
func (s *Server) handleAuditDecisionsWrapper(ctx context.Context, data json.RawMessage) (interface{}, error) {
	var params map[string]interface{}
	if err := json.Unmarshal(data, &params); err != nil {
		return nil, fmt.Errorf("failed to parse parameters: %w", err)
	}
	return s.handleAuditDecisions(ctx, params)
}

// handleAuditPatternsWrapper wraps the audit patterns handler for ubus
func (s *Server) handleAuditPatternsWrapper(ctx context.Context, data json.RawMessage) (interface{}, error) {
	var params map[string]interface{}
	if err := json.Unmarshal(data, &params); err != nil {
		return nil, fmt.Errorf("failed to parse parameters: %w", err)
	}
	return s.handleAuditPatterns(ctx, params)
}

// handleAuditRootCauseWrapper wraps the audit root cause handler for ubus
func (s *Server) handleAuditRootCauseWrapper(ctx context.Context, data json.RawMessage) (interface{}, error) {
	var params map[string]interface{}
	if err := json.Unmarshal(data, &params); err != nil {
		return nil, fmt.Errorf("failed to parse parameters: %w", err)
	}
	return s.handleAuditRootCause(ctx, params)
}

// handleAuditStatsWrapper wraps the audit stats handler for ubus
func (s *Server) handleAuditStatsWrapper(ctx context.Context, data json.RawMessage) (interface{}, error) {
	var params map[string]interface{}
	if err := json.Unmarshal(data, &params); err != nil {
		return nil, fmt.Errorf("failed to parse parameters: %w", err)
	}
	return s.handleAuditStats(ctx, params)
}

// handleAuditDecisions handles audit decisions queries
func (s *Server) handleAuditDecisions(ctx context.Context, params map[string]interface{}) (interface{}, error) {
	if s.decision == nil || s.decision.DecisionLogger == nil {
		return map[string]interface{}{
			"error":   "Audit system not available",
			"message": "Decision audit logger not initialized",
		}, nil
	}

	// Parse parameters
	since := time.Now().Add(-24 * time.Hour) // Default to last 24 hours
	if val, ok := params["since"]; ok {
		if sinceStr, ok := val.(string); ok {
			if parsed, err := time.Parse(time.RFC3339, sinceStr); err == nil {
				since = parsed
			}
		}
	}

	limit := 100 // Default limit
	if val, ok := params["limit"]; ok {
		if limitVal, ok := val.(float64); ok {
			limit = int(limitVal)
		} else if limitVal, ok := val.(int); ok {
			limit = limitVal
		}
	}

	decisionType := ""
	if val, ok := params["type"]; ok {
		if typeStr, ok := val.(string); ok {
			decisionType = typeStr
		}
	}

	var decisions []*audit.DecisionRecord
	if decisionType != "" {
		decisions = s.decision.DecisionLogger.GetDecisionsByType(decisionType, limit)
	} else {
		decisions = s.decision.DecisionLogger.GetRecentDecisions(since, limit)
	}

	return map[string]interface{}{
		"decisions": decisions,
		"count":     len(decisions),
		"since":     since.Format(time.RFC3339),
		"limit":     limit,
	}, nil
}

// handleAuditPatterns handles audit patterns queries
func (s *Server) handleAuditPatterns(ctx context.Context, params map[string]interface{}) (interface{}, error) {
	if s.decision == nil || s.decision.PatternAnalyzer == nil {
		return map[string]interface{}{
			"error":   "Audit system not available",
			"message": "Pattern analyzer not initialized",
		}, nil
	}

	// Parse parameters
	window := 24 * time.Hour // Default to 24 hours
	if val, ok := params["window_hours"]; ok {
		if hours, ok := val.(float64); ok {
			window = time.Duration(hours) * time.Hour
		} else if hours, ok := val.(int); ok {
			window = time.Duration(hours) * time.Hour
		}
	}

	// Get recent decisions for pattern analysis
	since := time.Now().Add(-window)
	decisions := s.decision.DecisionLogger.GetRecentDecisions(since, 1000)

	// Analyze patterns
	patterns := s.decision.PatternAnalyzer.AnalyzePatterns(decisions, window)

	return map[string]interface{}{
		"patterns": patterns,
		"count":    len(patterns),
		"window":   window.String(),
	}, nil
}

// handleAuditRootCause handles audit root cause queries
func (s *Server) handleAuditRootCause(ctx context.Context, params map[string]interface{}) (interface{}, error) {
	if s.decision == nil || s.decision.RootCauseAnalyzer == nil {
		return map[string]interface{}{
			"error":   "Audit system not available",
			"message": "Root cause analyzer not initialized",
		}, nil
	}

	// Parse parameters
	decisionID := ""
	if val, ok := params["decision_id"]; ok {
		if idStr, ok := val.(string); ok {
			decisionID = idStr
		}
	}

	category := ""
	if val, ok := params["category"]; ok {
		if catStr, ok := val.(string); ok {
			category = catStr
		}
	}

	if decisionID != "" {
		// Get specific decision
		decision := s.decision.DecisionLogger.GetDecisionByID(decisionID)
		if decision == nil {
			return map[string]interface{}{
				"error":   "Decision not found",
				"message": fmt.Sprintf("Decision with ID %s not found", decisionID),
			}, nil
		}

		// Get related records for analysis
		relatedRecords := s.decision.DecisionLogger.GetRecentDecisions(time.Now().Add(-1*time.Hour), 50)
		rootCause := s.decision.RootCauseAnalyzer.AnalyzeRootCause(decision, relatedRecords)

		return map[string]interface{}{
			"decision_id": decisionID,
			"root_cause":  rootCause,
		}, nil
	}

	if category != "" {
		// Get decisions by category
		decisions := s.decision.DecisionLogger.GetRecentDecisions(time.Now().Add(-24*time.Hour), 1000)
		rootCauses := s.decision.RootCauseAnalyzer.GetRootCauseByCategory(decisions, category)

		return map[string]interface{}{
			"category":    category,
			"root_causes": rootCauses,
			"count":       len(rootCauses),
		}, nil
	}

	return map[string]interface{}{
		"error":   "Missing parameter",
		"message": "Either decision_id or category parameter is required",
	}, nil
}

// handleAuditStats handles audit statistics queries
func (s *Server) handleAuditStats(ctx context.Context, params map[string]interface{}) (interface{}, error) {
	if s.decision == nil || s.decision.DecisionLogger == nil {
		return map[string]interface{}{
			"error":   "Audit system not available",
			"message": "Decision audit logger not initialized",
		}, nil
	}

	// Parse parameters
	since := time.Now().Add(-24 * time.Hour) // Default to last 24 hours
	if val, ok := params["since"]; ok {
		if sinceStr, ok := val.(string); ok {
			if parsed, err := time.Parse(time.RFC3339, sinceStr); err == nil {
				since = parsed
			}
		}
	}

	// Get decision statistics
	decisionStats := s.decision.DecisionLogger.GetDecisionStats(since)

	// Get root cause statistics
	decisions := s.decision.DecisionLogger.GetRecentDecisions(since, 1000)
	rootCauseStats := s.decision.RootCauseAnalyzer.GetRootCauseStats(decisions)

	return map[string]interface{}{
		"decision_stats":   decisionStats,
		"root_cause_stats": rootCauseStats,
		"since":            since.Format(time.RFC3339),
	}, nil
}
