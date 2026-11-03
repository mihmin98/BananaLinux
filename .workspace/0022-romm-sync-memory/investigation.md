# 0022 RomM sync memory: investigation

Issue: #22 · Branch: `k/0022-romm-sync-memory` · Status: refined · Confidence: 91% (proceed)

## The request

With the RG DS Plus paired to a RomM server through upstream's Home server (Settings > Network > Home server),
**Sync now** and **Choose games** freeze the whole device until it is restarted. The user's RomM library has 1000+
games for each system, all with artwork, and RetroAchievements metadata turned on in RomM.

The cause, found in the code (*Current state*): `banana-sync` reads RomM's game list 500 games a page and keeps every
page's full JSON tree in memory until the end. Each game's entry carries all of RomM's provider metadata, the complete
RetroAchievements list included, none of which `banana-sync` uses. With no swap, a library this size outgrows the RAM
and the device thrashes instead of failing.

Decided with the user:

- **Option 1** (*Options*): the list read 50 games a page, each page reduced right away to the 18 fields
  `banana-sync` reads and freed before the next one, and `with_provider_metadata=false` asked of RomM. No streaming
  JSON parser.
- **A fork patch on `k/0022-romm-sync-memory`**, in upstream's `ui/src/sync.cpp`, marked `kiwami:`; not a branch off
  `main` offered upstream (*Oversights*, 5).
- **No setting**: the games, covers and `gamelist.xml` written to the card stay the same; only the requests and the
  memory change.
- **A peak-memory line in `sync.log`** at the end of each sync, for the device test and later reports.
- **The final check compares with `main`'s `banana-sync`**: both run against the same large fake library, their
  `games` output and the files they write diffed (local, not CI).
- **The device test is after the fix only**: the cause isn't confirmed on today's image first.
- **A note in `docs/FORK.md`** (*Taking in upstream*) about the patch, so a merge from `main` checks it.

Out of scope: the file entries RomM leaves out of the list since 5.3 unless asked for (`with_files`, *Suggestions*,
3), its own issue; dropping duplicate ids across pages (*Suggestions*, 2); a streaming JSON parser (*Options*, 2);
the saves and states lists (small, *Current state*); the Romhost backend.

## Oversights in the requirements

1. **It isn't only Sync now and Choose games.** Every RomM path goes through the same `Romm::list()`: Choose games
   (`banana-sync games`, `ui/src/sync.cpp:1792-1801`), Sync now (`doSync`, `:1703` through `manifest`, `:927`), a
   saves-only sync (`Romm::saves`, `:1051`: the saves are matched to the games by name, so the list is read even
   then), the automatic sync on leaving a game (`ui/src/main.cpp:2909-2913`, `--auto quit`, saves only) and the one
   25 s after Wi-Fi connects (`system/rootfs/usr/libexec/banana/wifi:114`, `--auto wifi`). With Auto sync on, the
   device can freeze right after quitting a game, or shortly after every boot that joins Wi-Fi ("And on Wi-Fi").
   *Proposed*: nothing more; the fix is in `list()`, so it covers all of them. The device test exercises Sync now,
   Choose games and a saves-only sync.

2. **Skipping systems doesn't help Choose games.** `banana-sync games` lists with an empty skip set
   (`ui/src/sync.cpp:1801`, `b->manifest(m, {}, err)`), so every system RomM has that Banana knows is read, Skip or
   not. Setting systems to Skip only shrinks Sync now's list. (This corrects the workaround given before the
   investigation.) *Proposed*: leave as upstream has it; with the fix, the whole list fits anyway.

3. **Choose games runs without the sync lock.** `games` returns before the lock is taken (`ui/src/sync.cpp:1792-1824`,
   lock at `:1834`), so it can run beside an automatic sync, each with its own copy of the list in memory. With the
   fix, two compact lists are small. *Proposed*: leave it.

4. **The page size costs requests.** 50 a page instead of 500 is 10 times the requests: 160 for 8000 games. The
   server's work per game is the same (it serializes each game once either way), and a request on the LAN costs
   tens of ms, so a few seconds in all. *Decided*: 50 (as the fork's own RomM client on `k/0008-romm` uses).

5. **An upstream file changed beyond a hook.** `docs/FORK.md` keeps upstream's files to small `kiwami:` hooks, and a
   fix meant for upstream on a branch off `main`. This rewrites `Romm::list()` and `Romm::manifest()` (about 40
   lines) and adds a log line in `doSync`. Upstream changes `ui/src/sync.cpp` often (8 commits from 26 Oct to 1 Nov,
   `git log main -- ui/src/sync.cpp`), so a later merge from `main` is likely to conflict there. *Decided*: a fork
   patch on the feature branch, its changed blocks marked `kiwami:`, not offered upstream. If upstream fixes the
   freeze its own way, the merge takes upstream's and drops the fork's.

6. **The device may already be in trouble while booting.** With Auto sync on "And on Wi-Fi", the freeze can come
   25 s after each boot (*Oversights*, 1), before the user can change the setting. *Proposed*: the manual test
   notes it; the fixed image clears it. If it happens before the update is in, Auto sync can be turned off by
   editing `homesync_auto = off` in `banana/settings.conf` on the card from a computer.

## Suggestions

1. **A peak-memory line in `sync.log`.** At the end of each sync's report (`ui/src/sync.cpp:1733`, the `done:` line),
   `banana-sync` says its peak memory (`VmHWM` from `/proc/self/status`). One more changed line in upstream's file;
   `ui/tests/sync.sh` greps the report for other words, so nothing breaks. The `games` command writes no report, so
   Choose games' peak is read over SSH in the device test. *Decided*: in.

2. **Drop duplicate ids across pages.** RomM's offset paging over a library that changes while it's read (a scan
   adding a game) can return a game twice or skip one. Smaller pages make the window a little wider; it can happen
   with 500 a page today, and nothing has shown it. A 2-line guard. *Decided*: left out.

3. **`with_files` (RomM 5.3 and later).** RomM's `/api/roms` returns no `files[]` unless `with_files=true` is asked
   for (`backend/endpoints/roms/__init__.py` at RomM 5.3.0, 5.4.0 and 5.4.1: default `False`), and `banana-sync`
   doesn't ask. So on a real RomM, downloads aren't checked against their SHA-1/MD5, and multi-disc PS1 games are
   skipped (`ui/src/sync.cpp:939-976`: no files, `has_multiple_files` true, nothing to make the `.m3u` from).
   Upstream's stand-in (`ui/tests/fake-romm.py:66-84`) always sends `files[]`, so its tests don't see it.
   *Decided*: a separate issue. The compact form keeps the five file fields, so that fix is one parameter.

4. **A note for merges from upstream.** `docs/FORK.md`'s *Taking in upstream* lists what the fork changes in or
   shadows of upstream's files; this patch could get a line there ("the RomM listing in `ui/src/sync.cpp`: if
   upstream fixes its memory, drop the fork's"). *Decided*: in, added in the plan's last step.

## Current state

**What runs.** `banana-sync` is upstream's (`ui/src/sync.cpp`, built by `ui/Makefile:39` from `sync.cpp` and
`json.cpp` only, installed as `/usr/bin/banana-sync`, `buildroot/package/banana-ui/banana-ui.mk:16`). The menu starts
it in the background: Sync now through `Settings::hsRun("sync")` (`ui/src/settings.cpp:6309-6322, 6342-6351`),
Choose games through `pickJob` (`ui/src/settings.cpp:4323`, `banana-sync ... games`). The fork has no code in this
path: `git diff main...kiwami -- ui/src/sync.cpp ui/src/json.cpp ui/src/json.h` is empty, and upstream's `main` on
GitHub is still `803e61c` (`git ls-remote upstream refs/heads/main`), with nothing newer on `sync.cpp`.

**The listing.** `Romm::list()` (`ui/src/sync.cpp:887-923`):

- `GET /api/platforms`; each platform whose slug maps to a Banana folder and isn't skipped is kept (`:891-900`).
- For each, `GET /api/roms?platform_ids=<id>&platform_id=<id>&limit=500&offset=<n>&order_by=name&with_char_index=false&with_filter_values=false&with_rom_id_index=false`
  until a short page or `total` (`:903-920`).
- Every item's `JPtr` (the parsed object, its whole subtree) is pushed onto `romJson` (`:886, :915`), and a compact
  `RommRom { id, folder, stem, fsName }` onto `roms` (`:879, :916`). The page's text (`Resp::body`) goes when the loop
  turns; the trees stay until the process ends.
- `Romm::manifest()` (`:925-1013`) then walks `romJson` to build the `Manifest` (`MGame`: title, files, gamelist
  fields; `:549-567`). The saves sync (`:1048-1171`) uses only `roms` and `platforms`.

**The 18 fields it reads** (every `->str/num/flag/get` on an item, `:914-1007`):

| Where | Fields |
|---|---|
| the game | `id`, `platform_id`, `fs_name`, `fs_name_no_ext`, `name`, `fs_size_bytes`, `has_multiple_files`, `missing_from_fs`, `summary`, `path_cover_large` |
| `files[]` | `id`, `file_name`, `file_size_bytes`, `sha1_hash`, `md5_hash` |
| `metadatum` | `genres[0]`, `first_release_date`, `publishers[0]` |
| the page | `items`, `total` |

**What RomM sends.** RomM 5.4.1's list endpoint returns `SimpleRomSchema` (`backend/endpoints/responses/rom.py`),
which carries every provider's metadata: `igdb_metadata`, `moby_metadata`, `ss_metadata`, `launchbox_metadata`,
`hasheous_metadata`, `flashpoint_metadata`, `hltb_metadata` and the others, `merged_screenshots`, and
`merged_ra_metadata` (`rom.py:449` at 5.4.1). The RetroAchievements block is the heavy one
(`backend/handler/metadata/ra_handler.py`): the game's dates, genres and companies, and its `achievements`, each with
13 fields (`ra_id`, `title`, `description`, `points`, `num_awarded`, `num_awarded_hardcore`, `badge_id`, `badge_url`,
`badge_url_lock`, `badge_path`, `badge_path_lock`, `display_order`, `type`). RomM's development branch has added
`with_provider_metadata` (default true) to null all of those; no release has it yet (absent at 5.3.0, 5.4.0, 5.4.1).
FastAPI ignores query parameters an endpoint doesn't declare, so sending it to 5.4.1 is harmless.

**How much memory.** `banana::parseJson` (`ui/src/json.h:11-36`) builds a `shared_ptr<JVal>` per value, each with a
string, two vectors, a double and a bool. Measured on the host (`json.cpp` built alone, RSS before and after
parsing a generated page):

| Page | Text | Tree |
|---|---|---|
| 500 games, IGDB-like metadata, no achievements | 1975 KB (4 KB a game) | 16.1 MB (×8.2) |
| 50 games, 50 achievements each | 1410 KB (29 KB a game) | 7.9 MB (×5.6, 160 KB a game) |

So with RetroAchievements on, about 160 KB a game stays in memory: 6000 games (1000 for each of six systems) is about
1 GB of trees. The image has no swap (no `swapon`/zram anywhere in `system/rootfs` or `kiwami/rootfs`); out of memory,
the kernel drops and re-reads the squashfs' code pages long before the OOM killer acts, which is the freeze seen. This
is inferred from the code and the measurement, not confirmed on the device (decided: test after the fix only).

**What already exists to build on.** `RommRom` (`:879`) is a compact per-game record already; the fork's own RomM
client on `k/0008-romm` reads `/api/roms` 50 at a time and turns each item into a small `Rom` on the spot
(`ui/src/kiwami/romm/api.cpp:263-287` on that branch), the pattern this fix follows. `say()` (`:112`) writes the
report to `banana/logs/sync.log` (`:88-120`). Upstream's tests run `banana-sync` against `ui/tests/fake-romm.py`
(RomM 5.4.1's endpoints, a library made from a folder of files, small items, `limit`/`offset` honoured,
unknown parameters ignored) in `ui/tests/sync.sh`, run by `make test` (`ui/tests/run.sh:895`).

## What has to change

- **`ui/src/sync.cpp`, `Romm`** (`:880-1013`): `romJson` gives way to a compact per-game record holding the 18 fields
  (the first genre and publisher only, `files[]` as a small vector of the five fields), filled from each page as it is
  parsed; the page's tree and text are freed before the next request. `limit=500` becomes `limit=50` (and the two
  `500`s of the stop test), `&with_provider_metadata=false` is added. `manifest()` builds the same `Manifest` from the
  compact records, in the same order. `roms` and the saves sync stay as they are. Marked `kiwami:`.
- **`ui/src/sync.cpp`, `doSync`** (`:1733`): a line after `done:` with the peak memory (`VmHWM`), marked `kiwami:`.
- **`kiwami/tests/`**: a test of the fork's own (a stand-in serving a large RomM library with RetroAchievements-sized
  items, generated, not from files) that runs `banana-sync games` and a sync, and checks the peak memory stays
  bounded, the requests are `limit=50` with `with_provider_metadata=false`, and the output lists every game. Run by
  `kiwami/tests/ci.sh` (it needs `make -C ui banana-sync`).
- **`docs/FORK.md`**: a line under *Taking in upstream* about the patch.

No setting, no package, no dependency (`kiwami/DEPENDENCIES.md` unchanged), nothing in the rootfs.

## Options

1. **Small pages, each reduced to the fields used and freed (chosen).** Peak memory is one page (about 1.4 MB of
   text and 8 MB of tree with heavy RetroAchievements data) plus the compact list (about 1-2 KB a game, mostly the
   summary: 8-16 MB for 8000 games). About 40 changed lines in one function pair, the same JSON parser. Costs more
   requests (*Oversights*, 4).
2. **A streaming parser.** The body parsed as curl hands it over, each game's fields picked out without a tree or the
   page's text. Near-constant memory, but a new incremental JSON parser (a few hundred lines) to write and test, to
   save the 10 MB a page that option 1 frees anyway. Rejected.
3. **A parser that skips what isn't needed.** Whole pages, but `merged_ra_metadata` and the other blocks skipped while
   parsing, so no tree is built for them. A middle ground if option 1 isn't enough; not needed by the numbers above.
4. **`with_provider_metadata=false` alone.** Fixes it at the source, but only on a RomM newer than 5.4.1, which isn't
   released. Kept as part of option 1, not instead of it.

## Risks

- **Performance and footprint**: memory goes down from up to about 1 GB to tens of MB; nothing runs while idle that
  didn't before; the rootfs is unchanged. A sync takes a few seconds more for the extra requests.
- **Conflicts with upstream**: `ui/src/sync.cpp` is upstream's and moves fast (*Oversights*, 5). A merge that brings
  changes to `Romm::list()`/`manifest()` conflicts there; one that adds a field read from `romJson` elsewhere fails to
  build (no `romJson` any more), which `make -C ui` shows at once.
- **Changed output**: the `Manifest` must come out the same, game for game and in order, or the card's files and
  `gamelist.xml` change. The final check's comparison with `main`'s build covers it.
- **Hardware and boards**: nothing board-specific; both share the SoC, and only the RG DS Plus is at hand to test.
  The RG DS's RAM is no larger, so it benefits the same.
- **The cause isn't confirmed on the device**: if the freeze has another cause as well, the test after the fix shows
  it (the sync still freezes), and `sync.log`'s memory line tells which.

## Testing

- **Headless, the fork's test** (`kiwami/tests/`): a generated library of a few thousand games with
  RetroAchievements-sized entries; `banana-sync games` and `banana-sync sync --dry-run` (or a real sync with tiny
  ROMs) against it; peak memory read from the child's `ru_maxrss` (or `VmHWM`), bounded at a fixed figure that the
  old code exceeds many times over; the requests' parameters from the stand-in's log; every game in `games`' output.
- **Headless, upstream's**: `make test` (it runs `ui/tests/sync.sh` against `fake-romm.py`: pairing, syncs, saves,
  conflicts, PS1 discs, https) passes unchanged.
- **Final check, local**: `main`'s `banana-sync` (from a `git worktree` of `main`) and the fixed one against the same
  library: their `games` output and the card they write (`roms/`, `gamelist.xml`, `romsync.json`'s files) diffed.
- **On the device (RG DS Plus, the user's RomM)**: Sync now and Choose games finish without a freeze; a saves-only
  sync too. To collect: `banana/logs/sync.log` (the `done:` and memory lines), over SFTP or from the card; for Choose
  games, `VmHWM` read over SSH from `/proc/<pid>/status` while `banana-sync ... games` runs, and `free -m` before and
  after.

## Open questions

1. Add a note about the patch to `docs/FORK.md`'s *Taking in upstream* (*Suggestions*, 4)? *Answered*: yes.
2. Where should the code go: a branch off `main` offered upstream, or a fork patch? *Answered*: a fork patch on
   `k/0022-romm-sync-memory`, marked `kiwami:`.
3. A setting for the change? *Answered*: no.
4. Include `with_files`? *Answered*: no, a separate issue.
5. The device test before and after, or after only? *Answered*: after only.
6. Peak memory: an SSH loop, or a line in `sync.log`? *Answered*: a line in `sync.log`.
7. Extras: compare with `main`'s output, drop duplicate ids? *Answered*: compare with `main`'s output only.
8. (Planning) Each game kept as a compact struct with `manifest()` rewritten to read it, or as a pruned copy of its
   JSON tree (only the fields `manifest()` reads) so that `manifest()` stays upstream's? *Answered*: the pruned tree,
   in a fork header (`ui/src/kiwami/syncmem.h`): about 6 marked lines in `ui/src/sync.cpp` instead of about 45, the
   output the same by construction, at about 5 KB a game instead of 1-2 KB.
9. (Planning) The pruning's unit tests in `kiwami/tests/units.cpp` or in a file of their own? *Answered*: their own
   (`kiwami/tests/syncmem-units.cpp`); `k/0008-romm` rewrites much of `units.cpp`.
10. (Planning) A `kiwami:` prerequisite line in `ui/Makefile` so banana-sync is rebuilt when only the header changes?
    *Answered*: yes.
