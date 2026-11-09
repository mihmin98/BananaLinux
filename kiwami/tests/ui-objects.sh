#!/bin/sh
# banana-ui built from objects, one file per call (ui/src/kiwami/kiwami.mk), is byte for byte the binary upstream's
# rule makes in one g++ call, unstripped and stripped. Both are built in one copy of ui/, one after the other, so the
# paths compiled into them are the same.
#   kiwami/tests/ui-objects.sh                this machine's g++, ALSA=0 as make ui builds it; then, on the objects,
#                                             what a change rebuilds (a .cpp, a header, the flags), the -j make is
#                                             given without one, and make clean
#   kiwami/tests/ui-objects.sh <output dir>   a Buildroot output's target toolchain, with banana-ui.mk's command
#                                             (make -B, its CXXFLAGS and LDFLAGS) and Buildroot's strip
set -e
cd "$(dirname "$0")/../.."
fail() { echo "ui-objects: $*" >&2; exit 1; }
# (no make above this one: the -j check needs make without one; ccache off, so each build really compiles)
unset MAKEFLAGS MFLAGS MAKELEVEL
export CCACHE_DISABLE=1

T=$(mktemp -d)
trap 'rm -rf "$T"' EXIT
# (ui/ but its tests and assets, which the build doesn't read)
mkdir "$T/ui"
for f in ui/*; do
    case $f in ui/tests|ui/assets) ;; *) cp -a "$f" "$T/ui/" ;; esac
done
U=$T/ui

if [ -n "$1" ]; then
    OUT=$(cd "$1" && pwd)
    eval "$(make -s -C "$OUT" printvars QUOTED_VARS=YES \
        VARS='TARGET_CXX PKG_CONFIG_HOST_BINARY TARGET_CXXFLAGS TARGET_LDFLAGS TARGET_MAKE_ENV STRIPCMD')"
    [ -x "$TARGET_CXX" ] || fail "no target compiler in $OUT ($TARGET_CXX): build it first"
    # (buildroot/package/banana-ui/banana-ui.mk's build command)
    m() { eval "env $TARGET_MAKE_ENV make -s -B -C \"\$U\" CXX=\"\$TARGET_CXX\" PKG_CONFIG=\"\$PKG_CONFIG_HOST_BINARY\" \
        CXXFLAGS=\"\$TARGET_CXXFLAGS -O2\" LDFLAGS=\"\$TARGET_LDFLAGS\" \"\$@\""; }
    strip_to() { cp "$1" "$2"; $STRIPCMD "$2"; }
    ON="$OUT's target toolchain"
else
    m() { make -s -C "$U" ALSA=0 "$@"; }
    strip_to() { strip -o "$2" "$1"; }
    ON="this machine's g++"
fi

# the one call, as upstream builds it
m clean >/dev/null
m KIWAMI_SERIAL=1 banana-ui
[ ! -e "$U/src/kiwami/obj" ] || fail "KIWAMI_SERIAL=1 made src/kiwami/obj/"
mv "$U/banana-ui" "$T/one"
# from objects
m clean >/dev/null
m banana-ui
N=$(find "$U/src/kiwami/obj" -name '*.o' | wc -l)
[ "$N" -gt 0 ] || fail "no objects in src/kiwami/obj/"
cp "$U/banana-ui" "$T/objects"
cmp "$T/one" "$T/objects" || fail "$ON: the objects' banana-ui differs from the one call's"
strip_to "$T/one" "$T/one.stripped"
strip_to "$T/objects" "$T/objects.stripped"
cmp "$T/one.stripped" "$T/objects.stripped" || fail "$ON: stripped, the objects' banana-ui differs from the one call's"
echo "ui-objects: $ON: banana-ui from $N objects is the one call's, byte for byte, unstripped and stripped"
[ -z "$1" ] || exit 0

# what a change rebuilds: the objects newer than a mark made just before it (a second apart: mtimes can be coarse)
newer() { (cd "$U" && find src/kiwami/obj -name '*.o' -newer "$T/mark" | sort); }
change() { sleep 1; touch "$T/mark"; }

change; touch "$U/src/colors.cpp"; m banana-ui
[ "$(newer)" = src/kiwami/obj/src/colors.cpp.o ] || fail "src/colors.cpp changed, these were compiled: $(newer)"

H=src/kiwami/qrcodegen.hpp
(cd "$U" && grep -l "$H" $(find src/kiwami/obj -name '*.d') | sed 's/\.d$/.o/' | sort) > "$T/includers"
[ -s "$T/includers" ] && [ "$(wc -l < "$T/includers")" -lt "$N" ] || fail "$H: $(wc -l < "$T/includers") of $N objects include it"
change; touch "$U/$H"; m banana-ui
[ "$(newer)" = "$(cat "$T/includers")" ] || fail "$H changed: compiled $(newer | wc -l) objects, $(wc -l < "$T/includers") include it"

change; m CXXFLAGS='-O2 -DKIWAMI_FLAGS_TEST' banana-ui
[ "$(newer | wc -l)" = "$N" ] || fail "the flags changed: compiled $(newer | wc -l) of $N objects"
echo "ui-objects: a .cpp compiled its object again, $H the $(wc -l < "$T/includers") including it, the flags all $N"

make -n -p -C "$U" ALSA=0 banana-ui 2>/dev/null | grep -q '^MAKEFLAGS = .* -j[0-9]' || fail "make without -j wasn't given one"
make -s -C "$U" clean
[ ! -e "$U/src/kiwami/obj" ] || fail "make clean left src/kiwami/obj/"
echo "ui-objects: make without -j given one job per CPU; make clean removes src/kiwami/obj/"
