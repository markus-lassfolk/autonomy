include $(TOPDIR)/rules.mk

APP_TITLE:=VuCI UI Support for Autonomy System
APP_NAME:=$(notdir ${CURDIR})
CLOSED_GPL_INSTALL:=y

ifeq (m, $(CONFIG_PACKAGE_$(APP_NAME)))
	APP_TLT_NAME:=Autonomy System
	APP_ROUTER:=$(TLT_PLATFORM_NAME)
endif

define Package/$(APP_NAME)/description
	Comprehensive web UI for the Autonomy autonomous networking system.
	Provides monitoring, configuration, and control interface for:
	- Starlink health monitoring
	- GPS and location services
	- OpenCELLID data submission
	- Network failover management
	- System health monitoring
	- Telemetry and analytics
endef

include ../app.mk

# call BuildPackage - OpenWrt buildroot signature
