#pragma once
// banana-ui (kiwami): the Game Boy Pokémon companion's place in upstream's DS companion model (ui/src/nds/model.h):
// Generations I and II, and the six games, as values of upstream's enums. Header-only: upstream's own test binaries
// (ui/Makefile) link ui/src/nds/ without the fork's sources, so what its hooks call is defined here, inline.
// Copyright (C) 2026 Lidor Itzhari (PokeDaisy) and the kiwami fork's contributors. GNU GPL version 3 with PokeDaisy's
// additional term under section 7(b): LICENSE and NOTICE in this folder. SPDX-License-Identifier: GPL-3.0-only
// Based on PokeDaisy by Lidor Itzhari - https://github.com/lidor30/pokedaisy
#include "../../nds/model.h"

namespace banana::kiwami::gbpk
{
	// nds::Gen and nds::GameId are enum classes over int, so these values are valid ones of theirs that upstream's
	// enumerators never take: where upstream compares with its own (g != Gen::V), these take its Gen III/IV path, and
	// the fork's hooks replace what differs (kiwami/GB-POKEMON.md)
	constexpr nds::Gen GEN_I = (nds::Gen)16, GEN_II = (nds::Gen)17;
	constexpr nds::GameId RED = (nds::GameId)100, BLUE = (nds::GameId)101, YELLOW = (nds::GameId)102,
		GOLD = (nds::GameId)103, SILVER = (nds::GameId)104, CRYSTAL = (nds::GameId)105;

	inline bool oldGen(nds::Gen g) { return g == GEN_I || g == GEN_II; }
	inline bool gbGame(nds::GameId id) { return (int)id >= (int)RED && (int)id <= (int)CRYSTAL; }
}
