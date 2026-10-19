include $(TOPDIR)/rules.mk

PKG_NAME:=aiqos-collector
PKG_VERSION:=1.0.1
PKG_RELEASE:=9

include $(INCLUDE_DIR)/package.mk

define Package/aiqos-collector
  SECTION:=net
  CATEGORY:=Network
  TITLE:=AiQoS Go signal collector (pre-built)
  DEPENDS:=+aiqos-engine
endef

define Build/Compile
endef

define Package/aiqos-collector/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) ./files/aiqos-collector $(1)/usr/bin/
endef

$(eval $(call BuildPackage,aiqos-collector))
