#!/usr/bin/env python3
# Finds the libraries a package links without declaring the package they come from. Under per-package directories
# (kiwami.fragment) a package builds against only what it depends on, so such a library is a link that fails, or an
# optional feature a configure script drops without a word, depending on the build order. Built one package at a
# time, it works only when the provider happens to come first. Run on an output's fresh build:
#   deps-audit.py <output dir> [--info <show-info.json>]
#   deps-audit.py --self-test     the audit on a made-up output, with the host's cc and readelf
# Every target package the output builds is audited, Buildroot's own too (its sqlite linked libz without declaring
# zlib), and each is named with its tree: Buildroot's, upstream's (buildroot/package/) or the fork's
# (kiwami/package/). Their ELF files in target/, by build/packages-file-list.txt, give the libraries they link
# (readelf -d's NEEDED); a library's provider is the package whose file list holds a file of that name. A provider
# outside the package's dependencies (recursively, through virtual packages: libegl, then mesa3d-panfrost) is
# reported, and the missing ones go in kiwami/external.mk. Buildroot's graph comes from make show-info, which holds
# the dependencies added late with += (bluez-alsa's ldacbt), unless --info gives it.
# A package's list is what changed in target/ between its configure step and the end of its install (pkg-generic.mk's
# pkg_size_before and pkg_size_after). Built one package at a time, that also holds what other packages installed in
# between: ldacbt's staging install comes early, for bluez-alsa's configure (buildroot/external.mk), its target
# install in the package order, and dozens of packages install in between. So there, without per-package directories,
# a file in two lists is the other package's when that one's install ended inside this one's window (build-time.log).
# Files that more than one package installed are then listed apart: their last writer is the build order's one package
# at a time, and the package names' order when per-package directories are merged. An incremental output's lists are
# muddied (a package reinstalled records the files it touched again, a package gone leaves its list), so the audit
# says when the output isn't a fresh one.
# Exit 0: every library linked is declared; 1: a package is reported; 2: no audit.
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile

# packages whose undeclared libraries are expected, and why
ALLOWED = {
    "drastic": "a prebuilt binary: nothing is linked at build time",
}
NEEDED = re.compile(r"\(NEEDED\)\s+Shared library: \[([^\]]+)\]")


class Fail(Exception):
    """Something that stops the audit (exit 2)."""


def log(msg):
    print("deps-audit: " + msg, file=sys.stderr, flush=True)


def audited(info):
    """The target packages, not virtual."""
    return sorted(n for n, p in info.items() if p.get("type") == "target" and not p.get("virtual"))


def tree(info, name):
    """Whose package it is, by the folder of its .mk: upstream's buildroot/package/, the fork's kiwami/package/, or
    Buildroot's own package/."""
    d = os.path.normpath((info.get(name) or {}).get("package_dir") or "").split(os.sep)
    if len(d) >= 3 and d[-2] == "package" and d[-3] in ("buildroot", "kiwami"):
        return "upstream's" if d[-3] == "buildroot" else "the fork's"
    return "Buildroot's"


def closure(info, name):
    """name and everything it depends on, recursively (a virtual package depends on its provider)."""
    seen, todo = set(), [name]
    while todo:
        n = todo.pop()
        if n not in seen:
            seen.add(n)
            todo.extend((info.get(n) or {}).get("dependencies") or [])
    return seen


def read_list(path):
    """packages-file-list.txt's lines (<package>,./<path>), as (package, path)."""
    lines = []
    try:
        with open(path, encoding="utf-8", errors="surrogateescape") as f:
            for line in f:
                pkg, sep, p = line.rstrip("\n").partition(",")
                if sep:
                    lines.append((pkg, os.path.normpath(p)))
    except OSError as e:
        raise Fail(f"{path}: {e.strerror}: is the output built?")
    return lines


def read_times(out):
    """build-time.log (<time>:start|end:<step>:<package>): each package's configure starts and install-target ends."""
    conf, inst = {}, {}
    try:
        with open(os.path.join(out, "build", "build-time.log"), encoding="utf-8", errors="replace") as f:
            for line in f:
                t = [x.strip() for x in line.split(":")]
                if len(t) != 4:
                    continue
                if t[1] == "start" and t[2] == "configure":
                    conf.setdefault(t[3], []).append(float(t[0]))
                elif t[1] == "end" and t[2] == "install-target":
                    inst.setdefault(t[3], []).append(float(t[0]))
    except (OSError, ValueError):
        pass
    return conf, inst


def attribute(lines, conf, inst):
    """Built one package at a time: a file in two lists left to the package whose install ended inside the other's
    window (its last configure start to its last install end). Returns the lines kept, and per package the files its
    list gave away and the packages they went to."""
    def inside(q, p):
        if not conf.get(p) or not inst.get(p) or not inst.get(q):
            return False
        return conf[p][-1] < inst[q][-1] < inst[p][-1]
    owners = {}
    for pkg, p in lines:
        if pkg not in owners.setdefault(p, []):
            owners[p].append(pkg)
    kept, moved = [], {}
    for pkg, p in lines:
        to = [q for q in owners[p] if q != pkg and inside(q, pkg)]
        if to:
            m = moved.setdefault(pkg, [0, set()])
            m[0] += 1
            m[1].update(to)
        else:
            kept.append((pkg, p))
    return kept, moved


def index(lines):
    """Each package's paths, and each path's packages."""
    files, owners = {}, {}
    for pkg, p in lines:
        files.setdefault(pkg, []).append(p)
        if pkg not in owners.setdefault(p, []):
            owners[p].append(pkg)
    return files, owners


def is_elf(path):
    if os.path.islink(path) or not os.path.isfile(path):
        return False
    with open(path, "rb") as f:
        return f.read(4) == b"\x7fELF"


def needed(readelf, path):
    r = subprocess.run([readelf, "-d", "-W", path], capture_output=True, text=True, env=dict(os.environ, LC_ALL="C"))
    return NEEDED.findall(r.stdout)


def audit(info, files, owners, target, readelf, allowed=ALLOWED):
    """What each audited package links from outside its dependencies. Returns a dict: reports and allowed, each
    {package: {library: (providers, [files])}}; unprovided, {package: {library: [files]}}; overlaps, {path: packages};
    each audited package's tree, and the counts of packages (per tree) and ELF files."""
    providers = {}
    for p, pkgs in owners.items():
        for pkg in pkgs:
            providers.setdefault(os.path.basename(p), set()).add(pkg)
    res = {"reports": {}, "allowed": {}, "unprovided": {}, "packages": {}, "trees": {}, "elf": 0,
           "overlaps": {p: pkgs for p, pkgs in sorted(owners.items()) if len(pkgs) > 1}}
    for pkg in audited(info):
        t = res["trees"][pkg] = tree(info, pkg)
        res["packages"][t] = res["packages"].get(t, 0) + 1
        deps = closure(info, pkg)
        for p in files.get(pkg, []):
            f = os.path.join(target, p)
            if not is_elf(f):
                continue
            res["elf"] += 1
            for lib in needed(readelf, f):
                prov = providers.get(lib)
                if not prov:
                    res["unprovided"].setdefault(pkg, {}).setdefault(lib, []).append(p)
                elif not prov & deps:
                    into = res["allowed" if pkg in allowed else "reports"].setdefault(pkg, {})
                    into.setdefault(lib, (sorted(prov), []))[1].append(p)
    return res


def incremental(info, files, inst):
    """Why the output's file lists aren't a fresh build's: packages installed more than once, or no longer built."""
    def some(names):
        return ", ".join(names[:5]) + (", ..." if len(names) > 5 else "")
    why = []
    twice = sorted(p for p, ends in inst.items() if len(ends) > 1)
    if twice:
        why.append(f"{len(twice)} packages installed more than once ({some(twice)})")
    gone = sorted(p for p in files if p not in info)
    if gone:
        why.append(f"the file lists of {len(gone)} packages no longer built ({some(gone)})")
    return why


def where(paths):
    return paths[0] + (f" and {len(paths) - 1} more" if len(paths) > 1 else "")


def write(res, why, moved, allowed=ALLOWED, out=sys.stdout):
    counts = ", ".join(f"{n} {t}" for t, n in sorted(res["packages"].items()))
    print(f"{sum(res['packages'].values())} packages ({counts}), {res['elf']} ELF files", file=out)
    if why:
        print("warning: not a fresh build, so the file lists are muddied: " + "; ".join(why)
              + ". Audit a fresh output (make clean-<board>, then make <board>)", file=out)
    if moved:
        print("\nfiles of packages installed between another's configure and install (built one package at a time), "
              "counted as theirs:", file=out)
        for pkg, (n, to) in sorted(moved.items()):
            print(f"  {pkg}'s list: {n} files of {len(to)} package" + ("s" if len(to) > 1 else ""), file=out)
    for pkg, libs in sorted(res["allowed"].items()):
        print(f"\nallowed: {pkg} ({allowed[pkg]}):", file=out)
        for lib, (prov, paths) in sorted(libs.items()):
            print(f"  {lib} from {', '.join(prov)} ({where(paths)})", file=out)
    if res["unprovided"]:
        print("\nno package provides:", file=out)
        for pkg, libs in sorted(res["unprovided"].items()):
            for lib, paths in sorted(libs.items()):
                print(f"  {pkg}: {lib} ({where(paths)})", file=out)
    if res["overlaps"]:
        print(f"\nfiles installed by more than one package ({len(res['overlaps'])}):", file=out)
        for p, pkgs in res["overlaps"].items():
            print(f"  {p}: {', '.join(pkgs)}", file=out)
    if res["reports"]:
        print("\nNOT DECLARED:", file=out)
        for pkg, libs in sorted(res["reports"].items()):
            for lib, (prov, paths) in sorted(libs.items()):
                print(f"  {pkg} ({res['trees'][pkg]}): {lib} from {' or '.join(prov)}, not among its dependencies "
                      f"({where(paths)})", file=out)
    else:
        print("\nevery library linked is declared", file=out)


def tool(name):
    t = shutil.which(name)
    if not t:
        raise Fail(f"{name} not found")
    return t


def self_test():
    cc, readelf = tool("cc"), tool("readelf")
    checks = []

    def check(name, ok, res):
        checks.append(name)
        if not ok:
            print(f"deps-audit self-test: FAIL: {name}: {res}", file=sys.stderr)
            sys.exit(1)

    with tempfile.TemporaryDirectory() as t:
        tg = os.path.join(t, "target")
        os.makedirs(os.path.join(tg, "usr/lib"))
        os.makedirs(os.path.join(tg, "usr/bin"))
        os.makedirs(os.path.join(t, "build"))
        for n, src in (("foo.c", "int foo(void) { return 1; }\n"),
                       ("bar.c", "int foo(void);\nint main(void) { return foo(); }\n")):
            with open(os.path.join(t, n), "w") as f:
                f.write(src)
        lib, bar = os.path.join(tg, "usr/lib/libfoo.so.1"), os.path.join(tg, "usr/bin/bar")
        subprocess.run([cc, "-shared", "-fPIC", "-Wl,-soname,libfoo.so.1", "-o", lib, os.path.join(t, "foo.c")],
                       check=True)
        subprocess.run([cc, "-o", bar, os.path.join(t, "bar.c"), lib], check=True)
        libs = needed(readelf, bar)
        check("bar links libfoo.so.1", "libfoo.so.1" in libs, libs)
        libs = sorted(set(libs + needed(readelf, lib)))
        with open(os.path.join(tg, "usr/bin/bar.sh"), "w") as f:
            f.write("#!/bin/sh\n")
        # the host's C library comes from "toolchain", as the target's from toolchain-external-bootlin
        lines = ["foo,./usr/lib/libfoo.so.1", "bar,./usr/bin/bar", "bar,./usr/bin/bar.sh"]
        lines += [f"toolchain,./lib/{n}" for n in libs if n != "libfoo.so.1"]
        lst = os.path.join(t, "packages-file-list.txt")

        def run(deps, allowed=None, extra=(), pkgdir="/x/buildroot/package/bar", times=(), ppd=False):
            info = {
                "toolchain": {"type": "target", "virtual": True, "package_dir": "package/toolchain",
                              "dependencies": []},
                "foo": {"type": "target", "virtual": False, "package_dir": "/x/kiwami/package/foo",
                        "dependencies": ["toolchain"]},
                "libfoo": {"type": "target", "virtual": True, "package_dir": "package/libfoo",
                           "dependencies": ["toolchain", "foo"]},
                "bar": {"type": "target", "virtual": False, "package_dir": pkgdir,
                        "dependencies": ["toolchain"] + deps},
                "qux": {"type": "target", "virtual": False, "package_dir": "/x/kiwami/package/qux",
                        "dependencies": ["toolchain"]},
            }
            with open(lst, "w") as f:
                f.write("\n".join(lines + list(extra)) + "\n")
            with open(os.path.join(t, "build", "build-time.log"), "w") as f:
                f.write("".join(f"{s}:{e:5}:{step:20}: {p}\n" for s, e, step, p in times))
            conf, inst = read_times(t)
            kept, moved = (read_list(lst), {}) if ppd else attribute(read_list(lst), conf, inst)
            files, owners = index(kept)
            return audit(info, files, owners, tg, readelf, allowed or {}), incremental(info, files, inst), moved

        res, why, _ = run([])
        check("bar without foo among its dependencies: reported",
              list(res["reports"]) == ["bar"] and res["reports"]["bar"]["libfoo.so.1"] == (["foo"], ["usr/bin/bar"])
              and not res["unprovided"] and res["elf"] == 2 and not why, res)
        res, _, _ = run(["foo"])
        check("with foo: not reported", not res["reports"] and not res["allowed"], res)
        res, _, _ = run(["libfoo"])
        check("through a virtual package that depends on foo: not reported", not res["reports"], res)
        res, _, _ = run([], allowed={"bar": "a test"})
        check("bar allowed: not reported, listed as allowed", not res["reports"] and list(res["allowed"]) == ["bar"],
              res)
        res, _, _ = run([], pkgdir="package/bar")
        check("a package of Buildroot's own: audited, and named so",
              list(res["reports"]) == ["bar"] and res["packages"] == {"Buildroot's": 1, "the fork's": 2}, res)
        lines.remove("foo,./usr/lib/libfoo.so.1")
        res, _, _ = run([])
        check("a library no package provides: listed apart, not reported",
              not res["reports"] and res["unprovided"] == {"bar": {"libfoo.so.1": ["usr/bin/bar"]}}, res)
        lines.append("foo,./usr/lib/libfoo.so.1")
        res, why, _ = run(["foo"], extra=["toolchain,./usr/bin/bar"])
        check("a file in two packages' lists: listed apart",
              res["overlaps"] == {"usr/bin/bar": ["bar", "toolchain"]} and not res["reports"] and not why, res)
        res, why, _ = run(["foo"], extra=["gone,./usr/share/gone"])
        check("a package no longer built in the lists: not a fresh output", len(why) == 1 and "gone" in why[0], why)
        res, why, _ = run(["foo"], times=[(1, "end", "install-target", "bar"), (2, "end", "install-target", "bar")])
        check("a package installed twice: not a fresh output", len(why) == 1 and "bar" in why[0], why)
        # ldacbt's shape: qux configured early, foo and bar installed, then qux's install; qux's list holds theirs
        win = [(1, "start", "configure", "qux"), (2, "end", "install-target", "foo"),
               (3, "end", "install-target", "bar"), (4, "end", "install-target", "qux")]
        held = ["qux,./usr/lib/libfoo.so.1", "qux,./usr/bin/bar", "qux,./usr/share/qux"]
        res, why, moved = run(["foo"], extra=held, times=win)
        check("one package at a time, others' files in a list's window: theirs",
              not res["reports"] and not res["overlaps"] and moved == {"qux": [2, {"foo", "bar"}]} and not why,
              (res, moved))
        # qux's list holding bar (which links foo's library, and qux doesn't declare foo)
        held = ["qux,./usr/bin/bar", "qux,./usr/share/qux"]
        res, _, moved = run(["foo"], extra=held, times=win, ppd=True)
        check("the same with per-package directories: the package's own, reported and listed apart",
              list(res["reports"]) == ["qux"] and res["overlaps"] == {"usr/bin/bar": ["bar", "qux"]} and not moved,
              (res, moved))
        res, _, moved = run(["foo"], extra=held, times=[(1, "end", "install-target", "bar"),
                                                         (2, "start", "configure", "qux"),
                                                         (3, "end", "install-target", "foo"),
                                                         (4, "end", "install-target", "qux")])
        check("a file installed before the window: the package's own, reported and listed apart",
              list(res["reports"]) == ["qux"] and res["overlaps"] == {"usr/bin/bar": ["bar", "qux"]} and not moved,
              (res, moved))
    print(f"deps-audit self-test: {len(checks)} checks passed")


def main(argv):
    if argv == ["--self-test"]:
        try:
            self_test()
        except Fail as e:
            log(str(e))
            return 2
        return 0
    if len(argv) not in (1, 3) or (len(argv) == 3 and argv[1] != "--info"):
        print("usage: deps-audit.py <output dir> [--info <show-info.json>]\n       deps-audit.py --self-test",
              file=sys.stderr)
        return 2
    out = argv[0]
    try:
        readelf = tool("readelf")
        if len(argv) == 3:
            with open(argv[2]) as f:
                info = json.load(f)
        else:
            r = subprocess.run(["make", "-s", "-C", out, "show-info"], capture_output=True, text=True)
            if r.returncode:
                raise Fail(f"make -C {out} show-info failed:\n{r.stderr}")
            info = json.loads(r.stdout)
        with open(os.path.join(out, ".config")) as f:
            ppd = "BR2_PER_PACKAGE_DIRECTORIES=y\n" in f
        lines = read_list(os.path.join(out, "build", "packages-file-list.txt"))
        conf, inst = read_times(out)
        kept, moved = (lines, {}) if ppd else attribute(lines, conf, inst)
        files, owners = index(kept)
        res = audit(info, files, owners, os.path.join(out, "target"), readelf)
    except OSError as e:
        log(f"{e.filename}: {e.strerror}" if e.filename else str(e))
        return 2
    except (Fail, ValueError) as e:
        log(str(e))
        return 2
    print(f"output: {os.path.abspath(out)} ({'with' if ppd else 'without'} per-package directories)")
    write(res, incremental(info, files, inst), moved)
    return 1 if res["reports"] else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
