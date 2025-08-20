# UCI schema stub for autonomy (for documentation and LuCI CBI modeling)
# This is not enforced by UCI; use as a typed reference.

package: autonomy

# =============================================================================
# CORE SYSTEM CONFIGURATION
# =============================================================================

config main
    type: main
    unique: true
    options:
        # Basic daemon control
        enable: { type: bool, default: true }
        use_mwan3: { type: bool, default: true }
        log_level: { type: enum, values: [debug, info, warn, error], default: info }
        log_file: { type: string, nullable: true, notes: "Path to log file (empty = syslog)" }
        
        # Timing and intervals
        poll_interval_ms: { type: int, default: 1500, min: 500, max: 10000, notes: "Polling interval in milliseconds" }
        history_window_s: { type: int, default: 600, min: 60, max: 3600, notes: "History window in seconds" }
        min_uptime_s: { type: int, default: 20, min: 5, max: 300, notes: "Minimum uptime before considering member stable" }
        cooldown_s: { type: int, default: 20, min: 5, max: 300, notes: "Cooldown period after member state change" }
        
        # Memory and storage
        retention_hours: { type: int, default: 24, min: 1, max: 168, notes: "Data retention period in hours" }
        max_ram_mb: { type: int, default: 16, min: 8, max: 128, notes: "Maximum RAM usage in MB" }
        
        # Decision making
        predictive: { type: bool, default: true, notes: "Enable predictive failover using machine learning" }
        switch_margin: { type: int, default: 10, min: 0, max: 50, notes: "Margin for switching decisions" }
        data_cap_mode: { type: enum, values: [balanced, aggressive, conservative], default: balanced }
        
        # Service endpoints
        metrics_listener: { type: bool, default: false, notes: "Enable metrics HTTP listener" }
        health_listener: { type: bool, default: true, notes: "Enable health check HTTP listener" }
        
        # Performance and Security
        performance_profiling: { type: bool, default: false }
        security_auditing: { type: bool, default: true }
        profiling_enabled: { type: bool, default: false }
        auditing_enabled: { type: bool, default: true }
        max_failed_attempts: { type: int, default: 5, min: 1, max: 20 }
        block_duration: { type: int, default: 300, min: 60, max: 3600, notes: "Block duration in seconds" }

# =============================================================================
# NETWORK THRESHOLDS
# =============================================================================

config thresholds 'failover'
    type: thresholds
    key: failover
    options:
        loss: { type: int, default: 5, min: 1, max: 50, notes: "Packet loss percentage to trigger failover" }
        latency: { type: int, default: 1200, min: 100, max: 5000, notes: "Latency threshold in milliseconds" }
        min_duration_s: { type: int, default: 10, min: 5, max: 300, notes: "Minimum duration before failover" }

config thresholds 'restore'
    type: thresholds
    key: restore
    options:
        loss: { type: int, default: 1, min: 0, max: 20, notes: "Packet loss percentage to trigger restore" }
        latency: { type: int, default: 800, min: 50, max: 3000, notes: "Latency threshold in milliseconds" }
        min_duration_s: { type: int, default: 30, min: 10, max: 600, notes: "Minimum duration before restore" }

config thresholds 'weights'
    type: thresholds
    key: weights
    options:
        respect_user_weights: { type: bool, default: true }
        dynamic_adjustment: { type: bool, default: true }
        emergency_override: { type: bool, default: true }
        only_emergency_override: { type: bool, default: false }
        restore_timeout_s: { type: int, default: 300, min: 60, max: 1800 }
        minimal_adjustment_points: { type: int, default: 5, min: 1, max: 20 }
        temporary_boost_points: { type: int, default: 20, min: 5, max: 50 }
        temporary_adjustment_duration_s: { type: int, default: 600, min: 300, max: 3600 }
        emergency_adjustment_duration_s: { type: int, default: 1800, min: 600, max: 7200 }

config thresholds 'intelligence'
    type: thresholds
    key: intelligence
    options:
        starlink_obstruction_threshold: { type: float, default: 10.0, min: 0.0, max: 100.0 }
        cellular_signal_threshold: { type: float, default: -85.0, min: -140.0, max: -50.0, notes: "RSRP threshold in dBm" }
        latency_degradation_threshold: { type: float, default: 2.0, min: 1.0, max: 10.0 }
        loss_threshold: { type: float, default: 5.0, min: 0.1, max: 50.0 }

# =============================================================================
# STARLINK API CONFIGURATION
# =============================================================================

config starlink 'api'
    type: starlink
    key: api
    options:
        host: { type: string, default: "192.168.100.1", notes: "Starlink dish IP address" }
        port: { type: int, default: 9200, min: 1024, max: 65535 }
        timeout_s: { type: int, default: 10, min: 5, max: 60 }
        grpc_first: { type: bool, default: true, notes: "Try gRPC before HTTP" }
        http_first: { type: bool, default: false, notes: "Try HTTP before gRPC" }

# =============================================================================
# MACHINE LEARNING
# =============================================================================

config ml 'settings'
    type: ml
    key: settings
    options:
        enabled: { type: bool, default: true }
        model_path: { type: string, default: "/etc/autonomy/models.json" }
        training: { type: bool, default: true }
        prediction: { type: bool, default: true }

# =============================================================================
# MONITORING ENDPOINTS
# =============================================================================

config monitoring 'mqtt'
    type: monitoring
    key: mqtt
    options:
        broker: { type: string, nullable: true, notes: "MQTT broker URL (empty = disabled)" }
        topic: { type: string, default: "autonomy/status" }

# =============================================================================
# NOTIFICATIONS
# =============================================================================

config notifications 'pushover'
    type: notifications
    key: pushover
    options:
        enabled: { type: bool, default: false }
        token: { type: string, nullable: true, notes: "Pushover application token" }
        user: { type: string, nullable: true, notes: "Pushover user key" }
        device: { type: string, nullable: true, notes: "Pushover device name (empty = all devices)" }

config notifications 'settings'
    type: notifications
    key: settings
    options:
        threshold: { type: enum, values: [info, warning, error, critical], default: warning }
        acknowledgment_tracking: { type: bool, default: true }
        location_enabled: { type: bool, default: true }
        rich_context_enabled: { type: bool, default: true }
        cooldown_s: { type: int, default: 300, min: 60, max: 3600 }
        max_per_hour: { type: int, default: 20, min: 1, max: 100 }

config notifications 'events'
    type: notifications
    key: events
    options:
        failover: { type: bool, default: true }
        failback: { type: bool, default: true }
        member_down: { type: bool, default: true }
        member_up: { type: bool, default: false }
        predictive: { type: bool, default: true }
        critical: { type: bool, default: true }
        recovery: { type: bool, default: true }

config notifications 'priorities'
    type: notifications
    key: priorities
    options:
        failover: { type: int, default: 1, min: -2, max: 2, notes: "Pushover priority for failover events" }
        failback: { type: int, default: 0, min: -2, max: 2 }
        member_down: { type: int, default: 1, min: -2, max: 2 }
        member_up: { type: int, default: -1, min: -2, max: 2 }
        predictive: { type: int, default: 0, min: -2, max: 2 }
        critical: { type: int, default: 2, min: -2, max: 2 }
        recovery: { type: int, default: 0, min: -2, max: 2 }

# =============================================================================
# METERED MODE INTEGRATION
# =============================================================================

config metered 'settings'
    type: metered
    key: settings
    options:
        enabled: { type: bool, default: false }
        warning_threshold: { type: int, default: 80, min: 50, max: 95, notes: "Warning threshold percentage" }
        critical_threshold: { type: int, default: 95, min: 80, max: 99, notes: "Critical threshold percentage" }
        hysteresis_margin: { type: int, default: 5, min: 1, max: 20 }
        stability_delay: { type: int, default: 300, min: 60, max: 1800, notes: "Stability delay in seconds" }
        reconnect_method: { type: enum, values: [gentle, aggressive, none], default: gentle }
        debug: { type: bool, default: false }

# =============================================================================
# GPS CONFIGURATION
# =============================================================================

config gps 'settings'
    type: gps
    key: settings
    options:
        enabled: { type: bool, default: true }
        source_priority: { type: string, default: "rutos,starlink,cellular", notes: "Comma-separated list of GPS sources" }
        movement_threshold_m: { type: int, default: 500, min: 10, max: 5000, notes: "Movement threshold in meters" }
        accuracy_threshold_m: { type: int, default: 50, min: 5, max: 500, notes: "GPS accuracy threshold in meters" }
        staleness_threshold_s: { type: int, default: 300, min: 60, max: 3600, notes: "GPS data staleness threshold" }
        collection_interval_s: { type: int, default: 60, min: 10, max: 300, notes: "GPS collection interval" }
        movement_detection: { type: bool, default: true }
        location_clustering: { type: bool, default: true }
        retry_attempts: { type: int, default: 3, min: 1, max: 10 }
        retry_delay_s: { type: int, default: 5, min: 1, max: 60 }
        google_api_enabled: { type: bool, default: false }
        google_api_key: { type: string, nullable: true }
        google_elevation_api_enabled: { type: bool, default: false }
        
        # GPS API Server Configuration
        api_server_enabled: { type: bool, default: false }
        api_server_port: { type: int, default: 8081, min: 1024, max: 65535 }
        api_server_host: { type: string, default: "localhost" }
        api_server_auth_key: { type: string, nullable: true }

config gps 'main'
    type: gps
    key: main
    options:
        enabled: { type: bool, default: true }
        source_priority: { type: string, default: "rutos,starlink,opencellid,google" }
        movement_threshold_m: { type: int, default: 100, min: 10, max: 5000 }
        accuracy_threshold_m: { type: int, default: 100, min: 5, max: 500 }
        staleness_threshold_s: { type: int, default: 300, min: 60, max: 3600 }
        collection_timeout_s: { type: int, default: 30, min: 10, max: 120 }
        retry_attempts: { type: int, default: 3, min: 1, max: 10 }
        retry_delay_s: { type: int, default: 2, min: 1, max: 60 }
        prefer_high_accuracy: { type: bool, default: true }
        enable_movement_detection: { type: bool, default: true }
        enable_location_clustering: { type: bool, default: true }
        
        # Hybrid confidence-based prioritization
        enable_hybrid_prioritization: { type: bool, default: true }
        min_acceptable_confidence: { type: float, default: 0.5, min: 0.0, max: 1.0 }
        fallback_confidence_threshold: { type: float, default: 0.7, min: 0.0, max: 1.0 }
        
        # OpenCellID Configuration
        opencellid_enabled: { type: bool, default: false }
        opencellid_api_key: { type: string, nullable: true }
        opencellid_contribute: { type: bool, default: false }
        
        # Enhanced OpenCellID Settings
        opencellid_cache_size_mb: { type: int, default: 25, min: 5, max: 100 }
        opencellid_max_cells_per_lookup: { type: int, default: 5, min: 1, max: 10 }
        opencellid_negative_cache_ttl_hours: { type: int, default: 12, min: 1, max: 48 }
        opencellid_contribution_interval: { type: int, default: 10, min: 1, max: 60, notes: "Minutes between contributions" }
        opencellid_min_gps_accuracy: { type: float, default: 20.0, min: 5.0, max: 100.0 }
        opencellid_movement_threshold: { type: float, default: 250.0, min: 50.0, max: 1000.0 }
        opencellid_rsrp_change_threshold: { type: float, default: 6.0, min: 1.0, max: 20.0 }
        opencellid_timing_advance_enabled: { type: bool, default: true }
        opencellid_fusion_confidence: { type: float, default: 0.5, min: 0.0, max: 1.0 }
        
        # Ratio-based rate limiting
        opencellid_ratio_limit: { type: float, default: 8.0, min: 1.0, max: 20.0 }
        opencellid_ratio_window_hours: { type: int, default: 48, min: 24, max: 168 }
        
        # Automated scheduling
        opencellid_scheduler_enabled: { type: bool, default: true }
        opencellid_moving_interval: { type: int, default: 2, min: 1, max: 10, notes: "Minutes when moving" }
        opencellid_stationary_interval: { type: int, default: 10, min: 5, max: 60, notes: "Minutes when stationary" }
        opencellid_max_scans_per_hour: { type: int, default: 30, min: 5, max: 100 }

config gps 'cell_tower'
    type: gps
    key: cell_tower
    options:
        cell_tower_enabled: { type: bool, default: true }
        mozilla_enabled: { type: bool, default: true }
        cell_tower_max_cells: { type: int, default: 6, min: 1, max: 10 }
        cell_tower_timeout: { type: int, default: 30, min: 10, max: 120 }

# =============================================================================
# WIFI OPTIMIZATION
# =============================================================================

config wifi 'optimization'
    type: wifi
    key: optimization
    options:
        enabled: { type: bool, default: false }
        movement_threshold: { type: float, default: 100.0, min: 10.0, max: 1000.0, notes: "GPS movement threshold in meters" }
        stationary_time: { type: int, default: 300, min: 60, max: 1800, notes: "Time to be stationary before optimization" }
        min_improvement: { type: int, default: 3, min: 1, max: 10, notes: "Minimum dBm improvement required" }
        dwell_time: { type: int, default: 30, min: 10, max: 120, notes: "Time to test each channel" }
        noise_default: { type: int, default: -95, min: -120, max: -70, notes: "Default noise floor in dBm" }
        vht80_threshold: { type: int, default: 25, min: 10, max: 50, notes: "Minimum signal for 80MHz channels" }
        vht40_threshold: { type: int, default: 20, min: 10, max: 50, notes: "Minimum signal for 40MHz channels" }
        use_dfs: { type: bool, default: false, notes: "Use DFS channels (radar detection)" }
        cooldown: { type: int, default: 3600, min: 300, max: 86400, notes: "Cooldown between optimizations" }
        gps_accuracy_threshold: { type: float, default: 50.0, min: 5.0, max: 200.0 }
        location_logging: { type: bool, default: true }
        timezone: { type: string, default: "UTC" }

config wifi 'scheduler'
    type: wifi
    key: scheduler
    options:
        nightly_enabled: { type: bool, default: true }
        nightly_time: { type: string, default: "02:00", pattern: "HH:MM" }
        nightly_window: { type: int, default: 60, min: 30, max: 180, notes: "Window for nightly optimization in minutes" }
        weekly_enabled: { type: bool, default: true }
        weekly_days: { type: string, default: "sunday", notes: "Comma-separated list of days" }
        weekly_time: { type: string, default: "03:00", pattern: "HH:MM" }
        weekly_window: { type: int, default: 120, min: 60, max: 360, notes: "Window for weekly optimization in minutes" }
        check_interval: { type: int, default: 300, min: 60, max: 1800, notes: "Scheduler check interval in seconds" }
        skip_if_recent: { type: bool, default: true }
        recent_threshold: { type: int, default: 1800, min: 300, max: 7200, notes: "Recent optimization threshold in seconds" }

# =============================================================================
# MEMBER CONFIGURATIONS
# =============================================================================

config member
    type: member
    repeatable: true
    key: <name>
    options:
        detect: { type: enum, values: [auto, disable, force], default: auto, notes: "Detection mode" }
        class: { type: enum, values: [starlink, cellular, wifi, lan], required: true, notes: "Member class" }
        weight: { type: int, default: 50, min: 0, max: 100, notes: "Weight for scoring (0-100)" }
        min_uptime_s: { type: int, default: 20, min: 5, max: 300, notes: "Minimum uptime in seconds" }
        cooldown_s: { type: int, default: 20, min: 5, max: 300, notes: "Cooldown period in seconds" }
        
        # Cellular-specific options
        prefer_roaming: { type: bool, default: false, notes: "Prefer roaming connections" }
        metered: { type: bool, default: false, notes: "Mark as metered connection" }
        
        # Advanced options
        priority: { type: int, default: 0, min: -10, max: 10, notes: "Priority adjustment" }
        max_latency_ms: { type: int, default: 0, min: 0, max: 10000, notes: "Maximum acceptable latency (0 = no limit)" }
        max_loss_pct: { type: int, default: 0, min: 0, max: 100, notes: "Maximum acceptable packet loss (0 = no limit)" }
        bandwidth_limit_mbps: { type: float, default: 0.0, min: 0.0, max: 10000.0, notes: "Bandwidth limit in Mbps (0 = no limit)" }
        
        # Health check options
        health_check_enabled: { type: bool, default: true }
        health_check_interval_s: { type: int, default: 30, min: 10, max: 300 }
        health_check_timeout_s: { type: int, default: 10, min: 5, max: 60 }
        health_check_retries: { type: int, default: 3, min: 1, max: 10 }
        
        # Probe configuration
        probes:
            type: list(string)
            notes: "Health check probes (e.g., icmp:1.1.1.1, http:https://example.com/health)"
        up_threshold: { type: int, default: 3, min: 1, max: 20 }
        down_threshold: { type: int, default: 3, min: 1, max: 20 }
        latency_warn_ms: { type: int, default: 100, min: 10, max: 1000 }
        latency_crit_ms: { type: int, default: 250, min: 50, max: 2000 }
        jitter_warn_ms: { type: int, default: 30, min: 5, max: 200 }
        loss_warn_pct: { type: int, default: 5, min: 0, max: 100 }
        loss_crit_pct: { type: int, default: 20, min: 0, max: 100 }
        
        # Cellular-specific advanced options
        cell_prefer_bands: { type: string, nullable: true, notes: "CSV of LTE bands, e.g., 3,7,20" }
        min_rsrp_dbm: { type: int, default: -110, min: -140, max: -50, notes: "Minimum RSRP in dBm" }
        min_sinr_db: { type: int, default: 0, min: -20, max: 30, notes: "Minimum SINR in dB" }
        use_modem_metrics: { type: bool, default: false, notes: "Use modem-provided metrics" }
