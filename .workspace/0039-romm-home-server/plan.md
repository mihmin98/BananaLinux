# 0039 RomM home server: plan

Issue: #39 · Branch: `k/0039-romm-home-server` · Investigation: [investigation.md](investigation.md) · Status: approved

## Summary

The fork takes in upstream's pending Home server rework at its head
`cab127ba708273b46ef40de098609e23a45abc3f` (*the rework*). The merge re-ports the fork's two `banana-sync` patches:
0022 (RomM's list kept small) and 0024 (Show sync times). Then come four additions, each in `banana-sync` and in
Settings:

- **BIOS downloads** from RomM's firmware, into `/data/bios`, and DraStic's folder for the DS's.
- **Lighter lookups**:
  - before a game, the game's RomM id remembered, or found by RomM's name search, instead of listing its whole
    system;
  - in Sync now, ROMs with no match by name are found by their hash.
- **An incremental list.** The slimmed game list is kept on the card and brought up to date with `updated_after`, and
  deletions are found through RomM's id list.
- **Installs from RomM's web page.** Requests are claimed at Sync now and on Wi-Fi connect, and downloaded through the
  rework's Get games list. They are reported back to RomM, behind a setting that is off by default.

The fork's code is header-only, in `ui/src/kiwami/romm/` (as `syncmem.h` and `synctimes.h` are), so `banana-sync`'s
build recipe stays upstream's. Only its dependency line in `ui/Makefile` (the fork's existing `kiwami:` line) grows.
The hooks sit in the rework's `sync.cpp`, `sync_romm.cpp`, `sync_core.cpp` and `settings.cpp`.

**Decided while planning:**

- **Quick resume**: dropped from this feature. The rework already removes a replaced save's resume states
  (`cab127ba:ui/src/save_vault.cpp:148-173`, `dropResumeStates`); the investigation is corrected.
- **The merge**: one merge commit carries both re-ports and their tests.
- **The list cache**: kept on the card, as JSON lines.
- **Wi-Fi with installs**: a run of the games only (`--games-only`) when Auto sync wouldn't sync on Wi-Fi.
- **DraStic's BIOS**: `nds_bios_arm7.bin`, `nds_bios_arm9.bin` and `nds_firmware.bin` in `/data/drastic/system/`.
  These are the names in its binary (`strings`), and the folder `run-drastic:8-17` copies from `/usr/share/drastic/`.

## Acceptance criteria

The rework and the re-ports:

- **A1**: Given step 1's merge, `git merge-base --is-ancestor cab127ba HEAD` holds. `make test`,
  `system/tests/launchers-test.sh`, bananad's `sleep-test.sh` (run as root) and `kiwami/tests/ci.sh .buildroot` pass,
  and `make -C ui` and `make -C system/bananad CFLAGS="-O2 -Werror"` build without warnings.
- **A2**: Given a RomM stand-in with 400 games on each of six systems, each about 25 KB of JSON (RetroAchievements and
  provider blocks, as `romm-big.py` makes them):
  - `banana-sync games`, Sync now with games on Get games' list, and one game's sync before it starts each peak under
    100 MB;
  - the rework's own `banana-sync` (no fork hooks), on the same library, peaks at 256 MB or more;
  - both list the same games.
- **A3**: With `kiwami_sync_times = on`, a full sync writes `sync.times` and statuses for each phase in a known order,
  and `sync.log` ends with each phase's time. A sync of one game (before or after it) never writes `sync.times`. With
  the setting off, none of it happens. Show sync times is the Home server page's last row, for both RomM and Romhost
  (*Deviations*, 2026-11-16).

BIOS:

- **A4**: Given a RomM pairing by the device flow, the requested scopes are the rework's seven plus `firmware.read` and
  `collections.read`.
- **A5**: Given a paired RomM with firmware for nds, gba and psx and for a platform Banana has no system for,
  `banana-sync kiwami bios list` prints one line for each file of the three mapped systems:
  `<id> TAB <system> TAB <file name> TAB <bytes> TAB missing|same|different`. The comparison is against `/data/bios`,
  and for the DS also against DraStic's file.
- **A6**: `banana-sync kiwami bios get <id>...` downloads each file through `<file>.part`, checked against RomM's MD5 (or
  SHA-1), into `/data/bios/<file name>`.
  - A file already there with the same hash is skipped.
  - One that differs is never overwritten, and is reported.
  - The DS's `bios7.bin`, `bios9.bin` and `firmware.bin` (any case) are also written as DraStic's
    `/data/drastic/system/nds_bios_arm7.bin`, `nds_bios_arm9.bin` and `nds_firmware.bin` when that file isn't there.
    The folder is made if needed.
  - The last line is `ok <n> <skipped> <kept>` or `error <why>`.
- **A7**: Given a token without `firmware.read` (an older pairing), `bios list` answers `error scope`, writes nothing,
  and the BIOS page says to pair again (Server > Pair).
- **A8**: Given a RomM pairing, the Home server page has a **BIOS** row, and none for Romhost or unpaired.
  - Its page lists the files by system. Missing files are ticked, A toggles one, and Start downloads the ticked files.
  - The top panel shows the selected file's state and the result.
  - It works at 640x480 and 1024x768, with the touchscreen too.

Lookups:

- **A9**: Given a game whose RomM id was recorded by an earlier listing, the sync before it (`sync --rom <path> --auto
  launch`) makes no `GET /api/roms?platform_ids=` request. It makes one `GET /api/roms/<id>`, and its save syncs as the
  rework's would.
- **A10**: Given a game with no recorded id, the sync before it asks one
  `GET /api/roms?platform_ids=<p>&search_term=<stem>...`, keeps the exact name match, and records the id.
- **A11**: Given a recorded id that no longer names that file (renamed or deleted on the server), or a search with no
  exact match, the sync falls back to the rework's listing of the system, with the same outcome as the rework's.
- **A12**: Given a ROM on the card that no server game matches by name, Sync now hashes it (MD5, archives skipped)
  when a server game of one file on its system has its size, and compares the MD5 with the listing's (*Deviations*,
  2026-11-17: no `GET /api/roms/by-hash`). If one such game has it, with no card file of its own name:
  - its save syncs in that game's slot;
  - the hash is remembered by size and modification time, so the next Sync now doesn't hash it again;
  - two card files with the same hash, or a server game that already has a card file of its name, get no alias, and
    `sync.log` says so.

The incremental list:

- **A13**: Given a full listing kept in `banana/kiwami-romm-list.jsonl`, the next Sync now asks each platform only
  `updated_after=<the server's time of the last listing less 60 s>` pages (*Deviations*, 2026-11-17; with no `Date`
  from the server, the newest updated_at less 60 s), plus one `GET /api/roms/identifiers`:
  - a game added or changed on the server is in the list (Get games' cache, `homesync-server-games.tsv`), where RomM's
    name order puts it;
  - a game deleted from it is gone;
  - a platform not in the cache is listed in full.
- **A14**: A full listing happens instead whenever:
  - there is no cache, or it is another server's;
  - the identifiers request fails;
  - Get games' X (`banana-sync games`) is used;
  - after Forget or a pairing with another server, which also drop the file.

  The cache is read one line at a time: A2's bound holds on the cached path too.

Installs:

- **A15**: Given `kiwami_romm_installs` missing or `off` (the default), nothing of installs happens: no capability
  request and no claim. Turning the row On sends `PUT /api/devices/<id>` with `{"capabilities":
  {"remote_install": true}}`, and Off with `false`. The row shows only for a RomM of 5.4 or newer.
- **A16**: With installs on and requests pending, Sync now claims them (`POST /api/devices/<id>/installs/claim`) and
  maps each to its game by the request's `file_ids` against the manifest's download URLs. Then:
  - each game goes on Get games' list and is downloaded by the same run, to the card "Games go to" names;
  - the request is reported `done` once its files are on the card;
  - a game already on either card is reported `already_installed`, with no download.
- **A17**: A request that can't be met is reported `failed`, with the reason from `homesync-get-failed.txt` or the
  fork's own (500 characters at most). The cases: a game of several files the menu can't start, a system Banana
  doesn't have, the card full. A request whose game waits for the second card while it's out stays `taken`, is
  claimed again at the next sync, and is reported when it's done.
- **A18**: On Wi-Fi connect (`sync --auto wifi`), with installs on and requests pending:
  - with Auto sync Off or "Leaving a game", a games-only run downloads them and syncs no save, and then the rework's
    own Wi-Fi run goes on as before;
  - with "And on Wi-Fi", the rework's run claims and downloads them with the rest;
  - with installs off, the run is the rework's.

Everything:

- **A19**: With the fork's settings at their defaults (Show sync times Off, installs Off), `make test`'s renders in
  `ui/tests/out/` match the rework's own build at `cab127ba`. Upstream's `main` has no rework to compare with.
- **A20**: On the RG DS Plus against the user's RomM 5.4.1:
  - BIOS, installs from the web page, saves shared with Grout or Argosy, and the lighter launch work as above;
  - the time before a game on a 1000+ game system is under 5 s once its id is remembered;
  - a second Sync now with nothing changed reads under a tenth of the first's bytes.

## Risks

- **Performance and footprint.**
  - Nothing runs while idle: claims happen only at Sync now and Wi-Fi connect, BIOS only on request.
  - The menu's frames are unchanged: the BIOS page draws like Settings' other pages, and A19 compares renders.
  - The sync before a game drops from listing a system (up to the 20 s cap) to one or two small requests (A9, A10,
    M5).
  - The list cache costs 10-15 MB on the card for 10,000 games, read one line at a time, so memory stays at A2's bound
    (A14).
  - The hashing is in Sync now only, once per file (A12). A 512 MB DS ROM takes a few seconds once.
- **Conflicts with upstream.**
  - The fork carries the rework ahead of `main`. When upstream merges it unchanged, the merge from `main` brings
    nothing new. If upstream squashes, rewrites or drops it, `docs/FORK.md` says what to do (step 1).
  - The new hooks are in the rework's files (`sync.cpp`, `sync_romm.cpp`, `sync_core.cpp`, `settings.cpp`), each a
    `kiwami:` line listed in `docs/FORK.md`.
  - The tests guard the rework's internals the fork relies on:
    - the download URL format (`/api/roms/<id>/content/`, `/api/roms/<file id>/files/content/`) that installs map by
      (`romm-installs.sh`);
    - `onCard()` (`romm-lookup.sh`);
    - the keys `slim()` keeps (`romm-list.sh`'s guard).
- **RomM's API.** Written against 5.4.1. The fork's stand-in, `kiwami/tests/romm-kiwami.py`, copies 5.4.1's install,
  firmware, by-hash, search, `updated_after` and identifiers behaviour from its source (cited in its header). A RomM
  that doesn't answer them gets the rework's path: a full listing and no installs (A11, A14, A15).
- **`updated_after` holes.** RomM may not bump a game's `updated_at` when only its files change (Argosy's notes). The
  60 s overlap, the id list, X's full refresh and a full listing whenever the cache is missing bound this. M6 looks at
  it on the real server.
- **Saves.** The fork changes no save logic. The aliases (A12) only make `onCard()` return a renamed card file for a
  server game. Ambiguous cases get no alias.
- **Hardware.** Only the RG DS Plus is at hand. Nothing here is board-specific, and the pages are drawn at both sizes.
- **Step 1 is large**: one merge commit with conflict resolution, two re-ports and rewritten tests. It is reviewed as
  the conflict list in *Steps*, 1 plus the fork's diff against `cab127ba` (`git diff cab127ba HEAD -- <the fork's
  files>`).

## Steps

Each step is one commit, and leaves the tree building and the tests passing (`make test`, `kiwami/tests/ci.sh
.buildroot`, `make -C ui`, `-Werror` where C/C++ changes). The tests named in each step are run in it. Long runs are
kept to what the step touches; `make test` runs once per step that changes banana-ui.

### 1. The rework taken in, with 0022 and 0024 re-ported

- **Before**: the user fetches the rework and merges it on `k/0039-romm-home-server`:
  ```sh
  git fetch https://github.com/mjkoo/BananaLinux.git cab127ba708273b46ef40de098609e23a45abc3f
  git merge --no-ff --no-commit cab127ba708273b46ef40de098609e23a45abc3f
  ```
  Claude resolves the conflicts and stages the result. The user commits the merge.
- **Files**:
  - `ui/Makefile` (hook): keep the rework's `banana-sync` rule and recipe. The fork's dependency line names
    `src/kiwami/syncmem.h src/kiwami/synctimes.h $(wildcard src/kiwami/romm/*.h)`, so the later steps' headers are
    covered.
  - `ui/src/main.cpp` (hook): both sides' includes.
  - `ui/src/settings.h` (hook): the `Page` enum is the rework's with `KIWAMI` last; the `Entry` enum is the rework's
    with `ENTRY_KIWAMI` last.
  - `ui/src/settings.cpp` (hook):
    - the rework's Home server rows, with `Kiwami::rows(*this, HS);	// kiwami:` after the Sync now `add` in both the
      RomM and the Romhost branch;
    - the `ENTRY_KIWAMI` help line kept beside the rework's new entries.
  - `ui/src/sync.cpp` (hook): the rework's file, with the fork's hooks:
    - the `#include "kiwami/syncmem.h"` and `"kiwami/synctimes.h"` lines;
    - `synctimes::start(get("kiwami_sync_times", "off") == "on" && onlyRom.empty(), RUN)` after `auto get = ...` in
      `doSync`: one game's sync never times;
    - `say("memory: peak %s", ...)` and the `synctimes::report()` loop after the `done:` line.
  - `ui/src/sync_core.cpp` (hook): `banana::kiwami::synctimes::seen(statusLine);` in `status()`, after
    `statusLine = line;`, plus its include.
  - `ui/src/sync_romm.cpp` (hook): in `Romm::list()` (`cab127ba:ui/src/sync_romm.cpp:103-158`):
    - `syncmem::Listing` over the platforms;
    - `syncmem::PAGE` a page in the loop's step;
    - the query with `limit=PAGE`;
    - `romJson.push_back(banana::kiwami::syncmem::slim(*it))`;
    - the listing's progress status (only on a `running` status);
    - the break on a short page.

    The rework reads `romFiles(*it)` and `entry` from the full item before `slim()`, so the `RommRom` keeps its
    hashes. Also `synctimes::tick()` at the top of the rework's slot listing loop (`listHeads`, "Reading saves") and
    of the states listing.
  - `ui/src/kiwami/syncmem.h` (changed): `slim()` keeps everything the rework's `Romm` reads from an item:
    - scalars `fs_name_no_ext`, `revision` and `updated_at` (for step 6) beside today's;
    - `tags` and `regions` as arrays of their strings;
    - each file's `archive_members` as `{size, crc_hash}` objects.

    The comment names `sync_romm.cpp`.
  - `kiwami/tests/syncmem-units.cpp` (changed): the new keys and arrays.
  - `kiwami/tests/romm-list.sh` (changed):
    - builds `banana-sync` with the rework's sources (read from `ui/Makefile`'s rule, so a source upstream adds is
      included);
    - the guard reads every `str/num/flag/get` key of `sync_romm.cpp`'s `Romm`, `romFiles()`, `romTitle()` and
      `romRegions()`;
    - the runs are `games`, and `sync --games-only` with 20 keys on `homesync-get.txt`;
    - the save-download checks go: the rework's slots are covered by its own `ui/tests/sync-slots.sh`;
    - `--compare <other banana-sync>` takes the rework's own build.
  - `kiwami/tests/romm-big.py` (changed): heartbeat 5.4.1, `GET /api/devices/<id>` (the rework's device check), and
    `updated_at` on each game (for step 6).
  - `kiwami/tests/sync-times.sh` (changed): the phase lists of the rework's RomM runs.
    - Games only: `Starting`, `Listing games`, `Checking the card`, `Games`, `Gamelists`.
    - A full run against `ui/tests/fake-romm.py` with a small library: adds `Reading saves` and the slots' own
      statuses.

    Plus a one-game run that writes no `sync.times`.
  - `kiwami/tests/sync-times-ui.sh` (changed): the fixture holds a paired `romsync.json` (RomM, then Romhost), since
    the rework shows Sync now only when paired. The row is checked after Sync now in both.
  - `docs/FORK.md` (changed):
    - *Taking in upstream*: `kiwami` carries the rework at `cab127ba` ahead of `main`, and what to do when upstream
      merges it unchanged, with more commits, squashed or rewritten, or drops it;
    - 0022's and 0024's hooks at their new places.
  - `kiwami/DEPENDENCIES.md` (changed): in *Upstream's, that the fork depends on*, the Home server rework, pending
    upstream, commit `cab127ba`, taken in by this merge (what the fork's `banana-sync` hooks rely on).
- **Change**: the merge as above. Every conflict of the investigation's *Oversights*, 4 is resolved with both sides
  kept; nothing of the rework's behaviour changes. The re-ported hooks do what they did on `main`'s `banana-sync`, at
  the rework's places.
- **Tests**: `make test` (the rework's `ui/tests/run.sh`, which also runs its `sync.sh`, `sync-slots.sh`,
  `romm-interop.sh` and `paths-test.sh`), `system/tests/launchers-test.sh`, `sudo
  system/bananad/tests/sleep-test.sh`, `kiwami/tests/ci.sh .buildroot` (romm-list, sync-times, sync-times-ui and the
  rest), `make -C ui`, `make -C system/bananad CFLAGS="-O2 -Werror"`. Local: `romm-list.sh --compare` with the rework's
  own `banana-sync` (A2), built from a worktree at `cab127ba` (`git worktree add`, the user's to run).
- **Commit**: `Merge upstream's Home server rework (cab127ba) into k/0039-romm-home-server` (the body: the rework
  pinned; 0022 and 0024 re-ported onto it).

### 2. BIOS downloads in banana-sync

- **Files**:
  - `ui/src/kiwami/romm/romm.h` (new): the fork's part of `banana-sync`, header-only, in `banana::kiwami::romm`.
    - `SCOPES`, the extra scopes: `, \"firmware.read\", \"collections.read\"`.
    - `stateFile()`: `ROOT + "/banana/kiwami-romm.json"`, the fork's state: recorded ids, the hash cache, pending
      installs. Plus `dropState()`.
    - `command(args)`, the `kiwami` verb's dispatch: `bios list`, `bios get <id>...`.
    - `platformsMapped(Backend&)`: `GET /api/platforms`, each named with `bsync::kiwamiFolder()`.
  - `ui/src/kiwami/romm/bios.h` (new): `list()` and `get()` as in A5-A7.
    - `GET /api/firmware?platform_id=<p>` for each mapped platform.
    - States from MD5s.
    - Downloads with `http()` into `.part`, checked, then renamed.
    - DraStic's names: `bios7.bin` to `nds_bios_arm7.bin`, `bios9.bin` to `nds_bios_arm9.bin`, `firmware.bin` to
      `nds_firmware.bin` (any case).
    - A 403 gives `error scope`.
    - Its lines go to stdout for the page, the last one the result. It doesn't open `sync.log`, so the last sync's
      report stays.
  - `ui/src/sync.cpp` (hook):
    - the pairing's scopes line (`cab127ba:ui/src/sync.cpp:1040`) ends `\"devices.write\"" +
      banana::kiwami::romm::SCOPES + "]}"`;
    - `if (cmd == "kiwami") return banana::kiwami::romm::command(args);	// kiwami:` after `cu.load()`
      (`:1608`);
    - `banana::kiwami::romm::dropState();	// kiwami:` in `dropServerLists()` (`:398-402`, Forget and a pairing with
      another server);
    - `#include "kiwami/romm/romm.h"`.
  - `ui/src/sync_romm.cpp` (hook): one line after the anonymous namespace,
    `std::string kiwamiFolder(const JVal& p) { ... }	// kiwami:`. It is the rework's own mapping (`ROMM_SLUGS`, then
    `BANANA_FOLDERS` by `fs_slug`) exported to the fork's headers, declared in `romm.h`.
  - `kiwami/tests/romm-kiwami.py` (new): the fork's stand-in, in front of upstream's `ui/tests/fake-romm.py` (started
    by the test on another port). It forwards every request it doesn't answer, and notes each token's scopes from the
    pairing answers it forwards and from `/test/token`. It answers:
    - `GET /api/firmware[?platform_id]` and `/api/firmware/<id>/content/<name>`, from `<library>/firmware/<platform
      fs_slug>/`, with `firmware.read` required (403);
    - `POST /test/fault`-style hooks of its own under `/kiwami/`.

    Its header cites RomM 5.4.1's `backend/endpoints/firmware.py` for the fields.
  - `kiwami/tests/romm-bios.sh` (new): A4-A7 against the two stand-ins.
    - Pairs by the device flow (`/test/approve`) and checks the scopes in `/test/log`'s init body.
    - Firmware for nds (`bios7.bin`, `BIOS9.BIN`, `firmware.bin`), gba (`gba_bios.bin`), psx (`scph5501.bin`) and 3ds
      (unmapped).
    - `bios list` before and after.
    - `bios get`: files and hashes in `/data/bios`; DraStic's three; the same file skipped; a different
      `gba_bios.bin` kept and reported; an existing `nds_bios_arm7.bin` left alone; a truncated download (fault)
      leaves no file.
    - A token minted without `firmware.read` gives `error scope`, with nothing written.
  - `kiwami/tests/ci.sh` (changed): runs `romm-bios.sh`.
  - `docs/FORK.md` (changed): the four hooks.
- **Tests**: `kiwami/tests/romm-bios.sh` (it builds `banana-sync` from `ui/Makefile`'s rule with `-Wall -Werror`
  added), `romm-list.sh`.
- **Commit**: `RomM home server: BIOS downloads from RomM into /data/bios and DraStic's folder`

### 3. The BIOS page in Settings

- **Files**:
  - `ui/src/kiwami/kiwami.h` (changed): `Page::ROMM_BIOS` and its functions.
  - `ui/src/kiwami/kiwami.cpp` (changed): in `rows()` for `"Home server"`, after Show sync times, a **BIOS** row
    (`kiwami.romm.bios`, a page) when `st.hsRowsFor == "romm"`. The help: "BIOS files from RomM for the DS, the Game
    Boy Advance and the PlayStation, into the card's bios folder."
  - `ui/src/kiwami/romm-bios.cpp` (new): the page.
    - Opening runs `<syncTool()> kiwami bios list` as a `wifi::Job` (`gamesync::syncTool()`, so `BANANA_SYNC`
      names a stand-in).
    - The bottom panel lists the files under their system, ticked when missing. A toggles, Up/Down and L/R move,
      Start downloads the ticked files (`kiwami bios get <ids>`), B goes back.
    - The top panel shows the file's system, size, state (Missing / On the card / A different file is there, kept)
      and the last result ("3 downloaded, 1 already there").
    - `error scope`: "This pairing can't read BIOS files. Pair again (Server, A) to allow them."
    - No Wi-Fi: the rework's message.
    - Touch: a row toggles, the Download button at the bottom.
    - The log line `kiwami: romm bios: <n> listed` / `... got: <last line>`.
  - `kiwami/tests/fake-romm-sync.sh` (new): a stand-in `banana-sync` for the fork's pages. `kiwami bios list` prints
    `$FAKE_BIOS` lines, `bios get` prints `ok`, and each call goes to `$FAKE_SYNC_LOG`. Everything else goes to
    upstream's `ui/tests/fake-sync.sh`.
  - `kiwami/tests/romm-bios-ui.sh` (new): headless at both sizes.
    - Paired RomM: the row after Show sync times.
    - Paired Romhost and unpaired: no row.
    - The page: the list, the ticks, A untick, Start calls `kiwami bios get` with the ticked ids, the result.
    - `error scope`'s message; touch on a row and on the button.
    - Shots in `$OUT`.
  - `kiwami/tests/ci.sh` (changed): runs it when `ui/banana-ui` is built.
- **Tests**: `romm-bios-ui.sh`, `sync-times-ui.sh` (the row order), `make test`, `kiwami/tests/ui-objects.sh`.
- **Commit**: `RomM home server: a BIOS page on Home server`

### 4. A game's sync before it starts, without listing its system

- **Files**:
  - `ui/src/kiwami/romm/lookup.h` (new).
    - `Pages`: the source of `Romm::list()`'s pages for one platform. For a run of one game (`onlyRom` set, slots in
      use) whose folder is that platform's, it first tries:
      - the recorded id: `GET /api/roms/<id>`, taken when its `platform_id` and `fs_name` still match `onlyRom`'s;
      - else `GET /api/roms?platform_ids=<p>&search_term=<stem>&limit=50&with_files=true&...`.

      Either answer is a one-page `{"items": [...], "total": n}`. With no exact match it goes on as 0022's paging
      does, so the rework's matching runs on whatever it gets.
    - For the paging, 0022's query. `KIWAMI_ROMM_ROM_ID_INDEX=1` in the environment leaves `with_rom_id_index=false`
      out, for M7's measurement.
    - `record(Romm-like&)`: after a full listing, the `folder/fs_name` to `id` of every game found on either card
      (`onCard()`), kept in `kiwami-romm.json`'s `ids`. After a one-game run, that game's.
  - `ui/src/sync_romm.cpp` (hook): in `list()`, the 0022 page loop takes its pages from
    `banana::kiwami::romm::Pages pages(*this, onlyRom, sendOk, pf.first, pf.second);	// kiwami:`.
    - `Resp rr = pages.fetch(off);` replaces the `call("GET", q)` line (the query moves into `Pages`, 0022's
      parameters with it).
    - `banana::kiwami::romm::record(*this);	// kiwami:` before `listed = true;`. It is a template over the rework's
      `Romm`, reading `roms` and calling `onCard()`.
  - `ui/src/kiwami/romm/romm.h` (changed): includes `lookup.h`, and `ids` in the state file.
  - `kiwami/tests/romm-kiwami.py` (changed):
    - `GET /api/roms/<id>`, built from the stand-in's listing of that game's platform;
    - `search_term` on `/api/roms`, matched as RomM 5.4.1 does (`roms/__init__.py:660`: a case-insensitive substring
      of the name or file name), the rest forwarded.
  - `kiwami/tests/romm-lookup.sh` (new): A9-A11 with the stand-ins, 50 GBA games, `homesync_launch = on`, a save on the
    card and one in the slot.
    - The first launch: one search and no listing.
    - The second launch: one `GET /api/roms/<id>`.
    - A game renamed on the server: the listing, and the outcome checked against the rework's own build in the same
      case (its `launch.status` and the card's save the same).
    - A search with two near names: the exact one kept.
  - `kiwami/tests/romm-list.sh`, `kiwami/tests/romm-big.py` (changed, *Deviations*): the one-game run by its
    remembered id; `GET /api/roms/<id>` and `search_term` in the big stand-in.
  - `kiwami/tests/ci.sh` (changed).
  - `docs/FORK.md` (changed).
- **Tests**: `romm-lookup.sh`, `romm-list.sh` (paging unchanged for full runs, A2), `ui/tests/sync-slots.sh` (the
  rework's own slot cases, still passing with the hook).
- **Commit**: `RomM home server: a game's save synced before it without listing its system`

### 5. ROMs found by their hash in Sync now

- **Files** (*Deviations*, 2026-11-17):
  - `ui/src/kiwami/romm/hashes.h` (new). `byHash(Romm-like&)`, after a full listing in a sync (its "running" status):
    - the card's files under each mapped system's folder (both cards, skipping archives: `zip`, `7z` and the rework's
      `packedName()` list, and `.m3u`) that no listed game matches by name, and of the size of a listed game of one
      file (not an archive) on that system (`romJson`'s `files[]` `file_size_bytes`);
    - their MD5s from `kiwami-romm.json`'s `hashes` (keyed by path, size and mtime), else hashed (`  by hash: reading
      <path>`), with a `synctimes::tick` "Matching by hash" status;
    - compared with those games' MD5s from the listing, no request;
    - a game on the same platform with no card file of its own name, the only server game with that MD5, and only one
      card file with it, gets an alias, kept in `aliases` (server id to card path).

    Each decision is written to `sync.log`: `  by hash: <path> is <server name>`, or `... not matched: <why>`.
  - `ui/src/kiwami/romm/romm.h` (changed): `State`'s `hashes` and `aliases`; `aliases()`, read once a run, and
    `aliasOf()`; includes `hashes.h`.
  - `ui/src/kiwami/romm/lookup.h` (changed): `Pages`' `exact()` also takes a game whose alias is the card file.
  - `ui/src/sync_romm.cpp` (hook):
    - first line of `onCard()`:
      `{ std::string a = banana::kiwami::romm::aliasOf(r.id); if (!a.empty()) return a; }	// kiwami: a ROM found by its contents, under another name on the card`.
      `aliasOf` checks the file still exists.
    - `banana::kiwami::romm::byHash(*this);	// kiwami:` before the `record` hook, so the alias's id is remembered.
  - `kiwami/tests/romm-hash.sh` (new): A12.
    - A renamed card copy of a server game, with a save, found by hash.
    - Its save uploaded into that game's `autosave` slot (`/test/state`).
    - The next Sync now hashes nothing (the log's lines counted).
    - Touching the file's mtime hashes it again.
    - Two card copies with one hash: no alias.
    - A server game with its own-name card file: no alias for the renamed copy.
    - Two server games with one hash, and a card file of a server game's size with other bytes: no alias.
    - A `.zip`, and a file of no server game's size: not hashed.
    - The sync before the renamed game: one `GET /api/roms/<id>`, no listing, the slot's newer save brought.
    - The renamed file removed: its alias and MD5 dropped.
  - `kiwami/tests/ci.sh`, `docs/FORK.md` (changed).
- **Tests**: `romm-hash.sh`, `romm-lookup.sh`, `ui/tests/sync-slots.sh` (plus `romm-list.sh`, `sync-times.sh` and
  `romm-bios.sh`, whose runs go through `list()`).
- **Commit**: `RomM home server: ROMs under another name found by their hash`

### 6. The game list kept on the card and brought up to date

- **Files** (*Deviations*, 2026-11-17):
  - `ui/src/kiwami/romm/listcache.h` (new): `banana/kiwami-romm-list.jsonl`, one slimmed game a line, after a header
    line `{"server": <url>, "version": 1, "listed_at": <the server's time the listing began>}`.
    - `Pages`, for a full run (no `onlyRom`), when the cache is this server's (`Cached`): the cached games of that
      platform, read one line at a time.
    - Merged with `updated_after=<listed_at less 60 s>` pages (the platform's newest updated_at less 60 s when the
      server gave no `Date`), in RomM's name order (`name_sort_key`, then id); a cached game replaced by its newer
      copy (by id). They go to list()'s loop as pages of `syncmem::PAGE` with the platform's `total`.
    - Before the platforms, one `GET /api/roms/identifiers`. A cached id missing from it is dropped; a failure means a
      full listing.
    - A platform with no cached game is listed in full.
    - At the end of a complete listing (`keepList()`, from `record()`), the file is written again (to `.new.<pid>`,
      then renamed), unless nothing changed.
    - A listing broken off keeps the old file.
    - `banana-sync games` (Get games' X) sets `full`.
    - Its log lines go to `sync.log` in a sync, to stderr otherwise (`games` prints its games on stdout).
  - `ui/src/kiwami/romm/romm.h` (changed): `listFile()`; `dropState()` removes the file too; `romsQuery()` moved here
    from `lookup.h` (both use it); includes `listcache.h`.
  - `ui/src/kiwami/romm/lookup.h` (changed): `Pages` takes the cached path for full runs, and notes the listing's
    time from a full listing's first answer; `record()` calls `keepList()`.
  - `ui/src/kiwami/syncmem.h` (changed): `slim()` keeps `name_sort_key`.
  - `ui/src/sync.cpp` (hook): `banana::kiwami::romm::fullListing();	// kiwami: Get games' X lists everything` in the
    `games` command, before `b->manifest(...)` (`cab127ba:ui/src/sync.cpp:1693`).
  - `kiwami/tests/romm-kiwami.py` (changed):
    - every platform listing on `/api/roms` with `updated_at` (each game's file's mtime, or the time a test touched
      it) and `name_sort_key` (RomM's `compute_name_sort_key()`), in RomM's name order, and `updated_after`;
    - `GET /api/roms/identifiers`, and a `500` fault for it;
    - a `/kiwami/touch` hook to change a game; a game is deleted with upstream's `/test/delete-rom`.
  - `kiwami/tests/romm-big.py` (changed): `updated_after` and `identifiers`, for the memory run, and each answer's
    size in `<log>.bytes`.
  - `kiwami/tests/romm-incremental.sh` (new): A13-A14.
    - The first Sync now lists in full and writes the cache.
    - The second asks only `updated_after` pages (since the header's `listed_at` less 60 s) and `identifiers`, and
      doesn't write the file again.
    - A game touched on the server: its new name in `homesync-server-games.tsv`, where RomM's name order puts it.
    - A deleted game: gone.
    - A game added: after the game RomM's name order puts it after.
    - A new platform: listed in full.
    - The identifiers request failing (fault): a full listing.
    - `games`: a full listing, in the order the incremental list had.
    - Another server paired: no cache used.
    - Forget: the file gone.
  - `kiwami/tests/romm-list.sh` (changed): the games-only run without a cache (still a full listing); then a Sync now
    on the big library through the cache. Its peak is under 100 MB, its answers' bytes a tenth of the full listing's
    or fewer (its requests: 6 `updated_after` and `identifiers`), Get games' list the same, and the file not written
    again.
  - `kiwami/tests/ci.sh`, `docs/FORK.md` (changed).
- **Tests**: `romm-incremental.sh`, `romm-list.sh`, `romm-lookup.sh`, `romm-hash.sh` (plus `romm-bios.sh`,
  `sync-times.sh`, and upstream's `ui/tests/sync.sh`, `sync-slots.sh` and `romm-interop.sh`, whose syncs now try the
  cache: their stand-in answers no `identifiers`, so they list in full).
- **Commit**: `RomM home server: the game list kept on the card, brought up to date with what changed`

### 7. Installs from RomM's web page, in banana-sync

- **Files**:
  - `ui/src/kiwami/romm/installs.h` (new).
    - `capability(bool on)`: `PUT /api/devices/<c.device>` with `{"capabilities": {"remote_install": on}}`. The verb
      `kiwami installs on|off`, last line `ok` / `error <why>`.
    - `claim(Backend&, Conf&, settings)`, with `kiwami_romm_installs = on`, RomM 5.4 or newer and a device id:
      - sends the capability once per device id (kept in `kiwami-romm.json`), so a new pairing gets it;
      - `POST /api/devices/<id>/installs/claim`;
      - for each request, its game found in `b.manifest()` by its `file_ids` and `rom_id` against each file's URL
        (`/api/roms/<rom_id>/content/`, `/api/roms/<file id>/files/content/`);
      - a game on either card: `already_installed` reported at once;
      - a game not in the manifest (a system Banana doesn't have, a game of several files the rework skips): `failed`
        with "Banana can't run this game", or the rework's reason;
      - else the key added with `editGetList()`, and `request id -> key` kept as pending.
    - `wanted(Conf&, settings)`: the setting on, and `GET /api/devices/<id>/installs` listing a pending request.
    - `report(Conf&)`, at the end of a run, for each pending request:
      - its key off `homesync-get.txt` and its files on a card: `done`;
      - its key in `homesync-get-failed.txt`: `failed` with that reason (cut to 500 characters), and the key taken off
        the list (`editGetList` drop);
      - else left `taken`.

      A 404 or 409 from RomM (expired, cancelled): the pending entry dropped and logged.
  - `ui/src/sync.cpp` (hook), in `doSync`:
    - first in the `if (!autoWhy.empty())` block:
      `if (autoWhy == "wifi" && get("homesync_auto", "off") != "both" && c.backend == "romm" && banana::kiwami::romm::wanted(c, set)) { gamesOnly = true; doSync(false, "", ""); gamesOnly = false; c = loadConf(); }	// kiwami: games sent from RomM's page, on Wi-Fi with Auto sync not syncing then`;
    - after `b->onlyRom = onlyRom;`:
      `if (romm && onlyRom.empty() && !savesOnly && !dryRun) banana::kiwami::romm::claim(*b, c, set);	// kiwami: onto Get games' list, brought by this run (more())`;
    - before the `status("done ...")` line:
      `if (romm && onlyRom.empty() && !dryRun) banana::kiwami::romm::report(c);	// kiwami:`.
  - `kiwami/tests/romm-kiwami.py` (changed): RomM 5.4.1's installs.
    - `/api/devices/<id>/installs` (GET), `/installs/claim` (POST: pending to taken, and the taken ones again), and
      `/installs/<rid>` (PUT: `done`, `already_installed`, `failed` with `reason` ≤ 500, else 422; 409 from a wrong
      status).
    - A request accepted only for a device whose capabilities hold `remote_install: true` (read from upstream's
      stand-in, `GET /api/devices/<id>`).
    - `file_ids` chosen as `policy.py` does.
    - `/kiwami/install {rom_id}` (the web UI's queue).
    - An expiry hook.
  - `kiwami/tests/romm-installs.sh` (new): A15-A18.
    - Off: no request to installs or devices `PUT`.
    - `kiwami installs on`: the capability on the device. A request before it: refused (400).
    - Sync now with three requests (a GBA game, a PS1 disc set, a game already on the card): the first two downloaded
      and `done`, the third `already_installed`.
    - A multi-file GBA game: `failed`.
    - "Games go to" the second card while it's out (`--run` without `sd2/roms`): `taken`, then `done` with the card
      in.
    - An expired request: dropped.
    - `--auto wifi` with Auto sync off: games-only, no `/api/saves` request, then the rework's return.
    - With `both`: one run downloads them.
    - Installs off: the run as the rework's (its requests compared).
  - `kiwami/tests/ci.sh`, `docs/FORK.md` (changed).
- **Tests**: `romm-installs.sh`, `romm-incremental.sh`, `ui/tests/sync.sh` (the rework's Get games and Wi-Fi cases).
- **Commit**: `RomM home server: games sent from RomM's web page downloaded at the next sync`

### 8. The installs row in Settings

- **Files**:
  - `ui/src/kiwami/kiwami.cpp` (changed): in `rows()` for `"Home server"`, when RomM is 5.4 or newer
    (`st.hsSlotsKept()`, the rework's test for slots, the same version), an **Installs from RomM** row before BIOS.
    - Key `kiwami_romm_installs`, values Off/On (`off`/`on`), default Off.
    - The help: "Games you send to this handheld from RomM's web page come at the next Sync now, or when Wi-Fi
      connects. RomM keeps a request two days."
    - Changing it writes `settings.conf` and runs `<syncTool()> kiwami installs on|off` detached. A failure is shown
      ("RomM didn't take it: <why>"), and the row is put back.
  - `kiwami/tests/fake-romm-sync.sh` (changed): `kiwami installs on|off` prints `$FAKE_INSTALLS` or `ok`.
  - `kiwami/tests/romm-installs-ui.sh` (new): headless at both sizes.
    - Paired RomM 5.4.1: the row before BIOS, Off; turning it On writes the setting and calls `kiwami installs on`;
      the error case puts it back.
    - RomM 5.3.1 (the rework's old-server `romsync.json`) and Romhost: no row.
  - `kiwami/tests/ci.sh`, `docs/FORK.md` (changed).
- **Tests**: `romm-installs-ui.sh`, `romm-bios-ui.sh`, `sync-times-ui.sh`, `make test`, `ui-objects.sh`.
- **Commit**: `RomM home server: Installs from RomM, a row on Home server`

### 9. Final check

- `make test`, `make -C ui`, `make -C system/bananad CFLAGS="-O2 -Werror"`, `system/tests/launchers-test.sh`, `sudo
  system/bananad/tests/sleep-test.sh`, `kiwami/tests/ci.sh .buildroot`.
- The fork's tests in `kiwami/tests/`, and locally `romm-list.sh --compare` against the rework's own build at
  `cab127ba` (A2).
- The hooks: `git diff --stat cab127ba...HEAD` and `git grep -n 'kiwami:'` agree with `docs/FORK.md`, and list only
  the hooks this feature meant to add (the comparison is against the rework, since `main` doesn't have it).
- With the fork's settings at their defaults, `make test`'s renders in `ui/tests/out/` match the rework's own build's
  (A19).
- `kiwami/DEPENDENCIES.md` lists the rework at `cab127ba`.
- `docs/FORK.md` updated; `docs/ARCHITECTURE.md` and `CLAUDE.md` checked (`CLAUDE.md`'s banana-sync description and
  `sync.cpp` paths).
- `with_rom_id_index` (M7): if the measurement shows `with_rom_id_index=false` slower on the user's server, `Pages`
  drops it in this step, with `romm-list.sh`'s request assertion updated; the outcome goes in the manual test table.
- The manual tests below all passed.
- Every acceptance criterion is met: each is listed by at least one automated or manual test, and those passed.

## Automated tests

| Test | Where | What it checks | Criteria |
|---|---|---|---|
| The rework's own suites | `ui/tests/run.sh` (`make test`), `ui/tests/sync.sh`, `sync-slots.sh`, `romm-interop.sh`, `system/tests/launchers-test.sh`, `sleep-test.sh` | The rework works as merged, with the fork's hooks in it | A1 |
| romm-list | `kiwami/tests/romm-list.sh`, `syncmem-units.cpp`, `romm-big.py` | Peak memory of `games`, games-only Sync now and a one-game sync on a 2400-game library; the slim guard; `--compare` with the rework's build; the cached Sync now (bytes, peak, same list) | A2, A13, A14 |
| sync-times | `kiwami/tests/sync-times.sh`, `synctimes-units.cpp` | The phases of the rework's runs, `sync.times`, the report; nothing off; nothing for one game | A3 |
| sync-times-ui | `kiwami/tests/sync-times-ui.sh` | The line and the row after Sync now, RomM and Romhost | A3, A19 |
| romm-bios | `kiwami/tests/romm-bios.sh`, `romm-kiwami.py` | Scopes; list; get; DraStic's copies; kept and skipped files; faults; no scope | A4-A7 |
| romm-bios-ui | `kiwami/tests/romm-bios-ui.sh`, `fake-romm-sync.sh` | The BIOS row and page at both sizes, keys and touch | A7, A8 |
| romm-lookup | `kiwami/tests/romm-lookup.sh` | Recorded id, search, fallback, outcome same as the rework's | A9-A11 |
| romm-hash | `kiwami/tests/romm-hash.sh` | By hash, alias, cache, ambiguous cases, archives, the sync before a renamed game | A12 |
| romm-incremental | `kiwami/tests/romm-incremental.sh` | `updated_after` pages, identifiers, changes, deletes, additions, RomM's name order, full listings | A13, A14 |
| romm-installs | `kiwami/tests/romm-installs.sh` | Capability, claim, report, already there, failed, second card, expiry, Wi-Fi runs, off | A15-A18 |
| romm-installs-ui | `kiwami/tests/romm-installs-ui.sh` | The row, its setting and call, hidden for old RomM and Romhost | A15 |
| Renders | the final check | Defaults render as the rework's build | A19 |

All are run by `kiwami/tests/ci.sh` (the UI ones when `ui/banana-ui` is built), except `--compare` and the renders'
comparison, which are local.

## Manual tests

On the RG DS Plus (the only device at hand), against the user's RomM 5.4.1, on the image built from the final step.
Logs are fetched over SSH/SFTP (Settings > Network > SSH) or from the card on a computer (`banana/logs/` on the card
is `/data/banana/logs/` on the device):

- `sync.log` and `sync.log.1`: each sync's report, with the fork's `by hash:`, `memory: peak` and `times:` lines.
- `banana-ui.log`: the last run's; copy it before restarting the menu.
- `/run/banana/launch.status`: the last sync before a game.

| # | Board | Steps | Expected | Logs to collect | Criteria | Outcome |
|---|---|---|---|---|---|---|
| M1 | rgds-plus | Update to the build. Home server: Forget, then Server > Pair by QR, approved in RomM. Open Home server. | Paired. The rows are the rework's plus Show sync times (last), Installs from RomM (Off) and BIOS. RomM's token page lists `firmware.read`. | `sync.log`, `banana-ui.log`, a photo of the page | A4, A8, A15 | not run |
| M2 | rgds-plus | BIOS: open the page. Start with the missing files ticked. Then play a DS game under DraStic and under BananaDS, and a PS1 game. | The DS, GBA and PS1 files in `/data/bios`, and DraStic's three in `/data/drastic/system/`. Reopened, all "On the card". The games start. | `banana-ui.log`, `ls -l /data/bios /data/drastic/system` | A5, A6, A8 | not run |
| M3 | rgds-plus | Installs from RomM On. On a phone, in RomM's web UI, install a GBA game and a PS1 game on this device. Switch Wi-Fi off and on, wait a minute. | Both games on the card (in their shelves). RomM shows them done. | `sync.log`, a photo of RomM's install status | A15, A16, A18 | not run |
| M4 | rgds-plus | As M3, with Games go to: Second SD card and the card out. Sync now. Then put the card in and Sync now again. | The first time: waiting (RomM: taken). The second: on the second card, done. | `sync.log` both times | A17 | not run |
| M5 | rgds-plus | Sync before playing On. Start a game of a system with 1000+ games, twice. Then a game renamed on the card, after a Sync now. | The first start does a search, the second a single lookup, each with the wait under 5 s. The renamed game's save syncs (`by hash:` line). | `sync.log`, `/run/banana/launch.status`, `banana-ui.log` (the wait's times) | A9, A10, A12, A20 | not run |
| M6 | rgds-plus | Sync now twice, with Show sync times On. Then rename a game in RomM and Sync now. | The second sync's Listing games is much shorter, and its peak under 100 MB. The rename shows in Get games. | `sync.log` and `sync.log.1` (`memory:`, `times:`) | A13, A14, A20 | not run |
| M7 | rgds-plus | Over SSH: `time banana-sync games > /dev/null`, then the same with `KIWAMI_ROMM_ROM_ID_INDEX=1` (the fork's switch that leaves `with_rom_id_index=false` out), three times each. | The two medians recorded. The final check keeps or drops the parameter by them. | the shell output | (final check) | not run |
| M8 | rgds-plus | A save made in Grout or Argosy (or RomM's player) for a GBA game on the card, with Quick resume On. Start the game on Banana. | The newer save is loaded, not the old resume state. | `sync.log`, `/run/banana/launch.status` | A20 | not run |
| M9 | rgds-plus | Close the lid during M3's download, then open it after a minute. | The download goes on, or the next sync finishes it. Nothing half-written in `roms/`. | `sync.log`, `ls` of the folder | A16 | not run |

Notes from the runs (what the logs showed, what was fixed):

## Deviations

Where the implementation turned out different from this plan, why, and what changed (dated).

- **2026-11-16, step 1: the QR library.** banana-ui didn't link after the merge: the fork's `ui/src/kiwami/qrcodegen.cpp`
  (Tailscale's login QR) and the rework's `ui/src/qr/qrcodegen.cpp` (RomM's pairing QR) both define `qrcodegen::*`.
  The investigation had called the two copies harmless. They are byte-identical (1.8.0, `720f62bd`), so the fork's
  copy is deleted and `tsstatus.cpp` includes `qr/qrcodegen.hpp` (as upstream's `settings_homesrv.cpp` does: `ui-objects.sh` finds its includers by that path in the `.d` files). `kiwami/tests/ci.sh`'s units build,
  `units.cpp`'s comment and `ui-objects.sh`'s watched header move to `ui/src/qr/`. `kiwami/DEPENDENCIES.md`'s row moves
  to *Upstream's, that the fork depends on*. Agreed with the user.
- **2026-11-16, step 1: Show sync times' download phase is "Downloading".** The rework's download status is "Downloading
  game <i> of <N>", or "Downloading" for one game, where `main`'s was "Games i/N". `synctimes::phaseOf()` now strips a
  trailing " <i> of <N>" too, and takes both forms as one phase, Downloading. Its report line is "times: Downloading
  <s> s, <a> of <b> MB, <i> of <n> games", and its time left is the larger of the MB's and the games'. The plan's
  "Games" in the phase lists is Downloading. The rework also brings the games before the saves, so a full Sync now's
  phases are Starting, Listing games, Checking the card, Downloading, Gamelists, Saves, Reading states. Reading saves
  comes once a slot has a head to read (the next sync). Agreed with the user.
- **2026-11-16, step 1: the listing's progress reads `synctimes::lastStatus()`.** `statusLine` is in `sync_core.cpp`'s
  anonymous namespace in the rework, out of `sync_romm.cpp`'s reach. `synctimes::seen()`, already hooked in `status()`,
  keeps the last line whatever the setting, and the progress hook writes only over a "running" one. No extra hook
  line.
- **2026-11-16, step 1: Show sync times is the Home server page's last row**, not right after Sync now (A3 changed). The
  rework's `ui/tests/sync.sh` reaches Get games, Manage saves, Save slot and Device name by counting Downs from Server,
  and the fork's row after Sync now shifted them all: about 25 of its cases failed in `make test`. No upstream test
  reaches the page's end, so the hook (`Kiwami::rows(*this, HS)`) is one line after Forget this server's `add`,
  outside the RomM and Romhost branches, where the plan had one in each. `sync-times-ui.sh` reaches it Up from Server.
  Steps 3 and 8 add their rows through the same hook, so Installs from RomM and BIOS follow it at the page's end (their
  order among the fork's rows as planned). Agreed with the user.
- **2026-11-16, step 1: the tests, beyond the plan's list** (agreed with the user).
  - `romm-big.py` also answers the rework's RomM 5.4 save path: `POST /api/sync/negotiate` and the session's
    `/complete`, and `/api/saves` filtered by `rom_ids` and `slot`. Its two saves are in the `autosave` slot at real
    sizes (GBA 128 KiB, DS 512 KiB): the rework refuses others. The test cards carry a device id.
  - `romm-list.sh` also runs one game's sync before it (A2 names it): the peak, the save in place, only GBA listed,
    no `sync.times`. `--compare` takes a commit as well as a binary and builds that commit's `banana-sync` from
    `git archive` (no worktree): `--compare cab127ba`.
  - `sync-times.sh` runs against `ui/tests/fake-romm.py` alone: a full Sync now with two games on Get games' list
    (the downloads answered 3 s late, for the Downloading phase), the next Sync now, a games-only one, and one game's
    sync before it and after it (never timed). `--compare` leaves out the Listing games statuses and the `memory:`
    line, which are 0022's and which the rework's own build doesn't have.
  - `synctimes`' `tick()` lines also go back at the rework's places in `sync.cpp` (`want`, `gone`, the gamelists,
    Romhost's saves), as on `main`.
- **2026-11-16, step 2: the pairing's scopes are read from the fork's stand-in.** Upstream's `fake-romm.py` logs no
  request bodies (`/test/log` holds the method and path), so `romm-kiwami.py` writes the device pairing's
  `requested_scopes` to its own log ("init scopes: ..."), which `romm-bios.sh` checks for A4.
- **2026-11-16, step 3: `sync-times-ui.sh` and `docs/FORK.md` change too.** BIOS is the page's last row for a paired
  RomM, after Show sync times, so `sync-times-ui.sh` reaches Show sync times with two Ups from Server for RomM (one
  for Romhost), and `docs/FORK.md`'s note on the row's place names the fork's rows after Forget and the BIOS row's
  test. Step 8's Installs from RomM row (between them) adds one more Up for RomM. Agreed with the user.
- **2026-11-16, step 4: the ids are keyed by the card file's path, and `romm-list.sh` and `romm-big.py` change.**
  - `kiwami-romm.json`'s `ids` map the card file's roms-relative path (what `--rom` names and `onCard()` returns,
    `gba/Foo (USA).gba`, `psx/Game.m3u`) to the RomM id, not `folder/fs_name`, so the lookup is one map read and needs
    no name rebuilt for a disc set or a subfolder. Agreed with the user.
  - `romm-list.sh`'s listings before its one-game run now remember game 10's id, so that run asks
    `GET /api/roms/200010` and lists nothing. Its check expects that (A9 on the big library), and `romm-big.py`
    answers `GET /api/roms/<id>` and `search_term`. Agreed with the user.
  - RomM 5.4.1's `search_term` (`backend/handler/database/roms_handler.py`, `_filter_by_search_term()`) isn't a
    substring of the whole term. It splits the term at `|` into alternatives, and each blank-separated word has to be
    in the game's name, file name or other titles, case aside (on MariaDB, a word of 3+ letters through FULLTEXT as the
    start of one of theirs). A card file's stem always matches its own `fs_name` this way. The two stand-ins copy that.
    The search's answer goes to the loop as it came, near names and all: the rework's own matching (`onCard() ==
    onlyRom`) keeps the exact game, and `record()` records only that one.
  - `record()` clears `Romm`'s `cardIndex` after a full listing. `onCard()` memoizes the card's ROMs for the whole run,
    so without it a game Get games downloads later in the same Sync now would be missing when the saves look.
  - `romm-lookup.sh` builds the rework's own `banana-sync` from `git archive cab127ba` at `-O0` (5 s), when the
    repository has that commit (CI's checkout has the whole history), and says it skipped the comparison otherwise.
- **2026-11-17, step 5: matched from the listing, not by `GET /api/roms/by-hash`; a renamed game's id remembered.**
  - Sync now already holds every mapped system's games with each file's size and MD5 (`with_files=true`, kept by
    `slim()`), so `byHash()` asks RomM nothing. It reads a card file only when a server game of one file on its system
    (no archive) has its very size, and compares the MD5 with the listing's. A card full of the user's own games, not on
    the server, is never read, where hashing every unmatched file for `by-hash` would read them all once (gigabytes of
    DS ROMs on the first Sync now). The MD5s are still kept in `kiwami-romm.json`'s `hashes`, for a file of a server
    game's size that isn't its copy. `romm-kiwami.py` doesn't change. Two server games with the same MD5 also get no
    alias (said). Agreed with the user.
  - `byHash()` runs before `record()`, not after it, so the alias's path gets its RomM id, and `Pages`' `exact()` takes a
    game whose alias is the card file: the sync before a renamed game is one `GET /api/roms/<id>`, not a listing of its
    system. `romm-hash.sh` checks it. Agreed with the user.
  - `byHash()` runs only under a sync's "running" status, as the listing's progress does, so Get games' refresh (`banana-sync
    games`) reads no file; its runs use the aliases the last Sync now found.
- **2026-11-17, step 6: changes since the server's own time, RomM's name order, and bytes measured.**
  - `updated_after` is the server's time of the last listing (the `Date` header of its first answer, kept as the
    file's `listed_at`) less 60 s, not each platform's newest `updated_at` less 60 s, which fetched again at every Sync
    now every game updated in the minute before the newest one: after a quick RomM scan, a whole system. The newest
    `updated_at` less 60 s is the fallback when the server sends no `Date`. Agreed with the user.
  - The changed and new games are merged into the kept ones in RomM's name order (`name_sort_key`, which `slim()` now
    keeps, then id; the name lowercased with no key), because Get games shows a system's games in the file's order:
    added at the end, a new game would show last until Get games' X. Agreed with the user.
  - `romm-list.sh`'s cached run can't make a tenth of the first's requests (6 `updated_after` and `identifiers`
    against 12): it checks its answers' bytes under a tenth of the full listing's (A20's measure), from `romm-big.py`'s
    `.bytes` log. Its games-only run removes the cache first, so it is still the full listing A2 measures. Agreed with
    the user.
  - `romsQuery()` moves from `lookup.h` to `romm.h`, as `listcache.h` uses it too; the file is written by
    `keepList()`, called from `record()` (no hook of its own); `updated_after` pages are asked for by name, with
    `romsQuery()`'s `order_by=name`, not by id. A game deleted in the tests goes through upstream's `/test/delete-rom`,
    so the fork's stand-in has no `/kiwami/delete`.
  - The cache's log lines go to `sync.log` only in a sync: `say()` also prints on stdout, where `banana-sync games`
    prints its games for the page.

## Progress

A step is marked done in its own commit, so the commit column holds the subject (a commit can't carry its own hash).

| Step | Status | Commit |
|---|---|---|
| 1. The rework taken in, with 0022 and 0024 re-ported | done | Merge upstream's Home server rework (cab127ba) into k/0039-romm-home-server |
| 2. BIOS downloads in banana-sync | done | RomM home server: BIOS downloads from RomM into /data/bios and DraStic's folder |
| 3. The BIOS page in Settings | done | RomM home server: a BIOS page on Home server |
| 4. A game's sync before it starts, without listing its system | done | RomM home server: a game's save synced before it without listing its system |
| 5. ROMs found by their hash in Sync now | done | RomM home server: ROMs under another name found by their hash |
| 6. The game list kept on the card and brought up to date | done | RomM home server: the game list kept on the card, brought up to date with what changed |
| 7. Installs from RomM's web page, in banana-sync | todo | |
| 8. The installs row in Settings | todo | |
| 9. Final check | todo | |
