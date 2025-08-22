package gps

import (
	"context"
	"fmt"
	"testing"
	"time"

	"github.com/markus-lassfolk/autonomy/pkg/logx"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
)

func TestDefaultEnhanced5GConfig(t *testing.T) {
	config := DefaultEnhanced5GConfig()
	
	assert.NotNil(t, config)
	assert.Equal(t, "192.168.100.1", config.Host)
	assert.Equal(t, 9200, config.Port)
	assert.Equal(t, 10*time.Second, config.Timeout)
	assert.True(t, config.EnableAdvancedParsing)
	assert.True(t, config.EnableCarrierAggregation)
	assert.Equal(t, 3, config.RetryAttempts)
	assert.Equal(t, 0.3, config.ConfidenceThreshold)
	assert.Equal(t, 0.5, config.QualityScoreThreshold)
}

func TestNewEnhanced5GCollector(t *testing.T) {
	logger := logx.NewLogger("test", "debug")
	
	// Test with nil config
	collector := NewEnhanced5GCollector(nil, logger)
	assert.NotNil(t, collector)
	assert.Equal(t, "192.168.100.1", collector.host)
	assert.Equal(t, 9200, collector.port)
	assert.Equal(t, 10*time.Second, collector.timeout)
	assert.True(t, collector.enableAdvancedParsing)
	assert.True(t, collector.enableCarrierAggregation)
	assert.Equal(t, 3, collector.retryAttempts)
	
	// Test with custom config
	customConfig := &Enhanced5GConfig{
		Host:                    "192.168.1.100",
		Port:                    8080,
		Timeout:                 5 * time.Second,
		EnableAdvancedParsing:   false,
		EnableCarrierAggregation: false,
		RetryAttempts:           5,
		ConfidenceThreshold:     0.5,
		QualityScoreThreshold:   0.7,
	}
	
	collector = NewEnhanced5GCollector(customConfig, logger)
	assert.NotNil(t, collector)
	assert.Equal(t, "192.168.1.100", collector.host)
	assert.Equal(t, 8080, collector.port)
	assert.Equal(t, 5*time.Second, collector.timeout)
	assert.False(t, collector.enableAdvancedParsing)
	assert.False(t, collector.enableCarrierAggregation)
	assert.Equal(t, 5, collector.retryAttempts)
}

func TestParseQNWINFO(t *testing.T) {
	logger := logx.NewLogger("test", "debug")
	collector := NewEnhanced5GCollector(nil, logger)
	
	tests := []struct {
		name     string
		line     string
		expected *Enhanced5GCellInfo
	}{
		{
			name: "Valid QNWINFO line",
			line: "QNWINFO: \"5G\",\"310260\",\"0x12345678\",\"1234\",\"5678\",\"-85\",\"-12\",\"15\"",
			expected: &Enhanced5GCellInfo{
				Technology:     "5G",
				MCC:           "310",
				MNC:           "260",
				NCI:           0x12345678,
				PCI:           1234,
				EARFCN:        5678,
				RSRP:          -85,
				RSRQ:          -12,
				SINR:          15,
				Type:          "serving",
			},
		},
		{
			name: "Invalid QNWINFO line - missing fields",
			line: "QNWINFO: \"5G\",\"310260\"",
			expected: nil,
		},
		{
			name: "Invalid QNWINFO line - wrong format",
			line: "INVALID: data",
			expected: nil,
		},
		{
			name: "Empty line",
			line: "",
			expected: nil,
		},
	}
	
	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			result := collector.parseQNWINFO(tt.line)
			if tt.expected == nil {
				assert.Nil(t, result)
			} else {
				assert.NotNil(t, result)
				assert.Equal(t, tt.expected.Technology, result.Technology)
				assert.Equal(t, tt.expected.MCC, result.MCC)
				assert.Equal(t, tt.expected.MNC, result.MNC)
				assert.Equal(t, tt.expected.NCI, result.NCI)
				assert.Equal(t, tt.expected.PCI, result.PCI)
				assert.Equal(t, tt.expected.EARFCN, result.EARFCN)
				assert.Equal(t, tt.expected.RSRP, result.RSRP)
				assert.Equal(t, tt.expected.RSRQ, result.RSRQ)
				assert.Equal(t, tt.expected.SINR, result.SINR)
				assert.Equal(t, tt.expected.Type, result.Type)
			}
		})
	}
}

func TestParseQCSQ(t *testing.T) {
	logger := logx.NewLogger("test", "debug")
	collector := NewEnhanced5GCollector(nil, logger)
	
	tests := []struct {
		name     string
		line     string
		expected *Enhanced5GCellInfo
	}{
		{
			name: "Valid QCSQ line",
			line: "QCSQ: \"5G\",\"-85\",\"-12\",\"15\"",
			expected: &Enhanced5GCellInfo{
				Technology: "5G",
				RSRP:      -85,
				RSRQ:      -12,
				SINR:      15,
				Type:      "signal",
			},
		},
		{
			name: "Invalid QCSQ line - missing fields",
			line: "QCSQ: \"5G\"",
			expected: nil,
		},
		{
			name: "Invalid QCSQ line - wrong format",
			line: "INVALID: data",
			expected: nil,
		},
	}
	
	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			result := collector.parseQCSQ(tt.line)
			if tt.expected == nil {
				assert.Nil(t, result)
			} else {
				assert.NotNil(t, result)
				assert.Equal(t, tt.expected.Technology, result.Technology)
				assert.Equal(t, tt.expected.RSRP, result.RSRP)
				assert.Equal(t, tt.expected.RSRQ, result.RSRQ)
				assert.Equal(t, tt.expected.SINR, result.SINR)
				assert.Equal(t, tt.expected.Type, result.Type)
			}
		})
	}
}

func TestParseQENG(t *testing.T) {
	logger := logx.NewLogger("test", "debug")
	collector := NewEnhanced5GCollector(nil, logger)
	
	tests := []struct {
		name     string
		line     string
		expected *Enhanced5GCellInfo
	}{
		{
			name: "Valid QENG line",
			line: "QENG: \"servingcell\",\"5G\",\"310260\",\"0x12345678\",\"1234\",\"5678\",\"-85\",\"-12\",\"15\"",
			expected: &Enhanced5GCellInfo{
				Technology: "5G",
				MCC:       "310",
				MNC:       "260",
				NCI:       0x12345678,
				PCI:       1234,
				EARFCN:    5678,
				RSRP:      -85,
				RSRQ:      -12,
				SINR:      15,
				Type:      "serving",
			},
		},
		{
			name: "Valid QENG neighbor line",
			line: "QENG: \"neighbourcell\",\"5G\",\"310260\",\"0x87654321\",\"4321\",\"8765\",\"-90\",\"-15\",\"10\"",
			expected: &Enhanced5GCellInfo{
				Technology: "5G",
				MCC:       "310",
				MNC:       "260",
				NCI:       0x87654321,
				PCI:       4321,
				EARFCN:    8765,
				RSRP:      -90,
				RSRQ:      -15,
				SINR:      10,
				Type:      "neighbor",
			},
		},
		{
			name: "Invalid QENG line - missing fields",
			line: "QENG: \"servingcell\",\"5G\"",
			expected: nil,
		},
		{
			name: "Invalid QENG line - wrong format",
			line: "INVALID: data",
			expected: nil,
		},
	}
	
	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			result := collector.parseQENG(tt.line)
			if tt.expected == nil {
				assert.Nil(t, result)
			} else {
				assert.NotNil(t, result)
				assert.Equal(t, tt.expected.Technology, result.Technology)
				assert.Equal(t, tt.expected.MCC, result.MCC)
				assert.Equal(t, tt.expected.MNC, result.MNC)
				assert.Equal(t, tt.expected.NCI, result.NCI)
				assert.Equal(t, tt.expected.PCI, result.PCI)
				assert.Equal(t, tt.expected.EARFCN, result.EARFCN)
				assert.Equal(t, tt.expected.RSRP, result.RSRP)
				assert.Equal(t, tt.expected.RSRQ, result.RSRQ)
				assert.Equal(t, tt.expected.SINR, result.SINR)
				assert.Equal(t, tt.expected.Type, result.Type)
			}
		})
	}
}

func TestParseNetworkOperator(t *testing.T) {
	logger := logx.NewLogger("test", "debug")
	collector := NewEnhanced5GCollector(nil, logger)
	
	tests := []struct {
		name     string
		line     string
		expected string
	}{
		{
			name:     "Valid operator line",
			line:     "COPS: 0,0,\"AT&T\",7",
			expected: "AT&T",
		},
		{
			name:     "Valid operator line with quotes",
			line:     "COPS: 0,0,\"Verizon Wireless\",7",
			expected: "Verizon Wireless",
		},
		{
			name:     "Invalid operator line",
			line:     "COPS: 0,0,7",
			expected: "",
		},
		{
			name:     "Empty line",
			line:     "",
			expected: "",
		},
	}
	
	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			result := collector.parseNetworkOperator(tt.line)
			assert.Equal(t, tt.expected, result)
		})
	}
}

func TestCalculateConfidence(t *testing.T) {
	logger := logx.NewLogger("test", "debug")
	collector := NewEnhanced5GCollector(nil, logger)
	
	tests := []struct {
		name     string
		info     *Enhanced5GNetworkInfo
		expected float64
	}{
		{
			name: "High confidence - all data available",
			info: &Enhanced5GNetworkInfo{
				Technology: "5G",
				ServingCell: &Enhanced5GCellInfo{
					RSRP: -85,
					RSRQ: -12,
					SINR: 15,
				},
				NeighborCells: []*Enhanced5GCellInfo{
					{RSRP: -90, RSRQ: -15, SINR: 10},
					{RSRP: -88, RSRQ: -13, SINR: 12},
				},
				NetworkOperator: "AT&T",
				Confidence:      0.8,
			},
			expected: 0.8,
		},
		{
			name: "Medium confidence - partial data",
			info: &Enhanced5GNetworkInfo{
				Technology: "5G",
				ServingCell: &Enhanced5GCellInfo{
					RSRP: -95,
					RSRQ: -18,
					SINR: 8,
				},
				NetworkOperator: "Verizon",
				Confidence:      0.5,
			},
			expected: 0.5,
		},
		{
			name: "Low confidence - minimal data",
			info: &Enhanced5GNetworkInfo{
				Technology: "5G",
				ServingCell: &Enhanced5GCellInfo{
					RSRP: -110,
					RSRQ: -25,
					SINR: 5,
				},
				Confidence: 0.2,
			},
			expected: 0.2,
		},
	}
	
	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			result := collector.calculateConfidence(tt.info)
			assert.Equal(t, tt.expected, result)
		})
	}
}

func TestDetectCarrierAggregation(t *testing.T) {
	logger := logx.NewLogger("test", "debug")
	collector := NewEnhanced5GCollector(nil, logger)
	
	// Mock AT command responses
	mockResponses := map[string]string{
		"AT+QCAINFO": "QCAINFO: \"CA\",\"2\",\"0x12345678\",\"0x87654321\"",
		"AT+QNWINFO": "QNWINFO: \"5G\",\"310260\",\"0x12345678\",\"1234\",\"5678\",\"-85\",\"-12\",\"15\"",
	}
	
	// Test with carrier aggregation detected
	collector.mockATResponses = mockResponses
	result := collector.detectCarrierAggregation(context.Background())
	assert.True(t, result)
	
	// Test without carrier aggregation
	collector.mockATResponses = map[string]string{
		"AT+QCAINFO": "QCAINFO: \"NO_CA\"",
	}
	result = collector.detectCarrierAggregation(context.Background())
	assert.False(t, result)
}

func TestCollect5GNetworkInfo(t *testing.T) {
	logger := logx.NewLogger("test", "debug")
	collector := NewEnhanced5GCollector(nil, logger)
	
	// Mock AT command responses for comprehensive test
	collector.mockATResponses = map[string]string{
		"AT+QNWINFO": "QNWINFO: \"5G\",\"310260\",\"0x12345678\",\"1234\",\"5678\",\"-85\",\"-12\",\"15\"",
		"AT+QCSQ":    "QCSQ: \"5G\",\"-85\",\"-12\",\"15\"",
		"AT+QENG":    "QENG: \"servingcell\",\"5G\",\"310260\",\"0x12345678\",\"1234\",\"5678\",\"-85\",\"-12\",\"15\"",
		"AT+COPS":    "COPS: 0,0,\"AT&T\",7",
		"AT+QCAINFO": "QCAINFO: \"CA\",\"2\",\"0x12345678\",\"0x87654321\"",
	}
	
	ctx := context.Background()
	result, err := collector.Collect5GNetworkInfo(ctx)
	
	require.NoError(t, err)
	assert.NotNil(t, result)
	assert.Equal(t, "5G", result.Technology)
	assert.Equal(t, "AT&T", result.NetworkOperator)
	assert.True(t, result.CarrierAggregation)
	assert.Greater(t, result.Confidence, 0.0)
	assert.LessOrEqual(t, result.Confidence, 1.0)
	
	// Verify serving cell data
	assert.NotNil(t, result.ServingCell)
	assert.Equal(t, "5G", result.ServingCell.Technology)
	assert.Equal(t, "310", result.ServingCell.MCC)
	assert.Equal(t, "260", result.ServingCell.MNC)
	assert.Equal(t, int64(0x12345678), result.ServingCell.NCI)
	assert.Equal(t, 1234, result.ServingCell.PCI)
	assert.Equal(t, 5678, result.ServingCell.EARFCN)
	assert.Equal(t, -85, result.ServingCell.RSRP)
	assert.Equal(t, -12, result.ServingCell.RSRQ)
	assert.Equal(t, 15, result.ServingCell.SINR)
	assert.Equal(t, "serving", result.ServingCell.Type)
}

func TestGet5GNetworkSummary(t *testing.T) {
	logger := logx.NewLogger("test", "debug")
	collector := NewEnhanced5GCollector(nil, logger)
	
	// Mock AT command responses
	collector.mockATResponses = map[string]string{
		"AT+QNWINFO": "QNWINFO: \"5G\",\"310260\",\"0x12345678\",\"1234\",\"5678\",\"-85\",\"-12\",\"15\"",
		"AT+QCSQ":    "QCSQ: \"5G\",\"-85\",\"-12\",\"15\"",
		"AT+QENG":    "QENG: \"servingcell\",\"5G\",\"310260\",\"0x12345678\",\"1234\",\"5678\",\"-85\",\"-12\",\"15\"",
		"AT+COPS":    "COPS: 0,0,\"AT&T\",7",
		"AT+QCAINFO": "QCAINFO: \"CA\",\"2\",\"0x12345678\",\"0x87654321\"",
	}
	
	ctx := context.Background()
	result, err := collector.Get5GNetworkSummary(ctx)
	
	require.NoError(t, err)
	assert.NotNil(t, result)
	
	// Verify summary fields
	assert.Equal(t, "5G", result["technology"])
	assert.Equal(t, "AT&T", result["network_operator"])
	assert.Equal(t, true, result["carrier_aggregation"])
	assert.Greater(t, result["confidence"], 0.0)
	assert.LessOrEqual(t, result["confidence"], 1.0)
	assert.Equal(t, "good", result["quality_score"])
	
	// Verify serving cell data
	servingCell, ok := result["serving_cell"].(map[string]interface{})
	assert.True(t, ok)
	assert.Equal(t, "5G", servingCell["technology"])
	assert.Equal(t, "310", servingCell["mcc"])
	assert.Equal(t, "260", servingCell["mnc"])
	assert.Equal(t, float64(0x12345678), servingCell["nci"])
	assert.Equal(t, float64(1234), servingCell["pci"])
	assert.Equal(t, float64(5678), servingCell["earfcn"])
	assert.Equal(t, float64(-85), servingCell["rsrp"])
	assert.Equal(t, float64(-12), servingCell["rsrq"])
	assert.Equal(t, float64(15), servingCell["sinr"])
}

func TestExecuteATCommand(t *testing.T) {
	logger := logx.NewLogger("test", "debug")
	collector := NewEnhanced5GCollector(nil, logger)
	
	// Test successful command execution
	collector.mockATResponses = map[string]string{
		"AT+QNWINFO": "QNWINFO: \"5G\",\"310260\",\"0x12345678\",\"1234\",\"5678\",\"-85\",\"-12\",\"15\"",
	}
	
	ctx := context.Background()
	result, err := collector.executeATCommand(ctx, "AT+QNWINFO")
	
	require.NoError(t, err)
	assert.Equal(t, "QNWINFO: \"5G\",\"310260\",\"0x12345678\",\"1234\",\"5678\",\"-85\",\"-12\",\"15\"", result)
	
	// Test command with retry logic
	collector.mockATResponses = map[string]string{
		"AT+QCSQ": "ERROR",
	}
	
	result, err = collector.executeATCommand(ctx, "AT+QCSQ")
	assert.Error(t, err)
	assert.Empty(t, result)
}

func TestEnhanced5GCollector_Integration(t *testing.T) {
	logger := logx.NewLogger("test", "debug")
	collector := NewEnhanced5GCollector(nil, logger)
	
	// Comprehensive integration test
	collector.mockATResponses = map[string]string{
		"AT+QNWINFO": "QNWINFO: \"5G\",\"310260\",\"0x12345678\",\"1234\",\"5678\",\"-85\",\"-12\",\"15\"",
		"AT+QCSQ":    "QCSQ: \"5G\",\"-85\",\"-12\",\"15\"",
		"AT+QENG":    "QENG: \"servingcell\",\"5G\",\"310260\",\"0x12345678\",\"1234\",\"5678\",\"-85\",\"-12\",\"15\"\nQENG: \"neighbourcell\",\"5G\",\"310260\",\"0x87654321\",\"4321\",\"8765\",\"-90\",\"-15\",\"10\"",
		"AT+COPS":    "COPS: 0,0,\"AT&T\",7",
		"AT+QCAINFO": "QCAINFO: \"CA\",\"2\",\"0x12345678\",\"0x87654321\"",
	}
	
	ctx := context.Background()
	
	// Test network info collection
	networkInfo, err := collector.Collect5GNetworkInfo(ctx)
	require.NoError(t, err)
	assert.NotNil(t, networkInfo)
	
	// Test summary generation
	summary, err := collector.Get5GNetworkSummary(ctx)
	require.NoError(t, err)
	assert.NotNil(t, summary)
	
	// Verify data consistency
	assert.Equal(t, networkInfo.Technology, summary["technology"])
	assert.Equal(t, networkInfo.NetworkOperator, summary["network_operator"])
	assert.Equal(t, networkInfo.CarrierAggregation, summary["carrier_aggregation"])
	assert.Equal(t, networkInfo.Confidence, summary["confidence"])
	
	// Verify serving cell data
	assert.NotNil(t, networkInfo.ServingCell)
	servingCell := summary["serving_cell"].(map[string]interface{})
	assert.Equal(t, networkInfo.ServingCell.Technology, servingCell["technology"])
	assert.Equal(t, networkInfo.ServingCell.MCC, servingCell["mcc"])
	assert.Equal(t, networkInfo.ServingCell.MNC, servingCell["mnc"])
	
	// Verify neighbor cells
	assert.Len(t, networkInfo.NeighborCells, 1)
	assert.Equal(t, "neighbor", networkInfo.NeighborCells[0].Type)
}

// Mock implementation for testing
func (e5g *Enhanced5GCollector) mockATCommand(command string) (string, error) {
	if response, exists := e5g.mockATResponses[command]; exists {
		if response == "ERROR" {
			return "", fmt.Errorf("AT command failed: %s", command)
		}
		return response, nil
	}
	return "", fmt.Errorf("AT command not mocked: %s", command)
}

// Add mock field to collector for testing
type Enhanced5GCollector struct {
	*Enhanced5GCollector
	mockATResponses map[string]string
}
