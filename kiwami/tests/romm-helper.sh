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
