# PREDICTIVE FAILOVER CONFIGURATION ADDED TO UNIFIED TEMPLATE

## 🎯 **Configuration Variables Added**

Successfully added comprehensive predictive failover configuration to the unified config template at:
**Location**: `config/config.unified.template.sh` 

### **Added Variables**

```bash
# --- Predictive Failover Settings ---

# Enable comprehensive health monitoring
export ENABLE_HEALTH_MONITORING="true"

# Reboot warning window (300 seconds = 5 minutes)
export REBOOT_WARNING_SECONDS="300"

# Enable predictive reboot monitoring
export ENABLE_PREDICTIVE_REBOOT_MONITORING="true"

# Enhanced failover decision logging
export ENABLE_ENHANCED_FAILOVER_LOGGING="false"

# Minimum hardware health score threshold
export HARDWARE_HEALTH_THRESHOLD="75"
```

### **📍 Integration Location**

- **Section**: "5. ADVANCED SYSTEM CONFIGURATION"
- **Subsection**: "Predictive Failover Settings" (new subsection)
- **Position**: After "Advanced Data Management", before "Data Limits and Thresholds"

### **📖 Documentation Features**

Each variable includes comprehensive documentation:
- **Purpose explanation**: What the variable controls
- **Benefits description**: Why you would enable/configure it
- **Impact assessment**: Resource usage and system effects
- **Calculation examples**: For time-based settings (seconds to minutes)
- **Recommended values**: Production-ready defaults with rationale
- **Use case guidance**: When to adjust settings

### **⚙️ Default Values**

- **Health Monitoring**: Enabled (provides predictive capabilities)
- **Reboot Warning**: 5 minutes (balanced prediction vs cellular usage)
- **Predictive Monitoring**: Enabled (core functionality)
- **Enhanced Logging**: Disabled (production default, enable for troubleshooting)
- **Hardware Threshold**: 75% (balanced reliability threshold)

### **🔗 Configuration Usage**

The monitoring scripts automatically use these variables when loaded:

```bash
# Scripts automatically load configuration
. /etc/starlink-config/config.sh

# Variables are available to the library functions
check_starlink_health       # Uses ENABLE_HEALTH_MONITORING
should_failover_for_reboot  # Uses REBOOT_WARNING_SECONDS
get_reboot_status          # Uses ENABLE_PREDICTIVE_REBOOT_MONITORING
```

### **📋 Configuration Examples**

**Conservative (Early Failover)**:
```bash
export REBOOT_WARNING_SECONDS="600"    # 10 minutes
export HARDWARE_HEALTH_THRESHOLD="80"  # Higher quality requirement
```

**Aggressive (Minimal Cellular Usage)**:
```bash
export REBOOT_WARNING_SECONDS="60"     # 1 minute
export HARDWARE_HEALTH_THRESHOLD="60"  # More tolerance for issues
```

**Troubleshooting (Detailed Logging)**:
```bash
export ENABLE_ENHANCED_FAILOVER_LOGGING="true"  # Detailed decision logs
```

## ✅ **Ready for Production**

The unified configuration template now provides complete predictive failover configuration with:
- Production-ready defaults
- Comprehensive documentation  
- Clear guidance for customization
- Integration with existing monitoring architecture

Users can now enable and configure predictive failover through the standard configuration process without needing to manually set environment variables.
