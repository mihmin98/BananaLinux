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
