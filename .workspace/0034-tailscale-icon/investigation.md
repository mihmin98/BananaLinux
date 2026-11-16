# 0034 Tailscale icon: investigation

Issue: #34 · Branch: `k/0034-tailscale-icon` · Status: draft · Confidence: 97% (proceed)

## The request

Show at a glance, on the top screen's status bar, that the handheld is in its tailnet, so it can be reached over SSH
or File sharing without opening Settings > Network > Tailscale (feature 0001). The icon sits right after the Wi-Fi
icon.

Decided with the user (question round 1):

- **Themes**: Dark and Light only, the default theme family, whose bar is drawn by ROCKNIXDS Pixel's engine. The
  layout themes and Carts are left as they are.
- **When it shows**: while the handheld is logged in to its tailnet. It is drawn at the Wi-Fi icon's opacity: in
  full while Wi-Fi is up, dimmed (35%) while it is down. Nothing shows while Tailscale is off, starting, waiting for
  a login, or after the login expired. The Tailscale row in Settings already says which of those it is.
- **No setting of its own**: the Tailscale switch (off by default) is the only gate. With Tailscale off the bar is
  exactly upstream's, which satisfies the fork's rule that changed behaviour defaults to upstream's.

Decided with the user (question round 2):

- **How**: a pass-through drawing backend of the fork's own in front of the engine (*Options*, A). The battery, the
  battery percentage, the brand plate and the version move right to make room while the icon shows.
- **The look**: Tailscale's logo in the theme's ink. Nine dots in a 3x3 grid: the T (the middle row and the bottom
  centre) solid, the other five faded.
- **Detection**: `tailscale0` has an IPv4 address. Checked when the bar is drawn, at most every 2 s, as the Wi-Fi
  check does.
- **Timing**: like the Wi-Fi icon. The icon changes at the menu's next redraw: a press, the clock's minute, or a resume.

Out of scope: the layout themes (upstream's 15 JSON themes, and a user's), Carts, Settings' screens (they have no
status bar), the bottom panel, and a setting for the icon.

## Oversights in the requirements

1. **There is no room next to Wi-Fi, and the bar isn't the fork's.** The engine draws the Wi-Fi icon at x 18
   (18x16 px at 640x480), the battery at 44, and the brand plate at 78 (`ui/rnds/RndsUI.cpp:1527-1560`). Only
   8 px separate Wi-Fi from the battery. `ui/rnds/` is vendored and the fork never edits it (`docs/FORK.md`).
   *Decided*: the fork's backend shifts what follows the Wi-Fi icon in the left group (*Options*, A).

2. **The bar moves.** On every cursor move the whole top screen, status bar included, is drawn 10 px lower and rises
   back over 280 ms (`rise(topAt)`, `RndsUI.cpp:1515`; `topAt = now` in `render(animate)`, `:1077`; off with
   Settings > Menu look > Motion). An icon drawn at fixed coordinates from outside the engine would stay still while
   the Wi-Fi icon moves.
   *Decided*: the icon is drawn at the Wi-Fi draw's own y, so it rises with it.

3. **Detection, and waiting for approval.** In the Tailscale 1.98.8 source the fork builds
   (`ipn/ipnlocal/local.go:6080-6101`), NeedsLogin (expired keys included), Stopped and NoState reconfigure the engine
   with an empty config, which removes `tailscale0`'s addresses (`wgengine/router/osrouter/router_linux.go:421-453`).
   Running has them. Starting and NeedsMachineAuth call `authReconfigLocked()`, which assigns them once a netmap
   exists. So the icon may also show while the device waits for an admin's approval, a tailnet setting that is off
   by default. With Tailscale switched off, `tailscaled` is stopped and `tailscale0` is gone.
   *Decided*: accepted. The address check is microseconds; asking tailscaled for its state means a request to another
   process from the drawing path.

4. **Lag.** The menu redraws only on input, on the clock's minute (`main.cpp:3266-3267`) and after a resume. After
   boot (tailscaled reaches Running some 4-5 s after the menu's first frame, 0001's M5) or after switching Tailscale
   on, the icon can appear up to a minute late. The Wi-Fi icon behaves the same (`Library::online()`,
   `library.cpp:1059`).
   *Decided*: like Wi-Fi. An rtnetlink watch would need more hooks in `main.cpp`'s loop.

5. **Wi-Fi down while in the tailnet.** tailscaled stays Running and keeps its address when `wlan0` goes. On the
   RG DS, `sleep.d/10-wifi` unloads the module around a suspend. The Wi-Fi icon dims then (`RndsUI.cpp:1531`).
   *Decided*: the Tailscale icon takes the Wi-Fi draw's tint, so it dims with it.

6. **Room on the right.** The shift has to leave the date and clock alone and not run into them. Estimated at
   640x480 with the note "ALPHA 0.5.3":
   - The left group ends near x 360 today, and near 385 with the icon.
   - A 12-hour clock ("01:01 PM") puts the date's left edge near 470.
   - A longer stage name in `/etc/banana/stage`, or the battery's "icon and text" mode (the percentage before the
     battery, `:1565-1575`), takes some of that room.

   *Proposed*: the plan measures it in headless shots: both sizes, every battery mode, note shown, 12-hour clock.

7. **Both boards, both sizes, sleep, first boot, updates.** Nothing board-specific: the same bar on `rgds`,
   `rgds-plus` and `universal`. At 1024x768 the engine uses its `@1.6x` art (`RndsUI.cpp:505`, Wi-Fi 29x26), so the
   icon needs a second pixel map rather than a scaled one. Around sleep, the menu isn't drawn during a game or a
   suspend, and the SIGUSR1 redraw after a resume checks again. The feature has no state on the card, so first boot
   and A/B updates aren't affected.

## Suggestions

- **A headless way to show the version note** (`rnds::brandNote` is set only from `/etc/banana/version`,
  `main.cpp:2108-2115`, so headless shots never show it): an environment variable read by the fork's backend file
  for the tests, so the shift is checked with the note too. Costs a few lines in fork code. *Proposed* for the plan.
- **The interface the check reads, overridable for tests** (e.g. `KIWAMI_TS_IFACE=lo`, which always has an address):
  the headless test then needs no fake interface. *Proposed* for the plan.
- **Left out**: the layout themes. A `tailscale` hook in `layout.cpp` for users' own themes would be one line, but it
  was decided against (round 1). A warning icon for "Tailscale on but needs a login" was also left out (round 1).

## Current state

**The status bar** (Dark and Light, `ui/rnds/RndsUI.cpp`, vendored from ROCKNIXDS):

- `drawTop()` (`:1780-1790`) draws the wallpaper and the screen box, then `drawStatus(ry)` with `ry = rise(topAt)`.
- `drawStatus` (`:1527-1604`) draws, in this order:
  - the Wi-Fi sprite at (18, 24 + ry), tinted with `INK` at alpha 1 or 0.35 by `src->online()` (`:1530-1531`);
  - the battery: the sprite at 44 (or after the percentage text in "text" mode), its level fill and the charging
    bolt's fills (`:1532-1577`);
  - the brand plate at `brandX` (78, or after the percentage) (`:1578`);
  - the stage badge and the version (`:1579-1597`);
  - the logo sprite (`:1598-1599`);
  - the clock, right-aligned at 622, and the date to its left (`:1600-1603`).
- Sprites go to the backend as `be->draw(tex, round(X(x)) - ox, round(X(y)) - oy, w, h, tint)` (`drawSprite`,
  `:574-590`). `still("wifi", …)` uploads the Wi-Fi PNG once into the texture cache (`:707-713`). The art is
  `wifi@1x.png` (18x16) or `wifi@1.6x.png` (29x26) (`:505, :519`).
- The top screen takes no touches (`touch()` returns for `screen != 1`, `:2390`). Moving draws there can't put any
  touch area out of step.
- `ui/rnds/RndsUI.cpp` has had 39 upstream commits. The status lines are unchanged since the first import.

**The backend** (`ui/rnds/RndsUI.h:104-116`): `rnds::Backend` has seven virtual calls: `upload`, `draw`, `fill`,
`pushClip`, `popClip`, `beginScreen`, `endScreen`. The host's `SoftBackend` (`ui/src/soft.h`) records the draws and
diffs them per screen at `endScreen`, where it also calls the host's `overlay` (notices, the flight recorder's dot,
`main.cpp:2332-2347`). The overlay draws on the real backend, so nothing the fork's backend does reaches it.

**Where the menu gets its backend**: `RndsMenu(&lib, &be, …)` in `runHeadless` (`main.cpp:857`) and in `makeMenu`
(`main.cpp:2182`, also used when the theme changes). Each line has been changed by upstream once or twice. Settings
and the other menus use `be` directly.

**Wi-Fi's own check**: `Library::online()` reads `/sys/class/net/wlan0/operstate` at most every 2 s, when asked by a
drawing (`library.cpp:1059-1065`).

**The fork**: Tailscale (0001) runs `tailscaled --tun=tailscale0` while switched on
(`kiwami/rootfs/usr/libexec/kiwami/tailscale`). The fork's UI code is in `ui/src/kiwami/`; `ra-icon.cpp` draws
icons from pixel maps in code. The fork's headless UI tests are `kiwami/tests/*-ui.sh` (e.g. `tailscale-ui.sh`),
run by `kiwami/tests/ci.sh`. Nothing of the fork touches the status bar or `ui/rnds/`.

**Upstream (`main`)** has nothing about Tailscale or VPNs.

## What has to change

- `ui/src/kiwami/statusbar.{h,cpp}` (new, the fork's), a `rnds::Backend` that forwards every call to the real one.
  On the top screen:
  - it recognises the Wi-Fi sprite's draw by its place and size (or its texture);
  - while `tailscale0` has an address, it draws Tailscale's icon after that draw, at its y and with its tint;
  - it shifts every draw and fill after it in the left group right by the icon's width and a gap, up to the clock;
  - it starts again at each `beginScreen(0)`;
  - without the tailnet, or when no Wi-Fi draw is found, it changes nothing, so frames stay bit-identical to
    upstream's.
- The address check: an `ioctl(SIOCGIFADDR)` on `tailscale0`, at most every 2 s, only while the top screen is drawn.
  The interface name can be overridden for the tests.
- The icon: two pixel maps (about 16 px at 1x and 26 px at 1.6x), Tailscale's 3x3 dots with the T solid and the rest
  faded, uploaded once and tinted like the Wi-Fi icon.
- Hooks in upstream's files: two `kiwami:` lines in `ui/src/main.cpp` (`:857` and `:2182`) handing `RndsMenu` the
  fork's backend instead of `&be`. They can't be avoided: the menu's backend is chosen only there. The fork's backend
  object has to outlive menus remade on a theme change; how is for the plan.
- `kiwami/tests/`: a headless test of the bar with and without the tailnet (*Testing*).
- No settings, no packages, no rootfs files, no dependencies.

## Options

**A. A pass-through backend in front of the engine** (*chosen*). The icon really sits next to Wi-Fi, follows the
rise and the dimming, and lives in the fork's files with two one-line hooks. It costs one virtual call per draw. It
depends on the engine's draw order in `drawStatus`. A ROCKNIXDS re-import that changes the bar could misplace the
icon or the shift. The headless test catches that, and the backend draws nothing when it can't find the Wi-Fi draw.

**B. The host's existing overlay** (`main.cpp:2332`, one line). This is the simplest. But the icon can't sit next to
Wi-Fi (there's no room) and stays still while the bar rises. Not chosen.

**C. A hook in `drawStatus`** (about four changed lines in `ui/rnds/RndsUI.cpp`). This is the cleanest code, but it
breaks the fork's rule for `ui/rnds/`, and an upstream re-import could conflict or drop it. Not chosen.

## Risks

- **Performance and footprint**:
  - Frame time: one extra virtual call per draw (a few hundred per frame) and an ioctl at most every 2 s while the
    top screen is drawn; microseconds.
  - Idle: nothing. No thread or timer, and the check runs only when the menu draws anyway.
  - Memory: two icon textures of a few KB. Rootfs: a few KB of code. Boot: none.
  - When the icon appears or goes, the shifted part of the bar is redrawn once.
- **Conflicts with upstream**: `main.cpp` changes often (158 commits), but the two lines rarely (1-2 changes each).
  The real coupling is to the order and positions of the engine's status draws: unchanged since the import, but a
  re-import can change them without a textual conflict. Only the fork's test would show it.
- **Hardware and boards**: the same on `rgds`, `rgds-plus` and `universal`. Only the device can show:
  - that `tailscale0` really loses its address on logout and expiry;
  - the icon during an approval wait (oversight 3);
  - the dimming after a sleep on the RG DS Plus (the user's only device).
- **Room on the right** (oversight 6): a long stage name with a 12-hour clock may crowd the date; to be measured.

## Testing

- **Headless** (`kiwami/tests/`, a new `*-ui.sh` run by `ci.sh`), home and game list at 640x480 and 1024x768:
  - Without the tailnet: shots identical to upstream's, compared with a run without the fork's backend, or with
    `ui/tests/run.sh`'s.
  - With the tailnet (the interface overridden to one with an address):
    - the icon's pixels right after Wi-Fi;
    - the battery, the brand plate and (when shown) the version note moved by exactly the shift;
    - the date and clock unmoved;
    - Dark and Light; battery modes icon, text and none; charging; 12-hour clock;
    - Wi-Fi down (dimmed) in the fake fixture;
    - a shot in the middle of a rise (`press right; wait 100`), with the icon moved with Wi-Fi.
- **`make test`** must still pass unchanged (no tailnet there), as `ui/tests/run.sh` asserts on log lines and shots.
- **On the device** (RG DS Plus), with Tailscale logged in:
  - The icon after boot and after switching Tailscale on (by the next press or minute).
  - Gone after Tailscale is switched off, after a logout and with an expired key.
  - Dimmed with Wi-Fi off, and through a sleep and resume.
  - Dark and Light.

  To collect: photos of the bar, `/data/banana/logs/tailscale.log`, and `ip addr show tailscale0` over SSH in each
  state.

## Open questions

1. Which themes? *Answered*: Dark and Light only.
2. What counts as connected, and how does it look? *Answered*: logged in (an address on `tailscale0`), dimmed with
   Wi-Fi like the Wi-Fi icon, nothing in other states.
3. A setting for the icon? *Answered*: no; the Tailscale switch is the gate.
4. How to make room? *Answered*: the fork's pass-through backend (*Options*, A).
5. The look? *Answered*: Tailscale's dots, the T solid and the rest faded.
6. Detection? *Answered*: `tailscale0`'s address, with an approval wait possibly counting.
7. Timing? *Answered*: like Wi-Fi, at the next redraw.
