include $(TOPDIR)/rules.mk

PKG_NAME:=luci-app-5g-dashboard
PKG_VERSION:=1.0.0
PKG_RELEASE:=15

PKG_MAINTAINER:=5G Dashboard
PKG_LICENSE:=MIT

include $(INCLUDE_DIR)/package.mk

define Package/luci-app-5g-dashboard
  SECTION:=luci
  CATEGORY:=LuCI
  SUBMENU:=3. Applications
  TITLE:=5G Cellular Network Dashboard
  PKGARCH:=all
endef

define Package/luci-app-5g-dashboard/description
  A modern 5G cellular network dashboard for OpenWrt.
endef

define Build/Compile
endef

define Package/luci-app-5g-dashboard/install
	$(INSTALL_DIR) $(1)/www/5g-dashboard
	$(INSTALL_DATA) ./root/www/5g-dashboard/style.css $(1)/www/5g-dashboard/
	$(INSTALL_DATA) ./root/www/5g-dashboard/main.js $(1)/www/5g-dashboard/
	$(INSTALL_DATA) ./root/www/5g-dashboard/index.html $(1)/www/5g-dashboard/
	$(INSTALL_DATA) ./root/www/5g-dashboard/tabs.html $(1)/www/5g-dashboard/
	$(INSTALL_DATA) ./root/www/5g-dashboard/tabs.js $(1)/www/5g-dashboard/
	$(INSTALL_DIR) $(1)/www/cgi-bin
	$(INSTALL_BIN) ./root/www/cgi-bin/get_5g_info $(1)/www/cgi-bin/
	$(INSTALL_DIR) $(1)/usr/lib/lua/luci/controller
	$(INSTALL_DATA) ./luasrc/controller/5g_dashboard.lua $(1)/usr/lib/lua/luci/controller/
	$(INSTALL_DIR) $(1)/usr/lib/lua/luci/view/5g_dashboard
	$(INSTALL_DATA) ./luasrc/view/5g_dashboard/index.htm $(1)/usr/lib/lua/luci/view/5g_dashboard/
	$(INSTALL_DIR) $(1)/usr/share/rpcd/acl.d
	$(INSTALL_DATA) ./root/usr/share/rpcd/acl.d/luci-app-5g-dashboard.json $(1)/usr/share/rpcd/acl.d/
endef

define Package/luci-app-5g-dashboard/postinst
#!/bin/sh
[ -n "$${IPKG_INSTROOT}" ] || {
	rm -rf /tmp/luci-indexcache*
	rm -rf /tmp/luci-modulecache/
	/etc/init.d/rpcd restart
}
endef

$(eval $(call BuildPackage,luci-app-5g-dashboard))
