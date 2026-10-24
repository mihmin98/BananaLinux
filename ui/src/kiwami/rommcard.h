#pragma once
// banana-ui (kiwami): RomM on the card, as the RomM helper (src/kiwami/romm/) and Settings' RomM pages both read it.
// File work only, no network, so Settings reads it when it opens and the unit test runs it on a made-up card:
//   <card>/banana/accounts.conf     romm_url, romm_token, romm_device_id beside the other accounts' keys (upstream's
//                                   file, which file sharing hides); the helper writes the token only when pairing
//                                   and logging out
//   <card>/banana/settings.conf     romm, romm_autosync, romm_target (else <etc>/settings.conf's)
//   <card>/banana/romm/identifier   the handheld's id for RomM (client_device_identifier), made once
//   <card>/banana/logs/romm.log     the helper's log
//   <run>/romm.status               the helper's state, one line
// The paths are moved for the tests by KIWAMI_ROMM_CARD (/data), KIWAMI_RUN (/run/banana), KIWAMI_ROMM_ETC
// (/etc/banana), KIWAMI_LIBRETRO (/usr/lib/libretro) and KIWAMI_ROMM_NDS_SAVES (/usr/libexec/banana/nds-saves).
#include <string>

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
}
