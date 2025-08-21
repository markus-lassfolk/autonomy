include $(TOPDIR)/rules.mk

LUCI_TITLE:=Autonomy Multi-Interface Failover
LUCI_DEPENDS:=+autonomy +luci-compat +luci-base
LUCI_PKGARCH:=all

PKG_NAME:=vuci-app-autonomy
PKG_VERSION:=1.0.0
PKG_RELEASE:=1

PKG_MAINTAINER:=autonomy Team <autonomy@example.com>
PKG_LICENSE:=GPL-3.0-or-later
PKG_LICENSE_FILES:=LICENSE

include $(TOPDIR)/feeds/luci/luci.mk

# call BuildPackage - OpenWrt buildroot signature
$(eval $(call BuildPackage,vuci-app-autonomy))

define Package/vuci-app-autonomy/install
	$(INSTALL_DIR) $(1)/usr/lib/lua/luci/controller
	$(INSTALL_DATA) root/usr/lib/lua/luci/controller/autonomy.lua $(1)/usr/lib/lua/luci/controller/
	
	$(INSTALL_DIR) $(1)/usr/lib/lua/luci/view/autonomy
	$(INSTALL_DATA) root/usr/lib/lua/luci/view/autonomy/status.htm $(1)/usr/lib/lua/luci/view/autonomy/
	$(INSTALL_DATA) root/usr/lib/lua/luci/view/autonomy/config.htm $(1)/usr/lib/lua/luci/view/autonomy/
	$(INSTALL_DATA) root/usr/lib/lua/luci/view/autonomy/interfaces.htm $(1)/usr/lib/lua/luci/view/autonomy/
	$(INSTALL_DATA) root/usr/lib/lua/luci/view/autonomy/telemetry.htm $(1)/usr/lib/lua/luci/view/autonomy/
	$(INSTALL_DATA) root/usr/lib/lua/luci/view/autonomy/logs.htm $(1)/usr/lib/lua/luci/view/autonomy/
	$(INSTALL_DATA) root/usr/lib/lua/luci/view/autonomy/monitoring.htm $(1)/usr/lib/lua/luci/view/autonomy/
	
	$(INSTALL_DIR) $(1)/usr/libexec/rpcd
	$(INSTALL_BIN) root/usr/libexec/rpcd/autonomy $(1)/usr/libexec/rpcd/
	
	$(INSTALL_DIR) $(1)/usr/lib/lua/luci/view/autonomy
	$(INSTALL_DATA) root/usr/lib/lua/luci/view/autonomy/pwa-manifest.json $(1)/usr/lib/lua/luci/view/autonomy/
	$(INSTALL_DATA) root/usr/lib/lua/luci/view/autonomy/sw.js $(1)/usr/lib/lua/luci/view/autonomy/
	$(INSTALL_DATA) root/usr/lib/lua/luci/view/autonomy/autonomy-pwa.js $(1)/usr/lib/lua/luci/view/autonomy/
	
	$(INSTALL_DIR) $(1)/usr/share/autonomy/icons
	$(INSTALL_DATA) icons/autonomy-192.png $(1)/usr/share/autonomy/icons/
	$(INSTALL_DATA) icons/autonomy-512.png $(1)/usr/share/autonomy/icons/
	
	$(INSTALL_DIR) $(1)/usr/share/autonomy/screenshots
	$(INSTALL_DATA) screenshots/autonomy-status.png $(1)/usr/share/autonomy/screenshots/
	$(INSTALL_DATA) screenshots/autonomy-mobile.png $(1)/usr/share/autonomy/screenshots/
endef

