# 0008 RomM: plan

Issue: #8 · Branch: `k/0008-romm` · Investigation: [investigation.md](investigation.md) · Status: approved

## Summary

Banana gets a RomM client of the fork's own. It has three parts:

- **A helper**, `/usr/libexec/kiwami/romm`: a C++ program built from `ui/src/kiwami/romm/` by a new package,
  `kiwami-romm`. It does everything that talks to the server: pairing, saves, Sync now, the game lists, downloads and
  BIOS files. Requests go through libcurl, `dlopen()`ed as `raclient.cpp` and `banana-scrape` do; replies are read with
  the fork's JSON reader, and save hashes come from rcheevos' vendored `md5.c`.
- **A wrapper**, `/usr/libexec/kiwami/romm-game`, which `kiwami/post-build.sh` puts in front of the nine game
  systems' `run` lines in the image's `systems.conf`. With RomM on it pulls the game's save before the game and
  pushes it after, in the background. Otherwise it `exec`s the launcher, so the game runs as it does on `main`.
- **Settings > RomM**, an eighth group after RetroAchievements, with seven rows:
  - RomM, On/Off;
  - Account: the server, pairing by QR code, log out;
  - Sync around games, On/Off;
  - Sync now: progress, results, conflicts to resolve;
  - Games: RomM's platforms, their games, downloads;
  - Downloads to: Main card or Second card, shown only with a second slot;
  - BIOS.

  Settings reads the rows' values from the card. It runs the helper only from a RomM page, never when it opens.

The approach is the investigation's option 1, saves first: steps 1 to 4 give a working sync around games and Sync
now, steps 5 and 6 their Settings pages, steps 7 and 8 the browser, downloads and BIOS. No upstream file changes: the
group's rows are added from the existing RetroAchievements hook call (`ui/src/settings.cpp:507`).

### Refinements over the investigation

Decided while planning (24 Oct 2026):

- **The group's rows** are the seven above. RomM (`romm`, default Off), Sync around games (`romm_autosync`, default
  On) and Downloads to (`romm_target`, `main` or `sd2`, default `main`) are upstream-style choice rows in
  `settings.conf`, which upstream's Settings loads and saves. Account, Sync now, Games and BIOS are the fork's pages.
- **The server's version** (`GET /api/heartbeat`, `SYSTEM.VERSION`):
  - below 5.4.0 is refused: every verb that syncs exits, and the Account page says the server is too old;
  - 5.4.x syncs;
  - a newer version syncs too, with a `romm.log` line and the Account page saying it is newer than the client was
    written for.
- **Log out deletes the device on the server**, then forgets it. `DELETE /api/devices/{id}` revokes the token and
  drops the handheld from RomM's devices (best effort: with no server it is skipped and logged). Then the token and
  device id leave `accounts.conf`.
- **The tile is RomM's logo, pixelated**: a 32x32 pixel map drawn after RomM's isotipo, beside RetroAchievements' in
  `ra-icon.cpp`. It shows a purple disc with a controller's corner and four peach buttons, in the icons' look (step 5
  checks the logo's terms: RomM's repository is AGPL-3.0, and the map is a new drawing, not the file).

Found while planning, from the code and RomM 5.4.1's source:

- **The web player's saves pair with the handheld's.** RomM's EmulatorJS player uploads with
  `emulator = <its core>` in the slot `autosave` (`frontend/src/views/Player/EmulatorJS/utils.ts`). When it loads a
  save it picks by slot, not by core; only save states are filtered by core. So the client sends slot `autosave` and
  its core's id as `emulator`, and leaves out negotiation's `emulators` filter: the newest `autosave` pairs whichever
  emulator wrote it (the investigation's "one RomM save per game").
- **The negotiation is scoped.** `rom_ids` holds the game's id for a pull or push, and the card's matched games' ids
  (at most 500 per request, `MAX_ROM_IDS_PER_QUERY`) for Sync now. Without it RomM offers every save the user has
  (`backend/endpoints/sync/__init__.py`).
- **An upload RomM refuses with 409** ("Slot has a newer save since your last sync") is a conflict, recorded as one.
- **The wrapper doesn't pass `TERM` on.** bananad signals the whole process group (`system/bananad/bananad.c:563`), so
  the game has it already, and a second one could cut DraStic's resume save short (`run-drastic`'s trap waits 5 s for
  that reason). The wrapper takes it, waits for the game, and records the push as pending.
- **The Quick resume guard is in the helper**, so it covers every save that is replaced (a pull, Sync now, a conflict
  resolved). It is needed for RetroArch only: `run-drastic` and `run-bananads` already drop a resume state older than
  the save, and a pulled save is written with the current time.
- **A pulled DS save is written as DraStic's `.dsv`, then `nds-saves` runs.** `nds-saves` makes BananaDS's `.sav` from
  it, raw, at its exact size, with a DeSmuME footer stripped (which a save from another device's DS core may carry).
  Pushes send the raw `.sav` after `nds-saves`, as decided.
- **One file for `key=value` edits.** `rastore.cpp`'s `confGet`, `edit` and `replace` move to `ui/src/kiwami/conf.{h,cpp}`,
  which RetroAchievements' store, the helper and the RomM pages share. The helper writes `accounts.conf` only when
  pairing and logging out.
- **`kiwami/DEPENDENCIES.md`**: RomM's API (5.4.1) goes in the fork's own table, since it isn't upstream's and isn't
  shipped; libcurl goes in upstream's table.
- **Corrected**: the image has the `curl` command (`BR2_PACKAGE_LIBCURL_CURL=y`, since upstream's `e9352e7`, the
  Store). The investigation said it hadn't; nothing depends on it.

## Acceptance criteria

The group and its rows

- **A1**: given Settings opens on its groups, then:
  - RomM, with its logo, is the eighth tile, after RetroAchievements, and the first two rows are the same pixels as
    with the fork's groups left out;
  - with `KIWAMI_ROMM_GROUP=0` the grid is RetroAchievements' seven groups, and with both groups left out, upstream's
    six;
  - the group's rows are RomM (Off), Account, Sync around games (On), Sync now, Games, Downloads to (only with a second
    slot) and BIOS; the log has `banana-ui: kiwami: rows in RomM: ...`.
- **A2**: given any state of the card, when Settings opens, then no helper runs and nothing is asked of the server.
  The rows' values come from the card:
  - Account: `Not paired`, or the server's host;
  - Sync now: `Off`, `Not yet`, `Synced`, `<n> conflicts`, `No server`, `Pair again` (the last sync's result).
- **A3**: given RomM Off (the default), or not paired, or Sync around games Off, when a game starts, then the wrapper
  `exec`s the launcher with the same arguments, environment and pid: no helper runs, and nothing is written under
  `/data/banana/romm/`.

The account

- **A4**: given the Account page, when a server is typed, then it is kept as `romm_url` in `accounts.conf`, with
  `http://` added when there's no scheme and a trailing `/` removed. The file's other lines stay as they were.
- **A5**: given a server, when Pair is pressed, then the top panel shows the QR code of RomM's pairing link and the
  code to type. Once the user approves it in RomM, `romm_token` and `romm_device_id` are in `accounts.conf` and the
  page says Paired, with the server's version. Denied, or expired, the page says so and nothing is written. Pairing
  again replaces a pairing still waiting.
- **A6**: given a server below 5.4.0, then the page says it is too old and every syncing verb refuses. 5.4.x syncs; a
  newer version syncs with a warning (log and page). No answer: `No server`.
- **A7**: given a token RomM refuses (401), then the Account page and the Sync now row say `Pair again`, and no save on
  the card changes. Log out, A twice, asks RomM to delete the device (skipped with no server), then removes
  `romm_url`'s token and device id; the server address and every other line stay. One A, then any other key, logs
  nothing out.
- **A8**: the token appears nowhere but `accounts.conf`. Not in `romm.log`, banana-ui's log, the status file or a
  command line (the helper reads it from the file).

Saves around a game

- **A9**: given RomM on, paired, Sync around games on, and a game RomM has, when it starts, then:
  - if RomM's `autosave` is newer, the card's save is replaced, the old one kept as `.bak`, in the folder of the core
    that will run (`core.<system>` honoured), and RetroArch's `<game>.state.auto` renamed `.bak`;
  - if RomM's is the same or older, nothing on the card changes.
- **A10**: given the same, when the game quits, then the menu comes back without waiting for the network. The save
  goes up in the background:
  - into slot `autosave`, `emulator` the core's libretro id (`melonds` for the DS), with its MD5;
  - with a play session (start, end, duration).
- **A11**: given both the card's save and RomM's changed since the last sync, then the card's save is kept, neither
  side is overwritten, and the conflict is listed: the Sync now row says `1 conflict`. Resolving it:
  - "Keep this card's" uploads with `overwrite`;
  - "Take RomM's" downloads RomM's, with the card's kept as `.bak` and the guard of A9.

  Either way it leaves the list.
- **A12**: given RomM answers `delete` for a slot emptied there, then the card's save is renamed `.bak` and logged;
  nothing on the card is deleted.
- **A13**: given no network, a server that doesn't answer or one that stalls, when a game starts, then the pull gives
  up within 3 s and the game starts with the card's save. A push that fails is kept pending, with its play session,
  and goes with the next push or Sync now.
- **A14**: given a shutdown while playing (`TERM` to the game's process group), then the wrapper doesn't send the game
  another `TERM`. It waits for the game to end, records the push and the play session as pending, and makes no
  request.
- **A15**: given a DS game:
  - after a game under either emulator, `nds-saves` runs before the push, and the raw `.sav` goes up;
  - a save pulled from RomM is written as `drastic/backup/<game>.dsv` and `nds-saves` makes `saves/nds/<game>.sav` from
    it, its size exact and a DeSmuME footer left off, so both emulators start from it.
- **A16**: given a game on the card, then its RomM id is found:
  - first from the downloads' record;
  - then by its file name in its platform's list (exactly, then without case);
  - for a game in a folder of its own, by the folder's name.

  A game RomM doesn't have is skipped, logged and counted in Sync now's result. A game on the second card syncs its
  save on `/data`, and a game on both cards is one save.
- **A17**: given a card with its own `/data/banana/systems.conf` without the wrapper, then games start as on `main`.
  Sync around games' help and the Sync now page say the automatic sync is off for that card, and Sync now still
  works.
- **A18**: given the image's build, then `post-build.sh` puts the wrapper in front of the `run` lines of `nds gba gbc
  gb nes snes megadrive psx n64`, once. A second run changes nothing, the apps', PICO-8's and the Ports' lines are left
  alone, and a missing section or `run` line stops the build. Every shelf still starts its launcher, and the RetroArch
  systems still get the game panel.

Sync now

- **A19**: given RomM on and paired, when Sync now is pressed, then:
  - the pending pushes go first;
  - every game on both cards with a save, or with a save in RomM, is synced (uploads, downloads, conflicts, games
    RomM doesn't have), with progress on the page;
  - the result is kept on the card and becomes the row's value.

  Only one sync runs at a time: Sync now while one runs does nothing, and a push after a game waits for the one
  running (up to 60 s, then pending).
- **A20**: given a transfer cut short (sleep, Wi-Fi dropping, power), then no save, game or BIOS file is ever left
  half-written. Each is written to a temporary and renamed, and what didn't finish is pending or retried.

Games, downloads, BIOS

- **A21**: given the Games page, then it lists RomM's platforms that map to Banana's systems, with their game counts.
  A platform's games come 50 to a page, with a search through the keyboard, and a game on either card is marked.
  When A is pressed on one, then:
  - it downloads into the target card's `roms/<folder>/`, with progress;
  - a game of several files is unpacked into a folder of its name;
  - it goes through a temporary name the library doesn't list;
  - its id is recorded, and the library lists it once it's done.
- **A22**: given a download, then:
  - with Downloads to set to Second card and the second card not in, it goes to the main card and the page says so;
  - free space is checked first: not enough, and nothing is written;
  - it holds `/run/banana/awake.pid`, when free, until it ends;
  - one interrupted resumes from where it stopped (HTTP range), or starts over when the server sends the whole file.
- **A23**: given BIOS, when pressed, then RomM's firmware for the mapped platforms goes into `/data/bios`. A file
  already there with the same MD5 is skipped, and one with another MD5 is kept as `.bak`.

Footprint, both boards

- **A24**: with nothing running:
  - no process of the feature is alive in the menu;
  - the groups' frame time with the RomM group is within 10% of the frame time without it (`bench`);
  - the helper is under 1 MB;
  - with RomM Off, a game's start costs one `exec`.
- **A25**: every request is one `romm.log` line (verb, method, path, status or `no connection`, milliseconds), and the
  log is kept under 256 KB (the last one as `romm.1.log`). The pages are drawn right at 640x480 and 1024x768.

## Risks

- **Saves overwritten by older ones**, the worst case. Guarded by:
  - RomM's hash-based negotiation;
  - a conflict never overwrites, in the automatic sync or in Sync now;
  - every replaced save kept as `.bak`, and nothing deleted;
  - writes through a temporary and a rename;
  - the stand-in server's stateful negotiation, which follows RomM's `compare_save_state` (step 2), and the helper's
    tests of each operation;
  - M2 to M6 on the device, against the user's RomM.
- **The stand-in server drifting from RomM.** It is written from RomM 5.4.1's source (the negotiation, the 409 rule,
  the device flow), not from the docs. The manual tests run against a real RomM 5.4.1. A RomM upgrade can still change
  the API: the version check warns, and `kiwami/DEPENDENCIES.md` names the version the client follows.
- **The web player's saves in another emulator's format** (a GBA save from mGBA in the browser, read by gpSP here).
  Saves are raw SRAM or flash for every core used, and the DS's footer is handled. M3 checks a GBA game both ways, and
  per-system differences stay a known limit.
- **A game started right after a wake, or during a long download.** The pull waits at most 3 s (no network yet, or the
  saves lock held). The game then starts with the card's save, and the push after it still goes. Tested by A13 and M7.
- **Performance and footprint.**
  - Nothing runs in the menu: no daemon, no polling while no RomM page shows. Settings opening reads two small files.
  - A game's start: with RomM off, one `exec`; on, one negotiation, tens of milliseconds on the LAN, at most 3 s with no
    network.
  - The push runs after the game, in its own session.
  - Rootfs: the helper (a few hundred KB) and a shell script.
  - The group's tile is drawn from a pixel map, like RetroAchievements'.
  - Covered by A24's bench and M8's timings.
- **Conflicts with upstream.** No upstream file changes:
  - the rows come from the existing `kiwami:` RetroAchievements hook;
  - the `run` lines are wrapped at build time, and `post-build.sh` stops if their shape changes;
  - a system upstream adds isn't synced until it is added to the list;
  - upstream moving saves (`retroarch.cfg`'s `savefile_directory`, `nds-saves`, `run-retroarch`'s core choice) has to
    be followed by the helper: those files go in `docs/FORK.md`'s list of what the fork shadows (step 4).

  The `run` lines are the most likely to move, and the build catches that.
- **`awake.pid` is shared with `banana-scrape`**, which writes it whether it is free or not and removes it when it
  ends. A scrape started during a download takes the idle-sleep hold away when the scrape finishes. The download then
  only depends on the user not leaving the handheld idle for 10 minutes; if it is cut, it resumes. Accepted: a
  scrape and a download at once is rare, and the hold is a convenience.
- **busybox `unzip` and very large zips.** A game of several files over 4 GB, a ZIP64 archive, may not unpack. The
  download then says so and keeps the zip. Rare for the nine systems: PS1 discs are under 1 GB each.

## Steps

Each step is one commit, and leaves the tree building and `make test` and `kiwami/tests/ci.sh .buildroot` passing.
Paths in `/data`, `/run/banana`, `/etc/banana` and `/usr/lib/libretro` are moved for the tests by `KIWAMI_ROMM_CARD`
(default `/data`), `KIWAMI_RUN` (default `/run/banana`, the Tailscale helper's name for it), `KIWAMI_ROMM_ETC`
(default `/etc/banana`), `KIWAMI_LIBRETRO` (default `/usr/lib/libretro`, as `kiwami.cpp` has it) and
`KIWAMI_ROMM_NDS_SAVES` (default `/usr/libexec/banana/nds-saves`); `KIWAMI_ROMM_SYNC=1` keeps the helper from
detaching, for the tests.

### 1. The helper, its package, and pairing

- **Files**:
  - `ui/src/kiwami/conf.h`, `ui/src/kiwami/conf.cpp` (new)
  - `ui/src/kiwami/rastore.cpp` (changed)
  - `ui/src/kiwami/rommcard.h`, `ui/src/kiwami/rommcard.cpp` (new)
  - `ui/src/kiwami/romm/romm.mk`, `main.cpp`, `http.h`, `http.cpp`, `api.h`, `api.cpp` (new)
  - `kiwami/package/kiwami-romm/Config.in`, `kiwami/package/kiwami-romm/kiwami-romm.mk` (new)
  - `kiwami/Config.in`, `kiwami/configs/kiwami.fragment` (changed)
  - `kiwami/tests/romm-server.py`, `kiwami/tests/romm/library.json`, `kiwami/tests/romm-helper.sh` (new)
  - `kiwami/tests/units.cpp`, `kiwami/tests/ci.sh` (changed)
  - `kiwami/DEPENDENCIES.md` (changed)
- **Change**:
  - **`conf.{h,cpp}`**, in `banana::kiwami`: `confGet(path, key)`; `confEdit(path, edits, sep, create, error)`, with
    `struct ConfEdit { key, value, del }`; `replaceFile(path, text, error)` (`<path>.new`, `fsync`, `rename`). Moved
    unchanged from `rastore.cpp`'s anonymous namespace. `rastore.cpp` calls them; its tests in `units.cpp` pass
    unchanged.
  - **`rommcard.{h,cpp}`**: the card as the helper and Settings both read it, with no network. `struct Card` (paths
    from the environment above).
    - `Account account(card)`: `romm_url`, `romm_token`, `romm_device_id` from `accounts.conf`.
    - `setServer(card, url, error)`: normalised, `http://` added when there's no scheme and a trailing `/` removed.
    - `paired(...)` writes the token and device id; `forget(...)` removes the token and device id. Both through
      `confEdit`, keeping every other line.
    - `std::string setting(card, key, dflt)`: the card's `settings.conf`, else `/etc/banana/settings.conf`, as
      `run-retroarch`'s `setting()` reads it.
    - `deviceIdentifier(card)`: a random 16-hex id made once and kept in `banana/romm/identifier`, RomM's
      `client_device_identifier`.
  - **`romm/http.{h,cpp}`**: libcurl `dlopen()`ed (`libcurl.so.4`), the `curl_easy_*` and `curl_slist_*` calls only,
    option numbers as `raclient.cpp` writes them. One `Request` struct: method, URL, headers, body, the timeouts
    (connect and total, in ms), a file to write the body to with a resume offset, a progress callback. It returns
    `{ long status (0: none), std::string body, double ms, std::string error }`. `Authorization: Bearer` comes from
    the account. A multipart body (an upload's `saveFile`) is built by hand, with its own boundary. Every request
    logs one line through `log()`; the URL's query is kept, and a header never is.
  - **`romm/api.{h,cpp}`**: each request and its reply, parsed with `json.h`. The parsers are pure functions, unit
    tested:
    - `heartbeat` → version;
    - `deviceInit(name, identifier, version, scopes)` → `{ device_code, user_code, verification_path_complete,
      expires_in, interval }`;
    - `deviceToken(code)` → `{ access_token, device_id }`, or `authorization_pending`, `slow_down`, `access_denied`
      or `expired_token`;
    - `deleteDevice(id)`.

    The scopes asked are `roms.read platforms.read firmware.read assets.read assets.write devices.read devices.write
    roms.user.write`. The name sent is "BananaOS on <model>" (`/proc/device-tree/model`), the client "BananaOS", the
    version `/etc/banana/version`. `versionOk(v)` gives `OLD` below 5.4.0, `OK` for 5.4.x and `NEWER` above.
  - **`romm/main.cpp`**: `romm <verb> [args]`. Its log is `banana/logs/romm.log`, each line timestamped in the menu's
    time zone, moved to `romm.1.log` past 256 KB. The verbs:
    - `check`: one line to stdout, `ok <v>`, `newer <v>`, `old <v>`, `unpaired`, `revoked` (401 on
      `GET /api/devices/<id>`) or `down`. Exit 0 for `ok` and `newer`.
    - `pair`: `romm_url` from `accounts.conf`. `POST /api/auth/device/init`, then status and stdout
      `pairing <user_code>\t<url + verification_path_complete>`. Then it detaches and polls `/api/auth/device/token`
      every `interval` s (5 s more after `slow_down`) until it is approved (the keys written, status
      `paired <version>`), denied or expired (status `pair-failed denied|expired`). Its pid is in
      `$RUN/romm-pair.pid`, and a second `pair` stops the first.
    - `logout`: `DELETE /api/devices/<id>` (best effort, 5 s), then `forget`.

    The status file is `$RUN/romm.status`, one line, written through `.new` and a rename.
  - **`romm/romm.mk`**: `make -f src/kiwami/romm/romm.mk OUT=<dir>` (run in `ui/`) builds `<OUT>/romm` from
    `romm/*.cpp`, `json.cpp`, `conf.cpp`, `rommcard.cpp` and rcheevos' `src/rhash/md5.c` (as C, as `kiwami.mk` builds
    it), `-std=c++17 -Wall -O2 -ldl`.
  - **`kiwami/package/kiwami-romm/`**: `KIWAMI_ROMM_SITE = $(BR2_EXTERNAL_KIWAMI_PATH)/../ui`, `SITE_METHOD = local`,
    built with `romm.mk` (`TARGET_CXX`, `TARGET_CC`), installed as `/usr/libexec/kiwami/romm`. `Config.in` selects
    `BR2_PACKAGE_LIBCURL` (upstream's image has it: libcurl comes at run time). `kiwami/Config.in` sources it, and
    the fragment sets `BR2_PACKAGE_KIWAMI_ROMM=y`.
  - **`kiwami/tests/romm-server.py`**: a stand-in RomM 5.4.1 on 127.0.0.1, a free port written to a file, as
    `ra-server.py`. Its library comes from `kiwami/tests/romm/library.json`: platforms, ROMs with their files' bytes
    made up from a seed, firmware. In this step it answers:
    - `GET /api/heartbeat` (the version from `KIWAMI_ROMM_VERSION`, default `5.4.1`);
    - the device flow, with `authorization_pending` until the test calls `POST /test/approve`, `/test/deny` or
      `/test/expire`, and `slow_down` when polled too fast;
    - `GET /api/devices/<id>` and `DELETE /api/devices/<id>`;
    - `POST /test/revoke`, after which the device's token gets 401;
    - with `KIWAMI_ROMM_DOWN`, closing every connection; with `KIWAMI_ROMM_STALL=<s>`, holding each answer that long.

    Each request appends `METHOD path?query auth=<0|1>` to its log (never the token).
  - **`kiwami/tests/romm-helper.sh`**: the helper built for the host (`romm.mk`, `OUT` a temporary folder), on a
    made-up card and `/run/banana`, against the stand-in server. It checks:
    - `check` against 5.3.2, 5.4.0, 5.4.1 and 5.5.0, down, unpaired and revoked;
    - `pair` approved, denied and expired, and a second `pair` replacing the first;
    - `logout` with the server up (`DELETE` logged) and down, with `accounts.conf`'s other lines (`ra_user=...`) kept
      in both;
    - the token absent from `romm.log`, the status file and the server's log;
    - `romm.log` rotated past 256 KB.
  - **`kiwami/tests/units.cpp`**: the `api.cpp` parsers (heartbeat, init, each token answer, a malformed one),
    `versionOk`, `rommcard`'s server normalisation and its keys' edits, `conf.cpp` through rastore's existing tests.
  - **`kiwami/tests/ci.sh`**: `conf.cpp`, `rommcard.cpp` and `romm/api.cpp` added to the units' build;
    `romm-server.py` compiled; `romm-helper.sh` run.
  - **`kiwami/DEPENDENCIES.md`**:
    - the fork's table: "RomM's API, 5.4.1 (tag `9c74d01`, 22 Oct 2026), not shipped, the server the client is written
      against: `ui/src/kiwami/romm/api.cpp`, `kiwami/tests/romm-server.py`; a bump checks the negotiation, the save
      upload's 409 rule and the device flow against the new tag";
    - upstream's table: "libcurl (Buildroot's), `dlopen()`ed by the RomM helper".
- **Tests**: `kiwami/tests/romm-helper.sh`, `units.cpp`; `kiwami/tests/ci.sh .buildroot` (every board configures with
  `BR2_PACKAGE_KIWAMI_ROMM`); `make test`; `make -C ui`; `make kiwami-romm-rgds-plus` builds it for the target.
- **Commit**: `RomM: a helper of the fork's own, its package, and pairing by RomM's device flow`

### 2. A game's save: its RomM id, the pull before it and the push after

- **Files**:
  - `ui/src/kiwami/romm/saves.h`, `saves.cpp`, `games.h`, `games.cpp` (new)
  - `ui/src/kiwami/romm/api.h`, `api.cpp`, `main.cpp` (changed)
  - `ui/src/kiwami/rommcard.h`, `rommcard.cpp` (changed)
  - `kiwami/tests/romm-server.py`, `kiwami/tests/romm/library.json`, `kiwami/tests/romm-helper.sh` (changed)
  - `kiwami/tests/units.cpp`, `kiwami/tests/ci.sh` (changed)
- **Change**:
  - **`saves.{h,cpp}`**:
    - **`systems(card)`**: `/data/banana/systems.conf`, else `/etc/banana/systems.conf`: each section's id, `folder`,
      `ext`, `flat`, `run`.
    - **`SYNCED`**: the nine systems and their RomM slugs: `nds`→`nds`; `gba`, `gbc`, `gb`, `n64`, `psx` the same;
      `nes`→`nes` or `famicom`; `snes`→`snes` or `sfam`; `megadrive`→`genesis`.
    - **`coreOf(card, system)`**: `core.<system>` when `<libretro>/<c>_libretro.so` is there, else the word after
      `run-retroarch` in the `run` line, `pcsx_rearmed` for `run-ps1`, `nds` for `run-nds`. This is
      `run-retroarch:14-19`'s choice.
    - **`LIBRARY_NAME`**: the cores' save folders: `gpsp`→`gpSP`, `mgba`→`mGBA`, `gambatte`→`Gambatte`,
      `sameboy`→`SameBoy`, `fceumm`→`FCEUmm`, `mesen`→`Mesen`, `snes9x`→`Snes9x`, `picodrive`→`PicoDrive`,
      `genesis_plus_gx`→`Genesis Plus GX`, `mupen64plus_next`→`Mupen64Plus-Next`, `pcsx_rearmed`→`PCSX-ReARMed`. A
      core not in it is skipped and logged.
    - **`saveOf(card, system, rom)`**: the RetroArch systems' save is
      `/data/saves/<library_name>/<rom stem>.srm`, and their state `/data/states/<library_name>/<stem>.state.auto`. The
      DS's is `/data/saves/nds/<stem>.sav`, with `nds-saves <rom>` run before it is read. Its `emulator` is the core id,
      `melonds` for the DS.
    - **`replaceSave(save, bytes)`**:
      - the old file to `.bak`, then the new one through `.new` and a rename, its time now;
      - for the DS, written as `/data/drastic/backup/<stem>.dsv` (the old one to `.bak`), then `nds-saves <rom>`;
      - for RetroArch, `<stem>.state.auto` renamed `.state.auto.bak` (the Quick resume guard), logged.
    - **`dropSave(save)`**, on a `delete`: the save renamed `.bak`, logged.
  - **`games.{h,cpp}`**: a game's RomM id.
    - First from `banana/romm/games` (`<system>\t<file name>\t<rom id>` lines: downloads, step 7, and matches found
      before).
    - Then from the platform's list, cached in `banana/romm/platforms/<slug>.tsv` (`<id>\t<fs_name>\t<size>\t<name>`;
      `GET /api/platforms`, then `GET /api/roms?platform_ids=<id>&limit=10000`). The file's name is matched against
      `fs_name` exactly, then without case. A game in a folder of its own is matched by the folder's name. A match is
      written to `games`.
    - On a miss the list is fetched again, at most once per run of the helper. Not found: `none`, logged.
  - **`api`**:
    - `platforms()` and `roms(platform, search, offset, limit)`, the parsers for `items`/`total`;
    - `negotiate(saves, romIds)` → `session_id` and the operations (`action`, `rom_id`, `save_id`, `slot`,
      `file_name`, `server_content_hash`);
    - `uploadSave(rom, emulator, slot, deviceId, hash, session, overwrite, file)`: `POST /api/saves?...` with
      `autocleanup=true`, multipart `saveFile`; 409 is a conflict;
    - `downloadSave(id, deviceId, session)` → bytes;
    - `confirmDownload(id, deviceId, hash)`;
    - `complete(session, done, failed, playSessions)`.
  - **`rommcard`**:
    - `banana/romm/pending`: `push\t<system>\t<rom>` and `play\t<rom id>\t<start>\t<end>` lines, each once;
    - `banana/romm/conflicts`: `<system>\t<rom>\t<rom id>\t<save id>\t<server time>\t<noted>`;
    - `banana/romm/last`: the last sync's result, `<time>\t<state>\t<sent>\t<fetched>\t<conflicts>\t<skipped>`,
      where state is `synced`, `conflicts`, `down` or `revoked`.

    Each is read, and written through `replaceFile`.
  - **`main.cpp`**:
    - **The saves lock**: `flock` on `$RUN/romm-saves.lock`. `pull` waits within its budget, `push` up to 60 s (else
      pending), `pending` doesn't need it.
    - **The version check**: `heartbeat` first in `pull` and `push`. `OLD` stops them, logged; `NEWER` logs a warning.
    - **`pull <system> <rom>`**, within 3 s in all (connect 1.5 s, each request the rest of the budget):
      - the id (A16), and the save read;
      - `negotiate` with the save (when there is one) and `rom_ids=[id]`;
      - the `autosave` slot's operation:
        - `download`: `downloadSave`, `replaceSave`, `confirmDownload`;
        - `conflict`: recorded in `conflicts`;
        - `delete`: `dropSave`;
        - `upload` or `no_op`: left for the push;
      - `complete`.

      Every failure is logged and exits 0: the game starts.
    - **`push <system> <rom> <start> <end>`**: detaches (fork, `setsid`, the parent returns at once) unless
      `KIWAMI_ROMM_SYNC=1`, then takes the lock.
      - One negotiation for this game and every pending one (their saves and ids).
      - `upload` → `uploadSave`, and a 409 is recorded as a conflict; `download` → as in `pull`; `conflict` →
        recorded; `delete` → `dropSave`.
      - `complete` with this play session and the pending ones (UTC ISO times, `duration_ms`, `save_slot`
        `autosave`).
      - On success the pending lines it carried go. When there is no network, no server, or 401, this game and its
        play session are added to `pending`.
    - **`pending <system> <rom> <start> <end>`**: the push and the play session added to `pending`, no network.
  - **`romm-server.py`**: a stateful sync, following RomM 5.4.1's `sync/__init__.py` and `handler/sync/comparison.py`.
    - It keeps saves per user, `(rom_id, slot)`, each with its hash and time, and per device the last synced time and
      hashes.
    - `negotiate` answers as `compare_save_state` and `compare_missing_server_save` do, scoped by `rom_ids`. Deleted
      slots answer `delete`.
    - `POST /api/saves` has the 409 rule; `GET /api/saves/<id>/content` and `POST /api/saves/<id>/downloaded`.
    - `POST /api/sync/sessions/<id>/complete` records play sessions. `GET /api/platforms` and `GET /api/roms` come from
      `library.json`.
    - For the tests: `POST /test/save?rom_id=&slot=&emulator=` uploads as another device (the web player),
      `/test/delete-slot`, and `GET /test/saves` and `GET /test/play-sessions` dump the state.
  - **`library.json`**: GBA, DS, PS1 (one multi-file disc), SNES and Mega Drive (`genesis`) platforms, with games named
    to exercise matching: case, spaces, a folder.
- **Tests**:
  - **`romm-helper.sh`**:
    - pull and push of a GBA save under gpSP, and under mGBA with `core.gba = mgba`;
    - the web player's newer save pulled, the old one as `.bak`, `.state.auto` renamed;
    - a same-hash no-op touching nothing;
    - a conflict (both changed) recorded, the card's save untouched;
    - a 409 on upload recorded;
    - a `delete` turning the save into `.bak`;
    - a DS save after DraStic (a newer `.dsv`: `nds-saves` run, the raw `.sav` uploaded), and pulled with a DeSmuME
      footer (the `.sav` raw at its size). This runs upstream's `nds-saves` with its `/data` moved into the card by
      `sed`, through `KIWAMI_ROMM_NDS_SAVES`;
    - the server down, and stalling 10 s: `pull` returns within 3.5 s and exits 0;
    - `push` failing, then `pending` sent by the next `push`, with the play sessions in `GET /test/play-sessions`;
    - a game RomM doesn't have, skipped;
    - a second card's game, its save on `/data`;
    - 5.3.2 refused, 5.5.0 synced with the warning in `romm.log`;
    - two pushes at once: the second waits for the lock.
  - **`units.cpp`**: `systems()` over upstream's `systems.conf`, `coreOf` with and without `core.<system>` (the `.so`
    there or not), `saveOf` for each of the nine, name matching (exact, case, folder, none), the negotiation parser.
- **Commit**: `RomM: a game's save pulled from RomM before it and pushed after, the conflicts kept for later`

### 3. Sync now and the conflicts resolved

- **Files**: `ui/src/kiwami/romm/main.cpp`, `games.cpp`, `kiwami/tests/romm-helper.sh` (changed)
- **Change**:
  - **`sync`**: detaches (unless `KIWAMI_ROMM_SYNC=1`) and takes the saves lock without waiting: it exits 3, `busy`, when
    held.
    - The games of the nine systems on both cards, listed by `ext` (into subfolders unless `flat`), one per
      `(system, stem)`. Each platform's list is fetched once, and the games are matched (A16).
    - Pending pushes first. Then negotiations in batches of at most 500 ids, each with the saves the card has, and
      every operation applied as `push` applies it.
    - Progress in the status file, `running <done> <total>\t<system>\t<game>`. Then
      `done sync <sent> <fetched> <conflicts> <skipped>` and `last` written, or `error sync down|revoked|old`.
    - The games RomM doesn't have are listed in `romm.log` and counted as skipped.
  - **`resolve <n> local|server`**: the n-th line of `conflicts`, under the lock.
    - `local`: the save uploaded with `overwrite=true`.
    - `server`: negotiated again for that game; the current save of the slot downloaded and put in place by
      `replaceSave`.
    - The line removed and `last`'s count updated. It exits 1, leaving the line, when the request fails.
- **Tests**:
  - **`romm-helper.sh`**:
    - a card with games on both cards and in subfolders, some with saves, some with only RomM's, one RomM doesn't
      have, one conflict: the counts in the status and in `last`, the files on the card, the server's saves;
    - a second `sync` while one runs (the lock held by the test) exits 3;
    - pending sent first;
    - `resolve 1 local` and `resolve 1 server` (the `.bak`, the guard, the list emptied);
    - `sync` with the server down: `last` says `down`.
- **Commit**: `RomM: Sync now for every game on both cards, and a conflict resolved either way`

### 4. The wrapper around the games, put into the image's systems.conf

- **Files**:
  - `kiwami/rootfs/usr/libexec/kiwami/romm-game` (new)
  - `kiwami/post-build.sh` (changed)
  - `kiwami/tests/romm-game.sh` (new)
  - `kiwami/tests/ci.sh` (changed)
  - `docs/FORK.md` (changed)
- **Change**:
  - **`romm-game <launcher> <args...>`** (sh).
    - It `exec "$@"` unless all of these hold:
      - `settings.conf`'s `romm` is `on` (the card's, else the image's; `run-retroarch`'s `setting()`);
      - `romm_autosync` isn't `off`;
      - `accounts.conf` has `romm_token`;
      - the helper is executable;
      - `BANANA_SYSTEM` and `BANANA_ROM` are set (banana-ui exports them).
    - Otherwise:
      - `start=$(date +%s)`; `romm pull "$BANANA_SYSTEM" "$BANANA_ROM"`;
      - the game as a child, `"$@" <&0 &`, so it keeps banana-ui's stdin;
      - `trap 'term=1' TERM INT HUP`: not passed on, since the group has it;
      - waited for until it is gone, as `run-retroarch` waits;
      - then `romm pending ...` after a `TERM`, else `romm push ...` (which detaches);
      - `exit` with the game's status.
  - **`kiwami/post-build.sh`**: an awk pass over `$T/etc/banana/systems.conf`, as the `7z` one. In the sections
    `nds gba gbc gb nes snes megadrive psx n64`, a `run =` line not already starting with
    `/usr/libexec/kiwami/romm-game ` gets it in front of the command. A chosen section with no `run` line stops the
    build, naming it. It is written through `.tmp`.
  - **`docs/FORK.md`**, *Taking in upstream* 3: `systems.conf`'s `run` lines (the nine wrapped), `retroarch.cfg`'s
    `savefile_directory` and `savestate_directory`, `run-retroarch`'s core choice and `nds-saves` (where the helper
    finds saves) added to the files the fork shadows, with what to check. The table's `systems.conf` row mentions the
    wrapper.
- **Tests**:
  - **`kiwami/tests/romm-game.sh`**: the wrapper with a stand-in helper (it records its arguments) and a stand-in game
    (it records its pid, arguments, stdin and environment, and sleeps or exits as told). It checks:
    - each of A3's conditions unmet: the game's pid is the wrapper's (an `exec`), no helper call;
    - all met: `pull`, the game, `push <system> <rom> <start> <end>`, in that order, with the game's exit status kept;
    - `TERM` to the group while the game runs: the game gets one `TERM` only, the wrapper waits, then `pending`;
    - stdin passed through;
    - a ROM with spaces and quotes.
  - **`kiwami/tests/ci.sh`**:
    - `post-build.sh` over upstream's `systems.conf`: the nine `run` lines start with the wrapper once, the others are
      unchanged, and a second run changes nothing;
    - a copy without `[psx]`'s `run` line fails, naming `[psx]`;
    - the seven RetroArch systems' wrapped lines still contain `run-retroarch` or `run-ps1`, the strings banana-ui's
      game-panel test looks for (`ui/src/main.cpp:1383`; headless has no panels to lease, so this is checked on the
      line, and M2 sees the panel);
    - banana-ui headless over the post-built file, as `7z-ui.sh` does: a game on each of the nine shelves is launched
      (its `launch` line).
  - `make test`.
- **Commit**: `RomM: the games' saves synced around each game, through a wrapper in the image's systems.conf`

### 5. Settings: the RomM group and its Account page

- **Files**:
  - `ui/src/kiwami/kiwami.h`, `kiwami.cpp`, `ra-icon.cpp` (changed)
  - `ui/src/kiwami/romm.h`, `ui/src/kiwami/romm-account.cpp` (new)
  - `kiwami/tests/romm-ui.sh` (new)
  - `kiwami/tests/ra-ui.sh`, `kiwami/tests/ci.sh` (changed)
- **Change**:
  - **`Kiwami::rows`**: in the `"RetroAchievements"` call, after RetroAchievements' rows (and whether or not they are
    left out), RomM's are added, unless `KIWAMI_ROMM_GROUP=0`. They are logged on their own line
    (`banana-ui: kiwami: rows in RomM: romm kiwami.romm.account romm_autosync`), so RetroAchievements' line stays as it
    is:
    - `romm`: a choice in `settings.conf`, `{ "Off", "On" }` / `{ "off", "on" }`, icon `kiwami.romm`;
    - `kiwami.romm.account`: a page;
    - `romm_autosync`: a choice, `{ "On", "Off" }` / `{ "on", "off" }`. Its help says when the card's own
      `systems.conf` has no wrapper (A17), which is checked when Settings opens: the file read, no process run.
  - **`romm.h`**: the pages' state, as `ra.h`: the card (`rommcard.h`), the account, the helper's path (`KIWAMI_ROMM`,
    else `/usr/libexec/kiwami/romm`), the status line, the page's rows and selection.
  - **`Page`**: `ROMM_ACCOUNT` added to the enum and to `open/press/touch/render/entryHelp/entered/update/wake`'s
    routing. `opened()` reads the card only.
  - **`romm-account.cpp`**: the Account page, in Tailscale's page's look.
    - Rows: Server (upstream's keyboard, `ENTRY_KIWAMI`; `setServer`), Pair, Log out (A twice; shown when paired).
    - The top panel: the state (`check`'s answer, asked through a `wifi::Job` when the page opens and after Pair or
      Log out, never when Settings opens), then the selected row's help.
    - Pair runs `romm pair` through a job. Its line puts the QR code (`qrFor`, `renderLogin`, as Tailscale's) and the
      code on the top panel. The status file is read every 0.5 s while the page shows, until `paired` or
      `pair-failed`.
    - The row's value is `Not paired`, or the server's host.
  - **`ra-icon.cpp`**: RomM's logo as a second 32x32 map, served by `icon("kiwami.romm")`. The logo's terms are checked
    first (RomM's README and repository for a trademark or logo policy), and the outcome noted in the file's comment.
  - **`kiwami/tests/ra-ui.sh`**: `export KIWAMI_ROMM_GROUP=0` at the top, so its grid of seven groups and its counts
    hold.
  - **`kiwami/tests/romm-ui.sh`**: banana-ui headless at 640x480 and 1024x768, with the helper built for the host
    (`KIWAMI_ROMM`) against `romm-server.py`. It checks:
    - the grid: the eighth tile and its logo; RIGHT/DOWN reaching it; the first two rows the same as with
      `KIWAMI_ROMM_GROUP=0`;
    - the rows and their log line;
    - A2: no helper run and no request on opening, the server's log empty;
    - the server typed (`http://` added; `accounts.conf`'s other lines kept);
    - Pair: the QR shot, `/test/approve`, then Paired with 5.4.1; denied;
    - `check` on opening the page with 5.3.2 (too old), 5.5.0 (newer) and down;
    - revoked: `Pair again`;
    - Log out, one A and then two;
    - the groups' frame time with the group and without (`bench 120`, within 10%);
    - no token in the log.
  - **`ci.sh`**: `romm-ui.sh` run when `ui/banana-ui` is built.
- **Tests**: `kiwami/tests/romm-ui.sh`, `ra-ui.sh`, `tailscale-ui.sh`, `cores-ui.sh`; `make test` (upstream's renders
  unchanged: the first two rows); `make -C ui`.
- **Commit**: `RomM: a group in Settings, after RetroAchievements, with its logo and the Account page`

### 6. Settings: Sync now and the conflicts

- **Files**: `ui/src/kiwami/kiwami.h`, `kiwami.cpp`, `romm.h` (changed); `ui/src/kiwami/romm-sync.cpp` (new);
  `kiwami/tests/romm-ui.sh` (changed)
- **Change**:
  - **The row `kiwami.romm.sync`** (a page), after Sync around games. Its value comes from `last` and `conflicts`:
    `Off` (RomM off), `Not paired`, `Not yet`, `Synced`, `<n> conflicts`, `No server`, `Pair again`.
  - **`romm-sync.cpp`**, the Sync now page.
    - A starts `romm sync`, detached as `banana-scrape` is, and the status is read every 0.5 s while the page shows.
    - The top panel: a progress bar and the game, as `scrapeBar`, then the result: sent, fetched, conflicts, not on
      RomM, the time of the last sync.
    - The bottom panel lists the conflicts (game, system, RomM's time). A on one asks "Keep this card's" or "Take
      RomM's" (LEFT/RIGHT, then A twice), and runs `romm resolve <n> local|server`.
    - Off, unpaired or revoked, the page says what to do instead of starting.
    - With the card's own `systems.conf` unwrapped, the page says the automatic sync is off for this card.
- **Tests**: **`romm-ui.sh`**, with the stand-in server's library:
  - Sync now from the page: progress shot, then the result; the row's value after;
  - a conflict made through `/test/save`, listed, resolved both ways (the card's files and the server's saves
    checked);
  - Sync now down, then revoked;
  - A pressed while a sync runs: nothing new started.
- **Commit**: `RomM: Sync now in Settings, with its progress, its result and the conflicts to resolve`

### 7. The helper's games, downloads and BIOS

- **Files**: `ui/src/kiwami/romm/main.cpp`, `api.h`, `api.cpp`, `games.cpp` (changed); `ui/src/kiwami/romm/download.h`,
  `download.cpp` (new); `kiwami/tests/romm-server.py`, `kiwami/tests/romm/library.json`, `kiwami/tests/romm-helper.sh`,
  `kiwami/tests/units.cpp` (changed)
- **Change**:
  - **`platforms`**: stdout lines `<id>\t<slug>\t<system>\t<rom_count>\t<name>`, for the platforms that map to the nine
    systems.
  - **`roms <platform id> <offset> [search]`**: `total <n>`, then 50 lines
    `<id>\t<size>\t<on card 0|1>\t<fs_name>\t<name>`. On card means listed in `games`, or a file or folder of that name
    in the system's folder on either card.
  - **`download <rom id>`**: detached; one download at a time (`$RUN/romm-download.lock`, exits 3 when held).
    - **The target**: `romm_target`. `sd2` means `$RUN/sd2/roms` names a folder (the library sees it). Otherwise it
      is the main card, with status `running ...\tmain card: the second card isn't in`.
    - **Free space**: `statvfs` against the size (twice it for a game of several files) first; short of it,
      `error download space <need> <free>` and nothing written.
    - **`awake.pid`**: written when absent or naming a dead process, and removed at the end only if it still names
      this one.
    - **Single file**: `GET /api/roms/<id>/content/<fs_name>` to `<roms>/<folder>/.romm-<id>.part`. A `.part` already
      there resumes with `Range: bytes=<size>-` (206 appended, 200 started over). Then it is renamed to `<fs_name>`.
      The download gives up after 30 s of under 1 KB/s.
    - **Several files**: the zip to `.romm-<id>.zip.part`, then `unzip -q -o` into `.romm-<id>.dir` and that renamed
      to `<fs_name>`. A failed unpack keeps the zip and says so.
    - The id is recorded in `games`. The progress is `running <bytes> <total>\t<system>\t<name>`, then
      `done download <id> <path>`.
  - **`bios`**: detached, the same download lock.
    - `GET /api/firmware?platform_id=<id>` for each mapped platform, then each file to `/data/bios/<file_name>` through
      `.part`.
    - A file already there with the same MD5 is skipped; one with another MD5 is renamed `.bak` first.
    - `done bios <fetched> <skipped>`.
  - **`romm-server.py`**: `GET /api/roms/<id>/content/<name>` with `Range` (206), a zip for a ROM of several files;
    `GET /api/firmware` and its content; `KIWAMI_ROMM_CUT=<bytes>` closing a download after that many bytes; a
    platform's search (`search_term`) and paging (`offset`, `limit`).
- **Tests**:
  - **`romm-helper.sh`**:
    - `platforms` (only the mapped ones);
    - `roms` paging and search, and the on-card mark;
    - a download to each card; the second card absent, so the main card is used and the status says so;
    - a game of several files unpacked into its folder;
    - a cut download resumed (206), and started over (200);
    - not enough space (a small `tmpfs`, or `KIWAMI_ROMM_FREE` for the free-space figure);
    - `awake.pid` taken when free, left alone when another live pid has it, and removed;
    - BIOS fetched, skipped when the same, the different one kept as `.bak`;
    - the library's extension filter never listing `.part` or `.romm-*`.
  - **`units.cpp`**: the ROM list and firmware parsers.
- **Commit**: `RomM: RomM's games listed and downloaded to either card, and its BIOS files into /data/bios`

### 8. Settings: Games, Downloads to and BIOS

- **Files**: `ui/src/kiwami/kiwami.h`, `kiwami.cpp`, `romm.h` (changed); `ui/src/kiwami/romm-games.cpp`,
  `ui/src/kiwami/romm-bios.cpp` (new); `kiwami/tests/romm-ui.sh` (changed)
- **Change**:
  - **The rows**:
    - `kiwami.romm.games` (a page);
    - `romm_target`, a choice `{ "Main card", "Second card" }` / `{ "main", "sd2" }`, added only when
      `$RUN/sd2/slot` is there, as upstream's Second SD card row;
    - `kiwami.romm.bios` (a page).

    The group's log line lists them.
  - **`romm-games.cpp`**: the platforms, with their counts, from `romm platforms` (a job, run when the page opens).
    - A on one lists its games, 50 a page, from `romm roms`. L/R or the last row change the page; Y searches with the
      keyboard. A game on a card is marked.
    - A on a game starts `romm download <id>`, then the progress shows on the top panel (the status every 0.5 s).
      When it is done, the library is read again (`libChanged`, as the scrape's end does).
    - With a download running, another A says so.
  - **`romm-bios.cpp`**: A starts `romm bios`; the progress, then fetched and skipped.
- **Tests**: **`romm-ui.sh`**:
  - the platforms page; a platform's games, its second page, a search;
  - a download with its progress shot, then the game in the library (its `lib` log line), marked on the card;
  - `romm_target` shown with a slot (`KIWAMI_RUN/sd2/slot` made) and not without;
  - the fallback said on the page;
  - BIOS;
  - shots at both sizes.
- **Commit**: `RomM: Games in Settings, RomM's platforms and games downloaded from the menu, and its BIOS files`

### 9. Final check

- `make test`, `make -C ui`, `make -C system/bananad CFLAGS="-O2 -Werror"`, `kiwami/tests/ci.sh .buildroot`.
- The fork's tests in `kiwami/tests/`: `romm-helper.sh`, `romm-game.sh`, `romm-ui.sh` and every earlier one.
- `make rgds-plus` (and `make rgds`): the image builds; `/usr/libexec/kiwami/romm` is in it, under 1 MB; the image's
  `systems.conf` has the nine `run` lines wrapped.
- `tools/qemu-boot.sh output/rgds/images/banana-rgds.img.gz`: it boots; `romm check` against a `romm-server.py` on the
  host (QEMU's user network, `10.0.2.2`) answers `ok 5.4.1`; a GBA game's wrapped start logs `romm-game` and the
  launcher.
- The hooks: `git diff --stat main...HEAD` and `git grep -n 'kiwami:'` (the commands in `docs/FORK.md`) agree, and this
  feature added none.
- With the fork's options off, `make test`'s renders in `ui/tests/out/` match `main`'s.
- `kiwami/DEPENDENCIES.md` lists RomM's API and libcurl.
- `docs/FORK.md` updated (step 4); `docs/ARCHITECTURE.md` and `CLAUDE.md` unchanged (nothing of upstream's changes).
- The manual tests below all passed.
- Every acceptance criterion is met: each is listed by at least one automated or manual test, and those passed.

## Automated tests

| Test | Where | What it checks | Criteria |
|---|---|---|---|
| Pairing, version, log out | `kiwami/tests/romm-helper.sh` (step 1) + `romm-server.py` | `check` per version, down, unpaired, revoked; `pair` approved, denied, expired, replaced; `logout` up and down, other lines kept; no token anywhere; log rotation | A5, A6, A7, A8, A25 |
| Parsers and the card | `kiwami/tests/units.cpp` (steps 1, 2, 7) | RomM's replies, `versionOk`, server normalisation, `accounts.conf` edits, `systems.conf`, cores and save paths, name matching | A4, A6, A9, A16 |
| Saves around a game | `kiwami/tests/romm-helper.sh` (step 2) | Pull and push per core, the web player's save, guard, no-op, conflict, 409, delete, DS both ways, down and stall within 3 s, pending, not on RomM, second card, lock | A9-A13, A15, A16, A20 |
| Sync now and resolve | `kiwami/tests/romm-helper.sh` (step 3) | Both cards, counts, `last`, busy, pending first, resolve both ways, down | A11, A19 |
| The wrapper | `kiwami/tests/romm-game.sh` (step 4) | `exec` when off; order and status when on; `TERM` not passed on, then pending; stdin; odd names | A3, A10, A14 |
| The image's systems.conf | `kiwami/tests/ci.sh` (step 4) | Nine lines wrapped once, others untouched, idempotent, a missing `run` stops the build; every shelf starts its launcher, the game panel kept | A18 |
| The group and Account | `kiwami/tests/romm-ui.sh` (step 5) | Grid, logo, rows, nothing run on opening, server typed, pairing by QR, versions, revoked, log out, bench, no token | A1, A2, A4-A8, A24 |
| RetroAchievements' grid kept | `kiwami/tests/ra-ui.sh` (step 5) | With `KIWAMI_ROMM_GROUP=0`, every earlier check unchanged | A1 |
| Sync now page | `kiwami/tests/romm-ui.sh` (step 6) | Progress, result, row value, conflicts resolved, down, revoked, busy | A11, A17, A19 |
| Downloads and BIOS | `kiwami/tests/romm-helper.sh` (step 7) | Lists, paging, search, both cards, fallback, unpack, resume, space, `awake.pid`, BIOS skip and `.bak` | A20-A23 |
| Games and BIOS pages | `kiwami/tests/romm-ui.sh` (step 8) | Platforms, games, search, download and library, Downloads to row, fallback, BIOS | A21, A22, A23, A25 |
| Upstream's look | `make test` (every step) | Renders unchanged with the fork's options off | A1 |

## Manual tests

On the RG DS Plus, with an image from this branch (`make rgds-plus`, flashed or as an update), Wi-Fi joined, and the
user's RomM 5.4.1 reachable on the LAN and over Tailscale. Logs are fetched over SSH/SFTP (Settings > Network > SSH) or
from the card on a computer (`banana/logs/` on the card is `/data/banana/logs/` on the device). The logs are:

- `/data/banana/logs/romm.log`: every request, with its time and result;
- `/data/banana/logs/banana-ui.log`: the `starting` lines and the `kiwami:` and `romm:` lines;
- the card's `banana/romm/` folder (`pending`, `conflicts`, `last`, `games`);
- for a RetroArch game, the file listing of `/data/saves/<core>/` and `/data/states/<core>/` (`ls -l --full-time`).

| # | Board | Steps | Expected | Logs to collect | Criteria | Outcome |
|---|---|---|---|---|---|---|
| M1 | rgds-plus | Settings > RomM: Server `http://<LAN address>:<port>`, Pair, scan the QR on a phone, approve. Again with the Tailscale address (`100.x.y.z`) after Log out | Paired with 5.4.1 both times; Log out removes the device from RomM's devices page | `romm.log`, `banana-ui.log`, `accounts.conf`'s keys (names only) | A4, A5, A7, A8 | not run |
| M2 | rgds-plus | RomM on, Second screen on. A GBA game with a save: play, save in game, quit. In RomM's web page, check the save. Play it in RomM's web player, save there. Start it again on the handheld | The game panel shows on the other screen (the wrapper kept it); the save is in RomM after the quit, the menu back at once; the handheld starts from the web player's save, the old one as `.bak` | `romm.log`, `banana-ui.log`, `/data/saves/gpSP/` listing | A9, A10 | not run |
| M3 | rgds-plus | M2 with `core.gba = mgba`; then a SNES and a Mega Drive game | The save in the chosen core's folder; each system's save loads in RomM's web player and back | as M2, the cores' folders | A9, A10 | not run |
| M4 | rgds-plus | A DS game under DraStic: save, quit. Switch it to BananaDS (X on the game), start | BananaDS has the save; RomM has the raw `.sav` (its size); after a save in BananaDS, DraStic finds it | `romm.log`, `/data/saves/nds/`, `/data/drastic/backup/` listings | A15 | not run |
| M5 | rgds-plus | Wi-Fi off: play a game, save, quit. Change the same game's save in the web player. Wi-Fi on, Sync now | One conflict listed; "Take RomM's" puts RomM's in place, the card's as `.bak`; another time "Keep this card's" | `romm.log`, `banana/romm/conflicts` before and after | A11, A13, A19 | not run |
| M6 | rgds-plus | Quick resume on: play a RetroArch game, quit (the auto state written). Save newer in the web player. Start on the handheld | The game starts from the pulled save, not the resume state (`.state.auto.bak` there) | `romm.log`, `/data/states/<core>/` listing | A9 | not run |
| M7 | rgds-plus | Close the lid in the menu, open it, start a game at once | The game starts within 3 s more than usual; the pull logged as no connection; the push after the game goes | `romm.log` (times), `banana-ui.log` | A13 | not run |
| M8 | rgds-plus | Time a GBA game's start and the menu's return after a quit, with RomM off and on (LAN), five times each | On: start at most ~0.2 s longer; return the same as off | `banana-ui.log` (its `starting` and back lines' times), `romm.log` | A10, A24 | not run |
| M9 | rgds-plus | Hold the power button while playing (shutdown), boot, Sync now | The shutdown isn't held up past its 5 s; after the boot the pending push goes with Sync now | `romm.log`, `banana/romm/pending` before the sync | A14, A19 | not run |
| M10 | rgds-plus | Games: download a GBA game to the second card, a PS1 game of several files to the main card; close the lid during a large download, open it, download it again; take the second card out, download | Games in the library; the multi-file game in its folder and starting; the cut download resumes; without the card it goes to the main card and the page says so | `romm.log`, `banana-ui.log`, the folders' listings | A21, A22 | not run |
| M11 | rgds-plus | BIOS, twice | Files in `/data/bios`; the second time all skipped | `romm.log`, `/data/bios` listing | A23 | not run |
| M12 | rgds-plus | A game on the second card that RomM has: play, save, quit; Sync now. A game RomM doesn't have | The save in RomM, from `/data/saves`; the other skipped and counted | `romm.log`, `banana/romm/last` | A16, A19 | not run |

Notes from the runs (what the logs showed, what was fixed):

## Deviations

Where the implementation turned out different from this plan, why, and what changed (dated).

## Progress

A step is marked done in its own commit, so the commit column holds the subject (a commit can't carry its own hash).

| Step | Status | Commit |
|---|---|---|
| 1. The helper, its package, and pairing | todo | |
| 2. A game's save: its RomM id, the pull before it and the push after | todo | |
| 3. Sync now and the conflicts resolved | todo | |
| 4. The wrapper around the games, put into the image's systems.conf | todo | |
| 5. Settings: the RomM group and its Account page | todo | |
| 6. Settings: Sync now and the conflicts | todo | |
| 7. The helper's games, downloads and BIOS | todo | |
| 8. Settings: Games, Downloads to and BIOS | todo | |
| 9. Final check | todo | |
