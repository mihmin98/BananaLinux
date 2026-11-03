#!/bin/sh
# Network > Home server > Show sync times in banana-ui's Settings, headless at both panel sizes (sync-times.cpp): the
# line under Sync now's bar while a sync runs (its text, logged once a shape), its clocks moving between two shots, its
# time left only once the phase has run 3 s, nothing for a times file of another sync (its pid) or another phase, with
# the row Off, or on another row; and the row itself, right after Sync now, writing kiwami_sync_times. The sync is a
# stand-in: KIWAMI_SYNC_RUN names a folder with sync.status, sync.pid and sync.times as banana-sync writes them, and the
# headless clock is at 102401 ms on Sync now. Needs ui/banana-ui (make ui) and ImageMagick (the test library's art).
# Run by kiwami/tests/ci.sh when banana-ui is built, or by hand:
#   kiwami/tests/sync-times-ui.sh       (the shots in $OUT, /tmp/kiwami-sync-times by default)
set -e
cd "$(dirname "$0")/../.."
UI=$PWD/ui
OUT=${OUT:-${TMPDIR:-/tmp}/kiwami-sync-times}
FX=$OUT/fixture
RUN=$OUT/run
rm -rf "$OUT"; mkdir -p "$OUT" "$RUN"
"$UI/tests/fixture.sh" "$FX" >/dev/null 2>&1
CONF=$FX/data/settings.conf
fail=0
# Settings opens on its groups: R three times is Network, A its list, Up twice Home server (from Wi-Fi, round the end),
# A its page, Down Sync now. (The headless script updates Settings after a wait or a shot, not after a press.)
TO_SERVER="home 0; wait 2400; press start; press r; press r; press r; press a; press up; press up; press a"
ROW="$TO_SERVER; press down; wait 1"
GAMES='running 5 20 Games 3/10|Game 0003 (USA).gba'
D=$(printf '\302\267')	# (the line's ·)

# st <size> <name> <settings.conf lines> <status> <sync.times> <script>: one run of banana-ui with that sync running
st() {
    size=$1 name=$2 log=$OUT/$1-$2.log
    printf "$3\n" > "$CONF"
    echo "$4" > "$RUN/sync.status"
    echo 4242 > "$RUN/sync.pid"
    echo "$5" > "$RUN/sync.times"
    KIWAMI_SYNC_RUN=$RUN "$UI/banana-ui" --headless "$OUT/$size-" --size $size \
        --assets "$UI/assets/rnds" --systems "$PWD/system/rootfs/etc/banana/systems.conf" --roms "$FX/roms" --data "$FX/data" \
        --script "$6" 2> "$log" || { echo "FAIL $size $name: banana-ui exited $?"; fail=1; }
}
logged() { grep -qF -- "$1" "$log" || { echo "FAIL $size $name: no '$1' in the log"; fail=1; }; }
# the line logged exactly as given, and that once
once() {
    n=$(grep -cxF -- "banana-ui: kiwami: sync times: $1" "$log" || true)
    [ "$n" = 1 ] || { echo "FAIL $size $name: 'sync times: $1' logged $n times: $(grep 'sync times:' "$log" | head -n 3)"; fail=1; }
}
none() { ! grep -q 'sync times:' "$log" || { echo "FAIL $size $name: a line drawn: $(grep 'sync times:' "$log" | head -n 1)"; fail=1; }; }
shown() {
    f=$OUT/$size-$1-top.png
    [ -s "$f" ] && [ "$(convert "$f" -format '%[fx:standard_deviation>0.01]' info: 2>/dev/null)" = 1 ] || { echo "FAIL $size $name: $f missing or blank"; fail=1; }
}
same() { cmp -s "$OUT/$size-$1-top.png" "$OUT/$size-$2-top.png" || { echo "FAIL $size $name: $1 and $2 differ"; fail=1; }; }
differ() { cmp -s "$OUT/$size-$1-top.png" "$OUT/$size-$2-top.png" && { echo "FAIL $size $name: $1 and $2 are the same"; fail=1; }; true; }

for size in 640x480 1024x768; do
    ON="wifi = on\nkiwami_sync_times = on"
    # a Games phase 10.4 s in: its time left (files' 41.6 s over the MB's 31.2), the clocks moving with no key pressed
    st $size on-games "$ON" "$GAMES" "4242 2001 92001 Games" "$ROW; shot games-a; wait 2000; shot games-b"
    once "Games $D 0:10 $D 42 s left $D sync 1:40"
    shown games-a; shown games-b; differ games-a games-b
    grep 'shot games-' "$log" | sed "s/^/sync-times-ui: $size on: /"
    # 2 s into its phase: no time left yet; at 3.5 s, one (files' 14 s)
    st $size on-early "$ON" "$GAMES" "4242 2001 100401 Games" "$ROW; shot early-a; wait 1500; shot early-b"
    once "Games $D 0:02 $D sync 1:40"
    once "Games $D 0:03 $D 15 s left $D sync 1:41"
    [ "$(grep 'sync times:' "$log" | head -n 1)" = "banana-ui: kiwami: sync times: Games $D 0:02 $D sync 1:40" ] ||
        { echo "FAIL $size $name: the estimate before the line without it"; fail=1; }
    # Starting: nothing counted, no estimate
    st $size on-starting "$ON" "running 0 0 |My RomM" "4242 92001 92001 Starting" "$ROW; shot starting"
    once "Starting $D 0:10 $D sync 0:10"
    shown starting
    # a times file of another sync (a killed one's, or before banana-sync's first status), or of another phase: nothing
    st $size stale "$ON" "$GAMES" "4241 2001 92001 Games" "$ROW; shot stale"
    none; shown stale
    st $size phase-mismatch "$ON" "$GAMES" "4242 2001 92001 Saves" "$ROW; shot mismatch"
    none; same mismatch stale
    # the row Off (no key): upstream's bar alone, as with no line
    st $size off "wifi = on" "$GAMES" "4242 2001 92001 Games" "$ROW; shot off; wait 2000; shot off-b"
    none; same off stale; differ off games-a; same off off-b
    grep 'shot off' "$log" | sed "s/^/sync-times-ui: $size off: /"
    # another row (Server): nothing
    st $size other-row "$ON" "$GAMES" "4242 2001 92001 Games" "$TO_SERVER; wait 1; shot other"
    none
    # the row: right after Sync now, and A on it writes the setting
    st $size row "wifi = on" "$GAMES" "4242 2001 92001 Games" "$ROW; press down; press a; wait 1; shot row"
    logged "banana-ui: kiwami: rows in Home server: kiwami_sync_times"
    grep -qx 'kiwami_sync_times *= *"*on"*' "$CONF" || { echo "FAIL $size $name: settings.conf has no kiwami_sync_times = on: $(grep kiwami_sync "$CONF")"; fail=1; }
done

[ $fail = 0 ] || exit 1
echo "sync-times-ui: all checks passed ($OUT)"
