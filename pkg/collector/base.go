package collector

import (
	"context"
	"fmt"
	"math"
	"net"
	"time"

	"github.com/markus-lassfolk/autonomy/pkg"
	"github.com/markus-lassfolk/autonomy/pkg/adaptive"
	"github.com/markus-lassfolk/autonomy/pkg/logx"
	"github.com/markus-lassfolk/autonomy/pkg/predictive"
)

// BaseCollector provides common functionality for all collectors
type BaseCollector struct {
	timeout        time.Duration
	targets        []string
	latencyHistory map[string][]float64
	historySize    int
	logger         *logx.Logger

	// Predictive analysis
	predictor         *predictive.GenericPredictor
	predictiveEnabled bool

	// Adaptive sampling
	adaptiveSampler    *adaptive.AdaptiveSampler
	rateOptimizer      *adaptive.RateOptimizer
	connectionDetector *adaptive.ConnectionDetector

	// Performance optimization
	connectionPool map[string]*net.Conn
	lastCleanup    time.Time
	cache          map[string]*CachedResult
}

// CachedResult caches ping results to reduce network calls
type CachedResult struct {
	Latency   float64
	Loss      float64
	Timestamp time.Time
	TTL       time.Duration
}

// NewBaseCollector creates a new base collector
func NewBaseCollector(timeout time.Duration, targets []string, logger *logx.Logger) *BaseCollector {
	if len(targets) == 0 {
		targets = []string{"8.8.8.8", "1.1.1.1"}
	}
	if timeout == 0 {
		timeout = 5 * time.Second
	}
	if logger == nil {
		logger = &logx.Logger{} // Default logger
	}

	bc := &BaseCollector{
		timeout:           timeout,
		targets:           targets,
		latencyHistory:    make(map[string][]float64),
		historySize:       10,
		logger:            logger,
		predictiveEnabled: false, // Disabled by default, enabled per interface type
		connectionPool:    make(map[string]*net.Conn),
		cache:             make(map[string]*CachedResult),
		lastCleanup:       time.Now(),
	}

	// Initialize adaptive sampling components
	bc.initializeAdaptiveSampling()

	return bc
}

// initializeAdaptiveSampling initializes adaptive sampling components
func (bc *BaseCollector) initializeAdaptiveSampling() {
	// Initialize rate optimizer
	rateConfig := adaptive.DefaultRateOptimizerConfig()
	bc.rateOptimizer = adaptive.NewRateOptimizer(rateConfig, bc.logger)

	// Initialize connection detector
	detectorConfig := &adaptive.ConnectionDetectorConfig{
		Enabled:           true,
		DetectionInterval: 30 * time.Second,
	}
	bc.connectionDetector = adaptive.NewConnectionDetector(detectorConfig, bc.logger)

	// Initialize adaptive sampler
	samplerConfig := &adaptive.SamplingConfig{
		Enabled:        true,
		BaseInterval:   1 * time.Second,
		MaxInterval:    120 * time.Second,
		MinInterval:    1 * time.Second,
		AdaptationRate: 0.2,
	}
	bc.adaptiveSampler = adaptive.NewAdaptiveSampler(samplerConfig, bc.logger)

	bc.logger.Info("Adaptive sampling initialized")
}

// CollectCommonMetrics collects common latency and loss metrics
func (bc *BaseCollector) CollectCommonMetrics(ctx context.Context, member *pkg.Member) (*pkg.Metrics, error) {
	metrics := &pkg.Metrics{
		Timestamp: time.Now(),
	}

	// Collect latency and loss from multiple targets
	var totalLatency float64
	var totalLoss float64
	var validTargets int

	for _, target := range bc.targets {
		latency, loss, err := bc.pingTarget(ctx, target)
		if err != nil {
			continue
		}

		totalLatency += latency
		totalLoss += loss
		validTargets++
	}

	if validTargets == 0 {
		return nil, fmt.Errorf("no valid targets responded for member %s", member.Name)
	}

	// Calculate averages
	avgLatency := totalLatency / float64(validTargets)
	avgLoss := totalLoss / float64(validTargets)
	metrics.LatencyMS = &avgLatency
	metrics.LossPercent = &avgLoss

	// Calculate jitter based on latency history
	jitter := bc.calculateJitter(member.Name, avgLatency)
	metrics.JitterMS = &jitter

	return metrics, nil
}

// GetAdaptiveSamplingInterval returns the optimal sampling interval based on current conditions
func (bc *BaseCollector) GetAdaptiveSamplingInterval(ctx context.Context, member *pkg.Member) time.Duration {
	if bc.adaptiveSampler == nil {
		return 1 * time.Second // Default interval
	}

	// Get current connection type
	connectionType := bc.getConnectionTypeForMember(member)

	// Get data usage for the member
	dataUsage := bc.getDataUsageForMember(member)

	// Get optimal rate from rate optimizer
	optimalRate := bc.rateOptimizer.GetOptimalRate(ctx, connectionType, dataUsage)

	bc.logger.Debug("Adaptive sampling interval calculated",
		"member", member.Name,
		"connection_type", connectionType,
		"data_usage", dataUsage,
		"optimal_rate", optimalRate)

	return optimalRate
}

// getConnectionTypeForMember determines the connection type for a member
func (bc *BaseCollector) getConnectionTypeForMember(member *pkg.Member) adaptive.ConnectionType {
	switch member.Class {
	case pkg.ClassStarlink:
		return adaptive.ConnectionTypeStarlink
	case pkg.ClassCellular:
		return adaptive.ConnectionTypeCellular
	case pkg.ClassWiFi:
		return adaptive.ConnectionTypeWiFi
	case pkg.ClassLAN:
		return adaptive.ConnectionTypeLAN
	default:
		return adaptive.ConnectionTypeUnknown
	}
}

// getDataUsageForMember gets the data usage for a member (placeholder implementation)
func (bc *BaseCollector) getDataUsageForMember(member *pkg.Member) float64 {
	// TODO: Implement actual data usage tracking
	// For now, return a conservative estimate
	return 10.0 // 10 MB per hour
}

// UpdateAdaptiveSampling updates adaptive sampling with performance metrics
func (bc *BaseCollector) UpdateAdaptiveSampling(processingTime time.Duration, memoryUsage int64, cpuUsage float64, queueDepth int, dataUsage float64) {
	if bc.rateOptimizer == nil {
		return
	}

	// Create performance metric
	metric := &adaptive.PerformanceMetric{
		Timestamp:      time.Now(),
		ProcessingTime: processingTime,
		MemoryUsage:    memoryUsage,
		CPUUsage:       cpuUsage,
		QueueDepth:     queueDepth,
		DataUsage:      dataUsage,
	}

	// Add to rate optimizer
	if err := bc.rateOptimizer.AddPerformanceMetric(metric); err != nil {
		bc.logger.Warn("Failed to add performance metric to rate optimizer", "error", err)
	}

	bc.logger.Debug("Adaptive sampling updated with performance metrics",
		"processing_time", processingTime.Milliseconds(),
		"memory_usage", memoryUsage,
		"cpu_usage", cpuUsage,
		"queue_depth", queueDepth,
		"data_usage", dataUsage)
}

// pingTarget pings a single target and returns latency and loss
func (bc *BaseCollector) pingTarget(ctx context.Context, target string) (latency, loss float64, err error) {
	// Use TCP connect timing as fallback (ICMP might be blocked)
	start := time.Now()

	conn, err := net.DialTimeout("tcp", target+":80", bc.timeout)
	if err != nil {
		return 0, 100, err // 100% loss if can't connect
	}
	defer conn.Close()

	latency = float64(time.Since(start).Milliseconds())
	loss = 0 // TCP connect success = 0% loss

	return latency, loss, nil
}

// calculateJitter calculates jitter using a rolling window of latency samples
// It maintains a short history per member and returns the standard deviation
// of the collected latencies. If there are fewer than 2 samples, jitter is 0.
func (bc *BaseCollector) calculateJitter(memberName string, latency float64) float64 {
	history := append(bc.latencyHistory[memberName], latency)
	if len(history) > bc.historySize {
		history = history[len(history)-bc.historySize:]
	}
	bc.latencyHistory[memberName] = history

	if len(history) < 2 {
		return 0
	}

	var sum float64
	for _, v := range history {
		sum += v
	}
	mean := sum / float64(len(history))

	var variance float64
	for _, v := range history {
		diff := v - mean
		variance += diff * diff
	}
	variance /= float64(len(history))
	return math.Sqrt(variance)
}

// Validate validates a member for this collector
func (bc *BaseCollector) Validate(member *pkg.Member) error {
	if member == nil {
		return fmt.Errorf("member cannot be nil")
	}
	if member.Name == "" {
		return fmt.Errorf("member name cannot be empty")
	}
	if member.Iface == "" {
		return fmt.Errorf("member interface cannot be empty")
	}
	return nil
}

// CollectorFactory creates collectors based on member class
type CollectorFactory struct {
	config map[string]interface{}
}

// NewCollectorFactory creates a new collector factory
func NewCollectorFactory(config map[string]interface{}) *CollectorFactory {
	return &CollectorFactory{
		config: config,
	}
}

// CreateCollector creates a collector for the given member
func (cf *CollectorFactory) CreateCollector(member *pkg.Member) (pkg.Collector, error) {
	switch member.Class {
	case pkg.ClassStarlink:
		return NewStarlinkCollector(cf.config)
	case pkg.ClassCellular:
		return NewCellularCollector(cf.config)
	case pkg.ClassWiFi:
		return NewWiFiCollector(cf.config)
	case pkg.ClassLAN:
		return NewLANCollector(cf.config)
	case pkg.ClassOther:
		return NewGenericCollector(cf.config)
	default:
		return NewGenericCollector(cf.config)
	}
}

// GenericCollector is a fallback collector for unknown member types
type GenericCollector struct {
	*BaseCollector
}

// NewGenericCollector creates a new generic collector
func NewGenericCollector(config map[string]interface{}) (*GenericCollector, error) {
	timeout := 5 * time.Second
	if t, ok := config["timeout"].(time.Duration); ok {
		timeout = t
	}

	targets := []string{"8.8.8.8", "1.1.1.1"}
	if t, ok := config["targets"].([]string); ok {
		targets = t
	}

	logger := &logx.Logger{} // Default logger for now
	return &GenericCollector{
		BaseCollector: NewBaseCollector(timeout, targets, logger),
	}, nil
}

// Collect collects metrics for a generic member
func (gc *GenericCollector) Collect(ctx context.Context, member *pkg.Member) (*pkg.Metrics, error) {
	if err := gc.Validate(member); err != nil {
		return nil, err
	}

	return gc.CollectCommonMetrics(ctx, member)
}

// Validate validates a member for the generic collector
func (gc *GenericCollector) Validate(member *pkg.Member) error {
	return gc.BaseCollector.Validate(member)
}

// LANCollector collects metrics for LAN interfaces
type LANCollector struct {
	*BaseCollector
}

// NewLANCollector creates a new LAN collector
func NewLANCollector(config map[string]interface{}) (*LANCollector, error) {
	timeout := 3 * time.Second // LAN should be faster
	if t, ok := config["timeout"].(time.Duration); ok {
		timeout = t
	}

	targets := []string{"8.8.8.8", "1.1.1.1"}
	if t, ok := config["targets"].([]string); ok {
		targets = t
	}

	logger := &logx.Logger{} // Default logger for now
	return &LANCollector{
		BaseCollector: NewBaseCollector(timeout, targets, logger),
	}, nil
}

// Collect collects metrics for a LAN member
func (lc *LANCollector) Collect(ctx context.Context, member *pkg.Member) (*pkg.Metrics, error) {
	if err := lc.Validate(member); err != nil {
		return nil, err
	}

	return lc.CollectCommonMetrics(ctx, member)
}

// Validate validates a member for the LAN collector
func (lc *LANCollector) Validate(member *pkg.Member) error {
	return lc.BaseCollector.Validate(member)
}

// EnablePredictive enables predictive analysis for this collector
func (bc *BaseCollector) EnablePredictive(interfaceType string) {
	if bc.predictor == nil {
		bc.predictor = predictive.NewGenericPredictor(bc.logger, interfaceType, nil)
		bc.predictiveEnabled = true
		bc.logger.Info("Enabled predictive analysis", "interface_type", interfaceType)
	}
}

// DisablePredictive disables predictive analysis
func (bc *BaseCollector) DisablePredictive() {
	bc.predictiveEnabled = false
	bc.predictor = nil
	bc.logger.Info("Disabled predictive analysis")
}

// AddPredictiveSample adds a sample to the predictive system
func (bc *BaseCollector) AddPredictiveSample(ctx context.Context, metrics *pkg.Metrics) error {
	if !bc.predictiveEnabled || bc.predictor == nil {
		return nil // Predictive analysis disabled
	}

	// Calculate data quality based on available metrics
	quality := bc.calculateDataQuality(metrics)

	return bc.predictor.AddSample(ctx, metrics, quality)
}

// CheckPredictiveFailover checks if predictive failover should be triggered
func (bc *BaseCollector) CheckPredictiveFailover(ctx context.Context) (bool, string, error) {
	if !bc.predictiveEnabled || bc.predictor == nil {
		return false, "predictive analysis disabled", nil
	}

	return bc.predictor.ShouldTriggerFailover(ctx)
}

// GetPredictiveStatus returns the status of the predictive system
func (bc *BaseCollector) GetPredictiveStatus() map[string]interface{} {
	if !bc.predictiveEnabled || bc.predictor == nil {
		return map[string]interface{}{
			"enabled": false,
		}
	}

	status := bc.predictor.GetStatus()
	status["enabled"] = true
	return status
}

// calculateDataQuality calculates a quality score for the metrics
func (bc *BaseCollector) calculateDataQuality(metrics *pkg.Metrics) float64 {
	quality := 0.0
	components := 0

	// Latency availability and reasonableness
	if metrics.LatencyMS != nil {
		if *metrics.LatencyMS > 0 && *metrics.LatencyMS < 10000 { // Reasonable latency range
			quality += 0.3
		}
	}
	components++

	// Loss percentage availability and reasonableness
	if metrics.LossPercent != nil {
		if *metrics.LossPercent >= 0 && *metrics.LossPercent <= 100 { // Valid loss range
			quality += 0.3
		}
	}
	components++

	// Timestamp recency (fresher data is higher quality)
	age := time.Since(metrics.Timestamp)
	if age < 10*time.Second {
		quality += 0.2
	} else if age < 60*time.Second {
		quality += 0.1
	}
	components++

	// Interface-specific quality indicators
	interfaceQuality := 0.0

	// GPS data quality
	if metrics.GPSValid != nil && *metrics.GPSValid {
		interfaceQuality += 0.1
	}

	// Signal strength availability (for wireless interfaces)
	if metrics.SignalStrength != nil || metrics.RSRP != nil || metrics.SNR != nil {
		interfaceQuality += 0.1
	}

	quality += interfaceQuality
	components++

	if components == 0 {
		return 0.5 // Default quality if no indicators available
	}

	return math.Min(quality, 1.0)
}

// PerformPredictiveAnalysis performs predictive analysis and updates metrics
func (bc *BaseCollector) PerformPredictiveAnalysis(ctx context.Context, metrics *pkg.Metrics) error {
	if !bc.predictiveEnabled {
		return nil
	}

	// Add sample to predictive system
	if err := bc.AddPredictiveSample(ctx, metrics); err != nil {
		bc.logger.Warn("Failed to add predictive sample", "error", err)
		return err
	}

	// Check if predictive failover should be triggered
	shouldFailover, reason, err := bc.CheckPredictiveFailover(ctx)
	if err != nil {
		bc.logger.Warn("Failed to check predictive failover", "error", err)
		return err
	}

	if shouldFailover {
		// Add predictive failover flags to metrics
		metrics.PredictiveFailover = &shouldFailover
		metrics.PredictiveReason = &reason

		bc.logger.Info("Predictive failover triggered",
			"interface_type", bc.predictor.GetStatus()["interface_type"],
			"reason", reason)
	}

	return nil
}
