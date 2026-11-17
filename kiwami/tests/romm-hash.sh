#!/bin/sh
# banana-sync's Sync now finding a ROM on the card under another name than the server's by its contents
# (ui/src/kiwami/romm/hashes.h), against upstream's stand-in RomM 5.4.1 (ui/tests/fake-romm.py) with the fork's in
# front of it (kiwami/tests/romm-kiwami.py): a renamed copy of a server game is its alias, and its save goes up into
# that game's autosave slot; the next Sync now reads no file again, one whose time changed is read again, and one gone
# drops its alias; two card copies of one game, a game already on the card under its own name, two server games of
# the same contents and a file of no server game's contents get no alias, said; an archive and a file of no server
# game's size are never read; the sync before the renamed game asks for it by its remembered id alone. Needs g++,
# python3 and curl. Run by kiwami/tests/ci.sh, or by hand:
#   kiwami/tests/romm-hash.sh
set -e
cd "$(dirname "$0")/../.."
T=$(mktemp -d)
PIDS=
trap 'for p in $PIDS; do kill $p 2>/dev/null; done; [ -n "$KEEP" ] || rm -rf "$T"' EXIT
fail() { echo "romm-hash: $*" >&2; exit 1; }
failed=
err() { echo "romm-hash: $*" >&2; failed=1; }

# banana-sync by ui/Makefile's own rule (its sources on the recipe line), warnings as errors
srcs=$(awk '/^banana-sync: src\/sync\.cpp/ { getline; print; exit }' ui/Makefile | grep -o 'src/[A-Za-z0-9_/]*\.cpp' | sed 's|^|ui/|')
[ -n "$srcs" ] || fail "no banana-sync rule in ui/Makefile"
# shellcheck disable=SC2086
${CXX:-g++} -std=c++17 -O2 -Werror -Wall -Wno-unused-function -o "$T/banana-sync" $srcs -ldl
BIN=$T/banana-sync
export BANANA_SYSTEMS="$PWD/system/rootfs/etc/banana/systems.conf"

# the library (GBA): a game the card has renamed, one it has twice under other names, one it has under its own name
# and another, two games of one content, a zip it has renamed, and a game it has under its own name. The card adds a
# file of Game A's size with other bytes, and one of a size no server game has
L=$T/lib C=$T/card G=$T/card/roms/gba
mkdir -p "$L/gba" "$T/firmware" "$C/banana" "$C/run" "$G" "$C/saves/gba"
printf 'gba rom game a' > "$L/gba/Game A (USA).gba"
printf 'gba rom game bb' > "$L/gba/Game B (USA).gba"
printf 'gba rom owned game' > "$L/gba/Owned (USA).gba"
printf 'gba rom twin game' > "$L/gba/Twin 1 (USA).gba"
printf 'gba rom twin game' > "$L/gba/Twin 2 (USA).gba"
printf 'gba rom game 01, its own name' > "$L/gba/Game 01 (USA).gba"
python3 -c 'import sys, zipfile
with zipfile.ZipFile(sys.argv[1], "w") as z: z.writestr("Zipped (USA).gba", "gba rom zipped game")' "$L/gba/Zipped (USA).zip"
cp "$L/gba/Game A (USA).gba" "$G/My Game A.gba"
cp "$L/gba/Game B (USA).gba" "$G/B one.gba"
cp "$L/gba/Game B (USA).gba" "$G/B two.gba"
cp "$L/gba/Owned (USA).gba" "$G/Owned (USA).gba"
cp "$L/gba/Owned (USA).gba" "$G/Owned copy.gba"
cp "$L/gba/Twin 1 (USA).gba" "$G/Twin.gba"
cp "$L/gba/Zipped (USA).zip" "$G/Zipped copy.zip"
cp "$L/gba/Game 01 (USA).gba" "$G/Game 01 (USA).gba"
printf 'gba rom other!' > "$G/Stranger.gba"
printf 'gba homebrew of a size no server game has' > "$G/Homebrew.gba"
# the renamed game's save on the card, 128 KiB (the size the rework takes of a GBA save)
python3 -c 'import sys; open(sys.argv[1], "wb").write((b"my game a, played here" * 131072)[:131072])' "$C/saves/gba/My Game A.srm"
port() { python3 -c 'import socket; s = socket.socket(); s.bind(("127.0.0.1", 0)); print(s.getsockname()[1])'; }
UP=$(port)
python3 ui/tests/fake-romm.py "$UP" "$L" 2> "$T/fake-romm.log" & PIDS="$PIDS $!"
python3 kiwami/tests/romm-kiwami.py "$T/port" "$UP" "$T/firmware" "$T/requests.log" 2> "$T/romm-kiwami.log" & PIDS="$PIDS $!"
for i in $(seq 50); do [ -s "$T/port" ] && curl -s "http://127.0.0.1:$UP/api/heartbeat" > /dev/null && break; sleep 0.1; done
[ -s "$T/port" ] || fail "the stand-ins didn't start: $(tail -n 3 "$T/romm-kiwami.log" "$T/fake-romm.log")"
U=http://127.0.0.1:$(cat "$T/port")
jget() { python3 -c "import json, sys; print(json.load(sys.stdin)$1)"; }
TOK=$(curl -s -XPOST "$U/test/token" -d '{}' | jget '["token"]')
printf '{"backend": "romm", "server": "%s", "token": "%s", "name": "hash", "serverVersion": "5.4.1"}\n' "$U" "$TOK" > "$C/banana/romsync.json"
printf 'homesync_auto = quit\nhomesync_launch = on\n' > "$C/banana/settings.conf"

# (the test's own requests go to upstream's stand-in, past the fork's, whose log holds banana-sync's alone)
srv() { curl -s -H "Authorization: Bearer $TOK" "http://127.0.0.1:$UP$1"; }
# rid <file name>: the server's id of that game
rid() { srv "/api/roms?platform_ids=1&limit=500" | python3 -c 'import json, sys; print(next(r["id"] for r in json.load(sys.stdin)["items"] if r["fs_name"] == sys.argv[1]))' "$1"; }
A=$(rid "Game A (USA).gba")
# syncnow <n>: a Sync now, its output in $T/sync<n>.out
syncnow() {
    : > "$T/requests.log"
    "$BIN" --root "$C" --run "$C/run" --status "$C/run/sync.status" sync > "$T/sync$1.out" 2>&1 || err "Sync now $1 failed: $(tail -n 3 "$T/sync$1.out")"
}
# reads <n>: the card files Sync now <n> read for their MD5
reads() { grep -c '  by hash: reading ' "$T/sync$1.out" || true; }
said() { grep -qF "  by hash: $2" "$T/sync$1.out" || err "Sync now $1: not said \"by hash: $2\": $(grep 'by hash' "$T/sync$1.out")"; }
state() { python3 -c 'import json, sys; print(json.dumps(json.load(open(sys.argv[1])).get(sys.argv[2], {}), sort_keys=True))' "$C/banana/kiwami-romm.json" "$1" 2>/dev/null; }
# slotted: the MD5 of Game A's newest save in its autosave slot on the server
slotted() { srv /test/state | python3 -c 'import json, sys
s = [v for v in json.load(sys.stdin)["saves"].values() if v["rom_id"] == int(sys.argv[1]) and v["slot"] == "autosave"]
print(max(s, key=lambda v: (v["updated_at"], v["id"]))["content_hash"] if s else "")' "$A"; }

# A12: the first Sync now reads the six card files of a server game's size that no server game is named after
syncnow 1
[ "$(reads 1)" = 6 ] || err "Sync now 1: $(reads 1) files read, not 6: $(grep 'by hash: reading' "$T/sync1.out")"
for f in Homebrew.gba "Zipped copy.zip" "Game 01 (USA).gba" "Owned (USA).gba"; do
    ! grep -qF "by hash: reading gba/$f" "$T/sync1.out" || err "Sync now 1: $f read"
done
said 1 "gba/My Game A.gba is Game A (USA).gba"
said 1 "gba/B one.gba and gba/B two.gba not matched: both are Game B (USA).gba"
said 1 "gba/Owned copy.gba not matched: Owned (USA).gba is on the card under its own name"
said 1 "gba/Twin.gba not matched: two of the server's games have its contents (Twin 1 (USA).gba, Twin 2 (USA).gba)"
said 1 "gba/Stranger.gba not matched: no server game of its system has its contents"
[ "$(state aliases)" = "{\"$A\": \"gba/My Game A.gba\"}" ] || err "Sync now 1: the aliases kept: $(state aliases)"
[ "$(python3 -c 'import json, sys; print(json.load(open(sys.argv[1]))["ids"].get("gba/My Game A.gba", ""))' "$C/banana/kiwami-romm.json")" = "$A" ] ||
    err "Sync now 1: the renamed game's id not remembered: $(state ids)"
[ "$(slotted)" = "$(md5sum < "$C/saves/gba/My Game A.srm" | cut -d' ' -f1)" ] ||
    err "Sync now 1: the renamed game's save not in Game A's autosave slot: $(slotted), $(grep -i 'my game a' "$T/sync1.out")"

# the next Sync now reads nothing again, and keeps the alias
syncnow 2
[ "$(reads 2)" = 0 ] || err "Sync now 2: $(reads 2) files read again: $(grep 'by hash: reading' "$T/sync2.out")"
said 2 "gba/My Game A.gba is Game A (USA).gba"
[ "$(state aliases)" = "{\"$A\": \"gba/My Game A.gba\"}" ] || err "Sync now 2: the aliases kept: $(state aliases)"

# a file whose time changed is read again, the rest not
touch -d '2001-02-03 04:05:06' "$G/My Game A.gba"
syncnow 3
[ "$(reads 3)" = 1 ] && grep -qF 'by hash: reading gba/My Game A.gba' "$T/sync3.out" ||
    err "Sync now 3: not the touched file alone read: $(grep 'by hash: reading' "$T/sync3.out")"
said 3 "gba/My Game A.gba is Game A (USA).gba"

# the sync before the renamed game: its remembered id alone, no listing, no search; the slot's newer version brought
python3 -c 'import json, sys; json.dump({"rom_id": int(sys.argv[1]), "slot": "autosave", "file_name": "Game A (USA).srm",
    "data": (b"from another device" * 131072)[:131072].hex()}, open(sys.argv[2], "w"))' "$A" "$T/seed.json"
curl -s -XPOST "$U/test/save" --data-binary @"$T/seed.json" > /dev/null
: > "$T/requests.log"
"$BIN" --root "$C" --run "$C/run" --status "$C/run/sync.status" sync --rom "gba/My Game A.gba" --auto launch > "$T/launch.out" 2>&1 ||
    err "the sync before the renamed game failed: $(tail -n 3 "$T/launch.out")"
[ "$(grep -c "^GET /api/roms/$A?\$" "$T/requests.log")" = 1 ] && ! grep -q '^GET /api/roms?' "$T/requests.log" ||
    err "the sync before the renamed game: $(grep '^GET /api/roms' "$T/requests.log")"
python3 -c 'import sys; sys.exit(0 if open(sys.argv[1], "rb").read() == (b"from another device" * 131072)[:131072] else 1)' "$C/saves/gba/My Game A.srm" ||
    err "the sync before the renamed game: the slot's newer save not brought: $(cat "$T/launch.out")"
grep -q '^done ' "$C/run/launch.status" || err "the sync before the renamed game: launch.status $(cat "$C/run/launch.status")"

# the renamed file gone: its alias dropped, and its MD5
rm "$G/My Game A.gba"
syncnow 4
[ "$(state aliases)" = "{}" ] || err "Sync now 4: an alias of a file gone kept: $(state aliases)"
! state hashes | grep -q 'My Game A' || err "Sync now 4: the MD5 of a file gone kept: $(state hashes)"
[ "$(reads 4)" = 0 ] || err "Sync now 4: $(reads 4) files read"

[ -z "$failed" ] || exit 1
echo "romm-hash: all checks passed (a renamed game's alias and save, read once, read again when touched, the sync before it by its id, no alias for two copies, an own-name copy, two server games or other bytes; archives and other sizes never read)"
