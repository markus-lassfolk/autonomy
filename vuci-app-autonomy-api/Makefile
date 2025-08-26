include $(TOPDIR)/rules.mk

APP_TITLE:=VuCI API Support for Autonomy System
APP_NAME:=$(notdir ${CURDIR})
CLOSED_GPL_INSTALL:=y

ifeq (m, $(CONFIG_PACKAGE_$(APP_NAME)))
	APP_TLT_NAME:=Autonomy API
	APP_ROUTER:=$(TLT_PLATFORM_NAME)
endif

define Package/$(APP_NAME)/description
	API backend for the Autonomy autonomous networking system.
	Provides REST API endpoints for monitoring, configuration, and control.
endef

include ../app.mk

# call BuildPackage - OpenWrt buildroot signature
