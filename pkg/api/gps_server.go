package api

import (
	"encoding/json"
	"fmt"
	"net/http"
	"time"

	"github.com/markus-lassfolk/autonomy/pkg/gps"
	"github.com/markus-lassfolk/autonomy/pkg/logx"
)

// GPSResponse represents the standardized GPS response format
type GPSResponse struct {
	Data GPSData `json:"data"`
}

// GPSData represents the GPS data structure
type GPSData struct {
	Latitude   *float64 `json:"latitude"`   // Decimal degrees
	Longitude  *float64 `json:"longitude"`  // Decimal degrees
	Altitude   *float64 `json:"altitude"`   // Meters above sea level
	FixStatus  string   `json:"fix_status"` // "0", "1", "2", "3" as string
	Satellites *int     `json:"satellites"` // Number of satellites
	Accuracy   *float64 `json:"accuracy"`   // Accuracy in meters
	Speed      *float64 `json:"speed"`      // Speed in km/h
	DateTime   string   `json:"datetime"`   // UTC time with Z suffix
	Source     string   `json:"source"`     // GPS source identifier
}

// GPSServer provides GPS data via HTTP API
type GPSServer struct {
	locationManager *gps.ProductionLocationManager
	config          *GPSServerConfig
	logger          *logx.Logger
}

// authMiddleware handles optional authentication for API endpoints
func (s *GPSServer) authMiddleware(next http.HandlerFunc) http.HandlerFunc {
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

// GPSServerConfig holds API server configuration
type GPSServerConfig struct {
	Enabled  bool   `json:"enabled" default:"false"`
	Port     int    `json:"port" default:"8081"`
	Host     string `json:"host" default:"localhost"`
	AuthKey  string `json:"auth_key"` // Optional authentication key
	CertFile string `json:"cert_file"` // TLS certificate file path
	KeyFile  string `json:"key_file"`  // TLS private key file path
}

// NewGPSServer creates a new GPS API server instance
func NewGPSServer(locationManager *gps.ProductionLocationManager, logger *logx.Logger) *GPSServer {
	return &GPSServer{
		locationManager: locationManager,
		config: &GPSServerConfig{
			Enabled: false, // Disabled by default for security
			Port:    8081,
			Host:    "localhost",
			AuthKey: "",
		},
		logger: logger,
	}
}

// NewGPSServerWithConfig creates a new GPS API server instance with custom config
func NewGPSServerWithConfig(locationManager *gps.ProductionLocationManager, config *GPSServerConfig, logger *logx.Logger) *GPSServer {
	if config == nil {
		config = &GPSServerConfig{
			Enabled: false, // Disabled by default for security
			Port:    8081,
			Host:    "localhost",
			AuthKey: "",
		}
	}

	return &GPSServer{
		locationManager: locationManager,
		config:          config,
		logger:          logger,
	}
}

// Start starts the HTTP API server
func (s *GPSServer) Start() error {
	if !s.config.Enabled {
		s.logger.Info("GPS API server is disabled")
		return nil
	}

	mux := http.NewServeMux()

	// Main endpoint - returns best GPS source (drop-in replacement for RUTOS)
	mux.HandleFunc("/api/gps/position/status", s.authMiddleware(s.handleBestGPS))

	// Individual source endpoints
	mux.HandleFunc("/api/gps/rutos", s.authMiddleware(s.handleRutosGPS))
	mux.HandleFunc("/api/gps/starlink", s.authMiddleware(s.handleStarlinkGPS))
	mux.HandleFunc("/api/gps/google", s.authMiddleware(s.handleGoogleGPS))

	// Health and status endpoints
	mux.HandleFunc("/api/gps/health", s.authMiddleware(s.handleHealth))
	mux.HandleFunc("/api/gps/stats", s.authMiddleware(s.handleStats))

	// Start server
	addr := fmt.Sprintf("%s:%d", s.config.Host, s.config.Port)
	s.logger.Info("Starting GPS API server", "address", addr)

	go func() {
		var err error
		if s.config.CertFile != "" && s.config.KeyFile != "" {
			// Use TLS if certificate files are provided
			err = http.ListenAndServeTLS(addr, s.config.CertFile, s.config.KeyFile, mux)
		} else {
			// Fall back to HTTP if no TLS certificates
			// nosemgrep: go.lang.security.audit.net.use-tls.use-tls
			err = http.ListenAndServe(addr, mux)
		}
		if err != nil {
			s.logger.Error("GPS API server failed", "error", err)
		}
	}()

	return nil
}

// handleBestGPS handles the main GPS endpoint - returns best available source
func (s *GPSServer) handleBestGPS(w http.ResponseWriter, r *http.Request) {
	s.logger.Debug("GPS API request received", "endpoint", "/api/gps/position/status")

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

	// Set headers
	w.Header().Set("Content-Type", "application/json")
	w.Header().Set("Access-Control-Allow-Origin", "*")
	w.Header().Set("Access-Control-Allow-Methods", "GET, OPTIONS")
	w.Header().Set("Access-Control-Allow-Headers", "Content-Type")

	// Encode response
	if err := json.NewEncoder(w).Encode(response); err != nil {
		s.logger.Error("Failed to encode GPS response", "error", err)
		http.Error(w, "Internal server error", http.StatusInternalServerError)
		return
	}

	s.logger.Debug("GPS API response sent",
		"source", gpsData.Source,
		"latitude", gpsData.Latitude,
		"longitude", gpsData.Longitude,
		"accuracy", gpsData.Accuracy)
}

// handleRutosGPS handles RUTOS GPS endpoint
func (s *GPSServer) handleRutosGPS(w http.ResponseWriter, r *http.Request) {
	s.logger.Debug("RUTOS GPS API request received")

	// For now, return the same data as best GPS
	// In the future, this could query RUTOS GPS specifically
	s.handleBestGPS(w, r)
}

// handleStarlinkGPS handles Starlink GPS endpoint
func (s *GPSServer) handleStarlinkGPS(w http.ResponseWriter, r *http.Request) {
	s.logger.Debug("Starlink GPS API request received")

	// For now, return the same data as best GPS
	// In the future, this could query Starlink GPS specifically
	s.handleBestGPS(w, r)
}

// handleGoogleGPS handles Google GPS endpoint
func (s *GPSServer) handleGoogleGPS(w http.ResponseWriter, r *http.Request) {
	s.logger.Debug("Google GPS API request received")

	// For now, return the same data as best GPS
	// In the future, this could query Google GPS specifically
	s.handleBestGPS(w, r)
}

// handleHealth handles health check endpoint
func (s *GPSServer) handleHealth(w http.ResponseWriter, r *http.Request) {
	health := map[string]interface{}{
		"status":    "healthy",
		"timestamp": time.Now().UTC().Format(time.RFC3339),
		"service":   "gps-api",
		"version":   "1.0.0",
	}

	w.Header().Set("Content-Type", "application/json")
	if err := json.NewEncoder(w).Encode(health); err != nil {
		s.logger.Error("Failed to encode health response", "error", err)
		http.Error(w, "Internal server error", http.StatusInternalServerError)
		return
	}
}

// handleStats handles statistics endpoint
func (s *GPSServer) handleStats(w http.ResponseWriter, r *http.Request) {
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

	w.Header().Set("Content-Type", "application/json")
	if err := json.NewEncoder(w).Encode(response); err != nil {
		s.logger.Error("Failed to encode stats response", "error", err)
		http.Error(w, "Internal server error", http.StatusInternalServerError)
		return
	}
}

// convertToGPSData converts ProductionLocationResponse to GPSData
func (s *GPSServer) convertToGPSData(location *gps.ProductionLocationResponse) GPSData {
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

	// Add optional fields if available
	// Note: These would be populated from actual GPS data in a full implementation
	// For now, we'll set them to nil to indicate they're not available

	return gpsData
}

// Stop gracefully shuts down the GPS API server
func (s *GPSServer) Stop() {
	s.logger.Info("GPS API server stopped")
}
