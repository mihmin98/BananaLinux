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

# banana-ui's code of the fork's that needs no screen (the JSON reader, Tailscale's status, the name, the QR code,
# Settings' grid of groups, RetroAchievements' replies and its account on the card), with the host's g++
T=$(mktemp -d)
${CXX:-g++} -std=c++17 -Wall -Werror -I ui/src -o "$T/units" kiwami/tests/units.cpp ui/src/kiwami/json.cpp ui/src/kiwami/tsstatus.cpp \
    ui/src/kiwami/qrcodegen.cpp ui/src/kiwami/grid.cpp ui/src/kiwami/raclient.cpp ui/src/kiwami/rastore.cpp -ldl
"$T/units" kiwami/tests/tailscale kiwami/tests/ra
# the stand-in RetroAchievements server the UI tests run against (its bytecode in $T, not beside it)
PYTHONPYCACHEPREFIX="$T" python3 -m py_compile kiwami/tests/ra-server.py
rm -rf "$T"

# Tailscale in banana-ui's Settings, headless, when banana-ui is built (CI builds it before)
if [ -x ui/banana-ui ]; then kiwami/tests/tailscale-ui.sh; else echo "tailscale-ui: skipped, no ui/banana-ui (make ui)"; fi

# .7z games on the cartridge systems' shelves, headless, when banana-ui is built
if [ -x ui/banana-ui ]; then kiwami/tests/7z-ui.sh; else echo "7z-ui: skipped, no ui/banana-ui (make ui)"; fi

# the fork's cores in Settings > Other systems, headless, when banana-ui is built
if [ -x ui/banana-ui ]; then kiwami/tests/cores-ui.sh; else echo "cores-ui: skipped, no ui/banana-ui (make ui)"; fi

# the RetroAchievements group in Settings and the grid scrolling to it, headless, when banana-ui is built
if [ -x ui/banana-ui ]; then kiwami/tests/ra-ui.sh; else echo "ra-ui: skipped, no ui/banana-ui (make ui)"; fi

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

sed -i '/veto files/d' "$T/usr/libexec/banana/share"
! sh kiwami/post-build.sh "$T" rgds 2>/dev/null || { echo "post-build: no failure without the veto line" >&2; exit 1; }
rm -rf "$T"
echo "post-build: veto and 7z checked"

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
