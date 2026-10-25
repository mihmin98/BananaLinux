#!/bin/sh
# The RomM wrapper around the games (kiwami/rootfs/usr/libexec/kiwami/romm-game), on a made-up card, with a stand-in
# helper (it writes its arguments to a log) and a stand-in game (it writes its pid, its arguments, its stdin, its
# environment and the TERMs it gets, then exits as told). Run by kiwami/tests/ci.sh:
#   kiwami/tests/romm-game.sh
set -e
cd "$(dirname "$0")/../.."
W=$PWD/kiwami/rootfs/usr/libexec/kiwami/romm-game
X=$(mktemp -d "${TMPDIR:-/tmp}/kiwami-romm-game.XXXXXX")
trap 'rm -rf "$X"' EXIT
mkdir -p "$X/card/banana" "$X/etc" "$X/bin"
export KIWAMI_ROMM_CARD=$X/card KIWAMI_ROMM_ETC=$X/etc KIWAMI_ROMM=$X/bin/romm
L=$X/log
n=0
fail() { echo "romm-game: FAIL: $*" >&2; [ -f "$L" ] && sed 's/^/  log: /' "$L" >&2; exit 1; }
ok() { n=$((n + 1)); }

# the stand-in helper: "romm <args>" in the log, one argument a line after it
cat > "$X/bin/romm" <<'EOF'
#!/bin/sh
{ echo "romm $1"; shift; for a; do echo "  $a"; done; } >> "$GAMELOG"
EOF
# the stand-in game: its pid, arguments, stdin's first line and environment; "term" for each TERM it gets, after the
# first going on for half a second (an emulator writing its saves), so a second one would be seen; exits GAME_EXIT,
# after GAME_SLEEP seconds (in tenths, so a TERM is taken at once)
cat > "$X/bin/game" <<'EOF'
#!/bin/sh
t=
trap 'echo term >> "$GAMELOG"; t=1' TERM
echo $$ > "$GAMELOG.pid"
{ echo "game"; for a; do echo "  $a"; done; read -r line || true; echo "  stdin: $line"
  echo "  system: $BANANA_SYSTEM"; echo "  rom: $BANANA_ROM"; } >> "$GAMELOG"
i=0
while [ -z "$t" ] && [ $i -lt $((${GAME_SLEEP:-0} * 10)) ]; do sleep 0.1 & wait $! || true; i=$((i + 1)); done
[ -z "$t" ] || for i in 1 2 3 4 5; do sleep 0.1 & wait $! || true; done
echo "game ended" >> "$GAMELOG"
[ -n "$t" ] && exit 0
exit ${GAME_EXIT:-0}
EOF
chmod +x "$X/bin/romm" "$X/bin/game"
export GAMELOG=$L
ROM="$X/card/roms/gba/It's \"a\" game (USA).gba"
export BANANA_SYSTEM=gba BANANA_ROM="$ROM"

# the wrapper with a line on its stdin, in the background: its pid in $wp, its exit status in $st once it ends
run() {
    rm -f "$L" "$L.pid"
    printf 'from banana-ui\n' | "$W" "$X/bin/game" --core gpsp "$ROM" 2>"$X/err" & wp=$!
    wait $wp && st=0 || st=$?
}
# the game ran as the wrapper itself (an exec), with the arguments, stdin and environment as given, no helper call
execd() {
    run
    [ "$(cat "$L.pid")" = "$wp" ] || fail "$1: the game isn't the wrapper's exec ($(cat "$L.pid") vs $wp)"; ok
    ! grep -q '^romm ' "$L" || fail "$1: the helper ran"; ok
    [ "$(sed -n '/^game$/,/^  rom: /p' "$L")" = "game
  --core
  gpsp
  $ROM
  stdin: from banana-ui
  system: $BANANA_SYSTEM
  rom: $BANANA_ROM" ] || fail "$1: the game's arguments, stdin or environment"; ok
}

execd "nothing set"
printf 'romm = on\n' > "$X/etc/settings.conf"
execd "RomM on, not paired"
printf 'ra_user = alice\nromm_url = http://romm\nromm_token = rmm_abc\nromm_device_id = d1\n' > "$X/card/banana/accounts.conf"
printf 'romm = off\n' > "$X/card/banana/settings.conf"
execd "RomM off on the card over the image's on"
printf 'romm = on\nromm_autosync = off\n' > "$X/card/banana/settings.conf"
execd "Sync around games off"
printf 'theme = carts\n  romm =   on  \n' > "$X/card/banana/settings.conf"
chmod -x "$X/bin/romm"
execd "the helper not executable"
chmod +x "$X/bin/romm"
( unset BANANA_SYSTEM; execd "no BANANA_SYSTEM" ); n=$((n + 3))
( unset BANANA_ROM; execd "no BANANA_ROM" ); n=$((n + 3))

# all of them: pull, the game (a child, with stdin), push with the play's times, in that order; the game's status kept
GAME_EXIT=7 run
[ "$st" = 7 ] || fail "on: the wrapper exited $st, not the game's 7"; ok
[ "$(cat "$L.pid")" != "$wp" ] || fail "on: the game was exec'd"; ok
grep -qx "romm-game: gba: the save synced with RomM around the game" "$X/err" || fail "on: no line in banana-ui's log"; ok
[ "$(grep -v '^  ' "$L" | tr '\n' ' ')" = "romm pull game game ended romm push " ] || fail "on: the order"; ok
[ "$(sed -n '/^romm pull$/,/^game$/p' "$L" | sed -n 2,3p)" = "  gba
  $ROM" ] || fail "on: pull's arguments"; ok
grep -qxF "  stdin: from banana-ui" "$L" || fail "on: the game's stdin"; ok
grep -qxF "  $ROM" "$L" && [ "$(sed -n '/^game$/,/stdin/p' "$L" | sed -n 4p)" = "  $ROM" ] || fail "on: the game's arguments"; ok
set -- $(sed -n '/^romm push$/,$p' "$L" | sed -n '4,5p')
[ "$(sed -n '/^romm push$/,$p' "$L" | sed -n '2,3p')" = "  gba
  $ROM" ] && [ "$1" -gt 1700000000 ] && [ "$2" -ge "$1" ] || fail "on: push's arguments"; ok

# a TERM to the game's process group (bananad's shutdown): the game gets one, the wrapper waits for it to end, then
# keeps the push for later (pending) instead of sending it
rm -f "$L" "$L.pid"
GAME_SLEEP=30 setsid "$W" "$X/bin/game" "$ROM" < /dev/null 2>/dev/null & wp=$!
for i in $(seq 100); do [ -s "$L.pid" ] && break; sleep 0.05; done
[ -s "$L.pid" ] || fail "TERM: the game didn't start"
sleep 0.1
kill -TERM -- -"$wp"
wait $wp && st=0 || st=$?
[ "$(grep -c '^term$' "$L")" = 1 ] || fail "TERM: the game got $(grep -c '^term$' "$L") TERMs"; ok
[ "$(grep -v '^  ' "$L" | tr '\n' ' ')" = "romm pull game term game ended romm pending " ] || fail "TERM: the order"; ok
[ "$st" = 0 ] || fail "TERM: the wrapper exited $st, not the game's 0"; ok

echo "romm-game: $n checks passed"
