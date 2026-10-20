// banana-ui (kiwami): the fork's rows and pages in Settings (kiwami.h): the rows and the hooks' plumbing; the pages
// are in their own files (tailscale.cpp).
#include "kiwami.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/wait.h>
#include <unistd.h>

namespace banana
{
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
		tool = helper();
		on = tsOn();
		dropQr();
		if (!on || tool.empty())
		{
			ts = kiwami::TsStatus();
			show(tool.empty() ? "no helper" : "switched off", "Off");
			return;
		}
		if (!job.running()) job.start(quoted(tool) + " status", st.sync);
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

	bool Settings::Kiwami::update(double t)
	{
		// Tailscale's page asks again every 2 s while it shows
		bool shown = st.page == KIWAMI;
		if ((!shown || view != MENU) && qrTex) dropQr();	// (left another way than B or START: the keyboard, Devices)
		if (shown && on && !tool.empty() && t >= nextPoll && !job.running())
		{
			job.start(quoted(tool) + " status", st.sync);
			nextPoll = t + 2000;
		}
		std::string out;
		if (!job.take(out) || !on) return false;	// (an answer from before it was switched off: dropped)
		ts = kiwami::parseStatus(out);
		std::string state = "state " + (ts.backend.empty() ? std::string("unreadable") : ts.backend);
		if (!ts.self.ipv4.empty()) state += ", " + ts.self.ipv4;
		if (!ts.error.empty()) state += ", error \"" + ts.error + "\"";
		return show(state, kiwami::rowWord(ts, hasNetwork())) || shown;
	}

	int Settings::Kiwami::wake(double t, bool shown) const
	{
		if (job.running()) return 100;
		if (shown && on && !tool.empty()) return std::max(0, (int)(nextPoll - t));
		return -1;
	}

	std::string Settings::Kiwami::value(const std::string& key) const
	{
		return key == "kiwami.tailscale" ? word : "";
	}

	// a program of the fork's, its own session; detached so that it outlives a game the menu starts next, waited for
	// in the headless test (so what it does is done before the next command)
	void Settings::Kiwami::run(const char* verb, const char* arg)
	{
		if (tool.empty()) return;
		fprintf(stderr, "banana-ui: tailscale: run %s%s%s\n", verb, arg ? " " : "", arg ? arg : "");
		pid_t p = fork();
		if (p == 0)
		{
			setsid();
			if (!st.sync && fork() != 0) _exit(0);
			execl(tool.c_str(), tool.c_str(), verb, arg, (char*)nullptr);
			_exit(127);
		}
		if (p > 0) waitpid(p, nullptr, 0);
	}
}
