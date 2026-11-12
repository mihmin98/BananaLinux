// banana-ui (kiwami): the Game Boy Pokémon companion's ROM reader (rom.h): what both generations share, and the
// generation's own reader (rom1.cpp for Red, Blue and Yellow, rom2.cpp for Gold, Silver and Crystal).
// Copyright (C) 2026 Lidor Itzhari (PokeDaisy) and the kiwami fork's contributors. GNU GPL version 3 with PokeDaisy's
// additional term under section 7(b): LICENSE and NOTICE in this folder. SPDX-License-Identifier: GPL-3.0-only
// Based on PokeDaisy by Lidor Itzhari - https://github.com/lidor30/pokedaisy
#include "rom.h"
#include "pics.h"
#include "text.h"
#include "../../nds/rom.h"
#include <algorithm>

namespace banana::kiwami::gbpk
{
	namespace
	{
		// Gen I's and II's item ids to Gen IV's (items.inc)
		struct ItemId { uint8_t id; uint16_t gen4; };
#include "items.inc"
	}

	uint16_t gen2Item(uint8_t id)
	{
		if (!id) return 0;
		for (const auto& e : GEN2_ITEMS) if (e.id == id) return e.gen4;
		return (uint16_t)(GEN2_ONLY + id);
	}

	uint16_t gen1Item(uint8_t id)
	{
		if (!id) return 0;
		for (const auto& e : GEN1_ITEMS) if (e.id == id) return e.gen4;
		// TM01-50 as Gen IV's TM01-50 (328-377), HM01-05 as its HM01-05 (420-424)
		if (id >= GEN1_TM01 && id < GEN1_TM01 + 50) return (uint16_t)(328 + id - GEN1_TM01);
		if (id >= GEN1_HM01 && id < GEN1_HM01 + 5) return (uint16_t)(420 + id - GEN1_HM01);
		return (uint16_t)(GEN1_ONLY + id);
	}

	nds::Pocket gen1Pocket(uint8_t id)
	{
		if ((id >= 0x01 && id <= 0x04) || id == 0x08) return nds::POCKET_BALLS;	// (the Safari Ball, a key item to the game)
		if (id >= GEN1_HM01 && id < GEN1_TM01 + 50) return nds::POCKET_TMS;
		for (uint8_t k : GEN1_KEY_ITEMS) if (k == id) return nds::POCKET_KEY;
		return nds::POCKET_ITEMS;
	}

	nds::Type gen2Type(uint8_t t)
	{
		if (t <= 5) return (nds::Type)t;
		if (t >= 7 && t <= 9) return (nds::Type)(t - 1);
		if (t >= 20 && t <= 27) return (nds::Type)(t - 20 + nds::FIRE);
		return nds::NO_TYPE;
	}

	std::string RomView::text(size_t o, size_t max) const
	{
		if (o == NONE || o >= b.size()) return "";
		return decode(gen, b.data() + o, std::min(max, b.size() - o));
	}

	size_t RomView::skip(size_t o, size_t max) const
	{
		for (size_t i = 0; o != NONE && i < max && o + i < b.size(); i++)
			if (b[o + i] == 0x50) return o + i + 1;
		return NONE;
	}

	uint8_t percent(uint8_t raw) { return (uint8_t)((raw * 100 + 127) / 255); }

	void defaultMoves(const nds::RomData& r, nds::TrainerMon& m)
	{
		if (m.species >= r.species.size()) return;
		std::vector<uint16_t> known;
		for (const auto& lm : r.species[m.species].learnset)
		{
			if (lm.level > m.level) break;
			if (std::find(known.begin(), known.end(), lm.move) == known.end()) known.push_back(lm.move);
		}
		size_t from = known.size() > 4 ? known.size() - 4 : 0;
		for (size_t j = from; j < known.size(); j++) m.moves[j - from] = known[j];
	}

	std::string evoHow(const nds::RomData& r, uint16_t method, uint16_t param)
	{
		auto item = [&](uint16_t i) { return i < r.items.size() && !r.items[i].name.empty() ? r.items[i].name : std::string("an item"); };
		const std::string lv = "Level " + std::to_string(param);
		switch (method)
		{
		case nds::evo::LEVEL: return lv;
		case nds::evo::USE_ITEM: return "Use " + item(param);
		case nds::evo::TRADE: return "Trade";
		case nds::evo::TRADE_ITEM: return "Trade holding " + item(param);
		case nds::evo::FRIENDSHIP: return "Level up with high friendship";
		case nds::evo::FRIENDSHIP_DAY: return "Level up with high friendship by day";
		case nds::evo::FRIENDSHIP_NIGHT: return "Level up with high friendship at night";
		case nds::evo::LEVEL_ATK_GT_DEF: return lv + " with Attack higher than Defense";
		case nds::evo::LEVEL_ATK_EQ_DEF: return lv + " with Attack equal to Defense";
		case nds::evo::LEVEL_ATK_LT_DEF: return lv + " with Defense higher than Attack";
		default: return "";
		}
	}

	// (no picture by gender or form in these games: one a species)
	std::shared_ptr<const nds::Sprite> GbRomData::sprite(uint16_t species, bool, uint8_t) const
	{
		if (!rom) return nullptr;
		if (gen == 1) return gen1FrontPic(RomView{ *rom, 1 }, pics1, species);
		return gen2FrontPic(RomView{ *rom }, pics, species);
	}

	std::shared_ptr<const nds::RomData> readRom(std::shared_ptr<const std::vector<uint8_t>> rom, const GameData& d,
		std::string* why)
	{
		auto no = [&](const std::string& w) -> std::shared_ptr<const nds::RomData> { if (why) *why = w; return nullptr; };
		if (!rom || rom->size() < 0x8000) return no("no ROM");
		auto out = std::make_shared<GbRomData>();
		out->rom = rom;
		out->game = d.game;
		out->gen = d.gen;
		RomView r{ *rom, d.gen };
		std::string w;
		if (d.gen != 1 && d.gen != 2) return no(d.game.title + ": not a Game Boy game's data file");
		if (!(d.gen == 1 ? readGen1(r, d, *out, &w) : readGen2(r, d, *out, &w))) return no(d.game.title + "'s ROM: " + w);
		return out;
	}
}
