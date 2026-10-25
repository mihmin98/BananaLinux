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
echo "0.4.2-test" > "$OUT/etc/version"
printf 'Anbernic RG DS Plus\0' > "$OUT/model"
D=$FX/data A=$FX/data/accounts.conf
fail=0
# the stand-in server, and one that never answers
SLOG=$OUT/server.log
PYTHONPYCACHEPREFIX="$OUT/pyc" python3 kiwami/tests/romm-server.py "$OUT/port" "$SLOG" & SERVER=$!
KIWAMI_ROMM_DOWN=1 PYTHONPYCACHEPREFIX="$OUT/pyc" python3 kiwami/tests/romm-server.py "$OUT/port-down" "$OUT/server-down.log" & DOWN=$!
trap 'kill $SERVER $DOWN 2>/dev/null' EXIT
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
    logged "banana-ui: kiwami: rows in RomM: romm kiwami.romm.account romm_autosync"
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
