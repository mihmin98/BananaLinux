# 0036 Status on the game panel: investigation

Issue: #36 · Branch: `k/0036-panel-status` · Status: refined · Confidence: 97% (proceed)

## The request

While a game plays, the other screen's control panel (upstream's *game panel*: the game's title, its achievements,
and the Save, Load, Menu, Fast and Quit tiles) should show the date and time, the battery and the network's state.
Today nothing on either screen says any of that until the game is quit.

Decided with the user (question round 1):

- **Where**: the panel's tiles view only, the one named in the request. The companion pages (GBA and Game Boy
  Pokémon, the game companion) and the on-screen keyboard keep their own layouts.
- **What**: the clock and date; the battery's percentage and icon; the Wi-Fi state; and, as additions, Tailscale's
  icon (the fork's 0034) and the time played this session.
- **Network**: the Wi-Fi link, as the menu shows it (the icon dimmed while `wlan0` is down). No SSID, no internet
  check.
- **Battery**: always the icon and the percentage, whatever the menu's Battery setting.
- **Signal strength**: the user asked to include it only if the main menu shows it. It doesn't (*Current state*), so
  it's a separate issue, #37 ("Wi-Fi signal strength in the status bar"), and this feature has the on/off icon.

Decided with the user (question round 2):

- **Placement**: with the setting on, the panel's bottom hint line ("Tap a tile, or hold F with the button under it",
  y 438) becomes a status strip: Wi-Fi and Tailscale on the left, the time played in the centre, the battery, the
  date and the time on the right.
- **Refresh**: the time and time played at the minute; battery and network read every 10 s.
- **Time played**: hours and minutes ("Played 1:23"), counted as the library counts play time (time asleep not
  counted).
- **Low battery**: the battery's icon and percentage in red at 10% and under.
- **Formats**: the menu's. The time follows Settings > Menu look > Clock (24h/12h); the date is the menu's `11/16`.
- **Idle dim**: left alone. The panel still dims or turns off when idle, and a touch lights it again.
- **Setting**: a row `Status on the game panel` (Off/On) in Settings > Other systems > Second screen, off by default
  (the fork's rule). With it off, the panel is upstream's, pixel for pixel.
- **Language**: the strip's word ("Played") and the new row stay English in every language, as the fork's other rows
  do.

Out of scope: the companion pages and the keyboard; Wi-Fi signal strength (#37); games without the game panel (the
DS, the official PICO-8, PortMaster and other non-RetroArch launchers, and `retroarch_panel = off`); a low-battery
notice on the panel.

The issue's text (#36):

```
A status strip on the game panel: clock, battery and network while a game plays
```

```markdown
While a RetroArch (or BananaFront) game plays, the other screen shows the game panel: the game's title, its achievements, and the tiles to save, load, open the menu, fast-forward and quit. Nothing on either screen then says the time, how much battery is left, or whether the handheld is online, short of quitting the game.

With a new setting on, the panel's bottom line (today's hint "Tap a tile, or hold F with the button under it") becomes a status strip:

- **Left:** the Wi-Fi icon, dimmed while `wlan0` is down (as the menu's status bar), and Tailscale's icon while the handheld is in its tailnet.
- **Centre:** the time played this session, in hours and minutes (`Played 1:23`), counted as the library counts play time, without time asleep.
- **Right:** the battery's percentage and icon (with the charging bolt, in red at 10% and under), the date and the time, in the menu's formats (Settings > Menu look > Clock).

The time and time played change at the minute; battery and network are read every 10 s. The panel still dims or turns off when left alone, as set in Second screen.

The setting is a row in Settings > Other systems > Second screen, `Status on the game panel`, off by default: off, the panel is upstream's.

Out of scope: the companion pages (GBA and Game Boy Pokémon, the game companion) and the on-screen keyboard, which keep their own layouts; Wi-Fi signal strength (a separate issue: the menu has none either); DS games, which use both screens.
```

## Oversights in the requirements

1. **The panel redraws only when something changes.** The game panel's loop wakes at least every 100 ms
   (`poll(..., 100)`, `ui/src/main.cpp:2651`), but draws only when `Settings::gameUpdate` says something changed
   (`main.cpp:2622-2633`, `settings_gamepanel.cpp:579-620`): achievements, a companion's data, a tile's flash. A clock
   drawn into today's frame would stay at the minute the panel was last drawn.
   *Decided*: the fork's part of `gameUpdate` reports a change when the minute turns (time and time played), and
   when the battery or network reading it takes every 10 s differs from what is drawn. The backend diffs the draw
   lists (`ui/src/soft.cpp`), so only the strip's pixels are sent to the panel.

2. **The hint goes away.** With the setting on, the strip takes the hint's line. The hint has two forms: with touch,
   "Tap a tile, or hold F with the button under it"; without it, "Hold F with the button under each tile"
   (`settings_gamepanel.cpp:973`). Each tile still shows its F combination under its label (`:968-971`), so little
   is lost.
   *Decided*: the strip replaces both forms. The strip takes no touches: a tap at y ≥ 424 on the tiles view does
   nothing today (only the game companion's pages use that band, `:709-719`), and nothing changes there.

3. **It shows only while the panel is lit.** By default the panel dims to 25% after 30 s with no touch
   (`system/rootfs/etc/banana/settings.conf:26-30`, `BottomIdle`, `main.cpp:1899-1985`), and the game's buttons
   don't light it again (a QA decision, `main.cpp:2708-2712`). At 25% the strip is still readable; with "Turns off"
   it needs a touch first.
   *Decided*: left alone. "Stays on" (Second screen > Game panel when idle) exists for anyone who wants the strip
   always visible.

4. **A companion opens on its pages.** With the GBA or Game Boy Pokémon companion or the game companion on, the panel
   starts on the companion's pages, which have no strip (round 1). The strip shows after switching to the panel's
   menu: F, the corner icon, or Back (`settings_gamepanel.cpp:515-575`).
   *Decided*: accepted, as round 1 settled the views.

5. **Only games with the game panel.** The panel exists only for RetroArch, BananaFront, `run-ps1` and fake-08's
   PICO-8 games with `retroarch_panel` on and the display open (`main.cpp:2772-2775`). The DS (both screens, or the
   DS companion's overlay), the official PICO-8, and every other launcher take both panels; with
   `retroarch_panel = off` the other screen stays dark.
   *Decided*: out of scope (in the issue).

6. **Sleep.** bananad SIGSTOPs the game and the system suspends; banana-ui is frozen with it. Around a suspend:
   - The clock: after a resume, the loop's next pass (≤ 100 ms) sees the new minute and redraws.
   - Time played: counted on `CLOCK_MONOTONIC`, which stops while asleep. That is how upstream measures the play
     time it records (`nowMs()`, `main.cpp:86-91`; `secs`, `:3062`; `recordPlay`, `:3087`), so the strip's count
     matches the library's.
   - Battery and network: a 10 s period on `CLOCK_MONOTONIC` would keep the pre-sleep reading for up to 10 s after
     a resume. `Library::power()` keeps its own 2 s cache on `CLOCK_BOOTTIME` for this reason (`library.cpp:1010`).
   - Wi-Fi: on the RG DS, `sleep.d/10-wifi` unloads the Wi-Fi module around a suspend (from 0034's investigation),
     so the icon is dimmed after a resume until Wi-Fi is back. The menu shows the same.

   *Decided*: the 10 s period counts on `CLOCK_BOOTTIME`, so the first pass after a resume reads again. Time played
   uses `CLOCK_MONOTONIC`.

7. **No battery or no Wi-Fi.** `Library::battery()` is -1 when no supply reports a capacity (QEMU, a host)
   (`library.cpp:1020-1057`). With no `wlan0`, `online()` is false (`:1059-1065`).
   *Decided*: no battery reading draws no battery; no `wlan0` draws the Wi-Fi icon dimmed, as the menu does.

8. **Both boards, both panels, both screens.** The panel is drawn in a 640x480 layout scaled by `s` (`X()`), so
   1024x768 on the RG DS Plus is the same layout. The theme's sprites come in `@1x` and `@1.6x`
   (`ui/assets/rnds/wifi@*.png`, `battery@*.png`). The panel can be the top screen when RetroArch is set to the
   bottom one (`video_monitor_index`, `main.cpp:2779-2780`); the drawing doesn't change. A 12-hour clock
   ("02:32 PM") is the widest right side.
   *Decided*: the plan measures the strip at both sizes with a 12-hour clock, 100% and charging.

9. **The row's place.** The fork's existing hook in Second screen is after "Pokémon companion"
   (`ui/src/settings.cpp:643`), and Second screen has no entry in `build()`'s `ORDER`, so rows stay where they are
   added (`ui/src/kiwami/kiwami.cpp:116-120`). The row belongs after "Game panel idle time"
   (`settings.cpp:638-639`).
   *Decided*: `Kiwami::rows` inserts the row into `items` after `bottom_idle_seconds`'s row, instead of appending
   it, so no new hook is needed. The plan confirms that the list keeps that order. `kiwami/tests/gbpk-ui.sh:71`
   asserts the exact list of the fork's Second screen rows (anchored with `$`) and changes in the same step.

10. **Headless runs don't reach the library.** `kiwami::library` is set only in the device's `main`
    (`main.cpp:2118`), not in `runHeadless` (`main.cpp:852`). The script's `battery <pct> [charging]` verb
    (`main.cpp:923-929`) works through that library's `testPower`. Also, on a test machine the real clock and the
    host's own `wlan0` would make shots differ from run to run.
    *Decided*: one `kiwami:` line in `runHeadless` sets `kiwami::library`. The plan adds test overrides in the fork's
    code: a fixed time, and the Wi-Fi state or interface, like `KIWAMI_TS_IFACE` for Tailscale
    (`ui/src/kiwami/statusbar.cpp:247-249`).

## Suggestions

- **Signal strength**: split off as #37 (round 1). When #37 is built, the strip's Wi-Fi icon can take its bars.
- **Tailscale's mark from 0034, shared**: its pixel maps (`MARK16`, `MARK26`) are file-static in
  `ui/src/kiwami/statusbar.cpp:21,41`. They move into a small header of the fork's, so the strip draws the same icon
  with the same check (`kiwami::hasIPv4("tailscale0")`, `:229`). *Proposed* for the plan.
- **A low-battery notice on the panel** (the menu's `low.check`, `main.cpp:3316`, runs only in the menu's loop, so
  during a game the only warning is bananad's LED): *not taken*. The red battery at 10% and under is the warning
  here (round 2). Left as a later idea.
- **Left out**: the SSID, an internet check, seconds in time played, the companion pages and the keyboard (rounds 1
  and 2).

## Current state

**The game panel** (upstream's, `ui/src/settings_gamepanel.cpp`, 4 upstream commits, the last on 2026-11-07):

- `gameOpen` (`:334-378`) runs at each game's start: title, system, screen, touch, the RetroAchievements watch.
  `gameClose` (`:483-493`) runs at its end.
- `gameUpdate(t)` (`:579-620`) returns true when something wants a frame. `t` is `nowMs()`, `CLOCK_MONOTONIC`.
- `gameFrame` (`:803-978`) draws, in this order:
  - the keyboard alone while it's open (`:840`);
  - a companion's pages alone while they show (`:842-854`);
  - otherwise the tiles view:
    - the header box at (18, 14, 604×54), with the title on the left and the system on the right, or rich presence
      under the title (`:851-867`);
    - the achievements or save-states box at (18, 80, 604×206) (`:868-950`);
    - five or six tiles at y 300-420 (`:951-972`);
    - the hint centred at y 438 (`:973-975`);
    - the companion's corner icon (`:976`).
- Touch on the tiles view: the corner (`:657`), the achievements box toggles the save states (`:665-670`), the tiles
  at y 300-420 (`:705`). Nothing at y ≥ 424 outside the game companion's pages (`:709-719`).

**The loop** (`main.cpp:2559-2740`, `gamePanel`): `settingsView.gameOpen(...)` (`:2579`), then every pass
`gameUpdate(t)` and, when it says so and no flip is pending, `gameRender()` and `disp.present` (`:2622-2633`); poll
timeout 100 ms, 10 ms while the memory export is awaited (`:2651`). Everything runs on the main thread under
`worker.uiLock`.

**The information, already in the host** (`ui/src/library.{h,cpp}`):

- `clockText()` follows the Clock setting (`%H:%M` or `%I:%M %p`) and `dateText()` is `%m/%d`
  (`library.cpp:990-1008`).
- `battery()`, `charging()` and `power()` read `/sys/class/power_supply` at most every 2 s on `CLOCK_BOOTTIME`. A
  charger online counts as charging, because the rk817 gauge says "Discharging" while it charges
  (`library.cpp:1020-1057`, `library.h:89-107`).
- `online()` reads `/sys/class/net/wlan0/operstate` at most every 2 s (`library.cpp:1059-1065`).
- The fork reaches the library through `kiwami::library` (`ui/src/kiwami/library.h`, set at `main.cpp:2118`), under
  the host's `uiLock`, which the game panel's loop holds.

**The menu's status bar** (Dark and Light, `ui/rnds/RndsUI.cpp:1526-1604`, vendored):

- the Wi-Fi sprite tinted 1.0 or 0.35 by `online()`;
- the battery sprite with its level fill and charging bolt, plus the percentage in "text" mode;
- the clock right-aligned at 622 and the date left of it.

No signal strength anywhere in the menu. Layout themes see `online` as 0/1 (`ui/src/layout.cpp:545`); Carts shows
no Wi-Fi. Bars exist only in Settings > Network's scan list (`settings.cpp:3073`, `:3128`, `wifi::Network::bars()`,
`ui/src/wifi.h:24`).

**Settings' drawing** that the fork can use: `Settings::Kiwami` is nested in `Settings` (`ui/src/settings.h:692`,
`ui/src/kiwami/kiwami.h`), so it calls Settings' private `text`, `box`, `fit`, `X`, `be` and the theme's `assets`,
and gets the palette through `Kiwami::colours(light)` (`settings.cpp:208`). `kw` exists from Settings' first
`build()` (`kiwami.cpp:62`). `notice()` already draws the theme's battery icon and turns the level red at 10% and
under (`settings.cpp:2101-2120`). Precedent for drawing into an upstream screen from a hook: `kw->syncTimes(...)`
(`settings.cpp:6949`).

**Second screen's rows** (`settings.cpp:630-650`): Second screen (`retroarch_panel`), Game panel when idle, Game
panel idle time, Pokémon companion, then the fork's GB/GBC companion rows (`Kiwami::rows`, `:643`;
`kiwami.cpp:116-138`), Challenge mode and Game companion. The fork's settings keys start with `kiwami_` in
`settings.conf` (`kiwami_pokemon_companion_gb`, `kiwami_sync_times`).

**Upstream (`main`)** has no status on the game panel. `settings_gamepanel.cpp` is identical on `main` and
`kiwami` except for one `kiwami:` line (`:557`, Back in a Game Boy game).

## What has to change

- **New fork code** in `ui/src/kiwami/` (e.g. `panel-status.cpp`, declared in `kiwami.h`), which:
  - notes the session's start (`CLOCK_MONOTONIC`) and reads `kiwami_panel_status` from `settings.conf` at each
    game's start;
  - on each `gameUpdate` pass:
    - turns true when the minute changes;
    - every 10 s (`CLOCK_BOOTTIME`), reads the battery, Wi-Fi and Tailscale and turns true when a reading differs
      from what is drawn;
    - logs a line when what is drawn changes, for the tests and the device;
  - draws the strip:
    - Wi-Fi (the theme's sprite, dimmed when down) and Tailscale's mark on the left;
    - "Played h:mm" in the centre;
    - on the right, the percentage, the battery sprite with level and bolt (red at 10% and under), the date and the
      clock;
    - at both scales, in the panel's palette.
- **The row**: `Kiwami::rows(..., "Second screen")` adds `Status on the game panel` (`kiwami_panel_status`, Off/On,
  default Off), inserted after Game panel idle time.
- **Tailscale's mark** moved from `statusbar.cpp` into a header of the fork's, shared by both.
- **Hooks in upstream's files** (each one line, or two around unchanged lines, marked `kiwami:`):
  - `settings_gamepanel.cpp` `gameOpen`: `if (kw) kw->panelOpened();`. The start time and the setting, per game.
    It can't be avoided: Settings' own `now` there is whatever the last `update()`, `gameButton()` or
    `gameUpdate()` set (`settings.cpp:2650`, `settings_gamepanel.cpp:554`, `:581`), which can be minutes old, so
    the fork can't take the start from it.
  - `settings_gamepanel.cpp` `gameUpdate`: `if (kw && kw->panelUpdate(t)) changed = true;`. Without it the strip
    is never redrawn.
  - `settings_gamepanel.cpp` `gameFrame`: the hint's three lines wrapped as `if (!kw || !kw->panelStatus()) {` …
    `}`, unchanged and not re-indented. The strip is drawn instead of the hint.
  - `main.cpp` `runHeadless`: `kiwami::library = &lib;`, so the headless tests reach the battery verb's readings.
- **Tests** in `kiwami/tests/` (*Testing*); `kiwami/tests/gbpk-ui.sh:71` updated for the new row.
- No packages, no rootfs files, no dependencies, nothing in `ui/rnds/`.

## Options

**A. A strip in the hint's place, drawn by the fork through `Settings::Kiwami`** (*chosen*, round 2). It has room for
everything, leaves the header and tiles as they are, and touches upstream in three small places in one rarely
changed file.

**B. The strip around the hint.** The hint stays, with about 135 px free on each side: only Wi-Fi/Tailscale and the
battery and clock fit, with no date and no time played. Not chosen.

**C. In the header**, like a phone's status bar, instead of the system name. It changes upstream's header layout
(more hook lines in its width arithmetic), cuts long titles shorter, and still needs the bottom line for the date and
time played. Not chosen.

**D. A pass-through backend**, as 0034 did for the menu. It isn't needed: the game panel is Settings' own drawing,
which the fork can already reach through `Settings::Kiwami`, while 0034's bar was drawn by the vendored engine. Not
chosen.

## Risks

- **Performance and footprint**:
  - Each 100 ms pass adds a `clock_gettime` and a minute comparison.
  - Every 10 s, a scan of `/sys/class/power_supply`, one read of `wlan0`'s operstate and one `ioctl` on
    `tailscale0`: microseconds, beside the game.
  - A redraw a minute (more when the battery or network changes) sends only the strip's pixels.
  - No thread, no timer of its own, nothing while the menu is idle; a few KB of code and two small textures.
  - With the setting off, `panelUpdate` returns at once and nothing is read.
- **Conflicts with upstream**:
  - `settings_gamepanel.cpp` has had 4 upstream commits. The hooks sit in `gameOpen`, `gameUpdate` and around the
    hint, which a redesign of the panel's bottom line would conflict with (a visible, textual conflict).
  - `runHeadless`'s hook is one line in `main.cpp`, which upstream changes often, but rarely there.
  - The fork's row depends on the key `bottom_idle_seconds` existing. If it went, the row would fall to the end of
    Second screen. The test of the row's place would show that.
- **Hardware and boards**: the same on `rgds`, `rgds-plus` and `universal`. Only the device shows the real battery
  and charger readings, Wi-Fi dropping and coming back, and the strip around a sleep. The user has only an RG DS
  Plus, so the RG DS (and its module unload around sleep) isn't tested on hardware.
- **The 12-hour clock's width** at 1024x768 with 100%: to be measured in the plan's shots.

## Testing

- **Headless** (a new `kiwami/tests/panel-status-ui.sh`, run by `ci.sh`), using `gamepanel 1 <rom>` and `gshot` at
  640x480 and 1024x768:
  - setting off: the panel's shots identical to upstream's (the hint present), and no strip log line;
  - setting on, with a fixed time and the Wi-Fi state overridden:
    - the strip instead of the hint;
    - a log line naming what is drawn (time, date, battery, charging, Wi-Fi, Tailscale, played);
    - battery at 78%, 100%, 9% (red) and charging (the `battery` verb);
    - Wi-Fi up and down; Tailscale up (`KIWAMI_TS_IFACE=lo`) and down;
    - 24h and 12h;
    - six tiles (the keyboard tile) and no touch (the non-touch hint replaced too);
    - light and dark;
  - a `wait` across a minute boundary of the fixed clock: the log shows a redraw with the next minute.
  - The row: its place after Game panel idle time, Off by default, `gbpk-ui.sh`'s row list updated.
- **`make test`** must pass unchanged: upstream's fixture has the setting off.
- **On the device** (RG DS Plus), setting on, a GBA game through RetroArch and one through BananaFront:
  - the strip at the start;
  - the clock turning at the minute;
  - the battery percentage falling and the bolt on the charger;
  - Wi-Fi switched off and back on (within 10 s);
  - Tailscale on and off;
  - a sleep of a few minutes: the clock right at once after resume, and time played not counting the sleep;
  - the panel on the top screen (RetroArch on the bottom);
  - the companion's pages, then F to the menu with the strip.

  To collect: photos of the panel, and `/data/banana/logs/banana-ui.log`'s strip lines (the log names what is
  drawn and when it changed).

## Open questions

1. Which views show the status? *Decided*: the tiles view only (round 1).
2. What is shown besides the clock, battery and Wi-Fi? *Decided*: Tailscale and time played; signal strength only if
   the menu has it. It doesn't, so it's #37 (round 1).
3. What does "network connectivity" mean? *Decided*: the Wi-Fi link, as in the menu (round 1).
4. How does the battery show? *Decided*: always the icon and the percentage (round 1).
5. Where is the strip? *Decided*: the hint's line, replacing it (round 2).
6. How often does it refresh? *Decided*: the time at the minute, battery and network every 10 s (round 2).
7. How does time played look? *Decided*: `h:mm` (round 2).
8. What gates it? *Decided*: a Second screen row, off by default (round 2, after the fork's rule was pointed out).
9. Which languages? *Decided*: English, as the fork's other rows (round 2).
