#pragma once
// banana-ui (kiwami): the game panel's status strip's state, for Settings > Other systems > Second screen > Status on
// the game panel (kiwami_panel_status): the time and date, the battery, Wi-Fi and Tailscale, and the time played this
// game, everything the strip knows but its drawing (Settings::Kiwami's). No I/O of its own: its sources are given, so
// the tests give stand-ins.
//
// update() runs on each pass of the panel's loop, with the loop's clock (CLOCK_MONOTONIC ms, which stops while
// asleep, as the library's play time). The battery, Wi-Fi and Tailscale are read at the first pass, then every 10 s of
// that clock, and at once on the first pass after a sleep (asleep() grew: CLOCK_BOOTTIME - CLOCK_MONOTONIC). The clock
// and date are formatted only when the minute turns, the time played only when its minute does, as the menu's
// Library::clockText and dateText format them. A line for the log is ready when a game starts, when a reading
// changes, and after every sleep ("after a sleep: " before it); never for the minute alone.
#include <ctime>
#include <functional>
#include <string>

namespace banana::kiwami
{
	struct PanelReadings
	{
		int percent = -1;		// the battery's, -1 for no reading
		bool charging = false, wifi = false, tailnet = false;
		bool operator==(const PanelReadings& o) const
		{
			return percent == o.percent && charging == o.charging && wifi == o.wifi && tailnet == o.tailnet;
		}
		bool operator!=(const PanelReadings& o) const { return !(*this == o); }
	};

	struct PanelState
	{
		std::string clock, date, played;	// "14:32" or "02:32 PM", "11/16", "1:23"
		PanelReadings r;
		bool operator==(const PanelState& o) const
		{
			return clock == o.clock && date == o.date && played == o.played && r == o.r;
		}
		bool operator!=(const PanelState& o) const { return !(*this == o); }
	};

	class PanelStatus
	{
	public:
		struct Sources
		{
			std::function<PanelReadings()> read;	// the battery, Wi-Fi and Tailscale (at most every 10 s, and after a sleep)
			std::function<time_t()> wall;			// the real clock (time(nullptr))
			std::function<double()> asleep;			// ms slept since boot: CLOCK_BOOTTIME - CLOCK_MONOTONIC
		};
		explicit PanelStatus(Sources s);
		void open(bool twelveHour, time_t fixedClock = 0);	// a game starts; fixedClock: the tests' clock at its start
		bool update(double t);				// t: the panel loop's ms (CLOCK_MONOTONIC); true: the state changed
		bool ready() const { return started; }	// update() ran since open()
		const PanelState& state() const { return st; }
		std::string logLine();				// the line to log after update(), "" none (taken once)

		static std::string played(double ms);						// "0:00", "0:07", "1:23", "12:05"
		static std::string clockText(time_t w, bool twelveHour);	// "%H:%M" or "%I:%M %p", localtime
		static std::string dateText(time_t w);						// "%m/%d"

	private:
		std::string line() const;			// the state as the log says it

		Sources src;
		bool twelve = false;
		time_t fixed = 0;
		bool started = false;
		double start = 0, lastRead = 0, lastAsleep = 0;
		long long minute = -1, playedMin = -1;	// what clock/date and played were last formatted for
		PanelState st;
		PanelReadings logged;				// the readings of the last line
		std::string pending;
	};
}
