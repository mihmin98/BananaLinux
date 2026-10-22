#!/bin/sh
# The fork's cores' RetroArch overrides (kiwami/rootfs/usr/libexec/kiwami/core-overrides) in a made-up RetroArch
# config folder: Mesen's and SameBoy's override files get video_threaded once, a value already there and the files'
# other lines kept, no other core touched; and run-retroarch calls it. Run by kiwami/tests/ci.sh:
#   kiwami/tests/core-overrides.sh
set -e
cd "$(dirname "$0")/../.."
H=$PWD/kiwami/rootfs/usr/libexec/kiwami/core-overrides
X=$(mktemp -d "${TMPDIR:-/tmp}/kiwami-co.XXXXXX")
export KIWAMI_RA_CONFIG=$X/config KIWAMI_RA_CFG=$X/retroarch.cfg
C=$KIWAMI_RA_CONFIG
n=0
fail() { echo "core-overrides: FAIL: $*" >&2; [ -d "$X" ] && find "$X" -type f | sed 's/^/  file: /' >&2; exit 1; }
ok() { n=$((n + 1)); }
fresh() { rm -rf "$X"/*; mkdir -p "$X"; }
# run <core>: the script, its stderr in $X/err
run() { "$H" "$1" 2> "$X/err" || fail "$1: exited $?"; }
LINE='video_threaded = "true"'

for core in mesen:Mesen sameboy:SameBoy; do
    c=${core%:*} name=${core#*:} F=$C/$name/$name.cfg
    # an empty folder: the file made with the line alone, and a line in the log
    fresh; run $c
    [ "$(cat "$F")" = "$LINE" ] || fail "$c: $F is '$(cat "$F" 2>/dev/null)'"; ok
    [ "$(cat "$X/err")" = "kiwami: core-overrides: $c draws on its own thread ($F)" ] || fail "$c: logged '$(cat "$X/err")'"; ok
    # again: nothing changes, nothing logged
    cp "$F" "$X/once"; run $c
    cmp -s "$F" "$X/once" || fail "$c: a second run changed $F"; ok
    [ ! -s "$X/err" ] || fail "$c: a second run logged '$(cat "$X/err")'"; ok
    # the user's own value: kept as it is
    fresh; mkdir -p "${F%/*}"; printf 'video_threaded = "false"\n' > "$F"; run $c
    [ "$(cat "$F")" = 'video_threaded = "false"' ] || fail "$c: the user's value became '$(cat "$F")'"; ok
    [ ! -s "$X/err" ] || fail "$c: logged '$(cat "$X/err")' over the user's value"; ok
    # an override with other lines: the line appended, the others kept, in order
    fresh; mkdir -p "${F%/*}"; printf 'video_shader_enable = "true"\naudio_latency = "96"\n' > "$F"; run $c
    [ "$(cat "$F")" = "$(printf 'video_shader_enable = "true"\naudio_latency = "96"\n%s' "$LINE")" ] || fail "$c: other lines: $(cat "$F")"; ok
    # ...and its last line with no newline: not glued onto it
    fresh; mkdir -p "${F%/*}"; printf 'audio_latency = "96"' > "$F"; run $c
    [ "$(cat "$F")" = "$(printf 'audio_latency = "96"\n%s' "$LINE")" ] || fail "$c: no final newline: $(cat "$F")"; ok
done

# no other core: nothing made, nothing logged
fresh
for c in genesis_plus_gx fceumm gambatte picodrive gpsp mgba snes9x pcsx_rearmed mupen64plus_next ""; do
    run "$c"
    [ ! -s "$X/err" ] || fail "'$c' logged '$(cat "$X/err")'"; ok
done
[ ! -e "$C" ] || fail "other cores made $(find "$C")"; ok

# the folder: the card's rgui_config_directory when absolute, else Banana's /data/retroarch/config
unset KIWAMI_RA_CONFIG
printf 'video_driver = "gl"\nrgui_config_directory = "%s"\n' "$X/elsewhere" > "$KIWAMI_RA_CFG"; run mesen
[ "$(cat "$X/elsewhere/Mesen/Mesen.cfg")" = "$LINE" ] || fail "rgui_config_directory not followed: $(cat "$X/err")"; ok
for d in '' 'rgui_config_directory = ":/config"' 'rgui_config_directory = "~/config"'; do
    printf '%s\n' "$d" > "$KIWAMI_RA_CFG"
    # (only the folder it would use is checked, through its dir(): /data isn't written by the test)
    got=$(sh -c "$(sed -n '/^dir()/,/^}/p' "$H"); CFG=\$KIWAMI_RA_CFG; dir")
    [ "$got" = /data/retroarch/config ] || fail "with '$d' the folder is '$got', not /data/retroarch/config"; ok
done
rm -f "$KIWAMI_RA_CFG"
got=$(KIWAMI_RA_CFG=$X/none sh -c "$(sed -n '/^dir()/,/^}/p' "$H"); CFG=\$KIWAMI_RA_CFG; dir")
[ "$got" = /data/retroarch/config ] || fail "with no retroarch.cfg the folder is '$got'"; ok

# run-retroarch calls it with the core that runs, right after saying which one that is
R=system/rootfs/usr/libexec/banana/run-retroarch
grep -A1 '^\[ "\$c" != "\$1" \] && echo "run-retroarch: core' "$R" | tail -n 1 |
    grep -qxF '[ -x /usr/libexec/kiwami/core-overrides ] && /usr/libexec/kiwami/core-overrides "$c"	# kiwami: the fork'"'"'s cores'"'"' RetroArch settings' ||
    fail "run-retroarch has no core-overrides hook after the core's log line"; ok

rm -rf "$X"
echo "core-overrides: all $n checks passed"
