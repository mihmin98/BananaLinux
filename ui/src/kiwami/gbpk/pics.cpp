// banana-ui (kiwami): the Game Boy Pokémon companion's pictures (pics.h): Gen II's LZ and its front pictures, as
// pret's pokecrystal and pokegold draw them (home/decompress.asm, engine/gfx/load_pics.asm, engine/gfx/color.asm);
// Gen I's compression and front pictures, as pret's pokered and pokeyellow do (home/uncompress.asm, home/pics.asm).
// Copyright (C) 2026 Lidor Itzhari (PokeDaisy) and the kiwami fork's contributors. GNU GPL version 3 with PokeDaisy's
// additional term under section 7(b): LICENSE and NOTICE in this folder. SPDX-License-Identifier: GPL-3.0-only
// Based on PokeDaisy by Lidor Itzhari - https://github.com/lidor30/pokedaisy
#include "pics.h"
#include <algorithm>

namespace banana::kiwami::gbpk
{
	namespace
	{
		// engine/gfx/load_pics.asm: FixPicBank's PICS_FIX (a picture's stored bank is its bank less this; Crystal's
		// PicsBanks has a bank for each of "Pics 1" .. "Pics 24"); Gold's and Silver's list of pairs ends at -1
		const int PICS_FIX = 0x36, PICS_BANKS = 24, MAX_FIXES = 8;
		const int PIC_POINTER = 6;			// data/pokemon/pic_pointers.asm: the front's bank and pointer, the back's
		const int BASE_DATA = 32, BASE_PIC_SIZE = 17;	// data/pokemon/base_stats.asm: the picture's size, dn w, h
		const int PALETTE = 8;				// data/pokemon/palettes.asm: the normal's two colours, the shiny's two
		const uint16_t UNOWN = 201;			// constants/pokemon_constants.asm: its pictures are by letter (A: the first)
		// ram/wram.asm wDecompressEnemyFrontpic: the buffer a front picture is decompressed into, $80 tiles
		const size_t PIC_BUFFER = 0x80 * 16;
		const uint32_t WHITE = 0xFFFFFF, BLACK = 0;	// engine/gfx/color.asm LoadPalette_White_Col1_Col2_Black
	}

	bool lz2(const uint8_t* p, size_t n, std::vector<uint8_t>& out, size_t maxOut)
	{
		out.clear();
		size_t i = 0;
		auto in = [&](uint8_t& v) { if (i >= n) return false; v = p[i++]; return true; };
		for (;;)
		{
			uint8_t b, x, y;
			if (!in(b)) return false;
			if (b == 0xFF) return true;					// LZ_END
			int cmd = b >> 5;
			size_t len;
			if (cmd == 7)								// LZ_LONG: 111ccc hh, then the length's low byte
			{
				cmd = b >> 2 & 7;
				if (!in(x)) return false;
				len = ((size_t)(b & 3) << 8 | x) + 1;
			}
			else len = (size_t)(b & 0x1F) + 1;
			if (len > maxOut - std::min(maxOut, out.size())) return false;
			switch (cmd)
			{
			case 0:										// LZ_LITERAL
				if (len > n - i) return false;
				out.insert(out.end(), p + i, p + i + len);
				i += len;
				break;
			case 1:										// LZ_ITERATE
				if (!in(x)) return false;
				out.insert(out.end(), len, x);
				break;
			case 2:										// LZ_ALTERNATE: the two bytes by turns, from the first
				if (!in(x) || !in(y)) return false;
				for (size_t k = 0; k < len; k++) out.push_back(k & 1 ? y : x);
				break;
			case 3:										// LZ_ZERO
				out.insert(out.end(), len, 0);
				break;
			default:
			{
				// a copy of what is written: a negative offset (bit 7, 7 bits) back from the end, less one; else 15
				// bits from the start. LZ_LONG's own command (7) copies as LZ_REPEAT does (the code falls through)
				if (!in(x)) return false;
				size_t from;
				if (x & 0x80)
				{
					if ((size_t)(x & 0x7F) + 1 > out.size()) return false;
					from = out.size() - (x & 0x7F) - 1;
				}
				else
				{
					if (!in(y)) return false;
					from = (size_t)x << 8 | y;
				}
				if (from >= out.size()) return false;
				if (cmd == 6)							// LZ_REVERSE: back from the offset
				{
					if (len > from + 1) return false;
					for (size_t k = 0; k < len; k++) out.push_back(out[from - k]);
				}
				else if (cmd == 5)						// LZ_FLIP: each byte's bits the other way round
					for (size_t k = 0; k < len; k++)
					{
						uint8_t v = out[from + k], f = 0;
						for (int j = 0; j < 8; j++) f |= (uint8_t)((v >> j & 1) << (7 - j));
						out.push_back(f);
					}
				else									// LZ_REPEAT (it may run into what it writes)
					for (size_t k = 0; k < len; k++) out.push_back(out[from + k]);
			}
			}
		}
	}

	uint32_t rgb555(uint16_t c)
	{
		const uint32_t r = c & 31, g = c >> 5 & 31, b = c >> 10 & 31;
		return (r << 3 | r >> 2) << 16 | (g << 3 | g >> 2) << 8 | (b << 3 | b >> 2);
	}

	bool pictureFrame(const std::vector<uint8_t>& tiles, int w, int h, nds::Sprite& out)
	{
		if (w < 1 || w > 7 || h < 1 || h > 7 || tiles.size() < (size_t)w * h * 16) return false;
		out.w = w * 8;
		out.h = h * 8;
		std::vector<uint8_t> f((size_t)out.w * out.h);
		for (int t = 0; t < w * h; t++)
		{
			const int tx = t / h, ty = t % h;	// (in columns)
			for (int y = 0; y < 8; y++)
				for (int x = 0; x < 8; x++) f[(size_t)(ty * 8 + y) * out.w + tx * 8 + x] = (uint8_t)tilePixel(&tiles[(size_t)t * 16], x, y);
		}
		// the white the edge reaches, side by side (an outline drawn corner to corner holds), stays clear; the rest is
		// the inside's (marked OUT while the fill runs)
		const uint8_t OUT = 0xFF;
		std::vector<int> todo;
		auto reach = [&](int x, int y) { if (x < 0 || y < 0 || x >= out.w || y >= out.h) return; uint8_t& v = f[(size_t)y * out.w + x]; if (v == 0) { v = OUT; todo.push_back(y * out.w + x); } };
		for (int x = 0; x < out.w; x++) { reach(x, 0); reach(x, out.h - 1); }
		for (int y = 0; y < out.h; y++) { reach(0, y); reach(out.w - 1, y); }
		while (!todo.empty())
		{
			const int at = todo.back(), x = at % out.w, y = at / out.w;
			todo.pop_back();
			reach(x - 1, y); reach(x + 1, y); reach(x, y - 1); reach(x, y + 1);
		}
		for (uint8_t& v : f) v = v == OUT ? 0 : v == 0 ? PIC_INSIDE : v;
		out.frames.assign(1, std::move(f));
		return true;
	}

	std::shared_ptr<const nds::Sprite> gen2FrontPic(const RomView& r, const Gen2Pics& t, uint16_t species)
	{
		if (species < 1 || species > t.count || t.pointers == RomView::NONE || t.palettes == RomView::NONE) return nullptr;
		// the picture's size (PadFrontpic: 5 and 6 as they are, any other 7)
		int size = r.u8(t.baseData + (size_t)BASE_DATA * (species - 1) + BASE_PIC_SIZE) & 0xF;
		if (size != 5 && size != 6) size = 7;
		// GetFrontpicPointer: the stored bank, fixed (FixPicBank), and the pointer in it
		const size_t e = species == UNOWN && t.unown != RomView::NONE ? t.unown : t.pointers + (size_t)PIC_POINTER * (species - 1);
		if (!r.has(e, 3)) return nullptr;
		int bank = r.u8(e);
		if (t.bankPairs)
		{
			for (int k = 0; k < MAX_FIXES && r.u8(t.banks + 2 * k) != 0xFF; k++)
				if (r.u8(t.banks + 2 * k) == bank) { bank = r.u8(t.banks + 2 * k + 1); break; }
		}
		else
		{
			// (the game's sub: 8 bits; PicsBanks' first is "Pics 1"'s bank)
			const int at = (bank - (r.u8(t.banks) - PICS_FIX)) & 0xFF;
			if (at >= PICS_BANKS) return nullptr;
			bank = r.u8(t.banks + at);
		}
		const uint16_t a = r.le16(e + 1);
		const size_t o = a >= 0x4000 ? r.at(bank, a) : RomView::NONE;
		if (o == RomView::NONE) return nullptr;
		// (the decompressor reads its bank's window: past 7FFF isn't the next bank)
		const size_t end = std::min(r.b.size(), (o / 0x4000 + 1) * 0x4000);
		std::vector<uint8_t> tiles;
		auto sp = std::make_shared<nds::Sprite>();
		if (!lz2(r.b.data() + o, end - o, tiles, PIC_BUFFER) || !pictureFrame(tiles, size, *sp)) return nullptr;
		// the palettes: white, the species' two colours, black; the shiny's two after the normal's
		// (GetMonNormalOrShinyPalettePointer)
		const size_t pal = t.palettes + (size_t)PALETTE * species;
		if (!r.has(pal, PALETTE)) return nullptr;
		sp->normal[0] = sp->shiny[0] = sp->normal[PIC_INSIDE] = sp->shiny[PIC_INSIDE] = WHITE;
		sp->normal[3] = sp->shiny[3] = BLACK;
		for (int k = 0; k < 2; k++)
		{
			sp->normal[1 + k] = rgb555(r.le16(pal + 2 * k));
			sp->shiny[1 + k] = rgb555(r.le16(pal + 4 + 2 * k));
		}
		return sp;
	}

	bool pic1(const uint8_t* p, size_t n, int& wTiles, int& hTiles, std::vector<uint8_t>& out)
	{
		out.clear();
		if (n < 1) return false;
		wTiles = p[0] >> 4;
		hTiles = p[0] & 0xF;
		if (wTiles < 1 || wTiles > 7 || hTiles < 1 || hTiles > 7) return false;
		// ReadNextInputBit: the stream's bits from each byte's highest; past n, the stream has run out
		size_t i = 1;
		int left = 0;
		uint8_t cur = 0;
		bool ok = true;
		auto bit = [&]() -> int {
			if (!left)
			{
				if (i >= n) { ok = false; return 0; }
				cur = p[i++];
				left = 8;
			}
			return cur >> --left & 1;
		};
		// two 1-bit planes, each wTiles columns of 8 px by H rows, a byte a row of a column (sSpriteBuffer1, 2)
		const int H = hTiles * 8;
		const size_t size = (size_t)wTiles * H;
		std::vector<uint8_t> plane[2] = { std::vector<uint8_t>(size), std::vector<uint8_t>(size) };
		const int first = bit();	// the plane the first chunk goes in (wSpriteLoadFlags' bit 0)
		int mode = 0;
		for (int chunk = 0; chunk < 2 && ok; chunk++)
		{
			std::vector<uint8_t>& b = plane[first ^ chunk];
			if (chunk == 1) mode = bit() ? 1 + bit() : 0;	// (the last chunk's unpacking mode: 0, 10 or 11)
			// WriteSpriteBitsToBuffer and MoveToNextBufferPosition: a pair at bits 7-6 of each row of a column, then
			// 5-4, 3-2, 1-0, then the next column; the chunk ends at its last pair, a run of zeros cut there
			int col = 0, y = 0, shift = 6;
			bool done = false;
			auto put = [&](int pair) {
				b[(size_t)col * H + y] |= (uint8_t)(pair << shift);
				if (++y < H) return;
				y = 0;
				if (shift) shift -= 2;
				else { shift = 6; done = ++col == wTiles; }
			};
			bool zeros = !bit();	// (the chunk starts with a run of zeros or with pairs)
			while (!done && ok)
			{
				if (zeros)
				{
					// a run: c ones and a 0, then c + 1 bits, plus LengthEncodingOffsetList's 2^(c + 1) - 1 (16 entries;
					// 16-bit arithmetic: 0 runs 65536)
					int c = 0;
					while (bit() && ok)
						if (++c > 15) return false;
					uint32_t v = 0;
					for (int k = 0; k <= c; k++) v = v << 1 | (uint32_t)bit();
					uint32_t run = (v + (2u << c) - 1) & 0xFFFF;
					if (!run) run = 0x10000;
					for (; run && !done && ok; run--) put(0);
					zeros = false;
				}
				else
				{
					const int hi = bit(), pair = hi << 1 | bit();
					if (!pair) zeros = true;
					else put(pair);
				}
			}
		}
		if (!ok) return false;
		// SpriteDifferentialDecode: across each row, a 1 bit flips the value, from 0 at the row's start
		auto delta = [&](std::vector<uint8_t>& b) {
			for (int y = 0; y < H; y++)
			{
				int last = 0;
				for (int col = 0; col < wTiles; col++)
				{
					const uint8_t v = b[(size_t)col * H + y];
					uint8_t o = 0;
					for (int k = 7; k >= 0; k--) { last ^= v >> k & 1; o |= (uint8_t)(last << k); }
					b[(size_t)col * H + y] = o;
				}
			}
		};
		// UnpackSprite: mode 0 both planes decoded; 1 the first chunk's decoded and XORed into the second's; 2 both
		// decoded, the second's first, then the same XOR (XorSpriteChunks, UnpackSpriteMode2)
		std::vector<uint8_t>& a = plane[first];
		std::vector<uint8_t>& c = plane[first ^ 1];
		if (mode == 0) { delta(plane[0]); delta(plane[1]); }
		else
		{
			if (mode == 2) delta(c);
			delta(a);
			for (size_t k = 0; k < size; k++) c[k] ^= a[k];
		}
		// InterlaceMergeSpriteBuffers: sSpriteBuffer1 is each row's low plane, sSpriteBuffer2 its high one
		out.assign(size * 2, 0);
		for (int col = 0; col < wTiles; col++)
			for (int y = 0; y < H; y++)
			{
				const size_t t = (size_t)col * hTiles + y / 8, o = t * 16 + (size_t)(y % 8) * 2;
				out[o] = plane[0][(size_t)col * H + y];
				out[o + 1] = plane[1][(size_t)col * H + y];
			}
		return true;
	}

	std::shared_ptr<const nds::Sprite> gen1FrontPic(const RomView& r, const Gen1Pics& t, uint16_t species)
	{
		if (species < 1 || species >= t.pic.size() || species >= t.palette.size()) return nullptr;
		const size_t o = t.pic[species], pal = t.palette[species];
		if (o == RomView::NONE || !r.has(o, 1) || !r.has(pal, 8)) return nullptr;
		// (the stream is read in its bank's window: past 7FFF isn't the next bank)
		const size_t end = std::min(r.b.size(), (o / 0x4000 + 1) * 0x4000);
		std::vector<uint8_t> tiles;
		int w, h;
		auto sp = std::make_shared<nds::Sprite>();
		if (!pic1(r.b.data() + o, end - o, w, h, tiles) || !pictureFrame(tiles, w, h, *sp)) return nullptr;
		for (int k = 0; k < 4; k++) sp->normal[k] = sp->shiny[k] = rgb555(r.le16(pal + 2 * k));
		sp->normal[PIC_INSIDE] = sp->shiny[PIC_INSIDE] = sp->normal[0];
		return sp;
	}
}
