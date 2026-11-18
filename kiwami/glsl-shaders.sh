#!/bin/sh
# RetroArch's GLSL shaders as libretro wrote them, after upstream's install (kiwami/glsl-shaders.mk's hook):
#   glsl-shaders.sh <source> <installed>
# Every .glsl in the package's source folder is copied back over its installed copy, undoing upstream's rewrite of
# every vertex stage to highp. Then, in a shader whose fragment stage defines COMPAT_PRECISION highp (where the GPU
# has it) and whose vertex stage defines no highp of its own, the vertex stage's "#define COMPAT_PRECISION mediump"
# lines say highp too, so the uniforms both stages declare have one precision, as the GLES linker wants; a vertex
# stage always has highp. (A vertex stage with highp under #ifdef GL_FRAGMENT_PRECISION_HIGH, as zfast_crt's, already
# matches on the GPU, and its #else line is left as it is.) At libretro's ce9a4124 those are zfast_crt_geo,
# zfast_crt_geo_svideo, zfast_crt_nogeo_svideo and pixellate, the shaders the rewrite was for; every other shader keeps
# both stages as libretro wrote them (as ROCKNIX ships them). Only those lines change: every other byte is the source's. Stops on a source with no shader, and on a shader upstream's install didn't install (its
# install changed: kiwami/tests/glsl-shaders.sh checks both).
set -e
S=$1 D=$2
[ -d "$S" ] && [ -d "$D" ] || { echo "glsl-shaders.sh: usage: glsl-shaders.sh <source> <installed shaders>" >&2; exit 1; }
L=$(cd "$S" && find . -name '*.glsl' | sed 's|^\./||' | sort)
[ -n "$L" ] || { echo "glsl-shaders.sh: no .glsl in $S" >&2; exit 1; }
for f in $L; do
    [ -f "$D/$f" ] || { echo "glsl-shaders.sh: $f isn't installed in $D (upstream's install changed?)" >&2; exit 1; }
done
n=0 raised=
for f in $L; do
    cp -p "$S/$f" "$D/$f"
    n=$((n + 1))
    lines=$(awk 'FNR == 1 { pass++; s = "" }
        /defined\(VERTEX\)/ { s = "v" }  /defined\(FRAGMENT\)/ { s = "f" }
        pass == 1 && /^[ \t]*#[ \t]*define[ \t]+COMPAT_PRECISION[ \t]+highp/ { if (s == "f") hi = 1; if (s == "v") vhi = 1 }
        pass == 2 && hi && !vhi && s == "v" && /^[ \t]*#[ \t]*define[ \t]+COMPAT_PRECISION[ \t]+mediump/ { print FNR }' "$S/$f" "$S/$f")
    [ -n "$lines" ] || continue
    for l in $lines; do sed -i "${l}s/mediump/highp/" "$D/$f"; done
    raised="$raised $f"
done
echo "glsl-shaders.sh: $n shaders as libretro wrote them, the vertex stage at highp in $(echo $raised | wc -w):$raised"
