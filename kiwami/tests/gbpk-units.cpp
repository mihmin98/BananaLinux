// The Game Boy Pokémon companion's code that needs no screen (ui/src/kiwami/gbpk/), checked on its own: which game a
// ROM is, its data files, its text. Built and run by kiwami/tests/ci.sh with the host's g++ (the vendored C first,
// make -C ui kiwami-c, for the 7z reader):
//   g++ -std=c++17 -Wall -Werror -I ui/src kiwami/tests/gbpk-units.cpp ui/src/kiwami/gbpk/*.cpp ui/src/kiwami/json.cpp
//       ui/src/kiwami/ra7z.cpp ui/src/zip.cpp ui/src/zipdir.cpp ui/src/nds/nitro.cpp
//       ui/src/kiwami/third_party/lzma/*.kiwami.o -lz -o gbpk-units
//   ./gbpk-units kiwami/rootfs/usr/share/banana/companions/gb-pokemon kiwami/tests/gbpk
// Copyright (C) 2026 Lidor Itzhari (PokeDaisy) and the kiwami fork's contributors. GNU GPL version 3 with PokeDaisy's
// additional term under section 7(b): LICENSE and NOTICE in ui/src/kiwami/gbpk/. SPDX-License-Identifier: GPL-3.0-only
// Based on PokeDaisy by Lidor Itzhari - https://github.com/lidor30/pokedaisy
#include "kiwami/gbpk/games.h"
#include "kiwami/gbpk/text.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
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
	if (failed)
	{
		fprintf(stderr, "gbpk-units: %d of %d checks failed\n", failed, checks);
		return 1;
	}
	printf("gbpk-units: %d checks passed\n", checks);
	return 0;
}
