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

# 7z ROMs: the cartridge systems also list .7z games, which RetroArch (built with 7z support) unpacks when the game
# starts, as it does .zip ones. A chosen system with no ext line stops the build rather than leaving it out; a line
# that already lists 7z is left as it is. Written through a temporary, so a failed run leaves the file as it was.
C=$T/etc/banana/systems.conf
awk -v want="gba gb gbc nes snes megadrive n64" '
    BEGIN { n = split(want, w, " "); for (i = 1; i <= n; i++) need[w[i]] = 1 }
    /^\[.*\]$/ { sec = substr($0, 2, length($0) - 2) }
    (sec in need) && /^ext *=/ { if ($0 !~ /[ =]7z( |$)/) $0 = $0 " 7z"; has[sec] = 1 }
    { print }
    END { for (s in need) if (!(s in has)) { print "kiwami/post-build.sh: no ext line for [" s "] in " FILENAME > "/dev/stderr"; bad = 1 }
          exit bad }' "$C" > "$C.tmp" || { rm -f "$C.tmp"; exit 1; }
mv "$C.tmp" "$C"
