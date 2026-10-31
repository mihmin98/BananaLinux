#!/bin/sh
# The fork's Buildroot post-build, run after upstream's (buildroot/board/rgds/post-build.sh) and after both rootfs
# overlays have been copied: changes to upstream's files in the image go here rather than a whole copy in the overlay.
#   post-build.sh <target dir> <board: rgds | rgds-plus | universal>
# (BR2_CONFIG, the board's .config, as Buildroot gives it to post-build scripts: the fork's options)
set -e
T=$1

# File sharing: the fork's secrets neither listed nor opened over SMB, as the share script keeps upstream's own:
# tailscaled's state (the handheld's keys in the tailnet), and RetroArch master's credentials file
# (retroarch-keychain.cfg, the RetroAchievements password and token from RetroArch's exit until the next scrub folds
# it into retroarch.cfg, and the .key a sealed build would leave). Upstream's veto line is matched exactly: if it
# moves or changes shape, the build stops here rather than sharing them.
S=$T/usr/libexec/banana/share
V='^	veto files = /.*/$'
[ "$(grep -c "$V" "$S")" = 1 ] || { echo "kiwami/post-build.sh: no single 'veto files' line in $S" >&2; exit 1; }
# veto <name> <its regex>: the name appended to upstream's line when the line doesn't have it yet, then looked for
# anywhere on it (the fork's names follow upstream's)
veto() {
    grep -q "^	veto files = /.*/$2/" "$S" || sed -i "/^	veto files = /s|\$|$1/|" "$S"
    grep -q "^	veto files = /.*/$2/" "$S" || { echo "kiwami/post-build.sh: $1 not vetoed in $S" >&2; exit 1; }
}
veto '*tailscaled.state' '\*tailscaled\.state'
veto '*retroarch-keychain.*' '\*retroarch-keychain\.\*'

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

# Over-the-air updates off (BR2_PACKAGE_KIWAMI_OTA_OFF): the update script's check, download and beta-login go to the
# fork's ota-off, which says why, through a first case arm before upstream's own. Upstream's "case $1 in" line is
# matched exactly: if it moves or changes shape, the build stops here rather than offering upstream's releases.
if [ -n "$BR2_CONFIG" ] && grep -qx 'BR2_PACKAGE_KIWAMI_OTA_OFF=y' "$BR2_CONFIG"; then
    U=$T/usr/libexec/banana/update
    [ -x "$T/usr/libexec/kiwami/ota-off" ] || { echo "kiwami/post-build.sh: no usr/libexec/kiwami/ota-off in the target" >&2; exit 1; }
    [ "$(grep -c '^case \$1 in$' "$U")" = 1 ] || { echo "kiwami/post-build.sh: no single 'case \$1 in' line in $U" >&2; exit 1; }
    grep -q '^check|download|beta-login) exec /usr/libexec/kiwami/ota-off ' "$U" ||
        sed -i '/^case \$1 in$/a check|download|beta-login) exec /usr/libexec/kiwami/ota-off "$@" ;;\t# kiwami: over-the-air updates off' "$U"
    grep -q '^check|download|beta-login) exec /usr/libexec/kiwami/ota-off ' "$U" || { echo "kiwami/post-build.sh: ota-off not hooked into $U" >&2; exit 1; }
fi
