#!/bin/sh
# Compares two builds' SD card images and update packages by their contents, with what already differs between two
# builds of one commit (dates, random IDs, times, build paths, Buildroot's bookkeeping) excused and listed:
#   kiwami/tests/image-compare.sh <old> <new>             each an output directory or a folder saved from one
#   kiwami/tests/image-compare.sh --save <output dir> <folder>
# In an output it reads images/banana-<board>.img.gz and images/banana-<board>-<version>.update.tar, the version being
# its target/etc/banana/version; in a saved folder, its one .img.gz, its one .update.tar and origin, the path of the
# output they were built in. --save copies an output's two into a new or empty folder, with origin, so the output can
# be built again. Builds made in paths of different lengths can't be compared where a binary holds the path (sqlite3's
# and bananads' __FILE__): it warns, and those differences fail. The tools are an output's host/bin, the second
# argument's preferred (KIWAMI_TOOLS=<dir> to name one; two saved folders need it), its python3 running
# image-compare.py, so the image's own .pyc files can be read. It works in a folder under ${TMPDIR:-/tmp} (about
# 2.5 GB), removed on exit. Exit 0: the same contents; 1: different; 2: no comparison (a file or tool missing).
set -e
ME=$(cd "$(dirname "$0")" && pwd)
fail() { echo "image-compare: $*" >&2; exit 2; }

# out <dir>: the output's image and update package (IMG, UPD) and its path (ORIGIN)
out() {
    set -- "$1"/images/banana-*.img.gz
    [ $# = 1 ] && [ -f "$1" ] || fail "$(dirname "$1"): not exactly one banana-<board>.img.gz"
    IMG=$1
    d=$(dirname "$(dirname "$1")")
    [ -f "$d/target/etc/banana/version" ] || fail "$d: no target/etc/banana/version"
    b=$(basename "$IMG" .img.gz)
    UPD=$d/images/$b-$(cat "$d/target/etc/banana/version").update.tar
    [ -f "$UPD" ] || fail "$d: no $(basename "$UPD")"
    ORIGIN=$(cd "$d" && pwd -P)
}

# saved <dir>: a saved folder's
saved() {
    set -- "$1"/*.img.gz
    [ $# = 1 ] && [ -f "$1" ] || fail "$(dirname "$1"): not exactly one .img.gz"
    IMG=$1
    d=$(dirname "$1")
    set -- "$d"/*.update.tar
    [ $# = 1 ] && [ -f "$1" ] || fail "$d: not exactly one .update.tar"
    UPD=$1
    [ -s "$d/origin" ] || fail "$d: no origin"
    ORIGIN=$(cat "$d/origin")
}

# side <arg>: an output (OUTPUT=1) or a saved folder
side() {
    [ -d "$1" ] || fail "$1: no such folder"
    if [ -f "$1/origin" ]; then saved "$1"; OUTPUT=; elif [ -d "$1/images" ]; then out "$1"; OUTPUT=1
    else fail "$1: neither an output (images/) nor a saved folder (origin)"; fi
}

if [ "$1" = --save ]; then
    [ $# = 3 ] || fail "usage: image-compare.sh --save <output dir> <folder>"
    [ -d "$2/images" ] || fail "$2: not an output (no images/)"
    out "$(cd "$2" && pwd)"
    mkdir -p "$3"
    [ -z "$(ls -A "$3")" ] || fail "$3 isn't empty"
    cp "$IMG" "$UPD" "$3/"
    echo "$ORIGIN" > "$3/origin"
    echo "image-compare: saved $(basename "$IMG") and $(basename "$UPD") from $ORIGIN in $3"
    exit 0
fi
[ $# = 2 ] || fail "usage: image-compare.sh <old> <new> (each an output directory or a saved folder)
       image-compare.sh --save <output dir> <folder>"

side "$(cd "$1" 2>/dev/null && pwd || echo "$1")"
OLD_IMG=$IMG OLD_UPD=$UPD OLD_PATH=$ORIGIN TOOLS=${OUTPUT:+$ORIGIN/host/bin}
side "$(cd "$2" 2>/dev/null && pwd || echo "$2")"
NEW_IMG=$IMG NEW_UPD=$UPD NEW_PATH=$ORIGIN
[ -z "$OUTPUT" ] || TOOLS=$ORIGIN/host/bin
TOOLS=${KIWAMI_TOOLS:-$TOOLS}
[ -n "$TOOLS" ] || fail "two saved folders: name an output's tools, KIWAMI_TOOLS=<output>/host/bin"
for t in python3 unsquashfs lz4 mcopy mdir; do
    [ -x "$TOOLS/$t" ] || fail "no $t in $TOOLS"
done

W=$(mktemp -d "${TMPDIR:-/tmp}/image-compare.XXXXXX")
trap 'chmod -R u+rwX "$W" 2>/dev/null; rm -rf "$W"' EXIT
trap 'exit 2' INT TERM
set +e
PATH=$TOOLS:$PATH "$TOOLS/python3" -I "$ME/image-compare.py" --work "$W" \
    --old "$OLD_IMG" "$OLD_UPD" "$OLD_PATH" --new "$NEW_IMG" "$NEW_UPD" "$NEW_PATH"
exit $?
