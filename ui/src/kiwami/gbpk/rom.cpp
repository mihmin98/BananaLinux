// banana-ui (kiwami): the Game Boy Pokémon companion's ROM reader (rom.h): what both generations share, and the
// generation's own reader (rom2.cpp for Gold, Silver and Crystal).
// Copyright (C) 2026 Lidor Itzhari (PokeDaisy) and the kiwami fork's contributors. GNU GPL version 3 with PokeDaisy's
// additional term under section 7(b): LICENSE and NOTICE in this folder. SPDX-License-Identifier: GPL-3.0-only
// Based on PokeDaisy by Lidor Itzhari - https://github.com/lidor30/pokedaisy
#include "rom.h"
#include "pics.h"
#include "text.h"
#include <algorithm>

namespace banana::kiwami::gbpk
{
	namespace
	{
		// Gen II's item ids to Gen IV's (items.inc)
		const struct { uint8_t gen2; uint16_t gen4; } ITEMS[] = {
#include "items.inc"
		};
	}

	uint16_t gen2Item(uint8_t id)
	{
		if (!id) return 0;
		for (const auto& e : ITEMS) if (e.gen2 == id) return e.gen4;
		return (uint16_t)(GEN2_ONLY + id);
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
		return decode(2, b.data() + o, std::min(max, b.size() - o));
	}

	size_t RomView::skip(size_t o, size_t max) const
	{
		for (size_t i = 0; o != NONE && i < max && o + i < b.size(); i++)
			if (b[o + i] == 0x50) return o + i + 1;
		return NONE;
	}

	// (no picture by gender or form in these games: one a species)
	std::shared_ptr<const nds::Sprite> GbRomData::sprite(uint16_t species, bool, uint8_t) const
	{
		if (!rom || gen != 2) return nullptr;
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
		RomView r{ *rom };
		std::string w;
		if (d.gen == 2)
		{
			if (!readGen2(r, d, *out, &w)) return no(d.game.title + "'s ROM: " + w);
			return out;
		}
		return no(d.game.title + "'s ROM: Generation I's tables aren't read yet");
	}
}
