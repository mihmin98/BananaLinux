// banana-ui (kiwami): Red's, Blue's and Yellow's tables from the player's ROM (rom.h), and the ROM hacks' that keep
// their engine (Yellow Legacy, PureRGB). The layouts are pret's (pokered; pokeyellow the same where it isn't named),
// and the hacks' own (their source at the release's tag) where the data file has their keys, named at each table
// below; where each table is comes from the data file. Each table is checked as it is read: PokedexOrder gives each
// species one internal index (a form its base's number), every base data record names its own species and the first
// is Bulbasaur's, every move record its own animation and the first is Pound's, the lists end where pret's do, a
// class's trainers end where the next class's begin, Bulbasaur's front picture decodes at its base data's size, every
// hidden item has its place in HiddenItemCoords, and a pointer that leaves its bank stops the read. The pictures and
// the town map are decoded by pics.cpp and maps.cpp.
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
		const int DEX_ENTRY = 8;			// PureRGB's data/pokemon/dex_entries.asm dex_entry: feet, inches, pounds * 10,
											// the text's text_far_end (its command, address and bank)
		const uint8_t OWN_LEVELS_ALT = 0xFE, OWN_MOVES = 0xFD;	// PureRGB's data/trainers/parties.asm: a level each, bit 7
											// an alternate palette; and a moveset (MoveSetMappings') then the same
		const int ORIGINAL_TYPING = 3, REMAP_BITS = 24;	// PureRGB's engine/pokemon/remap_typings.asm OriginalTypings:
														// species, two types; ram/wram.asm wPkmnTypeRemapFlags: 3 bytes
		const int REMAPPABLE_MOVE = 4;		// PureRGB's engine/battle/remap_move_data.asm RemappableMoves: move, species,
		const int ANY_SPECIES = 0xFF, SAME_POWER = 0xFF, MODIFIER_FUNCTION = 0xFE;	// power, accuracy (-1, -1, -2)
		const int LONE_SLOT = 3;			// PureRGB's data/trainers/special_moves.asm LoneMoves: member, move's place, move
		const uint8_t END = 0xFF;
		enum { EVOLVE_LEVEL = 1, EVOLVE_ITEM, EVOLVE_TRADE };	// constants/pokemon_data_constants.asm
		// Red's and Blue's gym leaders by the gym's number, which each gym's script gives LoneMoves (wGymLeaderNo:
		// scripts/PewterGym.asm 1 .. scripts/ViridianGym.asm 8), and the leader's trainer there (data/maps/objects/
		// *Gym.asm: each leader's first, Giovanni's third)
		struct Gym { const char* cls; uint8_t number; };
		const Gym GYMS[8] = { { "leaderBrock", 1 }, { "leaderMisty", 1 }, { "leaderLtSurge", 1 }, { "leaderErika", 1 },
			{ "leaderKoga", 1 }, { "leaderSabrina", 1 }, { "leaderBlaine", 1 }, { "leaderGiovanni", 3 } };

		// a Pokédex entry's text: its text_far (cmd: TX_FAR, PureRGB's text_far_end; the address, the bank) to the text,
		// which ends at <DEXEND> (the entry's last full stop: home/text.asm PlaceDexEnd) or the terminator
		std::string dexText(const RomView& r, size_t far, uint8_t cmd)
		{
			if (!r.has(far, 4) || r.u8(far) != cmd) return "";
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
		// (one of two layouts: retail's key, or a ROM hack's, which must have the table)
		auto either = [&](const char* k, const char* hack) { return d.rom.count(hack) ? table(hack) : table(k); };
		const size_t baseData = table("baseData"), dexOrder = table("dexOrder"), names = table("pokemonNames"),
			dexPtrs = either("dexEntryPointers", "dexEntryData"), evos = table("evosAttacksPointers"), moves = table("moves"),
			moveNames = either("moveNames", "moveNameTable"), itemNames = either("itemNames", "itemNameTable"), prices = table("itemPrices"), tmHm = table("tmHmMoves"),
			external = table("externalMapEntries"), internal = either("internalMapEntries", "internalMapEntriesWild"), wildData = table("wildData"),
			wildSlots = table("wildSlots"), oldRod = optional("oldRod"), oldRodMons = optional("oldRodMons"), goodRod = either("goodRodMons", "goodRodList"), trainers = table("trainerGroups"),
			trainersEnd = table("trainerGroupsEnd"), classNames = table("trainerClassNames"), paletteIds = table("monPaletteIds"),
			palettes = table("palettes"), typeChart = table("typeChart");
		// PureRGB's (its own layouts, each named where it is read): the Pokédex pages by number with their categories
		// apart, the names through pointer tables, the forms' base data, the Good Rod's lists, its priority moves, and
		// its trainers' movesets and boss moves
		const bool dexRecords = d.rom.count("dexEntryData"), moveNamePtrs = d.rom.count("moveNameTable"),
			itemNamePtrs = d.rom.count("itemNameTable"), goodRodList = d.rom.count("goodRodList");
		const size_t dexCatMap = dexRecords ? table("dexCategoryMap") : RomView::NONE, dexCats = dexRecords ? table("dexCategories") : RomView::NONE,
			nonDexData = optional("nonDexBaseData"), nonDexSpecies = optional("nonDexSpecies"), goodRodOcean = optional("goodRodOcean"),
			oceanMaps = optional("oceanMaps"), priorityMoves = optional("priorityMoves"), moveSets = optional("trainerMoveSets"),
			loneSlots = optional("loneMoveSlots"), teamSixth = optional("teamMoveSixth");
		// the base data's size and where its front picture's bank is (PureRGB's own byte; else by the internal index's
		// range, below)
		const int baseSize = d.size("baseDataSize", BASE_DATA), picBankAt = d.size("basePicBank", -1);
		const uint8_t textFar = (uint8_t)d.size("textFar", TX_FAR);
		// the front pictures' banks ("Pics 1" .. "Pics 5", each its first picture's) unless the base data has each one's,
		// and the town map's tables (maps.cpp reads them; here only that the data file has them)
		if (picBankAt < 0) for (const char* k : { "pics1", "pics2", "pics3", "pics4", "pics5" }) table(k);
		for (const char* k : { "townMap", "townMapTiles", "townMapOrder", "townMapOrderEnd" }) table(k);
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
		if ((oldRod == RomView::NONE) == (oldRodMons == RomView::NONE)) return no("the data file gives neither rom.oldRod nor rom.oldRodMons, or both");
		if ((mew == RomView::NONE) != (mewPic == RomView::NONE)) return no("the data file gives Mew's base data apart without its picture's bank, or that without it");
		if ((nonDexData == RomView::NONE) != (nonDexSpecies == RomView::NONE)) return no("the data file gives rom.nonDexBaseData without rom.nonDexSpecies, or that without it");
		if ((goodRodOcean == RomView::NONE) != (oceanMaps == RomView::NONE) || (goodRodOcean != RomView::NONE && !goodRodList))
			return no("the data file gives rom.goodRodOcean without rom.oceanMaps or rom.goodRodList, or those without it");
		if ((loneSlots == RomView::NONE) != (teamSixth == RomView::NONE) || (loneSlots != RomView::NONE && (loneMoves != RomView::NONE || teamMoves != RomView::NONE)))
			return no("the data file gives rom.loneMoveSlots without rom.teamMoveSixth, or with rom.loneMoves or rom.teamMoves");
		if (baseSize < BASE_DATA || baseSize > 64 || (picBankAt >= 0 && (picBankAt < BASE_DATA - 1 || picBankAt >= baseSize)))
			return no("the data file's baseDataSize and basePicBank aren't a base data record's");
		const int nSp = d.size("numPokemon"), nIdx = d.size("numPokemonIndexes"), nMoves = d.size("numMoves"),
			nItems = d.size("numItems"), nClasses = d.size("numTrainerClasses"), nMaps = d.size("numMaps"),
			firstIndoor = d.size("firstIndoorMap"), quickAttack = d.size("quickAttack"), counter = d.size("counter");
		// (UncompressMonSprite's bounds, by internal index: "Pics 1" below TANGELA + 1, .., "Pics 5" from STARMIE + 1)
		const int picsBound[4] = { d.size("picsTangela") + 1, d.size("picsMoltres") + 1, d.size("picsBeedrill") + 2, d.size("picsStarmie") + 1 };
		if (nSp < 1 || nSp > nIdx || nIdx > 254 || nMoves < 1 || nMoves > 254 || nItems < 1 || nItems > 254 || nClasses < 1 ||
			nClasses > 255 || nMaps < 1 || nMaps > 255 || firstIndoor < 1 || firstIndoor > nMaps)
			return no("the data file's counts (numPokemon, numPokemonIndexes, numMoves, numItems, numTrainerClasses, numMaps, firstIndoorMap) aren't Gen I's");
		if (picBankAt < 0 && (picsBound[0] < 2 || picsBound[0] >= picsBound[1] || picsBound[1] >= picsBound[2] || picsBound[2] >= picsBound[3] || picsBound[3] > 256))
			return no("the data file's picsTangela, picsMoltres, picsBeedrill and picsStarmie aren't in order");
		// (the TM and HM items, pret's constants/item_constants.asm: the five HMs after the items, the fifty TMs right
		// after them, up to $FF at most; and none of the data file's items in their places)
		const int hm01 = d.size("hm01"), tm01 = d.size("tm01");
		if (hm01 <= nItems || tm01 != hm01 + NUM_HMS || tm01 + NUM_TMS > 256 || gen1Item(d, (uint8_t)tm01) != 328 ||
			gen1Item(d, (uint8_t)hm01) != 420)
			return no("the data file's tm01 and hm01 aren't Gen I's machines' places");

		// PureRGB's forms: NonDexPokemonSpecies, the internal indexes whose base data is NonDexMonsBaseStats' (in its
		// order) rather than BaseStats' by their number, to -1 (engine/pokemon/get_mon_header.asm GetMonHeader2);
		// MissingNo. among them has no number
		std::map<int, size_t> nonDex;		// internal index: its base data
		if (nonDexSpecies != RomView::NONE)
			for (size_t k = 0;; k++)
			{
				if (!r.has(nonDexSpecies + k, 1) || k > 64) return no("NonDexPokemonSpecies has no end");
				const uint8_t i = r.u8(nonDexSpecies + k);
				if (i == END) break;
				if (!i || (int)i > nIdx || nonDex.count(i)) return no("NonDexPokemonSpecies' entry " + std::to_string(k + 1) + " isn't an internal index");
				nonDex[i] = nonDexData + (size_t)baseSize * k;
				if (!r.has(nonDex[i], (size_t)baseSize)) return no("NonDexMonsBaseStats runs past the ROM");
			}
		// the species' internal order (data/pokemon/dex_order.asm PokedexOrder: a national number an internal index,
		// 0 for none), each species once; a form (above) has its base species' number too
		if (!r.has(dexOrder, (size_t)nIdx)) return no("PokedexOrder runs past the ROM");
		std::vector<int> internalOf((size_t)nSp + 1, 0);
		std::vector<std::pair<int, int>> forms;		// (internal index, number), in the indexes' order
		out.national.assign(256, 0);
		out.form.assign(256, 0);
		for (int i = 1; i <= nIdx; i++)
		{
			const int n = r.u8(dexOrder + i - 1);
			if (n > nSp) return no("PokedexOrder gives species " + std::to_string(n) + ", past " + std::to_string(nSp));
			if (n && nonDex.count(i)) forms.push_back({ i, n });
			else if (n && internalOf[n]) return no("PokedexOrder gives species " + std::to_string(n) + " twice");
			else if (n) internalOf[n] = i;
			if (n) out.national[i] = (uint16_t)n;
		}
		for (int s = 1; s <= nSp; s++)
			if (!internalOf[s]) return no("PokedexOrder has no internal index for species " + std::to_string(s));
		// (each form's record names its base species, as the game's dex number byte)
		for (const auto& [i, n] : forms)
			if (r.u8(nonDex[i]) != n) return no("NonDexMonsBaseStats' record for index " + std::to_string(i) + " isn't species " + std::to_string(n) + "'s");
		auto national = [&](uint8_t i) -> uint16_t { return out.national[i]; };

		// the species' base data, checked (each record its species; Bulbasaur's first; Mew's apart in Red and Blue)
		static const uint8_t BULBASAUR[5] = { 45, 49, 49, 45, 65 };
		const int inTable = mew != RomView::NONE ? nSp - 1 : nSp;
		if (!r.has(baseData, (size_t)baseSize * inTable) || (mew != RomView::NONE && !r.has(mew, (size_t)baseSize))) return no("BaseStats runs past the ROM");
		// (the forms after the Pokédex in species, each base's together from its formsAt, as upstream's forms are laid out)
		std::stable_sort(forms.begin(), forms.end(), [](const auto& a, const auto& b) { return a.second < b.second; });
		const int nAll = nSp + (int)forms.size();
		std::vector<int> internalAt((size_t)nAll + 1, 0);	// a species index's internal index
		for (int s = 1; s <= nSp; s++) internalAt[s] = internalOf[s];
		for (size_t j = 0; j < forms.size(); j++) internalAt[nSp + 1 + j] = forms[j].first;
		auto record = [&](int s) { return s > nSp ? nonDex[internalAt[s]] : s > inTable ? mew : baseData + (size_t)baseSize * (s - 1); };
		auto numberOf = [&](int s) { return s > nSp ? (int)national((uint8_t)internalAt[s]) : s; };
		for (int s = 1; s <= nSp; s++)
			if (r.u8(record(s)) != s) return no("BaseStats' record " + std::to_string(s) + " isn't species " + std::to_string(s) + "'s");
		if (memcmp(r.b.data() + baseData + 1, BULBASAUR, 5) != 0) return no("BaseStats' first record isn't Bulbasaur's");
		// the front pictures (home/pics.asm UncompressMonSprite: the base data's pointer, in the bank its internal index
		// falls in; Red's and Blue's Mew, cp MEW, in its own; PureRGB's in the bank its base data names, BASE_PICBANK) and
		// their palettes (data/pokemon/palettes.asm MonsterPalettes: an id by national number, from MissingNo.'s at 0; a
		// form its base's), checked: Bulbasaur's decodes, at the size its base data gives
		if (!r.has(paletteIds, (size_t)nSp + 1)) return no("MonsterPalettes runs past the ROM");
		out.pics1.pic.assign((size_t)nAll + 1, RomView::NONE);
		out.pics1.palette.assign((size_t)nAll + 1, RomView::NONE);
		static const char* const PICS[5] = { "pics1", "pics2", "pics3", "pics4", "pics5" };
		for (int s = 1; s <= nAll; s++)
		{
			int k = 0;
			while (picBankAt < 0 && k < 4 && internalAt[s] >= picsBound[k]) k++;
			const int bank = picBankAt >= 0 ? r.u8(record(s) + picBankAt) : s > inTable ? d.rom.at("mewPicFront").bank : d.rom.at(PICS[k]).bank;
			const uint16_t a = r.le16(record(s) + BASE_FRONT_PIC);
			if (a >= 0x4000) out.pics1.pic[s] = r.at(bank, a);
			const size_t pal = palettes + (size_t)PALETTE * r.u8(paletteIds + numberOf(s));
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

		// the moves PureRGB gives priority, each +1, to -1 (data/battle/priority_moves.asm PriorityMoves, engine/battle/
		// core.asm CheckPriority); none goes last
		std::set<int> first;
		if (priorityMoves != RomView::NONE)
			for (size_t o = priorityMoves;; o++)
			{
				if (!r.has(o, 1) || o >= priorityMoves + 64) return no("PriorityMoves has no end");
				if (r.u8(o) == END) break;
				if (r.u8(o) < 1 || r.u8(o) > nMoves) return no("PriorityMoves' entry " + std::to_string(o - priorityMoves + 1) + " isn't a move");
				first.insert(r.u8(o));
			}
		else if (quickAttack) first.insert(quickAttack);
		// a name of a list of strings (pret's MoveNames and ItemNames), or by a pointer each in the table's bank
		// (PureRGB's MoveNameJumpTable and ItemNameJumpTable)
		auto nameAt = [&](size_t list, bool byPointer, size_t& at, int n) -> size_t {
			if (!byPointer) { const size_t o = at; at = r.skip(at, 16); return o; }
			return r.near(list + 2 * (size_t)(n - 1));
		};
		// moves: names, the category by the type (Normal .. Ghost physical, Fire .. Dragon special; no power: status),
		// Quick Attack first and Counter last by their ids (engine/battle/core.asm MainInBattleLoop), or PureRGB's list
		out.moves.resize((size_t)nMoves + 1);
		size_t mn = moveNames;
		for (int m = 1; m <= nMoves; m++)
		{
			const size_t e = moves + MOVE * (m - 1);
			nds::MoveData& v = out.moves[m];
			const size_t nm = nameAt(moveNames, moveNamePtrs, mn, m);
			if (nm == RomView::NONE) return no(std::string(moveNamePtrs ? "MoveNameJumpTable's pointer for" : "MoveNames ends before") + " move " + std::to_string(m) + (moveNamePtrs ? " leaves its bank" : ""));
			v.name = nameCase(r.text(nm, 16));
			v.effect = r.u8(e + 1);
			v.power = r.u8(e + 2);
			v.type = gen2Type(d, r.u8(e + 3));
			v.accuracy = percent(r.u8(e + 4));
			v.pp = r.u8(e + 5);
			v.cat = v.power == 0 ? nds::MoveCategory::STATUS : category(d, r.u8(e + 3));
			v.priority = (uint8_t)(int8_t)(first.count(m) ? 1 : m == counter ? -1 : 0);
		}
		// the type chart (data/types/type_matchups.asm TypeEffects, rom.cpp readTypeChart)
		if (!readTypeChart(r, d, typeChart, "TypeEffects", out.rules.chart, why)) return false;

		// items: each its name (ItemNames, a list of strings; the elevators' floors after them aren't read) and price
		// (ItemPrices, bcd3), at its Gen IV id or GEN1_ONLY + its own (rom.cpp gen1Item); the TMs and HMs as Gen IV
		// numbers them (TM01 328, HM01 420)
		out.items.resize((size_t)GEN1_ONLY + 256);
		if (!r.has(prices, (size_t)PRICE * nItems)) return no("ItemPrices runs past the ROM");
		size_t in = itemNames;
		for (int i = 1; i <= nItems; i++)
		{
			const size_t nm = nameAt(itemNames, itemNamePtrs, in, i);
			if (nm == RomView::NONE) return no(std::string(itemNamePtrs ? "ItemNameJumpTable's pointer for" : "ItemNames ends before") + " item " + std::to_string(i) + (itemNamePtrs ? " leaves its bank" : ""));
			nds::ItemData& it = out.items[gen1Item(d, (uint8_t)i)];
			it.name = nameCase(r.text(nm, 16));
			const size_t p = prices + PRICE * (size_t)(i - 1);
			const int a = bcd(r.u8(p)), b = bcd(r.u8(p + 1)), c = bcd(r.u8(p + 2));
			if (a < 0 || b < 0 || c < 0) return no("ItemPrices' price " + std::to_string(i) + " isn't decimal");
			it.price = (uint16_t)std::min(65535, a * 10000 + b * 100 + c);
			it.pocket = gen1Pocket(d, (uint8_t)i);
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

		// species, by national number; PureRGB's forms after them, each base's from its formsAt (their Pokédex page their
		// base's)
		out.species.resize((size_t)nAll + 1);
		for (size_t j = 0; j < forms.size(); j++)
		{
			nds::Species& base = out.species[forms[j].second];
			if (!base.formsAt) base.formsAt = (uint16_t)(nSp + 1 + j);
			base.formCount = (uint8_t)(nSp + 1 + j - base.formsAt + 2);
			out.form[forms[j].first] = (uint8_t)(base.formCount - 1);
		}
		if (dexRecords && (!r.has(dexPtrs, (size_t)DEX_ENTRY * nSp) || !r.has(dexCatMap, (size_t)nSp)))
			return no("DexEntryData or PokemonCategoryMapping runs past the ROM");
		for (int s = 1; s <= nAll; s++)
		{
			nds::Species& sp = out.species[s];
			const size_t b = record(s);
			const int idx = internalAt[s];
			sp.name = s > nSp ? out.species[numberOf(s)].name : nameCase(r.text(names + MON_NAME * (size_t)(idx - 1), MON_NAME));
			for (int i = 0; i < 4; i++) sp.base[i] = r.u8(b + 1 + i);
			sp.base[4] = sp.base[5] = r.u8(b + 5);		// (Special, both special stats)
			sp.types[0] = gen2Type(d, r.u8(b + 6));
			sp.types[1] = gen2Type(d, r.u8(b + 7));
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
			// feet and inches, pounds * 10, the text's text_far. PureRGB's: a fixed record by Pokédex number (feet, inches,
			// pounds * 10, its text_far_end), its category the string PokemonCategoryMapping's index names among
			// DexCategoryStrings (data/pokemon/dex_entries.asm GetDexEntryData, dex_pokemon_categories.asm). A form's is
			// its base's
			if (s > nSp) { const nds::Species& base = out.species[numberOf(s)]; sp.category = base.category; sp.height = base.height;
				sp.weight = base.weight; sp.dexText = base.dexText; }
			else if (dexRecords)
			{
				const size_t e = dexPtrs + (size_t)DEX_ENTRY * (s - 1);
				size_t c = dexCats;
				for (int k = r.u8(dexCatMap + s - 1); k > 0 && c != RomView::NONE; k--) c = r.skip(c, 16);
				sp.category = nameCase(r.text(c, 16));
				const int inches = r.u8(e) * 12 + r.u8(e + 1), lb10 = r.le16(e + 2);
				sp.height = (uint16_t)((inches * 254 + 500) / 1000);
				sp.weight = (uint16_t)((lb10 * 4536 + 5000) / 10000);
				sp.dexText = dexText(r, e + 4, textFar);
			}
			else
			{
				const size_t e = r.near(dexPtrs + 2 * (size_t)(idx - 1));
				const size_t hw = r.skip(e, 16);
				if (hw != RomView::NONE && r.has(hw, 4))
				{
					sp.category = nameCase(r.text(e, 16));
					const int inches = r.u8(hw) * 12 + r.u8(hw + 1), lb10 = r.le16(hw + 2);
					sp.height = (uint16_t)((inches * 254 + 500) / 1000);		// in 0.1 m
					sp.weight = (uint16_t)((lb10 * 4536 + 5000) / 10000);		// in 0.1 kg
					sp.dexText = dexText(r, hw + 4, textFar);
				}
			}
			// evolutions, then the moves learned by level (data/pokemon/evos_moves.asm: a pointer an internal index, in
			// the same bank)
			size_t p = r.near(evos + 2 * (size_t)(idx - 1));
			if (p == RomView::NONE) return no("EvosMovesPointerTable's pointer for species " + std::to_string(s) + " leaves its bank");
			// (an entry of a kind Gen I hasn't ends them: the game never evolves by it, engine/pokemon/evos_moves.asm
			// .evoEntryLoop; Yellow Legacy's Metapod has one, a stray "db 7, HARDEN". The moves start after the record's
			// first 0, as the game's .skipEvolutionDataLoop finds them: no evolution entry holds a 0)
			const size_t rec = p;
			for (int k = 0; k < 8 && r.u8(p); k++)
			{
				nds::Evolution ev;
				const uint8_t kind = r.u8(p);
				if (kind == EVOLVE_LEVEL) { ev.method = nds::evo::LEVEL; ev.param = r.u8(p + 1); ev.into = national(r.u8(p + 2)); p += 3; }
				else if (kind == EVOLVE_ITEM) { ev.method = nds::evo::USE_ITEM; ev.param = gen1Item(d, r.u8(p + 1)); ev.into = national(r.u8(p + 3)); p += 4; }
				else if (kind == EVOLVE_TRADE) { ev.method = nds::evo::TRADE; ev.into = national(r.u8(p + 2)); p += 3; }
				else break;
				if (ev.into >= 1) sp.evolutions.push_back(ev);
			}
			p = rec;
			for (int k = 0; k < 64 && r.u8(p); k++) p++;
			if (r.u8(p)) return no("species " + std::to_string(s) + "'s evolutions have no end");
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
		// (PureRGB's entries have a fifth byte: the map whose wild Pokémon the town map shows there, not read)
		const size_t internalSize = d.rom.count("internalMapEntriesWild") ? INTERNAL_MAP + 1 : INTERNAL_MAP;
		std::vector<size_t> groups;
		for (size_t o = internal; r.has(o, 1) && r.u8(o) != END; o += internalSize)
		{
			if (!r.has(o, internalSize) || groups.size() > 255) return no("InternalMapEntries has no end");
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
		// PureRGB's Old Rod is one of two in its code, its Good Rod one of four of its list to -1, or of its ocean list on
		// an ocean map (ItemUseGoodRod, data/wild/good_rod.asm, data/maps/ocean_maps.asm IsMapOceanMap).
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
		// (the Old Rod: retail's one Pokémon in ItemUseOldRod's code, lb bc, 5, MAGIKARP after its call and jp c; or
		// Yellow Legacy's OldRodMons, two of level and species, each half the bites as GoodRodMons' by the same loop; or
		// PureRGB's two in its code, by call Random, and 1, jr z: lb bc, 10, MAGIKARP, jr, then lb bc, 10, GOLDEEN)
		const bool oldRodOk = oldRod != RomView::NONE && r.has(oldRod, 9) && r.u8(oldRod) == 0xCD && r.u8(oldRod + 3) == 0xDA && r.u8(oldRod + 6) == 0x01;
		const bool oldRodTwo = oldRod != RomView::NONE && r.has(oldRod, 21) && r.u8(oldRod) == 0xCD && r.u8(oldRod + 3) == 0xDA &&
			r.u8(oldRod + 6) == 0xCD && r.u8(oldRod + 9) == 0xE6 && r.u8(oldRod + 10) == 0x01 && r.u8(oldRod + 11) == 0x28 &&
			r.u8(oldRod + 13) == 0x01 && r.u8(oldRod + 16) == 0x18 && r.u8(oldRod + 18) == 0x01;
		if (oldRodMons != RomView::NONE && !r.has(oldRodMons, 4)) return no("OldRodMons runs past the ROM");
		// (the Good Rod: retail's two; PureRGB's lists of level and species to -1, each at even chances)
		auto rodList = [&](size_t o, const char* what, std::vector<std::pair<uint8_t, uint8_t>>& mons) {
			for (; r.has(o, 2) && r.u8(o) != END && mons.size() < 8; o += 2) mons.push_back({ r.u8(o + 1), r.u8(o) });
			if (!r.has(o, 1) || r.u8(o) != END || mons.empty()) return no(std::string(what) + " has no end");
			return true;
		};
		std::vector<std::pair<uint8_t, uint8_t>> goodMons, oceanMons;	// (species, level)
		std::set<int> ocean;
		if (!goodRodList)
		{
			if (!r.has(goodRod, 4)) return no("GoodRodMons runs past the ROM");
			for (int j = 0; j < 2; j++) goodMons.push_back({ r.u8(goodRod + 2 * j + 1), r.u8(goodRod + 2 * j) });
		}
		else if (!rodList(goodRod, "GoodRodMons", goodMons)) return false;
		if (goodRodOcean != RomView::NONE)
		{
			if (!rodList(goodRodOcean, "GoodRodMonsOcean", oceanMons)) return false;
			for (size_t o = oceanMaps;; o++)
			{
				if (!r.has(o, 1) || o >= oceanMaps + 256) return no("OceanMaps has no end");
				if (r.u8(o) == END) break;
				ocean.insert(r.u8(o));
			}
		}
		for (const auto& [m, mons] : superMons) if (m < nMaps) water.insert(m);
		for (int m : water)
		{
			nds::Location& loc = out.zones[(uint16_t)m];
			if (oldRodOk) add(loc, r.u8(oldRod + 7), r.u8(oldRod + 8), 100, EncounterKind::OLD_ROD);
			if (oldRodTwo)
				for (int j = 0; j < 2; j++) add(loc, r.u8(oldRod + 14 + 5 * j), r.u8(oldRod + 15 + 5 * j), 50, EncounterKind::OLD_ROD);
			if (oldRodMons != RomView::NONE)
				for (int j = 0; j < 2; j++) add(loc, r.u8(oldRodMons + 2 * j + 1), r.u8(oldRodMons + 2 * j), 50, EncounterKind::OLD_ROD);
			const auto& good = ocean.count(m) ? oceanMons : goodMons;
			for (const auto& [sp, lv] : good) add(loc, sp, lv, (int)((100 + good.size() / 2) / good.size()), EncounterKind::GOOD_ROD);
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
		// (and PureRGB's functions for two, three and five of the item: engine/events/hidden_items.asm)
		std::vector<Addr> giveItem = { d.rom.at("hiddenItems") };
		for (const char* k : { "hiddenItems2", "hiddenItems3", "hiddenItems5" }) if (d.rom.count(k)) giveItem.push_back(d.rom.at(k));
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
				if (std::none_of(giveItem.begin(), giveItem.end(), [&](const Addr& a) { return r.u8(e + 3) == a.bank && r.le16(e + 4) == a.addr; }))
					continue;
				auto f = flagOf.find((uint32_t)(m << 16 | r.u8(e) << 8 | r.u8(e + 1)));
				if (f == flagOf.end())
					return no("map " + std::to_string(m) + "'s hidden item at " + std::to_string(r.u8(e + 1)) + ", " + std::to_string(r.u8(e)) +
						" isn't in HiddenItemCoords");
				out.hidden.push_back({ m, r.u8(e + 1), r.u8(e), gen1Item(d, r.u8(e + 2)), f->second });
			}
		}

		// trainers (data/trainers/parties.asm: TrainerDataPointers' pointer a class; a class's trainers one after another
		// up to the next class's, the last class's up to TrainerAI: a level and its species to 0, or OWN_LEVELS and a
		// level and species each to 0), by class << 8 | number, named by the class (TrainerNames): Gen I's trainers
		// have no names of their own. PureRGB's (with its MoveSetMappings) have two kinds more: OWN_LEVELS_ALT as
		// OWN_LEVELS, and OWN_MOVES, a moveset's number from 1 and then the same; their levels' bit 7, and OWN_LEVELS'
		// too, is an alternate palette (engine/battle/read_trainer_party.asm ReadTrainer .SpecialTrainerLoop). An
		// OWN_MOVES party's members take the moveset's four moves each, in order (data/trainers/custom_movesets.asm
		// LoadTrainerMoveSet)
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
				const bool pure = moveSets != RomView::NONE;
				const bool movesOwn = pure && r.u8(o) == OWN_MOVES;
				const bool own = r.u8(o) == OWN_LEVELS || (pure && (r.u8(o) == OWN_LEVELS_ALT || movesOwn));
				const uint8_t level = r.u8(o);
				size_t q = o + 1;
				size_t set = RomView::NONE;		// (an OWN_MOVES party's moveset: four moves a member)
				if (movesOwn)
				{
					const int k = r.u8(q++);
					set = k >= 1 ? r.near(moveSets + 2 * (size_t)(k - 1)) : RomView::NONE;
					if (set == RomView::NONE || k > 64) return no("trainer " + std::to_string(n) + " of class " + std::to_string(c + 1) + "'s moveset isn't one of MoveSetMappings'");
				}
				// (PureRGB's real MissingNo., a species of its own with no Pokédex number, fights in its Champion's Arena: it
				// is left out of the team, its place kept for the moveset)
				for (size_t member = 0; q < end && r.u8(q); member++)
				{
					nds::TrainerMon m;
					m.level = own ? (uint8_t)(r.u8(q++) & (pure ? 0x7F : 0xFF)) : level;
					const uint8_t i = r.u8(q++);
					m.species = national(i);
					const bool unnumbered = pure && !m.species && nonDex.count(i);
					if (m.level < 1 || m.level > 100 || (!m.species && !unnumbered) || member == 6 || q > end)
						return no("trainer " + std::to_string(n) + " of class " + std::to_string(c + 1) + " isn't a party");
					if (unnumbered) continue;
					defaultMoves(out, m);
					if (set != RomView::NONE)
						for (int k = 0; k < 4; k++)
						{
							const size_t mv = set + 4 * member + (size_t)k;
							if (!r.has(mv, 1) || r.u8(mv) > nMoves) return no("trainer " + std::to_string(n) + " of class " + std::to_string(c + 1) + "'s moveset isn't moves");
							m.moves[k] = r.u8(mv);
						}
					tr.team.push_back(m);
				}
				if (q >= end || tr.team.empty()) return no("trainer " + std::to_string(n) + " of class " + std::to_string(c + 1) + " has no end");
				o = q + 1;
				if (own && !movesOwn) ownLevels.insert(tr.id);
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
		// (PureRGB's: LoneMoves name the move's place too, and TeamMoves go to the sixth member's second move, both to the
		// parties with a level each but its OWN_MOVES ones; read_trainer_party.asm .AddLoneMove, .GiveTeamMoves)
		std::set<int> lone;
		const size_t loneAt = loneSlots != RomView::NONE ? loneSlots : loneMoves, teamAt = teamSixth != RomView::NONE ? teamSixth : teamMoves;
		const size_t loneSize = loneSlots != RomView::NONE ? LONE_SLOT : 2;
		if (loneAt != RomView::NONE)
		{
			if (!r.has(loneAt, loneSize * 8)) return no("LoneMoves runs past the ROM");
			for (int g = 0; g < 8; g++)
			{
				const int cls = d.size(GYMS[g].cls);
				const size_t e = loneAt + loneSize * (size_t)g;
				const size_t slot = loneSize == LONE_SLOT ? r.u8(e + 1) : 2;
				if (cls < 1 || cls > nClasses || !give((uint16_t)(cls << 8 | GYMS[g].number), r.u8(e), slot, r.u8(e + loneSize - 1), true))
					return no(std::string("LoneMoves' gym ") + std::to_string(g + 1) + " isn't a party of its leader's (" + GYMS[g].cls + ")");
				lone.insert(cls << 8 | GYMS[g].number);
			}
		}
		if (teamAt != RomView::NONE)
			for (size_t o = teamAt; r.u8(o) != END; o += 2)
			{
				if (!r.has(o, 3) || o >= teamAt + 2 * 64) return no("TeamMoves has no end");
				const size_t member = teamSixth != RomView::NONE ? 5 : 4, slot = teamSixth != RomView::NONE ? 1 : 2;
				for (int n = 1; n < 256; n++)
					if (!lone.count(r.u8(o) << 8 | n) && out.trainers.count((uint16_t)(r.u8(o) << 8 | n))) give((uint16_t)(r.u8(o) << 8 | n), member, slot, r.u8(o + 1), true);
			}
		if (special != RomView::NONE)
			for (size_t o = special; r.u8(o) != END;)
			{
				if (!r.has(o, 3) || o >= special + 0x4000) return no("SpecialTrainerMoves has no end");	// (within its bank: Yellow Legacy's is 2 KB)
				const uint16_t id = (uint16_t)(r.u8(o) << 8 | r.u8(o + 1));
				for (o += 2; r.u8(o); o += 3)
					if (!r.has(o, 4) || !give(id, (size_t)r.u8(o) - 1, (size_t)r.u8(o + 1) - 1, r.u8(o + 2), false))
						return no("SpecialTrainerMoves' entry for trainer " + std::to_string(id >> 8) + ":" + std::to_string(id & 0xFF) + " isn't its party's");
				o++;
			}

		// PureRGB's rules (rules.h GameRules). The species whose category of a type goes by its base stats: base Special
		// above Attack (engine/battle/core.asm DynamicTypeCheckPlayer), by national number
		for (int s = 1; s <= nSp && s < 256; s++)
			out.rules.statsSpecial[(size_t)s] = out.species[s].base[4] > out.species[s].base[1];
		// the species the type NPC gives back their original types: species, two types, to -1 (engine/pokemon/
		// remap_typings.asm OriginalTypings), at most the save's 24 bits (wPkmnTypeRemapFlags, 3 bytes)
		const size_t originals = optional("originalTypings");
		if (originals != RomView::NONE)
			for (size_t o = originals;; o += ORIGINAL_TYPING)
			{
				if (!r.has(o, 1)) return no("OriginalTypings runs past the ROM");
				if (r.u8(o) == END) break;
				const size_t n = out.originalTypes.size();
				const uint8_t i = r.u8(o);
				if (n >= REMAP_BITS || !r.has(o, ORIGINAL_TYPING)) return no("OriginalTypings has no end");
				GbRomData::OriginalTyping t;
				const nds::Species& base = out.species[national(i)];
				t.species = out.form[i] ? (uint16_t)(base.formsAt + out.form[i] - 1) : national(i);
				t.types[0] = gen2Type(d, r.u8(o + 1));
				t.types[1] = gen2Type(d, r.u8(o + 2));
				if (!national(i) || t.types[0] == nds::NO_TYPE || t.types[1] == nds::NO_TYPE)
					return no("OriginalTypings' entry " + std::to_string(n + 1) + " isn't a species and two types");
				out.originalTypes.push_back(t);
			}
		// the moves stronger by the species that uses them: move, species (-1 any), power (-1 the move's, -2 a rule of the
		// battle's, by the function the fourth byte names), accuracy (0 the move's), to -1; the game takes a move's first
		// entry (engine/battle/remap_move_data.asm CheckRemapMoveData, IsInArray), so a move's later ones are passed over.
		// Only the species' (the signatures) are kept: those of any species depend on the battle (Sing's, Explosion's ..)
		const size_t remappable = optional("remappableMoves");
		if (remappable != RomView::NONE)
		{
			std::set<int> seen;
			for (size_t o = remappable;; o += REMAPPABLE_MOVE)
			{
				if (!r.has(o, 1)) return no("RemappableMoves runs past the ROM");
				if (r.u8(o) == END) break;
				const size_t n = (o - remappable) / REMAPPABLE_MOVE + 1;
				if (n > 64 || !r.has(o, REMAPPABLE_MOVE)) return no("RemappableMoves has no end");
				const int m = r.u8(o), sp = r.u8(o + 1), power = r.u8(o + 2), acc = r.u8(o + 3);
				if (m < 1 || m > nMoves || (sp != ANY_SPECIES && !national((uint8_t)sp)) || (power == MODIFIER_FUNCTION && sp != ANY_SPECIES))
					return no("RemappableMoves' entry " + std::to_string(n) + " isn't a move and a species");
				if (!seen.insert(m).second || sp == ANY_SPECIES) continue;
				Signature s;
				s.move = out.moves[m].name;
				s.species = national((uint8_t)sp);
				s.power = power == SAME_POWER ? 0 : (uint8_t)power;
				s.accuracy = acc ? percent((uint8_t)acc) : 0;
				out.rules.signatures.push_back(s);
			}
		}
		return true;
	}
}
