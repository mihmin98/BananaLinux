# 0024 Sync times: plan

Issue: #24 · Branch: `k/0024-sync-eta` · Investigation: [investigation.md](investigation.md) · Status: approved

## Summary

With **Network > Home server > Show sync times** On, a Home server sync shows one line under Sync now's bar on the
top screen: the phase, how long it has run, its time left once there is a rate to go on, and the whole sync's time,
in upstream Files' style: `Games · 2:10 · 3 min left · sync 6:40`. Settings wakes for it while the Sync now row is
selected, so the clock ticks every second and upstream's bar moves live. The steps that show nothing today become
phases with a count of their own (Checking the card, Removing, Gamelists, Reading saves / states). Each phase's
duration goes at the end of the sync's report (`banana/logs/sync.log`). With the setting Off (the default; a missing
key is Off), everything is as `kiwami` is today: the same status lines, no times file, no new wakes, the same report.

The approach is the investigation's option 1:

- **`banana-sync` keeps the times** in a header of the fork's, `ui/src/kiwami/synctimes.h`, which `sync.cpp`
  includes beside #22's `syncmem.h`. A `kiwami:` line in upstream's `status()` gives it every status line. When a
  `running` line's title starts a new phase, it writes `<run>/sync.times`
  (`<pid> <sync start> <phase start> <phase>`, `CLOCK_MONOTONIC` ms, so time asleep doesn't count) before the status
  file, and keeps the phase's duration for the report. The new phases are one `kiwami:` line each in the loops that
  run silently today. Each line writes a throttled status: at most every 250 ms, plus its first and last item.
- **The menu works the clock out live.** The same header holds the formatting and the time-left rule. A new file of
  the fork's, `ui/src/kiwami/sync-times.cpp`, reads `sync.times` and `sync.pid` when the status changes, builds the
  line in `Kiwami::update`, draws it from a `kiwami:` call at the end of upstream's `syncBar()`, and wakes Settings
  through the fork's existing `Kiwami::wake` hook.
- **The hooks**: 9 `kiwami:` lines in `ui/src/sync.cpp`, 2 in `ui/src/settings.cpp` (the row, and the call in
  `syncBar()`), and #22's existing `ui/Makefile` line widened to the new header.

## Acceptance criteria

- **A1** (Off is unchanged): given `kiwami_sync_times` missing or `off` in `settings.conf`, when any sync runs (Sync
  now, saves-only, `--auto`), then `banana-sync`'s status lines and `sync.log` are the same as `kiwami`'s build before
  this feature (apart from the report's timestamp and the peak memory). No `sync.times` is ever written. With Settings
  on Sync now during a sync, no line is drawn under the bar and no `sync times:` line is logged.
- **A2** (the row): Network > Home server has a row **Show sync times** (Off / On), right after Sync now. Changing it
  writes `kiwami_sync_times = on` / `off` to `banana/settings.conf`. Its help says what the line shows and that the
  report gets each step's time.
- **A3** (the phases): given the setting On, when a RomM sync with games runs, then the status titles come in this
  order: `Starting` (empty title), `Listing games`, `Checking the card`, `Games`, `Removing` (only when files left
  the server), `Gamelists`, `Reading saves`, `Saves`. With Save states On, `Reading states` and `States` (`States`
  only when there is a state) follow. A saves-only sync goes `Starting`, `Listing games`, `Reading saves`, `Saves`.
  On Romhost: `Starting`, `Checking the card`, `Games`, `Removing`, `Gamelists`, `Reading saves`, `Saves`. A phase
  with nothing to go through (no file to remove, no local save) writes no status.
- **A4** (the new phases' statuses): each new phase writes `running <n> <total> <phase>|<item>`, where `n` is the
  item in hand counted from 1 (as upstream's Saves does) and `item` is the file's name, or the system's folder for
  Gamelists and Reading saves on RomM. It writes its first and last item, and in between at most once every 250 ms.
- **A5** (the times file): given the setting On, while a sync runs `<run>/sync.times` holds one line,
  `<banana-sync's pid> <sync start> <phase start> <phase>`, in `CLOCK_MONOTONIC` milliseconds. It is written when a
  phase starts, before that phase's first status, and removed when the sync ends (`done`, or an `error`). A sync
  killed leaves it behind; A8 covers that.
- **A6** (the line): given the setting On, a sync running, and Settings on the Home server list with Sync now
  selected, then under the bar (at `y + 78`, 16 px, the muted colour, fitted to the bar's width) it shows
  `<phase> · <phase elapsed> · <time left> left · sync <sync elapsed>`, or without `· <time left> left` when there is
  no estimate (A7). Elapsed is `m:ss`, or `h:mm:ss` from an hour. The time left is in Files' `duration()` style
  (`45 s`, `3 min`, `1 h 5 min`). It fits inside the top screen's box at 640x480 and at 1024x768.
- **A7** (the estimate): the time left is shown only once the phase has run 3 s and at least one unit is done. It is
  the phase's elapsed time × (total − done) / done:
  - `Listing games`: games read of the expected total.
  - `Games`: the larger of the MB estimate (the bar's MB) and the files estimate (`Games i/N`: `i − 1` done of `N`).
  - Every other counted phase: `n − 1` items done of `total`.
  - A phase with a total of 0 (`Starting`, or `Listing games` without RomM's `rom_count`): no estimate.
- **A8** (stale times): the line shows only while the status is `running`, `sync.times`' pid is the one in
  `<run>/sync.pid`, and its phase is the status's phase. So a times file left by an earlier or killed sync, or the
  moment between Sync now and `banana-sync`'s first status (Settings' own `running 0 0 |starting`), shows no line.
- **A9** (the clock ticks): given the setting On, a sync running and Sync now selected, Settings wakes at the next
  status read (every 500 ms) and at the next whole second of either clock, so the line and upstream's bar move without
  a key press. On any other row or page, or with the setting Off, Settings wakes as upstream does.
- **A10** (right whenever it's opened): Settings opened partway through a sync, or during an automatic one (Wi-Fi
  connecting, leaving a game), shows the times since the sync's and the phase's real starts.
- **A11** (sleep): time the handheld spends asleep during a sync isn't counted in either clock, and the estimate after
  a resume is from the awake rate.
- **A12** (the report): given the setting On, `sync.log` ends, after `memory:`, with one line per phase in order,
  `times: <phase> <seconds, one decimal> s`, plus `, <n> of <total>` for a counted phase and, for Games,
  `, <MB> of <MB> MB, <i> of <N> files`. A last line reads `times: sync <seconds> s`. With the setting Off, there are
  no `times:` lines (A1).
- **A13** (upstream's and the fork's tests): `make test` (upstream's `ui/tests/sync.sh` included) and
  `kiwami/tests/romm-list.sh` pass unchanged.

## Risks

- **Performance and footprint.** With the setting On and Sync now selected during a sync, Settings wakes about
  twice a second and redraws its top screen when the text changes. The frame is a diff of one line, under a
  millisecond as upstream's frames are. `banana-sync` adds a `clock_gettime` per item in the new phases (vDSO, tens of
  ns) and at most four tmpfs writes a second. The times file is written only at a phase's start. Nothing runs while
  idle, and with the setting Off nothing runs at all. The rootfs gains a few KB of code in two binaries, and no file.
  banana-ui's log gets one `sync times:` line when the line's shape changes (a new phase, or the estimate appearing),
  not every second, because the log is on the card. Guarded by the headless frame times (`shot`'s
  `frame ... ms`) and the M1 device run.
- **Conflicts with upstream.** `ui/src/sync.cpp` (8 upstream commits in a week) gets 9 one-line hooks. Five sit at the
  top of a loop's body (`want`, `gone`, `m.systems`, RomM's platforms, Romhost's local saves), and an upstream change
  to one of those loops conflicts there or moves it. `ui/src/settings.cpp` gets 2 lines: one beside the Sync now row's
  `add`, and one at the end of `syncBar()`. `banana-merge-upstream` sees each conflict. The silent cases are a title
  reworded upstream (it would start a phase with the new name: harmless) and a new silent step (no time of its own);
  `sync-times.sh`'s exact phase sequence (A3) fails on either. If upstream adds Home server to `ORDER`, the fork's row
  goes after every row `ORDER` names; the `docs/FORK.md` note (step 2) says to add a `FOLLOWS` entry then.
- **The test hook.** `KIWAMI_SYNC_RUN` puts the test's status into Settings' `syncLine`. It is read only from the
  environment, which the device's session script doesn't set. Unset, the code takes the `/run/banana` path and
  touches nothing of upstream's.
- **Romhost isn't run by the fork's tests.** Upstream's `sync.sh` runs the real Romhost only when its source is beside
  the tree (CI skips it), and the user syncs with RomM. Romhost's one new phase (`Reading saves`, its hash loop) and
  the shared phases go through the same `tick()` the RomM test exercises. The risk is a wrong item string, which only
  changes the text under the bar.
- **The estimate is an average rate.** The phase's average since it began doesn't jump, but it lags a change of
  speed, such as a slow ISO after many covers. Taking the larger of the MB and files estimates leans long. M1 shows
  how it settles on the user's library.
- **Boards.** Nothing board-specific. The line is laid out in the 640x480 frame and scaled at 1024x768 (shots at
  both). Only the RG DS Plus is at hand; the RG DS runs the same binaries.
- **Strings.** The new phase titles and the line are in English only, as #22's `Listing games` is: the fork's strings
  aren't in `ui/assets/lang/*.tsv`.

## Steps

Each step is one commit, and leaves the tree building and `make test` passing.

### 1. banana-sync keeps each phase's time, with the silent steps as phases

- **Files**:
  - `ui/src/kiwami/synctimes.h` (new)
  - `ui/src/sync.cpp` (hook, 9 `kiwami:` lines)
  - `ui/Makefile` (hook: #22's line 41 widened)
  - `kiwami/tests/synctimes-units.cpp` (new)
  - `kiwami/tests/sync-times.sh` (new)
  - `kiwami/tests/romm-big.py` (changed)
  - `kiwami/tests/ci.sh` (changed)
  - `docs/FORK.md` (changed)
  - `.workspace/0024-sync-eta/plan.md` (changed, Progress)
- **Change**:
  - **`ui/src/kiwami/synctimes.h`**: header-only (`banana-sync` is built from `sync.cpp` and `json.cpp` alone, as for
    `syncmem.h`), `#pragma once`, namespace `banana::kiwami::synctimes`. Its header comment covers what it is for
    (Home server > Show sync times: each phase's start for the menu's line, and each phase's duration for the report),
    the times file's format, and that the phases follow the status titles. Includes `<cstdio>`, `<ctime>`,
    `<string>`, `<vector>`, `<unistd.h>`.
    ```cpp
    inline long long monoMs();						// CLOCK_MONOTONIC in ms: time asleep isn't counted (as banana-ui's nowMs())
    // a status line's phase: "running <a> <b> <what>|<item>" -> what, without a trailing " <i>/<N>" ("Games 3/10" is
    // Games), "Starting" for an empty what (as Settings draws it); "" for a line that isn't running
    inline std::string phaseOf(const std::string& status);
    constexpr int TICK_MS = 250;					// the new phases' statuses: at most one every TICK_MS, and their first and last
    struct Phase { std::string name, what; long long ms = 0, a = 0, b = 0; };	// a phase over: its last status's title and counts
    struct Tracker
    {
        bool on = false;							// kiwami_sync_times = on, for this sync
        std::string run;							// banana-sync's --run folder: sync.times goes there
        long long syncStart = 0, phaseStart = 0;
        std::string phase, what;					// the phase in hand, and its last status's title
        long long a = 0, b = 0;						// and counts
        std::vector<Phase> done;
        bool ended = false;
        std::string tickPhase; long long tickN = 0, tickAt = 0;
    };
    inline Tracker& tracker();						// the one tracker (a function's static: header-only)
    inline void start(bool on, const std::string& run);	// doSync: a fresh tracker; the sync's clock starts
    inline void seen(const std::string& status);	// status()'s hook: each line written
    inline void tick(void (*status)(const std::string&), const char* phase, size_t total, const std::string& item);
    inline std::vector<std::string> report();		// the report's times: lines; none when off
    ```
    - `start(on, run)`: resets the tracker, sets `on` and `run`, and sets `syncStart = monoMs()`. It writes nothing.
    - `seen(s)`: nothing when off or ended. For a `running` line: if `phaseOf(s)` differs from `phase`, it closes the
      phase in hand (if any) into `done` (`ms = now − phaseStart`, its last `what`, `a`, `b`). Then it starts the new
      one (`phaseStart = now`) and writes `run + "/sync.times"` atomically (`.new`, then `rename`) as
      `"%d %lld %lld %s\n"`: `getpid()`, `syncStart`, `phaseStart`, the phase. In every case it keeps the line's
      `what`, `a` and `b`. For any other line (`done ...`, `error ...`), it closes the phase in hand, unlinks
      `sync.times` and sets `ended`.
    - `tick(status, phase, total, item)`: nothing when off. When `phase` isn't `tickPhase`, it starts counting again
      (`tickN = 0`). It counts the item (`tickN++`). It calls `status("running " + tickN + " " + total + " " + phase +
      "|" + item)` when `tickN == 1`, when `tickN == total`, or when `TICK_MS` has passed since its last write. The
      status goes through upstream's `status()`, so `seen()` takes the phase from it.
    - `report()`: empty when off. Otherwise one line per entry of `done`: `times: <name> <ms/1000, %.1f> s`, then
      `, <a> of <b>` when `b > 0`, and for `Games`, `, <a> of <b> MB, <i> of <N> files` (from its last `what`). Then
      `times: sync <(now − syncStart)/1000, %.1f> s`.
  - **`ui/src/sync.cpp`** (line numbers at `6c7cd6e`). Each new line ends in a `kiwami:` comment, with no other
    change around it. `kt` below stands for `banana::kiwami::synctimes` written out in full.
    1. after `:26` (`#include "kiwami/syncmem.h"`):
       `#include "kiwami/synctimes.h"	// kiwami: each phase's time (Home server > Show sync times)`
    2. in `status()`, after `:62` (`statusLine = s;`), so the times file is written before the status file:
       `kt::seen(s);	// kiwami: a phase's start, for the menu's line and the report`
    3. in `doSync`, after `:1678` (`auto get = ...`):
       `kt::start(get("kiwami_sync_times", "off") == "on", RUN);	// kiwami: Show sync times`
    4. in `syncRoms`, after `:1299` (`const MFile& f = *kv.second;`, the top of the `want` loop, before its
       `continue`s): `kt::tick(status, "Checking the card", want.size(), baseOf(f.path));	// kiwami:`
    5. at the top of the `gone` loop (`:1418`):
       `kt::tick(status, "Removing", gone.size(), baseOf(rel));	// kiwami:`
    6. at the top of the gamelists' `for (auto& s : m.systems)` loop (`:1434`; not the `games` command's at `:1808`):
       `kt::tick(status, "Gamelists", m.systems.size(), s.folder);	// kiwami:`
    7. at the top of RomM's `sync()` platforms loop (`:1072`):
       `kt::tick(status, kind == "save" ? "Reading saves" : "Reading states", platforms.size(), pf.second);	// kiwami:`
    8. at the top of Romhost's `saves()` hash loop (`:804`):
       `kt::tick(status, "Reading saves", local.size(), baseOf(kv.first));	// kiwami:`
    9. after `:1737` (the `memory:` line):
       `for (auto& l : kt::report()) say("%s", l.c_str());	// kiwami: each phase's time (Show sync times)`

    `status` and `baseOf` are upstream's, in the anonymous namespace above every hook. `status` is passed as a
    function pointer, so the header needs nothing of `sync.cpp`'s.
  - **`ui/Makefile`**: #22's line `banana-sync: src/kiwami/syncmem.h	# kiwami: ...` becomes
    `banana-sync: src/kiwami/syncmem.h src/kiwami/synctimes.h	# kiwami: the fork's headers sync.cpp includes`
    (the same hook, no new line). banana-ui already depends on every `src/kiwami/*.h` (`:19`).
  - **`kiwami/tests/romm-big.py`**: with `ROMM_BIG_SLOW=<game id>` in the environment, that game's
    `/api/roms/<id>/content/...` answer waits 3 s first, so the test can read `sync.times` during the Games phase. The
    usage comment says so. Without it, nothing changes (`romm-list.sh` doesn't set it).
  - **`kiwami/tests/synctimes-units.cpp`**, in `syncmem-units.cpp`'s `CHECK` / `EQ` style:
    - `phaseOf`:
      - `running 0 0 |My RomM` is `Starting`.
      - `running 5 20 Games 3/10|Game 0003 (USA).gba` is `Games`.
      - `running 10 180 Listing games|gba 10 / 30` is `Listing games`.
      - `running 1 6 Reading saves|gba` is `Reading saves`.
      - `done 1 0 0 0 0 0|` and `error stopped` are `""`.
    - The tracker, on a temporary run folder:
      - `start(true, dir)` writes no file.
      - `seen("running 0 0 |x")` writes `sync.times` with `getpid()`, two equal numbers, and `Starting`.
      - `seen("running 1 5 Checking the card|a")` rewrites it with the same sync start, a phase start not earlier than
        before, and `Checking the card`.
      - `seen("running 2 5 Checking the card|b")` leaves the file's text as it was.
      - `seen("done 0 0 0 0 0 0|")` removes it.
      - `report()` is three lines: `times: Starting <x> s`, `times: Checking the card <x> s, 2 of 5`,
        `times: sync <x> s`. Each `<x>` is a number with one decimal.
      - A Games phase's last `running 812 812 Games 120/120|z` reports as `times: Games <x> s, 812 of 812 MB, 120 of
        120 files`.
    - **Off**: after `start(false, dir)`, `seen()` and `tick()` write nothing (no file, no status call), and
      `report()` is empty.
    - **The throttle**: 1000 `tick()`s in a tight loop, with a status function that records its lines, record
      `running 1 1000 X|i1` first and `running 1000 1000 X|i1000` last, and at most 4 lines. A tick after `TICK_MS`
      (a `usleep(260000)`) is recorded. A new phase's first tick is recorded at once, counted from 1.
  - **`kiwami/tests/sync-times.sh`** (`set -e` style as `romm-list.sh`, temp folder `$T`, a trap killing the
    stand-in):
    1. It builds `$T/banana-sync` as `romm-list.sh` does, and `$T/synctimes-units` from
       `kiwami/tests/synctimes-units.cpp` (`-std=c++17 -Wall -Werror -I ui/src`), and runs the units.
    2. It starts `romm-big.py` with 30 games a system and 0 achievements, with `ROMM_BIG_SLOW=200012` (GBA's game 12).
       It makes card A paired with it as `romm-list.sh`'s `card()` does, with `settings.conf`
       `kiwami_sync_times = on` and `homesync_states = on`. It runs `bs -v sync` there in the background, stderr to a
       file.
    3. **The times file mid-sync** (A5): it waits (up to 15 s) until `run/sync.times` ends in ` Games`, then checks
       that its pid is `run/sync.pid`'s and that the sync's start ≤ the phase's start. After the sync, the file is
       gone.
    4. **The phases** (A3): an awk program takes the `status:` lines, maps each `running` line to its phase as
       `phaseOf` does, and joins consecutive distinct ones. The result has to be exactly
       `Starting,Listing games,Checking the card,Games,Gamelists,Reading saves,Saves,Reading states`, followed by the
       `done` line. Otherwise it fails with `sync-times: phases <got>, not <expected>`. (`romm-big.py` has no states,
       so there is no `States` phase.)
    5. **The new phases' lines** (A4): `Checking the card`'s first line is `running 1 <W> Checking the card|...` and
       its last `running <W> <W> Checking the card|...`, with fewer than `W` lines in all (W from the first line). The
       same holds for `Gamelists` with `W` = 6, the six systems.
    6. **The report** (A12): `banana/logs/sync.log`'s lines after `memory:` are a `times:` line per phase in the
       order of 4, each matching `^times: <phase> [0-9]+\.[0-9] s`. `Listing games` ends in `, 180 of 180`, and
       `Games` has `MB,` and `files`. The last is `times: sync <x> s`.
    7. **Removing** (A3): it restarts the stand-in with 25 games (no slow game) and syncs card A again. The phases are
       `Starting,Listing games,Checking the card,Games,Removing,Gamelists,Reading saves,Saves,Reading states` (Games:
       game 11's six files, which fail on their null size, tried again; see *Deviations*), and `Removing`'s last line
       is `running <R> <R> Removing|...`.
    8. **Saves only** (A3): `bs -v sync --saves-only` on card A gives `Starting,Listing games,Reading saves,Saves,
       Reading states`.
    9. **Off** (A1): card B without the key, the same two syncs. The `status:` lines contain no `Checking the card`,
       `Removing`, `Gamelists`, `Reading saves` or `Reading states`. `sync.log` has no `times:` line, and
       `run/sync.times` doesn't exist (also polled during the slow game).
    10. `--compare <other banana-sync>` (local, for the final check, as `romm-list.sh --compare`): 9's two syncs also
        run with the other build on a card C of its own. The `status:` lines must be the same, and so must the two
        `sync.log`s, apart from the first line (the time) and the number on `memory: peak`.
    11. It prints `sync-times: all checks passed`.
  - **`kiwami/tests/ci.sh`**: a block after `romm-list.sh`'s:
    `# Home server > Show sync times: banana-sync's phases, its times file and the report's times` and
    `kiwami/tests/sync-times.sh`.
  - **`docs/FORK.md`**: in *Taking in upstream*, step 3, after the paragraph on RomM's game list, a few sentences on
    the times. `ui/src/sync.cpp`'s `status()` hands each line to `synctimes::seen()`, which starts a phase on each new
    title. The silent steps get a `tick()` line at the top of their loops (`want`, `gone`, the gamelists,
    RomM's and Romhost's saves listing). A title upstream rewords, or a new step that writes no status, shows in
    `kiwami/tests/sync-times.sh`'s phase list, which then needs the new name.
- **Tests**: `kiwami/tests/sync-times.sh` (new, all of the above), `kiwami/tests/romm-list.sh` (unchanged, passes),
  `make test` (upstream's `sync.sh` against the changed `sync.cpp`), `make -C ui`, `kiwami/tests/ci.sh .buildroot`.
- **Commit**: `Sync times: banana-sync keeps each phase's time, with the silent steps as phases`

### 2. The times under Sync now's bar, and the Show sync times row

- **Files**:
  - `ui/src/kiwami/synctimes.h` (changed)
  - `ui/src/kiwami/sync-times.cpp` (new)
  - `ui/src/kiwami/kiwami.h` (changed)
  - `ui/src/kiwami/kiwami.cpp` (changed)
  - `ui/src/settings.cpp` (hook, 2 `kiwami:` lines)
  - `kiwami/tests/synctimes-units.cpp` (changed)
  - `kiwami/tests/sync-times-ui.sh` (new)
  - `kiwami/tests/ci.sh` (changed)
  - `docs/FORK.md` (changed)
  - `.workspace/0024-sync-eta/plan.md` (changed, Progress)
- **Change**:
  - **`ui/src/kiwami/synctimes.h`**, the menu's side, after the tracker:
    ```cpp
    struct Times { long long pid = 0, syncStart = 0, phaseStart = 0; std::string phase; };
    inline bool parse(const std::string& line, Times& t);	// sync.times' line; false when it isn't one
    inline std::string clock(double ms);			// elapsed: 0:05, 59:59, 1:00:00, 1:02:03
    inline std::string left(double s);				// Files' duration(): 45 s, 3 min, 1 h 5 min
    constexpr double ETA_AFTER_MS = 3000;			// no estimate before the phase has run this long
    inline double secondsLeft(const std::string& status, double phaseMs);	// A7's rule; -1 none
    inline std::string line(const std::string& status, const Times& t, double now);	// A6's line; "" when A8 says none
    inline int nextTick(const Times& t, double now);	// ms to the next whole second of either clock
    ```
    - `clock`: floors to whole seconds. `m:ss` under an hour, `h:mm:ss` from one. A negative value is `0:00`.
    - `left`: the same text as `ui/src/settings_files.cpp:50-55`'s `duration()` (`(int)s + 1` seconds under a
      minute, `(int)(s / 60) + 1` minutes under an hour, then `h` and `min`). It is copied, because Files' is in an
      anonymous namespace. The estimate is capped at 359999 s, as Files caps it.
    - `secondsLeft`: the rule in A7. It parses `a`, `b` and `what` as `syncBar()` does (`Games i/N` for the files).
      An estimate needs `done ≥ 1` and `total > 0`; `done ≥ total` gives 0.
    - `line`: `""` unless `phaseOf(status)` is non-empty and equals `t.phase`. Otherwise it returns
      `phase + " · " + clock(now − phaseStart) + (left ≥ 0 ? " · " + left(s) + " left" : "") + " · sync " +
      clock(now − syncStart)`. The `·` is `"\xc2\xb7"`, as Files writes it.
    - `nextTick`: `min` over the two starts of `1000 − (now − start) mod 1000`, at least 1.
  - **`ui/src/kiwami/kiwami.h`**: in the hooks, `void syncTimes(float x, float y, float w);	// syncBar(): the line under
    Sync now's bar`. In the private part, Show sync times' members, with a comment pointing to `sync-times.cpp`:
    ```cpp
    bool timesOn() const;						// the Show sync times row is On
    bool timesShown() const;					// on, the list on Sync now, a sync running: where syncBar() draws
    std::string timesRun() const;				// KIWAMI_SYNC_RUN, else /run/banana
    bool timesUpdate(double t);
    int timesWake(double t) const;
    std::string timesSeen;						// the status line sync.times was last read for
    kiwami::synctimes::Times times;
    bool timesValid = false;					// A8: the file's pid is sync.pid's
    std::string timesText, timesShape;			// the line, and its shape last logged
    ```
    The file's header comment gets a line for Network > Home server > Show sync times.
  - **`ui/src/kiwami/sync-times.cpp`** (new; `#include "kiwami.h"` and `"synctimes.h"`):
    - `timesOn()`: the `st.items` entry with key `kiwami_sync_times` has the value `on` (the row's state as upstream
      loads and saves it; `settings.conf` isn't read every frame).
    - `timesShown()`: `timesOn() && st.mOpen && st.page == LIST && !st.inGroups && st.langAsk < 0 && st.sel` is in
      range, `st.items[st.sel].key == "hs_sync"` and `st.syncing()`. This is `render()`'s own condition for the bar
      (`ui/src/settings.cpp:2329-2353`).
    - `timesUpdate(t)`:
      1. **Test only**: with `KIWAMI_SYNC_RUN` set, it reads `<dir>/sync.status`'s first line into `st.syncLine` on
         each update, since upstream's `pollSync()` reads the fixed `/run/banana/sync.status`, which a test can't
         write. It runs after `pollSync()` (`kw->update` is the last thing in `update()`, `:2650`), so the test's
         line is the one drawn.
      2. If `!timesShown()`: clear `timesText` and the shape, return whether the text was non-empty.
      3. When `st.syncLine != timesSeen`: read `<run>/sync.times` and `<run>/sync.pid` (an `ifstream` line each). Set
         `timesValid = parse(...) && pid == sync.pid's`, and `timesSeen = st.syncLine`.
      4. `text = timesValid ? line(st.syncLine, times, t) : ""`. When the shape (the phase, and whether there is a
         time left) differs from the last one logged, and the text isn't empty, it logs
         `banana-ui: kiwami: sync times: <text>`. It returns `text != timesText` and keeps the text.
    - `timesWake(t)`: `-1` unless `timesShown()`. Otherwise the smaller of `std::max(0, (int)(st.syncNext − t))`
      (upstream's next status read) and, with `timesValid`, `nextTick(times, t)`.
    - `syncTimes(x, y, w)`: when `timesShown()` and `timesText` isn't empty,
      `st.text(st.fit(timesText, false, 16, w), false, 16, x, y, colours(st.light).muted)`.
  - **`ui/src/kiwami/kiwami.cpp`**:
    - `rows()`: a `"Home server"` branch that pushes one CHOICE item: section `Home server`, icon `phone`, label
      `Show sync times`, key `kiwami_sync_times`, file `s.data + "/settings.conf"`, names `{ "Off", "On" }`, values
      `{ "off", "on" }`. Its help: "On: while a sync runs, the line under Sync now's bar shows how long the current
      step has taken, its time left once there's a rate to go on, and the whole sync's time. Each step's time also
      goes in the sync's report (banana/logs/sync.log)." The log line becomes `rows in Home server:
      kiwami_sync_times`.
    - `update()`: `|| timesUpdate(t)`, evaluated every time (as `raUpdate()` is, into a local first).
    - `wake()`: the smaller non-negative of its current value and `timesWake(t)`.
  - **`ui/src/settings.cpp`** (line numbers at `6c7cd6e`), two hooks:
    - after the Sync now row's `add(...)` (`:807-808`), before Games':
      `Kiwami::rows(*this, HS);	// kiwami: Show sync times, after Sync now`. Home server isn't in `ORDER`
      (`:838-850`), so its rows keep the order they're added in, and the hook's position is the row's place.
      `Kiwami::place` needs no entry.
    - at the end of `syncBar()`, after the item's `text(...)` (`:6455`):
      `if (kw) kw->syncTimes(x, y + 78, w);	// kiwami: the sync's times under the bar (Home server > Show sync times)`
  - **`kiwami/tests/synctimes-units.cpp`**, more checks:
    - `clock`: 0 → `0:00`, 5400 → `0:05`, 3599999 → `59:59`, 3600000 → `1:00:00`, 3723400 → `1:02:03`, −5 → `0:00`.
    - `left`: 44.2 → `45 s`, 150 → `3 min`, 3900 → `1 h 5 min`. Each also equals the text of a copy of Files'
      `duration()` in the test, over 0 to 7200 s in steps of 0.7.
    - `secondsLeft`:
      - `Games` 5/20 MB, `3/10`, at 10400 ms: 41.6 (files 41.6 > MB 31.2).
      - `Games` 0/1 MB, `4/10`, at 6000: 14 (files only).
      - `Games` 4/10 MB, `1/10`, at 4000: 6 (MB only, no file done).
      - `Games` at 2999: −1.
      - `Listing games` 60/180 at 6000: 12. `Listing games` 60/0: −1.
      - `Saves` 1/12 at 5000: −1 (none done). `Saves` 4/12 at 6000: 18.
      - `Starting` (`running 0 0 |x`) at 9000: −1.
    - `line`:
      - Status `running 5 20 Games 3/10|g` with times `{1, 2001, 92001, "Games"}` at 102401:
        `Games · 0:10 · 42 s left · sync 1:40`.
      - The same at 94001: `Games · 0:02 · sync 1:32`.
      - Times phase `Saves` with a Games status: `""`. A `done` status: `""`.
    - `parse`: `4242 2001 92001 Checking the card` → those values. `x`, `""` and `4242 1` → false.
    - `nextTick`: starts 0 and 400 at 1250 → 150; both 0 at 3000 → 1000.
  - **`kiwami/tests/sync-times-ui.sh`** (new, `tailscale-ui.sh`'s shape; needs `ui/banana-ui`). The fixture comes
    from `ui/tests/fixture.sh`, with `wifi = on` in `settings.conf`, `KIWAMI_SYNC_RUN=$OUT/run` holding `sync.status`,
    `sync.pid` (`4242`) and `sync.times`. The way to Sync now is
    `ROW="home 0; wait 2400; press start; press r; press r; press r; press a; press up; press up; press a; press down;
    wait 1"`, which leaves the headless clock at 102401. At 640x480 and 1024x768:
    - **on-games** (A6, A7, A9): `kiwami_sync_times = on`, status `running 5 20 Games 3/10|Game 0003 (USA).gba`,
      times `4242 2001 92001 Games`; `$ROW; shot games-a; wait 2000; shot games-b`. The log has
      `banana-ui: kiwami: sync times: Games · 0:10 · 42 s left · sync 1:40`, once. `games-a-top` and `games-b-top`
      differ (the clock moved), and neither is blank.
    - **on-early** (A7): times `4242 2001 100401 Games`; `$ROW; shot early-a; wait 1500; shot early-b`. The log has
      `sync times: Games · 0:02 · sync 1:40`, then `sync times: Games · 0:03 · 15 s left · sync 1:41`.
    - **on-starting** (A7): status `running 0 0 |My RomM`, times `4242 92001 92001 Starting`. The log has
      `sync times: Starting · 0:10 · sync 0:10`.
    - **stale** (A8): on-games with times pid `4241`. No `sync times:` in the log; shot `stale`.
    - **phase-mismatch** (A8): on-games with times phase `Saves`. No `sync times:` in the log.
    - **off** (A1): on-games' files without the key. No `sync times:` in the log. Shot `off-top` equals `stale-top`
      (bar, no line) and differs from `games-a-top`.
    - **other-row** (A9's scope): on-games, with `ROW` stopping on Server (no last `press down`). No `sync times:`.
    - **the row** (A2): `ROW` then `press down; press a; wait 1`. The log has
      `banana-ui: kiwami: rows in Home server: kiwami_sync_times`, and `settings.conf` gets
      `kiwami_sync_times = on`: the row right after Sync now changes the setting.
    - Frame times: the script prints each `shot` line's `frame <ms>` for on-games, to compare by eye with `off`'s
      (no bound asserted: CI hosts vary).
  - **`kiwami/tests/ci.sh`**: after `sync-times.sh`'s block,
    `if [ -x ui/banana-ui ]; then kiwami/tests/sync-times-ui.sh; else echo "sync-times-ui: skipped, no ui/banana-ui (make ui)"; fi`.
  - **`docs/FORK.md`**: in *Taking in upstream*, step 3's paragraph on Settings' order: Home server's row
    Show sync times is placed by its hook's position after the Sync now row, because Home server isn't in `ORDER`. If
    upstream adds the page to `ORDER`, `Kiwami::place`'s `FOLLOWS` needs `{ "Home server", "Show sync times", "Sync
    now" }`, which `sync-times-ui.sh`'s row check catches.
- **Tests**: `kiwami/tests/sync-times-ui.sh` (new), `kiwami/tests/sync-times.sh` (with the new units), `make test`,
  `make -C ui`, `kiwami/tests/ci.sh .buildroot` (the fork's other UI tests, `tailscale-ui.sh` and `ra-ui.sh`, pass
  with the extra `rows in` line).
- **Commit**: `Sync times: the times under Sync now's bar, and the Show sync times row`

### 3. Final check

- `make test`, `make -C ui`, `make -C system/bananad CFLAGS="-O2 -Werror"`, `kiwami/tests/ci.sh .buildroot`.
- The fork's tests in `kiwami/tests/`, `sync-times.sh` and `sync-times-ui.sh` among them.
- **Off is `kiwami`'s** (A1): `kiwami`'s `ui/` before this feature, extracted with
  `git archive <merge-base> ui | tar -x -C $T/before`, its banana-sync built there, and
  `kiwami/tests/sync-times.sh --compare $T/before/banana-sync` passes.
- The hooks: `git diff --stat main...HEAD` and `git grep -n 'kiwami:'` (the commands in `docs/FORK.md`) agree, and
  the new ones are the 9 in `ui/src/sync.cpp` and the 2 in `ui/src/settings.cpp` (and the widened `ui/Makefile`
  line).
- With the fork's options off, `make test`'s renders in `ui/tests/out/` match `main`'s (`main`'s `ui/` built from a
  `git archive` the same way). The exceptions are shots of Settings pages that list the fork's rows, now including
  Home server's, as Network's already lists Tailscale.
- `kiwami/DEPENDENCIES.md`: unchanged (no dependency added).
- `docs/FORK.md` updated (steps 1 and 2); `docs/ARCHITECTURE.md` and `CLAUDE.md` don't describe the sync's status
  (checked: nothing to change).
- The manual tests below all passed.
- Every acceptance criterion is met: each is listed by at least one automated or manual test, and those passed.
- **Commit**: `Sync times: the final check done`

## Automated tests

| Test | Where | What it checks | Criteria |
|---|---|---|---|
| Units: phases, tracker, throttle, report, off | `kiwami/tests/synctimes-units.cpp` (run by `sync-times.sh`) | `phaseOf`, the times file's writes and removal, `report()`'s lines, nothing when off, the 250 ms throttle | A3, A4, A5, A12, A1 |
| Units: the menu's rule and text | `kiwami/tests/synctimes-units.cpp` | `clock`, `left` (against Files' `duration()`), `secondsLeft` for each phase kind, `line`, `parse`, `nextTick` | A6, A7, A8, A9 |
| banana-sync On | `kiwami/tests/sync-times.sh` | the phase order of a sync with games, a second with removals, a saves-only one; the new phases' first and last lines and their throttle; `sync.times` mid-sync (pid, starts) and gone after; the report's `times:` lines | A3, A4, A5, A12 |
| banana-sync Off | `kiwami/tests/sync-times.sh` | no new titles, no `times:`, no `sync.times` | A1 |
| Off against `kiwami`'s build | `kiwami/tests/sync-times.sh --compare` (final check, local) | the same status lines and report as the build before the feature | A1 |
| The menu's line | `kiwami/tests/sync-times-ui.sh` | the line's text (logged) for a Games phase with an estimate, before 3 s, Starting; the clock moving between shots; nothing when stale, mismatched, off, or on another row; both sizes; frame time | A6, A7, A8, A9, A1 |
| The row | `kiwami/tests/sync-times-ui.sh` | Show sync times right after Sync now, writing `kiwami_sync_times = on` | A2 |
| Upstream's and #22's | `make test` (`ui/tests/sync.sh`), `kiwami/tests/romm-list.sh` | unchanged, passing | A13 |

## Manual tests

On the device, for what headless tests can't reach. Logs are fetched over SSH/SFTP (Settings > Network > SSH) or from
the card on a computer (`banana/logs/` on the card is `/data/banana/logs/` on the device). Only the RG DS Plus is at
hand.

| # | Board | Steps | Expected | Logs to collect | Criteria | Outcome |
|---|---|---|---|---|---|---|
| M1 | rgds-plus | Paired with your RomM, Show sync times On, Games to sync with a few new games (or a system just switched to Sync). Sync now, and leave Settings on the Sync now row to the end. Photograph the line in each phase. | The clock ticks every second with no key pressed, and the bar moves. Each phase shows its name and time: Starting, Listing games, Checking the card, Games, Gamelists, Reading saves, Saves. The time left appears about 3 s into each counted phase and settles rather than jumping. The Games estimate is close to the real time taken. | `/data/banana/logs/sync.log` (the `times:` lines); `/data/banana/logs/banana-ui.log` (`grep 'sync times:'`); the photos | A3, A6, A7, A9, A12 | not run |
| M2 | rgds-plus | During a long sync (M1's), press B to the menu, wait a minute timed on a phone, then go back to Settings > Network > Home server > Sync now. Then set Auto sync to And on Wi-Fi, switch Wi-Fi off and on, and open Sync now while the automatic sync runs. | Back on Sync now, the sync's time has grown by the minute away, and the phase's time is right for its phase. The automatic sync shows its line too. | `sync.log` of each sync; `banana-ui.log` | A10 | not run |
| M3 | rgds-plus | During a Games download or the Saves phase, note the line's times, sleep with the power button for a minute (timed), and wake. | Both clocks carry on from where they were, without the minute asleep. The estimate stays reasonable after the resume. The sync finishes. | `sync.log` (the phase's duration has no extra minute); `banana-ui.log` | A11 | not run |
| M4 | rgds-plus | Show sync times Off, Sync now, Settings left on Sync now. | No line under the bar. The bar moves only on a key press, as upstream's does. `sync.log` has no `times:` line. | `sync.log`; `banana-ui.log` (no `sync times:`) | A1 | not run |
| M5 | rgds-plus | Show sync times On. Sync now, Stop (A again) partway through, then Sync now again at once. | Between the two, no line with the stopped sync's times. The new sync's line starts at `Starting · 0:00`. | `sync.log` of the second sync; `banana-ui.log` | A8 | not run |

Notes from the runs (what the logs showed, what was fixed):

## Deviations

Where the implementation turned out different from this plan, why, and what changed (dated).

- **2026-11-03, step 1, `sync-times.sh`'s second sync.** The plan expected no Games phase in the second sync (the
  stand-in with 25 games). There is one: `romm-big.py`'s edge case 11 (a file whose `file_size_bytes` is null) ends in
  "size mismatch" on every sync, so each sync downloads game 11's six files again (`Games 1/6` to `6/6`). The
  edge case is the stand-in's, and `romm-list.sh` relies on it, so the test expects
  `Starting,Listing games,Checking the card,Games,Removing,...` instead (step 1's item 7 updated). For the same reason
  every sync against the stand-in exits 1 with its `done` status, so the test doesn't require exit 0: the phase lists
  end in `done`, and `--compare` checks that both builds end the same way.

## Progress

A step is marked done in its own commit, so the commit column holds the subject (a commit can't carry its own hash).

| Step | Status | Commit |
|---|---|---|
| 1. banana-sync keeps each phase's time, with the silent steps as phases | done | Sync times: banana-sync keeps each phase's time, with the silent steps as phases |
| 2. The times under Sync now's bar, and the Show sync times row | todo | |
| 3. Final check | todo | |
