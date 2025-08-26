include $(TOPDIR)/rules.mk

PKG_NAME:=vuci-app-autonomy-ui
PKG_VERSION:=1.0.0
PKG_RELEASE:=1

PKG_LICENSE:=MIT
PKG_LICENSE_FILES:=LICENSE

include $(INCLUDE_DIR)/package.mk

define Package/$(PKG_NAME)
	SECTION:=net
	CATEGORY:=Network
	TITLE:=VuCI UI for Autonomy System
	DEPENDS:=+vuci-app-autonomy-api +luci-base +luci-compat
endef

define Package/$(PKG_NAME)/description
	Comprehensive web UI for the Autonomy autonomous networking system.
	Provides monitoring, configuration, and control interface.
endef

define Build/Compile
endef

define Package/$(PKG_NAME)/install
	$(INSTALL_DIR) $(1)/usr/share/autonomy-ui
	$(CP) ./src/* $(1)/usr/share/autonomy-ui/
endef

$(eval $(call BuildPackage,$(PKG_NAME)))
