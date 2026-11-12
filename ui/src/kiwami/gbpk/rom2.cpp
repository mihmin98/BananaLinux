// banana-ui (kiwami): Gold's, Silver's and Crystal's tables from the player's ROM (rom.h). The layouts are pret's
// (pokecrystal; pokegold the same), named at each table below; where each table is comes from the data file. Each
// table is checked as it is read: every base data record names its own species and the first is Bulbasaur's, every
// move record its own animation and the first is Pound's, the lists end where pret's do, and a pointer that leaves
// its bank stops the read.
// Copyright (C) 2026 Lidor Itzhari (PokeDaisy) and the kiwami fork's contributors. GNU GPL version 3 with PokeDaisy's
// additional term under section 7(b): LICENSE and NOTICE in this folder. SPDX-License-Identifier: GPL-3.0-only
// Based on PokeDaisy by Lidor Itzhari - https://github.com/lidor30/pokedaisy
#include "rom.h"
#include "maps.h"
#include "pics.h"
#include "text.h"
#include "../../nds/rom.h"
#include <algorithm>
#include <cstdio>
#include <cstring>

namespace banana::kiwami::gbpk
{
	namespace
	{
		using nds::Encounter;
		using nds::EncounterKind;
		using nds::TimeOfDay;

		// pret's layouts and the games' fixed numbers
		const int BASE_DATA = 32;			// data/pokemon/base_stats.asm: species, HP .. Sp. Def, types, catch rate, base
											// exp, items, gender, 3, picture size, 4, growth, egg groups, TM/HM bits
		const int MOVE = 7;					// data/moves/moves.asm: animation (the move's id), effect, power, type,
											// accuracy, PP, effect chance (the two in n * 255 / 100)
		const int MON_NAME = 10;			// data/pokemon/names.asm
		const int ITEM_ATTR = 7;			// data/items/attributes.asm: price (dw), held effect, parameter, property,
											// pocket, menus
		const int LANDMARK = 4;				// data/maps/landmarks.asm: x, y, the name's pointer
		const int MAP_HEADER = 9;			// data/maps/maps.asm: attributes' bank, tileset, environment, attributes'
											// pointer, landmark, music, phone and palette, fishing group
		const int MAP_ATTRIBUTES = 12;		// data/maps/attributes.asm: border, height, width, the blocks' bank and
											// pointer, the scripts' (and events') bank and pointer, the events' pointer,
											// connections
		const int WARP = 5, COORD_EVENT = 8, BG_EVENT = 5;	// macros/scripts/maps.asm (a bg event: y, x, function,
											// script)
		const uint8_t BGEVENT_ITEM = 7;		// constants/script_constants.asm
		const int GRASS_SLOTS = 7, WATER_SLOTS = 3;
		const int GRASS = 2 + 3 + 3 * GRASS_SLOTS * 2;	// data/wild/*_grass.asm: map, three rates, morning, day, night
		const int WATER = 2 + 1 + WATER_SLOTS * 2;		// data/wild/*_water.asm: map, rate, the slots
		const int FISH_GROUP = 7;			// data/wild/fish.asm: the bite chance, the three rods' tables
		const int TIME_FISH = 4;			// data/wild/fish.asm TimeFishGroups: day species and level, night's
		const int NUM_TMS = 50, NUM_HMS = 7;	// constants/item_constants.asm (the TM/HM bits, then the tutors')
		const uint8_t BASE_HAPPINESS = 70;	// constants/pokemon_data_constants.asm
		const int BASE_PRIORITY = 1;		// constants/battle_constants.asm (GetMovePriority's default)
		const int SURF_LEVELS = 4;			// engine/overworld/wildmons.asm ChooseWildEncounter: a surfing one's + 0-4
		const uint8_t END = 0xFF;
		// constants/pokemon_data_constants.asm, constants/trainer_data_constants.asm, constants/item_data_constants.asm
		enum { EVOLVE_LEVEL = 1, EVOLVE_ITEM, EVOLVE_TRADE, EVOLVE_HAPPINESS, EVOLVE_STAT };
		enum { TR_ANYTIME = 1, TR_MORNDAY, TR_NITE };
		enum { ATK_GT_DEF = 1, ATK_LT_DEF, ATK_EQ_DEF };
		enum { TRAINERTYPE_MOVES = 1, TRAINERTYPE_ITEM = 2 };
		enum { ITEM = 1, KEY_ITEM, BALL, TM_HM };

		// the games' "n percent" (n * 255 / 100) as n
		uint8_t percent(uint8_t raw) { return (uint8_t)((raw * 100 + 127) / 255); }

		// the four moves a Pokémon knows at its level without a trainer's own (the game's FillMoves: the last four
		// learned by level up)
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

		// a map's hidden items: the events its attributes point to (in the scripts' bank: two filler bytes, then the
		// warps, the coord events and the bg events, each a count and its records), the bg events of the item kind, and
		// the hiddenitem each points to (macros/scripts/maps.asm: the flag, dw, then the item). A map whose events
		// don't read has none.
		void hiddenItems(const RomView& r, size_t header, uint16_t zone, GbRomData& out)
		{
			const size_t a = r.at(r.u8(header), r.le16(header + 3));
			if (!r.has(a, MAP_ATTRIBUTES)) return;
			const int bank = r.u8(a + 6);
			size_t e = r.at(bank, r.le16(a + 9));
			if (!r.has(e, 3)) return;
			e += 2;
			for (int size : { WARP, COORD_EVENT })
			{
				e += 1 + (size_t)size * r.u8(e);
				if (!r.has(e, 1)) return;
			}
			const int n = r.u8(e++);
			if (!r.has(e, (size_t)BG_EVENT * n)) return;
			for (int i = 0; i < n; i++, e += BG_EVENT)
			{
				if (r.u8(e + 2) != BGEVENT_ITEM) continue;
				const size_t s = r.at(bank, r.le16(e + 3));
				if (r.has(s, 3)) out.hidden.push_back({ zone, r.u8(e + 1), r.u8(e), gen2Item(r.u8(s + 2)), r.le16(s) });
			}
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
	}

	bool readGen2(const RomView& r, const GameData& d, GbRomData& out, std::string* why)
	{
		auto no = [&](const std::string& w) { if (why) *why = w; return false; };
		std::string missing;
		auto table = [&](const char* k) {
			size_t o = r.at(d.rom.count(k) ? d.rom.at(k) : Addr{});
			if (o == RomView::NONE && missing.empty()) missing = k;
			return o;
		};
		const size_t baseData = table("baseData"), names = table("pokemonNames"), moves = table("moves"),
			moveNames = table("moveNames"), moveDesc = table("moveDescriptions"), prio = table("moveEffectPriorities"),
			itemNames = table("itemNames"), itemAttr = table("itemAttributes"), itemDesc = table("itemDescriptions"),
			evos = table("evosAttacksPointers"), tmHm = table("tmHmMoves"), dexPtrs = table("dexEntryPointers"),
			grassProbs = table("grassProbs"), waterProbs = table("waterProbs"), fish = table("fishGroups"),
			timeFish = table("timeFishGroups"), treeMaps = table("treeMonMaps"), rockMaps = table("rockMonMaps"),
			treeMons = table("treeMons"), trainers = table("trainerGroups"), classNames = table("trainerClassNames"),
			landmarks = table("landmarks"), mapGroups = table("mapGroups"), mapsEnd = table("mapHeadersEnd"),
			picPointers = table("picPointers"), unownPics = table("unownPicPointers"), monPalettes = table("monPalettes");
		// a picture's bank: Crystal's PicsBanks, or Gold's and Silver's pairs (pics.cpp)
		const bool bankPairs = d.rom.count("picBankFixes") > 0;
		const size_t picBanks = table(bankPairs ? "picBankFixes" : "picsBanks");
		for (const char* k : { "townMapGfx", "johtoMap", "kantoMap", "townMapPalMap", "pokegearPals" }) table(k);
		const size_t grass[2] = { table("johtoGrass"), table("kantoGrass") }, water[2] = { table("johtoWater"), table("kantoWater") };
		// a Pokédex entry's bank: Crystal's table of four, or Gold's and Silver's first bank and the three after it
		const size_t dexBanks = d.rom.count("dexEntryBanks") ? r.at(d.rom.at("dexEntryBanks")) : RomView::NONE;
		const int dexFirst = d.rom.count("dexEntryFirst") ? d.rom.at("dexEntryFirst").bank : -1;
		if (!missing.empty()) return no("no rom." + missing + " in the data file, or it is past the ROM's end");
		if (dexBanks == RomView::NONE && dexFirst < 0) return no("no rom.dexEntryBanks or rom.dexEntryFirst in the data file");
		const int nSp = d.size("numPokemon"), nMoves = d.size("numMoves"), nClasses = d.size("numTrainerClasses"),
			nLandmarks = d.size("numLandmarks"), firstTm = d.size("firstTm"), vitalThrow = d.size("vitalThrow");
		if (nSp < 1 || nSp > 254 || nMoves < 1 || nMoves > 254 || nClasses < 1 || nClasses > 255 || nLandmarks < 1 ||
			nLandmarks > 255 || firstTm < 2 || firstTm > 256)
			return no("the data file's counts (numPokemon, numMoves, numTrainerClasses, numLandmarks, firstTm) aren't Gen II's");

		// the species' base data, checked (BASE_DEX_NO in each record; Bulbasaur's base stats first)
		static const uint8_t BULBASAUR[6] = { 45, 49, 49, 45, 65, 65 };
		if (!r.has(baseData, (size_t)BASE_DATA * nSp) || !r.has(names, (size_t)MON_NAME * nSp)) return no("BaseData or PokemonNames runs past the ROM");
		for (int s = 1; s <= nSp; s++)
			if (r.u8(baseData + BASE_DATA * (s - 1)) != s) return no("BaseData's record " + std::to_string(s) + " isn't species " + std::to_string(s) + "'s");
		if (memcmp(r.b.data() + baseData + 1, BULBASAUR, 6) != 0) return no("BaseData's first record isn't Bulbasaur's");
		// the moves, checked (each record's animation is its move; Pound first: Normal, 40, 35 PP)
		if (!r.has(moves, (size_t)MOVE * nMoves)) return no("Moves runs past the ROM");
		for (int m = 1; m <= nMoves; m++)
			if (r.u8(moves + MOVE * (m - 1)) != m) return no("Moves' record " + std::to_string(m) + " isn't move " + std::to_string(m) + "'s");
		if (r.u8(moves + 2) != 40 || r.u8(moves + 3) != 0 || r.u8(moves + 5) != 35) return no("Moves' first record isn't Pound's");
		// the pictures, checked (PokemonPalettes' first entry, no species', is RGB 30, 22, 17 and 16, 14, 19 twice;
		// Bulbasaur's front picture decodes)
		out.pics.pointers = picPointers;
		out.pics.unown = unownPics;
		out.pics.banks = picBanks;
		out.pics.bankPairs = bankPairs;
		out.pics.palettes = monPalettes;
		out.pics.baseData = baseData;
		out.pics.count = nSp;
		static const uint8_t PALETTE_0[8] = { 0xDE, 0x46, 0xD0, 0x4D, 0xDE, 0x46, 0xD0, 0x4D };
		if (!r.has(monPalettes, 8 * ((size_t)nSp + 1)) || memcmp(r.b.data() + monPalettes, PALETTE_0, 8) != 0)
			return no("PokemonPalettes' first entry isn't the games'");
		if (!gen2FrontPic(r, out.pics, 1)) return no("Bulbasaur's front picture doesn't decode");

		// moves: names (a list of strings), descriptions (a pointer each), priorities (MoveEffectPriorities: effect,
		// priority pairs to -1; Vital Throw goes last, by its id; the rest BASE_PRIORITY), the category by the type
		// (Normal .. Steel physical, Fire .. Dark special; no power: status)
		out.moves.resize((size_t)nMoves + 1);
		size_t mn = moveNames;
		for (int m = 1; m <= nMoves; m++)
		{
			const size_t e = moves + MOVE * (m - 1);
			nds::MoveData& v = out.moves[m];
			if (mn == RomView::NONE) return no("MoveNames ends before move " + std::to_string(m));
			v.name = nameCase(r.text(mn, 16));
			mn = r.skip(mn, 16);
			v.effect = r.u8(e + 1);
			v.power = r.u8(e + 2);
			v.type = gen2Type(r.u8(e + 3));
			v.accuracy = percent(r.u8(e + 4));
			v.pp = r.u8(e + 5);
			v.effectChance = percent(r.u8(e + 6));
			v.cat = v.power == 0 ? nds::MoveCategory::STATUS : r.u8(e + 3) < 20 ? nds::MoveCategory::PHYSICAL : nds::MoveCategory::SPECIAL;
			int p = BASE_PRIORITY;
			if (m == vitalThrow) p = 0;
			else
				for (size_t q = prio; r.has(q, 2) && r.u8(q) != END && q < prio + 64; q += 2)
					if (r.u8(q) == v.effect) { p = r.u8(q + 1); break; }
			v.priority = (uint8_t)(int8_t)(p - BASE_PRIORITY);
			v.description = r.text(r.near(moveDesc + 2 * (m - 1)), 200);
		}

		// items: below TM01 each its name, attributes and description, at its Gen IV id or GEN2_ONLY + its own; the TMs
		// and HMs as Gen IV numbers them (TM01 328, HM01 420)
		out.items.resize((size_t)GEN2_ONLY + 256);
		size_t in = itemNames;
		for (int i = 1; i < firstTm && i < 256; i++)
		{
			if (in == RomView::NONE) return no("ItemNames ends before item " + std::to_string(i));
			nds::ItemData& it = out.items[gen2Item((uint8_t)i)];
			it.name = nameCase(r.text(in, 16));
			in = r.skip(in, 16);
			const size_t a = itemAttr + ITEM_ATTR * (size_t)(i - 1);
			if (!r.has(a, ITEM_ATTR)) return no("ItemAttributes runs past the ROM");
			it.price = r.le16(a);
			const uint8_t pocket = r.u8(a + 5);
			it.pocket = pocket == BALL ? nds::POCKET_BALLS : pocket == KEY_ITEM ? nds::POCKET_KEY : pocket == TM_HM ? nds::POCKET_TMS : nds::POCKET_ITEMS;
			it.description = r.text(r.near(itemDesc + 2 * (size_t)(i - 1)), 200);
		}
		for (int n = 1; n <= NUM_TMS + NUM_HMS; n++)
		{
			char nm[8];
			snprintf(nm, sizeof nm, n <= NUM_TMS ? "TM%02d" : "HM%02d", n <= NUM_TMS ? n : n - NUM_TMS);
			nds::ItemData& it = out.items[n <= NUM_TMS ? 327 + n : 419 + n - NUM_TMS];
			it.name = nm;
			it.pocket = nds::POCKET_TMS;
		}
		// TMHMMoves: a move each TM and HM teaches, in their order (the tutors' after them)
		out.tmMoves.assign(100, 0);
		if (!r.has(tmHm, NUM_TMS + NUM_HMS)) return no("TMHMMoves runs past the ROM");
		for (int k = 0; k < NUM_TMS + NUM_HMS; k++)
			out.tmMoves[k < NUM_TMS ? k : 92 + k - NUM_TMS] = r.u8(tmHm + k);

		// species
		out.species.resize((size_t)nSp + 1);
		for (int s = 1; s <= nSp; s++)
		{
			nds::Species& sp = out.species[s];
			const size_t b = baseData + BASE_DATA * (s - 1);
			sp.name = nameCase(r.text(names + MON_NAME * (s - 1), MON_NAME));
			for (int i = 0; i < 6; i++) sp.base[i] = r.u8(b + 1 + i);
			sp.types[0] = gen2Type(r.u8(b + 7));
			sp.types[1] = gen2Type(r.u8(b + 8));
			sp.catchRate = r.u8(b + 9);
			sp.baseExp = r.u8(b + 10);
			sp.genderRatio = r.u8(b + 13);
			sp.growth = r.u8(b + 22);
			sp.baseFriendship = BASE_HAPPINESS;
			for (int k = 0; k < NUM_TMS + NUM_HMS; k++)
				if (r.u8(b + 24 + k / 8) >> (k % 8) & 1) sp.tms.push_back((uint16_t)(k < NUM_TMS ? k + 1 : 93 + k - NUM_TMS));
			// the Pokédex entry (engine/pokedex/pokedex_2.asm GetDexEntryPointer: a pointer each, in the bank of the
			// species' 64): category, height (feet * 100 + inches), weight (pounds * 10), two pages
			const int q = ((s - 1) >> 6) & 3;
			const size_t e = r.at(dexBanks != RomView::NONE ? r.u8(dexBanks + q) : dexFirst + q, r.le16(dexPtrs + 2 * (s - 1)));
			const size_t hw = r.skip(e, 16);
			if (hw != RomView::NONE && r.has(hw, 4))
			{
				sp.category = nameCase(r.text(e, 16));
				const int h = r.le16(hw), inches = h / 100 * 12 + h % 100, lb = r.le16(hw + 2);
				sp.height = (uint16_t)((inches * 254 + 500) / 1000);		// in 0.1 m
				// in 0.1 kg, as the game reckons it for the Heavy Ball (engine/items/item_effects.asm HeavyBallMultiplier:
				// the pounds halved, less a 16th of that and a 32nd)
				const int half = lb >> 1;
				sp.weight = (uint16_t)(half - (half >> 4) - (half >> 5));
				const size_t p1 = hw + 4, p2 = r.skip(p1, 200);
				sp.dexText = r.text(p1, 200);
				if (p2 != RomView::NONE)
				{
					const std::string t2 = r.text(p2, 200);
					if (!t2.empty()) sp.dexText += (sp.dexText.empty() ? "" : " ") + t2;
				}
			}
			// evolutions, then the moves learned by level (data/pokemon/evos_attacks.asm: a pointer each in the same bank)
			size_t p = r.near(evos + 2 * (s - 1));
			if (p == RomView::NONE) return no("EvosAttacksPointers' pointer for species " + std::to_string(s) + " leaves its bank");
			for (int k = 0; k < 8 && r.u8(p); k++)
			{
				const uint8_t kind = r.u8(p);
				nds::Evolution ev;
				if (kind == EVOLVE_STAT)
				{
					const uint8_t c = r.u8(p + 2);
					ev.method = c == ATK_GT_DEF ? nds::evo::LEVEL_ATK_GT_DEF : c == ATK_LT_DEF ? nds::evo::LEVEL_ATK_LT_DEF : nds::evo::LEVEL_ATK_EQ_DEF;
					ev.param = r.u8(p + 1);
					ev.into = r.u8(p + 3);
					p += 4;
				}
				else
				{
					const uint8_t a = r.u8(p + 1);
					ev.into = r.u8(p + 2);
					p += 3;
					switch (kind)
					{
					case EVOLVE_LEVEL: ev.method = nds::evo::LEVEL; ev.param = a; break;
					case EVOLVE_ITEM: ev.method = nds::evo::USE_ITEM; ev.param = gen2Item(a); break;
					case EVOLVE_TRADE: ev.method = a == END ? nds::evo::TRADE : nds::evo::TRADE_ITEM; ev.param = a == END ? 0 : gen2Item(a); break;
					case EVOLVE_HAPPINESS: ev.method = a == TR_MORNDAY ? nds::evo::FRIENDSHIP_DAY : a == TR_NITE ? nds::evo::FRIENDSHIP_NIGHT : nds::evo::FRIENDSHIP; break;
					default: return no("species " + std::to_string(s) + " has an evolution of a kind Gen II hasn't (" + std::to_string(kind) + ")");
					}
				}
				if (ev.into >= 1 && ev.into <= nSp) sp.evolutions.push_back(ev);
			}
			p++;
			for (int k = 0; k < 40 && r.u8(p); k++, p += 2)
				if (r.u8(p + 1) >= 1 && r.u8(p + 1) <= nMoves) sp.learnset.push_back({ r.u8(p), r.u8(p + 1) });
		}
		for (auto& sp : out.species)
			for (auto& ev : sp.evolutions) ev.how = evoHow(out, ev.method, ev.param);

		// places: the landmarks' names, then every map of every group (MapGroupPointers: a pointer a group, the groups'
		// headers one after another up to the first map's attributes), named by its landmark
		out.locationNames.resize((size_t)nLandmarks);
		if (!r.has(landmarks, (size_t)LANDMARK * nLandmarks)) return no("Landmarks runs past the ROM");
		for (int i = 0; i < nLandmarks; i++) out.locationNames[i] = nameCase(r.text(r.near(landmarks + LANDMARK * i + 2), 24));
		std::vector<size_t> groups;
		size_t firstGroup = RomView::NONE;
		for (size_t o = mapGroups; o + 2 <= firstGroup && groups.size() < 64; o += 2)
		{
			const size_t g = r.near(o);
			if (g == RomView::NONE || g <= o) return no("MapGroupPointers' pointer " + std::to_string(groups.size() + 1) + " isn't after the table");
			groups.push_back(g);
			firstGroup = std::min(firstGroup, g);
		}
		if (mapsEnd / 0x4000 != mapGroups / 0x4000 || mapsEnd <= firstGroup) return no("rom.mapHeadersEnd isn't after the map groups");
		std::map<uint16_t, uint8_t> fishGroupOf, landmarkOf;
		for (size_t g = 0; g < groups.size(); g++)
		{
			size_t end = mapsEnd;
			for (size_t o : groups) if (o > groups[g]) end = std::min(end, o);
			if (end > mapsEnd || (end - groups[g]) % MAP_HEADER || end == groups[g] || (end - groups[g]) / MAP_HEADER > 255)
				return no("map group " + std::to_string(g + 1) + "'s headers aren't whole");
			for (size_t m = 1; m <= (end - groups[g]) / MAP_HEADER; m++)
			{
				const size_t h = groups[g] + MAP_HEADER * (m - 1);
				const uint16_t z = (uint16_t)((g + 1) << 8 | m);
				nds::Location& loc = out.zones[z];
				loc.id = z;
				const uint8_t lm = r.u8(h + 5);
				if (lm < out.locationNames.size()) loc.name = out.locationNames[lm];
				landmarkOf[z] = lm;
				if (r.u8(h + 8)) fishGroupOf[z] = r.u8(h + 8);
				hiddenItems(r, h, z, out);
			}
		}
		auto zone = [&](size_t o) -> nds::Location& {
			const uint16_t z = (uint16_t)(r.u8(o) << 8 | r.u8(o + 1));
			nds::Location& loc = out.zones[z];
			loc.id = z;
			return loc;
		};
		auto has = [](const nds::Location& l, EncounterKind k) {
			for (const auto& e : l.encounters) if (e.kind == k) return true;
			return false;
		};
		auto add = [&](nds::Location& l, uint8_t species, uint8_t lo, uint8_t hi, int pct, EncounterKind k, TimeOfDay t, uint8_t cond = 0) {
			if (species < 1 || species > nSp || !lo) return;
			Encounter e;
			e.species = species;
			e.minLevel = lo;
			e.maxLevel = hi;
			e.percent = (uint8_t)std::clamp(pct, 0, 100);
			e.kind = k;
			e.time = t;
			e.condition = cond;
			l.encounters.push_back(e);
		};

		// the slots' chances (data/wild/probabilities.asm: cumulative percent, slot * 2)
		int grassPct[GRASS_SLOTS], waterPct[WATER_SLOTS];
		auto probs = [&](size_t o, int* pct, int n) {
			int last = 0;
			for (int j = 0; j < n; j++)
			{
				if (!r.has(o + 2 * j, 2) || r.u8(o + 2 * j + 1) != 2 * j || r.u8(o + 2 * j) <= last) return false;
				pct[j] = r.u8(o + 2 * j) - last;
				last = r.u8(o + 2 * j);
			}
			return last == 100;
		};
		if (!probs(grassProbs, grassPct, GRASS_SLOTS) || !probs(waterProbs, waterPct, WATER_SLOTS)) return no("GrassMonProbTable or WaterMonProbTable isn't 100 percent");
		// grass by morning, day and night; water, whose levels the game raises by 0-4 (a map's first table each)
		static const TimeOfDay TIMES[3] = { TimeOfDay::MORNING, TimeOfDay::DAY, TimeOfDay::NIGHT };
		for (size_t t : grass)
			for (size_t o = t; r.u8(o) != END; o += GRASS)
			{
				if (!r.has(o, GRASS + 1)) return no("a grass table runs past the ROM");
				nds::Location& loc = zone(o);
				if (has(loc, EncounterKind::GRASS)) continue;
				for (int tm = 0; tm < 3; tm++)
					for (int j = 0; j < GRASS_SLOTS; j++)
					{
						const size_t s = o + 5 + (size_t)(tm * GRASS_SLOTS + j) * 2;
						add(loc, r.u8(s + 1), r.u8(s), r.u8(s), grassPct[j], EncounterKind::GRASS, TIMES[tm]);
					}
			}
		for (size_t t : water)
			for (size_t o = t; r.u8(o) != END; o += WATER)
			{
				if (!r.has(o, WATER + 1)) return no("a water table runs past the ROM");
				nds::Location& loc = zone(o);
				if (has(loc, EncounterKind::SURF)) continue;
				for (int j = 0; j < WATER_SLOTS; j++)
				{
					const size_t s = o + 3 + (size_t)j * 2;
					add(loc, r.u8(s + 1), r.u8(s), (uint8_t)std::min(100, r.u8(s) + SURF_LEVELS), waterPct[j], EncounterKind::SURF, TimeOfDay::ANY);
				}
			}
		// fishing, by the map's fishing group (engine/events/fish.asm: a rod's entries are a cumulative chance out of
		// 256 taken when the roll is at or below it, a species and level, to the one at 255; species 0 is a level of
		// TimeFishGroups: its day species by morning and day, its night species at night). The games give every map
		// with no water of its own the first group, Shore (data/maps/maps.asm: the Pokémon Centers, the towers, the
		// gates), so a map of that group fishes only when it has water to surf (its water table)
		static const EncounterKind RODS[3] = { EncounterKind::OLD_ROD, EncounterKind::GOOD_ROD, EncounterKind::SUPER_ROD };
		const uint8_t SHORE = 1;
		for (const auto& [z, fg] : fishGroupOf)
		{
			const size_t f = fish + FISH_GROUP * (size_t)(fg - 1);
			if (fg > 32 || !r.has(f, FISH_GROUP)) return no("map " + std::to_string(z) + "'s fishing group isn't in FishGroups");
			nds::Location& loc = out.zones[z];
			if (fg == SHORE && !has(loc, EncounterKind::SURF)) continue;
			for (int rod = 0; rod < 3; rod++)
			{
				size_t q = r.near(f + 1 + 2 * rod);
				int last = -1;
				for (int k = 0; k < 8 && q != RomView::NONE && r.has(q, 3); k++, q += 3)
				{
					const int cum = r.u8(q), pct = ((cum - last) * 100 + 128) / 256;
					last = cum;
					if (r.u8(q + 1) == 0)
					{
						const size_t tf = timeFish + TIME_FISH * (size_t)r.u8(q + 2);
						if (!r.has(tf, TIME_FISH)) return no("a fishing table's time group isn't in TimeFishGroups");
						add(loc, r.u8(tf), r.u8(tf + 1), r.u8(tf + 1), pct, RODS[rod], TimeOfDay::MORNING);
						add(loc, r.u8(tf), r.u8(tf + 1), r.u8(tf + 1), pct, RODS[rod], TimeOfDay::DAY);
						add(loc, r.u8(tf + 2), r.u8(tf + 3), r.u8(tf + 3), pct, RODS[rod], TimeOfDay::NIGHT);
					}
					else add(loc, r.u8(q + 1), r.u8(q + 2), r.u8(q + 2), pct, RODS[rod], TimeOfDay::ANY);
					if (cum == END) break;
				}
			}
		}
		// Headbutt and Rock Smash (data/wild/treemon_maps.asm: map and set to -1; data/wild/treemons.asm: a set's
		// percent, species and level to -1, the common trees' then the rare trees'; Rock Smash takes the first list)
		auto trees = [&](size_t maps, EncounterKind k, int lists) {
			for (size_t o = maps; r.u8(o) != END && o < maps + 3 * 128; o += 3)
			{
				nds::Location& loc = zone(o);
				const uint8_t set = r.u8(o + 2);
				if (!set || has(loc, k)) continue;
				size_t q = r.near(treeMons + 2 * (size_t)set);
				if (q == RomView::NONE) return false;
				for (int list = 0; list < lists; list++, q++)
					for (int n = 0; n < 16 && r.u8(q) != END; n++, q += 3)
						add(loc, r.u8(q + 1), r.u8(q + 2), r.u8(q + 2), r.u8(q), k, TimeOfDay::ANY, (uint8_t)list);
			}
			return true;
		};
		if (!trees(treeMaps, EncounterKind::HEADBUTT, 2) || !trees(rockMaps, EncounterKind::ROCK_SMASH, 1)) return no("TreeMons' pointer leaves its bank");
		// the Pokégear's maps, and each place's spot on them (maps.cpp)
		std::string w;
		if (!readPokegearMaps(r, d, landmarkOf, out, &w)) return no(w);

		// trainers (data/trainers/parties.asm: a group a class, TrainerGroups' pointer each; a group's trainers one after
		// another to the next group: name, party type, then level, species, item when the type has items, four moves
		// when it has moves, to -1), by class << 8 | number, the class's name from TrainerClassNames
		std::vector<std::string> classes;
		size_t cn = classNames;
		for (int c = 0; c < nClasses && cn != RomView::NONE; c++) { classes.push_back(nameCase(r.text(cn, 16))); cn = r.skip(cn, 16); }
		std::vector<size_t> starts;
		if (!r.has(trainers, 2 * (size_t)nClasses)) return no("TrainerGroups runs past the ROM");
		for (int c = 0; c < nClasses; c++) starts.push_back(r.near(trainers + 2 * c));
		for (int c = 0; c < nClasses; c++)
		{
			size_t o = starts[c];
			if (o == RomView::NONE) return no("TrainerGroups' pointer for class " + std::to_string(c + 1) + " leaves its bank");
			size_t end = (o / 0x4000 + 1) * 0x4000;
			for (size_t s : starts) if (s != RomView::NONE && s > o) end = std::min(end, s);
			for (int n = 1; n < 256 && o < end; n++)
			{
				nds::Trainer tr;
				tr.id = (uint16_t)((c + 1) << 8 | n);
				tr.cls = c < (int)classes.size() ? classes[c] : "";
				tr.name = nameCase(r.text(o, 16));
				size_t q = r.skip(o, 16);
				if (q == RomView::NONE || q >= end) break;
				const uint8_t type = r.u8(q++);
				if (type > (TRAINERTYPE_MOVES | TRAINERTYPE_ITEM)) break;
				bool ok = true;
				while (q < end && r.u8(q) != END && ok)
				{
					nds::TrainerMon m;
					m.level = r.u8(q);
					m.species = r.u8(q + 1);
					q += 2;
					if (type & TRAINERTYPE_ITEM) m.item = gen2Item(r.u8(q++));
					if (type & TRAINERTYPE_MOVES) { for (int j = 0; j < 4; j++) m.moves[j] = r.u8(q + j); q += 4; }
					else defaultMoves(out, m);
					ok = m.level >= 1 && m.level <= 100 && m.species >= 1 && m.species <= nSp && tr.team.size() < 6;
					tr.team.push_back(m);
				}
				if (!ok || q >= end || tr.team.empty()) break;	// (past the last group: not a trainer)
				o = q + 1;
				out.trainers[tr.id] = std::move(tr);
			}
		}
		return true;
	}
}
