// banana-ui (kiwami): Home server > BIOS, a paired RomM's (0039). The page asks banana-sync (gamesync::syncTool(), so
// BANANA_SYNC names a stand-in) for RomM's BIOS files of Banana's systems, kiwami bios list (romm/bios.h), when it
// opens. The bottom panel lists them under their systems, the missing ones ticked: A ticks or unticks one, Up and Down
// move, L and R a page, Start downloads the ticked ones (kiwami bios get <ids>) and then lists them again, B goes
// back. A tap on a file ticks it, the buttons at the bottom are B, Start and A. The top panel shows the selected file's
// system, size and state (the DS's three that DraStic reads say so), and the last download's result. Each answer is logged ("kiwami: romm bios: ..."), for the
// tests.
#include "kiwami.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <sstream>

namespace banana
{
	using rnds::Color;

	static const int BIOS_VIS = 8;	// rows on the bottom panel, Get games' size

	// the systems' names, as Settings' Home server systems page has them
	static std::string biosSystemName(const std::string& id)
	{
		static const struct { const char* id; const char* name; } NAMES[] = { { "nds", "Nintendo DS" },
			{ "gba", "Game Boy Advance" }, { "gbc", "Game Boy Color" }, { "gb", "Game Boy" }, { "nes", "NES" },
			{ "snes", "Super Nintendo" }, { "megadrive", "Mega Drive" }, { "c64", "Commodore 64" },
			{ "zxspectrum", "ZX Spectrum" }, { "psx", "PlayStation" }, { "n64", "Nintendo 64" },
			{ "gameandwatch", "Game & Watch" } };
		for (auto& n : NAMES) if (id == n.id) return n.name;
		return id;
	}

	static std::string biosSize(long long b)
	{
		if (b < 0) return "";
		char t[32];
		if (b < (1LL << 20)) snprintf(t, sizeof t, "%lld KB", (b + 512) >> 10);
		else snprintf(t, sizeof t, "%.1f MB", b / 1048576.0);
		return t;
	}

	// banana-sync's last line (its result), without the line's end
	static std::string lastLine(const std::string& out)
	{
		std::string s = out;
		while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) s.pop_back();
		size_t nl = s.rfind('\n');
		return nl == std::string::npos ? s : s.substr(nl + 1);
	}

	// an "error <why>" of banana-sync's, as the page says it
	static std::string biosWhy(const std::string& why, const char* what)
	{
		if (why == "scope") return "This pairing can't read BIOS files. Pair again (Server, A) to allow them.";
		if (why == "unpaired" || why == "not paired") return "RomM doesn't know this handheld now: pair again (Server, A).";
		return std::string(what) + ": " + why + ".";
	}

	void Settings::Kiwami::biosRun(BiosAsk what, const std::string& args)
	{
		biosAsk = what;
		biosJob.start(st.syncCmd(args) + " 2>/dev/null", st.sync);
	}

	Settings::Result Settings::Kiwami::biosOpen()
	{
		st.page = KIWAMI;
		// (opened again while its download or list still runs: that one's answer is the one waited for)
		if (biosJob.running()) return MOVED;
		bsel = btop = 0;
		biosDone.clear();
		bios.clear();
		if (!st.wifiOn())
		{
			biosNote = "Wi-Fi is off: switch it on in Network first.";
			fprintf(stderr, "banana-ui: kiwami: romm bios: wifi off\n");
			return MOVED;
		}
		biosNote.clear();
		biosRun(BiosAsk::LIST, "kiwami bios list");
		return MOVED;
	}

	void Settings::Kiwami::biosListed(const std::string& out)
	{
		std::string at = bsel >= 0 && bsel < (int)bios.size() ? bios[bsel].id : "";
		std::string last = lastLine(out);
		bios.clear();
		if (last.compare(0, 3, "ok ") != 0)
		{
			biosNote = last.compare(0, 6, "error ") == 0 ? biosWhy(last.substr(6), "The list didn't come")
				: "The list didn't come: banana-sync didn't answer.";
			fprintf(stderr, "banana-ui: kiwami: romm bios: not listed: %s\n", last.empty() ? "(nothing)" : last.c_str());
			bsel = btop = 0;
			return;
		}
		biosNote.clear();
		std::istringstream in(out);
		std::string ln;
		while (std::getline(in, ln))
		{
			std::vector<std::string> f;
			std::istringstream ls(ln);
			for (std::string c; std::getline(ls, c, '\t');) f.push_back(c);
			if (f.size() != 5) continue;
			BiosFile b;
			b.id = f[0]; b.system = f[1]; b.name = f[2]; b.state = f[4];
			b.bytes = atoll(f[3].c_str());
			b.on = b.state == "missing";
			bios.push_back(b);
		}
		// (each system's files together, the systems in the order RomM gave them: two of RomM's platforms can be one
		// system of Banana's)
		std::vector<std::string> order;
		for (auto& b : bios) if (std::find(order.begin(), order.end(), b.system) == order.end()) order.push_back(b.system);
		auto rank = [&](const std::string& s) { return std::find(order.begin(), order.end(), s) - order.begin(); };
		std::stable_sort(bios.begin(), bios.end(), [&](const BiosFile& a, const BiosFile& b) { return rank(a.system) < rank(b.system); });
		bsel = 0;
		for (int i = 0; i < (int)bios.size(); i++) if (bios[i].id == at) bsel = i;
		biosScroll();
		fprintf(stderr, "banana-ui: kiwami: romm bios: %d listed\n", (int)bios.size());
		for (auto& b : bios)
			fprintf(stderr, "banana-ui: kiwami: romm bios: file %s %s/%s %s%s\n", b.id.c_str(), b.system.c_str(), b.name.c_str(),
				b.state.c_str(), b.on ? ", ticked" : "");
	}

	bool Settings::Kiwami::biosUpdate()
	{
		bool changed = false;
		std::string out;
		// (a download's answer starts the list again: in the headless test it's done at once, and taken here too)
		while (biosJob.take(out))
		{
			BiosAsk was = biosAsk;
			biosAsk = BiosAsk::NONE;
			changed = true;
			if (was == BiosAsk::LIST) { biosListed(out); continue; }
			std::string last = lastLine(out);
			fprintf(stderr, "banana-ui: kiwami: romm bios: got: %s\n", last.empty() ? "(nothing)" : last.c_str());
			long long g = 0, s = 0, k = 0;
			if (last.compare(0, 3, "ok ") == 0 && sscanf(last.c_str() + 3, "%lld %lld %lld", &g, &s, &k) == 3)
			{
				std::vector<std::string> parts;
				if (g) parts.push_back(std::to_string(g) + " downloaded");
				if (s) parts.push_back(std::to_string(s) + " already there");
				if (k) parts.push_back(std::to_string(k) + " not written: a different file is there");
				biosDone.clear();
				for (auto& p : parts) biosDone += (biosDone.empty() ? "" : ", ") + p;
				biosDone = biosDone.empty() ? "Nothing downloaded." : biosDone + ".";
			}
			else biosDone = last.compare(0, 6, "error ") == 0 ? biosWhy(last.substr(6), "Not all came")
				: "Not all came: banana-sync didn't answer.";
			biosRun(BiosAsk::LIST, "kiwami bios list");
		}
		return changed && st.page == KIWAMI && pageOpen == Page::ROMM_BIOS;
	}

	std::vector<int> Settings::Kiwami::biosRows() const
	{
		std::vector<int> r;
		for (int i = 0; i < (int)bios.size(); i++)
		{
			if (i == 0 || bios[i].system != bios[i - 1].system) r.push_back(-1 - i);
			r.push_back(i);
		}
		return r;
	}

	void Settings::Kiwami::biosScroll()
	{
		std::vector<int> rows = biosRows();
		int n = (int)rows.size(), at = 0;
		for (int i = 0; i < n; i++) if (rows[i] == bsel) at = i;
		int first = at > 0 && rows[at - 1] < 0 ? at - 1 : at;	// (the first file of a system: its head shows too)
		if (first < btop) btop = first;
		if (at >= btop + BIOS_VIS) btop = at - BIOS_VIS + 1;
		btop = std::max(0, std::min(btop, n - BIOS_VIS));
	}

	Settings::Result Settings::Kiwami::biosPress(Key k)
	{
		int n = (int)bios.size();
		const bool getting = biosAsk == BiosAsk::GET;
		switch (k)
		{
		case Key::UP: case Key::DOWN:
			if (n < 2) return NONE;
			bsel = (bsel + (k == Key::UP ? -1 : 1) + n) % n;
			biosScroll();
			return MOVED;
		case Key::L: case Key::R:
			if (n < 2) return NONE;
			bsel = std::max(0, std::min(n - 1, bsel + (k == Key::L ? -BIOS_VIS : BIOS_VIS)));
			biosScroll();
			return MOVED;
		case Key::A:
			if (!n || getting) return NONE;
			bios[bsel].on = !bios[bsel].on;
			fprintf(stderr, "banana-ui: kiwami: romm bios: %s %s\n", bios[bsel].on ? "ticked" : "unticked", bios[bsel].name.c_str());
			return MOVED;
		case Key::START:
		{
			if (biosJob.running() || !n) return NONE;
			std::string ids;
			for (auto& b : bios) if (b.on) ids += " " + b.id;
			if (ids.empty()) { biosDone = "Nothing ticked: A ticks a file."; return MOVED; }
			if (!st.wifiOn()) { biosDone = "Wi-Fi is off: switch it on in Network first."; return MOVED; }
			biosDone.clear();
			fprintf(stderr, "banana-ui: kiwami: romm bios: get%s\n", ids.c_str());
			biosRun(BiosAsk::GET, "kiwami bios get" + ids);
			return MOVED;
		}
		case Key::B: st.page = LIST; return MOVED;
		default: return NONE;
		}
	}

	Settings::Result Settings::Kiwami::biosTouch(float cx, float cy)
	{
		float dx = st.downX, dy = st.downY;
		// the buttons: Back, Download, Tick (where biosRender() draws them)
		if (dy >= 418 && dy <= 466) return biosPress(dx < 213 ? Key::B : dx < 427 ? Key::START : Key::A);
		std::vector<int> rows = biosRows();
		int n = (int)rows.size();
		for (int r = 0; r < BIOS_VIS && btop + r < n; r++)
		{
			float ry = 80 + r * 41;
			if (dy < ry || dy > ry + 37 || rows[btop + r] < 0) continue;
			bsel = rows[btop + r];
			biosPress(Key::A);	// (selected, and ticked or unticked)
			return MOVED;
		}
		// a swipe up or down scrolls a page
		if (std::fabs(cy - dy) > 40 && n > BIOS_VIS)
		{
			btop = std::max(0, std::min(n - BIOS_VIS, btop + (cy < dy ? BIOS_VIS : -BIOS_VIS)));
			for (int r = 0; r < BIOS_VIS && btop + r < n; r++)
				if (rows[btop + r] >= 0) { bsel = rows[btop + r]; break; }
			return MOVED;
		}
		return NONE;
	}

	void Settings::Kiwami::biosRender()
	{
		Colours p = colours(st.light);
		int n = (int)bios.size();
		bsel = std::max(0, std::min(bsel, n - 1));
		int ticked = 0;
		for (auto& b : bios) ticked += b.on;
		auto stateWord = [](const std::string& s) {
			return s == "missing" ? "Missing" : s == "same" ? "On the card" : s == "different" ? "A different file is there, kept" : "";
		};

		// ---- top: the selected file, the last download's result ----
		st.be->beginScreen(0);
		st.box("screen", false, 0, 0, 640, 480);
		st.box("hero", false, 18, 20, 604, 436);
		st.icon("phone", 50, 56, 3);
		st.text("Home server", true, 17, 170, 62, p.muted, true, 1.4f);
		st.text("BIOS", true, 30, 170, 88, p.ink);
		std::string accent = biosAsk == BiosAsk::GET ? "Downloading..." : biosAsk == BiosAsk::LIST ? "Asking RomM..."
			: n ? std::to_string(n) + (n == 1 ? " file, " : " files, ") + std::to_string(ticked) + " ticked" : "";
		if (!accent.empty()) st.text(st.fit(accent, true, 22, 420), true, 22, 170, 130, p.accent);
		float y = 196;
		if (!biosNote.empty()) y = para(biosNote, y, 4, p.ink) + 14;
		else if (n)
		{
			const BiosFile& b = bios[bsel];
			st.text(st.fit(b.name, true, 24, 540), true, 24, 50, y, p.ink);
			std::string about = biosSystemName(b.system);
			if (b.bytes >= 0) about += " \xc2\xb7 " + biosSize(b.bytes);
			// (the three DraStic reads too, under its own names: romm/bios.h's drasticName)
			std::string lc = b.name;
			for (auto& ch : lc) ch = (char)tolower((unsigned char)ch);
			if (b.system == "nds" && (lc == "bios7.bin" || lc == "bios9.bin" || lc == "firmware.bin")) about += " \xc2\xb7 DraStic's too";
			st.text(st.fit(about, false, 20, 540), false, 20, 50, y + 38, p.muted);
			st.text(st.fit(stateWord(b.state) + std::string(b.on ? " \xc2\xb7 ticked" : ""), true, 20, 540), true, 20, 50, y + 68,
				b.state == "missing" ? p.accent : p.ink);
			y += 112;
		}
		if (!biosDone.empty()) y = para(biosDone, y, 3, p.ink) + 14;
		para("A ticks a file, Start downloads the ticked ones. A file already on the card is never written over.", y,
			std::max(1, (int)((440 - y) / 28)), p.muted);
		st.be->endScreen();

		// ---- bottom: the files under their systems ----
		rnds::Backend* be = st.be;
		be->beginScreen(1);
		st.box("screen", false, 0, 0, 640, 480);
		st.box("bubble", false, 18, 14, 604, 54);
		st.text("BIOS", true, 22, 38, 28, p.ink);
		if (n)
		{
			std::string count = std::to_string(bsel + 1) + " / " + std::to_string(n);
			float cw = st.text(count, true, 17, 0, 0, p.muted, false, 0, true);
			st.text(count, true, 17, 602 - cw, 31, p.muted);
		}
		if (!n && biosAsk == BiosAsk::LIST) st.text("Loading...", true, 20, 38, 226, p.muted);
		else if (!n) st.wrapped(!biosNote.empty() ? biosNote : "RomM has no BIOS files for Banana's systems.", 20, 38, 100, 564, 4, p.muted);
		std::vector<int> rows = biosRows();
		for (int r = 0; r < BIOS_VIS && btop + r < (int)rows.size(); r++)
		{
			int i = rows[btop + r];
			float ry = 80 + r * 41;
			if (i < 0)
			{
				st.text(st.fit(biosSystemName(bios[-1 - i].system), true, 17, 560, true, 1.4f), true, 17, 34, ry + 14, p.muted, true, 1.4f);
				continue;
			}
			const BiosFile& b = bios[i];
			bool sel = i == bsel;
			st.box(sel ? "pick" : "slot", sel, 18, ry, 604, 37);
			Color c = sel ? p.onPick : p.ink;
			std::string tag = b.state == "missing" ? "Missing" : b.state == "same" ? "On card" : "Different";
			float tw = st.text(tag, true, 17, 0, 0, c, false, 0, true);
			st.text(b.on ? "[x]" : "[ ]", true, 18, 34, ry + 9, b.on ? (sel ? p.onPickAccent : p.accent) : c);
			st.text(st.fit(b.name, true, 18, 520 - tw - 24), true, 18, 74, ry + 9, c);
			st.text(tag, true, 17, 606 - tw, ry + 10, sel ? p.onPick : b.state == "missing" ? p.accent : p.muted);
		}
		float bw = st.faceWidth("B", "Back");
		st.face("B", "Back", 18, bw);
		if (n && biosAsk != BiosAsk::GET)
		{
			float sw = st.faceWidth("START", "Download");
			st.face("START", "Download", 320 - sw / 2, sw);
			std::string a = bios[bsel].on ? "Untick" : "Tick";
			float aw = st.faceWidth("A", a);
			st.face("A", a, 622 - aw, aw);
		}
		be->endScreen();
	}
}
