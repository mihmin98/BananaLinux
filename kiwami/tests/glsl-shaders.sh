#!/bin/sh
# RetroArch's GLSL shaders as the image installs them, checked by kiwami/tests/glsl-precision.py (with
# kiwami/tests/glsl-known.txt's faults of libretro's own excused):
#   kiwami/tests/glsl-shaders.sh <buildroot dir>      libretro's shaders at upstream's pin (the tarball in the Buildroot
#                                                    folder's dl/, else fetched from GitHub), installed by upstream's
#                                                    package outside Buildroot (kiwami/tests/glsl-shaders-shim.mk):
#   - with BR2_PACKAGE_KIWAMI_GLSL_SHADERS_PRECISION off, upstream's install alone, which must fail the check: when it
#     passes, upstream has fixed its rewrite, and the fork's hook may no longer be needed (docs/FORK.md);
#   - with it on, upstream's install and the fork's hook (kiwami/glsl-shaders.mk), which must pass it; every .glsl is
#     then libretro's, but in the shaders the hook names, whose fragment stage defines COMPAT_PRECISION highp, whose
#     vertex stage defined none, and whose vertex stage's "#define COMPAT_PRECISION mediump" lines alone say highp;
#     and every other file is as off;
#   - the hook's script stops on a source with no shader, and on a shader upstream's install didn't install.
#   kiwami/tests/glsl-shaders.sh --output <output dir>   the shaders a built output's target holds, the check alone
# Run by kiwami/tests/ci.sh (the first way).
set -e
cd "$(dirname "$0")/../.."
fail() { echo "glsl-shaders: FAIL: $*" >&2; exit 1; }
S=usr/share/libretro/shaders/glsl
CHECK="python3 -I kiwami/tests/glsl-precision.py --known kiwami/tests/glsl-known.txt"

if [ "$1" = --output ]; then
    [ -d "$2/target/$S" ] || fail "no $2/target/$S"
    $CHECK "$2/target/$S" > "${TMPDIR:-/tmp}/glsl-shaders-output.$$" 2>&1 || {
        grep -v '^known: ' "${TMPDIR:-/tmp}/glsl-shaders-output.$$" | head -n 40 >&2
        rm -f "${TMPDIR:-/tmp}/glsl-shaders-output.$$"; fail "$2's shaders don't pass the check"; }
    echo "glsl-shaders: $2: $(tail -n 1 "${TMPDIR:-/tmp}/glsl-shaders-output.$$" | sed 's/^glsl-precision: //')"
    rm -f "${TMPDIR:-/tmp}/glsl-shaders-output.$$"
    exit 0
fi
[ -d "$1" ] || { echo "usage: kiwami/tests/glsl-shaders.sh <buildroot dir> | --output <output dir>" >&2; exit 2; }

X=$(mktemp -d "${TMPDIR:-/tmp}/kiwami-glsl.XXXXXX")
trap 'rm -rf "$X"' EXIT
SHIM="make -s -f kiwami/tests/glsl-shaders-shim.mk"
V=$($SHIM version)
[ -n "$V" ] || fail "no RETROARCH_GLSL_SHADERS_VERSION from upstream's .mk"
TAR=$1/dl/retroarch-glsl-shaders/retroarch-glsl-shaders-$V.tar.gz
if [ ! -f "$TAR" ]; then
    TAR=$X/glsl-shaders.tar.gz
    curl -fsSL -o "$TAR" "https://github.com/libretro/glsl-shaders/archive/$V.tar.gz" || fail "can't fetch glsl-shaders $V"
fi
mkdir "$X/src"
tar -xzf "$TAR" -C "$X/src" --strip-components=1

# off: upstream's install alone fails the check
$SHIM SRC="$X/src" TARGET_DIR="$X/off" "$X/src/.install" > "$X/off.build" 2>&1 || { cat "$X/off.build" >&2; fail "upstream's install failed"; }
rc=0; $CHECK "$X/off/$S" > "$X/off.log" 2>&1 || rc=$?
[ $rc != 0 ] || fail "upstream's shaders pass the check: see whether the fork's hook is still needed (docs/FORK.md)"
[ $rc = 1 ] || { cat "$X/off.log" >&2; fail "the check couldn't run on upstream's shaders"; }
UP=$(tail -n 1 "$X/off.log" | sed 's/^glsl-precision: [0-9]* shaders, //')

# on: upstream's install and the fork's hook pass it
$SHIM SRC="$X/src" TARGET_DIR="$X/on" BR2_PACKAGE_KIWAMI_GLSL_SHADERS_PRECISION=y "$X/src/.install" > "$X/on.build" 2>&1 ||
    { cat "$X/on.build" >&2; fail "upstream's install and the fork's hook failed"; }
$CHECK "$X/on/$S" > "$X/on.log" 2>&1 || { grep -v '^known: ' "$X/on.log" | head -n 40 >&2; fail "the fork's shaders don't pass the check"; }

# ... each .glsl libretro's, but the vertex stages the hook says it raised, and only their COMPAT_PRECISION lines
named=$(sed -n 's/^glsl-shaders\.sh: .* the vertex stage at highp in [0-9]*: //p' "$X/on.build" | tr ' ' '\n' | sed '/^$/d' | sort)
[ -n "$named" ] || fail "the hook named no shader it raised: $(cat "$X/on.build")"
changed=$(cd "$X/src" && find . -name '*.glsl' | sed 's|^\./||' | sort | while IFS= read -r f; do
    cmp -s "$f" "$X/on/$S/$f" || echo "$f"; done)
[ "$changed" = "$named" ] || fail "the shaders that differ from libretro's ($(echo $changed)) aren't those the hook named ($(echo $named))"
for f in $changed; do
    awk '/defined\(VERTEX\)/ { s = "v" } /defined\(FRAGMENT\)/ { s = "f" }
         /^[ \t]*#[ \t]*define[ \t]+COMPAT_PRECISION[ \t]+highp/ { if (s == "f") hi = 1; if (s == "v") vhi = 1 }
         END { exit !(hi && !vhi) }' "$X/src/$f" ||
        fail "$f raised, but its fragment stage doesn't define COMPAT_PRECISION highp, or its vertex stage does"
    other=$(diff "$X/src/$f" "$X/on/$S/$f" | grep -vE '^[0-9]+(,[0-9]+)?c[0-9]+(,[0-9]+)?$|^---$' |
        grep -vE '^< [ 	]*#[ 	]*define[ 	]+COMPAT_PRECISION[ 	]+mediump[[:space:]]*$' |
        grep -vE '^> [ 	]*#[ 	]*define[ 	]+COMPAT_PRECISION[ 	]+highp[[:space:]]*$' || true)
    [ -z "$other" ] || fail "$f differs from libretro's beyond COMPAT_PRECISION mediump to highp: $other"
done

# ... and every other file as upstream installs it
diff -r -x '*.glsl' "$X/off" "$X/on" > "$X/other.diff" || fail "files other than shaders differ with the hook: $(head -n 5 "$X/other.diff")"

# the hook's script stops on a source with no shader, and on a shader that isn't installed
mkdir "$X/empty"
! sh kiwami/glsl-shaders.sh "$X/empty" "$X/on/$S" 2> "$X/err" || fail "the hook's script took a source with no shader"
grep -q 'no \.glsl' "$X/err" || fail "the hook's script's message for no shader: $(cat "$X/err")"
cp -a "$X/on" "$X/miss"
rm "$X/miss/$S/stock.glsl"
! sh kiwami/glsl-shaders.sh "$X/src" "$X/miss/$S" 2> "$X/err" || fail "the hook's script took a shader that isn't installed"
grep -q 'stock\.glsl' "$X/err" || fail "the hook's script doesn't name the shader that isn't installed: $(cat "$X/err")"

echo "glsl-shaders: libretro's $V; upstream's install: $UP; the fork's: none ($(echo $named | wc -w) vertex stages raised: $(echo $named))"
