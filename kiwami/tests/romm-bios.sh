#!/bin/sh
# banana-sync's BIOS downloads from RomM (ui/src/kiwami/romm/bios.h; kiwami bios list, kiwami bios get), against
# upstream's stand-in RomM 5.4.1 (ui/tests/fake-romm.py) with the fork's in front of it for the firmware
# (kiwami/tests/romm-kiwami.py): a device pairing asks for firmware.read and collections.read beside the rework's
# scopes; the list holds the files of Banana's systems only, each missing, the same or different against /data/bios
# and, for the DS's three, DraStic's copies; a download lands checked in /data/bios and DraStic's folder under its
# names; a file there already is skipped when the same and kept when different; a download cut short leaves nothing;
# a token without firmware.read gets "error scope" and nothing written; Forget drops the fork's state file. Needs g++,
# python3 and curl. Run by kiwami/tests/ci.sh, or by hand:
#   kiwami/tests/romm-bios.sh
set -e
cd "$(dirname "$0")/../.."
T=$(mktemp -d)
PIDS=
trap 'for p in $PIDS; do kill $p 2>/dev/null; done; [ -n "$KEEP" ] || rm -rf "$T"' EXIT
fail() { echo "romm-bios: $*" >&2; exit 1; }
failed=
err() { echo "romm-bios: $*" >&2; failed=1; }

# banana-sync by ui/Makefile's own rule (its sources on the recipe line), warnings as errors
SRCS=$(awk '/^banana-sync: src\/sync\.cpp/ { getline; print; exit }' ui/Makefile | grep -o 'src/[A-Za-z0-9_/]*\.cpp' | sed 's|^|ui/|')
[ -n "$SRCS" ] || fail "no banana-sync rule in ui/Makefile"
# shellcheck disable=SC2086
${CXX:-g++} -std=c++17 -O2 -Wall -Wno-unused-function -Werror -o "$T/banana-sync" $SRCS -ldl
BIN=$T/banana-sync
export BANANA_SYSTEMS="$PWD/system/rootfs/etc/banana/systems.conf"

# the library (a game a system, so each is a platform) and RomM's firmware: the DS's three (one name in capitals),
# the GBA's, the PlayStation's, and the 3DS's, which Banana has no system for
L=$T/lib FW=$T/firmware
for s in nds gba psx 3ds; do mkdir -p "$L/$s" "$FW/$s"; head -c 2000 /dev/urandom > "$L/$s/Game ($s).bin"; done
head -c 16384 /dev/urandom > "$FW/nds/bios7.bin"
head -c 4096 /dev/urandom > "$FW/nds/BIOS9.BIN"
head -c 262144 /dev/urandom > "$FW/nds/firmware.bin"
head -c 16384 /dev/urandom > "$FW/gba/gba_bios.bin"
head -c 524288 /dev/urandom > "$FW/psx/scph5501.bin"
head -c 65536 /dev/urandom > "$FW/3ds/boot9.bin"
port() { python3 -c 'import socket; s = socket.socket(); s.bind(("127.0.0.1", 0)); print(s.getsockname()[1])'; }
UP=$(port)
python3 ui/tests/fake-romm.py "$UP" "$L" 2> "$T/fake-romm.log" & PIDS="$PIDS $!"
python3 kiwami/tests/romm-kiwami.py "$T/port" "$UP" "$FW" "$T/requests.log" 2> "$T/romm-kiwami.log" & PIDS="$PIDS $!"
for i in $(seq 50); do [ -s "$T/port" ] && curl -s "http://127.0.0.1:$UP/api/heartbeat" > /dev/null && break; sleep 0.1; done
[ -s "$T/port" ] || fail "the stand-ins didn't start: $(tail -n 3 "$T/romm-kiwami.log" "$T/fake-romm.log")"
U=http://127.0.0.1:$(cat "$T/port")

# bs <card> <args...>: banana-sync on the card
bs() { c=$1; shift; "$BIN" --root "$c" --run "$c/run" --status "$c/run/sync.status" "$@"; }
# lastline <file>
lastline() { tail -n 1 "$1"; }

# A4: a pairing by RomM's device flow asks for the rework's seven scopes and the fork's two
C=$T/card
mkdir -p "$C/banana" "$C/run"
bs "$C" pair "$U" > "$T/pair.out" 2>&1 & PP=$!
code=
for i in $(seq 50); do
    case "$(cat "$C/run/sync.status" 2>/dev/null)" in "pair "*) code=$(cut -d' ' -f2 "$C/run/sync.status"); break;; esac
    sleep 0.1
done
[ -n "$code" ] || fail "no pairing code shown: $(cat "$C/run/sync.status" 2>/dev/null)"
curl -s -XPOST "$U/test/approve" -d "{\"user_code\": \"$code\"}" > /dev/null
wait $PP || fail "the pairing failed: $(cat "$T/pair.out")"
grep -qx 'init scopes: me.read,roms.read,platforms.read,assets.read,assets.write,devices.read,devices.write,firmware.read,collections.read' "$T/requests.log" ||
    err "the pairing's scopes: $(grep '^init scopes' "$T/requests.log")"

# A5: the list, everything missing: the five files of the DS, the GBA and the PlayStation, not the 3DS's
bs "$C" kiwami bios list > "$T/list1" || fail "bios list failed: $(cat "$T/list1")"
[ "$(lastline "$T/list1")" = "ok 5" ] || err "bios list's last line: $(lastline "$T/list1")"
for f in nds/bios7.bin nds/BIOS9.BIN nds/firmware.bin gba/gba_bios.bin psx/scph5501.bin; do
    s=${f%%/*} n=${f#*/}
    grep -qE "^[0-9]+	$s	$n	$(wc -c < "$FW/$f")	missing\$" "$T/list1" || err "bios list: no missing line for $f: $(grep -F "$n" "$T/list1")"
done
! grep -q 'boot9' "$T/list1" || err "bios list: the 3DS's file, which Banana has no system for, is listed"
[ ! -e "$C/bios" ] && [ ! -e "$C/drastic" ] || err "bios list wrote to the card"
! grep -q '^GET /api/firmware/' "$T/requests.log" || err "bios list asked for a file"
# ids <list file> [name...]: the ids of those files (every file's without names)
ids() { l=$1; shift; if [ $# = 0 ]; then grep -E '^[0-9]+	' "$l" | cut -f1; else for n in "$@"; do grep -E "^[0-9]+	[a-z]+	$n	" "$l" | cut -f1; done; fi | tr '\n' ' '; }

# what the card has already: a different GBA BIOS, kept; DraStic's own ARM7 BIOS, a different one, kept
mkdir -p "$C/bios" "$C/drastic/system"
head -c 16384 /dev/urandom > "$C/bios/gba_bios.bin"; cp "$C/bios/gba_bios.bin" "$T/own-gba.bin"
head -c 16384 /dev/urandom > "$C/drastic/system/nds_bios_arm7.bin"; cp "$C/drastic/system/nds_bios_arm7.bin" "$T/own-arm7.bin"
bs "$C" kiwami bios list > "$T/list2" || fail "bios list failed: $(cat "$T/list2")"
grep -qE '	gba	gba_bios.bin	16384	different$' "$T/list2" || err "bios list: the card's own GBA BIOS isn't different: $(grep gba_bios "$T/list2")"
grep -qE '	nds	bios7.bin	16384	different$' "$T/list2" || err "bios list: DraStic's own ARM7 BIOS doesn't make bios7.bin different: $(grep bios7 "$T/list2")"

# A6: everything asked for: each file checked in /data/bios, the DS's in DraStic's folder under its names, the
# card's own left alone and said
bs "$C" kiwami bios get $(ids "$T/list2") > "$T/get1" || err "bios get failed: $(cat "$T/get1")"
[ "$(lastline "$T/get1")" = "ok 4 0 2" ] || err "bios get's last line: $(lastline "$T/get1") (not ok 4 0 2)"
for f in nds/bios7.bin nds/BIOS9.BIN nds/firmware.bin psx/scph5501.bin; do
    cmp -s "$FW/$f" "$C/bios/${f#*/}" || err "bios get: bios/${f#*/} isn't RomM's"
done
cmp -s "$FW/nds/BIOS9.BIN" "$C/drastic/system/nds_bios_arm9.bin" || err "bios get: no DraStic nds_bios_arm9.bin from BIOS9.BIN"
cmp -s "$FW/nds/firmware.bin" "$C/drastic/system/nds_firmware.bin" || err "bios get: no DraStic nds_firmware.bin"
cmp -s "$T/own-arm7.bin" "$C/drastic/system/nds_bios_arm7.bin" || err "bios get: DraStic's own nds_bios_arm7.bin was written over"
cmp -s "$T/own-gba.bin" "$C/bios/gba_bios.bin" || err "bios get: the card's own gba_bios.bin was written over"
grep -qE '^[0-9]+	bios/gba_bios.bin	kept$' "$T/get1" || err "bios get: the GBA BIOS kept isn't said: $(grep gba_bios "$T/get1")"
grep -qE '^[0-9]+	drastic/system/nds_bios_arm7.bin	kept$' "$T/get1" || err "bios get: DraStic's ARM7 BIOS kept isn't said"
grep -qE '^[0-9]+	drastic/system/nds_bios_arm9.bin	got$' "$T/get1" || err "bios get: DraStic's ARM9 BIOS not said got"
! find "$C" -name '*.part' -o -name '*.new' | grep -q . || err "bios get left work files: $(find "$C" -name '*.part' -o -name '*.new')"
[ ! -e "$C/banana/logs/sync.log" ] || err "bios get wrote sync.log"
# (once more: what's there is the same, nothing fetched)
: > "$T/requests.log"
bs "$C" kiwami bios get $(ids "$T/list2") > "$T/get2" || err "bios get again failed: $(cat "$T/get2")"
[ "$(lastline "$T/get2")" = "ok 0 3 2" ] || err "bios get again: $(lastline "$T/get2") (not ok 0 3 2)"
! grep -q '/content/' "$T/requests.log" || err "bios get again downloaded: $(grep /content/ "$T/requests.log")"
bs "$C" kiwami bios list > "$T/list3"
[ "$(grep -c '	same$' "$T/list3")" = 3 ] && [ "$(grep -c '	different$' "$T/list3")" = 2 ] || err "bios list after: $(cat "$T/list3")"
# (one in /data/bios already, the same, DraStic's copy missing: copied, nothing fetched)
rm "$C/drastic/system/nds_firmware.bin"
: > "$T/requests.log"
bs "$C" kiwami bios get $(ids "$T/list3" firmware.bin) > "$T/get3" || err "bios get, DraStic's copy only: $(cat "$T/get3")"
cmp -s "$FW/nds/firmware.bin" "$C/drastic/system/nds_firmware.bin" && [ "$(lastline "$T/get3")" = "ok 1 0 0" ] ||
    err "bios get, DraStic's copy only: $(cat "$T/get3")"
! grep -q '/content/' "$T/requests.log" || err "bios get, DraStic's copy only: downloaded it again"

# a download cut short: nothing left, the error said; the next one gets it
head -c 131072 /dev/urandom > "$FW/psx/scph7001.bin"
bs "$C" kiwami bios list > "$T/list4"
grep -qE '	psx	scph7001.bin	131072	missing$' "$T/list4" || err "bios list: the new file: $(grep scph7001 "$T/list4")"
curl -s -XPOST "$U/kiwami/fault" -d '{"path": "/content/scph7001", "kind": "truncate"}' > /dev/null
! bs "$C" kiwami bios get $(ids "$T/list4" scph7001.bin) > "$T/get4" || err "bios get, cut short: it said ok"
case "$(lastline "$T/get4")" in "error scph7001.bin: "*) ;; *) err "bios get, cut short: $(lastline "$T/get4")";; esac
[ ! -e "$C/bios/scph7001.bin" ] && [ ! -e "$C/bios/scph7001.bin.part" ] || err "bios get, cut short: a file left: $(ls "$C/bios")"
bs "$C" kiwami bios get $(ids "$T/list4" scph7001.bin) > "$T/get5" || err "bios get after the cut: $(cat "$T/get5")"
cmp -s "$FW/psx/scph7001.bin" "$C/bios/scph7001.bin" || err "bios get after the cut: not RomM's file"

# A7: a token without firmware.read (a pairing from before the fork asked for it): error scope, nothing written
D=$T/card-old
mkdir -p "$D/banana" "$D/run"
tok=$(curl -s -XPOST "$U/test/token" -d '{}' | python3 -c 'import json, sys; print(json.load(sys.stdin)["token"])')
printf '{"backend": "romm", "server": "%s", "token": "%s", "name": "old", "serverVersion": "5.4.1"}\n' "$U" "$tok" > "$D/banana/romsync.json"
! bs "$D" kiwami bios list > "$T/old-list" || err "no scope: bios list said ok"
[ "$(cat "$T/old-list")" = "error scope" ] || err "no scope: bios list: $(cat "$T/old-list")"
! bs "$D" kiwami bios get $(ids "$T/list4" bios7.bin) > "$T/old-get" || err "no scope: bios get said ok"
[ "$(cat "$T/old-get")" = "error scope" ] || err "no scope: bios get: $(cat "$T/old-get")"
[ ! -e "$D/bios" ] && [ ! -e "$D/drastic" ] || err "no scope: something written"

# not paired, and a verb it doesn't know
E=$T/card-none
mkdir -p "$E/banana" "$E/run"
[ "$(bs "$E" kiwami bios list)" = "error not paired" ] || err "unpaired: bios list didn't say so"
bs "$E" kiwami nothing 2> /dev/null && err "an unknown kiwami verb said ok"

# Forget: the fork's state of that server goes with upstream's lists
echo '{}' > "$C/banana/kiwami-romm.json"
bs "$C" forget > /dev/null 2>&1 || err "forget failed"
[ ! -e "$C/banana/kiwami-romm.json" ] || err "Forget left kiwami-romm.json"

[ -z "$failed" ] || exit 1
echo "romm-bios: all checks passed ($(grep -c '' "$T/list4") files listed, the scopes, kept and skipped files, DraStic's copies, a cut download, no scope)"
