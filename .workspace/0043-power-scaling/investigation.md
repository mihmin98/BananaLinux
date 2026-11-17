# 0043 Dynamic power scaling: investigation

Issue: #43 · Branch: `k/0043-power-scaling` · Status: refined · Confidence: 93% (proceed)

## The request

The handheld should raise the CPU's performance when a game doesn't reach its target frame rate. The power scaling
should also show on the game panel's status strip (the fork's 0036, next to the time, battery and Wi-Fi). An open
question in the request: what fast-forward should do, whether it should go to the top clock or stop raising once
RetroArch's fast-forward limit (e.g. 300%) is reached.

Decided with the user (question round 1):

- **Goal**: both smoothness and battery. A frame-driven governor gives the lowest clock that keeps the game at full
  speed: up a step when a frame is late or close to it, down after a while with none. It replaces the fixed profile
  for the games it covers, in both directions (above `powersave`'s 1416 MHz cap when a game needs it, below
  `performance`'s top clock when it doesn't).
- **Emulators**: RetroArch, through a patch to the fork's RetroArch master, and BananaFront, through its existing
  governor. The DS (BananaDS, DraStic/SuperDrastic) keeps its own governors; PortMaster, Game & Watch, the official
  PICO-8 and the other launchers are out of scope.
- **The strip**: the mode and the CPU clock, e.g. `Auto 1104 MHz`; with the setting off, the system's profile name.
- **Fast-forward**: the target becomes the fast-forward limit times the game's speed (RetroArch's
  `fastforward_ratio`), so the clock rises only until that speed is reached. With no limit (`0`, RetroArch's
  default), the top clock while it lasts. Afterwards the clock goes back to where it was before.

Decided with the user (question round 2):

- **BananaFront's fast-forward**: left as upstream has it (no patch). BananaFront has no fast-forward limit and its
  governor doesn't step while fast-forwarding, so fast-forward there runs at the clock the governor had reached.
- **N64**: kept on its `performance` profile; the governor is off for N64 games (*Oversights* 5).
- **The row**: on the Other systems page, right after the Frontend row, off by default.
- **The strip's clock**: read every 2 s, redrawn only when it changes. With the setting off it shows the profile's
  name and the current clock (`Balanced 816 MHz`).
- **Additions taken**: per-game memory of the clocks, a log for the manual tests, and the GPU governed too.
- **GPU**: by frame attribution, BananaFront's method. A frame whose CPU work ran late raises the CPU; a frame on time
  whose flip came a refresh late raises the GPU's floor (devfreq `min_freq` under `simple_ondemand`). The GPU starts
  on demand for every covered system, with or without a shader. The user asked whether on demand helps shaders: it
  doesn't by itself (*Current state*, BananaFront's `gpufloor.c`), which is why the floor rises on GPU-caused misses.

Out of scope: the DS, N64, the non-RetroArch launchers, a fast-forward limit or top clock in BananaFront, the GPU's
load from Panfrost's fdinfo (*Options* C).

The issue's text (#43):

```
Dynamic power scaling: the CPU clock follows the game's frame rate in RetroArch and BananaFront
```

```markdown
Each system runs on a fixed clock profile today (`perf` in `systems.conf`, set by `banana-perf` when a game starts). `powersave` caps the CPU at 1416 MHz, so a heavy 8-bit or 16-bit game can miss frames with headroom left unused, while `performance` keeps SNES, N64 and ports at the top clock even when they need far less. The DS emulators have their own frame-driven governors and BananaFront raises a clock floor on a late frame (up to 1416 MHz), but RetroArch, which runs N64, PlayStation and every system BananaFront doesn't, has nothing that looks at its frames.

With a new setting on, games in RetroArch and BananaFront get a frame-driven governor: the lowest CPU clock that keeps the game at its full speed. The clock goes up a step when a frame is late (or close to it), and back down after a while with none. In RetroArch this is a patch to the fork's RetroArch; BananaFront's own governor is turned on and its floor's cap lifted through its existing options.

Fast-forward follows its limit in RetroArch: the target becomes the fast-forward ratio times the game's speed (`fastforward_ratio`, e.g. 3.0 for 300%), so the clock goes up only until that speed is reached; with no limit (RetroArch's default, `0`) it runs at the top clock while it lasts, and afterwards it goes back to where it was. BananaFront has no fast-forward limit and keeps its own behaviour (the clock its governor had reached).

The GPU is governed by the same frames: a frame whose CPU work was late raises the CPU, a frame on time that still missed its refresh raises the GPU's clock floor (on demand underneath), so shaders keep their speed. N64 stays on its `performance` profile.

The game panel's status strip (Settings > Other systems > Second screen > Status on the game panel) shows the mode and the CPU clock next to the time, battery and Wi-Fi, e.g. `Auto 1104 MHz`, or the system's profile name when the setting is off.

The setting is off by default: off, the clocks are upstream's profiles as they are today.

Out of scope: the DS (BananaDS and DraStic keep their own governors), PortMaster and other launchers that can't be patched.
```

After round 2 the description's fast-forward paragraph was replaced by the two above (BananaFront's fast-forward
as it is, the GPU, N64), and the user updated the issue to match (*Open questions* 8).

## Oversights in the requirements

1. **"The target frame rate for the device" is the game's speed, not the panel's 60 Hz.** A PAL game runs at 50
   frames a second, the Game Boy at 59.73, arcade boards at 57-ish, and RetroArch paces a 60.00 Hz panel against
   each with its audio sync. RetroArch's own frame limit is computed from the core's `fps`
   (`runloop_set_frame_limit`, `runloop.c:6257-6271`). Judging lateness by the flips' spacing alone would read a 50 Hz
   game's alternating 16.7/33.3 ms flips as late frames.
   *Proposed*: the governor's period is the core's `1 / (fps × ratio)` (ratio 1 at normal speed). A frame is late
   when its work doesn't fit that period, as BananaFront judges against its pace. The plan includes a PAL game in the
   manual tests.

2. **Fast-forward in RetroArch skips frames that never reach the display.** With `fastforward_frameskip` (default on,
   `config.def.h:1762`), RetroArch decides before each `core_run()` whether the frame is shown
   (`video_driver_ff_frameskip_decide`, `runloop.c:10045`). A frame it drops never reaches the KMS context's swap
   (`drm_ctx.c:695-713`), so a governor in the swap alone wouldn't see fast-forward's work.
   *Proposed*: the fast-forward decision is taken where the iteration ends (`runloop.c:10152` onward, next to the
   frame limiter and its smoothed `pace_period_usec`). The iteration's period is compared with the limit
   (`frame_limit_minimum_time`); a limit of 0 means the top clock.

3. **After fast-forward, the clock it reached must not be learned.** The user asked for the clock to go back. A
   governor that kept stepping during fast-forward would also write fast-forward's clock as the game's memory.
   *Proposed*: on entering fast-forward the normal-speed clock is saved; on leaving, it's restored, and the
   fast-forward clocks are never stored. BananaFront skips its governor during fast-forward too
   (`bananafront/src/main.c:885`).

4. **The GPU's floor isn't reset by anything after a crash.** `banana-perf` puts back the CPU's governor, min and max
   and the GPU's governor at each game's end (`main.cpp:3333`, `banana-perf:23-29`), but never the GPU's `min_freq`.
   RetroArch killed or crashed with a raised floor would leave it up through the menu. BananaFront's `gpufloor`
   records the floor it finds as "the old one" and would keep a stale one.
   *Proposed*: the RetroArch governor puts the GPU's `min_freq` at the GPU's lowest at its start and at its exit (not
   "the old value"). `run-retroarch` waits for RetroArch (`run-retroarch:397-399`), so the fork's script can also
   reset it there after a crash. The plan decides whether that costs a second hook line.

5. **N64 is GPU-bound and threaded.** GLideN64 runs on its own thread with three swapchain images
   (`run-retroarch`, `.banana-defaults-n64-2`). A main-thread CPU measure can look idle while the renderer thread or
   the GPU is the limit, and lowering the CPU would slow the renderer thread.
   *Decided*: N64 keeps its `performance` profile; the hook doesn't turn the governor on for `BANANA_SYSTEM=n64`.

6. **BananaFront's two governor modes, and the 1416 MHz cap the issue mentions.** BananaFront's default "floor" mode
   (schedutil plus a raised `scaling_min_freq`) stops at 1416 MHz or the profile's max (`cpugov.c:122-126`, `:139`).
   Its `--cpugov` mode (`BANANAFRONT_CPUGOV=1`, `main.c:704`) is a full-range governor: the `performance` governor
   with a stepped `scaling_max_freq` from the lowest clock to the top, remembered per game in
   `/data/bananafront/cpugov/`. It isn't capped by the profile, since it writes `scaling_max_freq` itself.
   *Proposed*: with the setting on, BananaFront gets `BANANAFRONT_CPUGOV=1`. That is the "lowest clock that holds the
   game" the user chose, with no patch to BananaFront. Upstream's own `bananafront_cpugov` (settings.conf only, no
   Settings row) stays as it is; either one turns the mode on.

7. **BananaFront's fast-forward has no limit and doesn't raise the clock.** It runs as many frames as fit in 80% of a
   refresh, up to 16 (`main.c:732-736`, `:851-855`), and skips the governor while doing so (`:885`). In `--cpugov`
   mode that clock is the cap.
   *Decided*: left as upstream has it (round 2): no fork patch to BananaFront, which upstream bumped 8 times since
   2026-10-01.

8. **A shader with the GPU on demand.** BananaFront's notes record that `simple_ondemand` kept the Mali-G52 at its
   lowest clock (200 MHz) through a shader pass, so flips missed their refresh on the RG DS Plus's 1024x768
   (`bananafront/src/gpufloor.c:1-6`). A CPU governor alone would blame those misses on the CPU and raise it for
   nothing.
   *Decided*: frame attribution (round 2). RetroArch's patch raises the GPU's floor on a flip a refresh late with the
   frame's CPU work on time. BananaFront already does this with a shader (`gpufloor.c`), and without one it scales on
   the display plane.

9. **The strip's mode label has to know whether the governor is on for this game.** banana-ui opens the panel when it
   starts the game, before `run-retroarch` has decided (N64 excluded, BananaFront or RetroArch).
   *Proposed*: the fork's launch script writes the mode (`auto`, or nothing) to a file in `/run/banana/`, and the strip
   reads it with the clock every 2 s. Off or missing: the profile's name from `systems.conf`'s `perf` (`balanced` when
   empty, as `main.cpp:3137` does). The plan settles how a stale file from the last game is told apart.

10. **The strip's log line.** 0036's `PanelStatus` logs a line whenever a reading changes (`panelstatus.h:11`). A clock
    read every 2 s that changes often would flood `banana-ui.log`.
    *Proposed*: the clock and mode go in the line at a game's start and after a sleep, not on every change. The
    governor's own log has the steps.

11. **The row's place.** "Frontend" is a row on the Other systems page (`settings.cpp:643`), not a page of its own.
    The fork has no `Kiwami::rows` hook for that page; its existing hooks are in Second screen, Network,
    RetroAchievements and Home server (`settings.cpp:661`, `:713`, `:801`, `:978`).
    *Proposed*: the plan picks between a new one-line hook after the Frontend row and adding the row from an existing
    hook, placed with `Kiwami::place`'s `FOLLOWS` (`kiwami.cpp:183`).

12. **Sleep.** bananad SIGSTOPs the game and the system suspends (`docs/ARCHITECTURE.md:604`). bananad and `sleep.d/`
    don't touch cpufreq or devfreq (checked with grep). The policy limits stay as they were across a suspend. The
    first iteration after resume is seconds long, and RetroArch already drops samples over a quarter second from its
    pacing average (`runloop.c`, the comment before `runloop_pace_sample_usable`).
    *Proposed*: the governor ignores any iteration over 100 ms (a sleep, a state save, the menu), as BananaFront does
    (`main.c`, "a gap of 100 ms or more is a stop").

## Suggestions

- **Per-game memory** (*taken*, round 2): RetroArch's governor starts a game one step above the CPU clock and at the
  GPU floor it last settled on, as BananaFront does (`cpugov.c:99-110`). It is kept in a small file per game on the
  card (`/data/banana/kiwami-clocks/<game>`). With no file, it starts at the top and steps down.
- **A log for the manual tests** (*taken*): `/data/banana/logs/kiwami-clocks.log`. One line at the start (game,
  core, fps, ratio, starting clocks) and a summary at the end (time at each clock, late frames by cause, steps,
  fast-forward time). The step changes are kept in memory and written at exit, so nothing is written to the card
  while playing (RetroArch's rule: no logging in pacing paths, its `AGENTS.md`).
- **GPU on demand with frame attribution** (*taken*, round 2, *Oversights* 8).
- **The GPU's clock on the strip**: *not asked*; proposed left out (the log has it). The strip shows the CPU's.
- **Panfrost's GPU busy time** (fdinfo, with its `profiling` switch): *not taken* (round 2, *Options* C). Possible
  later, to step the GPU before a miss.
- **A fast-forward limit in BananaFront** (RetroArch's `fastforward_ratio` read there too): *not taken*. It would be a
  BananaFront patch (*Oversights* 7).

## Current state

**Profiles** (upstream's, `system/rootfs/usr/bin/banana-perf`): `ui`, `powersave` (schedutil capped at 1416 MHz,
GPU on demand), `balanced`, `steady` (schedutil above a 1104 MHz floor, PlayStation), `performance` (CPU and GPU
`performance`), `superdrastic`. banana-ui runs `banana-perf <perf>` before launching (`ui/src/main.cpp:3137`) and
`banana-perf ui` after (`:3333`). `systems.conf`'s `perf`: GB/GBC/NES/Mega Drive/ZX `powersave`, GBA/C64 `balanced`,
PlayStation `steady`, SNES/N64/G&W/ports `performance` (`system/rootfs/etc/banana/systems.conf:37-173`). The GPU's
devfreq is found as `/sys/class/devfreq/*.gpu`. The kernel (7.2) has `simple_ondemand` but not the `userspace`
devfreq governor.

**Launch chain**: every RetroArch game goes through `run-retroarch`: directly, from `run-ps1:63` (PlayStation) and
from `run-pico8:16` (fake-08). `run-retroarch` defines `setting()` (`:7-11`), then hands BananaFront's systems to
`run-bananafront` with `exec` (`:18-37`). After that comes the fork's existing hook, `core-overrides` (`:49`), which
only RetroArch reaches. RetroArch runs as a child that the script waits for (`:378`, `:397-399`); BananaFront is
`exec`'d (`run-bananafront:102`). `run-bananafront:83` turns on `--cpugov` from upstream's `bananafront_cpugov`.

**BananaFront** (upstream's package at `e6b8e47`, source in `output/*/build/bananafront-*/src/`):

- `cpugov.c` has two modes:
  - *Floor* (the default): schedutil kept, `scaling_min_freq` up a step on a late frame or a slowest frame over 80%,
    down after 10 s, never above 1416 MHz or the profile's max.
  - *cpugov* (`--cpugov` or `BANANAFRONT_CPUGOV=1`): the `performance` governor with `scaling_max_freq` over the whole
    table. Down a step after 5 s if the second's slowest frame would fit in 65% at the lower clock; up at 90%; up at
    once on a miss, with an escalating ban on the clock that failed. Remembered per game; put back on exit.
- `gpufloor.c`: with a shader only, devfreq `min_freq` up a step on a flip a refresh late with the frame on time,
  held 30 s, down after 30 s without one.
- Fast-forward: no limit, frames in 80% of a refresh up to 16, governor not fed (`main.c:732-736`, `:851-855`, `:885`).

**RetroArch** (the fork's master, `kiwami/local.mk`, patches in `kiwami/patches/retroarch/`, applied when
`BR2_PACKAGE_KIWAMI_RETROARCH_MASTER=y`, which `kiwami/configs/kiwami.fragment:18` sets):

- `runloop_iterate` (`runloop.c:9653`):
  - `core_run_time` is taken just before `core_run()` (`:10041`);
  - fast-forward's frameskip is decided there too (`:10045`);
  - at `end:` (`:10152`), the frame limit is re-set when fast-forwarding (`runloop_get_fastforward_ratio`), and a
    smoothed real period `pace_period_usec` is kept.
- The frame limit: `frame_limit_minimum_time = 1e6 / (fps × ratio)`, 0 for a ratio under 0.1, i.e. unlimited
  (`:6257-6271`). `DEFAULT_FASTFORWARD_RATIO` is `0.0` (`config.def.h:1758`), and Banana's template
  (`system/rootfs/etc/banana/retroarch.cfg`) doesn't set it, so fast-forward is unlimited unless the user sets a
  limit in RetroArch's menu.
- The KMS context: `gfx_ctx_drm_swap_buffers` waits for the previous flip, queues the next (`drm_ctx.c:695-713`; the
  fork's patch 0002 skips the wait in fast-forward); `drm_flip_handler` (`:509`) gets each flip's time.
  `video_max_swapchain_images = 2` (template), 3 for N64.
- RetroArch's `AGENTS.md`: C89, nothing may block the main thread, no logging in pacing or sync paths.

**The strip** (the fork's 0036, `ui/src/kiwami/panel-status.cpp`, `panelstatus.{h,cpp}`):

- Hooked into `settings_gamepanel.cpp` at `:380` (open), `:620` (update) and `:976` (drawn instead of the hint).
- Left side: Wi-Fi from x 24, Tailscale's mark after it. Centre: "Played h:mm" on 320. Right: time, date, battery,
  percentage from 616 leftwards. The left has room from about x 90 to 270 (640 px layout) for a 15 px
  `Auto 1104 MHz`.
- Readings every 10 s on `CLOCK_BOOTTIME` and after a sleep; the log line on start, change and after a sleep.
- Test stand-ins through the environment (`KIWAMI_PANEL_CLOCK`, `KIWAMI_WIFI_OPERSTATE`, `KIWAMI_TS_IFACE`), tested
  by `kiwami/tests/panel-status-ui.sh` and `panelstatus-units.cpp`.

**Upstream (`main`)** has no frame-driven governor for RetroArch, and nothing of the strip. `banana-perf`,
`systems.conf`, `run-bananafront` and BananaFront are the same on `main` and `kiwami`; `run-retroarch` differs only
by the `core-overrides` hook.

## What has to change

- **A RetroArch patch** (`kiwami/patches/retroarch/0006-…`), C89 in RetroArch's style:
  - the governor in a new file (e.g. `gfx/drivers_context/kiwami_clocks.c`, one line in `Makefile.common`), on only
    when `KIWAMI_CLOCKS=1` is in the environment;
  - a call at the end of `runloop_iterate` (frame work, fast-forward state and limit, the core's fps);
  - a call in the KMS context's flip handler (flip times, for the GPU's share);
  - sysfs writes only on a step; at start and exit the CPU's governor/limits and the GPU's floor put back; the per-game
    memory read at start and written at exit; the log written at exit.
- **The launch script** `kiwami/rootfs/usr/libexec/kiwami/clocks`, sourced by one `kiwami:` line in `run-retroarch`
  after `setting()` and before BananaFront's dispatch. With `kiwami_power_scaling = on` and a system other than N64,
  it:
  - exports `KIWAMI_CLOCKS=1` (RetroArch) and `BANANAFRONT_CPUGOV=1` (BananaFront);
  - puts the GPU on `simple_ondemand`;
  - writes the mode for the strip.

  Off, it does nothing.
- **The row**: `Power scaling` (`kiwami_power_scaling`, Off/On, default Off) after Other systems' Frontend row, in
  `ui/src/kiwami/kiwami.cpp` (*Oversights* 11).
- **The strip**: the mode and the CPU clock (`scaling_cur_freq`, every 2 s) on the left after Wi-Fi/Tailscale, in
  `panel-status.cpp` and `PanelStatus`, with stand-ins for the tests.
- **Tests** in `kiwami/tests/` (*Testing*); `panel-status-ui.sh`'s row list if the row goes there.
- **Docs**: `kiwami/DEPENDENCIES.md` needs no new entry (no new dependency; RetroArch's pin is unchanged). The
  feature's behaviour goes where the fork documents its RetroArch patches.
- No new package, library or daemon; nothing in `ui/rnds/`, `ui/assets/`, the ROCKNIX files, `banana-perf` or
  `systems.conf`.

## Options

**A. The governor inside each frontend: a RetroArch patch, BananaFront's existing `--cpugov` and `gpufloor`**
(*chosen*, rounds 1-2). Each frontend knows its frames, its fast-forward state and its limit exactly. It costs a
patch to carry on RetroArch master's bumps (RetroArch's `runloop.c` changes often), kept small with the logic in a
file of its own.

**B. One outside governor** (in banana-ui's game-panel loop, or a preloaded shim like `banana-leaseshim.so`) that
judges from CPU load or flip times. It would work for any launcher, but it can't see fast-forward or its limit, can't
tell a 50 Hz game's pacing from a late frame, and banana-ui's loop only runs with the game panel on. It adds polling
during games. Not chosen.

**C. A plus Panfrost's GPU busy time** from fdinfo. It is more precise for the GPU, but the `profiling` switch is
global and adds a cost to every GPU job, and it's more work. Not chosen (round 2); possible later.

## Risks

- **Performance and footprint**:
  - Per iteration, a few comparisons on clock readings RetroArch already takes; sysfs writes only on a step (at most
    every ~300 ms); the card written only at exit.
  - The strip reads one sysfs file every 2 s.
  - Nothing runs in the menu or while idle; no thread, no daemon; a few KB.
  - Battery should improve for `performance` systems and may rise for heavy `powersave` games, which is the point. The
    log's time-at-clock lines are the measure.
- **Conflicts with upstream**:
  - `run-retroarch` changes very often (66 upstream commits since 2026-10-01); the hook is one sourced line near its
    top.
  - `settings.cpp` (276 commits since 2026-10-01) may get one hook line.
  - The RetroArch patch is upstream-of-upstream: each bump of the fork's RetroArch pin must still apply it
    (`kiwami/tests/retroarch-pin.sh`'s stamps).
  - BananaFront's `BANANAFRONT_CPUGOV` is read from the environment by upstream's BananaFront. If a bump renamed it,
    BananaFront would quietly fall back to the floor; the plan's test checks its log line (`cpugov: on`).
- **Hardware and boards**: `rgds` and `rgds-plus` are both RK3566, with the same CPU table up to 1992 MHz. The Plus's
  1024x768 panel loads the GPU more (shaders, `gpufloor.c`'s notes). Only the device shows real clocks, frame misses
  and battery. The user has only an RG DS Plus, so the RG DS isn't tested on hardware.
- **Misjudged lateness**: PAL and odd-rate cores, run-ahead (more work per frame, which is right), the RetroArch menu
  or pause (the governor holds), audio-sync stalls. The plan's manual tests include a PAL game and a menu open
  mid-game.
- **A wrong step down shows as a stutter**: the governor's headroom (BananaFront's 65%) and the ban on a failed clock
  are taken over from BananaFront's tested values.
- **Fast-forward with no limit** runs the CPU at the top clock while held or toggled on (the game panel's Fast tile
  toggles it). That is what the user chose.

## Testing

- **Headless, the governor's logic**: the governor's file compiled on the host by a fork test, outside RetroArch (as
  BananaFront tests its own with `BANANAFRONT_CPUFREQ_DIR`), against a fake cpufreq and devfreq folder fed with
  synthetic frames:
  - steps up on late CPU work, the GPU floor up on a late flip with the work on time;
  - steps down after the quiet period;
  - a 50 Hz period;
  - fast-forward with ratio 3 (up to its target only) and 0 (top), and the clock restored after;
  - stalls over 100 ms ignored;
  - per-game memory read and written;
  - the limits put back at exit.
- **Headless, the launch script**: a shell test with stand-in settings and sysfs folders. Off: nothing set. On:
  `KIWAMI_CLOCKS=1` and `BANANAFRONT_CPUGOV=1` exported, the GPU on demand, the mode file written. N64: nothing.
- **Headless, the strip and the row**: `panel-status-ui.sh` extended with a stand-in `scaling_cur_freq` and mode file:
  - `Auto 1104 MHz`, and `Balanced 816 MHz` with the setting off;
  - shots at 640x480 and 1024x768 with a 12-hour clock and Tailscale on (the widest case);
  - the row after Frontend, Off by default (its `rows in` line).
- **The RetroArch patch applies and builds**: `make retroarch-rebuild-rgds-plus` (long; the plan warns before it).
  `kiwami/tests/ci.sh` covers the rest; `make test` unchanged (setting off).
- **QEMU**: `virt` has no cpufreq; a check that RetroArch with `KIWAMI_CLOCKS=1` logs "no cpufreq" and runs is
  optional.
- **On the device** (RG DS Plus), setting on:
  - a heavy `powersave` game (a Mega Drive or NES stress scene) going above 1416 MHz;
  - a SNES game settling well under the top clock;
  - PlayStation (`steady`), and a PAL game;
  - fast-forward with `fastforward_ratio` 3.0 and 0;
  - a shader (zfast-lcd) on the Plus;
  - a GBA game in BananaFront (its log's `cpugov: on`);
  - N64 unchanged;
  - a sleep mid-game;
  - the RetroArch menu opened mid-game;
  - the strip's label and clock;
  - setting off: today's clocks.

  To collect: `/data/banana/logs/kiwami-clocks.log`, `/data/banana/logs/bananafront.log`, `banana-ui.log`'s strip
  lines, `/run/banana/retroarch.log`, and photos of the strip.

## Checklist

| Area | Found |
|---|---|
| Upstream and the fork's rules | Hooks: one line in `run-retroarch`, maybe one in `settings.cpp`. The RetroArch patch in `kiwami/patches/retroarch/`. Nothing in `ui/rnds/`, `ui/assets/`, ROCKNIX files, `banana-perf`, `systems.conf`, BananaFront. Setting off by default = upstream's profiles. |
| Existing behaviour | `banana-perf` before/after each game (`main.cpp:3137`, `:3333`) stays the safety net; BananaFront's `bananafront_cpugov` and floor unchanged when off; the strip's log line (*Oversights* 10); `panel-status-ui.sh`'s row assertions. |
| Both boards, both panels | Same SoC and CPU table; the strip at 640x480 and 1024x768; the Plus's GPU load with shaders (*Oversights* 8). |
| Sleep and the lid | No clock changes around a suspend; the long first iteration ignored (*Oversights* 12); the strip reads again after a sleep (0036). |
| Boot, first boot, updates | No change to boot. A card without `kiwami_power_scaling` reads off (`setting()` falls back to the image's template, which lacks it). Per-game files on `/data` survive updates. |
| The card and the network | Per-game memory and log on `/data`; a missing or unwritable folder means no memory and no log, not a failure. No network. |
| Games and emulators | `run-retroarch`, `run-ps1`, `run-pico8` (fake-08) covered; N64, DS, PortMaster, G&W, official PICO-8 out. `game.pid` and display handover untouched. |
| Performance and footprint | *Risks*: nothing idle, writes only on steps and at exit. |
| Dependencies | None new. Relies on the fork's RetroArch master pin and upstream's BananaFront `BANANAFRONT_CPUGOV`. |
| Errors and edge cases | No cpufreq/devfreq (QEMU, a PC): the governor is off and logs it. Odd game names in the memory file's name (sanitised, as BananaFront's per-game files). RetroArch crash: *Oversights* 4. |
| Security | Root writes to sysfs, as `banana-perf` does. The per-game file is parsed as one number; a bad one is ignored. Nothing listens. |
| Testing | *Testing*. |

## Open questions

1. What is the governor for? *Decided*: smoothness and battery, the lowest clock that holds the game (round 1).
2. Which frontends? *Decided*: RetroArch (patch) and BananaFront (its `--cpugov`); the DS keeps its own (round 1).
3. What does the strip show? *Decided*: mode and CPU clock, the profile name when off (round 1); read every 2 s,
   with the name and clock when off (round 2).
4. Fast-forward? *Decided*: follow the limit, top clock with no limit, back afterwards, in RetroArch (round 1).
   BananaFront left as it is (round 2).
5. N64? *Decided*: kept on its profile (round 2).
6. Where's the row? *Decided*: after the Frontend row on Other systems (round 2).
7. The GPU? *Decided*: frame attribution, on demand with a floor raised on GPU-caused misses, for every covered
   system (round 2, after the user asked whether CPU and GPU could both be judged from usage and frame rate).
8. The issue's text said BananaFront's fast-forward goes to the top clock, and didn't mention the GPU or N64.
   *Decided*: the description updated to round 2's decisions (the text above).
9. The names. *Decided*: the row `Power scaling` (`kiwami_power_scaling`, Off/On, default Off), the log
   `/data/banana/logs/kiwami-clocks.log`, the per-game memory in `/data/banana/kiwami-clocks/`.
