# 0017 Second-screen apps (PokeDaisy first): investigation

Issue: #17 · Branch: `k/0017-second-screen` (not created yet) · Status: draft (open: not being worked on yet) · Confidence: 87% (proceed)

## The request

While a RetroArch game plays on one panel, a separate program should be able to own the other panel for the length of
the game, and follow the game by reading the emulator's memory. The first such program is
[PokeDaisy](https://github.com/lidor30/pokedaisy)'s companion (party, bag, map, guide, Pokédex, battle info, trainer
card). It's meant for playing Pokémon in RetroArch with the companion on the bottom screen.

Decided with the user (question round 1):

- **Games**: retail English FireRed, LeafGreen and Emerald on the Game Boy Advance first. These are the games PokeDaisy's
  portable C core already reads. The rest (Ruby/Sapphire, ROM hacks, the Game Boy and Game Boy Color, the DS) come
  later, as separate features. They need PokeDaisy's readers ported from Kotlin to C first, so the contract designed
  here should not shut them out (see *Suggestions*, 6).
- **Mechanism**: generic. This is a fork-side "second-screen app" contract that any program can use, and PokeDaisy is
  the first program to use it.
- **Where PokeDaisy's code lives**: in a fork of PokeDaisy that the user owns. A `banana/` host goes next to its
  `3ds/ctr/`, and Banana's Buildroot package builds that fork at a pinned commit. No PokeDaisy code is copied into this
  repository.
- **Buttons**: touch only. The buttons all stay the game's, so gpSP's turbo on X/Y keeps working.

Out of scope: games other than FR/LG/E; button control of the companion; DraStic and BananaDS (the DS); the Android
app's features that write to the game's memory (the quick menu's FLY, PC anywhere, battle buttons). The 3DS core only
reads.

The issue's text (#17):

```
Second-screen apps while a RetroArch game runs, starting with the PokeDaisy companion

While a RetroArch game plays on the top panel, the bottom panel shows only Banana's game panel (achievements and the
quick-menu sheet). This adds a way for a separate program to take the bottom screen for the length of the game,
drawing there, answering its touches, and reading the running core's memory, so it can follow the game as it is
played.

The first such program is PokeDaisy's companion, ported from its portable C core (the one its 3DS port uses) through a
Banana host built from a fork of PokeDaisy at a pinned commit: party, bag, map, guide, Pokédex, battle info and trainer
card for retail English FireRed, LeafGreen and Emerald on the Game Boy Advance, driven by touch. Games it doesn't
support keep the game panel as it is today.

Behind a setting that is off by default, so Banana behaves as upstream does until it is turned on.

Out of scope for now: other Pokémon games (Ruby/Sapphire, ROM hacks, Game Boy and Game Boy Color, the DS), which
need PokeDaisy's readers ported to C first; the button controls (X/Y) for the companion.
```

## Oversights in the requirements

1. **The Android app can't come along; only the 3DS core can.** PokeDaisy is an Android app: 536 Kotlin files, about
   271,000 lines of Jetpack Compose, with mGBA and melonDS linked in over JNI (`app/src/main/`). Banana has no JVM and
   no Android. What can be ported is `3ds/core/`: about 13,000 lines of portable C, of which about 5,500 are generated
   tables. It covers the companion's tabs for retail FR/LG (rev 0 and 1) and Emerald, and the 3DS README calls it an
   "early preview" that has never run on real hardware. So the Banana companion has what the 3DS port has, not
   what the Android app has. That means no HUD, quick menu, battle buttons, IVs/EVs, learnsets or ROM hacks, and
   MAP/DEX/GUIDE's ROM tables are mapped for FireRed rev 1 and Emerald only (`3ds/README.md`, *What works*).
   *Proposed*: accept it. The feature brings the 3DS core's companion; anything more is ported into PokeDaisy's C
   core in the user's fork, game by game, later.

2. **PokeDaisy doesn't read another emulator's memory; it is the emulator.** On Android and on the 3DS, PokeDaisy
   links libmgba and reads the RAM buffers it owns (`3ds/ctr/main.c:384-392`: `core->getMemoryBlock(core, 0x02 ...)`).
   Reading RetroArch's game is new work on Banana's side. It is the main technical question of this feature (*Options*,
   A). The good news is that the core already reads through one callback,
   `typedef bool (*pd_read_fn)(void* ctx, uint32_t addr, void* out, size_t len)` (`3ds/core/pd_snapshot.h:14`),
   on GBA bus addresses. Anything that can answer "these bytes at this GBA address" can drive it unchanged.

3. **The companion also needs the ROM, not only the RAM.** MAP, POKéDEX, GUIDE, the party icons and the trainer card
   read tables and art from the ROM through a pointer (`pd_game.rom`, `pd_rom_at`, `3ds/core/pd_game.h:96-112`),
   about 16 MB for these games. RetroArch's two GBA cores differ here:
   - **mGBA** (`core.gba = mgba`) maps the ROM in its memory map (`src/platform/libretro/libretro.c:1773-1786` at the
     pinned `7a12d6d`), so the ROM can be read from RetroArch the same way as the RAM.
   - **gpSP** (the default, `systems.conf`'s `run-retroarch gpsp`) maps only IWRAM and EWRAM
     (`libretro/libretro.c:1190-1201`). It wants a file (`need_fullpath = true`, `:585`), and RetroArch unpacks a `.zip`
     or `.7z` game into `cache_directory = /tmp` (`system/rootfs/etc/banana/retroarch.cfg:43`) and hands the core that
     path.

   **Why PokeDaisy wants the whole ROM.** It reads the ROM in two ways:
   - *Tables at known addresses*, read as needed through `pd_rom_at`/`pd_rom_read` (`pd_game.c:167-180`). These are
     the party icons (`pd_icon.c:33-37`), Pokédex entries, base stats, ability names and front pictures
     (`pd_dex.c:33-117`), and trainers, learnsets and wild encounters (`pd_guide.c:97-217`). The addresses are
     fixed per game in `pd_config` (`pd_game.h`). This is small, scattered reads of a few hundred bytes each.
   - *Art found by fingerprint*: the region map, the player's head and the trainer card. `pd_romart_scan` hashes the
     whole ROM in one pass for about 50 blobs (`pd_romart.h`, `pd_romart.c:138`), "about a second on a New 3DS".
     PokeDaisy finds art this way so that it works on ROM hacks, where the art moves. In the retail games this
     feature covers, the art sits at the same place in every copy.

   So the whole ROM is needed only for the art scan. The tables need a few scattered ranges.

   Through `READ_CORE_MEMORY` (A1), the whole ROM isn't practical: 16 MB as hex text is 48 MB of `snprintf` on
   RetroArch's thread, and gpSP doesn't map it anyway. The ways to get it, by case:
   - **A `.gba`**: the companion reads the file (`BANANA_ROM`, exported by banana-ui, `ui/src/main.cpp:1433`).
   - **gpSP (the default) with a `.zip` or `.7z`**: RetroArch has already unpacked it to `/tmp` for gpSP, deleted
     when the game closes (`tasks/task_content.c:751-797`). It logs the path,
     `[Content] Content successfully extracted to: "<path>"` (`:793`), to its verbose log, which in the game panel's
     mode is `/run/banana/retroarch.log` (`run-retroarch`'s lease branch), the file banana-ui already reads for the
     achievements (`ui/src/settings.cpp:3376`). The companion reads that copy: no decoder, no cost.
   - **mGBA with a `.zip`**: unpacked in memory only (`need_fullpath = false`), so no file. The companion unpacks it
     with zlib, which is in the image (banana-ui already reads zips, `ui/src/zip.cpp`).
   - **mGBA with a `.7z`**: no file and no 7z decoder in the image. The possibilities are:
     - a decoder in the PokeDaisy host (the LZMA SDK's C decoder, public domain, or Buildroot's libarchive);
     - reading only the ranges the companion needs from mGBA's ROM map through `READ_CORE_MEMORY`. That means
       known places for the art in the retail games instead of the scan, and a ROM cache that fetches on a miss
       instead of `pd_rom_at`'s direct pointer: changes in the PokeDaisy fork, and a few hundred KB of hex over the
       first minutes;
     - no companion for that combination for now.

   *Decided*: RetroArch's `/tmp` copy when there is one (gpSP), and libarchive otherwise. libarchive is Buildroot's
   package, 3.8.7 in 2026.02.3, turned on in `kiwami/configs/kiwami.fragment`. It handles both `.zip` and `.7z`; its
   LZMA support comes from xz, which is in the image already (`BR2_PACKAGE_XZ=y`). So mGBA's archives need no code of
   the companion's own beyond libarchive's calls, and no separate zlib path. The companion keeps one copy of the ROM in
   its memory, about 16 MB, or maps RetroArch's `/tmp` copy (`/tmp` is a tmpfs, `rcS:17`, so it's in memory either
   way).

4. **Banana's quick-menu tiles would be covered.** Today the bottom panel shows achievements and the quick-menu pill
   (Save, Load, RetroArch's menu, Fast forward, Quit) (`ui/src/settings.cpp:3472-3509`). The pill sits along the
   panel's bottom edge (`ui/src/settings.h:32-35`: 176x34 at y 438). At 2x, that's the companion's bottom 21 rows,
   where its lists scroll. PokeDaisy's own SETTINGS tab already has SAVE STATE, LOAD STATE, LEAVE GAME and a
   fast-forward chip (`pd_ui.h:83-90`, `enum pd_action`). *Proposed*: hide Banana's pill while the companion shows,
   and turn the companion's actions into RetroArch's commands through the pipe banana-ui already writes
   (`SAVE_STATE`, `LOAD_STATE`, `FAST_FORWARD`, `QUIT`, `ui/src/main.cpp:1345-1354`). RetroArch's own menu stays on
   F + X. *Decided*: the pill is hidden while the companion shows, and the companion's actions become RetroArch's
   commands.

5. **Some of the companion's settings make no sense under RetroArch.** SCREEN (PIXEL/SHARP/STRETCH) and FAST-FORWARD
   speed are about the 3DS host's own game rendering (`pd_ui.h:43-58`), and so is "LEAVE GAME's" game list (`pd_menu`).
   Under RetroArch, the game's look is Banana's Shader setting and the fast-forward ratio is RetroArch's. *Proposed*:
   the PokeDaisy fork lets a host say which SETTINGS rows it has, a small change in `pd_ui_settings.c`, and the
   Banana host leaves those two out.

6. **The bottom panel dims while you watch it.** In this mode the bottom panel dims after `bottom_idle_seconds` with no
   touch or button (`BottomIdle`, `ui/src/main.cpp:834-905`). A companion is often read without being touched, for
   example a battle's matchups while you press the game's buttons. Buttons count as activity, so in practice it only
   dims when you stop playing. *Decided*: keep today's behaviour (the setting is the user's, and 0 turns it off),
   and look at it again after using it on the device.

7. **The game can be on the bottom screen.** Settings > Other systems > Screen can put RetroArch on the bottom panel
   (`video_monitor_index`, `ui/src/settings.cpp:443`), which puts the game panel on the top one. The top panel has its
   own touchscreen (`docs/EMULATORS.md:32`), and the game panel already handles touch by screen
   (`Settings::gameTouch(..., screen)`). *Decided*: the companion goes wherever the game panel is, with touch when
   that panel has it (the top touchscreen to check on the device).

8. **"Second screen: Off" leaves no screen for it.** With `retroarch_panel = off`, or when the DRM lease fails,
   RetroArch takes both panels and banana-ui draws nothing (Mode B, `ui/src/main.cpp:1359-1415`). *Proposed*: the
   companion exists only in the game panel's mode. Its setting's help says so.

9. **A game PokeDaisy doesn't support.** This covers any non-Pokémon GBA game, Ruby/Sapphire, other languages, and
   ROM hacks. Many hacks keep FireRed's header code `BPRE`, so detection by header alone can mistake a hack for
   FireRed and show garbage. *Proposed*: the companion decides after reading the header. When it isn't supported, it
   says so and exits, and banana-ui keeps its normal game panel (no flash, because nothing is shown until the
   companion's first frame). How well the 3DS core rejects hacks needs checking in its `pd_game_detect` (*Risks*).

10. **What else stops when the game stops.** Before a suspend, bananad stops the game's process group with
    `SIGSTOP`, and on a long power press it sends that group `SIGTERM` (`system/bananad/bananad.c:446-448,560-563`).
    The game runs `setsid()` (`ui/src/main.cpp:1428`), and a program in another session can't join that group. If
    banana-ui started the companion, it would keep running into the suspend and get no `SIGTERM`. *Proposed*: the
    companion is started inside the game's session and group, forked from the game's child process right after its
    `setsid()`. It stops and resumes with the game
    and ends with it, as `docs/EMULATORS.md`'s contract expects.

11. **The licence.** PokeDaisy is GPL-3.0; Banana (and so banana-ui) is MIT (`LICENSE`). Linking PokeDaisy's core
    into banana-ui would make that binary GPL. *Decided*: PokeDaisy stays its own program, built from the user's fork.
    It talks to banana-ui over a socket and shared memory, never linked into it. The fork's own pieces (banana-ui's side,
    the test stand-ins) are MIT like the rest.

## Suggestions

1. **`READ_CORE_MEMORY` over the stdin pipe Banana already has, not over UDP.** RetroArch's own command for this
   is cheap enough for PokeDaisy (*RetroArch's `READ_CORE_MEMORY`, researched* below). Its UDP form opens a control
   port on every network interface, and this kernel has no packet filter to close it. The stdin form uses the pipe
   banana-ui already writes the tiles' commands to; only the replies need a way back (*Options*, A1).

2. **Keep banana-ui as the bottom panel's only owner** (*Options*, B1; *decided*). The companion draws into shared memory and
   banana-ui shows it. Banana-ui already has the panel, its touch, its idle dim, the RetroArch command pipe and the
   recovery when something fails. A companion that crashes or exits leaves banana-ui's game panel there, with no lease
   to take back. It's also what makes the feature testable headless (*Testing*).

3. **Batch each snapshot's reads.** PokeDaisy's reader asks for one block at a time and waits for each
   (`pd_read_fn`). Through RetroArch's commands, each wait is up to a frame. The Banana host doesn't need to change
   the reader:
   - it runs `pd_snapshot_read` against a cache of the last answers and notes every address the cache didn't have;
   - it asks for all of those in one batch, and runs the reader again;
   - it starts each snapshot by asking again for every address the last one used.

   One round trip per snapshot is then the usual case, and two when the game has moved its SaveBlocks (FireRed and
   Emerald do on map changes). Every answer in a batch is read in the same pause between two frames, so a snapshot
   never mixes two frames. (With the shim of A2, the reads instead race the running core, and a frame counter is
   needed to catch a torn read.)

4. **Show the companion at whole-pixel scale on the RG DS Plus.** The core draws at 320x240 (`PD_UI_WIDTH/HEIGHT`,
   `pd_ui.h:20-21`). On the RG DS (640x480) that's exactly 2x. On the RG DS Plus (1024x768) it's 3.2x: 3x gives
   960x720 centred with 32/24-pixel margins in the companion's background colour, and 3.2x would make uneven pixels.
   *Proposed*: the largest whole factor, centred. A layout made for 1024x768 belongs to PokeDaisy's core, later.

5. **Wait for the first frame before switching.** banana-ui keeps drawing its game panel until the companion sends its
   first frame. This covers start-up (RetroArch loading, the companion reading the ROM), an unsupported game and a
   crash with no visible flash, and the user never sees an empty panel.

6. **Design the memory contract for the later games.** Reads by bus address through the core's memory map carry no
   GBA knowledge. A Game Boy core's map (mGBA's GB
   maps, `libretro.c:1827-1927`; Gambatte's and SameBoy's to check) works the same way, so Gen 1/2 later is PokeDaisy
   work only. The DS is different: DraStic is closed and not a libretro core, and BananaDS has its own frontend and
   needs the bottom panel for the DS's touchscreen. A DS companion would mean BananaDS publishing the same description
   (a change in SuperDrastic's repository) and a way to swap the bottom panel between the game and the companion, as
   the Android app does with one tap. That's a later feature. The channel between banana-ui and the app should carry
   "read these addresses" requests that a future BananaDS source could answer the same way.

7. **Leave out of the Banana host whatever RetroArch already does.** The 3DS host's game list, game rendering, sound,
   save files, save backups and save states (`3ds/ctr/main.c`) are all RetroArch's here. The Banana host is only:
   memory → `pd_snapshot_read` → `pd_ui_draw` → frame, and touch → `pd_ui_touch` → action. That's likely a few hundred
   lines.

## Current state

- **The game panel (upstream, `main`).** For a RetroArch game with `retroarch_panel = panel` (the default,
  `ui/src/settings_defaults.cpp:60`), `runGame` (`ui/src/main.cpp:1363`) leases the game's panel to RetroArch:
  `Display::lease` (`ui/src/kms.cpp:415-424`) gives a DRM lease of that panel's connector, CRTC and primary plane,
  which `banana-leaseshim.so` hands to RetroArch when it opens `/dev/dri/card0` (`ui/src/leaseshim.c`). It also makes
  the command FIFO `/run/banana/retroarch.cmd`, which RetroArch reads as stdin (`stdin_cmd_enable`,
  `run-retroarch`'s lease branch). banana-ui keeps the other panel and runs `gamePanel` (`ui/src/main.cpp:1300-1357`),
  a `poll()` loop over the DRM fd and the input devices with a 100 ms timeout. It draws `Settings::gameRender()` (the
  achievements, from RetroArch's verbose log `/run/banana/retroarch.log`, and the quick-menu sheet) when it changes,
  and turns touches into `SAVE_STATE`, `LOAD_STATE`, `MENU_TOGGLE`, `FAST_FORWARD` and `QUIT` written to the FIFO. When
  the game ends, it records the achievements' progress, closes the FIFO and display, and takes both panels back.
- **The launcher.** `system/rootfs/usr/libexec/banana/run-retroarch` (an upstream file with one `kiwami:` hook, the
  fork's core overrides) picks the core (`core.<system>`, so `core.gba = gpsp|mgba`), writes RetroArch's settings, and in
  the lease branch runs
  `LD_PRELOAD=/usr/lib/banana/banana-leaseshim.so retroarch ... -L "$CORE" "$2" < "$BANANA_RA_CMD" &`. The
  `LD_PRELOAD` is set on that one command, so a second preloaded library has to go on that line.
- **The GBA.** `systems.conf`'s `[gba]` runs `run-retroarch gpsp %ROM%` with `perf = balanced` (schedutil, full range);
  Settings > Other systems > Game Boy Advance switches to mGBA (`ui/src/settings.cpp:457`). The fork adds `7z` to its
  `ext` (0003). RetroArch is v1.22.2, built with networking, `HAVE_COMMAND`, `HAVE_NETWORK_CMD` and `HAVE_STDIN_CMD`
  (its build's `config.mk:50,147,236`).
- **The cores' memory maps.** gpSP sets `RETRO_ENVIRONMENT_SET_MEMORY_MAPS` with IWRAM (32 KB at `0x03000000`, at
  offset `0x8000` into its `iwram` array) and EWRAM (256 KB at `0x02000000`) (`libretro/libretro.c:1190-1201` at the
  pinned `5819380`). mGBA sets IWRAM, EWRAM, SRAM, the ROM (`0x08000000`, three mirrors), BIOS, VRAM, palette, OAM and
  IO (`_setupMaps`, `src/platform/libretro/libretro.c:1740-1820` at `7a12d6d`), on the first frame and again on every
  reset (`:1269,1936`). RetroArch resolves every `retro_*` function of the core with `dlsym`
  (`libretro-common/dynamic/dylib.c:201`).
- **RetroArch's `READ_CORE_MEMORY`, researched** (v1.22.2's sources, in the build tree):
  - **What it does.** `READ_CORE_MEMORY <hex addr> <bytes>` looks the address up in the core's memory map
    (`command_memory_get_pointer`, `command.c:1006-1032`) and replies
    `READ_CORE_MEMORY <addr> XX XX XX ...\n`, one `snprintf(" %02X")` per byte (`command.c:1074-1115`). It answers
    `-1 no memory map defined` / `-1 no descriptor for address` otherwise, and cuts a read at its region's end. It has
    no hardcore check, because reading doesn't change anything. `WRITE_CORE_MEMORY` turns hardcore off
    (`command.c:1142-1147`). The older `READ_CORE_RAM` goes through rcheevos' address table instead
    (`command.c:879-913`), and the command table itself prefers `READ_CORE_MEMORY` (`command.h:452-457`).
  - **When commands are served.** Every command interface is polled once per main-loop iteration, from the input
    polling (`input/input_driver.c:6416-6427`). That's once a frame while the game runs, and also while it's paused in
    RetroArch's menu. It happens between frames, on RetroArch's main thread (the core's thread). All commands
    waiting at that moment are served together, so a batch of reads sees one instant of the game.
  - **The three ways in.**
    - *stdin* (`stdin_cmd_enable`, which banana-ui's game panel already turns on): each poll reads up to 4,095 bytes
      from the pipe and runs every complete line in them (`command.c:302-339`, `CMD_BUF_SIZE` 4096). Replies are
      `fwrite` + `fflush` to RetroArch's stdout (`command.c:288-294`). Today that stdout is banana-ui's own log
      (`banana-ui-session`: `exec banana-ui >> /data/banana/logs/banana-ui.log 2>&1`), so a reply would land in the log.
    - *UDP* (`network_cmd_enable`, `network_cmd_port` 55355): each poll drains every waiting datagram (up to 2,047
      bytes each, several commands per datagram) and replies to the sender with `sendto` (`command.c:192-233`).
      It binds `AI_PASSIVE`, which means every interface (`command.c:241`,
      `libretro-common/net/net_socket.c:64-65`): reachable from the Wi-Fi network and from the fork's tailnet, with
      no way to restrict it. `CONFIG_NETFILTER` is off (`buildroot/board/rgds/linux-banana.fragment:49`), so there is
      no firewall either. Anyone there could send `QUIT`, `LOAD_STATE`, `RESET` or `WRITE_CORE_MEMORY`.
    - *Unix socket* (`command_uds_new`): only built into Lakka (`input/input_driver.c:5153-5155`). Not available.
  - **What PokeDaisy needs from it, measured.** I compiled the 3DS core's `pd_snapshot_read` on a PC with a counting
    read callback over the app's RAM fixtures. One snapshot is 29 reads / 2,661 bytes on FireRed and LeafGreen (rev 1),
    31 / 2,835 on Emerald, and 35 / 3,196 on Emerald in a battle. The largest single read is the party (600 bytes).
    The reads come in two or three dependent rounds: SaveBlock1/2 and the bag's pointers come from fixed IWRAM/EWRAM
    addresses, and the rest is read through them. Through `READ_CORE_MEMORY` that's about 1 KB of requests and about
    10 KB of hex replies per snapshot. On RetroArch's thread that's about 3,200 `snprintf` calls, a fraction of a
    millisecond on the A55, every 10 frames or so (the 3DS host's `SNAPSHOT_EVERY`). That's small next to a GBA frame.
  - **What it can't do well.** Anything large. The ROM (16 MB → 48 MB of hex on RetroArch's thread) or a whole RAM
    dump each time. gpSP doesn't map the ROM at all.
- **Reading another process's memory on this kernel.** `CONFIG_CROSS_MEMORY_ATTACH` is off
  (`buildroot/board/rgds/linux-rocknix.config:55`), so `process_vm_readv` doesn't exist. `/proc/<pid>/mem` doesn't
  depend on it and is readable by root. Yama is off (`:7121`). The toolchain is glibc (Bootlin's aarch64 glibc stable).
- **Sleep and shutdown.** bananad stops the game's process group (`getpgid` of `/run/banana/game.pid`) before a
  suspend and resumes it after. On a shutdown during a game, it sends `SIGTERM` to that group
  (`system/bananad/bananad.c:446-448,535,560-563`). banana-ui gets `SIGUSR1` after a wake and redraws.
- **The bottom panel's idle dim.** `BottomIdle` (`ui/src/main.cpp:834-905`) dims the bottom panel during a game-panel
  game after `bottom_idle_seconds`. Any button, or a touch on the bottom panel, brings it back.
- **The fork's pieces to build on.** `ui/src/kiwami/` (settings rows under Other systems through `Kiwami::rows`,
  `ui/src/kiwami/kiwami.cpp:61-75`; keys start with `kiwami.`), `kiwami/package/` (a second Buildroot external tree),
  `kiwami/rootfs/usr/libexec/kiwami/`, `kiwami/tests/` (shell tests with stand-ins, `units.cpp`), and
  `kiwami/DEPENDENCIES.md`. `.workspace/0006-more-cores` added the core rows next to which the new row would sit.
- **PokeDaisy's 3DS core.** `3ds/core/`, the C twin of the Android app's readers: `pd_game_detect` (the ROM header
  → `pd_config`, the RAM addresses, from `NativeReader.kt`), `pd_snapshot_read(snap, game, read, ctx)` (party,
  battle, foe team, bag, money, place, dex flags, event flags/vars, trainer card), `pd_ui_*` (immediate-mode UI on a
  `pd_canvas` of `0xRRGGBB` pixels at 320x240: `pd_ui_draw`, `pd_ui_touch`, `pd_ui_tick` for the blinking, and
  `pd_ui_take_action`). The 3DS host reads a snapshot every 10 frames (`SNAPSHOT_EVERY`, `3ds/ctr/main.c:56,485`). The
  desktop preview (`3ds/desktop/pd_preview.c`) drives the same core from RAM fixtures (`ewram.bin`, `iwram.bin`) into
  PNGs, with no emulator. `make fixture-roms` builds GBA ROMs that load a fixture's RAM and idle, with FireRed's or
  Emerald's header, so the whole thing can be tested without a Pokémon ROM (`3ds/README.md`, *Building and testing*).

## What has to change

With A1 (`READ_CORE_MEMORY` over the stdin pipe, decided) and B1 (banana-ui shows the app's frames, decided):

1. **RetroArch's replies come back to banana-ui.** In the game's child process, before `exec`, a `kiwami:` hook
   points the child's stdout at a pipe banana-ui reads. RetroArch's stdout today is banana-ui's log, inherited. The
   pipe's write end is made non-blocking, so RetroArch can never stall on it: if nobody reads, a reply is dropped, not
   waited on. banana-ui keeps the read end open for the whole game, so there is no `SIGPIPE`, and it drains the pipe
   in `gamePanel`'s `poll`. Lines starting `READ_CORE_MEMORY ` go to the app; any other line goes into banana-ui's log
   as before, so nothing is lost from it. Only while a second-screen app runs; otherwise stdout is as upstream has it.
2. **The app is started in the game's session.** In the same child, after `setsid()`, a hook forks the app (the
   setting's program) with its end of a socketpair, so it is in the game's process group. bananad then stops and
   resumes it with the game and sends it `SIGTERM` with the game (*Oversights*, 10), with no line in `run-retroarch`.
   When the game ends, banana-ui closes its end, and the app sees the end and exits.
3. **banana-ui's side** (`ui/src/kiwami/`, e.g. `panelapp.cpp`), during `gamePanel`:
   - it polls the app's socket and RetroArch's stdout pipe;
   - it takes the app's frames: a memfd it sends once, plus "frame ready" messages, scaled by a whole factor and
     centred;
   - it sends the app the panel's size and each touch on that panel (down, move, up, in the app's coordinates);
   - it passes the app's read batches to RetroArch's stdin, the FIFO it already writes (one `write` under 4 KB is
     atomic in a pipe, so it can't interleave with a tile's command), and passes the replies back;
   - it carries out the app's requests (`SAVE_STATE` ... `QUIT`) through the same FIFO;
   - it keeps its own game panel, and the RetroAchievements log reading behind it, until the app's first frame, and
     falls back to it when the app says the game isn't supported, closes the channel, or dies.

   Hooks in upstream's `ui/src/main.cpp`: in `runGame`'s child (stdout and the app, one call), in `runGame` around
   the game (make and close the channel), and in `gamePanel` (the extra fds in `poll`, the app's frame instead of
   `gameRender` while it shows, touches to the app instead of `gameTouch`). That's about five one-line calls into
   `ui/src/kiwami/`. `run-retroarch` is unchanged.
4. **A setting**: Settings > Other systems > *Second-screen app*, `kiwami.panel_app = off | pokedaisy`, default `off`
   (upstream's behaviour), a `Kiwami::rows` row. Its help says it needs *Second screen: Game panel*.
5. **The Banana host for PokeDaisy, in the user's fork of PokeDaisy** (e.g. `banana/main.c`, GPL-3.0, beside
   `3ds/ctr/`). It:
   - loads the ROM (*Oversights*, 3): the `.gba` itself, RetroArch's unpacked copy in `/tmp` for gpSP (its path from
     `/run/banana/retroarch.log`), and libarchive for an archive mGBA has only in memory (`.zip`, `.7z`); then runs
     `pd_game_detect`;
   - reads a snapshot every few frames through banana-ui's channel, batched by the cache-and-replay of *Suggestions*, 3;
   - draws with `pd_ui_draw` into the shared buffer, and turns touches into `pd_ui_touch` and actions into requests;
   - keeps its own settings (the guide notice) in `/data/pokedaisy/` and logs to `/data/banana/logs/pokedaisy/`.

   It also needs the small change in the fork's `pd_ui_settings.c` so a host can leave out SCREEN and FAST-FORWARD
   (*Oversights*, 5).
6. **A package for it** in `kiwami/package/` (e.g. `kiwami-pokedaisy`) that builds the fork at a pinned commit with the
   target's C compiler (no CMake, no devkitPro: the core is plain C99), depends on libarchive, and installs the
   binary under `/usr/libexec/kiwami/`. `BR2_PACKAGE_LIBARCHIVE=y` goes in `kiwami/configs/kiwami.fragment` (selected
   by the package too), with its command-line tools left off. In `kiwami/DEPENDENCIES.md`:
   - PokeDaisy's row: GPL-3.0, its commit, and where upstream PokeDaisy is;
   - libarchive in the table of upstream's dependencies: 3.8.7, pinned by Buildroot;
   - RetroArch's row updated: the fork now relies on its `READ_CORE_MEMORY`, its stdin command interface, and its
     "extracted to" log line.
7. **Tests** in `kiwami/tests/` (*Testing*).

No change to bananad, the kernel, RetroArch's package, `run-retroarch`, `systems.conf`, or the upstream settings rows.

With A2 instead (the shim), items 1 and 2 become: a preloaded `kiwami-coremem.so` (the fork's C) that wraps `dlsym`
to see the core's `SET_MEMORY_MAPS`, `retro_load_game` and `retro_run`, and publishes the regions, the content's path
or data, and a frame counter under `/run/banana/kiwami/`. It's added to the `LD_PRELOAD` on `run-retroarch`'s lease
line, which is a change to that upstream line, because the variable is set on that command alone. The app then reads
`/proc/<pid>/mem` itself (`process_vm_readv` isn't in this kernel), and banana-ui carries no reads.

## Options

### A. Reading the game's memory (decided: A1)

My first draft recommended the shim (now A2) and rejected `READ_CORE_MEMORY` as too slow. Measuring what PokeDaisy
actually reads (about 3 KB in about 30 reads per snapshot, *Current state*) showed that was wrong: RetroArch's own
command is cheap enough, so it's now the recommendation.

1. **`READ_CORE_MEMORY` over the stdin pipe, replies through banana-ui (decided).** A command RetroArch documents
   and keeps for exactly this (trackers and tools use it), served from the same memory maps as RetroAchievements.
   - *Gives*:
     - no preloaded library and no `dlsym` trick, and nothing patched in RetroArch, the cores or `run-retroarch`;
     - every read in a batch comes from the same pause between frames (no torn reads, no frame counter);
     - nothing listening on the network;
     - any core with a memory map works (gpSP, mGBA, the Game Boy cores later);
     - banana-ui sees and logs every read, which helps debugging.
   - *Costs*:
     - about 10 KB of hex formatted on RetroArch's thread per snapshot (a fraction of a millisecond, every ~10 frames);
     - one to two frames of delay per snapshot (no matter for a companion that refreshes a few times a second);
     - the cache-and-replay batching in the host (*Suggestions*, 3);
     - stdout routed through banana-ui (a hook in the game's child, *What has to change*, 1);
     - the ROM read from the game's file, so `.7z` needs a decoder or is left out (*Oversights*, 3, and open
       question 11);
     - large reads (the ROM, whole-RAM dumps) are off the table.
   - *To prove on the device first*: the replies arrive on the pipe and RetroArch's frame pacing doesn't change with
     a snapshot every 10 frames.
2. **A preloaded shim publishes the core's memory map, and the app reads `/proc/<pid>/mem`.** It wraps `dlsym` (the
   real one found with `dlvsym(RTLD_NEXT, "dlsym", ...)`, the way MangoHud does it) to see the core's
   `SET_MEMORY_MAPS`, `retro_load_game` and `retro_run`.
   - *Gives*:
     - reads of any size in microseconds, with RetroArch not involved at all;
     - the ROM from the core's map (mGBA) or the unpacked file RetroArch gave the core (gpSP, `/tmp`), so `.zip` and
       `.7z` need no decoder.
   - *Costs*:
     - about 150 lines of C with an unusual part (replacing `dlsym` in a glibc program), unproven on the device's
       glibc and sensitive to how RetroArch resolves the core's functions in later versions;
     - a change to upstream's `LD_PRELOAD` line in `run-retroarch`;
     - reads race the running core, so a frame counter and retries are needed.
   - The fallback if A1 turns out too slow, or if large reads are wanted later.
3. **`READ_CORE_MEMORY` over UDP** (`network_cmd_enable`, which the fork could turn on per core through its
   `core-overrides`, with no upstream hook at all). It's the simplest, but port 55355 is then open on every interface,
   with `QUIT`, `LOAD_STATE`, `RESET` and `WRITE_CORE_MEMORY` for anyone on the Wi-Fi or the tailnet. There's no
   firewall to close it (`CONFIG_NETFILTER` off) and no setting to bind it to loopback. Rejected for that.
4. **A patch to RetroArch** writing the memory map out itself, applied by the fork through a global patch directory.
   No `dlsym` trick, but a patch every RetroArch bump has to re-check, and a new mechanism for the fork. Only if A1 and
   A2 both fail.
5. **A proxy libretro core** that loads the real core and forwards every `retro_*` call. RetroArch then sees another
   core path, which affects per-core overrides (the fork's `core-overrides`), playlists and saves folders. Not worth it.
6. **Searching `/proc/<pid>/maps` for the GBA's RAM by its contents.** Fragile, slow at start, and different per core.
   Rejected.

### B. Who owns the bottom panel (decided: B1)

1. **banana-ui keeps it; the app draws into shared memory (decided).** The app is a separate process, but a
   renderer: banana-ui shows its frames, sends it touches, and carries out its requests.
   - *Gives*: one owner of the panel, touch, idle dim and RetroArch's command pipe; automatic fallback to the game
     panel; no KMS or evdev code in the app; a second-screen app that can be tested in banana-ui's headless mode; no
     change to upstream's `kms.cpp`.
   - *Costs*: a small protocol (socketpair, memfd, a few message types), and banana-ui copying the app's frame to the
     panel when it changes (640x480 or 960x720 pixels, about a millisecond on the A55, a few times a second).
2. **The app gets its own DRM lease of the bottom panel and reads the touchscreen itself.** A truly separate program
   that can draw however it likes.
   - *Costs*: `Display` can lease only one screen (`leased` is a single index, `ui/src/kms.cpp:415-424`), so
     upstream's KMS code changes, not just a hook. Every app needs its own KMS and evdev code. banana-ui must stop
     acting on the bottom's touches and take the lease back when the app fails. The idle dim and Banana's tiles need
     their own answers. It can't be tested headless.
   - *Gives*: more freedom for apps that aren't small, like a full-frame-rate one.

### C. Where PokeDaisy is built from (decided)

The user's fork of PokeDaisy, with a `banana/` host, pinned by commit in `kiwami/package/`. The alternatives were a
package building upstream PokeDaisy's `3ds/core/` with the host kept in this repository, or a copy of the core in
`kiwami/`. For development, Buildroot's `KIWAMI_POKEDAISY_OVERRIDE_SRCDIR` points the package at a local checkout.

## Risks

- **Performance and footprint.**
  - Nothing runs while the setting is off: no app, RetroArch's stdout as upstream has it.
  - With a companion, there's one more process during the game, waking a few times a second: snapshots every ~10
    frames, plus the blink ticks (`pd_ui_tick`) on MAP and the trainer card. That's small next to gpSP or mGBA on one
    core under `balanced`, but it may keep a second core out of deep idle (battery to measure).
  - Memory: the ROM copy (16 MB for these games, up to 32 MB for a GBA ROM), the app's tables, and a 320x240 buffer.
  - Rootfs: one binary with the generated tables, likely 1-2 MB, plus libarchive's library (about 1 MB, to measure;
    xz's liblzma is in the image already). Unpacking a `.7z` for mGBA costs a second or two of one core at the game's
    start, while RetroArch does its own unpacking; the companion shows nothing until it's ready.
  - The menu's frame time is unchanged. During the game, banana-ui copies the companion's frame only when it changes.
- **Conflicts with upstream.** The hooks are in `ui/src/main.cpp` (39 commits upstream since 11 Oct 2026, the most
  active file after `settings.cpp`, and `gamePanel`/`runGame` are where the game panel keeps changing). With A1,
  `run-retroarch` (20 commits, one fork hook already) stays untouched; A2 would add a hook to its `LD_PRELOAD` line.
  Conflicts in `main.cpp`'s hooks should be expected at merges; keeping each hook to one line matters. If upstream
  changes how the game's child is set up or stops passing the game panel's FIFO to RetroArch's stdin, the hooks follow. If
  upstream adds a second-screen mechanism of its own, the fork's should move onto it (*Upstreaming* in
  `docs/FORK.md`: a "second-screen app" hook is the kind of thing worth offering upstream).
- **RetroArch's thread (A1).** The hex replies are formatted between frames on the core's thread. The measured ~3 KB a
  snapshot is small, but a future reader that asks for much more (a whole RAM region a few times a second) would show
  up as frame-time spikes. The host should cap a batch's size and log it. The plan's first step proves the pipe and
  measures frame pacing on the device (gpSP at 60 fps with and without the companion, `BANANA_FRAME_STATS`-style
  numbers from RetroArch's log, or its statistics overlay).
- **RetroArch's stdout (A1).** Making it a non-blocking pipe affects anything else RetroArch or a core prints there:
  a full pipe drops those writes instead of blocking. banana-ui drains it and copies non-reply lines into its log,
  so in practice nothing changes. A RetroArch that one day writes replies elsewhere, or changes the reply format,
  breaks the companion: it's in `kiwami/DEPENDENCIES.md`'s RetroArch row, to check at each bump.
- **Pointers between rounds (A1).** A snapshot that needs a second round (the SaveBlocks moved) reads the pointers
  in one frame and the data in the next. A warp between the two could make one snapshot wrong for a refresh. The
  host re-reads the pointers in the same batch as their data and drops a snapshot whose pointers changed.
- **The `dlsym` wrapper (only with A2)** is the least certain technical piece of the alternative: glibc's versioned
  `dlsym` on aarch64 has to be found correctly, and reads race the core (a frame counter is needed).
- **PokeDaisy's core.** It's an "early preview", never run on hardware, moving fast upstream (the Android app is the
  reference, and the 3DS core follows it). Its hack rejection (*Oversights*, 9) and FireRed rev 0 / LeafGreen's
  missing tables are its own limits. Taking in upstream PokeDaisy is a merge in the user's fork and a commit bump
  here.
- **Hardware and boards.** RG DS: 2x, exact. RG DS Plus: 3x with margins. Both have a bottom touchscreen; the top
  one's touch (for the "game on the bottom" case) needs checking on the device. The bottom panel's backlight, sleep,
  and the frame pacing of RetroArch with a second process can only be checked on the device. Only the RG DS Plus is
  at hand for device tests (0003).
- **Licences.** GPL-3.0 PokeDaisy and MIT Banana stay separate programs. The image already ships GPL programs
  (RetroArch, cores), and PokeDaisy gets its line in `kiwami/DEPENDENCIES.md`.
- **Security.** Nothing listens on the network: the channel is a socketpair, RetroArch's commands come only through
  the stdin FIFO in `/run` (mode 0600, as today), and RetroArch's network commands stay off (A3 rejected). The app
  only reads RetroArch's memory, and banana-ui passes on nothing but `READ_CORE_MEMORY` and the five quick-menu
  commands from it (never `WRITE_CORE_MEMORY`).
  The ROM a user puts on the card is parsed by PokeDaisy's readers; a malformed ROM is PokeDaisy's bounds checking to
  survive (`pd_rom_at` takes a length). A crash only costs the companion, and banana-ui falls back.

## Testing

- **Headless, in this repository** (`kiwami/tests/`):
  - **A stand-in RetroArch**: a small program that reads commands on stdin and answers `READ_CORE_MEMORY` from a RAM
    fixture's `ewram.bin`/`iwram.bin`, the way RetroArch's `command_read_memory` formats them (including the `-1`
    errors), and logs the other commands. It stands in for `retroarch` in a headless game launch, as
    `ui/tests/fake-wifi.sh` does for Wi-Fi.
  - **banana-ui's side**: a stand-in second-screen app (a small C program that draws a pattern, logs touches, asks
    for a few reads, and can say "unsupported" or exit). banana-ui runs headless at 640x480 and 1024x768 with a
    game-panel script, and the test checks:
    - the shots: the app's frame at 2x and 3x, centred, and the game panel after an "unsupported" or a crash;
    - the log lines for touches, read batches and requests;
    - that a non-reply line RetroArch prints still reaches banana-ui's log;
    - that a stalled app never blocks the stand-in RetroArch (the non-blocking pipe).
  - **Setting off**: renders identical to `main`'s (`docs/FORK.md`, *Taking in upstream*, 4).
- **Headless, in the PokeDaisy fork**: the Banana host run on a PC against the same stand-in RetroArch over a
  fixture, with PNGs of each tab compared with `make preview`'s, and the cache-and-replay checked to give the same
  snapshot as direct reads. Better still: a real RetroArch on the PC with gpSP or mGBA and `stdin_cmd_enable`, on
  PokeDaisy's fixture ROMs (`make fixture-roms`), which runs the whole chain without a Pokémon ROM.
- **In QEMU** (`tools/qemu-boot.sh`): the image starts a GBA fixture ROM with the setting on, and the companion
  appears on `Virtual-2`. The emulators run without a GPU there, so check how far RetroArch gets.
- **On the device** (RG DS Plus, and the RG DS if available): FireRed and Emerald, with gpSP and mGBA, `.gba` and
  `.7z`. Check each tab against the game; a battle opening BATTLE by itself; save, load, fast-forward and quit from the
  companion; sleep and wake mid-battle; the lid; a long power press during a game (the companion ends with it); an
  unsupported game (the game panel stays); the bottom panel's dim; game on the bottom with the companion on the top;
  RetroArch's frame pacing with and without the companion; battery over 30 minutes. Logs to fetch:
  - banana-ui's log (the `panel app:` lines);
  - `/data/banana/logs/pokedaisy/` (detection, snapshot round trips and times, batches dropped for moved pointers);
  - `/run/banana/retroarch.log` (copy it before the next game).

## Open questions

1. **Which games?** *Answered*: retail English FR/LG/E first; the rest of GB/GBC/GBA/DS later, as separate features.
2. **Generic or PokeDaisy only?** *Answered*: generic, PokeDaisy first.
3. **Where PokeDaisy's port lives?** *Answered*: a host in the user's fork of PokeDaisy, pinned by commit.
4. **Buttons?** *Answered*: touch only.
5. **How the app reads the game's memory?** The user asked for RetroArch's `READ_CORE_MEMORY` to be researched as an
   alternative to the shim; the research is in *Current state* and *Options*, A. *Answered*: A1,
   `READ_CORE_MEMORY` over the stdin pipe with the replies through banana-ui, proved on the device in the plan's
   first step, with A2 (the shim) as the fallback. UDP (A3) rejected for the open port.
6. **Who owns the bottom panel: B1 (banana-ui shows the app's frames) or B2 (the app gets its own DRM lease)?**
   *Answered*: B1.
7. **The fork of PokeDaisy: its URL, and whether the user has created it.** *Answered*: not created yet; the plan
   names it as a placeholder, and the user creates it before the step that adds the package.
8. **Banana's quick-menu pill while the companion shows: hidden, with the companion's own SAVE/LOAD/LEAVE/fast-forward
   mapped to RetroArch's commands (*Oversights*, 4)?** *Answered*: yes, hidden.
9. **The companion on the top panel when the game is on the bottom (*Oversights*, 7)?** Recommended: yes, it follows
   the game panel. *Answered*: yes, it follows the game panel.
10. **The setting's form: a row *Second-screen app: Off / PokeDaisy* under Other systems, default Off?**
    *Answered*: yes, one row for every RetroArch game; the app steps aside for games it doesn't support.
11. **Archived games and the ROM (*Oversights*, 3)?** The user asked why the full ROM is needed and whether there is
    another way: the answer is in *Oversights*, 3 (only the art scan needs all of it; gpSP's archives are already
    unpacked by RetroArch in `/tmp`). *Answered*: RetroArch's `/tmp` copy when there is one, and libarchive for an
    archive mGBA has only in memory (`.zip` and `.7z`).
12. **PokeDaisy's SETTINGS without SCREEN and FAST-FORWARD speed (*Oversights*, 5)?** A small change in the PokeDaisy
    fork so a host can leave out rows that only make sense in the 3DS port's own player. Proposed: yes.
    *Unanswered*.
13. **3x centred on the RG DS Plus (*Suggestions*, 4)?** 960x720 with 32/24-pixel margins rather than uneven 3.2x
    pixels. Proposed: yes. *Unanswered*.
14. **A game PokeDaisy doesn't support keeps Banana's game panel (*Oversights*, 9; *Suggestions*, 5)?** Proposed: yes,
    with nothing shown until the companion's first frame. How well the core tells a hack with FireRed's header from
    FireRed is to check in `pd_game_detect`. *Unanswered*.
15. **The feature's name: `second-screen` (the folder and branch) or `pokedaisy`?** *Answered*: `second-screen`
    (`.workspace/0017-second-screen/`, `k/0017-second-screen`).
