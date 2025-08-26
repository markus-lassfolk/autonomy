#
# Copyright (C) 2025 Teltonika
#

include $(TOPDIR)/rules.mk

PKG_NAME:=autonomy
PKG_VERSION:=1.0.0
PKG_RELEASE:=1

PKG_LICENSE:=MIT
PKG_LICENSE_FILES:=LICENSE

include $(INCLUDE_DIR)/package.mk

define Package/autonomy
	SECTION:=base
	CATEGORY:=Base system
	TITLE:=Autonomous networking system for RUTOS devices
	DEPENDS:=+uci +ubus +mwan3 +gpsctl +gsmctl +luci-base +luci-compat
	ifeq (m, $(CONFIG_PACKAGE_autonomy))
		PKG_TLT_NAME:=autonomy
		PKG_ROUTER:=$(TLT_PLATFORM_NAME)
	endif
endef

define Package/autonomy/description
	Autonomous networking system for RUTOS devices.
	Provides intelligent network failover, GPS tracking, and monitoring
	with Starlink integration and cellular failover capabilities.
endef

define Build/Compile
	# Build Go binaries for ARM
	$(MAKE) -C $(PKG_BUILD_DIR) GOOS=linux GOARCH=arm CGO_ENABLED=0
endef

define Package/autonomy/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) ./files/autonomyd $(1)/usr/bin/
	$(INSTALL_BIN) ./files/autonomysysmgmt $(1)/usr/bin/
	$(INSTALL_BIN) ./files/autonomyctl $(1)/usr/bin/
	
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) ./files/autonomy.init $(1)/etc/init.d/autonomy
	
	$(INSTALL_DIR) $(1)/etc/config
	$(INSTALL_CONF) ./files/autonomy.config $(1)/etc/config/autonomy
	
	$(INSTALL_DIR) $(1)/etc/autonomy
	$(INSTALL_CONF) ./files/autonomy.watchdog.example $(1)/etc/autonomy/
	
	$(INSTALL_DIR) $(1)/etc/cron.d
	$(INSTALL_CONF) ./files/autonomy-cron $(1)/etc/cron.d/autonomy
	
	$(INSTALL_DIR) $(1)/usr/libexec/autonomy
	$(INSTALL_BIN) ./files/autonomy-metrics.sh $(1)/usr/libexec/autonomy/
	
	$(INSTALL_DIR) $(1)/etc/uci-defaults
	$(INSTALL_CONF) ./files/99-autonomy-defaults $(1)/etc/uci-defaults/
endef

$(eval $(call BuildPackage,autonomy))
