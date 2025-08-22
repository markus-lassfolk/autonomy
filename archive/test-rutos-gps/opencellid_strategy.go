package main

import (
	"fmt"
	"strings"
	"time"
)

// OpenCellIDUsageStrategy defines smart usage patterns for OpenCellID API
type OpenCellIDUsageStrategy struct {
	// API Limits (from search results)
	DailyRequestLimit    int `json:"daily_request_limit"`    // 5,000 requests/day
	MaxConcurrentThreads int `json:"max_concurrent_threads"` // 2 threads max

	// Usage Tracking
	RequestsToday      int       `json:"requests_today"`
	LastResetDate      time.Time `json:"last_reset_date"`
	ContributionsToday int       `json:"contributions_today"`

	// Smart Scheduling
	LocationRequestInterval   time.Duration `json:"location_request_interval"`
	ContributionInterval      time.Duration `json:"contribution_interval"`
	EmergencyRequestsReserved int           `json:"emergency_requests_reserved"`
}

// createOptimalUsageStrategy creates an optimal usage strategy
func createOptimalUsageStrategy() *OpenCellIDUsageStrategy {
	return &OpenCellIDUsageStrategy{
		// API Limits
		DailyRequestLimit:    5000, // From OpenCellID documentation
		MaxConcurrentThreads: 2,    // From server usage policy

		// Smart Usage Allocation
		EmergencyRequestsReserved: 100, // Reserve 100 requests for emergencies

		// Realistic Intervals (cell towers don't move!)
		LocationRequestInterval: 5 * time.Minute, // Every 5 minutes (288 requests/day max)
		ContributionInterval:    24 * time.Hour,  // Once per day

		// Tracking
		LastResetDate: time.Now().Truncate(24 * time.Hour),
	}
}

// displayUsageStrategy shows the recommended usage strategy
func displayUsageStrategy() {
	fmt.Println("🎯 OPENCELLID OPTIMAL USAGE STRATEGY")
	fmt.Println("=" + strings.Repeat("=", 40))
	fmt.Println("📊 Based on API limits: 5,000 requests/day")
	fmt.Println()

	strategy := createOptimalUsageStrategy()

	fmt.Println("📈 DAILY REQUEST ALLOCATION:")
	fmt.Printf("  🔢 Total Available: %d requests/day\n", strategy.DailyRequestLimit)
	fmt.Printf("  🚨 Emergency Reserve: %d requests\n", strategy.EmergencyRequestsReserved)
	maxLocationRequests := int(24 * time.Hour / strategy.LocationRequestInterval) // 288 requests/day at 5min intervals
	fmt.Printf("  📍 Location Requests: %d requests max (every %v)\n",
		maxLocationRequests, strategy.LocationRequestInterval)
	fmt.Printf("  📤 Contributions: ~100 requests (periodic)\n")

	fmt.Println("\n🎯 WHEN TO REQUEST LOCATION:")
	fmt.Println("  ✅ GPS signal lost (indoor/blocked)")
	fmt.Println("  ✅ GPS accuracy poor (>10m)")
	fmt.Println("  ✅ GPS unavailable (hardware failure)")
	fmt.Println("  ✅ Emergency fallback needed")
	fmt.Println("  ❌ NOT when GPS is working well")

	fmt.Println("\n📤 WHEN TO CONTRIBUTE DATA:")
	fmt.Println("  ✅ Daily: High-quality GPS + cell data")
	fmt.Println("  ✅ Location change: New cell tower detected")
	fmt.Println("  ✅ Signal change: Significant RSSI improvement")
	fmt.Println("  ✅ Startup: Once per daemon restart")
	fmt.Println("  ❌ NOT continuously or on every GPS reading")

	fmt.Println("\n⏰ RECOMMENDED SCHEDULE:")
	fmt.Printf("  📍 Location Requests: Every %v when GPS fails (max %d/day)\n",
		strategy.LocationRequestInterval, maxLocationRequests)
	fmt.Printf("  📤 Data Contribution: %v\n", strategy.ContributionInterval)
	fmt.Printf("  🔄 Usage Reset: Daily at midnight UTC\n")
	fmt.Printf("  🚨 Emergency Reserve: %d requests always available\n", strategy.EmergencyRequestsReserved)

	fmt.Println("\n🎯 SMART IMPLEMENTATION:")
	fmt.Println("  1. 📊 Track daily usage (reset at midnight)")
	fmt.Println("  2. 🚨 Reserve requests for emergencies")
	fmt.Println("  3. 📍 Only request location when GPS fails")
	fmt.Println("  4. 📤 Contribute data once daily")
	fmt.Println("  5. ⚡ Cache results to avoid duplicate requests")
	fmt.Println("  6. 🔄 Implement exponential backoff on errors")
}

// SmartLocationRequest determines if we should request location from OpenCellID
func (s *OpenCellIDUsageStrategy) ShouldRequestLocation(gpsStatus string, gpsAccuracy float64) (bool, string) {
	// Reset daily counter if needed
	s.resetDailyCounterIfNeeded()

	// Check if we have requests available
	availableRequests := s.DailyRequestLimit - s.RequestsToday - s.EmergencyRequestsReserved
	if availableRequests <= 0 {
		return false, "daily request limit reached"
	}

	// Only request when GPS is problematic
	switch gpsStatus {
	case "unavailable":
		return true, "GPS unavailable - using cell tower fallback"
	case "poor_accuracy":
		if gpsAccuracy > 10.0 {
			return true, fmt.Sprintf("GPS accuracy poor (%.1fm) - using cell tower", gpsAccuracy)
		}
	case "signal_lost":
		return true, "GPS signal lost - using cell tower fallback"
	case "indoor":
		return true, "Indoor location - GPS blocked, using cell tower"
	case "emergency":
		// Always allow emergency requests (use reserved quota)
		return true, "Emergency location request"
	default:
		return false, "GPS working well - no need for cell tower location"
	}

	return false, "GPS status acceptable"
}

// ShouldContributeData determines if we should contribute data to OpenCellID
func (s *OpenCellIDUsageStrategy) ShouldContributeData(gpsAccuracy float64, signalStrength int, lastContribution time.Time) (bool, string) {
	// Reset daily counter if needed
	s.resetDailyCounterIfNeeded()

	// Check daily contribution limit (max 1 per day to conserve requests)
	if s.ContributionsToday >= 1 {
		return false, "already contributed today"
	}

	// Only contribute high-quality data
	if gpsAccuracy > 5.0 {
		return false, fmt.Sprintf("GPS accuracy too poor (%.1fm) - need <5m for contribution", gpsAccuracy)
	}

	if signalStrength < -95 {
		return false, fmt.Sprintf("signal too weak (%d dBm) - need >-95 dBm", signalStrength)
	}

	// Don't contribute too frequently
	if time.Since(lastContribution) < 23*time.Hour {
		return false, "contributed too recently - wait 23+ hours"
	}

	return true, "high-quality data ready for contribution"
}

// resetDailyCounterIfNeeded resets counters at midnight UTC
func (s *OpenCellIDUsageStrategy) resetDailyCounterIfNeeded() {
	today := time.Now().UTC().Truncate(24 * time.Hour)
	if today.After(s.LastResetDate) {
		s.RequestsToday = 0
		s.ContributionsToday = 0
		s.LastResetDate = today
	}
}

// RecordLocationRequest records a location request
func (s *OpenCellIDUsageStrategy) RecordLocationRequest() {
	s.resetDailyCounterIfNeeded()
	s.RequestsToday++
}

// RecordContribution records a data contribution
func (s *OpenCellIDUsageStrategy) RecordContribution() {
	s.resetDailyCounterIfNeeded()
	s.RequestsToday++ // Contributions count as requests
	s.ContributionsToday++
}

// GetUsageStatus returns current usage status
func (s *OpenCellIDUsageStrategy) GetUsageStatus() map[string]interface{} {
	s.resetDailyCounterIfNeeded()

	availableRequests := s.DailyRequestLimit - s.RequestsToday - s.EmergencyRequestsReserved
	usagePercent := float64(s.RequestsToday) / float64(s.DailyRequestLimit) * 100

	return map[string]interface{}{
		"requests_today":      s.RequestsToday,
		"daily_limit":         s.DailyRequestLimit,
		"available_requests":  availableRequests,
		"emergency_reserved":  s.EmergencyRequestsReserved,
		"usage_percent":       usagePercent,
		"contributions_today": s.ContributionsToday,
		"reset_date":          s.LastResetDate.Format("2006-01-02"),
	}
}

// testUsageStrategy demonstrates the usage strategy
func testUsageStrategy() {
	fmt.Println("🧪 TESTING OPENCELLID USAGE STRATEGY")
	fmt.Println("=" + strings.Repeat("=", 40))

	strategy := createOptimalUsageStrategy()

	// Test scenarios
	scenarios := []struct {
		name           string
		gpsStatus      string
		gpsAccuracy    float64
		signalStrength int
		lastContrib    time.Time
	}{
		{"GPS Working Well", "good", 0.4, -53, time.Now().Add(-25 * time.Hour)},
		{"GPS Poor Accuracy", "poor_accuracy", 15.0, -53, time.Now().Add(-25 * time.Hour)},
		{"GPS Unavailable", "unavailable", 0, -53, time.Now().Add(-25 * time.Hour)},
		{"Indoor Location", "indoor", 0, -53, time.Now().Add(-25 * time.Hour)},
		{"Emergency", "emergency", 0, -53, time.Now().Add(-25 * time.Hour)},
		{"Poor Signal", "good", 0.4, -105, time.Now().Add(-25 * time.Hour)},
		{"Recent Contribution", "good", 0.4, -53, time.Now().Add(-1 * time.Hour)},
	}

	fmt.Println("\n📊 SCENARIO TESTING:")
	for i, scenario := range scenarios {
		fmt.Printf("\n%d. %s:\n", i+1, scenario.name)

		// Test location request
		shouldRequest, requestReason := strategy.ShouldRequestLocation(scenario.gpsStatus, scenario.gpsAccuracy)
		fmt.Printf("   📍 Request Location: %v (%s)\n", shouldRequest, requestReason)

		// Test contribution
		shouldContribute, contribReason := strategy.ShouldContributeData(
			scenario.gpsAccuracy, scenario.signalStrength, scenario.lastContrib)
		fmt.Printf("   📤 Contribute Data: %v (%s)\n", shouldContribute, contribReason)
	}

	// Show usage status
	fmt.Println("\n📊 CURRENT USAGE STATUS:")
	status := strategy.GetUsageStatus()
	for key, value := range status {
		fmt.Printf("   %s: %v\n", key, value)
	}

	fmt.Println("\n💡 IMPLEMENTATION RECOMMENDATIONS:")
	fmt.Println("   ✅ Integrate into autonomy GPS collector")
	fmt.Println("   ✅ Check strategy before each API call")
	fmt.Println("   ✅ Cache results to avoid duplicate requests")
	fmt.Println("   ✅ Monitor daily usage and adjust if needed")
	fmt.Println("   ✅ Implement exponential backoff on errors")
	fmt.Println("   ✅ Log all API usage for monitoring")
}
