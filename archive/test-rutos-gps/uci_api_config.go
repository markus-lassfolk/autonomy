package main

import (
	"fmt"
	"net"
	"strconv"
	"strings"

	"golang.org/x/crypto/ssh"
)

// APIServerConfig holds the API server configuration from UCI
type APIServerConfig struct {
	Enabled         bool   `json:"enabled"`         // Enable/disable API server
	Port            int    `json:"port"`            // Port to listen on
	BindAddress     string `json:"bind_address"`    // Address to bind to (default: "0.0.0.0")
	EnableCORS      bool   `json:"enable_cors"`     // Enable CORS headers
	RequestTimeout  int    `json:"request_timeout"` // Request timeout in seconds
	MaxConnections  int    `json:"max_connections"` // Maximum concurrent connections
	LogRequests     bool   `json:"log_requests"`    // Log API requests
	HealthCheckPath string `json:"health_check"`    // Health check endpoint path
}

// DefaultAPIServerConfig returns the default API server configuration
func DefaultAPIServerConfig() APIServerConfig {
	return APIServerConfig{
		Enabled:         false,         // Disabled by default
		Port:            8080,          // Default port
		BindAddress:     "0.0.0.0",     // Bind to all interfaces
		EnableCORS:      true,          // Enable CORS for web interfaces
		RequestTimeout:  30,            // 30 second timeout
		MaxConnections:  100,           // 100 concurrent connections
		LogRequests:     true,          // Log requests by default
		HealthCheckPath: "/api/health", // Default health check path
	}
}

// UCIAPIConfigManager manages API server configuration via UCI
type UCIAPIConfigManager struct {
	sshClient *ssh.Client
	config    APIServerConfig
}

// NewUCIAPIConfigManager creates a new UCI API config manager
func NewUCIAPIConfigManager(sshClient *ssh.Client) *UCIAPIConfigManager {
	return &UCIAPIConfigManager{
		sshClient: sshClient,
		config:    DefaultAPIServerConfig(),
	}
}

// LoadConfig loads API server configuration from UCI
func (ucm *UCIAPIConfigManager) LoadConfig() error {
	fmt.Println("📋 Loading API server configuration from UCI...")

	// Load configuration values from UCI
	enabled, err := ucm.getUCIBool("autonomy.api.enabled", false)
	if err != nil {
		fmt.Printf("⚠️  Failed to read api.enabled, using default: %v\n", err)
	}
	ucm.config.Enabled = enabled

	port, err := ucm.getUCIInt("autonomy.api.port", 8080)
	if err != nil {
		fmt.Printf("⚠️  Failed to read api.port, using default: %v\n", err)
	}
	ucm.config.Port = port

	bindAddr, err := ucm.getUCIString("autonomy.api.bind_address", "0.0.0.0")
	if err != nil {
		fmt.Printf("⚠️  Failed to read api.bind_address, using default: %v\n", err)
	}
	ucm.config.BindAddress = bindAddr

	enableCORS, err := ucm.getUCIBool("autonomy.api.enable_cors", true)
	if err != nil {
		fmt.Printf("⚠️  Failed to read api.enable_cors, using default: %v\n", err)
	}
	ucm.config.EnableCORS = enableCORS

	timeout, err := ucm.getUCIInt("autonomy.api.request_timeout", 30)
	if err != nil {
		fmt.Printf("⚠️  Failed to read api.request_timeout, using default: %v\n", err)
	}
	ucm.config.RequestTimeout = timeout

	maxConn, err := ucm.getUCIInt("autonomy.api.max_connections", 100)
	if err != nil {
		fmt.Printf("⚠️  Failed to read api.max_connections, using default: %v\n", err)
	}
	ucm.config.MaxConnections = maxConn

	logReq, err := ucm.getUCIBool("autonomy.api.log_requests", true)
	if err != nil {
		fmt.Printf("⚠️  Failed to read api.log_requests, using default: %v\n", err)
	}
	ucm.config.LogRequests = logReq

	// Validate configuration
	if err := ucm.validateConfig(); err != nil {
		return fmt.Errorf("invalid API configuration: %v", err)
	}

	fmt.Printf("✅ API server configuration loaded:\n")
	fmt.Printf("   • Enabled: %v\n", ucm.config.Enabled)
	fmt.Printf("   • Port: %d\n", ucm.config.Port)
	fmt.Printf("   • Bind Address: %s\n", ucm.config.BindAddress)
	fmt.Printf("   • CORS: %v\n", ucm.config.EnableCORS)
	fmt.Printf("   • Timeout: %ds\n", ucm.config.RequestTimeout)
	fmt.Printf("   • Max Connections: %d\n", ucm.config.MaxConnections)
	fmt.Printf("   • Log Requests: %v\n", ucm.config.LogRequests)

	return nil
}

// SaveConfig saves API server configuration to UCI
func (ucm *UCIAPIConfigManager) SaveConfig() error {
	fmt.Println("💾 Saving API server configuration to UCI...")

	// Create UCI section if it doesn't exist
	if err := ucm.ensureUCISection(); err != nil {
		return fmt.Errorf("failed to create UCI section: %v", err)
	}

	// Save all configuration values
	configs := map[string]string{
		"autonomy.api.enabled":         fmt.Sprintf("%v", ucm.config.Enabled),
		"autonomy.api.port":            fmt.Sprintf("%d", ucm.config.Port),
		"autonomy.api.bind_address":    ucm.config.BindAddress,
		"autonomy.api.enable_cors":     fmt.Sprintf("%v", ucm.config.EnableCORS),
		"autonomy.api.request_timeout": fmt.Sprintf("%d", ucm.config.RequestTimeout),
		"autonomy.api.max_connections": fmt.Sprintf("%d", ucm.config.MaxConnections),
		"autonomy.api.log_requests":    fmt.Sprintf("%v", ucm.config.LogRequests),
	}

	for key, value := range configs {
		if err := ucm.setUCIValue(key, value); err != nil {
			return fmt.Errorf("failed to set %s: %v", key, err)
		}
	}

	// Commit changes
	if err := ucm.commitUCI(); err != nil {
		return fmt.Errorf("failed to commit UCI changes: %v", err)
	}

	fmt.Println("✅ API server configuration saved to UCI")
	return nil
}

// GetConfig returns the current API server configuration
func (ucm *UCIAPIConfigManager) GetConfig() APIServerConfig {
	return ucm.config
}

// SetConfig updates the API server configuration
func (ucm *UCIAPIConfigManager) SetConfig(config APIServerConfig) error {
	if err := ucm.validateConfigStruct(config); err != nil {
		return fmt.Errorf("invalid configuration: %v", err)
	}
	ucm.config = config
	return nil
}

// validateConfig validates the current configuration
func (ucm *UCIAPIConfigManager) validateConfig() error {
	return ucm.validateConfigStruct(ucm.config)
}

// validateConfigStruct validates a configuration struct
func (ucm *UCIAPIConfigManager) validateConfigStruct(config APIServerConfig) error {
	// Validate port range
	if config.Port < 1 || config.Port > 65535 {
		return fmt.Errorf("port must be between 1 and 65535, got %d", config.Port)
	}

	// Validate bind address
	if config.BindAddress != "" {
		if ip := net.ParseIP(config.BindAddress); ip == nil && config.BindAddress != "0.0.0.0" {
			return fmt.Errorf("invalid bind address: %s", config.BindAddress)
		}
	}

	// Validate timeout
	if config.RequestTimeout < 1 || config.RequestTimeout > 300 {
		return fmt.Errorf("request timeout must be between 1 and 300 seconds, got %d", config.RequestTimeout)
	}

	// Validate max connections
	if config.MaxConnections < 1 || config.MaxConnections > 10000 {
		return fmt.Errorf("max connections must be between 1 and 10000, got %d", config.MaxConnections)
	}

	return nil
}

// CheckPortAvailability checks if the configured port is available
func (ucm *UCIAPIConfigManager) CheckPortAvailability() error {
	address := fmt.Sprintf("%s:%d", ucm.config.BindAddress, ucm.config.Port)

	fmt.Printf("🔍 Checking port availability: %s\n", address)

	// Try to bind to the port temporarily
	listener, err := net.Listen("tcp", address)
	if err != nil {
		if strings.Contains(err.Error(), "bind: address already in use") {
			return fmt.Errorf("port %d is already in use on %s", ucm.config.Port, ucm.config.BindAddress)
		}
		if strings.Contains(err.Error(), "bind: cannot assign requested address") {
			return fmt.Errorf("cannot bind to address %s (address not available)", ucm.config.BindAddress)
		}
		return fmt.Errorf("failed to check port availability: %v", err)
	}

	// Close the listener immediately
	listener.Close()

	fmt.Printf("✅ Port %d is available on %s\n", ucm.config.Port, ucm.config.BindAddress)
	return nil
}

// FindAvailablePort finds an available port starting from the configured port
func (ucm *UCIAPIConfigManager) FindAvailablePort() (int, error) {
	startPort := ucm.config.Port
	maxAttempts := 100

	fmt.Printf("🔍 Finding available port starting from %d...\n", startPort)

	for i := 0; i < maxAttempts; i++ {
		port := startPort + i
		if port > 65535 {
			break
		}

		address := fmt.Sprintf("%s:%d", ucm.config.BindAddress, port)
		listener, err := net.Listen("tcp", address)
		if err == nil {
			listener.Close()
			fmt.Printf("✅ Found available port: %d\n", port)
			return port, nil
		}
	}

	return 0, fmt.Errorf("no available ports found in range %d-%d", startPort, startPort+maxAttempts-1)
}

// Helper methods for UCI operations
func (ucm *UCIAPIConfigManager) getUCIString(key, defaultValue string) (string, error) {
	cmd := fmt.Sprintf("uci get %s 2>/dev/null || echo '%s'", key, defaultValue)
	output, err := executeCommand(ucm.sshClient, cmd)
	if err != nil {
		return defaultValue, err
	}
	return strings.TrimSpace(output), nil
}

func (ucm *UCIAPIConfigManager) getUCIInt(key string, defaultValue int) (int, error) {
	value, err := ucm.getUCIString(key, fmt.Sprintf("%d", defaultValue))
	if err != nil {
		return defaultValue, err
	}

	intValue, err := strconv.Atoi(value)
	if err != nil {
		return defaultValue, fmt.Errorf("invalid integer value for %s: %s", key, value)
	}

	return intValue, nil
}

func (ucm *UCIAPIConfigManager) getUCIBool(key string, defaultValue bool) (bool, error) {
	value, err := ucm.getUCIString(key, fmt.Sprintf("%v", defaultValue))
	if err != nil {
		return defaultValue, err
	}

	switch strings.ToLower(strings.TrimSpace(value)) {
	case "true", "1", "yes", "on", "enabled":
		return true, nil
	case "false", "0", "no", "off", "disabled":
		return false, nil
	default:
		return defaultValue, fmt.Errorf("invalid boolean value for %s: %s", key, value)
	}
}

func (ucm *UCIAPIConfigManager) setUCIValue(key, value string) error {
	cmd := fmt.Sprintf("uci set %s='%s'", key, value)
	_, err := executeCommand(ucm.sshClient, cmd)
	return err
}

func (ucm *UCIAPIConfigManager) ensureUCISection() error {
	// Check if autonomy package exists
	cmd := "uci show autonomy 2>/dev/null || uci set autonomy=package"
	_, err := executeCommand(ucm.sshClient, cmd)
	if err != nil {
		return err
	}

	// Check if api section exists
	cmd = "uci show autonomy.api 2>/dev/null || uci set autonomy.api=api"
	_, err = executeCommand(ucm.sshClient, cmd)
	return err
}

func (ucm *UCIAPIConfigManager) commitUCI() error {
	cmd := "uci commit autonomy"
	_, err := executeCommand(ucm.sshClient, cmd)
	return err
}

// testUCIAPIConfig tests the UCI API configuration functionality
func testUCIAPIConfig() {
	fmt.Println("⚙️  UCI API Configuration Test")
	fmt.Println("=============================")

	// Create SSH connection
	sshClient, err := createSSHClient()
	if err != nil {
		fmt.Printf("❌ SSH connection failed: %v\n", err)
		return
	}
	defer sshClient.Close()

	// Create UCI config manager
	configManager := NewUCIAPIConfigManager(sshClient)

	fmt.Println("\n📋 Testing configuration loading...")
	if err := configManager.LoadConfig(); err != nil {
		fmt.Printf("❌ Failed to load config: %v\n", err)
		return
	}

	config := configManager.GetConfig()

	fmt.Println("\n🔍 Testing port availability...")
	if err := configManager.CheckPortAvailability(); err != nil {
		fmt.Printf("⚠️  Port check failed: %v\n", err)

		// Try to find an available port
		if availablePort, err := configManager.FindAvailablePort(); err == nil {
			fmt.Printf("💡 Suggested available port: %d\n", availablePort)
		}
	}

	fmt.Println("\n💾 Testing configuration save...")
	if err := configManager.SaveConfig(); err != nil {
		fmt.Printf("❌ Failed to save config: %v\n", err)
		return
	}

	fmt.Println("\n📊 Current API Configuration:")
	fmt.Printf("   • Enabled: %v\n", config.Enabled)
	fmt.Printf("   • Port: %d\n", config.Port)
	fmt.Printf("   • Bind Address: %s\n", config.BindAddress)
	fmt.Printf("   • CORS Enabled: %v\n", config.EnableCORS)
	fmt.Printf("   • Request Timeout: %ds\n", config.RequestTimeout)
	fmt.Printf("   • Max Connections: %d\n", config.MaxConnections)
	fmt.Printf("   • Log Requests: %v\n", config.LogRequests)

	fmt.Println("\n📝 UCI Commands to Configure API Server:")
	fmt.Println("========================================")
	fmt.Println("# Enable API server:")
	fmt.Println("uci set autonomy.api.enabled='true'")
	fmt.Println("")
	fmt.Println("# Set custom port:")
	fmt.Println("uci set autonomy.api.port='9090'")
	fmt.Println("")
	fmt.Println("# Set bind address (default: 0.0.0.0):")
	fmt.Println("uci set autonomy.api.bind_address='127.0.0.1'")
	fmt.Println("")
	fmt.Println("# Disable CORS:")
	fmt.Println("uci set autonomy.api.enable_cors='false'")
	fmt.Println("")
	fmt.Println("# Set request timeout (seconds):")
	fmt.Println("uci set autonomy.api.request_timeout='60'")
	fmt.Println("")
	fmt.Println("# Commit changes:")
	fmt.Println("uci commit autonomy")

	fmt.Println("\n✅ UCI API Configuration Test Complete!")
}
