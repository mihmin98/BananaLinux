#!/bin/sh
# kiwami/build.sh, the top Makefile's board build, against made-up outputs (a .config, a package's stamp, per-package/)
# and a stand-in make that prints its arguments: per-package directories get -j<nproc> only when make has no -j and
# --output-sync=target only when it has no -O; KIWAMI_SERIAL=1 adds nothing; the four stops (the switch against the
# .config, the build against the .config) each run nothing and say what to run; a bad KIWAMI_SERIAL stops. Run by
# kiwami/tests/ci.sh:
#   kiwami/tests/build-mode.sh
set -e
cd "$(dirname "$0")/../.."
X=$(mktemp -d "${TMPDIR:-/tmp}/kiwami-bm.XXXXXX")
trap 'rm -rf "$X"' EXIT
n=0
fail() { echo "build-mode: FAIL: $*" >&2; exit 1; }
ok() { n=$((n + 1)); }
printf '#!/bin/sh\necho "$*" > "%s/ran"\n' "$X" > "$X/make"
chmod +x "$X/make"
O=$X/out/rgds-plus
# out <ppd|serial> [built] [perpkg]: a made-up output in that state
out() {
    rm -rf "$X/out" "$X/ran"
    mkdir -p "$O"
    if [ "$1" = ppd ]; then echo 'BR2_PER_PACKAGE_DIRECTORIES=y' > "$O/.config"
    else echo '# BR2_PER_PACKAGE_DIRECTORIES is not set' > "$O/.config"; fi
    shift
    for s; do
        case $s in
        built) mkdir -p "$O/build/host-skeleton" && touch "$O/build/host-skeleton/.stamp_configured" ;;
        perpkg) mkdir -p "$O/per-package/host-skeleton" ;;
        esac
    done
}
# runs <serial> <makeflags> <expected arguments>: build.sh runs make with them
runs() {
    KIWAMI_SERIAL=$1 MAKEFLAGS=$2 kiwami/build.sh "$O" "$X/make" > "$X/out.txt" 2> "$X/err" ||
        fail "exited $?: $(cat "$X/err")"
    ran=$(cat "$X/ran" 2>/dev/null || true)
    [ "$ran" = "$3" ] || fail "KIWAMI_SERIAL='$1' MAKEFLAGS='$2': make ran with '$ran', not '$3'"
    ok
}
# said <line>: what the last runs printed
said() { [ "$(cat "$X/out.txt")" = "$1" ] || fail "said '$(cat "$X/out.txt")', not '$1'"; ok; }
# stops <serial> <words of the message>: build.sh ($B, kiwami/build.sh by default) exits 1 with them, and make doesn't
# run
B=kiwami/build.sh
stops() {
    s=$1; shift
    ! KIWAMI_SERIAL=$s MAKEFLAGS= "$B" "$O" "$X/make" > /dev/null 2> "$X/err" || fail "KIWAMI_SERIAL='$s': didn't stop"
    [ ! -e "$X/ran" ] || fail "KIWAMI_SERIAL='$s': make ran"
    for w; do
        grep -qF -- "$w" "$X/err" || fail "KIWAMI_SERIAL='$s': the message doesn't say '$w': $(cat "$X/err")"
    done
    ok
}

J=-j$(nproc)
out ppd
runs '' '' "$J --output-sync=target -C $O"
said "build.sh: $O: per-package directories, $J, output per step"
out ppd built perpkg
runs '' '' "$J --output-sync=target -C $O"
# make's own -j (a jobserver), make's own -O, -s and a variable in the flags
runs '' 's -j4 --jobserver-auth=fifo:/x' "--output-sync=target -C $O"
said "build.sh: $O: per-package directories, make's -j4, output per step"
runs '' ' -j' "--output-sync=target -C $O"
runs '' ' -j16 -Otarget --jobserver-auth=fifo:/x' "-C $O"
runs '' ' -Oline' "$J -C $O"
runs '' ' --no-print-directory -- V=-j9' "$J --output-sync=target -C $O"
# KIWAMI_SERIAL=1 on an output configured that way, fresh or built: nothing added, whatever make has
out serial
runs 1 '' "-C $O"
said "build.sh: $O: as upstream (KIWAMI_SERIAL=1)"
out serial built
runs 1 ' -j16 --jobserver-auth=fifo:/x' "-C $O"

# the switch against the .config
out ppd
stops 1 "is configured with per-package directories" "rm $O/.config"
out serial
stops '' "is configured without per-package directories" "KIWAMI_SERIAL=1" "rm $O/.config"
# the build against the .config, in an output outside the checkout's output/ (OUT= named)
out ppd built
stops '' "holds a build made without per-package directories" \
    "make OUT=$X/out clean-rgds-plus, then make OUT=$X/out rgds-plus" \
    "rm $O/.config, then make OUT=$X/out KIWAMI_SERIAL=1 rgds-plus"
out serial built perpkg
stops 1 "holds a build made with per-package directories" \
    "make OUT=$X/out clean-rgds-plus, then make OUT=$X/out KIWAMI_SERIAL=1 rgds-plus"
# ... and in the checkout's own (a copy of build.sh in a made-up checkout), as today's output/rgds-plus stops
O=$X/co/output/rgds-plus
mkdir -p "$X/co/kiwami" && cp kiwami/build.sh "$X/co/kiwami/"
rm -rf "$X/co/output" && mkdir -p "$O/build/linux-7.2.7" && touch "$O/build/linux-7.2.7/.stamp_configured"
echo 'BR2_PER_PACKAGE_DIRECTORIES=y' > "$O/.config"
B=$X/co/kiwami/build.sh
stops '' "make clean-rgds-plus, then make rgds-plus"
B=kiwami/build.sh O=$X/out/rgds-plus
# a bad switch
out ppd
stops yes "KIWAMI_SERIAL is 1 or unset, not 'yes'"
echo "build-mode: $n checks passed"
