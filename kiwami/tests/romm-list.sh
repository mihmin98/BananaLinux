#!/bin/sh
# banana-sync's RomM listing on a large library: RomM's games asked for 50 at a time, without provider metadata, each
# kept to the fields Romm::manifest() reads (ui/src/kiwami/syncmem.h), so Choose games (banana-sync games), Sync now
# and a saves-only sync stay under 64 MB where upstream's held every page's whole tree. Against romm-big.py's 400
# games on each of six systems (and 3ds, which Banana has no system for), about 25 KB of JSON a game with RomM's
# provider blocks: the peak of each run (the child's ru_maxrss), the requests made, the games listed with RomM's edge
# cases, the dry run's files, the saves downloaded by name, and the peak memory each sync's report ends with. Also
# slim()'s unit tests (syncmem-units.cpp), and the guard: every key Romm::manifest() reads is kept by syncmem.h, so a
# field upstream starts reading isn't dropped. Needs g++ and python3. Run by kiwami/tests/ci.sh, or by hand:
#   kiwami/tests/romm-list.sh
#   kiwami/tests/romm-list.sh --compare <other banana-sync>
# --compare (local, the final check against main's build) also runs the other build on the same library: its games
# list and dry run have to be the same as this one's, and its peak at least 4 times the bound (the library reproduces
# upstream's problem). Then each build syncs a library of 30 games a system for real, and a saves-only sync after,
# onto a card of its own: the two cards' roms/ and saves/, and romsync.json's state, have to be the same.
set -e
OTHER=
if [ "$1" = --compare ]; then
    [ -x "$2" ] || { echo "romm-list: --compare needs another banana-sync, not \"$2\"" >&2; exit 2; }
    OTHER=$(cd "$(dirname "$2")" && pwd)/$(basename "$2")
fi
cd "$(dirname "$0")/../.."
T=$(mktemp -d)
SRV= SRV2=
trap '[ -z "$SRV" ] || kill $SRV 2>/dev/null; [ -z "$SRV2" ] || kill $SRV2 2>/dev/null; rm -rf "$T"' EXIT
fail() { echo "romm-list: $*" >&2; exit 1; }
failed=
err() { echo "romm-list: $*" >&2; failed=1; }

# banana-sync as ui/tests/sync.sh builds it, and slim()'s unit tests
${CXX:-g++} -std=c++17 -O2 -Wall -Wno-unused-function -o "$T/banana-sync" ui/src/sync.cpp ui/src/json.cpp -ldl
${CXX:-g++} -std=c++17 -Wall -Werror -I ui/src -o "$T/syncmem-units" kiwami/tests/syncmem-units.cpp ui/src/json.cpp
"$T/syncmem-units"

# the guard: the keys Romm::manifest() reads (str/num/flag/get on the game, its files and its metadatum), each kept
# by syncmem.h; first on a copy of sync.cpp that reads one more, which it has to catch
guard() {
    keys=$(awk '/struct Romm : Backend/ { r = 1 } r && /bool manifest\(/ { m = 1 } m && /bool matchRom\(/ { exit } m' "$1" |
        grep -o '\(str\|num\|flag\|get\)("[a-z_0-9]*"' | sed 's/.*("//; s/"$//' | sort -u)
    [ -n "$keys" ] || { echo "Romm::manifest() not found in $1"; return 1; }
    for k in $keys; do
        grep -q "\"$k\"" ui/src/kiwami/syncmem.h || { echo "Romm::manifest() reads \"$k\", which syncmem.h drops"; return 1; }
    done
    echo "$keys" | wc -l
}
sed '/std::string cover = it->str("path_cover_large");/a std::string ext = it->str("fs_extension");' ui/src/sync.cpp > "$T/sync-more.cpp"
grep -q 'fs_extension' "$T/sync-more.cpp" || fail "the guard's self-test: no line to add a key after in ui/src/sync.cpp"
[ "$(guard "$T/sync-more.cpp")" = 'Romm::manifest() reads "fs_extension", which syncmem.h drops' ] ||
    fail "the guard missed a key Romm::manifest() reads and syncmem.h drops"
KEYS=$(guard ui/src/sync.cpp) || fail "$KEYS"

# the stand-in, and a card paired with it
python3 kiwami/tests/romm-big.py "$T/port" "$T/requests.log" 400 40 & SRV=$!
for i in $(seq 100); do [ -s "$T/port" ] && break; sleep 0.1; done
[ -s "$T/port" ] || fail "the stand-in RomM didn't start"
# card <dir> <port file>: a card paired with the stand-in at that port
card() {
    mkdir -p "$1/banana" "$1/run" "$1/roms"
    printf '{"backend": "romm", "server": "http://127.0.0.1:%s", "token": "rmm_test", "name": "test"}\n' "$(cat "$2")" > "$1/banana/romsync.json"
}
C=$T/card BIN=$T/banana-sync
card "$C" "$T/port"
# bs <out> <args...>: banana-sync ($BIN) on the card ($C), its stdout in <out>; its peak resident size (kB) in $KB
bs() {
    out=$1; shift
    KB=$(python3 -c 'import resource, subprocess, sys
with open(sys.argv[1], "w") as o:
    rc = subprocess.call(sys.argv[2:], stdout=o)
print(resource.getrusage(resource.RUSAGE_CHILDREN).ru_maxrss)
sys.exit(rc)' "$out" "$BIN" --root "$C" --run "$C/run" --status "$C/run/sync.status" "$@")
}
BOUND=65536

# Choose games' list: every game of the six systems but the edge cases manifest() skips (1 missing, 6 out of the
# folder, 7 several files on a cartridge system, 9 another platform's), psx's game 7 as a disc set
: > "$T/requests.log"
bs "$T/games.out" games || fail "games failed: $(head -n 3 "$T/games.out")"
GAMES=$KB
[ "$(wc -l < "$T/games.out")" = 2377 ] || err "games listed $(wc -l < "$T/games.out") games, not 2377"
grep -q '^gba	gba/1942 (USA).gba	1942	' "$T/games.out" || err "games: no 1942 (a numeric name) on gba"
grep -q '^psx	psx/.discs/Game 0007 (USA)/Game 0007 (USA).cue	Game 0007	' "$T/games.out" || err "games: psx's game 7 not a disc set"
grep -q '	Tab Game 0005	' "$T/games.out" || err "games: game 5's tab not a space"
! grep -q 'Game 0001	\|Escape\|Game 0009	' "$T/games.out" || err "games: a game manifest() skips is listed"
[ "$GAMES" -lt $BOUND ] || err "games peaked at $((GAMES / 1024)) MB, over the 64 MB bound"
grep '^GET /api/roms?' "$T/requests.log" > "$T/roms.log" || true
[ "$(wc -l < "$T/roms.log")" = 48 ] || err "$(wc -l < "$T/roms.log") /api/roms requests, not 48 (8 for each of six platforms)"
! grep -q 'platform_ids=7&' "$T/roms.log" || err "3ds, which Banana has no system for, was asked for"
while read -r line; do
    case "$line" in *'&limit=50&'*) ;; *) err "an /api/roms request without limit=50: $line"; break;; esac
done < "$T/roms.log"
while read -r line; do
    case "$line" in *'&with_provider_metadata=false'*) ;; *) err "an /api/roms request without with_provider_metadata=false: $line"; break;; esac
done < "$T/roms.log"
[ -z "$failed" ] || exit 1

# Sync now, as a dry run: a "+" line for each file to download, the disc set's .m3u among them, none for a game skipped
bs "$T/dry.out" sync --dry-run || fail "the dry run failed: $(tail -n 3 "$T/dry.out")"
DRY=$KB
TODO=$(sed -n 's/^roms: \([0-9]*\) to download.*/\1/p' "$T/dry.out")
[ -n "$TODO" ] && [ "$(grep -c '^  + ' "$T/dry.out")" = "$TODO" ] ||
    err "the dry run: $(grep -c '^  + ' "$T/dry.out") files listed, $TODO to download"
grep -qx '  + psx/Game 0007 (USA).m3u' "$T/dry.out" || err "the dry run: no psx/Game 0007 (USA).m3u"
grep -qx '  + gba/1942 (USA).gba' "$T/dry.out" || err "the dry run: no gba/1942 (USA).gba"
! grep -q '^  + .*\(Escape (USA)\|Game 0001 (\)' "$T/dry.out" || err "the dry run: a skipped game's file to download"
[ "$DRY" -lt $BOUND ] || err "the dry run peaked at $((DRY / 1024)) MB, over the 64 MB bound"
# (its report on stdout only, as a dry run writes no sync.log: the peak memory right after the done: line)
grep -A1 '^done: ' "$T/dry.out" | tail -n 1 | grep -qx 'memory: peak [0-9]* kB' ||
    err "the dry run: no memory: peak line after done: ($(grep -A1 '^done: ' "$T/dry.out" | tail -n 1))"

# a saves-only sync (leaving a game): the saves matched to their games through the list, downloaded under their names
bs "$T/saves.out" sync --saves-only || fail "the saves-only sync failed: $(tail -n 3 "$T/saves.out")"
SAVES=$KB
[ "$(cat "$C/saves/Game 0010 (USA).srm" 2>/dev/null)" = "gba save 10" ] || err "the saves-only sync: no saves/Game 0010 (USA).srm"
[ "$(cat "$C/saves/nds/Game 0010 (USA).sav" 2>/dev/null)" = "nds save 10" ] || err "the saves-only sync: no saves/nds/Game 0010 (USA).sav"
[ "$SAVES" -lt $BOUND ] || err "the saves-only sync peaked at $((SAVES / 1024)) MB, over the 64 MB bound"
# its report on the card ends with done: and then the peak memory, under the bound
LOG=$C/banana/logs/sync.log
tail -n 2 "$LOG" 2>/dev/null | head -n 1 | grep -q '^done: ' || err "sync.log: the line before the last isn't done: ($(tail -n 2 "$LOG" 2>/dev/null | head -n 1))"
LOGKB=$(tail -n 1 "$LOG" 2>/dev/null | sed -n 's/^memory: peak \([0-9]*\) kB$/\1/p')
[ -n "$LOGKB" ] || err "sync.log: the last line isn't memory: peak <n> kB ($(tail -n 1 "$LOG" 2>/dev/null))"
[ -z "$LOGKB" ] || [ "$LOGKB" -lt $BOUND ] || err "sync.log: the saves-only sync's own peak $LOGKB kB, over the 64 MB bound"
[ -z "$failed" ] || exit 1

if [ -n "$OTHER" ]; then
    # the other build on the same library: the same games and the same files to download, at 4 times the bound or more
    BIN=$OTHER
    bs "$T/other-games.out" games || fail "the other banana-sync's games failed: $(head -n 3 "$T/other-games.out")"
    OGAMES=$KB
    cmp -s "$T/games.out" "$T/other-games.out" || { diff "$T/other-games.out" "$T/games.out" | head -n 10 >&2; err "games: not the other build's list"; }
    [ "$OGAMES" -ge $((BOUND * 4)) ] || err "the other banana-sync peaked at $((OGAMES / 1024)) MB: the library doesn't reproduce upstream's problem"
    bs "$T/other-dry.out" sync --dry-run || fail "the other banana-sync's dry run failed: $(tail -n 3 "$T/other-dry.out")"
    files() { grep '^  [+-] ' "$1"; sed -n 's/^\(roms: .*\), [0-9.]* GB free/\1/p' "$1"; }
    files "$T/dry.out" > "$T/dry.files"; files "$T/other-dry.out" > "$T/other-dry.files"
    cmp -s "$T/dry.files" "$T/other-dry.files" || { diff "$T/other-dry.files" "$T/dry.files" | head -n 10 >&2; err "the dry run: not the other build's files"; }
    [ "$(grep -c '^roms: ' "$T/dry.files")" = 1 ] || err "the dry run: no roms: line to compare"
    # a real sync and a saves-only sync from each build, each onto its own card, of a smaller library
    python3 kiwami/tests/romm-big.py "$T/port2" "$T/requests2.log" 30 40 & SRV2=$!
    for i in $(seq 100); do [ -s "$T/port2" ] && break; sleep 0.1; done
    [ -s "$T/port2" ] || fail "the second stand-in RomM didn't start"
    for b in this other; do
        C=$T/card-$b BIN=$T/banana-sync
        [ $b = this ] || BIN=$OTHER
        card "$C" "$T/port2"
        bs "$T/$b-sync.out" sync && echo 0 > "$T/$b-sync.rc" || echo $? > "$T/$b-sync.rc"
        bs "$T/$b-saves.out" sync --saves-only && echo 0 > "$T/$b-saves.rc" || echo $? > "$T/$b-saves.rc"
        python3 -c 'import json, sys; print(json.dumps(json.load(open(sys.argv[1])).get("state"), indent=1, sort_keys=True))' \
            "$C/banana/romsync.json" > "$T/$b-state.json"
    done
    for r in sync saves; do
        cmp -s "$T/this-$r.rc" "$T/other-$r.rc" || err "the $r runs ended differently: $(cat "$T/this-$r.rc"), the other's $(cat "$T/other-$r.rc")"
    done
    [ "$(find "$T/card-this/roms" -type f | wc -l)" -gt 100 ] || err "the real sync: only $(find "$T/card-this/roms" -type f | wc -l) files on the card"
    for d in roms saves; do
        diff -r "$T/card-this/$d" "$T/card-other/$d" > "$T/$d.diff" || { head -n 10 "$T/$d.diff" >&2; err "the real sync: the cards' $d/ differ"; }
    done
    cmp -s "$T/this-state.json" "$T/other-state.json" || { diff "$T/other-state.json" "$T/this-state.json" | head -n 10 >&2; err "the real sync: romsync.json's state differs"; }
    [ -z "$failed" ] || exit 1
    echo "romm-list: the other banana-sync peaked at $((OGAMES / 1024)) MB listing the games; its games, dry run, cards, state and saves the same ($(find "$T/card-this/roms" -type f | wc -l) files, sync $(cat "$T/this-sync.rc"), saves $(cat "$T/this-saves.rc"))"
fi

echo "romm-list: games $((GAMES / 1024)) MB, dry run $((DRY / 1024)) MB, saves $((SAVES / 1024)) MB peak; 48 requests of 50; $KEYS keys of manifest() kept; sync.log's peak $LOGKB kB"
