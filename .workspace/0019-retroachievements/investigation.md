# 0019 RetroAchievements: investigation

Issue: #19 · Branch: `k/0019-retroachievements` · Status: refined · Confidence: 94% (proceed)

## The request

Settings has no RetroAchievements place: no way to sign in that can be found (it is step 4 of System > First-time
setup), no way to sign out, and nothing that shows the account. The user wants a RetroAchievements place in Settings
to sign in and out and see the account's stats.

Decided with the user (question rounds 1 and 2):

- **Its own Settings group**, "RetroAchievements", a seventh tile after System. The group grid stays three tiles wide
  and shows two rows at a time. It scrolls by one row: moving down onto the third row shows rows 2 and 3, and moving
  back up onto the first row shows rows 1 and 2. Upstream's screens stay as they are until the cursor reaches the
  seventh tile.
- **The account, Google-like.** Sign in (name, then password, checked with the server). Sign out removes the password
  and token from the card, but the account is remembered in a list of signed-out accounts, with its name and avatar.
  Picking a remembered account fills in the name and asks for the password again. Any remembered account can be
  removed. "New account" signs in another one.
- **Stats**, fetched when the page opens (no polling, Wi-Fi needed), the last ones kept on the card and shown offline
  with their date:
  - points and profile: hardcore and softcore points, the avatar, unread messages;
  - progress for each of Banana's systems: games started, games completed or mastered, achievements unlocked;
  - games on this card: unlocked/total for the games Banana has recorded progress for (play stats), under the titles
    the menu shows (read from the library through a one-line hook).
- **A game's achievements**, opened from the games on this card: the full list, locked and unlocked, with badges. The
  game is identified by hashing its ROM the way RetroArch does (rcheevos' hash code, a new dependency), and the result
  is cached per ROM. `.zip` and `.7z` cartridge games are unpacked first, as RetroArch does. For `.7z` that means the
  LZMA SDK's 7z decoder, a second new dependency. PlayStation `.chd` discs are read through libchdr, a third new
  dependency, as RetroArch reads them.
- **The group's tile** shows a pixel trophy drawn by the fork's code.
- **Softcore/Hardcore** also in the new group: the same setting as upstream's row in Other systems, which stays.
- **The token is kept.** The token the server returns at sign-in is stored in `accounts.conf` as `ra_retroarch_token`,
  so the stats and RetroArch don't send the password each time.
- **The game panel's token is fixed.** The panel takes the token from `accounts.conf` when `retroarch.cfg` has none,
  which is always, since Banana scrubs it out of there.
- **The first-time setup's step stays upstream's.** It writes the same `accounts.conf` the group reads.

Out of scope (later proposals): checking every ROM in the library against the server; RetroArch's pop-up and unlock
sound options; a game's achievements from the library's game page (X on a game); recording RetroArch's progress when
Second screen is off. Those RetroArch games don't appear among the games on this card, and the page says so.

Upstream has an open, unanswered feature request for a "RetroAchievements menu" (opened 22 Oct 2026). It asks for
managing the settings and viewing achievements, and lists ROCKNIX's game index, unlock sounds and verbose mode. Upstream
may build its own later, which is one more reason the fork's code stays in its own files.

## Oversights in the requirements

1. **The group grid is full.** Settings opens on a grid of group tiles, three wide and two high. The six groups fill
   it, and `renderGroups` draws only the first six (`ui/src/settings.cpp:1371,1373`). A seventh group would exist (L/R
   in a list reaches it, `:949-956`) but be invisible on the grid. The keys assume full rows: UP/DOWN is
   `(gsel ± 3) % n` (`:972`), which with seven groups jumps from System (5) to Nintendo DS (2) on DOWN, not to the
   seventh tile. The touch grid (`:1022-1032`) and the animation (`:1321,1332`) also use `g % 3`, `g / 3` and fixed
   tile positions. *Decided*: the grid scrolls by rows, two shown at a time (see *The request*). This is the one
   larger hook into upstream's code (see *What has to change*).

2. **There is no icon for it.** A group's tile shows its first row's icon (`:1376`), from `ui/assets/rnds/icons/`
   (`:1201`), which is ROCKNIXDS's imported folder the fork can't add to. It has no trophy, and the star (`fav`)
   already means Favourites (Y in the library, `main.cpp:318`). *Decided*: the fork's code draws a pixel trophy, the
   way the game panel draws its tile icons (`settings.cpp:3279-3292`). The tile drawing calls the fork for that
   group's icon, through the grid hook (hook 2).

3. **Signing out has to reach four places, in order.** The account lives in:
   - `banana/accounts.conf` (`ra_user`, `ra_password`, `ra_retroarch_token`);
   - `retroarch.cfg` (`cheevos_username`, `cheevos_enable`). `run-retroarch:96-102` turns achievements off at the next
     game when `accounts.conf` exists and has no `ra_user`;
   - SuperDrastic's `/data/superdrastic/ra.token` (username and token, `superdrastic src/ra.c:402-406`);
   - BananaDS's `/data/bananads/ra.token` (`bananads src/ra.c:374-378`).

   A game cut short can also leave the password and token in `retroarch.cfg`, or in RetroArch master's
   `retroarch-keychain.cfg`. `ra-secrets scrub` moves them into `accounts.conf` ("the last login wins",
   `ra-secrets:19-27`). *Decided* (proposed, accepted with the report): sign out first runs `ra-secrets scrub`, so nothing comes back afterwards, then
   clears the three `accounts.conf` keys, sets `cheevos_enable = "false"` and an empty `cheevos_username` in
   `retroarch.cfg` (so the game panel says "off" at once), and deletes both `ra.token` files. The DS emulators already
   fall back to "off" with no account in their environment (`run-drastic:42-45`, `run-bananads:101-104`).

4. **An account can change behind the group's back.** Signing in through RetroArch's own menu, or through the setup's
   step, replaces `accounts.conf`'s account (`ra-secrets:20-24`, `settings.cpp:2767-2778`). *Decided* (proposed, accepted with the report): the group keeps
   the last account it saw. When it opens and `ra_user` differs, the old account goes to the remembered list, as if it
   had been signed out (its password is already gone, replaced).

5. **Upstream's setup check doesn't keep the token.** `startRaCheck` (`:2907-2970`) reads `Success`, `User` and
   `Score` from `login2` and drops the `Token`. A sign-in done in the setup leaves `accounts.conf` with a password and
   no token, as today. *Decided* (proposed, accepted with the report): no hook in upstream's check. When the group opens with a password and no token, it
   runs its own `login2` once and stores the token. The group's own sign-in stores the token directly.

6. **The game panel never gets a token.** `gameOpen` takes `cheevos_token` from `retroarch.cfg` (`:3428`, again at
   `:3453`), but `run-retroarch` puts the token only in `/run/banana/retroarch-account.cfg` (`run-retroarch:99`), and
   `ra-secrets` removes it from `retroarch.cfg` (`ra-secrets:30`). The panel's `r=patch` and `r=startsession` requests
   (`:3337,3362`) then go out with an empty `t=`. This was found in 0007 (its oversight 5) and left out there.
   *Decided*: fixed here, with one `kiwami:` line after `:3428` that takes `ra_retroarch_token` from `accounts.conf` when
   the token is empty and `ra_user` is the panel's user. To be confirmed on the device (the panel's achievements show).

7. **RetroArch's progress is recorded only with Second screen on.** The library's unlocked/total comes from
   `/run/banana/ra-progress`, written by the emulator (BananaDS: `bananads src/ra.c:288-300`) or, for RetroArch, by the
   game panel after the game (`main.cpp:1458`). RetroArch itself doesn't write it. With Second screen off, RetroArch
   games never get progress (`library.cpp:767` keeps the last one). *Decided*: accepted. The page's help says RetroArch
   games show up after a game played with Second screen on.

8. **Play stats know file names, not titles or paths.** `playstats.tsv` keys a game by `<system>/<rel>`
   (`library.cpp:321,583`); a second-card game's `rel` starts with `@sd2/` (`:556`). `Settings` has no access to the
   `Library` (nothing in `settings.h`). The list needs a title to show and the ROM's full path to hash. The menu's
   title comes from three places, the last that has one winning (`library.cpp:560-590`):
   - the file name cleaned of `_` and trailing `(...)`/`[...]` tags (`cleanTitle`, `:48`);
   - the gamelist's name;
   - the user's own title (Settings' game page).

   *Decided*: one `kiwami:` line in `main.cpp`, where the `Library` and `Settings` both exist, gives the fork's code
   read access to the library. The list then has exactly the menu's titles, each game's full `path` (the second card
   included) and its `achA`/`achB`, with nothing re-derived. The library is shared with the worker thread under
   `worker.uiLock` (`main.cpp:1456`), so the planning stage checks that Settings' calls already run under that lock,
   or takes it. A game whose file is gone is no longer in the library, so it drops out of the list.

9. **Hashing has per-system rules, and archives.** RetroArch's game ID comes from rcheevos' hash, which differs by
   console: the whole file for most cartridges (with headers skipped for NES and SNES), the header and the ARM9/ARM7
   binaries for the DS, the boot executable from the disc for the PlayStation. For a `.zip` cartridge, RetroArch hashes
   the unpacked member. `ui/src/zip.cpp` reads one member in order, which is enough for a cartridge buffer (64 MB at
   most, the N64). A zipped DS game needs seeking into a 64-512 MB member. `.7z` cartridges (the fork's 0003 lists
   them on seven systems, `kiwami/post-build.sh:25-28`) have no reader in banana-ui: the scraper skips them too
   (`ui/src/scrape.cpp:335`). The image's `liblzma` (xz) decodes raw LZMA streams but not the 7z container (file list,
   solid blocks, filters). RetroArch opens them with Igor Pavlov's LZMA SDK, which it vendors as `deps/7zip` (19.00:
   LZMA, LZMA2, BCJ/BCJ2, ARM and Delta filters, no PPMd) and hashes the unpacked game.
   *Decided*:
   - `.zip` cartridges are hashed through `zip.cpp` into a buffer;
   - `.7z` cartridges are hashed through the LZMA SDK's 7z decoder into a buffer (a new dependency, below), so they
     get the same ID RetroArch does;
   - plain `.nds`, and `.cue`/`.bin`/`.iso` (and an `.m3u` naming them, `hash.c:1167`) go through rcheevos' own disc
     reader;
   - `.chd` goes through libchdr, given to rcheevos as its disc reader, the way RetroArch reads it. PCSX-ReARMed
     carries the same library (`deps/libchdr`, with LZMA SDK 26.02, miniz and zstd beside it);
   - zipped DS games say "can't be identified from a .zip" (no DS system lists `.7z`);
   - PlayStation `.pbp`, `.img` and `.mdf` say they can't be identified. rcheevos hashes `.pbp` only for the PSP
     (`hash_disc.c:1033`), so RetroArch can't identify a PS1 `.pbp` either, and that game has no achievements in-game.
     `.img` and `.mdf` aren't formats its reader knows.

10. **The network and sleep.** Every request runs on a detached thread with libcurl `dlopen()`ed, as upstream's check
    and game panel do (`:2863-2885`, `:3234-3255`), with a 20 s timeout. If the handheld sleeps mid-request, Wi-Fi goes
    down, the request fails or times out, and the page shows the cached stats with their date. After resume, banana-ui
    is woken (SIGUSR1) and the page doesn't refetch by itself. *Decided* (proposed, accepted with the report): Y refreshes, and the stats are fetched once
    each time the group's page opens.

11. **Two kinds of points.** `login2` returns `Score` (hardcore) and `SoftcoreScore`
    (`rcheevos rc_api_user.c:57-66`), and `allprogress` returns `Unlocked` and `UnlockedHardcore` per game
    (`:418-420`). *Decided* (proposed, accepted with the report): show both, labelled, with "completed" (all unlocked in softcore) and "mastered" (all in
    hardcore) counted apart, as the site does.

12. **Small design details.** *Decided* (the defaults, accepted by the user):
    - the stats list all nine systems with achievements and a launcher (DS, GBA, GBC, GB, NES, SNES, Mega Drive,
      PlayStation, N64), and one with nothing yet says "No games started" instead of being hidden;
    - the group's Softcore/Hardcore row is labelled "Mode", apart from the group's name; upstream's row in Other
      systems keeps its name;
    - the cached stats and avatar stay with a remembered account (shown while its password is typed), and removing it
      deletes them; the ROM-hash cache belongs to no account and stays;
    - "New account", or picking a remembered one while signed in, signs the current account out first (it goes to
      the remembered list);
    - each unlocked achievement in a game's list says whether it was unlocked in hardcore or softcore;
    - the fork's new text is in English only: the translation files are upstream's, and untranslated lines already
      fall back to English.

## Suggestions

1. **No hook for the group's rows.** The group's rows are added by the fork's existing `Kiwami::rows` with a new
   section name, called once after System's rows. That is one `kiwami:` line at the end of `build()`
   (`settings.cpp:~505`); the rows' pages go through the `KIWAMI` page hooks that already exist (`:774,930,997,1401,
   1481,1637,1645`). The Softcore/Hardcore row is an ordinary `CHOICE` item on `retroarch.cfg`'s
   `cheevos_hardcore_mode_enable`, saved by upstream's generic `save()`, exactly as upstream's own row
   (`:447-448`). *Taken.*

2. **Stats cached for offline.** *Taken* (the user's pick). The cache is a plain file on the card
   (`/data/banana/cache/ra-stats`).

3. **Keep the token from the sign-in.** *Taken* (the user's pick). With a token, `run-retroarch` gives RetroArch both
   password and token (`:99`), and the stats page and the game list send `t=` instead of the password.

4. **Fix the game panel's token.** *Taken* (the user's pick, oversight 6).

5. **One curl helper for the fork.** Upstream's `Curl` loader, `raPost` and `fetchFile` are in an anonymous namespace
   in `settings.cpp` (`:2863-2885`, `:3234-3277`), out of the fork's reach. *Taken*: the fork has its own small copy
   in `ui/src/kiwami/` (about 60 lines), rather than a hook that exposes upstream's.

6. **Later, not now:** a game's achievements from X on a game (a hook in upstream's game page); checking the whole
   library's ROMs against the server; RetroArch's pop-up and unlock-sound switches (`cheevos_verbose_enable`,
   `cheevos_unlock_sound_enable`); recording RetroArch's progress without the game panel. Each is listed in the issue
   as out of scope.

## Current state

**Signing in** is step 4 of the first-time setup (`settings.cpp:3047`, the step's text `:3074-3087`), reached again
from Settings > System > First-time setup (`:499`). It types `ra_user` then `ra_password` into
`/data/banana/accounts.conf` (`:2767-2778`), then `startRaCheck` sends `r=login2` to
`https://retroachievements.org/dorequest.php` on a thread (`:2907-2970`), and a toast says "Logged in as <name> ·
<points>" or why not (`:1557-1565`). The headless test types an account there and checks `accounts.conf`
(`ui/tests/run.sh:82-98`, with `BANANA_RA_CHECK=0`, no server).

**The account's users:**
- `run-retroarch:87-102` puts `cheevos_username` and `cheevos_enable` into `retroarch.cfg`, and the password and
  `ra_retroarch_token` into `/run/banana/retroarch-account.cfg` (umask 077), appended to RetroArch's config.
  `ra-secrets scrub` (`ra-secrets:14-31`, the fork's `ra-keychain` hook first) moves whatever RetroArch saved into
  `accounts.conf` before and after each game.
- `run-drastic:41-45` and `run-bananads:99-104` pass `ra_user`/`ra_password` in the environment. Both emulators cache
  a token in `<data>/ra.token` and use it while its user matches.
- The game panel (`settings.cpp:3198-3610`) reads RetroArch's log for the game ID and unlocks, and asks the server for
  the game's list and the account's unlocks with `cheevos_username`/`cheevos_token` from `retroarch.cfg` (oversight
  6). It downloads badges to `/data/banana/cache/ra-badges` (`:3313-3326`) and decodes PNGs with
  `rnds::decodeFile` (`:3570`). That is the same machinery the avatar and a game's list need.
- `share:50` vetoes `accounts.conf` and `ra.token` on file sharing. `ra-reset` keeps the account (`ra-reset:7-30`).

**Mode:** Other systems > RetroAchievements is Softcore/Hardcore on `cheevos_hardcore_mode_enable`
(`settings.cpp:447-448`), Softcore by default (`system/rootfs/etc/banana/retroarch.cfg:52-54`).

**Progress in the library:** `playstats.tsv` holds `<system>/<rel> plays seconds last [unlocked total]`
(`library.cpp:321-333`), written by `recordPlay` (`:755-771`) from `/run/banana/ra-progress` after each game
(`main.cpp:1417-1487`). The Carts theme and the game info show it (`carts.cpp:908-913`, `library.cpp:840-845`).

**Settings' groups** are made from the rows' sections in order (`settings.cpp:678-683`). The grid is three by two,
drawn and touched at fixed positions (`:1022-1032`, `:1342-1393`). Upstream's tests reach groups with R presses from
the first one (`run.sh:45` "R four times is Network", `:206` "five R's" for System). A seventh group at the end doesn't
change those counts.

**The fork's Settings code** (`ui/src/kiwami/`, `kiwami.h`): `Settings::Kiwami`, nested in `Settings`, adds rows per
group (`kiwami.cpp:61-90`), owns the `KIWAMI` page and the `ENTRY_KIWAMI` keyboard entry, and draws with Settings'
helpers. Tailscale's page (`tailscale.cpp`) is the model: rows on the bottom panel, state and help on the top, a
background job.

**What the server offers with name and token** (rcheevos 12.5, vendored by BananaDS):
- `r=login2` → `User`, `Token`, `Score`, `SoftcoreScore`, `Messages`, `AvatarUrl` (`rc_api_user.c:57-66`);
- `r=allprogress&c=<console>` → per game `Achievements`, `Unlocked`, `UnlockedHardcore` (`:380-420`);
- `r=gameid&m=<hash>` → the game's ID;
- `r=patch` → its achievements (title, description, points, badge), as the game panel already uses;
- `r=startsession`/`r=unlocks` → the account's unlocks;
- `r=getfriendlist` → followed users (not used).

Rank, recent games and recent unlocks need the separate Web API key and are not used.

**Upstream (`main`, and `release-0.4.2`, which `kiwami` merged in `9a8e35e`):** the same account code, the setup with
one step fewer (no DS name: `git show main:ui/src/settings.cpp`, `:2425`). `git log main -- ui/src/settings.cpp` shows
large, frequent changes (the last three are feature-sized). Upstream has no RetroAchievements group or branch, only the
open request.

## What has to change

New, in the fork's places:

- `ui/src/kiwami/ra*.cpp`/`.h`: the group's rows and pages:
  - **Account**: the signed-in account (avatar, name, points), Sign out, and the remembered accounts with "New
    account". A on a remembered one asks for its password. Remove takes A twice.
  - **Stats**: points and profile, then per system.
  - **Games on this card**, and a game's achievements from there.
  - **Softcore/Hardcore**.

  Text entry goes through `ENTRY_KIWAMI` (upstream's keyboard).
- `ui/src/kiwami/`: the fork's curl helper (suggestion 5), JSON through the fork's existing `json.cpp`, the requests
  on threads, the caches. The fork's files on the card:
  - `/data/banana/ra-accounts`: the remembered accounts (name, avatar file) and the last account seen. No secrets.
  - `/data/banana/cache/ra-avatars/`
  - `/data/banana/cache/ra-stats`
  - `/data/banana/cache/ra-hashes`: `<path> <size> <mtime> <hash> <game id>`.
- rcheevos' hash code (`src/rhash/`: `hash.c`, `hash_rom.c`, `hash_disc.c`, `cdreader.c`, `md5.c`, with `rc_util.c`
  and the headers it needs), vendored into `ui/src/kiwami/` unchanged at rcheevos 12.5.0, or built from a pinned
  tarball. It goes in `kiwami/DEPENDENCIES.md` (MIT). `hash_encrypted.c`/`aes.c` (3DS) and `hash_zip.c` (MS-DOS and
  arcade) are left out if the build allows (`RC_HASH_NO_ENCRYPTED`, `RC_HASH_NO_ZIP`), to be checked in planning.
- The LZMA SDK's 7z decoder (`7zArcIn.c`, `7zDec.c`, `7zBuf.c`, `7zCrc.c`, `7zCrcOpt.c`, `7zStream.c`, `LzmaDec.c`,
  `Lzma2Dec.c`, `Bcj2.c`, `Bra.c`, `Bra86.c`, `BraIA64.c`, `Delta.c`, `CpuArch.c` and their headers, no encoder),
  vendored into `ui/src/kiwami/` unchanged. Public domain. It is listed in `kiwami/DEPENDENCIES.md` with its version:
  RetroArch's 19.00, so both open the same archives, or 26.02, the version libchdr carries, so the two share one
  `LzmaDec.c`. That choice is left to planning. It unpacks a `.7z` cartridge's first game file (the one RetroArch
  would pick) into memory, 64 MB at most.
- libchdr (BSD-3), at the commit PCSX-ReARMed `c881679` carries in `deps/libchdr` (its version read from there in
  planning). It is vendored or built as a package in `kiwami/package/`, whichever builds into banana-ui more simply,
  and listed in `kiwami/DEPENDENCIES.md`. Its bundled zstd and miniz come with it unless the image's zlib (and zstd,
  if present) can stand in. A small adapter gives rcheevos a disc reader (`rc_hash_init_custom_cdreader`) that reads
  `.chd` through it and falls back to rcheevos' own for `.cue`/`.bin`/`.iso`.
- The trophy icon, drawn as a pixel map in the fork's code (oversight 2).
- `kiwami/tests/`: the group headless (a stand-in server, below), sign out's effects on a fixture card, the hash
  against known hashes.

Hooks in upstream's files (each marked `kiwami:`):

1. `ui/src/settings.cpp` `build()`: one line, the group's rows after System's (`Kiwami::rows(*this, ...)`).
2. `ui/src/settings.cpp`, the group grid: scrolling by rows. Its drawing (`:1371-1376`), keys (`:968-973`), touch
   (`:1025-1027`) and animation (`:1321,1332`) take a first-row offset. The rows-of-three arithmetic for a last row
   that isn't full is the fork's function, called from those places, and so is the trophy for the group's tile
   (`:1376`, where upstream draws the PNG icon). This is the only hook of more than one line, and it can't be avoided
   once the group is a tile. With six groups (`main`) the offset is always 0, so upstream's render is unchanged.
3. `ui/src/settings.cpp` `gameOpen`: one line, the token from `accounts.conf` (oversight 6).
4. `ui/src/main.cpp`: one line giving the fork's code read access to the `Library` (oversight 8).
5. Possibly `ui/Makefile`: rcheevos', the LZMA SDK's and libchdr's C files, if `src/kiwami/*.cpp` doesn't pick them up
   (they are `.c`). One line next to the existing `kiwami:` lines.

No setting is needed to keep upstream's behaviour. The group adds a tile and touches nothing of upstream's until it's
used, and the game panel's token line only fills a value that is always empty today. The grid's look with seven
groups, scrolled, is new.

Nothing changes in the launchers, `ra-secrets`, the setup, or the rootfs (other than banana-ui's size). libcurl is
already in all three defconfigs (`banana_rgds_defconfig:93`). The image's packages change only if libchdr ends up as a
package in `kiwami/package/` rather than vendored.

## Options

1. **Recommended (decided): the fork's own group, pages and hashing, with four hooks plus the Makefile line.** It
   covers everything the user asked for, the fork's code stays in `ui/src/kiwami/`, and the hashing is the same code
   RetroArch and the DS emulators use, so the IDs match.
2. A row in Other systems instead of a group: no grid hook at all, but less visible. *Not chosen* by the user.
3. The grid four tiles wide: no scrolling, but upstream's look changes for everyone and the tiles get narrow. *Not
   chosen.*
4. Game IDs only from games played (no hashing): no new dependency, but most games would have no list (only RetroArch
   with Second screen on logs the ID today). *Not chosen.*

## Risks

- **Performance and footprint.** Nothing runs while idle: requests only when the group's pages open or Y is pressed,
  on threads, as upstream's check does. The menu's frame time is unchanged outside the group. Hashing reads up to
  64 MB for an N64 cartridge, a few MB for a DS game and part of a disc for the PlayStation, once per ROM (cached), on
  a thread, with "Identifying..." shown. A `.7z` cartridge is unpacked into memory first, up to 64 MB for an N64 game:
  a few seconds of LZMA on the RK3566, once (cached). A `.chd` is read only as far as the boot executable, a few hunks
  decompressed. rcheevos' hash code, the 7z decoder and libchdr (with its zstd) add roughly 300-400 KB to banana-ui
  together (to measure; less if libchdr shares the LZMA SDK and uses the image's zlib). banana-ui stays one binary,
  loaded at boot as now: the menu's frame time and idle activity don't change, and its resident memory grows by the
  code pages actually touched. Avatars (about 10 KB each) and badges are on the card.
- **Three vendored libraries.** Each is pinned and listed in `kiwami/DEPENDENCIES.md`, and a bump has to recheck it:
  rcheevos (to match the hashes RetroArch and the DS emulators compute), the LZMA SDK, and libchdr (against
  PCSX-ReARMed's pin). If upstream's RetroArch or PCSX-ReARMed move, the fork's copies don't follow by themselves.
- **Conflicts with upstream.** `settings.cpp` changes often and heavily, and the grid hook sits in `renderGroups`,
  `pressGroups` and the touch code, which upstream could restyle. If upstream adds a seventh group or its own
  RetroAchievements menu (the open request), the fork's group and the hook have to be reconsidered at that merge. The
  `gameOpen` line conflicts if upstream fixes the token itself, which would make the line unnecessary.
- **The server.** `allprogress`, `gameid` and `AvatarUrl` are what rcheevos 12.5 uses but aren't checked against the
  live server yet. One stats refresh is about 11 requests (`login2`, nine `allprogress`, the avatar); RetroAchievements
  rate-limits, so a refresh is never automatic in a loop. The user agent follows upstream's (`BananaOS/0.2 rcheevos`).
- **Credentials.** Nothing new is stored beyond what upstream stores: password and token in `accounts.conf`, which file
  sharing hides. The remembered list holds names only. Signing out removes more than anything does today (oversight
  3). An exFAT card read on a computer shows `accounts.conf`, as today.
- **Boards and panels.** Settings draws at 640x480 and scales (`X()`), the same on `rgds` (640x480) and `rgds-plus`
  (1024x768), and touch works on the bottom panel. Nothing is board-specific. The second card exists only on the
  RG DS Plus (`settings.cpp:497`), and the game list's path resolution handles its absence.
- **First boot and updates.** A new card has no account and no fork files: the group shows "Not signed in" and an
  empty remembered list. A card set up by an older version has `accounts.conf` from the setup, which the group
  adopts, getting a token once (oversight 5). An A/B update keeps everything on `/data`.
- **Games.** Nothing changes in how games run, apart from the game panel getting its token, which only makes requests
  succeed that fail today.

## Testing

Headless (`kiwami/tests/`, run by `kiwami/tests/ci.sh` and next to `make test`):
- the group: seven tiles, DOWN from System scrolls to the RetroAchievements tile (shots of both scroll positions at
  640x480 and 1024x768), the existing R-count navigation in `run.sh` still passes, and with the fork's rows absent the
  grid renders as `main`'s (PNG compare);
- sign in, sign out and the remembered list against a stand-in server. The fork's requests take their base URL from
  an environment variable (as `BANANA_RA_CHECK` and `KIWAMI_TAILSCALE` stand in for others) pointing to a small local
  HTTP server, or to canned `file://` replies. The test checks `accounts.conf`, `retroarch.cfg`, both `ra.token` files
  and `ra-accounts` after each step;
- the hash: known ROM fixtures (tiny homebrew-sized files built in the test) hashed by the fork's code against
  rcheevos' own `rhash` test vectors or a hash computed by RetroArch on the host. The same fixture plain, as a `.zip`
  and as a `.7z` (LZMA and LZMA2, made with the host's `7z`) gives the same hash. A tiny PlayStation disc image as
  `.cue`/`.bin` and as `.chd` (made with the host's `chdman`, if present; otherwise a checked-in fixture of a few KB)
  gives the same hash;
- the games on this card from a fixture library: the menu's titles (a renamed one, a gamelist name, a cleaned file
  name), a name with spaces, and a second-card game.

On the device (the checklist in the plan), with banana-ui's log (`/data/banana/logs/`) and the fork's request log
lines (status and timing only, never `p=` or `t=`):
- sign in, see points and the avatar, sign out, check that a RetroArch game and a DS game run without achievements,
  and sign back in from the remembered list;
- the stats per system and the games on this card, with the Wi-Fi off (the cached copy, dated) and on;
- a game's list for a GBA, an NES (headered), a DS (`.nds`), a zipped GB, a `.7z` SNES or N64 game, and a PlayStation
  game as `.cue` and as `.chd`, checked against the game panel's or RetroArch's own identification, and the time the
  `.7z` takes to
  identify the first time;
- the game panel with Second screen on: its achievement list now loads (oversight 6), from RetroArch's log
  `/run/banana/retroarch.log` and banana-ui's;
- sleep while the stats load, and after resume.

## Open questions

1. Which icon for the group's tile? *Answered*: a pixel trophy drawn by the fork's code (the star already means
   Favourites).
2. Game names in the games on this card? *Answered*: the menu's own titles, through a one-line hook in `main.cpp`
   that gives the fork read access to the `Library`, with its paths and progress.
3. PlayStation `.chd`? *Answered*: included, read through libchdr (a third dependency, PCSX-ReARMed's pin). `.pbp`
   can't be identified, by RetroArch either (rcheevos hashes `.pbp` only for the PSP).
4. Where should the account live in Settings? *Answered*: its own group, a seventh tile, the grid scrolling by rows.
5. Which stats? *Answered*: points and profile, progress per system, games on this card. Not rank or recent activity
   (they need the Web API key).
6. What does Sign out do? *Answered*: removes the credentials and remembers the account (name and avatar) in a list.
   Picking one asks for its password; any can be removed.
7. Re-signing in to a remembered account? *Answered*: asks for the password.
8. Extras from upstream's request? *Answered*: only a game's achievements list, from the games on this card, by
   hashing. Not the library check, pop-up or sound options, or the game page.
9. The Softcore/Hardcore row? *Answered*: in both places.
10. The setup's step? *Answered*: stays upstream's.
11. RetroArch's progress without Second screen? *Answered*: accepted, and the page says so.
12. Additions? *Answered*: stats shown offline, the sign-in's token kept, and the game panel's token fixed.
13. `.7z` cartridge games: identified too? *Answered*: yes. The LZMA SDK's 7z decoder (public domain, the one
    RetroArch vendors) goes into `ui/src/kiwami/` and `kiwami/DEPENDENCIES.md`.
14. The small design details (oversight 12)? *Answered*: the defaults.

### Confidence

| Factor | Weight | Score |
|---|---|---|
| The request is clear | 20% | 100% (confirmed in two rounds) |
| The current state is understood | 30% | 90% (traced with `path:line`; the server's `allprogress`/`gameid`/avatar replies are from rcheevos, not tried live) |
| The files to change are known | 20% | 85% (the hooks and the fork's files are known; how the three C libraries build into banana-ui, vendored or as a package, is to check) |
| The way to do it is settled | 20% | 100% (chosen by the user) |
| Nothing blocking | 10% | 100% (no open questions) |

Weighted: 20 + 27 + 17 + 20 + 10 = **94%**: **proceed**. The main risk is the grid hook in upstream's frequently changed
`settings.cpp` (*Risks*).
