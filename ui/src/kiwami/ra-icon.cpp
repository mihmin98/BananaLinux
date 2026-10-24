// banana-ui (kiwami): the fork's own icons, drawn from pixel maps in the theme's icons' look (32x32, a black outline one
// pixel wide, the colours of ui/assets/rnds/icons/: the gold of bank.png and sword.png, the greys of gear.png). Settings
// asks for one when the theme has no PNG of that name (kiwami.h, icon()).
#include "kiwami.h"

namespace banana
{
	// RetroAchievements' trophy: a gold cup with two handles on a grey plinth, lit from the left
	static const char* TROPHY[32] = {
		"................................",
		"................................",
		"................................",
		"........aaaaaaaaaaaaaaaa........",
		".......alllllllllllllggga.......",
		"...aaaaalllggggggggggdddaaaaa...",
		"..aggggglllggggggggggdddddddda..",
		".aggaaaglllggggggggggddddaaadda.",
		".agga.aglllggggggggggdddda.adda.",
		".agga.aglllggggggggggdddda.adda.",
		".agga.aglllggggggggggdddda.adda.",
		".aggaaagalllggggggggdddadaaadda.",
		".aggggggaalllggggggdddaadddddda.",
		"..aggggga.alllggggddda.addddda..",
		"...aaaaa...alllggddda...aaaaa...",
		"............alllddda............",
		".............alllda.............",
		".............aggdda.............",
		".............aggdda.............",
		".............aggdda.............",
		"............aaggddaa............",
		"...........agggggddda...........",
		"..........aaaaaaaaaaaa..........",
		"........aabbbbbbbbbhhhaa........",
		".......abbbbbbbbbbbbbhhha.......",
		".......abbbbblllgggbbhhha.......",
		".......abbbbbbbbbbbbbhhha.......",
		".......abbbbbbbbbbbbbhhha.......",
		"........aaaaaaaaaaaaaaaa........",
		"................................",
		"................................",
		"................................",
	};

	rnds::Bitmap Settings::Kiwami::icon(const std::string& name) const
	{
		rnds::Bitmap b;
		if (name != "kiwami.trophy") return b;
		b.w = b.h = 32;
		b.px.assign(32 * 32 * 4, 0);
		for (int y = 0; y < 32; y++)
			for (int x = 0; x < 32; x++)
			{
				uint32_t c;
				switch (TROPHY[y][x])
				{
				case 'a': c = 0x000000; break;	// the outline
				case 'l': c = 0xfff0a8; break;	// gold, lit
				case 'g': c = 0xf2c94c; break;	// gold
				case 'd': c = 0x8a6a1e; break;	// gold, in shade
				case 'b': c = 0xc3c3c3; break;	// the plinth
				case 'h': c = 0x959293; break;	// the plinth, in shade
				default: continue;				// clear
				}
				uint8_t* p = &b.px[(y * 32 + x) * 4];
				p[0] = c >> 16; p[1] = (c >> 8) & 0xff; p[2] = c & 0xff; p[3] = 255;
			}
		return b;
	}
}
