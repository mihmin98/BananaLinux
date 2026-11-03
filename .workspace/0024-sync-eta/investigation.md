# 0024 Sync times: investigation

Issue: #24 · Branch: `k/0024-sync-eta` · Status: refined · Confidence: 87% (proceed)

## The request

While a Home server sync runs (Settings > Network > Home server > **Sync now**), the top screen shows a bar, what is
being done and the item in hand, but not how long it has been going or how long is left. With a large RomM library
(1000+ games a system) a sync runs for many minutes, and nothing tells you whether it is moving or how long to wait.

Decided with the user:

- **One line under the existing bar**, on the top screen, while the Sync now row is selected and a sync runs:
  the phase, its elapsed time, the time left in it once there is a rate to go on, and the whole sync's elapsed time,
  in upstream Files' style: `Games · 2:10 · 3 min left · sync 6:40`. Elapsed as `m:ss` (`h:mm:ss` from an hour),
  time left as Files' `duration()` writes it (`45 s`, `3 min`, `1 h 5 min`, `ui/src/settings_files.cpp:50-55`).
  Nowhere else: not the Sync now row's value, not the Last sync help.
- **Every phase gets its time**, including the steps that show nothing today (*Current state*): they become phases of
  their own (checking the card, removing files, writing the gamelists, reading the server's saves).
- **No estimate without a rate**: a phase with nothing to count (starting, a single request) shows its elapsed time
  only; a counted phase shows the time left once some of it is done.
- **The times come from `banana-sync`**, not the menu, so they're right when Settings opens partway through a sync or
  after an automatic one (leaving a game, on Wi-Fi).
- **The Games phase's time left is the larger of two estimates**, from the bytes and from the files
  (*Oversights*, 2).
- **Awake time only**: `CLOCK_MONOTONIC` in both `banana-sync` and the menu; time asleep isn't counted
  (*Oversights*, 4).
- **Behind a setting, default Off**: a row on the Home server page. Off is exactly upstream: no new phases, no times
  line, no extra wakes, `sync.log` as `main` writes it (*Oversights*, 6).
- **Each phase's duration in `sync.log`**, at the end of the report (*Suggestions*, 1).

Out of scope: Choose games' list (`banana-sync games` writes no status, `ui/src/sync.cpp:921`), banana-scrape's box-art
bar, the Romhost server's side, translations of the new strings (*Risks*).

## Oversights in the requirements

1. **The clock wouldn't tick: Settings never wakes for a sync.** Settings reads `sync.status` in `pollSync()` at most
   every 500 ms (`ui/src/settings.cpp:6402-6403`), but only when the main loop runs `update()`. The loop sleeps until
   `nextWake()` (`ui/src/main.cpp:3170`), and `nextWake()` wakes for the box-art scrape (`ui/src/settings.cpp:2663`)
   but not for a sync; `pageWake()` has nothing for it either (`:2672-2687`). So on the device, upstream's bar moves
   only on a key press or the once-a-minute clock wake (`ui/src/main.cpp:3179-3180`). This comes from reading the
   code; it hasn't been seen on the device. A ticking clock needs a wake every second while it's on screen.
   *Proposed*: the fork's existing wake hook (`kw->wake`, `ui/src/settings.cpp:2659`, `Kiwami::wake`,
   `ui/src/kiwami/kiwami.cpp:179`) wakes Settings while a sync runs and Settings is open: every 500 ms for the status
   (as the scrape does), on the next second for the clock. With the setting On, so upstream's bar moves live too;
   with it Off, nothing changes. No new upstream line.

2. **Covers have no size, and the Games bar counts bytes.** A cover's `MFile::size` is -1 (`ui/src/sync.cpp:553`) and
   the bar sums `max(0, size)` (`:1351`), so a sync of thousands of covers and a few ROMs reads as almost done from
   the start, and an ETA from the bytes would be far too short; an estimate from the files alone is wrong the other
   way when a big ISO follows small files. *Decided*: the larger of the bytes estimate and the files estimate
   (`Games i/N`, already in the status, `:1390`). The bar itself stays upstream's.

3. **Phases that write no status leave the previous line up.** Between `Listing games` and `Games`, and after it, these
   run with the last status still shown (*Current state* has the list). Two of them can take minutes: the card check
   hashes every file of the list that is on the card without a state entry (a library copied by hand, adopted on the
   first sync: `localKey()`, `:571-577`, called at `:1323`), and removing a file hashes it first (`unchangedOnCard()`,
   `:579-585`, at `:1428`). *Decided*: each becomes a phase with a count (*What has to change*).

4. **A sync can be slept through.** Only a sync that downloads games holds `awake.pid` (`Awake`, `:1695`: `if (roms)`),
   which stops the idle sleep (`system/bananad/bananad.c:2109-2111`); the power button and the lid still sleep any
   sync (`:1813-1817`: the lid waits only for Files' `transfer.pid`). The device's wall clock can also jump when the
   network sets it. *Decided*: `CLOCK_MONOTONIC` (as banana-ui's `nowMs()`, `ui/src/main.cpp:81-84`), which doesn't
   count sleep, so the rate behind the ETA stays right after a resume.

5. **A stale times file at the start of the next sync.** Settings writes `running 0 0 |starting` itself before
   `banana-sync` starts (`ui/src/settings.cpp:6347`); a times file left by the last sync (or by one killed:
   `onKill`, `ui/src/sync.cpp:71-78`, rewrites only the status) would show its old elapsed time for that moment.
   *Proposed*: the times file carries `banana-sync`'s pid, and the menu shows the times only while the status is
   `running` and that pid is the one in `/run/banana/sync.pid` (written at the sync's start, `:1842`); until then,
   the line shows nothing.

6. **The fork's rule: changed behaviour behind a setting defaulting to upstream's** (`docs/FORK.md`, *Hooks in
   upstream's files*). The feature changes what Settings draws, the status lines `banana-sync` writes, the menu's
   wakes and `sync.log`. *Decided*: a setting, default Off, as above. Its row needs one more hook in upstream's
   `ui/src/settings.cpp`: `Kiwami::rows(*this, HS)` in the Home server page's rows (`:805-825`), placed by the
   fork's `Kiwami::place`. `banana-sync` reads the same key from `settings.conf` through `doSync`'s `get()`
   (`ui/src/sync.cpp:1677-1678`), and each of its hook lines is conditional on it.

7. **Many status writes in the new phases.** The card check runs once per file in the list (thousands); a status
   write is a file write and a rename on tmpfs (`status()`, `:59-66`), skipped only when the line is the same.
   Upstream's Saves phase writes once per file too. *Proposed*: the new phases write at most about four times a
   second (and at their end), so a large library doesn't add seconds of writes.

## Suggestions

1. **Phase times in `sync.log`.** At the end of each sync's report, after `done:` and #22's `memory:` line
   (`ui/src/sync.cpp:1736-1737`), one line per phase with its duration and count. It helps the device test and later
   reports; nothing on screen. With the setting Off, the report stays `main`'s. *Decided*: in.

2. **Files' wording for the line.** Upstream's Files page already shows `· 3 min left` (`ui/src/settings_files.cpp:1009`).
   The same `duration()` style keeps the menu consistent. *Decided*: in (the format under *The request*).

3. **The row's value or the Last sync help with the times.** Considered in round 1 and left out: the user wants the
   line under the bar only.

## Current state

**What runs.** `banana-sync` is upstream's (`ui/src/sync.cpp`), built from `sync.cpp` and `json.cpp` alone, plus the
fork's header-only `ui/src/kiwami/syncmem.h` (#22: `ui/Makefile:41` rebuilds it when that changes). Settings starts it
detached through `hsRun("sync")` (`ui/src/settings.cpp:6309-6322, 6342-6351`); the Wi-Fi script and leaving a game
start it too (`--auto wifi|quit`). Only one runs at a time (`sync.lock`, `ui/src/sync.cpp:1834-1835`).

**The status line.** `status()` (`:59-66`) writes one line to `/run/banana/sync.status` (tmpfs), atomically, skipping
an unchanged line. While syncing it is `running <a> <b> <what>|<item>`. Settings parses it in `syncBar()`
(`ui/src/settings.cpp:6441-6455`): a bar `a` of `b`, `what` as the title (17 px, at `y+26`), `item` under it (16 px, at
`y+52`). It's drawn at `(50, 330)`, 540 wide, on the top screen when the selected row is Sync now and `syncing()`
(`:2353-2357`); the hero box ends at 456, so a line at `y+78` (408) fits. Settings reads the status only while open
(`update()`, `:2511`).

**The phases, in order** (`doSync`, `ui/src/sync.cpp:1674-1738`):

| Phase | Status today | Where |
|---|---|---|
| Starting: the server's platforms (RomM) or its manifest (Romhost, one request) | `running 0 0 \|<server>` (title "Starting") | `:1698`; Romhost `:737-745`; RomM `:891` |
| Listing games (RomM, the fork's, #22) | `running <read> <expected> Listing games\|<system> <n> / <total>` | `:921`, `syncmem.h` `Listing` |
| Checking the card: the list against the card, hashing adopted and changed files | **none** | `syncRoms`, `:1296-1340` |
| Games: downloads | `running <MB> <MB total> Games <i>/<N>\|<file>`, per MB through curl's progress | `:1385-1414` |
| Removing: files that left the server, each hashed first | **none** | `:1416-1430` |
| Gamelists: `gamelist.xml` per system and card, and Romhost's report | **none** | `:1431-1490` |
| Reading saves: RomM's save (and state) list, one request per platform; Romhost: each local save hashed, then the plan | **none** | RomM `:1074-1093`; Romhost `:799-822` |
| Saves | `running <n> <total> Saves\|<file>` | RomM `:1100`, Romhost `:830` |
| States (RomM, Save states On) | `running <n> <total> States\|<file>` | `:1100` |
| Done | `done <counts>\|<note>` | `:1735` |

A saves-only sync (the Games row on Saves only, or leaving a game) skips the four games phases; on RomM it still
lists the games, which the saves are matched to by name (`:1053`).

**Clocks.** banana-ui's `nowMs()` is `CLOCK_MONOTONIC` (`ui/src/main.cpp:81-84`), and so is the `t` Settings gets.
`banana-sync` keeps no time today, apart from wall-clock UTC in the report's first line (`:106-110`).

**What already exists to build on.** The fork's Settings hooks (`ui/src/kiwami/kiwami.h:29-57`): `rows()` adds rows
to a group, `wake()` joins `nextWake()`, and `Kiwami` is a nested struct of `Settings`, so it can read `syncLine` and
`syncing()` and draw with its palette (`Kiwami::colours`). `syncmem.h` already holds the fork's `banana-sync` code as a
header and is included by `sync.cpp` (`:26`). Upstream Files' `duration()` (`ui/src/settings_files.cpp:50-55`) is the
style for the time left; it's in an anonymous namespace, so the fork writes its own.

**Upstream.** `git diff main...kiwami -- ui/src/sync.cpp` is #22's patch only; `main` is upstream's `803e61c`
(`git ls-remote upstream`), with no newer change. `ui/src/sync.cpp` changed in 8 upstream commits between 26 Oct and
1 Nov; `ui/src/settings.cpp` in about 60 in four days. Upstream's `ui/tests/sync.sh` checks the status only for
`running*` and `error stopped` (`:158-161`); the fork's `kiwami/tests/romm-list.sh` checks the `Listing games` lines
exactly (`:104-107`), which stay the same. No test renders a sync in progress (`ui/tests/fake-sync.sh` is done at
once).

## What has to change

- **`ui/src/kiwami/synctimes.h`** (new, header-only, included by `sync.cpp` beside `syncmem.h`; a `kiwami:`
  prerequisite in `ui/Makefile` as for `syncmem.h`): the sync's and the current phase's start (`CLOCK_MONOTONIC`), the
  phase taken from each `running` status line's title, the counts the ETA needs (the Games phase's bytes and files),
  written atomically to `/run/banana/sync.times` with the pid; each phase's duration kept for the report. Also the
  formatting shared with the menu (elapsed, time left, the ETA's rule).
- **`ui/src/sync.cpp`** (upstream's, `kiwami:` lines, each conditional on the setting): a call in `status()` that
  feeds the times; a status for each new phase (Checking the card, Removing, Gamelists, Reading saves on both
  backends), throttled; the setting read in `doSync`; the phase lines after `memory:`. About 8-10 lines.
- **`ui/src/kiwami/`, the menu**: the line under the bar (a `kiwami:` call in `syncBar()`, `ui/src/settings.cpp:6455`,
  one line); `Kiwami::wake()` waking for a running sync; the setting's row through `Kiwami::rows`.
- **`ui/src/settings.cpp`**: two hook lines: `Kiwami::rows(*this, HS)` in the Home server page and the call in
  `syncBar()`.
- **The setting**: a key in `settings.conf` (the name for the plan, e.g. `kiwami_sync_times`), default `off`, absent
  from upstream's `system/rootfs/etc/banana/settings.conf` (a missing key is Off).
- **`kiwami/tests/`**: unit tests of the times (phases, ETA rule, formatting), a `banana-sync` run against a stand-in
  checking each phase's status and the times file, the Off case the same as `main`'s, and a headless render of the
  line at 640x480 and 1024x768.

No package, no dependency (`kiwami/DEPENDENCIES.md` unchanged), nothing in the rootfs.

## Options

1. **`banana-sync` writes the times, the menu computes elapsed and ETA live (recommended).** `banana-sync` notes each
   phase's start and counts in a small side file; the menu reads it with the status and works out the clock every
   second, so it ticks during a long request. Right after a menu restart or an automatic sync; the status line's
   format and upstream's parser stay as they are.
2. **All in the status line.** The times appended to `what` or `item` as text. No side file, but it ticks only when
   `banana-sync` writes (frozen during a long request or a large page), and upstream's `syncBar()` would draw the extra
   text with the setting Off unless parsed out. Rejected.
3. **All in the menu.** The menu notes when it sees each phase begin. No change to `banana-sync`'s times, but wrong
   whenever Settings was closed at a phase's start (it reads the status only while open, `ui/src/settings.cpp:2511`)
   or the sync was automatic. Rejected (the user wants the times from `banana-sync`).

The ETA, in all of them: the phase's average rate since it began (stable, no jumps); shown once the phase has run a
few seconds (about 3 s, for the plan) and something is done; for Games, the larger of the bytes and the files
estimates.

## Risks

- **Performance and footprint**: Settings redraws its top screen once a second while a sync runs and the line is on
  screen (only with the setting On); the frame is a short diff. `banana-sync` adds throttled tmpfs writes; nothing
  runs while idle; the rootfs gains a few KB of code.
- **Conflicts with upstream**: `ui/src/sync.cpp` (8 upstream commits in a week) gains about 8-10 `kiwami:` lines in
  the phases' loops, which a change there could move; `ui/src/settings.cpp` (very busy) two lines, in the Home server
  page's rows and at the end of `syncBar()`. A merge that renames a phase's title or adds a status of its own needs a
  look at the phase detection.
- **The phase detection follows the titles' text**: a status title that changes upstream (say `Games` reworded) would
  start a new phase with a new name; harmless (the times reset), and the fork's test of the statuses would show it.
- **Hardware and boards**: nothing board-specific; the line is laid out in the 640x480 logical frame and scaled
  (`X()`) on the RG DS Plus's 1024x768. Only the RG DS Plus is at hand to test.
- **Strings**: the fork's strings aren't in `ui/assets/lang/*.tsv` (neither is #22's `Listing games`), so the line
  and the new phase titles show in English with the menu in Russian or Chinese.

## Testing

- **Headless, units** (`kiwami/tests/`): the phase changes, the elapsed and time-left formatting (`0:05`, `59:59`,
  `1:00:00`; `45 s`, `3 min`, `1 h 5 min`), the ETA's rule (none before the threshold or with nothing done; the larger
  of the two for Games), the stale-pid check.
- **Headless, `banana-sync`** against a stand-in (the fork's `kiwami/tests/romm-big.py`, or upstream's
  `ui/tests/fake-romm.py`): a sync with the setting On writes each phase's status in order and the times file; with
  it Off, the status lines and `sync.log` are the same as `main`'s build (`git worktree` of `main`, local).
  `kiwami/tests/romm-list.sh` and `make test` pass unchanged.
- **Headless, the menu**: Settings on Home server > Sync now with a stand-in `BANANA_SYNC` that leaves a `running`
  status and a times file: a `shot` at both sizes shows the line, a second `shot` after `wait 2000` shows the clock
  moved, and the wake logged by `shot` (`ui/src/main.cpp:1318`) is under a second while syncing and as `main`'s with the
  setting Off.
- **On the device (RG DS Plus, the user's RomM)**: the setting On, a full Sync now with Settings left open on Sync
  now: the clock ticks, each phase shows with its own time and an ETA that settles; Settings closed and reopened
  mid-sync shows the right elapsed time; a sleep with the power button during the Saves phase. To collect:
  `/data/banana/logs/sync.log` (the phase lines), over SFTP or from the card, and photos of the line at a few points.

## Open questions

1. Where the row's elapsed time and ETA show? *Answered*: one line under the bar only.
2. Which phases? *Answered*: every phase, the silent steps made phases of their own.
3. The whole sync's time too? *Answered*: phase and total.
4. An ETA with no rate? *Answered*: elapsed only until there is one.
5. A setting? *Answered*: yes, default Off (after weighing its cost: one hook line, about 15 fork lines, a test).
6. The Games ETA? *Answered*: the larger of the bytes and files estimates.
7. Sleep in the elapsed time? *Answered*: no, awake time only (`CLOCK_MONOTONIC`).
8. Additions? *Answered*: phase times in `sync.log`, and Files' style for the line.
9. The row's label, help and place on the Home server page (proposed: "Sync times", Off/On, after Sync now, where it
   is read), and the setting's key name. *Answered* (planning): **Show sync times**, Off/On, right after Sync now,
   key `kiwami_sync_times`.
10. The ETA's threshold (proposed: 3 s and something done) and the throttle of the new phases' writes (proposed:
    four a second). *Answered* (planning): 3 s and one unit done; at most one write every 250 ms, plus each phase's
    first and last item.
11. How the headless test gets a sync to show, since upstream reads the fixed `/run/banana/sync.status`? *Answered*
    (planning): a test-only `KIWAMI_SYNC_RUN=<dir>` in the environment; the fork's code reads the status, pid and
    times from there and puts the status into Settings' `syncLine`.
12. When Settings wakes for the clock? *Answered* (planning): only while the Sync now row is selected (where the line
    shows); elsewhere as upstream.
