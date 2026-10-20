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

# banana-ui's code of the fork's that needs no screen (the JSON reader, Tailscale's status, the name, the QR code),
# with the host's g++
T=$(mktemp -d)
${CXX:-g++} -std=c++17 -Wall -Werror -I ui/src -o "$T/units" kiwami/tests/units.cpp ui/src/kiwami/json.cpp ui/src/kiwami/tsstatus.cpp \
    ui/src/kiwami/qrcodegen.cpp
"$T/units" kiwami/tests/tailscale
rm -rf "$T"

# Tailscale in banana-ui's Settings, headless, when banana-ui is built (CI builds it before)
if [ -x ui/banana-ui ]; then kiwami/tests/tailscale-ui.sh; else echo "tailscale-ui: skipped, no ui/banana-ui (make ui)"; fi

# post-build.sh vetoes tailscaled's state in upstream's share script, and stops the build when the line isn't there
T=$(mktemp -d)
mkdir -p "$T/usr/libexec/banana"
cp system/rootfs/usr/libexec/banana/share "$T/usr/libexec/banana/"
sh kiwami/post-build.sh "$T" rgds
grep -q '^	veto files = /.*/\*tailscaled\.state/$' "$T/usr/libexec/banana/share" || { echo "post-build: no veto for tailscaled.state" >&2; exit 1; }
sed -i '/veto files/d' "$T/usr/libexec/banana/share"
! sh kiwami/post-build.sh "$T" rgds 2>/dev/null || { echo "post-build: no failure without the veto line" >&2; exit 1; }
rm -rf "$T"
echo "post-build: veto checked"

# every board configures with the fork's tree and options over upstream's, keeping all of the fork's options
for b in rgds rgds_plus universal; do
    O=${TMPDIR:-/tmp}/kiwami-config-$b
    make -s -C "$BR" BR2_EXTERNAL="$PWD/buildroot:$PWD/kiwami" O="$O" banana_${b}_defconfig
    kiwami/configure.sh "$BR" "$O"
    echo "$b: configured"
done
