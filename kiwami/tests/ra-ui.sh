#!/bin/sh
# The RetroAchievements group in banana-ui's Settings, headless at both panel sizes: its rows (Account, Mode) after
# System; the groups' grid showing two rows, the first two exactly as with six groups (KIWAMI_RA_GROUP=0 leaves the
# group out: upstream's grid), scrolling one row to the trophy's and back, wrapping with a short last row; taps on what
# shows; Mode and Other systems > RetroAchievements kept in step. Needs ui/banana-ui (make ui) and ImageMagick (the
# test library's art).
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

for size in 640x480 1024x768; do
    # the group's rows, after System; the first two rows of the grid as with six groups, pixel for pixel
    run $size groups "$SET; shot g0"
    logged "banana-ui: kiwami: rows in RetroAchievements: kiwami.ra.account cheevos_hardcore_mode_enable"
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
    logged "banana-ui: ra: account page (to come)"
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

    # Mode: RIGHT on it is Hardcore, and Other systems > RetroAchievements (L three times, its third row) says the
    # same; the other way round too. (Upstream's rows write /data/retroarch/retroarch.cfg, which the test machine
    # hasn't got: their values are checked on screen, the file on the device, M1.)
    OTHER_RA="press l; press l; press l; press down; press down; wait 1"
    run $size mode-before "$SET; $R4; press down; press a; press down; shot mode0; $OTHER_RA; shot ora0"
    unlogged "in both rows"
    run $size mode "$SET; $R4; press down; press a; press down; press right; wait 1; shot mode1; $OTHER_RA; shot ora1"
    logged "banana-ui: kiwami: Mode Hardcore in both rows"
    differ mode0 mode1 bot; differ ora0 ora1 bot
    run $size mode-back "$SET; press r; press r; press r; press a; press down; press down; press right; wait 1; press left; wait 1; shot ora2; press r; press r; press r; press down; wait 1; shot mode2"
    lines "in both rows" "banana-ui: kiwami: Mode Hardcore in both rows|banana-ui: kiwami: Mode Softcore in both rows|"
    same ora0 ora2 bot; same mode0 mode2 bot
done
[ $fail = 0 ] && echo "ra-ui: all checks passed ($OUT)"
exit $fail
