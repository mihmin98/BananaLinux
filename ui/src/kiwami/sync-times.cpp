// banana-ui (kiwami): Network > Home server > Show sync times (kiwami.h). While a sync runs and Sync now is selected,
// a line under upstream's bar: the phase, how long it has run, its time left once there is a rate to go on, and the
// whole sync's time ("Games · 2:10 · 3 min left · sync 6:40"). banana-sync writes each phase's start to
// <run>/sync.times (synctimes.h); the line is worked out here from that and the status Settings already reads, and
// Settings wakes at the next status read and the next whole second, so the clocks tick with no key pressed. A times
// file whose pid isn't sync.pid's (a sync killed, or one not started yet), or whose phase isn't the status's, shows
// nothing. With the row Off, nothing is read and Settings wakes as upstream's does.
#include "kiwami.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>

namespace banana
{
	namespace
	{
		std::string firstLine(const std::string& path)
		{
			std::ifstream f(path);
			std::string l;
			std::getline(f, l);
			return l;
		}
	}

	bool Settings::Kiwami::timesOn() const
	{
		for (auto& it : st.items)
			if (it.key == "kiwami_sync_times") return it.at >= 0 && it.at < (int)it.values.size() && it.values[it.at] == "on";
		return false;
	}

	bool Settings::Kiwami::timesShown() const
	{
		// render()'s own condition for Sync now's bar
		return st.mOpen && st.page == LIST && !st.hotAsk && !st.inGroups && st.langAsk < 0 && st.sel >= 0 &&
			st.sel < (int)st.items.size() && st.items[st.sel].key == "hs_sync" && st.syncing() && timesOn();
	}

	std::string Settings::Kiwami::timesRun() const
	{
		const char* e = getenv("KIWAMI_SYNC_RUN");
		return e && *e ? e : "/run/banana";
	}

	bool Settings::Kiwami::timesUpdate(double t)
	{
		// the test's status: upstream's pollSync() reads /run/banana/sync.status, which a test can't write (this runs
		// after it, so the test's line is the one drawn)
		if (const char* e = getenv("KIWAMI_SYNC_RUN"); e && *e) st.syncLine = firstLine(std::string(e) + "/sync.status");
		if (!timesShown())
		{
			bool had = !timesText.empty();
			timesText.clear();
			timesShape.clear();
			return had;
		}
		if (st.syncLine != timesSeen)
		{
			// a new status: a new phase's start is in sync.times by now (banana-sync writes it first)
			std::string run = timesRun();
			std::string pid = firstLine(run + "/sync.pid");
			timesValid = kiwami::synctimes::parse(firstLine(run + "/sync.times"), times) && !pid.empty() &&
				std::to_string(times.pid) == pid;
			timesSeen = st.syncLine;
		}
		std::string text = timesValid ? kiwami::synctimes::line(st.syncLine, times, t) : "";
		// (the log once a shape: a new phase, or its estimate appearing; not every second, the log is on the card)
		std::string shape = text.empty() ? "" : times.phase + (kiwami::synctimes::secondsLeft(st.syncLine, t - times.phaseStart) >= 0 ? " left" : "");
		if (!text.empty() && shape != timesShape) fprintf(stderr, "banana-ui: kiwami: sync times: %s\n", text.c_str());
		timesShape = shape;
		bool changed = text != timesText;
		timesText = text;
		return changed;
	}

	int Settings::Kiwami::timesWake(double t) const
	{
		if (!timesShown()) return -1;
		int w = std::max(0, (int)(st.syncNext - t));	// upstream's next status read
		if (timesValid) w = std::min(w, kiwami::synctimes::nextTick(times, t));
		return w;
	}

	void Settings::Kiwami::syncTimes(float x, float y, float w)
	{
		if (!timesShown() || timesText.empty()) return;
		st.text(st.fit(timesText, false, 16, w), false, 16, x, y, colours(st.light).muted);
	}
}
