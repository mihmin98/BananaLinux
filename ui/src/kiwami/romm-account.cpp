// banana-ui (kiwami): RomM's Account page, A on Settings > RomM > Account. The bottom panel has its rows in the
// Settings list's look: Server (upstream's keyboard; accounts.conf's romm_url), Pair (once there is a server), Log out
// (when paired; A twice). The top panel says the state, whichever row is selected, then that row's help: what the
// helper's check answered (asked when the page opens and after Pair, Log out or a new server), or while a pairing waits,
// its link's QR code and the code to type. Also the group's part of the hooks: its rows, the card read when Settings
// opens (no helper run, nothing asked of the server), the Account row's value, the helper's answers.
#include "romm.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <unistd.h>

namespace banana
{
	using rnds::Color;
	namespace rm = kiwami::romm;

	namespace
	{
		// a path for the shell, in single quotes
		std::string shq(const std::string& s)
		{
			std::string q = "'";
			for (char c : s) q += c == '\'' ? std::string("'\\''") : std::string(1, c);
			return q + "'";
		}

		bool starts(const std::string& s, const char* p) { return s.compare(0, strlen(p), p) == 0; }

		std::string firstLine(const std::string& path)
		{
			std::ifstream f(path);
			std::string ln;
			std::getline(f, ln);
			while (!ln.empty() && (ln.back() == '\r' || ln.back() == '\n' || ln.back() == ' ')) ln.pop_back();
			return ln;
		}

		// the card's own systems.conf (it replaces the image's) starts a synced system's games without romm-game: the
		// automatic sync can't run for this card
		bool unwrappedCard(const std::string& data)
		{
			std::ifstream f(data + "/systems.conf");
			if (!f) return false;
			static const char* const SYNCED[] = { "nds", "gba", "gbc", "gb", "nes", "snes", "megadrive", "psx", "n64" };
			std::string ln, sec;
			while (std::getline(f, ln))
			{
				size_t b = ln.find_first_not_of(" \t"), e = ln.find_last_not_of(" \t\r");
				if (b == std::string::npos) continue;
				ln = ln.substr(b, e - b + 1);
				if (ln[0] == '[' && ln.back() == ']') { sec = ln.substr(1, ln.size() - 2); continue; }
				if (ln.compare(0, 3, "run") || ln.find('=') == std::string::npos) continue;
				if (std::find_if(std::begin(SYNCED), std::end(SYNCED), [&](const char* s) { return sec == s; }) == std::end(SYNCED)) continue;
				if (ln.find("/usr/libexec/kiwami/romm-game ") == std::string::npos) return true;
			}
			return false;
		}

		// Sync around games' help, and what it says when this card's own systems.conf doesn't use the wrapper
		const char* const AUTOSYNC_HELP = "On: a game's save comes from RomM before the game when RomM's is newer, and goes "
			"back to RomM when the game ends, in the background. Off: only Sync now syncs. A save replaced here is kept as .bak "
			"beside it.";
		const char* const UNWRAPPED_HELP = " This card's own banana/systems.conf starts the games without RomM's wrapper, so "
			"this card syncs only with Sync now; banana/systems.conf.default has the wrapper on its run lines.";
	}

	// ---- the group's part of the hooks ---------------------------------------------------------------------------
	void Settings::Kiwami::rommRows(Settings& s)
	{
		std::string keys;
		if (!(getenv("KIWAMI_ROMM_GROUP") && strcmp(getenv("KIWAMI_ROMM_GROUP"), "0") == 0))
		{
			const char* group = "RomM";
			Item on;
			on.section = group; on.icon = "kiwami.romm"; on.label = "RomM"; on.key = "romm";
			on.help = "Your games' saves kept in step with your RomM server, and its games and BIOS files on the handheld. "
				"Off: nothing of RomM runs, and games start as they do without it.";
			on.file = s.data + "/settings.conf";
			on.names = { "Off", "On" }; on.values = { "off", "on" };
			s.items.push_back(on);
			Item acct;
			acct.section = group; acct.icon = "kiwami.romm"; acct.label = "Account"; acct.key = "kiwami.romm.account"; acct.kind = Item::PAGE;
			acct.help = "The RomM server the handheld syncs with, and its pairing. A opens it: the server's address, pairing "
				"by a QR code scanned with your phone, logging out.";
			s.items.push_back(acct);
			Item sync;
			sync.section = group; sync.icon = "gamepad"; sync.label = "Sync around games"; sync.key = "romm_autosync";
			sync.help = AUTOSYNC_HELP;
			sync.file = s.data + "/settings.conf";
			sync.names = { "On", "Off" }; sync.values = { "on", "off" };
			s.items.push_back(sync);
			keys = on.key + " " + acct.key + " " + sync.key;
		}
		fprintf(stderr, "banana-ui: kiwami: rows in RomM: %s\n", keys.empty() ? "none" : keys.c_str());
	}

	void Settings::Kiwami::rommOpened()
	{
		bool group = false;
		for (auto& it : st.items) group |= it.key == "kiwami.romm.account";
		if (!group) return;	// (KIWAMI_ROMM_GROUP=0: the card left alone)
		if (!romm) romm = std::make_shared<Romm>();
		const char* e = getenv("KIWAMI_ROMM");
		std::string h = e && *e ? e : "/usr/libexec/kiwami/romm";
		romm->helper = access(h.c_str(), X_OK) == 0 ? h : "";
		rommRead();
		romm->unwrapped = unwrappedCard(st.data);
		for (auto& it : st.items) if (it.key == "romm_autosync") it.help = std::string(AUTOSYNC_HELP) + (romm->unwrapped ? UNWRAPPED_HELP : "");
		const rm::Account& a = romm->account;
		fprintf(stderr, "banana-ui: romm: %s, %s%s\n", a.url.empty() ? "no server" : rm::host(a.url).c_str(),
			a.token.empty() ? "not paired" : "paired", romm->unwrapped ? ", the card's systems.conf without the wrapper" : "");
	}

	void Settings::Kiwami::rommRead()
	{
		romm->account = rm::account(romm->card);
	}

	std::string Settings::Kiwami::rommValue(const std::string& key) const
	{
		if (!romm || key != "kiwami.romm.account") return "";
		const rm::Account& a = romm->account;
		return a.url.empty() || a.token.empty() ? "Not paired" : rm::host(a.url);
	}

	void Settings::Kiwami::rommRun(const char* verb)
	{
		if (!romm || romm->helper.empty() || romm->job.running()) return;
		std::string out;
		if (romm->job.take(out)) rommTook(out);	// (the last run's answer, not taken in yet: not lost)
		if (romm->job.running()) return;
		fprintf(stderr, "banana-ui: romm: run %s\n", verb);
		romm->verb = verb;
		romm->job.start(shq(romm->helper) + " " + verb, st.sync);
	}

	bool Settings::Kiwami::rommTook(const std::string& out)
	{
		std::string line = out.substr(0, out.find('\n'));
		if (romm->verb == "check")
		{
			romm->check = line.empty() ? "down" : line;
			fprintf(stderr, "banana-ui: romm: check: %s\n", romm->check.c_str());
		}
		else if (romm->verb == "pair")
		{
			romm->pairing = line.empty() ? "pair-failed down" : line;
			if (starts(romm->pairing, "pairing "))
			{
				fprintf(stderr, "banana-ui: romm: pairing, code %s\n", romm->pairing.substr(8, romm->pairing.find('\t') - 8).c_str());
				romm->nextStatus = st.now + 500;
			}
			else fprintf(stderr, "banana-ui: romm: pairing failed: %s\n", romm->pairing.c_str() + strlen("pair-failed "));
		}
		else if (romm->verb == "logout")
		{
			fprintf(stderr, "banana-ui: romm: %s\n", line.empty() ? "logout-failed" : line.c_str());
			rommRead();
			romm->pairing.clear();
			romm->check.clear();
			romm->outArmed = false;
			if (!romm->account.url.empty()) rommRun("check");
		}
		return true;
	}

	// a pairing waiting: the status file the helper writes once it is approved, denied or expired
	bool Settings::Kiwami::rommPairStatus()
	{
		std::string line = firstLine(romm->card.status);
		if (line.empty() || line == romm->pairing || starts(line, "pairing ")) return false;
		romm->pairing = line;
		if (starts(line, "paired "))
		{
			fprintf(stderr, "banana-ui: romm: paired (RomM %s)\n", line.c_str() + 7);
			rommRead();
			romm->check.clear();
			rommRun("check");
		}
		else fprintf(stderr, "banana-ui: romm: pairing failed: %s\n", starts(line, "pair-failed ") ? line.c_str() + 12 : line.c_str());
		return true;
	}

	bool Settings::Kiwami::rommUpdate(double t)
	{
		if (!romm) return false;
		bool shown = st.page == KIWAMI && pageOpen == Page::ROMM_ACCOUNT;
		if (!shown && romm->qr) { romm->qr.reset(); romm->qrUrl.clear(); }
		bool changed = false;
		std::string out;
		if (romm->job.take(out)) changed = rommTook(out);
		if (shown && starts(romm->pairing, "pairing ") && t >= romm->nextStatus)
		{
			romm->nextStatus = t + 500;
			changed |= rommPairStatus();
		}
		return changed;
	}

	int Settings::Kiwami::rommWake(double t, bool shown) const
	{
		if (!romm) return -1;
		if (romm->job.running()) return 100;
		if (shown && starts(romm->pairing, "pairing ")) return std::max(0, (int)(romm->nextStatus - t));
		return -1;
	}

	// ---- the page ------------------------------------------------------------------------------------------------
	std::vector<std::string> Settings::Kiwami::rommAccountRows() const
	{
		std::vector<std::string> r = { "server" };
		if (!romm->account.url.empty()) r.push_back("pair");
		if (!romm->account.token.empty()) r.push_back("logout");
		return r;
	}

	Settings::Result Settings::Kiwami::rommAccountOpen()
	{
		if (!romm) return NONE;
		st.page = KIWAMI;
		romm->selKey = "server";
		romm->outArmed = false;
		rommRead();
		romm->check.clear();
		romm->pairing.clear();
		// a pairing still waiting from before the page was left
		std::string status = firstLine(romm->card.status);
		if (starts(status, "pairing ") && access((romm->card.run + "/romm-pair.pid").c_str(), F_OK) == 0)
		{
			romm->pairing = status;
			romm->nextStatus = st.now;
		}
		const rm::Account& a = romm->account;
		fprintf(stderr, "banana-ui: romm: account page: %s, %s\n", a.url.empty() ? "no server" : rm::host(a.url).c_str(),
			a.token.empty() ? "not paired" : "paired");
		if (!a.url.empty() && romm->pairing.empty()) rommRun("check");
		return MOVED;
	}

	Settings::Result Settings::Kiwami::rommChange(int dir)
	{
		const std::string& k = romm->selKey;
		if (k == "logout")
		{
			if (dir <= 0) { romm->outArmed = false; return MOVED; }
			if (!romm->outArmed) { romm->outArmed = true; return MOVED; }
			romm->outArmed = false;
			if (romm->job.running()) return NONE;
			rommRun("logout");
			return CHANGED;
		}
		if (dir <= 0) return NONE;
		if (k == "server") { st.startEntry(ENTRY_KIWAMI, "RomM", "Server", KIWAMI, romm->account.url); return MOVED; }
		if (k == "pair")
		{
			if (romm->helper.empty() || romm->job.running()) return NONE;
			romm->check.clear();
			romm->pairing.clear();
			rommRun("pair");
			return CHANGED;
		}
		return NONE;
	}

	Settings::Result Settings::Kiwami::rommAccountPress(Key k)
	{
		std::vector<std::string> rows = rommAccountRows();
		int n = (int)rows.size(), i = n - 1;	// (a row that went away: the last one)
		for (int r = 0; r < n; r++) if (rows[r] == romm->selKey) i = r;
		switch (k)
		{
		case Key::UP: case Key::DOWN:
			romm->selKey = rows[(i + (k == Key::UP ? -1 : 1) + n) % n];
			romm->outArmed = false;
			return MOVED;
		case Key::LEFT: romm->selKey = rows[i]; return rommChange(-1);
		case Key::RIGHT: case Key::A: romm->selKey = rows[i]; return rommChange(1);
		case Key::B: st.page = LIST; romm->outArmed = false; return MOVED;
		case Key::START: st.page = LIST; st.mOpen = false; return CLOSED;
		default: return NONE;
		}
	}

	Settings::Result Settings::Kiwami::rommAccountTouch(float, float)
	{
		// where the press went down, as upstream's pages: the chips, B on the left and A on the right, then the rows
		float dx = st.downX, dy = st.downY;
		if (dy >= 418 && dy <= 466) return rommAccountPress(dx < 320 ? Key::B : Key::A);
		std::vector<std::string> rows = rommAccountRows();
		for (int r = 0; r < (int)rows.size(); r++)
		{
			float ry = 82 + r * 64;
			if (dy < ry || dy > ry + 56) continue;
			if (rows[r] != romm->selKey) { romm->selKey = rows[r]; romm->outArmed = false; return MOVED; }
			return rommChange(1);
		}
		return NONE;
	}

	std::string Settings::Kiwami::rommEntryHelp() const
	{
		return "A types the key, B deletes, X capitals, L symbols (: / .), Y a space, START when it is typed. Or tap the keys. "
			"Your RomM server's address as a browser opens it, with its port: 192.168.1.10:8080, or its Tailscale address "
			"(100.x.y.z; a MagicDNS name doesn't resolve on the handheld). http:// is added when it has none. Kept in "
			"banana/accounts.conf on the card; another server than the one paired with forgets the pairing.";
	}

	Settings::Result Settings::Kiwami::rommEntered(std::string& text)
	{
		std::string url = rm::normalServer(text), error;
		rm::Account before = romm->account;
		// the token is the paired server's: another server mustn't be sent it
		bool other = !before.token.empty() && url != before.url;
		if (!rm::setServer(romm->card, text, error) || (other && !rm::forget(romm->card, error)))
		{
			fprintf(stderr, "banana-ui: romm: %s\n", error.c_str());
			st.message = error;
			return MOVED;
		}
		if (url.empty()) fprintf(stderr, "banana-ui: romm: server removed\n");
		else fprintf(stderr, "banana-ui: romm: server %s\n", url.c_str());
		if (other) fprintf(stderr, "banana-ui: romm: the pairing with %s forgotten (another server)\n", rm::host(before.url).c_str());
		rommRead();
		romm->check.clear();
		romm->pairing.clear();
		text.clear();
		st.message.clear();
		st.page = KIWAMI;
		if (!url.empty()) rommRun("check");
		return CHANGED;
	}

	// ---- drawing --------------------------------------------------------------------------------------------------
	// the top panel: RomM's logo, RomM, the title and a line in the accent, then the state (in ink) and the help (muted)
	void Settings::Kiwami::rommTop(const std::string& title, const std::string& accent, const std::string& state, const std::string& help)
	{
		Colours p = colours(st.light);
		st.be->beginScreen(0);
		st.box("screen", false, 0, 0, 640, 480);
		st.box("hero", false, 18, 20, 604, 436);
		st.icon("kiwami.romm", 50, 56, 3);
		st.text("RomM", true, 17, 170, 62, p.muted, true, 1.4f);
		st.text(st.fit(title, true, 30, 420), true, 30, 170, 88, p.ink);
		if (!accent.empty()) st.text(st.fit(accent, true, 22, 420), true, 22, 170, 130, p.accent);
		float y = para(state, 196, 7, p.ink);	// (the help gets what is left: a line at least)
		if (!state.empty()) y += 14;
		para(help, y, (int)((440 - y) / 28), p.muted);
		st.be->endScreen();
	}

	// the pairing link's QR code, made once a link: black modules on white, one pixel each
	bool Settings::Kiwami::rommQr(const std::string& url)
	{
		if (url == romm->qrUrl) return (bool)romm->qr;
		romm->qr.reset();
		romm->qrUrl = url;
		int side = 0;
		std::vector<bool> m = kiwami::loginQr(url, side);
		if (m.empty()) return false;
		rnds::Bitmap b;
		b.w = b.h = side;
		b.px.resize((size_t)side * side * 4);
		for (size_t i = 0; i < m.size(); i++)
		{
			uint8_t v = m[i] ? 0 : 255;
			b.px[i * 4] = b.px[i * 4 + 1] = b.px[i * 4 + 2] = v;
			b.px[i * 4 + 3] = 255;
		}
		romm->qr = st.be->upload(b, false);
		fprintf(stderr, "banana-ui: romm: pairing code %dx%d modules for %s\n", side, side, url.c_str());
		return (bool)romm->qr;
	}

	// the top panel while a pairing waits: the code as large as the panel allows on the left, as Tailscale's login
	void Settings::Kiwami::rommLogin(const std::string& title, const std::string& accent, const std::string& state, const std::string& help,
		const std::string& link, const std::string& foot)
	{
		Colours p = colours(st.light);
		rnds::Backend* be = st.be;
		be->beginScreen(0);
		st.box("screen", false, 0, 0, 640, 480);
		st.box("hero", false, 18, 20, 604, 436);
		int side = romm->qr->w, k = std::max(1, (int)std::floor(st.X(330) / side));
		be->draw(romm->qr, std::round(st.X(36)), std::round(st.X(34)), (float)side * k, (float)side * k, Color(1, 1, 1, 1));
		const float x = 384, w = 216;
		st.text("RomM", true, 17, x, 46, p.muted, true, 1.4f);
		st.text(st.fit(title, true, 30, w), true, 30, x, 72, p.ink);
		if (!accent.empty()) st.text(st.fit(accent, true, 22, w), true, 22, x, 114, p.accent);
		float y = para(state, 160, 7, p.ink, x, w);
		if (!state.empty()) y += 14;
		para(help, y, (int)((370 - y) / 28), p.muted, x, w);
		st.text(st.fit(link, false, 20, 556), false, 20, 50, 380, p.ink);
		st.text(st.fit(foot, false, 18, 556), false, 18, 50, 410, p.muted);
		be->endScreen();
	}

	void Settings::Kiwami::rommAccountRender()
	{
		Colours p = colours(st.light);
		std::vector<std::string> rows = rommAccountRows();
		if (std::find(rows.begin(), rows.end(), romm->selKey) == rows.end()) romm->selKey = rows.back();
		const rm::Account& a = romm->account;
		const std::string host = rm::host(a.url), &check = romm->check, &pairing = romm->pairing;
		bool paired = !a.token.empty();

		// ---- top: the state ----
		std::string title = paired ? "Paired" : "Not paired", accent = host, state, link, foot;
		bool qr = false;
		if (romm->helper.empty()) { title = "Not in this image"; state = "RomM's helper isn't in this image."; }
		else if (starts(pairing, "pairing "))
		{
			size_t tab = pairing.find('\t');
			std::string code = pairing.substr(8, tab == std::string::npos ? std::string::npos : tab - 8);
			link = tab == std::string::npos ? "" : pairing.substr(tab + 1);
			qr = rommQr(link);
			title = "Pair";
			accent = code;
			state = qr ? "Scan the code with your phone, log in to RomM and approve the handheld." :
				"Open " + link + " on your phone or a computer, log in to RomM and approve the handheld.";
			foot = "Or type the code " + code + " on RomM's pairing page. This page moves on once it is approved.";
		}
		else if (starts(pairing, "pair-failed "))
		{
			std::string why = pairing.substr(12);
			title = "Not paired";
			state = why == "denied" ? "The pairing was denied in RomM. Pair asks again." :
				why == "expired" ? "The code expired before it was approved. Pair gives a new one." :
				why == "noserver" ? "Type the server's address first." :
				starts(why, "old ") ? "RomM " + why.substr(4) + " is too old for this client: it needs 5.4.0 or newer." :
				why == "card" ? "The pairing couldn't be written to the card." :
				"No answer from " + a.url + ": is the server running, and the handheld on its network?";
		}
		else if (a.url.empty())
		{
			title = "No server";
			state = "Type your RomM server's address on Server, then Pair: a QR code to scan with your phone, approved in RomM.";
		}
		else if (check.empty()) state = romm->job.running() ? "Asking " + host + "..." : "";
		else if (starts(check, "ok ") || starts(check, "newer "))
		{
			std::string v = check.substr(check.find(' ') + 1);
			title = "Paired";
			state = "RomM " + v + " at " + a.url + ".";
			if (starts(check, "newer ")) state += " It is newer than 5.4, which this client was written for: syncing goes on, and "
				"banana/logs/romm.log notes it. If a save doesn't sync, Banana may need an update.";
			if (romm->unwrapped) state += " This card's own systems.conf starts the games without the wrapper: only Sync now syncs.";
		}
		else if (starts(check, "old "))
		{
			title = "Server too old";
			state = "RomM " + check.substr(4) + " at " + a.url + ": this client needs 5.4.0 or newer, and syncs nothing until "
				"the server is updated.";
		}
		else if (check == "unpaired") state = "RomM answers at " + a.url + ". Pair: a QR code to scan with your phone, approved in RomM.";
		else if (check == "revoked")
		{
			title = "Pair again";
			state = "RomM no longer accepts this handheld's pairing: the device was removed there, or its token revoked. Nothing "
				"on the card changes until it is paired again.";
		}
		else
		{
			title = "No server";
			state = "No answer from " + a.url + (paired ? ". The pairing stays, and syncing goes on once the server answers." :
				": is the server running, and the handheld on its network?");
		}

		const std::string& k = romm->selKey;
		std::string help;
		if (k == "server") help = "Your RomM server's address, as a browser opens it: 192.168.1.10:8080, or its Tailscale address. "
			"Kept in banana/accounts.conf on the card.";
		else if (k == "pair") help = "Pairs the handheld with your RomM account: scan the code, log in and approve it. Pairing "
			"again replaces the pairing.";
		else help = romm->outArmed ? "Press A again to log out." : "Removes the handheld from RomM's devices (when the server "
			"answers) and forgets the pairing here. The server's address stays. Press A twice.";
		if (!qr && romm->qr) { romm->qr.reset(); romm->qrUrl.clear(); }
		if (qr) rommLogin(title, accent, state, help, link, foot);
		else rommTop(title, accent, state, help);

		// ---- bottom: the rows ----
		rnds::Backend* be = st.be;
		be->beginScreen(1);
		st.box("screen", false, 0, 0, 640, 480);
		st.box("bubble", false, 18, 14, 604, 54);
		st.text("RomM", true, 22, 38, 28, p.ink);
		int at = 0;
		for (int r = 0; r < (int)rows.size(); r++) if (rows[r] == k) at = r;
		char count[32];
		snprintf(count, sizeof count, "%02d / %02d", at + 1, (int)rows.size());
		float cw = st.text(count, true, 17, 0, 0, p.muted, false, 0, true);
		st.text(count, true, 17, 602 - cw, 31, p.muted);
		for (int r = 0; r < (int)rows.size(); r++)
		{
			const std::string& key = rows[r];
			bool sel = r == at;
			float y = 82 + r * 64;
			st.box(sel ? "pick" : "slot", sel, 18, y, 604, 56);
			const char* label = key == "server" ? "Server" : key == "pair" ? "Pair" : "Log out";
			st.text(label, true, 20, 38, y + 16, sel ? p.onPick : p.muted);
			Color vc = sel ? (romm->outArmed && key == "logout" ? p.onPickAccent : p.onPick) : p.muted;
			std::string v = key == "server" ? (host.empty() ? "Not set" : st.fit(host, true, 19, 380)) :
				key == "pair" ? (starts(pairing, "pairing ") ? "Waiting" : paired ? "Paired" : "") :
				romm->outArmed ? "Press A again" : "";
			if (v.empty()) continue;
			float vw = st.text(v, true, 19, 0, 0, p.ink, false, 0, true);
			st.text(v, true, 19, 586 - vw, y + 17, vc);
		}
		float bw = st.faceWidth("B", "Back");
		st.face("B", "Back", 18, bw);
		std::string act = k == "server" ? "Change" : k == "pair" ? "Pair" : romm->outArmed ? "Confirm" : "Log out";
		float aw = st.faceWidth("A", act);
		st.face("A", act, 622 - aw, aw);
		be->endScreen();
	}
}
