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
- **A5**: Given a running game, the CPU steps on its work per core frame (the main thread's CPU time since the last
  screen update, divided by the core frames it brought, carried to the next when it brought none), and:
  - **up one step** when the slowest CPU work of the last second (its p99) passed 90% of the core's period;
  - **up at once** when the game ran slow (RetroArch's period over the core's by 3% for a second) with its CPU work
    over 80%, two steps for two in a second; the clock that failed is banned for 30 s, doubled each time it fails
    again;
  - **down one step** after 5 s with no step (up or down) and no late frame, when the p99 scaled to the lower clock
    fits 65% of the period;
  - **back up at once** when a late flip, or the game running slow, comes within 5 s of a step down with its CPU work
    under 80%: the GPU's pass shares the period with the work, so that step down left it no room. The lower clock is
    banned as a failed one is (30 s, doubled each time), the GPU's floor isn't raised for it, and a late flip within
    300 ms of going back up (a frame already under way at the lower clock) counts for nothing.

  The period is `1/fps`, so a 50 Hz game paced by its audio isn't judged late.

  With RetroArch's threaded video (Mesen and SameBoy, from the fork's `core-overrides`) the main thread's time isn't
  the core's work, so the CPU steps by trial: down a step after 5 s with no step and no late frame, without the p99's
  test; never up for "near the limit"; a late flip, or the game running slow, within 5 s of a step down undoes it as
  above, and any other late frame is the GPU's when the GPU is busy (A6), else the CPU's (up at once, the clock banned).
  The log's start line says so, and its steps down say `trial`.
- **A6**: Given a running game, when a flip comes later than the content's own spacing allows with that frame's CPU
  work under 80% of the period, or the game runs slow with its CPU work under 80%, and no step down of the CPU came in
  the 5 s before (A5), then the GPU's floor goes up a step (never above its `max_freq`) when the GPU runs above its
  floor (its `cur_freq`: its own governor found it busy), or its clock can't be read. With the GPU at its floor the late
  frame is neither's: logged and counted, no clock changed. The spacing:
  - a core RetroArch runs at the panel's rate (within `audio_max_timing_skew` of it, or of a whole fraction of it,
    with vsync on, RetroArch's own test) is that many refreshes apart: one missed refresh is late (past 1.5 refreshes
    for a 60 fps core), and its period is the refreshes';
  - any other (a PAL game on a 60 Hz panel) is late past `ceil(period/refresh) + 0.5` refreshes.

  The floor is held 30 s, and goes down a step after the hold without a miss. A floor needed again within 10 s of a
  drop doubles the hold for the rest of the game (30 s, 60 s, 120 s ...).
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
  - the game's clocks are written to `/data/banana/kiwami-clocks/<core>/<game>.clocks`:
    - the CPU's is the highest clock it was stepped up to at normal speed, the one the game needed; with no step up,
      the clock it ended on, but not over the remembered clock it started a step above;
    - the GPU's is the floor it held longest;
  - the log gets the steps and the late flips (kept in memory, up to the last 512), each with its numbers, and a
    summary: time at each CPU clock and GPU floor, late frames by cause, step downs and how many were undone,
    fast-forward time, each second's slowest work at each CPU clock, and how many core frames each screen update
    brought.
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

### 5. The docs

- **Files**: `docs/FORK.md` (changed).
- **Change**: *Taking in upstream*'s list of what the fork shadows gains Power scaling: the `run-retroarch` hook's
  place and its use of `setting()` (`clocks.sh`), the row's `FOLLOWS` after Frontend from the Second screen hook
  (`power-scaling-ui.sh`), BananaFront's `BANANAFRONT_CPUGOV` to re-read at each bump of its pin, and RetroArch's
  `0101`/`0102`. `buildroot/package/bananafront/bananafront.mk` joins the `git diff` of files read by hand.
- **Tests**: none (documentation).
- **Commit**: `Power scaling: what a merge of upstream re-reads for it`

### 6. Fix: one missed refresh is late, and the GPU floor's hold grows

- **Files**:
  - `kiwami/patches/retroarch/0101-gfx-drm-kiwami-clocks-a-frame-driven-clock-governor.patch` (changed);
  - `kiwami/patches/retroarch/0102-gfx-drm_ctx-the-kiwami-clocks-governor-at-each-swap-and-flip.patch` (changed);
  - `kiwami/tests/clocks-units.c` (changed);
  - `kiwami/DEPENDENCIES.md` (changed, RetroArch's row: the two settings the glue now reads).
- **Change** (A6 reworded, *Deviations*):
  - the frame gains `skew`: the glue passes `audio_max_timing_skew` with `video_vsync` on, else 0;
  - the governor applies RetroArch's test (`retroarch.c`'s audio rate control): a core within the skew of the
    refresh, or of a whole fraction of it, runs at it, so its period is that many refreshes and a flip past half a
    refresh more is late; any other keeps the core's own period and `ceil` spacing;
  - a GPU floor raised within 10 s of a drop doubles the hold (`KC_GPU_SOON_US`);
  - the log's start line says `(the panel's rate)` when the core runs at it.
- **Tests**: `clocks-units` first fails on the new cases, then passes:
  - Snes9x's 60.0988 fps on 60.17 Hz: one 33.2 ms flip with the work at 30% raises the GPU floor, at 85% the CPU;
    with vsync off (skew 0) the same gap isn't late;
  - 62 fps on 60.17 Hz with RetroArch's period at the refresh's: no "running slow";
  - the hold: 30 s, then 60 s and 120 s when needed again within 10 s of each drop, unchanged when needed after 15 s;
  - the existing GPU case waits 11 s after its drop, so its 30 s holds stay what it checks.

  `make retroarch-dirclean-rgds-plus`, `make retroarch-rgds-plus`: both patches apply without fuzz, no new warning;
  `kiwami/tests/ci.sh .buildroot`.
- **Commit**: `Power scaling: one missed refresh is a late frame, and the GPU floor's hold grows when it's still needed`

### 7. Fix: a late frame right after a step down is the CPU's, and the memory keeps the clock the game needed

- **Files**:
  - `kiwami/patches/retroarch/0101-gfx-drm-kiwami-clocks-a-frame-driven-clock-governor.patch` (changed);
  - `kiwami/tests/clocks-units.c` (changed).
- **Change** (A5, A6 and A9 reworded, *Deviations*):
  - a late flip, or a second running slow, within 5 s of a step down (`KC_DOWN_US`) with the work under 80% goes back
    up a step at once (`late after a step down`), bans the lower clock as a late frame does, and doesn't raise the
    GPU's floor; one within 300 ms after that (`KC_LATE_GAP_US`) counts for nothing; any other is the GPU's, as before;
  - the CPU's memory is the highest clock stepped up to at normal speed; with none, the clock it ended on (from before
    fast-forward if it ended in it), never over the remembered one it started a step above;
  - the summary's last line adds `(<n> undone)` after the headroom count, and the GPU floors' times are separated by
    commas like the CPU's.
- **Tests**: `clocks-units` first fails on the new cases, then passes:
  - a step down to 1800, then a late flip at 40% within 5 s: back to 1992, the GPU's floor at 200, a late flip within
    300 ms after it ignored; 1800 banned 30 s, then 60 s after failing again; a late flip 5.5 s after a step down
    raises the GPU as before; the log has two `late after a step down` steps and `headroom 3 (2 undone)`;
  - a step down, then a second running slow at 40%: back up, the GPU's floor at 200;
  - the memory: 1104 stepped up to once, then 15 s at 408: `cpu 1104000`; 23 s on the way down from the top: the
    clock it ended on, `cpu 1104000`; a memory of 816 and 3 s with no step: `cpu 816000`;
  - the fast-forward and stop cases' memories change with the rule (816000 and 1800000), and the GPU floors' times
    line has its commas;
  - the GPU hold-doubling case starts from a memory of 408 MHz, so the CPU, at its lowest from 5 s on, takes no step
    down near its late flips.

  `make retroarch-dirclean-rgds-plus`, `make retroarch-rgds-plus`: both patches apply without fuzz, no new warning;
  `kiwami/tests/ci.sh .buildroot`.
- **Commit**: `Power scaling: a late frame right after a step down undoes it, and a game remembers the clock it needed`

### 8. The log's numbers, and the work per core frame

- **Files**:
  - `kiwami/patches/retroarch/0101-gfx-drm-kiwami-clocks-a-frame-driven-clock-governor.patch` (changed);
  - `kiwami/patches/retroarch/0102-gfx-drm_ctx-the-kiwami-clocks-governor-at-each-swap-and-flip.patch` (changed);
  - `kiwami/tests/clocks-units.c` (changed);
  - `kiwami/DEPENDENCIES.md` (changed, RetroArch's row: the field the glue now reads).
- **Change** (A5 and A9 reworded, *Deviations*):
  - the frame gains `frame_no`, the core frames so far (`video_st->frame_count`, 0 unknown), which the glue reads at
    each swap (from the video thread under threaded video: an aligned 64-bit read);
  - the governor's work is per core frame: the main thread's CPU time since the last update divided by the frames it
    brought; an update with none carries its work to the next; an unknown count is one frame;
  - the log's events, a ring of 512 (was 256 steps):
    - steps from the second's window carry its p99 as a share of the period: `CPU 1992 -> 1800 MHz (headroom, p99
      40%)`, as do `near the limit`, `running slow` and `late after a step down` from running slow;
    - every late flip at normal speed gets a line before what it causes: `late flip 33.3 ms, work 30%, CPU 1992 MHz,
      GPU floor 200 MHz at 300 MHz` (`at` the GPU's `cur_freq`, read on a late flip only; the GPU part only with
      devfreq);
  - the summary adds a line per CPU clock with each second's slowest work in tens of percent (`each second's slowest
    work at 1416 MHz: 40% 12 s, 50% 30 s`, the seconds in each ten) and one with the core frames per screen update (`core
    frames per screen update: none 12, one 10500, two or more 30`).
- **Tests**: `clocks-units` first fails on the new cases, then passes:
  - updates bringing 1, 1, 0, 2 frames with the work proportional (0.2 of the period on the empty one, 0.6 on the
    double): a step down after 5 s at 40% per frame, which the per-update measure (p99 60%) never takes; the summary's
    `none`, `one`, `two or more` counts;
  - the step lines' p99 (`headroom, p99 40%`, `near the limit, p99 95%`, `running slow, p99 85%`);
  - a late flip's line with and without the GPU's `cur_freq`, and with no devfreq;
  - the p99 lines' buckets at a clock;
  - the existing cases' step lines with their numbers.

  `make retroarch-dirclean-rgds-plus`, `make retroarch-rgds-plus`: both patches apply without fuzz, no new warning;
  `kiwami/tests/ci.sh .buildroot`.
- **Commit**: `Power scaling: the log gives the numbers behind each step, and the work is per core frame`

### 9. The GPU's floor only for a busy GPU, and the swap's spacing in the log

- **Files**:
  - `kiwami/patches/retroarch/0101-gfx-drm-kiwami-clocks-a-frame-driven-clock-governor.patch` (changed);
  - `kiwami/tests/clocks-units.c` (changed).
- **Change** (A6 reworded, *Deviations*):
  - a late flip, or a second running slow, that would go to the GPU (the work under 80%, no step down just before)
    reads the GPU's `cur_freq`: above the floor, or unreadable, the floor goes up as before; at the floor, it's
    counted as neither's, and the GPU's hold isn't renewed by it;
  - the summary's late frames add `neither <n> (late flip) <n> (running slow)`;
  - a late flip's line gets the spacing of its frame's swap from the one before: `late flip 33.3 ms, swap gap 16.7 ms,
    work 30%, ...`.
- **Tests**: `clocks-units` first fails on the new cases, then passes:
  - a late flip at 30% with the GPU's `cur_freq` at its floor: no floor raised, no CPU step, `neither 1 (late flip)`;
    with `cur_freq` above the floor, the floor goes up;
  - a second running slow at 30% with the GPU at its floor: no floor raised, `neither 0 (late flip) 1 (running
    slow)`;
  - the late flip lines' `swap gap`;
  - the GPU case that reads `cur_freq` takes it away after its first late flip, so the rest runs as before (unknown).

  `make retroarch-dirclean-rgds-plus`, `make retroarch-rgds-plus`: both patches apply without fuzz, no new warning;
  `clocks-units` and `retroarch-pin` as `ci.sh` runs them.
- **Commit**: `Power scaling: the GPU's floor goes up only when the GPU is busy, and a late flip's line has its swap's spacing`

### 10. With threaded video, the CPU steps down by trial

- **Files**:
  - `kiwami/patches/retroarch/0101-gfx-drm-kiwami-clocks-a-frame-driven-clock-governor.patch` (changed);
  - `kiwami/patches/retroarch/0102-gfx-drm_ctx-the-kiwami-clocks-governor-at-each-swap-and-flip.patch` (changed);
  - `kiwami/tests/clocks-units.c` (changed);
  - `kiwami/DEPENDENCIES.md` (changed, RetroArch's row: the field the glue now reads).
- **Change** (A5 reworded, *Deviations*):
  - the frame gains `threaded`, which the glue sets from `video_driver_thread_wrapper_active()` (RetroArch's own
    test of the wrapper, defined without threads too);
  - with it, the governor:
    - steps down after 5 s with no step and no late frame, not banned, without the p99's test (`trial`);
    - never steps up for "near the limit";
    - judges a late flip or a second running slow without the work: within 5 s of a step down it undoes it (as
      step 7), else the GPU's when it's busy (step 9), else the CPU's (`kc_cpu_late`: up at once, the clock banned);
  - the start line adds `, threaded video: the CPU steps down by trial`.
- **Tests**: `clocks-units` first fails on the new cases, then passes:
  - threaded, the work at 95% and nothing late: down a step every 5 s (`trial, p99 95%`), never up;
  - threaded, a late flip at 95% within 5 s of a trial step: undone, the clock banned, the GPU's floor unchanged;
  - threaded, a late flip 5.5 s after a step with the GPU at its floor: the CPU up a step (`late frame`);
  - threaded, a second running slow: the CPU up a step;
  - the start line.

  `make retroarch-dirclean-rgds-plus`, `make retroarch-rgds-plus`: both patches apply without fuzz, no new warning;
  `clocks-units` and `retroarch-pin` as `ci.sh` runs them.
- **Commit**: `Power scaling: with threaded video the CPU steps down by trial, since its time isn't the core's work`

### 11. Final check

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
| retroarch build | `make retroarch-dirclean-rgds-plus`, `make retroarch-rgds-plus` (steps 2, 6, 7, 8, 9, 10, 11) | both patches apply without fuzz; RetroArch builds | A4 |
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
| M1 | rgds-plus | Power scaling **off**. Play a SNES game and a GBA game (Frontend Auto: BananaFront) 2 min each. | Play as today; the strip shows `Performance 1992 MHz` (SNES) and `Balanced <MHz>` (GBA, moving); no `kiwami-clocks.log` written; `bananafront.log` has the floor's `cpugov: clock floor`. | `banana-ui.log` (`panel clocks:` lines), `bananafront.log`, `ls /data/banana/logs` | A2, A12 | skipped (2026-11-18, the user's decision: the feature works well enough on M2, M3, M7, M10-M12) |
| M2 | rgds-plus | Frontend set to RetroArch. A heavy `powersave` game (a Mega Drive or NES stress scene, or a known heavy game) for 5 min. | No visible slowdown; the log shows steps above 1416 MHz where needed, few late frames after the first seconds. | `kiwami-clocks.log`, `retroarch.log` | A4, A5, A13 | failed (2026-11-18, 0.5.3.4 from `c31b6991`): Mesen never stepped down from 1992 MHz in 184 s, nothing late; step 8 measures the work per core frame and logs the numbers. Failed again (2026-11-18, 0.5.3.4 from `b0c279cd`): one frame per update, but the main thread busy 90-100% of each period at 1992 MHz; step 10 steps down by trial with threaded video. Passed (2026-11-18, 0.5.3.4 from `56c2fe0e`): smooth, 209 of 233 s at 1608 MHz, 4 single late flips in the first 2 min |
| M3 | rgds-plus | A SNES game (Snes9x in RetroArch) for 10 min of play. | Smooth; the summary shows most time under 1992 MHz; the strip shows `Auto <MHz>` changing. | `kiwami-clocks.log`, `banana-ui.log` | A5, A12, A13 | failed (2026-11-17, 0.5.3.4 from `07a68068`): stutters at the three "running slow" steps; fixed in step 6. Failed again (2026-11-18, 0.5.3.4 from `bcb1581f`): hitches after each step down to 816 MHz, blamed on the GPU; fixed in step 7. Passed (2026-11-18, 0.5.3.4 from `13b76988`): smooth, 300 of 308 s at 1104 MHz; the GPU's floor raised for nothing, fixed in step 9; the strip not checked in `banana-ui.log` |
| M4 | rgds-plus | A PlayStation game for 10 min (a 3D scene and a menu). | Smooth, sound without drop-outs; summary as M3. | `kiwami-clocks.log`, `retroarch.log` | A5, A13 | skipped (2026-11-18, the user's decision: the feature works well enough on M2, M3, M7, M10-M12) |
| M5 | rgds-plus | A PAL (50 Hz) game in RetroArch for 5 min. | Plays at its speed; the log's fps 50; the clock doesn't climb to the top for pacing. | `kiwami-clocks.log` | A5, A13 | skipped (2026-11-18, the user's decision: the feature works well enough on M2, M3, M7, M10-M12) |
| M6 | rgds-plus | In a RetroArch game: RetroArch's menu > Settings > Frame Throttle > Fast-Forward Rate 3.0x; fast-forward (R2 or the panel's Fast) 30 s; off; then Rate 0 (unlimited), 30 s; off. | At 3.0x the clock climbs only until ~300% (the log's fast-forward steps stop); unlimited: the top clock at once; each time, after it, the clock from before. | `kiwami-clocks.log` | A7 | skipped (2026-11-18, the user's decision: the feature works well enough on M2, M3, M7, M10-M12) |
| M7 | rgds-plus | Settings > Other systems > Picture & sound > Shader `lcd`; a GBA game with Frontend RetroArch, 5 min. | Smooth; the log shows GPU floor steps (cause GPU) if the shader needs them, and no lasting stutter. | `kiwami-clocks.log` | A6, A13 | passed on the log (2026-11-18, 0.5.3.4 from `c31b6991`): the GPU's floor up to 400 MHz for the shader, its hold doubled; no lasting stutter |
| M8 | rgds-plus | Frontend Auto; a GBA game (BananaFront) 5 min. | `bananafront.log`: `cpugov: on`; the strip `Auto <MHz>`. | `bananafront.log`, `banana-ui.log` | A3, A11, A12 | skipped (2026-11-18, the user's decision: the feature works well enough on M2, M3, M7, M10-M12) |
| M9 | rgds-plus | An N64 game 2 min. | As before; the strip `Performance 1992 MHz`; no `kiwami: clocks: on` line in `banana-ui.log`. | `banana-ui.log` | A3 | skipped (2026-11-18, the user's decision: the feature works well enough on M2, M3, M7, M10-M12) |
| M10 | rgds-plus | In M3's game: close the lid 2 min, open, play 1 min. | Play goes on; the log has no step at the resume (the gap dropped); the strip's clock read again at once. | `kiwami-clocks.log`, `banana-ui.log` (`after a sleep: panel clocks:`) | A8, A12, A13 | passed in part (2026-11-18, 0.5.3.4 from `13b76988`, in M3's game): play went on, no step around the lid; `banana-ui.log` not checked |
| M11 | rgds-plus | In M3's game: RetroArch's menu (F) open for 30 s, then back. | No steps logged while it's open. | `kiwami-clocks.log` | A8, A13 | passed (2026-11-18, 0.5.3.4 from `c31b6991`, in M7's game): ~80 s of menu at the start, no step in it |
| M12 | rgds-plus | Start M3's game again. | The start line says the remembered clock (one step above); `/data/banana/kiwami-clocks/<core>/<game>.clocks` exists. | `kiwami-clocks.log`, the `.clocks` file | A4, A9 | passed (2026-11-18, 0.5.3.4 from `13b76988`): `CPU 1416 MHz (a step above this game's)`, `GPU floor 200 MHz (this game's)`, from the first run's `cpu 1104000 gpu 200000000` |
| M13 | rgds-plus | Photos of the strip in M3 and M8. | The label readable, apart from Wi-Fi and "Played". | photos | A12 | skipped (2026-11-18, the user's decision: the feature works well enough on M2, M3, M7, M10-M12) |
| M14 | rgds-plus | Optional: the same SNES game 20 min with Power scaling off, then 20 min on, from a similar battery level, not charging. | Less battery used with it on. | the battery % at start and end of each; `kiwami-clocks.log` | A13 | skipped (2026-11-18, the user's decision: the feature works well enough on M2, M3, M7, M10-M12) |
| M15 | QEMU | Optional: `tools/qemu-boot.sh` on the image with Power scaling on (set on the card), a NES game. | RetroArch runs; the log says `off, no cpufreq`. | the card's `banana/logs/kiwami-clocks.log` | A10 | skipped (2026-11-18, the user's decision: the feature works well enough on M2, M3, M7, M10-M12) |

Notes from the runs (what the logs showed, what was fixed):

- **M3, 2026-11-17** (0.5.3.4 built from `07a68068`; Super Castlevania IV in Snes9x, 2 min, not 10). The CPU as
  planned: from 1992 MHz with no memory, down a step every ~5 s to 816 MHz by 29.4 s, held there for 89 s, nothing
  near the limit, `cpu 816000` remembered. The GPU's floor went 200 -> 300 MHz "running slow" at 50.6, 84.1 and
  115.5 s, each time 3.4 s or less after a 30 s hold ended (80.7, 114.1 s), and the summary had `GPU 0 (late flip)
  3 (running slow)`. The user saw stutters at those moments. Two causes: Snes9x's 60.10 fps against the panel's
  60.17 Hz made `ceil(period/refresh)` 2, so no flip short of 41.5 ms was late and a single missed refresh never
  counted, leaving only "running slow", which needs a whole second's stutter; and the 30 s hold dropped a floor the
  game still needed. Fixed in step 6.
- **M3, 2026-11-18** (0.5.3.4 built from `bcb1581f`; Super Star Wars (USA) (Rev 1) in Snes9x, 6 min, not 10). Step 6
  worked: the start line says `(the panel's rate)`, the 6 GPU raises are all `late flip`, none `running slow`; the
  hold doubled at 347.8 s (2.3 s after a drop) and stayed 30 s at 71.6 s (14 s after one); 121 s at 1104 MHz with the
  floor at 200 MHz had no late frame. The user saw hitches at the late flips. Both CPU steps down to 816 MHz were
  followed by late flips blamed on the GPU: 25.3 s down, 27.6 s floor 200 -> 300, 30.4 s CPU back up (late frame);
  223.0 s down, 223.1 s floor 200 -> 300, 237.5 s -> 400, 255.5 s -> 600, 257.0 s CPU back up. The work fitted 65% at
  816 MHz, but with the GPU's pass in the same period the frames ran over, while the work alone stayed under 80%; the
  floor then spent 78 s at 400-600 MHz coming down. `cpu 1104000` remembered. The GPU floors' summary line had no
  commas. In the same session, Comix Zone (USA) in Genesis Plus GX ran 23 s only (not M2): down 1992 -> 1104 MHz,
  nothing late, but `cpu 1608000` remembered, one of four clocks held 5.1 s each. Fixed in step 7.
- **M7, M11 and M2, 2026-11-18** (0.5.3.4 built from `c31b6991`).
  - *M7 and M11*: Castlevania - Aria of Sorrow (USA) in mGBA with the `lcd` shader, 179 s, RetroArch's menu open at the
    start. The time at the clocks adds up to 98.7 s: the ~80 s left are the menu, with no step in them (M11), and the
    first step down came 4.3 s of play after it. Step 7 worked: three steps down (1608 -> 1416 at 91.4 s, 1416 -> 1104
    at 129.4 s and 161.5 s) were undone 2.6 s, 1.7 s and 1.8 s later as `late after a step down`, each clock tried
    again only after its ban (124.4 s, 161.5 s), and `headroom 6 (3 undone)`. The GPU's floor went 200 -> 300 -> 400
    MHz on late flips; dropped to 300 at 143.9 s it was late again at 144.1 s and went back to 400, its hold doubled,
    so the shader needs it (M7). The summary counts 10 late flips for the GPU against 3 raises: they came in bursts.
    Remembered `cpu 1608000 gpu 400000000`.
  - *M2*: Castlevania III - Dracula's Curse (All Fixes) in Mesen, 184 s: no step at all, 1992 MHz throughout, nothing
    late, nothing near the limit, so each second's slowest work stayed between 59% and 90% of the period at the top
    clock. Under its `powersave` profile (at most 1416 MHz) Mesen plays at full speed, so its work isn't that heavy.
    Suspected: Mesen draws on its own thread (the fork's `core-overrides`), whose swaps don't line up one to one with
    the core's frames, so a screen update now and then brings two frames' work, and a second's slowest (p99 of ~60) is
    that double. The log has no numbers to tell. Step 8 adds them and measures the work per core frame.
- **M3, M10, M11 and M12, 2026-11-18** (0.5.3.4 built from `13b76988`; Super Star Wars (USA) (Rev 1) in Snes9x, 408 s,
  then 31 s again).
  - *M3*: smooth, the user says. Started from the last run's memory at 1416 MHz, down to 1104 by 5.1 s and there for
    300 of the 308 s of play. Three steps down to 816 MHz (10.1 s, 41.3 s, 277.3 s, p99 44-48% at 1104) were undone
    0.8-1.4 s later (work 66-76% at 816), each retried only after its ban. Each second's slowest work at 1104 MHz:
    40-49% for 92 s, 50-59% for 179 s, 60-69% for 19 s. Every screen update brought one core frame.
  - *The GPU*: 8 late flips (one refresh missed, 33.2 ms, work 58-76%) went to the GPU, its floor up to 600 MHz for
    133 s in all. At every one the GPU ran at its floor (`GPU floor 200 MHz at 200 MHz`, ... `600 MHz at 600 MHz`):
    simple_ondemand never found it busy, and at 600 MHz (334.6 s) a flip was late all the same. The floor bought
    nothing. Fixed in step 9.
  - *M10, M11*: ~100 s of the 408 aren't counted (RetroArch's menu and the lid closed, the user says), with no step.
  - *M12*: the second start began at `1416 MHz (a step above this game's)` and `GPU floor 200 MHz (this game's)`.
  - Not run: M2 (Mesen) on this image.
- **M2 and M3, 2026-11-18** (0.5.3.4 built from `b0c279cd`).
  - *M3* (Super Star Wars, 174 s): the GPU's floor at 200 MHz throughout, no late flip but the two right after the
    steps down to 816 MHz (12.8 s, 167.1 s), both undone, their swap gaps 17.1 and 16.4 ms (the frame reached its swap
    on time and missed the flip); 166 s at 1104 MHz. Step 9 holds.
  - *M2* (Castlevania III in Mesen, 150 s): no step, 1992 MHz throughout, nothing late. `core frames per screen
    update: none 3, one 9051, two or more 4`: step 8's guess was wrong, Mesen's updates bring one frame. `each
    second's slowest work at 1992 MHz: 70% 1 s, 80% 8 s, 90% 105 s, 100% 34 s`: the main thread is busy nearly the
    whole period, though Mesen with threaded video plays at 60 fps under `powersave` (0006's notes: 75-80 fps
    fast-forwarded at 1416 MHz, about 55% of the period at 1992 MHz). RetroArch's threaded video code was read for a
    busy wait and none found (its waits sleep after a 3 us spin); what else the main thread does is unknown. With
    threaded video its time can't stand for the core's work. Fixed in step 10.
- **M2, 2026-11-18** (0.5.3.4 built from `56c2fe0e`; Castlevania III - Dracula's Curse (All Fixes) in Mesen, 233 s).
  Smooth, the user says. The start line ends `threaded video: the CPU steps down by trial`; trials every ~5 s took it
  1992 -> 1800 -> 1608 -> 1416 -> 1104 MHz by 20.3 s (p99 83-197%, not used). 1104 was late 0.2 s later and undone;
  1416 late again at 26.1 s (5.8 s after, so a plain late frame) and up to 1608. 1416 was tried again at 56.5 s and
  119.9 s, each time after its ban (30 s, then 60 s), each late within 2.6 s and undone, the next ban 120 s, past the
  end. 4 late flips in all (33.2-33.3 ms, swap gap 18.4-19.4 ms, work 110-115%, the GPU at its 200 MHz floor), none
  after 120.1 s; 209.4 of 233.2 s at 1608 MHz, instead of 1992 MHz throughout. `core frames per screen update: none
  89, one 13850, two or more 89`. Remembered `cpu 1608000 gpu 200000000`. Not run: SameBoy.

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
- **2026-11-17, final check: a docs step before it.** `docs/FORK.md`'s list of what a merge re-reads by hand didn't
  cover Power scaling's hook, row placement and BananaFront's variable; the final check changes no code, so a new step
  5 adds them and the final check becomes step 6. Agreed with the user. The renders are compared with `kiwami` before
  this feature (`2c650844`), not `main`: `kiwami` carries the Home server rework ahead of `main`. Agreed with the user.
- **2026-11-17, step 4: the crowded strip's `Performance` from a copy of `systems.conf`, not an SNES ROM.** The headless
  `gamepanel` verb always opens the panel as the Game Boy Advance (`main.cpp`, upstream's), so `panel-status-ui.sh`
  writes a copy of `systems.conf` in `$OUT` with the Game Boy Advance's `perf` set to `performance` and passes it with
  `--systems` for the crowded run alone, with `1992000` as the CPU's kHz. Agreed with the user. Also, the profile's
  name (`PanelStatus::profileName`) and the label's text (`PanelClocks::text`) are in `panelstatus.cpp`, so
  `panelstatus-units` checks them; `panel-status.cpp` keeps the reading of the files and `systems.conf`'s entry.
- **2026-11-17, M3: a fix step before the final check.** M3's log showed single missed refreshes never counted as late
  for a core a hair under the panel's rate (most NTSC cores against the RG DS Plus's 60.17 Hz: `ceil` gave 2 refreshes),
  and the GPU floor's fixed 30 s hold dropping a floor still needed, so the game stuttered every ~33 s. Agreed with
  the user: RetroArch's own test (`audio_max_timing_skew`, which the glue now passes) decides whether the core runs at
  the panel's rate, and a floor needed within 10 s of its drop doubles the hold. Step 6 fixes it (A6 reworded); the
  final check becomes step 7. Writing the tests also showed that a core a little over the panel's rate was judged
  "running slow" against its own period every second; the same change fixes it.

- **2026-11-18, M3 again: a second fix step before the final check.** M3's second log showed a CPU step down to a
  clock where the work still fitted 65% but the GPU's pass no longer did, each late flip blamed on the GPU (its floor up
  to 600 MHz) until a late frame put the CPU back. Agreed with the user: a late frame within 5 s of a step down with
  the work under 80% undoes that step and bans the clock (A5, A6 reworded). The same log's short Comix Zone session
  remembered an arbitrary clock of its descent; agreed with the user, the CPU's memory becomes the highest clock the
  game was stepped up to, or the one it ended on (A9 reworded). Step 7 fixes both; the final check becomes step 8. The
  GPU hold-doubling unit test now starts with the CPU at its lowest clock, since under the new rule its late flips
  fell within 5 s of the CPU's background steps down.

- **2026-11-18, M2 and M7: a third fix step before the final check.** M2's Mesen never stepped down, and the log can't
  say why: it gives the steps, not the work behind them. Agreed with the user: the log gets the numbers (each step's
  p99, each late flip with its spacing, work and clocks, each second's slowest work by clock, the core frames per
  screen update), and the governor works on the work per core frame (the glue passes RetroArch's `frame_count`), which
  is the same as before with one frame per update. Step 8 does both (A5 and A9 reworded); the final check becomes
  step 9.
- **2026-11-18, M3: a fourth fix step before the final check.** Step 8's numbers showed the GPU at its floor at every
  late flip blamed on it, and the floor's raises to 600 MHz not stopping them. Agreed with the user: the floor goes up
  only when the GPU runs above it (devfreq's `cur_freq`, read on a late frame), else the late frame is logged and
  counted as neither's and no clock changes; each late flip's line also gets its swap's spacing, to show where the rest
  of the frame went. Step 9 does it (A6 reworded); the final check becomes step 10.
- **2026-11-18, M2: a fifth fix step before the final check.** Mesen's main thread reads 90-100% busy at 1992 MHz
  while the game plays at full speed at 1416 MHz under upstream's profile, so with threaded video the governor can't
  judge the CPU by its work, and Power scaling held Mesen at the top clock (more power than with it off). Agreed with
  the user: with threaded video the CPU steps down by trial, undone by a late frame (A5 reworded). The glue passes
  whether threaded video is on. Step 10 does it; the final check becomes step 11.
- **2026-11-18, step 11: the remaining manual tests skipped.** After M2 passed on `56c2fe0e`, the user chose to skip
  M1, M4, M5, M6, M8, M9, M13, M14 and M15, and the `banana-ui.log` checks left in M3 (the strip's `Auto <MHz>`) and
  M10 (`after a sleep: panel clocks:`), and to go to the merge. What that leaves unchecked on the device: the setting
  off (A2, covered by `clocks.sh`), a PlayStation and a PAL game (A13), fast-forward (A7, covered by `clocks-units`),
  BananaFront's `cpugov: on` (A11: only `clocks.sh`'s `BANANAFRONT_CPUGOV=1` and BananaFront's `main.c` at
  `e6b8e47` reading it), N64 left alone (A3, covered by `clocks.sh`), the strip on the device (A12, covered by
  `panelstatus-units` and `panel-status-ui`) and the battery saved. The final check's "manual tests all passed" is
  read as: those run passed, the rest skipped.
- **2026-11-18, step 11: the final check's renders.** `kiwami` (`2c650844`, the branch's base) was unpacked with
  `git archive` into `/tmp` and its `ui/tests/run.sh` run there; its `make test` stopped at `banana-bshare`, which no
  longer builds against the host's OpenSSL (`X509_get_subject_name` returns a `const X509_NAME*`), in a file this
  feature doesn't touch (the branch's copy was built before the host's update). Of the 875 shots, 738 are the same
  and 137 differ: the clock (status bars, `st-tz`, `states-sel`), `files-cards`' free space (`/tmp` against the
  repository's disk), Bshare's random PIN, animations caught at another frame (`settings-page-bot`, `bt-audio-bot`,
  `ndsoverlay`), PNGs with the same pixels; and Other systems' counter, `02 / 09` for `02 / 08` in `ux-ra-row` and
  `ux-ra-back` at both sizes: the `Power scaling` row A1 adds, shown with the setting off. A14 is read as: with the
  setting off nothing else on screen changes.

## Progress

A step is marked done in its own commit, so the commit column holds the subject (a commit can't carry its own hash).

| Step | Status | Commit |
|---|---|---|
| 1. The governor and its unit tests | done | Power scaling: a frame-driven clock governor for RetroArch, and its unit tests |
| 2. RetroArch calls the governor | done | Power scaling: RetroArch runs the governor when KIWAMI_CLOCKS=1 |
| 3. The setting: the launch script, run-retroarch's hook and the row | done | Power scaling: the setting, which turns on RetroArch's and BananaFront's governors |
| 4. The strip: the mode and the CPU clock | done | Power scaling: the game panel's status shows the clocks' mode and the CPU clock |
| 5. The docs | done | Power scaling: what a merge of upstream re-reads for it |
| 6. Fix: one missed refresh is late, and the GPU floor's hold grows | done | Power scaling: one missed refresh is a late frame, and the GPU floor's hold grows when it's still needed |
| 7. Fix: a late frame right after a step down is the CPU's, and the memory keeps the clock the game needed | done | Power scaling: a late frame right after a step down undoes it, and a game remembers the clock it needed |
| 8. The log's numbers, and the work per core frame | done | Power scaling: the log gives the numbers behind each step, and the work is per core frame |
| 9. The GPU's floor only for a busy GPU, and the swap's spacing in the log | done | Power scaling: the GPU's floor goes up only when the GPU is busy, and a late flip's line has its swap's spacing |
| 10. With threaded video, the CPU steps down by trial | done | Power scaling: with threaded video the CPU steps down by trial, since its time isn't the core's work |
| 11. Final check | done | Power scaling: the final check done |
