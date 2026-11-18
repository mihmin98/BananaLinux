#!/usr/bin/env python3
# Checks RetroArch's GLSL shaders (the image's /usr/share/libretro/shaders/glsl) the way RetroArch's "gl" driver
# compiles them on GLES 3, without a GPU:
#   glsl-precision.py [--known <file>] <shaders dir>   each .glsl's two stages, preprocessed by glslangValidator: exit
#                                     1 on a preprocessor error or a uniform both stages declare at two precisions
#   glsl-precision.py --self-test     the same on made-up shaders
# --known names faults already in libretro's own sources, one "<path>: <uniform or stage>" per line (# comments): each
# is reported as known and not counted, and one no longer found is a fault (fixed where it came from: drop its line).
# A .glsl holds both stages; RetroArch compiles it twice (gfx/drivers_shader/shader_glsl.c, RetroArch master 71f521e):
# its "#pragma parameter" lines blanked (gl_glsl_strip_parameter_pragmas), the first "#version N" found as text and the
# program taken from after its number, N given as "#version 300 es" from 130 to 329, "310 es" at 330, "320 es" above
# and "100" below (gl_glsl_compile_shader), no version line at all when there is none (GLSL ES 1.00 then), and the
# stage's six defines in front (VERTEX or FRAGMENT, PARAMETER_UNIFORM, ...). The pass aliases' "#define <alias>_ALIAS"
# are left out: they depend on the preset. glslangValidator -E defines GL_ES and GL_FRAGMENT_PRECISION_HIGH in both
# stages, as Mesa does on the Mali-G52.
# Two faults stop a shader on Panfrost: a stage that doesn't preprocess (a macro defined twice with two bodies), and a
# uniform declared in both stages at two precisions (a link error in GLSL ES 3.00; in 1.00 when both use it, and
# Panfrost lowers a mediump uniform to 16 bits). A declaration without a precision has its stage's default: highp
# float and int in the vertex stage, the "precision" statements' before it, mediump int and none for float in the
# fragment stage, lowp samplers in both. This models the GLSL rules, not Mesa's compiler: what passes here can still
# fail on the device, and the check is no substitute for loading the shader there.
import concurrent.futures
import os
import re
import shutil
import subprocess
import sys
import tempfile

DEFINES = ("#define {}\n#define PARAMETER_UNIFORM\n#define _HAS_ORIGINALASPECT_UNIFORMS\n#define _HAS_FRAMETIME_UNIFORMS\n"
           "#define _HAS_SENSOR_UNIFORMS\n#define _HAS_SWAPCOUNT_UNIFORM\n")
DEFAULTS = {"vertex": {"float": "highp", "int": "highp", "sampler2D": "lowp", "samplerCube": "lowp"},
            "fragment": {"int": "mediump", "sampler2D": "lowp", "samplerCube": "lowp"}}


def assemble(text, stage):
    """The source RetroArch hands the GLES compiler for one stage of a .glsl, and its GLSL ES version."""
    text = re.sub(r"#pragma parameter[^\n]*", lambda m: " " * len(m.group()), text)
    i = text.find("#version")
    if i < 0:
        header, program, es = "#version 100\n", text, "1.00"
    else:
        rest = text[i + 8:]                             # strtoul(): blanks, then digits
        j = k = len(rest) - len(rest.lstrip())
        while k < len(rest) and rest[k].isdigit():
            k += 1
        n, program = int(rest[j:k] or 0), rest[k:]
        if 130 <= n < 330:
            header, es = "#version 300 es\n", "3.00"
        elif n == 330:
            header, es = "#version 310 es\n", "3.10"
        elif n > 330:
            header, es = "#version 320 es\n", "3.20"
        else:
            header, es = "#version 100\n", "1.00"
    return header + DEFINES.format(stage.upper()) + program, es


def base_type(t):
    if t == "float" or re.fullmatch(r"(vec|mat)\d(x\d)?", t):
        return "float"
    if t in ("int", "uint") or re.fullmatch(r"[iu]vec\d", t):
        return "int"
    if "sampler" in t:
        return t
    return None                                         # bool, bvec, a struct: no precision of its own


def uniforms(text, stage):
    """{name: precision} of the uniforms a preprocessed stage declares at global scope."""
    text = re.sub(r"/\*.*?\*/", " ", text, flags=re.S)
    text = re.sub(r"//[^\n]*", "", text)
    text = "\n".join(l for l in text.split("\n") if not l.lstrip().startswith("#"))
    defaults, found = dict(DEFAULTS[stage]), {}
    depth, cur = 0, []
    for ch in text:
        if ch == "{":
            depth += 1
            cur = []                                    # a function, a struct or an interface block: not looked into
        elif ch == "}":
            depth -= 1
        elif depth == 0 and ch == ";":
            st = " ".join("".join(cur).split())
            cur = []
            m = re.fullmatch(r"precision (lowp|mediump|highp) (\w+)", st)
            if m:
                defaults[m.group(2)] = m.group(1)
                continue
            m = re.fullmatch(r"(?:layout ?\([^)]*\) )?uniform (?:(lowp|mediump|highp) )?(\w+) (.+)", st)
            if not m:
                continue
            q, t = m.group(1), base_type(m.group(2))
            if t is None:
                continue
            for d in m.group(3).split(","):
                n = re.match(r"\s*(\w+)", d)
                if n:
                    found[n.group(1)] = q or defaults.get(t, "none")
        elif depth == 0:
            cur.append(ch)
    return found


def check_one(tool, root, rel, work):
    """(path, preprocessor errors, mismatches) of one .glsl."""
    with open(os.path.join(root, rel), encoding="utf-8", errors="replace") as f:
        text = f.read()
    errors, seen, es = [], {}, None
    for stage, ext in (("vertex", "vert"), ("fragment", "frag")):
        src, es = assemble(text, stage)
        fd, tmp = tempfile.mkstemp(suffix="." + ext, dir=work)
        with os.fdopen(fd, "w", encoding="utf-8") as f:
            f.write(src)
        r = subprocess.run([tool, "-E", tmp], capture_output=True, text=True, errors="replace")
        os.unlink(tmp)
        if r.returncode != 0:
            first = next((l for l in (r.stdout + r.stderr).splitlines() if l.startswith("ERROR")), "exit %d" % r.returncode)
            errors.append("%s: %s stage: %s" % (rel, stage, first.replace(tmp, "<source>")))
        else:
            seen[stage] = uniforms(r.stdout, stage)
    mismatches = []
    if not errors:
        v, fr = seen["vertex"], seen["fragment"]
        for n in sorted(set(v) & set(fr)):
            if v[n] != fr[n]:
                mismatches.append("%s: %s: vertex %s, fragment %s (GLSL ES %s)" % (rel, n, v[n], fr[n], es))
    return rel, errors, mismatches


def find_tool():
    tool = shutil.which("glslangValidator")
    if not tool:
        print("glsl-precision: no glslangValidator (install glslang-tools)", file=sys.stderr)
        sys.exit(2)
    return tool


def read_known(path):
    with open(path, encoding="utf-8") as f:
        return [l for l in (l.split("#", 1)[0].strip() for l in f) if l]


def check(root, known=(), out=sys.stdout):
    """Checks every .glsl under root; returns (shaders, with an error, with a mismatch, known faults found, known faults
    gone), or None with none to check."""
    tool = find_tool()
    files = sorted(os.path.relpath(os.path.join(d, n), root)
                   for d, _, ns in os.walk(root) for n in ns if n.endswith(".glsl"))
    if not files:
        return None
    with tempfile.TemporaryDirectory() as work, \
            concurrent.futures.ThreadPoolExecutor(os.cpu_count() or 4) as pool:
        results = list(pool.map(lambda rel: check_one(tool, root, rel, work), files))
    found, e, m = set(), 0, 0
    for _, errors, mismatches in results:
        bad = set()
        for kind, lines in (("e", errors), ("m", mismatches)):
            for line in lines:
                k = next((k for k in known if line.startswith(k + ":")), None)
                if k:
                    found.add(k)
                    print("known: " + line, file=out)
                else:
                    bad.add(kind)
                    print(line, file=out)
        e, m = e + ("e" in bad), m + ("m" in bad)
    gone = [k for k in known if k not in found]
    for k in gone:
        print("%s: known, but no longer found" % k, file=out)
    return len(files), e, m, len(found), len(gone)


def main():
    args = sys.argv[1:]
    if args == ["--self-test"]:
        return self_test()
    known = []
    if len(args) == 3 and args[0] == "--known":
        known, args = read_known(args[1]), args[2:]
    if len(args) != 1:
        sys.exit("usage: glsl-precision.py [--known <file>] <shaders dir> | --self-test")
    res = check(args[0], known)
    if res is None:
        print("glsl-precision: no .glsl in %s" % args[0], file=sys.stderr)
        sys.exit(2)
    n, e, m, k, gone = res
    print("glsl-precision: %d shaders, %d with a preprocessor error, %d with a precision mismatch" % (n, e, m) +
          (", %d known faults excused, %d no longer found" % (k, gone) if known else ""))
    sys.exit(1 if e or m or gone else 0)


# A .glsl as libretro writes them: {V} is the vertex stage's COMPAT_PRECISION, {FDEF} the fragment stage's definition
# of it, {TOP} what goes before the stages, {VER} its #version line
SHADER = """{VER}{TOP}
#pragma parameter BRIGHT "Brightness \\"x\\"" 1.0 0.0 2.0 0.1
#if defined(VERTEX)
#ifdef GL_ES
#define COMPAT_PRECISION {V}
#else
#define COMPAT_PRECISION
#endif
attribute vec4 VertexCoord;
uniform mat4 MVPMatrix;
uniform COMPAT_PRECISION vec2 OutputSize;
uniform COMPAT_PRECISION vec2 TextureSize;
void main() {{ gl_Position = MVPMatrix * VertexCoord * OutputSize.x * TextureSize.x; }}
#elif defined(FRAGMENT)
#ifdef GL_ES
#ifdef GL_FRAGMENT_PRECISION_HIGH
precision highp float;
#else
precision mediump float;
#endif
{FDEF}
#else
#define COMPAT_PRECISION
#endif
uniform COMPAT_PRECISION vec2 OutputSize;
uniform COMPAT_PRECISION vec2 TextureSize;
uniform sampler2D Texture;
void main() {{ gl_FragColor = texture2D(Texture, OutputSize * TextureSize); }}
#endif
"""


def self_test():
    find_tool()
    cases = []

    def run(name, files, want_errors, want_mismatches, lines=(), known=(), want_known=(0, 0)):
        with tempfile.TemporaryDirectory() as t:
            for rel, text in files.items():
                os.makedirs(os.path.dirname(os.path.join(t, rel)), exist_ok=True)
                with open(os.path.join(t, rel), "w") as f:
                    f.write(text)

            class Out(list):
                def write(self, s):
                    self.append(s)
            out = Out()
            res = check(t, known, out)
            text = "".join(out)
            ok = (res is not None and res[1:] == (want_errors, want_mismatches) + want_known
                  and all(l in text for l in lines))
            if not ok:
                print("glsl-precision self-test: FAIL: %s: %s\n%s" % (name, res, text), file=sys.stderr)
                sys.exit(1)
        cases.append(name)

    def shader(v="mediump", f="mediump", ver="", top="", fdef=None):
        return SHADER.format(V=v, FDEF=fdef or "#define COMPAT_PRECISION " + f, VER=ver, TOP=top)

    run("both stages mediump", {"a.glsl": shader()}, 0, 0)
    run("vertex highp, fragment mediump, ES 1.00", {"s/a.glsl": shader(v="highp")}, 0, 1,
        ["s/a.glsl: OutputSize: vertex highp, fragment mediump (GLSL ES 1.00)",
         "s/a.glsl: TextureSize: vertex highp, fragment mediump (GLSL ES 1.00)"])
    run("vertex highp, fragment mediump, #version 130", {"a.glsl": shader(v="highp", ver="#version 130\n")}, 0, 1,
        ["a.glsl: OutputSize: vertex highp, fragment mediump (GLSL ES 3.00)"])
    src, es = assemble("// x\n#version 130\nvoid main() {}\n", "vertex")
    if not (src.startswith("#version 300 es\n#define VERTEX\n#define PARAMETER_UNIFORM\n") and es == "3.00"
            and "// x" not in src and "130" not in src):
        sys.exit("glsl-precision self-test: FAIL: #version 130 assembled as:\n" + src)
    cases.append("#version 130 assembled as 300 es, what's before it dropped")
    for n, want in ((110, "100"), (120, "100"), (329, "300 es"), (330, "310 es"), (400, "320 es")):
        if not assemble("#version %d\n" % n, "fragment")[0].startswith("#version %s\n#define FRAGMENT\n" % want):
            sys.exit("glsl-precision self-test: FAIL: #version %d isn't #version %s" % (n, want))
    cases.append("the version mapping")
    run("COMPAT_PRECISION defined twice", {"a.glsl": shader(v="highp", top="#ifdef GL_ES\n#define COMPAT_PRECISION "
                                                            "mediump\n#else\n#define COMPAT_PRECISION\n#endif\n")},
        1, 0, ["a.glsl: vertex stage: ERROR:", "COMPAT_PRECISION"])
    run("fragment highp under GL_FRAGMENT_PRECISION_HIGH, vertex highp",
        {"a.glsl": shader(v="highp", fdef="#ifdef GL_FRAGMENT_PRECISION_HIGH\n#define COMPAT_PRECISION highp\n#else\n"
                                        "#define COMPAT_PRECISION mediump\n#endif")}, 0, 0)
    run("a default precision against a declared one",
        {"a.glsl": "#if defined(VERTEX)\nuniform vec2 OutputSize;\nvoid main() { gl_Position = vec4(OutputSize, 0, 1); }\n"
                   "#else\nprecision mediump float;\nuniform vec2 OutputSize;\n"
                   "void main() { gl_FragColor = vec4(OutputSize, 0, 1); }\n#endif\n"}, 0, 1,
        ["a.glsl: OutputSize: vertex highp, fragment mediump (GLSL ES 1.00)"])
    run("int's default precisions",
        {"a.glsl": "#if defined(VERTEX)\nuniform int FrameCount;\nvoid main() { gl_Position = vec4(1); }\n"
                   "#else\nprecision mediump float;\nuniform int FrameCount;\n"
                   "void main() { gl_FragColor = vec4(float(FrameCount)); }\n#endif\n"}, 0, 1,
        ["a.glsl: FrameCount: vertex highp, fragment mediump (GLSL ES 1.00)"])
    run("several names in one declaration",
        {"a.glsl": "#if defined(VERTEX)\nuniform mediump vec2 a, b;\nvoid main() { gl_Position = vec4(a, b); }\n"
                   "#else\nprecision mediump float;\nuniform lowp vec2 a, b;\n"
                   "void main() { gl_FragColor = vec4(a, b); }\n#endif\n"}, 0, 1,
        ["a.glsl: a: vertex mediump, fragment lowp", "a.glsl: b: vertex mediump, fragment lowp"])
    run("one shader of two at fault", {"ok.glsl": shader(), "bad.glsl": shader(f="lowp")}, 0, 1,
        ["bad.glsl: OutputSize: vertex mediump, fragment lowp"])
    bad = {"ok.glsl": shader(), "bad.glsl": shader(f="lowp")}
    run("known faults excused", bad, 0, 0, ["known: bad.glsl: OutputSize: vertex mediump, fragment lowp"],
        ["bad.glsl: OutputSize", "bad.glsl: TextureSize"], (2, 0))
    run("one of two faults known", bad, 0, 1, ["bad.glsl: TextureSize: vertex mediump"], ["bad.glsl: OutputSize"], (1, 0))
    run("a known fault gone", {"ok.glsl": shader()}, 0, 0, ["bad.glsl: OutputSize: known, but no longer found"],
        ["bad.glsl: OutputSize"], (0, 1))
    run("a known preprocessor error", {"a.glsl": shader(v="highp", top="#define COMPAT_PRECISION mediump\n")}, 0, 0,
        ["known: a.glsl: vertex stage: ERROR:"], ["a.glsl: vertex stage"], (1, 0))
    with tempfile.TemporaryDirectory() as t:
        with open(os.path.join(t, "known"), "w") as f:
            f.write("# a comment\nbad.glsl: OutputSize  # why\n\n")
        if read_known(os.path.join(t, "known")) != ["bad.glsl: OutputSize"]:
            sys.exit("glsl-precision self-test: FAIL: the known list read as %s" % read_known(os.path.join(t, "known")))
    cases.append("the known list's comments")
    with tempfile.TemporaryDirectory() as t:
        if check(t) is not None:
            sys.exit("glsl-precision self-test: FAIL: an empty folder was checked")
        r = subprocess.run([sys.executable, "-I", os.path.abspath(__file__), t], capture_output=True, text=True)
        if r.returncode != 2:
            sys.exit("glsl-precision self-test: FAIL: an empty folder exited %d" % r.returncode)
    cases.append("an empty folder")
    print("glsl-precision self-test: %d cases passed" % len(cases))


if __name__ == "__main__":
    main()
