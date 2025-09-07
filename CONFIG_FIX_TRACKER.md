# UCI Configuration Fix Tracker

## 🚨 **Critical Issue Confirmed**
- **614 hardcoded values** found across **90 files**
- **UCI configuration is completely ignored** by all modules
- **Configuration changes have ZERO effect** on daemon behavior

## 📊 **Priority Fix Order**

### **Phase 1: Core Infrastructure (HIGH PRIORITY)**
| Module | Hardcoded Values | UCI Config Fields | Status | Priority |
|--------|------------------|-------------------|---------|----------|
| `gps_manager.c` | 18 | `gps_update_interval`, `gps_timeout` | 🔴 Not Started | **CRITICAL** |
| `network_failover.c` | 14 | `network_check_interval`, `failover_timeout`, `auto_failover` | 🔴 Not Started | **CRITICAL** |
| `starlink_tracker.c` | 6 | `starlink_check_interval`, `starlink_health_monitoring` | 🔴 Not Started | **CRITICAL** |
| `cellular_collector.c` | 10 | `collection_interval`, `timeout_seconds` | 🔴 Not Started | **HIGH** |

### **Phase 2: GPS System (HIGH PRIORITY)**
| Module | Hardcoded Values | UCI Config Fields | Status | Priority |
|--------|------------------|-------------------|---------|----------|
| `gps_comprehensive.c` | 2 | `gps_timeout`, `min_gps_accuracy` | 🔴 Not Started | **HIGH** |
| `gps_terrain.c` | 4 | `gps_terrain.update_interval` | 🔴 Not Started | **HIGH** |
| `gps_health.c` | 15 | `gps_update_interval`, `health_check_interval` | 🔴 Not Started | **HIGH** |
| `gps_fusion.c` | 6 | `gps_fusion`, `fusion_update_interval` | 🔴 Not Started | **MEDIUM** |

### **Phase 3: Network System (MEDIUM PRIORITY)**
| Module | Hardcoded Values | UCI Config Fields | Status | Priority |
|--------|------------------|-------------------|---------|----------|
| `network_discovery.c` | 10 | `network_discovery.discovery_interval` | 🔴 Not Started | **MEDIUM** |
| `network_collector.c` | 5 | `network_collector.collection_interval` | 🔴 Not Started | **MEDIUM** |
| `network_controller.c` | 6 | `network_controller.use_mwan3` | 🔴 Not Started | **MEDIUM** |

### **Phase 4: Starlink System (MEDIUM PRIORITY)**
| Module | Hardcoded Values | UCI Config Fields | Status | Priority |
|--------|------------------|-------------------|---------|----------|
| `starlink_snow_detection.c` | 11 | `snow_detection.*` | 🔴 Not Started | **MEDIUM** |
| `starlink_obstruction.c` | 14 | `starlink_obstruction.*` | 🔴 Not Started | **MEDIUM** |
| `starlink_comprehensive.c` | 7 | `starlink_comprehensive.*` | 🔴 Not Started | **MEDIUM** |

### **Phase 5: System Monitoring (LOW PRIORITY)**
| Module | Hardcoded Values | UCI Config Fields | Status | Priority |
|--------|------------------|-------------------|---------|----------|
| `telemetry_comprehensive.c` | 1 | `telemetry_comprehensive.*` | 🔴 Not Started | **LOW** |
| `predictive_engine.c` | 4 | `predictive_engine.*` | 🔴 Not Started | **LOW** |
| `system_management.c` | TBD | `system.*` | 🔴 Not Started | **LOW** |

## 🔧 **Implementation Strategy**

### **Step 1: Create Configuration Propagation System**
```c
// Add to each module header
int module_update_config(const autonomy_config_t *config);
int module_get_config(module_config_t *config);
```

### **Step 2: Update Main Daemon**
```c
// Add to autonomy-daemon.c
int update_all_module_configs(const autonomy_config_t *config);
int reload_configuration(void);
```

### **Step 3: Fix Each Module Systematically**
1. **Remove hardcoded constants**
2. **Add configuration update function**
3. **Use g_config values**
4. **Test configuration changes take effect**
5. **Add UBUS method for runtime config updates**

## ✅ **Verification Checklist**

### **For Each Module:**
- [ ] Remove all hardcoded constants
- [ ] Add `module_update_config()` function
- [ ] Use `g_config` values instead of hardcoded
- [ ] Test configuration changes take effect immediately
- [ ] Add UBUS method to update config at runtime
- [ ] Document configuration dependencies
- [ ] Add configuration validation

### **System-wide Verification:**
- [ ] All 614 hardcoded values replaced
- [ ] Configuration changes take effect immediately
- [ ] No hardcoded values remain
- [ ] Performance impact is minimal
- [ ] Configuration validation works
- [ ] Documentation is updated
- [ ] Automated tests verify configuration usage

## 🎯 **Success Criteria**

1. **Immediate Effect**: Changing UCI config immediately affects daemon behavior
2. **Complete Coverage**: All 614 hardcoded values replaced with UCI config
3. **Runtime Updates**: Can update configuration without restart
4. **Performance**: No significant performance degradation
5. **Testing**: Automated tests verify configuration usage
6. **Documentation**: Complete documentation of configuration usage

## 📈 **Progress Tracking**

### **Current Status: 0% Complete**
- **Files Fixed**: 0/90
- **Hardcoded Values Fixed**: 0/614
- **Modules Using UCI Config**: 0/90

### **Next Actions:**
1. **Start with gps_manager.c** (18 hardcoded values)
2. **Fix network_failover.c** (14 hardcoded values)
3. **Continue systematically** through priority list
4. **Test each fix** before moving to next module

---

**This is a CRITICAL architectural fix that will make the UCI configuration system actually functional.**
