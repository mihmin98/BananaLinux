# 0007 RetroArch master: investigation

Issue: #7 · Branch: `k/0007-retroarch-master` · Status: refined (rechecked 24 Oct 2026) · Confidence: 97% (proceed)

## The request

RetroArch's last release, v1.22.2, is from 20 Nov 2025. Master is 9,669 commits past it (GitHub's compare,
`v1.22.2...master` at `71f521e`, 24 Oct 2026), and there is no 1.23 tag yet. The user wants what master brings for
the RG DS Plus (no per-frame copy for software cores on KMS, reworked audio pacing and ALSA driver, a VSync off that
runs uncapped, a lighter 7z reader, rcheevos 12.4, touch and memory-safety fixes) in the fork's image, measured on
the device against v1.22.2 first.

Decided while discussing the request:

- **Evaluate, then ship.** The fork pins a master commit, measures it on the RG DS Plus against v1.22.2, and keeps
  it in the image only if it passes the bar below.
- **A build option**, not a runtime setting. A Kconfig option in `kiwami/Config.in` whose default (off) is upstream's
  v1.22.2, set on in `kiwami/configs/kiwami.fragment`. The image holds one RetroArch. An A/B comparison means flashing
  an image of each (the fork's last `kiwami` image is the v1.22.2 baseline).
- **The master commit `71f521e726c2b1886ee7a2484195ab63abc92127`** (24 Oct 2026), bumped by hand like the fork's
  cores. It replaces the first investigation's `a81e450` (22 Oct), which writes `log_to_file` into `retroarch.cfg`
  after a game-panel run (oversight 10). Moving to 1.23, or the next stable version, once it's tagged is a later bump.
- **The bar: no regression anywhere.** Every measured item has to be equal or better: fps under `powersave` for each
  tested game, audio, the game panel, RetroAchievements, 7z games, sleep, the headphone jack, fast-forward and
  shaders, and upstream's RetroArch hotkeys and Reset RetroArch. A regression gets fixed (as a deviation), or master
  isn't shipped.
- **Credentials: Banana adapts to master's credentials file, with one hook.** Built with `--disable-keychain` (no
  sealing). A fork script folds `retroarch-keychain.cfg` back into `retroarch.cfg` and deletes it, from one `kiwami:`
  line at the start of upstream's `ra-secrets scrub`. Upstream's scrub then does the rest, and file sharing vetoes
  the file (oversight 1).
- **Upstream's RetroArch hotkeys Off holds on master, with a second hook.** One `kiwami:` line in `run-retroarch`
  adds master's four new hotkeys to upstream's list, from a file only the master build installs (oversight 9).
- **Added to the scope**: a guard that stops the fork's pin from silently masking a RetroArch change upstream makes
  (suggestion 1); master built without the new features Banana can't use (network servers and clients, media
  decoders, the RAR reader, GameMode; suggestion 2); Mesen and SameBoy measured unthreaded on master too, as
  information only (suggestion 3).
- **TLS fallback**: if RetroArch's own TLS client fails on the device, master is built against Buildroot's mbedTLS
  instead, as a deviation (oversight 3).

Out of scope: a runtime choice between the two versions; exposing master's timed automatic save states as a setting;
RAR games; bumping the cores; the RG DS (no device to test on: on-device tests run on the RG DS Plus only); fixing
the game panel's token (oversight 5).

## Recheck, 24 Oct 2026

The first investigation and the plan were written on 22 Oct, against `a81e450` and the fork before upstream's 0.4.2.
Since then:

- **The fork took upstream's 0.4.2** (`9a8e35e`, from upstream's `release-0.4.2` branch, which upstream's `main` doesn't
  have yet: `.workspace/merges/2026-10-23-e1b2ac4.md`). For this feature it brings a RetroArch hotkeys switch in
  `run-retroarch` (oversight 9), a Reset RetroArch action (`ra-reset`, a fourth caller of `ra-secrets scrub`), a
  login on file sharing (`share` rewritten, its veto line unchanged), and bananad's dark-and-awake on the charger for
  any sleep, with the inputs grabbed and a low-battery shutdown (oversight 11). Upstream's RetroArch package and
  patches didn't change (`git diff 4101661 HEAD -- buildroot/package/retroarch buildroot/patches/retroarch` is
  empty), nor did `ra-secrets`, `retroarch.cfg`, `kiwami/post-build.sh` or `docs/FORK.md`. Line references in
  `run-retroarch`, `share`, `ui/src/settings.cpp`, `ui/src/main.cpp` and `system/bananad/bananad.c` moved and are
  updated below.
- **Master moved 165 commits** (`a81e450...71f521e`, 300 files), most of them Android, Windows, media decoders,
  shaders (a new slang compiler, `rslc`) and CI. `gfx/drivers_context/drm_ctx.c`, `audio/drivers/alsa.c`,
  `command.h`, the udev input and joypad drivers, `tasks/task_autodetect.c`, the 7z reader, rcheevos' client and the
  keychain's list of sensitive keys are byte-identical. What changed that Banana runs through: `configuration.c`
  (the `--log-file` fix, oversight 10), `audio/audio_driver.c` (core audio against a stopped driver is dropped
  instead of a blocking write, `0052ff6`), `gfx/video_thread_wrapper.c` with a new `video_thread_hw.c` and
  `gfx/drivers/gl2.c` (one GL context handed between the threads, `ffbe995`, `01f36ab`), `runloop.c` and the menu.
  `HAVE_IMAGEVIEWER` became `no` and `HAVE_RSLC=no` was added (`qb/config.params.sh`); every flag of suggestion 2 is
  still there. Upstream's three patches dry-run against `71f521e` exactly as against `a81e450` (oversight 2).
- **Nothing was implemented**: the plan's progress table has every step at `todo`. `plan.md` names `a81e450` and has
  no step for oversight 9 or tests for oversights 9 and 11, so it needs the same update (`/banana-architect 0007`).

## Oversights in the requirements

1. **Master moves RetroAchievements credentials out of `retroarch.cfg`, username included.** In master,
   `configuration.c:1160` lists 19 "sensitive" keys (`config_sensitive_keys`): `cheevos_username`,
   `cheevos_password`, `cheevos_token` and 16 others, the same settings marked `_SENSITIVE` at `:2098-2262`. On every
   save (`config_save_on_exit` is on: `system/rootfs/etc/banana/retroarch.cfg:45`) they are written to
   `retroarch-keychain.cfg` beside the config (`configuration.c:9060-9073`, `config_save_credentials` at `:9081`) and
   removed from `retroarch.cfg` by key name (`:9393-9403`). They are sealed when `HAVE_KEYCHAIN` is on (the default,
   `qb/config.params.sh:88`), with a key file `retroarch-keychain.key` in the same folder (`configuration.c:1192-1211`).
   On load the file is merged before any `--appendconfig`, so Banana's `/run` append still wins (`:6988-7010`).
   Against upstream's design, that breaks three things:
   - `ra-secrets scrub` only reads `retroarch.cfg` (`system/rootfs/usr/libexec/banana/ra-secrets:8,16-29`), so the
     password RetroArch got from `/run` (`run-retroarch:96-99`) would stay on the card in the keychain file.
   - File sharing's veto line (`system/rootfs/usr/libexec/banana/share:50`) doesn't name the file or its key.
   - The game panel decides "logged in" from `cheevos_username` in `retroarch.cfg` (`ui/src/settings.cpp:3427-3429`)
     right after the fork (`ui/src/main.cpp:1300`, the `gamePanel` lambda), racing `run-retroarch`'s `set_cfg`
     (`run-retroarch:98`). From the second game on, master would have removed the name, so the panel says "not
     logged in".
   With `--disable-keychain` the values stay in plain text in `retroarch-keychain.cfg` (sealing is the only thing
   the option turns off: the separate file is written regardless, `configuration.c:6988-7010`).
   *Decided*: built with `--disable-keychain`. A fork script (`kiwami/rootfs/usr/libexec/kiwami/`) runs from one
   `kiwami:` line at the start of `ra-secrets`' `scrub)` branch, before its early exits (`ra-secrets:14-16`). It
   moves the keychain file's lines into `retroarch.cfg` and deletes the file. Upstream's scrub then moves the
   password and token to `accounts.conf` ("the last login wins", as today) and leaves the username in
   `retroarch.cfg`, where the game panel finds it. That one hook covers all four callers: `run-retroarch` before
   (`:94`) and after (`:175`) the game, `share up` (`share:24`), and since 0.4.2 Reset RetroArch (`ra-reset:15`),
   which scrubs before it moves `retroarch.cfg` into a backup folder on the card. `kiwami/post-build.sh` adds
   `*retroarch-keychain.*` to share's veto line, as it already does for `tailscaled.state`
   (`kiwami/post-build.sh:12-15`; 0.4.2 rewrote `share` but left the line as it was, and the sd2 share copies the
   `[banana]` section with it). After a game the secrets sit in `retroarch.cfg` for as long as the scrub takes, as
   they do on v1.22.2 today.

2. **Upstream's RetroArch patches apply to any version.** `buildroot/patches/retroarch/` has no version subfolder,
   and Buildroot uses `<dir>/<pkg>/<version>/` only when it exists, else `<dir>/<pkg>/` (`.buildroot/package/pkg-utils.mk:166-170`).
   So all three would be applied to master. Dry-run against `71f521e` (`patch -p1 -F2 --dry-run`, Buildroot's fuzz 2;
   the same result as against `a81e450`):
   - `0001-AUDIO_REINIT-...`: fails (`command.h` 2/2 hunks, `alsa.c` 1/1). It isn't needed any more: master has
     `AUDIO_REINIT` as a stdin command (`command.h:1195`, `command.c:5185-5208`), and its ALSA driver frees alsa-lib's
     global configuration when it closes (`audio/drivers/alsa.c:740`), so a reinit reads where `default` plays now.
     bananad's headphone-jack message (`system/bananad/bananad.c:663`) needs no patch; its comment (`:659`) names
     the patch, which stays true for v1.22.2.
   - `0002-gfx-drm_ctx-fast-forward-...`: applies cleanly (offset 102). Still needed: master's flip wait
     (`drm_ctx.c:690-704`) is v1.22.2's.
   - `0003-menu-Online-Updater-...`: hunk 4 of 9 fails, and five others only apply with fuzz 2. Master builds the
     entries' labels differently (`MENU_ENUM_LABEL_UPDATE_OVERLAYS_STR` instead of `msg_hash_to_str(...)`), so it
     needs a rebase, not fuzz.
   *Decided*: the fork carries its own set for the pinned commit (0002 as is, 0003 rebased), and for the pinned build
   only the fork's patch folder is used for RetroArch (Options, A).

3. **HTTPS changes its TLS client.** v1.22.2 used a bundled mbedTLS (`HAVE_BUILTINMBEDTLS=auto`, its
   `qb/config.params.sh:41`). Master drops it for its own client on `libretro-common/crypto` (`HAVE_RETROSSL=auto`,
   `qb/config.params.sh:91`; `qb/config.libs.sh:579-625`). Upstream's `RETROARCH_CONF_OPTS`
   (`buildroot/package/retroarch/retroarch.mk:21-26`) don't choose one. RetroAchievements' login and unlocks go over
   it. Whether either client checks certificates wasn't read (not checked).
   *Decided*: take master's default. The device test logs in and unlocks something. If that fails, the fallback is
   `--enable-mbedtls` against Buildroot's `mbedtls` package, a new dependency recorded in `kiwami/DEPENDENCIES.md`,
   done as a deviation.

4. **Master's KMS context watches for hotplug and restarts the video driver.** New in master: a udev monitor on
   the DRM device (`drm_ctx.c:419-444`). On a `HOTPLUG=1` event for the card it can log `[KMS] Display changed,
   restarting the video driver.` (`:491`) and reinit video mid-game (`:460-495`). On Banana, the udev library is
   libudev-zero with no udev daemon, so the monitor may never get an event. That's a guess, not checked. If events
   do arrive (after a resume, when the panels come back), the reinit reopens `/dev/dri/card0`, which the lease shim
   turns into the lease again (`ui/src/leaseshim.c:15-19`). Master also asks for async page flips when the driver
   offers them (`drm_ctx.c:1128-1132`), which with VSync off could tear. (`drm_ctx.c` is identical at `a81e450` and
   `71f521e`.)
   *Proposed*: sleep mid-game in both panel modes and VSync off are device tests. `retroarch.log` is grepped for
   `[KMS] Display changed`.

5. **The game panel has probably no token on v1.22.2 either (upstream, nearby).** `gameOpen` takes `cheevos_token`
   from `retroarch.cfg` (`ui/src/settings.cpp:3428`), and looks again only when the file changes (`:3449-3454`). But
   since upstream's `5a08181`, `run-retroarch:94` and `ra-secrets:29` remove it before every game. The panel's
   requests then go out with an empty `t=` (`settings.cpp:3337`, `:3362`). From the code, its achievement list would
   fail ("the server sent ... (error)" in `banana-ui.log`) on v1.22.2 as on master. That's an inference, not seen on
   the device. 0.4.2 didn't change this.
   *Decided*: out of scope. The baseline test records what the panel does on v1.22.2, so master is compared against
   that. A fix for upstream (the panel reading `accounts.conf`'s `ra_retroarch_token`) stays a proposal.

6. **Upstream's moves under the fork's pin.** Upstream changed the RetroArch package twice and its patches three
   times in its first ten days (`git log main`, 11-21 Oct 2026); 0.4.2 changed neither. While the fork's option is
   on, a version bump in `retroarch.mk` would be overridden without notice. So would a new patch in
   `buildroot/patches/retroarch/`, since only the fork's folder is used for the pinned build. A merge from `main`
   would lose upstream's RetroArch change with no conflict.
   *Decided*: a guard (suggestion 1).

7. **Only the RG DS Plus to test on.** The option is in `kiwami.fragment`, so it applies to every board. The RG DS
   (`rgds`, 640x480 panels) gets master untested on its hardware.
   *Proposed*: accepted, as for 0006. The RG DS shares the SoC, the KMS/lease path and the ALSA routing. The
   difference is the panels' resolution, which RetroArch takes from the mode.

8. **The fork's dependence on RetroArch's behaviour.** `kiwami/DEPENDENCIES.md` (the RetroArch row) lists what the
   fork relies on: 7z support, saves and states sorted per core, per-core overrides loaded and stacked over
   `--appendconfig`. On master: `HAVE_7ZIP=yes` (`qb/config.params.sh:270`); `DEFAULT_SORT_SAVEFILES_ENABLE` and
   `DEFAULT_SORT_SAVESTATES_ENABLE` still true (`config.def.h:1117-1118`); `DEFAULT_AUTO_OVERRIDES_ENABLE` true
   (`:1111`); `config_load_override` still there (`configuration.c:7950`). The keys Banana writes
   (`video_monitor_index`, `video_max_swapchain_images`, `stdin_cmd_enable`, `audio_latency`, `video_vsync`,
   `savestate_auto_save`, and since 0.4.2 `joypad_autoconfig_dir` and the `input_<hotkey>_btn`/`_axis` binds...) all
   still exist; some moved to `settings/settings_def_*.h` under the same names (`joypad_autoconfig_dir`:
   `configuration.c:2211`, `settings/settings_def_dir_user.h:46`). The game panel's five commands (`SAVE_STATE`,
   `LOAD_STATE`, `MENU_TOGGLE`, `FAST_FORWARD`, `QUIT`; `ui/src/main.cpp:1347-1349`) are all in master's `command.h`.
   So are the log lines the panel parses (`settings.cpp:3390-3410`: `[RCHEEVOS]` through `CHEEVOS_LOG(RCHEEVOS_TAG
   ...)`; `Identified game:`, `Unknown game`, `Login failed`, `Awarding achievement ` in rcheevos' `rc_client.c`;
   `Generated hash ` in `rhash/hash.c:397`). The 7z reader is new (`libretro-common/formats/7z/`). It handles copy,
   LZMA, LZMA2, Delta and the BCJ/BCJ2/ARM/ARM64/PPC/SPARC/IA64 filters (`r7z_archive.c:92-105`). v1.22.2's LZMA SDK
   (`deps/7zip/`) had no PPMd either, so no 7z game that loads today should stop loading.
   *Proposed*: nothing to change. The device tests check them at run time, and `DEPENDENCIES.md` moves RetroArch to
   the fork's own table.

9. **Upstream's RetroArch hotkeys Off doesn't know master's hotkeys (new since 0.4.2).** Settings > Other systems >
   RetroArch hotkeys (`ra_hotkeys`, `ui/src/settings.cpp`) promises with Off "only F + X and F + Start ... and none
   bound in RetroArch's own menu either". `run-retroarch:109-116` lists every hotkey of v1.22.2. With Off it strips
   them from a copy of the pad profiles in `/run/banana/ra-autoconfig` (`:131-136`), appends a config with all of
   them as `nul` (`:137-140`), keeps the card's own lines in `/data/retroarch/.banana-hotkeys.keep` and puts them
   back after the game (`:141`, `:176-181`). The list is v1.22.2's exactly. Master has four more
   (`configuration.c:453-456`): `video_filter_toggle`, `headset_recenter`, `laser_pointer_toggle` and
   `headset_view_toggle`. All four are declared without a build condition and unbound by default
   (`config.def.keybinds.h:453-470`, `NO_BTN`), and neither the RG DS profile nor the joypads tarball bind them. But
   one bound in RetroArch's own menu would still work with Off, and would be kept as-is by the put-back (the
   `HKKEYS` pattern, `:124`, doesn't match it). On v1.22.2 they don't exist, so that's a regression against the bar.
   *Decided*: a second `kiwami:` line in `run-retroarch`, after the list (`:116`) and before `names=` (`:123`), so
   the strip, the `nul` lines and the put-back all take the names in. It adds the names from a file that only the
   master build installs (e.g. `/usr/share/kiwami/ra-hotkeys`, from `kiwami/external.mk` or `kiwami/post-build.sh`
   with the option on). With the option off there's no file and upstream's behaviour is unchanged. `ra-reset` needs
   nothing: it moves the whole `retroarch.cfg`.

10. **The first pin writes `log_to_file` into `retroarch.cfg` (`a81e450`, fixed in master).** At `a81e450`,
    `configuration.c:2267`'s `SETTING_OVERRIDE(RARCH_OVERRIDE_SETTING_LOG_TO_FILE)` marks the last row added before
    it, `show_hidden_files` (`:2266`), since `log_to_file` became a generated row. So with `--log-file`
    (`run-retroarch:156-157`, the game panel), `config_save_file` writes `log_to_file = "true"` into the card's
    `retroarch.cfg` and leaves `show_hidden_files` out. v1.22.2 marks the right row (its `configuration.c:1833-1834`).
    Master's `8b6754f` ("pin command-line overrides to their own rows", 23 Oct) fixes it with
    `SETTING_OVERRIDE_FOR(bools.log_to_file, ...)` (`71f521e`'s `configuration.c:3124`). The effect at `a81e450`
    would have been small: RetroArch only opens a log file without `--log-file` when verbose logging is on
    (`retroarch.c:8580-8591`), which Banana leaves off outside the game panel.
    *Decided*: pin `71f521e`, which also carries `0052ff6` (core audio against a stopped driver dropped instead of a
    write that could block forever, `audio/audio_driver.c`). The cost: master reworked threaded video and the GL
    driver on 23 Oct (`ffbe995`, `01f36ab`), and the fork runs Mesen and SameBoy threaded (0006). The measurements
    with threaded and unthreaded video cover it.

11. **New paths around a running game since 0.4.2.** These are upstream's own, so they aren't regressions in
    themselves, but they reach RetroArch on master as on v1.22.2:
    - *Dark on the charger*: bananad now stays dark and awake on the charger for any sleep (lid or power button),
      with the game SIGSTOPped and the buttons and touchscreens grabbed (`EVIOCGRAB`, `inputs_grab`). A battery too
      low there sends SIGCONT and then shuts down, so the game gets SIGTERM to write its saves
      (`system/bananad/bananad.c`, `suspend_now`). On master, the audio pacing and udev input after SIGCONT and the
      release of the grab are new territory. A shutdown mid-game writes `retroarch-keychain.cfg` on exit. The fold
      takes it in at the next scrub (the next game, `share up` or Reset RetroArch), the same "game cut short" case
      upstream's scrub handles for `retroarch.cfg`.
    - *Reset RetroArch* (`ra-reset`): scrubs, then moves `retroarch.cfg`, the remaps and the overrides into
      `retroarch/backup-<date>/` on the card, which file sharing shows. With the fold in the scrub, no keychain file
      or secret reaches the backup.
    *Decided*: both are device tests, on the baseline and on master (Testing).

## Suggestions

1. **A guard against upstream's RetroArch changes.** `kiwami/tests/ci.sh` fails when upstream's `RETROARCH_VERSION`
   isn't the one the fork last checked (`v1.22.2`). It also fails when the list or contents of
   `buildroot/patches/retroarch/` change (a sha256 of the folder, kept by the fork). The message names what to look
   at: rebase or drop the fork's patches, or the fork's pin. `docs/FORK.md`'s merge checks (step 3, what the fork
   shadows) gets the two paths. Cost: one more stamp to update when upstream moves. *Taken.*
2. **Master built without what Banana can't use.** Master turns on new built-ins by default
   (`qb/config.params.sh` at `71f521e`): an MCP server (`:75`, off until enabled in its menu), its own SMB and NFS
   clients (`:94-97`), media decoders (Vorbis, Opus, AAC, MP3, FLAC, MOD, WebM, MP4, AV1, VP9, MPEG-1; `:252-266`), a
   RAR reader (`HAVE_RRAR`, `:271`) and Feral GameMode (`:242`). They're turned off from `kiwami/external.mk`
   (`RETROARCH_CONF_OPTS +=`, read when the configure step runs, after upstream's `retroarch.mk` set it). That makes
   the binary smaller and leaves nothing that could listen on the network. The plan checks each flag exists and that
   none is needed by RetroArch's menu sounds or a core. The new slang compiler is off by default (`HAVE_RSLC=no`,
   `:180`) and needs nothing. Cost: a list to review on each bump. *Taken: all four groups.*
3. **Mesen and SameBoy unthreaded on master.** The fork gives them threaded video because v1.22.2 can't hold 60 fps
   under `powersave` without it (0006's M3 and M10; `kiwami/rootfs/usr/libexec/kiwami/core-overrides`). Master's "no
   per-frame copy for software cores" might be enough without it. Measured only. If it holds, dropping the overrides
   is a later change. *Taken (measurement only).* Master's threaded video was reworked on 23 Oct (oversight 10), so
   the threaded runs matter as much as the unthreaded ones.
4. **Timed automatic save states** (`DEFAULT_SAVESTATE_AUTOMATIC_INTERVAL 0`, off: `config.def.h:1721-1724`) as a
   setting. *Not taken*: a later feature, and writes to the exFAT card at intervals.
5. **RAR games** (master's reader) on the cartridge systems, as 0003 did for 7z. *Not taken*: the reader is turned
   off (suggestion 2). A feature of its own if wanted.
6. **The game panel's token from `accounts.conf`** (oversight 5): a fix for upstream. *Not taken* here.
7. **Offering the bump upstream** once the fork's measurements are in: the rebased 0003, 0001 dropped, the
   credentials fold, and master's four hotkeys in `run-retroarch`'s list (oversight 9). That's a branch off `main`
   (or upstream's release branch, as long as 0.4.2 isn't in its `main`). If upstream takes it, the fork's pin, patches
   and both hooks disappear in the next merge. *Proposal*, after this feature.

## Current state

- **The package is upstream's.** `buildroot/package/retroarch/retroarch.mk:7` pins `RETROARCH_VERSION = v1.22.2` from
  GitHub (`:8`), plus the joypad autoconfigs at a pinned commit as an extra download (`:16-18`, extracted by
  `:39-45`). There's no `.hash` file in the package folder, so Buildroot accepts the download unchecked. The
  configure options are `:21-26` (`--disable-sdl2` among them, so upstream's new SDL2 zero-copy patch doesn't reach
  RetroArch). Upstream's three patches are in `buildroot/patches/retroarch/` (`BR2_GLOBAL_PATCH_DIR`,
  `buildroot/configs/banana_*_defconfig:9-10`). The binary is 15,008,688 bytes in the last rgds-plus build
  (`output/rgds-plus/target/usr/bin/retroarch`).
- **How a second external tree can pin it.** 0006 noted (`.workspace/0006-more-cores/investigation.md`, oversight 5)
  that the fork's `external.mk` is read after upstream's `.mk` has been evaluated, too late for a version. But
  Buildroot reads the package override file (`BR2_PACKAGE_OVERRIDE_FILE`, default `$(CONFIG_DIR)/local.mk`) before
  any package (`.buildroot/Makefile:545-547`, then packages at `:550`, external trees at `:564`). It reads it after
  `.config` (`:237`) and after the external trees' paths are defined (`:190-193`). In that file,
  `override RETROARCH_VERSION = <sha>` beats `retroarch.mk:7`'s plain assignment and generic-package's own
  `:=` (`.buildroot/package/pkg-generic.mk:498-500`; a SHA sanitizes to itself). `RETROARCH_SITE` is recursive and
  follows it. Upstream doesn't set the option, so the fork's fragment can, with no upstream file touched. (The
  user's own `$(CONFIG_DIR)/local.mk` is kept with a `-include` from the fork's.)
- **Patches and hash for the pinned build.** The patch step takes its folders from `BR2_GLOBAL_PATCH_DIR` when it
  runs (`pkg-generic.mk:239-241`, `pkg-utils.mk:163-170`). So a target-specific value on RetroArch's
  `$(RETROARCH_TARGET_PATCH)`, set in `kiwami/external.mk` (where it's known), limits that one step to the fork's
  folder. `RETROARCH_HASH_FILES` (`pkg-generic.mk:502-507`) is recursive, so it can point at the fork's hash file.
  That file has to list the joypads tarball too: Buildroot fails on a hash file without an entry for a file it
  downloads (`.buildroot/support/download/check-hash:114-115`).
- **How Banana runs RetroArch.** `run-retroarch` (`system/rootfs/usr/libexec/banana/run-retroarch`) does the following:
  - picks the core (`:14-21`) and calls the fork's `core-overrides` hook (`:22`);
  - copies the config on first use (`:24`) and writes Banana's choices with `set_cfg` (`:29-86`);
  - puts the RetroAchievements account in through `/run` (`:91-102`, `ra-secrets scrub` at `:94` and `:175`);
  - since 0.4.2, with RetroArch hotkeys Off, strips the hotkeys from a copy of the pad profiles and appends them all
    as `nul`, putting the card's own lines back afterwards (`:103-143`, `:176-181`; oversight 9);
  - in game-panel mode, runs RetroArch on a DRM lease through `banana-leaseshim.so`, with commands on stdin and a
    `--verbose` log the panel reads (`:148-158`); otherwise on one panel with the other dark, with a FIFO for
    bananad's `AUDIO_REINIT` (`:159-171`).
  banana-ui makes the lease, the FIFO and the log (`ui/src/main.cpp:1372-1400`, `:1439`). The panel parses
  `/run/banana/retroarch.log` (`ui/src/settings.cpp:3370-3415`) and sends the five commands
  (`ui/src/main.cpp:1347-1349`).
- **Other callers of the scrub**: `share up` (`share:24`) and, since 0.4.2, `ra-reset:15` (Settings > Other systems >
  Reset RetroArch).
- **Defaults RetroArch runs with**: `system/rootfs/etc/banana/retroarch.cfg`. KMS/GL with two swapchain images
  (`:4-13`), ALSA `default` at 64 ms (`:22-25`), udev input (`:27-30`), RGUI, the directories (`:31-46`),
  `cheevos_hardcore_mode_enable = "false"` and Quick resume off (`:54-60`).
- **The fork's earlier features that use RetroArch.** 0003 (7z games: `kiwami/post-build.sh`, its RetroArch built
  with `HAVE_7ZIP`), 0006 (three cores, their rows, `core-overrides` for threaded video). `kiwami/DEPENDENCIES.md`'s
  RetroArch row records what they rely on. 0006's UI tests (`kiwami/tests/cores-ui.sh`) were adjusted in the 0.4.2
  merge for the two new rows (RetroArch hotkeys, Reset RetroArch); this feature adds no row.

## What has to change

- **`kiwami/Config.in`**: the build option (e.g. `BR2_PACKAGE_KIWAMI_RETROARCH_MASTER`, depending on
  `BR2_PACKAGE_RETROARCH`), default off.
- **`kiwami/configs/kiwami.fragment`**: the option on, and `BR2_PACKAGE_OVERRIDE_FILE="$(BR2_EXTERNAL_KIWAMI_PATH)/local.mk"`.
  `configure.sh` already checks that every fragment option survives.
- **`kiwami/local.mk`** (new): with the option on, `override RETROARCH_VERSION = 71f521e...`, and `-include` of the
  output's own `local.mk`.
- **`kiwami/external.mk`**: with the option on, the following:
  - RetroArch's patch step limited to `kiwami/patches/`;
  - `RETROARCH_HASH_FILES` set to the fork's hash file;
  - `RETROARCH_CONF_OPTS += --disable-keychain` and the disables of suggestion 2;
  - the hotkey names file of oversight 9 installed (or from `kiwami/post-build.sh`).
- **`kiwami/patches/retroarch/<sha>/`** (new): `0002` as upstream wrote it, `0003` rebased on master, and
  `retroarch.hash` (RetroArch's tarball, the joypads tarball, `COPYING`).
- **`kiwami/rootfs/usr/libexec/kiwami/<fold script>`** (new): folds `/data/retroarch/retroarch-keychain.cfg` into
  `retroarch.cfg` and deletes it, along with any `retroarch-keychain.key`.
- **Hook in `system/rootfs/usr/libexec/banana/ra-secrets`** (upstream's, one `kiwami:` line at the start of
  `scrub)`). This can't be avoided: it's the one place all four scrub callers go through. Without the option the
  script finds no file and does nothing, so upstream's behaviour is unchanged.
- **Hook in `system/rootfs/usr/libexec/banana/run-retroarch`** (upstream's, one `kiwami:` line between the hotkey
  list and `names=`, `:116-123`): master's four hotkeys added to `HOTKEYS` from the file the master build installs.
  It's the fork's second hook in this file, after `core-overrides` (`:22`).
- **`kiwami/post-build.sh`**: `*retroarch-keychain.*` on share's veto line, with the same stop-if-the-line-changed
  check.
- **`kiwami/tests/`**: a headless test of the fold (on its own and through `ra-secrets`, in a made-up `/data`), the
  veto in `ci.sh`'s post-build check, the guard (suggestion 1), a check that a configured board reports the pinned
  version (`make retroarch-show-info` or `printvars`), and the hotkeys hook (oversight 9).
- **`kiwami/DEPENDENCIES.md`**: RetroArch at `71f521e` in the fork's own table (where pinned, the patches, the
  disabled built-ins, the hotkey names, what a bump checks); the upstream row updated (the fork's pin overrides it
  while the option is on).
- **`docs/FORK.md`**: the merge checks name `buildroot/package/retroarch/retroarch.mk` and
  `buildroot/patches/retroarch/` for the guard.
- **`.workspace/0007-retroarch-master/plan.md`**: updated for the new pin, oversight 9's step and the new tests
  (the architect stage, before implementation).

No settings, no banana-ui changes, nothing in `ui/rnds/`, `ui/assets/` or the ROCKNIX-synced files.

## Options

- **A. Fork-only pin through the override file (recommended, chosen).** The pieces are those above. It works from
  the fork's files plus two hooks, the version is behind a build option, and the image has one RetroArch. Cost: the
  fork owns a RetroArch version, with its patches and hash, until upstream moves past it (the guard tells when).
  `local.mk`'s `override` is a less common use of the override file (it's documented for `<pkg>_OVERRIDE_SRCDIR`),
  so the plan's first step proves it on a configured board.
- **B. Command-line `RETROARCH_VERSION=` from the top `Makefile`.** It needs a hook in upstream's `Makefile`, and it's
  lost when Buildroot is run in the output directory directly (`make -C output/rgds-plus`). Not taken.
- **C. `RETROARCH_OVERRIDE_SRCDIR` to a checkout the fork prepares.** Buildroot then skips download, extract and
  patch, and the extract hook that unpacks the joypads (`retroarch.mk:39-45`) wouldn't run, so external pads would
  silently lose their autoconfigs. There's no hash either. Not taken.
- **D. A second RetroArch package of the fork's.** Upstream's cores' menu sits under `BR2_PACKAGE_RETROARCH`
  (`buildroot/package/retroarch/Config.in`), so both would be built, and the same `/usr/bin/retroarch` installed
  twice. Not taken.
- **E. Through upstream only.** A branch off `main` with the bump. The fork would wait for upstream's decision.
  Kept as a later proposal (suggestion 7).

## Risks

- **Performance and footprint.** Frame time under `powersave` could get better or worse. The issue expects better
  (the per-frame copy); only the device shows it. Master's threaded video and GL driver changed on 23 Oct, a day
  before the pin. Master's binary size is unknown until built; suggestion 2 holds it down. Peak memory (`VmHWM`) is
  compared. Nothing runs while idle: RetroArch runs only during a game, and the fold runs only at scrub time. Build
  time: one RetroArch build, as now.
- **Conflicts with upstream.** Two hook lines: `ra-secrets` (1 commit upstream, unchanged by 0.4.2) and
  `run-retroarch` (24 commits in the fork's history, 0.4.2 added 41 lines right where the hook goes). A change to
  upstream's hotkey list is a textual conflict next to the hook, which is wanted: whoever merges sees both lists. A
  version bump or a patch change upstream doesn't conflict textually; the guard catches it. `share`'s veto line is
  edited at build time (0.4.2 rewrote the file around it), and post-build stops the build if its shape changes.
  Upstream's 0.4.2 isn't in its `main` yet, so when upstream merges it there, `main` catches up with what `kiwami`
  already has.
- **Hardware and boards.** The DRM lease with master's KMS context, the hotplug monitor after a resume (oversight 4),
  async flips with VSync off, master's ALSA pacing on the RK3566's codec after SIGSTOP/SIGCONT (a sleep, and now the
  dark-on-charger pause with the inputs grabbed, oversight 11) and across the headphone jack, and touches on the
  panel banana-ui keeps ("fixes for touch": RetroArch's udev input sees every event device). Only the RG DS Plus is
  tested (oversight 7).
- **Master is master.** A commit between releases can carry bugs a release would have fixed: `a81e450` had one that
  reached Banana (oversight 10), and master adds about 80 commits a day. The pin is bumped by hand, and the bar means
  a regression isn't shipped.
- **The rebased 0003** could leave an Online Updater entry that can't work, or hide one that can. The device test
  opens the updater.
- **RetroArch's own TLS client** (oversight 3): a login that fails, or slowly. The fallback is decided.

## Testing

- **Headless** (`kiwami/tests/`, run by `kiwami/tests/ci.sh`):
  - the fold script: a keychain file with username, password and token is folded, the file and its key are gone, and
    through `ra-secrets scrub` the password and token reach `accounts.conf` ("last login wins"), the username stays
    in `retroarch.cfg`, and nothing is left on the card; with no keychain file, nothing changes and
    `retroarch.cfg`'s bytes are identical; it's idempotent;
  - post-build's veto for `retroarch-keychain.*`, next to the `tailscaled.state` one;
  - the guard: upstream's version and patch folder as recorded;
  - a configured board resolves `RETROARCH_VERSION` to the pinned SHA with the option on, and to `v1.22.2` with it
    off;
  - the hotkeys hook: with the names file, `run-retroarch`'s list (and so the `nul` config, the profile strip and the
    put-back pattern) holds the four names; without it, the list is upstream's. A stand-in `retroarch` and a made-up
    `/data` and `/run`, as `run-retroarch` can't be run on the build machine as is (the plan works out how).
  `make test` is unaffected (no banana-ui change); it is run in the final check anyway.
- **Build**: `make retroarch-rebuild-rgds-plus` with the option on, which patches cleanly and checks the hash;
  `configure`'s summary shows the disabled built-ins, the TLS client and the keychain off; the binary's size against
  15,008,688. A full image for each board before the device tests.
- **QEMU** (`tools/qemu-boot.sh`): only that the image boots and the menu comes up. RetroArch's KMS/GL path isn't
  representative there.
- **On the device** (RG DS Plus, the fork's v1.22.2 image first as the baseline, then master's). The same games and
  scenes on both:
  - fps under `powersave`: RetroArch's statistics OSD, plus 0006's M3 sampling of the CPU clock and load, and
    `VmHWM` into `/data/banana/logs/`. Systems: NES, GB/GBC, GBA, SNES, Mega Drive, N64, PS1, with the fork's
    Mesen/SameBoy threaded and unthreaded (suggestion 3);
  - audio: no crackle at 32/64/128 ms, the headphone jack mid-game, sleep mid-game (the lid) and resume in both
    panel modes (`retroarch.log`: no `[KMS] Display changed`);
  - dark on the charger mid-game (oversight 11): the lid shut and the power button on the charger, then back. The
    game resumes with sound and no crackle, no button stuck or lost after the grab is released, and the game panel
    still answers in panel mode;
  - VSync off: speed and tearing;
  - fast-forward (patch 0002);
  - the game panel: the lease, the five tiles, touches on the panel not reaching RetroArch, and the achievements
    list against the baseline (oversight 5);
  - RetroAchievements: login from the setup's account, an unlock, then after the game the following checked: no
    `retroarch-keychain.*` on the card, no password or token in `retroarch.cfg`, the token in `accounts.conf`, the
    username in `retroarch.cfg`; and a login made in RetroArch's own menu;
  - file sharing during and after a game: the keychain file never listed;
  - RetroArch hotkeys Off (oversight 9): only F + X and F + Start work; F + L/R/Y/L2/R2 do nothing; on master, a
    Video Filter toggle bound to a face button in RetroArch's menu doesn't fire either. After the game the hotkey
    lines are back as before and `.banana-hotkeys.keep` is gone. With hotkeys On, F + L/R/Y/L2/R2 work as before;
  - Reset RetroArch after a game with an account (oversight 11): the backup folder holds no password, token or
    `retroarch-keychain.*`, and the next game logs in again;
  - after a game-panel run, `retroarch.cfg` has no `log_to_file = "true"` (oversight 10);
  - 7z games (0003's); Quick resume; a GLSL shader preset; the Online Updater's entries (0003 rebased); an external
    pad's autoconfig.
  Logs to fetch: `/run/banana/retroarch.log` (copied to `/data/banana/logs/` before the next game),
  `/data/banana/logs/banana-ui.log`, `/data/banana/logs/sleep.log` (the dark-on-charger lines), the sampling files,
  and `ls -la /data/retroarch/ /data/retroarch/backup-*/` plus `grep -E 'cheevos_|log_to_file|_btn'` (redacted)
  after the game.

## Open questions

1. What should "done" look like? *Answered*: evaluate, then ship the pin in the fork's image if it passes the bar.
2. How does the image choose the version? *Answered*: a build option, default v1.22.2, on in the fragment; one
   RetroArch in the image.
3. Which RetroArch? *Answered*: today's master, `a81e450`; 1.23 is a later bump. *Changed at the recheck* (question
   15).
4. Credentials in master's keychain file? *Answered*: Banana adapts. Built unsealed, folded back into `retroarch.cfg`
   from one `kiwami:` line in `ra-secrets`, the file vetoed from sharing.
5. Which additions? *Answered*: the guard against upstream's changes, the new built-ins turned off, Mesen and SameBoy
   measured unthreaded. Not timed save states.
6. The game panel's token on v1.22.2? *Answered*: out of scope, recorded in the baseline.
7. The bar for shipping? *Answered*: no regression anywhere.
8. If the TLS client fails? *Answered*: fall back to Buildroot's mbedTLS, as a deviation.
9. Which built-ins to turn off? *Answered*: network servers and clients, media decoders, the RAR reader, GameMode.
10. Which games make up the measurement set (a demanding scene per system)? *Answered* (planning, 22 Oct 2026): NES
    Castlevania and Super Mario Bros, Game Boy Color Link's Awakening DX, Mega Drive Sonic 3 & Knuckles, GBA Pokémon
    Emerald, SNES Super Mario World.
11. N64 and PS1, not in the set? *Answered* (planning): left out, so not verified on master. No check of their own.
12. Which cores run the games of systems with two? *Answered* (planning): both: FCEUmm and Mesen, Gambatte and
    SameBoy, PicoDrive and Genesis Plus GX. Mesen and SameBoy also unthreaded on master (suggestion 3).
13. At which clocks are fps and load compared, given that GBA runs under `balanced` and SNES under `performance`
    (`systems.conf`), not `powersave`? *Answered* (planning): at each system's own profile (the bar). GBA and SNES
    also under `powersave`, as information only.
14. What's at hand for the device tests? *Answered* (planning): a computer on the network and a RetroAchievements
    account. No headphones and no external pad: the headphone jack and an external pad's autoconfig are not verified
    on the device. The jack's `AUDIO_REINIT` gets a stand-in, the command written to RetroArch's FIFO by hand.
15. Which commit, now that master is past `a81e450`? *Answered* (recheck, 24 Oct 2026): `71f521e`, for the
    `--log-file` fix and the stopped-driver fix (oversight 10).
16. Master's four new hotkeys and upstream's RetroArch hotkeys Off? *Answered* (recheck): a second `kiwami:` line in
    `run-retroarch`, from a file only the master build installs (oversight 9).
17. Device tests for upstream's 0.4.2 paths? *Answered* (recheck): all three: hotkeys Off in a game, Reset
    RetroArch, dark on the charger mid-game (oversights 9 and 11).
18. Which v1.22.2 image is the baseline, now that the device's 0.4.1 image differs from master's in more than
    RetroArch? *Answered* (planning, 24 Oct 2026): `kiwami` at 0.4.2, built from this branch before any of its changes
    to the image. Its sizes are the footprint baseline.
19. Should the build check that master's hotkeys are exactly upstream's list plus the four? *Answered* (planning): yes.
    A build script compares master's `DECLARE_META_BIND` names with `run-retroarch`'s list and the fork's four, and
    stops the build when they differ, so a later bump with a new hotkey can't get past RetroArch hotkeys Off.
