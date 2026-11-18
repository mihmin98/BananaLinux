// banana-ui (kiwami): the fork's rows and pages in Settings (kiwami.h): the rows and the hooks' plumbing, each page
// hook going to the page that is open, and the groups' grid; the pages are in their own files (tailscale.cpp,
// ra-account.cpp, ra-stats.cpp, ra-games.cpp, ra-game.cpp, romm-bios.cpp), and Show sync times' line in sync-times.cpp.
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

	void kiwami::moreCores(const std::string& system, const std::vector<std::pair<const char*, const char*>>& upstream,
		std::vector<std::string>& names, std::vector<std::string>& values)
	{
		auto has = [&](const std::string& v) { return std::find(values.begin(), values.end(), v) != values.end(); };
		const char* e = getenv("KIWAMI_LIBRETRO");
		if (e && *e)
		{
			std::vector<std::string> n, v;
			for (auto& c : upstream)
				if (has(c.first) || access((std::string(e) + "/" + c.first + "_libretro.so").c_str(), F_OK) == 0) { n.push_back(c.second); v.push_back(c.first); }
			names = n; values = v;
		}
		if ((system == "gb" || system == "gbc") && !has("sameboy"))
			for (const char* d : { "/usr/lib/libretro", "/data/retroarch/cores", e && *e ? e : "" })
				if (*d && access((std::string(d) + "/sameboy_libretro.so").c_str(), F_OK) == 0) { names.push_back("SameBoy"); values.push_back("sameboy"); break; }
	}

	// RetroAchievements > Mode's key: upstream's Other systems > RetroAchievements row's, in retroarch.cfg
	static const char* const MODE = "cheevos_hardcore_mode_enable";
	static const char* const INSTALLS = "kiwami_romm_installs";

	void Settings::Kiwami::rows(Settings& s, const char* group)
	{
		if (!s.kw) s.kw = std::make_shared<Kiwami>(s);	// (at the first build(), in Settings' constructor)
		std::string keys;
		if (strcmp(group, "Network") == 0)
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
			it.section = group; it.icon = "kiwami.ra"; it.label = "Account"; it.key = "kiwami.ra.account"; it.kind = Item::PAGE;
			it.help = "The RetroAchievements account your games unlock achievements on. A opens it: sign in, sign out, or "
				"switch to an account used here before.";
			s.items.push_back(it);
			Item stats;
			stats.section = group; stats.icon = "kiwami.ra"; stats.label = "Stats"; stats.key = "kiwami.ra.stats"; stats.kind = Item::PAGE;
			stats.help = "The account's points, and its progress on each of Banana's systems: games started, completed and "
				"mastered, achievements unlocked. Fetched when the page opens, and kept on the card for when there's no Wi-Fi.";
			s.items.push_back(stats);
			Item games;
			games.section = group; games.icon = "cart"; games.label = "Games on this card"; games.key = "kiwami.ra.games"; games.kind = Item::PAGE;
			games.help = "The games on this card with achievements, as far as the emulators have told Banana: unlocked of "
				"the total for each.";
			s.items.push_back(games);
			// upstream's Other systems > RetroAchievements, the same setting (load and save are upstream's)
			Item m;
			m.section = group; m.icon = "gamepad"; m.label = "Mode"; m.key = MODE;
			m.help = "Softcore: achievements unlock with save states, rewind and cheats still there. Hardcore: those are locked "
				"for the game, and the unlocks count as hardcore on retroachievements.org. The same setting as Other systems > "
				"RetroAchievements; RetroArch's own menu changes it too.";
			m.file = "/data/retroarch/retroarch.cfg";
			m.names = { "Softcore", "Hardcore" }; m.values = { "false", "true" };
			s.items.push_back(m);
			keys = it.key + " " + stats.key + " " + games.key + " " + m.key;
		}
		else if (strcmp(group, "Home server") == 0)
		{
			// Show sync times (sync-times.cpp), the first of the fork's rows at the page's end: Home server isn't in
			// build()'s ORDER, so its rows stay in the order they're added, and the hook's place (after Forget this server)
			// is theirs. Not after Sync now: upstream's tests reach the rows below it by counting Downs
			Item it;
			it.section = group; it.icon = "phone"; it.label = "Show sync times"; it.key = "kiwami_sync_times";
			it.help = "On: while a sync runs, the line under Sync now's bar shows how long the current step has taken, its "
				"time left once there's a rate to go on, and the whole sync's time. Each step's time also goes in the sync's "
				"report (banana/logs/sync.log).";
			it.file = s.data + "/settings.conf";
			it.names = { "Off", "On" }; it.values = { "off", "on" };
			s.items.push_back(it);
			keys = it.key;
			// Installs from RomM, a paired RomM 5.4 or newer's (the version the rework's save slots need, the same one
			// that has installs): banana-sync claims the games sent from RomM's page (romm/installs.h); a change is told
			// to RomM by installsUpdate()
			if (s.hsRowsFor == "romm" && s.hsSlotsKept())
			{
				Item in;
				in.section = group; in.icon = "phone"; in.label = "Installs from RomM"; in.key = INSTALLS;
				in.help = "Games you send to this handheld from RomM's web page come at the next Sync now, or when Wi-Fi "
					"connects. RomM keeps a request two days.";
				in.file = s.data + "/settings.conf";
				in.names = { "Off", "On" }; in.values = { "off", "on" };
				s.items.push_back(in);
				keys += " " + in.key;
			}
			// BIOS (romm-bios.cpp), a paired RomM's: RomM serves its firmware files (Romhost has none to give)
			if (s.hsRowsFor == "romm")
			{
				Item b;
				b.section = group; b.icon = "phone"; b.label = "BIOS"; b.key = "kiwami.romm.bios"; b.kind = Item::PAGE;
				b.help = "BIOS files from RomM for the DS, the Game Boy Advance and the PlayStation, into the card's bios folder.";
				s.items.push_back(b);
				keys += " " + b.key;
			}
		}
		else if (strcmp(group, "Second screen") == 0)
		{
			// the Game Boy Pokémon companion (ui/src/kiwami/gbpk/, kiwami/GB-POKEMON.md), right after upstream's Pokémon
			// companion: a row for each system, read at each game's start (game.cpp). Second screen isn't in build()'s
			// ORDER, so its rows stay where the hook adds them
			for (const char* sys : { "gb", "gbc" })
			{
				Item it;
				it.section = group; it.icon = "gamepad";
				it.label = strcmp(sys, "gb") == 0 ? "Pokémon: Game Boy" : "Pokémon: Game Boy Color";
				it.key = std::string("kiwami_pokemon_companion_") + sys;
				it.help = "The Pokémon games' pages on the second screen, as on the GBA: Red, Blue, Yellow, Gold, Silver and "
					"Crystal (English). A game follows the row of the system it is started from. Based on PokeDaisy by Lidor "
					"Itzhari.";
				it.file = s.data + "/settings.conf";
				it.names = { "Off", "On" }; it.values = { "off", "on" };
				it.hints = { "No companion.", "Battle, party, a guide, the map and more. Needs Second screen on. Challenge mode applies to it too." };
				s.items.push_back(it);
				keys += (keys.empty() ? "" : " ") + it.key;
			}
			// Status on the game panel (panel-status.cpp), right after upstream's Game panel idle time (else last), read
			// at each game's start
			Item it;
			it.section = group; it.icon = "picture";
			it.label = "Status on the game panel";
			it.key = "kiwami_panel_status";
			it.help = "The time and date, the battery, Wi-Fi and Tailscale, and how long the game has been played, along the "
				"bottom of the game panel while a game plays.";
			it.file = s.data + "/settings.conf";
			it.names = { "Off", "On" }; it.values = { "off", "on" };
			it.hints = { "The game panel's hint line, as before.", "A status strip instead of the hint line." };
			auto at = std::find_if(s.items.begin(), s.items.end(),
				[&](const Item& i) { return i.section == group && i.key == "bottom_idle_seconds"; });
			s.items.insert(at == s.items.end() ? at : at + 1, it);
			keys += (keys.empty() ? "" : " ") + it.key;
			// Power scaling (kiwami/rootfs/usr/libexec/kiwami/clocks, read by run-retroarch at each game's start), a row
			// of Other systems: that page has no hook of its own, and this call is inside upstream's block of it. build()
			// puts it after Frontend (place()'s FOLLOWS). Its own log line, so Second screen's stays as it was
			fprintf(stderr, "banana-ui: kiwami: rows in %s: %s\n", group, keys.c_str());
			Item ps;
			ps.section = "Other systems"; ps.icon = "gamepad"; ps.label = "Power scaling"; ps.key = "kiwami_power_scaling";
			ps.help = "Games in RetroArch and BananaFront run at the lowest CPU clock that keeps them at full speed, raised "
				"when frames are late and lowered when there's room; the GPU follows. Not the DS or N64.";
			ps.file = s.data + "/settings.conf";
			ps.names = { "Off", "On" }; ps.values = { "off", "on" };
			ps.hints = { "Each system's fixed clocks, as before.", "Clocks follow the game. The game panel's status shows them." };
			s.items.push_back(ps);
			group = "Other systems";
			keys = ps.key;
		}
		// (the tests read it)
		fprintf(stderr, "banana-ui: kiwami: rows in %s: %s\n", group, keys.empty() ? "none" : keys.c_str());
	}

	int Settings::Kiwami::place(const Order& order, const std::string& section, const std::string& name)
	{
		// the fork's tile and rows, each given the place of upstream's it follows (it comes after it: build() adds it
		// later, and a tie keeps that order)
		static const struct { const char* section; const char* name; const char* after; } FOLLOWS[] = {
			{ "", "RetroAchievements", "System" },	// the tiles
			{ "Network", "Tailscale", "Bluetooth" },
			{ "Other systems", "Power scaling", "Frontend" },
		};
		for (auto& f : FOLLOWS)
			if (section == f.section && name == f.name)
				for (auto& o : order)
					if (o.first == section)
						for (size_t i = 0; i < o.second.size(); i++) if (o.second[i] == f.after) return (int)i;
		return -1;
	}

	// ---- the hooks, to the page that is open (or, for the rows' values in the background, to each page's part) ------
	void Settings::Kiwami::opened()
	{
		modeAt = -1;
		for (auto& it : st.items) if (it.key == MODE) modeAt = it.at;	// (both rows as just read, from the one file)
		if (!installsJob.running()) installsAt = -1;	// (the installs row's value taken as it is at the next update)
		tsOpened();
		raOpened();
	}

	Settings::Result Settings::Kiwami::open(const std::string& key)
	{
		if (key == "kiwami.tailscale") { pageOpen = Page::TAILSCALE; return tsOpen(); }
		if (key == "kiwami.ra.account") { pageOpen = Page::RA_ACCOUNT; return raOpen(); }
		if (key == "kiwami.ra.stats") { pageOpen = Page::RA_STATS; return raStatsOpen(); }
		if (key == "kiwami.ra.games") { pageOpen = Page::RA_GAMES; return raGamesOpen(); }
		if (key == "kiwami.romm.bios") { pageOpen = Page::ROMM_BIOS; return biosOpen(); }
		return NONE;
	}

	Settings::Result Settings::Kiwami::press(Key k)
	{
		return pageOpen == Page::TAILSCALE ? tsPress(k) : pageOpen == Page::RA_ACCOUNT ? raPress(k)
			: pageOpen == Page::RA_STATS ? raStatsPress(k) : pageOpen == Page::RA_GAMES ? raGamesPress(k)
			: pageOpen == Page::RA_GAME ? raGamePress(k) : pageOpen == Page::ROMM_BIOS ? biosPress(k) : NONE;
	}

	Settings::Result Settings::Kiwami::touch(float cx, float cy)
	{
		return pageOpen == Page::TAILSCALE ? tsTouch(cx, cy) : pageOpen == Page::RA_ACCOUNT ? raTouch(cx, cy)
			: pageOpen == Page::RA_STATS ? raStatsTouch(cx, cy) : pageOpen == Page::RA_GAMES ? raGamesTouch(cx, cy)
			: pageOpen == Page::RA_GAME ? raGameTouch(cx, cy) : pageOpen == Page::ROMM_BIOS ? biosTouch(cx, cy) : NONE;
	}

	void Settings::Kiwami::render()
	{
		if (pageOpen == Page::TAILSCALE) tsRender();
		else if (pageOpen == Page::RA_ACCOUNT) raRender();
		else if (pageOpen == Page::RA_STATS) raStatsRender();
		else if (pageOpen == Page::RA_GAMES) raGamesRender();
		else if (pageOpen == Page::RA_GAME) raGameRender();
		else if (pageOpen == Page::ROMM_BIOS) biosRender();
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
		bool mode = syncMode(), ra = raUpdate(), times = timesUpdate(t), b = biosUpdate(), in = installsUpdate();
		return tsUpdate(t) || mode || ra || times || b || in;
	}

	int Settings::Kiwami::wake(double t, bool shown) const
	{
		int w = tsWake(t, shown && pageOpen == Page::TAILSCALE), r = raWake(), s = timesWake(t);
		w = w < 0 ? r : r < 0 ? w : std::min(w, r);
		if (biosJob.running() || installsJob.running()) w = w < 0 ? 100 : std::min(w, 100);	// (BIOS's banana-sync and the installs row's, their answers taken in)
		return w < 0 ? s : s < 0 ? w : std::min(w, s);
	}

	// ---- the groups' grid: the first row shown follows the selection (grid.h); logged when it moves, for the tests ---
	int Settings::Kiwami::gridTop(int gsel, int n)
	{
		int r = kiwami::gridTop(row0, gsel, n);
		if (r != row0) fprintf(stderr, "banana-ui: kiwami: groups from row %d\n", r);
		row0 = r;
		return r;
	}

	// Home server > Installs from RomM: upstream's Settings writes the row to settings.conf as for any other; a change
	// seen here is told to RomM (banana-sync's kiwami installs on|off: the device's remote_install capability, and with
	// on a check that the pairing can take installs). A failed On is put back to Off, so the row never says On for a
	// handheld RomM won't send to; a failed Off is kept (banana-sync stops claiming whatever RomM says, and RomM is told
	// at the next On). Each step is logged ("kiwami: romm installs: ..."), for the tests
	bool Settings::Kiwami::installsUpdate()
	{
		bool changed = false;
		std::string out;
		if (installsJob.take(out))
		{
			while (!out.empty() && (out.back() == '\n' || out.back() == '\r')) out.pop_back();
			std::string last = out.substr(out.rfind('\n') == std::string::npos ? 0 : out.rfind('\n') + 1);
			if (last == "ok")
			{
				fprintf(stderr, "banana-ui: kiwami: romm installs: %s: ok\n", installsOn ? "on" : "off");
				st.toast("Installs from RomM", installsOn ? "Games sent from RomM's page come at the next sync." : "RomM no longer offers this handheld.", 0x6ad07a, 1, 4000);
			}
			else installsFailed(last.compare(0, 6, "error ") == 0 ? last.substr(6) : "banana-sync didn't answer");
			changed = true;
		}
		if (installsJob.running()) return changed;
		const Item* it = st.itemByKey(INSTALLS);
		if (!it || it->at < 0 || it->at >= (int)it->values.size()) return changed;
		if (installsAt < 0 || it->at == installsAt) { installsAt = it->at; return changed; }
		installsAt = it->at;
		installsOn = it->values[it->at] == "on";
		if (!st.wifiOn()) { installsFailed("Wi-Fi is off"); return true; }
		fprintf(stderr, "banana-ui: kiwami: romm installs: %s asked\n", installsOn ? "on" : "off");
		installsJob.start(st.syncCmd(std::string("kiwami installs ") + (installsOn ? "on" : "off")) + " 2>/dev/null", st.sync);
		return changed;
	}

	void Settings::Kiwami::installsFailed(const std::string& why)
	{
		fprintf(stderr, "banana-ui: kiwami: romm installs: %s: %s%s\n", installsOn ? "on" : "off", why.c_str(), installsOn ? ", put back" : "");
		if (!installsOn)
		{
			st.toast("RomM couldn't be told", why + ". Its page may still offer this handheld; nothing sent is taken.", 0xf2c94c, 0, 7000);
			return;
		}
		for (auto& it : st.items)
			if (it.key == INSTALLS)
			{
				it.at = 0;
				st.save(it);
				installsAt = it.at;
			}
		st.toast("RomM didn't take it", why + ".", 0xff7a4a, 2, 7000);
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
