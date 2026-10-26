// banana-ui (kiwami): RomM's Games page, A on Settings > RomM > Games. The bottom panel lists, in the Settings list's
// look, RomM's platforms that are Banana's systems' (romm platforms, asked when the page opens), then A on one its
// games, 50 a page (romm roms: L or R, or the last row, turns the page; Y searches with upstream's keyboard), a game
// already on a card marked. The top panel has the selected one in full, and A on a game downloads it (romm download,
// detached: it goes on when the page is left): its progress, read from the helper's status file every 0.5 s while
// the page shows, then where it went, or why it didn't. Once a game is in place the library is read again, as after
// a scrape. Also the part both download pages share (the BIOS' is romm-bios.cpp): the status file's line when it is a
// download's or the BIOS', and whether one runs.
#include "romm.h"
#include <algorithm>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <sstream>
#include <sys/file.h>
#include <unistd.h>

namespace banana
{
	namespace rm = kiwami::romm;

	namespace
	{
		// a word for the shell, in single quotes
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

		// a helper holds the lock (the downloads' one: a game's or the BIOS'): a shared lock can't be had beside it
		bool locked(const std::string& path)
		{
			int fd = open(path.c_str(), O_RDONLY | O_CLOEXEC);
			if (fd < 0) return false;
			bool held = flock(fd, LOCK_SH | LOCK_NB) != 0;
			close(fd);
			return held;
		}

		std::vector<std::string> fields(const std::string& ln)
		{
			std::vector<std::string> f;
			size_t at = 0;
			while (true)
			{
				size_t t = ln.find('\t', at);
				f.push_back(ln.substr(at, t == std::string::npos ? std::string::npos : t - at));
				if (t == std::string::npos) return f;
				at = t + 1;
			}
		}

		std::string sizeText(long long b)
		{
			char s[32];
			if (b >= 1LL << 30) snprintf(s, sizeof s, "%.1f GB", b / 1073741824.0);
			else if (b >= 1 << 20) snprintf(s, sizeof s, "%.1f MB", b / 1048576.0);
			else snprintf(s, sizeof s, "%lld KB", (b + 1023) / 1024);
			return s;
		}

		// a download's line: "running <got> <total>\t<system>\t<name>\t<note>"
		struct Progress { long long got = 0, total = 0; std::string name, note; };
		Progress progress(const std::string& line)
		{
			Progress p;
			sscanf(line.c_str(), "running %lld %lld", &p.got, &p.total);
			std::vector<std::string> f = fields(line);
			if (f.size() > 2) p.name = f[2];
			if (f.size() > 3) p.note = f[3];
			return p;
		}

		bool biosLine(const std::string& l) { return l.find("\tbios\t") != std::string::npos || starts(l, "done bios ") || starts(l, "error bios "); }

		// what keeps the page from asking RomM (nullptr: nothing), and what the page says of it
		const char* notReady(bool helper, bool on, const rm::Account& a, const std::string& lastState)
		{
			return !helper ? "nohelper" : !on ? "off" : a.url.empty() || a.token.empty() ? "unpaired" : lastState == "revoked" ? "revoked" : nullptr;
		}

		std::string notReadyText(const char* why)
		{
			return !strcmp(why, "nohelper") ? "RomM's helper isn't in this image." :
				!strcmp(why, "off") ? "RomM is off: switch it on, the group's first row." :
				!strcmp(why, "unpaired") ? "Pair the handheld with your RomM server on Account first." :
				"RomM no longer accepts this handheld's pairing: pair it again on Account.";
		}

		std::string notReadyTitle(const char* why)
		{
			return !strcmp(why, "nohelper") ? "Not in this image" : !strcmp(why, "off") ? "RomM is off" : !strcmp(why, "unpaired") ?
				"Not paired" : "Pair again";
		}

		const int SHOWN = 5;	// the bottom panel's rows at once
		const long PAGE = 50;	// the games asked for at once (romm roms')
	}

	// ---- a download's or the BIOS' state: the card, no helper run ---------------------------------------------------
	void Settings::Kiwami::rommDownloadRead()
	{
		Romm& r = *romm;
		std::string line = firstLine(r.card.status);
		bool theirs = starts(line, "done download ") || starts(line, "error download ") || starts(line, "done bios ") || starts(line, "error bios ");
		if (starts(line, "running "))
		{
			std::vector<std::string> f = fields(line);
			theirs = f.size() == 4 || (f.size() == 3 && f[1] == "bios");
		}
		r.dlLine = theirs ? line : "";
		r.downloading = starts(r.dlLine, "running ") && locked(r.card.run + "/romm-download.lock");
	}

	bool Settings::Kiwami::rommDownloadPoll()
	{
		std::string before = romm->dlLine;
		rommDownloadRead();
		if (romm->downloading && romm->dlLine == before) return false;
		if (!romm->downloading) rommDownloadEnded();
		return true;
	}

	void Settings::Kiwami::rommDownloadEnded()
	{
		Romm& r = *romm;
		const std::string& l = r.dlLine;
		if (l.empty()) return;
		fprintf(stderr, "banana-ui: romm: %s\n", !starts(l, "running ") ? l.c_str() :
			biosLine(l) ? "bios stopped without a word" : "download stopped without a word");
		if (!starts(l, "done download ")) return;
		// (meant for the second card, and on this one: the helper's fallback, said on the page too)
		for (auto& it : st.items)
			if (it.key == "romm_target" && it.at >= 0 && it.at < (int)it.values.size() && it.values[it.at] == "sd2"
				&& starts(l.substr(l.find(' ', 14) + 1), (r.card.card + "/roms/").c_str()))
				fprintf(stderr, "banana-ui: romm: download: on this card, the second card isn't in\n");
		long id = atol(l.c_str() + 14);
		for (auto& g : r.games) if (g.id == id) g.on = true;
		st.libChanged = true;	// (the library read again at home, as after a scrape)
	}

	std::string Settings::Kiwami::rommDownloadWord(const char* what) const
	{
		if (!romm || !romm->downloading) return "";
		bool bios = biosLine(romm->dlLine);
		return !strcmp(what, "bios") ? (bios ? "Fetching" : "") : (bios ? "" : "Downloading");
	}

	// ---- the helper's answers ---------------------------------------------------------------------------------------
	bool Settings::Kiwami::rommGamesTook(const std::string& out)
	{
		Romm& r = *romm;
		std::istringstream in(out);
		std::string first, ln;
		std::getline(in, first);
		if (r.verb == "download")
		{
			// "" once it went into the background (or ended, in the headless test); busy; an error line (the status
			// file has it too)
			if (first == "busy")
			{
				r.gamesNote = "A download or the BIOS files are still coming: try again once they're done.";
				fprintf(stderr, "banana-ui: romm: download: busy\n");
				rommDownloadRead();
				return true;
			}
			rommDownloadRead();
			if (!r.downloading) rommDownloadEnded();
			r.nextStatus = st.now + 500;
			return true;
		}
		if (starts(first, "error ") || (r.verb == "roms" && !starts(first, "total ")))
		{
			r.listError = first.empty() ? "error " + r.verb + " down" : first;
			fprintf(stderr, "banana-ui: romm: %s\n", r.listError.c_str());
			return true;
		}
		r.listError.clear();
		if (r.verb == "platforms")
		{
			r.platforms.clear();
			std::istringstream all(out);
			for (std::string l; std::getline(all, l);)
			{
				std::vector<std::string> f = fields(l);
				if (f.size() < 5) continue;
				Romm::Platform p;
				p.id = atol(f[0].c_str()); p.slug = f[1]; p.system = f[2]; p.count = atol(f[3].c_str()); p.name = f[4];
				if (p.id) r.platforms.push_back(p);
			}
			r.listed = true;
			if (r.psel >= (int)r.platforms.size()) r.psel = 0;
			fprintf(stderr, "banana-ui: romm: platforms: %d\n", (int)r.platforms.size());
			return true;
		}
		r.total = atol(first.c_str() + 6);
		r.games.clear();
		while (std::getline(in, ln))
		{
			std::vector<std::string> f = fields(ln);
			if (f.size() < 5) continue;
			Romm::Game g;
			g.id = atol(f[0].c_str()); g.size = atoll(f[1].c_str()); g.on = f[2] == "1"; g.fsName = f[3]; g.name = f[4];
			if (g.id) r.games.push_back(g);
		}
		r.gsel = r.gtop = 0;
		fprintf(stderr, "banana-ui: romm: roms: %d from %ld of %ld\n", (int)r.games.size(), r.offset + 1, r.total);
		return true;
	}

	// ---- the page ---------------------------------------------------------------------------------------------------
	Settings::Result Settings::Kiwami::rommGamesOpen()
	{
		if (!romm) return NONE;
		st.page = KIWAMI;
		Romm& r = *romm;
		r.inPlatform = r.listed = false;
		r.platforms.clear();
		r.games.clear();
		r.psel = r.ptop = 0;
		r.listError.clear();
		r.gamesNote.clear();
		rommRead();
		r.nextStatus = st.now + 500;
		const char* why = notReady(!r.helper.empty(), rommOn(), r.account, r.last.state);
		fprintf(stderr, "banana-ui: romm: games page%s%s%s\n", why ? ": not listed (" : "", why ? why : "", why ? ")" : r.downloading ? ", a download running" : "");
		if (!why) rommRun("platforms");
		return MOVED;
	}

	void Settings::Kiwami::rommGamesList(long offset)
	{
		Romm& r = *romm;
		if (r.psel >= (int)r.platforms.size()) return;
		const Romm::Platform& p = r.platforms[r.psel];
		std::string head = std::to_string(p.id) + " " + std::to_string(offset < 0 ? 0 : offset);
		r.offset = offset < 0 ? 0 : offset;
		r.listError.clear();
		r.games.clear();
		r.gsel = r.gtop = 0;
		rommRun("roms", head + (r.search.empty() ? "" : " " + shq(r.search)), head + (r.search.empty() ? "" : " " + r.search));
	}

	Settings::Result Settings::Kiwami::rommGamesA()
	{
		Romm& r = *romm;
		if (const char* why = notReady(!r.helper.empty(), rommOn(), r.account, r.last.state))
		{
			r.gamesNote = notReadyText(why);
			return MOVED;
		}
		if (r.job.running()) return NONE;
		r.gamesNote.clear();
		if (!r.inPlatform)
		{
			if (r.platforms.empty()) { rommRun("platforms"); return CHANGED; }	// (again, after no answer)
			r.inPlatform = true;
			r.search.clear();
			r.total = 0;
			fprintf(stderr, "banana-ui: romm: games of %s\n", r.platforms[r.psel].name.c_str());
			rommGamesList(0);
			return CHANGED;
		}
		bool more = r.offset + PAGE < r.total;
		if (more && r.gsel == (int)r.games.size()) { rommGamesList(r.offset + PAGE); return CHANGED; }
		if (r.games.empty()) { rommGamesList(r.offset); return CHANGED; }	// (again, after no answer)
		rommDownloadRead();
		if (r.downloading)
		{
			r.gamesNote = biosLine(r.dlLine) ? "The BIOS files are still coming: the game can follow once they're done." :
				"A download is running: its progress is above. The next one can start once it's done.";
			fprintf(stderr, "banana-ui: romm: download: one is running, nothing started\n");
			return MOVED;
		}
		const Romm::Game& g = r.games[r.gsel];
		rommRun("download", std::to_string(g.id), std::to_string(g.id) + " " + g.fsName);
		return CHANGED;
	}

	Settings::Result Settings::Kiwami::rommGamesPress(Key k)
	{
		Romm& r = *romm;
		bool more = r.offset + PAGE < r.total;
		int n = r.inPlatform ? (int)r.games.size() + (more ? 1 : 0) : (int)r.platforms.size();
		int& sel = r.inPlatform ? r.gsel : r.psel;
		switch (k)
		{
		case Key::UP: case Key::DOWN:
			if (n == 0) return NONE;
			sel = (sel + (k == Key::UP ? -1 : 1) + n) % n;
			r.gamesNote.clear();
			return MOVED;
		case Key::L: case Key::R:
			if (!r.inPlatform || r.job.running()) return NONE;
			if (k == Key::R && more) { rommGamesList(r.offset + PAGE); return CHANGED; }
			if (k == Key::L && r.offset > 0) { rommGamesList(r.offset - PAGE); return CHANGED; }
			return NONE;
		case Key::Y:
			if (!r.inPlatform || r.job.running()) return NONE;
			st.startEntry(ENTRY_KIWAMI, "RomM", "Search " + r.platforms[r.psel].name, KIWAMI, r.search);
			return MOVED;
		case Key::A: return rommGamesA();
		case Key::B:
			r.gamesNote.clear();
			if (r.inPlatform) { r.inPlatform = false; r.listError.clear(); return MOVED; }
			st.page = LIST;
			rommRead();	// (the rows' values as the page left them)
			return MOVED;
		case Key::START: st.page = LIST; st.mOpen = false; return CLOSED;
		default: return NONE;
		}
	}

	Settings::Result Settings::Kiwami::rommGamesTouch(float, float)
	{
		// where the press went down, as upstream's pages: the chips, B on the left and A on the right, then the rows
		Romm& r = *romm;
		float dx = st.downX, dy = st.downY;
		if (dy >= 418 && dy <= 466) return rommGamesPress(dx < 320 ? Key::B : Key::A);
		bool more = r.offset + PAGE < r.total;
		int n = r.inPlatform ? (int)r.games.size() + (more ? 1 : 0) : (int)r.platforms.size();
		int& sel = r.inPlatform ? r.gsel : r.psel;
		int top = r.inPlatform ? r.gtop : r.ptop;
		for (int i = top; i < n && i < top + SHOWN; i++)
		{
			float ry = 82 + (i - top) * 64;
			if (dy < ry || dy > ry + 56) continue;
			if (i != sel) { sel = i; r.gamesNote.clear(); return MOVED; }
			return rommGamesA();
		}
		return NONE;
	}

	std::string Settings::Kiwami::rommGamesEntryHelp() const
	{
		return "A types the key, B deletes, X capitals, Y a space, START when it is typed. Or tap the keys. Words of the game's "
			"name, as RomM's search takes them; nothing typed lists all of the platform's games.";
	}

	Settings::Result Settings::Kiwami::rommGamesEntered(std::string& text)
	{
		Romm& r = *romm;
		size_t b = text.find_first_not_of(' '), e = text.find_last_not_of(' ');
		r.search = b == std::string::npos ? "" : text.substr(b, e - b + 1);
		text.clear();
		st.page = KIWAMI;
		fprintf(stderr, "banana-ui: romm: games: search \"%s\"\n", r.search.c_str());
		rommGamesList(0);
		return CHANGED;
	}

	// ---- drawing -----------------------------------------------------------------------------------------------------
	void Settings::Kiwami::rommGamesRender()
	{
		Colours p = colours(st.light);
		Romm& r = *romm;
		const std::string host = rm::host(r.account.url);
		bool more = r.offset + PAGE < r.total;
		int n = r.inPlatform ? (int)r.games.size() + (more ? 1 : 0) : (int)r.platforms.size();
		int& sel = r.inPlatform ? r.gsel : r.psel;
		int& top = r.inPlatform ? r.gtop : r.ptop;
		if (sel >= n) sel = std::max(0, n - 1);
		bool asking = r.job.running() && (r.verb == "platforms" || r.verb == "roms");
		bool toSd2 = false;
		for (auto& it : st.items) if (it.key == "romm_target") toSd2 = it.at >= 0 && it.at < (int)it.values.size() && it.values[it.at] == "sd2";

		// ---- top ----
		std::string title, accent, state, help;
		std::function<void()> bar;
		const char* why = notReady(!r.helper.empty(), rommOn(), r.account, r.last.state);
		if (why) { title = notReadyTitle(why); state = notReadyText(why); }
		else if (!r.inPlatform)
		{
			title = "Games";
			accent = host;
			if (asking) state = "Asking " + host + "...";
			else if (!r.listError.empty()) state = r.listError == "error platforms revoked" ? notReadyText("revoked") :
				r.listError == "error platforms old" ? "RomM at " + host + " is older than 5.4.0: update the server first." :
				"No answer from " + r.account.url + ": is the server running, and the handheld on its network? A asks again.";
			else if (r.platforms.empty()) state = r.listed ? "RomM has no games for Banana's systems." : "";
			else
			{
				const Romm::Platform& pl = r.platforms[sel];
				title = pl.name;
				state = std::to_string(pl.count) + (pl.count == 1 ? " game" : " games") + " in RomM.";
			}
			help = "RomM's platforms that are Banana's systems'. A lists a platform's games; a game downloaded goes into its "
				"system's folder on " + std::string(toSd2 ? "the second card" : "this card") + ".";
		}
		else
		{
			const Romm::Platform& pl = r.platforms[r.psel];
			if (asking) { title = pl.name; state = "Asking " + host + "..."; }
			else if (!r.listError.empty())
			{
				title = pl.name;
				state = "No answer from " + r.account.url + ": A asks again.";
			}
			else if (r.games.empty()) { title = pl.name; state = r.search.empty() ? "RomM has no games on this platform." : "No game's name has \"" + r.search + "\"."; }
			else if (sel == (int)r.games.size())
			{
				title = "Next page";
				accent = pl.name;
				state = "Games " + std::to_string(r.offset + PAGE + 1) + " to " + std::to_string(std::min(r.total, r.offset + 2 * PAGE)) + " of " +
					std::to_string(r.total) + ".";
			}
			else
			{
				const Romm::Game& g = r.games[sel];
				title = g.name.empty() ? g.fsName : g.name;
				accent = pl.name + ", " + sizeText(g.size);
				state = g.fsName + (g.on ? ": on a card already." : ".");
			}
			help = "A downloads the game to " + std::string(toSd2 ? "the second card" : "this card") + ". L and R turn the pages, Y "
				"searches" + (r.search.empty() ? "" : " (now: \"" + r.search + "\")") + ", B goes back to the platforms.";
		}
		// a download's progress, or how the last one ended, before the rest
		const std::string& l = r.dlLine;
		if (!why && !biosLine(l) && !l.empty())
		{
			std::string dl;
			if (r.downloading || (r.job.running() && r.verb == "download"))
			{
				Progress g = progress(l);
				title = "Downloading";
				accent = g.total > 0 ? sizeText(g.got) + " of " + sizeText(g.total) : "Starting";
				state = g.note.empty() ? "" : "To this card: the second card isn't in.";
				help = "The download goes on when the page is left; the library lists the game once it is whole. If it stops (the "
					"Wi-Fi, sleep), A on the game again goes on from where it stopped.";
				bar = [this, g, p] {
					const float x = 50, y = 330, w = 540;
					st.be->fill(st.X(x), st.X(y), st.X(w), st.X(14), p.line);
					if (g.total > 0) st.be->fill(st.X(x + 2), st.X(y + 2), st.X((w - 4) * (float)std::min(g.got, g.total) / g.total), st.X(10), p.accent);
					st.text(st.fit(g.name, true, 17, w), true, 17, x, y + 26, p.ink);
				};
			}
			else if (starts(l, "done download "))
			{
				std::string path = l.substr(l.find(' ', 14) + 1);
				bool fellBack = toSd2 && starts(path, (r.card.card + "/roms/").c_str());
				dl = "Downloaded to " + path + (fellBack ? ", on this card: the second card isn't in." : ".");
			}
			else if (starts(l, "error download "))
			{
				std::string e = l.substr(15);
				dl = e == "down" ? "The download got no answer from " + host + ": nothing was kept." :
					e == "cut" ? "The download stopped before the end: A on the game again goes on from where it stopped." :
					e == "revoked" ? notReadyText("revoked") :
					e == "old" ? "RomM at " + host + " is older than 5.4.0: nothing was downloaded." :
					e == "notfound" ? "RomM no longer has that game." :
					e == "system" ? "That game's platform isn't one of Banana's systems." :
					e == "name" ? "RomM's name for that game can't go on the card." :
					starts(e, "there ") ? "It's on the card already: " + e.substr(6) + ". Nothing was downloaded." :
					starts(e, "space ") ? "Not enough room on the card: nothing was written (" + e.substr(6) + " bytes needed and free)." :
					starts(e, "unpack ") ? "Its files couldn't be unpacked; the zip is kept as " + e.substr(7) + "." :
					e == "size" ? "It came with another size than RomM gave: dropped. A tries again." :
					"The card couldn't be written: nothing was kept.";
			}
			else if (starts(l, "running ")) dl = "The last download stopped before the end: A on the game again goes on from where it stopped.";
			if (!dl.empty()) state = dl + (state.empty() ? "" : " " + state);
		}
		if (!r.gamesNote.empty()) state = r.gamesNote + (state.empty() ? "" : " " + state);
		rommTop(title, accent, state, help, bar);

		// ---- bottom: the platforms, or a platform's games ----
		rnds::Backend* be = st.be;
		be->beginScreen(1);
		st.box("screen", false, 0, 0, 640, 480);
		st.box("bubble", false, 18, 14, 604, 54);
		std::string head = r.inPlatform ? r.platforms[r.psel].name : "Games";
		char count[48];
		if (r.inPlatform) snprintf(count, sizeof count, "%02ld / %02ld", r.games.empty() ? 0 : std::min(r.offset + sel + 1, r.total), r.total);
		else snprintf(count, sizeof count, "%02d / %02d", n ? sel + 1 : 0, n);
		float cw = st.text(count, true, 17, 0, 0, p.muted, false, 0, true);
		st.text(st.fit(head, true, 22, 560 - cw - 40), true, 22, 38, 28, p.ink);
		st.text(count, true, 17, 602 - cw, 31, p.muted);
		if (sel < top) top = sel;
		if (sel >= top + SHOWN) top = sel - SHOWN + 1;
		for (int i = top; i < n && i < top + SHOWN; i++)
		{
			bool s = i == sel;
			float y = 82 + (i - top) * 64;
			st.box(s ? "pick" : "slot", s, 18, y, 604, 56);
			std::string label, v;
			if (!r.inPlatform) { label = r.platforms[i].name; v = std::to_string(r.platforms[i].count); }
			else if (i == (int)r.games.size()) { label = "Next page"; v = std::to_string(r.offset + PAGE + 1) + "-" + std::to_string(std::min(r.total, r.offset + 2 * PAGE)); }
			else { const Romm::Game& g = r.games[i]; label = g.name.empty() ? g.fsName : g.name; v = g.on ? "On a card" : sizeText(g.size); }
			float vw = st.text(v, true, 19, 0, 0, p.ink, false, 0, true);
			st.text(st.fit(label, true, 20, 560 - 38 - vw - 24), true, 20, 38, y + 16, s ? p.onPick : p.muted);
			st.text(v, true, 19, 586 - vw, y + 17, s ? p.onPick : p.muted);
		}
		float bw = st.faceWidth("B", "Back");
		st.face("B", "Back", 18, bw);
		std::string act = !r.inPlatform ? "Open" : more && sel == (int)r.games.size() ? "Next" : "Download";
		float aw = st.faceWidth("A", act);
		st.face("A", act, 622 - aw, aw);
		be->endScreen();
	}
}
