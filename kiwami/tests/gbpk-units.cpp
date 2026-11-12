// The Game Boy Pokémon companion's code that needs no screen (ui/src/kiwami/gbpk/), checked on its own: which game a
// ROM is, its data files and the list of ROMs its ROM tests expect (kiwami/tests/gbpk/roms.sha1), its text, its ROM
// reader on a made-up ROM, its memory reader replayed over PokeDaisy's RAM captures (kiwami/tests/gbpk/fixtures/), and
// its guides through upstream's loader and logic.
// Built and run by kiwami/tests/ci.sh with the host's g++ (the vendored C first, make -C ui kiwami-c, for the 7z
// reader):
//   g++ -std=c++17 -Wall -Werror -I ui/src kiwami/tests/gbpk-units.cpp <ui/src/kiwami/gbpk/*.cpp but game.cpp, host.cpp
//       and panel.cpp> ui/src/kiwami/json.cpp
//       ui/src/kiwami/ra7z.cpp ui/src/zip.cpp ui/src/zipdir.cpp ui/src/nds/nitro.cpp ui/src/nds/model.cpp
//       ui/src/nds/logic.cpp ui/src/nds/guide.cpp ui/src/json.cpp ui/src/pokemon.cpp ui/src/pokemap.cpp
//       ui/src/kiwami/third_party/lzma/*.kiwami.o -lz -lpthread -o gbpk-units
// The rules' expected values are worked by hand, in the comments, from the games' own code in pret's disassemblies.
//   ./gbpk-units kiwami/rootfs/usr/share/banana/companions/gb-pokemon kiwami/tests/gbpk
// Copyright (C) 2026 Lidor Itzhari (PokeDaisy) and the kiwami fork's contributors. GNU GPL version 3 with PokeDaisy's
// additional term under section 7(b): LICENSE and NOTICE in ui/src/kiwami/gbpk/. SPDX-License-Identifier: GPL-3.0-only
// Based on PokeDaisy by Lidor Itzhari - https://github.com/lidor30/pokedaisy
#include "kiwami/gbpk/games.h"
#include "kiwami/gbpk/gsguide.h"
#include "kiwami/gbpk/maps.h"
#include "kiwami/gbpk/pics.h"
#include "kiwami/gbpk/reader.h"
#include "kiwami/gbpk/rom.h"
#include "kiwami/gbpk/rules.h"
#include "kiwami/gbpk/text.h"
#include "nds/guide.h"
#include "nds/logic.h"
#include "nds/rom.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <unistd.h>
#include <vector>

using namespace banana;
using namespace banana::kiwami::gbpk;

static int checks = 0, failed = 0;
#define CHECK(c) do { checks++; if (!(c)) { failed++; fprintf(stderr, "gbpk-units: FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)
#define EQ(a, b) do { checks++; std::string a_ = (a), b_ = (b); if (a_ != b_) { failed++; fprintf(stderr, "gbpk-units: FAIL %s:%d: %s is \"%s\", not \"%s\"\n", __FILE__, __LINE__, #a, a_.c_str(), b_.c_str()); } } while (0)

static std::string dataPath, testDir;

// a made-up cartridge header (0x150 bytes): title at 0x134, then a game code at 0x13F when given
static std::vector<uint8_t> header(const char* title, const char* code = nullptr)
{
	std::vector<uint8_t> h(0x150, 0);
	memcpy(h.data() + 0x134, title, strlen(title));
	if (code) memcpy(h.data() + 0x13F, code, 4);
	return h;
}

static void testDetect()
{
	std::string key;
	auto h = header("PM_CRYSTAL", "BYTE");
	CHECK(detect(h.data(), h.size(), key) == Match::KNOWN);
	EQ(key, "crystal");
	// Gold's and Silver's eleven letters fill the title, the code right after
	h = header("POKEMON_GLD", "AAUE");
	CHECK(detect(h.data(), h.size(), key) == Match::KNOWN);
	EQ(key, "gold");
	h = header("POKEMON_SLV", "AAXE");
	CHECK(detect(h.data(), h.size(), key) == Match::KNOWN);
	EQ(key, "silver");
	// Red's, Blue's and Yellow's titles: no code, the rest of the sixteen bytes 0
	h = header("POKEMON RED");
	CHECK(detect(h.data(), h.size(), key) == Match::KNOWN);
	EQ(key, "red");
	h = header("POKEMON BLUE");
	CHECK(detect(h.data(), h.size(), key) == Match::KNOWN);
	EQ(key, "blue");
	h = header("POKEMON YELLOW");
	CHECK(detect(h.data(), h.size(), key) == Match::KNOWN);
	EQ(key, "yellow");
	h = header("POKEMON REDX");
	CHECK(detect(h.data(), h.size(), key) == Match::NONE && key.empty());
	h = header("TETRIS");
	CHECK(detect(h.data(), h.size(), key) == Match::NONE);
	// a longer title that starts with a known one isn't it; a header cut short isn't read
	h = header("PM_CRYSTALX");
	CHECK(detect(h.data(), h.size(), key) == Match::NONE);
	h = header("PM_CRYSTAL", "BYTE");
	CHECK(detect(h.data(), 0x14F, key) == Match::NONE);
	CHECK(detect(nullptr, 0, key) == Match::NONE);
}

static void testData()
{
	GameData d;
	std::string why;
	CHECK(loadData("crystal", d, &why));
	EQ(why, "");
	EQ(d.key, "crystal");
	EQ(d.headerTitle, "PM_CRYSTAL");
	CHECK(d.gen == 2 && d.game.gen == GEN_II && d.game.id == CRYSTAL && d.game.dexSize == 251);
	EQ(d.game.title, "Crystal");
	CHECK(d.at("partyCount").bank == 1 && d.at("partyCount").addr == 0xDCD7);
	CHECK(d.at("battleMon").bank == 0 && d.at("battleMon").addr == 0xC62C);
	CHECK(!d.at("noSuchThing").ok());
	CHECK(d.size("itemsMax") == 20 && d.size("ballsMax") == 12 && d.size("keyItemsMax") == 25);
	CHECK(d.size("eventFlagBytes") == 256 && d.size("none", 7) == 7);
	CHECK(d.releases.size() == 3);
	bool au = false;
	for (const auto& r : d.releases) if (r.name == "Australia") au = r.code == "BYTU";
	CHECK(au);
	// the address READ_CORE_MEMORY takes: banks 0 and 1 as they are, 2-7 from 0x10000
	CHECK(d.at("partyCount").core() == 0xDCD7);
	CHECK((Addr{ 2, 0xD000 }.core() == 0x10000 && Addr{ 7, 0xDFFF }.core() == 0x15FFF));

	CHECK(loadData("gold", d, &why) && d.game.id == GOLD && d.at("partyCount").addr == 0xDA22);
	CHECK(d.releases.size() == 1 && d.releases[0].code == "AAUE");
	CHECK(loadData("silver", d, &why) && d.game.id == SILVER && d.releases[0].code == "AAXE");
	EQ(d.game.title, "Silver");

	// the guide's flags: Crystal's event flags from bit 0, then the derived bits (the Elite Four's: the League won or
	// the room's flag; Red's: the League won and EVENT_RED_IN_MT_SILVER); Gold's none yet
	CHECK(loadData("crystal", d, &why));
	CHECK(d.flags.size() == 1 && d.flags[0].ram == "eventFlags" && d.flags[0].bytes == 256 && d.flags[0].first == 0);
	CHECK(d.derived.size() == 5 && d.derived[0].bit == 2048 && !d.derived[0].all && (d.derived[0].of == std::vector<int>{ 68, 1464 }));
	CHECK(d.derived[4].bit == 2052 && d.derived[4].all && (d.derived[4].of == std::vector<int>{ 68, 1890 }));
	CHECK(loadData("gold", d, &why) && d.flags.empty() && d.derived.empty());

	// Gen I's: no WRAM banks, so D000-DFFF is bank 1 as the cores map it; Red's and Blue's addresses one byte after
	// Yellow's; Fast Red and Fast Blue releases of Red and Blue; Yellow's starter Pikachu by its internal index
	CHECK(loadData("red", d, &why) && d.gen == 1 && d.game.gen == GEN_I && d.game.id == RED && d.game.dexSize == 151);
	EQ(d.headerTitle, "POKEMON RED");
	CHECK(d.at("partyCount").bank == 1 && d.at("partyCount").addr == 0xD163 && d.at("enemyMon").bank == 0);
	CHECK(d.releases.size() == 2 && d.releases[0].code.empty());
	EQ(d.releases[1].name, "Fast Red (v1.0)");
	CHECK(d.size("bagSlots") == 20 && d.size("starterPikachu", -1) == -1 && !d.at("pikachuHappiness").ok());
	CHECK(loadData("blue", d, &why) && d.game.id == BLUE && d.at("partyCount").addr == 0xD163 && d.releases.size() == 2);
	CHECK(loadData("yellow", d, &why) && d.game.id == YELLOW && d.at("partyCount").addr == 0xD162 && d.releases.size() == 1);
	CHECK(d.size("starterPikachu") == 0x54 && d.at("pikachuHappiness").addr == 0xD46F);
	EQ(d.game.title, "Yellow");

	// refusals: a game it doesn't know, a missing file (a folder without the data files)
	CHECK(!loadData("pinball", d, &why));
	CHECK(why.find("not one of the six") != std::string::npos);
	setenv("KIWAMI_GBPK_DIR", testDir.c_str(), 1);
	CHECK(!loadData("red", d, &why));
	CHECK(why.find("can't be read") != std::string::npos);
	setenv("KIWAMI_GBPK_DIR", dataPath.c_str(), 1);

	// ... and flags it can't lay out: Crystal's file with one thing changed, in a folder of its own
	auto changed = [&](const char* from, const char* to) {
		std::ifstream in(dataPath + "/crystal.ram.json");
		std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
		const size_t at = text.find(from);
		CHECK(at != std::string::npos);
		if (at == std::string::npos) return std::string();
		text.replace(at, strlen(from), to);
		char dir[] = "/tmp/gbpk-units.XXXXXX";
		if (!mkdtemp(dir)) return std::string();
		const std::string path = std::string(dir) + "/crystal.ram.json";
		std::ofstream(path) << text;
		setenv("KIWAMI_GBPK_DIR", dir, 1);
		GameData x;
		std::string w;
		CHECK(!loadData("crystal", x, &w));
		setenv("KIWAMI_GBPK_DIR", dataPath.c_str(), 1);
		unlink(path.c_str());
		rmdir(dir);
		return w;
	};
	CHECK(changed("\"ram\": \"eventFlags\"", "\"ram\": \"noSuchKey\"").find("a flag bank") != std::string::npos);
	CHECK(changed("\"first\": 0", "\"first\": 4").find("a flag bank") != std::string::npos);
	CHECK(changed("\"bit\": 2048", "\"bit\": 100").find("derived flag") != std::string::npos);
	CHECK(changed("\"bit\": 2049", "\"bit\": 2048").find("derived flag") != std::string::npos);
	CHECK(changed("1890", "3000").find("derived flag") != std::string::npos);
}

static void testConfirm()
{
	GameData d;
	CHECK(loadData("crystal", d, nullptr));
	// a ROM with Crystal's title and another SHA-1 (a European release, a hack): not supported, its hash kept for the log
	std::vector<uint8_t> rom = readRomFile(testDir + "/made-up/rom.gbc", 8u << 20);
	CHECK(rom.size() == 0x8000);
	std::string key;
	CHECK(detect(rom.data(), rom.size(), key) == Match::KNOWN);
	EQ(key, "crystal");
	CHECK(confirm(rom, d) == Match::UNSUPPORTED);
	CHECK(d.release == -1 && d.game.romHash.size() == 40 && d.game.code.empty());
	// with its SHA-1 listed (a stand-in for a release), it's that release, and the code is the file's
	d.releases.push_back({ d.game.romHash, "Made-up", "BYTE" });
	CHECK(confirm(rom, d) == Match::KNOWN);
	CHECK(d.release == 3);
	EQ(d.game.code, "BYTE");
	CHECK(confirm({}, d) == Match::UNSUPPORTED);
}

static void testRomFile()
{
	std::string m = testDir + "/made-up";
	std::vector<uint8_t> whole = readRomFile(m + "/rom.gbc", 8u << 20);
	// the header only
	std::vector<uint8_t> h = readRomFile(m + "/rom.gbc", 0x150);
	CHECK(h.size() == 0x150 && memcmp(h.data(), whole.data(), 0x150) == 0);
	// the .gbc in a .zip, past a readme
	std::vector<uint8_t> z = readRomFile(m + "/game.zip", 8u << 20);
	CHECK(z == whole);
	CHECK(readRomFile(m + "/game.zip", 0x150) == h);
	// in a solid .7z, the first game in name order: other.gb, before rom.gbc
	std::vector<uint8_t> s = readRomFile(m + "/game.7z", 0x150);
	std::string key;
	CHECK(s.size() == 0x150 && memcmp(s.data() + 0x134, "TETRIS", 6) == 0);
	CHECK(detect(s.data(), s.size(), key) == Match::NONE);
	// no game: an archive with none, a missing file
	CHECK(readRomFile(testDir + "/../ra/readme.zip", 0x150).empty());
	CHECK(readRomFile(testDir + "/../ra/readme.7z", 0x150).empty());
	CHECK(readRomFile(m + "/missing.gbc", 0x150).empty());
}

static void testText()
{
	// CHRIS, ended by 0x50, with what follows left alone
	const uint8_t chris[] = { 0x82, 0x87, 0x91, 0x88, 0x92, 0x50, 0x80, 0x80 };
	EQ(decode(2, chris, sizeof chris), "CHRIS");
	EQ(decode(2, chris, 3), "CHR");
	// the PK and MN ligatures, the contractions and the signs
	const uint8_t lig[] = { 0xE1, 0xE2, 0x7F, 0x8C, 0x91, 0xE8, 0x7F, 0x8C, 0x88, 0x8C, 0x84, 0x50 };
	EQ(decode(2, lig, sizeof lig), "PKMN MR. MIME");
	const uint8_t sign[] = { 0x8D, 0x88, 0x83, 0x8E, 0x91, 0x80, 0x8D, 0xF5, 0x7F, 0x85, 0x80, 0x91, 0x85, 0x84, 0x93, 0x82, 0x87, 0xD6, 0xE7, 0x50 };
	EQ(decode(2, sign, sizeof sign), "NIDORAN♀ FARFETCH'v!");
	const uint8_t misc[] = { 0x54, 0x7F, 0x81, 0x80, 0x8B, 0x8B, 0x7F, 0xF6, 0xFF, 0xE3, 0xEF, 0x05, 0x50 };
	EQ(decode(2, misc, sizeof misc), "POKé BALL 09-♂");	// (0x05, a Japanese kana, left out)
	EQ(decode(2, nullptr, 4), "");
	// Gen I's own: é and the contractions at BA-BF, 'r and 'm at E4-E5, the single quotes; Gen II's umlauts and its 'd
	// at D0 are nothing there
	const uint8_t g1[] = { 0x70, 0x8F, 0x8E, 0x8A, 0xBA, 0x8C, 0x8E, 0x8D, 0x71, 0x7F, 0xE4, 0xE5, 0xBD, 0xBB, 0xC0, 0xD0, 0x50 };
	EQ(decode(1, g1, sizeof g1), "‘POKéMON’ 'r'm's'd");
	EQ(decode(2, g1, sizeof g1), "POKMON Ä'd");

	EQ(nameCase("CHIKORITA"), "Chikorita");
	EQ(nameCase("DOUBLE-EDGE"), "Double-Edge");
	EQ(nameCase("MR.MIME"), "Mr.Mime");
	EQ(nameCase("FARFETCH'D"), "Farfetch'd");
	EQ(nameCase("HI JUMP KICK"), "Hi Jump Kick");
	EQ(nameCase("NIDORAN♀"), "Nidoran♀");
	EQ(nameCase("PORYGON2"), "Porygon2");
	// what stays as it is: HP, PP, PKMN, GS and a TM with its number
	EQ(nameCase("HP UP"), "HP Up");
	EQ(nameCase("PP MAX"), "PP Max");
	EQ(nameCase("GS BALL"), "GS Ball");
	EQ(nameCase("PKMN TRAINER"), "PKMN Trainer");
	EQ(nameCase("TM01"), "TM01");
	EQ(nameCase("S.S.TICKET"), "S.S.Ticket");
	// a place's breakable spaces (<BSP> 0x1F) and a description's line breaks (<NEXT> 0x4E) are spaces
	const uint8_t bsp[] = { 0x8D, 0x84, 0x96, 0x1F, 0x81, 0x80, 0x91, 0x8A, 0x1F, 0x93, 0x8E, 0x96, 0x8D, 0x50 };
	EQ(nameCase(decode(2, bsp, sizeof bsp)), "New Bark Town");
	const uint8_t next[] = { 0x80, 0x7F, 0x87, 0x88, 0x93, 0x4E, 0x8C, 0x8E, 0x95, 0x84, 0x50 };
	EQ(decode(2, next, sizeof next), "A HIT MOVE");
}

// the list of ROMs the ROM tests expect (kiwami/tests/gbpk/roms.sha1, checked in the player's folders by gbpk-roms.sh)
// and the data files agree: a Game Boy line's SHA-1 is one of its game's releases, and every release of a game that
// has a data file is a line of the list under that game
static void testRomList()
{
	struct Line { std::string sha1, system, need, game, name; };
	std::vector<Line> lines;
	std::ifstream f(testDir + "/roms.sha1");
	std::string l;
	while (std::getline(f, l))
	{
		if (l.empty() || l[0] == '#') continue;
		Line x;
		size_t p = 0;
		for (std::string* s : { &x.sha1, &x.system, &x.need, &x.game })
		{
			size_t e = l.find(' ', p);
			if (e == std::string::npos) break;
			*s = l.substr(p, e - p);
			p = e + 1;
		}
		x.name = l.substr(p);
		CHECK(x.sha1.size() == 40 && x.sha1.find_first_not_of("0123456789abcdef") == std::string::npos);
		CHECK(x.need == "required" || x.need == "optional");
		CHECK(!x.game.empty() && !x.name.empty());
		lines.push_back(x);
	}
	int gb = 0, required = 0, games = 0, released = 0;
	for (const Line& x : lines)
	{
		CHECK(x.system == "gb" || x.system == "gba" || x.system == "nds");
		if (x.system != "gb") continue;
		gb++;
		required += x.need == "required";
		CHECK(x.name.size() > 4 && (x.name.compare(x.name.size() - 3, 3, ".gb") == 0 || x.name.compare(x.name.size() - 4, 4, ".gbc") == 0));
	}
	CHECK(lines.size() == 28 && gb == 10 && required == 6);
	// the ROM hacks read as the retail game they patch (roms.patches): each a Game Boy line, its retail ROM a required
	// one of the same game, its ranges in order and apart, inside the bank-0 code and the header (gbpk-roms.sh compares
	// the files themselves)
	std::ifstream pf(testDir + "/roms.patches");
	int hacks = 0;
	auto find = [&](const std::string& sha1) -> const Line* { for (const Line& x : lines) if (x.sha1 == sha1) return &x; return nullptr; };
	while (std::getline(pf, l))
	{
		if (l.empty() || l[0] == '#') continue;
		std::istringstream in(l);
		std::string hack, base, r;
		in >> hack >> base;
		const Line* h = find(hack);
		const Line* b = find(base);
		CHECK(h && b && h != b && h->system == "gb" && b->system == "gb" && h->game == b->game && b->need == "required");
		long last = -1;
		int ranges = 0;
		while (in >> r)
		{
			size_t dash = r.find('-');
			long lo = dash == std::string::npos ? -1 : strtol(r.substr(0, dash).c_str(), nullptr, 16);
			long hi = dash == std::string::npos ? -1 : strtol(r.substr(dash + 1).c_str(), nullptr, 16);
			CHECK(lo > last && hi > lo && hi <= 0x4000);
			last = hi;
			ranges++;
		}
		CHECK(ranges > 0);
		hacks++;
	}
	CHECK(hacks == 2);
	for (const char* key : { "red", "blue", "yellow", "gold", "silver", "crystal" })
	{
		GameData d;
		bool listed = false;
		for (const Line& x : lines) listed |= x.system == "gb" && x.game == key;
		CHECK(listed);
		CHECK(loadData(key, d, nullptr));
		games++;
		for (const Line& x : lines)
		{
			if (x.system != "gb" || x.game != key) continue;
			bool in = false;
			for (const auto& r : d.releases) in |= r.sha1 == x.sha1;
			if (!in) fprintf(stderr, "gbpk-units: roms.sha1's %s isn't one of %s's releases\n", x.name.c_str(), key);
			CHECK(in);
		}
		for (const auto& r : d.releases)
		{
			bool in = false;
			for (const Line& x : lines) in |= x.system == "gb" && x.game == key && x.sha1 == r.sha1;
			if (!in) fprintf(stderr, "gbpk-units: %s's release %s isn't in roms.sha1\n", key, r.name.c_str());
			CHECK(in);
			released++;
		}
	}
	CHECK(games == 6 && released == 10);
}

// ---- the ROM reader on a made-up ROM ----------------------------------------------------------------------------

// a made-up Gen II ROM, 64 KB, laid out by a made-up data file (every table in bank 1, 4000-7FFF): two species with
// their pictures and palettes, three moves, three items, four landmarks, one map group of two maps with wild Pokémon
// in the first (grass, water, three rods, a tree and a rock), one trainer class with two trainers, the Pokégear's two
// maps. The layouts are pret's (rom2.cpp, pics.cpp and maps.cpp name each).
struct MadeUpRom
{
	std::vector<uint8_t> b = std::vector<uint8_t>(0x10000, 0);
	uint16_t next = 0x4000;
	GameData d;
	uint16_t put(const std::vector<int>& v)
	{
		uint16_t a = next;
		for (int x : v) b[next++] = (uint8_t)x;
		return a;
	}
	void key(const char* k, uint16_t a) { d.rom[k] = Addr{ 1, a }; }
	// capitals, digits, spaces and . - as the games write them, '|' a line break (<NEXT>), '_' a breakable space
	// (<BSP>), then the terminator
	static std::vector<int> str(const char* t)
	{
		std::vector<int> v;
		for (const char* c = t; *c; c++)
			v.push_back(*c >= 'A' && *c <= 'Z' ? 0x80 + *c - 'A' : *c >= '0' && *c <= '9' ? 0xF6 + *c - '0' : *c == ' ' ? 0x7F :
				*c == '.' ? 0xE8 : *c == '-' ? 0xE3 : *c == '|' ? 0x4E : *c == '_' ? 0x1F : 0x7F);
		v.push_back(0x50);
		return v;
	}
	static std::vector<int> words(const std::vector<int>& ws)
	{
		std::vector<int> v;
		for (int w : ws) { v.push_back(w & 0xFF); v.push_back(w >> 8); }
		return v;
	}
	static std::vector<int> cat(std::vector<int> a, const std::vector<int>& c) { a.insert(a.end(), c.begin(), c.end()); return a; }
};

static std::shared_ptr<const std::vector<uint8_t>> madeUpRom(GameData& d, bool notBulbasaur = false)
{
	MadeUpRom m;
	using R = MadeUpRom;
	m.d.key = "crystal";
	m.d.gen = 2;
	m.d.game.id = CRYSTAL;
	m.d.game.gen = GEN_II;
	m.d.game.title = "Made-up";
	m.d.sizes = { { "numPokemon", 2 }, { "numMoves", 3 }, { "numTrainerClasses", 1 }, { "numLandmarks", 4 }, { "firstTm", 4 },
		{ "vitalThrow", 3 }, { "kantoLandmark", 2 }, { "fastShipLandmark", 3 } };
	// base data: species, HP .. Sp. Def, types (Grass 22, Poison 3), catch rate, base exp, items, gender, 3 bytes,
	// picture size (5 x 5, 6 x 6), 4 unused, growth, egg groups, TM/HM bits (TM01, and HM01: bit 50)
	std::vector<int> b1(32, 0), b2(32, 0);
	b1 = { 1, notBulbasaur ? 44 : 45, 49, 49, 45, 65, 65, 22, 3, 45, 64, 0, 0, 31, 0, 0, 0, 0x55, 0, 0, 0, 0, 3, 0, 0x01, 0, 0, 0, 0, 0, 0x04, 0 };
	b2 = { 2, 60, 62, 63, 60, 80, 80, 22, 22, 45, 141, 0, 0, 31, 0, 0, 0, 0x66, 0, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
	m.key("baseData", m.put(R::cat(b1, b2)));
	std::vector<int> n1 = R::str("BULBASAUR"), n2 = R::str("IVYSAUR");
	n1.resize(10, 0x50);
	n2.resize(10, 0x50);
	m.key("pokemonNames", m.put(R::cat(n1, n2)));
	// moves: animation (the id), effect, power, type, accuracy, PP, effect chance; Quick Fire (Fire, 20) with a priority
	// effect, Vital Throw by its id
	m.key("moves", m.put({ 1, 0, 40, 0, 255, 35, 0, 2, 0x67, 40, 20, 242, 30, 76, 3, 0x11, 70, 1, 255, 10, 0 }));
	m.key("moveNames", m.put(R::cat(R::cat(R::str("POUND"), R::str("QUICK FIRE")), R::str("VITAL THROW"))));
	const int md1 = m.put(R::str("A HIT|THAT POUNDS")), md2 = m.put(R::str("FIRST")), md3 = m.put(R::str("LAST"));
	m.key("moveDescriptions", m.put(R::words({ md1, md2, md3 })));
	m.key("moveEffectPriorities", m.put({ 0x67, 2, 0xFF }));
	// items: names, attributes (price, held effect, parameter, property, pocket: Ball 3, Item 1; menus), descriptions
	m.key("itemNames", m.put(R::cat(R::cat(R::str("MASTER BALL"), R::str("ULTRA BALL")), R::str("HP UP"))));
	m.key("itemAttributes", m.put({ 0, 0, 0, 0, 6, 3, 0, 0xB0, 0x04, 0, 0, 6, 3, 0, 0x48, 0x26, 0, 0, 0, 1, 0 }));
	const int id1 = m.put(R::str("THE BEST")), id2 = m.put(R::str("GOOD")), id3 = m.put(R::str("RAISES HP"));
	m.key("itemDescriptions", m.put(R::words({ id1, id2, id3 })));
	// evolutions then learnsets: Bulbasaur at 16 into 2, learns 1 at 1 and 2 at 7; 2 by its stats at 20 into 1
	const int e1 = m.put({ 1, 16, 2, 0, 1, 1, 7, 2, 0 }), e2 = m.put({ 5, 20, 1, 1, 0, 1, 1, 0 });
	m.key("evosAttacksPointers", m.put(R::words({ e1, e2 })));
	// TM01 teaches move 1, HM01 move 2
	std::vector<int> tm(58, 0);
	tm[0] = 1;
	tm[50] = 2;
	m.key("tmHmMoves", m.put(tm));
	// the Pokédex: category, height (2'04"), weight (15.0 lb), two pages; the entries' bank counted from bank 1
	const int x1 = m.put(R::cat(R::cat(R::cat(R::str("SEED"), { 204, 0, 150, 0 }), R::str("A PAGE|ONE")), R::str("TWO")));
	const int x2 = m.put(R::cat(R::cat(R::cat(R::str("SEED"), { 0x2C, 1, 0x1E, 1 }), R::str("P")), R::str("Q")));
	m.key("dexEntryPointers", m.put(R::words({ x1, x2 })));
	m.d.rom["dexEntryFirst"] = Addr{ 1, 0x4000 };
	// landmarks: x + 8, y + 16 (the point on the screen), the name's pointer: the special one, New Bark Town (at 92,
	// 84 on Johto's map), Pallet Town (Kanto's from 2, KANTO_LANDMARK; at 52, 108) and the Fast Ship (at 140, 116, on
	// Johto's)
	const int l0 = m.put(R::str("SPECIAL")), l1 = m.put(R::str("NEW_BARK_TOWN")), l2 = m.put(R::str("PALLET TOWN")),
		l3 = m.put(R::str("FAST SHIP"));
	m.key("landmarks", m.put({ 8, 16, l0 & 0xFF, l0 >> 8, 100, 100, l1 & 0xFF, l1 >> 8, 60, 124, l2 & 0xFF, l2 >> 8,
		148, 132, l3 & 0xFF, l3 >> 8 }));
	// map 1's attributes (border, height, width, the blocks' bank and pointer, the scripts' bank and pointer, the
	// events' pointer, connections) and its events in the scripts' bank: two filler bytes, a warp, no coord events, a
	// sign and a hidden item (y 5, x 9) whose hiddenitem is flag 0x123 and item 3, then no object events
	const int hid = m.put({ 0x23, 0x01, 3 });
	const int ev = m.put({ 0, 0, 1, 1, 2, 1, 0, 0, 0, 2, 3, 4, 0, 0, 0, 5, 9, 7, hid & 0xFF, hid >> 8, 0 });
	const int attr = m.put({ 0, 1, 1, 0, 0, 0, 1, 0, 0, ev & 0xFF, ev >> 8, 0 });
	// one map group, right after its pointer: map 1 at landmark 1 with fishing group 2 and its attributes in bank 1,
	// map 2 at landmark 0 with the Shore group, no water (no fishing) and no attributes; the headers end where the
	// next table starts
	const int mg = m.next;
	m.key("mapGroups", m.put(R::words({ mg + 2 })));
	m.put({ 1, 0, 2, attr & 0xFF, attr >> 8, 1, 0, 0, 2, 0, 0, 3, 0, 0, 0, 0, 0, 1 });
	m.key("mapHeadersEnd", m.next);
	// grass on map 1.1: map, three rates, morning's slots then day's and night's (level 3 + slot); morning's first is
	// species 1, the rest 2
	std::vector<int> g = { 1, 1, 10, 10, 10 };
	for (int t = 0; t < 3; t++) for (int j = 0; j < 7; j++) { g.push_back(3 + j); g.push_back(t == 0 && j == 0 ? 1 : 2); }
	g.push_back(0xFF);
	m.key("johtoGrass", m.put(g));
	m.key("kantoGrass", m.put({ 0xFF }));
	m.key("grassProbs", m.put({ 30, 0, 60, 2, 80, 4, 90, 6, 95, 8, 99, 10, 100, 12 }));
	m.key("waterProbs", m.put({ 60, 0, 90, 2, 100, 4 }));
	m.key("johtoWater", m.put({ 1, 1, 5, 10, 2, 15, 2, 20, 1, 0xFF }));
	m.key("kantoWater", m.put({ 0xFF }));
	// fishing groups 1 (Shore) and 2: the old rod species 1 Lv 5 at or under 0x7F, else 2 Lv 10; the good rod by the
	// time (TimeFishGroups' 0: 1 Lv 20 by day, 2 Lv 25 at night); the super rod 2 Lv 40
	const int oldRod = m.put({ 0x7F, 1, 5, 0xFF, 2, 10 }), goodRod = m.put({ 0xFF, 0, 0 }), superRod = m.put({ 0xFF, 2, 40 });
	m.key("fishGroups", m.put(R::cat({ 128 }, R::cat(R::words({ oldRod, oldRod, oldRod }), R::cat({ 128 }, R::words({ oldRod, goodRod, superRod }))))));
	m.key("timeFishGroups", m.put({ 1, 20, 2, 25 }));
	// Headbutt (set 1: the common trees' 1 and 2 Lv 10 at 50 % each, the rare trees' 2 Lv 12) and Rock Smash (set 2)
	const int set1 = m.put({ 50, 1, 10, 50, 2, 10, 0xFF, 100, 2, 12, 0xFF }), set2 = m.put({ 90, 1, 15, 10, 2, 15, 0xFF });
	m.key("treeMons", m.put(R::words({ set1, set1, set2 })));
	m.key("treeMonMaps", m.put({ 1, 1, 1, 0xFF }));
	m.key("rockMonMaps", m.put({ 1, 1, 2, 0xFF }));
	// trainers: one class, its group right after its pointer: Falkner with items and moves, then Bob with neither
	m.key("trainerClassNames", m.put(R::str("LEADER")));
	const int tg = m.next;
	m.key("trainerGroups", m.put(R::words({ tg + 2 })));
	m.put(R::cat(R::cat(R::str("FALKNER"), { 3, 7, 1, 0, 1, 2, 0, 0, 9, 2, 2, 3, 0, 0, 0, 0xFF }), R::cat(R::str("BOB"), { 0, 8, 1, 0xFF })));
	// the pictures: one LZ stream for both, 25 tiles (literal: tile 0 with pixels 1 at 0,0, 2 at 1,1 and 3 at 2,2;
	// tile 1, below it, 3 at 0,0; then a long zero fill of 368), too short for Ivysaur's 36. The stored bank is 1 less
	// PICS_FIX ($36), fixed by PicsBanks (its first, "Pics 1", bank 1). The palettes' first entry is the games' unused
	// one; Bulbasaur's normal colours red and green, its shiny ones blue and yellow
	std::vector<int> pic = { 0x1F, 0x80, 0, 0, 0x40, 0x20, 0x20 };
	pic.resize(17, 0);
	pic.push_back(0x80);
	pic.push_back(0x80);
	pic.resize(33, 0);
	pic.insert(pic.end(), { 0xED, 0x6F, 0xFF });
	const int lz = m.put(pic), stored = (1 - 0x36) & 0xFF;
	m.key("picPointers", m.put({ stored, lz & 0xFF, lz >> 8, 0, 0, 0, stored, lz & 0xFF, lz >> 8, 0, 0, 0 }));
	m.key("unownPicPointers", m.put({ 0, 0, 0, 0, 0, 0 }));
	std::vector<int> banks(24);
	for (int i = 0; i < 24; i++) banks[i] = 1 + i;
	m.key("picsBanks", m.put(banks));
	m.key("monPalettes", m.put({ 0xDE, 0x46, 0xD0, 0x4D, 0xDE, 0x46, 0xD0, 0x4D, 0x1F, 0, 0xE0, 0x03, 0, 0x7C, 0xFF, 0x03,
		0, 0, 0, 0, 0, 0, 0, 0 }));
	// the Pokégear: 48 tiles (0 blank, 1 all colour 3, 2 all colour 1 - LZ: zero 16, iterate 16, alternate 16, a long
	// zero fill of 720), tile 1 in palette 1 and tile 2 in palette 5 (the palette map's nybbles, the even id's low),
	// palette 0's colour 0 white, 1's colour 3 green, 5's colour 1 blue; Johto's map tiles 1 and 2 first on its top
	// row, Kanto's a tile past the 48 (no picture)
	m.key("townMapGfx", m.put({ 0x6F, 0x2F, 0xFF, 0x4F, 0xFF, 0x00, 0xEE, 0xCF, 0xFF }));
	std::vector<int> palMap(0x30, 0), pals(48, 0), johto(361, 0), kanto(361, 0);
	palMap[0] = 0x10;
	palMap[1] = 0x05;
	m.key("townMapPalMap", m.put(palMap));
	pals[0] = 0xFF; pals[1] = 0x7F;
	pals[8 + 6] = 0xE0; pals[8 + 7] = 0x03;
	pals[40 + 2] = 0x00; pals[40 + 3] = 0x7C;
	m.key("pokegearPals", m.put(pals));
	johto[0] = 1; johto[1] = 2; johto[360] = 0xFF;
	m.key("johtoMap", m.put(johto));
	kanto[5] = 48; kanto[360] = 0xFF;
	m.key("kantoMap", m.put(kanto));
	d = m.d;
	return std::make_shared<std::vector<uint8_t>>(m.b);
}

static void testRomTables()
{
	GameData d;
	std::string why;
	auto rom = madeUpRom(d);
	auto r = readRom(rom, d, &why);
	CHECK(r);
	EQ(why, "");
	if (!r) return;
	// species
	CHECK(r->species.size() == 3);
	const nds::Species& b = r->species[1];
	EQ(b.name, "Bulbasaur");
	EQ(r->species[2].name, "Ivysaur");
	CHECK((b.base[0] == 45 && b.base[1] == 49 && b.base[2] == 49 && b.base[3] == 45 && b.base[4] == 65 && b.base[5] == 65));
	CHECK(b.types[0] == nds::GRASS && b.types[1] == nds::POISON && b.catchRate == 45 && b.baseExp == 64 && b.genderRatio == 31 &&
		b.growth == 3 && b.baseFriendship == 70);
	CHECK((b.tms == std::vector<uint16_t>{ 1, 93 }));
	EQ(b.category, "Seed");
	CHECK(b.height == 7 && b.weight == 69);		// 28 in: 0.71 m; 15.0 lb: 75 - 4 - 2 (the game's reckoning)
	EQ(b.dexText, "A PAGE ONE TWO");
	CHECK(b.evolutions.size() == 1 && b.evolutions[0].method == nds::evo::LEVEL && b.evolutions[0].param == 16 && b.evolutions[0].into == 2);
	EQ(b.evolutions.empty() ? "" : b.evolutions[0].how, "Level 16");
	CHECK(b.learnset.size() == 2 && b.learnset[0].level == 1 && b.learnset[0].move == 1 && b.learnset[1].level == 7 && b.learnset[1].move == 2);
	const nds::Species& v = r->species[2];
	CHECK(v.evolutions.size() == 1 && v.evolutions[0].method == nds::evo::LEVEL_ATK_GT_DEF && v.evolutions[0].param == 20 && v.evolutions[0].into == 1);
	EQ(v.evolutions.empty() ? "" : v.evolutions[0].how, "Level 20 with Attack higher than Defense");
	CHECK(v.height == 9 && v.weight == 131 && v.types[0] == nds::GRASS && v.types[1] == nds::GRASS);
	// moves: the category by the type, the priorities (Quick Fire's effect +1, Vital Throw -1), the percents
	CHECK(r->moves.size() == 4);
	EQ(r->moves[1].name, "Pound");
	EQ(r->moves[1].description, "A HIT THAT POUNDS");
	CHECK(r->moves[1].power == 40 && r->moves[1].pp == 35 && r->moves[1].accuracy == 100 && r->moves[1].type == nds::NORMAL &&
		r->moves[1].cat == nds::MoveCategory::PHYSICAL && r->moves[1].priority == 0);
	EQ(r->moves[2].name, "Quick Fire");
	CHECK(r->moves[2].type == nds::FIRE && r->moves[2].cat == nds::MoveCategory::SPECIAL && (int8_t)r->moves[2].priority == 1 &&
		r->moves[2].accuracy == 95 && r->moves[2].effectChance == 30);
	CHECK(r->moves[3].type == nds::FIGHTING && (int8_t)r->moves[3].priority == -1 && r->moves[3].power == 70);
	// items at their Gen IV ids (Master and Ultra Ball) or GEN2_ONLY + theirs (HP Up), the TMs and HMs as Gen IV's
	EQ(r->items[1].name, "Master Ball");
	EQ(r->items[1].description, "THE BEST");
	CHECK(r->items[1].pocket == nds::POCKET_BALLS && r->items[2].price == 1200);
	EQ(r->items[GEN2_ONLY + 3].name, "HP Up");
	CHECK(r->items[GEN2_ONLY + 3].price == 9800 && r->items[GEN2_ONLY + 3].pocket == nds::POCKET_ITEMS);
	EQ(r->items[328].name, "TM01");
	EQ(r->items[420].name, "HM01");
	CHECK(r->tmMoves.size() == 100 && r->tmMoves[0] == 1 && r->tmMoves[92] == 2);
	// places
	CHECK((r->locationNames == std::vector<std::string>{ "Special", "New Bark Town", "Pallet Town", "Fast Ship" }));
	CHECK(r->zones.size() == 2 && r->zones.count(0x0101) && r->zones.count(0x0102));
	const nds::Location& z = r->zones.at(0x0101);
	EQ(z.name, "New Bark Town");
	EQ(r->zones.at(0x0102).name, "Special");
	CHECK(r->zones.at(0x0102).encounters.empty());
	// the hidden item on map 1 (the sign beside it isn't one; map 2 has no attributes, so no events)
	const std::vector<HiddenItem>& hidden = static_cast<const GbRomData&>(*r).hidden;
	CHECK(hidden.size() == 1);
	if (!hidden.empty())
	{
		CHECK(hidden[0].zone == 0x0101 && hidden[0].x == 9 && hidden[0].y == 5 && hidden[0].flag == 0x123 && hidden[0].item == gen2Item(3));
		EQ(r->items[hidden[0].item].name, "HP Up");
	}
	auto count = [&](nds::EncounterKind k) { int n = 0; for (const auto& e : z.encounters) n += e.kind == k; return n; };
	auto find = [&](nds::EncounterKind k, int species, nds::TimeOfDay t, int cond = 0) -> const nds::Encounter* {
		for (const auto& e : z.encounters) if (e.kind == k && e.species == species && e.time == t && e.condition == cond) return &e;
		return nullptr;
	};
	CHECK(count(nds::EncounterKind::GRASS) == 21);
	const nds::Encounter* e = find(nds::EncounterKind::GRASS, 1, nds::TimeOfDay::MORNING);
	CHECK(e && e->minLevel == 3 && e->maxLevel == 3 && e->percent == 30);
	CHECK(!find(nds::EncounterKind::GRASS, 1, nds::TimeOfDay::DAY));
	int pct = 0;
	for (const auto& x : z.encounters) if (x.kind == nds::EncounterKind::GRASS && x.time == nds::TimeOfDay::NIGHT) pct += x.percent;
	CHECK(pct == 100);
	// water: the slots' percents, levels + 0-4
	CHECK(count(nds::EncounterKind::SURF) == 3);
	e = find(nds::EncounterKind::SURF, 1, nds::TimeOfDay::ANY);
	CHECK(e && e->minLevel == 20 && e->maxLevel == 24 && e->percent == 10);
	// fishing: the old rod's two halves, the good rod by the time, the super rod
	e = find(nds::EncounterKind::OLD_ROD, 1, nds::TimeOfDay::ANY);
	CHECK(e && e->minLevel == 5 && e->percent == 50);
	e = find(nds::EncounterKind::OLD_ROD, 2, nds::TimeOfDay::ANY);
	CHECK(e && e->minLevel == 10 && e->percent == 50);
	CHECK(find(nds::EncounterKind::GOOD_ROD, 1, nds::TimeOfDay::MORNING) && find(nds::EncounterKind::GOOD_ROD, 1, nds::TimeOfDay::DAY));
	e = find(nds::EncounterKind::GOOD_ROD, 2, nds::TimeOfDay::NIGHT);
	CHECK(e && e->minLevel == 25 && e->percent == 100 && count(nds::EncounterKind::GOOD_ROD) == 3);
	e = find(nds::EncounterKind::SUPER_ROD, 2, nds::TimeOfDay::ANY);
	CHECK(e && e->minLevel == 40);
	// Headbutt's common and rare trees, Rock Smash
	CHECK(count(nds::EncounterKind::HEADBUTT) == 3 && find(nds::EncounterKind::HEADBUTT, 1, nds::TimeOfDay::ANY, 0));
	e = find(nds::EncounterKind::HEADBUTT, 2, nds::TimeOfDay::ANY, 1);
	CHECK(e && e->minLevel == 12 && e->percent == 100);
	e = find(nds::EncounterKind::ROCK_SMASH, 1, nds::TimeOfDay::ANY);
	CHECK(e && e->percent == 90 && e->minLevel == 15 && count(nds::EncounterKind::ROCK_SMASH) == 2);
	// trainers by class << 8 | number: Falkner's items and moves, Bob's moves from the learnset at his level
	CHECK(r->trainers.size() == 2 && r->trainers.count(0x0101) && r->trainers.count(0x0102));
	if (r->trainers.size() == 2)
	{
		const nds::Trainer& f = r->trainers.at(0x0101);
		EQ(f.name, "Falkner");
		EQ(f.cls, "Leader");
		CHECK(f.team.size() == 2 && f.team[0].level == 7 && f.team[0].species == 1 && f.team[0].item == 0 && f.team[0].moves[0] == 1 &&
			f.team[0].moves[1] == 2 && f.team[1].item == 2 && f.team[1].moves[0] == 3);
		const nds::Trainer& bob = r->trainers.at(0x0102);
		EQ(bob.name, "Bob");
		CHECK(bob.team.size() == 1 && bob.team[0].level == 8 && bob.team[0].moves[0] == 1 && bob.team[0].moves[1] == 2 && bob.team[0].moves[2] == 0);
	}

	// the pictures: Bulbasaur's 5 x 5 tiles in columns, its palettes white, the two colours, black; Ivysaur's stream is
	// too short for its 6 x 6, no species 3
	auto bulbasaur = r->sprite(1);
	CHECK(bulbasaur && bulbasaur->w == 40 && bulbasaur->h == 40 && bulbasaur->frames.size() == 1 && bulbasaur->anim.empty());
	if (bulbasaur && bulbasaur->frames.size() == 1 && bulbasaur->frames[0].size() == 1600)
	{
		const auto& f = bulbasaur->frames[0];
		CHECK(f[0] == 1 && f[41] == 2 && f[82] == 3 && f[8 * 40] == 3);
		int drawn = 0;
		for (uint8_t v : f) drawn += v != 0;
		CHECK(drawn == 4);
		CHECK(bulbasaur->normal[0] == 0xFFFFFF && bulbasaur->normal[1] == 0xFF0000 && bulbasaur->normal[2] == 0x00FF00 && bulbasaur->normal[3] == 0);
		CHECK(bulbasaur->shiny[0] == 0xFFFFFF && bulbasaur->shiny[1] == 0x0000FF && bulbasaur->shiny[2] == 0xFFFF00 && bulbasaur->shiny[3] == 0);
	}
	CHECK(!r->sprite(2) && !r->sprite(3) && !r->sprite(0));
	// Gold's and Silver's bank fix: a list of {stored, real} pairs to -1; a bank not in it is the stored one
	{
		std::vector<uint8_t> g = *rom;
		Gen2Pics t = static_cast<const GbRomData&>(*r).pics;
		t.bankPairs = true;
		t.banks = 0xF000;
		g[t.pointers] = 0x13;
		const uint8_t pairs[] = { 0x20, 9, 0x13, 1, 0xFF };
		memcpy(&g[0xF000], pairs, sizeof pairs);
		CHECK(gen2FrontPic(RomView{ g }, t, 1));
		g[0xF002] = 0xFF;
		CHECK(!gen2FrontPic(RomView{ g }, t, 1));
	}

	// the Pokégear's maps: Johto's picture (tile 1 green, tile 2 blue, tile 0 white), Kanto's none (a tile past the 48);
	// the cells a cell from a landmark's point name it (New Bark Town's 3 x 3 about 92, 84; Pallet Town's on Kanto's
	// about 52, 108; the Fast Ship's on Johto's about 140, 116); the places' spots: the player's 16 x 16 icon about
	// their landmark's point, on its map; the special landmark's on none
	CHECK(r->regionMaps.size() == 2);
	if (r->regionMaps.size() == 2)
	{
		const nds::RegionMap& j = r->regionMaps[0];
		const nds::RegionMap& k = r->regionMaps[1];
		EQ(j.name, "Johto");
		EQ(k.name, "Kanto");
		CHECK(j.w == 160 && j.h == 144 && j.cell == 8 && j.gridW == 20 && j.gridH == 18 && j.cellX == 0 && j.cellY == 0);
		CHECK(j.rgba.size() == 160 * 144 && k.rgba.empty());
		if (j.rgba.size() == 160 * 144)
			CHECK(j.rgba[0] == 0xFF00FF00u && j.rgba[7 * 160 + 7] == 0xFF00FF00u && j.rgba[8] == 0xFF0000FFu && j.rgba[16] == 0xFFFFFFFFu &&
				j.rgba[143 * 160 + 159] == 0xFFFFFFFFu);
		auto cell = [](const nds::RegionMap& m, int x, int y) { return m.cells[(size_t)y * m.gridW + x]; };
		CHECK(j.cells.size() == 360 && k.cells.size() == 360);
		CHECK(cell(j, 11, 10) == 1 && cell(j, 10, 9) == 1 && cell(j, 12, 11) == 1 && cell(j, 13, 10) == 0xFFFF && cell(j, 11, 8) == 0xFFFF);
		CHECK(cell(j, 17, 14) == 3 && cell(k, 17, 14) == 0xFFFF);
		CHECK(cell(k, 6, 13) == 2 && cell(j, 6, 13) == 0xFFFF && cell(k, 11, 10) == 0xFFFF);
		int named = 0;
		for (uint16_t c : j.cells) named += c != 0xFFFF;
		CHECK(named == 18);	// (New Bark Town's 9, the Fast Ship's 9)
	}
	const nds::MapSpot& s1 = r->zones.at(0x0101).spot;
	CHECK(s1.map == 0 && s1.x == 84 && s1.y == 76 && s1.w == 16 && s1.h == 16 && s1.tilesW == 0);
	CHECK(r->zones.at(0x0102).spot.map == -1);

	// refusals: a first species that isn't Bulbasaur, a table the data file doesn't give, no ROM, Generation I
	rom = madeUpRom(d, true);
	CHECK(!readRom(rom, d, &why));
	CHECK(why.find("Bulbasaur") != std::string::npos);
	rom = madeUpRom(d);
	d.rom.erase("trainerGroups");
	CHECK(!readRom(rom, d, &why));
	CHECK(why.find("rom.trainerGroups") != std::string::npos);
	CHECK(!readRom(nullptr, d, &why));
	rom = madeUpRom(d);
	d.gen = 1;
	CHECK(!readRom(rom, d, &why));
	// ... the palettes' first entry isn't the games', Bulbasaur's picture doesn't decode, the Pokégear's landmarks
	// aren't the landmarks'
	rom = madeUpRom(d);
	auto bad = std::make_shared<std::vector<uint8_t>>(*rom);
	(*bad)[RomView{ *bad }.at(d.rom.at("monPalettes"))] ^= 1;
	CHECK(!readRom(bad, d, &why));
	CHECK(why.find("PokemonPalettes") != std::string::npos);
	bad = std::make_shared<std::vector<uint8_t>>(*rom);
	(*bad)[RomView{ *bad }.at(d.rom.at("picPointers"))] = 0;
	CHECK(!readRom(bad, d, &why));
	CHECK(why.find("Bulbasaur's front picture") != std::string::npos);
	d.sizes["kantoLandmark"] = 4;
	CHECK(!readRom(rom, d, &why));
	CHECK(why.find("kantoLandmark") != std::string::npos);
	rom = madeUpRom(d);
	d.rom.erase("johtoMap");
	CHECK(!readRom(rom, d, &why));
	CHECK(why.find("rom.johtoMap") != std::string::npos);
}

// ---- the pictures' formats ----------------------------------------------------------------------------------------

// Gen II's LZ (pret's home/decompress.asm), command by command; the expected bytes are worked from that code
static void testLz()
{
	auto lz = [](std::vector<uint8_t> in, size_t max = 0x800) {
		std::vector<uint8_t> out;
		return lz2(in.data(), in.size(), out, max) ? out : std::vector<uint8_t>{ 0xEE, 0xEE, 0xEE };
	};
	using V = std::vector<uint8_t>;
	CHECK((lz({ 0x02, 7, 8, 9, 0xFF }) == V{ 7, 8, 9 }));						// literal, 3
	CHECK((lz({ 0x23, 0x55, 0xFF }) == V{ 0x55, 0x55, 0x55, 0x55 }));			// iterate, 4
	CHECK((lz({ 0x44, 0x12, 0x34, 0xFF }) == V{ 0x12, 0x34, 0x12, 0x34, 0x12 }));	// alternate, 5, from the first
	CHECK((lz({ 0x61, 0xFF }) == V{ 0, 0 }));									// zero, 2
	CHECK((lz({ 0x02, 1, 2, 3, 0x81, 0x82, 0xFF }) == V{ 1, 2, 3, 1, 2 }));	// repeat 2 from 2 + 1 back
	CHECK((lz({ 0x00, 0xAA, 0x82, 0x00, 0x00, 0xFF }) == V{ 0xAA, 0xAA, 0xAA, 0xAA }));	// repeat 3 from the start: into itself
	CHECK((lz({ 0x01, 0x01, 0x30, 0xA1, 0x81, 0xFF }) == V{ 0x01, 0x30, 0x80, 0x0C }));	// flip 2 from 1 + 1 back
	CHECK((lz({ 0x02, 1, 2, 3, 0xC2, 0x80, 0xFF }) == V{ 1, 2, 3, 3, 2, 1 }));	// reverse 3 from the last
	CHECK((lz({ 0xEE, 0x05, 0xFF }) == V(0x206, 0)));							// long zero: 2 << 8 | 5, + 1
	CHECK((lz({ 0xE0, 0x01, 4, 5, 0xFF }) == V{ 4, 5 }));						// long literal, 2
	CHECK((lz({ 0x00, 0x07, 0xFC, 0x01, 0x80, 0xFF }) == V{ 7, 7, 7 }));		// long command 7: a repeat
	CHECK((lz({ 0xFF }) == V{}));
	// refused: past its end (a literal's bytes, an alternate's second, no end mark), a copy before the start or past
	// what is written, longer than the buffer
	CHECK((lz({ 0x02, 1 }) == V{ 0xEE, 0xEE, 0xEE }));
	CHECK((lz({ 0x41, 1 }) == V{ 0xEE, 0xEE, 0xEE }));
	CHECK((lz({ 0x00, 1 }) == V{ 0xEE, 0xEE, 0xEE }));
	CHECK((lz({ 0x80, 0x80, 0xFF }) == V{ 0xEE, 0xEE, 0xEE }));
	CHECK((lz({ 0x00, 1, 0x80, 0x00, 0x01, 0xFF }) == V{ 0xEE, 0xEE, 0xEE }));
	CHECK((lz({ 0x00, 1, 0xC1, 0x80, 0xFF }) == V{ 0xEE, 0xEE, 0xEE }));
	CHECK((lz({ 0x63, 0xFF }, 3) == V{ 0xEE, 0xEE, 0xEE }));
	CHECK((lz({ 0x63, 0xFF }, 4) == V{ 0, 0, 0, 0 }));
}

// tiles to a picture: 2 x 2 tiles in columns (0 top left, 1 below it, 2 top right), the pixel's two bit planes; and
// the colours (RGB555 widened by their top 3 bits)
static void testPictureFormats()
{
	std::vector<uint8_t> tiles(4 * 16, 0);
	tiles[0] = 0x80;						// tile 0, row 0: pixel 0 is 1 (low plane)
	tiles[16 + 1] = 0x80;					// tile 1, row 0: pixel 0 is 2 (high plane): the picture's 0, 8
	tiles[32 + 14] = 0x01; tiles[32 + 15] = 0x01;	// tile 2, row 7: pixel 7 is 3: the picture's 15, 7
	nds::Sprite s;
	CHECK(pictureFrame(tiles, 2, s));
	CHECK(s.w == 16 && s.h == 16 && s.frames.size() == 1);
	if (s.frames.size() == 1 && s.frames[0].size() == 256)
	{
		const auto& f = s.frames[0];
		CHECK(f[0] == 1 && f[8 * 16] == 2 && f[7 * 16 + 15] == 3);
		int drawn = 0;
		for (uint8_t v : f) drawn += v != 0;
		CHECK(drawn == 3);
	}
	tiles.resize(3 * 16);
	CHECK(!pictureFrame(tiles, 2, s));
	// w x h: tile 1 of a 2 x 1 picture is on the right of tile 0; a side of 8 refused
	CHECK(pictureFrame(tiles, 2, 1, s) && s.w == 16 && s.h == 8 && s.frames.size() == 1 && s.frames[0][8] == 2 && s.frames[0][0] == 1);
	tiles.assign(8 * 16, 0);
	CHECK(!pictureFrame(tiles, 8, 1, s));
	CHECK(rgb555(0x7FFF) == 0xFFFFFF && rgb555(0) == 0 && rgb555(0x001F) == 0xFF0000 && rgb555(0x7C00) == 0x0000FF);
	CHECK(rgb555(0x46DE) == 0xF7B58C);	// RGB 30, 22, 17: 247, 181, 140
}

// Gen I's picture compression (pret's home/uncompress.asm) on 1 x 1-tile streams, the bits worked from that code. The
// first chunk: its plane's bit, 1 (pairs first), the pair 11 at the first place (row 0, bits 7-6), 00, a run of 31
// zeros (11110: c = 4, 2^5 - 1, then 00000) to the chunk's end. The second: its mode (0, 10 or 11), 0 (zeros first),
// a run of 1 (0, then 0), the pair 10 at row 1, 00, a run of 30 (1110: c = 3, 15, then 1111). Then each row decoded
// across (a 1 flips the value), and in modes 1 and 2 the second chunk's plane XORed with the first's
static void testPic1()
{
	using V = std::vector<uint8_t>;
	auto pic = [](V in, int& w, int& h) {
		std::vector<uint8_t> out;
		return pic1(in.data(), in.size(), w, h, out) ? out : V{ 0xEE };
	};
	// a tile's rows 0 and 1, low plane then high (the rest zeros)
	auto tile = [](uint8_t lo0, uint8_t hi0, uint8_t lo1, uint8_t hi1) { V t(16, 0); t[0] = lo0; t[1] = hi0; t[2] = lo1; t[3] = hi1; return t; };
	int w = 0, h = 0;
	// mode 0: both planes decoded: row 0's 11000000 is 10000000, row 1's 10000000 is 11111111
	CHECK((pic({ 0x11, 0x73, 0xC0, 0x08, 0xEF }, w, h) == tile(0x80, 0, 0, 0xFF)) && w == 1 && h == 1);
	// mode 1: the first decoded (10000000) and XORed into the second, not decoded
	CHECK((pic({ 0x11, 0x73, 0xC0, 0x84, 0x77, 0x80 }, w, h) == tile(0x80, 0x80, 0, 0x80)));
	// mode 2: both decoded, the second then XORed with the first
	CHECK((pic({ 0x11, 0x73, 0xC0, 0xC4, 0x77, 0x80 }, w, h) == tile(0x80, 0x80, 0, 0xFF)));
	// mode 1 with the first chunk in the high plane: the low one is the second, XORed
	CHECK((pic({ 0x11, 0xF3, 0xC0, 0x84, 0x77, 0x80 }, w, h) == tile(0x80, 0x80, 0x80, 0)));
	// refused: the stream runs out (its last byte gone), a side of 0 or 8
	CHECK((pic({ 0x11, 0x73, 0xC0, 0x08 }, w, h) == V{ 0xEE }));
	CHECK((pic({ 0x10, 0x73, 0xC0, 0x08, 0xEF }, w, h) == V{ 0xEE }));
	CHECK((pic({ 0x81, 0x73, 0xC0, 0x08, 0xEF }, w, h) == V{ 0xEE }));
	CHECK((pic({}, w, h) == V{ 0xEE }));
}

// Gen I's town map runs (LoadTownMap: the tile in the high nybble, the length in the low, 0 ends): tile 5 twice, tile
// 0 twenty-three times 15, tile 1 thirteen times, the screen's 360; refused when one short, one over, a run of 0 or no
// end in the bytes given
static void testTownMapCells()
{
	std::vector<uint8_t> rle = { 0x52 };
	rle.insert(rle.end(), 23, 0x0F);
	rle.push_back(0x1D);
	rle.push_back(0);
	auto cells = townMapCells(rle.data(), rle.size());
	CHECK(cells.size() == 360);
	if (cells.size() == 360) CHECK(cells[0] == 5 && cells[1] == 5 && cells[2] == 0 && cells[346] == 0 && cells[347] == 1 && cells[359] == 1);
	auto bad = rle;
	bad[24] = 0x1C;
	CHECK(townMapCells(bad.data(), bad.size()).empty());
	bad[24] = 0x1E;
	CHECK(townMapCells(bad.data(), bad.size()).empty());
	bad = rle;
	bad.insert(bad.begin() + 1, 0x30);
	CHECK(townMapCells(bad.data(), bad.size()).empty());
	CHECK(townMapCells(rle.data(), rle.size() - 1).empty());
}

// ---- the memory reader, over PokeDaisy's captures ----------------------------------------------------------------

// a capture: wram.bin (C000-DFFF as the bus had it), hram.bin (FF80-FFFE), wramx.bin (the eight WRAM banks)
struct Dump
{
	std::vector<uint8_t> wram, hram, wramx;
	bool load(const std::string& name)
	{
		auto file = [&](const char* f, std::vector<uint8_t>& out)
		{
			std::ifstream in(testDir + "/fixtures/" + name + "/" + f, std::ios::binary);
			out.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
			return !out.empty();
		};
		// (a Game Boy game's has no wramx.bin: its WRAM is C000-DFFF, no banks)
		bool ok = file("wram.bin", wram) && file("hram.bin", hram);
		if (ok && !file("wramx.bin", wramx)) wramx.clear();
		if (!ok) fprintf(stderr, "gbpk-units: no capture %s\n", name.c_str());
		return ok && wram.size() == 0x2000 && (wramx.empty() || wramx.size() == 0x8000);
	}
	// a byte as the cores' memory maps give READ_CORE_MEMORY: C000-CFFF bank 0, D000-DFFF bank 1, HRAM, and banks
	// 2-7 from 0x10000
	int at(uint32_t a) const
	{
		if (a >= 0xC000 && a < 0xD000) return wram[a - 0xC000];
		if (a >= 0xD000 && a < 0xE000) return wramx.empty() ? wram[a - 0xC000] : wramx[0x1000 + a - 0xD000];
		if (a >= 0xFF80 && a < 0xFFFF) return hram[a - 0xFF80];
		if (a >= 0x10000 && a < 0x16000 && !wramx.empty()) return wramx[0x2000 + a - 0x10000];
		return -1;
	}
	// (a byte of C000-CFFF or bank 1's D000-DFFF, to change a capture)
	uint8_t& d(uint16_t a) { return a < 0xD000 || wramx.empty() ? wram[a - 0xC000] : wramx[0x1000 + a - 0xD000]; }
};

// the emulator's replies to a set of requests, from a capture; "" when one can't be answered
static std::string answer(const std::string& req, const Dump& d)
{
	std::string out;
	size_t pos = 0, nl;
	while ((nl = req.find('\n', pos)) != std::string::npos)
	{
		unsigned a = 0; int n = 0;
		if (sscanf(req.c_str() + pos, "READ_CORE_MEMORY %x %d", &a, &n) != 2) return "";
		char b[32];
		snprintf(b, sizeof b, "READ_CORE_MEMORY %x", a);
		out += b;
		for (int i = 0; i < n; i++)
		{
			int v = d.at(a + (uint32_t)i);
			if (v < 0) return "";
			snprintf(b, sizeof b, " %02x", v);
			out += b;
		}
		out += "\n";
		pos = nl + 1;
	}
	return out;
}

// one set asked and answered (in two pieces, as a pipe may hand it over); the reader's live state then
static std::shared_ptr<banana::nds::Live> replay(Reader& r, const Dump& d, double& now, const nds::RomData* rom = nullptr)
{
	std::string q = r.requests(now);
	std::string a = answer(q, d);
	CHECK(!q.empty() && !a.empty());
	size_t half = a.size() / 2;
	r.feed(a.data(), half);
	r.feed(a.data() + half, a.size() - half);
	now += 250;
	return r.live(rom);
}

static GameData game(const char* key)
{
	GameData d;
	std::string why;
	if (!loadData(key, d, &why)) fprintf(stderr, "gbpk-units: %s\n", why.c_str());
	return d;
}

static const nds::BagItem* inBag(const nds::Live& l, uint8_t pocket, uint16_t item)
{
	auto p = l.bag.find(pocket);
	if (p == l.bag.end()) return nullptr;
	for (const auto& e : p->second) if (e.item == item) return &e;
	return nullptr;
}

// a made-up RomData: the species' names and gender ratios the reader looks up
struct FakeRom : nds::RomData
{
	std::shared_ptr<const nds::Sprite> sprite(uint16_t, bool, uint8_t) const override { return nullptr; }
};

static const uint16_t BERRY = 1000 + 0xAD, POTION = 1000 + 0x12, TACKLE = 33, GROWL = 45, LEER = 43;	// (POKE_BALL: rules.h's, 4)

// Crystal on ROUTE 29 (each release: the same save loaded): CHRIS with CHIKORITA holding a BERRY and a PIDGEY, the
// lab's POTION, four POKé BALLs (the README pokes five in and one was thrown at the PIDGEY), at (46, 11)
static void checkCrystal(const char* capture, const char* key)
{
	Dump d;
	if (!d.load(capture)) { failed++; return; }
	Reader r;
	r.open(game(key));
	double now = 0;
	auto l = replay(r, d, now);
	CHECK(l->valid);
	EQ(l->trainerName, "CHRIS");
	CHECK(l->trainerKnown);
	CHECK(l->party.size() == 2);
	if (l->party.size() == 2)
	{
		const nds::Mon& c = l->party[0];
		CHECK(c.species == 152 && c.item == BERRY && c.level == 5 && !c.egg);
		EQ(c.nick, "CHIKORITA");
		EQ(c.otName, "CHRIS");
		CHECK(c.otId == l->tid && c.moves[0] == TACKLE && c.moves[1] == GROWL && c.pp[0] > 0);
		CHECK(c.hp > 0 && c.hp <= c.maxHp && c.stats[0] > 0 && c.metLevel == 5);
		CHECK(l->party[1].species == 16 && l->party[1].item == 0);
		EQ(l->party[1].nick, "PIDGEY");
	}
	const nds::BagItem* balls = inBag(*l, nds::POCKET_BALLS, POKE_BALL);
	CHECK(balls && balls->count == 4);
	CHECK(inBag(*l, nds::POCKET_ITEMS, POTION));
	CHECK(l->zone == (24 << 8 | 3) && l->mapX == 46 && l->mapY == 11);
	CHECK(l->battle.kind == nds::BattleKind::NONE && !r.inBattle());
	// (both PIDGEY and CHIKORITA caught, the starter's pick seen)
	CHECK(l->dexCaught.size() == 32 && (l->dexCaught[(152 - 1) / 8] >> ((152 - 1) % 8) & 1) && (l->dexCaught[(16 - 1) / 8] >> ((16 - 1) % 8) & 1));
	CHECK(l->badges == 0 && l->playSeconds > 0);
	CHECK(r.serial() == 1 && r.error().empty());
	// the guide's flags: the 256 bytes of event flags, then a byte of derived bits; a new game hasn't won the League,
	// so none of them is set (EVENT_RED_IN_MT_SILVER is, from the start: Red is hidden till the Hall of Fame)
	auto flag = [&](int n) { return (size_t)(n >> 3) < l->flags.size() && (l->flags[(size_t)n >> 3] >> (n & 7) & 1); };
	CHECK(l->flags.size() == 257 && l->flags[256] == 0);
	CHECK(!flag(68) && flag(1890) && !flag(1213));
}

// the derived bits on the crystal capture with flags set: the League won sets the Elite Four's and Red's (his own
// flag is set from the start), a room's flag its member's alone; a bank not answered whole gives no flags
static void testDerivedFlags()
{
	Dump d;
	if (!d.load("crystal")) { failed++; return; }
	auto flags = [&](std::initializer_list<int> set) {
		Dump c = d;
		for (int n : set) c.d((uint16_t)(0xDA72 + n / 8)) |= (uint8_t)(1 << (n % 8));
		Reader r;
		r.open(game("crystal"));
		double now = 0;
		return replay(r, c, now)->flags;
	};
	std::vector<uint8_t> f = flags({ 68 });
	CHECK(f.size() == 257 && f[256] == 0x1F);
	f = flags({ 1465 });
	CHECK(f.size() == 257 && f[256] == 0x02);
	f = flags({ 1464, 1467 });
	CHECK(f.size() == 257 && f[256] == 0x09);
	// (the event flags asked for: one read of 256 bytes at DA72)
	Reader r;
	r.open(game("crystal"));
	CHECK(r.requests(0).find("READ_CORE_MEMORY da72 256\n") != std::string::npos);
	// ... and one answered short: the set isn't taken, nothing of it shows
	std::string q = r.requests(10000);
	std::string a = answer(q, d);
	const size_t at = a.find("READ_CORE_MEMORY da72 ");
	CHECK(at != std::string::npos);
	if (at != std::string::npos) a.erase(at + 21 + 3 * 200, 3 * 56);	// (the last 56 bytes' " xx")
	r.feed(a.data(), a.size());
	CHECK(!r.error().empty() && r.serial() == 0);
}

// ---- the guides ---------------------------------------------------------------------------------------------------

// Crystal's guide file through upstream's loader (a stand-in resolver: every place is map 0x0101) and logic: the
// bosses in PokeDaisy's order, done by their flags and the derived bits
static void testGuide()
{
	nds::GuideResolver res;
	res.zones = [](const std::string&) { return std::vector<uint16_t>{ 0x0101 }; };
	nds::GuideData g;
	std::string why = "-";
	CHECK(nds::loadGuideFile(dataPath + "/crystal.json", g, CRYSTAL, &why, &res));
	EQ(why, "");
	CHECK(g.unresolved.empty() && g.secrets.empty() && g.notes.size() == 2);
	int gyms = 0, e4 = 0, champion = 0, post = 0;
	for (const nds::Milestone& m : g.milestones)
	{
		gyms += m.kind == "gym";
		e4 += m.kind == "elite4";
		champion += m.kind == "champion";
		post += m.kind == "postgame";
		CHECK(m.trainerIds.size() == 1 && m.levelCap > 0 && m.foeAceLevel == m.levelCap && !m.placeZones.empty() &&
			m.doneWhen.compare(0, 5, "flag:") == 0);
	}
	CHECK(g.milestones.size() == 22 && gyms == 16 && e4 == 4 && champion == 1 && post == 1);
	if (g.milestones.size() == 22)
	{
		const nds::Milestone& f = g.milestones[0];
		EQ(f.name, "Gym 1: Falkner");
		EQ(f.place, "Violet City");
		EQ(f.foeName, "Falkner");
		EQ(f.doneWhen, "flag:1213");
		CHECK(f.trainerIds[0] == (1 << 8 | 1) && f.levelCap == 9);
		EQ(g.milestones[12].name, "Champion: Lance");
		EQ(g.milestones[12].doneWhen, "flag:68");
		EQ(g.milestones[21].name, "Red (Silver Cave)");
		EQ(g.milestones[21].doneWhen, "flag:2052");
	}
	// another game's guide is refused
	nds::GuideData other;
	CHECK(!nds::loadGuideFile(dataPath + "/crystal.json", other, GOLD, &why, &res));

	// the next milestone follows the save's flags
	nds::Live l;
	l.valid = true;
	l.flags.assign(257, 0);
	auto set = [&](int n) { l.flags[(size_t)n >> 3] |= (uint8_t)(1 << (n & 7)); };
	auto next = [&] { const nds::Milestone* m = nds::nextMilestone(g, l); return m ? m->id : std::string("none"); };
	EQ(next(), "gym-1-falkner");
	set(1213);
	EQ(next(), "gym-2-bugsy");
	for (int n = 1214; n <= 1220; n++) set(n);
	EQ(next(), "e4-will");
	set(2048);
	EQ(next(), "e4-koga");
	for (int n = 2049; n <= 2051; n++) set(n);
	EQ(next(), "champion-lance");
	set(68);
	EQ(next(), "gym-9-brock");
	for (int n = 1221; n <= 1228; n++) set(n);
	EQ(next(), "red");
	set(2052);
	EQ(next(), "none");
	// (no flags read: nothing is done)
	l.flags.clear();
	EQ(next(), "gym-1-falkner");
}

// Gold's guide from a made-up ROM's trainers (gsguide.h), with gold.ram.json's class ids: the stages in the story's
// order and by level in each, whatever the table's order; each leader done by its gym's badge, not its place (Chuck,
// Pryce and Jasmine are fought in that order, their badges are STORM, GLACIER and MINERAL: bits 5, 6 and 4 of
// wJohtoBadges, badge:6, 7 and 5); Janine, a Kanto leader below the Elite Four's level, after the Champion (SOULBADGE:
// bit 4 of wKantoBadges, badge:13); a leader class it doesn't know with Johto's, done by a later milestone
static void testRomGuide()
{
	const GameData d = game("gold");
	FakeRom rom;
	auto trainer = [&](int cls, const char* clsName, const char* name, std::vector<int> levels)
	{
		nds::Trainer t;
		t.id = (uint16_t)(cls << 8 | 1);
		t.cls = clsName;
		t.name = name;
		for (int lv : levels) { nds::TrainerMon m; m.level = (uint8_t)lv; m.species = 1; t.team.push_back(m); }
		rom.trainers[t.id] = t;
	};
	CHECK(romGuide(rom, d).milestones.empty());
	CHECK(d.size("leaderJasmine") == 6 && d.size("badgeMineral") == 4);
	trainer(d.size("leaderJasmine"), "Leader", "Jasmine", { 30, 35, 30 });
	trainer(0x0B, "Elite Four", "Will", { 40, 42 });
	trainer(0x10, "Champion", "Lance", { 50, 47 });
	trainer(d.size("leaderJanine"), "Leader", "Janine", { 36, 39 });
	trainer(d.size("leaderPryce"), "Leader", "Pryce", { 27, 31 });
	trainer(0x30, "Leader", "Ace", { 20 });
	trainer(d.size("leaderChuck"), "Leader", "Chuck", { 29, 30 });
	trainer(0x16, "Youngster", "Joey", { 4 });
	const nds::GuideData g = romGuide(rom, d);
	EQ(g.game, "gold");
	EQ(g.title, "Pokémon Gold");
	std::string got;
	for (const nds::Milestone& m : g.milestones)
		got += (got.empty() ? "" : ", ") + m.id + " " + m.doneWhen + " " + std::to_string(m.levelCap);
	EQ(got, "gym-1-ace later 20, gym-2-chuck badge:6 30, gym-3-pryce badge:7 31, gym-4-jasmine badge:5 35, "
		"e4-will later 42, champion-lance later 50, gym-5-janine badge:13 39");
	if (g.milestones.size() == 7)
	{
		const nds::Milestone& j = g.milestones[3];
		EQ(j.name, "Gym 4: Jasmine");
		EQ(j.kind, "gym");
		EQ(j.foeName, "Jasmine");
		CHECK(j.badge == 5 && j.trainerIds.size() == 1 && j.trainerIds[0] == (d.size("leaderJasmine") << 8 | 1));
		EQ(g.milestones[4].name, "Elite Four: Will");
		EQ(g.milestones[4].kind, "elite4");
		EQ(g.milestones[5].name, "Champion: Lance");
		EQ(g.milestones[5].kind, "champion");
	}

	// the next milestone follows the badges (Johto's in the low byte, Kanto's in the high one)
	nds::Live l;
	l.valid = true;
	auto next = [&] { const nds::Milestone* m = nds::nextMilestone(g, l); return m ? m->id : std::string("none"); };
	EQ(next(), "gym-1-ace");
	l.badges = 1 << 5;	// (STORMBADGE)
	EQ(next(), "gym-3-pryce");
	l.badges |= 1 << 4;	// (MINERALBADGE: Jasmine's, beaten before Pryce)
	EQ(next(), "gym-3-pryce");
	l.badges |= 1 << 6;
	EQ(next(), "e4-will");
	l.badges |= 1 << (8 + 4);	// (SOULBADGE: the Elite Four and the Champion done by a later milestone)
	EQ(next(), "none");
}

// ---- Generation I's captures ----

// the internal indexes of the Pokémon in Gen I's captures (pret's pokered constants/pokemon_constants.asm), as the ROM's
// table will give them: PIKACHU $54, SANDSLASH $61, SNORLAX $84, LAPRAS $13, PIDGEOT $97, RATICATE $A6, CHARMANDER $B0
static std::shared_ptr<GbRomData> gen1Rom()
{
	auto r = std::make_shared<GbRomData>();
	r->gen = 1;
	r->national.assign(256, 0);
	const std::pair<int, int> SP[] = { { 0x54, 25 }, { 0x61, 28 }, { 0x84, 143 }, { 0x13, 131 }, { 0x97, 18 }, { 0xA6, 20 }, { 0xB0, 4 } };
	for (auto [in, nat] : SP) r->national[(size_t)in] = (uint16_t)nat;
	return r;
}
static int bits(const std::vector<uint8_t>& v) { int n = 0; for (uint8_t b : v) n += __builtin_popcount(b); return n; }
static const uint16_t THUNDERBOLT = 85, THUNDER = 87, SLAM = 21, LIGHT_SCREEN = 113, SCRATCH = 10, EMBER = 52;

// Red, Blue and Yellow on the same save (their READMEs): Nelson, eight badges, ¥999999, 24:18 played, 10 owned and 106
// seen; six Lv100s, Joseph PIKACHU, Liam SANDSLASH, Fred SNORLAX, Henry LAPRAS, Seth PIDGEOT, Richard RATICATE; 17 bag
// items, no TOWN MAP; in Vermilion City's Pokémon Center (map $59) at (3, 5)
static std::shared_ptr<nds::Live> checkGen1(const char* capture, const char* key, Dump* changed = nullptr)
{
	Dump d;
	if (!d.load(capture)) { failed++; return std::make_shared<nds::Live>(); }
	if (changed) d = *changed;
	Reader r;
	r.open(game(key));
	double now = 0;
	auto rom = gen1Rom();
	auto l = replay(r, d, now, rom.get());
	CHECK(l->valid && l->trainerKnown);
	EQ(l->trainerName, "Nelson");
	const uint16_t species[6] = { 25, 28, 143, 131, 18, 20 };
	const char* const nicks[6] = { "Joseph", "Liam", "Fred", "Henry", "Seth", "Richard" };
	CHECK(l->party.size() == 6);
	for (size_t i = 0; i < l->party.size() && i < 6; i++)
	{
		const nds::Mon& m = l->party[i];
		CHECK(m.species == species[i] && m.level == 100 && m.item == 0 && !m.female && !m.shiny() && !m.egg);
		EQ(m.nick, nicks[i]);
		EQ(m.otName, "Nelson");
		CHECK(m.otId == l->tid && m.stats[3] > 0 && m.stats[3] == m.stats[4] && m.ivs[4] == m.ivs[5]);
	}
	if (l->party.size() == 6)
	{
		const nds::Mon& p = l->party[0];
		CHECK(p.maxHp == 182 && p.moves[0] == THUNDERBOLT && p.moves[1] == THUNDER && p.moves[2] == SLAM && p.moves[3] == LIGHT_SCREEN);
		// (no friendship in Red and Blue; Yellow's starter Pikachu has its happiness)
		CHECK(std::string(key) == "yellow" ? p.friendship > 0 : p.friendship == 0);
		for (size_t i = 1; i < 6; i++) CHECK(l->party[i].friendship == 0);
	}
	CHECK(l->zone == 0x59 && l->mapX == 3 && l->mapY == 5);
	// (24:18 when saved; the captures that played on reach 24:19)
	CHECK(l->money == 999999 && l->badges == 0xFF && l->playSeconds / 60 >= 24 * 60 + 18 && l->playSeconds / 60 <= 24 * 60 + 19);
	CHECK(l->dexCaught.size() == 19 && bits(l->dexCaught) == 10 && bits(l->dexSeen) == 106);
	size_t items = 0;
	for (const auto& [pocket, list] : l->bag) items += list.size();
	CHECK(items == 17 && !inBag(*l, nds::POCKET_KEY, GEN1_ONLY + 0x05));
	CHECK(l->timeOfDay == nds::TimeOfDay::ANY && l->flags.empty() && r.error().empty());
	return l;
}

static void testGen1Captures()
{
	// on the field
	for (auto [cap, key] : { std::pair{ "red", "red" }, { "blue", "blue" }, { "yellow", "yellow" } })
	{
		auto l = checkGen1(cap, key);
		CHECK(l->battle.kind == nds::BattleKind::NONE);
	}
	// the bag's pockets: the five stones, HM01-04 as Gen IV's 420-423, the key items by the game's list
	{
		auto l = checkGen1("red", "red");
		CHECK(inBag(*l, nds::POCKET_ITEMS, 81) && inBag(*l, nds::POCKET_ITEMS, 85));
		CHECK(inBag(*l, nds::POCKET_TMS, 420) && inBag(*l, nds::POCKET_TMS, 423) && !inBag(*l, nds::POCKET_TMS, 424));
		CHECK(l->bag.count(nds::POCKET_KEY) && l->bag.at(nds::POCKET_KEY).size() == 8);
	}
	// a wild CHARMANDER Lv12 (SCRATCH / GROWL / EMBER) against Joseph, PIKACHU Lv100 at 182/182, out of the first
	// slot: our battler is the party member (its name kept), the foe's types the battle's
	for (auto [cap, key, hp] : { std::tuple{ "red_battle", "red", 34 }, { "blue_battle", "blue", 32 }, { "yellow_battle", "yellow", -1 } })
	{
		auto l = checkGen1(cap, key);
		const nds::Battle& b = l->battle;
		CHECK(b.kind == nds::BattleKind::WILD && b.trainerId == 0 && b.foeParty.empty());
		CHECK(b.ours[0].present && b.ours[0].mon.species == 25 && b.ours[0].mon.level == 100 && b.ours[0].mon.hp == 182 && b.ours[0].mon.maxHp == 182);
		EQ(b.ours[0].mon.nick, "Joseph");
		CHECK(b.ours[0].types[0] == nds::ELECTRIC && b.ours[0].types[1] == nds::ELECTRIC && b.ours[0].stages[1] == 0);
		const nds::Mon& f = b.foes[0].mon;
		CHECK(b.foes[0].present && f.species == 4 && f.level == 12 && (hp < 0 || f.hp == hp) && f.hp == f.maxHp);
		CHECK(f.moves[0] == SCRATCH && f.moves[1] == GROWL && f.moves[2] == EMBER && f.moves[3] == 0);
		CHECK(b.foes[0].types[0] == nds::FIRE && b.foes[0].types[1] == nds::FIRE && f.stats[3] == f.stats[4]);
	}
	// the menus' captures read as the battle's
	for (auto [cap, key] : { std::pair{ "red_battle_menu", "red" }, { "red_battle_move2", "red" }, { "red_battle_party", "red" },
		{ "red_battle_switch_menu", "red" }, { "yellow_battle_menu", "yellow" }, { "yellow_battle_move2", "yellow" },
		{ "yellow_battle_party", "yellow" }, { "yellow_battle_switch_menu", "yellow" } })
	{
		auto l = checkGen1(cap, key);
		CHECK(l->battle.kind == nds::BattleKind::WILD && l->battle.ours[0].mon.species == 25 && l->battle.foes[0].mon.species == 4);
	}
	// Joseph fainted (0/182), in the party and out in the battle
	for (auto [cap, key] : { std::pair{ "red_battle_faint", "red" }, { "yellow_battle_faint", "yellow" } })
	{
		auto l = checkGen1(cap, key);
		CHECK(l->party.size() == 6 && l->party[0].hp == 0 && l->battle.ours[0].mon.hp == 0 && !l->battle.ours[0].present);
	}

	// a trainer battle, made from red_battle: one foe, a copy of the party's second (Liam, SANDSLASH) out, the
	// trainer's class and number
	{
		Dump d;
		if (d.load("red_battle"))
		{
			const GameData g = game("red");
			auto a = [&](const char* k) { return g.at(k).addr; };
			d.d(a("isInBattle")) = 2;
			d.d(a("enemyPartyCount")) = 1;
			d.d((uint16_t)(a("enemyPartyCount") + 1)) = 0x61;
			d.d((uint16_t)(a("enemyPartyCount") + 2)) = 0xFF;
			for (int i = 0; i < 44; i++) d.d((uint16_t)(a("enemyMons") + i)) = d.d((uint16_t)(a("partyMons") + 44 + i));
			d.d(a("enemyMonPartyPos")) = 0;
			d.d(a("enemyMon")) = 0x61;
			d.d(a("trainerClass")) = 0x22;
			d.d(a("trainerNo")) = 3;
			auto l = checkGen1("red_battle", "red", &d);
			const nds::Battle& b = l->battle;
			CHECK(b.kind == nds::BattleKind::TRAINER && b.trainerId == (0x22 << 8 | 3) && b.foeParty.size() == 1);
			// (the foe is its party's member: Liam's stat experience, which the battle's copy doesn't hold)
			CHECK(b.foes[0].mon.species == 28 && b.foes[0].mon.statExp[0] == 250);
			if (b.foeParty.size() == 1) CHECK(b.foeParty[0].species == 28 && b.foeParty[0].level == 100);
		}
	}

	// Yellow's Pikachu with another OT id: not the starter, no happiness
	{
		Dump d;
		if (d.load("yellow"))
		{
			d.d((uint16_t)(game("yellow").at("partyMons").addr + 12)) ^= 0xFF;
			Reader r;
			r.open(game("yellow"));
			double now = 0;
			auto rom = gen1Rom();
			auto l = replay(r, d, now, rom.get());
			CHECK(l->valid && l->party.size() == 6 && l->party[0].species == 25 && l->party[0].friendship == 0);
		}
	}
	// without the ROM's table a species is its internal index (PIKACHU $54); an index the table has no species for
	// (RATICATE's, made one) leaves the party unread
	{
		Dump d;
		if (d.load("red"))
		{
			Reader r;
			r.open(game("red"));
			double now = 0;
			auto l = replay(r, d, now);
			CHECK(l->valid && l->party.size() == 6 && l->party[0].species == 0x54);
			auto rom = gen1Rom();
			rom->national[0xA6] = 0;
			d.d(game("red").at("playTime").addr) ^= 1;	// (a new set: another byte)
			l = replay(r, d, now, rom.get());
			CHECK(!l->valid && l->party.empty());
		}
	}
	// a party count of 7, or a species list that isn't the structs', leaves it unread
	{
		Dump d;
		if (d.load("red"))
		{
			const uint16_t pc = game("red").at("partyCount").addr;
			Dump c = d;
			c.d(pc) = 7;
			Reader r;
			r.open(game("red"));
			double now = 0;
			CHECK(!replay(r, c, now, gen1Rom().get())->valid);
			c = d;
			c.d((uint16_t)(pc + 1)) = 0x13;
			CHECK(!replay(r, c, now, gen1Rom().get())->valid);
		}
	}
}

// a made-up Gen I ROM, 64 KB, laid out by a made-up data file (every table in bank 1): three species out of national
// order (internal 1 is species 2, 3 is 3 and $54 is 1, Bulbasaur, as Pikachu's index in the captures; 2 is none),
// the third's base data apart as Red's Mew; three moves, ten items, three maps under two town map names with grass,
// water and the three rods, three trainer classes (one empty) and a special move. The layouts are pret's (rom1.cpp
// names each).
static std::shared_ptr<const std::vector<uint8_t>> madeUpGen1(GameData& d, int bad = 0)
{
	MadeUpRom m;
	using R = MadeUpRom;
	m.d.key = "red";
	m.d.gen = 1;
	m.d.game.id = RED;
	m.d.game.gen = GEN_I;
	m.d.game.title = "Made-up";
	const int IDX = 0x54;
	m.d.sizes = { { "numPokemon", 3 }, { "numPokemonIndexes", IDX }, { "numMoves", 3 }, { "numItems", 10 }, { "hm01", 0xC4 },
		{ "tm01", 0xC9 }, { "numTrainerClasses", 3 }, { "numMaps", 3 }, { "firstIndoorMap", 1 }, { "quickAttack", 2 }, { "counter", 3 } };
	// PokedexOrder: internal 1 species 2, 3 species 3, $54 species 1 (or 2 again, for a refusal)
	std::vector<int> order(IDX, 0);
	order[0] = 2;
	order[2] = 3;
	order[IDX - 1] = bad == 1 ? 2 : 1;
	m.key("dexOrder", m.put(order));
	// base data: species, HP, Attack, Defense, Speed, Special, types (Grass 22, Poison 3), catch rate, base exp, picture
	// size and pointers (the front's at 4000 in its bank; species 2 none), level 1 moves (move 1), growth, TM/HM bits
	// (TM01, HM01: bit 50), pad; species 3's apart
	std::vector<int> b1 = { 1, bad == 2 ? 44 : 45, 49, 49, 45, 65, 22, 3, 45, 64, 0x55, 0x00, 0x40, 0, 0, 1, 0, 0, 0, 3, 0x01, 0, 0, 0, 0, 0, 0x04, 0 };
	std::vector<int> b2 = { 2, 60, 62, 63, 60, 80, 22, 22, 45, 141, 0x66, 0, 0, 0, 0, 1, 2, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0 };
	std::vector<int> b3 = { 3, 100, 100, 100, 100, 100, 24, 24, 45, 64, 0x55, 0x00, 0x40, 0, 0, 1, 0, 0, 0, 3, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x7F, 0 };
	m.key("baseData", m.put(R::cat(b1, b2)));
	m.key("mewBaseData", m.put(b3));
	// MonsterNames by internal index, 10 bytes each
	std::vector<int> names((size_t)IDX * 10, 0x50);
	auto name = [&](int idx, const char* t) { std::vector<int> v = R::str(t); for (size_t i = 0; i < v.size() && i < 10; i++) names[(idx - 1) * 10 + i] = v[i]; };
	name(1, "IVYSAUR");
	name(3, "MEW");
	name(IDX, "BULBASAUR");
	m.key("pokemonNames", m.put(names));
	// the Pokédex: category, feet and inches (2'04"), pounds * 10 (15.0), text_far to the text: TX_START, a line, <NEXT>,
	// <PAGE>, <DEXEND>
	std::vector<int> text = R::cat({ 0 }, R::str("A STRANGE|SEED"));
	text.back() = 0x49;
	text = R::cat(text, R::str("PAGE TWO"));
	text.back() = 0x5F;
	text.push_back(0x50);
	const int t1 = m.put(text);
	const int x1 = m.put(R::cat(R::str("SEED"), { 2, 4, 150, 0, 0x17, t1 & 0xFF, t1 >> 8, 1, 0x50 }));
	std::vector<int> dex((size_t)IDX * 2, 0);
	dex[(IDX - 1) * 2] = x1 & 0xFF;
	dex[(IDX - 1) * 2 + 1] = x1 >> 8;
	m.key("dexEntryPointers", m.put(dex));
	// evolutions then learnsets, by internal index: Bulbasaur at 16 into internal 1 (species 2), learns move 2 at 7;
	// species 2 by item $0A (a Moon Stone) into internal 3 (species 3); species 3 by trade into $54
	const int e1 = m.put({ 1, 16, 1, 0, 7, 2, 0 }), e2 = m.put({ 2, 0x0A, 1, 3, 0, 0 }), e3 = m.put({ 3, 1, IDX, 0, 0 });
	std::vector<int> evos((size_t)IDX * 2, 0);
	for (auto [idx, p] : { std::pair<int, int>{ IDX, e1 }, { 1, e2 }, { 3, e3 } }) { evos[(idx - 1) * 2] = p & 0xFF; evos[(idx - 1) * 2 + 1] = p >> 8; }
	m.key("evosAttacksPointers", m.put(evos));
	// moves: animation (the id), effect, power, type, accuracy, PP: Pound, Quick Attack (its id), Ember (Fire, 20; the
	// data file's counter, for its -1)
	m.key("moves", m.put({ 1, 0, 40, 0, 255, 35, 2, 0, 40, 0, 255, 30, 3, 4, 40, 20, 255, 25 }));
	m.key("moveNames", m.put(R::cat(R::cat(R::str("POUND"), R::str("QUICK ATTACK")), R::str("EMBER"))));
	// items 1-10: names and prices (bcd3: the Ultra Ball 1200, the Poké Ball 200)
	std::vector<int> in;
	for (const char* n : { "MASTER BALL", "ULTRA BALL", "GREAT BALL", "POKE BALL", "TOWN MAP", "BICYCLE", "SURFBOARD", "SAFARI BALL",
		"POKEDEX", "MOON STONE" })
		in = R::cat(in, R::str(n));
	m.key("itemNames", m.put(in));
	std::vector<int> prices(30, 0);
	prices[4] = 0x12;
	prices[10] = 0x02;
	m.key("itemPrices", m.put(prices));
	std::vector<int> tm(55, 0);
	tm[0] = 1;
	tm[50] = 2;
	m.key("tmHmMoves", m.put(tm));
	// the town map: map 0 outdoors (Pallet Town); maps 1 and 2 an indoor group below 3 (Viridian Forest), to -1
	const int n1 = m.put(R::str("PALLET TOWN")), n2 = m.put(R::str("VIRIDIAN FOREST"));
	m.key("externalMapEntries", m.put({ 0xB2, n1 & 0xFF, n1 >> 8 }));
	m.key("internalMapEntries", m.put({ 3, 0x42, n2 & 0xFF, n2 >> 8, 0xFF }));
	// wild Pokémon: map 0 none; map 1 grass (Bulbasaur Lv 3 first, then internal 1, Lv 3 + slot), no water; map 2 no
	// grass, water (species 3, Lv 20)
	const int w0 = m.put({ 0, 0 });
	std::vector<int> g = { 25 };
	for (int j = 0; j < 10; j++) { g.push_back(3 + j); g.push_back(j == 0 ? IDX : 1); }
	g.push_back(0);
	const int w1 = m.put(g);
	std::vector<int> w = { 0, 5 };
	for (int j = 0; j < 10; j++) { w.push_back(20); w.push_back(3); }
	const int w2 = m.put(w);
	m.key("wildData", m.put(R::words({ w0, w1, w2, 0xFFFF })));
	m.key("wildSlots", m.put({ 50, 0, 101, 2, 140, 4, 165, 6, 190, 8, 215, 10, 228, 12, 241, 14, 252, 16, 255, 18 }));
	// the rods: the Old Rod's code (call, jp c, lb bc, 5, Bulbasaur), the Good Rod's internal 1 and 3 at Lv 10, the
	// Super Rod's group at map 0: species 3 Lv 15 and Bulbasaur Lv 20
	m.key("oldRod", m.put({ 0xCD, 0, 0, 0xDA, 0, 0, 0x01, IDX, 5 }));
	m.key("goodRodMons", m.put({ 10, 1, 10, 3 }));
	const int group = m.put({ 2, 15, 3, 20, IDX });
	m.key("superRod", m.put({ 0, group & 0xFF, group >> 8, 0xFF }));
	// trainers: class 1 a party of one level (Lv 8: Bulbasaur, species 2) and one with its own (Bulbasaur Lv 10,
	// species 3 Lv 12); class 2 none (its pointer the next class's); class 3 species 2 Lv 20 (or internal 2, none, for a
	// refusal); then the end
	m.key("trainerClassNames", m.put(R::cat(R::cat(R::str("YOUNGSTER"), R::str("CHIEF")), R::str("BROCK"))));
	const int c1 = m.put({ 8, IDX, 1, 0, 0xFF, 10, IDX, 12, 3, 0 }), c3 = m.put({ 0xFF, 20, bad == 3 ? 2 : 1, 0 });
	m.key("trainerGroupsEnd", m.next);
	m.key("trainerGroups", m.put(R::words({ c1, c3, c3 })));
	// Yellow's table: class 1's second trainer, its second member's fourth move Ember; the list to -1
	m.key("specialTrainerMoves", m.put({ 1, 2, 2, 4, 3, 0, 0xFF }));
	// the front pictures by internal index (home/pics.asm UncompressMonSprite, the games' bounds): Bulbasaur ($54, from
	// BEEDRILL + 2 below STARMIE + 1: "Pics 3", here bank 2) 5 x 5 tiles, its first pixel 1 (a pair 11 and a run of 799
	// zeros: 111111110 and 288; the second plane a run of 800: 289); Mew, its base data apart, in its own bank (3):
	// testPic1's mode 0 tile; the other banks 1
	m.d.sizes.insert({ { "picsTangela", 0x1E }, { "picsMoltres", 0x49 }, { "picsBeedrill", 0x72 }, { "picsStarmie", 0x98 }, { "palTownMap", 3 } });
	for (const char* k : { "pics1", "pics2", "pics4", "pics5" }) m.d.rom[k] = Addr{ 1, 0x4000 };
	m.d.rom["pics3"] = Addr{ 2, 0x4000 };
	m.d.rom["mewPicFront"] = Addr{ 3, 0x4000 };
	const uint8_t bulbasaurPic[] = { 0x55, 0x73, 0xFD, 0x20, 0x3F, 0xD2, 0x10 }, mewPic[] = { 0x11, 0x73, 0xC0, 0x08, 0xEF };
	memcpy(&m.b[0x8000], bulbasaurPic, sizeof bulbasaurPic);
	memcpy(&m.b[0xC000], mewPic, sizeof mewPic);
	// the palettes: an id by national number (MissingNo.'s first), four RGB555 colours each: Bulbasaur's 2 white, red,
	// green, black; Mew's 0 white; the town map's 3 white, blue, green, black
	m.key("monPaletteIds", m.put({ 0, 2, 1, 0 }));
	m.key("palettes", m.put(R::words({ 0x7FFF, 0, 0, 0, 0x7FFF, 0, 0, 0, 0x7FFF, 0x001F, 0x03E0, 0, 0x7FFF, 0x7C00, 0x03E0, 0 })));
	// the town map: tile 1 (every pixel 1) in the first cell, tile 0 (every pixel 0) in the other 359; TownMapOrder Pallet
	// Town (its entry's x 2, y 11) then map 1 (Viridian Forest's group, x 2, y 4)
	std::vector<int> rle = { 0x11 };
	rle.insert(rle.end(), 23, 0x0F);
	rle.push_back(0x0E);
	rle.push_back(0);
	m.key("townMap", m.put(rle));
	std::vector<int> tiles(256, 0);
	for (int k = 0; k < 16; k += 2) tiles[16 + k] = 0xFF;
	m.key("townMapTiles", m.put(tiles));
	m.key("townMapOrder", m.put({ 0, 1 }));
	m.key("townMapOrderEnd", m.next);
	d = m.d;
	return std::make_shared<std::vector<uint8_t>>(m.b);
}

static void testGen1RomTables()
{
	GameData d;
	std::string why;
	auto rom = madeUpGen1(d);
	auto r = readRom(rom, d, &why);
	CHECK(r);
	EQ(why, "");
	if (!r) return;
	// the internal order, and the species by national number
	const std::vector<uint16_t>& nat = static_cast<const GbRomData&>(*r).national;
	CHECK(nat.size() == 256 && nat[1] == 2 && nat[2] == 0 && nat[3] == 3 && nat[0x54] == 1 && nat[0x55] == 0);
	CHECK(r->species.size() == 4);
	const nds::Species& b = r->species[1];
	EQ(b.name, "Bulbasaur");
	EQ(r->species[2].name, "Ivysaur");
	EQ(r->species[3].name, "Mew");
	CHECK((b.base[0] == 45 && b.base[1] == 49 && b.base[2] == 49 && b.base[3] == 45 && b.base[4] == 65 && b.base[5] == 65));
	CHECK(b.types[0] == nds::GRASS && b.types[1] == nds::POISON && b.catchRate == 45 && b.baseExp == 64 && b.genderRatio == 255 &&
		b.growth == 3 && b.baseFriendship == 0);
	CHECK((b.tms == std::vector<uint16_t>{ 1, 93 }));
	CHECK(r->species[3].base[0] == 100 && r->species[3].types[0] == nds::PSYCHIC && r->species[3].tms.size() == 55);
	EQ(b.category, "Seed");
	CHECK(b.height == 7 && b.weight == 68);		// 28 in: 0.71 m; 15.0 lb: 6.80 kg
	EQ(b.dexText, "A STRANGE SEED PAGE TWO.");
	// the level 1 moves, then the level-up ones; the evolutions into national numbers, their words by the items' names
	CHECK(b.learnset.size() == 2 && b.learnset[0].level == 1 && b.learnset[0].move == 1 && b.learnset[1].level == 7 && b.learnset[1].move == 2);
	CHECK(r->species[2].learnset.size() == 2 && r->species[2].learnset[1].move == 2 && r->species[2].learnset[1].level == 1);
	CHECK(b.evolutions.size() == 1 && b.evolutions[0].method == nds::evo::LEVEL && b.evolutions[0].param == 16 && b.evolutions[0].into == 2);
	const nds::Species& v = r->species[2];
	CHECK(v.evolutions.size() == 1 && v.evolutions[0].method == nds::evo::USE_ITEM && v.evolutions[0].param == 81 && v.evolutions[0].into == 3);
	EQ(v.evolutions.empty() ? "" : v.evolutions[0].how, "Use Moon Stone");
	CHECK(r->species[3].evolutions.size() == 1 && r->species[3].evolutions[0].method == nds::evo::TRADE && r->species[3].evolutions[0].into == 1);
	// moves: Quick Attack's +1 and the counter's -1 by their ids, the category by the type
	CHECK(r->moves.size() == 4);
	EQ(r->moves[2].name, "Quick Attack");
	CHECK(r->moves[1].power == 40 && r->moves[1].accuracy == 100 && r->moves[1].pp == 35 && r->moves[1].cat == nds::MoveCategory::PHYSICAL &&
		r->moves[1].priority == 0);
	CHECK((int8_t)r->moves[2].priority == 1 && (int8_t)r->moves[3].priority == -1);
	CHECK(r->moves[3].type == nds::FIRE && r->moves[3].cat == nds::MoveCategory::SPECIAL && r->moves[3].effect == 4);
	// items at their Gen IV ids (the balls, the Moon Stone) or GEN1_ONLY + theirs, their prices and pockets; the TMs and
	// HMs as Gen IV's
	EQ(r->items[1].name, "Master Ball");
	CHECK(r->items[2].price == 1200 && r->items[4].price == 200 && r->items[1].pocket == nds::POCKET_BALLS);
	EQ(r->items[81].name, "Moon Stone");
	EQ(r->items[GEN1_ONLY + 6].name, "Bicycle");
	CHECK(r->items[GEN1_ONLY + 6].pocket == nds::POCKET_KEY && r->items[5].pocket == nds::POCKET_BALLS);
	EQ(r->items[328].name, "TM01");
	EQ(r->items[424].name, "HM05");
	CHECK(r->tmMoves.size() == 100 && r->tmMoves[0] == 1 && r->tmMoves[92] == 2);
	// places: each map named by the town map; the names once each
	CHECK(r->zones.size() == 3);
	CHECK((r->locationNames == std::vector<std::string>{ "Pallet Town", "Viridian Forest" }));
	if (r->zones.size() != 3) return;
	EQ(r->zones.at(0).name, "Pallet Town");
	EQ(r->zones.at(1).name, "Viridian Forest");
	EQ(r->zones.at(2).name, "Viridian Forest");
	auto find = [&](uint16_t z, nds::EncounterKind k, int species, int level = 0) -> const nds::Encounter* {
		for (const auto& e : r->zones.at(z).encounters) if (e.kind == k && e.species == species && (!level || e.minLevel == level)) return &e;
		return nullptr;
	};
	auto count = [&](uint16_t z, nds::EncounterKind k) { int n = 0; for (const auto& e : r->zones.at(z).encounters) n += e.kind == k; return n; };
	// grass: the slots' 20, 20, 15, 10, 10, 10, 5, 5, 4 and 1 percent
	CHECK(count(1, nds::EncounterKind::GRASS) == 10 && count(1, nds::EncounterKind::SURF) == 0 && count(1, nds::EncounterKind::OLD_ROD) == 0);
	const nds::Encounter* e = find(1, nds::EncounterKind::GRASS, 1);
	CHECK(e && e->minLevel == 3 && e->maxLevel == 3 && e->percent == 20 && e->time == nds::TimeOfDay::ANY);
	int pct = 0;
	for (const auto& x : r->zones.at(1).encounters) pct += x.percent;
	CHECK(pct == 100);
	e = find(1, nds::EncounterKind::GRASS, 2, 12);
	CHECK(e && e->percent == 1);
	// water, and the Old and Good Rods with it; the Super Rod's map has those too, with its group at even chances
	CHECK(count(2, nds::EncounterKind::SURF) == 10 && find(2, nds::EncounterKind::SURF, 3, 20));
	e = find(2, nds::EncounterKind::OLD_ROD, 1, 5);
	CHECK(e && e->percent == 100);
	e = find(2, nds::EncounterKind::GOOD_ROD, 3, 10);
	CHECK(e && e->percent == 50 && find(2, nds::EncounterKind::GOOD_ROD, 2, 10) && count(2, nds::EncounterKind::SUPER_ROD) == 0);
	CHECK(count(0, nds::EncounterKind::GRASS) == 0 && count(0, nds::EncounterKind::OLD_ROD) == 1 && count(0, nds::EncounterKind::GOOD_ROD) == 2);
	e = find(0, nds::EncounterKind::SUPER_ROD, 3, 15);
	CHECK(e && e->percent == 50 && find(0, nds::EncounterKind::SUPER_ROD, 1, 20));
	// trainers by class << 8 | number, named by the class; moves from the learnset at the level, and Yellow's special
	// move; the empty class has none
	CHECK(r->trainers.size() == 3 && r->trainers.count(0x0101) && r->trainers.count(0x0102) && r->trainers.count(0x0301));
	if (r->trainers.size() == 3)
	{
		const nds::Trainer& y = r->trainers.at(0x0101);
		EQ(y.cls, "Youngster");
		EQ(y.name, "Youngster");
		CHECK(y.team.size() == 2 && y.team[0].species == 1 && y.team[0].level == 8 && y.team[1].species == 2 && y.team[1].level == 8 &&
			y.team[0].moves[0] == 1 && y.team[0].moves[1] == 2 && y.team[0].moves[2] == 0);
		const nds::Trainer& y2 = r->trainers.at(0x0102);
		CHECK(y2.team.size() == 2 && y2.team[0].level == 10 && y2.team[1].species == 3 && y2.team[1].level == 12 &&
			y2.team[1].moves[0] == 1 && y2.team[1].moves[3] == 3);
		const nds::Trainer& brock = r->trainers.at(0x0301);
		EQ(brock.name, "Brock");
		CHECK(brock.team.size() == 1 && brock.team[0].species == 2 && brock.team[0].level == 20);
	}
	// the pictures: Bulbasaur's from "Pics 3"'s bank, 5 x 5 tiles, its first pixel 1, in its palette (shiny the same);
	// Mew's from its own bank, 1 x 1; Ivysaur none
	auto bulbasaur = r->sprite(1);
	CHECK(bulbasaur && bulbasaur->w == 40 && bulbasaur->h == 40 && bulbasaur->frames.size() == 1);
	if (bulbasaur && bulbasaur->frames.size() == 1 && bulbasaur->frames[0].size() == 1600)
	{
		int drawn = 0;
		for (uint8_t v : bulbasaur->frames[0]) drawn += v != 0;
		CHECK(bulbasaur->frames[0][0] == 1 && drawn == 1);
		CHECK(bulbasaur->normal[0] == 0xFFFFFF && bulbasaur->normal[1] == 0xFF0000 && bulbasaur->normal[2] == 0x00FF00 && bulbasaur->normal[3] == 0);
		CHECK(bulbasaur->shiny == bulbasaur->normal);
	}
	auto mew = r->sprite(3);
	CHECK(mew && mew->w == 8 && mew->h == 8 && mew->frames.size() == 1 && mew->frames[0][0] == 1 && mew->frames[0][8] == 2);
	CHECK(!r->sprite(2) && !r->sprite(0) && !r->sprite(4));
	// the town map: the first cell blue (tile 1, the town map's palette's colour 1), the rest white; the cells a cell
	// from TownMapOrder's places name them (Pallet Town about 4, 12, its entry's x + 2, y + 1; Viridian Forest about
	// 4, 5); the places' spots: the game's 16 x 16 icon at their entry (x * 8 + 12, y * 8 + 5)
	CHECK(r->regionMaps.size() == 1);
	if (r->regionMaps.size() == 1)
	{
		const nds::RegionMap& k = r->regionMaps[0];
		EQ(k.name, "Kanto");
		CHECK(k.w == 160 && k.h == 144 && k.cell == 8 && k.gridW == 20 && k.gridH == 18 && k.cellX == 0 && k.cellY == 0);
		CHECK(k.rgba.size() == 160 * 144);
		if (k.rgba.size() == 160 * 144)
			CHECK(k.rgba[0] == 0xFF0000FFu && k.rgba[7 * 160 + 7] == 0xFF0000FFu && k.rgba[8] == 0xFFFFFFFFu && k.rgba[143 * 160 + 159] == 0xFFFFFFFFu);
		auto cell = [&](int x, int y) { return k.cells[(size_t)y * k.gridW + x]; };
		CHECK(k.cells.size() == 360);
		if (k.cells.size() == 360)
		{
			CHECK(cell(4, 12) == 0 && cell(3, 11) == 0 && cell(5, 13) == 0 && cell(6, 12) == 0xFFFF && cell(4, 10) == 0xFFFF);
			CHECK(cell(4, 5) == 1 && cell(5, 6) == 1 && cell(4, 7) == 0xFFFF);
			int named = 0;
			for (uint16_t c : k.cells) named += c != 0xFFFF;
			CHECK(named == 18);
		}
	}
	const nds::MapSpot& pallet = r->zones.at(0).spot;
	CHECK(pallet.map == 0 && pallet.x == 28 && pallet.y == 93 && pallet.w == 16 && pallet.h == 16);
	CHECK(r->zones.at(2).spot.map == 0 && r->zones.at(2).spot.x == 28 && r->zones.at(2).spot.y == 37);

	// the reader's live() through the ROM's internal order: the red capture's party cut to its first, Pikachu's index
	// $54, here species 1; without the ROM the index as it is
	{
		Dump dump;
		if (dump.load("red"))
		{
			const GameData red = game("red");
			const uint16_t pc = red.at("partyCount").addr;
			dump.d(pc) = 1;
			dump.d((uint16_t)(pc + 2)) = 0xFF;
			for (const nds::RomData* with : { r.get(), (const nds::RomData*)nullptr })
			{
				Reader rd;
				rd.open(red);
				double now = 0;
				auto l = replay(rd, dump, now, with);
				CHECK(l->party.size() == 1 && l->party[0].species == (with ? 1 : 0x54));
			}
		}
		else failed++;
	}

	// refusals: a species twice in PokedexOrder, a first record that isn't Bulbasaur's, a trainer whose species isn't
	// one, a table the data file doesn't give, no Super Rod table (or both)
	rom = madeUpGen1(d, 1);
	CHECK(!readRom(rom, d, &why) && why.find("PokedexOrder") != std::string::npos);
	rom = madeUpGen1(d, 2);
	CHECK(!readRom(rom, d, &why) && why.find("Bulbasaur") != std::string::npos);
	rom = madeUpGen1(d, 3);
	CHECK(!readRom(rom, d, &why) && why.find("class 3") != std::string::npos);
	rom = madeUpGen1(d);
	GameData d2 = d;
	d2.rom.erase("wildData");
	CHECK(!readRom(rom, d2, &why) && why.find("rom.wildData") != std::string::npos);
	d2 = d;
	d2.rom["superRodSlots"] = d2.rom["superRod"];
	CHECK(!readRom(rom, d2, &why) && why.find("superRod") != std::string::npos);
	d2 = d;
	d2.sizes["tm01"] = 0xC8;
	CHECK(!readRom(rom, d2, &why) && why.find("tm01") != std::string::npos);
	// ... Bulbasaur's picture in another bank, Mew's base data apart without its picture's bank, TownMapOrder not a list
	// or naming a map with no entry; a town map whose runs don't fill the screen is left undrawn
	d2 = d;
	d2.rom["pics3"] = d2.rom["pics2"];
	CHECK(!readRom(rom, d2, &why) && why.find("Bulbasaur's front picture") != std::string::npos);
	d2 = d;
	d2.rom.erase("mewPicFront");
	CHECK(!readRom(rom, d2, &why) && why.find("Mew") != std::string::npos);
	d2 = d;
	d2.rom["townMapOrderEnd"] = d2.rom["townMapOrder"];
	CHECK(!readRom(rom, d2, &why) && why.find("townMapOrder") != std::string::npos);
	d2 = d;
	d2.rom.erase("townMapTiles");
	CHECK(!readRom(rom, d2, &why) && why.find("rom.townMapTiles") != std::string::npos);
	auto bad = std::make_shared<std::vector<uint8_t>>(*rom);
	const size_t order = RomView{ *bad }.at(d.rom.at("townMapOrder"));
	(*bad)[order + 1] = 3;
	CHECK(!readRom(bad, d, &why) && why.find("TownMapOrder's map 3") != std::string::npos);
	bad = std::make_shared<std::vector<uint8_t>>(*rom);
	(*bad)[RomView{ *bad }.at(d.rom.at("townMap"))] = 0x12;
	r = readRom(bad, d, &why);
	CHECK(r && r->regionMaps.size() == 1 && r->regionMaps[0].rgba.empty() && r->regionMaps[0].cells.size() == 360);
}

static void testCaptures()
{
	checkCrystal("crystal", "crystal");
	checkCrystal("crystal_rev0", "crystal");
	checkCrystal("crystal_au", "crystal");

	Dump d;
	double now = 0;
	// a wild SENTRET (Lv3) against CHIKORITA (Lv5, 19/19, TACKLE / GROWL), out of the party's first slot
	if (d.load("crystal_battle"))
	{
		Reader r;
		r.open(game("crystal"));
		auto l = replay(r, d, now);
		const nds::Battle& b = l->battle;
		CHECK(l->valid && r.inBattle() && b.kind == nds::BattleKind::WILD);
		CHECK(b.ours[0].present && b.ours[0].mon.species == 152 && b.ours[0].mon.level == 5);
		CHECK(b.ours[0].mon.hp == 19 && b.ours[0].mon.maxHp == 19);
		CHECK(b.ours[0].mon.moves[0] == TACKLE && b.ours[0].mon.moves[1] == GROWL && !b.ours[0].mon.moves[2]);
		EQ(b.ours[0].mon.nick, "CHIKORITA");	// (the party's, under the battle's copy)
		CHECK(b.ours[0].types[0] == nds::GRASS && b.ours[0].types[1] == nds::GRASS);
		CHECK(b.foes[0].present && b.foes[0].mon.species == 161 && b.foes[0].mon.level == 3);
		CHECK(b.foes[0].types[0] == nds::NORMAL);
		CHECK(b.foeParty.empty() && b.trainerId == 0);
		for (int i = 0; i < 8; i++) CHECK(b.ours[0].stages[i] == 0 && b.foes[0].stages[i] == 0);
	}
	else failed++;
	// after CHIKORITA fainted: its HP 0 in the party
	if (d.load("crystal_battle_faint"))
	{
		Reader r;
		r.open(game("crystal"));
		auto l = replay(r, d, now);
		CHECK(l->valid && l->party.size() == 2 && l->party[0].hp == 0 && l->party[1].hp > 0);
	}
	else failed++;
	// Gold on ROUTE 29: GOLD (ID 54088) with CYNDAQUIL (Lv5, 17/20, TACKLE / LEER, a BERRY) and PIDGEY (Lv2, 13/13), a
	// POTION and three POKé BALLs, 3000 money, at (45, 11); Silver loads the same save
	for (const char* key : { "gold", "silver" })
	{
		if (!d.load(key)) { failed++; continue; }
		Reader r;
		r.open(game(key));
		auto l = replay(r, d, now);
		CHECK(l->valid);
		EQ(l->trainerName, "GOLD");
		CHECK(l->tid == 54088 && l->money == 3000);
		CHECK(l->party.size() == 2);
		if (l->party.size() == 2)
		{
			const nds::Mon& c = l->party[0];
			CHECK(c.species == 155 && c.level == 5 && c.hp == 17 && c.maxHp == 20 && c.item == BERRY);
			CHECK(c.moves[0] == TACKLE && c.moves[1] == LEER && c.metLevel == 0);	// (no caught data in Gold and Silver)
			const nds::Mon& p = l->party[1];
			CHECK(p.species == 16 && p.level == 2 && p.hp == 13 && p.maxHp == 13 && p.moves[0] == TACKLE);
		}
		const nds::BagItem* potion = inBag(*l, nds::POCKET_ITEMS, POTION);
		const nds::BagItem* balls = inBag(*l, nds::POCKET_BALLS, POKE_BALL);
		CHECK(potion && potion->count == 1 && balls && balls->count == 3);
		CHECK(l->zone == (24 << 8 | 3) && l->mapX == 45 && l->mapY == 11);
	}
	// Silver's wild SENTRET (0xA1, 13/13)
	if (d.load("silver_battle"))
	{
		Reader r;
		r.open(game("silver"));
		auto l = replay(r, d, now);
		CHECK(l->battle.kind == nds::BattleKind::WILD && l->battle.foes[0].mon.species == 161);
		CHECK(l->battle.foes[0].mon.hp == 13 && l->battle.foes[0].mon.maxHp == 13);
		CHECK(l->battle.ours[0].mon.species == 155 && l->battle.ours[0].mon.hp == 17);
	}
	else failed++;
	// a trainer battle (CHAMPION LANCE, class 0x10, number 1): GYARADOS (Lv44) out of six
	if (d.load("gold_trainer"))
	{
		Reader r;
		r.open(game("gold"));
		auto l = replay(r, d, now);
		const nds::Battle& b = l->battle;
		CHECK(b.kind == nds::BattleKind::TRAINER && b.trainerId == (0x10 << 8 | 1));
		const uint16_t team[] = { 130, 149, 149, 142, 6, 149 };
		CHECK(b.foeParty.size() == 6);
		for (size_t i = 0; i < 6 && i < b.foeParty.size(); i++) CHECK(b.foeParty[i].species == team[i]);
		CHECK(b.foes[0].mon.species == 130 && b.foes[0].mon.level == 44 && b.foes[0].present);
		CHECK(b.foes[0].types[0] == nds::WATER && b.foes[0].types[1] == nds::FLYING);
		CHECK(b.ours[0].mon.species == 155 && b.ours[0].mon.hp == 17);
	}
	else failed++;
	// the other battle captures read as battles, each with its party
	for (const char* c : { "crystal_battle_menu", "crystal_battle_party", "crystal_battle_switch_menu" })
	{
		if (!d.load(c)) { failed++; continue; }
		Reader r;
		r.open(game("crystal"));
		auto l = replay(r, d, now);
		CHECK(l->valid && l->party.size() == 2 && r.inBattle() && l->battle.kind == nds::BattleKind::WILD);
	}
	for (const char* c : { "gold_battle", "gold_battle_faint", "gold_battle_menu", "gold_battle_party", "gold_battle_switch_menu" })
	{
		if (!d.load(c)) { failed++; continue; }
		Reader r;
		r.open(game("gold"));
		auto l = replay(r, d, now);
		CHECK(l->valid && l->party.size() == 2 && r.inBattle() && l->battle.kind == nds::BattleKind::WILD);
		CHECK(l->battle.foes[0].mon.species == 16);
	}
}

static void testReaderRules()
{
	Dump d;
	if (!d.load("crystal")) { failed++; return; }
	// pacing: a set at once; nothing while it waits for its answer; once answered, the next 250 ms after the last
	{
		Reader r;
		r.open(game("crystal"));
		std::string q = r.requests(0);
		// (wPartyCount and its list, inside one of the set's reads: merged with its neighbours, from wMapGroup's)
		bool covered = false;
		for (size_t pos = 0, nl; (nl = q.find('\n', pos)) != std::string::npos; pos = nl + 1)
		{
			unsigned a = 0; int n = 0;
			if (sscanf(q.c_str() + pos, "READ_CORE_MEMORY %x %d", &a, &n) == 2 && a <= 0xDCD7 && 0xDCD7 + 8 <= a + (unsigned)n) covered = true;
		}
		CHECK(!q.empty() && covered);
		CHECK(r.requests(100).empty());
		std::string a = answer(q, d);
		r.feed(a.data(), a.size());
		CHECK(r.requests(200).empty());
		CHECK(!r.requests(250).empty());
		// (unanswered: nothing until 10 s have passed, then asked again)
		CHECK(r.requests(5000).empty());
		CHECK(!r.requests(10300).empty());
		// the same memory again: no new serial
		a = answer(r.requests(20000), d);
		CHECK(a.empty());	// (none was due: the last set still waits)
		Reader s;
		s.open(game("crystal"));
		double now = 0;
		replay(s, d, now);
		replay(s, d, now);
		CHECK(s.serial() == 1);
		d.d(0xDCE0) = 0x01;	// (CHIKORITA's item: a MASTER BALL)
		auto l = replay(s, d, now);
		CHECK(s.serial() == 2 && l->party.size() == 2 && l->party[0].item == 1);
		d.d(0xDCE0) = 0xAD;
		// the reads are merged: a set is far fewer lines than the keys
		size_t lines = 0;
		for (char c : q) lines += c == '\n';
		CHECK(lines > 5 && lines < 25);
	}
	// an emulator with no memory map: the error, and nothing valid
	{
		Reader r;
		r.open(game("crystal"));
		std::string q = r.requests(0), a;
		size_t pos = 0, nl;
		while ((nl = q.find('\n', pos)) != std::string::npos)
		{
			unsigned addr = 0;
			sscanf(q.c_str() + pos, "READ_CORE_MEMORY %x", &addr);
			char b[96];
			snprintf(b, sizeof b, "READ_CORE_MEMORY %x -1 no memory map defined\n", addr);
			a += b;
			pos = nl + 1;
		}
		r.feed("some other line\n", 16);
		r.feed(a.data(), a.size());
		EQ(r.error(), "no memory map defined");
		CHECK(!r.live(nullptr)->valid && r.serial() == 0);
	}
	// a party count of 7, a species list that doesn't match: not valid
	{
		Reader r;
		r.open(game("crystal"));
		double now = 0;
		d.d(0xDCD7) = 7;
		CHECK(!replay(r, d, now)->valid);
		d.d(0xDCD7) = 2;
		d.d(0xDCD9) = 0x13;		// (the list's PIDGEY made another species)
		CHECK(!replay(r, d, now)->valid);
		d.d(0xDCD9) = 16;
		CHECK(replay(r, d, now)->valid);
	}
	// DVs: a shiny set (Defense, Speed and Special 10, Attack 2, 3, 6, 7, 10, 11, 14 or 15) and a plain one, the HP
	// DV from the others' lowest bits, and the gender from the species' ratio
	{
		FakeRom rom;
		rom.species.resize(252);
		rom.species[152].name = "Chikorita";
		rom.species[152].genderRatio = 31;		// (GENDER_F12_5: female while Attack << 4 | Speed <= 31)
		rom.species[16].name = "Pidgey";
		rom.species[16].genderRatio = 127;
		Reader r;
		r.open(game("crystal"));
		double now = 0;
		const uint8_t dv0 = d.d(0xDCDF + 21), dv1 = d.d(0xDCDF + 22);
		d.d(0xDCDF + 21) = 0x2A; d.d(0xDCDF + 22) = 0xAA;	// Attack 2, Defense 10, Speed 10, Special 10: shiny
		auto l = replay(r, d, now, &rom);
		const nds::Mon& c = l->party[0];
		CHECK(c.shiny());
		CHECK(c.ivs[1] == 2 && c.ivs[2] == 10 && c.ivs[3] == 10 && c.ivs[4] == 10 && c.ivs[5] == 10);
		CHECK(c.ivs[0] == 0);		// (Attack 0, Defense 0, Speed 0, Special 0 in their lowest bits)
		CHECK(!c.female);			// (0x2A > 31: male)
		EQ(c.nick, "Chikorita");	// (no nickname of its own: the species' name as the pages write it)
		d.d(0xDCDF + 21) = 0x1A;	// Attack 1: not shiny; 0x1A = 26 <= 31: female; HP DV 1000 | 0 | 0 | 0 = 8
		l = replay(r, d, now, &rom);
		CHECK(!l->party[0].shiny() && l->party[0].female && l->party[0].ivs[0] == 8);
		d.d(0xDCDF + 21) = dv0; d.d(0xDCDF + 22) = dv1;
		// stat experience: HP, Attack, Defense, Speed, Special, big-endian at 11-20
		d.d(0xDCDF + 11) = 0x01; d.d(0xDCDF + 12) = 0x02; d.d(0xDCDF + 19) = 0xFF; d.d(0xDCDF + 20) = 0xFF;
		l = replay(r, d, now, &rom);
		CHECK(l->party[0].statExp[0] == 0x0102 && l->party[0].statExp[4] == 0xFFFF);
	}
}

// ---- Gen I's and Gen II's rules, through upstream's logic and its hooks ---------------------------------------------

static nds::Fighter fighter(uint16_t species, uint8_t level, nds::Type t1, nds::Type t2, std::initializer_list<uint16_t> stats,
	uint16_t item = 0, uint32_t status = 0)
{
	nds::Fighter f;
	f.species = species; f.level = level; f.item = item; f.status = status;
	f.types[0] = t1; f.types[1] = t2;
	int i = 0;
	for (uint16_t v : stats) f.stats[i++] = v;	// Attack, Defense, Speed, Sp. Atk, Sp. Def
	f.hp = f.maxHp = 100;
	return f;
}
static nds::MoveData move(nds::Type t, uint8_t power)
{
	nds::MoveData m;
	m.type = t; m.power = power;
	m.cat = t < nds::FIRE ? nds::MoveCategory::PHYSICAL : nds::MoveCategory::SPECIAL;
	return m;
}
#define RANGE(r, l, h) do { checks++; if ((r).lo != (l) || (r).hi != (h)) { failed++; fprintf(stderr, "gbpk-units: FAIL %s:%d: %s is %d-%d, not %d-%d\n", __FILE__, __LINE__, #r, (r).lo, (r).hi, (l), (h)); } } while (0)
#define NEAR(a, b) do { checks++; double a_ = (a), b_ = (b); if (std::fabs(a_ - b_) > 1e-6) { failed++; fprintf(stderr, "gbpk-units: FAIL %s:%d: %s is %.8f, not %.8f\n", __FILE__, __LINE__, #a, a_, b_); } } while (0)

static void testRules()
{
	using namespace nds;
	// the type chart through upstream's effectiveness(): Gen I's cells while its chart is in use, upstream's (Gen II's)
	// otherwise (pokered and pokecrystal data/types/type_matchups.asm)
	useRules(GEN_I);
	CHECK(effectiveness(GHOST, PSYCHIC, NO_TYPE) == 0 && effectiveness(BUG, POISON, NO_TYPE) == 8);
	CHECK(effectiveness(POISON, BUG, NO_TYPE) == 8 && effectiveness(ICE, FIRE, NO_TYPE) == 4);
	CHECK(effectiveness(FIGHTING, STEEL, NO_TYPE) == 4 && effectiveness(DARK, PSYCHIC, NO_TYPE) == 4);
	CHECK(effectiveness(WATER, FIRE, NO_TYPE) == 8 && effectiveness(NORMAL, GHOST, NO_TYPE) == 0);	// (the same in both)
	CHECK(oldRules());
	useRules(GEN_II);
	CHECK(effectiveness(GHOST, PSYCHIC, NO_TYPE) == 8 && effectiveness(BUG, POISON, NO_TYPE) == 2);
	CHECK(effectiveness(POISON, BUG, NO_TYPE) == 4 && effectiveness(ICE, FIRE, NO_TYPE) == 2);
	CHECK(effectiveness(FIGHTING, STEEL, NO_TYPE) == 8);
	CHECK(oldRules());
	useRules(Gen::IV);	// (the host puts it back: upstream's games untouched)
	CHECK(!oldRules() && effectiveness(GHOST, PSYCHIC, NO_TYPE) == 8);

	// damage through upstream's damage(), Gen II (pokecrystal effect_commands.asm):
	// Lv 5 TACKLE (35, Normal), Attack 11 against Defense 9, no STAB, neutral:
	//   ((2*5/5 + 2) * 35 * 11 / 9) / 50 = (4*35*11 = 1540, / 9 = 171) / 50 = 3, + 2 = 5; random 5*217/255 = 4 .. 5
	useRules(GEN_II);
	Fighter chiko = fighter(152, 5, GRASS, NO_TYPE, { 11, 13, 10, 11, 13 });
	Fighter sentret = fighter(161, 3, NORMAL, NO_TYPE, { 8, 9, 9, 7, 8 });
	DamageRange r = damage(GEN_II, move(NORMAL, 35), chiko, sentret);
	RANGE(r, 4, 5);
	CHECK(!r.stab && r.quarters == 4);
	// EMBER (40, Fire, special) at Lv 50, Sp. Atk 100 against Sp. Def 80, a Grass foe, STAB:
	//   (22*40*100 = 88000, / 80 = 1100) / 50 = 22, + 2 = 24; STAB + 12 = 36; x2 = 72; random 72*217/255 = 61 .. 72
	// with Charcoal: 22 * 110 / 100 = 24, + 2 = 26; STAB + 13 = 39; x2 = 78; random 78*217/255 = 66 .. 78
	Fighter cynda = fighter(155, 50, FIRE, NO_TYPE, { 90, 80, 100, 100, 85 });
	Fighter grass = fighter(152, 50, GRASS, NO_TYPE, { 80, 90, 80, 70, 80 });
	r = damage(GEN_II, move(FIRE, 40), cynda, grass);
	RANGE(r, 61, 72);
	CHECK(r.stab && r.quarters == 8);
	cynda.item = 249;
	r = damage(GEN_II, move(FIRE, 40), cynda, grass);
	RANGE(r, 66, 78);
	// Gen II's Dragon Fang does nothing (Dragon Scale is Dragon's); the same Ember with a Fire badge (Volcano, Kanto's
	// 7th: bit 14), + 1/8 before STAB: 24 + 3 = 27; + 13 = 40; x2 = 80; 80*217/255 = 68 .. 80
	cynda.item = 0;
	setBadges(1 << 14);
	r = damage(GEN_II, move(FIRE, 40), cynda, grass);
	RANGE(r, 68, 80);
	setBadges(1 << 0);	// (Zephyr: Flying's, not Fire's)
	r = damage(GEN_II, move(FIRE, 40), cynda, grass);
	RANGE(r, 61, 72);
	setBadges(0);
	// burned: the battle's Attack already halved by the game, nothing more (a burned and an unburned with the same
	// stats deal the same)
	Fighter burned = cynda;
	burned.status = 0x10;
	DamageRange r1 = damage(GEN_II, move(NORMAL, 35), cynda, grass), r2 = damage(GEN_II, move(NORMAL, 35), burned, grass);
	CHECK(r1.lo == r2.lo && r1.hi == r2.hi && r1.hi > 0);
	// PIKACHU's Light Ball doubles its Sp. Atk: THUNDERSHOCK (40, Electric) at Lv 50, Sp. Atk 50 against a Water foe's
	// Sp. Def 50: with it (22*40*100/50 = 1760) / 50 = 35, + 2 = 37; STAB + 18 = 55; x2 = 110; 110*217/255 = 93 .. 110;
	// without (22*40*50/50 = 880) / 50 = 17, + 2 = 19; + 9 = 28; x2 = 56; 56*217/255 = 47 .. 56
	Fighter pika = fighter(25, 50, ELECTRIC, NO_TYPE, { 55, 40, 90, 50, 50 }, LIGHT_BALL);
	Fighter water = fighter(7, 50, WATER, NO_TYPE, { 50, 60, 40, 50, 50 });
	r = damage(GEN_II, move(ELECTRIC, 40), pika, water);
	RANGE(r, 93, 110);
	pika.item = 0;
	r = damage(GEN_II, move(ELECTRIC, 40), pika, water);
	RANGE(r, 47, 56);
	// both above 255: / 4 (Attack 300 -> 75, Defense 280 -> 70): (22*35*75 = 57750, / 70 = 825) / 50 = 16, + 2 = 18;
	// 18*217/255 = 15 .. 18
	Fighter big = fighter(76, 50, ROCK, GROUND, { 300, 300, 100, 100, 100 }), wall = fighter(208, 50, STEEL, GROUND, { 100, 280, 50, 60, 70 });
	r = damage(GEN_II, move(NORMAL, 35), big, wall);
	CHECK(r.quarters == 2);	// (Normal on Steel: / 2)
	// (18 / 2 = 9; 9*217/255 = 7 .. 9)
	RANGE(r, 7, 9);
	// a status move, and a type that does nothing
	CHECK(damage(GEN_II, MoveData{}, cynda, grass).hi == 0);
	r = damage(GEN_II, move(NORMAL, 35), chiko, fighter(92, 5, GHOST, POISON, { 5, 5, 5, 5, 5 }));
	CHECK(r.quarters == 0 && r.hi == 0);

	// Gen I (pokered engine/battle/core.asm): one Special stat, the reader's Sp. Atk and Sp. Def both; PSYCHIC (90) at
	// Lv 50, Special 135 against Special 90 (Attack 50 and Defense 200 not used), STAB, neutral:
	//   (22*90*135 = 267300, / 90 = 2970) / 50 = 59, + 2 = 61; + 30 = 91; 91*217/255 = 77 .. 91
	useRules(GEN_I);
	Fighter kadabra = fighter(65, 50, PSYCHIC, NO_TYPE, { 50, 45, 120, 135, 135 });
	Fighter normal = fighter(20, 50, NORMAL, NO_TYPE, { 80, 200, 90, 90, 90 });
	r = damage(GEN_I, move(PSYCHIC, 90), kadabra, normal);
	RANGE(r, 77, 91);
	// no held items in Gen I: Charcoal's id changes nothing; Ghost on Psychic does nothing there
	Fighter charm = fighter(4, 50, FIRE, NO_TYPE, { 60, 50, 70, 60, 60 }, 249);
	Fighter charm0 = charm; charm0.item = 0;
	r1 = damage(GEN_I, move(FIRE, 40), charm, normal); r2 = damage(GEN_I, move(FIRE, 40), charm0, normal);
	CHECK(r1.lo == r2.lo && r1.hi == r2.hi);
	r = damage(GEN_I, move(GHOST, 20), fighter(92, 30, GHOST, POISON, { 40, 40, 80, 100, 100 }), kadabra);
	CHECK(r.quarters == 0 && r.hi == 0);
	// Speed: the battle's own (paralysis already in it), whatever the stages say
	Fighter slow = kadabra;
	slow.status = 0x40; slow.stages[3] = 2;
	CHECK(effectiveSpeed(GEN_I, slow) == 120 && effectiveSpeed(GEN_II, slow) == 120);

	// catching, through upstream's catchChance()
	FakeRom rom;
	rom.species.resize(252);
	rom.species[1].catchRate = 255;
	rom.species[152].catchRate = 45;
	rom.species[152].genderRatio = 31;
	Mon wild;
	wild.species = 1; wild.level = 5; wild.hp = wild.maxHp = 20;
	CatchContext c;
	// Gen I, catch rate 255 at full HP (20) with a Poké Ball: R1 in 0..255 never above the rate (256 of 256);
	// W = (20*255 / 12 = 425) / (20/4 = 5) = 85; then R2 <= 85: 86/256 = 0.3359375
	useRules(GEN_I);
	NEAR(catchChance(GEN_I, rom, wild, POKE_BALL, c), 86.0 / 256);
	// Gen I, rate 45 at full HP (30), asleep, with an Ultra Ball: R1 in 0..150; under 25 caught (25 of 151); 25..70 pass
	// (46); W = (30*255/12 = 637) / (30/4 = 7) = 91: (25 + 46 * 92/256) / 151
	wild.species = 152; wild.hp = wild.maxHp = 30; wild.status = 3;
	NEAR(catchChance(GEN_I, rom, wild, ULTRA_BALL, c), (25 + 46 * 92.0 / 256) / 151);
	CHECK(catchChance(GEN_I, rom, wild, MASTER_BALL, c) == 1);
	// Gen II, rate 45 at 1 HP of 30, asleep, with an Ultra Ball: 45*2 = 90; 3*30 = 90, 2*1 = 2: 90*(90-2)/90 = 88;
	// asleep + 10 = 98: 99/256
	useRules(GEN_II);
	wild.hp = 1;
	NEAR(catchChance(GEN_II, rom, wild, ULTRA_BALL, c), 99.0 / 256);
	CHECK(catchChance(GEN_II, rom, wild, MASTER_BALL, c) == 1);
	// burned: nothing added (the game's bug); full HP, Poké Ball: 45*(90-60)/90 = 15: 16/256
	wild.hp = 30; wild.status = 0x10;
	NEAR(catchChance(GEN_II, rom, wild, POKE_BALL, c), 16.0 / 256);
	// the Level Ball, Lv 20 against Lv 5 (5 < 20/2, not < 20/4): 45*2*2 = 180, no HP part: 181/256
	wild.level = 5; c.ourLevel = 20;
	NEAR(catchChance(GEN_II, rom, wild, LEVEL_BALL, c), 181.0 / 256);
	// the Moon Ball never boosts (the game's bug): as a Poké Ball
	NEAR(catchChance(GEN_II, rom, wild, MOON_BALL, c), 16.0 / 256);
	// max HP above 341: 3*400 = 1200 / 4 = 300, low byte 44; 2*400 = 800 / 4 = 200; (44 - 200) & 0xFF = 100:
	// 45*100/44 = 102 (the game's bug: a full-HP catch looks easier): 103/256
	wild.status = 0; wild.hp = wild.maxHp = 400;
	NEAR(catchChance(GEN_II, rom, wild, POKE_BALL, c), 103.0 / 256);
	useRules(Gen::IV);

	// Hidden Power (pokecrystal engine/battle/hidden_power.asm): DVs all 15: power (15*5 + 3) / 2 + 31 = 70, type
	// (3 << 2 | 3) = 15, Dark; Attack 12, Defense 13, Speed 6, Special 2: power (12*5 + 2) / 2 + 31 = 62, type
	// (0 << 2 | 1) = 1, Flying
	const uint8_t all15[6] = { 15, 15, 15, 15, 15, 15 }, other[6] = { 0, 12, 13, 6, 2, 2 };
	CHECK(hiddenPowerPower(all15) == 70 && hiddenPowerType(all15) == DARK);
	CHECK(hiddenPowerPower(other) == 62 && hiddenPowerType(other) == FLYING);
	// a stat (CalcMonStatC): HP base 45, DV 8, no stat experience, Lv 5: (106 * 5 / 100 = 5) + 5 + 10 = 20; Attack base
	// 100, DV 15, 65535 stat experience (root 255), Lv 100: (230 + 63) * 100 / 100 + 5 = 298; 100 experience: root 10
	CHECK(statFrom(45, 8, 0, 5, true) == 20);
	CHECK(statFrom(100, 15, 65535, 100, false) == 298);
	CHECK(statFrom(100, 15, 100, 100, false) == 237);
}

int main(int argc, char** argv)
{
	if (argc < 3)
	{
		fprintf(stderr, "usage: gbpk-units <the data files' folder> <kiwami/tests/gbpk>\n");
		return 2;
	}
	dataPath = argv[1];
	testDir = argv[2];
	setenv("KIWAMI_GBPK_DIR", dataPath.c_str(), 1);
	testDetect();
	testData();
	testConfirm();
	testRomFile();
	testText();
	testRomList();
	testRomTables();
	testLz();
	testPictureFormats();
	testPic1();
	testTownMapCells();
	testCaptures();
	testGen1Captures();
	testGen1RomTables();
	testDerivedFlags();
	testGuide();
	testRomGuide();
	testReaderRules();
	testRules();
	if (failed)
	{
		fprintf(stderr, "gbpk-units: %d of %d checks failed\n", failed, checks);
		return 1;
	}
	printf("gbpk-units: %d checks passed\n", checks);
	return 0;
}
