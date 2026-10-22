# 0003 7z ROMs: investigation

Issue: #3 · Branch: `k/0003-7z-roms` · Status: refined

## The request

Games kept as `.7z` archives show up in the library and play, like `.zip` ones do today. The collection in question is
on the second SD card, `roms/<system>/<game>.7z`, one game per archive; its DS games are unpacked `.nds` and already
show up.

Decided while discussing the request:

- `.7z` games are listed by default, with no setting (see *Open questions*, 1).
- The systems: Game Boy Advance, Game Boy, Game Boy Color, NES, Super Nintendo, Mega Drive and Nintendo 64 (`gba`,
  `gb`, `gbc`, `nes`, `snes`, `megadrive`, `n64`). The N64 is kept if its start time and memory measured on the device
  are acceptable.

Out of scope: the DS in `.7z`, the PlayStation in `.7z`, PICO-8, Ports and the built-in apps. Covers for the second
card's games (`banana-scrape` scans only `/data/roms`) are a separate issue.

## Oversights in the requirements

1. **It's not the second card.** The second card's `roms` folder is scanned like `/data/roms`; games are picked only
   by extension, and `.7z` is in no system's `ext` list. The same `.7z` on the main card is skipped as well. The fix
   is the same for both cards.

2. **Games keep their saves, not their history.** RetroArch names saves and states after the archive
   (`game.7z` -> `/data/saves/game.srm`), the same as `game.zip` or `game.gba` would: repacking a game keeps its save.
   But play counts, time played, favourites and RetroAchievements progress shown in the library are keyed by
   `<system>/<file name with extension>` (`ui/src/library.cpp:522`), so `game.zip` repacked as `game.7z` starts
   from zero there. *Proposed*: accept it.

3. **The same game twice.** `game.gba` and `game.7z` side by side are two entries sharing one save file (the same
   stem). That's already true of `.zip` today. *Proposed*: leave it.

4. **An archive with several games in it.** RetroArch opens the first file (sorted by name) whose extension the core
   takes (`retroarch tasks/task_content.c:817-855`); the rest are unreachable. *Proposed*: one game per archive is the
   supported form, as with `.zip`.

5. **The card's own `systems.conf` wins.** `/data/banana/systems.conf`, when it exists, replaces the image's file
   entirely (`system/rootfs/etc/banana/systems.conf:2`). A card with its own copy gets no `7z` from the image.
   *Proposed*: leave it; whoever keeps a copy edits it.

6. **The DS isn't RetroArch.** The DS path knows `.nds` and `.zip` only: the menu's cartridge icon from inside the
   ROM (`ui/src/library.cpp:201,762`, `ui/src/zip.cpp`), `nds-saves`, and whether DraStic or BananaDS can open a
   `.7z` at all is unknown. DS ROMs are 64-512 MB: unpacked on every start they'd cost seconds and that much RAM.
   *Decided*: out of scope.

7. **The PlayStation.** Its list has no `zip` either (`chd cue pbp m3u`); a `.7z` disc image would be unpacked to
   `/tmp`, in RAM, hundreds of MB. CHD already compresses and streams. *Decided*: out of scope.

8. **Covers for the second card.** `banana-scrape` scans only `/data/roms` (`ui/src/scrape.cpp:383`), so games on the
   second card get no covers from Settings' scraper, `.7z` or not. *Decided*: a separate issue (worth having, since the
   whole collection is on that card).

## Suggestions

- Add `7z` to the cartridge systems only: `gba`, `gbc`, `gb`, `nes`, `snes`, `megadrive`, `n64`, the ones that list
  `zip` today minus the DS. Nothing to build, no new dependency: RetroArch is built with 7z support already.
- Measure the start time and memory of the biggest case (an N64 `.7z`, 64 MB unpacked) on the device before deciding
  on `n64`.
- The scraper already treats `.7z` like `.zip` (TwilightBoxart isn't sent an archive's header,
  `ui/src/scrape.cpp:330`); libretro's covers match by name. Nothing to do there.

## Current state

- **The library.** `Library::loadDefs()` (`ui/src/library.cpp:219`) reads each system's `ext` list from
  `systems.conf`; `listRoms()` (`:369`) keeps the files whose name ends in one of them, without case, two folders
  deep, for `/data/roms` and the second card's `roms` (`Library::roots()`, `:432`, from
  `/run/banana/sd2/roms`, written by `usr/libexec/banana/sd2`).
- **`systems.conf`** (`system/rootfs/etc/banana/systems.conf`): `zip` for nds, gba, gbc, gb, nes, snes, megadrive,
  n64; not psx. Upstream (`main`) is the same: the fork hasn't touched it.
- **RetroArch** v1.22.2 (`buildroot/package/retroarch/retroarch.mk`) doesn't disable 7z: the build has
  `HAVE_7ZIP = 1` (libretro-common's LZMA SDK). Given an archive, it opens the first matching file inside
  (`tasks/task_content.c:817-855`), then:
  - cores that take the ROM in memory (`need_fullpath = false`: gambatte, snes9x, mupen64plus-next) get it unpacked
    into memory;
  - cores that want a file (`need_fullpath = true`: gpsp, fceumm, picodrive, and mGBA when chosen for the GBA) get it
    unpacked into `cache_directory`, `/tmp` (`system/rootfs/etc/banana/retroarch.cfg:43`), a tmpfs (`rcS:17`), and
    the file is deleted when the game is closed (`task_content.c:393`).
  This is the path `.zip` takes today; only the decoder differs.
- **The 7z decoder** (`libretro-common/file/archive_file_7z.c:245-272`) unpacks the archive's whole solid block into
  memory, then copies the file out: about twice the ROM's size for a moment, single-threaded LZMA.
- **The fork's place for this**: `kiwami/post-build.sh` already edits upstream files in `$TARGET_DIR`, and
  `docs/FORK.md` names it for additions to `systems.conf`. Its "What the fork shadows" step already lists
  `systems.conf` for review after each upstream merge.

## What has to change

- `kiwami/post-build.sh`: append ` 7z` to the `ext` line of each chosen system in
  `$TARGET_DIR/etc/banana/systems.conf`, idempotently, and stop the build if a chosen system's section or `ext`
  line isn't found (upstream renamed or reshaped it), as it does for the share veto.
- `kiwami/tests/ci.sh`: run post-build on a copy of upstream's `systems.conf`, check each system's list, check that
  a missing section fails the build; then a headless banana-ui run with that file over a fixture with `.7z` files
  (on both roots, `/data/roms` and a stand-in second card) that lists them on the right shelves.
- No hook in upstream's files, no package, no new dependency (`kiwami/DEPENDENCIES.md` unchanged).

## Options

1. **`kiwami/post-build.sh` adds `7z` in the image (recommended).** A few lines in the fork's own file, nothing in
   upstream's, checked by CI. Covers every card that doesn't keep its own `systems.conf`.
2. **The card's own `/data/banana/systems.conf` with `7z` added.** No code at all, works today; but the copy replaces
   the image's file, so a system upstream adds or changes later never reaches this card until the copy is redone.
   Good as a stopgap, not as the feature.
3. **The fork's whole `systems.conf` in `kiwami/rootfs/`.** Same result as 1, but every upstream change to the file
   then has to be copied by hand. `docs/FORK.md` rules it out.
4. **banana-ui unpacking archives itself** (to a cache on the card) before launch. Would serve the DS too, but it is
   C++ in upstream's library and launch path, card writes, and a cache to manage, for what RetroArch already does.
   Not worth it.

## Risks

- **Performance and footprint.** No change to the menu, idle activity or rootfs size: the menu's scan already looks
  at every file; one more extension in the comparison costs nothing measurable. The cost is at game start: unpacking
  LZMA on the RK3566's Cortex-A55, about 1-2 s for a 32 MB GBA game and more for a 64 MB N64 one (to measure), and
  about twice the ROM's size in RAM while it unpacks (plus the copy in `/tmp` for file-wanting cores, gone when the
  game closes). Cartridge games of the 8/16-bit systems are under 6 MB: unnoticeable.
- **Conflicts with upstream.** None in git: no upstream file changes. If upstream reshapes `systems.conf` or renames
  a system, post-build fails the build instead of dropping `7z` silently. If upstream adds `7z` itself, post-build
  must leave the line alone (the idempotent check).
- **Hardware and boards.** Same image contents on `rgds`, `rgds-plus` and `universal`; nothing board-specific. Only
  the RG DS Plus is at hand for the device tests.
- **RetroAchievements.** RetroArch identifies a game by hashing the ROM: from memory, or from the unpacked file in
  `/tmp`, as for `.zip`. Should work; worth checking once on the device.
- **A game killed mid-unpack** (power key held, empty battery): the partial file is in `/tmp`, gone at the next boot.

## Testing

- **Headless** (`kiwami/tests/`): post-build's edit on upstream's `systems.conf` (each system, idempotent, failure
  on a missing section); banana-ui headless over a fixture with `.7z` games on both roots, the shelves and game
  counts checked in its log.
- **On the device** (RG DS Plus): a `.7z` game of each chosen system from the second card starts and plays (one of
  each core: gpsp, gambatte, fceumm, snes9x, picodrive, mupen64plus-next); its save is written as `<game>.srm` and
  is found again after a restart; the start time of the biggest one (N64), and memory while it runs
  (`free -m` over SSH); RetroAchievements identifies one game. Logs: RetroArch's log for the game
  (`[Content] ... extracting`, the core's load lines), from `/data/banana/logs/` or RetroArch's own log setting.

## Open questions

Questions for the user, each answered here once settled (and the answer moved into the sections above).

1. **A setting?** `docs/FORK.md` puts changed behaviour behind a setting defaulting to upstream's. Here, with no
   `.7z` files on the card, everything behaves and renders as `main` does; with them, upstream would just not show
   them. A setting would need hooks in upstream's `library.cpp` and `settings.cpp`.
   *Answered*: no setting; `.7z` games are listed by default, an addition like a new system.
2. **Which systems?** *Answered*: gba, gbc, gb, nes, snes, megadrive, n64 (n64 kept if the device test's start time
   and memory are acceptable).
3. **The DS in `.7z`?** *Answered*: out of scope.
4. **The PlayStation in `.7z`?** *Answered*: out of scope.
5. **Covers for the second card's games** (`banana-scrape` scans only `/data/roms`)? *Answered*: a separate issue.
