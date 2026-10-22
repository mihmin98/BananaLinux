# 0007 RetroArch master: plan

Issue: #7 · Branch: `k/0007-retroarch-master` · Investigation: [investigation.md](investigation.md) · Status: approved

## Summary

The fork's image runs RetroArch's master at `a81e450` (22 Oct 2026) instead of upstream's v1.22.2, but only if, measured
on the RG DS Plus against v1.22.2, nothing gets worse. It's a build option, `BR2_PACKAGE_KIWAMI_RETROARCH_MASTER` in
`kiwami/Config.in`. Its default (off) is upstream's v1.22.2, and `kiwami/configs/kiwami.fragment` turns it on. The image
holds one RetroArch.

The approach chosen in the investigation (option A) has five parts:

- **The pin.** Buildroot's package override file, `BR2_PACKAGE_OVERRIDE_FILE`, is set by the fragment to the fork's
  `kiwami/local.mk`. That file is read before any package, and with the option on it holds
  `override RETROARCH_VERSION = a81e450...`.
- **The fork's patches and hash.** `kiwami/external.mk` limits RetroArch's patch step to `kiwami/patches/`, which
  holds upstream's 0002 as it is and 0003 rebased (0001 is no longer needed). It also points the download at the
  fork's hash file.
- **Configure options.** RetroArch is built with `--disable-keychain`, and without master's new built-ins that Banana
  can't use.
- **The credentials.** A fork script folds master's `retroarch-keychain.cfg` back into `retroarch.cfg`. It runs from
  one `kiwami:` line at the start of upstream's `ra-secrets scrub`. File sharing vetoes the keychain files.
- **A guard.** CI fails when upstream changes its RetroArch package or patches under the fork's pin.

No banana-ui change, no settings row, nothing in `ui/rnds/`, `ui/assets/` or the ROCKNIX-synced files.

### Refinements over the investigation

- **The bar is measured at each system's own profile** (the user's choice). `systems.conf` runs GBA under `balanced`
  and SNES under `performance`, not `powersave`, so the investigation's "fps under `powersave` for each game" applies
  to NES, Game Boy Color and Mega Drive only. GBA and SNES are also measured under `powersave`, as information.
- **The measurement set** (the user's choice) is 10 runs: NES Castlevania and Super Mario Bros on FCEUmm and Mesen,
  Game Boy Color Link's Awakening DX on Gambatte and SameBoy, Mega Drive Sonic 3 & Knuckles on PicoDrive and Genesis
  Plus GX, GBA Pokémon Emerald on gpSP, SNES Super Mario World on Snes9x. **N64 and PS1 are left out**, so they're not
  verified on master.
- **"No regression", in numbers** (proposed here, to approve with the plan). In every run, master's lowest fps is at
  least v1.22.2's. Its average CPU share is at most v1.22.2's plus the run-to-run spread (Castlevania on FCEUmm is
  measured twice on v1.22.2 for that spread, counted as at least 2 points). Its peak memory (`VmHWM`) is at most
  110% of v1.22.2's.
- **FLAC stays.** Master's CHD reader decodes CD-audio hunks with `rflac` (`Makefile.common:2281-2287`). With
  `--disable-rflac`, RetroArch's own CHD reading (RetroAchievements' hashing of a CHD game) could fail on images with
  FLAC hunks. So "media decoders" off means the other ten (AV1, VP9, MP4, WebM, MPEG-1, AAC, Opus, Vorbis, MP3, MOD).
  WAV, the menu sounds' format, stays as in v1.22.2.
- **No version subfolder for the patches.** The investigation had `kiwami/patches/retroarch/<sha>/`. Buildroot uses a
  version folder when one exists and the parent folder otherwise (`pkg-utils.mk:166-170`). So a bump that forgot to
  rename the folder would silently build with *no* patches. In `kiwami/patches/retroarch/` they apply to whatever
  version is pinned, and a bump they don't fit fails the build.
- **The patch folder is `private` to RetroArch's patch step.** A plain target-specific `BR2_GLOBAL_PATCH_DIR` would
  also reach every package built as a prerequisite of that step, taking upstream's patches away from them. `private`
  stops that. Tried on a scratch config with `make -n` on the patch stamp. Two folders were applied: the package's own
  (`buildroot/package/retroarch/`, which holds no patches) and the fork's. Upstream's `buildroot/patches/retroarch/`
  was not applied.
- **Master's version is stamped.** Master's `version.all` still says `1.22.2`, and its tarball has no `.git`, so the
  binary's version string is the same as v1.22.2's. `GIT_VERSION` is passed to its make (an exported `private` variable
  on the build step). `retroarch --version` and `retroarch.log` then say `(Git a81e450)`, which is how each device log
  shows which RetroArch ran.
- **The guard also watches the joypads' pin.** The fork's hash file has to list upstream's joypad tarball
  (`RETROARCH_JOYPADS_VERSION`), so a bump of it upstream would break the fork's download. The guard flags it in CI
  first. Its stamps live in the guard's script, next to the message that says what to do.
- **The keychain file always wins the fold.** RetroArch writes the keychain file and strips `retroarch.cfg` in the
  same save, so the keychain's values are the latest. A key in both files ends with the keychain's line only. Every
  `key = "value"` line is folded, the empty ones too: without sealing, master writes all of its sensitive keys
  (`netplay_password = ""` ...), which v1.22.2 kept in `retroarch.cfg` anyway.
- **Three things in the bar can't be tested on the device**, by what's at hand: the headphone jack and an external
  pad (neither available), and N64 and PS1 (left out). The jack gets a stand-in: `AUDIO_REINIT` written to RetroArch's
  FIFO (`/run/banana/retroarch.cmd`) by hand, the command bananad writes on a jack change (`bananad.c:588`). For the
  pad, the image keeps the same autoconfig files.
- **A script for the device measurements**, `.workspace/0007-retroarch-master/measure.sh` (step 1). It makes the
  baseline and master's runs the same, and keeps secrets out of what is collected.

## Acceptance criteria

- **A1** (the option): `BR2_PACKAGE_KIWAMI_RETROARCH_MASTER` is in Kiwami's menu, depends on `BR2_PACKAGE_RETROARCH`
  and defaults to off. The fragment sets it on, with `BR2_PACKAGE_OVERRIDE_FILE="$(BR2_EXTERNAL_KIWAMI_PATH)/local.mk"`.
  Both survive Buildroot's resolution on `rgds`, `rgds_plus` and `universal`. With it on, each configured board resolves
  RetroArch as follows:
  - `RETROARCH_VERSION` is `a81e450c00745b27ba417d486799b12f09c38ea4`;
  - the download is checked against `kiwami/patches/retroarch/retroarch.hash` alone;
  - the patch step applies `buildroot/package/retroarch/` and `kiwami/patches/retroarch/`, never
    `buildroot/patches/retroarch/`;
  - `RETROARCH_CONF_OPTS` ends with the fork's options (A3).
- **A2** (off is upstream's): with the option off, RetroArch resolves as upstream's: `v1.22.2`, Buildroot's default
  hash files, `buildroot/patches/retroarch/` applied, upstream's `RETROARCH_CONF_OPTS` unchanged. Either way, an
  override file the user keeps in the output folder (`$(CONFIG_DIR)/local.mk`, Buildroot's default) is still read.
- **A3** (the build): `make retroarch-rgds-plus` does the following:
  - it checks both tarballs against the fork's hash file and applies `0002-...` and `0003-...` from
    `kiwami/patches/retroarch/` with no fuzz;
  - it configures with `HAVE_KEYCHAIN = 0`, and with the MCP server, the NFS and SMB clients, the RAR reader, GameMode
    and the ten decoders off;
  - it keeps 7z, CHD with FLAC, RetroArch's own TLS client and stdin commands (`HAVE_STDIN_CMD`);
  - it builds an aarch64 `retroarch` whose `NEEDED` libraries are v1.22.2's (or fewer) and whose `strings` hold
    `a81e450`.
  Its size is recorded against v1.22.2's 15,008,688 bytes.
- **A4** (the guard): `kiwami/tests/ci.sh` fails when any of these differs from what the fork last checked:
  - upstream's `RETROARCH_VERSION` (`v1.22.2`);
  - its `RETROARCH_JOYPADS_VERSION` (`f3b62b8...`);
  - the files of `buildroot/patches/retroarch/` plus any `*.patch` in `buildroot/package/retroarch/` (a sha256).
  Its message names what changed and what to look at.
- **A5** (the fold): given `/data/retroarch/retroarch-keychain.cfg`, `ra-secrets scrub` first moves its
  `key = "value"` lines into `retroarch.cfg`, each key once with the keychain's value. It then deletes the file and any
  `retroarch-keychain.key`, and logs one line that names no value. Values with `& | \` are carried byte for byte. A
  last line with no newline isn't glued to. With no `retroarch.cfg`, nothing is done, and the keychain stays (vetoed,
  A8).
- **A6** (the credentials' end state): after a scrub that found the keychain, the account's password and token are in
  `banana/accounts.conf` (the last login wins, as upstream's scrub decides). The username is in `retroarch.cfg`. No
  password or token is in `retroarch.cfg`, and no `retroarch-keychain.*` is left on the card.
- **A7** (no keychain is upstream's): with no keychain file, `ra-secrets scrub` leaves `retroarch.cfg` and
  `accounts.conf` byte-identical to what upstream's script (without the hook) leaves, and the fold logs nothing.
- **A8** (file sharing): the image's share script vetoes `*retroarch-keychain.*` next to `*tailscaled.state`, each
  once. A second post-build run changes nothing, and the build stops if the veto line is gone or changes shape. On the
  device, over SMB, neither file is listed or opened.
- **A9** (speed): in each of the 10 runs at the system's profile, master meets the bar (*Refinements*: fps, CPU share,
  `VmHWM`) against v1.22.2 on the same card and scene. GBA and SNES under `powersave` are recorded as information.
- **A10** (audio): at 32, 64 and 128 ms audio latency, master plays without crackle wherever v1.22.2 did, and with no
  more underruns logged. After `AUDIO_REINIT` written to the FIFO three times mid-game, the sound carries on (the
  headphone jack's stand-in; the jack itself isn't verified).
- **A11** (sleep): closing the lid mid-game for 10 s and opening it, with the game panel on and with Second screen off,
  brings the game back with picture and sound. With the game panel on, `retroarch.log` has no `[KMS] Display changed`.
- **A12** (VSync off, fast-forward): with VSync off the game runs at its normal speed, as on v1.22.2, and tearing is
  noted. Fast-forward (patch 0002) runs at least as fast as on v1.22.2.
- **A13** (the game panel): RetroArch draws on the leased panel. The five tiles (save state, load state, menu,
  fast-forward, quit) work. A touch on the panel doesn't reach RetroArch. The achievements list behaves as on v1.22.2
  (investigation, oversight 5).
- **A14** (RetroAchievements): with the setup's account, the following holds over master's own TLS client:
  - the game logs in and is identified, and an achievement unlocks;
  - afterwards the card is as A6 says;
  - a login made in RetroArch's own menu ends up in `accounts.conf`.
- **A15** (the rest of what Banana uses): on master the following work:
  - a `.7z` game starts (0003);
  - Quick resume resumes;
  - Settings > Other systems > Shader applies its preset;
  - the Online Updater lists the same entries as on v1.22.2;
  - a game starts with a fresh `retroarch.cfg` (the image's template);
  - the image holds the same joypad autoconfig files as v1.22.2's.
- **A16** (back to v1.22.2): installing the v1.22.2 update over a card master has used, a game starts and
  RetroAchievements logs in.
- **A17** (footprint, upstream's look): the rootfs's size is recorded against the 172,720,128-byte baseline, and
  nothing new runs while idle. `make test`'s renders are identical to `kiwami`'s from before the feature. The only new
  `kiwami:` hook is `ra-secrets`'.
- **A18** (information, suggestion 3): Mesen (Castlevania) and SameBoy (Link's Awakening DX) on master without
  threaded video, under `powersave`: fps and CPU share recorded. Dropping their overrides would be a later change.

## Risks

- **Performance and footprint.**
  - Frame time on master could go either way. Only the device shows it: M1 and M3, against the bar.
  - The binary: v1.22.2's is 15,008,688 bytes. Master's is measured after step 2 and after step 3, which should
    shrink it.
  - Memory: `VmHWM` in every run.
  - Idle: nothing new runs. RetroArch only runs during a game, and the fold only during a scrub (before and after a
    game, at share up).
  - Build time: one RetroArch build, as now. Switching the option changes `RETROARCH_DIR`, so the first build after a
    switch builds RetroArch from scratch.
- **Conflicts with upstream.**
  - Merges: one hook line in `ra-secrets` (1 commit upstream since 18 Oct); a conflict there is resolved by putting
    the line back at the start of `scrub)`.
  - Changes the merge doesn't flag: a version or patch change in upstream's RetroArch merges silently, and the guard
    (A4) fails CI on it. `share`'s veto line is edited at build time, and post-build stops on a change of shape (A8).
  - Overlap: `kiwami/local.mk` takes the package override file over. If upstream sets `BR2_PACKAGE_OVERRIDE_FILE`
    one day, the fragment replaces it. `configure.sh` doesn't catch that, but the merge's defconfig diff (in
    `docs/FORK.md`'s list) does.
- **The override mechanism.** `override` in the override file is a less common use of it (Buildroot documents it for
  `<pkg>_OVERRIDE_SRCDIR`). A Buildroot bump could change when the file is read, and `retroarch-pin.sh` resolves the
  version on every board, in CI, each time (A1, A2).
- **The patch-dir trick** relies on Buildroot taking `BR2_GLOBAL_PATCH_DIR` when the patch recipe runs
  (`pkg-generic.mk:239-241`). `retroarch-pin.sh` checks the patch step's folders with `make -n` in CI, so a Buildroot
  change that breaks it fails there, not on the device.
- **Master is master.** It may carry bugs that a release would fix. The bar means a regression isn't shipped: a
  regression gets fixed as a deviation, or the option is set off in the fragment, which puts the image back to
  v1.22.2 with no other change.
- **The credentials.**
  - The secrets stay on the card in `retroarch.cfg` for as long as the after-game scrub takes, as on v1.22.2.
  - The keychain file lives from RetroArch's exit (or a "Save configuration" in its menu) until the next scrub.
    File sharing vetoes it (A8). A card in a computer still shows it, as it shows `accounts.conf`.
  - The fold's tests (A5-A7) cover the other parts.
- **The rebased 0003** could leave an Online Updater entry that can't work, or hide one that can. M11 compares the
  entries with v1.22.2's.
- **The disabled built-ins.** A configure flag that doesn't exist makes master's `configure` stop ("Unknown option"),
  so a bump that drops one fails the build loudly. A built-in that turns out to be needed is found by the device tests.
  FLAC is kept for CHD.
- **Hardware and boards.**
  - Master's KMS context on the lease, its hotplug monitor after resume (M6), its async flips with VSync off (M7), and
    its ALSA driver after SIGSTOP/SIGCONT and `AUDIO_REINIT` (M5, M6).
  - Not verified on the device: the headphone jack, an external pad, N64, PS1, and the RG DS. The RG DS shares the
    SoC, the lease and ALSA path; it gets the build check only.
- **TLS.** Master's own TLS client could fail the login (M9). The decided fallback is `--enable-mbedtls` against
  Buildroot's `mbedtls`, done as a deviation with its `DEPENDENCIES.md` line.

## Steps

Each step is one commit, and leaves the tree building and `make test` passing.

### 1. The measurements' script

- **Files**: `.workspace/0007-retroarch-master/measure.sh` (new).
- **Change**: a POSIX `sh` script, copied to the card (`/data/banana/measure.sh`) and run over SSH as
  `sh /data/banana/measure.sh <verb> ...`. It's the same on both images. It writes everything to
  `/data/banana/logs/ra-<name>.*`, and never a secret's value:
  - `run <name> [seconds]` (default 30), during a game. A header first: the date, `/etc/banana/version`,
    `retroarch --version` (master says `(Git a81e450)`), the CPU governor and `scaling_max_freq`, and
    `video_threaded` from `retroarch.cfg` and from the Mesen and SameBoy override files. Then one line per second: the
    CPU clock (`policy0/scaling_cur_freq`), the GPU clock (`/sys/class/devfreq/*.gpu/cur_freq`), and RetroArch's CPU
    share over that second (the change in `/proc/<pid>/stat`'s utime and stime, at `getconf CLK_TCK`). Then the
    average share, `VmHWM` and `VmRSS`. Into `ra-<name>.txt`. It stops with a message if no `retroarch` is running.
  - `powersave`: runs `banana-perf powersave` for the running game. That's for the information runs, and the
    system's own profile comes back with the next game.
  - `reinit`: writes `AUDIO_REINIT` to `/run/banana/retroarch.cmd`, the FIFO run-retroarch and banana-ui give
    RetroArch in both panel modes, three times 5 s apart. A line goes to `ra-reinit.txt` each time.
  - `log <name>`, after a game: copies `/run/banana/retroarch.log` (game panel on) to `ra-<name>.log`, with
    `cheevos_password`, `cheevos_token` and the `t=`/`p=` parameters of any URL masked by `sed`.
  - `card <name>`, after a game: writes `ra-<name>-card.txt` with the following:
    - `ls -la /data/retroarch/`;
    - for each `cheevos_*` key in `retroarch.cfg` and each `ra_*` key in `accounts.conf`, the key with `<set>`,
      `<empty>` or nothing in place of its value, and how many lines it has;
    - whether `retroarch-keychain.cfg` and `.key` exist;
    - the running share config's `veto files` line (`/run/banana/ksmbd.conf`, when sharing is on).
  The script has no `kiwami:` code and isn't part of the image.
- **Tests**: `sh -n` on it. On the host, `run` against a running `sleep 60` named by `RA_PID` (the script's own
  test hook) prints a header and 3 lines for `run x 3`; `card` on a made-up folder (`RA_DATA`) prints `<set>` for a
  set password, `<empty>` for an empty one and no value. `make test` (unchanged). M1, the baseline, runs with it on
  the v1.22.2 image (kiwami's current) before step 2's image is installed. Its results go into the plan as they come
  back.
- **Commit**: `RetroArch master: a script for the device measurements, the same on both versions`

### 2. Master's credentials file folded back into retroarch.cfg

- **Files**: `kiwami/rootfs/usr/libexec/kiwami/ra-keychain` (new, mode 755),
  `system/rootfs/usr/libexec/banana/ra-secrets` (hook), `kiwami/post-build.sh` (changed), `kiwami/tests/ra-keychain.sh`
  (new, mode 755), `kiwami/tests/ci.sh` (changed), `docs/FORK.md` (changed).
- **Change**:
  - The hook: one line in `ra-secrets`, the first in `scrub)` (before `[ -f $CFG ] || exit 0`, `:15`):
    ```sh
        [ -x /usr/libexec/kiwami/ra-keychain ] && /usr/libexec/kiwami/ra-keychain $CFG	# kiwami: RetroArch master's credentials file back into retroarch.cfg
    ```
    It's the one place all three callers go through: `run-retroarch:94` and `:134`, and `share:9`. On v1.22.2 the
    script finds no keychain file and does nothing, so this step changes nothing until step 3.
  - `ra-keychain <retroarch.cfg>`, in the shape of `core-overrides`:
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
    awk 'FNR == NR { if (/^[A-Za-z0-9_]+ = ".*"$/) { if (!($1 in v)) o[++n] = $1; v[$1] = $0 } next }
         !($2 == "=" && $1 in v) { print }
         END { for (i = 1; i <= n; i++) print v[o[i]] }' "$K" "$CFG" > "$CFG.kiwami" &&
        mv "$CFG.kiwami" "$CFG" && rm -f "$K" || { rm -f "$CFG.kiwami"; exit 0; }
    echo "kiwami: ra-keychain: retroarch-keychain.cfg folded into retroarch.cfg" >&2
    ```
    The exact shape is settled in this step. Settled already:
    - A last `retroarch.cfg` line with no newline gets one first, as `core-overrides` does.
    - A failure leaves both files as they were and the scrub carries on.
    - The log line names no key or value, and goes to banana-ui's log from run-retroarch.
    - A key file without a keychain file is removed too: it's a secret, and only a sealed build makes one.
  - `kiwami/post-build.sh`: the three Tailscale lines become a `veto <name> <regex>` function, called twice:
    `veto '*tailscaled.state' '\*tailscaled\.state'` and `veto '*retroarch-keychain.*' '\*retroarch-keychain\.\*'`.
    Each name is appended to upstream's line when the line doesn't already have it, then checked anywhere on the line
    (`^	veto files = /.*/<regex>/`) rather than at its end, since two names now follow upstream's. The single-line check
    before them stays as it is. The comment says what the keychain files are.
  - `kiwami/tests/ra-keychain.sh`, against a made-up card under a `mktemp -d`. It runs two copies of `ra-secrets`
    whose `/data/` is rewritten to the made-up one by `sed`: the hooked one, with the hook's
    `/usr/libexec/kiwami/` pointing at `kiwami/rootfs/usr/libexec/kiwami/`, and upstream's, without the hook line.
    The cases:
    - **no keychain**, a `retroarch.cfg` with `cheevos_username = "alice"` and `cheevos_password = "pw1"`, and an
      `accounts.conf`: the hooked and the upstream copy leave byte-identical `retroarch.cfg` and `accounts.conf`, and
      `ra-keychain` logs nothing (A7). `ra-keychain` alone on a `retroarch.cfg` with no secrets leaves it
      byte-identical;
    - **folded**: `retroarch.cfg` as master leaves it (no `cheevos_username`), and a keychain file with
      `cheevos_username = "alice"`, `cheevos_password = "pw1"`, `cheevos_token = "tok1"`, `netplay_password = ""`
      and `kiosk_mode_password = ""`. After `ra-keychain` alone, `retroarch.cfg` has the five lines once each after
      its own lines, kept in order. The keychain and a `retroarch-keychain.key` are gone, and the one log line is
      there, without `pw1` or `tok1` (A5);
    - **through the scrub**, the same files: `accounts.conf` gets `ra_user=alice`, `ra_password=pw1` and
      `ra_retroarch_token=tok1`. `retroarch.cfg` has `cheevos_username = "alice"` and no password or token line. No
      `retroarch-keychain.*` is left (A6);
    - **another account** in the keychain (`bob`, `pw2`, `tok2`) with `alice` in `accounts.conf`: `ra_user=bob`,
      `ra_password=pw2`, `ra_retroarch_token=tok2` (the last login wins) (A6);
    - **the keychain wins**: `retroarch.cfg` has `cheevos_username = "alice"` twice, and the keychain has `bob`: one
      `cheevos_username = "bob"` line is left (A5);
    - **special characters**: a password `p&w|d\x` is carried byte for byte into `accounts.conf` (A5);
    - **no final newline**: `retroarch.cfg`'s last line `audio_latency = "64"` with no newline is kept whole (A5);
    - **no retroarch.cfg**: the keychain file stays as it is, nothing is made, the exit is 0 (A5);
    - **idempotent**: a second scrub changes nothing (`cmp`) and logs nothing;
    - **the hook**: the line after `scrub)` in `system/rootfs/usr/libexec/banana/ra-secrets` is exactly the hook
      (`grep -A1`, as `core-overrides.sh` checks its own).
    It prints `ra-keychain: all <n> checks passed`, and on a `FAIL` it lists the files and exits 1.
  - `kiwami/tests/ci.sh`: runs `kiwami/tests/ra-keychain.sh` after `core-overrides.sh`. Its post-build checks:
    - line 42's tailscaled check goes from end-anchored to anywhere on the line, and a check for
      `/\*retroarch-keychain\.\*/` joins it;
    - each name is on the line exactly once (`grep -o | wc -l`);
    - a second post-build run leaves `share` byte-identical (`cmp`), as it does for `systems.conf`;
    - the run with the line taken out (`:64-65`) still fails.
  - `docs/FORK.md`, *Taking in upstream* step 3: the share script's sentence names the keychain files next to
    tailscaled's state. `ra-secrets` joins the `git diff $OLD main` list: if upstream reworks `scrub`, the hook has to
    stay first in it.
- **Tests**: `kiwami/tests/ra-keychain.sh` and the post-build checks, through `kiwami/tests/ci.sh .buildroot`;
  `sh -n` on `ra-keychain` and `ra-secrets` (ci.sh and CI's own); `make test`. The `docs/FORK.md` hook commands list
  `ra-secrets` with its `kiwami:` line.
- **Commit**: `RetroArch master: ra-secrets folds master's credentials file back into retroarch.cfg`

### 3. RetroArch's master, pinned behind a build option

- **Files**: `kiwami/Config.in` (changed), `kiwami/configs/kiwami.fragment` (changed), `kiwami/local.mk` (new),
  `kiwami/external.mk` (changed), `kiwami/patches/retroarch/0002-gfx-drm_ctx-fast-forward-with-two-swapchain-images.patch`
  (new, a copy of upstream's), `kiwami/patches/retroarch/0003-menu-Online-Updater-leaves-out-what-can-t-work-or-would-flood-the-card.patch`
  (new, rebased), `kiwami/patches/retroarch/retroarch.hash` (new), `kiwami/tests/retroarch-pin.sh` (new, mode 755),
  `kiwami/tests/ci.sh` (changed), `kiwami/DEPENDENCIES.md` (changed), `docs/FORK.md` (changed).
- **Change**:
  - `kiwami/Config.in`: after the cores' `source` lines, inside `menu "Kiwami"`:
    ```kconfig
    config BR2_PACKAGE_KIWAMI_RETROARCH_MASTER
    	bool "RetroArch's master instead of v1.22.2"
    	depends on BR2_PACKAGE_RETROARCH
    	help
    	  RetroArch built from its master branch at the commit kiwami/local.mk pins, with the fork's patches
    	  (kiwami/patches/retroarch/) instead of upstream's. Off, RetroArch is upstream's release.
    ```
  - `kiwami/configs/kiwami.fragment`: a comment (RetroArch's master, measured against v1.22.2 on the RG DS Plus;
    off puts upstream's back), then `BR2_PACKAGE_KIWAMI_RETROARCH_MASTER=y` and
    `BR2_PACKAGE_OVERRIDE_FILE="$(BR2_EXTERNAL_KIWAMI_PATH)/local.mk"`. Buildroot reads that path after `.config`,
    with the external path quoted, and `qstrip` takes the quotes out: tried on a scratch config.
  - `kiwami/local.mk`:
    ```make
    # The fork's package override file (BR2_PACKAGE_OVERRIDE_FILE, set in configs/kiwami.fragment): Buildroot reads it
    # after .config and before any package, so a version set here with override beats the package's own.
    # RetroArch's master over upstream's v1.22.2 (BR2_PACKAGE_KIWAMI_RETROARCH_MASTER); its patches, hash file and
    # options are in external.mk. A bump: the patches still apply, kiwami/tests/retroarch-pin.sh's stamps,
    # DEPENDENCIES.md.
    ifeq ($(BR2_PACKAGE_KIWAMI_RETROARCH_MASTER),y)
    override RETROARCH_VERSION = a81e450c00745b27ba417d486799b12f09c38ea4
    endif
    # the output's own override file, Buildroot's default, still read
    -include $(CONFIG_DIR)/local.mk
    ```
  - `kiwami/external.mk`, after its `include` line:
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
  - `kiwami/patches/retroarch/`:
    - `0002-...` is byte-identical to upstream's (it applies at offset 102);
    - `0003-...` is rebased on `a81e450`: the same behaviour, on master's `MENU_ENUM_LABEL_*_STR` entries. Its header
      says what was rebased, `Upstream-Status: Inappropriate [BananaOS-specific]` as upstream's;
    - upstream's 0001 isn't carried: master has `AUDIO_REINIT` (`command.h:1195`) and frees alsa-lib's configuration
      on close (`audio/drivers/alsa.c:740`).
  - `retroarch.hash`: `# Locally computed`, then the sha256 of `retroarch-a81e450c....tar.gz`, of
    `f3b62b847ed21939b668b897b18bbc1defbaae05.tar.gz` and of `COPYING`. They're computed by downloading once and
    running `sha256sum`; the RetroArch tarball fetched for planning had `8106b985...`.
  - `kiwami/tests/retroarch-pin.sh <buildroot dir> <output dir>...`, for outputs `ci.sh` has configured:
    - **on** (each output): `make -s -C <br> O=<o> printvars VARS='RETROARCH_VERSION RETROARCH_HASH_FILES
      RETROARCH_CONF_OPTS RETROARCH_TARGET_PATCH'`. The version is `a81e450c00745b27ba417d486799b12f09c38ea4`; the
      hash files are exactly the fork's; `RETROARCH_CONF_OPTS` contains `--disable-keychain`. Then `make -n` on the
      patch stamp: its `apply-patches.sh` lines name `buildroot/package/retroarch` and `kiwami/patches/retroarch`
      and not `buildroot/patches/retroarch`, and its `dl-wrapper` lines have only the fork's `-H` (A1). About 5 s
      each, with nothing downloaded (tried);
    - **off** (the first output, copied to `<o>-off`, the option set to `# ... is not set`, `olddefconfig`): the
      version is `v1.22.2`, the hash files are Buildroot's default pair, the options have no `--disable-keychain`,
      and `make -n` applies `buildroot/patches/retroarch`. Before the configure, a `local.mk` in `<o>-off` sets
      `KIWAMI_TEST_LOCAL_MK = 1`, and printvars shows it, as it does in the `on` output (A2);
    - prints `retroarch-pin: <board> pinned, off is upstream's`.
    `kiwami/tests/ci.sh` calls it after the boards' loop, with the three outputs.
  - `kiwami/DEPENDENCIES.md`:
    - **the fork's table** gets a new row: RetroArch (master), commit `a81e450`, 22 Oct 2026, pinned in
      `kiwami/local.mk` (`override RETROARCH_VERSION`, with `BR2_PACKAGE_KIWAMI_RETROARCH_MASTER`), with the sha256
      of both tarballs and `COPYING` in `kiwami/patches/retroarch/retroarch.hash`, from
      https://github.com/libretro/RetroArch. Its notes:
      - GPL-3.0+; the patches, upstream's 0002 as it is and 0003 rebased, with 0001 not needed and why;
      - `--disable-keychain` and the fold (`ra-keychain`, the sensitive keys `configuration.c` lists);
      - `GIT_VERSION`; the joypad tarball is upstream's pin, listed in the fork's hash;
      - the same reliance as v1.22.2 (7z, saves and states per core, per-core overrides over `--appendconfig`),
        checked at `a81e450`;
      - what a bump checks: the patches, the keychain's format and keys, the guard's stamps, the hash.
    - **upstream's table**, the RetroArch row: "while `BR2_PACKAGE_KIWAMI_RETROARCH_MASTER` is on (the fragment), the
      fork's pin above replaces it; `kiwami/tests/retroarch-pin.sh` stops CI when this version moves".
  - `docs/FORK.md`:
    - *Where a customization goes* gets a row: "Another version of one of upstream's packages:
      `kiwami/local.mk` (Buildroot's package override file, set by the fragment; `override <PKG>_VERSION`), its
      patches and hash in `kiwami/patches/<package>/`, behind an option in `kiwami/Config.in`".
    - *How the build takes in the fork* gets a sentence on the override file, and says a user's own
      `output/<board>/local.mk` is still read.
- **Tests**:
  - `kiwami/tests/ci.sh .buildroot`: the three boards keep both fragment lines (`configure.sh`), then
    `retroarch-pin.sh`.
  - `make retroarch-rgds-plus` (one board per call), its output kept in `/tmp/ra-master-build.log`:
    - the downloads checked against the fork's hash file;
    - `Applying 0002-...` and `Applying 0003-...` from `kiwami/patches/retroarch/`, with no fuzz or offset warnings
      for 0003;
    - in `output/rgds-plus/build/retroarch-a81e450.../config.mk`: `HAVE_KEYCHAIN = 0`, `HAVE_7ZIP = 1`,
      `HAVE_RETROSSL = 1`, `HAVE_STDIN_CMD = 1`, `HAVE_CHD = 1`, `HAVE_RFLAC = 1`.
    - On `output/rgds-plus/target/usr/bin/retroarch`: `file` (aarch64); its `readelf -d` `NEEDED` set against v1.22.2's
      13 libraries (above, in the step's overview); `strings | grep a81e450`; its size (`du -b`).
  - `make test` (unchanged, passing).
- **Commit**: `RetroArch master: master at a81e450 behind a build option, with the fork's patches for it`

### 4. Master without the built-ins Banana can't use

- **Files**: `kiwami/external.mk` (changed), `kiwami/tests/retroarch-pin.sh` (changed), `kiwami/DEPENDENCIES.md`
  (changed).
- **Change**:
  - `kiwami/external.mk`, in the option's block, the options grouped with a comment each:
    ```make
    # what master builds in by default and Banana can't use: the MCP server, its own NFS and SMB clients (nothing that
    # listens or mounts on the network), media decoders (WAV for the menu sounds and FLAC, which CHD's CD audio needs,
    # stay), the RAR reader (RAR games aren't listed) and Feral GameMode (no daemon)
    RETROARCH_CONF_OPTS += --disable-mcp --disable-retronfs --disable-retrosmb --disable-smbclient \
    	--disable-rav1 --disable-rvp9 --disable-rmp4 --disable-rwebm --disable-rmpeg1 \
    	--disable-raac --disable-ropus --disable-rvorbis --disable-rmp3 --disable-rmodtracker \
    	--disable-rrar --disable-gamemode
    ```
    Each flag is a `HAVE_` in master's `qb/config.params.sh` (`:75`, `:94-96`, `:241`, `:254-265`, `:270`). Master's
    `configure` stops on an unknown one (`qb/qb.params.sh`'s `opt_exists`). `--disable-smbclient` stops it looking for
    a system libsmb2 once its own client is off (`qb/config.libs.sh:1105-1121`). The stdin commands don't need the MCP
    server: `HAVE_COMMAND` comes from `HAVE_STDIN_CMD` (`config.libs.sh:238`).
  - `kiwami/tests/retroarch-pin.sh`: the `on` check also finds each of the 16 options in `RETROARCH_CONF_OPTS`, and
    `--disable-rflac` not there.
  - `kiwami/DEPENDENCIES.md`, the fork's RetroArch row: the disabled built-ins, why FLAC and WAV stay, and that a bump
    re-reads `qb/config.params.sh` for new default-on built-ins.
- **Tests**:
  - `kiwami/tests/ci.sh .buildroot`.
  - `make retroarch-reconfigure-rgds-plus`:
    - `config.mk` has `HAVE_MCP = 0`, `HAVE_RETRONFS = 0`, `HAVE_RETROSMB = 0`, `HAVE_SMBCLIENT = 0`,
      `HAVE_RRAR = 0`, `HAVE_GAMEMODE = 0` and the ten decoders at `0`, and still `HAVE_RFLAC = 1`, `HAVE_RWAV = 1`,
      `HAVE_CHD = 1`, `HAVE_7ZIP = 1`, `HAVE_STDIN_CMD = 1`, `HAVE_KEYCHAIN = 0`;
    - no `network/mcp_server.o` in the build folder;
    - the binary's size against step 3's and v1.22.2's, and `NEEDED` unchanged.
  - Then the whole image: `make rgds-plus`. `rootfs.squashfs`'s size against 172,720,128, and
    `banana-rgds-plus-<version>.update.tar` ready for the device tests (M2-M12).
  - `make test`.
- **Commit**: `RetroArch master: built without the MCP server, network clients, media decoders, RAR and GameMode`

### 5. A guard against upstream's RetroArch moving under the pin

- **Files**: `kiwami/tests/retroarch-pin.sh` (changed), `docs/FORK.md` (changed).
- **Change**:
  - `retroarch-pin.sh`, first, before any Buildroot call:
    ```sh
    # Upstream's RetroArch as it was when the fork's pin was last checked against it. When upstream moves one of these,
    # the fork's pin hides it: look at what upstream changed, then rebase or drop kiwami/patches/retroarch/, move the
    # pin (kiwami/local.mk) or turn it off, and update the stamp here.
    UP_VERSION=v1.22.2
    UP_JOYPADS=f3b62b847ed21939b668b897b18bbc1defbaae05
    UP_PATCHES=0a6ab4470613449b3910ca1788386257a32381a6aae1efeef0ae909be1c7f94f
    ```
    The checks run against `buildroot/package/retroarch/retroarch.mk`:
    - `RETROARCH_VERSION` and `RETROARCH_JOYPADS_VERSION`, read with `sed`, must equal the stamps;
    - the patches' sum, `(cd buildroot && { find patches/retroarch -type f; find package/retroarch -name '*.patch'; } |
      LC_ALL=C sort | xargs sha256sum | sha256sum | cut -c1-64)`, must equal `UP_PATCHES` (that's today's value);
    - each failure says what moved (`retroarch-pin: upstream's RETROARCH_VERSION is v1.23.0, the pin was checked
      against v1.22.2: ...`) and exits 1.
  - `docs/FORK.md`, *Taking in upstream* step 3: `buildroot/package/retroarch/retroarch.mk` and
    `buildroot/patches/retroarch/` join the `git diff $OLD main` list, with a sentence. The fork's pin shadows
    upstream's RetroArch version and patches; `kiwami/tests/ci.sh` stops on a change; the fix is in the stamps'
    comment.
- **Tests**:
  - `kiwami/tests/ci.sh .buildroot` passes.
  - `retroarch-pin.sh --self-test`, run by `ci.sh`, checks the guard on copies. It copies `buildroot/package/retroarch/`
    and `buildroot/patches/retroarch/` into a `mktemp -d`, which the guard reads through `KIWAMI_TREE` (`.` by
    default). The unchanged copy passes. Each of four changes fails with its own message:
    - `RETROARCH_VERSION = v1.23.0`;
    - a changed joypads commit;
    - a new `0004-x.patch` in `patches/retroarch/`;
    - an `x.patch` in `package/retroarch/`.
  - `make test`.
- **Commit**: `RetroArch master: CI stops when upstream's RetroArch moves under the fork's pin`

### 6. Final check

- `make test`, `make -C ui`, `make -C system/bananad CFLAGS="-O2 -Werror"`, `kiwami/tests/ci.sh .buildroot`.
- The fork's tests in `kiwami/tests/`, through `ci.sh`: `ra-keychain.sh`, `retroarch-pin.sh` (with its self-test),
  `core-overrides.sh`, `cores-ui.sh`, `tailscale-ui.sh`, `7z-ui.sh`, the helper and unit tests, post-build.
- The hooks: `git diff --stat main...HEAD` and `git grep -n 'kiwami:'` (the commands in `docs/FORK.md`) agree. The only
  new hook is `ra-secrets`' `ra-keychain` line.
- `make test`'s renders in `ui/tests/out/` match `kiwami`'s from before the feature: the branch's base, its `ui/`
  exported with `git archive` and built under `/tmp`, both suites run side by side so their clocks agree, as in 0006
  (A17). No banana-ui source changes in this feature, so they should be identical.
- `make retroarch-rgds` (one call): the `rgds` board builds the same RetroArch, patched, as no RG DS is at hand to
  run it.
- `make rgds-plus` from scratch for RetroArch (`make retroarch-dirclean-rgds-plus` first). The binary's and
  `rootfs.squashfs`'s sizes are noted against 15,008,688 and 172,720,128 bytes. `ls output/rgds-plus/target/usr/share/libretro/autoconfig/udev | wc -l`
  equals v1.22.2's count (A15).
- `kiwami/DEPENDENCIES.md` has the fork's RetroArch row (steps 3, 4) and upstream's row updated. `docs/FORK.md` is up
  to date (steps 2, 3, 5). `docs/ARCHITECTURE.md` and `CLAUDE.md` don't name RetroArch's version, so they need nothing.
  Upstream's `README.md` ("RetroArch 1.22") is upstream's, left alone.
- The manual tests below all passed, or a deviation says what wasn't verified and the user accepted it. The
  headphone jack, an external pad, N64, PS1 and the RG DS are not verified from the start (*Refinements*).
- Every acceptance criterion is met: each is listed by at least one automated or manual test, and those passed.
- **Commit**: `RetroArch master: the final check done`

## Automated tests

| Test | Where | What it checks | Criteria |
|---|---|---|---|
| The fold | `kiwami/tests/ra-keychain.sh` (run by `ci.sh`, step 2) | `ra-keychain` alone and through a hooked `ra-secrets` on a made-up card: the keychain's lines in `retroarch.cfg` once each, the keychain's value winning, the files gone, a log line with no value; through the scrub, password and token in `accounts.conf` (same and another account), the username in `retroarch.cfg`; special characters and a missing final newline; no `retroarch.cfg`; idempotent; with no keychain, byte-identical to upstream's script; the hook's place | A5, A6, A7 |
| The veto | `kiwami/tests/ci.sh`, post-build checks (step 2) | `*tailscaled.state` and `*retroarch-keychain.*` on upstream's veto line, once each; a second run changes nothing; the build stops without the line | A8 |
| The fork's options configured | `kiwami/tests/ci.sh` (`configure.sh` for `rgds`, `rgds_plus`, `universal`) | `BR2_PACKAGE_KIWAMI_RETROARCH_MASTER=y` and `BR2_PACKAGE_OVERRIDE_FILE` survive on every board | A1 |
| The pin | `kiwami/tests/retroarch-pin.sh` (run by `ci.sh`, steps 3-4) | On: version `a81e450...`, only the fork's hash file, the patch step's folders (`make -n`), the 17 configure options and FLAC kept. Off: upstream's version, hash files, patch folder, options. The output's own `local.mk` read both ways | A1, A2, A3 |
| The guard | `kiwami/tests/retroarch-pin.sh` and its `--self-test` (step 5) | Upstream's version, joypads commit and patches' sum against the stamps; each of the four changes caught, with its message | A4 |
| The build | steps 3-4 and the final check, by hand | The downloads hashed, the fork's patches applied cleanly, `config.mk`'s features, an aarch64 binary with v1.22.2's libraries and `a81e450` in it, its size, the squashfs's size, the autoconfigs' count | A3, A15, A17 |
| Upstream's own | `make test` | Passing; renders identical to `kiwami` before the feature; the frame bench unchanged | A17 |

## Manual tests

On the device, for what headless tests can't reach. Logs are fetched over SSH/SFTP (Settings > System > SSH) or from
the card on a computer (`banana/logs/` on the card is `/data/banana/logs/` on the device). Only the RG DS Plus is at
hand: every test runs on it.

Two images, one card. **v1.22.2** is the image the device runs now, `kiwami` at `19a559c` (0.4.1). **master** is the
update built in step 4 (or later), installed over the same card after M1 and M2. `measure.sh` (step 1) is copied to
`/data/banana/measure.sh` and run over SSH as `sh /data/banana/measure.sh ...`. It writes `/data/banana/logs/ra-*`, which
is all there is to fetch (`ls /data/banana/logs/ra-*`), plus `banana-ui.log` and `bananad.log` where a row says so.
Every measured run has the game panel on (Banana's default, so `retroarch.log` exists), RetroArch's statistics on
(Quick Menu > On-Screen Display > Notifications > Display Statistics), 30 s of the same scene on both images. The
lowest fps seen on the counter is noted, along with how the run went, by ear and by eye. A run's `<name>` is
`<image>-<game>-<core>`, e.g. `v122-cv-fceumm`, `master-cv-fceumm`. Games: `cv` Castlevania (stage 2), `smb` Super Mario
Bros (1-1 running right), `ladx` Link's Awakening DX (Mabe Village into the overworld), `s3k` Sonic 3 & Knuckles (Angel
Island 1, running), `emerald` Pokémon Emerald (a wild battle in tall grass), `smw` Super Mario World (Yoshi's Island 2).

| # | Board | Steps | Expected | Logs to collect | Criteria | Outcome |
|---|---|---|---|---|---|---|
| M1 | rgds-plus, v1.22.2 | The baseline. For each of the 10 game-core pairs (`cv` and `smb` on FCEUmm and Mesen, `ladx` on Gambatte and SameBoy, `s3k` on PicoDrive and Genesis Plus GX, `emerald` on gpSP, `smw` on Snes9x), the core chosen in Settings > Other systems: start the game, reach the scene, `measure.sh run v122-<game>-<core>`, then quit and `measure.sh log v122-<game>-<core>`. `cv` on FCEUmm once more as `v122-cv-fceumm-2` (the spread). `emerald` and `smw` again with `measure.sh powersave` before `run` (`v122-<game>-ps`). | Every run recorded; Mesen and SameBoy threaded (0006's overrides, in the header) | `ra-v122-*.txt`, `ra-v122-*.log`; the lowest fps of each run, noted | A9 (the baseline) | not run |
| M2 | rgds-plus, v1.22.2 | The baseline of the rest, on `s3k` with PicoDrive unless said: Settings > Other systems > Audio latency 32, 64, 128, a minute each on the speakers; at 64, `measure.sh reinit` mid-game. VSync off, `smw`: speed and tearing; VSync back on. Fast-forward (the game panel's tile) on `cv` FCEUmm: the fps it reaches. On a game with achievements (`smb` FCEUmm), the game panel's achievements list as it shows. RetroArch's menu > Online Updater: the entries listed (a photo or a list). After a game, `measure.sh card v122`. | Recorded as v1.22.2 does it; the panel's list may show an error (oversight 5): recorded as it is | `ra-v122-*.log` (named `v122-lat32` ... `v122-ff`, `v122-panel`), `ra-reinit.txt`, `ra-v122-card.txt`; what was heard and seen, noted | A10, A12, A13, A15 (the baseline) | not run |
| M3 | rgds-plus, master | Install master's update. M1 again with `master-` names. | Every run meets the bar against M1 (*Refinements*). A shortfall is a deviation | `ra-master-*.txt`, `ra-master-*.log` (each header says `(Git a81e450)`); the lowest fps, noted | A9 | not run |
| M4 | rgds-plus, master | Mesen and SameBoy unthreaded: over SSH, set `video_threaded = "false"` in `/data/retroarch/config/Mesen/Mesen.cfg` and `SameBoy/SameBoy.cfg`. Then `cv` on Mesen and `ladx` on SameBoy, `measure.sh run master-<game>-<core>-unthreaded`. Then delete both lines (the fork writes `"true"` again at the next start). | Recorded only (suggestion 3) | `ra-master-*-unthreaded.txt`; the lowest fps | A18 | not run |
| M5 | rgds-plus, master | M2's audio part: 32, 64, 128 ms on `s3k` PicoDrive, a minute each; at 64, `measure.sh reinit`. | No crackle where M2 had none; sound carries on after each reinit; no more underruns logged than v1.22.2's (`grep -ci 'underrun\|xrun'`) | `ra-master-lat32.log` ... `lat128.log`, `ra-reinit.txt`, `bananad.log` | A10 | not run |
| M6 | rgds-plus, master | Sleep mid-game: `s3k` PicoDrive with the game panel on, close the lid 10 s, open, play 30 s, quit, `measure.sh log master-sleep-panel`. Then Settings > Other systems > Second screen: Off, `cv` FCEUmm, the same. Second screen back to Game panel. | Each sleeps and comes back with picture and sound; `retroarch.log` (panel) has no `[KMS] Display changed` | `ra-master-sleep-panel.log`, `bananad.log`, `banana-ui.log` | A11 | not run |
| M7 | rgds-plus, master | M2's VSync off on `smw`, and fast-forward on `cv` FCEUmm. | VSync off: normal speed as on v1.22.2, tearing noted; fast-forward at least M2's fps | `ra-master-vsync.log`, `ra-master-ff.log`; the fps and what was seen, noted | A12 | not run |
| M8 | rgds-plus, master | The game panel, `smb` FCEUmm: save state, load state, menu (and back), fast-forward, quit tiles, each once. During the game, tap the panel away from the tiles a few times. The achievements list as in M2. | RetroArch on the top panel, the tiles all work, the taps do nothing in the game or RetroArch's menu, the list as in M2 | `ra-master-panel.log`, `banana-ui.log` | A13 | not run |
| M9 | rgds-plus, master | RetroAchievements, the setup's account: `smb` FCEUmm, the login (the panel and RetroArch's notification), the game identified, an unlock (e.g. the first one the game offers). Quit; `measure.sh card master-ra`. Then in RetroArch's own menu, Settings > Achievements: log out and in again with the same account (its password typed), quit; `measure.sh card master-ra-menu`. | Logged in over master's TLS client, identified, the unlock shown; after each game: no `retroarch-keychain.*`, no password or token in `retroarch.cfg`, the username there, `ra_password` and `ra_retroarch_token` `<set>` in `accounts.conf` | `ra-master-ra.log`, `ra-master-ra-card.txt`, `ra-master-ra-menu.log`, `ra-master-ra-menu-card.txt`, `banana-ui.log` (`kiwami: ra-keychain: ...` after each game) | A6, A14 | not run |
| M10 | rgds-plus, master | File sharing: Settings > Network > File sharing on. Start `cv` FCEUmm, open RetroArch's menu > Configuration File > Save Current Configuration (the keychain file now exists), and from the computer list `\\BANANA\banana\retroarch\` and try opening `retroarch\retroarch-keychain.cfg` by its path. Quit the game, list again. `measure.sh card master-share`. | Neither listing shows `retroarch-keychain.*`, the open is refused; the veto line in `ksmbd.conf` has `*retroarch-keychain.*/` | `ra-master-share-card.txt` (`ls` during is in the computer's listing: noted) | A8 | not run |
| M11 | rgds-plus, master | A `.7z` NES game and a `.7z` GBA game start (0003's). Quick resume on: `smb` quit mid-level, start again; Quick resume off. Shader: LCD, a game, then Off. RetroArch's menu > Online Updater: the entries, against M2's. A fresh `retroarch.cfg`: over SSH `mv /data/retroarch/retroarch.cfg /data/retroarch/retroarch.cfg.kept`, start `cv`, quit, `mv` it back. | Each works as on v1.22.2; the updater lists M2's entries; the fresh config game starts with Banana's settings | `banana-ui.log`, `ra-master-misc.log` (`measure.sh log` after the shader game) | A15 | not run |
| M12 | rgds-plus, back to v1.22.2 | Install the v1.22.2 update again over this card. `smb` FCEUmm: the login and the game identified. `measure.sh card v122-back`. | Starts and logs in; the card as A6 | `ra-v122-back.log`, `ra-v122-back-card.txt` | A16 | not run |

Notes from the runs (what the logs showed, what was fixed):

## Deviations

Where the implementation turned out different from this plan, why, and what changed (dated).

## Progress

A step is marked done in its own commit, so the commit column holds the subject (a commit can't carry its own hash).

| Step | Status | Commit |
|---|---|---|
| 1. The measurements' script | todo | |
| 2. Master's credentials file folded back into retroarch.cfg | todo | |
| 3. RetroArch's master, pinned behind a build option | todo | |
| 4. Master without the built-ins Banana can't use | todo | |
| 5. A guard against upstream's RetroArch moving under the pin | todo | |
| 6. Final check | todo | |
