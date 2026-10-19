#!/bin/sh
# The fork's Buildroot post-build, run after upstream's (buildroot/board/rgds/post-build.sh) and after both rootfs
# overlays have been copied: changes to upstream's files in the image go here rather than a whole copy in the overlay.
#   post-build.sh <target dir> <board: rgds | rgds-plus | universal>
set -e
T=$1

# File sharing: tailscaled's state (the handheld's keys in the tailnet) neither listed nor opened over SMB, as the share
# script keeps upstream's own secrets. Upstream's veto line is matched exactly: if it moves or changes shape, the build
# stops here rather than sharing the state.
S=$T/usr/libexec/banana/share
V='^	veto files = /.*/$'
[ "$(grep -c "$V" "$S")" = 1 ] || { echo "kiwami/post-build.sh: no single 'veto files' line in $S" >&2; exit 1; }
grep -q '^	veto files = .*/\*tailscaled\.state/$' "$S" || sed -i '/^	veto files = /s|$|*tailscaled.state/|' "$S"
grep -q '^	veto files = /.*/\*tailscaled\.state/$' "$S" || { echo "kiwami/post-build.sh: tailscaled.state not vetoed in $S" >&2; exit 1; }
