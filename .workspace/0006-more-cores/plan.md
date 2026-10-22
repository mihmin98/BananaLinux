# 0006 More cores: plan

Issue: #6 · Branch: `k/0006-more-cores` · Investigation: [investigation.md](investigation.md) · Status: approved

## Summary

Three more RetroArch cores in the image, each a second choice for a system Banana already has: Mesen for the NES,
SameBoy for the Game Boy and the Game Boy Color, Genesis Plus GX for the Mega Drive. Settings > Other systems gets one
row per system (NES, Game Boy, Game Boy Color, Mega Drive) after upstream's Game Boy Advance row. Each row writes
upstream's `core.<system>` in `banana/settings.conf`, which `run-retroarch` already reads, and shows only when the
fork's core is installed. The defaults stay upstream's cores, and each core keeps its own saves, as the rows' help
says.

The approach chosen in the investigation: one small package per core in the fork's Buildroot tree
(`kiwami/package/kiwami-libretro-<core>/`, pinned by commit, with a `.hash`), switched on in
`kiwami/configs/kiwami.fragment`. The rows are in `ui/src/kiwami/kiwami.cpp`, reached through one new hook line in
`ui/src/settings.cpp`. Upstream's code saves, loads and resets them, since they're plain choice rows on upstream's
keys. No change to `systems.conf` or upstream's core packages; the existing cores are not bumped. (Since M3, 22 Oct
2026: SameBoy's sound is at 96 kHz, and one `kiwami:` hook in `run-retroarch` gives Mesen RetroArch's threaded video
through its per-core override; see *Deviations*.)

### Refinements over the investigation

- **One step per core** (the user's choice): three package steps, each built and checked alone with its
  `DEPENDENCIES.md` row, then the rows, then the final check.
- **SameBoy's boot ROMs are converted with `od`** (the user's choice): a pre-build hook writes `libretro/*_boot.c`
  from `BootROMs/prebuilt/*_boot.bin`, using the pipeline SameBoy's makefile itself uses on Haiku
  (`libretro/Makefile:368-379`). Make then finds them up to date and never calls `hexdump`, which a Buildroot host
  needn't have (Buildroot's `host-util-linux` builds no programs: `--disable-all-programs`).
- **Only SameBoy and Genesis Plus GX stamp `GIT_VERSION`.** The investigation said all three do, but Mesen's
  `Libretro/Makefile` has no stamp; only its `jni/Android.mk` does. The other two get `GIT_VERSION` passed
  explicitly, so their version isn't Banana's commit (`git rev-parse` inside `output/build/` finds this repository).
- **The test's libretro folder is `KIWAMI_LIBRETRO`** (a folder, `/usr/lib/libretro` when unset), as
  `KIWAMI_TAILSCALE` is for Tailscale's helper.
- **banana-ui logs the fork's rows**, one line per group each time the list is built (at start, after a reset, after
  switching the DS emulator): `banana-ui: kiwami: rows in Other systems: core.nes core.gb core.gbc core.megadrive`
  (or `none`), and `banana-ui: kiwami: rows in Network: kiwami.tailscale`. The headless test asserts on them, and on
  the device `banana-ui.log` shows which of the fork's cores the image has.
- **Headless, the fork's rows follow VSync, not Game Boy Advance.** Upstream's GBA row checks an absolute path
  (`/usr/lib/libretro/mgba_libretro.so`, `settings.cpp:401`) that a build host doesn't have. On the device they follow
  GBA (M1).
- **`docs/FORK.md`'s merge checks** get `buildroot/package/retroarch/cores.in` and `run-retroarch` among the files
  read by hand after a merge. If upstream adds one of these cores, the fork drops its package and row. If
  `core.<system>` changes, the rows have to follow.

## Acceptance criteria

- **A1** (image): given a board configured with the fork (`rgds`, `rgds-plus`, `universal`), the three options stay
  set after Buildroot resolves the dependencies. The image then has `/usr/lib/libretro/mesen_libretro.so`,
  `sameboy_libretro.so` and `genesis_plus_gx_libretro.so`: aarch64 shared objects that need nothing beyond the
  toolchain's libc, libm, libstdc++ and libgcc_s, versioned with their own commits (not Banana's), with Genesis Plus
  GX built without its physical CD-ROM code.
- **A2** (rows shown): given the three cores installed, Settings > Other systems ends with NES, Game Boy, Game Boy
  Color and Mega Drive, in that order, after upstream's rows (after Game Boy Advance on the device). Each row offers
  upstream's core first and the fork's second: FCEUmm / Mesen, Gambatte / SameBoy, Gambatte / SameBoy, PicoDrive /
  Genesis Plus GX. Its help names both and says each keeps its own saves.
- **A3** (defaults): given a card with no `core.<system>` line, or upstream's value, each row shows upstream's core.
  Opening Settings and moving through the rows writes nothing.
- **A4** (change): when the user changes a row (Right, Left), `banana/settings.conf` gets `core.nes = mesen`,
  `core.gb = sameboy`, `core.gbc = sameboy` or `core.megadrive = genesis_plus_gx`; changing it again writes
  upstream's core back.
- **A5** (not installed): given an image without one of the cores (an older slot, upstream's image, a build without
  the option), that core's rows don't show (without SameBoy, neither Game Boy row). With none of them, Other systems
  has only upstream's rows and renders as it did before the feature. A card's `core.<system>` naming a missing core
  is left as it is.
- **A6** (fallback): given `core.<system>` naming a core the image lacks, a game of that system starts with
  `systems.conf`'s core, and the log says which core ran (upstream's `run-retroarch`, unchanged).
- **A7** (reset): Settings > System > Reset emulator settings puts the four back to `fceumm`, `gambatte`, `gambatte`
  and `picodrive`, and the rows are still shown after it.
- **A8** (Tailscale): Network's Tailscale row is where it was and only in Network; Other systems has no Tailscale row
  and Network no core row.
- **A9** (runs): given a row set to the fork's core, a game of that system starts with it (`banana-ui.log`:
  `run-retroarch: core mesen (Settings > Other systems), not fceumm`). It plays with sound and the controls, and
  quitting goes back to the menu.
- **A10** (speed): demanding games run at full speed (60 fps, sound without crackle) with each of the fork's cores,
  under the systems' `powersave` profile: SameBoy with its sound at 96 kHz (step 5), Mesen with RetroArch's threaded
  video (step 6). Any shortfall is recorded and handled as a deviation before the feature is merged. (Changed on
  22 Oct 2026, see *Deviations*.)
- **A16** (Mesen's threaded video): a Mesen game runs with `video_threaded = "true"` from Mesen's own override file
  (`/data/retroarch/config/Mesen/Mesen.cfg`), written by the fork when the file has no `video_threaded` line, so a
  user's own choice there is kept. The card's `retroarch.cfg` keeps `video_threaded = "false"`, and every other core
  runs with it. No core gets clocks of its own: each runs at its system's profile as upstream has it.
- **A11** (saves per core): after switching a system to the other core and back, the first core's in-game saves and
  states are found again. A Quick resume state of one core is never loaded by the other.
- **A12** (sleep and screens): closing the lid mid-game with each of the fork's cores sleeps, and opening it carries
  on with sound. The fork's cores run on the top or the bottom panel, with the game panel on or off.
- **A13** (RetroAchievements): with an account set, a game with achievements is identified with each of the fork's
  cores.
- **A14** (footprint): the rootfs grows by a measured amount and the squashfs still fits its 256 MB slot with room
  to spare. Nothing new runs while idle, and the menu's frame time is unchanged.
- **A15** (upstream's look): `make test`'s renders are identical to `kiwami`'s before the feature.

## Risks

- **Performance and footprint.**
  - The rootfs grows by an estimated 10-20 MB unpacked, about half of that in LZ4. The baseline is
    `output/rgds-plus/images/rootfs.squashfs` at 169,390,080 bytes (21 Oct 2026, `kiwami` with Tailscale), and the
    nine cores take 24,556,328 bytes. Each package step measures its `.so`, and the final check measures the
    squashfs (A14).
  - Nothing runs while idle. The menu's work is four more rows and three `access()` calls each time the list is
    built: at start, after a reset, after switching the DS emulator. The frame is unchanged (`make test`'s bench,
    A15).
  - Build time grows by several minutes, mostly Mesen's C++ at `-O3`.
  - In a game, Mesen and Genesis Plus GX do more per frame than FCEUmm and PicoDrive under `powersave` (1416 MHz cap):
    only the device shows it. M3 measures it (A10), and a shortfall becomes a deviation (a hook into the launch path).
- **Conflicts with upstream.**
  - One new hook line in `ui/src/settings.cpp`, a file upstream changes often. It's one line after the GBA row, and
    its conflict is resolved by putting the line back after that row.
  - Upstream adding one of these cores itself (both packages installing the same `.so`), or its own row for one of
    these systems (two rows on one key): `docs/FORK.md`'s merge checks read `cores.in` (step 4), and the merge skill
    flags it. The fork then drops its package and row.
  - Upstream renaming the group "Other systems" or "Network", which `rows()` matches by name: the rows would vanish.
    `cores-ui.sh` and `tailscale-ui.sh` fail on that.
  - Upstream changing how `core.<system>` is read (`run-retroarch`) or reset (`settings_defaults.cpp`): `cores-ui.sh`
    covers the reset, and the merge checks read `run-retroarch`.
- **The builds.** Each core's makefile guesses the platform from the build host (`uname`). Passing
  `platform=unix` and the target's compilers, as upstream's packages do, covers it; each step checks the `.so` with
  `file` and `readelf` (A1). SameBoy's `od` conversion could differ from `hexdump`'s: step 2 checks the generated
  arrays against the `.bin` files byte for byte. GitHub's archive tarballs can change, though rarely: the `.hash`
  then fails the download, which is the point of having it.
- **Saves.** Per core, as RetroArch keeps them. A user moving a save between cores by hand may find it doesn't load
  (Game Boy clocks, Mega Drive SRAM); the rows' help says each core keeps its own (A11).
- **Hardware and boards.** The three are plain software cores drawn through RetroArch's GL driver, the same on both
  boards and `universal`. Only the RG DS Plus is at hand: the RG DS gets configure and build checks only, which
  covers everything board-specific these cores could touch (none).
- **Mesen's repository is in maintenance** (last commit 20 Apr 2026): stable, slow to get fixes. Noted in
  `DEPENDENCIES.md`.
- **A dev host with libretro cores installed** (`/usr/lib/libretro/mesen_libretro.so` from a distribution package)
  would show the rows in `make test`'s runs, as upstream's GBA row does with mGBA. CI's runner and this machine have
  none. The fork's own test always sets `KIWAMI_LIBRETRO`.

## Steps

Each step is one commit, and leaves the tree building and `make test` passing.

### 1. Mesen, a second NES core

- **Files**: `kiwami/package/kiwami-libretro-mesen/Config.in` (new), `kiwami/package/kiwami-libretro-mesen/kiwami-libretro-mesen.mk` (new),
  `kiwami/package/kiwami-libretro-mesen/kiwami-libretro-mesen.hash` (new), `kiwami/Config.in` (changed),
  `kiwami/configs/kiwami.fragment` (changed), `kiwami/DEPENDENCIES.md` (changed).
- **Change**:
  - `Config.in`: `BR2_PACKAGE_KIWAMI_LIBRETRO_MESEN`, `depends on BR2_PACKAGE_RETROARCH` and
    `depends on BR2_INSTALL_LIBSTDCPP` (it's C++). The help: the NES core, chosen in Settings > Other systems > NES,
    upstream's FCEUmm staying the default; the repository's URL.
  - `kiwami-libretro-mesen.mk`, in the shape of upstream's `libretro-snes9x.mk`:
    ```make
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
    ```
  - `.hash`: `# Locally computed`, the tarball's sha256 (`kiwami-libretro-mesen-0102910c….tar.gz`) and `LICENSE`'s,
    as `kiwami-tailscale.hash` has. Computed by downloading once without the file
    (`make kiwami-libretro-mesen-source-rgds-plus`) and running `sha256sum` over the tarball and the extracted
    `LICENSE`.
  - `kiwami/Config.in`: `source` the package after Tailscale's.
  - `kiwami/configs/kiwami.fragment`: a comment (the fork's second cores, chosen in Settings > Other systems) and
    `BR2_PACKAGE_KIWAMI_LIBRETRO_MESEN=y`.
  - `kiwami/DEPENDENCIES.md`, the fork's table: Mesen (libretro), commit `0102910`, 20 Apr 2026, the `.mk`'s
    `KIWAMI_LIBRETRO_MESEN_VERSION` and the `.hash`, https://github.com/libretro/Mesen. Notes: GPL-3.0; the NES's
    second core (`mesen_libretro.so`, saves in `/data/saves/Mesen/`); the repository is in maintenance; ROCKNIX ships
    the same commit; a bump recomputes the hash.
- **Tests**:
  - `kiwami/tests/ci.sh .buildroot`: every board configures, keeping `BR2_PACKAGE_KIWAMI_LIBRETRO_MESEN=y`.
  - `make kiwami-libretro-mesen-rgds-plus` builds and installs it. On `output/rgds-plus/target/usr/lib/libretro/mesen_libretro.so`:
    `file` says ELF 64-bit ARM aarch64 shared object; `readelf -d` lists only `libstdc++`, `libm`, `libgcc_s` and
    `libc` as `NEEDED`; `readelf --dyn-syms` exports `retro_init` and `retro_run`; its size (`du -b`) is noted in the
    step's overview.
  - `make test` (unchanged, passing).
- **Commit**: `More cores: Mesen, a second NES core, in the image`

### 2. SameBoy, a second Game Boy and Game Boy Color core

- **Files**: `kiwami/package/kiwami-libretro-sameboy/Config.in` (new), `kiwami/package/kiwami-libretro-sameboy/kiwami-libretro-sameboy.mk` (new),
  `kiwami/package/kiwami-libretro-sameboy/kiwami-libretro-sameboy.hash` (new), `kiwami/Config.in` (changed),
  `kiwami/configs/kiwami.fragment` (changed), `kiwami/DEPENDENCIES.md` (changed).
- **Change**:
  - `Config.in`: `BR2_PACKAGE_KIWAMI_LIBRETRO_SAMEBOY`, `depends on BR2_PACKAGE_RETROARCH`. The help: the Game Boy
    and Game Boy Color core with its own open boot ROMs (no BIOS needed), chosen per system in Settings > Other
    systems, upstream's Gambatte staying the default.
  - `kiwami-libretro-sameboy.mk`:
    ```make
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
    ```
    and the install of `$(@D)/libretro/sameboy_libretro.so` (the path checked after the first build) to
    `/usr/lib/libretro/sameboy_libretro.so`. The five ROMs the hook converts are the five `libretro/Makefile.common`
    lists (`agb`, `cgb`, `dmg`, `sgb`, `sgb2`).
  - `.hash`: the tarball's and `LICENSE`'s sha256, computed as in step 1.
  - `kiwami/Config.in`: `source` it. `kiwami/configs/kiwami.fragment`: `BR2_PACKAGE_KIWAMI_LIBRETRO_SAMEBOY=y`.
  - `kiwami/DEPENDENCIES.md`: SameBoy (libretro), commit `aa158a8`, 23 Jul 2026, https://github.com/libretro/SameBoy.
    Notes: MIT; the Game Boy's and Game Boy Color's second core (`sameboy_libretro.so`, saves in
    `/data/saves/SameBoy/`); its boot ROMs are converted with `od` in a pre-build hook, and a bump checks that
    `libretro/Makefile`'s `%_boot.c` rule and `Makefile.common`'s list still match it; `GIT_VERSION` is passed; ROCKNIX
    ships the same commit; a bump recomputes the hash.
- **Tests**:
  - `kiwami/tests/ci.sh .buildroot`.
  - `make kiwami-libretro-sameboy-rgds-plus`. Its build log shows no `hexdump`. Each of the five generated
    `libretro/*_boot.c` holds the `.bin`'s bytes exactly: checked once by hand, with a host `cc` program that
    includes the `.c` and writes the array out, `cmp`'d with the `.bin`. Then the same `file`, `readelf -d` (`libm`,
    `libc`) and `--dyn-syms` checks as step 1, and `strings` finds ` aa158a8` and not this repository's `HEAD`. Its
    size is noted.
  - `make test`.
- **Commit**: `More cores: SameBoy, a second Game Boy and Game Boy Color core, in the image`

### 3. Genesis Plus GX, a second Mega Drive core

- **Files**: `kiwami/package/kiwami-libretro-genesis-plus-gx/Config.in` (new),
  `kiwami/package/kiwami-libretro-genesis-plus-gx/kiwami-libretro-genesis-plus-gx.mk` (new),
  `kiwami/package/kiwami-libretro-genesis-plus-gx/kiwami-libretro-genesis-plus-gx.hash` (new), `kiwami/Config.in` (changed),
  `kiwami/configs/kiwami.fragment` (changed), `kiwami/DEPENDENCIES.md` (changed).
- **Change**:
  - `Config.in`: `BR2_PACKAGE_KIWAMI_LIBRETRO_GENESIS_PLUS_GX`, `depends on BR2_PACKAGE_RETROARCH`. The help: the
    Mega Drive core (accurate, its sound in particular), chosen in Settings > Other systems > Mega Drive, upstream's
    PicoDrive staying the default.
  - `kiwami-libretro-genesis-plus-gx.mk`: version `aa74d79634cd230f65cce4180e92688e53883d2f`, `$(call
    github,libretro,Genesis-Plus-GX,…)`, license `Genesis Plus GX (non-commercial)` (as upstream writes PicoDrive's
    and Snes9x's), `LICENSE_FILES = LICENSE.txt`. The build:
    ```make
    # HAVE_CDROM=0: its unix build turns on reading a physical CD-ROM drive on Linux (Makefile.libretro), which the
    # handheld hasn't got (as PCSX-ReARMed's HAVE_PHYSICAL_CDROM=0 and RetroArch's --disable-cdrom upstream)
    define KIWAMI_LIBRETRO_GENESIS_PLUS_GX_BUILD_CMDS
    	$(TARGET_MAKE_ENV) $(MAKE) -C $(@D) -f Makefile.libretro platform=unix HAVE_CDROM=0 CC="$(TARGET_CC)" \
    		GIT_VERSION='" $(shell echo $(KIWAMI_LIBRETRO_GENESIS_PLUS_GX_VERSION) | cut -c1-7)"'
    endef
    ```
    and the install of `$(@D)/genesis_plus_gx_libretro.so` to `/usr/lib/libretro/genesis_plus_gx_libretro.so`.
  - `.hash`: the tarball's and `LICENSE.txt`'s sha256.
  - `kiwami/Config.in`: `source` it. `kiwami/configs/kiwami.fragment`: `BR2_PACKAGE_KIWAMI_LIBRETRO_GENESIS_PLUS_GX=y`.
  - `kiwami/DEPENDENCIES.md`: Genesis Plus GX (libretro), commit `aa74d79`, 16 Oct 2026,
    https://github.com/libretro/Genesis-Plus-GX. Notes: non-commercial license; the Mega Drive's second core
    (`genesis_plus_gx_libretro.so`, saves in `/data/saves/Genesis Plus GX/`); built with `HAVE_CDROM=0` and
    `GIT_VERSION` passed; newer than ROCKNIX's `b7e79b3`; a bump recomputes the hash.
- **Tests**:
  - `kiwami/tests/ci.sh .buildroot`.
  - `make kiwami-libretro-genesis-plus-gx-rgds-plus`. The build log compiles no libretro-common `cdrom` source and
    passes no `-DHAVE_CDROM`. The same `file`, `readelf` and `--dyn-syms` checks; `strings` finds `v1.7.4 aa74d79`
    (its `library_version`). Its size is noted.
  - `make test`.
- **Commit**: `More cores: Genesis Plus GX, a second Mega Drive core, in the image`

### 4. A row per system in Settings > Other systems

- **Files**: `ui/src/kiwami/kiwami.cpp` (changed), `ui/src/kiwami/kiwami.h` (changed), `ui/src/settings.cpp` (hook),
  `kiwami/tests/cores-ui.sh` (new), `kiwami/tests/ci.sh` (changed), `kiwami/DEPENDENCIES.md` (changed),
  `docs/FORK.md` (changed).
- **Change**:
  - `ui/src/settings.cpp`, the hook: one line after the Game Boy Advance row (`:401-402`, the `if (access(…mgba…))
    add(O, …)`), before `const char* W = "Network";`:
    ```cpp
    		Kiwami::rows(*this, O);	// kiwami: the fork's cores
    ```
    The existing `Kiwami::rows(*this, W);	// kiwami: Tailscale` stays as it is.
  - `ui/src/kiwami/kiwami.cpp`, `Settings::Kiwami::rows()` branches on the group's name. "Other systems" gets the
    core rows. "Network" gets the Tailscale row as today (no longer whatever the group). Then one log line for the
    group. Sketch:
    ```cpp
    // the libretro cores' folder: KIWAMI_LIBRETRO in the environment (the headless test's stand-in), else the image's
    static std::string libretro()
    {
    	const char* e = getenv("KIWAMI_LIBRETRO");
    	return e && *e ? e : "/usr/lib/libretro";
    }

    // Other systems: the fork's second cores, a row per system shown when the core is in the image. Upstream's
    // core.<system> keys (run-retroarch reads them) as upstream's Game Boy Advance row has, so upstream loads, saves
    // and resets them as any choice row; upstream's core first, its default
    static const struct { const char* key; const char* label; const char* help; const char* names[2]; const char* values[2]; } CORES[] = {
    	{ "core.nes", "NES", "The emulator for NES games. FCEUmm: light and fast. Mesen: the most accurate, and heavier. "
    		"Each keeps its own saves and save states: switching back finds them again.", { "FCEUmm", "Mesen" }, { "fceumm", "mesen" } },
    	{ "core.gb", "Game Boy", "The emulator for Game Boy games. Gambatte: light and accurate. SameBoy: the most "
    		"accurate, and heavier. Each keeps …", { "Gambatte", "SameBoy" }, { "gambatte", "sameboy" } },
    	{ "core.gbc", "Game Boy Color", "The emulator for Game Boy Color games. …", { "Gambatte", "SameBoy" }, { "gambatte", "sameboy" } },
    	{ "core.megadrive", "Mega Drive", "The emulator for Mega Drive games. PicoDrive: the fastest. Genesis Plus GX: "
    		"more accurate, with the better sound, and heavier. Each keeps …", { "PicoDrive", "Genesis Plus GX" }, { "picodrive", "genesis_plus_gx" } },
    };

    void Settings::Kiwami::rows(Settings& s, const char* group)
    {
    	if (!s.kw) s.kw = std::make_shared<Kiwami>(s);	// (at the first build(), in Settings' constructor)
    	std::string keys;
    	if (strcmp(group, "Other systems") == 0)
    		for (auto& c : CORES)
    		{
    			if (access((libretro() + "/" + c.values[1] + "_libretro.so").c_str(), F_OK) != 0) continue;
    			Item it;
    			it.section = group; it.icon = "gamepad"; it.label = c.label; it.help = c.help;
    			it.file = s.data + "/settings.conf"; it.key = c.key;
    			it.names = { c.names[0], c.names[1] }; it.values = { c.values[0], c.values[1] };
    			s.items.push_back(it);
    			keys += std::string(keys.empty() ? "" : " ") + c.key;
    		}
    	else if (strcmp(group, "Network") == 0)
    	{
    		… the Tailscale row as today …
    		keys = "kiwami.tailscale";
    	}
    	fprintf(stderr, "banana-ui: kiwami: rows in %s: %s\n", group, keys.empty() ? "none" : keys.c_str());
    }
    ```
    `owns()` is unchanged: it claims `kiwami.` keys only, so the core rows stay upstream's choice rows (`load()`,
    `save()`, `change()`, the reset). The help texts above are the proposal; the wording is settled in this step's
    review against the shots.
  - `ui/src/kiwami/kiwami.h`: the header comment gets the rows of Other systems (NES, Game Boy, Game Boy Color, Mega
    Drive, on `core.<system>`, shown when the core is in `/usr/lib/libretro` or `KIWAMI_LIBRETRO`), and "Its rows'
    keys start with "kiwami."" becomes "Its own rows' keys start with "kiwami." (the core rows are on upstream's
    `core.<system>`, handled by upstream as its own)".
  - `kiwami/tests/cores-ui.sh` (new), in the shape of `tailscale-ui.sh`. A fixture (`ui/tests/fixture.sh`) and three
    stand-in folders of empty files under `$OUT` (`${TMPDIR:-/tmp}/kiwami-cores` by default): `all` (the three
    `.so`), `sameboy` (`sameboy_libretro.so` only), `none` (empty). Each run of banana-ui headless writes its log
    and shots. Its script starts from `home 0; press start; press r; press r; press r; press a` (Other systems'
    list, on Screen) or five R's for System. At 640x480 and 1024x768:
    - `rows` (`all`, a fresh card): the log has exactly one `banana-ui: kiwami: rows in Other systems: core.nes
      core.gb core.gbc core.megadrive` and one `banana-ui: kiwami: rows in Network: kiwami.tailscale`, and no other
      `rows in` line. Down seven times reaches NES; a shot on each of the four rows, each present and not blank
      (ImageMagick, as `tailscale-ui.sh` checks), each one's top panel (the help) different from the next's. The
      card's `settings.conf` after the run is byte-for-byte the one banana-ui normalized at startup (a first run with
      `home 0` only makes it): browsing writes nothing (A3).
    - `set` (`all`): on each of the four rows, Right; the log has four `press right: settings result 2`, and
      `settings.conf` has `core.nes = mesen`, `core.gb = sameboy`, `core.gbc = sameboy`,
      `core.megadrive = genesis_plus_gx`, with `core.gba`, `core.snes`, `core.psx` and `core.n64` as before (A2's
      order, A3's starting point, A4).
    - `back` (`all`, the card set to the four fork cores): Right on NES and Game Boy, Left on Game Boy Color and Mega
      Drive; `settings.conf` has `fceumm`, `gambatte`, `gambatte`, `picodrive` (A4).
    - `last` (`all`): Up from Screen (the list goes round) and Right: only `core.megadrive` changes, to
      `genesis_plus_gx`. Mega Drive is the list's last row, with no Tailscale row after it (A2, A8).
    - `sameboy` (`sameboy`): `rows in Other systems: core.gb core.gbc` (A5).
    - `none` (`none`, the card set to `core.nes = mesen`): `rows in Other systems: none`; Up from Screen and Right
      sets `ra_vsync = off` (VSync is the last row); `core.nes = mesen` is still there (A5).
    - `reset` (`all`, the card set to the four fork cores and `core.gba = mgba`): System, Down eight times (Reset
      emulator settings, as `ui/tests/run.sh` reaches it), A, A. `settings.conf` has `core.nes = fceumm`,
      `core.gb = gambatte`, `core.gbc = gambatte`, `core.megadrive = picodrive`, `core.gba = gpsp`. The log has the
      `rows in Other systems: core.nes core.gb core.gbc core.megadrive` line twice (at start and after the reset's
      rebuild) (A7).
    Prints `cores-ui: all checks passed ($OUT)`, exits 1 on any `FAIL`.
  - `kiwami/tests/ci.sh`: runs `kiwami/tests/cores-ui.sh` when `ui/banana-ui` is built, as it does `tailscale-ui.sh`
    and `7z-ui.sh` (`cores-ui: skipped, no ui/banana-ui (make ui)` otherwise).
  - `kiwami/DEPENDENCIES.md`, upstream's table, RetroArch's row: its last column adds that the core rows' help (each
    core keeps its own saves) relies on RetroArch's per-core save and state folders (`sort_savefiles_enable`,
    `sort_savestates_enable`, on by default in `config.def.h`, not set in Banana's `retroarch.cfg`).
  - `docs/FORK.md`, *Taking in upstream*, step 3: `buildroot/package/retroarch/cores.in` and
    `system/rootfs/usr/libexec/banana/run-retroarch` join the `git diff $OLD main` list, with a sentence. A core
    upstream adds that the fork already has (Mesen, SameBoy, Genesis Plus GX) means the fork drops its package and
    row. A change to how `run-retroarch` reads `core.<system>`, or a row upstream adds for NES, Game Boy, Game Boy
    Color or Mega Drive, means the fork's rows have to follow.
- **Tests**: `kiwami/tests/cores-ui.sh` itself, through `kiwami/tests/ci.sh .buildroot`; `kiwami/tests/tailscale-ui.sh`
  unchanged and passing (the Tailscale row in Network, A8); `make test` passing and its renders identical to before
  the step (A15, nothing sets `KIWAMI_LIBRETRO` there); `make -C ui` (the full build, with ALSA); the shots of the
  four rows looked at in the review.
- **Commit**: `More cores: a row per system in Settings > Other systems for the fork's cores`

### 5. SameBoy's sound at 96 kHz

Added on 22 Oct 2026 (*Deviations*).

- **Files**: `kiwami/package/kiwami-libretro-sameboy/0001-libretro-sound-at-96-kHz.patch` (new), `kiwami/DEPENDENCIES.md`
  (changed).
- **Change**:
  - The patch, in the shape of Tailscale's (a header saying what and why, `Upstream: not sent`): `libretro/libretro.c`'s
    `#define AUDIO_FREQUENCY 384000` becomes `96000`, the rate SameBoy's own macOS frontend uses
    (`Cocoa/Document.m:403`). The sound chip is emulated per cycle whatever the rate; the rate is only how often a
    sample is written out (`Core/apu.c`, `render()`), and the high-pass filter and DAC fade follow it
    (`GB_set_sample_rate`). At 384 kHz RetroArch resamples 8x more audio than it plays, and SameBoy renders 8x more
    samples: about 12x Gambatte's 32.8 kHz. 96 kHz is a quarter of that and keeps an anti-aliasing margin over 48 kHz.
    Buildroot applies the package folder's patches itself.
  - `kiwami/DEPENDENCIES.md`, SameBoy's row: the patch, that a bump checks it still applies and the define is still
    there, and why (M3).
- **Tests**: `make kiwami-libretro-sameboy-dirclean-rgds-plus kiwami-libretro-sameboy-rgds-plus`, one at a time (a
  call with several board targets built only the first, 22 Oct 2026): the log shows the patch applied, the built
  `libretro/libretro.c` has `#define AUDIO_FREQUENCY 96000`, and the step 2 checks on the `.so` still hold;
  `kiwami/tests/ci.sh .buildroot`. Then an update built (`make rgds-plus`) and M10's SameBoy part run on it, which
  settles step 6's SameBoy entry.
- **Commit**: `More cores: SameBoy's sound at 96 kHz, a quarter of the resampling`

### 6. Mesen's threaded video

Added on 22 Oct 2026 (*Deviations*).

- **Files**: `kiwami/rootfs/usr/libexec/kiwami/core-overrides` (new), `system/rootfs/usr/libexec/banana/run-retroarch`
  (hook), `kiwami/tests/core-overrides.sh` (new), `kiwami/tests/ci.sh` (changed), `kiwami/DEPENDENCIES.md` (changed),
  `docs/FORK.md` (changed).
- **Change**:
  - `run-retroarch`, the hook: one line after the `run-retroarch: core …` log line (`:21`), once the core that runs is
    known (`$c`), before RetroArch starts:
    ```sh
    [ -x /usr/libexec/kiwami/core-overrides ] && /usr/libexec/kiwami/core-overrides "$c"	# kiwami: the fork's cores' RetroArch settings
    ```
  - `core-overrides <core>`: for `mesen`, RetroArch's per-core override `<dir>/Mesen/Mesen.cfg` (`<dir>` is
    `retroarch.cfg`'s `rgui_config_directory`, `/data/retroarch/config`; `KIWAMI_RA_CONFIG` in the environment, the
    test's stand-in), the folder and file named after the core's `library_name` (`Libretro/libretro.cpp:1162`), as
    RetroArch looks for it (`configuration.c:4631`). When the file has no `video_threaded` line, the line
    `video_threaded = "true"` is appended (the folder and file made if missing) and one line goes to stderr for
    banana-ui's log (`kiwami: core-overrides: mesen draws on its own thread (…/Mesen/Mesen.cfg)`); a file that has
    one, whatever its value (the user's, from RetroArch's Quick Menu > Overrides), is left as it is. Any other core:
    nothing. SameBoy joins (`SameBoy/SameBoy.cfg`) only if M10 shows 96 kHz falling short, recorded as a deviation.
    Why threaded video: drawing and the wait for the panel move to a second thread, so the core's frame no longer has
    to fit in one refresh together with them (M3's notes: Mesen 60 fps under `powersave` with it, 30 without). It
    costs up to a frame of input delay, not noticed in M3's run; RetroArch stacks the override over
    `run-retroarch`'s `--appendconfig` files (`configuration.c:4637`), so the game panel's settings stay.
  - `kiwami/tests/core-overrides.sh`, against a temporary `KIWAMI_RA_CONFIG`: `mesen` on an empty folder makes
    `Mesen/Mesen.cfg` with exactly the line, and logs; a second run changes nothing (byte for byte) and logs nothing;
    a file with `video_threaded = "false"` is left as it is; a file with other lines gets the line appended, the others
    kept; `sameboy`, `genesis_plus_gx`, `fceumm`, `gambatte`, `picodrive`, `gpsp`, `mgba`, `snes9x` make nothing;
    `run-retroarch` has the hook after the core's log line (a `grep`). Run by `ci.sh`.
  - `kiwami/DEPENDENCIES.md`, RetroArch's row: Mesen's threaded video relies on its per-core overrides
    (`auto_overrides_enable`, on by default in `config.def.h:911`, and `rgui_config_directory` in Banana's
    `retroarch.cfg`), and on overrides stacking over `--appendconfig`.
  - `docs/FORK.md`, the merge checks: the hook in `run-retroarch` (already in the list read by hand since step 4) and
    `system/rootfs/etc/banana/retroarch.cfg` (`video_threaded`, `rgui_config_directory`): if upstream turns threaded
    video on for every core, the fork's override is no longer needed.
  - The rows' help is unchanged: Mesen is already "the most accurate, and heavier", and with no clock change there's
    no battery cost to tell.
- **Tests**: `kiwami/tests/core-overrides.sh` through `ci.sh`; `sh -n` on both scripts; `make test`; then M10 on an
  update built from this step.
- **Commit**: `More cores: Mesen draws on its own thread, as it needs for 60 fps`

### 7. Final check

- `make test`, `make -C ui`, `make -C system/bananad CFLAGS="-O2 -Werror"`, `kiwami/tests/ci.sh .buildroot`.
- The fork's tests in `kiwami/tests/`: `cores-ui.sh`, `core-overrides.sh`, `tailscale-ui.sh`, `7z-ui.sh`, the helper
  and unit tests (through `ci.sh`).
- The hooks: `git diff --stat main...HEAD` and `git grep -n 'kiwami:'` (the commands in `docs/FORK.md`) agree. The
  only new hooks are `settings.cpp`'s `Kiwami::rows(*this, O);	// kiwami: the fork's cores` and `run-retroarch`'s
  `core-overrides` line (step 6).
- `make test`'s renders in `ui/tests/out/` match `kiwami`'s from before the feature (the branch's base, rendered from a
  `git worktree` of it in the same minute: the clock differs otherwise, as in 0003's deviation). Also the Other
  systems list (`home 0; press start; press r; press r; press r; press a; shot other`), rendered by both binaries
  with no core installed: identical (A5, A15).
- `kiwami/DEPENDENCIES.md` lists the three cores (steps 1-3), SameBoy's patch (step 5) and RetroArch's per-core saves
  (step 4).
- `docs/FORK.md` updated (steps 4 and 6). `docs/ARCHITECTURE.md` describes `core.<system>` only as upstream has it (`:86`),
  unchanged; `CLAUDE.md` doesn't list cores; upstream's `README.md` (its cores list) is upstream's, left alone.
- `make rgds-plus` from scratch for the three packages (`make kiwami-libretro-mesen-dirclean-rgds-plus` and the
  other two first). `output/rgds-plus/target/usr/lib/libretro/` has the three `.so` (A1), and their sizes are noted.
  The size of `output/rgds-plus/images/rootfs.squashfs` is compared with the baseline's 169,390,080 bytes and with
  the slot's 256 MB (A14). `banana-rgds-plus-<version>.update.tar` is installed on the RG DS Plus.
- `make rgds` configures and builds the three packages (`make kiwami-libretro-mesen-rgds` and the other two, one call
  each), as no RG DS is at hand to run them.
- The manual tests below all passed.
- Every acceptance criterion is met: each is listed by at least one automated or manual test, and those passed.
- **Commit**: `More cores: the final check done`

## Automated tests

| Test | Where | What it checks | Criteria |
|---|---|---|---|
| The fork's options configured | `kiwami/tests/ci.sh` (`configure.sh` for `rgds`, `rgds_plus`, `universal`) | The three `BR2_PACKAGE_KIWAMI_LIBRETRO_*=y` survive Buildroot's dependency resolution on every board | A1 |
| Each core built | steps 1-3, by hand (`make kiwami-libretro-<core>-rgds-plus`), again in the final check | aarch64 `.so` at its path, `NEEDED` only the toolchain's libraries, the libretro entry points exported, its own version stamped, Genesis Plus GX without its CD-ROM code, SameBoy's boot ROMs converted byte for byte | A1 |
| The core rows | `kiwami/tests/cores-ui.sh` (run by `ci.sh`) | banana-ui headless at both sizes against stand-in libretro folders: the four rows, their order and place at the list's end, the defaults, browsing writes nothing, Right/Left write each core and back, only SameBoy's rows without the others, none without any, a missing core's setting kept, Reset emulator settings, the rows' log lines for Other systems and Network | A2, A3, A4, A5, A7, A8 |
| Tailscale's row | `kiwami/tests/tailscale-ui.sh` (unchanged) | The Tailscale row and page still where they were in Network | A8 |
| Upstream's own | `make test` | Passing; renders identical to `kiwami` before the feature; frame bench unchanged | A14, A15 |
| Other systems as before | the final check (two binaries, one shot) | With no core installed, the Other systems list renders as it did before the feature | A5, A15 |
| Footprint | the final check (`make rgds-plus`) | The `.so` sizes and the squashfs's against the baseline and the slot | A14 |
| Mesen's override | `kiwami/tests/core-overrides.sh` (run by `ci.sh`, step 6) | Mesen's override file gets `video_threaded = "true"` once, a user's own value kept, other lines kept, no other core touched; `run-retroarch` calls `core-overrides` after the core's log line | A16 |

## Manual tests

On the device, for what headless tests can't reach. Logs are fetched over SSH/SFTP (Settings > System > SSH) or from
the card on a computer (`banana/logs/` on the card is `/data/banana/logs/` on the device). Only the RG DS Plus is at
hand: every test runs on it, and the RG DS gets the build checks of the final check only.

RetroArch's own log is `/run/banana/retroarch.log` (in RAM, only while the game panel is on, and replaced when the
next game starts). After quitting each game, before starting another, copy it over SSH to the card, named after the
test: `cp /run/banana/retroarch.log /data/banana/logs/cores-M2-mesen.log`. banana-ui's log is
`/data/banana/logs/banana-ui.log`, bananad's `/data/banana/logs/bananad.log`.

| # | Board | Steps | Expected | Logs to collect | Criteria | Outcome |
|---|---|---|---|---|---|---|
| M1 | rgds-plus | Install the update; open Settings > Other systems and go down the list; read each new row's help; change each row and back. | After Game Boy Advance: NES (FCEUmm), Game Boy (Gambatte), Game Boy Color (Gambatte), Mega Drive (PicoDrive), each offering the fork's core; the help as reviewed in step 4 | `banana-ui.log` (the `rows in Other systems` line); `grep '^core\.' /data/banana/settings.conf` after each change | A1, A2, A3, A4 | not run |
| M2 | rgds-plus | Set the four rows to Mesen, SameBoy, SameBoy, Genesis Plus GX. Start a game of each system, play a minute with sound, quit from RetroArch's menu. | Each starts with the fork's core, plays with sound and the controls, quits back to the menu | `banana-ui.log` (`run-retroarch: core mesen (Settings > Other systems), not fceumm` and the others); `retroarch.log` after each (`cores-M2-<core>.log`: the core's name and version, the content loaded) | A9 | not run |
| M3 | rgds-plus | With RetroArch's frame counter on (its menu: Settings > On-Screen Display > On-Screen Notifications > Notification Visibility > Display Statistics), play a demanding scene for each core: NES (e.g. Battletoads, Kirby's Adventure), Game Boy Color (e.g. a game with raster effects, Shantae), Mega Drive (e.g. Sonic 3, Vectorman, Virtua Racing). Meanwhile, over SSH: `P=$(pidof retroarch); for i in $(seq 30); do cat /sys/devices/system/cpu/cpufreq/policy0/scaling_cur_freq; top -b -n 1 \| grep -w retroarch; sleep 1; done > /data/banana/logs/cores-M3-<core>.txt; grep VmHWM /proc/$P/status >> /data/banana/logs/cores-M3-<core>.txt`. The same scene with the upstream core, for comparison. | 60 fps throughout with each fork core, no crackle; the CPU clock and load, and the peak memory, noted against the upstream core's. A shortfall is a deviation (A10) | `cores-M3-<core>.txt` for each fork and upstream core; the fps seen, noted | A10, A14 | failed (22 Oct 2026, 0.4.1 from `e25ebf0`): Mesen and SameBoy below 60 under `powersave`; see the notes. Fixed by steps 5 and 6, checked again by M10 |
| M4 | rgds-plus | A NES game with a battery save (e.g. The Legend of Zelda): save in game with FCEUmm, quit. Switch NES to Mesen, start it, save something else, quit. Switch back to FCEUmm, start it. The same for a Game Boy game (Gambatte / SameBoy, e.g. Pokémon, whose clock is in the save) and a Mega Drive one with SRAM (PicoDrive / Genesis Plus GX, e.g. Phantasy Star IV). | Mesen starts with no save; back on FCEUmm, the first save is there. The same for the others | `ls -lR /data/saves /data/states > /data/banana/logs/cores-M4.txt` at the end | A11 | not run |
| M5 | rgds-plus | Quick resume on (Settings > Other systems). A NES game with Mesen: quit mid-level, start again. Switch to FCEUmm and start it. | Mesen resumes mid-level; FCEUmm starts the game from its title (it doesn't load Mesen's state) | `ls -lR /data/states >> /data/banana/logs/cores-M5.txt`; `retroarch.log` of each start | A11 | not run |
| M6 | rgds-plus | During a game with each fork core: close the lid, wait 10 s, open it. | It sleeps; the game carries on with sound after opening | `bananad.log` | A12 | not run |
| M7 | rgds-plus | Other systems > Screen: Bottom, start a Mesen game; Second screen: Off, start a SameBoy game; back to Top and Game panel, a Genesis Plus GX game. | Each plays on the chosen panel, the other one showing the game panel or dark as set | `banana-ui.log`; `retroarch.log` where the game panel is on | A12 | not run |
| M8 | rgds-plus | Over SSH, set `core.nes = nestopia` in `/data/banana/settings.conf` (a core the image hasn't got); start a NES game. Then look at the NES row. | The game starts with FCEUmm; the row shows `nestopia` as it is (upstream's handling of a value it doesn't list) | `banana-ui.log` (`run-retroarch: core fceumm`) | A6 | not run |
| M9 | rgds-plus | Only with a RetroAchievements account set: a game with achievements on each fork core. | The game is identified: its achievements on the game panel, `[RCHEEVOS]` lines naming it | `retroarch.log` of each | A13 | not run |
| M10 | rgds-plus | M3 again with the fixes, the frame counter on, under `powersave` and with the card's `video_threaded = "false"`. SameBoy (on step 5's update): Link's Awakening DX and Castlevania: The Adventure, and a minute of each on headphones next to Gambatte. Mesen (on step 6's update): Castlevania and Super Mario Bros, normally and fast-forwarded, a jump felt against FCEUmm's; the lid closed mid-game for 10 s and opened; quit. Over SSH during each, the `m3` measurement of the notes below (named `cores-M10-<core>-<game>.txt`); after Mesen, `cat /data/retroarch/config/Mesen/Mesen.cfg; grep '^video_threaded' /data/retroarch/retroarch.cfg`. Then a Gambatte game: not threaded. | SameBoy: 60 fps unthreaded, no audible difference beyond the two cores' own. Mesen: 60 fps, the controls as FCEUmm's; `Mesen.cfg` holds `video_threaded = "true"`, `retroarch.cfg` still `"false"`; `retroarch.log` has `[Override] Core-specific overrides found at "/data/retroarch/config/Mesen/Mesen.cfg"`, Gambatte's none. A shortfall is a deviation (A10) | `cores-M10-*.txt`; the fps seen, what was heard and felt, noted; `banana-ui.log` (`kiwami: core-overrides: mesen draws on its own thread`); `retroarch.log` of the Mesen and Gambatte games (`cores-M10-mesen.log`, `cores-M10-gambatte.log`) | A10, A16 | not run |

Notes from the runs (what the logs showed, what was fixed):

- **M3, 22 Oct 2026** (0.4.1 from `e25ebf0`, `logs/cores-M3-*.txt` and `logs/cores-M4clock-*.txt`). The fps seen on
  RetroArch's counter, the clock each phase held (`scaling_cur_freq`, every second):

  | Core, game | `powersave` (≤1416) | fixed 1608 | fixed 1800 | `balanced` (≤1992) | `performance` (1992) |
  |---|---|---|---|---|---|
  | Mesen, Castlevania | 30 | 30 | 33 | 30 (stayed at 1416) | 60 |
  | Mesen, Super Mario Bros | 30 | 30 | 50-52 | | |
  | SameBoy, Link's Awakening DX | 30 | 60 | 60 | 60 (at 1992) | 60 |
  | SameBoy, Castlevania: The Adventure | 55 | 60 | 60 | | |
  | FCEUmm, Castlevania | 60 | | | 60 | 60 |
  | Gambatte, Link's Awakening DX | 60 | | | 60 | 60 |
  | Genesis Plus GX, Sonic 1 / Sonic 3 & Knuckles | 60 / 60 | | | 60 | 60 |

  The fixed clocks were `performance` with `scaling_max_freq` capped and the GPU on demand; `performance` (the last
  column) had the GPU at its top clock too. A frame over 16.7 ms misses the refresh and VSync halves it to 30. The
  governor can't climb out of that: at 30 fps the CPU waits half the time and looks idle (Mesen under `balanced` stayed
  at 1416 MHz). Peak memory (`VmHWM`) 37-41 MB for every core. Mesen's sound is at 48 kHz and it is built `-O3` with
  LTO: it is heavy as it is. SameBoy's libretro port sends 384 kHz (`libretro/libretro.c:11`), which RetroArch
  resamples to 48 kHz each frame (Gambatte sends 32.8 kHz). ROCKNIX ships both cores unpatched (the same commits
  and tarballs) and runs every game at `performance` unless a governor is set (`runemu.sh:14`, `:437-439`), with
  RetroArch's `video_threaded = "true"` on the RK3566 (`retroarch/sources/RK3566/retroarch.cfg`); Banana has it off
  and two swapchain images, for the lowest latency.
- **Threaded video, 22 Oct 2026** (the same image, turned on in RetroArch's menu, under `powersave`): Mesen
  (Castlevania) 60 fps, 75-80 fast-forwarded; SameBoy the same (60 fps). The controls on Mesen didn't feel slower.

## Deviations

Where the implementation turned out different from this plan, why, and what changed (dated).

- **22 Oct 2026: Mesen and SameBoy fall short under `powersave` (M3); two steps added before the final check.** Mesen
  needs the top clock (1992 MHz) for 60 fps as Banana runs RetroArch, SameBoy 1608 MHz as built; Genesis Plus GX holds
  60 under `powersave`. With RetroArch's threaded video both hold 60 under `powersave` (the notes). The user's choices:
  SameBoy's sound goes to 96 kHz (step 5, a patch), which costs no input delay, measured unthreaded before deciding
  whether it needs threaded video too; Mesen gets threaded video through its per-core override, written by the fork's
  `core-overrides` from a `kiwami:` hook in `run-retroarch` (step 6). No core gets clocks of its own. A10 now names the
  fixes, A16 is new, M10 checks them. Considered and left out: Mesen at the top clock through `banana-perf
  superdrastic` (planned first, the battery note in its help with it, replaced once threaded video was measured);
  threaded video for every core (upstream's latency choice); a governor steering the clock from RetroArch's frame
  times, its own feature (an issue of its own); Nestopia instead of Mesen; SameBoy at 48 kHz. The old step 5 (the
  final check) is now step 7.

## Progress

A step is marked done in its own commit, so the commit column holds the subject (a commit can't carry its own hash).

| Step | Status | Commit |
|---|---|---|
| 1. Mesen, a second NES core | done | More cores: Mesen, a second NES core, in the image |
| 2. SameBoy, a second Game Boy and Game Boy Color core | done | More cores: SameBoy, a second Game Boy and Game Boy Color core, in the image |
| 3. Genesis Plus GX, a second Mega Drive core | done | More cores: Genesis Plus GX, a second Mega Drive core, in the image |
| 4. A row per system in Settings > Other systems | done | More cores: a row per system in Settings > Other systems for the fork's cores |
| 5. SameBoy's sound at 96 kHz | done | More cores: SameBoy's sound at 96 kHz, a quarter of the resampling |
| 6. Mesen's threaded video | todo | |
| 7. Final check | todo | |
