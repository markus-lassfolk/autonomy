package main

import (
	"encoding/json"
	"fmt"
	"log"
	"net/http"
	"strconv"
	"time"

	"golang.org/x/crypto/ssh"
)

// RutosGPSResponse mimics the RUTOS GPS API response format
type RutosGPSResponse struct {
	Data RutosGPSData `json:"data"`
}

// RutosGPSData matches the expected RUTOS GPS data structure
type RutosGPSData struct {
	Latitude   *float64 `json:"latitude"`   // Decimal degrees
	Longitude  *float64 `json:"longitude"`  // Decimal degrees
	Altitude   *float64 `json:"altitude"`   // Meters above sea level
	FixStatus  string   `json:"fix_status"` // "0", "1", "2", "3" as string
	Satellites *int     `json:"satellites"` // Number of satellites
	Accuracy   *float64 `json:"accuracy"`   // HDOP in meters
	Speed      *float64 `json:"speed"`      // Speed in km/h
	DateTime   string   `json:"datetime"`   // UTC time with Z suffix
	Source     string   `json:"source"`     // GPS source identifier
}

// autonomyAPIServer provides GPS data via HTTP API
type autonomyAPIServer struct {
	sshClient     *ssh.Client
	config        APIServerConfig
	configManager *UCIAPIConfigManager
}

// NewautonomyAPIServer creates a new API server instance
func NewautonomyAPIServer(sshClient *ssh.Client) *autonomyAPIServer {
	configManager := NewUCIAPIConfigManager(sshClient)
	return &autonomyAPIServer{
		sshClient:     sshClient,
		configManager: configManager,
		config:        DefaultAPIServerConfig(),
	}
}

// NewautonomyAPIServerWithConfig creates a new API server instance with custom config
func NewautonomyAPIServerWithConfig(sshClient *ssh.Client, config APIServerConfig) *autonomyAPIServer {
	configManager := NewUCIAPIConfigManager(sshClient)
	return &autonomyAPIServer{
		sshClient:     sshClient,
		configManager: configManager,
		config:        config,
	}
}

// LoadConfig loads configuration from UCI
func (s *autonomyAPIServer) LoadConfig() error {
	return s.configManager.LoadConfig()
}

// Start starts the HTTP API server with UCI configuration
func (s *autonomyAPIServer) Start() error {
	// Load configuration from UCI
	if err := s.configManager.LoadConfig(); err != nil {
		fmt.Printf("⚠️  Failed to load UCI config, using defaults: %v\n", err)
	}
	s.config = s.configManager.GetConfig()

	// Check if API server is enabled
	if !s.config.Enabled {
		fmt.Println("ℹ️  API server is disabled in UCI configuration")
		return nil
	}

	// Check port availability
	if err := s.configManager.CheckPortAvailability(); err != nil {
		fmt.Printf("❌ Port availability check failed: %v\n", err)

		// Try to find an available port
		if availablePort, portErr := s.configManager.FindAvailablePort(); portErr == nil {
			fmt.Printf("💡 Using available port %d instead of %d\n", availablePort, s.config.Port)
			s.config.Port = availablePort
		} else {
			return fmt.Errorf("no available ports found: %v", portErr)
		}
	}

	mux := http.NewServeMux()

	// Main endpoint - returns best GPS source (drop-in replacement for RUTOS)
	mux.HandleFunc("/api/gps/position/status", s.handleBestGPS)

	// Individual source endpoints
	mux.HandleFunc("/api/gps/rutos", s.handleRutosGPS)
	mux.HandleFunc("/api/gps/starlink", s.handleStarlinkGPS)
	mux.HandleFunc("/api/gps/google", s.handleGoogleGPS)

	// Health check endpoint (configurable path)
	mux.HandleFunc(s.config.HealthCheckPath, s.handleHealth)

	// Status endpoint showing all sources
	mux.HandleFunc("/api/gps/all", s.handleAllSources)

	// Configuration endpoint
	mux.HandleFunc("/api/config", s.handleConfig)

	addr := fmt.Sprintf("%s:%d", s.config.BindAddress, s.config.Port)
	fmt.Printf("🌐 autonomy GPS API Server starting on %s\n", addr)
	fmt.Printf("📍 Main endpoint: http://%s/api/gps/position/status\n", addr)
	fmt.Printf("🔧 Individual sources:\n")
	fmt.Printf("   • RUTOS:    http://%s/api/gps/rutos\n", addr)
	fmt.Printf("   • Starlink: http://%s/api/gps/starlink\n", addr)
	fmt.Printf("   • Google:   http://%s/api/gps/google\n", addr)
	fmt.Printf("   • All:      http://%s/api/gps/all\n", addr)
	fmt.Printf("   • Health:   http://%s%s\n", addr, s.config.HealthCheckPath)
	fmt.Printf("   • Config:   http://%s/api/config\n", addr)

	// Create server with timeouts
	server := &http.Server{
		Addr:         addr,
		Handler:      mux,
		ReadTimeout:  time.Duration(s.config.RequestTimeout) * time.Second,
		WriteTimeout: time.Duration(s.config.RequestTimeout) * time.Second,
		IdleTimeout:  60 * time.Second,
	}

	return server.ListenAndServe()
}

// setCORSHeaders sets CORS headers if enabled
func (s *autonomyAPIServer) setCORSHeaders(w http.ResponseWriter) {
	if s.config.EnableCORS {
		w.Header().Set("Access-Control-Allow-Origin", "*")
		w.Header().Set("Access-Control-Allow-Methods", "GET, POST, OPTIONS")
		w.Header().Set("Access-Control-Allow-Headers", "Content-Type")
	}
}

// logRequest logs API requests if enabled
func (s *autonomyAPIServer) logRequest(r *http.Request, endpoint string) {
	if s.config.LogRequests {
		fmt.Printf("📡 API Request: %s %s from %s\n", r.Method, endpoint, r.RemoteAddr)
	}
}

// handleBestGPS returns the best available GPS source in RUTOS format
func (s *autonomyAPIServer) handleBestGPS(w http.ResponseWriter, r *http.Request) {
	s.logRequest(r, "/api/gps/position/status")
	w.Header().Set("Content-Type", "application/json")
	s.setCORSHeaders(w)

	// Collect data from all sources
	rutosData := s.collectRutosData()
	starlinkData := s.collectStarlinkData()
	googleData := s.collectGoogleData()

	// Select best source based on accuracy and availability
	bestData := s.selectBestGPSSource(rutosData, starlinkData, googleData)

	response := RutosGPSResponse{Data: bestData}

	if err := json.NewEncoder(w).Encode(response); err != nil {
		http.Error(w, "Failed to encode response", http.StatusInternalServerError)
		return
	}
}

// handleRutosGPS returns RUTOS GPS data
func (s *autonomyAPIServer) handleRutosGPS(w http.ResponseWriter, r *http.Request) {
	s.logRequest(r, "/api/gps/rutos")
	w.Header().Set("Content-Type", "application/json")
	s.setCORSHeaders(w)

	data := s.collectRutosData()
	response := RutosGPSResponse{Data: data}

	json.NewEncoder(w).Encode(response)
}

// handleStarlinkGPS returns Starlink GPS data
func (s *autonomyAPIServer) handleStarlinkGPS(w http.ResponseWriter, r *http.Request) {
	s.logRequest(r, "/api/gps/starlink")
	w.Header().Set("Content-Type", "application/json")
	s.setCORSHeaders(w)

	data := s.collectStarlinkData()
	response := RutosGPSResponse{Data: data}

	json.NewEncoder(w).Encode(response)
}

// handleGoogleGPS returns Google GPS data
func (s *autonomyAPIServer) handleGoogleGPS(w http.ResponseWriter, r *http.Request) {
	s.logRequest(r, "/api/gps/google")
	w.Header().Set("Content-Type", "application/json")
	s.setCORSHeaders(w)

	data := s.collectGoogleData()
	response := RutosGPSResponse{Data: data}

	json.NewEncoder(w).Encode(response)
}

// handleHealth returns server health status
func (s *autonomyAPIServer) handleHealth(w http.ResponseWriter, r *http.Request) {
	s.logRequest(r, s.config.HealthCheckPath)
	w.Header().Set("Content-Type", "application/json")
	s.setCORSHeaders(w)

	health := map[string]interface{}{
		"status":    "healthy",
		"timestamp": time.Now().UTC().Format("2006-01-02T15:04:05Z"),
		"sources": map[string]bool{
			"rutos":    s.sshClient != nil,
			"starlink": true, // Simulated for now
			"google":   true, // Simulated for now
		},
	}

	json.NewEncoder(w).Encode(health)
}

// handleAllSources returns data from all GPS sources
func (s *autonomyAPIServer) handleAllSources(w http.ResponseWriter, r *http.Request) {
	s.logRequest(r, "/api/gps/all")
	w.Header().Set("Content-Type", "application/json")
	s.setCORSHeaders(w)

	allSources := map[string]RutosGPSData{
		"rutos":    s.collectRutosData(),
		"starlink": s.collectStarlinkData(),
		"google":   s.collectGoogleData(),
	}

	json.NewEncoder(w).Encode(allSources)
}

// handleConfig returns or updates server configuration
func (s *autonomyAPIServer) handleConfig(w http.ResponseWriter, r *http.Request) {
	s.logRequest(r, "/api/config")
	w.Header().Set("Content-Type", "application/json")
	s.setCORSHeaders(w)

	switch r.Method {
	case "GET":
		// Return current configuration
		config := map[string]interface{}{
			"api_server": s.config,
			"endpoints": map[string]string{
				"best_gps": "/api/gps/position/status",
				"rutos":    "/api/gps/rutos",
				"starlink": "/api/gps/starlink",
				"google":   "/api/gps/google",
				"all":      "/api/gps/all",
				"health":   s.config.HealthCheckPath,
				"config":   "/api/config",
			},
		}
		json.NewEncoder(w).Encode(config)

	case "POST":
		// Update configuration (future enhancement)
		http.Error(w, "Configuration updates not implemented", http.StatusNotImplemented)

	default:
		http.Error(w, "Method not allowed", http.StatusMethodNotAllowed)
	}
}

// collectRutosData collects GPS data from RUTOS
func (s *autonomyAPIServer) collectRutosData() RutosGPSData {
	// Get real RUTOS GPS data
	lat, lon, alt, err1 := s.getGPSCtlCoordinates()
	atData, err2 := s.getATCommandData()
	gpsctlDetails, err3 := s.getGPSCtlDetails()

	if err1 != nil && err2 != nil {
		// Return error state
		return RutosGPSData{
			FixStatus: "0",
			DateTime:  time.Now().UTC().Format("2006-01-02T15:04:05Z"),
			Source:    "RUTOS (No Fix)",
		}
	}

	// Use best accuracy available
	var accuracy *float64
	if err3 == nil && gpsctlDetails.Accuracy > 0 {
		accuracy = &gpsctlDetails.Accuracy
	} else if err2 == nil {
		hdop := atData.HDOP * 1.0 // Conservative estimate
		accuracy = &hdop
	}

	// Use best satellite count
	var satellites *int
	if err2 == nil {
		satellites = &atData.Satellites
	}

	// Convert speed from knots to km/h
	var speedKmh *float64
	if err3 == nil {
		// gpsctl speed is in m/s, convert to km/h
		speed := gpsctlDetails.Speed * 3.6
		speedKmh = &speed
	} else if err2 == nil {
		// AT command speed is in knots, convert to km/h
		speed := atData.SpeedKnots * 1.852
		speedKmh = &speed
	}

	// Get fix type
	fixType := "0"
	if err2 == nil {
		fixType = strconv.Itoa(atData.FixType)
	}

	return RutosGPSData{
		Latitude:   &lat,
		Longitude:  &lon,
		Altitude:   &alt,
		FixStatus:  fixType,
		Satellites: satellites,
		Accuracy:   accuracy,
		Speed:      speedKmh,
		DateTime:   time.Now().UTC().Format("2006-01-02T15:04:05Z"),
		Source:     "RUTOS Combined",
	}
}

// collectStarlinkData collects GPS data from Starlink (simulated)
func (s *autonomyAPIServer) collectStarlinkData() RutosGPSData {
	// In production, this would query actual Starlink APIs
	lat := 59.48005181
	lon := 18.27987656
	alt := 21.5
	accuracy := 5.0
	satellites := 14
	speed := 0.0 // km/h

	return RutosGPSData{
		Latitude:   &lat,
		Longitude:  &lon,
		Altitude:   &alt,
		FixStatus:  "3",
		Satellites: &satellites,
		Accuracy:   &accuracy,
		Speed:      &speed,
		DateTime:   time.Now().UTC().Format("2006-01-02T15:04:05Z"),
		Source:     "Starlink Multi-API",
	}
}

// collectGoogleData collects GPS data from Google (simulated)
func (s *autonomyAPIServer) collectGoogleData() RutosGPSData {
	// In production, this would query actual Google Geolocation API
	lat := 59.47982600
	lon := 18.27992100
	alt := 6.0
	accuracy := 45.0

	return RutosGPSData{
		Latitude:  &lat,
		Longitude: &lon,
		Altitude:  &alt,
		FixStatus: "1", // Network-based fix
		Accuracy:  &accuracy,
		DateTime:  time.Now().UTC().Format("2006-01-02T15:04:05Z"),
		Source:    "Google Geolocation",
	}
}

// selectBestGPSSource selects the best GPS source based on accuracy and availability
func (s *autonomyAPIServer) selectBestGPSSource(rutos, starlink, google RutosGPSData) RutosGPSData {
	// Priority order: RUTOS (most accurate) > Starlink > Google

	// Check RUTOS first (highest accuracy)
	if rutos.FixStatus != "0" && rutos.Latitude != nil && rutos.Longitude != nil {
		rutos.Source = "RUTOS (Best)"
		return rutos
	}

	// Check Starlink second
	if starlink.FixStatus != "0" && starlink.Latitude != nil && starlink.Longitude != nil {
		starlink.Source = "Starlink (Fallback)"
		return starlink
	}

	// Use Google as last resort
	if google.FixStatus != "0" && google.Latitude != nil && google.Longitude != nil {
		google.Source = "Google (Last Resort)"
		return google
	}

	// No GPS available
	return RutosGPSData{
		FixStatus: "0",
		DateTime:  time.Now().UTC().Format("2006-01-02T15:04:05Z"),
		Source:    "No GPS Available",
	}
}

// Helper methods (reuse existing GPS collection methods)
func (s *autonomyAPIServer) getGPSCtlCoordinates() (float64, float64, float64, error) {
	// Reuse existing implementation
	sott := &StandardizedOutputTableTest{sshClient: s.sshClient}
	return sott.getGPSCtlCoordinates()
}

func (s *autonomyAPIServer) getATCommandData() (*QuectelGPSData, error) {
	// Reuse existing implementation
	sott := &StandardizedOutputTableTest{sshClient: s.sshClient}
	return sott.getATCommandData()
}

func (s *autonomyAPIServer) getGPSCtlDetails() (*GPSCtlDetails, error) {
	// Reuse existing implementation
	sott := &StandardizedOutputTableTest{sshClient: s.sshClient}
	return sott.getGPSCtlDetails()
}

// testautonomyAPIServer tests the API server
func testautonomyAPIServer() {
	fmt.Println("🌐 autonomy GPS API Server Test")
	fmt.Println("===============================")

	// Create SSH connection
	sshClient, err := createSSHClient()
	if err != nil {
		fmt.Printf("❌ SSH connection failed: %v\n", err)
		return
	}
	defer sshClient.Close()

	// Create and start API server
	server := NewautonomyAPIServer(sshClient)

	fmt.Println("🚀 Starting autonomy GPS API Server...")
	fmt.Println("📝 Node-Red Configuration:")
	fmt.Println("   Change URL from: https://192.168.80.1/api/gps/position/status")
	fmt.Println("   To:              http://localhost:8080/api/gps/position/status")
	fmt.Println("")
	fmt.Println("🔧 Available Endpoints:")
	fmt.Println("   • Best GPS:     /api/gps/position/status")
	fmt.Println("   • RUTOS only:   /api/gps/rutos")
	fmt.Println("   • Starlink only:/api/gps/starlink")
	fmt.Println("   • Google only:  /api/gps/google")
	fmt.Println("   • All sources:  /api/gps/all")
	fmt.Println("   • Health check: /api/health")
	fmt.Println("")
	fmt.Println("⚡ Server running... Press Ctrl+C to stop")

	if err := server.Start(); err != nil {
		log.Fatalf("❌ Server failed to start: %v", err)
	}
}
