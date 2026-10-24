#pragma once
// banana-ui (kiwami): the RetroAchievements account on the card, for Settings' RetroAchievements pages. File work
// only, no screen and no network, so the unit test runs it on a made-up card:
//   <data>/accounts.conf          ra_user, ra_password, ra_retroarch_token: upstream's file (the setup writes it,
//                                 run-retroarch, run-drastic, run-bananads and ra-secrets read it), kept as key=value
//   <data>/ra-accounts            the fork's: "current\t<name>" and "remembered\t<name>" lines, most recent first, no
//                                 secrets
//   <data>/cache/ra-avatars/      <name in lower case>.png (raclient's avatar)
//   <data>/cache/ra-stats/        <name in lower case>: the Stats page's numbers and when they were fetched
//   <data>/cache/ra-hashes        each game identified: "<path>\t<size>\t<mtime>\t<hash>\t<id>" (a game's page)
//   <data>/cache/ra-badges/       <badge>.png, the achievements' pictures (upstream's game panel keeps them there too)
//   <data>/../retroarch/retroarch.cfg, <data>/../superdrastic/ra.token, <data>/../bananads/ra.token: what signing out
//                                 also clears
// <data> is Settings' data folder, /data/banana on the device. Names are compared without case, as the server does.
// Each change logs a "banana-ui: ra: ..." line with names only, never a password or a token.
#include "conf.h"
#include <string>
#include <vector>

namespace banana::kiwami::ra
{
	struct Card
	{
		std::string data, accounts, list, avatars, stats, hashes, badges, retroarch, superdrastic, bananads;
		explicit Card(const std::string& data);
	};

	// the account in accounts.conf
	struct Account { std::string user, password, token; };
	Account account(const Card& c);
	// ra-accounts: the account the fork last saw signed in ("" for none), and the signed-out ones, most recent first
	struct Accounts { std::string current; std::vector<std::string> remembered; };
	Accounts accounts(const Card& c);

	bool sameName(const std::string& a, const std::string& b);

	// each false with error set when a file couldn't be written; what was written before that stays, and a file that
	// couldn't be written is as it was

	// name signed in: accounts.conf's three keys (no ra_retroarch_token line when token is ""). Another account that
	// was current goes to the remembered list first; name becomes current and leaves the list
	bool signedIn(const Card& c, const std::string& name, const std::string& password, const std::string& token, std::string& error);
	// signing out, in this order: the scrub (KIWAMI_RA_SCRUB, else /usr/libexec/banana/ra-secrets scrub), waited for,
	// so that nothing it moves out of retroarch.cfg comes back afterwards; the three keys out of accounts.conf;
	// retroarch.cfg's cheevos_enable "false" and cheevos_username "" (when the file is there); both ra.token files
	// deleted; the account to the front of the remembered list
	bool signOut(const Card& c, std::string& error);
	// a remembered account off the list, with its avatar and its cached stats
	bool remove(const Card& c, const std::string& name, std::string& error);
	// accounts.conf's account changed behind the fork's back (the setup's step, a login in RetroArch's own menu, which
	// ra-secrets brings in): the old current account is remembered and the new one becomes current. true when it did
	bool adopt(const Card& c);
	// signed in with a password the server hasn't given a token for yet (offline, or the setup's step)
	bool needsToken(const Card& c);
	// the token for the game panel's account (retroarch.cfg's cheevos_username, which never has the token beside it)
	std::string panelToken(const Card& c, const std::string& user);

	// the Stats page's numbers for an account, kept for offline: per system in CONSOLES' order
	struct SystemStats { int started = 0, completed = 0, mastered = 0, unlocked = 0; };
	struct Stats
	{
		long long when = 0;		// when fetched, seconds since 1970; 0: none kept
		long score = 0, softScore = 0, messages = 0;
		SystemStats systems[9];
	};
	bool saveStats(const Card& c, const std::string& name, const Stats& s, std::string& error);
	Stats loadStats(const Card& c, const std::string& name);

	// a game's hash and RetroAchievements ID, kept by its path, size and modification time, so a game is hashed once and
	// again only when its file changes. id 0: not known yet (asked again: a hash the server doesn't know may be added)
	struct GameHash { std::string hash; unsigned id = 0; };
	bool cachedHash(const Card& c, const std::string& path, GameHash& h);	// false: none for the file as it is now
	bool cacheHash(const Card& c, const std::string& path, const GameHash& h, std::string& error);
}
