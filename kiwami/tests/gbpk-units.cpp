// The Game Boy Pokémon companion's code that needs no screen (ui/src/kiwami/gbpk/), checked on its own: which game a
// ROM is, its data files, its text, and its memory reader replayed over PokeDaisy's RAM captures (kiwami/tests/gbpk/
// fixtures/). Built and run by kiwami/tests/ci.sh with the host's g++ (the vendored C first, make -C ui kiwami-c, for
// the 7z reader):
//   g++ -std=c++17 -Wall -Werror -I ui/src kiwami/tests/gbpk-units.cpp ui/src/kiwami/gbpk/*.cpp ui/src/kiwami/json.cpp
//       ui/src/kiwami/ra7z.cpp ui/src/zip.cpp ui/src/zipdir.cpp ui/src/nds/nitro.cpp ui/src/nds/model.cpp
//       ui/src/pokemon.cpp ui/src/pokemap.cpp ui/src/kiwami/third_party/lzma/*.kiwami.o -lz -lpthread -o gbpk-units
//   ./gbpk-units kiwami/rootfs/usr/share/banana/companions/gb-pokemon kiwami/tests/gbpk
// Copyright (C) 2026 Lidor Itzhari (PokeDaisy) and the kiwami fork's contributors. GNU GPL version 3 with PokeDaisy's
// additional term under section 7(b): LICENSE and NOTICE in ui/src/kiwami/gbpk/. SPDX-License-Identifier: GPL-3.0-only
// Based on PokeDaisy by Lidor Itzhari - https://github.com/lidor30/pokedaisy
#include "kiwami/gbpk/games.h"
#include "kiwami/gbpk/reader.h"
#include "kiwami/gbpk/text.h"
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

static const uint16_t BERRY = 1000 + 0xAD, POTION = 1000 + 0x12, POKE_BALL = 4, TACKLE = 33, GROWL = 45, LEER = 43;

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
	if (failed)
	{
		fprintf(stderr, "gbpk-units: %d of %d checks failed\n", failed, checks);
		return 1;
	}
	printf("gbpk-units: %d checks passed\n", checks);
	return 0;
}
