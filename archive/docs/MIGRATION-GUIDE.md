# 🔄 Migration Guide: Cron to Daemon-Based Monitoring

**Upgrading from Legacy Cron-Based to Intelligent Daemon-Based Monitoring**

## 🚀 Why Upgrade to Daemon Mode?

### **Daemon Advantages:**
✅ **Persistent Intelligence**: Maintains historical data and trends between cycles  
✅ **Adaptive Timing**: Adjusts intervals dynamically based on network conditions  
✅ **Immediate Response**: No waiting for next cron execution during issues  
✅ **Resource Efficiency**: Single persistent process vs spawning new processes  
✅ **Better State Management**: Maintains connection state and performance history  
✅ **Intelligent Decisions**: Combines current + historical data for predictive failover  

### **Cron Limitations:**
❌ Each execution starts fresh (no memory of previous cycles)  
❌ Fixed intervals can't adapt to changing conditions  
❌ Process overhead from repeated script startup  
❌ Lost state between executions  
❌ No correlation between monitoring cycles  

## 📊 Monitoring Mode Comparison

| Feature | Cron Mode | Daemon Mode | Hybrid Mode |
|---------|-----------|-------------|-------------|
| **Intelligent Analysis** | ❌ Basic | ✅ Full | ✅ Full |
| **Historical Trends** | ❌ None | ✅ Complete | ✅ Complete |
| **Predictive Failover** | ❌ Reactive | ✅ Proactive | ✅ Proactive |
| **Resource Usage** | 🟡 Medium | ✅ Low | 🟡 Medium |
| **Adaptation** | ❌ Fixed | ✅ Dynamic | ✅ Dynamic |
| **State Persistence** | ❌ None | ✅ Full | ✅ Full |
| **Legacy Compatibility** | ✅ Full | 🟡 Limited | ✅ Full |

## 🔧 Migration Options

### Option 1: Clean Migration (Recommended)
```bash
# Deploy new intelligent system
./deploy-starlink-solution-v3-rutos.sh

# Choose "1) Daemon" mode during setup
# This will:
# - Remove legacy cron jobs
# - Install intelligent daemon
# - Enable autostart at boot
# - Start monitoring immediately
```

### Option 2: Hybrid Migration (Conservative)
```bash
# Deploy with hybrid mode
./deploy-starlink-solution-v3-rutos.sh

# Choose "2) Hybrid" mode during setup
# This keeps:
# - Intelligent daemon for main monitoring
# - Essential cron jobs for maintenance
# - Full backward compatibility
```

### Option 3: Manual Migration
```bash
# 1. Stop existing cron-based monitoring
crontab -l | grep -v starlink | crontab -

# 2. Download new intelligent system
curl -L https://raw.githubusercontent.com/markus-lassfolk/autonomy/main/Starlink-RUTOS-Failover/starlink_monitor_unified-rutos.sh > /root/starlink_monitor_unified-rutos.sh
chmod +x /root/starlink_monitor_unified-rutos.sh

# 3. Test the system
/root/starlink_monitor_unified-rutos.sh validate
/root/starlink_monitor_unified-rutos.sh discover
/root/starlink_monitor_unified-rutos.sh test --debug

# 4. Start daemon manually
/root/starlink_monitor_unified-rutos.sh start --daemon

# 5. Check status
/root/starlink_monitor_unified-rutos.sh status
```

## 📋 Pre-Migration Checklist

### **System Requirements**
- [ ] RUTOS firmware with MWAN3 package installed
- [ ] At least one MWAN3-managed interface configured
- [ ] UCI configuration access (test: `uci show mwan3`)
- [ ] 64MB+ RAM available for daemon process
- [ ] 50MB+ storage for logs and state files

### **Configuration Backup**
```bash
# Backup current configuration
mkdir -p /root/backup-$(date +%Y%m%d)
cp /root/config.sh /root/backup-$(date +%Y%m%d)/
crontab -l > /root/backup-$(date +%Y%m%d)/crontab-backup
```

### **Current System Assessment**
```bash
# Check current monitoring
crontab -l | grep starlink

# Check MWAN3 status
mwan3 status

# Check interface configuration
uci show mwan3 | grep interface
```

## 🔄 Migration Process

### Step 1: Assessment
```bash
# Check if MWAN3 is properly configured
if ! command -v mwan3 >/dev/null 2>&1; then
    echo "❌ MWAN3 not found - install with: opkg install mwan3"
    exit 1
fi

# Check MWAN3 interfaces
interface_count=$(uci show mwan3 | grep "interface=" | wc -l)
if [ "$interface_count" -eq 0 ]; then
    echo "⚠️ No MWAN3 interfaces configured"
    echo "Configure MWAN3 first: https://openwrt.org/docs/guide-user/network/wan/multiwan/mwan3"
fi
```

### Step 2: Download and Test
```bash
# Download latest intelligent monitoring system
cd /root
curl -L -o starlink_monitor_unified-rutos.sh \
    https://raw.githubusercontent.com/markus-lassfolk/autonomy/main/Starlink-RUTOS-Failover/starlink_monitor_unified-rutos.sh
chmod +x starlink_monitor_unified-rutos.sh

# Test system compatibility
./starlink_monitor_unified-rutos.sh validate
./starlink_monitor_unified-rutos.sh discover
```

### Step 3: Migration Execution
```bash
# Option A: Automated migration with deployment script
./deploy-starlink-solution-v3-rutos.sh

# Option B: Manual daemon setup
# Remove legacy cron jobs
(crontab -l 2>/dev/null | grep -v starlink || true) | crontab -

# Start intelligent daemon
./starlink_monitor_unified-rutos.sh start --daemon
```

### Step 4: Verification
```bash
# Check daemon status
./starlink_monitor_unified-rutos.sh status

# Monitor logs
tail -f /root/logs/rutos-lib.log

# Check MWAN3 integration
./starlink_monitor_unified-rutos.sh discover

# Generate test report
./starlink_monitor_unified-rutos.sh report
```

## 🔧 Configuration Updates

### **Environment Variables (New)**
```bash
# Add to /root/config.sh or environment
export MONITORING_INTERVAL=60              # Main cycle interval
export QUICK_CHECK_INTERVAL=30             # Quick health checks
export DEEP_ANALYSIS_INTERVAL=300          # Deep analysis cycle
export LATENCY_WARNING_THRESHOLD=200       # Warning latency (ms)
export LATENCY_CRITICAL_THRESHOLD=500      # Critical latency (ms)
export MAX_METRIC_ADJUSTMENT=50            # Max metric change per cycle
```

### **Legacy Configuration Compatibility**
The intelligent system maintains compatibility with existing configuration variables:
- `STARLINK_IP`, `STARLINK_PORT`
- `MWAN_IFACE`, `MWAN_MEMBER`
- `LATENCY_THRESHOLD`, `PACKET_LOSS_THRESHOLD`
- `METRIC_GOOD`, `METRIC_BAD`

## 🚨 Troubleshooting Migration Issues

### **Issue 1: MWAN3 Not Found**
```bash
# Solution: Install MWAN3
opkg update
opkg install mwan3

# Configure basic MWAN3 setup
# See: https://openwrt.org/docs/guide-user/network/wan/multiwan/mwan3
```

### **Issue 2: UCI Configuration Access**
```bash
# Test UCI access
uci show mwan3

# If permission denied:
chmod 644 /etc/config/mwan3
```

### **Issue 3: Daemon Won't Start**
```bash
# Check script permissions
ls -la /root/starlink_monitor_unified-rutos.sh

# Check dependencies
./starlink_monitor_unified-rutos.sh validate

# Check system resources
free -m
df -h
```

### **Issue 4: Legacy Cron Jobs Interfering**
```bash
# Remove all starlink-related cron jobs
crontab -l | grep -v starlink | crontab -

# Restart cron service
/etc/init.d/cron restart
```

### **Issue 5: Performance Issues**
```bash
# Check daemon resource usage
ps aux | grep starlink

# Check log size
du -sh /root/logs/

# Reduce monitoring frequency if needed
export MONITORING_INTERVAL=120
export QUICK_CHECK_INTERVAL=60
```

## 📊 Post-Migration Monitoring

### **System Health Checks**
```bash
# Daily health check
./starlink_monitor_unified-rutos.sh status

# Weekly comprehensive analysis
./starlink_monitor_unified-rutos.sh analyze

# Monthly system report
./starlink_monitor_unified-rutos.sh report
```

### **Performance Verification**
```bash
# Check daemon uptime and resource usage
ps -eo pid,ppid,cmd,%mem,%cpu,etime | grep starlink

# Monitor decision accuracy
grep "DECISION:" /root/logs/rutos-lib.log | tail -10

# Check metric adjustments
grep "METRIC ADJUSTMENT:" /root/logs/rutos-lib.log | tail -10
```

### **Rollback Procedure (If Needed)**
```bash
# Stop intelligent daemon
./starlink_monitor_unified-rutos.sh stop

# Restore legacy cron monitoring
(
    echo "# Legacy Starlink monitoring"
    echo "*/5 * * * * /root/starlink_monitor_legacy.sh"
) | crontab -

# Restart cron
/etc/init.d/cron restart
```

## 🎯 Migration Success Metrics

### **Before Migration (Cron-Based)**
- ❌ Each monitoring cycle starts fresh
- ❌ No correlation between measurements
- ❌ Fixed 5-minute intervals regardless of conditions
- ❌ Reactive failover after user impact

### **After Migration (Daemon-Based)**
- ✅ Persistent historical analysis and trend tracking
- ✅ Intelligent correlation between past and current performance
- ✅ Adaptive intervals based on network conditions
- ✅ Predictive failover before user experience issues
- ✅ Dynamic metric adjustment based on performance patterns
- ✅ Multi-interface support with automatic discovery

## 📞 Support and Next Steps

### **Getting Help**
1. **System Validation**: `./starlink_monitor_unified-rutos.sh validate`
2. **Debug Mode**: `./starlink_monitor_unified-rutos.sh test --debug`
3. **Interface Discovery**: `./starlink_monitor_unified-rutos.sh discover`
4. **Log Analysis**: `tail -f /root/logs/rutos-lib.log`

### **Advanced Configuration**
Once migration is complete, explore advanced features:
- Multi-cellular modem support (up to 8 modems)
- Custom performance thresholds
- Historical trend analysis
- Predictive failure detection

---

**📝 Note**: The intelligent daemon-based system represents a fundamental improvement in monitoring architecture, providing the foundation for true predictive failover and multi-interface intelligence that simply wasn't possible with cron-based approaches.
