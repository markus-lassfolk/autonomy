package decision

import (
	"context"
	"fmt"
	"math"
	"os"
	"sort"
	"sync"
	"time"

	"github.com/markus-lassfolk/autonomy/pkg"
	"github.com/markus-lassfolk/autonomy/pkg/audit"
	"github.com/markus-lassfolk/autonomy/pkg/collector"
	"github.com/markus-lassfolk/autonomy/pkg/discovery"
	"github.com/markus-lassfolk/autonomy/pkg/logx"
	"github.com/markus-lassfolk/autonomy/pkg/monitoring"
	"github.com/markus-lassfolk/autonomy/pkg/telem"
	"github.com/markus-lassfolk/autonomy/pkg/uci"
)

// Engine implements the decision logic for failover
type Engine struct {
	mu sync.RWMutex

	// Configuration
	config *uci.Config

	// Dependencies
	logger             *logx.Logger
	telemetry          *telem.Store
	adaptiveMonitoring *monitoring.AdaptiveMonitoringManager

	// Audit system
	DecisionLogger    *audit.DecisionLogger
	PatternAnalyzer   *audit.PatternAnalyzer
	RootCauseAnalyzer *audit.RootCauseAnalyzer

	// State
	members     map[string]*pkg.Member
	memberState map[string]*MemberState
	current     *pkg.Member
	lastSwitch  time.Time

	// Scoring state
	scores map[string]*pkg.Score

	// Hysteresis state
	badWindows  map[string]time.Time
	goodWindows map[string]time.Time
	cooldowns   map[string]time.Time
	warmups     map[string]time.Time

	// Predictive state
	lastPredictive time.Time
	predictiveRate time.Duration

	// Advanced predictive algorithms
	predictiveModels map[string]*PredictiveModel
	trendAnalysis    map[string]*TrendAnalysis
	patternDetector  *PatternDetector
	mlPredictor      *MLPredictor
	predictiveEngine *PredictiveEngine

	// CPU optimization: Caching for expensive calculations
	scoreCache     map[string]*ScoreCacheEntry
	lastScoreClean time.Time
	scoreCacheTTL  time.Duration

	// CPU optimization: Pre-computed normalization tables
	latencyNormTable     map[float64]float64
	lossNormTable        map[float64]float64
	jitterNormTable      map[float64]float64
	obstructionNormTable map[float64]float64

	// CPU optimization: Connection pooling for API calls
	apiConnPool map[string]*APIConnection
}

// PredictiveModel represents a predictive model for a member
type PredictiveModel struct {
	MemberName   string
	LastUpdate   time.Time
	HealthTrend  float64 // -1.0 to 1.0 (declining to improving)
	FailureRisk  float64 // 0.0 to 1.0 (low to high risk)
	RecoveryTime time.Duration
	Confidence   float64 // 0.0 to 1.0
	DataPoints   []DataPoint
	ModelType    string // "linear", "exponential", "ml"
}

// DataPoint represents a historical data point
type DataPoint struct {
	Timestamp time.Time
	Latency   float64
	Loss      float64
	Score     float64
	Status    string
}

// TrendAnalysis tracks trends for a member
type TrendAnalysis struct {
	MemberName     string
	LatencyTrend   float64 // ms per minute
	LossTrend      float64 // % per minute
	ScoreTrend     float64 // points per minute
	Volatility     float64 // standard deviation
	LastCalculated time.Time
	Window         time.Duration
}

// PatternDetector detects patterns in member behavior
type PatternDetector struct {
	patterns map[string]*Pattern
	mu       sync.RWMutex
}

// Pattern represents a detected pattern
type Pattern struct {
	ID           string
	MemberName   string
	Type         string // "cyclic", "deteriorating", "improving", "stable"
	Confidence   float64
	StartTime    time.Time
	EndTime      time.Time
	Description  string
	LatencyTrend float64 // Latency trend for pattern matching
	LossTrend    float64 // Loss trend for pattern matching
	ScoreTrend   float64 // Score trend for pattern matching
}

// ScoreCacheEntry represents a cached score calculation
type ScoreCacheEntry struct {
	Score       *pkg.Score
	MetricsHash uint64
	Timestamp   time.Time
	TTL         time.Duration
}

// APIConnection represents a pooled API connection
type APIConnection struct {
	LastUsed time.Time
	Client   interface{} // Type depends on API
	Healthy  bool
}

// Note: MLPredictor and MLModel are defined in predictive.go to avoid duplication

// MemberState tracks the state of a member
type MemberState struct {
	Member     *pkg.Member
	LastSeen   time.Time
	LastUpdate time.Time
	Status     string // eligible|cooldown|warmup|failed
	Uptime     time.Duration
}

// NewEngine creates a new decision engine
func NewEngine(config *uci.Config, logger *logx.Logger, telemetry *telem.Store) *Engine {
	// Create adaptive monitoring manager
	adaptiveMonitoring := monitoring.NewAdaptiveMonitoringManager(logger)
	// Create predictive engine configuration
	predictiveConfig := &PredictiveConfig{
		Enabled:             config.Predictive,
		LookbackWindow:      time.Duration(config.HistoryWindowS) * time.Second,
		PredictionHorizon:   time.Duration(config.FailMinDurationS*2) * time.Second,
		ConfidenceThreshold: 0.7,
		AnomalyThreshold:    0.8,
		TrendSensitivity:    0.1,
		PatternMinSamples:   10,
		MLEnabled:           true,
		MLModelPath:         "/tmp/autonomy/ml_models.json",
	}

	// Ensure ML model directory exists
	if predictiveConfig.MLEnabled {
		if err := os.MkdirAll("/tmp/autonomy", 0o755); err != nil {
			logger.Warn("Failed to create ML model directory, disabling ML features", "error", err)
			predictiveConfig.MLEnabled = false
		}
	}

	// Initialize audit system
	decisionLogger := audit.NewDecisionLogger(logger, 1000, "/var/log/autonomy")
	patternAnalyzer := audit.NewPatternAnalyzer(logger)
	rootCauseAnalyzer := audit.NewRootCauseAnalyzer(logger)

	return &Engine{
		config:               config,
		logger:               logger,
		telemetry:            telemetry,
		adaptiveMonitoring:   adaptiveMonitoring,
		DecisionLogger:       decisionLogger,
		PatternAnalyzer:      patternAnalyzer,
		RootCauseAnalyzer:    rootCauseAnalyzer,
		members:              make(map[string]*pkg.Member),
		memberState:          make(map[string]*MemberState),
		scores:               make(map[string]*pkg.Score),
		badWindows:           make(map[string]time.Time),
		goodWindows:          make(map[string]time.Time),
		cooldowns:            make(map[string]time.Time),
		warmups:              make(map[string]time.Time),
		predictiveRate:       time.Duration(config.FailMinDurationS*5) * time.Second,
		predictiveModels:     make(map[string]*PredictiveModel),
		trendAnalysis:        make(map[string]*TrendAnalysis),
		patternDetector:      NewPatternDetector(),
		mlPredictor:          NewMLPredictor("", logger), // Use empty model path for now
		predictiveEngine:     NewPredictiveEngine(predictiveConfig, logger),
		scoreCache:           make(map[string]*ScoreCacheEntry),
		scoreCacheTTL:        30 * time.Second, // Default 30 second cache TTL
		latencyNormTable:     make(map[float64]float64),
		lossNormTable:        make(map[float64]float64),
		jitterNormTable:      make(map[float64]float64),
		obstructionNormTable: make(map[float64]float64),
		apiConnPool:          make(map[string]*APIConnection),
	}
}

// Initialize initializes the decision engine with the current active member
func (e *Engine) Initialize(controller pkg.Controller) error {
	e.mu.Lock()
	defer e.mu.Unlock()

	// Get the current active member from the controller
	currentMember, err := controller.GetCurrentMember()
	if err != nil {
		e.logger.Warn("Failed to get current member during initialization", "error", err)
		// Don't return error, just log warning - this is not critical
		return nil
	}

	if currentMember != nil {
		e.current = currentMember
		e.logger.Info("Initialized decision engine with current active member",
			"member", currentMember.Name,
			"interface", currentMember.Iface,
			"class", currentMember.Class)
	} else {
		e.logger.Info("No current active member found during initialization")
	}

	return nil
}

// Tick performs one decision cycle
func (e *Engine) Tick(controller pkg.Controller) error {
	e.mu.Lock()
	defer e.mu.Unlock()

	// Update member states
	e.updateMemberStates()

	// Collect metrics for all members
	if err := e.collectMetrics(); err != nil {
		e.logger.Error("Failed to collect metrics", "error", err)
		return err
	}

	// Update scores
	e.updateScores()

	// Make decision
	if err := e.makeDecision(controller); err != nil {
		e.logger.Error("Failed to make decision", "error", err)
		return err
	}

	return nil
}

// updateMemberStates updates the state of all members
func (e *Engine) updateMemberStates() {
	now := time.Now()

	for name, member := range e.members {
		state, exists := e.memberState[name]
		if !exists {
			state = &MemberState{
				Member:     member,
				LastSeen:   now,
				LastUpdate: now,
				Status:     pkg.StatusWarmup, // Start in warmup, will become eligible after min_uptime
			}
			e.memberState[name] = state
			e.logger.Debug("Created new member state", "member", name, "status", state.Status)
		}

		// Update uptime (time since member was first seen)
		state.Uptime = now.Sub(state.LastSeen)

		// Update last update time
		state.LastUpdate = now

		// Check cooldown
		if cooldownUntil, exists := e.cooldowns[name]; exists && now.Before(cooldownUntil) {
			state.Status = pkg.StatusCooldown
			continue
		}

		// Check warmup
		if warmupUntil, exists := e.warmups[name]; exists && now.Before(warmupUntil) {
			state.Status = pkg.StatusWarmup
			continue
		}

		// Check minimum uptime
		minUptime := time.Duration(e.config.MinUptimeS) * time.Second
		if state.Uptime < minUptime {
			state.Status = pkg.StatusWarmup
			e.logger.Debug("Member in warmup", "member", name, "uptime", state.Uptime, "required", minUptime)
			continue
		}

		state.Status = pkg.StatusEligible
		e.logger.Debug("Member is eligible", "member", name, "uptime", state.Uptime)
	}
}

// collectMetrics collects metrics for all members
func (e *Engine) collectMetrics() error {
	ctx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
	defer cancel()

	e.logger.Debug("Starting metrics collection", "member_count", len(e.members))

	for name, member := range e.members {
		// Skip members in cooldown or warmup
		if state := e.memberState[name]; state != nil {
			if state.Status == pkg.StatusCooldown || state.Status == pkg.StatusWarmup {
				e.logger.Debug("Skipping member in cooldown/warmup", "member", name, "status", state.Status, "uptime", state.Uptime)
				continue
			}
		}

		// Check adaptive monitoring mode
		monitoringMode := e.adaptiveMonitoring.GetMonitoringMode(member)
		if monitoringMode == monitoring.MonitoringDisabled {
			// Cast to get usage percentage for logging
			if dataLimit, ok := member.DataLimitConfig.(*discovery.DataLimitConfig); ok && dataLimit != nil {
				e.logger.Info("Skipping metrics collection - monitoring disabled due to data limit",
					"member", name,
					"data_usage_percent", dataLimit.UsagePercentage)
			}
			continue
		}

		// Log monitoring mode and data usage savings
		if member.DataLimitConfig != nil {
			if dataLimit, ok := member.DataLimitConfig.(*discovery.DataLimitConfig); ok && dataLimit != nil {
				savings := e.adaptiveMonitoring.CalculateDataUsageSavings(monitoringMode, member.Class)
				monthlyUsage := e.adaptiveMonitoring.EstimateMonthlyDataUsage(monitoringMode, member.Class)
				e.logger.Debug("Adaptive monitoring active",
					"member", name,
					"mode", monitoringMode.String(),
					"data_usage_percent", dataLimit.UsagePercentage,
					"monthly_monitoring_mb", monthlyUsage.TotalUsageMB,
					"data_savings_percent", savings.TotalSavingsPercent)
			}
		}

		e.logger.Debug("Collecting metrics for member", "member", name, "class", member.Class, "monitoring_mode", monitoringMode.String())

		// Collect metrics
		metrics, err := e.collectMemberMetrics(ctx, member)
		if err != nil {
			e.logger.Error("Failed to collect metrics for member", "member", name, "error", err)
			continue
		}

		e.logger.Debug("Successfully collected metrics", "member", name, "latency", metrics.LatencyMS, "loss", metrics.LossPercent)

		// Store in telemetry
		if err := e.telemetry.AddSample(name, metrics, e.scores[name]); err != nil {
			e.logger.Error("Failed to store metrics", "member", name, "error", err)
		}

		// Update member state
		if state := e.memberState[name]; state != nil {
			state.LastUpdate = time.Now()
		}
	}

	return nil
}

// collectorFactory returns the appropriate collector for a member based on its class
func (e *Engine) collectorFactory(member *pkg.Member) (pkg.Collector, error) {
	cfg := map[string]interface{}{}

	switch member.Class {
	case pkg.ClassStarlink:
		return collector.NewSimpleStarlinkCollector(cfg)
	case pkg.ClassCellular:
		return collector.NewCellularCollector(cfg)
	case pkg.ClassWiFi:
		return collector.NewWiFiCollector(cfg)
	case pkg.ClassLAN:
		return collector.NewLANCollector(cfg)
	case pkg.ClassOther:
		return collector.NewGenericCollector(cfg)
	default:
		return collector.NewGenericCollector(cfg)
	}
}

// collectMemberMetrics collects metrics for a specific member
func (e *Engine) collectMemberMetrics(ctx context.Context, member *pkg.Member) (*pkg.Metrics, error) {
	coll, err := e.collectorFactory(member)
	if err != nil {
		return nil, fmt.Errorf("failed to create collector for %s: %w", member.Name, err)
	}

	metrics, err := coll.Collect(ctx, member)
	if err != nil {
		return nil, fmt.Errorf("failed to collect metrics for %s: %w", member.Name, err)
	}

	return metrics, nil
}

// updateScores updates the scores for all members
func (e *Engine) updateScores() {
	now := time.Now()

	for name, member := range e.members {
		// Get recent metrics
		samples, err := e.telemetry.GetSamples(name, now.Add(-time.Duration(e.config.HistoryWindowS)*time.Second))
		if err != nil {
			e.logger.Error("Failed to get samples for scoring", "member", name, "error", err)
			continue
		}

		if len(samples) == 0 {
			continue
		}

		// Calculate scores
		score := e.calculateScore(member, samples)
		e.scores[name] = score

		// Update predictive engine with new data
		if e.predictiveEngine != nil && len(samples) > 0 {
			latest := samples[len(samples)-1]
			e.predictiveEngine.UpdateMemberData(name, latest.Metrics, score)
		}

		// Update trend analysis
		e.updateTrendAnalysis(name, samples)
	}
}

// calculateScore calculates the score for a member based on recent samples with CPU optimization
func (e *Engine) calculateScore(member *pkg.Member, samples []*telem.Sample) *pkg.Score {
	if len(samples) == 0 {
		return &pkg.Score{
			Instant:   0,
			EWMA:      0,
			Final:     0,
			UpdatedAt: time.Now(),
		}
	}

	// CPU optimization: Check cache first
	latest := samples[len(samples)-1]
	cacheKey := fmt.Sprintf("%s:%d", member.Name, latest.Timestamp.Unix())

	e.mu.RLock()
	if cached, exists := e.scoreCache[cacheKey]; exists && time.Since(cached.Timestamp) < cached.TTL {
		e.mu.RUnlock()
		return cached.Score
	}
	e.mu.RUnlock()

	// Calculate instant score from latest sample
	instant := e.calculateInstantScore(member, latest.Metrics)

	// Calculate EWMA
	ewma := e.calculateEWMA(member.Name, instant)

	// Calculate window average
	windowAvg := e.calculateWindowAverage(samples)

	// Calculate final score
	final := 0.30*instant + 0.50*ewma + 0.20*windowAvg

	score := &pkg.Score{
		Instant:   instant,
		EWMA:      ewma,
		Final:     final,
		UpdatedAt: time.Now(),
	}

	// CPU optimization: Cache the result
	e.mu.Lock()
	e.scoreCache[cacheKey] = &ScoreCacheEntry{
		Score:       score,
		MetricsHash: e.calculateMetricsHash(latest.Metrics),
		Timestamp:   time.Now(),
		TTL:         e.scoreCacheTTL,
	}
	e.mu.Unlock()

	// CPU optimization: Clean old cache entries periodically
	e.cleanScoreCache()

	return score
}

// calculateInstantScore calculates the instant score for a member
func (e *Engine) calculateInstantScore(member *pkg.Member, metrics *pkg.Metrics) float64 {
	// Get member config
	memberConfig := e.config.Members[member.Name]

	// Apply class-specific scoring
	var score float64
	switch member.Class {
	case pkg.ClassStarlink:
		score = e.scoreStarlink(metrics, memberConfig)
	case pkg.ClassCellular:
		score = e.scoreCellular(metrics, memberConfig)
	case pkg.ClassWiFi:
		score = e.scoreWiFi(metrics, memberConfig)
	case pkg.ClassLAN:
		score = e.scoreLAN(metrics, memberConfig)
	default:
		score = e.scoreGeneric(metrics, memberConfig)
	}

	// Apply weight
	if memberConfig != nil {
		score = score * float64(memberConfig.Weight) / 100.0
	}

	// Clamp to 0-100
	return math.Max(0, math.Min(100, score))
}

// scoreStarlink calculates score for Starlink members
func (e *Engine) scoreStarlink(metrics *pkg.Metrics, config *uci.MemberConfig) float64 {
	score := 100.0

	// Latency penalty
	if metrics.LatencyMS != nil {
		latPenalty := e.normalize(*metrics.LatencyMS, 50, 1500) * 20
		score -= latPenalty
	}

	// Loss penalty
	if metrics.LossPercent != nil {
		lossPenalty := e.normalize(*metrics.LossPercent, 0, 10) * 30
		score -= lossPenalty
	}

	// Jitter penalty
	if metrics.JitterMS != nil {
		jitterPenalty := e.normalize(*metrics.JitterMS, 5, 200) * 15
		score -= jitterPenalty
	}

	// Obstruction penalty
	if metrics.ObstructionPct != nil {
		obstPenalty := e.normalize(*metrics.ObstructionPct, 0, 10) * 25
		score -= obstPenalty
	}

	// Enhanced Outage penalty (graduated based on count)
	if metrics.Outages != nil && *metrics.Outages > 0 {
		outageCount := float64(*metrics.Outages)
		// Graduated penalty: 10 points per outage, max 30 points
		outagePenalty := math.Min(outageCount*10, 30)
		score -= outagePenalty

		e.logger.Debug("Applied graduated outage penalty",
			"outages", *metrics.Outages,
			"penalty", outagePenalty)
	}

	// Enhanced Events penalty (scoring for current state)
	if metrics.Events != nil && len(*metrics.Events) > 0 {
		events := *metrics.Events
		eventPenalty := 0.0
		criticalEvents := 0
		warningEvents := 0

		// Analyze events by severity
		for _, event := range events {
			switch event.Severity {
			case "critical":
				criticalEvents++
				eventPenalty += 8 // 8 points per critical event
			case "warning":
				warningEvents++
				eventPenalty += 3 // 3 points per warning event
			default:
				eventPenalty += 1 // 1 point per info event
			}
		}

		// Cap total event penalty at 20 points
		eventPenalty = math.Min(eventPenalty, 20)
		score -= eventPenalty

		e.logger.Debug("Applied events penalty",
			"total_events", len(events),
			"critical", criticalEvents,
			"warning", warningEvents,
			"penalty", eventPenalty)
	}

	return score
}

// scoreCellular calculates score for cellular members
func (e *Engine) scoreCellular(metrics *pkg.Metrics, config *uci.MemberConfig) float64 {
	score := 100.0

	// Latency penalty
	if metrics.LatencyMS != nil {
		latPenalty := e.normalize(*metrics.LatencyMS, 50, 1500) * 20
		score -= latPenalty
	}

	// Loss penalty
	if metrics.LossPercent != nil {
		lossPenalty := e.normalize(*metrics.LossPercent, 0, 10) * 30
		score -= lossPenalty
	}

	// Signal quality bonus/penalty
	if metrics.RSRP != nil {
		// RSRP ranges from -140 to -44 dBm
		rsrpScore := float64(*metrics.RSRP+140) / 96.0 * 100
		if rsrpScore > 100 {
			rsrpScore = 100
		} else if rsrpScore < 0 {
			rsrpScore = 0
		}
		score = score*0.7 + rsrpScore*0.3
	}

	// Roaming penalty
	if metrics.Roaming != nil && *metrics.Roaming {
		if config == nil || !config.PreferRoaming {
			score -= 15 // Penalty for roaming
		}
	}

	return score
}

// scoreWiFi calculates score for WiFi members
func (e *Engine) scoreWiFi(metrics *pkg.Metrics, config *uci.MemberConfig) float64 {
	score := 100.0

	// Latency penalty
	if metrics.LatencyMS != nil {
		latPenalty := e.normalize(*metrics.LatencyMS, 50, 1500) * 20
		score -= latPenalty
	}

	// Loss penalty
	if metrics.LossPercent != nil {
		lossPenalty := e.normalize(*metrics.LossPercent, 0, 10) * 30
		score -= lossPenalty
	}

	// Signal strength bonus/penalty
	if metrics.SignalStrength != nil {
		// WiFi signal typically ranges from -100 to -30 dBm
		signalScore := float64(*metrics.SignalStrength+100) / 70.0 * 100
		if signalScore > 100 {
			signalScore = 100
		} else if signalScore < 0 {
			signalScore = 0
		}
		score = score*0.7 + signalScore*0.3
	}

	return score
}

// scoreLAN calculates score for LAN members
func (e *Engine) scoreLAN(metrics *pkg.Metrics, config *uci.MemberConfig) float64 {
	score := 100.0

	// Latency penalty (LAN should be very fast)
	if metrics.LatencyMS != nil {
		latPenalty := e.normalize(*metrics.LatencyMS, 1, 100) * 25
		score -= latPenalty
	}

	// Loss penalty (LAN should have no loss)
	if metrics.LossPercent != nil {
		lossPenalty := *metrics.LossPercent * 50 // High penalty for any loss on LAN
		score -= lossPenalty
	}

	return score
}

// scoreGeneric calculates score for generic members
func (e *Engine) scoreGeneric(metrics *pkg.Metrics, config *uci.MemberConfig) float64 {
	score := 100.0

	// Latency penalty
	if metrics.LatencyMS != nil {
		latPenalty := e.normalize(*metrics.LatencyMS, 50, 1500) * 20
		score -= latPenalty
	}

	// Loss penalty
	if metrics.LossPercent != nil {
		lossPenalty := e.normalize(*metrics.LossPercent, 0, 10) * 30
		score -= lossPenalty
	}

	return score
}

// normalize normalizes a value between 0 and 1 based on good and bad thresholds with CPU optimization
func (e *Engine) normalize(value, good, bad float64) float64 {
	if value <= good {
		return 0
	}
	if value >= bad {
		return 1
	}

	// CPU optimization: Direct calculation (can be extended with pre-computed tables)
	// In a full implementation, we would populate tables for common ranges at startup
	return (value - good) / (bad - good)
}

// calculateEWMA calculates the exponential weighted moving average
func (e *Engine) calculateEWMA(memberName string, instant float64) float64 {
	// Implement EWMA calculation with configurable alpha and historical data support
	alpha := 0.2 // EWMA factor

	if score, exists := e.scores[memberName]; exists {
		return alpha*instant + (1-alpha)*score.EWMA
	}

	return instant
}

// calculateWindowAverage calculates the average over the history window
func (e *Engine) calculateWindowAverage(samples []*telem.Sample) float64 {
	if len(samples) == 0 {
		return 0
	}

	total := 0.0
	count := 0

	for _, sample := range samples {
		if sample.Score != nil {
			total += sample.Score.Instant
			count++
		}
	}

	if count == 0 {
		return 0
	}

	return total / float64(count)
}

// makeDecision makes the failover decision
func (e *Engine) makeDecision(controller pkg.Controller) error {
	startTime := time.Now()

	// Get eligible members ranked by score
	eligible := e.getEligibleMembers()
	if len(eligible) == 0 {
		// Log decision with no eligible members
		e.logDecision("no_eligible", "no_eligible_members", nil, nil, "No eligible members found", 0.0, nil, nil, time.Since(startTime), false, "No eligible members")
		return fmt.Errorf("no eligible members")
	}

	// Sort by final score (descending)
	sort.Slice(eligible, func(i, j int) bool {
		scoreI := e.scores[eligible[i].Name]
		scoreJ := e.scores[eligible[j].Name]
		if scoreI == nil || scoreJ == nil {
			return false
		}
		return scoreI.Final > scoreJ.Final
	})

	top := eligible[0]

	// Check if we need to switch
	if e.shouldSwitch(top) {
		return e.performSwitch(controller, top)
	}

	// Log decision to stay with current member
	e.logDecision("no_switch", "score_check", e.current, top, "No switch needed - score delta insufficient",
		e.scores[top.Name].Final, e.scores[top.Name], nil, time.Since(startTime), true, "")

	return nil
}

// getEligibleMembers returns all eligible members
func (e *Engine) getEligibleMembers() []*pkg.Member {
	var eligible []*pkg.Member

	e.logger.Debug("Checking member eligibility", "total_members", len(e.members))

	for name, member := range e.members {
		if state := e.memberState[name]; state != nil {
			e.logger.Debug("Member status check", "member", name, "status", state.Status, "uptime", state.Uptime)
			if state.Status == pkg.StatusEligible {
				eligible = append(eligible, member)
				e.logger.Debug("Member is eligible", "member", name)
			}
		} else {
			e.logger.Debug("Member has no state", "member", name)
		}
	}

	e.logger.Debug("Eligible members found", "count", len(eligible))
	return eligible
}

// shouldSwitch determines if we should switch to the given member
func (e *Engine) shouldSwitch(target *pkg.Member) bool {
	if e.current == nil {
		e.logger.Info("Should switch: no current member, switching to target", "target", target.Name)
		return true // No current member, switch to target
	}

	if e.current.Name == target.Name {
		e.logger.Debug("Should switch: already using this member", "member", target.Name)
		return false // Already using this member
	}

	// Check switch margin
	currentScore := e.scores[e.current.Name]
	targetScore := e.scores[target.Name]

	if currentScore == nil || targetScore == nil {
		return false
	}

	scoreDelta := targetScore.Final - currentScore.Final
	if scoreDelta < float64(e.config.SwitchMargin) {
		return false // Not enough improvement
	}

	// Check cooldown
	if time.Since(e.lastSwitch) < time.Duration(e.config.CooldownS)*time.Second {
		return false // In cooldown period
	}

	// Check predictive conditions
	if e.config.Predictive && e.shouldPredictiveSwitch(target) {
		return true
	}

	// Check duration windows
	return e.checkDurationWindows(target)
}

// shouldPredictiveSwitch checks if we should switch due to predictive conditions
func (e *Engine) shouldPredictiveSwitch(target *pkg.Member) bool {
	now := time.Now()

	// Rate limit predictive decisions
	if now.Sub(e.lastPredictive) < e.predictiveRate {
		return false
	}

	if e.current == nil || e.predictiveEngine == nil {
		return false
	}

	// Get failure prediction for current member
	prediction, err := e.predictiveEngine.PredictFailure(e.current.Name)
	if err != nil {
		e.logger.Debug("Failed to get failure prediction", "member", e.current.Name, "error", err)
		return false
	}

	// Check if prediction indicates high failure risk
	if prediction.Risk > 0.7 && prediction.Confidence > 0.6 {
		e.logger.Info("Predictive failover triggered",
			"current", e.current.Name,
			"target", target.Name,
			"risk", prediction.Risk,
			"confidence", prediction.Confidence,
			"method", prediction.Method,
		)

		e.lastPredictive = now
		return true
	}

	// Check for specific predictive triggers based on member class
	if e.checkClassSpecificPredictiveTriggers(target) {
		e.logger.Info("Class-specific predictive failover triggered",
			"current", e.current.Name,
			"target", target.Name,
		)

		e.lastPredictive = now
		return true
	}

	// Check trend-based predictive triggers
	if e.checkTrendBasedPredictiveTriggers(target) {
		e.logger.Info("Trend-based predictive failover triggered",
			"current", e.current.Name,
			"target", target.Name,
		)

		e.lastPredictive = now
		return true
	}

	return false
}

// checkClassSpecificPredictiveTriggers checks for generic predictive conditions
func (e *Engine) checkClassSpecificPredictiveTriggers(target *pkg.Member) bool {
	if e.current == nil {
		return false
	}

	// Get recent samples for current member
	now := time.Now()
	samples, err := e.telemetry.GetSamples(e.current.Name, now.Add(-5*time.Minute))
	if err != nil || len(samples) < 3 {
		return false
	}

	// Check for generic predictive failover flag in latest sample
	latest := samples[len(samples)-1]
	if latest.Metrics.PredictiveFailover != nil && *latest.Metrics.PredictiveFailover {
		reason := "unknown"
		if latest.Metrics.PredictiveReason != nil {
			reason = *latest.Metrics.PredictiveReason
		}
		e.logger.Info("Generic predictive failover triggered",
			"interface_type", e.current.Class,
			"reason", reason,
		)
		return true
	}

	// Fallback to legacy class-specific triggers for backward compatibility
	switch e.current.Class {
	case pkg.ClassStarlink:
		return e.checkStarlinkPredictiveTriggers(samples)
	case pkg.ClassCellular:
		return e.checkCellularPredictiveTriggers(samples)
	case pkg.ClassWiFi:
		return e.checkWiFiPredictiveTriggers(samples)
	}

	return false
}

// checkStarlinkPredictiveTriggers checks Starlink-specific predictive conditions
func (e *Engine) checkStarlinkPredictiveTriggers(samples []*telem.Sample) bool {
	if len(samples) < 3 {
		return false
	}

	latest := samples[len(samples)-1]
	metrics := latest.Metrics

	// Check for predictive obstruction failover trigger
	if metrics.PredictiveFailover != nil && *metrics.PredictiveFailover {
		reason := "unknown"
		if metrics.PredictiveReason != nil {
			reason = *metrics.PredictiveReason
		}
		e.logger.Info("Predictive obstruction failover triggered",
			"reason", reason,
		)
		return true
	}

	// Check for rapid obstruction increase
	if metrics.ObstructionPct != nil && *metrics.ObstructionPct > 5.0 {
		// Check if obstruction is accelerating
		if len(samples) >= 3 {
			prev1 := samples[len(samples)-2]
			prev2 := samples[len(samples)-3]

			if prev1.Metrics.ObstructionPct != nil && prev2.Metrics.ObstructionPct != nil {
				current := *metrics.ObstructionPct
				prev1Val := *prev1.Metrics.ObstructionPct
				prev2Val := *prev2.Metrics.ObstructionPct

				// Check for acceleration in obstruction
				if current > prev1Val && prev1Val > prev2Val {
					acceleration := (current - prev1Val) - (prev1Val - prev2Val)
					if acceleration > 2.0 { // 2% acceleration threshold
						e.logger.Info("Starlink obstruction acceleration detected",
							"current", current,
							"prev1", prev1Val,
							"prev2", prev2Val,
							"acceleration", acceleration,
						)
						return true
					}
				}
			}
		}
	}

	// Check for thermal issues
	if metrics.ThermalThrottle != nil && *metrics.ThermalThrottle {
		e.logger.Info("Starlink thermal throttling detected")
		return true
	}

	// Check for pending software update reboot
	if metrics.SwupdateRebootReady != nil && *metrics.SwupdateRebootReady {
		e.logger.Info("Starlink software update reboot pending")
		return true
	}

	// Check for persistently low SNR
	if metrics.IsSNRPersistentlyLow != nil && *metrics.IsSNRPersistentlyLow {
		e.logger.Info("Starlink persistently low SNR detected")
		return true
	}

	// Enhanced Outages trend analysis (predictive - look for patterns)
	if len(samples) >= 5 {
		recentOutages := 0
		totalOutages := 0

		// Analyze outage pattern in recent samples
		for i := len(samples) - 5; i < len(samples); i++ {
			if samples[i].Metrics.Outages != nil && *samples[i].Metrics.Outages > 0 {
				recentOutages++
				totalOutages += *samples[i].Metrics.Outages
			}
		}

		// Trigger if 3+ samples in last 5 have outages (pattern detection)
		if recentOutages >= 3 {
			e.logger.Info("Starlink outage pattern detected - predictive failover triggered",
				"samples_with_outages", recentOutages,
				"total_outages", totalOutages,
				"window_size", 5)
			return true
		}

		// Trigger if total outages in recent window exceeds threshold
		if totalOutages >= 5 {
			e.logger.Info("Starlink high outage frequency detected - predictive failover triggered",
				"total_outages", totalOutages,
				"window_size", 5)
			return true
		}
	}

	// Enhanced Events-based predictive triggers (look for critical events)
	if metrics.Events != nil && len(*metrics.Events) > 0 {
		events := *metrics.Events

		for _, event := range events {
			// Immediate trigger for critical events
			if event.Severity == "critical" {
				e.logger.Info("Starlink critical event detected - predictive failover triggered",
					"event_type", event.Type,
					"message", event.Message)
				return true
			}

			// Trigger for specific high-impact event types
			switch event.Type {
			case "network_outage", "connectivity_loss":
				e.logger.Info("Starlink network connectivity event detected - predictive failover triggered",
					"event_type", event.Type,
					"severity", event.Severity,
					"message", event.Message)
				return true

			case "thermal_shutdown", "hardware_failure":
				e.logger.Info("Starlink hardware event detected - predictive failover triggered",
					"event_type", event.Type,
					"severity", event.Severity,
					"message", event.Message)
				return true

			case "obstruction_detected":
				// Only trigger for severe obstruction events
				if event.Severity == "warning" || event.Severity == "critical" {
					e.logger.Info("Starlink severe obstruction event detected - predictive failover triggered",
						"event_type", event.Type,
						"severity", event.Severity,
						"message", event.Message)
					return true
				}
			}
		}

		// Pattern detection: Multiple warning events in recent period
		warningCount := 0
		for _, event := range events {
			if event.Severity == "warning" {
				warningCount++
			}
		}

		if warningCount >= 3 {
			e.logger.Info("Starlink multiple warning events detected - predictive failover triggered",
				"warning_events", warningCount)
			return true
		}
	}

	return false
}

// checkCellularPredictiveTriggers checks cellular-specific predictive conditions
func (e *Engine) checkCellularPredictiveTriggers(samples []*telem.Sample) bool {
	if len(samples) < 3 {
		return false
	}

	latest := samples[len(samples)-1]
	metrics := latest.Metrics

	// Check for signal degradation
	if metrics.RSRP != nil && *metrics.RSRP < -110 {
		e.logger.Info("Cellular signal severely degraded", "rsrp", *metrics.RSRP)
		return true
	}

	// Check for roaming activation
	if metrics.Roaming != nil && *metrics.Roaming {
		e.logger.Info("Cellular roaming detected")
		return true
	}

	// Check for rapid RSRP degradation
	if len(samples) >= 3 && metrics.RSRP != nil {
		prev1 := samples[len(samples)-2]
		prev2 := samples[len(samples)-3]

		if prev1.Metrics.RSRP != nil && prev2.Metrics.RSRP != nil {
			current := float64(*metrics.RSRP)
			prev1Val := float64(*prev1.Metrics.RSRP)
			prev2Val := float64(*prev2.Metrics.RSRP)

			// Check for rapid degradation (RSRP getting more negative)
			if current < prev1Val-5 && prev1Val < prev2Val-5 {
				e.logger.Info("Cellular rapid signal degradation detected",
					"current", current,
					"prev1", prev1Val,
					"prev2", prev2Val,
				)
				return true
			}
		}
	}

	return false
}

// checkWiFiPredictiveTriggers checks WiFi-specific predictive conditions
func (e *Engine) checkWiFiPredictiveTriggers(samples []*telem.Sample) bool {
	if len(samples) < 3 {
		return false
	}

	latest := samples[len(samples)-1]
	metrics := latest.Metrics

	// Check for very poor signal strength
	if metrics.SignalStrength != nil && *metrics.SignalStrength < -80 {
		e.logger.Info("WiFi signal severely degraded", "signal", *metrics.SignalStrength)
		return true
	}

	// Check for very low SNR
	if metrics.SNR != nil && *metrics.SNR < 10 {
		e.logger.Info("WiFi SNR critically low", "snr", *metrics.SNR)
		return true
	}

	return false
}

// checkTrendBasedPredictiveTriggers checks for trend-based predictive conditions
func (e *Engine) checkTrendBasedPredictiveTriggers(target *pkg.Member) bool {
	if e.current == nil {
		return false
	}

	// Get trend analysis for current member
	trend, exists := e.trendAnalysis[e.current.Name]
	if !exists {
		return false
	}

	now := time.Now()
	// Only use recent trend data
	if now.Sub(trend.LastCalculated) > 2*time.Minute {
		return false
	}

	// Check for rapid latency increase
	if trend.LatencyTrend > 50.0 { // 50ms per minute increase
		e.logger.Info("Rapid latency increase detected",
			"member", e.current.Name,
			"trend", trend.LatencyTrend,
		)
		return true
	}

	// Check for rapid loss increase
	if trend.LossTrend > 2.0 { // 2% per minute increase
		e.logger.Info("Rapid loss increase detected",
			"member", e.current.Name,
			"trend", trend.LossTrend,
		)
		return true
	}

	// Check for rapid score degradation
	if trend.ScoreTrend < -10.0 { // 10 points per minute decrease
		e.logger.Info("Rapid score degradation detected",
			"member", e.current.Name,
			"trend", trend.ScoreTrend,
		)
		return true
	}

	return false
}

// updateTrendAnalysis updates trend analysis for a member
func (e *Engine) updateTrendAnalysis(memberName string, samples []*telem.Sample) {
	if len(samples) < 5 {
		return // Need at least 5 samples for trend analysis
	}

	now := time.Now()

	// Get or create trend analysis
	trend, exists := e.trendAnalysis[memberName]
	if !exists {
		trend = &TrendAnalysis{
			MemberName:     memberName,
			LastCalculated: now,
			Window:         time.Duration(e.config.HistoryWindowS) * time.Second,
		}
		e.trendAnalysis[memberName] = trend
	}

	// Only update if enough time has passed
	if now.Sub(trend.LastCalculated) < 30*time.Second {
		return
	}

	// Calculate trends using linear regression on recent samples
	recentSamples := samples
	if len(samples) > 20 {
		recentSamples = samples[len(samples)-20:] // Use last 20 samples
	}

	// Calculate latency trend
	trend.LatencyTrend = e.calculateTrendForMetric(recentSamples, func(s *telem.Sample) float64 {
		if s.Metrics.LatencyMS != nil {
			return *s.Metrics.LatencyMS
		}
		return 0
	})

	// Calculate loss trend
	trend.LossTrend = e.calculateTrendForMetric(recentSamples, func(s *telem.Sample) float64 {
		if s.Metrics.LossPercent != nil {
			return *s.Metrics.LossPercent
		}
		return 0
	})

	// Calculate score trend (if we have score data)
	if len(recentSamples) > 0 {
		// Get scores for recent samples
		scoreValues := make([]float64, 0, len(recentSamples))
		timestamps := make([]time.Time, 0, len(recentSamples))

		for _, sample := range recentSamples {
			// Calculate instant score for each sample
			member := e.members[memberName]
			if member != nil {
				instantScore := e.calculateInstantScore(member, sample.Metrics)
				scoreValues = append(scoreValues, instantScore)
				timestamps = append(timestamps, sample.Timestamp)
			}
		}

		if len(scoreValues) >= 3 {
			trend.ScoreTrend = e.calculateTrendFromValues(timestamps, scoreValues)
		}
	}

	// Calculate volatility (standard deviation of recent scores)
	if len(recentSamples) >= 3 {
		latencyValues := make([]float64, 0, len(recentSamples))
		for _, sample := range recentSamples {
			if sample.Metrics.LatencyMS != nil {
				latencyValues = append(latencyValues, *sample.Metrics.LatencyMS)
			}
		}
		if len(latencyValues) > 0 {
			trend.Volatility = e.calculateStandardDeviation(latencyValues)
		}
	}

	trend.LastCalculated = now
}

// calculateTrendForMetric calculates trend for a specific metric
func (e *Engine) calculateTrendForMetric(samples []*telem.Sample, extractor func(*telem.Sample) float64) float64 {
	if len(samples) < 3 {
		return 0.0
	}

	timestamps := make([]time.Time, len(samples))
	values := make([]float64, len(samples))

	for i, sample := range samples {
		timestamps[i] = sample.Timestamp
		values[i] = extractor(sample)
	}

	return e.calculateTrendFromValues(timestamps, values)
}

// calculateTrendFromValues calculates trend from timestamp/value pairs
func (e *Engine) calculateTrendFromValues(timestamps []time.Time, values []float64) float64 {
	if len(timestamps) != len(values) || len(values) < 2 {
		return 0.0
	}

	n := float64(len(values))

	// Convert timestamps to seconds since first timestamp
	baseTime := timestamps[0]
	x := make([]float64, len(timestamps))
	for i, ts := range timestamps {
		x[i] = ts.Sub(baseTime).Seconds()
	}

	// Calculate linear regression
	sumX := 0.0
	sumY := 0.0
	sumXY := 0.0
	sumX2 := 0.0

	for i := 0; i < len(x); i++ {
		sumX += x[i]
		sumY += values[i]
		sumXY += x[i] * values[i]
		sumX2 += x[i] * x[i]
	}

	// Calculate slope (trend per second)
	denominator := n*sumX2 - sumX*sumX
	if denominator == 0 {
		return 0.0
	}

	slope := (n*sumXY - sumX*sumY) / denominator

	// Convert to per-minute trend
	return slope * 60.0
}

// calculateStandardDeviation calculates standard deviation of values
func (e *Engine) calculateStandardDeviation(values []float64) float64 {
	if len(values) < 2 {
		return 0.0
	}

	// Calculate mean
	sum := 0.0
	for _, v := range values {
		sum += v
	}
	mean := sum / float64(len(values))

	// Calculate variance
	variance := 0.0
	for _, v := range values {
		diff := v - mean
		variance += diff * diff
	}
	variance /= float64(len(values))

	return math.Sqrt(variance)
}

// checkDurationWindows checks if duration windows are satisfied
func (e *Engine) checkDurationWindows(target *pkg.Member) bool {
	now := time.Now()

	// Check if target has been good long enough
	if goodStart, exists := e.goodWindows[target.Name]; exists {
		if now.Sub(goodStart) >= time.Duration(e.config.RestoreMinDurationS)*time.Second {
			return true
		}
	}

	// Check if current member has been bad long enough
	if e.current != nil {
		if badStart, exists := e.badWindows[e.current.Name]; exists {
			if now.Sub(badStart) >= time.Duration(e.config.FailMinDurationS)*time.Second {
				return true
			}
		}
	}

	return false
}

// performSwitch performs the actual switch
func (e *Engine) performSwitch(controller pkg.Controller, target *pkg.Member) error {
	startTime := time.Now()
	from := e.current

	// Perform the switch
	if err := controller.Switch(from, target); err != nil {
		// Log failed switch decision
		e.logDecision("failover", "score_check", from, target, fmt.Sprintf("Switch failed: %v", err),
			e.scores[target.Name].Final, e.scores[target.Name], nil, time.Since(startTime), false, err.Error())

		e.logger.Error("Failed to perform switch", "from", from, "to", target, "error", err)
		return err
	}

	// Update state after successful switch
	e.current = target
	e.lastSwitch = time.Now()

	// Log successful switch decision
	e.logDecision("failover", "score_check", from, target, "Switch completed successfully",
		e.scores[target.Name].Final, e.scores[target.Name], nil, time.Since(startTime), true, "")

	// Log the switch
	e.logger.LogSwitch(
		func() string {
			if from != nil {
				return from.Name
			} else {
				return "none"
			}
		}(),
		target.Name,
		"score",
		func() float64 {
			if score := e.scores[target.Name]; score != nil {
				return score.Final
			} else {
				return 0
			}
		}(),
		map[string]interface{}{
			"switch_margin": e.config.SwitchMargin,
		},
	)

	// Add event to telemetry
	event := &pkg.Event{
		ID:        fmt.Sprintf("switch_%d", time.Now().Unix()),
		Type:      pkg.EventFailover,
		Timestamp: time.Now(),
		From: func() string {
			if from != nil {
				return from.Name
			} else {
				return "none"
			}
		}(),
		To:     target.Name,
		Reason: "score",
		Data: map[string]interface{}{
			"switch_margin": e.config.SwitchMargin,
		},
	}

	if err := e.telemetry.AddEvent(event); err != nil {
		e.logger.Warn("Failed to add telemetry event", "error", err)
	}

	return nil
}

// AddMember adds a member to the decision engine
func (e *Engine) AddMember(member *pkg.Member) {
	e.mu.Lock()
	defer e.mu.Unlock()

	e.members[member.Name] = member
	e.logger.LogDiscovery(member.Name, string(member.Class), member.Iface, map[string]interface{}{
		"weight": member.Weight,
		"policy": member.Policy,
	})
}

// RemoveMember removes a member from the decision engine
func (e *Engine) RemoveMember(name string) {
	e.mu.Lock()
	defer e.mu.Unlock()

	delete(e.members, name)
	delete(e.memberState, name)
	delete(e.scores, name)
	delete(e.badWindows, name)
	delete(e.goodWindows, name)
	delete(e.cooldowns, name)
	delete(e.warmups, name)

	e.logger.LogEvent(pkg.EventMemberLost, name, map[string]interface{}{
		"reason": "removed",
	})
}

// GetMembers returns all members
func (e *Engine) GetMembers() []*pkg.Member {
	e.mu.RLock()
	defer e.mu.RUnlock()

	members := make([]*pkg.Member, 0, len(e.members))
	for _, member := range e.members {
		members = append(members, member)
	}

	return members
}

// GetCurrentMember returns the current active member
func (e *Engine) GetCurrentMember() *pkg.Member {
	e.mu.RLock()
	defer e.mu.RUnlock()

	return e.current
}

// GetScores returns all member scores
func (e *Engine) GetScores() map[string]*pkg.Score {
	e.mu.RLock()
	defer e.mu.RUnlock()

	scores := make(map[string]*pkg.Score)
	for k, v := range e.scores {
		scores[k] = v
	}

	return scores
}

// GetMemberState returns the state of a specific member
func (e *Engine) GetMemberState(memberName string) (*MemberState, error) {
	e.mu.RLock()
	defer e.mu.RUnlock()

	state, exists := e.memberState[memberName]
	if !exists {
		return nil, fmt.Errorf("member state not found: %s", memberName)
	}

	return state, nil
}

// Advanced Predictive Methods

// NewPatternDetector creates a new pattern detector
func NewPatternDetector() *PatternDetector {
	return &PatternDetector{
		patterns: make(map[string]*Pattern),
	}
}

// Note: NewMLPredictor is defined in predictive.go to avoid duplication

// logDecision logs a decision to the audit trail
func (e *Engine) logDecision(decisionType, trigger string, fromMember, toMember *pkg.Member, reasoning string, confidence float64, score *pkg.Score, metrics *pkg.Metrics, executionTime time.Duration, success bool, errorMsg string) {
	if e.DecisionLogger == nil {
		return
	}

	// Generate decision ID
	decisionID := fmt.Sprintf("decision_%d", time.Now().UnixNano())

	// Create decision record
	record := &audit.DecisionRecord{
		Timestamp:     time.Now(),
		DecisionID:    decisionID,
		DecisionType:  decisionType,
		Trigger:       trigger,
		FromMember:    fromMember,
		ToMember:      toMember,
		Reasoning:     reasoning,
		Confidence:    confidence,
		Metrics:       metrics,
		Score:         score,
		Context:       make(map[string]interface{}),
		ExecutionTime: executionTime,
		Success:       success,
		Error:         errorMsg,
	}

	// Add context information
	if e.current != nil {
		record.Context["current_member"] = e.current.Name
	}
	record.Context["switch_margin"] = e.config.SwitchMargin
	record.Context["cooldown_seconds"] = e.config.CooldownS

	// Log the decision
	if err := e.DecisionLogger.LogDecision(context.Background(), record); err != nil {
		e.logger.Error("Failed to log decision to audit trail", "error", err, "decision_id", decisionID)
	}

	// Perform root cause analysis if this is a failover decision
	if decisionType == "failover" && !success {
		e.performRootCauseAnalysis(record)
	}
}

// performRootCauseAnalysis performs root cause analysis on a decision
func (e *Engine) performRootCauseAnalysis(record *audit.DecisionRecord) {
	if e.RootCauseAnalyzer == nil {
		return
	}

	// Get related records for analysis
	relatedRecords := e.DecisionLogger.GetRecentDecisions(time.Now().Add(-1*time.Hour), 50)

	// Perform root cause analysis
	rootCause := e.RootCauseAnalyzer.AnalyzeRootCause(record, relatedRecords)
	if rootCause != nil {
		e.logger.Info("Root cause analysis completed",
			"decision_id", record.DecisionID,
			"category", rootCause.Category,
			"confidence", rootCause.Confidence,
			"impact", rootCause.Impact,
		)
	}
}

// GetLastFailoverTime returns the timestamp of the last failover
func (e *Engine) GetLastFailoverTime() time.Time {
	e.mu.RLock()
	defer e.mu.RUnlock()
	return e.lastSwitch
}

// calculateMetricsHash creates a hash of metrics for cache invalidation
func (e *Engine) calculateMetricsHash(metrics *pkg.Metrics) uint64 {
	var hash uint64

	if metrics.LatencyMS != nil {
		hash = hash*31 + uint64(*metrics.LatencyMS)
	}
	if metrics.LossPercent != nil {
		hash = hash*31 + uint64(*metrics.LossPercent*100)
	}
	if metrics.JitterMS != nil {
		hash = hash*31 + uint64(*metrics.JitterMS)
	}
	if metrics.ObstructionPct != nil {
		hash = hash*31 + uint64(*metrics.ObstructionPct*100)
	}
	if metrics.Outages != nil {
		hash = hash*31 + uint64(*metrics.Outages)
	}

	return hash
}

// cleanScoreCache removes expired cache entries
func (e *Engine) cleanScoreCache() {
	// Only clean every 5 minutes to avoid excessive overhead
	if time.Since(e.lastScoreClean) < 5*time.Minute {
		return
	}

	e.mu.Lock()
	defer e.mu.Unlock()

	now := time.Now()
	for key, entry := range e.scoreCache {
		if now.Sub(entry.Timestamp) > entry.TTL {
			delete(e.scoreCache, key)
		}
	}

	e.lastScoreClean = now
}
