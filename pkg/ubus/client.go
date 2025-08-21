package ubus

import (
	"bufio"
	"context"
	"encoding/binary"
	"encoding/json"
	"fmt"
	"io"
	"net"
	"os/exec"
	"strings"
	"sync"

	"github.com/markus-lassfolk/autonomy/pkg/logx"
)

// Client represents a ubus client
type Client struct {
	logger    *logx.Logger
	conn      net.Conn
	connected bool
	mu        sync.RWMutex
	callID    uint32
	callMu    sync.Mutex
	// Registered handlers for incoming requests
	handlers map[string]map[string]MethodHandler
	// Registered objects
	objects map[string]bool
}

// Message represents a ubus message
type Message struct {
	Type    string          `json:"type"`
	Method  string          `json:"method,omitempty"`
	Path    string          `json:"path,omitempty"`
	Data    json.RawMessage `json:"data,omitempty"`
	ID      uint32          `json:"id,omitempty"`
	Code    int             `json:"code,omitempty"`
	Message string          `json:"message,omitempty"`
}

// MethodHandler represents a ubus method handler
type MethodHandler func(ctx context.Context, data json.RawMessage) (interface{}, error)

// NewClient creates a new ubus client
func NewClient(logger *logx.Logger) *Client {
	return &Client{
		logger:   logger,
		handlers: make(map[string]map[string]MethodHandler),
		objects:  make(map[string]bool),
	}
}

// Connect connects to the ubus daemon
func (c *Client) Connect(ctx context.Context) error {
	c.mu.Lock()
	defer c.mu.Unlock()

	if c.connected {
		return nil
	}

	// Try to connect to ubus socket - check multiple possible paths
	socketPaths := []string{
		"/var/run/ubus/ubus.sock", // RUTOS path
		"/var/run/ubus.sock",      // Standard OpenWrt path
	}

	var conn net.Conn
	var err error
	for _, path := range socketPaths {
		conn, err = net.Dial("unix", path)
		if err == nil {
			if c.logger != nil {
				c.logger.Debug("Connected to ubus socket", "path", path)
			}
			break
		}
	}
	if err != nil {
		return fmt.Errorf("failed to connect to ubus socket: %w", err)
	}

	c.conn = conn
	c.connected = true

	if c.logger != nil {
		c.logger.Info("Connected to ubus daemon")
	}

	return nil
}

// Disconnect disconnects from the ubus daemon
func (c *Client) Disconnect() error {
	c.mu.Lock()
	defer c.mu.Unlock()

	if !c.connected {
		return nil
	}

	if c.conn != nil {
		c.conn.Close()
	}

	c.connected = false

	if c.logger != nil {
		c.logger.Info("Disconnected from ubus daemon")
	}

	return nil
}

// Call makes a ubus call
func (c *Client) Call(ctx context.Context, object, method string, data interface{}) (json.RawMessage, error) {
	// Always use CLI approach for RUTOS compatibility
	// Socket-based approach is disabled due to protocol complexity
	return c.callViaCLI(ctx, object, method, data)
}

// CallViaCLI makes a ubus call using the CLI (public method)
func (c *Client) CallViaCLI(ctx context.Context, object, method string, data interface{}) (json.RawMessage, error) {
	return c.callViaCLI(ctx, object, method, data)
}

// callViaCLI makes a ubus call using the CLI
func (c *Client) callViaCLI(ctx context.Context, object, method string, data interface{}) (json.RawMessage, error) {
	args := []string{"call", object, method}

	var payload []byte
	if data != nil {
		b, err := json.Marshal(data)
		if err != nil {
			return nil, fmt.Errorf("failed to marshal data: %w", err)
		}
		payload = b
		args = append(args, string(payload))
	}

	// Execute ubus CLI command
	cmd := exec.CommandContext(ctx, "ubus", args...)
	output, err := cmd.Output()
	if err != nil {
		return nil, fmt.Errorf("ubus command failed: %w", err)
	}

	return json.RawMessage(output), nil
}

// RegisterObject registers an object with the ubus daemon
func (c *Client) RegisterObject(ctx context.Context, name string, methods map[string]MethodHandler) error {
	c.mu.Lock()

	defer c.mu.Unlock()

	if c.handlers == nil {
		c.handlers = make(map[string]map[string]MethodHandler)
	}

	c.handlers[name] = methods

	if c.logger != nil {
		c.logger.Info("Registering ubus object", "name", name, "methods", len(methods))
	}

	methodNames := make([]string, 0, len(methods))
	for m := range methods {
		methodNames = append(methodNames, m)
	}

	data, err := json.Marshal(methodNames)
	if err != nil {
		return fmt.Errorf("failed to marshal method names: %w", err)
	}
	msg := &Message{Type: "register", Path: name, Data: data}

	c.callMu.Lock()
	defer c.callMu.Unlock()
	if err := c.sendMessage(msg); err != nil {
		return fmt.Errorf("failed to register object: %w", err)
	}

	resp, err := c.readMessage()
	if err != nil {
		return fmt.Errorf("failed to read register response: %w", err)
	}
	if resp.Code != 0 {
		return fmt.Errorf("registration failed: %s", resp.Message)
	}
	return nil
}

// UnregisterObject unregisters an object from the ubus daemon
func (c *Client) UnregisterObject(ctx context.Context, name string) error {
	if c.logger != nil {
		c.logger.Info("Unregistering ubus object", "name", name)
	}

	c.mu.Lock()
	delete(c.objects, name)
	c.mu.Unlock()

	msg := &Message{Type: "unregister", Path: name}
	c.callMu.Lock()
	defer c.callMu.Unlock()
	if err := c.sendMessage(msg); err != nil {
		return fmt.Errorf("failed to unregister object: %w", err)
	}
	return nil
}

// Listen listens for ubus messages
func (c *Client) Listen(ctx context.Context) error {
	c.mu.RLock()
	conn := c.conn
	connected := c.connected
	c.mu.RUnlock()

	if !connected || conn == nil {
		return fmt.Errorf("not connected to ubus")
	}

	reader := bufio.NewReader(conn)

	for {
		select {
		case <-ctx.Done():
			return ctx.Err()
		default:

			line, err := reader.ReadBytes('\n')
			if err != nil {
				return err
			}

			var msg Message
			if err := json.Unmarshal(line, &msg); err != nil {
				if c.logger != nil {
					c.logger.Error("Failed to decode ubus message", "error", err)
				}
				continue
			}

			go c.handleMessage(ctx, msg)
		}
	}
}

// callViaSocket makes a ubus call using the direct socket connection
func (c *Client) callViaSocket(ctx context.Context, conn net.Conn, object, method string, data interface{}) (json.RawMessage, error) {
	msg := Message{
		Type:   "call",
		Path:   object,
		Method: method,
		ID:     c.GetNextCallID(),
	}

	if data != nil {
		raw, err := json.Marshal(data)
		if err != nil {
			return nil, err
		}
		msg.Data = raw
	}

	payload, err := json.Marshal(msg)
	if err != nil {
		return nil, err
	}

	if deadline, ok := ctx.Deadline(); ok {
		conn.SetDeadline(deadline)
	}

	if _, err := conn.Write(append(payload, '\n')); err != nil {
		return nil, err
	}

	reader := bufio.NewReader(conn)
	line, err := reader.ReadBytes('\n')
	if err != nil {
		return nil, err
	}

	var resp Message
	if err := json.Unmarshal(line, &resp); err != nil {
		return nil, err
	}

	if resp.Code != 0 {
		return nil, fmt.Errorf("ubus error: %d %s", resp.Code, resp.Message)
	}

	return resp.Data, nil
}

// handleMessage handles incoming messages from the ubus socket
func (c *Client) handleMessage(ctx context.Context, msg Message) {
	if msg.Type != "request" {
		return
	}

	c.mu.RLock()
	methods := c.handlers[msg.Path]
	handler := methods[msg.Method]
	conn := c.conn
	c.mu.RUnlock()

	if handler == nil {
		if c.logger != nil {
			c.logger.Warn("No handler for ubus method", "path", msg.Path, "method", msg.Method)
		}
		return
	}

	data, err := handler(ctx, msg.Data)
	resp := Message{Type: "response", ID: msg.ID}
	if err != nil {
		resp.Code = -1
		resp.Message = err.Error()
	} else if data != nil {
		if raw, err := json.Marshal(data); err == nil {
			resp.Data = raw
		}
	}

	if conn != nil {
		if payload, err := json.Marshal(resp); err == nil {
			conn.Write(append(payload, '\n'))
		}

		result, err := handler(ctx, msg.Data)
		resp := &Message{Type: "response", ID: msg.ID}
		if err != nil {
			resp.Code = 500
			resp.Message = err.Error()
		} else if result != nil {
			if b, err := json.Marshal(result); err == nil {
				resp.Data = b
			} else {
				resp.Code = 500
				resp.Message = err.Error()
			}
		}

		c.callMu.Lock()
		if err := c.sendMessage(resp); err != nil {
			c.logger.Error("failed to send response", "error", err)
		}
		c.callMu.Unlock()
	}
}

// sendMessage encodes and sends a message with length framing
func (c *Client) sendMessage(msg *Message) error {
	data, err := json.Marshal(msg)
	if err != nil {
		return err
	}
	length := uint32(len(data))
	header := make([]byte, 4)
	binary.BigEndian.PutUint32(header, length)
	if _, err := c.conn.Write(header); err != nil {
		return err
	}
	_, err = c.conn.Write(data)
	return err
}

// readMessage reads a framed message from the connection
func (c *Client) readMessage() (*Message, error) {
	header := make([]byte, 4)
	if _, err := io.ReadFull(c.conn, header); err != nil {
		return nil, err
	}
	length := binary.BigEndian.Uint32(header)
	body := make([]byte, length)
	if _, err := io.ReadFull(c.conn, body); err != nil {
		return nil, err
	}
	var msg Message
	if err := json.Unmarshal(body, &msg); err != nil {
		return nil, err
	}
	return &msg, nil
}

// ListObjects lists available ubus objects
func (c *Client) ListObjects(ctx context.Context) ([]string, error) {
	cmd := exec.CommandContext(ctx, "ubus", "list")
	output, err := cmd.Output()
	if err != nil {
		return nil, fmt.Errorf("failed to list ubus objects: %w", err)
	}

	lines := strings.Split(strings.TrimSpace(string(output)), "\n")
	var objects []string
	for _, line := range lines {
		if line != "" {
			objects = append(objects, strings.TrimSpace(line))
		}
	}

	return objects, nil
}

// ListMethods lists methods for a specific object
func (c *Client) ListMethods(ctx context.Context, object string) (map[string]interface{}, error) {
	cmd := exec.CommandContext(ctx, "ubus", "list", object)
	output, err := cmd.Output()
	if err != nil {
		return nil, fmt.Errorf("failed to list methods for object %s: %w", object, err)
	}

	var methods map[string]interface{}
	if err := json.Unmarshal(output, &methods); err != nil {
		return nil, fmt.Errorf("failed to parse methods: %w", err)
	}

	return methods, nil
}

// ValidateUbus checks if ubus is available and working
func (c *Client) ValidateUbus(ctx context.Context) error {
	_, err := exec.CommandContext(ctx, "ubus", "version").Output()
	if err != nil {
		return fmt.Errorf("ubus is not available: %w", err)
	}
	return nil
}

// GetSystemInfo gets system information via ubus
func (c *Client) GetSystemInfo(ctx context.Context) (map[string]interface{}, error) {
	output, err := c.Call(ctx, "system", "info", nil)
	if err != nil {
		return nil, err
	}

	var info map[string]interface{}
	if err := json.Unmarshal(output, &info); err != nil {
		return nil, fmt.Errorf("failed to parse system info: %w", err)
	}

	return info, nil
}

// GetNetworkInfo gets network information via ubus
func (c *Client) GetNetworkInfo(ctx context.Context) (map[string]interface{}, error) {
	output, err := c.Call(ctx, "network.device", "status", nil)
	if err != nil {
		return nil, err
	}

	var info map[string]interface{}
	if err := json.Unmarshal(output, &info); err != nil {
		return nil, fmt.Errorf("failed to parse network info: %w", err)
	}

	return info, nil
}

// GetInterfaceStatus gets the status of a specific interface
func (c *Client) GetInterfaceStatus(ctx context.Context, iface string) (map[string]interface{}, error) {
	data := map[string]interface{}{
		"name": iface,
	}

	output, err := c.Call(ctx, "network.device", "status", data)
	if err != nil {
		return nil, err
	}

	var status map[string]interface{}
	if err := json.Unmarshal(output, &status); err != nil {
		return nil, fmt.Errorf("failed to parse interface status: %w", err)
	}

	return status, nil
}

// GetMWAN3Status gets mwan3 status via ubus
func (c *Client) GetMWAN3Status(ctx context.Context) (map[string]interface{}, error) {
	output, err := c.Call(ctx, "mwan3", "status", nil)
	if err != nil {
		return nil, err
	}

	var status map[string]interface{}
	if err := json.Unmarshal(output, &status); err != nil {
		return nil, fmt.Errorf("failed to parse mwan3 status: %w", err)
	}

	return status, nil
}

// GetMWAN3Interfaces gets mwan3 interfaces via ubus
func (c *Client) GetMWAN3Interfaces(ctx context.Context) (map[string]interface{}, error) {
	output, err := c.Call(ctx, "mwan3", "interfaces", nil)
	if err != nil {
		return nil, err
	}

	var interfaces map[string]interface{}
	if err := json.Unmarshal(output, &interfaces); err != nil {
		return nil, fmt.Errorf("failed to parse mwan3 interfaces: %w", err)
	}

	return interfaces, nil
}

// GetMWAN3Members gets mwan3 members via ubus
func (c *Client) GetMWAN3Members(ctx context.Context) (map[string]interface{}, error) {
	output, err := c.Call(ctx, "mwan3", "members", nil)
	if err != nil {
		return nil, err
	}

	var members map[string]interface{}
	if err := json.Unmarshal(output, &members); err != nil {
		return nil, fmt.Errorf("failed to parse mwan3 members: %w", err)
	}

	return members, nil
}

// GetMWAN3Policies gets mwan3 policies via ubus
func (c *Client) GetMWAN3Policies(ctx context.Context) (map[string]interface{}, error) {
	output, err := c.Call(ctx, "mwan3", "policies", nil)
	if err != nil {
		return nil, err
	}

	var policies map[string]interface{}
	if err := json.Unmarshal(output, &policies); err != nil {
		return nil, fmt.Errorf("failed to parse mwan3 policies: %w", err)
	}

	return policies, nil
}

// GetCellularInfo gets cellular information via ubus (RutOS specific)
func (c *Client) GetCellularInfo(ctx context.Context) (map[string]interface{}, error) {
	// Try different cellular ubus objects
	objects := []string{"mobiled", "gsm", "cellular"}

	for _, object := range objects {
		output, err := c.Call(ctx, object, "status", nil)
		if err == nil {
			var info map[string]interface{}
			if err := json.Unmarshal(output, &info); err == nil {
				return info, nil
			}
		}
	}

	return nil, fmt.Errorf("no cellular ubus object found")
}

// GetWiFiInfo gets WiFi information via ubus
func (c *Client) GetWiFiInfo(ctx context.Context) (map[string]interface{}, error) {
	output, err := c.Call(ctx, "iwinfo", "info", nil)
	if err != nil {
		return nil, err
	}

	var info map[string]interface{}
	if err := json.Unmarshal(output, &info); err != nil {
		return nil, fmt.Errorf("failed to parse WiFi info: %w", err)
	}

	return info, nil
}

// GetWiFiDevices gets WiFi devices via ubus
func (c *Client) GetWiFiDevices(ctx context.Context) ([]string, error) {
	output, err := c.Call(ctx, "iwinfo", "devices", nil)
	if err != nil {
		return nil, err
	}

	var devices []string
	if err := json.Unmarshal(output, &devices); err != nil {
		return nil, fmt.Errorf("failed to parse WiFi devices: %w", err)
	}

	return devices, nil
}

// GetWiFiDeviceInfo gets information for a specific WiFi device
func (c *Client) GetWiFiDeviceInfo(ctx context.Context, device string) (map[string]interface{}, error) {
	data := map[string]interface{}{
		"device": device,
	}

	output, err := c.Call(ctx, "iwinfo", "info", data)
	if err != nil {
		return nil, err
	}

	var info map[string]interface{}
	if err := json.Unmarshal(output, &info); err != nil {
		return nil, fmt.Errorf("failed to parse WiFi device info: %w", err)
	}

	return info, nil
}

// IsConnected returns whether the client is connected to ubus
func (c *Client) IsConnected() bool {
	c.mu.RLock()
	defer c.mu.RUnlock()
	return c.connected
}

// GetNextCallID returns the next call ID
func (c *Client) GetNextCallID() uint32 {
	c.callMu.Lock()
	defer c.callMu.Unlock()
	c.callID++
	return c.callID
}

// SetConn allows injection of a custom connection (primarily for testing)
func (c *Client) SetConn(conn net.Conn) {
	c.mu.Lock()
	defer c.mu.Unlock()
	c.conn = conn
	c.connected = conn != nil
}
