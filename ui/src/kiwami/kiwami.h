#pragma once
// banana-ui (kiwami): the fork's rows and pages in Settings. Settings::Kiwami is nested in upstream's Settings (one
// line in settings.h), so it reads the screen's state and draws with its own helpers (box, text, face...), and
// settings.cpp calls it through one-line hooks marked "kiwami:". Its rows' keys start with "kiwami."; its pages are
// Settings' page KIWAMI, and its texts typed on upstream's keyboard come back as ENTRY_KIWAMI.
//   Network > Tailscale (kiwami.tailscale): the state, from the Tailscale helper (/usr/libexec/kiwami/tailscale, or
//   KIWAMI_TAILSCALE in the environment: the test's stand-in), asked once when Settings opens and only while
//   settings.conf has tailscale = on. A opens Tailscale's page (tailscale.cpp).
#include "../settings.h"
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
		void opened();								// Settings opening: the rows' values, asked once
		bool owns(const std::string& key) const { return key.compare(0, 7, "kiwami.") == 0; }
		Result open(const std::string& key);		// A on a row of the fork's: its page
		Result press(Key k);						// on the fork's pages
		Result touch(float cx, float cy);
		void render();
		std::string value(const std::string& key) const;	// a row's value in the list
		bool update(double t);						// true: something to draw again
		int wake(double t, bool shown) const;		// ms until update() has something to do, -1 nothing
		std::string entryHelp() const;				// the keyboard's help, typing for the fork's pages
		Result entered(std::string& text);			// the keyboard's Done, for the fork's pages

	private:
		Settings& st;
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
	};
}
