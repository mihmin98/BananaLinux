# The fork's package override file (BR2_PACKAGE_OVERRIDE_FILE, set in configs/kiwami.fragment): Buildroot reads it
# after .config and before any package, so a version set here with override beats the package's own.
# RetroArch's master over upstream's v1.22.2 (BR2_PACKAGE_KIWAMI_RETROARCH_MASTER); its patches, hash file, options
# and hotkeys are in external.mk. A bump: the patches still apply, the hash, kiwami/tests/retroarch-pin.sh's stamps,
# DEPENDENCIES.md.
ifeq ($(BR2_PACKAGE_KIWAMI_RETROARCH_MASTER),y)
override RETROARCH_VERSION = 71f521e726c2b1886ee7a2484195ab63abc92127
endif
# the output's own override file, Buildroot's default, still read
-include $(CONFIG_DIR)/local.mk
