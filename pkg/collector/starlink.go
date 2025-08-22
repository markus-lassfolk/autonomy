package collector

import (
	"context"
	"encoding/json"
	"fmt"
	"os/exec"
	"strconv"
	"time"

	"github.com/markus-lassfolk/autonomy/pkg"
	"github.com/markus-lassfolk/autonomy/pkg/logx"
	"github.com/markus-lassfolk/autonomy/pkg/obstruction"
	"github.com/markus-lassfolk/autonomy/pkg/starlink"
)

// Note: Mock data types removed from production code

// StarlinkCollector collects metrics from Starlink dish
type StarlinkCollector struct {
	*BaseCollector
	apiHost        string
	apiPort        int
	timeout        time.Duration
	grpcFirst      bool
	httpFirst      bool
	starlinkClient *starlink.Client

	// Predictive obstruction management
	obstructionPredictor *obstruction.ObstructionPredictor
	trendAnalyzer        *obstruction.TrendAnalyzer
	patternLearner       *obstruction.PatternLearner
	movementDetector     *obstruction.MovementDetector
	patternMatcher       *obstruction.PatternMatcher

	// Prediction state
	lastPredictiveCheck time.Time
	predictiveEnabled   bool
}

// StarlinkAPIResponse represents the comprehensive enhanced response from Starlink API
type StarlinkAPIResponse struct {
	Status struct {
		// Device Information
		DeviceInfo struct {
			ID                 string `json:"id"`
			HardwareVersion    string `json:"hardwareVersion"`
			SoftwareVersion    string `json:"softwareVersion"`
			CountryCode        string `json:"countryCode"`
			GenerationNumber   int32  `json:"generationNumber"`
			BootCount          int    `json:"bootCount"`
			SoftwarePartNumber string `json:"softwarePartNumber"`
			UTCOffsetS         int32  `json:"utcOffsetS"`
		} `json:"deviceInfo"`

		// Device State
		DeviceState struct {
			UptimeS uint64 `json:"uptimeS"`
		} `json:"deviceState"`

		// Enhanced Obstruction Statistics
		ObstructionStats struct {
			CurrentlyObstructed              bool      `json:"currentlyObstructed"`
			FractionObstructed               float64   `json:"fractionObstructed"`
			Last24hObstructedS               int       `json:"last24hObstructedS"`
			ValidS                           int       `json:"validS"`
			WedgeFractionObstructed          []float64 `json:"wedgeFractionObstructed"`
			WedgeAbsFractionObstructed       []float64 `json:"wedgeAbsFractionObstructed"`
			TimeObstructed                   float64   `json:"timeObstructed"`
			PatchesValid                     int       `json:"patchesValid"`
			AvgProlongedObstructionIntervalS float64   `json:"avgProlongedObstructionIntervalS"`
		} `json:"obstructionStats"`

		// Outage Information
		Outage struct {
			LastOutageS    int `json:"lastOutageS"`
			OutageCount    int `json:"outageCount"`
			OutageDuration int `json:"outageDuration"`
		} `json:"outage"`

		// Network Performance
		PopPingLatencyMs      float64 `json:"popPingLatencyMs"`
		DownlinkThroughputBps float64 `json:"downlinkThroughputBps"`
		UplinkThroughputBps   float64 `json:"uplinkThroughputBps"`
		PopPingDropRate       float64 `json:"popPingDropRate"`
		EthSpeedMbps          int32   `json:"ethSpeedMbps"`

		// Signal Quality
		SNR                  float64 `json:"snr"`
		SnrDb                float64 `json:"snrDb"`
		SecondsSinceLastSnr  int     `json:"secondsSinceLastSnr"`
		IsSnrAboveNoiseFloor bool    `json:"isSnrAboveNoiseFloor"`
		IsSnrPersistentlyLow bool    `json:"isSnrPersistentlyLow"`

		// Positioning
		BoresightAzimuthDeg   float64 `json:"boresightAzimuthDeg"`
		BoresightElevationDeg float64 `json:"boresightElevationDeg"`

		// Hardware Status
		HardwareSelfTest struct {
			Passed       bool     `json:"passed"`
			TestResults  []string `json:"testResults"`
			LastTestTime int64    `json:"lastTestTime"`
		} `json:"hardwareSelfTest"`

		// Thermal Status
		Thermal struct {
			Temperature     float64 `json:"temperature"`
			ThermalThrottle bool    `json:"thermalThrottle"`
			ThermalShutdown bool    `json:"thermalShutdown"`
		} `json:"thermal"`

		// Power Status
		Power struct {
			PowerDraw  float64 `json:"powerDraw"`
			Voltage    float64 `json:"voltage"`
			Current    float64 `json:"current"`
			PowerState string  `json:"powerState"`
		} `json:"power"`

		// GPS Status
		GpsStats struct {
			GpsValid bool `json:"gpsValid"`
			GpsSats  int  `json:"gpsSats"`
		} `json:"gpsStats"`
	} `json:"status"`
}

// NewStarlinkCollector creates a new Starlink collector
func NewStarlinkCollector(config map[string]interface{}) (*StarlinkCollector, error) {
	// Default configuration
	timeout := 30 * time.Second
	apiHost := "192.168.100.1"
	apiPort := 9200

	// Parse configuration
	if t, ok := config["timeout"].(int); ok {
		timeout = time.Duration(t) * time.Second
	}

	if h, ok := config["starlink_host"].(string); ok {
		apiHost = h
	}

	if p, ok := config["starlink_port"].(int); ok {
		apiPort = p
	}

	// Protocol preference configuration
	grpcFirst := true
	if g, ok := config["starlink_grpc_first"].(bool); ok {
		grpcFirst = g
	}

	httpFirst := false
	if h, ok := config["starlink_http_first"].(bool); ok {
		httpFirst = h
	}

	// Ping targets
	targets := []string{"8.8.8.8", "1.1.1.1"}
	if t, ok := config["targets"].([]string); ok {
		targets = t
	}

	// Initialize centralized Starlink client
	starlinkClient := starlink.DefaultClient(nil)

	logger := &logx.Logger{} // Default logger for now

	// Initialize predictive obstruction management components
	obstructionPredictor := obstruction.NewObstructionPredictor(logger, nil)
	trendAnalyzer := obstruction.NewTrendAnalyzer(logger, nil)
	patternLearner := obstruction.NewPatternLearner(logger, nil)
	movementDetector := obstruction.NewMovementDetector(logger)
	patternMatcher := obstruction.NewPatternMatcher(logger)

	collector := &StarlinkCollector{
		BaseCollector:        NewBaseCollector(timeout, targets, logger),
		apiHost:              apiHost,
		apiPort:              apiPort,
		timeout:              timeout,
		grpcFirst:            grpcFirst,
		httpFirst:            httpFirst,
		starlinkClient:       starlinkClient,
		obstructionPredictor: obstructionPredictor,
		trendAnalyzer:        trendAnalyzer,
		patternLearner:       patternLearner,
		movementDetector:     movementDetector,
		patternMatcher:       patternMatcher,
		predictiveEnabled:    true,
	}

	// Enable predictive analysis for Starlink interfaces
	predictiveEnabled := true
	if p, ok := config["predictive_enabled"].(bool); ok {
		predictiveEnabled = p
	}

	if predictiveEnabled {
		collector.BaseCollector.EnablePredictive("starlink")
	}
	collector.predictiveEnabled = predictiveEnabled

	return collector, nil
}

// SetLogger sets the logger for the Starlink client
func (sc *StarlinkCollector) SetLogger(logger *logx.Logger) {
	// Store logger for use in the collector
	// Note: The centralized starlink client doesn't have a SetLogger method
	// but we can use the logger in our collector methods
}

// Collect collects metrics from Starlink
func (sc *StarlinkCollector) Collect(ctx context.Context, member *pkg.Member) (*pkg.Metrics, error) {
	if err := sc.Validate(member); err != nil {
		return nil, err
	}

	// Start with common metrics
	metrics, err := sc.CollectCommonMetrics(ctx, member)
	if err != nil {
		return nil, err
	}

	// Collect Starlink-specific metrics
	starlinkMetrics, err := sc.collectStarlinkMetrics(ctx)
	if err != nil {
		// Real error - log but don't fail
		fmt.Printf("Warning: Failed to collect Starlink metrics: %v\n", err)
	} else {
		// Real data collected successfully
		if starlinkMetrics != nil {
			// Merge real Starlink metrics
			if starlinkMetrics.ObstructionPct != nil {
				metrics.ObstructionPct = starlinkMetrics.ObstructionPct
			}
			if starlinkMetrics.Outages != nil {
				metrics.Outages = starlinkMetrics.Outages
			}
			// Add all other available real metrics
			if starlinkMetrics.LatencyMS != nil && *starlinkMetrics.LatencyMS > 0 {
				metrics.LatencyMS = starlinkMetrics.LatencyMS
			}
			if starlinkMetrics.LossPercent != nil && *starlinkMetrics.LossPercent >= 0 {
				metrics.LossPercent = starlinkMetrics.LossPercent
			}
			if starlinkMetrics.SNR != nil {
				metrics.SNR = starlinkMetrics.SNR
			}
			// Real data collected successfully (no special indicator needed)
		}
	}

	// Perform predictive analysis using the generic system
	if err := sc.BaseCollector.PerformPredictiveAnalysis(ctx, metrics); err != nil {
		// Log error but don't fail the collection
		fmt.Printf("Warning: Predictive analysis failed: %v\n", err)
	}

	// Also perform Starlink-specific obstruction analysis if enabled
	if sc.predictiveEnabled && metrics != nil {
		if err := sc.performStarlinkSpecificAnalysis(ctx, metrics); err != nil {
			// Log error but don't fail the collection
			fmt.Printf("Warning: Starlink-specific analysis failed: %v\n", err)
		}
	}

	return metrics, nil
}

// collectStarlinkMetrics collects comprehensive metrics from Starlink API with enhanced diagnostics
func (sc *StarlinkCollector) collectStarlinkMetrics(ctx context.Context) (*pkg.Metrics, error) {
	// Use centralized Starlink client to get metrics
	metrics, err := sc.starlinkClient.GetMetrics(ctx)
	if err != nil {
		// Failed to get metrics from centralized client, falling back

		// Fallback to old method for backward compatibility
		apiResp, fallbackErr := sc.getStarlinkAPIData(ctx)
		if fallbackErr != nil {
			// No mock data fallback in production
			return nil, fmt.Errorf("failed to get Starlink API data: %w", fallbackErr)
		}

		// Use old extraction method as fallback
		detailedMetrics := sc.extractMetricsFromAPIResponseDetailed(apiResp)
		return detailedMetrics, nil
	}

	// Successfully got metrics from centralized client
	return metrics, nil
}

// getStarlinkAPIData retrieves data from Starlink API using our centralized client
func (sc *StarlinkCollector) getStarlinkAPIData(ctx context.Context) (*StarlinkAPIResponse, error) {
	// Try gRPC first if configured
	if sc.grpcFirst {
		if response, err := sc.tryStarlinkGRPC(ctx); err == nil {
			return response, nil
		}
	}

	// Try HTTP if configured
	if sc.httpFirst {
		if response, err := sc.tryStarlinkHTTP(ctx); err == nil {
			return response, nil
		}
	}

	// Fallback: try gRPC then HTTP
	if response, err := sc.tryStarlinkGRPC(ctx); err == nil {
		return response, nil
	}

	if response, err := sc.tryStarlinkHTTP(ctx); err == nil {
		return response, nil
	}

	// All methods failed
	return nil, fmt.Errorf("all Starlink API methods failed")
}

// tryStarlinkGRPC attempts to call the Starlink gRPC API using our centralized client
func (sc *StarlinkCollector) tryStarlinkGRPC(ctx context.Context) (*StarlinkAPIResponse, error) {
	// Use our centralized Starlink client with working dynamic protobuf
	if sc.starlinkClient == nil {
		return nil, fmt.Errorf("starlink client not initialized")
	}

	// Get status using centralized client
	response, err := sc.starlinkClient.CallMethod(ctx, starlink.MethodGetStatus)
	if err != nil {
		return nil, fmt.Errorf("centralized starlink client failed: %w", err)
	}

	// Parse the JSON response
	var grpcResponse map[string]interface{}
	if err := json.Unmarshal([]byte(response), &grpcResponse); err != nil {
		return nil, fmt.Errorf("failed to parse centralized client response: %w", err)
	}

	// Convert to our API structure
	return sc.convertGRPCResponseToAPI(grpcResponse)
}

// tryStarlinkHTTP attempts to call the Starlink HTTP API (fallback)
func (sc *StarlinkCollector) tryStarlinkHTTP(ctx context.Context) (*StarlinkAPIResponse, error) {
	// HTTP implementation would go here
	// For now, return error to indicate HTTP is not implemented
	return nil, fmt.Errorf("HTTP API not implemented")
}

// TestStarlinkMethod tests a specific Starlink API method using centralized client
func (sc *StarlinkCollector) TestStarlinkMethod(ctx context.Context, method string) (string, error) {
	fmt.Printf("Debug: Testing Starlink API method: %s\n", method)

	// Convert method string to APIMethod type
	var apiMethod starlink.APIMethod
	switch method {
	case "get_status":
		apiMethod = starlink.MethodGetStatus
	case "get_diagnostics":
		apiMethod = starlink.MethodGetDiagnostics
	case "get_location":
		apiMethod = starlink.MethodGetLocation
	case "get_history":
		apiMethod = starlink.MethodGetHistory
	case "get_device_info":
		apiMethod = starlink.MethodGetDeviceInfo
	default:
		return "", fmt.Errorf("unsupported method: %s", method)
	}

	// Use centralized client to test the method
	response, err := sc.starlinkClient.TestMethod(ctx, apiMethod)
	if err != nil {
		return "", fmt.Errorf("centralized client test failed for %s: %w", method, err)
	}

	fmt.Printf("Debug: %s method returned %d bytes via centralized client\n", method, len(response))
	return response, nil
}

// Note: Mock data functions removed from production code - use test files for mocks

// extractMetricsFromAPIResponseDetailed extracts detailed metrics from API response
func (sc *StarlinkCollector) extractMetricsFromAPIResponseDetailed(apiResp *StarlinkAPIResponse) *pkg.Metrics {
	// Extract comprehensive metrics from unified API response
	metrics := &pkg.Metrics{
		Timestamp: time.Now(),
	}

	// Basic obstruction data (enhanced with quality validation)
	obstructionPct := apiResp.Status.ObstructionStats.FractionObstructed * 100
	metrics.ObstructionPct = &obstructionPct

	// Enhanced obstruction data
	obstructionTime := apiResp.Status.ObstructionStats.TimeObstructed
	metrics.ObstructionTimePct = &obstructionTime

	// ValidS is already int, just assign it
	validS := int64(apiResp.Status.ObstructionStats.ValidS)
	metrics.ObstructionValidS = &validS

	avgProlonged := apiResp.Status.ObstructionStats.AvgProlongedObstructionIntervalS
	metrics.ObstructionAvgProlonged = &avgProlonged

	patchesValid := apiResp.Status.ObstructionStats.PatchesValid
	metrics.ObstructionPatchesValid = &patchesValid

	// Enhanced outage tracking
	outages := apiResp.Status.Outage.OutageCount
	if apiResp.Status.Outage.LastOutageS > 0 && apiResp.Status.Outage.LastOutageS < 300 { // Recent outage (5 minutes)
		outages++
	}
	metrics.Outages = &outages

	// Network performance metrics
	if apiResp.Status.PopPingLatencyMs > 0 {
		latency := apiResp.Status.PopPingLatencyMs
		metrics.LatencyMS = &latency
	}

	if apiResp.Status.PopPingDropRate >= 0 {
		lossPercent := apiResp.Status.PopPingDropRate * 100
		metrics.LossPercent = &lossPercent
	}

	// SNR data for signal quality assessment
	if apiResp.Status.SNR > 0 {
		snr := int(apiResp.Status.SNR)
		metrics.SNR = &snr
	} else if apiResp.Status.SnrDb > 0 {
		snr := int(apiResp.Status.SnrDb)
		metrics.SNR = &snr
	}

	// System uptime and boot count
	if apiResp.Status.DeviceState.UptimeS <= 9223372036854775807 {
		uptime := int64(apiResp.Status.DeviceState.UptimeS)
		metrics.UptimeS = &uptime
	}

	if apiResp.Status.DeviceInfo.BootCount > 0 {
		metrics.BootCount = &apiResp.Status.DeviceInfo.BootCount
	}

	// Enhanced Starlink Diagnostics - SNR quality indicators
	metrics.IsSNRAboveNoiseFloor = &apiResp.Status.IsSnrAboveNoiseFloor
	metrics.IsSNRPersistentlyLow = &apiResp.Status.IsSnrPersistentlyLow

	return metrics
}

// convertGRPCResponseToAPI converts gRPC response to our API structure
func (sc *StarlinkCollector) convertGRPCResponseToAPI(grpcResponse map[string]interface{}) (*StarlinkAPIResponse, error) {
	// Convert the gRPC response to our API structure
	response := &StarlinkAPIResponse{}

	// Extract dishGetStatus from the response
	if dishGetStatus, ok := grpcResponse["dishGetStatus"].(map[string]interface{}); ok {
		// Convert the dishGetStatus to our Status structure
		// This is a simplified conversion - in production you'd want more robust mapping

		// Device Info
		if deviceInfo, ok := dishGetStatus["deviceInfo"].(map[string]interface{}); ok {
			if id, ok := deviceInfo["id"].(string); ok {
				response.Status.DeviceInfo.ID = id
			}
			if hwVersion, ok := deviceInfo["hardwareVersion"].(string); ok {
				response.Status.DeviceInfo.HardwareVersion = hwVersion
			}
			if swVersion, ok := deviceInfo["softwareVersion"].(string); ok {
				response.Status.DeviceInfo.SoftwareVersion = swVersion
			}
		}

		// Device State
		if deviceState, ok := dishGetStatus["deviceState"].(map[string]interface{}); ok {
			if uptime, ok := deviceState["uptimeS"].(string); ok {
				// Convert string to uint64
				if uptimeInt, err := strconv.ParseUint(uptime, 10, 64); err == nil {
					response.Status.DeviceState.UptimeS = uptimeInt
				}
			}
		}

		// Obstruction Stats
		if obstructionStats, ok := dishGetStatus["obstructionStats"].(map[string]interface{}); ok {
			if fractionObstructed, ok := obstructionStats["fractionObstructed"].(float64); ok {
				response.Status.ObstructionStats.FractionObstructed = fractionObstructed
			}
			if validS, ok := obstructionStats["validS"].(float64); ok {
				response.Status.ObstructionStats.ValidS = int(validS)
			}
		}

		// Network Performance
		if latency, ok := dishGetStatus["popPingLatencyMs"].(float64); ok {
			response.Status.PopPingLatencyMs = latency
		}
		if dropRate, ok := dishGetStatus["popPingDropRate"].(float64); ok {
			response.Status.PopPingDropRate = dropRate
		}

		// GPS Stats
		if gpsStats, ok := dishGetStatus["gpsStats"].(map[string]interface{}); ok {
			if gpsValid, ok := gpsStats["gpsValid"].(bool); ok {
				response.Status.GpsStats.GpsValid = gpsValid
			}
			if gpsSats, ok := gpsStats["gpsSats"].(float64); ok {
				response.Status.GpsStats.GpsSats = int(gpsSats)
			}
		}
	}

	return response, nil
}

// IsAvailable checks if the Starlink API is accessible
func (sc *StarlinkCollector) IsAvailable(ctx context.Context) bool {
	// Simple connectivity test
	cmd := exec.CommandContext(ctx, "ping", "-c", "1", "-W", "2", sc.apiHost)
	return cmd.Run() == nil
}

// performStarlinkSpecificAnalysis performs Starlink-specific obstruction analysis
func (sc *StarlinkCollector) performStarlinkSpecificAnalysis(ctx context.Context, metrics *pkg.Metrics) error {
	// Add sample to obstruction predictor
	if err := sc.obstructionPredictor.AddSample(ctx, metrics); err != nil {
		return fmt.Errorf("failed to add sample to predictor: %w", err)
	}

	// Add data points to trend analyzer
	now := time.Now()
	dataQuality := sc.calculateDataQuality(metrics)

	if metrics.ObstructionPct != nil {
		sc.trendAnalyzer.AddObstructionPoint(now, *metrics.ObstructionPct/100.0, dataQuality)
	}
	if metrics.SNR != nil {
		sc.trendAnalyzer.AddSNRPoint(now, float64(*metrics.SNR), dataQuality)
	}
	if metrics.LatencyMS != nil {
		sc.trendAnalyzer.AddLatencyPoint(now, *metrics.LatencyMS, dataQuality)
	}

	// Add observation to pattern learner if we have obstruction
	if metrics.ObstructionPct != nil && *metrics.ObstructionPct > 5.0 {
		// Convert metrics to obstruction sample
		sample := obstruction.ObstructionSample{
			Timestamp:           now,
			CurrentlyObstructed: *metrics.ObstructionPct > 0, // Consider obstructed if > 0%
			FractionObstructed:  *metrics.ObstructionPct / 100.0,
			SNR:                 0,
			TimeObstructed:      0,
			ValidS:              0,
			PatchesValid:        0,
		}

		if metrics.SNR != nil {
			sample.SNR = float64(*metrics.SNR) // Convert int to float64
		}
		if metrics.ObstructionTimePct != nil {
			sample.TimeObstructed = *metrics.ObstructionTimePct
		}
		if metrics.ObstructionValidS != nil {
			sample.ValidS = int(*metrics.ObstructionValidS) // Convert int64 to int
		}
		if metrics.ObstructionPatchesValid != nil {
			sample.PatchesValid = *metrics.ObstructionPatchesValid
		}

		if err := sc.patternLearner.AddObservation(ctx, sample); err != nil {
			return fmt.Errorf("failed to add observation to pattern learner: %w", err)
		}
	}

	// Check if we should trigger predictive failover
	shouldFailover, reason, err := sc.obstructionPredictor.ShouldTriggerFailover(ctx)
	if err != nil {
		return fmt.Errorf("failed to check predictive failover: %w", err)
	}

	if shouldFailover {
		// Add predictive failover flag to metrics
		metrics.PredictiveFailover = &shouldFailover
		metrics.PredictiveReason = &reason

		fmt.Printf("PREDICTIVE FAILOVER TRIGGERED: %s\n", reason)
	}

	// Perform periodic cleanup and analysis
	if time.Since(sc.lastPredictiveCheck) > 30*time.Second {
		sc.lastPredictiveCheck = now

		// Cleanup expired pattern matches
		sc.patternMatcher.CleanupExpiredMatches(ctx)

		// Check if obstruction map should be refreshed due to movement
		if shouldRefresh, refreshReason := sc.movementDetector.ShouldRefreshObstructionMap(); shouldRefresh {
			fmt.Printf("OBSTRUCTION MAP REFRESH SUGGESTED: %s\n", refreshReason)
			// In a full implementation, this would trigger map refresh
		}
	}

	return nil
}

// calculateDataQuality calculates a quality score for the current metrics
func (sc *StarlinkCollector) calculateDataQuality(metrics *pkg.Metrics) float64 {
	quality := 0.0
	components := 0

	// GPS validity contributes to quality
	if metrics.GPSValid != nil && *metrics.GPSValid {
		quality += 0.3
	}
	components++

	// Valid seconds contributes to quality
	if metrics.ObstructionValidS != nil {
		validScore := float64(*metrics.ObstructionValidS) / 300.0 // Normalize to 5 minutes
		if validScore > 1.0 {
			validScore = 1.0
		}
		quality += validScore * 0.3
	}
	components++

	// Patches valid contributes to quality
	if metrics.ObstructionPatchesValid != nil {
		patchScore := float64(*metrics.ObstructionPatchesValid) / 100.0 // Normalize to 100 patches
		if patchScore > 1.0 {
			patchScore = 1.0
		}
		quality += patchScore * 0.2
	}
	components++

	// SNR availability contributes to quality
	if metrics.SNR != nil && *metrics.SNR > 0 {
		quality += 0.2
	}
	components++

	if components == 0 {
		return 0.5 // Default quality if no indicators available
	}

	return quality
}

// GetPredictiveStatus returns the current status of predictive components
func (sc *StarlinkCollector) GetPredictiveStatus() map[string]interface{} {
	if !sc.predictiveEnabled {
		return map[string]interface{}{
			"enabled": false,
		}
	}

	status := map[string]interface{}{
		"enabled":               true,
		"last_predictive_check": sc.lastPredictiveCheck,
	}

	if sc.obstructionPredictor != nil {
		status["obstruction_predictor"] = sc.obstructionPredictor.GetStatus()
	}

	if sc.trendAnalyzer != nil {
		status["trend_analyzer"] = sc.trendAnalyzer.GetStatus()
	}

	if sc.patternLearner != nil {
		status["pattern_learner"] = sc.patternLearner.GetStatus()
	}

	if sc.movementDetector != nil {
		status["movement_detector"] = sc.movementDetector.GetStatus()
	}

	if sc.patternMatcher != nil {
		status["pattern_matcher"] = sc.patternMatcher.GetStatus()
	}

	return status
}

// UpdateLocation updates the movement detector with GPS location
func (sc *StarlinkCollector) UpdateLocation(ctx context.Context, location *obstruction.LocationInfo) error {
	if !sc.predictiveEnabled || sc.movementDetector == nil {
		return nil
	}

	return sc.movementDetector.UpdateLocation(ctx, location)
}
