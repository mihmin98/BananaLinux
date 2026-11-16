#!/bin/sh
# banana-sync's RomM listing on a large library: RomM's games asked for 200 at a time, without provider metadata,
# each kept to the fields Romm reads of a listed game (ui/src/kiwami/syncmem.h), so Get games' list (banana-sync
# games), Sync now bringing the games on Get games' list (sync --games-only) and one game's sync before it starts stay
# under 100 MB where upstream's held every page's whole tree. Against romm-big.py's 400 games on each of six systems
# (and 3ds, which Banana has no system for), about 25 KB of JSON a game with RomM's provider blocks: the peak of each
# run (the child's ru_maxrss), the requests made, the games listed with RomM's edge cases, the games downloaded, the
# progress while the list is read (Listing games), the save brought before a game (the game alone, by the RomM id the
# listings before it remembered: ui/src/kiwami/romm/lookup.h), and the peak memory each sync's report ends with. Also slim()'s unit tests (syncmem-units.cpp), and the guard: every key Romm reads of a listed game
# (sync_romm.cpp's manifest(), romFiles(), romTitle(), romRegions()) is kept by syncmem.h, so a field upstream starts
# reading isn't dropped. banana-sync is built from ui/Makefile's own rule, its sources as upstream lists them. Needs
# g++ and python3. Run by kiwami/tests/ci.sh, or by hand:
#   kiwami/tests/romm-list.sh
#   kiwami/tests/romm-list.sh --compare <other banana-sync, or a commit to build one from>
# --compare (local, the final check against the Home server rework's own build: --compare cab127ba) also runs the
# other build on the same library: its games list has to be the same as this one's, and its peak at least 256 MB (the
# library reproduces upstream's problem). Then each build brings the same games, and then one game's save, each onto
# a card of its own: the two cards' roms/ and saves/, Get games' cached list and the wait's status have to be the same.
set -e
OTHER= OTHERREV=
if [ "$1" = --compare ]; then
    if [ -f "$2" ] && [ -x "$2" ]; then OTHER=$(cd "$(dirname "$2")" && pwd)/$(basename "$2")
    elif [ -n "$2" ] && git -C "$(dirname "$0")" rev-parse -q --verify "$2^{commit}" > /dev/null; then OTHERREV=$2
    else echo "romm-list: --compare needs another banana-sync or a commit, not \"$2\"" >&2; exit 2
    fi
fi
cd "$(dirname "$0")/../.."
T=$(mktemp -d)
SRV=
trap '[ -z "$SRV" ] || kill $SRV 2>/dev/null; rm -rf "$T"' EXIT
fail() { echo "romm-list: $*" >&2; exit 1; }
failed=
err() { echo "romm-list: $*" >&2; failed=1; }

# build <ui folder> <out>: banana-sync from that ui/ folder, by its Makefile's rule (the sources on its recipe line)
build() {
    srcs=$(awk '/^banana-sync: src\/sync\.cpp/ { getline; print; exit }' "$1/Makefile" | grep -o 'src/[A-Za-z0-9_/]*\.cpp' | sed "s|^|$1/|")
    [ -n "$srcs" ] || fail "no banana-sync rule in $1/Makefile"
    # shellcheck disable=SC2086
    ${CXX:-g++} -std=c++17 -O2 -Wall -Wno-unused-function -o "$2" $srcs -ldl
}
build ui "$T/banana-sync"
if [ -n "$OTHERREV" ]; then
    mkdir -p "$T/other"
    git archive "$OTHERREV" ui/Makefile ui/src | tar -x -C "$T/other"
    build "$T/other/ui" "$T/other-banana-sync"
    OTHER=$T/other-banana-sync
fi
${CXX:-g++} -std=c++17 -Wall -Werror -I ui/src -o "$T/syncmem-units" kiwami/tests/syncmem-units.cpp ui/src/json.cpp
"$T/syncmem-units"

# the guard: the keys Romm reads of a listed game (str/num/flag/get in romFiles(), romTitle(), romRegions() and
# manifest()), each kept by syncmem.h; first on a copy of sync_romm.cpp that reads one more, which it has to catch
readers() {
    awk '/std::vector<RomFile> romFiles\(/ { r = 1 } r && /bool packedName\(/ { r = 0 }
        /bool manifest\(/ { r = 1 } r && /struct Canon / { exit } r' "$1"
}
guard() {
    keys=$(readers "$1" | grep -o '\(str\|num\|flag\|get\)("[a-z_0-9]*"' | sed 's/.*("//; s/"$//' | sort -u)
    [ -n "$keys" ] || { echo "Romm's readers not found in $1"; return 1; }
    for k in $keys; do
        grep -q "\"$k\"" ui/src/kiwami/syncmem.h || { echo "Romm reads \"$k\", which syncmem.h drops"; return 1; }
    done
    echo "$keys" | wc -l
}
sed '/std::string cover = .*it->str("path_cover_large");/a std::string ext = it->str("fs_extension");' ui/src/sync_romm.cpp > "$T/romm-more.cpp"
grep -q 'fs_extension' "$T/romm-more.cpp" || fail "the guard's self-test: no line to add a key after in ui/src/sync_romm.cpp"
[ "$(guard "$T/romm-more.cpp")" = 'Romm reads "fs_extension", which syncmem.h drops' ] ||
    fail "the guard missed a key Romm reads and syncmem.h drops"
KEYS=$(guard ui/src/sync_romm.cpp) || fail "$KEYS"
# (and it reads all of them: the readers it looks in are where they were)
for k in fs_name_no_ext revision tags regions archive_members crc_hash path_cover_large genres; do
    readers ui/src/sync_romm.cpp | grep -q "\"$k\"" || fail "the guard doesn't see Romm read \"$k\": romFiles(), romTitle(), romRegions() or manifest() moved"
done

# the stand-in, and a card paired with it as a RomM device
python3 kiwami/tests/romm-big.py "$T/port" "$T/requests.log" 400 40 & SRV=$!
for i in $(seq 100); do [ -s "$T/port" ] && break; sleep 0.1; done
[ -s "$T/port" ] || fail "the stand-in RomM didn't start"
# card <dir>: a card paired with the stand-in, Auto sync and Sync before playing on, GBA's game 10 on it
card() {
    mkdir -p "$1/banana" "$1/run" "$1/roms/gba"
    printf '{"backend": "romm", "server": "http://127.0.0.1:%s", "token": "rmm_test", "name": "test", "device": "1"}\n' "$(cat "$T/port")" > "$1/banana/romsync.json"
    printf 'homesync_auto = quit\nhomesync_launch = on\n' > "$1/banana/settings.conf"
    printf 'rom 200010\n' > "$1/roms/gba/Game 0010 (USA).gba"
}
C=$T/card BIN=$T/banana-sync
card "$C"
# bs <out> <args...>: banana-sync ($BIN) on the card ($C), its stdout in <out>; its peak resident size (kB) in $KB
bs() {
    out=$1; shift
    KB=$(python3 -c 'import resource, subprocess, sys
with open(sys.argv[1], "w") as o:
    rc = subprocess.call(sys.argv[2:], stdout=o)
print(resource.getrusage(resource.RUSAGE_CHILDREN).ru_maxrss)
sys.exit(rc)' "$out" "$BIN" --root "$C" --run "$C/run" --status "$C/run/sync.status" "$@")
}
BOUND=102400 MB=100	# (kB, and in MB for the messages)
# requests <what> <n> [platform id]: the /api/roms requests since the log was emptied, n of them, each of 200 and
# without provider metadata, none for 3ds (and with an id, only for that platform)
requests() {
    grep '^GET /api/roms?' "$T/requests.log" > "$T/roms.log" || true
    [ "$(wc -l < "$T/roms.log")" = "$2" ] || err "$1: $(wc -l < "$T/roms.log") /api/roms requests, not $2"
    ! grep -q 'platform_ids=7&' "$T/roms.log" || err "$1: 3ds, which Banana has no system for, was asked for"
    while read -r line; do
        case "$line" in *'&limit=200&'*'&with_provider_metadata=false'*) ;; *) err "$1: a request not of 200 without provider metadata: $line"; break;; esac
        [ -z "$3" ] || case "$line" in *"platform_ids=$3&"*) ;; *) err "$1: a request for another platform: $line"; break;; esac
    done < "$T/roms.log"
}

# Get games' list: every game of the six systems but the edge cases manifest() skips (1 missing, 6 out of the
# folder, 7 several files on a cartridge system, 9 another platform's), psx's game 7 as a disc set
: > "$T/requests.log"
bs "$T/games.out" games || fail "games failed: $(head -n 3 "$T/games.out")"
GAMES=$KB
[ "$(wc -l < "$T/games.out")" = 2377 ] || err "games listed $(wc -l < "$T/games.out") games, not 2377"
grep -q '^gba	gba/1942 (USA).gba	1942	' "$T/games.out" || err "games: no 1942 (a numeric name) on gba"
grep -q '^psx	psx/.discs/Game 0007 (USA)/Game 0007 (USA).cue	Game 0007	' "$T/games.out" || err "games: psx's game 7 not a disc set"
grep -q '	Tab Game 0005	' "$T/games.out" || err "games: game 5's tab not a space"
! grep -q 'Game 0001	\|Escape\|Game 0009	' "$T/games.out" || err "games: a game manifest() skips is listed"
[ "$GAMES" -lt $BOUND ] || err "games peaked at $((GAMES / 1024)) MB, over the $MB MB bound"
requests games 12
# (the RomM id of the game on the card remembered, for the sync before it)
grep -q '"gba/Game 0010 (USA).gba": 200010' "$C/banana/kiwami-romm.json" 2>/dev/null || err "games: game 10's RomM id not remembered: $(cat "$C/banana/kiwami-romm.json" 2>/dev/null)"
# (no status written: Get games' refresh must not show in Sync now's row), and Get games' list kept on the card
[ ! -e "$C/run/sync.status" ] || err "games wrote a status: $(cat "$C/run/sync.status")"
[ "$(wc -l < "$C/banana/homesync-server-games.tsv" 2>/dev/null || echo 0)" -ge 2377 ] || err "games: no Get games' list on the card"
[ -z "$failed" ] || exit 1

# Sync now's games: 20 games on Get games' list (the disc set and 1942 among them) brought by Get games' own run
{ grep -o '	psx/.discs/Game 0007 (USA)/Game 0007 (USA).cue	\|	gba/1942 (USA).gba	' "$T/games.out" | tr -d '\t'
  awk -F '\t' 'NR % 130 == 0 { print $2 }' "$T/games.out"; } > "$T/keys"
[ "$(sort -u "$T/keys" | wc -l)" = 20 ] || fail "not 20 games to ask for: $(wc -l < "$T/keys")"
cp "$T/keys" "$C/banana/homesync-get.txt"
: > "$T/requests.log"
bs "$T/get.out" -v sync --games-only 2> "$T/get.err" || fail "the games-only sync failed: $(tail -n 3 "$T/get.out")"
GET=$KB
while read -r k; do [ -s "$C/roms/$k" ] || err "the games-only sync: no roms/$k"; done < "$T/keys"
[ -s "$C/roms/psx/Game 0007 (USA).m3u" ] || err "the games-only sync: no psx/Game 0007 (USA).m3u"
! grep -qv '^#' "$C/banana/homesync-get.txt" || err "the games-only sync: games left on Get games' list: $(grep -v '^#' "$C/banana/homesync-get.txt" | head -n 3)"
[ "$GET" -lt $BOUND ] || err "the games-only sync peaked at $((GET / 1024)) MB, over the $MB MB bound"
requests "the games-only sync" 12
# its progress while the list is read (-v: each status on stderr), a status a page: games read of the six systems'
# rom_count, the system and how far into it
grep '^status: running [0-9]* [0-9]* Listing games|' "$T/get.err" > "$T/listing" || true
[ "$(wc -l < "$T/listing")" = 12 ] || err "the games-only sync: $(wc -l < "$T/listing") Listing games statuses, not 12"
[ "$(head -n 1 "$T/listing")" = 'status: running 200 2400 Listing games|nds 200 / 400' ] || err "the games-only sync's first listing status: $(head -n 1 "$T/listing")"
[ "$(tail -n 1 "$T/listing")" = 'status: running 2400 2400 Listing games|psx 400 / 400' ] || err "the games-only sync's last listing status: $(tail -n 1 "$T/listing")"
# its report on the card ends with done: and then the peak memory, under the bound
LOG=$C/banana/logs/sync.log
tail -n 2 "$LOG" 2>/dev/null | head -n 1 | grep -q '^done: ' || err "sync.log: the line before the last isn't done: ($(tail -n 2 "$LOG" 2>/dev/null | head -n 1))"
LOGKB=$(tail -n 1 "$LOG" 2>/dev/null | sed -n 's/^memory: peak \([0-9]*\) kB$/\1/p')
[ -n "$LOGKB" ] || err "sync.log: the last line isn't memory: peak <n> kB ($(tail -n 1 "$LOG" 2>/dev/null))"
[ -z "$LOGKB" ] || [ "$LOGKB" -lt $BOUND ] || err "sync.log: the games-only sync's own peak $LOGKB kB, over the $MB MB bound"
[ -z "$failed" ] || exit 1

# one game's sync before it starts: the game alone, by its remembered id, no listing, and its save brought from the slot
: > "$T/requests.log"
bs "$T/one.out" sync --rom "gba/Game 0010 (USA).gba" --auto launch || fail "the sync before a game failed: $(tail -n 3 "$T/one.out")"
ONE=$KB
S=$C/saves/gba/Game\ 0010\ \(USA\).srm
[ "$(wc -c < "$S" 2>/dev/null)" = 131072 ] && [ "$(head -c 11 "$S")" = "gba save 10" ] || err "the sync before a game: no saves/gba/Game 0010 (USA).srm"
case "$(cat "$C/run/launch.status" 2>/dev/null)" in "done 0 0 0 1 0 0|"*) ;; *) err "the sync before a game: launch.status $(cat "$C/run/launch.status" 2>/dev/null)";; esac
[ "$ONE" -lt $BOUND ] || err "the sync before a game peaked at $((ONE / 1024)) MB, over the $MB MB bound"
requests "the sync before a game" 0
[ "$(grep -c '^GET /api/roms/200010?$' "$T/requests.log")" = 1 ] || err "the sync before a game: not one GET /api/roms/200010: $(grep '^GET /api/roms' "$T/requests.log" | head -n 3)"
[ ! -e "$C/run/sync.times" ] || err "the sync before a game wrote sync.times"
[ -z "$failed" ] || exit 1

if [ -n "$OTHER" ]; then
    # the other build on the same library, on a fresh card: the same games, at 256 MB or more
    C=$T/card-games BIN=$OTHER
    card "$C"
    bs "$T/other-games.out" games || fail "the other banana-sync's games failed: $(head -n 3 "$T/other-games.out")"
    OGAMES=$KB
    cmp -s "$T/games.out" "$T/other-games.out" || { diff "$T/other-games.out" "$T/games.out" | head -n 10 >&2; err "games: not the other build's list"; }
    [ "$OGAMES" -ge 262144 ] || err "the other banana-sync peaked at $((OGAMES / 1024)) MB: the library doesn't reproduce upstream's problem"
    # the same games, then the same save, from each build onto its own card
    for b in this other; do
        C=$T/card-$b BIN=$T/banana-sync
        [ $b = this ] || BIN=$OTHER
        card "$C"
        cp "$T/keys" "$C/banana/homesync-get.txt"
        bs "$T/$b-get.out" sync --games-only && echo 0 > "$T/$b-get.rc" || echo $? > "$T/$b-get.rc"
        bs "$T/$b-one.out" sync --rom "gba/Game 0010 (USA).gba" --auto launch && echo 0 > "$T/$b-one.rc" || echo $? > "$T/$b-one.rc"
        cp "$C/run/launch.status" "$T/$b-launch.status" 2>/dev/null || : > "$T/$b-launch.status"
    done
    for r in get one; do
        cmp -s "$T/this-$r.rc" "$T/other-$r.rc" || err "the $r runs ended differently: $(cat "$T/this-$r.rc"), the other's $(cat "$T/other-$r.rc")"
    done
    for d in roms saves; do
        diff -r "$T/card-this/$d" "$T/card-other/$d" > "$T/$d.diff" || { head -n 10 "$T/$d.diff" >&2; err "the cards' $d/ differ"; }
    done
    # (Get games' lists from their second line: the first names the server and the time it was made)
    for b in this other; do tail -n +2 "$T/card-$b/banana/homesync-server-games.tsv" > "$T/$b-games.tsv" 2>/dev/null || :; done
    [ -s "$T/this-games.tsv" ] && cmp -s "$T/this-games.tsv" "$T/other-games.tsv" || { diff "$T/other-games.tsv" "$T/this-games.tsv" | head -n 6 >&2; err "Get games' lists on the cards differ"; }
    cmp -s "$T/this-launch.status" "$T/other-launch.status" || err "the waits' statuses differ: $(cat "$T/this-launch.status"), the other's $(cat "$T/other-launch.status")"
    [ -z "$failed" ] || exit 1
    echo "romm-list: the other banana-sync peaked at $((OGAMES / 1024)) MB listing the games; its games, cards and saves the same ($(find "$T/card-this/roms" -type f | wc -l) files, games-only $(cat "$T/this-get.rc"), before a game $(cat "$T/this-one.rc"))"
fi

echo "romm-list: games $((GAMES / 1024)) MB, games-only sync $((GET / 1024)) MB, before a game $((ONE / 1024)) MB peak (by its id); 12 requests of 200, 12 listing statuses; $KEYS keys Romm reads kept; sync.log's peak $LOGKB kB"
