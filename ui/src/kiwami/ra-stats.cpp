// banana-ui (kiwami): RetroAchievements' Stats page, A on Settings > RetroAchievements > Stats. The top panel has the
// account: its avatar, name, hardcore and softcore points, unread messages, and the date of the numbers when they are
// the ones kept on the card; the bottom panel the nine systems in raclient's CONSOLES order, each with its games
// started, completed (every achievement unlocked) and mastered (every one in Hardcore), and its achievements unlocked.
// The numbers are fetched when the page opens and on Y, never otherwise: login2 with the token (the password when
// there is none yet, which keeps the token it gives), the avatar, then allprogress for each system, one request at a
// time on the pages' thread. Once fetched they are kept on the card (rastore's stats), and shown from there when the
// server can't be reached.
#include "ra.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <ctime>

namespace banana
{
	using rnds::Color;
	namespace ra = kiwami::ra;

	// the theme's icon of each system (Mega Drive's is md)
	static std::string systemIcon(const char* system)
	{
		return strcmp(system, "megadrive") == 0 ? "md" : system;
	}

	static std::string asOf(long long when)
	{
		char b[32];
		time_t t = (time_t)when;
		struct tm tm;
		strftime(b, sizeof b, "%Y-%m-%d %H:%M", localtime_r(&t, &tm));
		return b;
	}

	Settings::Result Settings::Kiwami::raStatsOpen()
	{
		if (!ra) return NONE;
		st.page = KIWAMI;
		raRead();
		ra->ssel = ra->stop = 0;
		ra->statsError.clear();
		ra->stats = ra::Stats();
		ra->statsCached = false;
		if (ra->user.empty())
		{
			fprintf(stderr, "banana-ui: ra: stats page: not signed in\n");	// (nothing asked)
			return MOVED;
		}
		// what the card has, shown while the server is asked
		ra->stats = ra::loadStats(ra->card, ra->user);
		ra->statsCached = ra->stats.when != 0;
		raStatsFetch();
		return MOVED;
	}

	void Settings::Kiwami::raStatsFetch()
	{
		ra::Account a = ra::account(ra->card);
		if (a.user.empty()) return;
		struct Fetch { ra::Login login; ra::Progress progress[9]; int got = 0; std::string error; bool refused = false; };
		auto f = std::make_shared<Fetch>();
		std::string dir = ra->card.avatars;
		raRun([f, a, dir] {
			f->login = ra::login2(a.user, a.token.empty() ? a.password : "", a.token);
			if (!f->login.ok) { f->error = f->login.error; f->refused = f->login.refused; return; }
			ra::avatar(f->login.user, f->login.avatarUrl, dir);
			for (int i = 0; i < 9; i++)
			{
				f->progress[i] = ra::allProgress(f->login.user, f->login.token, ra::CONSOLES[i].id);
				if (!f->progress[i].ok) { f->error = f->progress[i].error; f->refused = f->progress[i].refused; return; }
				f->got++;
			}
		}, [this, f, a] {
			ra::Account now = ra::account(ra->card);
			if (!ra::sameName(now.user, a.user)) { raRead(); return; }	// (another account since: its answer dropped)
			std::string error;
			const std::string key = ra::fileName(a.user);
			if (f->login.ok)
			{
				ra->points[key] = { f->login.score, f->login.softScore, f->login.messages };
				ra->avatars.erase(key);
				// a password with no token yet: the token it got, kept
				if (now.token.empty() && !now.password.empty() && now.password == a.password
					&& ra::signedIn(ra->card, now.user, now.password, f->login.token, error))
					fprintf(stderr, "banana-ui: ra: token kept for %s\n", now.user.c_str());
			}
			if (f->got == 9)
			{
				ra::Stats s;
				s.when = (long long)time(nullptr);
				s.score = f->login.score; s.softScore = f->login.softScore; s.messages = f->login.messages;
				for (int i = 0; i < 9; i++)
				{
					const ra::Progress& p = f->progress[i];
					s.systems[i] = { p.started, p.completed, p.mastered, p.unlocked };
				}
				ra::saveStats(ra->card, a.user, s, error);
				ra->stats = s;
				ra->statsCached = false;
				ra->statsError.clear();
				fprintf(stderr, "banana-ui: ra: stats for %s: 9 systems\n", a.user.c_str());
			}
			else
			{
				ra->stats = ra::loadStats(ra->card, a.user);
				ra->statsCached = ra->stats.when != 0;
				ra->statsError = f->refused ? "retroachievements.org refused the account (" + f->error + "): sign in again on the Account page."
					: f->error == "no connection" ? "No connection" : "No answer from retroachievements.org (" + f->error + ")";
				if (ra->statsCached)
					fprintf(stderr, "banana-ui: ra: stats for %s from the cache (as of %lld)\n", a.user.c_str(), ra->stats.when);
				else fprintf(stderr, "banana-ui: ra: no stats for %s: %s\n", a.user.c_str(), f->error.c_str());
			}
			raRead();
		}, false);
	}

	Settings::Result Settings::Kiwami::raStatsPress(Key k)
	{
		const int n = 9;
		switch (k)
		{
		case Key::UP: case Key::DOWN:
			ra->ssel = (ra->ssel + (k == Key::UP ? -1 : 1) + n) % n;
			if (ra->ssel < ra->stop) ra->stop = ra->ssel;
			if (ra->ssel >= ra->stop + ROWS) ra->stop = ra->ssel - ROWS + 1;
			return MOVED;
		case Key::Y:
			if (ra->user.empty() || ra->task) return NONE;
			raStatsFetch();
			return MOVED;
		case Key::B: st.page = LIST; ra->avatars.clear(); return MOVED;
		case Key::START: st.page = LIST; st.mOpen = false; ra->avatars.clear(); return CLOSED;
		default: return NONE;
		}
	}

	Settings::Result Settings::Kiwami::raStatsTouch(float, float cy)
	{
		float dx = st.downX, dy = st.downY;
		if (dy >= 418 && dy <= 466) return raStatsPress(dx < 320 ? Key::B : Key::Y);
		for (int r = 0; r < ROWS && ra->stop + r < 9; r++)
		{
			float ry = 82 + r * 64;
			if (dy < ry || dy > ry + 56) continue;
			if (ra->stop + r == ra->ssel) return NONE;
			ra->ssel = ra->stop + r;
			return MOVED;
		}
		// a swipe up or down scrolls a page
		if (std::fabs(cy - dy) > 40)
		{
			ra->stop = std::max(0, std::min(9 - ROWS, ra->stop + (cy < dy ? ROWS : -ROWS)));
			ra->ssel = std::max(ra->stop, std::min(ra->ssel, ra->stop + ROWS - 1));
			return MOVED;
		}
		return NONE;
	}

	void Settings::Kiwami::raStatsRender()
	{
		Colours p = colours(st.light);
		const bool updating = ra->task && !ra->signingIn, have = ra->stats.when != 0;
		const ra::Stats& s = ra->stats;
		const ra::Console& sys = ra::CONSOLES[ra->ssel];
		const ra::SystemStats& y = s.systems[ra->ssel];

		// ---- top: the account ----
		if (ra->user.empty()) raTop("Stats", "", "Sign in on the Account page first: the stats are the account's.", "", "");
		else
		{
			std::string accent, state, help;
			auto q = ra->points.find(ra::fileName(ra->user));
			if (q != ra->points.end())
				accent = raPoints(q->second.score) + " \xc2\xb7 " + std::to_string(q->second.softScore) + " softcore";
			if (updating) state = "Updating...";
			else if (!ra->statsError.empty()) state = ra->statsError + (ra->statsCached ? ": the stats as of " + asOf(s.when) + "." : ".");
			else if (ra->statsCached) state = "As of " + asOf(s.when) + ".";
			if (q != ra->points.end() && q->second.messages)
				state += std::string(state.empty() ? "" : " ") + std::to_string(q->second.messages)
					+ (q->second.messages == 1 ? " unread message" : " unread messages") + " on retroachievements.org.";
			if (have)
				help = std::string(sys.name) + ": " + (y.started == 0 ? "no games started yet." : std::to_string(y.started)
					+ (y.started == 1 ? " game" : " games") + " started, " + std::to_string(y.completed) + " completed (every achievement "
					"unlocked), " + std::to_string(y.mastered) + " mastered (every one in Hardcore), " + std::to_string(y.unlocked)
					+ (y.unlocked == 1 ? " achievement" : " achievements") + " unlocked.");
			raTop(ra->user, accent, state, help, ra->user);
		}

		// ---- bottom: the systems ----
		rnds::Backend* be = st.be;
		be->beginScreen(1);
		st.box("screen", false, 0, 0, 640, 480);
		st.box("bubble", false, 18, 14, 604, 54);
		st.text("Stats", true, 22, 38, 28, p.ink);
		char count[32];
		snprintf(count, sizeof count, "%02d / %02d", ra->ssel + 1, 9);
		float cw = st.text(count, true, 17, 0, 0, p.muted, false, 0, true);
		st.text(count, true, 17, 602 - cw, 31, p.muted);
		for (int r = 0; r < ROWS && ra->stop + r < 9; r++)
		{
			int i = ra->stop + r;
			bool sel = i == ra->ssel;
			float ry = 82 + r * 64;
			st.box(sel ? "pick" : "slot", sel, 18, ry, 604, 56);
			Color c = sel ? p.onPick : p.ink, m = sel ? p.onPick : p.muted;
			st.icon(systemIcon(ra::CONSOLES[i].system), 30, ry + 12, 1);
			st.text(ra::CONSOLES[i].name, true, 20, 74, ry + 6, c);
			if (!have) continue;
			const ra::SystemStats& z = s.systems[i];
			std::string detail = z.started == 0 ? "No games started" : std::to_string(z.started) + " started \xc2\xb7 "
				+ std::to_string(z.completed) + " completed \xc2\xb7 " + std::to_string(z.mastered) + " mastered";
			st.text(detail, false, 15, 74, ry + 32, m);
			if (z.started == 0) continue;
			std::string v = std::to_string(z.unlocked) + (z.unlocked == 1 ? " achievement" : " achievements");
			float vw = st.text(v, true, 17, 0, 0, c, false, 0, true);
			st.text(v, true, 17, 586 - vw, ry + 8, c);
		}
		float bw = st.faceWidth("B", "Back");
		st.face("B", "Back", 18, bw);
		if (!ra->user.empty() && !updating)
		{
			float yw = st.faceWidth("Y", "Update");
			st.face("Y", "Update", 622 - yw, yw);
		}
		be->endScreen();
	}
}
