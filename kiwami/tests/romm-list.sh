#!/bin/sh
# banana-sync's RomM listing on a large library: RomM's games asked for 50 at a time, without provider metadata, each
# kept to the fields Romm::manifest() reads (ui/src/kiwami/syncmem.h), so Choose games (banana-sync games), Sync now
# and a saves-only sync stay under 64 MB where upstream's held every page's whole tree. Against romm-big.py's 400
# games on each of six systems (and 3ds, which Banana has no system for), about 25 KB of JSON a game with RomM's
# provider blocks: the peak of each run (the child's ru_maxrss), the requests made, the games listed with RomM's edge
# cases, the dry run's files, and the saves downloaded by name. Also slim()'s unit tests (syncmem-units.cpp), and the
# guard: every key Romm::manifest() reads is kept by syncmem.h, so a field upstream starts reading isn't dropped.
# Needs g++ and python3. Run by kiwami/tests/ci.sh, or by hand:
#   kiwami/tests/romm-list.sh
set -e
cd "$(dirname "$0")/../.."
T=$(mktemp -d)
SRV=
trap '[ -z "$SRV" ] || kill $SRV 2>/dev/null; rm -rf "$T"' EXIT
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
C=$T/card
mkdir -p "$C/banana" "$C/run" "$C/roms"
printf '{"backend": "romm", "server": "http://127.0.0.1:%s", "token": "rmm_test", "name": "test"}\n' "$(cat "$T/port")" > "$C/banana/romsync.json"
# bs <out> <args...>: banana-sync on the card, its stdout in <out>; its peak resident size (kB) in $KB
bs() {
    out=$1; shift
    KB=$(python3 -c 'import resource, subprocess, sys
with open(sys.argv[1], "w") as o:
    rc = subprocess.call(sys.argv[2:], stdout=o)
print(resource.getrusage(resource.RUSAGE_CHILDREN).ru_maxrss)
sys.exit(rc)' "$out" "$T/banana-sync" --root "$C" --run "$C/run" --status "$C/run/sync.status" "$@")
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

# a saves-only sync (leaving a game): the saves matched to their games through the list, downloaded under their names
bs "$T/saves.out" sync --saves-only || fail "the saves-only sync failed: $(tail -n 3 "$T/saves.out")"
SAVES=$KB
[ "$(cat "$C/saves/Game 0010 (USA).srm" 2>/dev/null)" = "gba save 10" ] || err "the saves-only sync: no saves/Game 0010 (USA).srm"
[ "$(cat "$C/saves/nds/Game 0010 (USA).sav" 2>/dev/null)" = "nds save 10" ] || err "the saves-only sync: no saves/nds/Game 0010 (USA).sav"
[ "$SAVES" -lt $BOUND ] || err "the saves-only sync peaked at $((SAVES / 1024)) MB, over the 64 MB bound"
[ -z "$failed" ] || exit 1

echo "romm-list: games $((GAMES / 1024)) MB, dry run $((DRY / 1024)) MB, saves $((SAVES / 1024)) MB peak; 48 requests of 50; $KEYS keys of manifest() kept"
