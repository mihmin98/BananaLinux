// banana-ui (kiwami): Red's, Blue's and Yellow's tables from the player's ROM (rom.h). The layouts are pret's
// (pokered; pokeyellow the same where it isn't named), named at each table below; where each table is comes from the
// data file. Each table is checked as it is read: PokedexOrder gives each species one internal index, every base data
// record names its own species and the first is Bulbasaur's, every move record its own animation and the first is
// Pound's, the lists end where pret's do, a class's trainers end where the next class's begin, Bulbasaur's front
// picture decodes at its base data's size, every hidden item has its place in HiddenItemCoords, and a pointer that
// leaves its bank stops the read. The pictures and the town map are decoded by pics.cpp and maps.cpp.
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
#include <map>
#include <set>

namespace banana::kiwami::gbpk
{
	namespace
	{
		using nds::EncounterKind;

		// pret's layouts and the games' fixed numbers
		const int BASE_DATA = 28;			// data/pokemon/base_stats/*.asm: species, HP, Attack, Defense, Speed, Special,
											// types, catch rate, base exp, picture size, the pictures' pointers, the four
											// level 1 moves, growth, TM/HM bits, a pad
		const int BASE_PIC_SIZE = 10, BASE_FRONT_PIC = 11;	// (the front picture's first byte, its size; its pointer)
		const int PALETTE = 8;				// data/sgb/sgb_palettes.asm: four RGB555 colours
		const int MOVE = 6;					// data/moves/moves.asm: animation (the move's id), effect, power, type,
											// accuracy (n * 255 / 100), PP
		const int MON_NAME = 10;			// data/pokemon/names.asm
		const int PRICE = 3;				// data/items/prices.asm: bcd3
		const int EXTERNAL_MAP = 3, INTERNAL_MAP = 4;	// data/maps/town_map_entries.asm: (the indoor group's bound,)
											// dn y, x, the name's pointer
		const int WILD_SLOTS = 10;			// data/wild/maps/*.asm: grass's rate and, when it isn't 0, ten levels and
											// species; then water's the same
		const int NUM_TMS = 50, NUM_HMS = 5;	// constants/item_constants.asm (the TM/HM bits)
		const int SUPER_ROD = 3;			// data/wild/super_rod.asm SuperRodData: map, the group's pointer (a group: a
											// count, then its levels and species)
		const int HIDDEN_EVENT = 6;			// data/events/hidden_events.asm hidden_event: y, x, the function's argument,
											// its bank and address
		const int HIDDEN_COORDS = 3;		// data/events/hidden_item_coords.asm hidden_item: map, y, x
		const int ROD_SLOTS = 4;			// pokeyellow's data/wild/super_rod.asm: map, four species and levels
		const int YELLOW_ROD_PERCENT[ROD_SLOTS] = { 40, 30, 20, 10 };	// pokeyellow's engine/items/super_rod.asm
		const uint8_t OWN_LEVELS = 0xFF;	// data/trainers/parties.asm: a party with a level each
		const uint8_t TX_FAR = 0x17, DEXEND = 0x5F, TERMINATOR = 0x50;	// macros/scripts/text.asm, constants/charmap.asm
		const uint8_t END = 0xFF;
		enum { EVOLVE_LEVEL = 1, EVOLVE_ITEM, EVOLVE_TRADE };	// constants/pokemon_data_constants.asm
		// Red's and Blue's gym leaders by the gym's number, which each gym's script gives LoneMoves (wGymLeaderNo:
		// scripts/PewterGym.asm 1 .. scripts/ViridianGym.asm 8), and the leader's trainer there (data/maps/objects/
		// *Gym.asm: each leader's first, Giovanni's third)
		struct Gym { const char* cls; uint8_t number; };
		const Gym GYMS[8] = { { "leaderBrock", 1 }, { "leaderMisty", 1 }, { "leaderLtSurge", 1 }, { "leaderErika", 1 },
			{ "leaderKoga", 1 }, { "leaderSabrina", 1 }, { "leaderBlaine", 1 }, { "leaderGiovanni", 3 } };

		// a Pokédex entry's text: its text_far (TX_FAR, the address, the bank) to the text, which ends at <DEXEND> (the
		// entry's last full stop: home/text.asm PlaceDexEnd) or the terminator
		std::string dexText(const RomView& r, size_t far)
		{
			if (!r.has(far, 4) || r.u8(far) != TX_FAR) return "";
			const size_t t = r.at(r.u8(far + 3), r.le16(far + 1));
			if (t == RomView::NONE) return "";
			size_t n = 0;
			while (n < 400 && r.has(t, n + 1) && r.u8(t + n) != DEXEND && r.u8(t + n) != TERMINATOR) n++;
			std::string s = r.text(t, n);
			if (r.has(t, n + 1) && r.u8(t + n) == DEXEND) s += ".";
			const size_t a = s.find_first_not_of(' ');
			return a == std::string::npos ? "" : s.substr(a);
		}

		// two digits of binary-coded decimal; -1 when a nybble isn't one
		int bcd(uint8_t b) { return (b >> 4) > 9 || (b & 15) > 9 ? -1 : (b >> 4) * 10 + (b & 15); }
	}

	bool readGen1(const RomView& r, const GameData& d, GbRomData& out, std::string* why)
	{
		auto no = [&](const std::string& w) { if (why) *why = w; return false; };
		std::string missing;
		auto table = [&](const char* k) {
			size_t o = r.at(d.rom.count(k) ? d.rom.at(k) : Addr{});
			if (o == RomView::NONE && missing.empty()) missing = k;
			return o;
		};
		auto optional = [&](const char* k) { return d.rom.count(k) ? table(k) : RomView::NONE; };
		const size_t baseData = table("baseData"), dexOrder = table("dexOrder"), names = table("pokemonNames"),
			dexPtrs = table("dexEntryPointers"), evos = table("evosAttacksPointers"), moves = table("moves"),
			moveNames = table("moveNames"), itemNames = table("itemNames"), prices = table("itemPrices"), tmHm = table("tmHmMoves"),
			external = table("externalMapEntries"), internal = table("internalMapEntries"), wildData = table("wildData"),
			wildSlots = table("wildSlots"), oldRod = table("oldRod"), goodRod = table("goodRodMons"), trainers = table("trainerGroups"),
			trainersEnd = table("trainerGroupsEnd"), classNames = table("trainerClassNames"), paletteIds = table("monPaletteIds"),
			palettes = table("palettes");
		// the front pictures' banks ("Pics 1" .. "Pics 5", each its first picture's) and the town map's tables (maps.cpp
		// reads them; here only that the data file has them)
		for (const char* k : { "pics1", "pics2", "pics3", "pics4", "pics5", "townMap", "townMapTiles", "townMapOrder", "townMapOrderEnd" }) table(k);
		// Red's and Blue's: Mew's base data apart, the Super Rod's groups, the bosses' LoneMoves and TeamMoves; Yellow's:
		// the Super Rod's four slots and SpecialTrainerMoves
		const size_t mew = optional("mewBaseData"), mewPic = optional("mewPicFront"), superRod = optional("superRod"), rodSlots = optional("superRodSlots"),
			loneMoves = optional("loneMoves"), teamMoves = optional("teamMoves"), special = optional("specialTrainerMoves");
		// the hidden items: the maps with hidden events (Red's and Blue's events through a table of pointers of its own,
		// Yellow's each with its map), the function that gives an item, and the items' flags
		const size_t hiddenMaps = table("hiddenEventMaps"), hiddenCoords = table("hiddenItemCoords"),
			hiddenPointers = optional("hiddenEventPointers");
		table("hiddenItems");
		if (!missing.empty()) return no("no rom." + missing + " in the data file, or it is past the ROM's end");
		if ((superRod == RomView::NONE) == (rodSlots == RomView::NONE)) return no("the data file gives neither rom.superRod nor rom.superRodSlots, or both");
		if ((mew == RomView::NONE) != (mewPic == RomView::NONE)) return no("the data file gives Mew's base data apart without its picture's bank, or that without it");
		const int nSp = d.size("numPokemon"), nIdx = d.size("numPokemonIndexes"), nMoves = d.size("numMoves"),
			nItems = d.size("numItems"), nClasses = d.size("numTrainerClasses"), nMaps = d.size("numMaps"),
			firstIndoor = d.size("firstIndoorMap"), quickAttack = d.size("quickAttack"), counter = d.size("counter");
		// (UncompressMonSprite's bounds, by internal index: "Pics 1" below TANGELA + 1, .., "Pics 5" from STARMIE + 1)
		const int picsBound[4] = { d.size("picsTangela") + 1, d.size("picsMoltres") + 1, d.size("picsBeedrill") + 2, d.size("picsStarmie") + 1 };
		if (nSp < 1 || nSp > nIdx || nIdx > 254 || nMoves < 1 || nMoves > 254 || nItems < 1 || nItems > 254 || nClasses < 1 ||
			nClasses > 255 || nMaps < 1 || nMaps > 255 || firstIndoor < 1 || firstIndoor > nMaps)
			return no("the data file's counts (numPokemon, numPokemonIndexes, numMoves, numItems, numTrainerClasses, numMaps, firstIndoorMap) aren't Gen I's");
		if (picsBound[0] < 2 || picsBound[0] >= picsBound[1] || picsBound[1] >= picsBound[2] || picsBound[2] >= picsBound[3] || picsBound[3] > 256)
			return no("the data file's picsTangela, picsMoltres, picsBeedrill and picsStarmie aren't in order");
		// (the TM and HM items where items.inc has them)
		if (d.size("tm01") < 1 || gen1Item((uint8_t)d.size("tm01")) != 328 || d.size("hm01") < 1 || gen1Item((uint8_t)d.size("hm01")) != 420)
			return no("the data file's tm01 and hm01 aren't the items' table's");

		// the species' internal order (data/pokemon/dex_order.asm PokedexOrder: a national number an internal index,
		// 0 for none), each species once
		if (!r.has(dexOrder, (size_t)nIdx)) return no("PokedexOrder runs past the ROM");
		std::vector<int> internalOf((size_t)nSp + 1, 0);
		out.national.assign(256, 0);
		for (int i = 1; i <= nIdx; i++)
		{
			const int n = r.u8(dexOrder + i - 1);
			if (n > nSp || (n && internalOf[n])) return no("PokedexOrder gives species " + std::to_string(n) + " twice, or one past " + std::to_string(nSp));
			if (n) { internalOf[n] = i; out.national[i] = (uint16_t)n; }
		}
		for (int s = 1; s <= nSp; s++)
			if (!internalOf[s]) return no("PokedexOrder has no internal index for species " + std::to_string(s));
		auto national = [&](uint8_t i) -> uint16_t { return out.national[i]; };

		// the species' base data, checked (each record its species; Bulbasaur's first; Mew's apart in Red and Blue)
		static const uint8_t BULBASAUR[5] = { 45, 49, 49, 45, 65 };
		const int inTable = mew != RomView::NONE ? nSp - 1 : nSp;
		if (!r.has(baseData, (size_t)BASE_DATA * inTable) || (mew != RomView::NONE && !r.has(mew, BASE_DATA))) return no("BaseStats runs past the ROM");
		auto record = [&](int s) { return s > inTable ? mew : baseData + (size_t)BASE_DATA * (s - 1); };
		for (int s = 1; s <= nSp; s++)
			if (r.u8(record(s)) != s) return no("BaseStats' record " + std::to_string(s) + " isn't species " + std::to_string(s) + "'s");
		if (memcmp(r.b.data() + baseData + 1, BULBASAUR, 5) != 0) return no("BaseStats' first record isn't Bulbasaur's");
		// the front pictures (home/pics.asm UncompressMonSprite: the base data's pointer, in the bank its internal index
		// falls in; Red's and Blue's Mew, cp MEW, in its own) and their palettes (data/pokemon/palettes.asm
		// MonsterPalettes: an id by national number, from MissingNo.'s at 0), checked: Bulbasaur's decodes, at the
		// size its base data gives
		if (!r.has(paletteIds, (size_t)nSp + 1)) return no("MonsterPalettes runs past the ROM");
		out.pics1.pic.assign((size_t)nSp + 1, RomView::NONE);
		out.pics1.palette.assign((size_t)nSp + 1, RomView::NONE);
		static const char* const PICS[5] = { "pics1", "pics2", "pics3", "pics4", "pics5" };
		for (int s = 1; s <= nSp; s++)
		{
			int k = 0;
			while (k < 4 && internalOf[s] >= picsBound[k]) k++;
			const int bank = s > inTable ? d.rom.at("mewPicFront").bank : d.rom.at(PICS[k]).bank;
			const uint16_t a = r.le16(record(s) + BASE_FRONT_PIC);
			if (a >= 0x4000) out.pics1.pic[s] = r.at(bank, a);
			const size_t pal = palettes + (size_t)PALETTE * r.u8(paletteIds + s);
			if (r.has(pal, PALETTE)) out.pics1.palette[s] = pal;
		}
		if (!gen1FrontPic(r, out.pics1, 1) || r.u8(out.pics1.pic[1]) != r.u8(record(1) + BASE_PIC_SIZE))
			return no("Bulbasaur's front picture doesn't decode at its base data's size");
		if (!r.has(names, (size_t)MON_NAME * nIdx)) return no("MonsterNames runs past the ROM");
		// the moves, checked (each record's animation is its move; Pound first: Normal, 40, 35 PP)
		if (!r.has(moves, (size_t)MOVE * nMoves)) return no("Moves runs past the ROM");
		for (int m = 1; m <= nMoves; m++)
			if (r.u8(moves + MOVE * (m - 1)) != m) return no("Moves' record " + std::to_string(m) + " isn't move " + std::to_string(m) + "'s");
		if (r.u8(moves + 2) != 40 || r.u8(moves + 3) != 0 || r.u8(moves + 5) != 35) return no("Moves' first record isn't Pound's");

		// moves: names (a list of strings), the category by the type (Normal .. Ghost physical, Fire .. Dragon special; no
		// power: status), Quick Attack first and Counter last by their ids (engine/battle/core.asm MainInBattleLoop)
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
			v.cat = v.power == 0 ? nds::MoveCategory::STATUS : r.u8(e + 3) < 20 ? nds::MoveCategory::PHYSICAL : nds::MoveCategory::SPECIAL;
			v.priority = (uint8_t)(int8_t)(m == quickAttack ? 1 : m == counter ? -1 : 0);
		}

		// items: each its name (ItemNames, a list of strings; the elevators' floors after them aren't read) and price
		// (ItemPrices, bcd3), at its Gen IV id or GEN1_ONLY + its own (rom.cpp gen1Item); the TMs and HMs as Gen IV
		// numbers them (TM01 328, HM01 420)
		out.items.resize((size_t)GEN1_ONLY + 256);
		if (!r.has(prices, (size_t)PRICE * nItems)) return no("ItemPrices runs past the ROM");
		size_t in = itemNames;
		for (int i = 1; i <= nItems; i++)
		{
			if (in == RomView::NONE) return no("ItemNames ends before item " + std::to_string(i));
			nds::ItemData& it = out.items[gen1Item((uint8_t)i)];
			it.name = nameCase(r.text(in, 16));
			in = r.skip(in, 16);
			const size_t p = prices + PRICE * (size_t)(i - 1);
			const int a = bcd(r.u8(p)), b = bcd(r.u8(p + 1)), c = bcd(r.u8(p + 2));
			if (a < 0 || b < 0 || c < 0) return no("ItemPrices' price " + std::to_string(i) + " isn't decimal");
			it.price = (uint16_t)std::min(65535, a * 10000 + b * 100 + c);
			it.pocket = gen1Pocket((uint8_t)i);
		}
		for (int n = 1; n <= NUM_TMS + NUM_HMS; n++)
		{
			char nm[8];
			snprintf(nm, sizeof nm, n <= NUM_TMS ? "TM%02d" : "HM%02d", n <= NUM_TMS ? n : n - NUM_TMS);
			nds::ItemData& it = out.items[n <= NUM_TMS ? 327 + n : 419 + n - NUM_TMS];
			it.name = nm;
			it.pocket = nds::POCKET_TMS;
		}
		// TechnicalMachines: the move each TM and HM teaches, in their order
		out.tmMoves.assign(100, 0);
		if (!r.has(tmHm, NUM_TMS + NUM_HMS)) return no("TechnicalMachines runs past the ROM");
		for (int k = 0; k < NUM_TMS + NUM_HMS; k++)
			out.tmMoves[k < NUM_TMS ? k : 92 + k - NUM_TMS] = r.u8(tmHm + k);

		// species, by national number
		out.species.resize((size_t)nSp + 1);
		for (int s = 1; s <= nSp; s++)
		{
			nds::Species& sp = out.species[s];
			const size_t b = record(s);
			const int idx = internalOf[s];
			sp.name = nameCase(r.text(names + MON_NAME * (size_t)(idx - 1), MON_NAME));
			for (int i = 0; i < 4; i++) sp.base[i] = r.u8(b + 1 + i);
			sp.base[4] = sp.base[5] = r.u8(b + 5);		// (Special, both special stats)
			sp.types[0] = gen2Type(r.u8(b + 6));
			sp.types[1] = gen2Type(r.u8(b + 7));
			sp.catchRate = r.u8(b + 8);
			sp.baseExp = r.u8(b + 9);
			sp.genderRatio = 255;	// (no genders: the pages draw no sign)
			sp.growth = r.u8(b + 19);
			for (int k = 0; k < NUM_TMS + NUM_HMS; k++)
				if (r.u8(b + 20 + k / 8) >> (k % 8) & 1) sp.tms.push_back((uint16_t)(k < NUM_TMS ? k + 1 : 93 + k - NUM_TMS));
			// the moves it starts with, as learned at level 1
			for (int k = 0; k < 4; k++)
				if (r.u8(b + 15 + k) >= 1 && r.u8(b + 15 + k) <= nMoves) sp.learnset.push_back({ 1, r.u8(b + 15 + k) });
			// the Pokédex entry (data/pokemon/dex_entries.asm: a pointer an internal index, in the table's bank): category,
			// feet and inches, pounds * 10, the text's text_far
			const size_t e = r.near(dexPtrs + 2 * (size_t)(idx - 1));
			const size_t hw = r.skip(e, 16);
			if (hw != RomView::NONE && r.has(hw, 4))
			{
				sp.category = nameCase(r.text(e, 16));
				const int inches = r.u8(hw) * 12 + r.u8(hw + 1), lb10 = r.le16(hw + 2);
				sp.height = (uint16_t)((inches * 254 + 500) / 1000);		// in 0.1 m
				sp.weight = (uint16_t)((lb10 * 4536 + 5000) / 10000);		// in 0.1 kg
				sp.dexText = dexText(r, hw + 4);
			}
			// evolutions, then the moves learned by level (data/pokemon/evos_moves.asm: a pointer an internal index, in
			// the same bank)
			size_t p = r.near(evos + 2 * (size_t)(idx - 1));
			if (p == RomView::NONE) return no("EvosMovesPointerTable's pointer for species " + std::to_string(s) + " leaves its bank");
			for (int k = 0; k < 8 && r.u8(p); k++)
			{
				nds::Evolution ev;
				switch (r.u8(p))
				{
				case EVOLVE_LEVEL: ev.method = nds::evo::LEVEL; ev.param = r.u8(p + 1); ev.into = national(r.u8(p + 2)); p += 3; break;
				case EVOLVE_ITEM: ev.method = nds::evo::USE_ITEM; ev.param = gen1Item(r.u8(p + 1)); ev.into = national(r.u8(p + 3)); p += 4; break;
				case EVOLVE_TRADE: ev.method = nds::evo::TRADE; ev.into = national(r.u8(p + 2)); p += 3; break;
				default: return no("species " + std::to_string(s) + " has an evolution of a kind Gen I hasn't (" + std::to_string(r.u8(p)) + ")");
				}
				if (ev.into >= 1) sp.evolutions.push_back(ev);
			}
			p++;
			for (int k = 0; k < 40 && r.u8(p); k++, p += 2)
				if (r.u8(p + 1) >= 1 && r.u8(p + 1) <= nMoves) sp.learnset.push_back({ r.u8(p), r.u8(p + 1) });
		}
		for (auto& sp : out.species)
			for (auto& ev : sp.evolutions) ev.how = evoHow(out, ev.method, ev.param);

		// places: every map by its number, named as the town map names it (engine/items/town_map.asm LoadTownMapEntry:
		// below FIRST_INDOOR_MAP the map's own ExternalMapEntries entry; above, the first InternalMapEntries group whose
		// bound is past the map, to -1), and its point on the town map (the entry's dn y, x); the names in the order
		// the town map lists them
		if (!r.has(external, (size_t)EXTERNAL_MAP * firstIndoor)) return no("ExternalMapEntries runs past the ROM");
		std::map<uint16_t, uint8_t> pointOf;
		auto placeName = [&](int m, size_t entry) { pointOf[(uint16_t)m] = r.u8(entry); return nameCase(r.text(r.near(entry + 1), 24)); };
		std::vector<std::string> mapName((size_t)nMaps);
		for (int m = 0; m < firstIndoor; m++) mapName[m] = placeName(m, external + EXTERNAL_MAP * (size_t)m);
		std::vector<size_t> groups;
		for (size_t o = internal; r.has(o, 1) && r.u8(o) != END; o += INTERNAL_MAP)
		{
			if (!r.has(o, INTERNAL_MAP) || groups.size() > 255) return no("InternalMapEntries has no end");
			groups.push_back(o);
		}
		for (int m = firstIndoor; m < nMaps; m++)
			for (size_t g : groups)
				if (m < r.u8(g)) { mapName[m] = placeName(m, g + 1); break; }
		for (int m = 0; m < nMaps; m++)
		{
			if (mapName[m].empty()) continue;
			if (std::find(out.locationNames.begin(), out.locationNames.end(), mapName[m]) == out.locationNames.end())
				out.locationNames.push_back(mapName[m]);
		}
		auto add = [&](nds::Location& l, uint8_t species, uint8_t level, int pct, EncounterKind k) {
			const uint16_t n = national(species);
			if (!n || !level) return;
			nds::Encounter e;
			e.species = n;
			e.minLevel = e.maxLevel = level;
			e.percent = (uint8_t)std::clamp(pct, 0, 100);
			e.kind = k;
			e.time = nds::TimeOfDay::ANY;
			l.encounters.push_back(e);
		};

		// the slots' chances (data/wild/probabilities.asm WildMonEncounterSlotChances: the cumulative chance out of 256,
		// less 1, and slot * 2), then each map's grass and water (data/wild/grass_water.asm WildDataPointers: a pointer
		// a map, to -1)
		int slotPct[WILD_SLOTS];
		{
			int last = 0;
			for (int j = 0; j < WILD_SLOTS; j++)
			{
				const size_t o = wildSlots + 2 * (size_t)j;
				if (!r.has(o, 2) || r.u8(o + 1) != 2 * j || r.u8(o) + 1 <= last) return no("WildMonEncounterSlotChances isn't the games' ten slots");
				slotPct[j] = ((r.u8(o) + 1 - last) * 100 + 128) / 256;
				last = r.u8(o) + 1;
			}
			if (last != 256) return no("WildMonEncounterSlotChances doesn't add up to 256");
		}
		if (!r.has(wildData, 2 * ((size_t)nMaps + 1)) || r.le16(wildData + 2 * (size_t)nMaps) != 0xFFFF) return no("WildDataPointers doesn't end after the maps");
		std::set<int> water;
		for (int m = 0; m < nMaps; m++)
		{
			nds::Location& loc = out.zones[(uint16_t)m];
			loc.id = (uint16_t)m;
			loc.name = mapName[m];
			size_t w = r.near(wildData + 2 * (size_t)m);
			if (w == RomView::NONE) return no("WildDataPointers' pointer for map " + std::to_string(m) + " leaves its bank");
			for (EncounterKind k : { EncounterKind::GRASS, EncounterKind::SURF })
			{
				if (!r.has(w, 1 + 2 * WILD_SLOTS)) return no("map " + std::to_string(m) + "'s wild Pokémon run past the ROM");
				if (!r.u8(w++)) continue;
				for (int j = 0; j < WILD_SLOTS; j++, w += 2) add(loc, r.u8(w + 1), r.u8(w), slotPct[j], k);
				if (k == EncounterKind::SURF) water.insert(m);
			}
		}

		// fishing (engine/items/item_effects.asm): the Old Rod's one Pokémon in its code (ItemUseOldRod: call
		// FishingInit, jp c, then lb bc, level, species), the Good Rod's two at even chances (GoodRodMons: level,
		// species), the Super Rod's by the map: Red's and Blue's groups at even chances (ReadSuperRodData), Yellow's four
		// slots. The Old and Good Rods fish any water: they are listed where the Super Rod is, or the map has water.
		std::map<int, std::vector<std::pair<uint8_t, uint8_t>>> superMons;	// map: (species, level) a slot
		std::map<int, std::vector<int>> superPct;
		if (superRod != RomView::NONE)
			for (size_t o = superRod; r.u8(o) != END; o += SUPER_ROD)
			{
				const size_t g = r.near(o + 1);
				if (!r.has(o, SUPER_ROD + 1) || o >= superRod + SUPER_ROD * 64 || g == RomView::NONE || !r.has(g, 1 + 2 * (size_t)r.u8(g)) ||
					r.u8(g) < 1 || r.u8(g) > 4)
					return no("SuperRodData isn't its maps and groups");
				const int n = r.u8(g);
				for (int j = 0; j < n; j++) { superMons[r.u8(o)].push_back({ r.u8(g + 2 + 2 * j), r.u8(g + 1 + 2 * j) }); superPct[r.u8(o)].push_back((100 + n / 2) / n); }
			}
		else
			for (size_t o = rodSlots; r.u8(o) != END; o += 1 + 2 * ROD_SLOTS)
			{
				if (!r.has(o, 2 + 2 * ROD_SLOTS) || o >= rodSlots + (1 + 2 * ROD_SLOTS) * 64) return no("SuperRodFishingSlots has no end");
				for (int j = 0; j < ROD_SLOTS; j++) { superMons[r.u8(o)].push_back({ r.u8(o + 1 + 2 * j), r.u8(o + 2 + 2 * j) }); superPct[r.u8(o)].push_back(YELLOW_ROD_PERCENT[j]); }
			}
		const bool oldRodOk = r.has(oldRod, 9) && r.u8(oldRod) == 0xCD && r.u8(oldRod + 3) == 0xDA && r.u8(oldRod + 6) == 0x01;
		if (!r.has(goodRod, 4)) return no("GoodRodMons runs past the ROM");
		for (const auto& [m, mons] : superMons) if (m < nMaps) water.insert(m);
		for (int m : water)
		{
			nds::Location& loc = out.zones[(uint16_t)m];
			if (oldRodOk) add(loc, r.u8(oldRod + 7), r.u8(oldRod + 8), 100, EncounterKind::OLD_ROD);
			for (int j = 0; j < 2; j++) add(loc, r.u8(goodRod + 2 * j + 1), r.u8(goodRod + 2 * j), 50, EncounterKind::GOOD_ROD);
			auto it = superMons.find(m);
			if (it != superMons.end())
				for (size_t j = 0; j < it->second.size(); j++) add(loc, it->second[j].first, it->second[j].second, superPct[m][j], EncounterKind::SUPER_ROD);
		}

		// the town map, and each place's spot on it (maps.cpp)
		std::string w;
		if (!readTownMap(r, d, pointOf, out, &w)) return no(w);

		// the hidden items (data/events/hidden_events.asm): HiddenEventMaps, the maps with hidden events to -1 (Red's and
		// Blue's a map id each, its events at HiddenEventPointers' pointer in the same place; Yellow's a map id and its
		// pointer), each map's events to -1; those that call HiddenItems are hidden items, the function's argument the
		// item, the flag the place of the map, y and x in HiddenItemCoords (to -1), as the game finds it
		// (engine/events/hidden_items.asm FindHiddenItemOrCoinsIndex)
		std::map<uint32_t, uint16_t> flagOf;
		for (size_t o = hiddenCoords, i = 0;; o += HIDDEN_COORDS, i++)
		{
			if (!r.has(o, 1) || i > 255) return no("HiddenItemCoords has no end");
			if (r.u8(o) == END) break;
			if (!r.has(o, HIDDEN_COORDS)) return no("HiddenItemCoords has no end");
			flagOf.emplace((uint32_t)(r.u8(o) << 16 | r.u8(o + 1) << 8 | r.u8(o + 2)), (uint16_t)i);
		}
		const Addr giveItem = d.rom.at("hiddenItems");
		const size_t mapEntry = hiddenPointers != RomView::NONE ? 1 : 3;
		for (size_t o = hiddenMaps, i = 0;; o += mapEntry, i++)
		{
			if (!r.has(o, 1) || i > 255) return no("HiddenEventMaps has no end");
			const uint8_t m = r.u8(o);
			if (m == END) break;
			size_t e = hiddenPointers != RomView::NONE ? r.near(hiddenPointers + 2 * i) : r.near(o + 1);
			if (m >= nMaps || e == RomView::NONE) return no("HiddenEventMaps' map " + std::to_string(m) + " has no events in its bank");
			for (int n = 0;; n++, e += HIDDEN_EVENT)
			{
				if (!r.has(e, 1) || n > 64) return no("map " + std::to_string(m) + "'s hidden events have no end");
				if (r.u8(e) == END) break;
				if (!r.has(e, HIDDEN_EVENT)) return no("map " + std::to_string(m) + "'s hidden events have no end");
				if (r.u8(e + 3) != giveItem.bank || r.le16(e + 4) != giveItem.addr) continue;
				auto f = flagOf.find((uint32_t)(m << 16 | r.u8(e) << 8 | r.u8(e + 1)));
				if (f == flagOf.end())
					return no("map " + std::to_string(m) + "'s hidden item at " + std::to_string(r.u8(e + 1)) + ", " + std::to_string(r.u8(e)) +
						" isn't in HiddenItemCoords");
				out.hidden.push_back({ m, r.u8(e + 1), r.u8(e), gen1Item(r.u8(e + 2)), f->second });
			}
		}

		// trainers (data/trainers/parties.asm: TrainerDataPointers' pointer a class; a class's trainers one after another
		// up to the next class's, the last class's up to TrainerAI: a level and its species to 0, or OWN_LEVELS and a
		// level and species each to 0), by class << 8 | number, named by the class (TrainerNames): Gen I's trainers
		// have no names of their own
		std::vector<std::string> classes;
		size_t cn = classNames;
		for (int c = 0; c < nClasses && cn != RomView::NONE; c++) { classes.push_back(nameCase(r.text(cn, 16))); cn = r.skip(cn, 16); }
		if (!r.has(trainers, 2 * (size_t)nClasses)) return no("TrainerDataPointers runs past the ROM");
		std::vector<size_t> starts;
		for (int c = 0; c < nClasses; c++) starts.push_back(r.near(trainers + 2 * c));
		std::set<uint16_t> ownLevels;
		for (int c = 0; c < nClasses; c++)
		{
			size_t o = starts[c];
			if (o == RomView::NONE || o / 0x4000 != trainersEnd / 0x4000 || o > trainersEnd)
				return no("TrainerDataPointers' pointer for class " + std::to_string(c + 1) + " isn't before TrainerAI in its bank");
			size_t end = trainersEnd;
			for (int k = c + 1; k < nClasses; k++) if (starts[k] >= o) end = std::min(end, starts[k]);
			for (int n = 1; o < end; n++)
			{
				if (n > 255) return no("class " + std::to_string(c + 1) + " has no end");
				nds::Trainer tr;
				tr.id = (uint16_t)((c + 1) << 8 | n);
				tr.cls = tr.name = c < (int)classes.size() ? classes[c] : "";
				const bool own = r.u8(o) == OWN_LEVELS;
				const uint8_t level = r.u8(o);
				size_t q = o + 1;
				while (q < end && r.u8(q))
				{
					nds::TrainerMon m;
					m.level = own ? r.u8(q++) : level;
					m.species = national(r.u8(q++));
					if (m.level < 1 || m.level > 100 || !m.species || tr.team.size() == 6 || q > end)
						return no("trainer " + std::to_string(n) + " of class " + std::to_string(c + 1) + " isn't a party");
					defaultMoves(out, m);
					tr.team.push_back(m);
				}
				if (q >= end || tr.team.empty()) return no("trainer " + std::to_string(n) + " of class " + std::to_string(c + 1) + " has no end");
				o = q + 1;
				if (own) ownLevels.insert(tr.id);
				out.trainers[tr.id] = std::move(tr);
			}
		}
		// the moves some trainers are given over their level's (engine/battle/read_trainer_party.asm ReadTrainer). Red's
		// and Blue's, to a party with a level each only: a gym leader's LoneMoves entry by the gym's number (the party's
		// member from 0, its third move), else the class's TeamMoves (its fifth member's third move; the Champion's, given
		// by his starter in the code, aren't read). Yellow's SpecialTrainerMoves, to any party: a class and trainer, then a
		// member and a move's place from 1 and the move each, to 0; the list to -1
		auto give = [&](uint16_t id, size_t member, size_t slot, uint8_t move, bool ownOnly) {
			auto t = out.trainers.find(id);
			if (t == out.trainers.end() || (ownOnly && !ownLevels.count(id)) || member >= t->second.team.size() || slot >= 4 || move < 1 ||
				move > nMoves)
				return false;
			t->second.team[member].moves[slot] = move;
			return true;
		};
		std::set<int> lone;
		if (loneMoves != RomView::NONE)
		{
			if (!r.has(loneMoves, 2 * 8)) return no("LoneMoves runs past the ROM");
			for (int g = 0; g < 8; g++)
			{
				const int cls = d.size(GYMS[g].cls);
				if (cls < 1 || cls > nClasses || !give((uint16_t)(cls << 8 | GYMS[g].number), r.u8(loneMoves + 2 * g), 2, r.u8(loneMoves + 2 * g + 1), true))
					return no(std::string("LoneMoves' gym ") + std::to_string(g + 1) + " isn't a party of its leader's (" + GYMS[g].cls + ")");
				lone.insert(cls << 8 | GYMS[g].number);
			}
		}
		if (teamMoves != RomView::NONE)
			for (size_t o = teamMoves; r.u8(o) != END; o += 2)
			{
				if (!r.has(o, 3) || o >= teamMoves + 2 * 64) return no("TeamMoves has no end");
				for (int n = 1; n < 256; n++)
					if (!lone.count(r.u8(o) << 8 | n) && out.trainers.count((uint16_t)(r.u8(o) << 8 | n))) give((uint16_t)(r.u8(o) << 8 | n), 4, 2, r.u8(o + 1), true);
			}
		if (special != RomView::NONE)
			for (size_t o = special; r.u8(o) != END;)
			{
				if (!r.has(o, 3) || o >= special + 1024) return no("SpecialTrainerMoves has no end");
				const uint16_t id = (uint16_t)(r.u8(o) << 8 | r.u8(o + 1));
				for (o += 2; r.u8(o); o += 3)
					if (!r.has(o, 4) || !give(id, (size_t)r.u8(o) - 1, (size_t)r.u8(o + 1) - 1, r.u8(o + 2), false))
						return no("SpecialTrainerMoves' entry for trainer " + std::to_string(id >> 8) + ":" + std::to_string(id & 0xFF) + " isn't its party's");
				o++;
			}
		return true;
	}
}
