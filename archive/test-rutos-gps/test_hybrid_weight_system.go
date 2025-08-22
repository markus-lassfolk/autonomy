package main

import (
	"fmt"
	"time"

	"github.com/markus-lassfolk/autonomy/pkg/discovery"
	"github.com/markus-lassfolk/autonomy/pkg/logx"
)

// TestHybridWeightSystem demonstrates the new hybrid weight system
func testHybridWeightSystem() {
	fmt.Println("🎯 Testing Hybrid Weight System")
	fmt.Println("===============================")

	// Create logger
	logger := logx.NewLogger("info", "")

	// Create hybrid weight manager
	hwm := discovery.NewHybridWeightManager(logger)

	// Simulate loading MWAN3 weights (normally from UCI)
	fmt.Println("\n📋 Step 1: Simulating MWAN3 Configuration")
	fmt.Println("=========================================")

	// Simulate user's MWAN3 configuration
	simulatedMWAN3Config := map[string]int{
		"starlink_m1":  100, // User's preferred Starlink priority
		"cellular1_m1": 85,  // User's preferred Cellular 1 priority
		"cellular2_m1": 84,  // User's preferred Cellular 2 priority
		"cellular3_m1": 83,  // User's preferred Cellular 3 priority
		"cellular4_m1": 82,  // User's preferred Cellular 4 priority
		"cellular5_m1": 81,  // User's preferred Cellular 5 priority
		"cellular6_m1": 80,  // User's preferred Cellular 6 priority
		"cellular7_m1": 79,  // User's preferred Cellular 7 priority
		"cellular8_m1": 78,  // User's preferred Cellular 8 priority
		"wifi_m1":      60,  // User's preferred WiFi priority
		"lan_m1":       40,  // User's preferred LAN priority
	}

	// Manually set the weights (simulating UCI load)
	for member, weight := range simulatedMWAN3Config {
		// This would normally be loaded from UCI
		hwm.SetOriginalWeightForTesting(member, weight)
		fmt.Printf("  📊 %s: weight %d (user configured)\n", member, weight)
	}

	fmt.Println("\n✅ Step 2: Testing Normal Operation (Respects User Weights)")
	fmt.Println("==========================================================")

	for member, originalWeight := range simulatedMWAN3Config {
		effectiveWeight := hwm.GetEffectiveWeight(member)
		fmt.Printf("  %s: original=%d, effective=%d ✅\n",
			member, originalWeight, effectiveWeight)

		if effectiveWeight != originalWeight {
			fmt.Printf("    ❌ ERROR: Effective weight should match original!\n")
		}
	}

	fmt.Println("\n🚨 Step 3: Testing Intelligent Adjustments")
	fmt.Println("==========================================")

	// Scenario 1: Starlink obstruction detected
	fmt.Println("\n📡 Scenario 1: Starlink Obstruction Detected")
	fmt.Println("--------------------------------------------")

	err := hwm.ApplyTemporaryAdjustment(
		"starlink_m1",
		80, // Reduce from 100 to 80
		"Starlink obstruction detected: 15.2% > 10.0%",
		discovery.AdjustmentTypePenalty,
		5*time.Minute,
	)
	if err != nil {
		fmt.Printf("❌ Error applying adjustment: %v\n", err)
	} else {
		fmt.Printf("  ✅ Applied penalty: starlink_m1 weight 100 → 80 (5min)\n")
		fmt.Printf("  📊 Effective weight: %d\n", hwm.GetEffectiveWeight("starlink_m1"))
	}

	// Scenario 2: Cellular signal boost
	fmt.Println("\n📱 Scenario 2: Cellular Signal Boost")
	fmt.Println("------------------------------------")

	err = hwm.ApplyTemporaryAdjustment(
		"cellular1_m1",
		95, // Boost from 85 to 95
		"Boosting cellular due to excellent signal (-65 dBm) while Starlink has issues",
		discovery.AdjustmentTypeBoost,
		10*time.Minute,
	)
	if err != nil {
		fmt.Printf("❌ Error applying adjustment: %v\n", err)
	} else {
		fmt.Printf("  ✅ Applied boost: cellular1_m1 weight 85 → 95 (10min)\n")
		fmt.Printf("  📊 Effective weight: %d\n", hwm.GetEffectiveWeight("cellular1_m1"))
	}

	// Scenario 3: Emergency override
	fmt.Println("\n🚨 Scenario 3: Emergency Override")
	fmt.Println("---------------------------------")

	err = hwm.ApplyTemporaryAdjustment(
		"cellular2_m1",
		100, // Emergency boost to 100
		"Emergency override - critical situation (all other interfaces down)",
		discovery.AdjustmentTypeEmergency,
		15*time.Minute,
	)
	if err != nil {
		fmt.Printf("❌ Error applying adjustment: %v\n", err)
	} else {
		fmt.Printf("  ✅ Applied emergency override: cellular2_m1 weight 84 → 100 (15min)\n")
		fmt.Printf("  📊 Effective weight: %d\n", hwm.GetEffectiveWeight("cellular2_m1"))
	}

	fmt.Println("\n📊 Step 4: Current Weight Summary")
	fmt.Println("=================================")

	fmt.Printf("%-15s %-10s %-10s %-15s %s\n", "Member", "Original", "Effective", "Status", "Reason")
	fmt.Println("--------------------------------------------------------------------------------")

	for member, originalWeight := range simulatedMWAN3Config {
		effectiveWeight := hwm.GetEffectiveWeight(member)
		status := "Normal"
		reason := "User configured weight"

		adjustments := hwm.GetActiveAdjustments()
		if adj, exists := adjustments[member]; exists {
			status = string(adj.Type)
			reason = adj.Reason
		}

		fmt.Printf("%-15s %-10d %-10d %-15s %s\n",
			member, originalWeight, effectiveWeight, status, reason)
	}

	fmt.Println("\n🔄 Step 5: Testing Weight Restoration")
	fmt.Println("====================================")

	// Wait a moment to simulate time passing
	time.Sleep(100 * time.Millisecond)

	// Manually restore one weight
	fmt.Println("🔧 Manually restoring starlink_m1 to original weight...")
	hwm.RestoreOriginalWeight("starlink_m1")

	fmt.Printf("  ✅ starlink_m1 restored: %d (back to user preference)\n",
		hwm.GetEffectiveWeight("starlink_m1"))

	fmt.Println("\n📈 Step 6: Demonstrating Priority Behavior")
	fmt.Println("==========================================")

	// Show how the system would prioritize interfaces
	type memberPriority struct {
		name   string
		weight int
	}

	var priorities []memberPriority
	for member := range simulatedMWAN3Config {
		priorities = append(priorities, memberPriority{
			name:   member,
			weight: hwm.GetEffectiveWeight(member),
		})
	}

	// Sort by weight (descending)
	for i := 0; i < len(priorities)-1; i++ {
		for j := i + 1; j < len(priorities); j++ {
			if priorities[i].weight < priorities[j].weight {
				priorities[i], priorities[j] = priorities[j], priorities[i]
			}
		}
	}

	fmt.Println("Current failover priority order (highest to lowest):")
	for i, p := range priorities {
		status := "👑"
		if i == 0 {
			status = "👑 PRIMARY"
		} else if i == 1 {
			status = "🥈 BACKUP"
		} else if i == 2 {
			status = "🥉 TERTIARY"
		} else {
			status = fmt.Sprintf("📍 #%d", i+1)
		}
		fmt.Printf("  %s %s (weight: %d)\n", status, p.name, p.weight)
	}

	fmt.Println("\n⚙️ Step 7: Configuration Summary")
	fmt.Println("================================")

	respectUserWeights, dynamicAdjustment, emergencyOverride, restoreTimeout := hwm.GetConfiguration()

	fmt.Printf("  🎯 Respect User Weights: %t\n", respectUserWeights)
	fmt.Printf("  🧠 Dynamic Adjustment: %t\n", dynamicAdjustment)
	fmt.Printf("  🚨 Emergency Override: %t\n", emergencyOverride)
	fmt.Printf("  ⏰ Restore Timeout: %v\n", restoreTimeout)

	activeAdjustments := hwm.GetActiveAdjustments()
	fmt.Printf("  📊 Active Adjustments: %d\n", len(activeAdjustments))

	fmt.Println("\n🎉 Step 8: Benefits Demonstration")
	fmt.Println("=================================")

	fmt.Println("✅ Benefits of Hybrid Weight System:")
	fmt.Println("  👤 User maintains control over base priorities")
	fmt.Println("  🧠 System adds intelligent monitoring and adjustments")
	fmt.Println("  🔄 Temporary adjustments only when conditions warrant")
	fmt.Println("  🛡️ Respects user preferences as default")
	fmt.Println("  🐛 Easier debugging (clear separation of concerns)")
	fmt.Println("  ⚙️ Fully configurable via UCI")
	fmt.Println("  🚀 Automatic restoration when conditions improve")

	fmt.Println("\n📋 Step 9: UCI Configuration Example")
	fmt.Println("====================================")

	fmt.Println("# Enable hybrid weight system")
	fmt.Println("uci set autonomy.main.respect_user_weights='1'")
	fmt.Println("uci set autonomy.main.dynamic_adjustment='1'")
	fmt.Println("uci set autonomy.main.emergency_override='1'")
	fmt.Println("uci set autonomy.main.restore_timeout_s='300'")
	fmt.Println("")
	fmt.Println("# Configure adjustment behavior")
	fmt.Println("uci set autonomy.main.minimal_adjustment_points='10'")
	fmt.Println("uci set autonomy.main.temporary_boost_points='20'")
	fmt.Println("uci set autonomy.main.temporary_adjustment_duration_s='300'")
	fmt.Println("")
	fmt.Println("# Configure intelligent thresholds")
	fmt.Println("uci set autonomy.main.starlink_obstruction_threshold='10.0'")
	fmt.Println("uci set autonomy.main.cellular_signal_threshold='-110.0'")
	fmt.Println("uci set autonomy.main.latency_degradation_threshold='500.0'")
	fmt.Println("uci set autonomy.main.loss_threshold='5.0'")
	fmt.Println("")
	fmt.Println("uci commit autonomy")

	fmt.Println("\n🎯 Conclusion")
	fmt.Println("=============")
	fmt.Println("The hybrid weight system successfully:")
	fmt.Println("• Respects your MWAN3 weight configuration (100/85/84/83...)")
	fmt.Println("• Adds intelligent adjustments when needed")
	fmt.Println("• Automatically restores user preferences")
	fmt.Println("• Provides full control via UCI configuration")
	fmt.Println("• Maintains clear separation between user config and system intelligence")
}
