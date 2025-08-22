package gps

import (
	"context"
	"testing"
	"time"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
)

func TestDefaultStarlinkAPICollectorConfig(t *testing.T) {
	config := DefaultStarlinkAPICollectorConfig()
	
	assert.NotNil(t, config)
	assert.Equal(t, "192.168.100.1", config.Host)
	assert.Equal(t, 9200, config.Port)
	assert.Equal(t, 10*time.Second, config.Timeout)
	assert.True(t, config.EnableAllAPIs)
	assert.True(t, config.EnableLocationAPI)
	assert.True(t, config.EnableStatusAPI)
	assert.True(t, config.EnableDiagnosticsAPI)
	assert.Equal(t, 3, config.RetryAttempts)
	assert.Equal(t, 0.3, config.ConfidenceThreshold)
	assert.Equal(t, 0.5, config.QualityScoreThreshold)
}

func TestNewStarlinkAPICollector(t *testing.T) {
	// Test with nil config
	collector := NewStarlinkAPICollector(nil, nil)
	assert.NotNil(t, collector)
	assert.Equal(t, "192.168.100.1", collector.starlinkHost)
	assert.Equal(t, 9200, collector.starlinkPort)
	assert.Equal(t, 10*time.Second, collector.timeout)
	assert.NotNil(t, collector.starlinkClient)
	assert.Nil(t, collector.logger)
	
	// Test with custom config
	customConfig := &StarlinkAPICollectorConfig{
		Host:                    "192.168.1.100",
		Port:                    8080,
		Timeout:                 5 * time.Second,
		EnableAllAPIs:           false,
		EnableLocationAPI:       true,
		EnableStatusAPI:         false,
		EnableDiagnosticsAPI:    false,
		RetryAttempts:           5,
		ConfidenceThreshold:     0.5,
		QualityScoreThreshold:   0.7,
	}
	
	collector = NewStarlinkAPICollector(customConfig, nil)
	assert.NotNil(t, collector)
	assert.Equal(t, "192.168.1.100", collector.starlinkHost)
	assert.Equal(t, 8080, collector.starlinkPort)
	assert.Equal(t, 5*time.Second, collector.timeout)
	assert.NotNil(t, collector.starlinkClient)
}

func TestComprehensiveStarlinkGPS_Structure(t *testing.T) {
	gps := &ComprehensiveStarlinkGPS{
		Latitude:  37.7749,
		Longitude: -122.4194,
		Altitude:  100.0,
		Accuracy:  5.0,
		
		HorizontalSpeedMps: 10.0,
		VerticalSpeedMps:   2.0,
		GPSSource:          "GNC_FUSED",
		
		GPSValid:        boolPtr(true),
		GPSSatellites:   intPtr(8),
		NoSatsAfterTTFF: boolPtr(false),
		InhibitGPS:      boolPtr(false),
		
		LocationEnabled:        boolPtr(true),
		UncertaintyMeters:      float64Ptr(3.5),
		UncertaintyMetersValid: boolPtr(true),
		GPSTimeS:               float64Ptr(1234567890.0),
		
		DataSources:  []string{"get_location", "get_status", "get_diagnostics"},
		CollectedAt:  time.Now(),
		CollectionMs: 150,
		Valid:        true,
		Confidence:   0.85,
		QualityScore: "excellent",
	}
	
	assert.Equal(t, 37.7749, gps.Latitude)
	assert.Equal(t, -122.4194, gps.Longitude)
	assert.Equal(t, 100.0, gps.Altitude)
	assert.Equal(t, 5.0, gps.Accuracy)
	assert.Equal(t, 10.0, gps.HorizontalSpeedMps)
	assert.Equal(t, 2.0, gps.VerticalSpeedMps)
	assert.Equal(t, "GNC_FUSED", gps.GPSSource)
	
	assert.True(t, *gps.GPSValid)
	assert.Equal(t, 8, *gps.GPSSatellites)
	assert.False(t, *gps.NoSatsAfterTTFF)
	assert.False(t, *gps.InhibitGPS)
	
	assert.True(t, *gps.LocationEnabled)
	assert.Equal(t, 3.5, *gps.UncertaintyMeters)
	assert.True(t, *gps.UncertaintyMetersValid)
	assert.Equal(t, 1234567890.0, *gps.GPSTimeS)
	
	assert.Len(t, gps.DataSources, 3)
	assert.True(t, gps.Valid)
	assert.Equal(t, 0.85, gps.Confidence)
	assert.Equal(t, "excellent", gps.QualityScore)
}

func TestShouldCollectLocation(t *testing.T) {
	collector := NewStarlinkAPICollector(nil, nil)
	
	// Should always return true
	result := collector.shouldCollectLocation()
	assert.True(t, result)
}

func TestShouldCollectStatus(t *testing.T) {
	collector := NewStarlinkAPICollector(nil, nil)
	
	// Should always return true
	result := collector.shouldCollectStatus()
	assert.True(t, result)
}

func TestShouldCollectDiagnostics(t *testing.T) {
	collector := NewStarlinkAPICollector(nil, nil)
	
	// Should always return true
	result := collector.shouldCollectDiagnostics()
	assert.True(t, result)
}

func TestCalculateConfidence(t *testing.T) {
	collector := NewStarlinkAPICollector(nil, nil)
	
	tests := []struct {
		name     string
		gps      *ComprehensiveStarlinkGPS
		expected float64
	}{
		{
			name: "High confidence - all data available",
			gps: &ComprehensiveStarlinkGPS{
				Latitude:  37.7749,
				Longitude: -122.4194,
				GPSValid:  boolPtr(true),
				GPSSatellites: intPtr(8),
				Accuracy: 5.0,
				UncertaintyMeters: float64Ptr(3.5),
				UncertaintyMetersValid: boolPtr(true),
				DataSources: []string{"get_location", "get_status", "get_diagnostics"},
			},
			expected: 0.9, // 0.4 + 0.2 + 0.2 + 0.1 + 0.1 + 0.1
		},
		{
			name: "Medium confidence - partial data",
			gps: &ComprehensiveStarlinkGPS{
				Latitude:  37.7749,
				Longitude: -122.4194,
				GPSValid:  boolPtr(true),
				GPSSatellites: intPtr(6),
				Accuracy: 15.0,
				DataSources: []string{"get_location", "get_status"},
			},
			expected: 0.7, // 0.4 + 0.2 + 0.1 + 0.1
		},
		{
			name: "Low confidence - minimal data",
			gps: &ComprehensiveStarlinkGPS{
				Latitude:  37.7749,
				Longitude: -122.4194,
				GPSValid:  boolPtr(false),
				DataSources: []string{"get_location"},
			},
			expected: 0.4, // 0.4 only
		},
		{
			name: "No confidence - no location data",
			gps: &ComprehensiveStarlinkGPS{
				Latitude:  0.0,
				Longitude: 0.0,
				DataSources: []string{},
			},
			expected: 0.0,
		},
	}
	
	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			result := collector.calculateConfidence(tt.gps)
			assert.Equal(t, tt.expected, result)
		})
	}
}

func TestCalculateQualityScore(t *testing.T) {
	collector := NewStarlinkAPICollector(nil, nil)
	
	tests := []struct {
		name     string
		gps      *ComprehensiveStarlinkGPS
		expected string
	}{
		{
			name: "Excellent quality",
			gps: &ComprehensiveStarlinkGPS{
				GPSValid: boolPtr(true),
				GPSSatellites: intPtr(10),
				Accuracy: 5.0,
				UncertaintyMeters: float64Ptr(5.0),
				UncertaintyMetersValid: boolPtr(true),
			},
			expected: "excellent", // 0.3 + 0.3 + 0.2 + 0.2 = 1.0
		},
		{
			name: "Good quality",
			gps: &ComprehensiveStarlinkGPS{
				GPSValid: boolPtr(true),
				GPSSatellites: intPtr(7),
				Accuracy: 20.0,
				UncertaintyMeters: float64Ptr(25.0),
				UncertaintyMetersValid: boolPtr(true),
			},
			expected: "good", // 0.3 + 0.2 + 0.1 + 0.1 = 0.7
		},
		{
			name: "Fair quality",
			gps: &ComprehensiveStarlinkGPS{
				GPSValid: boolPtr(true),
				GPSSatellites: intPtr(5),
				Accuracy: 50.0,
				UncertaintyMeters: float64Ptr(75.0),
				UncertaintyMetersValid: boolPtr(true),
			},
			expected: "fair", // 0.3 + 0.1 + 0.0 + 0.0 = 0.4
		},
		{
			name: "Poor quality",
			gps: &ComprehensiveStarlinkGPS{
				GPSValid: boolPtr(false),
				GPSSatellites: intPtr(2),
				Accuracy: 100.0,
			},
			expected: "poor", // 0.0 + 0.0 + 0.0 + 0.0 = 0.0
		},
	}
	
	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			result := collector.calculateQualityScore(tt.gps)
			assert.Equal(t, tt.expected, result)
		})
	}
}

func TestMergeLocationData(t *testing.T) {
	collector := NewStarlinkAPICollector(nil, nil)
	
	gps := &ComprehensiveStarlinkGPS{}
	data := map[string]interface{}{
		"latitude":              37.7749,
		"longitude":             -122.4194,
		"altitude":              100.0,
		"accuracy":              5.0,
		"horizontal_speed_mps":  10.0,
		"vertical_speed_mps":    2.0,
		"gps_source":            "GNC_FUSED",
	}
	
	collector.mergeLocationData(gps, data)
	
	assert.Equal(t, 37.7749, gps.Latitude)
	assert.Equal(t, -122.4194, gps.Longitude)
	assert.Equal(t, 100.0, gps.Altitude)
	assert.Equal(t, 5.0, gps.Accuracy)
	assert.Equal(t, 10.0, gps.HorizontalSpeedMps)
	assert.Equal(t, 2.0, gps.VerticalSpeedMps)
	assert.Equal(t, "GNC_FUSED", gps.GPSSource)
}

func TestMergeStatusData(t *testing.T) {
	collector := NewStarlinkAPICollector(nil, nil)
	
	gps := &ComprehensiveStarlinkGPS{}
	data := map[string]interface{}{
		"gps_valid":          boolPtr(true),
		"gps_satellites":     intPtr(8),
		"no_sats_after_ttff": boolPtr(false),
		"inhibit_gps":        boolPtr(false),
	}
	
	collector.mergeStatusData(gps, data)
	
	assert.True(t, *gps.GPSValid)
	assert.Equal(t, 8, *gps.GPSSatellites)
	assert.False(t, *gps.NoSatsAfterTTFF)
	assert.False(t, *gps.InhibitGPS)
}

func TestMergeDiagnosticsData(t *testing.T) {
	collector := NewStarlinkAPICollector(nil, nil)
	
	gps := &ComprehensiveStarlinkGPS{}
	data := map[string]interface{}{
		"location_enabled":         boolPtr(true),
		"uncertainty_meters":       float64Ptr(3.5),
		"uncertainty_meters_valid": boolPtr(true),
		"gps_time_s":               float64Ptr(1234567890.0),
	}
	
	collector.mergeDiagnosticsData(gps, data)
	
	assert.True(t, *gps.LocationEnabled)
	assert.Equal(t, 3.5, *gps.UncertaintyMeters)
	assert.True(t, *gps.UncertaintyMetersValid)
	assert.Equal(t, 1234567890.0, *gps.GPSTimeS)
}

func TestGetGPSLocation(t *testing.T) {
	collector := NewStarlinkAPICollector(nil, nil)
	
	// This will fail in test environment without real Starlink client
	// but we can test the error handling
	ctx := context.Background()
	location, err := collector.GetGPSLocation(ctx)
	
	assert.Error(t, err)
	assert.Nil(t, location)
}

func TestGetGPSStatus(t *testing.T) {
	collector := NewStarlinkAPICollector(nil, nil)
	
	// This will fail in test environment without real Starlink client
	// but we can test the error handling
	ctx := context.Background()
	status, err := collector.GetGPSStatus(ctx)
	
	assert.Error(t, err)
	assert.Nil(t, status)
}

func TestIsAvailable(t *testing.T) {
	collector := NewStarlinkAPICollector(nil, nil)
	
	// This will fail in test environment without real Starlink client
	ctx := context.Background()
	available := collector.IsAvailable(ctx)
	
	assert.False(t, available)
}

func TestGetComprehensiveGPSMetrics(t *testing.T) {
	collector := NewStarlinkAPICollector(nil, nil)
	
	// This will fail in test environment without real Starlink client
	// but we can test the error handling
	ctx := context.Background()
	metrics, err := collector.GetComprehensiveGPSMetrics(ctx)
	
	assert.Error(t, err)
	assert.Nil(t, metrics)
}

func TestStarlinkAPICollector_Integration(t *testing.T) {
	collector := NewStarlinkAPICollector(nil, nil)
	
	// Test collector creation and configuration
	assert.NotNil(t, collector)
	assert.Equal(t, "192.168.100.1", collector.starlinkHost)
	assert.Equal(t, 9200, collector.starlinkPort)
	assert.Equal(t, 10*time.Second, collector.timeout)
	assert.NotNil(t, collector.starlinkClient)
	
	// Test that the collector has the expected methods
	// (This is a structural test to ensure the interface is available)
	assert.NotNil(t, collector.CollectComprehensiveGPS)
	assert.NotNil(t, collector.GetGPSLocation)
	assert.NotNil(t, collector.GetGPSStatus)
	assert.NotNil(t, collector.IsAvailable)
	assert.NotNil(t, collector.GetComprehensiveGPSMetrics)
}

func TestStarlinkAPICollector_Configuration(t *testing.T) {
	// Test various configuration combinations
	testCases := []struct {
		name   string
		config *StarlinkAPICollectorConfig
	}{
		{
			name:   "Default configuration",
			config: nil,
		},
		{
			name: "Minimal configuration",
			config: &StarlinkAPICollectorConfig{
				Host: "192.168.1.100",
				Port: 8080,
			},
		},
		{
			name: "Full configuration",
			config: &StarlinkAPICollectorConfig{
				Host:                    "192.168.1.100",
				Port:                    8080,
				Timeout:                 15 * time.Second,
				EnableAllAPIs:           true,
				EnableLocationAPI:       true,
				EnableStatusAPI:         true,
				EnableDiagnosticsAPI:    true,
				RetryAttempts:           5,
				ConfidenceThreshold:     0.5,
				QualityScoreThreshold:   0.7,
			},
		},
		{
			name: "Partial APIs enabled",
			config: &StarlinkAPICollectorConfig{
				Host:                    "192.168.1.100",
				Port:                    8080,
				EnableAllAPIs:           false,
				EnableLocationAPI:       true,
				EnableStatusAPI:         false,
				EnableDiagnosticsAPI:    true,
			},
		},
	}
	
	for _, tc := range testCases {
		t.Run(tc.name, func(t *testing.T) {
			collector := NewStarlinkAPICollector(tc.config, nil)
			assert.NotNil(t, collector)
			assert.NotNil(t, collector.starlinkClient)
			
			if tc.config != nil {
				assert.Equal(t, tc.config.Host, collector.starlinkHost)
				assert.Equal(t, tc.config.Port, collector.starlinkPort)
				assert.Equal(t, tc.config.Timeout, collector.timeout)
			}
		})
	}
}

// Helper functions for creating pointers
func boolPtr(b bool) *bool {
	return &b
}

func intPtr(i int) *int {
	return &i
}

func float64Ptr(f float64) *float64 {
	return &f
}
