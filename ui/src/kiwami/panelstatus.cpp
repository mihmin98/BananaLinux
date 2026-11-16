// banana-ui (kiwami): the game panel's status strip's state (panelstatus.h).
#include "panelstatus.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <utility>

namespace banana::kiwami
{
	static const double READ_MS = 10000;	// the battery and network read at most this often
	static const double WOKE_MS = 1000;		// asleep() grown by more than this since the last pass: a resume

	PanelStatus::PanelStatus(Sources s) : src(std::move(s)) {}

	void PanelStatus::open(bool twelveHour, time_t fixedClock)
	{
		twelve = twelveHour;
		fixed = fixedClock;
		started = false;
		minute = playedMin = -1;
		st = PanelState();
		logged = PanelReadings();
		pending.clear();
	}

	bool PanelStatus::update(double t)
	{
		double a = src.asleep ? src.asleep() : 0;
		bool first = !started, woke = false;
		PanelState before = st;
		if (first)
		{
			started = true;
			start = t;
		}
		else
			woke = a - lastAsleep > WOKE_MS;
		lastAsleep = a;
		if (first || woke || t - lastRead >= READ_MS)
		{
			st.r = src.read ? src.read() : PanelReadings();
			lastRead = t;
		}
		double ran = std::max(0.0, t - start);
		time_t w = fixed ? fixed + (time_t)std::floor(ran / 1000) : src.wall ? src.wall() : time(nullptr);
		long long m = (long long)std::floor((double)w / 60);
		if (m != minute)
		{
			minute = m;
			st.clock = clockText(w, twelve);
			st.date = dateText(w);
		}
		long long pm = (long long)(ran / 60000);
		if (pm != playedMin)
		{
			playedMin = pm;
			st.played = played(ran);
		}
		if (first || woke || st.r != logged)
		{
			pending = (woke ? "after a sleep: " : "") + line();
			logged = st.r;
		}
		return first || st != before;
	}

	std::string PanelStatus::logLine()
	{
		std::string l;
		l.swap(pending);
		return l;
	}

	std::string PanelStatus::line() const
	{
		std::string battery = st.r.percent < 0 ? "none" : std::to_string(st.r.percent) + "%";
		return "panel status: " + st.clock + " " + st.date + ", battery " + battery + (st.r.charging ? ", charging" : "")
			+ ", wifi " + (st.r.wifi ? "up" : "down") + ", tailscale " + (st.r.tailnet ? "up" : "down") + ", played "
			+ st.played;
	}

	std::string PanelStatus::played(double ms)
	{
		long long m = ms > 0 ? (long long)(ms / 60000) : 0;
		char b[32];
		snprintf(b, sizeof b, "%lld:%02lld", m / 60, m % 60);
		return b;
	}

	// as Library::clockText and dateText (library.cpp), on a given time
	std::string PanelStatus::clockText(time_t w, bool twelveHour)
	{
		struct tm t;
		localtime_r(&w, &t);
		char b[16];
		strftime(b, sizeof b, twelveHour ? "%I:%M %p" : "%H:%M", &t);
		return b;
	}

	std::string PanelStatus::dateText(time_t w)
	{
		struct tm t;
		localtime_r(&w, &t);
		char b[16];
		strftime(b, sizeof b, "%m/%d", &t);
		return b;
	}
}
