#!/bin/sh
# The fork's cores in banana-ui's Settings, headless at both panel sizes, against stand-in libretro folders
# (KIWAMI_LIBRETRO, empty files named as the cores): Other systems' rows for NES, Game Boy, Game Boy Color and Mega
# Drive after upstream's, shown only for the cores there; browsing writes nothing, Right and Left write each core and
# upstream's back, a missing core's setting is kept, Reset emulator settings puts upstream's back; Tailscale's row only
# in Network. Needs ui/banana-ui (make ui) and ImageMagick (the test library's art).
# Run by kiwami/tests/ci.sh when banana-ui is built, or by hand:
#   kiwami/tests/cores-ui.sh        (the shots in $OUT, /tmp/kiwami-cores by default)
set -e
cd "$(dirname "$0")/../.."
UI=$PWD/ui
OUT=${OUT:-${TMPDIR:-/tmp}/kiwami-cores}
FX=$OUT/fixture
rm -rf "$OUT"; mkdir -p "$OUT/all" "$OUT/sameboy" "$OUT/none"
: > "$OUT/all/mesen_libretro.so"; : > "$OUT/all/sameboy_libretro.so"; : > "$OUT/all/genesis_plus_gx_libretro.so"
: > "$OUT/sameboy/sameboy_libretro.so"
CONF=$FX/data/settings.conf
fail=0
# Settings opens on its groups: R three times is Other systems, A its list (on Screen); R five times is System. Down
# seven times from Screen is the first row after VSync (upstream's Game Boy Advance row needs mGBA in /usr/lib/libretro,
# which a build host hasn't got)
OTHER="home 0; press start; press r; press r; press r; press a"
SYSTEM="home 0; press start; press r; press r; press r; press r; press r; press a"
DOWN7="press down; press down; press down; press down; press down; press down; press down"

# a fresh card: the fixture, and the settings.conf banana-ui writes at its first start
card() {
    "$UI/tests/fixture.sh" "$FX" >/dev/null 2>&1
    "$UI/banana-ui" --headless "$OUT/$size-" --size $size --assets "$UI/assets/rnds" \
        --systems "$PWD/system/rootfs/etc/banana/systems.conf" --roms "$FX/roms" --data "$FX/data" --script "home 0" 2>/dev/null
    for kv in "$@"; do sed -i "s/^${kv%% = *} = .*/$kv/" "$CONF"; done
    cp "$CONF" "$OUT/before.conf"
}
# run <size> <name> <libretro folder> <script>: one run of banana-ui, its log in $OUT/<size>-<name>.log
run() {
    size=$1 name=$2 log=$OUT/$1-$2.log
    KIWAMI_LIBRETRO=$OUT/$3 "$UI/banana-ui" --headless "$OUT/$size-" --size $size --assets "$UI/assets/rnds" \
        --systems "$PWD/system/rootfs/etc/banana/systems.conf" --roms "$FX/roms" --data "$FX/data" \
        --script "$4" 2> "$log" || { echo "FAIL $size $name: banana-ui exited $?"; fail=1; }
}
# the log's "rows in" lines are exactly these (one a line)
rows() {
    got=$(grep '^banana-ui: kiwami: rows in ' "$log" | sed 's/^banana-ui: kiwami: rows in //' | tr '\n' '|')
    [ "$got" = "$1" ] || { echo "FAIL $size $name: the rows are '$got', not '$1'"; fail=1; }
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
    # the four rows after VSync, a shot of each (the help on the top panel, each its own); browsing writes nothing
    card
    run $size rows all "$OTHER; $DOWN7; wait 400; shot nes; press down; wait 400; shot gb; press down; wait 400; shot gbc; press down; wait 400; shot md"
    rows "Other systems: core.nes core.gb core.gbc core.megadrive|Network: kiwami.tailscale|RetroAchievements: kiwami.ra.account kiwami.ra.stats cheevos_hardcore_mode_enable|"
    shown nes gb gbc md
    differ nes gb; differ gb gbc; differ gbc md
    cmp -s "$OUT/before.conf" "$CONF" || { echo "FAIL $size rows: browsing changed settings.conf: $(diff "$OUT/before.conf" "$CONF" | tr '\n' ' ')"; fail=1; }

    # Right on each: the fork's core, the others as they were
    card
    run $size set all "$OTHER; $DOWN7; press right; press down; press right; press down; press right; press down; press right"
    [ "$(grep -c '^banana-ui: press right: settings result 2$' "$log")" = 4 ] || { echo "FAIL $size set: not four changes: $(grep 'press right' "$log" | tr '\n' ' ')"; fail=1; }
    has "core.nes = mesen" "core.gb = sameboy" "core.gbc = sameboy" "core.megadrive = genesis_plus_gx" \
        "core.gba = gpsp" "core.snes = snes9x" "core.psx = pcsx_rearmed" "core.n64 = mupen64plus_next"
    changed "core.gbc = sameboy core.gb = sameboy core.nes = mesen core.megadrive = genesis_plus_gx "

    # and back to upstream's, with Right (two choices: it goes round) and Left
    card "core.nes = mesen" "core.gb = sameboy" "core.gbc = sameboy" "core.megadrive = genesis_plus_gx"
    run $size back all "$OTHER; $DOWN7; press right; press down; press right; press down; press left; press down; press left"
    has "core.nes = fceumm" "core.gb = gambatte" "core.gbc = gambatte" "core.megadrive = picodrive"

    # Mega Drive is the last core row, before upstream's RetroArch hotkeys and Reset RetroArch (Up three times from
    # Screen goes round to it), no Tailscale row after it
    card
    run $size last all "$OTHER; press up; press up; press up; wait 400; shot last; press right"
    shown last
    changed "core.megadrive = genesis_plus_gx "

    # SameBoy alone: its two rows only
    card
    run $size sameboy sameboy "$OTHER; $DOWN7; press right; press down; press right"
    rows "Other systems: core.gb core.gbc|Network: kiwami.tailscale|RetroAchievements: kiwami.ra.account kiwami.ra.stats cheevos_hardcore_mode_enable|"
    changed "core.gbc = sameboy core.gb = sameboy "

    # none of them: upstream's rows only (VSync last before RetroArch hotkeys and Reset RetroArch), and a card's core
    # naming a missing one kept
    card "core.nes = mesen"
    run $size none none "$OTHER; press up; press up; press up; press right"
    rows "Other systems: none|Network: kiwami.tailscale|RetroAchievements: kiwami.ra.account kiwami.ra.stats cheevos_hardcore_mode_enable|"
    has "ra_vsync = off" "core.nes = mesen"
    changed "ra_vsync = off "

    # Reset emulator settings (Down eight from Time zone, A twice): upstream's cores back, the rows built again
    card "core.nes = mesen" "core.gb = sameboy" "core.gbc = sameboy" "core.megadrive = genesis_plus_gx" "core.gba = mgba"
    run $size reset all "$SYSTEM; press down; press down; press down; press down; press down; press down; press down; press down; press a; press a; wait 300"
    has "core.nes = fceumm" "core.gb = gambatte" "core.gbc = gambatte" "core.megadrive = picodrive" "core.gba = gpsp"
    rows "Other systems: core.nes core.gb core.gbc core.megadrive|Network: kiwami.tailscale|RetroAchievements: kiwami.ra.account kiwami.ra.stats cheevos_hardcore_mode_enable|Other systems: core.nes core.gb core.gbc core.megadrive|Network: kiwami.tailscale|RetroAchievements: kiwami.ra.account kiwami.ra.stats cheevos_hardcore_mode_enable|"
done
[ $fail = 0 ] && echo "cores-ui: all checks passed ($OUT)"
exit $fail
