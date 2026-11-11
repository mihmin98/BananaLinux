// banana-ui (kiwami): Generation II's items that upstream's logic and pages know by their Gen IV number (ui/src/nds/
// logic.cpp's Item enum, the TMs from 328 and HMs from 420, the rods, the evolution stones and held items): by Gen II
// id (pret's constants/item_constants.asm, the same in pokecrystal and pokegold), its Gen IV id (upstream's own
// ui/src/gba/gen3items.inc, read from the games' ROMs, gives each). Any other item is 1000 + its Gen II id, as
// upstream's Gen III-only items are kept apart. rom.cpp includes it (gen2Item, for the ROM and the memory readers).
// Copyright (C) 2026 Lidor Itzhari (PokeDaisy) and the kiwami fork's contributors. GNU GPL version 3 with PokeDaisy's
// additional term under section 7(b): LICENSE and NOTICE in this folder. SPDX-License-Identifier: GPL-3.0-only
// Based on PokeDaisy by Lidor Itzhari - https://github.com/lidor30/pokedaisy
{ 0x01, 1 },	// MASTER_BALL
{ 0x02, 2 },	// ULTRA_BALL
{ 0x04, 3 },	// GREAT_BALL
{ 0x05, 4 },	// POKE_BALL
{ 0x08, 81 },	// MOON_STONE
{ 0x16, 82 },	// FIRE_STONE
{ 0x17, 83 },	// THUNDERSTONE
{ 0x18, 84 },	// WATER_STONE
{ 0x22, 85 },	// LEAF_STONE
{ 0x3A, 445 },	// OLD_ROD
{ 0x3B, 446 },	// GOOD_ROD
{ 0x3D, 447 },	// SUPER_ROD
{ 0x4C, 237 },	// SOFT_SAND
{ 0x4D, 244 },	// SHARP_BEAK
{ 0x51, 245 },	// POISON_BARB
{ 0x52, 221 },	// KINGS_ROCK
{ 0x58, 222 },	// SILVERPOWDER
{ 0x5F, 243 },	// MYSTIC_WATER
{ 0x60, 248 },	// TWISTEDSPOON
{ 0x62, 241 },	// BLACKBELT_I
{ 0x66, 240 },	// BLACKGLASSES
{ 0x6B, 246 },	// NEVERMELTICE
{ 0x6C, 242 },	// MAGNET
{ 0x71, 247 },	// SPELL_TAG
{ 0x75, 239 },	// MIRACLE_SEED
{ 0x76, 258 },	// THICK_CLUB
{ 0x7D, 238 },	// HARD_STONE
{ 0x8A, 249 },	// CHARCOAL
{ 0x8F, 233 },	// METAL_COAT
{ 0x90, 250 },	// DRAGON_FANG
{ 0x97, 235 },	// DRAGON_SCALE
{ 0x9D, 495 },	// HEAVY_BALL
{ 0x9F, 493 },	// LEVEL_BALL
{ 0xA0, 494 },	// LURE_BALL
{ 0xA1, 492 },	// FAST_BALL
{ 0xA3, 236 },	// LIGHT_BALL
{ 0xA4, 497 },	// FRIEND_BALL
{ 0xA5, 498 },	// MOON_BALL
{ 0xA6, 496 },	// LOVE_BALL
{ 0xA9, 80 },	// SUN_STONE
{ 0xAC, 252 },	// UP_GRADE
{ 0xB1, 499 },	// PARK_BALL: the Bug-Catching Contest's (x1.5), which Gen IV calls the Sport Ball
