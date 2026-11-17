#!/bin/sh
# Status on the game panel (ui/src/kiwami/panelstatus.h, panel-status.cpp), headless at both panel sizes:
# - its row in Settings > Other systems > Second screen, right after upstream's Game panel idle time, Off on a fresh
#   card, A on it writing kiwami_panel_status = on;
# - off (no key, then off): no panel status or clocks line, the same panel both ways;
# - on (gamepanel 1 /nonexistent.gba, the tests' fixed clock 14:32:50 on 11/16 in UTC, Wi-Fi's operstate from a file,
#   lo standing for the tailnet): the line when the game starts, the panel above the strip as off, the strip where the
#   hint was; the strip drawn again at the minute (14:33) and at a minute played, with no line; the battery read every
#   10 s of the panel's time (50%, then 9% with the level in Dark's red, then charging: no red); Wi-Fi down and the
#   tailnet down each changing the left part;
# - Power scaling's clocks after Wi-Fi and Tailscale (the CPU's kHz and the launch script's mode file from files in
#   $OUT): Auto with a fresh mode file, the profile (Balanced, the Game Boy Advance's) with none or a stale one (a minute
#   older than the panel); the MHz read again at 2 s and drawn, with no new line;
# - the most crowded strip (a 12-hour clock, 100%, charging, the tailnet up, Performance 1992 MHz from a copy of
#   systems.conf where the Game Boy Advance's perf is performance, the system the gamepanel verb opens): its parts
#   apart (KIWAMI_PANEL_TRACE=1);
# - Light: the strip in its palette;
# - six tiles (the Keys tile): the strip, and the keyboard drawn as off;
# - the Game Boy Pokémon companion's pages drawn as off, and its menu with the strip.
# Needs ui/banana-ui (make ui) and ImageMagick. Run by kiwami/tests/ci.sh when banana-ui is built, or by hand:
#   kiwami/tests/panel-status-ui.sh       (the shots and logs in $OUT, /tmp/kiwami-panel-status-ui by default)
set -e
cd "$(dirname "$0")/../.."
UI=$PWD/ui
OUT=${OUT:-${TMPDIR:-/tmp}/kiwami-panel-status-ui}
FX=$OUT/fixture
CAP=$PWD/kiwami/tests/gbpk/fixtures
rm -rf "$OUT"; mkdir -p "$OUT"
"$UI/tests/fixture.sh" "$FX" >/dev/null 2>&1
CONF=$FX/data/settings.conf
fail=0
im() { if command -v magick >/dev/null; then magick "$@"; else convert "$@"; fi; }

# the stand-ins: a fixed clock (14:32:50 on 11/16) in UTC, Wi-Fi's operstate from a file, the tailnet's interface
export TZ=UTC KIWAMI_PANEL_CLOCK=1794839570 KIWAMI_WIFI_OPERSTATE=$OUT/operstate KIWAMI_PANEL_TRACE=1
export KIWAMI_GBPK_DIR=$PWD/kiwami/rootfs/usr/share/banana/companions/gb-pokemon
# the clocks' stand-ins: the CPU's kHz ($KHZ) and the mode file ($MODE: fresh, stale (60 s before the run) or none)
export KIWAMI_CPUFREQ_CUR=$OUT/cpufreq KIWAMI_CLOCKS_MODE=$OUT/kiwami-clocks
SYSTEMS=$PWD/system/rootfs/etc/banana/systems.conf
WIFI=up IFACE=lo VKBD= KHZ=1104000 MODE=

# run <size> <name> <settings.conf lines> <script>: one run of banana-ui, its log in $OUT/<size>-<name>.log and its
# shots as $OUT/<size>-<name>-<shot>-{top,bot}.png
run() {
    size=$1 name=$2 log=$OUT/$1-$2.log
    printf "$3\n" > "$CONF"
    echo "$WIFI" > "$OUT/operstate"
    echo "$KHZ" > "$KIWAMI_CPUFREQ_CUR"
    rm -f "$KIWAMI_CLOCKS_MODE"
    case $MODE in
        fresh) echo auto > "$KIWAMI_CLOCKS_MODE" ;;
        stale) echo auto > "$KIWAMI_CLOCKS_MODE"; touch -d "@$(($(date +%s) - 60))" "$KIWAMI_CLOCKS_MODE" ;;
    esac
    KIWAMI_TS_IFACE=$IFACE BANANA_TEST_VKBD=$VKBD "$UI/banana-ui" --headless "$OUT/$size-$name-" --size $size \
        --assets "$UI/assets/rnds" --systems "$SYSTEMS" --roms "$FX/roms" --data "$FX/data" \
        --script "$4" 2> "$log" || { echo "FAIL $size $name: banana-ui exited $?"; fail=1; }
}
logged() { grep -qE -- "$1" "$log" || { echo "FAIL $size $name: no '$1' in the log: $(grep 'kiwami' "$log" | tail -n 4 | tr '\n' ' ')"; fail=1; }; }
# the strip's lines (not the trace's), one per line
lines() { grep 'kiwami: panel status: [^d]' "$OUT/$size-$1.log" | sed 's/.*kiwami: //' || true; }
# the clocks' lines
clines() { grep 'kiwami: \(after a sleep: \)*panel clocks:' "$OUT/$size-$1.log" | sed 's/.*kiwami: //' || true; }
# the first line matching $1 comes before the first matching $2
before() {
    a=$(grep -nE -- "$1" "$log" | head -n 1 | cut -d: -f1) b=$(grep -nE -- "$2" "$log" | head -n 1 | cut -d: -f1)
    [ -n "$a" ] && [ -n "$b" ] && [ "$a" -lt "$b" ] || { echo "FAIL $size $name: '$1' isn't before '$2'"; fail=1; }
}
# <run>-<shot> pairs, bottom screens: the same, different, the same or different within <w>x<h>+<x>+<y>
same() { cmp -s "$OUT/$size-$1-bot.png" "$OUT/$size-$2-bot.png" || { echo "FAIL $size: $1 and $2 differ"; fail=1; }; }
differ() { cmp -s "$OUT/$size-$1-bot.png" "$OUT/$size-$2-bot.png" && { echo "FAIL $size: $1 and $2 are the same"; fail=1; }; true; }
pixels() { d=$(im compare -metric AE -crop "$3" "$OUT/$size-$1-bot.png" "$OUT/$size-$2-bot.png" null: 2>&1 || true); echo "${d%% *}"; }
region() { [ "$(pixels "$@")" = 0 ] || { echo "FAIL $size: $1 and $2 differ in $3 ($(pixels "$@") pixels)"; fail=1; }; }
regdiff() { [ "$(pixels "$@")" != 0 ] || { echo "FAIL $size: $1 and $2 are the same in $3"; fail=1; }; }
blank() {
    f=$OUT/$size-$1-bot.png
    [ -s "$f" ] && [ "$(im "$f" -format '%[fx:standard_deviation>0.01]' info: 2>/dev/null)" = 1 ] || { echo "FAIL $size: $f missing or blank"; fail=1; }
}
# pixels of colour $3 in shot $1 within $2
count() { im "$OUT/$size-$1-bot.png" -crop "$2" +repage -format %c histogram:info:- | grep -i "#$3" | sed 's/^ *\([0-9]*\):.*/\1/' | awk '{ n += $1 } END { print n + 0 }'; }
# the trace's field after <word> (left ends at, played, right starts at)
field() { grep 'kiwami: panel status: drawn' "$OUT/$size-$1.log" | tail -n 1 | sed -n "s/.*$2 \([0-9.]*\).*/\1/p"; }

PANEL="home 0; battery 78; gamepanel 1 /nonexistent.gba"
# a systems.conf whose Game Boy Advance runs performance
sed '/^\[gba\]/,/^\[/s/^perf = .*/perf = performance/' "$SYSTEMS" > "$OUT/systems-performance.conf"
grep -A6 '^\[gba\]' "$OUT/systems-performance.conf" | grep -qx 'perf = performance' || { echo "FAIL: no performance in the copy of systems.conf"; fail=1; }
for size in 640x480 1024x768; do
    W=${size%x*} H=${size#*x}
    k=$(awk -v w=$W 'BEGIN { print w / 640 }')
    Y=$(awk -v k=$k 'BEGIN { printf "%d", 424 * k }')		# the strip's band, below the tiles
    ABOVE="${W}x$Y+0+0" STRIP="${W}x$((H - Y))+0+$Y"
    LEFT="$(awk -v k=$k 'BEGIN { printf "%dx", 80 * k }')$((H - Y))+0+$Y"
    RIGHT="$((W / 2))x$((H - Y))+$((W / 2))+$Y"
    CLK="$(awk -v k=$k 'BEGIN { printf "%dx", 160 * k }')$((H - Y))+$(awk -v k=$k 'BEGIN { printf "%d", 90 * k }')+$Y"	# the clocks' label, x 90-250
    GAP=$(awk -v k=$k 'BEGIN { print (k > 1 ? 13 / k : 8) }')		# the parts apart: 8 px at 640x480, 13 at 1024x768

    # the row: Settings opens on its tiles, R is Other systems, A its list, Down four times Second screen, A its page;
    # Down three more is the row (after Second screen, Game panel when idle and Game panel idle time), A sets it On
    run $size row "wifi = on" "home 0; wait 2400; press start; press r; press a; press down; press down; press down; press down; press a; \
press down; press down; press down; wait 1; shot off; press a; wait 1; shot on"
    logged "banana-ui: kiwami: rows in Second screen: kiwami_pokemon_companion_gb kiwami_pokemon_companion_gbc kiwami_panel_status$"
    grep -qx 'kiwami_panel_status *= *"*on"*' "$CONF" || { echo "FAIL $size row: no kiwami_panel_status = on: $(grep kiwami_panel "$CONF")"; fail=1; }
    differ row-off row-on

    # off: no key, then off; no line, nothing read, the same panel
    run $size none "" "$PANEL; gshot a; wait 10000; gshot b"
    run $size off "kiwami_panel_status = off" "$PANEL; gshot a; wait 10000; gshot b"
    for n in none off; do [ -z "$(lines $n)" ] && ! grep -q 'panel status\|panel clocks' "$OUT/$size-$n.log" || { echo "FAIL $size $n: $(grep 'panel status\|panel clocks' "$OUT/$size-$n.log" | head -n 1)"; fail=1; }; done
    same none-a off-a; same off-a off-b

    # on: the line at the start; above the strip the panel as off, the strip where the hint was. The minute (14:33)
    # and a minute played: drawn again, no line. The battery read every 10 s: 50% not before, then 9% (low: Dark's red
    # in the right part), then 9% charging (the accent and the bolt: no red)
    run $size on "kiwami_panel_status = on" "$PANEL; gshot a; wait 10000; gshot b; wait 50000; gshot c; \
battery 50; wait 5000; gshot d; wait 5000; gshot e; battery 9; wait 10000; gshot low; battery 9 charging; wait 10000; gshot ch"
    expect="panel status: 14:32 11/16, battery 78%, wifi up, tailscale up, played 0:00
panel status: 14:34 11/16, battery 50%, wifi up, tailscale up, played 0:01
panel status: 14:34 11/16, battery 9%, wifi up, tailscale up, played 0:01
panel status: 14:34 11/16, battery 9%, charging, wifi up, tailscale up, played 0:01"
    [ "$(lines on)" = "$expect" ] || { echo "FAIL $size on: the lines: $(lines on | tr '\n' '|')"; fail=1; }
    [ "$(clines on)" = "panel clocks: Balanced 1104 MHz" ] || { echo "FAIL $size on: the clocks' lines: $(clines on | tr '\n' '|')"; fail=1; }
    before "gshot d" "battery 50%"
    region off-a on-a "$ABOVE"; regdiff off-a on-a "$STRIP"
    differ on-a on-b; differ on-b on-c; same on-c on-d; differ on-d on-e
    [ "$(count on-low "$RIGHT" ff5a4f)" -gt 0 ] || { echo "FAIL $size on: 9% has no red"; fail=1; }
    [ "$(count on-e "$RIGHT" ff5a4f)" = 0 ] || { echo "FAIL $size on: 50% has red"; fail=1; }
    [ "$(count on-ch "$RIGHT" ff5a4f)" = 0 ] || { echo "FAIL $size on: 9% charging has red"; fail=1; }

    # Wi-Fi down: dimmed; the tailnet down: no mark; the left part changes either way
    WIFI=down
    run $size wifidown "kiwami_panel_status = on" "$PANEL; gshot a"
    WIFI=up IFACE=kiwami-none0
    run $size tsdown "kiwami_panel_status = on" "$PANEL; gshot a"
    IFACE=lo
    [ "$(lines wifidown)" = "panel status: 14:32 11/16, battery 78%, wifi down, tailscale up, played 0:00" ] || { echo "FAIL $size wifidown: $(lines wifidown)"; fail=1; }
    [ "$(lines tsdown)" = "panel status: 14:32 11/16, battery 78%, wifi up, tailscale down, played 0:00" ] || { echo "FAIL $size tsdown: $(lines tsdown)"; fail=1; }
    regdiff on-a wifidown-a "$LEFT"; regdiff on-a tsdown-a "$LEFT"

    # the clocks: Auto with a fresh mode file, its label left of "Played" (the part between the mark and it not blank,
    # and not as Balanced's); a mode file older than the panel by a minute is a past game's: the profile
    MODE=fresh
    run $size auto "kiwami_panel_status = on" "$PANEL; gshot a"
    MODE=stale
    run $size stale "kiwami_panel_status = on" "$PANEL; gshot a"
    MODE=
    [ "$(clines auto)" = "panel clocks: Auto 1104 MHz" ] || { echo "FAIL $size auto: $(clines auto | tr '\n' '|')"; fail=1; }
    [ "$(clines stale)" = "panel clocks: Balanced 1104 MHz" ] || { echo "FAIL $size stale: $(clines stale | tr '\n' '|')"; fail=1; }
    [ "$(im "$OUT/$size-auto-a-bot.png" -crop "$CLK" +repage -format '%[fx:standard_deviation>0.01]' info:)" = 1 ] ||
        { echo "FAIL $size auto: no label in $CLK"; fail=1; }
    regdiff on-a auto-a "$CLK"; region on-a stale-a "$STRIP"; region on-a auto-a "$RIGHT"

    # the MHz read again at 2 s (816 written once the first shot is taken): the left part drawn again, no new line
    MODE=fresh
    rm -f "$OUT/$size-redraw.log"
    ( i=0; until grep -q 'gshot a' "$OUT/$size-redraw.log" 2>/dev/null || [ $i -ge 200 ]; do sleep 0.05; i=$((i + 1)); done
      echo 816000 > "$KIWAMI_CPUFREQ_CUR" ) &
    run $size redraw "kiwami_panel_status = on" "$PANEL; gshot a; sleep 1000; wait 1100; gshot b"
    wait
    MODE=
    [ "$(clines redraw)" = "panel clocks: Auto 1104 MHz" ] || { echo "FAIL $size redraw: the clocks' lines: $(clines redraw | tr '\n' '|')"; fail=1; }
    regdiff redraw-a redraw-b "$CLK"; region redraw-a redraw-b "$RIGHT"
    KHZ=1104000

    # the most crowded strip: a 12-hour clock, 100%, charging, the tailnet up; its three parts apart
    SYSTEMS=$OUT/systems-performance.conf KHZ=1992000
    run $size crowded "kiwami_panel_status = on\nclock = 12h" "home 0; battery 100 charging; gamepanel 1 /nonexistent.gba; gshot a"
    SYSTEMS=$PWD/system/rootfs/etc/banana/systems.conf KHZ=1104000
    lines crowded | grep -q '^panel status: 02:32 PM 11/16, battery 100%, charging, ' || { echo "FAIL $size crowded: $(lines crowded)"; fail=1; }
    [ "$(clines crowded)" = "panel clocks: Performance 1992 MHz" ] || { echo "FAIL $size crowded: $(clines crowded | tr '\n' '|')"; fail=1; }
    l=$(field crowded 'left ends at') p=$(field crowded played) r=$(field crowded 'right starts at')
    pe=$(grep 'kiwami: panel status: drawn' "$OUT/$size-crowded.log" | tail -n 1 | sed -n 's/.*played [0-9.]*-\([0-9.]*\),.*/\1/p')
    [ -n "$l" ] && [ -n "$p" ] && [ -n "$pe" ] && [ -n "$r" ] && awk -v l=$l -v p=$p -v e=$pe -v r=$r -v g=$GAP 'BEGIN { exit !(l + g <= p && e + g <= r) }' ||
        { echo "FAIL $size crowded: the parts too close: left to $l, played $p-$pe, right from $r (gap $GAP)"; fail=1; }

    # Light: the strip in its palette
    run $size light "kiwami_panel_status = on\ntheme = light" "$PANEL; gshot a"
    differ on-a light-a; regdiff on-a light-a "$STRIP"

    # six tiles (the Keys tile): the strip there; the keyboard drawn as off
    VKBD=1
    run $size sixoff "" "$PANEL; gshot a; gtouch 576 360; gshot keys"
    run $size six "kiwami_panel_status = on" "$PANEL; gshot a; gtouch 576 360; gshot keys"
    VKBD=
    logged "gtouch 576 360: action [0-9]* keys 1"
    [ -n "$(lines six)" ] || { echo "FAIL $size six: no line"; fail=1; }
    region sixoff-a six-a "$ABOVE"; regdiff sixoff-a six-a "$STRIP"; same sixoff-keys six-keys

    # the Game Boy Pokémon companion (Crystal, its captures): the pages drawn as off; its menu with the strip
    GB="home 0; battery 78; gbpanel 1 $CAP/crystal; gbhost; gbfeed $CAP/crystal; sleep 300; gshot party; gtouch 600 28; gshot menu"
    run $size gboff "" "$GB"
    run $size gb "kiwami_panel_status = on" "$GB"
    logged "gtouch 600 28: action 0 keys 0 view game$"
    same gboff-party gb-party; region gboff-menu gb-menu "$ABOVE"; regdiff gboff-menu gb-menu "$STRIP"

    for s in on-a on-low on-ch auto-a crowded-a light-a six-a six-keys gb-party gb-menu; do blank $s; done
done
[ $fail = 0 ] && echo "panel-status-ui: all checks passed ($OUT; the strip in its *-on-a-bot.png and *-crowded-a-bot.png)"
exit $fail
