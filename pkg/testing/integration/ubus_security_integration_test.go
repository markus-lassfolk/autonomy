package testing

import (
	"context"
	"encoding/json"
	"net"
	"strings"
	"testing"
	"time"

	"github.com/markus-lassfolk/autonomy/pkg/logx"
	"github.com/markus-lassfolk/autonomy/pkg/security"
	"github.com/markus-lassfolk/autonomy/pkg/ubus"
)

// TestUbusClientCall verifies that the ubus client can send and receive messages
func TestUbusClientCall(t *testing.T) {
	clientConn, serverConn := net.Pipe()
	defer clientConn.Close()
	defer serverConn.Close()

	c := ubus.NewClient(nil)
	c.SetConn(clientConn)

	ctx, cancel := context.WithTimeout(context.Background(), time.Second)
	defer cancel()

	// Fake server responding to the call
	go func() {
		decoder := json.NewDecoder(serverConn)
		encoder := json.NewEncoder(serverConn)
		var msg ubus.Message
		if err := decoder.Decode(&msg); err != nil {
			return
		}
		if msg.Type == "call" && msg.Method == "ping" {
			resp := ubus.Message{Type: "response", ID: msg.ID, Data: json.RawMessage(`{"reply":"pong"}`)}
			if err := encoder.Encode(resp); err != nil {
				return
			}
		}
	}()

	payload := map[string]string{"input": "test"}
	resp, err := c.Call(ctx, "test", "ping", payload)
	if err != nil {
		t.Fatalf("Call failed: %v", err)
	}
	var out map[string]string
	if err := json.Unmarshal(resp, &out); err != nil {
		t.Fatalf("Failed to unmarshal response: %v", err)
	}
	if out["reply"] != "pong" {
		t.Fatalf("Unexpected response: %v", out)
	}
}

// TestAuditorAccessControl verifies access control and suspicious activity detection
func TestAuditorAccessControl(t *testing.T) {
	cfg := &security.AuditConfig{
		Enabled:           true,
		AccessControl:     true,
		ThreatDetection:   true,
		MaxFailedAttempts: 3,
		BlockDuration:     1,
		AllowedIPs:        []string{"127.0.0.1"},
	}
	auditor := security.NewAuditor(cfg, logx.NewLogger("debug", "test"))

	// Repeated unauthorized access should trigger a block
	for i := 0; i < 3; i++ {
		if auditor.CheckAccess("10.0.0.1", "", "GET", "/secure") {
			t.Fatalf("unauthorized access allowed on attempt %d", i)
		}
	}
	if auditor.CheckAccess("10.0.0.1", "", "GET", "/secure") {
		t.Fatalf("blocked IP allowed access")
	}

	// Allowed IP making many requests should trigger suspicious activity
	for i := 0; i < 25; i++ {
		auditor.CheckAccess("127.0.0.1", "agent", "GET", "/data")
	}

	events := auditor.GetSecurityEvents()
	var blockEvent, suspiciousEvent bool
	for _, e := range events {
		if e.Message == "IP blocked due to failed attempts" {
			blockEvent = true
		}
		if strings.Contains(e.Message, "Suspicious activity detected") {
			suspiciousEvent = true
		}
	}
	if !blockEvent {
		t.Errorf("expected block event to be recorded")
	}
	if !suspiciousEvent {
		t.Errorf("expected suspicious activity event to be recorded")
	}
}
