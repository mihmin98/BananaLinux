#pragma once
// banana-ui (kiwami): the fork's rows and pages in Settings. Settings::Kiwami is nested in upstream's Settings (one
// line in settings.h), so it reads the screen's state and draws with its own helpers (box, text, face...), and
// settings.cpp calls it through one-line hooks marked "kiwami:". Its rows' keys start with "kiwami."; its pages are
// Settings' page KIWAMI, and its texts typed on upstream's keyboard come back as ENTRY_KIWAMI.
//   Network > Tailscale (kiwami.tailscale): the state, from the Tailscale helper (/usr/libexec/kiwami/tailscale, or
//   KIWAMI_TAILSCALE in the environment: the test's stand-in), asked once when Settings opens and only while
//   settings.conf has tailscale = on
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
	};
}
