// banana-ui (kiwami): RomM's BIOS page, A on Settings > RomM > BIOS. The bottom panel has one row, Fetch BIOS files; A
// on it runs romm bios (detached: it goes on when the page is left), which puts RomM's firmware for Banana's systems
// into the card's bios folder, a file there with the same contents skipped and one that differs kept as .bak. The top
// panel says its progress (the helper's status file, read every 0.5 s while the page shows), then what was fetched,
// skipped and failed, or why it didn't run. The state it reads is the downloads' (romm-games.cpp): a game's download
// and the BIOS share their lock, one at a time.
#include "romm.h"
#include <algorithm>
#include <cstring>

namespace banana
{
	namespace rm = kiwami::romm;

	namespace
	{
		bool starts(const std::string& s, const char* p) { return s.compare(0, strlen(p), p) == 0; }

		// the BIOS' running line: "running <done> <total>\tbios\t<file>"
		struct Progress { int done = 0, total = 0; std::string file; };
		Progress progress(const std::string& line)
		{
			Progress p;
			sscanf(line.c_str(), "running %d %d", &p.done, &p.total);
			size_t at = line.find("\tbios\t");
			if (at != std::string::npos) p.file = line.substr(at + 6);
			return p;
		}

		bool biosLine(const std::string& l) { return l.find("\tbios\t") != std::string::npos || starts(l, "done bios ") || starts(l, "error bios "); }
	}

	bool Settings::Kiwami::rommBiosTook(const std::string& out)
	{
		Romm& r = *romm;
		std::string first = out.substr(0, out.find('\n'));
		// "" once it went into the background (or ended, in the headless test), busy, or an error line (the status file
		// has it too)
		if (first == "busy")
		{
			r.biosNote = "A game is still downloading: the BIOS files can come once it's done.";
			fprintf(stderr, "banana-ui: romm: bios: busy\n");
			rommDownloadRead();
			return true;
		}
		rommDownloadRead();
		if (!r.downloading) rommDownloadEnded();
		r.nextStatus = st.now + 500;
		return true;
	}

	Settings::Result Settings::Kiwami::rommBiosOpen()
	{
		if (!romm) return NONE;
		st.page = KIWAMI;
		romm->biosNote.clear();
		rommRead();
		romm->nextStatus = st.now + 500;
		fprintf(stderr, "banana-ui: romm: bios page%s\n", romm->downloading ? (biosLine(romm->dlLine) ? ", fetching" : ", a download running") : "");
		return MOVED;
	}

	Settings::Result Settings::Kiwami::rommBiosPress(Key k)
	{
		Romm& r = *romm;
		switch (k)
		{
		case Key::A:
		{
			const rm::Account& a = r.account;
			const char* why = r.helper.empty() ? "nohelper" : !rommOn() ? "off" : a.url.empty() || a.token.empty() ? "unpaired" :
				r.last.state == "revoked" ? "revoked" : nullptr;
			if (!why)
			{
				rommDownloadRead();
				if (r.downloading) why = "running";
			}
			if (why)
			{
				r.biosNote = !strcmp(why, "nohelper") ? "RomM's helper isn't in this image." :
					!strcmp(why, "off") ? "RomM is off: switch it on, the group's first row." :
					!strcmp(why, "unpaired") ? "Pair the handheld with your RomM server on Account first." :
					!strcmp(why, "revoked") ? "RomM no longer accepts this handheld's pairing: pair it again on Account." :
					biosLine(r.dlLine) ? "The BIOS files are coming: their progress is above." :
					"A game is downloading: the BIOS files can come once it's done.";
				fprintf(stderr, "banana-ui: romm: bios: not started (%s)\n", why);
				return MOVED;
			}
			if (r.job.running()) return NONE;
			r.biosNote.clear();
			rommRun("bios");
			return CHANGED;
		}
		case Key::B:
			st.page = LIST;
			rommRead();	// (the rows' values as the page left them)
			return MOVED;
		case Key::START: st.page = LIST; st.mOpen = false; return CLOSED;
		default: return NONE;
		}
	}

	Settings::Result Settings::Kiwami::rommBiosTouch(float, float)
	{
		// where the press went down, as upstream's pages: the chips, B on the left and A on the right, then the row
		float dx = st.downX, dy = st.downY;
		if (dy >= 418 && dy <= 466) return rommBiosPress(dx < 320 ? Key::B : Key::A);
		if (dy >= 82 && dy <= 138) return rommBiosPress(Key::A);
		return NONE;
	}

	void Settings::Kiwami::rommBiosRender()
	{
		Colours p = colours(st.light);
		Romm& r = *romm;
		const std::string& l = biosLine(r.dlLine) ? r.dlLine : std::string();
		bool fetching = (r.downloading && !l.empty()) || (r.job.running() && r.verb == "bios");

		// ---- top ----
		std::string title, accent, state;
		std::function<void()> bar;
		if (r.helper.empty()) { title = "Not in this image"; state = "RomM's helper isn't in this image."; }
		else if (fetching)
		{
			Progress g = progress(l);
			title = "Fetching";
			accent = g.total ? std::to_string(g.done) + " of " + std::to_string(g.total) : "Starting";
			bar = [this, g, p] {
				const float x = 50, y = 330, w = 540;
				st.be->fill(st.X(x), st.X(y), st.X(w), st.X(14), p.line);
				if (g.total > 0) st.be->fill(st.X(x + 2), st.X(y + 2), st.X((w - 4) * std::min(g.done, g.total) / g.total), st.X(10), p.accent);
				st.text(st.fit(g.file, true, 17, w), true, 17, x, y + 26, p.ink);
			};
		}
		else if (starts(l, "done bios "))
		{
			int fetched = 0, skipped = 0, failed = 0;
			sscanf(l.c_str(), "done bios %d %d %d", &fetched, &skipped, &failed);
			title = "BIOS files";
			accent = std::to_string(fetched) + " fetched";
			state = std::to_string(fetched) + " fetched into the bios folder, " + std::to_string(skipped) + " already there" +
				(failed ? ", " + std::to_string(failed) + " failed (banana/logs/romm.log says why)." : ".");
		}
		else if (starts(l, "error bios "))
		{
			std::string e = l.substr(11);
			title = "Not fetched";
			state = e == "down" ? "No answer from " + r.account.url + ": what came is kept, and A goes on." :
				e == "revoked" ? "RomM no longer accepts this handheld's pairing: pair it again on Account." :
				e == "old" ? "RomM at " + rm::host(r.account.url) + " is older than 5.4.0: update the server first." :
				e == "unpaired" ? "Pair the handheld with your RomM server on Account first." :
				"The card's bios folder couldn't be written.";
		}
		else if (starts(l, "running ")) { title = "Stopped"; state = "The last fetch stopped before the end: A starts it again."; }
		else { title = "BIOS files"; state = "Not fetched yet."; }
		if (!r.biosNote.empty()) state = r.biosNote + (state.empty() ? "" : " " + state);
		rommTop(title, accent, state, "RomM's BIOS files for Banana's systems, into the card's bios folder, where the emulators "
			"look for them. A file already there with the same contents is skipped; one that differs is kept as .bak beside it.", bar);

		// ---- bottom: the one row ----
		rnds::Backend* be = st.be;
		be->beginScreen(1);
		st.box("screen", false, 0, 0, 640, 480);
		st.box("bubble", false, 18, 14, 604, 54);
		st.text("BIOS", true, 22, 38, 28, p.ink);
		st.box("pick", true, 18, 82, 604, 56);
		st.text("Fetch BIOS files", true, 20, 38, 98, p.onPick);
		std::string v = fetching ? "Fetching" : "";
		if (!v.empty())
		{
			float vw = st.text(v, true, 19, 0, 0, p.ink, false, 0, true);
			st.text(v, true, 19, 586 - vw, 99, p.onPick);
		}
		float bw = st.faceWidth("B", "Back");
		st.face("B", "Back", 18, bw);
		float aw = st.faceWidth("A", "Fetch");
		st.face("A", "Fetch", 622 - aw, aw);
		be->endScreen();
	}
}
