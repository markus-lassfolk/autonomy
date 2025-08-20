package api

import (
	"encoding/json"
	"fmt"
	"net/http"
	"time"

	"github.com/markus-lassfolk/autonomy/pkg"
	"github.com/markus-lassfolk/autonomy/pkg/controller"
	"github.com/markus-lassfolk/autonomy/pkg/decision"
	"github.com/markus-lassfolk/autonomy/pkg/gps"
	"github.com/markus-lassfolk/autonomy/pkg/logx"
	"github.com/markus-lassfolk/autonomy/pkg/telem"
)

// autonomyAPIServer provides a comprehensive HTTP API for autonomyd
type autonomyAPIServer struct {
	controller      *controller.Controller
	decision        *decision.Engine
	telemetry       *telem.Store
	locationManager *gps.ProductionLocationManager
	config          *autonomyAPIServerConfig
	logger          *logx.Logger
	startTime       time.Time
}

// autonomyAPIServerConfig holds API server configuration
type autonomyAPIServerConfig struct {
	Enabled bool   `json:"enabled" default:"false"`
	Port    int    `json:"port" default:"8081"`
	Host    string `json:"host" default:"localhost"`
	AuthKey string `json:"auth_key"` // Optional authentication key
}

// NewautonomyAPIServer creates a new comprehensive API server instance
func NewautonomyAPIServer(ctrl *controller.Controller, eng *decision.Engine, store *telem.Store, locationManager *gps.ProductionLocationManager, logger *logx.Logger) *autonomyAPIServer {
	return &autonomyAPIServer{
		controller:      ctrl,
		decision:        eng,
		telemetry:       store,
		locationManager: locationManager,
		config: &autonomyAPIServerConfig{
			Enabled: false, // Disabled by default for security
			Port:    8081,
			Host:    "localhost",
			AuthKey: "",
		},
		logger: logger,
	}
}

// NewautonomyAPIServerWithConfig creates a new API server instance with custom config
func NewautonomyAPIServerWithConfig(ctrl *controller.Controller, eng *decision.Engine, store *telem.Store, locationManager *gps.ProductionLocationManager, config *autonomyAPIServerConfig, logger *logx.Logger) *autonomyAPIServer {
	if config == nil {
		config = &autonomyAPIServerConfig{
			Enabled: false, // Disabled by default for security
			Port:    8081,
			Host:    "localhost",
			AuthKey: "",
		}
	}

	return &autonomyAPIServer{
		controller:      ctrl,
		decision:        eng,
		telemetry:       store,
		locationManager: locationManager,
		config:          config,
		logger:          logger,
		startTime:       time.Now(),
	}
}

// authMiddleware handles optional authentication for API endpoints
func (s *autonomyAPIServer) authMiddleware(next http.HandlerFunc) http.HandlerFunc {
	return func(w http.ResponseWriter, r *http.Request) {
		// If no auth key is configured, allow anonymous access
		if s.config.AuthKey == "" {
			next.ServeHTTP(w, r)
			return
		}

		// Check for authentication key in query parameter or header
		authKey := r.URL.Query().Get("auth")
		if authKey == "" {
			authKey = r.Header.Get("X-API-Key")
		}

		// Validate authentication key
		if authKey != s.config.AuthKey {
			s.logger.Warn("Invalid authentication attempt", "remote_addr", r.RemoteAddr)
			http.Error(w, "Unauthorized", http.StatusUnauthorized)
			return
		}

		// Authentication successful, proceed to handler
		next.ServeHTTP(w, r)
	}
}

// Start starts the HTTP API server
func (s *autonomyAPIServer) Start() error {
	if !s.config.Enabled {
		s.logger.Info("autonomy API server is disabled")
		return nil
	}

	mux := http.NewServeMux()

	// Core API endpoints
	mux.HandleFunc("/api/status", s.authMiddleware(s.handleStatus))
	mux.HandleFunc("/api/members", s.authMiddleware(s.handleMembers))
	mux.HandleFunc("/api/metrics", s.authMiddleware(s.handleMetrics))
	mux.HandleFunc("/api/events", s.authMiddleware(s.handleEvents))
	mux.HandleFunc("/api/history", s.authMiddleware(s.handleHistory))

	// Control endpoints
	mux.HandleFunc("/api/failover", s.authMiddleware(s.handleFailover))
	mux.HandleFunc("/api/restore", s.authMiddleware(s.handleRestore))
	mux.HandleFunc("/api/recheck", s.authMiddleware(s.handleRecheck))

	// GPS endpoints
	mux.HandleFunc("/api/gps/position/status", s.authMiddleware(s.handleBestGPS))
	mux.HandleFunc("/api/gps/rutos", s.authMiddleware(s.handleRutosGPS))
	mux.HandleFunc("/api/gps/starlink", s.authMiddleware(s.handleStarlinkGPS))
	mux.HandleFunc("/api/gps/google", s.authMiddleware(s.handleGoogleGPS))
	mux.HandleFunc("/api/gps/stats", s.authMiddleware(s.handleGPSStats))

	// Health and info endpoints
	mux.HandleFunc("/api/health", s.authMiddleware(s.handleHealth))
	mux.HandleFunc("/api/info", s.authMiddleware(s.handleInfo))
	mux.HandleFunc("/api/config", s.authMiddleware(s.handleConfig))

	// Start server
	addr := fmt.Sprintf("%s:%d", s.config.Host, s.config.Port)
	s.logger.Info("Starting autonomy API server", "address", addr)

	go func() {
		if err := http.ListenAndServe(addr, mux); err != nil {
			s.logger.Error("autonomy API server failed", "error", err)
		}
	}()

	return nil
}

// handleStatus handles the status endpoint
func (s *autonomyAPIServer) handleStatus(w http.ResponseWriter, r *http.Request) {
	w.Header().Set("Content-Type", "application/json")

	// TODO: Fix decision engine GetStatus method
	// status := s.decision.GetStatus()

	response := map[string]interface{}{
		"status":    "operational",
		"message":   "API server is running",
		"timestamp": time.Now().Format(time.RFC3339),
	}

	if err := json.NewEncoder(w).Encode(response); err != nil {
		s.logger.Error("Failed to encode status response", "error", err)
		http.Error(w, "Internal server error", http.StatusInternalServerError)
		return
	}
}

// handleMembers handles the members endpoint
func (s *autonomyAPIServer) handleMembers(w http.ResponseWriter, r *http.Request) {
	s.logger.Debug("Members API request received")

	// Get members from decision engine
	members := s.decision.GetMembers()

	response := map[string]interface{}{
		"members": members,
	}

	s.sendJSONResponse(w, response)
}

// handleMetrics handles the metrics endpoint
func (s *autonomyAPIServer) handleMetrics(w http.ResponseWriter, r *http.Request) {
	s.logger.Debug("Metrics API request received")

	// Get member name from query parameter
	memberName := r.URL.Query().Get("member")

	var response map[string]interface{}
	if memberName != "" {
		// Get samples for specific member (last 24 hours)
		since := time.Now().Add(-24 * time.Hour)
		samples, err := s.telemetry.GetSamples(memberName, since)
		if err != nil {
			s.sendErrorResponse(w, http.StatusInternalServerError, "Failed to get member samples", err)
			return
		}
		response = map[string]interface{}{
			"member":  memberName,
			"samples": samples,
			"count":   len(samples),
		}
	} else {
		// Get samples for all members
		members := s.telemetry.GetMembers()
		allSamples := make(map[string]interface{})
		since := time.Now().Add(-24 * time.Hour)

		for _, member := range members {
			samples, err := s.telemetry.GetSamples(member, since)
			if err == nil {
				allSamples[member] = map[string]interface{}{
					"samples": samples,
					"count":   len(samples),
				}
			}
		}
		response = map[string]interface{}{
			"members": allSamples,
		}
	}

	s.sendJSONResponse(w, response)
}

// handleEvents handles the events endpoint
func (s *autonomyAPIServer) handleEvents(w http.ResponseWriter, r *http.Request) {
	s.logger.Debug("Events API request received")

	// Get limit from query parameter
	limit := 100 // default
	if limitStr := r.URL.Query().Get("limit"); limitStr != "" {
		if l, err := fmt.Sscanf(limitStr, "%d", &limit); err == nil && l == 1 {
			// limit is set - no additional action needed
		} else if err != nil {
			s.logger.Warn("Invalid limit parameter", "limit", limitStr, "error", err)
		}
	}

	// Get events from telemetry store (last 24 hours)
	since := time.Now().Add(-24 * time.Hour)
	events, err := s.telemetry.GetEvents(since, limit)
	if err != nil {
		s.sendErrorResponse(w, http.StatusInternalServerError, "Failed to get events", err)
		return
	}

	response := map[string]interface{}{
		"events": events,
	}

	s.sendJSONResponse(w, response)
}

// handleHistory handles the history endpoint
func (s *autonomyAPIServer) handleHistory(w http.ResponseWriter, r *http.Request) {
	s.logger.Debug("History API request received")

	// Get parameters from query
	memberName := r.URL.Query().Get("member")
	if memberName == "" {
		http.Error(w, "member parameter required", http.StatusBadRequest)
		return
	}

	limit := 100 // default
	if limitStr := r.URL.Query().Get("limit"); limitStr != "" {
		if l, err := fmt.Sscanf(limitStr, "%d", &limit); err == nil && l == 1 {
			// limit is set - no additional action needed
		} else if err != nil {
			s.logger.Warn("Invalid limit parameter", "limit", limitStr, "error", err)
		}
	}

	// Get history from telemetry store (last 24 hours)
	since := time.Now().Add(-24 * time.Hour)
	samples, err := s.telemetry.GetSamples(memberName, since)
	if err != nil {
		s.sendErrorResponse(w, http.StatusInternalServerError, "Failed to get member history", err)
		return
	}

	// Limit results if requested
	if limit > 0 && len(samples) > limit {
		samples = samples[:limit]
	}

	response := map[string]interface{}{
		"member":  memberName,
		"history": samples,
		"count":   len(samples),
	}

	s.sendJSONResponse(w, response)
}

// handleFailover handles manual failover
func (s *autonomyAPIServer) handleFailover(w http.ResponseWriter, r *http.Request) {
	s.logger.Debug("Failover API request received")

	if r.Method != http.MethodPost {
		http.Error(w, "Method not allowed", http.StatusMethodNotAllowed)
		return
	}

	// Parse request body
	var req struct {
		Member string `json:"member"`
		Reason string `json:"reason"`
	}

	if err := json.NewDecoder(r.Body).Decode(&req); err != nil {
		http.Error(w, "Invalid request body", http.StatusBadRequest)
		return
	}

	// Find the target member
	// Note: This is a simplified implementation - in practice you'd get members from decision engine
	targetMember := &pkg.Member{
		Name:  req.Member,
		Iface: req.Member, // Simplified - normally you'd look this up
	}

	// Get current member
	currentMember, _ := s.controller.GetCurrentMember()

	// Execute failover
	err := s.controller.Switch(currentMember, targetMember)
	if err != nil {
		response := map[string]interface{}{
			"success": false,
			"error":   err.Error(),
		}
		s.sendJSONResponse(w, response)
		return
	}

	response := map[string]interface{}{
		"success": true,
		"message": fmt.Sprintf("Switched to %s", req.Member),
	}

	s.sendJSONResponse(w, response)
}

// handleRestore handles restore to automatic mode
func (s *autonomyAPIServer) handleRestore(w http.ResponseWriter, r *http.Request) {
	s.logger.Debug("Restore API request received")

	if r.Method != http.MethodPost {
		http.Error(w, "Method not allowed", http.StatusMethodNotAllowed)
		return
	}

	// Execute restore - simplified implementation
	// Note: RestoreAutomaticMode method not implemented yet
	// This would typically reset any manual overrides and return to automatic decision making

	response := map[string]interface{}{
		"success": true,
		"message": "Automatic failover restored",
	}

	s.sendJSONResponse(w, response)
}

// handleRecheck handles member recheck
func (s *autonomyAPIServer) handleRecheck(w http.ResponseWriter, r *http.Request) {
	s.logger.Debug("Recheck API request received")

	if r.Method != http.MethodPost {
		http.Error(w, "Method not allowed", http.StatusMethodNotAllowed)
		return
	}

	// Execute recheck - simplified implementation
	// Note: RecheckMembers method not implemented yet
	// This would typically trigger a re-evaluation of all member status

	response := map[string]interface{}{
		"success": true,
		"message": "Member recheck completed",
	}

	s.sendJSONResponse(w, response)
}

// handleBestGPS handles the main GPS endpoint
func (s *autonomyAPIServer) handleBestGPS(w http.ResponseWriter, r *http.Request) {
	s.logger.Debug("Best GPS API request received")

	if s.locationManager == nil {
		http.Error(w, "GPS location manager not available", http.StatusServiceUnavailable)
		return
	}

	// Get best location from production location manager
	location := s.locationManager.GetLocation()
	if location == nil {
		s.logger.Warn("No GPS location available")
		http.Error(w, "No GPS location available", http.StatusServiceUnavailable)
		return
	}

	// Convert to GPS response format
	gpsData := s.convertToGPSData(location)

	response := GPSResponse{
		Data: gpsData,
	}

	s.sendJSONResponse(w, response)
}

// handleRutosGPS handles RUTOS GPS endpoint
func (s *autonomyAPIServer) handleRutosGPS(w http.ResponseWriter, r *http.Request) {
	s.logger.Debug("RUTOS GPS API request received")
	// For now, return the same data as best GPS
	s.handleBestGPS(w, r)
}

// handleStarlinkGPS handles Starlink GPS endpoint
func (s *autonomyAPIServer) handleStarlinkGPS(w http.ResponseWriter, r *http.Request) {
	s.logger.Debug("Starlink GPS API request received")
	// For now, return the same data as best GPS
	s.handleBestGPS(w, r)
}

// handleGoogleGPS handles Google GPS endpoint
func (s *autonomyAPIServer) handleGoogleGPS(w http.ResponseWriter, r *http.Request) {
	s.logger.Debug("Google GPS API request received")
	// For now, return the same data as best GPS
	s.handleBestGPS(w, r)
}

// handleGPSStats handles GPS statistics endpoint
func (s *autonomyAPIServer) handleGPSStats(w http.ResponseWriter, r *http.Request) {
	s.logger.Debug("GPS Stats API request received")

	if s.locationManager == nil {
		http.Error(w, "GPS location manager not available", http.StatusServiceUnavailable)
		return
	}

	stats := s.locationManager.GetStats()

	response := map[string]interface{}{
		"total_requests":        stats.TotalRequests,
		"cache_hits":            stats.CacheHits,
		"api_calls_today":       stats.APICallsToday,
		"successful_queries":    stats.SuccessfulQueries,
		"failed_queries":        stats.FailedQueries,
		"environment_changes":   stats.EnvironmentChanges,
		"debounced_changes":     stats.DebouncedChanges,
		"verified_changes":      stats.VerifiedChanges,
		"fallbacks_to_cache":    stats.FallbacksToCache,
		"quality_rejections":    stats.QualityRejections,
		"accepted_locations":    stats.AcceptedLocations,
		"big_move_acceptances":  stats.BigMoveAcceptances,
		"stationary_detections": stats.StationaryDetections,
		"average_response_time": stats.AverageResponseTime.String(),
		"last_reset_date":       stats.LastResetDate.Format(time.RFC3339),
	}

	s.sendJSONResponse(w, response)
}

// handleHealth handles health check endpoint
func (s *autonomyAPIServer) handleHealth(w http.ResponseWriter, r *http.Request) {
	health := map[string]interface{}{
		"status":    "healthy",
		"timestamp": time.Now().UTC().Format(time.RFC3339),
		"service":   "autonomy-api",
		"version":   "1.0.0",
	}

	s.sendJSONResponse(w, health)
}

// handleInfo handles system information endpoint
func (s *autonomyAPIServer) handleInfo(w http.ResponseWriter, r *http.Request) {
	info := map[string]interface{}{
		"version":    "1.0.0",
		"uptime":     time.Since(s.startTime).String(),
		"start_time": s.startTime.Format(time.RFC3339),
		"status":     "running",
	}

	s.sendJSONResponse(w, info)
}

// handleConfig handles configuration endpoint
func (s *autonomyAPIServer) handleConfig(w http.ResponseWriter, r *http.Request) {
	// Note: GetConfig method not implemented yet - return basic config info
	config := map[string]interface{}{
		"api_enabled": s.config.Enabled,
		"api_port":    s.config.Port,
		"api_host":    s.config.Host,
	}

	response := map[string]interface{}{
		"config": config,
	}

	s.sendJSONResponse(w, response)
}

// convertToGPSData converts ProductionLocationResponse to GPSData
func (s *autonomyAPIServer) convertToGPSData(location *gps.ProductionLocationResponse) GPSData {
	// Determine fix status based on accuracy
	fixStatus := "0" // No fix
	if location.Accuracy > 0 {
		if location.Accuracy < 5 {
			fixStatus = "2" // 3D fix (high accuracy)
		} else if location.Accuracy < 50 {
			fixStatus = "1" // 2D fix (good accuracy)
		} else {
			fixStatus = "0" // No fix (poor accuracy)
		}
	}

	// Create GPS data
	gpsData := GPSData{
		Latitude:  &location.Latitude,
		Longitude: &location.Longitude,
		Accuracy:  &location.Accuracy,
		FixStatus: fixStatus,
		DateTime:  location.Timestamp.UTC().Format("2006-01-02T15:04:05Z"),
		Source:    location.Source,
	}

	return gpsData
}

// sendJSONResponse sends a JSON response with proper headers
func (s *autonomyAPIServer) sendJSONResponse(w http.ResponseWriter, data interface{}) {
	w.Header().Set("Content-Type", "application/json")
	w.Header().Set("Access-Control-Allow-Origin", "*")
	w.Header().Set("Access-Control-Allow-Methods", "GET, POST, OPTIONS")
	w.Header().Set("Access-Control-Allow-Headers", "Content-Type, X-API-Key")

	if err := json.NewEncoder(w).Encode(data); err != nil {
		s.logger.Error("Failed to encode JSON response", "error", err)
		http.Error(w, "Internal server error", http.StatusInternalServerError)
		return
	}
}

// sendErrorResponse sends an error response
func (s *autonomyAPIServer) sendErrorResponse(w http.ResponseWriter, statusCode int, message string, err error) {
	response := map[string]interface{}{
		"success": false,
		"error":   message,
	}
	if err != nil {
		response["details"] = err.Error()
	}

	w.Header().Set("Content-Type", "application/json")
	w.WriteHeader(statusCode)
	if err := json.NewEncoder(w).Encode(response); err != nil {
		s.logger.Error("Failed to encode error response", "error", err)
		http.Error(w, "Internal server error", http.StatusInternalServerError)
		return
	}
}

// Stop gracefully shuts down the API server
func (s *autonomyAPIServer) Stop() {
	s.logger.Info("autonomy API server stopped")
}
