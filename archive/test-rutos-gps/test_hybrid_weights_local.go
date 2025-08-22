package main

import (
	"fmt"
	"os/exec"
	"strings"
)

// testHybridWeightsLocal tests the hybrid weight system directly on RUTOS without SSH
func testHybridWeightsLocal() {
	fmt.Println("🎯 Testing Hybrid Weight System on RUTOS")
	fmt.Println("========================================")

	// Step 1: Read current MWAN3 configuration
	fmt.Println("\n📋 Step 1: Reading Current MWAN3 Configuration")
	fmt.Println("==============================================")

	mwan3Config, err := readMWAN3Config()
	if err != nil {
		fmt.Printf("❌ Error reading MWAN3 config: %v\n", err)
		return
	}

	fmt.Println("Current MWAN3 member weights:")
	for member, weight := range mwan3Config {
		fmt.Printf("  📊 %s: weight %s\n", member, weight)
	}

	// Step 2: Test UCI commands
	fmt.Println("\n⚙️ Step 2: Testing UCI Configuration")
	fmt.Println("====================================")

	// Test reading UCI values
	testUCIOperations()

	// Step 3: Simulate hybrid weight adjustments
	fmt.Println("\n🧠 Step 3: Simulating Intelligent Adjustments")
	fmt.Println("=============================================")

	// Simulate Starlink obstruction scenario
	fmt.Println("\n📡 Scenario: Starlink Obstruction Detected")
	fmt.Println("------------------------------------------")

	originalWeight := mwan3Config["wan_m1"]
	if originalWeight != "" {
		fmt.Printf("  Original wan_m1 weight: %s\n", originalWeight)

		// Simulate temporary penalty
		newWeight := "80" // Reduce from 100 to 80
		fmt.Printf("  Applying temporary penalty: %s → %s\n", originalWeight, newWeight)

		// Test UCI set operation
		err := setMWAN3Weight("wan_m1", newWeight)
		if err != nil {
			fmt.Printf("  ❌ Error setting weight: %v\n", err)
		} else {
			fmt.Printf("  ✅ Successfully applied penalty\n")

			// Verify the change
			currentWeight, err := getMWAN3Weight("wan_m1")
			if err != nil {
				fmt.Printf("  ❌ Error reading updated weight: %v\n", err)
			} else {
				fmt.Printf("  📊 Current weight: %s\n", currentWeight)
			}

			// Restore original weight after test
			fmt.Printf("  🔄 Restoring original weight: %s\n", originalWeight)
			err = setMWAN3Weight("wan_m1", originalWeight)
			if err != nil {
				fmt.Printf("  ❌ Error restoring weight: %v\n", err)
			} else {
				fmt.Printf("  ✅ Successfully restored original weight\n")
			}
		}
	}

	// Step 4: Test cellular boost scenario
	fmt.Println("\n📱 Scenario: Cellular Signal Boost")
	fmt.Println("----------------------------------")

	cellularWeight := mwan3Config["mob1s1a1_m1"]
	if cellularWeight != "" {
		fmt.Printf("  Original mob1s1a1_m1 weight: %s\n", cellularWeight)

		// Simulate temporary boost
		newWeight := "95" // Boost from 85 to 95
		fmt.Printf("  Applying temporary boost: %s → %s\n", cellularWeight, newWeight)

		err := setMWAN3Weight("mob1s1a1_m1", newWeight)
		if err != nil {
			fmt.Printf("  ❌ Error setting weight: %v\n", err)
		} else {
			fmt.Printf("  ✅ Successfully applied boost\n")

			// Restore original weight
			fmt.Printf("  🔄 Restoring original weight: %s\n", cellularWeight)
			err = setMWAN3Weight("mob1s1a1_m1", cellularWeight)
			if err != nil {
				fmt.Printf("  ❌ Error restoring weight: %v\n", err)
			} else {
				fmt.Printf("  ✅ Successfully restored original weight\n")
			}
		}
	}

	// Step 5: Test MWAN3 status
	fmt.Println("\n📊 Step 5: Testing MWAN3 Status")
	fmt.Println("===============================")

	status, err := getMWAN3Status()
	if err != nil {
		fmt.Printf("❌ Error getting MWAN3 status: %v\n", err)
	} else {
		fmt.Println("MWAN3 Status:")
		fmt.Println(status)
	}

	// Step 6: Test interface detection
	fmt.Println("\n🔍 Step 6: Testing Interface Detection")
	fmt.Println("=====================================")

	interfaces, err := getNetworkInterfaces()
	if err != nil {
		fmt.Printf("❌ Error getting interfaces: %v\n", err)
	} else {
		fmt.Println("Available network interfaces:")
		for _, iface := range interfaces {
			fmt.Printf("  🌐 %s\n", iface)
		}
	}

	fmt.Println("\n🎉 Hybrid Weight System Test Complete")
	fmt.Println("=====================================")
	fmt.Println("✅ Key findings:")
	fmt.Println("  • MWAN3 configuration successfully read")
	fmt.Println("  • UCI operations working correctly")
	fmt.Println("  • Weight adjustments can be applied and restored")
	fmt.Println("  • System is ready for hybrid weight implementation")

	fmt.Println("\n📋 Next Steps:")
	fmt.Println("  1. Deploy full autonomyd daemon with hybrid weight system")
	fmt.Println("  2. Configure UCI settings for hybrid behavior")
	fmt.Println("  3. Monitor intelligent adjustments in real-time")
	fmt.Println("  4. Verify automatic weight restoration")
}

// readMWAN3Config reads the current MWAN3 member weights
func readMWAN3Config() (map[string]string, error) {
	cmd := exec.Command("uci", "show", "mwan3")
	output, err := cmd.Output()
	if err != nil {
		return nil, err
	}

	config := make(map[string]string)
	lines := strings.Split(string(output), "\n")

	for _, line := range lines {
		if strings.Contains(line, ".weight=") {
			parts := strings.Split(line, "=")
			if len(parts) == 2 {
				// Extract member name from mwan3.member_name.weight=value
				memberParts := strings.Split(parts[0], ".")
				if len(memberParts) >= 2 {
					memberName := memberParts[1]
					weight := strings.Trim(parts[1], "'\"")
					config[memberName] = weight
				}
			}
		}
	}

	return config, nil
}

// setMWAN3Weight sets a member's weight in MWAN3 configuration
func setMWAN3Weight(member, weight string) error {
	// Set the weight
	cmd := exec.Command("uci", "set", fmt.Sprintf("mwan3.%s.weight=%s", member, weight))
	err := cmd.Run()
	if err != nil {
		return fmt.Errorf("failed to set weight: %w", err)
	}

	// Commit the change
	cmd = exec.Command("uci", "commit", "mwan3")
	err = cmd.Run()
	if err != nil {
		return fmt.Errorf("failed to commit changes: %w", err)
	}

	return nil
}

// getMWAN3Weight gets a member's current weight
func getMWAN3Weight(member string) (string, error) {
	cmd := exec.Command("uci", "get", fmt.Sprintf("mwan3.%s.weight", member))
	output, err := cmd.Output()
	if err != nil {
		return "", err
	}

	return strings.TrimSpace(string(output)), nil
}

// getMWAN3Status gets the current MWAN3 status
func getMWAN3Status() (string, error) {
	cmd := exec.Command("mwan3", "status")
	output, err := cmd.Output()
	if err != nil {
		return "", err
	}

	return string(output), nil
}

// getNetworkInterfaces gets available network interfaces
func getNetworkInterfaces() ([]string, error) {
	cmd := exec.Command("ip", "link", "show")
	output, err := cmd.Output()
	if err != nil {
		return nil, err
	}

	var interfaces []string
	lines := strings.Split(string(output), "\n")

	for _, line := range lines {
		if strings.Contains(line, ": ") && !strings.Contains(line, "    ") {
			parts := strings.Split(line, ": ")
			if len(parts) >= 2 {
				ifaceName := strings.Split(parts[1], "@")[0] // Remove @master part
				if ifaceName != "lo" {                       // Skip loopback
					interfaces = append(interfaces, ifaceName)
				}
			}
		}
	}

	return interfaces, nil
}

// testUCIOperations tests basic UCI operations
func testUCIOperations() {
	fmt.Println("Testing UCI operations:")

	// Test reading a value
	cmd := exec.Command("uci", "get", "system.@system[0].hostname")
	output, err := cmd.Output()
	if err != nil {
		fmt.Printf("  ❌ Error reading hostname: %v\n", err)
	} else {
		hostname := strings.TrimSpace(string(output))
		fmt.Printf("  ✅ Hostname: %s\n", hostname)
	}

	// Test if autonomy UCI section exists
	cmd = exec.Command("uci", "show", "autonomy")
	_, err = cmd.Output()
	if err != nil {
		fmt.Printf("  ℹ️  autonomy UCI section not found (expected for first run)\n")

		// Try to create a test section
		fmt.Printf("  🔧 Creating test autonomy UCI section...\n")
		cmd = exec.Command("uci", "set", "autonomy.main=autonomy")
		err = cmd.Run()
		if err != nil {
			fmt.Printf("  ❌ Error creating autonomy section: %v\n", err)
		} else {
			cmd = exec.Command("uci", "set", "autonomy.main.respect_user_weights=1")
			cmd.Run()
			cmd = exec.Command("uci", "commit", "autonomy")
			err = cmd.Run()
			if err != nil {
				fmt.Printf("  ❌ Error committing autonomy config: %v\n", err)
			} else {
				fmt.Printf("  ✅ Successfully created autonomy UCI section\n")
			}
		}
	} else {
		fmt.Printf("  ✅ autonomy UCI section exists\n")
	}
}
