// banana-ui (kiwami): the fork's rows and pages in Settings (kiwami.h).
#include "kiwami.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <unistd.h>

namespace banana
{
	using rnds::Color;

	// the handheld has an IPv4 address on a network of its own (Wi-Fi, a USB adapter): not the loopback, not
	// Tailscale's; KIWAMI_NETWORK=0 or 1 says so instead (the headless test, whatever the machine running it has)
	static bool hasNetwork()
	{
		if (const char* e = getenv("KIWAMI_NETWORK")) return strcmp(e, "0") != 0;
		ifaddrs* all = nullptr;
		if (getifaddrs(&all) != 0) return false;
		bool found = false;
		for (ifaddrs* a = all; a && !found; a = a->ifa_next)
			found = a->ifa_addr && a->ifa_addr->sa_family == AF_INET && (a->ifa_flags & IFF_UP) && !(a->ifa_flags & IFF_LOOPBACK)
				&& strcmp(a->ifa_name, "tailscale0") != 0;
		freeifaddrs(all);
		return found;
	}

	// a path for the shell, in single quotes
	static std::string quoted(const std::string& s)
	{
		std::string q = "'";
		for (char c : s) q += c == '\'' ? std::string("'\\''") : std::string(1, c);
		return q + "'";
	}

	void Settings::Kiwami::rows(Settings& s, const char* group)
	{
		if (!s.kw) s.kw = std::make_shared<Kiwami>(s);	// (at the first build(), in Settings' constructor)
		Item it;
		it.section = group; it.icon = "phone"; it.label = "Tailscale"; it.key = "kiwami.tailscale"; it.kind = Item::PAGE;
		it.help = "The handheld in your tailnet, Tailscale's private network of your devices: reachable from them wherever "
			"it is, and reaching them. A opens Tailscale's page: on or off, the login (a QR code to scan with your phone), "
			"the devices.";
		s.items.push_back(it);
	}

	std::string Settings::Kiwami::helper() const
	{
		const char* e = getenv("KIWAMI_TAILSCALE");
		std::string h = e && *e ? e : "/usr/libexec/kiwami/tailscale";
		return access(h.c_str(), X_OK) == 0 ? h : "";
	}

	bool Settings::Kiwami::tsOn() const
	{
		return getConfValue(st.data + "/settings.conf", "tailscale", "off") == "on";
	}

	void Settings::Kiwami::opened()
	{
		// switched off (or no helper in this image): nothing is run, the row says Off
		std::string h = helper();
		if (!tsOn() || h.empty())
		{
			ts = kiwami::TsStatus();
			show(h.empty() ? "no helper" : "switched off", "Off");
			return;
		}
		if (!job.running()) job.start(quoted(h) + " status", st.sync);
	}

	// the row's value, and a line in the log when the state changed (the tests read it)
	bool Settings::Kiwami::show(const std::string& state, const std::string& w)
	{
		std::string line = state + ": row \"" + w + "\"";
		if (line != logged) fprintf(stderr, "banana-ui: tailscale: %s\n", line.c_str());
		logged = line;
		bool changed = w != word;
		word = w;
		return changed;
	}

	bool Settings::Kiwami::update(double)
	{
		std::string out;
		if (!job.take(out)) return false;
		ts = kiwami::parseStatus(out);
		std::string state = "state " + (ts.backend.empty() ? std::string("unreadable") : ts.backend);
		if (!ts.self.ipv4.empty()) state += ", " + ts.self.ipv4;
		if (!ts.error.empty()) state += ", error \"" + ts.error + "\"";
		return show(state, kiwami::rowWord(ts, hasNetwork()));
	}

	int Settings::Kiwami::wake(double, bool) const
	{
		return job.running() ? 100 : -1;
	}

	std::string Settings::Kiwami::value(const std::string& key) const
	{
		return key == "kiwami.tailscale" ? word : "";
	}

	Settings::Result Settings::Kiwami::open(const std::string& key)
	{
		if (key != "kiwami.tailscale") return NONE;
		st.page = KIWAMI;
		st.armed = -1;
		return MOVED;
	}

	// ---- Tailscale's page (for now its title and state; its rows come with the submenu) ------------------------
	Settings::Result Settings::Kiwami::press(Key k)
	{
		switch (k)
		{
		case Key::B: case Key::LEFT: st.page = LIST; return MOVED;
		case Key::START: st.page = LIST; st.mOpen = false; return CLOSED;
		default: return NONE;
		}
	}

	Settings::Result Settings::Kiwami::touch(float, float)
	{
		// the B chip (where the press went down, as upstream's pages)
		if (st.downY >= 418 && st.downY <= 466 && st.downX < 320) return press(Key::B);
		return NONE;
	}

	void Settings::Kiwami::render()
	{
		Colours p = colours(st.light);
		rnds::Backend* be = st.be;
		be->beginScreen(0);
		st.box("screen", false, 0, 0, 640, 480);
		st.box("hero", false, 18, 20, 604, 436);
		st.icon("phone", 50, 56, 3);
		st.text("Network", true, 17, 170, 62, p.muted, true, 1.4f);
		st.text("Tailscale", true, 30, 170, 88, p.ink);
		st.text(st.fit(word, true, 22, 420), true, 22, 170, 130, p.accent);
		st.wrapped("Switching Tailscale on and off, the login and the devices come here.", 20, 50, 196, 540, 7, p.muted);
		be->endScreen();

		be->beginScreen(1);
		st.box("screen", false, 0, 0, 640, 480);
		st.box("bubble", false, 18, 14, 604, 54);
		st.text("Tailscale", true, 22, 38, 28, p.ink);
		float bw = st.faceWidth("B", "Back");
		st.face("B", "Back", 18, bw);
		be->endScreen();
	}

	Settings::Result Settings::Kiwami::entered(std::string&)
	{
		return NONE;
	}
}
