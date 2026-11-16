// The game panel's status strip's state (ui/src/kiwami/panelstatus.h), checked on its own with stand-in sources that
// count their calls: the formats, the minute, the reads every 10 s and after a sleep, the changes, the log's lines and
// a new game. Built and run by kiwami/tests/ci.sh with the host's g++ (in UTC, set here):
//   g++ -std=c++17 -Wall -I ui/src kiwami/tests/panelstatus-units.cpp ui/src/kiwami/panelstatus.cpp -o panelstatus-units
//   ./panelstatus-units
#include "kiwami/panelstatus.h"
#include <cstdio>
#include <cstdlib>
#include <string>

using namespace banana::kiwami;

static int checks = 0, failed = 0;
#define CHECK(c) do { checks++; if (!(c)) { failed++; fprintf(stderr, "panelstatus-units: FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)
#define EQ(a, b) do { checks++; std::string a_ = (a), b_ = (b); if (a_ != b_) { failed++; fprintf(stderr, "panelstatus-units: FAIL %s:%d: %s is \"%s\", not \"%s\"\n", __FILE__, __LINE__, #a, a_.c_str(), b_.c_str()); } } while (0)

static const time_t CLOCK = 1794839570;		// 14:32:50 on 11/16 (UTC)

// the sources, counting their calls
struct Stand
{
	PanelReadings r;
	int reads = 0, walls = 0;
	double slept = 0;
	PanelStatus::Sources sources()
	{
		return { [this] { reads++; return r; }, [this] { walls++; return CLOCK; }, [this] { return slept; } };
	}
};

int main()
{
	setenv("TZ", "UTC", 1);
	tzset();

	// the formats
	EQ(PanelStatus::played(0), "0:00");
	EQ(PanelStatus::played(59999), "0:00");
	EQ(PanelStatus::played(60000), "0:01");
	EQ(PanelStatus::played(83 * 60000.0), "1:23");
	EQ(PanelStatus::played((12 * 60 + 5) * 60000.0), "12:05");
	EQ(PanelStatus::played(-5), "0:00");
	EQ(PanelStatus::clockText(CLOCK, false), "14:32");
	EQ(PanelStatus::clockText(CLOCK, true), "02:32 PM");
	EQ(PanelStatus::dateText(CLOCK), "11/16");

	Stand s;
	s.r.percent = 78; s.r.wifi = true;
	PanelStatus p(s.sources());
	p.open(false, CLOCK);
	CHECK(!p.ready());
	EQ(p.logLine(), "");

	// the first pass: one read, the whole state on the line
	CHECK(p.update(100000));
	CHECK(p.ready());
	CHECK(s.reads == 1);
	EQ(p.state().clock, "14:32");
	EQ(p.state().date, "11/16");
	EQ(p.state().played, "0:00");
	EQ(p.logLine(), "panel status: 14:32 11/16, battery 78%, wifi up, tailscale down, played 0:00");
	EQ(p.logLine(), "");		// taken once
	CHECK(s.walls == 0);		// the fixed clock: the real one isn't asked

	// 5 s on: nothing read, nothing changed
	CHECK(!p.update(105000));
	CHECK(s.reads == 1);
	// 10 s on: 14:33:00, read again (the same), no line for the minute
	CHECK(p.update(110000));
	CHECK(s.reads == 2);
	EQ(p.state().clock, "14:33");
	EQ(p.logLine(), "");
	// a minute played
	CHECK(p.update(160000));
	EQ(p.state().played, "0:01");
	EQ(p.logLine(), "");
	int reads = s.reads;

	// a reading changes: seen at the next 10 s, and logged
	s.r.percent = 77;
	CHECK(!p.update(165000));
	CHECK(p.state().r.percent == 78);
	CHECK(p.update(170000));
	CHECK(s.reads == reads + 1);
	EQ(p.logLine(), "panel status: 14:34 11/16, battery 77%, wifi up, tailscale down, played 0:01");
	s.r.charging = true;
	CHECK(p.update(180000));
	EQ(p.logLine(), "panel status: 14:34 11/16, battery 77%, charging, wifi up, tailscale down, played 0:01");
	s.r.wifi = false;
	CHECK(p.update(190000));
	EQ(p.logLine(), "panel status: 14:34 11/16, battery 77%, charging, wifi down, tailscale down, played 0:01");
	s.r.tailnet = true;
	CHECK(p.update(200000));
	EQ(p.logLine(), "panel status: 14:34 11/16, battery 77%, charging, wifi down, tailscale up, played 0:01");
	// the same readings again: nothing
	CHECK(!p.update(210000 - 1));
	CHECK(!p.update(210000));
	EQ(p.logLine(), "");

	// a sleep: read at once (100 ms after the last pass), the line marked, time played as before (it follows t)
	reads = s.reads;
	s.slept += 5000;
	s.r.percent = 70;
	CHECK(p.update(210100));
	CHECK(s.reads == reads + 1);
	CHECK(p.state().r.percent == 70);
	EQ(p.state().played, "0:01");
	EQ(p.logLine(), "after a sleep: panel status: 14:34 11/16, battery 70%, charging, wifi down, tailscale up, played 0:01");
	// a sleep with the same readings: read at once, still one line, marked
	s.slept += 3000;
	CHECK(!p.update(210200));
	CHECK(s.reads == reads + 2);
	EQ(p.logLine(), "after a sleep: panel status: 14:34 11/16, battery 70%, charging, wifi down, tailscale up, played 0:01");
	// under a second's growth (scheduling noise): no sleep
	s.slept += 500;
	CHECK(!p.update(210300));
	CHECK(s.reads == reads + 2);
	EQ(p.logLine(), "");

	// a new game: not ready until its first pass, which starts again at 0:00 with a whole line
	p.open(true, CLOCK);
	CHECK(!p.ready());
	EQ(p.logLine(), "");
	CHECK(p.update(500000));
	CHECK(p.ready());
	EQ(p.state().clock, "02:32 PM");
	EQ(p.state().played, "0:00");
	EQ(p.logLine(), "panel status: 02:32 PM 11/16, battery 70%, charging, wifi down, tailscale up, played 0:00");

	// no battery reading
	s.r = PanelReadings();
	p.open(false, CLOCK);
	CHECK(p.update(0));
	EQ(p.logLine(), "panel status: 14:32 11/16, battery none, wifi down, tailscale down, played 0:00");

	// the real clock, asked on each pass when no fixed one is given
	Stand r;
	PanelStatus q(r.sources());
	q.open(false);
	CHECK(q.update(1000));
	CHECK(r.walls == 1);
	EQ(q.state().clock, "14:32");
	CHECK(!q.update(1100));
	CHECK(r.walls == 2);

	if (failed) { fprintf(stderr, "panelstatus-units: %d of %d checks failed\n", failed, checks); return 1; }
	printf("panelstatus-units: %d checks passed\n", checks);
	return 0;
}
