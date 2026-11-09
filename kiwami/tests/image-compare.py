#!/usr/bin/env python3
# Compares two builds' SD card images and update packages by what they hold. kiwami/tests/image-compare.sh finds the
# files and the tools and runs this with an output's own python3, the image's Python, so the image's .pyc files can be
# read; the output's host/bin is on PATH for unsquashfs, lz4, mdir and mcopy:
#   image-compare.py --work <dir> --old <img.gz> <update.tar> <build path> --new <img.gz> <update.tar> <build path>
#   image-compare.py --self-test     the rules on made-up data, with the running Python and no other tool
# Two of today's builds of one commit already differ (BR2_REPRODUCIBLE is off). What differs that way is excused, and
# listed by class with the files it excused; anything else is a difference:
#   build dates  the kernel's, U-Boot's and BusyBox's banners, asctime()'s (OpenSSL), __DATE__ and __TIME__ (apart or
#                in one string), in ELF files, the kernel and U-Boot
#   random IDs   the GPT's disk and partition GUIDs and its CRC32s, the FAT's volume ID
#   times        the squashfs's creation time and file times, the FAT's and the tar's file times, the FIT's timestamp,
#                the source's mtime in a .pyc's header (Buildroot's .pyc files are timestamp-based), the mtimes in the
#                kernel's built-in initramfs (cpio)
#   build paths  the output's path (<anything>/<board>/ then build/, host/, per-package/<pkg>/, ...) in text files and
#                .pyc files (normalised; in a .pyc also the positions a longer path moves), at equal length elsewhere;
#                and an AArch64 ELF file linked around a path of another length (libtool's RPATH to the staging, which
#                Buildroot blanks to X's, or an include path in .rodata, longer under per-package directories): its
#                sections moved after it, and every address of theirs it holds moved with them (relinked())
#   bookkeeping  Buildroot's .files-list*, .stamp_* and .applied_patches_list copied into the image: their contents
#   derived      U-Boot's loader header and FIT hashes, GNU build IDs (the kernel's), the squashfs's size: of contents
#                compared on their own
# A run of differing bytes is excused only inside a span a rule's pattern matches at the same place in both files, or
# a span the structure names (a FIT's timestamp, a hash). Exit 0: the same contents; 1: different; 2: no comparison.
import bisect
import gzip
import hashlib
import marshal
import os
import re
import shutil
import struct
import subprocess
import sys
import tarfile
import types
import zlib

CLASSES = ("build dates", "random IDs", "times", "build paths", "bookkeeping", "derived")
SECTOR = 512
UBOOT_AT = 32 * 1024            # where the RK3568's boot ROM reads the loader (genimage.cfg.in's u-boot partition)
WINDOW = 1024                   # how far around a differing run a rule's match is looked for

MONTH = r"(?:Jan|Feb|Mar|Apr|May|Jun|Jul|Aug|Sep|Oct|Nov|Dec)"
WDAY = r"(?:Mon|Tue|Wed|Thu|Fri|Sat|Sun)"
TZ = r"(?:[A-Z]{2,5}|[+-]\d{2,4})"
DATES = [re.compile(p.encode()) for p in (
    WDAY + " " + MONTH + r" [ 0-3]\d \d\d:\d\d:\d\d " + TZ + r" \d{4}",   # date(1): the kernel's UTS_VERSION
    r"\(" + MONTH + r" [ 0-3]\d \d{4} - \d\d:\d\d:\d\d [+-]\d{4}\)",       # U-Boot's (Oct 26 2026 - 19:02:12 +0200)
    r"\(\d{4}-\d\d-\d\d \d\d:\d\d:\d\d " + TZ + r"\)",                     # BusyBox's (2026-10-26 18:54:36 EET)
    WDAY + " " + MONTH + r" [ 0-3]\d \d\d:\d\d:\d\d \d{4}",                # asctime(): OpenSSL's "built on: ... UTC"
    MONTH + r" [ 0-3]\d \d{4},? [0-2]\d:[0-5]\d:[0-5]\d",                   # __DATE__ " " __TIME__ in one (PCSX's)
    MONTH + r" [ 0-3]\d \d{4}",                                            # __DATE__
    r"(?<![\d:])[0-2]\d:[0-5]\d:[0-5]\d(?=\x00)",                          # __TIME__, a string of its own
)]
# where the dates are excused, two things that follow from a build's time: the GNU build ID note's SHA-1 (the kernel
# links with --build-id: a hash of what it links, dates included), and the mtime field of a cpio newc header (the
# kernel's built-in initramfs: magic, then ino, mode, uid, gid, nlink, mtime, 8 hex digits each)
BUILD_ID = re.compile(rb"(?<=\x04\x00\x00\x00\x14\x00\x00\x00\x03\x00\x00\x00GNU\x00)[\x00-\xff]{20}")
CPIO_MTIME = re.compile(rb"(?<=070701[0-9A-Fa-f]{40})[0-9A-Fa-f]{8}")
# an output's path: <anything>/<board>/ followed by one of Buildroot's folders (under per-package/, a package's)
COMP = r"[^/\x00-\x20\"'<>:;=,()\[\]{}|*?\\]+"
OUT = r"(?:/" + COMP + r")+?/(?:rgds-plus|rgds|universal)/"
SUB = r"(?:build|host|per-package|target|staging|images)/"
PATH_B = re.compile((OUT + SUB).encode())
NORM_B = re.compile((OUT + r"(?:per-package/" + COMP + r"/)?(?=" + SUB + ")").encode())
NORM_S = re.compile(OUT + r"(?:per-package/" + COMP + r"/)?(?=" + SUB + ")")
BLANKED = re.compile(rb"\x00X{20,}\x00")
BOOKKEEPING = re.compile(r"^(?:\.files-list.*\.(?:before|txt)|\.stamp_.*|\.applied_patches_list)$")
LISTING = re.compile(r"^(\S{10}) (\d+)/(\d+) +(\d+|\d+, *\d+) (\d{4}-\d\d-\d\d \d\d:\d\d(?::\d\d)?) squashfs-root(/.*)?$")


class Fail(Exception):
    """Something that stops the comparison (exit 2)."""


def log(msg):
    print("image-compare: " + msg, file=sys.stderr, flush=True)


class Report:
    def __init__(self, paths_differ=False):
        self.paths_differ = paths_differ   # the two builds' paths differ in length: binaries holding them can't match
        self.parts = []
        self.excused = {c: [] for c in CLASSES}
        self.diffs = []
        self.counts = {}
        self.cur = set()

    def excuse(self, cls, where):
        self.cur.add(cls)
        if where not in self.excused[cls]:
            self.excused[cls].append(where)

    def count(self, cls, label):
        """An excuse that many files share, listed once with their count."""
        self.cur.add(cls)
        self.counts[label] = self.counts.get(label, 0) + 1
        if label not in self.excused[cls]:
            self.excused[cls].append(label)

    def differ(self, where, what):
        self.diffs.append(f"{where}: {what}")

    def part(self, name, fn, *args):
        self.cur, n = set(), len(self.diffs)
        r = fn(self, *args)
        state = "same" if len(self.diffs) == n else "DIFFERENT"
        why = [c for c in CLASSES if c in self.cur]
        self.parts.append(f"{name}: {state}" + (f" ({', '.join(why)})" if why else ""))
        return r

    def write(self, out):
        for p in self.parts:
            print(p, file=out)
        for c in CLASSES:
            if self.excused[c]:
                print(f"\n{c}:", file=out)
                for w in self.excused[c]:
                    print("  " + w + (f" ({self.counts[w]} files)" if w in self.counts else ""), file=out)
        if self.diffs:
            print("\nDIFFERENT:", file=out)
            for d in self.diffs:
                print("  " + d, file=out)
        else:
            print("\nsame contents", file=out)


# --- runs of differing bytes and what excuses them

def runs(a, b, n=None):
    """The runs (start, end) of differing bytes in a[:n] and b[:n]."""
    n = min(len(a), len(b)) if n is None else n
    start, off, C = None, 0, 4096
    while off < n:
        ca, cb = a[off:min(off + C, n)], b[off:min(off + C, n)]
        if ca == cb:
            if start is not None:
                yield start, off
                start = None
        else:
            for i in range(len(ca)):
                if ca[i] != cb[i]:
                    if start is None:
                        start = off + i
                elif start is not None:
                    yield start, off + i
                    start = None
        off += C
    if start is not None:
        yield start, n


def first_diff(a, b):
    for s, _ in runs(a, b):
        return s
    return min(len(a), len(b))


def cover(a, b, s, e, rules, spans):
    """The (class, label) of the span or rule match covering a[s:e] at the same place in a and b, or None."""
    for ms, me, cls, label in spans:
        if ms <= s and e <= me:
            return cls, label
    for cls, rx in rules:
        for m in rx.finditer(a, max(0, s - WINDOW), min(len(a), e + WINDOW)):
            if m.start() > s:
                break
            if m.end() >= e:
                mb = rx.match(b, m.start(), min(len(b), e + WINDOW))
                if mb and mb.end() == m.end():
                    return cls, None
    return None


def excuse_runs(a, b, rules, spans=()):
    """Equal-length a and b: the (class, label)s that excuse every differing run, and the first run none does."""
    used = set()
    for s, e in runs(a, b):
        c = cover(a, b, s, e, rules, spans)
        if c is None:
            return used, s
        used.add(c)
    return used, None


DATE_RULES = [("build dates", rx) for rx in DATES] + [("derived", BUILD_ID), ("times", CPIO_MTIME)]
PATH_RULES = [("build paths", PATH_B)]


def normalise(data):
    return NORM_B.sub(b"<OUT>/", data)


# a code object's fields but its line and column positions (co_firstlineno, co_linetable), which a longer path moves
# (the columns after it on its line; pprint's wrapping of _sysconfigdata's long values); co_code's bytes as they are
RAW_CODE = ("co_code", "co_exceptiontable")
CODE_FIELDS = ("co_argcount", "co_posonlyargcount", "co_kwonlyargcount", "co_nlocals", "co_stacksize", "co_flags",
               "co_code", "co_consts", "co_names", "co_varnames", "co_freevars", "co_cellvars", "co_filename",
               "co_name", "co_qualname", "co_exceptiontable")


def code_tree(o, norm):
    """An unmarshalled .pyc's objects as a tree without line and column positions; norm: build paths normalised."""
    if isinstance(o, types.CodeType):
        return ("code",) + tuple((f, getattr(o, f) if f in RAW_CODE else code_tree(getattr(o, f), norm))
                                 for f in CODE_FIELDS if hasattr(o, f))
    if isinstance(o, str):
        return ("str", NORM_S.sub("<OUT>/", o) if norm else o)
    if isinstance(o, bytes):
        return ("bytes", normalise(o) if norm else o)
    if isinstance(o, (tuple, list)):
        return (type(o).__name__,) + tuple(code_tree(x, norm) for x in o)
    if isinstance(o, (frozenset, set)):
        return (type(o).__name__, frozenset(code_tree(x, norm) for x in o))
    if isinstance(o, dict):
        return ("dict",) + tuple((code_tree(k, norm), code_tree(v, norm)) for k, v in o.items())
    return (type(o).__name__, repr(o))


def compare_pyc(rep, where, a, b):
    """A .pyc: 16 bytes of header (magic, flags, then the source's mtime and size, or its hash), then marshalled code.
    The code may differ in build paths, and then in the positions they move; the header in the source's mtime (a
    time), and with a build path also in its size or hash."""
    if a[:8] != b[:8]:
        rep.differ(where, "magic or flags differ")
        return
    try:
        ca, cb = marshal.loads(a[16:]), marshal.loads(b[16:])
    except (ValueError, EOFError, TypeError) as e:
        rep.differ(where, f"can't be read by this Python {sys.version.split()[0]} ({e})")
        return
    paths = False
    if a[16:] != b[16:]:
        if code_tree(ca, False) == code_tree(cb, False):
            rep.differ(where, "only its line or column positions differ: its source moved")
            return
        if code_tree(ca, True) != code_tree(cb, True):
            rep.differ(where, f"its code differs (first byte at {first_diff(a, b):#x})")
            return
        paths = True
        rep.excuse("build paths", where)
    hashed = int.from_bytes(a[4:8], "little") & 1
    if not hashed and a[8:12] != b[8:12]:
        rep.count("times", f"{where.split(': ', 1)[0]}: .pyc files' source times")
    if (a[8:16] if hashed else a[12:16]) != (b[8:16] if hashed else b[12:16]) and not paths:
        rep.differ(where, "its source's " + ("hash" if hashed else "size") + " differs, its code doesn't")


def compare_file(rep, where, a, b, dates=None):
    """A file's contents under the file rules. dates: build dates excused (the kernel); None: in ELF files only."""
    if a == b:
        return
    if BOOKKEEPING.match(where.rsplit("/", 1)[-1]):
        rep.excuse("bookkeeping", where)
        return
    if where.endswith(".pyc"):
        compare_pyc(rep, where, a, b)
        return
    if b"\0" not in a and b"\0" not in b and normalise(a) == normalise(b):
        rep.excuse("build paths", where)
        return
    if dates is None:
        dates = a[:4] == b[:4] == b"\x7fELF"
    rules = (DATE_RULES if dates else []) + PATH_RULES
    if len(a) == len(b):
        used, bad = excuse_runs(a, b, rules)
        if bad is None:
            for cls, _ in used:
                rep.excuse(cls, where)
            return
        what = f"differs at {bad:#x}"
    else:
        what = f"sizes {len(a)} and {len(b)}, first difference at {first_diff(a, b):#x}"
    if a[:4] == b[:4] == b"\x7fELF":
        try:
            resized = relinked(a, b, rules)
            rep.excuse("build paths", f"{where} (linked around a build path of another length: {', '.join(resized)})")
            return
        except Relink as e:
            what += f"; not the same file linked around a longer path: {e}"
    if rep.paths_differ and (PATH_B.search(a) or PATH_B.search(b)):
        what += "; it holds a build path, and the two builds' paths differ in length"
    elif rep.paths_differ and (BLANKED.search(a) or BLANKED.search(b)):
        what += "; it holds a path blanked to X's (an RPATH), and the two builds' paths differ in length"
    rep.differ(where, what)


# --- ELF files linked again around a build path of another length
#
# A path the linker writes into a file (libtool's -rpath to the staging's usr/lib, which Buildroot then blanks to X's;
# an include path in an assert's message) is longer under per-package directories (per-package/<pkg>/ more) or in a
# longer output path. The section holding it grows, the sections after it move by a few bytes, and every address the
# file holds of what moved changes with them: calls and branches, adrp's page and the offset its add, ldr or str
# adds, symbols, relocations, .dynamic, GOT and pointer words, .eh_frame's. A pair is accepted when each section is
# the same but those addresses, each of them the address the old file's points to, moved with its section. Anything
# else stays a difference.

SHT_PROGBITS, SHT_SYMTAB, SHT_STRTAB, SHT_RELA, SHT_HASH, SHT_DYNAMIC, SHT_NOTE, SHT_NOBITS = 1, 2, 3, 4, 5, 6, 7, 8
SHT_DYNSYM, SHT_GNU_HASH, SHT_VERDEF, SHT_VERNEED, SHT_VERSYM = 11, 0x6ffffff6, 0x6ffffffd, 0x6ffffffe, 0x6fffffff
SHF_ALLOC, SHF_EXECINSTR, SHF_TLS = 2, 4, 0x400
EM_AARCH64 = 183
R_RELATIVE, R_IRELATIVE = 1027, 1032                        # R_AARCH64_*: the addend is an address
DT_ADDRESSES = {3, 4, 5, 6, 7, 12, 13, 17, 23, 25, 26, 32, 36, 0x6ffffef5, 0x6ffffff0, 0x6ffffffc, 0x6ffffffe}
DT_STRINGS = {1, 14, 15, 29, 0x7ffffffd, 0x7fffffff}        # NEEDED, SONAME, RPATH, RUNPATH, AUXILIARY, FILTER
DT_STRSZ = 10
# what a path holder may differ in: an output's path (normalised), per-package/<pkg>/ before host/ or target/ (also in
# a relative path), and in a string table a path blanked to X's (the longer one has its extra X's after a NUL)
PPD_CUT = re.compile(rb"per-package/" + COMP.encode() + rb"/(?=(?:host|target|build|staging)/)")
XRUNS = re.compile(rb"X{8,}(?:\x00X+(?=\x00))*")
HOLDER_RX = re.compile(rb"(?P<out>" + NORM_B.pattern + rb")|(?P<ppd>" + PPD_CUT.pattern + rb")")
HOLDER_STR_RX = re.compile(HOLDER_RX.pattern + rb"|(?P<x>" + XRUNS.pattern + rb")")
HOLDER_REPL = {"out": b"<OUT>/", "ppd": b"", "x": b"X"}


class Relink(Exception):
    """Why two ELF files aren't the same file linked again."""


class Sec:
    def __init__(self, d, name, h):
        (_, self.type, self.flags, self.addr, self.off, self.size, self.link, self.info, self.align,
         self.entsize) = h
        self.name = name
        self.data = b"" if self.type == SHT_NOBITS else d[self.off:self.off + self.size]
        self.edits = []                     # a path holder's (start, end, normalised length), set by relinked()


class Elf:
    def __init__(self, d):
        if len(d) < 64 or d[:4] != b"\x7fELF" or d[4] != 2 or d[5] != 1:
            raise Relink("not a 64-bit little-endian ELF file")
        self.d = d
        (self.type, self.machine, self.version, self.entry, self.phoff, self.shoff, self.flags, self.ehsize,
         self.phentsize, self.phnum, self.shentsize, self.shnum, self.shstrndx) = struct.unpack_from("<HHIQQQIHHHHHH",
                                                                                                    d, 16)
        if self.machine != EM_AARCH64:
            raise Relink(f"machine {self.machine}, not AArch64")
        try:
            self.ph = [struct.unpack_from("<IIQQQQQQ", d, self.phoff + i * self.phentsize) for i in range(self.phnum)]
            sh = [struct.unpack_from("<IIQQQQIIQQ", d, self.shoff + i * self.shentsize) for i in range(self.shnum)]
            names = sh[self.shstrndx][4]
            self.secs = [Sec(d, d[names + h[0]:d.index(b"\0", names + h[0])].decode(errors="replace"), h) for h in sh]
        except (struct.error, ValueError, IndexError):
            raise Relink("its headers can't be read")


def holder_edits(data, strtab):
    """A path holder's spans that normalising replaces, as (start, end, normalised length), and the normalised data."""
    edits, out, pos = [], [], 0
    for m in (HOLDER_STR_RX if strtab else HOLDER_RX).finditer(data):
        r = HOLDER_REPL[m.lastgroup]
        edits.append((m.start(), m.end(), len(r)))
        out += [data[pos:m.start()], r]
        pos = m.end()
    out.append(data[pos:])
    return edits, b"".join(out)


def holder_pair(da, db, strtab):
    """A section in two versions that may differ in a build path: each one's edits ((start, end, normalised length))
    when normalising makes them equal, else ([], []). Outside a string table, the string holding the path may also
    have another number of NULs after it (strings aligned to 8: an assert's file name in .rodata)."""
    if strtab:
        ea, na = holder_edits(da, True)
        eb, nb = holder_edits(db, True)
        return (ea, eb) if (ea or eb) and na == nb else ([], [])
    ta, tb = re.split(rb"(\x00+)", da), re.split(rb"(\x00+)", db)
    if len(ta) != len(tb):
        return [], []
    ea, eb, pa, pb, changed = [], [], 0, 0, False
    for k, (x, y) in enumerate(zip(ta, tb)):
        if k % 2 == 0:
            changed = x != y
            if changed:
                ex, nx = holder_edits(x, False)
                ey, ny = holder_edits(y, False)
                if nx != ny or not (ex or ey):
                    return [], []
                ea += [(pa + s, pa + e, n) for s, e, n in ex]
                eb += [(pb + s, pb + e, n) for s, e, n in ey]
        elif len(x) != len(y):
            if not changed:
                return [], []
            ea.append((pa, pa + len(x), 1))
            eb.append((pb, pb + len(y), 1))
        pa, pb = pa + len(x), pb + len(y)
    return ea, eb


def to_norm(edits, o):
    """An offset in a path holder in its normalised data; None inside a replaced span."""
    shift = 0
    for s, e, n in edits:
        if o <= s:
            break
        if o < e:
            return None
        shift += (e - s) - n
    return o - shift


def from_norm(edits, p):
    shift = 0
    for s, e, n in edits:
        ns = s - shift
        if p <= ns:
            break
        if p < ns + n:
            return None
        shift += (e - s) - n
    return p + shift


class Moves:
    """Where each of the old file's sections went in the new one."""

    def __init__(self, ea, eb):
        self.pairs = list(zip(ea.secs, eb.secs))
        self.alloc = sorted(((sa, sb) for sa, sb in self.pairs if sa.flags & SHF_ALLOC and sa.size
                             and not (sa.flags & SHF_TLS and sa.type == SHT_NOBITS)), key=lambda p: p[0].addr)

    @staticmethod
    def within(sa, sb, o):
        """An offset in old section sa at its new place in sb (None inside a replaced span)."""
        if not sa.edits and not sb.edits:
            return o
        p = to_norm(sa.edits, o)
        return None if p is None else from_norm(sb.edits, p)

    def addr(self, x, ends=True):
        """An address of the old file's in the new one: inside a section, or (ends) at a section's end; else None."""
        for sa, sb in self.alloc:
            if sa.addr <= x < sa.addr + sa.size:
                o = self.within(sa, sb, x - sa.addr)
                return None if o is None else sb.addr + o
        if ends:
            for sa, sb in self.alloc:
                if x == sa.addr + sa.size:
                    return sb.addr + sb.size
        return None

    def around(self, x, attr, end=False):
        """An address (attr "addr") or file offset ("off") for the program headers: in a section, at its end (first,
        for a segment's end), or in a gap that the section before it (by its end) and the one after it (by its start)
        moved across alike."""
        spans = []
        for sa, sb in self.pairs:
            if not sa.flags & SHF_ALLOC or sa.flags & SHF_TLS and sa.type == SHT_NOBITS:
                continue
            na = sa.size if attr == "addr" or sa.type != SHT_NOBITS else 0
            nb = sb.size if attr == "addr" or sb.type != SHT_NOBITS else 0
            spans.append((getattr(sa, attr), na, getattr(sb, attr), nb, sa, sb))
        if end:
            for a, na, b, nb, sa, sb in spans:
                if na and x == a + na:
                    return b + nb
        for a, na, b, nb, sa, sb in spans:
            if a <= x < a + na:
                o = self.within(sa, sb, x - a)
                return None if o is None else b + o
        for a, na, b, nb, sa, sb in spans:
            if x == a + na:
                return b + nb
        before = [(a + na, b + nb) for a, na, b, nb, _, _ in spans if a + na < x]
        after = [(a, b) for a, _, b, _, _, _ in spans if a > x]
        deltas = set()
        if before:
            e, f = max(before)
            deltas.add(f - e)
        if after:
            e, f = min(after)
            deltas.add(f - e)
        return x + deltas.pop() if len(deltas) == 1 else None


def sext(v, bits):
    return v - (1 << bits) if v >> (bits - 1) & 1 else v


def pcrel(w, pc):
    """A PC-relative instruction: (kind, its bits but the offset, its target), else None."""
    if w & 0x7C000000 == 0x14000000:                                        # b, bl
        return "b", w & 0xFC000000, pc + sext(w & 0x3FFFFFF, 26) * 4
    if w & 0xFF000000 == 0x54000000 or w & 0x7E000000 == 0x34000000 or w & 0x3B000000 == 0x18000000:
        return "b19", w & 0xFF00001F, pc + sext(w >> 5 & 0x7FFFF, 19) * 4    # b.cond, cbz, cbnz, ldr (literal)
    if w & 0x7E000000 == 0x36000000:                                        # tbz, tbnz
        return "b14", w & 0xFFF8001F, pc + sext(w >> 5 & 0x3FFF, 14) * 4
    if w & 0x1F000000 == 0x10000000:                                        # adr, adrp
        imm = sext((w >> 5 & 0x7FFFF) << 2 | w >> 29 & 3, 21)
        if w & 0x80000000:
            return "adrp", w & 0x9F00001F, (pc & ~0xFFF) + imm * 4096
        return "adr", w & 0x9F00001F, pc + imm
    return None


def lo12(w):
    """An add (immediate, unshifted) or a load or store at an unsigned offset: (its bits but the offset, base register,
    offset, the general register it writes or None), else None."""
    if w & 0x7FC00000 == 0x11000000:                                        # add, 32 or 64 bits, no shift
        return w & 0xFFC003FF, w >> 5 & 31, w >> 10 & 0xFFF, w & 31
    if w & 0x3B000000 == 0x39000000:                                        # ldr/str (unsigned offset)
        size, simd, opc = w >> 30, w >> 26 & 1, w >> 22 & 3
        scale = 4 if simd and opc & 2 else size
        load = not simd and opc != 0
        return w & 0xFFC003FF, w >> 5 & 31, (w >> 10 & 0xFFF) << scale, (w & 31) if load else None
    return None


def written(w):
    """The general registers an instruction writes, as far as tracking an adrp's page needs to know."""
    if w & 0xFC000000 == 0x94000000 or w & 0xFFFFFC1F == 0xD63F0000:         # bl, blr: x30, and what callees clobber
        return set(range(19)) | {30}
    if w & 0x1C000000 == 0x10000000 or w & 0x0E000000 == 0x0A000000:        # data processing
        return {w & 31}
    if w & 0x0E000000 == 0x0E000000:                                        # SIMD and FP (fmov, umov to a register)
        return {w & 31}
    if w & 0xFFF00000 == 0xD5300000:                                        # mrs
        return {w & 31}
    if w & 0x0A000000 == 0x08000000:                                        # loads and stores
        regs = set()
        if w & 0x3B200C00 in (0x38000400, 0x38000C00) or w & 0x3A800000 == 0x28800000 or w & 0x3B800000 == 0x29800000:
            regs.add(w >> 5 & 31)                                           # written back
        simd = w >> 26 & 1
        if w & 0x3B000000 == 0x28000000 or w & 0x3B000000 == 0x29000000:    # pairs
            if w >> 22 & 1 and not simd:
                regs |= {w & 31, w >> 10 & 31}
        elif w & 0x3F000000 == 0x08000000:                                  # exclusives
            regs |= {w & 31, w >> 16 & 31}
        elif w & 0x3B200C00 == 0x38200000:                                  # atomics
            regs.add(w & 31)
        elif not simd and w >> 22 & 3:
            regs.add(w & 31)
        return regs
    return set()


def check_code(mv, sa, sb, funcs):
    """Code: each instruction the same, but a PC-relative one's offset and the offset an add, ldr or str adds to an
    adrp's page, each pointing where the old one's target went. An offset's adrp is the last one to its register
    before it, else one to its register in the same function (funcs: .eh_frame's ranges; the compiler moves an adrp
    out of a loop, or lays it out after a use it branches back to). An adrp whose page moved is used by an offset
    that moved with it."""
    n = sa.size - sa.size % 4
    wa_all = struct.unpack_from(f"<{n // 4}I", sa.data)
    wb_all = struct.unpack_from(f"<{n // 4}I", sb.data)
    starts = [f[0] for f in funcs]

    def func(pc):
        k = bisect.bisect_right(starts, pc) - 1
        return k if k >= 0 and pc < funcs[k][1] else -1
    adrps = {}                                                  # (function, register): {(old page, new page)}
    for k, wa in enumerate(wa_all):
        p = pcrel(wa, sa.addr + 4 * k)
        if p and p[0] == "adrp":
            q = pcrel(wb_all[k], sb.addr + 4 * k)
            if not q or q[:2] != p[:2]:
                raise Relink(f"{sa.name}+{4 * k:#x}: {wa:08x} and {wb_all[k]:08x}")
            adrps.setdefault((func(sa.addr + 4 * k), wa & 31), set()).add((p[2], q[2]))
    used = set()                                                # (function, register, (old page, new page))
    pages = {}                                                  # register: (old page, new page), the last adrp's
    for k, (wa, wb) in enumerate(zip(wa_all, wb_all)):
        pa, pb = sa.addr + 4 * k, sb.addr + 4 * k
        p = pcrel(wa, pa)
        if p:
            q = pcrel(wb, pb)
            if not q or q[:2] != p[:2]:
                raise Relink(f"{sa.name}+{4 * k:#x}: {wa:08x} and {wb:08x}")
            if p[0] == "adrp":
                pages[wa & 31] = (p[2], q[2])
                continue
            t = mv.addr(p[2])
            if t is None and wa != wb or t is not None and t != q[2]:
                raise Relink(f"{sa.name}+{4 * k:#x}: {wa:08x} and {wb:08x} don't point to the same place")
            literal = wa & 0x3B000000 == 0x18000000
            for r in ({wa & 31} if not wa >> 26 & 1 else set()) if literal else written(wa):
                pages.pop(r, None)
            continue
        c = lo12(wa)
        if c and wa != wb:
            d = lo12(wb)
            if not d or d[:2] != c[:2]:
                raise Relink(f"{sa.name}+{4 * k:#x}: {wa:08x} and {wb:08x}")
            g = func(pa)
            cands = ([pages[c[1]]] if c[1] in pages else []) + sorted(adrps.get((g, c[1]), ()))
            for page in cands:
                if mv.addr(page[0] + c[2]) == page[1] + d[2]:
                    used.add((g, c[1], page))
                    break
            else:
                raise Relink(f"{sa.name}+{4 * k:#x}: {wa:08x} and {wb:08x}: no adrp to x{c[1]} makes them point to "
                             "the same place")
        elif wa != wb:
            raise Relink(f"{sa.name}+{4 * k:#x}: {wa:08x} and {wb:08x}")
        for r in written(wa):
            pages.pop(r, None)
    for (g, r), pairs in adrps.items():
        for page in pairs:
            if page[0] != page[1] and (g, r, page) not in used:
                raise Relink(f"{sa.name}: an adrp to another page ({page[0]:#x}, {page[1]:#x}) that no offset moved "
                             "with")
    if sa.data[n:] != sb.data[n:]:
        raise Relink(f"{sa.name}: its last bytes differ")


def u(fmt, d, o):
    return struct.unpack_from(fmt, d, o)[0]


def check_strings(mv, ia, oa, ob, what):
    """A string table offset (old section index ia) pointing at the same string in the new file."""
    sa, sb = mv.pairs[ia]
    if mv.within(sa, sb, oa) != ob:
        raise Relink(f"{what}: string offsets {oa:#x} and {ob:#x}")


def check_symbols(mv, sa, sb):
    for o in range(0, sa.size, 24):
        na, ia, oa, xa, va, za = struct.unpack_from("<IBBHQQ", sa.data, o)
        nb, ib, ob, xb, vb, zb = struct.unpack_from("<IBBHQQ", sb.data, o)
        if (ia, oa, xa, za) != (ib, ob, xb, zb):
            raise Relink(f"{sa.name}: symbol {o // 24} differs")
        check_strings(mv, sa.link, na, nb, f"{sa.name}: symbol {o // 24}'s name")
        if xa in (0, 0xFFF1, 0xFFF2) or 0 < xa < len(mv.pairs) and mv.pairs[xa][0].flags & SHF_TLS:
            if va != vb:
                raise Relink(f"{sa.name}: symbol {o // 24}'s value")
        elif xa < len(mv.pairs):
            s, t = mv.pairs[xa]
            w = None
            if va == s.addr + s.size:
                w = t.size
            elif s.addr <= va < s.addr + s.size:
                w = mv.within(s, t, va - s.addr)
            if w is None or t.addr + w != vb:
                raise Relink(f"{sa.name}: symbol {o // 24}'s value {va:#x} and {vb:#x}")
        elif va != vb:
            raise Relink(f"{sa.name}: symbol {o // 24}'s value")


def check_relocations(mv, sa, sb):
    for o in range(0, sa.size, 24):
        ra, ia, aa = struct.unpack_from("<QQq", sa.data, o)
        rb, ib, ab = struct.unpack_from("<QQq", sb.data, o)
        if ia != ib or mv.addr(ra, ends=False) != rb:
            raise Relink(f"{sa.name}: relocation {o // 24} at {ra:#x} and {rb:#x}")
        if (ia & 0xFFFFFFFF) in (R_RELATIVE, R_IRELATIVE):
            if mv.addr(aa) != ab:
                raise Relink(f"{sa.name}: relocation {o // 24}'s address {aa:#x} and {ab:#x}")
        elif aa != ab:
            raise Relink(f"{sa.name}: relocation {o // 24}'s addend")


def check_dynamic(mv, sa, sb):
    strtab = sa.link
    for o in range(0, sa.size, 16):
        ta, va = struct.unpack_from("<qQ", sa.data, o)
        tb, vb = struct.unpack_from("<qQ", sb.data, o)
        if ta != tb:
            raise Relink(f"{sa.name}: entry {o // 16}'s tag")
        if ta in DT_ADDRESSES and va:
            if mv.addr(va) != vb:
                raise Relink(f"{sa.name}: entry {o // 16} (tag {ta:#x}) {va:#x} and {vb:#x}")
        elif ta in DT_STRINGS:
            check_strings(mv, strtab, va, vb, f"{sa.name}: entry {o // 16}")
        elif ta == DT_STRSZ:
            if (va, vb) != (mv.pairs[strtab][0].size, mv.pairs[strtab][1].size):
                raise Relink(f"{sa.name}: DT_STRSZ isn't its string table's size")
        elif va != vb:
            raise Relink(f"{sa.name}: entry {o // 16} (tag {ta:#x})")


def check_versions(mv, sa, sb):
    """Version needs or definitions (sh_info of them): the same, but their names' string offsets."""
    need = sa.type == SHT_VERNEED
    size, cnt_at, name_at, aux_at, next_at = (16, 2, 4, 8, 12) if need else (20, 6, None, 12, 16)
    asize, aname_at, anext_at = (16, 8, 12) if need else (8, 0, 4)

    def same(xa, xb, at, what):
        if (xa[:at] + xa[at + 4:] if at is not None else xa) != (xb[:at] + xb[at + 4:] if at is not None else xb):
            raise Relink(f"{sa.name}: {what}")
        if at is not None:
            check_strings(mv, sa.link, u("<I", xa, at), u("<I", xb, at), f"{sa.name}: {what}'s name")
    o = 0
    for n in range(sa.info):
        ha, hb = sa.data[o:o + size], sb.data[o:o + size]
        same(ha, hb, name_at, f"entry {n}")
        a = o + u("<I", ha, aux_at)
        for k in range(u("<H", ha, cnt_at)):
            xa, xb = sa.data[a:a + asize], sb.data[a:a + asize]
            same(xa, xb, aname_at, f"entry {n}'s auxiliary {k}")
            if not u("<I", xa, anext_at):
                break
            a += u("<I", xa, anext_at)
        if not u("<I", ha, next_at):
            break
        o += u("<I", ha, next_at)


def uleb(d, o):
    v = shift = 0
    while True:
        b = d[o]
        o += 1
        v |= (b & 0x7F) << shift
        shift += 7
        if not b & 0x80:
            return v, o


PTR_SIZES = {0x00: 8, 0x02: 2, 0x03: 4, 0x04: 8, 0x0A: 2, 0x0B: 4, 0x0C: 8}
PTR_FORMATS = {0x00: "<Q", 0x02: "<H", 0x03: "<I", 0x04: "<Q", 0x0A: "<h", 0x0B: "<i", 0x0C: "<q"}


def eh_pointers(d):
    """.eh_frame's encoded pointers, (offset, encoding) of each CIE's personality and each FDE's start and LSDA; and
    the FDEs' starts alone (their code's length follows each)."""
    ptrs, fdes, cies, o = [], [], {}, 0
    while o + 4 <= len(d):
        length = u("<I", d, o)
        if length == 0:
            break
        if length == 0xFFFFFFFF:
            raise Relink(".eh_frame: a 64-bit record")
        end, cid = o + 4 + length, u("<I", d, o + 4)
        p = o + 8
        if cid == 0:
            version = d[p]
            aug_end = d.index(b"\0", p + 1)
            aug = d[p + 1:aug_end].decode()
            p = aug_end + 1
            _, p = uleb(d, p)                                               # code alignment
            _, p = uleb(d, p)                                               # data alignment (sleb: its bytes alike)
            p = p + 1 if version == 1 else uleb(d, p)[1]                    # return address register
            info = {"R": 0x00, "L": 0xFF}
            if aug.startswith("z"):
                _, p = uleb(d, p)
                for c in aug[1:]:
                    if c in "RL":
                        info[c] = d[p]
                        p += 1
                    elif c == "P":
                        enc = d[p]
                        if enc & 0x0F not in PTR_SIZES:
                            raise Relink(f".eh_frame: personality encoding {enc:#x}")
                        ptrs.append((p + 1, enc))
                        p += 1 + PTR_SIZES[enc & 0x0F]
                    elif c not in "SB":
                        raise Relink(f".eh_frame: augmentation {aug!r}")
            elif aug:
                raise Relink(f".eh_frame: augmentation {aug!r}")
            info["z"] = aug.startswith("z")
            cies[o] = info
        else:
            info = cies.get(o + 4 - cid)
            if info is None:
                raise Relink(".eh_frame: an FDE without its CIE")
            enc = info["R"]
            if enc & 0x0F not in PTR_SIZES:
                raise Relink(f".eh_frame: FDE pointer encoding {enc:#x}")
            ptrs.append((p, enc))
            fdes.append((p, enc))
            p += 2 * PTR_SIZES[enc & 0x0F]
            if info["z"]:
                _, p = uleb(d, p)
                if info["L"] != 0xFF:
                    ptrs.append((p, info["L"]))
        o = end
    return ptrs, fdes


def functions(sec):
    """The code ranges (start, end) .eh_frame's FDEs describe, sorted; [] without one that can be read."""
    if sec is None:
        return []
    try:
        _, fdes = eh_pointers(sec.data)
    except (Relink, IndexError, struct.error, UnicodeDecodeError):
        return []
    out = []
    for o, enc in fdes:
        f = PTR_FORMATS[enc & 0x0F]
        base = {0x00: 0, 0x10: sec.addr + o, 0x30: sec.addr}.get(enc & 0x70)
        if base is None:
            return []
        start = base + u(f, sec.data, o)
        out.append((start, start + u(f.upper() if f[1].islower() else f, sec.data, o + struct.calcsize(f))))
    return sorted(out)


def check_pointers(mv, sa, sb, ptrs):
    """Encoded pointers (pcrel, datarel to the section, or absolute; indirect or not) at the same place in both: each
    pointing where the old one's target went, and every other byte the same."""
    a, b = bytearray(sa.data), bytearray(sb.data)
    for o, enc in ptrs:
        f = PTR_FORMATS.get(enc & 0x0F)
        if f is None or enc & 0x70 not in (0x00, 0x10, 0x30):
            raise Relink(f"{sa.name}+{o:#x}: pointer encoding {enc:#x}")
        n = struct.calcsize(f)
        va, vb = u(f, a, o), u(f, b, o)
        base_a = {0x00: 0, 0x10: sa.addr + o, 0x30: sa.addr}[enc & 0x70]
        base_b = {0x00: 0, 0x10: sb.addr + o, 0x30: sb.addr}[enc & 0x70]
        if mv.addr(base_a + va) != base_b + vb:
            raise Relink(f"{sa.name}+{o:#x}: pointers {va:#x} and {vb:#x} don't point to the same place")
        a[o:o + n] = b[o:o + n] = bytes(n)
    if a != b:
        raise Relink(f"{sa.name}: differs at {first_diff(bytes(a), bytes(b)):#x}")


def eh_frame_hdr_pointers(d):
    """.eh_frame_hdr's: the pointer to .eh_frame, then its table of FDEs (start, FDE), as encoded."""
    if d[0] != 1:
        raise Relink(".eh_frame_hdr: version")
    ptr_enc, count_enc, table_enc = d[1], d[2], d[3]
    ptrs, o = [(4, ptr_enc)], 4 + PTR_SIZES.get(ptr_enc & 0x0F, 0)
    f = PTR_FORMATS.get(count_enc & 0x0F)
    if f is None or table_enc == 0xFF:
        return ptrs
    count, o = u(f, d, o), o + struct.calcsize(f)
    n = PTR_SIZES.get(table_enc & 0x0F, 0)
    for i in range(2 * count):
        ptrs.append((o + i * n, table_enc))
    return ptrs


def check_words(mv, sa, sb, rules):
    """Data: every differing byte in an aligned 8-byte word whose old value is an address that moved to its new
    value (the GOT, pointers), or excused by the rules."""
    a, b = sa.data, sb.data
    for s, e in runs(a, b):
        for w in range(s - s % 8, e, 8):
            if w + 8 > len(a):
                break
            va, vb = u("<Q", a, w), u("<Q", b, w)
            if va != vb and (not va or mv.addr(va) != vb):
                break
        else:
            continue
        if cover(a, b, s, e, rules, ()) is None:
            raise Relink(f"{sa.name}: differs at {s:#x}")


def relinked(a, b, rules):
    """Two AArch64 ELF files as one linked again, its sections moved by a build path of another length: the resized
    sections, else Relink with why not."""
    ea, eb = Elf(a), Elf(b)
    head = ("type", "machine", "version", "flags", "ehsize", "phentsize", "phnum", "shentsize", "shnum", "shstrndx",
            "phoff")
    if a[:16] != b[:16] or [getattr(ea, k) for k in head] != [getattr(eb, k) for k in head]:
        raise Relink("the ELF headers differ")
    resized = []
    for sa, sb in zip(ea.secs, eb.secs):
        if (sa.name, sa.type, sa.flags, sa.link, sa.info, sa.align, sa.entsize) != \
                (sb.name, sb.type, sb.flags, sb.link, sb.info, sb.align, sb.entsize):
            raise Relink(f"section {sa.name} and {sb.name} differ")
        if sa.data != sb.data and (sa.type == SHT_STRTAB or sa.type == SHT_PROGBITS and not sa.flags & SHF_EXECINSTR):
            ea_, eb_ = holder_pair(sa.data, sb.data, sa.type == SHT_STRTAB)
            if ea_ or eb_:
                sa.edits, sb.edits = ea_, eb_
                if sa.size != sb.size:
                    resized.append(f"{sa.name} {sb.size - sa.size:+}")
                continue
        if sa.size != sb.size:
            raise Relink(f"{sa.name}: sizes {sa.size} and {sb.size}")
    # the headers as a linker writes them: each allocated section on its alignment, inside a loaded segment at the
    # segment's offset to address; the others at address 0, moved in the file as far as the section headers
    for e in (ea, eb):
        loads = [p for p in e.ph if p[0] == 1]
        for s in e.secs:
            if not s.flags & SHF_ALLOC or not s.size or s.flags & SHF_TLS and s.type == SHT_NOBITS:
                continue
            if s.align > 1 and s.addr % s.align or not any(
                    p[3] <= s.addr and s.addr + s.size <= p[3] + p[6]
                    and (s.type == SHT_NOBITS or s.addr - s.off == p[3] - p[2] and s.off + s.size <= p[2] + p[5])
                    for p in loads):
                raise Relink(f"{s.name} isn't where its segment puts it")
    for sa, sb in zip(ea.secs, eb.secs):
        if not sa.flags & SHF_ALLOC and (sa.addr != sb.addr or sa.type and sb.off - sa.off != eb.shoff - ea.shoff
                                         or not sa.type and (sa.off, sa.size) != (sb.off, sb.size)):
            raise Relink(f"section {sa.name or 0}'s header")
    mv = Moves(ea, eb)
    if (ea.entry or eb.entry) and mv.addr(ea.entry) != eb.entry:
        raise Relink("the entry point")
    for i, (pa, pb) in enumerate(zip(ea.ph, eb.ph)):
        ta, fa, oa, va, xa, za, ma, la = pa
        tb, fb, ob, vb, xb, zb, mb, lb = pb
        if (ta, fa, la) != (tb, fb, lb):
            raise Relink(f"program header {i}")
        if (oa, va, xa, za, ma) == (ob, vb, xb, zb, mb):
            continue
        if (mv.around(oa, "off") != ob or mv.around(oa + za, "off", True) != ob + zb or mv.around(va, "addr") != vb
                or mv.around(va + ma, "addr", True) != vb + mb or xb - vb != xa - va):
            raise Relink(f"program header {i} (type {ta:#x}) doesn't cover the same sections")
    funcs = functions(next((sa for sa, _ in mv.pairs if sa.name == ".eh_frame"), None))
    for sa, sb in mv.pairs:
        if sa.type == SHT_NOBITS or sa.data == sb.data and sa.addr == sb.addr or sa.edits or sb.edits:
            continue
        if sa.flags & SHF_EXECINSTR:
            check_code(mv, sa, sb, funcs)
        elif sa.type in (SHT_DYNSYM, SHT_SYMTAB):
            check_symbols(mv, sa, sb)
        elif sa.type == SHT_RELA:
            check_relocations(mv, sa, sb)
        elif sa.type == SHT_DYNAMIC:
            check_dynamic(mv, sa, sb)
        elif sa.type in (SHT_VERNEED, SHT_VERDEF):
            check_versions(mv, sa, sb)
        elif sa.name == ".eh_frame":
            check_pointers(mv, sa, sb, eh_pointers(sa.data)[0])
        elif sa.name == ".eh_frame_hdr":
            check_pointers(mv, sa, sb, eh_frame_hdr_pointers(sa.data))
        elif sa.data != sb.data:
            if sa.flags & SHF_ALLOC and sa.type != SHT_NOTE:
                check_words(mv, sa, sb, rules)
            elif cover_all(sa.data, sb.data, rules) is None:
                raise Relink(f"{sa.name}: differs at {first_diff(sa.data, sb.data):#x}")
    # what no section holds: padding, the same zeros in both
    for e in (ea, eb):
        held = sorted([(0, e.ehsize), (e.phoff, e.phoff + e.phnum * e.phentsize),
                       (e.shoff, e.shoff + e.shnum * e.shentsize)]
                      + [(s.off, s.off + s.size) for s in e.secs if s.type != SHT_NOBITS])
        pos = 0
        for s, t in held:
            if s > pos and e.d[pos:s].strip(b"\0"):
                raise Relink(f"bytes outside the sections at {pos:#x}")
            pos = max(pos, t)
        if e.d[pos:].strip(b"\0"):
            raise Relink(f"bytes after the sections at {pos:#x}")
    if not resized:
        raise Relink("no section holds a build path of another length")
    return resized


def cover_all(a, b, rules):
    """Equal-length a and b with every differing run excused by the rules: the classes, else None."""
    if len(a) != len(b):
        return None
    used, bad = excuse_runs(a, b, rules)
    return None if bad is not None else used


# --- U-Boot: the Rockchip loader header and the FIT

def fdt_spans(d, off):
    """A FIT at d[off:]: the spans of its /timestamp and of every value under a hash* node; None if it isn't one."""
    try:
        magic, total, ostruct, ostrings, _, version, _, _, _, sstruct = struct.unpack_from(">10I", d, off)
        if magic != 0xd00dfeed or version < 16 or off + total > len(d):
            return None
        p, strings = off + ostruct, off + ostrings
        end = p + sstruct if version >= 17 else off + total
        path, spans, fit = [], [], False
        while p < end:
            tok, = struct.unpack_from(">I", d, p)
            p += 4
            if tok == 1:                                    # FDT_BEGIN_NODE
                name = d[p:d.index(b"\0", p)]
                p = off + ((p + len(name) + 1 - off + 3) & ~3)
                path.append(name)
                fit = fit or path == [b"", b"images"]
            elif tok == 2:                                  # FDT_END_NODE
                path.pop()
            elif tok == 3:                                  # FDT_PROP
                ln, nameoff = struct.unpack_from(">II", d, p)
                p += 8
                prop = d[strings + nameoff:d.index(b"\0", strings + nameoff)]
                if len(path) == 1 and prop == b"timestamp":
                    spans.append((p, p + ln, "times", "U-Boot: FIT timestamp"))
                elif len(path) >= 2 and path[-1].startswith(b"hash") and prop == b"value":
                    spans.append((p, p + ln, "derived", "U-Boot: FIT hashes"))
                p = off + ((p + ln - off + 3) & ~3)
            elif tok == 9:                                  # FDT_END
                break
            elif tok != 4:                                  # FDT_NOP
                return None
        return spans if fit else None
    except (struct.error, ValueError, IndexError):
        return None


def uboot_spans(d, base):
    """The spans in d (at disk offset base) that U-Boot's structures name: the loader header's hashes, the FITs'."""
    spans = []
    h = UBOOT_AT - base
    if 0 <= h and d[h:h + 4] == b"RKNS":
        # struct header0_info_v2 (u-boot's tools/rkcommon.c): 4 image entries of 88 bytes from 120, each with its
        # 64-byte hash at 24; the header's own hash at 1536 (512 bytes)
        for i in range(4):
            s = h + 120 + 88 * i + 24
            spans.append((s, s + 64, "derived", "U-Boot: loader header hashes"))
        spans.append((h + 1536, h + 2048, "derived", "U-Boot: loader header hashes"))
    i = d.find(b"\xd0\x0d\xfe\xed")
    while i >= 0:
        spans += fdt_spans(d, i) or []
        i = d.find(b"\xd0\x0d\xfe\xed", i + 4)
    return spans


def compare_uboot(rep, where, a, b, base):
    """Bytes outside the GPT and the partitions: U-Boot's structures masked, the rest under the build-date rule."""
    if a == b:
        return
    sb = set(uboot_spans(b, base))
    spans = [s for s in uboot_spans(a, base) if s in sb]
    used, bad = excuse_runs(a, b, DATE_RULES, spans)
    if bad is not None:
        rep.differ(where, f"differs at {base + bad:#x}")
        return
    for cls, label in used:
        rep.excuse(cls, label or where)


# --- the GPT

GPT_HDR = struct.Struct("<8sIIIIQQQQ16sQIII")
GPT_FIELDS = ("signature", "revision", "header size", "header CRC", "reserved", "LBA", "other header's LBA",
              "first usable LBA", "last usable LBA", "disk GUID", "entries' LBA", "number of entries", "entry size",
              "entries' CRC")


def gpt_parse(hdr, entries):
    """A GPT header and its entries: (fields, the CRCs' validity, [(type, first, last, attributes, name, rest)])."""
    f = dict(zip(GPT_FIELDS, GPT_HDR.unpack_from(hdr)))
    size = f["header size"]
    h = bytearray(hdr[:size])
    struct.pack_into("<I", h, 16, 0)
    n, es = f["number of entries"], f["entry size"]
    raw = entries[:n * es]
    ok = zlib.crc32(h) == f["header CRC"] and zlib.crc32(raw) == f["entries' CRC"]
    ents = [(raw[i:i + 16], *struct.unpack_from("<QQQ", raw, i + 32), raw[i + 56:i + 128], raw[i + 128:i + es])
            for i in range(0, n * es, es)]
    return f, hdr[size:], ok, ents


def compare_gpt(rep, where, ha, ea, hb, eb):
    """Field by field; the GUIDs and CRCs (genimage's GUIDs are random) only excused, each side's CRCs checked."""
    fa, resta, oka, enta = gpt_parse(ha, ea)
    fb, restb, okb, entb = gpt_parse(hb, eb)
    for side, ok in (("old", oka), ("new", okb)):
        if not ok:
            rep.differ(where, f"the {side} image's CRC32s don't match its header and entries")
    for k in GPT_FIELDS:
        if fa[k] != fb[k]:
            if k in ("disk GUID", "header CRC", "entries' CRC"):
                rep.excuse("random IDs", f"{where}: {k}")
            else:
                rep.differ(where, f"{k}: {fa[k]} and {fb[k]}")
    if resta != restb:
        rep.differ(where, "the header's reserved bytes")
    if len(enta) != len(entb):
        return
    names = ("type GUID", "first LBA", "last LBA", "attributes", "name", "reserved bytes")
    for i, (x, y) in enumerate(zip(enta, entb)):
        for k, u, v in zip(names, x, y):
            if u != v:
                rep.differ(where, f"partition {i + 1}'s {k}: {u!r} and {v!r}")
        # the unique GUID, ignored: bytes 16-31, not in the tuples
    if any(ea[i + 16:i + 32] != eb[i + 16:i + 32] for i in range(0, fa["number of entries"] * fa["entry size"],
                                                                     fa["entry size"])):
        rep.excuse("random IDs", f"{where}: partition GUIDs")


def partitions(ents):
    """The partitions in use: name -> (start, end) in bytes."""
    return {n.decode("utf-16-le").rstrip("\0"): (first * SECTOR, (last + 1) * SECTOR)
            for t, first, last, _, n, _ in ents if t != bytes(16)}


# --- the squashfs listing

def parse_listing(text):
    """unsquashfs -lln: path -> (type and mode, uid, gid, symlink target, time)."""
    out = {}
    for line in text.splitlines():
        if not line.strip():
            continue
        m = LISTING.match(line)
        if not m:
            raise Fail(f"unsquashfs -lln: can't read {line!r}")
        mode, uid, gid, _, time, path = m.groups()
        path, target = (path or "/"), None
        if mode[0] == "l":
            path, target = path.split(" -> ", 1)
        out[path.lstrip("/")] = (mode, uid, gid, target, time)
    return out


def only_in(rep, where, side, paths, listing):
    """Paths only one side has, a folder once with its count of entries under it."""
    paths = sorted(paths)
    i = 0
    while i < len(paths):
        p = paths[i]
        j = i + 1
        if listing[p][0][0] == "d":
            while j < len(paths) and paths[j].startswith(p + "/"):
                j += 1
        under = f" ({j - i - 1} entries under it)" if j - i > 1 else ""
        rep.differ(where, f"only in {side}: {p}{'/' if listing[p][0][0] == 'd' else ''}{under}")
        i = j


def compare_listing(rep, where, la, lb):
    """Each entry's type, mode, owner and symlink target; times excused. The regular files both have."""
    only_in(rep, where, "old", la.keys() - lb.keys(), la)
    only_in(rep, where, "new", lb.keys() - la.keys(), lb)
    files, times = [], 0
    for p in sorted(la.keys() & lb.keys()):
        (ma, ua, ga, ta, tia), (mb, ub, gb, tb, tib) = la[p], lb[p]
        what = []
        if ma != mb:
            what.append(f"mode {ma} and {mb}")
        if (ua, ga) != (ub, gb):
            what.append(f"owner {ua}/{ga} and {ub}/{gb}")
        if ta != tb:
            what.append(f"target {ta} and {tb}")
        if what:
            rep.differ(f"{where}: {p}", ", ".join(what))
        elif ma[0] == "-":
            files.append(p)
        times += tia != tib
    if times:
        rep.excuse("times", f"{where}: file times ({times} entries)")
    return files


# --- the parts

def run(*cmd, **kw):
    r = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, **kw)
    if r.returncode:
        raise Fail(f"{' '.join(map(str, cmd))} failed ({r.returncode}): {r.stderr.decode(errors='replace').strip()}")
    return r.stdout


def lz4d(data):
    return run("lz4", "-d", "-c", input=data)


class Side:
    def __init__(self, name, img_gz, tar, path):
        self.name, self.img_gz, self.tar, self.path = name, img_gz, tar, path
        self.img, self.f, self.size, self.parts = "", None, 0, {}   # the decompressed image, once compare() made it

    def read(self, off, n):
        self.f.seek(off)
        return self.f.read(n)


def same_bytes(rep, where, old, new, start, end):
    for off in range(start, end, 1 << 22):
        n = min(1 << 22, end - off)
        a, b = old.read(off, n), new.read(off, n)
        if a != b:
            rep.differ(where, f"differs at {off + first_diff(a, b):#x}")
            return


def compare_mbr(rep, old, new):
    same_bytes(rep, "MBR", old, new, 0, SECTOR)


def compare_gpts(rep, old, new):
    layout = []
    for where in ("GPT", "backup GPT"):
        hs, es = [], []
        for s in (old, new):
            h = s.read((1 if where == "GPT" else s.size // SECTOR - 1) * SECTOR, SECTOR)
            if h[:8] != b"EFI PART":
                raise Fail(f"{s.img_gz}: no {where} header")
            f = dict(zip(GPT_FIELDS, GPT_HDR.unpack_from(h)))
            hs.append(h)
            es.append(s.read(f["entries' LBA"] * SECTOR, f["number of entries"] * f["entry size"]))
            layout.append((f, h, es[-1]))
        compare_gpt(rep, where, hs[0], es[0], hs[1], es[1])
    return layout


def compare_outside(rep, old, new, regions):
    for start, end in regions:
        where = "U-Boot" if start <= UBOOT_AT < end else f"bytes {start:#x}-{end:#x}"
        compare_uboot(rep, where, old.read(start, end - start), new.read(start, end - start), start)


def mtools(side, start):
    return dict(os.environ, MTOOLS_SKIP_CHECK="1"), f"{side.img}@@{start}"


def compare_fat(rep, old, new, work, where):
    seen = []
    for s in (old, new):
        start, _ = s.parts[where]
        env, img = mtools(s, start)
        listing = run("mdir", "-/", "-a", "-i", img, "::", env=env).decode(errors="replace")
        label = re.search(r"Volume in drive : (?:is (.*)|has no label)", listing)
        serial = re.search(r"Volume Serial Number is (\S+)", listing)
        dest = os.path.join(work, f"{s.name}-{where}")
        os.mkdir(dest)
        run("mcopy", "-s", "-n", "-m", "-i", img, "::/*", dest + "/", env=env)
        tree = {}
        for d, dirs, files in os.walk(dest):
            for n in dirs + files:
                full = os.path.join(d, n)
                tree[os.path.relpath(full, dest)] = None if os.path.isdir(full) else full
        seen.append((label and label.group(1), serial and serial.group(1),
                     re.findall(r"\d{4}-\d\d-\d\d +\d\d?:\d\d", listing), tree))
    (la, sa, ta, a), (lb, sb, tb, b) = seen
    if la != lb:
        rep.differ(where, f"labels {la} and {lb}")
    if sa != sb:
        rep.excuse("random IDs", f"{where}: volume ID")
    if ta != tb:
        rep.excuse("times", f"{where}: file times")
    for side, extra in (("old", a.keys() - b.keys()), ("new", b.keys() - a.keys())):
        for p in sorted(extra):
            rep.differ(where, f"only in {side}: {p}")
    for p in sorted(a.keys() & b.keys()):
        if (a[p] is None) != (b[p] is None):
            rep.differ(f"{where}: {p}", "a folder in one, a file in the other")
        elif a[p] is not None:
            da, db = open(a[p], "rb").read(), open(b[p], "rb").read()
            if p.rsplit("/", 1)[-1] == "Image":             # LZ4 (post-image.sh): the kernel, its dates excused
                compare_file(rep, f"{where}: {p}", lz4d(da), lz4d(db), dates=True)
            elif da != db:
                rep.differ(f"{where}: {p}", f"differs at {first_diff(da, db):#x}")


SQUASHFS_IGNORED = ("Creation or last append time", "Filesystem size")


def compare_squashfs(rep, old, new, work, name):
    info, lists, roots, where = [], [], [], name.replace("banana-", "")
    for s in (old, new):
        start, _ = s.parts[name]
        text = run("unsquashfs", "-s", "-o", str(start), s.img).decode(errors="replace").replace(s.img, "<image>")
        info.append(text.splitlines())
        lists.append(parse_listing(run("unsquashfs", "-lln", "-o", str(start), s.img).decode("utf-8", "surrogateescape")))
        dest = os.path.join(work, f"{s.name}-{where}")
        log(f"extracting {s.name}'s {where}")
        run("unsquashfs", "-q", "-n", "-no-xattrs", "-o", str(start), "-d", dest, s.img)
        # readable here (modes are compared in the listing); a symlink is never followed, it may point anywhere
        for d, dirs, files in os.walk(dest):
            for n in dirs + files:
                p = os.path.join(d, n)
                if not os.path.islink(p):
                    os.chmod(p, 0o755 if n in dirs else os.lstat(p).st_mode & 0o7777 | 0o400)
        roots.append(dest)
    for x, y in zip(*info):
        if x != y:
            ignored = [k for k in SQUASHFS_IGNORED if x.startswith(k) and y.startswith(k)]
            if not ignored:
                rep.differ(where, f"superblock: {x!r} and {y!r}")
            elif ignored[0].startswith("Creation"):
                rep.excuse("times", f"{where}: creation time")
            else:
                rep.excuse("derived", f"{where}: filesystem size")
    if len(info[0]) != len(info[1]):
        rep.differ(where, "superblock: unsquashfs -s lists more for one")
    log(f"comparing {where}'s files")
    for p in compare_listing(rep, where, *lists):
        with open(os.path.join(roots[0], p), "rb") as fa, open(os.path.join(roots[1], p), "rb") as fb:
            compare_file(rep, f"{where}: {p}", fa.read(), fb.read())


def compare_update(rep, old, new):
    where = "update"
    tars = [tarfile.open(s.tar) for s in (old, new)]
    mems = [{m.name.split("/", 1)[1] if "/" in m.name else "": m for m in t.getmembers()} for t in tars]
    tops = [{m.name.split("/", 1)[0] for m in t.getmembers()} for t in tars]
    if tops[0] != tops[1]:
        rep.differ(where, f"its folders {sorted(tops[0])} and {sorted(tops[1])}")
    a, b = mems
    for side, extra in (("old", a.keys() - b.keys()), ("new", b.keys() - a.keys())):
        for p in sorted(extra):
            rep.differ(where, f"only in {side}: {p}")
    times = False
    for p in sorted(a.keys() & b.keys()):
        ma, mb = a[p], b[p]
        if ma.type != mb.type or ma.mode != mb.mode:
            rep.differ(f"{where}: {p or '.'}", f"type and mode {ma.type!r} {ma.mode:o} and {mb.type!r} {mb.mode:o}")
        times = times or ma.mtime != mb.mtime
    if times:
        rep.excuse("times", f"{where}: file times")
    sums = []
    for s, t, m in zip((old, new), tars, mems):
        # each side's rootfs.squashfs is its own root-a, bytes for bytes (its root-a is compared with the other's)
        hashes = {}
        for p, mem in m.items():
            if not mem.isfile():
                continue
            h, f = hashlib.sha256(), t.extractfile(mem)
            if p == "rootfs.squashfs":
                start, end = s.parts["banana-root-a"]
                off, mismatch = start, None
                while chunk := f.read(1 << 22):
                    h.update(chunk)
                    if mismatch is None and s.read(off, len(chunk)) != chunk:
                        mismatch = off
                    off += len(chunk)
                if mismatch is None and off <= end:
                    for z in range(off, end, 1 << 22):
                        if s.read(z, min(1 << 22, end - z)).strip(b"\0"):
                            mismatch = z
                            break
                if mismatch is not None or off > end:
                    rep.differ(f"{where}: rootfs.squashfs", f"isn't the {s.name} image's root-a")
            else:
                h.update(f.read())
            hashes[p] = h.hexdigest()
        listed = {}
        if "sha256sums" in m:
            for line in t.extractfile(m["sha256sums"]).read().decode(errors="replace").splitlines():
                digest, _, name = line.partition("  ")
                listed[name] = digest
                if hashes.get(name) != digest:
                    rep.differ(f"{where}: sha256sums", f"{name}'s sum isn't the {s.name} package's file's")
        sums.append(set(listed))
    if sums[0] != sums[1]:
        rep.differ(f"{where}: sha256sums", f"names {sorted(sums[0])} and {sorted(sums[1])}")
    for p in sorted(a.keys() & b.keys()):
        if not (a[p].isfile() and b[p].isfile()) or p in ("rootfs.squashfs", "sha256sums"):
            continue
        da, db = tars[0].extractfile(a[p]).read(), tars[1].extractfile(b[p]).read()
        if p == "Image":
            compare_file(rep, f"{where}: Image", lz4d(da), lz4d(db), dates=True)
        elif da != db:
            rep.differ(f"{where}: {p}", f"differs at {first_diff(da, db):#x}")


def compare(rep, old, new, work):
    for s in (old, new):
        log(f"decompressing {s.img_gz}")
        s.img = os.path.join(work, s.name + ".img")
        with gzip.open(s.img_gz, "rb") as i, open(s.img, "wb") as o:
            shutil.copyfileobj(i, o, 1 << 22)
        s.f = open(s.img, "rb")
        s.size = os.path.getsize(s.img)
    if old.size != new.size:
        rep.differ("disk image", f"sizes {old.size} and {new.size}")
        return
    rep.part("MBR", compare_mbr, old, new)
    layout = rep.part("GPT", compare_gpts, old, new)
    pa, pb = (partitions(gpt_parse(h, e)[3]) for _, h, e in layout[:2])
    if pa != pb:
        rep.differ("GPT", "the partitions differ, nothing more compared")
        return
    old.parts = new.parts = pa
    # what the GPT and the partitions don't hold: U-Boot from 32 KiB, then whatever lies between
    used = [(0, SECTOR), (SECTOR, 2 * SECTOR)]
    for f, _, _ in (layout[0], layout[2]):
        e = f["entries' LBA"] * SECTOR
        used.append((e, e + -(-f["number of entries"] * f["entry size"] // SECTOR) * SECTOR))
    used.append((old.size - SECTOR, old.size))
    used += pa.values()
    regions, at = [], 0
    for s, e in sorted(used):
        if s > at:
            regions.append((at, s))
        at = max(at, e)
    if at < old.size:
        regions.append((at, old.size))
    rep.part("U-Boot", compare_outside, old, new, regions)
    for name, (start, end) in sorted(pa.items(), key=lambda x: x[1]):
        short = name.replace("banana-", "")
        if name == "boot":
            rep.part("boot partition", compare_fat, old, new, work, name)
        elif name == "banana-root-a":
            rep.part(short, compare_squashfs, old, new, work, name)   # (reported as root-a)
        else:
            rep.part(short, same_bytes, short, old, new, start, end)
    log("comparing the update packages")
    rep.part("update package", compare_update, old, new)


# --- self-test

def self_test():
    checks = []

    def check(name, fn, same, classes=(), hint=False):
        rep = Report(paths_differ=hint)
        fn(rep)
        got = [c for c in CLASSES if rep.excused[c]]
        ok = (not rep.diffs) == same and (not same or set(classes) <= set(got))
        if hint:
            ok = ok and any("differ in length" in d for d in rep.diffs)
        checks.append(name)
        if not ok:
            print(f"image-compare self-test: FAIL: {name}: differences {rep.diffs}, excused {rep.excused}",
                  file=sys.stderr)
            sys.exit(1)

    def files(a, b, name="x", dates=None):
        return lambda rep: compare_file(rep, name, a, b, dates)

    elf = b"\x7fELF\x02\x01\x01" + bytes(57)
    tail = b"\x00\x01\x02\x03" * 64
    k1 = bytes(300) + b"Linux version 7.2.7-banana #1 SMP PREEMPT Mon Nov  2 15:18:16 EET 2026\n\x00" + tail
    k2 = bytes(300) + b"Linux version 7.2.7-banana #1 SMP PREEMPT Tue Nov 10 09:01:02 EET 2026\n\x00" + tail
    check("kernel banners, the date", files(k1, k2, dates=True), True, ["build dates"])
    check("kernel banners, the date and another byte", files(k1, k2.replace(b"7.2.7", b"7.2.8"), dates=True), False)
    bb1 = elf + b"BusyBox v1.37.0 (2026-10-26 18:54:36 EET)\x00" + tail
    bb2 = elf + b"BusyBox v1.37.0 (2026-11-09 07:05:59 EET)\x00" + tail
    check("BusyBox's date in an ELF file", files(bb1, bb2), True, ["build dates"])
    check("BusyBox's date and another byte", files(bb1, bb2[:-1] + b"\x09"), False)
    check("BusyBox's date in a file that isn't ELF", files(bb1[4:], bb2[4:]), False)
    d1 = elf + b"RetroArch\x00Nov  2 2026\x0015:18:16\x00" + tail
    d2 = elf + b"RetroArch\x00Oct 26 2026\x0009:01:59\x00" + tail
    check("__DATE__ and __TIME__ in an ELF file", files(d1, d2), True, ["build dates"])
    check("__DATE__ and another byte", files(d1, d2.replace(b"RetroArch", b"RetroArcX")), False)
    o1 = elf + b"not available\x00\x00\x00built on: Mon Nov  9 10:41:15 2026 UTC\x00\x00platform: linux-aarch64\x00" + tail
    o2 = elf + b"not available\x00\x00\x00built on: Mon Nov  9 10:59:51 2026 UTC\x00\x00platform: linux-aarch64\x00" + tail
    check("OpenSSL's asctime() date", files(o1, o2), True, ["build dates"])
    check("OpenSSL's date and another byte", files(o1, o2.replace(b"aarch64", b"aarch65")), False)
    x1 = elf + b"Starting PCSX-ReARMed  (%s)\n\x00\x00\x00\x00build time: Nov  9 2026 12:53:54\n\x00" + tail
    x2 = elf + b"Starting PCSX-ReARMed  (%s)\n\x00\x00\x00\x00build time: Nov  9 2026 13:10:13\n\x00" + tail
    check("__DATE__ and __TIME__ in one string", files(x1, x2), True, ["build dates"])

    def note(seed):
        return b"\x04\x00\x00\x00\x14\x00\x00\x00\x03\x00\x00\x00GNU\x00" + hashlib.sha1(seed).digest() + b"Linux\x00"
    check("the kernel's build ID", files(k1 + note(b"1"), k2 + note(b"2"), dates=True), True,
          ["build dates", "derived"])
    check("the kernel's build ID note, its type changed",
          files(k1 + note(b"1"), k2 + note(b"2").replace(b"\x03\x00\x00\x00GNU", b"\x04\x00\x00\x00GNU"), dates=True),
          False)

    def cpio(mode, mtime):
        return b"070701" + b"".join(b"%08X" % v for v in (0x2D1, mode, 0, 0, 3, mtime, 0, 0, 0, 0, 0, 4, 0)) + b"dev\x00"
    c1, c2 = k1 + cpio(0x41ED, 0x6AF1A54F) + tail, k1 + cpio(0x41ED, 0x6AF1A92B) + tail
    check("the built-in initramfs's mtimes", files(c1, c2, dates=True), True, ["times"])
    check("the built-in initramfs's mode", files(c1, k1 + cpio(0x41FF, 0x6AF1A92B) + tail, dates=True), False)
    u1 = b"U-Boot SPL 2026.01 (Oct 26 2026 - 19:02:12 +0200)\x00" + tail
    u2 = b"U-Boot SPL 2026.01 (Nov 09 2026 - 07:15:42 +0200)\x00" + tail
    check("U-Boot's date", lambda rep: compare_uboot(rep, "U-Boot", u1, u2, 0), True, ["build dates"])

    # a FIT: /timestamp and /images/uboot/hash-1/value changed are excused; the data it hashes isn't
    def fdt(root):
        st, strings, offs = bytearray(), bytearray(), {}

        def pad(b):
            return b + bytes(-len(b) % 4)

        def node(name, props, kids):
            st.extend(struct.pack(">I", 1) + pad(name + b"\0"))
            for p, v in props:
                if p not in offs:
                    offs[p] = len(strings)
                    strings.extend(p + b"\0")
                st.extend(struct.pack(">III", 3, len(v), offs[p]) + pad(v))
            for k in kids:
                node(*k)
            st.extend(struct.pack(">I", 2))
        node(*root)
        st.extend(struct.pack(">I", 9))
        total = 56 + len(st) + len(strings)
        return (struct.pack(">10I", 0xd00dfeed, total, 56, 56 + len(st), 40, 17, 16, 0, len(strings), len(st))
                + bytes(16) + st + strings)

    def fit(ts, digest):
        h = (b"hash-1", [(b"algo", b"sha256\0"), (b"value", digest)], [])
        img = (b"uboot", [(b"data-size", struct.pack(">I", 64)), (b"data-offset", struct.pack(">I", 0))], [h])
        return fdt((b"", [(b"timestamp", struct.pack(">I", ts)), (b"description", b"FIT\0")],
                    [(b"images", [], [img]), (b"configurations", [], [])]))
    data1 = b"U-Boot 2026.01 (Oct 26 2026 - 19:02:12 +0200)\x00".ljust(64, b"\x01")
    data2 = b"U-Boot 2026.01 (Nov 09 2026 - 07:15:42 +0200)\x00".ljust(64, b"\x01")
    f1 = bytes(512) + fit(1761498132, hashlib.sha256(data1).digest()) + data1
    f2 = bytes(512) + fit(1762665342, hashlib.sha256(data2).digest()) + data2
    check("a FIT's timestamp, hash and the dates it hashes",
          lambda rep: compare_uboot(rep, "U-Boot", f1, f2, 0), True, ["times", "derived", "build dates"])
    f3 = f2[:-1] + b"\x02"
    check("a FIT's image data changed", lambda rep: compare_uboot(rep, "U-Boot", f1, f3, 0), False)

    # the Rockchip loader header at 32 KiB: its hashes excused, the rest not
    def rkns(digest):
        h = bytearray(2048)
        h[:4] = b"RKNS"
        struct.pack_into("<I", h, 8, (2 << 16) + 384)
        h[120 + 88 + 24:120 + 88 + 24 + 32] = digest
        h[1536:1568] = hashlib.sha256(bytes(h[:1536])).digest()
        return bytes(UBOOT_AT - 4096) + bytes(h)
    r1, r2 = rkns(hashlib.sha256(b"spl 1").digest()), rkns(hashlib.sha256(b"spl 2").digest())
    check("the loader header's hashes", lambda rep: compare_uboot(rep, "U-Boot", r1, r2, 4096), True, ["derived"])
    r3 = bytearray(r2)
    r3[UBOOT_AT - 4096 + 8] ^= 1
    check("the loader header's image count", lambda rep: compare_uboot(rep, "U-Boot", r1, bytes(r3), 4096), False)

    # build paths: text (any length), .pyc (constants), other files (equal length only)
    t1 = b"LIBDIR='/x/output/cmp-a/rgds-plus/host/lib'\nprefix=/usr\n"
    t2 = b"LIBDIR='/x/output/cmp-b/rgds-plus/per-package/python3/host/lib'\nprefix=/usr\n"
    check("a text file's build paths", files(t1, t2), True, ["build paths"])
    check("a text file's build paths and another change", files(t1, t2.replace(b"/usr", b"/opt")), False)
    b1 = elf + b"\x00/x/output/cmp-a/rgds-plus/build/sqlite-3.51/sqlite3.c\x00" + tail
    b2 = elf + b"\x00/x/output/cmp-b/rgds-plus/build/sqlite-3.51/sqlite3.c\x00" + tail
    b3 = elf + b"\x00/x/output/cmp-bb/rgds-plus/build/sqlite-3.51/sqlite3.c\x00" + tail
    check("a binary's build paths of equal length", files(b1, b2), True, ["build paths"])
    check("a binary's build paths of different length", files(b1, b3), False, hint=True)
    check("a byte beside a binary's build path", files(b1, b2.replace(b"\x00/x", b"\x01/x")), False)

    def pyc(src, mtime=None):
        import importlib.util
        code = compile(src, "/usr/lib/python3.14/_sysconfigdata.py", "exec")
        if mtime is None:       # hash-based (flags 3, as compileall's checked-hash), else Buildroot's timestamp-based
            head = struct.pack("<I", 3) + hashlib.sha256(src.encode()).digest()[:8]
        else:
            head = struct.pack("<III", 0, mtime, len(src))
        return importlib.util.MAGIC_NUMBER + head + marshal.dumps(code)
    src1 = ("build_time_vars = {'LIBDIR': '/x/output/cmp-a/rgds-plus/host/lib', 'N': 1}\n"
            "def f():\n    return ('/x/output/cmp-a/rgds-plus/target/usr', b'/x/output/cmp-a/rgds-plus/host/')\n")
    src2 = src1.replace("cmp-a/rgds-plus/host", "cmp-b/rgds-plus/per-package/python3/host")
    p1, p2, p3 = pyc(src1), pyc(src2), pyc(src1.replace("'N': 1", "'N': 2"))
    check("a .pyc's build paths", files(p1, p2, "root-a: _sysconfigdata.pyc"), True, ["build paths"])
    check("a .pyc's other constant", files(p1, p3, "root-a: _sysconfigdata.pyc"), False)
    check("a .pyc's source time", files(pyc(src1, 1761498132), pyc(src1, 1762665342), "root-a: os.pyc"), True,
          ["times"])
    check("a .pyc whose source only moved", files(pyc(src1, 1761498132), pyc("\n" + src1, 1761498132), "root-a: os.pyc"),
          False)

    # the GPT: GUIDs and CRCs excused, a partition's last LBA not
    def gpt(seed, last):
        ents = bytearray(128 * 128)
        for i, (first, end, name) in enumerate(((32768, 163839, "boot"), (163840, last, "banana-root-a"))):
            ents[i * 128:(i + 1) * 128] = (hashlib.md5(b"type").digest() + hashlib.md5(seed + bytes([i])).digest()
                                           + struct.pack("<QQQ", first, end, 0) + name.encode("utf-16-le").ljust(72, b"\0"))
        h = bytearray(GPT_HDR.pack(b"EFI PART", 0x10000, 92, 0, 0, 1, 1212455, 18765, 1212422,
                                   hashlib.md5(seed).digest(), 2, 128, 128, zlib.crc32(ents)) + bytes(420))
        struct.pack_into("<I", h, 16, zlib.crc32(h[:92]))
        return bytes(h), bytes(ents)
    g1, g2, g3 = gpt(b"a", 688127), gpt(b"b", 688127), gpt(b"b", 688000)
    check("GPT GUIDs and CRCs", lambda rep: compare_gpt(rep, "GPT", *g1, *g2), True, ["random IDs"])
    check("GPT, a partition's last LBA", lambda rep: compare_gpt(rep, "GPT", *g1, *g3), False)

    # the squashfs listing: times excused, a mode not; a bookkeeping file's contents excused, its absence not
    l1 = ("drwxr-xr-x 0/0                     285 2026-11-09 10:55 squashfs-root\n"
          "drwxr-xr-x 0/0                    1044 2026-11-09 10:55 squashfs-root/bin\n"
          "lrwxrwxrwx 0/0                       7 2026-10-26 18:54 squashfs-root/bin/sh -> busybox\n"
          "-rwsr-xr-x 0/0                 1009336 2026-10-26 18:54 squashfs-root/bin/busybox\n"
          "-rw-r--r-- 0/0                     812 2026-10-26 18:54 squashfs-root/usr/share/glsl/.files-list-host.before\n")
    l2 = l1.replace("2026-10-26 18:54", "2026-11-09 11:02")
    check("listing times", lambda rep: compare_listing(rep, "root-a", parse_listing(l1), parse_listing(l2)), True,
          ["times"])
    check("listing, a mode", lambda rep: compare_listing(rep, "root-a", parse_listing(l1),
                                                         parse_listing(l2.replace("-rwsr-xr-x", "-rwxr-xr-x"))), False)
    check("a bookkeeping file's contents", files(b"1234 5678\n", b"8765 4321\n", "usr/share/glsl/.files-list-host.before"),
          True, ["bookkeeping"])
    l3 = "".join(x + "\n" for x in l1.splitlines() if ".files-list" not in x)
    check("a bookkeeping file on one side only",
          lambda rep: compare_listing(rep, "root-a", parse_listing(l1), parse_listing(l3)), False)

    # an ELF file linked again around a longer blanked RPATH: every section after .dynstr moves, and each address of
    # what moved with it; anything else changed as well is a difference
    old, _ = test_elf(0)
    new, at = test_elf(19)
    check("an ELF file linked around a longer path", files(old, new, "usr/bin/x"), True, ["build paths"])
    check("an ELF file the same", files(old, test_elf(0)[0], "usr/bin/x"), True)
    for name, sec, off, value in (
            ("an instruction", ".text", 12, 0xAA0003E4),                     # mov x4, x0 instead of x3
            ("a call's target", ".text", 0, None),
            ("an adrp page's offset", ".text", 8, None),
            ("an offset from an adrp after it", ".text", 24, None),
            ("a branch's target", ".text", 20, None),
            ("a string in .rodata", ".rodata", 5, 0x4A),
            ("a pointer in .data", ".data", 0, None),
            ("the GOT's lazy slot", ".got", 8, None),
            ("a symbol's value", ".dynsym", 24 + 8, None),
            ("a symbol's name", ".dynstr", 12, 0x70),
            ("a relocation's address", ".rela.dyn", 16, None),
            ("an FDE's start", ".eh_frame", 32, None),
            (".eh_frame_hdr's table", ".eh_frame_hdr", 12, None),
            (".dynamic's string table", ".dynamic", 16 + 8, None),
            ("a section header's address", "sh:.plt", 16, None)):
        bad = bytearray(new)
        o = at[sec] + off
        w = struct.unpack_from("<I", bad, o)[0]
        if value is None:                       # an address 8 further; in code, an instruction further
            value = w + (8 if sec != ".text" else 8 << 10 if lo12(w) else 1 << 5 if w >> 24 == 0x54 else 1)
        if value > 0xFF:
            struct.pack_into("<I", bad, o, value)
        else:
            bad[o] = value
        check(f"an ELF file linked around a longer path, and {name} changed", files(old, bytes(bad), "usr/bin/x"),
              False)
    bad = bytearray(old)
    struct.pack_into("<I", bad, at[".text"] + 12, 0xAA0003E4)
    check("an ELF file laid out the same, an instruction changed", files(old, bytes(bad), "usr/bin/x"), False)
    po, _ = test_elf(0, b"../../host/usr/include/python3.14/x.h")
    pn, pat = test_elf(0, b"../../per-package/util-linux/host/usr/include/python3.14/x.h")
    check("an ELF file with a longer include path in .rodata", files(po, pn, "usr/lib/x.so"), True, ["build paths"])
    bad = bytearray(pn)
    bad[pat[".rodata"] + 6] ^= 1
    check("... and the path's other characters changed", files(po, bytes(bad), "usr/lib/x.so"), False)
    print(f"image-compare self-test: {len(checks)} checks passed")


def test_elf(extra, path=b"hello"):
    """A small AArch64 ELF file as the self-test's: its .dynstr holds a path blanked to 415 X's, and extra more after a
    NUL (a longer path), so all after it moves; .rodata holds path, then "world". Returns it, and where its sections
    (and, as "sh:<name>", their headers) are in the file."""
    def ins_adrp(rd, pc, t):
        imm = (t >> 12) - (pc >> 12)
        return 0x90000000 | (imm & 3) << 29 | (imm >> 2 & 0x7FFFF) << 5 | rd

    def ins_adr(rd, pc, t):
        imm = t - pc
        return 0x10000000 | (imm & 3) << 29 | (imm >> 2 & 0x7FFFF) << 5 | rd

    def ins_b(op, pc, t):
        return op | ((t - pc) >> 2 & 0x3FFFFFF)
    dynstr = b"\0libc.so.6\0foo\0" + b"X" * 415 + (b"\0" + b"X" * (extra - 1) if extra else b"") + b"\0bar\0"
    bar = len(dynstr) - 4
    rodata = b"\0" * 4 + path + b"\0"
    rodata += bytes(-len(rodata) % 8)
    world = len(rodata)
    rodata += b"world\0\0\0"
    names = [".dynsym", ".dynstr", ".rela.dyn", ".plt", ".text", ".rodata", ".eh_frame_hdr", ".eh_frame", ".dynamic",
             ".got", ".data", ".bss", ".shstrtab"]
    sizes = [72, len(dynstr), 48, 16, 40, len(rodata), 20, 52, 144, 24, 8, 16, 0]
    aligns = [8, 1, 8, 16, 16, 8, 4, 8, 8, 8, 8, 16, 1]
    shstrtab = b"\0" + b"".join(n.encode() + b"\0" for n in names)
    sizes[-1] = len(shstrtab)
    addr, off, pos, rw = {}, {}, 0x200, None
    for n, size, al in zip(names, sizes, aligns):
        pos += -pos % al
        if n == ".dynamic":
            rw = pos
        off[n] = pos
        addr[n] = pos + (0x10000 if rw is not None and n != ".shstrtab" else 0)
        if n != ".bss":
            pos += size
    off[".shstrtab"] = off[".bss"]
    end = off[".shstrtab"] + len(shstrtab)
    shoff = end + -end % 8
    d = bytearray(shoff + 14 * 64)
    A = addr
    d[off[".dynstr"]:off[".dynstr"] + len(dynstr)] = dynstr
    struct.pack_into("<IBBHQQ", d, off[".dynsym"] + 24, 11, 0x12, 0, 5, A[".text"], 40)
    struct.pack_into("<IBBHQQ", d, off[".dynsym"] + 48, bar, 0x11, 0, 11, A[".data"], 8)
    struct.pack_into("<QQq", d, off[".rela.dyn"], A[".data"], 1027, A[".rodata"] + 4)
    struct.pack_into("<QQq", d, off[".rela.dyn"] + 24, A[".got"] + 16, 1 << 32 | 1025, 0)
    got1 = A[".got"] + 8
    plt = [ins_adrp(16, A[".plt"], got1), 0xF9400211 | (got1 & 0xFFF) >> 3 << 10,
           0x91000210 | (got1 & 0xFFF) << 10, 0xD61F0220]
    t = A[".text"]
    text = [ins_b(0x94000000, t, A[".plt"]),                                 # bl to the PLT
            ins_adrp(0, t + 4, A[".rodata"] + 4), 0x91000000 | (A[".rodata"] + 4 & 0xFFF) << 10,
            0xAA0003E3,                                                     # mov x3, x0
            ins_adr(1, t + 16, A[".rodata"] + world),
            0x54000001 | ((t - (t + 20)) >> 2 & 0x7FFFF) << 5,              # b.ne to the start
            0xF9400265 | (A[".data"] & 0xFFF) >> 3 << 10,                   # ldr x5, [x19, :lo12:data]
            0xD65F03C0,                                                     # ret
            ins_adrp(19, t + 32, A[".data"]), ins_b(0x14000000, t + 36, t + 24)]
    struct.pack_into("<4I", d, off[".plt"], *plt)
    struct.pack_into("<10I", d, off[".text"], *text)
    d[off[".rodata"]:off[".rodata"] + len(rodata)] = rodata
    eh, hdr = off[".eh_frame"], off[".eh_frame_hdr"]
    struct.pack_into("<IIB3sBBBBB3s5x", d, eh, 20, 0, 1, b"zR\0", 4, 0x78, 30, 1, 0x1B, b"\x0c\x1f\x00")
    struct.pack_into("<IIiIB", d, eh + 24, 20, 28, A[".text"] - (A[".eh_frame"] + 32), 40, 0)
    struct.pack_into("<BBBBiIii", d, hdr, 1, 0x1B, 0x03, 0x3B, A[".eh_frame"] - (A[".eh_frame_hdr"] + 4), 1,
                     A[".text"] - A[".eh_frame_hdr"], A[".eh_frame"] + 24 - A[".eh_frame_hdr"])
    for k, (tag, v) in enumerate(((1, 1), (5, A[".dynstr"]), (6, A[".dynsym"]), (10, len(dynstr)), (11, 24),
                                  (7, A[".rela.dyn"]), (8, 48), (9, 24), (0, 0))):
        struct.pack_into("<qQ", d, off[".dynamic"] + 16 * k, tag, v)
    struct.pack_into("<QQQ", d, off[".got"], A[".dynamic"], A[".plt"], 0)
    struct.pack_into("<Q", d, off[".data"], A[".rodata"] + 4)
    d[off[".shstrtab"]:end] = shstrtab
    rx_end = off[".eh_frame"] + 52
    struct.pack_into("<HHIQQQIHHHHHH", d, 16, 3, EM_AARCH64, 1, A[".text"], 64, shoff, 0, 64, 56, 3, 64, 14, 13)
    d[:16] = b"\x7fELF\x02\x01\x01" + bytes(9)
    struct.pack_into("<IIQQQQQQ", d, 64, 1, 5, 0, 0, 0, rx_end, rx_end, 0x10000)
    rw_size = off[".data"] + 8 - rw
    struct.pack_into("<IIQQQQQQ", d, 120, 1, 6, rw, A[".dynamic"], A[".dynamic"], rw_size,
                     A[".bss"] + 16 - A[".dynamic"], 0x10000)
    struct.pack_into("<IIQQQQQQ", d, 176, 2, 6, off[".dynamic"], A[".dynamic"], A[".dynamic"], 144, 144, 8)
    types = [11, 3, 4, 1, 1, 1, 1, 1, 6, 1, 1, 8, 3]
    flags = [2, 2, 2, 6, 6, 2, 2, 2, 3, 3, 3, 3, 0]
    links = {".dynsym": 2, ".rela.dyn": 1, ".dynamic": 2}
    infos = {".dynsym": 1}
    entsizes = {".dynsym": 24, ".rela.dyn": 24, ".dynamic": 16, ".got": 8}
    where = dict(off)
    for k, n in enumerate(names):
        o = shoff + 64 * (k + 1)
        struct.pack_into("<IIQQQQIIQQ", d, o, shstrtab.index(n.encode() + b"\0"), types[k], flags[k],
                         A[n] if flags[k] else 0, off[n], sizes[k], links.get(n, 0), infos.get(n, 0), aligns[k],
                         entsizes.get(n, 0))
        where["sh:" + n] = o
    return bytes(d), where


def main(argv):
    if argv == ["--self-test"]:
        self_test()
        return 0
    try:
        i = argv.index("--work")
        work = argv[i + 1]
        o, n = argv.index("--old"), argv.index("--new")
        old, new = Side("old", *argv[o + 1:o + 4]), Side("new", *argv[n + 1:n + 4])
    except (ValueError, IndexError, TypeError):
        print("usage: image-compare.py --work <dir> --old <img.gz> <update.tar> <build path> "
              "--new <img.gz> <update.tar> <build path>\n       image-compare.py --self-test", file=sys.stderr)
        return 2
    rep = Report(len(old.path) != len(new.path))
    if rep.paths_differ:
        log(f"warning: the builds' paths differ in length ({old.path}, {new.path}): a binary holding one can't be "
            "compared, and its differences fail")
    print(f"old: {old.img_gz}, {old.tar} (built in {old.path})")
    print(f"new: {new.img_gz}, {new.tar} (built in {new.path})\n", flush=True)
    try:
        compare(rep, old, new, work)
    except Fail as e:
        rep.write(sys.stdout)
        print(f"image-compare: {e}", file=sys.stderr)
        return 2
    rep.write(sys.stdout)
    return 1 if rep.diffs else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
