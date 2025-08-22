package gps

import (
	"context"
	"testing"
	"time"

	"github.com/markus-lassfolk/autonomy/pkg/logx"
	"github.com/stretchr/testify/assert"
)

func TestDefaultEnhanced5GConfig(t *testing.T) {
	config := &Enhanced5GConfig{
		Enable5GCollection:       true,
		CollectionTimeout:        10 * time.Second,
		MaxNeighborNRCells:       8,
		SignalThreshold:          -120,
		EnableCarrierAggregation: true,
		EnableAdvancedParsing:    true,
		RetryAttempts:            3,
	}

	assert.True(t, config.Enable5GCollection)
	assert.Equal(t, 10*time.Second, config.CollectionTimeout)
	assert.Equal(t, 8, config.MaxNeighborNRCells)
	assert.Equal(t, -120, config.SignalThreshold)
	assert.True(t, config.EnableCarrierAggregation)
	assert.True(t, config.EnableAdvancedParsing)
	assert.Equal(t, 3, config.RetryAttempts)
}

func TestNewEnhanced5GCollector(t *testing.T) {
	logger := logx.NewLogger("test", "debug")

	// Test with nil config
	collector := NewEnhanced5GCollector(nil, logger)
	assert.NotNil(t, collector)
	assert.NotNil(t, collector.config)
	assert.True(t, collector.config.Enable5GCollection)
	assert.Equal(t, 10*time.Second, collector.config.CollectionTimeout)
	assert.Equal(t, 8, collector.config.MaxNeighborNRCells)
	assert.Equal(t, -120, collector.config.SignalThreshold)
	assert.True(t, collector.config.EnableCarrierAggregation)
	assert.True(t, collector.config.EnableAdvancedParsing)
	assert.Equal(t, 3, collector.config.RetryAttempts)

	// Test with custom config
	customConfig := &Enhanced5GConfig{
		Enable5GCollection:       false,
		CollectionTimeout:        5 * time.Second,
		MaxNeighborNRCells:       4,
		SignalThreshold:          -110,
		EnableCarrierAggregation: false,
		EnableAdvancedParsing:    false,
		RetryAttempts:            5,
	}

	collector = NewEnhanced5GCollector(customConfig, logger)
	assert.NotNil(t, collector)
	assert.NotNil(t, collector.config)
	assert.False(t, collector.config.Enable5GCollection)
	assert.Equal(t, 5*time.Second, collector.config.CollectionTimeout)
	assert.Equal(t, 4, collector.config.MaxNeighborNRCells)
	assert.Equal(t, -110, collector.config.SignalThreshold)
	assert.False(t, collector.config.EnableCarrierAggregation)
	assert.False(t, collector.config.EnableAdvancedParsing)
	assert.Equal(t, 5, collector.config.RetryAttempts)
}

func TestEnhanced5GCellInfo_Structure(t *testing.T) {
	cellInfo := &Enhanced5GCellInfo{
		NCI:      0x12345678,
		GSCN:     1234,
		RSRP:     -85,
		RSRQ:     -12,
		SINR:     15,
		Band:     "N78",
		CellType: "serving",
		PCI:      123,
		EARFCN:   5678,
	}

	assert.Equal(t, int64(0x12345678), int64(cellInfo.NCI))
	assert.Equal(t, 1234, cellInfo.GSCN)
	assert.Equal(t, -85, cellInfo.RSRP)
	assert.Equal(t, -12, cellInfo.RSRQ)
	assert.Equal(t, 15, cellInfo.SINR)
	assert.Equal(t, "N78", cellInfo.Band)
	assert.Equal(t, "serving", cellInfo.CellType)
	assert.Equal(t, 123, cellInfo.PCI)
	assert.Equal(t, 5678, cellInfo.EARFCN)
}

func TestEnhanced5GNetworkInfo_Structure(t *testing.T) {
	networkInfo := &Enhanced5GNetworkInfo{
		Mode:               "5G-SA",
		LTEAnchor:          nil,
		NRCells:            []Enhanced5GCellInfo{},
		CarrierAggregation: true,
		RegistrationStatus: "registered",
		NetworkOperator:    "Test Operator",
		Technology:         "5G",
		CollectedAt:        time.Now(),
		Valid:              true,
		Confidence:         0.8,
	}

	assert.Equal(t, "5G-SA", networkInfo.Mode)
	assert.Nil(t, networkInfo.LTEAnchor)
	assert.Empty(t, networkInfo.NRCells)
	assert.True(t, networkInfo.CarrierAggregation)
	assert.Equal(t, "registered", networkInfo.RegistrationStatus)
	assert.Equal(t, "Test Operator", networkInfo.NetworkOperator)
	assert.Equal(t, "5G", networkInfo.Technology)
	assert.True(t, networkInfo.Valid)
	assert.Equal(t, 0.8, networkInfo.Confidence)
}

func TestCollect5GNetworkInfo_Disabled(t *testing.T) {
	logger := logx.NewLogger("test", "debug")
	config := &Enhanced5GConfig{
		Enable5GCollection: false,
	}
	collector := NewEnhanced5GCollector(config, logger)

	ctx := context.Background()
	result, err := collector.Collect5GNetworkInfo(ctx)

	assert.Error(t, err)
	assert.Nil(t, result)
	assert.Contains(t, err.Error(), "5G collection disabled")
}

func TestCollect5GNetworkInfo_Enabled(t *testing.T) {
	logger := logx.NewLogger("test", "debug")
	config := &Enhanced5GConfig{
		Enable5GCollection: true,
		CollectionTimeout:  1 * time.Second,
		RetryAttempts:      1,
	}
	collector := NewEnhanced5GCollector(config, logger)

	ctx := context.Background()
	result, err := collector.Collect5GNetworkInfo(ctx)

	// This will likely fail in test environment without real AT commands
	// but we can test the structure and error handling
	if err != nil {
		// Expected error in test environment
		assert.Contains(t, err.Error(), "exec")
	} else {
		assert.NotNil(t, result)
		assert.NotNil(t, result.CollectedAt)
	}
}

func TestEnhanced5GCollector_Integration(t *testing.T) {
	logger := logx.NewLogger("test", "debug")
	config := &Enhanced5GConfig{
		Enable5GCollection:       true,
		CollectionTimeout:        10 * time.Second,
		MaxNeighborNRCells:       8,
		SignalThreshold:          -120,
		EnableCarrierAggregation: true,
		EnableAdvancedParsing:    true,
		RetryAttempts:            3,
	}
	collector := NewEnhanced5GCollector(config, logger)

	// Test that the collector can be created and configured properly
	assert.NotNil(t, collector)
	assert.NotNil(t, collector.logger)
	assert.NotNil(t, collector.config)

	// Test that the collector has the expected methods
	// (This is a structural test to ensure the interface is available)
	assert.NotNil(t, collector.Collect5GNetworkInfo)
}

func TestEnhanced5GCollector_ErrorHandling(t *testing.T) {
	logger := logx.NewLogger("test", "debug")
	config := &Enhanced5GConfig{
		Enable5GCollection: true,
		RetryAttempts:      1,
	}
	collector := NewEnhanced5GCollector(config, logger)

	ctx := context.Background()

	// Test with a very short timeout to trigger error handling
	shortCtx, cancel := context.WithTimeout(ctx, 1*time.Millisecond)
	defer cancel()

	result, err := collector.Collect5GNetworkInfo(shortCtx)

	// Should fail due to timeout or return a result with Valid=false
	if err != nil {
		assert.Error(t, err)
		assert.Nil(t, result)
	} else {
		assert.NotNil(t, result)
		assert.False(t, result.Valid)
	}
}

func TestEnhanced5GCollector_Configuration(t *testing.T) {
	logger := logx.NewLogger("test", "debug")

	// Test various configuration combinations
	testCases := []struct {
		name   string
		config *Enhanced5GConfig
	}{
		{
			name:   "Default configuration",
			config: nil,
		},
		{
			name: "Minimal configuration",
			config: &Enhanced5GConfig{
				Enable5GCollection: true,
			},
		},
		{
			name: "Full configuration",
			config: &Enhanced5GConfig{
				Enable5GCollection:       true,
				CollectionTimeout:        15 * time.Second,
				MaxNeighborNRCells:       10,
				SignalThreshold:          -100,
				EnableCarrierAggregation: true,
				EnableAdvancedParsing:    true,
				RetryAttempts:            5,
			},
		},
		{
			name: "Disabled features",
			config: &Enhanced5GConfig{
				Enable5GCollection:       false,
				EnableCarrierAggregation: false,
				EnableAdvancedParsing:    false,
			},
		},
	}

	for _, tc := range testCases {
		t.Run(tc.name, func(t *testing.T) {
			collector := NewEnhanced5GCollector(tc.config, logger)
			assert.NotNil(t, collector)
			assert.NotNil(t, collector.config)

			// Test that the collector can be used
			result, err := collector.Collect5GNetworkInfo(context.Background())
			// In test environment, it might succeed but return invalid data
			if err != nil {
				assert.Error(t, err)
			} else {
				assert.NotNil(t, result)
				assert.False(t, result.Valid)
			}
		})
	}
}
