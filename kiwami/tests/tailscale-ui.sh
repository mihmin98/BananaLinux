#!/bin/sh
# Tailscale in banana-ui's Settings, headless at both panel sizes, against a stand-in helper (fake-tailscale.sh): the
# Network list's Tailscale row and Tailscale's page in each state (the row's value in the log, and shots), the
# login's QR code (read back with zbarimg when installed), switching it on and off, the device's name typed, logging
# out, the helper asked every 2 s while the page shows and never once it is closed or while Tailscale is off. Needs ui/banana-ui (make ui) and ImageMagick (the test library's art).
# Run by kiwami/tests/ci.sh when banana-ui is built, or by hand:
#   kiwami/tests/tailscale-ui.sh        (the shots in $OUT, /tmp/kiwami-ui by default)
set -e
cd "$(dirname "$0")/../.."
UI=$PWD/ui
OUT=${OUT:-${TMPDIR:-/tmp}/kiwami-ui}
FX=$OUT/fixture
rm -rf "$OUT"; mkdir -p "$OUT"
"$UI/tests/fixture.sh" "$FX" >/dev/null 2>&1
export KT_REC=$OUT/rec
STANDIN=$PWD/kiwami/tests/fake-tailscale.sh
CONF=$FX/data/settings.conf
fail=0
# Settings opens on its groups: R four times is Network, A its list, Down three times the Tailscale row. (The headless
# script updates Settings after a wait, a shot or a touch, not after a press: "wait 1" takes the helper's answer in.)
ROW="home 0; press start; press r; press r; press r; press r; press a; press down; press down; press down; wait 1"
# a key of the keyboard by touch (row 0-4, column 0-9), as ui/tests/run.sh types
key() { for rc in "$@"; do awk -v rc=$rc 'BEGIN { split(rc, a, ","); printf "touch 1 %.0f %.0f; ", 18 + a[2] * 60.4 + 27, 80 + a[1] * 66 + 29 }'; done; }

# ts <size> <name> <settings.conf lines> <state> <network> <row expected> <script after the row> [shots]: one run of
# banana-ui on the Tailscale row; the row's value checked in the log and the shots for being there and not blank
ts() {
    size=$1 name=$2 log=$OUT/$1-ts-$2.log
    printf "$3\n" > "$CONF"
    : > "$KT_REC"
    KT_STATE=$4 KIWAMI_NETWORK=$5 KIWAMI_TAILSCALE=${HELPER:-$STANDIN} "$UI/banana-ui" --headless "$OUT/$size-" --size $size \
        --assets "$UI/assets/rnds" --systems "$PWD/system/rootfs/etc/banana/systems.conf" --roms "$FX/roms" --data "$FX/data" \
        --script "$ROW; $7" 2> "$log" || { echo "FAIL $size $name: banana-ui exited $?"; fail=1; return; }
    grep -q "^banana-ui: tailscale: .*: row \"$6\"$" "$log" ||
        { echo "FAIL $size $name: the row isn't \"$6\": $(grep 'tailscale:' "$log" | tail -n 1)"; fail=1; }
    for s in $8; do
        for p in top bot; do
            f=$OUT/$size-$s-$p.png
            [ -s "$f" ] && [ "$(convert "$f" -format '%[fx:standard_deviation>0.01]' info: 2>/dev/null)" = 1 ] || { echo "FAIL $size $name: $f missing or blank"; fail=1; }
        done
    done
}
# the helper was asked exactly this (one call a line)
asked() {
    [ "$(tr '\n' '|' < "$KT_REC")" = "$1" ] || { echo "FAIL $size $name: the helper was asked '$(tr '\n' '|' < "$KT_REC")', not '$1'"; fail=1; }
}
same() { cmp -s "$OUT/$size-$1-bot.png" "$OUT/$size-$2-bot.png" || { echo "FAIL $size $name: $1 and $2 differ"; fail=1; }; }
differ() { cmp -s "$OUT/$size-$1-$3.png" "$OUT/$size-$2-$3.png" && { echo "FAIL $size $name: $1 and $2 are the same"; fail=1; }; true; }
logged() { grep -qF -- "$1" "$log" || { echo "FAIL $size $name: no '$1' in the log"; fail=1; }; }

for size in 640x480 1024x768; do
    # each state: the row, then the page (A); off, nothing is run, the page open or not
    page() { ts $size $1 "$2" $3 $4 "$5" "shot ts-$1; press a; shot tsp-$1" "ts-$1 tsp-$1"; }
    page unset "" running 1 Off; asked ""
    page off "tailscale = off" running 1 Off; asked ""
    HELPER=$OUT/no-helper page no-helper "tailscale = on" running 1 Off; asked ""
    # on: asked when Settings opens, and again when the page opens
    page not-yet "tailscale = on" off 1 Starting; asked "status|status|"
    page error "tailscale = on" error 1 Error
    page starting "tailscale = on" starting 1 Starting
    page no-wifi "tailscale = on" starting 0 "Waiting for Wi-Fi"
    page login "tailscale = on" needs-login 1 "Log in"
    page refused "tailscale = on" refused 1 "Log in"
    page approval "tailscale = on" needs-approval 1 "Waiting for approval"
    page expired "tailscale = on" expired 1 "Log in again"
    page clock "tailscale = on" clock 1 "Log in"
    page running "tailscale = on" running 1 100.64.0.7
    # still connected to Tailscale with no address of the handheld's own (Self online): the address
    page running-no-wifi "tailscale = on" running 0 100.64.0.7

    # the login's QR code, at 8 device pixels a module (12 at 1024x768): made for the stand-in's link, and read back
    # from the shot when zbarimg is installed (CI doesn't install it)
    for s in login:1a2b3c4d5e6f7a refused:1a2b3c4d5e6f7a expired:9f8e7d6c5b4a3b; do
        name=${s%:*} url=https://login.tailscale.com/a/${s#*:}
        grep -q "tailscale: login code 41x41 modules for $url$" "$OUT/$size-ts-$name.log" || { echo "FAIL $size $name: no login code for $url"; fail=1; }
        if command -v zbarimg >/dev/null; then
            got=$(zbarimg -q --raw "$OUT/$size-tsp-$name-top.png" 2>/dev/null || true)
            [ "$got" = "$url" ] || { echo "FAIL $size $name: the code reads '$got', not $url"; fail=1; }
        fi
    done
    command -v zbarimg >/dev/null || [ $size != 640x480 ] || echo "tailscale-ui: no zbarimg, the QR codes aren't read back"

    # B goes back to the list (and the B chip by touch), START closes Settings from the page
    ts $size back "tailscale = on" running 1 100.64.0.7 \
        "shot ts-list; press a; press b; shot ts-back; press a; touch 1 60 440; shot ts-touch; press a; press start" "ts-list"
    same ts-list ts-back; same ts-list ts-touch
    tail -n 1 "$log" | grep -q "press start: settings closed" || { echo "FAIL $size: START on the page didn't close Settings"; fail=1; }

    # switched on (Right) and off again (a tap on its value): settings.conf, the helper's up and down, and asked in
    # between only while on
    ts $size onoff "tailscale = off" needs-login 1 Off \
        "press a; shot tsp-onoff-off; press right; shot tsp-onoff-on; touch 1 500 110; shot tsp-onoff-off2; wait 3000; wait 3000" \
        "tsp-onoff-off tsp-onoff-on"
    asked "up|status|down|"
    logged 'tailscale: switched on: row "Starting"'
    logged 'tailscale: switched off: row "Off"'
    grep -qx 'tailscale = off' "$CONF" || { echo "FAIL $size onoff: settings.conf: $(tr '\n' ' ' < "$CONF")"; fail=1; }
    differ tsp-onoff-off tsp-onoff-on top; same tsp-onoff-off tsp-onoff-off2

    # every 2 s while the page shows, then nothing once B closed it
    ts $size poll "tailscale = on" running 1 100.64.0.7 "press a; wait 1; wait 2000; wait 2000; press b; wait 5000; wait 5000"
    asked "status|status|status|status|"

    # the name: typed by touch (Kiwi 2: Shift, K, Shift, i w i, Space, 2), kept as kiwi-2 and passed to the helper;
    # then one with an underscore (the symbols' _) refused, the keyboard staying with its message
    ts $size name "tailscale = on\ntailscale_name = x" running 1 100.64.0.7 \
        "press a; wait 1; press down; press down; shot tsp-name-row; press a; shot ts-name-kbd; press b; $(key 4,0 2,7 4,0 1,7 1,1 1,7 4,4 0,1) press start; shot tsp-named; press a; $(key 4,2 0,5) press start; shot ts-name-bad" \
        "tsp-name-row ts-name-kbd tsp-named ts-name-bad"
    asked "status|status|name kiwi-2|"
    grep -qx 'tailscale_name = kiwi-2' "$CONF" || { echo "FAIL $size name: settings.conf: $(tr '\n' ' ' < "$CONF")"; fail=1; }
    logged 'tailscale: name "kiwi-2_" refused'
    tail -n 1 "$log" | grep -q "shot ts-name-bad" && grep "press start" "$log" | tail -n 1 | grep -q "settings result 1" ||
        { echo "FAIL $size name: the bad name closed the keyboard"; fail=1; }
    differ tsp-name-row tsp-named bot

    # log out: Up wraps to it; the first A only arms it, the second runs the helper's logout, asked again at once
    ts $size logout "tailscale = on" running 1 100.64.0.7 \
        "press a; wait 1; press up; shot tsp-logout; press a; shot tsp-logout-armed; press a; shot tsp-logout-done" \
        "tsp-logout tsp-logout-armed tsp-logout-done"
    asked "status|status|logout|status|"
    differ tsp-logout tsp-logout-armed bot

    # Devices (when connected): this one, those online, the rest, by name; eight of them, five rows: Down scrolls, wraps
    # round to the top; a tap selects; the selected one in full on the top panel; B back to the menu
    ts $size devices "tailscale = on" running 1 100.64.0.7 \
        "press a; wait 1; press down; shot tsp-devrow; press a; shot tsp-devices; press down; press down; press down; press down; press down; press down; shot tsp-devices-down; press down; shot tsp-device-offline; press down; shot tsp-devices-wrap; touch 1 300 238; shot tsp-device-tap; wait 2000; press b; shot tsp-devback" \
        "tsp-devices tsp-devices-down tsp-device-offline tsp-device-tap"
    logged "tailscale: devices: banana-rgds (this), alexs-iphone, desktop, nas, raspberrypi, banana-rgds-plus (offline), living-room-tv (offline), macbook (offline)"
    same tsp-devrow tsp-devback; same tsp-devices tsp-devices-wrap
    differ tsp-devrow tsp-devices bot; differ tsp-devices tsp-devices-down bot; differ tsp-devices-down tsp-device-offline top
    differ tsp-devices tsp-device-tap top; differ tsp-devices tsp-device-tap bot
    asked "status|status|status|"      # (asked as the menu is, while the devices show)
done
[ $fail = 0 ] && echo "tailscale-ui: all checks passed ($OUT)"
exit $fail
