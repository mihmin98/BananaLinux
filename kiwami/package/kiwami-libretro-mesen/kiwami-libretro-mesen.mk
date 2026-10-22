################################################################################
#
# kiwami-libretro-mesen
#
################################################################################

# libretro's Mesen (the NES, accurate): its Libretro makefile, no frontends. Mesen 2 has no libretro core.
KIWAMI_LIBRETRO_MESEN_VERSION = 0102910c39ad1a62bc3f784466f3f67ca9eae335
KIWAMI_LIBRETRO_MESEN_SITE = $(call github,libretro,Mesen,$(KIWAMI_LIBRETRO_MESEN_VERSION))
KIWAMI_LIBRETRO_MESEN_LICENSE = GPL-3.0
KIWAMI_LIBRETRO_MESEN_LICENSE_FILES = LICENSE

define KIWAMI_LIBRETRO_MESEN_BUILD_CMDS
	$(TARGET_MAKE_ENV) $(MAKE) -C $(@D)/Libretro platform=unix CC="$(TARGET_CC)" CXX="$(TARGET_CXX)" AR="$(TARGET_AR)"
endef

define KIWAMI_LIBRETRO_MESEN_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0644 $(@D)/Libretro/mesen_libretro.so $(TARGET_DIR)/usr/lib/libretro/mesen_libretro.so
endef

$(eval $(generic-package))
