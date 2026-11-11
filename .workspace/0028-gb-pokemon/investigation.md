# 0028 Pokémon companion for the Game Boy and Game Boy Color: investigation

Issue: #28 · Branch: `k/0028-gb-pokemon` · Status: refined · Confidence: 90% (proceed)

## The request

The game panel's Pokémon companion follows the Game Boy Advance Pokémon games with the DS companion's pages (party,
battle advice, team, Here, Map, bag, Pokédex, guide, journey log). The request is the same depth for the Game Boy and
Game Boy Color games, on the panel the game isn't on. The user also asked whether the companion takes its information
from PokeDaisy (`/home/mih/Documents/Projects/pokedaisy/`, https://github.com/lidor30/pokedaisy).

**PokeDaisy, the answer.** Banana's companion takes PokeDaisy's *idea* and none of its code or data. Upstream's reader
says so (`ui/src/pokemon.h:6-9`: "None of its code is here (it is GPL-3.0, Banana MIT): the layouts below are from the
games' public documentation and decompilations"), and so do the credits (`system/rootfs/etc/banana/credits.txt:40`,
`README.md:118`, `README.md:381`: "BananaOS's is its own code, written from the games' documented data"). Upstream reads
every name, sprite, table and encounter from the player's ROM at run time, and writes the guides "from facts in our
own words, never copied" (`docs/COMPANIONS.md`, *DS Pokémon guide data*). The fork's earlier plan to run PokeDaisy's C
core as a second-screen app (#17, `.workspace/0017-second-screen/`) was never carried out. Upstream built its own
companion instead.

Decided with the user:

- **Games**: the English retail releases PokeDaisy supports: Red, Blue, Yellow (USA/Europe), Gold, Silver
  (USA/Europe), Crystal (USA rev 0, USA/Europe rev 1, Australia).
- **Fork only**: lives on `kiwami`, not offered upstream. The fork's code goes in `ui/src/kiwami/`, and upstream's files
  get only marked `kiwami:` hooks.
- **PokeDaisy's data is extracted**: RAM addresses, ROM table locations, flag ids, the guide tables' structure (which
  flag marks which boss beaten, which hidden item is where, by id), and PokeDaisy's own hand-written text (FAQ answers,
  its item descriptions). The games' own content is left out: names and text the games contain, and tables copied out
  of the ROMs (wild encounters, trainer teams, base stats). The fork is public, and upstream's rule is that nothing of
  the games is in Banana; banana-ui reads that content from the player's ROM (oversight 11). The extracted data goes
  into JSON and markdown files in the fork under PokeDaisy's GPL-3.0, with its required attribution.
- **The C++ may follow PokeDaisy's Kotlin readers freely**, so the GB companion's sources are GPL-3.0, derived from
  PokeDaisy. They carry its license and attribution, and the fork's banana-ui binary, which links them, is a GPL-3.0
  work as a whole. Banana's MIT code is compatible with that (oversight 2).
- **Game data comes from the player's ROM at run time**, as upstream does it: names, base stats, moves, learnsets,
  evolutions, items, wild encounters, trainers, sprites, Pokédex text, town maps. PokeDaisy's extracted tables are kept
  as reference data. The tests check the ROM reader against the facts kept, and against more when ROMs are given
  (oversight 11).
- **Guides now**: PokeDaisy's tables for Red, Blue, Yellow and Crystal become guide files in Banana's guide schema.
  Gold and Silver get a guide built from the ROM's trainer table at run time, by the fork's code.
- **Gen-aware pages**: hooks in upstream's `ui/src/nds/pages.cpp` so these games show what they have (DVs, stat
  experience, Special) and not what they don't (abilities, natures, IVs/EVs).
- **Settings**: one fork row per system, `Pokémon companion: Game Boy` and `Pokémon companion: Game Boy Color`, each off
  by default. Upstream's GBA row stays as it is. Challenge mode follows upstream's `gba_challenge` row.
- **Tests**: PokeDaisy's RAM captures of the six games go into `kiwami/tests/`, under its license.
- **The Map page** shows the game's own map decoded from the ROM: Gen I's town map, Gen II's Pokégear Johto and Kanto
  maps. Squares are used only when a map can't be decoded.
- **PokeDaisy's data for every mainline game it supports** is extracted too, English releases only, as reference data:
  Red/Blue/Yellow, Gold/Silver/Crystal, Ruby/Sapphire/Emerald, FireRed/LeafGreen, Platinum, HeartGold/SoulSilver,
  Black/White, Black 2/White 2 (PokeDaisy has no Diamond/Pearl). It isn't installed in the image and isn't read by
  banana-ui. Upstream's GBA and DS companions are unchanged. The same extraction script as the GB data writes it, in
  this feature.
- **The attribution stays in the fork**: its documentation and the credits screen. No line in upstream's `README.md`.

Out of scope, each a later feature: the European Yellows (DE/FR/IT/ES), the ROM hacks PokeDaisy supports (PureRed/
PureBlue, Yellow Legacy, Crystal Legacy, Crystal Clear, Prism, Polished Crystal), other languages, and PokeDaisy's
features beyond the companion (HUD, quick menu, battle buttons, widescreen, writing to the game's memory).

## Oversights in the requirements

1. **Upstream owns every piece this builds on, and changes it daily.** The reader (`ui/src/pokemon.*`, 1,781 + 340
   lines), the GBA host and panel (`ui/src/gba/`), and the DS model, logic, pages, session and guide loader
   (`ui/src/nds/`) are all upstream's: 143 upstream commits touch them, most since 24 Oct 2026. `git diff main...kiwami`
   shows no fork change in any of them. Upstream commits since 11 Oct: `main.cpp` 158, `settings.cpp` 276, `settings.h`
   122, `pages.cpp` 34, `model.h` 16, `logic.cpp` 9, `settings_gamepanel.cpp` 4, `gba/panel.cpp` 3.
   - The hooks this needs sit in the busiest files. `pages.cpp`'s Gen-aware hooks are the largest of them.
   - Upstream could add GB/GBC itself; it went from the GBA to the DS in about two weeks. Then the fork's work would
     duplicate it.

   *Proposed*: keep every hook one line calling into `ui/src/kiwami/`, and have the plan count them. The
   `banana-merge-upstream` checks would watch `ui/src/nds/`, `ui/src/gba/` and `ui/src/pokemon.*`. If upstream ships a
   GB companion, retire the fork's.

2. **PokeDaisy's 7(b) term asks for more than "the documentation for that part".** PokeDaisy's `NOTICE` requires the
   line "Based on PokeDaisy by Lidor Itzhari - https://github.com/lidor30/pokedaisy" in three places: (1) a NOTICE
   kept with the source, (2) the work's README or equivalent documentation, and (3) where the work shows credits or an
   About screen. Banana's credits today say "inspired by" (`credits.txt:40`), which fits upstream's code but not GPL
   data the fork ships.
   - *Decided*: the extracted files sit in their own folders with PokeDaisy's `LICENSE` and a `NOTICE`.
   - *Decided* (the user: "read Kotlin freely"): the GB companion's C++ is written with PokeDaisy's readers as its
     model, so it is GPL-3.0. Each of its files carries a GPL-3.0 header and the 7(b) line, and the files sit apart
     from the fork's MIT files (their own names or subfolder in `ui/src/kiwami/`).
     - The fork's banana-ui links them, so the binary as built is GPL-3.0 as a whole. It must stay offered with its
       source (the fork's repository is public) and can't go upstream as it is.
     - `kiwami/DEPENDENCIES.md` and the fork's documentation say so. This is the consequence of the choice, and it
       stops a later contribution of this code to MIT upstream.
   - *Decided*: `kiwami/post-build.sh` appends the line to the image's `credits.txt`, which Settings > About shows,
     and the fork's documentation carries it as the "equivalent documentation". Upstream's `README.md` is left alone
     (*Open questions*, 1).
   - With the code GPL anyway, whether the per-game addresses are compiled in or loaded from the data files at run
     time is no longer a license question. *Proposed*: load them from the data files, so the extraction script stays
     the one place they come from, and a later hack or language is a data file. The plan decides.

3. **The six games' headers don't tell them apart from what's out of scope.** The European Yellows carry the same
   title as English Yellow but keep their RAM 5 bytes later from `wBattleMon` up (PokeDaisy `GuideGen1.kt:12-13`). ROM
   hacks keep their base game's header (Crystal Legacy, Crystal Clear and Polished Crystal are pokecrystal forks).
   Matched by title alone, any of them would read garbage.
   - *Proposed*: tell each release by the whole ROM's CRC-32 (the list from PokeDaisy's checked SHA-1s and No-Intro).
     A known title with an unknown CRC shows "Not supported" alone, as upstream does for an unlisted GBA hack (upstream
     97433b41).

4. **Gen I/II rules differ from what `nds/logic.cpp` knows (Gen III-V).** Each of these would give wrong advice if
   left to the existing logic:
   - Gen I's type chart differs from Gen II-V's: Ghost on Psychic does nothing, Bug and Poison are super effective on
     each other, Ice on Fire is neutral. There's no Dark, Steel or Fairy. Gen II's chart is Gen III's.
   - Gen I has one Special stat for both attacking and defending.
   - Gen I and Gen II each have their own catch formula (and Gen II its status bug), not Gen III-V's.
   - DVs run 0-15 and stat experience 0-65535, not IVs 0-31 and EVs 0-255.
   - In Gen II, shininess, gender and Hidden Power come from the DVs. Upstream's `Mon::shiny()` works it out from the
     personality value and trainer IDs (`nds/model.cpp:108`), and Hidden Power from IVs (`:110-112`).
   - Gen I has no held items, gender or breeding. Neither generation has abilities or natures.
   - The type-boosting held items add 10%, not 20%.

   The damage formula's shape is Gen III's (`nds/logic.cpp:149-150`), so most of it carries over.
   - *Proposed*: `Gen::I`/`Gen::II` in `nds/model.h:20` (a hook), Gen I's chart beside `useModernTypeChart`
     (`model.cpp:83-100`, a hook), and the catch formulas and the Special/DV/stat-experience rules in the fork's
     code, called from one-line hooks in `logic.cpp`.
   - Shininess can be carried in the data without a hook: the fork sets `pid`/`otId` so upstream's test gives the DVs'
     answer. The plan checks which way is cleaner.

5. **Which system's row turns it on.** `gbc` lists `.gb` files too (`system/rootfs/etc/banana/systems.conf:42`), so Red
   or Gold may live in either folder. The row that applies is the one for the system the game was started from, not
   the cartridge. *Proposed*: say so in both rows' hints.

6. **"Same depth" includes what Banana draws from the ROM**: sprites, the town map and Pokédex text. These are real
   work for GB games:
   - Gen I's front sprites use their own bit-plane compression, Gen II's an LZ scheme, each coloured with the
     species' GBC palette.
   - Gen I's town map and Gen II's Pokégear maps (Johto, Kanto) are tile maps.
   - PokeDaisy reads all of these from the ROM too (`Gen1Dex.kt:3-9`, `Gen2Art.kt:6-12`), so its readers are a guide
     to where things are.

   *Decided*: in scope. The Map page shows the game's own map, as on the GBA (*Open questions*, 2).

10. **The Gen III-V data has no consumer yet.** It is extracted for reference only, so nothing in the image checks
    it, and it can go stale without anyone noticing.
    - It's large: PokeDaisy's English Gen III-V tables are about 30,000 lines of Kotlin, plus `gamedata/gen3/` and
      `gamedata/nds/`. Extracted as JSON, that's several MB in the repository.
    - It's all GPL-3.0, so it needs the same `LICENSE`/`NOTICE` as the GB data.

    *Proposed*: put it in a fork folder that the image doesn't take in, for example `kiwami/data/pokedaisy/<game>/`,
    with a markdown index. The extraction script's check (suggestion 1) keeps it the same as the pinned PokeDaisy
    commit, and `kiwami/DEPENDENCIES.md` names the commit. The GB/GBC games' reference tables (wild encounters, boss
    teams) go there too. Only what banana-ui reads (the GB data files and guides) goes into
    `kiwami/rootfs/usr/share/banana/companions/gb-pokemon/`.

7. **The pages assume Gen III+ in more places than the battle.** The places found:
   - the wild battler's nature, IVs and Hidden Power (`pages.cpp:1468-1520`);
   - the battler card's ability (`:1287`);
   - the summary's nature, ability and item (`:1624-1627`);
   - the stats table's IV, EV and nature arrows, with 31 as the top (`:1660-1680`).

   Gen-aware pages (decided) means hooks at each of these. *Proposed*: the plan finds them all and keeps each hook to a
   call that returns what to show. The pages' private drawing helpers aren't reachable from outside `Pages::Impl`, so
   some hooks will be a line inside upstream's code rather than a call out. That's the main conflict risk.

8. **Gen II's bank switching is not a problem through either core.** Crystal switches the D000-DFFF window to other
   WRAM banks mid-frame (PokeDaisy `Gen2Reader.kt:8-11`).
   - Gambatte's memory map gives 0xD000 as bank 1 always (`gambatte-memory.h:58`: `wramdata(0) + 0x1000`) and banks
     2-7 at 0x10000 (`libretro.cpp:2836-2843`).
   - SameBoy does the same (`libretro/libretro.c:615`).
   - BananaFront resolves addresses exactly as RetroArch does (`bananafront/src/memmap.c:1-4`).

   Upstream's `companions/systems/gbc.json` notes 0xD000 as "the bank mapped now", which neither core does. That's
   harmless here. *No change needed.*

9. **Strings and translations.** Upstream's pages pass labels through `tr()` with `ui/assets/lang/*.tsv`. The fork's new
   labels (DV, Stat exp., Special) have no translations and show in English in every language. *Proposed*: accept it.

11. **The fork is public, and much of PokeDaisy's data is the games' own content.** Several of PokeDaisy's tables are
    generated from the ROMs or pret's builds:
    - names (`Gen4TablesPlatinum.kt`: "Generated by scripts/gen_gen4_tables.py platinum from pret/pokeplatinum");
    - item names, Pokédex and item text in the games' words;
    - wild tables, trainer teams, base stats, gender ratios.

    Committing them would put Nintendo's game data in a public repository, which upstream avoids by reading everything
    from the player's ROM. *Decided*: the extraction keeps facts (addresses, offsets, flag ids, table locations, the
    guide's structure by ids, place and boss names only where the guide schema needs them to find the ROM's own
    records) and PokeDaisy's own words (its FAQ, its hand-written item descriptions such as
    `ItemDescriptionsYellow.kt`: "in our own words"). It leaves out the games' names, text and tables. The tests that
    compare the ROM reader with PokeDaisy's tables then need ROMs (`GB_ROMS`, optional), or compare only the facts kept.

12. **Parsing Kotlin is the extraction script's risk.**
    - PokeDaisy's generated files (`*Gen.kt`) are regular and easy to read.
    - Its hand-written configs (`Gen2Config(...)` with named arguments and defaults, `wramx(1, 0xD892)`, configs built
      on one another) need a small evaluator, or part of them transcribed by hand with the script checking them.
    - For Gen III, PokeDaisy's own `gamedata/gen3/*.toml` is a cleaner source than the Kotlin it generates.

    *Proposed*: the plan sizes this, with the TOML first, the generated Kotlin next, and the hand-written configs by a
    minimal evaluator. Whatever the script can't read is listed in its output, not silently dropped.

13. **Gen I's flags are in four banks.** Upstream's `done_when: flag:N` reads bit N of one array, `Live::flags`
    (`nds/logic.cpp:284`). Gen I keeps event, missable-object, hidden-item and trade flags apart, and PokeDaisy numbers
    them into ranges: 0x10000 + n, 0x20000 + n, and so on (`GuideGen1.kt:20-23`). *Proposed*: the fork's `Live::flags`
    lays the banks out at the same offsets, so the converted guides' `flag:N` keep PokeDaisy's numbers.

## Suggestions

1. **Regenerate the extracted data with a script, not by hand.** A fork script (for example
   `kiwami/tools/pokedaisy-extract.py`) reads PokeDaisy's Kotlin tables at a pinned commit and writes the JSON and
   markdown. The PokeDaisy commit (`135ac809`, 1.5.0.1) goes in `kiwami/DEPENDENCIES.md`. Taking in a newer PokeDaisy is
   then a re-run and a diff. Cost: one script. It's also what makes the later European Yellows and hacks cheap.

2. **Build it Crystal-first, end to end.** Crystal has the most data (held items, gender, time of day, two regions) and
   PokeDaisy's most complete fixtures. Then Gold/Silver (pokegold: the same engine, other addresses), then Gen I. Each
   game's reader can be checked on the device before the next. The plan decides the steps.

3. **Two wrappers instead of a dozen hooks.** Upstream's game panel calls the GBA reader and panel by their concrete
   types:
   - `pkmn::Companion companion` in `main.cpp:2293`, used at `:2560-2576`, `:2593`, `:2607`, `:2671`, `:2724`,
     `:2816-2830`;
   - `gba::Panel gbaPanel` in `settings.h:718`, used about 12 times in `settings_gamepanel.cpp:495-838`.

   A fork class with the same members, dispatching to upstream's object or to the GB one, could replace each
   *declaration* (one hook line each) rather than every use. The plan weighs this against a hook at each use.

4. **Use PokeDaisy's fixtures as headless tests.** The RAM captures have their `README.txt`s saying what's in them.
   For example `crystal`: CHRIS, Chikorita with a Berry, five Poké Balls, on Route 29 at (46, 11). They test the RAM
   reader without a ROM: 33 folders for the six games, 1.1 MB. The pages can then be rendered at 640x480 and 1024x768
   with a made-up `RomData`, as `ui/tests/gba-pages-test.cpp` does for the GBA.

5. **Gold/Silver's ROM-built guide from upstream's removed code.** a103f12e's `guideFromRom` (MIT, removed by
   97433b41) built gyms, Elite Four and Champion from the trainer table in level order. The fork can carry its own
   version for Gold/Silver, whose trainers have class names (LEADER, ELITE FOUR, CHAMPION). Gen I's leaders are
   trainer classes without names, but Gen I has PokeDaisy's guide.

6. **Compare upstream's GBA and DS guides with PokeDaisy's tables later.** With the Gen III-V reference data in the
   repository, a later fork feature (or a report to upstream, as text) could check upstream's guides against
   PokeDaisy's: the gyms' teams, the hidden items' flags, the encounters. This is not part of #28.

Left out on purpose: PokeDaisy's widescreen, HUD, quick menu and battle buttons; its non-English texts
(`YellowText*Gen.kt`, `EmeraldText*Gen.kt`, the ports); the hacks' tables.

## Current state

**The GBA Pokémon companion, end to end (upstream):**

- **Turning on**: when a game starts, `main.cpp:2821` opens the reader only for `sd.id == "gba"`, with
  `pokemon_companion = on` and a second panel. `pkmn::Companion::open` (`pokemon.cpp:1170`) checks the header code,
  then reads the whole ROM on a thread (`readRom`, `:1141-1167`: a file, `.zip` or `.7z`) into `RomTables`. Then a FIFO
  for RetroArch's replies is made (`main.cpp:2825-2830`).
- **Reading**: `Companion::requests` sends a set of `READ_CORE_MEMORY <addr> <len>` lines every 250 ms
  (`pokemon.cpp:1316`), once the last set was answered.
  - `main.cpp:2593` sends them through BananaFront's shared memory (`CompLink`, `complink.h`) when the emulator
    attached, else on RetroArch's stdin.
  - The replies come back through `feed` (`main.cpp:2607`, `:2671`).
- **The pages' data**: `gba::Host` (`gba/host.cpp:22-113`) does three things.
  - It turns the tables into an `nds::RomData` on a thread (`gen3rom`) and loads the guide (`nds::loadGuide`,
    `nds/guide.cpp:298-313`, from `gba-pokemon/<game>.json`).
  - It hands each new snapshot to an `nds::SessionThread` as an `nds::Live` (`gen3live`). The session works out the
    advice, the journey log and the notices on its own thread (`nds/session.h:84-122`).
  - It passes the result to the pages as `PageData`.
- **The pages**: `gba::Panel` (`gba/panel.cpp`) hosts `nds::Pages` on the game panel. It calls the host's `tick`,
  `takeData`, `markSeen`, `ready`, `failed`, `unsupported` and `title`, and the reader's `wantExt`, `active` and
  `snapshot().inBattle` (`panel.cpp:20-90`). Settings routes touches, drawing and the menu/companion switch through
  `gbaPanel` (`settings_gamepanel.cpp:495-838`).
- **Generations**: `nds::Gen { III, IV, V }` (`nds/model.h:20`).
  - The logic branches on it: damage `logic.cpp:149-175`, catch `:230-253`, TM numbers `:341`.
  - The type chart is a global with a Gen VI switch (`model.cpp:83-100`).
  - `GameId` has the GBA and DS games plus `HACK` with a key (`model.h:21`).
  - The guide loader knows its games from a static table (`nds/guide.cpp:13-29`), and its folders from
    `guideDir`/`gbaGuideDir` (`:123-133`).
- **Settings**: Settings > Other systems > Second screen has upstream's rows: Pokémon companion, Challenge mode, Game
  companion (`settings.cpp:640-649`).
  - The fork adds rows through `Kiwami::rows(*this, group)` hooks (`settings.cpp:694`, `:782`, `:890`), but none in
    the Second screen group yet.
  - `Kiwami::place` orders them (`ui/src/kiwami/kiwami.h:10-11`).
- **Tests (upstream)**: `ui/tests/gba-rom-test`, `gba-live-test`, `gba-pages-test`, `guide-test`, `nds-logic-test`,
  `nds-pages-test`. The ROM ones use real ROMs only when `GBA_ROMS` names a folder (`ui/tests/run.sh:38-40`). The
  headless verbs `gamepanel <screen> <rom>` and `pokefeed <file>` (`main.cpp:1115-1160`) drive the GBA companion from a
  file of replies.

**The GB/GBC side today:**

- `gb` and `gbc` run `run-retroarch gambatte` (`systems.conf:39-51`). That goes to BananaFront on Auto
  (`run-retroarch:16`, `BF_SYSTEMS`), or to RetroArch (Hardcore mode, or the Frontend row).
- The fork adds SameBoy as a second core (0006, `kiwami/package/kiwami-libretro-sameboy/`).
- Both cores publish memory maps, so `READ_CORE_MEMORY` reaches WRAM, HRAM, SRAM and the GBC's banks 2-7 (oversight
  8).
- Upstream's generic Game companion already accepts schema-2 definitions for `gb`/`gbc`
  (`companions/systems/gb.json`, `gbc.json`), but none ship. It isn't the depth asked for.

**PokeDaisy (`/home/mih/Documents/Projects/pokedaisy`, commit `135ac809`, 1.5.0.1; GPL-3.0 with a 7(b) attribution
term, `NOTICE`):**

- **Readers**: `Gen1Reader.kt` (393 lines: Red, Blue and Yellow's RAM from pret's pokered/pokeyellow symbols) and
  `Gen2Reader.kt` (636 lines: Gold, Silver and Crystal's, Crystal's bank 1 given as `wramx` addresses). Both live in
  `app/src/main/kotlin/com/pokedaisy/app/companion/data/`.
- **Art and Pokédex, read from the ROM**: `Gen1Art.kt`, `Gen2Art.kt` (fonts, party icons, Pokégear maps), `Gen1Dex.kt`
  and `Gen2Dex.kt` (Pokédex pages, base stats, front sprites with Gen I's compression and GBC palettes).
- **Guide tables, generated from pret's builds** (`scripts/gen_gen1_guide.py`, `scripts/gen_crystal_guide.py`):
  - `GuideGen1Gen.kt` (1,463 lines) and `GuideCrystalGen.kt` (783): per build, the RAM the guide reads (event,
    missable, hidden-item and trade flags, badges), wild tables, bosses (gyms, Elite Four, rivals with teams and the
    flags that mark them beaten), and areas (items, hidden items, trades with their flags).
  - `GuideGen1.kt` and `GuideCrystal.kt` (about 200 lines each): the rules over them and a few FAQ answers.
  - Gold and Silver have no guide.
- **Tables generated from the ROM**: `Gen2SpeciesCrystal.kt` (gender ratios, growth rates, TM items) and item names.
  Banana reads these from the ROM instead (decided).
- **RAM captures**: `app/src/test/resources/fixtures/` (`wram.bin`, `hram.bin`, Crystal's `wramx.bin`). Each has a
  `README.txt` naming the save and what's on screen.

## What has to change

**New, the fork's own** (`ui/src/kiwami/`, names to be fixed in the plan; the build compiles only
`ui/src/kiwami/*.cpp` (`ui/Makefile:18`), so the files sit in that folder, or `ui/src/kiwami/kiwami.mk` adds a
subfolder):

- **Detection**: the six games' releases by CRC-32 from the ROM header and file (`.gb`, `.gbc`, `.zip`, `.7z`, with the
  same reading as upstream's `readRom`). An unknown release of a known title gives "Not supported".
- **The RAM reader**: `READ_CORE_MEMORY` sets every 250 ms with each game's addresses from the GPL data file. It
  decodes the party, the battle (both sides, the foe's party), the bag (Gen I one list; Gen II four pockets and the TM
  case), money, badges (Johto + Kanto), the Pokédex, place and coordinates, play time, time of day (Gen II), the
  daycare, the roamers (Gold/Silver), and the story and trainer flags. It produces an `nds::Live`.
- **The ROM reader**: an `nds::RomData` holding:
  - species (base stats, types, growth, catch rate, gender ratio in Gen II, learnsets, evolutions, TM/HM
    compatibility, Gen I's internal-to-Dex map);
  - moves, items, map and location names, wild encounters (Gen II by time of day), trainers;
  - Pokédex text;
  - `sprite()` (Gen I and Gen II decompression, GBC palettes, the shiny palette in Gen II);
  - the town maps (`RegionMap`);
  - the Gen I/II character set.
- **The Gen I/II rules**: Gen I's type chart, the Special stat, DVs and stat experience, both catch formulas, Gen II
  shininess, gender and Hidden Power, and the 10% type items.
- **A host and a panel** like `gba::Host` and `gba::Panel`, reusing `nds::SessionThread` and `nds::Pages`.
- **Gold/Silver's ROM-built guide** (suggestion 5).
- **Settings rows** `kiwami.pokemon_companion.gb` and `kiwami.pokemon_companion.gbc` (off), placed after upstream's
  Pokémon companion row. Their hints say a game follows the folder it's started from (oversight 5) and that Challenge
  mode applies too.

**Data, the fork's own (GPL-3.0, PokeDaisy's):**

- `kiwami/rootfs/usr/share/banana/companions/gb-pokemon/`:
  - per game, the data file (RAM addresses, flag banks);
  - the guide files `red.json`, `blue.json`, `yellow.json`, `crystal.json` in the `nds-pokemon-guide` schema:
    milestones from PokeDaisy's bosses (foe, level cap from the team, `done_when` `badge:N` or `flag:N`), secrets
    from its hidden items, notes from its FAQ;
  - `LICENSE` (GPL-3.0) and `NOTICE` (the 7(b) line).
- A markdown reference of the GB/GBC structures and the extraction (`docs/GB-POKEMON.md` or beside the data: the plan
  picks), with the attribution.
- The reference data for every mainline game PokeDaisy supports, English releases (oversight 10):
  `kiwami/data/pokedaisy/` (not in the image), with `LICENSE`, `NOTICE` and a markdown index.
- The extraction script (suggestion 1), writing both the image's GB files and the reference data, with a check mode.
- `kiwami/DEPENDENCIES.md`: PokeDaisy's data at `135ac809`, GPL-3.0 + 7(b), where it's used.

**Hooks in upstream's files** (each a marked `kiwami:` line or few; the exact list is the plan's):

- `ui/src/main.cpp`:
  - open the GB companion at launch beside `:2821` for `gb`/`gbc` with its row on;
  - its requests and replies in the game panel loop (`:2593`, `:2607`, `:2671`);
  - its host's creation, attach and teardown (`:2560-2576`, `:2724`);
  - the headless `gamepanel`/`pokefeed` verbs for tests (`:1115-1160`).

  Alternatively, one declaration hook through a wrapper (suggestion 3).
- `ui/src/settings.h` / `ui/src/settings_gamepanel.cpp`: route the game panel to the GB panel (the `gbaPanel` uses), or
  one declaration hook through a wrapper.
- `ui/src/settings.cpp`: `Kiwami::rows(*this, SS)` in the Second screen group.
- `ui/src/nds/model.h` / `model.cpp`: `Gen::I`, `Gen::II`, Gen I's type chart.
- `ui/src/nds/logic.cpp`: the catch formula, Special, DVs and stat experience, the type items.
- `ui/src/nds/pages.cpp`: the Gen-aware fields (oversight 7).
- `ui/src/nds/guide.cpp`: the six games in `GAMES` (`:13-29`) and the `gb-pokemon` folder (`:123-133`, `:312`).
- `kiwami/post-build.sh`: the credits line (not an upstream file).

Why a hook can't be avoided: the game panel's loop, the Settings group, the `Gen` enum, the logic's formulas and the
pages' drawing are upstream code with no extension point.

## Options

- **A. A fork reader feeding upstream's session and pages, with hooks** (chosen). The Gen I/II reader, ROM reader,
  rules, host and panel are the fork's; upstream's `SessionThread`, advice, journey, notices and `Pages` are reused
  through hooks. It gives the same look and features as the GBA companion. The cost is about 15-25 hook lines in busy
  upstream files, and work at each upstream merge.
- **B. Written upstream's way, in `ui/src/nds/` and `ui/src/gba/` directly.** The cleanest code, but it breaks the
  fork's rules and conflicts at every merge, unless offered upstream. Rejected: the user wants it fork only.
- **C. The fork's own pages.** No `pages.cpp` hooks, but the DS look and features would have to be rebuilt. Rejected
  by the user.
- **D. Schema-2 definitions for the generic Game companion.** No C++, but none of the depth: no battle advice, no
  sprites, no guide. Rejected: not what was asked.

## Risks

- **Performance and footprint.**
  - Reads: a set every 250 ms of a few hundred bytes, the same as the GBA. Nothing runs while no GB Pokémon game plays.
  - The ROM: read once on a thread (1-2 MB).
  - Sprites: decoded on demand by the pages (`pages.cpp:287-342`, the decoder thread).
  - Rootfs: the guide and data JSON should be well under the GBA guides' 1.3 MB, and squashfs compresses them.
  - Memory: the ROM kept for look-ups (2 MB at most).
  - The game: no change to what it gets.
- **Conflicts with upstream.** The hooks are in `main.cpp`, `settings.cpp`, `settings.h`, `pages.cpp`, `model.h`,
  `logic.cpp`, `guide.cpp` and `settings_gamepanel.cpp` (churn under oversight 1). `pages.cpp` and `main.cpp` are the
  likeliest to conflict. A change upstream makes to `nds::Live`, `RomData` or `PageData` can break the fork's reader at
  compile time, which is at least loud.
- **Upstream adding GB/GBC**: duplicate work (oversight 1).
- **Hardware and boards.** The pages already lay out for 640x480 and 1024x768. Only the RG DS Plus is available for
  on-device tests, so the RG DS's 640x480 panel is checked headless only.
- **Correctness.** Wrong addresses or a missed revision would show wrong data; the CRC list and the fixtures guard
  against that. A Gen I/II rule missed would give wrong advice, which upstream's design treats as worse than none (the
  GBA design's anti-goal). The logic tests should cover each rule.
- **Repository size.** The reference data adds several MB of JSON to the repository (not to the image).
- **One long-lived branch.** #28 stays one feature (decided), probably 15-25 step commits, while upstream keeps moving.
  Each merge of `main` into `kiwami` during the work needs `kiwami` merged into the branch too, and the hooks checked
  again. The Crystal-first order (suggestion 2) gives a working state early.
- **License.** The fork's banana-ui becomes a GPL-3.0 work, through the GB companion's files (oversight 2). Each GPL
  file must carry its header and the 7(b) line, and the data folders their `LICENSE`/`NOTICE`. A later wish to send any
  of this code to MIT upstream would need it rewritten without PokeDaisy.
- **Size.** This is the fork's largest feature so far: a reader, a ROM decoder (with two sprite compressions and town
  maps), rules, a host, a panel, guides and an extraction script. The plan should split it into steps that each work
  on the device (suggestion 2).

## Testing

**Headless** (`kiwami/tests/`, run by `kiwami/tests/ci.sh`: unit tests built beside `units.cpp`, and `*-ui.sh`
scripts that drive `ui/banana-ui --headless`, as `7z-ui.sh` and `cores-ui.sh` do; `make test` has no fork hook and
needn't get one):

- The RAM reader against PokeDaisy's captures. Each capture's `README.txt` gives the expected values (party, battle,
  bag, place), checked through a replay of `READ_CORE_MEMORY` replies built from the dump.
- The rules: Gen I's chart cells, the Special stat, the catch formulas against worked examples, DV-based shininess,
  gender and Hidden Power.
- Each guide file loads with every entry (the `nds-pokemon-guide` schema; `tools/companions/nds-validate.py` or a fork
  validator for national numbers up to 151/251).
- The extraction script's output matches what's committed.
- The pages at 640x480 and 1024x768 with a made-up `RomData` and a fixture's `Live`, through the `gamepanel`/`pokefeed`
  verbs. Checked for no ability, nature or IV/EV rows, and DVs and stat experience shown.
- `make test` stays green with the rows off (upstream's renders unchanged).

**ROM tests**: optional, only when the user points `GB_ROMS` at a folder. Not planned, since the user didn't name one.

**On the device (RG DS Plus)**, each game started from the menu with Second screen on and its row on, in BananaFront
and in RetroArch (Hardcore mode or the Frontend row), with gambatte and SameBoy:

- the party, a wild battle and a trainer battle (advice and catch chance against the game's own screens), the bag, the
  Pokédex, the map and place, the guide's next milestone;
- sleep in the middle of a battle;
- a European Yellow or a hack (if the user has one) shows "Not supported".
- The game's frame rate with the companion on and off.
- Logs: `/data/banana/logs/banana-ui.log` (detection, "read in N ms", the reader's errors).

## Open questions

1. README line for the 7(b) attribution? *Answered*: kept in the fork. The fork's documentation (the GB companion's
   markdown, the data's `NOTICE`, `docs/FORK.md`) is the "equivalent documentation", and the credits screen gets the
   line through `kiwami/post-build.sh`. Upstream's `README.md` isn't changed.
2. The Map page's picture? *Answered*: the game's own town map (Gen I) and Pokégear maps (Gen II), decoded from the
   ROM. The squares the pages draw for a map with no picture (`model.h` `RegionMap`: "rgba empty") are the fallback
   only.
3. Which games? *Answered*: English retail Red, Blue, Yellow, Gold, Silver, Crystal. The European Yellows and the hacks
   come later.
4. Fork only or upstream? *Answered*: fork only.
5. PokeDaisy's license? *Answered*: extract its data with its attribution. The data is GPL-3.0, and so is the GB
   companion's C++, which may follow PokeDaisy's Kotlin (oversight 2).
6. Guide files? *Answered*: now. PokeDaisy's tables converted for Red/Blue/Yellow/Crystal, and a ROM-built guide for
   Gold/Silver (upstream's `guideFromRom` is gone, so the fork carries its own).
7. Game data source? *Answered*: the player's ROM at run time, PokeDaisy's tables as the tests' reference.
8. Pages? *Answered*: Gen-aware, by hooks in `pages.cpp`.
9. Setting? *Answered*: one fork row per system (`gb`, `gbc`), off by default. Challenge mode follows `gba_challenge`.
10. Test data? *Answered*: PokeDaisy's RAM captures of the six games in `kiwami/tests/`, under its license.
11. PokeDaisy's data for the other mainline games? *Answered*: extracted too, every mainline game it supports up to
    Black 2/White 2, English releases only, as reference data that isn't shipped or read by banana-ui. Part of #28.
12. What the extracted data keeps? *Answered*: facts and PokeDaisy's own words. The games' names, text and tables are
    left out (oversight 11).
13. How far the C++ is kept from PokeDaisy's Kotlin? *Answered*: not at all ("read Kotlin freely"). The GB companion's
    code is GPL-3.0, and so is the fork's banana-ui as a whole (oversight 2).
14. One feature or several? *Answered*: one feature (#28), in Crystal-first steps.
15. How is the ROM reader tested headless? *Answered* (planning, 2026-11-11): unit tests on made-up tables and
    hand-built compressed data in CI, and a second test that runs only when `GB_ROMS` names a folder with the user's
    own six ROMs (never committed, skipped in CI). This replaces "ROM tests: not planned" under *Testing*.
16. Which hook form in `ui/src/nds/pages.cpp`? *Answered* (planning): the fork's blocks as members of `Pages::Impl`:
    two `#include` lines (declarations inside the struct, definitions at the file's end) and the call-site lines.
17. How do Gen I/II, the six games and stat experience get into `ui/src/nds/model.h`? *Answered* (planning): `Gen` and
    `GameId` values as constants in the fork's header, cast into upstream's enum classes, so the two enum lines aren't
    edited. One added line in `struct Mon` for the 16-bit stat experience. This replaces oversight 4's proposed
    `Gen::I`/`Gen::II` hook.
18. How is the game panel routed to the GB companion? *Answered* (planning): `settings.h`'s `gbaPanel` member becomes
    a fork wrapper holding upstream's `gba::Panel` and the GB one (nothing in `settings_gamepanel.cpp`), and
    `main.cpp` gets explicit one-line hooks. No wrapper around `pkmn::Companion`.
19. Hide a place's hidden items once taken? *Answered* (planning): no. They are always listed, as upstream's GBA and DS
    companions list them (the guide model has no "taken" state). The items' flags stay in the reference data.
20. Gen I's Team score counts coverage over 17 types (Steel and Dark don't exist there), so it reads up to 3 points
    low. Fix with three more hooks? *Answered* (planning): no, left as it is and documented.
21. Where do the addresses and ROM offsets PokeDaisy doesn't have come from (Gen II's badges, Gold and Silver's flags,
    the wild tables and trainers in the ROM)? *Answered* (planning, 2026-11-11): pret's disassemblies (pokecrystal,
    pokegold, pokered, pokeyellow), added to `kiwami/DEPENDENCIES.md`. The user clones them; a script extracts the
    symbol addresses and constants needed into the repository.
22. The games' rules and layouts the plan states from memory? *Answered* (planning): each is checked against
    Bulbapedia before the step that implements it is written.
23. The reference data for Gen III to V, here or its own issue? *Answered* (planning): it stays in #28.
