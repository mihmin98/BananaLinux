#!/bin/sh
# The RomM group in banana-ui's Settings, headless at both panel sizes, with the RomM helper built for this machine
# against the stand-in RomM server (romm-server.py): the eighth tile, with RomM's logo, reached by RIGHT and DOWN, the
# grid's first two rows the same as with the fork's groups left out; the group's rows and their log line; nothing run
# and nothing asked of the server when Settings opens, paired or not; the card's own systems.conf without the wrapper
# noted in Sync around games' help. The Account page: the server typed (http://
# added, accounts.conf's other lines kept), pairing by QR code (approved, denied), the check on opening against
# 5.3.2, 5.5.0, no server and a revoked token, Log out (A then another key, A twice), another server forgetting the
# pairing. The groups' frame time with the group and without. No token in the log. Needs ui/banana-ui (make ui),
# python3 and ImageMagick.
# Run by kiwami/tests/ci.sh when banana-ui is built, or by hand:
#   kiwami/tests/romm-ui.sh        (the shots in $OUT, /tmp/kiwami-romm-ui by default)
set -e
cd "$(dirname "$0")/../.."
UI=$PWD/ui
OUT=${OUT:-${TMPDIR:-/tmp}/kiwami-romm-ui}
FX=$OUT/fixture
rm -rf "$OUT"; mkdir -p "$OUT/bin" "$OUT/run" "$OUT/etc"
"$UI/tests/fixture.sh" "$FX" >/dev/null 2>&1
# the helper and Settings read the card at KIWAMI_ROMM_CARD/banana: the fixture's data folder
ln -s data "$FX/banana"
make -s -C ui -f src/kiwami/romm/romm.mk OUT="$OUT/bin"
export KIWAMI_ROMM=$OUT/bin/romm KIWAMI_ROMM_CARD=$FX KIWAMI_RUN=$OUT/run KIWAMI_ROMM_ETC=$OUT/etc KIWAMI_MODEL=$OUT/model
# for Sync now: upstream's systems.conf and nds-saves (its /data moved into the fixture), no second cores
cp system/rootfs/etc/banana/systems.conf "$OUT/etc/"
sed "s|/data/|$FX/|g" system/rootfs/usr/libexec/banana/nds-saves > "$OUT/bin/nds-saves"; chmod +x "$OUT/bin/nds-saves"
mkdir -p "$OUT/libretro"
export KIWAMI_LIBRETRO=$OUT/libretro KIWAMI_ROMM_NDS_SAVES=$OUT/bin/nds-saves
echo "0.4.2-test" > "$OUT/etc/version"
printf 'Anbernic RG DS Plus\0' > "$OUT/model"
D=$FX/data A=$FX/data/accounts.conf
fail=0
# the stand-in server, and one that never answers
SLOG=$OUT/server.log
PYTHONPYCACHEPREFIX="$OUT/pyc" python3 kiwami/tests/romm-server.py "$OUT/port" "$SLOG" & SERVER=$!
KIWAMI_ROMM_DOWN=1 PYTHONPYCACHEPREFIX="$OUT/pyc" python3 kiwami/tests/romm-server.py "$OUT/port-down" "$OUT/server-down.log" & DOWN=$!
SYNCSRV= LOCKER=
trap 'kill $SERVER $DOWN $SYNCSRV $LOCKER 2>/dev/null' EXIT
for i in $(seq 100); do [ -s "$OUT/port" ] && [ -s "$OUT/port-down" ] && break; sleep 0.05; done
[ -s "$OUT/port" ] && [ -s "$OUT/port-down" ] || { echo "FAIL: the stand-in server didn't start"; exit 1; }
URL=http://127.0.0.1:$(cat "$OUT/port") URL_DOWN=http://127.0.0.1:$(cat "$OUT/port-down")
post() { python3 -c "import sys, urllib.request; urllib.request.urlopen(urllib.request.Request(sys.argv[1], method='POST'))" "$URL$1"; }
# approve <approve|deny>: the next pairing decided as soon as the helper has asked for it (the pairing runs keep the
# helper in the foreground, KIWAMI_ROMM_SYNC=1, so Settings' job ends once it is decided)
# (its pid in $AP, waited for after the run: a bare wait would wait for the servers too)
approve() { ( for i in $(seq 200); do grep -q '^POST /api/auth/device/init' "$SLOG" && { post "/test/$1"; exit 0; }; sleep 0.05; done ) & AP=$!; }
# Settings opens on its groups (Display & sound, Sleep & power, Nintendo DS / Other systems, Network, System /
# RetroAchievements, RomM): R seven times is RomM, four Network
SET="home 0; press start"
R4="press r; press r; press r; press r"
GROUP="$SET; $R4; press r; press r; press r"
LIST="$GROUP; press a"
ACCT="$LIST; press down; press a"
SYNC="$LIST; press down; press down; press down; press a"

# run <size> <name> <script>: one run of banana-ui, its log in $OUT/<size>-<name>.log
run() {
    size=$1 name=$2 log=$OUT/$1-$2.log
    "$UI/banana-ui" --headless "$OUT/$size-" --size $size --assets "$UI/assets/rnds" \
        --systems "$PWD/system/rootfs/etc/banana/systems.conf" --roms "$FX/roms" --data "$FX/data" \
        --script "$3" 2> "$log" || { echo "FAIL $size $name: banana-ui exited $?"; fail=1; }
}
logged() { grep -qxF -- "$1" "$log" || { echo "FAIL $size $name: no '$1' in the log"; fail=1; }; }
unlogged() { ! grep -qF -- "$1" "$log" || { echo "FAIL $size $name: '$1' in the log"; fail=1; }; }
lines() {
    got=$(grep -F -- "$1" "$log" | tr '\n' '|')
    [ "$got" = "$2" ] || { echo "FAIL $size $name: '$1' lines are '$got', not '$2'"; fail=1; }
}
shown() {
    for s in "$@"; do
        for p in top bot; do
            f=$OUT/$size-$s-$p.png
            [ -s "$f" ] && [ "$(convert "$f" -format '%[fx:standard_deviation>0.01]' info: 2>/dev/null)" = 1 ] || { echo "FAIL $size $name: $f missing or blank"; fail=1; }
        done
    done
}
same() { cmp -s "$OUT/$size-$1-$3.png" "$OUT/$size-$2-$3.png" || { echo "FAIL $size $name: $1 and $2 differ ($3)"; fail=1; }; }
differ() { cmp -s "$OUT/$size-$1-$3.png" "$OUT/$size-$2-$3.png" && { echo "FAIL $size $name: $1 and $2 are the same ($3)"; fail=1; }; true; }
painted() { convert "$OUT/$size-$1-$2.png" -unique-colors txt:- 2>/dev/null | grep -qi "#$3" || { echo "FAIL $size $name: no #$3 in $1 ($2)"; fail=1; }; }
unpainted() { ! convert "$OUT/$size-$1-$2.png" -unique-colors txt:- 2>/dev/null | grep -qi "#$3" || { echo "FAIL $size $name: #$3 in $1 ($2)"; fail=1; }; }
holds() {
    [ "$(cat "$1" 2>/dev/null)" = "$(printf "$2")" ] || { echo "FAIL $size $name: $1 is '$(cat "$1" 2>/dev/null | tr '\n' '|')'"; fail=1; }
}
served() {
    got=$(tr '\n' '|' < "$SLOG"); : > "$SLOG"
    [ "$got" = "$1" ] || { echo "FAIL $size $name: the server had '$got', not '$1'"; fail=1; }
}
key() { for rc in "$@"; do awk -v rc=$rc 'BEGIN { split(rc, a, ","); printf "touch 1 %.0f %.0f; ", 18 + a[2] * 60.4 + 27, 80 + a[1] * 66 + 29 }'; done; }
tokens=

for size in 640x480 1024x768; do
    rm -rf "$A" "$D/romm" "$OUT/run"/*
    post "/test/version?v=5.4.1"
    : > "$SLOG"

    # ---- the grid: RomM the eighth tile, the first two rows as with the fork's groups left out ----
    run $size groups "$SET; shot g0"
    logged "banana-ui: kiwami: rows in RomM: romm kiwami.romm.account romm_autosync kiwami.romm.sync"
    KIWAMI_RA_GROUP=0 KIWAMI_ROMM_GROUP=0 run $size six "$SET; shot six"
    logged "banana-ui: kiwami: rows in RomM: none"
    shown g0
    same g0 six top; same g0 six bot
    # RIGHT from System, twice: the third row shown, RomM's tile selected, its logo (the shaded purple on no other tile)
    run $size tile "$GROUP; shot g8"
    lines "groups from row" "banana-ui: kiwami: groups from row 1|"
    painted g8 bot 38286A; unpainted g0 bot 38286A
    painted g8 top 38286A	# (the group's icon on the top panel)
    # DOWN from Network reaches it
    run $size down "$SET; $R4; press down; shot g8d"
    same g8 g8d bot
    # with RomM left out, RetroAchievements' seven: RIGHT from RetroAchievements wraps to Display & sound
    KIWAMI_ROMM_GROUP=0 run $size seven "$SET; $R4; press r; press r; press r; shot wrap7"
    same g0 wrap7 bot

    # ---- the rows, and nothing run or asked when Settings opens (a fresh card, then a paired one) ----
    printf 'ra_user=alice\nsmb_user=me\n' > "$A"
    run $size list "$LIST; shot list"
    logged "banana-ui: romm: no server, not paired"
    unlogged "banana-ui: romm: run"
    served ""
    shown list
    printf 'ra_user=alice\nromm_url=%s\nromm_token=rmm_0123456789abcdef\nromm_device_id=dev-1\nsmb_user=me\n' "$URL" > "$A"
    run $size list-paired "$LIST; shot list-paired"
    logged "banana-ui: romm: 127.0.0.1:$(cat "$OUT/port"), paired"
    unlogged "banana-ui: romm: run"
    served ""
    differ list list-paired bot		# (Account: Not paired, then the server's host)
    # the card's own systems.conf: upstream's (no wrapper) is noted, and Sync around games' help says so; a copy with
    # the wrapper (post-build's) isn't
    cp system/rootfs/etc/banana/systems.conf "$D/systems.conf"
    run $size unwrapped "$LIST; press down; press down; shot unwrapped"
    logged "banana-ui: romm: 127.0.0.1:$(cat "$OUT/port"), paired, the card's systems.conf without the wrapper"
    mkdir -p "$OUT/target/etc/banana" "$OUT/target/usr/libexec/banana"
    cp system/rootfs/usr/libexec/banana/share "$OUT/target/usr/libexec/banana/"
    cp system/rootfs/etc/banana/systems.conf "$OUT/target/etc/banana/"
    sh kiwami/post-build.sh "$OUT/target" rgds
    cp "$OUT/target/etc/banana/systems.conf" "$D/systems.conf"
    run $size wrapped "$LIST; press down; press down; shot wrapped"
    logged "banana-ui: romm: 127.0.0.1:$(cat "$OUT/port"), paired"
    differ unwrapped wrapped top
    rm -f "$D/systems.conf"

    # ---- the Account page ----
    # a fresh card: no server, nothing run; the server typed by touch (127), http:// added, the other lines kept
    printf 'ra_user=alice\nsmb_user=me\n' > "$A"
    run $size server "$ACCT; shot nosrv; press a; $(key 0,0 0,1 0,6) press start; shot typed"
    logged "banana-ui: romm: account page: no server, not paired"
    logged "banana-ui: romm: server http://127"
    holds "$A" 'ra_user=alice\nsmb_user=me\nromm_url=http://127'
    logged "banana-ui: romm: check: down"
    shown nosrv typed
    differ nosrv typed bot

    # Pair, denied: the code and its QR code, then the page says so; nothing written
    printf 'ra_user=alice\nromm_url=%s\nsmb_user=me\n' "$URL" > "$A"
    : > "$SLOG"; approve deny
    KIWAMI_ROMM_SYNC=1 run $size denied "$ACCT; press down; press a; shot qr-denied; wait 600; shot denied"
    wait "$AP"
    grep -q "^banana-ui: romm: pairing, code [A-Z0-9]*\$" "$log" || { echo "FAIL $size $name: no code"; fail=1; }
    logged "banana-ui: romm: pairing failed: denied"
    holds "$A" "ra_user=alice\nromm_url=$URL\nsmb_user=me"
    # Pair, approved: the QR code of the pairing link, then Paired, checked: ok 5.4.1
    : > "$SLOG"; approve approve
    KIWAMI_ROMM_SYNC=1 run $size paired "$ACCT; press down; press a; shot qr; wait 600; wait 1; shot paired"
    wait "$AP"
    grep -q "^banana-ui: romm: pairing code [0-9]*x[0-9]* modules for $URL/pair/device?user_code=[A-Z0-9]*\$" "$log" || { echo "FAIL $size $name: no QR code"; fail=1; }
    logged "banana-ui: romm: paired (RomM 5.4.1)"
    logged "banana-ui: romm: check: ok 5.4.1"
    tok=$(sed -n 's/^romm_token=//p' "$A"); dev=$(sed -n 's/^romm_device_id=//p' "$A"); tokens="$tokens $tok"
    [ -n "$tok" ] && [ -n "$dev" ] || { echo "FAIL $size $name: not paired: $(tr '\n' '|' < "$A")"; fail=1; }
    shown qr paired
    painted qr top 000000; painted qr top FFFFFF
    differ qr paired top; differ qr-denied denied top

    # the check when the page opens: 5.3.2 too old, 5.5.0 newer, no server
    post "/test/version?v=5.3.2"
    run $size old "$ACCT; shot old"
    logged "banana-ui: romm: check: old 5.3.2"
    post "/test/version?v=5.5.0"
    run $size newer "$ACCT; shot newer"
    logged "banana-ui: romm: check: newer 5.5.0"
    post "/test/version?v=5.4.1"
    sed -i "s|^romm_url=.*|romm_url=$URL_DOWN|" "$A"
    run $size nosrv-paired "$ACCT; shot down"
    logged "banana-ui: romm: check: down"
    sed -i "s|^romm_url=.*|romm_url=$URL|" "$A"
    differ old newer top; differ newer down top

    # ---- Sync now: its own stand-in server for this size (the saves it keeps), paired through the helper ----
    cp "$A" "$OUT/accounts.main"
    SSLOG=$OUT/server-sync-$size.log
    PYTHONPYCACHEPREFIX="$OUT/pyc" python3 kiwami/tests/romm-server.py "$OUT/port-sync-$size" "$SSLOG" & SYNCSRV=$!
    for i in $(seq 100); do [ -s "$OUT/port-sync-$size" ] && break; sleep 0.05; done
    SURL=http://127.0.0.1:$(cat "$OUT/port-sync-$size")
    spost() { python3 -c "import sys, urllib.request; urllib.request.urlopen(urllib.request.Request(sys.argv[1], method='POST'))" "$SURL$1"; }
    websave() { python3 -c "import sys, urllib.request; urllib.request.urlopen(urllib.request.Request(sys.argv[1], data=sys.argv[2].encode(), method='POST'))" "$SURL/test/save?rom_id=$1&slot=autosave&emulator=$2" "$3"; }
    ssaves() { python3 -c "import json, sys, urllib.request; [print(s['emulator'], s['data']) for s in json.load(urllib.request.urlopen(sys.argv[1])) if s['rom_id'] == int(sys.argv[2])]" "$SURL/test/saves" "$1"; }
    printf 'ra_user=alice\nromm_url=%s\nsmb_user=me\n' "$SURL" > "$A"
    out=$("$KIWAMI_ROMM" pair); code=$(printf '%s' "$out" | sed -n 's/^pairing \([A-Z0-9]*\)	.*/\1/p')
    spost "/test/approve?user_code=$code"
    for i in $(seq 100); do grep -q '^paired' "$OUT/run/romm.status" && break; sleep 0.05; done
    tokens="$tokens $(sed -n 's/^romm_token=//p' "$A")"
    # two games RomM has, each with a save; the fixture's nine others RomM doesn't have
    SKIPPED=9
    PA="$FX/roms/gba/Pocket Adventure (USA).gba" LB="$FX/roms/snes/Late Bloomer.sfc"
    PSRM="$FX/saves/gpSP/Pocket Adventure (USA).srm" LSRM="$FX/saves/Snes9x/Late Bloomer.srm"
    rm -rf "$FX/saves"; mkdir -p "$FX/saves/gpSP" "$FX/saves/Snes9x"
    touch "$PA" "$LB"; printf 'card-v1' > "$PSRM"; printf 'snes-v1' > "$LSRM"
    sed -i '/^romm = /d' "$D/settings.conf"

    # RomM off: the row says so, the page doesn't start
    run $size sync-off "$SYNC; press a; shot sync-off"
    logged "banana-ui: romm: sync now: Off"
    logged "banana-ui: romm: sync page: Off, 0 conflicts"
    logged "banana-ui: romm: sync: not started (off)"
    unlogged "banana-ui: romm: run sync"
    echo 'romm = on' >> "$D/settings.conf"
    # on, never synced: Not yet, nothing run on opening; this card's own systems.conf without the wrapper said on the page
    : > "$SSLOG"
    run $size sync-notyet "$SYNC; shot sync-notyet"
    logged "banana-ui: romm: sync now: Not yet"
    unlogged "banana-ui: romm: run"
    served_sync=$(cat "$SSLOG"); [ -z "$served_sync" ] || { echo "FAIL $size $name: the server was asked: $served_sync"; fail=1; }
    cp system/rootfs/etc/banana/systems.conf "$D/systems.conf"
    run $size sync-unwrapped "$SYNC; shot sync-unwrapped"
    rm -f "$D/systems.conf"
    shown sync-off sync-notyet sync-unwrapped
    differ sync-off sync-notyet top; differ sync-notyet sync-unwrapped top

    # Sync now: both saves sent, the fixture's other games not on RomM; the row says Synced once the page is left
    KIWAMI_ROMM_SYNC=1 run $size sync "$SYNC; press a; wait 1; shot sync-done; press b; shot sync-list"
    logged "banana-ui: romm: run sync"
    logged "banana-ui: romm: sync: done sync 2 0 0 $SKIPPED"
    logged "banana-ui: romm: sync now: Synced"
    [ "$(ssaves 101)" = "gpsp card-v1" ] && [ "$(ssaves 402)" = "snes9x snes-v1" ] || { echo "FAIL $size $name: RomM has '$(ssaves 101)' '$(ssaves 402)'"; fail=1; }
    shown sync-done sync-list
    differ sync-notyet sync-done top

    # both sides changed: two conflicts listed, nothing overwritten
    sleep 1; printf 'card-v2' > "$PSRM"; printf 'snes-v2' > "$LSRM"
    websave 101 mgba web-v2; websave 402 snes9x web-snes
    KIWAMI_ROMM_SYNC=1 run $size conflicts "$SYNC; press a; wait 1; shot conflicts; press down; shot conflict; press b; shot conflicts-list"
    logged "banana-ui: romm: sync: done sync 0 0 2 $SKIPPED"
    logged "banana-ui: romm: sync now: 2 conflicts"
    [ "$(cat "$PSRM")" = card-v2 ] && [ "$(cat "$LSRM")" = snes-v2 ] || { echo "FAIL $size $name: a save in conflict overwritten"; fail=1; }
    [ "$(wc -l < "$D/romm/conflicts")" = 2 ] || { echo "FAIL $size $name: the conflicts are $(tr '\n' '|' < "$D/romm/conflicts")"; fail=1; }
    shown conflicts conflict
    differ conflicts conflict top; differ conflicts conflict bot
    # a choice left with B, and one A then another key: nothing run
    run $size undecided "$SYNC; press down; press a; shot asking; press right; shot asking-romm; press b; press a; press a; press up"
    unlogged "banana-ui: romm: run resolve"
    differ asking asking-romm bot
    # Take RomM's for the first (the card's kept as .bak), Keep this card's for the other (sent over RomM's)
    n1=$(cut -f2 "$D/romm/conflicts" | sed -n 1p); r1=$(basename "$n1")
    KIWAMI_ROMM_SYNC=1 run $size take "$SYNC; press down; press a; press right; press a; shot armed; press a; wait 1; shot taken"
    logged "banana-ui: romm: run resolve 1 server $n1"
    logged "banana-ui: romm: resolved 1 server"
    logged "banana-ui: romm: sync now: 1 conflict"
    n2=$(cut -f2 "$D/romm/conflicts")
    KIWAMI_ROMM_SYNC=1 run $size keep "$SYNC; press down; press a; press a; press a; wait 1; shot kept; press b"
    logged "banana-ui: romm: run resolve 1 local $n2"
    logged "banana-ui: romm: resolved 1 local"
    logged "banana-ui: romm: sync now: Synced"
    [ ! -s "$D/romm/conflicts" ] || { echo "FAIL $size $name: conflicts left: $(tr '\n' '|' < "$D/romm/conflicts")"; fail=1; }
    if [ "$n1" = "$PA" ]; then
        [ "$(cat "$PSRM")" = web-v2 ] && [ "$(cat "$PSRM.bak")" = card-v2 ] || { echo "FAIL $size take: the GBA save is $(cat "$PSRM")"; fail=1; }
        [ "$(ssaves 402 | tail -n 1)" = "snes9x snes-v2" ] || { echo "FAIL $size keep: RomM has '$(ssaves 402 | tail -n 1)'"; fail=1; }
    else
        [ "$(cat "$LSRM")" = web-snes ] && [ "$(cat "$LSRM.bak")" = snes-v2 ] || { echo "FAIL $size take: the SNES save is $(cat "$LSRM")"; fail=1; }
        [ "$(ssaves 101 | tail -n 1)" = "gpsp card-v2" ] || { echo "FAIL $size keep: RomM has '$(ssaves 101 | tail -n 1)'"; fail=1; }
    fi
    shown armed taken kept
    differ armed taken top

    # a sync running (the status says so, a helper holds the saves lock): Syncing, its progress, A starts nothing
    python3 -c "import fcntl, sys, time; f = open(sys.argv[1], 'a'); fcntl.flock(f, fcntl.LOCK_EX); open(sys.argv[2], 'w').close(); time.sleep(60)" \
        "$OUT/run/romm-saves.lock" "$OUT/locked" & LOCKER=$!
    for i in $(seq 100); do [ -e "$OUT/locked" ] && break; sleep 0.05; done
    printf 'running 3 10\tgba\tPocket Adventure (USA)\n' > "$OUT/run/romm.status"
    : > "$SSLOG"
    run $size running "$SYNC; shot running; press a; wait 600"
    logged "banana-ui: romm: sync now: Syncing"
    logged "banana-ui: romm: sync page: Syncing, 0 conflicts, a sync running"
    logged "banana-ui: romm: sync: a sync is running, nothing started"
    unlogged "banana-ui: romm: run sync"
    shown running
    differ sync-done running top
    # the lock held by another helper (a push after a game), the status not running: the helper says busy
    printf 'done sync 0 0 0 1\n' > "$OUT/run/romm.status"
    run $size busy "$SYNC; press a; wait 1; shot busy"
    logged "banana-ui: romm: run sync"
    logged "banana-ui: romm: sync: busy"
    kill $LOCKER; wait $LOCKER 2>/dev/null || true; LOCKER=; rm -f "$OUT/locked"
    # running, and nothing holds the lock: it stopped; Sync now starts again
    printf 'running 3 10\tgba\tPocket Adventure (USA)\n' > "$OUT/run/romm.status"
    KIWAMI_ROMM_SYNC=1 run $size stopped "$SYNC; shot stopped; press a; wait 1"
    unlogged "banana-ui: romm: sync now: Syncing"
    logged "banana-ui: romm: run sync"
    logged "banana-ui: romm: sync: done sync 0 0 0 $SKIPPED"
    differ running stopped top

    # no server: No server, nothing changed
    sed -i "s|^romm_url=.*|romm_url=$URL_DOWN|" "$A"
    KIWAMI_ROMM_SYNC=1 run $size sync-down "$SYNC; press a; wait 1; shot sync-down; press b"
    logged "banana-ui: romm: sync: error sync down"
    logged "banana-ui: romm: sync now: No server"
    shown sync-down
    kill $SYNCSRV; wait $SYNCSRV 2>/dev/null || true; SYNCSRV=
    cp "$OUT/accounts.main" "$A"
    rm -rf "$D/romm"

    # Log out: A, then DOWN, logs nothing out; A twice: the device deleted on RomM, the token and the device gone, the
    # server and the other lines kept
    cp "$A" "$OUT/accounts.before"
    run $size out-down "$ACCT; press down; press down; press a; shot out-armed; press down"
    unlogged "banana-ui: romm: run logout"
    holds "$A" "$(cat "$OUT/accounts.before")"
    : > "$SLOG"
    run $size out "$ACCT; press down; press down; press a; press a; wait 1; shot out"
    logged "banana-ui: romm: run logout"
    logged "banana-ui: romm: logged-out"
    grep -qx "DELETE /api/devices/$dev auth=1" "$SLOG" || { echo "FAIL $size $name: no DELETE"; fail=1; }
    holds "$A" "ra_user=alice\nromm_url=$URL\nsmb_user=me"
    differ out-armed out bot

    # paired again, then the token revoked on RomM: Pair again
    : > "$SLOG"; approve approve
    KIWAMI_ROMM_SYNC=1 run $size again "$ACCT; press down; press a; wait 1; wait 600; wait 1"
    wait "$AP"
    logged "banana-ui: romm: paired (RomM 5.4.1)"
    tokens="$tokens $(sed -n 's/^romm_token=//p' "$A")"
    post /test/revoke
    run $size revoked "$ACCT; shot revoked"
    logged "banana-ui: romm: check: revoked"
    shown revoked
    # Sync now with the token refused: Pair again, and the page doesn't start another
    KIWAMI_ROMM_SYNC=1 run $size sync-revoked "$SYNC; press a; wait 1; press a; shot sync-revoked; press b"
    logged "banana-ui: romm: sync: error sync revoked"
    logged "banana-ui: romm: sync: not started (revoked)"
    logged "banana-ui: romm: sync now: Pair again"
    lines "banana-ui: romm: run sync" "banana-ui: romm: run sync|"
    shown sync-revoked
    # another server (the address with its last digit deleted): the pairing forgotten here, nothing sent to either
    run $size other "$ACCT; press a; press b; press start"
    logged "banana-ui: romm: the pairing with 127.0.0.1:$(cat "$OUT/port") forgotten (another server)"
    ! grep -q '^romm_token=\|^romm_device_id=' "$A" || { echo "FAIL $size $name: the pairing kept: $(tr '\n' '|' < "$A")"; fail=1; }
    grep -qx "romm_url=$(echo "$URL" | sed 's/.$//')" "$A" || { echo "FAIL $size $name: the server: $(tr '\n' '|' < "$A")"; fail=1; }

    # the groups' frame time with the group as without it (bench's average, each under a millisecond, the group's
    # within twice the other's and 0.05 ms)
    run $size bench-romm "$SET; bench 120"; romm_ms=$(sed -n 's/^banana-ui: bench: [0-9]* frames, \([0-9.]*\) ms average.*/\1/p' "$log")
    KIWAMI_ROMM_GROUP=0 run $size bench-seven "$SET; bench 120"; seven_ms=$(sed -n 's/^banana-ui: bench: [0-9]* frames, \([0-9.]*\) ms average.*/\1/p' "$log")
    awk -v a="$romm_ms" -v b="$seven_ms" 'BEGIN { exit !(a != "" && b != "" && a < 1 && b < 1 && a <= 2 * b + 0.05) }' \
        || { echo "FAIL $size bench: the groups take $romm_ms ms with the group, $seven_ms ms without"; fail=1; }

    # no token in any of this size's logs, nor in the helper's
    for t in $tokens; do
        ! grep -lF "$t" "$OUT/$size"-*.log "$D/logs/romm.log" "$SLOG" || { echo "FAIL $size: a token in a log"; fail=1; }
    done
done
[ $fail = 0 ] && echo "romm-ui: all checks passed ($OUT)"
exit $fail
