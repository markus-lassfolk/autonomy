include $(TOPDIR)/rules.mk

PKG_NAME:=autonomy-ui
PKG_VERSION:=1.0.0
PKG_RELEASE:=1

PKG_SOURCE_PROTO:=git
PKG_SOURCE_URL:=https://github.com/markus-lassfolk/autonomy.git
PKG_SOURCE_VERSION:=main
PKG_SOURCE_SUBDIR:=$(PKG_NAME)
PKG_SOURCE:=$(PKG_NAME)-$(PKG_VERSION).tar.gz

PKG_MAINTAINER:=Markus Lassfolk <markus@example.com>
PKG_LICENSE:=GPL-2.0
PKG_LICENSE_FILES:=LICENSE

PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)

include $(INCLUDE_DIR)/package.mk

define Package/autonomy-ui
  SECTION:=net
  CATEGORY:=Network
  TITLE:=Autonomy Web UI (VUCI)
  DEPENDS:=+luci +luci-base +autonomy-api
  URL:=https://github.com/markus-lassfolk/autonomy
endef

define Package/autonomy-ui/description
  Vue.js-based web interface for Autonomy system management.
  Provides VUCI integration for configuration and monitoring.
endef

define Build/Prepare
	$(call Build/Prepare/Default)
	$(CP) $(PKG_BUILD_DIR)/src/web/autonomy-ui/* $(PKG_BUILD_DIR)/
endef

define Build/Configure
	# No configuration needed for web assets
endef

define Build/Compile
	# No compilation needed for web assets
endef

define Package/autonomy-ui/install
	$(INSTALL_DIR) $(1)/www/autonomy
	$(CP) $(PKG_BUILD_DIR)/dist/* $(1)/www/autonomy/
	
	$(INSTALL_DIR) $(1)/usr/share/rpcd/acl.d
	$(INSTALL_DATA) ./files/autonomy-ui.json $(1)/usr/share/rpcd/acl.d/
endef

$(eval $(call BuildPackage,autonomy-ui))
