// banana-ui (kiwami): a game's achievements, A on one of Settings > RetroAchievements > Games on this card. One request
// on the pages' thread: the game's hash (kept on the card by path, size and modification time, else computed by
// rahash as RetroArch computes it), its ID (gameid, kept beside the hash once the server knows it), then, signed in,
// the game's core achievements (patch) and the account's unlocks in either mode (unlocks, h=0 and h=1), and last the
// badges the card hasn't got yet (into cache/ra-badges, which upstream's game panel uses too). The bottom panel lists
// the achievements, unlocked first, each with its badge (dimmed while locked), its points and H or S for the mode it
// was unlocked in; the top one has the game's count and the selected achievement in full. Signed out, the game is
// identified and the page asks to sign in: the server gives a game's list only to an account.
#include "ra.h"
#include "rahash.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <unistd.h>

namespace banana
{
	using rnds::Color;
	namespace ra = kiwami::ra;

	static std::string fileOf(const std::string& path)
	{
		size_t s = path.rfind('/');
		return s == std::string::npos ? path : path.substr(s + 1);
	}

	Settings::Result Settings::Kiwami::raGameOpen(int i)
	{
		if (!ra || i < 0 || i >= (int)ra->games.size()) return NONE;
		pageOpen = Page::RA_GAME;
		ra->game = ra->games[i];
		ra->gameState.clear();
		ra->gameHash.clear();
		ra->gameTitle = ra->game.title;
		ra->gameId = 0;
		ra->cheevos.clear();
		ra->unlocked = ra->hardcore = 0;
		ra->asel = ra->atop = 0;
		ra->badges.clear();
		raRead();
		raGameLoad();
		return MOVED;
	}

	void Settings::Kiwami::raGameLoad()
	{
		const Ra::Game g = ra->game;
		const ra::Card card = ra->card;
		const ra::Account a = ra::account(card);
		// what the request found: the hash (and how long it took when computed), the ID, the list and the unlocks, or
		// the first thing that failed
		struct Load
		{
			ra::GameHash h;
			bool hashed = false;
			double ms = 0;
			std::string unidentified;	// why rahash couldn't
			ra::Reply failed;			// the request that failed (failed.error set)
			std::string token;			// a login's, when the card had only the password
			ra::Patch patch;
			ra::Unlocks soft, hard;
		};
		auto l = std::make_shared<Load>();
		auto phase = ra->phase = std::make_shared<std::atomic<int>>(Ra::IDENTIFYING);
		int run = ++ra->gameRun;
		ra->loading = true;
		raRun([l, g, card, a, phase] {
			std::string error;
			if (!ra::cachedHash(card, g.path, l->h))
			{
				auto t0 = std::chrono::steady_clock::now();
				ra::Hash h = ra::hash(g.path, g.system, g.exts);
				l->ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
				if (h.hash.empty()) { l->unidentified = h.error; return; }
				l->h.hash = h.hash;
				l->hashed = true;
				ra::cacheHash(card, g.path, l->h, error);
			}
			if (!l->h.id)
			{
				ra::GameId id = ra::gameId(l->h.hash);
				if (!id.ok) { l->failed = id; return; }
				if (!id.id) return;	// (a hash the server doesn't know)
				l->h.id = id.id;
				ra::cacheHash(card, g.path, l->h, error);
			}
			if (a.user.empty()) return;	// (signed out: identified only)
			*phase = Ra::LOADING;
			std::string token = a.token;
			if (token.empty())
			{
				ra::Login in = ra::login2(a.user, a.password, "");
				if (!in.ok) { l->failed = in; return; }
				token = l->token = in.token;
			}
			l->patch = ra::patch(a.user, token, l->h.id);
			if (!l->patch.ok) { l->failed = l->patch; return; }
			l->soft = ra::unlocks(a.user, token, l->h.id, false);
			if (!l->soft.ok) { l->failed = l->soft; return; }
			l->hard = ra::unlocks(a.user, token, l->h.id, true);
			if (!l->hard.ok) l->failed = l->hard;
		}, [this, l, g, a, run] {
			if (run != ra->gameRun) return;	// (the page opened again since: this answer dropped)
			ra->loading = false;
			const std::string file = fileOf(g.path);
			if (l->hashed) fprintf(stderr, "banana-ui: ra: hashed %s in %.0f ms: %s\n", file.c_str(), l->ms, l->h.hash.c_str());
			if (!l->unidentified.empty())
			{
				fprintf(stderr, "banana-ui: ra: can't identify %s: %s\n", file.c_str(), l->unidentified.c_str());
				ra->gameState = "Can't identify this game: " + l->unidentified + ".";
				return;
			}
			ra->gameHash = l->h.hash;
			ra->gameId = l->h.id;
			// a password with no token yet: the token the login gave, kept (as the Stats page keeps it)
			ra::Account now = ra::account(ra->card);
			std::string error;
			if (!l->token.empty() && ra::sameName(now.user, a.user) && now.token.empty() && now.password == a.password
				&& ra::signedIn(ra->card, now.user, now.password, l->token, error))
			{
				fprintf(stderr, "banana-ui: ra: token kept for %s\n", now.user.c_str());
				raRead();
			}
			if (!l->failed.error.empty())
			{
				const std::string& e = l->failed.error;
				fprintf(stderr, "banana-ui: ra: no achievements for %s: %s\n", file.c_str(), e.c_str());
				ra->gameState = l->failed.refused ? "retroachievements.org refused the account (" + e + "): sign in again on the Account page."
					: e == "no connection" ? "No connection." : "No answer from retroachievements.org (" + e + ").";
				return;
			}
			if (!ra->gameId)
			{
				fprintf(stderr, "banana-ui: ra: %s: hash %s unknown to RetroAchievements\n", file.c_str(), ra->gameHash.c_str());
				ra->gameState = "RetroAchievements doesn't know this version of the game (hash " + ra->gameHash + ").";
				return;
			}
			if (a.user.empty())
			{
				fprintf(stderr, "banana-ui: ra: game %u identified, not signed in\n", ra->gameId);
				ra->gameState = "Sign in on the Account page to see this game's achievements: retroachievements.org shows "
					"them to an account. RetroAchievements knows it as game " + std::to_string(ra->gameId) + ".";
				return;
			}
			// unlocked first (in either mode: the softcore list has every one), then locked, each in the set's order
			auto has = [](const std::vector<unsigned>& v, unsigned id) { return std::find(v.begin(), v.end(), id) != v.end(); };
			std::vector<Ra::Cheevo> on, off;
			for (auto& x : l->patch.achievements)
			{
				Ra::Cheevo c;
				c.a = x;
				c.hardcore = has(l->hard.ids, x.id);
				c.unlocked = c.hardcore || has(l->soft.ids, x.id);
				(c.unlocked ? on : off).push_back(c);
			}
			ra->unlocked = (int)on.size();
			ra->hardcore = (int)std::count_if(on.begin(), on.end(), [](const Ra::Cheevo& c) { return c.hardcore; });
			ra->cheevos = on;
			ra->cheevos.insert(ra->cheevos.end(), off.begin(), off.end());
			if (!l->patch.title.empty()) ra->gameTitle = l->patch.title;
			fprintf(stderr, "banana-ui: ra: game %u \"%s\": %d achievements, %d unlocked (%d hardcore)\n", ra->gameId,
				ra->gameTitle.c_str(), (int)ra->cheevos.size(), ra->unlocked, ra->hardcore);
			if (ra->cheevos.empty()) ra->gameState = "This game has no achievements yet.";
			raGameBadges();
		}, false);
	}

	// the badges the card hasn't got, fetched after the list shows; each drawn as it arrives (a missing one is looked
	// for again once they are all in)
	void Settings::Kiwami::raGameBadges()
	{
		std::vector<std::string> missing;
		for (auto& c : ra->cheevos)
		{
			const std::string& b = c.a.badge;
			if (!b.empty() && std::find(missing.begin(), missing.end(), b) == missing.end()
				&& access((ra->card.badges + "/" + b + ".png").c_str(), F_OK) != 0)
				missing.push_back(b);
		}
		if (missing.empty()) return;
		*ra->phase = Ra::BADGES;
		std::string dir = ra->card.badges;
		int run = ra->gameRun;
		ra->loading = true;
		raRun([missing, dir] {
			for (auto& b : missing) ra::badge(b, dir);
		}, [this, run, missing] {
			if (run != ra->gameRun) return;
			ra->loading = false;
			for (auto& b : missing) ra->badges.erase(b);
		}, false);
	}

	Settings::Result Settings::Kiwami::raGamePress(Key k)
	{
		int n = (int)ra->cheevos.size();
		switch (k)
		{
		case Key::UP: case Key::DOWN:
			if (n < 2) return NONE;
			ra->asel = (ra->asel + (k == Key::UP ? -1 : 1) + n) % n;
			if (ra->asel < ra->atop) ra->atop = ra->asel;
			if (ra->asel >= ra->atop + ROWS) ra->atop = ra->asel - ROWS + 1;
			return MOVED;
		case Key::B: pageOpen = Page::RA_GAMES; ra->badges.clear(); return MOVED;
		case Key::START: st.page = LIST; st.mOpen = false; return CLOSED;
		default: return NONE;
		}
	}

	Settings::Result Settings::Kiwami::raGameTouch(float, float cy)
	{
		float dx = st.downX, dy = st.downY;
		int n = (int)ra->cheevos.size();
		if (dy >= 418 && dy <= 466) return dx < 320 ? raGamePress(Key::B) : NONE;
		for (int r = 0; r < ROWS && ra->atop + r < n; r++)
		{
			float ry = 82 + r * 64;
			if (dy < ry || dy > ry + 56) continue;
			if (ra->atop + r == ra->asel) return NONE;
			ra->asel = ra->atop + r;
			return MOVED;
		}
		// a swipe up or down scrolls a page
		if (std::fabs(cy - dy) > 40 && n > ROWS)
		{
			ra->atop = std::max(0, std::min(n - ROWS, ra->atop + (cy < dy ? ROWS : -ROWS)));
			ra->asel = std::max(ra->atop, std::min(ra->asel, ra->atop + ROWS - 1));
			return MOVED;
		}
		return NONE;
	}

	// a badge in a square of size (CSS px), decoded once while the page shows, dimmed while locked; the trophy when the
	// card hasn't got it
	void Settings::Kiwami::raBadge(const std::string& name, float x, float y, float size, bool dim)
	{
		rnds::TexPtr tex;
		if (!name.empty())
		{
			auto it = ra->badges.find(name);
			if (it == ra->badges.end())
			{
				rnds::Bitmap b = rnds::decodeFile(ra->card.badges + "/" + name + ".png");
				it = ra->badges.emplace(name, b.empty() ? rnds::TexPtr() : st.be->upload(b, true)).first;
			}
			tex = it->second;
		}
		if (tex)
		{
			float px = std::round(st.X(size));
			st.be->draw(tex, std::round(st.X(x)), std::round(st.X(y)), px, px, Color(1, 1, 1, dim ? 0.35f : 1));
			return;
		}
		int k = std::max(1, (int)(size / 32));
		st.icon("kiwami.trophy", x + (size - 32 * k) / 2, y + (size - 32 * k) / 2, k);
	}

	void Settings::Kiwami::raGameRender()
	{
		Colours p = colours(st.light);
		int n = (int)ra->cheevos.size();
		ra->asel = std::max(0, std::min(ra->asel, n - 1));
		const Ra::Cheevo* sel = n ? &ra->cheevos[ra->asel] : nullptr;
		const int phase = ra->phase ? ra->phase->load() : Ra::IDENTIFYING;
		const bool busy = ra->loading && phase != Ra::BADGES;
		const std::string doing = phase == Ra::IDENTIFYING ? "Identifying the game..." : "Loading its achievements...";

		// ---- top: the game, and the selected achievement in full ----
		std::string accent, state, help;
		if (n)
		{
			accent = std::to_string(ra->unlocked) + " / " + std::to_string(n) + " \xc2\xb7 " + std::to_string(ra->hardcore) + " hardcore";
			state = sel->a.title + ": " + sel->a.description + " (" + std::to_string(sel->a.points)
				+ (sel->a.points == 1 ? " point" : " points") + ")";
			help = !sel->unlocked ? "Locked." : sel->hardcore ? "Unlocked in Hardcore (H)." : "Unlocked in Softcore (S).";
		}
		else state = busy ? doing : ra->gameState;
		st.be->beginScreen(0);
		st.box("screen", false, 0, 0, 640, 480);
		st.box("hero", false, 18, 20, 604, 436);
		raBadge(sel ? sel->a.badge : "", 50, 56, 96, sel && !sel->unlocked);
		st.text("RetroAchievements \xc2\xb7 " + ra->game.systemName, true, 17, 170, 62, p.muted, true, 1.4f);
		st.text(st.fit(ra->gameTitle, true, 30, 420), true, 30, 170, 88, p.ink);
		if (!accent.empty()) st.text(st.fit(accent, true, 22, 420), true, 22, 170, 130, p.accent);
		float y = para(state, 196, 7, p.ink);
		if (!state.empty()) y += 14;
		para(help, y, (int)((440 - y) / 28), p.muted);
		st.be->endScreen();

		// ---- bottom: the achievements ----
		rnds::Backend* be = st.be;
		be->beginScreen(1);
		st.box("screen", false, 0, 0, 640, 480);
		st.box("bubble", false, 18, 14, 604, 54);
		st.text("Achievements", true, 22, 38, 28, p.ink);
		if (n)
		{
			char count[32];
			snprintf(count, sizeof count, "%02d / %02d", ra->asel + 1, n);
			float cw = st.text(count, true, 17, 0, 0, p.muted, false, 0, true);
			st.text(count, true, 17, 602 - cw, 31, p.muted);
		}
		for (int r = 0; r < ROWS && ra->atop + r < n; r++)
		{
			const Ra::Cheevo& c = ra->cheevos[ra->atop + r];
			bool on = ra->atop + r == ra->asel;
			float ry = 82 + r * 64;
			st.box(on ? "pick" : "slot", on, 18, ry, 604, 56);
			Color ink = on ? p.onPick : c.unlocked ? p.ink : p.muted, m = on ? p.onPick : p.muted;
			raBadge(c.a.badge, 30, ry + 8, 40, !c.unlocked);
			std::string v = std::to_string(c.a.points) + (c.unlocked ? (c.hardcore ? "  H" : "  S") : "");
			float vw = st.text(v, true, 17, 0, 0, ink, false, 0, true);
			st.text(v, true, 17, 586 - vw, ry + 8, c.unlocked ? (on ? p.onPickAccent : p.accent) : m);
			st.text(st.fit(c.a.title, true, 20, 490 - vw), true, 20, 82, ry + 6, ink);
			st.text(st.fit(c.a.description, false, 15, 500), false, 15, 82, ry + 32, m);
		}
		if (!n) st.text(busy ? doing : ra->gameState.empty() ? "" : "No achievements to show.", true, 20, 38, 98, p.muted);
		float bw = st.faceWidth("B", "Back");
		st.face("B", "Back", 18, bw);
		be->endScreen();
	}
}
