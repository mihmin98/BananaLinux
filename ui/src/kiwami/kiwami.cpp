// banana-ui (kiwami): the fork's rows and pages in Settings (kiwami.h): the rows and the hooks' plumbing, each page
// hook going to the page that is open, and the groups' grid; the pages are in their own files (tailscale.cpp,
// ra-account.cpp, ra-stats.cpp).
#include "ra.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/wait.h>
#include <unistd.h>

namespace banana
{
	// the handheld has an IPv4 address on a network of its own (Wi-Fi, a USB adapter): not the loopback, not
	// Tailscale's; KIWAMI_NETWORK=0 or 1 says so instead (the headless test, whatever the machine running it has)
	static bool hasNetwork()
	{
		if (const char* e = getenv("KIWAMI_NETWORK")) return strcmp(e, "0") != 0;
		ifaddrs* all = nullptr;
		if (getifaddrs(&all) != 0) return false;
		bool found = false;
		for (ifaddrs* a = all; a && !found; a = a->ifa_next)
			found = a->ifa_addr && a->ifa_addr->sa_family == AF_INET && (a->ifa_flags & IFF_UP) && !(a->ifa_flags & IFF_LOOPBACK)
				&& strcmp(a->ifa_name, "tailscale0") != 0;
		freeifaddrs(all);
		return found;
	}

	// a path for the shell, in single quotes
	static std::string quoted(const std::string& s)
	{
		std::string q = "'";
		for (char c : s) q += c == '\'' ? std::string("'\\''") : std::string(1, c);
		return q + "'";
	}

	// the libretro cores' folder: KIWAMI_LIBRETRO in the environment (the headless test's stand-in), else the image's
	static std::string libretro()
	{
		const char* e = getenv("KIWAMI_LIBRETRO");
		return e && *e ? e : "/usr/lib/libretro";
	}

	// RetroAchievements > Mode's key: upstream's Other systems > RetroAchievements row's, in retroarch.cfg
	static const char* const MODE = "cheevos_hardcore_mode_enable";

	// Other systems: the fork's second cores, a row per system shown when the core is in the image. Upstream's
	// core.<system> keys (run-retroarch reads them) as upstream's Game Boy Advance row has, so upstream loads, saves
	// and resets them as any choice row; upstream's core first, its default
	static const struct { const char* key; const char* label; const char* help; const char* names[2]; const char* values[2]; } CORES[] = {
		{ "core.nes", "NES", "The emulator for NES games. FCEUmm: light and fast. Mesen: the most accurate, and heavier. "
			"Each keeps its own saves and save states: switching back finds them again.", { "FCEUmm", "Mesen" }, { "fceumm", "mesen" } },
		{ "core.gb", "Game Boy", "The emulator for Game Boy games. Gambatte: light and accurate. SameBoy: the most accurate, "
			"and heavier. Each keeps its own saves and save states: switching back finds them again.", { "Gambatte", "SameBoy" }, { "gambatte", "sameboy" } },
		{ "core.gbc", "Game Boy Color", "The emulator for Game Boy Color games. Gambatte: light and accurate. SameBoy: the most "
			"accurate, and heavier. Each keeps its own saves and save states: switching back finds them again.", { "Gambatte", "SameBoy" }, { "gambatte", "sameboy" } },
		{ "core.megadrive", "Mega Drive", "The emulator for Mega Drive games. PicoDrive: the fastest. Genesis Plus GX: more "
			"accurate, with the better sound, and heavier. Each keeps its own saves and save states: switching back finds them "
			"again.", { "PicoDrive", "Genesis Plus GX" }, { "picodrive", "genesis_plus_gx" } },
	};

	void Settings::Kiwami::rows(Settings& s, const char* group)
	{
		if (!s.kw) s.kw = std::make_shared<Kiwami>(s);	// (at the first build(), in Settings' constructor)
		std::string keys;
		if (strcmp(group, "Other systems") == 0)
			for (auto& c : CORES)
			{
				if (access((libretro() + "/" + c.values[1] + "_libretro.so").c_str(), F_OK) != 0) continue;
				Item it;
				it.section = group; it.icon = "gamepad"; it.label = c.label; it.help = c.help;
				it.file = s.data + "/settings.conf"; it.key = c.key;
				it.names = { c.names[0], c.names[1] }; it.values = { c.values[0], c.values[1] };
				s.items.push_back(it);
				keys += std::string(keys.empty() ? "" : " ") + c.key;
			}
		else if (strcmp(group, "Network") == 0)
		{
			Item it;
			it.section = group; it.icon = "phone"; it.label = "Tailscale"; it.key = "kiwami.tailscale"; it.kind = Item::PAGE;
			it.help = "The handheld in your tailnet, Tailscale's private network of your devices: reachable from them wherever "
				"it is, and reaching them. A opens Tailscale's page: on or off, the login (a QR code to scan with your phone), "
				"the devices.";
			s.items.push_back(it);
			keys = it.key;
		}
		else if (strcmp(group, "RetroAchievements") == 0 && !(getenv("KIWAMI_RA_GROUP") && strcmp(getenv("KIWAMI_RA_GROUP"), "0") == 0))
		{
			Item it;
			it.section = group; it.icon = "kiwami.trophy"; it.label = "Account"; it.key = "kiwami.ra.account"; it.kind = Item::PAGE;
			it.help = "The RetroAchievements account your games unlock achievements on. A opens it: sign in, sign out, or "
				"switch to an account used here before.";
			s.items.push_back(it);
			Item stats;
			stats.section = group; stats.icon = "kiwami.trophy"; stats.label = "Stats"; stats.key = "kiwami.ra.stats"; stats.kind = Item::PAGE;
			stats.help = "The account's points, and its progress on each of Banana's systems: games started, completed and "
				"mastered, achievements unlocked. Fetched when the page opens, and kept on the card for when there's no Wi-Fi.";
			s.items.push_back(stats);
			// upstream's Other systems > RetroAchievements, the same setting (load and save are upstream's)
			Item m;
			m.section = group; m.icon = "gamepad"; m.label = "Mode"; m.key = MODE;
			m.help = "Softcore: achievements unlock with save states, rewind and cheats still there. Hardcore: those are locked "
				"for the game, and the unlocks count as hardcore on retroachievements.org. The same setting as Other systems > "
				"RetroAchievements; RetroArch's own menu changes it too.";
			m.file = "/data/retroarch/retroarch.cfg";
			m.names = { "Softcore", "Hardcore" }; m.values = { "false", "true" };
			s.items.push_back(m);
			keys = it.key + " " + stats.key + " " + m.key;
		}
		// (the tests read it, and on the device it says which of the fork's cores the image has)
		fprintf(stderr, "banana-ui: kiwami: rows in %s: %s\n", group, keys.empty() ? "none" : keys.c_str());
	}

	std::string Settings::Kiwami::panelToken(const std::string& user, const std::string& data)
	{
		if (user.empty()) return "";	// (achievements off)
		std::string t = kiwami::ra::panelToken(kiwami::ra::Card(data), user);
		fprintf(stderr, "banana-ui: ra: game panel: %s\n", t.empty() ? "no token in accounts.conf either" : "the token from accounts.conf");
		return t;
	}

	// ---- the hooks, to the page that is open (or, for the rows' values in the background, to each page's part) ------
	void Settings::Kiwami::opened()
	{
		modeAt = -1;
		for (auto& it : st.items) if (it.key == MODE) modeAt = it.at;	// (both rows as just read, from the one file)
		tsOpened();
		raOpened();
	}

	Settings::Result Settings::Kiwami::open(const std::string& key)
	{
		if (key == "kiwami.tailscale") { pageOpen = Page::TAILSCALE; return tsOpen(); }
		if (key == "kiwami.ra.account") { pageOpen = Page::RA_ACCOUNT; return raOpen(); }
		if (key == "kiwami.ra.stats") { pageOpen = Page::RA_STATS; return raStatsOpen(); }
		return NONE;
	}

	Settings::Result Settings::Kiwami::press(Key k)
	{
		return pageOpen == Page::TAILSCALE ? tsPress(k) : pageOpen == Page::RA_ACCOUNT ? raPress(k)
			: pageOpen == Page::RA_STATS ? raStatsPress(k) : NONE;
	}

	Settings::Result Settings::Kiwami::touch(float cx, float cy)
	{
		return pageOpen == Page::TAILSCALE ? tsTouch(cx, cy) : pageOpen == Page::RA_ACCOUNT ? raTouch(cx, cy)
			: pageOpen == Page::RA_STATS ? raStatsTouch(cx, cy) : NONE;
	}

	void Settings::Kiwami::render()
	{
		if (pageOpen == Page::TAILSCALE) tsRender();
		else if (pageOpen == Page::RA_ACCOUNT) raRender();
		else if (pageOpen == Page::RA_STATS) raStatsRender();
	}

	std::string Settings::Kiwami::entryHelp() const
	{
		return pageOpen == Page::TAILSCALE ? tsEntryHelp() : pageOpen == Page::RA_ACCOUNT ? raEntryHelp() : "";
	}

	Settings::Result Settings::Kiwami::entered(std::string& text)
	{
		return pageOpen == Page::TAILSCALE ? tsEntered(text) : pageOpen == Page::RA_ACCOUNT ? raEntered(text) : NONE;
	}

	bool Settings::Kiwami::update(double t)
	{
		bool mode = syncMode(), ra = raUpdate();
		return tsUpdate(t) || mode || ra;
	}

	int Settings::Kiwami::wake(double t, bool shown) const
	{
		int w = tsWake(t, shown && pageOpen == Page::TAILSCALE), r = raWake();
		return w < 0 ? r : r < 0 ? w : std::min(w, r);
	}

	// ---- the groups' grid: the first row shown follows the selection (grid.h); logged when it moves, for the tests ---
	int Settings::Kiwami::gridTop(int gsel, int n)
	{
		int r = kiwami::gridTop(row0, gsel, n);
		if (r != row0) fprintf(stderr, "banana-ui: kiwami: groups from row %d\n", r);
		row0 = r;
		return r;
	}

	// Mode and upstream's Other systems > RetroAchievements are two rows of one setting, each read when Settings opens:
	// a change to one is taken into the other at the next update (they're in different groups, never on screen together)
	bool Settings::Kiwami::syncMode()
	{
		if (!st.mOpen || modeAt < 0) return false;
		int rows = 0, at = modeAt;
		for (auto& it : st.items)
			if (it.key == MODE && ++rows && at == modeAt) at = it.at;
		if (rows < 2 || at == modeAt) return false;	// (one row: the group left out)
		modeAt = at;
		for (auto& it : st.items)
			if (it.key == MODE)
			{
				it.at = at;
				if (it.section == "RetroAchievements") fprintf(stderr, "banana-ui: kiwami: Mode %s in both rows\n", it.names[at].c_str());
			}
		return true;
	}

	// ---- Tailscale's part: the Network row's value, from the helper's status; its page is tailscale.cpp -------------
	std::string Settings::Kiwami::helper() const
	{
		const char* e = getenv("KIWAMI_TAILSCALE");
		std::string h = e && *e ? e : "/usr/libexec/kiwami/tailscale";
		return access(h.c_str(), X_OK) == 0 ? h : "";
	}

	bool Settings::Kiwami::tsOn() const
	{
		return getConfValue(st.data + "/settings.conf", "tailscale", "off") == "on";
	}

	void Settings::Kiwami::tsOpened()
	{
		// switched off (or no helper in this image): nothing is run, the row says Off
		tool = helper();
		on = tsOn();
		dropQr();
		if (!on || tool.empty())
		{
			ts = kiwami::TsStatus();
			show(tool.empty() ? "no helper" : "switched off", "Off");
			return;
		}
		if (!job.running()) job.start(quoted(tool) + " status", st.sync);
	}

	// the row's value, and a line in the log when the state changed (the tests read it)
	bool Settings::Kiwami::show(const std::string& state, const std::string& w)
	{
		std::string line = state + ": row \"" + w + "\"";
		if (line != logged) fprintf(stderr, "banana-ui: tailscale: %s\n", line.c_str());
		logged = line;
		bool changed = w != word;
		word = w;
		return changed;
	}

	bool Settings::Kiwami::tsUpdate(double t)
	{
		// Tailscale's page asks again every 2 s while it shows
		bool shown = st.page == KIWAMI && pageOpen == Page::TAILSCALE;
		if ((!shown || view != MENU) && qrTex) dropQr();	// (left another way than B or START: the keyboard, Devices)
		if (shown && on && !tool.empty() && t >= nextPoll && !job.running())
		{
			job.start(quoted(tool) + " status", st.sync);
			nextPoll = t + 2000;
		}
		std::string out;
		if (!job.take(out) || !on) return false;	// (an answer from before it was switched off: dropped)
		ts = kiwami::parseStatus(out);
		std::string state = "state " + (ts.backend.empty() ? std::string("unreadable") : ts.backend);
		if (!ts.self.ipv4.empty()) state += ", " + ts.self.ipv4;
		if (!ts.error.empty()) state += ", error \"" + ts.error + "\"";
		return show(state, kiwami::rowWord(ts, hasNetwork())) || shown;
	}

	int Settings::Kiwami::tsWake(double t, bool shown) const
	{
		if (job.running()) return 100;
		if (shown && on && !tool.empty()) return std::max(0, (int)(nextPoll - t));
		return -1;
	}

	std::string Settings::Kiwami::value(const std::string& key) const
	{
		return key == "kiwami.tailscale" ? word : key.compare(0, 10, "kiwami.ra.") == 0 ? raValue(key) : "";
	}

	// a program of the fork's, its own session; detached so that it outlives a game the menu starts next, waited for
	// in the headless test (so what it does is done before the next command)
	void Settings::Kiwami::run(const char* verb, const char* arg)
	{
		if (tool.empty()) return;
		fprintf(stderr, "banana-ui: tailscale: run %s%s%s\n", verb, arg ? " " : "", arg ? arg : "");
		pid_t p = fork();
		if (p == 0)
		{
			setsid();
			if (!st.sync && fork() != 0) _exit(0);
			execl(tool.c_str(), tool.c_str(), verb, arg, (char*)nullptr);
			_exit(127);
		}
		if (p > 0) waitpid(p, nullptr, 0);
	}
}
