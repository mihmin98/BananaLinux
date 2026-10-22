# 0006 More cores: investigation

Issue: #6 · Branch: `k/0006-more-cores` · Status: refined · Confidence: 93% (proceed)

## The request

More RetroArch cores in the image, picked from what ROCKNIX ships, and a check that the existing cores are up to date
(updated where that's safe).

Decided while discussing the request:

- **Alternatives only**: a second core for systems Banana already has, no new shelves. The three:
  - **Mesen** for the NES (`nes`), next to FCEUmm;
  - **SameBoy** for the Game Boy and the Game Boy Color (`gb`, `gbc`), next to Gambatte;
  - **Genesis Plus GX** for the Mega Drive (`megadrive`), next to PicoDrive.
- Each is a choice in **Settings > Other systems**, one row per system (NES, Game Boy, Game Boy Color, Mega Drive)
  after upstream's Game Boy Advance row. A row writes `core.<system>` in `banana/settings.conf`, the setting
  `run-retroarch` already reads, and shows only when the fork's core is installed, as upstream's GBA row does. The
  defaults stay upstream's cores.
- **Saves stay per core** (RetroArch's own behaviour), and the rows' help says so.
- **Clocks: measure first.** The systems keep their `powersave` profile; a manual test checks for dropped frames
  before any change is planned.
- **The existing cores: none bumped by the fork.** Eight of the nine are at their upstream's latest commit; FCEUmm's
  two newer commits are left for upstream to take.
- Added to the scope: a `.hash` file for each core's download, a headless test of the rows in `kiwami/tests/`, and a
  note that the packages could be offered upstream.

Out of scope: new systems (Master System, Game Gear, PC Engine, Neo Geo Pocket, WonderSwan, Lynx, Atari, arcade...),
other alternatives ROCKNIX has (Nestopia, Gearboy, mGBA for the Game Boy, Snes9x 2010, SwanStation, ParaLLEl N64,
VBA-M...), carrying saves between cores, and bumping upstream's own core packages.

## Oversights in the requirements

1. **Saves don't follow the game to another core.** RetroArch v1.22.2 sorts saves and states into a folder per core
   by default (`config.def.h:917-918`, `DEFAULT_SORT_SAVEFILES_ENABLE true`, `DEFAULT_SORT_SAVESTATES_ENABLE true`),
   and Banana's `retroarch.cfg` doesn't change that (`system/rootfs/etc/banana/retroarch.cfg:36-37` set only the two
   roots). So a game played with FCEUmm has `/data/saves/FCEUmm/<game>.srm`. Switched to Mesen, it starts with no
   save (`/data/saves/Mesen/`), and switching back finds the old one again. Upstream's GBA row (gpSP / mGBA) already
   behaves this way. The good side: one core's Quick resume state is never loaded by another core.
   Carrying saves over would need a hook in `run-retroarch` and work per format. Game Boy clocks differ (Gambatte
   keeps a `.rtc` beside the `.srm`; SameBoy appends its clock), and PicoDrive's and Genesis Plus GX's SRAM layouts
   are said to differ (not checked).
   *Decided*: saves stay per core; each row's help says so ("each keeps its own saves; switching back finds them").

2. **Heavier cores under the `powersave` profile.** `nes`, `gb`, `gbc` and `megadrive` all run at `perf = powersave`
   (`system/rootfs/etc/banana/systems.conf:32,39,46,61`), which is schedutil capped at 1416 MHz
   (`system/rootfs/usr/bin/banana-perf:23`). The profile belongs to the system, not the core: banana-ui sets it
   before the launcher runs (`ui/src/main.cpp:1348`), and `run-retroarch` picks the core after that
   (`run-retroarch:13-17`). Mesen and Genesis Plus GX do more work per frame than FCEUmm and PicoDrive. ROCKNIX ships
   all three on the RK3566, but under its own clocks.
   *Decided*: measure first. A manual test runs demanding games with each core under `powersave` and reads RetroArch's
   log. Only if a core falls short is a fix planned, as a deviation; it would need a hook in upstream's launch path.

3. **The card's own `systems.conf`.** It replaces the image's (`systems.conf:2`), but that doesn't matter here:
   `core.<system>` is applied by `run-retroarch` whatever the `run` line names, as long as the card's section ids are
   `nes`, `gb`, `gbc` and `megadrive` (`BANANA_SYSTEM` is the section id). *Proposed*: nothing to do.

4. **An image without the core.** A card set to `core.nes = mesen` booted into an image that lacks Mesen (the
   previous slot from before this feature, upstream's image, a build with the option off) falls back to the
   `systems.conf` core: `run-retroarch:16` only uses an installed core, and logs which one ran (`:20-21`). The row
   shows a value it doesn't list as it is and keeps it (`ui/src/settings.cpp:505-509`). *Proposed*: nothing to do.

5. **FCEUmm's pin is upstream's file.** `buildroot/package/libretro-fceumm/libretro-fceumm.mk:7` is two commits
   behind (`ac0d00a`, 11 Oct 2026: OneBus famiclone games keep a separate CHR ROM, `src/boards/onebus.c`, 4 lines).
   The fork doesn't edit upstream's packages (`docs/FORK.md`), and overriding a version from a second external tree
   doesn't work cleanly: `generic-package` expands the version when upstream's `.mk` is evaluated, before the fork's
   files are read. *Decided*: leave it for upstream.

6. **What "Reset emulator settings" does.** It puts `core.*` back to the template's values (`ui/src/settings.cpp:427`,
   `resetApply`, `ui/src/settings_defaults.cpp:205-209`), and the template already has `core.nes`, `core.gb`,
   `core.gbc` and `core.megadrive` (`system/rootfs/etc/banana/settings.conf:45-54`). The fork's rows go back to
   upstream's cores with no change. *Proposed*: nothing to do; the headless test checks it.

## Suggestions

- **Mesen, SameBoy and Genesis Plus GX in one go, one package each** (`kiwami/package/kiwami-libretro-<core>/`),
  mirroring upstream's `libretro-*` packages and the fork's `kiwami-tailscale` naming. *Taken.*
- **mGBA as a Game Boy / Color choice** costs nothing (already in the image for the GBA). *Not taken* (the user's
  pick).
- **Hash files** for the three downloads, as `kiwami-tailscale.hash` has. Cost: a hash to recompute on every bump.
  *Taken.*
- **A headless test of the rows**: they show only when the core's `.so` is installed, which a host never has, so
  banana-ui needs a test override for the libretro folder (an environment variable, as `KIWAMI_TAILSCALE` is for
  Tailscale's helper). *Taken.*
- **Offer the packages upstream.** Upstream added mGBA as a second GBA core on 18 Oct 2026 (`7a3aa80`); these three
  fit the same pattern. Once upstream has them, the fork drops its packages and rows. *Taken as a note*: not part of
  this feature's steps.
- **Compile-time details to settle in the plan**:
  - Genesis Plus GX switches on physical CD-ROM probing on Linux (`Makefile.libretro`, `HAVE_CDROM = 1`): pass
    `HAVE_CDROM=0`, as upstream does for PCSX-ReARMed (`HAVE_PHYSICAL_CDROM=0`) and RetroArch (`--disable-cdrom`).
  - SameBoy turns its prebuilt open boot ROMs into C with the build host's `hexdump` (`libretro/Makefile:368-379`,
    which ROCKNIX covers with `util-linux:host`): either rely on the host's (present on CachyOS and Ubuntu), or
    generate the files with `od` in a pre-build hook.
  - All three makefiles stamp `git rev-parse --short HEAD` as the core's version (`GIT_VERSION`). Built inside this
    repository's `output/`, a tarball would get Banana's commit: pass `GIT_VERSION` explicitly.

Nearby, outside the scope (proposals for upstream): `docs/EMULATORS.md:55` says saves go to `/data/saves/<system>`,
but RetroArch's go to `/data/saves/<core>/` (oversight 1).

## Current state

- **Cores in the image**: upstream's packages in `buildroot/package/libretro-*` (gpSP, mGBA, Gambatte, FCEUmm, Snes9x,
  PicoDrive, PCSX-ReARMed, Mupen64Plus-Next, fake-08), each `default y` in `buildroot/package/retroarch/cores.in`
  under `BR2_PACKAGE_RETROARCH`. Each builds the core's libretro makefile with `platform=unix` and the target's CC/CXX
  and installs `/usr/lib/libretro/<core>_libretro.so`. None has a `.hash` file. Upstream pinned them between
  12 and 20 Oct 2026.
- **Up to date?** (`git ls-remote` against each repository's HEAD, 22 Oct 2026):

  | Core | Banana's pin | Upstream's HEAD | |
  |---|---|---|---|
  | gpSP | `5819380` | `5819380` | latest |
  | mGBA | `7a12d6d` | `7a12d6d` | latest |
  | Gambatte | `d9d6cd0` | `d9d6cd0` | latest |
  | FCEUmm | `7a542da` (26 Sep 2026) | `ac0d00a` (11 Oct 2026) | 2 commits behind (oversight 5) |
  | Snes9x | `fae2fea` | `fae2fea` | latest |
  | PicoDrive | `1890c29` | `1890c29` | latest |
  | PCSX-ReARMed | `c881679` | `c881679` | latest |
  | Mupen64Plus-Next | `12edd2c` | `12edd2c` | latest |
  | fake-08 | `814991a` | `814991a` | latest |
  | RetroArch | v1.22.2 | v1.22.2 (latest tag) | latest |

  ROCKNIX's pins are older than Banana's for several (e.g. gpSP `8d268a6`, mGBA `e31759b`).
- **ROCKNIX** (`ROCKNIX/distribution` at `c1b8c1e`, 22 Oct 2026,
  `projects/ROCKNIX/packages/virtual/emulators/package.mk`) builds about 140 libretro cores for the RK3566. Its
  alternatives for Banana's systems:
  - NES: Nestopia (default), FCEUmm, QuickNES, Mesen
  - GB/GBC: Gambatte (default), SameBoy, Gearboy, TGB Dual, mGBA, VBA-M, DoubleCherryGB, SkyEmu, Mesen-S
  - Mega Drive: Genesis Plus GX (default), its wide variant, PicoDrive
  - SNES: Snes9x and its 2002/2005/2010 forks, Supafaust, bsnes-mercury/2014, Mesen-S
  - GBA: mGBA, VBA-M, VBA Next, Beetle GBA, SkyEmu, gpSP
  - PS1: PCSX-ReARMed, SwanStation, DuckStation
  - N64: Mupen64Plus-Next, Mupen64Plus, ParaLLEl N64

  Its recipes for the three chosen cores (`packages/emulators/libretro/{mesen,sameboy,genesis-plus-gx}-lr`) carry no
  patches. They build `make -C Libretro`, `make -C libretro BOOTROMS_DIR=…/BootROMs/prebuilt` and
  `make -f Makefile.libretro`.
- **The three cores**:

  | Core | Repository | HEAD | License | ROCKNIX's pin | Library name (save folder) |
  |---|---|---|---|---|---|
  | Mesen | libretro/Mesen | `0102910` (20 Apr 2026) | GPL-3.0 | the same | `Mesen` |
  | SameBoy | libretro/SameBoy | `aa158a8` (23 Jul 2026) | MIT | the same | `SameBoy` |
  | Genesis Plus GX | libretro/Genesis-Plus-GX | `aa74d79` (16 Oct 2026) | non-commercial (like PicoDrive's and Snes9x's) | `b7e79b3` (older) | `Genesis Plus GX` |

  Mesen is C++ (`Libretro/Makefile`, `-O3` on unix), with SevenZip and Lua bundled. SameBoy is C, with its
  boot ROMs prebuilt in the tree (`BootROMs/prebuilt/*.bin`, its own open ones, no BIOS needed). Genesis Plus GX is C,
  with its CHD, zlib and Tremor bundled.
- **Picking a core**: `run-retroarch` reads `core.$BANANA_SYSTEM` from the card's `settings.conf` (then the image's)
  and uses it when that core is installed (`system/rootfs/usr/libexec/banana/run-retroarch:13-17`). Any `core.<x>`
  value is valid (`ui/src/settings_defaults.cpp:125-142`). Settings shows one such row upstream, the Game Boy Advance
  one, only when mGBA is installed (`ui/src/settings.cpp:401-402`).
- **The fork's rows**: `Kiwami::rows(Settings&, group)` (`ui/src/kiwami/kiwami.h:21`, `kiwami.cpp:39-48`) is called
  once, for Network (`ui/src/settings.cpp:407`), and adds the Tailscale row whatever the group. The header says the
  fork's rows' keys start with `kiwami.` (`kiwami.h:4`). Rows with upstream's own keys (`core.nes`) are plain choice
  items, saved and loaded by upstream's code (`Settings::load`, `:505-509`), so `owns()` must not claim them.
- **The rootfs**: the squashfs (LZ4, `buildroot/configs/banana_rgds_plus_defconfig:60`) is 169 MB in a 256 MB slot
  (`output/rgds-plus/images/genimage.cfg`, the size fixed in `buildroot/board/rgds/genimage.cfg.in:33,37` since the
  A/B layout), the fork's Tailscale included. Today's nine cores take 24 MB unpacked.

## What has to change

- **Packages** (the fork's tree): `kiwami/package/kiwami-libretro-mesen/`, `kiwami-libretro-sameboy/` and
  `kiwami-libretro-genesis-plus-gx/`. Each has a `Config.in` (`depends on BR2_PACKAGE_RETROARCH`; Mesen also on
  `BR2_INSTALL_LIBSTDCPP`), a `.mk` pinned at the commit above, and a `.hash`. Each installs
  `/usr/lib/libretro/{mesen,sameboy,genesis_plus_gx}_libretro.so`. They're sourced from `kiwami/Config.in` and switched
  on in `kiwami/configs/kiwami.fragment`, so `kiwami/configure.sh` fails if one is dropped.
- **Settings rows** (`ui/src/kiwami/kiwami.cpp`, `kiwami.h`): `rows()` branches on the group. Network gets Tailscale
  as today. Other systems gets NES (FCEUmm / Mesen), Game Boy (Gambatte / SameBoy), Game Boy Color (Gambatte /
  SameBoy) and Mega Drive (PicoDrive / Genesis Plus GX). Each row writes `core.<system>` in `banana/settings.conf`,
  shows only when the fork's `.so` is in the libretro folder (`/usr/lib/libretro`, or a test override from the
  environment), and its help names both cores and says each keeps its own saves. The header's comment about `kiwami.`
  keys gets amended.
- **One hook** in `ui/src/settings.cpp`, after the GBA row (`:402`): `Kiwami::rows(*this, O);	// kiwami: the fork's
  cores`. It can't be avoided: the rows have to sit in upstream's group, and `build()` is upstream's. Upstream changes
  `settings.cpp` often (8 commits in the last ten days), but a one-line hook resolves easily.
- **Settings defaults**: none new. `core.nes`, `core.gb`, `core.gbc` and `core.megadrive` already default to
  upstream's cores in the template and in `settings_defaults.cpp:61-68`.
- **Tests**: `kiwami/tests/cores-ui.sh` (rows shown and written, reset, the Tailscale row only in Network), run by
  `kiwami/tests/ci.sh` when banana-ui is built.
- **`kiwami/DEPENDENCIES.md`**: three rows in the fork's table. RetroArch's row in upstream's table also notes that the
  rows' help relies on its per-core save folders (`sort_savefiles_enable`'s default).
- Nothing in `systems.conf`, `run-retroarch`, `kiwami/post-build.sh` (the `7z` lines already cover `nes`, `gb`, `gbc`
  and `megadrive`) or the rootfs overlay.

## Options

1. **The fork's packages plus a row per system (recommended, chosen).** Small packages in the fork's tree, one hook
   line, upstream's machinery (`core.<system>`, `run-retroarch`, the reset) unchanged. Upstream's look is unchanged,
   since the rows need the cores installed.
2. **Packages only, picked by hand in `settings.conf`.** No hook at all, but nobody would find the choice.
3. **Make the new cores the defaults.** Changes upstream's behaviour (and moves every existing save out of reach,
   oversight 1). `docs/FORK.md` rules it out.
4. **Edit upstream's `cores.in` and packages.** Conflicts with every upstream bump. Ruled out; offering them upstream
   (Suggestions) is the clean version of this.

## Risks

- **Performance and footprint.**
  - Nothing runs while idle, and the menu's frame is unchanged: four more rows in a list.
  - The rootfs grows by an estimated 10-20 MB unpacked (Mesen being the largest), about half of that in LZ4, against
    roughly 87 MB of headroom in the slot. The plan measures it per core.
  - Build time grows by several minutes (Mesen's C++).
  - In a game, the heavier cores may need clocks `powersave` caps (oversight 2): only the device shows it.
- **Conflicts with upstream.**
  - One hook line in `ui/src/settings.cpp`, which upstream changes often.
  - If upstream adds one of these cores itself, both packages would install the same `.so`. The merge skill would
    catch it, and the fork's package and row are then dropped.
  - If upstream adds its own row for one of these systems, there would be two rows for the same key.
- **Hardware and boards.** All three cores are plain software cores drawn through RetroArch's GL driver: same on
  `rgds`, `rgds-plus` and `universal`, either panel, game panel or not (`run-retroarch`'s two paths don't depend on the
  core). Only the device shows speed, sound and RetroAchievements.
- **Save formats.** Not a risk with per-core saves. But a user moving a save file by hand between cores may find it
  doesn't load (Game Boy clocks, Mega Drive SRAM).
- **Mesen's repository is in maintenance** (last commit 20 Apr 2026; Mesen 2 is a separate project, archived and with
  no libretro core). It is stable but won't get fixes quickly.
- **SameBoy's build needs the host's `hexdump`**, which Buildroot doesn't guarantee (Suggestions).

## Testing

- **Headless** (`kiwami/tests/cores-ui.sh`, banana-ui `--script` at 640x480 and 1024x768):
  - With a stand-in libretro folder holding the three `.so` names, Other systems shows the four rows after the GBA
    row, and each A/Right writes the expected `core.<system>` value.
  - Without them, the rows don't show, and upstream's renders (`make test`) are unchanged.
  - Reset emulator settings puts them back to upstream's cores.
  - Network still has the Tailscale row, and Other systems doesn't.
- **Build**: `kiwami/tests/ci.sh .buildroot` configures every board with the three options kept. A full `make
  rgds-plus` builds the three cores. `file` on each `.so` (aarch64, no undefined symbols beyond libc/libstdc++/libm),
  and the squashfs size before and after.
- **QEMU**: `tools/qemu-boot.sh` can start a game per core (a free homebrew ROM) and show RetroArch's log line
  `run-retroarch: core mesen (Settings > Other systems), not fceumm`. Speed there means nothing.
- **Device only** (RG DS Plus; there is no RG DS to test on):
  - Each core starts a game from its row and runs at full speed under `powersave`: demanding games per system, with
    RetroArch's log (`/run/banana/retroarch.log` with the game panel on, copied to `/data/banana/logs/` by the test,
    or RetroArch's on-screen frame stats).
  - Sound, sleep and resume mid-game, Quick resume, RetroAchievements login and unlocks.
  - Switching back finds the first core's save.
  - Both screen choices (top / bottom).

## Open questions

1. Which cores to add? *Answered*: Mesen (NES), SameBoy (GB/GBC), Genesis Plus GX (Mega Drive), alternatives only.
2. Update FCEUmm? *Answered*: no, left for upstream.
3. Saves across cores? *Answered*: per core, said in the rows' help.
4. How to pick a core? *Answered*: a row per system in Settings > Other systems, shown when the core is installed.
5. Clocks for the heavier cores? *Answered*: measure on the device first.
6. Additions? *Answered*: hash files, a headless test of the rows, a note on offering the packages upstream.
7. (Planning) How to split the packages into commits? *Answered*: one step per core.
8. (Planning) SameBoy's `hexdump`? *Answered*: the boot ROMs converted with `od` in a pre-build hook, no host
   requirement.
