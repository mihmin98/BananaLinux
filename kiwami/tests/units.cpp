// The fork's banana-ui code that needs no screen, checked on its own: the JSON reader, Tailscale's status, the
// device's name, the login's QR code, the devices' list, Settings' grid of groups, RetroAchievements' replies, its
// account on the card and its games' hashes. Built and run by kiwami/tests/ci.sh with the host's g++ (the vendored C
// first, make -C ui kiwami-c):
//   g++ -std=c++17 -Wall -I ui/src kiwami/tests/units.cpp ui/src/kiwami/json.cpp ui/src/kiwami/tsstatus.cpp
//       ui/src/kiwami/qrcodegen.cpp ui/src/kiwami/grid.cpp ui/src/kiwami/raclient.cpp ui/src/kiwami/rastore.cpp
//       ui/src/kiwami/rahash.cpp $(find ui/src/kiwami/third_party -name '*.kiwami.o') -ldl -o units
//   ./units kiwami/tests/tailscale kiwami/tests/ra
#include "kiwami/grid.h"
#include "kiwami/json.h"
#include "kiwami/raclient.h"
#include "kiwami/rahash.h"
#include "kiwami/rastore.h"
#include "kiwami/tsstatus.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

using namespace banana::kiwami;

static int checks = 0, failed = 0;
#define CHECK(c) do { checks++; if (!(c)) { failed++; fprintf(stderr, "units: FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)
#define EQ(a, b) do { checks++; std::string a_ = (a), b_ = (b); if (a_ != b_) { failed++; fprintf(stderr, "units: FAIL %s:%d: %s is \"%s\", not \"%s\"\n", __FILE__, __LINE__, #a, a_.c_str(), b_.c_str()); } } while (0)

static std::string dir, raDir;
static std::string capture(const std::string& name, const std::string& in = dir)
{
	std::ifstream f(in + "/" + name + ".json");
	if (!f) { fprintf(stderr, "units: no capture %s/%s.json\n", in.c_str(), name.c_str()); failed++; }
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

#define IEQ(a, b) do { checks++; int a_ = (a), b_ = (b); if (a_ != b_) { failed++; fprintf(stderr, "units: FAIL %s:%d: %s is %d, not %d\n", __FILE__, __LINE__, #a, a_, b_); } } while (0)

static void testGrid()
{
	// a full last row: upstream's (g ± 3) % n, for every group
	for (int n : { 3, 6, 9 })
		for (int g = 0; g < n; g++)
		{
			IEQ(gridMove(g, n, false), (g + 3) % n);
			IEQ(gridMove(g, n, true), (g + n - 3) % n);
			IEQ(gridTop(0, g, n), n == 9 && g >= 6 ? 1 : 0);
		}
	// seven groups: RetroAchievements (6) alone on the third row
	IEQ(gridMove(3, 7, false), 6);	// DOWN: the tile below
	IEQ(gridMove(4, 7, false), 6);	// DOWN from a row above the last, nothing below: the last tile
	IEQ(gridMove(5, 7, false), 6);
	IEQ(gridMove(6, 7, false), 0);	// DOWN on the last row: the first row, same column
	IEQ(gridMove(6, 7, true), 3);	// UP: the tile above
	IEQ(gridMove(0, 7, true), 6);	// UP on the first row: the last row, same column
	IEQ(gridMove(1, 7, true), 6);	// ... or its last tile when that column is empty
	IEQ(gridMove(2, 7, true), 6);
	// eight: 6 and 7 on the third row
	IEQ(gridMove(4, 8, false), 7);
	IEQ(gridMove(5, 8, false), 7);
	IEQ(gridMove(7, 8, false), 1);
	IEQ(gridMove(1, 8, true), 7);
	IEQ(gridMove(2, 8, true), 7);
	IEQ(gridMove(7, 8, true), 4);
	// the rows shown move only to show the selection: down to the third shows 2 and 3, up to the second keeps them,
	// up to the first shows 1 and 2
	for (int n : { 7, 8 })
	{
		IEQ(gridTop(0, 6, n), 1);
		IEQ(gridTop(1, 3, n), 1);
		IEQ(gridTop(1, 0, n), 0);
		IEQ(gridTop(0, 3, n), 0);
		IEQ(gridTop(1, n - 1, n), 1);
		IEQ(gridTop(5, 0, n), 0);	// (a row from a longer grid: back in range)
	}
	IEQ(gridTop(3, 0, 6), 0);
}

// RetroAchievements' replies (kiwami/tests/ra, the stand-in server's): each request's, a refusal (the server saying
// no), and what isn't an answer (none, a page that isn't JSON, JSON cut short, an empty body)
static void testRa()
{
	using namespace banana::kiwami::ra;
	auto ra = [](const std::string& name) { return capture(name, raDir); };
	auto nothing = [](const Reply& r, const char* error) {
		CHECK(!r.ok); CHECK(!r.refused); EQ(r.error.substr(0, std::string(error).size()), error);
	};

	std::string body = ra("login2");
	for (auto& [k, v] : { std::make_pair("@USER@", "alice"), std::make_pair("@TOKEN@", "tok1"), std::make_pair("@MEDIA@", "http://m") })
		for (size_t at; (at = body.find(k)) != std::string::npos;) body.replace(at, strlen(k), v);
	Login l = parseLogin(body, 200);
	CHECK(l.ok); CHECK(!l.refused);
	EQ(l.user, "alice"); EQ(l.token, "tok1"); EQ(l.avatarUrl, "http://m/UserPic/alice.png");
	IEQ((int)l.score, 1234); IEQ((int)l.softScore, 56); IEQ((int)l.messages, 2);
	l = parseLogin(ra("login2-refused"), 401);
	CHECK(!l.ok); CHECK(l.refused); EQ(l.error, "Invalid user/password combination. Please try again.");
	CHECK(parseLogin("", 401).refused);
	CHECK(parseLogin("{\"Success\":false,\"Error\":\"Account locked\"}", 200).refused);
	nothing(parseLogin("", 0), "no connection");
	nothing(parseLogin(ra("unavailable"), 503), "unreadable answer (503)");
	nothing(parseLogin(ra("malformed"), 200), "unreadable answer (200)");
	nothing(parseLogin("", 200), "unreadable answer (200)");
	nothing(parseLogin("{\"Success\":true,\"User\":\"alice\"}", 200), "unreadable answer (no user or token)");
	nothing(parseLogin("{\"Success\":true,\"User\":\"alice\",\"Token\":\"tok1\"}", 500), "unreadable answer (500)");

	// a game completed in softcore, one mastered, one started; then none
	Progress p = parseProgress(ra("allprogress"), 200);
	CHECK(p.ok);
	IEQ((int)p.games.size(), 3);
	if (p.games.size() == 3)
	{
		IEQ((int)p.games[0].id, 1446); IEQ(p.games[0].total, 10); IEQ(p.games[0].unlocked, 10); IEQ(p.games[0].unlockedHardcore, 0);
		IEQ((int)p.games[2].id, 11240); IEQ(p.games[2].unlockedHardcore, 1);
	}
	IEQ(p.started, 3); IEQ(p.completed, 1); IEQ(p.mastered, 1); IEQ(p.unlocked, 18);
	p = parseProgress(ra("allprogress-empty"), 200);
	CHECK(p.ok); IEQ(p.started, 0); IEQ(p.unlocked, 0);
	CHECK(parseProgress(ra("token-refused"), 401).refused);
	nothing(parseProgress("{\"Success\":true}", 200), "unreadable answer (no Response)");
	nothing(parseProgress(ra("malformed"), 200), "unreadable answer");
	nothing(parseProgress("", 0), "no connection");

	GameId g = parseGameId(ra("gameid-m0f343b0931126a20f133d67c2b018a3b"), 200);
	CHECK(g.ok); IEQ((int)g.id, 1446);
	g = parseGameId(ra("gameid"), 200);
	CHECK(g.ok); IEQ((int)g.id, 0);
	nothing(parseGameId("", 0), "no connection");

	// core achievements only (Flags 3; the unofficial one, 5, dropped), their texts unescaped
	Patch pa = parsePatch(ra("patch"), 200);
	CHECK(pa.ok); EQ(pa.title, "Orchard Tactics");
	IEQ((int)pa.achievements.size(), 3);
	if (pa.achievements.size() == 3)
	{
		IEQ((int)pa.achievements[0].id, 101); EQ(pa.achievements[0].title, "First Harvest");
		EQ(pa.achievements[0].description, "Pick the first apple"); EQ(pa.achievements[0].badge, "101234"); IEQ(pa.achievements[0].points, 5);
		EQ(pa.achievements[1].title, "Pear \"Tree\" \xc3\xa9");
		IEQ((int)pa.achievements[2].id, 103); IEQ(pa.achievements[2].points, 25);
	}
	CHECK(parsePatch(ra("token-refused"), 401).refused);
	nothing(parsePatch("{\"Success\":true}", 200), "unreadable answer (no PatchData)");
	nothing(parsePatch(ra("malformed"), 200), "unreadable answer");

	Unlocks u = parseUnlocks(ra("unlocks"), 200);
	CHECK(u.ok); IEQ((int)u.ids.size(), 2);
	if (u.ids.size() == 2) { IEQ((int)u.ids[0], 101); IEQ((int)u.ids[1], 102); }
	u = parseUnlocks(ra("unlocks-h1"), 200);
	IEQ((int)u.ids.size(), 1);
	CHECK(parseUnlocks(ra("token-refused"), 401).refused);
	nothing(parseUnlocks("", 0), "no connection");

	// the consoles, in the Stats page's order
	const unsigned ids[9] = { 18, 5, 6, 4, 7, 3, 1, 12, 2 };
	for (int i = 0; i < 9; i++) IEQ((int)CONSOLES[i].id, (int)ids[i]);
	EQ(CONSOLES[6].system, "megadrive");

	EQ(escape("Alice_1-2.3~"), "Alice_1-2.3~");
	EQ(escape("a b&c=d/+%\xc3\xa9"), "a%20b%26c%3Dd%2F%2B%25%C3%A9");
	EQ(fileName("Alice_01"), "alice_01");
	EQ(fileName("../x"), "");
	EQ(fileName("a/b"), "");
	EQ(fileName(""), "");
}

static std::string slurp(const std::string& path)
{
	std::ifstream f(path);
	std::stringstream ss;
	ss << f.rdbuf();
	return ss.str();
}
static void spit(const std::string& path, const std::string& text) { std::ofstream(path) << text; }
static bool exists(const std::string& path) { return access(path.c_str(), F_OK) == 0; }

// the account on a made-up card (<tmp>/banana as Settings' data folder, retroarch/, superdrastic/ and bananads/ beside
// it): signing in, out, removing and adopting, what each leaves in each file
static void testRaCard()
{
	using namespace banana::kiwami::ra;
	const char* tmp = getenv("TMPDIR");
	std::string tmpl = std::string(tmp && *tmp ? tmp : "/tmp") + "/kiwami-units-XXXXXX";
	if (!mkdtemp(&tmpl[0])) { fprintf(stderr, "units: no temporary folder\n"); failed++; return; }
	std::string t = tmpl, data = t + "/banana";
	for (const char* d : { "/banana", "/banana/cache", "/banana/cache/ra-avatars", "/retroarch", "/superdrastic", "/bananads" })
		mkdir((t + d).c_str(), 0755);
	Card c(data + "/");
	EQ(c.accounts, data + "/accounts.conf");
	EQ(c.retroarch, t + "/retroarch/retroarch.cfg");
	EQ(c.superdrastic, t + "/superdrastic/ra.token");
	EQ(c.bananads, t + "/bananads/ra.token");

	// a fresh card: no account, nothing remembered, nothing written by looking
	CHECK(!adopt(c));
	CHECK(accounts(c).current.empty() && accounts(c).remembered.empty());
	CHECK(!exists(c.list) && !exists(c.accounts));
	CHECK(!needsToken(c));

	// read as the share script reads it: spaces round the key and the value, quotes, the last line winning
	spit(c.accounts, "smb_user=me\n  ra_user = \"Bob\" \r\nra_user_x=no\n# ra_user=comment\nsmb_password=x\n");
	EQ(confGet(c.accounts, "ra_user"), "Bob");
	spit(c.accounts, "smb_user=me\nra_user=old\nra_user=Bob\nsmb_password=x\n");
	EQ(confGet(c.accounts, "ra_user"), "Bob");
	EQ(confGet(c.accounts, "ra_password"), "");

	// signing in writes the three keys, Bob's lines replaced and the others kept in their order; Bob, the account
	// before, is remembered
	std::string error;
	CHECK(signedIn(c, "alice", "pw1", "tok1", error));
	EQ(slurp(c.accounts), "smb_user=me\nra_user=alice\nsmb_password=x\nra_password=pw1\nra_retroarch_token=tok1\n");
	EQ(slurp(c.list), "current\talice\nremembered\tBob\n");
	CHECK(!needsToken(c));
	// a second account: the first remembered, in front; no token, no token line
	CHECK(signedIn(c, "carol", "pw3", "", error));
	EQ(slurp(c.accounts), "smb_user=me\nra_user=carol\nsmb_password=x\nra_password=pw3\n");
	EQ(slurp(c.list), "current\tcarol\nremembered\talice\nremembered\tBob\n");
	CHECK(needsToken(c));
	// names differing only in case are one account: ALICE leaves the list, carol joins it
	CHECK(signedIn(c, "ALICE", "pw1", "tok1", error));
	Accounts a = accounts(c);
	EQ(a.current, "ALICE");
	CHECK(a.remembered.size() == 2);
	if (a.remembered.size() == 2) { EQ(a.remembered[0], "carol"); EQ(a.remembered[1], "Bob"); }
	CHECK(sameName("Alice", "aLICE") && !sameName("alice", "alice2") && sameName("", ""));

	// signing out: the scrub first, the keys still there when it runs; then the keys gone, retroarch.cfg's two lines,
	// both tokens deleted, the account remembered first
	spit(t + "/scrub.sh", "grep -q '^ra_password=pw1$' \"$1\" && echo keys > \"$2\" || echo no keys > \"$2\"\n");
	setenv("KIWAMI_RA_SCRUB", ("sh " + t + "/scrub.sh " + c.accounts + " " + t + "/scrubbed").c_str(), 1);
	spit(c.retroarch, "video_driver = \"kms\"\ncheevos_enable = \"true\"\ncheevos_username = \"ALICE\"\ncheevos_username = \"x\"\n");
	spit(c.superdrastic, "ALICE\ntok1\n");
	spit(c.bananads, "ALICE\ntok1\n");
	CHECK(signOut(c, error));
	EQ(slurp(t + "/scrubbed"), "keys\n");
	EQ(slurp(c.accounts), "smb_user=me\nsmb_password=x\n");
	EQ(slurp(c.retroarch), "video_driver = \"kms\"\ncheevos_enable = \"false\"\ncheevos_username = \"\"\n");
	CHECK(!exists(c.superdrastic) && !exists(c.bananads));
	EQ(slurp(c.list), "remembered\tALICE\nremembered\tcarol\nremembered\tBob\n");
	CHECK(!adopt(c));	// (nothing on the card, nothing current: nothing to adopt)

	// with no retroarch.cfg, signing out makes none; a scrub that fails signs out all the same
	unlink(c.retroarch.c_str());
	CHECK(signedIn(c, "carol", "pw3", "tok3", error));
	setenv("KIWAMI_RA_SCRUB", "false", 1);
	CHECK(signOut(c, error));
	CHECK(!exists(c.retroarch));
	EQ(slurp(c.accounts), "smb_user=me\nsmb_password=x\n");
	EQ(accounts(c).remembered[0], "carol");
	setenv("KIWAMI_RA_SCRUB", "true", 1);

	// removing: off the list, with its avatar and its cached stats; the account signed in can't be removed
	Stats s;
	s.when = 1792370530; s.score = 1234; s.softScore = 56; s.messages = 2;
	s.systems[1] = { 3, 1, 1, 18 };
	s.systems[8] = { 1, 0, 0, 4 };
	CHECK(saveStats(c, "Bob", s, error));
	Stats r = loadStats(c, "BOB");
	CHECK(r.when == 1792370530 && r.score == 1234 && r.softScore == 56 && r.messages == 2);
	IEQ(r.systems[1].started, 3); IEQ(r.systems[1].completed, 1); IEQ(r.systems[1].mastered, 1); IEQ(r.systems[1].unlocked, 18);
	IEQ(r.systems[8].unlocked, 4); IEQ(r.systems[0].started, 0);
	CHECK(loadStats(c, "nobody").when == 0);
	spit(c.avatars + "/bob.png", "png");
	CHECK(remove(c, "BOB", error));
	CHECK(!exists(c.avatars + "/bob.png") && !exists(c.stats + "/bob"));
	EQ(slurp(c.list), "remembered\tcarol\nremembered\tALICE\n");
	CHECK(signedIn(c, "alice", "pw1", "tok1", error));
	CHECK(!remove(c, "Alice", error));
	EQ(accounts(c).current, "alice");

	// adopting the account accounts.conf names (the setup's step, RetroArch's own login): the old one remembered
	spit(c.accounts, "smb_user=me\nra_user=carol\nra_password=pw3\n");
	CHECK(adopt(c));
	EQ(slurp(c.list), "current\tcarol\nremembered\talice\n");
	CHECK(!adopt(c));
	spit(c.accounts, "ra_user=CAROL\n");
	CHECK(!adopt(c));
	spit(c.accounts, "smb_user=me\n");
	CHECK(adopt(c));
	EQ(slurp(c.list), "remembered\tcarol\nremembered\talice\n");

	// a folder that can't be written: signing in fails, with the files as they were (root writes anywhere: skipped)
	if (geteuid() != 0)
	{
		std::string conf = slurp(c.accounts), list = slurp(c.list);
		chmod(data.c_str(), 0555);
		error.clear();
		CHECK(!signedIn(c, "dave", "pw4", "tok4", error));
		CHECK(error.find("can't write") == 0);
		EQ(slurp(c.accounts), conf);
		EQ(slurp(c.list), list);
		CHECK(!exists(c.accounts + ".new"));
		chmod(data.c_str(), 0755);
	}
	unsetenv("KIWAMI_RA_SCRUB");
	if (system(("rm -rf " + t).c_str()) != 0) {}
}

// the game panel's token, as a RetroArch game leaves the card: retroarch.cfg with the account's name and no token
// (ra-secrets scrubs it out), accounts.conf with both. The panel (settings.cpp's gameOpen) reads the token from
// retroarch.cfg and, when it has none there, asks panelToken (its kiwami: line, here as one expression)
static void testPanelToken()
{
	using namespace banana::kiwami::ra;
	const char* tmp = getenv("TMPDIR");
	std::string tmpl = std::string(tmp && *tmp ? tmp : "/tmp") + "/kiwami-units-XXXXXX";
	if (!mkdtemp(&tmpl[0])) { fprintf(stderr, "units: no temporary folder\n"); failed++; return; }
	std::string t = tmpl;
	mkdir((t + "/banana").c_str(), 0755);
	mkdir((t + "/retroarch").c_str(), 0755);
	Card card(t + "/banana");
	auto panel = [&card](const std::string& user) {
		std::string token = confGet(card.retroarch, "cheevos_token");
		if (token.empty()) token = panelToken(card, user);
		return token;
	};
	spit(card.retroarch, "cheevos_enable = \"true\"\ncheevos_username = \"Alice\"\n");
	spit(card.accounts, "smb_user=me\nra_user=alice\nra_password=pw1\nra_retroarch_token=tok1\n");
	EQ(panelToken(card, "Alice"), "tok1");
	EQ(panel("Alice"), "tok1");
	// another account in accounts.conf, or none: no token
	spit(card.accounts, "ra_user=bob\nra_retroarch_token=tok2\n");
	EQ(panelToken(card, "Alice"), "");
	unlink(card.accounts.c_str());
	EQ(panelToken(card, "Alice"), "");
	EQ(panelToken(card, ""), "");
	// a card from before the scrub, its token in retroarch.cfg: that one, as before
	spit(card.retroarch, "cheevos_username = \"Alice\"\ncheevos_token = \"old\"\n");
	spit(card.accounts, "ra_user=alice\nra_retroarch_token=tok1\n");
	EQ(panel("Alice"), "old");
	if (system(("rm -rf " + t).c_str()) != 0) {}
}

// a file's MD5 as md5sum computes it (not rcheevos' own MD5, which is what is tested)
static std::string md5sum(const std::string& path)
{
	std::string out;
	if (FILE* p = popen(("md5sum '" + path + "'").c_str(), "r"))
	{
		char b[64] = "";
		if (fgets(b, sizeof b, p)) out.assign(b, strnlen(b, 32));
		pclose(p);
	}
	return out;
}
static std::string md5of(const std::string& bytes, const std::string& tmp)
{
	spit(tmp, bytes);
	return md5sum(tmp);
}

// the games' hashes as RetroArch computes them (rcheevos), each against an MD5 of the bytes rcheevos' rules say it
// hashes, computed here with md5sum; the PlayStation's formats against each other
static void testHash()
{
	using namespace banana::kiwami::ra;
	const char* tmp = getenv("TMPDIR");
	std::string tmpl = std::string(tmp && *tmp ? tmp : "/tmp") + "/kiwami-units-XXXXXX";
	if (!mkdtemp(&tmpl[0])) { fprintf(stderr, "units: no temporary folder\n"); failed++; return; }
	std::string t = tmpl, scratch = t + "/expected";
	auto bytes = [](size_t n, int seed) { std::string b(n, 0); for (size_t i = 0; i < n; i++) b[i] = (char)((i * 13 + seed) % 251); return b; };

	// a cartridge: the whole file (the fixture library's 1024 zero bytes, whose hash the RetroAchievements tests use)
	spit(t + "/zero.gba", std::string(1024, 0));
	EQ(hash(t + "/zero.gba", "gba").hash, "0f343b0931126a20f133d67c2b018a3b");
	EQ(md5sum(t + "/zero.gba"), "0f343b0931126a20f133d67c2b018a3b");
	std::string gb = bytes(2048, 1);
	spit(t + "/g.gb", gb);
	EQ(hash(t + "/g.gb", "gb").hash, md5of(gb, scratch));
	EQ(hash(t + "/g.gb", "megadrive").hash, md5of(gb, scratch));
	// an NES with its 16-byte header: the rest
	std::string nes = bytes(1024, 2);
	spit(t + "/n.nes", std::string("NES\x1a", 4) + std::string(12, 1) + nes);
	EQ(hash(t + "/n.nes", "nes").hash, md5of(nes, scratch));
	// a SNES with a 512-byte copier header (512 bytes over a whole number of 8 KB): the rest; a file 512 bytes over
	// another size has no header by rcheevos' rule, and is hashed whole
	std::string snes = bytes(8192, 3);
	spit(t + "/s.sfc", std::string(512, 0) + snes);
	EQ(hash(t + "/s.sfc", "snes").hash, md5of(snes, scratch));
	spit(t + "/s2.sfc", std::string(512, 0) + bytes(1024, 3));
	EQ(hash(t + "/s2.sfc", "snes").hash, md5sum(t + "/s2.sfc"));
	// an N64 in its three byte orders: the big-endian one's MD5 each time
	std::string z64 = "\x80\x37\x12\x40" + bytes(4092, 4), v64 = z64, n64 = z64;
	for (size_t i = 0; i < z64.size(); i += 2) std::swap(v64[i], v64[i + 1]);
	for (size_t i = 0; i < z64.size(); i += 4) { std::swap(n64[i], n64[i + 3]); std::swap(n64[i + 1], n64[i + 2]); }
	spit(t + "/a.z64", z64); spit(t + "/a.v64", v64); spit(t + "/a.n64", n64);
	std::string want = md5of(z64, scratch);
	EQ(hash(t + "/a.z64", "n64").hash, want);
	EQ(hash(t + "/a.v64", "n64").hash, want);
	EQ(hash(t + "/a.n64", "n64").hash, want);
	// a DS game: its header's first 0x160 bytes, the ARM9 and ARM7 code and the 0xA00 bytes of icon, where the header
	// says they are
	std::string nds(0xD000, 0), arm9 = bytes(0x800, 5), arm7 = bytes(0x400, 6), icon = bytes(0xA00, 7);
	auto le = [&nds](size_t at, uint32_t v) { for (int i = 0; i < 4; i++) nds[at + i] = (char)(v >> (8 * i)); };
	memcpy(&nds[0], "KIWAMI TEST", 11);
	le(0x20, 0x4000); le(0x2C, 0x800); le(0x30, 0x8000); le(0x3C, 0x400); le(0x68, 0xC000);
	nds.replace(0x4000, arm9.size(), arm9); nds.replace(0x8000, arm7.size(), arm7); nds.replace(0xC000, icon.size(), icon);
	spit(t + "/d.nds", nds);
	EQ(hash(t + "/d.nds", "nds").hash, md5of(nds.substr(0, 0x160) + arm9 + arm7 + icon, scratch));
	// a PlayStation disc (kiwami/tests/ra, make-fixtures.sh): the boot executable's name and its bytes, from the ISO, a
	// MODE1/2048 cue on it, the raw MODE2/2352 sectors and their cue, an .m3u naming that cue
	std::string exe = slurp(raDir + "/SLUS_000.01");
	CHECK(exe.size() == 4096);
	want = md5of("SLUS_000.01" + exe, scratch);
	for (const char* f : { "psx.iso", "psx-mode1.cue", "psx-mode2.cue", "psx.bin", "psx.m3u" })
	{
		Hash h = hash(raDir + "/" + f, "psx");
		if (h.hash != want) fprintf(stderr, "units: %s: \"%s\" (%s)\n", f, h.hash.c_str(), h.error.c_str());
		EQ(h.hash, want);
	}
	// a cartridge in a .zip or .7z (kiwami/tests/ra, make-fixtures.sh): the game in it, unpacked, as RetroArch picks
	// it: the first in name order, letters' case aside, of the files whose extension is the system's
	std::vector<std::string> gbaExts = { "gba", "zip", "7z" }, nesExts = { "nes", "zip", "7z" };
	want = md5sum(raDir + "/cart.gba");
	EQ(hash(raDir + "/cart.gba", "gba").hash, want);
	for (const char* f : { "cart.zip", "cart-lzma.7z", "cart-lzma2.7z", "several.zip", "several.7z" })
	{
		Hash h = hash(raDir + "/" + f, "gba", gbaExts);
		if (h.hash != want) fprintf(stderr, "units: %s: \"%s\" (%s)\n", f, h.hash.c_str(), h.error.c_str());
		EQ(h.hash, want);
	}
	// an NES's header left out of the game in a .7z, as of the plain file
	want = md5of(slurp(raDir + "/cart.nes").substr(16), scratch);
	EQ(hash(raDir + "/cart.nes", "nes").hash, want);
	EQ(hash(raDir + "/cart-nes.7z", "nes", nesExts).hash, want);
	// no game in it: none at all, or none of the system's
	EQ(hash(raDir + "/readme.zip", "gba", gbaExts).error, "no game in the archive");
	EQ(hash(raDir + "/readme.7z", "gba", gbaExts).error, "no game in the archive");
	EQ(hash(raDir + "/cart.zip", "nes", nesExts).error, "no game in the archive");
	EQ(hash(raDir + "/cart-lzma.7z", "nes", nesExts).error, "no game in the archive");
	// damaged: cut short, a byte of the packed game changed (the decoder or the CRC says so), not an archive
	std::string zip = slurp(raDir + "/cart.zip"), sz = slurp(raDir + "/cart-lzma2.7z");
	spit(t + "/cut.zip", zip.substr(0, zip.size() / 2));
	EQ(hash(t + "/cut.zip", "gba", gbaExts).error, "damaged archive");
	spit(t + "/cut.7z", sz.substr(0, sz.size() / 2));
	EQ(hash(t + "/cut.7z", "gba", gbaExts).error, "damaged archive");
	std::string bad = sz;
	bad[32 + 40] ^= 0x55;
	spit(t + "/bad.7z", bad);
	EQ(hash(t + "/bad.7z", "gba", gbaExts).error, "damaged archive");
	spit(t + "/not.7z", std::string(4096, 'x'));
	EQ(hash(t + "/not.7z", "gba", gbaExts).error, "damaged archive");
	spit(t + "/not.zip", std::string(4096, 'x'));
	EQ(hash(t + "/not.zip", "gba", gbaExts).error, "damaged archive");
	// over 64 MB unpacked, refused on what the archive says before anything is unpacked: big.7z's header, and a .zip's
	// directory made to say 65 MB; a .zip whose game is longer than its directory says is damaged
	EQ(hash(raDir + "/big.7z", "gba", gbaExts).error, "over 64 MB unpacked");
	size_t cd = zip.find(std::string("PK\x01\x02", 4));
	CHECK(cd != std::string::npos);
	auto sized = [&](uint32_t n) { std::string z = zip; for (int i = 0; i < 4; i++) z[cd + 24 + i] = (char)(n >> (8 * i)); return z; };
	spit(t + "/big.zip", sized(65u << 20));
	EQ(hash(t + "/big.zip", "gba", gbaExts).error, "over 64 MB unpacked");
	spit(t + "/short.zip", sized(100));
	EQ(hash(t + "/short.zip", "gba", gbaExts).error, "damaged archive");
	// a DS game in an archive is refused unread
	spit(t + "/d.zip", "not read");
	EQ(hash(t + "/d.zip", "nds", { "nds", "zip" }).error, "a DS game can't be identified from a .zip");
	EQ(hash(t + "/d.7z", "nds", { "nds", "7z" }).error, "a DS game can't be identified from a .7z");
	// what can't be identified says why, and nothing is read
	spit(t + "/p.pbp", "PBP");
	Hash h = hash(t + "/p.pbp", "psx");
	CHECK(h.hash.empty() && h.error.find(".pbp can't be identified") != std::string::npos);
	CHECK(hash(t + "/p.img", "psx").error.find(".img can't be identified") != std::string::npos);
	CHECK(hash(t + "/zero.gba", "ports").error == "no achievements for this system here");
	h = hash(t + "/missing.gba", "gba");
	CHECK(h.hash.empty() && !h.error.empty());
	IEQ((int)consoleOf("nds"), 18); IEQ((int)consoleOf("psx"), 12); IEQ((int)consoleOf("ports"), 0);
	if (system(("rm -rf " + t).c_str()) != 0) {}
}

int main(int argc, char** argv)
{
	dir = argc > 1 ? argv[1] : "kiwami/tests/tailscale";
	raDir = argc > 2 ? argv[2] : "kiwami/tests/ra";
	testJson();
	testStatus();
	testName();
	testQr();
	testDevices();
	testGrid();
	testRa();
	testRaCard();
	testPanelToken();
	testHash();
	if (failed) { fprintf(stderr, "units: %d of %d checks failed\n", failed, checks); return 1; }
	printf("units: %d checks passed\n", checks);
	return 0;
}
