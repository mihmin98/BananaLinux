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
endif
