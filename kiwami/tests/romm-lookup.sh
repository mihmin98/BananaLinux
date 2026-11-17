#!/bin/sh
# banana-sync's sync before a game (sync --rom <path> --auto launch) without listing the game's system
# (ui/src/kiwami/romm/lookup.h), against upstream's stand-in RomM 5.4.1 (ui/tests/fake-romm.py) with the fork's in
# front of it for a game alone and RomM's name search (kiwami/tests/romm-kiwami.py): a game never synced is found by
# one search, its id remembered; then by that id alone, with no listing; a listing of every system (Get games')
# remembers the ids of the games on the card; a search that finds near names keeps the exact one; an id that names
# another game now is looked for by name again; a game renamed on the server is looked for by its system's listing,
# with the outcome of the Home server rework's own build (cab127ba, built when the repository has it) on the same
# card. Each save is brought from the game's autosave slot. Needs g++, python3 and curl. Run by kiwami/tests/ci.sh, or
# by hand:
#   kiwami/tests/romm-lookup.sh
set -e
cd "$(dirname "$0")/../.."
T=$(mktemp -d)
PIDS=
trap 'for p in $PIDS; do kill $p 2>/dev/null; done; [ -n "$KEEP" ] || rm -rf "$T"' EXIT
fail() { echo "romm-lookup: $*" >&2; exit 1; }
failed=
err() { echo "romm-lookup: $*" >&2; failed=1; }

# banana-sync by ui/Makefile's own rule (its sources on the recipe line): this tree's, warnings as errors, and the
# rework's own from its commit, when the repository has it
build() {
    srcs=$(awk '/^banana-sync: src\/sync\.cpp/ { getline; print; exit }' "$1/Makefile" | grep -o 'src/[A-Za-z0-9_/]*\.cpp' | sed "s|^|$1/|")
    [ -n "$srcs" ] || fail "no banana-sync rule in $1/Makefile"
    # shellcheck disable=SC2086
    ${CXX:-g++} -std=c++17 $3 -Wall -Wno-unused-function -o "$2" $srcs -ldl
}
build ui "$T/banana-sync" "-O2 -Werror"
BIN=$T/banana-sync REWORK=
if git rev-parse -q --verify 'cab127ba^{commit}' > /dev/null; then
    mkdir -p "$T/rework"
    git archive cab127ba ui/Makefile ui/src | tar -x -C "$T/rework"
    build "$T/rework/ui" "$T/rework-banana-sync" -O0
    REWORK=$T/rework-banana-sync
else
    echo "romm-lookup: the rework's own build skipped (cab127ba isn't in this repository)"
fi
export BANANA_SYSTEMS="$PWD/system/rootfs/etc/banana/systems.conf"

# the library: 50 GBA games, a game with a near name beside it, and one to rename; the card holds a copy of each
L=$T/lib C=$T/card
mkdir -p "$L/gba" "$T/firmware" "$C/banana" "$C/run" "$C/roms/gba"
for i in $(seq -w 1 50); do printf 'gba rom %s' "$i" > "$L/gba/Game $i (USA).gba"; done
printf 'gba rom puzzle' > "$L/gba/Puzzle (USA).gba"
printf 'gba rom puzzle rev 1' > "$L/gba/Puzzle (USA) (Rev 1).gba"
printf 'gba rom renamed' > "$L/gba/Old Name (USA).gba"
cp "$L/gba/"* "$C/roms/gba/"
rm "$C/roms/gba/Puzzle (USA) (Rev 1).gba"
port() { python3 -c 'import socket; s = socket.socket(); s.bind(("127.0.0.1", 0)); print(s.getsockname()[1])'; }
UP=$(port)
python3 ui/tests/fake-romm.py "$UP" "$L" 2> "$T/fake-romm.log" & PIDS="$PIDS $!"
python3 kiwami/tests/romm-kiwami.py "$T/port" "$UP" "$T/firmware" "$T/requests.log" 2> "$T/romm-kiwami.log" & PIDS="$PIDS $!"
for i in $(seq 50); do [ -s "$T/port" ] && curl -s "http://127.0.0.1:$UP/api/heartbeat" > /dev/null && break; sleep 0.1; done
[ -s "$T/port" ] || fail "the stand-ins didn't start: $(tail -n 3 "$T/romm-kiwami.log" "$T/fake-romm.log")"
U=http://127.0.0.1:$(cat "$T/port")
jget() { python3 -c "import json, sys; print(json.load(sys.stdin)$1)"; }
TOK=$(curl -s -XPOST "$U/test/token" -d '{}' | jget '["token"]')
printf '{"backend": "romm", "server": "%s", "token": "%s", "name": "lookup", "serverVersion": "5.4.1"}\n' "$U" "$TOK" > "$C/banana/romsync.json"
printf 'homesync_auto = quit\nhomesync_launch = on\n' > "$C/banana/settings.conf"

# (the test's own requests go to upstream's stand-in, past the fork's, whose log holds banana-sync's alone)
srv() { curl -s -H "Authorization: Bearer $TOK" "http://127.0.0.1:$UP$1"; }
# rid <file name>: the server's id of that game
rid() { srv "/api/roms?platform_ids=1&limit=500" | python3 -c 'import json, sys; print(next(r["id"] for r in json.load(sys.stdin)["items"] if r["fs_name"] == sys.argv[1]))' "$1"; }
# seed <file name> <text>: a version of its save in its autosave slot, from another device (the text again and again
# to 128 KiB, the size the rework takes of a GBA save)
seed() {
    python3 -c 'import json, sys; json.dump({"rom_id": int(sys.argv[1]), "slot": "autosave", "file_name": sys.argv[2],
        "data": (sys.argv[3].encode() * 131072)[:131072].hex()}, open(sys.argv[4], "w"))' "$(rid "$1")" "${1%.*}.srm" "$2" "$T/seed.json"
    curl -s -XPOST "$U/test/save" --data-binary @"$T/seed.json" > /dev/null
}
# launch <card> <file name> [banana-sync]: the sync before that game starts, its output in $T/launch.out
launch() {
    : > "$T/requests.log"
    "${3:-$BIN}" --root "$1" --run "$1/run" --status "$1/run/sync.status" sync --rom "gba/$2" --auto launch > "$T/launch.out" 2>&1 ||
        err "the sync before $2 failed: $(tail -n 3 "$T/launch.out")"
}
# listings: the requests for a listing of a system's games (no search); searches: RomM's name searches
listings() { grep '^GET /api/roms?' "$T/requests.log" | grep -vc 'search_term=' || true; }
searches() { grep -c '^GET /api/roms?.*&search_term=' "$T/requests.log" || true; }
byid() { grep -c "^GET /api/roms/$1?\$" "$T/requests.log" || true; }
remembered() { python3 -c 'import json, sys; print(json.load(open(sys.argv[1]))["ids"].get(sys.argv[2], ""))' "$C/banana/kiwami-romm.json" "gba/$1" 2>/dev/null; }
# brought <file name> <text>: the card's save is the one seeded with that text
brought() { python3 -c 'import sys; sys.exit(0 if open(sys.argv[1], "rb").read() == (sys.argv[2].encode() * 131072)[:131072] else 1)' "$C/saves/gba/${1%.*}.srm" "$2" 2> /dev/null; }

# A10: a game never synced, no id remembered: one search, no listing; the save brought; its id remembered
seed "Game 07 (USA).gba" "slot 07 a"
launch "$C" "Game 07 (USA).gba"
[ "$(searches)" = 1 ] && [ "$(listings)" = 0 ] || err "a game never synced: $(searches) searches, $(listings) listings: $(grep '^GET /api/roms' "$T/requests.log")"
grep -q '^GET /api/roms?.*&search_term=Game%2007%20%28USA%29&limit=50&' "$T/requests.log" || err "a game never synced: not a search for its name: $(grep search_term "$T/requests.log")"
brought "Game 07 (USA).gba" "slot 07 a" || err "a game never synced: its save not brought: $(cat "$T/launch.out")"
grep -q '^done ' "$C/run/launch.status" || err "a game never synced: launch.status $(cat "$C/run/launch.status")"
[ "$(remembered "Game 07 (USA).gba")" = "$(rid "Game 07 (USA).gba")" ] || err "a game never synced: its id not remembered: $(cat "$C/banana/kiwami-romm.json" 2>/dev/null)"
grep -q 'found by its name' "$T/launch.out" || err "a game never synced: not said found by its name: $(cat "$T/launch.out")"

# A9: the same game again: its id alone, no listing, no search; the slot's newer version brought
seed "Game 07 (USA).gba" "slot 07 b"
launch "$C" "Game 07 (USA).gba"
[ "$(byid "$(rid "Game 07 (USA).gba")")" = 1 ] && [ "$(searches)" = 0 ] && [ "$(listings)" = 0 ] ||
    err "a game remembered: $(searches) searches, $(listings) listings: $(grep '^GET /api/roms' "$T/requests.log")"
brought "Game 07 (USA).gba" "slot 07 b" || err "a game remembered: the newer save not brought: $(cat "$T/launch.out")"
grep -q 'as remembered' "$T/launch.out" || err "a game remembered: not said: $(cat "$T/launch.out")"

# a listing of every system (Get games' refresh) remembers the ids of the games on the card, and not the one it hasn't
"$BIN" --root "$C" --run "$C/run" --status "$C/run/sync.status" games > "$T/games.out" 2>&1 || err "games failed: $(tail -n 3 "$T/games.out")"
[ "$(python3 -c 'import json, sys; print(len(json.load(open(sys.argv[1]))["ids"]))' "$C/banana/kiwami-romm.json")" = 52 ] ||
    err "games: not the 52 games on the card remembered: $(cat "$C/banana/kiwami-romm.json")"
[ "$(remembered "Game 10 (USA).gba")" = "$(rid "Game 10 (USA).gba")" ] && [ -z "$(remembered "Puzzle (USA) (Rev 1).gba")" ] ||
    err "games: the ids remembered: $(cat "$C/banana/kiwami-romm.json")"
seed "Game 10 (USA).gba" "slot 10"
launch "$C" "Game 10 (USA).gba"
[ "$(byid "$(rid "Game 10 (USA).gba")")" = 1 ] && [ "$(searches)" = 0 ] && [ "$(listings)" = 0 ] && brought "Game 10 (USA).gba" "slot 10" ||
    err "a game remembered by the listing: $(grep '^GET /api/roms' "$T/requests.log") $(cat "$T/launch.out")"

# A10: a search that finds near names (Puzzle (USA) and its Rev 1): the exact one's save, and its id
seed "Puzzle (USA).gba" "puzzle"
seed "Puzzle (USA) (Rev 1).gba" "puzzle rev 1"
python3 -c 'import json, sys; c = json.load(open(sys.argv[1])); c["ids"].pop(sys.argv[2]); json.dump(c, open(sys.argv[1], "w"))' "$C/banana/kiwami-romm.json" "gba/Puzzle (USA).gba"
launch "$C" "Puzzle (USA).gba"
[ "$(searches)" = 1 ] && [ "$(listings)" = 0 ] || err "near names: $(searches) searches, $(listings) listings"
brought "Puzzle (USA).gba" "puzzle" || err "near names: not the exact game's save: $(cat "$T/launch.out")"
[ "$(remembered "Puzzle (USA).gba")" = "$(rid "Puzzle (USA).gba")" ] || err "near names: not the exact game's id remembered: $(remembered "Puzzle (USA).gba")"

# A11: an id that names another game now: looked for by name, and the right id remembered
python3 -c 'import json, sys; c = json.load(open(sys.argv[1])); c["ids"][sys.argv[2]] = int(sys.argv[3]); json.dump(c, open(sys.argv[1], "w"))' \
    "$C/banana/kiwami-romm.json" "gba/Game 08 (USA).gba" "$(rid "Game 09 (USA).gba")"
seed "Game 08 (USA).gba" "slot 08"
launch "$C" "Game 08 (USA).gba"
[ "$(byid "$(rid "Game 09 (USA).gba")")" = 1 ] && [ "$(searches)" = 1 ] && [ "$(listings)" = 0 ] ||
    err "an id of another game: $(grep '^GET /api/roms' "$T/requests.log")"
grep -q 'is no longer gba/Game 08 (USA).gba (another file)' "$T/launch.out" || err "an id of another game: not said: $(cat "$T/launch.out")"
brought "Game 08 (USA).gba" "slot 08" && [ "$(remembered "Game 08 (USA).gba")" = "$(rid "Game 08 (USA).gba")" ] ||
    err "an id of another game: $(remembered "Game 08 (USA).gba") remembered, $(cat "$T/launch.out")"

# A11: a game renamed on the server (a new id in RomM, the one remembered gone): neither its id nor a search finds it
# under the card file's name, so its system is listed, and the outcome is the rework's: the game isn't the server's,
# the card's save left as it is, its id forgotten
seed "Old Name (USA).gba" "old name"
launch "$C" "Old Name (USA).gba"
brought "Old Name (USA).gba" "old name" || err "the game to rename: its save not brought first: $(cat "$T/launch.out")"
mv "$L/gba/Old Name (USA).gba" "$L/gba/New Name (USA).gba"
head -c 1024 /dev/urandom > "$C/saves/gba/Old Name (USA).srm"
cp "$C/saves/gba/Old Name (USA).srm" "$T/own.srm"
cp -a "$C" "$T/card-rework"
launch "$C" "Old Name (USA).gba"
[ "$(searches)" = 1 ] && [ "$(listings)" = 1 ] || err "renamed on the server: $(searches) searches, $(listings) listings: $(grep '^GET /api/roms' "$T/requests.log")"
grep -q 'not found by RomM.s search (no game of its name), its system listed' "$T/launch.out" || err "renamed on the server: not said: $(cat "$T/launch.out")"
cmp -s "$T/own.srm" "$C/saves/gba/Old Name (USA).srm" || err "renamed on the server: the card's save changed"
[ -z "$(remembered "Old Name (USA).gba")" ] || err "renamed on the server: its old id still remembered"
cp "$C/run/launch.status" "$T/this.status"
if [ -n "$REWORK" ]; then
    launch "$T/card-rework" "Old Name (USA).gba" "$REWORK"
    cmp -s "$T/this.status" "$T/card-rework/run/launch.status" || err "renamed on the server: launch.status $(cat "$T/this.status"), the rework's $(cat "$T/card-rework/run/launch.status")"
    diff -r "$C/saves" "$T/card-rework/saves" > /dev/null || err "renamed on the server: the saves differ from the rework's"
fi

[ -z "$failed" ] || exit 1
echo "romm-lookup: all checks passed (a search, an id, near names, an id of another game, a renamed game$([ -n "$REWORK" ] && echo ", as the rework's build"); $(python3 -c 'import json, sys; print(len(json.load(open(sys.argv[1]))["ids"]))' "$C/banana/kiwami-romm.json") ids remembered)"
