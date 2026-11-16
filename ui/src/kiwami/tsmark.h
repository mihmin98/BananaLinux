#pragma once
// banana-ui (kiwami): Tailscale's mark, drawn after the Wi-Fi icon by the menu's status bar (statusbar.cpp) and the
// game panel's status strip (panel-status.cpp), at the engine's two sizes of its art: 16 px beside the 18x16 Wi-Fi
// icon (@1x), 26 px beside the 29x26 one (@1.6x).
#include "../../rnds/RndsUI.h"

namespace banana::kiwami
{
	// Tailscale's mark: nine dots in a 3x3 grid, the T (the middle row and the bottom centre) solid, the other five
	// faded. '#' solid, '+' faded (35%), '.' none; drawn in the Wi-Fi icon's tint. At 1x beside the 18x16 Wi-Fi icon:
	// 4x4 dots, 2 px apart
	inline const char* const MARK16[16] = {
		"++++..++++..++++",
		"++++..++++..++++",
		"++++..++++..++++",
		"++++..++++..++++",
		"................",
		"................",
		"####..####..####",
		"####..####..####",
		"####..####..####",
		"####..####..####",
		"................",
		"................",
		"++++..####..++++",
		"++++..####..++++",
		"++++..####..++++",
		"++++..####..++++",
	};

	// at 1.6x beside the 29x26 Wi-Fi icon: 6x6 dots with their corners cut, 4 px apart
	inline const char* const MARK26[26] = {
		".++++......++++......++++.",
		"++++++....++++++....++++++",
		"++++++....++++++....++++++",
		"++++++....++++++....++++++",
		"++++++....++++++....++++++",
		".++++......++++......++++.",
		"..........................",
		"..........................",
		"..........................",
		"..........................",
		".####......####......####.",
		"######....######....######",
		"######....######....######",
		"######....######....######",
		"######....######....######",
		".####......####......####.",
		"..........................",
		"..........................",
		"..........................",
		"..........................",
		".++++......####......++++.",
		"++++++....######....++++++",
		"++++++....######....++++++",
		"++++++....######....++++++",
		"++++++....######....++++++",
		".++++......####......++++.",
	};

	// the mark as an alpha bitmap (big: the 26 px one), to draw in the Wi-Fi icon's tint
	inline rnds::Bitmap tsMark(bool big)
	{
		const char* const* map = big ? MARK26 : MARK16;
		int n = big ? 26 : 16;
		rnds::Bitmap b;
		b.w = b.h = n;
		b.alpha = true;
		b.px.assign(n * n, 0);
		for (int y = 0; y < n; y++)
			for (int x = 0; x < n; x++)
				b.px[y * n + x] = map[y][x] == '#' ? 255 : map[y][x] == '+' ? 89 : 0;	// 89: 35% of 255
		return b;
	}
}
