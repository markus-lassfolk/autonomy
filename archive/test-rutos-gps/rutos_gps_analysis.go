package main

import (
	"fmt"
	"strings"
	"time"

	"golang.org/x/crypto/ssh"
)

// analyzeRutOSGPSMethods explores all possible GPS integration methods
func analyzeRutOSGPSMethods(client *ssh.Client) {
	fmt.Println("🔍 RutOS GPS Integration Analysis")
	fmt.Println("=" + strings.Repeat("=", 35))

	// 1. Check RutOS GPS services and daemons
	fmt.Println("\n📡 1. GPS Services & Daemons:")
	checkGPSServices(client)

	// 2. Check RutOS API methods
	fmt.Println("\n🔧 2. RutOS API Methods:")
	checkRutOSAPIs(client)

	// 3. Check GPS configuration files
	fmt.Println("\n📄 3. GPS Configuration:")
	checkGPSConfig(client)

	// 4. Check modem GPS capabilities
	fmt.Println("\n📱 4. Modem GPS Capabilities:")
	checkModemGPS(client)

	// 5. Analyze /dev/tty reliability
	fmt.Println("\n⚠️  5. /dev/tty Reliability Analysis:")
	analyzeTTYReliability(client)

	// 6. Recommendations
	fmt.Println("\n💡 6. Integration Recommendations:")
	provideRecommendations()
}

func checkGPSServices(client *ssh.Client) {
	services := map[string]string{
		"gpsd status":        "ps | grep gpsd",
		"gpsd config":        "cat /etc/default/gpsd 2>/dev/null || echo 'No gpsd config'",
		"gpsd socket":        "netstat -ln | grep 2947 || echo 'No gpsd socket'",
		"ntp_gps status":     "ps | grep ntp_gps",
		"GPS kernel modules": "lsmod | grep gps || echo 'No GPS modules'",
		"GPS device drivers": "dmesg | grep -i gps | tail -5 || echo 'No GPS in dmesg'",
	}

	for name, cmd := range services {
		fmt.Printf("  %s:\n", name)
		output, err := executeCommand(client, cmd)
		if err != nil {
			fmt.Printf("    ❌ Error: %v\n", err)
		} else {
			lines := strings.Split(strings.TrimSpace(output), "\n")
			for _, line := range lines {
				if strings.TrimSpace(line) != "" {
					fmt.Printf("    %s\n", line)
				}
			}
		}
		fmt.Println()
	}
}

func checkRutOSAPIs(client *ssh.Client) {
	apis := map[string]string{
		"ubus list GPS":      "ubus list | grep -i gps || echo 'No GPS in ubus'",
		"ubus list location": "ubus list | grep -i location || echo 'No location in ubus'",
		"ubus list modem":    "ubus list | grep -i modem",
		"uci GPS config":     "uci show | grep -i gps || echo 'No GPS in UCI'",
		"luci GPS modules":   "find /usr/lib/lua/luci -name '*gps*' 2>/dev/null || echo 'No GPS modules'",
		"RutOS GPS API":      "find /usr/bin -name '*gps*' 2>/dev/null",
	}

	for name, cmd := range apis {
		fmt.Printf("  %s:\n", name)
		output, err := executeCommand(client, cmd)
		if err != nil {
			fmt.Printf("    ❌ Error: %v\n", err)
		} else {
			lines := strings.Split(strings.TrimSpace(output), "\n")
			for _, line := range lines {
				if strings.TrimSpace(line) != "" {
					fmt.Printf("    %s\n", line)
				}
			}
		}
		fmt.Println()
	}
}

func checkGPSConfig(client *ssh.Client) {
	configs := map[string]string{
		"GPS config files":    "find /etc -name '*gps*' 2>/dev/null",
		"Modem config":        "cat /etc/config/network | grep -A 10 -B 5 gps || echo 'No GPS in network config'",
		"System config":       "cat /etc/config/system | grep -A 5 -B 5 gps || echo 'No GPS in system config'",
		"GPS init scripts":    "find /etc/init.d -name '*gps*' 2>/dev/null",
		"GPS hotplug scripts": "find /etc/hotplug.d -name '*gps*' 2>/dev/null || echo 'No GPS hotplug'",
	}

	for name, cmd := range configs {
		fmt.Printf("  %s:\n", name)
		output, err := executeCommand(client, cmd)
		if err != nil {
			fmt.Printf("    ❌ Error: %v\n", err)
		} else {
			lines := strings.Split(strings.TrimSpace(output), "\n")
			for _, line := range lines {
				if strings.TrimSpace(line) != "" {
					fmt.Printf("    %s\n", line)
				}
			}
		}
		fmt.Println()
	}
}

func checkModemGPS(client *ssh.Client) {
	commands := map[string]string{
		"Modem info":            "gsmctl -i",
		"Modem capabilities":    "gsmctl -A 'AT+CGMM'",
		"GPS support check":     "gsmctl -A 'AT+CGPS?'",
		"GPS power status":      "gsmctl -A 'AT+CGPSPWR?'",
		"Available AT commands": "gsmctl -A 'AT+CLAC' | grep -i gps || echo 'No GPS AT commands'",
		"Modem GPS status":      "gsmctl -A 'AT+CGPSINFO'",
	}

	for name, cmd := range commands {
		fmt.Printf("  %s:\n", name)
		output, err := executeCommand(client, cmd)
		if err != nil {
			fmt.Printf("    ❌ Error: %v\n", err)
		} else {
			// Clean up the output
			output = strings.ReplaceAll(output, "ERROR", "❌ ERROR")
			output = strings.ReplaceAll(output, "OK", "✅ OK")
			lines := strings.Split(strings.TrimSpace(output), "\n")
			for _, line := range lines {
				if strings.TrimSpace(line) != "" {
					fmt.Printf("    %s\n", line)
				}
			}
		}
		fmt.Println()
	}
}

func analyzeTTYReliability(client *ssh.Client) {
	fmt.Println("  Testing /dev/tty GPS device reliability...")

	// Test multiple reads to check consistency
	device := "/dev/ttyUSB1"

	fmt.Printf("  📊 Testing %s reliability (5 quick reads):\n", device)

	for i := 1; i <= 5; i++ {
		start := time.Now()
		cmd := fmt.Sprintf("timeout 3 head -n 3 %s", device)
		output, err := executeCommand(client, cmd)
		duration := time.Since(start)

		fmt.Printf("    Read #%d (%v): ", i, duration)
		if err != nil {
			fmt.Printf("❌ Failed: %v\n", err)
		} else if strings.Contains(output, "$GP") || strings.Contains(output, "$GN") {
			fmt.Printf("✅ NMEA data received\n")
		} else if strings.TrimSpace(output) == "" {
			fmt.Printf("⚠️  No data (device might be busy)\n")
		} else {
			fmt.Printf("❓ Unexpected data: %s\n", strings.ReplaceAll(output, "\n", " "))
		}
	}

	// Test device permissions and access
	fmt.Printf("\n  📋 Device Analysis:\n")
	deviceTests := map[string]string{
		"Device permissions": fmt.Sprintf("ls -la %s", device),
		"Device type":        fmt.Sprintf("file %s", device),
		"Device in use":      fmt.Sprintf("lsof %s 2>/dev/null || echo 'Device not in use'", device),
		"Device speed":       fmt.Sprintf("stty -F %s speed 2>/dev/null || echo 'Cannot get speed'", device),
	}

	for name, cmd := range deviceTests {
		fmt.Printf("    %s: ", name)
		output, err := executeCommand(client, cmd)
		if err != nil {
			fmt.Printf("❌ Error: %v\n", err)
		} else {
			fmt.Printf("%s\n", strings.TrimSpace(output))
		}
	}
}

func provideRecommendations() {
	fmt.Println("  🎯 GPS Integration Strategy Recommendations:")
	fmt.Println()
	fmt.Println("  📊 PRIORITY ORDER (Most Reliable → Least Reliable):")
	fmt.Println("    1. ✅ RutOS API/ubus (if available) - Most reliable, proper abstraction")
	fmt.Println("    2. ✅ gpsd daemon (if configured) - Standard GPS daemon, handles device management")
	fmt.Println("    3. ⚠️  Direct /dev/tty reading - Works but needs extensive error handling")
	fmt.Println("    4. ❌ AT commands - Unreliable, modem-dependent")
	fmt.Println()
	fmt.Println("  🛡️  FAILSAFES FOR /dev/tty METHOD:")
	fmt.Println("    • ✅ Device availability check before reading")
	fmt.Println("    • ✅ Timeout protection (3-5 seconds max)")
	fmt.Println("    • ✅ Multiple device fallback (/dev/ttyUSB0, /dev/ttyUSB1, /dev/ttyUSB2)")
	fmt.Println("    • ✅ NMEA sentence validation")
	fmt.Println("    • ✅ Coordinate sanity checks")
	fmt.Println("    • ✅ Retry logic with exponential backoff")
	fmt.Println("    • ✅ Process isolation (separate goroutine)")
	fmt.Println("    • ✅ Resource cleanup (close file handles)")
	fmt.Println()
	fmt.Println("  ⚡ PERFORMANCE CONSIDERATIONS:")
	fmt.Println("    • Cache GPS data for 30-60 seconds")
	fmt.Println("    • Use non-blocking reads")
	fmt.Println("    • Implement circuit breaker pattern")
	fmt.Println("    • Monitor device health")
	fmt.Println()
	fmt.Println("  🔄 RECOMMENDED IMPLEMENTATION:")
	fmt.Println("    1. Try RutOS API first (ubus/gpsd)")
	fmt.Println("    2. Fallback to /dev/tty with full error handling")
	fmt.Println("    3. Cache successful reads")
	fmt.Println("    4. Implement health monitoring")
	fmt.Println("    5. Graceful degradation on failures")
}
