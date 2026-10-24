#pragma once
// banana-ui (kiwami): the state of Settings > RetroAchievements' pages, kept by Settings::Kiwami while Settings lives:
// the account on the card (rastore.h), what the server last said of each account, the avatars on screen, and the one
// request running in the background (raclient.h: each blocks, up to 20 s). The Account page is ra-account.cpp, the
// Stats page ra-stats.cpp, the games on this card ra-games.cpp, a game's achievements ra-game.cpp.
#include "kiwami.h"
#include "raclient.h"
#include "rastore.h"
#include <atomic>
#include <functional>
#include <map>
#include <ctime>
#include <mutex>

namespace banana
{
	inline std::string raPoints(long n) { return std::to_string(n) + (n == 1 ? " point" : " points"); }

	struct Settings::Kiwami::Ra
	{
		explicit Ra(const std::string& data) : card(data) {}
		kiwami::ra::Card card;
		// the card as last read (when Settings opens, when the page opens, after each change): accounts.conf's account
		// (the list's row shows it) and whether it has a token, and ra-accounts' remembered ones
		std::string user;
		bool token = false;
		std::vector<std::string> remembered;
		// each account's points, by name in lower case: the last login's, else the Stats page's kept on the card
		struct Points { long score = 0, softScore = 0, messages = 0; };
		std::map<std::string, Points> points;

		// the request in the background: its work on a thread (inline in the headless test), then what it found taken in
		// by update() on the menu's thread. A sign-in holds the page's A and X until it is done; the token fetched when the
		// page opens doesn't, and a sign-in started meanwhile replaces it (its answer is dropped)
		struct Task { std::mutex mu; bool done = false; };
		std::shared_ptr<Task> task;
		std::function<void()> then;
		bool signingIn = false;

		// the Account page
		std::string selKey;					// the selected row: account, signout, r:<name> (remembered), new
		int rtop = 0;						// the first row shown
		bool outArmed = false;				// Sign out waiting for its second A
		std::string rmArmed;				// the remembered account waiting for its second X
		enum Typing { NAME, PASSWORD } typing = NAME;	// what the keyboard is typing for the page
		std::string typedName;				// the account the password is for
		std::map<std::string, rnds::TexPtr> avatars;	// by name in lower case, null for none (the logo instead)

		// the Stats page: the numbers shown, fetched now or kept on the card (cached: as of stats.when)
		kiwami::ra::Stats stats;
		bool statsCached = false;
		std::string statsError;				// why the last fetch failed, "" when it didn't
		int ssel = 0, stop = 0;				// the selected system, the first one shown

		// the games on this card: the library's games with achievements, copied when Settings or the page opens (a
		// rescan can't take them from under the page), the last played first
		struct Game
		{
			std::string title, system, systemName, path, rel;
			std::vector<std::string> exts;	// the system's (an archive's game is picked by them)
			int unlocked = 0, total = 0;
			time_t last = 0;
		};
		std::vector<Game> games;
		int gsel = 0, gtop = 0;

		// a game's achievements, A on one of the games on this card: the game, then what its request found (the hash and
		// the ID, kept on the card; the list and the account's unlocks), the badges on screen
		Game game;
		int gameRun = 0;					// the page's openings: an answer for an earlier one is dropped
		bool loading = false;				// its request running
		std::shared_ptr<std::atomic<int>> phase;	// what that request is doing: IDENTIFYING, LOADING, BADGES
		enum { IDENTIFYING, LOADING, BADGES };
		std::string gameState;				// why there is no list, "" while loading or when there is one
		std::string gameHash, gameTitle;
		unsigned gameId = 0;
		struct Cheevo { kiwami::ra::Achievement a; bool unlocked = false, hardcore = false; };
		std::vector<Cheevo> cheevos;		// unlocked first, then locked, each in the set's order
		int unlocked = 0, hardcore = 0;
		int asel = 0, atop = 0;
		std::map<std::string, rnds::TexPtr> badges;	// by badge name, null for one not on the card (a trophy instead)
	};
}
