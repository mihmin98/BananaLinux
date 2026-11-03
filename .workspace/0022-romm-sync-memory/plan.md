# 0022 RomM sync memory: plan

Issue: #22 · Branch: `k/0022-romm-sync-memory` · Investigation: [investigation.md](investigation.md) · Status: approved

## Summary

With a large RomM library, **Sync now**, **Choose games**, a saves-only sync and the automatic syncs (leaving a game,
after Wi-Fi joins) no longer freeze the handheld. `banana-sync` asks RomM for 50 games a page instead of 500, adds
`with_provider_metadata=false` (ignored by RomM 5.4.1, honoured by later releases), and keeps from each game only the
fields `Romm::manifest()` reads. Each page's text and full tree are freed before the next request. At the end of each
sync, the report (`banana/logs/sync.log`) says the sync's peak memory. The games, covers, `gamelist.xml` and state
written to the card stay as upstream's; only the requests and the memory change. There is no setting (investigation,
question 3).

The approach chosen in the investigation (option 1), with one refinement from planning (question 8):

- **Each game is kept as a pruned copy of its JSON tree, not as a new struct.** `Romm::manifest()` reads each game
  only through `JVal::str/num/flag/get`. `list()` therefore stores `syncmem::slim(*it)`: a new small object holding
  only the members `manifest()` reads, with the same values. `manifest()` itself stays upstream's, so the `Manifest`
  is the same by construction. Scalar nodes are shared, not copied, and none of them holds a reference to its parent,
  so the rest of the page is freed. The pruning lives in a header of the fork's, `ui/src/kiwami/syncmem.h`. It is
  header-only because `banana-sync` is built from `sync.cpp` and `json.cpp` alone (`ui/Makefile:39-40`, and
  `ui/tests/sync.sh:17` compiles it the same way), and a `.cpp` in `ui/src/kiwami/` would go into banana-ui through
  the Makefile's wildcard instead. The cost is about 5 KB a game kept (around 40 MB for 8000 games) instead of the
  1-2 KB a struct would take. Either way, that is far below today's ~160 KB a game.
- **The hooks**: 6 `kiwami:` lines in `ui/src/sync.cpp` (the include, the page size in three places, the pruning, and
  the memory line) and 1 in `ui/Makefile` (the header as a prerequisite of `banana-sync`, question 10).
- **A guard against upstream's drift.** If upstream adds a field read in `Romm::manifest()`, `slim()` would silently
  drop it. The fork's test reads the keys used in `manifest()` out of `ui/src/sync.cpp` and stops when the header
  doesn't keep one.
- **The unit tests get a file of their own**, `kiwami/tests/syncmem-units.cpp` (question 9), run by the feature's
  test script, which keeps clear of `k/0008-romm`'s changes to `units.cpp`.

What stays as upstream has it: the saves and states lists, Romhost, Choose games running without the lock and
listing skipped systems (investigation, *Oversights* 2 and 3), and duplicate ids across pages. A sync without a
network, or over sleep, behaves as before.

## Acceptance criteria

- **A1**: Given a RomM library of 2400 games on six of Banana's systems, each game's entry about 24 KB of JSON
  (RetroAchievements with 40 achievements, IGDB and ScreenScraper blocks, screenshots), when Choose games' list is
  read (`banana-sync games`), then the process peaks under **64 MB** resident. Upstream's build peaks several times
  over that bound (checked in the final check).
- **A2**: Given the same library, when Sync now runs (`banana-sync sync --dry-run`) and when a saves-only sync runs
  (`banana-sync sync --saves-only`), then each peaks under 64 MB.
- **A3**: When the games are listed, then every `/api/roms` request asks for `limit=50` with
  `with_provider_metadata=false`, every page is read (8 requests for a platform of 400 games), and no platform Banana
  has no system for (`3ds`) is asked for.
- **A4**: Given games with the edge cases RomM can send (a game missing from RomM's disk, a path that climbs out of
  the folder, a multi-file game on a cartridge system, a PS1 disc set with a `.txt` beside it, a null summary, no
  `metadatum`, empty genres, publishers that aren't a list, a release date in seconds, a `.jpg` cover, no cover, a
  numeric name, a tab in a name, a null `file_size_bytes`, an entry of another platform in the page), when listed,
  then `games`' output, the dry run's file list, and a real sync's card (ROMs, covers, `.m3u`, `gamelist.xml`,
  `romsync.json`'s `state`) are identical to upstream's build on the same library. For every member `manifest()`
  reads, `slim()`'s copy answers `str/num/flag/get` exactly as the original does.
- **A5**: Given saves on RomM for a GBA and a DS game, when a saves-only sync runs, then they are downloaded to the
  card as `saves/Game 0010 (USA).srm` and `saves/nds/Game 0010 (USA).sav` (the saves are still matched to their
  games through the listing).
- **A6**: When a sync ends (Sync now, saves-only, automatic), then its report ends with `memory: peak <n> kB` (the
  process's `VmHWM`) after the `done:` line: in `banana/logs/sync.log`, and on stdout for a dry run. If `VmHWM`
  can't be read, the line says `memory: peak unknown`.
- **A7**: `make test`, which includes upstream's `ui/tests/sync.sh` (pairing, syncs, gamelists, covers, PS1 discs,
  saves both ways, conflicts, the second card, https), passes unchanged against the fixed build.
- **A8**: Given an upstream change that makes `Romm::manifest()` read a key that `syncmem.h` doesn't keep, when the
  fork's checks run (`kiwami/tests/ci.sh`), then they stop with `romm-list: Romm::manifest() reads "<key>", which
  syncmem.h drops`.
- **A9**: On the RG DS Plus, paired with the user's RomM (1000+ games a system, RetroAchievements on): Sync now,
  Choose games, a saves-only sync on leaving a game and the automatic sync after Wi-Fi joins at boot all finish
  without freezing the handheld, and the menu stays usable while they run.
- **A10**: Nothing outside banana-sync changes: banana-ui's renders (`make test`'s PNGs) match `main`'s, and the
  rootfs gains no file.

## Risks

- **Performance and footprint.** Peak memory goes from up to about 1 GB to tens of MB (A1, A2; on the device, M1 and
  M2). The kept list costs about 5 KB a game; for 8000 games that is about 40 MB, still small beside the RAM, and
  measured by the memory line (M1). The extra requests cost a few seconds a sync: 10 times as many, each tens of ms on
  a LAN. Nothing runs while idle, the rootfs is unchanged (the header compiles into the binary), boot time is
  unchanged, and banana-ui is untouched (A10).
- **Conflicts with upstream.** `ui/src/sync.cpp` changes often upstream. If upstream rewrites `Romm::list()`, the
  merge conflicts on the hooked lines, and `banana-merge-upstream` will see that. The silent case is upstream starting
  to read a new field in `manifest()`: the guard (A8) stops CI on it. If upstream fixes the memory its own way, the
  merge drops the fork's lines and header (the `docs/FORK.md` note, step 3). The two `ui/Makefile` lines sit beside
  its existing `kiwami:` lines. `kiwami/tests/ci.sh` gets one block; `k/0008-romm` also changes that file, so merging
  that branch later may need its blocks put side by side, a mechanical fix.
- **Changed output.** If `slim()` dropped or changed a value, the card's files or `gamelist.xml` would change. Three
  checks guard it: the unit tests (every accessor answers the same, A4), upstream's `sync.sh` against the fixed build
  (A7), and the comparison with `main`'s build on a library of edge cases (final check, A4).
- **The 64 MB bound on a CI host.** The baseline resident size (libcurl and OpenSSL loaded through `dlopen`) varies
  by host. The fixed build is expected to peak at about 35 MB, and upstream's at about 300 MB. If the measured peak
  leaves less than 1.5 times headroom under 64 MB, the bound is changed and the change recorded under *Deviations*.
- **The test's time in CI.** Three listings of 2400 generated ~24 KB entries (about 60 MB of JSON each) from a
  Python stand-in should take a few seconds. Real downloads are only in the compare mode (local, a small library).
- **Boards.** Nothing board-specific. Only the RG DS Plus is at hand to test. The RG DS shares the SoC and runs the
  same binary, and its RAM is no larger, so it gains the same.
- **The cause isn't confirmed on the device** (investigation, question 5: test after the fix only). If the freeze
  has another cause as well, M1 and M2 still freeze, and the memory line or the `VmHWM` loop shows whether memory is
  still the cause.

## Steps

Each step is one commit, and leaves the tree building and `make test` passing.

### 1. 50 games a page, each kept small, with a test of a large library

This is a bug fix, so the step starts with the test that reproduces it. Before `sync.cpp` is touched, the stand-in and
`romm-list.sh`'s listing checks are written and run against upstream's code. They fail with
`romm-list: games peaked at <~300> MB, over the 64 MB bound` and
`romm-list: an /api/roms request without limit=50: ...limit=500...`. The fix then makes them pass. All of it is
one commit.

- **Files**:
  - `ui/src/kiwami/syncmem.h` (new)
  - `ui/src/sync.cpp` (hook, 5 `kiwami:` lines)
  - `ui/Makefile` (hook, 1 `kiwami:` line)
  - `kiwami/tests/romm-big.py` (new)
  - `kiwami/tests/romm-list.sh` (new)
  - `kiwami/tests/syncmem-units.cpp` (new)
  - `kiwami/tests/ci.sh` (changed)
  - `.workspace/0022-romm-sync-memory/plan.md` (changed, Progress)
- **Change**:
  - **`ui/src/kiwami/syncmem.h`**: header-only, `#pragma once`, includes `"../json.h"`, namespace
    `banana::kiwami::syncmem`. The header comment says what it is for: banana-sync's RomM listing held to the fields
    `Romm::manifest()` reads (`ui/src/sync.cpp`). It also says that a key `manifest()` starts reading has to be added
    to the lists below, and that `kiwami/tests/romm-list.sh` checks this.
    ```cpp
    constexpr int PAGE = 50;	// RomM's games asked for this many at a time (upstream: 500)
    // the members of a game Romm::manifest() reads as scalars, of each of its files[], and of its metadatum
    inline const char* const GAME_KEYS[] = { "id", "platform_id", "fs_name", "name", "fs_size_bytes",
        "has_multiple_files", "missing_from_fs", "summary", "path_cover_large" };
    inline const char* const FILE_KEYS[] = { "id", "file_name", "file_size_bytes", "sha1_hash", "md5_hash" };
    inline const char* const META_KEYS[] = { "first_release_date" };
    inline const char* const META_FIRST[] = { "genres", "publishers" };	// only their first element is read
    inline JPtr slim(const JVal& game);
    ```
    `slim()`'s rules make each answer the same as the original's (`ui/src/json.cpp:9-41`):
    - **A scalar member** (GAME_KEYS, FILE_KEYS, META_KEYS) is kept when present: its node is shared if it is a
      string, number, boolean or null. An array or object in its place becomes a fresh null node. It stays
      *present*, because `manifest()` tests `files[0]->get("file_size_bytes")` for presence, and the
      `str/num/flag` default of a null node is the same as that of an array or object.
    - **`files`**: an array becomes a new array with as many elements, each a new object holding that element's
      FILE_KEYS members by the rule above (a non-object element gives an empty object). `files` that isn't an array
      is dropped (`manifest()` checks `isArr()`).
    - **`metadatum`**: an object becomes a new object with META_KEYS by the scalar rule, and each of `genres` and
      `publishers` that is an array becomes a new array: empty if it was empty, otherwise holding one element,
      `arr[0]`'s node if it is a string and a fresh null node otherwise (`manifest()` reads `arr[0]->s`, which is
      `""` for anything but a string). A `metadatum` that isn't an object is dropped (`get()` on it finds nothing).
    - **Anything else is left out**: `merged_ra_metadata`, `igdb_metadata` and the other provider blocks,
      `merged_screenshots`, and every other member. A `game` that isn't an object gives an empty object.
    - **The first member of a name** is the one kept, as `JVal::get()` finds it.
  - **`ui/src/sync.cpp`** (line numbers at `2fdbd3d`), each changed line ending in a `kiwami:` comment, with no
    other change around it:
    - after `:25` (`#include "sync_util.h"`), a new line:
      `#include "kiwami/syncmem.h"	// kiwami: RomM's game list kept small (banana-sync's memory)`
    - `:903`: `off += 500` becomes `off += banana::kiwami::syncmem::PAGE` (`// kiwami: 50 a page, upstream 500`).
    - `:906`: `"&limit=500&offset="` becomes `"&limit=" + std::to_string(banana::kiwami::syncmem::PAGE) + "&offset="`,
      and `&with_provider_metadata=false` is added at the end of the query string
      (`// kiwami: 50 a page, and no provider metadata (RomM after 5.4.1)`).
    - `:915`: `romJson.push_back(it);` becomes `romJson.push_back(banana::kiwami::syncmem::slim(*it));`
      (`// kiwami: only what manifest() reads; the page is freed`).
    - `:919`: both `500`s become `banana::kiwami::syncmem::PAGE` (`// kiwami: 50 a page`). `PAGE` is an
      `int` constant expression, so `items->arr.size() < PAGE` gives no sign-compare warning, as the literal didn't.
    - `romJson`'s type, `roms`, `manifest()` and the saves code stay as they are.
  - **`ui/Makefile`**: after the `banana-sync:` rule (`:39-40`), one line:
    `banana-sync: src/kiwami/syncmem.h	# kiwami: the fork's RomM listing, a header sync.cpp includes`.
  - **`kiwami/tests/romm-big.py`**: a stand-in RomM 5.4.1 with a generated library (the shape of `fake-romm.py`'s
    items, plus RomM's heavy blocks). Python standard library only. Usage:
    `romm-big.py <port file> <log file> <games per system> <achievements per game>`. It listens on 127.0.0.1 and
    writes its port to `<port file>` once ready (as `ra-server.py` does), and logs every request's method, path and
    query to `<log file>`. It answers:
    - `GET /api/heartbeat` with version 5.4.1.
    - `GET /api/platforms` with seven platforms, ids 1-7: `nds`, `gba`, `gbc`, `snes`, `genesis` (fs_slug
      `megadrive`), `psx`, `3ds`.
    - `GET /api/roms`, honouring `platform_ids`, `limit` and `offset`, with `total`, and ignoring unknown parameters
      (as RomM 5.4.1 does, `with_provider_metadata` included). Its items are generated a page at a time,
      deterministically from (platform, index).
    - `GET /api/roms/{id}/content/{name}` and `GET /api/roms/{file id}/files/content/{name}` with the content
      `rom <id>\n`, and `files[]`' sizes, SHA-1 and MD5 computed from it.
    - `GET /assets/romm/resources/roms/...` with a small fixed PNG or JPEG.
    - `GET /api/saves` with two saves (GBA and DS game 10), `GET /api/saves/{id}/content` with their bytes,
      `GET /api/states` with `[]`.
    - Any `Bearer rmm_...` token is accepted; anything else gets 403.

    Every game `i` of platform `p` has: `id = p*100000 + i`, `fs_name` `Game {i:04} (USA).{ext}`, `fs_name_no_ext`,
    `name` `Game {i:04}`, a summary of about 300 characters, `metadatum` with genres `["Action", "Puzzle"]`,
    publishers `["Banana"]` and `first_release_date` in ms, a `.png` cover with `?ts=`, one file, and the heavy
    blocks: `merged_ra_metadata` with `<achievements per game>` achievements of RomM's 13 fields, plus
    `igdb_metadata`, `ss_metadata`, `launchbox_metadata` and `merged_screenshots` (about 24 KB of JSON a game at 40
    achievements). These indexes are the edge cases:
    - 1: `missing_from_fs: true`
    - 2: `summary` null, no `metadatum`, `path_cover_large` null
    - 3: `genres: []`, `publishers: "Banana"`, `first_release_date` in seconds, a `.jpg` cover
    - 4: `name: 1942` (a number), `fs_name` `1942 (USA).{ext}`
    - 5: a tab in `name`
    - 6: `fs_name` `../Escape (USA).{ext}`
    - 7: `has_multiple_files: true`, a folder `fs_name`, with a `.cue`, two `.bin`s and a `.txt`
    - 8: `files: []` (no hash, the size from `fs_size_bytes`)
    - 9: `platform_id` of another platform
    - 11: one file whose `file_size_bytes` is null

    So each cartridge system lists `N - 4` games (1, 6 and 7 skipped, 9 not listed), and `psx` `N - 3` (7 becomes a
    disc set with an `.m3u`).
  - **`kiwami/tests/romm-list.sh`** (`set -e` style as the fork's other tests, temp dir `$T`, cleanup trap killing
    the stand-in):
    1. Build `$T/banana-sync` from `ui/src/sync.cpp ui/src/json.cpp` with `${CXX:-g++} -std=c++17 -O2 -Wall
       -Wno-unused-function -ldl` (`sync.sh`'s line). Build `$T/syncmem-units` from
       `kiwami/tests/syncmem-units.cpp ui/src/json.cpp` with `-std=c++17 -Wall -Werror -I ui/src`, and run it.
    2. **The guard** (A8). awk takes the lines of `ui/src/sync.cpp` from the `bool manifest(` that follows
       `struct Romm : Backend` up to `bool matchRom(`. `grep -o '\(str\|num\|flag\|get\)("[a-z_0-9]*"'` takes the
       keys out of them. Each key has to appear quoted in `ui/src/kiwami/syncmem.h`, or the script fails with
       `romm-list: Romm::manifest() reads "<key>", which syncmem.h drops`. If no keys are found, it fails with
       `romm-list: Romm::manifest() not found in ui/src/sync.cpp`.
    3. Start `romm-big.py` with 400 games a system and 40 achievements. Make a card in `$T/card` (`banana/`, `run/`,
       `roms/`) whose `banana/romsync.json` is `{"backend": "romm", "server": "http://127.0.0.1:<port>", "token":
       "rmm_test", "name": "test"}`. `bs` runs banana-sync on it with `--root --run --status`, as `sync.sh` does.
    4. A helper `peak <out> <cmd...>` runs the command under
       `python3 -c 'import resource, subprocess, sys; ...'`, writes its stdout to `<out>`, and records
       `ru_maxrss` (kB) of the child.
    5. `games` (A1, A3, A4): it exits 0 with 2377 lines (5 × 396 + 397), and includes a line for
       `gba/1942 (USA).gba`, a `psx` line whose key is under `psx/.discs/Game 0007 (USA)/`, and game 5's title
       with a space where the tab was. Its peak is under 65536 kB, or it fails with
       `romm-list: games peaked at <n> MB, over the 64 MB bound`. The stand-in's log for this run has exactly 48
       `/api/roms` requests (8 for each of six platforms) and none with `platform_ids=7`. Each has `limit=50` and
       `with_provider_metadata=false`, or it fails with `romm-list: an /api/roms request without limit=50: <line>`.
    6. `sync --dry-run` (A2, A4): it exits 0. Its `+ ` lines number as many as its `roms:` line's `<n> to
       download`, and include `+ psx/Game 0007 (USA).m3u` and `+ gba/1942 (USA).gba`, and none for
       `Escape (USA)` or for `Game 0001`. It peaks under 64 MB.
    7. `sync --saves-only` (A2, A5): it exits 0, `$T/card/saves/Game 0010 (USA).srm` and
       `$T/card/saves/nds/Game 0010 (USA).sav` hold the stand-in's bytes, and it peaks under 64 MB.
    8. It prints one line: `romm-list: games <a> MB, dry run <b> MB, saves <c> MB peak; 48 requests of 50`.
  - **`kiwami/tests/syncmem-units.cpp`** (A4), built and run by `romm-list.sh`, in the `CHECK`/`EQ` style of
    `units.cpp`:
    - a parsed game with every edge above (null, numeric, array-in-place-of-scalar members, `files` of two
      elements, one not an object, `metadatum` with a numeric `genres[0]` and an empty `publishers`, and duplicated
      members) gives, through `slim()`, the same answers as the original for every key `manifest()` reads:
      `str`/`num`/`flag` with and without defaults, `get(...) != nullptr`, `files`' size and each file's five
      fields, `genres[0]->s`, `publishers[0]->s`, `first_release_date`;
    - the slimmed object's member names are a subset of the four lists plus `files` and `metadatum`;
    - a non-object game gives an empty object;
    - **the page is freed**: with a `std::weak_ptr` to the original's `merged_ra_metadata` node, after the parsed
      page is reset while the slimmed copy is kept, the weak pointer has expired.
  - **`kiwami/tests/ci.sh`**: a block after the units' block (before `tailscale-ui`):
    `# RomM's game list in banana-sync: 50 a page, each game kept small, against a large made-up library` and
    `kiwami/tests/romm-list.sh`. It doesn't need `ui/banana-ui`, and it builds its own banana-sync.
- **Tests**: before the fix, `kiwami/tests/romm-list.sh` fails as described above. After it, these pass:
  `kiwami/tests/romm-list.sh` alone, `make test` (A7, upstream's `sync.sh` against the fixed build),
  `make -C ui banana-sync` (touching only `syncmem.h` then rebuilds it), `kiwami/tests/ci.sh .buildroot`, and
  `git grep -n 'kiwami:' -- ui/src/sync.cpp ui/Makefile`, which lists the 5 + 1 new lines.
- **Commit**: `RomM sync memory: 50 games a page, each kept to the fields banana-sync reads`

### 2. The sync's peak memory in its report

- **Files**:
  - `ui/src/kiwami/syncmem.h` (changed)
  - `ui/src/sync.cpp` (hook, 1 `kiwami:` line)
  - `kiwami/tests/syncmem-units.cpp` (changed)
  - `kiwami/tests/romm-list.sh` (changed)
  - `.workspace/0022-romm-sync-memory/plan.md` (changed, Progress)
- **Change**:
  - **`syncmem.h`**:
    - `inline std::string peakFrom(const std::string& status)`: the number on the `VmHWM:` line of a
      `/proc/<pid>/status` text, as `"<n> kB"`, or `"unknown"` when there is no such line or no digits on it.
    - `inline std::string peak()`: `peakFrom` of `/proc/self/status` read whole (`"unknown"` if it can't be opened).
  - **`ui/src/sync.cpp`**: right after `doSync`'s `say("done: ...")` (`:1733` at `2fdbd3d`), a new line:
    `say("memory: peak %s", banana::kiwami::syncmem::peak().c_str());	// kiwami: for reports of a sync that strains the handheld`.
    Nothing reads the report's last line (`settings.cpp` reads the status file, and `sync.sh` greps for other
    words), so nothing moves. Romhost syncs get the line too (`doSync` is shared), which is harmless.
- **Tests**:
  - `syncmem-units.cpp`: `peakFrom("VmPeak:\t  9000 kB\nVmHWM:\t    2345 kB\nVmRSS:\t 100 kB\n")` is `"2345 kB"`;
    `peakFrom("VmRSS:\t 100 kB\n")` and `peakFrom("VmHWM:\n")` are `"unknown"`; `peak()` on the host matches
    `^[0-9]+ kB$`.
  - `romm-list.sh`: the dry run's stdout has a `memory: peak <n> kB` line right after its `done:` line. After the
    saves-only sync, `$T/card/banana/logs/sync.log`'s last two lines are `done: ...` and `memory: peak <n> kB`,
    with `<n>` under 65536 (A6).
  - The commands of step 1 pass again (`make test` covers `sync.sh`'s report checks).
- **Commit**: `RomM sync memory: the sync's peak memory at the end of its report`

### 3. Final check

- `make test`, `make -C ui`, `make -C ui banana-sync CXXFLAGS="-O2 -Werror"` (fails only if upstream's
  `sync.cpp` already warns; if so, compare its warnings with `main`'s and check none is on a `kiwami:` line),
  `make -C system/bananad CFLAGS="-O2 -Werror"`, `kiwami/tests/ci.sh .buildroot`.
- **The comparison with `main`'s build** (local, A1, A4). It adds a `--compare <other banana-sync>` mode to
  `kiwami/tests/romm-list.sh`, written in this step:
  - On the 400-a-system library, `games` from both are identical. The dry runs' `+ ` and `- ` lines are identical,
    and so are their `roms:` lines once the free-space figure is cut. The other build must peak at 4 times the bound
    or more, or the script fails with `romm-list: the other banana-sync peaked at <n> MB: the library doesn't
    reproduce upstream's problem`.
  - On a second stand-in with 30 games a system and the same edge cases, a real `sync` and then a `sync
    --saves-only` with each build, each onto its own card. Then `diff -r` of the two cards' `roms/` and `saves/`,
    and the two `banana/romsync.json` files' `state` objects compared with `python3 -c` (`json.load`), must show no
    difference.

  main's build comes from read-only git, with no worktree or checkout:
  ```sh
  M=$(mktemp -d); for f in sync.cpp sync_util.h json.cpp json.h; do git show main:ui/src/$f > "$M/$f"; done
  g++ -std=c++17 -O2 -Wall -Wno-unused-function -o "$M/banana-sync" "$M/sync.cpp" "$M/json.cpp" -ldl
  kiwami/tests/romm-list.sh --compare "$M/banana-sync"
  ```
  Both peaks go in the plan's notes.
- **The hooks**: `git diff --stat main...HEAD` and `git grep -n 'kiwami:'` (the commands in `docs/FORK.md`) agree,
  and this feature adds only `ui/src/sync.cpp`'s 6 lines and `ui/Makefile`'s 1 line.
- **With the fork's options off, `make test`'s renders in `ui/tests/out/` match `main`'s** (A10: banana-ui isn't
  touched). The image's rootfs gains no file.
- `kiwami/DEPENDENCIES.md`: unchanged, because the feature adds no dependency.
- **`docs/FORK.md`**, *Taking in upstream*, item 3 (*What the fork shadows*), gets a sentence at the end (question 1):
  "And RomM's game list in banana-sync: `ui/src/sync.cpp`'s `Romm::list()` asks for 50 games a page and keeps each as
  `syncmem::slim()`'s copy (`ui/src/kiwami/syncmem.h`), and `doSync` ends its report with the peak memory. A field
  upstream starts reading in `Romm::manifest()` has to be kept by `slim()`, and `kiwami/tests/romm-list.sh` stops on
  one it drops. If upstream fixes the listing's memory its own way, the merge takes upstream's and drops the fork's
  lines and header." `docs/ARCHITECTURE.md` and `CLAUDE.md` don't describe the listing, so they stay as they are.
- The manual tests below all passed.
- Every acceptance criterion is met: each is listed by at least one automated or manual test, and those passed.
- **Files**:
  - `kiwami/tests/romm-list.sh` (changed, `--compare`)
  - `docs/FORK.md` (changed)
  - `.workspace/0022-romm-sync-memory/plan.md` (changed: Progress, manual test outcomes, notes)
- **Commit**: `RomM sync memory: the final check, compared with upstream's banana-sync, and a note for merges`

## Automated tests

| Test | Where | What it checks | Criteria |
|---|---|---|---|
| Unit tests of `slim()` | `kiwami/tests/syncmem-units.cpp` (run by `romm-list.sh`) | every key `manifest()` reads answers the same through the copy, edge cases included; only the kept members are there; the page's heavy nodes are freed | A4 |
| Unit tests of `peakFrom()`/`peak()` | `kiwami/tests/syncmem-units.cpp` | `VmHWM` read; `unknown` without it | A6 |
| The guard | `kiwami/tests/romm-list.sh` (CI, via `ci.sh`) | every key `Romm::manifest()` reads is kept by `syncmem.h` | A8 |
| Choose games on a large library | `kiwami/tests/romm-list.sh` + `romm-big.py` | under 64 MB; 48 requests, all `limit=50` + `with_provider_metadata=false`, no `3ds`; 2377 games with the edge cases right | A1, A3, A4 |
| Sync now (dry run) and saves-only | `kiwami/tests/romm-list.sh` | under 64 MB each; the saves downloaded by name; the `memory: peak` line on stdout and last in `sync.log` | A2, A5, A6 |
| Upstream's sync tests | `ui/tests/sync.sh` (`make test`) | pairing, syncs, gamelists, covers, PS1 discs, saves, conflicts, second card, https, all unchanged, against the fixed build | A7, A4 |
| Comparison with `main`'s build | `kiwami/tests/romm-list.sh --compare` (local, final check) | identical `games`, dry-run lists, cards, `state` and saves; `main`'s build peaks 4 times the bound or more | A1, A4 |
| Renders | `make test` vs `main`'s (final check) | banana-ui unchanged | A10 |

## Manual tests

On the device, for what headless tests can't reach. Logs are fetched over SSH/SFTP (Settings > Network > SSH) or from
the card on a computer (`banana/logs/` on the card is `/data/banana/logs/` on the device). Only an RG DS Plus is at
hand, so all of them run on it. The RG DS runs the same binary on the same SoC with no more RAM.

Before M1: if the handheld freezes after boot before the update is in (Auto sync "And on Wi-Fi", investigation
*Oversights* 6), set `homesync_auto = off` in `banana/settings.conf` on the card from a computer, update, and then
put it back for M4.

| # | Board | Steps | Expected | Logs to collect | Criteria | Outcome |
|---|---|---|---|---|---|---|
| M1 | rgds-plus | Update to this branch's image. Over SSH, `free -m`. Settings > Network > Home server > **Sync now**, with the user's RomM; move around the menu while it runs. When it ends, `free -m` again. Run Sync now a second time (nothing new). | Both syncs end with a result in Settings; the menu answers throughout; nothing freezes. `sync.log` ends with `done:` and then `memory: peak <n> kB`, with `<n>` under 100000 (about 50000 expected for 6000 games). | `/data/banana/logs/sync.log` and `sync.log.1` (both syncs); the two `free -m` outputs | A2, A6, A9 | not run |
| M2 | rgds-plus | Over SSH, start `while p=$(pidof banana-sync); do grep VmHWM /proc/$p/status; sleep 1; done; echo done` and leave it waiting (start it a second time if it ends before the list is opened). Open Settings > Network > Home server > **Choose games**. Scroll through two systems' lists. | The list appears with every system's games (1000+ each); no freeze. The loop's last `VmHWM` is under 100000 kB. | the SSH loop's output (copied from the terminal) | A1, A3, A9 | not run |
| M3 | rgds-plus | Set Auto sync to **Leaving a game**. Start a game whose save is on RomM (or make a new save), then quit it. | Back in the menu without a freeze; the save is uploaded or downloaded as upstream would. `sync.log` shows the saves lines, `done:` and `memory: peak`. | `/data/banana/logs/sync.log` | A2, A5, A6, A9 | not run |
| M4 | rgds-plus | Set Auto sync to **And on Wi-Fi**. Reboot, and use the menu for 2 minutes after Wi-Fi connects (the sync starts 25 s after). | The menu stays usable; a new `sync.log`, timed after the boot, ends with `done:` and `memory: peak`. | `/data/banana/logs/sync.log` | A6, A9 | not run |

Notes from the runs (what the logs showed, what was fixed):

## Deviations

Where the implementation turned out different from this plan, why, and what changed (dated).

## Progress

A step is marked done in its own commit, so the commit column holds the subject (a commit can't carry its own hash).

| Step | Status | Commit |
|---|---|---|
| 1. 50 games a page, each kept small, with a test of a large library | todo | |
| 2. The sync's peak memory in its report | todo | |
| 3. Final check | todo | |
