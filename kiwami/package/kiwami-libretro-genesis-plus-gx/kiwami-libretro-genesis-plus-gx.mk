################################################################################
#
# kiwami-libretro-genesis-plus-gx
#
################################################################################

# libretro's Genesis Plus GX (the Mega Drive, accurate): its Makefile.libretro
KIWAMI_LIBRETRO_GENESIS_PLUS_GX_VERSION = aa74d79634cd230f65cce4180e92688e53883d2f
KIWAMI_LIBRETRO_GENESIS_PLUS_GX_SITE = $(call github,libretro,Genesis-Plus-GX,$(KIWAMI_LIBRETRO_GENESIS_PLUS_GX_VERSION))
KIWAMI_LIBRETRO_GENESIS_PLUS_GX_LICENSE = Genesis Plus GX (non-commercial)
KIWAMI_LIBRETRO_GENESIS_PLUS_GX_LICENSE_FILES = LICENSE.txt

# HAVE_CDROM=0: its unix build turns on reading a physical CD-ROM drive on Linux (Makefile.libretro), which the
# handheld hasn't got (as PCSX-ReARMed's HAVE_PHYSICAL_CDROM=0 and RetroArch's --disable-cdrom upstream). Its version
# is its own commit, not the one `git rev-parse` finds (this repository's, around output/)
define KIWAMI_LIBRETRO_GENESIS_PLUS_GX_BUILD_CMDS
	$(TARGET_MAKE_ENV) $(MAKE) -C $(@D) -f Makefile.libretro platform=unix HAVE_CDROM=0 CC="$(TARGET_CC)" \
		GIT_VERSION='" $(shell echo $(KIWAMI_LIBRETRO_GENESIS_PLUS_GX_VERSION) | cut -c1-7)"'
endef

define KIWAMI_LIBRETRO_GENESIS_PLUS_GX_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0644 $(@D)/genesis_plus_gx_libretro.so $(TARGET_DIR)/usr/lib/libretro/genesis_plus_gx_libretro.so
endef

$(eval $(generic-package))
