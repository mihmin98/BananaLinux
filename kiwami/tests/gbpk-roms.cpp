// The Game Boy Pokémon companion's ROM reader (ui/src/kiwami/gbpk/rom.h) on the player's own ROMs, which are never in
// the repository: each .gb/.gbc in the folder that is a release the data files list (kiwami/tests/gbpk/roms.sha1, by
// SHA-1) is read, and what the ROM is known to hold is checked against what was read, the game's guide file with it.
// With a PokeDaisy checkout, every species' gender ratio and growth rate are compared with PokeDaisy's tables
// (Gen2SpeciesCrystal.kt), and Crystal's, Red's, Blue's and Yellow's hidden items with their guides' (GuideCrystalGen.kt,
// GuideGen1Gen.kt). With
// GBPK_ROMS_OUT set, every species' front picture (normal, and Gen II's shiny) and the region maps are written there as
// PNGs, a game's <key>-pics.png, <key>-shiny.png and <key>-<map>.png, for looking at. Built and run by
// kiwami/tests/gbpk-roms.sh after its check of the folder:
//   gbpk-roms <the data files' folder> <roms.sha1> <the ROMs' folder> [<pokedaisy checkout>]
// Copyright (C) 2026 Lidor Itzhari (PokeDaisy) and the kiwami fork's contributors. GNU GPL version 3 with PokeDaisy's
// additional term under section 7(b): LICENSE and NOTICE in ui/src/kiwami/gbpk/. SPDX-License-Identifier: GPL-3.0-only
// Based on PokeDaisy by Lidor Itzhari - https://github.com/lidor30/pokedaisy
#include "kiwami/gbpk/games.h"
#include "kiwami/gbpk/rom.h"
#include "kiwami/gbpk/gsguide.h"
#include "nds/guide.h"
#include "nds/rom.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../third_party/stb_image_write.h"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <fstream>
#include <map>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

using namespace banana;
using namespace banana::kiwami::gbpk;

static int checks = 0, failed = 0;
static std::string current;
#define CHECK(c) do { checks++; if (!(c)) { failed++; fprintf(stderr, "gbpk-roms: FAIL %s: %s:%d: %s\n", current.c_str(), __FILE__, __LINE__, #c); } } while (0)
#define EQ(a, b) do { checks++; std::string a_ = (a), b_ = (b); if (a_ != b_) { failed++; fprintf(stderr, "gbpk-roms: FAIL %s: %s:%d: %s is \"%s\", not \"%s\"\n", current.c_str(), __FILE__, __LINE__, #a, a_.c_str(), b_.c_str()); } } while (0)

// a Kotlin intArrayOf(...) named name in text, as numbers
static std::vector<int> intArray(const std::string& text, const std::string& name)
{
	std::vector<int> out;
	size_t a = text.find("val " + name + " = intArrayOf(");
	if (a == std::string::npos) return out;
	a = text.find('(', a) + 1;
	size_t b = text.find(')', a);
	std::stringstream s(text.substr(a, b - a));
	std::string t;
	while (std::getline(s, t, ',')) if (t.find_first_of("0123456789") != std::string::npos) out.push_back(atoi(t.c_str() + t.find_first_of("0123456789")));
	return out;
}

static const nds::Location* place(const nds::RomData& r, const std::string& name)
{
	for (const auto& [z, l] : r.zones) if (l.name == name && !l.encounters.empty()) return &l;
	return nullptr;
}

// a picture (0xRRGGBB, opaque) as a PNG
static void png(const std::string& path, const std::vector<uint32_t>& px, int w, int h)
{
	std::vector<uint8_t> rgb((size_t)w * h * 3);
	for (size_t i = 0; i < px.size(); i++) { rgb[i * 3] = (uint8_t)(px[i] >> 16); rgb[i * 3 + 1] = (uint8_t)(px[i] >> 8); rgb[i * 3 + 2] = (uint8_t)px[i]; }
	if (!stbi_write_png(path.c_str(), w, h, 3, rgb.data(), w * 3)) fprintf(stderr, "gbpk-roms: couldn't write %s\n", path.c_str());
}

// every species' front picture: drawn, at its size (5, 6 or 7 tiles; Gen II's from its base data, Gen I's the
// stream's own first byte), Chikorita's 5 x 5; Gen I's Bulbasaur 5 x 5 and Mewtwo 7 x 7 (pret's
// gfx/pokemon/front/*.png), each with one palette (shiny the same); written as two sheets of 16 x 16 boxes of 7 x 7
// tiles, each picture where the game puts it in its box (PadFrontpic, LoadUncompressedSpriteData: a column in, on the
// box's bottom), on its own white
static void checkPics(const nds::RomData& r, const std::vector<uint8_t>& rom, const std::string& out, const std::string& key)
{
	const GbRomData& g = static_cast<const GbRomData&>(r);
	const int box = 56, cols = 16, sheetW = cols * box, sheetH = 16 * box, count = (int)r.species.size() - 1;
	std::vector<uint32_t> normal((size_t)sheetW * sheetH, 0xC0C0C0), shiny = normal;
	int drawn = 0, sized = 0, same = 0;
	auto t0 = std::chrono::steady_clock::now();
	for (int s = 1; s <= count; s++)
	{
		auto sp = r.sprite((uint16_t)s);
		if (!sp || sp->frames.size() != 1 || sp->w != sp->h) { fprintf(stderr, "gbpk-roms: %s: species %d's picture doesn't decode\n", current.c_str(), s); continue; }
		const auto& f = sp->frames[0];
		bool any = false;
		for (uint8_t v : f) any |= v != 0;
		drawn += any;
		// (Gen II: BaseData's picture size, the record's byte 17, dn w, h, rom.h's GbRomData keeping where BaseData is;
		// Gen I: the picture's own first byte, dn w, h)
		const int size = g.gen == 1 ? rom[g.pics1.pic[s]] >> 4 : rom[g.pics.baseData + 32 * (s - 1) + 17] & 0xF;
		sized += sp->w == size * 8;
		same += sp->normal == sp->shiny;
		const int ox = (s - 1) % cols * box + (7 - size + 1) / 2 * 8, oy = (s - 1) / cols * box + (7 - size) * 8;
		for (int y = 0; y < sp->h; y++)
			for (int x = 0; x < sp->w; x++)
			{
				const uint8_t v = f[(size_t)y * sp->w + x];
				normal[(size_t)(oy + y) * sheetW + ox + x] = sp->normal[v];
				shiny[(size_t)(oy + y) * sheetW + ox + x] = sp->shiny[v];
			}
	}
	const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
	CHECK(drawn == count && sized == count && count == (g.gen == 1 ? 151 : 251));
	if (g.gen == 1)
	{
		auto bulbasaur = r.sprite(1), mewtwo = r.sprite(150);
		CHECK(bulbasaur && bulbasaur->w == 40 && bulbasaur->h == 40 && mewtwo && mewtwo->w == 56 && mewtwo->h == 56);
		CHECK(same == count);
	}
	else
	{
		auto chikorita = r.sprite(152);
		CHECK(chikorita && chikorita->w == 40 && chikorita->h == 40);
	}
	fprintf(stderr, "gbpk-roms: %s: %d front pictures decoded in %.1f ms\n", current.c_str(), drawn, ms);
	if (out.empty()) return;
	png(out + "/" + key + "-pics.png", normal, sheetW, sheetH);
	if (g.gen == 2) png(out + "/" + key + "-shiny.png", shiny, sheetW, sheetH);
}

// Gen I's town map: drawn, Pallet Town's place on it and its cell naming it, as TownMapOrder's first; every named
// place on it; written as <key>-Kanto.png
static void checkTownMap(const nds::RomData& r, const std::string& out, const std::string& key)
{
	CHECK(r.regionMaps.size() == 1);
	if (r.regionMaps.size() != 1) return;
	const nds::RegionMap& m = r.regionMaps[0];
	EQ(m.name, "Kanto");
	CHECK(m.w == 160 && m.h == 144 && m.rgba.size() == 160 * 144 && m.cells.size() == 360);
	if (!out.empty() && m.rgba.size() == (size_t)m.w * m.h) png(out + "/" + key + "-" + m.name + ".png", m.rgba, m.w, m.h);
	const nds::MapSpot* pallet = r.zones.count(0) ? &r.zones.at(0).spot : nullptr;
	CHECK(pallet && pallet->map == 0 && pallet->w == 16 && pallet->h == 16);
	if (pallet && pallet->map == 0)
	{
		const int cx = (int)(pallet->x + pallet->w / 2) / m.cell, cy = (int)(pallet->y + pallet->h / 2) / m.cell;
		const uint16_t id = cx >= 0 && cy >= 0 && cx < m.gridW && cy < m.gridH ? m.cells[(size_t)cy * m.gridW + cx] : 0xFFFF;
		EQ(id < r.locationNames.size() ? r.locationNames[id] : "", "Pallet Town");
	}
	int placed = 0, named = 0, cells = 0;
	for (const auto& [z, l] : r.zones)
		if (!l.name.empty()) { named++; placed += l.spot.map == 0; }
	for (uint16_t c : m.cells) cells += c != 0xFFFF;
	CHECK(placed == named && named > 100 && cells > 47);
	fprintf(stderr, "gbpk-roms: %s: %d places on the town map, %d cells named\n", current.c_str(), placed, cells);
}

// the Pokégear's maps: both drawn, New Bark Town's place on Johto's and its cell naming it, Pallet Town's on Kanto's;
// every named place but the rooms of the special landmark on a map
static void checkMaps(const nds::RomData& r, const std::string& out, const std::string& key)
{
	CHECK(r.regionMaps.size() == 2);
	if (r.regionMaps.size() != 2) return;
	EQ(r.regionMaps[0].name, "Johto");
	EQ(r.regionMaps[1].name, "Kanto");
	for (const auto& m : r.regionMaps)
	{
		CHECK(m.w == 160 && m.h == 144 && m.rgba.size() == 160 * 144 && m.cells.size() == 360);
		if (!out.empty() && m.rgba.size() == (size_t)m.w * m.h) png(out + "/" + key + "-" + m.name + ".png", m.rgba, m.w, m.h);
	}
	auto spotOf = [&](const std::string& name) -> const nds::MapSpot* {
		for (const auto& [z, l] : r.zones) if (l.name == name) return &l.spot;
		return nullptr;
	};
	const nds::MapSpot* nb = spotOf("New Bark Town");
	const nds::MapSpot* pallet = spotOf("Pallet Town");
	CHECK(nb && nb->map == 0 && pallet && pallet->map == 1);
	if (nb && nb->map == 0)
	{
		const nds::RegionMap& j = r.regionMaps[0];
		const int cx = (int)(nb->x + nb->w / 2) / j.cell, cy = (int)(nb->y + nb->h / 2) / j.cell;
		const uint16_t id = cx >= 0 && cy >= 0 && cx < j.gridW && cy < j.gridH ? j.cells[(size_t)cy * j.gridW + cx] : 0xFFFF;
		EQ(id < r.locationNames.size() ? r.locationNames[id] : "", "New Bark Town");
	}
	int placed = 0, named = 0;
	for (const auto& [z, l] : r.zones)
		if (!l.name.empty() && l.name != "Special") { named++; placed += l.spot.map >= 0; }
	CHECK(placed == named && named > 300);
	fprintf(stderr, "gbpk-roms: %s: %d places on the Pokégear's maps\n", current.c_str(), placed);
}

static void checkGen2(const nds::RomData& r, const std::string& key, const std::string& pokedaisy)
{
	CHECK(r.species.size() == 252 && r.moves.size() == 252);
	int named = 0;
	for (size_t s = 1; s < r.species.size(); s++) named += !r.species[s].name.empty() && !r.species[s].learnset.empty();
	CHECK(named == 251);
	named = 0;
	for (size_t m = 1; m < r.moves.size(); m++) named += !r.moves[m].name.empty();
	CHECK(named == 251);
	// Chikorita (pret's data/pokemon/base_stats/chikorita.asm, evos_attacks.asm, dex_entries/chikorita.asm)
	const nds::Species& c = r.species[152];
	EQ(c.name, "Chikorita");
	CHECK((c.base[0] == 45 && c.base[1] == 49 && c.base[2] == 65 && c.base[3] == 45 && c.base[4] == 49 && c.base[5] == 65));
	CHECK(c.types[0] == nds::GRASS && c.types[1] == nds::GRASS && c.catchRate == 45 && c.genderRatio == 31 && c.growth == 3);
	CHECK(c.evolutions.size() == 1 && c.evolutions[0].into == 153 && c.evolutions[0].method == nds::evo::LEVEL && c.evolutions[0].param == 16);
	EQ(c.evolutions.empty() ? "" : c.evolutions[0].how, "Level 16");
	CHECK(!c.learnset.empty() && c.learnset[0].level == 1 && c.learnset[0].move == 33);
	EQ(r.moves[33].name, "Tackle");
	EQ(c.category, "Leaf");
	CHECK(c.height == 9 && c.weight == 64);	// 2'11" and 14.0 lb: 0.9 m, and 6.4 kg as the game reckons it
	CHECK(c.dexText.size() > 40 && c.dexText.find("  ") == std::string::npos);
	CHECK(!c.tms.empty() && c.tms.back() >= 93);	// (Cut and Flash: HMs)
	// the moves: Pound, Quick Attack's priority, Vital Throw's, a special one, TM01
	CHECK(r.moves[1].power == 40 && r.moves[1].accuracy == 100 && r.moves[1].pp == 35 && r.moves[1].cat == nds::MoveCategory::PHYSICAL);
	CHECK((int8_t)r.moves[98].priority == 1 && (int8_t)r.moves[233].priority == -1 && (int8_t)r.moves[182].priority == 2);
	EQ(r.moves[98].name, "Quick Attack");
	CHECK(r.moves[53].type == nds::FIRE && r.moves[53].cat == nds::MoveCategory::SPECIAL);	// Flamethrower
	CHECK(r.tmMoves[0] == 223 && r.tmMoves[92] == 15);	// TM01 DynamicPunch, HM01 Cut
	CHECK(!r.moves[1].description.empty());
	// items
	EQ(r.items[1].name, "Master Ball");
	CHECK(r.items[1].pocket == nds::POCKET_BALLS);
	EQ(r.items[328].name, "TM01");
	CHECK(r.items[GEN2_ONLY + 0x1A].name == "HP Up" && r.items[GEN2_ONLY + 0x1A].price == 9800);
	// Route 29's morning grass, Falkner
	const nds::Location* route29 = place(r, "Route 29");
	CHECK(route29);
	bool pidgey = false, sentret = false;
	for (const auto& e : route29 ? route29->encounters : std::vector<nds::Encounter>{})
		if (e.kind == nds::EncounterKind::GRASS && e.time == nds::TimeOfDay::MORNING) { pidgey |= e.species == 16; sentret |= e.species == 161; }
	CHECK(pidgey && sentret);
	// fishing where there's water: New Bark Town's sea, not Route 29 (no water, the default group every map has)
	auto fishes = [](const nds::Location* l) {
		for (const auto& e : l ? l->encounters : std::vector<nds::Encounter>{}) if (e.kind == nds::EncounterKind::OLD_ROD) return true;
		return false;
	};
	const nds::Location* newBark = nullptr;
	for (const auto& [z, l] : r.zones) if (l.name == "New Bark Town" && fishes(&l)) newBark = &l;
	CHECK(newBark && !fishes(route29));
	const nds::Trainer* falkner = nullptr;
	for (const auto& [id, t] : r.trainers) if (t.name == "Falkner") falkner = &t;
	CHECK(falkner && falkner->cls == "Leader" && falkner->id == (1 << 8 | 1));
	CHECK(falkner && falkner->team.size() == 2 && falkner->team[0].species == 16 && falkner->team[0].level == 7 &&
		falkner->team[1].species == 17 && falkner->team[1].level == 9);
	// every place with wild Pokémon is named, and the kinds are there
	std::set<nds::EncounterKind> kinds;
	int places = 0, unnamed = 0;
	for (const auto& [z, l] : r.zones)
		if (!l.encounters.empty())
		{
			places++;
			unnamed += l.name.empty();
			for (const auto& e : l.encounters) kinds.insert(e.kind);
		}
	CHECK(unnamed == 0 && places > 80);
	CHECK(kinds.count(nds::EncounterKind::SURF) && kinds.count(nds::EncounterKind::OLD_ROD) && kinds.count(nds::EncounterKind::SUPER_ROD) &&
		kinds.count(nds::EncounterKind::HEADBUTT) && kinds.count(nds::EncounterKind::ROCK_SMASH));
	CHECK(r.trainers.size() > 400);
	fprintf(stderr, "gbpk-roms: %s: %zu places, %d with wild Pokémon, %zu trainers, %zu place names\n", current.c_str(),
		r.zones.size(), places, r.trainers.size(), r.locationNames.size());

	// PokeDaisy's gender ratios and growth rates (its tables are Crystal's; Gold's and Silver's species are the same)
	if (!pokedaisy.empty())
	{
		std::ifstream f(pokedaisy + "/app/src/main/kotlin/com/pokedaisy/app/companion/data/Gen2SpeciesCrystal.kt");
		std::string text((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
		std::vector<int> g = intArray(text, "gen2GenderRatiosCrystal"), gr = intArray(text, "gen2GrowthRatesCrystal");
		CHECK(g.size() == 252 && gr.size() == 252);
		int differ = 0;
		for (size_t s = 1; s < std::min({ g.size(), gr.size(), r.species.size() }); s++)
			if (r.species[s].genderRatio != g[s] || r.species[s].growth != gr[s]) { differ++; fprintf(stderr, "gbpk-roms: %s: species %zu: gender %d growth %d, PokeDaisy's %d %d\n", current.c_str(), s, r.species[s].genderRatio, r.species[s].growth, g[s], gr[s]); }
		CHECK(differ == 0);
		fprintf(stderr, "gbpk-roms: %s: 251 gender ratios and growth rates are PokeDaisy's (%s)\n", current.c_str(), key.c_str());
	}
}

// Red's, Blue's and Yellow's tables (Fast Red and Fast Blue as Red and Blue): pret's pokered and pokeyellow say what
// each holds (data/pokemon/base_stats/bulbasaur.asm, dex_entries.asm, evos_moves.asm, data/wild/maps/Route1.asm,
// data/trainers/parties.asm and special_moves.asm, engine/items/item_effects.asm)
static void checkGen1(const nds::RomData& r, const GameData& d)
{
	const bool yellow = d.game.id == YELLOW;
	CHECK(r.species.size() == 152 && r.moves.size() == 166);
	int named = 0;
	for (size_t s = 1; s < r.species.size(); s++) named += !r.species[s].name.empty() && !r.species[s].learnset.empty() && !r.species[s].dexText.empty();
	CHECK(named == 151);
	named = 0;
	for (size_t m = 1; m < r.moves.size(); m++) named += !r.moves[m].name.empty();
	CHECK(named == 165);
	// Bulbasaur: Special as both special stats, no gender, Tackle and Growl from level 1, Ivysaur at 16
	const nds::Species& b = r.species[1];
	EQ(b.name, "Bulbasaur");
	CHECK((b.base[0] == 45 && b.base[1] == 49 && b.base[2] == 49 && b.base[3] == 45 && b.base[4] == 65 && b.base[5] == 65));
	CHECK(b.types[0] == nds::GRASS && b.types[1] == nds::POISON && b.catchRate == 45 && b.baseExp == 64 && b.genderRatio == 255 &&
		b.growth == 3);
	CHECK(b.learnset.size() >= 2 && b.learnset[0].level == 1 && b.learnset[0].move == 33 && b.learnset[1].level == 1 && b.learnset[1].move == 45);
	CHECK(b.evolutions.size() == 1 && b.evolutions[0].into == 2 && b.evolutions[0].method == nds::evo::LEVEL && b.evolutions[0].param == 16);
	EQ(b.category, "Seed");
	CHECK(b.height == 7 && b.weight == 68);	// 2'04" and 15.0 lb: 0.7 m, 6.8 kg
	CHECK(b.dexText.size() > 40 && b.dexText.back() == '.' && b.dexText.find("  ") == std::string::npos);
	CHECK(!b.tms.empty() && b.tms.back() == 93);	// (Cut: HM01)
	EQ(r.species[151].name, "Mew");
	CHECK(r.species[151].base[0] == 100 && r.species[151].base[4] == 100 && r.species[151].tms.size() == 55);
	EQ(r.species[29].name, "Nidoran♀");
	// Clefairy by a Moon Stone, Kadabra by trade
	CHECK(r.species[35].evolutions.size() == 1 && r.species[35].evolutions[0].into == 36);
	EQ(r.species[35].evolutions.empty() ? "" : r.species[35].evolutions[0].how, "Use Moon Stone");
	CHECK(r.species[64].evolutions.size() == 1 && r.species[64].evolutions[0].method == nds::evo::TRADE && r.species[64].evolutions[0].into == 65);
	// the moves: Pound, Quick Attack's and Counter's priorities, a special one, TM01 and HM01
	EQ(r.moves[1].name, "Pound");
	CHECK(r.moves[1].power == 40 && r.moves[1].accuracy == 100 && r.moves[1].pp == 35 && r.moves[1].cat == nds::MoveCategory::PHYSICAL);
	CHECK((int8_t)r.moves[98].priority == 1 && (int8_t)r.moves[68].priority == -1 && r.moves[33].priority == 0);
	EQ(r.moves[98].name, "Quick Attack");
	CHECK(r.moves[53].type == nds::FIRE && r.moves[53].cat == nds::MoveCategory::SPECIAL);	// Flamethrower
	CHECK(r.tmMoves[0] == 5 && r.tmMoves[92] == 15);	// TM01 Mega Punch, HM01 Cut
	// items
	EQ(r.items[1].name, "Master Ball");
	CHECK(r.items[1].pocket == nds::POCKET_BALLS && r.items[2].price == 1200);
	EQ(r.items[81].name, "Moon Stone");
	EQ(r.items[328].name, "TM01");
	EQ(r.items[424].name, "HM05");
	CHECK(r.items[GEN1_ONLY + 0x28].name == "Rare Candy" && r.items[GEN1_ONLY + 0x28].price == 4800);
	// Route 1's grass; fishing at Pallet Town: the Old Rod's Magikarp, the Good Rod's Goldeen and Poliwag, the Super
	// Rod's; none on Route 1
	const nds::Location* route1 = place(r, "Route 1");
	bool pidgey = false, rattata = false;
	for (const auto& e : route1 ? route1->encounters : std::vector<nds::Encounter>{})
		if (e.kind == nds::EncounterKind::GRASS) { pidgey |= e.species == 16; rattata |= e.species == 19; }
	CHECK(pidgey && rattata);
	const nds::Location* pallet = r.zones.count(0) ? &r.zones.at(0) : nullptr;
	EQ(pallet ? pallet->name : "", "Pallet Town");
	auto has = [](const nds::Location* l, nds::EncounterKind k, int species, int level) {
		for (const auto& e : l ? l->encounters : std::vector<nds::Encounter>{})
			if (e.kind == k && e.species == species && (!level || e.minLevel == level)) return true;
		return false;
	};
	CHECK(has(pallet, nds::EncounterKind::OLD_ROD, 129, 5) && has(pallet, nds::EncounterKind::GOOD_ROD, 118, 10) &&
		has(pallet, nds::EncounterKind::GOOD_ROD, 60, 10));
	int superRod = 0;
	for (const auto& e : pallet ? pallet->encounters : std::vector<nds::Encounter>{}) superRod += e.kind == nds::EncounterKind::SUPER_ROD;
	CHECK(superRod >= 1 && superRod <= 4 && !has(route1, nds::EncounterKind::OLD_ROD, 129, 0));
	// Brock: Geodude and Onix, Lv 12 and 14 (Yellow's 10 and 12), Onix with Bide (Yellow's: Bind and Bide); Lorelei's
	// Lapras with Blizzard (Red's and Blue's TeamMoves; Yellow's table gives it too)
	const nds::Trainer* brock = nullptr;
	const nds::Trainer* lorelei = nullptr;
	for (const auto& [id, t] : r.trainers)
	{
		if (t.name == "Brock" && !brock) brock = &t;
		if (t.name == "Lorelei" && !lorelei) lorelei = &t;
	}
	CHECK(brock && brock->cls == "Brock" && (brock->id & 0xFF) == 1);
	CHECK(brock && brock->team.size() == 2 && brock->team[0].species == 74 && brock->team[1].species == 95 &&
		brock->team[0].level == (yellow ? 10 : 12) && brock->team[1].level == (yellow ? 12 : 14));
	CHECK(brock && brock->team.size() == 2 && brock->team[1].moves[yellow ? 3 : 2] == 117 && (!yellow || brock->team[1].moves[2] == 20));
	CHECK(lorelei && lorelei->team.size() == 5 && lorelei->team[4].species == 131 && lorelei->team[4].moves[2] == 59);
	// every place with wild Pokémon is named
	std::set<nds::EncounterKind> kinds;
	int places = 0, unnamed = 0;
	for (const auto& [z, l] : r.zones)
		if (!l.encounters.empty())
		{
			places++;
			unnamed += l.name.empty();
			for (const auto& e : l.encounters) kinds.insert(e.kind);
		}
	CHECK(unnamed == 0 && places > 50);
	CHECK(kinds.count(nds::EncounterKind::GRASS) && kinds.count(nds::EncounterKind::SURF) && kinds.count(nds::EncounterKind::OLD_ROD) &&
		kinds.count(nds::EncounterKind::GOOD_ROD) && kinds.count(nds::EncounterKind::SUPER_ROD));
	CHECK(r.trainers.size() > 350);
	fprintf(stderr, "gbpk-roms: %s: %zu places, %d with wild Pokémon, %zu trainers, %zu place names\n", current.c_str(),
		r.zones.size(), places, r.trainers.size(), r.locationNames.size());
}

// the hidden items from the maps' events, and the game's guide file over this ROM: every place found by its name,
// every boss's teams the ROM's with its top level (the Champion of Gen I three, one for each starter). Crystal's hidden
// items are PokeDaisy's, by flag and item (its GuideCrystalGen.kt's HIDDEN lines: landmark|HIDDEN|item|flag|...); Red's,
// Blue's and Yellow's by map, flag and item (GuideGen1Gen.kt's <prefix>_AREAS_RAW: map|HIDDEN|item|0x20000 + flag|...).
static void checkGuide(const nds::RomData& r, const GameData& d, const std::string& pokedaisy)
{
	const std::vector<HiddenItem>& hidden = static_cast<const GbRomData&>(r).hidden;
	int named = 0;
	for (const HiddenItem& h : hidden)
		named += r.zones.count(h.zone) && !r.zones.at(h.zone).name.empty() && h.item < r.items.size() && !r.items[h.item].name.empty();
	CHECK(hidden.size() > 50 && named == (int)hidden.size());
	fprintf(stderr, "gbpk-roms: %s: %zu hidden items\n", current.c_str(), hidden.size());
	if (d.key == "crystal" && !pokedaisy.empty())
	{
		std::ifstream f(pokedaisy + "/app/src/main/kotlin/com/pokedaisy/app/companion/data/GuideCrystalGen.kt");
		// (a multiset: the National Park's Full Heal lies on the park's map and on its Bug Contest copy, one flag)
		std::multiset<std::pair<int, int>> theirs, ours;
		for (std::string l; std::getline(f, l);)
		{
			int lm, item, flag;
			if (sscanf(l.c_str(), "%d|HIDDEN|%d|%d|", &lm, &item, &flag) == 3) theirs.insert({ flag, gen2Item((uint8_t)item) });
		}
		for (const HiddenItem& h : hidden) ours.insert({ h.flag, h.item });
		CHECK(theirs.size() == 85 && ours == theirs);
		fprintf(stderr, "gbpk-roms: %s: the %zu hidden items are PokeDaisy's, by flag and item\n", current.c_str(), theirs.size());
	}
	if (d.gen == 1 && !pokedaisy.empty())
	{
		std::ifstream f(pokedaisy + "/app/src/main/kotlin/com/pokedaisy/app/companion/data/GuideGen1Gen.kt");
		const std::string table = std::string(d.key == "red" ? "G1R" : d.key == "blue" ? "G1B" : "G1Y") + "_AREAS_RAW";
		std::set<std::tuple<int, int, int>> theirs, ours;
		bool in = false;
		for (std::string l; std::getline(f, l);)
		{
			if (l.find("val " + table) != std::string::npos) in = true;
			else if (in && l == ")") break;
			int map, item, flag;
			if (in && sscanf(l.c_str(), "%d|HIDDEN|%d|%d|", &map, &item, &flag) == 3) theirs.insert({ map, flag - 0x20000, gen1Item((uint8_t)item) });
		}
		// (and one more: the item left on UNUSED_MAP_6F, a map the game never loads, which PokeDaisy's generator leaves
		// out; the Here page lists a map's own, so it never shows)
		const int UNUSED_MAP_6F = 0x6F;
		int unused = 0;
		for (const HiddenItem& h : hidden)
			if (h.zone == UNUSED_MAP_6F) unused++;
			else ours.insert({ h.zone, h.flag, h.item });
		CHECK(theirs.size() == (d.key == "yellow" ? 54u : 53u) && unused == 1 && ours.size() + 1 == hidden.size() && ours == theirs);
		fprintf(stderr, "gbpk-roms: %s: the %zu hidden items are PokeDaisy's, by map, flag and item, and one on a map the game never loads\n",
			current.c_str(), theirs.size());
	}
	if (d.game.id == GOLD || d.game.id == SILVER)
	{
		// Gold's and Silver's, from the ROM's trainers (gsguide.h): Johto's eight leaders, the Elite Four, Lance, then
		// Kanto's eight, Falkner first; each leader by its gym's badge (Jasmine's MINERALBADGE is bit 4, Pryce's
		// GLACIERBADGE bit 6, Blue's EARTHBADGE bit 7 of Kanto's), the Elite Four and the Champion by a later one
		const nds::GuideData g = romGuide(r, d);
		std::string order, badges;
		std::map<std::string, std::string> doneWhen;
		for (const nds::Milestone& m : g.milestones)
		{
			order += (order.empty() ? "" : ", ") + m.foeName;
			doneWhen[m.foeName] = m.doneWhen;
			if (m.kind == "gym") badges += m.doneWhen.compare(0, 6, "badge:") == 0 ? "" : " " + m.foeName;
		}
		EQ(order, "Falkner, Bugsy, Whitney, Morty, Chuck, Pryce, Jasmine, Clair, Will, Koga, Bruno, Karen, Lance, "
			"Janine, Brock, Lt.Surge, Erika, Misty, Sabrina, Blaine, Blue");
		EQ(badges, "");
		EQ(doneWhen["Falkner"], "badge:1");
		EQ(doneWhen["Chuck"], "badge:6");
		EQ(doneWhen["Jasmine"], "badge:5");
		EQ(doneWhen["Pryce"], "badge:7");
		EQ(doneWhen["Clair"], "badge:8");
		EQ(doneWhen["Brock"], "badge:9");
		EQ(doneWhen["Blue"], "badge:16");
		EQ(doneWhen["Will"], "later");
		EQ(doneWhen["Lance"], "later");
		fprintf(stderr, "gbpk-roms: %s: the guide from the ROM's trainers, %zu milestones: %s\n", current.c_str(),
			g.milestones.size(), order.c_str());
		return;
	}
	const std::string path = dataDir() + "/" + d.key + ".json";
	if (FILE* f = fopen(path.c_str(), "r")) fclose(f);
	else return;
	nds::GuideData g;
	std::string why;
	const nds::GuideResolver res = nds::guideResolver(r);
	CHECK(nds::loadGuideFile(path, g, d.game.id, &why, &res));
	EQ(why, "");
	std::string unresolved;
	for (const std::string& p : g.unresolved) unresolved += (unresolved.empty() ? "" : ", ") + p;
	EQ(unresolved, "");
	for (const nds::Milestone& m : g.milestones)
	{
		CHECK(m.trainerIds.size() == (d.gen == 1 && m.kind == "champion" ? 3u : 1u));
		for (size_t i = 0; i < std::max<size_t>(1, m.trainerIds.size()); i++)
		{
			auto t = i < m.trainerIds.size() ? r.trainers.find(m.trainerIds[i]) : r.trainers.end();
			int top = 0;
			if (t != r.trainers.end()) for (const nds::TrainerMon& tm : t->second.team) top = std::max<int>(top, tm.level);
			if (t == r.trainers.end() || top != m.levelCap)
			{
				failed++;
				fprintf(stderr, "gbpk-roms: FAIL %s: %s: the ROM's trainer %d has top level %d, not %d\n", current.c_str(), m.id.c_str(),
					i < m.trainerIds.size() ? m.trainerIds[i] : -1, top, m.levelCap);
			}
			checks++;
		}
	}
	fprintf(stderr, "gbpk-roms: %s: %s.json's %zu milestones, their places and teams, found in the ROM\n", current.c_str(),
		d.key.c_str(), g.milestones.size());
}

int main(int argc, char** argv)
{
	if (argc < 4)
	{
		fprintf(stderr, "usage: gbpk-roms <the data files' folder> <roms.sha1> <the ROMs' folder> [<pokedaisy checkout>]\n");
		return 2;
	}
	setenv("KIWAMI_GBPK_DIR", argv[1], 1);
	const std::string dir = argv[3], pokedaisy = argc > 4 ? argv[4] : "";
	std::set<std::string> listed;
	std::ifstream lf(argv[2]);
	for (std::string l; std::getline(lf, l);) if (l.size() > 40 && l[0] != '#') listed.insert(l.substr(0, 40));
	std::vector<std::string> files;
	if (DIR* dp = opendir(dir.c_str()))
	{
		while (dirent* e = readdir(dp))
		{
			std::string n = e->d_name;
			std::string low = n;
			for (char& ch : low) ch = (char)tolower((unsigned char)ch);
			if ((low.size() > 3 && low.compare(low.size() - 3, 3, ".gb") == 0) || (low.size() > 4 && low.compare(low.size() - 4, 4, ".gbc") == 0)) files.push_back(n);
		}
		closedir(dp);
	}
	std::sort(files.begin(), files.end());
	int read = 0;
	for (const std::string& n : files)
	{
		current = n;
		auto rom = std::make_shared<std::vector<uint8_t>>(readRomFile(dir + "/" + n, 8u << 20));
		std::string key, why;
		GameData d;
		if (detect(rom->data(), rom->size(), key) != Match::KNOWN || !loadData(key, d, &why) || confirm(*rom, d) != Match::KNOWN)
		{
			fprintf(stderr, "gbpk-roms: %s: not a release the data files list, left alone\n", n.c_str());
			continue;
		}
		CHECK(listed.count(d.game.romHash));
		auto t0 = std::chrono::steady_clock::now();
		auto r = readRom(rom, d, &why);
		double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
		fprintf(stderr, "gbpk-roms: %s: %s (%s), read in %.1f ms%s%s\n", n.c_str(), d.game.title.c_str(), d.releases[d.release].name.c_str(),
			ms, r ? "" : ": ", r ? "" : why.c_str());
		CHECK(r);
		if (!r) continue;
		read++;
		const char* out = getenv("GBPK_ROMS_OUT");
		if (d.gen == 1)
		{
			checkGen1(*r, d);
			checkGuide(*r, d, pokedaisy);
			checkPics(*r, *rom, out ? out : "", key);
			checkTownMap(*r, out ? out : "", key);
			continue;
		}
		checkGen2(*r, key, pokedaisy);
		checkGuide(*r, d, pokedaisy);
		checkPics(*r, *rom, out ? out : "", key);
		checkMaps(*r, out ? out : "", key);
	}
	if (failed)
	{
		fprintf(stderr, "gbpk-roms: %d of %d checks failed\n", failed, checks);
		return 1;
	}
	printf("gbpk-roms: %d ROMs read, %d checks passed\n", read, checks);
	return 0;
}
