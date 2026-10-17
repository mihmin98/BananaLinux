# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## This fork

This repository is a personal fork of upstream BananaLinux. `main` tracks the upstream repo; `kiwami` carries this
fork's own customizations, which upstream does not have. Do customization work on `kiwami` (or branches off it), keep
`main` as a clean mirror of upstream, and bring upstream changes in by updating `main` and merging it into `kiwami`.
When comparing against "upstream" behaviour, `main` is the reference.

## Git: the user commits, not Claude

Never commit, amend, merge, rebase, cherry-pick, reset, checkout/switch branches, stash, tag, push, or otherwise
change the repository's history, refs or branches; the user does all of that by hand. Read-only git (`status`,
`diff`, `log`, `show`, `blame`) is fine, and so is `git add` to stage the files that belong in the commit. When a
change is ready, stage it and give the user a commit message to use (subject line plus body, in the style described
under *Conventions*), then stop. Commit messages carry no `Co-Authored-By` trailer or other AI attribution lines.

## The project

BananaOS ("Banana") is a Buildroot-based Linux for the Anbernic RG DS and RG DS Plus (RK3566, two panels, clamshell).
No systemd, udev, compositor or sound server: busybox init runs `rcS`, which starts `bananad`, and init starts
`banana-ui` on both panels. The root is a read-only squashfs; everything the user adds lives on an exFAT data
partition mounted at `/data` (`/data/banana/` holds settings, logs, accounts, updates). `docs/ARCHITECTURE.md` is
the detailed reference (card layout, A/B updates, boot, the menu's renderer, sleep, clocks, QEMU); read it before
changing anything structural. `docs/EMULATORS.md` is the contract between the menu and any game launcher.

## Commands

```sh
make ui              # banana-ui for this machine, headless build (ALSA=0)
make test            # builds ui, then ui/tests/run.sh: headless renders + checks at 640x480 and 1024x768 (needs ImageMagick)
make -C ui           # the full frontend with ALSA (what CI also builds)
make -C system/bananad CFLAGS="-O2 -Werror"          # bananad + banana-panel-tune (CI uses -Werror, also for aarch64)
sudo system/bananad/tests/sleep-test.sh              # bananad's sleep against a fake /sys in private namespaces

make rgds | rgds-plus | universal                    # full SD image via Buildroot 2026.02.3 (cloned into .buildroot/; 1h+)
make <target>-rgds                                   # any Buildroot target per board, e.g. linux-menuconfig-rgds, banana-ui-rebuild-rgds
tools/qemu-boot.sh output/rgds/images/banana-rgds.img.gz [--interactive]   # boot the whole image in QEMU
```

There is no single-test runner: `ui/tests/run.sh` is one script. To check one thing, run `banana-ui --headless`
directly with a `--script` (fixture from `ui/tests/fixture.sh <dir>`):

```sh
ui/banana-ui --headless out/ --size 1024x768 --assets ui/assets/rnds --splash ui/assets/splash --banana ui/assets/banana \
    --systems system/rootfs/etc/banana/systems.conf --roms <fx>/roms --data <fx>/data \
    --script "home 0; wait 2400; shot home; press a; wait 2400; shot lib; bench 120"
```

Script verbs: `home`, `press <btn>`, `touch`, `wait <ms>`, `shot <name>` (writes `<prefix><name>-top/bot.png`),
`bench <frames>`, `splash`, `setup`, `battery <pct> [charging]`, `romicon <path>`, `mem`, `lib`. Tests assert on
banana-ui's stderr log lines (e.g. `press a: used, view 1`), so changing log wording can break `run.sh`.
Env hooks used by tests: `BANANA_WIFI` (stand-in `wifi` script, `ui/tests/fake-wifi.sh`), `BANANA_RA_CHECK=0`.

CI (`.github/workflows/ci.yml`) also runs `sh -n` on every shell script under `system/rootfs/{etc/init.d,usr/bin,usr/libexec/banana,etc/banana/sleep.d}`,
`buildroot/board/rgds/*.sh` and `tools/*.sh`, and verifies each defconfig keeps every option after Buildroot
expands it (an option silently dropped by a missing dependency fails CI). Image and QEMU-boot jobs run only on
manual dispatch.

## Layout and how the pieces connect

- `ui/` — `banana-ui` (C++17, software-rendered, KMS dumb buffers). `src/` is Banana's host: `main.cpp` (poll loop,
  CLI, headless script), `soft.cpp` (the Backend: diffing draw lists per screen, run-length textures), `kms.cpp`,
  `library.cpp` (the Source: systems, games, gamelist.xml/media art, play stats), `settings.cpp` (settings and
  first-time setup), `wifi.cpp`, `splash.cpp` (boot animation), `carts*.cpp` (Carts theme). Also builds
  `banana-scrape` and `banana-leaseshim.so` (DRM lease preloaded into RetroArch).
- `ui/rnds/` — ROCKNIXDS Pixel's engine, **imported** from ROCKNIXDS by `tools/import-rnds.sh`. Treat as vendored:
  `ui/rnds/UPSTREAM` lists the local changes that must be carried into ROCKNIXDS before re-importing (re-import
  overwrites them). `ui/assets/overrides/` is copied over the imported assets.
- `system/bananad/` — `bananad.c` (single C file: sleep/lid/power key, LEDs, ALSA routing, volume/brightness keys,
  battery guard, dimming) and `banana-panel-tune.c`. Config `/etc/banana/bananad.conf`, overridden by
  `/data/banana/bananad.conf`; reloaded on SIGHUP. Wakes banana-ui with SIGUSR1 after resume.
- `system/rootfs/` — rootfs overlay: `etc/init.d/rcS`, `etc/banana/systems.conf` (per-system `run` command with
  `%ROM%`/`%SYSTEM%`, `perf` profile), default `settings.conf`, `sleep.d/` hooks (`pre`/`post`), and the launchers
  and helpers in `usr/libexec/banana/` (`run-*`, `firstboot`, `update`, `wifi`, `volume`, `share`, `ssh`).
  `usr/bin/banana-perf` sets clock profiles.
- `buildroot/` — BR2_EXTERNAL tree. `configs/banana_{rgds,rgds_plus,universal}_defconfig`; `package/` has Banana's
  packages (`banana-ui` and `bananad` are `SITE_METHOD = local` pointing at `ui/` and `system/bananad/`; emulators
  and cores pinned by commit). `external.mk` applies ROCKNIX kernel patches with fuzz and copies the out-of-tree
  panel/joypad drivers and device trees into the kernel. `board/rgds/` is shared by both handhelds.
- `buildroot/board/rgds/` — mostly **synced from ROCKNIX** at the commit in `board/rgds/ROCKNIX` by
  `tools/sync-rocknix-hw.sh` (kernel patches, DTS, drivers, base kernel config, U-Boot patches, BL31). Keep those
  files as ROCKNIX wrote them; Banana's own changes go in `linux-banana.fragment`, `uboot-*.fragment`,
  `dts-patches/`, and `buildroot/pending-patches/`.
- `post-build.sh` stamps `VERSION` (repo root) into `/etc/banana/version` and os-release; `post-image.sh` builds the
  `.img.gz` (genimage) and the `banana-<board>-<version>.update.tar` A/B update package.

Key runtime contracts that span files: banana-ui writes the running game's pid to `/run/banana/game.pid` and closes
display/input/sound before launching (bananad SIGSTOPs that process group on sleep); settings changes are written
immediately to the file that owns them (`settings.conf`, `bananad.conf` + SIGHUP, `superdrastic.conf`,
`drastic.cfg`); kernel cmdline `banana.console=1`, `banana.top=`, `banana.touch=` adapt the image to QEMU.

## Conventions

- Prose style in comments, docs and commit messages is plain descriptive English about behaviour ("The menu holds
  still: no bob ..."), not conventional-commit prefixes; a component prefix like `banana-ui:`, `rnds:`, `qemu-boot:`
  or `BananaDS <sha>:` is used when it helps. Match this.
- Performance and footprint are the point of the project (menu frames under a millisecond, nothing running while
  idle, games get the whole device). Avoid adding polling, background threads, daemons or rootfs weight.
- Rendering optimisations in banana-ui/rnds are expected to keep frames bit-identical; `make test`'s PNGs in
  `ui/tests/out/` are the way to compare.
- Supported boards are `rgds` and `rgds-plus` (and `universal`, one card for both, built locally but not in CI).
