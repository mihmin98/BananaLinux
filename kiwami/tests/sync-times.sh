#!/bin/sh
# Home server > Show sync times in banana-sync (ui/src/kiwami/synctimes.h): with kiwami_sync_times = on, a RomM sync's
# statuses come as phases in a known order, the steps that had none of their own (Checking the card, Gamelists,
# Reading saves / states) among them, each with its first and last item and throttled in between; <run>/sync.times
# holds the sync's pid and the phase's start while it runs and is gone after; and sync.log ends with each phase's
# time. A sync of one game, before it or after it, never times. With the key missing, nothing of it: no new statuses,
# no times file, no times: lines. Against upstream's stand-in RomM 5.4.1 (ui/tests/fake-romm.py) with a small
# library: three GBA games and a DS game on the card (a save of the first), and two more GBA games for Get games, the
# downloads answered 3 s late so the Downloading phase can be looked at while it runs. Also the header's unit tests
# (synctimes-units.cpp). Needs g++, python3 and curl. Run by kiwami/tests/ci.sh, or by hand:
#   kiwami/tests/sync-times.sh
#   kiwami/tests/sync-times.sh --compare <other banana-sync>
# --compare (local, the final check against the Home server rework's own build) also runs the other build's syncs
# with the setting off, on a card and a server of its own: their status lines but the Listing games progress (the
# fork's 0022), and their sync.log apart from its first line (the time), the peak memory (0022's), the free space, the
# device and the card's folder, have to be this build's.
set -e
OTHER=
if [ "$1" = --compare ]; then
    [ -x "$2" ] || { echo "sync-times: --compare needs another banana-sync, not \"$2\"" >&2; exit 2; }
    OTHER=$(cd "$(dirname "$2")" && pwd)/$(basename "$2")
fi
cd "$(dirname "$0")/../.."
T=$(mktemp -d)
SRV=
trap '[ -z "$SRV" ] || kill $SRV 2>/dev/null; [ -n "$KEEP" ] || rm -rf "$T"' EXIT
fail() { echo "sync-times: $*" >&2; exit 1; }
failed=
err() { echo "sync-times: $*" >&2; failed=1; }

# banana-sync by ui/Makefile's own rule (its sources on the recipe line), and the header's unit tests
SRCS=$(awk '/^banana-sync: src\/sync\.cpp/ { getline; print; exit }' ui/Makefile | grep -o 'src/[A-Za-z0-9_/]*\.cpp' | sed 's|^|ui/|')
[ -n "$SRCS" ] || fail "no banana-sync rule in ui/Makefile"
# shellcheck disable=SC2086
${CXX:-g++} -std=c++17 -O2 -Wall -Wno-unused-function -o "$T/banana-sync" $SRCS -ldl
${CXX:-g++} -std=c++17 -Wall -Werror -I ui/src -o "$T/synctimes-units" kiwami/tests/synctimes-units.cpp
"$T/synctimes-units"
# (the image's systems.conf, and no RetroArch cores, as ui/tests/sync.sh runs it)
export BANANA_SYSTEMS="$PWD/system/rootfs/etc/banana/systems.conf" BANANA_LIBRETRO="$T/no-cores"

# the library, and what the cards hold of it (the same bytes for every card)
L=$T/lib
mkdir -p "$L/gba" "$L/nds" "$T/seed"
for i in 1 2 3 4 5; do head -c 70000 /dev/urandom > "$L/gba/Game $i (USA).gba"; done
head -c 300000 /dev/urandom > "$L/nds/Peel Quest (USA).nds"
head -c 131072 /dev/urandom > "$T/seed/save.srm"
# serve: the stand-in started afresh (a server of its own for each card: the slots are its state), its address in $U
serve() {
    [ -z "$SRV" ] || { kill $SRV 2>/dev/null; wait $SRV 2>/dev/null || true; }
    P=$(python3 -c 'import socket; s = socket.socket(); s.bind(("127.0.0.1", 0)); print(s.getsockname()[1])')
    python3 ui/tests/fake-romm.py "$P" "$L" 2>> "$T/fake-romm.log" & SRV=$!
    U=http://127.0.0.1:$P
    for i in $(seq 50); do curl -s "$U/api/heartbeat" > /dev/null && return 0; sleep 0.1; done
    fail "the stand-in RomM didn't start: $(tail -n 3 "$T/fake-romm.log")"
}
BIN=$T/banana-sync
# card <dir> [settings.conf line...]: a card paired with the stand-in by a pairing code, Auto sync and Sync before
# playing on (as the menu's pairing turns them on), its games and the first one's save, the lines added to its settings
card() {
    d=$1; shift
    mkdir -p "$d/banana" "$d/run" "$d/roms/gba" "$d/roms/nds" "$d/saves/gba"
    for i in 1 2 3; do cp "$L/gba/Game $i (USA).gba" "$d/roms/gba/"; done
    cp "$L/nds/Peel Quest (USA).nds" "$d/roms/nds/"
    cp "$T/seed/save.srm" "$d/saves/gba/Game 1 (USA).srm"
    code=$(curl -s -XPOST "$U/test/paircode" | python3 -c 'import json, sys; print(json.load(sys.stdin)["code"])')
    "$BIN" --root "$d" --run "$d/run" pair "$U" "$code" > /dev/null || fail "$d: pairing failed"
    printf 'homesync_auto = quit\nhomesync_launch = on\n' >> "$d/banana/settings.conf"
    for l in "$@"; do echo "$l" >> "$d/banana/settings.conf"; done
}
# bs <card> <out> <args...>: banana-sync ($BIN) -v on the card, its stdout in <out>, its stderr (status: lines) in <out>.err
bs() {
    c=$1 out=$2; shift 2
    "$BIN" --root "$c" --run "$c/run" --status "$c/run/sync.status" -v "$@" > "$out" 2> "$out.err"
}
# ask <card>: games 4 and 5 on Get games' list; slow: the next two downloads answered 3 s late
ask() { "$BIN" --root "$1" --run "$1/run" get "gba/Game 4 (USA).gba" "gba/Game 5 (USA).gba" > /dev/null; }
slow() { curl -s -XPOST "$U/test/fault" -d '{"path": "^GET /api/roms/[0-9]+/content/", "kind": "delay", "arg": 3, "count": 2}' > /dev/null; }
# phases <err file>: the status lines' phases as synctimes::phaseOf() takes them ("running <a> <b> <what>|<item>" ->
# what without a trailing " i/N" or " i of N", Downloading for "Downloading game", Starting for none; another
# state's first word), consecutive ones joined
phases() {
    sed -n 's/^status: //p' "$1" | awk '
        {
            s = $0
            if (s ~ /^running /) {
                sub(/^running [^ ]* [^ ]*/, "", s)
                i = index(s, "|"); if (i) s = substr(s, 1, i - 1)
                gsub(/^[ \t]+|[ \t]+$/, "", s); sub(/ [0-9]+\/[0-9]+$/, "", s); sub(/ [0-9]+ of [0-9]+$/, "", s)
                if (s == "Downloading game") s = "Downloading"
                if (s == "") s = "Starting"
            } else sub(/ .*/, "", s)
            if (s != last) { out = out (out == "" ? "" : ",") s; last = s }
        }
        END { print out }'
}
# expect <what> <err file> <phases>
expect() {
    got=$(phases "$2")
    [ "$got" = "$3" ] || err "$1: phases $got, not $3"
}
# ends <what> <err file> <phase> [W]: the phase's first status is "running 1 <W> <phase>|<item>", its last
# "running <W> <W> <phase>|<item>" (W from the first, or the one given)
ends() {
    grep "^status: running [0-9]* [0-9]* $3|" "$2" > "$T/lines" || true
    first=$(head -n 1 "$T/lines") last=$(tail -n 1 "$T/lines")
    W=$(echo "$first" | awk '{ print $4 }')
    [ -n "$W" ] || { err "$1: no $3 status"; return 0; }
    [ -z "$4" ] || [ "$W" = "$4" ] || err "$1: $3 counts $W, not $4"
    case "$first" in "status: running 1 $W $3|"?*) ;; *) err "$1: $3's first status: $first";; esac
    case "$last" in "status: running $W $W $3|"?*) ;; *) err "$1: $3's last status: $last";; esac
}
# report <what> <card> <phases>: sync.log ends, after memory:, with a times: line a phase in order and the whole sync's
report() {
    sed -n '/^memory: /,$p' "$2/banana/logs/sync.log" | tail -n +2 > "$T/times"
    [ -s "$T/times" ] || { err "$1: sync.log: no lines after memory:"; return 0; }
    ! grep -qvE '^times: .* [0-9]+\.[0-9] s(, .*)?$' "$T/times" || err "$1: sync.log: a line after memory: isn't a times: line: $(grep -vE '^times: .* [0-9]+\.[0-9] s(, .*)?$' "$T/times" | head -n 1)"
    got=$(sed 's/^times: \(.*\) [0-9][0-9]*\.[0-9] s.*$/\1/' "$T/times" | paste -sd, -)
    [ "$got" = "$3,sync" ] || err "$1: sync.log: times of $got, not $3,sync"
    tail -n 1 "$T/times" | grep -qE '^times: sync [0-9]+\.[0-9] s$' || err "$1: sync.log: the last line is $(tail -n 1 "$T/times")"
}
NEW='Checking the card|Removing|Gamelists|Reading saves|Reading states'

serve
A=$T/card-a
card "$A" "kiwami_sync_times = on" "homesync_states = on"

# Sync now with two games on Get games' list: the games first, then the saves. sync.times in the Downloading phase
# (the slow downloads hold it there): the sync's pid and its starts
ask "$A"; slow
bs "$A" "$T/a1.out" sync & BS=$!
times= pid=
for i in $(seq 150); do
    times=$(cat "$A/run/sync.times" 2>/dev/null || true)
    case "$times" in *" Downloading") pid=$(cat "$A/run/sync.pid" 2>/dev/null || true); break;; esac
    sleep 0.1
done
wait $BS || err "the sync failed: $(tail -n 3 "$T/a1.out")"
case "$times" in
    *" Downloading")
        set -- $times
        [ "$1" = "$pid" ] || err "sync.times' pid $1, not sync.pid's $pid"
        [ "$2" -le "$3" ] || err "sync.times: the sync's start $2 after the phase's $3"
        ;;
    *) err "no sync.times in the Downloading phase in 15 s (last: $times)";;
esac
[ ! -e "$A/run/sync.times" ] || err "sync.times left after the sync: $(cat "$A/run/sync.times")"
P1='Starting,Listing games,Checking the card,Downloading,Gamelists,Saves,Reading states'
expect "the sync" "$T/a1.out.err" "$P1,done"
# the new phases' statuses: the first and the last item (the two games' files, their system's gamelist)
ends "the sync" "$T/a1.out.err" "Checking the card" 2
ends "the sync" "$T/a1.out.err" "Gamelists"
ends "the sync" "$T/a1.out.err" "Reading states" 2
report "the sync" "$A" "$P1"
grep -qE '^times: Listing games [0-9]+\.[0-9] s, 6 of 6$' "$T/times" || err "sync.log: $(grep '^times: Listing games' "$T/times")"
grep -qE '^times: Downloading [0-9]+\.[0-9] s, [0-9]+ of [0-9]+ MB, 2 of 2 games$' "$T/times" || err "sync.log: $(grep '^times: Downloading' "$T/times")"
[ -s "$A/roms/gba/Game 5 (USA).gba" ] || err "the sync didn't bring Game 5"

# the next one, the slot holding the save sent: its head read (Reading saves)
bs "$A" "$T/a2.out" sync || err "the second sync failed: $(tail -n 3 "$T/a2.out")"
P2='Starting,Listing games,Reading saves,Saves,Reading states'
expect "the second sync" "$T/a2.out.err" "$P2,done"
ends "the second sync" "$T/a2.out.err" "Reading saves" 1
report "the second sync" "$A" "$P2"

# Get games' own run (--games-only): the games alone
"$BIN" --root "$A" --run "$A/run" get "nds/Peel Quest (USA).nds" > /dev/null
rm "$A/roms/nds/Peel Quest (USA).nds"
bs "$A" "$T/a3.out" sync --games-only || err "the games-only sync failed: $(tail -n 3 "$T/a3.out")"
P3='Starting,Listing games,Checking the card,Downloading,Gamelists'
expect "the games-only sync" "$T/a3.out.err" "$P3,done"
report "the games-only sync" "$A" "$P3"
grep -qE '^times: Downloading [0-9]+\.[0-9] s, [0-9]+ of [0-9]+ MB$' "$T/times" || err "the games-only sync: sync.log: $(grep '^times: Downloading' "$T/times")"

# one game's sync, before it (on launch.status) and after it (on gameend.status): never timed, polled while it runs
for w in launch quit; do
    cp "$L/gba/Game 1 (USA).gba" "$A/roms/gba/" 2> /dev/null || true
    "$BIN" --root "$A" --run "$A/run" -v sync --rom "gba/Game 1 (USA).gba" --auto $w > "$T/one-$w.out" 2> "$T/one-$w.err" & BS=$!
    seen=
    while kill -0 $BS 2>/dev/null; do [ ! -e "$A/run/sync.times" ] || seen=1; sleep 0.02; done
    wait $BS || err "one game's sync ($w) failed: $(tail -n 3 "$T/one-$w.out")"
    [ -z "$seen" ] && [ ! -e "$A/run/sync.times" ] || err "one game's sync ($w) wrote sync.times"
    ! grep -q '^times: ' "$T/one-$w.out" || err "one game's sync ($w) reported times"
    ! grep -qE "^status: running [0-9]+ [0-9]+ ($NEW)\|" "$T/one-$w.err" || err "one game's sync ($w) wrote a new phase's status: $(grep -E "^status: running [0-9]+ [0-9]+ ($NEW)\|" "$T/one-$w.err" | head -n 1)"
done
grep -q '^status: done ' "$T/one-launch.err" || err "the sync before a game didn't end: $(tail -n 1 "$T/one-launch.err")"

# the setting off: none of it, polled while the slow downloads hold the sync
# offrun <card> <prefix>: Sync now with games asked for, the next Sync now, and a games-only one, on that card
offrun() {
    ask "$1"; slow
    bs "$1" "$T/$2-1.out" sync & BS=$!
    seen=
    while kill -0 $BS 2>/dev/null; do [ ! -e "$1/run/sync.times" ] || seen=1; sleep 0.05; done
    wait $BS && echo 0 > "$T/$2-1.rc" || echo $? > "$T/$2-1.rc"
    [ -z "$seen" ] || err "off: sync.times written during $2's sync"
    cp "$1/banana/logs/sync.log" "$T/$2-1.log"
    bs "$1" "$T/$2-2.out" sync && echo 0 > "$T/$2-2.rc" || echo $? > "$T/$2-2.rc"
    cp "$1/banana/logs/sync.log" "$T/$2-2.log"
    "$BIN" --root "$1" --run "$1/run" get "nds/Peel Quest (USA).nds" > /dev/null
    rm "$1/roms/nds/Peel Quest (USA).nds"
    bs "$1" "$T/$2-3.out" sync --games-only && echo 0 > "$T/$2-3.rc" || echo $? > "$T/$2-3.rc"
    cp "$1/banana/logs/sync.log" "$T/$2-3.log"
}
serve
B=$T/card-b
card "$B" "homesync_states = on"
offrun "$B" b
for r in 1 2 3; do
    ! grep -qE "^status: running [0-9]+ [0-9]+ ($NEW)\|" "$T/b-$r.out.err" || err "off: sync $r wrote a new phase's status: $(grep -E "^status: running [0-9]+ [0-9]+ ($NEW)\|" "$T/b-$r.out.err" | head -n 1)"
    ! grep -q '^times: ' "$T/b-$r.log" || err "off: sync $r's sync.log has times: lines"
    [ "$(cat "$T/b-$r.rc")" = 0 ] || err "off: sync $r failed: $(tail -n 3 "$T/b-$r.out")"
done
[ ! -e "$B/run/sync.times" ] || err "off: sync.times left after the syncs"

if [ -n "$OTHER" ]; then
    # the other build's syncs off, on a card and a server of their own: the same statuses and the same report
    serve
    C=$T/card-c BIN=$OTHER
    card "$C" "homesync_states = on"
    offrun "$C" c
    BIN=$T/banana-sync
    # (each card's own folder and device, the time, the peak memory and the free space taken out; and the fork's
    # listing progress, 0022's)
    norm() { sed "1d; /^memory: peak [0-9]* kB\$/d; s/, [0-9.]* GB free/, <n> GB free/; s|$T/card-[bc]/|<card>/|g; s/[0-9a-f]\{8\}-[0-9a-f-]\{27\}/<uuid>/g" "$1"; }
    for r in 1 2 3; do
        cmp -s "$T/b-$r.rc" "$T/c-$r.rc" || err "off: sync $r ended $(cat "$T/b-$r.rc"), the other build's $(cat "$T/c-$r.rc")"
        for b in b c; do grep '^status: ' "$T/$b-$r.out.err" | grep -v '^status: running [0-9]* [0-9]* Listing games|' > "$T/$b-$r.st" || true; done
        cmp -s "$T/b-$r.st" "$T/c-$r.st" || { diff "$T/c-$r.st" "$T/b-$r.st" | head -n 10 >&2; err "off: sync $r's statuses aren't the other build's"; }
        norm "$T/b-$r.log" > "$T/b-$r.norm"; norm "$T/c-$r.log" > "$T/c-$r.norm"
        cmp -s "$T/b-$r.norm" "$T/c-$r.norm" || { diff "$T/c-$r.norm" "$T/b-$r.norm" | head -n 10 >&2; err "off: sync $r's sync.log isn't the other build's"; }
    done
    [ -n "$failed" ] || echo "sync-times: off, the same statuses ($(cat "$T"/b-?.st | wc -l)) and reports as the other banana-sync"
fi

[ -z "$failed" ] || exit 1
echo "sync-times: all checks passed"
