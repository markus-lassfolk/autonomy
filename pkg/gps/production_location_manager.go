package gps

import (
	"context"
	"fmt"
	"math"
	"os/exec"
	"strconv"
	"strings"
	"sync"
	"time"

	"github.com/markus-lassfolk/autonomy/pkg/logx"
)

// ProductionLocationManager implements production-ready location management
type ProductionLocationManager struct {
	// Configuration
	config *ProductionLocationConfig

	// State management
	lastKnownLocation  *ProductionLocationResponse
	lastQueryTimestamp time.Time
	lastEnvironmentSig *EnvironmentSignature

	// Async operation management
	queryInProgress  bool
	backgroundCtx    context.Context
	backgroundCancel context.CancelFunc

	// Statistics
	stats *ProductionLocationStats

	// Thread safety
	mu sync.RWMutex

	// Logger
	logger *logx.Logger
}

// ProductionLocationConfig holds production configuration
type ProductionLocationConfig struct {
	// API settings
	GoogleAPIKey             string `json:"google_api_key"`
	GoogleGeoLocationEnabled bool   `json:"google_geolocation_enabled" default:"false"`
	MonthlyQuota             int    `json:"monthly_quota" default:"10000"`

	// Timing settings
	MinQueryInterval      time.Duration `json:"min_query_interval" default:"5m"`
	MaxCacheAge           time.Duration `json:"max_cache_age" default:"60m"`
	DebounceTime          time.Duration `json:"debounce_time" default:"10s"`
	RetryVerificationTime time.Duration `json:"retry_verification_time" default:"10s"`

	// Change detection thresholds
	CellChangeThreshold float64 `json:"cell_change_threshold" default:"0.35"`
	WiFiChangeThreshold float64 `json:"wifi_change_threshold" default:"0.40"`
	CellTopN            int     `json:"cell_top_n" default:"8"`
	WiFiTopK            int     `json:"wifi_top_k" default:"10"`

	// Stationary detection
	StationaryIntervals []time.Duration `json:"stationary_intervals"` // [10m, 20m, 40m, 60m]
	StationaryThreshold time.Duration   `json:"stationary_threshold" default:"2h"`

	// Quality gating (ChatGPT recommended parameters)
	AccuracyImprovement     float64 `json:"accuracy_improvement" default:"0.8"`      // Accept if 80% of old accuracy
	MinMovementDistance     float64 `json:"min_movement_distance" default:"300"`     // 300 meters
	MovementAccuracyFactor  float64 `json:"movement_accuracy_factor" default:"1.5"`  // 1.5x accuracy for movement
	AccuracyRegressionLimit float64 `json:"accuracy_regression_limit" default:"1.2"` // Allow 20% accuracy loss
	ChiSquareThreshold      float64 `json:"chi_square_threshold" default:"5.99"`     // 95% confidence in 2D

	// Background operation settings
	BackgroundUpdateInterval time.Duration `json:"background_interval" default:"30s"`
	MaxConcurrentQueries     int           `json:"max_concurrent_queries" default:"1"`
}

// ProductionLocationResponse represents the final location response
type ProductionLocationResponse struct {
	Latitude     float64       `json:"latitude"`
	Longitude    float64       `json:"longitude"`
	Accuracy     float64       `json:"accuracy"` // Meters - for UI display only
	Timestamp    time.Time     `json:"timestamp"`
	Source       string        `json:"source"`
	FromCache    bool          `json:"from_cache"`
	APICallMade  bool          `json:"api_call_made"`
	ResponseTime time.Duration `json:"response_time"`
	Altitude     float64       `json:"altitude"`
	Satellites   int           `json:"satellites"`
}

// ProductionLocationStats tracks operational statistics
type ProductionLocationStats struct {
	TotalRequests        int64         `json:"total_requests"`
	CacheHits            int64         `json:"cache_hits"`
	APICallsToday        int64         `json:"api_calls_today"`
	SuccessfulQueries    int64         `json:"successful_queries"`
	FailedQueries        int64         `json:"failed_queries"`
	EnvironmentChanges   int64         `json:"environment_changes"`
	DebouncedChanges     int64         `json:"debounced_changes"`
	VerifiedChanges      int64         `json:"verified_changes"`
	FallbacksToCache     int64         `json:"fallbacks_to_cache"`
	QualityRejections    int64         `json:"quality_rejections"`
	AcceptedLocations    int64         `json:"accepted_locations"`
	BigMoveAcceptances   int64         `json:"big_move_acceptances"`
	StationaryDetections int64         `json:"stationary_detections"`
	LastResetDate        time.Time     `json:"last_reset_date"`
	AverageResponseTime  time.Duration `json:"average_response_time"`
}

// EnvironmentSignature represents the current environment state
type EnvironmentSignature struct {
	CellularSignature string    `json:"cellular_signature"`
	WiFiSignature     string    `json:"wifi_signature"`
	Timestamp         time.Time `json:"timestamp"`
}

// NewProductionLocationManager creates a new production location manager
func NewProductionLocationManager(config *ProductionLocationConfig, logger *logx.Logger) (*ProductionLocationManager, error) {
	if config == nil {
		config = &ProductionLocationConfig{
			GoogleGeoLocationEnabled: false,
			MonthlyQuota:             10000,
			MinQueryInterval:         5 * time.Minute,
			MaxCacheAge:              60 * time.Minute,
			DebounceTime:             10 * time.Second,
			RetryVerificationTime:    10 * time.Second,
			CellChangeThreshold:      0.35,
			WiFiChangeThreshold:      0.40,
			CellTopN:                 8,
			WiFiTopK:                 10,
			StationaryIntervals:      []time.Duration{10 * time.Minute, 20 * time.Minute, 40 * time.Minute, 60 * time.Minute},
			StationaryThreshold:      2 * time.Hour,
			AccuracyImprovement:      0.8,
			MinMovementDistance:      300,
			MovementAccuracyFactor:   1.5,
			AccuracyRegressionLimit:  1.2,
			ChiSquareThreshold:       5.99,
			BackgroundUpdateInterval: 30 * time.Second,
			MaxConcurrentQueries:     1,
		}
	}

	backgroundCtx, backgroundCancel := context.WithCancel(context.Background())

	plm := &ProductionLocationManager{
		config:           config,
		backgroundCtx:    backgroundCtx,
		backgroundCancel: backgroundCancel,
		stats:            &ProductionLocationStats{LastResetDate: time.Now()},
		logger:           logger,
	}

	// Start background monitoring
	go plm.backgroundMonitor()

	logger.Info("Production location manager initialized",
		"google_api_enabled", config.GoogleGeoLocationEnabled,
		"monthly_quota", config.MonthlyQuota,
		"min_query_interval", config.MinQueryInterval,
		"max_cache_age", config.MaxCacheAge)

	return plm, nil
}

// GetLocation returns the best available location with intelligent caching
func (plm *ProductionLocationManager) GetLocation() *ProductionLocationResponse {
	plm.mu.Lock()
	defer plm.mu.Unlock()

	plm.stats.TotalRequests++

	// Check if we have a recent, valid cached location
	if plm.lastKnownLocation != nil {
		cacheAge := time.Since(plm.lastKnownLocation.Timestamp)
		if cacheAge < plm.config.MaxCacheAge {
			plm.stats.CacheHits++
			plm.logger.Debug("Returning cached location",
				"cache_age", cacheAge,
				"source", plm.lastKnownLocation.Source,
				"accuracy", plm.lastKnownLocation.Accuracy)
			return plm.lastKnownLocation
		}
	}

	// Perform new location query
	startTime := time.Now()
	location, err := plm.performLocationQuery()
	responseTime := time.Since(startTime)

	if err != nil {
		plm.stats.FailedQueries++
		plm.logger.Error("Location query failed", "error", err)

		// Return cached location as fallback if available
		if plm.lastKnownLocation != nil {
			plm.stats.FallbacksToCache++
			plm.logger.Warn("Falling back to cached location", "cache_age", time.Since(plm.lastKnownLocation.Timestamp))
			return plm.lastKnownLocation
		}

		return nil
	}

	// Apply quality gates
	location, err = plm.applyQualityGates(location)
	if err != nil {
		plm.stats.QualityRejections++
		plm.logger.Warn("Location rejected by quality gates", "error", err)

		// Return cached location as fallback
		if plm.lastKnownLocation != nil {
			plm.stats.FallbacksToCache++
			return plm.lastKnownLocation
		}

		return nil
	}

	// Update statistics
	location.ResponseTime = responseTime
	plm.stats.SuccessfulQueries++
	plm.stats.AverageResponseTime = (plm.stats.AverageResponseTime + responseTime) / 2

	// Update last known location
	plm.lastKnownLocation = location
	plm.lastQueryTimestamp = time.Now()

	plm.logger.Info("Location query successful",
		"source", location.Source,
		"accuracy", location.Accuracy,
		"response_time", responseTime,
		"api_call_made", location.APICallMade)

	return location
}

// backgroundMonitor runs background monitoring for environment changes
func (plm *ProductionLocationManager) backgroundMonitor() {
	ticker := time.NewTicker(plm.config.BackgroundUpdateInterval)
	defer ticker.Stop()

	for {
		select {
		case <-plm.backgroundCtx.Done():
			return
		case <-ticker.C:
			plm.checkForEnvironmentChanges()
		}
	}
}

// checkForEnvironmentChanges monitors for significant environment changes
func (plm *ProductionLocationManager) checkForEnvironmentChanges() {
	currentSig, err := plm.getCurrentEnvironmentSignature()
	if err != nil {
		plm.logger.Debug("Failed to get current environment signature", "error", err)
		return
	}

	if plm.lastEnvironmentSig != nil {
		changed, reason := plm.detectSignificantChange(plm.lastEnvironmentSig, currentSig)
		if changed {
			plm.stats.EnvironmentChanges++
			plm.logger.Info("Environment change detected", "reason", reason)
			plm.debounceAndVerifyChange(currentSig, reason)
		}
	}

	plm.lastEnvironmentSig = currentSig
}

// debounceAndVerifyChange implements debouncing for environment changes
func (plm *ProductionLocationManager) debounceAndVerifyChange(initialSig *EnvironmentSignature, initialReason string) {
	time.Sleep(plm.config.DebounceTime)

	// Check if change is still present after debounce period
	currentSig, err := plm.getCurrentEnvironmentSignature()
	if err != nil {
		plm.logger.Debug("Failed to verify environment change", "error", err)
		return
	}

	changed, reason := plm.detectSignificantChange(initialSig, currentSig)
	if changed {
		plm.stats.VerifiedChanges++
		plm.logger.Info("Environment change verified after debounce", "reason", reason)
		plm.triggerBackgroundUpdate(reason)
	} else {
		plm.stats.DebouncedChanges++
		plm.logger.Debug("Environment change debounced", "initial_reason", initialReason)
	}
}

// triggerBackgroundUpdate triggers a background location update
func (plm *ProductionLocationManager) triggerBackgroundUpdate(reason string) {
	if plm.queryInProgress {
		plm.logger.Debug("Query already in progress, skipping background update")
		return
	}

	plm.queryInProgress = true
	go func() {
		defer func() { plm.queryInProgress = false }()

		plm.logger.Info("Triggering background location update", "reason", reason)

		location, err := plm.performLocationQuery()
		if err != nil {
			plm.logger.Error("Background location update failed", "error", err)
			return
		}

		// Apply quality gates
		location, err = plm.applyQualityGates(location)
		if err != nil {
			plm.logger.Warn("Background location update rejected by quality gates", "error", err)
			return
		}

		plm.mu.Lock()
		plm.lastKnownLocation = location
		plm.lastQueryTimestamp = time.Now()
		plm.mu.Unlock()

		plm.logger.Info("Background location update successful",
			"source", location.Source,
			"accuracy", location.Accuracy,
			"reason", reason)
	}()
}

// performLocationQuery performs the actual location query with priority-based fallback
func (plm *ProductionLocationManager) performLocationQuery() (*ProductionLocationResponse, error) {
	startTime := time.Now()

	// Priority 1: Try GPS (Quectel GNSS) - highest accuracy
	if gpsLocation, err := plm.queryGPS(); err == nil && gpsLocation != nil {
		return &ProductionLocationResponse{
			Latitude:     gpsLocation.Latitude,
			Longitude:    gpsLocation.Longitude,
			Accuracy:     gpsLocation.Accuracy,
			Timestamp:    time.Now(),
			Source:       gpsLocation.Source,
			FromCache:    false,
			APICallMade:  false,
			ResponseTime: time.Since(startTime),
			Altitude:     gpsLocation.Altitude,
			Satellites:   gpsLocation.Satellites,
		}, nil
	}

	// Priority 2: Try Enhanced WiFi (if available)
	if wifiLocation, err := plm.queryEnhancedWiFi(); err == nil && wifiLocation != nil {
		return &ProductionLocationResponse{
			Latitude:     wifiLocation.Latitude,
			Longitude:    wifiLocation.Longitude,
			Accuracy:     wifiLocation.Accuracy,
			Timestamp:    time.Now(),
			Source:       wifiLocation.Source,
			FromCache:    false,
			APICallMade:  true,
			ResponseTime: time.Since(startTime),
		}, nil
	}

	// Priority 3: Try Combined Cell+WiFi (Google API)
	if combinedLocation, err := plm.queryCombinedCellWiFi(); err == nil && combinedLocation != nil {
		return &ProductionLocationResponse{
			Latitude:     combinedLocation.Latitude,
			Longitude:    combinedLocation.Longitude,
			Accuracy:     combinedLocation.Accuracy,
			Timestamp:    time.Now(),
			Source:       combinedLocation.Source,
			FromCache:    false,
			APICallMade:  true,
			ResponseTime: time.Since(startTime),
		}, nil
	}

	// Priority 4: Try WiFi-Only
	if wifiOnlyLocation, err := plm.queryWiFiOnly(); err == nil && wifiOnlyLocation != nil {
		return &ProductionLocationResponse{
			Latitude:     wifiOnlyLocation.Latitude,
			Longitude:    wifiOnlyLocation.Longitude,
			Accuracy:     wifiOnlyLocation.Accuracy,
			Timestamp:    time.Now(),
			Source:       wifiOnlyLocation.Source,
			FromCache:    false,
			APICallMade:  true,
			ResponseTime: time.Since(startTime),
		}, nil
	}

	// Priority 5: Try Cellular-Only (last resort)
	if cellularLocation, err := plm.queryCellularOnly(); err == nil && cellularLocation != nil {
		return &ProductionLocationResponse{
			Latitude:     cellularLocation.Latitude,
			Longitude:    cellularLocation.Longitude,
			Accuracy:     cellularLocation.Accuracy,
			Timestamp:    time.Now(),
			Source:       cellularLocation.Source,
			FromCache:    false,
			APICallMade:  true,
			ResponseTime: time.Since(startTime),
		}, nil
	}

	return nil, fmt.Errorf("all location sources failed")
}

// applyQualityGates applies quality validation to location data
func (plm *ProductionLocationManager) applyQualityGates(newLocation *ProductionLocationResponse) (*ProductionLocationResponse, error) {
	// Check if we have a previous location to compare against
	if plm.lastKnownLocation != nil {
		distance := plm.calculateDistance(plm.lastKnownLocation, newLocation)

		// Check for big moves (accept immediately)
		if distance > plm.config.MinMovementDistance {
			plm.stats.BigMoveAcceptances++
			plm.logger.Info("Big move detected, accepting location", "distance", distance)
			return newLocation, nil
		}

		// Check accuracy regression
		accuracyRatio := newLocation.Accuracy / plm.lastKnownLocation.Accuracy
		if accuracyRatio > plm.config.AccuracyRegressionLimit {
			return nil, fmt.Errorf("accuracy regression too high: %.2f > %.2f", accuracyRatio, plm.config.AccuracyRegressionLimit)
		}

		// Check for reasonable accuracy improvement
		if accuracyRatio < plm.config.AccuracyImprovement {
			plm.logger.Debug("Location rejected: insufficient accuracy improvement", "ratio", accuracyRatio)
			return nil, fmt.Errorf("insufficient accuracy improvement: %.2f < %.2f", accuracyRatio, plm.config.AccuracyImprovement)
		}
	}

	plm.stats.AcceptedLocations++
	return newLocation, nil
}

// getCurrentEnvironmentSignature gets the current environment signature
func (plm *ProductionLocationManager) getCurrentEnvironmentSignature() (*EnvironmentSignature, error) {
	// This would implement cellular and WiFi signature generation
	// For now, return a simple signature
	return &EnvironmentSignature{
		CellularSignature: "default_cellular",
		WiFiSignature:     "default_wifi",
		Timestamp:         time.Now(),
	}, nil
}

// detectSignificantChange detects if there's a significant environment change
func (plm *ProductionLocationManager) detectSignificantChange(old, new *EnvironmentSignature) (bool, string) {
	if old.CellularSignature != new.CellularSignature {
		return true, "cellular_change"
	}
	if old.WiFiSignature != new.WiFiSignature {
		return true, "wifi_change"
	}
	return false, ""
}

// calculateDistance calculates distance between two locations
func (plm *ProductionLocationManager) calculateDistance(loc1, loc2 *ProductionLocationResponse) float64 {
	const earthRadius = 6371000 // meters

	lat1 := loc1.Latitude * math.Pi / 180
	lat2 := loc2.Latitude * math.Pi / 180
	deltaLat := (loc2.Latitude - loc1.Latitude) * math.Pi / 180
	deltaLon := (loc2.Longitude - loc1.Longitude) * math.Pi / 180

	a := math.Sin(deltaLat/2)*math.Sin(deltaLat/2) +
		math.Cos(lat1)*math.Cos(lat2)*
			math.Sin(deltaLon/2)*math.Sin(deltaLon/2)
	c := 2 * math.Atan2(math.Sqrt(a), math.Sqrt(1-a))

	return earthRadius * c
}

// GetStats returns the current statistics
func (plm *ProductionLocationManager) GetStats() *ProductionLocationStats {
	plm.mu.RLock()
	defer plm.mu.RUnlock()

	stats := *plm.stats // Copy to avoid race conditions
	return &stats
}

// PrintProductionStats prints production statistics
func (plm *ProductionLocationManager) PrintProductionStats() {
	stats := plm.GetStats()

	plm.logger.Info("Production Location Manager Statistics",
		"total_requests", stats.TotalRequests,
		"cache_hits", stats.CacheHits,
		"api_calls_today", stats.APICallsToday,
		"successful_queries", stats.SuccessfulQueries,
		"failed_queries", stats.FailedQueries,
		"environment_changes", stats.EnvironmentChanges,
		"debounced_changes", stats.DebouncedChanges,
		"verified_changes", stats.VerifiedChanges,
		"fallbacks_to_cache", stats.FallbacksToCache,
		"quality_rejections", stats.QualityRejections,
		"accepted_locations", stats.AcceptedLocations,
		"big_move_acceptances", stats.BigMoveAcceptances,
		"stationary_detections", stats.StationaryDetections,
		"average_response_time", stats.AverageResponseTime)
}

// Close gracefully shuts down the production location manager
func (plm *ProductionLocationManager) Close() error {
	plm.backgroundCancel()
	plm.logger.Info("Production location manager shut down")
	return nil
}

// Placeholder methods for location queries - these would be implemented with actual GPS/WiFi/Cellular logic
func (plm *ProductionLocationManager) queryGPS() (*ProductionLocationResponse, error) {
	// Implement actual GPS query using gpsctl
	plm.logger.Debug("Querying RUTOS GPS via gpsctl")

	// Check GPS status first
	statusCmd := exec.Command("gpsctl", "-s")
	statusOutput, err := statusCmd.Output()
	if err != nil {
		return nil, fmt.Errorf("failed to get GPS status: %w", err)
	}

	status := strings.TrimSpace(string(statusOutput))
	if status != "1" {
		return nil, fmt.Errorf("GPS not active, status: %s", status)
	}

	// Get GPS coordinates
	latCmd := exec.Command("gpsctl", "-i")
	latOutput, err := latCmd.Output()
	if err != nil {
		return nil, fmt.Errorf("failed to get latitude: %w", err)
	}

	lonCmd := exec.Command("gpsctl", "-x")
	lonOutput, err := lonCmd.Output()
	if err != nil {
		return nil, fmt.Errorf("failed to get longitude: %w", err)
	}

	altCmd := exec.Command("gpsctl", "-a")
	altOutput, err := altCmd.Output()
	if err != nil {
		return nil, fmt.Errorf("failed to get altitude: %w", err)
	}

	accCmd := exec.Command("gpsctl", "-u")
	accOutput, err := accCmd.Output()
	if err != nil {
		return nil, fmt.Errorf("failed to get accuracy: %w", err)
	}

	satCmd := exec.Command("gpsctl", "-p")
	satOutput, err := satCmd.Output()
	if err != nil {
		return nil, fmt.Errorf("failed to get satellite count: %w", err)
	}

	// Parse values
	lat, err := strconv.ParseFloat(strings.TrimSpace(string(latOutput)), 64)
	if err != nil {
		return nil, fmt.Errorf("failed to parse latitude: %w", err)
	}

	lon, err := strconv.ParseFloat(strings.TrimSpace(string(lonOutput)), 64)
	if err != nil {
		return nil, fmt.Errorf("failed to parse longitude: %w", err)
	}

	alt, err := strconv.ParseFloat(strings.TrimSpace(string(altOutput)), 64)
	if err != nil {
		return nil, fmt.Errorf("failed to parse altitude: %w", err)
	}

	acc, err := strconv.ParseFloat(strings.TrimSpace(string(accOutput)), 64)
	if err != nil {
		return nil, fmt.Errorf("failed to parse accuracy: %w", err)
	}

	sat, err := strconv.Atoi(strings.TrimSpace(string(satOutput)))
	if err != nil {
		return nil, fmt.Errorf("failed to parse satellite count: %w", err)
	}

	// Validate coordinates
	if lat == 0 && lon == 0 {
		return nil, fmt.Errorf("invalid GPS coordinates: 0,0")
	}

	plm.logger.Info("RUTOS GPS data collected successfully",
		"latitude", lat,
		"longitude", lon,
		"altitude", alt,
		"accuracy", acc,
		"satellites", sat)

	return &ProductionLocationResponse{
		Latitude:     lat,
		Longitude:    lon,
		Accuracy:     acc,
		Altitude:     alt,
		Satellites:   sat,
		Timestamp:    time.Now(),
		Source:       "rutos",
		FromCache:    false,
		APICallMade:  false,
		ResponseTime: 0, // Local query, no network delay
	}, nil
}

func (plm *ProductionLocationManager) queryEnhancedWiFi() (*ProductionLocationResponse, error) {
	// This would implement enhanced WiFi query
	return nil, fmt.Errorf("Enhanced WiFi query not implemented")
}

func (plm *ProductionLocationManager) queryCombinedCellWiFi() (*ProductionLocationResponse, error) {
	// Implement combined cell+WiFi query using Google Location API
	if plm.config.GoogleAPIKey == "" {
		return nil, fmt.Errorf("Google API key not configured")
	}

	plm.logger.Debug("Querying Google Location API")

	// For now, we'll use a simple HTTP request to Google's Location API
	// In a full implementation, this would collect WiFi and cellular data
	// and send it to Google's API

	// Since we don't have WiFi/cellular data collection implemented yet,
	// we'll return an error indicating this source is not available
	return nil, fmt.Errorf("Google Location API requires WiFi/cellular data collection (not implemented)")
}

func (plm *ProductionLocationManager) queryWiFiOnly() (*ProductionLocationResponse, error) {
	// This would implement WiFi-only query
	return nil, fmt.Errorf("WiFi-only query not implemented")
}

func (plm *ProductionLocationManager) queryCellularOnly() (*ProductionLocationResponse, error) {
	// This would implement cellular-only query
	return nil, fmt.Errorf("Cellular-only query not implemented")
}

// Note: CellularLocationIntelligence is defined in cellular_intelligence.go

// UbusWiFiAccessPoint represents WiFi access point data
type UbusWiFiAccessPoint struct {
	BSSID          string  `json:"bssid"`
	SSID           string  `json:"ssid"`
	SignalStrength int     `json:"signal_strength"`
	Channel        int     `json:"channel"`
	Frequency      float64 `json:"frequency"`
	Quality        int     `json:"quality"`
}
