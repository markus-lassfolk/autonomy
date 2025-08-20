# 🧠 Intelligent Starlink Monitoring System v3.0

**MWAN3-Integrated Predictive Failover with Dynamic Metric Management**

## 🚀 Revolutionary Features

This system represents a complete architectural redesign that abandons legacy constraints to build the optimal
intelligent failover solution for RUTOS:

### ✨ Core Intelligence Features

- **🔍 Automatic MWAN3 Discovery**: Scans UCI configuration to find all managed interfaces
- **🧠 Interface Classification**: Automatically detects cellular, WiFi, ethernet, and satellite connections
- **📊 Dynamic Metric Adjustment**: Intelligent metric modification based on performance and trends
- **📈 Historical Performance Analysis**: Multi-source data analysis for predictive decisions
- **🎯 Predictive Failover**: Prevents user experience issues before they occur
- **⚡ Interface-Specific Testing**: Optimized testing for each connection type

### 🌐 Multi-Interface Support

- **📱 Cellular Modems**: Up to 8 cellular modems (mob1s1a1-mob8s1a1)
- **📡 WiFi Bridges**: Wireless bridges and client connections
- **🔌 Ethernet**: Wired connections and bridged networks
- **🛰️ Satellite**: Starlink and other satellite internet connections

### 🎛️ Intelligent Metric Management

```
Severity Levels:
├── MINOR Issues    → +5 metric adjustment
├── MODERATE Issues → +10 metric adjustment
├── MAJOR Issues    → +20 metric adjustment
├── CRITICAL Issues → +50 metric adjustment
└── DOWN/FAILED     → +100 metric adjustment
```

## 📁 Project Structure

```
autonomy/
├── Starlink-RUTOS-Failover/
│   ├── starlink_monitor_unified-rutos.sh  # Main intelligent monitoring system
│   ├── lib/
│   │   └── rutos-lib.sh                   # RUTOS Library System v2.7.1
│   └── config/
│       └── config.template.sh             # Configuration template
├── test-intelligent-system.sh             # System testing script
└── README-INTELLIGENT.md                  # This documentation
```

## 🔧 Quick Start

### 1. Test the System

```bash
# Run comprehensive system test
./test-intelligent-system.sh

# Test individual components
cd Starlink-RUTOS-Failover
./starlink_monitor_unified-rutos.sh validate
./starlink_monitor_unified-rutos.sh discover
./starlink_monitor_unified-rutos.sh test --debug
```

### 2. Start Monitoring

```bash
# Start daemon in background
./starlink_monitor_unified-rutos.sh start --daemon

# Check status
./starlink_monitor_unified-rutos.sh status

# View live logs
tail -f logs/rutos-lib.log
```

### 3. Monitor and Manage

```bash
# Generate comprehensive report
./starlink_monitor_unified-rutos.sh report

# Run historical analysis
./starlink_monitor_unified-rutos.sh analyze

# Stop when needed
./starlink_monitor_unified-rutos.sh stop
```

## 🎯 Command Reference

### Main Commands

```bash
start                    # Start intelligent monitoring daemon
stop                     # Stop running monitoring daemon
status                   # Show current monitoring status
test                     # Run single monitoring cycle (test mode)
discover                 # Discover and display MWAN3 interfaces
analyze                  # Run historical performance analysis
report                   # Generate comprehensive system report
validate                 # Validate system configuration
help                     # Show detailed help information
```

### Advanced Options

```bash
--daemon                 # Run in daemon mode (background)
--interval=N             # Set monitoring interval in seconds (default: 60)
--quick-interval=N       # Set quick check interval in seconds (default: 30)
--deep-interval=N        # Set deep analysis interval in seconds (default: 300)
--debug                  # Enable debug logging
--dry-run               # Enable dry run mode (no changes)
--log-level=LEVEL       # Set log level (info, debug, trace)
```

## 🧠 How It Works

### 1. **Discovery Phase**

- Scans MWAN3 UCI configuration for managed interfaces
- Discovers interface → member → policy relationships
- Classifies each interface by type (cellular/WiFi/ethernet/satellite)

### 2. **Performance Testing Phase**

- Runs interface-specific connectivity tests
- Measures latency, packet loss, and availability
- Collects cellular signal strength and diagnostics
- Tests WiFi link quality and ethernet connectivity

### 3. **Historical Analysis Phase**

- Analyzes MWAN3 tracking logs for interface history
- Reviews monitor logs for past performance patterns
- Calculates trend direction and performance scores
- Identifies patterns that predict failures

### 4. **Intelligent Decision Phase**

- Combines current performance with historical trends
- Calculates appropriate metric adjustments
- Applies changes to MWAN3 configuration
- Ensures smooth failover before user impact

### 5. **Continuous Monitoring**

- Runs main cycle every 60 seconds (configurable)
- Quick health checks every 30 seconds
- Deep system analysis every 5 minutes
- Comprehensive reporting and logging

## 📊 Performance Thresholds

### Default Thresholds

```bash
# Latency thresholds
LATENCY_WARNING_THRESHOLD=200      # ms
LATENCY_CRITICAL_THRESHOLD=500     # ms

# Packet loss thresholds
PACKET_LOSS_WARNING_THRESHOLD=2    # %
PACKET_LOSS_CRITICAL_THRESHOLD=5   # %

# Historical analysis
HISTORICAL_ANALYSIS_WINDOW=1800    # 30 minutes
TREND_ANALYSIS_SAMPLES=10          # Number of samples

# Safety limits
MAX_METRIC_ADJUSTMENT=50           # Maximum adjustment per cycle
MAX_ADJUSTMENTS_PER_CYCLE=3        # Max interfaces adjusted per cycle
ADJUSTMENT_COOLDOWN=120            # Seconds between adjustments
```

### Customization

```bash
# Set custom thresholds as environment variables
export LATENCY_WARNING_THRESHOLD=150
export PACKET_LOSS_CRITICAL_THRESHOLD=3
export MONITORING_INTERVAL=30

# Or use command-line options
./starlink_monitor_unified-rutos.sh start --interval=30 --debug
```

## 📈 Monitoring and Reporting

### Log Files

```
logs/
├── rutos-lib.log                      # Main system log
├── intelligent_monitoring_report.log  # Monitoring reports
├── deep_analysis_report.log          # Deep system analysis
└── monitoring_state/                 # Runtime state files
```

### Report Format

```
=== INTELLIGENT MONITORING REPORT - 2024-01-15 14:30:45 ===

INTERFACE SUMMARY:
  ✅ wwan0          (cellular/lte) - Metric:  1, Latency:  45ms, Loss:  0%, Issues: 0
  ⚠️ eth0.1         (ethernet/wan) - Metric: 15, Latency: 120ms, Loss:  1%, Issues: 1
  🔌 mob1s1a1       (cellular/lte) - Metric: 50, Latency: 999ms, Loss: 50%, Issues: 3

SYSTEM STATUS: All interfaces monitored and metrics adjusted based on performance
==============================================================
```

## 🔧 System Requirements

### RUTOS Environment

- **Firmware**: RUTOS RUT5_R_00.07.09.7 or compatible
- **Architecture**: armv7l with busybox shell
- **Packages**: MWAN3 package installed and configured
- **UCI Access**: Read/write access to MWAN3 configuration

### Network Interfaces

- **Minimum**: At least one MWAN3-managed interface
- **Recommended**: Multiple interfaces for failover capability
- **Supported Types**: Cellular, WiFi, Ethernet, Satellite

### System Resources

- **Memory**: 64MB RAM minimum for monitoring daemon
- **Storage**: 50MB for logs and state files
- **CPU**: ARM7 compatible processor

## 🛠️ Advanced Configuration

### Environment Variables

```bash
# Monitoring behavior
export MONITORING_INTERVAL=60              # Main cycle interval
export QUICK_CHECK_INTERVAL=30             # Quick check interval
export DEEP_ANALYSIS_INTERVAL=300          # Deep analysis interval

# Performance thresholds
export LATENCY_WARNING_THRESHOLD=200       # Warning latency (ms)
export LATENCY_CRITICAL_THRESHOLD=500      # Critical latency (ms)
export PACKET_LOSS_WARNING_THRESHOLD=2     # Warning packet loss (%)
export PACKET_LOSS_CRITICAL_THRESHOLD=5    # Critical packet loss (%)

# Safety limits
export MAX_METRIC_ADJUSTMENT=50            # Max metric change per cycle
export MAX_ADJUSTMENTS_PER_CYCLE=3         # Max interfaces changed per cycle
export ADJUSTMENT_COOLDOWN=120             # Cooldown between adjustments

# Debug and development
export DEBUG=1                             # Enable debug logging
export DRY_RUN=1                          # Safe mode - no actual changes
export RUTOS_TEST_MODE=1                  # Enable trace logging
```

### Custom Testing Intervals

```bash
# Fast monitoring for testing
./starlink_monitor_unified-rutos.sh start --interval=15 --quick-interval=5

# Conservative monitoring for production
./starlink_monitor_unified-rutos.sh start --interval=120 --deep-interval=600
```

## 🔍 Troubleshooting

### Common Issues

#### 1. No MWAN3 Interfaces Found

```bash
# Check MWAN3 installation
opkg list-installed | grep mwan3

# Check UCI configuration
uci show mwan3

# Validate MWAN3 status
mwan3 status
```

#### 2. Metric Adjustments Not Applied

```bash
# Check UCI write permissions
uci set mwan3.test.test=1 && uci commit mwan3

# Enable debug logging
./starlink_monitor_unified-rutos.sh test --debug

# Check for rate limiting
grep "COOLDOWN" logs/rutos-lib.log
```

#### 3. Performance Detection Issues

```bash
# Test interface connectivity manually
ping -I wwan0 -c 3 8.8.8.8

# Check cellular signal strength
gsmctl -A AT+CSQ

# Review historical data
./starlink_monitor_unified-rutos.sh analyze
```

### Debug Mode

```bash
# Enable comprehensive debugging
export DEBUG=1
export RUTOS_TEST_MODE=1

# Run with debug output
./starlink_monitor_unified-rutos.sh test --debug

# Monitor live debugging
tail -f logs/rutos-lib.log | grep -E "(DEBUG|TRACE)"
```

## 🚀 Integration Examples

### Cron Integration

```bash
# Add to crontab for automatic startup
# Start monitoring every 5 minutes if not running
*/5 * * * * /usr/local/starlink/bin/starlink_monitor_unified-rutos.sh start --daemon >/dev/null 2>&1
```

### Service Integration

```bash
# Create init.d service script
#!/bin/sh /etc/rc.common

START=95
STOP=10

start() {
    /usr/local/starlink/bin/starlink_monitor_unified-rutos.sh start --daemon
}

stop() {
    /usr/local/starlink/bin/starlink_monitor_unified-rutos.sh stop
}
```

### Custom Alerting

```bash
# Integration with external alerting
if ! ./starlink_monitor_unified-rutos.sh status; then
    logger "Starlink monitoring daemon not running"
    # Send notification
fi
```

## 📞 Support and Development

### Getting Help

1. **Run system validation**: `./starlink_monitor_unified-rutos.sh validate`
2. **Check logs**: Review `logs/rutos-lib.log` for detailed information
3. **Test individual components**: Use `discover`, `test`, and `analyze` commands
4. **Enable debug mode**: Add `--debug` to any command for detailed output

### Development Mode

```bash
# Safe testing with no actual changes
export DRY_RUN=1
./starlink_monitor_unified-rutos.sh test --debug

# Trace mode for maximum detail
export RUTOS_TEST_MODE=1
./starlink_monitor_unified-rutos.sh test --debug
```

### Contributing

This system uses the RUTOS Library System v2.7.1 for standardized logging, error handling, and RUTOS compatibility. All
scripts follow POSIX sh standards for busybox compatibility.

---

**📝 Note**: This system represents a complete redesign optimized for RUTOS environments with MWAN3 integration. It
provides intelligent, predictive failover capabilities that prevent user experience issues through proactive monitoring
and dynamic metric management.
