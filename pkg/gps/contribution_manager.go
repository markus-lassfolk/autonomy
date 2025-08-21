package gps

import (
	"bytes"
	"context"
	"encoding/csv"
	"encoding/json"
	"fmt"
	"io"
	"math"
	"net/http"
	"strconv"
	"strings"
	"sync"
	"time"

	"github.com/markus-lassfolk/autonomy/pkg/logx"
)

// ContributionManager manages data contributions to OpenCellID
type ContributionManager struct {
	logger          *logx.Logger
	config          *OpenCellIDGPSConfig
	httpClient      *http.Client
	queue           *ContributionQueue
	lastFlush       time.Time
	stats           *ContributionStats
	dualRateLimiter interface{} // Rate limiter for submissions (can be *DualRateLimiter or *RatioBasedRateLimiter)
	mu              sync.RWMutex
}

// CellObservation represents a complete cell tower observation for contribution
type CellObservation struct {
	GPS         GPSObservation     `json:"gps"`
	ServingCell ServingCellInfo    `json:"serving_cell"`
	Neighbors   []NeighborCellInfo `json:"neighbors"`
	Metrics     *CellularMetrics   `json:"metrics,omitempty"`
	ObservedAt  time.Time          `json:"observed_at"`
}

// GPSObservation represents GPS data for contribution
type GPSObservation struct {
	Latitude  float64   `json:"latitude"`  // 6 decimal places for sub-meter accuracy
	Longitude float64   `json:"longitude"` // 6 decimal places for sub-meter accuracy
	Accuracy  float64   `json:"accuracy"`  // Accuracy in meters
	Speed     float64   `json:"speed"`     // Speed in m/s
	Heading   float64   `json:"heading"`   // Heading in degrees
	Timestamp time.Time `json:"timestamp"` // UTC timestamp with 'Z' suffix
}

// ContributionQueue manages pending contributions with deduplication
type ContributionQueue struct {
	observations    []CellObservation
	lastSubmissions map[string]ContributionHistory // Key: cellKey, Value: last submission info
	mu              sync.RWMutex
}

// ContributionHistory tracks when we last submitted data for a cell
type ContributionHistory struct {
	LastSubmission  time.Time      `json:"last_submission"`
	LastLocation    GPSObservation `json:"last_location"`
	LastRSRP        *int           `json:"last_rsrp,omitempty"`
	SubmissionCount int            `json:"submission_count"`
	CellKey         string         `json:"cell_key"`       // For tracking per-cell intervals
	MinInterval     time.Duration  `json:"min_interval"`   // Minimum time between submissions (default: 15min)
	RSRPThreshold   int            `json:"rsrp_threshold"` // RSRP change threshold in dB (default: 6)
}

// ContributionStats tracks contribution statistics
type ContributionStats struct {
	TotalObservations     int       `json:"total_observations"`
	QueuedObservations    int       `json:"queued_observations"`
	SuccessfulUploads     int       `json:"successful_uploads"`
	FailedUploads         int       `json:"failed_uploads"`
	LastUploadTime        time.Time `json:"last_upload_time"`
	LastUploadSize        int       `json:"last_upload_size"`
	TotalCellsContributed int       `json:"total_cells_contributed"`
	UploadErrors          []string  `json:"upload_errors"`
}

// OpenCellIDMeasurement represents a measurement in OpenCellID format
type OpenCellIDMeasurement struct {
	Lon        float64 `json:"lon"`
	Lat        float64 `json:"lat"`
	MCC        int     `json:"mcc"`
	MNC        int     `json:"mnc"`
	LAC        int     `json:"lac,omitempty"`
	CellID     int     `json:"cellid,omitempty"`
	TAC        int     `json:"tac,omitempty"`
	ECI        int     `json:"eci,omitempty"`
	Signal     int     `json:"signal,omitempty"`
	MeasuredAt int64   `json:"measured_at"` // Unix timestamp in milliseconds
	Rating     float64 `json:"rating,omitempty"`
	Speed      float64 `json:"speed,omitempty"`
	Direction  float64 `json:"direction,omitempty"`
	Act        string  `json:"act"`           // Radio technology
	TA         int     `json:"ta,omitempty"`  // Timing Advance
	PCI        int     `json:"pci,omitempty"` // Physical Cell ID
	PSC        int     `json:"psc,omitempty"` // Primary Scrambling Code
	// CDMA fields
	SID int `json:"sid,omitempty"` // System ID
	NID int `json:"nid,omitempty"` // Network ID
	BID int `json:"bid,omitempty"` // Base ID
}

// OpenCellIDUploadRequest represents the JSON upload request format
type OpenCellIDUploadRequest struct {
	Measurements []OpenCellIDMeasurement `json:"measurements"`
}

// NewContributionManager creates a new contribution manager
func NewContributionManager(config *OpenCellIDGPSConfig, logger *logx.Logger, ratioRateLimiter *RatioBasedRateLimiter) *ContributionManager {
	return &ContributionManager{
		logger: logger,
		config: config,
		httpClient: &http.Client{
			Timeout: 60 * time.Second, // Longer timeout for uploads
		},
		queue: &ContributionQueue{
			observations:    make([]CellObservation, 0),
			lastSubmissions: make(map[string]ContributionHistory),
		},
		stats: &ContributionStats{
			UploadErrors: make([]string, 0),
		},
		dualRateLimiter: ratioRateLimiter, // Reusing field name for compatibility
	}
}

// SubmissionDecision represents the result of evaluating whether to submit a cell observation
type SubmissionDecision struct {
	ShouldSubmit bool     `json:"should_submit"`
	Reason       string   `json:"reason"`
	CellKey      string   `json:"cell_key"`
	Triggers     []string `json:"triggers,omitempty"` // What triggered the submission
}

// ShouldSubmitObservation determines if a cell observation should be submitted based on enhanced criteria
func (cm *ContributionManager) ShouldSubmitObservation(observation CellObservation) SubmissionDecision {
	// Check GPS accuracy first (must be ≤20m)
	if observation.GPS.Accuracy > cm.config.MinGPSAccuracyM {
		return SubmissionDecision{
			ShouldSubmit: false,
			Reason:       fmt.Sprintf("gps_accuracy_insufficient: %.1fm > %.1fm", observation.GPS.Accuracy, cm.config.MinGPSAccuracyM),
		}
	}

	// Generate cell key for tracking
	cellKey := fmt.Sprintf("%s-%s-%s-%s-%s",
		observation.ServingCell.MCC,
		observation.ServingCell.MNC,
		observation.ServingCell.TAC,
		observation.ServingCell.CellID,
		observation.ServingCell.Technology,
	)

	cm.queue.mu.RLock()
	history, exists := cm.queue.lastSubmissions[cellKey]
	cm.queue.mu.RUnlock()

	decision := SubmissionDecision{
		CellKey:  cellKey,
		Triggers: make([]string, 0),
	}

	// If cell not in cache, always submit (new cell trigger)
	if !exists {
		decision.ShouldSubmit = true
		decision.Reason = "new_cell_observed"
		decision.Triggers = append(decision.Triggers, "new_cell")
		return decision
	}

	// Check minimum interval (default: 15 minutes)
	minInterval := 15 * time.Minute
	if history.MinInterval > 0 {
		minInterval = history.MinInterval
	}

	timeSinceLastSubmission := time.Since(history.LastSubmission)
	if timeSinceLastSubmission < minInterval {
		return SubmissionDecision{
			ShouldSubmit: false,
			Reason:       fmt.Sprintf("min_interval_not_met: %v < %v", timeSinceLastSubmission, minInterval),
			CellKey:      cellKey,
		}
	}

	// Check movement threshold (≥250m)
	if cm.config.MovementThresholdM > 0 {
		distance := cm.calculateDistance(history.LastLocation, observation.GPS)
		if distance >= cm.config.MovementThresholdM {
			decision.ShouldSubmit = true
			decision.Triggers = append(decision.Triggers, fmt.Sprintf("movement_%.0fm", distance))
		}
	}

	// Check RSRP change threshold (≥6dB)
	rsrpThreshold := 6 // Default 6dB
	if history.RSRPThreshold > 0 {
		rsrpThreshold = history.RSRPThreshold
	}

	if history.LastRSRP != nil && observation.ServingCell.RSRP != 0 {
		rsrpChange := int(math.Abs(float64(observation.ServingCell.RSRP - *history.LastRSRP)))
		if rsrpChange >= rsrpThreshold {
			decision.ShouldSubmit = true
			decision.Triggers = append(decision.Triggers, fmt.Sprintf("rsrp_change_%ddB", rsrpChange))
		}
	}

	// Set reason based on triggers
	if decision.ShouldSubmit {
		if len(decision.Triggers) > 0 {
			decision.Reason = fmt.Sprintf("triggered_by: %v", decision.Triggers)
		} else {
			decision.Reason = "criteria_met"
		}
	} else {
		decision.Reason = "no_significant_change"
	}

	return decision
}

// QueueObservation queues a cell observation for contribution
func (cm *ContributionManager) QueueObservation(obs *CellObservation) {
	if !cm.config.ContributeData || cm.config.APIKey == "" {
		return
	}

	// Validate GPS accuracy
	if obs.GPS.Accuracy > cm.config.MinGPSAccuracyM {
		cm.logger.LogDebugVerbose("contribution_skipped_accuracy", map[string]interface{}{
			"gps_accuracy": obs.GPS.Accuracy,
			"min_required": cm.config.MinGPSAccuracyM,
		})
		return
	}

	cm.mu.Lock()
	defer cm.mu.Unlock()

	// Check if we should contribute this observation
	if !cm.shouldContribute(obs) {
		return
	}

	// Add to queue
	cm.queue.mu.Lock()
	cm.queue.observations = append(cm.queue.observations, *obs)
	cm.queue.mu.Unlock()

	// Update statistics
	cm.stats.TotalObservations++
	cm.stats.QueuedObservations++

	cm.logger.LogDebugVerbose("contribution_queued", map[string]interface{}{
		"cell_id":      obs.ServingCell.CellID,
		"mcc":          obs.ServingCell.MCC,
		"mnc":          obs.ServingCell.MNC,
		"gps_accuracy": obs.GPS.Accuracy,
		"queue_size":   len(cm.queue.observations),
	})

	// Check if it's time to flush
	if cm.shouldFlush() {
		go func() {
			if err := cm.FlushPendingContributions(context.Background()); err != nil {
				cm.logger.Warn("Failed to flush pending contributions", "error", err)
			}
		}()
	}
}

// shouldContribute determines if an observation should be contributed
func (cm *ContributionManager) shouldContribute(obs *CellObservation) bool {
	cellKey := cm.getCellKey(&obs.ServingCell)

	cm.queue.mu.RLock()
	history, exists := cm.queue.lastSubmissions[cellKey]
	cm.queue.mu.RUnlock()

	// Always contribute if we've never seen this cell
	if !exists {
		cm.logger.LogDebugVerbose("contribution_new_cell", map[string]interface{}{
			"cell_key": cellKey,
		})
		return true
	}

	// Check movement threshold
	distance := cm.calculateDistance(history.LastLocation, obs.GPS)
	if distance >= cm.config.MovementThresholdM {
		cm.logger.LogDebugVerbose("contribution_movement", map[string]interface{}{
			"cell_key":  cellKey,
			"distance":  distance,
			"threshold": cm.config.MovementThresholdM,
		})
		return true
	}

	// Check RSRP change threshold
	if cm.hasSignificantRSRPChange(history, obs) {
		cm.logger.LogDebugVerbose("contribution_rsrp_change", map[string]interface{}{
			"cell_key": cellKey,
		})
		return true
	}

	// Check time-based contribution (avoid too frequent submissions)
	minInterval := time.Duration(cm.config.ContributionIntervalMin) * time.Minute
	if time.Since(history.LastSubmission) < minInterval {
		cm.logger.LogDebugVerbose("contribution_too_frequent", map[string]interface{}{
			"cell_key":        cellKey,
			"last_submission": history.LastSubmission,
			"min_interval":    minInterval,
		})
		return false
	}

	return false
}

// hasSignificantRSRPChange checks if there's a significant RSRP change
func (cm *ContributionManager) hasSignificantRSRPChange(history ContributionHistory, obs *CellObservation) bool {
	if history.LastRSRP == nil || obs.Metrics == nil || obs.Metrics.RSRP == nil {
		return false
	}

	rsrpChange := math.Abs(float64(*obs.Metrics.RSRP - *history.LastRSRP))
	return rsrpChange >= cm.config.RSRPChangeThresholdDB
}

// shouldFlush determines if it's time to flush the queue
func (cm *ContributionManager) shouldFlush() bool {
	flushInterval := time.Duration(cm.config.ContributionIntervalMin) * time.Minute
	return time.Since(cm.lastFlush) >= flushInterval && len(cm.queue.observations) > 0
}

// FlushPendingContributions uploads all pending contributions to OpenCellID
func (cm *ContributionManager) FlushPendingContributions(ctx context.Context) error {
	cm.mu.Lock()
	defer cm.mu.Unlock()

	// Check submission rate limit (supports both DualRateLimiter and RatioBasedRateLimiter)
	if cm.dualRateLimiter != nil {
		var allowed bool
		switch limiter := cm.dualRateLimiter.(type) {
		case *RatioBasedRateLimiter:
			allowed = limiter.TrySubmission()
		case *DualRateLimiter:
			allowed = limiter.TrySubmission()
		default:
			cm.logger.Warn("unknown_rate_limiter_type", "type", fmt.Sprintf("%T", limiter))
			allowed = true // Default to allowing if unknown type
		}

		if !allowed {
			cm.logger.Debug("contribution_rate_limited", "reason", "submission_quota_exceeded")
			return fmt.Errorf("submission rate limit exceeded")
		}
	}

	cm.queue.mu.Lock()
	observations := make([]CellObservation, len(cm.queue.observations))
	copy(observations, cm.queue.observations)
	cm.queue.observations = cm.queue.observations[:0] // Clear queue
	cm.queue.mu.Unlock()

	if len(observations) == 0 {
		return nil
	}

	cm.logger.Info("contribution_flush_start",
		"observation_count", len(observations),
		"queue_cleared", true,
	)

	// Convert observations to OpenCellID format
	measurements := cm.convertToOpenCellIDFormat(observations)

	// Try JSON upload first (preferred)
	err := cm.uploadJSON(ctx, measurements)
	if err != nil {
		cm.logger.Warn("contribution_json_failed",
			"error", err.Error(),
			"fallback", "csv_upload",
		)

		// Fallback to CSV upload
		err = cm.uploadCSV(ctx, measurements)
		if err != nil {
			cm.stats.FailedUploads++
			cm.addUploadError(fmt.Sprintf("Both JSON and CSV upload failed: %v", err))
			return fmt.Errorf("upload failed: %w", err)
		}
	}

	// Update submission history
	cm.updateSubmissionHistory(observations)

	// Update statistics
	cm.stats.SuccessfulUploads++
	cm.stats.LastUploadTime = time.Now()
	cm.stats.LastUploadSize = len(observations)
	cm.stats.QueuedObservations -= len(observations)
	cm.stats.TotalCellsContributed += cm.countUniqueCells(observations)
	cm.lastFlush = time.Now()

	cm.logger.Info("contribution_flush_success",
		"uploaded_observations", len(observations),
		"unique_cells", cm.countUniqueCells(observations),
		"upload_method", "json_or_csv",
	)

	return nil
}

// uploadJSON uploads measurements using JSON format
func (cm *ContributionManager) uploadJSON(ctx context.Context, measurements []OpenCellIDMeasurement) error {
	request := OpenCellIDUploadRequest{
		Measurements: measurements,
	}

	jsonData, err := json.Marshal(request)
	if err != nil {
		return fmt.Errorf("failed to marshal JSON: %w", err)
	}

	// Create multipart form data
	var body bytes.Buffer
	body.WriteString("key=" + cm.config.APIKey + "&")
	body.WriteString("datafile=")
	body.Write(jsonData)

	req, err := http.NewRequestWithContext(ctx, "POST",
		"https://opencellid.org/measure/uploadJson", &body)
	if err != nil {
		return fmt.Errorf("failed to create request: %w", err)
	}

	req.Header.Set("Content-Type", "application/x-www-form-urlencoded")

	resp, err := cm.httpClient.Do(req)
	if err != nil {
		return fmt.Errorf("HTTP request failed: %w", err)
	}
	defer resp.Body.Close()

	if resp.StatusCode != 200 {
		body, _ := io.ReadAll(resp.Body)
		return fmt.Errorf("HTTP error %d: %s", resp.StatusCode, string(body))
	}

	// Check response
	var result map[string]interface{}
	if err := json.NewDecoder(resp.Body).Decode(&result); err != nil {
		return fmt.Errorf("failed to parse response: %w", err)
	}

	if code, ok := result["code"].(float64); ok && code != 0 {
		return fmt.Errorf("API error code: %.0f", code)
	}

	return nil
}

// uploadCSV uploads measurements using CSV format
func (cm *ContributionManager) uploadCSV(ctx context.Context, measurements []OpenCellIDMeasurement) error {
	// Create CSV data
	var csvBuffer bytes.Buffer
	writer := csv.NewWriter(&csvBuffer)

	// Write header
	header := []string{
		"mcc", "mnc", "lac", "cellid", "lon", "lat", "signal",
		"measured_at", "rating", "speed", "direction", "act", "ta", "psc", "tac", "pci", "sid", "nid", "bid",
	}
	if err := writer.Write(header); err != nil {
		return fmt.Errorf("failed to write CSV header: %w", err)
	}

	// Write measurements
	for _, m := range measurements {
		record := []string{
			strconv.Itoa(m.MCC),
			strconv.Itoa(m.MNC),
			strconv.Itoa(m.LAC),
			strconv.Itoa(m.CellID),
			fmt.Sprintf("%.6f", m.Lon),
			fmt.Sprintf("%.6f", m.Lat),
			strconv.Itoa(m.Signal),
			strconv.FormatInt(m.MeasuredAt, 10),
			fmt.Sprintf("%.1f", m.Rating),
			fmt.Sprintf("%.1f", m.Speed),
			fmt.Sprintf("%.1f", m.Direction),
			m.Act,
			strconv.Itoa(m.TA),
			strconv.Itoa(m.PSC),
			strconv.Itoa(m.TAC),
			strconv.Itoa(m.PCI),
			strconv.Itoa(m.SID),
			strconv.Itoa(m.NID),
			strconv.Itoa(m.BID),
		}
		if err := writer.Write(record); err != nil {
			return fmt.Errorf("failed to write CSV record: %w", err)
		}
	}
	writer.Flush()

	// Create multipart form data
	var body bytes.Buffer
	body.WriteString("key=" + cm.config.APIKey + "&")
	body.WriteString("datafile=")
	body.Write(csvBuffer.Bytes())

	req, err := http.NewRequestWithContext(ctx, "POST",
		"https://opencellid.org/measure/uploadCsv", &body)
	if err != nil {
		return fmt.Errorf("failed to create request: %w", err)
	}

	req.Header.Set("Content-Type", "application/x-www-form-urlencoded")

	resp, err := cm.httpClient.Do(req)
	if err != nil {
		return fmt.Errorf("HTTP request failed: %w", err)
	}
	defer resp.Body.Close()

	if resp.StatusCode != 200 {
		body, _ := io.ReadAll(resp.Body)
		return fmt.Errorf("HTTP error %d: %s", resp.StatusCode, string(body))
	}

	return nil
}

// convertToOpenCellIDFormat converts observations to OpenCellID measurement format
func (cm *ContributionManager) convertToOpenCellIDFormat(observations []CellObservation) []OpenCellIDMeasurement {
	var measurements []OpenCellIDMeasurement

	for _, obs := range observations {
		measurement := cm.convertSingleObservation(obs)
		if measurement != nil {
			measurements = append(measurements, *measurement)
		}
	}

	return measurements
}

// convertSingleObservation converts a single observation to OpenCellID format
func (cm *ContributionManager) convertSingleObservation(obs CellObservation) *OpenCellIDMeasurement {
	// Parse cell identifiers
	mcc, err := strconv.Atoi(obs.ServingCell.MCC)
	if err != nil {
		cm.logger.Warn("invalid_mcc", "mcc", obs.ServingCell.MCC)
		return nil
	}

	mnc, err := strconv.Atoi(obs.ServingCell.MNC)
	if err != nil {
		cm.logger.Warn("invalid_mnc", "mnc", obs.ServingCell.MNC)
		return nil
	}

	cellID, err := strconv.Atoi(obs.ServingCell.CellID)
	if err != nil {
		cm.logger.Warn("invalid_cellid", "cellid", obs.ServingCell.CellID)
		return nil
	}

	measurement := &OpenCellIDMeasurement{
		Lon:        obs.GPS.Longitude,
		Lat:        obs.GPS.Latitude,
		MCC:        mcc,
		MNC:        mnc,
		CellID:     cellID,
		MeasuredAt: obs.GPS.Timestamp.UnixNano() / 1e6, // Convert to milliseconds
		Rating:     obs.GPS.Accuracy,
		Speed:      obs.GPS.Speed,
		Direction:  obs.GPS.Heading,
		Act:        cm.normalizeRadioTechnology(obs.ServingCell.Technology),
	}

	// Add LAC/TAC based on technology
	if tac, err := strconv.Atoi(obs.ServingCell.TAC); err == nil {
		if strings.ToUpper(obs.ServingCell.Technology) == "LTE" || strings.ToUpper(obs.ServingCell.Technology) == "NR" {
			measurement.TAC = tac
		} else {
			measurement.LAC = tac
		}
	}

	// Add signal strength if available
	if obs.Metrics != nil && obs.Metrics.RSRP != nil {
		measurement.Signal = *obs.Metrics.RSRP
	}

	// Add timing advance if available
	if obs.Metrics != nil && obs.Metrics.TimingAdvance != nil {
		measurement.TA = *obs.Metrics.TimingAdvance
	}

	// Handle CDMA fields (use MNC as SID, LAC as NID, CellID as BID)
	if strings.ToUpper(obs.ServingCell.Technology) == "CDMA" {
		measurement.SID = mnc
		measurement.NID = measurement.LAC
		measurement.BID = cellID
	}

	return measurement
}

// normalizeRadioTechnology normalizes radio technology names for OpenCellID
func (cm *ContributionManager) normalizeRadioTechnology(tech string) string {
	tech = strings.ToUpper(tech)
	switch tech {
	case "GSM", "EDGE", "GPRS":
		return "GSM"
	case "UMTS", "WCDMA", "HSPA", "HSDPA", "HSUPA", "HSPA+":
		return "UMTS"
	case "LTE", "4G":
		return "LTE"
	case "NR", "5G":
		return "NR"
	case "CDMA", "EVDO", "1XRTT":
		return "CDMA"
	default:
		return tech
	}
}

// updateSubmissionHistory updates the submission history for contributed cells
func (cm *ContributionManager) updateSubmissionHistory(observations []CellObservation) {
	cm.queue.mu.Lock()
	defer cm.queue.mu.Unlock()

	for _, obs := range observations {
		cellKey := cm.getCellKey(&obs.ServingCell)

		history := ContributionHistory{
			LastSubmission:  time.Now(),
			LastLocation:    obs.GPS,
			SubmissionCount: 1,
		}

		if existing, exists := cm.queue.lastSubmissions[cellKey]; exists {
			history.SubmissionCount = existing.SubmissionCount + 1
		}

		if obs.Metrics != nil && obs.Metrics.RSRP != nil {
			history.LastRSRP = obs.Metrics.RSRP
		}

		cm.queue.lastSubmissions[cellKey] = history
	}
}

// getCellKey generates a unique key for a cell
func (cm *ContributionManager) getCellKey(cell *ServingCellInfo) string {
	return fmt.Sprintf("%s-%s-%s-%s", cell.MCC, cell.MNC, cell.TAC, cell.CellID)
}

// calculateDistance calculates distance between two GPS observations
func (cm *ContributionManager) calculateDistance(gps1, gps2 GPSObservation) float64 {
	const earthRadiusM = 6371000

	lat1Rad := gps1.Latitude * math.Pi / 180
	lat2Rad := gps2.Latitude * math.Pi / 180
	deltaLatRad := (gps2.Latitude - gps1.Latitude) * math.Pi / 180
	deltaLonRad := (gps2.Longitude - gps1.Longitude) * math.Pi / 180

	a := math.Sin(deltaLatRad/2)*math.Sin(deltaLatRad/2) +
		math.Cos(lat1Rad)*math.Cos(lat2Rad)*
			math.Sin(deltaLonRad/2)*math.Sin(deltaLonRad/2)
	c := 2 * math.Atan2(math.Sqrt(a), math.Sqrt(1-a))

	return earthRadiusM * c
}

// countUniqueCells counts unique cells in observations
func (cm *ContributionManager) countUniqueCells(observations []CellObservation) int {
	cellSet := make(map[string]bool)
	for _, obs := range observations {
		cellKey := cm.getCellKey(&obs.ServingCell)
		cellSet[cellKey] = true
	}
	return len(cellSet)
}

// addUploadError adds an upload error to the error list (with limit)
func (cm *ContributionManager) addUploadError(err string) {
	maxErrors := 10
	if len(cm.stats.UploadErrors) >= maxErrors {
		// Remove oldest error
		cm.stats.UploadErrors = cm.stats.UploadErrors[1:]
	}
	cm.stats.UploadErrors = append(cm.stats.UploadErrors,
		fmt.Sprintf("%s: %s", time.Now().Format("15:04:05"), err))
}

// GetStats returns current contribution statistics
func (cm *ContributionManager) GetStats() ContributionStats {
	cm.mu.RLock()
	defer cm.mu.RUnlock()

	cm.queue.mu.RLock()
	queueSize := len(cm.queue.observations)
	cm.queue.mu.RUnlock()

	stats := *cm.stats
	stats.QueuedObservations = queueSize
	return stats
}
