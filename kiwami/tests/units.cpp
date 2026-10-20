// The fork's banana-ui code that needs no screen, checked on its own: the JSON reader, Tailscale's status, the
// device's name, the login's QR code and the devices' list. Built and run by kiwami/tests/ci.sh with the host's g++:
//   g++ -std=c++17 -Wall -I ui/src kiwami/tests/units.cpp ui/src/kiwami/json.cpp ui/src/kiwami/tsstatus.cpp
//       ui/src/kiwami/qrcodegen.cpp -o units
//   ./units kiwami/tests/tailscale
#include "kiwami/json.h"
#include "kiwami/tsstatus.h"
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

using namespace banana::kiwami;

static int checks = 0, failed = 0;
#define CHECK(c) do { checks++; if (!(c)) { failed++; fprintf(stderr, "units: FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)
#define EQ(a, b) do { checks++; std::string a_ = (a), b_ = (b); if (a_ != b_) { failed++; fprintf(stderr, "units: FAIL %s:%d: %s is \"%s\", not \"%s\"\n", __FILE__, __LINE__, #a, a_.c_str(), b_.c_str()); } } while (0)

static std::string dir;
static std::string capture(const std::string& name)
{
	std::ifstream f(dir + "/" + name + ".json");
	if (!f) { fprintf(stderr, "units: no capture %s/%s.json\n", dir.c_str(), name.c_str()); failed++; }
	std::stringstream ss;
	ss << f.rdbuf();
	return ss.str();
}

static void testJson()
{
	json::Value v;
	CHECK(json::parse(" {\"a\": [1, -2.5e2, true, false, null, \"x\"], \"b\": {\"c\": {}}, \"d\": []} ", v));
	CHECK(v.type == json::Value::OBJ && v.size() == 3);
	CHECK(v["a"].size() == 6);
	CHECK(v["a"][0].n == 1 && v["a"][1].n == -250);
	CHECK(v["a"][2].boolean() && !v["a"][3].boolean() && v["a"][3].type == json::Value::BOOL);
	CHECK(v["a"][4].isNull());
	EQ(v["a"][5].str(), "x");
	CHECK(v["b"]["c"].type == json::Value::OBJ && v["b"]["c"].size() == 0);
	CHECK(v["d"].type == json::Value::ARR && v["d"].size() == 0);
	// missing keys, indexes and wrong types read as null, and chain
	CHECK(v["nope"]["deeper"][3].isNull());
	EQ(v["a"].str(), "");
	CHECK(v["a"][99].isNull());
	CHECK(!v["a"][0].boolean());
	// the last of two members of the same name, as Go's decoder
	CHECK(json::parse("{\"k\": 1, \"k\": 2}", v) && v["k"].n == 2);

	// escapes: the simple ones, \u in the BMP, a surrogate pair, a lone half, and Go's \u0026 for &
	CHECK(json::parse("\"q\\\" b\\\\ s\\/ \\b\\f\\n\\r\\t\"", v));
	EQ(v.str(), "q\" b\\ s/ \b\f\n\r\t");
	CHECK(json::parse("\"caf\\u00e9 \\u2019 \\u0026 \\ud83c\\udf4c\"", v));
	EQ(v.str(), "caf\xc3\xa9 \xe2\x80\x99 & \xf0\x9f\x8d\x8c");
	CHECK(json::parse("\"\\ud83c x \\udf4c\"", v));
	EQ(v.str(), "\xef\xbf\xbd x \xef\xbf\xbd");
	CHECK(json::parse("\"raw UTF-8: \xc3\xa9\"", v));
	EQ(v.str(), "raw UTF-8: \xc3\xa9");

	// malformed: refused, and the value left null
	const char* bad[] = { "", "   ", "{", "}", "[1,]", "[1 2]", "{\"a\" 1}", "{\"a\":}", "{a: 1}", "{\"a\": 1,}", "\"open",
		"\"bad \\x escape\"", "\"\\u12\"", "\"\\u12zz\"", "\"tab\there\"", "01", "1.", "-", "1e", ".5", "+1", "tru", "nul", "[1] 2",
		"{\"a\": 1} x", "[\"a\"", "\"\\" };
	for (const char* b : bad)
	{
		bool ok = json::parse(b, v);
		if (ok) fprintf(stderr, "units: accepted %s\n", b);
		CHECK(!ok && v.isNull());
	}
	// deep nesting stops, without the stack running out
	CHECK(!json::parse(std::string(100000, '['), v));
	std::string deep = std::string(64, '[') + std::string(64, ']');
	CHECK(json::parse(deep, v));
	CHECK(!json::parse("[" + deep + "]", v));
	// a truncated capture (a read cut short) is refused anywhere it is cut
	std::string run = capture("running");
	size_t close = run.rfind('}');
	for (size_t at = 0; at < close; at += 97) CHECK(!json::parse(run.substr(0, at), v));
	CHECK(json::parse(run, v));
}

static void testStatus()
{
	// the helper's own lines, before tailscaled answers
	TsStatus s = parseStatus("state off\n");
	CHECK(s.state == TsStatus::OFF && s.error.empty());
	EQ(rowWord(s, true), "Starting");
	s = parseStatus("state off\nerror the tun module could not be loaded\n");
	CHECK(s.state == TsStatus::OFF);
	EQ(s.error, "the tun module could not be loaded");
	EQ(rowWord(s, true), "Error");
	s = parseStatus("state starting\n");
	CHECK(s.state == TsStatus::STARTING);
	EQ(rowWord(s, true), "Starting");
	EQ(rowWord(s, false), "Waiting for Wi-Fi");
	// nothing readable (the helper failed): still starting, nothing invented
	s = parseStatus("");
	CHECK(s.state == TsStatus::STARTING && s.peers.empty() && s.self.name.empty());
	s = parseStatus("{\"BackendState\": \"Runn");
	CHECK(s.state == TsStatus::STARTING && s.backend.empty());

	s = parseStatus(capture("starting"));
	CHECK(s.state == TsStatus::STARTING && !s.self.online);
	EQ(rowWord(s, true), "Starting");
	EQ(rowWord(s, false), "Waiting for Wi-Fi");

	s = parseStatus(capture("needs-login"));
	CHECK(s.state == TsStatus::NEEDS_LOGIN);
	EQ(s.backend, "NeedsLogin");
	EQ(s.authUrl, "https://login.tailscale.com/a/1a2b3c4d5e6f7a");
	EQ(s.self.name, "banana-rgds");	// no MagicDNS name yet: the host name
	CHECK(s.self.ipv4.empty() && s.peers.empty() && s.tailnet.empty());
	EQ(rowWord(s, true), "Log in");
	EQ(rowWord(s, false), "Waiting for Wi-Fi");

	// a refused auth key: the helper's error, then tailscaled's NeedsLogin
	s = parseStatus("error auth key refused: backend error: invalid key: API key does not exist\n" + capture("needs-login"));
	CHECK(s.state == TsStatus::NEEDS_LOGIN);
	EQ(s.error, "auth key refused: backend error: invalid key: API key does not exist");
	EQ(s.authUrl, "https://login.tailscale.com/a/1a2b3c4d5e6f7a");
	EQ(rowWord(s, true), "Log in");

	s = parseStatus(capture("needs-approval"));
	CHECK(s.state == TsStatus::NEEDS_APPROVAL);
	EQ(rowWord(s, true), "Waiting for approval");
	EQ(rowWord(s, false), "Waiting for approval");

	s = parseStatus(capture("running"));
	CHECK(s.state == TsStatus::RUNNING && s.error.empty() && s.health.empty());
	EQ(s.tailnet, "someone@example.com");
	EQ(s.self.name, "banana-rgds");
	EQ(s.self.dnsName, "banana-rgds.tail5c2e1.ts.net.");
	EQ(s.self.ipv4, "100.64.0.7");
	EQ(s.self.ipv6, "fd7a:115c:a1e0::7");
	CHECK(s.self.online && !s.self.expired);
	CHECK(s.peers.size() == 7);
	if (s.peers.size() == 7)
	{
		EQ(s.peers[0].name, "desktop");
		EQ(s.peers[0].os, "windows");
		EQ(s.peers[1].hostName, "Alex\xe2\x80\x99s iPhone");
		EQ(s.peers[1].name, "alexs-iphone");
		EQ(s.peers[3].ipv4, "100.96.3.15");
		CHECK(!s.peers[3].online);
		EQ(s.peers[3].lastSeen, "2026-10-18T21:42:10Z");
		CHECK(s.peers[0].online);
		EQ(s.peers[0].lastSeen, "");	// Go's zero time
	}
	EQ(rowWord(s, true), "100.64.0.7");
	EQ(rowWord(s, false), "100.64.0.7");	// still connected to Tailscale: the address
	TsStatus off = s;
	off.self.online = false;
	EQ(rowWord(off, false), "Waiting for Wi-Fi");
	off.self.ipv4.clear();
	EQ(rowWord(off, true), "Connected");

	s = parseStatus(capture("expired"));
	CHECK(s.state == TsStatus::NEEDS_LOGIN && s.self.expired);
	EQ(rowWord(s, true), "Log in again");
	CHECK(s.health.size() == 1);

	s = parseStatus(capture("clock"));
	CHECK(s.state == TsStatus::NEEDS_LOGIN && s.authUrl.empty());
	CHECK(s.health.size() == 1 && s.health[0].find("x509") != std::string::npos);

	TsStatus stopped = parseStatus("{\"BackendState\": \"Stopped\"}");
	CHECK(stopped.state == TsStatus::STOPPED);
	EQ(rowWord(stopped, true), "Stopped");
}

// the name: the board's by default (as the helper's, kiwami/tests/tailscale-helper.sh, from the same models), and what
// the keyboard's text becomes
static void testName()
{
	EQ(defaultName("Anbernic RG DS"), "banana-rgds");
	EQ(defaultName("Anbernic RG DS Plus"), "banana-rgds-plus");
	EQ(defaultName("Something else"), "banana");
	EQ(defaultName(""), "banana");
	EQ(normaliseName("My Handheld"), "my-handheld");
	EQ(normaliseName("  Kiwi 2 "), "kiwi-2");
	EQ(normaliseName("banana-rgds-plus"), "banana-rgds-plus");
	EQ(normaliseName("a"), "a");
	EQ(normaliseName("0"), "0");
	EQ(normaliseName(std::string(63, 'x')), std::string(63, 'x'));
	const std::string bad[] = { "", "   ", std::string(64, 'x'), "my_handheld", "-first", "last-", "a b-", "caf\xc3\xa9", "a.b", "a/b",
		" - ", "tab\there", "semi;colon" };
	for (const std::string& b : bad)
	{
		bool ok = normaliseName(b).empty();
		if (!ok) fprintf(stderr, "units: name accepted: \"%s\"\n", b.c_str());
		CHECK(ok);
	}
}

// the login link's QR code: a link of Tailscale's (~45 characters) is a version 4 code, 33 modules, 41 with the quiet
// zone: 8 device pixels a module fit the 640x480 top panel
static void testQr()
{
	int side = -1;
	for (const char* url : { "https://login.tailscale.com/a/1a2b3c4d5e6f7a", "https://login.tailscale.com/a/1a2b3c4d5e6f7a8" })
	{
		std::vector<bool> m = loginQr(url, side);
		CHECK(side == 33 + 8 && m.size() == 41u * 41u);
		if (m.size() != 41u * 41u) continue;
		bool quiet = true;
		for (int i = 0; i < 41; i++)
			for (int q = 0; q < 4; q++)
				quiet &= !m[q * 41 + i] && !m[(40 - q) * 41 + i] && !m[i * 41 + q] && !m[i * 41 + 40 - q];
		CHECK(quiet);
		// the finders' outer rings at three corners, dark; the module inside the top left one's ring, light
		CHECK(m[4 * 41 + 4] && m[4 * 41 + 10] && m[10 * 41 + 4] && m[4 * 41 + 30] && m[30 * 41 + 4] && m[36 * 41 + 10]);
		CHECK(!m[5 * 41 + 5]);
	}
	CHECK(loginQr("", side).empty() && side == 0);
	CHECK(loginQr(std::string(4000, 'x'), side).empty() && side == 0);	// more than a QR code holds
}

// the devices page: this one first, those online, the rest, each by name; when an offline one was last seen
static void testDevices()
{
	std::vector<TsDevice> l = deviceList(parseStatus(capture("running")));
	std::string order;
	for (auto& d : l) order += d.name + (d.self ? "*" : d.online ? "" : "-") + " ";
	EQ(order, "banana-rgds* alexs-iphone desktop nas raspberrypi banana-rgds-plus- living-room-tv- macbook- ");
	CHECK(deviceList(TsStatus()).empty());

	long long now = 1792370530;	// 2026-10-19T00:42:10Z
	EQ(seenAgo("2026-10-19T00:42:00Z", now), "a moment ago");
	EQ(seenAgo("2026-10-19T00:45:00Z", now), "a moment ago");	// a clock behind the other device's
	EQ(seenAgo("2026-10-19T00:41:10Z", now), "a minute ago");
	EQ(seenAgo("2026-10-19T00:30:10.123456789Z", now), "12 minutes ago");
	EQ(seenAgo("2026-10-18T23:42:10Z", now), "an hour ago");
	EQ(seenAgo("2026-10-18T21:42:10Z", now), "3 hours ago");
	EQ(seenAgo("2026-10-19T02:42:10+03:00", now), "an hour ago");	// 23:42:10Z
	EQ(seenAgo("2026-10-18T00:00:00Z", now), "a day ago");
	EQ(seenAgo("2026-10-12T19:05:00Z", now), "6 days ago");
	EQ(seenAgo("2026-08-01T10:00:00Z", now), "on 2026-08-01");
	EQ(seenAgo("", now), "");
	EQ(seenAgo("yesterday", now), "");
	EQ(seenAgo("2026-10-18T21:42:10", now), "");	// no zone
	EQ(osName("linux"), "Linux");
	EQ(osName("windows"), "Windows");
	EQ(osName("iOS"), "iOS");
	EQ(osName("plan9"), "plan9");
}

int main(int argc, char** argv)
{
	dir = argc > 1 ? argv[1] : "kiwami/tests/tailscale";
	testJson();
	testStatus();
	testName();
	testQr();
	testDevices();
	if (failed) { fprintf(stderr, "units: %d of %d checks failed\n", failed, checks); return 1; }
	printf("units: %d checks passed\n", checks);
	return 0;
}
