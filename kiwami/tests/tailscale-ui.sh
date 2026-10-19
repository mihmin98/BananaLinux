#!/bin/sh
# Tailscale in banana-ui's Settings, headless at both panel sizes, against a stand-in helper (fake-tailscale.sh): the
# Network list's Tailscale row in each state (its value in the log, and shots), Tailscale's page opened and closed, and
# the helper never run while Tailscale is off. Needs ui/banana-ui (make ui) and ImageMagick (the test library's art).
# Run by kiwami/tests/ci.sh when banana-ui is built, or by hand:
#   kiwami/tests/tailscale-ui.sh        (the shots in $OUT, /tmp/kiwami-ui by default)
set -e
cd "$(dirname "$0")/../.."
UI=$PWD/ui
OUT=${OUT:-${TMPDIR:-/tmp}/kiwami-ui}
FX=$OUT/fixture
rm -rf "$OUT"; mkdir -p "$OUT"
"$UI/tests/fixture.sh" "$FX" >/dev/null
export KT_REC=$OUT/rec
STANDIN=$PWD/kiwami/tests/fake-tailscale.sh
fail=0
# Settings opens on its groups: R four times is Network, A its list, Down three times the Tailscale row
ROW="home 0; press start; press r; press r; press r; press r; press a; press down; press down; press down"

# ts <size> <name> <settings.conf line> <state> <network> <row expected> [script after the row]: one run of banana-ui
ts() {
    size=$1 name=$2 log=$OUT/$1-ts-$2.log
    printf '%s\n' "$3" > "$FX/data/settings.conf"
    : > "$KT_REC"
    KT_STATE=$4 KIWAMI_NETWORK=$5 KIWAMI_TAILSCALE=${HELPER:-$STANDIN} "$UI/banana-ui" --headless "$OUT/$size-" --size $size \
        --assets "$UI/assets/rnds" --systems "$PWD/system/rootfs/etc/banana/systems.conf" --roms "$FX/roms" --data "$FX/data" \
        --script "$ROW; shot ts-$name${7:+; $7}" 2> "$log" || { echo "FAIL $size $name: banana-ui exited $?"; fail=1; return; }
    grep -q "^banana-ui: tailscale: .*: row \"$6\"$" "$log" ||
        { echo "FAIL $size $name: the row isn't \"$6\": $(grep 'tailscale:' "$log" | tail -n 1)"; fail=1; }
    for p in top bot; do
        f=$OUT/$size-ts-$name-$p.png
        [ -s "$f" ] && [ "$(convert "$f" -format '%[fx:standard_deviation>0.01]' info: 2>/dev/null)" = 1 ] || { echo "FAIL $size $name: $f missing or blank"; fail=1; }
    done
}
# the helper asked exactly this (one call a line)
asked() {
    [ "$(cat "$KT_REC" | tr '\n' ' ')" = "$1" ] || { echo "FAIL $size $name: the helper was asked '$(cat "$KT_REC" | tr '\n' ' ')', not '$1'"; fail=1; }
}

for size in 640x480 1024x768; do
    # off: nothing run, whatever tailscaled would say
    ts $size unset "" running 1 Off; asked ""
    ts $size off "tailscale = off" running 1 Off; asked ""
    # on, but no helper in the image: Off, nothing run
    HELPER=$OUT/no-helper ts $size no-helper "tailscale = on" running 1 Off; asked ""
    # on: the helper's status asked once, when Settings opens
    ts $size not-yet "tailscale = on" off 1 Starting; asked "status "
    ts $size error "tailscale = on" error 1 Error
    ts $size starting "tailscale = on" starting 1 Starting
    ts $size no-wifi "tailscale = on" starting 0 "Waiting for Wi-Fi"
    ts $size login "tailscale = on" needs-login 1 "Log in"
    ts $size refused "tailscale = on" refused 1 "Log in"
    ts $size approval "tailscale = on" needs-approval 1 "Waiting for approval"
    ts $size expired "tailscale = on" expired 1 "Log in again"
    ts $size clock "tailscale = on" clock 1 "Log in"
    # connected; then its page: A opens it, B goes back (and the B chip by touch), START closes Settings from it
    ts $size running "tailscale = on" running 1 100.64.0.7 \
        "press a; shot ts-page; press b; shot ts-back; press a; touch 1 60 440; shot ts-touch; press a; press start"
    asked "status "
    for s in back touch; do
        cmp -s "$OUT/$size-ts-running-bot.png" "$OUT/$size-ts-$s-bot.png" || { echo "FAIL $size: the list after $s isn't the list before the page"; fail=1; }
    done
    cmp -s "$OUT/$size-ts-running-bot.png" "$OUT/$size-ts-page-bot.png" && { echo "FAIL $size: A on the row didn't open the page"; fail=1; }
    tail -n 1 "$log" | grep -q "press start: settings closed" || { echo "FAIL $size: START on the page didn't close Settings"; fail=1; }
    # still connected to Tailscale with no address of the handheld's own (Self online): the address
    ts $size running-no-wifi "tailscale = on" running 0 100.64.0.7
done
[ $fail = 0 ] && echo "tailscale-ui: all checks passed ($OUT)"
exit $fail
