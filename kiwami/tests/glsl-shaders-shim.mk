# Upstream's retroarch-glsl-shaders package outside Buildroot, for kiwami/tests/glsl-shaders.sh: its .mk and the fork's
# hook (kiwami/glsl-shaders.mk), with what they use of Buildroot's defined here, and its install step run as
# Buildroot's pkg-generic.mk runs it (the install commands, then the post-install hooks):
#   make -s -f kiwami/tests/glsl-shaders-shim.mk version
#   make -s -f kiwami/tests/glsl-shaders-shim.mk SRC=<source> TARGET_DIR=<dir> [BR2_PACKAGE_KIWAMI_GLSL_SHADERS_PRECISION=y] <source>/.install
# Run from the repository's root.
github = https://github.com/$(1)/$(2)/archive/$(3)
generic-package =
define sep


endef
BR2_EXTERNAL_KIWAMI_PATH := $(CURDIR)/kiwami

include buildroot/package/retroarch-glsl-shaders/retroarch-glsl-shaders.mk
include kiwami/glsl-shaders.mk

.PHONY: version
version:
	@echo $(RETROARCH_GLSL_SHADERS_VERSION)

# the target is in the source, so $(@D) is the source folder, as a package's build folder is in Buildroot
$(SRC)/.install:
	$(RETROARCH_GLSL_SHADERS_INSTALL_TARGET_CMDS)
	$(foreach hook,$(RETROARCH_GLSL_SHADERS_POST_INSTALL_TARGET_HOOKS),$(call $(hook))$(sep))
