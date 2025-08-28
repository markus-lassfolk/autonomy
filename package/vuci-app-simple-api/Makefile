include $(TOPDIR)/rules.mk

PKG_NAME:=vuci-app-simple-api
PKG_VERSION:=1.0
PKG_RELEASE:=9
PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)

include $(INCLUDE_DIR)/package.mk

define Package/vuci-app-simple-api
  SECTION:=vuci
  CATEGORY:=VUCI
  TITLE:=Simple VUCI API
  DEPENDS:=+lua
endef

define Package/vuci-app-simple-api/description
  Simple VUCI API service for testing
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
endef

define Build/Configure
endef

define Build/Compile
endef

define Package/vuci-app-simple-api/install
	$(INSTALL_DIR) $(1)/usr/lib/lua/api/services
	$(INSTALL_DATA) ./files/usr/lib/lua/api/services/simple.lua $(1)/usr/lib/lua/api/services/
endef

$(eval $(call BuildPackage,vuci-app-simple-api))
