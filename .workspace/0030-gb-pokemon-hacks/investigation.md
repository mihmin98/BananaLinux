# 0030 Pokémon companion for the Game Boy ROM hacks: investigation

Issue: #30 · Branch: `k/0030-gb-pokemon-hacks` · Status: refined · Confidence: 85% (proceed)

## The request

The fork's Game Boy Pokémon companion (#28, `ui/src/kiwami/gbpk/`, `kiwami/GB-POKEMON.md`) reads the English retail
Red, Blue, Yellow, Gold, Silver and Crystal, and Fast Red and Fast Blue as Red and Blue. Every other ROM with one of
their headers shows "Not supported". The request is to support the ROM hacks PokeDaisy (`lidor30/pokedaisy`, pinned
at `135ac809`, 1.5.0.1) supports. The user's first list named thirteen hacks across three consoles and asked which
famous ones it missed.

Decided with the user:

- **Three issues, not one.** The user's list spans three companions. The GBA hacks (Recharged Yellow, which is a
  FireRed rebuild, not a Game Boy hack; Recharged Emerald, Emerald Legacy, Radical Red) and Renegade Platinum on the DS
  live in upstream's companions (`ui/src/pokemon.cpp`, `ui/src/gba/`, `ui/src/nds/`). Each got an issue of its own,
  written in this session. Unbound v2.1.1.1, Lazarus v2 and Seaglass v3.0 are already upstream's
  (`ui/src/gba/hacks.cpp:15-21`). Diamond and Pearl are too (`ui/src/nds/model.cpp:20`, `nds/guide.cpp:14`).
- **#30 is the Game Boy hacks, and of those the four nearest their base games**:
  - Crystal Legacy v1.3.1;
  - Yellow Legacy v1.0.10;
  - Crystal Clear v2.5.11 and v2.6.0 (each in its Standard Colors and Color Filter builds);
  - PureRed, PureBlue and PureGreen at v2.7.6 and v2.7.7, six builds; PokeDaisy reads only PureRed and PureBlue v2.7.6
    (*Open questions*, 1).
- **Polished Crystal and Prism go to issues of their own** (oversights 2 and 3).
- **Same pages as the base games** (Battle, Party, Team, Here, Map, Guide, Dex, Log), from each hack's own RAM
  addresses and its own ROM tables read at run time, as #28 does it. Nothing of the hacks' content (names, tables,
  text) goes into the repository.
- **Addresses from the hacks' own symbols.** Crystal Legacy, Yellow Legacy and PureRGB are pret forks with public
  sources but publish only patches. The user clones them at their tags and builds them, and `pret-extract.py` reads
  their `.sym` files (oversight 6). Crystal Clear has no source: a fork script maps pret/pokecrystal's symbols into it
  through the code that uses them, PokeDaisy's `port_gen1.py` method (oversight 7).
- **Guides**: PokeDaisy's for Crystal Legacy and Yellow Legacy, converted as #28 converted Crystal's and Yellow's.
  Crystal Clear's is built from the ROM's trainer classes, as Gold's and Silver's is (`gsguide.cpp`). PureRed's and
  PureBlue's is Red's and Blue's PokeDaisy guide if its flags and bosses check out against PureRGB's source, else one
  built from the ROM (oversight 8).
- **PureRGB follows the save**: the player can switch species' types back at an NPC, and switch four type matchups in
  the options. The companion reads both from the save (oversight 4).
- **The user has the ROMs** at the versions listed, for the ROM tests and the device.

Out of scope: Polished Crystal and Prism (their own issues), the GBA and DS hacks (their own issues), other versions
of these hacks, other languages, and hacks PokeDaisy doesn't support.

**Famous hacks missing from the first list** (the user's question):

- **Supported by PokeDaisy, and already upstream's**: Gaia, Glazed, Heart & Soul and SoulGold on the GBA, and Origin
  HeartGold on the DS.
- **Supported by PokeDaisy, not yet anywhere in Banana**: Crystal Clear and Prism on the Game Boy, and a long tail on
  the GBA (Emerald Rogue, Elite Redux, Emerald Kaizo, Ash Gray, Quetzal, Ultra Violet and others,
  `GameTitles.kt:114-215`).
- **Not supported by PokeDaisy** (from general knowledge, not checked against any source here): Run & Bun, Inclement
  Emerald, Shin Pokémon Red/Blue, Red++, Crystal Kaizo+ and Brown, and on the DS Sacred Gold/Storm Silver and Blaze
  Black 2/Volt White 2 (none of them is in PokeDaisy's tree). Each would need every address found from scratch.

## Oversights in the requirements

1. **Several hacks keep their base game's header title, and today's detection maps a title to one game.**
   - Crystal Legacy's Makefile gives `-t PM_CRYSTAL -i BYTE` (Crystal's). Yellow Legacy's gives `"POKEMON YELLOW"`, and
     PureRGB's `"POKEMON RED"`, `"POKEMON BLUE"` and `"POKEMON GREEN"`, each from its Makefile at its tag.
   - Crystal Clear's title isn't known without its source. It is read from the user's ROM during planning.
   - `detect` (`ui/src/kiwami/gbpk/games.cpp:203-218`) returns the first game whose `header_title` matches, and
     `confirm` (`:220-240`) then checks the SHA-1 against that one game's releases. Today PureRed is therefore taken for
     Red, and fails its SHA-1: "Not supported".
   - *Proposed*: a title gives every game that has it, and the whole file's SHA-1 picks among them. This is the fork's
     own code; no hook changes.

2. **Polished Crystal is close to a new generation.**
   - The engine: Fairy, the physical/special split, abilities, natures, and EVs instead of stat experience.
   - The species and the save: 9-bit species ids with variant forms, its own Pokédex order, new bag pockets (Medicine,
     Berries, a key items list), and a TM case of bits (PokeDaisy `Gen2Reader.kt:237-286`, `Gen2Dex.kt`).
   - The builds: 16 of them (3.2.0-3.2.3, normal and faithful, each with a debug build), with the same RAM but each
     with its own ROM addresses.
   - Its battle rules would have to be worked out from its source, as #28 did for Gen I and II from pret's.
   - *Decided*: its own issue.

3. **Prism doesn't fit upstream's model.**
   - Its four new types (Gas, Sound, Tri, Prism) have no place in upstream's `nds::Type` (`ui/src/nds/model.h:43`,
     18 types, 20 uses of `NUM_TYPES` in `ui/src/nds/`).
   - It has invented species (Phancero, Libabeel) and species ids that aren't national numbers (PokeDaisy
     `gen_gen2_tables.py:29-35`).
   - Its text is Huffman-packed (`Gen2Dex.kt:70-100`), its TMs aren't items, and it has six regions.
   - *Decided*: its own issue, which decides how its types fit (a hook into upstream's enum, or another way).

4. **PureRGB's types and matchups are the player's choice, not the ROM's.**
   - An NPC at the Indigo Plateau's Route 23 switches a species' new type back to the original, one species or all of
     them (PureRGB's README).
   - The options menu switches four matchups between Gen I's and Gen II's: Ghost on Psychic, Ice on Fire, Bug on Poison
     and Poison on Bug (`FEATURES.md:54`, Ghost super effective on Psychic by default).
   - PokeDaisy reads only the ROM's types (`SpeciesTypesPureRgb.kt`) and chart (`TypeChartPureRgb.kt`).
   - *Decided*: follow the save. Where PureRGB keeps both settings is found in its source and read with the other RAM.
   - The types are then the ROM's with the switched species put back; the chart is the ROM's with the four options
     applied.

5. **PureRGB changed moves the reader names by id.**
   - Seven moves are renamed and changed: Counter is now Drain Punch, Constrict Siphon Snag, Slam Filthy Slam, and so on
     (`FEATURES.md:467-474`).
   - Some moves' power depends on the battle. Filthy Slam has 130 against a poisoned foe, and Heat Rush raises Special
     when a Fire type uses it.
   - Today `rom1.cpp` gives Counter -1 priority by the id in the data file's `sizes`. In PureRGB that id is Drain
     Punch's.
   - *Proposed*: priorities and Gen I's special cases are taken from each hack's constants (the script fails on a name
     it can't find). A move whose power depends on the battle is advised at its listed power, and the hack's notes say
     so.

6. **The open-source hacks publish no symbol files.**
   - Crystal Legacy and Yellow Legacy release IPS/BPS patches, and PureRGB BPS patches (their GitHub releases).
   - PokeDaisy says each rebuilds byte for byte from its tag (`Gen2Reader.kt:117`, `Gen1Reader.kt:147-149`,
     `fixtures/purered/README.txt`: PureRGB with rgbds 1.0.3). Crystal Legacy's `rgbdscheck.asm` asks for rgbds 0.5.2
     or newer, Yellow Legacy's for 0.6.0, and PureRGB v2.7.7's for 1.0.0.
   - *Decided*: the user clones the three at their tags and builds them with the rgbds each needs.
     `pret-extract.py` reads their `.sym` and `constants/` as it reads pret's, and checks that each build's SHA-1 is
     the release's.
   - rgbds is a tool for the extraction only, never in the image. Its versions go in `kiwami/DEPENDENCIES.md`, with
     the hacks' commits.

7. **Crystal Clear's tables are only partly located.**
   - PokeDaisy locates its names, base data, moves, items, type chart, landmarks and map groups
     (`gen_gen2_tables.py:128-157`), and its Pokédex pages, pictures, palettes and descriptions (`Gen2Dex.kt:169-197`).
   - It doesn't locate the trainers, wild Pokémon, evolutions and learnsets, fishing, Headbutt and Rock Smash tables,
     hidden items, the Pokégear maps or the move priorities, which the Battle, Here, Map and Guide pages need
     (`rom2.cpp`'s 46 table keys).
   - Its text is packed with a dictionary (`textDict`, bank 0 `0x11FF`), which `RomView::text` doesn't decode
     (`rom.h:56-60`).
   - Its RAM is Crystal's except the map ids and caught flags (`Gen2Reader.kt:192-207`). It added landmarks past the Fast
     Ship, each placed on a region by a stand-in table (`landmark_regions`).
   - *Decided*: a fork script maps pokecrystal's symbols into each Crystal Clear build through the code that uses each
     one (PokeDaisy's `port_gen1.py` method, `scripts/port_gen1.py:1-8`), run on the user's ROMs. A symbol with no
     clear match fails the run, and every table found is checked against the ROM's contents as `rom2.cpp` checks
     today's.
   - It needs both versions' ROMs, and the Color Filter builds, which PokeDaisy says share every address with their
     Standard Colors builds.

8. **Only two of the four hacks have a PokeDaisy guide.**
   - PokeDaisy has Yellow Legacy's (`GuideGen1Gen.kt`'s `G1L_*` tables, `GuideGen1.kt:56`, `:174`) and Crystal
     Legacy's (`GuideCrystalLegacyGen.kt`).
   - It has none for Crystal Clear or PureRGB.
   - *Decided*: Crystal Clear's guide is built from the ROM's trainer classes, as `gsguide.cpp` builds Gold's and
     Silver's. Crystal Clear is open-world (the player picks a region to start in), so its order is by level within
     each region, and the plan checks that this makes sense on the ROM.
   - *Decided*: PureRed's and PureBlue's take Red's and Blue's PokeDaisy guide if PureRGB's constants give the same
     bosses and flags. Otherwise their guide is built from the ROM.

9. **Each hack's type chart, items and Pokédex size are its own; today the Gen I chart is hard-coded.**
   - `rules.h:31-41` gives Gen I's five differences from upstream's chart for every Gen I game. Yellow Legacy has its
     own chart (`TypeChartYellowLegacy.kt`), and PureRGB's depends on the save (oversight 4).
   - Items are numbered per game:
     - `items.inc` maps Gen II's ids to Gen IV's;
     - Crystal Legacy's TM01 is item `0xC1` (`Gen2Reader.kt:117`), and its bag holds 84;
     - Yellow Legacy's bag holds 59 and PureRGB's 30;
     - PureRGB adds items (the Hyper Ball, `FEATURES.md:218-226`).
   - Upstream's guide loader has the six games as rows (`guide-games.inc`), and `gbGame()` takes ids 100-105
     (`gbpk.h:19-20`).
   - *Proposed*:
     - the chart is read from every game's ROM (pokered's `TypeEffects`, pokecrystal's `TypeMatchups`). The retail
       games' are checked equal to today's in the ROM tests, and `rules.h` keeps the table for the game in use;
     - the items map comes from each hack's constants;
     - the hacks get GameIds from 106 up, rows in `guide-games.inc`, and their `dexSize` from the data file.

     All of this is in the fork's files: the hooks in `nds/model.cpp` (`chart`) and `nds/logic.cpp` already call into
     `rules.h`. No new upstream hook is expected.

10. **PureRGB's species and Pokédex aren't Red's.**
    - Its forms (Floating Weezing, the spirits) are internal species of their own that carry a base species' Dex number
      (`Gen1Reader.kt:170-172`).
    - Its base data is 35 bytes with the picture's bank in it, its Pokédex pages are fixed records with the category
      apart, and its text command for a far text is renumbered (`Gen1Dex.kt:32-42`, `:80-96`). It has its own
      character set (`Gen1Charset.PURE`).
    - It is a Color cart that switches D000 to WRAM bank 2 for its palettes. The cores map D000 as bank 1 always (#28,
      oversight 8), so the reads stay right.
    - *Proposed*: `rom1.cpp` reads these layouts, chosen by the data file, and a form shows as its base species, as in
      PokeDaisy. The plan decides whether a form's own types and stats are shown.

11. **The hacks move on.** PureRGB already has v2.7.7 (*decided*: read too) and its patches are versioned. Crystal
    Legacy and Yellow Legacy had releases in July 2026. A newer release shows "Not supported" until it is added. That
    is upstream's own rule for its GBA hacks (`docs/COMPANIONS.md`, *The GBA ROM hacks*). *Proposed*: accepted, and
    `kiwami/GB-POKEMON.md` says how a version is added: build it, run the scripts, add its SHA-1 and its captures if
    there are any.

12. **The rows stay the system's.** The hacks run under Settings' existing rows, Pokémon: Game Boy and Game Boy Color,
    by the folder they're started from (`kiwami/GB-POKEMON.md`, *On the game panel*). PureRGB is a Color cart that
    PokeDaisy's author keeps as `.gb` (`fixtures/purered/README.txt`), so it may be in either folder. *No change
    needed.*

## Suggestions

1. **Order by effort, each hack on the device before the next**: Crystal Legacy (Crystal's engine, symbols), Yellow
   Legacy (Yellow's engine, symbols), PureRed/PureBlue/PureGreen (new layouts, the save's types and chart), then
   Crystal Clear (the symbol-mapping script, packed text, the ROM-built guide). Cost: none; the plan decides.
2. **PokeDaisy's captures of the hacks as tests**: 22 folders, 1.1 MB, unchanged from `app/src/test/resources/
   fixtures/` at `135ac809`. They are crystal_legacy (4), yellow_legacy (4), crystal_clear (6, both versions),
   purered (6) and pureblue (2), each with its `README.txt` saying what's on screen. Like #28's 33, they go in
   `kiwami/tests/gbpk/fixtures/`. There are none for v2.7.7 or PureGreen.
3. **Read every game's type chart from its ROM** (oversight 9). This replaces `rules.h`'s hard-coded Gen I cells with
   one path for the retail games and the hacks. Cost: a table read and a ROM test.
4. **Issue texts for Polished Crystal and Prism** follow from the scope decisions. They can be written when the user
   wants them.

Left out on purpose: PokeDaisy's own features for these hacks (their fonts, party icons, the battle buttons'
cursors), and their non-English builds.

## Current state

**Which game a ROM is** (`ui/src/kiwami/gbpk/games.cpp`):

- `GAMES` (`:45-47`) lists the six keys and ids.
- `index()` (`:83-104`) reads each data file's `header_title`.
- `detect` (`:203-218`) matches the title at 0x134, so one title gives one game.
- `confirm` (`:220-240`) takes the whole file's SHA-1 among that game's `releases`.
- `loadData` (`:136-201`) refuses a key not in `GAMES` and fixes `dexSize` to 151 or 251 (`:199`).
- The ids are `gbpk.h:16-20` (100-105, `gbGame()` a range). The guide loader's rows are `guide-games.inc`, included in
  upstream's `nds/guide.cpp` by one `kiwami:` line.

**The data files** (`kiwami/rootfs/usr/share/banana/companions/gb-pokemon/<game>.ram.json`; schema in
`kiwami/GB-POKEMON.md`, *The data files*):

- One `ram`, `rom` and `sizes` section a game, shared by its releases.
- A release with `"hack": true` is a patch read as the game (Fast Red and Fast Blue, `gbpk-symbols.txt`'s `hack`
  lines, checked against `kiwami/tests/gbpk/roms.patches`).
- `pokedaisy-extract.py` leaves PokeDaisy's hacks out by name (`:96-98`, `:887-892`, `:2147`, `:2230`), and
  `pret-extract.py` reads pret's four repositories' `symbols` branches (`kiwami/tools/gbpk-symbols.txt`, 326 lines).

**The readers**:

- `reader.cpp` (608 lines) reads WRAM by the data file's addresses into `nds::Live`, with Gen I's
  `party_struct`/`battle_struct` (44/29 bytes) and Gen II's (48/32).
- `rom1.cpp` (515) and `rom2.cpp` (481) read the ROM's tables at the `rom` keys, checking each as they go.
- `pics.cpp` decodes Gen I's and Gen II's pictures, `maps.cpp` the town map and the Pokégear's.
- `rom.h:46-60` (`RomView`) decodes text in the generation's character set (`text.cpp`).
- Pokémon ids are national numbers. Gen I's go through the ROM's internal order (`GbRomData::national`).

**The rules** (`rules.h`):

- Gen I's chart is five hard-coded differences (`:31-41`), on while `oldChart` (`useRules`, `:22-25`).
- The damage, Speed and catch rules are reached from upstream's `logic.cpp` and `model.cpp` through four `kiwami:`
  lines.
- The pages' Gen-aware blocks decide by generation (`pages-impl.inc:27-31`, `kiwamiOld`), through upstream's
  `pages.cpp` hooks.

**Upstream hooks** (`git diff main...kiwami`): `main.cpp` 14 lines, `settings.cpp` 37, `settings.h` 8, `pages.cpp`
10, `logic.cpp` 4, `model.cpp` 2, `model.h` 1, `guide.cpp` 1, `settings_gamepanel.cpp` 1. Each calls into
`ui/src/kiwami/gbpk/`.

**Tests**:

- `kiwami/tests/gbpk-units.cpp` (2258 lines): PokeDaisy's 33 retail captures, made-up ROMs and the rules.
- `gbpk-ui.sh`: headless at both sizes.
- `gbpk-roms.sh` and `gbpk-roms.cpp`: the user's ROMs in `GB_ROMS`, by default `~/gb-companion/roms`, which today holds
  the nine retail files and Fast Red/Blue, and no hack.

**PokeDaisy for these hacks** (all under `app/src/main/kotlin/com/pokedaisy/app/companion/data/` unless named):

- **RAM**: `GEN2_CRYSTAL_LEGACY` (`Gen2Reader.kt:118-150`), `GEN2_CRYSTAL_CLEAR_V2_6_0` and `_V2_5_11`
  (`:192-207`), `GEN1_PURE_RED`/`_BLUE` (`Gen1Reader.kt:148-179`), `GEN1_YELLOW_LEGACY` (`:181-199`).
- **ROM tables**:
  - `Gen1Dex.kt:80-104` (PureRGB's and Yellow Legacy's Pokédex, pictures and palettes);
  - `Gen2Dex.kt:117-131` (Crystal Legacy's, its pages a bank and address each: `dexEntryFar`) and `:169-197`
    (Crystal Clear's);
  - `scripts/gen_gen2_tables.py:128-157` (Crystal Clear's names, moves, items, chart, landmarks, map groups);
  - `scripts/gen_gen1_tables.py` (Yellow Legacy's and PureRGB's).
- **Its tables of the hacks' content** (`MoveData*`, `ItemNames*`, `SpeciesTypes*`, `TypeChart*`, `Gen2Species*`): not
  taken, as for the retail games (#28, oversight 11).
- **Guides**: Yellow Legacy's (`GuideGen1Gen.kt:1111-1300`, `GuideGen1.kt:56-60`) and Crystal Legacy's
  (`GuideCrystalLegacyGen.kt`: bosses, areas, the grass and water odds).
- **SHA-1s** (`GameTitles.kt:188-215`): Crystal Clear 4, Yellow Legacy 1, Crystal Legacy 1, PureRed 1, PureBlue 1.
- **Captures**: the 22 folders of suggestion 2.

**The hacks' repositories** (GitHub, read 2026-11-13):

| | Repository | Tags | Licence |
|---|---|---|---|
| Crystal Legacy | `cRz-Shadows/Pokemon_Crystal_Legacy` | V1.3.1 | none, like pret's |
| Yellow Legacy | `cRz-Shadows/Pokemon_Yellow_Legacy` | V1.0.10 | none |
| PureRGB | `Vortyne/pureRGB` | v2.7.6, v2.7.7 (Red, Blue, Green patches each) | none |

Only facts about the builds are taken (addresses, constants, SHA-1s), as #28 takes them from pret.

## What has to change

**The fork's code** (`ui/src/kiwami/gbpk/`, GPL-3.0 with PokeDaisy's attribution):

- `games.cpp`/`games.h`: a title gives several games and the SHA-1 picks one (oversight 1); the hacks' keys and ids;
  `dexSize` from the data file.
- `gbpk.h`: the hacks' GameIds (106 up) and `gbGame()`; `guide-games.inc`: their rows.
- The data file schema: a release's own `rom` addresses where builds differ (Crystal Clear's two versions differ in
  one table); a game's `layout` choices (PureRGB's base data, Pokédex records and text command, Crystal Legacy's far
  Pokédex pointers); per-game item maps and move ids.
- `reader.cpp`: the hacks' bag sizes and pockets; PureRGB's type and matchup switches read from the save
  (oversight 4).
- `rom1.cpp`:
  - PureRGB's base data, Pokédex records, character set, forms and new items;
  - each game's chart from its ROM;
  - the priorities by each game's constants.
- `rom2.cpp`: Crystal Legacy's far Pokédex pointers and item ids; Crystal Clear's tables, its added landmarks' regions,
  and its packed text (`text.cpp`).
- `rules.h`: the chart of the game in use, from its ROM (oversight 9).
- `host.cpp`/`gsguide.cpp`: the guides (oversight 8).

**Data and scripts**:

- `pret-extract.py`: the three hacks' repositories with their builds' `.sym`, checked against the releases' SHA-1s.
  `gbpk-symbols.txt` gets their lines.
- A new script maps pokecrystal's symbols into Crystal Clear (oversight 7).
- `pokedaisy-extract.py`: takes the hacks' configs (`Gen1Config`/`Gen2Config` `.copy`s, `Gen1DexTables`,
  `Gen2DexTables`), and from `scripts/gen_gen2_tables.py`'s `GAMES` Crystal Clear's locations (Python, read with
  `ast`, not run). It converts the Legacy hacks' guides. The rest of PokeDaisy's hacks stay left out by name.
- New data files in `kiwami/rootfs/usr/share/banana/companions/gb-pokemon/`, one per game: `crystal-legacy`,
  `yellow-legacy`, `purered`, `pureblue`, `puregreen`, `crystal-clear` (names fixed by the plan); and the Legacy hacks'
  guide files.
- `kiwami/DEPENDENCIES.md`: the three repositories' tags and commits, and rgbds 0.5.2, 0.6.x and 1.0.x as extraction
  tools.

**Tests**: PokeDaisy's 22 captures into `kiwami/tests/gbpk/fixtures/` with expected values in `gbpk-units.cpp`;
made-up ROMs for each new layout; the hacks' lines in `roms.sha1` (optional, the user's own files) and their checks
in `gbpk-roms.cpp`.

**Documentation**: `kiwami/GB-POKEMON.md` (the hacks, their sources, how a version is added).

**Hooks in upstream's files**: none new expected. The existing ones (`model.cpp`'s chart, `logic.cpp`'s rules,
`pages.cpp`'s blocks, `guide.cpp`'s rows) call into the fork's files, where every change above lands. If the plan
finds one, it is named there with why.

## Options

- **A. Extend #28's companion, one data file and reader layout per hack** (chosen). The same pages, readers and
  tests, with the hacks as games of their own. The cost is in the scripts (three builds, Crystal Clear's mapping) and
  in the readers' new layouts, all in the fork's files.
- **B. Read the hacks as their base games where they share the engine, as Fast Red is read.** Cheaper, but every hack
  moved its addresses (Yellow Legacy's 78 bytes from `wPlayerMoney`, Crystal Legacy's 11 in bank 1), so it would read
  garbage. Rejected.
- **C. Take PokeDaisy's tables of the hacks' content.** No ROM layouts to learn, but it puts the hacks' names, moves
  and tables in a public repository, against #28's rule, and their authors' content at that. Rejected.

## Risks

- **Performance and footprint.** At run time it's the same as #28: one set of reads every 250 ms and the ROM read
  once, and nothing runs with the rows off or for another game. The image gets six data files and two guides, tens of
  KB before squashfs. The binary grows by the new layouts, probably under 100 KB.
- **Conflicts with upstream.** No new upstream hook is planned. The existing ones are in `pages.cpp`, `logic.cpp`,
  `model.cpp`, `guide.cpp`, `main.cpp` and the settings files, which upstream changes often (#28, oversight 1).
- **Crystal Clear's mapping** is the largest unknown. A symbol whose code the hack rewrote has no match, and that
  table is then missing until it's found by its contents. The script reports it rather than guessing.
- **PureRGB's switches**: if the source keeps the type switch per species in a way the RAM reader can't read whole,
  the types follow the ROM and the notes say so. This is checked first in the plan.
- **Builds**: a build that doesn't match its release's SHA-1 (wrong rgbds, a tag that isn't the release) stops the
  extraction. PureGreen and v2.7.7 have no PokeDaisy captures, so their RAM is checked only on the device and by the
  symbols.
- **Hardware and boards**: the same as #28. The pages lay out at 640x480 and 1024x768, and only the RG DS Plus is
  there for device tests.
- **Correctness**: a hack's changed rule that isn't followed (a move's conditional power, PureRGB's Onix) gives
  advice off by that much; the notes list what isn't followed.

## Testing

**Headless** (`kiwami/tests/ci.sh`):

- `gbpk-units`: each hack's captures read into the pages' data, checked against their READMEs, the same way as #28's
  33.
- Each new layout on made-up ROMs: PureRGB's base data, Pokédex records and character set; Crystal Legacy's far
  pointers; Crystal Clear's packed text; a chart read from the ROM.
- Detection: one title, several games, the SHA-1 picks; an unknown SHA-1 with a hack's title is "Not supported".
- PureRGB's switches applied to types and the chart.
- The scripts' `--check`, and the scan for the games' names over the new data files.
- `gbpk-ui.sh`: a hack's capture through the panel at both sizes.

**ROM tests** (`gbpk-roms.sh` with `GB_ROMS`): each hack's tables read and checked as the retail games' are (names,
a known species, Route 1's or 29's wild Pokémon, the first leader's team, every hidden item on a named map, the
pictures and maps drawn), the retail games' charts read equal to today's hard-coded ones, and the guides' bosses
found.

**On the device (RG DS Plus)**: each hack started from the Game Boy Color row (PureRGB from the Game Boy row too), in
BananaFront and RetroArch, with Gambatte and SameBoy. Check the party, a wild and a trainer battle (advice against the
game's own screens), the bag, the Dex, the map, and the guide's next milestone. For PureRGB, a species switched back
at the NPC and a matchup option changed. Logs: `/data/banana/logs/banana-ui.log` (`gb companion: <title>
(<release>) read in <N> ms`, the reader's and the guide's lines).

## Open questions

1. PureRGB's builds? *Answered*: all six, PureRed, PureBlue and PureGreen at v2.7.6 and v2.7.7, built from the two
   tags.
2. Which hacks are in #30? *Answered*: Crystal Legacy, Yellow Legacy, Crystal Clear, and PureRed/PureBlue (and
   PureGreen). Polished Crystal and Prism get issues of their own.
3. The GBA and DS hacks? *Answered*: their own issues (texts written in this session). Unbound, Lazarus and Seaglass
   are upstream's already. Diamond and Pearl too.
4. Where the open-source hacks' addresses come from? *Answered*: built from their tags by the user; `.sym` read by
   `pret-extract.py`.
5. Crystal Clear's missing tables? *Answered*: a fork script maps pokecrystal's symbols through the code that uses
   them, run on the user's ROMs.
6. Guides for the hacks without PokeDaisy's? *Answered*: built from the ROM (Crystal Clear), or Red's and Blue's if
   they check out (PureRGB).
7. PureRGB's switched types and matchups? *Answered*: followed from the save.
8. The ROMs? *Answered*: the user has the versions listed.
9. Issue #30's text lists Polished Crystal and Prism and not PureGreen or v2.7.7. *Answered* (planning, 2026-11-13): an
   updated text is written with the plan, for the user to paste into #30.
10. PureRGB's six types of its own (Typeless, Crystal, Bonemerang, Tri, Floating, Magma), found in planning? *Answered*:
    values of the fork's past upstream's 18, with three hook lines naming and colouring them (plan, *Refinements*).
11. PureRGB's signature moves (`RemappableMoves`)? *Answered*: followed, from the ROM, unless the save turned them off;
    the battle-state ones at their listed power.
12. PureRGB's forms? *Answered*: their own types and stats on the Battle, Party and Team pages, the base species in the
    Dex.
13. Crystal Clear's four SGB Mode builds (found in the user's ROMs, in no list of PokeDaisy's)? *Answered* (2026-11-13):
    read too; Crystal Clear is eight releases.
14. The hacks are in `~/gb-companion/roms/romhacks/`? *Answered*: `gbpk-roms.sh` reads subfolders one level deep.
