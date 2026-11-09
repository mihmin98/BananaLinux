# 0026 Build speed: plan

Issue: #26 · Branch: `k/0026-build-speed` · Investigation: [investigation.md](investigation.md) · Status: approved

## Summary

Full image builds get faster, and the image and update package keep their contents. Two changes:

- **Buildroot's top-level parallel build.** `BR2_PER_PACKAGE_DIRECTORIES=y` in the fork's fragment. The top Makefile's
  board rule runs `kiwami/build.sh`, which adds `-j$(nproc)` when none was given and groups the output per step.
- **banana-ui compiled one file per call.** `ui/src/kiwami/kiwami.mk` turns upstream's `SRC` into objects, linked by
  upstream's own rule in the same order. They build in parallel and ccache keeps each one, and the binary is byte for
  byte the one upstream's single `g++` call makes.

`KIWAMI_SERIAL=1`, given when an output is configured, builds that output as upstream does. A comparison tool checks
that two builds' images and update packages have the same contents, apart from the known noise. A dependency audit
finds what packages link without declaring it. Missing dependencies are added in `kiwami/external.mk`, under
per-package directories only.

### Refinements over the investigation

- **The image's banana-ui follows the output's mode, not the command line.** Buildroot clears `MAKEOVERRIDES`
  (`.buildroot/Makefile:157`). So inside the image build the switch is read from the output's `.config`:
  `kiwami/external.mk` exports `KIWAMI_SERIAL=1` to banana-ui's build step when per-package directories are off. On the
  host, `make ui` takes `KIWAMI_SERIAL=1` from the command line.
- **An output's mode is fixed when it's configured.** `kiwami/configure.sh` leaves per-package directories out when
  `KIWAMI_SERIAL=1`. `kiwami/build.sh` refuses a build in two cases, and says what to run:
  - the switch disagrees with the output's `.config`;
  - the output holds a build made the other way (`build/` without `per-package/`, or the reverse).

  `KIWAMI_SERIAL` is `1` or unset; any other value is an error.
- **Compared builds are made in one directory, reused: `OUT=$PWD/output/cmp`.** `sqlite3` and `bananads` embed
  their build directory (`__FILE__`), and ccache shares objects across outputs. Builds made in the same path embed the
  same one. A path of another length would shift everything after it in the file.
  - The comparison reads only a build's image and update package (about 0.4 GB), so each build's two files are saved
    (`image-compare.sh --save`) before the next build replaces it.
  - The audit and the target identity check need the whole output, and run right after their build.
- **The investigation's audit read a muddied file list.** In the incremental `output/rgds-plus`,
  `packages-file-list.txt` attributes `usr/bin/banana-ui` and `usr/bin/bananads` to `ldacbt`. The direct check of
  banana-ui's binaries confirmed their libraries are declared (`freetype libdrm alsa-lib zlib libcurl openssl libssh2
  libsmb2`). The audit (step 2) runs on a fresh output and reports files attributed to more than one package.
- **U-Boot's noise is more than its dates.** Between the `rgds` and `rgds-plus` builds of 26 Oct (the same U-Boot
  configuration), `u-boot-rockchip.bin` differs in 104 bytes at six places:
  - the SPL's and U-Boot's date strings (offsets 183062 and 9128877);
  - the FIT's `timestamp` (8355919);
  - three SHA-256 digests: the Rockchip loader header's entry 1 (232) and its own hash (1536), and the FIT's hash of
    U-Boot proper (8356261).

  The comparison masks the digests where the structures put them. The content they hash is compared with only the
  dates masked, so a real change still shows.
- **Your shell aliases `make` to `make -j16`.** Your builds already run with a top-level jobserver, and only
  `.NOTPARALLEL` keeps packages one at a time. `kiwami/build.sh` adds `-j` only to a plain `make` (CI, another shell).
  The board rule's hook starts with `+`, so the jobserver reaches `build.sh`'s make on make 4.3's pipe jobserver too
  (CI's Ubuntu; 4.4's fifo works either way, tested).
- **`make clean-<board> && make <board>` are two commands.** With `-j16`, `make clean-x x` would run both goals at once.

### Decided while planning

- **banana-ui's identity check runs in full in `kiwami/tests/ci.sh`**: both builds, byte for byte, on every run (about
  2.5 min). An upstream change to banana-ui's rule that breaks identity is caught on the next push.
- **A step that needs a real build is committed after that build's check passed**: step 1 after the baseline pair,
  so the masks are right when committed; steps 2-4 after theirs.
- **The parallel build's output is grouped per step** (`--output-sync=target`).
- **One comparison build directory, and saved files**: the comparison builds take at most about 20 GB beside your own
  outputs (one build, the saved files, the cold runs' cache), not four outputs side by side.

## Acceptance criteria

- **A1**: given a fresh `rgds-plus` output and no `KIWAMI_SERIAL`, when `make rgds-plus` runs, then:
  - `.config` has `BR2_PER_PACKAGE_DIRECTORIES=y` and `output/rgds-plus/per-package/` is made;
  - `build.sh` prints that it builds with per-package directories, adding `-j$(nproc)` when no `-j` was given and
    always `--output-sync=target`;
  - the image and the update package are made.
- **A2**: given a new-way build and a `KIWAMI_SERIAL=1` (today's way) build of the same commit, both built in
  `output/cmp/rgds-plus` (one of them saved with `--save` before the other replaced it), when `kiwami/tests/image-compare.sh` compares them, then it exits 0. Its report holds only the
  known noise, each class with the files it excused:
  - build dates (the kernel, U-Boot, BusyBox, RetroArch, VICE, libpython, OpenSSL, PCSX-ReARMed);
  - random IDs (GPT GUIDs and CRCs, the FAT's volume ID);
  - times (squashfs, FAT, tar, the `.pyc` files' source mtimes, the kernel's initramfs);
  - build paths (Python's configuration, libstdc++'s gdb helper, `__FILE__` strings; ELF files linked around a longer
    path, which per-package directories make: libtool's RPATH to the staging, an include path in `.rodata`);
  - Buildroot's bookkeeping files;
  - derived (U-Boot's hashes, the kernel's build ID, the squashfs's size).
- **A3**: given a cache warmed by a build of the same commit, the wall time of a fresh `rgds-plus` build the new way is
  lower than today's way. Both are recorded.
- **A4**: given an empty throwaway ccache (`BR2_CCACHE_DIR`), the same holds for a cold build. Both are recorded, and
  `~/.buildroot-ccache` is left as it was.
- **A5**: given a fresh output, when `make KIWAMI_SERIAL=1 OUT=<dir> rgds-plus` runs, then it builds today's way:
  - `.config` has no `BR2_PER_PACKAGE_DIRECTORIES=y`, and there is no `per-package/`;
  - no `-j` or `--output-sync` is added;
  - banana-ui is built in one `g++` call (no `src/kiwami/obj/` in its build directory).

  Its image compares equal to a new-way build (A2).
- **A6**: given an output whose `.config` or build is in the other mode, or `KIWAMI_SERIAL` set to anything but `1`,
  when `make <board>` runs, then `build.sh` stops before building, with a message naming what to remove or run. Today's
  `output/rgds-plus`, built before this feature, stops this way, with `make clean-rgds-plus`.
- **A7**: banana-ui built from objects is byte-identical to the one `g++` call, unstripped and stripped:
  - on the host compiler (`ALSA=0`, as `make ui`);
  - on the output's target toolchain with `banana-ui.mk`'s flags and Buildroot's strip.
- **A8**: on the host, `make ui` builds banana-ui from objects in parallel with no `-j` given. Then:
  - changing one `.cpp` recompiles only its object;
  - changing a header recompiles the objects that include it;
  - changing the flags recompiles every object;
  - `make -C ui clean` removes `ui/src/kiwami/obj/`;
  - `KIWAMI_SERIAL=1` builds the one call;
  - `make test` passes, and its renders are identical to the branch base's.
- **A9**: given a fresh output, when `kiwami/tests/deps-audit.py` audits it, then it lists each target package (since
  step 3 Buildroot's own too, each named with its tree) that links a library from a package outside its dependencies.
  On today's `rgds-plus` that is upstream's `libretro-mupen64plus-next` (`libEGL.so.1`, `libGLESv2.so.2`), and
  Buildroot's `sqlite` (`libz.so.1`). With the fork's fix (A1's output), nothing; `drastic` is allowed (prebuilt, it
  links nothing at build time). Files attributed to more than one package are listed apart.
- **A10**: `rgds` and `universal` configure with the fork's options (`kiwami/tests/ci.sh`) and build the new way.
- **A11**: the comparison fails on a real difference. Compared with today's incremental `output/rgds-plus`, it reports
  the stale Poketool, PortMaster and `romm` files. In its self-test, every planted difference is reported and every
  noise class excused.
- **A12**: the image built the new way boots on the RG DS Plus to the menu, and a game starts.

## Risks

- **Performance and footprint.** On the device, none: the image's contents are checked to be the same (A2), so frame
  time, idle activity, rootfs size, memory and boot time don't change. On the build machine:
  - each output grows by `per-package/` (hardlinks) and two merged copies (host 1.7 GB, target 0.5 GB);
  - the comparison builds use one output, `output/cmp/rgds-plus` (about 15-17 GB), each build's image and update
    package saved in `output/cmp-saved/` (about 0.4 GB each), and the cold runs' cache, `output/cmp-ccache` (1-2 GB):
    about 20 GB at most beside your own outputs (104 GB are free);
  - a parallel build peaks higher in memory, which 61 GB covers;
  - `kiwami/tests/ci.sh` takes about 2.5 min longer (the identity check's one-call reference build): you chose that.
- **Conflicts with upstream.**
  - One hook line in the top `Makefile` (5 upstream commits in all), on the board rule's recipe.
  - `kiwami.mk` rewrites `SRC`, which upstream's `ui/Makefile` changes often (44 commits since 26 Oct). A new source
    is picked up on its own. A source added to `SRC` after line 20 stays in the one call, and the link is still
    identical. A change to the shape of banana-ui's rule makes `ui-objects.sh` fail in CI.
  - New upstream packages may lack dependencies: the after-merge step (`docs/FORK.md`, the merge skill) runs the audit
    and a fresh build.
- **Buildroot's "experimental" option.** A package can change silently under per-package directories (an optional
  feature dropped, a file two packages install merged in another order). Only the comparison shows that. It runs on
  the first parallel build (step 3) and again at the end, and the switch builds today's way meanwhile.
- **The comparison's masks hiding a real change.** A mask excuses only bytes that differ, and only inside a span its
  pattern matches at the same place in both files. Every excused file is listed, and the self-test plants a difference
  inside and outside each mask. The baseline pair (step 1) shows the masks are complete before the tool judges a
  change.
- **The estimates.** 5-8 min warm and 25-40 min cold are estimates; A3 and A4 record the measurements. If the parallel
  build gains little, steps 3 and 4 still stand apart (step 4 needs nothing of step 3 but the mode).
- **`--output-sync=target`.** A long step's output shows when it ends, so the build can look stalled. Recursive makes
  (the kernel, packages' own makes) still print as they go, a target at a time. Documented in `docs/FORK.md`.
- **Hardware and boards.** Nothing runs differently on the handheld. `rgds` and `universal` take every change: they
  configure in `ci.sh` and are built once (M8). Only `rgds-plus` is compared and booted (only the RG DS Plus is at
  hand).

## Steps

Each step is one commit, and leaves the tree building and `make test` passing. A step that needs a real image build
(steps 1-4) is committed only once that build's check has passed: you run the builds, and the outcomes go in *Manual
tests*.

### 1. The image comparison

- **Files**: `kiwami/tests/image-compare.sh` (new), `kiwami/tests/image-compare.py` (new), `kiwami/tests/ci.sh`
  (changed), `CLAUDE.md` (changed).
- **Change**:
  - `image-compare.sh <old> <new>`, each an output directory or a folder saved from one, e.g.
    `output/cmp-saved/base output/cmp/rgds-plus`:
    - In an output it finds `images/banana-<board>.img.gz` (exactly one) and
      `images/banana-<board>-<version>.update.tar`, the version being the output's `target/etc/banana/version`. In a
      saved folder it finds the same two files (exactly one of each), and `origin`.
    - `image-compare.sh --save <output dir> <folder>` copies those two files into the folder, with `origin`: the
      output's absolute path. It refuses a folder that isn't empty.
    - The two builds' paths (an output's own, a folder's `origin`) are compared. If they differ in length, it warns
      that binaries embedding them can't be compared, and their differences then fail.
    - The tools are the `host/bin` of the first output among the arguments, the second preferred (`unsquashfs`,
      `lz4`, `mcopy`, `mdir`, and its `python3`, run as `python3 -I kiwami/tests/image-compare.py`). That Python is
      the image's own version, so it can read the image's `.pyc` files. Two saved folders need `KIWAMI_TOOLS=<an
      output>/host/bin`. A missing tool is named.
    - Works in `mktemp -d` under `${TMPDIR:-/tmp}` (about 2.5 GB: two images and two root filesystems), removed on
      exit.
  - `image-compare.py`, its parts:
    - **The disk image.** Both `.img.gz` are decompressed, and must be the same size. The protective MBR is compared
      byte for byte. The GPT, primary and backup, field by field: signature, revision, sizes, LBAs, the number and
      size of entries; per partition its type GUID, first and last LBA, attributes and name. Ignored: the disk and
      partition GUIDs and both CRC32s (genimage draws the GUIDs at random, `host-genimage-19/image-hd.c:868,1124`).
      Every byte outside the GPT and the partitions is compared under the U-Boot rule.
    - **U-Boot** (from 32 KiB up to the first partition). It is split structurally:
      - The Rockchip loader header (`RKNS`, v2) at the start: each image entry's 64-byte hash and the header's own
        hash (at 1536) are masked. The offsets are confirmed against `tools/rkcommon.c` in the output's
        `build/uboot-custom/`.
      - The FIT, found by its FDT magic: a small FDT walker in Python (header, struct tokens, the strings block)
        masks `/timestamp` and every `value` under a `hash*` node.
      - The rest (the SPL, U-Boot proper, the FIT's external data) is compared under the build-date rule.
    - **The boot partition** (FAT): extracted with `mcopy -s`, listed with `mdir`, and its label read. Compared: the
      label, every file's name, size and content. Ignored: the volume ID and the files' times. `a/Image` is LZ4
      (`lz4 -d`) and is compared under the kernel rule. The device trees and `extlinux.conf` are byte for byte.
    - **The root partitions.**
      - `root-a`'s squashfs: `unsquashfs -s` (all but its time lines) and `unsquashfs -lln` (each entry's type,
        mode, uid/gid, path and symlink target; sizes too, except for Buildroot's bookkeeping files) are compared,
        then both are extracted (`-no-xattrs`: the image has none) and each regular file compared under the file
        rules below.
      - `root-b` must be equal in both (zeros).
    - **The update package**, with Python's `tarfile`:
      - each member's name, type and mode;
      - `VERSION`, `board` and the `.dtb` files byte for byte;
      - `Image` (LZ4) under the kernel rule;
      - `rootfs.squashfs` byte-equal to the same side's `root-a` (already compared);
      - `sha256sums`: the same names, and each side's sums matching its own files.
  - **The file rules.**
    - **Mask by difference.** Equal-length files are compared byte by byte. Each run of differing bytes must lie
      inside a span that one rule's pattern matches at the same offset in both files. The file is then listed under
      that rule's class. A run no rule covers is a difference.
    - **Build dates**, in ELF files, the kernel and U-Boot:
      - `date`'s format (`Mon Nov  2 15:18:16 EET 2026`, the kernel's `UTS_VERSION`);
      - U-Boot's (`(Oct 26 2026 - 19:02:12 +0200)`);
      - BusyBox's (`(2026-10-26 18:54:36 EET)`);
      - `__DATE__` (`Nov  2 2026`), and `__TIME__` as a NUL-terminated `HH:MM:SS`.
    - **Build paths.** An output's path is `<anything>/<board>/` followed by `build/`, `host/`, `per-package/`,
      `target/`, `staging/` or `images/`.
      - Text files (no NUL) and `.pyc`: both are normalised (that path to `<OUT>/`, `per-package/<pkg>/` dropped)
        and compared. A `.pyc` is unmarshalled, its string constants normalised recursively and re-marshalled.
      - Other files: mask by difference, paths of equal length.
    - **Buildroot's bookkeeping**: `.files-list*.before`, `.files-list*.txt`, `.stamp_*`, `.applied_patches_list`,
      anywhere. Contents ignored; presence and mode compared.
    - Anything else must be byte-equal.
  - **The report**: one line per part (`GPT: same`, `U-Boot: same (build dates, FIT timestamp, digests)`, ...), then
    per class the files it excused. Then `DIFFERENT:` with each file and its first differing offset, or `same
    contents`. Exit 0 only with no differences.
  - `--self-test`: synthetic data, no external tools, the running Python (3.12 on CI):
    - two kernel banners differing in the date: same, build dates; another byte changed as well: different;
    - the same for BusyBox's and `__DATE__`'s formats;
    - a synthetic FIT (an FDT built in the test) with `/timestamp` and a hash `value`, both changed: same; a change
      in an image's data: different;
    - a text file with `/x/output/cmp-a/rgds-plus/host/` against `/x/output/cmp-b/rgds-plus/per-package/python3/host/`:
      same, build paths;
    - a binary with paths of equal length: same; of different length: different, with the hint;
    - two `.pyc` compiled from modules whose constants differ only in such a path: same; in another constant:
      different;
    - two GPT headers differing in GUIDs and CRCs: same; in a partition's last LBA: different;
    - two `unsquashfs -lln` listings differing in times: same; in a mode: different;
    - a bookkeeping file's contents changed: same; a bookkeeping file missing on one side: different.
  - `kiwami/tests/ci.sh`: `python3 -I kiwami/tests/image-compare.py --self-test`, after the units.
  - `CLAUDE.md`, *Commands*: two lines, `kiwami/tests/image-compare.sh <old> <new>` (outputs or saved folders) and
    `kiwami/tests/image-compare.sh --save <output> <folder>`.
- **Tests**:
  - `python3 -I kiwami/tests/image-compare.py --self-test`, `kiwami/tests/ci.sh .buildroot`, `make test`.
  - Before the commit, M1: the baseline pair today's way, compared (exit 0, only the known noise), and compared with
    today's incremental `output/rgds-plus` (fails on Poketool, PortMaster and `romm`). A mask the baseline shows
    missing is added here, with its self-test case.
- **Commit**: `Build speed: a comparison of two builds' images and update packages`

### 2. The dependency audit

- **Files**: `kiwami/tests/deps-audit.py` (new), `kiwami/tests/ci.sh` (changed).
- **Change**:
  - `deps-audit.py <output dir> [--info <show-info.json>]`:
    - Buildroot's graph comes from `make -s -C <output dir> show-info`, unless `--info` is given. It already holds the
      late `+=` dependencies (bluez-alsa's `ldacbt` is there).
    - The packages audited are the folders of `buildroot/package/` and `kiwami/package/` that are enabled.
    - For each, its files in `build/packages-file-list.txt` that are ELF files in `target/` give its `NEEDED`
      libraries (`readelf -d`).
    - Each library's provider is the package whose file list holds a path with that name (the toolchain's are
      `toolchain-external-bootlin`).
    - Without per-package directories, a file in two lists is the other package's when that one's install ended
      inside this one's window, from its configure step to the end of its install (`build/build-time.log`). Buildroot
      takes a package's list over that window, and ldacbt's holds what 33 packages installed in it (*Deviations*).
    - A provider outside the package's dependency closure (recursive, through virtual packages: `libegl` →
      `mesa3d-panfrost`) is reported as `<package>: <library> from <provider>, not among its dependencies`.
    - Allowed, with the reason printed: `drastic` (a prebuilt binary: nothing is linked at build time).
    - A library no package provides is reported apart.
    - Target files attributed to more than one package are listed apart, with a count. In an incremental output that
      list is long, and the audit says to use a fresh output: it knows one by a package installed more than once
      (`build-time.log`) or the list of a package no longer built. In a fresh one, it is the files whose merge order
      changes under per-package directories (the investigation's oversight 5).
    - Exit 1 when a package is reported.
  - `--self-test`, with the host's `cc` and `readelf`, in `mktemp -d`:
    - a `libfoo.so.1` owned by package `foo`, and a binary `bar` linking it;
    - `bar` without `foo` among its dependencies: reported;
    - with `foo`, or through a virtual package that depends on `foo`: not reported;
    - `bar` allowed: not reported;
    - a file in two packages' lists: listed apart;
    - a package of Buildroot's own: not audited; a library no package provides: listed apart, not reported;
    - a package installed twice, or the list of one no longer built: not a fresh output;
    - one package at a time, a list holding what others installed in its window: theirs; with per-package
      directories, or installed before the window: its own, reported and listed apart.
  - `kiwami/tests/ci.sh`: `python3 -I kiwami/tests/deps-audit.py --self-test`, when `cc` and `readelf` are there.
- **Tests**: the self-test, `kiwami/tests/ci.sh .buildroot`, `make test`. Before the commit, M2: the audit on M1's
  second build, still in `output/cmp/rgds-plus` (a fresh output), reports `libretro-mupen64plus-next` (`libEGL.so.1`, `libGLESv2.so.2`) and whatever
  else it finds, which goes into step 3's fixes.
- **Commit**: `Build speed: an audit of the libraries packages link against what they declare`

### 3. Packages built in parallel, in per-package directories

- **Files**: `kiwami/configs/kiwami.fragment` (changed), `kiwami/configure.sh` (changed), `kiwami/build.sh` (new),
  `Makefile` (hook), `kiwami/external.mk` (changed), `kiwami/tests/build-mode.sh` (new), `kiwami/tests/ci.sh`
  (changed), `docs/FORK.md` (changed), `.claude/skills/banana-merge-upstream/SKILL.md` (changed),
  `kiwami/DEPENDENCIES.md` (changed), `CLAUDE.md` (changed); since M3 (*Deviations*), `kiwami/tests/image-compare.py`
  and `kiwami/tests/deps-audit.py` (changed).
- **Change**:
  - `kiwami.fragment`: `BR2_PER_PACKAGE_DIRECTORIES=y`, under a comment. Each package builds in its own copy of what
    it depends on, so independent ones build at once. The top Makefile's board rule runs `kiwami/build.sh`, which adds
    the `-j`. `KIWAMI_SERIAL=1`, given when an output is configured, leaves it out.
  - `kiwami/configure.sh`: `KIWAMI_SERIAL` from the environment (make's command line puts it there, tested) must be
    empty or `1`, or it stops. With `1`, the fragment's `BR2_PER_PACKAGE_DIRECTORIES=` line is dropped from `OPTS`
    (`configure.sh:29`) before options are appended and checked, and it prints `configure.sh: <output>: without
    per-package directories (KIWAMI_SERIAL=1)`.
  - `kiwami/build.sh <output dir> <make command...>`:
    - **The mode**: `ppd` when `.config` has `BR2_PER_PACKAGE_DIRECTORIES=y`; `serial` when `KIWAMI_SERIAL=1`; `built`
      when any `build/*/.stamp_configured` exists; `perpkg` when `per-package/` exists. `KIWAMI_SERIAL` set to anything
      else stops.
    - **It stops (exit 1, nothing run)**, with these messages:
      - `serial` and `ppd`: `<out> is configured with per-package directories; KIWAMI_SERIAL=1 applies when an
        output is configured: build it without, or rm <out>/.config to configure it again`;
      - neither: the same the other way round (`... configured without them (KIWAMI_SERIAL=1): build it with
        KIWAMI_SERIAL=1 ...`);
      - `ppd`, `built`, no `perpkg`: `<out> holds a build made without per-package directories: make clean-<board>,
        then make <board>`;
      - no `ppd`, with `perpkg`: the same the other way round.
    - **The flags.** With `ppd`: `-j$(nproc)` unless `$MAKEFLAGS` already has `-j` or `jobserver`, and
      `--output-sync=target` unless it has `output-sync` or ` -O`.
    - **It prints** one line (`build.sh: <out>: per-package directories, -j16, output per step`, or `... as upstream
      (KIWAMI_SERIAL=1)`), then `exec "$@" <flags> -C <out>`.
  - `Makefile:32`, the hook: `$(MAKE) -C $(OUT)/$(1)` becomes
    `+kiwami/build.sh $(OUT)/$(1) $(MAKE)	# kiwami: in parallel, per-package directories (KIWAMI_SERIAL=1: as upstream)`.
    The `+` marks it as a recursive make for make 4.3's jobserver. The `%-$(1)` rule (`Makefile:34-35`) is left alone:
    `make <target>-<board>` runs in the output's own mode.
  - `kiwami/external.mk`, a block under `ifeq ($(BR2_PER_PACKAGE_DIRECTORIES),y)`, so `KIWAMI_SERIAL=1` builds exactly
    as upstream. What upstream's packages link without declaring it:
    ```make
    # mupen64plus-next links libEGL and libGLESv2 (FORCE_GLES3=1)
    LIBRETRO_MUPEN64PLUS_NEXT_DEPENDENCIES += libegl libgles
    $(LIBRETRO_MUPEN64PLUS_NEXT_TARGET_CONFIGURE): | libegl libgles
    ```
    The `+=` reaches the per-package copy, which reads `_FINAL_DEPENDENCIES` when the step runs
    (`.buildroot/package/pkg-generic.mk:256,804`). The order-only prerequisite orders the build, as upstream does for
    bluez-alsa's `ldacbt` (`buildroot/external.mk:55-61`). It also holds whatever M2 and M3 find: M3 found Buildroot's
    `sqlite` (`zlib`) and `openal` (`dbus`), each under its packages' options (*Deviations*).
  - Since M3 (*Deviations*): `kiwami/tests/image-compare.py` accepts an AArch64 ELF file linked around a build path
    of another length (`relinked()`), with its self-test's synthetic ELF; `kiwami/tests/deps-audit.py` audits every
    target package and names its tree.
  - `kiwami/tests/build-mode.sh`: `build.sh` against made-up outputs (a `.config`, `build/x/.stamp_configured`,
    `per-package/`), with a stand-in make that prints its arguments:
    - `ppd` and no `MAKEFLAGS`: `-j<nproc> --output-sync=target -C <out>`;
    - with `MAKEFLAGS='s -j4 --jobserver-auth=fifo:/x'`: `--output-sync=target -C <out>`;
    - serial `.config` with `KIWAMI_SERIAL=1`: `-C <out>`;
    - each of the four stops: exit 1, its message (`rm <out>/.config` or `make clean-<board>`), and the stand-in not
      run;
    - `KIWAMI_SERIAL=yes`: stops.
  - `kiwami/tests/ci.sh`: runs `build-mode.sh`. After the boards' loop, which now also checks the fragment's
    `BR2_PER_PACKAGE_DIRECTORIES=y` survives, it configures `rgds_plus` once with `KIWAMI_SERIAL=1` and checks its
    `.config` has no `BR2_PER_PACKAGE_DIRECTORIES=y`.
  - `docs/FORK.md`:
    - *How the build takes in the fork*: four `kiwami:` lines; a paragraph on per-package directories (`build.sh`,
      the `-j` and the grouped output, the dependencies in `kiwami/external.mk`, `KIWAMI_SERIAL=1` and the modes,
      switching is a fresh output and the first build after it is cold for ccache);
    - the fresh-build note: `make clean-<board>`, then `make <board>`, two commands; an incremental build keeps the
      files of packages that have gone, as 0.5.3's Poketool and PortMaster stayed in an older output;
    - the comparison, for a change to how the image is built;
    - *Taking in upstream*, item 1: when upstream adds or changes a package, a fresh `rgds-plus` build and
      `kiwami/tests/deps-audit.py output/rgds-plus`. A missing dependency goes in `kiwami/external.mk`'s block, and
      `KIWAMI_SERIAL=1` builds meanwhile.
  - The merge skill:
    - its *Build configuration* row: a package upstream adds or changes, its declared dependencies against what it
      links (per-package directories);
    - step 8 (*Check*): the fresh build and the audit when packages were added or changed, run by you.
  - `kiwami/DEPENDENCIES.md`, Buildroot's row: the fork relies on per-package directories (`BR2_PER_PACKAGE_DIRECTORIES`,
    experimental in 2026.02.3) and the top-level parallel build they allow (`kiwami.fragment`, `kiwami/build.sh`).
  - `CLAUDE.md`, *Commands*: the `make rgds | rgds-plus | universal` line gains "packages in parallel; KIWAMI_SERIAL=1
    on a fresh output builds as upstream".
- **Tests**:
  - `kiwami/tests/build-mode.sh`, `kiwami/tests/ci.sh .buildroot`, `make test`, `sh -n kiwami/build.sh`.
  - Before the commit, M3: the first parallel build of `rgds-plus`, fresh, until it completes (each missing dependency
    it shows added to the block); then the audit on it (nothing reported) and the comparison with the saved `base` (exit 0);
    and `make rgds-plus` on today's `output/rgds-plus` stopping with `make clean-rgds-plus`.
- **Commit**: `Build speed: packages built in parallel, in per-package directories`

### 4. banana-ui compiled one file per call

- **Files**: `ui/src/kiwami/kiwami.mk` (changed), `ui/src/kiwami/.gitignore` (new), `kiwami/external.mk` (changed),
  `kiwami/tests/ui-objects.sh` (new), `kiwami/tests/ci.sh` (changed), `docs/FORK.md` (changed),
  `.claude/skills/banana-merge-upstream/SKILL.md` (changed).
- **Change**:
  - `kiwami.mk`, a block after `SRC += $(KIWAMI_O)` (`kiwami.mk:28`), unless `KIWAMI_SERIAL=1`:
    ```make
    KIWAMI_OBJ = src/kiwami/obj
    KIWAMI_UI_SRC := $(filter %.cpp %.c,$(SRC))
    SRC := $(foreach s,$(SRC),$(if $(filter %.cpp %.c,$(s)),$(KIWAMI_OBJ)/$(s).o,$(s)))
    # compiled as banana-ui's rule compiles them (ui/Makefile:32): the same command, one file and -c
    $(KIWAMI_OBJ)/%.cpp.o: %.cpp $(KIWAMI_OBJ)/flags
    	@mkdir -p $(@D)
    	$(CXX) $(CXXFLAGS) $(UI_FLAGS) $(LDFLAGS) -MMD -MP -c -o $@ $<
    $(KIWAMI_OBJ)/%.c.o: %.c $(KIWAMI_OBJ)/flags      # (g++ compiles a .c as C++, as the one call does)
    	(the same recipe)
    $(KIWAMI_OBJ)/flags: FORCE                         # rewritten only when the command changes
    	@mkdir -p $(@D); echo '$(CXX) $(CXXFLAGS) $(UI_FLAGS) $(LDFLAGS)' | cmp -s - $@ || echo '...' > $@
    -include $(KIWAMI_UI_SRC:%=$(KIWAMI_OBJ)/%.d)
    ```
    The details:
    - The objects keep their paths (`src/json.cpp` and `src/kiwami/json.cpp` share a name).
    - `$(UI_FLAGS)` is defined after the include (`ui/Makefile:21-29`). Recipes expand late, so it's the rule's own.
    - `-MMD -MP` writes the header dependencies (it doesn't change the object, checked by A7).
    - Upstream's rule (`ui/Makefile:31-32`) then links `$(SRC)` (objects in its order, the vendored libraries'
      `.kiwami.o` where they were) with its own command.
    - Without a `-j` (`-j` or `jobserver` in `$(MAKEFLAGS)`), `MAKEFLAGS += -j$(shell nproc)`. make 4.4 honours it in
      the same make (tested); 4.3 (CI) in `make ui`'s sub-make (`ui/Makefile:69-70`).
    - `kiwami-clean` also removes `$(KIWAMI_OBJ)`, and the file's header comment says the C++ objects are built here too.
  - `ui/src/kiwami/.gitignore`: `/obj/` (the fork's own file; no line in upstream's `.gitignore`).
  - `kiwami/external.mk`: in an output without per-package directories, banana-ui's build step gets
    `KIWAMI_SERIAL=1`, so the image's banana-ui is built as upstream builds it:
    ```make
    ifneq ($(BR2_PER_PACKAGE_DIRECTORIES),y)
    $(BANANA_UI_TARGET_BUILD): private export KIWAMI_SERIAL = 1
    endif
    ```
    (The pattern of `$(RETROARCH_TARGET_BUILD): private export GIT_VERSION`.)
  - `kiwami/tests/ui-objects.sh [<output dir>]`. It builds banana-ui twice in a copy of `ui/` under `mktemp -d`
    (`make clean` first): with `KIWAMI_SERIAL=1` (it checks no `src/kiwami/obj/` was made) and from objects (it checks
    they were). Then it `cmp`s the two binaries, unstripped and stripped.
    - **Host**, without an argument: the host's `g++`, `ALSA=0`, `strip`. Then, on the objects build:
      - one `.cpp` touched: only its object is newer;
      - `src/kiwami/qrcodegen.hpp` touched: the objects whose `.d` names it are newer, and one whose `.d` doesn't is
        not;
      - `CXXFLAGS='-O2 -DKIWAMI_FLAGS_TEST'`: every object is newer;
      - `make -n -p banana-ui` without `-j` shows `-j` in `MAKEFLAGS`;
      - `make clean`: `src/kiwami/obj/` is gone.
    - **Target**, with an output: `TARGET_CXX`, `PKG_CONFIG_HOST_BINARY`, `TARGET_CXXFLAGS` with `-O2`,
      `TARGET_LDFLAGS` and `TARGET_MAKE_ENV`'s `PATH` (from `make -s -C <out> printvars ... QUOTED_VARS=YES`), with
      `-B`, as `banana-ui.mk:14-18` builds it. Stripped with `<TARGET_CROSS>strip --remove-section=.comment
      --remove-section=.note` (Buildroot's `STRIPCMD`).
  - `kiwami/tests/ci.sh`: `kiwami/tests/ui-objects.sh` when `pkg-config --exists freetype2 libdrm zlib` (as `make ui`
    needs), else `ui-objects: skipped`.
  - `docs/FORK.md`:
    - *How the build takes in the fork*: banana-ui from objects (`kiwami.mk`, `src/kiwami/obj/`, upstream's rule
      linking them, byte-identical and checked in `ci.sh`, `KIWAMI_SERIAL=1` for the one call, in the image following
      the output's mode);
    - *Where a customization goes*, the banana-ui row: `kiwami.mk` also compiles upstream's sources one file per
      call;
    - *Taking in upstream*, item 3: `ui/Makefile` added to the `git diff` list, and a sentence that a change to
      banana-ui's rule's command is followed in `kiwami.mk`, `ui-objects.sh` failing when they no longer make the same
      binary.
  - The merge skill, its *Build-time edits* row: banana-ui's rule in `ui/Makefile`, which `kiwami.mk` compiles one
    file per call.
- **Tests**:
  - `kiwami/tests/ui-objects.sh`, `make test`, `make -C ui` (with ALSA), `kiwami/tests/ci.sh .buildroot`.
  - Before the commit, M4: `kiwami/tests/ui-objects.sh output/cmp/rgds-plus` (M3's build, its target toolchain:
    identical); then a fresh parallel build in its place, compared with the saved `base` (exit 0), whose
    `build/banana-ui-local/` has `src/kiwami/obj/`.
- **Commit**: `Build speed: banana-ui compiled one file per call`

### 5. Final check

- `make test`, `make -C ui`, `make -C system/bananad CFLAGS="-O2 -Werror"`, `kiwami/tests/ci.sh .buildroot`.
- The fork's tests in `kiwami/tests/`: `image-compare.py --self-test`, `deps-audit.py --self-test`,
  `build-mode.sh`, `ui-objects.sh`.
- The hooks: `git diff --stat main...HEAD` and `git grep -n 'kiwami:'` (the commands in `docs/FORK.md`) agree. The
  only new hook is `Makefile`'s board rule: four `kiwami:` lines in `Makefile`, none elsewhere.
- The renders: `make test`'s renders in `ui/tests/out/` identical to the branch base's (`2215bd1c`, rendered from
  `git worktree add ../banana-base 2215bd1c`, in the same minute: the clock is on some screens). Like 0003's final
  check, the base is `kiwami` before the feature, not `main`, whose screens lack the fork's rows.
- `kiwami/DEPENDENCIES.md`: Buildroot's row names per-package directories; no new external dependency.
- `docs/FORK.md`, `CLAUDE.md`, the merge skill: as steps 1, 3 and 4 left them, still true.
- M5-M9 below have passed, and their numbers are in the table.
- Every acceptance criterion is met: each is listed by at least one automated or manual test, and those passed.
- `output/cmp`, `output/cmp-saved` and `output/cmp-ccache` are deleted, by you.
- **Commit**: `Build speed: the final check done`

## Automated tests

| Test | Where | What it checks | Criteria |
|---|---|---|---|
| comparison self-test | `kiwami/tests/image-compare.py --self-test`, in `ci.sh` | each noise class excused (dates in four formats, FIT timestamp and digests, GPT GUIDs and CRCs, build paths in text, `.pyc` and equal-length binaries, an ELF file linked around a longer path in `.dynstr` or `.rodata`, bookkeeping, listing times) and each planted difference reported (in the relinked ELF: an instruction, a call's, a branch's and an adrp offset's target, data and GOT pointers, a symbol, a relocation, an FDE, `.eh_frame_hdr`, `.dynamic`, a section header) | A2, A11 |
| audit self-test | `kiwami/tests/deps-audit.py --self-test`, in `ci.sh` | an undeclared library reported, Buildroot's packages too and named so; declared, through a virtual package, or allowed: not; a file in two lists listed apart | A9 |
| build modes | `kiwami/tests/build-mode.sh`, in `ci.sh` | `build.sh`'s flags in each mode (`-j<nproc>` only without one, `--output-sync=target`, nothing serial) and its four stops with their messages; a bad `KIWAMI_SERIAL` | A1, A5, A6 |
| configuration | `kiwami/tests/ci.sh`'s boards loop | every board keeps `BR2_PER_PACKAGE_DIRECTORIES=y` with the fork's options; `rgds_plus` with `KIWAMI_SERIAL=1` doesn't have it | A1, A5, A10 |
| banana-ui identity | `kiwami/tests/ui-objects.sh`, in `ci.sh` | host: objects and one call byte-identical, unstripped and stripped; one `.cpp`, one header and the flags rebuild what they should; `-j` without one; `clean` removes the objects | A7, A8 |
| upstream's own | `make test` | passing; the renders identical to the branch base's | A8 |

## Manual tests

On the build machine for the builds (board `rgds-plus` unless said), on the RG DS Plus for M9. Each build is timed
with `time make ...`; collect the `time` output, the output's `build/build-time.log`, and the comparison's or audit's
report (redirect each to a file, e.g. `> output/m1-compare.txt 2>&1`). The handheld's logs come over SSH/SFTP
(Settings > Network > SSH) or from the card (`banana/logs/` is `/data/banana/logs/`). Only the RG DS Plus is at hand.

Every comparison build is made in `output/cmp/rgds-plus`, removed before each (`rm -rf output/cmp/rgds-plus`), so
each build's checks run right after it. The builds a later test compares with are saved first: `base` (today's way,
the reference to the end) and `new` (the new way, warm). Disk: one output (15-17 GB), the saved files (about 0.4 GB
each), and `output/cmp-ccache` (1-2 GB) during the cold runs.

| # | Board | Steps | Expected | Logs to collect | Criteria | Outcome |
|---|---|---|---|---|---|---|
| M1 | rgds-plus (build machine) | Step 1, before its commit. Today's way, warm (before step 3, plain `make` is today's way): `time make OUT=$PWD/output/cmp rgds-plus`; `kiwami/tests/image-compare.sh --save output/cmp/rgds-plus output/cmp-saved/base`; `rm -rf output/cmp/rgds-plus`; `time make OUT=$PWD/output/cmp rgds-plus` again. Then `kiwami/tests/image-compare.sh output/cmp-saved/base output/cmp/rgds-plus` and `kiwami/tests/image-compare.sh output/cmp-saved/base output/rgds-plus`. The second build stays for M2. | Both build. The first comparison exits 0 with only the known noise; the second fails, listing Poketool (`usr/share/banana/apps/bank/`), `PortMaster.zip` and `usr/libexec/kiwami/romm*` (and warns that the paths differ in length). The second build's time is today's warm time. | both `time` outputs, both `build-time.log`, both reports | A2 (its baseline), A3 (today's), A11 | passed |
| M2 | rgds-plus (build machine) | Step 2, before its commit. `python3 -I kiwami/tests/deps-audit.py output/cmp/rgds-plus` (M1's second build) | Reports `libretro-mupen64plus-next`: `libEGL.so.1`, `libGLESv2.so.2` from `mesa3d-panfrost`; `drastic` allowed; the overlap list short (a fresh output). Any other report goes into step 3. | the report | A9 | passed |
| M3 | rgds-plus (build machine) | Step 3, before its commit. `rm -rf output/cmp/rgds-plus && time make OUT=$PWD/output/cmp rgds-plus` (the first parallel build: per-package paths, so cold for ccache). A package that fails for a missing dependency: its fix goes in `kiwami/external.mk`'s block, then `make OUT=$PWD/output/cmp rgds-plus` again. Then `python3 -I kiwami/tests/deps-audit.py output/cmp/rgds-plus`, `image-compare.sh output/cmp-saved/base output/cmp/rgds-plus`, and `make rgds-plus` on today's `output/rgds-plus`. The build stays for M4. | `build.sh`'s line says per-package directories and `--output-sync=target`; the build completes; the audit reports nothing; the comparison exits 0 (build paths list Python's configuration and libstdc++'s gdb helper); `make rgds-plus` stops with `make clean-rgds-plus` | `time`, `build-time.log`, the terminal around any failure, the audit, the report, the stop's message | A1, A2, A6, A9 | passed |
| M4 | rgds-plus (build machine) | Step 4, before its commit. `kiwami/tests/ui-objects.sh output/cmp/rgds-plus` (M3's build). Then `rm -rf output/cmp/rgds-plus && time make OUT=$PWD/output/cmp rgds-plus`; `ls output/cmp/rgds-plus/build/banana-ui-local/src/kiwami/obj`; `image-compare.sh output/cmp-saved/base output/cmp/rgds-plus`. | identity on the target toolchain; the build completes with banana-ui's objects; the comparison exits 0 | `ui-objects.sh`'s output, `time`, `build-time.log` (banana-ui's build step), the report | A1, A2, A7 | passed |
| M5 | rgds-plus (build machine) | Final check. Warm, the new way (the cache warm from M4's build of the same commit): `rm -rf output/cmp/rgds-plus && time make OUT=$PWD/output/cmp rgds-plus`; `image-compare.sh output/cmp-saved/base output/cmp/rgds-plus`; `image-compare.sh --save output/cmp/rgds-plus output/cmp-saved/new`. | completes faster than M1's second build; the comparison exits 0 | `time`, `build-time.log`, the report | A2, A3 | passed |
| M6 | rgds-plus (build machine) | Final check. Cold, today's way: `ccache -s` (with `BR_CACHE_DIR=$HOME/.buildroot-ccache`, the output's `host/bin/ccache`); `rm -rf output/cmp/rgds-plus output/cmp-ccache && time env BR2_CCACHE_DIR=$PWD/output/cmp-ccache make KIWAMI_SERIAL=1 OUT=$PWD/output/cmp rgds-plus` (about an hour); `grep BR2_PER_PACKAGE output/cmp/rgds-plus/.config`; `ls output/cmp/rgds-plus/per-package`; banana-ui's build in the build's log (since M6, *Deviations*); `image-compare.sh output/cmp-saved/new output/cmp/rgds-plus`; `ccache -s` again. | `build.sh` says as upstream; no per-package directories; banana-ui compiled in one call (no `-c -o src/kiwami/obj` in the log); banana-ui's build step about 95 s in `build-time.log`; the comparison exits 0; `~/.buildroot-ccache`'s totals unchanged by it | `time`, `build-time.log`, the report, both `ccache -s` | A4, A5, A2 | passed |
| M7 | rgds-plus (build machine) | Final check. Cold, the new way: `rm -rf output/cmp/rgds-plus output/cmp-ccache && time env BR2_CCACHE_DIR=$PWD/output/cmp-ccache make OUT=$PWD/output/cmp rgds-plus`; `image-compare.sh output/cmp-saved/base output/cmp/rgds-plus`. Then `rm -rf output/cmp output/cmp-ccache`. | completes faster than M6; the comparison exits 0 | `time`, `build-time.log`, the report | A4, A2 | passed |
| M8 | rgds, universal (build machine) | Final check. The other boards, the new way: `make clean-rgds && make rgds`, then `make universal`. | both build (`rgds`'s old output stops first with `make clean-rgds` if `make rgds` is run before the clean) | `time` of each, the terminal on a failure | A10 | passed |
| M9 | rgds-plus (the handheld) | Final check. The saved `new` update package (`output/cmp-saved/new/banana-rgds-plus-<version>.update.tar`) in `banana/updates/` on the card, applied from Settings > System > Update (or its `.img.gz` flashed to a spare card); start; open a game and quit. | boots to the menu on both panels; the game starts and returns | `/data/banana/logs/banana-ui.log` and `bananad.log` if anything's off | A12 | passed |

Notes from the runs (what the logs showed, what was fixed):

- **M1** (2026-11-09; 0.5.3, from `6831924f` and step 1's files, which don't reach the image; `make -j16`, your
  alias, run by Claude):
  - **Builds**: both completed, the first in 1141 s, the second in 957 s. 957 s (16.0 min) is today's warm time
    for A3. The first build re-filled the cache's entries for the new output path.
  - **The baseline comparison** first failed on three files, all noise the masks lacked:
    - `libcrypto.so.3`: OpenSSL's `built on: Mon Nov  9 10:41:15 2026 UTC`, asctime's format;
    - `pcsx_rearmed_libretro.so`: `build time: Nov  9 2026 12:53:54\n`, `__DATE__` and `__TIME__` in one string;
    - the kernel (`a/Image` and the update's `Image`): its GNU build ID note (20 bytes at 0x10e2f24), and the mtimes in
      its built-in initramfs's cpio headers (`070701...`, 0x11cf3df on).

    With those four masks (*Deviations*), it exits 0 with only the noise A2 lists: build dates in U-Boot, the kernel,
    BusyBox, libcrypto, libpython, PCSX and VICE; 575 `.pyc` source times and 3317 file times; the shaders'
    bookkeeping files. RetroArch's `__DATE__` is the same, because both builds ran the same day. No build paths:
    both builds were made in one path.
  - **Against today's incremental `output/rgds-plus`** it exits 1 (A11) and warns that the paths differ in length.
    It lists, only in the incremental output:
    - Poketool's `usr/share/banana/apps/bank/` (2797 entries under it);
    - `usr/share/banana/portmaster/PortMaster.zip`;
    - `usr/libexec/kiwami/romm` and `romm-game`.

    It also lists the differences an incremental output gathers: older packages, the shaders' modes, `bananads` and
    `sqlite3` with the hint, and 80 libraries whose RPATH Buildroot blanked to X's of the path's length (a hint added
    for them). One of these is a real difference: Python's `HAVE_BLUETOOTH_BLUETOOTH_H` is 0 there, because Python
    was built before bluez in that output. Buildroot declares that dependency (`bluez5_utils-headers`,
    `python3.mk:266`), so per-package directories keep it.
  - The second build stays in `output/cmp/rgds-plus` for M2. `base` is saved in `output/cmp-saved/base`. The reports
    are in `output/m1-compare.txt` and `output/m1-compare-incremental.txt`, the step times in
    `output/m1-build-time-{1,2}.log`.
- **M2** (2026-11-09; M1's second build in `output/cmp/rgds-plus`, 0.5.3 from `6831924f`; run by Claude): exits 1, in
  5 s, over 35 packages and 43 ELF files.
  - **Reported**: `libretro-mupen64plus-next`, `libEGL.so.1` and `libGLESv2.so.2` from `mesa3d-panfrost`
    (`usr/lib/libretro/mupen64plus_next_libretro.so`). Nothing else, so step 3's block needs no other fix yet.
  - **Allowed**: `drastic`, with `libSDL2-2.0.so.0` (sdl2), `libasound.so.2` (alsa-lib) and `libz.so.1` (libzlib).
  - **No library without a provider.**
  - **Files installed by more than one package**: 78, all in `usr/share/alsa/`. They are alsa-lib's, copied over
    again from staging by alsa-utils' install (`alsa-utils.mk:80`). Merged by name under per-package directories,
    alsa-utils still comes last, as in the build order.
  - **ldacbt's list** held 1566 files of 33 packages installed in its window, now counted as theirs (*Deviations*).
    Read as it was, the audit reported ldacbt for 18 libraries and listed 1644 files.
  - **On today's incremental `output/rgds-plus`** it warns that the output isn't fresh: 7 packages installed more than
    once, and the lists of `kiwami-romm`, `portmaster` and `rocknixds-bank`, no longer built.
  - The report is in `output/m2-audit.txt`.
- **M3** (2026-11-09; 0.5.3, from `b61c8f04` (step 2) with step 3's files; `make -j16`, your alias, run by Claude):
  - **The first build** completed with no failure, in 753 s (12.5 min), cold-ish for ccache: 11,645 hits and 16,912
    misses (the per-package paths). `build.sh` said `per-package directories, make's -j16, output per step`.
  - **Its audit** reported nothing (92 packages, 326 ELF files; the 78 alsa files apart). **Its comparison** with
    `base` failed on 34 files:
    - `sqlite3` and `libsqlite3` without zlib, and `libopenal` without D-Bus: undeclared optional dependencies of
      Buildroot's packages;
    - 30 libtool-linked files and `pylibmount.so` laid out around the longer per-package path.

    Fixed and handled as *Deviations* says.
  - **The second build**, fresh, with `sqlite`'s and `openal`'s dependencies: 380 s (6.3 min), warm. Against M1's
    957 s today's way, a first look at A3, which M5 measures.
  - **Its audit** reports nothing, and **its comparison** with `base` exits 0: only the known noise, the 31 relinked
    files listed under build paths with their `.dynstr` or `.rodata` growth, beside Python's configuration and
    libstdc++'s gdb helper.
  - **`make rgds-plus` on today's `output/rgds-plus`** configured it again (the fragment changed) and stopped: `holds a
    build made without per-package directories: make clean-rgds-plus, then make rgds-plus; or, to build it as it was,
    rm .../output/rgds-plus/.config, then make KIWAMI_SERIAL=1 rgds-plus`. Its `.config` now has per-package
    directories.
  - The build stays in `output/cmp/rgds-plus` for M4. The logs are `output/m3-*` (the first build) and
    `output/m3b-*` (the second), and `output/m3-stop.txt`.
- **M4** (2026-11-09; 0.5.3, from `2a4da855` (step 3) with step 4's files; `make -j16`, your alias, run by Claude):
  - **`ui-objects.sh output/cmp/rgds-plus`** (M3's second build, its target toolchain, ccache off): banana-ui from 76
    objects is the one call's, byte for byte, unstripped and stripped (2:19). On the host, the same, and the rebuild
    checks pass (2:22).
  - **The fresh build** completed in 378 s (6.3 min), against M3's second build's 380 s; banana-ui's objects were new
    to ccache. `build.sh` said `per-package directories, make's -j16, output per step`. Its
    `build/banana-ui-local/src/kiwami/obj/` holds the 76 objects and `flags`.
  - **banana-ui's build step** (its five programs and the lease shim) took 42 s in `build-time.log`, against 142 s in
    M3's second build and 108 s in M1's (one call, warm).
  - **Its comparison** with `base` exits 0: the same noise as M3's second build (the 31 relinked files, Python's
    configuration, libstdc++'s gdb helper), and banana-ui in no class, byte-identical.
  - The logs are `output/m4-build.txt`, `output/m4-time.txt`, `output/m4-build-time.log` and `output/m4-compare.txt`.
    The build stays in `output/cmp/rgds-plus`; M5 replaces it.
- **M5-M8** (2026-11-09; 0.5.3, from `Build speed: banana-ui compiled one file per call` (step 4's commit); `make -j16`, run by
  Claude one after another, `/tmp/kiwami-final/builds.sh`; the logs in `output/m5-*` to `output/m8-*` and
  `output/final-builds.txt`):

  | Build | Mode | Cache | Wall time | banana-ui's build step | Comparison |
  |---|---|---|---|---|---|
  | M1's second | today's way | warm | 957 s (16.0 min) | 108 s | the baseline |
  | M5 | the new way | warm | 363 s (6.0 min) | 24 s | with `base`: exit 0 |
  | M6 | `KIWAMI_SERIAL=1` | cold, `output/cmp-ccache` | 1697 s (28.3 min) | 116 s | with `new`: exit 0 |
  | M7 | the new way | cold, `output/cmp-ccache` | 798 s (13.3 min) | 58 s | with `base`: exit 0 |
  | M8 `rgds` | the new way | `~/.buildroot-ccache` | 423 s (7.1 min) | | |
  | M8 `universal` | the new way | `~/.buildroot-ccache` | 445 s (7.4 min) | | |

  - **A3**: warm, the new way takes 363 s against today's 957 s, 2.6 times faster. **A4**: cold, 798 s against 1697
    s, 2.1 times faster.
  - **The comparisons** (M5 and M7 with `base`, M6 with `new`) each exit 0 with `same contents`: the known noise,
    the same 31 relinked files under build paths, and banana-ui in no class (byte-identical).
  - **M6 built today's way**: `build.sh` said `as upstream (KIWAMI_SERIAL=1)`, its `.config` has `#
    BR2_PER_PACKAGE_DIRECTORIES is not set` and there was no `per-package/`. Its log shows no object compiled and
    banana-ui linked in one `aarch64-linux-g++ ... -o banana-ui src/main.cpp ...`. Its build folder held
    `src/kiwami/obj/`, the host's, copied with `ui/` (*Deviations*).
  - **`~/.buildroot-ccache`**: `ccache -s` the same before M6, after it and after M7 (1.5 GB, 258992 cacheable
    calls).
  - **M5's build** is saved in `output/cmp-saved/new`, for M9.
  - **M8**: `make clean-rgds` emptied today's `output/rgds`. `rgds` and then `universal` (a fresh output) built the
    new way, each with its image and update package (0.5.3).
- **M9** (2026-11-09; M5's build, `output/cmp-saved/new`, 0.5.3 from step 4's commit; on the RG DS Plus, by you):
  booted to the menu on both panels, and a game started and returned to the menu. Nothing was off, so no logs were
  needed.
- **The final check** (2026-11-09, by Claude):
  - `make test` (plain make, no `-j`), `make -C ui` (17.7 s, from objects in parallel),
    `make -C system/bananad CFLAGS="-O2 -Werror"` and `kiwami/tests/ci.sh .buildroot` pass, with the fork's tests:
    `image-compare.py --self-test` (54 checks), `deps-audit.py --self-test` (13), `build-mode.sh` (18) and
    `ui-objects.sh`. `make test` and `ci.sh` ran on step 4's tree, which the final check's commit doesn't change.
  - **The hooks**: `docs/FORK.md`'s two commands agree over `main...HEAD` (12 upstream files, each with its
    `kiwami:` lines). Since the branch base `2215bd1c` the feature changed one upstream file, `Makefile`, on its
    board rule's hook: four `kiwami:` lines there.
  - **The renders**: `banana-ui` built in `../banana-base` (`2215bd1c`, one call) and on this branch (from objects)
    are byte-identical, unstripped and stripped, so the renders are the same by construction. Both trees' `run.sh`
    passed, run at once. Of 1565 PNGs, 91 differ only in their files' bytes. The 39 that differ in pixels show what
    changes from run to run: the clock, the tree's own path (`banana-base` against `BananaLinux`), Bshare's random
    PIN, the save states' times, and the selection and the DS overlay's marker caught at another moment of their
    animations.
  - `kiwami/DEPENDENCIES.md` names per-package directories in Buildroot's row; no new external dependency.
    `docs/FORK.md`, `CLAUDE.md` and the merge skill hold as steps 1, 3 and 4 left them.
  - **Every acceptance criterion is met**: A1 (M3, M5, `build-mode.sh`, `ci.sh`), A2 (M3-M7), A3 (M5 against M1),
    A4 (M6, M7), A5 (M6, `build-mode.sh`, `ci.sh`), A6 (M3's stop, `build-mode.sh`), A7 (`ui-objects.sh`, M4), A8
    (`ui-objects.sh`, `make test`, the renders), A9 (M2, M3), A10 (`ci.sh`, M8), A11 (M1, the self-test), A12 (M9).
  - `output/cmp`, `output/cmp-saved`, `output/cmp-ccache` and the `../banana-base` worktree are left for you to
    delete.

## Deviations

Where the implementation turned out different from this plan, why, and what changed (dated).

- **2026-11-09, step 1: the comparison's rules, refined while writing it against today's outputs.** Agreed with you
  before going on.
  - **The squashfs listing's sizes aren't compared on their own.** A build path's length changes a text file's size
    (Python's configuration holds `per-package/python3/host/` under per-package directories). The content rules decide,
    and they accept a length change only from build paths.
  - **`unsquashfs -s`'s `Filesystem size` line is ignored**, like its time line, and listed under a sixth class,
    *derived*. It changes whenever an excused file compresses to another size, and the files are compared one by one.
    *Derived* also holds U-Boot's loader header and FIT hashes (the plan's "digests"): all are of contents compared on
    their own.
  - **A `.pyc`'s code objects are compared field by field, not re-marshalled.** marshal's output depends on how objects
    are shared, and normalising changes that. Line and column positions are left out when a build path changed: a
    longer path moves the columns after it, and pprint wraps `_sysconfigdata`'s long values differently. When no path
    changed, positions that differ are a difference (the source moved).
  - **The investigation was wrong about `.pyc` files.** Buildroot's are timestamp-based (flags 0), and the header holds
    the source's mtime, which differs between builds: 575 files between `output/rgds` and `output/rgds-plus`. Those
    mtimes are excused under *times*, on one counted line. A header that differs otherwise (the source's size or hash)
    is a difference unless a build path changed the code too. Self-test cases were added for both, and for U-Boot's date
    format, `__TIME__`, a date outside an ELF file (a difference) and the loader header: 27 checks in all.
- **2026-11-09, step 1: four masks M1's baseline pair showed missing** (the plan's provision for them). Two of today's
  builds also differ in:
  - OpenSSL's asctime date and PCSX's `__DATE__ __TIME__` string, both now build dates;
  - the kernel's GNU build ID (`--build-id`, a hash of what it links), now derived;
  - its built-in initramfs's cpio mtimes, now times.

  The build ID and cpio masks cover only their own bytes: the 20-byte SHA-1 after a `GNU` note header, and the mtime
  field of a `070701` header. They apply where dates do (ELF files, the kernel, U-Boot). Each has self-test cases,
  one excused and one a difference: 34 checks in all. The length hint also names a path Buildroot blanked to X's (an
  RPATH).
- **2026-11-09, step 2: a file in two lists is counted as the package's that installed it in the other's window.**
  Agreed with you before going on.
  - **Why.** Buildroot takes a package's file list from its configure step to the end of its install (`pkg_size_before`
    and `pkg_size_after`, `.buildroot/package/pkg-generic.mk:258,382`). ldacbt configures early: upstream's
    order-only prerequisite (`buildroot/external.mk:61`) makes its staging install before bluez-alsa's configure. Its
    target install comes later, in the package order. Built one package at a time, 33 packages install in between,
    and their 1566 files land in ldacbt's list. Read as it was, the audit reported ldacbt for 18 libraries on M1's
    fresh build, and listed 1644 files as installed by two packages.
  - **The rule.** Without per-package directories, a file in two lists is the other package's when that one's install
    ended inside this one's window (`build/build-time.log`). Under per-package directories the rule is off: each
    package's list is of its own `per-package/<pkg>/target/`, so a file in two lists is one both installed.
  - **Also.** The audit says when an output isn't fresh: a package installed more than once, or the list of a package
    no longer built. Self-test cases for both, and for the window rule each way: 13 checks in all.
- **2026-11-09, step 3: what M3's comparison found, and three tools changed for it.** Agreed with you before going on.
  - **Two optional dependencies of Buildroot's own packages.**
    - One package at a time, `sqlite`'s configure finds zlib, and `sqlite3` and `libsqlite3` link it.
    - `openal`'s cmake finds D-Bus and builds in RTKit's real-time priority (D-Bus loaded with dlopen).

    Neither package declares them (`sqlite.mk`, `openal.mk`), so with per-package directories both came out without
    them. The audit saw neither: it audited only upstream's and the fork's packages, and openal's D-Bus isn't linked.
    Both are now in `kiwami/external.mk`'s block, each under its packages' options.
  - **The audit covers every target package**, Buildroot's own too, each named with its tree. On a build made one
    package at a time it would also report `sqlite`'s `libz.so.1`. A dependency a package only looks for still shows
    only in the comparison, so `docs/FORK.md` and the merge skill add a comparison with a `KIWAMI_SERIAL=1` build when
    upstream moves Buildroot or adds packages.
  - **A build with per-package directories isn't byte for byte today's.**
    - 30 libtool-linked binaries and libraries (bluez', bluez-alsa's, curl's, dbus', freetype's, mpg123's, SDL2_image's,
      _mixer's and _ttf's, theora's, vorbis', ksmbd-tools') are linked with `-rpath <staging>/usr/lib`, and the
      staging is longer by `per-package/<pkg>/`.
    - Buildroot blanks the path to X's in the image, but `.dynstr` keeps its length, so the sections after it move by
      16 to 64 bytes. Calls, branches, adrp pages and their offsets, symbols, relocations, `.dynamic`, GOT and pointer
      words and `.eh_frame` all change with them.
    - `pylibmount.so` does the same through an assert's relative include path in `.rodata`
      (`../../per-package/util-linux/host/...`).

    No change to the build avoids it, and two of today's builds made in output paths of different lengths differ the
    same way.
  - **The comparison checks such a file as the same file linked again** (`relinked()`). You chose this over a
    structural check, dropping per-package directories, or padding the link path.
    - The path holder is normalised: the X's, `per-package/<pkg>/`, an output's path, and a string's padding after it.
    - Each section's move comes from the headers, and every difference must be an address that moved with its
      section. That covers PC-relative instructions (branches, `adr`, and `adrp` with the `add`, `ldr` or `str` that
      uses its page: the last `adrp` to that register, else one in the same function by `.eh_frame`'s ranges), symbols,
      relocations, `.dynamic`, version records, GOT and pointer words, `.eh_frame` and `.eh_frame_hdr`'s encoded
      pointers, and the headers' layout. Everything else is compared byte for byte, the date rules aside.
    - The instructions are decoded in Python, not by the output's objdump as first proposed, so the self-test runs in
      CI.
    - On M3's 34 differing files it accepts the 31 shifted ones, and still reports `sqlite3`, `libsqlite3` and
      `libopenal`. In seven of the real pairs, 300 random bit flips and the targeted changes were all reported.
    - 20 self-test cases on a synthetic ELF, 54 checks in all. The rule is about 650 lines, more than the 300
      estimated.

- **2026-11-09, final check: A5's check of banana-ui's one call is its build log, not a missing `obj/`.** Agreed
  with you. Buildroot's local site method copies all of `ui/` into `build/banana-ui-local/`, the host's
  `ui/src/kiwami/obj/` from a `make ui` too, and `KIWAMI_SERIAL=1` leaves it unused. So M6's build folder held the
  folder while banana-ui was built in one call. M6's check is the log: no `-c -o src/kiwami/obj` compile, and one link
  from the sources. (In an output with per-package directories the copied objects are compiled again for the target,
  `make -B`, as `kiwami.mk` says of its C objects.)

## Progress

A step is marked done in its own commit, so the commit column holds the subject (a commit can't carry its own hash).

| Step | Status | Commit |
|---|---|---|
| 1. The image comparison | done | Build speed: a comparison of two builds' images and update packages |
| 2. The dependency audit | done | Build speed: an audit of the libraries packages link against what they declare |
| 3. Packages built in parallel, in per-package directories | done | Build speed: packages built in parallel, in per-package directories |
| 4. banana-ui compiled one file per call | done | Build speed: banana-ui compiled one file per call |
| 5. Final check | done | Build speed: the final check done |
