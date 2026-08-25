include $(TOPDIR)/rules.mk

PKG_NAME:=aiqos-engine
PKG_VERSION:=4.5.0
PKG_RELEASE:=2

PKG_MAINTAINER:=AiQoS Team
PKG_LICENSE:=GPL-2.0-only

include $(INCLUDE_DIR)/package.mk

define Package/aiqos-engine
  SECTION:=net
  CATEGORY:=Network
  TITLE:=AiQoS 5G Adaptive Traffic Shaping Engine
  DEPENDS:=+kmod-sched-cake +kmod-ifb +tc-full +libubus +libblobmsg-json +libubox +libnl-tiny +librt
endef

define Package/aiqos-engine/description
  100ms adaptive QCI/AMBR-aware CAKE rate controller for 5G CPE.
  Reads signal telemetry from /dev/shm/aiqos_state and dynamically
  adjusts CAKE bandwidth via Netlink tc.
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)/
endef

define Build/Compile
	$(TARGET_CC) $(TARGET_CFLAGS) $(TARGET_LDFLAGS) \
		-I$(PKG_BUILD_DIR)/include \
		-I$(STAGING_DIR)/usr/include/libnl-tiny \
		-I$(STAGING_DIR)/usr/include \
		-Wall -Wextra -O2 -std=gnu11 -D_GNU_SOURCE \
		-o $(PKG_BUILD_DIR)/aiqosd \
		$(PKG_BUILD_DIR)/engine/aiqosd.c \
		$(PKG_BUILD_DIR)/engine/pid.c \
		$(PKG_BUILD_DIR)/engine/kalman.c \
		$(PKG_BUILD_DIR)/engine/netlink_ctrl.c \
		$(PKG_BUILD_DIR)/engine/scenario.c \
		$(PKG_BUILD_DIR)/engine/shm_reader.c \
		$(PKG_BUILD_DIR)/engine/spatial_guidance.c \
		-lubus -lblobmsg_json -lubox -lnl-tiny -lm -lrt -lpthread
endef

define Package/aiqos-engine/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/aiqosd $(1)/usr/bin/aiqosd

	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) ./files/etc/init.d/aiqosd $(1)/etc/init.d/aiqosd
	$(INSTALL_BIN) ./files/etc/init.d/aiqos-collector $(1)/etc/init.d/aiqos-collector

	$(INSTALL_DIR) $(1)/etc/config
	$(INSTALL_CONF) ./files/aiqos.config $(1)/etc/config/aiqos

	$(INSTALL_DIR) $(1)/etc/nftables.d
	$(INSTALL_DATA) ./files/etc/nftables.d/20-aiqos-mss.nft $(1)/etc/nftables.d/
	$(INSTALL_DATA) ./files/etc/nftables.d/40-aiqos-selective-hnat.nft $(1)/etc/nftables.d/

	$(INSTALL_DIR) $(1)/etc/sysctl.d
	$(INSTALL_DATA) ./files/etc/sysctl.d/99-aiqos-tcp.conf $(1)/etc/sysctl.d/

	$(INSTALL_DIR) $(1)/usr/libexec/rpcd
	$(INSTALL_BIN) ./files/usr/libexec/rpcd/aiqos $(1)/usr/libexec/rpcd/aiqos

	$(INSTALL_DIR) $(1)/usr/share/rpcd/acl.d
	$(INSTALL_DATA) ./files/usr/share/rpcd/acl.d/aiqos.json $(1)/usr/share/rpcd/acl.d/


	$(INSTALL_DIR) $(1)/www/luci-static/resources/view/aiqos
	$(INSTALL_DATA) ./files/www/luci-static/resources/view/aiqos/aiqos.js $(1)/www/luci-static/resources/view/aiqos/
endef

define Package/aiqos-engine/conffiles
/etc/config/aiqos
/etc/sysctl.d/99-aiqos-tcp.conf
endef

$(eval $(call BuildPackage,aiqos-engine))