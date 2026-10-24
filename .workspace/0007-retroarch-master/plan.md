# 0007 RetroArch master: plan

Issue: #7 · Branch: `k/0007-retroarch-master` · Investigation: [investigation.md](investigation.md) · Status: approved

Rewritten on 24 Oct 2026, after the investigation's recheck: the pin is `71f521e` instead of `a81e450`, the fork has
taken upstream's 0.4.2, and that brings a step for master's hotkeys (oversight 9) and device tests for 0.4.2's paths
around a game (oversight 11). Nothing of the 22 Oct plan had been implemented.

## Summary

The fork's image runs RetroArch's master at `71f521e` (24 Oct 2026) instead of upstream's v1.22.2, but only if nothing
gets worse when it's measured on the RG DS Plus against v1.22.2. It's a build option,
`BR2_PACKAGE_KIWAMI_RETROARCH_MASTER` in `kiwami/Config.in`. Its default (off) is upstream's v1.22.2, and
`kiwami/configs/kiwami.fragment` turns it on. The image holds one RetroArch.

The approach chosen in the investigation (option A) has six parts:

- **The pin.** The fragment sets Buildroot's package override file, `BR2_PACKAGE_OVERRIDE_FILE`, to the fork's
  `kiwami/local.mk`. Buildroot reads that file before any package. With the option on, it holds
  `override RETROARCH_VERSION = 71f521e...`.
- **The fork's patches and hash.** `kiwami/external.mk` limits RetroArch's patch step to `kiwami/patches/`, which holds
  upstream's 0002 unchanged and 0003 rebased (0001 is no longer needed). It also points the download check at the
  fork's hash file.
- **Configure options.** RetroArch is built with `--disable-keychain`, and without the new built-ins of master's that
  Banana can't use.
- **The credentials.** A fork script folds master's `retroarch-keychain.cfg` back into `retroarch.cfg`. It runs from one
  `kiwami:` line at the start of upstream's `ra-secrets scrub`, which all four callers go through: `run-retroarch`
  before and after a game, `share up`, and Reset RetroArch. File sharing vetoes the keychain files.
- **Master's hotkeys.** One `kiwami:` line in `run-retroarch` adds master's four new hotkeys to upstream's list, so
  RetroArch hotkeys Off covers them. The names come from a file that only the master build installs, and the build
  stops if master's hotkeys aren't exactly upstream's list plus these four.
- **A guard.** CI fails when upstream changes its RetroArch package or patches under the fork's pin.

There's no banana-ui change and no settings row, and nothing goes in `ui/rnds/`, `ui/assets/` or the ROCKNIX-synced
files.

### Refinements over the investigation

- **The bar is measured at each system's own profile** (the user's choice, question 13). `systems.conf` runs GBA under
  `balanced` and SNES under `performance`, not `powersave`, so the investigation's "fps under `powersave` for each game"
  applies to NES, Game Boy Color and Mega Drive only. GBA and SNES are also measured under `powersave`, as information.
- **The measurement set** (questions 10-12) is 10 runs:
  - NES: Castlevania and Super Mario Bros, on FCEUmm and Mesen;
  - Game Boy Color: Link's Awakening DX, on Gambatte and SameBoy;
  - Mega Drive: Sonic 3 & Knuckles, on PicoDrive and Genesis Plus GX;
  - GBA: Pokémon Emerald, on gpSP;
  - SNES: Super Mario World, on Snes9x.
  **N64 and PS1 are left out**, so they're not verified on master.
- **"No regression", in numbers.** In every run:
  - master's lowest fps is at least v1.22.2's;
  - its average CPU share is at most v1.22.2's plus the run-to-run spread. Castlevania on FCEUmm is measured twice on
    v1.22.2 to get the spread, and it counts as at least 2 points;
  - its peak memory (`VmHWM`) is at most 110% of v1.22.2's.
- **The baseline is kiwami at 0.4.2** (the user's choice, question 18). It's an image built from this branch after step
  1. Only `.workspace/` differs from `kiwami` at that point, so the image is `kiwami`'s at `848af54`. The earlier
  plan's 0.4.1 image would differ from master's in more than RetroArch: 0.4.2 changed `run-retroarch`'s hotkeys, Reset
  RetroArch and bananad's dark-on-charger. That build's sizes are the footprint baseline: the retroarch binary, the
  `rootfs.squashfs` and the `NEEDED` libraries. 0006's 172,720,128-byte rootfs and the 0.3.0 build's 15,008,688-byte
  binary are from before 0.4.2.
- **The two update packages are told apart by name.** Both images say 0.4.2 (`VERSION`). The packages are copied to the
  card as `banana-rgds-plus-0.4.2-v1222.update.tar` and `banana-rgds-plus-0.4.2-ramaster.update.tar`. Update lists the
  part after the board as the version (`update list`), so the menu shows which one is which.
- **FLAC stays.** Master's CHD reader decodes CD-audio hunks with `rflac` (`Makefile.common:2293-2310` at `71f521e`).
  With `--disable-rflac`, RetroArch's own CHD reading (RetroAchievements' hashing of a CHD game) could fail on images
  with FLAC hunks. So "media decoders off" means the other ten: AV1, VP9, MP4, WebM, MPEG-1, AAC, Opus, Vorbis, MP3
  and MOD. WAV, the format of the menu sounds, stays as in v1.22.2.
- **No version subfolder for the patches.** The investigation had `kiwami/patches/retroarch/<sha>/`. Buildroot uses a
  version folder when one exists and the parent folder otherwise (`pkg-utils.mk:166-170`), so a bump that forgot to
  rename the folder would silently build with *no* patches. In `kiwami/patches/retroarch/`, the patches apply to
  whatever version is pinned, and a bump they don't fit fails the build.
- **The patch folder is `private` to RetroArch's patch step.** A plain target-specific `BR2_GLOBAL_PATCH_DIR` would also
  reach every package built as a prerequisite of that step, taking upstream's patches away from them. `private` stops
  that. This was tried on a scratch config with `make -n` on the patch stamp: two folders were applied, the package's
  own (`buildroot/package/retroarch/`, which holds no patches) and the fork's. Upstream's `buildroot/patches/retroarch/`
  was not applied.
- **Master's version is stamped.** Master's `version.all` still says `1.22.2`, and its tarball has no `.git`, so the
  binary's version string would be the same as v1.22.2's. The build step gets `GIT_VERSION` as a `private export`
  variable: RetroArch's `Makefile:83-85` takes it from the environment. With GNU Make 4.4.1, such a variable reaches the
  step's recipe and not its prerequisites (tried). `retroarch --version` and `retroarch.log` then say `(Git 71f521e)`,
  so every device log shows which RetroArch ran.
- **The guard also watches the joypads' pin.** The fork's hash file has to list upstream's joypad tarball
  (`RETROARCH_JOYPADS_VERSION`), so a bump of it upstream would break the fork's download. The guard flags that in CI
  first. Its stamps live in the guard's script, next to the message that says what to do.
- **In the fold, the keychain file always wins.** RetroArch writes the keychain file and strips `retroarch.cfg` in the
  same save, so the keychain's values are the latest. A key in both files ends up with the keychain's line only. Every
  `key = "value"` line is folded, the empty ones too. Without sealing, master writes all of its sensitive keys
  (`netplay_password = ""` ...), which v1.22.2 kept in `retroarch.cfg` anyway. Master also reads the existing keychain
  file before it writes one (`config_save_credentials`, `configuration.c:9092-9109`). A file left by a run cut short is
  therefore carried over and still folded at the next scrub.
- **Master's hotkeys: the names live in `kiwami/external.mk`, checked at build time** (the user's choice, question 19).
  `KIWAMI_RETROARCH_HOTKEYS` lists the four. A post-install hook of RetroArch's, defined only with the option on, runs
  `kiwami/retroarch-hotkeys.sh`. The script compares master's `DECLARE_META_BIND` names in the built source's
  `configuration.c` with `run-retroarch`'s `HOTKEYS` list plus the four. The three that Off keeps (`enable_hotkey`,
  `menu_toggle`, `exit_emulator`) are left out of the comparison. Then it writes `/usr/share/kiwami/ra-hotkeys`. Tried
  on the extracted sources: v1.22.2's 55 names are exactly upstream's list, and `71f521e`'s 59 are that list plus the
  four. With the option off there's no hook and no file, so `run-retroarch` is upstream's.
- **Three things in the bar can't be tested on the device**, given what's at hand (question 14):
  - the headphone jack, and an external pad (neither is at hand);
  - N64 and PS1 (left out of the set).
  The jack gets a stand-in: `AUDIO_REINIT` written by hand to RetroArch's FIFO (`/run/banana/retroarch.cmd`). That's
  the command bananad writes on a jack change (`system/bananad/bananad.c:663`). For the pad, the image keeps the same
  autoconfig files.
- **Two paths of 0.4.2's aren't tested on the device**:
  - the dark-on-charger low-battery shutdown mid-game, which needs a charger too weak for the handheld and a nearly
    empty battery. Its effect on the credentials (a keychain file left at shutdown) is the "game cut short" case of the
    headless fold test;
  - Reset RetroArch with a keychain file present. The after-game scrub always folds the file first, so M14 puts one
    back by hand before the reset.
- **A script for the device measurements**, `.workspace/0007-retroarch-master/measure.sh` (step 1). It makes the
  baseline's runs and master's the same, and keeps secrets out of what is collected.

## Acceptance criteria

- **A1** (the option): `BR2_PACKAGE_KIWAMI_RETROARCH_MASTER` is in Kiwami's menu, depends on `BR2_PACKAGE_RETROARCH`
  and defaults to off. The fragment sets it on, with `BR2_PACKAGE_OVERRIDE_FILE="$(BR2_EXTERNAL_KIWAMI_PATH)/local.mk"`.
  Both survive Buildroot's resolution on `rgds`, `rgds_plus` and `universal`. With the option on, each configured board
  resolves RetroArch as follows:
  - `RETROARCH_VERSION` is `71f521e726c2b1886ee7a2484195ab63abc92127`;
  - the download is checked against `kiwami/patches/retroarch/retroarch.hash` alone;
  - the patch step applies `buildroot/package/retroarch/` and `kiwami/patches/retroarch/`, never
    `buildroot/patches/retroarch/`;
  - `RETROARCH_CONF_OPTS` ends with the fork's options (A3);
  - `RETROARCH_POST_INSTALL_TARGET_HOOKS` has the hotkeys' hook, and `KIWAMI_RETROARCH_HOTKEYS` lists
    `video_filter_toggle headset_recenter laser_pointer_toggle headset_view_toggle` (A9).
- **A2** (off is upstream's): with the option off, RetroArch resolves as upstream's:
  - version `v1.22.2`;
  - Buildroot's default hash files;
  - `buildroot/patches/retroarch/` applied;
  - upstream's `RETROARCH_CONF_OPTS`, unchanged;
  - no hotkeys hook.
  Either way, an override file the user keeps in the output folder (`$(CONFIG_DIR)/local.mk`, Buildroot's default) is
  still read.
- **A3** (the build): `make retroarch-rgds-plus` does the following:
  - it checks both tarballs against the fork's hash file and applies `0002-...` and `0003-...` from
    `kiwami/patches/retroarch/` with no fuzz;
  - it configures with `HAVE_KEYCHAIN = 0`, and with the MCP server, the NFS and SMB clients, the RAR reader, GameMode
    and the ten decoders off;
  - it keeps 7z, CHD with FLAC, RetroArch's own TLS client and stdin commands (`HAVE_STDIN_CMD`);
  - it builds an aarch64 `retroarch` whose `NEEDED` libraries are all ones the baseline image already ships and whose `strings` hold
    `71f521e`, and installs `/usr/share/kiwami/ra-hotkeys` with the four names.
  Its size is recorded against the baseline's (step 1).
- **A4** (the guard): `kiwami/tests/ci.sh` fails when any of these differs from what the fork last checked:
  - upstream's `RETROARCH_VERSION` (`v1.22.2`);
  - its `RETROARCH_JOYPADS_VERSION` (`f3b62b8...`);
  - the files of `buildroot/patches/retroarch/` plus any `*.patch` in `buildroot/package/retroarch/` (a sha256).
  Its message names what changed and what to look at.
- **A5** (the fold): given `/data/retroarch/retroarch-keychain.cfg`, `ra-secrets scrub` first moves the file's
  `key = "value"` lines into `retroarch.cfg`, each key once, with the keychain's value. It then deletes the file and any
  `retroarch-keychain.key`, and logs one line that names no value. These hold:
  - values with `& | \` are carried byte for byte;
  - a last line with no newline isn't glued to;
  - with no `retroarch.cfg`, nothing is done, and the keychain stays (it's vetoed, A8).
- **A6** (the credentials' end state): after a scrub that found the keychain:
  - the account's password and token are in `banana/accounts.conf` (the last login wins, as upstream's scrub decides);
  - the username is in `retroarch.cfg`;
  - no password or token is in `retroarch.cfg`, and no `retroarch-keychain.*` is left on the card.
  That holds for each of the four callers: `run-retroarch` before and after a game, `share up`, and Reset RetroArch,
  whose backup folder gets neither a secret nor a keychain file.
- **A7** (no keychain is upstream's): with no keychain file, `ra-secrets scrub` leaves `retroarch.cfg` and
  `accounts.conf` byte-identical to what upstream's script (without the hook) leaves, and the fold logs nothing.
- **A8** (file sharing): the image's share script vetoes `*retroarch-keychain.*` next to `*tailscaled.state`, each
  once. A second post-build run changes nothing, and the build stops if the veto line is gone or changes shape. On the
  device, over SMB, neither keychain file is listed or opened.
- **A9** (master's hotkeys): with `/usr/share/kiwami/ra-hotkeys` present and RetroArch hotkeys Off, `run-retroarch`
  covers master's four hotkeys as it does upstream's 55:
  - each of the four is `nul` (`_btn` and `_axis`) in the appended config;
  - each is left out of the pad profiles' copy;
  - a card line for one of them (bound in RetroArch's menu) is kept aside before the game and put back after it,
    exactly once, with no `nul` left.
  Without the file, `run-retroarch` does what upstream's does, byte for byte. `kiwami/retroarch-hotkeys.sh` stops the
  build, naming the hotkey, when master has a hotkey that neither upstream's list nor the four name, when one of the
  four is gone from master, or when one of the four is already in upstream's list. On the device, with Off, only F + X
  and F + Start do anything, and a hotkey of master's bound to a face button is `nul` during the game and back after
  it. With On, F + L/R/Y/L2/R2 work as on v1.22.2.
- **A10** (speed): in each of the 10 runs at the system's profile, master meets the bar (*Refinements*: fps, CPU share,
  `VmHWM`) against v1.22.2, on the same card and scene. GBA and SNES under `powersave` are recorded as information.
- **A11** (audio): at 32, 64 and 128 ms audio latency, master plays without crackle wherever v1.22.2 did, with no more
  underruns logged. After `AUDIO_REINIT` is written to the FIFO three times mid-game, the sound carries on. That's the
  headphone jack's stand-in; the jack itself isn't verified.
- **A12** (sleep): the lid is closed mid-game for 10 s and opened, once with the game panel on and once with Second
  screen off. Each time the game comes back with picture and sound. With the game panel on, `retroarch.log` has no
  `[KMS] Display changed`.
- **A13** (dark on the charger): on the charger, a game is put to sleep with the lid, and then with the power button.
  The screens go dark and the handheld stays awake (`sleep.log`). On waking, the game resumes with sound and no
  crackle, no button is stuck or lost after the grab is released, and in game-panel mode the tiles still answer. All
  of this as on v1.22.2.
- **A14** (VSync off, fast-forward): with VSync off the game runs at its normal speed, as on v1.22.2, and any tearing
  is noted. Fast-forward (patch 0002) runs at least as fast as on v1.22.2.
- **A15** (the game panel):
  - RetroArch draws on the leased panel;
  - the five tiles work (save state, load state, menu, fast-forward, quit);
  - a touch on the panel doesn't reach RetroArch;
  - the achievements list behaves as on v1.22.2 (investigation, oversight 5).
- **A16** (RetroAchievements): with the setup's account, over master's own TLS client:
  - the game logs in and is identified, and an achievement unlocks;
  - afterwards the card is as A6 says;
  - a login made in RetroArch's own menu ends up in `accounts.conf`.
- **A17** (Reset RetroArch): after a game with an account, and with a keychain file present, Reset RetroArch leaves a
  backup folder with no password, token or `retroarch-keychain.*`. The next game logs in again. As on v1.22.2.
- **A18** (the rest of what Banana uses): on master the following work:
  - a `.7z` game starts (0003);
  - Quick resume resumes;
  - Settings > Other systems > Shader applies its preset;
  - the Online Updater lists the same entries as on v1.22.2;
  - a game starts with a fresh `retroarch.cfg` (the image's template);
  - the image holds the same joypad autoconfig files as the baseline's;
  - after a game-panel run, `retroarch.cfg` has no `log_to_file = "true"` (oversight 10).
- **A19** (back to v1.22.2): after the v1.22.2 update is installed over a card master has used, a game starts and
  RetroAchievements logs in.
- **A20** (footprint, upstream's look):
  - the rootfs's size is recorded against the baseline's, and nothing new runs while idle;
  - `make test`'s renders are identical to `kiwami`'s from before the feature;
  - the only new `kiwami:` hooks are `ra-secrets`' and `run-retroarch`'s second one.
- **A21** (information, suggestion 3): Mesen (Castlevania) and SameBoy (Link's Awakening DX) on master without threaded
  video, under `powersave`. Their fps and CPU share are recorded. Dropping their overrides would be a later change.

## Risks

- **Performance and footprint.**
  - Frame time on master could go either way, and only the device shows it: M4 and M5, against the bar. Master's
    threaded video and GL driver were reworked on 23 Oct (`ffbe995`, `01f36ab`), a day before the pin, and the fork
    runs Mesen and SameBoy threaded. M4 measures them threaded and M5 unthreaded.
  - The binary: it's measured after step 3 and again after step 5, which should shrink it. Both are compared with the
    baseline's (step 1).
  - Memory: `VmHWM` in every run.
  - Idle: nothing new runs. RetroArch only runs during a game, and the fold only during a scrub (before and after a
    game, at share up, at Reset RetroArch).
  - Build time: one RetroArch build, as now. Switching the option changes `RETROARCH_DIR`, so the first build after a
    switch builds RetroArch from scratch.
- **Conflicts with upstream.**
  - Merges: two hook lines.
    - `ra-secrets` has had one upstream commit, and 0.4.2 left it alone. A conflict there is resolved by putting the
      line back at the start of `scrub)`.
    - `run-retroarch` is upstream's busiest launcher, and 0.4.2 added 41 lines right where the hook goes. The hook sits
      on the line after upstream's `HOTKEYS` list, so a change to the list conflicts textually next to the hook. That's
      wanted: whoever merges sees both lists. If upstream adds one of master's four, `retroarch-hotkeys.sh` stops the
      next master build, and the fix is to drop it from `KIWAMI_RETROARCH_HOTKEYS`.
  - Changes the merge doesn't flag: a version or patch change in upstream's RetroArch merges silently, and the guard
    (A4) fails CI on it. `share`'s veto line is edited at build time, and post-build stops on a change of shape (A8).
  - Overlap: `kiwami/local.mk` takes over the package override file. If upstream sets `BR2_PACKAGE_OVERRIDE_FILE` one
    day, the fragment replaces it. `configure.sh` doesn't catch that, but the merge's defconfig diff (in
    `docs/FORK.md`'s list) does.
  - Upstream's 0.4.2 isn't in its `main` yet (it came from upstream's `release-0.4.2` branch). When upstream merges it
    there, `main` catches up with what `kiwami` already has, and the two hooks sit on 0.4.2's lines.
- **The override mechanism.** Using `override` in the override file is less common: Buildroot documents the file for
  `<pkg>_OVERRIDE_SRCDIR`. A Buildroot bump could change when the file is read. `retroarch-pin.sh` resolves the version
  on every board, in CI, each time (A1, A2).
- **The patch-dir trick** relies on Buildroot taking `BR2_GLOBAL_PATCH_DIR` when the patch recipe runs
  (`pkg-generic.mk:239-241`). `retroarch-pin.sh` checks the patch step's folders with `make -n` in CI, so a Buildroot
  change that breaks it fails there, not on the device.
- **A stale hotkeys file.** `/usr/share/kiwami/ra-hotkeys` is written into the target folder when RetroArch is
  installed. An output built with the option on, then switched off and rebuilt without a clean, still holds the file.
  Buildroot asks for a clean build after a configuration change anyway. The leftover file is harmless on v1.22.2:
  - the four extra `nul` lines name keys v1.22.2 doesn't know;
  - the put-back's pattern only matches more lines, and those are put back as they were.
- **Master is master.** It may carry bugs that a release would fix. `a81e450` had one that reached Banana (oversight
  10), and master adds about 80 commits a day. The bar means a regression isn't shipped: either it's fixed as a
  deviation, or the fragment turns the option off, which puts the image back to v1.22.2 with no other change.
- **The credentials.**
  - The secrets stay on the card in `retroarch.cfg` for as long as the after-game scrub takes, as on v1.22.2.
  - The keychain file lives from RetroArch's exit (or a "Save configuration" in its menu) until the next scrub. File
    sharing vetoes it (A8). A card put in a computer still shows it, as it shows `accounts.conf`.
  - A dark-on-charger shutdown mid-game leaves the keychain file until the next scrub. That's the headless test's
    "game cut short" case, not a device test.
  - The fold's tests (A5-A7) cover the rest.
- **The rebased 0003** could leave an Online Updater entry that can't work, or hide one that can. M15 compares the
  entries with M2's.
- **The disabled built-ins.** A configure flag that doesn't exist makes master's `configure` stop ("Unknown option",
  `qb/qb.params.sh:86`), so a bump that drops one fails the build loudly. A built-in that turns out to be needed shows
  up in the device tests. FLAC is kept for CHD.
- **Hardware and boards.**
  - On the device, master's KMS context runs on the lease, its hotplug monitor runs after resume (M7), its async flips
    run with VSync off (M9), and its ALSA driver runs after SIGSTOP/SIGCONT and `AUDIO_REINIT` (M6, M7, M8). M8 also
    covers the input grab being released.
  - Not verified on the device: the headphone jack, an external pad, N64, PS1, the RG DS, and the dark-on-charger
    low-battery shutdown. The RG DS shares the SoC, the lease and the ALSA path, and gets the build check only.
- **TLS.** Master's own TLS client could fail the login (M11). The decided fallback is `--enable-mbedtls` against
  Buildroot's `mbedtls`, done as a deviation with its `DEPENDENCIES.md` line.

## Steps

Each step is one commit, and leaves the tree building and `make test` passing.

### 1. The measurements' script

- **Files**: `.workspace/0007-retroarch-master/measure.sh` (new).
- **Change**: a POSIX `sh` script, copied to the card as `/data/banana/measure.sh` and run over SSH as
  `sh /data/banana/measure.sh <verb> ...`. It's the same on both images. It writes everything to
  `/data/banana/logs/ra-<name>.*`, and never writes a secret's value. The verbs:
  - **`run <name> [seconds]`** (default 30), during a game. It writes `ra-<name>.txt`:
    - a header: the date, `/etc/banana/version`, `retroarch --version` (master says `(Git 71f521e)`), the CPU governor
      and `scaling_max_freq`, and `video_threaded` from `retroarch.cfg` and from the Mesen and SameBoy override files;
    - one line per second: the CPU clock (`policy0/scaling_cur_freq`), the GPU clock
      (`/sys/class/devfreq/*.gpu/cur_freq`), and RetroArch's CPU share over that second (the change in
      `/proc/<pid>/stat`'s utime and stime, at `getconf CLK_TCK`);
    - at the end, the average share, `VmHWM` and `VmRSS`.
    If no `retroarch` is running, it stops with a message.
  - **`powersave`**: runs `banana-perf powersave` for the running game. That's for the information runs; the system's
    own profile comes back with the next game.
  - **`reinit`**: writes `AUDIO_REINIT` to `/run/banana/retroarch.cmd`, three times, 5 s apart, with a line in
    `ra-reinit.txt` each time. That's the FIFO `run-retroarch` and banana-ui give RetroArch in both panel modes.
  - **`hotkeys <name>`**, during a game: writes `ra-<name>-hotkeys.txt` with the following:
    - the `input_*` lines of `/run/banana/retroarch-hotkeys.cfg` for master's four hotkeys and for `save_state`, or "no
      Off config";
    - for each file in `/run/banana/ra-autoconfig/udev/`, whether it still has a line for any hotkey in `run-retroarch`'s
      list or the four;
    - the lines of `/data/retroarch/.banana-hotkeys.keep`, or that there's none.
    Hotkey binds aren't secrets, so their values are shown.
  - **`log <name>`**, after a game: copies `/run/banana/retroarch.log` (game panel on) to `ra-<name>.log`. `sed` masks
    `cheevos_password`, `cheevos_token`, and the `t=`/`p=` parameters of any URL.
  - **`card <name>`**, after a game: writes `ra-<name>-card.txt` with the following:
    - `ls -la /data/retroarch/`;
    - for each `cheevos_*` key in `retroarch.cfg` and each `ra_*` key in `accounts.conf`: the key, then `<set>`,
      `<empty>` or nothing in place of its value, and how many lines it has;
    - whether `retroarch-keychain.cfg` and `.key` exist, and whether `.banana-hotkeys.keep` does;
    - `retroarch.cfg`'s `log_to_file`, `joypad_autoconfig_dir` and `input_<hotkey>_btn`/`_axis` lines, values shown;
    - for each `backup-*` folder: `ls -laR`, and its `retroarch.cfg`'s `cheevos_*` keys masked as above;
    - the running share config's `veto files` line (`/run/banana/ksmbd.conf`, when sharing is on).
  The script has no `kiwami:` code and isn't part of the image.
- **Tests**:
  - `sh -n` on the script.
  - On the host, against a running `sleep 60` named by `RA_PID` (the script's own test hook): `run x 3` prints a header
    and 3 lines.
  - On a made-up folder (`RA_DATA`, `RA_RUN`):
    - `card` prints `<set>` for a set password and `<empty>` for an empty one, and no value; a `backup-1/retroarch.cfg`
      with a password shows it as `<set>` too;
    - `hotkeys` lists a `nul` line for `video_filter_toggle`.
  - `make test` (unchanged).
  - **The baseline** (question 18), after the commit:
    - `make rgds-plus`;
    - the update package copied as `output/baseline-0.4.2/banana-rgds-plus-0.4.2-v1222.update.tar`;
    - recorded in the plan's notes: `du -b output/rgds-plus/target/usr/bin/retroarch`, `rootfs.squashfs`'s size, the
      binary's `readelf -d` `NEEDED` list, and `ls output/rgds-plus/target/usr/share/libretro/autoconfig/udev | wc -l`.
    M1-M3 run on it before step 2's changes reach the device.
- **Commit**: `RetroArch master: a script for the device measurements, the same on both versions`

### 2. Master's credentials file folded back into retroarch.cfg

- **Files**:
  - `kiwami/rootfs/usr/libexec/kiwami/ra-keychain` (new, mode 755);
  - `system/rootfs/usr/libexec/banana/ra-secrets` (hook);
  - `kiwami/post-build.sh` (changed);
  - `kiwami/tests/ra-keychain.sh` (new, mode 755);
  - `kiwami/tests/ci.sh` (changed);
  - `docs/FORK.md` (changed).
- **Change**:
  - **The hook**: one line in `ra-secrets`, the first in `scrub)`, between `scrub)` (`:14`) and
    `[ -f $CFG ] || exit 0` (`:15`):
    ```sh
        [ -x /usr/libexec/kiwami/ra-keychain ] && /usr/libexec/kiwami/ra-keychain $CFG	# kiwami: RetroArch master's credentials file back into retroarch.cfg
    ```
    It's the one place all four callers go through: `run-retroarch:94` and `:175`, `share:24` and `ra-reset:15`. On
    v1.22.2 the script finds no keychain file and does nothing, so this step changes nothing until step 3.
  - **`ra-keychain <retroarch.cfg>`**, in the shape of `core-overrides`:
    ```sh
    #!/bin/sh
    # RetroArch master's credentials file back into retroarch.cfg: ra-secrets calls it (a kiwami: hook) at the start of
    # a scrub, which then moves the password and token to accounts.conf as it does on v1.22.2.
    #   ra-keychain <retroarch.cfg>
    # Master writes its "sensitive" settings (cheevos_username, _password, _token and 16 more) to retroarch-keychain.cfg
    # beside retroarch.cfg on every save, and takes them out of retroarch.cfg; the fork builds it unsealed
    # (--disable-keychain). Their lines go back into retroarch.cfg, the keychain's value winning (it is the latest),
    # and the file goes, with a key file a sealed build would have left. Upstream's scrub, file sharing's veto and the
    # game panel (cheevos_username in retroarch.cfg) then see what v1.22.2 left. (.workspace/0007-retroarch-master/)
    CFG=$1
    K=${CFG%/*}/retroarch-keychain.cfg
    rm -f "${CFG%/*}/retroarch-keychain.key"
    [ -f "$K" ] && [ -f "$CFG" ] || exit 0
    awk 'FILENAME == ARGV[1] { if (/^[A-Za-z0-9_]+ = ".*"$/) { if (!($1 in v)) o[++n] = $1; v[$1] = $0 } next }
         !($2 == "=" && $1 in v) { print }
         END { for (i = 1; i <= n; i++) print v[o[i]] }' "$K" "$CFG" > "$CFG.kiwami" &&
        mv "$CFG.kiwami" "$CFG" && rm -f "$K" || { rm -f "$CFG.kiwami"; exit 0; }
    echo "kiwami: ra-keychain: retroarch-keychain.cfg folded into retroarch.cfg" >&2
    ```
    The exact shape is settled in this step. Already settled:
    - a last `retroarch.cfg` line with no newline gets one first, as `core-overrides` does;
    - a failure leaves both files as they were, and the scrub carries on;
    - the log line names no key or value; from `run-retroarch` it goes to banana-ui's log;
    - a key file without a keychain file is removed too: it's a secret, and only a sealed build makes one.
  - **`kiwami/post-build.sh`**: the three Tailscale lines (`:12-15`) become a `veto <name> <regex>` function, called
    twice:
    - `veto '*tailscaled.state' '\*tailscaled\.state'`;
    - `veto '*retroarch-keychain.*' '\*retroarch-keychain\.\*'`.
    Each name is appended to upstream's line when the line doesn't have it yet. It's then checked anywhere on the line
    (`^	veto files = /.*/<regex>/`) rather than at its end, since two names now follow upstream's. The single-line check
    before them (`:12-13`) stays as it is. The comment says what the keychain files are.
  - **`kiwami/tests/ra-keychain.sh`**, against a made-up card under a `mktemp -d`. It runs two copies of `ra-secrets`.
    In both, `sed` rewrites the paths in one pass: `s#\(^\|[^A-Za-z0-9_.]\)/\(data\|usr\)/#\1$X/\2/#g`. One pass means
    a `TMPDIR` under `/run` or `/usr` can't be rewritten twice. The hooked copy reaches the fork's script at
    `$X/usr/libexec/kiwami/ra-keychain`, a copy of `kiwami/rootfs/usr/libexec/kiwami/ra-keychain`. Upstream's copy
    has the hook line taken out. The cases:
    - **no keychain**: a `retroarch.cfg` with `cheevos_username = "alice"` and `cheevos_password = "pw1"`, and an
      `accounts.conf`. The hooked and the upstream copy leave byte-identical `retroarch.cfg` and `accounts.conf`, and
      `ra-keychain` logs nothing (A7). `ra-keychain` alone, on a `retroarch.cfg` with no secrets, leaves it
      byte-identical;
    - **folded**: `retroarch.cfg` as master leaves it (no `cheevos_username`), and a keychain file with
      `cheevos_username = "alice"`, `cheevos_password = "pw1"`, `cheevos_token = "tok1"`, `netplay_password = ""` and
      `kiosk_mode_password = ""`. After `ra-keychain` alone:
      - `retroarch.cfg` has the five lines once each, in order, after its own lines;
      - the keychain and a `retroarch-keychain.key` are gone;
      - the one log line is there, without `pw1` or `tok1` (A5);
    - **through the scrub**, with the same files: `accounts.conf` gets `ra_user=alice`, `ra_password=pw1` and
      `ra_retroarch_token=tok1`. `retroarch.cfg` has `cheevos_username = "alice"` and no password or token line. No
      `retroarch-keychain.*` is left (A6);
    - **another account** in the keychain (`bob`, `pw2`, `tok2`), with `alice` in `accounts.conf`: the result is
      `ra_user=bob`, `ra_password=pw2`, `ra_retroarch_token=tok2` (the last login wins) (A6);
    - **the keychain wins**: `retroarch.cfg` has `cheevos_username = "alice"` twice, and the keychain has `bob`. One
      `cheevos_username = "bob"` line is left (A5);
    - **special characters**: a password `p&w|d\x` is carried byte for byte into `accounts.conf` (A5);
    - **no final newline**: `retroarch.cfg`'s last line, `audio_latency = "64"` with no newline, is kept whole (A5);
    - **an empty keychain file**: `retroarch.cfg`, with a key twice and a line of another shape, is kept byte for byte,
      and the keychain file is gone (Deviations, 24 Oct 2026);
    - **no retroarch.cfg**: the keychain file stays as it is, nothing is made, and the exit is 0 (A5);
    - **a failed fold** (the folder not writable, skipped as root): both files as they were, no temporary, no "folded"
      log line, exit 0 (Deviations, 24 Oct 2026);
    - **idempotent**: a second scrub changes nothing (`cmp`) and logs nothing;
    - **the hook**: the line after `scrub)` in `system/rootfs/usr/libexec/banana/ra-secrets` is exactly the hook
      (`grep -A1`, as `core-overrides.sh` checks its own).
    On success it prints `ra-keychain: all <n> checks passed`. On a `FAIL` it lists the files and exits 1.
  - **`kiwami/tests/ci.sh`**:
    - it runs `kiwami/tests/ra-keychain.sh` after `core-overrides.sh` (`:17`);
    - in the post-build checks, `:42`'s tailscaled check goes from end-anchored to anywhere on the line, and a check
      for `/\*retroarch-keychain\.\*/` joins it;
    - each name is on the line exactly once (`grep -o | wc -l`);
    - a second post-build run leaves `share` byte-identical (`cmp`), as it does for `systems.conf`;
    - the run with the line taken out (`:64-65`) still fails.
  - **`docs/FORK.md`**, *Taking in upstream* step 3:
    - the share script's sentence names the keychain files next to tailscaled's state;
    - `system/rootfs/usr/libexec/banana/ra-secrets` joins the `git diff $OLD main` list, with a sentence: if upstream
      reworks `scrub`, the hook has to stay first in it, and a new caller of the scrub gets the fold with no change.
- **Tests**:
  - `kiwami/tests/ra-keychain.sh` and the post-build checks, through `kiwami/tests/ci.sh .buildroot`;
  - `sh -n` on `ra-keychain` and `ra-secrets`, both by ci.sh and by CI's own check;
  - `make test`;
  - `docs/FORK.md`'s hook commands list `ra-secrets` with its `kiwami:` line.
- **Commit**: `RetroArch master: ra-secrets folds master's credentials file back into retroarch.cfg`

### 3. RetroArch's master, pinned behind a build option

- **Files**:
  - `kiwami/Config.in` (changed);
  - `kiwami/configs/kiwami.fragment` (changed);
  - `kiwami/local.mk` (new);
  - `kiwami/external.mk` (changed);
  - `kiwami/patches/retroarch/0002-gfx-drm_ctx-fast-forward-with-two-swapchain-images.patch` (new, a copy of upstream's);
  - `kiwami/patches/retroarch/0003-menu-Online-Updater-leaves-out-what-can-t-work-or-would-flood-the-card.patch` (new,
    rebased);
  - `kiwami/patches/retroarch/retroarch.hash` (new);
  - `kiwami/tests/retroarch-pin.sh` (new, mode 755);
  - `kiwami/tests/ci.sh` (changed);
  - `kiwami/DEPENDENCIES.md` (changed);
  - `docs/FORK.md` (changed).
- **Change**:
  - **`kiwami/Config.in`**, after the cores' `source` lines (`:6`), inside `menu "Kiwami"`:
    ```kconfig
    config BR2_PACKAGE_KIWAMI_RETROARCH_MASTER
    	bool "RetroArch's master instead of v1.22.2"
    	depends on BR2_PACKAGE_RETROARCH
    	help
    	  RetroArch built from its master branch at the commit kiwami/local.mk pins, with the fork's patches
    	  (kiwami/patches/retroarch/) instead of upstream's. Off, RetroArch is upstream's release.
    ```
  - **`kiwami/configs/kiwami.fragment`**, at the end: a comment (RetroArch's master, measured against v1.22.2 on the RG
    DS Plus; off puts upstream's back), then `BR2_PACKAGE_KIWAMI_RETROARCH_MASTER=y` and
    `BR2_PACKAGE_OVERRIDE_FILE="$(BR2_EXTERNAL_KIWAMI_PATH)/local.mk"`. Buildroot reads that path after `.config`, with
    the external path quoted, and `qstrip` takes the quotes out (tried on a scratch config).
  - **`kiwami/local.mk`**:
    ```make
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
    ```
  - **`kiwami/external.mk`**, after its `include` line (`:1`):
    ```make
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
    ```
  - **`kiwami/patches/retroarch/`**:
    - `0002-...` is byte-identical to upstream's (it applies at offset 102 on `71f521e`);
    - `0003-...` is rebased on `71f521e`. It keeps the same behaviour, on master's `MENU_ENUM_LABEL_*_STR` entries. Its
      header says what was rebased, and carries `Upstream-Status: Inappropriate [BananaOS-specific]`, as upstream's
      does;
    - upstream's 0001 isn't carried: master has `AUDIO_REINIT` (`command.h:1195`) and frees alsa-lib's configuration
      on close (`audio/drivers/alsa.c:740`).
  - **`retroarch.hash`**: `# Locally computed`, then the sha256 of `retroarch-71f521e726c2b1886ee7a2484195ab63abc92127.tar.gz`,
    of `f3b62b847ed21939b668b897b18bbc1defbaae05.tar.gz` and of `COPYING`. They're computed by downloading once and
    running `sha256sum`.
  - **`kiwami/tests/retroarch-pin.sh <buildroot dir> <output dir>...`**, for the outputs `ci.sh` has configured:
    - **on** (each output): `make -s -C <br> O=<o> printvars VARS='RETROARCH_VERSION RETROARCH_HASH_FILES
      RETROARCH_CONF_OPTS RETROARCH_TARGET_PATCH'`. The checks:
      - the version is `71f521e726c2b1886ee7a2484195ab63abc92127`;
      - the hash files are exactly the fork's;
      - `RETROARCH_CONF_OPTS` contains `--disable-keychain`;
      - `make -n` on the patch stamp: its `apply-patches.sh` lines name `buildroot/package/retroarch` and
        `kiwami/patches/retroarch`, and not `buildroot/patches/retroarch`, and its `dl-wrapper` lines have only the
        fork's `-H` (A1).
      That takes about 5 s per output, with nothing downloaded (tried);
    - **off** (the first output copied to `<o>-off`, the option set to `# ... is not set`, then `olddefconfig`):
      - the version is `v1.22.2`, the hash files are Buildroot's default pair, and the options have no
        `--disable-keychain`;
      - `make -n` applies `buildroot/patches/retroarch`;
      - before the configure, a `local.mk` in `<o>-off` sets `KIWAMI_TEST_LOCAL_MK = 1`, and printvars shows it, as it
        does in the `on` output (A2);
    - at the end it prints `retroarch-pin: <board> pinned, off is upstream's`.
    `kiwami/tests/ci.sh` calls it after the boards' loop (`:70-75`), with the three outputs.
  - **`kiwami/DEPENDENCIES.md`**:
    - **the fork's table** gets a new row: RetroArch (master), commit `71f521e`, 24 Oct 2026. It's pinned in
      `kiwami/local.mk` (`override RETROARCH_VERSION`, with `BR2_PACKAGE_KIWAMI_RETROARCH_MASTER`), with the sha256 of
      both tarballs and of `COPYING` in `kiwami/patches/retroarch/retroarch.hash`, from
      https://github.com/libretro/RetroArch. Its notes:
      - GPL-3.0+;
      - the patches: upstream's 0002 as it is and 0003 rebased, with 0001 not needed, and why;
      - `--disable-keychain` and the fold (`ra-keychain`, and the sensitive keys `configuration.c:1160` lists);
      - `GIT_VERSION`; the joypad tarball is upstream's pin, listed in the fork's hash file;
      - the same reliance as on v1.22.2 (7z, saves and states per core, per-core overrides over `--appendconfig`),
        checked at `71f521e`;
      - why `71f521e` and not `a81e450`: the `--log-file` row fix (`8b6754f`) and the stopped-driver audio fix
        (`0052ff6`);
      - what a bump checks: the patches, the keychain's format and keys, the guard's stamps, the hash.
    - **upstream's table**, the RetroArch row: while `BR2_PACKAGE_KIWAMI_RETROARCH_MASTER` is on (the fragment), the
      fork's pin above replaces it, and `kiwami/tests/retroarch-pin.sh` stops CI when this version moves.
  - **`docs/FORK.md`**:
    - *Where a customization goes* gets a row: "Another version of one of upstream's packages: `kiwami/local.mk`
      (Buildroot's package override file, set by the fragment; `override <PKG>_VERSION`), its patches and hash in
      `kiwami/patches/<package>/`, its options in `kiwami/external.mk`, behind an option in `kiwami/Config.in`".
    - *How the build takes in the fork* gets a sentence on the override file, and says a user's own
      `output/<board>/local.mk` is still read.
- **Tests**:
  - `kiwami/tests/ci.sh .buildroot`: the three boards keep both fragment lines (`configure.sh`), then
    `retroarch-pin.sh` runs.
  - `make retroarch-rgds-plus` (one board per call), its output kept in the session's scratchpad
    (`ra-master-build.log`). The checks:
    - the downloads are checked against the fork's hash file;
    - `Applying 0002-...` and `Applying 0003-...` come from `kiwami/patches/retroarch/`, with no fuzz or offset warning
      for 0003;
    - `output/rgds-plus/build/retroarch-71f521e.../config.mk` has `HAVE_KEYCHAIN = 0`, `HAVE_7ZIP = 1`,
      `HAVE_RETROSSL = 1`, `HAVE_STDIN_CMD = 1`, `HAVE_CHD = 1` and `HAVE_RFLAC = 1`;
    - on `output/rgds-plus/target/usr/bin/retroarch`: `file` says aarch64, each library of its `readelf -d` `NEEDED` is
      one the baseline image already ships (step 1; Deviations, 24 Oct 2026), `strings | grep 71f521e` finds the commit, and its size is recorded (`du -b`).
  - `make test` (unchanged, passing).
- **Commit**: `RetroArch master: master at 71f521e behind a build option, with the fork's patches for it`

### 4. RetroArch hotkeys Off covers master's four new hotkeys

- **Files**:
  - `system/rootfs/usr/libexec/banana/run-retroarch` (hook);
  - `kiwami/retroarch-hotkeys.sh` (new, mode 755);
  - `kiwami/external.mk` (changed);
  - `kiwami/tests/ra-hotkeys.sh` (new, mode 755);
  - `kiwami/tests/bin/retroarch` (new, mode 755, a stand-in);
  - `kiwami/tests/retroarch-pin.sh` (changed);
  - `kiwami/tests/ci.sh` (changed);
  - `kiwami/DEPENDENCIES.md` (changed);
  - `docs/FORK.md` (changed).
- **Change**:
  - **The hook**: one line in `run-retroarch`, right after the `HOTKEYS` list's closing line (`:116`,
    `toggle_statistics ... volume_up"`) and before `HK=` (`:117`). From there, `names=` (`:123`) and so the profile
    strip (`:135`), the `nul` lines (`:139`), the kept lines (`:141`) and the put-back (`:126`, `:178`) all take the
    names in:
    ```sh
    [ -f /usr/share/kiwami/ra-hotkeys ] && HOTKEYS="$HOTKEYS $(cat /usr/share/kiwami/ra-hotkeys)"	# kiwami: RetroArch master's hotkeys too
    ```
    Without the file (the option off), `HOTKEYS` is upstream's.
  - **`kiwami/retroarch-hotkeys.sh <configuration.c> <run-retroarch> <out file> <name>...`**, a build script next to
    `post-build.sh`:
    - master's hotkeys are `sed -n 's/^ *DECLARE_META_BIND( *[0-9]*, *\([a-z0-9_]*\),.*/\1/p'` on `configuration.c`,
      without `enable_hotkey`, `menu_toggle` and `exit_emulator`, then `sort -u`;
    - upstream's list is the `HOTKEYS="..."` block of `run-retroarch` (`sed -n '/^HOTKEYS="/,/"$/p'`, the quotes taken
      off, one word a line);
    - it stops (exit 1), with a message naming each hotkey concerned, in these cases:
      - master has a hotkey that neither list names ("master's hotkey <x> isn't in run-retroarch's list or
        KIWAMI_RETROARCH_HOTKEYS: add it to KIWAMI_RETROARCH_HOTKEYS in kiwami/external.mk");
      - one of the names isn't master's ("<x> is no hotkey of this RetroArch's: drop it");
      - one of the names is already in upstream's list ("run-retroarch lists <x> now: drop it from
        KIWAMI_RETROARCH_HOTKEYS");
    - otherwise it writes the names, one a line, to the out file (through a temporary, `mkdir -p` its folder).
    The comment says what it guards: RetroArch hotkeys Off promises no hotkey but F + X and F + Start, and a hotkey that
    no list names would still work.
  - **`kiwami/external.mk`**, in the option's block:
    ```make
    # master's hotkeys that v1.22.2 doesn't have, for Settings > Other systems > RetroArch hotkeys Off: run-retroarch (a
    # kiwami: hook) adds them to upstream's list from /usr/share/kiwami/ra-hotkeys. The build stops when master's
    # hotkeys aren't upstream's list and these (kiwami/retroarch-hotkeys.sh).
    KIWAMI_RETROARCH_HOTKEYS = video_filter_toggle headset_recenter laser_pointer_toggle headset_view_toggle
    define KIWAMI_RETROARCH_HOTKEYS_INSTALL
    	$(BR2_EXTERNAL_KIWAMI_PATH)/retroarch-hotkeys.sh $(@D)/configuration.c \
    		$(BR2_EXTERNAL_BANANA_PATH)/../system/rootfs/usr/libexec/banana/run-retroarch \
    		$(TARGET_DIR)/usr/share/kiwami/ra-hotkeys $(KIWAMI_RETROARCH_HOTKEYS)
    endef
    RETROARCH_POST_INSTALL_TARGET_HOOKS += KIWAMI_RETROARCH_HOTKEYS_INSTALL
    ```
    `run-retroarch` is read where upstream's defconfig takes its overlay from
    (`BR2_ROOTFS_OVERLAY="$(BR2_EXTERNAL_BANANA_PATH)/../system/rootfs"`, `banana_rgds_plus_defconfig:24`), since the
    overlay isn't in the target folder yet when RetroArch is installed.
  - **`kiwami/tests/bin/retroarch`**, a stand-in for `run-retroarch`'s `retroarch`. It copies each `--appendconfig`
    file and the folder named by an appended `joypad_autoconfig_dir` into `$KR_SEEN`. Then it does what RetroArch does
    on exit: the appended hotkeys config's lines go into the `--config` file, as RetroArch writes the binds it ran with.
  - **`kiwami/tests/ra-hotkeys.sh`**:
    - **the build script**, on a made-up `configuration.c`, written from `run-retroarch`'s list plus the four plus
      `enable_hotkey`, `menu_toggle` and `exit_emulator` (twice, as master's `#ifdef HAVE_LAKKA` has it), each as a
      `   DECLARE_META_BIND(2, <name>, ...)` line:
      - it passes, and the out file holds the four names, in order;
      - with a `rumble_toggle` line added, it exits 1, the message names `rumble_toggle`, and no out file is made;
      - with `headset_recenter` taken out, it exits 1, naming `headset_recenter`;
      - with `reset` given as a fifth name, it exits 1, naming `reset`;
    - **`run-retroarch` in a made-up root**: `$X` holds `data/`, `run/banana/`, `etc/banana/` and `usr/`. In it:
      - `etc/banana/retroarch.cfg` and `etc/banana/settings.conf` are copies of the repository's;
      - `data/banana/settings.conf` has `ra_hotkeys = off`;
      - `usr/lib/libretro/fceumm_libretro.so` is an empty file;
      - `usr/share/libretro/autoconfig/udev/pad.cfg` has `input_a_btn = "0"`, `input_enable_hotkey_btn = "9"`,
        `input_save_state_btn = "5"` and `input_video_filter_toggle_btn = "3"`;
      - `usr/libexec/banana/ra-secrets` is upstream's script, rewritten;
      - `usr/libexec/banana/one-panel` defines `one_panel_dark` and `one_panel_restore` as no-ops;
      - `PATH` starts with `kiwami/tests/bin`.
      The script is rewritten with the same one-pass `sed` as `ra-keychain.sh`, extended to
      `\(data\|run\|etc/banana\|usr\)`. The card's `retroarch.cfg` starts as the template plus
      `input_video_filter_toggle_btn = "2"` and `input_save_state_btn = "4"`. Each run is
      `sh <copy> fceumm /x/game.nes`, with Second screen off (no `BANANA_DRM_LEASE_FD`). The cases:
      - **with `usr/share/kiwami/ra-hotkeys`** (the four), the hooked copy:
        - the seen hotkeys config has `input_<h>_btn = "nul"` and `input_<h>_axis = "nul"` for each of the four;
        - the seen `pad.cfg` has no `input_video_filter_toggle_btn` or `input_save_state_btn` line, and keeps
          `input_a_btn` and `input_enable_hotkey_btn`;
        - while the game runs, `.banana-hotkeys.keep` has `input_video_filter_toggle_btn = "2"`;
        - afterwards, `retroarch.cfg` has `input_video_filter_toggle_btn = "2"` and `input_save_state_btn = "4"` once
          each, no `nul` line for either, no `/run` autoconfig folder, and no `.banana-hotkeys.keep` (A9);
      - **without the file**: the hooked copy and upstream's (the hook line taken out) leave byte-identical
        `retroarch.cfg`, seen configs and seen profiles. The seen hotkeys config has no `video_filter_toggle` line
        (A9: off is upstream's);
      - **hotkeys On** (`ra_hotkeys = on`), with the file: no hotkeys config is appended, and `retroarch.cfg` is
        byte-identical to its state before the run, apart from what `set_cfg` writes, which upstream's copy writes the
        same way (`cmp` against upstream's run);
      - **the hook's place**: the line after the `HOTKEYS` list's closing line is exactly the hook (`grep -A1`).
    On success it prints `ra-hotkeys: all <n> checks passed`. On a `FAIL` it lists the files and exits 1.
  - **`kiwami/tests/retroarch-pin.sh`**: the `on` check also reads `KIWAMI_RETROARCH_HOTKEYS` (the four) and
    `RETROARCH_POST_INSTALL_TARGET_HOOKS` (it contains `KIWAMI_RETROARCH_HOTKEYS_INSTALL`). The `off` check finds no
    such hook.
  - **`kiwami/tests/ci.sh`**: it runs `kiwami/tests/ra-hotkeys.sh` after `ra-keychain.sh`.
  - **`kiwami/DEPENDENCIES.md`**, the fork's RetroArch row: master's four hotkeys (`configuration.c:453-456`),
    `KIWAMI_RETROARCH_HOTKEYS`, and the build-time check that makes a bump with a new hotkey stop.
  - **`docs/FORK.md`**, *Taking in upstream* step 3, the sentence on `run-retroarch` (already in the list): the fork's
    second hook there sits on the line after upstream's `HOTKEYS` list, and adds master's hotkeys to it. If upstream
    adds one of them to its list, the master build stops (`kiwami/retroarch-hotkeys.sh`), and the name comes out of
    `KIWAMI_RETROARCH_HOTKEYS`.
- **Tests**:
  - `kiwami/tests/ra-hotkeys.sh`, through `kiwami/tests/ci.sh .buildroot` (with `retroarch-pin.sh`'s new checks);
  - `sh -n` on `run-retroarch`, `kiwami/retroarch-hotkeys.sh` and the stand-in;
  - `make retroarch-reinstall-rgds-plus`: `output/rgds-plus/target/usr/share/kiwami/ra-hotkeys` holds the four names,
    and the build log has no stop from the script;
  - `make test`.
- **Commit**: `RetroArch master: RetroArch hotkeys Off also turns off master's four new hotkeys`

### 5. Master without the built-ins Banana can't use

- **Files**: `kiwami/external.mk` (changed), `kiwami/tests/retroarch-pin.sh` (changed), `kiwami/DEPENDENCIES.md`
  (changed).
- **Change**:
  - **`kiwami/external.mk`**, in the option's block, with a comment for the group:
    ```make
    # what master builds in by default and Banana can't use: the MCP server, its own NFS and SMB clients (nothing that
    # listens or mounts on the network), media decoders (WAV for the menu sounds and FLAC, which CHD's CD audio needs,
    # stay), the RAR reader (RAR games aren't listed) and Feral GameMode (no daemon)
    RETROARCH_CONF_OPTS += --disable-mcp --disable-retronfs --disable-retrosmb --disable-smbclient \
    	--disable-rav1 --disable-rvp9 --disable-rmp4 --disable-rwebm --disable-rmpeg1 \
    	--disable-raac --disable-ropus --disable-rvorbis --disable-rmp3 --disable-rmodtracker \
    	--disable-rrar --disable-gamemode
    ```
    Each flag is a `HAVE_` in `71f521e`'s `qb/config.params.sh` (`:75`, `:94-96`, `:242`, `:255-266`, `:271`), and
    master's `configure` stops on an unknown one (`qb/qb.params.sh:81-86`, `opt_exists`). `--disable-smbclient` stops
    it looking for a system libsmb2 once its own client is off (`qb/config.libs.sh:1101-1121`). The stdin commands
    don't need the MCP server: `HAVE_COMMAND` comes from `HAVE_STDIN_CMD` (`config.libs.sh:238`).
  - **`kiwami/tests/retroarch-pin.sh`**: the `on` check also finds each of the 16 options in `RETROARCH_CONF_OPTS`, and
    finds no `--disable-rflac`.
  - **`kiwami/DEPENDENCIES.md`**, the fork's RetroArch row: the disabled built-ins, why FLAC and WAV stay, and that a
    bump re-reads `qb/config.params.sh` for new built-ins that are on by default.
- **Tests**:
  - `kiwami/tests/ci.sh .buildroot`.
  - `make retroarch-reconfigure-rgds-plus`. The checks:
    - `config.mk` has `HAVE_MCP = 0`, `HAVE_RETRONFS = 0`, `HAVE_RETROSMB = 0`, `HAVE_SMBCLIENT = 0`, `HAVE_RRAR = 0`,
      `HAVE_GAMEMODE = 0` and the ten decoders at `0`;
    - it still has `HAVE_RFLAC = 1`, `HAVE_RWAV = 1`, `HAVE_CHD = 1`, `HAVE_7ZIP = 1`, `HAVE_STDIN_CMD = 1` and
      `HAVE_KEYCHAIN = 0`;
    - the build folder has no `network/mcp_server.o`;
    - the binary's size is compared with step 3's and the baseline's, and `NEEDED` is unchanged.
  - Then the whole image, `make rgds-plus`:
    - `rootfs.squashfs`'s size is compared with the baseline's;
    - `/usr/share/kiwami/ra-hotkeys` is in the target;
    - the update package is copied as `output/master-71f521e/banana-rgds-plus-0.4.2-ramaster.update.tar`, ready for
      the device tests (M4-M16).
  - `make test`.
- **Commit**: `RetroArch master: built without the MCP server, network clients, media decoders, RAR and GameMode`

### 6. A guard against upstream's RetroArch moving under the pin

- **Files**: `kiwami/tests/retroarch-pin.sh` (changed), `docs/FORK.md` (changed).
- **Change**:
  - **`retroarch-pin.sh`**, first, before any Buildroot call:
    ```sh
    # Upstream's RetroArch as it was when the fork's pin was last checked against it. When upstream moves one of these,
    # the fork's pin hides it: look at what upstream changed, then rebase or drop kiwami/patches/retroarch/, move the
    # pin (kiwami/local.mk) or turn it off, and update the stamp here.
    UP_VERSION=v1.22.2
    UP_JOYPADS=f3b62b847ed21939b668b897b18bbc1defbaae05
    UP_PATCHES=<sha256 of the three patches, computed in this step>
    ```
    The checks run against `buildroot/package/retroarch/retroarch.mk`:
    - `RETROARCH_VERSION` and `RETROARCH_JOYPADS_VERSION`, read with `sed`, must equal the stamps;
    - the patches' sum must equal `UP_PATCHES`. It's
      `(cd buildroot && { find patches/retroarch -type f; find package/retroarch -name '*.patch'; } | LC_ALL=C sort |
      xargs sha256sum | sha256sum | cut -c1-64)`, computed now: upstream's patches haven't changed since the first
      plan (`git diff 4101661 HEAD -- buildroot/patches/retroarch` is empty), so it should be the first plan's
      `0a6ab447...`, and the step checks that;
    - each failure says what moved and exits 1, for example `retroarch-pin: upstream's RETROARCH_VERSION is v1.23.0,
      the pin was checked against v1.22.2: ...`.
  - **`docs/FORK.md`**, *Taking in upstream* step 3: `buildroot/package/retroarch/retroarch.mk` and
    `buildroot/patches/retroarch/` join the `git diff $OLD main` list. A sentence says why: the fork's pin shadows
    upstream's RetroArch version and patches, `kiwami/tests/ci.sh` stops on a change, and what to do is in the stamps'
    comment.
- **Tests**:
  - `kiwami/tests/ci.sh .buildroot` passes.
  - `retroarch-pin.sh --self-test`, run by `ci.sh`, checks the guard on copies. It copies `buildroot/package/retroarch/`
    and `buildroot/patches/retroarch/` into a `mktemp -d`, which the guard reads through `KIWAMI_TREE` (`.` by
    default). The unchanged copy passes, and each of these four changes fails with its own message:
    - `RETROARCH_VERSION = v1.23.0`;
    - a changed joypads commit;
    - a new `0004-x.patch` in `patches/retroarch/`;
    - an `x.patch` in `package/retroarch/`.
  - `make test`.
- **Commit**: `RetroArch master: CI stops when upstream's RetroArch moves under the fork's pin`

### 7. Final check

- `make test`, `make -C ui`, `make -C system/bananad CFLAGS="-O2 -Werror"`, `kiwami/tests/ci.sh .buildroot`.
- The fork's tests in `kiwami/tests/`, through `ci.sh`: `ra-keychain.sh`, `ra-hotkeys.sh`, `retroarch-pin.sh` (with its
  self-test), `core-overrides.sh`, `cores-ui.sh`, `tailscale-ui.sh`, `7z-ui.sh`, the helper and unit tests, post-build.
- The hooks: `git diff --stat main...HEAD` and `git grep -n 'kiwami:'` (the commands in `docs/FORK.md`) agree. The only
  new hooks are `ra-secrets`' `ra-keychain` line and `run-retroarch`'s `ra-hotkeys` line.
- `make test`'s renders in `ui/tests/out/` match `kiwami`'s from before the feature (A20). The branch's base `ui/` is
  exported with `git archive` and built under the scratchpad, and both suites run side by side so their clocks agree,
  as in 0006. This feature changes no banana-ui source, so they should be identical.
- `make retroarch-rgds` (one call): the `rgds` board builds the same RetroArch, patched, with its hotkeys file, since
  no RG DS is at hand to run it.
- `make rgds-plus` from scratch for RetroArch (`make retroarch-dirclean-rgds-plus` first). The binary's and
  `rootfs.squashfs`'s sizes are noted against the baseline's. `ls output/rgds-plus/target/usr/share/libretro/autoconfig/udev | wc -l`
  equals the baseline's count (A18).
- `kiwami/DEPENDENCIES.md` has the fork's RetroArch row (steps 3-5), and upstream's row is updated. `docs/FORK.md` is
  up to date (steps 2, 3, 4, 6). `docs/ARCHITECTURE.md` and `CLAUDE.md` don't name RetroArch's version, so they need
  nothing. Upstream's `README.md` ("RetroArch 1.22") is upstream's, and is left alone.
- The manual tests below all passed, or a deviation says what wasn't verified and the user accepted it. These are not
  verified from the start (*Refinements*): the headphone jack, an external pad, N64, PS1, the RG DS, and the
  dark-on-charger low-battery shutdown.
- Every acceptance criterion is met: each is listed by at least one automated or manual test, and those passed.
- **Commit**: `RetroArch master: the final check done`

## Automated tests

| Test | Where | What it checks | Criteria |
|---|---|---|---|
| The fold | `kiwami/tests/ra-keychain.sh` (run by `ci.sh`, step 2) | `ra-keychain` alone and through a hooked `ra-secrets` on a made-up card: the keychain's lines in `retroarch.cfg` once each, the keychain's value winning, the files gone, a log line with no value. Through the scrub: password and token in `accounts.conf` (same and another account), the username in `retroarch.cfg`. Special characters, a missing final newline, no `retroarch.cfg`, idempotent. With no keychain, byte-identical to upstream's script. The hook's place | A5, A6, A7 |
| The veto | `kiwami/tests/ci.sh`, post-build checks (step 2) | `*tailscaled.state` and `*retroarch-keychain.*` on upstream's veto line, once each; a second run changes nothing; the build stops without the line | A8 |
| The fork's options configured | `kiwami/tests/ci.sh` (`configure.sh` for `rgds`, `rgds_plus`, `universal`) | `BR2_PACKAGE_KIWAMI_RETROARCH_MASTER=y` and `BR2_PACKAGE_OVERRIDE_FILE` survive on every board | A1 |
| The pin | `kiwami/tests/retroarch-pin.sh` (run by `ci.sh`, steps 3-5) | On: version `71f521e...`, only the fork's hash file, the patch step's folders (`make -n`), the 17 configure options with FLAC kept, the hotkeys hook and its four names. Off: upstream's version, hash files, patch folder and options, no hotkeys hook. The output's own `local.mk` read both ways | A1, A2, A3, A9 |
| Master's hotkeys | `kiwami/tests/ra-hotkeys.sh` (run by `ci.sh`, step 4), with the stand-in `kiwami/tests/bin/retroarch` | `retroarch-hotkeys.sh`: passes on upstream's list plus the four and writes them; stops, naming it, on an extra hotkey of master's, a missing one, or one already in upstream's list. A rewritten `run-retroarch` in a made-up root with hotkeys Off: the four `nul`, stripped from the profiles' copy, a bound one kept and put back once. Without the file, byte-identical to upstream's; with hotkeys On, as upstream's. The hook's place | A9 |
| The guard | `kiwami/tests/retroarch-pin.sh` and its `--self-test` (step 6) | Upstream's version, joypads commit and patches' sum against the stamps; each of the four changes caught, with its own message | A4 |
| The build | steps 1, 3, 4, 5 and the final check, by hand | The baseline's sizes. The downloads hashed, the fork's patches applied cleanly, `config.mk`'s features, an aarch64 binary with the baseline's libraries and `71f521e` in it, `ra-hotkeys` installed, the binary's and the squashfs's sizes, the autoconfigs' count | A3, A9, A18, A20 |
| Upstream's own | `make test` | Passing; renders identical to `kiwami` before the feature; the frame bench unchanged | A20 |

## Manual tests

These are on the device, for what headless tests can't reach. Logs are fetched over SSH with `scp -O` (Settings > Network >
SSH; dropbear has no SFTP), over File sharing, or from the card on a computer (`banana/logs/` on the card is `/data/banana/logs/` on the device). Only the RG DS
Plus is at hand, so every test runs on it.

Two images, one card:
- **v1.22.2** is the baseline: `kiwami` at 0.4.2, built in step 1 (`banana-rgds-plus-0.4.2-v1222.update.tar`), and
  installed with Settings > System > Update before M1;
- **master** is the update built in step 5 (`banana-rgds-plus-0.4.2-ramaster.update.tar`), installed over the same card
  after M1-M3.

`measure.sh` (step 1) is copied to `/data/banana/measure.sh` and run over SSH as `sh /data/banana/measure.sh ...`. It
writes `/data/banana/logs/ra-*`, which is all there is to fetch (`ls /data/banana/logs/ra-*`), plus
`/data/banana/logs/banana-ui.log` and `/data/banana/logs/sleep.log` where a row says so.

Every measured run is done the same way:
- the game panel is on (Banana's default, so `retroarch.log` exists);
- RetroArch's statistics are on (Quick Menu > On-Screen Display > Notifications > Display Statistics);
- it covers 30 s of the same scene on both images;
- the lowest fps seen on the counter is noted, along with how the run went, by ear and by eye.

A run's `<name>` is `<image>-<game>-<core>`, e.g. `v122-cv-fceumm` or `master-cv-fceumm`. The games and their scenes:
- `cv`: Castlevania, stage 2;
- `smb`: Super Mario Bros, 1-1 running right;
- `ladx`: Link's Awakening DX, from Mabe Village into the overworld;
- `s3k`: Sonic 3 & Knuckles, Angel Island 1, running;
- `emerald`: Pokémon Emerald, a wild battle in tall grass;
- `smw`: Super Mario World, Yoshi's Island 2.

| # | Board | Steps | Expected | Logs to collect | Criteria | Outcome |
|---|---|---|---|---|---|---|
| M1 | rgds-plus, v1.22.2 | The baseline's speed. For each of the 10 game-core pairs, choose the core in Settings > Other systems, start the game, reach the scene, run `measure.sh run v122-<game>-<core>`, then quit and run `measure.sh log v122-<game>-<core>`. The pairs: `cv` and `smb` on FCEUmm and Mesen, `ladx` on Gambatte and SameBoy, `s3k` on PicoDrive and Genesis Plus GX, `emerald` on gpSP, `smw` on Snes9x. Then `cv` on FCEUmm once more, as `v122-cv-fceumm-2` (the spread). Then `emerald` and `smw` again with `measure.sh powersave` before `run` (`v122-<game>-ps`). | Every run recorded; Mesen and SameBoy threaded (0006's overrides, shown in the header) | `ra-v122-*.txt`, `ra-v122-*.log`; the lowest fps of each run, noted | A10 (the baseline) | passed, in part (24 Oct 2026, 0.4.2 at `82010f7`): the 10 pairs at 60 fps, plus mGBA. Not run: `v122-cv-fceumm-2`, `v122-emerald-ps`, `v122-smw-ps` (notes) |
| M2 | rgds-plus, v1.22.2 | The baseline of the rest, on `s3k` with PicoDrive unless said otherwise. Audio: Settings > Other systems > Audio latency 32, 64 and 128, a minute each on the speakers; at 64, `measure.sh reinit` mid-game. VSync off on `smw`: speed and tearing; then VSync back on. Fast-forward (the game panel's tile) on `cv` FCEUmm: the fps it reaches. On a game with achievements (`smb` FCEUmm), the game panel's achievements list as it shows. RetroArch's menu > Online Updater: the entries listed (a photo or a list). After a game-panel game, `measure.sh card v122`. | Recorded as v1.22.2 does it. The panel's list may show an error (oversight 5), and is recorded as it is | `ra-v122-lat32.log` ... `ra-v122-ff.log`, `ra-v122-panel.log`, `ra-reinit.txt`, `ra-v122-card.txt`; what was heard and seen, noted | A11, A14, A15, A18 (the baseline) | not run |
| M3 | rgds-plus, v1.22.2 | The baseline of 0.4.2's paths. **Hotkeys**: with RetroArch hotkeys On, in RetroArch's menu (Settings > Input > Hotkeys) bind Save State to Y (a hotkey, so with F held), then Configuration File > Save Current Configuration, and quit. Settings > Other systems > RetroArch hotkeys: Off. In `cv` FCEUmm, press F + L, F + R, F + Y, F + L2, F + R2, then F + X (menu) and F + Start (quit). During the game, `measure.sh hotkeys v122-off`; after it, `measure.sh card v122-off`. Hotkeys back On: F + L/R/Y/L2/R2 once each (F + Y now saves a state). **Dark on the charger**: on the charger, `s3k` PicoDrive with the game panel on; close the lid for 30 s and open it; play 20 s; press the power button, wait 30 s, press it again; play 20 s; quit, `measure.sh log v122-charger`. **Reset RetroArch**: after a game with the account, Settings > Other systems > Reset RetroArch; `measure.sh card v122-reset`; start `smb` FCEUmm. Finally, unbind Save State from Y again in RetroArch's menu. | Hotkeys Off: only F + X and F + Start do anything; the save-state line is `nul` during the game and back as Y after it, with `.banana-hotkeys.keep` gone. On: each works as before. Dark on the charger: the screens dark, the handheld awake, the game back with sound, no button stuck, the tiles answer. Reset: the backup has no password, token or keychain, and the next game logs in. All recorded as the baseline | `ra-v122-off-hotkeys.txt`, `ra-v122-off-card.txt`, `ra-v122-charger.log`, `ra-v122-reset-card.txt`, `sleep.log` (the `dark on the charger` lines), `banana-ui.log` | A9, A13, A17 (the baseline) | not run |
| M4 | rgds-plus, master | Install master's update. M1 again, with `master-` names. | Every run meets the bar against M1 (*Refinements*). A shortfall is a deviation | `ra-master-*.txt`, `ra-master-*.log` (each header says `(Git 71f521e)`); the lowest fps, noted | A10 | not run |
| M5 | rgds-plus, master | Mesen and SameBoy unthreaded. Over SSH, set `video_threaded = "false"` in `/data/retroarch/config/Mesen/Mesen.cfg` and in `SameBoy/SameBoy.cfg`. Run `cv` on Mesen and `ladx` on SameBoy, under `measure.sh powersave`, with `measure.sh run master-<game>-<core>-unthreaded`. Then delete both lines (the fork writes `"true"` again at the next start). | Recorded only (suggestion 3) | `ra-master-*-unthreaded.txt`; the lowest fps | A21 | not run |
| M6 | rgds-plus, master | M2's audio part: 32, 64 and 128 ms on `s3k` PicoDrive, a minute each; at 64, `measure.sh reinit`. | No crackle where M2 had none; the sound carries on after each reinit; no more underruns logged than on v1.22.2 (`grep -ci 'underrun\|xrun'`) | `ra-master-lat32.log` ... `ra-master-lat128.log`, `ra-reinit.txt` | A11 | not run |
| M7 | rgds-plus, master | Sleep mid-game, off the charger. `s3k` PicoDrive with the game panel on: close the lid for 10 s, open it, play 30 s, quit, then `measure.sh log master-sleep-panel`. Then Settings > Other systems > Second screen: Off, and the same with `cv` FCEUmm. Set Second screen back to Game panel. | Each sleeps and comes back with picture and sound; the panel's `retroarch.log` has no `[KMS] Display changed` | `ra-master-sleep-panel.log`, `sleep.log`, `banana-ui.log` | A12 | not run |
| M8 | rgds-plus, master | M3's dark on the charger, as `master-charger`. Then once more with Second screen Off, the lid only, on `cv` FCEUmm. | As M3, and the game comes back with sound and no crackle; no `[KMS] Display changed` in the panel's log | `ra-master-charger.log`, `sleep.log`, `banana-ui.log` | A13 | not run |
| M9 | rgds-plus, master | M2's VSync off on `smw`, and fast-forward on `cv` FCEUmm. | VSync off: normal speed, as on v1.22.2, with tearing noted; fast-forward reaches at least M2's fps | `ra-master-vsync.log`, `ra-master-ff.log`; the fps and what was seen, noted | A14 | not run |
| M10 | rgds-plus, master | The game panel, on `smb` FCEUmm: the save state, load state, menu (and back), fast-forward and quit tiles, each once. During the game, tap the panel away from the tiles a few times. Check the achievements list as in M2. | RetroArch is on the top panel, the tiles all work, the taps do nothing in the game or in RetroArch's menu, and the list is as in M2 | `ra-master-panel.log`, `banana-ui.log` | A15 | not run |
| M11 | rgds-plus, master | RetroAchievements with the setup's account, on `smb` FCEUmm: the login (the panel and RetroArch's notification), the game identified, an unlock (e.g. the first one the game offers). Quit, then `measure.sh card master-ra`. Then in RetroArch's own menu, Settings > Achievements: log out, and log in again with the same account (its password typed). Quit, then `measure.sh card master-ra-menu`. | Logged in over master's TLS client, identified, the unlock shown. After each game: no `retroarch-keychain.*`, no password or token in `retroarch.cfg`, the username there, and `ra_password` and `ra_retroarch_token` `<set>` in `accounts.conf` | `ra-master-ra.log`, `ra-master-ra-card.txt`, `ra-master-ra-menu.log`, `ra-master-ra-menu-card.txt`, `banana-ui.log` (`kiwami: ra-keychain: ...` after each game) | A6, A16 | not run |
| M12 | rgds-plus, master | File sharing: Settings > Network > File sharing on. Start `cv` FCEUmm, open RetroArch's menu > Configuration File > Save Current Configuration (the keychain file now exists). From the computer, list `\\BANANA\banana\retroarch\` and try opening `retroarch\retroarch-keychain.cfg` by its path. Quit the game, and list again. Then turn sharing off and on (`share up`'s scrub), and `measure.sh card master-share`. | Neither listing shows `retroarch-keychain.*`, and the open is refused; the veto line in `ksmbd.conf` has `*retroarch-keychain.*/`; no keychain file is left after `share up` | `ra-master-share-card.txt`; the listing taken during the game is from the computer, and is noted | A6, A8 | not run |
| M13 | rgds-plus, master | M3's hotkeys, as `master-off`, with one addition: in RetroArch's menu, also bind Video Filter (Toggle), one of master's four, to B before saving; with Off, F + B does nothing either. With Off, during the game, `measure.sh hotkeys master-off`. After the game, `measure.sh card master-off`. Then unbind both in RetroArch's menu. | As M3. Also: `input_video_filter_toggle_btn` is `nul` in the Off config, no profile in `/run/banana/ra-autoconfig` has a line for it, and after the game it's back as B, once | `ra-master-off-hotkeys.txt`, `ra-master-off-card.txt`, `banana-ui.log` | A9 | not run |
| M14 | rgds-plus, master | Reset RetroArch with a keychain file. With the account, start `cv` FCEUmm, and use Save Current Configuration in RetroArch's menu. Over SSH, `cp /data/retroarch/retroarch-keychain.cfg /run/kc` (`/run` is not shared and is gone at a reboot). Quit. Then `cp /run/kc /data/retroarch/retroarch-keychain.cfg; rm /run/kc`, then Settings > Other systems > Reset RetroArch. `measure.sh card master-reset`. Start `smb` FCEUmm: the login. | The backup folder has a `retroarch.cfg` with no password or token, and no `retroarch-keychain.*` anywhere on the card; `accounts.conf` has the token; the next game logs in. As M3 | `ra-master-reset-card.txt`, `banana-ui.log` (the fold's line at the reset) | A6, A17 | not run |
| M15 | rgds-plus, master | The rest. A `.7z` NES game and a `.7z` GBA game start (0003's). Quick resume on: quit `smb` mid-level and start it again; then Quick resume off. Shader: LCD, a game, then Off. RetroArch's menu > Online Updater: compare the entries with M2's. A fresh `retroarch.cfg`: over SSH `mv /data/retroarch/retroarch.cfg /data/retroarch/retroarch.cfg.kept`, start `cv`, quit, and `mv` it back. After a game-panel game, `measure.sh card master-misc`. | Each works as on v1.22.2; the updater lists M2's entries; the game with a fresh config starts with Banana's settings; `ra-master-misc-card.txt` has no `log_to_file = "true"` (oversight 10) | `banana-ui.log`, `ra-master-misc.log` (`measure.sh log` after the shader game), `ra-master-misc-card.txt` | A18 | not run |
| M16 | rgds-plus, back to v1.22.2 | Install the v1.22.2 update (`-v1222`) again over this card. On `smb` FCEUmm: the login and the game identified. `measure.sh card v122-back`. | The game starts and logs in; the card is as A6 says | `ra-v122-back.log`, `ra-v122-back-card.txt` | A19 | not run |

Notes from the runs (what the logs showed, what was fixed):

- **The baseline image** (24 Oct 2026). `make rgds-plus` from scratch (`output/rgds-plus/` deleted first; the old
  output was a 0.3.0 build from 17 Oct) at `82010f7`, step 1's commit, which is `kiwami`'s `848af54` plus
  `.workspace/`. It took 54 minutes. `VERSION` is 0.4.2, and RetroArch is `v1.22.2` with upstream's 0001, 0002 and
  0003 applied (`.applied_patches_list`). The update package is copied as
  `output/baseline-0.4.2/banana-rgds-plus-0.4.2-v1222.update.tar` (186,275,840 bytes, sha256 `139855c0...`).
  - `usr/bin/retroarch`: 15,008,688 bytes (`du -b`), aarch64, stripped. `strings` has `1.22.2` and no commit.
  - `rootfs.squashfs`: 175,738,880 bytes.
  - `NEEDED`: `liblzma.so.5 libasound.so.2 libfreetype.so.6 libudev.so.1 libgbm.so.1 libdrm.so.2 libGLESv2.so.2
    libEGL.so.1 libstdc++.so.6 libm.so.6 libmvec.so.1 libgcc_s.so.1 libc.so.6` (13).
  - Joypad autoconfig files (`usr/share/libretro/autoconfig/udev`): 91.
- **M1** (24 Oct 2026, the baseline image above, on the RG DS Plus). The logs are in `logs/` (`ra-v122-*.txt`,
  `ra-v122-*.log`). Every run was at its system's profile: `schedutil` up to 1416 MHz for NES, Game Boy Color and
  Mega Drive, up to 1992 MHz for GBA (`balanced`), and `performance` at 1992 MHz for SNES. Mesen's and SameBoy's
  overrides had `video_threaded` true, and their logs say `Starting threaded video driver` (cv-mesen: 3 of 6191
  frames dropped). The RetroArch logs have only the usual lines on every run (`[MIDI] snd_seq_open failed`, `[DRM]
  Couldn't get device resources`), and no underrun.

  | Run | Lowest fps | CPU share, average (lowest-highest second) | VmHWM (kB) |
  |---|---|---|---|
  | cv-fceumm | 60 | 53.1 (51.9-54.3) | 40,324 |
  | cv-mesen | 60 | 90.0 (86.2-92.1) | 45,716 |
  | smb-fceumm | 60 | 52.9 (47.5-56.8) | 40,288 |
  | smb-mesen | 60 | 85.4 (83.3-88.2) | 45,088 |
  | ladx-gambatte | 60 | 42.7 (40.1-45.0) | 41,012 |
  | ladx-sameboy | 60 | 77.0 (74.5-79.2) | 37,696 |
  | s3k-picodrive | 60 | 52.0 (48.5-56.3) | 45,284 |
  | s3k-gpgx | 60 | 54.4 (49.5-59.8) | 58,712 |
  | emerald-gpsp | 60 | 42.4 (37.2-48.0) | 54,736 |
  | emerald-mgba (not in the set) | 60 | 55.2 (51.9-59.8) | 103,668 |
  | smw-snes9x | 60 | 34.9 (26.4-39.2) | 53,824 |

  - Not run: the second Castlevania on FCEUmm (the spread), so the CPU share's bar takes its floor of 2 points, and
    the two `powersave` runs of GBA and SNES (information only).
  - The mGBA run (Settings > Other systems' other GBA core) is an extra. M4 can repeat it, as information.
- **Step 3's build** (24 Oct 2026, `make retroarch-rgds-plus` over the baseline's output, reconfigured by the
  fragment). Buildroot downloaded master itself, and both tarballs were checked against the fork's hash file
  (`retroarch-71f521e....tar.gz: OK`, `f3b62b8....tar.gz: OK`). 0002 applied at an offset of 102 lines and 0003
  exactly, both from `kiwami/patches/retroarch/` (`.applied_patches_list`). `config.mk` has `HAVE_KEYCHAIN = 0`,
  `HAVE_7ZIP = 1`, `HAVE_RETROSSL = 1`, `HAVE_STDIN_CMD = 1`, `HAVE_CHD = 1`, `HAVE_RFLAC = 1` and `HAVE_RZSTD = 1`.
  The only compiler warning is master's own (`command.c:3094`, `-Wformat-truncation`).
  - `usr/bin/retroarch`: 17,971,528 bytes, aarch64, 2,962,840 bytes (19.7%) over the baseline's; step 5 removes
    built-ins. `strings` has `71f521e`.
  - `NEEDED`: `libasound.so.2 libfreetype.so.6 libudev.so.1 libgbm.so.1 libdrm.so.2 libGLESv2.so.2 libEGL.so.1
    libz.so.1 libstdc++.so.6 libm.so.6 libmvec.so.1 libgcc_s.so.1 libc.so.6` (13): the baseline's without
    `liblzma.so.5` and with `libz.so.1`, both already in the image (Deviations).

## Deviations

Where the implementation turned out different from this plan, why, and what changed (dated).

- **24 Oct 2026, step 2: the fold tells the keychain file apart by its name.** The planned awk took the keychain's
  lines while `FNR == NR`. When `retroarch-keychain.cfg` is empty (0 bytes), that also holds for every line of
  `retroarch.cfg`, which would then be read as the keychain. Its lines of another shape would be dropped, and only the
  last of a repeated key kept. The awk compares `FILENAME` with `ARGV[1]` (the keychain's path, which `awk -v` would
  have put through escape processing) instead. The step's code above is updated.
- **24 Oct 2026, step 2: two more cases in `kiwami/tests/ra-keychain.sh`.**
  - An empty keychain file, with a `retroarch.cfg` that has a key twice and a comment line. It fails on the
    `FNR == NR` version.
  - A failed fold (the folder made read-only): both files stay as they were, no temporary is left, no "folded" line
    is logged, and the scrub carries on. That's "a failure leaves both files as they were", which the plan settled
    but didn't test. It's skipped when run as root, since root writes anyway.
- **24 Oct 2026, step 3: master's `NEEDED` isn't a subset of v1.22.2's.** Master links the system zlib
  (`libz.so.1`) and no longer `liblzma.so.5`. It dropped v1.22.2's bundled zlib (`HAVE_BUILTINZLIB`) and bundled
  Zstandard for `-lz` and libretro-common's own decoders (`HAVE_RZSTD`, which also reads CHD's Zstandard hunks), and
  its xdelta softpatching became a self-contained VCDIFF decoder. Both libraries are in the baseline image anyway
  (`libz.so.1` is linked by banana-ui and freetype), so the image ships nothing new. A3's and step 3's check
  becomes "every `NEEDED` library is one the baseline image already ships", and step 5 compares with step 3's set.

## Progress

A step is marked done in its own commit, so the commit column holds the subject (a commit can't carry its own hash).

| Step | Status | Commit |
|---|---|---|
| 1. The measurements' script | done | RetroArch master: a script for the device measurements, the same on both versions |
| 2. Master's credentials file folded back into retroarch.cfg | done | RetroArch master: ra-secrets folds master's credentials file back into retroarch.cfg |
| 3. RetroArch's master, pinned behind a build option | done | RetroArch master: master at 71f521e behind a build option, with the fork's patches for it |
| 4. RetroArch hotkeys Off covers master's four new hotkeys | todo | |
| 5. Master without the built-ins Banana can't use | todo | |
| 6. A guard against upstream's RetroArch moving under the pin | todo | |
| 7. Final check | todo | |
