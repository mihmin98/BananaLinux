# 0039 RomM home server: investigation

Issue: #39 · Branch: `k/0039-romm-home-server` · Status: refined · Confidence: 91% (proceed)

## The request

Compare Banana's RomM support (game downloads and save sync) with RomM's two other device clients, **Grout** (Go, Linux
handhelds; read at `0bb35c6`, release v5.4.1.0) and **Argosy** (Kotlin, an Android launcher; read at `f30898ada`,
3.0.0-beta.6). Find what they do that Banana doesn't, what of it would improve RomM on Banana, and whatever else turns
up.

The comparison found that upstream already has an open, unmerged rework of the Home server that closes about half the
gap (*Current state*, *Upstream's pending rework*). It is called "Home server: RomM saves that follow the game, kept
safe, shared with Grout; RomM set up, games and saves from the menu", on its author's branch
`feat/home-server-onboarding`, with head `cab127ba` (88 commits, about 19,000 lines added, opened 14 Nov 2026). It's
called *the rework* below.

Decided with the user:

- **The rework is taken into the fork** at its head `cab127ba`, by a merge of that commit (not a cherry-pick), and is
  recorded in `docs/FORK.md` as pinned. The rework's later commits come in only when the user chooses, or when
  upstream merges it. Upstream merges pull request heads unchanged: the heads of six past pull requests are all
  ancestors of `main` (checked with `git merge-base --is-ancestor`). So once upstream merges the same commits, merging
  `main` into `kiwami` brings nothing new from them.
- **A roadmap plus a first feature.** This issue is the foundation:
  1. the rework taken in;
  2. the fork's two `banana-sync` patches re-ported onto it: 0022's memory fix and 0024's Show sync times;
  3. lighter per-game lookups;
  4. installs from RomM's web page;
  5. BIOS downloads;
  6. the Quick resume guard.

  Two follow-up issues are written out below (*Suggestions*, *Follow-ups*).
- **Save interop matters**: the user's saves have to work with Argosy (Android), Grout (another handheld) and RomM's
  web player. The rework's `autosave` slot is what makes that work.
- **Issue #8** (0008-romm, the fork's own RomM client, never built) is superseded by this issue. The user closes it.
- **The fork's additions live inside `banana-sync`**: sources in `ui/src/kiwami/romm/`, linked by a `kiwami:` line in
  `ui/Makefile`'s `banana-sync` rule, and called from `kiwami:` hooks in the rework's `sync.cpp` and `sync_romm.cpp`.
- **Installs from RomM's page** are collected at **Sync now** and **on Wi-Fi connect**, behind a setting that is off
  by default.
- **BIOS** files go to `/data/bios`, and the DS's also to DraStic's own folder under its own names, when they aren't
  there already.
- **Token scope**: one `kiwami:` hook adds `firmware.read` (and `collections.read`, for the follow-up) to the pairing's
  requested scopes. A handheld paired before is told to pair again when the BIOS page gets a 403.
- **Lookup for a game never synced**: before the game, RomM's name search on its platform. In Sync now, in the
  background, a hash lookup for what is still unmatched.
- **Region and version picker** (follow-up): one entry per game, with the versions chosen on A, as Argosy does.
- **`with_rom_id_index=false`**: measured on the user's server, with and without it, and dropped only if it is
  slower there (*Suggestions*, 9).

Out of scope (follow-ups, *Suggestions*): Get games' search, details and covers, RomM's collections and filters, the
region and version picker, play time sent to RomM, the vault's pruning. Left out altogether: RomM 5.5's snapshot sync
(5.5 is still alpha: 5.5.0-alpha.2, 15 Nov), screenshot uploads, syncing favourites and ratings to RomM, the "now
playing" heartbeat, a custom mapping from platforms to folders, and unpacking archives.

## The gap analysis

What each client has, against upstream `main` (the fork's `kiwami` is the same here, plus 0022 and 0024), the rework at
`cab127ba`, and where it lands in the fork.

| Feature | Grout | Argosy | Banana `main` | The rework | Fork |
|---|---|---|---|---|---|
| Pairing by RomM's device flow, with a QR code | yes | yes | code and URL as text | QR, X types a code | taken in |
| Certificate trust for self-signed https | skip-verify switch | no | on/off | trust one certificate | taken in |
| Save sync before a game | no (manual) | yes | no | yes, with a 20 s cap, B skips | taken in, made lighter (3) |
| Save sync after a game | no | yes | Auto sync, off by default | yes, with a banner | taken in |
| Saves in RomM 5.4's `autosave` slot (shared with Grout, Argosy, RomM's page) | yes | yes | **skips slotted saves** | yes, or a custom slot | taken in |
| Conflicts decided by content, not clocks | server-side `negotiate` | hashes | newer file time wins | three MD5s | taken in |
| Conflicts resolved on the device | yes, skip/local/remote | yes | no, a `.bak` | Manage saves | taken in |
| Save backups and their retention | `.backup/`, keep N | save cache | `.bak`, unbounded | vault, never pruned | follow-up C |
| Download one game on demand | yes | yes | no (all, or ticked) | Get games, a queue | taken in |
| Search the server's games | yes | yes, fuzzy | no | no | follow-up B |
| Game details (cover, summary, metadata) | yes | yes | no | no | follow-up B |
| Collections (normal, smart, virtual) | yes | yes | no | no | follow-up B |
| Filters (genre, region, ...) | yes | yes | no | no | follow-up B |
| Region and version choice | file version | sibling picker | no | no | follow-up B |
| BIOS (firmware) downloads | yes | yes | no | no | **this issue (5)** |
| Install pushed from RomM's web UI (RomM 5.4) | no | yes | no | no | **this issue (4)** |
| Play sessions sent to RomM | no | yes | no | no | follow-up C |
| Incremental list (`updated_after`) | yes, local cache | measured, not done | no | list cached, full refresh | **this issue (3)** |
| ROM lookup by hash (`/api/roms/by-hash`) | yes | no | no | no | **this issue (3)** |
| Quick resume / autoload guard | warns in its docs | yes | no | DS only | **this issue (6)** |
| Screenshot upload | scope only | yes | no | no | left out |
| Favourites, ratings and status to RomM | no | yes | no | no | left out |
| "Now playing" heartbeat | no | yes | no | no | left out (network during games) |
| RomM 5.5 snapshot sync | no | yes | no | no | left out (5.5 is alpha) |
| Unpack zip/7z downloads | yes | yes | no | zips checked by contents | left out (cores read zips) |
| Custom platform-to-folder mapping | yes | yes | fixed slugs | fixed slugs | left out |

Sources: Grout `docs/usage/{guide,save-sync,settings}.md`, `romm/endpoints.go:4-41`, `saves/flow.go:33-100`,
`settings/config.go:316-318`. Argosy `RomMApi.kt` (about 60 endpoints), `RomMCapabilities.kt:28-41`,
`data/install/PushInstallCoordinator.kt`, `docs/romm-sync-{rules,optimization}.md`, `docs/save-sync-flow.md`.

## Oversights in the requirements

1. **Upstream is already doing half of this.** The rework adds the sync before a game, the `autosave` slot,
   content-based conflicts, Manage saves, a vault, a journal against power cuts, Get games and QR pairing. Building any
   of that in the fork would duplicate it, and would conflict with it wherever upstream merges it. *Decided*: take the
   rework in at `cab127ba`, and build only what it lacks.

2. **Banana can't see Grout's or Argosy's saves today.** `ui/src/sync.cpp:1187` skips every server save that has a
   slot ("another client's versioned slots: left to it"). Grout writes to `autosave` (`settings/config.go:318`), and
   so does Argosy (`docs/save-sync-flow.md`, model point 7). Banana uploads without a slot, which Grout counts as
   `autosave` (`saves/flow.go:166`). So Grout and Argosy read Banana's saves, but Banana never reads theirs. Conflicts
   also go by clocks: `sync.cpp:1252` compares the card's file time with RomM's `updated_at`. *Decided*: the rework's
   slots fix both.

3. **The rework would bring back 0022's freeze on a large library, and before every game.** The rework's
   `Romm::list()` (`cab127ba:ui/src/sync_romm.cpp:103-158`) asks for 500 games a page with `with_files=true`. It keeps
   every page's whole JSON tree (`romJson.push_back(it)`, plus `allJson`), with RomM's provider metadata and
   RetroAchievements lists, exactly the problem 0022 fixed. And the sync before a game lists that game's whole system
   (`:120-124`, `:536-553`) before picking its ROM. With 1000+ games a system, that is a few hundred MB, and up to the
   20 s cap, before every game. *Decided*: re-port 0022's `syncmem` hooks into the rework's `list()`, and make the
   per-game sync not list at all (3, *What has to change*).

4. **The fork's 0022 and 0024 patches don't survive the merge.** A three-way merge in `/tmp` of the rework, its base
   `87e7e1ca` and `kiwami` (`git merge-file`; nothing was fetched) gives these conflicts:
   - `ui/src/sync.cpp`: 4 hunks. The fork's `Romm` hooks sit in code the rework moved to `sync_romm.cpp`, and 0024's
     `start()` and `report()` lines sit in a reworked `doSync`.
   - `ui/src/settings.cpp`: 2 hunks. The Home server rows, where `Kiwami::rows(*this, HS)` sits after Sync now, and the
     `ENTRY_KIWAMI` help.
   - `ui/src/settings.h`: 2 hunks. The `Page` and `Entry` enums both grew: the fork's `KIWAMI` against the rework's
     `SAVES` and `HOMESRV`.
   - `ui/src/main.cpp`: 1 hunk. The includes: the fork's three against the rework's `gamesync.h`.
   - `ui/Makefile`: 1 hunk. The `banana-sync` rule: the rework's new sources against the fork's header line.

   `.gitignore`, `run-retroarch` (the fork's `core-overrides` hook) and `rcK` merge cleanly. The rework also changes
   the RomM side of 0024's phases: RomM's games no longer mirror, so "Checking the card", "Removing" and "Gamelists"
   stay Romhost's only. `kiwami/tests/romm-list.sh`, `sync-times.sh` and `sync-times-ui.sh` assert on today's
   behaviour (Choose games, mirrored RomM games), and each has a `--compare` against a `main` build. *Decided*: re-port
   both, with the tests rewritten against the rework's behaviour.

5. **Installs from RomM's page need more than a claim.** In RomM 5.4.1:
   - A request is only accepted for a device whose capabilities say `{"remote_install": true}`
     (`backend/endpoints/device/install.py:184`, `handler/device_install/policy.py:16-25`). The rework never sets
     capabilities: it only reads the device, `cab127ba:ui/src/sync_romm.cpp:464,485`.
   - Requests expire after **2 days** by default (`DEVICE_INSTALL_REQUEST_TTL_DAYS`, `config/__init__.py:407`).
   - A claim returns the requests already taken too (`install.py:257-279`). A request stays `taken` until the device
     reports `done`, `already_installed` or `failed` with a reason of up to 500 characters (`install.py:59-75`).
   - A request carries the ROM's game, update and DLC files (`policy.py:31-43`).
   - The server can turn installs off (`DEVICE_INSTALL_ENABLED`) or exclude platforms
     (`DEVICE_INSTALL_EXCLUDED_PLATFORM_SLUGS`).

   *Decided*:
   - The setting's row sets and clears the capability (`PUT /api/devices/{id}`), with **Off** as the default.
   - Claimed requests go into the rework's Get games list (`homesync-get.txt`, `banana-sync get <key>`,
     `cab127ba:ui/src/sync.cpp:1706-1714`), so they download with its queue, its space check, `.part` resumes and
     "Games go to".
   - Each request is reported once its game is on the card, or failed with the rework's reason (a game of several
     files the menu can't start, a system Banana doesn't have, no space).
   - A request still waiting for the second card stays `taken` and is claimed again at the next sync.
   - A request the handheld didn't see within 2 days is RomM's to show as expired. The setting's help says so.

6. **The Wi-Fi hook only syncs when Auto sync allows it.** The rework's `wifi` script runs
   `banana-sync sync --auto wifi` 25 s after connecting (`cab127ba:system/rootfs/usr/libexec/banana/wifi:115`).
   `banana-sync` returns at once unless Auto sync is "Leaving a game and on Wi-Fi" (`cab127ba:ui/src/sync.cpp:1314`).
   *Decided*: a `kiwami:` hook in that early-return path claims and downloads installs when the setting is on,
   whatever Auto sync says. No change to the `wifi` script.

7. **BIOS needs a scope the pairing doesn't ask for.** `firmware.read` is missing from the requested scopes both on
   `main` (`ui/src/sync.cpp:1730`) and in the rework (`cab127ba:ui/src/sync.cpp:1040`). RomM's firmware endpoints
   require it (`backend/endpoints/firmware.py:155,183`). *Decided*: a `kiwami:` hook adds `firmware.read` and
   `collections.read`. A 403 tells the user to pair again.

8. **The DS's BIOS has two homes.** BananaDS and RetroArch read `/data/bios`
   (`system/rootfs/usr/libexec/banana/run-bananads:13`, `retroarch.cfg:42`). DraStic runs from `/data/drastic`
   (`run-drastic:6,18`) and looks for its own file names there. *Decided*: `/data/bios`, plus DraStic's copies when
   they aren't there. DraStic's exact folder and names are checked in the plan, against its binary or documentation.
   A file already there with the same hash is skipped. A different file of the same name is never overwritten, and is
   reported.

9. **Quick resume hides a pulled save.** With Quick resume on, `run-retroarch` makes RetroArch load the automatic
   state (`cab127ba:system/rootfs/usr/libexec/banana/run-retroarch:191-193`), so a newer save the sync before a game
   pulls is played under the old state. The rework guards only the DS (its `nds-saves --played` record). Grout only
   warns about it (`docs/usage/save-sync.md`, "Syncs can be obscured by autoload"). *Decided*: 0008's guard. When the
   sync before a game replaces a save, that game's `<stem>.state.auto` (in `/data/states`, flat as the saves are) is
   renamed to `.bak`, kept and never deleted.

10. **The rework changes upstream's defaults and behaviour, beyond RomM.** Taken in, these come before upstream ships
    them:
    - RomM's games come only through Get games (no more mirroring of every game).
    - Pairing turns on Auto sync and Sync before playing.
    - The two mGBA cores are patched so a GBA save is always 128 KB.
    - `bananad.c`, `rcK`, `nds-saves`, `run-bananads`, `run-drastic`, `run-retroarch` and `share` change.
    - New tests are added to `make test` and `system/tests/launchers-test.sh`.

    The fork's rule that changed behaviour sits behind a setting defaulting to upstream's doesn't apply: this *is*
    upstream's code, only early. *Decided*: accept it, and list in `docs/FORK.md` that `kiwami` carries the rework
    ahead of `main`, at `cab127ba`.

11. **The rework may change, or never be merged.** If upstream merges it squashed or rewritten, or drops it, the fork
    carries 19,000 lines in upstream's files that `main` doesn't have, and the next merge from `main` conflicts across
    them. *Decided*: `docs/FORK.md` records the pinned head and what to do in each case:
    - merged unchanged: nothing;
    - merged with more commits: they merge on top;
    - squashed or rewritten: resolve to upstream's version and re-port the fork's hooks;
    - dropped: decide then between keeping it as the fork's or reverting the merge.

    The `banana-merge-upstream` skill's report then has one more thing to check.

## Suggestions

1. **Lighter per-game lookups** (*decided*, this issue). The rework's sync of one game lists its whole system
   (*Oversights*, 3). In order:
   - The game's RomM id, remembered: the rework's slot records are keyed `<rom id>/<slot>` with the save's path
     (`cab127ba:ui/src/sync_romm.cpp:540-551`, `romsync.json`). One `GET /api/roms/{id}` confirms the game, instead of
     listing its system.
   - Else, RomM's search on that platform, `/api/roms?platform_ids=<id>&search_term=<stem>`
     (`backend/endpoints/roms/__init__.py:660`). The exact name is kept, as today's matching does.
   - In Sync now, in the background, games still unmatched are hashed and looked up with `/api/roms/by-hash`
     (`roms/__init__.py:1201-1223`). That also finds renamed files. The hash is remembered by size and modification
     time, so a 512 MB DS ROM is hashed once.

   Get games' cached list (`homesync-server-games.tsv`, `cab127ba:ui/src/sync.cpp:387-393`; 38.6 s for 742 games in
   the rework's own test) is refreshed with `updated_after` (`roms/__init__.py:617`). Deletions are found by the id
   set, `/api/roms/identifiers`, which Argosy found is the only safe way: absence from an incremental list isn't
   deletion (`docs/romm-sync-rules.md`, "A rom missing from a pass is not proof of deletion"). A full refresh stays
   available as the X it already has.

   Cost: a few hundred lines in `ui/src/kiwami/romm/`, plus hooks at the top of `list()` and in the sync of one game.
2. **Installs from RomM's web page** (*decided*, this issue): *Oversights*, 5 and 6. Browsing 10,000 games is better
   done on a phone with RomM's web UI than on a 640x480 list. Argosy does it (`PushInstallCoordinator.kt`), with a
   websocket that Banana leaves out (no idle network).
3. **BIOS downloads** (*decided*, this issue). On a new BIOS page under Home server:
   - `GET /api/firmware?platform_id=` for each platform mapped to a Banana system, listing what's missing (ticked) and
     what's there.
   - Download into `/data/bios` (and DraStic's copy, *Oversights*, 8), each file through `.part` and checked against
     RomM's MD5 or SHA-1.
4. **The Quick resume guard** (*decided*, this issue): *Oversights*, 9.
5. **Follow-up B: Get games for a big library** (*decided* as a follow-up). Its issue text is under *Follow-ups*.
   - A search through the keyboard, filtering the cached list on the card (instant, no server).
   - The selected game's cover, summary, year, genre and size on the top screen. The cover is fetched only after the
     selection rests, and cached on the card with a size limit.
   - RomM's collections (`/api/collections`, `/smart`, `/virtual`) and filters (genre, region, language) as ways in.
     This needs `collections.read`, added in this issue.
   - One entry per game, grouped by RomM's gallery key (the first of the IGDB, ScreenScraper, MobyGames, RA,
     Hasheous... ids, with the platform: Argosy's `RomMSiblingIdentity.groupKey`, matching RomM's `group_by_meta_id`).
     A on a group opens its versions (regions, revisions; hacks and translations told apart by RomM's file name tags).
6. **Follow-up C: play time and the vault** (*decided* as a follow-up). Its issue text is under *Follow-ups*.
   - Each play's start and end sent to RomM (`POST /api/play-sessions`), with the save sync after the game. Pending
     ones are kept on the card. Banana already measures each play (`ui/src/main.cpp:3087`, `lib.recordPlay`).
   - The vault pruned: keep the N newest copies a game, as Grout's "Save backups" setting does. **No limit** is the
     default (the rework's behaviour). The rework lists the unpruned vault among its own known limits.
7. **Left out** (not asked to be built; listed so they aren't re-derived):
   - RomM 5.5's snapshot sync: Argosy turns it on from the heartbeat's `SAVE_SYNC` block, and 5.5 is alpha.
   - Screenshot uploads.
   - Favourites, ratings and status sent to RomM (`PUT /api/roms/{id}/props`).
   - The "now playing" heartbeat (`POST /api/activity/heartbeat` every 90 s, network traffic during a game).
   - A custom mapping from platforms to folders.
   - Unpacking zip and 7z downloads: Banana's cores read zips, and the fork's RetroArch reads 7z.
8. **The rework's RomM-side defaults** (pairing turns on Auto sync and Sync before playing) are what the user wants
   for interop. Nothing to change.
9. **`with_rom_id_index=false`** (*decided*: measure). Argosy measured it 2-4x slower at deep offsets on RomM
   5.1.0-alpha.4: without the id index the server pages with `OFFSET`, with no covering index
   (`docs/romm-sync-optimization.md`, section 1). `main`, the rework and 0022's query all send it. With 0022's 200 a
   page, a 1000-game system means 5 pages at growing offsets. The final check times a full list on the user's server
   both ways, and the `syncmem` hook drops the parameter only if it is slower there.

## Follow-ups

Two issues for later. Their text, to be refined when each is investigated:

- **Get games for a big library: search, details, collections and filters, versions.** Suggestion 5.
- **RomM: play time sent to the server, and old save copies pruned.** Suggestion 6.

## Current state

**Upstream's Home server on `main`** (and `kiwami`), in `banana-sync`, `ui/src/sync.cpp` (1973 lines):

- Two backends behind one `Backend` (`:638`): Romhost (Homeport's protocol) and RomM (`:953-1305`).
- **Pairing** (`:1691-1761`): RomM's device flow (`/api/auth/device/init`, then polling `/token`), a client-token
  code, or an `rmm_` token pasted. The code and the URL are shown as text in Settings (`ui/src/settings.cpp:6912-6920`).
  The scopes are at `:1730`.
- **Games**: `Romm::list()` (`:960-995`) reads every mapped platform, 200 a page with the fork's 0022 patch
  (`ui/src/kiwami/syncmem.h:21`; upstream asks for 500). `manifest()` (`:996-1124`) turns it into files.
  - `syncRoms()` (`:1338-1606`) mirrors the server: every game of every system not set to Skip, or only those ticked in
    Choose games (`homesync_games = chosen`, `homesync-games.txt`).
  - Files go to `.part`, are checked and renamed (`fetchTo`, `:760-796`). A disc set becomes `psx/.discs/` with an
    `.m3u`, a cartridge game of several files is skipped, and the cover is `path_cover_large`.
  - `gamelist.xml` is merged.
- **Saves**: `Romm::sync()` (`:1166-1269`) lists `/api/saves?platform_id=` per platform and matches by name
  (`matchRom`, `:1129-1155`). It skips slotted saves (`:1187`) and decides by remembered hashes. When both sides
  changed, the newer **by clock** wins (`:1252`), the other kept as `.bak`. Uploads are tagged `retroarch` or
  `bananaos-nds` (`:1280`). There is no `negotiate`.
- **When it runs**: Sync now. After a game, with Auto sync "Leaving a game" or more (`ui/src/main.cpp:3101`, saves
  only). On Wi-Fi connect, with Auto sync "And on Wi-Fi" (`system/rootfs/usr/libexec/banana/wifi:114`). **Nothing
  runs before a game.**
- **Settings** (`ui/src/settings.cpp:888-921`): Server, Sync now (with the fork's Show sync times row,
  `Kiwami::rows`, `:893`), Games, Games go to, Save states, Auto sync, Systems, Games to sync, Choose games (a
  per-system tick list, no search, no details, `:4678-4830`), Check certificate, Forget.
- **No BIOS, no install pushes, no play sessions, no collections.**
- The saves layout: RetroArch's saves are flat in `/data/saves` (`sort_savefiles_enable = "false"`,
  `system/rootfs/etc/banana/retroarch.cfg:45`), and the states flat in `/data/states`. The DS's saves are in
  `saves/nds`, reconciled with DraStic's `.dsv` by `nds-saves`.

**The fork's patches to it**: 0022 (`syncmem.h`: 200 a page, `slim()`, the listing's progress, the peak-memory line)
and 0024 (`synctimes.h`, `sync-times.cpp`: Show sync times). Their hooks are listed in `docs/FORK.md:259-271`. Tests:
`kiwami/tests/romm-list.sh`, `romm-big.py`, `syncmem-units.cpp`, `sync-times.sh`, `sync-times-ui.sh` and
`synctimes-units.cpp`, all run by `kiwami/tests/ci.sh:229-236`.

**Upstream's pending rework** (`cab127ba`, read from its tree; base `87e7e1ca`, upstream's `main` 3 commits past the
local `main` `b02d6ff`, in README and Store files only):

- `banana-sync` split into `sync.cpp`, `sync_core.cpp` and `sync_romm.cpp` (1641 lines), plus `save_adapt`,
  `save_paths`, `save_vault` and `get_list`. On the menu's side, `gamesync.cpp` (the wait before a game) and
  `settings_homesrv.cpp` (the RomM form). It also vendors a QR library in `ui/src/qr/`, beside the fork's own
  `ui/src/kiwami/qrcodegen.cpp`: two copies of Nayuki's generator, harmless.
- **Saves in a slot** (`autosave`, or `homesync_slot_name`), decided from three MD5s (the card's, the slot head's, the
  last agreed), never from clocks. A conflict is held, with both copies kept.
  - Every replaced save goes to `banana/save-vault`, and a journal covers power cuts.
  - Manage saves resolves conflicts on the device.
  - A RomM before 5.4 is still synced by content.
- **The sync before a game**: `banana-sync sync --rom <path> --auto launch`, with a 20 s cap and B to skip
  (`gamesync.h`, `Wait`). It needs Sync before playing (`homesync_launch`, off unless pairing turns it on). It lists
  the game's system first (*Oversights*, 3).
- **Get games**: a per-system list cached on the card. A queues a game (`homesync-get.txt`), Sync now downloads the
  queue, and a game once on the card is never updated or removed by the sync.
- The RomM form: an address, then QR pairing. Certificate trust. A device name (`rename`). The pairing file is vetoed
  from File sharing (`cab127ba:system/rootfs/usr/libexec/banana/share:51`, `*romsync.json*`).
- The rework calls `negotiate` only to learn which ROMs have a save in the slot, and doesn't follow its plan
  (`sync_romm.cpp:755-788`).

**RomM 5.4.1** (`rommapp/romm` tag `5.4.1`, read through GitHub's API):

- Installs: `backend/endpoints/device/install.py`, `handler/device_install/`.
- Firmware: `backend/endpoints/firmware.py`, `GET /api/firmware?platform_id=` and `/{id}/content/{name}`, under
  `firmware.read`.
- ROM lookups: `/api/roms/by-hash` (crc, md5, sha1 or ra), `updated_after`, `search_term`,
  `/api/roms/identifiers`.
- Releases: 5.4.1 is the latest stable (22 Oct). 5.5.0-alpha.2 came out on 15 Nov. Grout's latest is v5.4.1.0.

**Upstream's open requests**: a feature request to download single games from RomM, which the rework answers. A Store
request for "Banana RomM", a Grout fork packaged for Banana by a third party (xkoldxx/banana-romm, v0.1.0). It's an app
outside the menu, a second RomM device on the same handheld, and its saves would clash with `banana-sync`'s. The fork
doesn't use it.

## What has to change

The step-by-step detail is for the plan. In order:

1. **The rework taken in.** The user merges `cab127ba` into `k/0039-romm-home-server`. The user fetches it from the
   author's fork, since the investigation fetches nothing. Then:
   - The conflicts of *Oversights*, 4 are resolved: the enums, includes and Makefile get both sides.
   - Settings' `Kiwami::rows(*this, HS)` is placed in the rework's RomM rows.
   - `kiwami/tests/*` are fixed where they assert today's RomM mirroring.
   - `docs/FORK.md` records the pinned head (*Oversights*, 10 and 11).

   `make test`, the rework's own tests and `kiwami/tests/ci.sh` must pass.
2. **0022 re-ported**: the `syncmem` hooks in the rework's `Romm::list()` (`sync_romm.cpp:103-158`): `PAGE` a page,
   `slim()` keeping what the rework reads (`files[]` with their hashes and `archive_members`, the name tags `romTitle()`
   reads...), the listing's progress, and the peak line. `romm-list.sh`'s key guard catches a field `slim()` drops.
3. **0024 re-ported**: `synctimes::seen()` in `status()`, `tick()` in the steps without a status of their own (the
   rework's), `start()` and `report()` in its `doSync`. `sync-times.sh` gets the new phase lists.
4. **Lighter lookups**: `ui/src/kiwami/romm/lookup.{h,cpp}`, with a hook at the start of `Romm::list()` for a scoped
   run (one game) and one in Get games' refresh. The id and hash cache lives in `/data/banana/kiwami-romm.json`, the
   fork's own file (*decided*). It holds no secrets, so File sharing still shows it, unlike the rework's
   `romsync.json`.
5. **Installs from RomM's page**: `ui/src/kiwami/romm/installs.{h,cpp}`.
   - A `banana-sync` verb sets the capability when the setting changes.
   - At Sync now (hook before the queue is read) and on Wi-Fi (hook in the `--auto wifi` path), it claims, adds the
     games to `homesync-get.txt` and reports after.
   - The setting is `kiwami_romm_installs = off`, a row on the Home server page (RomM only, 5.4 and newer).
6. **BIOS downloads**: `ui/src/kiwami/romm/bios.{h,cpp}` (a `banana-sync bios [--list]` verb), and a BIOS page from a
   Home server row (`Kiwami::rows`, `ui/src/kiwami/`). Plus the scope hook in `sync.cpp`'s pairing.
7. **The Quick resume guard**: a hook where the sync before a game installs a pulled save (the rework's install
   through the vault) calls `kiwami::romm::resumeGuard(rel)`.
8. **Docs**: `docs/FORK.md` (the rework pinned, every new hook), and `kiwami/DEPENDENCIES.md` (RomM's API: 5.4.1 is the
   version written against; the rework, pinned at `cab127ba`, as upstream code carried early).
9. **The final check**, including the `with_rom_id_index` measurement (*Suggestions*, 9).

Settings, all in `settings.conf`, with upstream's (the rework's) behaviour as the default: `kiwami_romm_installs = off`.
BIOS and lookups have no setting: BIOS is an action, and the lookups change only requests (as 0022 did).

No new external dependency: libcurl, OpenSSL and the JSON reader are already used by `banana-sync`.

## Options

1. **Take the rework in, add the gaps on top (chosen).** The fork gets upstream's direction now, merges cleanly once
   upstream merges the same commits, and spends its own work only on what nobody else is building. Costs:
   - 19,000 lines of code that upstream hasn't reviewed yet, in upstream's files;
   - re-porting 0022 and 0024;
   - the risk in *Oversights*, 11.
2. **Build on today's `main`.** It works now, but would rebuild the rework's sync before a game, its slots and Get
   games, and conflict across `sync.cpp` and Settings when upstream merges it. Rejected.
3. **Gaps only, waiting for upstream to merge the rework.** No risk of carrying unmerged code, but nothing is usable
   until upstream acts, and the fork's additions can't be tested on the code they'll sit in. Rejected by the user.
4. **Grout, or the "Banana RomM" Grout fork, from the Store.** A second RomM device on the handheld beside
   `banana-sync`, with its own paths and no sync before a game in the menu. Rejected (as in 0008).

## Risks

- **Performance and footprint.**
  - Nothing new runs while idle: installs are claimed only at Sync now and Wi-Fi connect, and BIOS only on request.
  - The menu's frames don't change: the rework's wait screen is drawn by its own code.
  - The sync before a game goes from listing a whole system to one or two small requests (*Suggestions*, 1). 0022's
    memory fix keeps a full list under 100 MB.
  - The rework adds its own weight: `banana-sync` grows by several thousand lines (hundreds of KB), and a vault on the
    card that grows unpruned until follow-up C.
- **Conflicts with upstream.**
  - The merge conflicts are listed in *Oversights*, 4.
  - The rework touches many upstream files, which `kiwami` will carry ahead of `main` (*Oversights*, 10 and 11).
  - The fork's new hooks sit in the rework's `sync.cpp`, `sync_romm.cpp` and `settings_homesrv.cpp`, files that are
    upstream's from the day it's merged.
  - `sync.cpp` changed in 18 upstream commits since it was added (26 Oct), the most recent on 6 Nov.
- **RomM's API.** Written against 5.4.1. The install API is new in 5.4. RomM 5.5 (alpha) adds snapshot sync, which
  Argosy already follows. A RomM before 5.4 gets no installs: the row hides itself (the rework reads the server's
  version at each sync).
- **Saves.** The rework's slot logic, vault and journal are its author's, tested by its own suites (66 QEMU cases, an
  interop test with Grout's real sync code). The fork adds the Quick resume guard, which only renames.
- **Hardware and boards.** Nothing board-specific. "Games go to" the second card exists only on the RG DS Plus, the
  only device at hand. Pages are drawn at 640x480 and 1024x768.
- **BIOS legality** is the user's business: the files come from the user's own server.
- **The rework's own known limits come along**:
  - the vault is never pruned;
  - one game's sync lists its system (fixed here);
  - an empty slot is asked about outside the slots at every save sync;
  - its translations are machine drafts.

## Testing

- **Headless, `kiwami/tests/`**, against the rework's `ui/tests/fake-romm.py`, extended (or a fork stand-in beside it)
  with installs (claim, report, capability, expiry), firmware, `by-hash`, `search_term`, `updated_after` and
  `identifiers`:
  - **Lookups**: one game's sync makes no `/api/roms?platform_ids` listing when its id is remembered, and one search
    when it isn't. Sync now finds a renamed ROM by hash, and doesn't hash it twice.
  - **Installs**: the capability set and cleared with the setting. A claim during Sync now and on `--auto wifi` with
    Auto sync off. The game downloaded through Get games' queue, and the request reported `done`,
    `already_installed`, or `failed` with its reason. A request for the second card while it's out stays `taken`.
  - **BIOS**: the missing files listed, downloaded and checked; the same hash skipped; a different file not
    overwritten; DraStic's copy; a 403 telling the user to pair again.
  - **The Quick resume guard**: `.state.auto` renamed only when the pull replaced the save.
  - **0022 and 0024** re-ported: `romm-list.sh` and `sync-times.sh` rewritten against the rework, with their unit
    tests.
  - **banana-ui headless**: the BIOS page and the installs row at both sizes, with the log lines checked.
- **The rework's own suites** stay green: `make test` (`ui/tests/run.sh`), `system/tests/launchers-test.sh`,
  `sleep-test.sh`, `tools/qemu-sync.sh`.
- **On the device** (RG DS Plus, the user's RomM 5.4.1):
  - Pair again, for the new scopes.
  - A game sent from RomM's web page on a phone arrives after Wi-Fi connects, and RomM shows it done.
  - BIOS for the DS and the PS1.
  - A GBA save made in Grout or Argosy appears before the game starts on Banana, with Quick resume on.
  - A game's start time with 1000+ games on its system, with and without the lookups.
  - Sync now's peak memory.
  - The `with_rom_id_index` timing.

  Logs: `/data/banana/logs/sync.log` (every request with the fork's lines, the peak memory, the times), banana-ui's
  log, and `launch.status` / `gameend.status`, fetched over SSH/SFTP.

## Open questions

1. **How does the fork relate to upstream's pending rework?** *Answered*: take it in, at a pinned head.
2. **What does the investigation produce?** *Answered*: a roadmap and a first feature, cut as foundation plus two
   follow-ups.
3. **Which clients must saves work with?** *Answered*: Argosy, Grout, RomM's web player.
4. **Issue #8?** *Answered*: superseded by this issue; the user closes it.
5. **Which gaps?** *Answered*: all eight offered. Installs, BIOS and lighter lookups go here. Search and details,
   collections and filters, and the version picker go to follow-up B. Play time and vault pruning go to follow-up C.
6. **0022 and 0024?** *Answered*: both re-ported.
7. **Which head?** *Answered*: `cab127ba`, updated by choice.
8. **Where does the code go?** *Answered*: inside `banana-sync`, through `kiwami:` hooks.
9. **When are installs collected?** *Answered*: Sync now and Wi-Fi connect.
10. **How are versions shown?** *Answered*: one entry per game, picked on A (follow-up B).
11. **The `firmware.read` scope?** *Answered*: hooked into the pairing, and the user pairs again.
12. **The lookup for a game never synced?** *Answered*: a name search before the game, the hash in Sync now.
13. **Where does BIOS go?** *Answered*: `/data/bios` plus DraStic's copy.
14. **The Quick resume guard?** *Answered*: in this issue.
15. **`with_rom_id_index`?** *Answered*: measured on the user's server, then decided.
16. **DraStic's BIOS folder and file names.** *Unanswered*: a fact to check in the plan, not a choice.
17. **The proposals not asked about at first**: accepting the rework's behaviour changes (*Oversights*, 10), what
    to do if the rework changes upstream (*Oversights*, 11), how installs are handled (*Oversights*, 5 and 6), and
    the lookups' cache file (*What has to change*, 4). *Answered*: all four accepted as proposed.
