/* Power scaling's governor for RetroArch (kiwami/patches/retroarch/0101-*: gfx/drivers_context/kiwami_clocks.c), checked
 * on its own against made-up cpufreq and devfreq folders, fed made-up frames: the start and the game's memory, the CPU's
 * steps up, down and bans, a late frame right after a step down undoing it, a 50 Hz game on a 60 Hz panel, a core
 * RetroArch runs at the panel's rate (Snes9x's 60.10 fps on the RG DS Plus's 60.17 Hz: one missed refresh is late), the
 * GPU's floor and its hold doubled when it's needed again soon after a drop, fast-forward with and without a limit, the
 * game's memory, the menu and stalls, the stop and its files, no cpufreq. Built and run by kiwami/tests/ci.sh, the patch
 * applied to an empty folder and the governor built with RetroArch's C89 flags:
 *   cc -std=c99 -Wall -D_GNU_SOURCE -I <folder>/gfx/drivers_context kiwami/tests/clocks-units.c kiwami_clocks.o
 *   ./clocks-units */
#include "kiwami_clocks.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int checks = 0, failed = 0;
#define CHECK(c) do { checks++; if (!(c)) { failed++; fprintf(stderr, "clocks-units: FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)
#define EQ(a, b) do { checks++; const char *a_ = (a), *b_ = (b); if (strcmp(a_, b_)) { failed++; fprintf(stderr, "clocks-units: FAIL %s:%d: %s is \"%s\", not \"%s\"\n", __FILE__, __LINE__, #a, a_, b_); } } while (0)

static char root[256], cpu[300], gpu[300], data[300], logf[300];

static const char *path(const char *dir, const char *name)
{
	static char p[4][600];
	static int n;
	n = (n + 1) % 4;
	snprintf(p[n], sizeof p[n], "%s/%s", dir, name);
	return p[n];
}

static void put(const char *dir, const char *name, const char *fmt, ...)
{
	va_list ap;
	FILE *f = fopen(path(dir, name), "w");
	if (!f) { fprintf(stderr, "clocks-units: can't write %s\n", path(dir, name)); exit(1); }
	va_start(ap, fmt);
	vfprintf(f, fmt, ap);
	va_end(ap);
	fclose(f);
}

// a file's first line, "" when there's none
static const char *get(const char *dir, const char *name)
{
	static char b[4][256];
	static int n;
	FILE *f;
	n = (n + 1) % 4;
	b[n][0] = 0;
	if ((f = fopen(path(dir, name), "r"))) {
		if (!fgets(b[n], sizeof b[n], f)) b[n][0] = 0;
		fclose(f);
	}
	b[n][strcspn(b[n], "\n")] = 0;
	return b[n];
}

// a whole file
static const char *slurp(const char *p)
{
	static char b[65536];
	size_t n = 0;
	FILE *f = fopen(p, "r");
	if (f) { n = fread(b, 1, sizeof b - 1, f); fclose(f); }
	b[n] = 0;
	return b;
}

static int lines_with(const char *text, const char *what)
{
	int n = 0;
	for (const char *s = strstr(text, what); s; s = strstr(s + 1, what)) n++;
	return n;
}

// fresh folders: the CPU as the menu leaves it (schedutil, 600-1416 MHz), the GPU's floor at its lowest, no memory, no log
static void setup(void)
{
	char cmd[1100];
	snprintf(cmd, sizeof cmd, "rm -rf '%s/cpu' '%s/gpu' '%s/data' '%s/log'", root, root, root, root);
	if (system(cmd)) exit(1);
	mkdir(cpu, 0755);
	mkdir(gpu, 0755);
	put(cpu, "scaling_available_frequencies", "408000 600000 816000 1104000 1416000 1608000 1800000 1992000 \n");
	put(cpu, "scaling_governor", "schedutil\n");
	put(cpu, "scaling_min_freq", "600000\n");
	put(cpu, "scaling_max_freq", "1416000\n");
	put(gpu, "available_frequencies", "200000000 300000000 400000000 600000000 700000000 800000000\n");
	put(gpu, "min_freq", "200000000\n");
	put(gpu, "max_freq", "800000000\n");
}

static void remember(const char *core, const char *game, const char *text)
{
	char d[600];
	mkdir(data, 0755);
	snprintf(d, sizeof d, "%s/%s", data, core);
	mkdir(d, 0755);
	char n[300];
	snprintf(n, sizeof n, "%s.clocks", game);
	put(d, n, "%s", text);
}

// the frames: a swap dt after the last with work as a share of the core's period, then its flip 0.5 ms later
static struct kiwami_clocks_frame F;

// the first swap, of a core at fps on a panel at hz, vsync on with RetroArch's default audio_max_timing_skew (5%)
static void begin_on(double fps, double hz)
{
	memset(&F, 0, sizeof F);
	F.now_us = 1000000000;
	F.fps = fps;
	F.refresh_hz = hz;
	F.skew = 0.05;
	F.pace_us = (int64_t)(1000000.0 / fps + 0.5);
	kiwami_clocks_swap(&F);
}

static void begin_at(double fps) { begin_on(fps, 60.0); }
static void begin(void) { begin_at(60.0); }

static void frame(long dt, double work)
{
	F.now_us += dt;
	F.cpu_us += (int64_t)(work * 1000000.0 / F.fps);
	kiwami_clocks_swap(&F);
	kiwami_clocks_flip(F.now_us + 500);
}

// seconds of frames at the core's own period
static void run(double seconds, double work)
{
	long dt = (long)(1000000.0 / F.fps + 0.5);
	int64_t end = F.now_us + (int64_t)(seconds * 1000000.0);
	while (F.now_us + dt <= end) frame(dt, work);
}

static int start(const char *core, const char *game)
{
	return kiwami_clocks_start(cpu, gpu, data, logf, core, game);
}

static unsigned long maxf(void) { return strtoul(get(cpu, "scaling_max_freq"), NULL, 10); }

int main(void)
{
	snprintf(root, sizeof root, "%s/clocks-units-XXXXXX", getenv("TMPDIR") ? getenv("TMPDIR") : "/tmp");
	if (!mkdtemp(root)) { perror("clocks-units: mkdtemp"); return 1; }
	snprintf(cpu, sizeof cpu, "%s/cpu", root);
	snprintf(gpu, sizeof gpu, "%s/gpu", root);
	snprintf(data, sizeof data, "%s/data", root);
	snprintf(logf, sizeof logf, "%s/log", root);

	// start with no memory: the top clock, the lowest floor, min at the lowest, "performance"
	setup();
	CHECK(start("Snes9x", "Game.sfc") == 1);
	EQ(get(cpu, "scaling_max_freq"), "1992000");
	EQ(get(cpu, "scaling_min_freq"), "408000");
	EQ(get(cpu, "scaling_governor"), "performance");
	EQ(get(gpu, "min_freq"), "200000000");
	CHECK(kiwami_clocks_mhz() == 1992);
	kiwami_clocks_stop();

	// a memory: a step above its CPU clock, its GPU floor
	setup();
	remember("Snes9x", "Game.sfc", "cpu 816000\ngpu 300000000\n");
	CHECK(start("Snes9x", "Game.sfc") == 1);
	EQ(get(cpu, "scaling_max_freq"), "1104000");
	EQ(get(gpu, "min_freq"), "300000000");
	CHECK(kiwami_clocks_mhz() == 1104);
	kiwami_clocks_stop();

	// a bad memory is ignored: a number with garbage, a clock not in the table, a word
	setup();
	remember("Snes9x", "Game.sfc", "cpu 816000x\ngpu 250000000\n");
	start("Snes9x", "Game.sfc");
	EQ(get(cpu, "scaling_max_freq"), "1992000");
	EQ(get(gpu, "min_freq"), "200000000");
	kiwami_clocks_stop();
	setup();
	remember("Snes9x", "Game.sfc", "cpu fast\n");
	start("Snes9x", "Game.sfc");
	EQ(get(cpu, "scaling_max_freq"), "1992000");
	kiwami_clocks_stop();

	// near the limit: a second of works at 95% of 16.7 ms steps up one, not two
	setup();
	remember("c", "g", "cpu 816000\n");
	start("c", "g");
	begin();
	run(0.9, 0.95);
	CHECK(maxf() == 1104000);
	run(0.2, 0.95);
	CHECK(maxf() == 1416000);
	kiwami_clocks_stop();

	// down: 5 s at 40% steps down one (not before), at 60% doesn't (60% x 1992/1800 is over 65%)
	setup();
	start("c", "g");
	begin();
	run(4.5, 0.40);
	CHECK(maxf() == 1992000);
	run(1.0, 0.40);
	CHECK(maxf() == 1800000);
	kiwami_clocks_stop();
	setup();
	start("c", "g");
	begin();
	run(8.0, 0.60);
	CHECK(maxf() == 1992000);
	kiwami_clocks_stop();

	// running slow (RetroArch's period 5% over the core's for a second) with the work at 85%: up at once; that clock is
	// banned 30 s, then 60 s when it fails again
	setup();
	remember("c", "g", "cpu 816000\n");
	start("c", "g");
	begin();
	F.pace_us = 17500;
	run(1.05, 0.85);
	CHECK(maxf() == 1416000);
	int64_t late = F.now_us;
	F.pace_us = 16667;
	run(29.0 - (F.now_us - late) / 1e6, 0.40);
	CHECK(maxf() == 1416000);
	run(2.0, 0.40);
	CHECK(maxf() == 1104000);
	F.pace_us = 17500;
	run(1.05, 0.85);
	CHECK(maxf() == 1416000);
	late = F.now_us;
	F.pace_us = 16667;
	run(59.0 - (F.now_us - late) / 1e6, 0.40);
	CHECK(maxf() == 1416000);
	run(2.0, 0.40);
	CHECK(maxf() == 1104000);
	kiwami_clocks_stop();
	CHECK(lines_with(slurp(logf), "CPU 1104 -> 1416 MHz (running slow)") == 2);

	// two late frames (flips a refresh late, the work at 85%) in a second: up one, then two
	setup();
	remember("c", "g", "cpu 816000\n");
	start("c", "g");
	begin();
	run(2.0, 0.30);
	frame(33333, 0.85);
	CHECK(maxf() == 1416000);
	run(0.4, 0.85);
	frame(33333, 0.85);
	CHECK(maxf() == 1800000);
	EQ(get(gpu, "min_freq"), "200000000");
	kiwami_clocks_stop();

	// a late flip within 5 s of a step down, with the work under 80% (the GPU's pass shares the period with it): the step
	// down's, not the GPU's. Back up at once, the GPU's floor left alone, and the lower clock banned 30 s, then 60 s; a late
	// flip within 300 ms of that (a frame already under way at the lower clock) counts for nothing; one over 5 s after the
	// step down is the GPU's, as before
	setup();
	start("c", "g");
	begin();
	run(5.5, 0.40);
	CHECK(maxf() == 1800000);
	frame(33333, 0.40);
	CHECK(maxf() == 1992000);
	EQ(get(gpu, "min_freq"), "200000000");
	late = F.now_us;
	frame(33333, 0.40);
	EQ(get(gpu, "min_freq"), "200000000");
	run(29.0 - (F.now_us - late) / 1e6, 0.40);
	CHECK(maxf() == 1992000);
	run(2.0, 0.40);
	CHECK(maxf() == 1800000);
	frame(33333, 0.40);
	CHECK(maxf() == 1992000);
	late = F.now_us;
	run(59.0 - (F.now_us - late) / 1e6, 0.40);
	CHECK(maxf() == 1992000);
	run(2.0, 0.40);
	CHECK(maxf() == 1800000);
	run(5.5, 0.60);			// (60% x 1800/1608 is over 65%: no step)
	CHECK(maxf() == 1800000);
	frame(33333, 0.60);
	CHECK(maxf() == 1800000);
	EQ(get(gpu, "min_freq"), "300000000");
	kiwami_clocks_stop();
	CHECK(lines_with(slurp(logf), "CPU 1800 -> 1992 MHz (late after a step down)\n") == 2);
	CHECK(strstr(slurp(logf), "headroom 3 (2 undone)\n") != NULL);
	// ... and the game running slow for a second within 5 s of a step down, the work under 80%: the step down's too
	setup();
	start("c", "g");
	begin();
	run(5.5, 0.40);
	CHECK(maxf() == 1800000);
	F.pace_us = 17500;
	run(2.1, 0.40);
	CHECK(maxf() == 1992000);
	EQ(get(gpu, "min_freq"), "200000000");
	kiwami_clocks_stop();

	// a 50 Hz game on a 60 Hz panel, paced by its sound: flips 16.7 and 33.3 ms apart, the work at 30% of 20 ms. No
	// step up and no GPU floor (and down only after 5 s)
	setup();
	remember("c", "pal", "cpu 816000\n");
	start("c", "pal");
	begin_at(50.0);
	for (int i = 0; i < 180; i++) frame(i % 2 ? 33333 : 16667, 0.30);
	CHECK(maxf() == 1104000);
	EQ(get(gpu, "min_freq"), "200000000");
	kiwami_clocks_stop();
	CHECK(lines_with(slurp(logf), " -> ") == 0);
	CHECK(lines_with(slurp(logf), "50.00 fps, 60.00 Hz, ") == 1);	// (its own rate)

	// a late flip with the work at 30%: the GPU's floor up a step, held 30 s, then down; two in 300 ms: up two, down one
	// each 30 s
	setup();
	start("c", "g");
	begin();
	run(1.0, 0.30);
	frame(33333, 0.30);
	EQ(get(gpu, "min_freq"), "300000000");
	int64_t miss = F.now_us;
	run(29.0 - (F.now_us - miss) / 1e6, 0.30);
	EQ(get(gpu, "min_freq"), "300000000");
	run(2.0, 0.30);
	EQ(get(gpu, "min_freq"), "200000000");
	run(11.0, 0.30);		// (over 10 s after the drop: the hold stays 30 s)
	frame(33333, 0.30);
	run(0.3, 0.30);
	frame(33333, 0.30);
	EQ(get(gpu, "min_freq"), "400000000");
	miss = F.now_us;
	run(29.0, 0.30);
	EQ(get(gpu, "min_freq"), "400000000");
	run(2.0, 0.30);
	EQ(get(gpu, "min_freq"), "300000000");
	run(28.0, 0.30);
	EQ(get(gpu, "min_freq"), "300000000");
	run(2.0, 0.30);
	EQ(get(gpu, "min_freq"), "200000000");
	// ... and the game running slow for a whole second's window with the work at 30%: the GPU's too
	F.pace_us = 17500;
	run(2.1, 0.30);
	EQ(get(gpu, "min_freq"), "300000000");
	F.pace_us = 16667;
	// never above its max_freq
	for (int i = 0; i < 8; i++) { run(0.3, 0.30); frame(33333, 0.30); }
	EQ(get(gpu, "min_freq"), "800000000");
	kiwami_clocks_stop();
	EQ(get(gpu, "min_freq"), "200000000");
	setup();
	put(gpu, "max_freq", "400000000\n");
	start("c", "g");
	begin();
	for (int i = 0; i < 8; i++) { run(0.3, 0.30); frame(33333, 0.30); }
	EQ(get(gpu, "min_freq"), "400000000");
	kiwami_clocks_stop();

	// a floor needed again within 10 s of its drop is held twice as long: 30 s, then 60 s, then 120 s; needed again later
	// than that, the hold stays as it is (the CPU at its lowest clock from 5 s, so no late flip comes right after a step
	// down)
	setup();
	remember("c", "g", "cpu 408000\n");
	start("c", "g");
	begin();
	run(1.0, 0.30);
	frame(33333, 0.30);
	EQ(get(gpu, "min_freq"), "300000000");
	miss = F.now_us;
	run(29.0 - (F.now_us - miss) / 1e6, 0.30);
	EQ(get(gpu, "min_freq"), "300000000");
	run(2.0, 0.30);
	EQ(get(gpu, "min_freq"), "200000000");
	run(5.0, 0.30);
	frame(33333, 0.30);
	EQ(get(gpu, "min_freq"), "300000000");
	miss = F.now_us;
	run(59.0 - (F.now_us - miss) / 1e6, 0.30);
	EQ(get(gpu, "min_freq"), "300000000");
	run(2.0, 0.30);
	EQ(get(gpu, "min_freq"), "200000000");
	run(3.0, 0.30);
	frame(33333, 0.30);
	miss = F.now_us;
	run(119.0 - (F.now_us - miss) / 1e6, 0.30);
	EQ(get(gpu, "min_freq"), "300000000");
	run(2.0, 0.30);
	EQ(get(gpu, "min_freq"), "200000000");
	run(15.0, 0.30);
	frame(33333, 0.30);
	miss = F.now_us;
	run(119.0 - (F.now_us - miss) / 1e6, 0.30);
	EQ(get(gpu, "min_freq"), "300000000");
	run(2.0, 0.30);
	EQ(get(gpu, "min_freq"), "200000000");
	kiwami_clocks_stop();

	// a core a hair under the panel's rate, which RetroArch runs at the panel's (within audio_max_timing_skew): Snes9x's
	// 60.10 fps on the RG DS Plus's 60.17 Hz. One missed refresh (a flip 33.2 ms after the last) is late: the GPU's with the
	// work at 30%, the CPU's at 85%. The start line says it's run at the panel's rate
	setup();
	start("c", "snes");
	begin_on(60.0988, 60.17);
	run(1.0, 0.30);
	frame(33239, 0.30);
	EQ(get(gpu, "min_freq"), "300000000");
	CHECK(maxf() == 1992000);
	kiwami_clocks_stop();
	CHECK(strstr(slurp(logf), "60.10 fps, 60.17 Hz (the panel's rate), ") != NULL);
	setup();
	remember("c", "snes", "cpu 816000\n");
	start("c", "snes");
	begin_on(60.0988, 60.17);
	run(1.0, 0.30);
	frame(33239, 0.85);
	CHECK(maxf() == 1416000);
	EQ(get(gpu, "min_freq"), "200000000");
	kiwami_clocks_stop();
	// ... with vsync off (no skew given) the core keeps its own rate: the same gap isn't late
	setup();
	start("c", "snes");
	begin_on(60.0988, 60.17);
	F.skew = 0;
	run(1.0, 0.30);
	frame(33239, 0.30);
	EQ(get(gpu, "min_freq"), "200000000");
	kiwami_clocks_stop();
	CHECK(strstr(slurp(logf), "(the panel's rate)") == NULL);
	// a core a little over the panel's rate (62 fps on 60.17 Hz), run at the panel's: RetroArch's period is the
	// refresh's, which isn't running slow
	setup();
	start("c", "fast");
	begin_on(62.0, 60.17);
	F.pace_us = 16620;
	run(3.0, 0.30);
	EQ(get(gpu, "min_freq"), "200000000");
	kiwami_clocks_stop();
	CHECK(lines_with(slurp(logf), "(running slow)\n") == 0);	// (no step for it; the summary names it with a comma)

	// fast-forward with no limit: the top at once, the clock from before after it
	setup();
	remember("c", "g", "cpu 816000\n");
	start("c", "g");
	begin();
	run(1.0, 0.30);
	F.ff = 1;
	frame(16667, 0.30);
	CHECK(maxf() == 1992000);
	run(2.0, 0.30);
	CHECK(maxf() == 1992000);
	F.ff = 0;
	frame(16667, 0.30);
	CHECK(maxf() == 1104000);
	kiwami_clocks_stop();

	// fast-forward with a limit (3x: 5556 us): up a step a second while RetroArch's period is over it, never down, the
	// clock from before after it; the memory stays the game's (816 MHz: it started a step above and needed no more),
	// though fast-forward ran longer and higher
	setup();
	remember("c", "g", "cpu 816000\n");
	start("c", "g");
	begin();
	run(1.0, 0.30);
	F.ff = 1;
	F.ff_limit_us = 5556;
	F.pace_us = 8000;
	run(0.9, 0.95);
	CHECK(maxf() == 1104000);
	run(0.2, 0.95);
	CHECK(maxf() == 1416000);
	run(1.0, 0.95);
	CHECK(maxf() == 1608000);
	F.pace_us = 5556;
	run(8.0, 0.10);
	CHECK(maxf() == 1608000);
	F.ff = 0;
	F.ff_limit_us = 0;
	F.pace_us = 16667;
	frame(16667, 0.30);
	CHECK(maxf() == 1104000);
	run(1.0, 0.30);
	kiwami_clocks_stop();
	EQ(get(data, "c/g.clocks"), "cpu 816000");
	CHECK(strstr(slurp(logf), "fast-forward 10.") != NULL);

	// the game's memory is the highest clock it was stepped up to at normal speed, though it spent longer lower down:
	// 1104 MHz, needed once, then 15 s at 408 MHz
	setup();
	remember("c", "g", "cpu 600000\n");
	start("c", "g");
	begin();
	run(1.0, 0.95);
	frame(16667, 0.95);		// (the second's last frame: the next window is all at 30%)
	CHECK(maxf() == 1104000);
	run(30.0, 0.30);
	CHECK(maxf() == 408000);
	kiwami_clocks_stop();
	EQ(get(data, "c/g.clocks"), "cpu 1104000");
	// ... never stepped up: the clock it ended on, here still on the way down from the top after 23 s (four clocks of
	// 5 s each, the one it ended on 3 s)
	setup();
	start("c", "g");
	begin();
	run(23.0, 0.30);
	CHECK(maxf() == 1104000);
	kiwami_clocks_stop();
	EQ(get(data, "c/g.clocks"), "cpu 1104000");
	// ... but not over the memory it started from: 3 s at a step above it, no step, keeps 816 MHz
	setup();
	remember("c", "g", "cpu 816000\n");
	start("c", "g");
	begin();
	run(3.0, 0.30);
	kiwami_clocks_stop();
	EQ(get(data, "c/g.clocks"), "cpu 816000");

	// the menu (hold) counts nothing; a 2 s gap starts the window afresh
	setup();
	remember("c", "g", "cpu 816000\n");
	start("c", "g");
	begin();
	F.hold = 1;
	run(3.0, 0.95);
	frame(33333, 0.95);
	frame(33333, 0.10);
	CHECK(maxf() == 1104000);
	EQ(get(gpu, "min_freq"), "200000000");
	F.hold = 0;
	run(0.5, 0.95);
	frame(2000000, 0.95);
	run(0.6, 0.95);
	CHECK(maxf() == 1104000);
	run(0.5, 0.95);
	CHECK(maxf() == 1416000);
	kiwami_clocks_stop();

	// stop: the limits as found, the floor at its lowest, the game's clocks (the CPU's it ended on, never stepped up; the
	// GPU's floor held longest), the log; once
	setup();
	CHECK(start("Snes9x", "a/b\x01" "c.sfc") == 1);
	begin();
	run(1.0, 0.30);
	frame(33333, 0.30);
	run(8.0, 0.30);
	kiwami_clocks_stop();
	EQ(get(cpu, "scaling_governor"), "schedutil");
	EQ(get(cpu, "scaling_min_freq"), "600000");
	EQ(get(cpu, "scaling_max_freq"), "1416000");
	EQ(get(gpu, "min_freq"), "200000000");
	CHECK(kiwami_clocks_mhz() == 0);
	EQ(slurp(path(data, "Snes9x/a_b_c.sfc.clocks")), "cpu 1800000\ngpu 300000000\n");
	const char *log = slurp(logf);
	CHECK(strstr(log, "kiwami-clocks: start: a_b_c.sfc (Snes9x), 60.00 fps, 60.00 Hz (the panel's rate), CPU 1992 MHz (the top) of 408-1992, GPU floor 200 MHz (the lowest) of 200-800\n") != NULL);
	CHECK(strstr(log, "s: GPU floor 200 -> 300 MHz (late flip)\n") != NULL);
	CHECK(strstr(log, "s: CPU 1992 -> 1800 MHz (headroom)\n") != NULL);
	CHECK(strstr(log, "kiwami-clocks: end after ") != NULL);
	CHECK(strstr(log, "kiwami-clocks: time at each CPU clock: 1800 MHz 4.") != NULL);
	CHECK(strstr(log, ", 1992 MHz 5.") != NULL);
	CHECK(strstr(log, "kiwami-clocks: time at each GPU floor: 200 MHz 1.0 s, 300 MHz 8.0 s\n") != NULL);
	CHECK(strstr(log, "kiwami-clocks: late frames: CPU 0 (late flip) 0 (running slow), GPU 1 (late flip) 0 (running slow)") != NULL);
	CHECK(strstr(log, "kiwami-clocks: remembered in ") != NULL);
	kiwami_clocks_stop();
	CHECK(lines_with(slurp(logf), "end after") == 1);
	EQ(get(cpu, "scaling_max_freq"), "1416000");

	// no GPU devfreq: the CPU alone
	setup();
	CHECK(kiwami_clocks_start(cpu, NULL, data, logf, "c", "g") == 1);
	begin();
	run(1.0, 0.30);
	frame(33333, 0.30);
	EQ(get(gpu, "min_freq"), "200000000");
	kiwami_clocks_stop();
	CHECK(strstr(slurp(logf), "no GPU devfreq") != NULL);

	// no cpufreq: off, said once, nothing written, and the calls do nothing
	setup();
	put(gpu, "min_freq", "300000000\n");
	char none[400];
	snprintf(none, sizeof none, "%s/none", root);
	CHECK(kiwami_clocks_start(none, gpu, data, logf, "c", "g") == 0);
	begin();
	run(2.0, 0.95);
	frame(33333, 0.30);
	kiwami_clocks_stop();
	CHECK(kiwami_clocks_mhz() == 0);
	EQ(slurp(logf), "kiwami-clocks: off, no cpufreq\n");
	EQ(get(gpu, "min_freq"), "300000000");
	CHECK(access(data, F_OK) != 0);
	// ... and a cpufreq that can't be written to (read-only files; root writes them anyway)
	if (geteuid() != 0) {
		setup();
		chmod(path(cpu, "scaling_min_freq"), 0444);
		CHECK(start("c", "g") == 0);
		EQ(get(cpu, "scaling_min_freq"), "600000");
		EQ(get(cpu, "scaling_max_freq"), "1416000");
		EQ(get(cpu, "scaling_governor"), "schedutil");
		EQ(get(gpu, "min_freq"), "200000000");
		CHECK(lines_with(slurp(logf), "kiwami-clocks: off, no cpufreq\n") == 1);
	}

	char cmd[300];
	snprintf(cmd, sizeof cmd, "rm -rf '%s'", root);
	if (system(cmd)) return 1;
	if (failed) {
		fprintf(stderr, "clocks-units: %d of %d checks failed\n", failed, checks);
		return 1;
	}
	printf("clocks-units: %d checks passed\n", checks);
	return 0;
}
