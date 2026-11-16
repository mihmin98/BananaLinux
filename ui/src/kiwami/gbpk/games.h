#pragma once
// banana-ui (kiwami): the Game Boy Pokémon companion's games. Which one a ROM is: its header's title against each data
// file's header_title, which gives every game with that title (a ROM hack keeps its base game's), then the whole
// file's SHA-1 against those games' releases, which picks one (a known title with another SHA-1 is a release or a ROM
// hack the companion doesn't read: "Not supported"). The data files are
// /usr/share/banana/companions/gb-pokemon/<game>.ram.json, extracted from PokeDaisy and pret by
// kiwami/tools/pokedaisy-extract.py (their schema: kiwami/GB-POKEMON.md).
// Copyright (C) 2026 Lidor Itzhari (PokeDaisy) and the kiwami fork's contributors. GNU GPL version 3 with PokeDaisy's
// additional term under section 7(b): LICENSE and NOTICE in this folder. SPDX-License-Identifier: GPL-3.0-only
// Based on PokeDaisy by Lidor Itzhari - https://github.com/lidor30/pokedaisy
#include "gbpk.h"
#include "text.h"
#include <array>
#include <bitset>
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace banana::kiwami::gbpk
{
	// a ROM's bytes: the file, or the first .gb/.gbc in a .zip or .7z; limit: the first that many at most (the header:
	// 0x150). Empty when there is no game to read.
	std::vector<uint8_t> readRomFile(const std::string& path, size_t limit);

	// the data files' folder: $KIWAMI_GBPK_DIR, else the image's
	std::string dataDir();

	// a WRAM or HRAM address as the data files give it: bank 0 for C000-CFFF and HRAM, 1 for the D000 bank the cores
	// map there, 2-7 another WRAM bank
	struct Addr
	{
		int bank = 0;
		uint16_t addr = 0;
		bool ok() const { return addr != 0; }
		// the address READ_CORE_MEMORY takes: the cores' memory maps put WRAM banks 2-7 at 0x10000 up
		uint32_t core() const { return bank >= 2 ? 0x10000u + (uint32_t)(bank - 2) * 0x1000u + (addr - 0xD000u) : addr; }
	};
	// a release: its SHA-1, name and header's game code (the Game Boy Color games'); and the addresses it has of its own,
	// where a ROM hack's versions disagree (each rebuilt: PureRGB's v2.7.6 and v2.7.7), laid over the game's when a ROM
	// is this release (pickRelease)
	struct Release { std::string sha1, name, code; std::map<std::string, Addr> ram, rom; };

	// the guide's flags (Live::flags, its done_when "flag:N"): a bank of the save's flags at a ram key's address, its
	// bytes laid in from bit `first`; and a bit worked out from those, set when all (or any) of `of` are (Crystal's
	// Elite Four: the League won or the room's flag, which the game clears)
	struct FlagBank { std::string ram; int bytes = 0, first = 0; };
	struct DerivedFlag { int bit = 0; bool all = false; std::vector<int> of; };

	// one game's data file
	struct GameData
	{
		std::string key;					// "crystal": the file's name, and the guide's
		nds::Game game;						// id, gen, title and dexSize (the file's dex_size, else 151 or 251); code
											// and romHash once a ROM is confirmed
		int gen = 0;						// 1 or 2
		std::string headerTitle;			// the header's title at 0x134 ("PM_CRYSTAL")
		std::vector<Release> releases;
		std::map<std::string, Addr> ram, rom;
		std::map<std::string, int> sizes;
		std::vector<FlagBank> flags;		// the banks the guide reads (none: no guide flags)
		std::vector<DerivedFlag> derived;
		// the game's items and types, by its own ids (pret's constants: kiwami/tools/gbpk-symbols.txt): the items
		// upstream's logic knows by their Gen IV id, the balls, Gen I's key items (its one bag list's pockets), each
		// type id's nds::Type (NO_TYPE where the game has none), and its physical/special split: the special types,
		// and those whose category goes by the attacker's base Attack and Special
		std::map<uint8_t, uint16_t> items;
		std::set<uint8_t> balls, keyItems;
		std::array<nds::Type, 256> types;
		std::bitset<256> special, splitStats;
		bool pureText = false;				// its text in PureRGB's character set (the file's "charset": "pure")
		bool romGuide = false;				// its guide made from the ROM's trainers (gsguide.h), none in a file
		bool guideRegions = false;			// in its regions' order (the file's "guide": "rom-regions": Crystal Clear's)
		bool romLevels = false;				// its guide file's bosses at the levels of the ROM's teams (the file's
											// "guide_levels": "rom": a ROM hack's guide converted from a retail game's)
		int release = -1;					// the confirmed ROM's, in releases

		GameData() { types.fill(nds::NO_TYPE); }

		Addr at(const std::string& k) const { auto i = ram.find(k); return i == ram.end() ? Addr{} : i->second; }
		// its text's character set: the file's, else its generation's
		Charset charset() const { return pureText ? Charset::PURE : gen == 1 ? Charset::GEN1 : Charset::GEN2; }
		int size(const std::string& k, int def = 0) const { auto i = sizes.find(k); return i == sizes.end() ? def : i->second; }
	};

	// <dataDir>/<key>.ram.json into out; false with the reason in why (a missing file, another schema or kind, a
	// game key it doesn't know, an address outside WRAM and HRAM, a flag bank or a derived bit out of place, a
	// dex_size it can't use, no items or types, a type, a character set, a guide or guide levels it doesn't know)
	bool loadData(const std::string& key, GameData& out, std::string* why);
	// d's release i as the one the ROM is: d.release, and the release's own addresses over the game's
	void pickRelease(GameData& d, int i);
	// the release a PokeDaisy capture is of, for a game with no ROM to tell (the tests' stand-in): the one whose SHA-1
	// starts with what its README.txt names ("sha1 2e94d09c..."), else the first
	int captureRelease(const GameData& d, const std::string& readme);
	// each key's data file, in order; one that doesn't load is left out, its reason added to why ("; " between)
	std::vector<GameData> loadGames(const std::vector<std::string>& keys, std::string* why);
	// the game a PokeDaisy capture's folder is for (kiwami/tests/gbpk/fixtures/<name>): the longest key its name starts
	// with, "_" read as "-", up to its end or a "_" (crystal_clear_v2_5_11_battle: crystal-clear; crystal_battle:
	// crystal); "" when none
	std::string fixtureKey(const std::string& name);
	// a type by the data files' name for it ("normal" .. "dark" in nds::Type's order, and the fork's own, gbpk.h's:
	// "typeless" .. "magma"); NO_TYPE for a name it doesn't know
	nds::Type typeByName(const std::string& name);

	enum class Match { NONE, KNOWN, UNSUPPORTED };
	// the games whose header_title the header has (keys: their files' names, the retail game first, as the games are
	// listed): KNOWN, or NONE (no game has it, or no data file). The data files are read once, the first time.
	Match detect(const uint8_t* header, size_t n, std::vector<std::string>& keys);
	// the whole ROM's SHA-1 among d's releases: KNOWN, with d.release, d.game.romHash and d.game.code set; else
	// UNSUPPORTED (another release, a ROM hack, a bad dump), with d.game.romHash still set for the log
	Match confirm(const std::vector<uint8_t>& rom, GameData& d);
	// the same among several games (detect's): the index of the one whose release the ROM is, confirmed as above;
	// else -1, with each one's romHash set
	int confirmAny(const std::vector<uint8_t>& rom, std::vector<GameData>& ds);
}
