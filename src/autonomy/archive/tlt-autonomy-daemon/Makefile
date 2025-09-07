include $(TOPDIR)/rules.mk

PKG_NAME:=tlt_autonomy_daemon
PKG_VERSION:=6.1.0
PKG_RELEASE:=1
PKG_LICENSE:=MIT
PKG_MAINTAINER:=Autonomy Team

include $(INCLUDE_DIR)/package.mk

define Package/tlt-autonomy-daemon
  SECTION:=utils
  CATEGORY:=Utilities
  TITLE:=Autonomy daemon with direct ubus registration
  DEPENDS:=+libubus +libubox +libblobmsg-json +libjson-c +libuci +libmicrohttpd +libcurl
endef

define Package/tlt-autonomy-daemon/description
 Autonomy daemon that registers directly with ubus using libubus API.
 Provides status, config, start, stop, restart, and health methods.
endef

TARGET_CFLAGS += -Wall -O2
TARGET_LDFLAGS += -lubus -lblobmsg_json -lubox -ljson-c -luci -lcurl

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)/
endef

define Build/Compile
	$(TARGET_CC) $(TARGET_CFLAGS) \
		-I$(STAGING_DIR)/usr/include \
		-I$(PKG_BUILD_DIR)/modules \
		-o $(PKG_BUILD_DIR)/autonomy-daemon \
		$(PKG_BUILD_DIR)/autonomy-daemon.c \
		$(PKG_BUILD_DIR)/modules/logging.c \
		$(PKG_BUILD_DIR)/modules/network.c \
		$(PKG_BUILD_DIR)/modules/gps.c \
		$(PKG_BUILD_DIR)/modules/config.c \
		$(PKG_BUILD_DIR)/modules/pidfile.c \
		$(PKG_BUILD_DIR)/modules/ubus_methods.c \
		$(PKG_BUILD_DIR)/modules/network_ubus.c \
		$(PKG_BUILD_DIR)/modules/network_collector.c \
		$(PKG_BUILD_DIR)/modules/network_collector.h \
		$(PKG_BUILD_DIR)/modules/collectors/cellular_collector.c \
		$(PKG_BUILD_DIR)/modules/collectors/cellular_collector.h \
		$(PKG_BUILD_DIR)/modules/network_controller.c \
		$(PKG_BUILD_DIR)/modules/network_controller.h \
		$(PKG_BUILD_DIR)/modules/network_failover.c \
		$(PKG_BUILD_DIR)/modules/network_failover.h \
		$(PKG_BUILD_DIR)/modules/network_discovery.c \
		$(PKG_BUILD_DIR)/modules/network_discovery.h \
		$(PKG_BUILD_DIR)/modules/gps_ubus.c \
		$(PKG_BUILD_DIR)/modules/gps_rutos.c \
		$(PKG_BUILD_DIR)/modules/gps_rutos.h \
		$(PKG_BUILD_DIR)/modules/gps_starlink.c \
		$(PKG_BUILD_DIR)/modules/gps_starlink.h \
		$(PKG_BUILD_DIR)/modules/gps_manager.c \
		$(PKG_BUILD_DIR)/modules/gps_manager.h \
		$(PKG_BUILD_DIR)/modules/gps_comprehensive.c \
		$(PKG_BUILD_DIR)/modules/gps_comprehensive.h \
		$(PKG_BUILD_DIR)/modules/gps_fusion_engine.c \
		$(PKG_BUILD_DIR)/modules/gps_fusion_engine.h \
		$(PKG_BUILD_DIR)/modules/gps_comprehensive_ubus.c \
		$(PKG_BUILD_DIR)/modules/gps_comprehensive_ubus.h \
		$(PKG_BUILD_DIR)/modules/external_apis.c \
		$(PKG_BUILD_DIR)/modules/external_apis.h \
		$(PKG_BUILD_DIR)/modules/external_apis_ubus.c \
		$(PKG_BUILD_DIR)/modules/external_apis_ubus.h \
		$(PKG_BUILD_DIR)/modules/wifi_enhanced.c \
		$(PKG_BUILD_DIR)/modules/wifi_enhanced.h \
		$(PKG_BUILD_DIR)/modules/wifi_enhanced_ubus.c \
		$(PKG_BUILD_DIR)/modules/wifi_enhanced_ubus.h \
		$(PKG_BUILD_DIR)/modules/starlink_comprehensive.c \
		$(PKG_BUILD_DIR)/modules/starlink_comprehensive.h \
		$(PKG_BUILD_DIR)/modules/starlink_comprehensive_ubus.c \
		$(PKG_BUILD_DIR)/modules/starlink_comprehensive_ubus.h \
		$(PKG_BUILD_DIR)/modules/starlink_api_version_monitor.c \
		$(PKG_BUILD_DIR)/modules/starlink_api_version_monitor.h \
		$(PKG_BUILD_DIR)/modules/starlink_api_version_monitor_ubus.c \
		$(PKG_BUILD_DIR)/modules/starlink_api_version_monitor_ubus.h \
		$(PKG_BUILD_DIR)/modules/notifications_comprehensive.c \
		$(PKG_BUILD_DIR)/modules/notifications_comprehensive.h \
		$(PKG_BUILD_DIR)/modules/notifications_comprehensive_ubus.c \
		$(PKG_BUILD_DIR)/modules/notifications_comprehensive_ubus.h \
		$(PKG_BUILD_DIR)/modules/telemetry_comprehensive.c \
		$(PKG_BUILD_DIR)/modules/telemetry_comprehensive.h \
		$(PKG_BUILD_DIR)/modules/telemetry_comprehensive_ubus.c \
		$(PKG_BUILD_DIR)/modules/telemetry_comprehensive_ubus.h \
		$(PKG_BUILD_DIR)/modules/gps_location_reference.c \
		$(PKG_BUILD_DIR)/modules/gps_location_reference.h \
		$(PKG_BUILD_DIR)/modules/gps_confidence.c \
		$(PKG_BUILD_DIR)/modules/gps_confidence.h \
		$(PKG_BUILD_DIR)/modules/gps_accuracy.c \
		$(PKG_BUILD_DIR)/modules/gps_accuracy.h \
		$(PKG_BUILD_DIR)/modules/gps_nmea.c \
		$(PKG_BUILD_DIR)/modules/gps_nmea.h \
		$(PKG_BUILD_DIR)/modules/gps_movement.c \
		$(PKG_BUILD_DIR)/modules/gps_movement.h \
		$(PKG_BUILD_DIR)/modules/gps_clustering.c \
		$(PKG_BUILD_DIR)/modules/gps_clustering.h \
		$(PKG_BUILD_DIR)/modules/gps_health.c \
		$(PKG_BUILD_DIR)/modules/gps_health.h \
		$(PKG_BUILD_DIR)/modules/gps_fusion.c \
		$(PKG_BUILD_DIR)/modules/gps_fusion.h \
		$(PKG_BUILD_DIR)/modules/gps_geofence.c \
		$(PKG_BUILD_DIR)/modules/gps_geofence.h \
		$(PKG_BUILD_DIR)/modules/gps_events.c \
		$(PKG_BUILD_DIR)/modules/gps_events.h \
		$(PKG_BUILD_DIR)/modules/gps_location_services.c \
		$(PKG_BUILD_DIR)/modules/gps_location_services.h \
		$(PKG_BUILD_DIR)/modules/gps_integration.c \
		$(PKG_BUILD_DIR)/modules/gps_integration.h \
		$(PKG_BUILD_DIR)/modules/gps_connector.c \
		$(PKG_BUILD_DIR)/modules/gps_connector.h \
		$(PKG_BUILD_DIR)/modules/gps_system.c \
		$(PKG_BUILD_DIR)/modules/gps_system.h \
		$(PKG_BUILD_DIR)/modules/gps_coordinate_utils.c \
		$(PKG_BUILD_DIR)/modules/gps_coordinate_utils.h \
		$(PKG_BUILD_DIR)/modules/gps_obstruction.c \
		$(PKG_BUILD_DIR)/modules/gps_obstruction.h \
		$(PKG_BUILD_DIR)/modules/gps_adaptive_cache.c \
		$(PKG_BUILD_DIR)/modules/gps_adaptive_cache.h \
		$(PKG_BUILD_DIR)/modules/gps_google_api.c \
		$(PKG_BUILD_DIR)/modules/gps_google_api.h \
		$(PKG_BUILD_DIR)/modules/gps_opencellid.c \
		$(PKG_BUILD_DIR)/modules/gps_opencellid.h \
		$(PKG_BUILD_DIR)/modules/opencellid_complete.c \
		$(PKG_BUILD_DIR)/modules/opencellid_complete.h \
		$(PKG_BUILD_DIR)/modules/opencellid_ubus.c \
		$(PKG_BUILD_DIR)/modules/opencellid_ubus.h \
		$(PKG_BUILD_DIR)/modules/gps_cell_tower.c \
		$(PKG_BUILD_DIR)/modules/gps_cell_tower.h \
		$(PKG_BUILD_DIR)/modules/gps_weather.c \
		$(PKG_BUILD_DIR)/modules/gps_weather.h \
		$(PKG_BUILD_DIR)/modules/gps_terrain.c \
		$(PKG_BUILD_DIR)/modules/gps_terrain.h \
		$(PKG_BUILD_DIR)/modules/gps_performance.c \
		$(PKG_BUILD_DIR)/modules/gps_performance.h \
		$(PKG_BUILD_DIR)/modules/gps_error_recovery.c \
		$(PKG_BUILD_DIR)/modules/gps_error_recovery.h \
		$(PKG_BUILD_DIR)/modules/starlink_obstruction.c \
		$(PKG_BUILD_DIR)/modules/starlink_obstruction.h \
		$(PKG_BUILD_DIR)/modules/wifi_management.c \
		$(PKG_BUILD_DIR)/modules/wifi_management.h \
		$(PKG_BUILD_DIR)/modules/wifi_management_ubus.c \
		$(PKG_BUILD_DIR)/modules/overlay_management.c \
		$(PKG_BUILD_DIR)/modules/overlay_management.h \
		$(PKG_BUILD_DIR)/modules/overlay_management_ubus.c \
		$(PKG_BUILD_DIR)/modules/disk_monitor.c \
		$(PKG_BUILD_DIR)/modules/disk_monitor.h \
		$(PKG_BUILD_DIR)/modules/disk_monitor_ubus.c \
		$(PKG_BUILD_DIR)/modules/uci_maintenance.c \
		$(PKG_BUILD_DIR)/modules/uci_maintenance.h \
		$(PKG_BUILD_DIR)/modules/uci_maintenance_ubus.c \
		$(PKG_BUILD_DIR)/modules/ubus_monitor.c \
		$(PKG_BUILD_DIR)/modules/ubus_monitor.h \
		$(PKG_BUILD_DIR)/modules/ubus_monitor_ubus.c \
		$(PKG_BUILD_DIR)/modules/service_watchdog.c \
		$(PKG_BUILD_DIR)/modules/service_watchdog.h \
		$(PKG_BUILD_DIR)/modules/service_watchdog_ubus.c \
		$(PKG_BUILD_DIR)/modules/notifications/notification_types.c \
		$(PKG_BUILD_DIR)/modules/notifications/notification_types.h \
		$(PKG_BUILD_DIR)/modules/notifications/priority_queue.c \
		$(PKG_BUILD_DIR)/modules/notifications/priority_queue.h \
		$(PKG_BUILD_DIR)/modules/notifications/adaptive_rate_limiter.c \
		$(PKG_BUILD_DIR)/modules/notifications/adaptive_rate_limiter.h \
		$(PKG_BUILD_DIR)/modules/notifications/notification_deduplicator.c \
		$(PKG_BUILD_DIR)/modules/notifications/notification_deduplicator.h \
		$(PKG_BUILD_DIR)/modules/notifications/notification_manager.c \
		$(PKG_BUILD_DIR)/modules/notifications/notification_manager.h \
		$(PKG_BUILD_DIR)/modules/notifications/smart_manager.c \
		$(PKG_BUILD_DIR)/modules/notifications/smart_manager.h \
		$(PKG_BUILD_DIR)/modules/notifications/contextual_alerts.c \
		$(PKG_BUILD_DIR)/modules/notifications/contextual_alerts.h \
		$(PKG_BUILD_DIR)/modules/notifications/emergency_detector.c \
		$(PKG_BUILD_DIR)/modules/notifications/emergency_detector.h \
		$(PKG_BUILD_DIR)/modules/notifications/data_limit_notifications.c \
		$(PKG_BUILD_DIR)/modules/notifications/data_limit_notifications.h \
		$(PKG_BUILD_DIR)/modules/notifications/escalation_manager.c \
		$(PKG_BUILD_DIR)/modules/notifications/escalation_manager.h \
		$(PKG_BUILD_DIR)/modules/notifications/webhook_client.c \
		$(PKG_BUILD_DIR)/modules/notifications/webhook_client.h \
		$(PKG_BUILD_DIR)/modules/notifications/priority_optimizer.c \
		$(PKG_BUILD_DIR)/modules/notifications/priority_optimizer.h \
		$(PKG_BUILD_DIR)/modules/notifications/intelligence_engine.c \
		$(PKG_BUILD_DIR)/modules/notifications/intelligence_engine.h \
		$(PKG_BUILD_DIR)/modules/notifications/email_client.c \
		$(PKG_BUILD_DIR)/modules/notifications/email_client.h \
		$(PKG_BUILD_DIR)/modules/notifications/multi_channel.c \
		$(PKG_BUILD_DIR)/modules/notifications/multi_channel.h \
		$(PKG_BUILD_DIR)/modules/notifications/notification_statistics.c \
		$(PKG_BUILD_DIR)/modules/notifications/notification_statistics.h \
		$(PKG_BUILD_DIR)/modules/notifications/delivery_optimizer.c \
		$(PKG_BUILD_DIR)/modules/notifications/delivery_optimizer.h \
		$(PKG_BUILD_DIR)/modules/notifications/acknowledgment_tracker.c \
		$(PKG_BUILD_DIR)/modules/notifications/acknowledgment_tracker.h \
		$(PKG_BUILD_DIR)/modules/notifications/pushover_client.c \
		$(PKG_BUILD_DIR)/modules/notifications/pushover_client.h \
		$(PKG_BUILD_DIR)/modules/notifications/channel_intelligence.c \
		$(PKG_BUILD_DIR)/modules/notifications/channel_intelligence.h \
		$(PKG_BUILD_DIR)/modules/notifications/discord_client.c \
		$(PKG_BUILD_DIR)/modules/notifications/discord_client.h \
		$(PKG_BUILD_DIR)/modules/notifications/slack_client.c \
		$(PKG_BUILD_DIR)/modules/notifications/slack_client.h \
		$(PKG_BUILD_DIR)/modules/notifications/notification_events.c \
		$(PKG_BUILD_DIR)/modules/notifications/notification_events.h \
		$(PKG_BUILD_DIR)/modules/notifications/notification_config.c \
		$(PKG_BUILD_DIR)/modules/notifications/notification_config.h \
		$(PKG_BUILD_DIR)/modules/notifications/telegram_client.c \
		$(PKG_BUILD_DIR)/modules/notifications/telegram_client.h \
		$(PKG_BUILD_DIR)/modules/notifications/sms_client.c \
		$(PKG_BUILD_DIR)/modules/notifications/sms_client.h \
		$(PKG_BUILD_DIR)/modules/notifications/alert_templates.c \
		$(PKG_BUILD_DIR)/modules/notifications/alert_templates.h \
		$(PKG_BUILD_DIR)/modules/telemetry/telemetry_store.c \
		$(PKG_BUILD_DIR)/modules/telemetry/telemetry_store.h \
		$(PKG_BUILD_DIR)/modules/telemetry/metrics_server.c \
		$(PKG_BUILD_DIR)/modules/telemetry/metrics_server.h \
		$(PKG_BUILD_DIR)/modules/analytics/analytics_engine.h \
		$(PKG_BUILD_DIR)/modules/analytics/analytics_engine.c \
		$(PKG_BUILD_DIR)/modules/analytics/performance_analyzer.h \
		$(PKG_BUILD_DIR)/modules/analytics/performance_analyzer.c \
		$(PKG_BUILD_DIR)/modules/analytics/trend_analyzer.h \
		$(PKG_BUILD_DIR)/modules/analytics/trend_analyzer.c \
		$(PKG_BUILD_DIR)/modules/analytics/health_analyzer.h \
		$(PKG_BUILD_DIR)/modules/analytics/usage_analyzer.h \
		$(PKG_BUILD_DIR)/modules/analytics/usage_analyzer.c \
		$(PKG_BUILD_DIR)/modules/analytics/predictive_analyzer.h \
		$(PKG_BUILD_DIR)/modules/mqtt/mqtt_client.h \
		$(PKG_BUILD_DIR)/modules/mqtt/mqtt_client.c \
		$(PKG_BUILD_DIR)/modules/mqtt/mqtt_telemetry.c \
		$(PKG_BUILD_DIR)/modules/metered/metered_manager.h \
		$(PKG_BUILD_DIR)/modules/metered/metered_manager.c \
		$(PKG_BUILD_DIR)/modules/decision/decision_engine.h \
		$(PKG_BUILD_DIR)/modules/decision/decision_engine.c \
		$(PKG_BUILD_DIR)/modules/predictive/predictive_engine.h \
		$(PKG_BUILD_DIR)/modules/predictive/predictive_engine.c \
		$(PKG_BUILD_DIR)/modules/performance/performance_monitor.h \
		$(PKG_BUILD_DIR)/modules/performance/performance_monitor.c \
		$(PKG_BUILD_DIR)/modules/security/security_monitor.h \
		$(PKG_BUILD_DIR)/modules/security/security_monitor.c \
		$(PKG_BUILD_DIR)/modules/external/external_api_client.h \
		$(PKG_BUILD_DIR)/modules/external/external_api_client.c \
		$(PKG_BUILD_DIR)/modules/testing/test_framework.h \
		$(PKG_BUILD_DIR)/modules/testing/test_framework.c \
		$(PKG_BUILD_DIR)/modules/system_management.c \
		$(PKG_BUILD_DIR)/modules/system_ubus.c \
		$(PKG_BUILD_DIR)/modules/starlink/starlink_types.h \
		$(PKG_BUILD_DIR)/modules/starlink/starlink_client.c \
		$(PKG_BUILD_DIR)/modules/starlink/starlink_collector.c \
		$(PKG_BUILD_DIR)/modules/starlink/starlink_ubus.c \
		$(PKG_BUILD_DIR)/modules/starlink/starlink_cluster.c \
		$(PKG_BUILD_DIR)/modules/starlink/starlink_cluster_ubus.c \
		$(PKG_BUILD_DIR)/modules/notifications/notification_types.h \
		$(PKG_BUILD_DIR)/modules/notifications/notification_manager.c \
		$(PKG_BUILD_DIR)/modules/notifications/notification_manager.h \
		$(PKG_BUILD_DIR)/modules/notifications/notification_ubus.c \
		$(PKG_BUILD_DIR)/modules/notifications/notification_ubus.h \
		$(PKG_BUILD_DIR)/modules/logx.c \
		$(PKG_BUILD_DIR)/modules/logx.h \
		$(PKG_BUILD_DIR)/modules/types.h \
		$(PKG_BUILD_DIR)/modules/uci_manager.c \
		$(PKG_BUILD_DIR)/modules/uci_manager.h \
		$(PKG_BUILD_DIR)/modules/api_server.c \
		$(PKG_BUILD_DIR)/modules/api_server.h \
		-L$(STAGING_DIR)/usr/lib $(TARGET_LDFLAGS)
endef

define Package/tlt-autonomy-daemon/install
	$(INSTALL_DIR) $(1)/usr/sbin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/autonomy-daemon $(1)/usr/sbin/
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) ./files/etc/init.d/autonomy $(1)/etc/init.d/
	$(INSTALL_DIR) $(1)/usr/share/rpcd/acl.d
	$(INSTALL_CONF) ./files/usr/share/rpcd/acl.d/autonomy.json $(1)/usr/share/rpcd/acl.d/
	$(INSTALL_DIR) $(1)/etc/config
	$(INSTALL_CONF) ./files/etc/config/autonomy $(1)/etc/config/
	$(INSTALL_DIR) $(1)/www
	$(INSTALL_DATA) ./files/www/autonomy-cellular-map.html $(1)/www/
	$(INSTALL_DATA) ./files/www/autonomy-cellular-map-demo.html $(1)/www/
endef

$(eval $(call BuildPackage,tlt-autonomy-daemon))
