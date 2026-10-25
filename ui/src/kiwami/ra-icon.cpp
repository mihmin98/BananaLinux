// banana-ui (kiwami): the fork's own icons, drawn from pixel maps in the theme's icons' look (32x32, a black outline one
// pixel wide, the colours of ui/assets/rnds/icons/: the gold of bank.png and sword.png, the greys of gear.png, the blues
// of cart.png and gamepad.png). Settings asks for one when the theme has no PNG of that name (kiwami.h, icon()).
#include "kiwami.h"

namespace banana
{
	// RetroAchievements' logo, pixelated: a blue R and a gold A, each stroke with the logo's dark line along its middle,
	// lit from the top. The group's tile, its Account and Stats rows, and an account with no avatar
	static const char* LOGO[32] = {
		"................................",
		"................................",
		"................................",
		"................................",
		"................................",
		"................................",
		"aaaaaaaaaaa..........aaaaa......",
		"aLLLLLLLLLaa........allllla.....",
		"aLDDDDDDDDLLa.......aldddla.....",
		"aLDLLLLLLLDLLa......aldldla.....",
		"aLDLaaaaaaLDLa.....aldlaldla....",
		"aLDLa....aLDLa.....aldlaldla....",
		"aBDBa....aBDBa....agdga.agdga...",
		"aBDBaaaaaaBDBa....agdga.agdga...",
		"aBDBBBBBBBDBBa....agdga.agdga...",
		"aBDBDDDDDDDBa....agdga...agdga..",
		"aBDBBBBDBBBa.....agdga...agdga..",
		"aBDBaaaBDBa......agdgaaaaagdga..",
		"aBDBa.aBDBa.....agdgggggggggdga.",
		"aBDBa.aBDBBa....agdddddddddddga.",
		"aMDMa..aMDMa....agdgggggggggdga.",
		"aMDMa..aMMDMa...agdgaaaaaaagdga.",
		"aMDMa...aMDMa..agdga.......agdga",
		"aMDMa...aMMDMa.agdga.......agdga",
		"aMDMa....aMDMa.agdga.......agdga",
		"aaaaa....aaaaa.aaaaa.......aaaaa",
		"................................",
		"................................",
		"................................",
		"................................",
		"................................",
		"................................",
	};

	// a trophy: a gold cup with two handles on a grey plinth, lit from the left. An achievement whose badge isn't on the
	// card
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

	// RomM's logo, pixelated: its isotipo's purple disc, a controller's corner (its lit half and its shaded one, split on
	// the diagonal) with four buttons, two cream and two peach, on the lilac of the disc's rim; the colours are the
	// isotipo's own. A new drawing after it, not RomM's file: RomM's repository (AGPL-3.0, tag 9c74d01) has no trademark
	// or logo policy in its README, its contributing guide or elsewhere, and the AGPL grants no trademark rights, so the
	// logo is used only to name RomM. The group's tile, its RomM and Account rows, the Account page
	static const char* ROMM[32] = {
		"................................",
		"...........aaaaaaaaaa...........",
		".........aaaWWUUUUUUaaa.........",
		".......aaaWWUUUUUUUUUUaaa.......",
		"......aaWWWUUUUUUUUUUUUUaa......",
		".....aaWWWUUUUUUUUUUUUUUUaa.....",
		"....aaWWWUUUUUUUYYUUUUUUUVaa....",
		"...aaWWUUUUUUUUYYYYUUUUUUUVaa...",
		"...aWWUUUUUUUUUYYYYUUUUUUKVVa...",
		"..aaWUUUUUUUUUUUYYUUUUUUKKVVaa..",
		"..aWUUUUUUUUYYUUUUUUEEUKKKKVVa..",
		".aaUUUUUUUUYYYYUUUUEEEEKKKKVVaa.",
		".aUUUUUUUUUYYYYUUUUEEEEKKKKVVVa.",
		".aUUUUUUUUUUYYUUUUUUEEKKKKKKVVa.",
		".aUUUUUUUUUUUUUUEEUKKKKKKKKKVVa.",
		".aUUUUUUUUUUUUUEEEEKKKKKKKKKVVa.",
		".aUUUUUUUUUUUUUEEEEKKKKKKKKKKVa.",
		".aUUUUUUUUUUUUUUEEKKKKKKKKKKKVa.",
		".aUUUUUUUUUUUUUKKKKKKKKKKKKKKVa.",
		".aUUUUUUUUUUUUKKKKKKKKKKKKKKKVa.",
		".aaUUUUUUUUUUKKKKKKKKKKKKKKKKaa.",
		"..aUUUUUUUUUKKKKKKKKKKKKKKKKKa..",
		"..aaUUUUUUUKKKKKKKKKKKKKKKKKaa..",
		"...aUUUUUUKKKKKKKKKKKKKKKKKKa...",
		"...aaVVVVVVVVVVKKKKKKKKKKKKaa...",
		"....aaVVVVVVVVVVKKKKKKKKKKaa....",
		".....aaVVVVVVVVVVKKKKKKKVaa.....",
		"......aaVVVVVVVVVVKKKKKVaa......",
		".......aaaVVVVVVVVVKKKaaa.......",
		".........aaaVVVVVVVVaaa.........",
		"...........aaaaaaaaaa...........",
		"................................",
	};

	rnds::Bitmap Settings::Kiwami::icon(const std::string& name) const
	{
		rnds::Bitmap b;
		const char* const* map = name == "kiwami.trophy" ? TROPHY : name == "kiwami.ra" ? LOGO : name == "kiwami.romm" ? ROMM : nullptr;
		if (!map) return b;
		b.w = b.h = 32;
		b.px.assign(32 * 32 * 4, 0);
		for (int y = 0; y < 32; y++)
			for (int x = 0; x < 32; x++)
			{
				uint32_t c;
				switch (map[y][x])
				{
				case 'a': c = 0x000000; break;	// the outline
				case 'l': c = 0xfff0a8; break;	// gold, lit
				case 'g': c = 0xf2c94c; break;	// gold
				case 'd': c = 0x8a6a1e; break;	// gold, in shade
				case 'b': c = 0xc3c3c3; break;	// the plinth
				case 'h': c = 0x959293; break;	// the plinth, in shade
				case 'L': c = 0x7ea8e6; break;	// blue, lit
				case 'B': c = 0x4c8ee0; break;	// blue
				case 'M': c = 0x3d6fb8; break;	// blue, lower down
				case 'D': c = 0x1f3d6e; break;	// blue, in shade
				case 'W': c = 0xede4f1; break;	// RomM's lilac, lit
				case 'V': c = 0xbda5cf; break;	// RomM's lilac
				case 'U': c = 0x553f99; break;	// RomM's purple
				case 'K': c = 0x38286a; break;	// RomM's purple, in shade
				case 'Y': c = 0xe5c7a7; break;	// RomM's cream button
				case 'E': c = 0xe1a38e; break;	// RomM's peach button
				default: continue;				// clear
				}
				uint8_t* p = &b.px[(y * 32 + x) * 4];
				p[0] = c >> 16; p[1] = (c >> 8) & 0xff; p[2] = c & 0xff; p[3] = 255;
			}
		return b;
	}
}
