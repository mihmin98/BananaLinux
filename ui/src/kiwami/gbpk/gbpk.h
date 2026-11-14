#pragma once
// banana-ui (kiwami): the Game Boy Pokémon companion's place in upstream's DS companion model (ui/src/nds/model.h):
// Generations I and II, and the games (the six and the ROM hacks read as games of their own), as values of upstream's
// enums. Header-only: upstream's own test binaries
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
	// the ROM hacks (kiwami/GB-POKEMON.md, The ROM hacks)
	constexpr nds::GameId CRYSTAL_LEGACY = (nds::GameId)106, YELLOW_LEGACY = (nds::GameId)107, PURERED = (nds::GameId)108,
		PUREBLUE = (nds::GameId)109, PUREGREEN = (nds::GameId)110, CRYSTAL_CLEAR = (nds::GameId)111;

	// the fork's own types, past upstream's 18 (nds::Type 0-17): PureRGB's six (its constants/type_constants.asm at
	// v2.7.7: Struggle's, Hardened Onix's, Bonemerang's, Tri Attack's, the Floating forms', Volcanic Magmar's). Values
	// of upstream's enum it never takes, as GEN_I is of nds::Gen: upstream's loops stop at NUM_TYPES, its names give ""
	// and its colours grey past it, and three marked hooks (ui/src/nds/pages.cpp's typeChip, ui/src/nds/logic.cpp's
	// typeName) name and colour these instead. rules.h's chart has cells for them
	constexpr nds::Type TYPELESS = (nds::Type)32, CRYSTAL_TYPE = (nds::Type)33, BONEMERANG = (nds::Type)34, TRI = (nds::Type)35,
		FLOATING = (nds::Type)36, MAGMA = (nds::Type)37;
	// a fork's type's name, nullptr for any other type. Bonemerang's is Ground, as PureRGB's TypeNames names it
	inline const char* extraTypeName(nds::Type t)
	{
		static const char* const NAMES[] = { "Typeless", "Crystal", "Ground", "Tri", "Floating", "Magma" };
		return t >= TYPELESS && t <= MAGMA ? NAMES[t - TYPELESS] : nullptr;
	}
	// its chip's colour (Bonemerang's Ground's, upstream's TYPE_COLORS[GROUND]); 0 for any other type
	inline uint32_t extraTypeColor(nds::Type t)
	{
		static const uint32_t COLORS[] = { 0x68a090, 0x88d0e0, 0xe0c068, 0xc8b0e8, 0x98a8f0, 0xd05038 };
		return t >= TYPELESS && t <= MAGMA ? COLORS[t - TYPELESS] : 0;
	}

	inline bool oldGen(nds::Gen g) { return g == GEN_I || g == GEN_II; }
	inline bool gbGame(nds::GameId id) { return (int)id >= (int)RED && (int)id <= (int)CRYSTAL_CLEAR; }
	// the retail game whose engine a game is, for a per-game branch (Crystal's caught data, Yellow's Pikachu): a hack's
	// base, a retail game itself. PureGreen's is Red's: nothing branches on Red or Blue
	inline nds::GameId baseGame(nds::GameId id)
	{
		if (id == CRYSTAL_LEGACY || id == CRYSTAL_CLEAR) return CRYSTAL;
		if (id == YELLOW_LEGACY) return YELLOW;
		if (id == PURERED || id == PUREGREEN) return RED;
		if (id == PUREBLUE) return BLUE;
		return id;
	}
}
