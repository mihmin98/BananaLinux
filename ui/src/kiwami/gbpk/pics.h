#pragma once
// banana-ui (kiwami): the Game Boy Pokémon companion's pictures, decoded from the player's ROM: Gen II's LZ, Gen I's
// picture compression, the Game Boy's 2-bit tiles, and a species' front picture in its palettes (Gen II's Game Boy
// Color normal and shiny; Gen I's one, Super Game Boy or Game Boy Color) as upstream's nds::Sprite. The formats are
// pret's (pokecrystal, pokegold; pokered, pokeyellow), named where each is read (pics.cpp).
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

	// a w x h-tile picture, its tiles in columns as the games store a front picture, into out's first frame (8-bit
	// indices, the pixel's value: 0 is clear); false when tiles is short or a side isn't 1-7 tiles
	bool pictureFrame(const std::vector<uint8_t>& tiles, int w, int h, nds::Sprite& out);
	inline bool pictureFrame(const std::vector<uint8_t>& tiles, int size, nds::Sprite& out) { return pictureFrame(tiles, size, size, out); }

	// Gen I's picture compression (pret's home/uncompress.asm) from p's first n bytes: the size (dn w, h in tiles),
	// then two bit planes, each packets of bit pairs and runs of zeros, the second's unpacking mode (each plane
	// decoded across its rows, or the second also XORed with the first), into out as w x h 2-bit tiles in columns (16
	// bytes each, as LoadUncompressedSpriteData leaves them, the picture not padded to 7 x 7). false when a side isn't
	// 1-7 tiles or the stream runs past n. (Never flipped: the front pictures aren't)
	bool pic1(const uint8_t* p, size_t n, int& wTiles, int& hTiles, std::vector<uint8_t>& out);

	// a species' front picture (1 .. t.count; Unown's is letter A's), one frame, its two palettes from the ROM; null
	// when it doesn't decode. Safe from any thread: it only reads the ROM
	std::shared_ptr<const nds::Sprite> gen2FrontPic(const RomView& r, const Gen2Pics& t, uint16_t species);
	// Gen I's (1 .. t.pic's last), one frame at its own size, its one palette's four colours from the ROM (shiny the
	// same: no shiny Pokémon in Gen I); null when it doesn't decode. Safe from any thread
	std::shared_ptr<const nds::Sprite> gen1FrontPic(const RomView& r, const Gen1Pics& t, uint16_t species);
}
