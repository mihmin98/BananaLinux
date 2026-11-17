# Working on the fork

This repository is a fork of BananaLinux. Upstream keeps changing, so the fork is kept in a shape where taking in its
changes is a merge with few or no conflicts: the fork's own work lives in its own files, and upstream's files carry
only small, marked hooks into them.

## Branches

| | |
|---|---|
| `main` | upstream, exactly: never committed to, only fast-forwarded to `upstream/main` |
| `kiwami` | the fork: `main` plus the customizations; upstream comes in by merging `main` into it |
| `k/<NNNN>-<name>` | work on one feature, branched off `kiwami` and merged back when it works |

Feature branches are `k/<NNNN>-<name>`, not `kiwami/<name>`: git keeps a branch as a file under `.git/refs/heads/`,
so a branch `kiwami` and a branch `kiwami/x` can't both exist.

A small change goes on `kiwami` directly. A feature has a GitHub issue on this fork, and the issue's number, four
digits, is its ID: issue #7 about Tailscale is the branch `k/0007-tailscale`, and its investigation and plan are in
`.workspace/0007-tailscale/` (the stages are in `CLAUDE.md`, carried out by the skills `banana-investigate`,
`banana-architect` and `banana-develop` in `.claude/skills/`, which hold the templates). Two features on the same
subject stay apart by their numbers.

A feature branch comes back through a pull request in GitHub's UI. Opened from a fork, a pull request's base is the
parent repository by default: set it to this fork's `kiwami`, or open it from
`https://github.com/<owner>/<repo>/compare/kiwami...k/<NNNN>-<name>`, which sets both. Merge with "Create a merge
commit" (not squash or rebase), so the feature's step commits, which its plan's progress section points to, stay in
the history. The description ends with `Closes #<issue>`: `kiwami` is the fork's default branch on GitHub, and GitHub
closes an issue that way only on a merge into the default branch.

A change meant for upstream (a fix, or a hook upstream could use too) is a branch off `main`, not `kiwami`, so it
carries nothing of the fork.

## Once, on each clone

```sh
git remote add upstream https://github.com/<upstream owner>/BananaLinux.git
git fetch upstream
git branch -u upstream/main main
git config rerere.enabled true            # a conflict resolved once is resolved the same way the next time
git config merge.conflictStyle zdiff3     # conflicts show the common ancestor too, not only both sides
```

## Where a customization goes

The rule: a new file of the fork's own wherever the build allows one; an upstream file only for a hook into it.

| Customization | Where |
|---|---|
| Packages (Tailscale, emulators, cores, Grout...) | `kiwami/package/`, a second Buildroot external tree (`kiwami/external.desc`, `Config.in`, `external.mk`) |
| Image options | `kiwami/configs/kiwami.fragment`, merged over upstream's `banana_*_defconfig` at build time; upstream's defconfigs are not edited |
| Another version of one of upstream's packages | `kiwami/local.mk` (Buildroot's package override file, set by the fragment; `override <PKG>_VERSION`), its patches and hash in `kiwami/patches/<package>/`, its options in `kiwami/external.mk`, behind an option in `kiwami/Config.in` |
| Kernel options | `kiwami/linux.fragment`, listed after upstream's (the last one wins); busybox and U-Boot the same way when needed |
| Files in the image | `kiwami/rootfs/`, a second rootfs overlay (copied after upstream's, so it can also replace an upstream file) |
| Additions to upstream's files in the image (`systems.conf`, the theme's `systems.cfg`) | `kiwami/post-build.sh`, appending or inserting into the copy in `$TARGET_DIR`, rather than a whole replacement in the overlay |
| Boot and network services | hook folders that run every file in them: `/etc/banana/sleep.d/` (around a suspend), `/usr/share/udhcpc/default.script.d/` (on an address from DHCP) |
| banana-ui | `ui/src/kiwami/` (inside `ui/`: the package builds only that folder): its own theme behind `banana::Menu`, its own settings rows, its own handlers for the host's events. C libraries it vendors go unchanged in `ui/src/kiwami/third_party/<library>/`, each with the `SHA256SUMS` of its pinned version's files (checked by `kiwami/tests/ci.sh`) and a `VENDORED` note, and are built by `ui/src/kiwami/kiwami.mk` (the `ui/Makefile`'s third `kiwami:` line), which also compiles banana-ui's own sources one file per call (*How the build takes in the fork*) |
| A feature's code under another licence | its own folder with its `LICENSE` and `NOTICE`: the Game Boy Pokémon companion's `ui/src/kiwami/gbpk/` (GPL-3.0 with PokeDaisy's 7(b) attribution term, `kiwami/GB-POKEMON.md`). What upstream's files' hooks call there is header-only (`gbpk.h`, `rules.h`, the `.inc` files), as upstream's test binaries link `ui/src/nds/` without the fork's sources |
| Scripts that make the fork's committed data | `kiwami/tools/` (`pokedaisy-extract.py`, `pret-extract.py`, `gbpk-port.py`), each with a `--check` that `kiwami/tests/ci.sh` runs against the `SOURCES` it writes |
| Data kept for reference, not built or installed | `kiwami/data/` (PokeDaisy's reference data in `kiwami/data/pokedaisy/`), outside `kiwami/rootfs/` so nothing of it reaches the image |
| Tests of the fork's features | `kiwami/tests/` (a one-line `kiwami:` hook if `make test` has to run them) |
| A feature's investigation and plan | `.workspace/<NNNN>-<name>/` |
| The skills of the feature workflow and of taking in upstream | `.claude/skills/banana-*/` |
| The report of a merge from upstream | `.workspace/merges/<YYYY-MM-DD>-<main's short sha>.md` |
| Settings only | `/data/banana/` on the card (`settings.conf`, `bananad.conf`, `systems.conf`): nothing in the repo |
| External code and its versions | `kiwami/DEPENDENCIES.md`: every dependency the fork adds, its version and where it is pinned, updated in the commit that adds or bumps it; and upstream's that the fork's code relies on (Buildroot, its Go, Linux) |

Left alone, because their owners overwrite them: `ui/rnds/` and `ui/assets/rnds/` (ROCKNIXDS, `tools/import-rnds.sh`),
the ROCKNIX-synced files in `buildroot/board/rgds/` (`tools/sync-rocknix-hw.sh`), and `ui/assets/overrides/` (upstream's
own pictures over the import). A change there goes to its owner, or is done from the fork's files instead (a wrapper
around `RndsMenu`, a file in the overlay).

## How the build takes in the fork

The top `Makefile` (four `kiwami:` lines) configures each board with `BR2_EXTERNAL=buildroot:kiwami`, so Buildroot
sees the fork's packages, and then runs `kiwami/configure.sh` over the `.config` upstream's defconfig made. That
appends the fork's rootfs overlay, post-build script and kernel fragment to upstream's lists, merges
`kiwami/configs/kiwami.fragment`, lets Buildroot resolve the dependencies and fails if an option of the fork's was
dropped. A board's output is configured again when the fragment or the script change; packages already built aren't
rebuilt by that (`make linux-reconfigure-rgds` after a change to `linux.fragment`, for example).

The fragment also sets Buildroot's package override file (`BR2_PACKAGE_OVERRIDE_FILE`) to `kiwami/local.mk`, which
Buildroot reads after `.config` and before any package: that is where the fork pins another version of an upstream
package (RetroArch's master). It still reads the output's own override file, Buildroot's default
(`output/<board>/local.mk`, for a `<PKG>_OVERRIDE_SRCDIR` while working on a package), so that keeps working.

**Packages in parallel.** The fragment turns on Buildroot's per-package directories (`BR2_PER_PACKAGE_DIRECTORIES`,
experimental): each package builds in its own copy of what it depends on (`per-package/<package>/` in the output), so
packages that don't depend on each other build at once. The board rule's fourth `kiwami:` line runs the build through
`kiwami/build.sh`. It adds `-j$(nproc)` when make has no `-j` (a `make -j16` keeps its own), and `--output-sync=target`,
so a step's lines come out together when it ends. A long step (the kernel's, Mesa's) can then look stalled, though
the packages' own makes print as they go. A package now sees only what it declares. What packages use without
declaring it, which they found one package at a time only because it happened to be built first, goes in
`kiwami/external.mk`'s per-package block: upstream's mupen64plus-next links libEGL and libGLESv2, and Buildroot's
sqlite finds zlib and its openal D-Bus. `kiwami/tests/deps-audit.py <output>` finds a library a package links on a
fresh build. A library a package only looks for (an optional feature) shows only in a comparison with a build made
with `KIWAMI_SERIAL=1` (below).

`KIWAMI_SERIAL=1`, given when an output is configured (`make KIWAMI_SERIAL=1 <board>` on a fresh one), leaves the
option out: that output then builds as upstream's, one package at a time with nothing added, which is the fallback
when a merge breaks the parallel build. An output's mode is fixed when it's configured, since Buildroot can't switch
an existing one: `build.sh` stops, saying what to run, when `KIWAMI_SERIAL` and the output's `.config` disagree, or
when the output holds a build made the other way. An output configured with it is built with it every time: a change
to the fragment or `configure.sh` configures it again, and without the switch `build.sh` then stops. Switching is a
fresh output, and its first build misses ccache everywhere (the compilers' sysroot, part of each object's key, moves
under `per-package/`), so it costs a cold build once.

**banana-ui from objects.** Upstream's rule builds banana-ui in one `g++` call, all of its sources at once on one
core. `ui/src/kiwami/kiwami.mk` turns its `SRC` into objects in `ui/src/kiwami/obj/` instead, each compiled with the
rule's own command, `-c` and the header dependencies added, in parallel (make is given `-j$(nproc)` when it has no
`-j`), and kept by ccache one by one. Upstream's rule then links them in its order. The binary is byte for byte the
one call's, unstripped and stripped: `kiwami/tests/ui-objects.sh` checks it with the host's `g++` in `ci.sh`, and
with an output's target toolchain when given one (`kiwami/tests/ui-objects.sh output/rgds-plus`). On the host, a
change compiles again only the objects it touches. `KIWAMI_SERIAL=1` builds the one call (`make KIWAMI_SERIAL=1 ui`).
In an image, banana-ui follows the output's mode: `kiwami/external.mk` gives its build `KIWAMI_SERIAL=1` in an output
configured without per-package directories.

**Fresh builds.** An image for the handheld comes from a fresh output: `make clean-<board>`, then `make <board>`, as
two commands (with a `-j`, `make clean-<board> <board>` would run both at once). An incremental build keeps the files
of packages that have gone, because Buildroot never removes them from `target/`: an output built since before 0.5.3
still put Poketool and PortMaster in its images after upstream dropped them.

**Comparing two builds.** A change to how the image is built, rather than to what's in it, is checked by building it
both ways and comparing: `kiwami/tests/image-compare.sh <old> <new>` reads two outputs, or folders saved from one with
`--save <output> <folder>`. It fails on anything but the noise two builds of one commit already differ in (build
dates, random IDs, times, build paths, Buildroot's bookkeeping), and lists what it excused. Both are built in the
same output path (`make OUT=$PWD/output/cmp <board>`, one after the other), because some binaries embed it. A build
with per-package directories is never byte for byte one made without: some 30 libtool-linked binaries and libraries
are linked with the longer per-package staging path in their RPATH (Buildroot blanks it afterwards), so their
sections move by a few bytes. The comparison checks each such file as the same file linked again: every address in it
moved with its section, and nothing else differs.

`kiwami/tests/ci.sh <buildroot dir>` checks the fork's shell scripts and configures every board that way (and
`rgds_plus` with `KIWAMI_SERIAL=1`); CI runs it after upstream's checks (one `kiwami:` step in
`.github/workflows/ci.yml`), and `kiwami/tests/ci.sh .buildroot` runs it locally. `make savedefconfig-<board>` would
write the merged options into upstream's defconfig: don't use it on the fork's builds.

## Hooks in upstream's files

Where an upstream file has to change (the Makefiles, `main.cpp`'s theme list, `settings.cpp`'s rows), the change is a
call into the fork's own code, as few lines as it can be, and marked so all of them can be found:

```cpp
if (theme == "kiwami") return new KiwamiMenu(&lib, &be, sc);	// kiwami: the fork's theme
```

```make
SRC += $(wildcard src/kiwami/*.cpp)	# kiwami: the fork's sources
```

Every hook carries `kiwami:` in its comment. Everything the fork changes in upstream's files:

```sh
git diff --stat main...kiwami -- . ':!kiwami' ':!ui/src/kiwami' ':!.workspace' ':!.claude' ':!docs/FORK.md' ':!CLAUDE.md'
git grep -n 'kiwami:' -- . ':!kiwami' ':!ui/src/kiwami' ':!.workspace' ':!.claude' ':!docs/FORK.md' ':!CLAUDE.md'
```

The first list should stay short and the two should agree: a file in the diff with no `kiwami:` in it is a change that
crept in outside a hook.

Behaviour that changes what upstream does is behind a setting whose default is upstream's, so with the fork's options
off the fork behaves, and renders, as `main` does.

## Commits

- One customization per commit (or a few), its hooks in the same commit as the code they call, so a conflict later
  says which customization it belongs to.
- Subjects in the repository's style (plain descriptive English, a component prefix where it helps: `kiwami:`,
  `banana-ui:`, `tailscale:`), no conventional-commit prefixes.
- No reformatting, reindenting or reordering of upstream's code in a hook commit: only the lines the hook needs.
- No references to upstream's (or any other repository's) issues and pull requests in commit messages, issues, pull
  requests or comments: no `<owner>/<repo>#12`, `GH-12` or link to one, and no `@` mention of their people. GitHub
  lists each one on that issue's or pull request's page, where the fork's customizations don't belong. A bare `#12`
  can resolve to either repository, so `#<n>` is written only for this fork's own issues and an upstream subject's
  "(#12)" isn't copied; upstream's work is named by what it does, or by its commit's short SHA.
- What the fork can't avoid: upstream's own commits carry their numbers ("(#13)" from a squash-merge), and pushing
  them to the fork (`main`, or `kiwami` after a merge) has GitHub record "<you> added a commit that references this
  issue" on upstream's item, with upstream's own commit subject. Their messages can't be changed without breaking
  `main` as a mirror; only the fork's own commits and texts are kept free of references.

## Taking in upstream

Often, while upstream's changes are small:

```sh
git fetch upstream
git log --oneline main..upstream/main               # what's new
OLD=$(git rev-parse main)
git switch main && git merge --ff-only upstream/main
git switch kiwami && git merge main
```

Conflicts are then in hooks, and resolving one is putting its line back where upstream's code now is (rerere repeats
it next time).

**Upstream's Home server rework, carried ahead of `main`.** `kiwami` carries upstream's pending rework of the Home
server ("RomM saves that follow the game, kept safe, shared with Grout; RomM set up, games and saves from the menu"),
merged from its author's branch at `cab127ba` by 0039 before upstream merged it: 88 commits in upstream's files
(`banana-sync` split into `sync.cpp`, `sync_core.cpp` and `sync_romm.cpp`, the save vault, Get games, the sync before a
game, the RomM form's QR pairing). The fork's hooks in `banana-sync` sit in its files. Until `main` has it,
`git merge-base --is-ancestor cab127ba main` fails, and what to do at each merge from `main` depends on what upstream
did with it:

- **merged unchanged** (upstream merges pull requests' heads as they are): `cab127ba` is an ancestor of `main` and the
  merge brings nothing new of it. `main` is the reference again from then on.
- **merged with more commits**: they merge on top of `cab127ba` as any upstream change does; the hooks in the files
  they touch are checked as usual.
- **squashed or rewritten**: `main` has the same files written by other commits, and the merge conflicts across them.
  Resolve each to upstream's version (`git checkout --theirs`), then put the fork's hooks back (*Hooks in upstream's
  files*, and the paragraphs on RomM's game list and the phase times below), and run `romm-list.sh`, `sync-times.sh`
  and `sync-times-ui.sh`.
- **dropped**: decide then, with the user, between keeping it as the fork's own (its files listed here as the fork's)
  and reverting the merge (`git revert -m 1` of 0039's merge commit, the fork's later hooks re-ported onto `main`'s
  `banana-sync`).

While `kiwami` carries it, the comparisons below that name `main` (the hooks' `git diff --stat`, the renders) are made
against `cab127ba` instead: `main...kiwami` would list the whole rework as the fork's. The `banana-merge-upstream`
skill's report says which of the four cases a merge is.

The skill `banana-merge-upstream` (`.claude/skills/`) carries this out once `main` is up to date: before the merge it
predicts the textual conflicts (`git merge-tree`) and the logic ones (upstream changes that merge cleanly but break or
duplicate what the fork relies on, unmerged `k/*` branches included) and settles each with the user; the merge then
runs as `git merge --no-ff --no-commit main`, so the resolutions, the fork's adaptations and the report
(`.workspace/merges/<date>-<main's sha>.md`) all go into the merge commit. After the merge:

1. **Builds and tests**: `make test`, `make -C ui`, `make -C system/bananad CFLAGS="-O2 -Werror"`, and
   `kiwami/tests/ci.sh .buildroot` (the fork's shell scripts, and every board configured with the fork's options). If
   upstream moved Buildroot, its Go or Linux, check `kiwami/DEPENDENCIES.md`'s second table against the fork's code.
   When upstream adds a package or changes how one builds, a fresh `rgds-plus` build (`make clean-rgds-plus`, then
   `make rgds-plus`) and `kiwami/tests/deps-audit.py output/rgds-plus`. A library it reports goes in
   `kiwami/external.mk`'s per-package block, and `KIWAMI_SERIAL=1` builds images meanwhile. When upstream moves
   Buildroot or adds packages, also the comparison (*Comparing two builds*) with a `KIWAMI_SERIAL=1` build of the same
   commit: an optional library a package looks for only shows there.
2. **The hooks are all there**: the `git grep` above lists the same hooks as before the merge. A hook whose line upstream
   removed is not a conflict, only a hook gone.
3. **What the fork shadows**: files the fork replaces or patches at build time get no conflicts, so their upstream
   changes are read by hand:
   ```sh
   git diff $OLD main -- system/rootfs/etc/banana/systems.conf buildroot/configs/ ui/src/menu.h \
       system/rootfs/usr/libexec/banana/share buildroot/package/retroarch/cores.in \
       system/rootfs/usr/libexec/banana/run-retroarch system/rootfs/usr/libexec/banana/run-bananafront \
       system/rootfs/etc/banana/retroarch.cfg \
       system/rootfs/usr/libexec/banana/ra-secrets buildroot/package/retroarch/retroarch.mk \
       buildroot/patches/retroarch/ system/rootfs/usr/libexec/banana/update ui/Makefile \
       ui/src/nds/ ui/src/gba/ ui/src/pokemon.h ui/src/pokemon.cpp ui/src/settings_gamepanel.cpp \
       ui/rnds/RndsUI.cpp buildroot/package/bananafront/bananafront.mk
   ```
   (the systems list, whose N64 `ext` line `kiwami/post-build.sh` adds `7z` to (upstream lists it on the other six
   cartridge systems; the build stops if N64's line is gone; a new cartridge system without it may want it too), the defconfigs the fork adds to, the share script
   whose `veto files` `kiwami/post-build.sh` adds tailscaled's state and RetroArch master's credentials files
   (`*retroarch-keychain.*`) to (the build stops if the line changes shape; a new place where upstream shares the card
   wants the same veto), `banana::Menu`, the interface the fork's theme
   implements: when it changes, `git diff $OLD main -- ui/src/carts.cpp` shows how upstream changed its own theme to
   match, and upstream's cores and the launcher that picks one: a core upstream adds that the fork already has
   (Mesen, SameBoy, Genesis Plus GX) means the fork drops its package, and the fork's cores are choices in upstream's
   own core rows in Settings > Other systems > Emulators (`RA_SYSTEMS` in `ui/src/settings.cpp`, which names Mesen and Genesis Plus
   GX; SameBoy joins the Game Boy's through the fork's `kiwami::moreCores` hook at the end of `coreChoices`), so a
   change to that table or to how `run-retroarch` reads `core.<system>` has to be followed; the fork's `core-overrides` hook sits after
   `run-retroarch`'s core log line and gives Mesen and SameBoy threaded video, so RetroArch's settings matter too:
   if upstream's `retroarch.cfg` turns `video_threaded` on for every core, the fork's overrides are no longer
   needed, and a moved `rgui_config_directory` moves where they are written. They apply only when RetroArch runs the
   game: before the core is picked, `run-retroarch` hands the systems in its `BF_SYSTEMS` to upstream's BananaFront
   (`run-bananafront`, which takes `core.<system>` too and reads no `.cfg` overrides) while Settings > Other systems >
   Frontend is on Auto, so a change to that list or to how Auto chooses changes what runs the fork's cores. And
   RetroAchievements' secrets: the fork's `ra-keychain` hook is the first line of `ra-secrets`' `scrub`, which folds
   RetroArch master's credentials file back into `retroarch.cfg` before upstream's scrub reads it. If upstream
   reworks `scrub`, the hook stays first in it; a new caller of the scrub gets the fold with no change. And upstream's
   RetroArch itself: while `BR2_PACKAGE_KIWAMI_RETROARCH_MASTER` is on, the fork's pin (`kiwami/local.mk`) and patches
   (`kiwami/patches/retroarch/`) shadow upstream's version and patches, so a change upstream makes there never
   reaches the image. `kiwami/tests/ci.sh` stops when upstream's `RETROARCH_VERSION`, `RETROARCH_JOYPADS_VERSION` or
   patches differ from what the fork last checked, and what to do is in the comment above the stamps in
   `kiwami/tests/retroarch-pin.sh`; a download upstream adds to `RETROARCH_EXTRA_DOWNLOADS` needs its hash in the
   fork's `kiwami/patches/retroarch/retroarch.hash`, or the download stops. And Power scaling (0043): its hook is the
   line in `run-retroarch` right after `setting()`'s block and before the Frontend's dispatch, so BananaFront's games
   get it too, and the script it sources (`/usr/libexec/kiwami/clocks`) uses that `setting()`;
   `kiwami/tests/clocks.sh` stops if the line moves or `setting()` goes. Its row follows Frontend on Other systems
   (`Kiwami::place`'s `FOLLOWS`, added from the Second screen hook, which upstream's Other systems block holds), so a
   renamed or moved Frontend row, or that hook moved out of the block, moves it, which `power-scaling-ui.sh` catches.
   BananaFront's full-range governor is upstream's own, turned on by `BANANAFRONT_CPUGOV=1` (`main.c` at
   `bananafront.mk`'s pin): a bump of BananaFront re-reads that it still takes the variable and still logs `cpugov:
   on`, since a renamed one would quietly fall back to its floor. RetroArch's side is the fork's patches `0101` and
   `0102` (`kiwami/DEPENDENCIES.md`'s RetroArch row), which a bump of the fork's RetroArch pin must still apply. And
   the update script: with
   `BR2_PACKAGE_KIWAMI_OTA_OFF` on, `kiwami/post-build.sh` puts a first case arm after its `case $1 in` line that sends
   `check`, `download` and `beta-login` to `/usr/libexec/kiwami/ota-off` (the build stops if that line changes shape);
   a new command of upstream's that fetches from its releases wants the same). And Settings' order: `build()` sorts the
   tiles and each page's rows by its `ORDER` table, and the fork's `kiwami:` line in its `place` gives each of the
   fork's entries the place of the one it follows (`Kiwami::place`, `ui/src/kiwami/kiwami.cpp`: the RetroAchievements
   tile after System, Network's Tailscale after Bluetooth); a name upstream drops from `ORDER` sends the fork's entry
   to upstream's own placing (a tile after the tileless sections, out of reach), which `ra-ui.sh` and
   `tailscale-ui.sh` catch. Upstream's own tests reach Network's rows from both ends (two Downs to Bluetooth, Up to
   File sharing, two Ups to Home server), which is why Tailscale sits between Bluetooth and Home server. Home server's
   Show sync times is placed by its hook's position, the line after the Forget this server row's `add`, because Home
   server isn't in `ORDER`: after Forget, for RomM and Romhost alike, and the fork's other rows after it (a paired
   RomM 5.4's Installs from RomM, `kiwami.cpp`'s `installsUpdate()`, then a paired RomM's BIOS,
   `ui/src/kiwami/romm-bios.cpp`, the page's last). Installs from RomM is a row of upstream's kind, which upstream's
   `change()` and `save()` write to `settings.conf`; the fork's `update()` sees its value change and runs `banana-sync
   kiwami installs on|off`, with no hook of its own. Not after Sync now, where it was before
   the Home server rework: the rework's `ui/tests/sync.sh` reaches Get games, Manage saves, Save slot and Device name
   by counting Downs from Server, and no upstream test reaches the page's end (none goes Up in it). A row upstream adds
   after Forget, or a test of its that goes Up from Server, needs the hook moved; `sync-times-ui.sh`'s and
   `romm-bios-ui.sh`'s and `romm-installs-ui.sh`'s row checks (Up from Server) and `make test` catch it. If upstream adds the page to `ORDER`, the row needs a place in
   `Kiwami::place`'s `FOLLOWS`. And RomM's
   game list in banana-sync: `ui/src/sync_romm.cpp`'s `Romm::list()` asks for 200 games a page, keeps each as
   `syncmem::slim()`'s copy (`ui/src/kiwami/syncmem.h`) and, in a sync, says after each page how far it got in the
   status Settings draws as Sync now's bar ("Listing games"), and `doSync` (`ui/src/sync.cpp`) ends its report with the
   peak memory. The progress goes only over a sync's own "running" status, which `synctimes::lastStatus()` keeps
   (`status()`'s own copy is in `sync_core.cpp`'s anonymous namespace), so Get games' refresh writes none. A field
   upstream starts reading of a listed game in `manifest()`, `romFiles()`, `romTitle()` or `romRegions()` has to be
   kept by `slim()`, and `kiwami/tests/romm-list.sh` stops on one it drops (and when those readers move, as it can't
   see them then). If upstream fixes the listing's memory its own way, the merge takes upstream's and drops the fork's
   lines and header. And the sync's phase times (Home server > Show sync times, `ui/src/kiwami/synctimes.h`):
   `status()` in `ui/src/sync_core.cpp` hands each line it writes to `synctimes::seen()`, which starts a phase on each
   new title, and the steps that write no status of their own get a `tick()` line at the top of their loops (in
   `sync.cpp`: `want`, `gone`, the gamelists and Romhost's saves; in `sync_romm.cpp`: RomM's slot heads, `listHeads`,
   and its old saves' and states' listing, `sync()`). `doSync`'s `start()` line times no sync of one game (`onlyRom`).
   `phaseOf()` takes the downloads' "Downloading game <i> of <N>" and "Downloading" as one phase, Downloading. A title
   upstream rewords, or a new step that writes no status, shows in `kiwami/tests/sync-times.sh`'s phase lists, which
   then need the new name. And the fork's RomM additions in banana-sync (`ui/src/kiwami/romm/`, header-only, the
   `kiwami` verb: BIOS from RomM's firmware, and installs from its web page): `sync.cpp` includes
   `kiwami/romm/romm.h`, the device pairing's `requested_scopes` line ends with the fork's `SCOPES` (`firmware.read`, `collections.read`), `main()` hands
   `kiwami` to `romm::command()` right after libcurl is loaded (the settings, the certificates and the paths are read
   by then), and `dropServerLists()` (Forget, and a pairing with another server) drops the fork's
   `banana/kiwami-romm.json` and `banana/kiwami-romm-list.jsonl` too. `sync_romm.cpp` exports its platforms' mapping as `bsync::kiwamiFolder()`, one line
   after its anonymous namespace that copies `list()`'s own (`ROMM_SLUGS`, then `BANANA_FOLDERS` by `fs_slug`): if
   upstream changes that mapping, the line follows it. A scope upstream adds to its own list goes before the fork's;
   if upstream starts asking for `firmware.read` itself, the fork's `SCOPES` drops it. The fork's verbs reach RomM
   through upstream's `newRomm()`, `Backend::call()` and `fetchTo()`, and `bios get` says how far it got through
   upstream's `status()`, with `STATUS` pointed at its own `kiwami-bios.status` under `--run` (never Sync now's
   `sync.status`), which the BIOS page reads while it waits, so a change to those shows in
   `kiwami/tests/romm-bios.sh` and `romm-bios-ui.sh`. One game's sync (before or after it) doesn't list the game's system
   (`ui/src/kiwami/romm/lookup.h`): `sync_romm.cpp` includes `kiwami/romm/romm.h`. In `Romm::list()`, each platform's
   page loop takes its pages from a `romm::Pages` made before it, whose `fetch(off)` replaces the loop's query and
   `call()` (0022's query moved into it, `romsQuery()` in `romm.h`). For one game's run it first asks for the game alone: its
   remembered id (`GET /api/roms/<id>`), else RomM's name search. Both answers go to the loop as one short page, which
   ends it. `romm::record(*this)`, the line before `listed = true`, keeps the ids of the games on either card in
   `kiwami-romm.json` and clears `cardIndex` after a full listing. It reads `Romm`'s `roms`, `onlyRom`, `onCard()` and
   `cardIndex`, and `Pages` copies `list()`'s rule for a game's file on the card (`entryOf()`: a disc set's `.m3u`, none
   for another game of several files). A change to those, or to how `list()` pages, shows in
   `kiwami/tests/romm-lookup.sh` (the requests a sync before a game makes, and the outcome of a renamed game against
   the rework's own build) and in `romm-list.sh`. A card's ROM under another name than the server's is found by its
   contents (`ui/src/kiwami/romm/hashes.h`): `romm::byHash(*this)`, the line before `record`'s, in a sync's listing of
   every system pairs the card files no server game is named after with the server games of one file that have no
   card file of their name, by size and then by the listing's MD5 (no request of its own; archives, whose MD5 RomM
   takes of what's inside, copied from `packedName()`, left out), and keeps each pair as an alias in
   `kiwami-romm.json`, with the MD5s it read. `onCard()`'s first line returns a game's alias (`romm::aliasOf()`) before
   its own lookup by name, so the save of a game renamed on the card syncs in that game's slot, and `Pages` takes a
   game remembered under its alias. It reads `Romm`'s `romJson` (`files[]`' `file_size_bytes` and MD5s) besides the
   above; `onCard()`'s callers all see the alias. A change to how `onCard()` matches, or to `packedName()`, shows in
   `kiwami/tests/romm-hash.sh`. A sync of every system reads RomM's game list from the card
   (`ui/src/kiwami/romm/listcache.h`, `banana/kiwami-romm-list.jsonl`, the slimmed games one a line) and asks RomM only
   for its game ids (`GET /api/roms/identifiers`) and each system's changes since the last listing (`updated_after`:
   the server's time of that listing, its `Date` header, less 60 s): `Pages` hands `list()`'s loop the kept games still
   there merged with the changed ones in RomM's name order (`name_sort_key`, which `slim()` keeps, then id), as pages
   of `syncmem::PAGE`, so the loop reads them as RomM's own. `record()` writes the list again after a listing that went
   through, when it changed. No list, another server's, the ids failing, and Get games' refresh (`sync.cpp`'s
   `games` command, its `kiwami:` line `romm::fullListing()` before `manifest()`) list everything. It relies on the
   loop ending at a short page or at `total`, and on RomM keeping a game's `updated_at` when it changes it. A change to
   `list()`'s loop, or a field `manifest()` starts reading that RomM's changes don't bump `updated_at` for, shows in
   `kiwami/tests/romm-incremental.sh` and `romm-list.sh`. Games sent from RomM's web page (`ui/src/kiwami/romm/installs.h`,
   the setting `kiwami_romm_installs`, off by default; `kiwami installs on|off` sets the device's `remote_install`
   capability): three `kiwami:` lines in `doSync`. `romm::claim(*b, c, set)`, right after the heartbeat in a run of
   the games of every system, claims RomM's requests and puts each game on Get games' list (`editGetList()`), which
   the run's `more()` passes bring. It finds a game in `manifest()` by the URL of its first file
   (`/api/roms/<id>/content/`, `/api/roms/<file id>/files/content/`) and takes that file's path as the key, as
   `gameKey()` does. `romm::report(c)`, before the `done` status, reports each request as its files on the card,
   Get games' list and `homesync-get-failed.txt` say. A reason that begins `not enough space` is reported `failed`,
   and any other is left for the next sync. The first line of the `if (!autoWhy.empty())` block runs a games-only
   `doSync` on Wi-Fi when Auto sync isn't "And on Wi-Fi" and RomM has a request this handheld hasn't claimed
   (`romm::wanted()`). A change to those URLs, to `gameKey()`, to how Get games' list and its failed file are kept,
   to the space error's wording or to `more()` shows in `kiwami/tests/romm-installs.sh`. And banana-ui's rule in
   `ui/Makefile`: `kiwami.mk` compiles its `SRC` one file per call with the rule's command, so a change to that command
   (a flag, a variable, how it reads `SRC`) is followed in `kiwami.mk`'s object rules. `kiwami/tests/ui-objects.sh`
   (in `ci.sh`) fails when the two no longer make the same binary. A source upstream adds to `SRC` is compiled as an
   object with no change; one added after `kiwami.mk`'s include line stays in the link's own call. And the Game Boy
   Pokémon companion's rules in upstream's DS companion (`kiwami/GB-POKEMON.md`): `model.cpp`'s first line of `one()`
   gives the type chart's cells of the Game Boy game being read (its ROM's), and `logic.cpp`'s first lines of `damage()`,
   `effectiveSpeed()` and `catchChance()` hand Gen I and II (`kiwami::gbpk::GEN_I`/`GEN_II`, values of upstream's
   `nds::Gen`) to the fork's own formulas in `ui/src/kiwami/gbpk/rules.h` (`effectiveSpeed`'s `Gen` isn't named, so its
   hook asks `oldRules()`, which the companion's host turns on and off with the game); `model.h`'s `statExp` line
   carries Gen I/II's stat experience in `nds::Mon`. What they call stays header-only: upstream's test binaries
   (`ui/Makefile`'s `tests/nds-*`, `gba-*`, `guide-test`) link `ui/src/nds/` without the fork's sources. A change
   upstream makes to these functions' signatures, `Fighter`, `DamageRange`, `CatchContext` or `Mon` breaks the fork at
   compile time; a rule upstream adds inside them (a held item, a ball) doesn't reach the Game Boy games, which take
   the fork's alone; a new upstream `switch` on `Gen` with no default falls through for the fork's values.
   `kiwami/tests/gbpk-units.cpp` checks the rules through upstream's entry points. And the same companion on the game
   panel: Settings' `gbaPanel` (`settings.h`) is the fork's `kiwami::gbpk::PanelSwitch`, which holds upstream's
   `gba::Panel` and the Game Boy one and forwards every member `settings_gamepanel.cpp` and `settings.h` call (upstream's
   first). A member upstream starts calling on `gbaPanel`, or a change to one's signature in `gba/panel.h`, fails to
   compile until `ui/src/kiwami/gbpk/panel.h` forwards it. `Settings::gameButton`'s line after F's sends Back
   (`BTN_Z`) to `compToggle()` while the Game Boy panel is open; if upstream gives the Game Boy systems a Companion
   button of their own (their Buttons page, `gCompKeys` in `gameOpen`), the line goes. In `main.cpp`, the hooks follow the game's companion
   wiring: `gbGame.open` after the block that makes the reply pipe for upstream's Pokémon and game companions (it
   makes its own when they didn't), `attach` just before `gbaAttach`, the requests after upstream's `r = ...` line, the
   two feeds as an `if (...) {} else` before upstream's `if (companion.active())` (the replies of BananaFront's shared
   memory and of RetroArch's stdout), `release` after the game's pidfd closes and `close` beside `companion.close()`
   at the game's end, and the headless verbs before the script's `unknown command`. If upstream moves the pipe or the
   feeds, or a third companion joins them, the hooks move with them; `kiwami/tests/gbpk-ui.sh` drives the panel through
   the headless verbs, and the Settings rows (the hook after upstream's Pokémon companion row in Second screen, a page
   not in `ORDER`, so the rows stay where they are added). And the same companion's pages: `ui/src/nds/pages.cpp`
   includes `ui/src/kiwami/gbpk/pages-decl.inc` as the last members of `struct Pages::Impl` and
   `pages-impl.inc` after the file's closing brace, and eight lines hand Gen I/II's blocks to them, each by the
   statement it sits beside: the Battle page's wild box (`if (kiwamiWild(...)) return;` before its
   `box("bubble", 360, ...)`), the Party page's Nature/Ability/Item rows (`if (kiwamiSummary(...)) {} else` before
   `if (!m.egg)`), its Friendship block (`if (kiwamiNoFriendship(m)) {} else` before the block's `{`), its Stats block
   with the Effort points total (`if (!kiwamiStats(...)) {` before `heading(tr("Stats")...)`, the closing `}` before
   `// moves with PP`), the Dex page's Ability lines (`if (kiwamiNoAbilities()) ay -= 15; else` before
   `text(tr("Ability")...)`) and its Base stats block (`if (!kiwamiBaseStats(...)) {` before
   `heading(tr("Base stats")...)`, the `}` after its `y += 26;`). Upstream's lines between a `{` and its `}` keep their
   indentation. The `.inc` files draw with `Pages::Impl`'s own helpers and constants (`text`, `textR`, `bar`, `fill`,
   `box`, `heading`, `typeChip`, `measure`, `tr`, `STAT_NAMES`, `GOOD`, ...): one upstream renames or reshapes fails to
   compile and is followed there. A block upstream reworks moves its hook with it; one upstream adds that shows natures,
   IVs, EVs or abilities draws empty or upstream's values for the Game Boy games until a hook skips it, which
   `kiwami/tests/gbpk-ui.sh`'s shots and its trace lines (`KIWAMI_GBPK_TRACE=1`) show. And the same companion's guides:
   `ui/src/nds/guide.cpp` includes `ui/src/kiwami/gbpk/guide-games.inc` as the last rows of its `GAMES` table (the six
   games' and the ROM hacks' ids as literals, keys and codes), so `loadGuideFile` takes their files; the fork's host loads them itself
   from `gb-pokemon/`. A change upstream makes to `GameRow`'s fields breaks the rows at compile time; a schema upstream
   adds to the guide files (a field, a `done_when` rule) is the fork's to use or not, and `kiwami/tests/gbpk-units.cpp`
   loads Crystal's file through upstream's loader and follows its milestones through upstream's `nextMilestone`.
   And the same companion's own types (PureRGB's six, values of upstream's `nds::Type` from 32, past `NUM_TYPES`:
   `ui/src/kiwami/gbpk/gbpk.h`): `pages.cpp` includes `gbpk.h` after `../lang.h`, and `typeChip`'s first line draws
   their chips (`extraTypeName`, `extraTypeColor`) before upstream's `if (t >= NUM_TYPES) return 0;`; `logic.cpp`'s
   `typeName`, a one-line function, is changed rather than added to, the fork's names first and upstream's line as it
   was after them, so an upstream change to it is a conflict on that line alone, taken with the fork's part put back.
   Every other place upstream names, colours or counts types stops at `NUM_TYPES` (an empty name, grey, not counted),
   so a new one does too; one that indexes an array by a type without that guard reads past it for these, which
   `kiwami/tests/gbpk-units.cpp` built with `-fsanitize=address,undefined` shows (its advice and chart over them).
   In upstream's companion code (`ui/src/nds/`, `ui/src/gba/`, `ui/src/pokemon.*`) as a whole, what to look for: a
   member added to `nds::Live`, `Mon`, `RomData`, `PageData` or `gba::Panel`, which the fork's reader, ROM reader or
   `PanelSwitch` may have to fill or forward (a `Live` field left empty shows as nothing on the pages, or as wrong
   advice); a new use of `Gen` in the logic or the pages, which takes the Gen III/IV path for the fork's `GEN_I` and
   `GEN_II` unless a hook skips it; `pkmn::parseReply` or the request pacing reworked, which the fork's reader uses as
   upstream's GBA companion does. And upstream adding a Game Boy or Game Boy Color companion of its own: the fork's is
   then retired at that merge, its hooks, rows and data files removed (`kiwami/GB-POKEMON.md`, *What it doesn't do*). And
   the status bar's Tailscale icon: the fork's `kiwami::StatusBar` (`ui/src/kiwami/statusbar.h`, handed to `RndsMenu`
   by `main.cpp`'s two hooks instead of the host's backend) relies on the order the engine's `drawStatus` draws the bar
   in: the Wi-Fi sprite first at (18, 24), then the left part, which it moves right, then the clock, right-aligned,
   the first draw reaching past x 560, which it leaves where it is. A re-import of ROCKNIXDS that changes `drawStatus`
   merges without a conflict; `kiwami/tests/tailscale-icon-ui.sh` (in `ci.sh`) fails on it, and the backend draws
   nothing when it can't find the Wi-Fi draw. And the game panel's status strip (Second screen > Status on the game
   panel, `ui/src/kiwami/panel-status.cpp`): four `kiwami:` lines in `ui/src/settings_gamepanel.cpp`, the include
   after `nds/overlay.h`, `panelOpened()` as the last line of `gameOpen`, `panelUpdate(t)` before `gameUpdate`'s
   flash check, and `if (!kw || !kw->panelStatus()) {` with its `}` around `gameFrame`'s three hint lines (not
   re-indented). A rework of the panel's bottom line or of `gameUpdate` moves them, visibly; the strip is drawn only
   where the hint was, so a new view upstream draws before the hint and returns from (as the keyboard and the
   companions' pages do) shows no strip, and one drawn below the tiles shows it. Its row is inserted after Second
   screen's `bottom_idle_seconds` row; if upstream drops that key the row falls to the end of the page. The strip
   formats its time and date with `Library::clockText`/`dateText`'s `strftime` strings (`%H:%M`, `%I:%M %p`,
   `%m/%d`), copied into `PanelStatus` so the tests can give it a clock: a change upstream makes to the menu's formats
   is followed there. `kiwami/tests/panel-status-ui.sh` (in `ci.sh`) catches each of these: the row's place, the
   strip's lines and its shots against the panel with the setting off.
4. **Upstream's look unchanged**: with the fork's options off, `make test`'s renders in `ui/tests/out/` match `main`'s
   (rendered from a second checkout, `git worktree add ../banana-main main`; `cab127ba`'s while `kiwami` carries the
   Home server rework ahead of `main`).

## Upstreaming

A hook that would serve anyone (themes registering themselves, a `systems.d/` folder of extra systems, an overridable
`BR2_EXTERNAL`, a hook after a game ends) is worth offering upstream on a branch off `main`. Once it is merged
upstream, the fork's copy of it disappears in the next merge and the fork carries one hook fewer.
