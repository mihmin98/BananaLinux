#pragma once
// banana-ui (kiwami): the state of Settings > RomM's pages, kept by Settings::Kiwami while Settings lives: RomM on the
// card as rommcard.h reads it (no network: its paths are the helper's, KIWAMI_ROMM_CARD and KIWAMI_RUN in the
// environment for the tests), the helper (/usr/libexec/kiwami/romm, or KIWAMI_ROMM in the environment: the test's
// build), and what the helper last said. The helper runs only from a RomM page, never when Settings opens. The Account
// page is romm-account.cpp, Sync now's romm-sync.cpp, Games' romm-games.cpp and BIOS' romm-bios.cpp.
#include "kiwami.h"
#include "rommcard.h"
#include "../wifi.h"

namespace banana
{
	struct Settings::Kiwami::Romm
	{
		kiwami::romm::Card card;
		std::string helper;					// "" when it isn't there
		kiwami::romm::Account account;		// accounts.conf's, as read when Settings or the page opened, and after each change
		bool unwrapped = false;				// the card's own systems.conf starts a synced system's games without romm-game

		// the helper's one run at a time, in the background (waited for in the headless test): check, pair, logout,
		// sync, download and bios (which go on detached once started), resolve, platforms or roms
		wifi::Job job;
		std::string verb;					// what it runs
		std::string check;					// check's last answer: ok <v>, newer <v>, old <v>, unpaired, revoked, down; "" not asked
		// a pairing: the status file's line, "pairing <code>\t<link>", "paired <version>" or "pair-failed <why>" ("" none),
		// read every 0.5 s while the page shows and it waits
		std::string pairing;
		double nextStatus = 0;

		// the Account page
		std::string selKey = "server";		// the selected row: server, pair, logout
		bool outArmed = false;				// Log out waiting for its second A
		std::string qrUrl;					// the pairing link the code is of
		rnds::TexPtr qr;					// one pixel a module, its quiet zone included; none once the page closes

		// Sync now, as read from the card when Settings opens, when its page opens or is left, and after each run
		kiwami::romm::Last last;			// the last sync's result
		std::vector<kiwami::romm::Conflict> conflicts;
		// the status file's line when it is a sync's ("running <done> <total>\t<system>\t<game>", "done sync ...",
		// "error sync <why>"), else ""; read every 0.5 s while the page shows and a sync runs
		std::string syncLine;
		bool syncing = false;				// a sync runs: the line says running, and the saves lock is held
		bool stopped = false;				// the line says running, and nothing holds the lock: it ended without a word
		std::string word;					// the row's value, as last logged

		// the Sync now page: Sync now, then the conflicts
		int ssel = 0, stop = 0;				// the selected row, the first one shown
		bool asking = false;				// a conflict's choice shown: Keep this card's (local) or Take RomM's
		bool takeRomm = false;
		bool armed = false;					// the choice waiting for its second A
		std::string note;					// what the last press led to (busy, refused...), on the top panel

		// Games: RomM's platforms that are Banana's systems' (romm platforms, run when the page opens), then a platform's
		// games, 50 a page (romm roms), searched with the keyboard
		struct Platform { long id = 0, count = 0; std::string slug, system, name; };
		struct Game { long id = 0; long long size = 0; bool on = false; std::string fsName, name; };
		std::vector<Platform> platforms;
		bool listed = false;				// the platforms came back
		bool inPlatform = false;			// a platform's games show, not the platforms
		int psel = 0, ptop = 0;				// the platforms' selected row, the first one shown
		std::vector<Game> games;			// the page of games shown
		long total = 0, offset = 0;			// the platform's games (searched), and the page's first
		std::string search;
		int gsel = 0, gtop = 0;				// the games' selected row (the last may be Next page), the first one shown
		std::string listError;				// romm platforms' or roms' error line ("" none)
		std::string gamesNote;				// what the last press led to, on the top panel

		// a download or the BIOS (the status file's line when it is one of theirs: "running <got> <total>\t<system>\t
		// <name>\t<note>", "running <done> <total>\tbios\t<file>", "done download|bios ...", "error download|bios ...";
		// else ""), read when Settings and the pages open, and every 0.5 s while the page shows and one runs
		std::string dlLine;
		bool downloading = false;			// the line says running, and the download lock is held
		std::string biosNote;				// the BIOS page's note
	};
}
