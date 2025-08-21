# Configuration Preservation in Install Script

Version: 2.7.1

## Question: Configuration File Safety

> "If there is a configuration file, I hope we are not overwriting that if we re-run the install script. Is that right?"

## Answer: ✅ **COMPLETELY SAFE**

The install script is designed to **preserve existing configuration files** when re-run. Your customized settings will
**never be overwritten**.

## How It Works

### Configuration Logic

The install script uses this logic to check for existing configuration:

```bash
# Create config.sh from template if it doesn't exist
if [ ! -f "$INSTALL_DIR/config/config.sh" ]; then
    cp "$INSTALL_DIR/config/config.template.sh" "$INSTALL_DIR/config/config.sh"
    print_status "$YELLOW" "Configuration file created from template"
    print_status "$YELLOW" "Please edit $INSTALL_DIR/config/config.sh before using"
fi
```

### What This Means

1. **First Installation**:

   - Creates `config.sh` from `config.template.sh`
   - Shows yellow message asking you to edit it

2. **Re-running Install Script**:
   - Checks if `config.sh` exists
   - **Skips creation if it exists**
   - **Your settings remain untouched**

## File Behavior During Re-Installation

| File                     | Behavior                                 | Safe?       |
| ------------------------ | ---------------------------------------- | ----------- |
| `config.sh`              | **Preserved** - Never overwritten        | ✅ **SAFE** |
| `config.template.sh`     | **Updated** - Gets latest template       | ✅ **SAFE** |
| `scripts/*.sh`           | **Updated** - Gets latest features/fixes | ✅ **SAFE** |
| `validate-config.sh`     | **Updated** - Gets latest validation     | ✅ **SAFE** |
| `upgrade-to-advanced.sh` | **Updated** - Gets latest upgrade logic  | ✅ **SAFE** |

## Benefits of This Design

### ✅ **Safe to Re-run**

- Run the install script as many times as needed
- Update to latest features without losing settings
- Fix issues by re-running installer

### ✅ **Upgrade Path**

- Get new script features automatically
- New configuration options available in updated template
- Use `upgrade-to-advanced.sh` to migrate settings

### ✅ **No Data Loss**

- Customized thresholds preserved
- API keys and tokens preserved
- Network settings preserved
- All personal configurations retained

## How to Update Configuration

If you want to see new configuration options after re-running install:

### Option 1: Manual Comparison

```bash
# Compare your config with new template
diff /root/starlink-monitor/config/config.sh /root/starlink-monitor/config/config.template.sh
```

### Option 2: Use Upgrade Script

```bash
# Upgrade to advanced template (preserves all settings)
/root/starlink-monitor/scripts/upgrade-to-advanced.sh
```

### Option 3: Manual Update

```bash
# View new options in template
cat /root/starlink-monitor/config/config.template.sh

# Edit your config to add new options
vi /root/starlink-monitor/config/config.sh
```

## Testing Commands

You can verify this behavior yourself:

```bash
# First installation
curl -fL https://raw.githubusercontent.com/markus-lassfolk/autonomy/main/scripts/install.sh | sh

# Edit your config
vi /root/starlink-monitor/config/config.sh

# Re-run installer (should preserve your changes)
curl -fL https://raw.githubusercontent.com/markus-lassfolk/autonomy/main/scripts/install.sh | sh

# Verify your changes are still there
cat /root/starlink-monitor/config/config.sh
```

## Best Practices

1. **Always backup before major changes**:

   ```bash
   cp /root/starlink-monitor/config/config.sh /root/starlink-monitor/config/config.sh.backup
   ```

2. **Test configuration after updates**:

   ```bash
   /root/starlink-monitor/scripts/validate-config.sh
   ```

3. **Use upgrade script for advanced features**:

   ```bash
   /root/starlink-monitor/scripts/upgrade-to-advanced.sh
   ```

## Conclusion

**Your configuration is completely safe!** The install script is designed for production use where preserving user
settings is critical. You can confidently re-run the installation to get updates without fear of losing your
customizations.

The script follows the principle: **Update the tools, preserve the configuration**.
