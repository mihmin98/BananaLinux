# 0043 Dynamic power scaling: plan

Issue: #43 · Branch: `k/0043-power-scaling` · Investigation: [investigation.md](investigation.md) · Status: approved

## Summary

With Settings > Other systems > `Power scaling` on (off by default), RetroArch and BananaFront games run on a
frame-driven clock governor instead of their system's fixed `banana-perf` profile. N64 keeps its profile, and the DS
keeps its own governors.

- **RetroArch** gets a governor of the fork's, in two patches to the fork's RetroArch master:
  - `0101` adds its files (`gfx/drivers_context/kiwami_clocks.{c,h}`), plain C89 with no RetroArch headers, so a host
    test can apply the patch to an empty folder and test it;
  - `0102` builds it and calls it from the KMS context (`drm_ctx.c`): at each swap, at each page flip, and at exit.

  The governor runs the CPU the way BananaFront's `--cpugov` does: the `performance` governor with a stepped
  `scaling_max_freq`, remembered per game. It works on the core's own frame period, not the panel's 60 Hz:
  - **CPU work** is the main thread's CPU time between swaps (its clock read by id, so threaded video's swaps too), so
    a wait (vsync, an ALSA write blocking for audio sync, the frame limiter) never counts as work;
  - **whether the game keeps its speed** is RetroArch's own smoothed iteration period (`pace_period_usec`) against the
    core's period.

  The GPU is governed by attribution: a frame that misses its refresh with its CPU work on time, or a game running slow
  with its CPU work fitting, raises the GPU's floor (devfreq `min_freq` under `simple_ondemand`).
- **Fast-forward** (RetroArch) follows `fastforward_ratio`:
  - a limit raises the CPU only until that speed is reached;
  - no limit (`0`) means the top clock;
  - on leaving, the clock from before comes back, and nothing learnt while fast-forwarding is kept.
- **BananaFront** gets `BANANAFRONT_CPUGOV=1`: its own full-range governor, with no patch. Its fast-forward stays as
  upstream has it.
- **The launch script** `kiwami/rootfs/usr/libexec/kiwami/clocks`, sourced by one `kiwami:` line near the top of
  `run-retroarch`, turns this on: the environment, the GPU on demand from its lowest floor, a fresh log, and the mode
  for the strip.
- **The strip** (0036's `Status on the game panel`) shows the mode and the CPU clock after Wi-Fi/Tailscale, read
  every 2 s:
  - `Auto 1104 MHz` while the governor runs;
  - `Balanced 816 MHz` (the system's profile name) otherwise.

Decided while planning:

- **No new hook in `settings.cpp`**: the row is added from the fork's existing Second screen hook (`settings.cpp:661`,
  inside the Other systems block), with section `Other systems`, and placed after Frontend by `Kiwami::place`'s
  `FOLLOWS`.
- **`runloop.c` isn't patched.** The swap reads what the governor needs from RetroArch's runloop and video state
  (`runloop_state_get_ptr()`, `video_state_get_ptr()`). Fast-forward's skipped frames never reach the swap, but its
  decision only needs the smoothed period and the limit, which the next presented frame reads.
- **Patch numbers `0101`, `0102`**: upstream's `buildroot/patches/retroarch/` uses 0001-0006, so the fork's own
  start where upstream's won't reach.
- **The strip's clocks are logged on a line of their own** (`panel clocks: Auto 1104 MHz`) at a game's start, after
  a sleep and when the mode changes, never on a clock change alone. 0036's `panel status:` lines and their tests
  stay as they are.
- **A crash's stale GPU floor** is cleared by the launch script at the next start (the GPU's `min_freq` to its
  lowest), so no second hook after `run-retroarch`'s `wait` is needed.

## Acceptance criteria

- **A1**: Given Settings > Other systems, then a row `Power scaling` (Off/On) follows `Frontend` and shows Off on a
  card that never set it. A on it writes `kiwami_power_scaling = on` to `settings.conf`.
- **A2**: Given the setting off (or missing), when a game starts through `run-retroarch`, then nothing of the fork's
  runs:
  - no `KIWAMI_CLOCKS` or `BANANAFRONT_CPUGOV` in the environment;
  - the GPU's governor and floor untouched;
  - no mode file, no log;
  - RetroArch touches no cpufreq or devfreq file;
  - BananaFront runs its default floor, as upstream's.
- **A3**: Given the setting on and a system other than N64, when a game starts through `run-retroarch` (directly,
  from `run-ps1` or from `run-pico8`'s fake-08), then:
  - `KIWAMI_CLOCKS=1`, `BANANAFRONT_CPUGOV=1`, `KIWAMI_CLOCKS_DATA=/data/banana/kiwami-clocks` and
    `KIWAMI_CLOCKS_LOG=/data/banana/logs/kiwami-clocks.log` are exported;
  - the GPU is on `simple_ondemand` with `min_freq` at its lowest;
  - the last log is kept as `kiwami-clocks.1.log`;
  - `auto` is written to `/run/banana/kiwami-clocks`.

  For N64, none of it.
- **A4**: Given RetroArch with `KIWAMI_CLOCKS=1` and cpufreq present, when the game's first frame is swapped, then the
  governor starts:
  - it puts `scaling_min_freq` at the lowest clock and `scaling_governor` to `performance`;
  - it starts `scaling_max_freq` one step above the game's remembered clock, or at the top with none;
  - it puts the GPU's floor at the remembered one, or the lowest;
  - it logs a start line (game, core, fps, refresh, starting clocks).
- **A5**: Given a running game, the CPU steps:
  - **up one step** when the slowest CPU work of the last second (its p99) passed 90% of the core's period;
  - **up at once** when the game ran slow (RetroArch's period over the core's by 3% for a second) with its CPU work
    over 80%, two steps for two in a second; the clock that failed is banned for 30 s, doubled each time it fails
    again;
  - **down one step** after 5 s with no step (up or down) and no late frame, when the p99 scaled to the lower clock
    fits 65% of the period.

  The period is `1/fps`, so a 50 Hz game paced by its audio isn't judged late.
- **A6**: Given a running game, when a flip comes later than the content's own spacing allows (more than
  `ceil(period/refresh) + 0.5` refreshes after the last) with that frame's CPU work under 80% of the period, or the
  game runs slow with its CPU work under 80%, then the GPU's floor goes up a step (never above its `max_freq`). It is
  held 30 s, and goes down a step after 30 s without one.
- **A7**: Given fast-forward on in RetroArch:
  - **with no limit** (`frame_limit_minimum_time` 0), the CPU is set to the top clock at once;
  - **with a limit**, the CPU steps up one a second while RetroArch's period is over the limit by 3%, and never steps
    down;
  - **when it ends**, the clock from before it comes back.

  Neither the clocks nor the samples of fast-forward reach the per-game memory or the normal-speed windows.
- **A8**: Given RetroArch's menu open, the game paused or in slow motion, the governor neither steps nor counts those
  frames. Any swap or flip gap over 100 ms (a sleep, a state, a load) is dropped and starts the windows afresh.
- **A9**: Given RetroArch quitting normally (F + Start, the panel's Quit, `SIGTERM`), then:
  - the CPU's governor, `scaling_min_freq` and `scaling_max_freq` are put back as they were found, and the GPU's
    `min_freq` goes to its lowest;
  - the game's clocks (CPU, the clock it held longest; GPU, the floor it held longest) are written to
    `/data/banana/kiwami-clocks/<core>/<game>.clocks`;
  - the log gets the steps (kept in memory, up to the last 256) and a summary: time at each CPU clock and GPU floor,
    late frames by cause, fast-forward time.
- **A10**: Given no cpufreq (QEMU, a PC) or an unwritable one, the governor logs `kiwami-clocks: off, no cpufreq`
  once, writes nothing and RetroArch runs on. No GPU devfreq: the CPU is governed alone.
- **A11**: Given the setting on and a BananaFront game, `bananafront.log` says `cpugov: on`.
- **A12**: Given `Status on the game panel` on:
  - with `/run/banana/kiwami-clocks` saying `auto`, written at most 5 s before the panel opened or after, the strip
    shows `Auto <MHz>` after Wi-Fi/Tailscale;
  - otherwise it shows the game's system's profile name and the MHz: `Powersave`, `Balanced`, `Steady`,
    `Performance`, an unknown one capitalised, none read as `Balanced`;
  - the MHz is `scaling_cur_freq / 1000`, read every 2 s and at once after a sleep, redrawn only when it changes; no
    reading draws the name alone;
  - a `panel clocks:` line is logged at the game's start, after a sleep and when the mode changes, and not on a clock
    change alone;
  - in the most crowded strip (a 12-hour clock, 100%, charging, the tailnet up, `Performance 1992 MHz`), the parts
    stay apart at both sizes.

  With the strip off nothing is read, and 0036's `panel status:` lines are unchanged.
- **A13**: Given the device (RG DS Plus), with the setting on:
  - a heavy `powersave` game holds its speed above 1416 MHz where it needs to;
  - a SNES game settles under the top clock;
  - a PlayStation and a PAL game hold their speed without climbing to the top;
  - a shader on the Plus keeps its speed, with the GPU's floor raised;
  - a sleep mid-game and RetroArch's menu leave no steps in the log.
- **A14**: Given the setting off, `make test`'s renders and the fork's existing UI tests are unchanged.

## Risks

- **Performance and footprint**:
  - Per swap, two `clock_gettime` calls and a few comparisons; per flip, one subtraction.
  - sysfs writes only on a step (at most every 300 ms), the card written only at exit.
  - The strip reads one sysfs file every 2 s.
  - No thread, no daemon, nothing in the menu or while idle; a few KB in RetroArch and banana-ui.
  - Battery: the point of the feature. M2-M5's time-at-clock summaries are the measure; M14 compares on and off.
- **A wrong step down shows as a stutter**: the thresholds are BananaFront's (90% up, 65% headroom down, 5 s, the
  escalating ban), tested on its stress ROMs. A5's unit tests pin them, and M2-M5 judge them on the device.
- **Misjudged lateness**: PAL and odd-rate cores (A5, `clocks-units` with a 50 Hz period, M5); run-ahead (more work,
  rightly); the menu and pause (A8, M11); a sleep (A8, M10). A threaded core's other threads don't count in the main
  thread's CPU time; the "game running slow" check covers them (A5).
- **Conflicts with upstream**:
  - `run-retroarch` changes very often (66 upstream commits since 2026-10-01). The hook is one sourced line between
    `setting()` and the Frontend comment; `clocks.sh` checks it's there and in that order.
  - No change to `settings.cpp`, `settings_gamepanel.cpp` or `main.cpp`.
  - The RetroArch patches must still apply at each bump of the fork's pin. `0102`'s three hunks are in `drm_ctx.c`'s
    flip handler, swap and init, and one line in `Makefile.common`'s `HAVE_KMS` list; a bump's rebuild shows
    `Applying 0101`/`0102` without fuzz (`kiwami/DEPENDENCIES.md`'s RetroArch row says so).
- **BananaFront's `BANANAFRONT_CPUGOV`** is read by upstream's BananaFront (`main.c:704`). A bump that renamed it would
  quietly fall back to its floor. M8 checks `cpugov: on` in its log; the final check re-reads `main.c` at the pinned
  commit.
- **A crash** leaves the CPU limits to `banana-perf ui` at the game's end (`main.cpp:3333`) and the GPU's floor to the
  next start's script (A3). Until then the menu runs with the raised floor: some battery, no harm.
- **Hardware**: the user has only an RG DS Plus. The RG DS (same SoC and CPU table, a 640x480 panel and less GPU load)
  isn't run on hardware; its strip is covered by the headless 640x480 shots.

## Steps

Each step is one commit, and leaves the tree building and `make test` passing.

### 1. The governor and its unit tests

- **Files**:
  - `kiwami/patches/retroarch/0101-gfx-drm-kiwami-clocks-a-frame-driven-clock-governor.patch` (new): adds
    `gfx/drivers_context/kiwami_clocks.h` and `kiwami_clocks.c` only, so it applies to an empty folder;
  - `kiwami/tests/clocks-units.c` (new);
  - `kiwami/tests/ci.sh` (changed);
  - `kiwami/DEPENDENCIES.md` (changed, RetroArch's row).
- **Change**: the governor, C89 as RetroArch's `AGENTS.md` asks (declarations first, `/* */` comments, no blocking, no
  logging but at start and exit). It depends only on the C library and is given everything else:

  ```c
  struct kiwami_clocks_frame
  {
     int64_t now_us;      /* CLOCK_MONOTONIC, at the swap */
     int64_t cpu_us;      /* the main thread's CPU time (CLOCK_THREAD_CPUTIME_ID), at the swap */
     double  fps;         /* the core's (av_info.timing.fps) */
     double  refresh_hz;  /* the panel's mode */
     int64_t pace_us;     /* RetroArch's smoothed iteration period (pace_period_usec) */
     int64_t ff_limit_us; /* frame_limit_minimum_time while fast-forwarding: 0 no limit */
     int     ff;          /* fast-forward on */
     int     hold;        /* the menu, paused, slow motion: nothing counted */
  };
  int      kiwami_clocks_start(const char *cpu_dir, const char *gpu_dir, const char *data_dir,
                               const char *log_path, const char *core, const char *game);
  void     kiwami_clocks_swap(const struct kiwami_clocks_frame *f);
  void     kiwami_clocks_flip(int64_t t_us);   /* a page flip's time (the event's sec/usec) */
  void     kiwami_clocks_stop(void);
  unsigned kiwami_clocks_mhz(void);            /* the CPU clock it set, 0 off */
  ```

  - **Start**:
    - reads `scaling_available_frequencies`, the governor, min and max (A10 if any is missing), and the GPU's
      `available_frequencies` and `min_freq` (none: CPU only);
    - reads the game's file (`<data_dir>/<core>/<game>.clocks`: `cpu <kHz>` and `gpu <Hz>` lines; a name's `/` and
      control characters replaced with `_`; a bad number ignored);
    - writes `scaling_min_freq` to the lowest clock, the governor to `performance`, `scaling_max_freq` to the start
      clock, and the GPU's `min_freq` to its start floor (A4);
    - keeps the start line for the log.
  - **Swap**:
    - work = `cpu_us` since the last swap; gap = `now_us` since it;
    - a gap over 100 ms or `hold` resets the windows (A8);
    - normal speed: the CPU rules of A5 (a one-second window of up to 64 works, its p99), the "running slow" check
      (`pace_us > 1.03 × period` over the window), the GPU rules of A6;
    - fast-forward: A7, with the clock from before kept and restored on leaving.
  - **Flip**: the gap since the last flip against `ceil(period × refresh) + 0.5` refreshes. A late flip with the last
    frame's work under 80% of the period is the GPU's (A6); over 80%, the CPU's rules have it.
  - **Stop**: A9 (limits back, the game's file, the log: the start line, the steps from a ring of 256, the summary).
    It runs once, whatever calls it.
  - Paths are given: the defaults are the glue's (step 2), and the tests pass their own folders.

  `DEPENDENCIES.md`'s RetroArch row adds that the fork carries its own patches `0101` (the governor's files) and,
  from step 2, `0102`, which a bump must still apply.
- **Tests**:
  - `kiwami/tests/clocks-units.c`, built by `ci.sh`:
    - applies `0101` with `patch -p1` into a temporary folder;
    - compiles the governor with the test using RetroArch's C89 flags (`-std=c89 -pedantic -Werror=pedantic
      -Wno-long-long -Werror=declaration-after-statement -Wall -Werror`, plus the feature macros the file defines);
    - runs it against a made-up cpufreq folder (408000 600000 816000 1104000 1416000 1608000 1800000 1992000) and GPU
      folder (200000000 … 800000000), fed synthetic frames.

    It asserts the files' contents after each case:
    - start: no memory gives max 1992000, min 408000, governor `performance`, GPU floor 200000000; a memory of
      `cpu 816000` starts at 1104000; a bad memory file is ignored;
    - a second of works at 95% of 16.7 ms steps up one; 5 s at 40% steps down one, and at 60% doesn't (65% scaled);
    - "running slow" with work at 85% steps up at once; two in a second step up two; the failed clock is banned 30 s,
      then 60 s;
    - a 50 Hz period (`fps` 50, refresh 60) with flips alternating 16.7/33.3 ms and work at 30%: no step, no GPU
      floor;
    - a late flip (33.3 ms at 60 Hz) with work at 30% raises the GPU floor a step, held 30 s, down after 30 s more;
    - fast-forward: with no limit, max at 1992000 at once and 816000 back after; with a limit, up one a second while
      `pace_us` is over it, never down, back after; the memory written at stop is the normal-speed clock;
    - `hold` and a 2 s gap: no step, windows reset;
    - stop: governor, min and max as found, GPU floor lowest, the game file with `cpu`/`gpu` lines, the log with its
      start line, a step line and the summary;
    - no cpufreq folder: `start` returns 0, the log says `off, no cpufreq`, nothing written.
  - `kiwami/tests/ci.sh .buildroot` passes.
- **Commit**: `Power scaling: a frame-driven clock governor for RetroArch, and its unit tests`

### 2. RetroArch calls the governor

- **Files**:
  - `kiwami/patches/retroarch/0102-gfx-drm_ctx-the-kiwami-clocks-governor-at-each-swap-and-flip.patch` (new);
  - `kiwami/DEPENDENCIES.md` (changed, the row's `0102`).
- **Change**: the patch, in RetroArch's style:
  - `Makefile.common`: `gfx/drivers_context/kiwami_clocks.o` next to `drm_ctx.o` in the `HAVE_KMS` list (`:1511`).
  - `drm_ctx.c`, the glue (static, at the top, with `#include "kiwami_clocks.h"`, `runloop.h`, `menu/menu_driver.h`
    under `HAVE_MENU`):
    - **once per process**, on the first swap with content loaded (`path_get(RARCH_PATH_BASENAME)` not empty), when
      `getenv("KIWAMI_CLOCKS")` is `1`: `kiwami_clocks_start()`;
      - the CPU folder `/sys/devices/system/cpu/cpufreq/policy0`, or `KIWAMI_CLOCKS_CPUFREQ`;
      - the GPU's first `/sys/class/devfreq/*.gpu`, or `KIWAMI_CLOCKS_DEVFREQ`;
      - the data folder and log from `KIWAMI_CLOCKS_DATA` and `KIWAMI_CLOCKS_LOG`;
      - the core `runloop_st->system.info.library_name`, the game the basename;
      - then `atexit(kiwami_clocks_stop)`. A context re-created mid-game doesn't start it again.
    - **in `gfx_ctx_drm_swap_buffers`** (`:652`), first: the frame's facts filled and `kiwami_clocks_swap()`:
      - `clock_gettime` `CLOCK_MONOTONIC` and the main thread's CPU clock (its id for thread `getpid()`, so a swap on
        threaded video's thread still reads the core's work; *Deviations*);
      - `video_state_get_ptr()->av_info.timing.fps`;
      - the refresh from `g_drm_mode` (`clock × 1000 / (htotal × vtotal)`);
      - `pace_period_usec`, `frame_limit_minimum_time`;
      - `RUNLOOP_FLAG_FASTMOTION`;
      - `hold`: `RUNLOOP_FLAG_PAUSED | RUNLOOP_FLAG_SLOWMOTION`, or the menu alive.
    - **in `drm_flip_handler`** (`:509`): `kiwami_clocks_flip(sec × 1000000 + usec)`.
  - Without `KIWAMI_CLOCKS=1`, the cost is one `getenv` at the first swap and an `if` per swap and flip.
- **Tests**:
  - `kiwami/tests/clocks-units.c` and `ci.sh` as in step 1.
  - `make retroarch-dirclean-rgds-plus`, then `make retroarch-rgds-plus` (long: RetroArch alone, a few minutes with
    ccache; warned before it's run; a `-rebuild` wouldn't apply the patches again, *Deviations*): the
    log shows `Applying 0101-…` and `Applying 0102-…` from `kiwami/patches/retroarch/` with no fuzz or offset warning,
    and RetroArch builds with `-Werror` off as today, with no new warning in `drm_ctx.c` or `kiwami_clocks.c`.
  - `kiwami/tests/retroarch-pin.sh` (in `ci.sh`): the patch step's folder is unchanged.
  - Optional: `tools/qemu-boot.sh` on an image is not required here (M15 covers QEMU's `no cpufreq` path, if wanted).
- **Commit**: `Power scaling: RetroArch runs the governor when KIWAMI_CLOCKS=1`

### 3. The setting: the launch script, run-retroarch's hook and the row

- **Files**:
  - `kiwami/rootfs/usr/libexec/kiwami/clocks` (new);
  - `system/rootfs/usr/libexec/banana/run-retroarch` (hook);
  - `ui/src/kiwami/kiwami.cpp` (changed);
  - `kiwami/tests/clocks.sh` (new);
  - `kiwami/tests/power-scaling-ui.sh` (new);
  - `kiwami/tests/ci.sh` (changed).
- **Change**:
  - **`clocks`**, sourced (not run), using the caller's `setting()`. When `setting kiwami_power_scaling` is `on` and
    `BANANA_SYSTEM` isn't `n64`:
    - exports `KIWAMI_CLOCKS=1 BANANAFRONT_CPUGOV=1 KIWAMI_CLOCKS_DATA=/data/banana/kiwami-clocks
      KIWAMI_CLOCKS_LOG=/data/banana/logs/kiwami-clocks.log`;
    - moves an existing log to `kiwami-clocks.1.log`;
    - for the first `${KIWAMI_DEVFREQ_ROOT:-/sys/class/devfreq}/*.gpu`: `simple_ondemand` into `governor`, and the
      lowest of `available_frequencies` into `min_freq`;
    - writes `auto` to `${KIWAMI_CLOCKS_MODE:-/run/banana/kiwami-clocks}`;
    - one stderr line, `kiwami: clocks: on (<system>)`, which banana-ui's log keeps.

    Otherwise it does nothing and prints nothing. Its variables are prefixed `kc_`, so `run-retroarch`'s aren't
    touched.
  - **The hook**, one line in `run-retroarch` after `setting()`'s closing `}` (`:11`) and before the Frontend comment
    (`:12`), so BananaFront's games get it too:

    ```sh
    [ -f /usr/libexec/kiwami/clocks ] && . /usr/libexec/kiwami/clocks	# kiwami: Power scaling (RetroArch's and BananaFront's clock governors)
    ```

  - **The row**, in `Kiwami::rows` when `group` is `Second screen` (the hook runs inside upstream's Other systems
    block). An `Item` with:
    - `section = "Other systems"`, icon `gamepad`, label `Power scaling`, key `kiwami_power_scaling`, file
      `settings.conf`, Off/On (`off`/`on`);
    - help: "Games in RetroArch and BananaFront run at the lowest CPU clock that keeps them at full speed, raised when
      frames are late and lowered when there's room; the GPU follows. Not the DS or N64.";
    - hints: "Each system's fixed clocks, as before." and "Clocks follow the game. The game panel's status shows them.".

    `FOLLOWS` gains `{ "Other systems", "Power scaling", "Frontend" }`. A comment says why it's added from the Second
    screen call (no hook on the page; this call is inside it). Its own log line: `banana-ui: kiwami: rows in Other
    systems: kiwami_power_scaling`, so `panel-status-ui.sh`'s anchored Second screen line is unchanged.
- **Tests**:
  - `kiwami/tests/clocks.sh` (in `ci.sh`), sourcing the script in a subshell with a stand-in `setting()` over a made-up
    `settings.conf`, and made-up devfreq root, mode file and log folder:
    - off and missing: no variable exported, devfreq files and mode file untouched, no output (A2);
    - on, `gba`:
      - the four variables;
      - `governor` = `simple_ondemand`, `min_freq` = the lowest of an unsorted list;
      - an old log moved to `.1`;
      - the mode file `auto`;
      - the stderr line (A3);
    - on, `n64`: as off (A3);
    - no devfreq: the rest still done;
    - `run-retroarch` has the hook line exactly once, after the line `setting() {`'s block and before `case
      $BANANA_SYSTEM in`.
  - `kiwami/tests/power-scaling-ui.sh` (in `ci.sh` when `ui/banana-ui` is built), headless at 640x480 and 1024x768:
    - the log has `rows in Other systems: kiwami_power_scaling`;
    - on Other systems' page the row after Frontend reads Off on a fresh card, and A writes
      `kiwami_power_scaling = on` (A1);
    - shots of the page off and on.
  - `make ui`, `make test`, `kiwami/tests/ci.sh .buildroot`, `sh -n` on the script (`ci.sh`'s loop over
    `kiwami/rootfs/*/libexec`).
- **Commit**: `Power scaling: the setting, which turns on RetroArch's and BananaFront's governors`

### 4. The strip: the mode and the CPU clock

- **Files**:
  - `ui/src/kiwami/panelstatus.h`, `panelstatus.cpp` (changed);
  - `ui/src/kiwami/panel-status.cpp` (changed);
  - `ui/src/kiwami/kiwami.h` (changed, if a member is needed);
  - `kiwami/tests/panelstatus-units.cpp`, `kiwami/tests/panel-status-ui.sh` (changed).
- **Change**:
  - **`PanelStatus`**:
    - `Sources` gains `std::function<PanelClocks()> clocks` (`struct PanelClocks { std::string mode; int mhz = -1; }`,
      with `==`), and `PanelState` gains `PanelClocks c`;
    - in `update()`: read at the first pass, after a sleep, and every 2 s of the loop's clock (`CLOCKS_MS = 2000`,
      beside `READ_MS`); a change makes `update()` return true;
    - `clocksLine()`: `panel clocks: <mode> <mhz> MHz` (or `<mode>` alone), pending at the first pass, after a sleep
      (`after a sleep: ` before it) and when `mode` changes; taken by the caller as `logLine()` is.
  - **`panel-status.cpp`**:
    - the source:
      - the mode is `Auto` when `${KIWAMI_CLOCKS_MODE:-/run/banana/kiwami-clocks}` says `auto` and its mtime is no
        more than 5 s before `panelOpened()`'s `time(nullptr)`;
      - otherwise the profile, from `kiwami::library->systemDefs()`'s entry whose `name` is the panel's `gSystem`,
        mapped `powersave`→`Powersave`, `balanced`→`Balanced`, `steady`→`Steady`, `performance`→`Performance`, empty
        → `Balanced` (banana-ui's default, `main.cpp:3137`), any other capitalised;
      - the MHz from `${KIWAMI_CPUFREQ_CUR:-/sys/devices/system/cpu/cpufreq/policy0/scaling_cur_freq}` / 1000, -1 if
        unreadable;
    - `panelUpdate()` logs `clocksLine()` too;
    - the drawing: the label at `end + 12` px after Wi-Fi/Tailscale, at y 438, size 15, in `p.muted`; the trace's
      `left ends at` includes it.
- **Tests**:
  - `kiwami/tests/panelstatus-units.cpp` (in `ci.sh`), with a stand-in `clocks`:
    - first pass `Auto 1104`, and the line `panel clocks: Auto 1104 MHz`;
    - at 1.9 s no read; at 2 s a read of 816, `update()` true and no line;
    - a mode change to `Balanced` gives a line;
    - after a sleep, a read at once and `after a sleep: panel clocks: …`;
    - `mhz` -1 gives `panel clocks: Balanced`;
    - 0036's `panel status:` lines exactly as before.
  - `kiwami/tests/panel-status-ui.sh`, with `KIWAMI_CPUFREQ_CUR` and `KIWAMI_CLOCKS_MODE` pointing at files in
    `$OUT`:
    - mode file fresh, 1104000: `panel clocks: Auto 1104 MHz` and the label's pixels present left of "Played";
    - no mode file, the GBA game (`balanced`): `panel clocks: Balanced 1104 MHz`;
    - a mode file touched 60 s earlier than the panel's opening: `Balanced` (stale);
    - after a `wait 2100` with 816000 written: the strip redrawn (a region diff on the left part), and no new
      `panel clocks:` line;
    - the crowded case with `Performance 1992 MHz` (an SNES ROM, `perf = performance`): the parts apart by `GAP` at
      both sizes;
    - the strip off: no `panel clocks:` line;
    - the existing `panel status:` expectations unchanged.
  - `make ui`, `make test`, `kiwami/tests/ci.sh .buildroot`.
- **Commit**: `Power scaling: the game panel's status shows the clocks' mode and the CPU clock`

### 5. Final check

- `make test`, `make -C ui`, `make -C system/bananad CFLAGS="-O2 -Werror"`, `kiwami/tests/ci.sh .buildroot`.
- The fork's tests in `kiwami/tests/`, among them `clocks-units`, `clocks.sh`, `power-scaling-ui.sh` and
  `panel-status-ui.sh`.
- The hooks: `git diff --stat main...HEAD` and `git grep -n 'kiwami:'` (the commands in `docs/FORK.md`) agree. This
  feature adds only `run-retroarch`'s line (and the patches, which are the fork's files).
- With the setting off, `make test`'s renders in `ui/tests/out/` match `main`'s.
- `kiwami/DEPENDENCIES.md`'s RetroArch row names `0101` and `0102`; no new dependency. BananaFront's
  `BANANAFRONT_CPUGOV` re-read at its pinned commit (`main.c`).
- An image for `rgds-plus` from the branch (`make rgds-plus`), for the manual tests.
- The manual tests below all passed, with their logs analysed and the outcomes in the table.
- Every acceptance criterion is met: each is listed by at least one automated or manual test, and those passed.

## Automated tests

| Test | Where | What it checks | Criteria |
|---|---|---|---|
| clocks-units | `kiwami/tests/clocks-units.c` (from `ci.sh`) | `0101` applies to an empty folder; the governor in C89 against made-up cpufreq/devfreq: start and memory, CPU up/down/ban, 50 Hz pacing, GPU attribution, fast-forward with and without a limit, holds and stalls, stop and its files, no cpufreq | A4, A5, A6, A7, A8, A9, A10 |
| retroarch build | `make retroarch-dirclean-rgds-plus`, `make retroarch-rgds-plus` (steps 2, 5) | both patches apply without fuzz; RetroArch builds | A4 |
| retroarch-pin | `kiwami/tests/retroarch-pin.sh` (from `ci.sh`) | the patch step's folder is still the fork's | A4 |
| clocks | `kiwami/tests/clocks.sh` (from `ci.sh`) | the launch script off, on, N64, no devfreq; the hook's place in `run-retroarch` | A2, A3 |
| power-scaling-ui | `kiwami/tests/power-scaling-ui.sh` (from `ci.sh`) | the row after Frontend, Off, A writes on, at both sizes | A1 |
| panelstatus-units | `kiwami/tests/panelstatus-units.cpp` (from `ci.sh`) | the clocks read every 2 s and after a sleep, their line's rules, 0036's lines unchanged | A12 |
| panel-status-ui | `kiwami/tests/panel-status-ui.sh` (from `ci.sh`) | `Auto`, the profile name, a stale mode file, the redraw at 2 s, the crowded strip, off | A12, A14 |
| make test | `ui/tests/run.sh` | upstream's renders, the setting off | A14 |

## Manual tests

On the RG DS Plus (the only device), with an image from the branch, `Status on the game panel` on and `Power
scaling` on (unless said). Logs are fetched over SSH/SFTP (Settings > Network > SSH) or from the card on a computer
(`banana/logs/` on the card is `/data/banana/logs/` on the device). `/run/banana/retroarch.log` is in RAM: fetch it
over SSH before quitting the next game or turning off. Each RetroArch game's `kiwami-clocks.log` is moved to
`kiwami-clocks.1.log` by the next one, so fetch after each test or run one game per test.

| # | Board | Steps | Expected | Logs to collect | Criteria | Outcome |
|---|---|---|---|---|---|---|
| M1 | rgds-plus | Power scaling **off**. Play a SNES game and a GBA game (Frontend Auto: BananaFront) 2 min each. | Play as today; the strip shows `Performance 1992 MHz` (SNES) and `Balanced <MHz>` (GBA, moving); no `kiwami-clocks.log` written; `bananafront.log` has the floor's `cpugov: clock floor`. | `banana-ui.log` (`panel clocks:` lines), `bananafront.log`, `ls /data/banana/logs` | A2, A12 | not run |
| M2 | rgds-plus | Frontend set to RetroArch. A heavy `powersave` game (a Mega Drive or NES stress scene, or a known heavy game) for 5 min. | No visible slowdown; the log shows steps above 1416 MHz where needed, few late frames after the first seconds. | `kiwami-clocks.log`, `retroarch.log` | A4, A5, A13 | not run |
| M3 | rgds-plus | A SNES game (Snes9x in RetroArch) for 10 min of play. | Smooth; the summary shows most time under 1992 MHz; the strip shows `Auto <MHz>` changing. | `kiwami-clocks.log`, `banana-ui.log` | A5, A12, A13 | not run |
| M4 | rgds-plus | A PlayStation game for 10 min (a 3D scene and a menu). | Smooth, sound without drop-outs; summary as M3. | `kiwami-clocks.log`, `retroarch.log` | A5, A13 | not run |
| M5 | rgds-plus | A PAL (50 Hz) game in RetroArch for 5 min. | Plays at its speed; the log's fps 50; the clock doesn't climb to the top for pacing. | `kiwami-clocks.log` | A5, A13 | not run |
| M6 | rgds-plus | In a RetroArch game: RetroArch's menu > Settings > Frame Throttle > Fast-Forward Rate 3.0x; fast-forward (R2 or the panel's Fast) 30 s; off; then Rate 0 (unlimited), 30 s; off. | At 3.0x the clock climbs only until ~300% (the log's fast-forward steps stop); unlimited: the top clock at once; each time, after it, the clock from before. | `kiwami-clocks.log` | A7 | not run |
| M7 | rgds-plus | Settings > Other systems > Picture & sound > Shader `lcd`; a GBA game with Frontend RetroArch, 5 min. | Smooth; the log shows GPU floor steps (cause GPU) if the shader needs them, and no lasting stutter. | `kiwami-clocks.log` | A6, A13 | not run |
| M8 | rgds-plus | Frontend Auto; a GBA game (BananaFront) 5 min. | `bananafront.log`: `cpugov: on`; the strip `Auto <MHz>`. | `bananafront.log`, `banana-ui.log` | A3, A11, A12 | not run |
| M9 | rgds-plus | An N64 game 2 min. | As before; the strip `Performance 1992 MHz`; no `kiwami: clocks: on` line in `banana-ui.log`. | `banana-ui.log` | A3 | not run |
| M10 | rgds-plus | In M3's game: close the lid 2 min, open, play 1 min. | Play goes on; the log has no step at the resume (the gap dropped); the strip's clock read again at once. | `kiwami-clocks.log`, `banana-ui.log` (`after a sleep: panel clocks:`) | A8, A12, A13 | not run |
| M11 | rgds-plus | In M3's game: RetroArch's menu (F) open for 30 s, then back. | No steps logged while it's open. | `kiwami-clocks.log` | A8, A13 | not run |
| M12 | rgds-plus | Start M3's game again. | The start line says the remembered clock (one step above); `/data/banana/kiwami-clocks/<core>/<game>.clocks` exists. | `kiwami-clocks.log`, the `.clocks` file | A4, A9 | not run |
| M13 | rgds-plus | Photos of the strip in M3 and M8. | The label readable, apart from Wi-Fi and "Played". | photos | A12 | not run |
| M14 | rgds-plus | Optional: the same SNES game 20 min with Power scaling off, then 20 min on, from a similar battery level, not charging. | Less battery used with it on. | the battery % at start and end of each; `kiwami-clocks.log` | A13 | not run |
| M15 | QEMU | Optional: `tools/qemu-boot.sh` on the image with Power scaling on (set on the card), a NES game. | RetroArch runs; the log says `off, no cpufreq`. | the card's `banana/logs/kiwami-clocks.log` | A10 | not run |

Notes from the runs (what the logs showed, what was fixed):

## Deviations

Where the implementation turned out different from this plan, why, and what changed (dated).

- **2026-11-17, step 1: two readings of the plan settled with the user.**
  - *The CPU's step down* waits 5 s after any step, up or down, as BananaFront's `cpugov.c` does, not only after a step
    up: successive steps down are 5 s apart (1992 to 816 MHz takes some 20 s on a game with no memory). A5 reworded.
  - *The GPU's line in the game's memory* is the floor held longest (at normal speed), as the CPU's is the clock held
    longest, not the floor in force at the exit. A9 reworded.
- **2026-11-17, step 2: the CPU work is read from the main thread's clock by its id, not `CLOCK_THREAD_CPUTIME_ID`.**
  With `video_threaded` on (the fork's `core-overrides` turns it on for Mesen and SameBoy) RetroArch's video thread
  calls the swap, where the calling thread's clock would measure the drawing and blame every slow frame on the GPU.
  The glue reads Linux's CPU clock of thread `getpid()` (the main thread), `(~pid << 3) | 6`, glibc's own encoding,
  from whichever thread swaps; without threaded video it's the same clock as before. Agreed with the user. Also, the
  build check is `retroarch-dirclean` then `retroarch`, since `retroarch-rebuild` doesn't apply new patches. The
  patch's third `drm_ctx.c` hunk is its includes, not an init: the governor starts at the first swap with a game.
  The first rebuild warned in `kiwami_clocks.c` (`-Wformat-truncation`: at Buildroot's `-O2 -D_FORTIFY_SOURCE=1` the
  game's `.clocks` path could take 903 bytes of a 900-byte buffer), so step 2's commit also enlarges that buffer and
  its temporary's in 0101, and `ci.sh` builds the governor with those flags so it catches this on the host.
- **2026-11-17, step 4: the crowded strip's `Performance` from a copy of `systems.conf`, not an SNES ROM.** The headless
  `gamepanel` verb always opens the panel as the Game Boy Advance (`main.cpp`, upstream's), so `panel-status-ui.sh`
  writes a copy of `systems.conf` in `$OUT` with the Game Boy Advance's `perf` set to `performance` and passes it with
  `--systems` for the crowded run alone, with `1992000` as the CPU's kHz. Agreed with the user. Also, the profile's
  name (`PanelStatus::profileName`) and the label's text (`PanelClocks::text`) are in `panelstatus.cpp`, so
  `panelstatus-units` checks them; `panel-status.cpp` keeps the reading of the files and `systems.conf`'s entry.

## Progress

A step is marked done in its own commit, so the commit column holds the subject (a commit can't carry its own hash).

| Step | Status | Commit |
|---|---|---|
| 1. The governor and its unit tests | done | Power scaling: a frame-driven clock governor for RetroArch, and its unit tests |
| 2. RetroArch calls the governor | done | Power scaling: RetroArch runs the governor when KIWAMI_CLOCKS=1 |
| 3. The setting: the launch script, run-retroarch's hook and the row | done | Power scaling: the setting, which turns on RetroArch's and BananaFront's governors |
| 4. The strip: the mode and the CPU clock | done | Power scaling: the game panel's status shows the clocks' mode and the CPU clock |
| 5. Final check | todo | |
