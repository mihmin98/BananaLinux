#!/bin/sh
# Power scaling's row (ui/src/kiwami/kiwami.cpp), headless at both panel sizes, in Settings > Other systems: added by
# the fork's Second screen hook (its own log line, Second screen's as it was), placed right after upstream's Frontend,
# Off on a fresh card, A on it writing kiwami_power_scaling = on.
# Other systems' list is reached from its end: Up from its first row goes to its last, Reset RetroArch, then
# RetroAchievements, then this row, then Frontend (whatever comes before: Emulators shows only with two cores of a
# system installed). A on Frontend (Auto) writes ra_frontend = bananafront, so four Ups are Frontend and three this row.
# Needs ui/banana-ui (make ui) and ImageMagick. Run by kiwami/tests/ci.sh when banana-ui is built, or by hand:
#   kiwami/tests/power-scaling-ui.sh       (the shots and logs in $OUT, /tmp/kiwami-power-scaling-ui by default)
set -e
cd "$(dirname "$0")/../.."
UI=$PWD/ui
OUT=${OUT:-${TMPDIR:-/tmp}/kiwami-power-scaling-ui}
FX=$OUT/fixture
rm -rf "$OUT"; mkdir -p "$OUT"
"$UI/tests/fixture.sh" "$FX" >/dev/null 2>&1
CONF=$FX/data/settings.conf
fail=0

# run <size> <name> <settings.conf lines> <script>: one run of banana-ui, its log in $OUT/<size>-<name>.log and its
# shots as $OUT/<size>-<name>-<shot>-{top,bot}.png
run() {
    size=$1 name=$2 log=$OUT/$1-$2.log
    printf "$3\n" > "$CONF"
    "$UI/banana-ui" --headless "$OUT/$size-$name-" --size $size \
        --assets "$UI/assets/rnds" --systems "$PWD/system/rootfs/etc/banana/systems.conf" --roms "$FX/roms" --data "$FX/data" \
        --script "$4" 2> "$log" || { echo "FAIL $size $name: banana-ui exited $?"; fail=1; }
}
logged() { grep -qE -- "$1" "$log" || { echo "FAIL $size $name: no '$1' in the log: $(grep 'kiwami' "$log" | tail -n 4 | tr '\n' ' ')"; fail=1; }; }
holds() { grep -qx "$1 *= *\"*$2\"*" "$CONF" || { echo "FAIL $size $name: no $1 = $2 in settings.conf: $(grep -v '^#' "$CONF" | tr '\n' '|')"; fail=1; }; }
# (Settings writes its page's other rows too, at their values: a row left alone is missing or at its default)
left() { ! grep "^ *$1 *=" "$CONF" | grep -qv "= *\"*$2\"*$" || { echo "FAIL $size $name: $1 changed: $(grep "$1" "$CONF")"; fail=1; }; }
differ() { cmp -s "$OUT/$size-$1-$3.png" "$OUT/$size-$2-$3.png" && { echo "FAIL $size: $1 and $2 are the same ($3)"; fail=1; }; true; }

# Settings opens on its tiles: R is Other systems, A its list (on its first row)
OTHER="home 0; wait 2400; press start; press r; press a"
for size in 640x480 1024x768; do
    # the row, three Ups from the first row: Off on a fresh card, so A writes on; the page shot before and after
    run $size row "wifi = on" "$OTHER; press up; press up; press up; wait 1; shot off; press a; wait 1; shot on"
    logged "banana-ui: kiwami: rows in Second screen: kiwami_pokemon_companion_gb kiwami_pokemon_companion_gbc kiwami_panel_status$"
    logged "banana-ui: kiwami: rows in Other systems: kiwami_power_scaling$"
    holds kiwami_power_scaling on
    left ra_frontend auto
    differ row-off row-on bot
    # ... and A again: off
    run $size again "wifi = on\nkiwami_power_scaling = on" "$OTHER; press up; press up; press up; wait 1; press a; wait 1"
    holds kiwami_power_scaling off
    # four Ups is upstream's Frontend (Auto: A gives BananaFront), so the row follows it
    run $size frontend "wifi = on" "$OTHER; press up; press up; press up; press up; wait 1; press a; wait 1"
    holds ra_frontend bananafront
    left kiwami_power_scaling off
done

[ $fail = 0 ] || { echo "power-scaling-ui: FAILED (shots and logs in $OUT)"; exit 1; }
echo "power-scaling-ui: all checks passed ($OUT)"
