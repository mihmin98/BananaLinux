// The fork's status bar backend (ui/src/kiwami/statusbar.h), Tailscale's icon after the Wi-Fi icon, checked on its own
// against a backend that records every call, with frames drawn the way RndsUI.cpp's drawStatus draws the bar. Built
// and run by kiwami/tests/ci.sh with the host's g++:
//   g++ -std=c++17 -Wall -I ui/src kiwami/tests/statusbar-units.cpp ui/src/kiwami/statusbar.cpp -o statusbar-units
//   ./statusbar-units
#include "kiwami/statusbar.h"
#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

using namespace banana::kiwami;

namespace rnds { std::string brandNote; }	// the engine's (RndsUI.cpp), which statusBar() sets headless

static int checks = 0, failed = 0;
#define CHECK(c) do { checks++; if (!(c)) { failed++; fprintf(stderr, "statusbar-units: FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)
#define EQ(a, b) do { checks++; std::string a_ = (a), b_ = (b); if (a_ != b_) { failed++; fprintf(stderr, "statusbar-units: FAIL %s:%d: %s is \"%s\", not \"%s\"\n", __FILE__, __LINE__, #a, a_.c_str(), b_.c_str()); } } while (0)

// a texture with a name, so the record says which one was drawn
struct FakeTex : rnds::Tex { std::string name; rnds::Bitmap bitmap; };

static std::string fmt(const char* f, ...) __attribute__((format(printf, 1, 2)));
static std::string fmt(const char* f, ...)
{
	char b[256];
	va_list a;
	va_start(a, f);
	vsnprintf(b, sizeof b, f, a);
	va_end(a);
	return b;
}

// the host's backend, recording
struct Recorder : rnds::Backend
{
	std::vector<std::string> calls;
	std::vector<std::shared_ptr<FakeTex>> uploads;
	rnds::TexPtr upload(const rnds::Bitmap& b, bool smooth) override
	{
		auto t = std::make_shared<FakeTex>();
		t->w = b.w; t->h = b.h; t->alpha = b.alpha; t->bitmap = b;
		t->name = fmt("upload%zu", uploads.size());
		uploads.push_back(t);
		calls.push_back(fmt("upload %dx%d", b.w, b.h));
		return t;
	}
	void draw(const rnds::TexPtr& t, float x, float y, float w, float h, const rnds::Color& tint) override
	{
		calls.push_back(fmt("draw %s %g,%g %gx%g a%.2f", t ? static_cast<FakeTex*>(t.get())->name.c_str() : "-", x, y, w, h, tint.a));
	}
	void fill(float x, float y, float w, float h, const rnds::Color& c) override { calls.push_back(fmt("fill %g,%g %gx%g", x, y, w, h)); }
	void pushClip(float x, float y, float w, float h) override { calls.push_back(fmt("clip %g,%g %gx%g", x, y, w, h)); }
	void popClip() override { calls.push_back("unclip"); }
	void beginScreen(int s) override { calls.push_back(fmt("begin %d", s)); }
	void endScreen() override { calls.push_back("end"); }
};

static rnds::TexPtr tex(const char* name, int w, int h)
{
	auto t = std::make_shared<FakeTex>();
	t->w = w; t->h = h; t->name = name;
	return t;
}

// the engine's textures at 640x480 and at 1024x768
struct Art
{
	rnds::TexPtr wall, wifi, batt, plate, ver, logo, clock, date, sheet, cart;
	float s;
	explicit Art(float s_) : s(s_)
	{
		auto S = [&](float v) { return (int)std::round(v * s); };
		wall = tex("wall", S(640), S(480));
		wifi = s > 1.3f ? tex("wifi", 29, 26) : tex("wifi", 18, 16);
		batt = tex("batt", S(26), S(14));
		plate = tex("plate", S(148), S(26));
		ver = tex("ver", S(60), S(20));
		logo = tex("logo", S(100), S(20));
		clock = tex("clock", S(70), S(20));
		date = tex("date", S(55), S(20));
		sheet = tex("sheet", S(604), S(400));
		cart = tex("cart", S(40), S(20));
	}
};

// one frame of both screens as the engine draws it: the top screen's wallpaper, the bar (Wi-Fi at (18, 24 + rise),
// the battery and its level, the plate, the version, the logo, the clock and the date), the sheet; then the bottom
// screen, with a draw at Wi-Fi's place
static void frame(rnds::Backend& be, const Art& a, float rise = 0, float wifiAlpha = 1, bool wifi = true)
{
	auto X = [&](float v) { return std::round(v * a.s); };
	rnds::Color ink(1, 1, 1, 1), dim(1, 1, 1, wifiAlpha);
	be.beginScreen(0);
	be.draw(a.wall, 0, 0, a.wall->w, a.wall->h, ink);
	if (wifi) be.draw(a.wifi, X(18), X(24 + rise), a.wifi->w, a.wifi->h, dim);
	be.draw(a.batt, X(44), X(25 + rise), a.batt->w, a.batt->h, ink);
	be.fill(X(47), X(28 + rise), X(16), X(8), ink);
	be.draw(a.plate, X(78), X(19 + rise), a.plate->w, a.plate->h, ink);
	be.draw(a.ver, X(236), X(22 + rise), a.ver->w, a.ver->h, ink);
	be.draw(a.logo, X(85), X(22 + rise), a.logo->w, a.logo->h, ink);
	be.draw(a.clock, X(552), X(20 + rise), a.clock->w, a.clock->h, ink);
	be.draw(a.date, X(489), X(20 + rise), a.date->w, a.date->h, ink);
	be.draw(a.sheet, X(18), X(60), a.sheet->w, a.sheet->h, ink);
	be.endScreen();
	be.beginScreen(1);
	be.draw(a.cart, X(18), X(24), a.cart->w, a.cart->h, ink);
	be.pushClip(X(18), X(24), X(100), X(20));
	be.fill(X(20), X(26), X(10), X(10), ink);
	be.popClip();
	be.endScreen();
}

struct Rig
{
	Recorder rec, plain;
	bool on = true;
	double t = 0;
	int asked = 0;
	std::vector<std::string> lines;
	StatusBar bar;
	explicit Rig(float s) : bar(&rec, s, [this] { asked++; return on; }, [this] { return t; })
	{
		bar.log = [this](const std::string& l) { lines.push_back(l); };
	}
};

static int count(const std::vector<std::string>& v, const std::string& prefix)
{
	int n = 0;
	for (auto& l : v) n += l.compare(0, prefix.size(), prefix) == 0;
	return n;
}

int main()
{
	// off: a whole frame forwarded identically on both screens, nothing uploaded, nothing logged (A1)
	{
		Art a(1);
		Rig r(1);
		r.on = false;
		frame(r.bar, a);
		frame(r.plain, a);
		CHECK(r.rec.calls == r.plain.calls);
		CHECK(r.rec.uploads.empty());
		CHECK(r.lines.empty());
	}

	// on, 640x480: the icon right after Wi-Fi, the left part moved 24 px, the clock, the date and the sheet unmoved
	// (A2)
	{
		Art a(1);
		Rig r(1);
		frame(r.bar, a);
		const std::vector<std::string> want = {
			"begin 0",
			"draw wall 0,0 640x480 a1.00",
			"draw wifi 18,24 18x16 a1.00",
			"upload 16x16",
			"draw upload0 44,24 16x16 a1.00",
			"draw batt 68,25 26x14 a1.00",
			"fill 71,28 16x8",
			"draw plate 102,19 148x26 a1.00",
			"draw ver 260,22 60x20 a1.00",
			"draw logo 109,22 100x20 a1.00",
			"draw clock 552,20 70x20 a1.00",
			"draw date 489,20 55x20 a1.00",
			"draw sheet 18,60 604x400 a1.00",
			"end",
			"begin 1",
			"draw cart 18,24 40x20 a1.00",
			"clip 18,24 100x20",
			"fill 20,26 10x10",
			"unclip",
			"end",
		};
		CHECK(r.rec.calls == want);
		if (r.rec.calls != want)
			for (auto& c : r.rec.calls) fprintf(stderr, "  %s\n", c.c_str());
		CHECK(r.lines.size() == 1);
		if (!r.lines.empty())
			EQ(r.lines[0], "tailscale icon: shown, alpha 1.00, 5 draws moved 24 px, the left part ends at 320, the clock's "
				"part starts at 489");
	}

	// on, 1024x768: the 26x26 icon at (71, 38) after the 29x26 Wi-Fi at (29, 38), the shift 39 (A2)
	{
		Art a(1.6f);
		Rig r(1.6f);
		frame(r.bar, a);
		CHECK(count(r.rec.calls, "upload 26x26") == 1);
		CHECK(count(r.rec.calls, "draw upload0 71,38 26x26 a1.00") == 1);
		CHECK(count(r.rec.calls, "draw batt 109,40 ") == 1);	// round(44 * 1.6) + 39
		CHECK(count(r.rec.calls, "draw clock 883,32 ") == 1);	// round(552 * 1.6), unmoved
		CHECK(r.lines.size() == 1 && r.lines[0].find("moved 39 px") != std::string::npos);
	}

	// the rise: the icon at the Wi-Fi's y (A4); one shown line over ten frames of it (the layout unchanged)
	{
		Art a(1);
		Rig r(1);
		for (int i = 10; i >= 0; i--)
		{
			r.rec.calls.clear();
			frame(r.bar, a, (float)i);
			CHECK(count(r.rec.calls, fmt("draw upload0 44,%d 16x16 ", 24 + i)) == 1);
		}
		CHECK(r.lines.size() == 1);
	}

	// dim: Wi-Fi at 35%, the icon too (A3); the line says so, and changes when Wi-Fi is back
	{
		Art a(1);
		Rig r(1);
		frame(r.bar, a, 0, 0.35f);
		CHECK(count(r.rec.calls, "draw upload0 44,24 16x16 a0.35") == 1);
		frame(r.bar, a, 0, 1);
		CHECK(r.lines.size() == 2);
		CHECK(r.lines.size() == 2 && r.lines[0].find("alpha 0.35") != std::string::npos && r.lines[1].find("alpha 1.00") != std::string::npos);
	}

	// no Wi-Fi draw: the frame as it was drawn, one line over two frames (A6)
	{
		Art a(1);
		Rig r(1);
		frame(r.bar, a, 0, 1, false);
		frame(r.bar, a, 0, 1, false);
		Recorder plain;
		frame(plain, a, 0, 1, false);
		frame(plain, a, 0, 1, false);
		CHECK(r.rec.calls == plain.calls);
		CHECK(r.lines.size() == 1);
		if (!r.lines.empty()) EQ(r.lines[0], "tailscale icon: no Wi-Fi icon in the bar");
	}

	// the bottom screen is never touched, even with a Wi-Fi-sized draw at Wi-Fi's place
	{
		Art a(1);
		Rig r(1);
		r.bar.beginScreen(1);
		r.bar.draw(a.wifi, 18, 24, 18, 16, rnds::Color(1, 1, 1, 1));
		r.bar.fill(44, 25, 10, 10, rnds::Color(1, 1, 1, 1));
		r.bar.endScreen();
		const std::vector<std::string> want = { "begin 1", "draw wifi 18,24 18x16 a1.00", "fill 44,25 10x10", "end" };
		CHECK(r.rec.calls == want);
		CHECK(r.asked == 0);	// asked only when the top screen is drawn
	}

	// the check's pace: once over frames 1.9 s apart, again at 2 s (A7); hidden once when the tailnet goes
	{
		Art a(1);
		Rig r(1);
		frame(r.bar, a);
		r.t = 1.9;
		frame(r.bar, a);
		CHECK(r.asked == 1);
		r.on = false;
		r.t = 2.0;
		frame(r.bar, a);
		CHECK(r.asked == 2);
		r.t = 3.0;
		frame(r.bar, a);
		r.t = 4.5;
		frame(r.bar, a);
		CHECK(r.asked == 3);
		CHECK(r.lines.size() == 2);
		if (r.lines.size() == 2) EQ(r.lines[1], "tailscale icon: hidden");
		// and its frames are forwarded as they are
		Recorder plain;
		frame(plain, a);
		r.rec.calls.clear();
		frame(r.bar, a);
		CHECK(r.rec.calls == plain.calls);
	}

	// the icon: uploaded once per scale, its A8 pixels the map's solid and faded dots
	{
		Art a(1), b(1.6f);
		Rig r(1);
		frame(r.bar, a);
		frame(r.bar, a);
		r.bar.retarget(&r.rec, 1.6f);
		frame(r.bar, b);
		frame(r.bar, b);
		r.bar.retarget(&r.rec, 1);
		frame(r.bar, a);
		CHECK(count(r.rec.calls, "upload ") == 2);
		CHECK(r.rec.uploads.size() == 2);
		if (r.rec.uploads.size() == 2)
		{
			const rnds::Bitmap& i = r.rec.uploads[0]->bitmap;
			CHECK(i.alpha && i.w == 16 && i.h == 16 && i.px.size() == 256);
			auto at = [&](const rnds::Bitmap& m, int x, int y) { return (int)m.px[y * m.w + x]; };
			CHECK(at(i, 0, 0) == 89 && at(i, 15, 3) == 89);		// top row: faded
			CHECK(at(i, 4, 0) == 0 && at(i, 0, 4) == 0);		// the gaps
			CHECK(at(i, 0, 6) == 255 && at(i, 15, 9) == 255);	// the middle row: solid
			CHECK(at(i, 0, 12) == 89 && at(i, 6, 12) == 255 && at(i, 12, 15) == 89);	// the bottom: the T's foot solid
			int solid = 0, faded = 0;
			for (uint8_t p : i.px) { solid += p == 255; faded += p == 89; }
			CHECK(solid == 4 * 16 && faded == 5 * 16);
			const rnds::Bitmap& j = r.rec.uploads[1]->bitmap;
			CHECK(j.alpha && j.w == 26 && j.h == 26 && j.px.size() == 676);
			CHECK(at(j, 0, 0) == 0 && at(j, 1, 0) == 89 && at(j, 0, 1) == 89);	// a dot's corner cut
			CHECK(at(j, 10, 10) == 0 && at(j, 11, 10) == 255 && at(j, 10, 20) == 0 && at(j, 11, 21) == 255);
			solid = faded = 0;
			for (uint8_t p : j.px) { solid += p == 255; faded += p == 89; }
			CHECK(solid == 4 * 32 && faded == 5 * 32);
		}
		// a new host backend (the theme changed): uploaded again on it
		Recorder other;
		r.bar.retarget(&other, 1);
		frame(r.bar, a);
		CHECK(count(other.calls, "upload 16x16") == 1);
	}

	// the interface's address
	CHECK(hasIPv4("lo"));
	CHECK(!hasIPv4("kiwami-none0"));
	CHECK(!hasIPv4(""));

	if (failed) { fprintf(stderr, "statusbar-units: %d of %d checks failed\n", failed, checks); return 1; }
	printf("statusbar-units: %d checks passed\n", checks);
	return 0;
}
