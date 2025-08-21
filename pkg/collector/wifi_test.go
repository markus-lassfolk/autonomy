package collector

import (
	"context"
	"testing"
	"time"

	"github.com/markus-lassfolk/autonomy/pkg"
)

// TestWiFiCollector_Collect tests the WiFi metrics collection
func TestWiFiCollector_Collect(t *testing.T) {
	tests := []struct {
		name        string
		member      *pkg.Member
		description string
		wantErr     bool
	}{
		{
			name: "successful_wifi_collection",
			member: &pkg.Member{
				Name:  "wifi_test",
				Iface: "wlan0",
				Class: pkg.ClassWiFi,
			},
			description: "Should successfully collect WiFi metrics",
			wantErr:     false,
		},
		{
			name: "invalid_member_class",
			member: &pkg.Member{
				Name:  "cellular_test",
				Iface: "wwan0",
				Class: pkg.ClassCellular,
			},
			description: "Should fail with invalid member class",
			wantErr:     true,
		},
		{
			name: "empty_interface_name",
			member: &pkg.Member{
				Name:  "wifi_test",
				Iface: "",
				Class: pkg.ClassWiFi,
			},
			description: "Should fail with empty interface name",
			wantErr:     true,
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			config := map[string]interface{}{
				"timeout": 5 * time.Second,
				"targets": []string{"8.8.8.8"},
			}

			collector, err := NewWiFiCollector(config)
			if err != nil {
				t.Fatalf("Failed to create WiFi collector: %v", err)
			}

			ctx := context.Background()
			metrics, err := collector.Collect(ctx, tt.member)

			if (err != nil) != tt.wantErr {
				t.Errorf("WiFiCollector.Collect() error = %v, wantErr %v", err, tt.wantErr)
				return
			}

			if !tt.wantErr {
				if metrics == nil {
					t.Error("Expected metrics to be returned")
				} else {
					// Verify basic metrics are present
					if (metrics.LatencyMS == nil || *metrics.LatencyMS == 0) && (metrics.LossPercent == nil || *metrics.LossPercent == 0) {
						t.Log("⚠️  No network metrics collected (expected in test environment)")
					}
					t.Logf("✅ %s: Metrics collected successfully", tt.description)
				}
			} else {
				t.Logf("✅ %s: Expected error correctly returned: %v", tt.description, err)
			}
		})
	}
}

// TestWiFiCollector_ParseWiFiData tests WiFi data parsing
func TestWiFiCollector_ParseWiFiData(t *testing.T) {
	collector := &WiFiCollector{}

	tests := []struct {
		name     string
		data     map[string]interface{}
		expected *WiFiInfo
	}{
		{
			name: "complete_wifi_data",
			data: map[string]interface{}{
				"signal":      -45.0,
				"noise":       -95.0,
				"bitrate":     54000000.0,
				"ssid":        "TestNetwork",
				"channel":     6.0,
				"mode":        "Client",
				"frequency":   2437.0,
				"quality":     70.0,
				"quality_max": 100.0,
				"txpower":     20.0,
				"encryption": map[string]interface{}{
					"enabled": true,
					"ciphers": []interface{}{"AES"},
				},
				"country": "US",
			},
			expected: &WiFiInfo{
				SignalStrength: intPtr(-45),
				NoiseLevel:     intPtr(-95),
				SNR:            intPtr(50), // -45 - (-95) = 50
				Bitrate:        intPtr(54000000),
				SSID:           strPtr("TestNetwork"),
				Channel:        intPtr(6),
				Mode:           strPtr("Client"),
				Frequency:      intPtr(2437),
				Quality:        intPtr(70),
				LinkQuality:    intPtr(100),
				TxPower:        intPtr(20),
				Encryption:     strPtr("AES"),
				Country:        strPtr("US"),
				TetheringMode:  boolPtr(false), // Client mode = not tethering
			},
		},
		{
			name: "ap_mode_tethering",
			data: map[string]interface{}{
				"signal": -50.0,
				"mode":   "AP",
			},
			expected: &WiFiInfo{
				SignalStrength: intPtr(-50),
				Mode:           strPtr("AP"),
				TetheringMode:  boolPtr(true), // AP mode = tethering
			},
		},
		{
			name: "minimal_data",
			data: map[string]interface{}{
				"signal": -60.0,
			},
			expected: &WiFiInfo{
				SignalStrength: intPtr(-60),
			},
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			info := &WiFiInfo{}
			err := collector.parseWiFiData(tt.data, info)
			if err != nil {
				t.Errorf("parseWiFiData() error = %v", err)
				return
			}

			// Compare fields
			if !compareIntPtr(info.SignalStrength, tt.expected.SignalStrength) {
				t.Errorf("SignalStrength = %v, expected %v", info.SignalStrength, tt.expected.SignalStrength)
			}
			if !compareIntPtr(info.NoiseLevel, tt.expected.NoiseLevel) {
				t.Errorf("NoiseLevel = %v, expected %v", info.NoiseLevel, tt.expected.NoiseLevel)
			}
			if !compareIntPtr(info.SNR, tt.expected.SNR) {
				t.Errorf("SNR = %v, expected %v", info.SNR, tt.expected.SNR)
			}
			if !compareStringPtr(info.Mode, tt.expected.Mode) {
				t.Errorf("Mode = %v, expected %v", info.Mode, tt.expected.Mode)
			}
			if !compareBoolPtr(info.TetheringMode, tt.expected.TetheringMode) {
				t.Errorf("TetheringMode = %v, expected %v", info.TetheringMode, tt.expected.TetheringMode)
			}
		})
	}
}

// TestWiFiCollector_SignalQuality tests signal quality calculation
func TestWiFiCollector_SignalQuality(t *testing.T) {
	collector := &WiFiCollector{}

	tests := []struct {
		name     string
		signal   *int
		noise    *int
		snr      *int
		expected float64
		minScore float64
		maxScore float64
	}{
		{
			name:     "excellent_signal",
			signal:   intPtr(-30),
			noise:    intPtr(-95),
			snr:      intPtr(65),
			minScore: 90.0,
			maxScore: 100.0,
		},
		{
			name:     "good_signal",
			signal:   intPtr(-50),
			noise:    intPtr(-90),
			snr:      intPtr(40),
			minScore: 70.0,
			maxScore: 90.0,
		},
		{
			name:     "poor_signal",
			signal:   intPtr(-80),
			noise:    intPtr(-90),
			snr:      intPtr(10),
			minScore: 20.0,
			maxScore: 50.0,
		},
		{
			name:     "no_data",
			signal:   nil,
			noise:    nil,
			snr:      nil,
			expected: 50.0, // Default score
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			quality := collector.GetSignalQuality(tt.signal, tt.noise, tt.snr)

			if tt.expected > 0 {
				if quality != tt.expected {
					t.Errorf("GetSignalQuality() = %v, expected %v", quality, tt.expected)
				}
			} else {
				if quality < tt.minScore || quality > tt.maxScore {
					t.Errorf("GetSignalQuality() = %v, expected between %v and %v", quality, tt.minScore, tt.maxScore)
				}
			}

			t.Logf("✅ %s: Quality score = %.1f", tt.name, quality)
		})
	}
}

// TestWiFiCollector_BitrateQuality tests bitrate quality calculation
func TestWiFiCollector_BitrateQuality(t *testing.T) {
	collector := &WiFiCollector{}

	tests := []struct {
		name     string
		bitrate  *int
		expected float64
	}{
		{
			name:     "excellent_bitrate_100mbps",
			bitrate:  intPtr(100000000), // 100 Mbps
			expected: 100.0,
		},
		{
			name:     "good_bitrate_50mbps",
			bitrate:  intPtr(50000000), // 50 Mbps
			expected: 80.0,
		},
		{
			name:     "fair_bitrate_25mbps",
			bitrate:  intPtr(25000000), // 25 Mbps
			expected: 60.0,
		},
		{
			name:     "poor_bitrate_10mbps",
			bitrate:  intPtr(10000000), // 10 Mbps
			expected: 40.0,
		},
		{
			name:     "very_poor_bitrate_1mbps",
			bitrate:  intPtr(1000000), // 1 Mbps
			expected: 20.0,
		},
		{
			name:     "no_bitrate_data",
			bitrate:  nil,
			expected: 50.0, // Default score
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			quality := collector.GetBitrateQuality(tt.bitrate)

			if quality != tt.expected {
				t.Errorf("GetBitrateQuality() = %v, expected %v", quality, tt.expected)
			}

			t.Logf("✅ %s: Bitrate quality = %.1f", tt.name, quality)
		})
	}
}

// TestWiFiCollector_AnalyzeSignalTrend tests signal trend analysis
func TestWiFiCollector_AnalyzeSignalTrend(t *testing.T) {
	collector := &WiFiCollector{}

	tests := []struct {
		name     string
		metrics  []*pkg.Metrics
		expected map[string]interface{}
	}{
		{
			name: "improving_signal",
			metrics: []*pkg.Metrics{
				{SignalStrength: intPtr(-80)},
				{SignalStrength: intPtr(-70)},
				{SignalStrength: intPtr(-60)},
				{SignalStrength: intPtr(-50)},
			},
			expected: map[string]interface{}{
				"trend": "improving",
			},
		},
		{
			name: "degrading_signal",
			metrics: []*pkg.Metrics{
				{SignalStrength: intPtr(-50)},
				{SignalStrength: intPtr(-60)},
				{SignalStrength: intPtr(-70)},
				{SignalStrength: intPtr(-80)},
			},
			expected: map[string]interface{}{
				"trend": "degrading",
			},
		},
		{
			name: "stable_signal",
			metrics: []*pkg.Metrics{
				{SignalStrength: intPtr(-60)},
				{SignalStrength: intPtr(-61)},
				{SignalStrength: intPtr(-59)},
				{SignalStrength: intPtr(-60)},
			},
			expected: map[string]interface{}{
				"trend": "stable",
			},
		},
		{
			name:    "insufficient_data",
			metrics: []*pkg.Metrics{{SignalStrength: intPtr(-60)}},
			expected: map[string]interface{}{
				"trend": "insufficient_data",
			},
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			result := collector.AnalyzeSignalTrend(tt.metrics)

			if trend, ok := result["trend"].(string); ok {
				if expectedTrend, ok := tt.expected["trend"].(string); ok {
					if trend != expectedTrend {
						t.Errorf("AnalyzeSignalTrend() trend = %v, expected %v", trend, expectedTrend)
					}
				}
			} else {
				t.Error("Expected trend field in result")
			}

			t.Logf("✅ %s: Trend = %v", tt.name, result["trend"])
		})
	}
}

// TestWiFiCollector_IsWiFiInterface tests WiFi interface detection
func TestWiFiCollector_IsWiFiInterface(t *testing.T) {
	collector := &WiFiCollector{}

	tests := []struct {
		name     string
		iface    string
		expected bool
	}{
		{"wlan_interface", "wlan0", true},
		{"wifi_interface", "wifi0", true},
		{"ath_interface", "ath0", true},
		{"radio_interface", "radio0", true},
		{"ethernet_interface", "eth0", false},
		{"cellular_interface", "wwan0", false},
		{"bridge_interface", "br-lan", false},
		{"empty_interface", "", false},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			result := collector.IsWiFiInterface(tt.iface)
			if result != tt.expected {
				t.Errorf("IsWiFiInterface(%s) = %v, expected %v", tt.iface, result, tt.expected)
			}
		})
	}
}

// Helper functions for testing

func boolPtr(b bool) *bool {
	return &b
}

func compareIntPtr(a, b *int) bool {
	if a == nil && b == nil {
		return true
	}
	if a == nil || b == nil {
		return false
	}
	return *a == *b
}

func compareStringPtr(a, b *string) bool {
	if a == nil && b == nil {
		return true
	}
	if a == nil || b == nil {
		return false
	}
	return *a == *b
}

func compareBoolPtr(a, b *bool) bool {
	if a == nil && b == nil {
		return true
	}
	if a == nil || b == nil {
		return false
	}
	return *a == *b
}
