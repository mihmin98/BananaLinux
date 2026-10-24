include $(sort $(wildcard $(BR2_EXTERNAL_KIWAMI_PATH)/package/*/*.mk))

# RetroArch's master (local.mk pins it): the fork's patches instead of upstream's, which are for v1.22.2 (only for
# its patch step: private, so packages built before it keep theirs), the fork's hash file (both tarballs and
# COPYING), its credentials file unsealed (ra-keychain folds it back into retroarch.cfg), and its own commit in
# its version (master still calls itself 1.22.2)
ifeq ($(BR2_PACKAGE_KIWAMI_RETROARCH_MASTER),y)
$(RETROARCH_TARGET_PATCH): private BR2_GLOBAL_PATCH_DIR = $(BR2_EXTERNAL_KIWAMI_PATH)/patches
RETROARCH_HASH_FILES = $(BR2_EXTERNAL_KIWAMI_PATH)/patches/retroarch/retroarch.hash
RETROARCH_CONF_OPTS += --disable-keychain
$(RETROARCH_TARGET_BUILD): private export GIT_VERSION = $(shell echo $(RETROARCH_VERSION) | cut -c1-7)
# master's hotkeys that v1.22.2 doesn't have, for Settings > Other systems > RetroArch hotkeys Off: run-retroarch (a
# kiwami: hook) adds them to upstream's list from /usr/share/kiwami/ra-hotkeys. The build stops when master's
# hotkeys aren't upstream's list and these (kiwami/retroarch-hotkeys.sh). run-retroarch is read where the overlay
# comes from: the overlay isn't in the target yet when RetroArch is installed.
KIWAMI_RETROARCH_HOTKEYS = video_filter_toggle headset_recenter laser_pointer_toggle headset_view_toggle
define KIWAMI_RETROARCH_HOTKEYS_INSTALL
	$(BR2_EXTERNAL_KIWAMI_PATH)/retroarch-hotkeys.sh $(@D)/configuration.c \
		$(BR2_EXTERNAL_BANANA_PATH)/../system/rootfs/usr/libexec/banana/run-retroarch \
		$(TARGET_DIR)/usr/share/kiwami/ra-hotkeys $(KIWAMI_RETROARCH_HOTKEYS)
endef
RETROARCH_POST_INSTALL_TARGET_HOOKS += KIWAMI_RETROARCH_HOTKEYS_INSTALL
endif
