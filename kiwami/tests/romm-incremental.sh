#!/bin/sh
# banana-sync's RomM game list kept on the card and brought up to date (ui/src/kiwami/romm/listcache.h), against
# upstream's stand-in RomM 5.4.1 (ui/tests/fake-romm.py) with the fork's in front of it (kiwami/tests/romm-kiwami.py):
# the first Sync now lists every system and keeps the list; the next asks RomM only for its game ids and each
# system's changes since the last listing's time, less 60 s, and lists the same games without writing the list again;
# a game renamed in RomM, one deleted and one added show in Get games' list, in RomM's name order; a new system is
# listed in full; RomM's ids that can't be read, Get games' refresh (banana-sync games) and another server's list mean
# a full listing; Forget removes the list. Needs g++, python3 and curl. Run by kiwami/tests/ci.sh, or by hand:
#   kiwami/tests/romm-incremental.sh
set -e
cd "$(dirname "$0")/../.."
T=$(mktemp -d)
PIDS=
trap 'for p in $PIDS; do kill $p 2>/dev/null; done; [ -n "$KEEP" ] || rm -rf "$T"' EXIT
fail() { echo "romm-incremental: $*" >&2; exit 1; }
failed=
err() { echo "romm-incremental: $*" >&2; failed=1; }

# banana-sync by ui/Makefile's own rule (its sources on the recipe line), warnings as errors
srcs=$(awk '/^banana-sync: src\/sync\.cpp/ { getline; print; exit }' ui/Makefile | grep -o 'src/[A-Za-z0-9_/]*\.cpp' | sed 's|^|ui/|')
[ -n "$srcs" ] || fail "no banana-sync rule in ui/Makefile"
# shellcheck disable=SC2086
${CXX:-g++} -std=c++17 -O2 -Werror -Wall -Wno-unused-function -o "$T/banana-sync" $srcs -ldl
BIN=$T/banana-sync
export BANANA_SYSTEMS="$PWD/system/rootfs/etc/banana/systems.conf"

# the library: 20 GBA games, last changed long ago (RomM's updated_at here is the file's time); the card holds one
L=$T/lib C=$T/card
mkdir -p "$L/gba" "$T/firmware" "$C/banana" "$C/run" "$C/roms/gba"
for i in $(seq -w 1 20); do printf 'gba rom %s' "$i" > "$L/gba/Game $i (USA).gba"; done
touch -d '2026-01-01 00:00:00' "$L/gba/"*
cp "$L/gba/Game 01 (USA).gba" "$C/roms/gba/"
port() { python3 -c 'import socket; s = socket.socket(); s.bind(("127.0.0.1", 0)); print(s.getsockname()[1])'; }
UP=$(port)
python3 ui/tests/fake-romm.py "$UP" "$L" 2> "$T/fake-romm.log" & PIDS="$PIDS $!"
python3 kiwami/tests/romm-kiwami.py "$T/port" "$UP" "$T/firmware" "$T/requests.log" 2> "$T/romm-kiwami.log" & PIDS="$PIDS $!"
for i in $(seq 50); do [ -s "$T/port" ] && curl -s "http://127.0.0.1:$UP/api/heartbeat" > /dev/null && break; sleep 0.1; done
[ -s "$T/port" ] || fail "the stand-ins didn't start: $(tail -n 3 "$T/romm-kiwami.log" "$T/fake-romm.log")"
PORT=$(cat "$T/port")
U=http://127.0.0.1:$PORT
jget() { python3 -c "import json, sys; print(json.load(sys.stdin)$1)"; }
TOK=$(curl -s -XPOST "$U/test/token" -d '{}' | jget '["token"]')
pair() { printf '{"backend": "romm", "server": "%s", "token": "%s", "name": "incremental", "serverVersion": "5.4.1"}\n' "$1" "$TOK" > "$C/banana/romsync.json"; }
pair "$U"
LIST=$C/banana/kiwami-romm-list.jsonl TSV=$C/banana/homesync-server-games.tsv

# (the test's own requests go to upstream's stand-in, past the fork's, whose log holds banana-sync's alone)
srv() { curl -s -H "Authorization: Bearer $TOK" "http://127.0.0.1:$UP$1"; }
rid() { srv "/api/roms?platform_ids=1&limit=500" | python3 -c 'import json, sys; print(next(r["id"] for r in json.load(sys.stdin)["items"] if r["fs_name"] == sys.argv[1]))' "$1"; }
# syncnow <n>: a Sync now, its output in $T/sync<n>.out, its requests in $T/req<n>.log
syncnow() {
    : > "$T/requests.log"
    "$BIN" --root "$C" --run "$C/run" --status "$C/run/sync.status" sync > "$T/sync$1.out" 2>&1 || err "Sync now $1 failed: $(tail -n 3 "$T/sync$1.out")"
    cp "$T/requests.log" "$T/req$1.log"
}
# listings <n>: run <n>'s requests for a system's games in full; changes: for its changes alone; ids: for RomM's ids
listings() { grep '^GET /api/roms?' "$T/req$1.log" | grep -v 'updated_after=' | grep -vc 'search_term=' || true; }
changes() { grep -c '^GET /api/roms?.*&updated_after=' "$T/req$1.log" || true; }
ids() { grep -c '^GET /api/roms/identifiers?' "$T/req$1.log" || true; }
asked() { [ "$(listings "$1")/$(changes "$1")/$(ids "$1")" = "$2" ] || err "$3: $(listings "$1") listings, $(changes "$1") for changes, $(ids "$1") for ids, not $2: $(grep '^GET /api/roms' "$T/req$1.log")"; }
# keys: Get games' list, its games in order; title <key>: its name there; lines: the games in the list kept
keys() { tail -n +2 "$TSV" | cut -f 2; }
title() { tail -n +2 "$TSV" | awk -F '\t' -v k="$1" '$2 == k { print $3 }'; }
lines() { tail -n +2 "$LIST" | wc -l; }

# A13, A14: no list on the card: every system listed, the list kept, this server's
syncnow 1
asked 1 1/0/0 "the first Sync now"
[ "$(head -n 1 "$LIST" 2>/dev/null | jget '["server"]' 2>/dev/null)" = "$U" ] && [ "$(lines)" = 20 ] ||
    err "the first Sync now: the list kept: $(head -n 2 "$LIST" 2>/dev/null)"
[ "$(keys | wc -l)" = 20 ] || err "the first Sync now: Get games' list has $(keys | wc -l) games"
keys > "$T/keys1"
cp "$LIST" "$T/list1"

# the next: RomM's ids and the changes since the last listing's time less 60 s, nothing listed in full; the same
# games; the list on the card not written again
SINCE=$(head -n 1 "$LIST" | python3 -c 'import datetime, json, sys; t = json.load(sys.stdin)["listed_at"]
print(datetime.datetime.fromtimestamp(t - 60, datetime.timezone.utc).strftime("%Y-%m-%dT%H%%3A%M%%3A%SZ") if t > 0 else "no listed_at")')
syncnow 2
asked 2 0/1/1 "the second Sync now"
grep -q "^GET /api/roms?platform_ids=1&platform_id=1&updated_after=$SINCE&" "$T/req2.log" ||
    err "the second Sync now: not the changes since $SINCE: $(grep updated_after "$T/req2.log")"
keys | cmp -s - "$T/keys1" || err "the second Sync now: Get games' list changed"
cmp -s "$LIST" "$T/list1" || err "the second Sync now: the list written again, with nothing changed"
grep -q '  gba: 20 games as kept on the card, 0 changed or new on the server since .*, 0 gone' "$T/sync2.out" || err "the second Sync now: not said: $(grep 'games' "$T/sync2.out")"

# a game renamed in RomM: its new name in Get games' list, where RomM's name order puts it (after Game 20)
curl -s -XPOST "$U/kiwami/touch" -d "{\"id\": $(rid "Game 05 (USA).gba"), \"name\": \"Game Five\"}" > /dev/null
syncnow 3
asked 3 0/1/1 "a game renamed"
[ "$(title "gba/Game 05 (USA).gba")" = "Game Five" ] || err "a game renamed: its name in Get games' list: $(title "gba/Game 05 (USA).gba")"
[ "$(keys | tail -n 1)" = "gba/Game 05 (USA).gba" ] || err "a game renamed: not where RomM's name order puts it: $(keys | tr '\n' ' ')"
[ "$(lines)" = 20 ] || err "a game renamed: the list kept has $(lines) games"

# a game deleted in RomM: gone from Get games' list and the list kept
curl -s -XPOST "$U/test/delete-rom" -d "{\"id\": $(rid "Game 07 (USA).gba")}" > /dev/null
syncnow 4
asked 4 0/1/1 "a game deleted"
! keys | grep -q 'Game 07' || err "a game deleted: still in Get games' list"
[ "$(lines)" = 19 ] || err "a game deleted: the list kept has $(lines) games"
# (Game Five again among the changes: renamed less than 60 s before the last listing began)
grep -q '  gba: 18 games as kept on the card, 1 changed or new on the server since .*, 1 gone' "$T/sync4.out" || err "a game deleted: not said: $(grep 'games as kept' "$T/sync4.out")"

# a game added in RomM: in Get games' list after Game 10, where RomM's name order puts it
printf 'gba rom 10.5' > "$L/gba/Game 10 and a Half (USA).gba"
syncnow 5
asked 5 0/1/1 "a game added"
[ "$(keys | grep -A 1 'Game 10 (USA)' | tail -n 1)" = "gba/Game 10 and a Half (USA).gba" ] || err "a game added: not after Game 10: $(keys | tr '\n' ' ')"
[ "$(lines)" = 20 ] || err "a game added: the list kept has $(lines) games"

# a system new on the server: listed in full, the others by their changes
mkdir -p "$L/snes"
printf 'snes rom' > "$L/snes/Snes Game (USA).sfc"
syncnow 6
asked 6 1/1/1 "a new system"
grep -q '^GET /api/roms?platform_ids=2&platform_id=2&limit=' "$T/req6.log" || err "a new system: not listed in full: $(grep '^GET /api/roms?' "$T/req6.log")"
keys | grep -q '^snes/Snes Game (USA).sfc$' || err "a new system: its game not in Get games' list"
grep -q '  snes: not in the games list kept on the card, listed in full' "$T/sync6.out" || err "a new system: not said"
keys > "$T/keys6"

# RomM's ids that can't be read: every system listed in full, the same games
curl -s -XPOST "$U/kiwami/fault" -d '{"path": "GET /api/roms/identifiers", "kind": "500"}' > /dev/null
syncnow 7
asked 7 2/0/1 "RomM's ids failing"
grep -q "  the games list: every system listed in full, RomM's game ids can't be read" "$T/sync7.out" || err "RomM's ids failing: not said: $(grep 'games list' "$T/sync7.out")"
keys | cmp -s - "$T/keys6" || err "RomM's ids failing: Get games' list differs from the one brought up to date: $(keys | diff "$T/keys6" - | head -n 4)"

# Get games' refresh (banana-sync games): every system listed in full, in the order the list brought up to date had
: > "$T/requests.log"
"$BIN" --root "$C" --run "$C/run" --status "$C/run/sync.status" games > "$T/games.out" 2> "$T/games.err" || err "games failed: $(tail -n 3 "$T/games.out")"
cp "$T/requests.log" "$T/req8.log"
asked 8 2/0/0 "games"
[ "$(wc -l < "$T/games.out")" = 21 ] || err "games: $(wc -l < "$T/games.out") lines on stdout, not the 21 games: $(grep -v '	' "$T/games.out" | head -n 3)"
cut -f 2 "$T/games.out" | cmp -s - "$T/keys6" || err "games: not the order the list brought up to date had: $(cut -f 2 "$T/games.out" | diff "$T/keys6" - | head -n 4)"
[ "$(lines)" = 21 ] || err "games: the list kept has $(lines) games"

# another server's list (the same stand-in by another address): every system listed in full
pair "http://localhost:$PORT"
syncnow 9
asked 9 2/0/0 "another server's list"
grep -q "  the games list: every system listed in full, the one kept on the card is another server's" "$T/sync9.out" || err "another server's list: not said"
[ "$(head -n 1 "$LIST" | jget '["server"]')" = "http://localhost:$PORT" ] || err "another server's list: not kept as this server's"

# Forget: the list gone
"$BIN" --root "$C" --run "$C/run" --status "$C/run/sync.status" forget > /dev/null 2>&1 || err "forget failed"
[ ! -e "$LIST" ] || err "Forget: the list still on the card"

[ -z "$failed" ] || exit 1
echo "romm-incremental: all checks passed (a full listing kept, the changes alone after it, a game renamed, deleted and added, a new system, RomM's ids failing, Get games' refresh, another server's list, Forget)"
