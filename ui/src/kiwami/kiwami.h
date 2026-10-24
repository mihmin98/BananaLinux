#pragma once
// banana-ui (kiwami): the fork's rows and pages in Settings. Settings::Kiwami is nested in upstream's Settings (one
// line in settings.h), so it reads the screen's state and draws with its own helpers (box, text, face...), and
// settings.cpp calls it through one-line hooks marked "kiwami:". Its own rows' keys start with "kiwami." (the core
// rows are on upstream's core.<system>, handled by upstream as its own); its pages are Settings' page KIWAMI, and its
// texts typed on upstream's keyboard come back as ENTRY_KIWAMI.
//   Other systems > NES, Game Boy, Game Boy Color, Mega Drive (core.nes, core.gb, core.gbc, core.megadrive): upstream's
//   core or the fork's second one (Mesen, SameBoy, Genesis Plus GX), each row shown when its core is in
//   /usr/lib/libretro (or KIWAMI_LIBRETRO in the environment: the test's stand-in folder).
//   Network > Tailscale (kiwami.tailscale): the state, from the Tailscale helper (/usr/libexec/kiwami/tailscale, or
//   KIWAMI_TAILSCALE in the environment: the test's stand-in), asked once when Settings opens and only while
//   settings.conf has tailscale = on. A opens Tailscale's page (tailscale.cpp).
//   RetroAchievements (a seventh group, after System; KIWAMI_RA_GROUP=0 in the environment leaves it out, for the
//   tests): Account (kiwami.ra.account), Stats (kiwami.ra.stats), Games on this card (kiwami.ra.games) and Mode (upstream's cheevos_hardcore_mode_enable, the same setting as Other
//   systems > RetroAchievements, the two rows kept in step). Its tile has RetroAchievements' logo (ra-icon.cpp), and the
//   grid, three tiles a row, shows two rows at a time, scrolling to the third (grid.h). A on Account opens the Account
//   page (ra-account.cpp): sign in, sign out, and the accounts signed out, remembered. Stats (kiwami.ra.stats): points
//   and progress per system (ra-stats.cpp), fetched when the page opens or on Y, kept on the card for offline. Games on
//   this card (kiwami.ra.games): the library's games the emulators recorded achievements for (ra-games.cpp), A on one
//   its achievements, locked and unlocked (ra-game.cpp), the game identified by hashing it as RetroArch does.
#include "../settings.h"
#include <functional>
#include "grid.h"
#include "tsstatus.h"

namespace banana
{
	struct Settings::Kiwami
	{
		explicit Kiwami(Settings& s) : st(s) {}

		// the hooks, in settings.cpp's order
		struct Colours { rnds::Color ink, muted, line, accent, onPick, onPickAccent; };
		static Colours colours(bool light);			// the screen's palette (defined in settings.cpp, beside it)
		static void rows(Settings& s, const char* group);	// build(): the fork's rows in a group, after upstream's
		// gameOpen(): the game panel's token when retroarch.cfg has none (always: ra-secrets scrubs it out), from
		// accounts.conf, where Banana keeps it, for the panel's account
		static std::string panelToken(const std::string& user, const std::string& data);
		void opened();								// Settings opening: the rows' values, asked once
		bool owns(const std::string& key) const { return key.compare(0, 7, "kiwami.") == 0; }
		Result open(const std::string& key);		// A on a row of the fork's: its page
		Result press(Key k);						// on the fork's pages
		int gridMove(int g, int n, bool up) const { return kiwami::gridMove(g, n, up); }	// UP or DOWN on the groups
		Result touch(float cx, float cy);
		int gridTop(int gsel, int n);				// the groups' touch and drawing: the first of the two rows shown
		rnds::Bitmap icon(const std::string& name) const;	// an icon the theme has no PNG of: the fork's, or empty
		void render();
		std::string value(const std::string& key) const;	// a row's value in the list
		bool update(double t);						// true: something to draw again
		int wake(double t, bool shown) const;		// ms until update() has something to do, -1 nothing
		std::string entryHelp() const;				// the keyboard's help, typing for the fork's pages
		Result entered(std::string& text);			// the keyboard's Done, for the fork's pages

	private:
		Settings& st;
		// the page that shows while Settings' page is KIWAMI: open() sets it, and the page hooks go to its own functions
		enum class Page { NONE, TAILSCALE, RA_ACCOUNT, RA_STATS, RA_GAMES, RA_GAME } pageOpen = Page::NONE;
		int row0 = 0;								// the groups' first row shown, kept as the selection moves
		int modeAt = -1;							// Mode's value as both its rows last had it
		bool syncMode();							// a change to one of Mode's two rows shown in the other

		// Tailscale's part of the hooks (kiwami.cpp: the row's value, asked in the background)
		void tsOpened();
		bool tsUpdate(double t);
		int tsWake(double t, bool shown) const;
		std::string helper() const;					// the Tailscale helper, "" when there is none
		bool tsOn() const;							// settings.conf's tailscale = on
		wifi::Job job;								// the helper's status, in the background
		kiwami::TsStatus ts;
		std::string word;							// the Tailscale row's value
		std::string logged;							// the last state written to the log
		bool show(const std::string& state, const std::string& w);

		// Tailscale's page (tailscale.cpp): its rows (Tailscale on / off, Devices, Device name, Log out) on the bottom
		// panel, the state and the selected row's help on the top one; the helper's status every 2 s while it shows.
		// Devices: the tailnet's, on the bottom panel, the selected one in full on the top one
		Result tsOpen();
		Result tsPress(Key k);
		Result tsTouch(float cx, float cy);
		void tsRender();
		std::string tsEntryHelp() const;
		Result tsEntered(std::string& text);
		enum View { MENU, DEVICES } view = MENU;
		std::string tool;							// the helper, as found when Settings opened
		bool on = false;							// tailscale = on, as read then or set since
		std::string selKey = "on";					// the selected row
		bool armed = false;							// Log out waiting for its second A
		double nextPoll = 0;
		int dsel = 0, dtop = 0;						// the devices page: the selected row, the first one shown
		std::string dname;							// the selected device (the selection follows it as the list changes)
		void devicesOpened();
		std::vector<kiwami::TsDevice> devices();	// the list now, dsel and dtop kept on the selected device
		std::string model;							// the device tree's model, read once
		bool modelRead = false;
		std::vector<std::string> menuRows() const;	// the rows' keys: on, devices, name, logout
		bool loggedIn() const;
		std::string name();							// tailscale_name, else the board's
		void run(const char* verb, const char* arg = nullptr);	// the helper, detached (waited for in the headless test)
		void setOn(bool v);
		Result change(int dir);
		Result pressMenu(Key k);
		Result pressDevices(Key k);
		Result touchDevices(float cy);
		void renderTop(const std::string& title, const std::string& accent, const std::string& state, const std::string& help);
		// the login: its link's QR code on the left of the top panel, the rest beside and under it
		std::string qrUrl;							// the link the code is of
		rnds::TexPtr qrTex;							// one pixel a module, its quiet zone included; none once the page closes
		bool qrFor(const std::string& url);
		void dropQr() { qrTex.reset(); qrUrl.clear(); }
		void renderLogin(const std::string& title, const std::string& accent, const std::string& state, const std::string& help,
			const std::string& link, const std::string& foot);
		void renderMenu();
		void renderDevices();
		float para(const std::string& t, float top, int maxLines, const rnds::Color& c, float x = 50, float width = 540);

		// RetroAchievements' pages (ra.h: their state, made when Settings first opens with the group). Their part of the
		// hooks: the account adopted when Settings opens, the Account row's value, the request's answer taken in
		struct Ra;
		std::shared_ptr<Ra> ra;
		void raOpened();
		std::string raValue(const std::string& key) const;
		bool raUpdate();
		int raWake() const;
		void raRead();								// the account and the list, from the card
		void raRun(std::function<void()> work, std::function<void()> then, bool signingIn);	// a request (Ra::task)
		// the Account page (ra-account.cpp): the account signed in (avatar, name, points) and Sign out, the remembered
		// accounts, New account, on the bottom panel; the selected one in full on the top one
		Result raOpen();
		Result raPress(Key k);
		Result raTouch(float cx, float cy);
		void raRender();
		std::string raEntryHelp() const;
		Result raEntered(std::string& text);
		std::vector<std::string> raRows() const;
		int raSelected(const std::vector<std::string>& rows);	// the selected row's index, the scroll kept on it
		void raSignIn(const std::string& name, const std::string& password);
		void raFetchToken();						// a password with no token yet: the server's token for it
		void raSignOut();
		void raRemove(const std::string& name);
		void raAvatar(const std::string& name, float x, float y, float size);	// its picture, or the logo
		void raTop(const std::string& title, const std::string& accent, const std::string& state, const std::string& help,
			const std::string& avatar);
		// the Stats page (ra-stats.cpp): the account's points and messages on the top panel, the nine systems' progress on
		// the bottom one; fetched when it opens and on Y (login2, the avatar, allprogress per system), else the card's
		Result raStatsOpen();
		Result raStatsPress(Key k);
		Result raStatsTouch(float cx, float cy);
		void raStatsRender();
		void raStatsFetch();
		// the games on this card (ra-games.cpp): the games with achievements the library has (kiwami/library.h), their
		// progress, the last played first
		void raGamesRead();
		Result raGamesOpen();
		Result raGamesPress(Key k);
		Result raGamesTouch(float cx, float cy);
		void raGamesRender();
		// a game's achievements (ra-game.cpp), A on one of the games on this card: the game identified (rahash.h, its
		// hash and ID kept on the card), its achievements and the account's unlocks asked, the badges fetched once
		Result raGameOpen(int i);
		Result raGamePress(Key k);
		Result raGameTouch(float cx, float cy);
		void raGameRender();
		void raGameLoad();
		void raGameBadges();
		void raBadge(const std::string& name, float x, float y, float size, bool dim);
	};
}
