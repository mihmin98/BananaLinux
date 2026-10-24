################################################################################
#
# kiwami-romm
#
################################################################################

# The RomM helper, from banana-ui's tree (ui/src/kiwami/romm/, with the fork's JSON reader and rcheevos' md5.c) but
# built on its own by romm.mk, so neither ui/Makefile nor banana-ui.mk changes. libcurl is dlopen()ed, not linked.
KIWAMI_ROMM_VERSION = local
KIWAMI_ROMM_SITE = $(BR2_EXTERNAL_KIWAMI_PATH)/../ui
KIWAMI_ROMM_SITE_METHOD = local
KIWAMI_ROMM_LICENSE = MIT
KIWAMI_ROMM_LICENSE_FILES = ../LICENSE

define KIWAMI_ROMM_BUILD_CMDS
	$(TARGET_MAKE_ENV) $(MAKE) -B -C $(@D) -f src/kiwami/romm/romm.mk OUT=$(@D) CXX="$(TARGET_CXX)" CC="$(TARGET_CC)" \
		CXXFLAGS="$(TARGET_CXXFLAGS) -O2" CFLAGS="$(TARGET_CFLAGS) -O2" LDFLAGS="$(TARGET_LDFLAGS)"
endef

define KIWAMI_ROMM_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/romm $(TARGET_DIR)/usr/libexec/kiwami/romm
endef

$(eval $(generic-package))
