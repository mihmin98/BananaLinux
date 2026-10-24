# 0008 RomM: investigation

Issue: #8 · Branch: `kiwami` (committed there at the user's request; the implementation gets `k/0008-romm`) · Status: refined (rechecked 24 Oct 2026 after 0007, and again after 0019, *Rechecked*; approved for planning the same day) · Confidence: 91% (proceed)

## The request

Banana gets a RomM client: games downloaded from the user's RomM server over Wi-Fi, and in-game saves kept in sync
with it, so a game can be carried on from another device (or RomM's own web player) and back. The request asked
whether Grout (RomM's own handheld client) would do, given that its paths may not match Banana's and that it may not
know the second card, or whether RomM support should be built into the system.

Decided with the user:

- **A client built into Banana** (the fork's own), not Grout (*Options*, 1 against 2 and 3). Its helper program is
  **C++**, like upstream's `banana-scrape`.
- **Saves sync both ways**: automatically around every game (the server's newer save pulled before the game
  starts, the save pushed after it quits) *and* with a **Sync now** for everything (after playing offline, say).
- **The automatic sync hooks in through `systems.conf`**: `kiwami/post-build.sh` wraps each game system's `run`
  line in the image's copy, so no upstream file changes.
- **A conflict in the automatic sync** (the card's save and the server's both changed since the last sync): the
  card's save is kept, nothing is overwritten, and the conflict is listed in Settings to resolve by hand.
- **Saves covered**: the RetroArch systems' (kept per core, `/data/saves/<core>/`) and the DS's (DraStic's and
  BananaDS's, reconciled as `nds-saves` does).
- **Downloaded games** go to the main card or the second card, set once in a setting, not asked on each download;
  set to the second card while it isn't in, a download goes to the main card and says so.
- **BIOS files** from RomM's firmware go to `/data/bios`.
- **The server**: RomM 5.4.1, reached over plain http on the LAN or over Tailscale.
- **Also in**: the Quick resume guard (*Suggestions*, 1), the push after a game in the background (2), each play's
  time sent to RomM (3).
- **One feature, saves first**: the plan's steps do the login and the save sync first (usable on their own), then the
  game browser, the downloads and the BIOS.
- **RomM is a group of its own in Settings** (decided 24 Oct, after 0019 gave RetroAchievements one), not a row under
  Network: *What has to change*.

Out of scope: save states (RetroArch's `.state`, the DS emulators' `.dss`/`.mst`), box art from RomM (Banana's own
scraper covers it), PICO-8's and the Ports' and apps' own data, uploading games to RomM, RomM's collections and
"install to device" pushes from the web UI.

### Rechecked (24 Oct 2026)

The report was checked again against what changed after it was written (23 Oct): 0007's RetroArch master
(`k/0007-retroarch-master`, about to be merged into `kiwami`), upstream's `main` on GitHub (31 commits past the
local `main`'s `a5e65b7`), and RomM's and Grout's releases. **It still holds**: every decision stands, and the
`path:line` references were read again on `k/0007-retroarch-master`. What changed:

- **RetroArch master's Cloud Sync now pulls before the game loads** (`retroarch.c:9566-9577` at `71f521e`): the
  reason *Suggestions*, 6 gave for leaving it out no longer holds on master. Other reasons still do, and the user
  confirmed the built-in client and no RetroArch WebDAV (*Suggestions*, 6, *Options*, 4, open question 13).
- **RetroArch master keeps what the client relies on**: saves per core (`DEFAULT_SORT_SAVEFILES_ENABLE` true,
  `config.def.h:1117` at `71f521e`, `:917` on v1.22.2), the automatic state still `<game>.state.auto`
  (`command.c:5826`), which the Quick resume guard renames. The guard has to work under both builds, since
  `BR2_PACKAGE_KIWAMI_RETROARCH_MASTER` can be turned off.
- **Upstream added a JSON reader of its own** (`ui/src/json.{h,cpp}`, `banana::parseJson`, for its layout themes:
  comments and trailing commas allowed), not merged into the fork yet. The helper keeps the fork's
  `ui/src/kiwami/json.h` (open question 14). The two don't clash: different namespaces, and `#include "json.h"` from
  `ui/src/kiwami/` finds the fork's first.
- **Upstream's other changes don't touch RomM's ground.** Layout themes (`ui/src/layout.cpp`, `theme.cpp`, a few lines in
  `main.cpp`'s menu set-up and CLI), the theme studios under `tools/`, the website and promo videos. Nothing in
  `runGame`, `systems.conf`, the `run-*` launchers, `nds-saves`, `share`, `wifi`, `sleep.d/` or Settings' pages. The
  RomM page is drawn by Settings, whatever the menu's theme. Their `ui/Makefile` `SRC` line changed next to the
  fork's hook (a conflict for the next upstream merge, not for RomM: the helper isn't built by `ui/Makefile`).
- **`kiwami/post-build.sh` now has a `veto` helper** (0007) for file sharing's veto line; the wrapping of `run` lines
  goes in the same file. RomM's token stays in `accounts.conf`, already vetoed (`share:50`).
- **RomM 5.4.1 and Grout v5.4.0.0 are still the latest releases.**

### Rechecked after RetroAchievements (24 Oct 2026)

Checked again after 0019's RetroAchievements was merged into `kiwami` (`5398736`, 18 commits past `b59fab5`, where the
recheck above was committed). Upstream's `main` on GitHub hasn't moved (still 31 commits past `a5e65b7`, the last on
23 Oct), and RomM 5.4.1 and Grout v5.4.0.0 are still the latest releases. **The report still holds**: no decision is
undone, and the `path:line` references below were read again on `kiwami` at `5398736`. What changed:

- **The fork's Settings pages are routed by which one is open** (`a3e6f63`): `Kiwami::open()` sets `pageOpen` from the
  row's key and `press()`, `touch()`, `render()`, `entryHelp()` and `entered()` call that page's own functions
  (`ui/src/kiwami/kiwami.cpp:141-178`, the `Page` enum in `kiwami.h`). RomM's pages are more entries in it.
- **A group of the fork's own is possible, and RomM gets one** (open question 15). RetroAchievements is a seventh
  group, after System (`ui/src/settings.cpp:507`, a `kiwami:` hook line), and the groups' grid shows two rows of
  three and scrolls to a third (`kiwami/grid.h`, the `kiwami:` lines in `settings.cpp`'s `pressGroups`, touch and
  drawing). Settings makes a group of each item's `section` in the order they first appear (`settings.cpp:683`), so
  `Kiwami::rows()` can add RomM's rows right after RetroAchievements' from the same hook call
  (`ui/src/kiwami/kiwami.cpp:90`): an eighth group and still no new upstream line. Eight groups fill three rows but
  one; `gridMove` already handles a last row that isn't full.
- **The fork has an HTTP client in banana-ui** (`ui/src/kiwami/raclient.{h,cpp}`): libcurl `dlopen()`ed as upstream's
  account check does, each request blocking (up to 20 s) on a thread the page starts (`Kiwami::raRun`), one log line
  per request without secrets. The helper is still a separate program (the wrapper needs it outside banana-ui); its
  transport follows raclient's (the same timeouts and log line), and banana-ui's RomM pages ask the helper, as decided.
- **An MD5 in the tree**: rcheevos' `src/rhash/md5.c` is vendored and built (`ui/src/kiwami/kiwami.mk`). RomM's
  negotiation sends each save's MD5 (`content_hash`); the helper can link that file rather than OpenSSL's libcrypto
  (*Suggestions*, 8).
- **accounts.conf has another writer of the fork's.** RetroAchievements' account store edits it in place, keeping
  every other line, through `<file>.new` and a rename (`ui/src/kiwami/rastore.cpp:50-98`), and upstream's
  `ra-secrets` edits it with `sed -i` before and after each RetroArch game (`run-retroarch:94,176`,
  `ra-secrets:11,23-24`). RomM's keys (`romm_url`, `romm_token`, `romm_device_id`) sit beside theirs: the helper
  writes them only when pairing and logging out, the same way as rastore's `edit`, and only reads the file around
  games, so the push after a game never races `ra-secrets`.
- **The library is reachable from Settings** (`kiwami::library`, `ui/src/kiwami/library.h`, set in `ui/src/main.cpp:1010`):
  the browser can mark a game as on the card from the library's games (both cards) instead of looking at the folders.
- **The tests have a model for a stand-in server**: `kiwami/tests/ra-server.py`, with its replies as files under
  `kiwami/tests/ra/`, and the URLs moved by the environment (`KIWAMI_RA_URL`). RomM's fake server follows it. The
  group changes `ra-ui.sh`'s grid shots (eight groups instead of seven): RomM's group gets a `KIWAMI_ROMM_GROUP=0` for
  the tests, as `KIWAMI_RA_GROUP=0` leaves RetroAchievements' out.
- **Corrected**: *Suggestions*, 7 said `banana-scrape` scans only `/data/roms`; it scans the second card's `roms` too
  (`ui/src/scrape.cpp:501-512`) and puts the art on that card (`:441-455`), already so at `b59fab5`. A game downloaded
  to the second card gets its cover from Banana's scraper like any other.
- Line drift only: `ui/src/main.cpp` moved by two lines (`runGame` `:1363-1499`, the game-panel test `:1383`),
  upstream's libcurl check is `ui/src/settings.cpp:2877`, the fork's cores `ui/src/kiwami/kiwami.cpp:53-63`.

## Oversights in the requirements

1. **Grout's paths aren't Banana's, beyond the ROMs.** Grout's ROCKNIX profile (`cfw/rocknix/rocknix.go`, Grout
   `15b02e2`, v5.4.0.0) takes ROMs from `$BASE_PATH/roms/<folder>` (`/storage` without `BASE_PATH`; Banana has no
   `/storage`, only `/roms -> /data/roms`, `buildroot/board/rgds/post-build.sh:19`), BIOS from `roms/bios`, and saves
   *beside the ROMs* (`savesBesideRoms: true`, `cfw/firmware.go:435-450`). Banana's folder names match ROCKNIX's
   (`gba`, `nds`, `megadrive`, `psx`...), so ROM downloads would land right with `BASE_PATH=/data`. Nothing else
   does: BIOS files are in `/data/bios`, and the saves are elsewhere (2). It has one ROM root (no second card) and no
   way to sync without its UI (its only `package main` is the app; no CLI), so "around every game" can't be done
   with it. *Decided*: the built-in client.

2. **RetroArch's saves are per core, not per system.** `retroarch.cfg` sets `savefile_directory = "/data/saves"`
   (`system/rootfs/etc/banana/retroarch.cfg:36`) and leaves `sort_savefiles_enable` at RetroArch's default, true
   (`config.def.h:917` on v1.22.2, `:1117` on master's `71f521e`), so a save is
   `/data/saves/<core's library_name>/<rom stem>.srm` (`runloop.c:8240-8260`, v1.22.2):
   `gpSP`, `mGBA`, `Gambatte`, `SameBoy`, `FCEUmm`, `Mesen`, `Snes9x`, `PicoDrive`, `Genesis Plus GX`,
   `Mupen64Plus-Next`, `PCSX-ReARMed`. `docs/EMULATORS.md`'s "Saves and states | `/data/saves/<system>`" is wrong for
   RetroArch (true only for BananaDS's `saves/nds`). Switching a system's core (`core.<system>`, Settings > Other
   systems) switches the save folder, so the client has to work out the folder from the core that will run, as
   `run-retroarch:14-19` does. *Decided*: the client syncs the save of the core chosen now; a save left in another
   core's folder is not synced (one RomM save per game, not one per core).

3. **The DS has two emulators and one save.** DraStic keeps `/data/drastic/backup/<game>.dsv`, BananaDS
   `/data/saves/nds/<game>.sav`; `nds-saves` makes them agree, but only *before* a game
   (`system/rootfs/usr/libexec/banana/nds-saves`, called by `run-drastic` and `run-bananads:120`). After a DraStic
   game the `.sav` is stale until the next launch. *Proposed*: the client runs `nds-saves` after a DS game and before
   any DS push (Sync now included), syncs the raw `.sav` (BananaDS's form, what other emulators read too), and writes
   a pulled save as `saves/nds/<game>.sav`; `nds-saves` carries it to DraStic's `.dsv` at the next start, keeping
   its exact size.

4. **A game already on the card has no RomM id.** RomM pairs saves by (`rom_id`, `slot`) (RomM's device sync
   protocol; `POST /api/sync/negotiate`). A game downloaded by the client can record its id; one copied over file
   sharing can't. *Proposed*: as Grout does, match by platform and file name (exactly, then without case) against
   the platform's ROM list, cached on the card and fetched again on a miss (at most once per Sync now); a game RomM
   doesn't have is skipped and listed.

5. **A card with its own `systems.conf` gets no automatic sync.** `/data/banana/systems.conf` replaces the image's
   file whole (`system/rootfs/etc/banana/systems.conf:2`), and the wrapper is in the image's copy. *Proposed*: accept
   it, as 0003's `.7z` did; Sync now still covers that card, and the RomM group says why the automatic sync is off when
   the card's `systems.conf` has no wrapper.

6. **No Wi-Fi at launch time.** After a wake, Wi-Fi comes back in the background (`sleep.d/10-wifi`, `post`), so a
   game started right after opening the lid finds no network for a few seconds. *Proposed*: the pull gives up quietly
   after a short timeout (connect 1.5 s, the whole pull 3 s), logs it, and the game starts with the card's save; the
   push afterwards goes as usual. Nothing is ever lost, only not pulled.

7. **Sleep in the middle of a transfer.** The lid or the power button suspends whatever runs (bananad), and
   `sleep.d/10-wifi` unloads the Wi-Fi driver; a download or a push breaks. The 10-minute idle sleep would also stop
   a long download (a 512 MB DS game over slow Wi-Fi). bananad already holds the idle sleep off while
   `/run/banana/awake.pid` names a live process (`system/bananad/bananad.c:1241-1251`, made for `banana-scrape`).
   *Proposed*: downloads take `awake.pid` while they run (only when it is free: one pid file, shared with
   `banana-scrape`, so neither overwrites the other's) and resume with an HTTP range after a break; a push that fails
   stays pending for the next sync.

8. **Quick resume hides a pulled save.** With Quick resume on, RetroArch loads the automatic state after the save
   file (`savestate_auto_load`, `run-retroarch`), so a newer save pulled from RomM would be ignored. BananaDS already
   drops its resume state when the `.sav` is newer (`run-bananads:122`); DraStic and RetroArch don't. *Decided*: the
   Quick resume guard (*Suggestions*, 1).

9. **Shutdown while playing.** A long power press sends `SIGTERM` to the game's process group and waits 5 s
   (`docs/EMULATORS.md`, *Shutdown while playing*); the wrapper is in that group. *Proposed*: the wrapper passes the
   `TERM` on, waits for the game as `run-retroarch` does, and does not push then: the save is marked pending and goes
   with the next sync.

10. **What the server says to delete.** RomM's negotiation can answer `delete` for a slot emptied on the server.
    *Decided*: the client never deletes a save on the card; it renames it to `.bak` and logs it (open question 2).

11. **Plain http.** Over the LAN, the token travels in the clear (anyone on that network could reuse it); over
    Tailscale it's encrypted. *Proposed*: accept http (the user's choice), say so in the server row's help, and take
    https too when the URL says so (libcurl verifies with the image's CA bundle).

12. **Tailscale's names don't resolve.** The fork's Tailscale runs with `--accept-dns=false`
    (`kiwami/rootfs/usr/libexec/kiwami/tailscale:124`), so a MagicDNS name (`romm.tailnet.ts.net`) doesn't resolve
    on the handheld. *Proposed*: the server row's help says to use the Tailscale address (`100.x.y.z`), which works at
    home as well, so one URL serves both cases.

13. **The second card's games' saves are on the main card.** RetroArch and the DS emulators save on `/data` wherever
    the game is (`/run/sd2/roms`, `system/rootfs/usr/libexec/banana/sd2`), so syncing reads and writes only `/data`.
    A game on both cards is one save. *Proposed*: nothing to do; the browser marks a game as on the card when either
    card has it.

## Suggestions

1. **The Quick resume guard** (*decided*). When a pull replaces a game's save, its automatic state is renamed to
   `.bak` (RetroArch's `<game>.state.auto` in the core's states folder, the same name on v1.22.2 and master,
   `command.c:5826`; DraStic's resume state), so the game starts from the save that was pulled. A few lines in the
   wrapper.
2. **The push in the background** (*decided*). After a game the menu comes back at once; the push runs detached (its
   own session, outside the game's process group), and one that fails is pending for the next sync (the next game's
   push, or Sync now). No waiting on a dark screen.
3. **Play time to RomM** (*decided*). Each play's start and end goes to RomM as a play session (RomM's
   `POST /api/play-sessions`, or batched with the sync session's `complete`), so RomM's own stats include the handheld.
   One small request with the push; pending ones are kept on the card like pending pushes.
4. **The second card's fallback** (*decided*): see *The request*.
5. **Speak RomM's names for cores.** Sending the libretro core id as a save's `emulator` (`gpsp`, `mgba`, `snes9x`,
   `genesis_plus_gx`...), as RomM's own RetroArch endpoint translates core folders to, lets RomM's web player see the
   saves. Pairing doesn't depend on it (it is by `rom_id` and slot), so it costs nothing. *Proposed*: yes, with the
   slot `autosave` (Grout's default, so saves made by Grout on another handheld pair with Banana's).
6. **Leave out**: RetroArch's own Cloud Sync (*decided*, again on 24 Oct). Banana's RetroArch is built with it
   (`HAVE_CLOUDSYNC`, WebDAV), and RomM 5.4.0 added a WebDAV endpoint for it (`/api/sync/retroarch`). On v1.22.2,
   RetroArch loads the game from the command line (`retroarch.c:~6030`) *before* it starts its first sync (`:6053`),
   so a pulled save arrives after the game has read the old one. Master (0007's `71f521e`) fixed that: a game from
   the command line waits for the sync (`retroarch.c:9566-9577`), and `retroarch --cloudsync` syncs and exits
   (`:8988-9020`). It still doesn't fit:
   - RomM's WebDAV takes the user's password (Basic auth, `backend/handler/auth/hybrid_auth.py:83-99` in RomM 5.4.1),
     not a device token that can be revoked. Master keeps it in its keychain (`webdav_password`), which the fork's
     `ra-keychain` folds back into `retroarch.cfg`, and file sharing shows that file.
   - RomM registers one "RetroArch" device per user, whatever the install (`backend/handler/sync/retroarch/device.py`),
     so the handheld and a PC's RetroArch would be the same device, and their conflicts would go unseen.
   - Nothing is pushed when RetroArch quits (`retroarch.c:10035`: no sync once shutdown has begun), only at the
     next RetroArch start, unless something runs `retroarch --cloudsync` after it.
   - It covers RetroArch only, not the DS, and only while `BR2_PACKAGE_KIWAMI_RETROARCH_MASTER` is on. Conflicts are
     resolved in RetroArch's menu, not Banana's Settings.
7. **Not now, worth an issue later**: save states; RomM's "install to device" pushes from the web UI. (Covers for
   downloaded games need nothing: `banana-scrape` covers both cards, `ui/src/scrape.cpp:501-512`.)
8. **The save's MD5 from rcheevos' `md5.c`** (*proposed*, 24 Oct): already vendored and built for RetroAchievements
   (`ui/src/kiwami/third_party/rcheevos/src/rhash/md5.c`), so the helper needs no libcrypto for it. For the plan to
   settle, with how the helper's build picks up that file.

## Current state

**Nothing of RomM in the fork or upstream** (`git grep -i romm` finds nothing; `main` neither).

**Games and their launch.** `systems.conf` (`system/rootfs/etc/banana/systems.conf`) lists the systems; games are
found in `/data/roms/<folder>` and the second card's `roms` (`ui/src/library.cpp:486-493`, `Library::roots()`, the
second root from `/run/banana/sd2/roms`). banana-ui's `runGame` (`ui/src/main.cpp:1363-1499`) replaces `%ROM%`
and `%SYSTEM%` in the system's `run` (`:1367`), decides from that string whether it is RetroArch, which gets the game
panel (`:1383`, `find("run-retroarch")`, `run-ps1`, `run-pico8`), closes the display (Mode B) or leases one panel
(Mode A), forks, `setsid()`, exports `BANANA_ROM` and `BANANA_SYSTEM` (`:1435-1436`), runs `sh -c <run>` and writes
its pid to `/run/banana/game.pid` (`:1449`). After the game: `banana-perf ui`, the panels back, `lib.recordPlay`
(`:1489`) with the seconds played. The library picks up games copied into ROM folders through inotify, read again
once the copying has been quiet for 2 s (`ui/src/main.cpp:341-375`; the second card's folders are watched too).
banana-ui's headless test prints a launch line instead of running the command (`kiwami/tests/7z-ui.sh:30-36`).

**Saves.**
- RetroArch: `/data/saves/<library_name>/<rom stem>.srm` (*Oversights*, 2), the stem of the `.zip`/`.7z` for an
  archive. The core: `systems.conf`'s, or `core.<system>` in `settings.conf` when it names an installed core
  (`system/rootfs/usr/libexec/banana/run-retroarch:14-19`); the fork adds Mesen, SameBoy and Genesis Plus GX
  (`ui/src/kiwami/kiwami.cpp:53-63`, `kiwami/DEPENDENCIES.md`). PlayStation: `run-ps1` ends in `run-retroarch` with
  PCSX-ReARMed (memory card per game, `<stem>.srm`).
- DS: `run-nds` picks DraStic or BananaDS (`nds_emulator`, `nds-per-game.conf`); `nds-saves` reconciles
  `drastic/backup/<g>.dsv` and `saves/nds/<g>.sav` before each start (*Oversights*, 3), takes a ROCKNIX-style `.dsv`
  beside the game once, keeps the replaced file as `.bak`.
- States: `/data/states/<core>/`, `/data/states/nds`; Quick resume is `savestate_auto_save`/`_load`.

**Network and accounts.** Wi-Fi stays up while a game runs (nothing in `wifi` stops it); it goes down for a sleep and
comes back after it in the background (`system/rootfs/etc/banana/sleep.d/10-wifi`). The first-time setup keeps
accounts in `/data/banana/accounts.conf`, which file sharing hides (`system/rootfs/usr/libexec/banana/share:50`,
`veto files = /*host_key_ed25519/*accounts.conf/*ra.token/*wifi.conf/`). libcurl (with OpenSSL and the CA bundle) is
in the image; upstream's code `dlopen`s it (`ui/src/settings.cpp:2877`; busybox's `wget` has no HTTPS), and so does
the fork's RetroAchievements client (`ui/src/kiwami/raclient.cpp:33`). There is no
`curl` command and no `jq`; `unzip` is busybox's (`/usr/bin/unzip`). Python 3.14 is in the image for PortMaster
(17 MB).

**What the fork already has to build on.**
- Settings pages of its own: `Settings::Kiwami` (`ui/src/kiwami/kiwami.h`) adds rows to a group (`rows()`, hooked in
  `ui/src/settings.cpp:458,466`) and a group of its own after System (`:507`, RetroAchievements), a page per row
  (`page == KIWAMI`, routed to the page open by `pageOpen`, `ui/src/kiwami/kiwami.cpp:141-178`), upstream's keyboard
  (`ENTRY_KIWAMI`), a background job polled while the page shows (`wifi::Job`, `update()`/`wake()`), and requests run
  on a thread (`raRun`). Tailscale's page
  (`ui/src/kiwami/tailscale.cpp`) is the model: a helper program, a status, a login by a QR code on the top panel
  (`qrFor()`, `renderLogin()`, Nayuki's QR generator, `ui/src/kiwami/qrcodegen.cpp`).
- A JSON reader (`ui/src/kiwami/json.h`), unit-tested (`kiwami/tests/units.cpp`). Upstream now has its own
  (`ui/src/json.h`, for themes, not merged yet); the helper keeps the fork's (open question 14).
- RetroArch's master (0007), on by default in `kiwami/configs/kiwami.fragment` and off with
  `BR2_PACKAGE_KIWAMI_RETROARCH_MASTER`: the same save and state layout as v1.22.2 (*Rechecked*).
- `kiwami/post-build.sh` already edits the image's `systems.conf` (0003's `7z`), failing the build if a section it
  expects isn't there.
- From 0019's RetroAchievements: an HTTP client in banana-ui (`ui/src/kiwami/raclient.{h,cpp}`), the account kept in
  `accounts.conf` by an in-place edit that keeps the other keys (`ui/src/kiwami/rastore.cpp:50-98`), the library
  reachable from Settings (`ui/src/kiwami/library.h`), rcheevos' `md5.c` built (`ui/src/kiwami/kiwami.mk`), and a
  stand-in server for the tests (`kiwami/tests/ra-server.py`).
- Upstream's `banana-scrape` (`ui/src/scrape.cpp`) is the model for a long network job: a separate program started
  detached from Settings, its progress in a status file (`/run/banana/scrape.status`), `awake.pid` held while it runs.

**RomM 5.4.1** (tag `9c74d01`, 22 Oct 2026; read from its source):
- Login: the device flow (`backend/endpoints/device/auth.py`): `POST /api/auth/device/init` (no credentials) returns
  a `device_code`, a short `user_code` and a verification path; the user opens it on a phone (a QR code) and
  approves; the device polls `POST /api/auth/device/token` until it gets a client token (`rmm_...`) bound to a new
  RomM device (`device_id`). Every call then sends `Authorization: Bearer rmm_...`. Scopes: `roms.read`,
  `platforms.read`, `firmware.read`, `assets.read`/`write`, `devices.read`/`write`, `roms.user.write`.
- Saves: `POST /api/sync/negotiate` with the device's saves (`rom_id`, `file_name`, `slot`, `emulator`,
  `content_hash` MD5, `updated_at`, size) answers an operation each (upload, download, conflict, no-op, delete);
  `POST /api/saves` (multipart) uploads, `GET /api/saves/{id}/content` downloads,
  `POST /api/sync/sessions/{id}/complete` closes the session (with play sessions). The server tracks which device
  has the newest version and flags a conflict when both sides changed.
- Games: `GET /api/platforms`, `GET /api/roms` (filter by platform, search, pages), `GET /api/roms/{id}/content/{name}`
  (a zip for a game of several files), `GET /api/firmware`, `GET /api/firmware/{id}/content/{name}`.
- Platform slugs for Banana's systems (`backend/utils/platform_slugs.py`): `nds`, `gba`, `gbc`, `gb`, `nes`
  (`famicom`), `snes` (`sfam`), `genesis` (Banana's `megadrive`), `psx`, `n64`, `pico`.

**Grout v5.4.0.0** (commit `15b02e2`): Go 1.26 with the gabagool SDL2 UI. The ROCKNIX release would start on Banana
from the Ports shelf (the binary needs glibc 2.17, Banana has 2.41, and SDL2, SDL2_image, SDL2_ttf, all in the image;
it brings SDL2_gfx), but its paths are ROCKNIX's (*Oversights*, 1). It is 48 MB, kept on the card.

## What has to change

All of it is in the fork's own files; no upstream file changes.

- **The helper, `/usr/libexec/kiwami/romm`** (C++, in `ui/src/kiwami/romm/` with the fork's `json.cpp`; built by a
  new package `kiwami/package/kiwami-romm` whose local site is `ui/`, as `banana-ui`'s is, so neither `ui/Makefile`
  nor `banana-ui.mk` changes; libcurl from the image). Verbs, for the wrapper and the Settings page:
  - `pair <url>` / `pair-poll`: the device flow; the token and device id written to `accounts.conf`
    (`romm_url`, `romm_token`, `romm_device_id`), edited in place as rastore's `edit` does (the other keys kept);
  the helper writes the file only here and when logging out.
  - `pull <system> <rom>` / `push <system> <rom> <start> <end>`: one game's save (the core's folder from
    `systems.conf` and `core.<system>`; the DS through `nds-saves`), with the timeouts of *Oversights*, 6; pending
    pushes and play sessions in `/data/banana/romm/pending`.
  - `sync`: every game's save on both cards (Sync now); conflicts written to `/data/banana/romm/conflicts`,
    `resolve <id> local|server` (the other kept as `.bak`).
  - `platforms`, `roms <platform> [search] [page]`: the browser's lists, as lines for banana-ui.
  - `download <rom id>`: to the target card's `roms/<folder>/`, through a temporary name the library doesn't list,
    unpacked when it's a zip of several files, with free space checked first, `awake.pid` held and an HTTP range on a
    retry; `bios`: RomM's firmware for the mapped systems into `/data/bios`, skipping files already there with the
    same hash.
  - Progress and results in `/run/banana/romm.status` (as `banana-scrape`'s), a log in `/data/banana/logs/romm.log`.
  - The ROM-list cache and the ids of downloaded games in `/data/banana/romm/`.
- **The wrapper, `/usr/libexec/kiwami/romm-game <run command...>`** (shell, `kiwami/rootfs/`): with RomM on, paired
  and the automatic sync on, `romm pull`, then the Quick resume guard when the pull replaced the save; the game as a
  child, with `TERM` passed on; then `romm push` detached in the background (none after a `TERM`). With RomM off, it
  `exec`s the command, so the game runs as it does on `main`.
- **`kiwami/post-build.sh`**: each game system's `run` line (`nds gba gbc gb nes snes megadrive psx n64`; not the
  apps, PICO-8 or Ports) prefixed with `/usr/libexec/kiwami/romm-game`, idempotently, and the build stopped if one of
  those sections or its `run` line is missing. The command still contains `run-retroarch`/`run-ps1`, so banana-ui's
  game panel test (`ui/src/main.cpp:1383`) is unchanged.
- **Settings > RomM, a group of its own** (decided 24 Oct; `ui/src/kiwami/`): its rows added by `Kiwami::rows()` after
  RetroAchievements' from the existing hook call (`ui/src/settings.cpp:507`; no new upstream line), so it is the eighth
  group, after RetroAchievements; its pages are entries in the `Page` routing. The rows, roughly (the plan settles them):
  RomM on/off; Server (the keyboard); Pair (the QR code and the code on the top panel, as Tailscale's login) and Log
  out; Sync around games on/off; Downloads to: Main card / Second card; Sync now (progress, then what was sent and
  fetched; its value Synced, *n* conflicts, No server) and the conflicts to resolve; Games (RomM's platforms that map
  to Banana's systems, then their games, paged, a search through the keyboard, A downloads with progress, a game
  already on a card marked from the library); BIOS (download). The group shows whether RomM is on or off, as
  RetroAchievements' does; with RomM off its rows say so and nothing runs.
- **Settings** in `/data/banana/settings.conf`, defaults keeping upstream's behaviour: `romm = off`,
  `romm_autosync = on` (only used once RomM is on), `romm_target = main` (`main` or `sd2`).
- **Docs**: the client in `docs/FORK.md`'s shadowed files (the `run` lines `post-build.sh` wraps, as for `7z`), and
  `docs/EMULATORS.md`'s save paths (*Oversights*, 2) noted for upstream rather than changed here.
- **No new external dependency**: libcurl and OpenSSL are upstream's, already in the image; `kiwami/DEPENDENCIES.md`
  gets RomM's API (5.4.1, the version the client is written against) in its second table and libcurl as upstream's.

## Options

1. **A client built into Banana (recommended, chosen).** The helper, the wrapper and the Settings pages above. It
   covers everything decided: the automatic sync around games, the second card, the save of the core actually used,
   the DS's two emulators, BIOS into `/data/bios`, the menu's look and the touchscreen. Cost: the most work (an HTTP
   client of a few verbs, the pairing, the negotiation, the browser), and the fork follows RomM's API from now on:
   the device sync API is recent (4.9), and Grout's versions track RomM's for that reason. Footprint: one small
   binary (hundreds of KB) and a shell script, nothing running while idle.
2. **Grout with a BananaOS profile.** A firmware entry for Banana in Grout's table (`cfw/firmware.go`): ROMs
   `/data/roms`, BIOS `/data/bios`, saves under `/data` per platform (`saves/gpSP`, `saves/mGBA`... and
   `drastic/backup`, `saves/nds`), offered upstream to Grout or carried by the fork as a patch (Go 1.26, Buildroot's
   prebuilt 1.26.3 builds it; SDL2_gfx would have to be added to the image). Less of our code, and Grout's other
   features. But: Sync now only (no headless mode), one save folder per platform that doesn't follow
   `core.<system>`, one ROM root (no second card), the DS's `.sav` stale after DraStic, a 48 MB app, and its timing
   in Grout's maintainers' hands.
3. **Grout for the games, our helper for the sync around games.** Two clients would be two RomM devices for one
   handheld, each seeing the other's saves as another device's: conflicts by design. Rejected.
4. **RetroArch's Cloud Sync against RomM's WebDAV endpoint.** No code for the RetroArch systems. Master's sync
   before the game fixes v1.22.2's order, but it takes the RomM password, shows up as one RetroArch device for all
   the user's installs, pushes nothing at quit and covers neither the DS nor a v1.22.2 build (*Suggestions*, 6).
   Rejected (again on 24 Oct, by the user).

Grout as it is, from the Ports shelf with `BASE_PATH=/data` in its `Grout.sh`, would already download games into the
right folders (not BIOS or saves). That's a stopgap the user can try on the card before the feature exists; nothing
of the fork's is needed for it.

## Risks

- **Performance and footprint.** Nothing runs while idle: the helper runs for a pull, a push, a sync or a download,
  and Settings polls its status only while a RomM page shows (every 0.5 s, as banana-scrape's). The menu's frames
  don't change. A game's start gets one request (the negotiation) and, when the server has a newer save, one download:
  tens of milliseconds on the LAN, up to the 3 s cap without a network. With RomM off the wrapper is one `exec`. Rootfs:
  the helper and the script, well under 1 MB. The push after a game runs while the menu is back (Wi-Fi and one core
  for a moment).
- **Conflicts with upstream.** No upstream file changes. If upstream renames a system or reshapes a `run` line,
  `post-build.sh` stops the build. If upstream adds its own wrapper in `run` lines (a hook after a game ends, which
  `docs/FORK.md` would like upstream to have), the fork's wrapper moves to it. A new system upstream adds isn't
  synced until it's added to the list (the build doesn't notice). Upstream changes where saves go
  (`retroarch.cfg`'s `savefile_directory`, `sort_savefiles_enable`, `nds-saves`), and the client follows: those
  files go in `docs/FORK.md`'s shadowed list.
- **RomM's API moving.** Written against 5.4.1. A server upgrade that changes the sync API breaks the client until it's
  updated; the client checks the server's version (`GET /api/heartbeat`) and says so on the page instead of syncing
  wrongly.
- **Saves going wrong** is the worst case: a save overwritten by an older one. Mitigations: RomM's hash-based
  negotiation; a conflict never overwrites in the automatic sync; every save replaced by a pull is kept as `.bak`
  first; nothing deleted (*Oversights*, 10).
- **Cores and formats.** A save from another device's emulator may not suit the core used here (a different RTC
  footer, a DraStic `.dsv` with DeSmuME's footer). `nds-saves` already reads DeSmuME's footer; the RetroArch cores
  read raw `.srm`. Mostly fine, worth checking per system on the device.
- **Hardware and boards.** Nothing board-specific: the same files on `rgds`, `rgds-plus` and `universal`; the pages
  drawn at 640x480 and 1024x768 like the rest of Settings. Only the RG DS Plus is at hand for the device tests, and
  only it has the second card's slot used here.
- **Clock.** RomM compares hashes; `updated_at` comes from the card's file times, which follow the handheld's clock.
  A handheld with a wrong clock (no network since a battery drain) only affects what RomM shows.

## Testing

- **Headless, `kiwami/tests/`.**
  - A fake RomM server (a small Python HTTP server on the host, run by the test): the device flow, the negotiation
    answering each operation, uploads, downloads, the ROM list, firmware, an http error, a stall (for the timeouts).
    The helper built for the host against it: pairing, pull and push of a RetroArch save in the right core's folder
    (with `core.<system>` set and unset), the DS's `.sav` after `nds-saves`, a conflict kept and listed and then
    resolved both ways (with the `.bak`), a delete turned into a `.bak`, pending pushes and play sessions sent on the
    next sync, a download to each card, the second card missing (the fallback), a multi-file zip, a resumed download,
    BIOS skipped when the same, file names with spaces and odd case.
  - The wrapper with stand-ins (a fake helper and a fake game): RomM off is an `exec`; the order pull, guard, game,
    push; `TERM` passed on with no push; the guard's `.bak`.
  - `post-build.sh` over upstream's `systems.conf`: the nine `run` lines wrapped once (idempotent), the apps left
    alone, a missing section failing the build; banana-ui headless over that file, every shelf launching (as
    `7z-ui.sh`).
  - banana-ui headless: the RomM group and its pages with the fake helper (as `tailscale-ui.sh` does with
    `fake-tailscale.sh`), at 640x480 and 1024x768, the log lines checked; the groups' grid with eight groups, and
    `ra-ui.sh`'s shots kept as they are with `KIWAMI_ROMM_GROUP=0`.
  - `kiwami/tests/ci.sh` runs all of it; `make test`'s renders unchanged with RomM off (upstream's look).
- **QEMU** (`tools/qemu-boot.sh`): the image boots, the wrapped `run` lines start a game's launcher, the helper
  runs on the target; it has no Wi-Fi, so the network is QEMU's user network to a fake server on the host.
- **On the device** (RG DS Plus, the user's RomM 5.4.1 over the LAN and over Tailscale): the pairing by QR; a GBA
  game played here, its save in RomM, carried on in RomM's web player, back here with the newer save; the same with a
  DS game under DraStic and under BananaDS; a game on the second card; a conflict made on purpose; Quick resume on; the
  lid closed during a download; a game started right after a wake (no network yet); the menu's return time after a
  game (the background push). Logs: `/data/banana/logs/romm.log` (every request, its time and result), banana-ui's
  log (the launch lines), RetroArch's log for the game (the save it loaded), fetched over SSH/SFTP.

## Open questions

1. **Grout or a built-in client?** *Answered*: a built-in client.
2. **What does the sync do when the server says a slot was deleted?** *Answered*: the save on the card is kept as
   `.bak` and nothing is deleted (*Oversights*, 10).
3. **Saves left in another core's folder** (a GBA game played under gpSP, then mGBA chosen): *Answered*: only the
   chosen core's save is synced (*Oversights*, 2). Offering to copy the other core's save over when the chosen one has
   none is not decided: a suggestion for the plan to weigh.
4. **The sync timing.** *Answered*: around every game and Sync now.
5. **Where downloads go.** *Answered*: a setting, main or second card, with the fallback to the main card.
6. **What is in scope.** *Answered*: the DS's saves and BIOS downloads; not save states or box art.
7. **The server.** *Answered*: RomM 5.4.1, http, on the LAN or over Tailscale.
8. **The helper's language.** *Answered*: C++.
9. **Where the automatic sync hooks in.** *Answered*: the `systems.conf` wrapper.
10. **A conflict in the automatic sync.** *Answered*: the card's save kept, the conflict listed in Settings.
11. **The suggestions.** *Answered*: the Quick resume guard, the push in the background, play time to RomM, the second
    card's fallback.
12. **One feature or two.** *Answered*: one, saves first.
13. **RetroArch master's Cloud Sync, now that it pulls before the game** (24 Oct). *Answered*: the built-in client
    stays; RetroArch's WebDAV isn't used (*Suggestions*, 6).
14. **Which JSON reader the helper uses, now that upstream has one** (24 Oct). *Answered*: the fork's
    (`ui/src/kiwami/json.h`).
15. **Where RomM goes in Settings, now that the fork has a group of its own** (24 Oct, after 0019). *Answered*: a group
    of its own, after RetroAchievements, rather than a row under Network.
