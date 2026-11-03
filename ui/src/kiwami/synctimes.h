#pragma once
// banana-sync (kiwami): each phase of a Home server sync timed, for Network > Home server > Show sync times
// (ui/src/sync.cpp). A phase is a status title: status() hands every line it writes to seen(), and a "running" line
// whose title (Games 3/10 is Games, an empty one Starting) isn't the phase in hand starts a new one. Its start goes to
// <run>/sync.times, one line written before that phase's first status, which the menu reads to draw the sync's and
// the phase's clocks under Sync now's bar:
//   <banana-sync's pid> <the sync's start> <the phase's start> <the phase>
// in CLOCK_MONOTONIC milliseconds, so time asleep isn't counted. The file goes when the sync ends ("done", "error"); a
// sync killed leaves it, and the menu takes it only while its pid is sync.pid's. Each phase's duration, with its last
// counts, goes at the end of the sync's report (report()). The steps that write no status of their own (the card
// checked against the list, files removed, the gamelists, the saves read) get one through tick(), at most every
// TICK_MS. With the setting off, none of this writes anything. banana-ui's side is at the end: the times file read
// (parse()), and the line drawn under the bar (line(): the phase, its time, its time left, the sync's time), worked
// out live from the status and the two starts (sync-times.cpp). Header-only: banana-sync is built from sync.cpp and
// json.cpp alone.
#include <algorithm>
#include <cstdio>
#include <ctime>
#include <sstream>
#include <string>
#include <vector>
#include <unistd.h>

namespace banana::kiwami::synctimes
{
	// CLOCK_MONOTONIC in ms: time asleep isn't counted (as banana-ui's nowMs())
	inline long long monoMs()
	{
		timespec ts;
		clock_gettime(CLOCK_MONOTONIC, &ts);
		return (long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
	}

	namespace detail
	{
		// a status line read as Settings' syncBar() reads it: "running <a> <b> <what>|<item>"; false for another state
		inline bool fields(const std::string& status, long long& a, long long& b, std::string& what)
		{
			std::istringstream in(status);
			std::string state, rest;
			a = b = 0;
			in >> state;
			if (state != "running") return false;
			in >> a >> b;
			std::getline(in, rest);
			size_t s = rest.find_first_not_of(" \t\r\n"), e = rest.find_last_not_of(" \t\r\n");
			rest = s == std::string::npos ? "" : rest.substr(s, e - s + 1);
			what = rest.substr(0, rest.find('|'));
			return true;
		}
		// "Games 3/10" -> "Games": a trailing " <i>/<N>" dropped; i and N set to its numbers when there is one
		inline std::string stripCount(const std::string& what, long long* i = nullptr, long long* n = nullptr)
		{
			size_t sp = what.rfind(' ');
			if (sp == std::string::npos) return what;
			std::string c = what.substr(sp + 1);
			size_t sl = c.find('/');
			if (sl == std::string::npos || sl == 0 || sl + 1 == c.size() || sl > 18 || c.size() - sl - 1 > 18) return what;
			if (c.find_first_not_of("0123456789/") != std::string::npos || c.find('/', sl + 1) != std::string::npos) return what;
			if (i) *i = std::stoll(c.substr(0, sl));
			if (n) *n = std::stoll(c.substr(sl + 1));
			return what.substr(0, sp);
		}
	}

	// a status line's phase: "running <a> <b> <what>|<item>" -> what, without a trailing " <i>/<N>" ("Games 3/10" is
	// Games), "Starting" for an empty what (as Settings draws it); "" for a line that isn't running
	inline std::string phaseOf(const std::string& status)
	{
		long long a, b;
		std::string what;
		if (!detail::fields(status, a, b, what)) return "";
		what = detail::stripCount(what);
		return what.empty() ? "Starting" : what;
	}

	constexpr int TICK_MS = 250;	// the new phases' statuses: at most one every TICK_MS, and their first and last
	struct Phase { std::string name, what; long long ms = 0, a = 0, b = 0; };	// a phase over: its last status's title and counts
	struct Tracker
	{
		bool on = false;				// kiwami_sync_times = on, for this sync
		std::string run;				// banana-sync's --run folder: sync.times goes there
		long long syncStart = 0, phaseStart = 0;
		std::string phase, what;		// the phase in hand, and its last status's title
		long long a = 0, b = 0;			// and counts
		std::vector<Phase> done;
		bool ended = false;
		std::string tickPhase;			// tick()'s phase, its items counted so far, and its last status's time
		long long tickN = 0, tickAt = 0;
	};
	// the one tracker (a function's static: header-only)
	inline Tracker& tracker()
	{
		static Tracker t;
		return t;
	}

	namespace detail
	{
		// the phase in hand, if any, closed into done at now
		inline void close(Tracker& t, long long now)
		{
			if (t.phase.empty()) return;
			t.done.push_back({ t.phase, t.what, now - t.phaseStart, t.a, t.b });
			t.phase.clear();
		}
		inline std::string timesFile(const Tracker& t) { return t.run + "/sync.times"; }
	}

	// doSync: a fresh tracker; the sync's clock starts. Writes nothing
	inline void start(bool on, const std::string& run)
	{
		Tracker& t = tracker();
		t = Tracker();
		t.on = on;
		t.run = run;
		t.syncStart = monoMs();
	}

	// status()'s hook: each line written. A new phase's start is in sync.times before the line is in the status file
	inline void seen(const std::string& status)
	{
		Tracker& t = tracker();
		if (!t.on || t.ended) return;
		long long now = monoMs(), a, b;
		std::string what;
		if (!detail::fields(status, a, b, what))
		{
			detail::close(t, now);	// (done, error: the sync is over)
			unlink(detail::timesFile(t).c_str());
			t.ended = true;
			return;
		}
		std::string ph = phaseOf(status);
		if (ph != t.phase)
		{
			detail::close(t, now);
			t.phase = ph;
			t.phaseStart = now;
			std::string file = detail::timesFile(t), tmp = file + ".new";
			if (FILE* f = fopen(tmp.c_str(), "w"))
			{
				fprintf(f, "%d %lld %lld %s\n", (int)getpid(), t.syncStart, t.phaseStart, ph.c_str());
				fclose(f);
				rename(tmp.c_str(), file.c_str());
			}
		}
		t.what = what;
		t.a = a;
		t.b = b;
	}

	// a step that writes no status of its own, at the top of its loop: the item in hand counted, and a status
	// "running <n> <total> <phase>|<item>" written for the first and the last, and in between at most every TICK_MS.
	// It goes through status() (sync.cpp's), so seen() takes the phase from it
	inline void tick(void (*status)(const std::string&), const char* phase, size_t total, const std::string& item)
	{
		Tracker& t = tracker();
		if (!t.on) return;
		if (t.tickPhase != phase) { t.tickPhase = phase; t.tickN = 0; }
		t.tickN++;
		long long now = monoMs();
		if (t.tickN == 1 || t.tickN == (long long)total || now - t.tickAt >= TICK_MS)
		{
			t.tickAt = now;
			status("running " + std::to_string(t.tickN) + " " + std::to_string(total) + " " + phase + "|" + item);
		}
	}

	// the report's times: lines (after its memory: line), one a phase and the whole sync's; none when off
	inline std::vector<std::string> report()
	{
		std::vector<std::string> out;
		Tracker& t = tracker();
		if (!t.on) return out;
		char b[160];
		for (auto& p : t.done)
		{
			snprintf(b, sizeof b, "times: %s %.1f s", p.name.c_str(), p.ms / 1000.0);
			std::string l = b;
			long long i = 0, n = 0;
			if (p.name == "Games" && detail::stripCount(p.what, &i, &n) != p.what)
				l += ", " + std::to_string(p.a) + " of " + std::to_string(p.b) + " MB, " + std::to_string(i) + " of " + std::to_string(n) + " files";
			else if (p.b > 0) l += ", " + std::to_string(p.a) + " of " + std::to_string(p.b);
			out.push_back(l);
		}
		snprintf(b, sizeof b, "times: sync %.1f s", (monoMs() - t.syncStart) / 1000.0);
		out.push_back(b);
		return out;
	}

	// ---- the menu's side (ui/src/kiwami/sync-times.cpp): the line under Sync now's bar, from the status and sync.times
	struct Times { long long pid = 0, syncStart = 0, phaseStart = 0; std::string phase; };
	// sync.times' line; false when it isn't one
	inline bool parse(const std::string& line, Times& t)
	{
		std::istringstream in(line);
		Times r;
		if (!(in >> r.pid >> r.syncStart >> r.phaseStart)) return false;
		std::getline(in, r.phase);
		if (r.phase.empty() || r.phase[0] != ' ') return false;
		r.phase.erase(0, 1);
		while (!r.phase.empty() && (r.phase.back() == '\n' || r.phase.back() == '\r')) r.phase.pop_back();
		if (r.phase.empty()) return false;
		t = r;
		return true;
	}

	// elapsed: 0:05, 59:59, 1:00:00, 1:02:03 (floored to the second; nothing below 0:00)
	inline std::string clock(double ms)
	{
		long long s = ms > 0 ? (long long)(ms / 1000) : 0;
		char b[32];
		if (s < 3600) snprintf(b, sizeof b, "%lld:%02lld", s / 60, s % 60);
		else snprintf(b, sizeof b, "%lld:%02lld:%02lld", s / 3600, s / 60 % 60, s % 60);
		return b;
	}

	// a time left as Files writes one (settings_files.cpp's duration(), in its anonymous namespace): 45 s, 3 min,
	// 1 h 5 min; capped at 359999 s as Files caps it
	inline std::string left(double s)
	{
		if (s > 359999) s = 359999;
		if (s < 60) return std::to_string((int)s + 1) + " s";
		if (s < 3600) return std::to_string((int)(s / 60) + 1) + " min";
		return std::to_string((int)(s / 3600)) + " h " + std::to_string((int)(s / 60) % 60) + " min";
	}

	constexpr double ETA_AFTER_MS = 3000;	// no estimate before the phase has run this long

	// the phase's time left in seconds, -1 for none: its elapsed time times (total - done) / done, once it has run
	// ETA_AFTER_MS and done one. Listing games: the games read of the expected; Games: the larger of the MB (the bar's)
	// and the files (Games i/N: i - 1 done of N); any other: the item in hand less one, of the total
	inline double secondsLeft(const std::string& status, double phaseMs)
	{
		long long a, b;
		std::string what;
		if (!detail::fields(status, a, b, what) || phaseMs < ETA_AFTER_MS) return -1;
		std::string phase = phaseOf(status);
		auto est = [&](long long done, long long total) -> double {
			if (done < 1 || total <= 0) return -1;
			if (done >= total) return 0;
			return phaseMs / 1000.0 * (double)(total - done) / (double)done;
		};
		double s;
		if (phase == "Listing games") s = est(a, b);
		else if (phase == "Games")
		{
			long long i = 0, n = 0;
			detail::stripCount(what, &i, &n);
			s = std::max(est(std::min(a, b), b), est(i - 1, n));
		}
		else s = est(a - 1, b);
		return s < 0 ? -1 : std::min(s, 359999.0);
	}

	// the line under Sync now's bar: "<phase> · <phase's time> · <time left> left · sync <sync's time>" (no time left
	// without an estimate); "" when the status isn't running, or its phase isn't the times file's
	inline std::string line(const std::string& status, const Times& t, double now)
	{
		std::string phase = phaseOf(status);
		if (phase.empty() || phase != t.phase) return "";
		const std::string dot = " \xc2\xb7 ";	// (as Files writes it)
		double s = secondsLeft(status, now - t.phaseStart);
		return phase + dot + clock(now - t.phaseStart) + (s >= 0 ? dot + left(s) + " left" : "") + dot + "sync " + clock(now - t.syncStart);
	}

	// ms to the next whole second of either clock, at least 1
	inline int nextTick(const Times& t, double now)
	{
		int best = 1000;
		for (long long start : { t.syncStart, t.phaseStart })
		{
			long long e = (long long)now - start, m = e % 1000;
			if (m < 0) m += 1000;
			best = std::min(best, (int)(1000 - m));
		}
		return std::max(1, best);
	}
}
