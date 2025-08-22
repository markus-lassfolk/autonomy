package main

import (
	"fmt"
	"time"
)

// SmartCellLocationService combines intelligent caching with OpenCellID queries
type SmartCellLocationService struct {
	cache  *IntelligentCellCache
	apiKey string
}

// NewSmartCellLocationService creates a new smart cell location service
func NewSmartCellLocationService(apiKey string) *SmartCellLocationService {
	return &SmartCellLocationService{
		cache:  NewIntelligentCellCache(),
		apiKey: apiKey,
	}
}

// GetLocation intelligently determines whether to query OpenCellID or use cached data
func (service *SmartCellLocationService) GetLocation(intel *CellularLocationIntelligence, gpsRef *GPSCoordinate) (*CellTowerLocation, error) {
	// Convert intelligence to environment format
	currentEnv, err := ParseCellEnvironmentFromIntelligence(intel)
	if err != nil {
		return nil, fmt.Errorf("failed to parse cell environment: %w", err)
	}

	// Check if we should query or use cache
	shouldQuery, reason := service.cache.ShouldQueryLocation(currentEnv)

	fmt.Printf("📡 Cell location decision: %s\n", reason)

	if !shouldQuery {
		// Use cached location
		cached := service.cache.GetCachedLocation()
		if cached != nil {
			fmt.Printf("📍 Using cached location: %.6f, %.6f (±%.0fm)\n",
				cached.Latitude, cached.Longitude, cached.Accuracy)
			return cached, nil
		}
	}

	// Query OpenCellID for new location
	fmt.Println("🔍 Querying OpenCellID for new location...")

	// Use the practical cell location method (area search with weighted averaging)
	result, err := getPracticalCellLocation()
	if err != nil {
		return nil, fmt.Errorf("failed to get cell location: %w", err)
	}

	// Extract the location from the result
	location := &CellTowerLocation{
		Latitude:  result.EstimatedLat,
		Longitude: result.EstimatedLon,
		Accuracy:  result.EstimatedAccuracy,
		Source:    "opencellid_smart_cache",
	}

	// Update cache
	service.cache.UpdateCache(currentEnv, location)

	fmt.Printf("📍 New location obtained: %.6f, %.6f (±%.0fm)\n",
		location.Latitude, location.Longitude, location.Accuracy)

	return location, nil
}

// GetCacheStatus returns detailed cache status information
func (service *SmartCellLocationService) GetCacheStatus(intel *CellularLocationIntelligence) error {
	currentEnv, err := ParseCellEnvironmentFromIntelligence(intel)
	if err != nil {
		return fmt.Errorf("failed to parse cell environment: %w", err)
	}

	service.cache.PrintCacheStatus(currentEnv)
	return nil
}

// SetCacheConfiguration allows customizing cache behavior
func (service *SmartCellLocationService) SetCacheConfiguration(maxAge time.Duration, debounceDelay time.Duration, changeThreshold float64, topTowersCount int) {
	service.cache.MaxCacheAge = maxAge
	service.cache.DebounceDelay = debounceDelay
	service.cache.TowerChangeThreshold = changeThreshold
	service.cache.TopTowersCount = topTowersCount
}

// testSmartCellLocation demonstrates the smart cell location service
func testSmartCellLocation() error {
	fmt.Println("=== Testing Smart Cell Location Service ===")

	// Read API key
	apiKey, err := loadOpenCellIDTokenLocal()
	if err != nil {
		return fmt.Errorf("failed to read API key: %w", err)
	}

	// Create service with custom configuration
	service := NewSmartCellLocationService(apiKey)

	// Customize cache settings for testing
	service.SetCacheConfiguration(
		30*time.Minute, // Cache for 30 minutes instead of 1 hour
		5*time.Second,  // 5 second debounce instead of 10
		0.30,           // 30% change threshold instead of 35%
		3,              // Monitor top 3 towers instead of 5
	)

	// Create test cellular intelligence (using hardcoded data for demo)
	intel := &CellularLocationIntelligence{
		ServingCell: ServingCellInfo{
			CellID:   "25939743",
			MCC:      "240",
			MNC:      "01",
			TAC:      "23",
			EARFCN:   1300,
			PCID:     443,
			Band:     "LTE B3",
			Operator: "Telia",
		},
		NeighborCells: []NeighborCellInfo{
			{PCID: 263, RSRP: -90, RSRQ: -15, EARFCN: 1300, CellType: "intra"},
			{PCID: 60, RSRP: -102, RSRQ: -20, EARFCN: 1300, CellType: "intra"},
			{PCID: 100, RSRP: -95, RSRQ: -18, EARFCN: 9360, CellType: "inter"},
		},
		SignalQuality: SignalQuality{
			RSSI: -54,
			RSRP: -84,
			SINR: 13,
			RSRQ: -8,
		},
		NetworkInfo: NetworkInfo{
			Operator:   "Telia",
			Technology: "5G-NSA",
			Band:       "LTE B3",
			Registered: true,
		},
	}

	// Create GPS reference (hardcoded for demo)
	gpsRef := &GPSCoordinate{
		Latitude:  59.48007,
		Longitude: 18.27985,
		Accuracy:  2.0,
	}

	fmt.Println("\n--- First Query (should always query) ---")
	location1, err := service.GetLocation(intel, gpsRef)
	if err != nil {
		return fmt.Errorf("first query failed: %w", err)
	}

	fmt.Println("\n--- Second Query (same environment, should use cache) ---")
	location2, err := service.GetLocation(intel, gpsRef)
	if err != nil {
		return fmt.Errorf("second query failed: %w", err)
	}

	fmt.Printf("Same location returned: %v\n",
		location1.Latitude == location2.Latitude && location1.Longitude == location2.Longitude)

	fmt.Println("\n--- Cache Status ---")
	err = service.GetCacheStatus(intel)
	if err != nil {
		return fmt.Errorf("failed to get cache status: %w", err)
	}

	// Simulate serving cell change
	fmt.Println("\n--- Serving Cell Change (should trigger new query) ---")
	intel.ServingCell.CellID = "25939999" // Different cell ID
	intel.ServingCell.PCID = 500          // Different PCI

	location3, err := service.GetLocation(intel, gpsRef)
	if err != nil {
		return fmt.Errorf("serving cell change query failed: %w", err)
	}

	fmt.Println("\n--- Immediate Retry (should be debounced) ---")
	location4, err := service.GetLocation(intel, gpsRef)
	if err != nil {
		return fmt.Errorf("debounced query failed: %w", err)
	}

	fmt.Printf("Debounced query used cache: %v\n",
		location3.Latitude == location4.Latitude && location3.Longitude == location4.Longitude)

	// Simulate major neighbor change
	fmt.Println("\n--- Major Neighbor Change (should trigger new query after debounce) ---")

	// Wait for debounce to clear
	fmt.Println("Waiting for debounce to clear...")
	time.Sleep(6 * time.Second)

	// Change most neighbor cells (>30% threshold)
	intel.NeighborCells = []NeighborCellInfo{
		{PCID: 263, RSRP: -90, RSRQ: -15, EARFCN: 1300, CellType: "intra"}, // Same
		{PCID: 200, RSRP: -88, RSRQ: -12, EARFCN: 1300, CellType: "intra"}, // New
		{PCID: 300, RSRP: -92, RSRQ: -16, EARFCN: 9360, CellType: "inter"}, // New
		{PCID: 400, RSRP: -94, RSRQ: -18, EARFCN: 3150, CellType: "inter"}, // New
	}

	_, err = service.GetLocation(intel, gpsRef)
	if err != nil {
		return fmt.Errorf("neighbor change query failed: %w", err)
	}

	fmt.Println("\n--- Final Cache Status ---")
	err = service.GetCacheStatus(intel)
	if err != nil {
		return fmt.Errorf("failed to get final cache status: %w", err)
	}

	fmt.Println("\n=== Smart Cell Location Test Complete ===")
	return nil
}

// Integration with existing main function
func runSmartCellLocationTest() error {
	fmt.Println("🧠 Running Smart Cell Location Test...")
	return testSmartCellLocation()
}
