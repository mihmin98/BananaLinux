#pragma once
// banana-ui (kiwami): the fork's rows and pages in Settings. Settings::Kiwami is nested in upstream's Settings (one
// line in settings.h), so it reads the screen's state and draws with its own helpers (box, text, face...), and
// settings.cpp calls it through one-line hooks marked "kiwami:". Its own rows' keys start with "kiwami."; its pages are
// Settings' page KIWAMI, and its texts typed on upstream's keyboard come back as ENTRY_KIWAMI.
//   Other systems > Emulators: the fork's second cores are choices in upstream's own core rows (a row per system with
//   two cores installed): Mesen and Genesis Plus GX are in upstream's list already, SameBoy joins the Game Boy's and
//   Game Boy Color's through kiwami::moreCores (its kiwami: line in coreChoices). (With Frontend on Auto, upstream's
//   BananaFront runs them, as it does upstream's cores for those systems.)
//   Settings' order: upstream's build() sorts tiles and rows by its ORDER table; Kiwami::place (a kiwami: line in its
//   place) gives the fork's tile and rows the place of the one each follows.
//   Network > Tailscale (kiwami.tailscale), after Bluetooth: the state, from the Tailscale helper (/usr/libexec/kiwami/tailscale, or
//   KIWAMI_TAILSCALE in the environment: the test's stand-in), asked once when Settings opens and only while
//   settings.conf has tailscale = on. A opens Tailscale's page (tailscale.cpp).
//   RetroAchievements (a seventh group, the tile after System; KIWAMI_RA_GROUP=0 in the environment leaves it out, for the
//   tests): Account (kiwami.ra.account), Stats (kiwami.ra.stats), Games on this card (kiwami.ra.games) and Mode (upstream's cheevos_hardcore_mode_enable, the same setting as Other
//   systems > RetroAchievements, the two rows kept in step). Its tile has RetroAchievements' logo (ra-icon.cpp), and the
//   grid, three tiles a row, shows two rows at a time, scrolling to the third (grid.h). A on Account opens the Account
//   page (ra-account.cpp): sign in, sign out, and the accounts signed out, remembered. Stats (kiwami.ra.stats): points
//   and progress per system (ra-stats.cpp), fetched when the page opens or on Y, kept on the card for offline. Games on
//   this card (kiwami.ra.games): the library's games the emulators recorded achievements for (ra-games.cpp), A on one
//   its achievements, locked and unlocked (ra-game.cpp), the game identified by hashing it as RetroArch does.
//   Network > Home server > Show sync times (kiwami_sync_times in settings.conf, after Forget this server): while a sync runs and
//   Sync now is selected, a line under its bar with the phase, its time, its time left and the sync's time
//   (sync-times.cpp, from banana-sync's sync.times: synctimes.h), Settings waking each second for it.
//   Network > Home server > Installs from RomM (kiwami_romm_installs in settings.conf, a paired RomM 5.4 or newer's,
//   after Show sync times, Off by default): games sent to this handheld from RomM's web page come at the next sync
//   (banana-sync's romm/installs.h). A change to it is told to RomM (kiwami installs on|off) in the background; a
//   failed On is put back, a failed Off kept (banana-sync stops claiming whatever RomM says).
//   Network > Home server > BIOS (kiwami.romm.bios, a paired RomM's, after Installs from RomM): RomM's BIOS files for
//   Banana's systems, ticked to download into the card's bios folder (romm-bios.cpp, through banana-sync's kiwami bios
//   verbs: romm/bios.h).
//   Other systems > Second screen > Status on the game panel (kiwami_panel_status in settings.conf, after Game panel
//   idle time, Off by default): read at each game's start. On, the game panel's tiles view draws a strip where its
//   hint line was (panel-status.cpp, its state in panelstatus.h): Wi-Fi and Tailscale's mark on the left, the time
//   played this game in the centre, the battery, the date and the time on the right; with them on the left, Power
//   scaling's mode (Auto, or the system's clock profile) and the CPU's MHz, read every 2 s. settings_gamepanel.cpp
//   calls it through four kiwami: lines (gameOpen, gameUpdate, the hint in gameFrame); the keyboard and the companions' pages
//   are drawn before the hint and return, as without it.
#include "../settings.h"
#include <functional>
#include "grid.h"
#include "panelstatus.h"
#include "synctimes.h"
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
		using Order = std::vector<std::pair<std::string, std::vector<std::string>>>;
		static int place(const Order& order, const std::string& section, const std::string& name);	// build()'s ORDER: the fork's own place, -1 none
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
		void syncTimes(float x, float y, float w);	// syncBar(): the line under Sync now's bar
		void panelOpened();							// gameOpen(): the status strip's setting read, and its game started
		bool panelUpdate(double t);					// gameUpdate(): true when the strip has something new to draw
		bool panelStatus();							// gameFrame(): the strip drawn (true), or upstream's hint to draw

	private:
		Settings& st;
		// the page that shows while Settings' page is KIWAMI: open() sets it, and the page hooks go to its own functions
		enum class Page { NONE, TAILSCALE, RA_ACCOUNT, RA_STATS, RA_GAMES, RA_GAME, ROMM_BIOS } pageOpen = Page::NONE;
		int row0 = 0;								// the groups' first row shown, kept as the selection moves
		int modeAt = -1;							// Mode's value as both its rows last had it
		bool syncMode();							// a change to one of Mode's two rows shown in the other

		// Show sync times' part of the hooks (sync-times.cpp): the line under Sync now's bar, worked out from the status
		// and banana-sync's sync.times, and the wakes that keep its clocks ticking
		bool timesOn() const;						// the Show sync times row is On
		bool timesShown() const;					// on, the list on Sync now, a sync running: where syncBar() draws
		std::string timesRun() const;				// KIWAMI_SYNC_RUN, else /run/banana
		bool timesUpdate(double t);
		int timesWake(double t) const;
		std::string timesSeen;						// the status line sync.times was last read for
		kiwami::synctimes::Times times;
		bool timesValid = false;					// the file's pid is sync.pid's
		std::string timesText, timesShape;			// the line, and its shape last logged

		// Home server > BIOS (romm-bios.cpp): banana-sync's kiwami bios list on the bottom panel, the files under their
		// systems, the missing ones ticked; Start downloads the ticked ones (kiwami bios get), then the list is asked
		// again. The top panel: the selected file's state, and the last download's result
		struct BiosFile { std::string id, system, name, state; long long bytes = -1; bool on = false; };
		std::vector<BiosFile> bios;
		int bsel = 0, btop = 0;						// the selected file, the first row shown
		enum class BiosAsk { NONE, LIST, GET } biosAsk = BiosAsk::NONE;	// what biosJob runs
		wifi::Job biosJob;
		std::string biosNote;						// why there's no list, else ""
		std::string biosDone;						// the last download's result, as said on the top panel
		// a download's progress, from banana-sync's <run>/kiwami-bios.status (romm/bios.h): file at of of (at 0: none
		// yet), the bytes before it of all; and the ids asked for, in their order (the rows' Done, Getting, Waiting)
		struct BiosProgress { int at = 0, of = 0; long long done = 0, all = 0; std::string name; };
		BiosProgress biosProg;
		std::vector<std::string> biosAsked;
		bool biosProgress();						// the status file read again while a download runs: true when it moved
		Result biosOpen();
		Result biosPress(Key k);
		Result biosTouch(float cx, float cy);
		void biosRender();
		bool biosUpdate();
		void biosRun(BiosAsk what, const std::string& args);
		void biosListed(const std::string& out);	// a list's answer taken in, the selection kept on its file
		std::vector<int> biosRows() const;			// the bottom panel's rows: a file's index, or -1 - the index of its system's first file
		void biosScroll();							// btop kept so that the selected file (and its system's head) shows

		// Home server > Installs from RomM (kiwami_romm_installs, kiwami.cpp): a change to the row, which upstream's
		// Settings writes to settings.conf, told to RomM by banana-sync's kiwami installs on|off; a failed On put back
		wifi::Job installsJob;
		int installsAt = -1;						// the row's value as last seen (-1: not yet)
		bool installsOn = false;					// what installsJob asks for
		bool installsUpdate();
		void installsFailed(const std::string& why);	// said, and an On put back

		// Status on the game panel's strip (panel-status.cpp): its state, made at the first game with the setting on
		bool panelOn = false;						// kiwami_panel_status = on at this game's start
		time_t panelOpenedAt = 0;					// this game's start (time(nullptr)): a mode file older by 5 s is a past game's
		std::unique_ptr<kiwami::PanelStatus> panel;
		std::string panelTrace;						// KIWAMI_PANEL_TRACE's last line (the parts' places)
		rnds::TexPtr panelSprite(const std::string& name, bool big);	// wifi, battery (@1x or @1.6x) or the mark (tsmark)
		void panelBattery(float bx, float by, const kiwami::PanelReadings& r, const rnds::Color& level, const rnds::Color& ink,
			const rnds::Color& edge);				// the battery sprite's level and bolt, as the engine's status bar draws them

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

	namespace kiwami
	{
		// upstream's coreChoices (Settings > Other systems: a system's installed cores, values and names in step), then:
		// SameBoy for the Game Boy and Game Boy Color when it is installed (the image's /usr/lib/libretro or the Core
		// Downloader's /data/retroarch/cores), after upstream's; and with KIWAMI_LIBRETRO in the environment (the headless
		// test's stand-in folder) upstream's own cores found there count as installed, in upstream's order
		void moreCores(const std::string& system, const std::vector<std::pair<const char*, const char*>>& upstream,
			std::vector<std::string>& names, std::vector<std::string>& values);
	}
}
