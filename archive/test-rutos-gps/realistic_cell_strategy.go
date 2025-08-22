package main

import (
	"fmt"
	"strings"
	"time"
)

// RealisticCellStrategy provides sensible cell tower location intervals
type RealisticCellStrategy struct {
	Name               string
	LocationInterval   time.Duration
	MaxRequestsPerDay  int
	UseCaseDescription string
	Pros               []string
	Cons               []string
}

// getRealisticStrategies returns different interval strategies
func getRealisticStrategies() []RealisticCellStrategy {
	return []RealisticCellStrategy{
		{
			Name:               "Conservative (10 minutes)",
			LocationInterval:   10 * time.Minute,
			MaxRequestsPerDay:  144, // 24*60/10 = 144
			UseCaseDescription: "Best for stable installations, minimal API usage",
			Pros: []string{
				"Very low API usage (144 requests/day max)",
				"Plenty of quota for emergencies (4,856 requests available)",
				"Suitable for stationary installations",
				"Battery friendly",
			},
			Cons: []string{
				"Slower response to location changes",
				"Less precise for mobile use cases",
			},
		},
		{
			Name:               "Balanced (5 minutes)",
			LocationInterval:   5 * time.Minute,
			MaxRequestsPerDay:  288, // 24*60/5 = 288
			UseCaseDescription: "Good balance between responsiveness and API usage",
			Pros: []string{
				"Reasonable API usage (288 requests/day max)",
				"Good responsiveness for location changes",
				"Still plenty of quota available (4,612 requests)",
				"Suitable for most use cases",
			},
			Cons: []string{
				"Higher usage than 10-minute interval",
			},
		},
		{
			Name:               "Responsive (1 minute)",
			LocationInterval:   1 * time.Minute,
			MaxRequestsPerDay:  1440, // 24*60/1 = 1440
			UseCaseDescription: "Quick response for mobile or critical applications",
			Pros: []string{
				"Very responsive to location changes",
				"Good for mobile installations",
				"Quick GPS fallback",
			},
			Cons: []string{
				"Higher API usage (1,440 requests/day)",
				"Less quota for other uses (3,460 requests available)",
				"May be overkill for stationary installations",
			},
		},
		{
			Name:               "Adaptive (Smart)",
			LocationInterval:   0,   // Variable
			MaxRequestsPerDay:  500, // Estimated average
			UseCaseDescription: "Adapts interval based on GPS status and movement",
			Pros: []string{
				"Optimal API usage",
				"Fast when needed, slow when stable",
				"Best overall efficiency",
				"Adapts to usage patterns",
			},
			Cons: []string{
				"More complex implementation",
				"Requires movement detection",
			},
		},
	}
}

// displayRealisticStrategies shows all strategy options
func displayRealisticStrategies() {
	fmt.Println("🎯 REALISTIC CELL TOWER LOCATION STRATEGIES")
	fmt.Println("=" + strings.Repeat("=", 50))
	fmt.Println("📊 You're absolutely right - cell towers don't move!")
	fmt.Println("📍 Much more sensible intervals for location requests")
	fmt.Println()

	strategies := getRealisticStrategies()

	for i, strategy := range strategies {
		fmt.Printf("%d. 🎯 %s\n", i+1, strategy.Name)
		fmt.Printf("   ⏰ Interval: %v\n", strategy.LocationInterval)
		if strategy.LocationInterval > 0 {
			fmt.Printf("   📊 Max Usage: %d requests/day (%.1f%% of quota)\n",
				strategy.MaxRequestsPerDay, float64(strategy.MaxRequestsPerDay)/50.0)
			fmt.Printf("   💰 Remaining: %d requests for other uses\n",
				5000-strategy.MaxRequestsPerDay-100) // -100 for emergency reserve
		} else {
			fmt.Printf("   📊 Est. Usage: %d requests/day (adaptive)\n", strategy.MaxRequestsPerDay)
		}
		fmt.Printf("   📋 Use Case: %s\n", strategy.UseCaseDescription)

		fmt.Printf("   ✅ Pros:\n")
		for _, pro := range strategy.Pros {
			fmt.Printf("      • %s\n", pro)
		}

		fmt.Printf("   ❌ Cons:\n")
		for _, con := range strategy.Cons {
			fmt.Printf("      • %s\n", con)
		}
		fmt.Println()
	}

	fmt.Println("💡 RECOMMENDATION FOR YOUR autonomy SYSTEM:")
	fmt.Println("   🎯 Start with 'Balanced (5 minutes)' strategy")
	fmt.Println("   📊 288 requests/day max = only 5.8% of quota used")
	fmt.Println("   🚨 4,612 requests still available for emergencies")
	fmt.Println("   ⚡ Good responsiveness without waste")
	fmt.Println("   🔄 Can adjust based on real-world usage patterns")
}

// AdaptiveLocationStrategy implements smart interval adjustment
type AdaptiveLocationStrategy struct {
	BaseInterval        time.Duration
	CurrentInterval     time.Duration
	LastLocationTime    time.Time
	LastGPSStatus       string
	MovementDetected    bool
	ConsecutiveFailures int
}

// NewAdaptiveStrategy creates a new adaptive strategy
func NewAdaptiveStrategy() *AdaptiveLocationStrategy {
	return &AdaptiveLocationStrategy{
		BaseInterval:    5 * time.Minute, // Start with 5 minutes
		CurrentInterval: 5 * time.Minute,
	}
}

// GetNextInterval calculates the next request interval based on conditions
func (a *AdaptiveLocationStrategy) GetNextInterval(gpsStatus string, locationChanged bool) time.Duration {
	switch {
	case gpsStatus == "emergency":
		// Emergency: request immediately, then back to base
		a.CurrentInterval = 30 * time.Second

	case gpsStatus == "unavailable" && a.ConsecutiveFailures > 3:
		// GPS completely failed: more frequent requests
		a.CurrentInterval = 2 * time.Minute

	case gpsStatus == "poor_accuracy":
		// Poor GPS: moderate frequency
		a.CurrentInterval = 3 * time.Minute

	case locationChanged || a.MovementDetected:
		// Movement detected: more frequent for a while
		a.CurrentInterval = 2 * time.Minute

	case gpsStatus == "good":
		// GPS working well: extend interval
		a.CurrentInterval = 10 * time.Minute

	default:
		// Default: use base interval
		a.CurrentInterval = a.BaseInterval
	}

	// Ensure minimum interval (don't spam the API)
	if a.CurrentInterval < 30*time.Second {
		a.CurrentInterval = 30 * time.Second
	}

	// Ensure maximum interval (don't wait too long)
	if a.CurrentInterval > 15*time.Minute {
		a.CurrentInterval = 15 * time.Minute
	}

	return a.CurrentInterval
}

// demonstrateAdaptiveStrategy shows how adaptive strategy works
func demonstrateAdaptiveStrategy() {
	fmt.Println("\n🧠 ADAPTIVE STRATEGY DEMONSTRATION")
	fmt.Println("=" + strings.Repeat("=", 40))

	adaptive := NewAdaptiveStrategy()

	scenarios := []struct {
		gpsStatus       string
		locationChanged bool
		description     string
	}{
		{"good", false, "GPS working well, stationary"},
		{"poor_accuracy", false, "GPS accuracy degraded"},
		{"unavailable", false, "GPS signal lost"},
		{"unavailable", false, "GPS still unavailable (consecutive failure)"},
		{"good", true, "GPS recovered, location changed"},
		{"good", false, "GPS stable, no movement"},
		{"emergency", false, "Emergency location request"},
	}

	fmt.Println("📊 Scenario Testing:")
	for i, scenario := range scenarios {
		if scenario.gpsStatus == "unavailable" {
			adaptive.ConsecutiveFailures++
		} else {
			adaptive.ConsecutiveFailures = 0
		}

		interval := adaptive.GetNextInterval(scenario.gpsStatus, scenario.locationChanged)

		fmt.Printf("%d. %s\n", i+1, scenario.description)
		fmt.Printf("   ⏰ Next request in: %v\n", interval)
		fmt.Printf("   📊 Daily usage if sustained: %d requests\n",
			int(24*time.Hour/interval))
		fmt.Println()
	}

	fmt.Println("💡 ADAPTIVE BENEFITS:")
	fmt.Println("   ⚡ Fast response when needed (30s-2min)")
	fmt.Println("   🔋 Efficient when stable (10-15min)")
	fmt.Println("   📊 Optimal API usage (typically 200-800 requests/day)")
	fmt.Println("   🎯 Adapts to real conditions automatically")
}

// testRealisticStrategy tests the realistic strategy approach
func testRealisticStrategy() {
	displayRealisticStrategies()
	demonstrateAdaptiveStrategy()

	fmt.Println("\n🎯 IMPLEMENTATION RECOMMENDATION:")
	fmt.Println("   1. 🚀 Start with 5-minute intervals (simple, effective)")
	fmt.Println("   2. 📊 Monitor actual usage patterns")
	fmt.Println("   3. 🧠 Consider adaptive strategy for optimization")
	fmt.Println("   4. 🔄 Adjust based on real-world performance")
	fmt.Println()
	fmt.Println("📊 EXPECTED USAGE WITH 5-MINUTE INTERVALS:")
	fmt.Println("   📍 Normal operation: 0-50 requests/day (GPS working)")
	fmt.Println("   🏢 Indoor use: 100-288 requests/day (GPS blocked)")
	fmt.Println("   🚨 GPS failure: 288 requests/day (continuous fallback)")
	fmt.Println("   💰 Total quota used: <6% (plenty of headroom)")
}
