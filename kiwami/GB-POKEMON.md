# The Game Boy Pokémon companion

**Based on PokeDaisy by Lidor Itzhari - https://github.com/lidor30/pokedaisy**

The kiwami fork's companion for Pokémon Red, Blue, Yellow, Gold, Silver and Crystal on the Game Boy and Game Boy
Color: while one of them plays on one panel, the other shows the pages upstream's companion shows for the Game Boy
Advance and DS games (Battle, Party, Team, Here, Map, Guide, Dex, Log). It is the fork's own, not upstream's, and is
off by default. Its investigation and plan are in `.workspace/0028-gb-pokemon/`.

The games are the English retail releases: Red, Blue and Yellow (USA, Europe), Gold and Silver (USA, Europe), Crystal
(USA, USA and Europe rev 1, Australia); and two ROM hacks read as the games they patch, Fast Red and Fast Blue (v1.0),
whose patch moves nothing the companion reads (from Red, Blue and Yellow's data files on). A release is told by the
whole file's SHA-1.

## PokeDaisy, and the licence

PokeDaisy (`lidor30/pokedaisy`, pinned at `135ac809`, 1.5.0.1) is an Android companion for the same games. It is
GPL-3.0 with an additional term under section 7(b): whoever conveys it, or a work based on it, keeps the line above in
a `NOTICE` kept with the source, in the work's README or equivalent documentation, and where the work shows credits.

The fork takes from PokeDaisy:

- **Its data**: the games' RAM addresses, the releases' SHA-1s, and later its guide tables' structure and its own
  words, extracted by a script (below). Nothing of the games' own content: their names, text and tables are read from
  the player's ROM at run time, as upstream's companions do.
- **Its readers' design**: the companion's C++ (`ui/src/kiwami/gbpk/`) follows PokeDaisy's Kotlin readers.

So the companion's sources and data are GPL-3.0 with PokeDaisy's 7(b) term, and **the fork's banana-ui, which links
them, is a GPL-3.0 work as a whole**: it is offered with its source (this repository), and none of the companion's
code can go to upstream, which is MIT. Banana's MIT code is compatible with that.

Where the attribution is:

- `NOTICE` and `LICENSE` (PokeDaisy's GPL-3.0 text) beside every GPL-3.0 folder: the image's
  `/usr/share/banana/companions/gb-pokemon/` (`kiwami/rootfs/usr/share/banana/companions/gb-pokemon/`), and from later
  steps `ui/src/kiwami/gbpk/`, `kiwami/tests/gbpk/` and `kiwami/data/pokedaisy/`.
- This file, the fork's documentation of the companion.
- Settings > System > Credits: `kiwami/post-build.sh` adds the line under *Thanks* in the image's
  `/etc/banana/credits.txt`, once, and stops the build if that file has no `[Thanks]` section.
- `kiwami/DEPENDENCIES.md`.

## Where the data comes from

Two sources, each read by its own script into files that are committed, so a build needs neither:

- **PokeDaisy**, by `kiwami/tools/pokedaisy-extract.py`: the RAM addresses in its readers' configs (`Gen2Reader.kt`'s
  `Gen2Config`, read by a small evaluator of its named arguments, `wramx(bank, addr)` and `.copy(...)`), Crystal's
  guide RAM (`GuideCrystalGen.kt`), and the releases' SHA-1s (`GameTitles.kt`). The configs' fields that are the
  games' tables (landmarks, gender ratios, TM items, the Pokédex pages' ROM tables) are left out by name; a field it
  can't read fails the run, listed under `unread:`.
- **pret's disassemblies** (`pret/pokecrystal`, `pret/pokegold`, and for Red, Blue and Yellow `pret/pokered` and
  `pret/pokeyellow`), by `kiwami/tools/pret-extract.py`, into `kiwami/tools/gbpk-extra.json`: what PokeDaisy doesn't
  have (Gen II's badges, the daycare, the roamers, the stat stages, the event flags; later the ROM tables' offsets),
  and each release's SHA-1 (`roms.sha1`) and ROM header (the title and game code its Makefile gives `rgbfix`).
  `kiwami/tools/gbpk-symbols.txt` lists every value taken: an address is looked up in the release's `.sym` on the
  repository's `symbols` branch, a constant evaluated from its `constants/*.asm` file (rgbds' `const` macros and the
  file's own `MACRO`s). pret publishes no licence; only these facts about the retail ROMs are taken, none of its code
  or tables, as PokeDaisy's own `NOTICE` describes its use of them.

What the two agree on is checked: every address PokeDaisy gives is also looked up in pret's symbols and must be the
same, a game's releases (Crystal's three) must agree on every address, and PokeDaisy's SHA-1s for a game must be
pret's. Each value in a data file says where it came from (`src`: `pokedaisy`, `pret (<symbol>)`, or both).

`SOURCES`, in the image's folder, has a section for each script: the commits read, and the SHA-256 of every file read
(`in`) and written (`out`).

### Running the extraction again

With PokeDaisy and the four pret repositories cloned beside each other (`~/Documents/Projects/pokedaisy`,
`~/Documents/Projects/pret/<name>`), each pret clone with its `symbols` branch
(`git clone https://github.com/pret/<name>`, then `git fetch origin symbols:symbols`):

```sh
python3 -I kiwami/tools/pret-extract.py ~/Documents/Projects/pret          # first: gbpk-extra.json, SOURCES' [pret]
python3 -I kiwami/tools/pokedaisy-extract.py ~/Documents/Projects/pokedaisy   # the data files, LICENSE, [pokedaisy]
```

`pokedaisy-extract.py` refuses a checkout at another commit than its `PIN` unless `--any-commit` is given; taking in a
newer PokeDaisy is changing `PIN`, running it, and reading the diff. Each script with `--check` and its checkout
writes to a temporary folder and compares with what is committed; with `--check` alone (as `kiwami/tests/ci.sh` runs
them) it verifies the committed files against `SOURCES`' hashes, and `pret-extract.py` that every line of
`gbpk-symbols.txt` has its value. A re-run is byte for byte the same.

## Which game a ROM is

`ui/src/kiwami/gbpk/games.cpp`, when a game starts from Game Boy or Game Boy Color with the system's row on (the game
panel calls it from step 8 of the plan):

1. The ROM's first 0x150 bytes are read: the file, or the first `.gb`/`.gbc` in a `.zip` or `.7z` in name order (as
   RetroArch takes it; a `.7z` through the fork's LZMA SDK reader, `ra7z.cpp`, not `7zz`).
2. Its header's title (0x134) is matched against each data file's `header_title`. Another title: no companion, nothing
   else read. The title fills 16 bytes on the first cartridges and 11 when a game code follows; a shorter one ends
   with a 0, so `PM_CRYSTALX` isn't Crystal.
3. The whole file's SHA-1 must be one of the file's `releases`. Another SHA-1, a known title (a European Yellow, a ROM
   hack other than Fast Red and Fast Blue, a bad dump): "Not supported", and nothing is read from the game's memory.

The six games are values of upstream's `nds::Gen` and `nds::GameId` (`gbpk.h`: `GEN_I` 16, `GEN_II` 17, `RED` 100 to
`CRYSTAL` 105), so upstream's model, session and pages take them unchanged. Names are decoded from the games' English
character set, pret's `constants/charmap.asm` (`text.cpp`), the only reference this feature uses for the games' rules
and layouts (Bulbapedia refuses automated access).

## Reading the game's memory

`ui/src/kiwami/gbpk/reader.cpp` asks the emulator for the game's WRAM as upstream's GBA companion does: a set of
`READ_CORE_MEMORY` lines every 250 ms, the next only once the last was answered, asked again after 10 s without an
answer, sent on RetroArch's stdin or through BananaFront's shared memory. The data file's addresses are read in one
set, neighbours merged into one read: Gen II is 10 or 11 reads, about 1.3 KB. A set whose replies are all there makes
a snapshot; an emulator that answers with no data (`-1 no memory map defined`) is reported, and nothing is shown.

The snapshot becomes upstream's `nds::Live`, in the DS companion's conventions, so upstream's session, logic and pages
take it:

- species are national numbers already; an egg is the species list's EGG (0xFD);
- items are their Gen IV numbers where upstream's logic knows them by number (`items.inc`: the balls, the Apricorn
  balls, the type-boosting items, Light Ball, Thick Club, the rods, the stones, the trade items), checked against
  pret's ids for Gold, Silver and Crystal; any other item is 1000 + its Gen II id. The Bug-Catching Contest's Park Ball
  (x1.5) is Gen IV's Sport Ball. TM01-50 and HM01-07 are Gen IV's 328-377 and 420-426;
- the DVs go in the IVs' places (HP's from the others' lowest bits, Special as both special stats), the stat
  experience in `statExp` (a hook in `nds::Mon`: HP, Attack, Defense, Speed, Special), and the EVs stay 0;
- the gender is the DVs' (pret's `GetGender`: female while Attack << 4 | Speed is at most the species' ratio), and
  the shininess too (`CheckShininess`: Defense, Speed and Special 10, Attack's bit 1 set), through a `pid` made of the
  DVs and the OT's id and an `otSecret` that make upstream's `Mon::shiny()` give the DVs' answer;
- a battle (`wBattleMode` 1 wild, 2 trainer) has our battler from the battle's copy over the party member out, the
  foe's from `wEnemyMon` (over the trainer's party member out), the battle's own types and stat levels (7 none), and
  a trainer's whole party and class << 8 | number;
- the place is the map's group << 8 | number, with the coordinates; the time of day, the badges (Johto's, then
  Kanto's), the Pokédex, the bag's four pockets (none until every list is well formed, as PokeDaisy reads it), the
  money, the play time, the day-care and the roamers.

The snapshot counts as the game's only once the party has a count of 1-6 whose species list matches the structures:
before a save is loaded WRAM holds leftovers.

### The test captures

`kiwami/tests/gbpk/fixtures/` holds PokeDaisy's RAM captures of Gold, Silver and Crystal (17 folders, unchanged from
its `app/src/test/resources/fixtures/` at `135ac809`, with their `README.txt`s), under its GPL-3.0 (the folder's
`LICENSE` and `NOTICE`). `kiwami/tests/gbpk-units.cpp` answers the reader's requests from them as the cores' memory
maps would (C000-CFFF bank 0, D000-DFFF bank 1, banks 2-7 from 0x10000) and checks what each README says: CHRIS's
CHIKORITA with a BERRY on Route 29 at (46, 11) in all three Crystals, the wild battles, Gold's party, bag, money and ID,
the CHAMPION's six in `gold_trainer`.

## The battle rules

Upstream's logic works out the Battle page's advice (each move's damage, who moves first, each ball's catch chance)
for Generations III to V. For Gen I and II, four hook lines in `ui/src/nds/logic.cpp` and `model.cpp` hand it to
`ui/src/kiwami/gbpk/rules.h`, which follows the games' own code in pret's disassemblies (named at each rule there):

- **The type chart** is Gen II's in upstream's table already. Gen I's differs in five ways (pokered's
  `data/types/type_matchups.asm` against pokecrystal's): no Steel or Dark (neutral both ways), Ghost does nothing to
  Psychic, Bug and Poison are super effective on each other, Ice is neutral on Fire. The Team page's coverage still
  counts 17 types in Gen I, so its score there tops out 2 below the others' (accepted in the plan).
- **Damage**: physical or special by the move's type (Fire and after special; Gen I's one Special stat is both special
  stats); the battle copy's Attack and Defense, which already hold the stat stages, the badges' stat boosts, burn's
  halved Attack and paralysis's quartered Speed, so none of them is applied again; Gen II's Thick Club and Light Ball;
  both stats / 4 above 255; `((2 * level / 5 + 2) * power * Attack / Defense) / 50`; Gen II's type items + 10 % (Dragon
  Scale is Dragon's, Dragon Fang does nothing, the Pink and Polkadot Bows are Normal's); at most 997, + 2; Gen II's
  badge type boost, + 1/8 for the player's moves of a badge's type (the badges come from the game through the host);
  STAB + 1/2; the type matchups; the random factor 217-255 / 255. Not counted, as upstream doesn't for the DS games:
  critical hits, Reflect and Light Screen, weather, Explosion's halved Defense.
- **Speed**: the battle copy's, which holds the stages and paralysis already.
- **Catching**, with the games' bugs: Gen I's two random numbers (worked out as the probability), and Gen II's rate
  where only sleep and freeze add (10), the HP part is reckoned in bytes (a Pokémon above 341 HP looks easier to
  catch than it is, as in the game), the Level Ball skips the HP part, the Moon Ball never boosts, the Love Ball wants
  the same gender, the Fast Ball counts only Magnemite, Grimer and Tangela, and the Heavy Ball goes by the Pokédex
  weight. Master Ball: certain.
- **Hidden Power** (Gen II) from the DVs, and a stat from its base, DV, stat experience and level (`statFrom`), for the
  guide built from the ROM.

`kiwami/tests/gbpk-units.cpp` checks each rule through upstream's own entry points (`effectiveness`, `damage`,
`effectiveSpeed`, `catchChance`), with the expected values worked by hand from pret's code in its comments.

## Reading the ROM

What the pages show of the games themselves (names, base stats, moves, items, learnsets, evolutions, the Pokédex,
wild Pokémon, trainers, place names) is read from the player's own ROM when the game starts, by
`ui/src/kiwami/gbpk/rom.cpp` and, for Gold, Silver and Crystal, `rom2.cpp`, into upstream's `nds::RomData` in the DS
companion's conventions, as upstream's `ui/src/gba/gen3rom.cpp` does for the GBA games. It takes under a millisecond
on a PC. Where each table is comes from the data file's `rom` section; its layout is pret's, named where it is read.

- **Each table is checked as it is read**: every base data record names its own species and the first is Bulbasaur's,
  every move record its own move and the first is Pound's, the lists end where pret's do, the map groups' headers are
  whole, and a pointer that leaves its bank stops the read. A ROM that fails gives no tables and says which.
- **Species**: name, types, base stats (Gen II's Sp. Atk and Sp. Def their own), catch rate, base experience, gender
  ratio, growth (the same ids as upstream's later games), base friendship (70, the games' `BASE_HAPPINESS`), the TMs
  and HMs it learns (HM01-07 as 93-99), the evolutions (in upstream's `nds::evo` numbering with upstream's words:
  level, stone, trade with or without an item, friendship any time, by day or at night, Attack against Defense), the
  moves learned by level, and the Pokédex entry: category, height (feet and inches to 0.1 m) and weight (0.1 lb to
  0.1 kg the way the game reckons it for the Heavy Ball, the pounds halved less a 16th and a 32nd of that: 0.453 a
  pound where the true factor is 0.4536, so the pages show what the game works with), and both pages of text.
- **Moves**: name, type, power, accuracy and effect chance (the games' 0-255 as a percent), PP, the description, the
  category by the type (Normal to Steel physical, Fire to Dark special), and the priority from the game's own table
  (`MoveEffectPriorities`: Protect, Detect and Endure +2, Quick Attack, Mach Punch and ExtremeSpeed +1, Roar,
  Whirlwind, Counter and Mirror Coat -1) and Vital Throw's -1, kept as upstream does (signed, 0 the normal).
- **Items** below TM01: name, price, pocket and description, at the Gen IV id `items.inc` gives or 1000 + the Gen II
  id; the TMs and HMs as Gen IV numbers them (TM01 328, HM01 420), each teaching the move `TMHMMoves` names.
- **Places**: each map of each group (`MapGroupPointers`; the last group ends where the first map's attributes start)
  by map group << 8 | number, named by its landmark, with its wild Pokémon:
  - grass by morning, day and night, each slot with its chance from the game's `GrassMonProbTable`;
  - water, whose level the game raises by 0-4 at random (`ChooseWildEncounter`), so a slot spans five levels;
  - the old, good and super rods by the map's fishing group, each entry's chance from the table's cumulative bytes as
    the game rolls them, and the time-dependent ones as their day species by morning and day and their night species
    at night. The games give every map with no water the first group (Shore), so a Shore map fishes only when it has
    water to surf: Crystal's 388 maps have 117 with wild Pokémon, none of them a building, a tower or a gate;
  - Headbutt's common and rare trees, and Rock Smash, from `TreeMonMaps` and `RockMonMaps`.
  The swarms (Dunsparce, Yanma, Qwilfish, Remoraid), which replace a map's tables while a swarm is on, aren't listed.
- **Trainers** by class << 8 | number, the key the battle's trainer has in memory: the class's name, the trainer's,
  and the team with its items and moves where the party has them, else the four moves the game gives (the last four
  learned by the Pokémon's level).

Names are written as the pages write them ("CHIKORITA" to "Chikorita", "HP UP" to "HP Up", "NEW BARK TOWN" to "New
Bark Town"); the descriptions and the Pokédex's text as the games write them, their line breaks as spaces.

### The pictures and the maps

`ui/src/kiwami/gbpk/pics.cpp` and `maps.cpp`, as pret's code draws them:

- **Gen II's LZ** (`home/decompress.asm`): literal, iterate, alternate, zero fill, and the three copies from what is
  already written (repeat, bit-flipped, reversed; a negative offset back from the end, or a positive one from the
  start), each in its short form or the long one, whose own command 7 copies as a repeat (the code falls through). A
  stream that runs past its bank, a copy that reaches outside what is written, or more than the game's buffer
  (`wDecompressEnemyFrontpic`, 0x800 bytes) is refused.
- **A species' front picture**, decoded when a page first draws it (upstream's pages decode on their own thread):
  `PokemonPicPointers`' bank and pointer (`UnownPicPointers`' first, letter A, for Unown), the bank fixed as
  `FixPicBank` does (Crystal: an index into `PicsBanks` less `PICS_FIX`, $36, a constant of pret's code kept in
  `pics.cpp`; Gold and Silver: a list of pairs), its 5, 6 or 7 tiles square (`BaseData`'s picture size; any other
  as 7, `PadFrontpic`) in columns, at that size, not padded: the pages crop a picture to what is drawn. Its palettes
  are the game's: white, the species' two colours from `PokemonPalettes`, black, and the shiny's two after them.
  Pixel 0 (white) is left clear, as the games' background. Crystal's animation frames, which follow the first in the
  same stream, aren't read: the pages show a still picture.
- **The Pokégear's Johto and Kanto maps** (`engine/pokegear/pokegear.asm`): the screen's 20 x 18 tiles
  (`JohtoMap`, `KantoMap`) over `TownMapGFX`'s 48 tiles (LZ), each tile in the palette `TownMapPals`' map gives its id,
  from Chris's Pokégear palettes (Kris's differ only in the cities' colour, and the map is read once, not by the save:
  hers show orange cities). A place's spot is the game's 16 x 16 player icon around its landmark's point
  (`Landmarks`), on the map the Pokégear picks (from `KANTO_LANDMARK` Kanto's, the Fast Ship's Johto's); a place with
  the special landmark (rooms the game puts on no map) is on none, and the Map page says the player is inside. A tap
  on the map names the landmark of that map nearest the cell's middle, a cell away at most. A map whose picture
  doesn't decode keeps its cells, which the pages draw as squares.

`kiwami/tests/gbpk-units.cpp` reads a made-up 64 KB ROM with every table and checks each field read back, and that a
ROM whose first species isn't Bulbasaur, whose palettes' first entry isn't the games' or whose Bulbasaur doesn't
decode, or a data file without a table, gives none. It checks the LZ command by command against bytes worked from
pret's code, a made-up picture's pixels and palettes, Gold's bank pairs, and the made-up maps' pictures, cells and
spots. `kiwami/tests/gbpk-roms.sh` reads the player's own ROMs (*The ROMs for the tests*).

## The data files

`/usr/share/banana/companions/gb-pokemon/<game>.ram.json` (`crystal`, `gold`, `silver`; Red, Blue and Yellow from
step 12), JSON with sorted keys:

```json
{ "schema": 1, "kind": "gb-pokemon-ram", "game": "crystal", "gen": 2, "title": "Crystal",
  "header_title": "PM_CRYSTAL",
  "releases": [ { "sha1": "f2f52230...", "name": "USA, Europe (Rev 1)", "code": "BYTE" } ],
  "ram": { "partyCount": { "bank": 1, "addr": "0xDCD7", "src": "pokedaisy pret (wPartyCount)" } },
  "sizes": { "itemsMax": 20, "ballsMax": 12, "keyItemsMax": 25, "speciesCount": 251, "eventFlagBytes": 256,
             "numPokemon": 251, "numMoves": 251, "firstTm": 191 },
  "flags": [],
  "rom": { "baseData": { "bank": 20, "addr": "0x5424", "src": "pokedaisy pret (BaseData)" } } }
```

- `header_title` is the ROM header's title (0x134), the same for a game's releases; `code` is a release's game code
  (0x13F), which only the Game Boy Color games have (Crystal's Australian release has its own, `BYTU`).
- A `ram` address's `bank` is 0 for C000-CFFF and HRAM, 1 for the D000 bank the cores map there, and from 2 up
  another WRAM bank, read at `0x10000 + (bank - 2) * 0x1000 + (addr - 0xD000)` through `READ_CORE_MEMORY`.
- `sizes` are counts and ids: the pockets' capacities, the species, the bytes of event flags (pret's `NUM_EVENTS` /
  8), the species, moves, trainer classes and landmarks (pret's constants), Vital Throw's move id, Kanto's first
  landmark and the Fast Ship's (`kantoLandmark`, `fastShipLandmark`: which Pokégear map a place is on), and TM01's
  item id (PokeDaisy's `firstTm`).
- `rom` is where each ROM table is: its bank and its address in the bank's window (bank 0 at 0000-3FFF, the others at
  4000-7FFF), from pret's symbols, and where PokeDaisy's `Gen2Dex.kt` gives the same table, checked equal to its
  value (`src` says which), and `Gen2Art.kt` the Pokégear's map tables (its `townMaps` are `johtoMap` and
  `kantoMap`). Crystal looks a Pokédex entry's bank up in a table (`dexEntryBanks`); Gold and Silver count it from the
  first entry's (`dexEntryFirst`, checked against PokeDaisy's `dexEntryFirstBank`). Crystal fixes a picture's bank by
  `picsBanks`, Gold and Silver by `picBankFixes`.
- `flags` (the flag banks the guides read) is filled by a later step.

## The ROMs for the tests

The companion's ROM reader is checked on the player's own ROMs, which are never added to this repository (it is
public, and nothing of the games is in it or in the image). `kiwami/tests/gbpk/roms.sha1` lists what the tests expect,
one line a file: its SHA-1, the folder it goes in (`gb`, `gba`, `nds`), `required` or `optional`, the game, and its
No-Intro name. The SHA-1s are PokeDaisy's (`GameTitles.kt`), which are pret's `roms.sha1` for the games pret has
disassembled; `gbpk-units` checks that the Game Boy lines and the data files' releases are the same, both ways.

The Game Boy and Game Boy Color games go **unpacked** (`.gb`, `.gbc`) in `GB_ROMS`, by default `~/gb-companion/roms`,
outside the repository so that no `git add` can pick one up:

| No-Intro file name | SHA-1 | |
|---|---|---|
| `Pokemon - Red Version (USA, Europe) (SGB Enhanced).gb` | `ea9bcae617fdf159b045185467ae58b2e4a48b9a` | required |
| `Pokemon - Blue Version (USA, Europe) (SGB Enhanced).gb` | `d7037c83e1ae5b39bde3c30787637ba1d4c48ce2` | required |
| `Pokemon - Yellow Version - Special Pikachu Edition (USA, Europe) (CGB+SGB Enhanced).gb` | `cc7d03262ebfaf2f06772c1a480c7d9d5f4a38e1` | required |
| `Pokemon - Gold Version (USA, Europe) (SGB Enhanced) (GB Compatible).gbc` | `d8b8a3600a465308c9953dfa04f0081c05bdcb94` | required |
| `Pokemon - Silver Version (USA, Europe) (SGB Enhanced) (GB Compatible).gbc` | `49b163f7e57702bc939d642a18f591de55d92dae` | required |
| `Pokemon - Crystal Version (USA, Europe) (Rev 1).gbc` | `f2f52230b536214ef7c9924f483392993e226cfb` | required |
| `Pokemon - Crystal Version (USA).gbc` | `f4cd194bdee0d04ca4eac29e09b8e4e9d818c133` | optional |
| `Pokemon - Crystal Version (Australia).gbc` | `a0fc810f1d4e124434f7be2c989ab5b5892ddf36` | optional |
| `Pokemon - Red Version (USA, Europe) (Fast Red) (v1.0) (80C).gb` | `e7bfd2c2245597da5016e682fca8f19665c6cfd0` | optional |
| `Pokemon - Blue Version (USA, Europe) (Fast Blue) (v1.0) (80C).gb` | `5030c1114554207191f2e6ffb1f2920cf3d7c668` | optional |

A file is found by its SHA-1, whatever it is called; a name other than the list's is only noted (older No-Intro sets
write Yellow's `(GBC,SGB Enhanced)`). The two optional Crystals share every address with rev 1, so the ROM tests'
Crystal checks run on each that is there.

Fast Red and Fast Blue are a ROM hack, the same 72-byte patch on Red and on Blue: running with B held, and a faster
bike. It changes `OverworldLoopLessDelay.moveAhead2` and `DoBikeSpeedup` (pret's `home/overworld.asm`), puts its
routine in the zeros before the header (0x00D0-0x00F6), and keeps one flag in WRAM D711, a byte pret's `pokered` leaves
unused; with the header's global checksum, that's all. No RAM address or ROM table the companion reads moves, so they
are read as Red and Blue: their SHA-1s are releases of `red` and `blue`. `kiwami/tests/gbpk/roms.patches` lists each
hack with its retail ROM's SHA-1 and the ranges its patch changes, and the check below compares the two files when both
are there, failing if a byte differs outside those ranges. Another hack, or another version of these, is "Not
supported" until it is checked the same way and added.

The English releases of the other mainline games PokeDaisy supports are listed too, all optional, for a later
comparison of upstream's guides with PokeDaisy's tables; nothing reads them yet. They go in `GBA_ROMS` (by default
`~/gba-companion/roms`, upstream's own test folder for its GBA companion) and `NDS_ROMS` (by default
`~/nds-companion/roms`): Ruby and Sapphire (USA, and USA, Europe rev 1 and rev 2), Emerald, FireRed and LeafGreen
(USA, and rev 1), Platinum (rev 1), HeartGold, SoulSilver, Black, White, Black 2 and White 2. Which SHA-1 is which
revision of Ruby, Sapphire, FireRed and LeafGreen follows the order of pret's `roms.sha1` and is to be confirmed on the
files themselves. PokeDaisy has no Diamond or Pearl; its non-English releases and the ROM hacks are left out.

The folders are checked, and the ROM reader run on the releases there, with:

```sh
kiwami/tests/gbpk-roms.sh [<pokedaisy checkout>]    # GB_ROMS, GBA_ROMS, NDS_ROMS to look elsewhere
```

It prints a line for each listed file: `ok`, `ok (named <its name>)`, `missing`, or `wrong file: sha1 <...>` for a file
with a listed name and other bytes (a European release, a ROM hack, a bad dump), and names the files the list hasn't.
Then each hack of `roms.patches` that is there is compared with its retail ROM. A required file missing or wrong, or a
hack that differs outside its patch, fails it; the optional files missing and the Game Boy Advance and DS folders
never do. Without
`GB_ROMS`' folder it says it's skipped and succeeds, as in CI, where `kiwami/tests/ci.sh` runs it (and checks it on
made-up folders).

Then it builds `kiwami/tests/gbpk-roms.cpp` and reads each release there through the companion's ROM reader, logging
each file and how long the read took. For Gold, Silver and Crystal it checks that all 251 species and moves are
named; Chikorita's base stats, types, evolution, first move, Pokédex category, height and weight; the priorities of
Quick Attack, Vital Throw and Protect; TM01 and HM01's moves; item names and prices; Route 29's morning grass with
Pidgey and Sentret; fishing at New Bark Town and none on Route 29; Falkner's Pidgey Lv 7 and Pidgeotto Lv 9; every
place with wild Pokémon named; every species' front picture drawn, at its base data's size, Chikorita's 5 x 5 tiles;
both Pokégear maps drawn, New Bark Town on Johto's with its cell naming it, Pallet Town on Kanto's, and every named
place but the special landmark's rooms on a map. Given PokeDaisy's checkout, every species' gender ratio and growth
rate is compared with its `Gen2SpeciesCrystal.kt`. The pictures and maps go to `GBPK_ROMS_OUT` (by default
`ui/tests/out/gbpk-roms/`) as PNGs for looking at: a sheet of every species' picture, normal and shiny, in the box the
game draws it in, and each map.
