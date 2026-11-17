#!/bin/sh
# Network > Home server > Installs from RomM in banana-ui's Settings, headless at both panel sizes (kiwami.cpp's
# installsUpdate()): the row, a paired RomM 5.4 or newer's, between Show sync times and BIOS, Off by default (none for
# a RomM 5.3.1, a Romhost or unpaired); turning it On writes kiwami_romm_installs = on and asks banana-sync's kiwami
# installs on, Off kiwami installs off; RomM refusing On puts the row back to Off; RomM not told of Off keeps Off; with
# Wi-Fi off banana-sync isn't asked and On is put back. banana-sync is kiwami/tests/fake-romm-sync.sh (BANANA_SYNC),
# which logs each call. Needs ui/banana-ui (make ui) and ImageMagick (the test library's art).
# Run by kiwami/tests/ci.sh when banana-ui is built, or by hand:
#   kiwami/tests/romm-installs-ui.sh       (the shots in $OUT, /tmp/kiwami-romm-installs by default)
set -e
cd "$(dirname "$0")/../.."
UI=$PWD/ui
OUT=${OUT:-${TMPDIR:-/tmp}/kiwami-romm-installs}
FX=$OUT/fixture
rm -rf "$OUT"; mkdir -p "$OUT"
"$UI/tests/fixture.sh" "$FX" >/dev/null 2>&1
CONF=$FX/data/settings.conf
# paired <backend> [version]: the fixture paired with a RomM (5.4.1 unless given) or a Romhost
paired() {
    printf '{"backend": "%s", "server": "http://home.example.com:8080", "token": "t", "serverName": "Shelf", "serverVersion": "%s", "name": "RG DS"}\n' "$1" "${2:-5.4.1}" > "$FX/data/romsync.json"
}
fail=0
# Settings opens on its groups: R three times is Network, A its list, Up twice Home server (from Wi-Fi, round the end),
# A its page (on Server); Up from there is the page's last row, BIOS, and Up again Installs from RomM. (The headless
# script updates Settings after a wait or a shot, not after a press: the row's change is seen at the wait after A, and
# banana-sync's answer at the next)
TO_SERVER="home 0; wait 2400; press start; press r; press r; press r; press a; press up; press up; press a"
ROW="$TO_SERVER; press up; press up; wait 1"
FLIP="press a; wait 1; wait 1"

# ui <size> <name> <settings.conf lines> <script>: one run of banana-ui, its log in $log and banana-sync's calls in $calls
ui() {
    size=$1 name=$2 log=$OUT/$1-$2.log calls=$OUT/$1-$2.calls
    printf "$3\n" > "$CONF"
    : > "$calls"
    BANANA_SYNC=$PWD/kiwami/tests/fake-romm-sync.sh FAKE_SYNC_LOG=$calls \
        "$UI/banana-ui" --headless "$OUT/$size-" --size $size \
        --assets "$UI/assets/rnds" --systems "$PWD/system/rootfs/etc/banana/systems.conf" --roms "$FX/roms" --data "$FX/data" \
        --script "$4" 2> "$log" || { echo "FAIL $size $name: banana-ui exited $?"; fail=1; }
}
logged() { grep -qF -- "$1" "$log" || { echo "FAIL $size $name: no '$1' in the log"; fail=1; }; }
unlogged() { ! grep -qF -- "$1" "$log" || { echo "FAIL $size $name: '$1' in the log: $(grep -F -- "$1" "$log" | head -n 1)"; fail=1; }; }
# called <count> <banana-sync's arguments from the verb on>: called that many times
called() {
    n=$(grep -c -- " $2\$" "$calls" || true)
    [ "$n" = "$1" ] || { echo "FAIL $size $name: '$2' called $n times, not $1: $(cat "$calls")"; fail=1; }
}
# setting <value>: settings.conf's kiwami_romm_installs
setting() {
    grep -qx "kiwami_romm_installs *= *\"*$1\"*" "$CONF" || { echo "FAIL $size $name: settings.conf's kiwami_romm_installs isn't $1: $(grep kiwami_romm "$CONF")"; fail=1; }
}
shown() {
    for s in top bot; do
        f=$OUT/$size-$1-$s.png
        [ -s "$f" ] && [ "$(convert "$f" -format '%[fx:standard_deviation>0.01]' info: 2>/dev/null)" = 1 ] || { echo "FAIL $size $name: $f missing or blank"; fail=1; }
    done
}
differ() { cmp -s "$OUT/$size-$1-$3.png" "$OUT/$size-$2-$3.png" && { echo "FAIL $size $name: $1 and $2 ($3) are the same"; fail=1; }; true; }

for size in 640x480 1024x768; do
    unset FAKE_INSTALLS
    # A15: a paired RomM 5.4.1: the row between Show sync times and BIOS, Off; On asks kiwami installs on and is kept,
    # Off again asks kiwami installs off
    paired romm
    ui $size flip "wifi = on" "$ROW; shot off; $FLIP; shot on; $FLIP; shot off-again"
    logged "banana-ui: kiwami: rows in Home server: kiwami_sync_times kiwami_romm_installs kiwami.romm.bios"
    logged "banana-ui: kiwami: romm installs: on asked"
    logged "banana-ui: kiwami: romm installs: on: ok"
    logged "banana-ui: kiwami: romm installs: off asked"
    logged "banana-ui: kiwami: romm installs: off: ok"
    called 1 "kiwami installs on"
    called 1 "kiwami installs off"
    setting off
    shown off; shown on; shown off-again
    differ off on bot
    # RomM refusing On (a pairing whose token isn't the device's own): put back to Off
    export FAKE_INSTALLS="error this pairing isn't the handheld's own: pair again from it (Server > Pair)"
    ui $size refused "wifi = on" "$ROW; $FLIP; shot refused"
    called 1 "kiwami installs on"
    logged "banana-ui: kiwami: romm installs: on: this pairing isn't the handheld's own: pair again from it (Server > Pair), put back"
    setting off
    shown refused
    # RomM not told of Off: Off kept
    ui $size off-failed "wifi = on\nkiwami_romm_installs = on" "$ROW; $FLIP; shot off-failed"
    called 1 "kiwami installs off"
    logged "banana-ui: kiwami: romm installs: off: this pairing isn't the handheld's own: pair again from it (Server > Pair)"
    unlogged "off: this pairing isn't the handheld's own: pair again from it (Server > Pair), put back"
    setting off
    unset FAKE_INSTALLS
    # Wi-Fi off: banana-sync not asked, On put back
    ui $size wifi-off "wifi = off" "$ROW; $FLIP; shot wifi-off"
    called 0 "kiwami installs .*"
    logged "banana-ui: kiwami: romm installs: on: Wi-Fi is off, put back"
    setting off
    # a RomM before 5.4, a Romhost, unpaired: no row
    paired romm 5.3.1
    ui $size old-romm "wifi = on" "$TO_SERVER; wait 1"
    logged "banana-ui: kiwami: rows in Home server: kiwami_sync_times kiwami.romm.bios"
    paired romhost
    ui $size romhost "wifi = on" "$TO_SERVER; wait 1"
    logged "banana-ui: kiwami: rows in Home server: kiwami_sync_times"
    unlogged "kiwami_romm_installs"
    rm -f "$FX/data/romsync.json"
    ui $size unpaired "wifi = on" "$TO_SERVER; wait 1"
    unlogged "kiwami_romm_installs"
    called 0 "kiwami installs .*"
    paired romm
done

[ $fail = 0 ] || exit 1
echo "romm-installs-ui: all checks passed ($OUT)"
