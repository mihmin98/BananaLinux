# 0003 7z ROMs: plan

Issue: #3 · Branch: `k/0003-7z-roms` · Investigation: [investigation.md](investigation.md) · Status: approved

## Summary

Games kept as `.7z` archives are listed and played on seven systems: Game Boy Advance, Game Boy, Game Boy Color,
NES, Super Nintendo, Mega Drive and Nintendo 64. They're found on either card, in any case (`.7z`, `.7Z`). RetroArch,
already built with 7z support, unpacks them when the game starts, as it does `.zip` ones. No setting: they are
listed by default.

The approach chosen in the investigation: `kiwami/post-build.sh` adds `7z` to those seven systems' `ext` lines in the
image's copy of `systems.conf`. It stops the build when one of them has no `ext` line, and leaves a line that already
has `7z` alone. No upstream file changes, no hook, no package, no new dependency.

### Refinements over the investigation

- **The second card can't be tested headless.** banana-ui reads the second card's folder from
  `/run/banana/sd2/roms`, a fixed path (`ui/src/library.cpp:435`), so the headless test covers `/data/roms`
  (`--roms`). Both cards go through the same `listRoms()` with the same lists; the second card is tested on the
  device (M1).
- **RetroArch is recorded as a dependency.** `kiwami/DEPENDENCIES.md`'s table of upstream's dependencies gets
  RetroArch: the feature relies on its 7z support (`HAVE_7ZIP`, on by default and not disabled by
  `buildroot/package/retroarch/retroarch.mk`). If upstream ever builds it with `--disable-7zip`, the games would be
  listed and fail to start.

## Steps

Each step is one commit, and leaves the tree building and `make test` passing.

### 1. The cartridge systems list .7z games

- **Files**: `kiwami/post-build.sh` (changed), `kiwami/tests/ci.sh` (changed), `kiwami/DEPENDENCIES.md` (changed),
  `docs/FORK.md` (changed).
- **Change**:
  - `kiwami/post-build.sh`, after the share veto: a block that rewrites `$T/etc/banana/systems.conf` with awk. In the
    sections `[gba]`, `[gb]`, `[gbc]`, `[nes]`, `[snes]`, `[megadrive]` and `[n64]`, the `ext = ...` line gets ` 7z`
    appended unless it already lists `7z` (upstream adding it itself, or post-build run twice). A chosen section
    with no `ext` line, or missing altogether, is an error naming it, and the build stops (the file is written
    through a temporary, so a failed run leaves it as it was). Sketch:
    ```sh
    C=$T/etc/banana/systems.conf
    awk -v want="gba gb gbc nes snes megadrive n64" '
        BEGIN { n = split(want, w, " "); for (i = 1; i <= n; i++) need[w[i]] = 1 }
        /^\[.*\]$/ { sec = substr($0, 2, length($0) - 2) }
        (sec in need) && /^ext *=/ { if ($0 !~ /[ =]7z( |$)/) $0 = $0 " 7z"; has[sec] = 1 }
        { print }
        END { for (s in need) if (!(s in has)) { print "kiwami/post-build.sh: no ext line for [" s "] in " FILENAME > "/dev/stderr"; bad = 1 }
              exit bad }' "$C" > "$C.tmp" || { rm -f "$C.tmp"; exit 1; }
    mv "$C.tmp" "$C"
    ```
  - `kiwami/tests/ci.sh`: the post-build check's stand-in target gets upstream's `systems.conf` as well as the share
    script (post-build now needs both). Then: each of the seven systems' `ext` line ends in ` 7z` and lists it once;
    `[nds]` and `[psx]` don't list it; a second run leaves the file byte-for-byte the same; with `[n64]`'s `ext` line
    removed, post-build fails and names `[n64]`.
  - `kiwami/DEPENDENCIES.md`: RetroArch in the upstream table (v1.22.2, `buildroot/package/retroarch/retroarch.mk`),
    the 7z ROMs depending on its 7z support.
  - `docs/FORK.md`, *What the fork shadows*: the systems list is named with what post-build adds to it (`7z` on seven
    systems' `ext` lines; the build stops if one is gone).
- **Tests**: `kiwami/tests/ci.sh .buildroot`'s post-build checks.
- **Commit**: `7z ROMs: the cartridge systems list .7z games`

### 2. A headless check: .7z games on their shelves

- **Files**: `kiwami/tests/7z-ui.sh` (new), `kiwami/tests/ci.sh` (changed).
- **Change**:
  - `kiwami/tests/7z-ui.sh`: runs post-build on a stand-in target (upstream's `systems.conf` and share script), makes
    a small library of empty files: `<system>/Game <system>.7z` for the seven systems (Super Nintendo's as
    `.7Z`, for the case; Mega Drive's in a subfolder, for the depth), plus `nds/Game nds.7z` and
    `psx/Game psx.7z`. It then runs banana-ui headless with the post-built `systems.conf`, launching the first game of
    every shelf in turn (`lib <n> 0; press a; wait 3000; wait 1; press b; press b`, as `ui/tests/run.sh` does for the
    favourite). The log must hold exactly seven `launch 0: ....7z (system <id>)` lines, one per system, and none
    for `nds` or `psx`. A second run with upstream's own `systems.conf` must launch no `.7z` at all, so the test
    shows the difference is post-build's.
  - `kiwami/tests/ci.sh`: runs it when `ui/banana-ui` is built, as `tailscale-ui.sh` is.
- **Tests**: `kiwami/tests/7z-ui.sh` itself, through `ci.sh`.
- **Commit**: `7z ROMs: a headless check that the .7z games are on their shelves`

### 3. Final check

- `make test`, `make -C ui`, `make -C system/bananad CFLAGS="-O2 -Werror"`, `kiwami/tests/ci.sh .buildroot`.
- The fork's tests in `kiwami/tests/`.
- The hooks: `git diff --stat main...HEAD` and `git grep -n 'kiwami:'` (the commands in `docs/FORK.md`) agree, and
  list no hook from this feature (it adds none).
- `make test`'s renders in `ui/tests/out/` match `main`'s (`make test` reads upstream's `systems.conf`, so they
  should match without anything switched off).
- `kiwami/DEPENDENCIES.md` lists RetroArch (step 1).
- `docs/FORK.md` updated (step 1); `docs/ARCHITECTURE.md` and `CLAUDE.md` don't describe the systems' lists.
- `make rgds-plus`, the image's `/etc/banana/systems.conf` checked for the seven `7z`s (`output/rgds-plus/target/`),
  installed on the RG DS Plus.
- The manual tests below all passed.
- **Commit**: `7z ROMs: the final check done`

## Automated tests

| Test | Where | What it checks |
|---|---|---|
| post-build's edit | `kiwami/tests/ci.sh` | `7z` on the seven systems' `ext` lines, once each; not on `[nds]` or `[psx]`; a second run changes nothing; a missing `ext` line stops the build, naming the system; the share veto as before |
| .7z games on the shelves | `kiwami/tests/7z-ui.sh` (run by `ci.sh`) | banana-ui headless, with the post-built `systems.conf`, lists and launches a `.7z` game on each of the seven systems (`.7Z` and a subfolder included) and not on the DS or the PlayStation; with upstream's file, none |
| upstream's own | `make test` | unchanged, passing; the renders identical to `main`'s |

## Manual tests

On the device, for what headless tests can't reach. Logs are fetched over SSH/SFTP (Settings > System > SSH) or from
the card on a computer (`banana/logs/` on the card is `/data/banana/logs/` on the device). Only the RG DS Plus is at
hand: every test runs on it, and the RG DS's are not run.

RetroArch's own log is `/run/banana/retroarch.log` (in RAM, only when the game is on the game panel, and replaced at
the next game's start): copy it over SSH right after quitting each game, before starting another, named after the
test (`retroarch-M2-gba.log`, ...).

| # | Board | Steps | Expected | Logs to collect | Outcome |
|---|---|---|---|---|---|
| M1 | rgds-plus | On the second card: `roms/<system>/<game>.7z` for the seven systems, the DS games as they are, one `.7z` in `roms/psx`. On the main card: one `.7z` in `/data/roms/gba`. Start the handheld and look at the shelves. | Every `.7z` game listed on its system's shelf, from both cards; the DS games still listed; the PlayStation's `.7z` not | `/data/banana/logs/banana-ui.log` | passed |
| M2 | rgds-plus | Start a `.7z` game on each of GBA (gpsp), Game Boy and Game Boy Color (gambatte), NES (fceumm), Super Nintendo (snes9x), Mega Drive (picodrive); play a minute, quit from RetroArch's menu. | Each starts, plays with sound, quits back to the menu | `banana-ui.log`; `retroarch.log` after each (`[Content]` lines: what it took from the archive, and whether it unpacked to `/tmp`) | passed |
| M3 | rgds-plus | The same N64 game as `.z64` and as `.7z`: time each from A to its first frame (a stopwatch); with the `.7z` running, over SSH: `grep -E 'VmHWM\|VmRSS' /proc/$(pidof retroarch)/status; free -m`, and the same with the `.z64`. | Both start and play; how much longer the `.7z` takes and how much more memory it peaks at (VmHWM) is noted, and you decide whether the N64 keeps `7z` | `retroarch.log` of each; the two outputs | passed |
| M4 | rgds-plus | A GBA `.7z` game (unpacked to a file): over SSH, `ls -l /tmp` while it runs; save in the game, quit, `ls -l /tmp` again and `ls -l /data/saves`; start it again. The same with a Super Nintendo `.7z` game (unpacked in memory), without the `/tmp` checks. | While the GBA game runs, its ROM is in `/tmp`, gone after quitting; `/data/saves/<game>.srm` written for both; each save loads at the next start | the `ls` outputs; `retroarch.log` of each | passed |
| M5 | rgds-plus | A game already played and saved as `.gba` (or `.zip`), replaced on the card by the same game as `.7z`; start it. | The in-game save is there (the same `<game>.srm`); its play count and time start from zero (expected, oversight 2) | `ls -l /data/saves`; `banana-ui.log` | passed |
| M6 | rgds-plus | During a `.7z` game, close the lid, wait 10 s, open it. | The game carries on | `/data/banana/logs/bananad.log` if it doesn't | passed |
| M7 | rgds-plus | Only with a RetroAchievements account set: start a `.7z` game that has achievements. | The game is identified: its achievements on the game panel, `retroarch.log` names the game (`[RCHEEVOS]` lines) | `retroarch.log` | passed |

Notes from the runs (what the logs showed, what was fixed):

- 2026-10-21, image `banana-rgds-plus-0.4.1` built from `c9cde8a`: its `/etc/banana/systems.conf` has `7z` on the
  seven systems' `ext` lines, once each, and differs from upstream's in nothing else. The user ran the tests on the
  RG DS Plus and reported them all working; no logs were passed back, so no measurements are recorded, and the N64
  keeps `7z` (M3).

## Deviations

Where the implementation turned out different from this plan, why, and what changed (dated).

- **2026-10-21, step 3: the renders are compared with `kiwami` before the feature, not with `main`.** `make test`'s
  renders on this branch differ from `main`'s on four screens (`wifi-row` and `share-row`, bottom, at both sizes):
  the Network page has the Tailscale row of feature 0002, which `main` doesn't. The baseline that shows this feature
  changes nothing on screen is `kiwami` where the branch left it (`2458ab8`): all 124 renders are identical to it.
  (Renders taken in different minutes also differ in the clock, on the home, library, ready, battery and setup
  screens; the comparisons were made in the same minute.)

## Progress

A step is marked done in its own commit, so the commit column holds the subject (a commit can't carry its own hash).

| Step | Status | Commit |
|---|---|---|
| 1. The cartridge systems list .7z games | done | `7z ROMs: the cartridge systems list .7z games` |
| 2. A headless check: .7z games on their shelves | done | `7z ROMs: a headless check that the .7z games are on their shelves` |
| 3. Final check | done | `7z ROMs: the final check done` |
