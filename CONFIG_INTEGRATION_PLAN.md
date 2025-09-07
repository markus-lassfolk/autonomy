# UCI Configuration Integration Plan

## 🚨 **Critical Issue Identified**
The autonomy daemon loads UCI configuration into `g_config` but modules use hardcoded values instead of the loaded configuration. This means configuration changes have NO EFFECT on actual daemon behavior.

## 📋 **Systematic Approach**

### Phase 1: Audit and Inventory
1. **Identify all modules with hardcoded values**
2. **Map UCI config fields to module usage**
3. **Create tracking spreadsheet**
4. **Identify configuration propagation points**

### Phase 2: Architecture Design
1. **Design configuration propagation system**
2. **Create module configuration update functions**
3. **Implement configuration change notifications**
4. **Add configuration validation**

### Phase 3: Implementation
1. **Update each module systematically**
2. **Add configuration update functions**
3. **Test configuration changes take effect**
4. **Verify no hardcoded values remain**

### Phase 4: Verification
1. **Create automated tests**
2. **Manual verification checklist**
3. **Documentation updates**
4. **Performance impact assessment**

## 🔍 **Module Audit Results**

### GPS Modules
- **gps_manager.c**: Uses hardcoded `GPS_UPDATE_INTERVAL=5`, `GPS_SOURCE_TIMEOUT=60`
- **gps_comprehensive.c**: Uses local config, not global `g_config`
- **gps_terrain.c**: Uses hardcoded `TERRAIN_UPDATE_INTERVAL`
- **gps_opencellid_*.c**: Uses hardcoded timeouts and intervals

### Network Modules
- **network_failover.c**: Uses hardcoded `DEFAULT_*` constants
- **network_discovery.c**: Likely uses hardcoded intervals
- **network_collector.c**: Likely uses hardcoded timeouts
- **cellular_collector.c**: Uses hardcoded `collection_interval=30`

### Starlink Modules
- **starlink_tracker.c**: Likely uses hardcoded check intervals
- **starlink_snow_detection.c**: Uses hardcoded thresholds
- **starlink_comprehensive.c**: Uses local config

### System Modules
- **telemetry_comprehensive.c**: Uses local config
- **predictive_engine.c**: Uses hardcoded intervals
- **system_management.c**: Likely uses hardcoded values

## 🎯 **Configuration Mapping**

| UCI Config Field | Current Usage | Modules Affected | Status |
|------------------|---------------|------------------|---------|
| `gps_update_interval` | Hardcoded 5s | gps_manager.c | ❌ Not Used |
| `gps_timeout` | Hardcoded 60s | gps_manager.c | ❌ Not Used |
| `min_gps_accuracy` | Hardcoded 10.0m | Multiple GPS modules | ❌ Not Used |
| `network_check_interval` | Hardcoded 30s | network_failover.c | ❌ Not Used |
| `failover_timeout` | Hardcoded 60s | network_failover.c | ❌ Not Used |
| `auto_failover` | Hardcoded true | network_failover.c | ❌ Not Used |
| `starlink_check_interval` | Hardcoded 30s | starlink_tracker.c | ❌ Not Used |
| `starlink_health_monitoring` | Hardcoded true | starlink modules | ❌ Not Used |
| `system_check_interval` | Hardcoded 60s | system modules | ❌ Not Used |
| `snow_detection_*` | Hardcoded thresholds | snow detection | ❌ Not Used |

## 🔧 **Implementation Strategy**

### 1. Configuration Propagation System
```c
// Add to each module
int module_update_config(const autonomy_config_t *config);
int module_get_config(module_config_t *config);
```

### 2. Global Configuration Update
```c
// Add to main daemon
int update_all_module_configs(const autonomy_config_t *config);
```

### 3. Configuration Change Notification
```c
// Add configuration reload capability
int reload_configuration(void);
```

## ✅ **Verification Checklist**

### For Each Module:
- [ ] Remove all hardcoded constants
- [ ] Add configuration update function
- [ ] Use `g_config` values instead of hardcoded
- [ ] Test configuration changes take effect
- [ ] Add UBUS method to update config
- [ ] Document configuration dependencies

### System-wide:
- [ ] All modules use UCI configuration
- [ ] Configuration changes are immediate
- [ ] No hardcoded values remain
- [ ] Performance impact is minimal
- [ ] Configuration validation works
- [ ] Documentation is updated

## 🚀 **Success Criteria**

1. **Configuration Changes Take Effect**: Modifying UCI config immediately affects daemon behavior
2. **No Hardcoded Values**: All timeouts, intervals, and thresholds come from UCI
3. **Runtime Configuration Updates**: Can update configuration without restart
4. **Comprehensive Coverage**: All modules use UCI configuration
5. **Performance Maintained**: No significant performance degradation
6. **Fully Tested**: Automated tests verify configuration usage

## 📊 **Progress Tracking**

| Module | Hardcoded Values Found | UCI Integration | Testing | Status |
|--------|----------------------|-----------------|---------|---------|
| gps_manager | 2 | ❌ | ❌ | 🔴 Not Started |
| network_failover | 5 | ❌ | ❌ | 🔴 Not Started |
| starlink_tracker | TBD | ❌ | ❌ | 🔴 Not Started |
| cellular_collector | 2 | ❌ | ❌ | 🔴 Not Started |
| telemetry_comprehensive | TBD | ❌ | ❌ | 🔴 Not Started |

## 🎯 **Next Steps**

1. **Complete module audit** - Find ALL hardcoded values
2. **Create configuration propagation system**
3. **Update modules one by one** with tracking
4. **Test each module** after update
5. **Create verification tests**
6. **Document final state**

---

**This is a CRITICAL architectural fix that will make the UCI configuration system actually functional.**
