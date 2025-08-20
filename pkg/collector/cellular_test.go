package collector

import (
	"testing"

	"github.com/markus-lassfolk/autonomy/pkg"
)

// TestCellularCollector_ExtractNumber tests number extraction from various formats
func TestCellularCollector_ExtractNumber(t *testing.T) {
	collector, err := NewCellularCollector(map[string]interface{}{})
	if err != nil {
		t.Fatalf("Failed to create cellular collector: %v", err)
	}

	tests := []struct {
		name     string
		data     map[string]interface{}
		keys     []string
		expected int
		found    bool
	}{
		{
			name:     "float64 value",
			data:     map[string]interface{}{"rsrp": -95.5},
			keys:     []string{"rsrp"},
			expected: -95,
			found:    true,
		},
		{
			name:     "int value",
			data:     map[string]interface{}{"rsrq": -10},
			keys:     []string{"rsrq"},
			expected: -10,
			found:    true,
		},
		{
			name:     "string value",
			data:     map[string]interface{}{"sinr": "15"},
			keys:     []string{"sinr"},
			expected: 15,
			found:    true,
		},
		{
			name:     "multiple keys, second matches",
			data:     map[string]interface{}{"signal_rsrp": -88},
			keys:     []string{"rsrp", "signal_rsrp"},
			expected: -88,
			found:    true,
		},
		{
			name:     "no match",
			data:     map[string]interface{}{"other": "value"},
			keys:     []string{"rsrp"},
			expected: 0,
			found:    false,
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			result, found := collector.extractNumber(tt.data, tt.keys)
			if found != tt.found {
				t.Errorf("Expected found=%v, got %v", tt.found, found)
			}
			if found && result != tt.expected {
				t.Errorf("Expected result=%d, got %d", tt.expected, result)
			}
			t.Logf("✅ %s: found=%v, result=%d", tt.name, found, result)
		})
	}
}

// TestCellularCollector_ExtractString tests string extraction
func TestCellularCollector_ExtractString(t *testing.T) {
	collector, err := NewCellularCollector(map[string]interface{}{})
	if err != nil {
		t.Fatalf("Failed to create cellular collector: %v", err)
	}

	tests := []struct {
		name     string
		data     map[string]interface{}
		keys     []string
		expected string
		found    bool
	}{
		{
			name:     "string value",
			data:     map[string]interface{}{"operator": "Test Operator"},
			keys:     []string{"operator"},
			expected: "Test Operator",
			found:    true,
		},
		{
			name:     "empty string ignored",
			data:     map[string]interface{}{"operator": ""},
			keys:     []string{"operator"},
			expected: "",
			found:    false,
		},
		{
			name:     "multiple keys",
			data:     map[string]interface{}{"operator_name": "Carrier"},
			keys:     []string{"operator", "operator_name"},
			expected: "Carrier",
			found:    true,
		},
		{
			name:     "no match",
			data:     map[string]interface{}{"other": "value"},
			keys:     []string{"operator"},
			expected: "",
			found:    false,
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			result, found := collector.extractString(tt.data, tt.keys)
			if found != tt.found {
				t.Errorf("Expected found=%v, got %v", tt.found, found)
			}
			if found && result != tt.expected {
				t.Errorf("Expected result=%s, got %s", tt.expected, result)
			}
			t.Logf("✅ %s: found=%v, result=%s", tt.name, found, result)
		})
	}
}

// TestCellularCollector_ExtractBool tests boolean extraction
func TestCellularCollector_ExtractBool(t *testing.T) {
	collector, err := NewCellularCollector(map[string]interface{}{})
	if err != nil {
		t.Fatalf("Failed to create cellular collector: %v", err)
	}

	tests := []struct {
		name     string
		data     map[string]interface{}
		keys     []string
		expected bool
		found    bool
	}{
		{
			name:     "boolean true",
			data:     map[string]interface{}{"roaming": true},
			keys:     []string{"roaming"},
			expected: true,
			found:    true,
		},
		{
			name:     "boolean false",
			data:     map[string]interface{}{"roaming": false},
			keys:     []string{"roaming"},
			expected: false,
			found:    true,
		},
		{
			name:     "string true",
			data:     map[string]interface{}{"roaming": "true"},
			keys:     []string{"roaming"},
			expected: true,
			found:    true,
		},
		{
			name:     "string 1",
			data:     map[string]interface{}{"roaming": "1"},
			keys:     []string{"roaming"},
			expected: true,
			found:    true,
		},
		{
			name:     "string false",
			data:     map[string]interface{}{"roaming": "false"},
			keys:     []string{"roaming"},
			expected: false,
			found:    true,
		},
		{
			name:     "string 0",
			data:     map[string]interface{}{"roaming": "0"},
			keys:     []string{"roaming"},
			expected: false,
			found:    true,
		},
		{
			name:     "no match",
			data:     map[string]interface{}{"other": "value"},
			keys:     []string{"roaming"},
			expected: false,
			found:    false,
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			result, found := collector.extractBool(tt.data, tt.keys)
			if found != tt.found {
				t.Errorf("Expected found=%v, got %v", tt.found, found)
			}
			if found && result != tt.expected {
				t.Errorf("Expected result=%v, got %v", tt.expected, result)
			}
			t.Logf("✅ %s: found=%v, result=%v", tt.name, found, result)
		})
	}
}

// TestCellularCollector_ParseMobiledData tests RutOS mobiled data parsing
func TestCellularCollector_ParseMobiledData(t *testing.T) {
	collector, err := NewCellularCollector(map[string]interface{}{})
	if err != nil {
		t.Fatalf("Failed to create cellular collector: %v", err)
	}

	// Mock comprehensive mobiled response
	mockData := map[string]interface{}{
		"rsrp":             -95,
		"rsrq":             -10,
		"sinr":             15,
		"rssi":             -70,
		"network_type":     "LTE",
		"operator":         "Test Operator",
		"band":             "B3",
		"cell_id":          "12345",
		"sim_slot":         1,
		"sim_count":        2,
		"sim_status":       "ready",
		"roaming":          false,
		"home_operator":    "Home Operator",
		"connection_state": "connected",
		"tac":              "ABCD",
		"earfcn":           1850,
		"pci":              256,
	}

	info := &CellularInfo{}
	err = collector.parseMobiledData(mockData, info)
	if err != nil {
		t.Fatalf("parseMobiledData failed: %v", err)
	}

	// Verify signal metrics
	if info.RSRP == nil || *info.RSRP != -95 {
		t.Errorf("Expected RSRP=-95, got %v", info.RSRP)
	}
	if info.RSRQ == nil || *info.RSRQ != -10 {
		t.Errorf("Expected RSRQ=-10, got %v", info.RSRQ)
	}
	if info.SINR == nil || *info.SINR != 15 {
		t.Errorf("Expected SINR=15, got %v", info.SINR)
	}
	if info.RSSI == nil || *info.RSSI != -70 {
		t.Errorf("Expected RSSI=-70, got %v", info.RSSI)
	}

	// Verify network information
	if info.NetworkType == nil || *info.NetworkType != "LTE" {
		t.Errorf("Expected NetworkType=LTE, got %v", info.NetworkType)
	}
	if info.Operator == nil || *info.Operator != "Test Operator" {
		t.Errorf("Expected Operator='Test Operator', got %v", info.Operator)
	}
	if info.Band == nil || *info.Band != "B3" {
		t.Errorf("Expected Band=B3, got %v", info.Band)
	}
	if info.CellID == nil || *info.CellID != "12345" {
		t.Errorf("Expected CellID=12345, got %v", info.CellID)
	}

	// Verify multi-SIM support
	if info.ActiveSim == nil || *info.ActiveSim != 1 {
		t.Errorf("Expected ActiveSim=1, got %v", info.ActiveSim)
	}
	if info.SimCount == nil || *info.SimCount != 2 {
		t.Errorf("Expected SimCount=2, got %v", info.SimCount)
	}
	if info.SimStatus == nil || *info.SimStatus != "ready" {
		t.Errorf("Expected SimStatus=ready, got %v", info.SimStatus)
	}

	// Verify roaming detection
	if info.Roaming == nil || *info.Roaming != false {
		t.Errorf("Expected Roaming=false, got %v", info.Roaming)
	}
	if info.HomeOperator == nil || *info.HomeOperator != "Home Operator" {
		t.Errorf("Expected HomeOperator='Home Operator', got %v", info.HomeOperator)
	}

	// Verify connection state
	if info.ConnectionState == nil || *info.ConnectionState != "connected" {
		t.Errorf("Expected ConnectionState=connected, got %v", info.ConnectionState)
	}

	// Verify advanced metrics
	if info.TAC == nil || *info.TAC != "ABCD" {
		t.Errorf("Expected TAC=ABCD, got %v", info.TAC)
	}
	if info.EARFCN == nil || *info.EARFCN != 1850 {
		t.Errorf("Expected EARFCN=1850, got %v", info.EARFCN)
	}
	if info.PCI == nil || *info.PCI != 256 {
		t.Errorf("Expected PCI=256, got %v", info.PCI)
	}

	t.Log("✅ Enhanced mobiled data parsing successful")
}

// TestCellularCollector_GetSignalQuality tests signal quality calculation
func TestCellularCollector_GetSignalQuality(t *testing.T) {
	collector, err := NewCellularCollector(map[string]interface{}{})
	if err != nil {
		t.Fatalf("Failed to create cellular collector: %v", err)
	}

	tests := []struct {
		name     string
		rsrp     *int
		rsrq     *int
		sinr     *int
		expected float64
		minScore float64
		maxScore float64
	}{
		{
			name:     "excellent signal",
			rsrp:     intPtr(-60),
			rsrq:     intPtr(-5),
			sinr:     intPtr(20),
			minScore: 70.0,
			maxScore: 90.0,
		},
		{
			name:     "good signal",
			rsrp:     intPtr(-80),
			rsrq:     intPtr(-10),
			sinr:     intPtr(10),
			minScore: 50.0,
			maxScore: 70.0,
		},
		{
			name:     "poor signal",
			rsrp:     intPtr(-120),
			rsrq:     intPtr(-18),
			sinr:     intPtr(-5),
			minScore: 0.0,
			maxScore: 40.0,
		},
		{
			name:     "only rsrp available",
			rsrp:     intPtr(-90),
			rsrq:     nil,
			sinr:     nil,
			minScore: 30.0,
			maxScore: 70.0,
		},
		{
			name:     "no metrics available",
			rsrp:     nil,
			rsrq:     nil,
			sinr:     nil,
			expected: 50.0,
			minScore: 50.0,
			maxScore: 50.0,
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			quality := collector.GetSignalQuality(tt.rsrp, tt.rsrq, tt.sinr)

			if tt.expected > 0 {
				if quality != tt.expected {
					t.Errorf("Expected quality=%.1f, got %.1f", tt.expected, quality)
				}
			} else {
				if quality < tt.minScore || quality > tt.maxScore {
					t.Errorf("Expected quality between %.1f and %.1f, got %.1f",
						tt.minScore, tt.maxScore, quality)
				}
			}

			t.Logf("✅ %s: quality=%.1f", tt.name, quality)
		})
	}
}

// TestCellularCollector_DetectRoamingType tests roaming type detection
func TestCellularCollector_DetectRoamingType(t *testing.T) {
	collector, err := NewCellularCollector(map[string]interface{}{})
	if err != nil {
		t.Fatalf("Failed to create cellular collector: %v", err)
	}

	tests := []struct {
		name            string
		currentOperator *string
		homeOperator    *string
		expected        string
	}{
		{
			name:            "international roaming",
			currentOperator: strPtr("Foreign Operator"),
			homeOperator:    strPtr("Home Operator"),
			expected:        "international",
		},
		{
			name:            "national roaming",
			currentOperator: strPtr("Same Operator"),
			homeOperator:    strPtr("Same Operator"),
			expected:        "national",
		},
		{
			name:            "missing current operator",
			currentOperator: nil,
			homeOperator:    strPtr("Home Operator"),
			expected:        "unknown",
		},
		{
			name:            "missing home operator",
			currentOperator: strPtr("Current Operator"),
			homeOperator:    nil,
			expected:        "unknown",
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			result := collector.detectRoamingType(tt.currentOperator, tt.homeOperator)
			if result != tt.expected {
				t.Errorf("Expected roaming type=%s, got %s", tt.expected, result)
			}
			t.Logf("✅ %s: roaming_type=%s", tt.name, result)
		})
	}
}

// TestCellularCollector_ParseQMIOutput tests QMI output parsing
func TestCellularCollector_ParseQMIOutput(t *testing.T) {
	collector, err := NewCellularCollector(map[string]interface{}{})
	if err != nil {
		t.Fatalf("Failed to create cellular collector: %v", err)
	}

	mockQMIOutput := `
[/dev/cdc-wdm0] Successfully got signal strength
	RSRP: '-95 dBm'
	RSRQ: '-10 dB'
	SINR: '15 dB'
	RSSI: '-70 dBm'
`

	info := &CellularInfo{}
	err = collector.parseQMIOutput(mockQMIOutput, info)
	if err != nil {
		t.Fatalf("parseQMIOutput failed: %v", err)
	}

	// Verify parsed values
	if info.RSRP == nil || *info.RSRP != -95 {
		t.Errorf("Expected RSRP=-95, got %v", info.RSRP)
	}
	if info.RSRQ == nil || *info.RSRQ != -10 {
		t.Errorf("Expected RSRQ=-10, got %v", info.RSRQ)
	}
	if info.SINR == nil || *info.SINR != 15 {
		t.Errorf("Expected SINR=15, got %v", info.SINR)
	}
	if info.ModemType == nil || *info.ModemType != "qmi" {
		t.Errorf("Expected ModemType=qmi, got %v", info.ModemType)
	}

	t.Log("✅ QMI output parsing successful")
}

// TestCellularCollector_ParseMBIMOutput tests MBIM output parsing
func TestCellularCollector_ParseMBIMOutput(t *testing.T) {
	collector, err := NewCellularCollector(map[string]interface{}{})
	if err != nil {
		t.Fatalf("Failed to create cellular collector: %v", err)
	}

	mockMBIMOutput := `
[/dev/cdc-wdm0] Successfully queried signal state
	RSRP: '-88 dBm'
	RSRQ: '-12 dB'
	SNR: '18 dB'
`

	info := &CellularInfo{}
	err = collector.parseMBIMOutput(mockMBIMOutput, info)
	if err != nil {
		t.Fatalf("parseMBIMOutput failed: %v", err)
	}

	// Verify parsed values
	if info.RSRP == nil || *info.RSRP != -88 {
		t.Errorf("Expected RSRP=-88, got %v", info.RSRP)
	}
	if info.RSRQ == nil || *info.RSRQ != -12 {
		t.Errorf("Expected RSRQ=-12, got %v", info.RSRQ)
	}
	if info.SINR == nil || *info.SINR != 18 {
		t.Errorf("Expected SINR=18, got %v", info.SINR)
	}
	if info.ModemType == nil || *info.ModemType != "mbim" {
		t.Errorf("Expected ModemType=mbim, got %v", info.ModemType)
	}

	t.Log("✅ MBIM output parsing successful")
}

// TestCellularCollector_Validate tests member validation
func TestCellularCollector_Validate(t *testing.T) {
	collector, err := NewCellularCollector(map[string]interface{}{})
	if err != nil {
		t.Fatalf("Failed to create cellular collector: %v", err)
	}

	tests := []struct {
		name    string
		member  *pkg.Member
		wantErr bool
	}{
		{
			name: "valid cellular member",
			member: &pkg.Member{
				Name:  "cellular_test",
				Iface: "wwan0",
				Class: pkg.ClassCellular,
			},
			wantErr: false,
		},
		{
			name: "wrong class",
			member: &pkg.Member{
				Name:  "test",
				Iface: "eth0",
				Class: pkg.ClassLAN,
			},
			wantErr: true,
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
				Iface: "wwan0",
				Class: pkg.ClassCellular,
			},
			wantErr: true,
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			err := collector.Validate(tt.member)
			if (err != nil) != tt.wantErr {
				t.Errorf("Validate() error = %v, wantErr %v", err, tt.wantErr)
			}
			if tt.wantErr {
				t.Logf("✅ %s: validation correctly failed: %v", tt.name, err)
			} else {
				t.Logf("✅ %s: validation passed", tt.name)
			}
		})
	}
}

// BenchmarkCellularCollector_ExtractNumber benchmarks number extraction
func BenchmarkCellularCollector_ExtractNumber(b *testing.B) {
	collector, err := NewCellularCollector(map[string]interface{}{})
	if err != nil {
		b.Fatalf("Failed to create cellular collector: %v", err)
	}

	data := map[string]interface{}{
		"rsrp": -95.5,
		"rsrq": -10,
		"sinr": "15",
	}
	keys := []string{"rsrp", "signal_rsrp", "rsrp_dbm"}

	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		_, _ = collector.extractNumber(data, keys)
	}
}

// Helper functions for tests
func intPtr(i int) *int {
	return &i
}

func strPtr(s string) *string {
	return &s
}
