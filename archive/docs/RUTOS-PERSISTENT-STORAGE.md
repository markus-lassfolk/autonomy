# 🔧 RUTOS Persistent Storage Guide

**Firmware Upgrade Survival for Intelligent Starlink Monitoring**

## 🚨 Critical RUTOS Limitation

**WARNING**: On RUTOS devices, `/root` is **NOT persistent** across firmware upgrades!

### ❌ What Gets Wiped During Firmware Upgrades:

- `/root/` - Entire root user directory
- `/tmp/` - Temporary files
- `/var/` - Variable data (except some system configs)
- Any scripts, configurations, or data stored in non-persistent locations

### ✅ What Survives Firmware Upgrades:

- `/opt/` - Optional software packages (persistent)
- `/mnt/` - Mount points (usually persistent)
- `/etc/config/` - UCI configuration files (backed up/restored)
- `/etc/init.d/` - System service scripts (usually persistent)

## 🏗️ New Persistent Storage Architecture

The **Intelligent Starlink Monitoring System v3.0** now uses a **persistent storage architecture** designed specifically
for RUTOS firmware upgrade survival:

### **Persistent Directory Structure:**

```
/usr/local/starlink/                          # Main installation (PERSISTENT)
├── bin/                                # Executable scripts (PERSISTENT)
│   ├── starlink_monitor_unified-rutos.sh
│   └── recover-after-firmware-upgrade.sh
├── config/                             # Configuration files (PERSISTENT)
│   └── config.sh
├── logs/                               # Log files (PERSISTENT)
│   ├── rutos-lib.log
│   ├── intelligent_monitoring_report.log
│   └── deep_analysis_report.log
├── state/                              # Runtime state (PERSISTENT)
│   └── monitoring_state/
├── lib/                                # Library files (PERSISTENT)
│   └── rutos-lib.sh
├── templates/                          # Service templates (PERSISTENT)
│   └── starlink-monitor.init
└── backup-YYYYMMDD-HHMMSS/            # Backups (PERSISTENT)
```

### **Convenience Symlinks (Recreation Required):**

```
/root/starlink_monitor_unified-rutos.sh -> /usr/local/starlink/bin/starlink_monitor_unified-rutos.sh
/root/config.sh -> /etc/starlink-config/config.sh
```

## 🔄 Firmware Upgrade Recovery Process

### **Automatic Recovery (Recommended)**

The deployment script creates an automatic recovery script:

```bash
# Run after firmware upgrade to restore full functionality
/usr/local/starlink/bin/recover-after-firmware-upgrade.sh
```

### **What the Recovery Script Does:**

1. ✅ Verifies persistent storage integrity
2. ✅ Recreates convenience symlinks in `/root/`
3. ✅ Restores daemon service to `/etc/init.d/`
4. ✅ Validates MWAN3 availability
5. ✅ Tests system functionality
6. ✅ Restarts monitoring daemon if configured

### **Manual Recovery Steps:**

If automatic recovery fails, manually restore:

```bash
# 1. Verify persistent storage
ls -la /usr/local/starlink/

# 2. Recreate symlinks
ln -sf /usr/local/starlink/bin/starlink_monitor_unified-rutos.sh /root/starlink_monitor_unified-rutos.sh
ln -sf /etc/starlink-config/config.sh /root/config.sh

# 3. Restore daemon service
cp /usr/local/starlink/templates/starlink-monitor.init /etc/init.d/starlink-monitor
chmod +x /etc/init.d/starlink-monitor

# 4. Restart monitoring
/etc/init.d/starlink-monitor start
```

## 📊 Comparison: Before vs After

### **❌ Legacy Storage (Pre-v3.0) - FIRMWARE UPGRADE VULNERABLE:**

```
/root/starlink_monitor_unified-rutos.sh    # WIPED during upgrade
/root/config.sh                            # WIPED during upgrade
/root/logs/                                 # WIPED during upgrade
```

**Result**: Complete system loss after firmware upgrade, requiring full reinstallation.

### **✅ Persistent Storage (v3.0+) - FIRMWARE UPGRADE RESILIENT:**

```
/usr/local/starlink/                              # SURVIVES firmware upgrade
├── bin/starlink_monitor_unified-rutos.sh  # SURVIVES firmware upgrade
├── config/config.sh                       # SURVIVES firmware upgrade
├── logs/                                   # SURVIVES firmware upgrade
└── templates/                              # SURVIVES firmware upgrade
```

**Result**: Only symlinks and service links need recreation (automatic via recovery script).

## 🔧 Migration from Legacy Storage

### **Automatic Migration:**

The new deployment script automatically handles migration:

```bash
# Deploy v3.0 with persistent storage
./deploy-starlink-solution-v3-rutos.sh

# Automatically:
# 1. Creates persistent directory structure
# 2. Moves existing files to persistent storage
# 3. Creates symlinks for backward compatibility
# 4. Sets up recovery mechanisms
```

### **Manual Migration:**

If upgrading manually:

```bash
# 1. Create persistent directories
mkdir -p /usr/local/starlink/{bin,config,logs,state,lib,templates}

# 2. Move existing files
mv /root/starlink_monitor_unified-rutos.sh /usr/local/starlink/bin/
mv /root/config.sh /etc/starlink-config/

# 3. Create symlinks
ln -sf /usr/local/starlink/bin/starlink_monitor_unified-rutos.sh /root/starlink_monitor_unified-rutos.sh
ln -sf /etc/starlink-config/config.sh /root/config.sh

# 4. Update configuration to use persistent paths
sed -i 's|LOG_DIR="/root/logs"|LOG_DIR="/usr/local/starlink/logs"|g' /etc/starlink-config/config.sh
```

## 🛡️ Firmware Upgrade Best Practices

### **Before Firmware Upgrade:**

1. **Verify persistent storage**: `ls -la /usr/local/starlink/`
2. **Backup configuration**: `cp /etc/starlink-config/config.sh /usr/local/starlink/backup-$(date +%Y%m%d)/`
3. **Note current status**: `/usr/local/starlink/bin/starlink_monitor_unified-rutos.sh status`

### **After Firmware Upgrade:**

1. **Run recovery script**: `/usr/local/starlink/bin/recover-after-firmware-upgrade.sh`
2. **Verify functionality**: `/usr/local/starlink/bin/starlink_monitor_unified-rutos.sh status`
3. **Check logs**: `tail -f /usr/local/starlink/logs/rutos-lib.log`

### **If Recovery Fails:**

```bash
# Check if MWAN3 was removed during upgrade
opkg list-installed | grep mwan3

# Reinstall MWAN3 if missing
opkg update && opkg install mwan3

# Reconfigure MWAN3 if needed
# (Your MWAN3 configuration should be preserved in /etc/config/mwan3)

# Test system manually
/usr/local/starlink/bin/starlink_monitor_unified-rutos.sh validate
/usr/local/starlink/bin/starlink_monitor_unified-rutos.sh discover
```

## 📋 Verification Checklist

### **Post-Installation Verification:**

```bash
# Check persistent storage structure
ls -la /usr/local/starlink/
ls -la /usr/local/starlink/bin/
ls -la /etc/starlink-config/

# Verify symlinks
ls -la /root/starlink_monitor_unified-rutos.sh
ls -la /root/config.sh

# Test functionality
/usr/local/starlink/bin/starlink_monitor_unified-rutos.sh validate
/usr/local/starlink/bin/starlink_monitor_unified-rutos.sh discover
```

### **Post-Firmware-Upgrade Verification:**

```bash
# Run recovery
/usr/local/starlink/bin/recover-after-firmware-upgrade.sh

# Verify system restoration
/usr/local/starlink/bin/starlink_monitor_unified-rutos.sh status

# Check daemon service
/etc/init.d/starlink-monitor status

# Verify monitoring functionality
tail -20 /usr/local/starlink/logs/rutos-lib.log
```

## 🎯 Key Benefits

### **System Resilience:**

✅ **Firmware Upgrade Survival**: Complete system preservation across upgrades  
✅ **Configuration Persistence**: All settings and customizations maintained  
✅ **Historical Data Retention**: Performance trends and analysis data preserved  
✅ **Zero Downtime Recovery**: Quick restoration with single command

### **Operational Benefits:**

✅ **Reduced Maintenance**: No need to reconfigure after firmware upgrades  
✅ **Consistent Performance**: Maintained intelligence and learning across upgrades  
✅ **Automated Recovery**: Self-healing system with minimal manual intervention  
✅ **Backward Compatibility**: Symlinks ensure existing scripts continue working

## 🚀 Best Practices Summary

1. **Always use persistent storage** (`/usr/local/starlink/`) for the intelligent monitoring system
2. **Run recovery script** after every firmware upgrade
3. **Backup configurations** before major firmware upgrades
4. **Verify MWAN3 availability** after firmware upgrades (may need reinstallation)
5. **Test system functionality** after recovery to ensure proper operation

This persistent storage architecture ensures that your **Intelligent Starlink Monitoring System** becomes a **permanent,
resilient part of your RUTOS infrastructure** that survives firmware upgrades and continues providing intelligent
network management without interruption! 🛡️
