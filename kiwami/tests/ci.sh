#!/bin/sh
# The fork's checks, as a step in CI after upstream's (which cloned Buildroot), or by hand:
#   kiwami/tests/ci.sh <buildroot dir>
set -e
BR=$(cd "$1" && pwd)
cd "$(dirname "$0")/../.."

# the fork's shell scripts parse
for f in $(find kiwami -name '*.sh') $(find kiwami/rootfs -type f \( -path '*/init.d/*' -o -path '*/bin/*' -o -path '*/libexec/*' -o -path '*/sleep.d/*' \)) kiwami/tests/bin/*; do
    sh -n "$f"
done

# the Tailscale helper, against stand-ins
kiwami/tests/tailscale-helper.sh

# the fork's cores' RetroArch overrides, in a made-up config folder, and run-retroarch's hook
kiwami/tests/core-overrides.sh

# RetroArch master's credentials file folded back into retroarch.cfg, on a made-up card, and ra-secrets' hook
kiwami/tests/ra-keychain.sh

# RetroArch hotkeys Off covers master's four new hotkeys: the build script that writes them, and run-retroarch's hook
# in a made-up root, with a stand-in retroarch
kiwami/tests/ra-hotkeys.sh

# the vendored C libraries are each their pinned version's files, unchanged
for d in ui/src/kiwami/third_party/*/; do (cd "$d" && sha256sum -c --quiet SHA256SUMS) || { echo "third_party: $d differs from its SHA256SUMS" >&2; exit 1; }; done
echo "third_party: $(ls -d ui/src/kiwami/third_party/*/ | wc -l) vendored libraries checked"

# banana-ui's code of the fork's that needs no screen (the JSON reader, Tailscale's status, the name, the QR code,
# Settings' grid of groups, RetroAchievements' replies, its account on the card and its games' hashes, RomM's replies,
# RomM on the card and the games' saves), with the host's g++ and the vendored C built by banana-ui's own rules
make -s -C ui kiwami-c
T=$(mktemp -d)
${CXX:-g++} -std=c++17 -Wall -Werror -I ui/src -o "$T/units" kiwami/tests/units.cpp ui/src/kiwami/json.cpp ui/src/kiwami/tsstatus.cpp \
    ui/src/kiwami/qrcodegen.cpp ui/src/kiwami/grid.cpp ui/src/kiwami/raclient.cpp ui/src/kiwami/rastore.cpp ui/src/kiwami/rahash.cpp \
    ui/src/kiwami/ra7z.cpp ui/src/kiwami/rachd.cpp ui/src/zip.cpp ui/src/kiwami/conf.cpp ui/src/kiwami/rommcard.cpp \
    ui/src/kiwami/romm/api.cpp ui/src/kiwami/romm/http.cpp ui/src/kiwami/romm/saves.cpp ui/src/kiwami/romm/games.cpp \
    $(find ui/src/kiwami/third_party -name '*.kiwami.o') -ldl -lz
"$T/units" kiwami/tests/tailscale kiwami/tests/ra system/rootfs/etc/banana/systems.conf
# the stand-in RetroAchievements and RomM servers the tests run against (their bytecode in $T, not beside them)
PYTHONPYCACHEPREFIX="$T" python3 -m py_compile kiwami/tests/ra-server.py kiwami/tests/romm-server.py
rm -rf "$T"

# the RomM helper, built for this machine, against the stand-in RomM server
kiwami/tests/romm-helper.sh

# the RomM wrapper around the games, with a stand-in helper and game
kiwami/tests/romm-game.sh

# Tailscale in banana-ui's Settings, headless, when banana-ui is built (CI builds it before)
if [ -x ui/banana-ui ]; then kiwami/tests/tailscale-ui.sh; else echo "tailscale-ui: skipped, no ui/banana-ui (make ui)"; fi

# .7z games on the cartridge systems' shelves, headless, when banana-ui is built
if [ -x ui/banana-ui ]; then kiwami/tests/7z-ui.sh; else echo "7z-ui: skipped, no ui/banana-ui (make ui)"; fi

# the fork's cores in Settings > Other systems, headless, when banana-ui is built
if [ -x ui/banana-ui ]; then kiwami/tests/cores-ui.sh; else echo "cores-ui: skipped, no ui/banana-ui (make ui)"; fi

# the RetroAchievements group in Settings and the grid scrolling to it, headless, when banana-ui is built
if [ -x ui/banana-ui ]; then kiwami/tests/ra-ui.sh; else echo "ra-ui: skipped, no ui/banana-ui (make ui)"; fi

# the RomM group in Settings and its Account page, with the helper against the stand-in RomM server, headless, when
# banana-ui is built
if [ -x ui/banana-ui ]; then kiwami/tests/romm-ui.sh; else echo "romm-ui: skipped, no ui/banana-ui (make ui)"; fi

# post-build.sh vetoes tailscaled's state and RetroArch master's credentials file in upstream's share script, each
# once and the same on a second run, and stops the build when the line isn't there
T=$(mktemp -d)
mkdir -p "$T/usr/libexec/banana" "$T/etc/banana"
cp system/rootfs/usr/libexec/banana/share "$T/usr/libexec/banana/"
cp system/rootfs/etc/banana/systems.conf "$T/etc/banana/"
sh kiwami/post-build.sh "$T" rgds
SH=$T/usr/libexec/banana/share
for v in '\*tailscaled\.state' '\*retroarch-keychain\.\*'; do
    grep -q "^	veto files = /.*/$v/" "$SH" || { echo "post-build: no veto for $v" >&2; exit 1; }
    [ "$(grep '^	veto files = ' "$SH" | grep -o "$v/" | wc -l)" = 1 ] || { echo "post-build: $v vetoed more than once" >&2; exit 1; }
done
cp "$SH" "$T/share.once"

# ... and adds 7z to the cartridge systems' ext lines, once each and nowhere else, the same on a second run, and stops
# the build when one of them has no ext line
C=$T/etc/banana/systems.conf
ext() { awk -v s="[$1]" '/^\[/ { in_s = ($0 == s) } in_s && /^ext *=/ { print }' "$C"; }
for s in gba gb gbc nes snes megadrive n64; do
    ext $s | grep -q ' 7z$' || { echo "post-build: no 7z on [$s]'s ext line: $(ext $s)" >&2; exit 1; }
    [ "$(ext $s | tr ' ' '\n' | grep -cx 7z)" = 1 ] || { echo "post-build: 7z more than once on [$s]: $(ext $s)" >&2; exit 1; }
done
for s in nds psx; do
    ! ext $s | grep -q '7z' || { echo "post-build: 7z on [$s]: $(ext $s)" >&2; exit 1; }
done
cp "$C" "$T/systems.once"
sh kiwami/post-build.sh "$T" rgds
cmp -s "$C" "$T/systems.once" || { echo "post-build: a second run changed systems.conf" >&2; exit 1; }
cmp -s "$SH" "$T/share.once" || { echo "post-build: a second run changed the share script" >&2; exit 1; }
awk '/^\[/ { in_s = ($0 == "[n64]") } !(in_s && /^ext *=/)' system/rootfs/etc/banana/systems.conf > "$C"
! sh kiwami/post-build.sh "$T" rgds 2>"$T/err" || { echo "post-build: no failure without [n64]'s ext line" >&2; exit 1; }
grep -q '\[n64\]' "$T/err" || { echo "post-build: the failure doesn't name [n64]: $(cat "$T/err")" >&2; exit 1; }
[ ! -e "$C.tmp" ] || { echo "post-build: left its temporary behind" >&2; exit 1; }
cp system/rootfs/etc/banana/systems.conf "$C"

# ... and puts the RomM wrapper in front of the nine synced systems' run lines, once each and nowhere else, the same
# on a second run, and stops the build when one of them has no run line; the RetroArch systems' lines still name
# their launcher, which is what gives a game the game panel (ui/src/main.cpp's ra)
R=/usr/libexec/kiwami/romm-game
run() { awk -v s="[$1]" '/^\[/ { in_s = ($0 == s) } in_s && /^run *= */ { sub(/^run *= */, ""); print }' "$2"; }
sh kiwami/post-build.sh "$T" rgds
for s in $(awk '/^\[.*\]$/ { print substr($0, 2, length($0) - 2) }' "$C"); do
    want=$(run $s system/rootfs/etc/banana/systems.conf)
    case " nds gba gbc gb nes snes megadrive psx n64 " in *" $s "*) want="$R $want" ;; esac
    [ "$(run $s "$C")" = "$want" ] || { echo "post-build: [$s]'s run line is '$(run $s "$C")', not '$want'" >&2; exit 1; }
done
for s in gba gbc gb nes snes megadrive n64; do
    run $s "$C" | grep -q " /usr/libexec/banana/run-retroarch " || { echo "post-build: [$s]'s run line lost run-retroarch" >&2; exit 1; }
done
run psx "$C" | grep -q " /usr/libexec/banana/run-ps1 " || { echo "post-build: [psx]'s run line lost run-ps1" >&2; exit 1; }
cp "$C" "$T/systems.once"
sh kiwami/post-build.sh "$T" rgds
cmp -s "$C" "$T/systems.once" || { echo "post-build: a second run wrapped a run line again" >&2; exit 1; }
awk '/^\[/ { in_s = ($0 == "[psx]") } !(in_s && /^run *=/)' system/rootfs/etc/banana/systems.conf > "$C"
! sh kiwami/post-build.sh "$T" rgds 2>"$T/err" || { echo "post-build: no failure without [psx]'s run line" >&2; exit 1; }
grep -q '\[psx\]' "$T/err" || { echo "post-build: the failure doesn't name [psx]: $(cat "$T/err")" >&2; exit 1; }
[ ! -e "$C.tmp" ] || { echo "post-build: left its temporary behind" >&2; exit 1; }
cp system/rootfs/etc/banana/systems.conf "$C"

# ... and over the post-built file, banana-ui headless launches a game on each of the nine shelves (every shelf's
# first game in turn, the shelves counted by going right round them, as kiwami/tests/7z-ui.sh does)
if [ -x ui/banana-ui ]; then
    sh kiwami/post-build.sh "$T" rgds
    mkdir -p "$T/data"
    for s in nds gba gbc gb nes snes megadrive psx n64; do
        e=$(awk -v s="[$s]" '/^\[/ { in_s = ($0 == s) } in_s && /^ext *=/ { print $3 }' "$C")
        mkdir -p "$T/roms/$s"; : > "$T/roms/$s/Game $s.$e"
    done
    shelves() { ui/banana-ui --headless "$T/" --size 640x480 --assets ui/assets/rnds --systems "$C" --roms "$T/roms" \
        --data "$T/data" --script "$1" 2>&1; }
    k=$(shelves "home 0; $(printf 'press right; %.0s' $(seq $(grep -c '^\[' "$C")))" |
        grep -o 'view 0 sys [0-9]*' | awk '$4 > m { m = $4 } END { print m + 1 }')
    script="home 0"; i=0
    while [ $i -lt $k ]; do script="$script; lib $i 0; press a; wait 3000; wait 1; press b; press b"; i=$((i + 1)); done
    shelves "$script" > "$T/shelves.log"
    for s in nds gba gbc gb nes snes megadrive psx n64; do
        grep -q "^banana-ui: launch [0-9]*: $T/roms/$s/Game $s\.[a-z0-9]* (system $s)$" "$T/shelves.log" ||
            { echo "post-build: no game launched on [$s] over the wrapped systems.conf: $(grep 'launch [0-9]*: ' "$T/shelves.log")" >&2; exit 1; }
    done
    echo "post-build: a game launched on each of the nine wrapped shelves ($k shelves)"
else echo "post-build: the wrapped shelves' launches skipped, no ui/banana-ui (make ui)"; fi

sed -i '/veto files/d' "$T/usr/libexec/banana/share"
! sh kiwami/post-build.sh "$T" rgds 2>/dev/null || { echo "post-build: no failure without the veto line" >&2; exit 1; }
rm -rf "$T"
echo "post-build: veto, 7z and the RomM wrapper checked"

# every board configures with the fork's tree and options over upstream's, keeping all of the fork's options
for b in rgds rgds_plus universal; do
    O=${TMPDIR:-/tmp}/kiwami-config-$b
    make -s -C "$BR" BR2_EXTERNAL="$PWD/buildroot:$PWD/kiwami" O="$O" banana_${b}_defconfig
    kiwami/configure.sh "$BR" "$O"
    echo "$b: configured"
done

# RetroArch's master as each board resolves it with the fork's options (the pin, the fork's hash file, patches and
# options), and upstream's v1.22.2 with the option off; first the guard, on copies of upstream's RetroArch (each move
# caught), then on upstream's own (it hasn't moved under the pin)
kiwami/tests/retroarch-pin.sh --self-test
kiwami/tests/retroarch-pin.sh "$BR" ${TMPDIR:-/tmp}/kiwami-config-rgds ${TMPDIR:-/tmp}/kiwami-config-rgds_plus \
    ${TMPDIR:-/tmp}/kiwami-config-universal
