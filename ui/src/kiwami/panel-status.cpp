// banana-ui (kiwami): Status on the game panel (kiwami.h): the strip drawn where the tiles view's hint line was, its
// state kept by kiwami::PanelStatus (panelstatus.h), its sources given here. Called from settings_gamepanel.cpp's
// kiwami: lines. Off (the setting's default), nothing is read and upstream's hint is drawn.
//
// The strip, in a 640x480 panel's px (X() to the device's), in the panel's palette:
//   left   the Wi-Fi sprite at (24, 440), dimmed to 35% while wlan0 is down, then Tailscale's mark 8 px after it while
//          the tailnet is up (tsmark.h), in its tint; the engine's art, @1x or @1.6x as the engine picks it
//   centre "Played h:mm", centred on 320
//   right  from 616 leftwards: the time, the date 8 px before it, the battery sprite 12 px before that, the percentage
//          6 px before the battery, right-aligned in room for "100%" so nothing moves as it changes. The level as the
//          engine's status bar draws it: in the accent with the bolt on the charger; in red (Settings' notice's) at 10%
//          and under off it, the percentage too; neither drawn with no battery reading
// The environment's stand-ins, for kiwami/tests/panel-status-ui.sh: KIWAMI_PANEL_CLOCK (a fixed clock at the game's
// start, seconds since the epoch), KIWAMI_WIFI_OPERSTATE (the file read for Wi-Fi's state), KIWAMI_TS_IFACE (the
// interface standing for the tailnet, as the status bar's), KIWAMI_PANEL_TRACE=1 (a line saying where the parts went).
#include "kiwami.h"
#include "library.h"
#include "statusbar.h"
#include "tsmark.h"
#include "../library.h"
#include "../settings_draw.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <time.h>

namespace banana
{
	using rnds::Color;

	// ms since boot spent asleep: CLOCK_BOOTTIME counts it, CLOCK_MONOTONIC doesn't
	static double asleepMs()
	{
		timespec b, m;
		clock_gettime(CLOCK_BOOTTIME, &b);
		clock_gettime(CLOCK_MONOTONIC, &m);
		return (b.tv_sec - m.tv_sec) * 1000.0 + (b.tv_nsec - m.tv_nsec) / 1e6;
	}

	static kiwami::PanelReadings readings()
	{
		kiwami::PanelReadings r;
		if (kiwami::library)
		{
			Library::Power p = kiwami::library->power();	// (its own 2 s cache, on CLOCK_BOOTTIME)
			r.percent = p.percent;
			r.charging = p.charging;
		}
		const char* op = getenv("KIWAMI_WIFI_OPERSTATE");
		std::ifstream f(op && *op ? op : "/sys/class/net/wlan0/operstate");
		std::string w;
		std::getline(f, w);
		r.wifi = w == "up";
		const char* ts = getenv("KIWAMI_TS_IFACE");
		r.tailnet = kiwami::hasIPv4(ts ? ts : "tailscale0");
		return r;
	}

	void Settings::Kiwami::panelOpened()
	{
		panelOn = getConfValue(st.data + "/settings.conf", "kiwami_panel_status", "off") == "on";
		if (!panelOn) return;
		if (!panel)
			panel = std::make_unique<kiwami::PanelStatus>(kiwami::PanelStatus::Sources{ readings, [] { return time(nullptr); }, asleepMs });
		const char* fixed = getenv("KIWAMI_PANEL_CLOCK");
		panel->open(kiwami::library && kiwami::library->twelveHour, fixed ? (time_t)atoll(fixed) : 0);
		panelTrace.clear();
	}

	bool Settings::Kiwami::panelUpdate(double t)
	{
		if (!panelOn || !panel) return false;
		bool changed = panel->update(t);
		std::string l = panel->logLine();
		if (!l.empty()) fprintf(stderr, "banana-ui: kiwami: %s\n", l.c_str());
		return changed;
	}

	rnds::TexPtr Settings::Kiwami::panelSprite(const std::string& name, bool big)
	{
		const std::string key = "kiwami:panel:" + name + (big ? "@1.6x" : "@1x");
		auto it = st.cache.find(key);
		if (it == st.cache.end())
		{
			rnds::Bitmap b = name == "mark" ? kiwami::tsMark(big) : rnds::decodeFile(st.assets + "/" + name + (big ? "@1.6x" : "@1x") + ".png");
			Cached c;
			if (!b.empty()) c.tex = st.be->upload(b, false);
			it = st.put(key, c, b);
		}
		return st.keep(it).tex;
	}

	void Settings::Kiwami::panelBattery(float bx, float by, const kiwami::PanelReadings& r, const Color& level, const Color& ink,
		const Color& edge)
	{
		// as RndsUI.cpp's drawStatus: the level 3 px in, 16 px at 100% on the device's pixel grid, 8 tall
		float fw = std::round(16 * std::min(1.0f, std::max(0.0f, r.percent / 100.0f)) * st.s) / st.s;
		if (fw > 0)
			st.be->fill(std::round(st.X(bx + 3)), std::round(st.X(by + 3)), std::round(st.X(fw)), std::round(st.X(8)), level);
		if (!r.charging) return;
		// the bolt, in ink with a dark edge left and right so it reads over the level and over the empty part alike
		static const int BOLT[8][2] = { { 4, 2 }, { 3, 2 }, { 2, 2 }, { 1, 5 }, { 3, 2 }, { 2, 2 }, { 1, 2 }, { 0, 2 } };	// x, length per row
		float x0 = bx + 3 + 5, y0 = by + 3;
		auto run = [&](float x, float y, float w, const Color& c)
		{
			float a = std::round(st.X(x)), b = std::round(st.X(x + w)), t = std::round(st.X(y)), u = std::round(st.X(y + 1));
			st.be->fill(a, t, b - a, u - t, c);
		};
		for (int i = 0; i < 8; i++) run(x0 + BOLT[i][0] - 1, y0 + i, BOLT[i][1] + 2, edge);
		for (int i = 0; i < 8; i++) run(x0 + BOLT[i][0], y0 + i, BOLT[i][1], ink);
	}

	bool Settings::Kiwami::panelStatus()
	{
		if (!panelOn || !panel || !panel->ready()) return false;
		const kiwami::PanelState& ps = panel->state();
		const kiwami::PanelReadings& r = ps.r;
		SPal p = pal(st.light);
		Settings::AsNamed asIs(st);	// (the time, the date, the numbers: drawn as they are)
		const bool big = st.s > 1.3f;	// the engine's pick of its @1x / @1.6x art

		// left: Wi-Fi, then Tailscale's mark, in Wi-Fi's tint
		const Color tint(p.ink.r, p.ink.g, p.ink.b, r.wifi ? 1.0f : 0.35f);
		float lx = std::round(st.X(24)), ly = std::round(st.X(440)), end = lx;
		if (rnds::TexPtr w = panelSprite("wifi", big))
		{
			st.be->draw(w, lx, ly, (float)w->w, (float)w->h, tint);
			end = lx + w->w;
		}
		if (r.tailnet)
			if (rnds::TexPtr m = panelSprite("mark", big))
			{
				float mx = end + std::round(st.X(8));
				st.be->draw(m, mx, ly, (float)m->w, (float)m->h, tint);
				end = mx + m->w;
			}
		const float leftEnd = end / st.s;

		// centre: the time played
		const std::string played = "Played " + ps.played;
		const float pw = st.text(played, false, 15, 0, 0, p.muted, false, 0, true);
		st.text(played, false, 15, 320 - pw / 2, 438, p.muted);

		// right: the time, the date, the battery and its percentage
		const float tw = st.text(ps.clock, true, 17, 0, 0, p.ink, false, 0, true);
		st.text(ps.clock, true, 17, 616 - tw, 437, p.ink);
		const float dw = st.text(ps.date, false, 15, 0, 0, p.muted, false, 0, true);
		const float dx = 616 - tw - 8 - dw;
		st.text(ps.date, false, 15, dx, 438, p.muted);
		float rightStart = dx;
		rnds::TexPtr b = r.percent >= 0 ? panelSprite("battery", big) : nullptr;
		if (b)
		{
			const bool low = !r.charging && r.percent <= 10;
			const Color red = st.light ? Color::hex(0xc8382d) : Color::hex(0xff5a4f);	// (Settings' notice's)
			const float bx = dx - 12 - b->w / st.s, by = 441;
			st.be->draw(b, std::round(st.X(bx)), std::round(st.X(by)), (float)b->w, (float)b->h, p.ink);
			panelBattery(bx, by, r, r.charging ? p.accent : low ? red : p.ink, p.ink, p.blackish);
			const std::string pct = std::to_string(std::min(100, r.percent)) + "%";
			const float slot = st.text("100%", false, 15, 0, 0, p.ink, false, 0, true);
			const float cw = st.text(pct, false, 15, 0, 0, p.ink, false, 0, true);
			st.text(pct, false, 15, bx - 6 - cw, 438, low ? red : p.ink);
			rightStart = bx - 6 - slot;
		}

		static const bool trace = getenv("KIWAMI_PANEL_TRACE") && strcmp(getenv("KIWAMI_PANEL_TRACE"), "1") == 0;
		if (trace)
		{
			char l[160];
			snprintf(l, sizeof l, "panel status: drawn: left ends at %.1f, played %.1f-%.1f, right starts at %.1f", leftEnd,
				320 - pw / 2, 320 + pw / 2, rightStart);
			if (panelTrace != l) fprintf(stderr, "banana-ui: kiwami: %s\n", l);
			panelTrace = l;
		}
		return true;
	}
}
