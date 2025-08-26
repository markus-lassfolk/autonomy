include $(TOPDIR)/rules.mk

PKG_NAME:=vuci-app-autonomy-api
PKG_VERSION:=1.0.0
PKG_RELEASE:=1

PKG_LICENSE:=MIT
PKG_LICENSE_FILES:=LICENSE

include $(INCLUDE_DIR)/package.mk

define Package/$(PKG_NAME)
	SECTION:=net
	CATEGORY:=Network
	TITLE:=VuCI API for Autonomy System
	DEPENDS:=+autonomy +luci-base +luci-compat +uhttpd
endef

define Package/$(PKG_NAME)/description
	API backend for the Autonomy autonomous networking system.
	Provides REST API endpoints for monitoring, configuration, and control.
endef

define Build/Compile
endef

define Package/$(PKG_NAME)/install
	$(INSTALL_DIR) $(1)/usr/libexec/autonomy-api
	$(INSTALL_BIN) ./src/autonomy-api.lua $(1)/usr/libexec/autonomy-api/
	
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) ./src/autonomy-api.init $(1)/etc/init.d/autonomy-api
	
	$(INSTALL_DIR) $(1)/etc/config
	$(INSTALL_CONF) ./src/autonomy-api.config $(1)/etc/config/autonomy-api
endef

$(eval $(call BuildPackage,$(PKG_NAME)))
