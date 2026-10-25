// banana-ui (kiwami): RomM's Sync now page, A on Settings > RomM > Sync now. The bottom panel has Sync now, then the
// games whose save changed on both sides (banana/romm/conflicts), in the Settings list's look. The top panel says, for
// Sync now, the sync's progress while it runs (the helper's status file, read every 0.5 s while the page shows) or the
// last one's result (banana/romm/last); for a conflict, the game in full and its choice: Keep this card's (romm resolve
// <n> local <rom>: sent over RomM's) or Take RomM's (server: put in place, the card's kept as .bak), LEFT or RIGHT, then
// A twice. The sync runs detached (the helper goes into the background once it holds the saves lock), so it goes on
// when the page is left. With RomM off, not paired or its pairing refused, the page says what to do instead.
#include "romm.h"
#include <algorithm>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <fstream>
#include <sys/file.h>
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

		// a helper holds the saves lock (a sync, a push after a game, a pull): a shared lock can't be had beside it
		bool savesLocked(const std::string& run)
		{
			int fd = open((run + "/romm-saves.lock").c_str(), O_RDONLY | O_CLOEXEC);
			if (fd < 0) return false;
			bool held = flock(fd, LOCK_SH | LOCK_NB) != 0;
			close(fd);	// (ours, if it was had, let go)
			return held;
		}

		// the synced systems' names, as the conflicts list shows them
		std::string systemName(const std::string& id)
		{
			static const struct { const char* id; const char* name; } NAMES[] = {
				{ "nds", "Nintendo DS" }, { "gba", "Game Boy Advance" }, { "gbc", "Game Boy Color" }, { "gb", "Game Boy" },
				{ "nes", "NES" }, { "snes", "Super Nintendo" }, { "megadrive", "Mega Drive" }, { "psx", "PlayStation" },
				{ "n64", "Nintendo 64" },
			};
			for (auto& n : NAMES) if (id == n.id) return n.name;
			return id;
		}

		// a game's name: its file's, without the folder and the extension
		std::string romTitle(const std::string& rom)
		{
			std::string n = rom.substr(rom.find_last_of('/') + 1);
			size_t dot = n.find_last_of('.');
			return dot == std::string::npos || dot == 0 ? n : n.substr(0, dot);
		}

		std::string when(long long t)
		{
			if (t <= 0) return "";
			time_t tt = (time_t)t;
			tm lt{};
			localtime_r(&tt, &lt);
			char b[40];
			strftime(b, sizeof b, "%d %b %Y, %H:%M", &lt);
			return b;
		}

		// RomM's time of a save (ISO 8601, UTC) as far as the minute
		std::string serverWhen(std::string t)
		{
			if (t.size() < 16) return t;
			t = t.substr(0, 16);
			if (t[10] == 'T') t[10] = ' ';
			return t + " UTC";
		}

		// the status line's progress: "running <done> <total>\t<system>\t<game>"
		struct Progress { int done = 0, total = 0; std::string system, game; };
		Progress progress(const std::string& line)
		{
			Progress p;
			sscanf(line.c_str(), "running %d %d", &p.done, &p.total);
			size_t t1 = line.find('\t');
			if (t1 == std::string::npos) return p;
			size_t t2 = line.find('\t', t1 + 1);
			p.system = line.substr(t1 + 1, t2 == std::string::npos ? std::string::npos : t2 - t1 - 1);
			if (t2 != std::string::npos) p.game = line.substr(t2 + 1);
			return p;
		}

		const int SHOWN = 5;	// the bottom panel's rows at once
	}

	// ---- the card: no helper run -----------------------------------------------------------------------------------
	void Settings::Kiwami::rommSyncRead()
	{
		Romm& r = *romm;
		r.last = rm::last(r.card);
		r.conflicts = rm::conflicts(r.card);
		std::string line = firstLine(r.card.status);
		r.syncLine = starts(line, "running ") || starts(line, "done sync ") || starts(line, "error sync ") ? line : "";
		bool running = starts(r.syncLine, "running ");
		r.syncing = running && savesLocked(r.card.run);
		r.stopped = running && !r.syncing;
		int rows = 1 + (int)r.conflicts.size();
		if (r.ssel >= rows) { r.ssel = rows - 1; r.asking = r.armed = false; }
		std::string w = rommSyncWord();
		if (w != r.word) fprintf(stderr, "banana-ui: romm: sync now: %s\n", w.c_str());
		r.word = w;
	}

	std::string Settings::Kiwami::rommSyncWord() const
	{
		const Romm& r = *romm;
		if (!rommOn()) return "Off";
		if (r.account.url.empty() || r.account.token.empty()) return "Not paired";
		if (r.syncing) return "Syncing";
		if (r.last.state == "revoked") return "Pair again";
		if (!r.conflicts.empty()) return r.conflicts.size() == 1 ? "1 conflict" : std::to_string(r.conflicts.size()) + " conflicts";
		if (r.last.state == "down") return "No server";
		if (r.last.state == "synced" || r.last.state == "conflicts") return "Synced";
		return "Not yet";
	}

	// the status file again while a sync runs; its end logged (the tests read it)
	bool Settings::Kiwami::rommSyncPoll()
	{
		std::string before = romm->syncLine;
		rommSyncRead();
		if (romm->syncLine == before && romm->syncing) return false;
		if (!romm->syncing)
			fprintf(stderr, "banana-ui: romm: sync: %s\n", romm->stopped ? "stopped without a word" : romm->syncLine.c_str());
		return true;
	}

	bool Settings::Kiwami::rommSyncTook(const std::string& out)
	{
		Romm& r = *romm;
		std::string line = out.substr(0, out.find('\n'));
		if (r.verb == "sync")
		{
			// "" once it went into the background (or ended, in the headless test), busy, or error sync unpaired
			if (line == "busy") r.note = "Another sync is running, or a game's save is still going up: try again in a moment.";
			else if (!line.empty()) r.note = "The sync didn't start: " + line + ".";
			if (!line.empty()) fprintf(stderr, "banana-ui: romm: sync: %s\n", line.c_str());
			rommSyncRead();
			if (line.empty() && !r.syncLine.empty() && !r.syncing)
				fprintf(stderr, "banana-ui: romm: sync: %s\n", r.stopped ? "stopped without a word" : r.syncLine.c_str());
			r.nextStatus = st.now + 500;
			return true;
		}
		if (line.empty()) line = "resolve-failed card";	// (the helper didn't answer)
		fprintf(stderr, "banana-ui: romm: %s\n", line.c_str());
		std::string why = starts(line, "resolve-failed ") ? line.substr(15) : "";
		const std::string host = rm::host(r.account.url);
		r.note = starts(line, "resolved ") ? (line.find(" local") != std::string::npos ? "This card's save was sent over RomM's." :
				"RomM's save is in place; this card's is kept beside it as .bak.") :
			line == "busy" ? "A sync is still running: try again once it ends." :
			why == "changed" ? "The conflicts changed meanwhile: the list is read again. Nothing was changed." :
			why == "down" ? "No answer from " + host + ": nothing was changed." :
			why == "revoked" ? "RomM no longer accepts this handheld's pairing: pair it again on Account." :
			starts(why, "old") ? "RomM at " + host + " is older than 5.4.0: nothing was changed." :
			why == "nosave" ? "There's no save of it on this card any more: Take RomM's instead." :
			why == "noserver" ? "RomM has no save of it now: Keep this card's instead." :
			why == "unpaired" ? "Pair the handheld on Account first." :
			"The save couldn't be written on the card. Nothing else was changed.";
		rommSyncRead();
		return true;
	}

	// ---- the page --------------------------------------------------------------------------------------------------
	Settings::Result Settings::Kiwami::rommSyncOpen()
	{
		if (!romm) return NONE;
		st.page = KIWAMI;
		Romm& r = *romm;
		r.ssel = r.stop = 0;
		r.asking = r.armed = false;
		r.note.clear();
		rommRead();
		r.nextStatus = st.now + 500;
		fprintf(stderr, "banana-ui: romm: sync page: %s, %d conflicts%s\n", r.word.c_str(), (int)r.conflicts.size(),
			r.syncing ? ", a sync running" : "");
		return MOVED;
	}

	Settings::Result Settings::Kiwami::rommSyncA()
	{
		Romm& r = *romm;
		const rm::Account& a = r.account;
		// what keeps it from starting, said on the page
		const char* why = r.helper.empty() ? "nohelper" : !rommOn() ? "off" : a.url.empty() || a.token.empty() ? "unpaired" :
			r.last.state == "revoked" ? "revoked" : r.syncing ? "running" : nullptr;
		if (why)
		{
			r.asking = r.armed = false;
			r.note = !strcmp(why, "nohelper") ? "RomM's helper isn't in this image." :
				!strcmp(why, "off") ? "RomM is off: switch it on, the group's first row." :
				!strcmp(why, "unpaired") ? "Pair the handheld with your RomM server on Account first." :
				!strcmp(why, "revoked") ? "RomM no longer accepts this handheld's pairing: pair it again on Account." :
				r.ssel == 0 ? "A sync is running: its progress is above." : "Wait for the sync to end, then resolve it.";
			fprintf(stderr, "banana-ui: romm: %s: %s\n", r.ssel == 0 ? "sync" : "resolve", !strcmp(why, "running") ?
				"a sync is running, nothing started" : ("not started (" + std::string(why) + ")").c_str());
			return MOVED;
		}
		if (r.job.running()) return NONE;
		if (r.ssel == 0)
		{
			r.note.clear();
			rommRun("sync");
			return CHANGED;
		}
		if (!r.asking) { r.asking = true; r.takeRomm = false; r.armed = false; return MOVED; }
		if (!r.armed) { r.armed = true; return MOVED; }
		const rm::Conflict& k = r.conflicts[r.ssel - 1];
		std::string n = std::to_string(r.ssel), side = r.takeRomm ? "server" : "local";
		r.asking = r.armed = false;
		r.note.clear();
		rommRun("resolve", n + " " + side + " " + shq(k.rom), n + " " + side + " " + k.rom);
		return CHANGED;
	}

	Settings::Result Settings::Kiwami::rommSyncPress(Key k)
	{
		Romm& r = *romm;
		int n = 1 + (int)r.conflicts.size();
		switch (k)
		{
		case Key::UP: case Key::DOWN:
			r.ssel = (r.ssel + (k == Key::UP ? -1 : 1) + n) % n;
			r.asking = r.armed = false;
			r.note.clear();
			return MOVED;
		case Key::LEFT: case Key::RIGHT:
			if (!r.asking) return NONE;
			r.takeRomm = k == Key::RIGHT;
			r.armed = false;
			return MOVED;
		case Key::A: return rommSyncA();
		case Key::B:
			if (r.asking) { r.asking = r.armed = false; return MOVED; }
			st.page = LIST;
			rommSyncRead();	// (the row's value as the page left it)
			return MOVED;
		case Key::START: st.page = LIST; st.mOpen = false; return CLOSED;
		default: return NONE;
		}
	}

	Settings::Result Settings::Kiwami::rommSyncTouch(float, float)
	{
		// where the press went down, as upstream's pages: the chips, B on the left and A on the right, then the rows
		Romm& r = *romm;
		float dx = st.downX, dy = st.downY;
		if (dy >= 418 && dy <= 466) return rommSyncPress(dx < 320 ? Key::B : Key::A);
		int n = 1 + (int)r.conflicts.size();
		for (int i = r.stop; i < n && i < r.stop + SHOWN; i++)
		{
			float ry = 82 + (i - r.stop) * 64;
			if (dy < ry || dy > ry + 56) continue;
			if (i != r.ssel) { r.ssel = i; r.asking = r.armed = false; r.note.clear(); return MOVED; }
			if (r.asking) return rommSyncPress(dx < 320 ? Key::LEFT : Key::RIGHT);	// (the choice's halves)
			return rommSyncA();
		}
		return NONE;
	}

	// ---- drawing -----------------------------------------------------------------------------------------------------
	void Settings::Kiwami::rommSyncRender()
	{
		Colours p = colours(st.light);
		Romm& r = *romm;
		const rm::Account& a = r.account;
		const std::string host = rm::host(a.url);
		int n = 1 + (int)r.conflicts.size();
		if (r.ssel >= n) r.ssel = n - 1;
		bool resolving = r.job.running() && r.verb == "resolve";

		// ---- top ----
		std::string title, accent, state, help;
		std::function<void()> bar;
		if (r.ssel == 0)
		{
			const rm::Last& l = r.last;
			if (r.helper.empty()) { title = "Not in this image"; state = "RomM's helper isn't in this image."; }
			else if (!rommOn()) { title = "RomM is off"; state = "Switch RomM on, the group's first row, to sync."; }
			else if (a.url.empty() || a.token.empty()) { title = "Not paired"; state = "Pair the handheld with your RomM server on Account first."; }
			else if (r.syncing || (r.job.running() && r.verb == "sync"))
			{
				Progress g = progress(r.syncLine);
				title = "Syncing";
				accent = g.total ? std::to_string(g.done) + " of " + std::to_string(g.total) : "Starting";
				help = "The sync goes on when the page is left; its result shows here and on the row.";
				bar = [this, g, p] {
					const float x = 50, y = 330, w = 540;
					st.be->fill(st.X(x), st.X(y), st.X(w), st.X(14), p.line);
					if (g.total > 0) st.be->fill(st.X(x + 2), st.X(y + 2), st.X((w - 4) * std::min(g.done, g.total) / g.total), st.X(10), p.accent);
					st.text(st.fit(g.system.empty() ? "Starting" : systemName(g.system), true, 17, w), true, 17, x, y + 26, p.ink);
					st.text(st.fit(g.game, false, 16, w), false, 16, x, y + 52, p.muted);
				};
			}
			else if (l.state == "revoked")
			{
				title = "Pair again";
				state = "RomM no longer accepts this handheld's pairing: the device was removed there, or its token revoked. Pair it "
					"again on Account; nothing on the card changes until then.";
			}
			else if (r.syncLine == "error sync old")
			{
				title = "Server too old";
				state = "RomM at " + host + " is older than 5.4.0: this client syncs nothing until the server is updated.";
			}
			else if (r.stopped)
			{
				title = "Stopped";
				state = "The last sync ended before it finished, without a word. Sync now starts it again; what it had done is kept.";
			}
			else if (l.state == "down")
			{
				title = "No server";
				accent = when(l.when);
				state = "No answer from " + a.url + " at the last sync: nothing was changed. Saves still to send wait for the next one.";
			}
			else if (l.state == "synced" || l.state == "conflicts")
			{
				title = "Synced";
				accent = when(l.when);
				state = std::to_string(l.sent) + " sent to RomM, " + std::to_string(l.fetched) + " taken from RomM, " +
					std::to_string(l.conflicts) + (l.conflicts == 1 ? " conflict, " : " conflicts, ") + std::to_string(l.skipped) +
					" not on RomM.";
				if (!r.conflicts.empty()) state += " The games changed on both sides are below: A on one resolves it.";
			}
			else
			{
				title = "Not synced yet";
				state = "Sync now sends this card's saves that are newer than RomM's, takes RomM's newer ones, and lists below the "
					"games changed on both sides, overwriting neither.";
			}
			if (r.unwrapped && !r.syncing) state += " This card's own systems.conf starts the games without RomM's wrapper: the "
				"automatic sync is off for this card, and only Sync now syncs it.";
			if (help.empty()) help = "A syncs every game of the nine synced systems on both cards. A save replaced here is kept as "
				".bak beside it.";
		}
		else
		{
			const rm::Conflict& k = r.conflicts[r.ssel - 1];
			title = romTitle(k.rom);
			accent = systemName(k.system);
			state = "Changed on this card and in RomM since the last sync: neither was overwritten. RomM's save: " +
				serverWhen(k.serverTime) + (k.noted ? ", noted " + when(k.noted) : "") + ".";
			help = resolving ? "Resolving..." :
				!r.asking ? "A chooses how to resolve it: Keep this card's (sent over RomM's) or Take RomM's (put in place, this "
					"card's kept as .bak)." :
				r.armed ? (r.takeRomm ? "Press A again to take RomM's save; this card's is kept as .bak." :
					"Press A again to send this card's save over RomM's.") :
				"LEFT or RIGHT chooses, A twice confirms, B leaves it as it is.";
		}
		if (!r.note.empty()) state = r.note + (state.empty() ? "" : " " + state);
		rommTop(title, accent, state, help, bar);

		// ---- bottom: Sync now, then the conflicts ----
		rnds::Backend* be = st.be;
		be->beginScreen(1);
		st.box("screen", false, 0, 0, 640, 480);
		st.box("bubble", false, 18, 14, 604, 54);
		st.text("Sync now", true, 22, 38, 28, p.ink);
		char count[32];
		snprintf(count, sizeof count, "%02d / %02d", r.ssel + 1, n);
		float cw = st.text(count, true, 17, 0, 0, p.muted, false, 0, true);
		st.text(count, true, 17, 602 - cw, 31, p.muted);
		if (r.ssel < r.stop) r.stop = r.ssel;
		if (r.ssel >= r.stop + SHOWN) r.stop = r.ssel - SHOWN + 1;
		for (int i = r.stop; i < n && i < r.stop + SHOWN; i++)
		{
			bool sel = i == r.ssel;
			float y = 82 + (i - r.stop) * 64;
			st.box(sel ? "pick" : "slot", sel, 18, y, 604, 56);
			std::string label = i == 0 ? "Sync now" : romTitle(r.conflicts[i - 1].rom), v;
			if (i == 0) v = r.syncing || (r.job.running() && r.verb == "sync") ? "Running" : rommSyncWord();
			else if (sel && resolving) v = "Resolving";
			else if (sel && r.asking) v = r.armed ? "Press A again" : r.takeRomm ? "< Take RomM's >" : "< Keep this card's >";
			else v = systemName(r.conflicts[i - 1].system);
			float vw = st.text(v, true, 19, 0, 0, p.ink, false, 0, true);
			st.text(st.fit(label, true, 20, 560 - 38 - vw - 24), true, 20, 38, y + 16, sel ? p.onPick : p.muted);
			Color vc = sel ? (r.armed ? p.onPickAccent : p.onPick) : p.muted;
			st.text(v, true, 19, 586 - vw, y + 17, vc);
		}
		float bw = st.faceWidth("B", r.asking ? "Cancel" : "Back");
		st.face("B", r.asking ? "Cancel" : "Back", 18, bw);
		std::string act = r.ssel == 0 ? "Sync" : !r.asking ? "Resolve" : r.armed ? "Confirm" : "Choose";
		float aw = st.faceWidth("A", act);
		st.face("A", act, 622 - aw, aw);
		be->endScreen();
	}
}
