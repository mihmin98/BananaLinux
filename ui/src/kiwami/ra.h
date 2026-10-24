#pragma once
// banana-ui (kiwami): the state of Settings > RetroAchievements' pages, kept by Settings::Kiwami while Settings lives:
// the account on the card (rastore.h), what the server last said of each account, the avatars on screen, and the one
// request running in the background (raclient.h: each blocks, up to 20 s). The Account page is ra-account.cpp.
#include "kiwami.h"
#include "raclient.h"
#include "rastore.h"
#include <functional>
#include <map>
#include <mutex>

namespace banana
{
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
		std::map<std::string, rnds::TexPtr> avatars;	// by name in lower case, null for none (the trophy instead)
	};
}
