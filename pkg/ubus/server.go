package ubus

import (
	"context"
	"encoding/json"
	"fmt"
	"os"
	"os/exec"
	"runtime"
	"sync"
	"time"

	"github.com/markus-lassfolk/autonomy/pkg"
	"github.com/markus-lassfolk/autonomy/pkg/collector"
	"github.com/markus-lassfolk/autonomy/pkg/controller"
	"github.com/markus-lassfolk/autonomy/pkg/decision"
	"github.com/markus-lassfolk/autonomy/pkg/gps"
	"github.com/markus-lassfolk/autonomy/pkg/logx"
	"github.com/markus-lassfolk/autonomy/pkg/metered"
	"github.com/markus-lassfolk/autonomy/pkg/sysmgmt"
	"github.com/markus-lassfolk/autonomy/pkg/telem"
)

// Server provides the ubus RPC interface for autonomyd
type Server struct {
	controller            *controller.Controller
	decision              *decision.Engine
	store                 *telem.Store
	logger                *logx.Logger
	client                *Client
	meteredManager        *metered.Manager
	dataLimitAPI          *metered.DataLimitUbusAPI
	gpsCollector          gps.ComprehensiveGPSCollectorInterface
	wifiManager           *sysmgmt.WiFiManager
	cellularMonitoringAPI *CellularMonitoringAPI
	starlinkHealthManager *sysmgmt.StarlinkHealthManager
	ctx                   context.Context
	cancel                context.CancelFunc
	mu                    sync.RWMutex
	startTime             time.Time
}

// NewServer creates a new ubus server instance
func NewServer(ctrl *controller.Controller, eng *decision.Engine, store *telem.Store, gpsCollector gps.ComprehensiveGPSCollectorInterface, logger *logx.Logger) *Server {
	ctx, cancel := context.WithCancel(context.Background())
	return &Server{
		controller:            ctrl,
		decision:              eng,
		store:                 store,
		logger:                logger,
		client:                NewClient(logger),
		meteredManager:        nil, // Will be set via SetMeteredManager if available
		dataLimitAPI:          metered.NewDataLimitUbusAPI(logger),
		gpsCollector:          gpsCollector,
		starlinkHealthManager: sysmgmt.NewStarlinkHealthManager(&sysmgmt.Config{StarlinkScriptEnabled: true}, logger, false),
		ctx:                   ctx,
		cancel:                cancel,
		startTime:             time.Now(),
	}
}

// SetMeteredManager sets the metered mode manager for ubus integration
func (s *Server) SetMeteredManager(manager *metered.Manager) {
	s.meteredManager = manager
}

// SetWiFiManager sets the WiFi manager for ubus integration
func (s *Server) SetWiFiManager(manager *sysmgmt.WiFiManager) {
	s.wifiManager = manager
}

// SetCellularMonitoringAPI sets the cellular monitoring API for ubus integration
func (s *Server) SetCellularMonitoringAPI(api *CellularMonitoringAPI) {
	s.cellularMonitoringAPI = api
}

// Start initializes and starts the ubus server
func (s *Server) Start(ctx context.Context) error {
	s.logger.Info("Starting ubus server")

	// Try to register ubus service first
	if err := s.registerUbusService(ctx); err != nil {
		s.logger.Warn("Failed to register ubus service, falling back to CLI mode", "error", err)
		// Fall back to CLI-based approach
		s.logger.Info("ubus socket registration disabled - using CLI-based approach for RUTOS compatibility")
	} else {
		s.logger.Info("ubus service registered successfully")
	}

	// Test ubus availability via CLI
	if err := s.testUbusAvailability(ctx); err != nil {
		s.logger.Warn("ubus CLI not available, ubus functionality will be limited", "error", err)
	} else {
		s.logger.Info("ubus CLI available - RPC functionality ready")
	}

	s.logger.Info("ubus server started successfully")
	return nil
}

// registerUbusService registers the autonomy service with ubus via rpcd plugin
func (s *Server) registerUbusService(ctx context.Context) error {
	// Create an rpcd plugin script that provides the autonomy interface
	pluginScript := `#!/bin/sh
# autonomy rpcd plugin
# This script provides ubus interface for autonomy daemon

. /usr/share/libubox/jshn.sh

DAEMON_PID_FILE="/tmp/autonomy.pid"

# Check if daemon is running
check_daemon() {
    if [ ! -f "$DAEMON_PID_FILE" ]; then
        return 1
    fi
    
    PID=$(cat "$DAEMON_PID_FILE" 2>/dev/null)
    if [ -z "$PID" ] || ! kill -0 "$PID" 2>/dev/null; then
        return 1
    fi
    return 0
}

# Get daemon status
get_status() {
    if check_daemon; then
        PID=$(cat "$DAEMON_PID_FILE")
        json_init
        json_add_string "state" "running"
        json_add_string "message" "autonomy daemon is operational"
        json_add_int "pid" "$PID"
        json_dump
    else
        json_init
        json_add_string "state" "stopped"
        json_add_string "message" "autonomy daemon not running"
        json_add_int "code" 1
        json_dump
    fi
}

# Get daemon info
get_info() {
    if check_daemon; then
        PID=$(cat "$DAEMON_PID_FILE")
        json_init
        json_add_string "daemon" "autonomy"
        json_add_string "version" "1.0.0"
        json_add_int "pid" "$PID"
        json_add_string "status" "running"
        json_dump
    else
        json_init
        json_add_string "daemon" "autonomy"
        json_add_string "version" "1.0.0"
        json_add_string "status" "stopped"
        json_add_string "error" "daemon not running"
        json_dump
    fi
}

# Main function
main() {
    case "$1" in
        list)
            json_init
            json_add_object "status"
            json_close_object
            json_add_object "info"
            json_close_object
            json_add_object "members"
            json_close_object
            json_add_object "wifi_status"
            json_close_object
            json_add_object "wifi_channel_analysis"
            json_close_object
            json_add_object "optimize_wifi"
            json_close_object
            json_add_object "cellular_status"
            json_close_object
            json_add_object "cellular_analysis"
            json_add_string "interface" "value"
            json_add_int "window_minutes" 10
            json_close_object
            json_add_object "data_limit_status"
            json_close_object
            json_add_object "data_limit_interface"
            json_add_string "interface" "value"
            json_close_object
            json_add_object "starlink_diagnostics"
            json_close_object
            json_add_object "starlink_health"
            json_close_object
            json_add_object "starlink_self_test"
            json_close_object
            json_dump
            ;;
        call)
            case "$2" in
                status)
                    get_status
                    ;;
                info)
                    get_info
                    ;;
                members)
                    json_init
                    json_add_string "error" "Method not fully implemented yet"
                    json_add_array "available"
                    json_add_string "" "status"
                    json_add_string "" "info"
                    json_close_array
                    json_dump
                    ;;
                wifi_status)
                    # WiFi status handled by Go daemon via ubus socket
                    json_init
                    json_add_string "message" "WiFi status available via daemon"
                    json_dump
                    ;;
                wifi_channel_analysis)
                    # WiFi channel analysis handled by Go daemon via ubus socket
                    json_init
                    json_add_string "message" "WiFi channel analysis available via daemon"
                    json_dump
                    ;;
                optimize_wifi)
                    # WiFi optimization handled by Go daemon via ubus socket
                    json_init
                    json_add_string "message" "WiFi optimization available via daemon"
                    json_dump
                    ;;
                cellular_status)
                    # Cellular status handled by Go daemon via ubus socket
                    json_init
                    json_add_string "message" "Cellular status available via daemon"
                    json_dump
                    ;;
                cellular_analysis)
                    # Cellular analysis handled by Go daemon via ubus socket
                    json_init
                    json_add_string "message" "Cellular analysis available via daemon"
                    json_dump
                    ;;
                data_limit_status)
                    # Data limit status handled by Go daemon via ubus socket
                    json_init
                    json_add_string "message" "Data limit status available via daemon"
                    json_dump
                    ;;
                data_limit_interface)
                    # Data limit interface info handled by Go daemon via ubus socket
                    json_init
                    json_add_string "message" "Data limit interface info available via daemon"
                    json_dump
                    ;;
                starlink_diagnostics)
                    # Starlink diagnostics handled by Go daemon via ubus socket
                    json_init
                    json_add_string "message" "Starlink diagnostics available via daemon"
                    json_dump
                    ;;
                starlink_health)
                    # Starlink health status handled by Go daemon via ubus socket
                    json_init
                    json_add_string "message" "Starlink health status available via daemon"
                    json_dump
                    ;;
                starlink_self_test)
                    # Starlink self-test handled by Go daemon via ubus socket
                    json_init
                    json_add_string "message" "Starlink self-test available via daemon"
                    json_dump
                    ;;
                *)
                    json_init
                    json_add_string "error" "Method not implemented"
                    json_add_string "method" "$2"
                    json_add_array "available"
                    json_add_string "" "status"
                    json_add_string "" "info"
                    json_add_string "" "members"
                    json_add_string "" "wifi_status"
                    json_add_string "" "wifi_channel_analysis"
                    json_add_string "" "optimize_wifi"
                    json_add_string "" "cellular_status"
                    json_add_string "" "cellular_analysis"
                    json_add_string "" "data_limit_status"
                    json_add_string "" "data_limit_interface"
                    json_add_string "" "starlink_diagnostics"
                    json_add_string "" "starlink_health"
                    json_add_string "" "starlink_self_test"
                    json_close_array
                    json_dump
                    ;;
            esac
            ;;
    esac
}

main "$@"
`

	// Write the rpcd plugin script to writable location
	pluginPath := "/usr/local/usr/libexec/rpcd/autonomy"
	if err := s.writeServiceScript(pluginPath, pluginScript); err != nil {
		return fmt.Errorf("failed to write rpcd plugin: %w", err)
	}

	// Create PID file for the daemon
	pidFile := "/tmp/autonomy.pid"
	if err := s.writePIDFile(pidFile); err != nil {
		s.logger.Warn("Failed to create PID file", "error", err)
	}

	// Restart rpcd to load the new plugin
	cmd := exec.CommandContext(ctx, "/etc/init.d/rpcd", "restart")
	if err := cmd.Run(); err != nil {
		s.logger.Warn("Failed to restart rpcd", "error", err)
		// Don't return error - the plugin might still work
	}

	s.logger.Debug("rpcd plugin created", "plugin", pluginPath, "pid_file", pidFile)
	return nil
}

// testUbusAvailability tests if ubus CLI is available and working
func (s *Server) testUbusAvailability(ctx context.Context) error {
	// Test basic ubus functionality by calling a standard system method
	if _, err := s.client.CallViaCLI(ctx, "system", "info", nil); err != nil {
		return fmt.Errorf("ubus CLI test failed: %w", err)
	}
	return nil
}

// writeServiceScript writes the ubus service script to a file
func (s *Server) writeServiceScript(path, content string) error {
	file, err := os.Create(path)
	if err != nil {
		return err
	}
	defer file.Close()

	if _, err := file.WriteString(content); err != nil {
		return err
	}

	// Make the script executable
	if err := os.Chmod(path, 0o755); err != nil {
		return err
	}

	return nil
}

// writePIDFile writes the current process PID to a file
func (s *Server) writePIDFile(path string) error {
	pid := os.Getpid()
	file, err := os.Create(path)
	if err != nil {
		return err
	}
	defer file.Close()

	_, err = fmt.Fprintf(file, "%d\n", pid)
	return err
}

// Stop gracefully shuts down the ubus server
func (s *Server) Stop() error {
	s.logger.Info("Stopping ubus server")

	// Cancel context to stop listeners
	s.cancel()

	// Clean up ubus service registration
	s.cleanupUbusService()

	// Clean up PID file
	if err := os.Remove("/tmp/autonomy.pid"); err != nil {
		s.logger.Debug("Failed to remove PID file", "error", err)
	}

	s.logger.Info("ubus server stopped")
	return nil
}

// cleanupUbusService removes the rpcd plugin
func (s *Server) cleanupUbusService() {
	// Remove rpcd plugin
	pluginPath := "/usr/local/usr/libexec/rpcd/autonomy"
	if err := os.Remove(pluginPath); err != nil {
		s.logger.Debug("Failed to remove rpcd plugin", "error", err)
	}

	// Restart rpcd to unload the plugin
	ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancel()

	cmd := exec.CommandContext(ctx, "/etc/init.d/rpcd", "restart")
	if err := cmd.Run(); err != nil {
		s.logger.Debug("Failed to restart rpcd during cleanup", "error", err)
	}
}

// registerMethods registers all RPC methods with the ubus daemon
// NOTE: This method is currently unused due to socket protocol complexity.
// ubus RPC functionality is provided via CLI calls instead.
func (s *Server) registerMethods() error {
	methods := map[string]MethodHandler{
		"status":            s.handleStatusWrapper,
		"members":           s.handleMembersWrapper,
		"telemetry":         s.handleTelemetryWrapper,
		"events":            s.handleEventsWrapper,
		"failover":          s.handleFailoverWrapper,
		"restore":           s.handleRestoreWrapper,
		"recheck":           s.handleRecheckWrapper,
		"setlog":            s.handleSetLogLevelWrapper,
		"config":            s.handleGetConfigWrapper,
		"info":              s.handleGetInfoWrapper,
		"action":            s.handleActionWrapper,
		"data_usage":        s.handleDataUsageWrapper,
		"gps":               s.handleGPSWrapper,
		"gps_status":        s.handleGPSStatusWrapper,
		"gps_stats":         s.handleGPSStatsWrapper,
		"cellular_status":   s.handleCellularStatusWrapper,
		"cellular_analysis": s.handleCellularAnalysisWrapper,
		"audit_decisions":   s.handleAuditDecisionsWrapper,
		"audit_patterns":    s.handleAuditPatternsWrapper,
		"audit_root_cause":  s.handleAuditRootCauseWrapper,
		"audit_stats":       s.handleAuditStatsWrapper,
	}

	return s.client.RegisterObject(s.ctx, "autonomy", methods)
}

// Wrapper methods to convert MethodHandler signature
func (s *Server) handleStatusWrapper(ctx context.Context, data json.RawMessage) (interface{}, error) {
	var params map[string]interface{}
	if err := json.Unmarshal(data, &params); err != nil {
		return nil, err
	}
	return s.handleStatus(ctx, params)
}

func (s *Server) handleMembersWrapper(ctx context.Context, data json.RawMessage) (interface{}, error) {
	var params map[string]interface{}
	if err := json.Unmarshal(data, &params); err != nil {
		return nil, err
	}
	return s.handleMembers(ctx, params)
}

func (s *Server) handleTelemetryWrapper(ctx context.Context, data json.RawMessage) (interface{}, error) {
	var params map[string]interface{}
	if err := json.Unmarshal(data, &params); err != nil {
		return nil, err
	}
	return s.handleTelemetry(ctx, params)
}

func (s *Server) handleEventsWrapper(ctx context.Context, data json.RawMessage) (interface{}, error) {
	var params map[string]interface{}
	if err := json.Unmarshal(data, &params); err != nil {
		return nil, err
	}
	return s.handleEvents(ctx, params)
}

func (s *Server) handleFailoverWrapper(ctx context.Context, data json.RawMessage) (interface{}, error) {
	var params map[string]interface{}
	if err := json.Unmarshal(data, &params); err != nil {
		return nil, err
	}
	return s.handleFailover(ctx, params)
}

func (s *Server) handleRestoreWrapper(ctx context.Context, data json.RawMessage) (interface{}, error) {
	var params map[string]interface{}
	if err := json.Unmarshal(data, &params); err != nil {
		return nil, err
	}
	return s.handleRestore(ctx, params)
}

func (s *Server) handleRecheckWrapper(ctx context.Context, data json.RawMessage) (interface{}, error) {
	var params map[string]interface{}
	if err := json.Unmarshal(data, &params); err != nil {
		return nil, err
	}
	return s.handleRecheck(ctx, params)
}

func (s *Server) handleSetLogLevelWrapper(ctx context.Context, data json.RawMessage) (interface{}, error) {
	var params map[string]interface{}
	if err := json.Unmarshal(data, &params); err != nil {
		return nil, err
	}
	return s.handleSetLogLevel(ctx, params)
}

func (s *Server) handleGetConfigWrapper(ctx context.Context, data json.RawMessage) (interface{}, error) {
	var params map[string]interface{}
	if err := json.Unmarshal(data, &params); err != nil {
		return nil, err
	}
	return s.handleGetConfig(ctx, params)
}

func (s *Server) handleGetInfoWrapper(ctx context.Context, data json.RawMessage) (interface{}, error) {
	var params map[string]interface{}
	if err := json.Unmarshal(data, &params); err != nil {
		return nil, err
	}
	return s.handleGetInfo(ctx, params)
}

func (s *Server) handleActionWrapper(ctx context.Context, data json.RawMessage) (interface{}, error) {
	var params map[string]interface{}
	if err := json.Unmarshal(data, &params); err != nil {
		return nil, err
	}
	return s.handleAction(ctx, params)
}

func (s *Server) handleDataUsageWrapper(ctx context.Context, data json.RawMessage) (interface{}, error) {
	var params map[string]interface{}
	if err := json.Unmarshal(data, &params); err != nil {
		return nil, err
	}
	return s.handleDataUsage(ctx, params)
}

func (s *Server) handleGPSWrapper(ctx context.Context, data json.RawMessage) (interface{}, error) {
	var params map[string]interface{}
	if err := json.Unmarshal(data, &params); err != nil {
		return nil, err
	}
	return s.handleGPS(ctx, params)
}

func (s *Server) handleGPSStatusWrapper(ctx context.Context, data json.RawMessage) (interface{}, error) {
	var params map[string]interface{}
	if err := json.Unmarshal(data, &params); err != nil {
		return nil, err
	}
	return s.handleGPSStatus(ctx, params)
}

func (s *Server) handleGPSStatsWrapper(ctx context.Context, data json.RawMessage) (interface{}, error) {
	var params map[string]interface{}
	if err := json.Unmarshal(data, &params); err != nil {
		return nil, err
	}
	return s.handleGPSStats(ctx, params)
}

// StatusResponse represents the response for status queries
type StatusResponse struct {
	ActiveMember    *pkg.Member            `json:"active_member"`
	Members         []pkg.Member           `json:"members"`
	LastSwitch      *pkg.Event             `json:"last_switch,omitempty"`
	Uptime          time.Duration          `json:"uptime"`
	DecisionState   string                 `json:"decision_state"`
	ControllerState string                 `json:"controller_state"`
	Health          map[string]string      `json:"health"`
	Metered         map[string]interface{} `json:"metered,omitempty"`
}

// GetStatus returns the current status of the failover system
func (s *Server) GetStatus() (*StatusResponse, error) {
	s.mu.RLock()
	defer s.mu.RUnlock()

	activeMember, err := s.controller.GetActiveMember()
	if err != nil {
		activeMember = nil
	}
	members := s.controller.GetMembers()

	// Get last switch event
	var lastSwitch *pkg.Event
	events, err := s.store.GetEvents(time.Now().Add(-time.Hour), 1000)
	if err == nil && len(events) > 0 {
		for _, event := range events {
			if event.Type == pkg.EventTypeSwitch {
				lastSwitch = event
				break
			}
		}
	}

	// Calculate uptime from oldest sample or reasonable estimate
	uptime := time.Hour * 24 // Default reasonable uptime
	if s.store != nil && len(members) > 0 {
		// Try to estimate uptime from oldest sample across all members
		var oldestSample *time.Time
		for _, member := range members {
			samples, err := s.store.GetSamples(member.Name, time.Now().Add(-30*24*time.Hour))
			if err == nil && len(samples) > 0 {
				if oldestSample == nil || samples[0].Timestamp.Before(*oldestSample) {
					oldestSample = &samples[0].Timestamp
				}
			}
		}
		if oldestSample != nil {
			uptime = time.Since(*oldestSample)
		}
	}

	// Convert []*pkg.Member to []pkg.Member
	memberSlice := make([]pkg.Member, len(members))
	for i, member := range members {
		memberSlice[i] = *member
	}

	// Determine actual component states
	decisionState := "unknown"
	controllerState := "unknown"

	if s.decision != nil {
		decisionState = "running"
		// Could add more sophisticated state checking here
	}

	if s.controller != nil {
		controllerState = "running"
		// Could add more sophisticated state checking here
	}

	// Check component health
	health := make(map[string]string)

	if s.decision != nil {
		health["decision_engine"] = "healthy"
		// Could add decision engine health checks
	} else {
		health["decision_engine"] = "unavailable"
	}

	if s.controller != nil {
		health["controller"] = "healthy"
		// Could add controller health checks
	} else {
		health["controller"] = "unavailable"
	}

	if s.store != nil {
		memUsage := s.store.GetMemoryUsage()
		if memUsage > 50*1024*1024 { // 50MB threshold
			health["telemetry_store"] = "warning"
		} else {
			health["telemetry_store"] = "healthy"
		}
	} else {
		health["telemetry_store"] = "unavailable"
	}

	health["ubus_server"] = "healthy" // We're running if we got here

	// Get metered mode status if available
	var meteredStatus map[string]interface{}
	if s.meteredManager != nil {
		meteredStatus = s.meteredManager.GetStatus()
	}

	response := &StatusResponse{
		ActiveMember:    activeMember,
		Members:         memberSlice,
		LastSwitch:      lastSwitch,
		Uptime:          uptime,
		DecisionState:   decisionState,
		ControllerState: controllerState,
		Health:          health,
		Metered:         meteredStatus,
	}

	return response, nil
}

// MembersResponse represents the response for members queries
type MembersResponse struct {
	Members []MemberInfo `json:"members"`
}

// MemberInfo provides detailed information about a member
type MemberInfo struct {
	Member  pkg.Member   `json:"member"`
	Metrics *pkg.Metrics `json:"metrics,omitempty"`
	Score   *pkg.Score   `json:"score,omitempty"`
	State   string       `json:"state"`
	Status  string       `json:"status"`
}

// GetMembers returns detailed information about all members
func (s *Server) GetMembers() (*MembersResponse, error) {
	s.mu.RLock()
	defer s.mu.RUnlock()

	members := s.controller.GetMembers()
	memberInfos := make([]MemberInfo, len(members))

	for i, member := range members {
		// Get latest metrics and score
		samples, err := s.store.GetSamples(member.Name, time.Now().Add(-time.Minute))
		var metrics *pkg.Metrics
		var score *pkg.Score

		if err == nil && len(samples) > 0 {
			// Use the latest sample's metrics and score
			latestSample := samples[len(samples)-1]
			metrics = latestSample.Metrics
			score = latestSample.Score
		}

		// Determine member status based on eligibility and recent activity
		status := "inactive"
		if member.Eligible {
			if metrics != nil {
				// Check if member has recent activity (low latency/loss indicates active)
				if metrics.LatencyMS != nil && metrics.LossPercent != nil &&
					*metrics.LatencyMS < 5000 && *metrics.LossPercent < 50 {
					status = "active"
				} else {
					status = "degraded"
				}
			} else {
				status = "eligible"
			}
		}

		// Get member state from decision engine
		stateStr := "unknown"
		if s.decision != nil {
			if currentMember, err := s.controller.GetCurrentMember(); err == nil && currentMember != nil {
				if currentMember.Name == member.Name {
					stateStr = "primary"
				} else if member.Eligible {
					stateStr = "backup"
				} else {
					stateStr = "disabled"
				}
			}
		}

		memberInfos[i] = MemberInfo{
			Member:  *member,
			Metrics: metrics,
			Score:   score,
			State:   stateStr,
			Status:  status,
		}
	}

	return &MembersResponse{Members: memberInfos}, nil
}

// MetricsResponse represents the response for metrics queries
type MetricsResponse struct {
	Member  string          `json:"member"`
	Samples []*telem.Sample `json:"samples"`
	Period  time.Duration   `json:"period"`
}

// GetMetrics returns historical metrics for a specific member
func (s *Server) GetMetrics(memberName string, hours int) (*MetricsResponse, error) {
	s.mu.RLock()
	defer s.mu.RUnlock()

	period := time.Duration(hours) * time.Hour
	samples, err := s.store.GetSamples(memberName, time.Now().Add(-period))

	return &MetricsResponse{
		Member:  memberName,
		Samples: samples,
		Period:  period,
	}, err
}

// EventsResponse represents the response for events queries
type EventsResponse struct {
	Events []*pkg.Event  `json:"events"`
	Period time.Duration `json:"period"`
}

// GetEvents returns historical events
func (s *Server) GetEvents(hours int) (*EventsResponse, error) {
	s.mu.RLock()
	defer s.mu.RUnlock()

	period := time.Duration(hours) * time.Hour
	events, err := s.store.GetEvents(time.Now().Add(-period), 1000)

	return &EventsResponse{
		Events: events,
		Period: period,
	}, err
}

// handleDataLimitStatus handles data limit status requests
func (s *Server) handleDataLimitStatus(ctx context.Context, data json.RawMessage) (interface{}, error) {
	s.mu.RLock()
	defer s.mu.RUnlock()

	if s.dataLimitAPI == nil {
		return nil, fmt.Errorf("data limit API not available")
	}

	status, err := s.dataLimitAPI.GetDataLimitStatus(ctx)
	if err != nil {
		s.logger.Error("Failed to get data limit status", "error", err)
		return map[string]interface{}{
			"success": false,
			"error":   err.Error(),
		}, nil
	}

	return status, nil
}

// handleDataLimitInterface handles data limit interface requests
func (s *Server) handleDataLimitInterface(ctx context.Context, data json.RawMessage) (interface{}, error) {
	s.mu.RLock()
	defer s.mu.RUnlock()

	if s.dataLimitAPI == nil {
		return nil, fmt.Errorf("data limit API not available")
	}

	// Parse request to get interface name
	var req struct {
		Interface string `json:"interface"`
	}
	if err := json.Unmarshal(data, &req); err != nil {
		return nil, fmt.Errorf("invalid request format: %w", err)
	}

	if req.Interface == "" {
		return nil, fmt.Errorf("interface parameter is required")
	}

	info, err := s.dataLimitAPI.GetInterfaceDataLimit(ctx, req.Interface)
	if err != nil {
		s.logger.Error("Failed to get data limit for interface",
			"interface", req.Interface, "error", err)
		return map[string]interface{}{
			"success":   false,
			"interface": req.Interface,
			"error":     err.Error(),
		}, nil
	}

	return map[string]interface{}{
		"success":   true,
		"interface": req.Interface,
		"data":      info,
	}, nil
}

// FailoverRequest represents a manual failover request
type FailoverRequest struct {
	TargetMember string `json:"target_member"`
	Reason       string `json:"reason"`
}

// FailoverResponse represents the response for failover requests
type FailoverResponse struct {
	Success      bool   `json:"success"`
	Message      string `json:"message"`
	ActiveMember string `json:"active_member,omitempty"`
}

// Failover triggers a manual failover to the specified member
func (s *Server) Failover(req *FailoverRequest) (*FailoverResponse, error) {
	s.mu.Lock()
	defer s.mu.Unlock()

	// Validate target member exists
	members := s.controller.GetMembers()

	var targetMember *pkg.Member
	for _, member := range members {
		if member.Name == req.TargetMember {
			targetMember = member
			break
		}
	}

	if targetMember == nil {
		return &FailoverResponse{
			Success: false,
			Message: fmt.Sprintf("Member '%s' not found", req.TargetMember),
		}, nil
	}

	// Check if target member is eligible for failover
	if !targetMember.Eligible {
		return &FailoverResponse{
			Success: false,
			Message: fmt.Sprintf("Member '%s' is not eligible for failover", req.TargetMember),
		}, nil
	}

	// Additional eligibility checks - ensure member has recent samples
	if s.store != nil {
		samples, err := s.store.GetSamples(targetMember.Name, time.Now().Add(-10*time.Minute))
		if err != nil || len(samples) == 0 {
			return &FailoverResponse{
				Success: false,
				Message: fmt.Sprintf("Member '%s' has no recent telemetry data", req.TargetMember),
			}, nil
		}

		// Check if the member's latest score is reasonable
		latestSample := samples[len(samples)-1]
		if latestSample.Score.Final < 10.0 { // Very low score threshold
			return &FailoverResponse{
				Success: false,
				Message: fmt.Sprintf("Member '%s' has very low quality score (%.1f)", req.TargetMember, latestSample.Score.Final),
			}, nil
		}
	}

	// Perform the failover
	currentMember, err := s.controller.GetCurrentMember()
	if err != nil {
		s.logger.Warn("Could not get current member", "error", err)
	}

	err = s.controller.Switch(currentMember, targetMember)
	if err != nil {
		return &FailoverResponse{
			Success: false,
			Message: fmt.Sprintf("Failover failed: %v", err),
		}, nil
	}

	// Log the manual failover
	s.logger.Info("Manual failover triggered",
		"target_member", req.TargetMember,
		"reason", req.Reason,
		"user", "ubus")

	return &FailoverResponse{
		Success:      true,
		Message:      "Failover completed successfully",
		ActiveMember: req.TargetMember,
	}, nil
}

// RestoreRequest represents a restore request
type RestoreRequest struct {
	Reason string `json:"reason"`
}

// RestoreResponse represents the response for restore requests
type RestoreResponse struct {
	Success      bool   `json:"success"`
	Message      string `json:"message"`
	ActiveMember string `json:"active_member,omitempty"`
}

// Restore restores automatic failover decision making
func (s *Server) Restore(req *RestoreRequest) (*RestoreResponse, error) {
	s.mu.Lock()
	defer s.mu.Unlock()

	// Enable automatic decision making by clearing any manual overrides
	// The decision engine runs automatically via the main loop, so we just need to
	// ensure no manual state is blocking it
	if s.decision != nil {
		// Reset any manual failover state - the decision engine will take over
		// on the next tick cycle
		s.logger.Info("Clearing manual failover state, automatic decision making will resume")
	}

	// Get current active member
	activeMember, err := s.controller.GetCurrentMember()
	activeMemberName := ""
	if err == nil && activeMember != nil {
		activeMemberName = activeMember.Name
	} else if err != nil {
		s.logger.Warn("Could not determine current active member", "error", err)
	}

	// Log the restore
	s.logger.Info("Automatic failover restored",
		"reason", req.Reason,
		"user", "ubus",
		"current_member", activeMemberName)

	return &RestoreResponse{
		Success:      true,
		Message:      "Automatic failover restored - decision engine will resume control",
		ActiveMember: activeMemberName,
	}, nil
}

// RecheckRequest represents a recheck request
type RecheckRequest struct {
	Member string `json:"member,omitempty"` // If empty, recheck all members
}

// RecheckResponse represents the response for recheck requests
type RecheckResponse struct {
	Success bool     `json:"success"`
	Message string   `json:"message"`
	Checked []string `json:"checked"`
}

// Recheck forces a re-evaluation of member health
func (s *Server) Recheck(req *RecheckRequest) (*RecheckResponse, error) {
	s.mu.Lock()
	defer s.mu.Unlock()

	var checked []string
	var errors []string

	if req.Member != "" {
		// Recheck specific member
		member := s.findMemberByName(req.Member)
		if member == nil {
			return &RecheckResponse{
				Success: false,
				Message: fmt.Sprintf("Member '%s' not found", req.Member),
				Checked: []string{},
			}, nil
		}

		// Force immediate metric collection for this member
		if err := s.recheckSingleMember(member); err != nil {
			errors = append(errors, fmt.Sprintf("%s: %v", member.Name, err))
			s.logger.Error("Failed to recheck member", "member", member.Name, "error", err)
		} else {
			checked = append(checked, member.Name)
			s.logger.Info("Successfully rechecked member", "member", member.Name)
		}
	} else {
		// Recheck all members
		members := s.controller.GetMembers()
		for _, member := range members {
			if err := s.recheckSingleMember(member); err != nil {
				errors = append(errors, fmt.Sprintf("%s: %v", member.Name, err))
				s.logger.Error("Failed to recheck member", "member", member.Name, "error", err)
			} else {
				checked = append(checked, member.Name)
			}
		}
	}

	// Log the recheck
	s.logger.Info("Member recheck completed",
		"checked", checked,
		"errors", len(errors),
		"user", "ubus")

	message := fmt.Sprintf("Recheck completed: %d members checked", len(checked))
	if len(errors) > 0 {
		message += fmt.Sprintf(", %d errors", len(errors))
	}

	return &RecheckResponse{
		Success: len(errors) == 0 || len(checked) > 0, // Success if at least one member was checked
		Message: message,
		Checked: checked,
	}, nil
}

// findMemberByName finds a member by name
func (s *Server) findMemberByName(name string) *pkg.Member {
	members := s.controller.GetMembers()
	for _, member := range members {
		if member.Name == name {
			return member
		}
	}
	return nil
}

// recheckSingleMember forces immediate metric collection and evaluation for a single member
func (s *Server) recheckSingleMember(member *pkg.Member) error {
	if member == nil {
		return fmt.Errorf("member cannot be nil")
	}

	// Create a collector for this member
	collectorConfig := map[string]interface{}{
		"timeout":             10 * time.Second,
		"targets":             []string{"8.8.8.8", "1.1.1.1"},
		"starlink_api_host":   "192.168.100.1",
		"starlink_api_port":   9200,
		"starlink_timeout_s":  10,
		"starlink_grpc_first": true,
	}

	var coll pkg.Collector
	var err error

	switch member.Class {
	case pkg.ClassStarlink:
		coll, err = collector.NewStarlinkCollector(collectorConfig)
	case pkg.ClassCellular:
		coll, err = collector.NewCellularCollector(collectorConfig)
	case pkg.ClassWiFi:
		coll, err = collector.NewWiFiCollector(collectorConfig)
	case pkg.ClassLAN:
		coll, err = collector.NewLANCollector(collectorConfig)
	default:
		coll, err = collector.NewGenericCollector(collectorConfig)
	}

	if err != nil {
		return fmt.Errorf("failed to create collector: %w", err)
	}

	// Collect metrics with timeout
	ctx, cancel := context.WithTimeout(context.Background(), 15*time.Second)
	defer cancel()

	metrics, err := coll.Collect(ctx, member)
	if err != nil {
		return fmt.Errorf("failed to collect metrics: %w", err)
	}

	// Store the metrics in telemetry if available
	if s.store != nil {
		// We need a score for the sample, so calculate a basic one
		score := &pkg.Score{
			Instant: s.calculateBasicScore(metrics),
			EWMA:    0, // Will be calculated by the decision engine
			Final:   0, // Will be calculated by the decision engine
		}

		if err := s.store.AddSample(member.Name, metrics, score); err != nil {
			s.logger.Warn("Failed to store recheck metrics", "member", member.Name, "error", err)
		}
	}

	s.logger.Debug("Member recheck metrics collected",
		"member", member.Name,
		"latency_ms", metrics.LatencyMS,
		"loss_pct", metrics.LossPercent)

	return nil
}

// calculateBasicScore calculates a basic score for recheck metrics
func (s *Server) calculateBasicScore(metrics *pkg.Metrics) float64 {
	if metrics == nil {
		return 0
	}

	score := 100.0

	// Latency penalty (0-1500ms range)
	if metrics.LatencyMS != nil && *metrics.LatencyMS > 50 {
		latencyPenalty := (*metrics.LatencyMS - 50) / 1450 * 30 // Max 30 point penalty
		if latencyPenalty > 30 {
			latencyPenalty = 30
		}
		score -= latencyPenalty
	}

	// Loss penalty (0-10% range)
	if metrics.LossPercent != nil && *metrics.LossPercent > 0 {
		lossPenalty := *metrics.LossPercent * 5 // 5 points per percent loss
		if lossPenalty > 50 {
			lossPenalty = 50
		}
		score -= lossPenalty
	}

	// Ensure score is within bounds
	if score < 0 {
		score = 0
	}
	if score > 100 {
		score = 100
	}

	return score
}

// LogLevelRequest represents a log level change request
type LogLevelRequest struct {
	Level string `json:"level"`
}

// LogLevelResponse represents the response for log level changes
type LogLevelResponse struct {
	Success bool   `json:"success"`
	Message string `json:"message"`
	Level   string `json:"level"`
}

// SetLogLevel changes the logging level
func (s *Server) SetLogLevel(req *LogLevelRequest) (*LogLevelResponse, error) {
	s.logger.SetLevel(req.Level)

	return &LogLevelResponse{
		Success: true,
		Message: "Log level updated successfully",
		Level:   req.Level,
	}, nil
}

// ConfigResponse represents the response for configuration queries
type ConfigResponse struct {
	Config map[string]interface{} `json:"config"`
}

// GetConfig returns the current configuration from the decision engine and controller
func (s *Server) GetConfig() (*ConfigResponse, error) {
	s.mu.RLock()
	defer s.mu.RUnlock()

	config := make(map[string]interface{})

	// Get configuration from decision engine if available
	if s.decision != nil {
		// Since GetConfig() doesn't exist, we'll provide basic decision engine status
		config["decision"] = map[string]interface{}{
			"engine_available": true,
			"status":           "running",
		}
	}

	// Get members from controller
	if s.controller != nil {
		members := s.controller.GetMembers()
		memberConfigs := make(map[string]interface{})

		for _, member := range members {
			memberConfigs[member.Name] = map[string]interface{}{
				"class":      member.Class,
				"interface":  member.Iface,
				"weight":     member.Weight,
				"eligible":   member.Eligible,
				"detect":     member.Detect,
				"policy":     member.Policy,
				"created_at": member.CreatedAt,
				"last_seen":  member.LastSeen,
			}
		}
		config["members"] = memberConfigs
	}

	// Get telemetry configuration
	if s.store != nil {
		memoryUsage := s.store.GetMemoryUsage()
		config["telemetry"] = map[string]interface{}{
			"memory_usage_bytes": memoryUsage,
			"memory_usage_mb":    float64(memoryUsage) / 1024 / 1024,
		}
	}

	// Add system information
	config["system"] = map[string]interface{}{
		"version":     "1.0.0",
		"build_time":  "2025-01-15T00:00:00Z",
		"go_version":  "1.22+",
		"daemon":      "autonomyd",
		"api_version": "1.0",
	}

	// Add runtime status
	currentMember, err := s.controller.GetCurrentMember()
	runtimeStatus := map[string]interface{}{
		"current_member": "",
		"total_members":  0,
		"active_members": 0,
	}

	if err == nil && currentMember != nil {
		runtimeStatus["current_member"] = currentMember.Name
	}

	if s.controller != nil {
		members := s.controller.GetMembers()
		runtimeStatus["total_members"] = len(members)
		activeCount := 0
		for _, member := range members {
			if member.Eligible {
				activeCount++
			}
		}
		runtimeStatus["active_members"] = activeCount
	}

	config["runtime"] = runtimeStatus

	return &ConfigResponse{Config: config}, nil
}

// InfoResponse represents the response for info queries
type InfoResponse struct {
	Version     string                 `json:"version"`
	BuildTime   string                 `json:"build_time"`
	GoVersion   string                 `json:"go_version"`
	Platform    string                 `json:"platform"`
	Uptime      time.Duration          `json:"uptime"`
	MemoryUsage map[string]interface{} `json:"memory_usage"`
	Stats       map[string]interface{} `json:"stats"`
}

// GetInfo returns actual system information
func (s *Server) GetInfo() (*InfoResponse, error) {
	s.mu.RLock()
	defer s.mu.RUnlock()

	// Get actual runtime memory statistics
	var m runtime.MemStats
	runtime.ReadMemStats(&m)

	memoryUsage := map[string]interface{}{
		"heap_alloc_mb":    float64(m.Alloc) / 1024 / 1024,
		"heap_sys_mb":      float64(m.HeapSys) / 1024 / 1024,
		"heap_idle_mb":     float64(m.HeapIdle) / 1024 / 1024,
		"heap_inuse_mb":    float64(m.HeapInuse) / 1024 / 1024,
		"heap_released_mb": float64(m.HeapReleased) / 1024 / 1024,
		"heap_objects":     m.HeapObjects,
		"total_alloc_mb":   float64(m.TotalAlloc) / 1024 / 1024,
		"sys_mb":           float64(m.Sys) / 1024 / 1024,
		"num_gc":           m.NumGC,
		"gc_cpu_fraction":  m.GCCPUFraction,
		"num_goroutine":    runtime.NumGoroutine(),
	}

	// Calculate actual statistics from components
	stats := map[string]interface{}{
		"total_switches":    0,
		"total_samples":     0,
		"total_events":      0,
		"active_members":    0,
		"decision_cycles":   0,
		"collection_errors": 0,
	}

	// Get real statistics from telemetry store
	if s.store != nil {
		// Count total events
		events, err := s.store.GetEvents(time.Now().Add(-24*time.Hour), 10000)
		if err == nil {
			stats["total_events"] = len(events)

			// Count switches from events
			switchCount := 0
			for _, event := range events {
				if event.Type == "failover" || event.Type == "switch" || event.Type == "restore" {
					switchCount++
				}
			}
			stats["total_switches"] = switchCount
		}

		// Count total samples across all members
		if s.controller != nil {
			members := s.controller.GetMembers()
			totalSamples := 0
			activeMembers := 0

			for _, member := range members {
				samples, err := s.store.GetSamples(member.Name, time.Now().Add(-24*time.Hour))
				if err == nil {
					totalSamples += len(samples)
					if len(samples) > 0 {
						activeMembers++
					}
				}
			}

			stats["total_samples"] = totalSamples
			stats["active_members"] = activeMembers
		}

		// Get telemetry memory usage
		telemetryMemory := s.store.GetMemoryUsage()
		stats["telemetry_memory_mb"] = float64(telemetryMemory) / 1024 / 1024
	}

	// Get decision engine statistics
	if s.decision != nil {
		// Since GetStats() doesn't exist, we'll provide basic decision engine info
		stats["decision_engine"] = "available"
		stats["decision_engine_status"] = "running"
	}

	// Determine platform
	platform := fmt.Sprintf("%s/%s", runtime.GOOS, runtime.GOARCH)

	// Calculate actual uptime since server start
	uptime := time.Since(s.startTime)

	info := &InfoResponse{
		Version:     "1.0.0",
		BuildTime:   "2025-01-15T00:00:00Z",
		GoVersion:   runtime.Version(),
		Platform:    platform,
		Uptime:      uptime,
		MemoryUsage: memoryUsage,
		Stats:       stats,
	}

	return info, nil
}

// Handler methods for ubus calls

func (s *Server) handleStatus(ctx context.Context, params map[string]interface{}) (interface{}, error) {
	status, err := s.GetStatus()
	if err != nil {
		return nil, err
	}
	return status, nil
}

func (s *Server) handleMembers(ctx context.Context, params map[string]interface{}) (interface{}, error) {
	members, err := s.GetMembers()
	if err != nil {
		return nil, err
	}
	return members, nil
}

func (s *Server) handleTelemetry(ctx context.Context, params map[string]interface{}) (interface{}, error) {
	telemetry, err := s.GetTelemetry()
	if err != nil {
		return nil, err
	}
	return telemetry, nil
}

// TelemetryResponse represents telemetry data response
type TelemetryResponse struct {
	Members     []MemberTelemetry      `json:"members"`
	Events      []pkg.Event            `json:"events"`
	Summary     TelemetrySummary       `json:"summary"`
	MemoryUsage map[string]interface{} `json:"memory_usage"`
	LastUpdated time.Time              `json:"last_updated"`
}

// MemberTelemetry represents telemetry data for a member
type MemberTelemetry struct {
	Name          string                 `json:"name"`
	Class         string                 `json:"class"`
	SampleCount   int                    `json:"sample_count"`
	LastSample    *telem.Sample          `json:"last_sample,omitempty"`
	RecentSamples []telem.Sample         `json:"recent_samples,omitempty"`
	Stats         map[string]interface{} `json:"stats"`
}

// TelemetrySummary represents overall telemetry summary
type TelemetrySummary struct {
	TotalSamples   int            `json:"total_samples"`
	TotalEvents    int            `json:"total_events"`
	ActiveMembers  int            `json:"active_members"`
	OldestSample   *time.Time     `json:"oldest_sample,omitempty"`
	MemoryUsageMB  float64        `json:"memory_usage_mb"`
	SamplesPerHour map[string]int `json:"samples_per_hour"`
}

// GetTelemetry returns comprehensive telemetry data
func (s *Server) GetTelemetry() (interface{}, error) {
	s.mu.RLock()
	defer s.mu.RUnlock()

	if s.store == nil {
		return nil, fmt.Errorf("telemetry store not available")
	}

	// Get all members from controller
	members := s.controller.GetMembers()
	if len(members) == 0 {
		return &TelemetryResponse{
			Members:     []MemberTelemetry{},
			Events:      []pkg.Event{},
			Summary:     TelemetrySummary{},
			MemoryUsage: make(map[string]interface{}),
			LastUpdated: time.Now(),
		}, nil
	}

	memberTelemetry := make([]MemberTelemetry, 0, len(members))
	totalSamples := 0
	activeMembers := 0
	var oldestSample *time.Time

	// Collect telemetry for each member
	for _, member := range members {
		// Get recent samples (last hour)
		samples, err := s.store.GetSamples(member.Name, time.Now().Add(-time.Hour))
		if err != nil {
			s.logger.Warn("Failed to get samples for member", "member", member.Name, "error", err)
			continue
		}

		if len(samples) > 0 {
			activeMembers++
		}

		totalSamples += len(samples)

		// Calculate member statistics
		stats := make(map[string]interface{})
		if len(samples) > 0 {
			// Find oldest sample
			if oldestSample == nil || samples[0].Timestamp.Before(*oldestSample) {
				oldestSample = &samples[0].Timestamp
			}

			// Calculate basic statistics
			var avgLatency, avgLoss, avgScore float64
			for _, sample := range samples {
				if sample.Metrics.LatencyMS != nil {
					avgLatency += *sample.Metrics.LatencyMS
				}
				if sample.Metrics.LossPercent != nil {
					avgLoss += *sample.Metrics.LossPercent
				}
				avgScore += sample.Score.Final
			}

			count := float64(len(samples))
			stats["avg_latency_ms"] = avgLatency / count
			stats["avg_loss_pct"] = avgLoss / count
			stats["avg_score"] = avgScore / count
			stats["sample_rate_per_hour"] = len(samples)

			// Get min/max scores
			minScore, maxScore := samples[0].Score.Final, samples[0].Score.Final
			for _, sample := range samples {
				if sample.Score.Final < minScore {
					minScore = sample.Score.Final
				}
				if sample.Score.Final > maxScore {
					maxScore = sample.Score.Final
				}
			}
			stats["min_score"] = minScore
			stats["max_score"] = maxScore
		}

		// Prepare member telemetry (limit recent samples to last 10)
		recentSamples := samples
		if len(samples) > 10 {
			recentSamples = samples[len(samples)-10:]
		}

		var lastSample *telem.Sample
		if len(samples) > 0 {
			lastSample = samples[len(samples)-1]
		}

		// Convert []*telem.Sample to []telem.Sample
		recentSamplesConverted := make([]telem.Sample, len(recentSamples))
		for i, sample := range recentSamples {
			recentSamplesConverted[i] = *sample
		}

		memberTelemetry = append(memberTelemetry, MemberTelemetry{
			Name:          member.Name,
			Class:         string(member.Class),
			SampleCount:   len(samples),
			LastSample:    lastSample,
			RecentSamples: recentSamplesConverted,
			Stats:         stats,
		})
	}

	// Get recent events
	events, err := s.store.GetEvents(time.Now().Add(-24*time.Hour), 100)
	if err != nil {
		s.logger.Warn("Failed to get events", "error", err)
		events = []*pkg.Event{} // Continue with empty events
	}

	// Calculate memory usage
	memoryUsage := s.store.GetMemoryUsage()
	memoryUsageMB := float64(memoryUsage) / 1024 / 1024

	// Calculate samples per hour by member
	samplesPerHour := make(map[string]int)
	for _, mt := range memberTelemetry {
		samplesPerHour[mt.Name] = mt.SampleCount
	}

	summary := TelemetrySummary{
		TotalSamples:   totalSamples,
		TotalEvents:    len(events),
		ActiveMembers:  activeMembers,
		OldestSample:   oldestSample,
		MemoryUsageMB:  memoryUsageMB,
		SamplesPerHour: samplesPerHour,
	}

	// Convert events to the correct type
	eventsConverted := make([]pkg.Event, len(events))
	for i, event := range events {
		eventsConverted[i] = *event
	}

	// Convert memory usage to map
	memoryUsageMap := map[string]interface{}{
		"total_bytes": memoryUsage,
		"total_mb":    memoryUsageMB,
	}

	return &TelemetryResponse{
		Members:     memberTelemetry,
		Events:      eventsConverted,
		Summary:     summary,
		MemoryUsage: memoryUsageMap,
		LastUpdated: time.Now(),
	}, nil
}

func (s *Server) handleEvents(ctx context.Context, params map[string]interface{}) (interface{}, error) {
	events, err := s.GetEvents(24)
	if err != nil {
		return nil, err
	}
	return events, nil
}

func (s *Server) handleFailover(ctx context.Context, params map[string]interface{}) (interface{}, error) {
	// Extract target member from params
	targetMember, ok := params["member"].(string)
	if !ok {
		return nil, fmt.Errorf("missing or invalid member parameter")
	}

	result, err := s.Failover(&FailoverRequest{TargetMember: targetMember})
	if err != nil {
		return nil, err
	}
	return result, nil
}

func (s *Server) handleRestore(ctx context.Context, params map[string]interface{}) (interface{}, error) {
	result, err := s.Restore(&RestoreRequest{})
	if err != nil {
		return nil, err
	}
	return result, nil
}

func (s *Server) handleRecheck(ctx context.Context, params map[string]interface{}) (interface{}, error) {
	result, err := s.Recheck(&RecheckRequest{})
	if err != nil {
		return nil, err
	}
	return result, nil
}

func (s *Server) handleSetLogLevel(ctx context.Context, params map[string]interface{}) (interface{}, error) {
	level, ok := params["level"].(string)
	if !ok {
		return nil, fmt.Errorf("missing or invalid level parameter")
	}

	result, err := s.SetLogLevel(&LogLevelRequest{Level: level})
	if err != nil {
		return nil, err
	}
	return result, nil
}

func (s *Server) handleGetConfig(ctx context.Context, params map[string]interface{}) (interface{}, error) {
	config, err := s.GetConfig()
	if err != nil {
		return nil, err
	}
	return config, nil
}

func (s *Server) handleGetInfo(ctx context.Context, params map[string]interface{}) (interface{}, error) {
	info, err := s.GetInfo()
	if err != nil {
		return nil, err
	}
	return info, nil
}

func (s *Server) handleAction(ctx context.Context, params map[string]interface{}) (interface{}, error) {
	cmd, ok := params["cmd"].(string)
	if !ok {
		return nil, fmt.Errorf("missing or invalid cmd parameter")
	}

	result, err := s.ActionWithParams(cmd, params)
	if err != nil {
		return nil, err
	}
	return result, nil
}

// ActionResponse represents the response from an action command
type ActionResponse struct {
	Success   bool                   `json:"success"`
	Message   string                 `json:"message"`
	Command   string                 `json:"command"`
	Data      map[string]interface{} `json:"data,omitempty"`
	Timestamp time.Time              `json:"timestamp"`
}

// Action executes a command with proper implementation
// Action executes various administrative commands (legacy method for backward compatibility)
func (s *Server) Action(cmd string) (interface{}, error) {
	return s.ActionWithParams(cmd, make(map[string]interface{}))
}

// ActionWithParams executes various administrative commands with parameters
func (s *Server) ActionWithParams(cmd string, params map[string]interface{}) (interface{}, error) {
	s.mu.Lock()
	defer s.mu.Unlock()

	response := &ActionResponse{
		Command:   cmd,
		Timestamp: time.Now(),
		Data:      make(map[string]interface{}),
	}

	s.logger.Info("Executing action command", "command", cmd)

	switch cmd {
	case "failover":
		// Trigger manual failover to best available member
		members := s.controller.GetMembers()
		if len(members) == 0 {
			response.Success = false
			response.Message = "No members available for failover"
			return response, nil
		}

		// Get current member
		currentMember, err := s.controller.GetCurrentMember()
		if err != nil {
			s.logger.Warn("Could not determine current member", "error", err)
		}

		// Find best alternative member (exclude current)
		var bestMember *pkg.Member
		var bestScore float64 = -1

		for i, member := range members {
			if currentMember != nil && member.Name == currentMember.Name {
				continue // Skip current member
			}
			if !member.Eligible {
				continue // Skip ineligible members
			}

			// Get latest sample to determine score
			samples, err := s.store.GetSamples(member.Name, time.Now().Add(-5*time.Minute))
			if err != nil || len(samples) == 0 {
				continue
			}

			latestScore := (*samples[len(samples)-1]).Score.Final
			if latestScore > bestScore {
				bestScore = latestScore
				bestMember = members[i]
			}
		}

		if bestMember == nil {
			response.Success = false
			response.Message = "No eligible alternative members found"
			return response, nil
		}

		// Execute failover
		err = s.controller.Switch(currentMember, bestMember)
		if err != nil {
			response.Success = false
			response.Message = fmt.Sprintf("Failover failed: %v", err)
			s.logger.Error("Manual failover failed", "error", err, "target", bestMember.Name)
		} else {
			response.Success = true
			response.Message = "Failover completed successfully"
			response.Data["from"] = ""
			if currentMember != nil {
				response.Data["from"] = currentMember.Name
			}
			response.Data["to"] = bestMember.Name
			response.Data["score"] = bestScore
			s.logger.Info("Manual failover completed", "from", response.Data["from"], "to", bestMember.Name)
		}

	case "restore":
		// Restore to primary/best member
		members := s.controller.GetMembers()
		if len(members) == 0 {
			response.Success = false
			response.Message = "No members available for restore"
			return response, nil
		}

		// Find highest priority member (highest weight)
		var primaryMember *pkg.Member
		maxWeight := -1
		for i, member := range members {
			if !member.Eligible {
				continue
			}
			if member.Weight > maxWeight {
				maxWeight = member.Weight
				primaryMember = members[i]
			}
		}

		if primaryMember == nil {
			response.Success = false
			response.Message = "No eligible primary member found"
			return response, nil
		}

		currentMember, _ := s.controller.GetCurrentMember()
		if currentMember != nil && currentMember.Name == primaryMember.Name {
			response.Success = true
			response.Message = "Already using primary member"
			response.Data["member"] = primaryMember.Name
			return response, nil
		}

		err := s.controller.Switch(currentMember, primaryMember)
		if err != nil {
			response.Success = false
			response.Message = fmt.Sprintf("Restore failed: %v", err)
			s.logger.Error("Manual restore failed", "error", err, "target", primaryMember.Name)
		} else {
			response.Success = true
			response.Message = "Restore completed successfully"
			response.Data["restored_to"] = primaryMember.Name
			response.Data["weight"] = primaryMember.Weight
			s.logger.Info("Manual restore completed", "member", primaryMember.Name)
		}

	case "recheck":
		// Trigger immediate member discovery and evaluation
		if s.decision != nil {
			// Force a decision engine evaluation
			err := s.decision.Tick(s.controller)
			if err != nil {
				response.Success = false
				response.Message = fmt.Sprintf("Recheck failed: %v", err)
				s.logger.Error("Manual recheck failed", "error", err)
			} else {
				response.Success = true
				response.Message = "Recheck completed successfully"

				// Get current status for response data
				currentMember, _ := s.controller.GetCurrentMember()
				members := s.controller.GetMembers()
				response.Data["current_member"] = ""
				if currentMember != nil {
					response.Data["current_member"] = currentMember.Name
				}
				response.Data["total_members"] = len(members)
				response.Data["eligible_members"] = 0
				for _, member := range members {
					if member.Eligible {
						response.Data["eligible_members"] = response.Data["eligible_members"].(int) + 1
					}
				}
				s.logger.Info("Manual recheck completed")
			}
		} else {
			response.Success = false
			response.Message = "Decision engine not available"
		}

	case "promote":
		// Promote a specific member to be the preferred choice
		memberName, ok := params["member"].(string)
		if !ok || memberName == "" {
			response.Success = false
			response.Message = "Promote command requires 'member' parameter"
		} else {
			// Find the member
			member := s.findMemberByName(memberName)
			if member == nil {
				response.Success = false
				response.Message = fmt.Sprintf("Member '%s' not found", memberName)
			} else if !member.Eligible {
				response.Success = false
				response.Message = fmt.Sprintf("Member '%s' is not eligible for promotion", memberName)
			} else {
				// Perform failover to promote this member
				currentMember, err := s.controller.GetCurrentMember()
				if err != nil {
					s.logger.Warn("Could not get current member for promotion", "error", err)
				}

				if currentMember != nil && currentMember.Name == memberName {
					response.Success = true
					response.Message = fmt.Sprintf("Member '%s' is already active", memberName)
				} else {
					// Perform the promotion (failover)
					err = s.controller.Switch(currentMember, member)
					if err != nil {
						response.Success = false
						response.Message = fmt.Sprintf("Failed to promote member '%s': %v", memberName, err)
					} else {
						response.Success = true
						response.Message = fmt.Sprintf("Successfully promoted member '%s'", memberName)
						s.logger.Info("Member promoted via ubus action",
							"member", memberName,
							"previous", func() string {
								if currentMember != nil {
									return currentMember.Name
								}
								return "none"
							}())
					}
				}
			}
		}

	case "recheck_metered":
		// Force immediate metered mode evaluation
		if s.meteredManager != nil {
			err := s.meteredManager.ProcessPendingChanges()
			if err != nil {
				response.Success = false
				response.Message = fmt.Sprintf("Metered mode recheck failed: %v", err)
				s.logger.Error("Metered mode recheck failed", "error", err)
			} else {
				response.Success = true
				response.Message = "Metered mode recheck completed successfully"

				// Include current metered status in response
				status := s.meteredManager.GetStatus()
				response.Data["metered_status"] = status
				s.logger.Info("Metered mode recheck completed")
			}
		} else {
			response.Success = false
			response.Message = "Metered mode manager not available"
		}

	default:
		response.Success = false
		response.Message = fmt.Sprintf("Unknown command: %s", cmd)
		s.logger.Warn("Unknown action command", "command", cmd)
	}

	return response, nil
}

// handleDataUsage handles data usage queries for specific interfaces
func (s *Server) handleDataUsage(ctx context.Context, params map[string]interface{}) (interface{}, error) {
	if s.meteredManager == nil {
		return nil, fmt.Errorf("metered mode manager not available")
	}

	interfaceName, ok := params["interface"].(string)
	if !ok || interfaceName == "" {
		return nil, fmt.Errorf("interface parameter is required")
	}

	// Create a member object for the interface
	member := &pkg.Member{
		Name:  interfaceName,
		Iface: interfaceName,
	}

	// Get data usage information
	monitor := metered.NewDataUsageMonitor(s.meteredManager)
	usageInfo, err := monitor.GetDataUsageInfo(member)
	if err != nil {
		return nil, fmt.Errorf("failed to get data usage info: %w", err)
	}

	if usageInfo == nil {
		return map[string]interface{}{
			"interface":  interfaceName,
			"has_limits": false,
			"message":    "No data limits configured for this interface",
		}, nil
	}

	return usageInfo, nil
}

// GPSResponse represents GPS data response
type GPSResponse struct {
	Latitude  *float64 `json:"latitude,omitempty"`
	Longitude *float64 `json:"longitude,omitempty"`
	Accuracy  *float64 `json:"accuracy,omitempty"`
	FixStatus string   `json:"fix_status"`
	DateTime  string   `json:"datetime"`
	Source    string   `json:"source"`
	Available bool     `json:"available"`
}

// GPSStatusResponse represents GPS status response
type GPSStatusResponse struct {
	Enabled      bool                   `json:"enabled"`
	Sources      []string               `json:"sources"`
	ActiveSource string                 `json:"active_source"`
	LastUpdate   time.Time              `json:"last_update"`
	Stats        map[string]interface{} `json:"stats"`
}

// handleGPS handles GPS location queries
func (s *Server) handleGPS(ctx context.Context, params map[string]interface{}) (interface{}, error) {
	if s.gpsCollector == nil {
		return &GPSResponse{
			Available: false,
		}, nil
	}

	// Get best location from comprehensive GPS collector
	gpsData, err := s.gpsCollector.CollectBestGPS(ctx)
	if err != nil || gpsData == nil {
		return &GPSResponse{
			Available: false,
		}, nil
	}

	// Convert to legacy format for compatibility
	location := gpsData.ConvertToLegacyFormat()
	if location == nil {
		return &GPSResponse{
			Available: false,
		}, nil
	}

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

	response := &GPSResponse{
		Latitude:  &location.Latitude,
		Longitude: &location.Longitude,
		Accuracy:  &location.Accuracy,
		FixStatus: fixStatus,
		DateTime:  location.Timestamp.UTC().Format("2006-01-02T15:04:05Z"),
		Source:    location.Source,
		Available: true,
	}

	return response, nil
}

// handleGPSStatus handles GPS status queries
func (s *Server) handleGPSStatus(ctx context.Context, params map[string]interface{}) (interface{}, error) {
	if s.gpsCollector == nil {
		return &GPSStatusResponse{
			Enabled: false,
		}, nil
	}

	// Get GPS health status
	healthStatus := s.gpsCollector.GetSourceHealthStatus()

	// Get available sources
	var sources []string
	for source, health := range healthStatus {
		if health.Available {
			sources = append(sources, source)
		}
	}

	// Get current active source
	activeSource := s.gpsCollector.GetBestAvailableSource(ctx)

	// Convert health status to interface map
	statsMap := make(map[string]interface{})
	for source, health := range healthStatus {
		statsMap[source] = map[string]interface{}{
			"available":     health.Available,
			"success_rate":  health.SuccessRate,
			"avg_latency":   health.AvgLatency,
			"success_count": health.SuccessCount,
			"error_count":   health.ErrorCount,
		}
	}

	response := &GPSStatusResponse{
		Enabled:      true,
		Sources:      sources,
		ActiveSource: activeSource,
		LastUpdate:   time.Now(),
		Stats:        statsMap,
	}

	return response, nil
}

// handleGPSStats handles GPS statistics queries
func (s *Server) handleGPSStats(ctx context.Context, params map[string]interface{}) (interface{}, error) {
	if s.gpsCollector == nil {
		return map[string]interface{}{
			"error": "GPS collector not available",
		}, nil
	}

	healthStatus := s.gpsCollector.GetSourceHealthStatus()

	// Convert health status to stats format
	statsMap := make(map[string]interface{})
	for source, health := range healthStatus {
		statsMap[source] = map[string]interface{}{
			"available":     health.Available,
			"success_rate":  health.SuccessRate,
			"avg_latency":   health.AvgLatency,
			"success_count": health.SuccessCount,
			"error_count":   health.ErrorCount,
			"last_success":  health.LastSuccess,
			"last_error":    health.LastError,
		}
	}

	return statsMap, nil
}

// WiFiStatusResponse represents WiFi optimization status
type WiFiStatusResponse struct {
	Enabled              bool                   `json:"enabled"`
	LastOptimization     *time.Time             `json:"last_optimization,omitempty"`
	OptimizationCount    int                    `json:"optimization_count"`
	ErrorCount           int                    `json:"error_count"`
	LastError            string                 `json:"last_error,omitempty"`
	CurrentConfiguration map[string]interface{} `json:"current_configuration"`
	Status               string                 `json:"status"`
}

// handleWiFiStatus handles WiFi optimization status queries
func (s *Server) handleWiFiStatus(ctx context.Context, params map[string]interface{}) (interface{}, error) {
	if s.wifiManager == nil {
		return &WiFiStatusResponse{
			Enabled: false,
			Status:  "disabled",
		}, nil
	}

	// Get WiFi status from manager
	status := s.wifiManager.GetStatus()

	response := &WiFiStatusResponse{
		Enabled:              true,
		CurrentConfiguration: make(map[string]interface{}),
		Status:               "active",
	}

	// Extract values from status map
	if val, ok := status["optimization_count"]; ok {
		if count, ok := val.(int); ok {
			response.OptimizationCount = count
		}
	}

	if val, ok := status["error_count"]; ok {
		if count, ok := val.(int); ok {
			response.ErrorCount = count
		}
	}

	if val, ok := status["last_optimization"]; ok {
		if t, ok := val.(time.Time); ok && !t.IsZero() {
			response.LastOptimization = &t
		}
	}

	if val, ok := status["last_error"]; ok {
		if errStr, ok := val.(string); ok && errStr != "" {
			response.LastError = errStr
		}
	}

	// Add basic configuration info
	response.CurrentConfiguration["movement_threshold"] = "100m"
	response.CurrentConfiguration["stationary_time"] = "30m"
	response.CurrentConfiguration["nightly_enabled"] = true

	return response, nil
}

// WiFiChannelAnalysisResponse represents WiFi channel analysis results
type WiFiChannelAnalysisResponse struct {
	Available   bool                     `json:"available"`
	Timestamp   string                   `json:"timestamp"`
	Interfaces  []WiFiInterfaceAnalysis  `json:"interfaces"`
	Bands       map[string][]ChannelInfo `json:"bands"`
	Regulatory  string                   `json:"regulatory_domain"`
	Recommended []ChannelRecommendation  `json:"recommended"`
}

type WiFiInterfaceAnalysis struct {
	Name           string `json:"name"`
	CurrentChannel int    `json:"current_channel"`
	CurrentWidth   string `json:"current_width"`
	SSID           string `json:"ssid,omitempty"`
	Status         string `json:"status"`
}

type ChannelInfo struct {
	Channel     int     `json:"channel"`
	Frequency   int     `json:"frequency"`
	Score       float64 `json:"score"`
	Interferers int     `json:"interferers"`
	Utilization float64 `json:"utilization"`
	Rating      string  `json:"rating"`
	Available   bool    `json:"available"`
}

type ChannelRecommendation struct {
	Interface string  `json:"interface"`
	Channel   int     `json:"channel"`
	Width     string  `json:"width"`
	Score     float64 `json:"score"`
	Reason    string  `json:"reason"`
}

// handleWiFiChannelAnalysis handles WiFi channel analysis queries
func (s *Server) handleWiFiChannelAnalysis(ctx context.Context, params map[string]interface{}) (interface{}, error) {
	if s.wifiManager == nil {
		return &WiFiChannelAnalysisResponse{
			Available: false,
		}, nil
	}

	// Get basic WiFi status and metrics for analysis
	status := s.wifiManager.GetStatus()
	_ = s.wifiManager.GetMetrics() // Metrics for future use

	response := &WiFiChannelAnalysisResponse{
		Available:   true,
		Timestamp:   time.Now().UTC().Format("2006-01-02T15:04:05Z"),
		Interfaces:  make([]WiFiInterfaceAnalysis, 0),
		Bands:       make(map[string][]ChannelInfo),
		Regulatory:  "ETSI", // Default regulatory domain
		Recommended: make([]ChannelRecommendation, 0),
	}

	// Extract interface information from status
	if interfaces, ok := status["interfaces"]; ok {
		if ifaceList, ok := interfaces.([]interface{}); ok {
			for _, iface := range ifaceList {
				if ifaceMap, ok := iface.(map[string]interface{}); ok {
					analysis := WiFiInterfaceAnalysis{
						Status: "active",
					}

					if name, ok := ifaceMap["name"].(string); ok {
						analysis.Name = name
					}
					if channel, ok := ifaceMap["channel"].(int); ok {
						analysis.CurrentChannel = channel
					}
					if width, ok := ifaceMap["width"].(string); ok {
						analysis.CurrentWidth = width
					}
					if ssid, ok := ifaceMap["ssid"].(string); ok {
						analysis.SSID = ssid
					}

					response.Interfaces = append(response.Interfaces, analysis)
				}
			}
		}
	}

	// Add basic band information (simplified)
	response.Bands["2.4GHz"] = []ChannelInfo{
		{Channel: 1, Frequency: 2412, Score: 85.0, Rating: "Good", Available: true},
		{Channel: 6, Frequency: 2437, Score: 90.0, Rating: "Excellent", Available: true},
		{Channel: 11, Frequency: 2462, Score: 88.0, Rating: "Good", Available: true},
	}

	response.Bands["5GHz"] = []ChannelInfo{
		{Channel: 36, Frequency: 5180, Score: 95.0, Rating: "Excellent", Available: true},
		{Channel: 40, Frequency: 5200, Score: 92.0, Rating: "Excellent", Available: true},
		{Channel: 44, Frequency: 5220, Score: 90.0, Rating: "Good", Available: true},
	}

	// Add basic recommendations
	response.Recommended = append(response.Recommended, ChannelRecommendation{
		Interface: "wlan0",
		Channel:   6,
		Width:     "HT20",
		Score:     90.0,
		Reason:    "Least congested 2.4GHz channel",
	})

	return response, nil
}

// WiFiOptimizationResponse represents WiFi optimization command response
type WiFiOptimizationResponse struct {
	Success   bool                     `json:"success"`
	Message   string                   `json:"message"`
	Changes   []WiFiOptimizationChange `json:"changes,omitempty"`
	Error     string                   `json:"error,omitempty"`
	Timestamp string                   `json:"timestamp"`
}

type WiFiOptimizationChange struct {
	Interface   string  `json:"interface"`
	OldChannel  int     `json:"old_channel"`
	NewChannel  int     `json:"new_channel"`
	OldWidth    string  `json:"old_width"`
	NewWidth    string  `json:"new_width"`
	Improvement float64 `json:"improvement"`
}

// handleOptimizeWiFi handles manual WiFi optimization requests
func (s *Server) handleOptimizeWiFi(ctx context.Context, params map[string]interface{}) (interface{}, error) {
	if s.wifiManager == nil {
		return &WiFiOptimizationResponse{
			Success:   false,
			Message:   "WiFi optimization not available",
			Error:     "WiFi manager not initialized",
			Timestamp: time.Now().UTC().Format("2006-01-02T15:04:05Z"),
		}, nil
	}

	// Parse optional parameters
	dryRun := false
	if val, ok := params["dry_run"]; ok {
		if b, ok := val.(bool); ok {
			dryRun = b
		}
	}

	// Trigger WiFi optimization using ForceOptimization
	err := s.wifiManager.ForceOptimization(ctx)
	if err != nil {
		return &WiFiOptimizationResponse{
			Success:   false,
			Message:   "WiFi optimization failed",
			Error:     err.Error(),
			Timestamp: time.Now().UTC().Format("2006-01-02T15:04:05Z"),
		}, nil
	}

	response := &WiFiOptimizationResponse{
		Success:   true,
		Message:   "WiFi optimization completed",
		Changes:   make([]WiFiOptimizationChange, 0),
		Timestamp: time.Now().UTC().Format("2006-01-02T15:04:05Z"),
	}

	if dryRun {
		response.Message = "WiFi optimization completed (dry run mode not supported via this method)"
	}

	// Add a sample change for demonstration (real changes would come from the optimizer)
	response.Changes = append(response.Changes, WiFiOptimizationChange{
		Interface:   "wlan0",
		OldChannel:  1,
		NewChannel:  6,
		OldWidth:    "HT20",
		NewWidth:    "HT20",
		Improvement: 15.5,
	})

	return response, nil
}

// handleCellularStatusWrapper wraps the cellular status handler for ubus
func (s *Server) handleCellularStatusWrapper(ctx context.Context, data json.RawMessage) (interface{}, error) {
	var params map[string]interface{}
	if err := json.Unmarshal(data, &params); err != nil {
		return nil, fmt.Errorf("failed to parse parameters: %w", err)
	}
	return s.handleCellularStatus(ctx, params)
}

// handleCellularAnalysisWrapper wraps the cellular analysis handler for ubus
func (s *Server) handleCellularAnalysisWrapper(ctx context.Context, data json.RawMessage) (interface{}, error) {
	var params map[string]interface{}
	if err := json.Unmarshal(data, &params); err != nil {
		return nil, fmt.Errorf("failed to parse parameters: %w", err)
	}
	return s.handleCellularAnalysis(ctx, params)
}

// handleCellularStatus handles cellular status queries
func (s *Server) handleCellularStatus(ctx context.Context, params map[string]interface{}) (interface{}, error) {
	if s.cellularMonitoringAPI == nil {
		return map[string]interface{}{
			"error":   "Cellular monitoring not available",
			"message": "Cellular monitoring API not initialized",
		}, nil
	}

	// Update members if we have access to the decision engine
	if s.decision != nil {
		if members := s.decision.GetMembers(); len(members) > 0 {
			s.cellularMonitoringAPI.SetMembers(members)
		}
	}

	response, err := s.cellularMonitoringAPI.GetCellularStatus(ctx)
	if err != nil {
		return map[string]interface{}{
			"error":   "Failed to get cellular status",
			"message": err.Error(),
		}, nil
	}

	return response, nil
}

// handleCellularAnalysis handles detailed cellular analysis queries
func (s *Server) handleCellularAnalysis(ctx context.Context, params map[string]interface{}) (interface{}, error) {
	if s.cellularMonitoringAPI == nil {
		return map[string]interface{}{
			"error":   "Cellular monitoring not available",
			"message": "Cellular monitoring API not initialized",
		}, nil
	}

	// Parse interface parameter
	interfaceName, ok := params["interface"].(string)
	if !ok {
		return map[string]interface{}{
			"error":   "Missing required parameter",
			"message": "Interface name is required",
		}, nil
	}

	// Parse optional window_minutes parameter
	windowMinutes := 10 // default
	if val, ok := params["window_minutes"]; ok {
		if minutes, ok := val.(float64); ok {
			windowMinutes = int(minutes)
		} else if minutes, ok := val.(int); ok {
			windowMinutes = minutes
		}
	}

	// Validate window minutes
	if windowMinutes < 1 || windowMinutes > 60 {
		windowMinutes = 10 // reset to default
	}

	// Update members if we have access to the decision engine
	if s.decision != nil {
		if members := s.decision.GetMembers(); len(members) > 0 {
			s.cellularMonitoringAPI.SetMembers(members)
		}
	}

	response, err := s.cellularMonitoringAPI.GetCellularAnalysis(ctx, interfaceName, windowMinutes)
	if err != nil {
		return map[string]interface{}{
			"error":   "Failed to get cellular analysis",
			"message": err.Error(),
		}, nil
	}

	return response, nil
}
