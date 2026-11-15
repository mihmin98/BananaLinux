#pragma once
// banana-ui (kiwami): the Game Boy Pokémon games' text, in their English character set: names in the party, the
// trainer's, and the ROM's species, move and item names, Pokédex pages and descriptions.
// Copyright (C) 2026 Lidor Itzhari (PokeDaisy) and the kiwami fork's contributors. GNU GPL version 3 with PokeDaisy's
// additional term under section 7(b): LICENSE and NOTICE in this folder. SPDX-License-Identifier: GPL-3.0-only
// Based on PokeDaisy by Lidor Itzhari - https://github.com/lidor30/pokedaisy
#include <cstddef>
#include <cstdint>
#include <string>

namespace banana::kiwami::gbpk
{
	// the character sets: Generation I's and II's (their constants/charmap.asm), and PureRGB's, Generation I's with its
	// one-byte words, quotes and signs (a data file's "charset": "pure")
	enum class Charset { GEN1 = 1, GEN2 = 2, PURE };
	// max bytes at most, up to the terminator (0x50), as UTF-8; the line breaks and breakable spaces as a space, bytes
	// with no character (the text's other commands, the Japanese) left out
	std::string decode(Charset cs, const uint8_t* p, size_t max);
	// a ROM hack's packed text, Crystal Clear's: PlaceNextChar's table (at the file offset `table` in the ROM) of three
	// bytes for each byte from `first` to 0x5F but 0x1F (a space) and 0x50 (the end). 0 and a pointer into bank 0: a
	// word, expanded in place (itself packed); 0 and a pointer past bank 0: a name in WRAM (<PLAYER>, <RIVAL>), and 1
	// and a routine for one of the character set's breaks (<LF>, <WBR>, <NEXT>, <LINE>, <PARA>, <CONT>), each as the
	// character set decodes its byte. Any other routine stops the text there, logged once
	struct TextDict { const uint8_t* rom = nullptr; size_t size = 0, table = 0; int first = 0; };
	std::string decode(Charset cs, const uint8_t* p, size_t max, const TextDict& dict);
	// a name the games write in capitals as the pages write names: each word's first letter kept, the rest lower
	// case ("CHIKORITA" to "Chikorita", "DOUBLE-EDGE" to "Double-Edge", "MR.MIME" to "Mr.Mime", "FARFETCH'D" to
	// "Farfetch'd"), with HP, PP, PKMN, GS, PC, TV and "TM01" kept ("HP UP" to "HP Up"): for the names of species,
	// moves, items, places and trainers
	std::string nameCase(const std::string& s);
}
