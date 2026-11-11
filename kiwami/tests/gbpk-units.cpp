// The Game Boy Pokémon companion's code that needs no screen (ui/src/kiwami/gbpk/), checked on its own: which game a
// ROM is, its data files, its text, and its memory reader replayed over PokeDaisy's RAM captures (kiwami/tests/gbpk/
// fixtures/). Built and run by kiwami/tests/ci.sh with the host's g++ (the vendored C first, make -C ui kiwami-c, for
// the 7z reader):
//   g++ -std=c++17 -Wall -Werror -I ui/src kiwami/tests/gbpk-units.cpp ui/src/kiwami/gbpk/*.cpp ui/src/kiwami/json.cpp
//       ui/src/kiwami/ra7z.cpp ui/src/zip.cpp ui/src/zipdir.cpp ui/src/nds/nitro.cpp ui/src/nds/model.cpp
//       ui/src/nds/logic.cpp ui/src/pokemon.cpp ui/src/pokemap.cpp ui/src/kiwami/third_party/lzma/*.kiwami.o -lz -lpthread
//       -o gbpk-units
// The rules' expected values are worked by hand, in the comments, from the games' own code in pret's disassemblies.
//   ./gbpk-units kiwami/rootfs/usr/share/banana/companions/gb-pokemon kiwami/tests/gbpk
// Copyright (C) 2026 Lidor Itzhari (PokeDaisy) and the kiwami fork's contributors. GNU GPL version 3 with PokeDaisy's
// additional term under section 7(b): LICENSE and NOTICE in ui/src/kiwami/gbpk/. SPDX-License-Identifier: GPL-3.0-only
// Based on PokeDaisy by Lidor Itzhari - https://github.com/lidor30/pokedaisy
#include "kiwami/gbpk/games.h"
#include "kiwami/gbpk/reader.h"
#include "kiwami/gbpk/rules.h"
#include "kiwami/gbpk/text.h"
#include "nds/logic.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
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
	// Red has no data file until its memory is read
	h = header("POKEMON RED");
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

	// refusals: a game it doesn't know, a missing file
	CHECK(!loadData("pinball", d, &why));
	CHECK(why.find("not one of the six") != std::string::npos);
	CHECK(!loadData("red", d, &why));
	CHECK(why.find("can't be read") != std::string::npos);
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

	EQ(nameCase("CHIKORITA"), "Chikorita");
	EQ(nameCase("DOUBLE-EDGE"), "Double-Edge");
	EQ(nameCase("MR.MIME"), "Mr.Mime");
	EQ(nameCase("FARFETCH'D"), "Farfetch'd");
	EQ(nameCase("HI JUMP KICK"), "Hi Jump Kick");
	EQ(nameCase("NIDORAN♀"), "Nidoran♀");
	EQ(nameCase("PORYGON2"), "Porygon2");
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
		bool ok = file("wram.bin", wram) && file("hram.bin", hram) && file("wramx.bin", wramx);
		if (!ok) fprintf(stderr, "gbpk-units: no capture %s\n", name.c_str());
		return ok && wram.size() == 0x2000 && wramx.size() == 0x8000;
	}
	// a byte as the cores' memory maps give READ_CORE_MEMORY: C000-CFFF bank 0, D000-DFFF bank 1, HRAM, and banks
	// 2-7 from 0x10000
	int at(uint32_t a) const
	{
		if (a >= 0xC000 && a < 0xD000) return wram[a - 0xC000];
		if (a >= 0xD000 && a < 0xE000) return wramx[0x1000 + a - 0xD000];
		if (a >= 0xFF80 && a < 0xFFFF) return hram[a - 0xFF80];
		if (a >= 0x10000 && a < 0x16000) return wramx[0x2000 + a - 0x10000];
		return -1;
	}
	uint8_t& d(uint16_t a) { return wramx[0x1000 + a - 0xD000]; }	// (bank 1's byte, to change a capture)
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
	testCaptures();
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
