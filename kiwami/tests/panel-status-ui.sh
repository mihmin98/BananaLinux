#!/bin/sh
# Status on the game panel (ui/src/kiwami/panelstatus.h), headless at both panel sizes:
# - its row in Settings > Other systems > Second screen, right after upstream's Game panel idle time, Off on a fresh
#   card, A on it writing kiwami_panel_status = on.
# Needs ui/banana-ui (make ui) and ImageMagick. Run by kiwami/tests/ci.sh when banana-ui is built, or by hand:
#   kiwami/tests/panel-status-ui.sh       (the shots and logs in $OUT, /tmp/kiwami-panel-status-ui by default)
set -e
cd "$(dirname "$0")/../.."
UI=$PWD/ui
OUT=${OUT:-${TMPDIR:-/tmp}/kiwami-panel-status-ui}
FX=$OUT/fixture
rm -rf "$OUT"; mkdir -p "$OUT"
"$UI/tests/fixture.sh" "$FX" >/dev/null 2>&1
CONF=$FX/data/settings.conf
fail=0

# run <size> <name> <script>: one run of banana-ui, its log in $OUT/<size>-<name>.log and its shots as
# $OUT/<size>-<name>-<shot>-{top,bot}.png
run() {
    size=$1 name=$2 log=$OUT/$1-$2.log
    "$UI/banana-ui" --headless "$OUT/$size-$name-" --size $size \
        --assets "$UI/assets/rnds" --systems "$PWD/system/rootfs/etc/banana/systems.conf" --roms "$FX/roms" --data "$FX/data" \
        --script "$3" 2> "$log" || { echo "FAIL $size $name: banana-ui exited $?"; fail=1; }
}
logged() { grep -qE -- "$1" "$log" || { echo "FAIL $size $name: no '$1' in the log: $(grep 'kiwami' "$log" | tail -n 4 | tr '\n' ' ')"; fail=1; }; }
differ() { cmp -s "$OUT/$size-$name-$1-bot.png" "$OUT/$size-$name-$2-bot.png" && { echo "FAIL $size $name: $1 and $2 are the same"; fail=1; }; true; }

for size in 640x480 1024x768; do
    # the row: Settings opens on its tiles, R is Other systems, A its list, Down four times Second screen, A its page;
    # Down three more is the row (after Second screen, Game panel when idle and Game panel idle time), A sets it On
    printf 'wifi = on\n' > "$CONF"
    run $size row "home 0; wait 2400; press start; press r; press a; press down; press down; press down; press down; press a; \
press down; press down; press down; wait 1; shot off; press a; wait 1; shot on"
    logged "banana-ui: kiwami: rows in Second screen: kiwami_pokemon_companion_gb kiwami_pokemon_companion_gbc kiwami_panel_status$"
    grep -qx 'kiwami_panel_status *= *"*on"*' "$CONF" || { echo "FAIL $size row: no kiwami_panel_status = on: $(grep kiwami_panel "$CONF")"; fail=1; }
    differ off on
done
[ $fail = 0 ] && echo "panel-status-ui: all checks passed ($OUT)"
exit $fail
