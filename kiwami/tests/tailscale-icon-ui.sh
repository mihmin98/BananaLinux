#!/bin/sh
# Tailscale's icon in the status bar (ui/src/kiwami/statusbar.h), headless at both panel sizes on the real engine, with
# KIWAMI_TS_IFACE naming the interface whose address stands for the tailnet (lo: always up). Off, and with an
# interface that doesn't exist: no line, and the same shots. On: one line, the shift, the Wi-Fi icon's alpha, room
# before the date, the bar right of the clock's part and everything below it unchanged, Settings unchanged. The most
# crowded bar (Light, the percentage, charging, a 12-hour clock, a version note): room before the date. A colour theme
# (Ocean: Dark's engine in other colours): shown too. Carts and a layout theme: no line. Needs ui/banana-ui (make ui) and ImageMagick.
# Run by kiwami/tests/ci.sh when banana-ui is built, or by hand:
#   kiwami/tests/tailscale-icon-ui.sh        (the shots in $OUT, /tmp/kiwami-ui-tsicon by default)
set -e
cd "$(dirname "$0")/../.."
UI=$PWD/ui
OUT=${OUT:-${TMPDIR:-/tmp}/kiwami-ui-tsicon}
FX=$OUT/fixture
rm -rf "$OUT"; mkdir -p "$OUT"
"$UI/tests/fixture.sh" "$FX" >/dev/null 2>&1
CONF=$FX/data/settings.conf
export BANANA_THEMES=$PWD/system/rootfs/usr/share/banana/themes	# the image's layout themes, as user:<name>
fail=0
SCRIPT="home 0; wait 2400; shot home; lib 0 0; wait 2400; shot lib; press right; wait 100; shot rise; press start; wait 400; shot settings"
SHOTS="home lib rise settings"
# the Wi-Fi icon's alpha as the host's Library reads it
[ "$(cat /sys/class/net/wlan0/operstate 2>/dev/null)" = up ] && ALPHA=1.00 || ALPHA=0.35
im() { if command -v magick >/dev/null; then magick "$@"; else convert "$@"; fi; }

# run <size> <name> <settings.conf lines> [script]: banana-ui headless, its log in $OUT/<size>-<name>.log and its
# shots as $OUT/<size>-<name>-<shot>-{top,bot}.png; IFACE is KIWAMI_TS_IFACE and NOTE KIWAMI_BRAND_NOTE (empty: none)
run() {
    size=$1 name=$2 log=$OUT/$1-$2.log
    printf "$3\n" > "$CONF"
    KIWAMI_TS_IFACE=$IFACE KIWAMI_BRAND_NOTE=$NOTE "$UI/banana-ui" --headless "$OUT/$size-$name-" --size $size --assets "$UI/assets/rnds" --banana "$UI/assets/banana" \
        --systems "$PWD/system/rootfs/etc/banana/systems.conf" --roms "$FX/roms" --data "$FX/data" \
        --script "${4:-$SCRIPT}" 2> "$log" || { echo "FAIL $size $name: banana-ui exited $?"; fail=1; }
}
lines() { grep -c 'kiwami: tailscale icon' "$OUT/$1-$2.log" || true; }
# the shown line's field after <word> (moved, ends at, starts at, alpha)
field() { grep 'kiwami: tailscale icon: shown' "$OUT/$1-$2.log" | head -n 1 | sed -n "s/.*$3 \([0-9.]*\).*/\1/p"; }
# a shown line, its shift, and room before the date (round(8 x scale) at least)
shown() {
    size=$1 name=$2 log=$OUT/$1-$2.log
    [ "$(grep -c 'kiwami: tailscale icon: shown' "$log" || true)" = 1 ] && [ "$(lines $size $name)" = 1 ] ||
        { echo "FAIL $size $name: not one shown line: $(grep 'tailscale icon' "$log" | tr '\n' '|')"; fail=1; return; }
    [ "$(field $size $name moved)" = "$SHIFT" ] || { echo "FAIL $size $name: moved $(field $size $name moved) px, not $SHIFT"; fail=1; }
    e=$(field $size $name 'ends at') s=$(field $size $name 'starts at')
    [ -n "$e" ] && [ -n "$s" ] && awk -v e=$e -v s=$s -v g=$GAP 'BEGIN { exit !(e + g <= s) }' ||
        { echo "FAIL $size $name: the left part ends at '$e', $GAP px from the date at '$s' is too close"; fail=1; }
}
same() { cmp -s "$OUT/$size-$1.png" "$OUT/$size-$2.png" || { echo "FAIL $size: $1 and $2 differ"; fail=1; }; }
# the two shots' region <w>x<h>+<x>+<y> pixel for pixel
region() {
    d=$(im compare -metric AE -crop "$3" "$OUT/$size-$1.png" "$OUT/$size-$2.png" null: 2>&1 || true)
    [ "${d%% *}" = 0 ] || { echo "FAIL $size: $1 and $2 differ in $3 ($d pixels)"; fail=1; }
}
blank() {
    f=$OUT/$size-$1.png
    [ -s "$f" ] && [ "$(im "$f" -format '%[fx:standard_deviation>0.01]' info: 2>/dev/null)" = 1 ] || { echo "FAIL $size: $f missing or blank"; fail=1; }
}

for size in 640x480 1024x768; do
    W=${size%x*} H=${size#*x}
    case $size in 640x480) SHIFT=24 GAP=8 BAR=58 ;; *) SHIFT=39 GAP=13 BAR=93 ;; esac
    # the headless clock is the real time: the runs compared with each other are made within one minute
    [ "$(date +%S | sed 's/^0//')" -lt 45 ] || sleep $((61 - $(date +%S | sed 's/^0//')))

    # off: no interface, then one that doesn't exist; no line, and the same frames
    IFACE= NOTE=
    run $size off ""
    IFACE=kiwami-none0
    run $size none ""
    for n in off none; do [ "$(lines $size $n)" = 0 ] || { echo "FAIL $size $n: $(grep 'tailscale icon' "$OUT/$size-$n.log" | head -n 1)"; fail=1; }; done
    for s in $SHOTS; do same off-$s-top none-$s-top; same off-$s-bot none-$s-bot; done

    # on (lo), Dark: the line, the shift and the alpha; the top screens differ only in the bar's left part (right of
    # the clock's part and below the bar the same), the bottom screens and Settings not at all
    IFACE=lo
    run $size on ""
    shown $size on
    [ "$(field $size on alpha)" = $ALPHA ] || { echo "FAIL $size on: alpha $(field $size on alpha), not the Wi-Fi icon's $ALPHA"; fail=1; }
    grep -q 'no Wi-Fi icon' "$OUT/$size-on.log" && { echo "FAIL $size on: no Wi-Fi icon found"; fail=1; }
    s=$(field $size on 'starts at'); s=${s%.*}
    for v in home lib; do
        cmp -s "$OUT/$size-off-$v-top.png" "$OUT/$size-on-$v-top.png" && { echo "FAIL $size: $v's top screen unchanged"; fail=1; }
        [ -n "$s" ] && region off-$v-top on-$v-top "$((W - s))x$H+$s+0"
        region off-$v-top on-$v-top "${W}x$((H - BAR))+0+$BAR"
        same off-$v-bot on-$v-bot
    done
    same off-settings-top on-settings-top; same off-settings-bot on-settings-bot

    # the most crowded bar: Light, the percentage, charging at 100%, a 12-hour clock, the version note
    NOTE="Alpha 0.5.3.4"
    run $size crowded "theme = light\nbattery = percent\nclock = 12h" \
        "home 0; battery 100 charging; wait 2400; shot home; lib 0 0; wait 2400; shot lib"
    shown $size crowded

    # a colour theme (the image's Ocean: Dark's engine, its own colours): the same bar, the icon in its ink
    NOTE=
    run $size colour "theme = user:ocean" "home 0; wait 2400; shot home"
    shown $size colour

    # Carts and a layout theme (the image's Canvas, its screens drawn from its JSON): their own bars, no line
    run $size carts "theme = carts" "home 0; wait 2400; shot home; press a; wait 2400; shot lib"
    run $size layout "theme = user:canvas" "home 0; wait 2400; shot home; press a; wait 2400; shot lib"
    for n in colour carts layout; do
        [ $n = colour ] && { grep -q 'theme:' "$OUT/$size-$n.log" && { echo "FAIL $size $n: $(grep 'theme:' "$OUT/$size-$n.log" | head -n 1)"; fail=1; }; continue; }
        [ "$(lines $size $n)" = 0 ] || { echo "FAIL $size $n: $(grep 'tailscale icon' "$OUT/$size-$n.log" | head -n 1)"; fail=1; }
        grep -q 'theme:' "$OUT/$size-$n.log" && { echo "FAIL $size $n: $(grep 'theme:' "$OUT/$size-$n.log" | head -n 1)"; fail=1; }
    done

    for s in off-home on-home on-lib on-rise crowded-home colour-home carts-home layout-home; do blank $s-top; blank $s-bot; done
done
[ $fail = 0 ] && echo "tailscale-icon-ui: all checks passed ($OUT; the icon in its *-on-home-top.png and *-crowded-home-top.png)"
exit $fail
