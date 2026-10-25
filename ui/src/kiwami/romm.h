#pragma once
// banana-ui (kiwami): the state of Settings > RomM's pages, kept by Settings::Kiwami while Settings lives: RomM on the
// card as rommcard.h reads it (no network: its paths are the helper's, KIWAMI_ROMM_CARD and KIWAMI_RUN in the
// environment for the tests), the helper (/usr/libexec/kiwami/romm, or KIWAMI_ROMM in the environment: the test's
// build), and what the helper last said. The helper runs only from a RomM page, never when Settings opens. The Account
// page is romm-account.cpp.
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

		// the helper's one run at a time, in the background (waited for in the headless test): check, pair or logout
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
	};
}
