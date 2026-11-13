# PokeDaisy's reference data

Based on PokeDaisy by Lidor Itzhari - https://github.com/lidor30/pokedaisy

The facts PokeDaisy 135ac809 holds about every mainline game it supports, English releases only, as JSON: what its readers read from the games' memory and ROM, its guides' structure by ids and flags, and its own words. Nothing here is copied into the image (the image's overlay is `kiwami/rootfs/`), and nothing of the games themselves is here: no species, move or item names, no Pokédex or item text, no wild tables, trainer teams, base stats, pictures or maps. Place and boss names are kept only where a guide names its bosses; PokeDaisy's own words are kept whole, though they name what they talk about.

Written by `kiwami/tools/pokedaisy-extract.py <checkout> --reference` (see `kiwami/GB-POKEMON.md`); `SOURCES` records PokeDaisy's commit and the SHA-256 of every file read and written, and the script's `--check` verifies them. Licence: `LICENSE` (GPL-3.0) and `NOTICE` (its 7(b) attribution term).

## The games

- **Red and Blue** (`red-blue/`): `guide.json`, `ram.json`, `releases.json`, `rom.json`.
- **Yellow** (`yellow/`): `guide.json`, `ram.json`, `releases.json`, `rom.json`, `text.json`.
- **Gold and Silver** (`gold-silver/`): `ram.json`, `releases.json`, `rom.json`.
- **Crystal** (`crystal/`): `guide.json`, `ram.json`, `releases.json`, `rom.json`, `text.json`.
- **Ruby and Sapphire** (`ruby-sapphire/`): `guide.json`, `ram.json`, `releases.json`, `rom.json`, `text.json`.
- **Emerald** (`emerald/`): `guide.json`, `ram.json`, `releases.json`, `rom.json`, `text.json`.
- **FireRed and LeafGreen** (`firered-leafgreen/`): `guide.json`, `ram.json`, `releases.json`, `rom.json`, `text.json`.
- **Platinum** (`platinum/`): `guide.json`, `ram.json`, `releases.json`, `rom.json`, `text.json`.
- **HeartGold and SoulSilver** (`heartgold-soulsilver/`): `guide.json`, `ram.json`, `releases.json`, `rom.json`, `text.json`.
- **Black and White** (`black-white/`): `guide.json`, `ram.json`, `releases.json`, `rom.json`, `text.json`.
- **Black 2 and White 2** (`black2-white2/`): `guide.json`, `ram.json`, `releases.json`, `rom.json`, `text.json`.

Each file:

- `releases.json`: the English releases PokeDaisy knows: SHA-1 and its title (GameTitles.kt).
- `ram.json`: the reader's configs, by PokeDaisy's name: RAM addresses, offsets, sizes and layouts.
- `rom.json`: the ROM tables' records: where the Pokédex, the pictures, the guide's tables and the rest are.
- `guide.json`: the guides' structure by ids: the bosses (title, place, the flag that marks them beaten, trainer ids), the generated boss tables without their teams, the areas' items, gifts and trades by place, id, flag and count, and the guides' constants (flags, items, vars).
- `text.json`: PokeDaisy's own words: its guide pages (the tips, and WHERE IS: each entry titled by what it is about, with PokeDaisy's answer and hint), and Yellow's item descriptions it wrote itself.

Numbers PokeDaisy writes in hex are written as hex strings ("0x020244EC"). A guide's `by_code` names the fields PokeDaisy works out by code from the save (a team chosen by the starter, a boss done when any of several flags is set): its source has the rule.

## Not extracted

What the script met and left out: by rule (a table of the game's, its text, a table of PokeDaisy's own ids), or because it is code rather than a value. Paths are PokeDaisy's names. A name "not in the files read" is one of PokeDaisy's values in a file the script doesn't read: its tables of the games' content (map places, item pockets, fly spots, town maps), its art (NdsArt) or its telemetry.

### Red and Blue

- `GEN1_BLUE.internalToDex`: the game's species order (a table of the game's)
- `GEN1_BLUE.pokedex.categorySuffix`: text (POKéMON)
- `GEN1_BLUE.pokedex.gen1.charset`: text (Gen1Charset.EN)
- `GEN1_BLUE.pokedex.gen1.internalToDex`: the game's species order (a table of the game's)
- `GEN1_BLUE.pokedex.probeCategory`: text (SEED)
- `GEN1_DEX_RED.charset`: text (Gen1Charset.EN)
- `GEN1_DEX_RED.internalToDex`: the game's species order (a table of the game's)
- `GEN1_RED.internalToDex`: the game's species order (a table of the game's)
- `GEN1_RED.pokedex.categorySuffix`: text (POKéMON)
- `GEN1_RED.pokedex.gen1.charset`: text (Gen1Charset.EN)
- `GEN1_RED.pokedex.gen1.internalToDex`: the game's species order (a table of the game's)
- `GEN1_RED.pokedex.probeCategory`: text (SEED)
- `Gen1Art.BLUE.charset`: text (Gen1Charset.EN)
- `Gen1Art.BLUE.fontPath`: text (gen1/red-blue-font.ttf)
- `Gen1Art.BLUE.iconsDir`: text (gen1/red_blue/pokemon)
- `Gen1Art.BLUE.playerPath`: text (regionmap/player_red_blue.png)
- `Gen1Art.BLUE.sha1`: TelemetrySampler isn't in the files read
- `Gen1Art.BLUE.townMapPath`: text (regionmap/red_blue.png)
- `Gen1Art.RED.charset`: text (Gen1Charset.EN)
- `Gen1Art.RED.fontPath`: text (gen1/red-blue-font.ttf)
- `Gen1Art.RED.iconsDir`: text (gen1/red_blue/pokemon)
- `Gen1Art.RED.playerPath`: text (regionmap/player_red_blue.png)
- `Gen1Art.RED.sha1`: TelemetrySampler isn't in the files read
- `Gen1Art.RED.townMapPath`: text (regionmap/red_blue.png)

### Yellow

- `GEN1_DEX_YELLOW.charset`: text (Gen1Charset.EN)
- `GEN1_DEX_YELLOW.internalToDex`: the game's species order (a table of the game's)
- `GEN1_YELLOW.internalToDex`: the game's species order (a table of the game's)
- `GEN1_YELLOW.pokedex.categorySuffix`: text (POKéMON)
- `GEN1_YELLOW.pokedex.gen1.charset`: text (Gen1Charset.EN)
- `GEN1_YELLOW.pokedex.gen1.internalToDex`: the game's species order (a table of the game's)
- `GEN1_YELLOW.pokedex.probeCategory`: text (SEED)
- `Gen1Art.YELLOW.charset`: text (Gen1Charset.EN)
- `Gen1Art.YELLOW.fontPath`: text (gen1/yellow-font.ttf)
- `Gen1Art.YELLOW.iconsDir`: text (gen1/yellow/pokemon)
- `Gen1Art.YELLOW.playerPath`: text (regionmap/player_yellow.png)
- `Gen1Art.YELLOW.sha1`: TelemetrySampler isn't in the files read
- `Gen1Art.YELLOW.townMapPath`: text (regionmap/yellow.png)

### Gold and Silver

- `GEN2_DEX_GOLD_SILVER.tmItems`: a table of the game's
- `GEN2_GOLD.genderRatios`: a table of the game's
- `GEN2_GOLD.growthRates`: a table of the game's
- `GEN2_GOLD.landmarks`: a table of the game's (map to landmark)
- `GEN2_GOLD.mailItems`: a table of the game's
- `GEN2_GOLD.pokedex.categorySuffix`: text (POKéMON)
- `GEN2_GOLD.pokedex.gen2.tmItems`: a table of the game's
- `GEN2_GOLD.pokedex.probeCategory`: text (SEED)
- `GEN2_GOLD.tmItems`: a table of the game's
- `GEN2_SILVER.genderRatios`: a table of the game's
- `GEN2_SILVER.growthRates`: a table of the game's
- `GEN2_SILVER.landmarks`: a table of the game's (map to landmark)
- `GEN2_SILVER.mailItems`: a table of the game's
- `GEN2_SILVER.pokedex.categorySuffix`: text (POKéMON)
- `GEN2_SILVER.pokedex.gen2.tmItems`: a table of the game's
- `GEN2_SILVER.pokedex.probeCategory`: text (SEED)
- `GEN2_SILVER.tmItems`: a table of the game's
- `Gen2Art.GOLD.fontPath`: text (gen2/gold-silver-font.ttf)
- `Gen2Art.GOLD.holdIconsPath`: text (gen2/gold_silver/hold_icons.png)
- `Gen2Art.GOLD.iconsDir`: text (gen2/gold_silver/pokemon)
- `Gen2Art.GOLD.playerPaths[0]`: text (regionmap/player_gold_silver.png)
- `Gen2Art.GOLD.sha1s[0]`: TelemetrySampler isn't in the files read
- `Gen2Art.GOLD.townMapPaths`: .map(...) on a list
- `Gen2Art.SILVER.fontPath`: text (gen2/gold-silver-font.ttf)
- `Gen2Art.SILVER.holdIconsPath`: text (gen2/gold_silver/hold_icons.png)
- `Gen2Art.SILVER.iconsDir`: text (gen2/gold_silver/pokemon)
- `Gen2Art.SILVER.playerPaths[0]`: text (regionmap/player_gold_silver.png)
- `Gen2Art.SILVER.sha1s[0]`: TelemetrySampler isn't in the files read
- `Gen2Art.SILVER.townMapPaths`: .map(...) on a list

### Crystal

- `GEN2_CRYSTAL.genderRatios`: a table of the game's
- `GEN2_CRYSTAL.growthRates`: a table of the game's
- `GEN2_CRYSTAL.landmarks`: a table of the game's (map to landmark)
- `GEN2_CRYSTAL.mailItems`: a table of the game's
- `GEN2_CRYSTAL.pokedex.categorySuffix`: text (POKéMON)
- `GEN2_CRYSTAL.pokedex.gen2.tmItems`: a table of the game's
- `GEN2_CRYSTAL.pokedex.probeCategory`: text (SEED)
- `GEN2_CRYSTAL.tmItems`: a table of the game's
- `GEN2_DEX_CRYSTAL.tmItems`: a table of the game's
- `Gen2Art.CRYSTAL.fontPath`: text (gen2/crystal-font.ttf)
- `Gen2Art.CRYSTAL.holdIconsPath`: text (gen2/crystal/hold_icons.png)
- `Gen2Art.CRYSTAL.iconsDir`: text (gen2/crystal/pokemon)
- `Gen2Art.CRYSTAL.playerPaths[0]`: text (regionmap/player_chris.png)
- `Gen2Art.CRYSTAL.playerPaths[1]`: text (regionmap/player_kris.png)
- `Gen2Art.CRYSTAL.sha1s[0]`: TelemetrySampler isn't in the files read
- `Gen2Art.CRYSTAL.sha1s[1]`: TelemetrySampler isn't in the files read
- `Gen2Art.CRYSTAL.sha1s[2]`: TelemetrySampler isn't in the files read
- `Gen2Art.CRYSTAL.townMapPaths`: .map(...) on a list

### Ruby and Sapphire

- `ruby.guideTables.probeName`: a boss's name, the game's text
- `ruby.pokedex.categorySuffix`: text (POKéMON)
- `ruby.pokedex.probeCategory`: text (SEED)
- `ruby.pokedex.regionName`: text (HOENN)
- `sapphire.guideTables.probeName`: a boss's name, the game's text
- `sapphire.pokedex.categorySuffix`: text (POKéMON)
- `sapphire.pokedex.probeCategory`: text (SEED)
- `sapphire.pokedex.regionName`: text (HOENN)

### Emerald

- `GUIDE_TABLES_EMERALD[probeName]`: text (ROXANNE)
- `POKEDEX_EMERALD[regionName]`: text (HOENN)
- `TRAINER_CARD_EMERALD[regionalDex]`: a table of PokeDaisy's (HOENN_TO_NATIONAL), not extracted
- `emerald_retail.guideTables[probeName]`: text (ROXANNE)
- `emerald_retail.pokedex[regionName]`: text (HOENN)
- `emerald_retail.trainerCard[regionalDex]`: a table of PokeDaisy's (HOENN_TO_NATIONAL), not extracted

### FireRed and LeafGreen

- `TRAINER_CARD_FIRERED[regionalDex]`: Kotlin code (IntArray(151) { it + 1 })
- `firered_rev1.trainerCard[regionalDex]`: Kotlin code (IntArray(151) { it + 1 })
- `leafgreen_rev0.trainerCard[regionalDex]`: Kotlin code (IntArray(151) { it + 1 })
- `leafgreen_rev1.trainerCard[regionalDex]`: Kotlin code (IntArray(151) { it + 1 })

### Platinum

- `GEN4_PLATINUM_US.flySpots`: flySpotsPlatinum isn't in the files read
- `GEN4_PLATINUM_US.itemPockets`: itemPocketsPlatinum isn't in the files read
- `GEN4_PLATINUM_US.mapPlaceIds`: mapPlaceIdsPlatinum isn't in the files read
- `GEN4_PLATINUM_US.mapPlaces`: mapPlacesPlatinum isn't in the files read
- `GEN4_PLATINUM_US.pokedexTables.categorySuffix`: text (Pokémon)
- `GEN4_PLATINUM_US.pokedexTables.gen4.fronts`: NdsArt isn't in the files read
- `GEN4_PLATINUM_US.pokedexTables.gen4.pages`: NdsArt isn't in the files read
- `GEN4_PLATINUM_US.pokedexTables.gen4.tmMoves`: a table of the game's
- `GEN4_PLATINUM_US.pokedexTables.probeCategory`: text (SEED)
- `GEN4_PLATINUM_US.pokedexTables.regionName`: text (SINNOH)
- `GEN4_PLATINUM_US.townMap.grid`: bytes copied from the ROM
- `GEN4_PLATINUM_US.townMap.headerPlaces`: mapPlaceIdsPlatinum isn't in the files read
- `GEN4_PLATINUM_US.townMap.mainMatrixHeaders`: mainMatrixHeadersPlatinum isn't in the files read

### HeartGold and SoulSilver

- `GEN4_HEARTGOLD_US.flySpots`: flySpotsHeartGold isn't in the files read
- `GEN4_HEARTGOLD_US.itemPockets`: itemPocketsHeartGold isn't in the files read
- `GEN4_HEARTGOLD_US.mapPlaceIds`: mapPlaceIdsHeartGold isn't in the files read
- `GEN4_HEARTGOLD_US.mapPlaces`: mapPlacesHeartGold isn't in the files read
- `GEN4_HEARTGOLD_US.pokedexTables.categorySuffix`: text (Pokémon)
- `GEN4_HEARTGOLD_US.pokedexTables.gen4.fronts`: NdsArt isn't in the files read
- `GEN4_HEARTGOLD_US.pokedexTables.gen4.pages`: NdsArt isn't in the files read
- `GEN4_HEARTGOLD_US.pokedexTables.gen4.tmMoves`: a table of the game's
- `GEN4_HEARTGOLD_US.pokedexTables.probeCategory`: text (SEED)
- `GEN4_HEARTGOLD_US.pokedexTables.regionName`: text (JOHTO)
- `GEN4_HEARTGOLD_US.pokegear.mainMatrixHeaders`: mainMatrixHeadersHeartGold isn't in the files read
- `GEN4_HEARTGOLD_US.pokegear.worldMap`: worldMapHeartGold isn't in the files read
- `GEN4_SOULSILVER_US.flySpots`: flySpotsHeartGold isn't in the files read
- `GEN4_SOULSILVER_US.itemPockets`: itemPocketsHeartGold isn't in the files read
- `GEN4_SOULSILVER_US.mapPlaceIds`: mapPlaceIdsHeartGold isn't in the files read
- `GEN4_SOULSILVER_US.mapPlaces`: mapPlacesHeartGold isn't in the files read
- `GEN4_SOULSILVER_US.pokedexTables.categorySuffix`: text (Pokémon)
- `GEN4_SOULSILVER_US.pokedexTables.gen4.fronts`: NdsArt isn't in the files read
- `GEN4_SOULSILVER_US.pokedexTables.gen4.pages`: NdsArt isn't in the files read
- `GEN4_SOULSILVER_US.pokedexTables.gen4.tmMoves`: a table of the game's
- `GEN4_SOULSILVER_US.pokedexTables.probeCategory`: text (SEED)
- `GEN4_SOULSILVER_US.pokedexTables.regionName`: text (JOHTO)
- `GEN4_SOULSILVER_US.pokegear.mainMatrixHeaders`: mainMatrixHeadersHeartGold isn't in the files read
- `GEN4_SOULSILVER_US.pokegear.worldMap`: worldMapHeartGold isn't in the files read

### Black and White

- `GEN5_BLACK.pokedexTables.categorySuffix`: text (Pokémon)
- `GEN5_BLACK.pokedexTables.gen4.fronts`: NdsArt isn't in the files read
- `GEN5_BLACK.pokedexTables.gen4.pages`: NdsArt isn't in the files read
- `GEN5_BLACK.pokedexTables.gen4.tmMoves`: a table of the game's
- `GEN5_BLACK.pokedexTables.probeCategory`: text (SEED)
- `GEN5_BLACK.pokedexTables.regionName`: text (UNOVA)
- `GEN5_BLACK.regionalDex`: a table of the game's (its regional Pokédex order)
- `GEN5_BLACK.townMapPlaces`: townMapPlacesBlack isn't in the files read
- `GEN5_BLACK.townMapZones`: townMapZonesBlack isn't in the files read
- `GEN5_BLACK.zonePlaceIds`: mapPlaceIdsBlack isn't in the files read
- `GEN5_BLACK.zonePlaces`: mapPlacesBlack isn't in the files read
- `GEN5_WHITE.pokedexTables.categorySuffix`: text (Pokémon)
- `GEN5_WHITE.pokedexTables.gen4.fronts`: NdsArt isn't in the files read
- `GEN5_WHITE.pokedexTables.gen4.pages`: NdsArt isn't in the files read
- `GEN5_WHITE.pokedexTables.gen4.tmMoves`: a table of the game's
- `GEN5_WHITE.pokedexTables.probeCategory`: text (SEED)
- `GEN5_WHITE.pokedexTables.regionName`: text (UNOVA)
- `GEN5_WHITE.regionalDex`: a table of the game's (its regional Pokédex order)
- `GEN5_WHITE.townMapPlaces`: townMapPlacesWhite isn't in the files read
- `GEN5_WHITE.townMapZones`: townMapZonesWhite isn't in the files read
- `GEN5_WHITE.zonePlaceIds`: mapPlaceIdsBlack isn't in the files read
- `GEN5_WHITE.zonePlaces`: mapPlacesBlack isn't in the files read

### Black 2 and White 2

- `GEN5_BLACK2.pokedexTables.categorySuffix`: text (Pokémon)
- `GEN5_BLACK2.pokedexTables.gen4.fronts`: NdsArt isn't in the files read
- `GEN5_BLACK2.pokedexTables.gen4.pages`: NdsArt isn't in the files read
- `GEN5_BLACK2.pokedexTables.gen4.tmMoves`: a table of the game's
- `GEN5_BLACK2.pokedexTables.probeCategory`: text (SEED)
- `GEN5_BLACK2.pokedexTables.regionName`: text (UNOVA)
- `GEN5_BLACK2.regionalDex`: a table of the game's (its regional Pokédex order)
- `GEN5_BLACK2.townMapPlaces`: townMapPlacesBlack2 isn't in the files read
- `GEN5_BLACK2.townMapZones`: townMapZonesBlack2 isn't in the files read
- `GEN5_BLACK2.zonePlaceIds`: mapPlaceIdsBlack2 isn't in the files read
- `GEN5_BLACK2.zonePlaces`: mapPlacesBlack2 isn't in the files read
- `GEN5_WHITE2.pokedexTables.categorySuffix`: text (Pokémon)
- `GEN5_WHITE2.pokedexTables.gen4.fronts`: NdsArt isn't in the files read
- `GEN5_WHITE2.pokedexTables.gen4.pages`: NdsArt isn't in the files read
- `GEN5_WHITE2.pokedexTables.gen4.tmMoves`: a table of the game's
- `GEN5_WHITE2.pokedexTables.probeCategory`: text (SEED)
- `GEN5_WHITE2.pokedexTables.regionName`: text (UNOVA)
- `GEN5_WHITE2.regionalDex`: a table of the game's (its regional Pokédex order)
- `GEN5_WHITE2.townMapPlaces`: townMapPlacesWhite2 isn't in the files read
- `GEN5_WHITE2.townMapZones`: townMapZonesWhite2 isn't in the files read
- `GEN5_WHITE2.zonePlaceIds`: mapPlaceIdsBlack2 isn't in the files read
- `GEN5_WHITE2.zonePlaces`: mapPlacesBlack2 isn't in the files read

Left out whole: PokeDaisy's hacks, its other languages' releases and texts (`*TextDeGen.kt`, `GameText*Gen.kt`, `gamedata/gen3/ports/`, `gamedata/nds/ports.toml`), and its tables of the games' content (species, move and item names and data, Pokédex and item text, wild Pokémon, trainer teams, base stats, gender ratios, growth rates, type charts, town maps).
