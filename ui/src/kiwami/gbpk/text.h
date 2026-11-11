#pragma once
// banana-ui (kiwami): the Game Boy Pokémon games' text, in their English character set: names in the party, the
// trainer's, and the ROM's species, move and item names.
// Copyright (C) 2026 Lidor Itzhari (PokeDaisy) and the kiwami fork's contributors. GNU GPL version 3 with PokeDaisy's
// additional term under section 7(b): LICENSE and NOTICE in this folder. SPDX-License-Identifier: GPL-3.0-only
// Based on PokeDaisy by Lidor Itzhari - https://github.com/lidor30/pokedaisy
#include <cstddef>
#include <cstdint>
#include <string>

namespace banana::kiwami::gbpk
{
	// max bytes at most, up to the terminator (0x50), as UTF-8; bytes with no character (the text's commands, the
	// Japanese) are left out. gen: 1 or 2
	std::string decode(int gen, const uint8_t* p, size_t max);
	// a name the games write in capitals as the pages write names: each word's first letter kept, the rest lower
	// case ("CHIKORITA" to "Chikorita", "DOUBLE-EDGE" to "Double-Edge", "MR.MIME" to "Mr.Mime", "FARFETCH'D" to
	// "Farfetch'd"): for species and move names
	std::string nameCase(const std::string& s);
}
