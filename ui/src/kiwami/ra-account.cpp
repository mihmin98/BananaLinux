// banana-ui (kiwami): RetroAchievements' Account page, A on Settings > RetroAchievements > Account. The bottom panel
// has its rows in the Settings list's look: the account signed in (its avatar, name and points) and Sign out (A twice),
// then the accounts signed out here before, remembered (A signs in again: the password is asked; X twice removes one),
// then New account (its name, then its password, on upstream's keyboard). A sign-in is checked with the server
// (login2) and keeps the token it gives; with no answer, the name and password are kept unchecked, and the token is
// fetched when the page next opens. The top panel shows the selected row in full. Also the group's part of the hooks
// that every RetroAchievements page shares: the account adopted when Settings opens, the row's value, the request.
#include "ra.h"
#include <algorithm>
#include <cmath>
#include <thread>

namespace banana
{
	using rnds::Color;
	namespace ra = kiwami::ra;

	static std::string points(long n)
	{
		return std::to_string(n) + (n == 1 ? " point" : " points");
	}

	// ---- the group's part of the hooks ---------------------------------------------------------------------------
	void Settings::Kiwami::raOpened()
	{
		bool group = false;
		for (auto& it : st.items) group |= it.key == "kiwami.ra.account";
		if (!group) return;	// (KIWAMI_RA_GROUP=0: the card left alone)
		if (!ra) ra = std::make_shared<Ra>(st.data);
		ra::adopt(ra->card);	// (an account the setup or RetroArch put there since)
		raRead();
	}

	void Settings::Kiwami::raRead()
	{
		ra::Account a = ra::account(ra->card);
		ra->user = a.user;
		ra->token = !a.token.empty();
		ra->remembered = ra::accounts(ra->card).remembered;
		std::vector<std::string> names = ra->remembered;
		names.push_back(a.user);
		for (auto& n : names)
		{
			std::string f = ra::fileName(n);
			if (f.empty() || ra->points.count(f)) continue;
			ra::Stats s = ra::loadStats(ra->card, n);
			if (s.when) ra->points[f] = { s.score, s.softScore, s.messages };
		}
	}

	std::string Settings::Kiwami::raValue() const
	{
		return !ra ? "" : ra->user.empty() ? "Not signed in" : ra->user;
	}

	void Settings::Kiwami::raRun(std::function<void()> work, std::function<void()> then, bool signingIn)
	{
		ra->signingIn = signingIn;
		if (st.sync)
		{
			ra->task.reset();
			work();
			then();
			ra->signingIn = false;
			return;
		}
		auto t = std::make_shared<Ra::Task>();
		ra->task = t;
		ra->then = std::move(then);
		std::thread([t, work] {
			work();
			std::lock_guard<std::mutex> l(t->mu);
			t->done = true;
		}).detach();
	}

	bool Settings::Kiwami::raUpdate()
	{
		if (!ra) return false;
		// the avatars only while the page or its keyboard shows
		bool shown = pageOpen == Page::RA_ACCOUNT && (st.page == KIWAMI || st.page == KEYBOARD);
		if (!shown && !ra->avatars.empty()) ra->avatars.clear();
		if (!ra->task) return false;
		{
			std::lock_guard<std::mutex> l(ra->task->mu);
			if (!ra->task->done) return false;
		}
		std::function<void()> then = std::move(ra->then);
		ra->task.reset();
		ra->signingIn = false;
		then();
		return true;
	}

	int Settings::Kiwami::raWake() const
	{
		return ra && ra->task ? 100 : -1;
	}

	// ---- signing in and out --------------------------------------------------------------------------------------
	void Settings::Kiwami::raSignIn(const std::string& name, const std::string& password)
	{
		fprintf(stderr, "banana-ui: ra: checking the account %s\n", name.c_str());
		st.toast("RetroAchievements", "Checking the account " + name + "...", 0x6ab0ff, 0, 30000);
		auto l = std::make_shared<ra::Login>();
		std::string dir = ra->card.avatars;
		raRun([l, name, password, dir] {
			*l = ra::login2(name, password, "");
			if (l->ok) ra::avatar(l->user, l->avatarUrl, dir);
		}, [this, l, name, password] {
			std::string error;
			if (l->ok)
			{
				// the name as the server writes it
				if (!ra::signedIn(ra->card, l->user, password, l->token, error)) { st.message = error; raRead(); return; }
				fprintf(stderr, "banana-ui: ra: signed in as %s (token kept)\n", l->user.c_str());
				ra->points[ra::fileName(l->user)] = { l->score, l->softScore, l->messages };
				ra->avatars.erase(ra::fileName(l->user));
				st.message.clear();
				st.toast("RetroAchievements", "Signed in as " + l->user + " \xc2\xb7 " + points(l->score), 0x6ad07a, 1, 5000);
				ra->selKey = "account";
			}
			else if (l->refused)
			{
				// (nothing kept: a password the server refused never is)
				fprintf(stderr, "banana-ui: ra: sign in refused for %s\n", name.c_str());
				const std::string& e = l->error;
				st.message = e.compare(0, 8, "refused ") == 0 || e.find("nvalid") != std::string::npos || e.find("assword") != std::string::npos
					? "Wrong account name or password" : e;
				st.toast("RetroAchievements sign-in failed", st.message, 0xff7a4a, 2, 7000);
			}
			else
			{
				// no answer: kept unchecked, as the setup keeps it; the token once the server answers
				if (!ra::signedIn(ra->card, name, password, "", error)) { st.message = error; raRead(); return; }
				fprintf(stderr, "banana-ui: ra: signed in as %s, not checked: %s\n", name.c_str(), l->error.c_str());
				st.message = "Not checked: " + l->error;
				st.toast("RetroAchievements", "Signed in as " + name + ", not checked: " + l->error, 0xf2c94c, 0, 7000);
				ra->selKey = "account";
			}
			raRead();
		}, true);
	}

	void Settings::Kiwami::raFetchToken()
	{
		ra::Account a = ra::account(ra->card);
		fprintf(stderr, "banana-ui: ra: no token for %s yet: asking for it\n", a.user.c_str());
		auto l = std::make_shared<ra::Login>();
		std::string dir = ra->card.avatars;
		raRun([l, a, dir] {
			*l = ra::login2(a.user, a.password, "");
			if (l->ok) ra::avatar(l->user, l->avatarUrl, dir);
		}, [this, l, a] {
			// (kept only for the account it was asked for, if that is still the one on the card)
			ra::Account now = ra::account(ra->card);
			if (l->ok && ra::sameName(now.user, a.user) && now.password == a.password && now.token.empty())
			{
				std::string error;
				if (ra::signedIn(ra->card, now.user, now.password, l->token, error))
					fprintf(stderr, "banana-ui: ra: token kept for %s\n", now.user.c_str());
				else st.message = error;
				ra->points[ra::fileName(now.user)] = { l->score, l->softScore, l->messages };
				ra->avatars.erase(ra::fileName(now.user));
			}
			else if (l->refused)
			{
				fprintf(stderr, "banana-ui: ra: the password kept for %s was refused\n", a.user.c_str());
				st.message = "retroachievements.org refused the password kept for " + a.user + ": sign out, then sign in again.";
			}
			else if (!l->ok) fprintf(stderr, "banana-ui: ra: no token for %s: %s\n", a.user.c_str(), l->error.c_str());
			raRead();
		}, false);
	}

	void Settings::Kiwami::raSignOut()
	{
		std::string name = ra->user, error;
		bool ok = ra::signOut(ra->card, error);
		raRead();
		st.message = error;
		if (ok) st.toast("RetroAchievements", "Signed out " + name, 0x6ab0ff, 0, 5000);
	}

	void Settings::Kiwami::raRemove(const std::string& name)
	{
		std::string error;
		ra::remove(ra->card, name, error);
		ra->points.erase(ra::fileName(name));
		ra->avatars.erase(ra::fileName(name));
		st.message = error;
		raRead();
	}

	// ---- the page -------------------------------------------------------------------------------------------------
	std::vector<std::string> Settings::Kiwami::raRows() const
	{
		std::vector<std::string> r;
		if (!ra->user.empty()) { r.push_back("account"); r.push_back("signout"); }
		for (auto& n : ra->remembered) r.push_back("r:" + n);
		r.push_back("new");
		return r;
	}

	int Settings::Kiwami::raSelected(const std::vector<std::string>& rows)
	{
		int n = (int)rows.size(), i = -1;
		for (int r = 0; r < n; r++) if (rows[r] == ra->selKey) i = r;
		if (i < 0) { i = 0; ra->selKey = rows[0]; }	// (a row that went away: the first)
		if (i < ra->rtop) ra->rtop = i;
		if (i >= ra->rtop + ROWS) ra->rtop = i - ROWS + 1;
		ra->rtop = std::max(0, std::min(ra->rtop, n - ROWS));
		return i;
	}

	Settings::Result Settings::Kiwami::raOpen()
	{
		if (!ra) return NONE;
		st.page = KIWAMI;
		st.message.clear();
		ra->outArmed = false;
		ra->rmArmed.clear();
		raRead();
		ra->selKey = raRows()[0];
		ra->rtop = 0;
		fprintf(stderr, "banana-ui: ra: account page: %s, %d remembered\n",
			ra->user.empty() ? "not signed in" : ("signed in as " + ra->user).c_str(), (int)ra->remembered.size());
		if (ra::needsToken(ra->card) && !ra->task) raFetchToken();
		return MOVED;
	}

	Settings::Result Settings::Kiwami::raPress(Key k)
	{
		std::vector<std::string> rows = raRows();
		int n = (int)rows.size(), i = raSelected(rows);
		const std::string key = rows[i];
		// a second A or X confirms; any other key takes the first back
		bool out = ra->outArmed;
		std::string rm = ra->rmArmed;
		ra->outArmed = false;
		ra->rmArmed.clear();
		bool busy = ra->task && ra->signingIn;
		switch (k)
		{
		case Key::UP: case Key::DOWN:
			ra->selKey = rows[(i + (k == Key::UP ? -1 : 1) + n) % n];
			st.message.clear();
			return MOVED;
		case Key::A:
			if (busy) return NONE;
			if (key == "signout")
			{
				if (!out) { ra->outArmed = true; return MOVED; }
				raSignOut();
				return CHANGED;
			}
			if (key == "new")
			{
				ra->typing = Ra::NAME;
				st.startEntry(ENTRY_KIWAMI, "RetroAchievements", "Account name", KIWAMI);
				return MOVED;
			}
			if (key.compare(0, 2, "r:") == 0)
			{
				ra->typing = Ra::PASSWORD;
				ra->typedName = key.substr(2);
				fprintf(stderr, "banana-ui: ra: password asked for %s\n", ra->typedName.c_str());
				st.startEntry(ENTRY_KIWAMI, "RetroAchievements password for", ra->typedName, KIWAMI);
				return MOVED;
			}
			return out || !rm.empty() ? MOVED : NONE;
		case Key::X:
			if (busy || key.compare(0, 2, "r:") != 0) return out || !rm.empty() ? MOVED : NONE;
			if (rm != key.substr(2)) { ra->rmArmed = key.substr(2); return MOVED; }
			raRemove(rm);
			return CHANGED;
		case Key::B: st.page = LIST; ra->avatars.clear(); return MOVED;
		case Key::START: st.page = LIST; st.mOpen = false; ra->avatars.clear(); return CLOSED;
		default: return out || !rm.empty() ? MOVED : NONE;
		}
	}

	// the chips along the bottom: B Back on the left, A on the right, X Remove in the middle on a remembered account

	Settings::Result Settings::Kiwami::raTouch(float, float cy)
	{
		float dx = st.downX, dy = st.downY;
		std::vector<std::string> rows = raRows();
		int n = (int)rows.size(), i = raSelected(rows);
		if (dy >= 418 && dy <= 466)
		{
			if (rows[i].compare(0, 2, "r:") == 0)
			{
				float xw = st.faceWidth("X", "Remove");
				if (dx >= 320 - xw / 2 && dx <= 320 + xw / 2) return raPress(Key::X);
			}
			return raPress(dx < 320 ? Key::B : Key::A);
		}
		for (int r = 0; r < ROWS && ra->rtop + r < n; r++)
		{
			float ry = 82 + r * 64;
			if (dy < ry || dy > ry + 56) continue;
			if (ra->rtop + r != i)
			{
				ra->selKey = rows[ra->rtop + r];
				ra->outArmed = false;
				ra->rmArmed.clear();
				st.message.clear();
				return MOVED;
			}
			return raPress(Key::A);
		}
		// a swipe up or down scrolls a page
		if (std::fabs(cy - dy) > 40 && n > ROWS)
		{
			ra->rtop = std::max(0, std::min(n - ROWS, ra->rtop + (cy < dy ? ROWS : -ROWS)));
			ra->selKey = rows[std::max(ra->rtop, std::min(i, ra->rtop + ROWS - 1))];
			return MOVED;
		}
		return NONE;
	}

	std::string Settings::Kiwami::raEntryHelp() const
	{
		return std::string("A types the key, B deletes, X capitals, L symbols, Y a space, START when it is typed. Or tap the keys. ") +
			(ra && ra->typing == Ra::PASSWORD ? "The account's password, checked with retroachievements.org. Kept in banana/accounts.conf "
				"on the card, which file sharing never shows." : "The account's name, as on retroachievements.org.");
	}

	Settings::Result Settings::Kiwami::raEntered(std::string& text)
	{
		std::string t = text;
		t.erase(0, t.find_first_not_of(' '));
		t.erase(t.find_last_not_of(' ') + 1);
		if (ra->typing == Ra::NAME)
		{
			if (t.empty()) { st.message = "The account's name, as on retroachievements.org."; return MOVED; }
			ra->typing = Ra::PASSWORD;
			ra->typedName = t;
			st.startEntry(ENTRY_KIWAMI, "RetroAchievements password for", t, KIWAMI);
			return CHANGED;
		}
		if (text.empty()) { st.message = "The account's password."; return MOVED; }
		if (ra->task && ra->signingIn) return NONE;
		std::string password = text;
		text.clear();
		st.message.clear();
		st.page = KIWAMI;
		raSignIn(ra->typedName, password);
		return CHANGED;
	}

	// ---- drawing ----------------------------------------------------------------------------------------------------
	// an account's avatar in a square of size (CSS px), decoded once while the page shows; the trophy when it has none
	void Settings::Kiwami::raAvatar(const std::string& name, float x, float y, float size)
	{
		std::string f = ra::fileName(name);
		rnds::TexPtr tex;
		if (!f.empty())
		{
			auto it = ra->avatars.find(f);
			if (it == ra->avatars.end())
			{
				rnds::Bitmap b = rnds::decodeFile(ra->card.avatars + "/" + f + ".png");
				it = ra->avatars.emplace(f, b.empty() ? rnds::TexPtr() : st.be->upload(b, true)).first;
			}
			tex = it->second;
		}
		if (tex)
		{
			float px = std::round(st.X(size));
			st.be->draw(tex, std::round(st.X(x)), std::round(st.X(y)), px, px, Color(1, 1, 1, 1));
			return;
		}
		int k = std::max(1, (int)(size / 32));
		st.icon("kiwami.trophy", x + (size - 32 * k) / 2, y + (size - 32 * k) / 2, k);
	}

	// the top panel: the account's avatar (or the trophy), RetroAchievements, the title and a line in the accent, then
	// the state (in ink) and the help (muted), as Tailscale's page
	void Settings::Kiwami::raTop(const std::string& title, const std::string& accent, const std::string& state, const std::string& help,
		const std::string& avatar)
	{
		Colours p = colours(st.light);
		st.be->beginScreen(0);
		st.box("screen", false, 0, 0, 640, 480);
		st.box("hero", false, 18, 20, 604, 436);
		raAvatar(avatar, 50, 56, 96);
		st.text("RetroAchievements", true, 17, 170, 62, p.muted, true, 1.4f);
		st.text(st.fit(title, true, 30, 420), true, 30, 170, 88, p.ink);
		if (!accent.empty()) st.text(st.fit(accent, true, 22, 420), true, 22, 170, 130, p.accent);
		float y = para(state, 196, 7, p.ink);
		if (!state.empty()) y += 14;
		para(help, y, (int)((440 - y) / 28), p.muted);
		st.be->endScreen();
	}

	void Settings::Kiwami::raRender()
	{
		Colours p = colours(st.light);
		std::vector<std::string> rows = raRows();
		int n = (int)rows.size(), at = raSelected(rows);
		const std::string key = rows[at];
		bool busy = ra->task && ra->signingIn;
		auto pts = [this](const std::string& name) -> const Ra::Points* {
			auto it = ra->points.find(ra::fileName(name));
			return it == ra->points.end() ? nullptr : &it->second;
		};

		// ---- top: the selected row in full ----
		std::string title, accent, state, help, avatar;
		if (key == "account" || key == "signout")
		{
			title = avatar = ra->user;
			if (const Ra::Points* q = pts(ra->user)) accent = points(q->score) + " \xc2\xb7 " + std::to_string(q->softScore) + " softcore";
			if (key == "account")
			{
				state = "Signed in. RetroArch, SuperDrastic and BananaDS unlock achievements on this account.";
				if (const Ra::Points* q = pts(ra->user); q && q->messages)
					state += " " + std::to_string(q->messages) + (q->messages == 1 ? " unread message" : " unread messages") + " on retroachievements.org.";
				if (!ra->token) state += " Not checked with the server yet: it is the next time this page opens with Wi-Fi.";
				help = "Hardcore points count unlocks in Hardcore mode, softcore points the others (Mode, in this group).";
			}
			else help = ra->outArmed ? "Press A again to sign out." : "The password and the login token leave the card, here and in the "
				"emulators, and achievements stop unlocking. The account stays in the list below, to sign in again with its password. "
				"Press A twice.";
		}
		else if (key == "new")
		{
			title = "New account";
			help = "Sign in to an account on retroachievements.org: its name, then its password, checked with the server. "
				"An account signed in now is remembered in this list.";
		}
		else
		{
			title = avatar = key.substr(2);
			if (const Ra::Points* q = pts(title)) accent = points(q->score) + " \xc2\xb7 " + std::to_string(q->softScore) + " softcore";
			state = "Signed out. A signs in again: its password is asked.";
			help = ra->rmArmed == title ? "Press X again to remove " + title + " from this list." :
				"X twice removes it from this list, with its picture and its stats kept on the card.";
		}
		if (busy) state = "Checking the account " + ra->typedName + "...";
		else if (!st.message.empty()) state = st.message;
		raTop(title, accent, state, help, avatar);

		// ---- bottom: the rows ----
		rnds::Backend* be = st.be;
		be->beginScreen(1);
		st.box("screen", false, 0, 0, 640, 480);
		st.box("bubble", false, 18, 14, 604, 54);
		st.text("Account", true, 22, 38, 28, p.ink);
		char count[32];
		snprintf(count, sizeof count, "%02d / %02d", at + 1, n);
		float cw = st.text(count, true, 17, 0, 0, p.muted, false, 0, true);
		st.text(count, true, 17, 602 - cw, 31, p.muted);
		for (int r = 0; r < ROWS && ra->rtop + r < n; r++)
		{
			const std::string& k = rows[ra->rtop + r];
			bool sel = ra->rtop + r == at;
			float y = 82 + r * 64;
			st.box(sel ? "pick" : "slot", sel, 18, y, 604, 56);
			Color c = sel ? p.onPick : p.muted, vc = c;
			std::string label, v;
			float lx = 38;
			if (k == "account" || k.compare(0, 2, "r:") == 0)
			{
				label = k == "account" ? ra->user : k.substr(2);
				raAvatar(label, 30, y + 8, 40);
				lx = 82;
				if (k == "account") { if (const Ra::Points* q = pts(label)) v = points(q->score); }
				else if (ra->rmArmed == label) { v = "Press X again"; vc = sel ? p.onPickAccent : p.muted; }
				else v = "Signed out";
				if (k == "account") c = vc = sel ? p.onPick : p.ink;
			}
			else if (k == "signout")
			{
				label = "Sign out";
				if (ra->outArmed) { v = "Press A again"; vc = sel ? p.onPickAccent : p.muted; }
			}
			else label = "New account";
			st.text(st.fit(label, true, 20, 330), true, 20, lx, y + 16, c);
			if (v.empty()) continue;
			float vw = st.text(v, true, 19, 0, 0, p.ink, false, 0, true);
			st.text(v, true, 19, 586 - vw, y + 17, vc);
		}
		float bw = st.faceWidth("B", "Back");
		st.face("B", "Back", 18, bw);
		if (key.compare(0, 2, "r:") == 0)
		{
			float xw = st.faceWidth("X", "Remove");
			st.face("X", "Remove", 320 - xw / 2, xw);
		}
		std::string act = busy ? "" : key == "signout" ? (ra->outArmed ? "Confirm" : "Sign out") : key == "account" ? "" : "Sign in";
		if (!act.empty())
		{
			float aw = st.faceWidth("A", act);
			st.face("A", act, 622 - aw, aw);
		}
		be->endScreen();
	}
}
