#!/bin/sh
# The Game Boy Pokémon companion in banana-ui, headless at both panel sizes (ui/src/kiwami/gbpk/game.cpp, panel.cpp,
# host.cpp; kiwami/GB-POKEMON.md):
# - its two rows in Settings > Other systems > Second screen, right after upstream's Pokémon companion, Off on a fresh
#   card, A on the first writing kiwami_pokemon_companion_gb = on;
# - the game panel with the companion for Crystal (gbpanel: a stand-in for the ROM's tables, as CI has no ROM), fed
#   PokeDaisy's captures (gbfeed): the pages ready, the party read, a battle showing the Battle page and its end the
#   Party page, the corner and the Back button switching to the menu and back (F to the menu), the rules on and put
#   back when the host goes, Crystal's guide file loaded (its 22 milestones; no hidden items, which come from the ROM);
# - a made-up ROM with Crystal's title and another SHA-1: "Not supported", and nothing asked of the game; Crystal,
#   Crystal Legacy and Crystal Clear, which keep Crystal's title, all named till the SHA-1 picks one, and "Not
#   supported" for all;
# - Crystal Legacy's pages on its captures (its own addresses): the party read, a wild battle on the Battle page, its
#   guide file loaded (22 milestones);
# - upstream's gamepanel after it: no companion, and not the Game Boy one again;
# - the pages' blocks for Gen II (KIWAMI_GBPK_TRACE=1 names each drawn): DVs and stat experience, Hidden Power, the
#   Item row alone, no abilities; the Party page as upstream draws it with them off (KIWAMI_GBPK_PAGES=0);
# - Red's pages on its captures (the stand-in's species by their internal index): Gen I's rules on, the party and a
#   wild battle, Gen I's blocks (one Special row, no Item or Friendship row, no Hidden Power, base stats with
#   Special), its guide file loaded (13 milestones), and its rules off before Crystal's companion opens after it;
# - the fork's own types (PureRGB's) named and coloured: a stand-in species made Crystal and Floating (gbtypes), its
#   two chips on the Party page in their colours;
# - Yellow Legacy's pages on its captures (Yellow's title, its own addresses): Gen I's rules, the party of six and a
#   wild battle, its guide file loaded (13 milestones);
# - PureRed's pages on its captures (Red's title, v2.7.6's addresses, the release its README names): Gen I's rules, the
#   party of six and a wild battle, its guide file loaded (13 milestones: Red's, by PureRGB's trainer ids);
# - Crystal Clear's pages on its captures (Crystal's title, its own map ids, the release its README names: v2.6.0's
#   and v2.5.11's): Gen II's rules, the party of two and a wild battle, a trainer's battle (Mom's six); no guide yet.
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
gtouch 600 28; gshot menu; gtouch 600 28; gbutton back; gbutton back; gbutton f; gbutton f; gbutton back; \
gbpanel 1 $CAP/crystal $MADE_UP; gbhost; gbfeed $CAP/crystal; gshot unsupported; \
gamepanel 1 /nonexistent.gba; gtouch 300 300; gbutton back"
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
    # Back (the button beside F) switches as the corner does; F shows the menu, and on the menu does nothing; Back on
    # upstream's panel after it does nothing
    b=$(grep -E 'gbutton ' "$log" | sed 's/.*gbutton //; s/view pages:.*/view pages/' | tr '\n' ';')
    [ "$b" = "back: switched view game;back: switched view pages;f: switched view game;f: nothing view game;back: switched view pages;back: nothing view ;" ] ||
        { echo "FAIL $size crystal: the buttons: $b"; fail=1; }
    shown wait; shown party; shown battle; shown menu
    differ wait party; differ party battle
    # the host goes with the next game: the rules back to upstream's before the made-up ROM's companion opens
    before "gb companion: rules on" "gb companion: rules off"
    before "gb companion: rules off" "gbpanel .*/crystal .*rom.gbc: companion Crystal"
    # a ROM with Crystal's title and another SHA-1: Not supported, nothing asked, no rules
    logged "gb companion: Crystal or Crystal Legacy or Crystal Clear: not a release the companion knows \(sha1 [0-9a-f]{40}\)"
    logged "gbhost: unsupported Crystal view pages:Party$"
    [ "$(grep -cE 'gb companion: rules on' "$log")" = 1 ] || { echo "FAIL $size crystal: the rules on for a ROM not supported"; fail=1; }
    [ "$(grep -E 'gbfeed ' "$log" | tail -n 1 | sed 's/.*asked \([0-9]*\).*/\1/')" = 0 ] || { echo "FAIL $size crystal: a ROM not supported was asked: $(grep gbfeed "$log" | tail -n 1)"; fail=1; }
    shown unsupported
    # upstream's gamepanel after it: no companion, the Game Boy one not opened again
    logged "gamepanel /nonexistent.gba: companion none"
    logged "gtouch 300 300: action [0-9]+ keys 0 view $"
    grep -E 'gb(host|feed|panel)|gtouch|gamepanel' "$log" | sed "s/^banana-ui: /gbpk-ui: $size: /; s|$PWD/||g"

    # three games of Crystal's title (Crystal Legacy and Crystal Clear keep it): all named, the retail game first, and
    # the made-up ROM's SHA-1 none's
    run $size shared "home 0; gbpanel 1 $CAP/crystal $MADE_UP; gbhost; gbfeed $CAP/crystal; gshot unsupported"
    logged "gb companion: Crystal \(or Crystal Legacy, Crystal Clear\)$"
    logged "gbpanel .*/crystal .*rom.gbc: companion Crystal$"
    logged "gb companion: Crystal or Crystal Legacy or Crystal Clear: not a release the companion knows \(sha1 [0-9a-f]{40}\)"
    logged "gbhost: unsupported Crystal view pages:Party$"
    ! grep -q 'gb companion: rules on' "$log" || { echo "FAIL $size shared: the rules on for a ROM not supported"; fail=1; }
    [ "$(grep -E 'gbfeed ' "$log" | tail -n 1 | sed 's/.*asked \([0-9]*\).*/\1/')" = 0 ] || { echo "FAIL $size shared: a ROM not supported was asked: $(grep gbfeed "$log" | tail -n 1)"; fail=1; }
    shown unsupported

    # Crystal Legacy's pages: its capture read at its own addresses (a party of two), a wild battle on the Battle page,
    # its end the Party page; its guide file loaded
    run $size legacy "home 0; gbpanel 1 $CAP/crystal_legacy; gbhost; gbfeed $CAP/crystal_legacy; sleep 300; gshot party; \
wait 300; gbfeed $CAP/crystal_legacy_battle; sleep 300; gshot battle; wait 300; gbfeed $CAP/crystal_legacy; sleep 100"
    logged "gbpanel .*/crystal_legacy: companion Crystal Legacy$"
    logged "gb companion: rules on \(Generation II\)"
    logged "gb companion: guide: 22 milestones, 0 hidden items$"
    ! grep -q 'gb companion: no guide' "$log" || { echo "FAIL $size legacy: $(grep 'no guide' "$log")"; fail=1; }
    logged "gbhost: ready Crystal Legacy view pages:Party$"
    logged "gbfeed .*/crystal_legacy: asked [1-9][0-9]*, party 2, battle 0, view pages:Party$"
    logged "gbfeed .*/crystal_legacy_battle: asked [1-9][0-9]*, party 2, battle 1, view pages:Battle$"
    [ "$(grep -cE 'gbfeed .*/crystal_legacy: asked [1-9][0-9]*, party 2, battle 0, view pages:Party$' "$log")" = 2 ] ||
        { echo "FAIL $size legacy: the battle's end didn't show the Party page"; fail=1; }
    shown party; shown battle
    differ party battle
    grep -E 'gb(host|feed|panel)' "$log" | sed "s/^banana-ui: /gbpk-ui: $size: legacy: /; s|$PWD/||g"

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

    # Red: Gen I's rules, its guide file, its party of six and a wild battle, Gen I's blocks on the Party, Battle and Dex
    # pages (a species picked off the list), none of Gen II's; then Crystal's companion, with Gen I's rules off before it
    export KIWAMI_GBPK_TRACE=1
    run $size red "home 0; gbpanel 1 $CAP/red; gbhost; gbfeed $CAP/red; sleep 300; gshot party; wait 300; \
gbfeed $CAP/red_battle; sleep 300; gshot battle; wait 300; gbfeed $CAP/red; sleep 100; gtouch 509 448; gshot dex; \
gtouch 100 298; gshot species; gbpanel 1 $CAP/crystal; gbhost"
    unset KIWAMI_GBPK_TRACE
    logged "gbpanel .*/red: companion Red$"
    logged "gb companion: rules on \(Generation I\)$"
    logged "gb companion: guide: 13 milestones, 0 hidden items$"
    ! grep -q 'gb companion: no guide' "$log" || { echo "FAIL $size red: $(grep 'no guide' "$log")"; fail=1; }
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

    # the fork's own types (PureRGB's, ui/src/kiwami/gbpk/gbpk.h) on the Party page: the stand-in's Pikachu (84, Red's
    # internal index) made Crystal and Floating, their chips in their colours (0x88d0e0, 0x98a8f0: a chip's worth of
    # each, counted, wherever the page puts them), and neither colour in Red's own shot above (Pikachu Normal)
    run $size types "home 0; gbtypes 84 crystal floating; gbpanel 1 $CAP/red; gbhost; gbfeed $CAP/red; sleep 300; gshot party"
    logged "gbtypes 84: crystal floating$"
    logged "gbfeed .*/red: asked [1-9][0-9]*, party 6, battle 0, view pages:Party$"
    shown party
    pixels() { convert "$1" -fill black +opaque "#$2" -fill white -opaque "#$2" -format '%[fx:round(mean*w*h)]' info: 2>/dev/null; }
    for c in 88d0e0 98a8f0; do
        n=$(pixels "$OUT/$size-types-party-bot.png" $c) r=$(pixels "$OUT/$size-red-party-bot.png" $c)
        [ "${n:-0}" -gt 200 ] && [ "${r:-1}" = 0 ] || { echo "FAIL $size types: #$c: ${n:-none} pixels with the fork's types, ${r:-none} without"; fail=1; }
        echo "gbpk-ui: $size: types: #$c: $n pixels on the Party page, $r without the fork's types"
    done

    # Yellow Legacy's pages: Yellow's title, its capture read at its own addresses (the party of six), a wild battle on
    # the Battle page, its end the Party page; Gen I's rules; its guide file loaded
    run $size ylegacy "home 0; gbpanel 1 $CAP/yellow_legacy; gbhost; gbfeed $CAP/yellow_legacy; sleep 300; gshot party; \
wait 300; gbfeed $CAP/yellow_legacy_battle; sleep 300; gshot battle; wait 300; gbfeed $CAP/yellow_legacy; sleep 100"
    logged "gbpanel .*/yellow_legacy: companion Yellow Legacy$"
    logged "gb companion: rules on \(Generation I\)$"
    logged "gb companion: guide: 13 milestones, 0 hidden items$"
    ! grep -q 'gb companion: no guide' "$log" || { echo "FAIL $size ylegacy: $(grep 'no guide' "$log")"; fail=1; }
    logged "gbhost: ready Yellow Legacy view pages:Party$"
    logged "gbfeed .*/yellow_legacy: asked [1-9][0-9]*, party 6, battle 0, view pages:Party$"
    logged "gbfeed .*/yellow_legacy_battle: asked [1-9][0-9]*, party 6, battle 1, view pages:Battle$"
    [ "$(grep -cE 'gbfeed .*/yellow_legacy: asked [1-9][0-9]*, party 6, battle 0, view pages:Party$' "$log")" = 2 ] ||
        { echo "FAIL $size ylegacy: the battle's end didn't show the Party page"; fail=1; }
    shown party; shown battle
    differ party battle
    grep -E 'gb(host|feed|panel)|gb companion: (rules|guide)' "$log" | sed "s/^banana-ui: /gbpk-ui: $size: ylegacy: /; s|$PWD/||g"

    # PureRed's pages: Red's title, its capture read at PureRGB's addresses in WRAM bank 1 (the party of six), a wild
    # battle on the Battle page, its end the Party page; Gen I's rules; its guide file loaded (its levels and hidden items
    # come from the ROM, which the stand-in hasn't)
    run $size purered "home 0; gbpanel 1 $CAP/purered; gbhost; gbfeed $CAP/purered; sleep 300; gshot party; \
wait 300; gbfeed $CAP/purered_battle; sleep 300; gshot battle; wait 300; gbfeed $CAP/purered; sleep 100"
    logged "gbpanel .*/purered: companion PureRed$"
    logged "gb companion: rules on \(Generation I\)$"
    logged "gb companion: guide: 13 milestones, 0 hidden items$"
    ! grep -q 'gb companion: no guide' "$log" || { echo "FAIL $size purered: $(grep 'no guide' "$log")"; fail=1; }
    logged "gbhost: ready PureRed view pages:Party$"
    logged "gbfeed .*/purered: asked [1-9][0-9]*, party 6, battle 0, view pages:Party$"
    logged "gbfeed .*/purered_battle: asked [1-9][0-9]*, party 6, battle 1, view pages:Battle$"
    [ "$(grep -cE 'gbfeed .*/purered: asked [1-9][0-9]*, party 6, battle 0, view pages:Party$' "$log")" = 2 ] ||
        { echo "FAIL $size purered: the battle's end didn't show the Party page"; fail=1; }
    shown party; shown battle
    differ party battle
    grep -E 'gb(host|feed|panel)|gb companion: (rules|guide|no guide)' "$log" | sed "s/^banana-ui: /gbpk-ui: $size: purered: /; s|$PWD/||g"

    # Crystal Clear's pages: Crystal's title, its captures read at its own map ids (the party of two), a wild battle on
    # the Battle page and its end the Party page, then v2.5.11's capture and its battle, and Mom's battle, her six;
    # Gen II's rules; no guide file (the next step makes its guide from the ROM, which the stand-in hasn't)
    run $size clear "home 0; gbpanel 1 $CAP/crystal_clear; gbhost; gbfeed $CAP/crystal_clear; sleep 300; gshot party; \
wait 300; gbfeed $CAP/crystal_clear_battle; sleep 300; gshot battle; wait 300; gbfeed $CAP/crystal_clear; sleep 100; \
gbpanel 1 $CAP/crystal_clear_v2_5_11; gbhost; gbfeed $CAP/crystal_clear_v2_5_11; sleep 300; \
gbfeed $CAP/crystal_clear_v2_5_11_battle; sleep 300; gbfeed $CAP/crystal_clear_trainer; sleep 300; gshot trainer"
    logged "gbpanel .*/crystal_clear: companion Crystal Clear$"
    logged "gbpanel .*/crystal_clear_v2_5_11: companion Crystal Clear$"
    logged "gb companion: rules on \(Generation II\)"
    logged "gb companion: no guide: "
    logged "gbhost: ready Crystal Clear view pages:Party$"
    logged "gbfeed .*/crystal_clear: asked [1-9][0-9]*, party 2, battle 0, view pages:Party$"
    logged "gbfeed .*/crystal_clear_battle: asked [1-9][0-9]*, party 2, battle 1, view pages:Battle$"
    [ "$(grep -cE 'gbfeed .*/crystal_clear: asked [1-9][0-9]*, party 2, battle 0, view pages:Party$' "$log")" = 2 ] ||
        { echo "FAIL $size clear: the battle's end didn't show the Party page"; fail=1; }
    logged "gbfeed .*/crystal_clear_v2_5_11: asked [1-9][0-9]*, party 2, battle 0, view pages:Party$"
    logged "gbfeed .*/crystal_clear_v2_5_11_battle: asked [1-9][0-9]*, party 2, battle 1, view pages:Battle$"
    logged "gbfeed .*/crystal_clear_trainer: asked [1-9][0-9]*, party 1, battle 1, view pages:Battle$"
    shown party; shown battle; shown trainer
    differ party battle; differ battle trainer
    grep -E 'gb(host|feed|panel)|gb companion: (rules|guide|no guide)' "$log" | sed "s/^banana-ui: /gbpk-ui: $size: clear: /; s|$PWD/||g"
done

[ $fail = 0 ] || exit 1
echo "gbpk-ui: all checks passed ($OUT)"
