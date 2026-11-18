# RetroArch's GLSL shaders (upstream's retroarch-glsl-shaders) as libretro wrote them. Upstream's install rewrites every
# shader's vertex stage to highp (RETROARCH_GLSL_SHADERS_VERTEX_HIGHP), which most of them then fail with: a
# COMPAT_PRECISION defined twice with two bodies, or uniforms at highp in the vertex stage and mediump in the fragment
# stage. After it, kiwami/glsl-shaders.sh copies the sources back and raises only the vertex stages whose fragment stage
# is highp, the shaders the rewrite was for. Buildroot expands the hooks when the install step runs, after upstream's
# .mk is read. Included by kiwami/external.mk, and by kiwami/tests/glsl-shaders-shim.mk, which runs upstream's install
# and this hook outside Buildroot.
ifeq ($(BR2_PACKAGE_KIWAMI_GLSL_SHADERS_PRECISION),y)
define KIWAMI_GLSL_SHADERS_PRECISION
	sh $(BR2_EXTERNAL_KIWAMI_PATH)/glsl-shaders.sh $(@D) $(TARGET_DIR)/usr/share/libretro/shaders/glsl
endef
RETROARCH_GLSL_SHADERS_POST_INSTALL_TARGET_HOOKS += KIWAMI_GLSL_SHADERS_PRECISION
endif
