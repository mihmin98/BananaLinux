#pragma once
// banana-ui (kiwami): the Game Boy Pokémon companion's pictures, decoded from the player's ROM: Gen II's LZ, the
// Game Boy's 2-bit tiles, and a species' front picture in its Game Boy Color palettes (normal and shiny) as upstream's
// nds::Sprite. The formats are pret's (pokecrystal; pokegold the same), named where each is read (pics.cpp).
// Copyright (C) 2026 Lidor Itzhari (PokeDaisy) and the kiwami fork's contributors. GNU GPL version 3 with PokeDaisy's
// additional term under section 7(b): LICENSE and NOTICE in this folder. SPDX-License-Identifier: GPL-3.0-only
// Based on PokeDaisy by Lidor Itzhari - https://github.com/lidor30/pokedaisy
#include "rom.h"
#include <cstdint>
#include <memory>
#include <vector>

namespace banana::kiwami::gbpk
{
	// Gen II's LZ (pret's home/decompress.asm) from p's first n bytes into out, up to its end mark (0xFF): literal,
	// iterate, alternate, zero fill, and the three copies from what is written (repeat, bit-flipped, reversed), each
	// in the short form or the long one. false, out unfinished, when the stream runs past n, a copy reaches outside
	// what is written, or out would grow past maxOut (the game's own buffer)
	bool lz2(const uint8_t* p, size_t n, std::vector<uint8_t>& out, size_t maxOut);

	// a Game Boy Color colour (RGB555, the game's dw) as 0xRRGGBB, each 5 bits widened by its top 3 as upstream's
	// nds::townmap does
	uint32_t rgb555(uint16_t c);

	// a 2-bit tile's pixel (0-3) at x, y: 16 bytes a tile, each row its low bit plane's byte then its high one's
	inline int tilePixel(const uint8_t* tile, int x, int y)
	{
		return (tile[2 * y] >> (7 - x) & 1) | (tile[2 * y + 1] >> (7 - x) & 1) << 1;
	}

	// a size x size-tile picture, its tiles in columns as the games store a front picture, into out's first frame
	// (8-bit indices, the pixel's value: 0 is clear); false when tiles is short
	bool pictureFrame(const std::vector<uint8_t>& tiles, int size, nds::Sprite& out);

	// a species' front picture (1 .. t.count; Unown's is letter A's), one frame, its two palettes from the ROM; null
	// when it doesn't decode. Safe from any thread: it only reads the ROM
	std::shared_ptr<const nds::Sprite> gen2FrontPic(const RomView& r, const Gen2Pics& t, uint16_t species);
}
