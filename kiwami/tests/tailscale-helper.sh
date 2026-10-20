#!/bin/sh
# The Tailscale helper (kiwami/rootfs/usr/libexec/kiwami/tailscale) against stand-ins for tailscaled, the CLI,
# modprobe and logger (kiwami/tests/bin/), in a made-up /data/banana and /run/banana. Run by kiwami/tests/ci.sh:
#   kiwami/tests/tailscale-helper.sh
set -e
cd "$(dirname "$0")/../.."
H=$PWD/kiwami/rootfs/usr/libexec/kiwami/tailscale
X=$(mktemp -d "${TMPDIR:-/tmp}/kiwami-ts.XXXXXX")
export PATH="$PWD/kiwami/tests/bin:$PATH"
export KIWAMI_DATA=$X/data KIWAMI_RUN=$X/run KIWAMI_SOCKET=$X/run/tailscale/tailscaled.sock KIWAMI_MODEL=$X/model
export KT_REC=$X/rec KT_SYSLOG=$X/syslog
B=$KIWAMI_DATA S=$KIWAMI_SOCKET
UP="tailscale up --accept-dns=false --netfilter-mode=off"
n=0
fail() { echo "tailscale-helper: FAIL: $*" >&2; [ -f "$KT_REC" ] && sed 's/^/  rec: /' "$KT_REC" >&2; [ -f "$B/logs/tailscale.log" ] && sed 's/^/  log: /' "$B/logs/tailscale.log" >&2; exit 1; }
ok() { n=$((n + 1)); }
# a fresh card and boot: settings.conf with the given lines, no records, nothing running
fresh() {
    "$H" down >/dev/null 2>&1 || true
    # one the helper lost, gone before the next starts (a stand-in deletes its socket on its way out)
    while pkill -KILL -f -- "--statedir=$X/"; do sleep 0.05; done
    rm -rf "$X/data" "$X/run" "$KT_REC" "$KT_SYSLOG" "$X/model"
    mkdir -p "$B" "$X/run"
    : > "$KT_REC"
    printf '%s\n' "$@" > "$B/settings.conf"
}
has() { grep -qxF -- "$1" "$KT_REC" || fail "$2: no '$1'"; ok; }
hasnt() { ! grep -qF -- "$1" "$KT_REC" || fail "$2: '$1' called"; ok; }
# a line in the record or the log, a few seconds at most (the login runs in the background)
waitrec() { for i in $(seq 50); do grep -qxF -- "$1" "$KT_REC" && { ok; return; }; sleep 0.1; done; fail "$2: no '$1'"; }
waitlog() { for i in $(seq 50); do grep -qF -- "$1" "$B/logs/tailscale.log" 2>/dev/null && { ok; return; }; sleep 0.1; done; fail "$2: no '$1' in tailscale.log"; }
alive() { [ -f "$X/run/tailscaled.pid" ] && kill -0 "$(cat "$X/run/tailscaled.pid")" 2>/dev/null; }
trap '"$H" down >/dev/null 2>&1; pkill -KILL -f -- "--statedir=$X/" || true; rm -rf "$X"' EXIT

# boot: nothing with the setting absent or off
for s in "" "tailscale = off"; do
    fresh "$s"
    "$H" boot
    [ ! -s "$KT_REC" ] || fail "boot with '$s': something ran"; ok
done
# status: off when nothing runs
fresh
[ "$("$H" status)" = "state off" ] || fail "status with nothing running"; ok

# boot with it on: tun loaded, tailscaled with exactly its flags, a login with exactly its flags
fresh "tailscale = on"
"$H" boot
has "modprobe tun" "boot"
has "tailscaled --statedir=$B/tailscale --socket=$S --tun=tailscale0 --no-logs-no-support" "boot"
waitrec "$UP --hostname=banana" "boot"
waitlog "logged in" "boot"
alive || fail "boot: tailscaled not running"; ok
grep -qxF "tailscaled: stand-in tailscaled started" "$KT_SYSLOG" || fail "tailscaled's output not in syslog"; ok
[ "$("$H" status)" = '{"BackendState": "Running"}' ] || fail "status while running"; ok
# up again: the same tailscaled, no second one (and the lock free: what the first left running doesn't hold it)
"$H" up || fail "up again: $(tail -n 1 "$B/logs/tailscale.log")"
[ "$(grep -c '^tailscaled ' "$KT_REC")" = 1 ] || fail "up while running started a second tailscaled"; ok

# down: tailscaled stopped, tun unloaded, the state kept
p=$(cat "$X/run/tailscaled.pid")
mkdir -p "$B/tailscale"; echo '{}' > "$B/tailscale/tailscaled.state"
"$H" down
! kill -0 "$p" 2>/dev/null || fail "down: tailscaled still running"; ok
has "modprobe -r tun" "down"
[ ! -e "$X/run/tailscaled.pid" ] && [ ! -e "$S" ] || fail "down: pid file or socket left"; ok
[ -f "$B/tailscale/tailscaled.state" ] || fail "down: the state is gone"; ok
waitlog "stopped" "down"
[ "$("$H" status)" = "state off" ] || fail "status after down"; ok

# an auth key: used, then deleted
fresh
mkdir -p "$B/tailscale"; echo good > "$B/tailscale/authkey"
"$H" up
waitrec "$UP --hostname=banana --auth-key=file:$B/tailscale/authkey" "auth key"
waitlog "logged in with the auth key" "auth key"
[ ! -e "$B/tailscale/authkey" ] || fail "auth key: not deleted after the login"; ok

# a refused one: renamed, its reason in status, and a login by QR code started instead
fresh
mkdir -p "$B/tailscale"; echo bad > "$B/tailscale/authkey"
KT_LOGIN=wait "$H" up
waitrec "$UP --hostname=banana" "refused key: the QR login"
[ ! -e "$B/tailscale/authkey" ] && [ "$(cat "$B/tailscale/authkey.refused")" = bad ] || fail "refused key: not renamed"; ok
"$H" status | grep -qxF "error auth key refused: backend error: invalid key: API key does not exist" || fail "refused key: status gives no reason"; ok
# down while that login waits: the login stopped with it, nothing left running
L="kiwami/tests/bin/tailscale --socket=$S up"
pgrep -f "$L" >/dev/null || fail "refused key: no login waiting"; ok
"$H" down
sleep 0.3
! pgrep -f "$L" >/dev/null || fail "down: the waiting login still runs"; ok

# logout: out, and a new login at once
fresh
"$H" up
waitlog "logged in" "logout"
: > "$KT_REC"
"$H" logout
has "tailscale logout" "logout"
waitrec "$UP --hostname=banana" "logout: the new login"
# and with nothing running it fails, calling nothing
"$H" down
: > "$KT_REC"
! "$H" logout || fail "logout with nothing running succeeded"; ok
hasnt "tailscale logout" "logout with nothing running"

# name: set while running, nothing while off
fresh
"$H" name my-handheld
hasnt "tailscale set" "name while off"
"$H" up
"$H" name my-handheld
has "tailscale set --hostname=my-handheld" "name"

# the name: Settings' tailscale_name, else the board's from the device tree's model, else banana
for m in "Anbernic RG DS:banana-rgds" "Anbernic RG DS Plus:banana-rgds-plus" "Something else:banana"; do
    fresh
    printf '%s\0' "${m%:*}" > "$X/model"
    "$H" up
    waitrec "$UP --hostname=${m#*:}" "name from model '${m%:*}'"
done
fresh "tailscale_name = my-handheld "
printf 'Anbernic RG DS\0' > "$X/model"
"$H" up
waitrec "$UP --hostname=my-handheld" "tailscale_name"

# the log's clock in Settings' time zone (rcS and rcK run the helper with no TZ)
fresh "timezone = XYZ-5"
a=$(TZ=XYZ-5 date '+%F %H:%M'); "$H" up; waitlog "logged in" "time zone"; b=$(TZ=XYZ-5 date '+%F %H:%M')
grep -q "^\($a\|$b\):.. started as" "$B/logs/tailscale.log" || fail "time zone: the log isn't in settings.conf's"; ok

# no tun: up fails, nothing started, the reason in status
fresh
KT_NO_TUN=1 "$H" up && fail "up without tun succeeded"; ok
hasnt "tailscaled " "up without tun"
[ "$("$H" status)" = "state off
error the tun module could not be loaded" ] || fail "status after no tun"; ok

# one change at a time: a stand-in tailscaled as slow to start and stop as the real one, so that runs started close
# together overlap as on the device (M2: on, off, on in quick succession lost a tailscaled)
export KT_START_DELAY=0.3 KT_STOP_DELAY=0.35    # not a multiple of the helper's 0.1 s polling
daemons() { pgrep -f -- "--statedir=$B/tailscale" || true; }
# tailscaled as the helper knows it: $1 on, one running, the one its pid file names, its socket there; off, none of them
settled() {
    if [ "$1" = on ]; then
        [ "$(daemons)" = "$(cat "$X/run/tailscaled.pid" 2>/dev/null)" ] && alive && [ -S "$S" ] || fail "$2: not on as it should be (running: $(daemons | tr '\n' ' '))"
    else
        [ -z "$(daemons)" ] && [ ! -e "$X/run/tailscaled.pid" ] && [ ! -e "$S" ] || fail "$2: not off as it should be (running: $(daemons | tr '\n' ' '))"
    fi
    ok
}
# on, then off and on again with the second switch landing anywhere in the first one's stop (when tailscaled has
# exited and the pid file is still there is the window M2 hit)
for d in 0.26 0.28 0.30 0.32 0.34 0.36 0.38 0.40 0.42 0.44; do
    fresh
    "$H" up
    waitlog "logged in" "up, down, up after $d s"
    "$H" down & sleep "$d"; "$H" up & wait
    settled on "up, down, up after $d s"
done
# the menu's switch, as fast as it goes: the setting written (as the menu does, whole), then apply, 50 ms apart; the
# last setting wins
for v in "on off on off on" "on off on off on off"; do
    fresh
    for s in $v; do
        echo "tailscale = $s" > "$B/settings.conf.new"; mv "$B/settings.conf.new" "$B/settings.conf"
        "$H" apply & sleep 0.05
    done
    wait
    settled "${v##* }" "apply: $v"
done
# status answers while a start holds the lock
fresh
"$H" up & sleep 0.1
[ "$("$H" status)" = "state starting" ] && kill -0 $! 2>/dev/null || fail "status while up holds the lock"; ok
wait
# the lock is free once up returns, though tailscaled, its logger and a login waiting for a QR code still run
fresh
KT_LOGIN=wait "$H" up
sleep 0.2
pgrep -f "kiwami/tests/bin/tailscale --socket=$S up" >/dev/null && alive || fail "lock: nothing left running to test"; ok
flock -n "$X/run/tailscale.lock" true || fail "lock: still held after up returned"; ok
# held for longer than 10 s (a change that hangs), down gives up rather than keep /data from being unmounted
flock "$X/run/tailscale.lock" sleep 11 & sleep 0.2
! "$H" down || fail "down with the lock held succeeded"; ok
waitlog "failed: busy, down gave up waiting for another change" "lock held"
alive || fail "lock held: down stopped tailscaled anyway"; ok
wait

echo "tailscale-helper: all $n checks passed"
