#!/bin/sh
# banana-sync's installs from RomM's web page (ui/src/kiwami/romm/installs.h; kiwami installs on|off, and doSync's
# claim, report and Wi-Fi hooks), against upstream's stand-in RomM 5.4.1 (ui/tests/fake-romm.py) with the fork's in
# front of it (kiwami/tests/romm-kiwami.py, which answers RomM 5.4.1's install requests): with the setting off nothing
# of installs is asked; kiwami installs on sets the device's capability, which RomM needs before it queues a request,
# and is refused (the capability put back off) for a pairing whose token isn't the device's own; Sync now claims the
# requests, brings a GBA game and a PlayStation disc set through Get games' list and reports them done, a game on the
# card already_installed and a game of several files on a cartridge system failed; a game for the second SD card
# while it's out stays taken until the card is in; a download that fails stays taken and on Get games' list until it
# comes; a game taken off Get games on the handheld, and one with no space for it, are reported failed; a request
# that ran out is dropped; Wi-Fi with Auto sync off or "Leaving a game" runs the games alone, with "And on Wi-Fi" the
# one run brings them; kiwami installs off clears the capability. Needs g++, python3 and curl. Run by
# kiwami/tests/ci.sh, or by hand:
#   kiwami/tests/romm-installs.sh
set -e
cd "$(dirname "$0")/../.."
T=$(mktemp -d)
PIDS=
trap 'for p in $PIDS; do kill $p 2>/dev/null; done; [ -n "$KEEP" ] || rm -rf "$T"' EXIT
fail() { echo "romm-installs: $*" >&2; exit 1; }
failed=
err() { echo "romm-installs: $*" >&2; failed=1; }

# banana-sync by ui/Makefile's own rule (its sources on the recipe line), warnings as errors
SRCS=$(awk '/^banana-sync: src\/sync\.cpp/ { getline; print; exit }' ui/Makefile | grep -o 'src/[A-Za-z0-9_/]*\.cpp' | sed 's|^|ui/|')
[ -n "$SRCS" ] || fail "no banana-sync rule in ui/Makefile"
# shellcheck disable=SC2086
${CXX:-g++} -std=c++17 -O2 -Wall -Wno-unused-function -Werror -o "$T/banana-sync" $SRCS -ldl
BIN=$T/banana-sync
export BANANA_SYSTEMS="$PWD/system/rootfs/etc/banana/systems.conf"

# the library: GBA games, one of several files (which the menu can't start), and a PlayStation disc set
L=$T/lib
mkdir -p "$L/gba/Multi (USA)" "$L/psx/Disc Set (USA)" "$T/firmware"
for g in Alpha Gamma Delta Epsilon Zeta Eta Kappa Theta Lambda Iota Mu; do printf 'gba rom %s' "$g" > "$L/gba/$g (USA).gba"; done
printf 'part 1' > "$L/gba/Multi (USA)/Multi (USA) (Part 1).gba"
printf 'part 2' > "$L/gba/Multi (USA)/Multi (USA) (Part 2).gba"
for d in 1 2; do
    printf 'FILE "Disc Set (USA) (Disc %s).bin" BINARY\n  TRACK 01 MODE2/2352\n    INDEX 01 00:00:00\n' "$d" > "$L/psx/Disc Set (USA)/Disc Set (USA) (Disc $d).cue"
    head -c 4096 /dev/urandom > "$L/psx/Disc Set (USA)/Disc Set (USA) (Disc $d).bin"
done
port() { python3 -c 'import socket; s = socket.socket(); s.bind(("127.0.0.1", 0)); print(s.getsockname()[1])'; }
UP=$(port)
python3 ui/tests/fake-romm.py "$UP" "$L" 2> "$T/fake-romm.log" & PIDS="$PIDS $!"
python3 kiwami/tests/romm-kiwami.py "$T/port" "$UP" "$T/firmware" "$T/requests.log" 2> "$T/romm-kiwami.log" & PIDS="$PIDS $!"
for i in $(seq 50); do [ -s "$T/port" ] && curl -s "http://127.0.0.1:$UP/api/heartbeat" > /dev/null && break; sleep 0.1; done
[ -s "$T/port" ] || fail "the stand-ins didn't start: $(tail -n 3 "$T/romm-kiwami.log" "$T/fake-romm.log")"
U=http://127.0.0.1:$(cat "$T/port")
jget() { python3 -c "import json, sys; print(json.load(sys.stdin)$1)"; }
TOK=$(curl -s -XPOST "$U/test/token" -d '{}' | jget '["token"]')

# bs <args...>: banana-sync on the card
C=$T/card
mkdir -p "$C/banana" "$C/run" "$C/roms/gba"
bs() { "$BIN" --root "$C" --run "$C/run" --status "$C/run/sync.status" "$@"; }
# the card paired by RomM's device flow: its token is the device's own
bs pair "$U" > "$T/pair.out" 2>&1 & PP=$!
code=
for i in $(seq 50); do
    case "$(cat "$C/run/sync.status" 2>/dev/null)" in "pair "*) code=$(cut -d' ' -f2 "$C/run/sync.status"); break;; esac
    sleep 0.1
done
[ -n "$code" ] || fail "no pairing code shown: $(cat "$C/run/sync.status" 2>/dev/null)"
curl -s -XPOST "$U/test/approve" -d "{\"user_code\": \"$code\"}" > /dev/null
wait $PP || fail "the pairing failed: $(cat "$T/pair.out")"
DEV=$(jget '["device"]' < "$C/banana/romsync.json")
[ -n "$DEV" ] || fail "the pairing has no device: $(cat "$C/banana/romsync.json")"
cp "$L/gba/Gamma (USA).gba" "$C/roms/gba/"

# setting <key> <value>: settings.conf's line for it
setting() { touch "$C/banana/settings.conf"; grep -v "^$1 " "$C/banana/settings.conf" > "$T/settings" || true; echo "$1 = $2" >> "$T/settings"; mv "$T/settings" "$C/banana/settings.conf"; }
# rid <system>/<file>: the game's RomM id
rid() { curl -s -H "Authorization: Bearer $TOK" "http://127.0.0.1:$UP/api/roms?platform_ids=$(plat "${1%%/*}")&limit=500" |
    python3 -c 'import json, sys; print(next(r["id"] for r in json.load(sys.stdin)["items"] if r["fs_name"] == sys.argv[1]))' "${1#*/}"; }
plat() { curl -s -H "Authorization: Bearer $TOK" "http://127.0.0.1:$UP/api/platforms" | python3 -c 'import json, sys; print(next(p["id"] for p in json.load(sys.stdin) if p["fs_slug"] == sys.argv[1]))' "$1"; }
# queue <system>/<file>: the game sent to this handheld from RomM's web page; the HTTP status in $T/queued
queue() { curl -s -o /dev/null -w '%{http_code}' -H "Authorization: Bearer $TOK" -XPOST "$U/api/devices/$DEV/installs" -d "{\"rom_id\": $(rid "$1")}" > "$T/queued"; }
# state <rom id>: the install request of that game, as RomM ends it (status, then reason), "live <status>" while it
# lives, "none" without one
state() { curl -s "$U/kiwami/installs" | python3 -c '
import json, sys
j, rid = json.load(sys.stdin), int(sys.argv[1])
live = [r for r in j["live"] if r["rom_id"] == rid]
ended = [r for r in j["ended"] if r["rom_id"] == rid]
print("live " + live[-1]["status"] if live else (ended[-1]["status"] + (": " + ended[-1]["reason"] if ended[-1]["reason"] else "")) if ended else "none")' "$1"; }
is() { got=$(state "$(rid "$1")"); [ "$got" = "$2" ] || err "$3: $1's request: $got, not $2"; }
# run <n> <args...>: banana-sync sync <args> (Sync now without), its output in $T/run<n>.out, its requests in $T/req<n>.log
run() {
    n=$1; shift
    : > "$T/requests.log"
    bs sync "$@" > "$T/run$n.out" 2>&1 || true
    cp "$T/requests.log" "$T/req$n.log"
}
asks() { grep -c "$2" "$T/req$1.log" || true; }
onlist() { grep -qxF "$1" "$C/banana/homesync-get.txt" 2>/dev/null; }
kept() { python3 -c 'import json, sys; sys.exit(0 if any(x["key"] == sys.argv[2] for x in json.load(open(sys.argv[1])).get("installs", {}).values()) else 1)' "$C/banana/kiwami-romm.json" "$1" 2>/dev/null; }
cap() { curl -s -H "Authorization: Bearer $TOK" "$U/api/devices/$DEV" | jget '.get("capabilities") or {}'; }

# A15: installs off (no setting): a Sync now asks nothing of installs nor sets the capability; RomM refuses a request
# for this handheld; Wi-Fi with Auto sync off asks nothing at all
run 1
[ "$(asks 1 /installs)" = 0 ] && ! grep -q '^capabilities' "$T/req1.log" || err "installs off, Sync now: $(grep -e /installs -e '^capabilities' "$T/req1.log")"
grep -q '^done ' "$C/run/sync.status" || err "installs off, Sync now: $(cat "$C/run/sync.status")"
queue gba/Alpha\ \(USA\).gba
[ "$(cat "$T/queued")" = 400 ] || err "a request before the capability: answered $(cat "$T/queued"), not 400"
run 2 --auto wifi
[ ! -s "$T/req2.log" ] || err "installs off, Wi-Fi: $(cat "$T/req2.log")"

# a pairing whose token isn't the device's own (a client token's pairing code): installs on refused, the capability
# put back off
C2=$T/card2
mkdir -p "$C2/banana" "$C2/run"
printf '{"backend": "romm", "server": "%s", "token": "%s", "device": "%s", "name": "other", "serverVersion": "5.4.1"}\n' "$U" "$TOK" "$DEV" > "$C2/banana/romsync.json"
: > "$T/requests.log"
"$BIN" --root "$C2" --run "$C2/run" --status "$C2/run/sync.status" kiwami installs on > "$T/unbound.out" 2>&1 && err "installs on, a token not the device's: it went through"
[ "$(tail -n 1 "$T/unbound.out")" = "error this pairing isn't the handheld's own: pair again from it (Server > Pair)" ] || err "installs on, a token not the device's: $(tail -n 1 "$T/unbound.out")"
[ "$(grep '^capabilities' "$T/requests.log" | cut -d' ' -f3- | tr '\n' ' ')" = '{"remote_install": true} {"remote_install": false} ' ] ||
    err "installs on, a token not the device's: the capability: $(grep '^capabilities' "$T/requests.log")"
[ "$(cap)" = "{'remote_install': False}" ] || err "installs on, a token not the device's: the device's capabilities: $(cap)"

# installs on: the capability set, a request then queued
bs kiwami installs on > "$T/on.out" 2>&1 || err "installs on failed: $(cat "$T/on.out")"
[ "$(tail -n 1 "$T/on.out")" = ok ] || err "installs on: $(tail -n 1 "$T/on.out")"
[ "$(cap)" = "{'remote_install': True}" ] || err "installs on: the device's capabilities: $(cap)"
setting kiwami_romm_installs on

# A16, A17: Sync now with four requests: a GBA game and a disc set downloaded and done, a game on the card
# already_installed, a game of several files failed
for g in "gba/Alpha (USA).gba" "psx/Disc Set (USA)" "gba/Gamma (USA).gba" "gba/Multi (USA)"; do
    queue "$g"; [ "$(cat "$T/queued")" = 201 ] || err "queueing $g: $(cat "$T/queued")"
done
run 3
[ "$(asks 3 "^POST /api/devices/$DEV/installs/claim")" = 1 ] || err "Sync now: $(asks 3 /installs/claim) claims"
cmp -s "$C/roms/gba/Alpha (USA).gba" "$L/gba/Alpha (USA).gba" || err "Sync now: Alpha not on the card"
[ -s "$C/roms/psx/Disc Set (USA).m3u" ] && cmp -s "$C/roms/psx/.discs/Disc Set (USA)/Disc Set (USA) (Disc 2).bin" "$L/psx/Disc Set (USA)/Disc Set (USA) (Disc 2).bin" ||
    err "Sync now: the disc set not on the card: $(find "$C/roms/psx" 2>/dev/null)"
is "gba/Alpha (USA).gba" done "Sync now"
is "psx/Disc Set (USA)" done "Sync now"
is "gba/Gamma (USA).gba" already_installed "Sync now"
is "gba/Multi (USA)" "failed: Banana can't run this game: its system isn't one of Banana's, or it's a game of several files on a cartridge system" "Sync now"
[ ! -s "$C/banana/homesync-get.txt" ] || [ -z "$(grep -v '^#' "$C/banana/homesync-get.txt")" ] || err "Sync now: Get games' list: $(cat "$C/banana/homesync-get.txt")"
grep -q "  installs: gba/Alpha (USA).gba sent from RomM, on Get games' list" "$T/run3.out" && grep -q '  installs: gba/Alpha (USA).gba reported done' "$T/run3.out" ||
    err "Sync now: not said: $(grep installs: "$T/run3.out")"

# A17: Games go to the second SD card, while it's out: the request stays taken, the game on Get games' list; with the
# card in, it comes there and is reported done
setting homesync_roms_to sd2
mkdir -p "$C/run/sd2"; : > "$C/run/sd2/slot"
queue "gba/Delta (USA).gba"
run 4
is "gba/Delta (USA).gba" "live taken" "the second SD card out"
onlist "gba/Delta (USA).gba" && kept "gba/Delta (USA).gba" || err "the second SD card out: Delta not waiting on Get games' list"
mkdir -p "$T/sd2/roms"; echo "$T/sd2/roms" > "$C/run/sd2/roms"
run 5
cmp -s "$T/sd2/roms/gba/Delta (USA).gba" "$L/gba/Delta (USA).gba" || err "the second SD card in: Delta not on it"
is "gba/Delta (USA).gba" done "the second SD card in"
kept "gba/Delta (USA).gba" && err "the second SD card in: Delta still kept as claimed"
setting homesync_roms_to card
rm -rf "$C/run/sd2"

# a download that fails stays taken, and on Get games' list; a game taken off that list on the handheld, and one
# with no space for it, are reported failed (the latter taken off the list); the first comes with the next Sync now
for g in Epsilon Zeta Eta; do queue "gba/$g (USA).gba"; done
curl -s -XPOST "$U/test/fault" -d '{"path": "GET /api/roms/[0-9]+/content/(Epsilon|Zeta|Eta)", "kind": "500", "count": 100}' > /dev/null
run 6
for g in Epsilon Zeta Eta; do is "gba/$g (USA).gba" "live taken" "a download failing"; onlist "gba/$g (USA).gba" || err "a download failing: $g not on Get games' list"; done
bs get --drop "gba/Zeta (USA).gba" > /dev/null
F=$C/banana/homesync-get-failed.txt
grep -v '^gba/Eta' "$F" > "$T/failed"; printf 'gba/Eta (USA).gba\tnot enough space on the card for this sync\n' >> "$T/failed"; mv "$T/failed" "$F"
run 7 --saves-only
is "gba/Zeta (USA).gba" "failed: taken off Get games on the handheld" "a game taken back"
is "gba/Eta (USA).gba" "failed: not enough space on the card for this sync" "no space"
onlist "gba/Eta (USA).gba" && err "no space: Eta still on Get games' list"
is "gba/Epsilon (USA).gba" "live taken" "a download failing, after a run of the saves"
curl -s -XPOST "$U/test/fault" -d '{"clear": true}' > /dev/null
run 8
is "gba/Epsilon (USA).gba" done "a download failing, then coming"
[ "$(asks 8 "^POST /api/devices/$DEV/installs/claim")" = 1 ] || err "a download failing, then coming: $(asks 8 /installs/claim) claims"

# a request that ran out while its game waited: the game still comes (it's on Get games' list), the request dropped
queue "gba/Kappa (USA).gba"
curl -s -XPOST "$U/test/fault" -d '{"path": "GET /api/roms/[0-9]+/content/Kappa", "kind": "500", "count": 100}' > /dev/null
run 9
kept "gba/Kappa (USA).gba" || err "a request running out: Kappa not kept as claimed"
curl -s -XPOST "$U/kiwami/expire" -d '{}' > /dev/null
curl -s -XPOST "$U/test/fault" -d '{"clear": true}' > /dev/null
run 10
[ -e "$C/roms/gba/Kappa (USA).gba" ] || err "a request running out: Kappa didn't come"
kept "gba/Kappa (USA).gba" && err "a request running out: still kept as claimed"
grep -q "  installs: gba/Kappa (USA).gba: RomM's request is gone" "$T/run10.out" || err "a request running out: not said: $(grep installs: "$T/run10.out")"

# A18: Wi-Fi with Auto sync off and nothing sent: one look at the requests, nothing else
setting homesync_auto off
run 11 --auto wifi
[ "$(cat "$T/req11.log")" = "GET /api/devices/$DEV/installs?" ] || err "Wi-Fi, nothing sent: $(cat "$T/req11.log")"
# Wi-Fi with Auto sync off and a game sent: a run of the games alone brings it, no save looked at
queue "gba/Theta (USA).gba"
run 12 --auto wifi
is "gba/Theta (USA).gba" done "Wi-Fi with Auto sync off"
[ "$(asks 12 '/api/saves\|/api/sync/')" = 0 ] || err "Wi-Fi with Auto sync off: saves asked: $(grep -e /api/saves -e /api/sync/ "$T/req12.log" | head -n 3)"
grep -q '|games only' "$C/run/sync.status" || err "Wi-Fi with Auto sync off: $(cat "$C/run/sync.status")"
# the same with "Leaving a game" (no save waiting to be sent): the games alone
setting homesync_auto quit
queue "gba/Lambda (USA).gba"
run 12q --auto wifi
is "gba/Lambda (USA).gba" done "Wi-Fi with Leaving a game"
[ "$(asks 12q '/api/saves\|/api/sync/')" = 0 ] || err "Wi-Fi with Leaving a game: saves asked: $(grep -e /api/saves -e /api/sync/ "$T/req12q.log" | head -n 3)"
# Wi-Fi with "And on Wi-Fi": the rework's one run brings it, the saves with it (one claim, no look at the requests
# before it)
setting homesync_auto both
queue "gba/Iota (USA).gba"
run 13 --auto wifi
is "gba/Iota (USA).gba" done "Wi-Fi with And on Wi-Fi"
[ "$(asks 13 "/installs/claim")/$(asks 13 "^GET /api/devices/$DEV/installs?")" = 1/0 ] ||
    err "Wi-Fi with And on Wi-Fi: $(asks 13 /installs/claim) claims, $(asks 13 "^GET /api/devices/$DEV/installs?") looks"
[ "$(asks 13 '/api/saves\|/api/sync/')" != 0 ] || err "Wi-Fi with And on Wi-Fi: the saves not synced"

# installs off again: nothing of installs asked, the request left; installs off clears the capability
setting kiwami_romm_installs off
setting homesync_auto off
queue "gba/Mu (USA).gba"
run 14
[ "$(asks 14 /installs)" = 0 ] || err "installs off again, Sync now: $(grep /installs "$T/req14.log")"
is "gba/Mu (USA).gba" "live pending" "installs off again"
run 15 --auto wifi
[ ! -s "$T/req15.log" ] || err "installs off again, Wi-Fi: $(cat "$T/req15.log")"
bs kiwami installs off > "$T/off.out" 2>&1 || err "installs off failed: $(cat "$T/off.out")"
[ "$(cap)" = "{'remote_install': False}" ] || err "installs off: the device's capabilities: $(cap)"

[ -z "$failed" ] || exit 1
echo "romm-installs: all checks passed (off, the capability and an unbound pairing, done, already_installed, failed, the second SD card, a failed download, taken back, no space, run out, Wi-Fi with Auto sync off, Leaving a game and And on Wi-Fi, off again)"
