#pragma once
// banana-ui (kiwami): RomM on the card, as the RomM helper (src/kiwami/romm/) and Settings' RomM pages both read it.
// File work only, no network, so Settings reads it when it opens and the unit test runs it on a made-up card:
//   <card>/banana/accounts.conf     romm_url, romm_token, romm_device_id beside the other accounts' keys (upstream's
//                                   file, which file sharing hides); the helper writes the token only when pairing
//                                   and logging out
//   <card>/banana/settings.conf     romm, romm_autosync, romm_target (else <etc>/settings.conf's)
//   <card>/banana/romm/identifier   the handheld's id for RomM (client_device_identifier), made once
//   <card>/banana/romm/pending      what didn't reach RomM yet: "push\t<system>\t<rom>" and
//                                   "play\t<system>\t<rom>\t<start>\t<end>" lines, each once
//   <card>/banana/romm/conflicts    the games whose save changed on both sides, one line each:
//                                   "<system>\t<rom>\t<rom id>\t<save id>\t<RomM's time>\t<noted>"
//   <card>/banana/romm/last         the last sync's result: "<time>\t<state>\t<sent>\t<fetched>\t<conflicts>\t<skipped>"
//   <card>/banana/romm/synced       each game's save at its last sync, "<rom id>\t<hash>[\t<RomM's hash>]": a save that
//                                   differs from it changed here since (RomM keeps its record per save, and misses it
//                                   when another device's newer save is a new one). RomM's hash is there when the
//                                   save taken from RomM isn't the bytes RomM hashed (a zip, a DS save nds-saves
//                                   rewrote): the save stands for it while it is unchanged
//   <card>/banana/logs/romm.log     the helper's log
//   <run>/romm.status               the helper's state, one line
// The paths are moved for the tests by KIWAMI_ROMM_CARD (/data), KIWAMI_RUN (/run/banana), KIWAMI_ROMM_ETC
// (/etc/banana), KIWAMI_LIBRETRO (/usr/lib/libretro) and KIWAMI_ROMM_NDS_SAVES (/usr/libexec/banana/nds-saves).
#include <string>
#include <vector>

namespace banana::kiwami::romm
{
	struct Card
	{
		std::string card, data, accounts, settings, dir, logs, status, run, etc, libretro, ndsSaves;
		Card();		// from the environment
	};

	struct Account { std::string url, token, deviceId; };
	Account account(const Card& c);

	// a server as typed: spaces round it dropped, http:// added when it has no scheme, a trailing / removed
	std::string normalServer(const std::string& url);
	// each false with error set when accounts.conf couldn't be written; every other line of it is kept
	bool setServer(const Card& c, const std::string& url, std::string& error);			// romm_url, normalised
	bool paired(const Card& c, const std::string& token, const std::string& deviceId, std::string& error);
	bool forget(const Card& c, std::string& error);									// the token and the device id out
	// the server's host (and port), for the Account row: "" with no server
	std::string host(const std::string& url);

	// settings.conf's value: the card's, else the image's, else dflt (run-retroarch's setting())
	std::string setting(const Card& c, const std::string& key, const std::string& dflt);

	// the handheld's id for RomM, 16 hex digits made once and kept in <card>/banana/romm/identifier; "" when it
	// couldn't be kept
	std::string deviceIdentifier(const Card& c);

	bool makeDir(const std::string& dir);

	// each false with error set when the file couldn't be written; each written through replaceFile
	struct Pending
	{
		struct Push { std::string system, rom; };
		struct Play { std::string system, rom; long long start = 0, end = 0; };
		std::vector<Push> pushes;
		std::vector<Play> plays;
		bool empty() const { return pushes.empty() && plays.empty(); }
	};
	Pending pending(const Card& c);
	// lines added (each once) or taken out, under a lock of their own (<run>/romm-pending.lock): romm pending adds to
	// it while a push may be sending it
	bool addPending(const Card& c, const Pending& add, std::string& error);
	bool removePending(const Card& c, const Pending& done, std::string& error);

	struct Conflict { std::string system, rom; long romId = 0, saveId = 0; std::string serverTime; long long noted = 0; };
	std::vector<Conflict> conflicts(const Card& c);
	bool noteConflict(const Card& c, const Conflict& k, std::string& error);	// replaces the game's line
	bool clearConflict(const Card& c, const std::string& system, const std::string& rom, std::string& error);

	// the hash of the game's save at its last sync ("" for none), and RomM's hash it stands for ("" when it is the
	// save's own); "" as hash takes the record out
	std::string syncedHash(const Card& c, long romId);
	std::string syncedStandsFor(const Card& c, long romId);
	bool setSynced(const Card& c, long romId, const std::string& hash, std::string& error, const std::string& standsFor = "");

	// the state: synced, conflicts, down or revoked
	struct Last { long long when = 0; std::string state; int sent = 0, fetched = 0, conflicts = 0, skipped = 0; };
	Last last(const Card& c);
	bool setLast(const Card& c, const Last& l, std::string& error);
}
