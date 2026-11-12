#pragma once
// banana-ui (kiwami): the Game Boy Pokémon companion's games. Which of the six a ROM is: its header's title against
// each data file's header_title, then the whole file's SHA-1 against the file's releases (a known title with another
// SHA-1 is a release or a ROM hack the companion doesn't read: "Not supported"). The data files are
// /usr/share/banana/companions/gb-pokemon/<game>.ram.json, extracted from PokeDaisy and pret by
// kiwami/tools/pokedaisy-extract.py (their schema: kiwami/GB-POKEMON.md).
// Copyright (C) 2026 Lidor Itzhari (PokeDaisy) and the kiwami fork's contributors. GNU GPL version 3 with PokeDaisy's
// additional term under section 7(b): LICENSE and NOTICE in this folder. SPDX-License-Identifier: GPL-3.0-only
// Based on PokeDaisy by Lidor Itzhari - https://github.com/lidor30/pokedaisy
#include "gbpk.h"
#include <cstdint>
#include <map>
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
	struct Release { std::string sha1, name, code; };	// code: the header's game code (the Game Boy Color games')

	// the guide's flags (Live::flags, its done_when "flag:N"): a bank of the save's flags at a ram key's address, its
	// bytes laid in from bit `first`; and a bit worked out from those, set when all (or any) of `of` are (Crystal's
	// Elite Four: the League won or the room's flag, which the game clears)
	struct FlagBank { std::string ram; int bytes = 0, first = 0; };
	struct DerivedFlag { int bit = 0; bool all = false; std::vector<int> of; };

	// one game's data file
	struct GameData
	{
		std::string key;					// "crystal": the file's name, and the guide's
		nds::Game game;						// id, gen, title and dexSize; code and romHash once a ROM is confirmed
		int gen = 0;						// 1 or 2
		std::string headerTitle;			// the header's title at 0x134 ("PM_CRYSTAL")
		std::vector<Release> releases;
		std::map<std::string, Addr> ram, rom;
		std::map<std::string, int> sizes;
		std::vector<FlagBank> flags;		// the banks the guide reads (none: no guide flags)
		std::vector<DerivedFlag> derived;
		int release = -1;					// the confirmed ROM's, in releases

		Addr at(const std::string& k) const { auto i = ram.find(k); return i == ram.end() ? Addr{} : i->second; }
		int size(const std::string& k, int def = 0) const { auto i = sizes.find(k); return i == sizes.end() ? def : i->second; }
	};

	// <dataDir>/<key>.ram.json into out; false with the reason in why (a missing file, another schema or kind, a
	// game key it doesn't know, an address outside WRAM and HRAM, a flag bank or a derived bit out of place)
	bool loadData(const std::string& key, GameData& out, std::string* why);

	enum class Match { NONE, KNOWN, UNSUPPORTED };
	// the game whose header_title the header has (key: its file's name): KNOWN, or NONE (not one of the six, or no
	// data file for it). The data files are read once, the first time.
	Match detect(const uint8_t* header, size_t n, std::string& key);
	// the whole ROM's SHA-1 among d's releases: KNOWN, with d.release, d.game.romHash and d.game.code set; else
	// UNSUPPORTED (another release, a ROM hack, a bad dump), with d.game.romHash still set for the log
	Match confirm(const std::vector<uint8_t>& rom, GameData& d);
}
