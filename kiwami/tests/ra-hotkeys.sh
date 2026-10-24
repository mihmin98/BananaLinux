#!/bin/sh
# RetroArch master's four new hotkeys under Settings > Other systems > RetroArch hotkeys Off: kiwami/retroarch-hotkeys.sh
# on a made-up configuration.c (it writes them, and stops the build naming a hotkey no list covers, a name that isn't
# one, or one upstream's list has), and run-retroarch with its kiwami: hook in a made-up root, with a stand-in
# retroarch (kiwami/tests/bin/retroarch): with /usr/share/kiwami/ra-hotkeys the four are nul, out of the profiles' copy,
# and a bound one is kept and put back; without it, and with hotkeys On, what upstream's run-retroarch does, byte for
# byte. Run by kiwami/tests/ci.sh:
#   kiwami/tests/ra-hotkeys.sh
set -e
cd "$(dirname "$0")/../.."
RUN=system/rootfs/usr/libexec/banana/run-retroarch
HOOK='[ -f /usr/share/kiwami/ra-hotkeys ] && HOTKEYS="$HOTKEYS $(cat /usr/share/kiwami/ra-hotkeys)"	# kiwami: RetroArch master'"'"'s hotkeys too'
FOUR='video_filter_toggle headset_recenter laser_pointer_toggle headset_view_toggle'
X=$(mktemp -d "${TMPDIR:-/tmp}/kiwami-hk.XXXXXX")
n=0
fail() { echo "ra-hotkeys: FAIL: $*" >&2; [ -d "$X" ] && find "$X" -type f ! -path '*/udev/*' | sed 's/^/  file: /' >&2; exit 1; }
ok() { n=$((n + 1)); }

# the build script, on a configuration.c made of run-retroarch's list, the four, and the three Off keeps (exit_emulator
# twice, as master's #ifdef HAVE_LAKKA has it)
UP=$(sed -n '/^HOTKEYS="/,/"$/p' $RUN | sed 's/^HOTKEYS=//' | tr -d '"')
src() { for h in $UP $FOUR enable_hotkey menu_toggle exit_emulator exit_emulator "$@"; do
    printf '   DECLARE_META_BIND(2, %s,   RARCH_X,   MENU_ENUM_LABEL_VALUE_X),\n' "$h"; done > "$X/configuration.c"; }
hk() { kiwami/retroarch-hotkeys.sh "$X/configuration.c" $RUN "$X/out/ra-hotkeys" "$@" 2> "$X/err"; }
src; hk $FOUR || fail "the build script stopped on upstream's list and the four: $(cat "$X/err")"; ok
[ "$(cat "$X/out/ra-hotkeys")" = "$(printf '%s\n' $FOUR)" ] || fail "the out file is $(cat "$X/out/ra-hotkeys")"; ok
rm -rf "$X/out"; src rumble_toggle
! hk $FOUR || fail "no stop on master's rumble_toggle"; ok
grep -q 'rumble_toggle' "$X/err" || fail "the stop doesn't name rumble_toggle: $(cat "$X/err")"; ok
[ ! -e "$X/out/ra-hotkeys" ] || fail "a stopped run wrote the out file"; ok
src; sed -i '/ headset_recenter,/d' "$X/configuration.c"
! hk $FOUR || fail "no stop with headset_recenter gone from master"; ok
grep -q 'headset_recenter' "$X/err" || fail "the stop doesn't name headset_recenter: $(cat "$X/err")"; ok
src
! hk $FOUR reset || fail "no stop on reset, which run-retroarch lists"; ok
grep -q 'reset' "$X/err" || fail "the stop doesn't name reset: $(cat "$X/err")"; ok

# run-retroarch in a made-up root: /data, /run, /etc/banana and /usr under $X, in one pass (as in ra-keychain.sh); the
# hooked copy, and upstream's with the hook taken out
R=$X/root
rw() { sed "s#\(^\|[^A-Za-z0-9_.]\)/\(data\|run\|etc/banana\|usr\)/#\1$R/\2/#g"; }
rw < $RUN > "$X/hooked"
grep -vxF "$HOOK" $RUN | rw > "$X/upstream"
! grep -q 'kiwami/ra-hotkeys' "$X/upstream" || fail "upstream's copy of run-retroarch still has the hook"
CFG=$R/data/retroarch/retroarch.cfg
# root on|off [ra-hotkeys]: a fresh root, RetroArch hotkeys On or Off, the four's file when asked; the card's
# retroarch.cfg the image's template with one of the four and Save State bound in RetroArch's menu
root() {
    rm -rf "$R" "$X/seen"
    mkdir -p "$R/data/banana" "$R/data/retroarch" "$R/run/banana" "$R/etc/banana" "$R/usr/lib/libretro" \
        "$R/usr/share/libretro/autoconfig/udev" "$R/usr/libexec/banana"
    cp system/rootfs/etc/banana/retroarch.cfg system/rootfs/etc/banana/settings.conf "$R/etc/banana/"
    echo "ra_hotkeys = $1" > "$R/data/banana/settings.conf"
    : > "$R/usr/lib/libretro/fceumm_libretro.so"
    printf 'input_a_btn = "0"\ninput_enable_hotkey_btn = "9"\ninput_save_state_btn = "5"\ninput_video_filter_toggle_btn = "3"\n' \
        > "$R/usr/share/libretro/autoconfig/udev/pad.cfg"
    rw < system/rootfs/usr/libexec/banana/ra-secrets > "$R/usr/libexec/banana/ra-secrets"
    chmod 755 "$R/usr/libexec/banana/ra-secrets"
    printf 'one_panel_dark() { :; }\none_panel_restore() { :; }\n' > "$R/usr/libexec/banana/one-panel"
    { cat system/rootfs/etc/banana/retroarch.cfg; printf 'input_video_filter_toggle_btn = "2"\ninput_save_state_btn = "4"\n'; } > "$CFG"
    if [ "$2" = ra-hotkeys ]; then mkdir -p "$R/usr/share/kiwami"; printf '%s\n' $FOUR > "$R/usr/share/kiwami/ra-hotkeys"; fi
}
# play <copy>: a game, Second screen off (no BANANA_DRM_LEASE_FD); what the stand-in saw in $X/seen
play() {
    env -u BANANA_DRM_LEASE_FD -u BANANA_SYSTEM PATH="$PWD/kiwami/tests/bin:$PATH" KR_SEEN="$X/seen" \
        sh "$X/$1" fceumm /x/game.nes 2> "$X/err" || fail "$1: run-retroarch exited $?: $(cat "$X/err")"
}
has() { grep -qxF "$2" "$1" || fail "no '$2' in ${1#$X/}"; }
hasnt() { ! grep -q "$2" "$1" || fail "'$2' in ${1#$X/}: $(grep "$2" "$1")"; }
once() { [ "$(grep -cxF "$2" "$1")" = 1 ] || fail "'$2' is $(grep -cxF "$2" "$1") times in ${1#$X/}"; }

# Off, with the four's file: each of them nul, out of the profiles' copy; the bound one kept during the game and put
# back once after it (A9)
root off ra-hotkeys; play hooked
S=$X/seen
for h in $FOUR; do has "$S/retroarch-hotkeys.cfg" "input_${h}_btn = \"nul\""; has "$S/retroarch-hotkeys.cfg" "input_${h}_axis = \"nul\""; done; ok
hasnt "$S/autoconfig/udev/pad.cfg" '^input_video_filter_toggle_btn'; hasnt "$S/autoconfig/udev/pad.cfg" '^input_save_state_btn'; ok
has "$S/autoconfig/udev/pad.cfg" 'input_a_btn = "0"'; has "$S/autoconfig/udev/pad.cfg" 'input_enable_hotkey_btn = "9"'; ok
has "$S/keep" 'input_video_filter_toggle_btn = "2"'; ok
once "$CFG" 'input_video_filter_toggle_btn = "2"'; once "$CFG" 'input_save_state_btn = "4"'; ok
hasnt "$CFG" '^input_\(video_filter_toggle\|save_state\)_btn = "nul"'; ok
hasnt "$CFG" '^joypad_autoconfig_dir = ".*/run/'; ok
[ ! -e "$R/data/retroarch/.banana-hotkeys.keep" ] || fail "the kept lines left on the card"; ok

# Off, without the file: what upstream's run-retroarch does, byte for byte, and no line for the four (A9)
for c in hooked upstream; do
    root off; play $c
    cp "$CFG" "$X/cfg.$c"; rm -rf "$X/seen.$c"; mv "$X/seen" "$X/seen.$c"
done
cmp -s "$X/cfg.hooked" "$X/cfg.upstream" || fail "without the file, retroarch.cfg differs from upstream's run"; ok
diff -r "$X/seen.hooked" "$X/seen.upstream" > /dev/null || fail "without the file, RetroArch was handed other configs or profiles than by upstream's"; ok
hasnt "$X/seen.hooked/retroarch-hotkeys.cfg" 'video_filter_toggle'; ok

# On, with the file: no hotkeys config, and retroarch.cfg as upstream's run leaves it
root on ra-hotkeys; play hooked; cp "$CFG" "$X/cfg.on"
[ ! -e "$X/seen/retroarch-hotkeys.cfg" ] || fail "hotkeys On: a hotkeys config was appended"; ok
root on; play upstream
cmp -s "$X/cfg.on" "$CFG" || fail "hotkeys On: retroarch.cfg differs from upstream's run"; ok

# the hook: the line after the HOTKEYS list's closing line
[ "$(grep -A1 '^toggle_statistics .* volume_up"$' $RUN | tail -n 1)" = "$HOOK" ] || fail "run-retroarch has no ra-hotkeys hook after the HOTKEYS list"; ok

rm -rf "$X"
echo "ra-hotkeys: all $n checks passed"
