# 🔧 UCI Configuration Restructure Proposal

## 📊 Current vs Proposed Structure

### ❌ **Current Issues**
- **118 lines** in single `main` section
- **Mixed concerns** (core, ML, API, notifications all together)
- **Hard to navigate** for users
- **Difficult to maintain** as features grow
- **Poor organization** makes troubleshooting harder

### ✅ **Proposed Benefits**

#### **1. Logical Grouping**
```uci
# Before: Everything in 'main'
config autonomy 'main'
    option enable '1'
    option pushover_token ''
    option starlink_api_host '192.168.100.1'
    option ml_enabled '1'
    option metered_mode_enabled '0'
    # ... 113 more lines

# After: Organized sections
config autonomy 'main'          # Core settings only
config notifications 'pushover' # Notification settings
config starlink 'api'          # Starlink API settings
config ml 'settings'            # ML configuration
config metered 'settings'       # Metered mode settings
```

#### **2. Easier Configuration**
- **Find settings faster**: `config notifications 'pushover'` vs searching through 118 lines
- **Logical grouping**: Related settings are together
- **Better documentation**: Each section can have focused comments
- **Reduced errors**: Smaller sections are easier to validate

#### **3. Maintainability**
- **Modular**: Add new features without bloating main section
- **Extensible**: Easy to add new subsections
- **Readable**: Clear separation of concerns
- **Debuggable**: Issues isolated to specific sections

## 🏗️ Proposed Structure

### **Core Sections**
1. **`autonomy 'main'`** - Essential daemon settings (15 options)
2. **`thresholds 'failover'`** - Failover thresholds (3 options)
3. **`thresholds 'restore'`** - Restore thresholds (3 options)
4. **`starlink 'api'`** - Starlink API settings (5 options)
5. **`ml 'settings'`** - Machine learning (4 options)
6. **`monitoring 'endpoints'`** - Metrics/health endpoints (4 options)
7. **`monitoring 'mqtt'`** - MQTT telemetry (3 options)
8. **`notifications 'pushover'`** - Pushover credentials (4 options)
9. **`notifications 'settings'`** - Notification behavior (6 options)
10. **`notifications 'events'`** - What to notify (7 options)
11. **`notifications 'priorities'`** - Priority levels (7 options)
12. **`metered 'settings'`** - Metered mode core (4 options)
13. **`metered 'thresholds'`** - Metered thresholds (3 options)

### **Size Comparison**
- **Before**: 1 section with 118 options
- **After**: 13 sections with 5-15 options each
- **Largest section**: 15 options (main) vs 118 options

## 🔄 Migration Strategy

### **Option 1: Gradual Migration (Recommended)**
1. **Keep backward compatibility** - support both formats
2. **Add new structured parsing** alongside existing
3. **Deprecation warnings** for old format
4. **Migration tool** to convert old → new format

### **Option 2: Breaking Change**
1. **Immediate switch** to new format
2. **Migration script** provided
3. **Clear upgrade documentation**
4. **Version bump** to indicate breaking change

## 💻 Implementation Requirements

### **UCI Parser Updates**
```go
// New parsing functions needed
func parseThresholds(section string) (*ThresholdConfig, error)
func parseStarlink(section string) (*StarlinkConfig, error)
func parseML(section string) (*MLConfig, error)
func parseMonitoring(section string) (*MonitoringConfig, error)
func parseNotifications(section string) (*NotificationConfig, error)
func parseMetered(section string) (*MeteredConfig, error)
```

### **Config Struct Updates**
```go
type Config struct {
    // Core settings (reduced)
    Enable      bool
    UseMWAN3    bool
    LogLevel    string
    // ... core only
    
    // Structured subsections
    Thresholds    ThresholdConfig
    Starlink      StarlinkConfig
    ML            MLConfig
    Monitoring    MonitoringConfig
    Notifications NotificationConfig
    Metered       MeteredConfig
}
```

### **Backward Compatibility**
```go
func LoadConfig() (*Config, error) {
    // Try new structured format first
    if config, err := loadStructuredConfig(); err == nil {
        return config, nil
    }
    
    // Fall back to legacy format with deprecation warning
    logger.Warn("Using legacy configuration format - please migrate to structured format")
    return loadLegacyConfig()
}
```

## 📋 User Experience

### **Before (Overwhelming)**
```bash
# User wants to configure notifications
uci show autonomy.main | grep -E "pushover|notify|priority"
# Returns 25+ lines mixed with other settings
```

### **After (Intuitive)**
```bash
# User wants to configure notifications
uci show autonomy.@notifications
# Shows only notification-related settings, clearly organized
```

### **Configuration Examples**

#### **Enable Pushover Notifications**
```bash
# Before: Mixed with 117 other options
uci set autonomy.main.pushover_enabled='1'
uci set autonomy.main.pushover_token='your_token'
uci set autonomy.main.pushover_user='your_user'

# After: Clear and focused
uci set autonomy.@notifications[0].pushover.enabled='1'
uci set autonomy.@notifications[0].pushover.token='your_token'
uci set autonomy.@notifications[0].pushover.user='your_user'
```

#### **Adjust Failover Thresholds**
```bash
# Before: Search through main section
uci set autonomy.main.fail_threshold_loss='3'
uci set autonomy.main.fail_threshold_latency='1000'

# After: Obvious location
uci set autonomy.@thresholds[0].failover.loss='3'
uci set autonomy.@thresholds[0].failover.latency='1000'
```

## 🎯 Recommendation

**Implement Option 1 (Gradual Migration)** with:

1. **Phase 1**: Add structured parsing alongside existing
2. **Phase 2**: Default to structured format for new installations
3. **Phase 3**: Deprecation warnings for legacy format
4. **Phase 4**: Remove legacy support in next major version

This approach:
- ✅ **Maintains compatibility** for existing users
- ✅ **Improves experience** for new users
- ✅ **Allows testing** of new format
- ✅ **Provides migration path**

Would you like me to implement this restructured approach?
