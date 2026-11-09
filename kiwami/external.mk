include $(sort $(wildcard $(BR2_EXTERNAL_KIWAMI_PATH)/package/*/*.mk))

# Under per-package directories (kiwami.fragment) a package builds against only what it depends on: what packages use
# without declaring it, which one package at a time they find because it happens to be built first. Those that link
# it kiwami/tests/deps-audit.py finds; those that only look for it (an optional feature) kiwami/tests/image-compare.sh
# finds, against a build made with KIWAMI_SERIAL=1. Buildroot reads this file after their .mk has been evaluated, so
# the += no longer orders the build: each is also an order-only prerequisite of the package's configure step, as
# upstream's buildroot/external.mk does for bluez-alsa's ldacbt. (The += still reaches the step's copy of its
# dependencies, which reads <PKG>_FINAL_DEPENDENCIES when it runs.) Without per-package directories (KIWAMI_SERIAL=1)
# they build exactly as upstream's.
ifeq ($(BR2_PER_PACKAGE_DIRECTORIES),y)
# upstream's mupen64plus-next links libEGL and libGLESv2 (FORCE_GLES3=1)
ifeq ($(BR2_PACKAGE_LIBRETRO_MUPEN64PLUS_NEXT),y)
LIBRETRO_MUPEN64PLUS_NEXT_DEPENDENCIES += libegl libgles
$(LIBRETRO_MUPEN64PLUS_NEXT_TARGET_CONFIGURE): | libegl libgles
endif
# Buildroot's sqlite: its configure finds zlib, and sqlite3 and libsqlite3 link it
ifeq ($(BR2_PACKAGE_SQLITE)$(BR2_PACKAGE_ZLIB),yy)
SQLITE_DEPENDENCIES += zlib
$(SQLITE_TARGET_CONFIGURE): | zlib
endif
# Buildroot's openal: its cmake finds D-Bus, for RTKit's real-time priority (D-Bus loaded with dlopen)
ifeq ($(BR2_PACKAGE_OPENAL)$(BR2_PACKAGE_DBUS),yy)
OPENAL_DEPENDENCIES += dbus
$(OPENAL_TARGET_CONFIGURE): | dbus
endif
endif

# RetroArch's master (local.mk pins it): the fork's patches instead of upstream's, which are for v1.22.2 (only for
# its patch step: private, so packages built before it keep theirs), the fork's hash file (both tarballs and
# COPYING), its credentials file unsealed (ra-keychain folds it back into retroarch.cfg), and its own commit in
# its version (master still calls itself 1.22.2)
ifeq ($(BR2_PACKAGE_KIWAMI_RETROARCH_MASTER),y)
$(RETROARCH_TARGET_PATCH): private BR2_GLOBAL_PATCH_DIR = $(BR2_EXTERNAL_KIWAMI_PATH)/patches
RETROARCH_HASH_FILES = $(BR2_EXTERNAL_KIWAMI_PATH)/patches/retroarch/retroarch.hash
RETROARCH_CONF_OPTS += --disable-keychain
$(RETROARCH_TARGET_BUILD): private export GIT_VERSION = $(shell echo $(RETROARCH_VERSION) | cut -c1-7)
# Settings > System > About names RetroArch's version from /usr/share/retroarch/version, which upstream's install
# writes from RETROARCH_VERSION (here the whole commit, 40 characters): master and its short commit instead
define KIWAMI_RETROARCH_VERSION_FILE
	printf 'master %s\n' '$(shell echo $(RETROARCH_VERSION) | cut -c1-7)' > $(TARGET_DIR)/usr/share/retroarch/version
endef
RETROARCH_POST_INSTALL_TARGET_HOOKS += KIWAMI_RETROARCH_VERSION_FILE
# what master builds in by default and Banana can't use: the MCP server, its own NFS and SMB clients (nothing that
# listens or mounts on the network), media decoders (WAV for the menu sounds and FLAC, which CHD's CD audio needs,
# stay), the RAR reader (RAR games aren't listed) and Feral GameMode (no daemon)
RETROARCH_CONF_OPTS += --disable-mcp --disable-retronfs --disable-retrosmb --disable-smbclient \
	--disable-rav1 --disable-rvp9 --disable-rmp4 --disable-rwebm --disable-rmpeg1 \
	--disable-raac --disable-ropus --disable-rvorbis --disable-rmp3 --disable-rmodtracker \
	--disable-rrar --disable-gamemode
endif
