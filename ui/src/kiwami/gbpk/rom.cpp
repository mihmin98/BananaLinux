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
	uint16_t gen2Item(const GameData& d, uint8_t id)
	{
		if (!id) return 0;
		auto i = d.items.find(id);
		return i != d.items.end() ? i->second : (uint16_t)(GEN2_ONLY + id);
	}

	namespace
	{
		const int GEN1_TMS = 50, GEN1_HMS = 5;
		// the data file's first TM and HM items (0: none, a file without them)
		int tm01(const GameData& d) { return d.size("tm01"); }
		int hm01(const GameData& d) { return d.size("hm01"); }
	}

	uint16_t gen1Item(const GameData& d, uint8_t id)
	{
		if (!id) return 0;
		auto i = d.items.find(id);
		if (i != d.items.end()) return i->second;
		// TM01-50 as Gen IV's TM01-50 (328-377), HM01-05 as its HM01-05 (420-424)
		if (tm01(d) > 0 && id >= tm01(d) && id < tm01(d) + GEN1_TMS) return (uint16_t)(328 + id - tm01(d));
		if (hm01(d) > 0 && id >= hm01(d) && id < hm01(d) + GEN1_HMS) return (uint16_t)(420 + id - hm01(d));
		return (uint16_t)(GEN1_ONLY + id);
	}

	nds::Pocket gen1Pocket(const GameData& d, uint8_t id)
	{
		if (d.balls.count(id)) return nds::POCKET_BALLS;	// (the Safari Ball too, a key item to the game)
		if (hm01(d) > 0 && id >= hm01(d) && id < tm01(d) + GEN1_TMS) return nds::POCKET_TMS;
		if (d.keyItems.count(id)) return nds::POCKET_KEY;
		return nds::POCKET_ITEMS;
	}

	nds::Type gen2Type(const GameData& d, uint8_t t)
	{
		return d.types[t];
	}

	std::string RomView::text(size_t o, size_t max) const
	{
		if (o == NONE || o >= b.size()) return "";
		if (dict != NONE) return decode(charset, b.data() + o, std::min(max, b.size() - o), TextDict{ b.data(), b.size(), dict, dictFirst });
		return decode(charset, b.data() + o, std::min(max, b.size() - o));
	}

	size_t RomView::skip(size_t o, size_t max) const
	{
		for (size_t i = 0; o != NONE && i < max && o + i < b.size(); i++)
			if (b[o + i] == 0x50) return o + i + 1;
		return NONE;
	}

	bool readTypeChart(const RomView& r, const GameData& d, size_t o, const char* name, TypeChart& out, std::string* why)
	{
		auto no = [&](const std::string& w) { if (why) *why = w; return false; };
		const uint8_t END = 0xFF, FORESIGHT = 0xFE;
		const int MAX_ENTRIES = 255;
		out.fill(-1);
		int n = 0;
		for (size_t e = o; ; e += 3)
		{
			if (!r.has(e, 1)) return no(std::string(name) + " runs past the ROM");
			if (r.u8(e) == END) break;
			if (r.u8(e) == FORESIGHT && !r.has(++e, 1)) return no(std::string(name) + " runs past the ROM");
			if (r.u8(e) == END) break;
			if (++n > MAX_ENTRIES || !r.has(e, 3)) return no(std::string(name) + " doesn't end");
			const int a = chartIndex(gen2Type(d, r.u8(e))), t = chartIndex(gen2Type(d, r.u8(e + 1)));
			const uint8_t m = r.u8(e + 2);
			if (a < 0 || t < 0 || (m != 0 && m != 5 && m != 10 && m != 20))
				return no(std::string(name) + "' entry " + std::to_string(n) + " isn't a type pair");
			int8_t& c = out[(size_t)(a * CHART_TYPES + t)];
			const int q = m * 4 / 10;		// (in quarters: 8, 4, 2, 0)
			c = (int8_t)(c < 0 ? q : c * q / 4);
		}
		if (out[(size_t)(chartIndex(nds::WATER) * CHART_TYPES + chartIndex(nds::FIRE))] != 8)
			return no(std::string(name) + " doesn't make Water super effective on Fire: not the game's chart");
		return true;
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

	// (no picture by gender in these games; by form only PureRGB's, each its own species past the Pokédex)
	std::shared_ptr<const nds::Sprite> GbRomData::sprite(uint16_t species, bool, uint8_t form) const
	{
		if (!rom) return nullptr;
		// (a form's index as upstream's nds::speciesIndex gives it, which the tests' binary doesn't link)
		const nds::Species* base = species < this->species.size() ? &this->species[species] : nullptr;
		const uint16_t i = form && base && base->formsAt && form < base->formCount ? (uint16_t)(base->formsAt + form - 1) : species;
		if (gen == 1) return gen1FrontPic(RomView{ *rom, Charset::GEN1 }, pics1, i);
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
		RomView r{ *rom, d.charset() };
		std::string w;
		if (d.gen != 1 && d.gen != 2) return no(d.game.title + ": not a Game Boy game's data file");
		// (Crystal Clear's packed text: its dictionary, from its first byte to 0x5F, three bytes an entry)
		if (d.rom.count("textDict"))
		{
			r.dict = r.at(d.rom.at("textDict"));
			r.dictFirst = d.size("textDictFirst");
			if (r.dictFirst < 1 || r.dictFirst >= 0x60 || !r.has(r.dict, 3 * (size_t)(0x60 - r.dictFirst)))
				return no(d.game.title + "'s ROM: its text's dictionary (rom.textDict, textDictFirst) isn't in the ROM");
		}
		if (!(d.gen == 1 ? readGen1(r, d, *out, &w) : readGen2(r, d, *out, &w))) return no(d.game.title + "'s ROM: " + w);
		// the rules the data file names: the types whose category goes by the attacker's stats, PureRGB's options (the
		// game has them where its data file reads them) and its Hyper Ball (an item of its own: rom.h gen1Item)
		for (int t = 0; t < 256; t++)
			if (d.splitStats[t] && chartIndex(d.types[t]) >= 0) out->rules.byStats[(size_t)chartIndex(d.types[t])] = true;
		out->rules.pureOptions = d.at("options3").ok();
		if (const int h = d.size("hyperBall"); d.gen == 1 && h > 0 && h < 256) out->rules.hyperBall = gen1Item(d, (uint8_t)h);
		return out;
	}

	int badgeTeam(uint16_t badges)
	{
		int n = 0;
		for (int b = 0; b < 16; b++) n += badges >> b & 1;
		return std::min(n, 15);
	}

	std::shared_ptr<const nds::RomData> withBadgeTeams(std::shared_ptr<const nds::RomData> rom, int team)
	{
		const auto* gb = dynamic_cast<const GbRomData*>(rom.get());
		if (!gb || gb->teams.empty() || team <= 0) return rom;
		auto out = std::make_shared<GbRomData>(*gb);
		for (const auto& [id, parties] : gb->teams)
			if (auto t = out->trainers.find(id); t != out->trainers.end() && !parties.empty())
				t->second.team = parties[std::min((size_t)team, parties.size() - 1)];
		return out;
	}

	std::shared_ptr<const nds::RomData> withOriginalTypes(std::shared_ptr<const nds::RomData> rom, uint32_t remap)
	{
		const auto* gb = dynamic_cast<const GbRomData*>(rom.get());
		if (!gb || gb->originalTypes.empty() || !remap) return rom;
		auto out = std::make_shared<GbRomData>(*gb);
		for (size_t n = 0; n < gb->originalTypes.size() && n < 32; n++)
		{
			const GbRomData::OriginalTyping& o = gb->originalTypes[n];
			if (!(remap >> n & 1) || o.species >= out->species.size()) continue;
			out->species[o.species].types[0] = o.types[0];
			out->species[o.species].types[1] = o.types[1];
		}
		return out;
	}
}
