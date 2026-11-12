// banana-ui (kiwami): Generation I's and II's items that upstream's logic and pages know by their Gen IV number
// (ui/src/nds/logic.cpp's Item enum, the TMs from 328 and HMs from 420, the rods, the evolution stones and held items):
// by the game's own id (pret's constants/item_constants.asm: pokecrystal's and pokegold's the same, and pokered's and
// pokeyellow's), its Gen IV id (upstream's own ui/src/gba/gen3items.inc, read from the games' ROMs, gives each). Any
// other item is 1000 + its own id, as upstream's Gen III-only items are kept apart. Gen I's TMs and HMs (TM01-50 from
// $C9, HM01-05 from $C4) are counted, not listed, and its key items are the game's own list (pret's
// data/items/key_items.asm KeyItemFlags, the same in pokered and pokeyellow), the pocket each goes in. rom.cpp includes
// it (gen1Item, gen1Pocket, gen2Item, for the ROM and the memory readers).
// Copyright (C) 2026 Lidor Itzhari (PokeDaisy) and the kiwami fork's contributors. GNU GPL version 3 with PokeDaisy's
// additional term under section 7(b): LICENSE and NOTICE in this folder. SPDX-License-Identifier: GPL-3.0-only
// Based on PokeDaisy by Lidor Itzhari - https://github.com/lidor30/pokedaisy
const ItemId GEN2_ITEMS[] = {
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
};

const ItemId GEN1_ITEMS[] = {
{ 0x01, 1 },	// MASTER_BALL
{ 0x02, 2 },	// ULTRA_BALL
{ 0x03, 3 },	// GREAT_BALL
{ 0x04, 4 },	// POKE_BALL
{ 0x08, 5 },	// SAFARI_BALL
{ 0x0A, 81 },	// MOON_STONE
{ 0x20, 82 },	// FIRE_STONE
{ 0x21, 83 },	// THUNDER_STONE
{ 0x22, 84 },	// WATER_STONE
{ 0x2F, 85 },	// LEAF_STONE
{ 0x4C, 445 },	// OLD_ROD
{ 0x4D, 446 },	// GOOD_ROD
{ 0x4E, 447 },	// SUPER_ROD
};
const uint8_t GEN1_KEY_ITEMS[] = {
	0x05, 0x06, 0x07, 0x08, 0x09,	// TOWN_MAP, BICYCLE, SURFBOARD, SAFARI_BALL, POKEDEX
	0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C,	// BOULDERBADGE .. EARTHBADGE
	0x1F, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x30,	// OLD_AMBER, DOME_FOSSIL, HELIX_FOSSIL, SECRET_KEY, ITEM_2C, BIKE_VOUCHER, CARD_KEY
	0x3F, 0x40, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A,	// S_S_TICKET, GOLD_TEETH, COIN_CASE, OAKS_PARCEL, ITEMFINDER, SILPH_SCOPE, POKE_FLUTE, LIFT_KEY
	0x4C, 0x4D, 0x4E,	// OLD_ROD, GOOD_ROD, SUPER_ROD
};
const uint8_t GEN1_HM01 = 0xC4, GEN1_TM01 = 0xC9;
