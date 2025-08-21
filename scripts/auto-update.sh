#!/bin/bash

# autonomy Auto-Update Script
# Checks for updates and installs them when enabled

set -e

# Configuration
CONF=/etc/autonomy/watch.conf
[ -f "$CONF" ] && . "$CONF"

# Default values
: ${AUTO_UPDATE_ENABLED:=0}
: ${UPDATE_CHANNEL:=stable}
: ${UPDATE_CHECK_INTERVAL:=86400}  # 24 hours in seconds

# File paths
UPDATE_LOG="/var/log/autonomy-update.log"
UPDATE_LOCK="/tmp/autonomy-update.lock"
FEED_URL="https://your-org.github.io/autonomy/opkg/${UPDATE_CHANNEL}"
CURRENT_VERSION_FILE="/etc/autonomy/version"

# Logging function
log() {
    echo "$(date '+%Y-%m-%d %H:%M:%S') auto-update: $*" | logger -t autonomy-update
    echo "$(date '+%Y-%m-%d %H:%M:%S') auto-update: $*" >> "$UPDATE_LOG"
}

# Check if auto-update is enabled
check_auto_update_enabled() {
    if [ "${AUTO_UPDATE_ENABLED:-0}" != "1" ]; then
        log "Auto-update disabled, exiting"
        exit 0
    fi
}

# Get current version
get_current_version() {
    if [ -f "$CURRENT_VERSION_FILE" ]; then
        cat "$CURRENT_VERSION_FILE"
    else
        echo "unknown"
    fi
}

# Get available version from feed
get_available_version() {
    local feed_url="$1"
    
    # Download Packages file
    local packages_file=$(mktemp)
    if curl -sS --max-time 30 -o "$packages_file" "$feed_url/Packages.gz" 2>/dev/null; then
        # Extract version from Packages file
        local version=$(grep "^Version:" "$packages_file" | head -1 | cut -d: -f2 | tr -d ' ')
        rm -f "$packages_file"
        echo "$version"
    else
        log "Failed to download Packages file from $feed_url"
        rm -f "$packages_file"
        echo "unknown"
    fi
}

# Download and install package
install_package() {
    local feed_url="$1"
    local version="$2"
    
    log "Installing autonomy version $version"
    
    # Create temporary directory
    local temp_dir=$(mktemp -d)
    cd "$temp_dir"
    
    # Download package
    local package_file="autonomy_${version}_arm_cortex-a7_neon-vfpv4.ipk"
    if ! curl -sS --max-time 300 -o "$package_file" "$feed_url/$package_file"; then
        log "Failed to download package $package_file"
        cd /
        rm -rf "$temp_dir"
        return 1
    fi
    
    # Verify package integrity
    if ! tar -tzf "$package_file" >/dev/null 2>&1; then
        log "Invalid package file: $package_file"
        cd /
        rm -rf "$temp_dir"
        return 1
    fi
    
    # Stop autonomy service
    log "Stopping autonomy service"
    /etc/init.d/autonomy stop 2>/dev/null || true
    
    # Install package
    log "Installing package"
    if opkg install "$package_file"; then
        # Update version file
        echo "$version" > "$CURRENT_VERSION_FILE"
        
        # Start autonomy service
        log "Starting autonomy service"
        /etc/init.d/autonomy start 2>/dev/null || true
        
        log "Successfully updated to version $version"
        
        # Send notification
        if [ -n "${WEBHOOK_URL:-}" ]; then
            send_update_notification "$version"
        fi
        
        cd /
        rm -rf "$temp_dir"
        return 0
    else
        log "Failed to install package"
        
        # Restart autonomy service
        log "Restarting autonomy service"
        /etc/init.d/autonomy start 2>/dev/null || true
        
        cd /
        rm -rf "$temp_dir"
        return 1
    fi
}

# Send update notification
send_update_notification() {
    local version="$1"
    
    # Get system metrics
    local overlay_pct=$(df -P /overlay 2>/dev/null | awk 'NR==2{gsub("%","",$5);print $5}' || echo "0")
    local mem_avail_mb=$(awk '/MemAvailable:/ {print int($2/1024)}' /proc/meminfo 2>/dev/null || echo "0")
    local load1=$(awk '{print $1}' /proc/loadavg 2>/dev/null || echo "0")
    
    # Anonymize device ID
    local device_public
    if [ "${ANONYMIZE_DEVICE_ID:-1}" = "1" ]; then
        device_public=$(printf "%s" "${DEVICE_ID:-unknown}" | openssl dgst -sha256 -hmac "${WATCH_SECRET:-salt}" -binary | xxd -p -c 256 | cut -c1-12 2>/dev/null || echo "anon-device")
    else
        device_public="${DEVICE_ID:-unknown}"
    fi
    
    # Create payload
    local payload=$(cat <<EOF
{
  "device_id": "$device_public",
  "fw": "$(cat /etc/version 2>/dev/null || echo 'unknown')",
  "severity": "info",
  "scenario": "auto_update",
  "overlay_pct": $overlay_pct,
  "mem_avail_mb": $mem_avail_mb,
  "load1": $load1,
  "ubus_ok": true,
  "actions": ["update"],
  "note": "Auto-updated to version $version",
  "ts": $(date +%s)
}
EOF
)
    
    # Create HMAC signature
    local sig=$(printf '%s' "$payload" | openssl dgst -sha256 -hmac "${WATCH_SECRET:-nosig}" -binary | xxd -p -c 256 2>/dev/null || echo "nosig")
    
    # Send webhook
    if curl -sS --max-time 8 -H "Content-Type: application/json" -H "X-Starwatch-Signature: sha256=$sig" -d "$payload" "$WEBHOOK_URL" >/dev/null 2>&1; then
        log "Update notification sent successfully"
    else
        log "Failed to send update notification"
    fi
}

# Check for updates
check_for_updates() {
    local current_version=$(get_current_version)
    local available_version=$(get_available_version "$FEED_URL")
    
    log "Current version: $current_version"
    log "Available version: $available_version"
    
    if [ "$available_version" = "unknown" ]; then
        log "Could not determine available version"
        return 1
    fi
    
    if [ "$current_version" != "$available_version" ]; then
        log "Update available: $current_version -> $available_version"
        
        # Install the update
        if install_package "$FEED_URL" "$available_version"; then
            log "Update completed successfully"
            return 0
        else
            log "Update failed"
            return 1
        fi
    else
        log "No update available"
        return 0
    fi
}

# Main function
main() {
    log "Starting auto-update check"
    
    # Check if auto-update is enabled
    check_auto_update_enabled
    
    # Check if already running
    if [ -f "$UPDATE_LOCK" ]; then
        local lock_pid=$(cat "$UPDATE_LOCK" 2>/dev/null || echo "")
        if [ -n "$lock_pid" ] && kill -0 "$lock_pid" 2>/dev/null; then
            log "Update already running (PID: $lock_pid)"
            exit 0
        else
            log "Removing stale lock file"
            rm -f "$UPDATE_LOCK"
        fi
    fi
    
    # Create lock file
    echo $$ > "$UPDATE_LOCK"
    
    # Clean up lock file on exit
    trap 'rm -f "$UPDATE_LOCK"' EXIT
    
    # Check for updates
    if check_for_updates; then
        log "Auto-update check completed successfully"
    else
        log "Auto-update check failed"
        exit 1
    fi
}

# Run main function
main "$@"
