#!/bin/sh
# The fork's cores in banana-ui's Settings, headless at both panel sizes, against stand-in libretro folders
# (KIWAMI_LIBRETRO, empty files named as the cores: upstream's image's cores, with or without the fork's): upstream's
# own core rows in Other systems (a row per system with two cores installed) offer Mesen for the NES, Genesis Plus GX
# for the Mega Drive and SameBoy (kiwami::moreCores, its kiwami: line) for the Game Boy and Game Boy Color; browsing
# writes nothing, Right writes each core, a missing core's setting is kept. Needs ui/banana-ui (make ui) and
# ImageMagick (the test library's art).
# Run by kiwami/tests/ci.sh when banana-ui is built, or by hand:
#   kiwami/tests/cores-ui.sh        (the shots in $OUT, /tmp/kiwami-cores by default)
set -e
cd "$(dirname "$0")/../.."
UI=$PWD/ui
OUT=${OUT:-${TMPDIR:-/tmp}/kiwami-cores}
FX=$OUT/fixture
rm -rf "$OUT"; mkdir -p "$OUT/all" "$OUT/sameboy" "$OUT/upstream"
for d in all sameboy upstream; do
    for c in gpsp mgba gambatte fceumm snes9x picodrive pcsx_rearmed mupen64plus_next; do : > "$OUT/$d/${c}_libretro.so"; done
done
: > "$OUT/all/mesen_libretro.so"; : > "$OUT/all/sameboy_libretro.so"; : > "$OUT/all/genesis_plus_gx_libretro.so"
: > "$OUT/sameboy/sameboy_libretro.so"
CONF=$FX/data/settings.conf
fail=0
# Settings opens on its groups: R three times is Other systems, A its list (on Screen). Its last rows, Up from Screen
# (it goes round): Reset RetroArch, RetroArch hotkeys, then the core rows in upstream's order, the last first (GBA,
# Game Boy Color, Game Boy, NES, Mega Drive; Super Nintendo, PlayStation and N64 have one core here, so no row)
OTHER="home 0; press start; press r; press r; press r; press a"
up() { n=$1; s=""; while [ $n -gt 0 ]; do s="$s; press up"; n=$((n - 1)); done; echo "${s#; }"; }

# a fresh card: the fixture, and the settings.conf banana-ui writes at its first start
card() {
    "$UI/tests/fixture.sh" "$FX" >/dev/null 2>&1
    "$UI/banana-ui" --headless "$OUT/$size-" --size $size --assets "$UI/assets/rnds" \
        --systems "$PWD/system/rootfs/etc/banana/systems.conf" --roms "$FX/roms" --data "$FX/data" --script "home 0" 2>/dev/null
    for kv in "$@"; do grep -q "^${kv%% = *} = " "$CONF" && sed -i "s/^${kv%% = *} = .*/$kv/" "$CONF" || echo "$kv" >> "$CONF"; done
    cp "$CONF" "$OUT/before.conf"
}
# run <size> <name> <libretro folder> <script>: one run of banana-ui, its log in $OUT/<size>-<name>.log
run() {
    size=$1 name=$2 log=$OUT/$1-$2.log
    KIWAMI_LIBRETRO=$OUT/$3 "$UI/banana-ui" --headless "$OUT/$size-" --size $size --assets "$UI/assets/rnds" \
        --systems "$PWD/system/rootfs/etc/banana/systems.conf" --roms "$FX/roms" --data "$FX/data" \
        --script "$4" 2> "$log" || { echo "FAIL $size $name: banana-ui exited $?"; fail=1; }
}
# settings.conf has these lines
has() {
    for l in "$@"; do grep -qx "$l" "$CONF" || { echo "FAIL $size $name: no '$l' in settings.conf: $(grep "^${l%% = *} " "$CONF")"; fail=1; }; done
}
# settings.conf differs from the card's before the run in these lines only (as "key = value", " " between them)
changed() {
    got=$(diff "$OUT/before.conf" "$CONF" | sed -n 's/^> //p' | tr '\n' ' ')
    [ "$got" = "$1" ] || { echo "FAIL $size $name: settings.conf changed '$got', not '$1'"; fail=1; }
}
shown() {
    for s in "$@"; do
        for p in top bot; do
            f=$OUT/$size-$s-$p.png
            [ -s "$f" ] && [ "$(convert "$f" -format '%[fx:standard_deviation>0.01]' info: 2>/dev/null)" = 1 ] || { echo "FAIL $size $name: $f missing or blank"; fail=1; }
        done
    done
}
differ() { cmp -s "$OUT/$size-$1-top.png" "$OUT/$size-$2-top.png" && { echo "FAIL $size $name: $1 and $2 are the same"; fail=1; }; true; }

for size in 640x480 1024x768; do
    # the five core rows with the fork's cores, a shot of each (each its own help); browsing writes nothing
    card
    run $size rows all "$OTHER; $(up 3); wait 400; shot md; press up; wait 400; shot nes; press up; wait 400; shot gb; press up; wait 400; shot gbc; press up; wait 400; shot gba"
    shown md nes gb gbc gba
    differ md nes; differ nes gb; differ gb gbc; differ gbc gba
    cmp -s "$OUT/before.conf" "$CONF" || { echo "FAIL $size rows: browsing changed settings.conf: $(diff "$OUT/before.conf" "$CONF" | tr '\n' ' ')"; fail=1; }

    # Right on each: the fork's core, after upstream's (Game Boy and Game Boy Color: Gambatte, mGBA, then SameBoy)
    card
    run $size set all "$OTHER; $(up 3); press right; press up; press right; press up; press right; press right; press up; press right; press right"
    has "core.megadrive = genesis_plus_gx" "core.nes = mesen" "core.gb = sameboy" "core.gbc = sameboy" "core.gba = gpsp"
    changed "core.megadrive = genesis_plus_gx core.nes = mesen core.gb = sameboy core.gbc = sameboy "

    # SameBoy alone: the Game Boy rows offer it, the NES and the Mega Drive have no row (Up three is the Game Boy)
    card
    run $size sameboy sameboy "$OTHER; $(up 3); press right; press right; press up; press right; press right"
    changed "core.gb = sameboy core.gbc = sameboy "

    # none of the fork's: upstream's rows alone, two choices each (Right twice goes round), and a card's core naming a
    # missing one kept
    card "core.nes = mesen"
    run $size upstream upstream "$OTHER; $(up 3); press right; press up; press right; press right"
    has "core.gb = mgba" "core.gbc = gambatte" "core.nes = mesen"
    changed "core.gb = mgba core.gbc = gambatte "
done
[ $fail = 0 ] && echo "cores-ui: all checks passed ($OUT)"
exit $fail
