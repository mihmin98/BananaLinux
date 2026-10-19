// banana-ui (kiwami): Tailscale's page, A on Settings > Network > Tailscale. The bottom panel has its rows in the
// Settings list's look: Tailscale ◀ On / Off ▶ (settings.conf's tailscale, and the helper's up or down), Devices (when
// connected), Device name (upstream's keyboard; settings.conf's tailscale_name, and the helper's name), Log out (when
// logged in; A twice). The top panel says the state, whichever row is selected, then that row's help. The helper's
// status is asked every 2 s while the page shows, never once it is closed.
#include "kiwami.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>

namespace banana
{
	using rnds::Color;
	using S = kiwami::TsStatus;

	std::vector<std::string> Settings::Kiwami::menuRows() const
	{
		bool devices = on && ts.state == S::RUNNING, logout = loggedIn();
		std::vector<std::string> r;
		for (const char* k : { "on", "devices", "name", "logout" })
			if ((strcmp(k, "devices") || devices) && (strcmp(k, "logout") || logout)) r.push_back(k);
		return r;
	}

	// in a tailnet (approved or not), connected or on its way: a login to log out of
	bool Settings::Kiwami::loggedIn() const
	{
		return on && (ts.state == S::RUNNING || ts.state == S::NEEDS_APPROVAL || (ts.state == S::STARTING && !ts.self.ipv4.empty()));
	}

	std::string Settings::Kiwami::name()
	{
		std::string n = getConfValue(st.data + "/settings.conf", "tailscale_name", "");
		if (!n.empty()) return n;
		if (!modelRead)
		{
			std::ifstream f("/proc/device-tree/model");
			std::getline(f, model, '\0');
			modelRead = true;
		}
		return kiwami::defaultName(model);
	}

	Settings::Result Settings::Kiwami::open(const std::string& key)
	{
		if (key != "kiwami.tailscale") return NONE;
		st.page = KIWAMI;
		view = MENU;
		selKey = "on";
		armed = false;
		nextPoll = st.now;	// asked again at once
		return MOVED;
	}

	void Settings::Kiwami::setOn(bool v)
	{
		on = v;
		armed = false;
		setConfValue(st.data + "/settings.conf", "tailscale", v ? "on" : "off");
		ts = S();
		if (tool.empty()) { show("no helper", "Off"); return; }
		run(v ? "up" : "down");
		show(v ? "switched on" : "switched off", v ? "Starting" : "Off");
		nextPoll = st.now;
	}

	Settings::Result Settings::Kiwami::change(int dir)
	{
		if (selKey == "on") { setOn(!on); return CHANGED; }
		if (selKey == "logout")
		{
			if (dir <= 0) { armed = false; return MOVED; }
			if (!armed) { armed = true; return MOVED; }
			armed = false;
			run("logout");	// (the helper starts a new login at once)
			nextPoll = st.now;
			return CHANGED;
		}
		if (dir <= 0) return NONE;
		if (selKey == "devices") { view = DEVICES; return MOVED; }
		if (selKey == "name") { st.startEntry(ENTRY_KIWAMI, "Tailscale", "Device name", KIWAMI, name()); return MOVED; }
		return NONE;
	}

	Settings::Result Settings::Kiwami::press(Key k)
	{
		return view == DEVICES ? pressDevices(k) : pressMenu(k);
	}

	Settings::Result Settings::Kiwami::pressMenu(Key k)
	{
		std::vector<std::string> rows = menuRows();
		int n = (int)rows.size(), i = n - 1;	// (a row that went away: the last one)
		for (int r = 0; r < n; r++) if (rows[r] == selKey) i = r;
		switch (k)
		{
		case Key::UP: case Key::DOWN:
			selKey = rows[(i + (k == Key::UP ? -1 : 1) + n) % n];
			armed = false;
			return MOVED;
		case Key::LEFT: selKey = rows[i]; return change(-1);
		case Key::RIGHT: case Key::A: selKey = rows[i]; return change(1);
		case Key::B: st.page = LIST; armed = false; dropQr(); return MOVED;
		case Key::START: st.page = LIST; st.mOpen = false; dropQr(); return CLOSED;
		default: return NONE;
		}
	}

	Settings::Result Settings::Kiwami::pressDevices(Key k)
	{
		switch (k)
		{
		case Key::B: case Key::LEFT: view = MENU; return MOVED;
		case Key::START: view = MENU; st.page = LIST; st.mOpen = false; return CLOSED;
		default: return NONE;
		}
	}

	Settings::Result Settings::Kiwami::touch(float, float)
	{
		// where the press went down, as upstream's pages: the chips, B on the left and A on the right, then the rows
		float dx = st.downX, dy = st.downY;
		if (dy >= 418 && dy <= 466) return dx < 320 ? press(Key::B) : view == MENU ? press(Key::A) : NONE;
		if (view != MENU) return NONE;
		std::vector<std::string> rows = menuRows();
		for (int r = 0; r < (int)rows.size(); r++)
		{
			float ry = 82 + r * 64;
			if (dy < ry || dy > ry + 56) continue;
			if (rows[r] != selKey) { selKey = rows[r]; armed = false; return MOVED; }
			return change(rows[r] == "on" && dx < 400 ? -1 : 1);	// (the selected row: its value's left half steps back)
		}
		return NONE;
	}

	std::string Settings::Kiwami::entryHelp() const
	{
		return "A types the key, B deletes, X capitals, L symbols (the hyphen), Y a space, START when it is typed. Or tap "
			"the keys. The handheld's name in your tailnet: letters, digits and hyphens; spaces become hyphens and capitals "
			"lowercase. Kept in banana/settings.conf on the card.";
	}

	Settings::Result Settings::Kiwami::entered(std::string& text)
	{
		std::string n = kiwami::normaliseName(text);
		if (n.empty())
		{
			fprintf(stderr, "banana-ui: tailscale: name \"%s\" refused\n", text.c_str());
			st.message = "Not a name for the tailnet: letters, digits and hyphens only, up to 63, with no hyphen first or last.";
			return MOVED;
		}
		setConfValue(st.data + "/settings.conf", "tailscale_name", n);
		fprintf(stderr, "banana-ui: tailscale: named %s\n", n.c_str());
		if (on) run("name", n.c_str());
		text.clear();
		st.message.clear();
		st.page = KIWAMI;
		return CHANGED;
	}

	// ---- drawing ----------------------------------------------------------------------------------------------------
	void Settings::Kiwami::render()
	{
		if (view == DEVICES) renderDevices(); else renderMenu();
	}

	// a paragraph on the top panel, as upstream's help (20 px, 540 wide at its left); the top of the next one
	float Settings::Kiwami::para(const std::string& t, float top, int maxLines, const Color& c, float x, float width)
	{
		if (t.empty() || maxLines <= 0 || !st.R) return top;
		rnds::TextStyle ts;
		ts.font = st.R; ts.size = 20;
		int n = (int)rnds::layoutWrap(rnds::utf8to32(t), ts, (int)width, maxLines).size();
		st.wrapped(t, 20, x, top, width, maxLines, c);
		return top + n * std::round(20 * 1.4f);
	}

	// the top panel: Tailscale, the title and a line in the accent, then the state (in ink) and the help (muted)
	void Settings::Kiwami::renderTop(const std::string& title, const std::string& accent, const std::string& state, const std::string& help)
	{
		Colours p = colours(st.light);
		st.be->beginScreen(0);
		st.box("screen", false, 0, 0, 640, 480);
		st.box("hero", false, 18, 20, 604, 436);
		st.icon("phone", 50, 56, 3);
		st.text("Tailscale", true, 17, 170, 62, p.muted, true, 1.4f);
		st.text(st.fit(title, true, 30, 420), true, 30, 170, 88, p.ink);
		if (!accent.empty()) st.text(st.fit(accent, true, 22, 420), true, 22, 170, 130, p.accent);
		float y = para(state, 196, 7, p.ink);	// (the help gets what is left: a line at least)
		if (!state.empty()) y += 14;
		para(help, y, (int)((440 - y) / 28), p.muted);
		st.be->endScreen();
	}

	// the login link's QR code, made once a link: black modules on white, one pixel each (drawn at a whole number of
	// device pixels a module, not smoothed)
	bool Settings::Kiwami::qrFor(const std::string& url)
	{
		if (url.empty()) { dropQr(); return false; }
		if (url == qrUrl) return (bool)qrTex;
		dropQr();
		qrUrl = url;
		int side = 0;
		std::vector<bool> m = kiwami::loginQr(url, side);
		if (m.empty()) return false;
		rnds::Bitmap b;
		b.w = b.h = side;
		b.px.resize((size_t)side * side * 4);
		for (size_t i = 0; i < m.size(); i++)
		{
			uint8_t v = m[i] ? 0 : 255;
			b.px[i * 4] = b.px[i * 4 + 1] = b.px[i * 4 + 2] = v;
			b.px[i * 4 + 3] = 255;
		}
		qrTex = st.be->upload(b, false);
		fprintf(stderr, "banana-ui: tailscale: login code %dx%d modules for %s\n", side, side, url.c_str());
		return (bool)qrTex;
	}

	// the top panel while a login waits: the code as large as the panel allows on the left (330 px of a 640x480 panel:
	// 8 device pixels a module at 640x480, 12 at 1024x768, for a link of ~45 characters), the title, the state and the
	// help beside it, the link and a note under it (the auth key, mostly)
	void Settings::Kiwami::renderLogin(const std::string& title, const std::string& accent, const std::string& state, const std::string& help,
		const std::string& link, const std::string& foot)
	{
		Colours p = colours(st.light);
		rnds::Backend* be = st.be;
		be->beginScreen(0);
		st.box("screen", false, 0, 0, 640, 480);
		st.box("hero", false, 18, 20, 604, 436);
		int side = qrTex->w, k = std::max(1, (int)std::floor(st.X(330) / side));
		be->draw(qrTex, std::round(st.X(36)), std::round(st.X(34)), (float)side * k, (float)side * k, Color(1, 1, 1, 1));
		const float x = 384, w = 216;
		st.text("Tailscale", true, 17, x, 46, p.muted, true, 1.4f);
		st.text(st.fit(title, true, 30, w), true, 30, x, 72, p.ink);
		if (!accent.empty()) st.text(st.fit(accent, true, 22, w), true, 22, x, 114, p.accent);
		float y = para(state, 160, 7, p.ink, x, w);
		if (!state.empty()) y += 14;
		para(help, y, (int)((370 - y) / 28), p.muted, x, w);
		st.text(st.fit(link, false, 20, 556), false, 20, 50, 380, p.ink);
		st.text(st.fit(foot, false, 18, 556), false, 18, 50, 410, p.muted);
		be->endScreen();
	}

	void Settings::Kiwami::renderMenu()
	{
		Colours p = colours(st.light);
		std::vector<std::string> rows = menuRows();
		if (std::find(rows.begin(), rows.end(), selKey) == rows.end()) selKey = rows.back();
		const std::string nm = name(), ip = ts.self.ipv4;
		std::string dns = ts.self.dnsName;
		if (!dns.empty() && dns.back() == '.') dns.pop_back();
		std::string url = ts.authUrl;
		if (url.compare(0, 8, "https://") == 0) url.erase(0, 8);
		int online = 0;
		for (auto& d : ts.peers) online += d.online;

		// ---- top: the state ----
		std::string title, accent, state, foot;
		bool qr = false;
		if (!on || tool.empty())
		{
			title = "Off";
			state = on ? "Tailscale isn't in this image." : "Tailscale puts the handheld in your tailnet, your own private network of devices, "
				"wherever each of them is. Once on, it starts again at every boot. SSH and File sharing then answer from the whole tailnet, "
				"not only this Wi-Fi.";
		}
		else if (word == "Waiting for Wi-Fi")
		{
			title = word;
			state = "Tailscale connects by itself once the handheld is on a network: Network, in this group, joins one.";
		}
		else switch (ts.state)
		{
		case S::OFF:
			title = ts.error.empty() ? "Starting" : "Error";
			state = ts.error.empty() ? "Tailscale is starting." : "Tailscale couldn't start: " + ts.error +
				". Switching it off and on again tries again; banana/logs/tailscale.log on the card says more.";
			break;
		case S::STARTING:
			title = "Starting";
			state = "Tailscale is starting and reaching its servers.";
			if (!ts.health.empty()) state += " " + ts.health[0];
			break;
		case S::NEEDS_LOGIN:
			title = ts.self.expired ? "Log in again" : "Log in";
			accent = nm;
			// with a link: its QR code, scanned from a phone (renderLogin, below)
			qr = qrFor(ts.authUrl);
			// (beside the code there is room for a few words: the key's and the expiry's notes go under it, as foot)
			state = std::string(ts.self.expired ? "This device's login expired. " : "") +
				(qr ? "Scan the code with your phone, log in, and approve " + nm + "." :
				 url.empty() ? "Asking Tailscale for a login link... Or put an auth key from the admin console in banana/tailscale/authkey on the card." :
				 "Open " + url + " on your phone or a computer, log in, and approve " + nm + ". Or put an auth key from the admin console in banana/tailscale/authkey on the card.") +
				(ts.self.expired && !qr ? " In the admin console, Disable key expiry keeps it logged in." : "");
			foot = ts.self.expired ? "Disable key expiry in the admin console to stay in." : "Or an auth key in banana/tailscale/authkey on the card.";
			break;
		case S::NEEDS_APPROVAL:
			title = "Waiting for approval";
			accent = nm;
			state = "Logged in. Your tailnet's admin has to approve " + nm + " in the admin console; this page moves on by itself then.";
			break;
		case S::RUNNING:
			title = "Connected";
			accent = ip.empty() ? nm : ip;
			if (!dns.empty()) state = "This is " + dns + ". ";
			if (!ip.empty()) state += "From the tailnet: ssh root@" + ip + ", and File sharing at \\\\" + ip + "\\banana when it is on.";
			break;
		case S::STOPPED:
			title = "Stopped";
			state = "Tailscale was stopped from outside the menu. Switching it off and on again brings it back.";
			break;
		}
		if (on && ts.state != S::OFF && !ts.error.empty())
		{
			const std::string refused = "auth key refused: ";
			if (ts.error.compare(0, refused.size(), refused) == 0)
			{
				std::string why = ts.error.substr(refused.size());
				if (why.compare(0, 15, "backend error: ") == 0) why.erase(0, 15);
				if (qr)
				{
					state = "Auth key refused: " + why + ". " + state;
					foot = "The refused key is now banana/tailscale/authkey.refused.";
				}
				else state = "The auth key on the card was refused (" + why + "); it is now authkey.refused. " + state;
			}
			else state = "Last error: " + ts.error + ". " + state;
		}
		// the certificates of Tailscale's servers don't check out with the clock wrong (an RTC that lost the time)
		bool clock = false;
		for (auto& h : ts.health) clock |= h.find("x509") != std::string::npos || h.find("certificate") != std::string::npos;
		if (on && clock) state += " The handheld's clock may be wrong, and Tailscale can't trust its servers then: connect Wi-Fi so the clock is set.";

		std::string help;
		if (selKey == "on") help = "On: the handheld joins your tailnet now and at every boot. Off: Tailscale stops and nothing of it runs; the login stays for next time.";
		else if (selKey == "devices") help = "The tailnet's devices: their names, their addresses and which are online.";
		else if (selKey == "name") help = "The handheld's name in the tailnet, and in its MagicDNS name. A name changed in the admin console stops following this one.";
		else help = armed ? "Press A again to log out." : "Takes the handheld out of the tailnet and asks for a new login at once. To stop Tailscale and stay logged in, switch it off instead. Press A twice.";
		if (!qr) dropQr();
		if (qr) renderLogin(title, accent, state, help, url, foot);
		else renderTop(title, accent, state, help);

		// ---- bottom: the rows ----
		rnds::Backend* be = st.be;
		be->beginScreen(1);
		st.box("screen", false, 0, 0, 640, 480);
		st.box("bubble", false, 18, 14, 604, 54);
		st.text("Tailscale", true, 22, 38, 28, p.ink);
		int at = 0;
		for (int r = 0; r < (int)rows.size(); r++) if (rows[r] == selKey) at = r;
		char count[32];
		snprintf(count, sizeof count, "%02d / %02d", at + 1, (int)rows.size());
		float cw = st.text(count, true, 17, 0, 0, p.muted, false, 0, true);
		st.text(count, true, 17, 602 - cw, 31, p.muted);
		for (int r = 0; r < (int)rows.size(); r++)
		{
			const std::string& k = rows[r];
			bool sel = r == at;
			float y = 82 + r * 64;
			st.box(sel ? "pick" : "slot", sel, 18, y, 604, 56);
			const char* label = k == "on" ? "Tailscale" : k == "devices" ? "Devices" : k == "name" ? "Device name" : "Log out";
			st.text(label, true, 20, 38, y + 16, sel ? p.onPick : p.muted);
			Color vc = sel ? (armed && k == "logout" ? p.onPickAccent : p.onPick) : p.muted;
			if (k == "on")
			{
				// ◀ On ▶, as a choice in upstream's list
				std::string v = on ? "On" : "Off";
				float vw = st.text(v, true, 19, 0, 0, p.ink, false, 0, true);
				st.arrow(true, 576, y + 21, vc);
				st.text(v, true, 19, 566 - vw, y + 17, vc);
				st.arrow(false, 556 - vw - 2 - std::round(13 / 2.0f + 1), y + 21, vc);
				continue;
			}
			std::string v = k == "devices" ? std::to_string(online) + " online" : k == "name" ? st.fit(nm, true, 19, 300) : armed ? "Press A again" : "";
			if (v.empty()) continue;
			float vw = st.text(v, true, 19, 0, 0, p.ink, false, 0, true);
			st.text(v, true, 19, 586 - vw, y + 17, vc);
		}
		float bw = st.faceWidth("B", "Back");
		st.face("B", "Back", 18, bw);
		std::string act = selKey == "on" || selKey == "name" ? "Change" : selKey == "devices" ? "Open" : armed ? "Confirm" : "Log out";
		float aw = st.faceWidth("A", act);
		st.face("A", act, 622 - aw, aw);
		be->endScreen();
	}

	// the tailnet's devices (their list comes with the next step)
	void Settings::Kiwami::renderDevices()
	{
		Colours p = colours(st.light);
		int online = 0;
		for (auto& d : ts.peers) online += d.online;
		renderTop("Devices", std::to_string(online) + " of " + std::to_string(ts.peers.size()) + " online",
			"The tailnet's devices, with their addresses, come here.", "");
		rnds::Backend* be = st.be;
		be->beginScreen(1);
		st.box("screen", false, 0, 0, 640, 480);
		st.box("bubble", false, 18, 14, 604, 54);
		st.text("Devices", true, 22, 38, 28, p.ink);
		float bw = st.faceWidth("B", "Back");
		st.face("B", "Back", 18, bw);
		be->endScreen();
	}
}
