// banana-sync's phase times (ui/src/kiwami/synctimes.h), checked on their own: a status line's phase, the times file
// written at each phase's start and removed when the sync ends, the report's times: lines, nothing at all when the
// setting is off, and the 250 ms throttle of the steps that had no status of their own. And the menu's side: the
// clocks, the time left (as Files writes one, and the rule for each phase), the line, the times file read, and the
// next tick. Built and run by
// kiwami/tests/sync-times.sh with the host's g++:
//   g++ -std=c++17 -Wall -Werror -I ui/src kiwami/tests/synctimes-units.cpp -o synctimes-units
//   ./synctimes-units
#include "kiwami/synctimes.h"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <regex>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

using namespace banana::kiwami;

static int checks = 0, failed = 0;
#define CHECK(c) do { checks++; if (!(c)) { failed++; fprintf(stderr, "synctimes-units: FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)
#define EQ(a, b) do { checks++; std::string a_ = (a), b_ = (b); if (a_ != b_) { failed++; fprintf(stderr, "synctimes-units: FAIL %s:%d: %s is \"%s\", not \"%s\"\n", __FILE__, __LINE__, #a, a_.c_str(), b_.c_str()); } } while (0)
#define MATCH(s, re) do { checks++; std::string s_ = (s); if (!std::regex_match(s_, std::regex(re))) { failed++; fprintf(stderr, "synctimes-units: FAIL %s:%d: %s is \"%s\", not /%s/\n", __FILE__, __LINE__, #s, s_.c_str(), re); } } while (0)

static std::string dir;	// a fresh run folder
static std::string timesPath() { return dir + "/sync.times"; }
static bool exists(const std::string& p) { struct stat st; return stat(p.c_str(), &st) == 0; }
static std::string slurp(const std::string& p)
{
	std::ifstream f(p);
	std::stringstream ss;
	ss << f.rdbuf();
	return ss.str();
}
// sync.times' fields
struct Fields { long long pid = -1, sync = -1, phase = -1; std::string name; };
static Fields fields(const std::string& text)
{
	Fields f;
	std::istringstream in(text);
	in >> f.pid >> f.sync >> f.phase;
	std::getline(in, f.name);
	if (!f.name.empty() && f.name[0] == ' ') f.name.erase(0, 1);
	return f;
}

// the status function tick() is given: its lines recorded, and handed on to seen() as sync.cpp's status() does
static std::vector<std::string> lines;
static void record(const std::string& s) { lines.push_back(s); synctimes::seen(s); }

static void testPhaseOf()
{
	EQ(synctimes::phaseOf("running 0 0 |My RomM"), "Starting");
	EQ(synctimes::phaseOf("running 5 20 Games 3/10|Game 0003 (USA).gba"), "Games");
	EQ(synctimes::phaseOf("running 10 180 Listing games|gba 10 / 30"), "Listing games");
	EQ(synctimes::phaseOf("running 1 6 Reading saves|gba"), "Reading saves");
	EQ(synctimes::phaseOf("running 0 0 |starting"), "Starting");	// (Settings' own, before banana-sync's first)
	EQ(synctimes::phaseOf("running 1 2 Checking the card|a/b 1/2.gba"), "Checking the card");	// (a count in the item isn't the title's)
	EQ(synctimes::phaseOf("done 1 0 0 0 0 0|"), "");
	EQ(synctimes::phaseOf("error stopped"), "");
	EQ(synctimes::phaseOf(""), "");
}

static void testTracker()
{
	synctimes::start(true, dir);
	CHECK(!exists(timesPath()));
	synctimes::seen("running 0 0 |x");
	Fields f = fields(slurp(timesPath()));
	CHECK(f.pid == getpid());
	CHECK(f.sync > 0 && f.sync == f.phase);
	EQ(f.name, "Starting");
	CHECK(!exists(timesPath() + ".new"));
	usleep(20000);
	synctimes::seen("running 1 5 Checking the card|a");
	Fields g = fields(slurp(timesPath()));
	CHECK(g.pid == getpid());
	CHECK(g.sync == f.sync);
	CHECK(g.phase >= f.phase + 15);
	EQ(g.name, "Checking the card");
	std::string before = slurp(timesPath());
	usleep(5000);
	synctimes::seen("running 2 5 Checking the card|b");
	EQ(slurp(timesPath()), before);
	synctimes::seen("done 0 0 0 0 0 0|");
	CHECK(!exists(timesPath()));
	synctimes::seen("running 0 0 |again");	// (after the end: nothing more)
	CHECK(!exists(timesPath()));
	std::vector<std::string> r = synctimes::report();
	CHECK(r.size() == 3);
	if (r.size() == 3)
	{
		MATCH(r[0], "times: Starting [0-9]+\\.[0-9] s");
		MATCH(r[1], "times: Checking the card [0-9]+\\.[0-9] s, 2 of 5");
		MATCH(r[2], "times: sync [0-9]+\\.[0-9] s");
	}

	// a Games phase reports its MB and its files from its last status
	synctimes::start(true, dir);
	synctimes::seen("running 0 0 |x");
	synctimes::seen("running 0 812 Games 1/120|a");
	synctimes::seen("running 812 812 Games 120/120|z");
	synctimes::seen("running 1 6 Gamelists|gba");
	synctimes::seen("error stopped");
	CHECK(!exists(timesPath()));
	r = synctimes::report();
	CHECK(r.size() == 4);
	if (r.size() == 4)
	{
		MATCH(r[0], "times: Starting [0-9]+\\.[0-9] s");
		MATCH(r[1], "times: Games [0-9]+\\.[0-9] s, 812 of 812 MB, 120 of 120 files");
		MATCH(r[2], "times: Gamelists [0-9]+\\.[0-9] s, 1 of 6");
		MATCH(r[3], "times: sync [0-9]+\\.[0-9] s");
	}
}

static void testOff()
{
	synctimes::start(false, dir);
	lines.clear();
	synctimes::seen("running 0 0 |x");
	synctimes::tick(record, "Removing", 3, "a");
	synctimes::seen("running 1 5 Checking the card|a");
	CHECK(!exists(timesPath()));
	CHECK(lines.empty());
	synctimes::seen("done 0 0 0 0 0 0|");
	CHECK(synctimes::report().empty());
}

static void testThrottle()
{
	synctimes::start(true, dir);
	lines.clear();
	for (int i = 1; i <= 1000; i++) synctimes::tick(record, "X", 1000, "i" + std::to_string(i));
	CHECK(!lines.empty() && lines.size() <= 4);
	if (!lines.empty())
	{
		EQ(lines.front(), "running 1 1000 X|i1");
		EQ(lines.back(), "running 1000 1000 X|i1000");
	}
	EQ(fields(slurp(timesPath())).name, "X");
	// a tick once TICK_MS has gone by is written
	synctimes::start(true, dir);
	lines.clear();
	synctimes::tick(record, "Y", 10, "a");
	synctimes::tick(record, "Y", 10, "b");
	CHECK(lines.size() == 1);
	usleep(260000);
	synctimes::tick(record, "Y", 10, "c");
	CHECK(lines.size() == 2);
	if (lines.size() == 2) EQ(lines[1], "running 3 10 Y|c");
	// a new phase's first tick at once, counted from 1
	synctimes::tick(record, "Z", 4, "d");
	CHECK(lines.size() == 3);
	if (lines.size() == 3) EQ(lines[2], "running 1 4 Z|d");
	EQ(fields(slurp(timesPath())).name, "Z");
	synctimes::seen("done 0 0 0 0 0 0|");
	CHECK(!exists(timesPath()));
}

// ---- the menu's side
// a number as text with one decimal, so EQ shows both sides
static std::string d1(double d) { char b[64]; snprintf(b, sizeof b, "%.1f", d); return b; }
// ui/src/settings_files.cpp's duration(), copied: left() has to write a time as Files does
static std::string filesDuration(double s)
{
	if (s < 60) return std::to_string((int)s + 1) + " s";
	if (s < 3600) return std::to_string((int)(s / 60) + 1) + " min";
	return std::to_string((int)(s / 3600)) + " h " + std::to_string((int)(s / 60) % 60) + " min";
}

static void testMenu()
{
	EQ(synctimes::clock(0), "0:00");
	EQ(synctimes::clock(5400), "0:05");
	EQ(synctimes::clock(3599999), "59:59");
	EQ(synctimes::clock(3600000), "1:00:00");
	EQ(synctimes::clock(3723400), "1:02:03");
	EQ(synctimes::clock(-5), "0:00");

	EQ(synctimes::left(44.2), "45 s");
	EQ(synctimes::left(150), "3 min");
	EQ(synctimes::left(3900), "1 h 5 min");
	int differ = 0;
	for (double s = 0; s <= 7200; s += 0.7) if (synctimes::left(s) != filesDuration(s)) differ++;
	CHECK(differ == 0);
	EQ(synctimes::left(1e9), filesDuration(359999));

	EQ(d1(synctimes::secondsLeft("running 5 20 Games 3/10|g", 10400)), "41.6");	// (files 41.6 > MB 31.2)
	EQ(d1(synctimes::secondsLeft("running 0 1 Games 4/10|g", 6000)), "14.0");		// (files only)
	EQ(d1(synctimes::secondsLeft("running 4 10 Games 1/10|g", 4000)), "6.0");		// (MB only: no file done)
	EQ(d1(synctimes::secondsLeft("running 5 20 Games 3/10|g", 2999)), "-1.0");
	EQ(d1(synctimes::secondsLeft("running 60 180 Listing games|gba 60 / 30", 6000)), "12.0");
	EQ(d1(synctimes::secondsLeft("running 60 0 Listing games|gba 60", 6000)), "-1.0");
	EQ(d1(synctimes::secondsLeft("running 1 12 Saves|a.srm", 5000)), "-1.0");		// (none done)
	EQ(d1(synctimes::secondsLeft("running 4 12 Saves|a.srm", 6000)), "18.0");
	EQ(d1(synctimes::secondsLeft("running 0 0 |x", 9000)), "-1.0");
	EQ(d1(synctimes::secondsLeft("running 12 12 Saves|a.srm", 6000)), "0.5");		// (the last in hand: 11 of 12 done)
	EQ(d1(synctimes::secondsLeft("done 1 0 0 0 0 0|", 9000)), "-1.0");

	synctimes::Times t{ 1, 2001, 92001, "Games" };
	EQ(synctimes::line("running 5 20 Games 3/10|g", t, 102401), "Games \xc2\xb7 0:10 \xc2\xb7 42 s left \xc2\xb7 sync 1:40");
	EQ(synctimes::line("running 5 20 Games 3/10|g", t, 94001), "Games \xc2\xb7 0:02 \xc2\xb7 sync 1:32");
	synctimes::Times saves{ 1, 2001, 92001, "Saves" };
	EQ(synctimes::line("running 5 20 Games 3/10|g", saves, 102401), "");
	EQ(synctimes::line("done 1 0 0 0 0 0|", t, 102401), "");

	synctimes::Times p;
	CHECK(synctimes::parse("4242 2001 92001 Checking the card", p));
	CHECK(p.pid == 4242 && p.syncStart == 2001 && p.phaseStart == 92001);
	EQ(p.phase, "Checking the card");
	synctimes::Times q;
	CHECK(!synctimes::parse("x", q));
	CHECK(!synctimes::parse("", q));
	CHECK(!synctimes::parse("4242 1", q));
	CHECK(!synctimes::parse("4242 1 2", q));
	CHECK(q.pid == 0 && q.phase.empty());	// (left as it was)

	CHECK(synctimes::nextTick({ 1, 0, 400, "X" }, 1250) == 150);
	CHECK(synctimes::nextTick({ 1, 0, 0, "X" }, 3000) == 1000);
}

int main()
{
	char tmpl[] = "/tmp/synctimes-units.XXXXXX";
	if (!mkdtemp(tmpl)) { perror("synctimes-units: mkdtemp"); return 1; }
	dir = tmpl;
	testPhaseOf();
	testTracker();
	testOff();
	testThrottle();
	testMenu();
	unlink(timesPath().c_str());
	rmdir(dir.c_str());
	if (failed) { fprintf(stderr, "synctimes-units: %d of %d checks failed\n", failed, checks); return 1; }
	printf("synctimes-units: %d checks passed\n", checks);
	return 0;
}
