#pragma once
// The RomM helper (kiwami): a game on the card's RomM id, which RomM pairs saves by.
//   1. <card>/banana/romm/games, "<system>\t<file name>\t<rom id>" lines: the games downloaded from RomM, and the
//      matches found before
//   2. the platform's list, cached in <card>/banana/romm/platforms/<slug>.tsv ("<id>\t<fs_name>\t<size>\t<name>"
//      lines; GET /api/platforms, then the platform's games 500 a page): the game's file name against fs_name,
//      exactly and then without case, and for a game in a folder of its own, the folder's name
//   3. on a miss, the list fetched again, once per run of the helper, when refetch is allowed (not by the pull before
//      a game, which only fetches a list none is cached for: a platform's list is heavy)
// A match is written to games.
#include "api.h"
#include "saves.h"
#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace banana::kiwami::romm
{
	// the names a game goes by on the card: its file's, then its folder's when it is in a folder of its own (not the
	// system's)
	std::vector<std::string> namesOf(const System& s, const std::string& rom);
	// a name's game in a list: fs_name exactly, then without case; 0 when none
	long matchRom(const std::vector<Rom>& list, const std::string& name);

	struct Games
	{
		// before: called before each request, with the server to set its timeouts; false: no time left, no request
		Games(const Card& c, Server& s, bool refetch, std::function<bool(Server&)> before = nullptr);
		long idOf(const System& s, const std::string& rom);	// 0: not on RomM, or not known now
		bool down = false;		// a list couldn't be fetched (no answer, or no time left)
		long status = 0;		// the last answer's HTTP status (401: the token refused)

	private:
		const Card& card;
		Server& server;
		bool refetch;
		std::function<bool(Server&)> before;
		bool platformsFetched = false;
		std::vector<Platform> platformList;
		std::set<std::string> fetched;							// the slugs fetched this run
		std::map<std::string, std::vector<Rom>> lists;			// by slug, as cached
		bool cached(const std::string& slug);
		bool fetch(const Synced& m);
		long lookUp(const Synced& m, const std::vector<std::string>& names);
		void record(const std::string& system, const std::string& name, long id);
	};
}
