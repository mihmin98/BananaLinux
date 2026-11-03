#!/bin/sh
# Home server > Show sync times in banana-sync (ui/src/kiwami/synctimes.h): with kiwami_sync_times = on, a RomM sync's
# statuses come as phases in a known order, the steps that had none of their own (Checking the card, Removing,
# Gamelists, Reading saves / states) among them, each with its first and last item and throttled in between;
# <run>/sync.times holds the sync's pid and the phase's start while it runs and is gone after; and sync.log ends with
# each phase's time. With the key missing, nothing of it: no new statuses, no times file, no times: lines. Against
# romm-big.py's library of 30 games a system, GBA's game 12 answered after 3 s so the Games phase can be looked at
# while it runs (game 11's null size ends each sync with errors: its status is "done" all the same). Also the header's
# unit tests (synctimes-units.cpp). Needs g++ and python3. Run by kiwami/tests/ci.sh, or by hand:
#   kiwami/tests/sync-times.sh
#   kiwami/tests/sync-times.sh --compare <other banana-sync>
# --compare (local, the final check against the build before the feature) also runs the other build's two syncs with
# the setting off, on a card of its own: their status lines, and their sync.log apart from its first line (the time),
# the peak memory and the free space, have to be this build's.
set -e
OTHER=
if [ "$1" = --compare ]; then
    [ -x "$2" ] || { echo "sync-times: --compare needs another banana-sync, not \"$2\"" >&2; exit 2; }
    OTHER=$(cd "$(dirname "$2")" && pwd)/$(basename "$2")
fi
cd "$(dirname "$0")/../.."
T=$(mktemp -d)
SRV=
trap '[ -z "$SRV" ] || kill $SRV 2>/dev/null; rm -rf "$T"' EXIT
fail() { echo "sync-times: $*" >&2; exit 1; }
failed=
err() { echo "sync-times: $*" >&2; failed=1; }

# banana-sync as ui/tests/sync.sh builds it, and the header's unit tests
${CXX:-g++} -std=c++17 -O2 -Wall -Wno-unused-function -o "$T/banana-sync" ui/src/sync.cpp ui/src/json.cpp -ldl
${CXX:-g++} -std=c++17 -Wall -Werror -I ui/src -o "$T/synctimes-units" kiwami/tests/synctimes-units.cpp
"$T/synctimes-units"

# serve <games a system> [slow game's id]: the stand-in RomM started afresh, its port in $T/port
serve() {
    [ -z "$SRV" ] || { kill $SRV 2>/dev/null; wait $SRV 2>/dev/null || true; }
    rm -f "$T/port"
    ROMM_BIG_SLOW=${2:-} python3 kiwami/tests/romm-big.py "$T/port" "$T/requests.log" "$1" 0 & SRV=$!
    for i in $(seq 100); do [ -s "$T/port" ] && break; sleep 0.1; done
    [ -s "$T/port" ] || fail "the stand-in RomM didn't start"
}
# card <dir> [settings.conf line...]: a card paired with the stand-in
card() {
    d=$1; shift
    mkdir -p "$d/banana" "$d/run" "$d/roms"
    printf '{"backend": "romm", "server": "http://127.0.0.1:%s", "token": "rmm_test", "name": "test"}\n' "$(cat "$T/port")" > "$d/banana/romsync.json"
    : > "$d/banana/settings.conf"
    for l in "$@"; do echo "$l" >> "$d/banana/settings.conf"; done
}
# repoint <dir>: the card's pairing moved to the stand-in's new port, the sync's state kept
repoint() { sed -i "s|http://127.0.0.1:[0-9]*|http://127.0.0.1:$(cat "$T/port")|g" "$1/banana/romsync.json"; }
BIN=$T/banana-sync
# bs <card> <out> <args...>: banana-sync ($BIN) -v on the card, its stdout in <out>, its stderr (status: lines) in <out>.err
bs() {
    c=$1 out=$2; shift 2
    "$BIN" --root "$c" --run "$c/run" --status "$c/run/sync.status" -v "$@" > "$out" 2> "$out.err"
}
# phases <err file>: the status lines' phases as synctimes::phaseOf() takes them ("running <a> <b> <what>|<item>" ->
# what without a trailing " i/N", Starting for none; another state's first word), consecutive ones joined
phases() {
    sed -n 's/^status: //p' "$1" | awk '
        {
            s = $0
            if (s ~ /^running /) {
                sub(/^running [^ ]* [^ ]*/, "", s)
                i = index(s, "|"); if (i) s = substr(s, 1, i - 1)
                gsub(/^[ \t]+|[ \t]+$/, "", s); sub(/ [0-9]+\/[0-9]+$/, "", s)
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
# ends <what> <err file> <phase> [W] [few]: the phase's first status is "running 1 <W> <phase>|<item>", its last
# "running <W> <W> <phase>|<item>" (W from the first, or the one given), and with few, fewer than W of them
ends() {
    grep "^status: running [0-9]* [0-9]* $3|" "$2" > "$T/lines" || true
    first=$(head -n 1 "$T/lines") last=$(tail -n 1 "$T/lines") n=$(wc -l < "$T/lines")
    W=$(echo "$first" | awk '{ print $4 }')
    [ -n "$W" ] || { err "$1: no $3 status"; return 0; }
    [ -z "$4" ] || [ "$W" = "$4" ] || err "$1: $3 counts $W, not $4"
    case "$first" in "status: running 1 $W $3|"?*) ;; *) err "$1: $3's first status: $first";; esac
    case "$last" in "status: running $W $W $3|"?*) ;; *) err "$1: $3's last status: $last";; esac
    [ -z "$5" ] || [ "$n" -lt "$W" ] || err "$1: $n $3 statuses for $W items, not throttled"
}
NEW='Checking the card|Removing|Gamelists|Reading saves|Reading states'

serve 30 200012
A=$T/card-a
card "$A" "kiwami_sync_times = on" "homesync_states = on"

# a sync with games: sync.times in the Games phase (the slow game holds it there), the sync's pid and starts
bs "$A" "$T/a1.out" sync & BS=$!
times= pid=
for i in $(seq 150); do
    times=$(cat "$A/run/sync.times" 2>/dev/null || true)
    case "$times" in *" Games") pid=$(cat "$A/run/sync.pid" 2>/dev/null || true); break;; esac
    sleep 0.1
done
wait $BS || true
case "$times" in
    *" Games")
        set -- $times
        [ "$1" = "$pid" ] || err "sync.times' pid $1, not sync.pid's $pid"
        [ "$2" -le "$3" ] || err "sync.times: the sync's start $2 after the phase's $3"
        ;;
    *) err "no sync.times in the Games phase in 15 s (last: $times)";;
esac
[ ! -e "$A/run/sync.times" ] || err "sync.times left after the sync: $(cat "$A/run/sync.times")"
P1='Starting,Listing games,Checking the card,Games,Gamelists,Reading saves,Saves,Reading states'
expect "the sync" "$T/a1.out.err" "$P1,done"
# the new phases' statuses: the first and the last item, throttled in between
ends "the sync" "$T/a1.out.err" "Checking the card" "" few
ends "the sync" "$T/a1.out.err" "Gamelists" 6 few
# the report: after memory:, a times: line a phase in order, and the whole sync's
LOG=$A/banana/logs/sync.log
sed -n '/^memory: /,$p' "$LOG" | tail -n +2 > "$T/times"
[ -s "$T/times" ] || err "sync.log: no lines after memory:"
grep -qvE '^times: .* [0-9]+\.[0-9] s(, .*)?$' "$T/times" && err "sync.log: a line after memory: isn't a times: line: $(grep -vE '^times: .* [0-9]+\.[0-9] s(, .*)?$' "$T/times" | head -n 1)"
got=$(sed 's/^times: \(.*\) [0-9][0-9]*\.[0-9] s.*$/\1/' "$T/times" | paste -sd, -)
[ "$got" = "$P1,sync" ] || err "sync.log: times of $got, not $P1,sync"
grep -qE '^times: Listing games [0-9]+\.[0-9] s, 180 of 180$' "$T/times" || err "sync.log: $(grep '^times: Listing games' "$T/times")"
grep -qE '^times: Games [0-9]+\.[0-9] s, [0-9]+ of [0-9]+ MB, [0-9]+ of [0-9]+ files$' "$T/times" || err "sync.log: $(grep '^times: Games' "$T/times")"
tail -n 1 "$T/times" | grep -qE '^times: sync [0-9]+\.[0-9] s$' || err "sync.log: the last line is $(tail -n 1 "$T/times")"

# the setting off: none of it, also polled while the slow game holds the sync
B=$T/card-b
card "$B" "homesync_states = on"
bs "$B" "$T/b1.out" sync & BS=$!
seen=
while kill -0 $BS 2>/dev/null; do [ ! -e "$B/run/sync.times" ] || seen=1; sleep 0.05; done
wait $BS && echo 0 > "$T/b1.rc" || echo $? > "$T/b1.rc"
[ -z "$seen" ] || err "off: sync.times written during the sync"
cp "$B/banana/logs/sync.log" "$T/b1.log"
bs "$B" "$T/b2.out" sync --saves-only && echo 0 > "$T/b2.rc" || echo $? > "$T/b2.rc"
cp "$B/banana/logs/sync.log" "$T/b2.log"
for r in b1 b2; do
    ! grep -qE "^status: running [0-9]+ [0-9]+ ($NEW)\|" "$T/$r.out.err" || err "off: $r wrote a new phase's status: $(grep -E "^status: running [0-9]+ [0-9]+ ($NEW)\|" "$T/$r.out.err" | head -n 1)"
    ! grep -q '^times: ' "$T/$r.log" || err "off: $r's sync.log has times: lines"
done
[ ! -e "$B/run/sync.times" ] || err "off: sync.times left after the syncs"

if [ -n "$OTHER" ]; then
    # the other build's two syncs off on a card of its own: the same statuses and the same report
    C=$T/card-c BIN=$OTHER
    card "$C" "homesync_states = on"
    bs "$C" "$T/c1.out" sync && echo 0 > "$T/c1.rc" || echo $? > "$T/c1.rc"
    cp "$C/banana/logs/sync.log" "$T/c1.log"
    bs "$C" "$T/c2.out" sync --saves-only && echo 0 > "$T/c2.rc" || echo $? > "$T/c2.rc"
    cp "$C/banana/logs/sync.log" "$T/c2.log"
    BIN=$T/banana-sync
    norm() { sed '1d; s/^memory: peak [0-9]* kB$/memory: peak <n> kB/; s/, [0-9.]* GB free/, <n> GB free/' "$1"; }
    for r in 1 2; do
        cmp -s "$T/b$r.rc" "$T/c$r.rc" || err "off: sync $r ended $(cat "$T/b$r.rc"), the other build's $(cat "$T/c$r.rc")"
        grep '^status: ' "$T/b$r.out.err" > "$T/b$r.st"; grep '^status: ' "$T/c$r.out.err" > "$T/c$r.st"
        cmp -s "$T/b$r.st" "$T/c$r.st" || { diff "$T/c$r.st" "$T/b$r.st" | head -n 10 >&2; err "off: sync $r's statuses aren't the other build's"; }
        norm "$T/b$r.log" > "$T/b$r.norm"; norm "$T/c$r.log" > "$T/c$r.norm"
        cmp -s "$T/b$r.norm" "$T/c$r.norm" || { diff "$T/c$r.norm" "$T/b$r.norm" | head -n 10 >&2; err "off: sync $r's sync.log isn't the other build's"; }
    done
    echo "sync-times: off, the same statuses ($(cat "$T/b1.st" "$T/b2.st" | wc -l)) and reports as the other banana-sync"
fi

# files gone from the server: a Removing phase, between the card checked and the gamelists (and a Games phase before
# it: game 11's six files, which failed on their null size, are tried again)
serve 25
repoint "$A"
bs "$A" "$T/a2.out" sync || true
expect "the second sync" "$T/a2.out.err" 'Starting,Listing games,Checking the card,Games,Removing,Gamelists,Reading saves,Saves,Reading states,done'
ends "the second sync" "$T/a2.out.err" "Removing"

# a saves-only sync (leaving a game)
bs "$A" "$T/a3.out" sync --saves-only || true
expect "the saves-only sync" "$T/a3.out.err" 'Starting,Listing games,Reading saves,Saves,Reading states,done'
[ ! -e "$A/run/sync.times" ] || err "sync.times left after the saves-only sync"

[ -z "$failed" ] || exit 1
echo "sync-times: all checks passed"
