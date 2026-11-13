#pragma once
// banana-ui (kiwami): the Game Boy Pokémon companion's ROM reader. The player's own ROM's tables as upstream's
// nds::RomData, in the conventions the DS companion's pages and logic work with, as ui/src/gba/gen3rom.h does for
// the GBA games: species by national number, types in nds::Type's order, items in Gen IV's numbering (items.inc;
// GEN2_ONLY + the game's own id for the rest), TMs as Gen IV numbers them (TM01-50 = 1-50, HM01-07 = 93-99),
// evolution methods in ui/src/nds/rom.h's nds::evo numbering, places by zone = map group << 8 | map number (Gen I: the
// map number), trainers by class << 8 | number (the battle's Battle::trainerId).
// Where the tables are is the data file's rom section (pret's symbols, PokeDaisy's Gen1Dex.kt and Gen2Dex.kt where
// they have them); their layouts are pret's source, named where each is read (rom1.cpp, rom2.cpp). Nothing of the
// games is in the image: every name, number and text comes from the ROM at run time.
// Copyright (C) 2026 Lidor Itzhari (PokeDaisy) and the kiwami fork's contributors. GNU GPL version 3 with PokeDaisy's
// additional term under section 7(b): LICENSE and NOTICE in this folder. SPDX-License-Identifier: GPL-3.0-only
// Based on PokeDaisy by Lidor Itzhari - https://github.com/lidor30/pokedaisy
#include "games.h"
#include "../../nds/model.h"
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace banana::kiwami::gbpk
{
	// Gen II's item ids in Gen IV's numbering where upstream's logic knows an item by it (items.inc), else
	// GEN2_ONLY + the id; 0 for 0
	constexpr uint16_t GEN2_ONLY = 1000;
	uint16_t gen2Item(uint8_t id);
	// Gen I's the same way (1000 + the id: a game is of one generation), its TMs and HMs as Gen IV's; and the pocket the
	// pages put it in (the game has one bag list): balls, TMs and HMs, the game's key items, the rest
	constexpr uint16_t GEN1_ONLY = 1000;
	uint16_t gen1Item(uint8_t id);
	nds::Pocket gen1Pocket(uint8_t id);
	// Gen II's types (pret's constants/type_constants.asm: Normal .. Rock 0-5, Bug, Ghost, Steel 7-9, Fire .. Dark
	// 20-27) in nds::Type's order; NO_TYPE for the others (Curse's ???)
	nds::Type gen2Type(uint8_t t);

	// The game's tables from the ROM (the whole file; kept for the sprites, decoded on demand). Null with the reason
	// in why when the data file lacks a table or a table's first entries don't check (another layout than the
	// release's: a patched ROM). Call it off the UI thread; the result is constant.
	std::shared_ptr<const nds::RomData> readRom(std::shared_ptr<const std::vector<uint8_t>> rom, const GameData& d,
		std::string* why = nullptr);

	// ---- for rom.cpp, rom1.cpp and rom2.cpp ----

	// the ROM's bytes by bank and address: bank 0 at 0000-3FFF, bank n at its 4000-7FFF window; its text in the
	// generation's character set
	struct RomView
	{
		static constexpr size_t NONE = (size_t)-1;
		const std::vector<uint8_t>& b;
		int gen = 2;
		// the file offset of bank:addr; NONE outside the ROM
		size_t at(int bank, uint32_t addr) const
		{
			if (addr >= 0x8000 || bank < 0) return NONE;
			size_t o = addr < 0x4000 ? addr : (size_t)bank * 0x4000 + (addr - 0x4000);
			return o < b.size() ? o : NONE;
		}
		size_t at(const Addr& a) const { return a.ok() ? at(a.bank, a.addr) : NONE; }
		bool has(size_t o, size_t n) const { return o != NONE && o <= b.size() && n <= b.size() - o; }
		uint8_t u8(size_t o) const { return o < b.size() ? b[o] : 0; }
		uint16_t le16(size_t o) const { return (uint16_t)(u8(o) | u8(o + 1) << 8); }
		// a pointer (dw) at o to the same bank as o: its file offset; NONE outside that bank
		size_t near(size_t o) const
		{
			const uint16_t p = le16(o);
			if (o == NONE || p >= 0x8000) return NONE;
			return p < 0x4000 ? at(0, p) : at((int)(o / 0x4000), p);
		}
		// the text at o up to its terminator (0x50), max bytes at most, as UTF-8 (text.h decode)
		std::string text(size_t o, size_t max) const;
		// the bytes from o up to and past the next terminator (0x50): the start of the next string of a list
		size_t skip(size_t o, size_t max = 64) const;
	};

	// where Gen II's front pictures are (file offsets; readGen2 fills it, pics.cpp decodes them): PokemonPicPointers and
	// UnownPicPointers (a bank and a pointer each, the front's then the back's), the bank fix's table (Crystal's
	// PicsBanks, or Gold's and Silver's list of pairs), PokemonPalettes, and BaseData for the picture's size
	struct Gen2Pics
	{
		size_t pointers = RomView::NONE, unown = RomView::NONE, banks = RomView::NONE, palettes = RomView::NONE,
			baseData = RomView::NONE;
		bool bankPairs = false;
		int count = 0;
	};

	// where Gen I's front pictures are (file offsets by national number, NONE when not there; readGen1 fills them, pics.cpp
	// decodes them): each species' compressed picture, in its bank by its internal index (home/pics.asm
	// UncompressMonSprite), and its palette's four colours (MonsterPalettes' id into the palettes)
	struct Gen1Pics { std::vector<size_t> pic, palette; };

	// a hidden item on a map (Gen II's: a map's background event of the item kind; Gen I's: a map's hidden event that
	// calls HiddenItems): the zone, where it lies on the map, the item (as items are numbered here) and the flag the
	// game sets once it's found (Gen II's an event flag, Gen I's its place in HiddenItemCoords)
	struct HiddenItem { uint16_t zone = 0; uint8_t x = 0, y = 0; uint16_t item = 0, flag = 0; };

	// what readRom gives; sprite() decodes a picture on demand (pics.cpp)
	struct GbRomData : nds::RomData
	{
		std::shared_ptr<const std::vector<uint8_t>> rom;
		int gen = 0;
		Gen2Pics pics;
		Gen1Pics pics1;
		std::vector<HiddenItem> hidden;		// by zone, in the maps' order (the Here page's secrets: host.cpp)
		std::vector<uint16_t> national;		// Gen I: a species' national number by its internal index (0: none)
		std::shared_ptr<const nds::Sprite> sprite(uint16_t species, bool female = false, uint8_t form = 0) const override;
	};
	// the games' "n percent" (n * 255 / 100) as n
	uint8_t percent(uint8_t raw);
	// the four moves a Pokémon knows at its level without a trainer's own (the games' FillMoves and WriteMonMoves: the
	// last four of its learnset up to its level, none twice)
	void defaultMoves(const nds::RomData& r, nds::TrainerMon& m);
	// an evolution's words ("Level 16", "Use Moon Stone"), the items by r's names
	std::string evoHow(const nds::RomData& r, uint16_t method, uint16_t param);

	// Gen I's tables into out (rom1.cpp)
	bool readGen1(const RomView& r, const GameData& d, GbRomData& out, std::string* why);
	// Gen II's tables into out (rom2.cpp)
	bool readGen2(const RomView& r, const GameData& d, GbRomData& out, std::string* why);
}
