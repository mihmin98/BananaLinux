#!/bin/sh
# A board's build, run by the top Makefile's board rule (its kiwami: line): Buildroot in the mode the output was
# configured in.
#   build.sh <output dir> <make command...>
# With per-package directories (kiwami.fragment), packages that don't depend on each other build at once: -j$(nproc)
# when make wasn't given a -j (Buildroot's top level builds one package at a time otherwise), and each step's output
# held until it ends (--output-sync=target), so that several packages' lines don't interleave. Configured with
# KIWAMI_SERIAL=1 (kiwami/configure.sh), the output builds as upstream's and nothing is added.
# Buildroot can't switch an existing output, so this stops before building when KIWAMI_SERIAL and the output's
# .config disagree, or when the output holds a build made the other way, and says what to run.
set -e
O=$1
shift
B=$(basename "$O")
# make's OUT= when the output isn't in the checkout's output/
P=$(cd "$O/.." && pwd)
[ "$P" = "$(cd "$(dirname "$0")/.." && pwd)/output" ] && M=make || M="make OUT=$P"
stop() { echo "build.sh: $O $*" >&2; exit 1; }

case ${KIWAMI_SERIAL-} in
''|1) ;;
*) stop "not built: KIWAMI_SERIAL is 1 or unset, not '$KIWAMI_SERIAL'" ;;
esac
grep -qsx 'BR2_PER_PACKAGE_DIRECTORIES=y' "$O/.config" && ppd=1 || ppd=
built=
for s in "$O"/build/*/.stamp_configured; do [ -e "$s" ] && built=1 && break; done
[ -d "$O/per-package" ] && perpkg=1 || perpkg=

if [ "${KIWAMI_SERIAL-}" = 1 ] && [ "$ppd" ]; then
    stop "is configured with per-package directories, and KIWAMI_SERIAL=1 applies when an output is configured:" \
        "build it without KIWAMI_SERIAL, or rm $O/.config to configure it again"
elif [ "${KIWAMI_SERIAL-}" != 1 ] && [ ! "$ppd" ]; then
    stop "is configured without per-package directories (KIWAMI_SERIAL=1): build it with KIWAMI_SERIAL=1," \
        "or rm $O/.config to configure it again"
elif [ "$ppd" ] && [ "$built" ] && [ ! "$perpkg" ]; then
    stop "holds a build made without per-package directories: $M clean-$B, then $M $B; or, to build it as it was," \
        "rm $O/.config, then $M KIWAMI_SERIAL=1 $B"
elif [ ! "$ppd" ] && [ "$perpkg" ]; then
    stop "holds a build made with per-package directories: $M clean-$B, then $M KIWAMI_SERIAL=1 $B; or, to build" \
        "it as it was, rm $O/.config, then $M $B"
fi

if [ ! "$ppd" ]; then
    echo "build.sh: $O: as upstream (KIWAMI_SERIAL=1)"
    exec "$@" -C "$O"
fi
# make's own flags: the single letters, then the options (the variables after " -- " left out)
F=" ${MAKEFLAGS-} "
F=${F%% -- *}
case $F in
*" -j"*|*jobserver*) J= j=$(echo "$F" | grep -o -- ' -j[0-9]*' | head -n 1 | tr -d ' '); jobs="make's ${j:-jobs}" ;;
*) J=-j$(nproc) jobs=$J ;;
esac
case $F in
*" -O"*|*output-sync*) S= ;;
*) S=--output-sync=target ;;
esac
echo "build.sh: $O: per-package directories, $jobs, output per step"
exec "$@" $J $S -C "$O"
