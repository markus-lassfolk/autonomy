include $(TOPDIR)/rules.mk

PKG_NAME:=vuci-app-simple-ui
PKG_VERSION:=1.0
PKG_RELEASE:=9
PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)

include $(INCLUDE_DIR)/package.mk

define Package/vuci-app-simple-ui
  SECTION:=vuci
  CATEGORY:=VUCI
  TITLE:=Simple VUCI UI
  DEPENDS:=+vuci-app-simple-api
endef

define Package/vuci-app-simple-ui/description
  Simple VUCI UI for testing
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
endef

define Build/Configure
endef

define Build/Compile
endef

define Package/vuci-app-simple-ui/install
	$(INSTALL_DIR) $(1)/usr/share/vuci/menu.d
	$(INSTALL_DATA) ./files/usr/share/vuci/menu.d/simple.json $(1)/usr/share/vuci/menu.d/
	$(INSTALL_DIR) $(1)/www/simple
	$(INSTALL_DATA) ./files/www/simple/index.html $(1)/www/simple/
endef

define Package/vuci-app-simple-ui/postinst
#!/bin/sh
[ -z "$$$${IPKG_INSTROOT}" ] && {
    echo "Creating symlinks for VUCI..."
    mkdir -p /overlay/root/upper/usr/share/vuci/menu.d 2>/dev/null
    ln -sf /usr/local/usr/share/vuci/menu.d/simple.json /overlay/root/upper/usr/share/vuci/menu.d/simple.json 2>/dev/null
    
    echo "Restarting web server..."
    /etc/init.d/uhttpd restart 2>/dev/null
    
    echo "Simple UI installed successfully!"
}
exit 0
endef

$(eval $(call BuildPackage,vuci-app-simple-ui))
