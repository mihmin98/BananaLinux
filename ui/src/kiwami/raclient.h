#pragma once
// banana-ui (kiwami): RetroAchievements' server, for Settings' RetroAchievements pages: the requests RetroArch's
// rcheevos makes to retroachievements.org's dorequest.php (POSTed, as rcheevos 12 sends them), their replies read
// with the fork's JSON reader, and the pictures from media.retroachievements.org. libcurl is dlopen()ed as upstream's
// account check does. Each call blocks (up to 20 s): the pages run them on a thread. Each request logs one line,
//   banana-ui: ra: <r> <HTTP status | no connection> <ms> ms
// and nothing else of it: never a password or a token. KIWAMI_RA_URL and KIWAMI_RA_MEDIA in the environment replace
// https://retroachievements.org and https://media.retroachievements.org (the tests' stand-in, ra-server.py).
#include <string>
#include <vector>

namespace banana::kiwami::ra
{
	// the Stats page's systems, in its order: Banana's system folder, RetroAchievements' console, the name
	struct Console { const char* system; unsigned id; const char* name; };
	extern const Console CONSOLES[9];

	// every reply: ok, the server answered yes; refused, it answered no (Success false, or 401/403: a wrong password, a
	// token no longer valid); neither, there was no answer that could be read (no connection, a timeout, a page that
	// isn't the server's JSON), worth trying again later. error says which, for the log and the page
	struct Reply { bool ok = false, refused = false; std::string error; };

	struct Login : Reply
	{
		std::string user, token, avatarUrl;			// user: the name as the server writes it
		long score = 0, softScore = 0, messages = 0;	// hardcore points, softcore points, unread messages
	};
	struct GameProgress { unsigned id = 0; int total = 0, unlocked = 0, unlockedHardcore = 0; };
	struct Progress : Reply
	{
		std::vector<GameProgress> games;
		// the system's sums: games started (any the server lists), completed (all unlocked, not all in hardcore), mastered
		// (all unlocked in hardcore), achievements unlocked (in either mode)
		int started = 0, completed = 0, mastered = 0, unlocked = 0;
	};
	struct GameId : Reply { unsigned id = 0; };		// 0: a hash the server doesn't know
	struct Achievement { unsigned id = 0; std::string title, description, badge; int points = 0; };
	struct Patch : Reply { std::string title; std::vector<Achievement> achievements; };	// core achievements only
	struct Unlocks : Reply { std::vector<unsigned> ids; };

	// the replies, from the body and the HTTP status (0: no answer)
	Login parseLogin(const std::string& body, long status);
	Progress parseProgress(const std::string& body, long status);
	GameId parseGameId(const std::string& body, long status);
	Patch parsePatch(const std::string& body, long status);
	Unlocks parseUnlocks(const std::string& body, long status);

	// the requests. login2 with the password, or with the token when password is ""
	Login login2(const std::string& user, const std::string& password, const std::string& token);
	Progress allProgress(const std::string& user, const std::string& token, unsigned console);
	GameId gameId(const std::string& hash);
	Patch patch(const std::string& user, const std::string& token, unsigned game);
	Unlocks unlocks(const std::string& user, const std::string& token, unsigned game, bool hardcore);
	// the pictures, into dir (made if missing): the file's path, "" when it couldn't be fetched. An avatar is
	// <dir>/<name in lower case>.png, from avatarUrl when the login gave one, else the media server's UserPic; a badge
	// is <dir>/<name>.png. Names that aren't plain (letters, digits, - and _) are refused
	std::string avatar(const std::string& user, const std::string& avatarUrl, const std::string& dir);
	std::string badge(const std::string& name, const std::string& dir);

	std::string escape(const std::string& s);		// for a form field: all but A-Z a-z 0-9 - . _ ~ as %XX
	std::string fileName(const std::string& name);	// a name for a file: in lower case, "" when it isn't plain
}
