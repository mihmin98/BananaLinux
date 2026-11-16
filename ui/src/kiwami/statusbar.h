#pragma once
// banana-ui (kiwami): Tailscale's icon after the Wi-Fi icon in ROCKNIXDS Pixel's status bar (the Dark and Light
// themes), while the handheld is in its tailnet (tailscale0 has an IPv4 address). The bar is drawn by the vendored
// engine (ui/rnds/), which the fork doesn't edit, so this is a rnds::Backend of the fork's own between the engine and
// the host's: main.cpp's two kiwami: hooks hand RndsMenu statusBar() instead of the host's backend (the headless menu
// and the device's); Carts, the layout themes, Settings and the host's overlays keep drawing on the host's.
//
// It relies on the order RndsUI.cpp's drawStatus draws the bar in (docs/FORK.md, Taking in upstream): first the Wi-Fi
// sprite at (18, 24 + the rise), the first small texture drawn at that x on the top screen; then the left part (the
// battery, its level and bolt, the percentage, the brand plate, the stage badge and version, the logo); then the clock,
// right-aligned at 622, the first draw reaching past x 560; then the date. The icon is drawn right after the Wi-Fi
// sprite, with its tint (dimmed with it while Wi-Fi is down) and at its y (rising with it), and the left part after it
// is moved right by the icon's width and a gap. A bar drawn otherwise (no Wi-Fi draw found) is left as it is, and said
// once in the log. With the tailnet down every call is forwarded unchanged: the bar is upstream's, pixel for pixel.
#include "../../rnds/RndsUI.h"
#include <functional>
#include <string>

namespace banana::kiwami
{
	// Tailscale's icon after the Wi-Fi icon in ROCKNIXDS Pixel's status bar (Dark and Light): every call goes to the
	// host's backend; on the top screen, while the tailnet is up, the Wi-Fi sprite's draw is followed by the icon, and
	// the bar's left part after it is moved right, up to the clock
	class StatusBar : public rnds::Backend
	{
	public:
		// up: the tailnet is up (asked at most every 2 s, when the top screen is drawn); now: seconds, monotonic
		StatusBar(rnds::Backend* real, float scale, std::function<bool()> up, std::function<double()> now);
		void retarget(rnds::Backend* real, float scale);	// a menu made again (a theme change)
		std::function<void(const std::string&)> log;		// a line when what is drawn changes
		rnds::TexPtr upload(const rnds::Bitmap& b, bool smooth) override;
		void draw(const rnds::TexPtr& t, float x, float y, float w, float h, const rnds::Color& tint) override;
		void fill(float x, float y, float w, float h, const rnds::Color& c) override;
		void pushClip(float x, float y, float w, float h) override;
		void popClip() override;
		void beginScreen(int screen) override;
		void endScreen() override;

	private:
		float px(float css) const;		// CSS px of the 640x480 bar to device px, rounded as the engine does
		rnds::TexPtr icon();			// the icon at this scale, uploaded on its first use
		bool moving(float x, float y, float w);	// in the left part: x is to be moved (false: the clock reached)
		void note(float x, float y, float w);	// a draw after the left part: the clock's part's start

		rnds::Backend* real;
		float scale;
		std::function<bool()> up;
		std::function<double()> now;
		bool shown = false;			// the tailnet was up when last asked
		bool asked = false;
		double askedAt = 0;
		rnds::TexPtr tex[2];		// the icon @1x and @1.6x (the engine's pick: scale > 1.3)
		int texW[2] = { 0, 0 };
		// this top screen's frame
		enum Phase { PASS, SEEK, MOVE, AFTER } phase = PASS;
		int screen = -1;
		float shift = 0, alpha = 0, end = 0, start = 0;
		int moved = 0;
		bool started = false;		// start holds a draw's x
		std::string last;			// the last line logged ("" none)
		bool noWifiSaid = false;
	};

	bool hasIPv4(const char* iface);	// SIOCGIFADDR on a datagram socket: false with no interface or no address
	// the process's one (made on the first call, retargeted on the next), for main.cpp's hooks: up is
	// hasIPv4("tailscale0"), or KIWAMI_TS_IFACE's interface. Headless: never up without KIWAMI_TS_IFACE, and
	// KIWAMI_BRAND_NOTE sets rnds::brandNote (the version note only the device has)
	rnds::Backend* statusBar(rnds::Backend* real, float scale, bool headless);
}
