# 0036 Status on the game panel: plan

Issue: #36 · Branch: `k/0036-panel-status` · Investigation: [investigation.md](investigation.md) · Status: approved

## Summary

With Settings > Other systems > Second screen > `Status on the game panel` on (off by default), the game panel's
tiles view draws a status strip where its hint line was:

- **Left:** the Wi-Fi icon, dimmed while `wlan0` is down, and Tailscale's mark while the handheld is in its tailnet.
- **Centre:** "Played h:mm".
- **Right:** the battery's percentage and icon, the date and the time.

The time and time played change at the minute; battery and network are read every 10 s, and again at once after a
resume. The strip's state is a small class of the fork's with no screen (`kiwami::PanelStatus`), unit-tested. The
drawing is `Settings::Kiwami`'s, called from `settings_gamepanel.cpp` through four `kiwami:` lines.

Decided while planning:

- **Steps**: three, plus the final check: the state and its unit tests; the row; the strip and its hooks.
- **Charging wins**: on the charger, the level is drawn in the accent colour with the bolt, at any level, as the
  menu's bar does. Red only when the battery is at 10% or under and not charging.
- **Logging**: one line when a game starts with the setting on, then one per change of the battery's percentage,
  charging, Wi-Fi or Tailscale, and one after every sleep, marked. The minute's tick isn't logged.

A correction to the investigation (oversight 10): `kiwami::library` is set at `main.cpp:2118`, and
`lib.twelveHour` at `:2121`, both before `main` hands over to `runHeadless` (`:2131`). Headless runs already see the
library (the `battery` verb's `testPower`) and the Clock setting, so no hook in `runHeadless` is needed, and
`main.cpp` isn't touched. The test stand-ins the investigation called for are environment variables read by the
fork's code:

- `KIWAMI_PANEL_CLOCK`: a fixed clock, so time is deterministic.
- `KIWAMI_WIFI_OPERSTATE`: the file read for the Wi-Fi state instead of `wlan0`'s.
- `KIWAMI_TS_IFACE`: the existing override of 0034's interface.

## Acceptance criteria

- **A1**: Given Settings > Other systems > Second screen, then a row `Status on the game panel` (Off/On) follows
  `Game panel idle time` and shows Off on a card that never set it. A on it writes `kiwami_panel_status = on` to
  `settings.conf`.
- **A2**: Given the setting off (or absent), when a game's panel opens, then the panel is upstream's: the hint line
  drawn as before, no `panel status` log line, and nothing read for the strip.
- **A3**: Given the setting on, when the tiles view is drawn, then the strip replaces the hint, and everything above
  it (header, achievements, tiles) is drawn as with the setting off:
  - left: the Wi-Fi icon, and Tailscale's mark after it when up;
  - centre: `Played h:mm`;
  - right: the percentage, the battery icon, the date and the time.

  This holds with five tiles, with six (the Keys tile) and for either hint form.
- **A4**: The time follows Settings > Menu look > Clock (`14:32`, or `02:32 PM`) and the date is the menu's `11/16`.
  When the minute turns, the strip shows the new time with no input.
- **A5**: Time played counts from the panel's first pass for this game on the panel loop's clock (`CLOCK_MONOTONIC`,
  as the library's play time), so time asleep isn't counted. It turns each minute. The next game starts again at
  `0:00`.
- **A6**: The battery:
  - its percentage and icon, the level drawn in proportion (16 px wide at 100%);
  - on the charger, the level in the accent colour with the bolt, at any level;
  - at 10% and under, not charging, the percentage and the level in red (Settings' notice's red);
  - with no battery reading (percent -1), no battery drawn.
- **A7**: Wi-Fi is drawn in full while the operstate is `up` and at 35% otherwise. A change shows within 10 s of the
  panel's time.
- **A8**: Tailscale's mark (0034's) is drawn after the Wi-Fi icon, at the Wi-Fi icon's opacity, while the interface
  (`tailscale0`, or `KIWAMI_TS_IFACE`) has an IPv4 address, and not otherwise. A change shows within 10 s.
- **A9**: After a sleep, the panel's first pass shows the current time, reads the battery and network at once (not up
  to 10 s later), and logs `after a sleep`. Time played hasn't counted the sleep.
- **A10**: The companion pages (the Game Boy Pokémon companion's) and the on-screen keyboard are drawn exactly as with
  the setting off. Switching from the pages to the panel's menu shows the strip.
- **A11**: At 640x480 and 1024x768 (`rgds`, `rgds-plus`), the most crowded strip (12-hour clock, 100%, charging,
  Tailscale up) has the three parts apart, at least 8 px at 640x480. The 1x art is used at 640x480 and the 1.6x art
  at 1024x768, as the menu picks it.
- **A12**: The log (`/data/banana/logs/banana-ui.log`) gets `banana-ui: kiwami: panel status: <time> <date>,
  battery <n>%[, charging], wifi up|down, tailscale up|down, played <h:mm>`:
  - once when a game starts with the setting on;
  - once per change of the battery's percentage, charging, Wi-Fi or Tailscale;
  - once after every sleep, readings changed or not, prefixed `after a sleep: `;
  - never for the minute alone.
- **A13**: With the setting on, the strip reads the battery and network at most once per 10 s of the panel's time
  (plus once after each sleep), runs no thread, and redraws only when its text or readings change. Off, it reads
  nothing.
- **A14**: Dark and Light both draw the strip in their own palette.

## Risks

- **Performance and footprint**:
  - With the setting on, each loop pass (≤ 100 ms) adds two `clock_gettime`, a `time()` and a comparison; `strftime`
    runs only when the minute turns.
  - Every 10 s: `Library::power()` (a scan of `/sys/class/power_supply`, itself cached for 2 s), one read of
    `wlan0`'s operstate and one `ioctl` on `tailscale0`.
  - A redraw a minute, of the strip's pixels alone (the backend diffs the draw lists).
  - One new text texture a minute; Settings' cache evicts the old ones (`evict()`, as every text).
  - No thread, nothing while the menu is idle, a few KB of code.

  Guarded by the unit test's read counts (A13) and the device's M1 log.
- **Conflicts with upstream**:
  - `settings_gamepanel.cpp` has had 4 upstream commits. The four hooks sit at the include list, the end of
    `gameOpen`, before `gameUpdate`'s flash check and around the hint's three lines. A rework of the panel's bottom
    line conflicts there, visibly.
  - The row inserts itself after the `bottom_idle_seconds` row. If upstream drops that key, the row falls to the end
    of Second screen and `panel-status-ui.sh`'s row check fails.
  - `docs/FORK.md` gets the paragraph that says so.
- **Formats copied from the library**: `Library::clockText`/`dateText` read the real clock, so the strip formats with
  the same `strftime` strings (`%H:%M`/`%I:%M %p`, `%m/%d`) itself, to take the tests' clock. If upstream changes the
  menu's formats, the strip keeps the old ones until someone follows. The unit test pins the strings; the risk is a
  mismatch in look, not a failure. Accepted.
- **The Tailscale interface in headless runs**: unlike the menu's bar, the strip reads `tailscale0` when
  `KIWAMI_TS_IFACE` is unset. Only the fork's own test turns the strip on, and it always sets the variable, so a test
  machine's own Tailscale doesn't leak in. Upstream's `make test` has the setting off.
- **Hardware**: only the device shows real battery and charger readings, Wi-Fi dropping, and a sleep. The user has an
  RG DS Plus only, so the RG DS (its Wi-Fi module unloaded around a sleep) isn't covered on hardware. Its code path
  is the same.
- **The row's place moves the GB companion's rows down one**: `gbpk-ui.sh` reaches them with one more Down (step 2).

## Steps

Each step is one commit, and leaves the tree building, `make test` and `kiwami/tests/ci.sh .buildroot` passing.

### 1. The strip's state, with no screen

- **Files**:
  - `ui/src/kiwami/panelstatus.h` (new)
  - `ui/src/kiwami/panelstatus.cpp` (new)
  - `kiwami/tests/panelstatus-units.cpp` (new)
  - `kiwami/tests/ci.sh` (changed)
- **Change**: `banana::kiwami::PanelStatus`, everything the strip knows apart from drawing it, testable on its own:

  ```cpp
  struct PanelReadings { int percent = -1; bool charging = false, wifi = false, tailnet = false; };	// == and !=
  struct PanelState { std::string clock, date, played; PanelReadings r; };							// == and !=
  class PanelStatus
  {
  public:
      struct Sources
      {
          std::function<PanelReadings()> read;	// the battery, Wi-Fi and Tailscale (at most every 10 s, and after a sleep)
          std::function<time_t()> wall;		// the real clock (time(nullptr))
          std::function<double()> asleep;		// ms slept since boot: CLOCK_BOOTTIME - CLOCK_MONOTONIC
      };
      explicit PanelStatus(Sources s);
      void open(bool twelveHour, time_t fixedClock = 0);	// a game starts; fixedClock: the tests' clock at its start
      bool update(double t);				// t: the panel loop's ms (CLOCK_MONOTONIC); true: the state changed
      bool ready() const;					// update() ran since open()
      const PanelState& state() const;
      std::string logLine();				// the line to log after update(), "" none (taken once)
      static std::string played(double ms);			// "0:00", "0:07", "1:23", "12:05"
      static std::string clockText(time_t w, bool twelveHour);	// "%H:%M" or "%I:%M %p", localtime
      static std::string dateText(time_t w);			// "%m/%d"
  };
  ```

  `update(t)`:
  - The first call after `open()` takes `t` as the start, reads, and builds the state.
  - Later calls read again when `t - lastRead >= 10000`, or when `asleep()` grew by more than 1000 ms since the last
    call (a resume).
  - The wall clock is `fixedClock + floor((t - start) / 1000)` when `fixedClock` is set, else `wall()`.
  - Clock and date are formatted only when `wall / 60` changes; `played` when `(t - start) / 60000` changes.
  - It returns true when the new state differs from the last.

  `logLine()`:
  - `panel status: 14:32 11/16, battery 78%, charging, wifi up, tailscale down, played 0:00`;
  - `battery none` when the percent is -1; `, charging` only while charging;
  - after the first `update()`, after an update whose readings differ from the last logged ones, and after every
    update that followed a sleep (readings changed or not), prefixed `after a sleep: ` then;
  - `""` when only the clock, date or time played changed.

  No Settings, no Library, no I/O: the glue in step 3 gives it its sources. Built into banana-ui by the Makefile's
  `src/kiwami/*.cpp` wildcard; unused until step 3.

- **Tests**: `kiwami/tests/panelstatus-units.cpp`, built and run by `ci.sh` after `statusbar-units` (the same
  `g++ -std=c++17 -Wall -Werror -I ui/src` line), with `TZ=UTC` set and stand-in sources counting their calls:
  - `played`: 0 → `0:00`, 59999 → `0:00`, 60000 → `0:01`, 83 min → `1:23`, 12 h 5 min → `12:05`.
  - `clockText(1794839570, false)` = `14:32`, `(…, true)` = `02:32 PM`; `dateText` = `11/16`.
  - With `fixedClock = 1794839570` (14:32:50):
    - first `update(100000)` true, one read, `logLine()` the whole state with `played 0:00`;
    - `update(105000)`: false, still one read;
    - `update(110000)`: 14:33, true, two reads, `logLine()` `""`;
    - `update(160000)`: `played 0:01`, true, `""`.
  - A reading change (the stand-in's percent 78 → 77) at the next 10 s: true, `logLine()` with `battery 77%`.
    Charging on: `, charging`. Wi-Fi and Tailscale each: their word changes.
  - The same readings again: `update` false, `logLine()` `""`.
  - `asleep()` grows by 5000 between two updates 100 ms apart: a read at once, a line with `after a sleep: `, with
    the readings changed and with them the same. Time played is unchanged by the sleep, since it follows `t`. A growth
    under a second isn't a sleep.
  - `open()` again: `ready()` false until the next update; that update starts at `played 0:00` with a full line.
  - `percent -1`: `battery none`.
  - Commands at the end of the step: `make ui`, `kiwami/tests/ci.sh .buildroot`, `make test`.
- **Commit**: `Status on the game panel: the strip's state, with no screen`

### 2. The Second screen row

- **Files**:
  - `ui/src/kiwami/kiwami.cpp` (changed)
  - `ui/src/kiwami/kiwami.h` (changed: the header's list of the fork's rows)
  - `kiwami/tests/gbpk-ui.sh` (changed)
  - `kiwami/tests/panel-status-ui.sh` (new)
  - `kiwami/tests/ci.sh` (changed)
- **Change**: in `Settings::Kiwami::rows`, the `"Second screen"` branch (`kiwami.cpp:116-138`) adds, after the Game
  Boy rows:

  ```cpp
  Item it;
  it.section = group; it.icon = "picture";	// (upstream's panel rows' icon)
  it.label = "Status on the game panel";
  it.key = "kiwami_panel_status";
  it.help = "The time and date, the battery, Wi-Fi and Tailscale, and how long the game has been played, along the "
      "bottom of the game panel while a game plays.";
  it.file = s.data + "/settings.conf";
  it.names = { "Off", "On" }; it.values = { "off", "on" };
  it.hints = { "The game panel's hint line, as before.", "A status strip instead of the hint line." };
  ```

  It is inserted right after the item with `section == group && key == "bottom_idle_seconds"`, else appended. Second
  screen isn't in `build()`'s `ORDER`, so the `stable_sort` keeps that place (`settings.cpp:925-958`). Its key goes
  last in the log line: `rows in Second screen: kiwami_pokemon_companion_gb kiwami_pokemon_companion_gbc
  kiwami_panel_status`.

  `kiwami.h`'s header comment gets a line for the row (and, in step 3, the strip). The row does nothing until step 3.
- **Tests**:
  - `kiwami/tests/gbpk-ui.sh:65-71`:
    - the rows run presses Down five times (not four) to reach the Game Boy row, since the new row sits between
      `Game panel idle time` and `Pokémon companion`;
    - its comment is updated;
    - the `logged` line ends `kiwami_pokemon_companion_gbc kiwami_panel_status$`.
  - `kiwami/tests/panel-status-ui.sh` (new; the structure of `tailscale-icon-ui.sh`: fixture, `run`, `logged`,
    `region`, both sizes), its first part, the row:
    - script: `home 0; wait 2400; press start; press r; press a;` four Downs; `press a;` three Downs;
      `wait 1; shot off; press a; wait 1; shot on`;
    - asserts: the rows line above; `settings.conf` has `kiwami_panel_status = on` after one A (so it was Off); the
      `off` and `on` bottom shots differ.
  - `ci.sh` runs it after `gbpk-ui.sh`, skipped without `ui/banana-ui`, as the other `*-ui.sh`.
  - Commands: `make ui`, `kiwami/tests/gbpk-ui.sh`, `kiwami/tests/panel-status-ui.sh`,
    `kiwami/tests/ci.sh .buildroot`, `make test`.
- **Commit**: `Status on the game panel: the Second screen row`

### 3. The strip drawn instead of the hint

- **Files**:
  - `ui/src/settings_gamepanel.cpp` (hook)
  - `ui/src/kiwami/panel-status.cpp` (new)
  - `ui/src/kiwami/kiwami.h` (changed)
  - `ui/src/kiwami/tsmark.h` (new)
  - `ui/src/kiwami/statusbar.cpp` (changed)
  - `kiwami/tests/panel-status-ui.sh` (changed)
  - `docs/FORK.md` (changed)
- **Change**:
  - **Hooks** in `settings_gamepanel.cpp`, each marked `kiwami:`, upstream's lines unchanged and not re-indented:
    - after `#include "nds/overlay.h"` (`:15`): `#include "kiwami/kiwami.h"	// kiwami: the game panel's status strip`;
    - the last line of `gameOpen`, after `:378`: `if (kw) kw->panelOpened();	// kiwami: the status strip's setting
      and start, per game`;
    - in `gameUpdate`, before `if (gFlash >= 0 ...)` (`:618`): `if (kw && kw->panelUpdate(t)) changed = true;
      // kiwami: the status strip (the minute, battery, network)`;
    - in `gameFrame`, around the hint (`:973-975`): `if (!kw || !kw->panelStatus()) {	// kiwami: the status strip
      instead of the hint` before, `}	// kiwami:` after.
  - **`kiwami.h`**: public `void panelOpened(); bool panelUpdate(double t); bool panelStatus();`; private
    `bool panelOn = false; std::unique_ptr<kiwami::PanelStatus> panel;` and the strip's drawing helpers. The header's
    comment describes the strip.
  - **`panel-status.cpp`** (the glue and the drawing):
    - `panelOpened()`: `panelOn = getConfValue(st.data + "/settings.conf", "kiwami_panel_status", "off") == "on"`.
      When on, it makes `panel` once with its sources and calls `panel->open(kiwami::library &&
      kiwami::library->twelveHour, atoll(getenv("KIWAMI_PANEL_CLOCK") ?: "0"))`. The sources:
      - `read`: `kiwami::library->power()`, or percent -1 without a library;
      - Wi-Fi: the first line of `KIWAMI_WIFI_OPERSTATE`'s file, else `/sys/class/net/wlan0/operstate`, `== "up"`;
      - Tailscale: `hasIPv4(KIWAMI_TS_IFACE` or `"tailscale0")`;
      - `wall`: `time(nullptr)`;
      - `asleep`: `CLOCK_BOOTTIME - CLOCK_MONOTONIC` in ms.
    - `panelUpdate(t)`: false while off. Otherwise `panel->update(t)`, its `logLine()` to stderr as
      `banana-ui: kiwami: <line>`, and the result. With `KIWAMI_PANEL_TRACE=1`, `panelStatus()` also logs where the
      parts went (below).
    - `panelStatus()`: false while off or not `ready()` (upstream's hint is drawn). Otherwise it draws the strip and
      returns true. It is called only on the tiles view: `gameFrame` returns earlier for the keyboard and the pages.
  - **The strip**, in the panel's 640x480 units (`X()` to device px), with the palette from
    `Kiwami::colours(st.light)`:
    - **Left**:
      - The Wi-Fi sprite at (24, 440): `assets/wifi@1x.png` (18x16), or `wifi@1.6x.png` when `s > 1.3` (the
        engine's pick). Uploaded once into Settings' cache (`put`/`keep`) and drawn at its own size, in ink at
        alpha 1 (up) or 0.35 (down).
      - Tailscale's mark at x 24 + 18 + 8 when up, 16 or 26 px, the same tint.
    - **Centre**: `Played h:mm`, regular 15, muted, centred on x 320, top 438. One string, so it isn't translated.
    - **Right**, from x 616 leftwards:
      - the time, medium 17, ink, right-aligned at 616, top 437;
      - the date, regular 15, muted, 8 px to its left;
      - the battery sprite (`battery@1x.png` 26x14, or `@1.6x`), 12 px left of the date, at y 441, in ink; its
        level `round(16 × percent × s) / s` wide and 8 tall at (+3, +3), as the engine draws it;
      - the percentage, regular 15, right-aligned 6 px left of the battery in room for `100%`, so nothing moves as
        it changes.
    - **Colours**:
      - charging: the level in the accent, with the engine's 8-row bolt (in ink with a dark edge) over it;
      - not charging and ≤ 10%: the percentage and the level in red (`0xff5a4f` Dark, `0xc8382d` Light, Settings'
        notice's);
      - no battery reading: neither drawn.
    - **Trace** (`KIWAMI_PANEL_TRACE=1`, tests only): `panel status: drawn: left ends at <x>, played <x0>-<x1>,
      right starts at <x>` in 640x480 units.
  - **`tsmark.h`**: Tailscale's two pixel maps (`MARK16`, `MARK26`) moved out of `statusbar.cpp`, with
    `rnds::Bitmap tsMark(bool big)` (the alpha bitmap `StatusBar::icon()` builds today). `statusbar.cpp` uses it; its
    drawing is unchanged (`statusbar-units` and `tailscale-icon-ui.sh` show it).
  - **`docs/FORK.md`**, *Taking in upstream* step 3: a paragraph on the strip.
    - The four hooks in `settings_gamepanel.cpp`, and what moves them: a rework of the hint line or of `gameUpdate`.
    - The row's place after `bottom_idle_seconds`.
    - The formats copied from `Library::clockText`/`dateText`.
    - `panel-status-ui.sh` catches each of these.
- **Tests**: `kiwami/tests/panel-status-ui.sh`, extended. Every run uses `TZ=UTC`, `KIWAMI_PANEL_CLOCK=1794839570`
  (14:32:50 on 11/16), `KIWAMI_WIFI_OPERSTATE=$OUT/operstate`, `KIWAMI_TS_IFACE` set and `KIWAMI_PANEL_TRACE=1`. At
  both sizes, the panel opened with `gamepanel 1 /nonexistent.gba`:
  - **Off** (no key, then `kiwami_panel_status = off`): no `panel status` line; the two `gshot`s are identical
    (A2).
  - **On**, `battery 78`, Wi-Fi `up`, Tailscale `lo` (A3, A6, A7, A8, A12):
    - the line `panel status: 14:32 11/16, battery 78%, wifi up, tailscale up, played 0:00`;
    - the bottom shot is identical to the off one above y 424 (`region`), and differs below it.
  - **The minute** (A4, A5, A12):
    - `gshot a; wait 10000; gshot b`: b's strip differs from a's (14:33), no new line;
    - `wait 50000; gshot c`: c differs from b (`played 0:01`), no new line.
  - **The 10 s reads** (A6, A13):
    - `battery 50; wait 5000; gshot` gives no new line;
    - `wait 5000; gshot` gives `battery 50%`;
    - `battery 9; wait 10000; gshot low` gives `battery 9%`, and the strip's right part has pixels of Dark's red
      (ImageMagick count of `#ff5a4f` within the region > 0);
    - `battery 9 charging; wait 10000; gshot ch` gives `battery 9%, charging`, and no red pixels.
  - **Wi-Fi down** (operstate `down`): `wifi down`, and the left icon's region differs from the up run's (A7).
  - **Tailscale down** (`KIWAMI_TS_IFACE=kiwami-none0`): `tailscale down`, and the region after the Wi-Fi icon
    differs (A8).
  - **Crowded** (`clock = 12h`, `battery 100 charging`, Tailscale up; A4, A11):
    - the line has `02:32 PM 11/16, battery 100%, charging`;
    - the trace's left end + 8 ≤ played's start, and played's end + 8 ≤ the right part's start (13 at 1024x768).
  - **Light** (`theme = light`): the shot isn't blank and differs from Dark's (A14).
  - **Six tiles** (`BANANA_TEST_VKBD=1`): the line is there and the strip region differs from the off run's (A3).
    Then `gtouch 576 360` (the keyboard): the keyboard shot is identical with the setting on and off (A10).
  - **Companion pages** (`KIWAMI_GBPK_DIR`, `gbpanel 1 $CAP/crystal; gbhost; gbfeed $CAP/crystal; sleep 300; gshot
    party; gtouch 600 28; gshot menu`, as `gbpk-ui.sh`): `party` is identical on and off; `menu` has the strip on
    (differs below y 424) (A10).
  - `make test` (upstream's fixture has the setting off: its shots unchanged), `statusbar-units` and
    `tailscale-icon-ui.sh` (the mark moved).
  - Commands: `make ui`, `make -C ui`, `kiwami/tests/panel-status-ui.sh`, `kiwami/tests/ci.sh .buildroot`,
    `make test`.
- **Commit**: `Status on the game panel: the strip drawn instead of the hint`

### 4. Final check

- `make test`, `make -C ui`, `make -C system/bananad CFLAGS="-O2 -Werror"`, `kiwami/tests/ci.sh .buildroot` (with
  `ui/banana-ui` built, so the `*-ui.sh` tests run, `panel-status-ui.sh` and `gbpk-ui.sh` among them).
- The hooks: `git diff --stat main...HEAD` and `git grep -n 'kiwami:'` agree. This feature adds exactly the four
  `kiwami:` lines in `ui/src/settings_gamepanel.cpp` (the include, `gameOpen`, `gameUpdate`, the hint's `{` and `}`),
  and nothing in `main.cpp`, `ui/rnds/`, `ui/assets/`.
- With the setting off, `make test`'s renders in `ui/tests/out/` match `main`'s (a `git worktree` of `main`, its
  `make test`, the PNGs compared with `cmp`).
- `kiwami/DEPENDENCIES.md`: nothing added (no dependency).
- `docs/FORK.md` (step 3) describes the hooks; `docs/ARCHITECTURE.md` and `CLAUDE.md` unchanged (nothing structural).
- An image built for `rgds-plus` with the branch, for the manual tests.
- The manual tests below all passed, their outcomes in the table.
- Every acceptance criterion is met: each is listed by at least one test below, and those passed.

## Automated tests

| Test | Where | What it checks | Criteria |
|---|---|---|---|
| State units | `kiwami/tests/panelstatus-units.cpp` (in `ci.sh`) | Played and clock formats, the minute, reads every 10 s and after a sleep, change detection, the log lines, a new game | A4, A5, A9, A12, A13 |
| The row | `kiwami/tests/panel-status-ui.sh` (in `ci.sh`) | The row after Game panel idle time, Off by default, A writes `on` | A1 |
| Off | `kiwami/tests/panel-status-ui.sh` | No line, the same panel with the key absent or off | A2 |
| On, readings | `kiwami/tests/panel-status-ui.sh` | The start line; above the strip unchanged; battery 50/9/charging, Wi-Fi down, Tailscale down; red pixels only when low and not charging | A3, A6, A7, A8, A12, A13 |
| The minute | `kiwami/tests/panel-status-ui.sh` | The strip redrawn at 14:33 and at `played 0:01`, no line | A4, A5, A12 |
| Crowded, both sizes | `kiwami/tests/panel-status-ui.sh` | 12-hour clock, 100%, charging: the parts apart (trace) | A4, A11 |
| Light | `kiwami/tests/panel-status-ui.sh` | The strip in Light's palette | A14 |
| Views | `kiwami/tests/panel-status-ui.sh` | Six tiles have the strip; keyboard and companion pages unchanged; the menu after the pages has it | A3, A10 |
| The GB rows | `kiwami/tests/gbpk-ui.sh` | The fork's Second screen rows still reached and listed | A1 |
| Tailscale's mark moved | `kiwami/tests/statusbar-units.cpp`, `tailscale-icon-ui.sh` | The menu's bar unchanged | (no regression) |
| Upstream's look | `make test`, final check vs `main` | Setting off: renders as `main`'s | A2 |

## Manual tests

On the RG DS Plus (the only device), with an image from the branch, the setting on (unless said), a GBA game through
RetroArch (`ra_frontend = retroarch`) and one through BananaFront. Logs are fetched over SSH/SFTP (Settings >
Network > SSH) or from the card on a computer: `banana/logs/banana-ui.log` on the card is
`/data/banana/logs/banana-ui.log` on the device (the last run's; copy it before restarting the menu).

| # | Board | Steps | Expected | Logs to collect | Criteria | Outcome |
|---|---|---|---|---|---|---|
| M1 | rgds-plus | Start a GBA game in RetroArch; look at the panel; play past two minute changes without touching it; then play 10+ min | Strip at once: Wi-Fi, (Tailscale if on), `Played 0:00`, battery %, date, time as the top screen's menu showed. Time and played turn at the minute with no touch. Strip legible when dimmed | `banana-ui.log` (`panel status:` lines), photo of the panel | A3, A4, A5, A12, A13 | not run |
| M2 | rgds-plus | Same with the setting off | The hint as before, no `panel status` line | `banana-ui.log` | A2 | not run |
| M3 | rgds-plus | In a game: plug the charger, unplug; play until the % drops one | Bolt and accent level within 10 s of plugging; gone within 10 s of unplugging; the new % within 10 s | `banana-ui.log` (`charging` lines with times), photos | A6, A12 | not run |
| M4 | rgds-plus | In a game: switch the Wi-Fi off from the router side or walk out of range, then back; with Tailscale on, `tailscale down` over SSH then `up` | Wi-Fi dims within 10 s and comes back; Tailscale's mark goes and returns within 10 s | `banana-ui.log`, `/data/banana/logs/tailscale.log`, `ip addr show tailscale0` over SSH before and after | A7, A8, A12 | not run |
| M5 | rgds-plus | In a game at `Played 0:0x`, close the lid for 3+ min, open it | The time right on the first frame after waking; a line starting `after a sleep:`; `Played` hasn't added the sleep; the game's play time in the library afterwards matches | `banana-ui.log` (the after-sleep line, `game ended after … s`), `/data/banana/logs/sleep.log` (bananad's) around the sleep | A5, A9, A12 | not run |
| M6 | rgds-plus | A GBA Pokémon game with the companion on: the pages, then F to the menu, then back | Pages as before; the menu shows the strip | photo of each | A10 | not run |
| M7 | rgds-plus | Settings > Other systems > Screen: RetroArch on the bottom; start a game | The panel on the top screen, with the strip | photo | A3, A11 | not run |
| M8 | rgds-plus | Clock set to 12-hour, theme Light, battery at 100% on the charger (or near); start a game | `02:32 PM`-style time, Light colours, nothing overlapping at 1024x768 | photo, `banana-ui.log` | A4, A11, A14 | not run |
| M9 | rgds-plus | A C64 or ZX Spectrum game (six tiles): open the Keys tile, close it | The strip with six tiles; the keyboard unchanged; strip back after Done | photo | A3, A10 | not run |
| M10 | rgds-plus | Battery under 10% (or `battery` from a nearly empty charge), not charging | Percentage and level in red | photo, `banana-ui.log` | A6 | not run |

Notes from the runs (what the logs showed, what was fixed):

## Deviations

Where the implementation turned out different from this plan, why, and what changed (dated).

- **2026-11-16, step 1: a line after every sleep.** The plan logged `after a sleep: ` only when a reading had
  changed across the sleep, but A9 and M5 expect that line after every wake, and on the RG DS Plus a lid closed for
  3 minutes usually leaves the battery's percentage, Wi-Fi and Tailscale as they were: M5 would find no line. Agreed
  with the user: the first update after a sleep always logs one line, marked, whether its readings changed or not
  (one line per sleep). Changed: the Summary's *Logging*, A12, step 1's `logLine()` and its sleep test.
- **2026-11-16, step 3: small changes while drawing the strip.**
  - The palette is Settings' `pal(st.light)` (`settings_draw.h`), not `Kiwami::colours()`: the bolt's dark edge is
    the palette's `blackish`, which `Colours` doesn't carry, and widening it would touch `colours()`'s `kiwami:` line
    in `settings.cpp`. The colours are the same ones.
  - The percentage is drawn in ink (the plan didn't say), red when low, as the engine's status bar draws it in ink.
  - The strip's texts are drawn as they are (`AsNamed`), not through the menu's language: they are numbers, the date
    and the one string `Played`.
  - The crowded check's gap at 1024x768 is 13 device px, which is 8.125 in the trace's 640x480 units.
  - `kiwami/tests/ci.sh` changed too: its comment above `panel-status-ui.sh` says what the test now covers.

## Progress

A step is marked done in its own commit, so the commit column holds the subject (a commit can't carry its own hash).

| Step | Status | Commit |
|---|---|---|
| 1. The strip's state, with no screen | done | Status on the game panel: the strip's state, with no screen |
| 2. The Second screen row | done | Status on the game panel: the Second screen row |
| 3. The strip drawn instead of the hint | done | Status on the game panel: the strip drawn instead of the hint |
| 4. Final check | todo | |
