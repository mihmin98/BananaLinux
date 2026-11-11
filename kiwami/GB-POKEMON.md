# The Game Boy Pokémon companion

**Based on PokeDaisy by Lidor Itzhari - https://github.com/lidor30/pokedaisy**

The kiwami fork's companion for Pokémon Red, Blue, Yellow, Gold, Silver and Crystal on the Game Boy and Game Boy
Color: while one of them plays on one panel, the other shows the pages upstream's companion shows for the Game Boy
Advance and DS games (Battle, Party, Team, Here, Map, Guide, Dex, Log). It is the fork's own, not upstream's, and is
off by default. Its investigation and plan are in `.workspace/0028-gb-pokemon/`.

The games are the English retail releases: Red, Blue and Yellow (USA, Europe), Gold and Silver (USA, Europe), Crystal
(USA, USA and Europe rev 1, Australia). A release is told by the whole file's SHA-1.

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
   hack, a bad dump): "Not supported", and nothing is read from the game's memory.

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

## The data files

`/usr/share/banana/companions/gb-pokemon/<game>.ram.json` (`crystal`, `gold`, `silver`; Red, Blue and Yellow from
step 12), JSON with sorted keys:

```json
{ "schema": 1, "kind": "gb-pokemon-ram", "game": "crystal", "gen": 2, "title": "Crystal",
  "header_title": "PM_CRYSTAL",
  "releases": [ { "sha1": "f2f52230...", "name": "USA, Europe (Rev 1)", "code": "BYTE" } ],
  "ram": { "partyCount": { "bank": 1, "addr": "0xDCD7", "src": "pokedaisy pret (wPartyCount)" } },
  "sizes": { "itemsMax": 20, "ballsMax": 12, "keyItemsMax": 25, "speciesCount": 251, "eventFlagBytes": 256 },
  "flags": [], "rom": {} }
```

- `header_title` is the ROM header's title (0x134), the same for a game's releases; `code` is a release's game code
  (0x13F), which only the Game Boy Color games have (Crystal's Australian release has its own, `BYTU`).
- A `ram` address's `bank` is 0 for C000-CFFF and HRAM, 1 for the D000 bank the cores map there, and from 2 up
  another WRAM bank, read at `0x10000 + (bank - 2) * 0x1000 + (addr - 0xD000)` through `READ_CORE_MEMORY`.
- `sizes` are counts: the pockets' capacities, the species, the bytes of event flags (pret's `NUM_EVENTS` / 8).
- `flags` (the flag banks the guides read) and `rom` (the ROM tables' offsets) are filled by later steps.
