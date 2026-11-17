#!/bin/sh
# Power scaling's launch script (kiwami/rootfs/usr/libexec/kiwami/clocks), sourced as run-retroarch sources it, with a
# stand-in setting() over a made-up settings.conf, a made-up devfreq folder, mode file and logs' folder:
# - off, and no key: nothing exported, the GPU's files and the mode file untouched, nothing said;
# - on: the four variables, the GPU on demand from the lowest of its (unsorted) clocks, the last log kept as .1, the
#   mode file, the line for banana-ui's log; no kc_* left behind, run-retroarch's own variables kept;
# - on for the N64: as off;
# - on with no GPU devfreq: the rest still done;
# - run-retroarch's hook: the line once, after setting()'s block and before the Frontend's dispatch.
# Run by kiwami/tests/ci.sh:
#   kiwami/tests/clocks.sh
set -e
cd "$(dirname "$0")/../.."
H=$PWD/kiwami/rootfs/usr/libexec/kiwami/clocks
R=$PWD/system/rootfs/usr/libexec/banana/run-retroarch
X=$(mktemp -d "${TMPDIR:-/tmp}/kiwami-clocks.XXXXXX")
n=0
fail() { echo "clocks: FAIL: $*" >&2; exit 1; }
ok() { n=$((n + 1)); }
G=$X/devfreq/fde60000.gpu
# fresh: the GPU as banana-perf's profile left it, an old log, no mode file
fresh() {
    rm -rf "$X"/*
    mkdir -p "$G" "$X/logs"
    echo performance > "$G/governor"
    echo 600000000 > "$G/min_freq"
    echo '400000000 200000000 800000000 300000000 ' > "$G/available_frequencies"
    echo old > "$X/logs/kiwami-clocks.log"
}
# src <settings.conf> <system>: the script sourced in a subshell, as run-retroarch does; its environment after it in
# $X/env, its stderr in $X/err
src() {
    printf '%s' "$1" > "$X/settings.conf"
    (
        unset KIWAMI_CLOCKS BANANAFRONT_CPUGOV KIWAMI_CLOCKS_DATA KIWAMI_CLOCKS_LOG
        export KIWAMI_DEVFREQ_ROOT=$X/devfreq KIWAMI_CLOCKS_MODE=$X/run/kiwami-clocks KIWAMI_CLOCKS_LOGS=$X/logs
        BANANA_SYSTEM=$2 CORE=/usr/lib/libretro/x_libretro.so v=kept c=kept
        setting() { sed -n "/^ *$1 *= */{s///;h};\${x;p}" "$X/settings.conf"; }
        . "$H"
        { env | grep -E '^(KIWAMI_CLOCKS|BANANAFRONT_CPUGOV|KIWAMI_CLOCKS_DATA|KIWAMI_CLOCKS_LOG)=' | sort
          set | grep '^kc_' || true
          echo "CORE=$CORE v=$v c=$c"; } > "$X/env"
    ) 2> "$X/err"
}
has() { grep -qxF "$1" "$X/env" || fail "$2: no '$1' in the environment: $(tr '\n' ' ' < "$X/env")"; }
untouched() {
    [ "$(grep -c '^KIWAMI\|^BANANAFRONT' "$X/env")" = 0 ] || fail "$1: exported $(grep '^KIWAMI\|^BANANAFRONT' "$X/env" | tr '\n' ' ')"; ok
    [ "$(cat "$G/governor") $(cat "$G/min_freq")" = "performance 600000000" ] || fail "$1: the GPU is $(cat "$G/governor") $(cat "$G/min_freq")"; ok
    [ ! -e "$X/run/kiwami-clocks" ] || fail "$1: a mode file"; ok
    [ "$(cat "$X/logs/kiwami-clocks.log")" = old ] && [ ! -e "$X/logs/kiwami-clocks.1.log" ] || fail "$1: the logs moved"; ok
    [ ! -s "$X/err" ] || fail "$1: said '$(cat "$X/err")'"; ok
}

# off, no key, a word that isn't on: nothing
for conf in 'kiwami_power_scaling = off\n' '' 'kiwami_power_scaling = yes\n'; do
    fresh; src "$(printf "$conf")" gba; untouched "'$conf'"
done

# on: everything
fresh; src "$(printf 'wifi = on\nkiwami_power_scaling = on\n')" gba
has KIWAMI_CLOCKS=1 on; has BANANAFRONT_CPUGOV=1 on; has KIWAMI_CLOCKS_DATA=/data/banana/kiwami-clocks on
has "KIWAMI_CLOCKS_LOG=$X/logs/kiwami-clocks.log" on; ok
[ "$(cat "$G/governor")" = simple_ondemand ] || fail "on: the GPU's governor is $(cat "$G/governor")"; ok
[ "$(cat "$G/min_freq")" = 200000000 ] || fail "on: the GPU's floor is $(cat "$G/min_freq"), not the lowest"; ok
[ "$(cat "$X/logs/kiwami-clocks.1.log")" = old ] && [ ! -e "$X/logs/kiwami-clocks.log" ] || fail "on: the old log isn't kiwami-clocks.1.log"; ok
[ "$(cat "$X/run/kiwami-clocks")" = auto ] || fail "on: the mode file says '$(cat "$X/run/kiwami-clocks" 2>/dev/null)'"; ok
[ "$(cat "$X/err")" = "kiwami: clocks: on (gba)" ] || fail "on: said '$(cat "$X/err")'"; ok
! grep -q '^kc_' "$X/env" || fail "on: left $(grep '^kc_' "$X/env" | tr '\n' ' ')"; ok
has "CORE=/usr/lib/libretro/x_libretro.so v=kept c=kept" on; ok
# ... and the image's default (no key in settings.conf, the template off) is off, as above; the template has none
! grep -q kiwami_power_scaling system/rootfs/etc/banana/settings.conf || fail "upstream's settings.conf names kiwami_power_scaling"; ok

# on for the N64: nothing
fresh; src "$(printf 'kiwami_power_scaling = on\n')" n64; untouched n64

# on with no GPU devfreq: the rest
fresh; rm -rf "$X/devfreq"; src "$(printf 'kiwami_power_scaling = on\n')" snes
has KIWAMI_CLOCKS=1 "no devfreq"; ok
[ "$(cat "$X/run/kiwami-clocks")" = auto ] || fail "no devfreq: no mode file"; ok
[ "$(cat "$X/err")" = "kiwami: clocks: on (snes)" ] || fail "no devfreq: said '$(cat "$X/err")'"; ok

# run-retroarch's hook: once, right after setting()'s closing brace, before the Frontend's case
HOOK='[ -f /usr/libexec/kiwami/clocks ] && . /usr/libexec/kiwami/clocks	# kiwami: Power scaling (RetroArch'"'"'s and BananaFront'"'"'s clock governors)'
[ "$(grep -cxF "$HOOK" "$R")" = 1 ] || fail "run-retroarch has the hook $(grep -cxF "$HOOK" "$R") times"; ok
at=$(grep -nxF "$HOOK" "$R" | cut -d: -f1)
s=$(grep -n '^setting() {$' "$R" | head -n 1 | cut -d: -f1)
b=$(awk -v s="$s" 'NR > s && /^}$/ { print NR; exit }' "$R")
c=$(grep -n '^case \$BANANA_SYSTEM in$' "$R" | head -n 1 | cut -d: -f1)
[ -n "$s" ] && [ -n "$b" ] && [ -n "$c" ] && [ "$at" -gt "$b" ] && [ "$at" -lt "$c" ] ||
    fail "run-retroarch's hook is at line $at, not between setting()'s end ($b) and the Frontend's case ($c)"; ok

rm -rf "$X"
echo "clocks: all $n checks passed"
