// banana-ui (kiwami): Tailscale's icon after the Wi-Fi icon in the Dark and Light themes' status bar (statusbar.h).
#include "statusbar.h"
#include "tsmark.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

namespace banana::kiwami
{
	StatusBar::StatusBar(rnds::Backend* real_, float scale_, std::function<bool()> up_, std::function<double()> now_)
		: real(real_), scale(scale_), up(std::move(up_)), now(std::move(now_))
	{
	}

	void StatusBar::retarget(rnds::Backend* real_, float scale_)
	{
		if (real_ != real) tex[0] = tex[1] = nullptr;	// textures belong to the backend that made them
		real = real_;
		scale = scale_;
		phase = PASS;
		screen = -1;
	}

	float StatusBar::px(float css) const { return std::round(css * scale); }

	rnds::TexPtr StatusBar::icon()
	{
		int i = scale > 1.3f ? 1 : 0;	// the engine's pick of its @1x / @1.6x art
		if (!tex[i])
		{
			rnds::Bitmap b = tsMark(i == 1);
			tex[i] = real->upload(b, false);
			texW[i] = b.w;
		}
		return tex[i];
	}

	bool StatusBar::moving(float x, float y, float w)
	{
		if (x + w > px(560) || y >= px(58))
		{
			phase = AFTER;	// the clock (or below the bar): it and everything after it as drawn
			return false;
		}
		return true;
	}

	void StatusBar::note(float x, float y, float /*w*/)
	{
		if (y >= px(58)) return;	// below the bar
		if (!started || x < start) start = x;
		started = true;
	}

	rnds::TexPtr StatusBar::upload(const rnds::Bitmap& b, bool smooth) { return real->upload(b, smooth); }

	void StatusBar::draw(const rnds::TexPtr& t, float x, float y, float w, float h, const rnds::Color& tint)
	{
		if (phase == SEEK && t && std::fabs(x - px(18)) < 0.5f && w <= px(24) && h <= px(20) && y >= px(14) && y <= px(36))
		{
			// the Wi-Fi sprite: it, then the icon one gap after it, at its y and with its tint
			real->draw(t, x, y, w, h, tint);
			rnds::TexPtr i = icon();
			float n = (float)texW[scale > 1.3f ? 1 : 0];
			real->draw(i, x + w + px(8), y, n, n, tint);
			shift = n + px(8);
			alpha = tint.a;
			phase = MOVE;
			return;
		}
		if (phase == MOVE && moving(x, y, w))
		{
			real->draw(t, x + shift, y, w, h, tint);
			end = std::max(end, x + shift + w);
			moved++;
			return;
		}
		if (phase == AFTER) note(x, y, w);
		real->draw(t, x, y, w, h, tint);
	}

	void StatusBar::fill(float x, float y, float w, float h, const rnds::Color& c)
	{
		if (phase == MOVE && moving(x, y, w))
		{
			real->fill(x + shift, y, w, h, c);
			end = std::max(end, x + shift + w);
			moved++;
			return;
		}
		if (phase == AFTER) note(x, y, w);
		real->fill(x, y, w, h, c);
	}

	void StatusBar::pushClip(float x, float y, float w, float h)
	{
		if (phase == MOVE && moving(x, y, w)) x += shift;
		real->pushClip(x, y, w, h);
	}

	void StatusBar::popClip() { real->popClip(); }

	void StatusBar::beginScreen(int s)
	{
		screen = s;
		phase = PASS;
		if (s == 0)
		{
			double t = now();
			if (!asked || t - askedAt >= 2)
			{
				shown = up();
				asked = true;
				askedAt = t;
			}
			if (shown)
			{
				phase = SEEK;
				shift = alpha = end = start = 0;
				moved = 0;
				started = false;
			}
		}
		real->beginScreen(s);
	}

	void StatusBar::endScreen()
	{
		if (screen == 0)
		{
			std::string line;
			if (phase == SEEK)
			{
				// the tailnet is up but the bar isn't drawn the way this knows: nothing was changed
				if (!noWifiSaid && log) log("tailscale icon: no Wi-Fi icon in the bar");
				noWifiSaid = true;
				line = "hidden";
			}
			else if (phase == MOVE || phase == AFTER)
			{
				char b[200];
				char s[32] = "none";
				if (started) snprintf(s, sizeof s, "%g", start);
				snprintf(b, sizeof b, "tailscale icon: shown, alpha %.2f, %d draws moved %g px, the left part ends at %g, "
					"the clock's part starts at %s", alpha, moved, shift, end, s);
				line = b;
			}
			else
				line = "hidden";
			// a line when it changes: the icon showing or going, or the bar's layout; nothing before it's first shown
			if (line != last && !(line == "hidden" && last.empty()))
			{
				if (log) log(line == "hidden" ? "tailscale icon: hidden" : line);
				last = line;
			}
		}
		phase = PASS;
		real->endScreen();
	}

	bool hasIPv4(const char* iface)
	{
		static int fd = -1;
		if (fd < 0) fd = socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0);
		if (fd < 0 || !iface || !*iface || strlen(iface) >= IFNAMSIZ) return false;
		struct ifreq r;
		memset(&r, 0, sizeof r);
		strncpy(r.ifr_name, iface, IFNAMSIZ - 1);
		r.ifr_addr.sa_family = AF_INET;
		return ioctl(fd, SIOCGIFADDR, &r) == 0 && r.ifr_addr.sa_family == AF_INET;
	}

	rnds::Backend* statusBar(rnds::Backend* real, float scale, bool headless)
	{
		static std::unique_ptr<StatusBar> one;
		if (one)
		{
			one->retarget(real, scale);
			return one.get();
		}
		// the interface: KIWAMI_TS_IFACE's for the tests, else tailscale0 on the device; headless runs never show the icon
		// without it, so they don't depend on the host's own Tailscale
		const char* e = getenv("KIWAMI_TS_IFACE");
		std::string iface = e && *e ? e : headless ? "" : "tailscale0";
		if (headless)
			if (const char* n = getenv("KIWAMI_BRAND_NOTE"); n && *n) rnds::brandNote = n;
		one = std::make_unique<StatusBar>(real, scale,
			[iface] { return !iface.empty() && hasIPv4(iface.c_str()); },
			[] { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec / 1e9; });
		one->log = [](const std::string& l) { fprintf(stderr, "banana-ui: kiwami: %s\n", l.c_str()); };
		return one.get();
	}
}
