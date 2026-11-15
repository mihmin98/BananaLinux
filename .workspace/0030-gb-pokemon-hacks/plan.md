# 0030 Pokémon companion for the Game Boy ROM hacks: plan

Issue: #30 · Branch: `k/0030-gb-pokemon-hacks` · Investigation: [investigation.md](investigation.md) · Status: approved

## Summary

#28's Game Boy companion learns four ROM hacks as games of their own, with the same pages (Battle, Party, Team, Here,
Map, Guide, Dex, Log):

- Crystal Legacy v1.3.1;
- Yellow Legacy v1.0.10;
- PureRed, PureBlue and PureGreen at v2.7.6 and v2.7.7;
- Crystal Clear v2.5.11 and v2.6.0, each in its four builds: Standard Colors and Color Filter, each with and without
  SGB Mode.

They run under Settings' existing rows (Pokémon: Game Boy, Pokémon: Game Boy Color), by the folder they're started
from.

The approach is the investigation's option A. Each hack has a data file of its own in
`kiwami/rootfs/usr/share/banana/companions/gb-pokemon/`, with its addresses from its own symbols:

- Crystal Legacy, Yellow Legacy and PureRGB: the user builds them from their tags, and `pret-extract.py` reads their
  `.sym`, checking each build's SHA-1 against the release's.
- Crystal Clear, which has no source: a new script maps pokecrystal's symbols into it through the code that uses
  them.

Everything of the hacks themselves (names, tables, text, charts) is read from the player's ROM at run time, as #28
does. All the code is in the fork's files. The upstream hooks are #28's, plus three lines for PureRGB's types
(*Refinements*).

The work is 16 steps, one hack after another, each on the device before the next:

1. Steps 1-3 change what every game shares, with no visible change for the retail games: several games to a header
   title, per-game items and types, and the type chart from the ROM.
2. Crystal Legacy works after step 5.
3. Yellow Legacy works after step 7.
4. PureRGB works after step 11.
5. Crystal Clear works after step 14.
6. Step 15 is the documentation and what it all weighs, and step 16 the final check.

### Refinements over the investigation

Found while reading the code and the hacks' sources (2026-11-13), each agreed with the user where it's a choice:

- **PureRGB has six types of its own** (`constants/type_constants.asm` at v2.7.7; the investigation missed them):
  - Typeless ($06: Struggle's);
  - Crystal ($09: Hardened Onix);
  - Bonemerang ($0A: the move, Ground that also hits Flying);
  - Tri ($11: Tri Attack);
  - Floating ($12: Floating Weezing and Magneton);
  - Magma ($13: Volcanic Magmar).

  Upstream's `nds::Type` has 18. *Decided*: they become values of the fork's own past upstream's (32-37), as
  `GEN_I` is a value of upstream's `nds::Gen`. Upstream already copes with a type past `NUM_TYPES`: the coverage loops
  skip it, `typeName` gives `""` and the colour is grey. The fork's chart hook (`model.cpp:89`) runs before
  upstream's `>= NUM_TYPES` guard, so their matchups are exact. Three new hook lines name and colour them on the pages
  and in the advice (step 8): `pages.cpp` (an include, and a line in `typeChip`) and `logic.cpp` (`typeName`). Prism's
  issue can reuse this.
- **The physical/special split is each game's.**
  - Yellow Legacy moved Ghost after `DEF SPECIAL`.
  - Crystal Legacy makes Ghost special and Dark physical in its code (`type_constants.asm`'s comments).
  - PureRGB makes Ghost special when the user's base Special is higher than its Attack, and Tri, Floating and Magma are
    special.
  - Today's rule is `t < FIRE` for every game (`rules.h` `damage`). It becomes the move's category as the data file's
    split gives it (step 2), with PureRGB's Ghost by the attacker's base stats (step 10).
- **The items are each game's, by name.**
  - `items.inc`'s fixed maps (`gen1Item`, `gen2Item`, `gen1Pocket`) become per-game tables in the data files.
    `pret-extract.py` makes them from each game's `constants/item_constants.asm` and `data/items/key_items.asm`, by
    the item's name.
  - The retail games' tables come out the same as today's, which a test checks (step 2).
  - PureRGB put a Hyper Ball where the Town Map was ($05) and made the Safari Ball an ordinary item.
- **The hacks' `roms.sha1` are pret's retail SHA-1s, left over.** For example, Crystal Legacy's `pokecrystal.gbc` line
  is retail Crystal's. So a build's SHA-1 is checked against one written in `gbpk-symbols.txt`'s line for it.
  PokeDaisy's `GameTitles.kt` has it for Crystal Legacy, Yellow Legacy and PureRed/PureBlue v2.7.6; for the others,
  the release patch applied to the user's retail ROM gives it. The header's title and code are read from the built ROM.
- **PureRGB follows the save with three things the game keeps** (oversight 4, *decided*):
  - `wPkmnTypeRemapFlags` (3 bytes, a bit per species of the type NPC's 23, `engine/pokemon/remap_typings.asm` with
    its `OriginalTypings` table in the ROM);
  - `wOptions3` bits 0-3, the four matchups. Bit 0: Ghost on Psychic, 2x or 0x. Bit 1: Ice on Fire, 1x or 0.5x. Bit
    2: Bug on Poison, 2x or 0.5x. Bit 3: Poison on Bug, 2x or 1x. Each "0 or 1" as PureRGB's `ram/wram.asm` says;
  - the event flag `FLAG_SIGNATURE_MOVES_TURNED_OFF`.
- **PureRGB's signature moves are followed** (*decided*). `engine/battle/remap_move_data.asm`'s `RemappableMoves` gives
  about 20 moves another power or accuracy when a given species uses them. It's read from the ROM by its symbol. The
  eight whose effect depends on the battle (Sing, DoubleSlap, Explosion, Selfdestruct, Firewall, Toxic, Skull Bash,
  Filthy Slam, through `ModifierFuncs`' seven rules) stay at their listed power, and `kiwami/GB-POKEMON.md` says so.
- **PureRGB's forms have their own types and stats** (*decided*). The Floating forms, the spirits and Volcanic Magmar
  are internal species that carry a base species' Pokédex number. They go after the 151 as upstream's forms do
  (`Species::formsAt`/`formCount`, `Mon::form`, as `ui/src/gba/gen3rom.cpp` lays them out). The Dex shows the base
  species, and the Battle, Party and Team pages show the form's own types and stats.
- **PureRGB's guide takes each boss's levels from the ROM.** The investigation took Red's and Blue's converted guide if
  its flags and bosses check out. They do, but for a few renamed flags: of Red's 510 event constants, 496 are in
  PureRGB's v2.7.6, 494 at the same number. Red's guide holds retail levels, though (`ace_level`, `level_cap`). So the
  conversion writes each boss's trainer ids, and the host takes the levels from the ROM's parties (step 11). PureGreen
  takes Red's milestones: the bosses and their flags are the same in the three builds.
- **Crystal Legacy's gyms scale with the badges** (PokeDaisy `GuideCrystal.kt:171-181`: the gyms can be taken in any
  order). The guide shows each gym's team for the badges the story order has at that point, and its note says the
  levels follow the badge count (step 5).
- **A detected title's log names every game that has it.** `Game::open` logs `gb companion: Crystal (or Crystal
  Legacy, Crystal Clear)` until the SHA-1 picks one. `gbpk-ui.sh`'s two assertions on that line change with it
  (step 1).
- **Crystal Clear's SGB Mode builds are read too** (the user's request, 2026-11-13, after checking the ROMs). Each
  version has four builds, not two, and the four SGB Mode ones are in no list of PokeDaisy's. Step 12 maps all eight,
  and fails if a version's builds disagree on a value the companion reads.
- **The ROM tests look one folder down** (the user's choice, 2026-11-13). The hacks are in `~/gb-companion/roms/romhacks/`,
  and `gbpk-roms.sh` read only the folder itself. Step 4 makes it read subfolders one level deep.
- **PokeDaisy stays at `135ac809`.** The user's checkout is at 1.5.1 (`0040e61f`), whose changes are German Gold,
  Silver and Crystal and nothing for these hacks. The extraction runs on a worktree at the pin (*Before the steps*).

## Acceptance criteria

- **A1**: given a row On and Second screen on, when one of the sixteen releases is started from that system (Crystal
  Legacy v1.3.1; Yellow Legacy v1.0.10; PureRed, PureBlue and PureGreen at v2.7.6 and v2.7.7; Crystal Clear v2.5.11
  and v2.6.0, Standard Colors and Color Filter, each with and without SGB Mode), as `.gb`, `.gbc`, `.zip` or `.7z`, then the log says
  `gb companion: <title> (<release>) read in N ms` with the hack's own title, and the pages show on the game panel.
- **A2**: given a file whose header title one of the hacks shares (`PM_CRYSTAL`, `POKEMON YELLOW`, `POKEMON RED`,
  `POKEMON BLUE`, `POKEMON GREEN`, Crystal Clear's), with a SHA-1 that's none of the listed releases (another version
  of a hack, a patch on top of one, a bad dump), then the panel shows "Not supported" and nothing is read from the
  game's memory.
- **A3**: the retail games and Fast Red and Fast Blue are read as on `kiwami`: the same release picked, the same tables
  (items, types, chart), the same pages. With both rows Off, nothing of the companion runs. `make test`'s renders and
  `gbpk-ui.sh`'s retail shots are unchanged.
- **A4**: given a running Crystal Legacy, then `Live` holds what its four fixtures' READMEs say. Its bag holds 84 items,
  its TMs are items from 0xC1, and its Pokédex pages come through their far pointers.
- **A5**: given a running Yellow Legacy, then `Live` holds what its four fixtures' READMEs say. Its bag holds 59, and
  its Pikachu's friendship shows as Yellow's does.
- **A6**: given a running PureRed, PureBlue or PureGreen, then `Live` holds what the eight PureRGB fixtures' READMEs say
  (PureGreen and v2.7.7 by their symbols and on the device). Its bag holds 30 and the Hyper Ball is a ball. A form is
  numbered and named as its base species in the Dex, and has its own stats on every page and its own types on the
  Battle page; the Party and Team pages and the team advice show its base species' types, as upstream shows its own
  forms' (*Deviations*, 2026-11-15).
- **A7**: given a running Crystal Clear (either version), then `Live` holds what its six fixtures' READMEs say. Its map
  ids and caught flags are read where it moved them.
- **A8**: given the player's ROM of a hack, then its species, base stats, types, moves, learnsets, evolutions, TM/HM
  compatibility, items, wild encounters, trainers, map names, Pokédex text, front pictures and maps are read from it
  and checked. Crystal Clear's packed text decodes, and PureRGB's text reads in its own character set. Nothing of the
  hacks is in the repository or the image.
- **A9**: each game's type chart is its ROM's (`TypeEffects` in Gen I, `TypeMatchups` in Gen II). The retail games'
  give exactly today's: Gen I's five differences from upstream's, Gen II upstream's own. Each game's physical/special
  split is its own: Yellow Legacy's Ghost is special; Crystal Legacy's Ghost is special and Dark physical; PureRGB's
  Ghost goes by the attacker's base Attack and Special.
- **A10**: given a PureRGB Pokémon or move of one of its six types, then the chips show its name and colour, the damage
  and the Battle page's matchups use PureRGB's chart for it (Water on Crystal 0.5x, Ground on Floating 0x, Bonemerang on
  Flying 1x), and the advice names it.
- **A11**: given a PureRGB save, then:
  - a species switched back at the type NPC has its original types on every page;
  - each of the four matchup options changes the chart as the game does;
  - a signature move used by its species has the signature power, unless the save turned signature moves off;
  - the Hyper Ball's catch chance is PureRGB's rule;
  - the moves whose power depends on the battle use their listed power, and `kiwami/GB-POKEMON.md` lists them.
- **A12**: Crystal Legacy and Yellow Legacy load PokeDaisy's guide for them, no entry skipped. PureRed, PureBlue and
  PureGreen load Red's and Blue's milestones with PureRGB's flags and the ROM's levels. Crystal Clear gets a guide built
  from its ROM's leaders, Elite Four and Champion, by level. On each, the Guide page's next milestone follows the save.
- **A13**: the data files are the scripts' output: `pret-extract.py` for the three built hacks (each build's SHA-1
  checked against its line), the new port script for Crystal Clear (run on the user's ROMs), and
  `pokedaisy-extract.py` for the guides and the cross-checks. Each script's `--check` passes without the checkouts or
  ROMs, by `SOURCES`' hashes. CI's name scan finds no game content in the new files. `kiwami/DEPENDENCIES.md` lists
  the three repositories at their tags and the rgbds versions used.
- **A14**: a hack started from either system follows that system's row (PureRGB as `.gb` from Game Boy, and as `.gb` or
  `.gbc` from Game Boy Color).
- **A15**: the pages lay out at 640x480 and 1024x768 for a hack's capture (`gbpk-ui.sh`), and on the device the pages
  follow each hack through BananaFront and RetroArch, with gambatte and SameBoy.
- **A16**: nothing runs for a game whose header title no data file has, or with the rows Off. The image
  grows by the hacks' data and guide files, and banana-ui by the new layouts, both measured in step 15. `kiwami/GB-POKEMON.md`
  says which hacks are read, where their data comes from, how a new version is added, and what isn't followed.

## Risks

- **Conflicts with upstream.** Three new marked lines (step 8): `pages.cpp`'s include and its line in `typeChip`, and
  `logic.cpp`'s `typeName`. `logic.cpp`'s is an upstream line changed, not added, because `typeName` is a one-line
  function. `pages.cpp` changes daily upstream, so each is one line beside a named function, and putting it back is the
  whole conflict. #28's 31 lines are unchanged. `docs/FORK.md`'s merge checks list the three (step 8).
- **The fork's types reach code that indexes by type.** Upstream guards every array indexed by a `Type` I found
  (`pages.cpp:82, 146, 147, 664, 898`, `logic.cpp:414, 575`, `model.cpp:91`, `rom.cpp:123`, `ram.cpp:167`). Step 8
  runs `gbpk-units` once by hand with `-fsanitize=address,undefined` over a PureRGB capture's advice and pages, and
  `gbpk-ui.sh` renders every page of a PureRGB capture. A new unguarded index upstream would read past an 18-entry
  array: the sanitizer run is repeated in the final check.
- **Retail behaviour changes under steps 1-3** (items, types and the chart from data files and ROMs instead of the
  code). Guards:
  - `gbpk-units` compares the regenerated retail item and type tables with today's `items.inc` values, kept in the
    test;
  - the ROM tests compare each retail ROM's chart with today's cells;
  - `make test` and `gbpk-ui.sh`'s retail shots stay bit-identical.
- **A build that isn't the release.** A wrong rgbds, or a tag that isn't the release, gives another SHA-1, and
  `pret-extract.py` stops. Each hack's rgbds version is recorded (*Before the steps*, `kiwami/DEPENDENCIES.md`).
- **Crystal Clear's mapping is the largest unknown.** A symbol whose code Crystal Clear rewrote has no match. The script
  reports it and fails; the step then finds it by its contents (`--data`) or stops and says which (*Deviations*).
  Each table found is checked against the ROM by `rom2.cpp`'s checks and the ROM tests.
- **PureRGB is a Color cart that switches D000 to WRAM bank 2 for its palettes.** #28 found that the cores' memory map
  gives D000 as bank 1 always, so the reads stay right. If gambatte or SameBoy gives the bank in use instead, the
  reads return palette bytes while the game is in a palette fade. Guard: manual test M6 on both cores, with PureRGB's
  COLORS option on, through a battle's start (the fade).
- **PureRGB's rules not followed.** The eight battle-state moves, critical hits and anything else `rules.h` doesn't
  model give advice off by that much. `kiwami/GB-POKEMON.md` lists them (step 10), as #28's notes do for the retail
  games.
- **Fixtures only where PokeDaisy has them.** PureGreen and v2.7.7 have no captures, and their RAM is checked by their
  symbols and on the device (M3). Crystal Clear v2.5.11 has two.
- **The hacks move on.** A newer release shows "Not supported" until its lines are added (oversight 11, accepted).
  `kiwami/GB-POKEMON.md` says how (step 15).
- **Performance and footprint.**
  - Idle: unchanged. Nothing runs with the rows Off, or for a title no data file has. A shared title now loads two or
    three small data files at launch instead of one.
  - Reads: one set every 250 ms as #28, plus 4 bytes for PureRGB (the remap flags and `wOptions3`).
  - The type chart: one table lookup per pair instead of #28's comparisons, read once from the ROM.
  - PureRGB's switched types: the ROM's data is copied with the species' types changed, once at the start and again
    only when the player switches a species at the NPC (the Session restarts on the copy; the journey log is on the
    card).
  - Rootfs: six data files and five guide files, expected under 150 KB before squashfs. Step 15 measures it.
  - Binary: banana-ui grows by the new layouts, the port script's tables and the packed text, expected under 100 KB.
    Step 15 measures it on x86-64.
  - Menu frames: untouched. The code runs in the game panel's loop only.
- **Repository size.** The 22 captures add 1.1 MB.
- **One long branch.** Each merge of `main` into `kiwami` during the work is merged into this branch too, and the hook
  list checked again (`git grep -n 'kiwami:'`).

## Before the steps

What has to be on the machine before the step that needs it. None of it is committed, and none of it is needed by
CI. On another machine, set it up from this list. The paths are the ones on this machine: the scripts take their
folders as arguments, so another layout works if the same folders are passed. The table says what each step needs.

| Step | Needs |
|---|---|
| 1 | the host build and the retail ROMs |
| 2-3 | + PokeDaisy at the pin and pret's four (the retail data files are regenerated) |
| 4-5 | + rgbds 0.5.2, Crystal Legacy built, its ROM |
| 6-7 | + rgbds 0.6.1, Yellow Legacy built, its ROM |
| 8 | the host build, with GCC's sanitizers (the `-fsanitize=address,undefined` run) |
| 9-11 | + rgbds 1.0.3, PureRGB built twice, its six ROMs |
| 12-14 | + the eight Crystal Clear ROMs and Crystal rev 1 (no build: no source) |
| 15-16 | everything above, Buildroot's host `mksquashfs` (the weight), and an image for the device |

Everything below was set up and checked on this machine on 2026-11-14: each section ends with what was found.

### 1. The host build and its tests

- The fork's repository on the feature's branch: `git clone <the fork's URL>` (`git remote get-url origin` on this
  machine), then `git switch k/0030-gb-pokemon-hacks`.
- Packages, on Arch/CachyOS: `sudo pacman -S --needed base-devel git python pkgconf freetype2 libdrm zlib alsa-lib
  imagemagick curl libssh2 openssl`. `make ui`, `make test` and `kiwami/tests/ci.sh` need them: g++, Python 3 for the
  scripts (standard library only), `pkg-config` with freetype2, libdrm and zlib for banana-ui, alsa-lib for
  `make -C ui`, ImageMagick's `convert` for the renders' checks.
- Buildroot's checkout, which `kiwami/tests/ci.sh .buildroot` reads (the defconfig checks): `make .buildroot/Makefile`
  clones it (Buildroot 2026.02.3, shallow). No image build is needed for it.
- GCC's sanitizer runtimes (libasan, libubsan; Arch's `gcc` has them) for step 8's and the final check's
  `-fsanitize=address,undefined` run of `gbpk-units`.
- `size` (binutils, in `base-devel`) for banana-ui's growth in step 15.
- `mksquashfs` for the data files' size after squashfs in step 15: Buildroot's own,
  `output/rgds-plus/host/bin/mksquashfs` (4.6.1), the one the image is made with, so no system package is needed.
  It's there once an image has been built (`make rgds-plus`, §7).
- Optional: `bear`, for clangd (`CLAUDE.md`, *Commands*); `gh`, for reading the issue.

Checked 2026-11-14: all of it present; the sanitizers compile and run; `.buildroot/` cloned; `output/rgds-plus/` built.

### 2. PokeDaisy at the pin (steps 2 on)

- `git clone https://github.com/lidor30/pokedaisy /home/mih/Documents/Projects/pokedaisy` (this machine has the SSH
  remote `git@github.com:lidor30/pokedaisy.git`). A checkout made earlier may not have the pin yet (this one was at
  `v1.3.1-1` on 2026-11-14): `git -C /home/mih/Documents/Projects/pokedaisy fetch origin` first.
- A worktree at the pin, so the checkout itself can be at any version: `git -C /home/mih/Documents/Projects/pokedaisy
  worktree add ../pokedaisy-135ac809 135ac809`. `pokedaisy-extract.py` is pointed at
  `/home/mih/Documents/Projects/pokedaisy-135ac809`, and refuses a checkout whose `HEAD` isn't `135ac809`.

Checked 2026-11-14: the worktree is at `135ac809` (the checkout at v1.5.1). It has the files the steps read
(`GameTitles.kt`, `Gen1Reader.kt`, `Gen2Reader.kt`, `Gen1Dex.kt`, `Gen2Dex.kt`, `GuideCrystal.kt`,
`GuideCrystalLegacyGen.kt`, `GuideGen1.kt`, `GuideGen1Gen.kt`, `scripts/gen_gen2_tables.py`, `scripts/port_gen1.py`)
and the 22 captures of these hacks in `app/src/test/resources/fixtures/` (crystal_legacy 4, yellow_legacy 4,
purered 6, pureblue 2, crystal_clear 6; its `emerald_legacy*` are a GBA hack's, not this feature's).

### 3. pret's four disassemblies (steps 2 on)

`pret-extract.py` reads them for the retail games' values (#28), which steps 2 and 3 add to. They need no build:
each `.sym` comes from its `symbols` branch. Each is at the commit `kiwami/DEPENDENCIES.md` pins (`SOURCES`' `[pret]`
section has the full hashes): pokecrystal `3c813b0b` (symbols `3b13ca4f`), pokegold `f2cde36f` (`4b57c3aa`), pokered
`0dbf95cd` (`bd77855a`), pokeyellow `b452f2ab` (`2dcc4b8f`). `pret-extract.py --check` against them must pass before
step 2. Step 12 also reads `pokecrystal11.sym` from pokecrystal's `symbols` branch. In
`/home/mih/Documents/Projects/pret/`:

```sh
for r in pokecrystal:3c813b0b pokegold:f2cde36f pokered:0dbf95cd pokeyellow:b452f2ab; do
    repo=${r%%:*} commit=${r#*:}
    git clone https://github.com/pret/$repo && git -C $repo fetch origin symbols:symbols && git -C $repo checkout $commit
done
```

A `symbols` branch that has moved past its pin is put back with `git -C <repo> branch -f symbols <commit>`.

Checked 2026-11-14: the four at their commits, each `symbols` branch at its pin, `pokecrystal11.sym` on pokecrystal's.

### 4. rgbds, the Game Boy assembler (steps 4, 6 and 9)

The toolchain that turns the hacks' sources into ROMs, and writes the `.sym` files that name their addresses. It is a
tool for the extraction only, never in the image. Each hack's tag builds byte for byte only with the rgbds of its
time. A newer one stops on syntax it removed, and an older one is refused by the hack's `rgbdscheck.asm`. Checked
2026-11-13: Arch's `rgbds` package (1.0.4) fails on Crystal Legacy V1.3.1 at its first file ("Undefined macro
`MAJOR`").

- Three versions are built from source and not installed: one clone, a worktree for each version beside it, the
  programs (`rgbasm`, `rgblink`, `rgbfix`, `rgbgfx`) left in the worktree's root:

  ```sh
  sudo pacman -S --needed base-devel bison libpng pkgconf git
  R=~/Documents/Projects/rgbds
  git clone https://github.com/gbdev/rgbds "$R"
  for v in 0.5.2 0.6.1 1.0.3; do
      git -C "$R" worktree add "$R-$v" v$v &&
      make -C "$R-$v" -j"$(nproc)"
  done
  for v in 0.5.2 0.6.1 1.0.3; do "$R-$v/rgbasm" -V; done
  ```

  - 0.5.2 for Crystal Legacy (PokeDaisy: "`make pokecrystal.gbc` with rgbds 0.5.2 at tag V1.3.1").
  - 0.6.1 for Yellow Legacy (PokeDaisy: "rgbds 0.6.1 at tag V1.0.10").
  - 1.0.3 for PureRGB (PokeDaisy built its releases with it, `fixtures/purered/README.txt`). If its builds don't give
    the releases' SHA-1s, step 9 stops and the version that does is a *Deviation*.
  - An old version that GCC 15 won't compile (its default is C23): `make -C "$R-$v" clean`, then
    `make -C "$R-$v" -j"$(nproc)" CFLAGS="-O2 -std=gnu11"`.
- A build names its rgbds by `make RGBDS=$HOME/Documents/Projects/rgbds-<v>/`. pret's Makefiles call
  `$(RGBDS)rgbasm`, so the value ends in `/`. Without it, an `rgbasm` on the `PATH` is used, if there is one.
- A build with the wrong rgbds leaves files it generated (graphics, tools) that a later build would take as up to
  date: `make clean` before building again.

Checked 2026-11-14: the three built with the system's GCC as they are (no `CFLAGS` needed; 0.6.1 gives one
`-Wunused-but-set-variable` warning), each worktree with its four programs, `rgbasm -V` giving v0.5.2, v0.6.1 and
v1.0.3.

### 5. The hacks' sources, built at their tags (steps 4, 6 and 9)

Beside pret's, in `/home/mih/Documents/Projects/pret/`. Each build's ROM must have its release's SHA-1, and the
`.sym` beside it is what `pret-extract.py` reads:

```sh
cd /home/mih/Documents/Projects/pret
git clone https://github.com/cRz-Shadows/Pokemon_Crystal_Legacy && git -C Pokemon_Crystal_Legacy checkout V1.3.1
make -C Pokemon_Crystal_Legacy RGBDS=$HOME/Documents/Projects/rgbds-0.5.2/ pokecrystal.gbc
sha1sum Pokemon_Crystal_Legacy/pokecrystal.gbc      # 74a6727e4406c35bc2d4f81347a5c5fdb8970663

git clone https://github.com/cRz-Shadows/Pokemon_Yellow_Legacy && git -C Pokemon_Yellow_Legacy checkout V1.0.10
make -C Pokemon_Yellow_Legacy RGBDS=$HOME/Documents/Projects/rgbds-0.6.1/ pokeyellow.gbc
sha1sum Pokemon_Yellow_Legacy/pokeyellow.gbc        # 2ac87de42afc70ef6016805acb3b206644417f97

for v in 2.7.6 2.7.7; do
    git clone https://github.com/Vortyne/pureRGB pureRGB-$v && git -C pureRGB-$v checkout v$v
    make -C pureRGB-$v RGBDS=$HOME/Documents/Projects/rgbds-1.0.3/ red blue green
done
sha1sum pureRGB-2.7.6/poke{red,blue,green}.gbc pureRGB-2.7.7/poke{red,blue,green}.gbc
```

The tags' commits: Crystal Legacy `V1.3.1` `6394641e`, Yellow Legacy `V1.0.10` `3a5358e8`, PureRGB `v2.7.6`
`9c6d40f6` and `v2.7.7` `b5a155eb`. A build's SHA-1 that isn't its ROM's below stops the step (the wrong rgbds, or a
tag that isn't the release).

Checked 2026-11-14: the four checkouts at those tags and commits, no tracked file changed. All eight builds give their
release's SHA-1, each equal to one ROM in `romhacks/` (§6), with its `.sym` beside it. So rgbds 1.0.3 is PureRGB's,
as step 9 records.

### 6. The ROMs (steps 1 on)

`GB_ROMS`, by default `~/gb-companion/roms`. The ROM tests find a file by its SHA-1, whatever its name. They read
`.gb` and `.gbc` files, unpacked, in the folder and, from step 4, in its subfolders one level down. Without the folder
the ROM tests are skipped and pass, as in CI.

- **The retail ROMs**, in the folder itself: the six required and the optional lines of
  `kiwami/tests/gbpk/roms.sha1` (Red, Blue, Yellow, Gold, Silver, Crystal rev 1; Crystal USA and Australia, Fast Red,
  Fast Blue). The hacks' patches need Red, Blue, Yellow, Crystal USA (rev 0) and Crystal rev 1 among them.
- **The hacks**, in `~/gb-companion/roms/romhacks/`. Each is its release's BPS patch applied to a retail ROM, with any
  BPS patcher (Flips: `flips --apply <patch>.bps <retail ROM> <hack ROM>`). The patches come from each hack's release
  page. The user's folder keeps them beside the ROMs, and the tests pass over the `.bps` files.
  - From Crystal (USA, Europe) (Rev 1): Crystal Legacy v1.3.1 `74a6727e`.
  - From Yellow (USA, Europe): Yellow Legacy v1.0.10 `2ac87de4`.
  - From Red: PureRed `2e94d09c` (v2.7.6), `60d445d5` (v2.7.7).
  - From Blue: PureBlue `d419fe24`, `b96ee563`; PureGreen `fe4c63a6`, `258ec7fe` (v2.7.6, v2.7.7).
  - From Crystal (USA), rev 0: Crystal Clear v2.5.11 `a1c4da6b`, Color Filter `6fc677bf`, SGB Mode `8be3dea8`, Color
    Filter SGB Mode `913c49bd`; v2.6.0 `84d5c492`, `534876e4`, `059632e8`, `88c943e6`.

  Checked 2026-11-13: each file is what its patch makes of the retail ROM it names, and each SHA-1 PokeDaisy lists
  matches. `sha1sum ~/gb-companion/roms/romhacks/*.gb*` against this list checks a copy. Every Crystal
  Clear build has Crystal's header title and code (`PM_CRYSTAL`, `BYTE`), so `PM_CRYSTAL` names three games (step 1).
  Each file gets its line in `roms.sha1` in step 4, 6, 9 or 13.
- `GBA_ROMS` and `NDS_ROMS` (`~/gba-companion/roms`, `~/nds-companion/roms`) are #28's optional lists: not needed here.

Checked 2026-11-14: the six required retail ROMs and the optional Crystal USA, Fast Red and Fast Blue match their
`roms.sha1` lines (Crystal Australia, optional, isn't there). The sixteen hacks in `romhacks/` match the list above, and
no other ROM is there. They are already patched, so no BPS patcher is needed on this machine.

### 7. The device (the manual tests)

- The RG DS Plus with a card of the fork's image built from the branch at the step a manual test names:
  `make rgds-plus` (Buildroot downloads and builds everything the first time: an hour or more and several GB), then the
  `.img.gz` in `output/rgds-plus/images/` written to the card. A card that already runs the fork can take the update
  package instead (`banana-rgds-plus-<version>.update.tar`, `docs/ARCHITECTURE.md`, *A/B updates*).
- The hacks' ROMs on the card, in its Game Boy Color folder (PureRed also in its Game Boy folder, for M3). Settings >
  Other systems > Second screen: the Pokémon rows On.
- SSH (Settings > Network > SSH) or a card reader, for `/data/banana/logs/`.

## Steps

Each step is one commit, and leaves the tree building and `make test` passing. Every step also changes
`.workspace/0030-gb-pokemon-hacks/plan.md` (its Progress row), not repeated under *Files*.

Common to the steps (as #28's):

- **Folder**: `ui/src/kiwami/gbpk/`, namespace `banana::kiwami::gbpk`. Every file starts with the GPL-3.0 header and
  `Based on PokeDaisy by Lidor Itzhari - https://github.com/lidor30/pokedaisy`. The new script
  `kiwami/tools/gbpk-port.py` carries the same.
- **Sources**: each rule and layout follows the hack's own source (its tag), named in the code's comment. Crystal Clear
  has none, so its layouts follow pokecrystal's, checked on the ROM. Only addresses, ids and SHA-1s are taken from
  the hacks' repositories (none publishes a licence), as #28 does with pret.
- **Logs**: `banana-ui: gb companion: ...` on stderr (on the device: `/data/banana/logs/banana-ui.log`).
- **Checks at the end of every step that touches C++**: `make test`, `make -C ui`, `kiwami/tests/ci.sh .buildroot`,
  run once at the step's end (the user's standing preference for short runs). With `GB_ROMS` holding the hacks the
  step adds, `kiwami/tests/gbpk-roms.sh` too.
- **Stale test binaries**: a step that changes `gbpk.h`, `rules.h`, `guide-games.inc` or the pages' `.inc` files
  removes upstream's test binaries that include them
  (`rm -f ui/tests/nds-logic-test ui/tests/nds-pages-test ui/tests/gba-pages-test ui/tests/guide-test`) before
  `make test`.
- **The keys** of the hacks' data files and guides: `crystal-legacy`, `yellow-legacy`, `purered`, `pureblue`,
  `puregreen`, `crystal-clear`. Their titles: Crystal Legacy, Yellow Legacy, PureRed, PureBlue, PureGreen, Crystal
  Clear. A fixture's folder names its game by its name with `_` as `-`, the longest key it starts with
  (`crystal_clear_v2_5_11_battle` is `crystal-clear`, `crystal_battle` is `crystal`).

### 1. Several games to one header title, the SHA-1 picks one

- **Files**:
  - `ui/src/kiwami/gbpk/gbpk.h`, `games.h`, `games.cpp`, `game.h`, `game.cpp`, `host.h`, `host.cpp`, `reader.cpp`,
    `pages-impl.inc` (changed)
  - `kiwami/tests/gbpk-units.cpp`, `kiwami/tests/gbpk-ui.sh`, `kiwami/tests/gbpk-roms.cpp` (changed)
  - `kiwami/GB-POKEMON.md` (changed: *Which game a ROM is*)
- **Change**:
  - **`gbpk.h`**:
    - the hacks' ids: `CRYSTAL_LEGACY = (nds::GameId)106, YELLOW_LEGACY = 107, PURERED = 108, PUREBLUE = 109,
      PUREGREEN = 110, CRYSTAL_CLEAR = 111`;
    - `gbGame()` covers 100-111;
    - `inline nds::GameId baseGame(nds::GameId id)`: the retail game whose engine a hack is (106 and 111 Crystal, 107
      Yellow, 108 and 110 Red, 109 Blue; a retail id itself).
  - **The per-game branches go by `baseGame`**:
    - `reader.cpp:395` (`caughtData`, Crystal's);
    - `pages-impl.inc:91` (Yellow's Pikachu's friendship).

    `host.cpp:26` (Gold's and Silver's guide from the ROM) stays by their ids: it becomes the data files' `"guide":
    "rom"` in step 2, which regenerates them (*Deviations*, 2026-11-14).
  - **`games.cpp`**:
    - `GAMES` gets the six keys with their ids. `loadData` takes `dexSize` from the file's `"dex_size"` when given,
      else 151 or 251 as today.
    - `detect` becomes `Match detect(const uint8_t* header, size_t n, std::vector<std::string>& keys)`: every key whose
      `header_title` the header has, in `GAMES`' order (the retail game first).
    - `confirm` stays per game. A new `int confirmAny(const std::vector<uint8_t>& rom, std::vector<GameData>& ds)`
      gives the index of the one whose release has the SHA-1 (its `release`, `romHash` and `code` set), or -1 with
      each's `romHash` set.
  - **`Game::open`** loads every candidate's data file (a candidate whose file doesn't load is logged and left out) and
    logs `gb companion: <first title>` plus `(or <the others' titles>)` when there are several.
  - **`Host`** takes `std::vector<GameData>`. Its loading thread reads the ROM once and runs `confirmAny`. The picked
    game's data is the host's from then on (`title()` is its title).
    - "Not supported" when none has the SHA-1, logged as `gb companion: <titles joined by " or ">: not a release the
      companion knows (sha1 ...)`.
    - The stand-in path (tests) passes one game, as today.
  - **`Game::script("gbpanel")`** maps a fixture folder to its key by the longest key it starts with (*Common*). With
    a ROM, it opens every candidate as `Game::open` does.
  - **`gbpk-roms.cpp`** picks the game as the host does: `detect`'s candidates loaded, `confirmAny` among them.
- **Tests**:
  - `gbpk-units`:
    - `testDetect`'s checks (`:59-91`) move to the new signature, its `NONE` cases kept with `keys` empty: `POKEMON
      REDX`, `TETRIS`, `PM_CRYSTALX`, a header cut short (A16: nothing for a title no data file has);
    - two made-up data files in a temporary `KIWAMI_GBPK_DIR` share a title. `detect` gives both, in `GAMES`' order;
      `confirmAny` picks each by its release's SHA-1, and gives -1 with both `romHash` set for a third SHA-1;
    - `baseGame` for the twelve ids (100-111);
    - the fixture name mapping for `crystal_clear_v2_5_11_battle`, `crystal_battle`, `purered_battle_menu`,
      `yellow_legacy` and `red`;
    - a data file with `"dex_size": 151` and one without.
  - `gbpk-ui.sh`: the two assertions on the made-up Crystal ROM's lines stay, as the made-up ROM's title has only
    Crystal's data file in this step (`companion Crystal`, `gb companion: Crystal: not a release ...`). A new run opens
    the made-up ROM with a second made-up data file of Crystal's title in a temporary `KIWAMI_GBPK_DIR`, and asserts
    `gb companion: Crystal \(or .*\)` and the joined "Not supported" line.
  - `make test`, `make -C ui`, `kiwami/tests/ci.sh .buildroot`.
- **Commit**: `GB Pokémon hacks: several games to one header title, the SHA-1 picks one`

### 2. Each game's items, types and physical/special split from its data file

- **Files**:
  - `kiwami/tools/pret-extract.py`, `kiwami/tools/gbpk-symbols.txt`, `kiwami/tools/gbpk-extra.json`,
    `kiwami/tools/pokedaisy-extract.py` (changed)
  - `kiwami/rootfs/usr/share/banana/companions/gb-pokemon/{red,blue,yellow,gold,silver,crystal}.ram.json`, `SOURCES`
    (regenerated)
  - `ui/src/kiwami/gbpk/games.h`, `games.cpp`, `rom.h`, `rom.cpp`, `rom1.cpp`, `rom2.cpp`, `reader.cpp`, `rules.h`,
    `host.cpp` (changed)
  - `ui/src/kiwami/gbpk/items.inc` (removed: its content is the data files' now)
  - `kiwami/tests/gbpk-units.cpp`, `kiwami/tests/gbpk-roms.cpp` (changed)
  - `kiwami/GB-POKEMON.md` (changed: *The data files*)
- **Change**:
  - **`gbpk-symbols.txt`** gets four line kinds, read by `pret-extract.py`:
    - `item <NAME> <gen4> [ball]` (global): an item upstream's logic knows by its Gen IV number, by the constant's name
      (today's `items.inc` rows, both generations: `item POKE_BALL 4 ball`, `item MOON_STONE 81`, ...);
    - `<games> items <file>`: the game's `item_constants.asm`. Each `item` line's name found there gives
      `[id, gen4]` in the data file's `items` (a name the game hasn't is skipped). The balls by the `ball` mark go in
      `balls`, and Gen I's `tm01`/`hm01` stay in `sizes`;
    - `<games> keyitems <file>`: Gen I's `data/items/key_items.asm` (`KeyItemFlags`: a `dbit TRUE|FALSE ; NAME` line
      an item from 1). The ids of the `TRUE` ones go in `key_items`, each comment's name checked against the item
      constant with that id;
    - `<games> types <file>`: the game's `constants/type_constants.asm`.
      - Each type constant goes in `types` as `{ "<id>": "<name>" }`. The name is the fork's for the type: `normal` ..
        `dark` in `nds::Type`'s order, and PureRGB's `typeless`, `crystal`, `bonemerang`, `tri`, `floating`, `magma`.
      - `BIRD` and `CURSE_TYPE` are left out. A constant it doesn't know fails the run.
      - `special` lists the ids at or past the file's `SPECIAL`.
    - `<games> split <TYPE> physical|special|stats`: a hand-written exception with its source in the comment. It moves
      the type's id in or out of `special`, or into `split_stats` (PureRGB's Ghost).
  - **The retail games' lines** give exactly `items.inc`'s tables (the generator's output is compared with the old
    file's values in the test below), and `gold`, `silver` and `crystal`'s `types` give `gen2Type`'s map.
  - **`pokedaisy-extract.py`** carries the four fields into the data files (it already merges `gbpk-extra.json`). The
    six retail files are regenerated: they gain `items`, `balls`, `types`, `special`, and Gen I's `key_items`.
  - **`games.cpp` `loadData`** reads them into `GameData`:
    - `std::map<uint8_t, uint16_t> items`;
    - `std::set<uint8_t> balls, keyItems`;
    - `std::array<nds::Type, 256> types` (`NO_TYPE` where absent);
    - `std::bitset<256> special, splitStats`.

    It refuses a file without `types` or `items` (the reason in `why`), and an unknown type name.
  - **The guide from the ROM** (moved here from step 1, *Deviations*, 2026-11-14): `pokedaisy-extract.py` writes
    `"guide": "rom"` in Gold's and Silver's data files, `loadData` reads it into `GameData::romGuide`, and `host.cpp`'s
    `guideFor` builds the guide from the ROM by it instead of by the two ids, so step 14 can give it to Crystal Clear.
    A unit test checks Gold and Silver have it and Crystal hasn't.
  - **`rom.cpp`**: `gen1Item`, `gen2Item`, `gen1Pocket` and `gen2Type` take the `GameData` (`gen1Item(d, id)`, ...)
    and read its tables instead of `items.inc`'s. Their callers pass it:
    - `gen1Item`: `rom1.cpp:123` (the `tm01`/`hm01` check), `:205, 271, 424`, `reader.cpp:568`;
    - `gen1Pocket`: `rom1.cpp:212`, `reader.cpp:568`;
    - `gen2Item`: `rom2.cpp:83, 182, 268, 269, 468`, `reader.cpp:25`;
    - `gen2Type`: `rom1.cpp:189, 238, 239`, `rom2.cpp:161, 214, 215`, `reader.cpp:26`.

    `gen1Pocket` uses `balls` and `key_items`.
  - **The move's category** (`rom1.cpp:191`, `rom2.cpp`'s move loop): `PHYSICAL` or `SPECIAL` by `special` over the
    move's raw type id (today `< 20`), `STATUS` without power as today.
  - **`rules.h` `damage`**: `phys` becomes `mv.cat == nds::MoveCategory::PHYSICAL` instead of `t < FIRE` (the same
    for the retail games, whose categories the ROM reader now sets from the same split).
- **Tests**:
  - `gbpk-units`:
    - for the six retail data files, `gen1Item`/`gen2Item`/`gen1Pocket` over ids 0-255 equal today's tables, which the
      test keeps as literal arrays (`items.inc`'s rows, `GEN1_KEY_ITEMS`, the TM/HM ranges);
    - `gen2Type` over 0-255 equals today's for Gen II, and for Gen I but ids 9 and 0x1B, which have no type there
      (*Deviations*, 2026-11-14);
    - a made-up data file with `split` Ghost special gives a Ghost move `SPECIAL`, and `damage` uses the Special stats
      for it;
    - a data file with an unknown type name doesn't load.
  - `python3 -I kiwami/tools/pret-extract.py --check` and `pokedaisy-extract.py --check` (in `ci.sh`), and both by hand
    against the checkouts.
  - `make test`, `make -C ui`, `kiwami/tests/ci.sh .buildroot`, `kiwami/tests/gbpk-roms.sh` (retail ROMs read as
    before).
- **Commit**: `GB Pokémon hacks: each game's items, types and physical/special split from its data file`

### 3. Each game's type chart from its ROM

- **Files**:
  - `ui/src/kiwami/gbpk/rom.h`, `rom1.cpp`, `rom2.cpp`, `rules.h`, `host.cpp`, `game.cpp` (changed)
  - `kiwami/tools/gbpk-symbols.txt`, `gbpk-extra.json`; the six retail `*.ram.json`, `SOURCES` (regenerated: `rom.typeChart`)
  - `kiwami/tests/gbpk-units.cpp`, `kiwami/tests/gbpk-roms.cpp` (changed)
  - `kiwami/GB-POKEMON.md` (changed: *The battle rules*)
- **Change**:
  - **`gbpk-symbols.txt`**: `red,blue rom.typeChart TypeEffects`, `yellow rom.typeChart TypeEffects`,
    `crystal,gold,silver rom.typeChart TypeMatchups`.
  - **`rom1.cpp`/`rom2.cpp`** read the table into `GbRomData::chart`. The table is triples of attacker, defender and
    multiplier (`SUPER_EFFECTIVE` 20, `NOT_VERY_EFFECTIVE` 05, `NO_EFFECT` 00, `EFFECTIVE` 10). It ends at `-1`; Gen II
    has a `-2` before the Foresight pairs, which count as any other. Each id goes through the game's `types` (step 2),
    and a pair with an unknown id or another multiplier fails the read ("TypeEffects' entry n isn't a type pair").
    Checked: Water on Fire is 2x, the first entry in both generations' tables.
  - **`rules.h`**:
    - `struct GameRules { std::array<int8_t, CELLS> chart; ... }`, with the chart's cells over a compact index of the
      game's types (`nds::Type` 0-17, the fork's 32-37 as 18-23), -1 for no entry (neutral);
    - `inline std::atomic<const GameRules*> gameRules{ nullptr }` and `useGameRules(const GameRules*)`;
    - `chart(a, d)` returns the cell (4 for no entry) while a `GameRules` is set. Without one it returns today's Gen I
      cells while `oldChart`, as the headless stand-in (no ROM) still needs.
  - **`host.cpp`**: the host makes its `GameRules` from the read ROM in `begin`, and sets it after `useRules`. The
    destructor clears it after the Session has stopped (`session.reset()` first, as today), then frees it.
  - **`game.cpp` `makeStandIn`**: unchanged. The stand-in has no chart, so `rules.h` falls back.
- **Tests**:
  - `gbpk-units`:
    - a made-up Gen I ROM's `TypeEffects` with Ghost on Psychic 0x and Ice on Fire listed as 10 read into `chart`, and
      `nds::effectiveness` gives them through upstream's entry point;
    - a table with an unknown type id fails the read;
    - with no `GameRules` set, the Gen I fallback gives today's cells.
  - `gbpk-roms.cpp`: for Red, Blue and Yellow the ROM's chart equals upstream's chart with today's five Gen I
    differences, and for Gold, Silver and Crystal upstream's chart without Fairy, cell for cell.
  - `make test`, `make -C ui`, `kiwami/tests/ci.sh .buildroot`, `kiwami/tests/gbpk-roms.sh`.
- **Commit**: `GB Pokémon hacks: each game's type chart read from its ROM`

### 4. Crystal Legacy: its data file from its build, its memory and its ROM

- **Files**:
  - `kiwami/tools/pret-extract.py`, `kiwami/tools/gbpk-symbols.txt`, `kiwami/tools/gbpk-extra.json`,
    `kiwami/tools/pokedaisy-extract.py` (changed)
  - `kiwami/rootfs/usr/share/banana/companions/gb-pokemon/crystal-legacy.ram.json` (new), `SOURCES` (changed)
  - `ui/src/kiwami/gbpk/rom2.cpp` (changed: far Pokédex pointers, far trainer groups and their party fields, Rock
    Smash's own table: *Deviations*, 2026-11-14)
  - `ui/src/kiwami/gbpk/guide-games.inc` (changed: the row)
  - `kiwami/tests/gbpk/fixtures/crystal_legacy{,_battle,_battle_menu,_battle_party}/` (new, PokeDaisy's, unchanged),
    `kiwami/tests/gbpk/NOTICE` (changed: the hacks' captures)
  - `kiwami/tests/gbpk/roms.sha1` (changed)
  - `kiwami/tests/gbpk-roms.sh`, `kiwami/tests/ci.sh` (changed: the folder's subfolders)
  - `kiwami/tests/gbpk-units.cpp`, `kiwami/tests/gbpk-roms.cpp`, `kiwami/tests/gbpk-ui.sh` (changed)
  - `kiwami/DEPENDENCIES.md` (changed: Crystal Legacy's row, rgbds 0.5.2's)
  - `kiwami/GB-POKEMON.md` (changed: a *The ROM hacks* section)
- **Change**:
  - **`pret-extract.py`** gets a build line, `<game> build <folder> <build> <sha1> <name>`, for a hack the user built
    from its tag:
    - the ROM is the checkout's working-tree `<build>.gbc`, its SHA-1 checked equal to the line's (else the run fails:
      "<folder>/<build>.gbc isn't <name>'s release: sha1 ..."), and the header's title and code read from its bytes;
    - the symbols are the working-tree `<build>.sym`, and the constants come from the checkout's `HEAD`;
    - `SOURCES` records the checkout's `HEAD` commit and its tag (`git describe --tags --exact-match`, else the run
      fails), and the SHA-256 of the `.sym` and of every constants file read;
    - the hack's `roms.sha1` isn't read (*Refinements*).
  - **`gbpk-symbols.txt`**:
    - `crystal-legacy build Pokemon_Crystal_Legacy pokecrystal 74a6727e4406c35bc2d4f81347a5c5fdb8970663 v1.3.1`;
    - `crystal-legacy` on every `crystal` line (RAM, ROM, constants, flags) whose symbol Crystal Legacy keeps. The run
      lists the others; each is then given in the hack's own symbol's name, or left out where Crystal Legacy dropped
      it (said in a comment);
    - `items`, `types` and `split` lines: `crystal-legacy split GHOST special` and `crystal-legacy split DARK physical`
      (its `type_constants.asm`'s comments; its code's check named in the comment);
    - `crystal-legacy rom.dexEntryFar PokedexDataPointerTable` (one pointer and bank a species; its symbol keeps
      Crystal's name, *Deviations*);
    - `crystal-legacy rom.trainerGroupsFar TrainerGroups`, its four party-type bits as `const` lines, and
      `crystal-legacy rom.rockSmashMons RockSmashMons` (*Deviations*, 2026-11-14).
  - **`pokedaisy-extract.py`**:
    - reads `GEN2_CRYSTAL_LEGACY`, `GEN2_DEX_CRYSTAL_LEGACY` and its SHA-1 (`GameTitles.kt`) as a cross-check of
      pret-extract's values, a key in both having to agree;
    - writes `crystal-legacy.ram.json` with `"title": "Crystal Legacy"` and `"dex_size": 251`;
    - its list of PokeDaisy's hacks left out by name (`:96-98`, `:887-892`) drops Crystal Legacy.
  - **`rom2.cpp`**: with `rom.dexEntryFar`, a species' page is the 3-byte entry's bank and address (Crystal Legacy's
    `PokedexDataPointerTable`, PokeDaisy `Gen2Dex.kt:45`), instead of `dexEntryBanks`/`dexEntryFirst`. With
    `rom.trainerGroupsFar`, a class's group is a bank and a pointer, and the party types' nickname, DVs, stat
    experience and happiness fields (their bits from `sizes`) are passed over; with `rom.rockSmashMons`, Rock Smash's
    sets are that table's, from 0 (*Deviations*, 2026-11-14).
  - **`guide-games.inc`**: `{ (GameId)106, "crystal-legacy", "BYTE", 251 }`.
  - **`roms.sha1`**: `74a6727e... gb optional crystal-legacy Pokemon Crystal - Legacy (v1.3.1) (TheSmithPlays).gbc`.
  - **`gbpk-roms.sh`**: `hashes` reads the folder's files and those of its subfolders one level down (the user keeps the
    hacks in `romhacks/`), each named by its path from the folder (`romhacks/<file>`). The rest is unchanged: `.bps`
    and other files are passed over, and a file the list hasn't is named and left alone.
  - **`kiwami/DEPENDENCIES.md`**: a row for `cRz-Shadows/Pokemon_Crystal_Legacy`. Tag `V1.3.1`, commit `6394641e`,
    pinned in `gb-pokemon/SOURCES` (`[pret]`), no licence published; built by the user and read for its symbols and
    constants only, not shipped. A row for rgbds 0.5.2, a tool for that build only, not in the image.
- **Tests**:
  - `gbpk-units` replays the four captures. For each, it asserts the README's trainer, party (species, levels, held
    items), bag (84 slots), place, and in the battle captures the battlers, as #28's `crystal*` tests do. The values are
    from the READMEs, written in the test.
  - The data file's `releases` hold the one SHA-1, and `roms.sha1`'s line matches it (the existing both-ways check).
  - `gbpk-roms.cpp` (with the ROM in `GB_ROMS`):
    - names, a known species (Chikorita, 152), Route 29's wild Pokémon, Falkner's team, every hidden item on a named
      map, Chikorita's front picture and the Pokégear's maps drawn;
    - the ROM's chart read;
    - a Pokédex page through a far pointer.
  - `gbpk-ui.sh`: `crystal_legacy` through the panel at both sizes (Party, Battle shots), asserting `companion Crystal
    Legacy`.
  - `ci.sh`'s made-up folders: a listed file in a subfolder is `ok (named romhacks/<file>)`, one two levels down isn't
    looked at, and a `.bps` beside it isn't named.
  - By hand: `pret-extract.py` and `pokedaisy-extract.py` against the checkouts, and `--check` without them.
  - `make test`, `make -C ui`, `kiwami/tests/ci.sh .buildroot`, `kiwami/tests/gbpk-roms.sh`.
- **Commit**: `GB Pokémon hacks: Crystal Legacy, from its build's symbols, its memory and its ROM`

### 5. Crystal Legacy's guide

- **Files**:
  - `kiwami/tools/pokedaisy-extract.py` (changed)
  - `kiwami/rootfs/usr/share/banana/companions/gb-pokemon/crystal-legacy.json` (new), `crystal-legacy.ram.json`
    (changed: the guide's flags, *Deviations*, 2026-11-14), `SOURCES` (changed)
  - `kiwami/tests/gbpk-units.cpp`, `kiwami/tests/gbpk-roms.cpp` (changed)
  - `kiwami/GB-POKEMON.md` (changed: *The guides*)
- **Change**:
  - **`pokedaisy-extract.py`**:
    - `crystal_guide` takes the game's tables by prefix: `CR_`/`GuideCrystalGen.kt` for Crystal,
      `CL_`/`GuideCrystalLegacyGen.kt` for Crystal Legacy, with `BOSSES_CRYSTAL_LEGACY` and `GUIDE_CRYSTAL_LEGACY` from
      `GuideCrystal.kt`;
    - Crystal Legacy's bosses whose team PokeDaisy picks by badge count (`GuideCrystal.kt:171-181`) take the team for
      the badges the story order has at that milestone. The milestone's `tips` say the gym's levels follow the badges;
    - its flag for Red is its own (`GuideCrystal.kt:205`: Legacy clears `EVENT_RED_IN_MT_SILVER` when he's beaten), so
      the rule is converted as written, never Crystal's.
  - **`crystal-legacy.json`** is written as Crystal's guide is, with the game's key and the code `BYTE`.
- **Tests**:
  - `gbpk-units`: the guide loads with `crystal-legacy`'s id, no entry skipped (`unresolved` empty with the stand-in's
    places). On the `crystal_legacy` capture the next milestone is the one its README's badges give.
  - `gbpk-roms.cpp` (ROM): every milestone's place resolves to a map of the ROM, and every boss's trainer ids are
    trainers of the ROM.
  - `pokedaisy-extract.py --check`; `make test`, `make -C ui`, `kiwami/tests/ci.sh .buildroot`,
    `kiwami/tests/gbpk-roms.sh`.
- **Commit**: `GB Pokémon hacks: Crystal Legacy's guide, from PokeDaisy's`

### 6. Yellow Legacy: its data file from its build, its memory and its ROM

- **Files**:
  - `kiwami/tools/gbpk-symbols.txt`, `gbpk-extra.json`, `pokedaisy-extract.py` (changed)
  - `kiwami/rootfs/usr/share/banana/companions/gb-pokemon/yellow-legacy.ram.json` (new), `SOURCES` (changed)
  - `ui/src/kiwami/gbpk/guide-games.inc` (changed), `rom1.cpp` (changed: its Old Rod and Metapod, *Deviations*,
    2026-11-14)
  - `kiwami/tests/gbpk/fixtures/yellow_legacy{,_battle,_battle_menu,_battle_party}/` (new), `kiwami/tests/gbpk/NOTICE`
    (changed)
  - `kiwami/tests/gbpk/roms.sha1`, `kiwami/tests/gbpk-units.cpp`, `kiwami/tests/gbpk-roms.cpp`,
    `kiwami/tests/gbpk-ui.sh` (changed)
  - `kiwami/DEPENDENCIES.md` (changed: Yellow Legacy's row, rgbds 0.6.1's)
  - `kiwami/GB-POKEMON.md` (changed)
- **Change**:
  - **`gbpk-symbols.txt`**:
    - `yellow-legacy build Pokemon_Yellow_Legacy pokeyellow 2ac87de42afc70ef6016805acb3b206644417f97 v1.0.10`;
    - `yellow-legacy` on `yellow`'s lines, as in step 4;
    - its `items`, `keyitems` and `types` (Ghost is special by its `DEF SPECIAL`: no `split` line);
    - `sizes.bagSlots` 59 from its `BAG_ITEM_CAPACITY` constant.
  - **`pokedaisy-extract.py`**: `GEN1_YELLOW_LEGACY` and `GEN1_DEX_YELLOW_LEGACY` as the cross-check, and
    `yellow-legacy.ram.json` with `"title": "Yellow Legacy"`.
  - **`guide-games.inc`**: `{ (GameId)107, "yellow-legacy", "PKYL", 151 }`.
  - **`roms.sha1`**: its line.
  - **The readers**: no change expected. Yellow Legacy keeps Yellow's structs (PokeDaisy `Gen1Reader.kt:181-199`), and
    its 2 MB MBC5 ROM is within `readRomFile`'s 8 MB. A table whose layout it changed is a *Deviation*.
  - **`kiwami/DEPENDENCIES.md`**: a row for `cRz-Shadows/Pokemon_Yellow_Legacy` (`V1.0.10`, `3a5358e8`), and one for
    rgbds 0.6.1.
- **Tests**:
  - `gbpk-units`: the four captures, as in step 4 (59 bag slots; Pikachu's friendship).
  - `gbpk-roms.cpp`:
    - names, Bulbasaur, Route 1's wild Pokémon, Brock's team, the hidden items, the pictures and the town map;
    - its chart read;
    - a Ghost move's category `SPECIAL`.
  - `gbpk-ui.sh`: `yellow_legacy` at both sizes, `companion Yellow Legacy`.
  - `make test`, `make -C ui`, `kiwami/tests/ci.sh .buildroot`, `kiwami/tests/gbpk-roms.sh`.
- **Commit**: `GB Pokémon hacks: Yellow Legacy, from its build's symbols, its memory and its ROM`

### 7. Yellow Legacy's guide

- **Files**:
  - `kiwami/tools/pokedaisy-extract.py` (changed)
  - `kiwami/rootfs/usr/share/banana/companions/gb-pokemon/yellow-legacy.json` (new), `yellow-legacy.ram.json`
    (changed: the guide's flags, *Deviations*, 2026-11-14), `SOURCES` (changed)
  - `kiwami/tests/gbpk-units.cpp`, `kiwami/tests/gbpk-ui.sh` (changed; `gbpk-roms.cpp` unchanged, *Deviations*,
    2026-11-14)
  - `kiwami/GB-POKEMON.md` (changed)
- **Change**: `GEN1_GUIDES` gets `'yellow-legacy': ('G1L', 'YELLOW_LEGACY', 'PKYL')`. `gen1_guide` reads
  `G1L_BOSSES_RAW`, `G1L_AREAS_RAW` and `GuideGen1.kt`'s `YELLOW_LEGACY_GUIDE_RAM` as it reads Yellow's.
- **Tests**: as step 5, for `yellow-legacy` (the next milestone on the `yellow_legacy` capture; the places and trainers
  on the ROM). `pokedaisy-extract.py --check`; `make test`, `make -C ui`, `kiwami/tests/ci.sh .buildroot`,
  `kiwami/tests/gbpk-roms.sh`.
- **Commit**: `GB Pokémon hacks: Yellow Legacy's guide, from PokeDaisy's`

### 8. Types of the fork's own on the pages, for PureRGB's six

- **Files**:
  - `ui/src/kiwami/gbpk/gbpk.h`, `rules.h`, `games.cpp` (changed); `games.h`, `game.h`, `game.cpp` (changed: the
    `gbtypes` test verb, *Deviations*, 2026-11-15)
  - `ui/src/nds/pages.cpp` (hook, 2 lines)
  - `ui/src/nds/logic.cpp` (hook, 1 line)
  - `kiwami/tests/gbpk-units.cpp`, `kiwami/tests/gbpk-ui.sh` (changed)
  - `docs/FORK.md` (changed: the companion's hooks in the merge checks)
  - `kiwami/GB-POKEMON.md` (changed: *The battle rules*)
- **Change**:
  - **`gbpk.h`** (header-only):
    - `constexpr nds::Type TYPELESS = (nds::Type)32, CRYSTAL_TYPE = 33, BONEMERANG = 34, TRI = 35, FLOATING = 36,
      MAGMA = 37`;
    - `inline const char* extraTypeName(nds::Type t)` gives "Typeless", "Crystal", "Ground" (Bonemerang: PureRGB's
      `TypeNames` names it Ground), "Tri", "Floating", "Magma", or nullptr for any other;
    - `inline uint32_t extraTypeColor(nds::Type t)` gives Typeless 0x68a090, Crystal 0x88d0e0, Bonemerang Ground's
      0xe0c068, Tri 0xc8b0e8, Floating 0x98a8f0, Magma 0xd05038.
  - **`games.cpp`**: the type names `typeless` .. `magma` in a data file's `types` map to these (step 2's names).
  - **`rules.h`**: the chart's compact index covers them (step 3), so a ROM chart with them is read whole.
  - **Hooks**:
    1. `ui/src/nds/pages.cpp`, after `#include "../lang.h"` (`:4`):
       `#include "../kiwami/gbpk/gbpk.h"	// kiwami: the fork's types (PureRGB's), named and coloured`
    2. `ui/src/nds/pages.cpp`, `typeChip` (`:662`), before `if (t >= NUM_TYPES) return 0;`:
       `if (const char* k = kiwami::gbpk::extraTypeName(t)) return chip(lang::tr("Pokémon types", k), kiwami::gbpk::extraTypeColor(t), x, y, true, measureOnly, h);	// kiwami:`
    3. `ui/src/nds/logic.cpp` `typeName` (`:414`), the line changed to
       `static std::string typeName(Type t) { if (const char* k = kiwami::gbpk::extraTypeName(t)) return k; return t < NUM_TYPES ? TYPE_NAME[t] : "?"; }	// kiwami: the fork's types`
       (`logic.cpp` already includes `rules.h`, which includes `gbpk.h`).
  - **`docs/FORK.md`**: the three lines, in the companion's paragraph of the merge checks.
- **Tests**:
  - `gbpk-units`:
    - `extraTypeName` and `extraTypeColor` for the six and for `FIRE`/`NO_TYPE`;
    - a made-up ROM chart with Water on Crystal 0.5x, Ground on Floating 0x and Bonemerang on Flying 1x (no entry)
      gives those through `nds::effectiveness`;
    - upstream's advice (`nds::computeAdvice`, whose static `teamScore` loops over `numTypes()`) runs over a party with
      a Crystal and Floating member and a Tri move, and its texts name "Tri", never "?".
  - By hand, once: `gbpk-units` built with `-fsanitize=address,undefined` runs clean (its output in the step's
    overview).
  - `gbpk-ui.sh`: a stand-in species of types Crystal and Floating on the Party page at both sizes. The test asserts the
    chip's colour, 0x88d0e0, at the chip's place in the PNG (ImageMagick's `convert ... -format '%[pixel:p{x,y}]'`).
  - `make test` (upstream's renders unchanged: the hooks change nothing for upstream's types), `make -C ui`,
    `kiwami/tests/ci.sh .buildroot`.
- **Commit**: `GB Pokémon hacks: types of the fork's own on the pages, for PureRGB's six`

### 9. PureRed, PureBlue and PureGreen: their data files from their builds, their memory and their ROMs

- **Files**:
  - `kiwami/tools/pret-extract.py`, `gbpk-symbols.txt`, `gbpk-extra.json`, `pokedaisy-extract.py` (changed)
  - `kiwami/rootfs/usr/share/banana/companions/gb-pokemon/{purered,pureblue,puregreen}.ram.json` (new), `SOURCES`
    (changed)
  - `ui/src/kiwami/gbpk/games.h`, `games.cpp`, `game.cpp`, `rom.h`, `rom.cpp`, `rom1.cpp`, `text.h`, `text.cpp`,
    `reader.cpp`, `guide-games.inc` (changed; `rom.cpp` for the character set and a form's picture, `pics.cpp`
    unchanged: *Deviations*, 2026-11-15)
  - `kiwami/tests/gbpk/fixtures/purered{,_battle,_battle_faint,_battle_menu,_battle_party,_battle_switch_menu}/`,
    `pureblue{,_battle}/` (new), `kiwami/tests/gbpk/NOTICE` (changed: the captures named)
  - `kiwami/tests/gbpk/roms.sha1`, `kiwami/tests/gbpk-units.cpp`, `kiwami/tests/gbpk-roms.cpp`,
    `kiwami/tests/gbpk-ui.sh` (changed)
  - `kiwami/DEPENDENCIES.md` (changed: PureRGB's row, rgbds 1.0.3's)
  - `kiwami/GB-POKEMON.md` (changed)
- **Change**:
  - **`gbpk-symbols.txt`**:
    - six build lines: `purered`, `pureblue` and `puregreen` builds in `pureRGB-2.7.6` and in `pureRGB-2.7.7`
      (`pokered`, `pokeblue`, `pokegreen`). The SHA-1s are PokeDaisy's for PureRed and PureBlue v2.7.6 (`2e94d09c`,
      `d419fe24`), and those of the user's patched ROMs for the other four, each checked against the user's own build.
      Where a game's two versions disagree on a value (a rebuild moves most of them), the value goes in each release's
      own `ram` or `rom` (below) rather than failing the run as today;
    - `purered,pureblue,puregreen` on `red`'s lines, as in step 4;
    - their `items` (the Hyper Ball as a `ball`), `keyitems` and `types` (the six, by the fork's names), and
      `purered,pureblue,puregreen split GHOST stats`;
    - the new layouts' symbols:
      - `rom.dexEntryData` (the fixed 8-byte pages by Pokédex number);
      - `rom.dexCategoryMap` and `rom.dexCategories`;
      - `const.basePicBank` (`BASE_PICBANK`, the base data's byte holding the picture's bank);
      - `sizes.baseDataSize` 35 (its `BASE_DATA_SIZE`);
      - `const.textFar` (its `text_far_end` command, $15);
    - `sizes.bagSlots` 30.
  - **`pokedaisy-extract.py`**: `GEN1_PURE_RED`, `GEN1_DEX_PURE_RED` and their SHA-1s as the cross-check (v2.7.6 only:
    PokeDaisy has nothing for v2.7.7 or PureGreen); the three data files, `"dex_size": 151`.
  - **The schema: a release's own addresses.** A release may carry `ram` and `rom` objects of its own, laid over the
    game's when `confirmAny` picks it (`games.cpp`: `loadData` keeps each release's, `confirmAny` merges the picked
    one's before the reader opens). A game whose releases all agree, every retail one among them, has none. The
    stand-in path (`gbpanel`, `gbpk-units`' replays) picks the release whose SHA-1 starts with the one the capture's
    `README.txt` names on its `rom:` line (`sha1 2e94d09c...`), else the first.
  - **`rom1.cpp`**, chosen by the data file's keys, not by the game's id:
    - **base data** of `baseDataSize` bytes, the front picture's bank at `basePicBank` rather than by the internal
      index's range (`picsTangela` .. `picsStarmie` absent), and Mew in the main table;
    - **Pokédex pages** from `dexEntryData`: feet, inches, pounds x 10 and the text's `textFar` command with its
      pointer, by Pokédex number. The category is `dexCategories`' string at `dexCategoryMap`'s index (PokeDaisy
      `Gen1Dex.kt:32-42`);
    - **forms**: an internal index whose `PokedexOrder` number another index already has is a form. The forms go after
      the 151 in `species` (each with its own base data, name and learnset). The base species gets `formsAt` and
      `formCount`, as `ui/src/gba/gen3rom.cpp:817` lays them out. `GbRomData::formOf` maps an internal index to its
      national number and form;
    - its new moves and changed ones are read as any move. The priorities go by `const.quickAttack` and
      `const.counter` (step 2's sizes) from its own constants: in PureRGB, Counter's id is Drain Punch's, so its line
      names the move PureRGB gives -1 (its `engine/battle/core.asm`), or none.
  - **`text.cpp`**: `decode` takes a character set, not a generation: `GEN1`, `GEN2`, `PURE`. `PURE` is Gen I's with
    PureRGB's `charmap.asm` changes: the words at 0x33-0x47, 0x4D "is", 0x5B-0x5E, 0x74, 0x9E-0x9F, 0xE9-0xEB, as
    PokeDaisy's `Gen1Charset.PURE` lists them. The data file's `"charset": "pure"` chooses it, and `RomView` carries
    it.
  - **`reader.cpp`**: a party or battle Pokémon of a form's internal index gets the base species' number and its `form`
    (`Mon::form`), so upstream's pages take the form's own data (`nds::rom.cpp:159`'s `formIndex`).
  - **`guide-games.inc`**: `{ 108, "purered", "PKRD", 151 }`, `{ 109, "pureblue", "PKBL", 151 }`,
    `{ 110, "puregreen", "PKRD", 151 }`.
  - **`kiwami/DEPENDENCIES.md`**: a row for `Vortyne/pureRGB` at `v2.7.6` (`9c6d40f6`) and `v2.7.7` (`b5a155eb`), and
    one for rgbds 1.0.3 (its builds give the releases' SHA-1s, checked 2026-11-14, *Before the steps* §5).
- **Tests**:
  - `gbpk-units`:
    - the eight captures: Nelson's party of six level 100s, Vermilion City's Pokémon Center (`wCurMap` 0x59) at (3, 5),
      16 items in a 30-slot bag, 10 owned and 106 seen (from `purered/README.txt`), and each battle capture's
      battlers;
    - a made-up ROM with 35-byte base data and fixed Pokédex records reads a form after the 151 with `formsAt` on its
      base;
    - a made-up data file with a release's own `ram`: `confirmAny` lays it over the game's for that release only, and a
      capture's README picks its release;
    - `decode(PURE, ...)` on its words.
  - `gbpk-roms.cpp`, for each of the six builds the user has:
    - names, Bulbasaur, Route 1, Brock's team, the hidden items, the pictures and the town map;
    - a form's own types (Floating Weezing: Poison and Floating);
    - the Hyper Ball's name and pocket, and its chart read with the six types.
  - `gbpk-ui.sh`: `purered` and `purered_battle` at both sizes, `companion PureRed`.
  - `make test`, `make -C ui`, `kiwami/tests/ci.sh .buildroot`, `kiwami/tests/gbpk-roms.sh`.
- **Commit**: `GB Pokémon hacks: PureRed, PureBlue and PureGreen, from their builds' symbols, their memory and their ROMs`

### 10. PureRGB's switched types and matchups, signature moves and Hyper Ball

- **Files**:
  - `kiwami/tools/gbpk-symbols.txt`, `gbpk-extra.json`, the three `pure*.ram.json`, `SOURCES` (changed)
  - `ui/src/kiwami/gbpk/rom.h`, `rom.cpp`, `rom1.cpp`, `rom2.cpp`, `reader.h`, `reader.cpp`, `rules.h`, `host.h`,
    `host.cpp` (changed; `rom.cpp` and `rom2.cpp`: *Deviations*, 2026-11-15)
  - `kiwami/tests/gbpk-units.cpp`, `kiwami/tests/gbpk-roms.cpp` (changed)
  - `kiwami/GB-POKEMON.md` (changed: *The battle rules*, *Reading the game's memory*, what PureRGB's isn't followed)
- **Change**:
  - **`gbpk-symbols.txt`**:
    - `ram.typeRemapFlags wPkmnTypeRemapFlags`, `ram.options3 wOptions3`;
    - `rom.originalTypings OriginalTypings`, `rom.remappableMoves RemappableMoves`;
    - `const.signaturesOff constants/event_constants.asm FLAG_SIGNATURE_MOVES_TURNED_OFF`;
    - `const.hyperBall constants/item_constants.asm HYPER_BALL` (*Deviations*, 2026-11-15: no `const.volcanicMagmar`
      and `const.magmar`, as the signatures go by national number).
  - **`rom1.cpp`**:
    - `OriginalTypings` (species, type, type to -1) into `GbRomData::originalTypes`, in the list's order: its bit n is
      the n-th entry, as `IsMonTypeRemapped` counts;
    - `RemappableMoves` (move, species or -1, power or -1/-2, accuracy, to -1) into `GameRules::signatures`, the
      species-bound entries only;
    - for every species, whether its base Special is above its Attack, into `GameRules::ghostSpecial`.
  - **`reader.cpp`**: the two bytes and the flag's byte are read with the set (`reader.h`: `uint32_t typeRemap() const`,
    `uint8_t options3() const`, `bool signaturesOff() const`).
  - **`rules.h`**:
    - `inline std::atomic<uint8_t> matchupOptions` and `std::atomic<bool> signaturesOff`, set by the host from each set,
      as `setBadges` is;
    - `chart(a, d)` applies the four options over the ROM's cells while `GameRules::pureOptions`: bit 0 Ghost on
      Psychic 2x or 0x, bit 1 Ice on Fire 1x or 0.5x, bit 2 Bug on Poison 2x or 0.5x, bit 3 Poison on Bug 2x or 1x
      (`ram/wram.asm`'s comments; the code that applies them, named in the comment, read in this step);
    - `damage`:
      - a signature move used by its species (Volcanic Magmar as Magmar) takes the table's power and accuracy unless
        `signaturesOff`;
      - a Ghost move is special when the attacker's species is in `ghostSpecial`;
    - the Hyper Ball's catch chance by PureRGB's `ItemUseBall` (`engine/items/item_effects.asm` at v2.7.7, read in this
      step; its ball factor and HP divisor in `catchGen1`'s terms). The ball is `GEN1_ONLY + const.hyperBall`.
  - **`host.cpp`**: when `typeRemap()` differs from the last, the host makes a copy of the ROM's data with each
    switched species' types from `originalTypes` (and its forms', if any) and starts its Session again on the copy.
    It logs `gb companion: PureRGB: <n> species with their original types, matchups 0x<bits>`, and does the same when
    the options change.
- **Tests**:
  - `gbpk-units`:
    - remap bits for Butterfree (the list's first) give it Bug/Flying on the Party page's data;
    - each option bit changes its cell through `nds::effectiveness`, and the other cells don't;
    - Drill Peck used by Fearow is 110 power, by Spearow its listed power, and the listed power with `signaturesOff`;
    - a Ghost move is special for Gengar's base stats and physical for a species with Attack above Special;
    - the Hyper Ball's chance on a hand-worked case from the code's formula;
    - the `purered` capture's own bytes give no remap and options 0.
  - `gbpk-roms.cpp`: `OriginalTypings` has 23 entries, Butterfree's first; `RemappableMoves` has Drill Peck for Fearow
    at 110.
  - `make test`, `make -C ui`, `kiwami/tests/ci.sh .buildroot`, `kiwami/tests/gbpk-roms.sh`.
- **Commit**: `GB Pokémon hacks: PureRGB's switched types and matchups, signature moves and Hyper Ball`

### 11. PureRGB's guides

- **Files**:
  - `kiwami/tools/pokedaisy-extract.py`, `kiwami/tools/gbpk-symbols.txt`, `gbpk-extra.json` (changed)
  - `kiwami/rootfs/usr/share/banana/companions/gb-pokemon/{purered,pureblue,puregreen}.json` (new), their `.ram.json`
    and `SOURCES` (changed)
  - `ui/src/kiwami/gbpk/host.cpp`, `gsguide.h`, `gsguide.cpp`, `games.h`, `games.cpp` (changed; all but `host.cpp`:
    *Deviations*, 2026-11-15)
  - `kiwami/tests/gbpk-units.cpp`, `kiwami/tests/gbpk-roms.cpp`, `kiwami/tests/gbpk-ui.sh` (changed)
  - `kiwami/GB-POKEMON.md` (changed: *The guides*, *The data files*)
- **Change**:
  - **`pokedaisy-extract.py`**:
    - `gen1_guide` converts Red's (`G1R_*`) for `purered` and `puregreen`, and Blue's (`G1B_*`) for `pureblue`;
    - each flag goes by its name through PureRGB's `constants/event_constants.asm` (`pret-extract.py`'s
      `const.flag.<NAME>` lines, which the conversion writes into `gbpk-symbols.txt`'s generated block). A name PureRGB
      doesn't have fails the run, and is given its replacement by a hand-written line with its source (as the
      investigation's check found a few renamed: `EVENT_OAK_GOT_PARCEL`, the Elite Four's autowalk events);
    - each boss gets `trainer_ids` (class << 8 | number: PureRGB's class constants and a table of the story's parties,
      `GEN1_BOSS_PARTIES`, as `G1R_BOSSES_RAW` has neither: *Deviations*, 2026-11-15), and its `ace_level` and
      `level_cap` are dropped.
  - **`host.cpp` `guideFor`**: a milestone with `trainer_ids` and no `level_cap` takes its `foeAceLevel` and `levelCap`
    from the ROM's parties of those trainers (the highest level), when the data file says `"guide_levels": "rom"`
    (`gsguide.cpp` `romLevels`, which `guideFor` calls, so `gbpk-units` reaches it).
    That's PureRGB's three, and any later hack whose levels aren't PokeDaisy's.
- **Tests**:
  - `gbpk-units`: the three guides load with no entry skipped. On the `purered` capture (eight badges) the next
    milestone is the Elite Four's. A stand-in ROM whose Brock has a level 20 ace gives the first milestone level 20.
  - `gbpk-roms.cpp`: every milestone's trainers are the ROM's and their levels are the ROM's.
  - `pokedaisy-extract.py --check`, `pret-extract.py --check`; `make test`, `make -C ui`,
    `kiwami/tests/ci.sh .buildroot`, `kiwami/tests/gbpk-roms.sh`.
- **Commit**: `GB Pokémon hacks: PureRGB's guides, Red's and Blue's with its flags and its levels`

### 12. Crystal Clear's addresses, mapped from pokecrystal's

- **Files**:
  - `kiwami/tools/gbpk-port.py` (new)
  - `kiwami/tools/gbpk-port.json` (new: its output)
  - `kiwami/tools/gbpk-symbols.txt` (changed: Crystal Clear's lines)
  - `kiwami/tools/pret-extract.py` (changed: the ported game's lines passed over, *Deviations*, 2026-11-15)
  - `kiwami/rootfs/usr/share/banana/companions/gb-pokemon/SOURCES` (changed: `[port]`)
  - `kiwami/tests/ci.sh` (changed)
  - `kiwami/GB-POKEMON.md` (changed: *Where the data comes from*)
- **Change**:
  - **`gbpk-port.py <pret folder> <roms folder> [--check]`** (Python 3, standard library). It ports pokecrystal11's
    symbols into each Crystal Clear build by the code that uses them, the method of PokeDaisy's
    `scripts/port_gen1.py` (named in its header; the script is the fork's own):
    - WRAM, HRAM and ROM symbols: each use (`ld a,[nn]`, `ld [nn],a`, `ld hl/de/bc,nn`, `call`, `jp`) in Crystal rev
      1, the code around it (10 bytes each side, other 16-bit operands masked) looked up in the hack. The operand at
      the same place votes. The bank of a ROM symbol comes from the `ld a,n` before it;
    - a symbol with no vote, or two values with votes, fails the run with its name. A table no code points at is found
      by its bytes (`--data`-style lines in the list), and must occur once.
  - **`gbpk-symbols.txt`**:
    - eight lines, one per build: `crystal-clear port pokecrystal pokecrystal11 <sha1> <name>`, with PokeDaisy's four
      SHA-1s (`a1c4da6b`, `6fc677bf` v2.5.11; `84d5c492`, `534876e4` v2.6.0) and the four SGB Mode builds' from the
      user's files (`8be3dea8`, `913c49bd` v2.5.11; `059632e8`, `88c943e6` v2.6.0: the user's request, 2026-11-13);
    - `crystal-clear` on every `crystal` line;
    - `crystal-clear items|types from crystal`. Its species, moves and types are Crystal's byte for byte (PokeDaisy
      `gen_gen2_tables.py`, checked with `--base`), and its item ids are found by Crystal's item names in its
      `ItemNames` on the ROM (numbers only written).
  - **The output** `gbpk-port.json`: the values a build, each with its votes. Values that differ between the two
    versions are kept per release (PokeDaisy: the move descriptions, 0x20 bytes apart). A version's four builds (Standard
    Colors, Color Filter, each with and without SGB Mode) must agree on every value the companion reads, or the run
    fails with the symbol and the builds (the SGB Mode builds differ in their palettes, which aren't read; a difference
    elsewhere is a *Deviation*).
  - **`SOURCES` `[port]`**: the eight ROMs' and Crystal rev 1's SHA-1s, `pokecrystal11.sym`'s SHA-256, and the output's.
    `--check` without the ROMs verifies the output against them.
  - **PokeDaisy's values** for Crystal Clear (`GEN2_CRYSTAL_CLEAR_*`, `GEN2_DEX_CRYSTAL_CLEAR_*`, `gen_gen2_tables.py`'s
    `crystal_clear`, read with `ast`) are compared by `pokedaisy-extract.py` in step 13. A disagreement fails there.
  - **The header's title** is read from the user's ROMs here and written in the output.
  - **`ci.sh`**: `python3 -I kiwami/tools/gbpk-port.py --check`.
- **Tests**:
  - `ci.sh`: the `--check`.
  - A made-up pair of "ROMs" in the test (a byte string with a `ld a,[$D000]` use, and the same moved by 0x40 with
    other bytes changed around it) ports the symbol, and a third with the use rewritten fails with its name
    (`python3 -I kiwami/tools/gbpk-port.py --self-test`, run by `ci.sh`).
  - By hand: the run on the user's ROMs, its report of every symbol and its votes in the step's overview.
- **Commit**: `GB Pokémon hacks: Crystal Clear's addresses, mapped from pokecrystal's by the code that uses them`

### 13. Crystal Clear: its data file, its packed text, its memory and its ROM

- **Files**:
  - `kiwami/tools/pokedaisy-extract.py` (changed)
  - `kiwami/rootfs/usr/share/banana/companions/gb-pokemon/crystal-clear.ram.json` (new), `SOURCES` (changed)
  - `ui/src/kiwami/gbpk/rom.h`, `rom.cpp`, `rom2.cpp`, `maps.cpp`, `text.h`, `text.cpp`, `guide-games.inc` (changed)
  - `kiwami/tests/gbpk/fixtures/crystal_clear{,_battle,_battle_menu,_trainer,_v2_5_11,_v2_5_11_battle}/` (new)
  - `kiwami/tests/gbpk/roms.sha1`, `kiwami/tests/gbpk-units.cpp`, `kiwami/tests/gbpk-roms.cpp`,
    `kiwami/tests/gbpk-ui.sh` (changed)
  - `kiwami/GB-POKEMON.md` (changed)
  - (*Deviations*, 2026-11-15) `kiwami/tools/gbpk-port.py`, `gbpk-symbols.txt`, `gbpk-port.json` (changed: `port.fixed`,
    the layouts' lines, the hack's own palettes), `ui/src/kiwami/gbpk/host.h`, `host.cpp` (changed: the parties by the
    save's badges), `kiwami/tests/gbpk/NOTICE` (changed: the captures)
- **Change**:
  - **`pokedaisy-extract.py`** merges `gbpk-port.json`, checks it against PokeDaisy's Crystal Clear values (a key in
    both must agree), and writes `crystal-clear.ram.json` (`"title": "Crystal Clear"`, `"dex_size": 251`,
    `"charset": "gen2"`, `"text_dict"`).
  - **A release's own `rom`** (step 9's schema): Crystal Clear's v2.5.11 move descriptions, from `gbpk-port.json`.
  - **`text.cpp`/`rom.cpp`**: `RomView::text` decodes a dictionary byte 0x16-0x5F (Crystal Clear's `PlaceNextChar`
    table at `rom.textDict`, 3 bytes an entry: 0 and a bank-0 string's pointer, or 1 and a routine). A string entry is
    expanded in place, and a routine entry is decoded as PokeDaisy `Gen2Dex.kt:25-27` names it. An unknown routine
    stops the text there, logged once.
  - **`rom2.cpp`**: Crystal Clear's landmarks past the Fast Ship take their region from `rom.landmarkRegions`
    (PokeDaisy `gen_gen2_tables.py`: `landmark_regions`). `maps.cpp` places them on the Pokégear map of that region.
  - **`guide-games.inc`**: `{ (GameId)111, "crystal-clear", "BYTE", 251 }` (every build's header code, *Before the
    steps* §6; step 12 writes it in its output from the ROMs).
  - **`roms.sha1`**: the eight lines.
- **Tests**:
  - `gbpk-units`:
    - the six captures. `crystal_clear`: CHRIS, a Bulbasaur holding a Miracle Seed and a Sentret, Route 29
      (`wMapGroup` 24, `wMapNumber` 3 at `DA3A`) at (45, 11). The trainer capture is Mom's battle;
    - the release's own `rom` laid over the game's for v2.5.11 and not for v2.6.0;
    - a made-up dictionary decoding "with " and "#MON".
  - `gbpk-roms.cpp`, for each of the eight builds:
    - names, Bulbasaur, Route 29, a gym leader's team, the hidden items, the pictures and the Pokégear maps;
    - a move description and a Pokédex page decoded with no unknown byte;
    - every landmark named and placed.
  - `gbpk-ui.sh`: `crystal_clear` at both sizes, `companion Crystal Clear`.
  - `make test`, `make -C ui`, `kiwami/tests/ci.sh .buildroot`, `kiwami/tests/gbpk-roms.sh`.
- **Commit**: `GB Pokémon hacks: Crystal Clear, its packed text, its memory and its ROM`

### 14. Crystal Clear's guide, built from its ROM

- **Files**:
  - `ui/src/kiwami/gbpk/gsguide.h`, `gsguide.cpp` (changed)
  - `kiwami/tools/pokedaisy-extract.py`, `crystal-clear.ram.json`, `SOURCES` (changed: `"guide": "rom"`)
  - `kiwami/tests/gbpk-units.cpp`, `kiwami/tests/gbpk-roms.cpp` (changed)
  - `kiwami/GB-POKEMON.md` (changed: *The guides*)
  - (*Deviations*, 2026-11-15) `kiwami/tools/gbpk-port.py`, `gbpk-symbols.txt`, `gbpk-port.json`, `gbpk-extra.json`
    (changed: `port.trainer`, the guide's classes and badges), `crystal.ram.json`, `gold.ram.json`, `silver.ram.json`
    (regenerated: the guide's constants), `ui/src/kiwami/gbpk/games.h`, `games.cpp` (changed: `"rom-regions"`),
    `host.h`, `host.cpp` (changed: the guide made again for the save's badges), `kiwami/tests/gbpk-ui.sh` (changed)
- **Change**:
  - **`gsguide.cpp`**: `romGuide` takes the stages from the data file. Gold and Silver keep today's (Johto's leaders,
    the Elite Four, the Champion, Kanto's leaders). Crystal Clear's are its regions, Johto and Kanto: each region's
    leaders by level, then the Elite Four and the Champion, from the region the save started in (the badges' order
    tells it: a Kanto badge before any of Johto's is a Kanto start).
  - A leader is done by its badge, as for Gold and Silver. Its class and badge come from Crystal's constants, checked
    on the ROM: each leader class's name in the ROM's `TrainerClassNames` is the one Crystal has there.
  - **The data file** says `"guide": "rom"`.
- **Tests**:
  - `gbpk-units`: a stand-in with a Kanto badge only gives Kanto's leaders first, and one with Falkner's badge gives
    Johto's.
  - `gbpk-roms.cpp`: on each build, 16 leaders, the Elite Four and the Champion found, each leader's level ordered
    within its region.
  - `make test`, `make -C ui`, `kiwami/tests/ci.sh .buildroot`, `kiwami/tests/gbpk-roms.sh`.
- **Commit**: `GB Pokémon hacks: Crystal Clear's guide, built from its ROM by region`

### 15. The documentation, and what the hacks weigh

- **Files**:
  - `kiwami/GB-POKEMON.md` (changed)
  - `CLAUDE.md` (changed: the companion's line names the hacks)
- **Change**:
  - **`kiwami/GB-POKEMON.md`**:
    - *The ROM hacks*: the sixteen releases and where each one's data comes from (the user's build at its tag with its
      rgbds, or the port script on the ROMs);
    - *Adding a version*: clone or tag, build with the recorded rgbds, add the build or port line with the release's
      SHA-1, run the scripts, add the `roms.sha1` line, and PokeDaisy's captures if it has any;
    - *What it doesn't do*: PureRGB's battle-state moves, Crystal Legacy's gym levels shown for the story order's badge
      count, Polished Crystal and Prism (their issues);
    - *What it weighs*: the image's new files (bytes before and after squashfs: Buildroot's
      `output/rgds-plus/host/bin/mksquashfs` on the folder, with the image's compression from the defconfig's
      `BR2_TARGET_ROOTFS_SQUASHFS*` options) and
      banana-ui's growth on x86-64 (`size ui/banana-ui` against step 1's parent's build).
  - **`CLAUDE.md`**: the `ui/src/kiwami/gbpk/` line says "(Red to Crystal, and four ROM hacks, on the game panel)".
- **Tests**: `kiwami/tests/ci.sh .buildroot` (the docs' checks). The numbers measured, written in the doc.
- **Commit**: `GB Pokémon hacks: the documentation, and what the hacks weigh`

### 16. Final check

- `make test`, `make -C ui`, `make -C system/bananad CFLAGS="-O2 -Werror"`, `kiwami/tests/ci.sh .buildroot`.
- `kiwami/tests/gbpk-roms.sh` with every ROM `roms.sha1` lists in `GB_ROMS` (the retail ones and the sixteen hacks).
- `gbpk-units` once with `-fsanitize=address,undefined` (step 8's run again).
- The scripts: `pret-extract.py`, `pokedaisy-extract.py` and `gbpk-port.py` against the checkouts and ROMs give the
  committed files, and `--check` without them passes.
- The hooks: `git diff --stat main...HEAD` and `git grep -n 'kiwami:'` (the commands in `docs/FORK.md`) agree, and the
  only new ones are step 8's three lines.
- With the rows Off, `make test`'s renders in `ui/tests/out/` match `main`'s, and `gbpk-ui.sh`'s retail shots match
  `kiwami`'s.
- `kiwami/DEPENDENCIES.md` lists the three repositories and rgbds 0.5.2, 0.6.1 and 1.0.3.
- `docs/FORK.md`, `docs/ARCHITECTURE.md`, `CLAUDE.md` updated where the feature changes what they describe.
- The manual tests below all passed.
- Every acceptance criterion is met: each is listed by at least one automated or manual test, and those passed.

## Automated tests

| Test | Where | What it checks | Criteria |
|---|---|---|---|
| Detection | `kiwami/tests/gbpk-units.cpp` | several games to a title, the SHA-1 picks; an unknown SHA-1 is none; a title no data file has is none; fixture names to keys; `baseGame` | A1, A2, A16 |
| Retail tables unchanged | `gbpk-units.cpp` | the regenerated retail items, pockets and types equal `items.inc`'s and `gen2Type`'s; the Gen I fallback chart | A3, A9 |
| Split and chart | `gbpk-units.cpp` | a type's category by the data file's split; a ROM chart through `nds::effectiveness` | A9 |
| Crystal Legacy's memory | `gbpk-units.cpp` | its four captures against their READMEs | A4 |
| Yellow Legacy's memory | `gbpk-units.cpp` | its four captures | A5 |
| PureRGB's memory and layouts | `gbpk-units.cpp` | its eight captures; 35-byte base data, fixed Pokédex records, forms after the 151; its character set | A6, A8 |
| Crystal Clear's memory and text | `gbpk-units.cpp` | its six captures; a release's own `rom`; the dictionary | A7, A8 |
| The fork's types | `gbpk-units.cpp`, `gbpk-ui.sh` | names, colours, chart cells; the chip drawn in the Party page's PNG | A10 |
| PureRGB's switches | `gbpk-units.cpp` | type remap, the four options, signature moves, Ghost by stats, the Hyper Ball | A9, A11 |
| Guides | `gbpk-units.cpp` | each hack's guide loads whole and follows a capture's save; ROM levels; Crystal Clear's regions | A12 |
| The hacks' ROMs | `kiwami/tests/gbpk-roms.cpp`, `gbpk-roms.sh` (with `GB_ROMS`) | every hack's tables, pictures, maps, chart, text, guides' places and trainers; the retail charts equal today's | A3, A8, A9, A12 |
| Pages at both sizes | `kiwami/tests/gbpk-ui.sh` | each hack's capture through the panel at 640x480 and 1024x768; retail shots unchanged | A3, A15 |
| Extraction | `kiwami/tests/ci.sh` | `pret-extract.py`, `pokedaisy-extract.py`, `gbpk-port.py` `--check` and `--self-test`; the data files' addresses; the name scan over the new files | A13 |
| Upstream unchanged | `make test` | upstream's renders and tests, with the three new hooks | A3 |

## Manual tests

On the RG DS Plus, the only board the user has (the RG DS has the same SoC and both panels; its untested difference is
the panel size, which `gbpk-ui.sh` covers at both sizes). Logs are fetched over SSH/SFTP (Settings > Network > SSH) or
from the card on a computer (`banana/logs/` on the card is `/data/banana/logs/` on the device). The image is built
from the branch at the step named.

| # | Board | Steps | Expected | Logs to collect | Criteria | Outcome |
|---|---|---|---|---|---|---|
| M1 | rgds-plus | After step 5. Crystal Legacy from Game Boy Color: Party, the bag, Dex, Map against the game's screens; a wild battle with balls, a trainer battle; the Guide page. | The hack's title in the log; the party, the 84-slot bag and the place match; damage ranges and a Ghost move's category hold; the guide's next milestone is the save's. | `banana-ui.log`: `gb companion: Crystal (or Crystal Legacy...)`, `Crystal Legacy (v1.3.1) read in N ms`, `guide: N milestones` | A1, A4, A9, A12 | not run |
| M2 | rgds-plus | After step 7. Yellow Legacy: as M1, and Pikachu's summary. | As M1; Pikachu's friendship shown; the 59-slot bag. | `banana-ui.log` | A1, A5, A12 | not run |
| M3 | rgds-plus | After step 11. PureRed v2.7.7 and PureGreen v2.7.6, each from Game Boy (`.gb`) and from Game Boy Color: Party, a battle against a Pokémon of one of its types (a Floating Weezing or a Hardened Onix if the save has one; Tri Attack otherwise), the type NPC switching a species back, the battle options' four matchups switched, a signature move, a Hyper Ball throw, the Guide page. | Chips name and colour the hack's types; the matchups and damage follow the options; the switched species shows its original types after the NPC; the signature move's power; the guide's levels are the ROM's. | `banana-ui.log`: `PureRGB: N species with their original types, matchups 0x...` after each switch | A1, A6, A10, A11, A12, A14 | not run |
| M4 | rgds-plus | After step 14. Crystal Clear v2.6.0 and v2.5.11 (one Color Filter build and one SGB Mode build among them): Party, Map (a landmark past the Fast Ship), a move description, a Dex page, a battle, the Guide page. | Text reads whole (no stray characters); the landmark placed on its region; the guide by region. | `banana-ui.log`: no `text: unknown routine` line | A1, A7, A8, A12 | not run |
| M5 | rgds-plus | A version of a hack that isn't listed (an older Crystal Legacy or PureRGB, if the user has one), then retail Crystal and Red. | "Not supported" alone for the first; retail as before. | `banana-ui.log`: `not a release the companion knows` | A2, A3 | not run |
| M6 | rgds-plus | PureRed with its COLORS option on, Crystal Legacy and Crystal Clear: Frontend on Auto (BananaFront) and on RetroArch, the Game Boy Color core gambatte then SameBoy; a battle's start in each. | The pages follow the game in all; PureRed's party stays right through the battle's palette fade. | `banana-ui.log` | A15 | not run |
| M7 | rgds-plus | One hack mid-battle: close the lid a minute, open it, fight on. A minute of walking with RetroArch's frame counter, row Off then On. | The pages resume and follow; the same frame rate both ways. | `banana-ui.log`, `/data/banana/logs/bananad.log` around the sleep | A15, A16 | not run |

Notes from the runs (what the logs showed, what was fixed):

## Deviations

Where the implementation turned out different from this plan, why, and what changed (dated).

- **2026-11-14, step 1**: Gold's and Silver's ROM-built guide as the data files' `"guide": "rom"` moves to step 2. The
  data files are `pokedaisy-extract.py`'s output, which step 1 doesn't touch and step 2 regenerates anyway; so step 1
  needs no PokeDaisy checkout, as *Before the steps* says. Step 1 leaves `host.cpp:26` on Gold's and Silver's ids.
- **2026-11-14, step 1**: `kiwami/tests/gbpk-roms.cpp` added to step 1's files. It calls `detect` and `confirm`, so it
  follows the new signature: it loads `detect`'s candidates and picks among them with `confirmAny`, as the host does.
- **2026-11-14, step 2**: Gen I's types aren't exactly today's `gen2Type`. Today's maps 9 to Steel and 0x1B to Dark for
  every game, but pokered's and pokeyellow's `type_constants.asm` define neither, so Red's, Blue's and Yellow's data
  files give no type for them (no ROM or save of theirs holds those ids). The test expects today's map for Gen I but
  those two ids, `NO_TYPE`; Gen II's is today's exactly.
- **2026-11-14, step 2**: `kiwami/tests/gbpk-roms.cpp` added to step 2's files: its two calls of `gen1Item` and
  `gen2Item` pass the game's data.
- **2026-11-14, step 3**: `ui/src/kiwami/gbpk/rom.cpp` and `host.h` added to step 3's files. The table's reading is
  the same in both generations, so it is one function, `readTypeChart` in `rom.cpp`, that `rom1.cpp` and `rom2.cpp`
  call; `host.h` holds the host's `GameRules`. `rom.h` includes `rules.h` for the chart's type. `game.cpp` is unchanged,
  as the step said.
- **2026-11-14, step 3**: Water on Fire is the first entry of Gen I's `TypeEffects` only; Gen II's `TypeMatchups`
  starts with Normal on Rock. The read checks the chart's Water-on-Fire cell is 2x, whatever its place in the table.
- **2026-11-14, step 4**: Crystal Legacy lays out two more ROM tables its own way, which the plan didn't foresee; the
  user chose to read them (rather than leave its trainers unread). Its `TrainerGroups` has a bank and a pointer a class
  (`dba`), and its party types have four more bits, each a field before the item and the moves: a nickname (a byte,
  then a string), two DVs, five words of stat experience, happiness (`engine/battle/read_trainer_party.asm`
  `ReadTrainerPartyPieces`; 9 of its trainers use them). Its Rock Smash sets are a table of their own, `RockSmashMons`,
  from 0 (`engine/events/treemons.asm`). `rom2.cpp` reads both by data-file keys, `rom.trainerGroupsFar` (with
  `sizes.trainerNickname`, `trainerDvs`, `trainerStatExp`, `trainerHappiness` from its
  `constants/trainer_data_constants.asm`) and `rom.rockSmashMons`, as it reads `dexEntryFar`; a game without them reads
  as before. The extra fields are passed over, so a foe's preview takes the default DVs. `gbpk-units` checks the three
  layouts on the made-up ROM laid out Crystal Legacy's way.
- **2026-11-14, step 4**: Crystal Legacy's far Pokédex table keeps Crystal's symbol, `PokedexDataPointerTable` (the plan
  said `DexEntryPointers`); `crystal-legacy` has it under `rom.dexEntryFar` and no `dexEntryPointers` or
  `dexEntryBanks`. PokeDaisy's `GEN2_DEX_CRYSTAL_LEGACY` gives its `dexEntryPointers` with `dexEntryFar = true`, which
  `pokedaisy-extract.py` takes as `dexEntryFar` (its `dexEntryBanks` a placeholder).
- **2026-11-14, step 4**: Crystal Legacy V1.3.1 is written for rgbds 0.5.2, whose syntax `pret-extract.py`'s constants
  evaluator didn't read: `NAME EQU value` and `name: MACRO` without `DEF`, and a structure's `rsreset`/`rb`/`rw`. It
  reads them now; pret's four give the same output.
- **2026-11-14, step 4**: `kiwami/tests/gbpk-ui.sh` added to step 4's files (its tests named it): with Crystal
  Legacy's data file, the made-up Crystal ROM is "Crystal or Crystal Legacy", so the step 1 run that copied a made-up
  second data file now runs on the real folder. `kiwami/tests/gbpk/NOTICE` names the hacks' captures. `gbpk-roms.cpp`
  reads the subfolders as `gbpk-roms.sh` does, and checks a hack's chart by being read whole (its entries counted),
  not against the retail chart.
- **2026-11-14, step 5**: `crystal-legacy.ram.json` added to step 5's files. Its guide reads the save's flags, so the
  data file gains them as Crystal's has them: `flags` (its 256 bytes of event flags), the Elite Four's four derived
  bits (2048-2051; Red is done by his own flag, so no fifth) and `tradeFlagBytes`, with its `CL_*` RAM constants
  checked against its build's symbols. `gbpk-roms.cpp` also compares Crystal Legacy's hidden items with PokeDaisy's
  (`GuideCrystalLegacyGen.kt`'s 85 HIDDEN lines), as it does Crystal's: a check the step didn't list.
- **2026-11-14, step 6**: Yellow Legacy's ROM has two records the readers didn't take, so `rom1.cpp` changes (the plan
  expected none), as the user chose. Its Metapod's evolutions hold a stray `db 7, HARDEN` before their end
  (`data/pokemon/evos_moves.asm`, its only one), which made the reader refuse the ROM: an entry of a kind Gen I hasn't
  now ends the evolutions (the game never evolves by it, `.evoEntryLoop`), and the moves start after the record's first
  0 (as `.skipEvolutionDataLoop` finds them; retail records hold no 0 there, so they read the same). Its Old Rod is a
  table, `OldRodMons` (two of level and species, at even chances as `GoodRodMons`), not retail's Magikarp in
  `ItemUseOldRod`'s code, which the reader found nowhere and so listed no Old Rod: the data file gives `rom.oldRodMons`
  instead of `rom.oldRod`, exactly one of the two. `SpecialTrainerMoves`' bound grows from 1 KB to its bank (Yellow
  Legacy's is 2 KB). Its five renamed symbols get lines of their own (`STARTER_PIKACHU`, `GBCBasePalettes`,
  `wMissableObjectFlags`, `HS_CERULEAN_CAVE_GUY` in `hide_show_constants.asm`, `HiddenObjectMaps`). `gbpk-units` checks
  both layouts on the made-up Gen I ROM; `gbpk-roms.cpp` checks Yellow Legacy's own values from its source (Kadabra at
  42, the rods, Brock's and Lorelei's moves, Lick special) and its hidden items against PokeDaisy's `G1L_AREAS_RAW`.
  `kiwami/tests/gbpk/NOTICE` names its captures. `gbpk-ui.sh` also asserts Crystal Legacy's guide loaded, which step 5
  left out (its comment still said the guide was to come).
- **2026-11-14, step 7**: `yellow-legacy.ram.json` added to step 7's files, as step 5 added Crystal Legacy's: its guide
  reads the save's flags, so the data file gains Yellow's banks (`flags`: 320 bytes of event flags and the 32 of its
  toggleable objects, at its own `0xD5F3`), the five derived bits 2816-2820 and `toggleFlagBytes`, its `G1L_*` RAM
  constants checked against its build's symbols. `kiwami/tests/gbpk-ui.sh` added too: its Yellow Legacy run said no
  guide was there, and now asserts the guide's 13 milestones. `gbpk-roms.cpp` needs no change: its `checkGuide` loads
  any game's guide file there is and checks every place and boss's team against the ROM, and already compares Yellow
  Legacy's hidden items with `G1L_AREAS_RAW` (step 6). `GEN1_GUIDES` gets a fourth field, what the game's tables were
  generated from, for the guide file's `sources` (the retail games' files come out unchanged).
- **2026-11-15, step 8**: the render check needs a stand-in species of the fork's types, and the stand-in (every
  species Normal) is made in `ui/src/kiwami/gbpk/game.cpp`, not one of the step's files. The user chose a headless
  verb: `gbtypes <species> <type> [<type>]` (`game.cpp`, documented in `game.h`), written before `gbpanel`, gives
  that species those types in the stand-ins made after it. The type names go through a new `typeByName` (`games.h`),
  which `loadData` uses too. `gbpk-ui.sh` counts each chip's colour in the Party page's shot (more than 200 pixels with
  the types, none in Red's own shot) instead of reading a pixel at the chip's place, which differs between the sizes
  and moves with upstream's layout (the user's choice). The one-off `-fsanitize=address,undefined` build of
  `gbpk-units` is made without `-Werror`: at `-O1` GCC's `-Wformat-truncation` warns on `rom1.cpp:223`'s and
  `rom2.cpp:206`'s `snprintf` of a TM's name (from #28, bounded by the TM count), which the normal `-O0` build doesn't.
  The run had no PureRGB capture to replay (step 9 brings them): it covers the fork's types through the unit test's
  chart and advice, and every existing capture. `rules.h` ties its chart's index to `gbpk.h`'s types
  (`FORK_TYPES = TYPELESS`, a `static_assert` on the 24 cells) rather than changing it. `docs/FORK.md`'s sentence on
  `model.cpp`'s chart hook said Gen I's chart; it now says the game's own, as step 3 made it.
- **2026-11-15, step 9**: PureRGB lays out more of its ROM its own way than the plan foresaw; the user chose to read it
  all (the reader refuses a ROM whose tables it can't read, so most of it had to be). `rom1.cpp` reads each by its
  data-file key, a retail game reading as before:
  - its forms' base data is a table of its own, `NonDexMonsBaseStats` in `NonDexPokemonSpecies`' order
    (`rom.nonDexBaseData`, `rom.nonDexSpecies`), not `BaseStats`, and its real MissingNo. is in that list with no
    number: it fights in its Champion's Arena (the gym guide's sixth) and is left out of a trainer's team;
  - its `NUM_POKEMON` counts MissingNo., so `numPokemon` is its `DEX_MEW`;
  - move and item names are pointer tables (`rom.moveNameTable`, `rom.itemNameTable`);
  - its parties have two kinds more, `$FE` and `$FD` (a moveset from `MoveSetMappings`, `rom.trainerMoveSets`), its
    `LoneMoves` are 3 bytes and its `TeamMoves` go to the sixth member's second move (`rom.loneMoveSlots`,
    `rom.teamMoveSixth`);
  - priority is a list, `PriorityMoves` (`rom.priorityMoves`), and no move is -1, so the `const.quickAttack` and
    `const.counter` lines leave it out;
  - its Old Rod is two in its code (read by its bytes, as retail's), its Good Rod a list of four and another on its
    ocean maps (`rom.goodRodList`, `rom.goodRodOcean`, `rom.oceanMaps`);
  - its indoor town map entries are 5 bytes (`rom.internalMapEntriesWild`);
  - its hidden items of two, three and five call functions of their own (`rom.hiddenItems2`, `3`, `5`).

  `gbpk-units` checks each on the made-up Gen I ROM; `gbpk-roms.cpp` checks PureRGB's own values from its source. The
  user also chose upstream's way for the forms' types: upstream takes a Pokémon's types from its species on the Party
  page and in its logic (a form only picks the picture), so a form shows its own types on the Battle page (the
  battle's copy in RAM) and its own stats everywhere, and its base's types on the Party and Team pages and in the
  advice; no hook is added, and A6 says so. The scripts: one game's `build` lines may name several checkouts (a
  version each); its constants must agree in all of them, and where its builds disagree on an address each release
  has its own in the data file (`games.cpp` `pickRelease` lays it over the game's; `loadData` refuses a release whose
  own addresses aren't every release's or are also the game's). The headless stand-in picks a capture's release by
  its README's SHA-1 (`captureRelease`). A `const` line may name several files joined by `+` (PureRGB's base data's
  constants need its battle and item constants'); the constants evaluator reads a decimal with a leading 0 and passes
  over a string's `EQUS` (its `macros/scripts/text.asm`, for `TX_FAR_END`). The Hyper Ball, a ball Gen IV hasn't, is
  `item HYPER_BALL 0 ball`. PokeDaisy reads no PureGreen and no v2.7.7: `pokedaisy-extract.py` checks GameTitles.kt's
  SHA-1s against v2.7.6's (`PD_RELEASES`) and its configs against that release's addresses. The character set is a
  data-file flag (`GameData::pureText`, `charset()`), which `RomView` and the reader take; `rom.cpp` changes for it and
  for a form's picture (`GbRomData::sprite`), and `pics.cpp` doesn't. `kiwami/tests/gbpk/NOTICE` names the captures.

- **2026-11-15, step 10**: upstream's `damage()` gets a `Fighter` (the national number, no form) and a `MoveData` (no
  move id), so three of PureRGB's rules are matched another way than its code's internal ids, as the user chose:
  - the signature moves go by the species' national number. Volcanic Magmar is Magmar's number, so the plan's
    `const.volcanicMagmar` and `const.magmar` lines aren't needed; no other form can use its base's signature (Spirit
    Irradiated, a Hitmonchan, knows Growth, Sludge, Struggle and Egg Bomb only). The move is matched by its ROM name
    (`Signature::move`), and the ROM tests check each signature's name is one move's only. The accuracy is read but
    reaches nothing, as upstream's logic takes no accuracy (Hypnosis, Fire Blast, Blizzard);
  - Ghost by the attacker's base stats goes by its national number (`GameRules::statsSpecial`, generalised from the
    plan's `ghostSpecial` to the data file's `split_stats`, `byStats`): of the twelve forms only Winter Dragonair splits
    otherwise than its base (Special 130, Dragonair's 70), so its Barrage is judged physical where the game makes it
    special. `kiwami/GB-POKEMON.md` lists it with what isn't followed.

  Also: the Safari Ball's factor is 8 in PureRGB's `ItemUseBall` too, the same function as the Hyper Ball's, and the
  rules follow it (the user's choice). A form is never switched back at the type NPC (the game compares the internal
  index; none of the 23 has a form), not "its forms' too" as the step said. An options change applies from the next
  set and is logged, without restarting the Session (only a change of the switched species restarts it, for its copy
  of the ROM's data); the log also says when the signature moves are off. `GbRomData` holds the game's `GameRules`
  (`rules`, its chart among them) instead of the chart alone, so `rom2.cpp`'s chart read and the tests' three uses
  name `rules.chart`; `rom.cpp`'s `readRom` sets the rules the data file names (the by-stats types, the options, the
  Hyper Ball) and has `withOriginalTypes`. The reader's flag byte is read by `const.signaturesOff` over
  `ram.eventFlags`. `gbpk-units` also checks the read refuses a bad entry in either list, and the retail games have
  none of PureRGB's rules; `gbpk-roms.cpp` checks Fire Punch and Hypnosis besides Drill Peck, and Gengar's and Machamp's
  split.

- **2026-11-15, step 11**: `G1R_BOSSES_RAW` has no trainer class or party number (only names, flags and teams; Red's
  guide finds its bosses by name and top level), and PureRGB's boss classes are two lower than Red's, each with
  Champion's Arena rematch parties after the story's. As the user chose, each boss's class is PureRGB's constant (the
  leaders' `leader<Name>`, new `const.class.<NAME>` lines for the Elite Four and `RIVAL3`), and its party a table in
  `pokedaisy-extract.py` (`GEN1_BOSS_PARTIES`, from the maps' `object_event`s and the Champion's script: leaders 1,
  Giovanni 3, the Elite Four 1, the Champion 1-3). `gbpk-roms.cpp` checks it both ways: in PureRGB's ROMs each id is
  a trainer of the boss's name (letters only: "LT.SURGE"; the Champion's class has no name there, `li ""`), and in
  Red's and Blue's the same party numbers are the ones their guides find, and PureRGB's own levels (Brock 14,
  Giovanni 64, Lorelei 66, Lance 69, the Champion 72).

  The flags' names are five hand-written `const.flag.<NAME>` lines (the user's choice), not a block the conversion
  writes: the conversion maps each of Red's milestone flags through Red's line to PureRGB's number, and stops on a flag
  without a line. The Cerulean Cave guard goes through each game's `toggleCeruleanCaveGuy` (PureRGB's 9, Red's 8), and
  PureRGB's toggleable objects are Red's 32 bytes (the same `flag_array $100`). Dotted const keys stay out of the data
  files' `sizes`. The three `.ram.json` gain the guide's flag banks, derived bits, `toggleFlagBytes` and
  `"guide_levels": "rom"`, which `games.h`/`games.cpp` read into `GameData::romLevels` (as steps 5 and 7 added the
  Legacy hacks' banks). The levels are set by `romLevels` in `gsguide.h`/`gsguide.cpp`, which `host.cpp`'s
  `guideFor` calls, as `gbpk-units` doesn't link `host.cpp` (the user's choice). The guides take no trades: PureRGB's
  in-game trades are all its own (`data/events/trades.asm`), so Red's notes would be wrong. `gbpk-units`' PureRGB
  captures now give the guide's flags (353 bytes, where they gave none), and `gbpk-ui.sh`'s PureRed run asserts its
  guide's 13 milestones instead of "no guide". `gbpk-roms.cpp`'s comparison of the hidden items with PokeDaisy's areas
  is limited to the four games PokeDaisy has them for: with a PokeDaisy checkout it compared PureRGB's with Yellow
  Legacy's (step 9's, unseen as the ROM tests had run without the checkout).

- **2026-11-15, step 12**: Crystal's code ports 46 of Crystal Clear's 49 RAM symbols by its votes, but about half of
  its 39 ROM tables: the hack moved its wild Pokémon, fishing, tree and rock tables with their code to bank 0x11, made
  its trainer groups a bank and a pointer, and rewrote the code around others. One single vote was wrong
  (`MapGroupPointers` at 40:4000), and several constants are its own. As the user chose, each value has a method,
  named by a line of its own in `gbpk-symbols.txt` where it isn't the votes (two at least, strays under an eighth):
  - `port.data`: Crystal's bytes, found once;
  - `port.code`: a pattern of the build's bytes, found once. Where it is also found in Crystal, it must give Crystal's
    value there (28 of the 47 are);
  - `port.count`: a table's length to its lowest pointer (the landmarks: 96 on Crystal, 117 here);
  - `port.same`: Crystal's constant, its table found whole by `port.data`;
  - `items|types from crystal`: by the item names in each ROM, and the whole type chart.

  Every value is found in all eight builds, the four of a version agree, and every address PokeDaisy also has agrees
  with it. `pret-extract.py` changes too (not in the step's files): it passes over the lines of a game with port lines,
  and `SOURCES`' header names the three scripts (`gbpk-extra.json` is unchanged). `--self-test` checks the votes,
  a pattern and a count on made-up ROMs. Besides Crystal's keys, the port gives the keys steps 13 and 14 need, found
  in the hack's own code: `rom.landmarkRegions` and `const.landmarkRegionsFirst` (RegionCheck's stand-ins, from
  landmark 96), `rom.textDict` and `const.textDictFirst` (PlaceNextChar's table, from byte 0x16), and the second
  block of event flags (as the user chose: below). Found for step 13, which reads them:
  - **Event flags**: `EventFlagAction` keeps flags from 2048 (`const.numEvents`, its `cp 8`) in a second block,
    `ram.eventFlags2` (D6F6), which runs at most to `ram.eventFlags2End` (`wMapObjects`, D71E: the 40 bytes Crystal
    leaves after `wCmdQueue`). The data file's `flags` gets it as a second block.
  - **`wPlayerGender`** (D472) holds 0-2, a player of three, not Crystal's bit.
  - **Fishing**: `FishGroups`' entries are 6 bytes (Crystal's 7), and its `Fish` routine is its own.
  - **Trees**: `TreeMons` counts its sets from 1 (19 of them; Crystal's 8 from 0).
  - **Trainers**: `rom.trainerGroupsFar` (a bank and a pointer a class, as Crystal Legacy's), and 105 classes. Its party
    types are still to be read.
  - **Palettes**: `GetMonPalettePointer` picks between two tables by bit 2 of D9C4 (an option). `rom.monPalettes` is
    02:6A2C, Crystal's own bytes (PokeDaisy's); the hack's own, 02:622C, is the one used while the bit is clear.
  - **Versions**: the evolutions' pointers differ between v2.5.11 and v2.6.0 too, not only the move descriptions.
    Its base data isn't Crystal's byte for byte, as PokeDaisy says: the held items and TM bits differ.

- **2026-11-15, step 13**: Crystal Clear's ROM had more of its own than step 12 had found; the user chose each way
  below (2026-11-15).
  - **Its trainers scale with the badges**: 386 of its 538 trainers have a party for each badge count, 0-15, each
    with a type of its own, ended by -2 and the last by -1 (`ReadTrainerParty`, 0E:4000: `CountSetBits` over both
    badge bytes, 16 taken as 15); a member's fields are its item, a nickname and its moves, in that order. As the
    user chose, every party is read (`GbRomData::teams`, the first as the trainer's team), and the host gives the
    pages the parties for the save's badge count: a copy of the ROM's data when the count changes (`withBadgeTeams`,
    `badgeTeam` in `rom.h`), the Session started again on it, as PureRGB's switched species (`host.h`, `host.cpp`:
    not in the step's files). The Arena's choice of level (WRAM DC19), which the game takes there, isn't followed
    (`kiwami/GB-POKEMON.md`, *What it doesn't do*). The game finds a trainer by counting the -1s before it, and class
    39's eleventh record has no name: it is passed over, the numbers after it kept. A trainer's name is now read
    once its record has read whole (garbage past the last group in a bank had logged an unknown routine).
  - **Its layouts with no byte to capture**, as the user chose, come from port lines (step 12's files change:
    `gbpk-port.py`, `gbpk-symbols.txt`, `gbpk-port.json`, `SOURCES`' `[port]` and `[pret]`): a new method,
    `port.fixed const.<key> <value> <pattern>`, keeps the line's value only if the pattern of the code is found once
    in each build (`--self-test` checks it). `fishGroupSize` 6 (three rods' pointers, no bite chance), `treeMonsFirst`
    1, `trainerTeams` 2, `trainerItem` 4, `trainerNickname` 8, `trainerMoves` 16; `fishChances` 100 is a `port.code`
    `{byte}` (its `RandomRange`'s). `rom2.cpp` reads each with Crystal's as the default.
  - **Its palettes**: the game uses its own table (02:622C) while an option's bit (D9C4 bit 2) is clear, as in all
    six captures (a new game's); 02:6A2C, which step 12 and PokeDaisy took, is Crystal's colours. As the user chose,
    `rom.monPalettes` is the hack's own (its `port.code` line changed), and `pokedaisy-extract.py` leaves PokeDaisy's
    `palettes` out for it, saying why. The Color Filter builds' palettes are all filtered, so `rom2.cpp`'s check of
    the first entry is now that its normal and shiny colours are the same and not black (true of every game), rather
    than Crystal's bytes.
  - **Smaller**: landmark 116, Origin Plains, reads the code after its table of stand-ins (0xC5, as PokeDaisy notes):
    it is on no map, and `gbpk-roms.cpp`'s map check names it. Natu's and Teddiursa's categories are the hack's own
    ("LITTLEBIRD", "LITTLEBEAR"); every other description and page decodes as Crystal's. The data file has no
    `"charset": "gen2"` or `"text_dict"` field as the step listed: the dictionary is `rom.textDict` and
    `textDictFirst`, the character set Gen II's by default. `pokedaisy-extract.py` reads a `Gen2DexTables` made by
    position from another (`.let`, v2.5.11's), and checks PokeDaisy's `coords` against `ram.yCoord` and its `textDict`
    against the port's; GameTitles.kt's four SHA-1s must be among the eight. `kiwami/tests/gbpk/NOTICE` names the
    captures.
  - **Step 14** builds the guide "each region's leaders by level": with the parties by badges, every leader's levels
    follow the save's badge count, so the order by level is to be settled there.

- **2026-11-15, step 14**: Crystal's constants don't give Crystal Clear's classes: it removed Crystal's first rival's
  class (9), so from Will on each class is one lower (Will 10, Brock 16, Blue 59; Crystal's 11, 17, 64), and the
  plan's check, the class name at Crystal's index, passes on the wrong leader (class 17 is Misty's, a Leader too). It
  also has a post-game Champion class (71, its trainers at 70, 90 and 100) and Elite 4 class (72-75), and post-game
  classes of their own names (100-105) whose first trainers are Will, Bruno, Karen, Koga and Lance again. As the user
  chose (2026-11-15):
  - **The classes are ported by name**: a new `gbpk-port.py` method, `port.trainer const.<key> rom.<groups> <base's
    groups>`, gives the build's class whose name (`TrainerClassNames`) and first trainer's name are Crystal's class's,
    compared in the ROMs, found in one class (`--self-test` checks it, and a class two of the build's are fails). The
    sixteen leaders, `eliteWill` .. `eliteKoga` and `champion` are ported so. Its badges' bits are Crystal's by
    `port.same` over `rom.engineFlags` (Crystal's `EngineFlags`, 486 bytes, found whole in every build). The guide's
    `const.leader*`/`badge*` lines and the new `eliteWill` .. `champion` lines name `crystal` (the port's base) and
    `crystal-clear` besides Gold and Silver, so `gbpk-extra.json` and the four data files are regenerated
    (`crystal.ram.json` gains the constants unused).
  - **The Elite Four and the Champion go by the data file's classes** in every ROM guide, not by the class names, so
    Crystal Clear's post-game ones are left out; Gold's and Silver's guides come out the same (the ROM tests).
  - **The order**: the data file says `"guide": "rom-regions"` (not `"rom"`: `games.h` `guideRegions`, and an unknown
    value refused). The first region is Kanto's when the save has more of Kanto's badges than of Johto's (the order
    they were won in isn't kept), and its leaders go by their badges' order, as every leader's party is at about the
    same level for a badge count (9 to 75), so level doesn't order them; the Elite Four (42-47) and Lance (50) are
    fixed, about eight badges' level, after the first region.
  - **The levels rise from the save**: each gym not yet beaten shows its party for the save's badge count and one more
    for each gym not yet beaten before it; a beaten one the count's. `romGuide` takes the save's badges, and the host
    makes the guide again with the parties when the badge count changes (`host.cpp` `badgeTeams`), before the
    Session starts again on it.
  - **A gym's id is its badge's** in this order (`gym-9-brock`), so it doesn't change when the save's region reorders
    the road; Gold's and Silver's keep theirs.
  - The ROM test's "each leader's level ordered within its region" is over the gyms not yet beaten: a beaten gym shows
    the count's party, as the next one does, and leaders differ by a level or two at a count (Brock 17, Misty 16).
    `gbpk-ui.sh`'s Crystal Clear run now asserts its guide made from the ROM (0 milestones on the stand-in, which has
    no leaders) instead of "no guide".

- **2026-11-15, step 15**: `docs/FORK.md` added to the step's files (the user's choice): its table of the fork's places
  names `gbpk-port.py` among the scripts that make committed data, and its note on `guide-games.inc` says the rows are
  the six games' and the ROM hacks'. `CLAUDE.md`'s companion line names `gbpk-port.py` with the other two scripts too,
  and is wrapped at 120 columns (it was over). The weight: banana-ui before is `105315b`'s `ui/` built from a `git
  archive` copy (no worktree), with the same `make` as the branch's; the squashfs figures are the folder alone with the
  image's options and `-nopad`. The code grew 134 KB, a third over *Risks*' expected 100 KB; the data 147 KB, under its
  150 KB. Polished Crystal and Prism have no issues yet, so *What it doesn't do* names them as later features, without
  numbers. The tests' ROM table loses Crystal Legacy's row: *The ROM hacks* lists the sixteen releases with their
  SHA-1s.

## Progress

A step is marked done in its own commit, so the commit column holds the subject (a commit can't carry its own hash).

| Step | Status | Commit |
|---|---|---|
| 1. Several games to one header title | done | GB Pokémon hacks: several games to one header title, the SHA-1 picks one |
| 2. Each game's items, types and split | done | GB Pokémon hacks: each game's items, types and physical/special split from its data file |
| 3. Each game's type chart from its ROM | done | GB Pokémon hacks: each game's type chart read from its ROM |
| 4. Crystal Legacy | done | GB Pokémon hacks: Crystal Legacy, from its build's symbols, its memory and its ROM |
| 5. Crystal Legacy's guide | done | GB Pokémon hacks: Crystal Legacy's guide, from PokeDaisy's |
| 6. Yellow Legacy | done | GB Pokémon hacks: Yellow Legacy, from its build's symbols, its memory and its ROM |
| 7. Yellow Legacy's guide | done | GB Pokémon hacks: Yellow Legacy's guide, from PokeDaisy's |
| 8. Types of the fork's own on the pages | done | GB Pokémon hacks: types of the fork's own on the pages, for PureRGB's six |
| 9. PureRed, PureBlue and PureGreen | done | GB Pokémon hacks: PureRed, PureBlue and PureGreen, from their builds' symbols, their memory and their ROMs |
| 10. PureRGB's switches, signatures and Hyper Ball | done | GB Pokémon hacks: PureRGB's switched types and matchups, signature moves and Hyper Ball |
| 11. PureRGB's guides | done | GB Pokémon hacks: PureRGB's guides, Red's and Blue's with its flags and its levels |
| 12. Crystal Clear's addresses, ported | done | GB Pokémon hacks: Crystal Clear's addresses, mapped from pokecrystal's by the code that uses them |
| 13. Crystal Clear | done | GB Pokémon hacks: Crystal Clear, its packed text, its memory and its ROM |
| 14. Crystal Clear's guide | done | GB Pokémon hacks: Crystal Clear's guide, built from its ROM by region |
| 15. The documentation and the weight | done | GB Pokémon hacks: the documentation, and what the hacks weigh |
| 16. Final check | todo | |
