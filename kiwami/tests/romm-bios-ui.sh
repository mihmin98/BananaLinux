#!/bin/sh
# Network > Home server > BIOS in banana-ui's Settings, headless at both panel sizes (romm-bios.cpp): the row, a
# paired RomM's only, the page's last (none for a Romhost or unpaired); its page, banana-sync's kiwami bios list
# shown with the missing files ticked, A unticking one, Start asking kiwami bios get for the ticked ones' ids and the
# list asked again after it; nothing ticked; RomM's answers "error scope" and a failed download; Wi-Fi off; and by
# touch, a row ticked and the Download button; a download's progress, file by file. banana-sync is kiwami/tests/fake-romm-sync.sh (BANANA_SYNC), which
# logs each call. Needs ui/banana-ui (make ui) and ImageMagick (the test library's art).
# Run by kiwami/tests/ci.sh when banana-ui is built, or by hand:
#   kiwami/tests/romm-bios-ui.sh       (the shots in $OUT, /tmp/kiwami-romm-bios by default)
set -e
cd "$(dirname "$0")/../.."
UI=$PWD/ui
OUT=${OUT:-${TMPDIR:-/tmp}/kiwami-romm-bios}
FX=$OUT/fixture
rm -rf "$OUT"; mkdir -p "$OUT"
"$UI/tests/fixture.sh" "$FX" >/dev/null 2>&1
CONF=$FX/data/settings.conf
# paired <backend>: the fixture paired with a RomM 5.4.1 or a Romhost (as banana-sync writes romsync.json)
paired() {
    printf '{"backend": "%s", "server": "http://home.example.com:8080", "token": "t", "serverName": "Shelf", "serverVersion": "5.4.1", "name": "RG DS"}\n' "$1" > "$FX/data/romsync.json"
}
fail=0
# Settings opens on its groups: R three times is Network, A its list, Up twice Home server (from Wi-Fi, round the end),
# A its page (on Server); Up from there is the page's last row, BIOS. (The headless script updates Settings after a
# wait or a shot, not after a press: each step's answer is taken at the wait after it)
TO_SERVER="home 0; wait 2400; press start; press r; press r; press r; press a; press up; press up; press a"
PAGE="$TO_SERVER; press up; wait 1; press a; wait 1"
# RomM's files, as kiwami bios list prints them: three systems' heads and seven files, more than the eight rows show
T=$(printf '\t')
LIST="11${T}nds${T}bios7.bin${T}16384${T}missing\n12${T}nds${T}BIOS9.BIN${T}4096${T}same\n13${T}nds${T}firmware.bin${T}262144${T}different
21${T}gba${T}gba_bios.bin${T}16384${T}missing\n31${T}psx${T}scph5501.bin${T}524288${T}missing
32${T}psx${T}scph1001.bin${T}524288${T}same\n33${T}psx${T}scph7001.bin${T}524288${T}missing\nok 7"

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
shown() {
    for s in top bot; do
        f=$OUT/$size-$1-$s.png
        [ -s "$f" ] && [ "$(convert "$f" -format '%[fx:standard_deviation>0.01]' info: 2>/dev/null)" = 1 ] || { echo "FAIL $size $name: $f missing or blank"; fail=1; }
    done
}
differ() { cmp -s "$OUT/$size-$1-$3.png" "$OUT/$size-$2-$3.png" && { echo "FAIL $size $name: $1 and $2 ($3) are the same"; fail=1; }; true; }

for size in 640x480 1024x768; do
    export FAKE_BIOS="$LIST"
    unset FAKE_BIOS_GET
    # A8: a paired RomM: the row after Show sync times and Installs from RomM, the page's last; the list, the missing
    # files ticked
    paired romm
    ui $size keys "wifi = on" "$PAGE; shot list; press down; press down; press down; press down; press down; press down; wait 1; shot scrolled; press up; press up; press up; press up; press up; press up; press a; wait 1; shot untick; press start; wait 1; shot got"
    logged "banana-ui: kiwami: rows in Home server: kiwami_sync_times kiwami_romm_installs kiwami.romm.bios"
    logged "banana-ui: kiwami: romm bios: 7 listed"
    logged "banana-ui: kiwami: romm bios: file 11 nds/bios7.bin missing, ticked"
    logged "banana-ui: kiwami: romm bios: file 12 nds/BIOS9.BIN same"
    unlogged "file 12 nds/BIOS9.BIN same, ticked"
    logged "banana-ui: kiwami: romm bios: file 13 nds/firmware.bin different"
    unlogged "file 13 nds/firmware.bin different, ticked"
    logged "banana-ui: kiwami: romm bios: file 33 psx/scph7001.bin missing, ticked"
    # A on the first file unticks it, Start downloads the other three missing ones, then the list is asked again
    logged "banana-ui: kiwami: romm bios: unticked bios7.bin"
    logged "banana-ui: kiwami: romm bios: get 21 31 33"
    called 1 "kiwami bios get 21 31 33"
    logged "banana-ui: kiwami: romm bios: got: ok 3 0 0"
    called 2 "kiwami bios list"
    shown list; shown scrolled; shown untick; shown got

    # A8: a download's progress, the job in the background (KIWAMI_BIOS_ASYNC) and banana-sync's status a second a
    # file (FAKE_BIOS_PACE): the four missing files, the first and the second coming, then the answer and the list
    # again (sleep: real time, the page updated)
    # (the run folder the test's own: banana-sync's status file goes there, BANANA_RUN; the list taken in first)
    mkdir -p "$OUT/run"
    BANANA_RUN=$OUT/run KIWAMI_BIOS_ASYNC=1 FAKE_BIOS_PACE=1 \
        ui $size progress "wifi = on" "$PAGE; sleep 500; press start; sleep 300; shot getting1; sleep 1000; shot getting2; sleep 1000; sleep 1000; sleep 1200; sleep 600; shot gotall"
    logged "banana-ui: kiwami: romm bios: get 11 21 31 33"
    logged "banana-ui: kiwami: romm bios: getting 1 of 4 bios7.bin"
    logged "banana-ui: kiwami: romm bios: getting 2 of 4 gba_bios.bin"
    logged "banana-ui: kiwami: romm bios: getting 3 of 4 scph5501.bin"
    logged "banana-ui: kiwami: romm bios: getting 4 of 4 scph7001.bin"
    logged "banana-ui: kiwami: romm bios: got: ok 4 0 0"
    called 2 "kiwami bios list"
    shown getting1; shown getting2; shown gotall
    differ list getting1 bot; differ getting1 getting2 bot; differ getting1 getting2 top; differ getting2 gotall bot
    [ ! -e "$OUT/run/kiwami-bios.status" ] || { echo "FAIL $size progress: kiwami-bios.status left"; fail=1; }
    differ list scrolled bot; differ list untick bot; differ untick got top
    # nothing ticked: no download
    ui $size none-ticked "wifi = on" "$PAGE; press a; press down; press down; press down; press a; press down; press a; press down; press down; press a; wait 1; press start; wait 1; shot none"
    logged "banana-ui: kiwami: romm bios: unticked scph7001.bin"
    called 0 "kiwami bios get.*"
    shown none
    # a download that failed, and one with a different file kept: their results (the list asked again after each)
    export FAKE_BIOS_GET="21${T}bios/gba_bios.bin${T}failed size differs\nerror gba_bios.bin: size differs"
    ui $size get-failed "wifi = on" "$PAGE; press start; wait 1; shot failed"
    logged "banana-ui: kiwami: romm bios: got: error gba_bios.bin: size differs"
    called 2 "kiwami bios list"
    shown failed
    export FAKE_BIOS_GET="ok 1 2 1"
    ui $size get-kept "wifi = on" "$PAGE; press start; wait 1; shot kept"
    logged "banana-ui: kiwami: romm bios: got: ok 1 2 1"
    differ failed kept top
    unset FAKE_BIOS_GET
    # A7: a pairing without firmware.read: its message, nothing to tick or download
    export FAKE_BIOS="error scope"
    ui $size scope "wifi = on" "$PAGE; shot scope; press start; wait 1"
    logged "banana-ui: kiwami: romm bios: not listed: error scope"
    called 0 "kiwami bios get.*"
    shown scope
    differ list scope top
    export FAKE_BIOS="$LIST"
    # Wi-Fi off: banana-sync isn't asked
    ui $size wifi-off "wifi = off" "$PAGE; shot wifi-off"
    logged "banana-ui: kiwami: romm bios: wifi off"
    called 0 "kiwami bios list"
    shown wifi-off
    # touch: a tap on the first file (the row under the DS's head) unticks it, the Download button downloads
    ui $size touch "wifi = on" "$PAGE; touch 1 300 140; wait 1; shot touch; touch 1 320 442; wait 1; shot touched"
    logged "banana-ui: kiwami: romm bios: unticked bios7.bin"
    called 1 "kiwami bios get 21 31 33"
    shown touch; shown touched
    # Romhost and unpaired: no BIOS row (Up from Server is Show sync times on a Romhost)
    paired romhost
    ui $size romhost "wifi = on" "$TO_SERVER; press up; wait 1; shot romhost"
    logged "banana-ui: kiwami: rows in Home server: kiwami_sync_times"
    unlogged "kiwami.romm.bios"
    rm -f "$FX/data/romsync.json"
    ui $size unpaired "wifi = on" "$TO_SERVER; wait 1; shot unpaired"
    unlogged "kiwami.romm.bios"
    called 0 "kiwami bios list"
done

[ $fail = 0 ] || exit 1
echo "romm-bios-ui: all checks passed ($OUT)"
