# 0028 Pokémon companion for the Game Boy and Game Boy Color: plan

Issue: #28 · Branch: `k/0028-gb-pokemon` · Investigation: [investigation.md](investigation.md) · Status: approved

## Summary

While English retail Red, Blue, Yellow, Gold, Silver or Crystal plays on one panel, the other shows the DS companion's
pages (Battle, Party, Team, Here, Map, Guide, Dex, Log), as upstream's GBA companion does. It is off by default, with
one Settings row per system (`gb`, `gbc`).

The approach is the investigation's option A. The fork's own reader, ROM decoder, rules, host and panel live in
`ui/src/kiwami/gbpk/` (GPL-3.0, modelled on PokeDaisy's Kotlin readers). They produce upstream's `nds::RomData` and
`nds::Live` in the DS conventions the GBA side already uses (species by national number, items in Gen IV's numbering,
TMs as Gen IV numbers them), so upstream's `nds::SessionThread`, advice, journey log and `nds::Pages` are reused
through marked hooks. Per-game addresses, flag banks and guides are data files extracted from PokeDaisy `135ac809` by a
script; everything of the games themselves is read from the player's ROM at run time.

The work is 18 steps, Crystal first: Gold, Silver and Crystal work on the device after step 8, with their guides after
step 11; Red, Blue and Yellow after step 15.

### Refinements over the investigation

Found while reading the code, each a change to what the investigation proposed:

- **Releases are told by the whole file's SHA-1, not its CRC-32.** PokeDaisy's checked list is SHA-1
  (`GameTitles.kt`, `Poller.kt`), and upstream already links a SHA-1 (`nds::Sha1`, `ui/src/nds/nitro.h:117`). A CRC-32
  list would need a second source (No-Intro) to trust.
- **The settings keys are `kiwami_pokemon_companion_gb` and `kiwami_pokemon_companion_gbc`** in `settings.conf`, in the
  form of the fork's existing `kiwami_sync_times`. The dotted form (`kiwami.tailscale`) is used for page keys.
- **The guides' `flag:N` are renumbered.** PokeDaisy's numbers (0x10000 + n a bank) would make `Live::flags` 32 KB,
  copied four times a second. The data file lists each bank with its address, length and first bit, back to back, and
  the extraction script converts the numbers. `Live::flags` is then under 1 KB.
- **Fewer logic hooks, more page hooks.** With items mapped to Gen IV ids by the reader, `tmNumber`, the rods, Surf and
  the balls work unhooked. The pages need 10 lines (open question 16). The total is about 31 marked lines in 8 upstream
  files, against the investigation's estimate of 15-25.
- **Everything the hooks in `ui/src/nds/` call is header-only.** Upstream's test binaries (`tests/gba-pages-test`,
  `nds-logic-test`, `guide-test`, in `ui/Makefile`) link `nds/*.cpp` without the fork's sources, so a hook calling a
  function defined in a fork `.cpp` would break `make test` at link time.
- **The fork's documentation is `kiwami/GB-POKEMON.md`**, not `docs/GB-POKEMON.md`: a new file under `docs/` would show
  in `docs/FORK.md`'s hook diff as a change with no `kiwami:` in it.
- **Hidden items are always listed** (open question 19). Upstream's guide model has no "taken" state for a secret, so
  the Here page lists a place's hidden items whether taken or not, as on the GBA and DS. Only the flag banks the
  milestones' `done_when` name are read. The items' flags stay in the reference data.
- **Gen I's Team score counts 17 types** (open question 20). Upstream's coverage loops run over `numTypes()` in `nds::Type`'s order, where
  Steel and Dark sit between the others, so they can't be left out without hooks in three more places. In Gen I the
  two are neutral to everything, the Coverage part tops out at 15 of 17, and the text says "of 17". Left as it is, and
  said in `kiwami/GB-POKEMON.md`.
- **The `crystal` capture holds four Poké Balls**, not the investigation's five: its README pokes five in and throws
  one, and the dump's count is 4.

## Acceptance criteria

- **A1**: given Settings > Other systems > Second screen, then the rows `Pokémon companion: Game Boy` and
  `Pokémon companion: Game Boy Color` follow upstream's Pokémon companion row, both Off on a fresh card, each writing
  `kiwami_pokemon_companion_gb` / `_gbc` = `on`/`off` to `settings.conf`. Their hints say a game follows the folder it
  is started from and that Challenge mode applies.
- **A2**: given both rows Off, when any game is started, then nothing of the fork's runs: no ROM read, no memory reads,
  no reply pipe made by the fork, and `make test`'s renders match `main`'s.
- **A3**: given a row On and Second screen on, when one of the nine supported files is started from that system
  (Red, Blue, Yellow USA/Europe; Gold, Silver USA/Europe; Crystal USA rev 0, USA/Europe rev 1, Australia; as `.gb`,
  `.gbc`, `.zip` or `.7z`), then the log says `gb companion: <title>` and the pages show on the game panel, first
  "Reading your game", then the Party page.
- **A4**: given a file with a supported game's header title but another SHA-1 (a European Yellow, a ROM hack), then
  the panel shows "Not supported" alone and nothing is read from the game's memory. A file with any other title gets no
  companion and the panel is as before.
- **A5**: given a supported game started from the system whose row is Off, then no companion (the row of the system the
  game was started from decides, not the cartridge).
- **A6**: given upstream's Game companion has a definition for the game (the user's own, on the card), then upstream's
  companion takes it and the fork's doesn't open.
- **A7**: given a running Gen II game, then `Live` holds what the fixtures' READMEs say: the trainer's name and id, the
  party (species by national number, level, HP, stats, DVs, stat experience, moves, PP, held item, status, friendship,
  egg), the bag's four pockets and the TMs, money, both regions' badges, the Pokédex, the map and coordinates, play
  time and the time of day. For `crystal`: CHRIS, a Chikorita holding a Berry, four Poké Balls, Route 29 at (46, 11).
- **A8**: given a running Gen I game, then the same from its fixtures, with one bag list, no held items, no friendship
  (but Yellow's Pikachu's), no time of day, and Special as both special stats.
- **A9**: given a wild battle, then the Battle page shows both battlers, each of the lead's moves with its damage range
  and who moves first, and the catch chance for each ball in the bag by the generation's own formula. A trainer battle
  shows the foe's team and no catch chance. A battle's start shows the Battle page and its end the Party page.
- **A10**: the rules are each generation's: Gen I's type chart (Ghost does nothing to Psychic, Bug and Poison hit each
  other hard, Ice is neutral to Fire, no Steel or Dark), one Special stat in Gen I, physical or special by the move's
  type, the type items' 10% in Gen II, no burn or paralysis counted twice (the game's battle stats already hold them),
  Gen I's and Gen II's catch formulas, and in Gen II shininess, gender and Hidden Power from the DVs.
- **A11**: the pages are Gen-aware. For these games they show DVs (0-15) and stat experience (0-65535), and in Gen I one
  Special row; they show no Nature, no Ability, no IV/EV columns and no "Effort points" line. Gen I shows no Item row
  and no Friendship row (but Yellow's Pikachu's). Gen II's wild box shows Hidden Power by its own formula.
- **A12**: given the player's ROM, then names, base stats, types, moves, learnsets, evolutions, TM/HM compatibility,
  items, wild encounters (Gen II by time of day), trainers, map names and Pokédex text come from it, and nothing of
  the games is in the repository or the image.
- **A13**: the Dex and Party pages show each species' front sprite decoded from the ROM in its GBC palette, and in
  Gen II the shiny palette for a shiny Pokémon.
- **A14**: the Map page shows the game's own map from the ROM (Gen I's town map; Gen II's Pokégear Johto and Kanto) with
  the player's place lit. A map that can't be decoded falls back to the pages' squares.
- **A15**: Red, Blue, Yellow and Crystal load their guide with no entry skipped; the Guide page's next milestone
  follows the save (badges and flags), and the Here page lists the place's hidden items (always, taken or not, as
  upstream's companions do).
- **A16**: Gold and Silver get a guide built from the ROM's trainer table: the gym leaders, the Elite Four and the
  Champion in level order, each done by its badge or by a later one.
- **A17**: Challenge mode follows upstream's `gba_challenge` row; the journey log and the seen notices are kept in
  `/data/banana/cache/gb-companion/`, per game and trainer.
- **A18**: it reads through BananaFront's shared memory and through RetroArch's pipe, with gambatte and with SameBoy.
- **A19**: the game keeps its frame rate with the companion on, and sleep in the middle of a battle resumes with the
  pages still following the game.
- **A20**: every GPL-3.0 file carries its header and PokeDaisy's 7(b) line; `LICENSE` and `NOTICE` sit in
  `ui/src/kiwami/gbpk/`, in the image's `gb-pokemon/` folder, in `kiwami/tests/gbpk/` and in `kiwami/data/pokedaisy/`;
  Settings > System > Credits shows the line; `kiwami/GB-POKEMON.md` and `kiwami/DEPENDENCIES.md` carry it and say the
  fork's banana-ui is a GPL-3.0 work as a whole.
- **A21**: the extracted files are the script's output for PokeDaisy `135ac809` (`--check` passes without PokeDaisy on
  the machine, by the recorded hashes, and against a checkout when one is given), and hold no game names, text or
  tables beyond the place and boss names the guide schema needs.
- **A22**: `kiwami/data/pokedaisy/` holds the reference data for every mainline game PokeDaisy supports (English
  releases), with an index, and nothing of it is in the image.
- **A23**: upstream's GBA and DS companions behave as on `main` (their tests in `make test` pass unchanged).

## Risks

- **Conflicts with upstream** (the main risk). About 31 marked lines in `main.cpp` (10), `pages.cpp` (10),
  `logic.cpp` (4), `model.cpp` (2), `settings.h` (2), `model.h`, `guide.cpp` and `settings.cpp` (1 each). `pages.cpp`
  and `main.cpp` change daily upstream. Each hook is one added line beside a named statement, so a conflict is putting
  the line back. A change to `nds::Live`, `RomData`, `PageData`, `Pages::Impl`'s helpers or `gba::Panel`'s members
  breaks the fork at compile time, which is loud. Step 17 adds these files to `docs/FORK.md`'s merge checks.
- **`gbaPanel`'s type is changed, not added to** (`settings.h:718`). Upstream calling a new `gba::Panel` member on it
  fails to compile until the wrapper forwards it. Loud, a one-line fix.
- **Constants cast into upstream's enums.** `nds::Gen` and `nds::GameId` are `enum class` with `int` underneath, so any
  `int` value is valid. Upstream's `g != Gen::V` branches then take the Gen III/IV path, which the fork's hooks
  replace where it differs. A new upstream `switch` on `Gen` with no default would fall through for the fork's values:
  `gbpk-units` checks `damage`, `effectiveSpeed` and `catchChance` end to end through upstream's entry points, and
  `gbpk-ui.sh` renders every page.
- **Wrong advice.** A wrong address or a missed rule shows wrong data, which upstream's design treats as worse than
  none. Guards: the SHA-1 list (A4), PokeDaisy's 33 RAM captures (A7, A8), the rules' unit tests (A10), and the
  `GB_ROMS` test against PokeDaisy's reference tables (A12). Gen I's Team score over 17 types is the one known
  inaccuracy (*Refinements*): the score is low by up to 3 points and nothing else follows from it.
- **The ROM reader without ROMs.** CI checks it on made-up data only. A wrong table offset in a real ROM shows in the
  `GB_ROMS` test on the user's machine and in manual tests M2-M7, not in CI.
- **A second source, pret's symbol files.** Four more repositories to pin (step 1). They publish no licence; only
  addresses, offsets and ids are taken, each listed in `gbpk-symbols.txt`. If a `symbols` branch lacks a release's
  file (Crystal's Australian build, Blue), that release's values are checked equal to its sibling's by the fixtures
  (`crystal_au`, `blue`) or the step stops.
- **Rules written from memory.** Guarded by the Bulbapedia check before each step that implements one (*Steps*,
  common rules), with the tests' expected values taken from the page.
- **Step 16 is the least certain in size**: about 30,000 lines of Kotlin read by rules for what is game content, with
  no consumer. It stays in #28 (the user's decision, confirmed 2026-11-11). It comes after everything that runs on
  the device, so a slip there delays only the merge.
- **Extraction.** PokeDaisy's hand-written Kotlin configs need a small evaluator. Whatever the script can't read is
  listed in its output and fails `--check`, never dropped silently.
- **License.** The fork's banana-ui becomes GPL-3.0 as a whole. Step 1 puts the attribution in the credits and the
  documentation before any GPL file reaches the image. None of `ui/src/kiwami/gbpk/` can be offered to MIT upstream.
- **Performance and footprint.**
  - Idle: nothing runs with the rows Off, or for a game that isn't one of the six (one 0x150-byte header read at
    launch when a row is On).
  - Reads: one set every 250 ms, the next only once the last was answered, as upstream's. Gen II about 1.5 KB a set,
    Gen I about 1.2 KB, flags included.
  - Threads: the ROM read (once) and upstream's `SessionThread`; the page's sprite decoder is upstream's.
  - Memory: the ROM kept for look-ups (1 MB Gen I, 2 MB Gen II), sprites decoded on demand.
  - Rootfs: the six data files and four guides, expected under 400 KB before squashfs. Step 17 measures it.
  - Binary: banana-ui grows by the reader and decoder, expected under 200 KB. Step 17 measures it.
  - Menu frames: untouched (the code runs only in the game panel's loop). `make test`'s bench lines are compared in the
    final check.
- **Repository size.** The fixtures add 1.5 MB and the reference data several MB.
- **Upstream ships its own GB companion.** Then the fork's is retired at that merge.
- **One long branch.** Each merge of `main` into `kiwami` during the work is merged into this branch too, and the hook
  list checked again (`git grep -n 'kiwami:'`).

## Steps

Each step is one commit, and leaves the tree building and `make test` passing. Every step also changes
`.workspace/0028-gb-pokemon/plan.md` (its Progress row), not repeated under *Files*.

Common to the steps:

- **Folder**: `ui/src/kiwami/gbpk/`, namespace `banana::kiwami::gbpk`. Every file there starts with the GPL-3.0 header
  and the line `Based on PokeDaisy by Lidor Itzhari - https://github.com/lidor30/pokedaisy`.
- **Logs**: `banana-ui: gb companion: ...` on stderr (on the device: `/data/banana/logs/banana-ui.log`).
- **Checks at the end of every step that touches C++**: `make test`, `make -C ui`, `kiwami/tests/ci.sh .buildroot`.
  Because of the user's standing preference for short runs, `make test` is run once a step, at its end.
- **Stale test binaries**: `ui/Makefile`'s test targets don't list the fork's headers, so a step that changes
  `gbpk.h`, `rules.h`, `guide-games.inc` or the pages' `.inc` files removes upstream's test binaries that include them
  (`rm -f ui/tests/nds-logic-test ui/tests/nds-pages-test ui/tests/gba-pages-test ui/tests/guide-test`) before
  `make test`, so they are built again.
- **Replaced 2026-11-11 (*Deviations*): pret's source is the only reference.** Bulbapedia refuses automated
  access, so the check below isn't made; each rule and layout follows pret's disassembly (the games' own code and
  data), named in the code's comment, and the tests' hand-worked values are taken from that code. What follows is
  the plan as it was written.
- **The games' rules and layouts are checked against Bulbapedia before they are written** (the user's instruction,
  2026-11-11), since several in this plan are from memory. Each step that implements one reads the page first, names
  it in the code's comment and in `kiwami/GB-POKEMON.md`, and takes the unit tests' hand-worked values from the page's
  formula, not from the code. Where the page and pret's source disagree, the source (the game's own code) wins and
  the difference is noted. A difference from what this plan says goes in *Deviations* before the step goes on. The
  pages, by step:
  - step 2: "ROM header" facts per game (the header titles `POKEMON RED`, `POKEMON BLUE`, `POKEMON YELLOW`,
    `POKEMON_GLD`, `POKEMON_SLV`, `PM_CRYSTAL` and the codes `AAUE`, `AAXE`, `BYTE` are from memory: confirmed on the
    user's ROMs in step 5 too), "Character encoding (Generation I)" and "(Generation II)";
  - steps 3 and 12: "Pokémon data structure (Generation I)" and "(Generation II)", "Save data structure" for both,
    "Individual values" (the HP DV, gender and shininess from DVs), "List of items by index number" for both
    generations and for Generation IV (the map in `items.inc`);
  - step 4: "Damage" (Generation I and II: the formula, the 217-255 random factor, the type items' 10%, badge
    boosts), "Stat" and "Status condition" (whether the battle's stats already hold burn's and paralysis's cuts and
    the stat stages, per generation), "Catch rate" (both generations' formulas, Gen II's status bug, each Apricorn
    ball's rule and bugs), "Type/Type chart" (Generation I's), "Hidden Power (move)/Calculation" (Generation II),
    "Physical move" and "Special move" (the split by type);
  - steps 6 and 13: "Pokémon species data structure" for both generations, "Experience" (the growth rates), "Methods
    of evolution", "List of TMs" and "HM" per generation, the wild encounter slots' percents per generation;
  - step 11: "Badge" (each generation's badge order, the bits checked against pret's constants);
  - Bulbapedia has no page for the picture compressions and the town maps' tile formats: steps 7 and 14 follow
    pret's source and PokeDaisy's decoders.
- **English labels**: the fork's new labels (DV, Stat exp., Special, the two rows' names and hints) pass through
  `tr()` with no entry in `ui/assets/lang/*.tsv`, so they show in English in every language (investigation,
  oversight 9, accepted). `kiwami/GB-POKEMON.md` says so.

### 1. PokeDaisy's data: the extraction script, Gen II's addresses, the licence and the credits

- **Files**:
  - `kiwami/tools/pokedaisy-extract.py` (new; the folder is new)
  - `kiwami/tools/pret-extract.py`, `kiwami/tools/gbpk-symbols.txt`, `kiwami/tools/gbpk-extra.json` (new)
  - `kiwami/rootfs/usr/share/banana/companions/gb-pokemon/` (new): `crystal.ram.json`, `gold.ram.json`,
    `silver.ram.json`, `LICENSE`, `NOTICE`, `SOURCES`
  - `kiwami/GB-POKEMON.md` (new)
  - `kiwami/DEPENDENCIES.md` (changed)
  - `kiwami/post-build.sh` (changed)
  - `kiwami/tests/ci.sh` (changed)
- **Change**:
  - **The script** (Python 3, standard library only): `pokedaisy-extract.py <pokedaisy checkout> [--check]`.
    - It refuses a checkout whose `HEAD` isn't `135ac809` unless `--any-commit` is given.
    - It reads `Gen2Reader.kt`'s `GEN2_CRYSTAL`, `GEN2_GOLD` and Silver's config with a minimal evaluator: named
      arguments, hex literals, `wramx(bank, addr)`, `.copy(...)` on another config, and references to constants in the
      same file. The fields that are games' tables (`landmarks`, `genderRatios`, `tmItems`, `pokedex`) are skipped by
      name. A field it can't evaluate is printed under `unread:` and makes the run fail.
    - It reads the releases' SHA-1s for the six games from `GameTitles.kt`.
    - It merges `gbpk-extra.json` (below): what PokeDaisy doesn't read (the badges, the daycare, the roamers, the
      stat stages, the time of day, the event flags' address). A key present in both must agree, or the run fails.
  - **pret's disassemblies, the second source.** PokeDaisy has no Gen II badge address, no Gold or Silver flags, and
    holds the wild tables and trainers as generated tables instead of reading them from the ROM, so those addresses and
    ROM offsets come from pret's symbol files. The user clones `pret/pokecrystal`, `pret/pokegold`, `pret/pokered`
    and `pret/pokeyellow` beside PokeDaisy (in `/home/mih/Documents/Projects/pret/`), with each repository's `symbols`
    branch, which holds the built `.sym` files for every release (if a repository has none, the step stops and says
    so: building one needs rgbds, which isn't planned).
    - `pret-extract.py <pret folder> [--check]` (Python 3, standard library) reads `gbpk-symbols.txt`, the list of
      what is wanted: a line a value, `<game> <key> <symbol>` (`crystal ram.johtoBadges wJohtoBadges`,
      `gold rom.trainers TrainerGroups`). It looks each symbol up in the release's `.sym` (bank and address) and
      writes `gbpk-extra.json`, each value with its symbol's name, the repository and its commit. A symbol not found
      fails the run. Constants that aren't symbols (the leader classes' ids for step 11) are read from the named
      `constants/*.asm` file by the same list (`gold const.leader.FALKNER constants/trainer_constants.asm FALKNER`).
    - Only addresses, offsets and ids are taken: none of pret's code or data tables. The four repositories publish no
      licence; what is copied are facts about the retail ROMs, as PokeDaisy's own `NOTICE` describes its use of them.
    - Its inputs' commits and the hashes of the `.sym` files read go in `SOURCES`; `--check` without the checkouts
      verifies `gbpk-extra.json` against them, as the other script's does.
    - Later steps add lines to `gbpk-symbols.txt` and re-run it (steps 6, 7, 11, 12, 13, 14).
    - It writes each `<game>.ram.json`, sorted and indented, so a re-run is byte-identical.
    - It writes `SOURCES`: PokeDaisy's commit, and the SHA-256 of every input file it read and every file it wrote.
    - `--check` with a checkout: writes to a temporary folder and compares with what's committed. `--check` without
      one: verifies the committed outputs against `SOURCES`' hashes.
  - **The data file's form**:
    ```json
    { "schema": 1, "kind": "gb-pokemon-ram", "game": "crystal", "gen": 2, "title": "Crystal",
      "header_title": "PM_CRYSTAL", "code": "BYTE",
      "releases": [ { "sha1": "f2f52230...", "name": "USA, Europe (Rev 1)" } ],
      "ram": { "partyCount": { "bank": 1, "addr": "0xDCD7" }, "battleMon": { "bank": 0, "addr": "0xC62C" } },
      "sizes": { "itemsMax": 20, "ballsMax": 12, "keyItemsMax": 25 },
      "flags": [ ], "rom": { } }
    ```
    `bank` 0 is the fixed area and 1 the bank the cores map at 0xD000; a bank from 2 up is read at
    `0x10000 + (bank - 2) * 0x1000 + (addr - 0xD000)` (investigation, oversight 8). `flags` and `rom` are filled by
    steps 6, 7 and 10.
  - **`LICENSE`** is PokeDaisy's GPL-3.0 text and **`NOTICE`** its copyright line, the 7(b) term and the attribution
    line, with a sentence saying what here is extracted from it.
  - **`kiwami/GB-POKEMON.md`**: what the feature is, the attribution line, the licence consequence (the fork's
    banana-ui is GPL-3.0 as a whole), the data files' schema and how to re-run the extraction. Later steps add their
    sections.
  - **`kiwami/DEPENDENCIES.md`**: a row for PokeDaisy's data and reader design, commit `135ac809` (1.5.0.1), GPL-3.0
    with the 7(b) term, pinned in `gb-pokemon/SOURCES`, with the note that banana-ui as built by the fork is GPL-3.0.
    And a row each for `pret/pokecrystal`, `pret/pokegold`, `pret/pokered` and `pret/pokeyellow`: the commit of the
    branch read and of its `symbols` branch, pinned in `gb-pokemon/SOURCES`, no licence published, used for symbol
    addresses and constants only (`kiwami/tools/gbpk-symbols.txt` lists each one taken), not built or shipped.
  - **`kiwami/post-build.sh`**: appends to the image's `/etc/banana/credits.txt`, once (checked with `grep -qF`), the
    line `PokeDaisy / Lidor Itzhari = Game Boy Pokémon companion. Based on PokeDaisy by Lidor Itzhari - https://github.com/lidor30/pokedaisy`
    under `[Thanks]`, and stops the build if the file has no `[Thanks]` section.
- **Tests**: in `ci.sh`:
  - `python3 -I kiwami/tools/pokedaisy-extract.py --check` (the hashes), and the three files parse as JSON with
    `kind` `gb-pokemon-ram` and every `ram` address inside 0xC000-0xDFFF or 0xFF80-0xFFFE;
  - `post-build.sh` on the stand-in target already used there: the credits line is there once, the same after a second
    run, and the build stops without `[Thanks]`.
  - `python3 -I kiwami/tools/pret-extract.py --check` (the hashes), and every line of `gbpk-symbols.txt` has its
    value in `gbpk-extra.json`.
  - By hand, once: both scripts' `--check` against the checkouts (`/home/mih/Documents/Projects/pokedaisy`,
    `/home/mih/Documents/Projects/pret`).
  **The user, before this step**: clones the four pret repositories into `/home/mih/Documents/Projects/pret/` with
  their `symbols` branches (`git clone https://github.com/pret/<name>`, then `git fetch origin symbols:symbols`).
- **Commit**: `GB Pokémon companion: PokeDaisy's Gen II addresses, extracted by a script, with its licence and credit`

### 2. Telling the games apart, and their text

- **Files**:
  - `ui/src/kiwami/gbpk/gbpk.h`, `games.h`, `games.cpp`, `text.h`, `text.cpp`, `LICENSE`, `NOTICE` (new; the folder is
    new)
  - `ui/src/kiwami/kiwami.mk` (changed)
  - `kiwami/tests/gbpk-units.cpp` (new)
  - `kiwami/tests/ci.sh` (changed)
- **Change**:
  - **`gbpk.h`** (header-only, includes `nds/model.h`): `constexpr nds::Gen GEN_I = (nds::Gen)16, GEN_II = (nds::Gen)17;`
    `constexpr nds::GameId RED = (nds::GameId)100, BLUE, YELLOW, GOLD, SILVER, CRYSTAL` (100-105);
    `inline bool oldGen(nds::Gen g) { return g == GEN_I || g == GEN_II; }`.
  - **`games.cpp`**:
    - `std::vector<uint8_t> readRomFile(const std::string& path, size_t limit)`: a file, or the first `.gb`/`.gbc` in
      a `.zip` (`ui/src/zip.h`) or `.7z` (`ui/src/kiwami/ra7z.h`).
    - `const char* dataDir()`: `$KIWAMI_GBPK_DIR`, else `/usr/share/banana/companions/gb-pokemon`.
    - `struct GameData { nds::Game game; int gen; std::map<std::string, Addr> ram; sizes; flags; rom; }` and
      `bool loadData(const std::string& key, GameData& out, std::string* why)`, with the fork's `json.h`.
    - `enum class Match { NONE, KNOWN, UNSUPPORTED }` and `Match detect(const uint8_t* header, size_t n, std::string& key)`:
      the header's title at 0x134 against each data file's `header_title` (an index of the six, read once); and
      `Match confirm(const std::vector<uint8_t>& rom, const GameData& d)`: the whole file's SHA-1 (`nds::Sha1`) in
      `releases`, else `UNSUPPORTED`.
    - `nds::Game` filled: `id` the fork's constant, `gen`, `code` from the file, `title`, `dexSize` 151 or 251,
      `romHash` the SHA-1.
  - **`text.cpp`**: `std::string decode(int gen, const uint8_t* p, size_t max)`, the games' English character set to
    UTF-8, stopping at 0x50; and `nameCase` for a species or move name in capitals ("CHIKORITA" to "Chikorita").
  - **`kiwami.mk`**: `SRC += $(wildcard src/kiwami/gbpk/*.cpp)` above the line that turns `SRC` into objects, and
    `banana-ui: $(wildcard src/kiwami/gbpk/*.h src/kiwami/gbpk/*.inc)` for the one-call build.
- **Tests**: `gbpk-units.cpp`, a test binary of its own (built and run by `ci.sh` beside `units`, with
  `-std=c++17 -Wall -Werror -I ui/src`, given `kiwami/rootfs/usr/share/banana/companions/gb-pokemon` and
  `kiwami/tests/gbpk`):
  - a made-up 0x150-byte header titled `PM_CRYSTAL` is `KNOWN` as `crystal`; `POKEMON RED` is `NONE` until step 12
    adds its file; `TETRIS` is `NONE`;
  - a made-up ROM with Crystal's title is `UNSUPPORTED` by `confirm`;
  - the three data files load, with `partyCount` at bank 1, 0xDCD7 for Crystal;
  - `decode` on Gen II's bytes for "CHRIS" and a name with the PK/MN ligatures.
  `kiwami/tests/ui-objects.sh` still passes (the object build is byte for byte the one call's).
- **Commit**: `GB Pokémon companion: the six games told by header title and SHA-1, their data files and character set`

### 3. Gen II's memory: the reader, checked against PokeDaisy's captures

- **Files**:
  - `ui/src/kiwami/gbpk/reader.h`, `reader.cpp`, `items.inc` (new)
  - `ui/src/nds/model.h` (hook)
  - `kiwami/tests/gbpk/fixtures/` (new): PokeDaisy's 17 Gen II captures (`crystal`, `crystal_au`, `crystal_rev0`,
    `crystal_battle*`, `gold*`, `silver*`), each with its `README.txt`; `kiwami/tests/gbpk/LICENSE`, `NOTICE`
  - `kiwami/tests/gbpk-units.cpp`, `kiwami/tests/ci.sh` (changed)
  - `kiwami/GB-POKEMON.md` (changed)
- **Change**:
  - **Hook**, `ui/src/nds/model.h`, in `struct Mon` after the `ivs`/`evs` line (`:151`):
    `uint16_t statExp[5] = {};	// kiwami: Gen I/II's stat experience: HP, Atk, Def, Speed, Special (ui/src/kiwami/gbpk/)`
  - **`class Reader`**, with `pkmn::Companion`'s shape:
    - `void open(const GameData& d)`, `close()`, `active()`;
    - `std::string requests(double nowMs)`: a set of `READ_CORE_MEMORY <addr> <len>` lines every 250 ms, the next only
      once the last was answered, asked again after 10 s without an answer (`pokemon.cpp:1316`'s rules);
    - `void feed(const char* data, size_t n)`: replies parsed with upstream's `pkmn::parseReply`;
    - `uint64_t serial()`, `bool inBattle()`, `std::shared_ptr<nds::Live> live(const nds::RomData* rom)`;
    - `std::string error()`: the emulator's reply when it had no data ("no memory map defined").
  - **A set for Gen II**: the party (count, species list, 6 x 48 bytes, nicknames, OT names), the player (name, id,
    money, play time, badges, gender), the map (group, number, X, Y), `wBattleMode`, `wBattleMon` and `wEnemyMon` (32
    bytes each) with the enemy party when in a trainer battle, the stat stages, the four pockets and the TM case, the
    two Pokédex bit sets, the time of day, the daycare, and in Gold and Silver the roamers.
  - **`live()`**, in the DS conventions:
    - species are national numbers already; an egg by the species list's EGG;
    - items through `items.inc`: by Gen II item id, `{ Gen IV id, nds::Pocket }` for the items the logic names (the
      balls, the Apricorn balls, the rods, the type items, Light Ball, Thick Club, the stones), TM01-50 as 328-377 and
      HM01-07 as 420-426, everything else `1000 + its own id` (as upstream's `GEN3_ONLY`);
    - DVs into `ivs` (HP's worked out from the other four), stat experience into `statExp`, `evs` left zero;
    - `stats` as Atk, Def, Speed, Sp. Atk, Sp. Def;
    - `female` from the Attack DV and the species' ratio (from `rom`, when given);
    - `pid` made from the DVs and the OT id, so the journey log can tell one Pokémon from another; `otSecret` chosen so
      upstream's `Mon::shiny()` gives the DVs' answer (Defense, Speed and Special DVs 10, Attack DV 2, 3, 6, 7, 10, 11,
      14 or 15);
    - the battle: `ours[0]`, `foes[0]`, the battle's own types, the stat stages, `foeParty`, `kind` wild or trainer;
    - `zone` = map group << 8 | map number, `timeOfDay`, `badges` (Johto's eight, then Kanto's), `bag` by pocket;
    - `valid` once a party with a count of 1-6 and a matching species list was read.
  - **The fixtures** are copied unchanged. `GB-POKEMON.md` says where they come from and what they are.
- **Tests**: `gbpk-units` replays each capture: for every read the reader asks, a reply built from the dump
  (`wram.bin` is 0xC000-0xDFFF, `hram.bin` 0xFF80-0xFFFE, `wramx.bin` the eight banks, 0x1000 each). It asserts:
  - `crystal`: trainer CHRIS, a party of two with a Chikorita (152) holding a Berry and a Pidgey (16), four Poké
    Balls in the balls pocket (the dump's count; the README pokes five in and throws one), zone 24 << 8 | 3 at
    (46, 11);
  - `crystal_battle`: a wild battle, the lead Chikorita Lv 5 at 19/19 with Tackle and Growl, the foe a Sentret Lv 3;
  - `gold_trainer`: a trainer battle with a foe party;
  - `crystal_rev0` and `crystal_au` read as `crystal` does; `gold` and `silver` each their README's party and place;
  - a set is answered once and not asked again within 250 ms; an error reply sets `error()` and leaves `valid` false;
  - a party count of 7 leaves `valid` false.
- **Commit**: `GB Pokémon companion: Gold, Silver and Crystal's memory read into the pages' Live`

### 4. Gen I's and Gen II's rules, in the logic

- **Files**:
  - `ui/src/kiwami/gbpk/rules.h` (new, header-only)
  - `ui/src/nds/model.cpp` (hook, 2 lines)
  - `ui/src/nds/logic.cpp` (hook, 4 lines)
  - `kiwami/tests/gbpk-units.cpp` (changed)
  - `docs/FORK.md` (changed)
- **Change**:
  - **`rules.h`**, all `inline`:
    - `inline std::atomic<int> oldChart{0}` and `void useChart(nds::Gen g)`: 1 for Gen I, else 0. Set by the host when
      a game's data is read, as upstream's `useModernTypeChart`.
    - `int chart(Type a, Type d)`: -1 when `oldChart` is 0. For Gen I: 4 for any pair with Steel or Dark, 0 for Ghost
      on Psychic, 8 for Bug on Poison and Poison on Bug, 4 for Ice on Fire, else -1 (upstream's cell).
    - `nds::DamageRange damage(Gen g, const MoveData&, const Fighter& a, const Fighter& d, Weather)`: the games'
      formula `((2L/5 + 2) * P * A / D) / 50 + 2`, then STAB and the two type multipliers, over the 217-255 random
      factor. The stats are taken as the battle holds them: no stat stages, burn or paralysis applied again. Gen II's
      type items add 10%, Light Ball doubles Pikachu's Special Attack, Thick Club Cubone's and Marowak's Attack. No
      abilities.
    - `int speed(const Fighter&)`: the Speed as the battle holds it.
    - `float catchChance(Gen g, const RomData&, const Mon& wild, uint16_t ball, const CatchContext&, float* mult)`:
      Gen I's (the ball's own random range and the status thresholds 25 and 12) worked out as a probability, and
      Gen II's (the modified rate with its status bug: only sleep and freeze add 10; the Apricorn balls' rules as the
      games have them, their bugs included). Master Ball 1.
    - `int hiddenPowerType(const uint8_t dvs[6])`, `hiddenPowerPower`: Gen II's.
    - `int statFrom(int base, int dv, int statExp, int level, bool hp)`: for the tests and the ROM-built guide.
  - **Hooks**:
    - `model.cpp`, after its includes: `#include "../kiwami/gbpk/rules.h"	// kiwami: Gen I's type chart`
    - `model.cpp`, first line of `one()` (`:87`):
      `if (int q = kiwami::gbpk::chart(a, d); q >= 0) return q;	// kiwami: Gen I's chart (Red, Blue, Yellow)`
    - `logic.cpp`, after its includes: `#include "kiwami/gbpk/rules.h"	// kiwami: Gen I/II's damage, speed and catch`
    - `logic.cpp`, first line of `damage()` (`:99`):
      `if (kiwami::gbpk::oldGen(g)) return kiwami::gbpk::damage(g, mv, a, d, w);	// kiwami:`
    - `logic.cpp`, first line of `effectiveSpeed()` (`:200`); its `Gen` parameter is unnamed, so the hook reads the
      chart's owner instead: `if (kiwami::gbpk::oldRules()) return kiwami::gbpk::speed(f);	// kiwami:`, where
      `oldRules()` is true while a Gen I or II game's data is in use (a second atomic, set with `useChart`).
    - `logic.cpp`, first line of `catchChance()` (`:217`):
      `if (kiwami::gbpk::oldGen(g)) return kiwami::gbpk::catchChance(g, rom, wild, ball, c, mult);	// kiwami:`
  - **`docs/FORK.md`**, *Taking in upstream*, point 3: the logic's and the chart's hooks, what each replaces, and that
    what they call must stay header-only.
- **Tests**: `gbpk-units`, linking `ui/src/nds/logic.cpp` and `model.cpp`:
  - the chart through `nds::effectiveness`: with Gen I on, Ghost/Psychic 0, Bug/Poison 8, Poison/Bug 8, Ice/Fire 4,
    Fighting/Steel 4; with it off, upstream's 8, 2, 4, 2, 8;
  - `nds::damage` with `GEN_II`: a Lv 5 Tackle worked by hand; Charcoal on Ember 10% over the same without;
    a burned attacker's range equal to the unburned one with the same stats;
  - `nds::damage` with `GEN_I`: Special used for both sides of a Psychic move;
  - `nds::catchChance`: Gen I, a full-HP catch rate 255 with a Poké Ball; Gen II, a catch rate 45 at 1 HP asleep with
    an Ultra Ball, each against the value worked by hand in the test's comment; a Master Ball is 1 in both;
  - shininess and gender through `Mon::shiny()` and `female` for a shiny DV set and a plain one;
  - Hidden Power for two DV sets.
  `make test` (upstream's `nds-logic-test` and `gba-pages-test` link without the fork's sources and pass unchanged).
- **Commit**: `GB Pokémon companion: Gen I's type chart and both generations' damage, speed and catch rules`

### 5. The ROMs for the ROM tests: the list, the folder and its check

- **Files**:
  - `kiwami/tests/gbpk/roms.sha1` (new)
  - `kiwami/tests/gbpk-roms.sh` (new)
  - `kiwami/tests/gbpk-units.cpp`, `kiwami/tests/ci.sh` (changed)
  - `kiwami/GB-POKEMON.md` (changed)
- **Change**: the ROMs themselves are never added to the repository (A12; the fork is public). The step commits the
  list of what the tests expect and the check of the user's folder, and the user copies the files.
  - **The folder**: `GB_ROMS`, by default `$HOME/gb-companion/roms`, outside the repository (as upstream's
    `~/gba-companion/roms` for `GBA_ROMS`), so nothing has to be ignored and a `git add` can't pick a ROM up.
  - **`roms.sha1`**, one line a file: the SHA-1, the system (`gb`, `gba`, `nds`), `required` or `optional`, the
    game's key, the No-Intro file name. The SHA-1s are PokeDaisy's (`GameTitles.kt`: pret's `roms.sha1` for pokered,
    pokeyellow, pokegold, pokecrystal). The Game Boy and Game Boy Color files, which this feature's tests read:

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

    The SHA-1 is what counts: a file is found by its hash, whatever it is called, and a name that differs from the
    list's is reported as a note (older No-Intro sets write Yellow's `(GBC,SGB Enhanced)`), not a failure. The two
    optional Crystals share every address with rev 1, so the ROM tests' Crystal checks run on each that is there.
  - **The GBA and DS files** (the user's request, 2026-11-11): the English releases of every other mainline game
    PokeDaisy supports, all `optional`. Nothing in #28 reads them: its reference data for these games (step 16) has no
    consumer yet, so the check only says which are there, for the later comparison of upstream's guides with
    PokeDaisy's tables (investigation, suggestion 6). Upstream's own tests read some of the same files from the same
    folders (`ui/tests/run.sh`: an Emerald in `GBA_ROMS`, the DS games in `NDS_ROMS`). SHA-1s from `GameTitles.kt`
    (pret's `roms.sha1` for pokeruby, pokefirered, pokeemerald, pokeplatinum, pokeheartgold; No-Intro's dumps for
    Black, White, Black 2 and White 2):

    | Folder | No-Intro file name | SHA-1 |
    |---|---|---|
    | `GBA_ROMS` | `Pokemon - Ruby Version (USA).gba` | `f28b6ffc97847e94a6c21a63cacf633ee5c8df1e` |
    | `GBA_ROMS` | `Pokemon - Ruby Version (USA, Europe) (Rev 1).gba` | `610b96a9c9a7d03d2bafb655e7560ccff1a6d894` |
    | `GBA_ROMS` | `Pokemon - Ruby Version (USA, Europe) (Rev 2).gba` | `5b64eacf892920518db4ec664e62a086dd5f5bc8` |
    | `GBA_ROMS` | `Pokemon - Sapphire Version (USA).gba` | `3ccbbd45f8553c36463f13b938e833f652b793e4` |
    | `GBA_ROMS` | `Pokemon - Sapphire Version (USA, Europe) (Rev 1).gba` | `4722efb8cd45772ca32555b98fd3b9719f8e60a9` |
    | `GBA_ROMS` | `Pokemon - Sapphire Version (USA, Europe) (Rev 2).gba` | `89b45fb172e6b55d51fc0e61989775187f6fe63c` |
    | `GBA_ROMS` | `Pokemon - Emerald Version (USA, Europe).gba` | `f3ae088181bf583e55daf962a92bb46f4f1d07b7` |
    | `GBA_ROMS` | `Pokemon - FireRed Version (USA).gba` | `41cb23d8dccc8ebd7c649cd8fbb58eeace6e2fdc` |
    | `GBA_ROMS` | `Pokemon - FireRed Version (USA, Europe) (Rev 1).gba` | `dd5945db9b930750cb39d00c84da8571feebf417` |
    | `GBA_ROMS` | `Pokemon - LeafGreen Version (USA).gba` | `574fa542ffebb14be69902d1d36f1ec0a4afd71e` |
    | `GBA_ROMS` | `Pokemon - LeafGreen Version (USA, Europe) (Rev 1).gba` | `7862c67bdecbe21d1d69ce082ce34327e1c6ed5e` |
    | `NDS_ROMS` | `Pokemon - Platinum Version (USA) (Rev 1).nds` | `0862ec35b24de5c7e2dcb88c9eea0873110d755c` |
    | `NDS_ROMS` | `Pokemon - HeartGold Version (USA).nds` | `4fcded0e2713dc03929845de631d0932ea2b5a37` |
    | `NDS_ROMS` | `Pokemon - SoulSilver Version (USA).nds` | `f8dc38ea20c17541a43b58c5e6d18c1732c7e582` |
    | `NDS_ROMS` | `Pokemon - Black Version (USA, Europe) (NDSi Enhanced).nds` | `26ad0b9967aa279c4a266ee69f52b9b2332399a5` |
    | `NDS_ROMS` | `Pokemon - White Version (USA, Europe) (NDSi Enhanced).nds` | `bc696a0dfb448c7b3a8a206f0f8214411a039208` |
    | `NDS_ROMS` | `Pokemon - Black Version 2 (USA, Europe) (NDSi Enhanced).nds` | `e51e6dfb8678a3d19dcd2a10691b96a569ca0abb` |
    | `NDS_ROMS` | `Pokemon - White Version 2 (USA, Europe) (NDSi Enhanced).nds` | `b5d7490be7b415b8f1e672a53e978a9cc667e56a` |

    `GBA_ROMS` is by default `$HOME/gba-companion/roms` (upstream's default) and `NDS_ROMS` `$HOME/nds-companion/roms`.
    Which SHA-1 is which revision of Ruby, Sapphire, FireRed and LeafGreen follows the order of pret's `roms.sha1`
    files and is checked against the user's files when they are copied: a revision's name and hash that turn out
    swapped are corrected in the list in this step. PokeDaisy has no Diamond or Pearl. The non-English releases it
    lists and the hacks are left out, as in the rest of the feature.
  - **`gbpk-roms.sh`**: with no folder at `GB_ROMS`, prints `gbpk-roms: skipped, no ROMs in <folder>` and succeeds
    (CI). With one, it hashes every `.gb` and `.gbc` there (`sha1sum`), and prints a line a manifest entry: `ok`,
    `ok (named <its name>)`, `missing`, or for a file with a listed name and another hash `wrong file: sha1 <...>`
    (a European release, a hack, a bad dump). A required file missing or wrong fails it; an optional one missing is a
    line. Files not in the list are named and left alone. Step 6 adds the reader's checks after this. The `gba` and
    `nds` entries are then looked for in `GBA_ROMS` and `NDS_ROMS` the same way, each a line (`ok`, `missing`,
    `wrong file`), none of them failing the check; a folder that isn't there is one line.
  - **`gbpk-units`**: every SHA-1 in `roms.sha1` for a game whose data file exists is in that file's `releases`, and
    the other way round (Gen II now; Gen I's three from step 12 on).
  - `kiwami/GB-POKEMON.md`: the table above and how to run the check.
- **Tests**: `ci.sh` runs `gbpk-roms.sh` (skipped there). `gbpk-units`' manifest check. On made-up folders in
  `ci.sh`: an empty folder fails naming the six required files; a file with a listed name and other bytes is
  `wrong file`; six made-up files can't pass (their hashes aren't the list's).
  **The user, before step 6**: copies the six required files (and the two optional ones if they have them) into
  `~/gb-companion/roms/` and runs `kiwami/tests/gbpk-roms.sh`, which must print `ok` for the six. The GBA and DS
  files go into `~/gba-companion/roms/` and `~/nds-companion/roms/` whenever the user has them; no step waits on them.
- **Commit**: `GB Pokémon companion: the list of ROMs its ROM tests expect, by SHA-1, and the check of the folder`

### 6. Gen II's ROM: the tables

- **Files**:
  - `ui/src/kiwami/gbpk/rom.h`, `rom.cpp`, `rom2.cpp` (new)
  - `kiwami/tools/pokedaisy-extract.py`, `kiwami/tools/gbpk-symbols.txt`, `kiwami/tools/gbpk-extra.json` (changed)
  - `kiwami/rootfs/usr/share/banana/companions/gb-pokemon/{crystal,gold,silver}.ram.json`, `SOURCES` (changed)
  - `kiwami/tests/gbpk-units.cpp`, `kiwami/tests/gbpk-roms.cpp` (new), `kiwami/tests/gbpk-roms.sh` (changed),
    `kiwami/tests/ci.sh` (changed)
  - `kiwami/GB-POKEMON.md` (changed)
- **Change**:
  - **The data files' `rom` section**: each table's bank and address, from PokeDaisy's `Gen2Dex.kt` and `Gen2Art.kt`
    where it reads them, and from pret's symbol files through `gbpk-extra.json` for the rest (wild tables, trainers,
    map headers, landmark names), each with its symbol's name.
  - **`rom.h`**: `std::shared_ptr<const nds::RomData> readRom(std::shared_ptr<const std::vector<uint8_t>> rom,
    const GameData& d, std::string* why)`; null with the reason when a table's first entries don't check.
  - **`rom2.cpp`** fills, for Gen II:
    - `species` 1-251: name, types (in `nds::Type`'s order), base stats (Sp. Atk and Sp. Def their own), catch rate,
      growth, gender ratio, the learnset, the evolutions (methods in `nds::evo`'s numbering, the `how` words through
      upstream's wording where a method matches), TM/HM numbers (HM01-07 as 93-99), Pokédex category, height, weight
      and text;
    - `moves`: name, type, power, accuracy, PP, the category by the type (physical up to Steel, special from Fire),
      priority for Quick Attack, Mach Punch, ExtremeSpeed and the others the games give one;
    - `items`: at the ids `items.inc` gives, with the ROM's names and pockets; `tmMoves`;
    - `zones` by map group << 8 | number: the landmark's name and the wild encounters, grass by morning, day and
      night, water, both rods' tables by time, Headbutt and Rock Smash, with each slot's percent;
    - `trainers`: class, name and team (levels, species, items and moves where the trainer's type has them), keyed by
      class << 8 | number;
    - `locationNames`: the landmarks.
    Each table is checked by its first entries before use (Bulbasaur's base stats 45/49/49/45/65/65, Pound at move 1).
  - `sprite()` returns none and `regionMaps` stays empty until step 7.
- **Tests**:
  - `gbpk-units`: a made-up 64 KB ROM with two species, three moves, one map's wild table and one trainer, laid out
    by a made-up data file; every field read back. A ROM whose first species doesn't check gives null and the reason.
  - `gbpk-roms.sh`, after step 5's check of the folder, builds and runs `gbpk-roms`, which reads each manifest file
    that is there and asserts, for Crystal, Gold and Silver: the SHA-1 is in the list; 251 species and 251
    moves named; Chikorita's base stats and types; Route 29's morning grass has Pidgey and Sentret; Falkner's team is
    Pidgey Lv 7 and Pidgeotto Lv 9; and, when a PokeDaisy checkout is given as its second argument, every species'
    gender ratio and growth rate against `Gen2SpeciesCrystal.kt`. Its log names each ROM and the time the read took.
- **Commit**: `GB Pokémon companion: Gold, Silver and Crystal's species, moves, items, encounters and trainers from the ROM`

### 7. Gen II's ROM: the sprites and the Pokégear's maps

- **Files**:
  - `ui/src/kiwami/gbpk/pics.h`, `pics.cpp`, `maps.h`, `maps.cpp` (new)
  - `ui/src/kiwami/gbpk/rom2.cpp` (changed)
  - `kiwami/tools/pokedaisy-extract.py`, the three `.ram.json`, `SOURCES` (changed)
  - `kiwami/tests/gbpk-units.cpp`, `kiwami/tests/gbpk-roms.cpp` (changed)
- **Change**:
  - **`pics.cpp`**: `bool lz2(const uint8_t* p, size_t n, std::vector<uint8_t>& out, size_t maxOut)`, Gen II's LZ
    (literal, repeat, alternate, zero fill, and the three copy modes, with the long form); 2-bits-a-pixel tiles to
    indices; a species' front picture (5x5 to 7x7 tiles) as an `nds::Sprite` of one frame, `normal` and `shiny` from
    the species' two palettes in the ROM (colours 0 white and 3 black, as the games set them), index 0 clear. Unown's
    letter from the DVs is left at A.
  - **`maps.cpp`**: the Pokégear's Johto and Kanto tile maps and their tiles into two `nds::RegionMap`s (160x144 px,
    cells 8 px), each cell the landmark's name id; `Location::spot` for each zone from its landmark's position; a zone
    with no landmark gets `map` -1. Any step that fails leaves `rgba` empty, so the pages draw squares.
  - `rom2.cpp`: `sprite()` decodes on demand (safe from any thread: the ROM is constant), and `regionMaps`.
- **Tests**:
  - `gbpk-units`: `lz2` on hand-built streams, one a command (expected bytes in the test), a stream running past its
    end and one longer than `maxOut` both refused; a made-up 2x2-tile picture with a palette decodes to the expected
    indices and colours; a made-up 3x2 tile map gives the expected cells.
  - `gbpk-roms` (with `GB_ROMS`): every species' sprite decodes with a non-empty picture, written as PNGs to its
    output folder with both maps for looking at; Chikorita's picture is 5x5 tiles wide; New Bark Town's cell names it.
- **Commit**: `GB Pokémon companion: Gen II's sprites in their palettes and the Pokégear's maps, decoded from the ROM`

### 8. The companion on the game panel, with its Settings rows

- **Files**:
  - `ui/src/kiwami/gbpk/host.h`, `host.cpp`, `panel.h`, `panel.cpp`, `game.h`, `game.cpp` (new)
  - `ui/src/main.cpp` (hook, 10 lines)
  - `ui/src/settings.h` (hook, 2 lines)
  - `ui/src/settings.cpp` (hook, 1 line)
  - `ui/src/kiwami/kiwami.cpp` (changed)
  - `kiwami/tests/gbpk-ui.sh` (new), `kiwami/tests/ci.sh` (changed)
  - `docs/FORK.md`, `kiwami/GB-POKEMON.md` (changed)
- **Change**:
  - **`Host`**, `gba::Host`'s shape (`tick`, `ready`, `failed`, `unsupported`, `takeData`, `markSeen`, `title`,
    `fed`): reads the ROM file and `readRom` on a thread, logs `gb companion: <title> read in <N> ms`, calls
    `useChart`, starts an `nds::SessionThread` (state in `<data>/cache/gb-companion`, challenge from `gba_challenge`),
    feeds it `Reader::live()` at each new serial, and sets `waitWhy` from `Reader::error()`. A file `confirm` refuses
    gives `unsupported`, logs `gb companion: <title>: not a release the companion knows (sha1 <...>)` and closes the
    reader, so nothing is asked of the game. Its destructor calls `useChart` with no game, so Gen I's chart and the
    old rules never outlive the game: upstream's hosts don't reset them, and an Emerald started after Red would
    otherwise be judged by Gen I's chart.
  - **`Panel`**, `gba::Panel`'s members over `Host` and `Reader`.
  - **`PanelSwitch`** (`panel.h`): holds a `gba::Panel` and a `gbpk::Panel`, with every member `gba::Panel` has.
    `open(gba::Host*, pkmn::Companion*, const gba::Panel::Look&)` opens upstream's panel, and the GB one too when a GB
    host is pending (`static void pending(Host*, Reader*)`, set by `Game::attach`). Every other member goes to
    whichever is active, upstream's first. `ready()` and `title()` likewise, so upstream's test verbs read the GB
    panel's state.
  - **`Game`** (`game.h`, the object `main.cpp` holds):
    - `void open(bool panel, const std::string& system, const std::string& rom, const std::string& settingsConf,
      bool taken, const char* outPath, int& outFd)`: returns unless `panel`, the system is `gb` or `gbc`, its row is
      `on` (the key's line read from `settingsConf`, missing: `off`), and `taken` (another companion opened) is false. Reads the header, `detect`s; for a known title opens the
      reader, makes the reply pipe as upstream does (`unlink`, `mkfifo` 0600, `open` `O_RDWR | O_NONBLOCK |
      O_CLOEXEC`) when `outFd` is -1, and logs `gb companion: <title>`. It closes the last game's first.
    - `void attach(int outFd, const std::string& data)`: makes the `Host` when active and `outFd >= 0`, and calls
      `PanelSwitch::pending`.
    - `bool active()`, `std::string requests(double)`, `bool feed(const char*, size_t)` (false when not active).
    - `void release(Settings& view, std::mutex& uiLock)`: under the lock `view.gbaRelease()`, then the host goes, and
      logs `gb companion: <N> states handed to the logic`.
    - `void close()`.
    - `static bool script(const std::string& op, std::istream& args, Settings& s, const std::string& out, double now)`: the
      headless verbs `gbpanel <screen> <fixture folder> [rom]` (the panel for a made-up game: the reader opened on the
      fixture's game with `kiwami/tests/gbpk`'s stand-in `RomData`, or on a real ROM when given) and
      `gbfeed <fixture folder>` (the reader's requests answered from the dump, then `gameUpdate`); each logs a line
      like upstream's (`gbfeed <folder>: asked <n>, party <n>, battle <0|1>, view <view>`).
  - **Hooks in `main.cpp`**:
    1. after `#include "kiwami/library.h"` (`:44`): `#include "kiwami/gbpk/game.h"	// kiwami: the GB Pokémon companion`
    2. after `pkmn::Companion companion;` (`:2293`): `kiwami::gbpk::Game gbGame;	// kiwami: the GB Pokémon companion, for the game running`
    3. in the headless script, before `else if (!op.empty()) { logf("unknown command` (`:1451`):
       `else if (kiwami::gbpk::Game::script(op, cs, settings, o.headless, now)) {}	// kiwami: gbpanel, gbfeed`
       (a static member with a static `Game` of its own, as upstream's `gamepanel` keeps a static `comp`: the script
       runs before `gbGame` is declared)
    4. in `gamePanel`, before `settingsView.gbaAttach(gbaHost.get());` (`:2576`): `gbGame.attach(outFd, o.data);	// kiwami:`
    5. after the `std::string r = companion.active() ? ...` line (`:2593`):
       `if (gbGame.active()) r = gbGame.requests(t);	// kiwami:`
    6. before `if (companion.active()) companion.feed(rep.data(), rep.size());` (`:2607`):
       `if (gbGame.feed(rep.data(), rep.size())) {} else	// kiwami:`
    7. before `if (companion.active()) companion.feed(buf, (size_t)n);` (`:2671`):
       `if (gbGame.feed(buf, (size_t)n)) {} else	// kiwami:`
    8. after `if (pidFd >= 0) ::close(pidFd);` (`:2723`): `gbGame.release(settingsView, worker.uiLock);	// kiwami:`
    9. after the `if (pkOn || gcOn) { ... }` block that makes the pipe (`:2826-2833`):
       `gbGame.open(panel, sd.id, rom, o.data + "/settings.conf", pkOn || gcOn, OUT, outFd);	// kiwami: the GB Pokémon companion`
    10. after `companion.close();` at the game's end (`:3046`): `gbGame.close();	// kiwami:`
  - **Hooks in `settings.h`**:
    - after `#include "gba/panel.h"` (`:23`): `#include "kiwami/gbpk/panel.h"	// kiwami: the GB Pokémon companion's panel`
    - `:718`, the member's type: `kiwami::gbpk::PanelSwitch gbaPanel;	// kiwami: gba::Panel, or the GB Pokémon companion's (ui/src/kiwami/gbpk/)`
  - **Hook in `settings.cpp`**, after the Pokémon companion row's hints (`:644`), before the Challenge mode comment:
    `Kiwami::rows(*this, SS);	// kiwami: the Pokémon companion for the Game Boy and the Game Boy Color`
  - **`kiwami.cpp`**, `Kiwami::rows`, group `Second screen`: two rows as Show sync times' (file `settings.conf`,
    `Off`/`On`, `off`/`on`, icon `gamepad`): `Pokémon companion: Game Boy` (`kiwami_pokemon_companion_gb`) and
    `Pokémon companion: Game Boy Color` (`kiwami_pokemon_companion_gbc`). Help: "The Pokémon games' pages on the second
    screen, as on the GBA: Red, Blue, Yellow, Gold, Silver and Crystal (English). A game follows the row of the system
    it is started from. Based on PokeDaisy by Lidor Itzhari." Hints: "No companion." / "Battle, party, a guide, the map
    and more. Needs Second screen on. Challenge mode applies to it too." The Second screen page isn't in `build()`'s
    `ORDER`, so the rows stay where the hook adds them.
  - **`docs/FORK.md`**: the wiring's hooks in point 3 (the `gbaPanel` wrapper must forward every `gba::Panel` member
    upstream uses; the pipe block and the two feeds in `main.cpp`).
- **Tests**: `gbpk-ui.sh`, as `sync-times-ui.sh` (a fixture from `ui/tests/fixture.sh`, both sizes, asserting on the
  log; `KIWAMI_GBPK_DIR` set to the repository's data folder):
  - the rows: `banana-ui: kiwami: rows in Second screen: kiwami_pokemon_companion_gb kiwami_pokemon_companion_gbc`,
    both Off on a fresh `settings.conf`, and A on the first writes `kiwami_pokemon_companion_gb = on`;
  - `gbpanel 1 kiwami/tests/gbpk/fixtures/crystal; gbfeed ...; gbahost; gshot party`: `gbahost: ready Crystal view
    pages:party`, `gbfeed`'s line with party 2, battle 0, and the shot not blank;
  - `crystal_battle`: `view pages:battle` after the feed, and `pages:party` after feeding `crystal` again;
  - a made-up ROM with Crystal's title and another SHA-1: `gbahost: unsupported`, and `gbfeed` asks 0 reads;
  - the Menu corner switches to the tiles and back (`gtouch`, `view game`, then `pages:party`);
  - upstream's `gamepanel` verb with no GBA ROM still logs `companion none`;
  - after a `gbpanel` on a Gen I stand-in game is released, `banana-ui: gb companion: rules off` is logged (the
    host's reset), and `gbpk-units` checks `nds::effectiveness(GHOST, PSYCHIC)` is upstream's 8 again after it.
  `make test` passes, and its renders match the previous step's (the rows are in Second screen, which no upstream shot
  shows; if one does, the final check's comparison with `main` covers it with the rows' presence accepted).
- **Commit**: `GB Pokémon companion: the pages on the game panel for Gold, Silver and Crystal, behind two Settings rows`

### 9. Gen-aware pages

- **Files**:
  - `ui/src/kiwami/gbpk/pages-decl.inc`, `pages-impl.inc` (new)
  - `ui/src/nds/pages.cpp` (hook, 10 lines)
  - `kiwami/tests/gbpk-ui.sh` (changed)
  - `docs/FORK.md` (changed)
- **Change**: the fork's blocks as members of `Pages::Impl`, drawn with its own helpers (`text`, `textR`, `bar`,
  `fill`, `box`, `heading`, `typeChip`, `pixels`, `measure`, `tr`). They include only `gbpk.h` and `rules.h`
  (header-only). With `KIWAMI_GBPK_TRACE=1` each block logs one line a draw (`banana-ui: gb companion: pages: <block>
  <what it drew>`), for the tests.
  - `bool kiwamiOld() const`: the ROM's `game.gen` is `GEN_I` or `GEN_II`.
  - `bool kiwamiWild(const Mon& f, float y, float bottom)`: false for other games. Else the right-hand box: "DVs" as
    five columns (HP, Atk, Def, Spe, Spc; 15 the top), and in Gen II Hidden Power's type and power from `rules.h`.
  - `bool kiwamiSummary(const Mon& m, float tx, float tw, float ly)`: false for other games. Else, for a Pokémon that
    isn't an egg: in Gen II the Item row alone, in Gen I nothing.
  - `bool kiwamiNoFriendship(const Mon& m) const`: true in Gen I unless the species is Pikachu (25) in Yellow.
  - `bool kiwamiStats(const Mon& m, float x, float& y, float w)`: false for other games. Else the Stats heading with
    the columns "DV" and "Stat exp.", the rows HP, Attack, Defense, Speed and, in Gen I, Special, in Gen II Sp. Atk
    and Sp. Def (both with the Special DV and stat experience); DV as a number and a bar over 15, stat experience as a
    bar over 65535 and its number; no total line. `y` advanced as upstream's block does.
  - `bool kiwamiNoAbilities() const`: `kiwamiOld()`.
  - `bool kiwamiBaseStats(const Species& sp, float x, float& y, float w)`: true in Gen I only: the five base stats
    with Special once, and the total of the five.
  - **Hooks in `pages.cpp`**:
    1. inside `struct Pages::Impl`, before its closing `};` (`:1069`):
       `#include "../kiwami/gbpk/pages-decl.inc"	// kiwami: Gen I/II's blocks (the GB Pokémon companion)`
    2. at the file's end, before the namespace's closing brace: `#include "../kiwami/gbpk/pages-impl.inc"	// kiwami:`
    3. the Battle page's wild box, before `box("bubble", 360, y, 268, bottom - y);` (`:1497`):
       `if (kiwamiWild(f, y, bottom)) return;	// kiwami: DVs, not a nature and IVs`
       (the function ends after this `if (wild)`/`else`, so returning is the same as skipping to its end)
    4. the Party page's summary, before `if (!m.egg)` (`:1622`): `if (kiwamiSummary(m, tx, tw, ly)) {} else	// kiwami: no Nature or Ability`
    5. before the Friendship block's `{` (`:1651`): `if (kiwamiNoFriendship(m)) {} else	// kiwami: Gen I has none`
    6. before `y = heading(tr("Stats"), x, y);` (`:1661`): `if (!kiwamiStats(m, x, y, w)) {	// kiwami: DVs and stat experience`
    7. after the Effort points total's block, before `// moves with PP` (`:1690`): `}	// kiwami:`
    8. the Dex page, before `text(tr("Ability"), false, 11, tx, ay, p.muted);` (`:2480`):
       `if (kiwamiNoAbilities()) ay -= 15; else	// kiwami: no abilities`
    9. before `y = heading(tr("Base stats"), x, y);` (`:2492`): `if (!kiwamiBaseStats(*sp, x, y, w)) {	// kiwami: Gen I's Special`
    10. after that block's `y += 26;` (`:2506`): `}	// kiwami:`
    Upstream's lines between 6-7 and 9-10 keep their indentation.
  - Left as upstream's: the Battle page's empty-state help text names "nature and IVs" (`:1389`); the Team page's
    "Effort points here" draws nothing when no species has an EV yield, which is the case here.
  - **`docs/FORK.md`**: the pages' hooks in point 3: the ten lines by the statement each sits beside, and that a helper
    upstream renames in `Pages::Impl` is followed in the two `.inc` files.
- **Tests**: `gbpk-ui.sh`, with `KIWAMI_GBPK_TRACE=1`, at both sizes:
  - `crystal`, Party page: `pages: summary item` and `pages: stats dv statexp 6 rows` logged; the shot differs from
    the same page drawn with the trace's blocks off (`KIWAMI_GBPK_PAGES=0`, a test switch that makes `kiwamiOld()`
    false);
  - `crystal_battle`, Battle page: `pages: wild dvs hidden-power`;
  - the Dex page: `pages: no abilities`;
  - a tap on a party row and a drag on the summary still scroll (upstream's `gtouch`).
  `make test`: upstream's `gba-pages-test` and `nds-pages-test` pass and their PNGs match the previous step's (for
  other games every block returns false).
- **Commit**: `GB Pokémon companion: the pages show DVs, stat experience and Special, and no natures or abilities`

### 10. Crystal's guide

- **Files**:
  - `ui/src/kiwami/gbpk/guide-games.inc` (new)
  - `ui/src/nds/guide.cpp` (hook, 1 line)
  - `ui/src/kiwami/gbpk/reader.cpp`, `host.cpp` (changed)
  - `kiwami/tools/pokedaisy-extract.py` (changed)
  - `kiwami/rootfs/usr/share/banana/companions/gb-pokemon/crystal.json` (new), `crystal.ram.json`, `SOURCES` (changed)
  - `kiwami/tests/gbpk-units.cpp` (changed)
  - `docs/FORK.md`, `kiwami/GB-POKEMON.md` (changed)
- **Change**:
  - **Hook**, `guide.cpp`, inside `GAMES` after the last row (`:27`):
    `#include "../kiwami/gbpk/guide-games.inc"	// kiwami: the GB and GBC Pokémon games (their files in gb-pokemon/)`
    The file holds six rows with the ids as literals (`{ (GameId)105, "crystal", "BYTE", 251 }`, ... `"red"`,
    `"PKRD"`, 151), so `guide.cpp` needs no fork header. Red, Blue and Yellow's codes are the fork's own (the carts
    have none); Gold's and Silver's are their headers' (`AAUE`, `AAXE`).
  - **The script** converts `GuideCrystalGen.kt` and `GuideCrystal.kt` to `crystal.json` (schema 1,
    `nds-pokemon-guide`, `game` `crystal`, `code` `BYTE`):
    - `places`: each area by its name, no zone ids (the loader's resolver finds them in the ROM's landmark names);
    - `milestones` from the bosses in story order: `kind` (gym, rival, elite4, champion, story), `foe` `{ name, class,
      ace_level }`, `level_cap` from the team's top level, `done_when` `badge:N` or `flag:N` (renumbered), `opens`
      from the areas the boss unlocks; no teams (the ROM has them);
    - `secrets` from the hidden items: the place, x, y, `condition` empty, `text` naming the item by PokeDaisy's own
      words where it has them, else "A hidden item" (always listed: *Refinements*);
    - `notes` from `GuideCrystal.kt`'s FAQ answers, in PokeDaisy's words;
    - `sources`: PokeDaisy and its commit.
    It also writes `crystal.ram.json`'s `flags`: the banks the milestones' `done_when` name (the event flags), each
    with its address, length and first bit.
  - **`reader.cpp`**: each bank in `flags` read with the set, in chunks of at most 256 bytes, laid into `Live::flags`
    at the bank's first bit.
  - **`host.cpp`**: `nds::loadGuideFile(dataDir() + "/" + key + ".json", gd, game.id, &why, &resolver)` with
    `nds::guideResolver(rom)`; no file or a refused one logs `gb companion: no guide: <why>` and the pages go on
    without.
- **Tests**: `gbpk-units`:
  - `crystal.json` loads through `nds::loadGuideFile` with a stand-in resolver and `why` empty (no entry skipped),
    with 16 badge milestones among its milestones;
  - with `Live::badges` = 0 the next milestone is the first; with badge 1 set, the one after Falkner's;
  - a flag bit set in `Live::flags` marks its `flag:N` milestone done;
  - the `crystal` fixture's `Live::flags` is the bank's length.
  `make test`: upstream's `guide-test` passes unchanged.
- **Commit**: `GB Pokémon companion: Crystal's guide, converted from PokeDaisy's tables`

### 11. Gold and Silver's guide, built from the ROM

- **Files**: `ui/src/kiwami/gbpk/gsguide.h`, `gsguide.cpp` (new); `ui/src/kiwami/gbpk/host.cpp` (changed);
  `kiwami/tools/gbpk-symbols.txt`, `kiwami/tools/gbpk-extra.json`, `gold.ram.json`, `silver.ram.json`, `SOURCES`
  (changed); `kiwami/tests/gbpk-units.cpp`, `kiwami/tests/gbpk-roms.cpp` (changed); `kiwami/GB-POKEMON.md` (changed)
- **Change**: `nds::GuideData romGuide(const nds::RomData& rom)`, the fork's own version of upstream's removed
  `guideFromRom` (commit a103f12e, MIT): the trainers whose class is LEADER, ELITE FOUR or CHAMPION, sorted by their
  team's top level; a milestone each (`kind` gym, elite4 or champion; `name` the trainer's; `levelCap` the top level;
  `trainerIds` its id; the Elite Four and the Champion `doneWhen` `later`). A leader's `badge` is not its place in
  that order: the games' badge bits are fixed, and by level Chuck (30) and Pryce (31) come before Jasmine (35) while
  her badge's bit comes before Pryce's, and Kanto's leaders differ more. It comes from a table in `gsguide.cpp` of the
  sixteen leaders' trainer class ids with each one's badge bit (1-8 Johto's, 9-16 Kanto's; the ids from pret's
  pokegold constants, through `gbpk-extra.json`), and `doneWhen` is `badge:N` with that bit. A leader class not in
  the table gets `later`. No places, secrets or notes. `host.cpp` uses it for `gold` and `silver`, which have no file.
- **Tests**: `gbpk-units`: a stand-in `RomData` with three leaders, one Elite Four member and a Champion out of order
  gives five milestones in level order, each leader's badge the table's and not its place (two leaders whose level
  order and badge order differ), and the second is next once the first's badge is set.
  `gbpk-roms` (with `GB_ROMS`): Gold's guide has 16 gyms, 4 Elite Four and 1 Champion, the first Falkner; Jasmine's
  milestone is `badge:6` and Pryce's `badge:7`.
- **Commit**: `GB Pokémon companion: Gold and Silver's gyms, Elite Four and Champion as a guide from the ROM's trainers`

### 12. Gen I's memory: Red, Blue and Yellow

- **Files**:
  - `kiwami/tools/pokedaisy-extract.py`, `kiwami/tools/gbpk-symbols.txt`, `kiwami/tools/gbpk-extra.json` (changed)
  - `kiwami/rootfs/usr/share/banana/companions/gb-pokemon/{red,blue,yellow}.ram.json` (new), `SOURCES` (changed)
  - `ui/src/kiwami/gbpk/reader.cpp`, `items.inc`, `text.cpp` (changed)
  - `kiwami/tests/gbpk/fixtures/` (new: the 16 Gen I captures, `red*`, `blue*`, `yellow*`)
  - `kiwami/tests/gbpk-units.cpp` (changed)
  - `kiwami/DEPENDENCIES.md` (changed: the rows for `pret/pokered` and `pret/pokeyellow`, first read here; *Deviations*,
    2026-11-11)
- **Change**:
  - The script reads `Gen1Reader.kt`'s configs (Red and Blue share one; Yellow's own) and writes the three files, with
    the header titles `POKEMON RED`, `POKEMON BLUE`, `POKEMON YELLOW` and the releases' SHA-1s. Their release lines,
    `pokered`, `pokeblue` and `pokeyellow`, go in `gbpk-symbols.txt`, so the titles and SHA-1s come from pret's
    Makefiles and `roms.sha1` as Gen II's do (step 1's deviation).
  - **A set for Gen I**: the party (count, species list, 6 x 44 bytes, nicknames, OT names), the player, the one bag
    list, the map and coordinates, `wIsInBattle`, `wBattleMon` and `wEnemyMon` with the enemy party in a trainer
    battle, the stat modifiers, the Pokédex, the badges, the play time, and in Yellow Pikachu's happiness.
  - **`live()` for Gen I**: species through the ROM's internal-index-to-Dex table (from `rom`; without one the index is
    passed as it is); `stats` with Special as both Sp. Atk and Sp. Def; no items held, `friendship` 0 but Yellow's
    Pikachu's; the bag split into `nds::Pocket`s by `items.inc`'s Gen I table (balls, TMs and HMs, key items, the
    rest); `timeOfDay` `ANY`; `zone` the map's number; `female` false and no shininess.
  - `items.inc`: Gen I's table (TM01-50 as 328-377, HM01-05 as 420-424).
  - `text.cpp`: Gen I's character set where it differs.
- **Tests**: `gbpk-units` replays the 16 captures and asserts each README's party, place and battle (`red`: its
  trainer, starter and map; `red_battle`: a wild battle with both battlers' levels and HP; `yellow`: Pikachu with its
  happiness), and that `POKEMON RED`'s header is now `KNOWN`.
- **Commit**: `GB Pokémon companion: Red, Blue and Yellow's memory read into the pages' Live`

### 13. Gen I's ROM: the tables

- **Files**: `ui/src/kiwami/gbpk/rom1.cpp` (new); `rom.cpp` (changed); `kiwami/tools/pokedaisy-extract.py`,
  `gbpk-extra.json`, the three Gen I `.ram.json`, `SOURCES` (changed); `kiwami/tests/gbpk-units.cpp`,
  `gbpk-roms.cpp` (changed)
- **Change**: `readRom` for Gen I, from the data files' `rom` section (PokeDaisy's `Gen1Dex.kt` and `Gen1Art.kt`
  where it reads them, pret's symbols for the rest):
  - the internal-index-to-Dex table, kept in the `RomData` for the reader (`uint16_t national(uint8_t internal)`);
  - `species` 1-151: name, types (Gen I's ids to `nds::Type`), base stats with Special in both `base[4]` and `base[5]`,
    catch rate, growth, the learnset (the level-1 moves and the level-up list), evolutions, TM/HM numbers (HM01-05 as
    93-97), Pokédex category, height, weight (imperial in the ROM, converted) and text;
  - `moves` 1-165, the category by the type; `items` at `items.inc`'s ids with the ROM's names; `tmMoves`;
  - `zones` by map number: the map's name and its grass and water tables (ten slots each with the games' fixed
    percents) and the rods;
  - `trainers`: class and team, keyed by class << 8 | number; a class's name is its trainer class name (a gym leader's
    class is its own name, so the guide's resolver finds "Brock").
- **Tests**: `gbpk-units`: a made-up ROM with three species out of Dex order, one map and one trainer, read back; the
  reader's `live()` with it maps an internal index to its national number. `gbpk-roms` (with `GB_ROMS`): Red, Blue and
  Yellow each name 151 species and 165 moves; Bulbasaur's base stats and types; Route 1's grass has Pidgey and
  Rattata; Brock's team is Geodude Lv 12 and Onix Lv 14 (Yellow: Lv 10 and Lv 12).
- **Commit**: `GB Pokémon companion: Red, Blue and Yellow's species, moves, items, encounters and trainers from the ROM`

### 14. Gen I's ROM: the sprites and the town map

- **Files**: `ui/src/kiwami/gbpk/pics.cpp`, `maps.cpp`, `rom1.cpp` (changed); `pokedaisy-extract.py`, the three Gen I
  `.ram.json`, `SOURCES` (changed); `kiwami/tests/gbpk-units.cpp`, `gbpk-roms.cpp` (changed)
- **Change**:
  - `pics.cpp`: `bool pic1(const uint8_t* p, size_t n, int& wTiles, int& hTiles, std::vector<uint8_t>& out)`, Gen I's
    picture compression (two bit planes, run-length and pair packets, the three decode modes with delta and XOR),
    refusing a size over 7x7 tiles or a stream that runs out; a species' front picture as an `nds::Sprite` with the
    species' Super Game Boy/GBC palette from the ROM (Yellow's GBC palettes; Red and Blue's SGB ones), `shiny` equal
    to `normal`.
  - `maps.cpp`: the town map's run-length tile map and tiles into one `nds::RegionMap` ("Kanto"), each cell named
    from the games' town map entries (the external and internal map lists), and `Location::spot` for each map.
- **Tests**: `gbpk-units`: `pic1` on a hand-built 1x1-tile stream in each of the three modes (expected bytes in the
  test), and a truncated one refused; a made-up run-length tile map gives the expected cells. `gbpk-roms` (with
  `GB_ROMS`): all 151 sprites decode and are written as PNGs with the map; Pallet Town's cell names it.
- **Commit**: `GB Pokémon companion: Gen I's sprites and town map, decoded from the ROM`

### 15. Red, Blue and Yellow's guides

- **Files**: `kiwami/tools/pokedaisy-extract.py` (changed);
  `kiwami/rootfs/usr/share/banana/companions/gb-pokemon/{red,blue,yellow}.json` (new), the three Gen I `.ram.json`,
  `SOURCES` (changed); `kiwami/tests/gbpk-units.cpp` (changed); `kiwami/GB-POKEMON.md` (changed)
- **Change**: the script converts `GuideGen1Gen.kt` (per build) and `GuideGen1.kt` as step 10 does Crystal's, plus:
  - of Gen I's four flag banks (event, missable objects, hidden items, in-game trades), those a milestone's
    `done_when` names into each file's `flags`, back to back, and every `flag:N` renumbered from PokeDaisy's 0x10000
    ranges to its bank's first bit;
  - secrets from the hidden items (always listed) and the in-game trades as notes;
  - the item descriptions PokeDaisy wrote itself (`ItemDescriptionsYellow.kt`) as the secrets' texts where one names
    the item.
  The reader needs no change (it reads whatever banks the file lists).
- **Tests**: `gbpk-units`: the three files load with `why` empty; eight badge milestones each; a rival fight's flag
  set in its bank marks its milestone done; the highest `flag:N` in each file is inside the banks' total length.
- **Commit**: `GB Pokémon companion: Red, Blue and Yellow's guides, converted from PokeDaisy's tables`

### 16. PokeDaisy's reference data for every mainline game

- **Files**: `kiwami/tools/pokedaisy-extract.py` (changed); `kiwami/data/pokedaisy/` (new): `LICENSE`, `NOTICE`,
  `SOURCES`, `README.md` (the index), and a folder a game (`red-blue`, `yellow`, `gold-silver`, `crystal`,
  `ruby-sapphire`, `emerald`, `firered-leafgreen`, `platinum`, `heartgold-soulsilver`, `black-white`,
  `black2-white2`); `kiwami/tests/ci.sh` (changed); `kiwami/DEPENDENCIES.md`, `kiwami/GB-POKEMON.md` (changed)
- **Change**: `pokedaisy-extract.py <checkout> --reference` writes, for each game's English releases, JSON files of
  the facts PokeDaisy holds: RAM addresses and layouts, ROM table locations, flag ids, the guides' structure by ids
  (bosses with the flags that mark them beaten, hidden items and trades with their flags and coordinates), and
  PokeDaisy's own words (FAQ answers, its item descriptions). Sources, in the investigation's order: `gamedata/gen3/
  *.toml` and `gamedata/nds/` first, the generated `*Gen.kt` next, the hand-written configs by the evaluator.
  - Left out by rule, each rule a list in the script: tables whose names mark games' content (species, move and item
    names, Pokédex and item text in the games' words, wild tables, trainer teams, base stats, gender ratios, growth
    rates), every non-English file (`*TextDeGen.kt`, `GameText*Gen.kt`, the ports), and the hacks.
  - What the script meets and can't classify is listed in `README.md` under "Not extracted", with the file's name.
  - `README.md` lists each game's files and what each holds, with the attribution. Nothing under `kiwami/data/` is
    copied into the image (the overlay is `kiwami/rootfs/`).
  - `--check` covers these files too (their hashes in `kiwami/data/pokedaisy/SOURCES`).
- **Tests**: `ci.sh`: `--check` passes; every file parses as JSON; a scan of the outputs for strings that must not be
  there (a list of twenty species, move and item names in capitals and in title case, such as `BULBASAUR`,
  `Thunderbolt`, `MASTER BALL`) finds none outside the guides' `foe` and `places` names; and
  `find kiwami/rootfs -path '*pokedaisy*'` is empty.
- **Commit**: `GB Pokémon companion: PokeDaisy's reference data for the mainline games, Gen I to Gen V`

### 17. The documentation, and what the feature weighs

- **Files**: `kiwami/GB-POKEMON.md`, `docs/FORK.md`, `CLAUDE.md`, `kiwami/DEPENDENCIES.md` (changed)
- **Change**:
  - `kiwami/GB-POKEMON.md` completed: the games and releases, how the companion turns on, what is read from memory
    and from the ROM, the DS conventions it keeps, the rules, the guides, the tests (with `GB_ROMS`), how to take in a
    newer PokeDaisy (re-run, diff, `SOURCES`), and the footprint measured here: banana-ui's size before and after
    (`size ui/banana-ui` on this branch and on `kiwami`), and the data folder's size.
  - `docs/FORK.md`: *Where a customization goes* gets the rows for `kiwami/tools/`, `kiwami/data/` and
    `ui/src/kiwami/gbpk/` (GPL-3.0, its own `LICENSE`/`NOTICE`, header-only files for the `nds/` hooks). Point 3's
    `git diff $OLD main --` list gains `ui/src/nds/ ui/src/gba/ ui/src/pokemon.h ui/src/pokemon.cpp`, with what to
    look for: a member added to `nds::Live`, `Mon`, `RomData`, `PageData` or `gba::Panel`; a new use of `Gen` in the
    logic or the pages; and upstream adding a GB/GBC companion of its own, which retires the fork's.
  - `CLAUDE.md`: one paragraph under *Layout*: `ui/src/kiwami/gbpk/` and its licence, and that banana-ui as the fork
    builds it is GPL-3.0.
- **Tests**: `kiwami/tests/ci.sh .buildroot`; the hook commands of `docs/FORK.md` list the hooks this plan names.
- **Commit**: `GB Pokémon companion: its documentation, the merge checks for upstream's companion code, and its footprint`

### 18. Final check

- `make test`, `make -C ui`, `make -C system/bananad CFLAGS="-O2 -Werror"`, `kiwami/tests/ci.sh .buildroot`.
- The fork's tests: `gbpk-units`, `gbpk-ui.sh`, `gbpk-roms.sh` with `GB_ROMS` set to the user's folder (and the
  PokeDaisy checkout), `pokedaisy-extract.py --check` against the checkout.
- `kiwami/tests/ui-objects.sh`: the object build is still byte for byte the one call's.
- The hooks: `git diff --stat main...HEAD` and `git grep -n 'kiwami:'` (the commands in `docs/FORK.md`) agree, and
  the new ones are exactly: `main.cpp` 10, `pages.cpp` 10, `logic.cpp` 4, `model.cpp` 2, `settings.h` 2, `model.h` 1,
  `guide.cpp` 1, `settings.cpp` 1.
- With both rows Off, `make test`'s renders in `ui/tests/out/` match `main`'s (a second checkout), and its bench lines
  are within noise of `main`'s.
- Nothing of the games in the repository: step 16's string scan over `kiwami/rootfs/usr/share/banana/companions/
  gb-pokemon/` and `kiwami/data/pokedaisy/`.
- Every file in `ui/src/kiwami/gbpk/` has the GPL header and the 7(b) line (`grep -L`), and the four `LICENSE`/`NOTICE`
  pairs are there (A20).
- `kiwami/DEPENDENCIES.md` lists PokeDaisy at `135ac809`.
- `docs/FORK.md`, `CLAUDE.md` and `kiwami/GB-POKEMON.md` say what the feature changed.
- A fresh `rgds-plus` image (`make clean-rgds-plus`, then `make rgds-plus`): the data folder and the credits line are
  in it, `kiwami/data/` isn't.
- The manual tests below all passed.
- Every acceptance criterion is met: each is listed by at least one automated or manual test, and those passed.
- The pull request's text.

## Automated tests

| Test | Where | What it checks | Criteria |
|---|---|---|---|
| The extraction's check | `kiwami/tools/pokedaisy-extract.py --check`, in `ci.sh` | the committed data is the script's output (hashes in `SOURCES`); every file parses | A21, A22 |
| No game content | `ci.sh` (steps 16, 18) | the string scan over the image's data and the reference data; nothing of `kiwami/data/` in the overlay | A12, A21, A22 |
| Credits | `ci.sh`, `post-build.sh` on a stand-in target | the 7(b) line once in `credits.txt`, the same on a second run, a stop without `[Thanks]` | A20 |
| Detection | `kiwami/tests/gbpk-units.cpp` | header titles, an unknown SHA-1 unsupported, another title none, the data files | A3, A4 |
| The RAM reader | `gbpk-units.cpp` over `kiwami/tests/gbpk/fixtures/` (33 captures) | each README's party, bag, place and battle; the request pacing; error replies | A7, A8, A9 |
| The rules | `gbpk-units.cpp` through `nds::effectiveness`, `damage`, `catchChance`, `Mon::shiny` | Gen I's chart, Special, the type items, no status counted twice, both catch formulas, DV shininess, gender, Hidden Power | A10 |
| The ROM reader on made-up data | `gbpk-units.cpp` | every table read back from a made-up ROM for each generation; both decompressions on hand-built streams; the tile maps; refusals | A12, A13, A14 |
| The ROM reader on the user's ROMs | `kiwami/tests/gbpk-roms.sh` (`GB_ROMS`, by default `~/gb-companion/roms`, skipped without; the files in `kiwami/tests/gbpk/roms.sha1`) | the nine releases' SHA-1s, species, moves, encounters, trainers, every sprite, the maps; PokeDaisy's reference tables | A3, A12, A13, A14, A16 |
| The guides | `gbpk-units.cpp` through `nds::loadGuideFile` | the four files load with nothing skipped; milestones follow badges and flags; hidden items' flags; the ROM-built guide | A15, A16 |
| The panel, headless | `kiwami/tests/gbpk-ui.sh` at 640x480 and 1024x768 | the rows and their keys; the pages on a fixture, the battle's start and end, the Menu switch, unsupported; the Gen-aware blocks' trace lines | A1, A3, A4, A9, A11 |
| Upstream unchanged | `make test` | upstream's GBA, DS, guide and logic tests pass; renders match `main`'s with the rows Off | A2, A23 |
| The build | `kiwami/tests/ui-objects.sh`, `make -C ui`, `ci.sh .buildroot` | the object build equals the one call; the boards configure | (the tree) |

## Manual tests

On the device, for what headless tests can't reach. Logs are fetched over SSH/SFTP (Settings > Network > SSH) or from
the card on a computer (`banana/logs/` on the card is `/data/banana/logs/` on the device). Only an RG DS Plus is
available, so the RG DS's 640x480 panel is covered headless only (`gbpk-ui.sh`).

Each run: Second screen on, the system's row On, the game started from the menu. Collect
`/data/banana/logs/banana-ui.log` after quitting the game.

| # | Board | Steps | Expected | Logs to collect | Criteria | Outcome |
|---|---|---|---|---|---|---|
| M1 | rgds-plus | After step 8. Both rows Off: start Crystal. Then the `gbc` row On: start Crystal from Game Boy Color; start it from Game Boy with only the `gbc` row On (the file copied there). | Off: the tiles as before, no `gb companion` line. On: "Reading your game", then Party. From Game Boy: no companion. | `banana-ui.log`: `gb companion: Crystal`, `read in N ms`, `states handed to the logic` | A2, A3, A5 | not run |
| M2 | rgds-plus | Crystal, a save with a party: look at Party, Dex, Here and Map against the game's own screens. Walk into another map. | Names, levels, HP, moves, held items and the place match the game. Sprites in colour. The Pokégear's map with the place lit, moving with the player. | `banana-ui.log`; a photo of each page beside the game | A7, A12, A13, A14 | not run |
| M3 | rgds-plus | Crystal: a wild battle with balls in the bag, then a trainer battle. Put the wild one to sleep, lower its HP. Use a move the page marks as a KO. | Battle page at the battle's start, Party at its end. Damage ranges hold; the catch chance rises with sleep and low HP; the trainer's team shown, no catch box. DVs shown, no nature. | `banana-ui.log` | A9, A10, A11 | not run |
| M4 | rgds-plus | Crystal: Guide page on a save before the first gym, beat Falkner, look again. Stand on a route with a hidden item. | The next milestone is Falkner, then the one after. The Here page lists the hidden item at its coordinates. | `banana-ui.log` (`no guide` must not appear) | A15 | not run |
| M5 | rgds-plus | Gold and Silver: as M2 and M3, briefly, and the Guide page. | As Crystal; the guide lists the gyms from the ROM, Falkner first. | `banana-ui.log` | A3, A7, A16 | not run |
| M6 | rgds-plus | After step 15. Red, Blue and Yellow: Party, a wild battle, a trainer battle, Dex, Map, Guide. In Yellow, Pikachu's summary. | One Special row, DVs and stat experience, no Item or Friendship row (Yellow's Pikachu has Friendship). The town map with the place lit. The guide's next milestone is Brock on a new save. | `banana-ui.log` | A8, A9, A10, A11, A13, A14, A15 | not run |
| M7 | rgds-plus | Each of the nine releases the user has, in `.gb`/`.gbc`, and one as `.zip` and one as `.7z`. | Each opens the companion. | `banana-ui.log`: one `gb companion: <title>` each | A3 | not run |
| M8 | rgds-plus | A European Yellow or a ROM hack of one of the six, if the user has one; and Tetris. | "Not supported" alone; Tetris gets no companion. | `banana-ui.log`: `not a release the companion knows (sha1 ...)` | A4 | not run |
| M9 | rgds-plus | Crystal with Settings > Other systems > Frontend on Auto (BananaFront), then on RetroArch; with the Game Boy Color's core set to gambatte, then SameBoy. Four runs, a battle in each. Then a fifth with RetroAchievements' Hardcore mode on (signed in), which sends the game to RetroArch. | The pages follow the game in the four. In Hardcore mode: either the same, or the panel says RetroArch can't read the game's memory; which one is recorded here and in `kiwami/GB-POKEMON.md`, and neither fails the test (whether RetroArch answers reads in Hardcore mode isn't known from upstream's code). | `banana-ui.log`: `the emulator attached to its memory export` with BananaFront, not with RetroArch; no `can't read the game's memory` | A18 | not run |
| M10 | rgds-plus | Crystal, in a wild battle: close the lid, wait a minute, open it. Fight on. | The game and the pages resume; the pages follow the next turn. | `banana-ui.log`, `/data/banana/logs/bananad.log` around the sleep | A19 | not run |
| M11 | rgds-plus | Crystal with the frame counter on (RetroArch's, Frontend on RetroArch): a minute of walking with the row Off, then On. Also BananaFront by eye for stutter. | The same frame rate both ways; no stutter when the pages update. | `banana-ui.log`; the two frame rates noted | A19 | not run |
| M12 | rgds-plus | Challenge mode On (upstream's row): Crystal, catch the first Pokémon on a route, let one faint. Quit, start again. | The Here and Log pages keep the first encounter and the lost Pokémon across the restart. | `banana-ui.log`; the files in `/data/banana/cache/gb-companion/` | A17 | not run |
| M13 | rgds-plus | A user definition for Red in `/data/banana/companions/gb/` with Game companion On and the `gb` row On (only if the user has one to hand; otherwise by the code's reading at review). | Upstream's game companion shows, not the fork's. | `banana-ui.log`: `game companion:` and no `gb companion:` | A6 | not run |
| M14 | rgds-plus | Settings > Other systems > Second screen, and Settings > System > Credits. An Emerald game with upstream's row On. | The two rows after Pokémon companion, Off on a fresh card. The PokeDaisy line in the credits. The GBA companion as before. | none | A1, A20, A23 | not run |

Notes from the runs (what the logs showed, what was fixed):

## Deviations

Where the implementation turned out different from this plan, why, and what changed (dated).

- **2026-11-11, step 1: the ROM header's facts come from pret, and the game code is per release.** pret's Makefiles
  give `rgbfix` each release's header title (`-t`) and game code (`-i`), and Crystal's Australian release has its own
  code, `BYTU`, not `BYTE`. So `pret-extract.py` reads both from the Makefile instead of the plan's values from memory
  (which they confirm: `PM_CRYSTAL`, `POKEMON_GLD`/`AAUE`, `POKEMON_SLV`/`AAXE`), and the data file's `code` moves from
  the top level into each release (`releases: [{sha1, name, code}]`); `header_title` stays per game. Gen I's releases
  have no code. Agreed with the user.
- **2026-11-11, step 1: the releases are lines of `gbpk-symbols.txt`.** `<game> release <repo> <build> <name>` names a
  release, its SHA-1 is read from the repository's `roms.sha1` and must be the set PokeDaisy's `GameTitles.kt` gives
  the game, and every symbol is looked up in each release's `.sym` (Crystal's three must agree, which they do). The
  `.sym` files are read with `git show symbols:<file>`, so no branch is checked out. `SOURCES` has a section per
  script, `[pokedaisy]` and `[pret]`, each written and checked by its own script. Agreed with the user.
- **2026-11-11, step 1: more is cross-checked than planned.** `gbpk-symbols.txt` also lists pret's symbol for every
  address PokeDaisy's `Gen2Config` gives, and every one agrees; Crystal's guide RAM in PokeDaisy's `GuideCrystalGen.kt`
  (event flags, time of day, badges, trade flags) is read too. Each `ram` value records its source (`src`), and
  `sizes.eventFlagBytes` is pret's `NUM_EVENTS` / 8 (256), checked against PokeDaisy's for Crystal.
- **2026-11-11, step 1: `pret/pokered` and `pret/pokeyellow` get their `kiwami/DEPENDENCIES.md` rows in step 12**, where
  they are first read and their commits first pinned in `SOURCES`, as `CLAUDE.md` asks (a dependency's row in the
  commit that adds it). Step 1 adds PokeDaisy's, `pret/pokecrystal`'s and `pret/pokegold`'s.
- **2026-11-11, step 1: `kiwami/tests/7z-ui.sh` changes too.** It runs `post-build.sh` on a stand-in target, which
  now needs upstream's `credits.txt` there (the credit stops the build without a `[Thanks]`), so it copies it as
  `ci.sh`'s two stand-ins do.
- **2026-11-11, from step 2: pret's source alone, no Bulbapedia.** Bulbapedia answers automated requests, its API
  included, with 403, and getting past that (a browser's User-Agent) is circumventing its bot check, which wasn't
  done. The user chose pret's source as the only reference for the whole feature: each rule and layout follows the
  games' own code in pret's disassemblies, named in the code, and the rule steps' (4, 6, 13) hand-worked test values
  come from that code. The plan already had pret win where the two disagreed. Step 2's character set is
  `constants/charmap.asm`; its header titles came from pret in step 1.
- **2026-11-11, step 2: made-up archives, and a few shapes.** `kiwami/tests/gbpk/made-up/` (new, not planned) holds a
  made-up 32 KB ROM with Crystal's title, and a `.zip` and a solid `.7z` holding it, made by its
  `make-fixtures.sh` with `bsdtar` as `kiwami/tests/ra/`'s are; `gbpk-units` reads them to check `readRomFile` (the
  header alone, the game past a readme, the first `.gb`/`.gbc` in name order as RetroArch takes it, none in an
  archive without a game) and `confirm` (refused, then known once its SHA-1 is listed). `GameData` has the releases
  with their codes (step 1's deviation) and the confirmed one's index (`release`); `flags` is left to step 10, which
  defines the flag banks. `Addr::core()` gives the address `READ_CORE_MEMORY` takes (banks 2-7 from 0x10000).
- **2026-11-11, step 3: three more symbols, and the step's other shapes.** The battle's trainer (`wOtherTrainerClass`,
  `wOtherTrainerID`, for `Battle::trainerId` as class << 8 | number, the key step 6's trainers use) and the day-care's
  `wStepsToEgg` were missing from step 1's list: `gbpk-symbols.txt` gets them, and both scripts were run again
  (`gbpk-extra.json`, the three data files and `SOURCES` change in this step). `items.inc` maps a Gen II id to its Gen
  IV id only: the bag's pocket is the list an item is in (items, balls, key items, the TM case), not a column of the
  table; besides the plan's items it maps the evolution stones and trade items (King's Rock, Metal Coat, Dragon Scale,
  Up-Grade) for step 6's evolutions, and the contest's Park Ball to Gen IV's Sport Ball (x1.5; Gen IV's Park Ball is a
  sure catch). Each of its 42 lines was checked against pret's ids in both repositories, and the Gen IV ids are
  upstream's own (`ui/src/gba/gen3items.inc`). The reader merges neighbouring reads (10 or 11 a set, about 1.3 KB);
  the event flags aren't read yet (step 10 reads the banks the guides name). The replies are parsed by upstream's
  `pkmn::parseReply`, so `gbpk-units` links `pokemon.cpp` and `pokemap.cpp`, and `model.cpp` for `Mon::shiny()`, which
  the DV tests use already. Our battler is the party member out under the battle's copy (its name, OT and stat
  experience kept), the foe the trainer's party member out under `wEnemyMon`.
- **2026-11-11, step 4: the rules as pret's code has them, with what the plan missed.** Each rule was read from the
  games' code (pokered and pokecrystal: `engine/battle/core.asm`, `effect_commands.asm`, `misc.asm`,
  `hidden_power.asm`, `engine/items/item_effects.asm`, `engine/pokemon/move_mon.asm`, `data/types/`), and the tests'
  values were worked from it. Gen I's chart differences are exactly the plan's (pret's two tables compared), and Gen
  II's chart is upstream's own. Found in the code and added: Gen II's **badge type boosts** (+ 1/8 for the player's
  moves of a badge's type, after the type item, before STAB), which need the player's badges: `rules.h` keeps them in
  an atomic, `setBadges`, which step 8's host sets from each `Live` (upstream's logic calls `damage()` only with our
  battler attacking); Gen II's **Dragon Scale** is Dragon's type item, Dragon Fang does nothing, the Pink and Polkadot
  Bows are Normal's; the stats above 255 are divided by 4 (Gen I once, Gen II until both fit); the catch formulas keep
  the games' bugs (Gen II: burn, poison and paralysis add nothing, the HP part in bytes over 341 HP, the Level Ball
  skips the HP part, the Moon Ball never boosts, the Love Ball wants the same gender, the Fast Ball counts three
  species). Not counted, as upstream's DS logic doesn't: critical hits, Reflect and Light Screen, weather (upstream
  passes none), Explosion's halved Defense. The type matchups are applied in the defender's types' order, not the
  games' table order, which can differ by 1 in a rare dual-type case. `useChart` is `useRules(nds::Gen)` (it sets the
  chart, the rules and clears the badges): **step 8's host calls it when a game's data is read and with an upstream
  `Gen` when the companion closes**, so a GBA or DS game after a Game Boy one gets upstream's rules. The Heavy Ball
  reads `Species::weight` in 0.1 kg, as upstream keeps it: **step 6 fills the weight in 0.1 kg** (the Gen II
  Pokédex's 0.1 lb converted). `docs/FORK.md` also gets `model.h`, `model.cpp` and `logic.cpp` in point 3's `git diff`
  list now, rather than in step 17. `gbpk-units` links `logic.cpp`.

## Progress

A step is marked done in its own commit, so the commit column holds the subject (a commit can't carry its own hash).

| Step | Status | Commit |
|---|---|---|
| 1. PokeDaisy's data: the extraction script, Gen II's addresses, the licence and the credits | done | `GB Pokémon companion: PokeDaisy's Gen II addresses, extracted by a script, with its licence and credit` |
| 2. Telling the games apart, and their text | done | `GB Pokémon companion: the six games told by header title and SHA-1, their data files and character set` |
| 3. Gen II's memory: the reader, checked against PokeDaisy's captures | done | `GB Pokémon companion: Gold, Silver and Crystal's memory read into the pages' Live` |
| 4. Gen I's and Gen II's rules, in the logic | done | `GB Pokémon companion: Gen I's type chart and both generations' damage, speed and catch rules` |
| 5. The ROMs for the ROM tests: the list, the folder and its check | todo | |
| 6. Gen II's ROM: the tables | todo | |
| 7. Gen II's ROM: the sprites and the Pokégear's maps | todo | |
| 8. The companion on the game panel, with its Settings rows | todo | |
| 9. Gen-aware pages | todo | |
| 10. Crystal's guide | todo | |
| 11. Gold and Silver's guide, built from the ROM | todo | |
| 12. Gen I's memory: Red, Blue and Yellow | todo | |
| 13. Gen I's ROM: the tables | todo | |
| 14. Gen I's ROM: the sprites and the town map | todo | |
| 15. Red, Blue and Yellow's guides | todo | |
| 16. PokeDaisy's reference data for every mainline game | todo | |
| 17. The documentation, and what the feature weighs | todo | |
| 18. Final check | todo | |
