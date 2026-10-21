#!/bin/sh
# .7z games on their shelves, headless: kiwami/post-build.sh run on a stand-in target, then banana-ui with the
# post-built systems.conf over a library of empty files, launching the first game of every shelf. A .7z game is listed
# and launched on each of the seven cartridge systems (Super Nintendo's as .7Z, Mega Drive's in a subfolder), and not
# on the DS or the PlayStation; with upstream's own systems.conf, none is. Needs ui/banana-ui (make ui).
# Run by kiwami/tests/ci.sh when banana-ui is built, or by hand:
#   kiwami/tests/7z-ui.sh        (the logs in $OUT, /tmp/kiwami-7z by default)
set -e
cd "$(dirname "$0")/../.."
UI=$PWD/ui
OUT=${OUT:-${TMPDIR:-/tmp}/kiwami-7z}
R=$OUT/roms
SEVEN="gba gb gbc nes snes megadrive n64"
rm -rf "$OUT"; mkdir -p "$OUT/target/etc/banana" "$OUT/target/usr/libexec/banana" "$OUT/data"
cp system/rootfs/usr/libexec/banana/share "$OUT/target/usr/libexec/banana/"
cp system/rootfs/etc/banana/systems.conf "$OUT/target/etc/banana/"
sh kiwami/post-build.sh "$OUT/target" rgds
for s in gba gb gbc nes n64 nds psx; do mkdir -p "$R/$s"; : > "$R/$s/Game $s.7z"; done
mkdir -p "$R/snes" "$R/megadrive/sub"
: > "$R/snes/Game snes.7Z"
: > "$R/megadrive/sub/Game megadrive.7z"
fail=0

ui() {   # ui <systems.conf> <log> <script>
    "$UI/banana-ui" --headless "$OUT/" --size 640x480 --assets "$UI/assets/rnds" --systems "$1" --roms "$R" \
        --data "$OUT/data" --script "$3" 2> "$2" || { echo "FAIL $2: banana-ui exited $?"; fail=1; }
}
# every shelf's first game launched in turn: the shelves counted first by going right round them on the home view
# (Right wraps to the first; "lib" takes no shelf past the last)
launches() {   # launches <systems.conf> <name>: the launch lines in $OUT/<name>.log
    ui "$1" "$OUT/$2-shelves.log" "home 0; $(printf 'press right; %.0s' $(seq $(grep -c '^\[' "$1")))"
    n=$(grep -o 'view 0 sys [0-9]*' "$OUT/$2-shelves.log" | awk '$4 > m { m = $4 } END { print m + 1 }')
    s=0 script="home 0"
    while [ $s -lt $n ]; do script="$script; lib $s 0; press a; wait 3000; wait 1; press b; press b"; s=$((s + 1)); done
    ui "$1" "$OUT/$2.log" "$script"
    echo "7z-ui: $2: $n shelves, $(grep -c 'launch [0-9]*: ' "$OUT/$2.log") launched" >&2
}

# post-built: one .7z game launched on each of the seven, at the path it has on the card, and none elsewhere
launches "$OUT/target/etc/banana/systems.conf" kiwami
log=$OUT/kiwami.log
for s in $SEVEN; do
    case $s in snes) f="snes/Game snes.7Z" ;; megadrive) f="megadrive/sub/Game megadrive.7z" ;; *) f="$s/Game $s.7z" ;; esac
    [ "$(grep -cxF "banana-ui: launch 0: $R/$f (system $s)" "$log")" = 1 ] ||
        { echo "FAIL kiwami: $f not launched once on [$s]: $(grep "(system $s)" "$log")"; fail=1; }
done
[ "$(grep -c 'launch [0-9]*: .*\.7[zZ] (system ' "$log")" = 7 ] ||
    { echo "FAIL kiwami: not seven .7z games launched: $(grep 'launch [0-9]*: ' "$log")"; fail=1; }
grep -q 'launch [0-9]*: .*\.7z (system \(nds\|psx\))$' "$log" &&
    { echo "FAIL kiwami: a .7z game on the DS or the PlayStation: $(grep '(system \(nds\|psx\))' "$log")"; fail=1; }

# upstream's own: no .7z game anywhere, so what lists them is post-build's edit
launches system/rootfs/etc/banana/systems.conf upstream
grep -q 'launch [0-9]*: .*\.7[zZ] (system ' "$OUT/upstream.log" &&
    { echo "FAIL upstream: a .7z game launched: $(grep 'launch [0-9]*: ' "$OUT/upstream.log")"; fail=1; }

[ $fail = 0 ] && echo "7z-ui: all checks passed ($OUT)"
exit $fail
