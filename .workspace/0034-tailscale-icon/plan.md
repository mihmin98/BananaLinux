# 0034 Tailscale icon: plan

Issue: #34 · Branch: `k/0034-tailscale-icon` · Investigation: [investigation.md](investigation.md) · Status: approved

## Summary

While the handheld is in its tailnet (`tailscale0` has an IPv4 address), the Dark and Light themes' status bar shows
Tailscale's icon right after the Wi-Fi icon. It is drawn with the Wi-Fi icon's tint, so it dims when Wi-Fi is down,
and it rises with the bar after a move. The battery, the battery percentage, the brand plate, the version and the logo
move right to make room; the date and the clock stay where they are. Nothing changes in any other state, theme or
screen.

The bar is drawn by the vendored ROCKNIXDS engine (`ui/rnds/`), which the fork doesn't edit. So the fork puts a
pass-through `rnds::Backend` of its own (`ui/src/kiwami/statusbar.{h,cpp}`) between the engine and the host's
backend:

- It recognises the Wi-Fi sprite's draw on the top screen and draws the icon after it.
- It moves the left part of the bar right until it reaches the clock.
- When the icon isn't shown, it forwards everything unchanged.

Three `kiwami:` lines in `ui/src/main.cpp` give it to `RndsMenu`: the include, the headless menu and the device's
menu.

## Acceptance criteria

- **A1**: given Tailscale off, or `tailscale0` with no address (logged out, an expired key, waiting for a login), when
  the menu draws, then the bar is upstream's, pixel for pixel, and no icon line is logged.
- **A2**: given `tailscale0` with an IPv4 address, when the Dark or Light menu's top screen is drawn, then:
  - Tailscale's icon is drawn one gap (8 px at 640x480, 13 px at 1024x768) after the Wi-Fi icon, top-aligned with it
    and with its tint;
  - every later draw of the bar's left part (battery, its level and bolt, the percentage, the brand plate, the stage
    badge and version, the logo) is moved right by the icon's width plus that gap: 24 px at 640x480, 39 px at
    1024x768;
  - the date and the clock, and everything below the bar, are unmoved.

  This holds on the home screen, a game list and the ready screen, at both panel sizes.
- **A3**: given the tailnet up and Wi-Fi down (the Wi-Fi icon at 35%), then the icon is drawn at the same 35%. With
  Wi-Fi up, both are drawn in full.
- **A4**: given the bar rising after a cursor move (Motion on), then in every frame the icon is drawn at the Wi-Fi
  icon's y.
- **A5**: given the icon shown with the most crowded bar (battery "icon and text" at 100%, charging, a 12-hour clock,
  the version note "Alpha 0.5.3.4"), then the left part ends at least 8 px (device: `round(8 × scale)`) before the
  date's left edge, at both sizes.
- **A6**: given the engine's bar drawn some other way (no Wi-Fi draw found where it is expected), when the icon would
  be shown, then nothing is drawn or moved, and `kiwami: tailscale icon: no Wi-Fi icon in the bar` is logged once.
- **A7**: the check runs only while the menu's top screen is drawn, at most every 2 s (an `ioctl`). No thread, timer or
  wake-up is added. The icon appears or goes at the menu's next redraw: a press, the clock's minute, or a resume.
- **A8**: the layout themes, Carts, and Settings' screens are unchanged with the tailnet up. Neither are the host's
  overlays (the notices, the flight recorder's dot), which are drawn on the host's backend.
- **A9**: headless runs (`make test`, the theme previews Settings renders) never show the icon unless
  `KIWAMI_TS_IFACE` names an interface, so they don't depend on the host's own Tailscale.
- **A10**: on the RG DS Plus with a logged-in tailnet:
  - the icon shows after boot and after switching Tailscale on (by the next press or minute);
  - it goes after switching Tailscale off, after a logout, and when the login is expired;
  - it dims with Wi-Fi off and through a sleep and resume;
  - it looks right in Dark and Light.

## Risks

- **The engine's draw order.** The backend relies on how `drawStatus` (`ui/rnds/RndsUI.cpp:1527-1604`) draws:
  1. Wi-Fi at (18, 24 + rise), the first small texture at that x;
  2. then the left part;
  3. then the clock, right-aligned at 622 (the first draw reaching past x 560);
  4. then the date.

  A ROCKNIXDS re-import that changes this merges without a conflict. Guards:
  - `kiwami/tests/tailscale-icon-ui.sh` runs the real engine and checks the icon line, the shift and the room on every
    `ci.sh` run.
  - The backend draws nothing when it can't find Wi-Fi (A6).
  - `docs/FORK.md`'s *Taking in upstream* item 3 gets `ui/rnds/RndsUI.cpp` to read by hand after a merge.
- **Moving draws over the dithered background.** Glyph edges moved onto other background pixels look slightly
  different from the same text unmoved. Expected and harmless: the tests compare the unmoved regions exactly and the
  moved ones only by position (log), not by pixels.
- **Performance and footprint**:
  - one virtual call more per draw (a few hundred per frame), and an `ioctl` at most every 2 s while the top screen
    is drawn: microseconds against the millisecond frame budget;
  - nothing while idle;
  - two A8 textures (16x16 or 26x26) uploaded once;
  - a few KB of code in banana-ui;
  - no rootfs files, packages or dependencies.

  `make test`'s `bench` lines in the final check show the frame time is unchanged.
- **Conflicts with upstream.** `ui/src/main.cpp` changes often (158 upstream commits), but the two `RndsMenu` lines
  have changed only once or twice each. A conflict is putting the call back around `&be`. The include sits with the
  other `kiwami/` includes.
- **The address as the signal** (investigation, oversight 3): the icon may show while waiting for an admin's approval.
  Only the device can show it (M3, noted if seen). It was accepted.
- **Hardware and boards.** The same code on `rgds`, `rgds-plus` and `universal`. Only an RG DS Plus is available, so
  the manual tests run there. The RG DS's Wi-Fi module unload around a sleep isn't tested on the device; the dimming
  follows the Wi-Fi icon's own tint either way.

## Steps

Each step is one commit, and leaves the tree building and `make test` passing.

### 1. The backend that puts the icon after Wi-Fi

- **Files**:
  - `ui/src/kiwami/statusbar.h` (new)
  - `ui/src/kiwami/statusbar.cpp` (new)
  - `kiwami/tests/statusbar-units.cpp` (new)
  - `kiwami/tests/ci.sh` (changed)
  - `.workspace/0034-tailscale-icon/plan.md` (changed: Progress)
- **Change**:
  - `statusbar.h`: the header comment says what it does, what it relies on in the engine (the draw order above), and
    who calls it.

    ```cpp
    namespace banana::kiwami
    {
        // Tailscale's icon after the Wi-Fi icon in ROCKNIXDS Pixel's status bar (Dark and Light): every call goes to
        // the host's backend; on the top screen, while the tailnet is up, the Wi-Fi sprite's draw is followed by the
        // icon, and the bar's left part after it is moved right, up to the clock
        class StatusBar : public rnds::Backend
        {
        public:
            // up: the tailnet is up (asked at most every 2 s, when the top screen is drawn); now: seconds, monotonic
            StatusBar(rnds::Backend* real, float scale, std::function<bool()> up, std::function<double()> now);
            void retarget(rnds::Backend* real, float scale);	// a menu made again (a theme change)
            std::function<void(const std::string&)> log;		// a line when what is drawn changes
            rnds::TexPtr upload(const rnds::Bitmap& b, bool smooth) override;
            void draw(const rnds::TexPtr& t, float x, float y, float w, float h, const rnds::Color& tint) override;
            void fill(float x, float y, float w, float h, const rnds::Color& c) override;
            void pushClip(float x, float y, float w, float h) override;
            void popClip() override;
            void beginScreen(int screen) override;
            void endScreen() override;
        };
        bool hasIPv4(const char* iface);	// SIOCGIFADDR on a datagram socket: false with no interface or no address
        // the process's one (made on the first call, retargeted on the next), for main.cpp's hooks: up is
        // hasIPv4("tailscale0"), or KIWAMI_TS_IFACE's interface. Headless: never up without KIWAMI_TS_IFACE, and
        // KIWAMI_BRAND_NOTE sets rnds::brandNote (the version note only the device has)
        rnds::Backend* statusBar(rnds::Backend* real, float scale, bool headless);
    }
    ```

  - The frame logic in `statusbar.cpp`, per top screen (`beginScreen(0)` resets it; screen 1 is passed through
    untouched):
    - `up()` is asked when the top screen begins, if 2 s have passed since the last time. While it is false, every
      call is forwarded unchanged.
    - **Seeking Wi-Fi.** The first `draw` that matches the Wi-Fi sprite ends this phase. To match, it must be at
      `x == round(18 × scale)`, with a texture at most `round(24 × scale)` wide and `round(20 × scale)` tall, and its
      top within `[round(14 × scale), round(36 × scale)]` (rest and rise). It is forwarded. Then the icon is drawn
      after it:
      - with the Wi-Fi draw's tint;
      - at `x + texture width + round(8 × scale)`, and at the Wi-Fi draw's y;
      - the shift is the icon's width plus that gap.
    - **Moving the left part.** Each later `draw`, `fill` and `pushClip` is forwarded with `x + shift`, until the
      first one whose right edge passes `round(560 × scale)` or whose top is at or below `round(58 × scale)`. That
      one, the clock, and everything after it are forwarded unchanged. `popClip` is always forwarded.
    - **Recorded for the log**: the furthest right edge of the moved draws (the left part's end), and the leftmost x
      of the draws in the bar after the stop (the clock's and the date's start).
    - **No Wi-Fi.** If the screen ends while still seeking and the icon was due, nothing was changed. The line
      `kiwami: tailscale icon: no Wi-Fi icon in the bar` is logged once per process.
    - **The log line**, written when its values change (on the icon showing or going, or the bar's layout changing;
      not once per frame of a rise):
      - `kiwami: tailscale icon: shown, alpha <a>, <n> draws moved <d> px, the left part ends at <e>, the clock's
        part starts at <s>`;
      - `kiwami: tailscale icon: hidden`.
  - **The icon**: two pixel maps in the code, in `ra-icon.cpp`'s style (`'#'` solid, `'+'` faded at 35%, `'.'`
    none). It is made into an A8 `rnds::Bitmap` on its first use at that scale and uploaded through the host's backend
    once.
    - 16x16 for scale < 1.3: 4x4 dots, 2 px apart.
    - 26x26 for 1.6: 6x6 dots with their corners cut, 4 px apart.
    - Tailscale's mark: the middle row and the bottom centre solid, the other five faded.
    - The picks match the engine's `@1x`/`@1.6x` choice (`s > 1.3f`, `RndsUI.cpp:505`).
  - `hasIPv4()`: `socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC)` once (kept), `ioctl(SIOCGIFADDR)` on the name.
  - `statusBar()`:
    - Holds the one `StatusBar` (function-local static) with `up` and `now` (`CLOCK_MONOTONIC`).
    - Logs to stderr as the fork's code does (`banana-ui: kiwami: ...`, as in `kiwami.cpp:138`).
    - Retargets on later calls: `makeMenu` (`main.cpp:2177`) makes a menu again on a theme change, and only one
      `RndsMenu` exists at a time.
  - `ci.sh`: after the `units` block, build and run the unit test like the others:

    ```sh
    ${CXX:-g++} -std=c++17 -Wall -Werror -I ui/src -o "$T/statusbar-units" kiwami/tests/statusbar-units.cpp \
        ui/src/kiwami/statusbar.cpp
    "$T/statusbar-units"
    ```

  - Nothing calls `statusBar()` yet. The new `.cpp` is compiled into banana-ui by the Makefile's existing
    `src/kiwami/*.cpp` line, unused.
- **Tests**:
  - `kiwami/tests/statusbar-units.cpp` uses a recording fake backend, `up` and `now` given by the test, and checks:
    - **Off**: a whole frame is forwarded identically, on both screens, and nothing is uploaded (A1).
    - **On, 640x480** (scale 1): the frame is wallpaper, Wi-Fi 18x16 at (18, 24), battery at (44, 25), a fill at
      (47, 28), the plate at (78, 19), the version text at (236, 22), the logo at (85, 22), the clock (70 wide) at
      (552, 20), the date at (489, 20), the sheet at (18, 60). The result:
      - the icon is 16x16 at (44, 24) with the Wi-Fi's tint, right after the Wi-Fi draw;
      - the battery is at 68, the fill at 71, the plate at 102, the text at 260 and the logo at 109;
      - the clock, the date and the sheet are unmoved;
      - the log says `… 5 draws moved 24 px, the left part ends at 320, the clock's part starts at 489` (A2).
    - **1024x768** (scale 1.6): Wi-Fi 29x26 at (29, 38). The icon is 26x26 at (71, 38), and the shift is 39 (A2).
    - **The rise**: Wi-Fi at (18, 34); the icon is at (44, 34) (A4).
    - **Dim**: the Wi-Fi tint at alpha 0.35; the icon's is 0.35 (A3).
    - **No Wi-Fi**: a frame without it is forwarded identically, and the line is logged once over two frames (A6).
    - **Screen 1**: draws there at Wi-Fi's place are never touched.
    - **The check's pace**: `up` is asked once over frames 1.9 s apart, and again after 2 s (A7).
    - **The log**: one `shown` line over ten frames of a rise; `hidden` once when `up` turns false.
    - **The icon**: uploaded once per scale; its A8 pixels are the solid and faded dots of the map.
    - **`hasIPv4`**: true for `"lo"`, false for `"kiwami-none0"`.
  - Pass at the end of the step: `kiwami/tests/ci.sh .buildroot`, `make test`, `make -C ui`.
- **Commit**: `Tailscale icon: a backend that puts it after the Wi-Fi icon`

### 2. The menu's bar through it

- **Files**:
  - `ui/src/main.cpp` (hook ×3)
  - `kiwami/tests/tailscale-icon-ui.sh` (new)
  - `kiwami/tests/ci.sh` (changed)
  - `docs/FORK.md` (changed)
  - `.workspace/0034-tailscale-icon/plan.md` (changed: Progress)
- **Change**:
  - `main.cpp`, three `kiwami:` lines:
    - after `#include "kiwami/gbpk/game.h"` (`:45`):
      `#include "kiwami/statusbar.h"	// kiwami: Tailscale's icon in the status bar`
    - in `runHeadless` (`:857`), `&be` becomes `kiwami::statusBar(&be, o.h / 480.0f, true)`, marked
      `// kiwami: Tailscale's icon in the status bar`;
    - in `makeMenu` (`:2182`), `&be` becomes `kiwami::statusBar(&be, sc, false)`, marked the same.

    Carts and the layout themes keep `&be`; Settings and the overlay keep drawing on `be` (A8).
  - The icon's art can be adjusted here, once the shots below are seen. If it changes, step 1's unit checks of the
    pixels are updated in this commit.
  - `docs/FORK.md`, *Taking in upstream* item 3: add `ui/rnds/RndsUI.cpp` to the files read by hand, with a sentence.
    The fork's `kiwami::StatusBar` relies on `drawStatus`'s draw order (Wi-Fi first at (18, 24), the left part, then
    the clock right-aligned past x 560); a change there shows in `tailscale-icon-ui.sh`.
  - `ci.sh`: `if [ -x ui/banana-ui ]; then kiwami/tests/tailscale-icon-ui.sh; else echo "tailscale-icon-ui: skipped, no ui/banana-ui (make ui)"; fi`,
    after `tailscale-ui.sh`'s line.
- **Tests**:
  - `kiwami/tests/tailscale-icon-ui.sh`, in `tailscale-ui.sh`'s shape (the fixture from `ui/tests/fixture.sh`, shots
    in `$OUT`). It runs headless at 640x480 and 1024x768, with the script
    `home 0; wait 2400; shot home; lib 0 0; wait 2400; shot lib; press right; wait 100; shot rise; press start; wait 400; shot settings`.
    1. **Off**: no `KIWAMI_TS_IFACE`, then `KIWAMI_TS_IFACE=kiwami-none0`. No `tailscale icon` line, and the two runs'
       shots are identical (`cmp`) (A1, A9).
    2. **On**: `KIWAMI_TS_IFACE=lo`, Dark.
       - One `shown` line, `moved 24 px` (39 at 1024x768).
       - The alpha is `1.00` if the host's `/sys/class/net/wlan0/operstate` says `up`, else `0.35` (A3, as the Wi-Fi
         icon reads it).
       - `ends at` + `round(8 × scale)` ≤ `starts at` (A2, A5).
       - Against run 1's shots: the `home` and `lib` top screens differ in the bar's left part. They are identical
         right of `starts at`, and below `round(58 × scale)` (ImageMagick crops, `compare -metric AE` = 0) (A2).
       - The `settings` shots are identical to run 1's (A8).
       - The headless clock is the real time, so runs 1 and 2 are made within one minute. Before run 1, the test
         waits for the next minute if fewer than 15 s of this one are left.
    3. **Crowded**: `KIWAMI_TS_IFACE=lo`, Light, `battery = percent`, `clock = 12h` in `settings.conf`,
       `battery 100 charging` in the script, `KIWAMI_BRAND_NOTE="Alpha 0.5.3.4"`. The `shown` line, with
       `ends at` + `round(8 × scale)` ≤ `starts at` (A5).
    4. **Other themes**: `KIWAMI_TS_IFACE=lo` with `theme = carts`, and with a layout theme (`theme = ocean`). No
       `tailscale icon` line (A8).
    5. **The shots** are checked for being there and not blank, as `tailscale-ui.sh` does. The `home-top` shots of
       runs 2 and 3 are left in `$OUT` for a look.
  - Pass at the end of the step: `make test` (no `KIWAMI_TS_IFACE`: unchanged), `kiwami/tests/ci.sh .buildroot`,
    `make -C ui`, `kiwami/tests/ui-objects.sh`.
- **Commit**: `Tailscale icon: the menu's status bar draws through it`

### 3. Final check

- `make test`, `make -C ui`, `make -C system/bananad CFLAGS="-O2 -Werror"`, `kiwami/tests/ci.sh .buildroot`
  (`statusbar-units` and `tailscale-icon-ui.sh` among them).
- `make test`'s `bench` frame times compared with `main`'s build: unchanged within noise (A7).
- The hooks: `git diff --stat main...HEAD` and `git grep -n 'kiwami:'` (the commands in `docs/FORK.md`) agree, and
  list only the three new `main.cpp` lines besides the existing hooks.
- With Tailscale off (no `KIWAMI_TS_IFACE`), `make test`'s renders in `ui/tests/out/` match `main`'s (A1).
- `kiwami/DEPENDENCIES.md`: nothing added (no dependency).
- `docs/FORK.md` updated (step 2). `docs/ARCHITECTURE.md` and `CLAUDE.md` describe nothing this changes.
- An image for `rgds-plus` (`make rgds-plus`, or `make banana-ui-rebuild-rgds-plus` and the image) for the manual
  tests below; they all passed or are noted.
- Every acceptance criterion is met: each is listed by at least one automated or manual test, and those passed.
- Progress marked done.
- **Commit**: `Tailscale icon: the final check`

## Automated tests

| Test | Where | What it checks | Criteria |
|---|---|---|---|
| Off is pass-through | `kiwami/tests/statusbar-units.cpp` | a whole frame forwarded identically, nothing uploaded | A1 |
| The icon and the shift | `kiwami/tests/statusbar-units.cpp` | icon after Wi-Fi at 640x480 and 1024x768, positions and shift exact, the clock's part unmoved | A2 |
| Dim and rise | `kiwami/tests/statusbar-units.cpp` | the icon takes Wi-Fi's tint and y | A3, A4 |
| No Wi-Fi | `kiwami/tests/statusbar-units.cpp` | nothing changed, one log line | A6 |
| The pace | `kiwami/tests/statusbar-units.cpp` | `up` asked at most every 2 s, only on the top screen | A7 |
| `hasIPv4` | `kiwami/tests/statusbar-units.cpp` | `lo` true, a missing interface false | A1, A9 |
| Off headless | `kiwami/tests/tailscale-icon-ui.sh` | no line, shots identical with no or a missing interface | A1, A9 |
| On, the real engine | `kiwami/tests/tailscale-icon-ui.sh` | the line, the shift, the alpha as Wi-Fi's, regions right and below unchanged, at both sizes | A2, A3 |
| Crowded bar | `kiwami/tests/tailscale-icon-ui.sh` | Light, percentage, charging, 12-hour clock, version note: room left before the date | A5 |
| Other themes and Settings | `kiwami/tests/tailscale-icon-ui.sh` | Carts and a layout theme log nothing; Settings shots unchanged | A8 |
| Upstream's tests | `make test` | unchanged with no `KIWAMI_TS_IFACE` | A1, A9 |

## Manual tests

On the device, for what headless tests can't reach. Logs are fetched over SSH/SFTP (Settings > Network > SSH) or from
the card on a computer (`banana/logs/` on the card is `/data/banana/logs/` on the device). The icon's own lines are in
banana-ui's log, `/data/banana/logs/banana-ui.log`. Tailscale's helper writes `/data/banana/logs/tailscale.log`.
Run `ip -4 addr show tailscale0` over SSH where asked.

| # | Board | Steps | Expected | Logs to collect | Criteria | Outcome |
|---|---|---|---|---|---|---|
| M1 | rgds-plus | Tailscale on and logged in, Wi-Fi on. Restart; wait for the menu; press a direction after ~10 s. | The icon after Wi-Fi, battery and brand moved right, date and clock where they were; nothing overlaps. A photo of the top screen. | `banana-ui.log` (`kiwami: tailscale icon: shown …`), `tailscale.log`, `ip -4 addr show tailscale0` | A2, A5, A10 | not run |
| M2 | rgds-plus | Move left and right on the home screen and in a game list. | The icon rises with the bar every time, never apart from Wi-Fi. | — | A4, A10 | not run |
| M3 | rgds-plus | Settings > Network > Tailscale: switch off, back to the menu, press. Switch on, back, wait ~10 s, press. Then Log out, back, press. | Off: no icon, the bar as before. On: the icon back. Logged out (waiting for the login): no icon. | `banana-ui.log` (`hidden` / `shown` lines), `tailscale.log`, `ip -4 addr show tailscale0` after each | A1, A7, A10 | not run |
| M4 | rgds-plus | Logged in, Settings > Network > Wi-Fi Off, back to the menu, press. Then Wi-Fi On, wait, press. | Wi-Fi off: the icon dimmed like the Wi-Fi icon. On: both in full. | `banana-ui.log` (alpha `0.35`, then `1.00`) | A3, A10 | not run |
| M5 | rgds-plus | Logged in, close the lid (sleep) for a minute, open it. Look at once and after Wi-Fi reconnects (press). | The icon still there; dimmed while Wi-Fi reconnects if the Wi-Fi icon is, then full. | `banana-ui.log`, `tailscale.log`, `/run/banana/messages` (tailscaled reconnecting) | A3, A7, A10 | not run |
| M6 | rgds-plus | Settings > Menu look > Theme Light, then a layout theme, then Carts; back to Dark. | Light: the icon in Light's ink. Layout theme and Carts: their own bars, no icon. | `banana-ui.log` | A8, A10 | not run |
| M7 | rgds-plus | Settings > Menu look: Battery as percentage, Clock 12h, at 100% on the charger. | Nothing overlaps; room left before the date. A photo. | `banana-ui.log` (`ends at` / `starts at`) | A5 | not run |
| M8 | rgds-plus | If a key expiry or a device approval can be arranged (admin console: expire the key, or require approval and log in again). Optional. | Expired: no icon. Approval wait: noted whether the icon shows (accepted either way). | `banana-ui.log`, `tailscale.log`, `ip -4 addr show tailscale0` | A1, A10 | not run |

Notes from the runs (what the logs showed, what was fixed):

## Deviations

Where the implementation turned out different from this plan, why, and what changed (dated).

- 2026-11-16, step 1: the log's "the left part ends at" is the furthest right edge of the moved draws, not the
  right edge of the last one. The engine draws the logo after the version text, and the logo ends before it; the
  step's own expected value (`ends at 320`, the version text's edge) was already the furthest. Step 1's wording fixed.

## Progress

A step is marked done in its own commit, so the commit column holds the subject (a commit can't carry its own hash).

| Step | Status | Commit |
|---|---|---|
| 1. The backend that puts the icon after Wi-Fi | done | Tailscale icon: a backend that puts it after the Wi-Fi icon |
| 2. The menu's bar through it | todo | |
| 3. Final check | todo | |
