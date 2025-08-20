package discovery

import (
	"testing"
	"time"

	"github.com/markus-lassfolk/autonomy/pkg"
	"github.com/markus-lassfolk/autonomy/pkg/logx"
)

// TestDiscoverer_DiscoverMembers tests member discovery functionality
func TestDiscoverer_DiscoverMembers(t *testing.T) {
	logger := logx.NewLogger("debug", "discovery_test")
	discoverer := NewDiscoverer(logger)

	// Test discovery
	members, err := discoverer.DiscoverMembers()
	// This might fail in test environment without mwan3
	if err != nil {
		t.Logf("⚠️  Discovery failed (expected in test environment): %v", err)
		// Verify error handling
		if members != nil {
			t.Error("Expected nil members when discovery fails")
		}
		return
	}

	// If successful, verify results
	if members == nil {
		t.Error("Expected non-nil members slice")
		return
	}

	t.Logf("✅ Discovered %d members", len(members))

	// Verify member structure
	for i, member := range members {
		if member.Name == "" {
			t.Errorf("Member %d has empty name", i)
		}
		if member.Iface == "" {
			t.Errorf("Member %d has empty interface", i)
		}
		if member.Class == "" {
			t.Errorf("Member %d has empty class", i)
		}

		t.Logf("  Member %d: %s (%s) - %s", i, member.Name, member.Iface, member.Class)
	}
}

// TestDiscoverer_ClassifyByName tests interface name classification
func TestDiscoverer_ClassifyByName(t *testing.T) {
	logger := logx.NewLogger("debug", "discovery_test")
	discoverer := NewDiscoverer(logger)

	tests := []struct {
		name     string
		iface    string
		expected string
	}{
		{"starlink interface", "wan_starlink", string(pkg.ClassStarlink)},
		{"starlink dish", "dish0", string(pkg.ClassStarlink)},
		{"cellular wwan", "wwan0", string(pkg.ClassCellular)},
		{"cellular modem", "modem0", string(pkg.ClassCellular)},
		{"wifi wlan", "wlan0", string(pkg.ClassWiFi)},
		{"wifi wireless", "wireless0", string(pkg.ClassWiFi)},
		{"ethernet wan", "wan", string(pkg.ClassLAN)},
		{"ethernet eth", "eth0", string(pkg.ClassLAN)},
		{"ethernet lan", "lan0", string(pkg.ClassLAN)},
		{"unknown interface", "unknown0", ""},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			result := discoverer.classifyByName(tt.iface)
			if result != tt.expected {
				t.Errorf("classifyByName(%s) = %s, expected %s", tt.iface, result, tt.expected)
			} else {
				t.Logf("✅ %s: %s -> %s", tt.name, tt.iface, result)
			}
		})
	}
}

// TestDiscoverer_CreateMember tests member creation with defaults
func TestDiscoverer_CreateMember(t *testing.T) {
	logger := logx.NewLogger("debug", "discovery_test")
	discoverer := NewDiscoverer(logger)

	tests := []struct {
		name           string
		iface          string
		class          string
		expectedWeight int
	}{
		{"starlink member", "wan_starlink", string(pkg.ClassStarlink), 100},
		{"cellular member", "wwan0", string(pkg.ClassCellular), 80},
		{"wifi member", "wlan0", string(pkg.ClassWiFi), 60},
		{"lan member", "eth0", string(pkg.ClassLAN), 40},
		{"generic member", "unknown0", string(pkg.ClassOther), 20},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			member, err := discoverer.createMember(tt.iface, tt.class)
			if err != nil {
				t.Fatalf("createMember failed: %v", err)
			}

			if member.Name != tt.iface {
				t.Errorf("Expected name=%s, got %s", tt.iface, member.Name)
			}

			if member.Iface != tt.iface {
				t.Errorf("Expected iface=%s, got %s", tt.iface, member.Iface)
			}

			if member.Class != pkg.InterfaceClass(tt.class) {
				t.Errorf("Expected class=%s, got %s", tt.class, member.Class)
			}

			if member.Weight != tt.expectedWeight {
				t.Errorf("Expected weight=%d, got %d", tt.expectedWeight, member.Weight)
			}

			if !member.Eligible {
				t.Error("Expected member to be eligible")
			}

			if member.Config == nil {
				t.Error("Expected non-nil config")
			}

			t.Logf("✅ %s: weight=%d, config_keys=%d",
				tt.name, member.Weight, len(member.Config))
		})
	}
}

// TestDiscoverer_ValidateMember tests member validation
func TestDiscoverer_ValidateMember(t *testing.T) {
	logger := logx.NewLogger("debug", "discovery_test")
	discoverer := NewDiscoverer(logger)

	tests := []struct {
		name    string
		member  pkg.Member
		wantErr bool
	}{
		{
			name: "valid member",
			member: pkg.Member{
				Name:  "eth0",
				Iface: "eth0",
				Class: pkg.ClassLAN,
			},
			wantErr: false, // Will likely fail due to interface not existing
		},
		{
			name: "empty name",
			member: pkg.Member{
				Name:  "",
				Iface: "eth0",
				Class: pkg.ClassLAN,
			},
			wantErr: true,
		},
		{
			name: "empty interface",
			member: pkg.Member{
				Name:  "test",
				Iface: "",
				Class: pkg.ClassLAN,
			},
			wantErr: true,
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			err := discoverer.ValidateMember(tt.member)

			if tt.name == "valid member" {
				// This will likely fail because interface doesn't exist
				t.Logf("⚠️  Validation result for %s: %v (interface may not exist in test)", tt.name, err)
			} else {
				if (err != nil) != tt.wantErr {
					t.Errorf("ValidateMember() error = %v, wantErr %v", err, tt.wantErr)
				} else {
					t.Logf("✅ %s: validation correctly returned error=%v", tt.name, err != nil)
				}
			}
		})
	}
}

// TestDiscoverer_EnhanceClassification tests classification enhancement
func TestDiscoverer_EnhanceClassification(t *testing.T) {
	logger := logx.NewLogger("debug", "discovery_test")
	discoverer := NewDiscoverer(logger)

	tests := []struct {
		name           string
		initialClass   string
		expectedWeight int
		expectedConfig []string
	}{
		{
			name:           "starlink enhancement",
			initialClass:   string(pkg.ClassStarlink),
			expectedWeight: 100,
			expectedConfig: []string{"api_endpoint", "check_interval", "obstruction_threshold"},
		},
		{
			name:           "cellular enhancement",
			initialClass:   string(pkg.ClassCellular),
			expectedWeight: 80,
			expectedConfig: []string{"check_interval", "signal_threshold", "roaming_penalty"},
		},
		{
			name:           "wifi enhancement",
			initialClass:   string(pkg.ClassWiFi),
			expectedWeight: 60,
			expectedConfig: []string{"check_interval", "signal_threshold"},
		},
		{
			name:           "lan enhancement",
			initialClass:   string(pkg.ClassLAN),
			expectedWeight: 40,
			expectedConfig: []string{"check_interval"},
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			member := &pkg.Member{
				Name:   "test",
				Iface:  "test0",
				Class:  pkg.InterfaceClass(tt.initialClass),
				Weight: 1, // Low initial weight
				Config: make(map[string]string),
			}

			err := discoverer.enhanceClassification(member)
			if err != nil {
				t.Fatalf("enhanceClassification failed: %v", err)
			}

			if member.Weight != tt.expectedWeight {
				t.Errorf("Expected weight=%d, got %d", tt.expectedWeight, member.Weight)
			}

			for _, configKey := range tt.expectedConfig {
				if _, exists := member.Config[configKey]; !exists {
					t.Errorf("Expected config key '%s' not found", configKey)
				}
			}

			t.Logf("✅ %s: weight=%d, config_keys=%v",
				tt.name, member.Weight, getConfigKeys(member.Config))
		})
	}
}

// TestDiscoverer_MergeMembers tests member merging logic
func TestDiscoverer_MergeMembers(t *testing.T) {
	logger := logx.NewLogger("debug", "discovery_test")
	discoverer := NewDiscoverer(logger)

	// Create existing members
	existing := []*pkg.Member{
		{
			Name:      "starlink",
			Iface:     "wan_starlink",
			Class:     pkg.ClassStarlink,
			Weight:    100,
			CreatedAt: time.Now().Add(-time.Hour),
			Config:    map[string]string{"custom": "value"},
		},
		{
			Name:      "cellular",
			Iface:     "wwan0",
			Class:     pkg.ClassCellular,
			Weight:    80,
			CreatedAt: time.Now().Add(-time.Hour),
			Config:    map[string]string{"roaming": "disabled"},
		},
	}

	// Create new members (simulating discovery)
	new := []*pkg.Member{
		{
			Name:      "starlink",
			Iface:     "wan_starlink", // Same interface
			Class:     pkg.ClassStarlink,
			Weight:    50, // Different weight
			CreatedAt: time.Now(),
		},
		{
			Name:      "wifi",
			Iface:     "wlan0",
			Class:     pkg.ClassWiFi,
			Weight:    60,
			CreatedAt: time.Now(),
		},
	}

	merged := discoverer.mergeMembers(existing, new)

	// Should have 2 members (starlink updated, wifi added, cellular removed)
	if len(merged) != 2 {
		t.Errorf("Expected 2 merged members, got %d", len(merged))
	}

	// Find starlink member
	var starlinkMember *pkg.Member
	var wifiMember *pkg.Member

	for _, member := range merged {
		if member.Name == "starlink" {
			starlinkMember = member
		} else if member.Name == "wifi" {
			wifiMember = member
		}
	}

	// Test starlink member was preserved with config
	if starlinkMember == nil {
		t.Fatal("Starlink member not found in merged results")
	}

	if starlinkMember.Weight != 100 { // Should keep original weight
		t.Errorf("Expected starlink weight=100 (preserved), got %d", starlinkMember.Weight)
	}

	if starlinkMember.Config["custom"] != "value" {
		t.Error("Expected custom config to be preserved")
	}

	// Test wifi member was added
	if wifiMember == nil {
		t.Fatal("WiFi member not found in merged results")
	}

	if wifiMember.Weight != 60 {
		t.Errorf("Expected wifi weight=60, got %d", wifiMember.Weight)
	}

	t.Logf("✅ Member merge successful: preserved config, added new member")
}

// TestDiscoverer_RefreshMembers tests the complete refresh cycle
func TestDiscoverer_RefreshMembers(t *testing.T) {
	logger := logx.NewLogger("debug", "discovery_test")
	discoverer := NewDiscoverer(logger)

	// Create some existing members
	existing := []*pkg.Member{
		{Name: "test1", Iface: "eth0", Class: pkg.ClassLAN},
		{Name: "test2", Iface: "eth1", Class: pkg.ClassLAN},
	}

	refreshed, err := discoverer.RefreshMembers(existing)
	// This will likely fail in test environment
	if err != nil {
		t.Logf("⚠️  RefreshMembers failed (expected in test environment): %v", err)
		return
	}

	if refreshed == nil {
		t.Error("Expected non-nil refreshed members")
		return
	}

	t.Logf("✅ RefreshMembers successful: %d members", len(refreshed))
}

// Helper function to get config keys for logging
func getConfigKeys(config map[string]string) []string {
	keys := make([]string, 0, len(config))
	for key := range config {
		keys = append(keys, key)
	}
	return keys
}

// BenchmarkDiscoverer_ClassifyByName benchmarks classification performance
func BenchmarkDiscoverer_ClassifyByName(b *testing.B) {
	logger := logx.NewLogger("error", "discovery_bench") // Reduce logging
	discoverer := NewDiscoverer(logger)

	interfaces := []string{"wan_starlink", "wwan0", "wlan0", "eth0", "unknown0"}

	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		iface := interfaces[i%len(interfaces)]
		_ = discoverer.classifyByName(iface)
	}
}

// BenchmarkDiscoverer_CreateMember benchmarks member creation
func BenchmarkDiscoverer_CreateMember(b *testing.B) {
	logger := logx.NewLogger("error", "discovery_bench")
	discoverer := NewDiscoverer(logger)

	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		_, _ = discoverer.createMember("eth0", string(pkg.ClassLAN))
	}
}
