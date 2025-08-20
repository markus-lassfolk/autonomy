package adaptive

import (
	"context"
	"fmt"
	"net"
	"os/exec"
	"strings"
	"sync"
	"time"

	"github.com/markus-lassfolk/autonomy/pkg/logx"
)

// ConnectionInfo represents information about a network connection
type ConnectionInfo struct {
	Type        ConnectionType `json:"type"`
	Interface   string         `json:"interface"`
	IPAddress   string         `json:"ip_address"`
	Gateway     string         `json:"gateway"`
	DNS         []string       `json:"dns"`
	MTU         int            `json:"mtu"`
	Speed       int64          `json:"speed"`       // Speed in Mbps
	Latency     time.Duration  `json:"latency"`     // Current latency
	PacketLoss  float64        `json:"packet_loss"` // Packet loss percentage
	LastUpdated time.Time      `json:"last_updated"`
	Confidence  float64        `json:"confidence"` // Confidence in detection (0-1)
}

// ConnectionDetector detects and monitors network connection types
type ConnectionDetector struct {
	config *ConnectionDetectorConfig
	logger *logx.Logger
	mu     sync.RWMutex

	// Current connection info
	currentConnection *ConnectionInfo

	// Detection history
	detectionHistory []*ConnectionInfo

	// Callbacks
	onConnectionChange func(oldType, newType ConnectionType)
}

// ConnectionDetectorConfig holds connection detector configuration
type ConnectionDetectorConfig struct {
	Enabled             bool          `json:"enabled"`
	DetectionInterval   time.Duration `json:"detection_interval"`   // How often to detect
	StarlinkIPRange     string        `json:"starlink_ip_range"`    // Starlink IP range (192.168.100.0/24)
	StarlinkGateway     string        `json:"starlink_gateway"`     // Starlink gateway (192.168.100.1)
	CellularInterfaces  []string      `json:"cellular_interfaces"`  // Known cellular interface patterns
	WiFiInterfaces      []string      `json:"wifi_interfaces"`      // Known WiFi interface patterns
	LANInterfaces       []string      `json:"lan_interfaces"`       // Known LAN interface patterns
	DetectionTimeout    time.Duration `json:"detection_timeout"`    // Timeout for detection operations
	ConfidenceThreshold float64       `json:"confidence_threshold"` // Minimum confidence for detection
	MaxHistorySize      int           `json:"max_history_size"`     // Maximum history entries
}

// NewConnectionDetector creates a new connection detector
func NewConnectionDetector(config *ConnectionDetectorConfig, logger *logx.Logger) *ConnectionDetector {
	if config == nil {
		config = &ConnectionDetectorConfig{
			Enabled:             true,
			DetectionInterval:   30 * time.Second,
			StarlinkIPRange:     "192.168.100.0/24",
			StarlinkGateway:     "192.168.100.1",
			CellularInterfaces:  []string{"wwan", "usb", "modem", "mobile"},
			WiFiInterfaces:      []string{"wlan", "wifi", "ath", "radio"},
			LANInterfaces:       []string{"eth", "lan", "ethernet"},
			DetectionTimeout:    10 * time.Second,
			ConfidenceThreshold: 0.7,
			MaxHistorySize:      100,
		}
	}

	cd := &ConnectionDetector{
		config:           config,
		logger:           logger,
		detectionHistory: make([]*ConnectionInfo, 0),
	}

	return cd
}

// Start begins connection detection
func (cd *ConnectionDetector) Start(ctx context.Context) error {
	if !cd.config.Enabled {
		return fmt.Errorf("connection detection is disabled")
	}

	// Perform initial detection
	if err := cd.detectConnection(ctx); err != nil {
		cd.logger.Warn("Initial connection detection failed", "error", err)
	}

	// Start periodic detection
	go cd.detectionLoop(ctx)

	cd.logger.Info("Connection detector started")
	return nil
}

// GetCurrentConnection returns the current connection information
func (cd *ConnectionDetector) GetCurrentConnection() *ConnectionInfo {
	cd.mu.RLock()
	defer cd.mu.RUnlock()

	if cd.currentConnection == nil {
		return nil
	}

	// Return a copy to avoid race conditions
	conn := *cd.currentConnection
	return &conn
}

// GetConnectionHistory returns the connection detection history
func (cd *ConnectionDetector) GetConnectionHistory() []*ConnectionInfo {
	cd.mu.RLock()
	defer cd.mu.RUnlock()

	// Return a copy of the history
	history := make([]*ConnectionInfo, len(cd.detectionHistory))
	copy(history, cd.detectionHistory)
	return history
}

// SetConnectionChangeCallback sets a callback for connection type changes
func (cd *ConnectionDetector) SetConnectionChangeCallback(callback func(oldType, newType ConnectionType)) {
	cd.mu.Lock()
	defer cd.mu.Unlock()
	cd.onConnectionChange = callback
}

// ForceDetection forces an immediate connection detection
func (cd *ConnectionDetector) ForceDetection(ctx context.Context) error {
	return cd.detectConnection(ctx)
}

// detectionLoop runs the periodic detection loop
func (cd *ConnectionDetector) detectionLoop(ctx context.Context) {
	ticker := time.NewTicker(cd.config.DetectionInterval)
	defer ticker.Stop()

	for {
		select {
		case <-ctx.Done():
			return
		case <-ticker.C:
			if err := cd.detectConnection(ctx); err != nil {
				cd.logger.Warn("Connection detection failed", "error", err)
			}
		}
	}
}

// detectConnection performs connection detection
func (cd *ConnectionDetector) detectConnection(ctx context.Context) error {
	cd.mu.Lock()
	defer cd.mu.Unlock()

	// Get default route information
	defaultRoute, err := cd.getDefaultRoute()
	if err != nil {
		cd.logger.Warn("Failed to get default route", "error", err)
		return err
	}

	// Detect connection type
	connInfo, err := cd.detectConnectionType(ctx, defaultRoute)
	if err != nil {
		cd.logger.Warn("Failed to detect connection type", "error", err)
		return err
	}

	// Check if connection type has changed
	oldType := ConnectionTypeUnknown
	if cd.currentConnection != nil {
		oldType = cd.currentConnection.Type
	}

	// Update current connection
	cd.currentConnection = connInfo

	// Add to history
	cd.addToHistory(connInfo)

	// Call callback if type changed
	if oldType != connInfo.Type && cd.onConnectionChange != nil {
		cd.onConnectionChange(oldType, connInfo.Type)
	}

	cd.logger.Debug("Connection detection completed",
		"type", connInfo.Type,
		"interface", connInfo.Interface,
		"confidence", connInfo.Confidence)

	return nil
}

// detectConnectionType determines the connection type based on various factors
func (cd *ConnectionDetector) detectConnectionType(ctx context.Context, defaultRoute *RouteInfo) (*ConnectionInfo, error) {
	connInfo := &ConnectionInfo{
		Type:        ConnectionTypeUnknown,
		Interface:   defaultRoute.Interface,
		IPAddress:   defaultRoute.Source,
		Gateway:     defaultRoute.Gateway,
		LastUpdated: time.Now(),
		Confidence:  0.0,
	}

	// Check for Starlink connection
	if cd.isStarlinkConnection(defaultRoute) {
		connInfo.Type = ConnectionTypeStarlink
		connInfo.Confidence = 0.95
		return connInfo, nil
	}

	// Check interface name patterns
	interfaceType, confidence := cd.detectByInterfaceName(defaultRoute.Interface)
	if confidence > cd.config.ConfidenceThreshold {
		connInfo.Type = interfaceType
		connInfo.Confidence = confidence
		return connInfo, nil
	}

	// Check interface characteristics
	interfaceType, confidence = cd.detectByInterfaceCharacteristics(ctx, defaultRoute.Interface)
	if confidence > cd.config.ConfidenceThreshold {
		connInfo.Type = interfaceType
		connInfo.Confidence = confidence
		return connInfo, nil
	}

	// Check network topology
	interfaceType, confidence = cd.detectByNetworkTopology(ctx, defaultRoute)
	if confidence > cd.config.ConfidenceThreshold {
		connInfo.Type = interfaceType
		connInfo.Confidence = confidence
		return connInfo, nil
	}

	// If we can't determine, mark as unknown with low confidence
	connInfo.Type = ConnectionTypeUnknown
	connInfo.Confidence = 0.1

	return connInfo, nil
}

// isStarlinkConnection checks if the connection is Starlink
func (cd *ConnectionDetector) isStarlinkConnection(route *RouteInfo) bool {
	// Check if gateway is Starlink gateway
	if route.Gateway == cd.config.StarlinkGateway {
		return true
	}

	// Check if source IP is in Starlink range
	if cd.isIPInRange(route.Source, cd.config.StarlinkIPRange) {
		return true
	}

	// Check if we can reach Starlink API
	if cd.canReachStarlinkAPI() {
		return true
	}

	return false
}

// detectByInterfaceName detects connection type by interface name patterns
func (cd *ConnectionDetector) detectByInterfaceName(interfaceName string) (ConnectionType, float64) {
	interfaceName = strings.ToLower(interfaceName)

	// Check cellular patterns
	for _, pattern := range cd.config.CellularInterfaces {
		if strings.Contains(interfaceName, pattern) {
			return ConnectionTypeCellular, 0.9
		}
	}

	// Check WiFi patterns
	for _, pattern := range cd.config.WiFiInterfaces {
		if strings.Contains(interfaceName, pattern) {
			return ConnectionTypeWiFi, 0.8
		}
	}

	// Check LAN patterns
	for _, pattern := range cd.config.LANInterfaces {
		if strings.Contains(interfaceName, pattern) {
			return ConnectionTypeLAN, 0.85
		}
	}

	return ConnectionTypeUnknown, 0.0
}

// detectByInterfaceCharacteristics detects by interface characteristics
func (cd *ConnectionDetector) detectByInterfaceCharacteristics(ctx context.Context, interfaceName string) (ConnectionType, float64) {
	// Get interface statistics
	stats, err := cd.getInterfaceStats(interfaceName)
	if err != nil {
		return ConnectionTypeUnknown, 0.0
	}

	// Check for cellular characteristics
	if stats.Carrier == "up" && stats.Speed > 0 && stats.Speed < 100 {
		// Low speed, likely cellular
		return ConnectionTypeCellular, 0.7
	}

	// Check for WiFi characteristics
	if stats.Carrier == "up" && stats.Speed >= 100 && stats.Speed <= 1000 {
		// Medium speed, likely WiFi
		return ConnectionTypeWiFi, 0.6
	}

	// Check for LAN characteristics
	if stats.Carrier == "up" && stats.Speed >= 1000 {
		// High speed, likely LAN
		return ConnectionTypeLAN, 0.8
	}

	return ConnectionTypeUnknown, 0.0
}

// detectByNetworkTopology detects by network topology analysis
func (cd *ConnectionDetector) detectByNetworkTopology(ctx context.Context, route *RouteInfo) (ConnectionType, float64) {
	// Check latency to gateway
	latency, err := cd.measureLatency(route.Gateway)
	if err != nil {
		return ConnectionTypeUnknown, 0.0
	}

	// High latency (>100ms) suggests cellular or satellite
	if latency > 100*time.Millisecond {
		return ConnectionTypeCellular, 0.6
	}

	// Low latency (<10ms) suggests LAN
	if latency < 10*time.Millisecond {
		return ConnectionTypeLAN, 0.7
	}

	// Medium latency (10-50ms) suggests WiFi
	if latency >= 10*time.Millisecond && latency <= 50*time.Millisecond {
		return ConnectionTypeWiFi, 0.5
	}

	return ConnectionTypeUnknown, 0.0
}

// Helper methods

func (cd *ConnectionDetector) getDefaultRoute() (*RouteInfo, error) {
	// Use ip route to get default route
	cmd := exec.Command("ip", "route", "show", "default")
	output, err := cmd.Output()
	if err != nil {
		return nil, fmt.Errorf("failed to get default route: %w", err)
	}

	lines := strings.Split(string(output), "\n")
	if len(lines) == 0 {
		return nil, fmt.Errorf("no default route found")
	}

	// Parse the first default route
	route := &RouteInfo{}
	fields := strings.Fields(lines[0])

	for i, field := range fields {
		switch field {
		case "default":
			if i+1 < len(fields) && strings.HasPrefix(fields[i+1], "via") {
				route.Gateway = fields[i+2]
			}
		case "dev":
			if i+1 < len(fields) {
				route.Interface = fields[i+1]
			}
		case "src":
			if i+1 < len(fields) {
				route.Source = fields[i+1]
			}
		}
	}

	return route, nil
}

func (cd *ConnectionDetector) isIPInRange(ip, cidr string) bool {
	_, ipNet, err := net.ParseCIDR(cidr)
	if err != nil {
		return false
	}

	parsedIP := net.ParseIP(ip)
	if parsedIP == nil {
		return false
	}

	return ipNet.Contains(parsedIP)
}

func (cd *ConnectionDetector) canReachStarlinkAPI() bool {
	// Try to connect to Starlink API
	conn, err := net.DialTimeout("tcp", cd.config.StarlinkGateway+":9200", 2*time.Second)
	if err != nil {
		return false
	}
	defer conn.Close()
	return true
}

func (cd *ConnectionDetector) measureLatency(target string) (time.Duration, error) {
	start := time.Now()
	conn, err := net.DialTimeout("tcp", target+":80", 5*time.Second)
	if err != nil {
		return 0, err
	}
	defer conn.Close()
	return time.Since(start), nil
}

func (cd *ConnectionDetector) getInterfaceStats(interfaceName string) (*InterfaceStats, error) {
	// This is a simplified implementation
	// In a real implementation, you would read from /sys/class/net/
	return &InterfaceStats{
		Carrier: "up",
		Speed:   1000, // Assume 1Gbps for now
	}, nil
}

func (cd *ConnectionDetector) addToHistory(connInfo *ConnectionInfo) {
	cd.detectionHistory = append(cd.detectionHistory, connInfo)

	// Trim history if it exceeds max size
	if len(cd.detectionHistory) > cd.config.MaxHistorySize {
		cd.detectionHistory = cd.detectionHistory[1:]
	}
}

// RouteInfo represents route information
type RouteInfo struct {
	Interface string `json:"interface"`
	Gateway   string `json:"gateway"`
	Source    string `json:"source"`
}

// InterfaceStats represents interface statistics
type InterfaceStats struct {
	Carrier string `json:"carrier"`
	Speed   int64  `json:"speed"` // Speed in Mbps
}
