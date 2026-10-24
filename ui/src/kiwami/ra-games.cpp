// banana-ui (kiwami): RetroAchievements' games on this card, A on Settings > RetroAchievements > Games on this card.
// The games come from the menu's library (kiwami/library.h, set by main.cpp's kiwami: line): every game of the nine
// systems' shelves (the second card's too) that an emulator recorded achievements for (play stats' unlocked and total:
// BananaDS and SuperDrastic after each game, RetroArch through the game panel, with Second screen on), under the title
// the menu shows. The bottom panel lists them, the last played first, with unlocked / total and a bar; the top one
// shows the selected game. Nothing is asked of the server here: A on a game opens its achievements (ra-game.cpp).
#include "library.h"
#include "ra.h"
#include "../library.h"
#include <algorithm>
#include <cmath>

namespace banana
{
	namespace kiwami { Library* library = nullptr; }
	using rnds::Color;
	namespace ra = kiwami::ra;

	// the theme's icon of each system (Mega Drive's is md)
	static std::string systemIcon(const std::string& system)
	{
		return system == "megadrive" ? "md" : system;
	}

	void Settings::Kiwami::raGamesRead()
	{
		ra->games.clear();
		Library* lib = kiwami::library;
		if (!lib) return;
		for (int i = 0; i < lib->systemCount(); i++)
		{
			const Shelf& sh = lib->shelf(i);
			if (sh.favorites || sh.recent || sh.search) continue;	// (collections: copies of the systems' games)
			const ra::Console* con = nullptr;
			for (auto& c : ra::CONSOLES) if (sh.def.id == c.system) con = &c;
			if (!con) continue;
			for (auto& g : sh.games)
			{
				if (g.achB <= 0) continue;
				Ra::Game r;
				r.title = g.title; r.system = sh.def.id; r.systemName = con->name; r.path = g.path; r.rel = g.rel;
				r.exts = sh.def.exts;
				r.unlocked = std::max(0, g.achA); r.total = g.achB; r.last = g.last;
				ra->games.push_back(r);
			}
		}
		std::stable_sort(ra->games.begin(), ra->games.end(), [](const Ra::Game& a, const Ra::Game& b) { return a.last > b.last; });
	}

	Settings::Result Settings::Kiwami::raGamesOpen()
	{
		if (!ra) return NONE;
		st.page = KIWAMI;
		raGamesRead();
		ra->gsel = ra->gtop = 0;
		fprintf(stderr, "banana-ui: ra: games on this card: %d\n", (int)ra->games.size());
		for (auto& g : ra->games)
			fprintf(stderr, "banana-ui: ra: game %s/%s \"%s\" %d / %d\n", g.system.c_str(), g.rel.c_str(), g.title.c_str(), g.unlocked, g.total);
		return MOVED;
	}

	Settings::Result Settings::Kiwami::raGamesPress(Key k)
	{
		int n = (int)ra->games.size();
		switch (k)
		{
		case Key::UP: case Key::DOWN:
			if (n < 2) return NONE;
			ra->gsel = (ra->gsel + (k == Key::UP ? -1 : 1) + n) % n;
			if (ra->gsel < ra->gtop) ra->gtop = ra->gsel;
			if (ra->gsel >= ra->gtop + ROWS) ra->gtop = ra->gsel - ROWS + 1;
			return MOVED;
		case Key::A: return n ? raGameOpen(ra->gsel) : NONE;
		case Key::B: st.page = LIST; return MOVED;
		case Key::START: st.page = LIST; st.mOpen = false; return CLOSED;
		default: return NONE;
		}
	}

	Settings::Result Settings::Kiwami::raGamesTouch(float, float cy)
	{
		float dx = st.downX, dy = st.downY;
		int n = (int)ra->games.size();
		if (dy >= 418 && dy <= 466) return raGamesPress(dx < 320 ? Key::B : Key::A);
		for (int r = 0; r < ROWS && ra->gtop + r < n; r++)
		{
			float ry = 82 + r * 64;
			if (dy < ry || dy > ry + 56) continue;
			if (ra->gtop + r == ra->gsel) return raGameOpen(ra->gsel);	// (a second tap opens it)
			ra->gsel = ra->gtop + r;
			return MOVED;
		}
		// a swipe up or down scrolls a page
		if (std::fabs(cy - dy) > 40 && n > ROWS)
		{
			ra->gtop = std::max(0, std::min(n - ROWS, ra->gtop + (cy < dy ? ROWS : -ROWS)));
			ra->gsel = std::max(ra->gtop, std::min(ra->gsel, ra->gtop + ROWS - 1));
			return MOVED;
		}
		return NONE;
	}

	void Settings::Kiwami::raGamesRender()
	{
		Colours p = colours(st.light);
		int n = (int)ra->games.size();
		ra->gsel = std::max(0, std::min(ra->gsel, n - 1));
		const std::string help = "The games Banana has seen achievements for. RetroArch games appear after one played with "
			"Settings > Other systems > Second screen on.";

		// ---- top: the selected game ----
		if (n == 0) raTop("Games on this card", "", "No games with achievements yet: a game shows here once its emulator "
			"has said how many it has unlocked.", help, "");
		else
		{
			const Ra::Game& g = ra->games[ra->gsel];
			std::string file = g.rel.substr(g.rel.rfind('/') == std::string::npos ? 0 : g.rel.rfind('/') + 1);
			std::string where = g.rel.compare(0, 5, "@sd2/") == 0 ? ", on the second card" : "";
			raTop(g.title, std::to_string(g.unlocked) + " / " + std::to_string(g.total) + " achievements",
				g.systemName + ": " + file + where + ". A shows its achievements.", help, "");
		}

		// ---- bottom: the games ----
		rnds::Backend* be = st.be;
		be->beginScreen(1);
		st.box("screen", false, 0, 0, 640, 480);
		st.box("bubble", false, 18, 14, 604, 54);
		st.text("Games on this card", true, 22, 38, 28, p.ink);
		if (n)
		{
			char count[32];
			snprintf(count, sizeof count, "%02d / %02d", ra->gsel + 1, n);
			float cw = st.text(count, true, 17, 0, 0, p.muted, false, 0, true);
			st.text(count, true, 17, 602 - cw, 31, p.muted);
		}
		for (int r = 0; r < ROWS && ra->gtop + r < n; r++)
		{
			const Ra::Game& g = ra->games[ra->gtop + r];
			bool sel = ra->gtop + r == ra->gsel;
			float ry = 82 + r * 64;
			st.box(sel ? "pick" : "slot", sel, 18, ry, 604, 56);
			Color c = sel ? p.onPick : p.ink;
			st.icon(systemIcon(g.system), 30, ry + 12, 1);
			std::string v = std::to_string(g.unlocked) + " / " + std::to_string(g.total);
			float vw = st.text(v, true, 17, 0, 0, c, false, 0, true);
			st.text(v, true, 17, 586 - vw, ry + 8, c);
			st.text(st.fit(g.title, true, 20, 490 - vw), true, 20, 74, ry + 6, c);
			// the bar: unlocked of the total, as the Carts theme draws it
			float bx = std::round(st.X(74)), bw = std::round(st.X(512)), by = std::round(st.X(ry + 38)), bh = std::max(2.0f, std::round(st.X(6)));
			be->fill(bx, by, bw, bh, sel ? Color(p.onPick.r, p.onPick.g, p.onPick.b, 0.25f) : p.line);
			if (g.unlocked > 0) be->fill(bx, by, std::max(bh, std::round(bw * g.unlocked / g.total)), bh, sel ? p.onPickAccent : p.accent);
		}
		if (n == 0) st.text("No games with achievements yet.", true, 20, 38, 98, p.muted);
		float bw = st.faceWidth("B", "Back");
		st.face("B", "Back", 18, bw);
		if (n)
		{
			float aw = st.faceWidth("A", "Achievements");
			st.face("A", "Achievements", 622 - aw, aw);
		}
		be->endScreen();
	}
}
