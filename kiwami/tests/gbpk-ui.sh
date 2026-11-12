#!/bin/sh
# The Game Boy Pokémon companion in banana-ui, headless at both panel sizes (ui/src/kiwami/gbpk/game.cpp, panel.cpp,
# host.cpp; kiwami/GB-POKEMON.md):
# - its two rows in Settings > Other systems > Second screen, right after upstream's Pokémon companion, Off on a fresh
#   card, A on the first writing kiwami_pokemon_companion_gb = on;
# - the game panel with the companion for Crystal (gbpanel: a stand-in for the ROM's tables, as CI has no ROM), fed
#   PokeDaisy's captures (gbfeed): the pages ready, the party read, a battle showing the Battle page and its end the
#   Party page, the corner switching to the menu and back, the rules on and put back when the host goes, Crystal's
#   guide file loaded (its 22 milestones; no hidden items, which come from the ROM);
# - a made-up ROM with Crystal's title and another SHA-1: "Not supported", and nothing asked of the game;
# - upstream's gamepanel after it: no companion, and not the Game Boy one again;
# - the pages' blocks for Gen II (KIWAMI_GBPK_TRACE=1 names each drawn): DVs and stat experience, Hidden Power, the
#   Item row alone, no abilities; the Party page as upstream draws it with them off (KIWAMI_GBPK_PAGES=0);
# - Red's pages on its captures (the stand-in's species by their internal index): Gen I's rules on, the party and a
#   wild battle, Gen I's blocks (one Special row, no Item or Friendship row, no Hidden Power, base stats with
#   Special), and its rules off before Crystal's companion opens after it.
# Needs ui/banana-ui (make ui) and ImageMagick (the shots not blank). Run by kiwami/tests/ci.sh when banana-ui is
# built, or by hand:
#   kiwami/tests/gbpk-ui.sh       (the shots and logs in $OUT, /tmp/kiwami-gbpk-ui by default)
set -e
cd "$(dirname "$0")/../.."
UI=$PWD/ui
OUT=${OUT:-${TMPDIR:-/tmp}/kiwami-gbpk-ui}
FX=$OUT/fixture
CAP=$PWD/kiwami/tests/gbpk/fixtures
MADE_UP=$PWD/kiwami/tests/gbpk/made-up/rom.gbc
export KIWAMI_GBPK_DIR=$PWD/kiwami/rootfs/usr/share/banana/companions/gb-pokemon
rm -rf "$OUT"; mkdir -p "$OUT"
"$UI/tests/fixture.sh" "$FX" >/dev/null 2>&1
CONF=$FX/data/settings.conf
fail=0

# run <size> <name> <script>: one run of banana-ui
run() {
    size=$1 name=$2 log=$OUT/$1-$2.log
    "$UI/banana-ui" --headless "$OUT/$size-$name-" --size $size \
        --assets "$UI/assets/rnds" --systems "$PWD/system/rootfs/etc/banana/systems.conf" --roms "$FX/roms" --data "$FX/data" \
        --script "$3" 2> "$log" || { echo "FAIL $size $name: banana-ui exited $?"; fail=1; }
}
logged() { grep -qE -- "$1" "$log" || { echo "FAIL $size $name: no '$1' in the log: $(grep -E 'gb|gtouch|gamepanel' "$log" | tail -n 4 | tr '\n' ' ')"; fail=1; }; }
# the first line matching $1 comes before the first matching $2
before() {
    a=$(grep -nE -- "$1" "$log" | head -n 1 | cut -d: -f1) b=$(grep -nE -- "$2" "$log" | head -n 1 | cut -d: -f1)
    [ -n "$a" ] && [ -n "$b" ] && [ "$a" -lt "$b" ] || { echo "FAIL $size $name: '$1' isn't before '$2'"; fail=1; }
}
shown() {
    f=$OUT/$size-$name-$1-bot.png
    [ -s "$f" ] && [ "$(convert "$f" -format '%[fx:standard_deviation>0.01]' info: 2>/dev/null)" = 1 ] || { echo "FAIL $size $name: $f missing or blank"; fail=1; }
}
differ() { cmp -s "$OUT/$size-$name-$1-bot.png" "$OUT/$size-$name-$2-bot.png" && { echo "FAIL $size $name: $1 and $2 are the same"; fail=1; }; true; }

for size in 640x480 1024x768; do
    # the rows: Settings opens on its tiles, R is Other systems, A its list, Down four times Second screen, A its page;
    # Down four more is the first of the two rows (after Pokémon companion), A sets it On
    printf 'wifi = on\n' > "$CONF"
    run $size rows "home 0; wait 2400; press start; press r; press a; press down; press down; press down; press down; press a; \
press down; press down; press down; press down; wait 1; shot off; press a; wait 1; shot on"
    logged "banana-ui: kiwami: rows in Second screen: kiwami_pokemon_companion_gb kiwami_pokemon_companion_gbc$"
    grep -qx 'kiwami_pokemon_companion_gb *= *"*on"*' "$CONF" || { echo "FAIL $size rows: no kiwami_pokemon_companion_gb = on: $(grep kiwami_pokemon "$CONF")"; fail=1; }
    ! grep -q 'kiwami_pokemon_companion_gbc' "$CONF" || { echo "FAIL $size rows: the Game Boy Color row written too"; fail=1; }
    shown off; differ off on

    # Crystal's pages: reading, then ready on the Party page; the capture read (a party of two), shown; a wild battle
    # shows the Battle page, its end the Party page; the corner shows the menu (the tiles) and the pages again
    run $size crystal "home 0; gbpanel 1 $CAP/crystal; gshot wait; gbhost; gbfeed $CAP/crystal; sleep 300; gshot party; \
wait 300; gbfeed $CAP/crystal_battle; sleep 300; gshot battle; wait 300; gbfeed $CAP/crystal; sleep 100; \
gtouch 600 28; gshot menu; gtouch 600 28; gbpanel 1 $CAP/crystal $MADE_UP; gbhost; gbfeed $CAP/crystal; gshot unsupported; \
gamepanel 1 /nonexistent.gba; gtouch 300 300"
    logged "gbpanel .*/crystal: companion Crystal"
    logged "gb companion: rules on \(Generation II\)"
    logged "gb companion: guide: 22 milestones, 0 hidden items$"
    ! grep -q 'gb companion: no guide' "$log" || { echo "FAIL $size crystal: $(grep 'no guide' "$log")"; fail=1; }
    logged "gbhost: ready Crystal view pages:Party$"
    logged "gbfeed .*/crystal: asked [1-9][0-9]*, party 2, battle 0, view pages:Party$"
    logged "gbfeed .*/crystal_battle: asked [1-9][0-9]*, party 2, battle 1, view pages:Battle$"
    [ "$(grep -cE 'gbfeed .*/crystal: asked [1-9][0-9]*, party 2, battle 0, view pages:Party$' "$log")" = 2 ] ||
        { echo "FAIL $size crystal: the battle's end didn't show the Party page"; fail=1; }
    logged "gtouch 600 28: action 0 keys 0 view game$"
    logged "gtouch 600 28: action 0 keys 0 view pages:"
    shown wait; shown party; shown battle; shown menu
    differ wait party; differ party battle
    # the host goes with the next game: the rules back to upstream's before the made-up ROM's companion opens
    before "gb companion: rules on" "gb companion: rules off"
    before "gb companion: rules off" "gbpanel .*/crystal .*rom.gbc: companion Crystal"
    # a ROM with Crystal's title and another SHA-1: Not supported, nothing asked, no rules
    logged "gb companion: Crystal: not a release the companion knows \(sha1 [0-9a-f]{40}\)"
    logged "gbhost: unsupported Crystal view pages:Party$"
    [ "$(grep -cE 'gb companion: rules on' "$log")" = 1 ] || { echo "FAIL $size crystal: the rules on for a ROM not supported"; fail=1; }
    [ "$(grep -E 'gbfeed ' "$log" | tail -n 1 | sed 's/.*asked \([0-9]*\).*/\1/')" = 0 ] || { echo "FAIL $size crystal: a ROM not supported was asked: $(grep gbfeed "$log" | tail -n 1)"; fail=1; }
    shown unsupported
    # upstream's gamepanel after it: no companion, the Game Boy one not opened again
    logged "gamepanel /nonexistent.gba: companion none"
    logged "gtouch 300 300: action [0-9]+ keys 0 view $"
    grep -E 'gb(host|feed|panel)|gtouch|gamepanel' "$log" | sed "s/^banana-ui: /gbpk-ui: $size: /; s|$PWD/||g"

    # Gen II's blocks on the pages (pages-impl.inc, traced): the Party page's Item row alone and its DVs and stat
    # experience, the wild box's DVs and Hidden Power, the Dex page with no abilities (Pidgey, the list dragged to it:
    # Bulbasaur, its first, isn't seen). A tap on a party row and a drag on the summary still move the page. A page's
    # taps are the ones it was last drawn with, so a shot comes before each tap. gbdrag is in the panel's own px.
    k=$(echo $size | awk -Fx '{print $2 / 480}')
    drag() { echo "gbdrag $(echo "$*" | awk -v k=$k '{for (i = 1; i <= NF; i++) printf "%s%d", (i > 1 ? " " : ""), $i * k}')"; }
    export KIWAMI_GBPK_TRACE=1
    run $size pages "home 0; gbpanel 1 $CAP/crystal; gbhost; gbfeed $CAP/crystal; sleep 300; gshot party; \
$(drag 400 380 400 100); gshot scrolled; gtouch 100 150; gshot second; gbfeed $CAP/crystal_battle; sleep 300; gshot battle; \
wait 300; gbfeed $CAP/crystal; sleep 100; gshot back; gtouch 509 448; gshot dex; $(drag 100 400 100 64); gshot list; \
gtouch 100 298; gshot pidgey"
    unset KIWAMI_GBPK_TRACE
    logged "gb companion: pages: summary item$"
    logged "gb companion: pages: stats dv statexp 6 rows$"
    logged "gb companion: pages: wild dvs hidden-power$"
    logged "gb companion: pages: no abilities$"
    logged "gbdrag [0-9 ]+: view pages:Party$"
    logged "gtouch 509 448: action 0 keys 0 view pages:Dex$"
    ! grep -qE "gb companion: pages: (summary none|no friendship|base stats|stats dv statexp 5)" "$log" ||
        { echo "FAIL $size pages: a Gen I block drawn for Crystal"; fail=1; }
    shown party; shown battle; shown pidgey
    differ party scrolled; differ scrolled second; differ list pidgey
    # the same Party page with the fork's blocks off: upstream's, so another picture, and nothing traced
    export KIWAMI_GBPK_PAGES=0 KIWAMI_GBPK_TRACE=1
    run $size upstream "home 0; gbpanel 1 $CAP/crystal; gbhost; gbfeed $CAP/crystal; sleep 300; gshot party"
    unset KIWAMI_GBPK_PAGES KIWAMI_GBPK_TRACE
    logged "gbfeed .*/crystal: asked [1-9][0-9]*, party 2, battle 0, view pages:Party$"
    ! grep -q "gb companion: pages:" "$log" || { echo "FAIL $size upstream: the fork's blocks drawn with KIWAMI_GBPK_PAGES=0"; fail=1; }
    shown party
    cmp -s "$OUT/$size-pages-party-bot.png" "$OUT/$size-upstream-party-bot.png" &&
        { echo "FAIL $size upstream: the Party page is the same with the fork's blocks off"; fail=1; }
    grep -E 'gbdrag|gb companion: pages:' "$OUT/$size-pages.log" | sort | uniq -c | sed "s/^ */gbpk-ui: $size: /"

    # Red: Gen I's rules, its party of six and a wild battle, Gen I's blocks on the Party, Battle and Dex pages (a
    # species picked off the list), none of Gen II's; then Crystal's companion, with Gen I's rules off before it
    export KIWAMI_GBPK_TRACE=1
    run $size red "home 0; gbpanel 1 $CAP/red; gbhost; gbfeed $CAP/red; sleep 300; gshot party; wait 300; \
gbfeed $CAP/red_battle; sleep 300; gshot battle; wait 300; gbfeed $CAP/red; sleep 100; gtouch 509 448; gshot dex; \
gtouch 100 298; gshot species; gbpanel 1 $CAP/crystal; gbhost"
    unset KIWAMI_GBPK_TRACE
    logged "gbpanel .*/red: companion Red$"
    logged "gb companion: rules on \(Generation I\)$"
    logged "gbhost: ready Red view pages:Party$"
    logged "gbfeed .*/red: asked [1-9][0-9]*, party 6, battle 0, view pages:Party$"
    logged "gbfeed .*/red_battle: asked [1-9][0-9]*, party 6, battle 1, view pages:Battle$"
    logged "gb companion: pages: summary none$"
    logged "gb companion: pages: no friendship$"
    logged "gb companion: pages: stats dv statexp 5 rows$"
    logged "gb companion: pages: wild dvs$"
    logged "gb companion: pages: base stats special$"
    logged "gb companion: pages: no abilities$"
    ! grep -qE "gb companion: pages: (summary item|stats dv statexp 6 rows|wild dvs hidden-power)" "$log" ||
        { echo "FAIL $size red: a Gen II block drawn for Red"; fail=1; }
    before "gb companion: rules on \(Generation I\)" "gb companion: rules off"
    before "gb companion: rules off" "gb companion: rules on \(Generation II\)"
    shown party; shown battle; shown species
    differ party battle
    grep -E 'gb(host|feed|panel)|gb companion: (rules|pages:)' "$log" | sort | uniq -c | sed "s/^ */gbpk-ui: $size: red: /; s|$PWD/||g"
done

[ $fail = 0 ] || exit 1
echo "gbpk-ui: all checks passed ($OUT)"
