# 0019 RetroAchievements: plan

Issue: #19 · Branch: `k/0019-retroachievements` · Investigation: [investigation.md](investigation.md) · Status: approved

## Summary

Settings gets a seventh group, RetroAchievements, after System. The group grid stays three tiles wide and shows two rows
at a time, scrolling one row to reach the third. The tile shows RetroAchievements' logo, pixelated (step 13; step 2
drew a trophy). The group has four rows:

- **Account**: sign in (name, password, checked with the server, the token kept); sign out, which forgets the password
  and token everywhere Banana and the emulators keep them; and a Google-like list of signed-out accounts with their
  names and avatars. A on one asks its password again, X twice removes it.
- **Stats**: hardcore and softcore points, unread messages and avatar, then progress for nine systems. They are
  fetched when the page opens or on Y, and kept on the card for offline.
- **Games on this card**: the games Banana recorded progress for, under the menu's titles. A on one shows the game's
  achievements, locked and unlocked, with badges. The game is identified by hashing its ROM as RetroArch does
  (rcheevos), `.zip` and `.7z` cartridges unpacked first (the LZMA SDK's 7z decoder), PlayStation `.chd` read through
  libchdr.
- **Mode**: Softcore/Hardcore, the same setting as upstream's row in Other systems.

The game panel also takes the token from `accounts.conf`. Today it reads it from `retroarch.cfg`, which never has it,
so its achievement list never loads.

The approach is the investigation's option 1:

- the fork's code goes in `ui/src/kiwami/`, its vendored C libraries in `ui/src/kiwami/third_party/`;
- the fork's requests go straight to `retroachievements.org`'s `dorequest.php` with libcurl, `dlopen()`ed as upstream
  does;
- there are six hooks into upstream's files:
  - `ui/src/settings.cpp`: the group's rows in `build()`, the fork's icon in `icon()`, the grid's scrolling (in
    `pressGroups`, the groups' touch code and `renderGroups`), and the panel's token in `gameOpen`;
  - `ui/src/main.cpp`: the library for the fork's pages;
  - `ui/Makefile`: an include of the fork's C build.

### Refinements over the investigation

- **libchdr is pinned at its master `47d7941`**, not PCSX-ReARMed's `9ccd3a7`. Its crafted-CHD fix matters here, and
  the user chose it (the investigation's question 15).
- **The LZMA SDK is pinned at 26.02**, 7-Zip's tag `26.02` (26 Jun 2026). That is the version libchdr bundles, so one
  `LzmaDec.c` serves both. The investigation left the choice between 26.02 and RetroArch's 19.00 to planning; 26.02
  opens everything 19.00 does.
- **The token for an account typed in the setup is fetched when the Account or Stats page opens,** not when Settings
  opens. Nothing goes on the network until one of the fork's pages is opened.
- **The fork reads its card files relative to Settings' data folder:**
  - `<data>/../retroarch/retroarch.cfg`
  - `<data>/../superdrastic/ra.token`
  - `<data>/../bananads/ra.token`

  On the device these are the paths the investigation names (data is `/data/banana`), and the headless tests can use
  a fixture card. The scrub that sign-out runs first is a command that `KIWAMI_RA_SCRUB` can replace for the tests.
- **The group's rows are `Account`, `Stats`, `Games on this card` and `Mode`.** Account is first, so its icon (the
  logo) is the tile's, and the tile's hint reads "account, stats".
- **No setting turns the group off**, as the investigation decided. What a user of `main` would notice:
  - a seventh tile below the first two rows;
  - RIGHT on System and DOWN on the second row reach it, instead of wrapping to the first row;
  - L/R in a list reach its list after System's.

  Upstream's first two rows look the same, pixel for pixel (A1, A2). The game panel's token is a bug fix.

## Acceptance criteria

The group and the grid

- **A1**: given Settings opens on its groups, then the grid shows the first two rows exactly as `main` does (six tiles,
  the same pixels), and the RetroAchievements tile, with RetroAchievements' logo, is the seventh.
  - RIGHT on System, or DOWN on a second-row tile, shows rows 2 and 3 with the cursor on RetroAchievements.
  - UP from RetroAchievements to the second row keeps rows 2 and 3; UP again to the first row shows rows 1 and 2.
  - DOWN on the third row goes to the first row's tile in the same column (Display & sound, from RetroAchievements).
  - UP on a first-row tile goes to the third row: the tile in the same column, or RetroAchievements where that column
    has none.
  - A tap on a tile that shows selects it, a second tap opens it, and a tile that doesn't show can't be tapped.
- **A2**: given the grid has six groups (the fork's group not built), then its keys, touch and render are upstream's,
  bit for bit. Upstream's navigation by R presses from the first group still reaches the same groups.
- **A3**: given the group's list, when Mode is changed, then `retroarch.cfg`'s `cheevos_hardcore_mode_enable` becomes
  `"true"` or `"false"`, and upstream's row in Other systems shows the same value (and the other way round).

The account

- **A4**: given no account, when the user signs in on the Account page (name, then password) and the server accepts,
  then:
  - `accounts.conf` has `ra_user`, `ra_password` and `ra_retroarch_token` (the token from `login2`);
  - the page shows the name, both kinds of points and the avatar (downloaded to `cache/ra-avatars/`);
  - a toast says "Signed in as <name>".
- **A5**: given an account, when a sign-in is refused (wrong name or password), then `accounts.conf` is as it was and
  the page says "Wrong account name or password".
- **A6**: given no connection (no answer, a timeout), when the user signs in, then `ra_user` and `ra_password` are
  kept with no token, and the page says "Not checked: no connection". The next time the Account or Stats page opens
  with the server reachable, the token is fetched and kept.
- **A7**: given an account, when Sign out is pressed twice with A, then, in this order, the scrub runs and:
  - `ra_user`, `ra_password` and `ra_retroarch_token` are gone from `accounts.conf`, and its other lines stay;
  - `retroarch.cfg` has `cheevos_enable = "false"` and `cheevos_username = ""`;
  - both `ra.token` files are deleted;
  - the account is first in the remembered list, with its avatar.

  A single A, then any other key, signs nothing out.
- **A8**: given remembered accounts, when A is pressed on one, then the keyboard asks its password with the name
  already filled in (A4/A5/A6 follow). When X is pressed twice on one, it is removed with its avatar and cached stats;
  X once, then any other key, removes nothing.
- **A9**: given an account, when the user signs in to another (New account, or a remembered one), then the current
  account goes to the remembered list first.
- **A10**: given the account in `accounts.conf` changed since the fork last saw it (the setup's step, a login in
  RetroArch's own menu), when Settings opens, then the old account goes to the remembered list and the new one is
  shown. A fresh card (no `accounts.conf`, no `ra-accounts`) shows "Not signed in" and an empty list.

The stats

- **A11**: given an account, when the Stats page opens, then it shows:
  - hardcore and softcore points, unread messages and the avatar (`login2`);
  - for each of DS, Game Boy Advance, Game Boy Color, Game Boy, NES, Super Nintendo, Mega Drive, PlayStation and
    Nintendo 64 (`allprogress`): games started, completed (every achievement unlocked) and mastered (every one in
    hardcore), and achievements unlocked. A system with none says "No games started".
- **A12**: given stats were fetched once, when the page opens with no connection, then the cached stats show with
  "as of <date>". Y fetches again. No request is made but when a page opens or on Y.
- **A13**: given no account, then the Stats page says to sign in first, and makes no request.

The games and a game's achievements

- **A14**: given games with recorded progress (play stats), when Games on this card opens, then it lists them with the
  menu's titles (renamed, gamelist or cleaned file name), their system and unlocked/total, second-card games included.
  With none, it says so, and says that RetroArch games appear after a game played with Second screen on.
- **A15**: given a game in that list, when A is pressed, then the game is hashed as RetroArch does and its ID resolved
  (`gameid`), both cached in `cache/ra-hashes` by path, size and modification time. The page lists the game's core
  achievements (title, description, points, badge), each unlocked one marked hardcore or softcore, and the
  unlocked/total count.

  This holds for:
  - cartridges: plain, `.zip`, `.7z` (LZMA, LZMA2), an NES with its 16-byte header, a SNES with a 512-byte copier
    header, an N64 in each byte order;
  - `.nds`;
  - PlayStation `.cue`/`.bin` (MODE1/2048 and MODE2/2352), `.iso`, `.chd` and an `.m3u` naming one of them.

  A cartridge archive holding several files is hashed on its first member in name order whose extension the system
  lists.
- **A16**: given a game that can't be identified (a zipped DS game, a PlayStation `.pbp`, `.img` or `.mdf`, a
  cartridge over 64 MB unpacked, an archive with no game in it, a damaged `.chd`, a hash the server doesn't know),
  then the page says why. banana-ui doesn't crash, and the menu stays responsive while a hash runs.

The panel, secrets, footprint, devices

- **A17**: given `retroarch.cfg` without a token and `accounts.conf` with `ra_user` (any case) and
  `ra_retroarch_token`, when a RetroArch game opens the game panel, then the panel uses that token. On the device, its
  achievement list loads.
- **A18**: banana-ui's log never has a password or token, nor a request's `p=` or `t=`. The fork's request lines name
  the request, the status and the time.
- **A19**: with the group's pages closed, the fork runs no thread and makes no request, and the menu's frame time is
  unchanged (`bench`). banana-ui's growth from the three libraries is measured and recorded (the investigation
  estimated 300-400 KB).
- **A20**: the vendored libraries are byte for byte their pinned versions (checked in CI), and each is in
  `kiwami/DEPENDENCIES.md` with its version and licence.
- **A21**: at 640x480 and 1024x768 the fork's pages render, and touch works on the bottom panel's rows.
- **A22**: given the handheld sleeps while a request runs, then after resume the page shows the cached stats (or the
  failure), and nothing is left hanging.

## Risks

- **Performance and footprint.**
  - Requests and hashing run only on a page's open, Y, or a game's A, each on one thread at a time, and their results
    are taken in by `update()` (A19). `wake()` asks for 100 ms only while one is in flight, as Tailscale's helper does.
  - Hashing reads up to 64 MB (N64) into memory, unpacks a `.7z` first, and reads only the boot file of a disc. It is
    once per ROM, cached.
  - banana-ui's growth is measured in step 14 (A19). A bench run in `ra-ui.sh` checks the groups' frame time against
    a run with the fork's group absent.
  - Nothing is added to the rootfs but banana-ui's size. libcurl and zlib are already in the image (zlib is
    banana-ui's dependency already).
- **Conflicts with upstream.**
  - `ui/src/settings.cpp` changes often and heavily. The grid hook modifies seven of upstream's lines in
    `renderGroups`, the touch code and `pressGroups`, and adds two. Upstream restyling the grid conflicts there
    visibly (that is wanted: a copy of the grid in the fork would silently drift).
  - If upstream adds a seventh group or its own RetroAchievements menu (its open request), the merge reconsiders the
    fork's group.
  - The `gameOpen` line conflicts if upstream fixes the token, and then goes away.
  - `ui/src/main.cpp` gets two lines, `ui/Makefile` one.
  - The final check runs `docs/FORK.md`'s two commands and the comparison of renders with `main`'s.
- **The live server.** `allprogress`, `gameid`, `unlocks` and `AvatarUrl` are written from rcheevos 12.5 and are
  tested against a stand-in, not the live server. M2-M5 check them on the device. RetroAchievements rate-limits; a
  stats refresh is about 11 requests and is only ever made on an open or Y.
- **Hash mismatches.** If the fork's hash differed from RetroArch's, a game would get another game's list or none.
  rcheevos' own hash code is vendored unchanged, and the fork only feeds it bytes. The tests check every format
  against independently computed MD5s and against each other (iso = cue = chd, plain = zip = 7z). M5 compares with the
  game panel's identification on the device.
- **Crafted files.** A damaged archive or `.chd` must not take the menu down. libchdr's pin has its crash fixes, the
  7z decoder checks its input, the unpacked size is capped at 64 MB before anything is allocated, and the tests feed
  each reader a truncated file (A16).
- **Credentials.** Nothing new is kept beyond upstream's (`accounts.conf`, which file sharing hides). The remembered
  list holds names only. Sign-out removes more than anything does today (A7). Logs never carry secrets (A18,
  `ra-ui.sh` greps the log for the stand-in's password and token).
- **Boards.** Settings draws on a 640x480 grid scaled by `X()`, and both sizes are rendered headless (A21). The
  second card exists only on the RG DS Plus; the list comes from the library, which already handles its absence.

## Steps

Each step is one commit and leaves the tree building with `make test` and `kiwami/tests/ci.sh .buildroot` passing.

### 1. The fork's Settings pages, routed by which one is open

- **Files**:
  - `ui/src/kiwami/kiwami.h` (changed)
  - `ui/src/kiwami/kiwami.cpp` (changed)
  - `ui/src/kiwami/tailscale.cpp` (changed)
  - `.workspace/0019-retroachievements/plan.md` (changed)
- **Change**: today every `KIWAMI` page hook goes to Tailscale's code. `Settings::Kiwami` gets
  `enum class Page { NONE, TAILSCALE } pageOpen = Page::NONE;` (the RetroAchievements pages join in later steps):
  - `open(key)` sets it from the key's page;
  - `press`, `touch`, `render`, `entryHelp` and `entered` dispatch on it;
  - Tailscale's functions are renamed `tsOpen`, `tsPress`, `tsTouch`, `tsRender`, `tsEntryHelp` and `tsEntered`;
  - `opened`, `update` and `wake` call each page's own part (Tailscale's `tsOpened`, `tsUpdate`, `tsWake`).

  No upstream file changes, and the hooks' signatures stay as they are. No behaviour changes: Tailscale's page logs and
  renders as before.
- **Tests**: `kiwami/tests/tailscale-ui.sh` and `kiwami/tests/cores-ui.sh` pass unchanged (their shots and log lines).
  `make test`, `kiwami/tests/ci.sh .buildroot`.
- **Commit**: `RetroAchievements: the fork's Settings pages routed by which one is open`

### 2. The RetroAchievements group, the scrolling grid and the trophy, with Mode

- **Files**:
  - `ui/src/settings.cpp` (hook)
  - `ui/src/kiwami/kiwami.h` (changed)
  - `ui/src/kiwami/kiwami.cpp` (changed)
  - `ui/src/kiwami/ra-icon.cpp` (new)
  - `ui/src/kiwami/grid.h`, `ui/src/kiwami/grid.cpp` (new: the grid's arithmetic, see *Deviations*)
  - `kiwami/tests/ra-ui.sh` (new)
  - `kiwami/tests/units.cpp` (changed)
  - `kiwami/tests/cores-ui.sh` (changed: its `rows in` lines gain RetroAchievements')
  - `kiwami/tests/ci.sh` (changed)
  - `.workspace/0019-retroachievements/plan.md` (changed)
- **Change**:
  - **The rows.** One added line after the Shut down row (`settings.cpp:506`, the end of `build()`):
    ```cpp
    		Kiwami::rows(*this, "RetroAchievements");	// kiwami: the RetroAchievements group, after System
    ```
    `Kiwami::rows` gets a `"RetroAchievements"` branch that adds, with section `"RetroAchievements"`:
    - `Account` (key `kiwami.ra.account`, `PAGE`, icon `kiwami.trophy`);
    - `Mode` (key `cheevos_hardcore_mode_enable`, file `/data/retroarch/retroarch.cfg`, `CHOICE`
      `{ "Softcore", "Hardcore" }` / `{ "false", "true" }`, icon `gamepad`).

    Mode's help is upstream's row's text, adding "the same setting as Other systems > RetroAchievements". Upstream's
    generic `load`/`save` handle it (`settings.cpp:580,733-740`), as they do upstream's row (`:447-448`). Upstream
    reads a row's value only when Settings opens, so `Kiwami::update` keeps the two rows in step: `modeAt` is their
    value when Settings opened, and when one row's value differs from it, the other row takes it (logged
    `banana-ui: kiwami: Mode <Softcore|Hardcore> in both rows`). Account's
    page is empty in this step: A on it logs `ra: account page (to come)` and returns `NONE`. Step 5 fills it. The rows'
    log line follows the existing pattern:
    `banana-ui: kiwami: rows in RetroAchievements: kiwami.ra.account cheevos_hardcore_mode_enable`.
  - **The icon.** One added line in `Settings::icon` after the PNG is decoded (`settings.cpp:1201`):
    ```cpp
    			if (b.empty() && kw) b = kw->icon(name);	// kiwami: the fork's own icons (the trophy)
    ```
    `Kiwami::icon(name)` returns, for `kiwami.trophy`, a 32x32 RGBA bitmap drawn from a pixel map in `ra-icon.cpp`, in
    the colours of `ui/assets/rnds/icons/` (sampled from `gear.png`'s and `fav.png`'s palette), and an empty bitmap for
    anything else. `iconCentred` goes through `icon`, so the tile, the groups' top panel and the list's top panel all
    get it.
  - **The grid.** The arithmetic is in `kiwami::gridMove(g, n, up)` and `kiwami::gridTop(row0, gsel, n)` (`grid.h`),
    which `units.cpp` links without Settings; `Kiwami::gridMove` calls the first, and `Kiwami::gridTop` the second. The
    first row shown, `r0`, comes from `int Kiwami::gridTop(int gsel, int n)`, which keeps `row0` and returns it:
    - with `rows = (n + 2) / 3` and `r = gsel / 3`: `row0 = clamp(row0, r - 1, r)`, then `clamp(row0, 0, max(0, rows - 2))`;
    - it logs `banana-ui: kiwami: groups from row <r0>` when `row0` changes;
    - with six groups it is always 0.

    In upstream's code, each modified line carries `// kiwami:`:
    - `pressGroups` (`:972`), one line modified:
      ```cpp
      		case Key::UP: case Key::DOWN: gsel = kw ? kw->gridMove(gsel, n, k == Key::UP) : (gsel + (k == Key::UP ? n - 3 : 3)) % n; return MOVED;	// kiwami: a last row that isn't full
      ```
      `Kiwami::gridMove`:
      - DOWN: `g + 3` while that exists; else, from a row above the last, the last tile; else (on the last row) the
        first row's tile in the same column (`g % 3`).
      - UP: `g - 3` while that exists; else the last row's tile in the same column, or the last tile when that column
        is empty.

      With `n % 3 == 0` this equals upstream's `(g ± 3) % n` for every `g` (checked by a unit test).
    - the groups' touch (`:1025-1027`): one added line, `int r0 = kw ? kw->gridTop(gsel, (int)groups.size()) : 0;`.
      The loop becomes `for (int g = 3 * r0; g < (int)groups.size() && g < 3 * r0 + 6; g++)`, and `y` becomes
      `82 + (g / 3 - r0) * 166`.
    - `renderGroups` (`:1371-1376`): the same added `r0` line, and the same loop bounds and `y` in both loops. The pick
      box is at `82 + std::min(1.0f, std::max(0.0f, aRow - r0)) * 166` (clamped to the two rows shown, so a wrap
      doesn't draw it over the chips). The `on` test keeps the absolute rows.

    With `r0 == 0` every expression is upstream's. The animation (`animating`, `advance`) is unchanged: the rows jump
    and the pick slides, as upstream's pick does.
- **Tests**:
  - `kiwami/tests/ra-ui.sh` (new; headless at 640x480 and 1024x768, on `ui/tests/fixture.sh`'s card; run by `ci.sh`
    when `ui/banana-ui` is built, like the other UI tests):
    - the log has the rows' line;
    - `press start; shot g0` is the same image as the same run with `KIWAMI_RA_GROUP=0`, which makes
      `Kiwami::rows` skip the group: six groups, the fork's grid code at `r0 = 0`. That checks A1's first rows and A2
      (the second run is A2's six-group grid);
    - from Settings: R×5 then RIGHT logs `groups from row 1`, and `shot g7` (the trophy tile selected) differs from
      `g0` in the bottom image;
    - UP×2 from there returns to `groups from row 0`;
    - DOWN on Network (row 2) reaches RetroAchievements (`press a` opens its list, which has `Account` and `Mode` on
      the bottom panel, shot `ra-list`);
    - a tap on a hidden tile's position does nothing, and a tap on the trophy tile selects it;
    - Mode: RIGHT on it writes `cheevos_hardcore_mode_enable = "true"` to the test's `retroarch.cfg`. Upstream
      hardcodes `/data/retroarch/retroarch.cfg`, so the test runs with that path only when `/data` is writable (it
      isn't on CI) and otherwise checks the row's value in the shot. A3's cross-check with upstream's row is M1.
  - `kiwami/tests/units.cpp`: `gridMove` against upstream's formula for `n = 3, 6, 9`, every `g`; `gridMove` and
    `gridTop` for `n = 7, 8` (each case above).
  - `make test` (upstream's `run.sh`, including its R-count navigation and `settings` shot, unchanged),
    `kiwami/tests/ci.sh .buildroot`.
- **Commit**: `RetroAchievements: a group in Settings, the grid scrolling to its row, with Mode`

### 3. RetroAchievements' requests, and a stand-in server for the tests

- **Files**:
  - `ui/src/kiwami/raclient.h` (new)
  - `ui/src/kiwami/raclient.cpp` (new)
  - `kiwami/tests/ra-server.py` (new)
  - `kiwami/tests/ra/` (new: canned replies `*.json`, two small PNGs)
  - `kiwami/tests/units.cpp` (changed)
  - `kiwami/tests/ci.sh` (changed)
  - `.workspace/0019-retroachievements/plan.md` (changed)
- **Change**: `namespace banana::kiwami::ra`, no UI.
  - **Transport.** libcurl, `dlopen("libcurl.so.4")` as upstream's (`settings.cpp:2863-2885`), its own copy (about 60
    lines). `post(fields)` and `fetch(url, path)` (to `path + ".part"`, then renamed). Both have a 20 s timeout and
    `NOSIGNAL`, with the user agent `BananaOS/0.2 rcheevos` as upstream's panel uses. The base URLs are
    `KIWAMI_RA_URL` (default `https://retroachievements.org`) and `KIWAMI_RA_MEDIA` (default
    `https://media.retroachievements.org`), for the tests.
  - **Each request** logs `banana-ui: ra: <r> <status|no connection> <ms> ms`, nothing else of it (A18).
  - **Requests and parsed replies**, each with the fork's `json` reader:
    - `login2(user, password | token)` → `{ ok, user, token, score, softScore, messages, avatarUrl, error, refused }`.
      `refused` is the server saying no (`Success: false`, or 401); otherwise it's "no connection".
    - `allProgress(user, token, console)` → per game `{ id, total, unlocked, unlockedHardcore }` and the per-system
      sums: started, completed (`unlocked == total`), mastered (`unlockedHardcore == total`), unlocked.
    - `gameId(hash)` → id (0 for an unknown game).
    - `patch(user, token, id)` → title, and the achievements with `Flags == 3` (core only): id, title, description,
      points, badge.
    - `unlocks(user, token, id, hardcore)` → ids.
    - `avatar(user, avatarUrl, dir)` → the file (`AvatarUrl` when given, else `<media>/UserPic/<User>.png`).
    - `badge(name, dir)` → `<media>/Badge/<name>.png` into `cache/ra-badges/`, the folder upstream's panel uses.
  - **The consoles**: `nds 18, gba 5, gbc 6, gb 4, nes 7, snes 3, megadrive 1, psx 12, n64 2`, in that order (the
    Stats page's).
  - **`ra-server.py`**, the stand-in: Python 3's `http.server` on `127.0.0.1` with a free port (it writes the port to
    the file named by its first argument, so the test knows where it is):
    - it answers `POST /dorequest.php` by its `r`, from `kiwami/tests/ra/<r>.json`. `login2` accepts
      `alice`/`pw1`/token `tok1` and `bob`/`pw2`/`tok2`, and refuses anything else with 401;
    - `KIWAMI_RA_SERVER_DOWN` makes it drop requests (no connection);
    - it serves `/UserPic/<name>.png` and `/Badge/<name>.png` from the two PNGs;
    - it appends one line a request to the log file named by its second argument: `r`, `u`, whether `p` and `t` were
      sent (never their values), `g`, `m`, `c`, `h`.
- **Tests**: `units.cpp`, against `kiwami/tests/ra/`:
  - each parser on its reply, plus a failure, a refusal, malformed JSON and an empty body;
  - the per-system sums on a reply with a completed, a mastered and a started game;
  - `patch` keeps `Flags 3` and drops `5`.

  `ci.sh`: `units` built with `raclient.cpp`, and `python3 -m py_compile kiwami/tests/ra-server.py`. `make test`,
  `kiwami/tests/ci.sh .buildroot`.
- **Commit**: `RetroAchievements: the server's requests, and a stand-in server for the tests`

### 4. The account on the card: signing in, out, and the remembered accounts

- **Files**:
  - `ui/src/kiwami/rastore.h` (new)
  - `ui/src/kiwami/rastore.cpp` (new)
  - `kiwami/tests/units.cpp` (changed)
  - `kiwami/tests/ci.sh` (changed)
  - `.workspace/0019-retroachievements/plan.md` (changed)
- **Change**: `namespace banana::kiwami::ra`, file work only, testable without Settings. `Card` holds the paths from
  Settings' data folder (`<data>/accounts.conf`, `<data>/ra-accounts`, `<data>/cache/ra-avatars`,
  `<data>/cache/ra-stats`, `<data>/../retroarch/retroarch.cfg`, and the two `ra.token` files at
  `<data>/../superdrastic/` and `<data>/../bananads/`).
  - **`accounts.conf`**: read as upstream's share script reads it (spaces round the key and value, quotes, the last
    line winning). It is written as `key=value`, replacing every line of that key or removing them, and keeping every
    other line in its order. Each write goes through a temporary file and a rename; on a write error the old file is
    kept and the error returned.
  - **`ra-accounts`**: `current\t<name>` and `remembered\t<name>` lines, most recent first, names compared without
    case. It holds no secrets.
  - **The operations**, each logging `banana-ui: ra: ...` with names only:
    - `signedIn(name, password, token)` writes the three keys (no token: no `ra_retroarch_token` line). If another
      account was current, it goes to the remembered list first (A9). The name becomes current and leaves the
      remembered list.
    - `signOut()` (A7):
      1. runs `KIWAMI_RA_SCRUB`, default `/usr/libexec/banana/ra-secrets scrub`, and waits for it. The scrub runs
         first so that it can't bring the password back afterwards;
      2. removes `ra_user`, `ra_password` and `ra_retroarch_token`;
      3. sets `cheevos_enable = "false"` and `cheevos_username = ""` in `retroarch.cfg`, but only if the file exists
         (RetroArch's format, as upstream's `save` writes it);
      4. deletes both `ra.token` files;
      5. moves current to the remembered list's front.
    - `remove(name)`: off the list, and its avatar and cached stats are deleted.
    - `adopt()` (A10): if `accounts.conf`'s `ra_user` isn't `ra-accounts`' current, the old current goes to the
      remembered list and the new name becomes current. It is called when Settings opens.
    - `needsToken()`: there is a name and a password, and no token (A6).
  - **The stats cache**: `ra-stats/<lowercased name>`, a small text file of the numbers and the fetch time.
- **Tests**: `units.cpp`, on a temporary card:
  - signing in writes the three keys and keeps an unrelated `smb_user` line;
  - a second account moves the first to the remembered list;
  - signing out runs a stand-in scrub (a script writing a marker, and checking that the keys are still there when it
    runs), removes the keys, writes `retroarch.cfg`'s two lines, deletes both `ra.token` files and remembers the name;
  - with no `retroarch.cfg`, signing out creates none;
  - removing deletes the avatar and the stats;
  - `adopt()` after `accounts.conf` names another user;
  - names differing only in case are one account;
  - a read-only folder makes `signedIn` fail with the files unchanged.

  `make test`, `kiwami/tests/ci.sh .buildroot`.
- **Commit**: `RetroAchievements: the account on the card, signed in, out and remembered`

### 5. The Account page

- **Files**:
  - `ui/src/kiwami/kiwami.h` (changed)
  - `ui/src/kiwami/kiwami.cpp` (changed)
  - `ui/src/kiwami/ra.h` (new: the pages' state)
  - `ui/src/kiwami/ra-account.cpp` (new)
  - `kiwami/tests/ra-ui.sh` (changed)
  - `.workspace/0019-retroachievements/plan.md` (changed)
- **Change**: the page `RA_ACCOUNT`, in Tailscale's page's layout: rows on the bottom panel, and the selected row's
  state and help on the top one.
  - **Rows.** Signed in, the account (avatar, name, points), then Sign out. Then the remembered accounts (avatar and
    name each), then New account. Signed out, only the remembered accounts and New account.
  - **Signing in.** New account starts `ENTRY_KIWAMI` for the name, then for the password. A remembered account
    starts the password with the name in the title ("RetroAchievements password for <name>"). The sign-in runs
    `login2` on a thread, or inline when `st.sync` (headless), showing "Checking the account <name>..." as upstream's
    toast does. Then:
    - **accepted**: `signedIn(name, password, token)`, avatar fetch, toast "Signed in as <name> · <points> points"
      (A4);
    - **refused**: nothing written, "Wrong account name or password" in `st.message` and a toast (A5);
    - **no connection**: `signedIn(name, password, "")`, "Not checked: no connection" (A6).
  - **Sign out.** A arms ("Press A again to sign out"), a second A signs out. Any other key disarms (A7).
  - **Removing.** On a remembered row, X arms and a second X removes; the chip "X Remove" shows (A8).
  - **Opening.** When the page opens with `needsToken()`, `login2` runs with the password and keeps the token. The
    Account row's value in the list is the name, or "Not signed in". `Kiwami::opened()` calls `adopt()` (A10).
  - **Avatars** are decoded with `rnds::decodeFile` and uploaded once per name (dropped when the page closes); a
    missing one shows the trophy.
  - **Logging.** Its log lines (names only):
    - `ra: signed in as alice (token kept)`
    - `ra: sign in refused for bob`
    - `ra: signed in as alice, not checked: no connection`
    - `ra: signed out alice`
    - `ra: removed bob`
    - `ra: remembered: alice bob`
    - `ra: adopted bob, alice remembered`
- **Tests**: `ra-ui.sh` with `ra-server.py`, at both sizes:
  - signing in as `alice`/`pw1` by touch-typing (upstream's `key()` helper):
    - `accounts.conf` has `ra_user=alice`, `ra_password=pw1` and `ra_retroarch_token=tok1`;
    - the server's log has one `login2` with `p` and no `t`;
    - the avatar file is there;
    - shots `acct-in` (top and bottom) are not blank.
  - `bob` with a wrong password: `accounts.conf` unchanged, the message in the log.
  - with `KIWAMI_RA_SERVER_DOWN`: `ra_user` and `ra_password` kept, no token.
  - opening again with the server up: one `login2`, the token kept.
  - Sign out:
    - with one A, then B: nothing changes;
    - with A, A: on a fixture card with `retroarch.cfg`, both `ra.token` files and `KIWAMI_RA_SCRUB` pointing at a
      stand-in that records its call, the scrub was called, the keys are gone, the two `retroarch.cfg` lines are
      written, the token files are gone, and `ra-accounts` remembers `alice`.
  - A on `alice` in the list: the keyboard title names alice; signing in removes her from the list.
  - X, X on `bob` (remembered): gone with his avatar. X, then DOWN: still there.
  - a fixture `accounts.conf` naming `bob` while `ra-accounts` says `alice`: Settings opening logs the adoption.
  - a fresh card: the row says "Not signed in".
  - the log never has `pw1`, `pw2`, `tok1` or `tok2` (A18).

  `make test`, `kiwami/tests/ci.sh .buildroot`.
- **Commit**: `RetroAchievements: the Account page, signing in, out and back with remembered accounts`

### 6. The game panel takes the token Banana keeps (a bug fix)

- **Files**:
  - `ui/src/settings.cpp` (hook)
  - `ui/src/kiwami/kiwami.h` (changed)
  - `ui/src/kiwami/rastore.cpp` (changed)
  - `kiwami/tests/units.cpp` (changed)
  - `.workspace/0019-retroachievements/plan.md` (changed)
- **Change**:
  - **The failing test first.** In `units.cpp`, `testPanelToken` sets up the card as a RetroArch game leaves it:
    - `retroarch.cfg` with `cheevos_username = "Alice"` and no token (what `ra-secrets` leaves, `ra-secrets:30`);
    - `accounts.conf` with `ra_user=alice` and `ra_retroarch_token=tok1`.

    It asserts that the panel's token is `tok1`. Upstream's `gameOpen` reads only `retroarch.cfg`
    (`settings.cpp:3428`), and the panel can't be opened headless (no script verb). So the test is written against
    `Settings::Kiwami::panelToken(user, data)` in its first form, a stub that reads `retroarch.cfg` as upstream does.
    It fails with `units: FAIL ... panelToken("Alice", card) is "", not "tok1"`.
  - **The fix.** `panelToken` returns `accounts.conf`'s `ra_retroarch_token` when its `ra_user` equals the panel's user
    without case, and "" otherwise. It goes in as one added line after `settings.cpp:3428`:
    ```cpp
    		if (raw->token.empty()) raw->token = Kiwami::panelToken(raw->user, data);	// kiwami: the token Banana keeps in accounts.conf (retroarch.cfg never has it)
    ```
- **Tests**: `testPanelToken` passes. It also checks:
  - another user in `accounts.conf` → "";
  - no `accounts.conf` → "";
  - a token present in `retroarch.cfg` (an old card) is used as before, since the hook runs only when it's empty.

  On the device: M6. `make test`, `kiwami/tests/ci.sh .buildroot`.
- **Commit**: `RetroAchievements: the game panel takes the token from accounts.conf, where Banana keeps it`

### 7. The Stats page

- **Files** (no upstream file: the row comes through step 2's hook):
  - `ui/src/kiwami/kiwami.cpp` (changed)
  - `ui/src/kiwami/ra.h` (changed)
  - `ui/src/kiwami/ra-stats.cpp` (new)
  - `ui/src/kiwami/rastore.cpp` (changed)
  - `kiwami/tests/ra-ui.sh` (changed)
  - `kiwami/tests/ra/allprogress-*.json` (new)
  - `.workspace/0019-retroachievements/plan.md` (changed)
- **Change**:
  - **The row.** `Stats` (key `kiwami.ra.stats`, `PAGE`) after Account. Its value is "<hardcore> points", or blank
    when not signed in.
  - **The page.** On the top panel: avatar, name, hardcore and softcore points, unread messages, and "as of
    <date, time>" when the numbers come from the cache. On the bottom panel, nine rows in the consoles' order: the
    system's name and icon (upstream's icons `nds`, `gba`, `gbc`, `gb`, `nes`, `snes`, `md`, `psx`, `n64`),
    "<started> started · <completed> completed · <mastered> mastered", and "<unlocked> achievements", or "No games
    started".
  - **The fetch.** When the page opens (and on Y): `login2` with the token (the password when there is no token yet,
    which also keeps the token), the avatar, then `allProgress` for each console. It runs on one thread, one request
    at a time, with "Updating..." showing.
    - Once done, the cache is written (`rastore`).
    - If it fails, the cache shows with its date and "No connection: the stats as of <date>", or "No connection" with
      no cache.
    - Not signed in, the page says "Sign in on the Account page first" and makes no request (A13).
  - **Logging.** `ra: stats for alice: 9 systems` or `ra: stats for alice from the cache (as of <epoch>)`.
- **Tests**: `ra-ui.sh`:
  - signed in as alice (step 5's card): open Stats, and the server log has one `login2` with `t` and no `p`, and nine
    `allprogress` lines with `c` in the order 18 5 6 4 7 3 1 12 2. The page logs 9 systems, shot `stats`;
  - Y adds exactly one more round;
  - closing and waiting 5 s adds none (A12, A19);
  - `KIWAMI_RA_SERVER_DOWN`: the cache is logged with its time, shot `stats-cached`;
  - signed out: no request, shot `stats-none`.

  `make test`, `kiwami/tests/ci.sh .buildroot`.
- **Commit**: `RetroAchievements: the Stats page, points and progress per system, kept for offline`

### 8. The games on this card, from the library

- **Files**:
  - `ui/src/main.cpp` (hook)
  - `ui/src/kiwami/library.h` (new)
  - `ui/src/kiwami/kiwami.cpp` (changed)
  - `ui/src/kiwami/ra.h` (changed)
  - `ui/src/kiwami/ra-games.cpp` (new)
  - `kiwami/tests/ra-ui.sh` (changed)
  - `.workspace/0019-retroachievements/plan.md` (changed)
- **Change**:
  - **The hook.** `library.h` declares `namespace banana { class Library; namespace kiwami { extern Library* library; } }`,
    defined in `ra-games.cpp`. `main.cpp` gets two added lines: the include next to its others, and after `Host lib(...)`
    (`main.cpp:1008`, which both the device's loop and `runHeadless` use):
    ```cpp
    	kiwami::library = &lib;	// kiwami: the library, for Settings' RetroAchievements pages
    ```
  - **The row.** `Games on this card` (key `kiwami.ra.games`, `PAGE`) after Stats. Its value is the count.
  - **The list.** When the page opens, under `worker.uiLock`: Settings' `open`, `press` and `render` already run under
    it (`main.cpp:1283,1611-1621`). The list walks `library->shelf(i)` for every shelf that isn't a collection
    (`favorites`/`recent`/`search` false) and whose `def.id` is one of the nine systems. It takes each game with
    `achB > 0`, copying `title`, `def.name`, `path`, `rel`, `achA` and `achB`. The page works on the copy, so a rescan
    can't pull it from under the page. The list is sorted by last play (`last`, newest first).
  - **The page.** Bottom: rows with the system's icon, the title, and "<a> / <b>" with a bar like the Carts theme's.
    Top: the selected game (title, system, file name), "A shows its achievements" (step 12), and the help: "The games
    Banana has seen achievements for. RetroArch games appear after one played with Settings > Other systems > Second
    screen on." With none, the top says so (A14).
  - **Logging.** `ra: games on this card: <n>`.
- **Tests**: `ra-ui.sh`. A fixture `playstats.tsv` gives:
  - `gba/Orchard Tactics.gba` 3/10;
  - `snes/Super Smoothie World.sfc` 0/5;
  - `gb/Pocket Peel.gb` with no achievement columns.

  A fixture `game-titles.tsv` renames Orchard Tactics to "Orchard Tactics DX". The page logs `games on this card: 2`,
  shot `games`, and the log line for the renamed title. An empty `playstats.tsv` gives `games on this card: 0`, shot
  `games-none`. `make test` (upstream's run.sh is unaffected by the global), `kiwami/tests/ci.sh .buildroot`.
- **Commit**: `RetroAchievements: the games on this card, with the menu's titles, from the library`

### 9. Hashing games as RetroArch does: rcheevos, and the fork's C build

- **Files**:
  - `ui/Makefile` (hook)
  - `ui/src/kiwami/kiwami.mk` (new)
  - `ui/src/kiwami/third_party/rcheevos/` (new, vendored)
  - `ui/src/kiwami/rahash.h` (new)
  - `ui/src/kiwami/rahash.cpp` (new)
  - `kiwami/tests/units.cpp` (changed)
  - `kiwami/tests/ci.sh` (changed)
  - `kiwami/DEPENDENCIES.md` (changed)
  - `docs/FORK.md` (changed)
  - `.workspace/0019-retroachievements/plan.md` (changed)
- **Change**:
  - **Vendoring.** rcheevos v12.5.0 (`143317322`, 14 Sep 2026, MIT), unchanged:
    - `include/` and `src/rc_compat.h`;
    - `src/rc_util.c` and `src/rc_compat.c`;
    - `src/rhash/hash.c`, `hash_rom.c`, `hash_disc.c`, `cdreader.c`, `md5.c` and the headers.

    `hash_encrypted.c`/`aes.c` and `hash_zip.c` are left out with `RC_HASH_NO_ENCRYPTED` and `RC_HASH_NO_ZIP`, if
    v12.5.0 has them; if not, the files are kept and the reason is noted in `DEPENDENCIES.md`. The folder has
    rcheevos' `LICENSE` and a `SHA256SUMS` of every vendored file, with a `VENDORED` note naming the source URL, tag
    and commit.
  - **The build hook.** One added line in `ui/Makefile` after the two existing `kiwami:` lines (line 13, so that it
    comes before `banana-ui: $(SRC)` reads `SRC`):
    ```make
    include src/kiwami/kiwami.mk	# kiwami: the fork's C libraries (RetroAchievements' hashing)
    ```
    `kiwami.mk`:
    - lists the C files and compiles each to `<file>.kiwami.o` with the C++ compiler's driver as C:
      `$(CXX) -x c $(KIWAMI_CFLAGS) -c`. Buildroot passes only `CXX` to banana-ui's build, so this needs no change to
      `buildroot/package/banana-ui/banana-ui.mk`;
    - `KIWAMI_CFLAGS = $(filter-out -std=% -W%,$(CXXFLAGS)) -std=gnu11 -w` plus the include folders and defines, so
      the target's flags carry over and vendored code's warnings stay quiet;
    - adds the objects to `SRC` (linked by upstream's rule; no `ar`, which would be the host's in a cross build);
    - gives a `kiwami-c` target (the objects alone, for `ci.sh`'s units) and adds a `kiwami-clean` to `clean`.

    `*.o` is already in `.gitignore`. Buildroot's `make -B` rebuilds the objects, so a host build's leftovers in the
    rsynced copy are never linked.
  - **`rahash`**: `Result hash(const std::string& path, const std::string& system)` → `{ hash, error }`, for the nine
    systems' consoles. Plain files and discs go through rcheevos' own readers (`rc_hash_generate_from_file`): `.nds`,
    `.cue`/`.bin`, `.iso`, `.m3u`, and every cartridge. `.zip`/`.7z` (step 10), `.chd` (step 11) and the refusals
    (A16: `.pbp`, `.img`, `.mdf`) answer "can't be identified" with the reason. rcheevos' log goes to banana-ui's
    stderr as `ra: hash: ...` lines.
  - **`DEPENDENCIES.md`**: a row for rcheevos (version, date, where it's pinned: the folder and its `SHA256SUMS`;
    licence; what a bump checks: that the hash rules for the nine consoles still match RetroArch's pin).
  - **`docs/FORK.md`**: its banana-ui line in *Where a customization goes* names `ui/src/kiwami/third_party/` (vendored
    C, checked by `SHA256SUMS`) and `kiwami.mk`.
- **Tests**:
  - `ci.sh`: `sha256sum -c` in each `third_party/*/` (A20); `make -C ui kiwami-c`; `units` linked with `rahash.cpp`
    and the objects.
  - `units.cpp` `testHash`, each expected MD5 computed in the test from the bytes:
    - a 1024-byte GBA → md5 of the file (`0f343b0931126a20f133d67c2b018a3b`, the fixture's);
    - an NES with `NES\x1a` → md5 of all but the first 16 bytes;
    - a SNES of 512 + 1024 bytes → md5 without the first 512;
    - an N64 as `.z64`, `.v64` and `.n64` → one hash;
    - a `.nds` built in the test → md5 of its header (0x160), ARM9, ARM7 and icon (0xA00), at the offsets its header
      gives;
    - a PlayStation `.iso` and its `.cue`s from `kiwami/tests/ra/` → equal hashes, and equal to an `.m3u` naming the
      cue;
    - `.pbp` → the refusal.
  - `make test` (banana-ui builds with the objects), `make -C ui` (with ALSA, as CI), `kiwami/tests/ci.sh .buildroot`,
    and `make banana-ui-rebuild-rgds` (the cross build, through Buildroot's `banana-ui.mk` unchanged).
- **Fixtures**: `kiwami/tests/ra/make-fixtures.sh`, run once by hand (needs `bsdtar` and, for step 11, `chdman`), makes
  the committed fixtures:
  - a PlayStation `psx.iso` (ISO 9660 by `bsdtar`, with `SYSTEM.CNF` and a small PS-X EXE);
  - `psx-mode1.cue` (MODE1/2048 on the iso);
  - `psx.bin`/`psx-mode2.cue` (the same sectors as MODE2/2352, wrapped by a few lines of Python).

  Each file stays small, and the script's commands are in the commit.
- **Commit**: `RetroAchievements: rcheevos' hashing, vendored, and the fork's C build in banana-ui`

### 10. Hashing games in .zip and .7z: the LZMA SDK's 7z decoder

- **Files**:
  - `ui/src/kiwami/third_party/lzma/` (new, vendored)
  - `ui/src/kiwami/kiwami.mk` (changed)
  - `ui/src/kiwami/rahash.cpp` (changed)
  - `ui/src/kiwami/ra7z.h` (new)
  - `ui/src/kiwami/ra7z.cpp` (new)
  - `kiwami/tests/ra/` (changed: fixtures)
  - `kiwami/tests/ra/make-fixtures.sh` (changed)
  - `kiwami/tests/units.cpp` (changed)
  - `kiwami/DEPENDENCIES.md` (changed)
  - `.workspace/0019-retroachievements/plan.md` (changed)
- **Change**:
  - **Vendoring.** 7-Zip 26.02's `C/` (tag `26.02`, `f9d78aff3`, public domain), only what the 7z decoder needs:
    `7z.h`, `7zArcIn.c`, `7zBuf.c`, `7zCrc.c`, `7zCrcOpt.c`, `7zDec.c`, `7zStream.c`, `7zAlloc.c`, `7zTypes.h`,
    `Bcj2.c`, `Bra.c`, `Bra86.c`, `BraIA64.c`, `Delta.c`, `CpuArch.c`, `LzmaDec.c` and `Lzma2Dec.c`, with their
    headers. The exact list is what links, and is recorded in `SHA256SUMS` with a `VENDORED` note.
  - **Cartridge archives.** For a `.zip`, `rahash` reads the member through upstream's `readZip` (`ui/src/zip.cpp`); for
    a `.7z`, `ra7z` uses `SzArEx_Open`/`SzArEx_Extract`. The member is the first in name order whose extension the
    system's `ext` list has (archives aside), as RetroArch picks. Its unpacked size is checked against 64 MB before
    anything is allocated, and the bytes go to `rc_hash_generate_from_buffer`.
  - **Refusals.** "No game in the archive", "Over 64 MB unpacked", "A DS game can't be identified from a .zip", and
    "Damaged archive" (a decoder error).
  - **`DEPENDENCIES.md`**: a row for the LZMA SDK (26.02, public domain, the files; a bump keeps libchdr's `LzmaDec`
    in step).
- **Tests**:
  - `units.cpp`, against committed fixtures made by `make-fixtures.sh` with `bsdtar`:
    - the GBA plain, as `.zip`, as `.7z` LZMA and as `.7z` LZMA2 → one hash;
    - the headered NES in a `.7z` → the headerless hash;
    - an archive with `readme.txt` and `game.gba` → `game.gba`'s hash;
    - an archive with only `readme.txt` → "No game";
    - a truncated `.7z` and a truncated `.zip` → "Damaged archive", no crash;
    - a `.7z` whose header claims 65 MB → refused before allocation (a crafted header, a few bytes).
  - `sha256sum -c` for `lzma/`.
  - `make test`, `make -C ui`, `kiwami/tests/ci.sh .buildroot`.
- **Commit**: `RetroAchievements: games in .zip and .7z hashed as RetroArch does, with the LZMA SDK's 7z decoder`

### 11. Hashing PlayStation .chd: libchdr

- **Files**:
  - `ui/src/kiwami/third_party/libchdr/` (new, vendored, with its `zstd` decompressor)
  - `ui/src/kiwami/kiwami.mk` (changed)
  - `ui/src/kiwami/rachd.h` (new)
  - `ui/src/kiwami/rachd.cpp` (new)
  - `ui/src/kiwami/rahash.cpp` (changed)
  - `kiwami/tests/ra/` (changed: `psx.chd`, a truncated copy)
  - `kiwami/tests/ra/make-fixtures.sh` (changed)
  - `kiwami/tests/units.cpp` (changed)
  - `kiwami/DEPENDENCIES.md` (changed)
  - `.workspace/0019-retroachievements/plan.md` (changed)
- **Change**:
  - **Vendoring.** libchdr master `47d7941` (11 Oct 2026, BSD-3), unchanged: `include/libchdr/`, `src/` (the
    library's C files and codecs), and `deps/zstd-1.5.7/lib/{common,decompress}` with `zstd.h`, built with
    `ZSTD_DISABLE_ASM`. zlib is the system's (`CHDR_SYSTEM_ZLIB`; banana-ui already links zlib through `pkg-config`).
    LZMA is step 10's `LzmaDec.c`; if libchdr's bundled `deps/lzma-26.02` differs from it, libchdr's file is kept
    under its own name and the reason noted. Licences are kept, with `SHA256SUMS` and `VENDORED`.
  - **`rachd`**: a disc reader for rcheevos (`rc_hash_init_custom_cdreader`) that serves `.chd` through libchdr and
    hands everything else to rcheevos' default reader:
    - it reads the CD track metadata (`CDROM_TRACK_METADATA2`);
    - it reads sectors by frame;
    - it handles MODE1/2048, MODE1_RAW and MODE2_RAW and the pregap offset as libretro-common's `chd_stream.c` does
      (RetroArch's reader).

    It is installed once, before the first hash.
  - **Refusal.** A `chd_open` or read error → "Damaged .chd".
  - **`DEPENDENCIES.md`**: rows for libchdr (the commit and why not PCSX-ReARMed's `9ccd3a7`) and zstd 1.5.7
    (decompressor only), with their licences and what a bump checks.
- **Tests**:
  - `units.cpp`:
    - `psx.chd` (made by `make-fixtures.sh` with `chdman createcd` from `psx-mode2.cue`, committed) → the same hash as
      the `.iso` and both `.cue`s;
    - a `.m3u` naming the `.chd` → the same;
    - the `.chd` cut to half → "Damaged .chd", no crash.
  - `sha256sum -c` for `libchdr/`.
  - `make test`, `make -C ui`, `kiwami/tests/ci.sh .buildroot`, `make banana-ui-rebuild-rgds`.
- **Commit**: `RetroAchievements: PlayStation .chd hashed through libchdr`

### 12. A game's achievements

- **Files**:
  - `ui/src/kiwami/ra.h` (changed)
  - `ui/src/kiwami/ra-game.cpp` (new)
  - `ui/src/kiwami/ra-games.cpp` (changed)
  - `ui/src/kiwami/rastore.cpp` (changed: the hash cache)
  - `kiwami/tests/ra-ui.sh` (changed)
  - `kiwami/tests/ra/` (changed: `gameid`, `patch`, `unlocks` replies)
  - `.workspace/0019-retroachievements/plan.md` (changed)
- **Change**: A on a game opens `RA_GAME`. One thread runs these steps, the page showing each ("Identifying...",
  "Loading..."):
  1. **The hash.** It is looked up in `cache/ra-hashes` (`<path>\t<size>\t<mtime>\t<hash>\t<id>`); otherwise it is
     computed by `rahash` and cached.
  2. **The ID.** `gameId(hash)` (cached in the same line).
  3. **The list.** `patch`, then `unlocks` with `h=0` and with `h=1`.
  4. **The badges.** Each badge is fetched into `cache/ra-badges/` if missing.

  The page:
  - **Bottom**: the achievements, unlocked first, then locked. Each has its badge (a locked one drawn dimmed), title,
    points, and an `H` or `S` mark on unlocked ones.
  - **Top**: the game's title, "<unlocked> / <total> · <hardcore> hardcore", and the selected achievement's
    description.
  - **States**: "Can't identify this game: <reason>" (A16), "RetroAchievements doesn't know this version of the game
    (hash <md5>)" (id 0), and "No connection". Not signed in, the list shows without unlocks, with "Sign in to see
    yours".

  B goes back to the games. Badges are uploaded when drawn and dropped when the page closes. The page logs
  `ra: game <id> "<title>": <n> achievements, <u> unlocked (<h> hardcore)` and `ra: can't identify <file>: <reason>`.
- **Tests**: `ra-ui.sh`:
  - A on Orchard Tactics (the fixture's 1024 zero bytes): the server log has `gameid` with `m=0f343b...`, then
    `patch g=<id>`, then two `unlocks` (`h=0`, `h=1`). The page logs its line, shot `game`;
  - opening it again: no `gameid` (cached);
  - a fixture `.pbp` in `psx/` with progress: the refusal logged, shot `game-pbp`;
  - an unknown hash (the stand-in answers id 0): the message;
  - `KIWAMI_RA_SERVER_DOWN` after the hash is cached: "No connection";
  - `bench 120` on the groups grid with the fork's group, against the same with `KIWAMI_RA_GROUP=0`: the frame times
    are within each other's noise (A19);
  - the log has no `pw1`/`tok1`.

  `make test`, `kiwami/tests/ci.sh .buildroot`.
- **Commit**: `RetroAchievements: a game's achievements, locked and unlocked, from the games on this card`

### 13. The group's icon: RetroAchievements' logo

- **Files**:
  - `ui/src/kiwami/ra-icon.cpp` (changed)
  - `ui/src/kiwami/kiwami.cpp` (changed)
  - `ui/src/kiwami/kiwami.h`, `ui/src/kiwami/ra.h` (changed: comments)
  - `ui/src/kiwami/ra-account.cpp` (changed)
  - `kiwami/tests/ra-ui.sh` (changed)
  - `.workspace/0019-retroachievements/plan.md` (changed)
- **Change**: the user's request after trying the update (see *Deviations*).
  - `ra-icon.cpp` gets a second pixel map, `kiwami.ra`: RetroAchievements' logo (a blue R and a gold A, each stroke
    with the logo's dark line along its middle), redrawn by hand at 32x32 in the icons' look. It has a black outline,
    the trophy's golds, and the blues of `cart.png` and `gamepad.png` (`#7EA8E6`, `#4C8EE0`, `#3D6FB8`, and `#1F3D6E` for
    the R's inner line). `icon()` serves both maps.
  - The Account and Stats rows take `kiwami.ra`, so the group's tile and the panels show the logo. An account with no
    avatar shows it too (`raAvatar`).
  - The trophy stays for an achievement whose badge isn't on the card (`ra-game.cpp`, unchanged).
  - No upstream file changes. The `icon()` hook's comment, "(the trophy)", stays as it is: the trophy is still one of
    the fork's icons.
- **Tests**: `ra-ui.sh`:
  - `painted` and `unpainted` check that a shot has a colour or hasn't;
  - `#1F3D6E`, the R's inner line, is on the logo's tile (`g7`, bottom) and on no tile of the first rows (`g0`). It is
    also on the group list's top panel (`ra-list`, Account's icon). Built with the trophy on the rows, these checks
    fail;
  - `make test`, `make -C ui`, `kiwami/tests/ci.sh .buildroot`.
- **Commit**: `RetroAchievements: the group's icon is RetroAchievements' logo, pixelated`

### 14. Final check

- `make test`, `make -C ui`, `make -C system/bananad CFLAGS="-O2 -Werror"`, `kiwami/tests/ci.sh .buildroot`.
- The fork's tests in `kiwami/tests/`: `units`, `ra-ui.sh` (with `ra-server.py`), `tailscale-ui.sh`, `cores-ui.sh`,
  `7z-ui.sh` and the rest that `ci.sh` runs.
- The hooks: `git diff --stat main...HEAD` and `git grep -n 'kiwami:'` (the commands in `docs/FORK.md`) agree, and
  this feature added exactly these:
  - `ui/src/settings.cpp`: `build()` 1 line, `icon()` 1, the grid 9 (2 added, 7 modified), `gameOpen` 1;
  - `ui/src/main.cpp`: 2;
  - `ui/Makefile`: 1.
- With the fork's group absent (`KIWAMI_RA_GROUP=0`) and with it present, `make test`'s renders in `ui/tests/out/`
  match `main`'s. Upstream's run.sh never scrolls the grid.
- `kiwami/DEPENDENCIES.md` lists rcheevos v12.5.0, the LZMA SDK 26.02, libchdr `47d7941` and zstd 1.5.7, each with
  its licence, and `sha256sum -c` passes in each `third_party/` folder.
- `docs/FORK.md` names `ui/src/kiwami/third_party/` and `kiwami.mk`. `docs/ARCHITECTURE.md` and `CLAUDE.md` need no
  change (checked: neither describes Settings' groups or the account).
- banana-ui's size, before (`kiwami` at this branch's base) and after, from `make banana-ui-rebuild-rgds-plus`
  (`output/rgds-plus/target/usr/bin/banana-ui`), recorded under *Manual tests* (A19).
- The manual tests below all passed.
- Every acceptance criterion is met: each is listed by at least one automated or manual test, and those passed.

## Automated tests

| Test | Where | What it checks | Criteria |
|---|---|---|---|
| The grid and the group | `kiwami/tests/ra-ui.sh` (step 2) | Rows' log line; the first rows identical to the six-group grid; RIGHT/DOWN/UP scroll and wrap; taps on shown and hidden tiles; the group's list; Mode's value | A1, A2, A3, A21 |
| Grid moves | `kiwami/tests/units.cpp` (step 2) | `gridMove` equals upstream's formula for n % 3 == 0; `gridMove`/`gridTop` for 7 and 8 groups | A1, A2 |
| Replies | `kiwami/tests/units.cpp` (step 3) | Each request's parser, refusals, malformed and empty replies, the per-system sums, core achievements only | A11, A15 |
| The card | `kiwami/tests/units.cpp` (step 4) | Sign in/out/remove/adopt on a temporary card; the scrub first; other lines kept; case; write errors | A4, A7, A8, A9, A10 |
| The Account page | `kiwami/tests/ra-ui.sh` (step 5) + `ra-server.py` | Sign in accepted, refused, offline; token fetched later; sign out (one A, two A's); remembered sign-in; X twice; adoption; fresh card; no secrets in the log | A4-A10, A18, A21 |
| The panel's token | `kiwami/tests/units.cpp` (step 6) | The token from `accounts.conf` for the panel's user; another user; none; an old card's own token | A17 |
| The Stats page | `kiwami/tests/ra-ui.sh` (step 7) | The requests made (one `login2` with the token, nine `allprogress` in order); Y; none after closing; the cache offline; signed out | A11, A12, A13, A19 |
| The games | `kiwami/tests/ra-ui.sh` (step 8) | The games with progress, the menu's titles (a renamed one), none | A14 |
| Hashes | `kiwami/tests/units.cpp` (steps 9-11) | Every format against an MD5 computed from its bytes, and against each other (plain = zip = 7z, iso = cue = chd = m3u); refusals; truncated and crafted files | A15, A16 |
| A game's achievements | `kiwami/tests/ra-ui.sh` (step 12) | `gameid` → `patch` → `unlocks` ×2; the hash cache; refusals, unknown game, no connection; bench against the six-group grid | A15, A16, A19 |
| Vendored code | `kiwami/tests/ci.sh` (steps 9-11) | `sha256sum -c` in each `third_party/` folder | A20 |
| Upstream's | `ui/tests/run.sh` (`make test`) | Unchanged, passing at every step; its renders equal to `main`'s | A2 |

## Manual tests

On the RG DS Plus, with an image from this branch (`make rgds-plus`, or `make banana-ui-rebuild-rgds-plus` and an
update), Wi-Fi joined, and a real RetroAchievements account (two, for M3). Logs are fetched over SSH/SFTP (Settings >
Network > SSH) or from the card on a computer (`banana/logs/` on the card is `/data/banana/logs/` on the device).
banana-ui's log is `/data/banana/logs/banana-ui.log`; its `ra:` lines and `kiwami:` lines are what the notes quote,
and they never carry a password or token.

| # | Board | Steps | Expected | Logs to collect | Criteria | Outcome |
|---|---|---|---|---|---|---|
| M1 | rgds-plus | Settings: RIGHT from System, DOWN/UP through the rows, taps on tiles of both scroll positions; open RetroAchievements; switch Mode, then look at Other systems > RetroAchievements, and switch it back there | The grid scrolls one row at a time, the logo's tile looks like the others, taps hit what shows; both Mode rows always agree, and `retroarch.cfg` follows | `banana-ui.log` (`groups from row`), `/data/retroarch/retroarch.cfg`'s `cheevos_hardcore_mode_enable` | A1, A3, A21 | passed (2026-10-24, the user; see the notes) |
| M2 | rgds-plus | Account: sign in with the real account (typed, then by touch); then sign in with a wrong password; then with Wi-Fi off, then on again and reopen the page | Signed in with avatar and points; the wrong one refused and nothing changed; offline kept unchecked, the token got on reopening | `banana-ui.log`; `banana/accounts.conf` (check its keys, don't share its values) | A4, A5, A6 | passed (2026-10-24, the user; see the notes) |
| M3 | rgds-plus | Sign out (A, A); start a RetroArch game and a DS game (BananaDS, then DraStic); sign back in from the remembered list; sign in to a second account, then switch back; remove the second with X, X | After sign-out: no achievement pop-up or login in either game, both `ra.token` gone, `retroarch.cfg` `cheevos_enable = "false"`; the lists behave as A7-A9 | `banana-ui.log`, `/run/banana/retroarch.log` (Second screen on), `banana/logs/superdrastic/`, BananaDS's log in `banana/logs/`; `ls /data/superdrastic/ra.token /data/bananads/ra.token` | A7, A8, A9 | passed (2026-10-24, the user; see the notes) |
| M4 | rgds-plus | Stats: open, compare with the account's page on retroachievements.org; Y; Wi-Fi off and reopen; sleep (lid) while "Updating...", open the lid | Points, completed/mastered and unlocked per system match the site; offline shows the cache with its date; after the lid, the page shows the result or the failure and works | `banana-ui.log` (the `ra:` request lines and times) | A11, A12, A22 | passed (2026-10-24, the user; see the notes) |
| M5 | rgds-plus | Games on this card: A on a GBA game, a `.7z` SNES or N64 game, a `.nds`, a PlayStation `.chd` and a `.cue`, each played before with Second screen on (so the panel identified it); time the first open of the `.7z` and the `.chd` | Each game's list matches the site's, the IDs match the game panel's (`Identified game:` in RetroArch's log), and the first open takes a few seconds at most | `banana-ui.log` (`ra: hash`, `ra: game` lines with the time), `/run/banana/retroarch.log` from each game's last run | A14, A15, A16 | passed (2026-10-24, the user; see the notes) |
| M6 | rgds-plus | Second screen on: play a game with achievements (a GBA game on mGBA, or `smb` on FCEUmm) | The game panel's achievement list loads (it showed "This game has no achievements" before step 6) | `banana-ui.log` (`game panel:` lines), `/run/banana/retroarch.log` (copy it before the next game: it's in `/run`) | A17 | passed (2026-10-24, the user; see the notes) |
| M7 | rgds-plus | The menu with Settings closed after using the pages, then `bench`-like use: scroll the library 30 s; `top -b -n 1` over SSH | No banana-ui thread beyond upstream's, no network activity; the menu as smooth as before. banana-ui's size recorded | `top` output, `ls -l /usr/bin/banana-ui` | A19 | passed (2026-10-24, the user; see the notes) |

Notes from the runs (what the logs showed, what was fixed):

- **M1-M7, 2026-10-24: passed, on the user's word.** The user ran them on the RG DS Plus with the 0.4.2 update built
  from `d8fbbc0` (step 12), and reported that everything works. No logs were passed back, so none were analysed, and
  the notes have no log lines. The one change the user asked for after that run, RetroAchievements' logo instead of the
  trophy, is step 13. It was checked headless (`ra-ui.sh`, and the renders at both sizes), and the image built from
  `92cbd22` has it.

- **banana-ui's size (A19), 2026-10-24.** Built for the RG DS Plus with its Buildroot toolchain, flags as
  `banana-ui.mk`'s, stripped with `aarch64-linux-strip`:
  - before, `kiwami` at `b59fab5`: 1,357,928 bytes (1,594,416 unstripped);
  - after, this branch at `d8fbbc0`: 1,919,352 bytes (2,231,784 unstripped), +561,424 (+41%);
  - after step 13, at `92cbd22` (`make banana-ui-rebuild-rgds-plus`): 1,919,880 bytes (2,232,360 unstripped),
    +561,952 (+41%) over the before. The logo's map adds 528 bytes. The image's copy, stripped by Buildroot, is
    1,919,760.

  `make banana-ui-rebuild-rgds-plus` gave the same binary as the hand cross-build of `d8fbbc0`, to the byte, so the
  before (which Buildroot can't build without checking it out) was measured the same way. The three libraries are
  about 279 KB of the growth (rcheevos 54 KB, the LZMA SDK 40 KB, libchdr with zstd 185 KB, `size` of their objects),
  under the investigation's 300-400 KB. The rest, about 282 KB, is the fork's pages, requests and card code.
- **The final check's automated part, 2026-10-24, at `92cbd22`.**
  - All pass: `make test`, `make -B -C ui` and `make -B -C system/bananad CFLAGS="-O2 -Werror"` (no warnings), and
    `kiwami/tests/ci.sh .buildroot`. That covers `units` 556 checks, the tailscale, 7z, cores and RetroAchievements UI
    tests, the three boards configured and the RetroArch pin. Every `third_party/` folder matches its `SHA256SUMS`.
  - The hooks: this feature's (`git diff b59fab5 HEAD`) are the plan's 15 lines, every added line marked `kiwami:`.
    `docs/FORK.md`'s two commands, run against `main` and `release-0.4.2` merged (`dc6eab9`), agree, but for
    `README.md`, `nds-saves` and `settings_defaults.cpp`. This feature doesn't touch them: they are how the release's
    merge (`9a8e35e`) settled `main` against the release, recorded in `.workspace/merges/2026-10-23-e1b2ac4.md`.
  - The renders: four `make test` suites ran side by side (22:25:55-22:26:20): this branch, this branch with
    `KIWAMI_RA_GROUP=0`, `kiwami` at `b59fab5`, and `release-0.4.2` at `e1b2ac4`, each exported with `git archive`.
    - With the group out, all 192 PNGs are identical to `b59fab5`'s.
    - With it, 191 are. The one left, `640x480-setup-bot.png`, differs in the time zone row's clock only (19:26 against
      19:25, 10x14 pixels): the suites crossed a minute. Before step 13 (21:38:01-21:38:27), all 192 were identical.
    - `release-0.4.2`'s renders differ from `b59fab5`'s in 26 PNGs, and it has no reset screens (8). This branch differs
      from it in the same PNGs, plus the clock's one: upstream's `main`, which `kiwami` took, not this feature.
  - The docs: `kiwami/DEPENDENCIES.md` lists rcheevos v12.5.0, the LZMA SDK 26.02, libchdr `47d7941` and zstd 1.5.7,
    each with its licence. `docs/FORK.md` names `ui/src/kiwami/third_party/` and `kiwami.mk`. `docs/ARCHITECTURE.md`
    and `CLAUDE.md` need no change.
  - The image: `make rgds-plus` from `92cbd22` (0.4.2), `banana-rgds-plus.img.gz` and
    `banana-rgds-plus-0.4.2.update.tar`.

## Deviations

Where the implementation turned out different from this plan, why, and what changed (dated).

- **2026-10-24, step 1: `Page` is a scoped enum.** The plan's `enum Page { NONE, TAILSCALE }` doesn't compile as
  intended: `Kiwami` is nested in `Settings`, so an unscoped `NONE` hides `Settings::Result`'s `NONE` in every member,
  and Tailscale's `return NONE;` would return a `Page`. It is `enum class Page { NONE, TAILSCALE } pageOpen =
  Page::NONE;` (agreed with the user), and the later pages are `Page::RA_ACCOUNT`, `Page::RA_STATS`, `Page::RA_GAMES`
  and `Page::RA_GAME`. `opened()` is routed like `update` and `wake` (Tailscale's part is `tsOpened`), so step 5's
  `adopt()` call goes beside it rather than into Tailscale's code.
- **2026-10-24, step 2: the grid's arithmetic in `grid.h`/`grid.cpp`.** As `Settings::Kiwami` members, `gridMove` and
  `gridTop` couldn't be linked into `units` without all of Settings. They are plain functions,
  `kiwami::gridMove(g, n, up)` and `kiwami::gridTop(row0, gsel, n)`, in two new files, and the `Kiwami` members call
  them (`gridTop` keeps `row0` and logs). Agreed with the user.
- **2026-10-24, step 2: Mode and Other systems > RetroAchievements kept in step by `Kiwami::update`.** Upstream reads a
  row's value only when Settings opens, so after Mode changed, upstream's row showed the old value until Settings was
  reopened, and A3 failed. `Kiwami::update`, which runs every loop while Settings is open, copies a change from either
  row to the other. There is no added hook. `ra-ui.sh` checks both ways on screen and in the log. Agreed with the user.
- **2026-10-24, step 2: `cores-ui.sh` updated.** It checks that the log's `kiwami: rows in` lines are exactly these.
  The group adds `RetroAchievements: kiwami.ra.account cheevos_hardcore_mode_enable` to its four expectations, which
  check more than before. Agreed with the user.
- **2026-10-24, step 2: the grid hook modifies seven of upstream's lines, not six.** `pressGroups` 1, the touch code 2
  (the loop, `y`), `renderGroups` 4 (the slots' loop, the pick, the tiles' loop, `y`). The final check's count is
  updated. The trophy's gold is the icons' `#F2C94C` (`bank.png`, `sword.png` and others), `#FFF0A8` (`bank.png`) and
  `#8A6A1E` (`cart.png`), and its plinth `gear.png`'s greys. `fav.png` has no gold.
- **2026-10-24, step 3: details the plan left open.**
  - A game counts once in the sums. Mastered means every achievement unlocked in hardcore. Completed means every
    achievement unlocked, not all of them in hardcore, so a mastered game isn't also completed, as on the site's
    profile. Started counts every game `allprogress` lists. Unlocked adds each game's larger of `Unlocked` and
    `UnlockedHardcore`.
  - `refused` is `Success: false`, or a 401 or 403. Anything else that isn't a yes is "no connection" when there was no
    answer, and "unreadable answer (<status>)" otherwise: a 5xx page, JSON cut short, an empty body, a yes with no
    token.
  - `fetch` is internal. `avatar` and `badge` call it, and refuse names that aren't plain (letters, digits, `-`, `_`).
    An avatar's file is the name in lower case, so `remove` (step 4) finds it whatever the case. A badge keeps its
    name, as upstream's panel names it in the same folder.
  - The stand-in also checks the token of every request but `gameid` (401, `token-refused.json`). It picks the most
    specific canned reply for the request's `c`, `g`, `m` and `h` (`unlocks-h1.json`,
    `gameid-m<md5>.json`), and fills `login2.json`'s `@USER@`, `@TOKEN@` and `@MEDIA@`. `ra/` also has the
    replies the parser tests need: `login2-refused`, `token-refused`, `allprogress-empty`, `malformed` and
    `unavailable`.
  - `units` takes the `ra/` folder as a second argument and links with `-ldl`. `ci.sh` compiles `ra-server.py`
    with its bytecode in the temporary folder.
- **2026-10-24, step 4: details the plan left open.**
  - The card's operations log their own lines: `ra: account <name> on the card (token kept|no token)`,
    `ra: signed out <name>`, `ra: removed <name>`, `ra: adopted <new>, <old> remembered` (`ra: adopted <new>` with
    none before, `ra: <old> no longer on the card, remembered` when `accounts.conf` lost its account) and, after each
    change to the list, `ra: remembered: <names>`. Step 5's page adds only its own (`signed in as`, `refused`, `not
    checked`).
  - `signedIn` takes the account before from `accounts.conf` when `ra-accounts` names none yet (`adopt()` not run).
  - `signOut` clears every place even when one fails, and says the first error. If `accounts.conf` can't be written,
    the list keeps the account current, since its password is still on the card. A scrub that fails is logged
    (`ra: the scrub failed (<status>), signing out all the same`) and the sign-out goes on. If the scrub brought in
    another account (RetroArch's own login), both are remembered, the one shown first.
  - `remove` refuses the account signed in.
  - The stats cache is `cache/ra-stats/<name in lower case>`, lines `when`, `score`, `softscore`, `messages` and one
    per system (`<system> <started> <completed> <mastered> <unlocked>`), read and written by `saveStats` and
    `loadStats` here so that `remove` has something to delete; step 7 fills it.
- **2026-10-24, step 5: details the plan left open.**
  - The pages' shared part of the hooks is in `ra-account.cpp`: the account adopted when Settings opens (only when the
    group's rows were built, so `KIWAMI_RA_GROUP=0` leaves the card alone), the Account row's value (read from the card
    when Settings opens and after each change, not at every frame), the request on a thread and its answer taken in by
    `update()`. A sign-in holds A and X until it is answered. The token fetch on opening doesn't, and a sign-in started
    meanwhile replaces it (its answer is dropped).
  - A sign-in keeps the name as the server writes it. An answer that isn't the server's yes or no (a 5xx, JSON cut
    short) is treated as no connection: kept unchecked, the page saying "Not checked: <why>".
  - A password kept on the card that the server refuses when the page fetches its token stays. The page says to sign
    out and in again.
  - The page's own log lines, besides the plan's and rastore's: `ra: account page: <signed in as alice | not signed
    in>, <n> remembered`, `ra: checking the account <name>`, `ra: password asked for <name>`, `ra: no token for <name>
    yet: asking for it`, `ra: token kept for <name>` and `ra: the password kept for <name> was refused`.
  - The keyboard's title naming alice is checked by `password asked for alice` and the shot `pw-alice`. The fresh
    card's "Not signed in" is checked by the list's shot differing from the signed-in one, and the request-free open.
    "No connection" is a second stand-in started with `KIWAMI_RA_SERVER_DOWN`, `KIWAMI_RA_URL` pointing at it for that
    run. `ra-ui.sh` also taps the page's rows (A21): a first tap selects New account, a second opens the same keyboard
    as A.
- **2026-10-24, step 6: the panel's token in `rastore`, tested there.** `units` can't link `Settings::Kiwami`, so the
  test is written against `kiwami::ra::panelToken(card, user)`, declared in `rastore.h` (not in the step's Files). The
  hook's expression (`retroarch.cfg`'s token, else `panelToken`) is a lambda in the test. `Settings::Kiwami::panelToken
  (user, data)`, which the hook calls, is in `kiwami.cpp` (not in the Files either). It returns "" with no user
  (achievements off) and logs `banana-ui: ra: game panel: the token from accounts.conf` or `... no token in
  accounts.conf either`, the lines M6 reads. Run against the stub first, the test failed as planned, with its
  arguments in this order: `panelToken(card, "Alice") is "", not "tok1"`.
- **2026-10-24, step 7: the files the Stats row touches.**
  - `kiwami.h` gets `Page::RA_STATS` and the page's functions, which every page needs (not in the step's Files).
  - `rastore.cpp` doesn't change: step 4 already reads and writes the stats cache.
  - `ra-account.cpp`: `raValue` takes the row's key (Stats' value is the points), the points' wording moved to `ra.h`
    (`raPoints`), and the avatars stay while either page shows.
  - The group's rows line gains `kiwami.ra.stats`, so `cores-ui.sh` changes (as in step 2). In `ra-ui.sh`, Mode is the
    group's third row.
- **2026-10-24, step 7: details the plan left open.**
  - The fixtures are `allprogress-c18.json` (DS: one game mastered, one started) and `allprogress-c2.json` (N64: none).
    The other systems get `allprogress.json`.
  - A fetch stops at the first request that fails, and the card's copy shows ("No connection: the stats as of
    <date>."). With none, it says "No connection.", logged `ra: no stats for alice: <why>`. A refused token says to
    sign in again on the Account page.
  - A fetch made with the password (no token yet) keeps the token it gets, and `ra-ui.sh` checks it.
  - An answer that comes back after the account changed is dropped.
  - Each system's row has two lines: its name and achievements unlocked, then started, completed and mastered. The
    top panel spells out the selected system's numbers.
- **2026-10-24, step 8: the games' row and page.**
  - As in step 7, `kiwami.h` (`Page::RA_GAMES`, the page's functions), `ra-account.cpp` (the row's count, read when
    Settings opens) and `cores-ui.sh` (the rows line) change too. Mode is now the group's fourth row in `ra-ui.sh`.
  - The row's icon is upstream's `cart`.
  - A on a game does nothing until step 12, so the top panel doesn't mention it yet.
  - The page logs each game it lists: `ra: game <system>/<rel> "<title>" <unlocked> / <total>`. The test reads the
    renamed title there.
  - The fixture's play stats and `game-titles.tsv` are written by `ra-ui.sh` (`fixture.sh` is upstream's) and put back
    after.
  - A second-card game can't be reached headless (the library finds the second card through `/run/banana/sd2`). It
    goes through the same shelves, so M5 covers it on the device.
- **2026-10-24, step 9: the SNES fixture is 512 + 8192 bytes, not 512 + 1024.** rcheevos skips a copier header only
  when the file is 512 bytes over a whole number of 8 KB (`hash_rom.c`'s `rc_hash_snes`), as real dumps are. So the
  plan's 1536-byte file is hashed whole, rightly, and the plan's expected value was wrong. The test now has a 512 +
  8192-byte file (its last 8192 bytes' MD5) and also checks the rule: a 512 + 1024-byte file is hashed whole. Agreed
  with the user.
- **2026-10-24, step 9: details the plan left open.**
  - `rahash` uses rcheevos 12's iterator (`rc_hash_initialize_iterator`, `rc_hash_generate`) with its own callbacks
    (messages, readers), not the deprecated `rc_hash_generate_from_file` and global setters. The pages hash on their
    thread, and nothing global is shared. Step 11's `.chd` reader goes into each iterator's `callbacks.cdreader`
    instead of `rc_hash_init_custom_cdreader`.
  - Vendored: five of `include/`'s headers (the ones the hashing includes), not the whole folder.
  - `RC_HASH_NO_ENCRYPTED` and `RC_HASH_NO_ZIP` change `rc_hash_iterator_t`'s layout, so `rahash.cpp` defines them
    too, before it includes `rc_hash.h`.
  - Until steps 10 and 11, `.zip`, `.7z` and `.chd` answer "can't be identified yet". rcheevos' verbose messages are
    logged too (`ra: hash: ...`), once a ROM.
  - The fixtures: the PlayStation disc's boot executable `SLUS_000.01` is committed beside the images, so the test
    computes the expected MD5 from its bytes. The ISO is unpadded (55 KB, the raw `psx.bin` 63 KB), and `psx.m3u`
    names the MODE2 cue. The cartridges and the `.nds` are built in the test, and each expected MD5 comes from
    `md5sum`, not rcheevos' own MD5.
  - RetroArch's pin (`71f521e`) carries rcheevos 12.4. What 12.5.0 changed in the hashing (the PS3, arcade folders, a
    disc handed over as a buffer) touches none of the nine systems' rules, so the IDs match. This is noted in
    `DEPENDENCIES.md`.
  - `ci.sh` runs `sha256sum -c` in each `third_party/*/` folder (it fails on a changed byte, tried on a copy) and
    links `units` with the objects `make -C ui kiwami-c` built.
- **2026-10-24, step 10: details the plan left open.**
  - `rahash` has no library, so `hash` takes the system's extensions as a third argument (`exts`, the shelf's
    `def.exts`; none by default, and then an archive has no game). Step 12 passes them.
  - RetroArch's pick, checked at `71f521e`: `content_file_get_path` lists the files whose extension the core takes,
    sorts them with `dir_list_sort` (case aside, `dir_list_casecmp`), and loads the first. `rahash` does the same with
    the system's extensions, archives aside. `several.zip` and `several.7z` hold `readme.txt`, `B.gba` and `a.gba` in
    that order, and the test expects `a.gba`.
  - A solid `.7z` unpacks the whole block its game is in (`SzArEx_Extract`), so that block's size is checked against
    64 MB too, before anything is allocated. A `.zip`'s game is checked on its directory's size, and one that turns
    out longer or shorter than that is damaged.
  - The reasons are worded as step 9's, in lower case after the page's "Can't identify this game:": "no game in the
    archive", "over 64 MB unpacked", "damaged archive", "a DS game can't be identified from a .zip" (or `.7z`).
  - The pick logs `ra: hash: <file> in <archive>` (or `no game in <archive>`), as rcheevos logs only "Hashing <n>
    byte buffer" for it. M5 reads that line.
  - `ci.sh` changes (not in the step's Files): `units` links `ra7z.cpp`, upstream's `zip.cpp` and `-lz`.
  - The truncated archives are cut in the test from the committed ones. The test also checks a changed byte of the
    packed game, a file that isn't an archive, and two `.zip`s whose directory was changed in the test (65 MB; 100
    bytes, shorter than the game).
  - Vendored: the plan's 15 `.c` files and the 14 headers they include on Linux (29 files, each the tag's).
    `7zWindows.h` (`_WIN32` only) and `Ppmd.h`/`Ppmd7.h` (`Z7_PPMD_SUPPORT` only) are left out.
  - `make-fixtures.sh` makes the PlayStation images again too, with new ISO timestamps. The committed ones were kept,
    since their hashes don't change.
  - `make banana-ui-rebuild-rgds` was run too: the decoder builds for the RK3566 with no warning.
- **2026-10-24, step 11: libchdr's LZMA isn't shared, and its zstd is one file.**
  - libchdr's `deps/lzma-26.02` wraps the SDK's `LzmaDec.c` (the same code but for two include lines) in a header that
    renames its symbols `CHDR_LzmaDec_*`, and libchdr's codecs call those names. Sharing step 10's copy would mean
    changing libchdr, so its own is built beside it, as the plan allowed. Step 10's notes, which said the two would
    share it (`lzma/VENDORED`, `DEPENDENCIES.md`), are corrected.
  - libchdr bundles zstd 1.5.7's decompressor as one file, `deps/zstd-1.5.7/zstddeclib.c`, with `ZSTD_DISABLE_ASM`
    set inside it, not as `lib/{common,decompress}`. libchdr carries no licence file for it, so zstd's own `LICENSE`
    from its tag `v1.5.7` is added beside it: the one vendored file not from libchdr's commit.
  - `chdman` wasn't on the host: the user installed `mame-tools` 0.289. `psx.chd` is `psx-mode2.cue` made into a CHD
    with chdman's default CD codecs (its hunks CD LZMA and CD Deflate), and `psx-chd.m3u` names it. Only those two
    lines of `make-fixtures.sh` were run, so the committed PlayStation images kept their bytes.
- **2026-10-24, step 11: details the plan left open.**
  - As decided in step 9, the reader goes into each iterator's `callbacks.cdreader`. It serves every disc: a `.chd`
    through libchdr, anything else through rcheevos' default reader, whose track it wraps. RetroArch instead swaps the
    iterator's callbacks when it opens a `.chd`.
  - A track is found as `chd_stream.c` and `cdfs.c` find it, checked at RetroArch `71f521e`:
    - frames are padded to 4 per track in the file, and sectors counted on the disc without the padding;
    - a pregap stored in the file (`PGTYPE:V...`) is passed over;
    - a raw sector's data starts 24 bytes in when sector 16 has "CD001" at 25 (CD-XA, as on a PlayStation disc), else
      16 bytes in;
    - rcheevos' special tracks (first data, last, largest) are handled too.
  - A `.chd` that can't be opened or read reports "damaged .chd: <libchdr's reason>" to the iterator's error callback
    before rcheevos reports anything, so that is the reason the page shows. A CHD without CD metadata is "damaged
    .chd: unsupported format".
  - The test also checks a changed byte in a compressed hunk (the decompression error), and a file that isn't a CHD.
  - `ci.sh` changes (not in the step's Files): `units` links `rachd.cpp`.
  - `make banana-ui-rebuild-rgds`: no warning; banana-ui is 2,175,136 bytes unstripped, 1,961,208 after step 10. The
    size before and after the feature is measured in step 13.
- **2026-10-24, step 12: signed out, a game is identified and the page asks to sign in.** The plan had the list shown
  without unlocks. But the server gives a game's list (`r=patch`) only to an account: rcheevos sends it with `u` and
  `t`, as upstream's game panel does. Signed out, the game is hashed and its ID asked (`gameid` needs no account), both
  kept, and the page says to sign in on the Account page, with the game's ID (logged `ra: game <id> identified, not
  signed in`). Chosen by the user. `ra-ui.sh` checks that nothing is asked when the ID is on the card.
- **2026-10-24, step 12: the files the page touches.**
  - `kiwami.h` (`Page::RA_GAME`, the page's functions), `kiwami.cpp` (the page hooks' dispatch) and `ra-account.cpp`
    (`raUpdate` drops the badges when the page doesn't show) change, as for the earlier pages.
  - `rastore.h` declares the hash cache (`cachedHash`, `cacheHash`) and the card's `hashes` and `badges` paths.
  - `kiwami/tests/ra/` doesn't change: step 3's `gameid`, `patch` and `unlocks` replies serve the page as they are.
- **2026-10-24, step 12: details the plan left open.**
  - The hash cache keeps a line once the game is hashed, with an empty ID until the server knows one. A hash the
    server doesn't know (ID 0) is asked again next time, since the set may be added. A path with a tab or a newline
    isn't kept. A line is replaced when the file changes (its size or modification time), and the threads' writes are
    serialised.
  - With a password and no token, the request logs in with the password first and keeps the token it gets, as the
    Stats page does.
  - An unlocked achievement is in either `unlocks` answer (the softcore one lists them all), H when the hardcore one
    has it. A locked badge is the unlocked one, dimmed: no `_lock` pictures are fetched.
  - The badges are fetched after the list shows, as a second request. Each row draws the trophy until its badge is on
    the card.
  - An answer for an earlier opening of the page is dropped (`gameRun`).
  - Log lines besides the plan's: `ra: hashed <file> in <ms> ms: <hash>` (M5's time), `ra: <file>: hash <md5> unknown
    to RetroAchievements`, `ra: no achievements for <file>: <why>`.
  - The games page: A, and a second tap on the selected game, open its achievements. Its top panel says so, and an A
    chip shows on its bottom panel.
  - The bench in `ra-ui.sh` follows a warm-up run. The test's game files change the card just before, and the first
    run after that rescans the library, which would make it the slower of the two. With the warm-up, the groups take
    the same time with the group as without it: 0.27 ms at 1024x768, 0.13 ms at 640x480.
  - `ra-ui.sh` also checks that the hash is kept on the card with its ID, that B goes back to the games as they were,
    and the login with no token yet.
- **2026-10-24, step 14: the baseline is the upstream `kiwami` holds, not `main`.** `kiwami` took upstream's `main`
  (`1dcb3e4`) and then upstream's `release-0.4.2` at `e1b2ac4` (`9a8e35e`), which isn't in `main`, and that release takes
  precedence over `main` (the user). Neither alone is the upstream in `kiwami`, and `git merge-tree -X theirs main
  e1b2ac4` (the release winning each conflict) gives a tree that doesn't compile (`settings.cpp`). So, agreed with the
  user:
  - The hooks: this feature's are `git diff b59fab5 HEAD` (the plan's base, `kiwami` before the feature). `docs/FORK.md`'s
    two commands run against that merged tree (`dc6eab9`), in memory, with no ref made.
  - The renders: against `kiwami` before the feature (`b59fab5`, exported with `git archive`), with the release's own
    (`e1b2ac4`) rendered beside them to show what of upstream differs on screen.
- **2026-10-24, a new step 13: the group's icon is RetroAchievements' logo.** After trying the update on the RG DS
  Plus, the user asked for the group's icon to be RetroAchievements' logo, pixelated, instead of step 2's trophy. It is
  hand-drawn: shrinking the logo alone loses its inner lines at 32 px. Chosen with the user:
  - the logo is on the group's tile, the Account and Stats rows, and stands in for a missing avatar;
  - a missing achievement badge keeps the trophy;
  - it is a step of its own, and the final check becomes step 14. The final check's results above were taken before
    it, so step 14 runs them again.

## Progress

A step is marked done in its own commit, so the commit column holds the subject (a commit can't carry its own hash).

| Step | Status | Commit |
|---|---|---|
| 1. The fork's Settings pages, routed by which one is open | done | RetroAchievements: the fork's Settings pages routed by which one is open |
| 2. The RetroAchievements group, the scrolling grid and the trophy, with Mode | done | RetroAchievements: a group in Settings, the grid scrolling to its row, with Mode |
| 3. RetroAchievements' requests, and a stand-in server for the tests | done | RetroAchievements: the server's requests, and a stand-in server for the tests |
| 4. The account on the card: signing in, out, and the remembered accounts | done | RetroAchievements: the account on the card, signed in, out and remembered |
| 5. The Account page | done | RetroAchievements: the Account page, signing in, out and back with remembered accounts |
| 6. The game panel takes the token Banana keeps (a bug fix) | done | RetroAchievements: the game panel takes the token from accounts.conf, where Banana keeps it |
| 7. The Stats page | done | RetroAchievements: the Stats page, points and progress per system, kept for offline |
| 8. The games on this card, from the library | done | RetroAchievements: the games on this card, with the menu's titles, from the library |
| 9. Hashing games as RetroArch does: rcheevos, and the fork's C build | done | RetroAchievements: rcheevos' hashing, vendored, and the fork's C build in banana-ui |
| 10. Hashing games in .zip and .7z: the LZMA SDK's 7z decoder | done | RetroAchievements: games in .zip and .7z hashed as RetroArch does, with the LZMA SDK's 7z decoder |
| 11. Hashing PlayStation .chd: libchdr | done | RetroAchievements: PlayStation .chd hashed through libchdr |
| 12. A game's achievements | done | RetroAchievements: a game's achievements, locked and unlocked, from the games on this card |
| 13. The group's icon: RetroAchievements' logo | done | RetroAchievements: the group's icon is RetroAchievements' logo, pixelated |
| 14. Final check | done | RetroAchievements: the final check done |
