# 0026 Build speed: investigation

Issue: #26 · Branch: `k/0026-build-speed` · Status: refined · Confidence: 92% (proceed)

## The request

Image builds take too long: a full build of a fresh output with the compiler cache already filled takes 15-18 minutes
on this machine (Ryzen 7 7700X, 16 threads, 61 GB of RAM), and a cold one, with an empty cache, an hour or more
(README). The feature makes them faster, and the image (`banana-<board>.img.gz`) and the update package
(`banana-<board>-<version>.update.tar`) must come out the same as before.

Decided while discussing the request:

- **Which builds**: full builds of a fresh output, with a warm and with a cold compiler cache. Rebuilds after a change
  in a few packages aren't the target (they get faster anyway, through banana-ui).
- **"The same"** means the same contents: every file of the root filesystem (content, type, mode, owner, symlink
  target), the kernel, the device trees, U-Boot, the boot partition and the partition layout equal to a build made
  today's way, apart from what already differs between two of today's builds of the same commit (build dates, mtimes,
  random UUIDs: *Current state*). A comparison script checks this; a difference that is only the output directory's
  path (Python's build configuration, libstdc++'s gdb helper) is allowed and listed apart (*Open questions*, 6).
- **Boards**: measured and verified on `rgds-plus`; `rgds` and `universal` take the same changes and must still
  configure and build.
- **In scope**: local image builds (`make rgds-plus`), and banana-ui's host build for the headless tests (`make ui`,
  `make test`).
- **The speed-ups**: banana-ui built from objects (in the image and on the host), and Buildroot's top-level parallel
  build (per-package directories), with the top Makefile adding the `-j` itself. One switch builds today's way.
- **Measurements**: warm and cold builds, today's way and the new way, run by the user (about three hours in all).

Out of scope: CI's image job (though it would take the changes: *Risks*), anything that changes the image's contents to
gain speed (compiler flags, kernel options, package versions, dropped packages), and banana-fetch's own compile
(*Suggestions*).

## Oversights in the requirements

1. **"Before" can't be today's output.** `output/rgds-plus` has been built incrementally since 26 Oct, and Buildroot
   never removes a package's files from `target/` when the package goes away: `target-finalize` copies into the
   existing directory (`.buildroot/Makefile:756-758`). The 0.5.3 image built there today still carries 2,799 files of
   Poketool (`usr/share/banana/apps/bank/`, 79 MB, its `Poketool.app` entry on the shelf), `PortMaster.zip`, and the
   fork's old `usr/libexec/kiwami/romm` and `romm-game`, all of which upstream's 0.5.3 dropped
   (`f2c083c0`) or the fork never merged. Compared with it, a fresh build of the same commit "differs" by all those
   files. *Proposed*: the reference is always a fresh build of the same commit, today's way, in its own output
   directory (`make OUT=<dir> rgds-plus`; the top Makefile's `OUT ?=`). The user should also know that the 0.5.3
   update on the handheld ships Poketool and PortMaster; the first fresh build removes them. *Decided*: yes, and a note
   on fresh builds goes in `docs/FORK.md` (*Suggestions*).

2. **Two builds of the same commit already differ.** `BR2_REPRODUCIBLE` is off, so these change from build to build:
   - the kernel's build date (`UTS_VERSION`, "#1 SMP PREEMPT Mon Nov 2 15:18:16 EET 2026") in `Image`;
   - U-Boot's build date and time ("U-Boot 2026.01 (Oct 26 2026 - 19:02:12 +0200)");
   - BusyBox's build time ("BusyBox v1.37.0 (2026-10-26 18:54:36 EET)");
   - `__DATE__` in RetroArch and in VICE's core (the build's day);
   - the GPT's disk and partition UUIDs, which genimage draws at random (`host-genimage-19/image-hd.c:868,1124`);
   - the FAT boot partition's volume ID and file times, and the squashfs's creation time and file mtimes;
   - Buildroot's bookkeeping files (`.files-list*.before`, `.stamp_*`, whose contents are inode numbers and mtimes),
     which `retroarch-glsl-shaders.mk:15` copies into the image with the shaders (`ls -A | grep -vxE ...` keeps dot
     files).

   Python's `.pyc` files don't change: Buildroot exports `SOURCE_DATE_EPOCH` even with `BR2_REPRODUCIBLE` off
   (`.buildroot/Makefile:540`), and `compileall` then writes hash-checked `.pyc` files.
   *Proposed*: the comparison masks these (the date strings in place, the UUIDs and times ignored) and fails on
   anything else. A first pair of today's builds proves the mask is complete before the tool judges a change.
   *Decided*: yes.

3. **Per-package directories expose undeclared dependencies.** A package then sees only what it declares. Checking
   what every package of `buildroot/package/` and `kiwami/package/` links (`readelf -d` on its files in `target/`)
   against what it declares, one package is missing a dependency: `libretro-mupen64plus-next` links `libEGL.so.1` and
   `libGLESv2.so.2` (`FORCE_GLES3=1`) and declares nothing (`libretro-mupen64plus-next.mk`). Today it builds only
   because Mesa happens to come first in the serial order. Under per-package directories its link would fail, loudly.
   bluez-alsa's `ldacbt`, added after `bluez-alsa.mk` is evaluated (`buildroot/external.mk:55-61`), should work: the
   per-package copy reads `<PKG>_FINAL_DEPENDENCIES` when the step runs (`.buildroot/package/pkg-generic.mk:256,804`),
   and upstream's order-only prerequisite orders it. A silent difference (a configure script that drops an optional
   feature because it no longer finds an undeclared library) isn't visible this way; the comparison catches it.
   *Proposed*: the missing dependencies are declared in `kiwami/external.mk`, each with an order-only prerequisite like
   upstream's for ldacbt (the fork's `external.mk` is read after every package is evaluated, so a `+=` alone doesn't
   order the build). The first parallel build may reveal more. *Decided*: yes.

4. **Upstream will keep adding packages.** Upstream doesn't build with per-package directories, so a package it adds or
   changes later with an undeclared dependency breaks the fork's parallel build after a merge, or changes the image
   silently. *Decided*: `docs/FORK.md`'s after-merge checks and `.claude/skills/banana-merge-upstream/SKILL.md` gain a
   step: the dependency audit (a script: oversight 3's `readelf` check against `make show-info`'s declared
   dependencies) is run on a new or changed upstream package, and missing dependencies go in `kiwami/external.mk`.
   The off switch builds images meanwhile.

5. **Files installed by two packages.** Today the last package installed wins, in build order. Under per-package
   directories the merge goes through the package list sorted by name (`.buildroot/Makefile:758`), so a file two
   packages install could end up as the other's. The current output's file lists are muddied by rebuilds
   (`packages-file-list.txt` attributes kernel modules to `ldacbt`), so this can only be checked on a fresh build's lists
   (`support/scripts/check-uniq-files`). *Proposed*: the plan checks the baseline's lists; the comparison catches any
   that comes out differently.

6. **Switching is a fresh output, and its first build is cold.** Buildroot can't turn per-package directories on in an
   existing output. And ccache's keys include the sysroot path relative to the output directory
   (`BR2_CCACHE_USE_BASEDIR`), which becomes `per-package/<pkg>/host/...`: the first parallel build misses the cache
   everywhere and costs a cold build. After that, fresh parallel builds hit the cache again. *Proposed*: say so in
   `docs/FORK.md`; the build stops with a message when an output's mode doesn't match the switch, rather than mixing
   them.

7. **Parallelism has to be there.** Per-package directories without a top-level `-j` are slower than today (each step
   first copies its dependencies' files), and Buildroot builds one package at a time otherwise (`.NOTPARALLEL`,
   `.buildroot/Makefile:247-252`). *Decided*: a `kiwami:` hook on the top Makefile's board rule (`Makefile:32`) adds
   `-j` when none was given, so plain `make rgds-plus` is parallel. With a top-level `-j`, Buildroot stops forcing
   `-j$(PARALLEL_JOBS)` on each package (`.buildroot/package/Makefile.in:23-24`), so packages share make's job slots
   instead of multiplying them (ninja-based packages, Mesa and the meson ones, still take their own `-j17`).

8. **The parallel build's output is interleaved.** Several packages print at once, so the line that failed is harder to
   find in the terminal. *Proposed*: the plan decides between make's `--output-sync=target` and pointing at the failed
   step's own log; nothing for the user to choose.

9. **Measuring a cold build mustn't empty the warm cache.** *Proposed*: cold runs use a throwaway cache
   (`BR2_CCACHE_DIR=<dir> make ...`, which overrides the defconfig's `$(HOME)/.buildroot-ccache`: `.buildroot/Config.in:385-390`),
   deleted afterwards. The comparison builds also need disk: about 15 GB per output (`output/rgds-plus` is 15 GB), and
   104 GB are free.

10. **A setting?** `docs/FORK.md` puts changed behaviour behind a setting defaulting to upstream's. The image is the
    same, so there's no behaviour to keep at upstream's default; the build method is what changes. *Decided*: both
    speed-ups are on by default, and one switch (e.g. `make KIWAMI_SERIAL=1 rgds-plus`) builds today's way from the same
    commit: no per-package directories, no added `-j`, banana-ui in one `g++` call. It builds the baseline (only the
    method differs from the new build) and is the fallback when a merge breaks the parallel build.

## Suggestions

- **banana-ui from objects** (*decided*: in). `ui/Makefile:31-32` compiles banana-ui's 76 sources in one `g++` call:
  serial, and uncacheable for ccache ("multiple source files"). It takes 95 s, measured, on the host and in the image
  (90-105 s in `build-time.log`), on every build. Compiled one file per call, on 16 jobs, it takes 16.7 s, bounded by
  `settings.cpp` alone (16.7 s; then `main.cpp` 10.8 s, `layout.cpp` 9.2 s). With ccache hits in the image it takes a
  few seconds. The binary is **byte-identical**: built from per-file objects linked in `SRC`'s order with the same
  flags, it equals the one-call build stripped and unstripped (7,055,184 bytes, host GCC 16.2; the target's GCC 14.3 is
  checked in the plan). The fork's `ui/src/kiwami/kiwami.mk`, which `ui/Makefile:20` already includes before the rule
  reads `SRC`, can do it alone: it already puts objects in `SRC` (`kiwami.mk:28`). So no upstream line changes. On the
  host, `make ui` and `make test` then recompile only what changed.
- **The parallel package build** (*decided*: in). `BR2_PER_PACKAGE_DIRECTORIES=y` in `kiwami/configs/kiwami.fragment`
  and the top Makefile's `-j`. From the measured step times and `make show-info`'s graph:
  - a warm full build's package steps add up to 16.2-17.6 min, run one after another;
  - the longest dependency chain is 3.0 min (host-python3, host-python-cython 53.5 s, host-python-pyyaml, Mesa, SDL2,
    dodakvarter), once banana-ui is fixed; 3.3 min before, ending at banana-ui's 105.8 s;
  - a warm full build should therefore land around 5-8 min, against 15-18 min today;
  - the cold build gains less, because its big compiles (the kernel, Mesa, RetroArch) already use every thread: an
    estimated 25-40 min against an hour or more.

  All four numbers are to be measured. Buildroot calls the option experimental.
- **Document fresh builds** (*decided*: in). A note in `docs/FORK.md`: images for the handheld come from a fresh output
  (`make clean-rgds-plus`, then `make rgds-plus`: the `%-$(1)` rule passes `clean` to Buildroot, which keeps
  `.config`), because incremental builds keep removed packages' files (oversight 1). The faster full build is what makes
  that affordable.
- **Texts to offer upstream** (*not taken*; a proposal): banana-ui's per-object build in upstream's own Makefile,
  `libretro-mupen64plus-next`'s missing `libegl libgles` dependency, and `retroarch-glsl-shaders`' install copying
  Buildroot's dot files into the image (about 4 MB). Once upstream takes one, the fork's copy goes.
- **banana-fetch from objects** (*not taken*). It's 14 sources in one call (`ui/Makefile:60`, 9 s). It would save about
  6 s in `make ui` and nothing in the image, where it builds alongside banana-ui.
- **Left out, with why**:
  - `BR2_REPRODUCIBLE` changes the image's timestamps, so the image wouldn't be today's.
  - The host's own tools instead of Buildroot's (`host-python-cython`, 53 s for Mesa's PyYAML; `host-gnutls`, 37 s for
    U-Boot): different versions could change what they generate.
  - `pigz` for the image's `.gz` would give different bytes, and only saves about 5 s: the whole image stage, squashfs
    to update tar, takes 13 s.
  - Building in tmpfs: 15 GB per output on top of the build's own memory.
  - Sharing Go's build cache across outputs: tailscale builds in 11 s.
  - The other one-call compiles: dodakvarter, SuperDrastic and the Store take 12-14 s each, and would overlap with
    other packages under the parallel build anyway.
  - Building `rgds` from `rgds-plus`'s output: the two differ in the board name and one firmware package, but the
    focus is `rgds-plus`.

## Current state

- **How a build runs.** `make rgds-plus` configures `output/rgds-plus` from upstream's defconfig and the fork's
  fragment (`Makefile:26-29`, `kiwami/configure.sh`) and runs Buildroot there (`Makefile:32`). Buildroot builds one
  package at a time (`BR2_PER_PACKAGE_DIRECTORIES` is unset, so `.NOTPARALLEL`: `.buildroot/Makefile:247-252`), each
  with `-j17` (`BR2_JLEVEL=0`: CPUs + 1, `.buildroot/package/Makefile.in:14-24`). The toolchain is Bootlin's prebuilt
  one (`BR2_TOOLCHAIN_EXTERNAL_BOOTLIN`), ccache is on (`banana_rgds_defconfig:13-14`, 3 GB,
  `~/.buildroot-ccache`, 1.0 GB used, shared between boards through `BR2_CCACHE_USE_BASEDIR`), and downloads are kept
  in `.buildroot/dl` (2.7 GB).
- **Where the time goes** (`output/*/build/build-time.log`). A fresh `rgds` output with a warm cache (26 Oct) took
  15.2 min of wall time. Its package steps took 12.9 min of that: `configure` 331.6 s (single-threaded scripts, and
  ccache can't cache autoconf's tests: 11,783 such calls in `ccache -s`), `build` 380.2 s, the rest extract and
  install. The image stage is cheap: 6 s from the last package to `rootfs.squashfs`, then 7 s to the update tar
  (`post-image.sh:30-40`). The most expensive steps of a warm build are host tools (`host-python-cython` 53.6 s,
  `host-gnutls` 36.9 s, `host-libunistring` 33.1 s, `host-python3` 28.6 s, `host-m4` 21.0 s), `banana-ui` (45.5 s
  then, 90-105 s now), `sqlite` 24.5 s, `bananads` 17.9 s and the kernel (17.8 s with cache hits). `rgds-plus`'s later
  sessions rebuilt 1-16 packages each, with banana-ui the largest (69-106 s) every time.
- **banana-ui's build.** `buildroot/package/banana-ui/banana-ui.mk:14-18` runs `make -B` on `ui/` for banana-ui and its
  four helpers. banana-ui's rule (`ui/Makefile:31-32`) compiles every source in `SRC` in one `$(CXX)` call (76 sources
  and the 39 C objects `kiwami.mk` builds separately, `kiwami.mk:25-28`). Two quirks matter for objects:
  - `src/bcomp.c` and `src/bovl.c` are compiled by `g++`, hence as C++;
  - `src/json.cpp` and `src/kiwami/json.cpp` share a name, so object names have to keep the path.

  The host build (`make ui`: `Makefile:39-45`, `ui/Makefile:69-70`) is the same one call, plus banana-bshare (2 s) and
  banana-fetch (9 s) one after another.
- **Where the fork can act.** The fork's `external.mk` is read after every package's `.mk`
  (`.buildroot/Makefile:550,564`) and already adjusts upstream's packages from there (RetroArch). `kiwami.mk` is
  included by upstream's Makefile before banana-ui's rule. The top Makefile has three `kiwami:` lines (`Makefile:27-29`).
  Upstream changes `ui/Makefile` often (44 commits since 26 Oct, mostly adding to `SRC`) and the top `Makefile`
  rarely (5 commits in all).
- **What's in the image that records the build.** Eight files in `target/` contain the output directory's path:
  - `__FILE__` strings in `sqlite3` and `bananads` (their `build/` directories, which per-package directories don't
    move);
  - Python's `config-3.14-*/Makefile`, `_sysconfigdata*.pyc` and `_sysconfig_vars*.json`, and the toolchain's
    `libstdc++.so.6.0.33-gdb.py` (the sysroot under `host/`, which becomes `per-package/<pkg>/host/`);
  - two of Buildroot's `.files-list-host.before` copied by the shaders' and Poketool's installs.
- **Upstream (`main`)** builds the same way. Nothing in the fork changes the build's speed today.

## What has to change

- `ui/src/kiwami/kiwami.mk`: banana-ui's `SRC` turned into objects, one per source:
  - compiled with exactly the flags of `ui/Makefile:32`'s call, `$(CXX)` for `.c` files too;
  - the objects named after their paths;
  - the C++ objects in their own directory per variant (ALSA on or off), with header dependencies (`-MMD`);
  - linked by upstream's own rule, unchanged.

  `make ui` gets its `-j` when none is given. `kiwami-clean` removes the objects. With the switch, `SRC` is left as it
  is. No line of `ui/Makefile` changes.
- `kiwami/configs/kiwami.fragment`: `BR2_PER_PACKAGE_DIRECTORIES=y`, which `kiwami/configure.sh` leaves out when the
  switch is set.
- `Makefile`: a fourth `kiwami:` hook, on the board rule's `$(MAKE) -C $(OUT)/$(1)` (`Makefile:32`). It adds `-j` when
  none was given and the switch isn't set, and stops when the output's mode doesn't match the switch (oversight 6).
  The board rule is needed because the top-level make is what decides the parallelism.
- `kiwami/external.mk`: the missing dependencies, `libretro-mupen64plus-next`'s `libegl libgles` and any the first
  parallel build reveals, each with its order-only prerequisite. With the switch, banana-ui's build is told to keep its
  one call (a target-specific export, as the fork does for RetroArch's `GIT_VERSION`).
- `kiwami/tests/`, new:
  - the image comparison: two images and update packages in, masked differences listed apart, anything else a failure
    (*Testing*);
  - the dependency audit: each package's `NEEDED` libraries against its declared dependencies, from an output directory;
  - banana-ui's identity check: built both ways and compared byte for byte, on the host compiler, and on the target
    toolchain when an output is given.

  `kiwami/tests/ci.sh` runs the host identity check.
- `docs/FORK.md`: *How the build takes in the fork* says it:
  - per-package directories, the `-j`, the switch, the first cold build after switching;
  - the note on fresh builds;
  - four `kiwami:` lines in the top Makefile.

  *Taking in upstream* gains the dependency audit, and the comparison when packages change.
  `.claude/skills/banana-merge-upstream/SKILL.md` gains the same step.
- `kiwami/DEPENDENCIES.md`: Buildroot's row in the second table names per-package directories (and its top-level
  parallel build) among what the fork relies on. No new external dependency.

## Options

1. **banana-ui from objects plus the parallel package build, one switch (recommended; chosen).** The image is checked
   to be the same by the comparison. Warm full builds should be 2-3x faster, and banana-ui 5x faster, in the image and
   in `make ui`/`make test`. It costs one hook line in the top Makefile, the fork's handling of undeclared dependencies
   (now and after merges), and one cold build after the switch.
2. **banana-ui from objects only.** No upstream line, no dependency work, and the image method is unchanged. Each build
   saves about 80 s (a warm full build: 15-18 min → about 14-16), and `make ui`/`make test` gain as in option 1. The
   fallback if the parallel build disappoints when measured.
3. **The parallel package build without the banana-ui change.** banana-ui's 105 s would end the longest chain
   (3.3 min instead of 3.0), and `make ui` keeps its 95 s. No reason to leave it out: it's the cheaper change and fully
   verified.
4. **Changing what's built** (fewer kernel options, `-O1`, prebuilt host tools, `BR2_REPRODUCIBLE`). Ruled out by the
   request: the image wouldn't be the same.

## Risks

- **Performance and footprint.** On the device, none: the image is the same. On the build machine, per-package
  directories add hardlinked copies (`per-package/`) and two full copies (host 1.7 GB and target 0.5 GB, merged at the
  end) to the 15 GB of each output. A parallel build takes more memory at its peak (several compilers at once; ninja
  packages keep their own `-j17`), which 61 GB covers.
- **Conflicts with upstream.**
  - One new line in the top `Makefile`, which upstream rarely changes.
  - `kiwami.mk` reads `SRC`, and upstream extends it often. A new source is picked up on its own. A change to the
    shape of banana-ui's rule (other flags, a different compile) would make the objects differ, and the identity check
    in `kiwami/tests/ci.sh` catches it.
  - New upstream packages may lack dependencies (oversight 4).
- **Buildroot's "experimental".** Some package could misbehave under per-package directories in a way the dependency
  audit doesn't see (an optional feature dropped, a file two packages install). The comparison is what shows it, so
  the first parallel build is compared before it's trusted, and the switch builds today's way meanwhile.
- **The estimate.** The 5-8 and 25-40 minutes are estimates from the logs, not measurements. If the parallel build
  gains little, option 2 stands alone.
- **Hardware and boards.** Nothing runs differently on the handheld. `rgds` and `universal` share every change; only
  `rgds-plus` is measured and compared, and `kiwami/tests/ci.sh` configures all three.
- **CI's image job** (upstream's `.github/workflows/ci.yml`, manual dispatch only) runs `make <board>` and would build in
  parallel too, from an empty output each time. Out of scope, but it would gain rather than lose.
- **The checklist's other rows**:
  - Sleep and the lid, the card and the network, games and emulators: n/a, nothing on the device changes.
  - Boot, first boot and updates: the update package is checked like the image, and nothing else is involved.
  - Security: nothing listens and no credentials are involved; the build only runs on the build machine.
  - Errors and edge cases: a failed parallel build (the switch), an output in the other mode (stopped with a message),
    a missing tool for the comparison (it says which).

## Testing

- **Headless** (`kiwami/tests/`):
  - banana-ui's identity check, built both ways and compared byte for byte. On the host compiler it runs in
    `kiwami/tests/ci.sh` (about 2 min, mostly the one-call reference). On the target toolchain it runs on demand,
    against an output.
  - The comparison script's own check: fed two builds' images, it lists the known noise and fails on a planted
    difference.
  - `kiwami/tests/ci.sh` configures every board with the fragment and checks that `BR2_PER_PACKAGE_DIRECTORIES`
    survives.
- **Builds, run by the user on the build machine** (logs: each output's `build/build-time.log`, the comparison's
  report, `ccache -s`):
  1. A baseline pair: two fresh `rgds-plus` builds today's way, from the same commit, with the switch, in two output
     directories, warm cache (about 35 min). The comparison must find only the known noise.
  2. The new way, fresh, after the switch (its first build is cold for ccache, oversight 6): compared with the
     baseline.
  3. Warm, both ways: a fresh build of each with the cache from the run before. Wall time from the start of `make` to
     the update tar.
  4. Cold, both ways: a fresh build of each with a throwaway `BR2_CCACHE_DIR` (about 1 h today's way, 25-40 min
     estimated).
  5. `rgds` and `universal`, the new way: they build.

  The numbers go in the plan's manual test table. The handheld needs nothing beyond what the comparison shows; booting
  the new image once on the RG DS Plus is a final sanity check.

## Open questions

1. **Which builds are slow?** *Answered*: full builds of a fresh output, with a warm and with a cold cache.
2. **How strict is "the same"?** *Answered*: the same contents, apart from what already differs between two of today's
   builds; checked by a comparison script.
3. **Which boards?** *Answered*: `rgds-plus` mostly; the others take the same changes and must still build.
4. **Which build paths?** *Answered*: local image builds, and the host `make ui`/`make test`; not CI's image job.
5. **Which speed-ups?** *Answered*: banana-ui from objects and the parallel package build; not banana-fetch's objects.
6. **Build paths in Python's configuration and libstdc++'s gdb helper under per-package directories?** *Answered*:
   allowed as noise, listed separately by the comparison.
7. **How does a build get its `-j`?** *Answered*: the top Makefile adds it (one `kiwami:` hook).
8. **What to measure?** *Answered*: warm and cold, both ways.
9. **An off switch?** *Answered*: yes, one, building today's way from the same commit.
10. **Merges?** *Answered*: a dependency-audit step in `docs/FORK.md`'s after-merge checks and in the merge skill.
11. **Extras?** *Answered*: the note on fresh builds is in; the texts for upstream stay a proposal.
