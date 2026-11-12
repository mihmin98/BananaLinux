#pragma once
// banana-ui (kiwami): the Game Boy Pokémon companion's region maps: Gold's, Silver's and Crystal's Pokégear maps of
// Johto and Kanto, decoded from the player's ROM as upstream's nds::RegionMap (160 x 144 px, the screen the Pokégear
// fills, in 8 px cells), and where every place is on them (nds::MapSpot), as pret's pokecrystal and pokegold draw them
// (engine/pokegear/pokegear.asm, data/maps/landmarks.asm).
// Copyright (C) 2026 Lidor Itzhari (PokeDaisy) and the kiwami fork's contributors. GNU GPL version 3 with PokeDaisy's
// additional term under section 7(b): LICENSE and NOTICE in this folder. SPDX-License-Identifier: GPL-3.0-only
// Based on PokeDaisy by Lidor Itzhari - https://github.com/lidor30/pokedaisy
#include "rom.h"
#include <cstdint>
#include <map>
#include <string>

namespace banana::kiwami::gbpk
{
	// The Pokégear's Johto and Kanto maps into out.regionMaps (0 Johto, 1 Kanto), and every zone's spot on them (the
	// game's 16 x 16 player icon around its landmark; the map its landmark's region, as the Pokégear picks it). Each
	// cell names the landmark of its map nearest its middle, within a cell. out.locationNames must be read already;
	// landmarkOf is each zone's landmark. A picture that doesn't decode is left empty, so the pages draw the places'
	// squares; false (why set, no maps) when the data file's numbers don't fit
	bool readPokegearMaps(const RomView& r, const GameData& d, const std::map<uint16_t, uint8_t>& landmarkOf, nds::RomData& out,
		std::string* why = nullptr);

	// the picture of a 20 x 18 tile map (0xFF after it, as FillTownMap reads it): tiles, the town map's (2-bit, 16 bytes
	// each), coloured by palMap (a nybble a tile id, the even id's low; ids from 0x60 palette 0) from pals (four RGB555
	// colours each, count of them). Empty when a tile or a palette isn't there
	std::vector<uint32_t> townMapPicture(const uint8_t* map, const std::vector<uint8_t>& tiles, const uint8_t* palMap,
		const uint8_t* pals, int count);
}
