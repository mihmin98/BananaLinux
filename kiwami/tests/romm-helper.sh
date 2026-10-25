#!/bin/sh
# The RomM helper (ui/src/kiwami/romm/), built for this machine, on a made-up card and /run/banana, against the
# stand-in RomM server (kiwami/tests/romm-server.py). Run by kiwami/tests/ci.sh:
#   kiwami/tests/romm-helper.sh
set -e
cd "$(dirname "$0")/../.."
X=$(mktemp -d "${TMPDIR:-/tmp}/kiwami-romm.XXXXXX")
mkdir -p "$X/bin" "$X/card/banana" "$X/run" "$X/etc"
make -s -C ui -f src/kiwami/romm/romm.mk OUT="$X/bin"
H=$X/bin/romm
export KIWAMI_ROMM_CARD=$X/card KIWAMI_RUN=$X/run KIWAMI_ROMM_ETC=$X/etc KIWAMI_MODEL=$X/model
B=$X/card/banana A=$X/card/banana/accounts.conf ST=$X/run/romm.status LOG=$X/card/banana/logs/romm.log
echo "0.4.2-test" > "$X/etc/version"
printf 'timezone = UTC0\n' > "$X/etc/settings.conf"
printf 'Anbernic RG DS Plus\0' > "$X/model"
n=0
SRV= DOWNSRV=
fail() {
    echo "romm-helper: FAIL: $*" >&2
    [ -f "$LOG" ] && tail -n 15 "$LOG" | sed 's/^/  log: /' >&2
    [ -f "$X/srv.log" ] && tail -n 10 "$X/srv.log" | sed 's/^/  server: /' >&2
    exit 1
}
ok() { n=$((n + 1)); }
cleanup() {
    pkill -f "^$H pair" 2>/dev/null || true
    [ -n "$SRV" ] && kill "$SRV" 2>/dev/null
    [ -n "$DOWNSRV" ] && kill "$DOWNSRV" 2>/dev/null
    rm -rf "$X"
}
trap cleanup EXIT

# a stand-in server: its pid in $!, its port in <port file> once it listens
serve() {
    PYTHONPYCACHEPREFIX="$X/pyc" python3 kiwami/tests/romm-server.py "$1" "$2" &
    for i in $(seq 100); do [ -s "$1" ] && return; sleep 0.05; done
    fail "the stand-in server didn't start"
}
serve "$X/port" "$X/srv.log"; SRV=$!
URL=http://127.0.0.1:$(cat "$X/port")
KIWAMI_ROMM_DOWN=1 serve "$X/downport" "$X/down.log"; DOWNSRV=$!
DOWNURL=http://127.0.0.1:$(cat "$X/downport")
post() { python3 -c "import sys, urllib.request; urllib.request.urlopen(urllib.request.Request(sys.argv[1], method='POST'))" "$URL$1"; }
get() { python3 -c "import sys, urllib.request; sys.stdout.write(urllib.request.urlopen(sys.argv[1]).read().decode())" "$URL$1"; }
# the status file's line starting with $1, within 5 s (the pairing goes on in the background)
waitstatus() { for i in $(seq 100); do case "$(cat "$ST" 2>/dev/null)" in "$1"*) ok; return;; esac; sleep 0.05; done; fail "$2: status is '$(cat "$ST" 2>/dev/null)', not '$1...'"; }
# a check's answer and exit status
checks() {
    out=$("$H" check) && st=0 || st=$?
    [ "$out" = "$1" ] || fail "$3: check said '$out', not '$1'"; ok
    [ "$st" = "$2" ] || fail "$3: check exited $st, not $2"; ok
}
key() { sed -n "s/^$1=//p" "$A"; }
OTHERS='ra_user=alice
smb_user=me'

# nothing on the card: unpaired, and nothing asked of a server
checks unpaired 1 "no server"
[ ! -s "$X/srv.log" ] || fail "no server: a request was made"; ok
"$H" pair >"$X/out" && fail "pair with no server succeeded" || true
[ "$(cat "$X/out")" = "pair-failed noserver" ] || fail "pair with no server said '$(cat "$X/out")'"; ok

# a server, not paired yet: the heartbeat asked, unpaired
printf 'ra_user=alice\nromm_url=%s\nsmb_user=me\n' "$URL" > "$A"
checks unpaired 1 "a server, unpaired"
grep -qx "GET /api/heartbeat auth=0" "$X/srv.log" || fail "unpaired: no heartbeat"; ok

# pairing, approved: the code and the page at once, the helper in the background, the keys written once approved,
# the other lines kept
out=$("$H" pair)
code=$(printf '%s' "$out" | sed -n 's/^pairing \([A-Z0-9]*\)	.*/\1/p')
[ -n "$code" ] || fail "pair said '$out'"; ok
[ "$out" = "pairing $code	$URL/pair/device?user_code=$code" ] || fail "pair's line: '$out'"; ok
[ "$(cat "$ST")" = "$out" ] || fail "pair: the status is '$(cat "$ST")'"; ok
pid=$(cat "$X/run/romm-pair.pid")
kill -0 "$pid" || fail "pair: not waiting in the background"; ok
[ -z "$(key romm_token)" ] || fail "pair: a token before the approval"; ok
post "/test/approve?user_code=$code"
waitstatus "paired 5.4.1" "pair approved"
token=$(key romm_token) dev=$(key romm_device_id)
case "$token" in rmm_*) ok;; *) fail "pair approved: romm_token is '$token'";; esac
[ -n "$dev" ] || fail "pair approved: no romm_device_id"; ok
[ "$(grep -v '^romm_' "$A")" = "$OTHERS" ] || fail "pair approved: the other lines changed: $(cat "$A")"; ok
[ "$(key romm_url)" = "$URL" ] || fail "pair approved: romm_url changed"; ok
for i in $(seq 40); do kill -0 "$pid" 2>/dev/null || break; sleep 0.05; done
! kill -0 "$pid" 2>/dev/null || fail "pair approved: still running"; ok
[ ! -e "$X/run/romm-pair.pid" ] || fail "pair approved: the pid file left"; ok
# the device as RomM has it: the handheld's name, the client and its version, the id kept on the card
ident=$(cat "$B/romm/identifier")
printf '%s' "$ident" | grep -qx '[0-9a-f]\{16\}' || fail "the identifier is '$ident'"; ok
[ "$(get /test/devices)" = "$dev BananaOS_on_Anbernic_RG_DS_Plus BananaOS 0.4.2-test $ident" ] || fail "the device: $(get /test/devices)"; ok

# check per version: 5.4.x syncs, newer syncs with a warning, older is refused
checks "ok 5.4.1" 0 "paired"
grep -qx "GET /api/devices/$dev auth=1" "$X/srv.log" || fail "check: the device wasn't asked for with the token"; ok
post "/test/version?v=5.4.0"; checks "ok 5.4.0" 0 "5.4.0"
post "/test/version?v=5.3.2"; checks "old 5.3.2" 1 "5.3.2"
grep -q "RomM 5.3.2 is older than 5.4.0, refused" "$LOG" || fail "5.3.2: not in the log"; ok
"$H" pair >"$X/out" && fail "pair with 5.3.2 succeeded" || true
[ "$(cat "$X/out")" = "pair-failed old 5.3.2" ] || fail "pair with 5.3.2 said '$(cat "$X/out")'"; ok
post "/test/version?v=5.5.0"; checks "newer 5.5.0" 0 "5.5.0"
grep -q "RomM 5.5.0 is newer than 5.4, which this client was written for" "$LOG" || fail "5.5.0: no warning in the log"; ok
post "/test/version?v=5.4.1"
# no answer
sed -i "s|^romm_url=.*|romm_url=$DOWNURL|" "$A"
checks down 1 "server down"
grep -q "check GET /api/heartbeat no connection" "$LOG" || fail "down: no 'no connection' line"; ok
sed -i "s|^romm_url=.*|romm_url=$URL|" "$A"

# log out with the server up: RomM asked to delete the device, then the keys gone and the rest kept
"$H" logout >"$X/out" || fail "logout failed: $(cat "$X/out")"
[ "$(cat "$X/out")" = logged-out ] || fail "logout said '$(cat "$X/out")'"; ok
grep -qx "DELETE /api/devices/$dev auth=1" "$X/srv.log" || fail "logout: no DELETE"; ok
[ -z "$(get /test/devices)" ] || fail "logout: the device is still on RomM"; ok
[ "$(cat "$A")" = "ra_user=alice
romm_url=$URL
smb_user=me" ] || fail "logout: accounts.conf is $(cat "$A")"; ok
checks unpaired 1 "logged out"
tokens=$token

# denied, and expired: the page says so, nothing written
out=$("$H" pair); code=$(printf '%s' "$out" | sed -n 's/^pairing \([A-Z0-9]*\)	.*/\1/p')
post "/test/deny?user_code=$code"
waitstatus "pair-failed denied" "pair denied"
[ -z "$(key romm_token)" ] || fail "pair denied: a token written"; ok
out=$("$H" pair); code=$(printf '%s' "$out" | sed -n 's/^pairing \([A-Z0-9]*\)	.*/\1/p')
post "/test/expire?user_code=$code"
waitstatus "pair-failed expired" "pair expired"
[ -z "$(key romm_token)" ] || fail "pair expired: a token written"; ok

# pairing again while one waits: the first stopped, the second's code the one that pairs
"$H" pair >/dev/null
first=$(cat "$X/run/romm-pair.pid")
out=$("$H" pair); code=$(printf '%s' "$out" | sed -n 's/^pairing \([A-Z0-9]*\)	.*/\1/p')
! kill -0 "$first" 2>/dev/null || fail "a second pair: the first still waiting"; ok
[ "$(cat "$X/run/romm-pair.pid")" != "$first" ] || fail "a second pair: the pid file still names the first"; ok
grep -q "the pairing waiting before ($first) stopped" "$LOG" || fail "a second pair: not in the log"; ok
post "/test/approve?user_code=$code"
waitstatus "paired 5.4.1" "the second pair"
dev=$(key romm_device_id) token=$(key romm_token); tokens="$tokens $token"

# a token RomM no longer takes: revoked; then logged out with no server: forgotten here, the rest kept
post /test/revoke
checks revoked 1 "revoked"
sed -i "s|^romm_url=.*|romm_url=$DOWNURL|" "$A"
"$H" logout >"$X/out" || fail "logout with no server failed: $(cat "$X/out")"
grep -q "logout: the device couldn't be deleted on RomM (no server), forgotten here all the same" "$LOG" || fail "logout with no server: not in the log"; ok
[ "$(cat "$A")" = "ra_user=alice
romm_url=$DOWNURL
smb_user=me" ] || fail "logout with no server: accounts.conf is $(cat "$A")"; ok

# ---- saves around a game ----------------------------------------------------------------------------------------
# upstream's systems.conf and nds-saves (its /data moved into the made-up card), a libretro folder for core.<system>
C=$X/card
cp system/rootfs/etc/banana/systems.conf "$X/etc/"
mkdir -p "$X/libretro" "$C/roms/gba" "$C/roms/nds" "$C/roms/snes" "$C/roms/megadrive" "$X/sd2/roms/psx/Disc Game"
sed "s|/data/|$C/|g" system/rootfs/usr/libexec/banana/nds-saves > "$X/bin/nds-saves"
chmod +x "$X/bin/nds-saves"
export KIWAMI_LIBRETRO=$X/libretro KIWAMI_ROMM_NDS_SAVES=$X/bin/nds-saves
sed -i "s|^romm_url=.*|romm_url=$URL|" "$A"
out=$("$H" pair); code=$(printf '%s' "$out" | sed -n 's/^pairing \([A-Z0-9]*\)	.*/\1/p')
post "/test/approve?user_code=$code"
waitstatus "paired 5.4.1" "pairing for the saves"
tokens="$tokens $(key romm_token)"
push() { KIWAMI_ROMM_SYNC=1 "$H" push "$@"; }
# the server's saves of a game: "<emulator> <hash> <size>" lines, oldest first
saves() { get /test/saves | python3 -c "import json, sys; [print(s['emulator'], s['hash'], s['size']) for s in json.load(sys.stdin) if s['rom_id'] == int(sys.argv[1])]" "$1"; }
plays() { get /test/play-sessions | python3 -c "import json, sys; [print(p['rom_id'], p['duration_ms'], p['save_slot']) for p in json.load(sys.stdin)]"; }
websave() { python3 -c "import sys, urllib.request; urllib.request.urlopen(urllib.request.Request(sys.argv[1], data=open(sys.argv[2], 'rb').read(), method='POST'))" "$URL/test/save?rom_id=$1&slot=autosave&emulator=$2" "$3"; }
sum() { md5sum "$1" | cut -d' ' -f1; }
count() { grep -c "$1" "$X/srv.log" || true; }
T1=$(($(date +%s) - 120)) T2=$(date +%s)

GBA="$C/roms/gba/Pocket Adventure (USA).gba" SRM="$C/saves/gpSP/Pocket Adventure (USA).srm" STATE="$C/states/gpSP/Pocket Adventure (USA).state.auto"
touch "$GBA"
# no save on either side: nothing written; the platform's list fetched (none cached), the match recorded
"$H" pull gba "$GBA" || fail "pull exited non-zero"
[ ! -e "$SRM" ] || fail "pull with no saves wrote one"; ok
grep -q "pull: Pocket Adventure (USA): no save on either side" "$LOG" || fail "pull: no 'no save' line"; ok
grep -q "^GET /api/roms?platform_ids=1&" "$X/srv.log" || fail "pull: the GBA list wasn't fetched"; ok
grep -qx "101	Pocket Adventure (USA).gba	4096	Pocket Adventure" "$B/romm/platforms/gba.tsv" || fail "the GBA list's cache: $(cat "$B/romm/platforms/gba.tsv")"; ok
grep -qx "gba	Pocket Adventure (USA).gba	101" "$B/romm/games" || fail "the match isn't recorded: $(cat "$B/romm/games")"; ok

# the push after it: the save into autosave as gpSP's, with its hash and the play
mkdir -p "$C/saves/gpSP" "$C/states/gpSP"
printf 'save-v1' > "$SRM"
push gba "$GBA" $T1 $T2
[ "$(saves 101)" = "gpsp $(sum "$SRM") 7" ] || fail "push: the server has '$(saves 101)'"; ok
[ "$(plays)" = "101 120000 autosave" ] || fail "push: the plays are '$(plays)'"; ok
grep -q "push: Pocket Adventure (USA): sent (gpsp)" "$LOG" || fail "push: no 'sent' line"; ok

# the same on both sides: nothing changes
n0=$(count "GET /api/saves/")
"$H" pull gba "$GBA"
[ "$(cat "$SRM")" = save-v1 ] && [ ! -e "$SRM.bak" ] || fail "a no-op pull changed the save"; ok
[ "$(count "GET /api/saves/")" = "$n0" ] || fail "a no-op pull downloaded"; ok
[ "$(count "GET /api/roms")" = 1 ] || fail "the list fetched again with a cache"; ok

# RomM's web player's newer save (its own core): taken, the card's kept as .bak, the Quick resume state renamed
printf 'web-v2' > "$X/web2"; websave 101 mgba "$X/web2"
echo state > "$STATE"
"$H" pull gba "$GBA"
[ "$(cat "$SRM")" = web-v2 ] || fail "the web player's save not taken: $(cat "$SRM")"; ok
[ "$(cat "$SRM.bak")" = save-v1 ] || fail "the card's save not kept as .bak"; ok
[ ! -e "$STATE" ] && [ -f "$STATE.bak" ] || fail "the Quick resume state not renamed"; ok
grep -q "the Quick resume state renamed .state.auto.bak" "$LOG" || fail "the guard not in the log"; ok
# ... and confirmed: the next pull and push leave it, nothing sent back
"$H" pull gba "$GBA"
push gba "$GBA" $T1 $T2
[ "$(cat "$SRM")" = web-v2 ] && [ "$(saves 101 | wc -l)" = 2 ] || fail "after taking RomM's: '$(saves 101)'"; ok

# both changed: a conflict noted, the card's save kept, nothing sent
sleep 0.05; printf 'card-v3' > "$SRM"
printf 'web-v3' > "$X/web3"; websave 101 mgba "$X/web3"
"$H" pull gba "$GBA"
[ "$(cat "$SRM")" = card-v3 ] || fail "a conflict overwrote the card's save"; ok
grep -q "^gba	$GBA	101	[0-9]*	20" "$B/romm/conflicts" || fail "the conflict isn't noted: $(cat "$B/romm/conflicts" 2>/dev/null)"; ok
push gba "$GBA" $T1 $T2
[ "$(saves 101 | wc -l)" = 3 ] || fail "a conflict was sent: '$(saves 101)'"; ok
[ "$(wc -l < "$B/romm/conflicts")" = 1 ] || fail "the conflict noted twice"; ok

# RomM's 409 on an upload: the web player saved first, the card's never synced: noted as a conflict
LATE="$C/roms/snes/Late Bloomer.sfc"; touch "$LATE"
printf 'web' > "$X/webl"; websave 402 snes9x "$X/webl"
sleep 0.05; mkdir -p "$C/saves/Snes9x"; printf 'card' > "$C/saves/Snes9x/Late Bloomer.srm"
push snes "$LATE" $T1 $T2
grep -q "Late Bloomer: RomM has a newer save since this handheld's last sync (409)" "$LOG" || fail "the 409 not in the log"; ok
grep -q "^snes	$LATE	402	" "$B/romm/conflicts" || fail "the 409 not noted as a conflict"; ok
[ "$(saves 402)" = "snes9x $(printf web | md5sum | cut -d' ' -f1) 3" ] || fail "the 409 upload kept: '$(saves 402)'"; ok

# a slot emptied on RomM: the card's save renamed .bak, nothing deleted
SPACE="$C/roms/snes/Space Name (Japan).sfc" SSRM="$C/saves/Snes9x/Space Name (Japan).srm"; touch "$SPACE"
printf 'space-v1' > "$SSRM"
push snes "$SPACE" $T1 $T2
post "/test/delete-slot?rom_id=401&slot=autosave"
"$H" pull snes "$SPACE"
[ ! -e "$SSRM" ] && [ "$(cat "$SSRM.bak")" = space-v1 ] || fail "a delete wasn't turned into a .bak"; ok
grep -q "emptied on RomM, the card's save renamed Space Name (Japan).srm.bak" "$LOG" || fail "the delete not in the log"; ok

# core.gba = mgba with the core installed: mGBA's folder and name; with it not installed, gpSP's (and a name that
# differs from RomM's only in case)
printf 'core.gba = mgba\n' > "$B/settings.conf"
touch "$X/libretro/mgba_libretro.so"
CQ="$C/roms/gba/Core Quest.gba"; touch "$CQ"
mkdir -p "$C/saves/mGBA"; printf 'mgba-save' > "$C/saves/mGBA/Core Quest.srm"
push gba "$CQ" $T1 $T2
[ "$(saves 103)" = "mgba $(sum "$C/saves/mGBA/Core Quest.srm") 9" ] || fail "mGBA's save: '$(saves 103)'"; ok
rm "$X/libretro/mgba_libretro.so"
CT="$C/roms/gba/case test (europe).gba"; touch "$CT"
printf 'case-save' > "$C/saves/gpSP/case test (europe).srm"
push gba "$CT" $T1 $T2
[ "$(saves 102)" = "gpsp $(sum "$C/saves/gpSP/case test (europe).srm") 9" ] || fail "gpSP's save of a name in another case: '$(saves 102)'"; ok
rm "$B/settings.conf"

# the DS after DraStic: nds-saves first, the raw .sav sent as melonDS's
DS="$C/roms/nds/Dual Screen (USA).nds" SAV="$C/saves/nds/Dual Screen (USA).sav" DSV="$C/drastic/backup/Dual Screen (USA).dsv"
touch "$DS"; mkdir -p "$C/saves/nds" "$C/drastic/backup"
printf 'old' > "$SAV"; touch -d '-1 hour' "$SAV"
head -c 512 /dev/zero | tr '\0' D > "$DSV"
push nds "$DS" $T1 $T2
cmp -s "$DSV" "$SAV" || fail "nds-saves didn't run before the push"; ok
[ "$(saves 201)" = "melonds $(sum "$DSV") 512" ] || fail "the DS's save: '$(saves 201)'"; ok
# ... and one from RomM with DeSmuME's footer: written as DraStic's, BananaDS's raw at its size
python3 -c "
import struct, sys
snip = b'|<--Snip above here to create a raw sav by excluding this DeSmuME savedata footer:'
sys.stdout.buffer.write(b'E' * 512 + snip.ljust(82, b' ')[:82] + struct.pack('<6I', 512, 512, 0, 0, 0, 0) + b'|-DESMUME SAVE-|')" > "$X/desmume.dsv"
head -c 512 /dev/zero | tr '\0' E > "$X/raw2"
websave 201 desmume "$X/desmume.dsv"
"$H" pull nds "$DS"
cmp -s "$DSV" "$X/desmume.dsv" || fail "RomM's DS save not written as DraStic's"; ok
cmp -s "$SAV" "$X/raw2" || fail "BananaDS's save isn't the raw 512 bytes: $(stat -c %s "$SAV") bytes"; ok
[ "$(head -c 512 "$SAV.bak" | tr -d D)" = "" ] || fail "BananaDS's save before not kept as .bak"; ok

# no server, and one that stalls: the pull gives up within 3 s and exits 0
KIWAMI_ROMM_STALL=10 serve "$X/stallport" "$X/stall.log"; STALLSRV=$!
sed -i "s|^romm_url=.*|romm_url=http://127.0.0.1:$(cat "$X/stallport")|" "$A"
t0=$(date +%s%N); "$H" pull gba "$GBA" || fail "a stalled pull exited non-zero"; t1=$(date +%s%N)
kill "$STALLSRV"
[ $(((t1 - t0) / 1000000)) -lt 3500 ] || fail "a stalled pull took $(((t1 - t0) / 1000000)) ms"; ok
sed -i "s|^romm_url=.*|romm_url=$DOWNURL|" "$A"
t0=$(date +%s%N); "$H" pull gba "$GBA" || fail "a pull with no server exited non-zero"; t1=$(date +%s%N)
[ $(((t1 - t0) / 1000000)) -lt 3500 ] || fail "a pull with no server took $(((t1 - t0) / 1000000)) ms"; ok
[ "$(cat "$SRM")" = card-v3 ] || fail "a pull with no server changed the save"; ok

# a push with no server: kept pending with its play; the next push sends it
MD="$C/roms/megadrive/Blast Processing.md"; touch "$MD"
mkdir -p "$C/saves/PicoDrive"; printf 'blast-v1' > "$C/saves/PicoDrive/Blast Processing.srm"
push megadrive "$MD" $T1 $T2
grep -qxF "push	megadrive	$MD" "$B/romm/pending" || fail "a push with no server isn't pending: $(cat "$B/romm/pending" 2>/dev/null)"; ok
grep -qxF "play	megadrive	$MD	$T1	$T2" "$B/romm/pending" || fail "its play isn't pending"; ok
"$H" pending gba "$GBA" $((T1 - 1000)) $((T2 - 1000))
grep -qxF "play	gba	$GBA	$((T1 - 1000))	$((T2 - 1000))" "$B/romm/pending" || fail "romm pending kept nothing"; ok
sed -i "s|^romm_url=.*|romm_url=$URL|" "$A"
DISC="$X/sd2/roms/psx/Disc Game/Disc Game.m3u"; touch "$DISC"
mkdir -p "$C/saves/PCSX-ReARMed"; printf 'psx-v1' > "$C/saves/PCSX-ReARMed/Disc Game.srm"
push psx "$DISC" $T1 $T2
[ ! -s "$B/romm/pending" ] || fail "pending not sent: $(cat "$B/romm/pending")"; ok
[ "$(saves 501)" = "picodrive $(sum "$C/saves/PicoDrive/Blast Processing.srm") 8" ] || fail "the pending push: '$(saves 501)'"; ok
plays | grep -qx "501 120000 autosave" || fail "the pending play not sent: $(plays)"; ok
[ "$(plays | grep -c '^101 120000 autosave$')" = 2 ] || fail "romm pending's play not sent: $(plays)"; ok
# the second card's game in a folder of its own: matched by the folder's name, its save on the main card
[ "$(saves 301)" = "pcsx_rearmed $(sum "$C/saves/PCSX-ReARMed/Disc Game.srm") 6" ] || fail "the folder game's save: '$(saves 301)'"; ok

# a game RomM doesn't have: skipped and logged; the push fetches the list again, the pull doesn't
UNK="$C/roms/gba/Unknown Game.gba"; touch "$UNK"; printf 'x' > "$C/saves/gpSP/Unknown Game.srm"
n0=$(count "GET /api/roms?platform_ids=1&")
push gba "$UNK" $T1 $T2
grep -q "push: Unknown Game.gba: not on RomM (gba's list)" "$LOG" || fail "a game RomM doesn't have isn't logged"; ok
[ "$(count "GET /api/roms?platform_ids=1&")" = $((n0 + 1)) ] || fail "the push didn't fetch the list again on a miss"; ok
[ ! -s "$B/romm/pending" ] || fail "a game RomM doesn't have left pending"; ok
"$H" pull gba "$UNK"
[ "$(count "GET /api/roms?platform_ids=1&")" = $((n0 + 1)) ] || fail "the pull fetched the list again on a miss"; ok

# a server too old: refused, nothing negotiated; a newer one: synced, with the warning
post "/test/version?v=5.3.2"; n0=$(count "POST /api/sync/negotiate")
"$H" pull gba "$GBA"
[ "$(count "POST /api/sync/negotiate")" = "$n0" ] || fail "5.3.2: negotiated"; ok
post "/test/version?v=5.5.0"
"$H" pull gba "$GBA"
[ "$(count "POST /api/sync/negotiate")" = $((n0 + 1)) ] || fail "5.5.0: not negotiated"; ok
post "/test/version?v=5.4.1"

# two at once: the push waits for the saves lock another holds
flock "$X/run/romm-saves.lock" sleep 1.5 & sleep 0.2
t0=$(date +%s%N); push gba "$GBA" $T1 $T2; t1=$(date +%s%N)
[ $(((t1 - t0) / 1000000)) -ge 1000 ] || fail "a push didn't wait for the lock"; ok
grep -q "push: waiting for another sync to end" "$LOG" || fail "the wait isn't in the log"; ok
[ ! -s "$B/romm/pending" ] || fail "the push that waited didn't go: $(cat "$B/romm/pending")"; ok

# ---- Sync now ---------------------------------------------------------------------------------------------------
# the card as the saves left it, and: the second card in; a game moved into a subfolder, its save changed; a changed
# save; a DS save pending from a shutdown, with its play; Space Name with no save here and a new one from RomM's web
# player. Pocket Adventure and Late Bloomer are still in conflict, Unknown Game isn't on RomM, Disc Game (the second
# card) and Core Quest (its mGBA save synced, none in gpSP's folder) have nothing to do
sync() { KIWAMI_ROMM_SYNC=1 "$H" sync; }
lastis() { [ "$(cut -f2- "$B/romm/last")" = "$1" ] || fail "$2: last is '$(cat "$B/romm/last" 2>/dev/null)', not '<time>	$1'"; ok; }
mkdir -p "$X/run/sd2"; printf '%s/\n' "$X/sd2/roms" > "$X/run/sd2/roms"
mkdir -p "$C/roms/gba/Collection"; mv "$CT" "$C/roms/gba/Collection/"
printf 'case-v2' > "$C/saves/gpSP/case test (europe).srm"
printf 'blast-v2' > "$C/saves/PicoDrive/Blast Processing.srm"
head -c 512 /dev/zero | tr '\0' F > "$DSV"
"$H" pending nds "$DS" $((T1 - 2000)) $((T2 - 2000))
printf 'space-web' > "$X/webs"; websave 401 snes9x "$X/webs"
# a second sync while one runs: busy, at once, nothing asked of RomM
flock "$X/run/romm-saves.lock" sleep 1 & FL=$!; sleep 0.2
n0=$(wc -l < "$X/srv.log")
out=$("$H" sync) && st=0 || st=$?
[ "$st" = 3 ] && [ "$out" = busy ] || fail "a sync while the saves are locked: '$out', exit $st"; ok
[ "$(wc -l < "$X/srv.log")" = "$n0" ] || fail "a busy sync asked RomM"; ok
wait "$FL"
sync || fail "sync exited non-zero"
[ "$(cat "$ST")" = "done sync 3 1 2 1" ] || fail "sync's status: '$(cat "$ST")'"; ok
lastis "conflicts	3	1	2	1" "sync"
[ "$(saves 102 | tail -n 1)" = "gpsp $(sum "$C/saves/gpSP/case test (europe).srm") 7" ] || fail "the subfolder's game: '$(saves 102)'"; ok
[ "$(saves 501 | tail -n 1)" = "picodrive $(sum "$C/saves/PicoDrive/Blast Processing.srm") 8" ] || fail "the changed save: '$(saves 501)'"; ok
[ "$(saves 201 | tail -n 1)" = "melonds $(sum "$DSV") 512" ] || fail "the pending DS save: '$(saves 201)'"; ok
[ "$(cat "$SSRM")" = space-web ] || fail "RomM's save of a game with none here: $(cat "$SSRM" 2>/dev/null)"; ok
[ ! -e "$C/saves/gpSP/Core Quest.srm" ] || fail "a save this handheld synced and no longer has came back"; ok
[ "$(cat "$SRM")" = card-v3 ] && [ "$(cat "$C/saves/Snes9x/Late Bloomer.srm")" = card ] || fail "sync overwrote a save in conflict"; ok
[ "$(cut -f1,3 "$B/romm/conflicts" | tr '\t\n' ' ')" = "gba 101 snes 402 " ] || fail "the conflicts: $(cat "$B/romm/conflicts")"; ok
[ "$(saves 301 | wc -l)" = 1 ] || fail "the second card's game sent again: '$(saves 301)'"; ok
grep -q "sync: Unknown Game.gba: not on RomM (gba's list)" "$LOG" || fail "the game RomM doesn't have isn't logged"; ok
# pending first: the DS's push and its play went, the file emptied
[ ! -s "$B/romm/pending" ] || fail "pending after sync: $(cat "$B/romm/pending")"; ok
plays | grep -qx "201 120000 autosave" || fail "the pending play not sent: $(plays)"; ok
# each platform's list fetched at most once (only the GBA's again here, for the game RomM doesn't have)
[ -z "$(sed -n "$((n0 + 1)),\$p" "$X/srv.log" | grep '^GET /api/roms?' | sort | uniq -d)" ] || fail "a list fetched twice in one sync"; ok
[ "$(sed -n "$((n0 + 1)),\$p" "$X/srv.log" | grep -c '^GET /api/roms?platform_ids=1&')" = 1 ] || fail "the GBA's list not fetched again on a miss"; ok

# resolve: the line has to be the ROM's still; Pocket Adventure with RomM's, Late Bloomer with this card's
PL=$(grep -n "	$GBA	" "$B/romm/conflicts" | cut -d: -f1) LL=$(grep -n "	$LATE	" "$B/romm/conflicts" | cut -d: -f1)
out=$("$H" resolve "$LL" server "$GBA") && fail "resolve of another line's ROM succeeded" || true
[ "$out" = "resolve-failed changed" ] && [ "$(wc -l < "$B/romm/conflicts")" = 2 ] || fail "resolve of another ROM: '$out'"; ok
out=$("$H" resolve 3 local "$GBA") && fail "resolve of a line that isn't there succeeded" || true
[ "$out" = "resolve-failed changed" ] || fail "resolve 3: '$out'"; ok
echo state > "$STATE"
out=$("$H" resolve "$PL" server "$GBA") || fail "resolve server failed: '$out'"
[ "$out" = "resolved $PL server" ] || fail "resolve server said '$out'"; ok
[ "$(cat "$SRM")" = web-v3 ] && [ "$(cat "$SRM.bak")" = card-v3 ] || fail "resolve server: the save is $(cat "$SRM"), .bak $(cat "$SRM.bak")"; ok
[ ! -e "$STATE" ] || fail "resolve server: the Quick resume state kept"; ok
lastis "conflicts	3	1	1	1" "resolve server"
LL=$(grep -n "	$LATE	" "$B/romm/conflicts" | cut -d: -f1)
out=$("$H" resolve "$LL" local "$LATE") || fail "resolve local failed: '$out'"
[ "$(saves 402 | tail -n 1)" = "snes9x $(printf card | md5sum | cut -d' ' -f1) 4" ] || fail "resolve local: '$(saves 402)'"; ok
[ ! -s "$B/romm/conflicts" ] || fail "the conflicts not emptied: $(cat "$B/romm/conflicts")"; ok
lastis "synced	3	1	0	1" "resolve local"
# ... settled: the next sync finds nothing to do but the game RomM doesn't have
sync
[ "$(cat "$ST")" = "done sync 0 0 0 1" ] || fail "a sync after resolving: '$(cat "$ST")'"; ok

# Sync now in the background with no server: down
sed -i "s|^romm_url=.*|romm_url=$DOWNURL|" "$A"
"$H" sync || fail "a sync with no server didn't go into the background"
waitstatus "error sync down" "sync with no server"
lastis "down	0	0	0	1" "sync with no server"
sed -i "s|^romm_url=.*|romm_url=$URL|" "$A"

# a token RomM refuses: the push kept pending, the last sync's state revoked; Sync now says so
post /test/revoke
push gba "$GBA" $T1 $T2
grep -qxF "push	gba	$GBA" "$B/romm/pending" || fail "a refused push isn't pending"; ok
[ "$(cut -f2 "$B/romm/last")" = revoked ] || fail "last isn't revoked: $(cat "$B/romm/last" 2>/dev/null)"; ok
sync && fail "a revoked sync exited 0" || true
[ "$(cat "$ST")" = "error sync revoked" ] || fail "a revoked sync's status: '$(cat "$ST")'"; ok
grep -qxF "push	gba	$GBA" "$B/romm/pending" || fail "a revoked sync lost what was pending"; ok
# paired again: the revoked result dropped, so Sync now starts over
out=$("$H" pair); code=$(printf '%s' "$out" | sed -n 's/^pairing \([A-Z0-9]*\)	.*/\1/p')
post "/test/approve?user_code=$code"
waitstatus "paired 5.4.1" "pairing again after the revoke"
[ ! -e "$B/romm/last" ] || fail "pairing again kept last: $(cat "$B/romm/last")"; ok
grep -q "pair: the last sync's result dropped" "$LOG" || fail "the dropped result isn't in the log"; ok
tokens="$tokens $(key romm_token)"

# the tokens nowhere but accounts.conf: not in the log, the status file or the server's log
for t in $tokens; do
    ! grep -qF "$t" "$LOG" "$ST" "$X/srv.log" || fail "a token in the log, the status or the server's log"; ok
done

# the log moved aside past 256 KB, its last lines kept as romm.1.log
head -c 270000 /dev/zero | tr '\0' 'x' > "$LOG"
"$H" check >/dev/null || true
[ "$(stat -c %s "$B/logs/romm.1.log")" -ge 262144 ] || fail "the log wasn't moved to romm.1.log"; ok
[ "$(stat -c %s "$LOG")" -lt 4096 ] || fail "romm.log not started again"; ok

echo "romm-helper: $n checks passed"
