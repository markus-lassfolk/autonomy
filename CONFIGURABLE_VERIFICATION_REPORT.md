
# UCI Configuration Verification Report - Configurable Values Only

## 📊 Verification Results
- **Total C files checked**: 140
- **Files with configurable values**: 68
- **Total configurable values found**: 138
- **Files clean**: 72

## 🎯 Status

## ⚠️  VERIFICATION FAILED!
**138 configurable values still need to be addressed.**

### Files with remaining configurable values:

#### src/c/autonomy-daemon/analytics/performance_analyzer.c (1 values)
- Line 40: `g_performance_analyzer.enabled = true;`

#### src/c/autonomy-daemon/analytics/predictive_engine.c (2 values)
- Line 37: `g_predictive_engine.config.enabled = true;`
- Line 40: `g_predictive_engine.config.enable_machine_learning = true;`

#### src/c/autonomy-daemon/analytics/trend_analyzer.c (2 values)
- Line 43: `g_trend_analyzer.config.min_data_points = 10;`
- Line 45: `g_trend_analyzer.config.enable_prediction = true;`

#### src/c/autonomy-daemon/external/external_apis.c (8 values)
- Line 136: `config->max_requests_per_hour = 1000;`
- Line 137: `config->max_requests_per_day = 10000;`
- Line 143: `config->max_requests_per_hour = 100;`
- Line 144: `config->max_requests_per_day = 1000;`
- Line 150: `config->max_requests_per_hour = 1000;`
- ... and 3 more values

#### src/c/autonomy-daemon/gps/gps_accuracy.c (1 values)
- Line 58: `g_accuracy_validator.enabled = true;`

#### src/c/autonomy-daemon/gps/gps_cell_tower.c (1 values)
- Line 48: `g_cell_tower.enabled = true;`

#### src/c/autonomy-daemon/gps/gps_clustering.c (1 values)
- Line 54: `g_clustering.enabled = true;`

#### src/c/autonomy-daemon/gps/gps_confidence.c (1 values)
- Line 61: `g_confidence_calc.enabled = true;`

#### src/c/autonomy-daemon/gps/gps_error_recovery.c (2 values)
- Line 548: `config.enabled = true;`
- Line 844: `source->last_retry = 0;`

#### src/c/autonomy-daemon/gps/gps_events.c (3 values)
- Line 90: `g_events.enabled = true;`
- Line 109: `g_events.events[i].enabled = false;`
- Line 171: `event->enabled = true;`

#### src/c/autonomy-daemon/gps/gps_fusion.c (1 values)
- Line 54: `g_fusion.enabled = true;`

#### src/c/autonomy-daemon/gps/gps_fusion_engine.c (1 values)
- Line 389: `double max_distance = 0.0;`

#### src/c/autonomy-daemon/gps/gps_google_api.c (1 values)
- Line 75: `g_google_api.enabled = true;`

#### src/c/autonomy-daemon/gps/gps_health.c (1 values)
- Line 56: `g_health_monitor.enabled = true;`

#### src/c/autonomy-daemon/gps/gps_integration.c (4 values)
- Line 51: `g_integration.enabled = true;`
- Line 69: `g_integration.gps_sources[i].enabled = false;`
- Line 134: `source->enabled = true;`
- Line 393: `source->enabled = false;`

#### src/c/autonomy-daemon/gps/gps_location_manager.c (1 values)
- Line 77: `source->enabled = true;`

#### src/c/autonomy-daemon/gps/gps_location_services.c (1 values)
- Line 68: `g_location_services.enabled = true;`

#### src/c/autonomy-daemon/gps/gps_manager.c (3 values)
- Line 60: `g_gps_manager.enabled = true;`
- Line 70: `g_gps_manager.sources[i].enabled = false;`
- Line 329: `g_gps_manager.sources[index].enabled = true;`

#### src/c/autonomy-daemon/gps/gps_movement.c (1 values)
- Line 56: `g_movement_detector.enabled = true;`

#### src/c/autonomy-daemon/gps/gps_nmea.c (1 values)
- Line 60: `g_nmea_parser.enabled = true;`

#### src/c/autonomy-daemon/gps/gps_obstruction.c (1 values)
- Line 50: `g_obstruction.enabled = true;`

#### src/c/autonomy-daemon/gps/gps_performance.c (1 values)
- Line 60: `g_performance.enabled = true;`

#### src/c/autonomy-daemon/gps/gps_rutos.c (1 values)
- Line 56: `g_rutos_gps.enabled = true;`

#### src/c/autonomy-daemon/gps/gps_starlink.c (2 values)
- Line 57: `g_starlink_gps.enabled = true;`
- Line 249: `request_config.timeout_seconds = 5;`

#### src/c/autonomy-daemon/gps/gps_system.c (4 values)
- Line 37: `g_gps_system.enabled = true;`
- Line 53: `g_gps_system.module_status[i].enabled = false;`
- Line 474: `module->enabled = false;`
- Line 604: `g_gps_system.module_status[i].enabled = false;`

#### src/c/autonomy-daemon/gps/gps_terrain.c (4 values)
- Line 68: `g_terrain.enabled = true;`
- Line 232: `int srtm_size = 1201;`
- Line 443: `double max_slope = 0.0;`
- Line 458: `double max_slope_here = 0.0;`

#### src/c/autonomy-daemon/gps/gps_weather.c (2 values)
- Line 74: `g_weather.enabled = true;`
- Line 500: `air_config.timeout = 10;`

#### src/c/autonomy-daemon/gps/opencellid_complete.c (1 values)
- Line 544: `response->size = 0;`

#### src/c/autonomy-daemon/network/network_collector.c (1 values)
- Line 328: `int test_port_count = 4;`

#### src/c/autonomy-daemon/network/network_collector_archive.c (3 values)
- Line 45: `g_collector.enabled = true;`
- Line 48: `g_collector.max_test_targets = 8;`
- Line 57: `g_collector.metrics_history_size = 100;`

#### src/c/autonomy-daemon/network/network_controller.c (4 values)
- Line 50: `g_network_controller.config.enabled = true;`
- Line 56: `g_network_controller.config.validation_timeout_seconds = 10;`
- Line 57: `g_network_controller.config.enable_callbacks = true;`
- Line 67: `g_network_controller.max_members = 16;`

#### src/c/autonomy-daemon/network/network_discovery.c (1 values)
- Line 47: `g_discovery.enabled = true;`

#### src/c/autonomy-daemon/network/network_failover.c (1 values)
- Line 41: `g_failover.enabled = true;`

#### src/c/autonomy-daemon/notifications/acknowledgment_tracker.c (1 values)
- Line 93: `g_acknowledgment_tracker.max_acknowledgments = 0;`

#### src/c/autonomy-daemon/notifications/alert_templates.c (2 values)
- Line 82: `g_alert_template_manager.max_templates = 0;`
- Line 103: `template->enabled = true;`

#### src/c/autonomy-daemon/notifications/channel_intelligence.c (1 values)
- Line 103: `g_channel_intelligence.max_channel_effectiveness_entries = 0;`

#### src/c/autonomy-daemon/notifications/data_limit_notifications.c (2 values)
- Line 84: `g_data_limit_manager.max_interfaces = 0;`
- Line 86: `g_data_limit_manager.max_last_notifications = 0;`

#### src/c/autonomy-daemon/notifications/delivery_optimizer.c (2 values)
- Line 335: `next_optimal.tm_min = 0;`
- Line 346: `next_optimal.tm_min = 0;`

#### src/c/autonomy-daemon/notifications/email_client.c (1 values)
- Line 392: `for (int attempt = 1; attempt <= max_attempts && !sent; attempt++) {`

#### src/c/autonomy-daemon/notifications/emergency_detector.c (2 values)
- Line 86: `g_emergency_detector.max_active_incidents = 0;`
- Line 88: `g_emergency_detector.max_failure_records = 0;`

#### src/c/autonomy-daemon/notifications/escalation_manager.c (2 values)
- Line 112: `g_escalation_manager.max_active_escalations = 0;`
- Line 114: `g_escalation_manager.max_escalation_history = 0;`

#### src/c/autonomy-daemon/notifications/intelligence_engine.c (3 values)
- Line 121: `g_intelligence_engine.max_notification_patterns = 0;`
- Line 123: `g_intelligence_engine.max_user_patterns = 0;`
- Line 538: `status->enabled = true;`

#### src/c/autonomy-daemon/notifications/notification_config.c (11 values)
- Line 252: `config->pushover_enabled = false;`
- Line 258: `config->pushover_enabled = false;`
- Line 268: `config->email_enabled = false;`
- Line 274: `config->email_enabled = false;`
- Line 280: `config->email_enabled = false;`
- ... and 6 more values

#### src/c/autonomy-daemon/notifications/notification_deduplicator.c (1 values)
- Line 56: `dedup->max_fingerprints = 0;`

#### src/c/autonomy-daemon/notifications/notification_statistics.c (2 values)
- Line 335: `stats->rate_limited = 0;`
- Line 360: `stats->latency_stats.max_latency_ms = 0;`

#### src/c/autonomy-daemon/notifications/priority_optimizer.c (1 values)
- Line 86: `g_priority_optimizer.max_learning_entries = 0;`

#### src/c/autonomy-daemon/notifications/priority_queue.c (1 values)
- Line 49: `queue->max_size = 0;`

#### src/c/autonomy-daemon/notifications/pushover_client.c (1 values)
- Line 139: `message->html_enabled = true;`

#### src/c/autonomy-daemon/notifications/smart_manager.c (4 values)
- Line 76: `g_smart_manager.stats.rate_limited = 0;`
- Line 81: `g_smart_manager.stats.max_latency = 0;`
- Line 109: `g_smart_manager.max_history_size = 0;`
- Line 402: `status->enabled = true;`

#### src/c/autonomy-daemon/starlink/starlink_api_version_monitor.c (2 values)
- Line 273: `version->minor_version = 0;`
- Line 813: `request_config.timeout_seconds = 5;`

#### src/c/autonomy-daemon/starlink/starlink_cluster.c (3 values)
- Line 25: `g_starlink_cluster.auto_failover_enabled = true;`
- Line 26: `g_starlink_cluster.failover_threshold = 3;`
- Line 27: `g_starlink_cluster.min_health_score = 70.0;`

#### src/c/autonomy-daemon/starlink/starlink_cluster_ubus.c (2 values)
- Line 113: `config.predictive_enabled = true;`
- Line 114: `config.enabled = true;`

#### src/c/autonomy-daemon/starlink/starlink_collector.c (1 values)
- Line 53: `g_collector_state.collection_enabled = true;`

#### src/c/autonomy-daemon/starlink/starlink_comprehensive.c (1 values)
- Line 908: `request_config.timeout_seconds = 15;`

#### src/c/autonomy-daemon/starlink/starlink_obstruction.c (3 values)
- Line 78: `g_obstruction.enabled = true;`
- Line 133: `g_obstruction.trend_analyzer.min_points_for_analysis = 10;`
- Line 144: `g_obstruction.movement_detector.location_history_size = 100;`

#### src/c/autonomy-daemon/starlink/starlink_snow_detection.c (1 values)
- Line 56: `g_snow_detection.enabled = true;`

#### src/c/autonomy-daemon/starlink/starlink_snow_detection_integration.c (2 values)
- Line 51: `g_integration.enabled = true;`
- Line 321: `sample->avg_prolonged_obstruction_interval_s = 0.0;`

#### src/c/autonomy-daemon/telemetry/telemetry_comprehensive_ubus.c (2 values)
- Line 269: `if (limit > 1000) limit = 1000;`
- Line 436: `const int MAX_SAMPLES = 5000;`

#### src/c/autonomy-daemon/telemetry/telemetry_store.c (4 values)
- Line 45: `buffer->size = 0;`
- Line 195: `g_telemetry_store.max_members = 64;`
- Line 219: `g_telemetry_store.status.enabled = true;`
- Line 268: `g_telemetry_store.max_members = 0;`

#### src/c/autonomy-daemon/ubus/ubus_monitor.c (2 values)
- Line 56: `g_ubus_monitor.fix_attempts = 0;`
- Line 304: `g_ubus_monitor.fix_attempts = 0;`

#### src/c/autonomy-daemon/utils/credential_manager.c (1 values)
- Line 66: `g_credential_manager.config.enable_encryption = false;`

#### src/c/autonomy-daemon/utils/disk_monitor.c (3 values)
- Line 44: `g_disk_monitor.config.max_log_size_mb = 100;`
- Line 45: `g_disk_monitor.config.max_temp_age_hours = 24;`
- Line 309: `int64_t size = 0;`

#### src/c/autonomy-daemon/utils/http_client.c (1 values)
- Line 139: `response->size = 0;`

#### src/c/autonomy-daemon/utils/http_client_libcurl.c (3 values)
- Line 92: `g_http_client.config.default_max_redirects = 5;`
- Line 94: `g_http_client.config.enable_compression = true;`
- Line 95: `g_http_client.config.max_concurrent_requests = 10;`

#### src/c/autonomy-daemon/utils/metered_manager.c (1 values)
- Line 48: `g_metered_manager.config.enabled = true;`

#### src/c/autonomy-daemon/utils/overlay_management.c (4 values)
- Line 45: `g_overlay_manager.config.enabled = true;`
- Line 46: `g_overlay_manager.config.overlay_space_threshold = 80;`
- Line 47: `g_overlay_manager.config.overlay_critical_threshold = 90;`
- Line 49: `g_overlay_manager.config.notifications_enabled = true;`

#### src/c/autonomy-daemon/utils/security_monitor.c (1 values)
- Line 683: `int failed_attempts = 0;`

#### src/c/autonomy-daemon/wifi/wifi_management_ubus.c (1 values)
- Line 322: `bool enabled = false;`

## 📈 Summary
- **UCI Configuration Integration**: ⚠️  INCOMPLETE
- **System Configurability**: ⚠️  PARTIALLY CONFIGURABLE
- **User Control**: ⚠️  LIMITED CONTROL

## 🎯 Next Steps

1. Address remaining 138 configurable values
2. Run verification script again
3. Complete UCI configuration integration
