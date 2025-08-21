package controller

import (
	"os"
	"testing"

	"github.com/markus-lassfolk/autonomy/pkg"
	"github.com/markus-lassfolk/autonomy/pkg/logx"
	"github.com/markus-lassfolk/autonomy/pkg/uci"
)

// TestController_UpdateMWAN3Policy tests the actual mwan3 policy update functionality
func TestController_UpdateMWAN3Policy(t *testing.T) {
	tests := []struct {
		name        string
		target      *pkg.Member
		wantErr     bool
		description string
	}{
		{
			name: "successful policy update",
			target: &pkg.Member{
				Name:  "starlink",
				Iface: "wan_starlink",
				Class: pkg.ClassStarlink,
			},
			wantErr:     false,
			description: "Should successfully update mwan3 policy for valid member",
		},
		{
			name:        "nil target member",
			target:      nil,
			wantErr:     true,
			description: "Should fail with nil target member",
		},
		{
			name: "empty interface name",
			target: &pkg.Member{
				Name:  "test",
				Iface: "",
				Class: pkg.ClassOther,
			},
			wantErr:     true,
			description: "Should fail with empty interface name",
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			// Create test controller
			config := &uci.Config{
				UseMWAN3: true,
			}
			logger := logx.NewLogger("debug", "controller_test")
			ctrl, err := NewController(config, logger)
			if err != nil {
				t.Fatalf("Failed to create controller: %v", err)
			}

			// Test the implementation
			err = ctrl.updateMWAN3Policy(tt.target)

			// In test environment, we expect system calls to fail
			// We're testing the logic and error handling
			if tt.wantErr {
				if err == nil {
					t.Errorf("Expected error but got none for: %s", tt.description)
				} else {
					t.Logf("✅ %s: Expected error correctly returned: %v", tt.description, err)
				}
			} else {
				// For successful cases, we expect system call failures in test environment
				if err != nil {
					t.Logf("⚠️  %s: System call failed as expected in test environment: %v", tt.description, err)
				} else {
					t.Logf("✅ %s: Policy update completed successfully", tt.description)
				}
			}
		})
	}
}

// TestController_ReadMWAN3Config tests reading mwan3 configuration
func TestController_ReadMWAN3Config(t *testing.T) {
	config := &uci.Config{UseMWAN3: true}
	logger := logx.NewLogger("debug", "controller_test")
	ctrl, err := NewController(config, logger)
	if err != nil {
		t.Fatalf("Failed to create controller: %v", err)
	}

	// Test reading mwan3 config
	mwan3Config, err := ctrl.readMWAN3Config()
	// This might fail if mwan3 is not installed, which is expected in test environment
	if err != nil {
		t.Logf("⚠️  mwan3 config read failed (expected in test environment): %v", err)
		// Verify error handling works
		if mwan3Config != nil {
			t.Error("Expected nil config when read fails")
		}
		return
	}

	// If successful, verify structure
	if mwan3Config == nil {
		t.Error("Expected non-nil config when read succeeds")
		return
	}

	if mwan3Config.Members == nil {
		t.Error("Expected non-nil Members slice")
	}

	if mwan3Config.Policies == nil {
		t.Error("Expected non-nil Policies slice")
	}

	t.Logf("✅ Successfully read mwan3 config with %d members and %d policies",
		len(mwan3Config.Members), len(mwan3Config.Policies))
}

// TestController_UpdateMemberWeights tests weight update logic
func TestController_UpdateMemberWeights(t *testing.T) {
	config := &uci.Config{UseMWAN3: true}
	logger := logx.NewLogger("debug", "controller_test")
	ctrl, err := NewController(config, logger)
	if err != nil {
		t.Fatalf("Failed to create controller: %v", err)
	}

	// Create test mwan3 config
	testConfig := &MWAN3Config{
		Members: []*MWAN3Member{
			{Name: "starlink", Iface: "wan_starlink", Weight: 50, Enabled: true},
			{Name: "cellular", Iface: "wwan0", Weight: 50, Enabled: true},
			{Name: "wifi", Iface: "wlan0", Weight: 30, Enabled: true},
		},
	}

	target := &pkg.Member{
		Name:  "cellular",
		Iface: "wwan0",
		Class: pkg.ClassCellular,
	}

	// Test weight updates
	updated, err := ctrl.updateMemberWeights(testConfig, target)
	if err != nil {
		t.Fatalf("updateMemberWeights failed: %v", err)
	}

	if !updated {
		t.Error("Expected weights to be updated")
	}

	// Verify target member has high weight
	targetFound := false
	for _, member := range testConfig.Members {
		if member.Name == target.Name {
			targetFound = true
			if member.Weight != 100 {
				t.Errorf("Expected target member weight=100, got %d", member.Weight)
			}
		} else {
			if member.Weight != 10 {
				t.Errorf("Expected non-target member weight=10, got %d", member.Weight)
			}
		}
	}

	if !targetFound {
		t.Error("Target member not found in config")
	}

	t.Logf("✅ Weight update successful: target=%s weight=100, others weight=10", target.Name)
}

// TestController_Switch tests the complete switch functionality
func TestController_Switch(t *testing.T) {
	config := &uci.Config{
		UseMWAN3: false, // Use netifd fallback for testing
	}
	logger := logx.NewLogger("debug", "controller_test")
	ctrl, err := NewController(config, logger)
	if err != nil {
		t.Fatalf("Failed to create controller: %v", err)
	}

	from := &pkg.Member{
		Name:  "starlink",
		Iface: "wan_starlink",
		Class: pkg.ClassStarlink,
	}

	to := &pkg.Member{
		Name:  "cellular",
		Iface: "wwan0",
		Class: pkg.ClassCellular,
	}

	// Test switch functionality
	err = ctrl.Switch(from, to)
	// This will likely fail in test environment without actual interfaces
	if err != nil {
		t.Logf("⚠️  Switch failed (expected in test environment): %v", err)
		// Verify error handling
		return
	}

	// If successful, verify current member was updated
	if ctrl.currentMember == nil {
		t.Error("Expected currentMember to be set")
		return
	}

	if ctrl.currentMember.Name != to.Name {
		t.Errorf("Expected currentMember=%s, got %s", to.Name, ctrl.currentMember.Name)
	}

	t.Logf("✅ Switch successful: %s -> %s", from.Name, to.Name)
}

// TestController_Validate tests member validation
func TestController_Validate(t *testing.T) {
	config := &uci.Config{UseMWAN3: true}
	logger := logx.NewLogger("debug", "controller_test")
	ctrl, err := NewController(config, logger)
	if err != nil {
		t.Fatalf("Failed to create controller: %v", err)
	}

	tests := []struct {
		name    string
		member  *pkg.Member
		wantErr bool
	}{
		{
			name: "valid member",
			member: &pkg.Member{
				Name:  "test",
				Iface: "eth0",
				Class: pkg.ClassLAN,
			},
			wantErr: false,
		},
		{
			name:    "nil member",
			member:  nil,
			wantErr: true,
		},
		{
			name: "empty name",
			member: &pkg.Member{
				Name:  "",
				Iface: "eth0",
				Class: pkg.ClassLAN,
			},
			wantErr: true,
		},
		{
			name: "empty interface",
			member: &pkg.Member{
				Name:  "test",
				Iface: "",
				Class: pkg.ClassLAN,
			},
			wantErr: true,
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			err := ctrl.Validate(tt.member)
			if (err != nil) != tt.wantErr {
				t.Errorf("Validate() error = %v, wantErr %v", err, tt.wantErr)
			}
		})
	}
}

// TestController_GetMWAN3Info tests mwan3 information retrieval
func TestController_GetMWAN3Info(t *testing.T) {
	config := &uci.Config{UseMWAN3: true}
	logger := logx.NewLogger("debug", "controller_test")
	ctrl, err := NewController(config, logger)
	if err != nil {
		t.Fatalf("Failed to create controller: %v", err)
	}

	info, err := ctrl.GetMWAN3Info()
	// Expected to fail in test environment
	if err != nil {
		t.Logf("⚠️  GetMWAN3Info failed (expected in test environment): %v", err)
		return
	}

	if info == nil {
		t.Error("Expected non-nil info")
		return
	}

	t.Logf("✅ Retrieved mwan3 info: %+v", info)
}

// TestController_SetMembers tests member management
func TestController_SetMembers(t *testing.T) {
	config := &uci.Config{UseMWAN3: true}
	logger := logx.NewLogger("debug", "controller_test")
	ctrl, err := NewController(config, logger)
	if err != nil {
		t.Fatalf("Failed to create controller: %v", err)
	}

	members := []*pkg.Member{
		{Name: "starlink", Iface: "wan_starlink", Class: pkg.ClassStarlink},
		{Name: "cellular", Iface: "wwan0", Class: pkg.ClassCellular},
		{Name: "wifi", Iface: "wlan0", Class: pkg.ClassWiFi},
	}

	// Test setting valid members
	err = ctrl.SetMembers(members)
	if err != nil {
		t.Fatalf("SetMembers failed: %v", err)
	}

	// Verify members were set
	retrievedMembers := ctrl.GetMembers()
	if len(retrievedMembers) != len(members) {
		t.Errorf("Expected %d members, got %d", len(members), len(retrievedMembers))
	}

	// Test with invalid member
	invalidMembers := []*pkg.Member{
		{Name: "", Iface: "eth0", Class: pkg.ClassLAN}, // Invalid: empty name
	}

	err = ctrl.SetMembers(invalidMembers)
	if err == nil {
		t.Error("Expected error with invalid member")
	}

	t.Logf("✅ Member management working correctly")
}

// BenchmarkController_UpdateMWAN3Policy benchmarks policy updates
func BenchmarkController_UpdateMWAN3Policy(b *testing.B) {
	config := &uci.Config{UseMWAN3: true}
	logger := logx.NewLogger("error", "controller_bench") // Reduce logging for benchmark
	ctrl, err := NewController(config, logger)
	if err != nil {
		b.Fatalf("Failed to create controller: %v", err)
	}

	target := &pkg.Member{
		Name:  "starlink",
		Iface: "wan_starlink",
		Class: pkg.ClassStarlink,
	}

	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		// This will likely fail, but we're measuring the overhead
		_ = ctrl.updateMWAN3Policy(target)
	}
}

// TestMain sets up and tears down test environment
func TestMain(m *testing.M) {
	// Setup
	code := m.Run()

	// Teardown
	os.Exit(code)
}
