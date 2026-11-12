// banana-ui (kiwami): the Game Boy Pokémon companion's region maps (maps.h): the Pokégear's Johto and Kanto maps and
// the places on them, as pret's pokecrystal and pokegold draw them; the town map of Kanto and its places, as pret's
// pokered and pokeyellow do.
// Copyright (C) 2026 Lidor Itzhari (PokeDaisy) and the kiwami fork's contributors. GNU GPL version 3 with PokeDaisy's
// additional term under section 7(b): LICENSE and NOTICE in this folder. SPDX-License-Identifier: GPL-3.0-only
// Based on PokeDaisy by Lidor Itzhari - https://github.com/lidor30/pokedaisy
#include "maps.h"
#include "pics.h"
#include <algorithm>
#include <climits>
#include <cstdlib>

namespace banana::kiwami::gbpk
{
	namespace
	{
		const int W = 20, H = 18, CELL = 8;		// constants/gfx_constants.asm SCREEN_WIDTH, SCREEN_HEIGHT; a tile's pixels
		const int TILES = 48;					// engine/pokegear/pokegear.asm LoadTownMapGFX: the town map's tiles
		const int PAL_MAP_TILES = 0x60;			// TownMapPals: the ids its palette map covers (the rest palette 0)
		const int PALETTES = 6;					// engine/gfx/cgb_layouts.asm _CGB_PokegearPals: the Pokégear's 6
		const int LANDMARK = 4;					// data/maps/landmarks.asm: x + 8, y + 16 (the sprites'), the name's pointer
		// data/sprite_anims/oam.asm OAMData_RedWalk: the player's icon, 2 x 2 tiles about the landmark's point
		const int ICON = 16;
		const size_t SCRATCH = 0x80 * 16;		// ram/wram.asm wDecompressScratch, where DecompressRequest2bpp writes
		const int LANDMARK_SPECIAL = 0;			// constants/landmark_constants.asm: on no map
		// Gen I (pokered's engine/items/town_map.asm): the 16 tiles LoadTownMap copies (WorldMapTileGraphics); a town
		// map entry's x, y as the icon's top left (TownMapCoordsToOAMCoords: 8 px a step from 24, 24;
		// WriteTownMapSpriteOAM's -4, -4 is -4, -3 with the carry; less the OAM's 8, 16)
		const int TOWN_MAP_TILES = 16, ENTRY_X = 12, ENTRY_Y = 5;
	}

	std::vector<uint32_t> townMapPicture(const uint8_t* map, const std::vector<uint8_t>& tiles, const uint8_t* palMap,
		const uint8_t* pals, int count)
	{
		const int nTiles = (int)(tiles.size() / 16), pw = W * CELL;
		std::vector<uint32_t> px((size_t)pw * H * CELL);
		for (int c = 0; c < W * H; c++)
		{
			const int t = map[c];
			if (t >= nTiles) return {};
			const int pal = t >= PAL_MAP_TILES ? 0 : palMap[t / 2] >> (t & 1 ? 4 : 0) & 7;
			if (pal >= count) return {};
			const uint8_t* colors = pals + 8 * pal;
			for (int y = 0; y < CELL; y++)
				for (int x = 0; x < CELL; x++)
				{
					const int v = tilePixel(&tiles[(size_t)t * 16], x, y);
					px[(size_t)(c / W * CELL + y) * pw + c % W * CELL + x] = 0xFF000000u | rgb555((uint16_t)(colors[2 * v] | colors[2 * v + 1] << 8));
				}
		}
		if (map[W * H] != 0xFF) return {};	// (FillTownMap: the screen, then its end mark)
		return px;
	}

	bool readPokegearMaps(const RomView& r, const GameData& d, const std::map<uint16_t, uint8_t>& landmarkOf, nds::RomData& out,
		std::string* why)
	{
		auto no = [&](const std::string& w) { if (why) *why = w; return false; };
		out.regionMaps.clear();
		auto table = [&](const char* k) { return r.at(d.rom.count(k) ? d.rom.at(k) : Addr{}); };
		const int n = (int)out.locationNames.size(), kanto = d.size("kantoLandmark"), fastShip = d.size("fastShipLandmark");
		const size_t landmarks = table("landmarks");
		if (kanto < 1 || kanto >= n || fastShip < 1 || fastShip >= n || !r.has(landmarks, (size_t)LANDMARK * n))
			return no("the data file's kantoLandmark and fastShipLandmark aren't among the landmarks");
		// each landmark's point on the screen and its map, as the Pokégear picks it (engine/pokegear/pokegear.asm
		// InitPokegearTilemap.Map: from KANTO_LANDMARK on Kanto's, but the Fast Ship's on Johto's)
		std::vector<int> lx(n), ly(n), region(n);
		for (int lm = 0; lm < n; lm++)
		{
			lx[lm] = r.u8(landmarks + (size_t)LANDMARK * lm) - 8;
			ly[lm] = r.u8(landmarks + (size_t)LANDMARK * lm + 1) - 16;
			region[lm] = lm == LANDMARK_SPECIAL ? -1 : lm == fastShip ? 0 : lm >= kanto ? 1 : 0;
		}

		// the pictures: TownMapGFX's tiles (LZ), each map's tiles in the palette TownMapPals gives their id
		// (PokegearMap, FillTownMap, TownMapPals)
		const size_t gfx = table("townMapGfx"), palMap = table("townMapPalMap"), pals = table("pokegearPals");
		const size_t maps[2] = { table("johtoMap"), table("kantoMap") };
		std::vector<uint8_t> tiles;
		bool drawn = gfx != RomView::NONE && r.has(palMap, PAL_MAP_TILES / 2) && r.has(pals, (size_t)PALETTES * 8) &&
			lz2(r.b.data() + gfx, std::min(r.b.size(), (gfx / 0x4000 + 1) * 0x4000) - gfx, tiles, SCRATCH) &&
			tiles.size() >= (size_t)TILES * 16;
		if (drawn) tiles.resize((size_t)TILES * 16);
		static const char* const NAMES[2] = { "Johto", "Kanto" };
		for (int m = 0; m < 2; m++)
		{
			nds::RegionMap rm;
			rm.name = NAMES[m];
			rm.w = W * CELL;
			rm.h = H * CELL;
			rm.cell = CELL;
			rm.gridW = W;
			rm.gridH = H;
			if (drawn && r.has(maps[m], (size_t)W * H + 1))
				rm.rgba = townMapPicture(&r.b[maps[m]], tiles, &r.b[palMap], &r.b[pals], PALETTES);
			// a cell names the map's landmark nearest its middle, a cell away at most (the lower id of two as near)
			rm.cells.assign((size_t)W * H, 0xFFFF);
			for (int cy = 0; cy < H; cy++)
				for (int cx = 0; cx < W; cx++)
				{
					int best = -1, bestD = INT_MAX;
					for (int lm = 0; lm < n; lm++)
					{
						if (region[lm] != m) continue;
						const int dist = std::max(std::abs(lx[lm] - (cx * CELL + CELL / 2)), std::abs(ly[lm] - (cy * CELL + CELL / 2)));
						if (dist <= CELL && dist < bestD) { best = lm; bestD = dist; }
					}
					if (best >= 0) rm.cells[(size_t)cy * W + cx] = (uint16_t)best;
				}
			out.regionMaps.push_back(std::move(rm));
		}

		// each place: the player's icon about its landmark, on its landmark's map; a place with no landmark, or the
		// special one (the rooms the game puts on no map), on none
		for (auto& [z, loc] : out.zones)
		{
			loc.spot = nds::MapSpot{};
			auto it = landmarkOf.find(z);
			if (it == landmarkOf.end() || it->second >= n || region[it->second] < 0) continue;
			const int lm = it->second;
			loc.spot.map = (int16_t)region[lm];
			const int x0 = lx[lm] - ICON / 2, y0 = ly[lm] - ICON / 2;
			loc.spot.x = (float)x0;
			loc.spot.y = (float)y0;
			loc.spot.w = loc.spot.h = ICON;
		}
		return true;
	}

	std::vector<uint8_t> townMapCells(const uint8_t* p, size_t n)
	{
		std::vector<uint8_t> cells;
		for (size_t i = 0;; i++)
		{
			if (i >= n) return {};
			if (!p[i]) break;
			// (a length of 0 would run 256 in the game's loop)
			const size_t len = p[i] & 0xF;
			if (!len || cells.size() + len > (size_t)W * H) return {};
			cells.insert(cells.end(), len, (uint8_t)(p[i] >> 4));
		}
		if (cells.size() != (size_t)W * H) return {};
		return cells;
	}

	bool readTownMap(const RomView& r, const GameData& d, const std::map<uint16_t, uint8_t>& pointOf, nds::RomData& out,
		std::string* why)
	{
		auto no = [&](const std::string& w) { if (why) *why = w; return false; };
		out.regionMaps.clear();
		auto table = [&](const char* k) { return r.at(d.rom.count(k) ? d.rom.at(k) : Addr{}); };
		// the places TownMapOrder steps through (data/maps/town_map_order.asm: a map each), at their entry's cell
		const size_t order = table("townMapOrder"), orderEnd = table("townMapOrderEnd");
		if (order == RomView::NONE || orderEnd == RomView::NONE || orderEnd <= order || orderEnd - order > 255 ||
			orderEnd / 0x4000 != order / 0x4000)
			return no("the data file's townMapOrder and townMapOrderEnd aren't a list");
		struct Place { int cx, cy; uint16_t name; };
		std::vector<Place> places;
		for (size_t o = order; o < orderEnd; o++)
		{
			const uint16_t m = r.u8(o);
			auto p = pointOf.find(m);
			auto z = out.zones.find(m);
			auto n = z == out.zones.end() ? out.locationNames.end() : std::find(out.locationNames.begin(), out.locationNames.end(), z->second.name);
			if (p == pointOf.end() || z == out.zones.end() || z->second.name.empty() || n == out.locationNames.end())
				return no("TownMapOrder's map " + std::to_string(m) + " has no named town map entry");
			// (the icon's middle: the cell 2 right of and 1 below the entry's x, y)
			places.push_back({ (p->second & 0xF) + 2, (p->second >> 4) + 1, (uint16_t)(n - out.locationNames.begin()) });
		}

		nds::RegionMap rm;
		rm.name = "Kanto";
		rm.w = W * CELL;
		rm.h = H * CELL;
		rm.cell = CELL;
		rm.gridW = W;
		rm.gridH = H;
		// the picture: the runs' tiles (WorldMapTileGraphics, 2-bit) in the town map's palette, the whole screen's
		// (engine/gfx/palettes.asm SetPal_TownMap: PAL_TOWNMAP)
		const size_t rle = table("townMap"), tiles = table("townMapTiles"), pals = table("palettes");
		const int pal = d.sizes.count("palTownMap") ? d.size("palTownMap") : -1;
		const size_t colors = pal >= 0 && pals != RomView::NONE ? pals + 8 * (size_t)pal : RomView::NONE;
		std::vector<uint8_t> cells;
		if (rle != RomView::NONE && r.has(tiles, (size_t)TOWN_MAP_TILES * 16) && r.has(colors, 8))
			cells = townMapCells(r.b.data() + rle, std::min(r.b.size(), (rle / 0x4000 + 1) * 0x4000) - rle);
		if (!cells.empty())
		{
			rm.rgba.resize((size_t)rm.w * rm.h);
			for (int c = 0; c < W * H; c++)
				for (int y = 0; y < CELL; y++)
					for (int x = 0; x < CELL; x++)
						rm.rgba[(size_t)(c / W * CELL + y) * rm.w + c % W * CELL + x] =
							0xFF000000u | rgb555(r.le16(colors + 2 * (size_t)tilePixel(&r.b[tiles + (size_t)cells[c] * 16], x, y)));
		}
		// a cell names the place nearest it, a cell away at most (the first in TownMapOrder of two as near)
		rm.cells.assign((size_t)W * H, 0xFFFF);
		for (int cy = 0; cy < H; cy++)
			for (int cx = 0; cx < W; cx++)
			{
				int bestD = 2;
				for (const Place& p : places)
				{
					const int dist = std::max(std::abs(p.cx - cx), std::abs(p.cy - cy));
					if (dist < bestD) { bestD = dist; rm.cells[(size_t)cy * W + cx] = p.name; }
				}
			}
		out.regionMaps.push_back(std::move(rm));

		// each place: the game's icon at its entry; a map with no entry or no name on none
		for (auto& [z, loc] : out.zones)
		{
			loc.spot = nds::MapSpot{};
			auto p = pointOf.find(z);
			if (p == pointOf.end() || loc.name.empty()) continue;
			loc.spot.map = 0;
			loc.spot.x = (float)((p->second & 0xF) * CELL + ENTRY_X);
			loc.spot.y = (float)((p->second >> 4) * CELL + ENTRY_Y);
			loc.spot.w = loc.spot.h = ICON;
		}
		return true;
	}
}
