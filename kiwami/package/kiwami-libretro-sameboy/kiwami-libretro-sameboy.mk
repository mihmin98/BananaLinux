################################################################################
#
# kiwami-libretro-sameboy
#
################################################################################

# libretro's SameBoy (the Game Boy and Game Boy Color, accurate): its libretro makefile, with the open boot ROMs
# prebuilt in its tree
KIWAMI_LIBRETRO_SAMEBOY_VERSION = aa158a889a48b538a0302873704a34577c8eb67d
KIWAMI_LIBRETRO_SAMEBOY_SITE = $(call github,libretro,SameBoy,$(KIWAMI_LIBRETRO_SAMEBOY_VERSION))
KIWAMI_LIBRETRO_SAMEBOY_LICENSE = MIT
KIWAMI_LIBRETRO_SAMEBOY_LICENSE_FILES = LICENSE

# Its makefile turns the prebuilt boot ROMs into C with hexdump, which a Buildroot host needn't have: written here
# with od instead (the makefile's own way on Haiku), and make finds them up to date
define KIWAMI_LIBRETRO_SAMEBOY_BOOT_ROMS
	for b in $(@D)/BootROMs/prebuilt/*_boot.bin; do \
		n=$$(basename $$b .bin); \
		{ echo "/* AUTO-GENERATED */"; echo "const unsigned char $$n[] = {"; \
		  od -A n -t x1 -v $$b | sed -e 's/^ /0x/' -e 's/ /, 0x/g' -e 's/$$/,/' | tr '\n' ' '; \
		  echo "};"; echo "const unsigned $${n}_length = sizeof($$n);"; } > $(@D)/libretro/$$n.c || exit 1; \
	done
endef
KIWAMI_LIBRETRO_SAMEBOY_PRE_BUILD_HOOKS += KIWAMI_LIBRETRO_SAMEBOY_BOOT_ROMS

# its version is its own commit, not the one `git rev-parse` finds (this repository's, around output/)
define KIWAMI_LIBRETRO_SAMEBOY_BUILD_CMDS
	$(TARGET_MAKE_ENV) $(MAKE) -C $(@D)/libretro platform=unix CC="$(TARGET_CC)" \
		GIT_VERSION='" $(shell echo $(KIWAMI_LIBRETRO_SAMEBOY_VERSION) | cut -c1-7)"'
endef

define KIWAMI_LIBRETRO_SAMEBOY_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0644 $(@D)/libretro/sameboy_libretro.so $(TARGET_DIR)/usr/lib/libretro/sameboy_libretro.so
endef

$(eval $(generic-package))
