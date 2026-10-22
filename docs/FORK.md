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
parent repository by default: set it to this fork's `kiwami`. Merge with "Create a merge commit" (not squash or
rebase), so the feature's step commits, which its plan's progress section points to, stay in the history. The
description ends with `Closes #<issue>`.

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
| Kernel options | `kiwami/linux.fragment`, listed after upstream's (the last one wins); busybox and U-Boot the same way when needed |
| Files in the image | `kiwami/rootfs/`, a second rootfs overlay (copied after upstream's, so it can also replace an upstream file) |
| Additions to upstream's files in the image (`systems.conf`, the theme's `systems.cfg`) | `kiwami/post-build.sh`, appending or inserting into the copy in `$TARGET_DIR`, rather than a whole replacement in the overlay |
| Boot and network services | hook folders that run every file in them: `/etc/banana/sleep.d/` (around a suspend), `/usr/share/udhcpc/default.script.d/` (on an address from DHCP) |
| banana-ui | `ui/src/kiwami/` (inside `ui/`: the package builds only that folder): its own theme behind `banana::Menu`, its own settings rows, its own handlers for the host's events |
| Tests of the fork's features | `kiwami/tests/` (a one-line `kiwami:` hook if `make test` has to run them) |
| A feature's investigation and plan | `.workspace/<NNNN>-<name>/` |
| The skills of the feature workflow | `.claude/skills/banana-*/` |
| Settings only | `/data/banana/` on the card (`settings.conf`, `bananad.conf`, `systems.conf`): nothing in the repo |
| External code and its versions | `kiwami/DEPENDENCIES.md`: every dependency the fork adds, its version and where it is pinned, updated in the commit that adds or bumps it; and upstream's that the fork's code relies on (Buildroot, its Go, Linux) |

Left alone, because their owners overwrite them: `ui/rnds/` and `ui/assets/rnds/` (ROCKNIXDS, `tools/import-rnds.sh`),
the ROCKNIX-synced files in `buildroot/board/rgds/` (`tools/sync-rocknix-hw.sh`), and `ui/assets/overrides/` (upstream's
own pictures over the import). A change there goes to its owner, or is done from the fork's files instead (a wrapper
around `RndsMenu`, a file in the overlay).

## How the build takes in the fork

The top `Makefile` (three `kiwami:` lines) configures each board with `BR2_EXTERNAL=buildroot:kiwami`, so Buildroot
sees the fork's packages, and then runs `kiwami/configure.sh` over the `.config` upstream's defconfig made. That
appends the fork's rootfs overlay, post-build script and kernel fragment to upstream's lists, merges
`kiwami/configs/kiwami.fragment`, lets Buildroot resolve the dependencies and fails if an option of the fork's was
dropped. A board's output is configured again when the fragment or the script change; packages already built aren't
rebuilt by that (`make linux-reconfigure-rgds` after a change to `linux.fragment`, for example).

`kiwami/tests/ci.sh <buildroot dir>` checks the fork's shell scripts and configures every board that way; CI runs it
after upstream's checks (one `kiwami:` step in `.github/workflows/ci.yml`), and `kiwami/tests/ci.sh .buildroot` runs it
locally. `make savedefconfig-<board>` would write the merged options into upstream's defconfig: don't use it on the
fork's builds.

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
it next time). After the merge:

1. **Builds and tests**: `make test`, `make -C ui`, `make -C system/bananad CFLAGS="-O2 -Werror"`, and
   `kiwami/tests/ci.sh .buildroot` (the fork's shell scripts, and every board configured with the fork's options). If
   upstream moved Buildroot, its Go or Linux, check `kiwami/DEPENDENCIES.md`'s second table against the fork's code.
2. **The hooks are all there**: the `git grep` above lists the same hooks as before the merge. A hook whose line upstream
   removed is not a conflict, only a hook gone.
3. **What the fork shadows**: files the fork replaces or patches at build time get no conflicts, so their upstream
   changes are read by hand:
   ```sh
   git diff $OLD main -- system/rootfs/etc/banana/systems.conf buildroot/configs/ ui/src/menu.h \
       system/rootfs/usr/libexec/banana/share
   ```
   (the systems list, whose `ext` lines `kiwami/post-build.sh` adds `7z` to on seven systems (the build stops if
   one of them is gone; a new cartridge system may want it too), the defconfigs the fork adds to, the share script
   whose `veto files` `kiwami/post-build.sh` adds tailscaled's state to (the build stops if the line changes shape; a
   new place where upstream shares the card wants the same veto), and `banana::Menu`, the interface the fork's theme
   implements: when it changes, `git diff $OLD main -- ui/src/carts.cpp` shows how upstream changed its own theme to
   match).
4. **Upstream's look unchanged**: with the fork's options off, `make test`'s renders in `ui/tests/out/` match `main`'s
   (rendered from a second checkout, `git worktree add ../banana-main main`).

## Upstreaming

A hook that would serve anyone (themes registering themselves, a `systems.d/` folder of extra systems, an overridable
`BR2_EXTERNAL`, a hook after a game ends) is worth offering upstream on a branch off `main`. Once it is merged
upstream, the fork's copy of it disappears in the next merge and the fork carries one hook fewer.
