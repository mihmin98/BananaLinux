#!/bin/sh
# The RetroAchievements group in banana-ui's Settings, headless at both panel sizes: its rows (Account, Mode) after
# System; the groups' grid showing two rows, the first two exactly as with six groups (KIWAMI_RA_GROUP=0 leaves the
# group out: upstream's grid), scrolling one row to the trophy's and back, wrapping with a short last row; taps on what
# shows; Mode and Other systems > RetroAchievements kept in step. The Account page against the stand-in server
# (ra-server.py): signing in (accepted, refused, with no connection and the token fetched later), out (A twice, the
# scrub first), back in from the remembered accounts, removing one (X twice), an account adopted from accounts.conf.
# The Stats page: the requests it makes on opening and on Y and none otherwise, the card's copy when the server can't
# be reached, nothing asked when signed out. The games on this card: the library's games with achievements, under the
# menu's titles. A game's achievements: the game hashed and identified once (kept on the card), its list and unlocks,
# the badges fetched once; a game that can't be identified, one the server doesn't know, no connection, signed out.
# The groups' frame time with the group and without. No password or token in the log. Needs ui/banana-ui (make ui),
# python3 and ImageMagick (the test library's art).
# Run by kiwami/tests/ci.sh when banana-ui is built, or by hand:
#   kiwami/tests/ra-ui.sh        (the shots in $OUT, /tmp/kiwami-ra by default)
set -e
cd "$(dirname "$0")/../.."
UI=$PWD/ui
OUT=${OUT:-${TMPDIR:-/tmp}/kiwami-ra}
FX=$OUT/fixture
rm -rf "$OUT"; mkdir -p "$OUT"
"$UI/tests/fixture.sh" "$FX" >/dev/null 2>&1
fail=0
# the stand-in RetroAchievements server, and one that never answers (no connection); every run talks to them only
SLOG=$OUT/server.log
python3 kiwami/tests/ra-server.py "$OUT/port" "$SLOG" & SERVER=$!
KIWAMI_RA_SERVER_DOWN=1 python3 kiwami/tests/ra-server.py "$OUT/port-down" "$OUT/server-down.log" & DOWN=$!
trap 'kill $SERVER $DOWN 2>/dev/null' EXIT
for i in $(seq 50); do [ -s "$OUT/port" ] && [ -s "$OUT/port-down" ] && break; sleep 0.1; done
[ -s "$OUT/port" ] && [ -s "$OUT/port-down" ] || { echo "FAIL: the stand-in server didn't start"; exit 1; }
URL=http://127.0.0.1:$(cat "$OUT/port")
export KIWAMI_RA_URL=$URL KIWAMI_RA_MEDIA=$URL
URL_DOWN=http://127.0.0.1:$(cat "$OUT/port-down")
# the card beside Settings' data folder: RetroArch's settings and the DS emulators' tokens, which signing out clears;
# the scrub's stand-in notes whether the account was still there when it ran
D=$FX/data
mkdir -p "$FX/retroarch" "$FX/superdrastic" "$FX/bananads"
printf 'if grep -q "^ra_password=" "%s/accounts.conf"; then echo keys; else echo none; fi >> "%s/scrubbed"\n' "$D" "$OUT" > "$OUT/scrub.sh"
export KIWAMI_RA_SCRUB="sh $OUT/scrub.sh"
# Settings opens on its groups (Display & sound, Sleep & power, Nintendo DS / Other systems, Network, System /
# RetroAchievements): R five times is System, four Network. (A shot draws, and the grid's rows shown follow the
# selection when it is drawn or tapped. The pick slides to the selection over the frames after a move, and a shot is
# one frame: shots compared are each the first of their run, or of the run's group's list.)
SET="home 0; press start"
R4="press r; press r; press r; press r"

# run <size> <name> <script>: one run of banana-ui, its log in $OUT/<size>-<name>.log
run() {
    size=$1 name=$2 log=$OUT/$1-$2.log
    "$UI/banana-ui" --headless "$OUT/$size-" --size $size --assets "$UI/assets/rnds" \
        --systems "$PWD/system/rootfs/etc/banana/systems.conf" --roms "$FX/roms" --data "$FX/data" \
        --script "$3" 2> "$log" || { echo "FAIL $size $name: banana-ui exited $?"; fail=1; }
}
logged() { grep -qxF -- "$1" "$log" || { echo "FAIL $size $name: no '$1' in the log"; fail=1; }; }
unlogged() { ! grep -qF -- "$1" "$log" || { echo "FAIL $size $name: '$1' in the log"; fail=1; }; }
# the log's lines starting with $1 are exactly these (one a line, "|" after each)
lines() {
    got=$(grep -F -- "$1" "$log" | tr '\n' '|')
    [ "$got" = "$2" ] || { echo "FAIL $size $name: '$1' lines are '$got', not '$2'"; fail=1; }
}
shown() {
    for s in "$@"; do
        for p in top bot; do
            f=$OUT/$size-$s-$p.png
            [ -s "$f" ] && [ "$(convert "$f" -format '%[fx:standard_deviation>0.01]' info: 2>/dev/null)" = 1 ] || { echo "FAIL $size $name: $f missing or blank"; fail=1; }
        done
    done
}
same() { cmp -s "$OUT/$size-$1-$3.png" "$OUT/$size-$2-$3.png" || { echo "FAIL $size $name: $1 and $2 differ ($3)"; fail=1; }; }
differ() { cmp -s "$OUT/$size-$1-$3.png" "$OUT/$size-$2-$3.png" && { echo "FAIL $size $name: $1 and $2 are the same ($3)"; fail=1; }; true; }
# a file's content is exactly $2 (printf's format), or the file isn't there ($2 "-")
holds() {
    if [ "$2" = - ]; then [ ! -e "$1" ] || { echo "FAIL $size $name: $1 is there"; fail=1; }; return; fi
    [ "$(cat "$1" 2>/dev/null)" = "$(printf "$2")" ] || { echo "FAIL $size $name: $1 is '$(cat "$1" 2>/dev/null | tr '\n' '|')'"; fail=1; }
}
# the stand-in server's requests since the last check, exactly these (one a line, "|" after each)
served() {
    got=$(tr '\n' '|' < "$SLOG"); : > "$SLOG"
    [ "$got" = "$1" ] || { echo "FAIL $size $name: the server had '$got', not '$1'"; fail=1; }
}
# upstream's keyboard by touch (ui/tests/run.sh's): row,column of each key
key() { for rc in "$@"; do awk -v rc=$rc 'BEGIN { split(rc, a, ","); printf "touch 1 %.0f %.0f; ", 18 + a[2] * 60.4 + 27, 80 + a[1] * 66 + 29 }'; done; }
ALICE=$(key 2,0 2,8 1,7 3,2 1,2) BOB=$(key 3,4 1,8 3,4) PW1=$(key 1,9 1,1 0,0) PW2=$(key 1,9 1,1 0,1) PW3=$(key 1,9 1,1 0,2)

for size in 640x480 1024x768; do
    rm -rf "$D/accounts.conf" "$D/ra-accounts" "$D/cache/ra-avatars" "$D/cache/ra-stats" "$D/cache/ra-hashes" "$D/cache/ra-badges" \
        "$FX/retroarch/retroarch.cfg" "$OUT/scrubbed"
    : > "$SLOG"
    # the group's rows, after System; the first two rows of the grid as with six groups, pixel for pixel
    run $size groups "$SET; shot g0"
    logged "banana-ui: kiwami: rows in RetroAchievements: kiwami.ra.account kiwami.ra.stats kiwami.ra.games cheevos_hardcore_mode_enable"
    unlogged "groups from row"
    mv "$OUT/$size-g0-top.png" "$OUT/$size-g0-ra-top.png"; mv "$OUT/$size-g0-bot.png" "$OUT/$size-g0-ra-bot.png"
    KIWAMI_RA_GROUP=0 run $size six "$SET; shot g0"
    logged "banana-ui: kiwami: rows in RetroAchievements: none"
    shown g0
    same g0 g0-ra top; same g0 g0-ra bot

    # RIGHT on System: rows 2 and 3, the trophy's tile selected; UP keeps them, UP again shows rows 1 and 2
    run $size scroll "$SET; press r; press r; press r; press r; press r; press right; shot g7; press up; shot g4; press up; shot g1"
    lines "groups from row" "banana-ui: kiwami: groups from row 1|banana-ui: kiwami: groups from row 0|"
    shown g7
    differ g0 g7 bot; differ g0 g7 top; differ g7 g4 bot
    # with six groups, upstream's grid: RIGHT on System is Display & sound, the rows never move
    KIWAMI_RA_GROUP=0 run $size scroll-six "$SET; press r; press r; press r; press r; press r; press right; shot six"
    unlogged "groups from row"
    same g0 six bot

    # DOWN on Network reaches RetroAchievements; DOWN there wraps to Display & sound, showing rows 1 and 2; A opens the
    # group's list, Account first
    run $size down "$SET; $R4; press down; shot g7d; press down; shot wrap"
    lines "groups from row" "banana-ui: kiwami: groups from row 1|banana-ui: kiwami: groups from row 0|"
    same g7 g7d bot
    run $size list "$SET; $R4; press down; press a; shot ra-list; press a"
    logged "banana-ui: ra: account page: not signed in, 0 remembered"
    shown ra-list

    # UP on Sleep & power: the third row's last tile (its column is empty); then a tap on the empty slot beside it does
    # nothing, a tap on Other systems (shown where row 1 was) selects it, a second opens it
    run $size taps "$SET; press r; press up; shot up; touch 1 320 327; touch 1 116 165; touch 1 116 165; shot opened"
    lines "groups from row" "banana-ui: kiwami: groups from row 1|"
    same g7 up bot
    lines "touch " "banana-ui: touch 320 327: settings result 0|banana-ui: touch 116 165: settings result 1|banana-ui: touch 116 165: settings result 1|"
    # (Other systems' list: its first row, Screen, on the top panel)
    run $size other "$SET; press r; press r; press r; press a; shot other"
    same opened other top
    # a tap on the trophy, where it shows, selects it (UP from it to Other systems keeps rows 2 and 3)
    run $size tap-ra "$SET; press r; press r; press r; press r; press r; press right; shot r7; press up; touch 1 116 327; shot t7"
    lines "touch " "banana-ui: touch 116 327: settings result 1|"
    same g7 t7 bot

    # Mode (the group's fourth row): RIGHT on it is Hardcore, and Other systems > RetroAchievements (L three times, its third row) says the
    # same; the other way round too. (Upstream's rows write /data/retroarch/retroarch.cfg, which the test machine
    # hasn't got: their values are checked on screen, the file on the device, M1.)
    OTHER_RA="press l; press l; press l; press down; press down; wait 1"
    run $size mode-before "$SET; $R4; press down; press a; press down; press down; press down; shot mode0; $OTHER_RA; shot ora0"
    unlogged "in both rows"
    run $size mode "$SET; $R4; press down; press a; press down; press down; press down; press right; wait 1; shot mode1; $OTHER_RA; shot ora1"
    logged "banana-ui: kiwami: Mode Hardcore in both rows"
    differ mode0 mode1 bot; differ ora0 ora1 bot
    run $size mode-back "$SET; press r; press r; press r; press a; press down; press down; press right; wait 1; press left; wait 1; shot ora2; press r; press r; press r; press down; press down; press down; wait 1; shot mode2"
    lines "in both rows" "banana-ui: kiwami: Mode Hardcore in both rows|banana-ui: kiwami: Mode Softcore in both rows|"
    same ora0 ora2 bot; same mode0 mode2 bot

    # ---- the Account page (Settings, R four times, DOWN: RetroAchievements; A: its list; A: Account) ----
    ACCT="$SET; $R4; press down; press a; press a"
    # a fresh card: not signed in, nothing remembered, nothing written and nothing asked by looking
    run $size fresh "$SET; $R4; press down; press a; shot fresh-list; press a; shot fresh"
    logged "banana-ui: ra: account page: not signed in, 0 remembered"
    holds "$D/accounts.conf" -; holds "$D/ra-accounts" -
    served ""
    shown fresh-list fresh
    printf 'smb_user=me\n' > "$D/accounts.conf"

    # New account: alice, pw1, typed by touch; the server's yes keeps the three keys, the token and the avatar
    run $size signin "$ACCT; press a; shot kb-name; $ALICE press start; $PW1 shot typed; press start; shot acct-in; press b; shot in-list"
    logged "banana-ui: ra: signed in as alice (token kept)"
    holds "$D/accounts.conf" 'smb_user=me\nra_user=alice\nra_password=pw1\nra_retroarch_token=tok1'
    holds "$D/ra-accounts" 'current\talice'
    served "r=login2 u=alice p=1 t=0|GET /UserPic/alice.png|"
    [ -s "$D/cache/ra-avatars/alice.png" ] || { echo "FAIL $size $name: no avatar"; fail=1; }
    shown acct-in in-list
    differ fresh-list in-list bot

    # bob with a wrong password: refused, nothing changed
    cp "$D/accounts.conf" "$OUT/accounts.before"
    run $size wrong "$ACCT; press down; press down; press a; $BOB press start; $PW3 press start; shot wrong"
    logged "banana-ui: ra: sign in refused for bob"
    holds "$D/accounts.conf" "$(cat "$OUT/accounts.before")"
    holds "$D/ra-accounts" 'current\talice'
    served "r=login2 u=bob p=1 t=0|"
    # bob with his own: alice remembered (signed out, her password gone)
    run $size bob "$ACCT; press down; press down; press a; $BOB press start; $PW2 press start; shot bob"
    logged "banana-ui: ra: signed in as bob (token kept)"
    holds "$D/accounts.conf" 'smb_user=me\nra_user=bob\nra_password=pw2\nra_retroarch_token=tok2'
    holds "$D/ra-accounts" 'current\tbob\nremembered\talice'
    served "r=login2 u=bob p=1 t=0|GET /UserPic/bob.png|"

    # A on alice, remembered: her password asked; signed in, she leaves the list and bob joins it
    run $size again "$ACCT; press down; press down; shot alice-row; press a; shot pw-alice; $PW1 press start; shot again"
    logged "banana-ui: ra: password asked for alice"
    logged "banana-ui: ra: signed in as alice (token kept)"
    holds "$D/ra-accounts" 'current\talice\nremembered\tbob'
    served "r=login2 u=alice p=1 t=0|GET /UserPic/alice.png|"
    shown alice-row pw-alice

    # X on bob, then DOWN: still there; X, X: gone, with his avatar
    run $size keep "$ACCT; press down; press down; press x; shot x-armed; press down"
    unlogged "removed"
    holds "$D/ra-accounts" 'current\talice\nremembered\tbob'
    [ -s "$D/cache/ra-avatars/bob.png" ] || { echo "FAIL $size $name: bob's avatar gone"; fail=1; }
    run $size remove "$ACCT; press down; press down; press x; press x; shot removed"
    logged "banana-ui: ra: removed bob"
    holds "$D/ra-accounts" 'current\talice'
    holds "$D/cache/ra-avatars/bob.png" -

    # Sign out: A, then B, changes nothing; A, A runs the scrub (the account still there), then the keys,
    # retroarch.cfg's two lines and both tokens go, and alice is remembered
    printf 'video_driver = "kms"\ncheevos_enable = "true"\ncheevos_username = "alice"\n' > "$FX/retroarch/retroarch.cfg"
    printf 'alice\ntok1\n' > "$FX/superdrastic/ra.token"; printf 'alice\ntok1\n' > "$FX/bananads/ra.token"
    cp "$D/accounts.conf" "$OUT/accounts.before"
    run $size out-b "$ACCT; press down; press a; shot out-armed; press b"
    unlogged "signed out"
    holds "$D/accounts.conf" "$(cat "$OUT/accounts.before")"
    holds "$OUT/scrubbed" -
    [ -e "$FX/superdrastic/ra.token" ] && [ -e "$FX/bananads/ra.token" ] || { echo "FAIL $size $name: a token gone"; fail=1; }
    run $size out "$ACCT; press down; press a; press a; shot out"
    logged "banana-ui: ra: signed out alice"
    holds "$OUT/scrubbed" keys
    holds "$D/accounts.conf" 'smb_user=me'
    holds "$FX/retroarch/retroarch.cfg" 'video_driver = "kms"\ncheevos_enable = "false"\ncheevos_username = ""'
    holds "$FX/superdrastic/ra.token" -; holds "$FX/bananads/ra.token" -
    holds "$D/ra-accounts" 'remembered\talice'
    differ out-armed out bot
    served ""

    # signing in with no connection: kept unchecked, no token; the next opening with the server there gets it, once
    KIWAMI_RA_URL=$URL_DOWN run $size offline "$ACCT; press a; $PW1 press start; shot offline"
    logged "banana-ui: ra: signed in as alice, not checked: no connection"
    holds "$D/accounts.conf" 'smb_user=me\nra_user=alice\nra_password=pw1'
    holds "$D/ra-accounts" 'current\talice'
    run $size later "$ACCT; shot later"
    logged "banana-ui: ra: no token for alice yet: asking for it"
    logged "banana-ui: ra: token kept for alice"
    holds "$D/accounts.conf" 'smb_user=me\nra_user=alice\nra_password=pw1\nra_retroarch_token=tok1'
    served "r=login2 u=alice p=1 t=0|GET /UserPic/alice.png|"
    # (opened again: nothing asked) a tap on New account selects it, a second opens the keyboard for its name
    run $size kept "$ACCT; touch 1 320 238; touch 1 320 238; shot kept-kb"
    served ""
    lines "touch " "banana-ui: touch 320 238: settings result 1|banana-ui: touch 320 238: settings result 1|"
    same kb-name kept-kb bot

    # the account in accounts.conf changed behind the page's back (the setup's step): adopted when Settings opens
    printf 'ra_user=bob\nra_password=pw2\n' > "$D/accounts.conf"
    run $size adopt "$SET; shot adopt"
    logged "banana-ui: ra: adopted bob, alice remembered"
    holds "$D/ra-accounts" 'current\tbob\nremembered\talice'
    served ""

    # ---- the Stats page (the group's second row), alice signed in with her token ----
    STATS="$SET; $R4; press down; press a; press down; press a"
    ROUND="r=login2 u=alice p=0 t=1|GET /UserPic/alice.png|"
    for c in 18 5 6 4 7 3 1 12 2; do ROUND="${ROUND}r=allprogress u=alice p=0 t=1 c=$c|"; done
    printf 'ra_user=alice\nra_password=pw1\nra_retroarch_token=tok1\n' > "$D/accounts.conf"
    # opening it asks once: login2 with the token, the avatar, the nine systems in the page's order; kept on the card
    run $size stats "$STATS; shot stats"
    logged "banana-ui: ra: stats for alice: 9 systems"
    served "$ROUND"
    shown stats
    grep -qx 'nds 2 0 1 52' "$D/cache/ra-stats/alice" && grep -qx 'gba 3 1 1 18' "$D/cache/ra-stats/alice" \
        && grep -qx 'n64 0 0 0 0' "$D/cache/ra-stats/alice" && grep -qx 'score 1234' "$D/cache/ra-stats/alice" \
        || { echo "FAIL $size $name: the stats kept: $(tr '\n' '|' < "$D/cache/ra-stats/alice")"; fail=1; }
    # Y asks once more; scrolling to the last system asks nothing; nothing after the page closes (5 s)
    run $size stats-y "$STATS; press y; press up; shot stats-n64; press b; wait 5000"
    served "$ROUND$ROUND"
    differ stats stats-n64 bot
    # no connection: the card's copy, with its date
    KIWAMI_RA_URL=$URL_DOWN run $size stats-cached "$STATS; shot stats-cached"
    grep -q "^banana-ui: ra: stats for alice from the cache (as of [0-9]*)\$" "$log" || { echo "FAIL $size $name: the cache not shown"; fail=1; }
    differ stats stats-cached top
    # a password with no token yet: login2 with it, and the token it gives kept
    printf 'ra_user=alice\nra_password=pw1\n' > "$D/accounts.conf"
    run $size stats-token "$STATS"
    logged "banana-ui: ra: token kept for alice"
    holds "$D/accounts.conf" 'ra_user=alice\nra_password=pw1\nra_retroarch_token=tok1'
    served "r=login2 u=alice p=1 t=0|GET /UserPic/alice.png|$(echo "$ROUND" | cut -d'|' -f3-)"
    # signed out: the page says so and asks nothing
    printf 'smb_user=me\n' > "$D/accounts.conf"
    run $size stats-none "$STATS; shot stats-none"
    logged "banana-ui: ra: stats page: not signed in"
    served ""
    shown stats-none

    # ---- the games on this card (the group's third row): the library's games with achievements recorded, the last
    # played first, under the menu's titles (Orchard Tactics renamed on its game page); Pocket Peel has none recorded
    GAMES="$SET; $R4; press down; press a; press down; press down; press a"
    cp "$D/playstats.tsv" "$OUT/playstats.before"
    now=$(date +%s)
    printf 'gba/Orchard Tactics.gba\t2\t900\t%s\t3\t10\nsnes/Super Smoothie World.sfc\t1\t600\t%s\t0\t5\ngb/Pocket Peel.gb\t4\t300\t%s\n' \
        $((now - 100)) $((now - 50)) $((now - 10)) > "$D/playstats.tsv"
    printf 'gba/Orchard Tactics.gba\tOrchard Tactics DX\t\n' > "$D/game-titles.tsv"
    run $size games "$GAMES; shot games; press down; shot games-2"
    lines "banana-ui: ra: game" 'banana-ui: ra: games on this card: 2|banana-ui: ra: game snes/Super Smoothie World.sfc "Super Smoothie World" 0 / 5|banana-ui: ra: game gba/Orchard Tactics.gba "Orchard Tactics DX" 3 / 10|'
    shown games games-2
    differ games games-2 top
    : > "$D/playstats.tsv"
    run $size games-none "$GAMES; shot games-none"
    logged "banana-ui: ra: games on this card: 0"
    shown games-none
    rm -f "$D/game-titles.tsv"

    # ---- a game's achievements (A on one of the games on this card), alice signed in with her token ----
    # Orchard Tactics (1024 zero bytes: the stand-in knows its hash as game 1446), a game the stand-in doesn't know, a
    # PlayStation .pbp (which can't be identified)
    mkdir -p "$FX/roms/psx"
    head -c 2048 /dev/zero | tr '\0' x > "$FX/roms/gba/Unknown Orchard.gba"; printf 'PBP' > "$FX/roms/psx/Peel Racer.pbp"
    printf 'gba/Orchard Tactics.gba\t2\t900\t%s\t3\t10\ngba/Unknown Orchard.gba\t1\t60\t%s\t0\t4\npsx/Peel Racer.pbp\t1\t60\t%s\t0\t8\n' \
        $((now - 100)) $((now - 200)) $((now - 300)) > "$D/playstats.tsv"
    printf 'ra_user=alice\nra_password=pw1\nra_retroarch_token=tok1\n' > "$D/accounts.conf"
    ZERO=0f343b0931126a20f133d67c2b018a3b
    LIST="r=patch u=alice p=0 t=1 g=1446|r=unlocks u=alice p=0 t=1 g=1446 h=0|r=unlocks u=alice p=0 t=1 g=1446 h=1|"
    # the first time: hashed (and kept with its ID), the list, the unlocks in either mode, the three badges
    run $size game "$GAMES; shot game-list; press a; shot game; press down; press down; shot game-3; press b; shot game-back"
    grep -q "^banana-ui: ra: hashed Orchard Tactics.gba in [0-9]* ms: $ZERO\$" "$log" || { echo "FAIL $size $name: no hash line"; fail=1; }
    logged 'banana-ui: ra: game 1446 "Orchard Tactics": 3 achievements, 2 unlocked (1 hardcore)'
    served "r=gameid u= p=0 t=0 m=$ZERO|${LIST}GET /Badge/101234.png|GET /Badge/101235.png|GET /Badge/101236.png|"
    grep -q "^$FX/roms/gba/Orchard Tactics.gba	1024	[0-9]*	$ZERO	1446\$" "$D/cache/ra-hashes" \
        || { echo "FAIL $size $name: the hash kept: $(tr '\n' '|' < "$D/cache/ra-hashes")"; fail=1; }
    for b in 101234 101235 101236; do [ -s "$D/cache/ra-badges/$b.png" ] || { echo "FAIL $size $name: no badge $b"; fail=1; }; done
    shown game game-3
    differ game game-3 top; differ game game-3 bot
    same game-list game-back bot
    # again: the hash and the ID from the card, the badges too: the list and the unlocks only
    run $size game-again "$GAMES; press a; shot game-again"
    unlogged "hashed"
    served "$LIST"
    same game game-again bot
    # a game the server doesn't know: hashed, gameid answers 0, the page says so (with the hash)
    UNKNOWN=$(head -c 2048 /dev/zero | tr '\0' x | md5sum | cut -c1-32)
    run $size game-unknown "$GAMES; press down; press a; shot game-unknown"
    logged "banana-ui: ra: Unknown Orchard.gba: hash $UNKNOWN unknown to RetroAchievements"
    served "r=gameid u= p=0 t=0 m=$UNKNOWN|"
    shown game-unknown
    # a .pbp: refused, nothing asked
    run $size game-pbp "$GAMES; press down; press down; press a; shot game-pbp"
    logged "banana-ui: ra: can't identify Peel Racer.pbp: a PlayStation .pbp can't be identified, by RetroArch either"
    served ""
    shown game-pbp
    # no connection, the game identified before: no list
    KIWAMI_RA_URL=$URL_DOWN run $size game-down "$GAMES; press a; shot game-down"
    logged "banana-ui: ra: no achievements for Orchard Tactics.gba: no connection"
    differ game game-down bot
    # a password with no token yet: login2 with it first, and the token it gives kept
    printf 'ra_user=alice\nra_password=pw1\n' > "$D/accounts.conf"
    run $size game-token "$GAMES; press a"
    logged "banana-ui: ra: token kept for alice"
    holds "$D/accounts.conf" 'ra_user=alice\nra_password=pw1\nra_retroarch_token=tok1'
    served "r=login2 u=alice p=1 t=0|$LIST"
    # signed out: identified from the card, nothing asked, the page asks to sign in
    printf 'smb_user=me\n' > "$D/accounts.conf"
    run $size game-out "$GAMES; press a; shot game-out"
    logged "banana-ui: ra: game 1446 identified, not signed in"
    served ""
    shown game-out
    rm -f "$FX/roms/gba/Unknown Orchard.gba" "$FX/roms/psx/Peel Racer.pbp"
    cp "$OUT/playstats.before" "$D/playstats.tsv"

    # the groups' frame time with the fork's group as without it (bench's average, each under a millisecond, the
    # group's within twice the other's and 0.05 ms), after a run that rescans the card the games above changed
    run $size bench-warm "home 0"
    run $size bench-ra "$SET; bench 120"; ra_ms=$(sed -n 's/^banana-ui: bench: [0-9]* frames, \([0-9.]*\) ms average.*/\1/p' "$log")
    KIWAMI_RA_GROUP=0 run $size bench-six "$SET; bench 120"; six_ms=$(sed -n 's/^banana-ui: bench: [0-9]* frames, \([0-9.]*\) ms average.*/\1/p' "$log")
    awk -v a="$ra_ms" -v b="$six_ms" 'BEGIN { exit !(a != "" && b != "" && a < 1 && b < 1 && a <= 2 * b + 0.05) }' \
        || { echo "FAIL $size bench: the groups take $ra_ms ms with the group, $six_ms ms without"; fail=1; }

    # no password or token in any of this size's logs
    ! grep -lE 'pw[123]|tok[12]' "$OUT/$size"-*.log || { echo "FAIL $size: a password or a token in the log"; fail=1; }
done
[ $fail = 0 ] && echo "ra-ui: all checks passed ($OUT)"
exit $fail
