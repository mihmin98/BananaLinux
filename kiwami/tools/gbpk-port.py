#!/usr/bin/env python3
# The Game Boy Pokémon companion's addresses for a ROM hack that publishes no source (Crystal Clear): pret's symbols for
# the retail game it was built from (pokecrystal11.sym, Crystal rev 1) ported into each of the hack's builds through
# the code that uses them, from the ROMs the user owns, written to kiwami/tools/gbpk-port.json. Only addresses, ids and
# the ROM header's facts are taken, none of the hack's code or tables (kiwami/GB-POKEMON.md, *Where the data comes
# from*).
# Copyright (C) 2026 Lidor Itzhari (PokeDaisy) and the kiwami fork's contributors. GNU GPL version 3 with PokeDaisy's
# additional term under section 7(b): kiwami/rootfs/usr/share/banana/companions/gb-pokemon/LICENSE and NOTICE.
# SPDX-License-Identifier: GPL-3.0-only
# Based on PokeDaisy by Lidor Itzhari - https://github.com/lidor30/pokedaisy (its scripts/port_gen1.py's method)
#
#   gbpk-port.py <pret folder> <roms folder>            write gbpk-port.json and SOURCES' [port]
#   gbpk-port.py <pret folder> <roms folder> --check    compare what they give with what's committed
#   gbpk-port.py --check                                without them: the committed files against SOURCES' hashes,
#                                                       and every port line of gbpk-symbols.txt answered in the output
#   gbpk-port.py --self-test                            the methods on made-up ROMs
#
# <pret folder> holds pret's checkout of the base game's repository with its symbols branch (pret-extract.py's). The
# ROMs are found in <roms folder> and its subfolders one level down by their SHA-1s, whatever their names: the base
# release (its SHA-1 from the repository's roms.sha1) and every build gbpk-symbols.txt's port lines name.
#
# Each value a ported game asks for (its games' ram., rom. and const. lines in gbpk-symbols.txt) is found in every build
# by one method, the votes unless a port line names another:
#   votes            every use of the base's symbol in the base's code (ld a,[nn], ld [nn],a, ld hl/de/bc,nn, call,
#                    jp, ...; a ROM symbol's only where the code is in the symbol's bank or loads it with ld a,n), the
#                    10 bytes each side of it (other 16-bit operands masked; failing that, ld a,n and jr's offsets
#                    too) found once in the build: the operand there votes, with the bank the build's ld a,n loads at
#                    the same place, or the bank the build's code is in. Two votes at least, and any other value
#                    under an eighth of the winner's (code the build reused), or the run fails with every value
#   port.data        the base's first <len> bytes at the symbol, found once in the build: a table the hack kept
#   port.code        a pattern of the build's bytes, found once: hex bytes, .. any byte, {addr} the value's two bytes,
#                    {bank} its bank's byte, {byte} a constant, {hi} a constant's high byte (the constant is it * 256);
#                    several patterns joined by / each found once, their captures together. Without {bank}, a ROM
#                    value in 4000-7FFF is in the bank of the code that names it. A pattern also found in the base must
#                    be found once there and give the base's own value: then it is the code that uses the symbol
#   port.count       a constant that is a table's length: its entries of <size> bytes up to the lowest of the
#                    pointers at <offset> in each (the table followed by what it points at), the same rule giving the
#                    base's constant on the base's table
#   port.same        a constant the base's, as the table named with it is the base's bytes (port.data, its length
#                    covering every entry)
#   port.fixed       a constant of the hack's own layout that its code holds in its shape rather than as a byte (six
#                    add hl,de for 6-byte entries, bit 4,a for a type's bit 16): the line's <value>, given only if the
#                    pattern (port.code's, without captures) is found once in the build
#   port.trainer     a trainer class: the build's class whose name (rom.trainerClassNames) and first trainer's name
#                    (the bytes to the 0x50 that ends each) are the base's class's, its constant's, compared in the
#                    two ROMs and found in one class of the build: the build's groups a bank and a pointer a class (a
#                    rom. key found before it, const.numTrainerClasses of them), the base's a pointer a class (its
#                    symbol)
# and its items and types by `<game> items|types from <base game>`: the base game's items (gbpk-extra.json's) by their
# names in each ROM's ItemNames, and its types as they are, its type chart being the base's (port.data over the whole
# chart, to its -1). A version's builds (the first word of the release's name) must agree on every value; where the
# versions disagree each release has its own (*The data files*). Python 3, standard library only.
import hashlib
import importlib.util
import json
import os
import random
import re
import sys
import tempfile

# pret-extract.py's helpers (the list's file, the symbols, SOURCES)
_spec = importlib.util.spec_from_file_location('pret_extract',
                                               os.path.join(os.path.dirname(os.path.abspath(__file__)), 'pret-extract.py'))
assert _spec and _spec.loader
pe = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(pe)
pe.PROG = 'gbpk-port'
fail = pe.fail
REPO, LIST, EXTRA, SOURCES = pe.REPO, pe.LIST, pe.OUT, pe.SOURCES
OUT = 'kiwami/tools/gbpk-port.json'
SECTION = 'port'

A16 = {0xFA, 0xEA, 0x21, 0x11, 0x01, 0x31, 0x08, 0xCD, 0xC3, 0xC2, 0xCA, 0xD2, 0xDA, 0xC4, 0xCC, 0xD4, 0xDC}
LOAD_OPS = {0xFA, 0xEA, 0x21, 0x11, 0x01, 0x31, 0x08}              # a WRAM address's uses
PTR_OPS = {0x21, 0x11, 0x01, 0xCD, 0xC3, 0xC2, 0xCA, 0xD2, 0xDA, 0xC4, 0xCC, 0xD4, 0xDC}   # a ROM address's
JR_OPS = {0x18, 0x20, 0x28, 0x30, 0x38}
LD_A = 0x3E
WINDOW = 10
MIN_VOTES = 2
STRAY = 8           # another value with under 1/STRAY of the winner's votes is a stray (code the hack reused)
METHODS = ('port.data', 'port.code', 'port.count', 'port.same', 'port.fixed', 'port.trainer')
CONST_METHODS = ('port.count', 'port.same', 'port.fixed', 'port.trainer')


class PortError(Exception):
    pass


def where(o):
    return o // 0x4000, o % 0x4000 + (0x4000 if o >= 0x4000 else 0)


def file_off(bank, addr):
    return addr if addr < 0x4000 else bank * 0x4000 + addr - 0x4000


def show(v):
    return '%02X:%04X' % v if isinstance(v, tuple) else str(v)


# --- The list ---

def read_port_list(text):
    """gbpk-symbols.txt's lines for the ported games (those with port lines): game -> its builds and asks."""
    rows = []
    for n, line in enumerate(text.splitlines(), 1):
        line = line.split('#', 1)[0].strip()
        if line:
            rows.append((n, line))
    ported = {}
    for n, line in rows:
        p = line.split(None, 2)
        if len(p) == 3 and p[1] == 'port':
            a = p[2].split(None, 3)
            if ',' in p[0] or len(a) != 4 or not re.fullmatch(r'[0-9a-f]{40}', a[2]):
                fail('%s:%d: a port line is <game> port <repo> <build> <sha1> <name>' % (LIST, n))
            g = ported.setdefault(p[0], {'builds': [], 'asks': {}, 'methods': {}, 'from': {}})
            base = (a[0], a[1])
            if g.setdefault('base', base) != base:
                fail('%s:%d: %s is ported from two builds' % (LIST, n, p[0]))
            g['builds'].append({'sha1': a[2], 'name': a[3]})
    for n, line in rows:
        p = line.split(None, 2)
        if p[0] == 'item' or len(p) < 3:
            continue
        games = [x for x in p[0].split(',') if x in ported]
        if not games:
            continue
        kind = p[1]
        if kind == 'port':
            continue
        if kind in METHODS or kind in ('items', 'types'):
            if len(games) != 1 or ',' in p[0]:
                fail('%s:%d: a %s line names one ported game' % (LIST, n, kind))
            g = ported[games[0]]
            if kind in ('items', 'types'):
                a = p[2].split()
                if len(a) != 2 or a[0] != 'from':
                    fail('%s:%d: a ported game\'s %s are <game> %s from <base game>' % (LIST, n, kind, kind))
                g['from'][kind] = (a[1], n)
                continue
            key, _, rest = p[2].partition(' ')
            if not re.fullmatch(r'(ram|rom|const)\.\w+', key) or not rest.strip():
                fail('%s:%d: %s <kind>.<key> ...' % (LIST, n, kind))
            if key in g['methods']:
                fail('%s:%d: %s %s has two methods' % (LIST, n, games[0], key))
            g['methods'][key] = (kind, rest.split(), n)
            continue
        k, dot, key = kind.partition('.')
        if k not in ('ram', 'rom', 'const') or not dot:
            fail('%s:%d: %s is ported: %s lines aren\'t for it' % (LIST, n, games[0], kind))
        a = p[2].split()
        for gm in games:
            if kind in ported[gm]['asks']:
                fail('%s:%d: %s %s given twice' % (LIST, n, gm, kind))
            ported[gm]['asks'][kind] = (a, n)    # (the base's symbol, or a const's file and name)
    for name, g in ported.items():
        for key, (m, a, n) in g['methods'].items():
            if key.startswith('const.') != (m in CONST_METHODS) and m != 'port.code':
                fail('%s:%d: %s %s' % (LIST, n, m, 'finds a table' if m == 'port.data' else 'is for a constant'))
            if key not in g['asks'] and m not in ('port.code', 'port.fixed'):
                fail('%s:%d: %s has no line naming the base\'s symbol for %s' % (LIST, n, name, key))
        for key, (a, n) in g['asks'].items():
            if key.startswith('const.') and key not in g['methods']:
                fail('%s:%d: %s has no source of its own: %s needs a port line' % (LIST, n, name, key))
    return ported


def compile_pattern(tokens, n):
    """A port.code pattern: its regexes (one each side of a /) and what each group captures."""
    out, rx, caps = [], b'', []
    for t in tokens + ['/']:
        if t == '/':
            if not rx:
                fail('%s:%d: an empty pattern' % (LIST, n))
            out.append((re.compile(rx, re.S), caps))
            rx, caps = b'', []
        elif t == '..':
            rx += b'.'
        elif t in ('{addr}', '{bank}', '{byte}', '{hi}'):
            caps.append(t[1:-1])
            rx += b'(..)' if t == '{addr}' else b'(.)'
        elif re.fullmatch(r'[0-9a-fA-F]{2}', t):
            rx += re.escape(bytes([int(t, 16)]))
        else:
            fail('%s:%d: %s isn\'t a byte, .., {addr}, {bank}, {byte}, {hi} or /' % (LIST, n, t))
    allc = [c for _, cs in out for c in cs]
    if len(allc) != len(set(allc)):
        fail('%s:%d: a capture given twice' % (LIST, n))
    return out


# --- The methods ---

def masked(d, lo, hi, keep, loose):
    """A regex for d[lo:hi]: the bytes as they are, a 16-bit operand after an a16 opcode any two bytes but the one being
    ported at keep (the group); loose, ld a,n's immediate and jr's offset any byte too."""
    out, i = b'', lo
    while i < hi:
        if i == keep:
            out += b'(..)'
            i += 2
            continue
        b = d[i]
        out += re.escape(bytes([b]))
        if b in A16 and i + 2 < hi and keep not in (i + 1, i + 2):
            out += b'..'
            i += 3
            continue
        if loose and (b == LD_A or b in JR_OPS) and i + 1 < hi and keep != i + 1:
            out += b'.'
            i += 2
            continue
        i += 1
    return re.compile(out, re.S)


def vote_sites(base, bank, addr):
    """The base's uses of a symbol: (operand's offset, window, the window's ld a,BANK operand or None)."""
    rom = addr < 0x8000
    ops = PTR_OPS if rom else LOAD_OPS
    out = []
    for m in re.finditer(re.escape(bytes([addr & 0xFF, addr >> 8])), base):
        i = m.start()
        if i == 0 or base[i - 1] not in ops:
            continue
        lo, hi = max(0, i - 1 - WINDOW), min(len(base), i + 2 + WINDOW)
        lda = None
        if rom and addr >= 0x4000:
            ld = [j + 1 for j in range(lo, hi - 1) if base[j] == LD_A and base[j + 1] == bank]
            if i // 0x4000 != bank and not ld:
                continue            # code of another bank naming its own address
            lda = ld[0] if ld else None
        out.append((i, lo, hi, lda))
    return out


def votes(base, hack, sites, bank, addr):
    rom = addr < 0x8000
    v = {}
    for i, lo, hi, lda in sites:
        found = []
        for loose in (False, True):
            found = list(masked(base, lo, hi, i, loose).finditer(hack))
            if len(found) == 1:
                break
        if len(found) != 1:
            continue
        m = found[0]
        new = m.group(1)[0] | m.group(1)[1] << 8
        if not rom:
            b = (1 if 0xD000 <= new <= 0xDFFF else 0)
        elif addr < 0x4000:
            b = 0
        elif lda is not None:
            b = hack[m.start() + lda - lo]
        elif (m.start() + i - lo) // 0x4000:
            b = (m.start() + i - lo) // 0x4000
        else:
            continue
        v[(b, new)] = v.get((b, new), 0) + 1
    return v


def decide(v, what):
    if not v:
        raise PortError('%s: no use of it found (no votes)' % what)
    best = sorted(v.items(), key=lambda kv: (-kv[1], kv[0]))
    top, n = best[0]
    others = sum(c for _, c in best[1:])
    if n < MIN_VOTES or any(c * STRAY >= n for _, c in best[1:]):
        raise PortError('%s: no clear value: %s' % (what, ', '.join('%s x%d' % (show(k), c) for k, c in best)))
    return top, 'votes %d' % n + (' (%d stray)' % others if others else '')


def find_code(pats, d, rom_value, what):
    """A port.code pattern in d: its captures, or None if it isn't there; more than once fails."""
    caps = {}
    for i, (rx, names) in enumerate(pats):
        found = list(rx.finditer(d))
        if not found:
            return None
        if len(found) > 1:
            raise PortError('%s: pattern %d found %d times (%s)' % (what, i + 1, len(found),
                            ', '.join(show(where(m.start())) for m in found[:4])))
        m = found[0]
        for gi, name in enumerate(names, 1):
            g = m.group(gi)
            caps[name] = (g[0] | g[1] << 8) if name == 'addr' else g[0]
            if name == 'addr':
                caps['at'] = m.start(gi)        # (where the code naming it is: its bank)
    return caps


def code_value(kind, caps, what):
    if kind == 'const':
        if 'byte' in caps:
            return caps['byte']
        if 'hi' in caps:
            return caps['hi'] << 8
        raise PortError('%s: a constant\'s pattern captures {byte} or {hi}' % what)
    if 'addr' not in caps:
        raise PortError('%s: an address\'s pattern captures {addr}' % what)
    a = caps['addr']
    if kind == 'ram':
        if not (0xC000 <= a <= 0xDFFF or 0xFF80 <= a <= 0xFFFE):
            raise PortError('%s: %04X isn\'t in WRAM or HRAM' % (what, a))
        return (1 if 0xD000 <= a <= 0xDFFF else 0, a)
    if a >= 0x8000:
        raise PortError('%s: %04X isn\'t in the ROM' % (what, a))
    if a < 0x4000:
        return (0, a)
    if 'bank' in caps:
        return (caps['bank'], a)
    b = caps['at'] // 0x4000
    if not b:
        raise PortError('%s: bank 0\'s code names %04X: the pattern needs {bank}' % (what, a))
    return (b, a)


def table_count(d, off, size, ptr):
    n, low = 0, 0x8000
    while off % 0x4000 + 0x4000 + size * n < low and n < 4096:
        p = d[off + size * n + ptr] | d[off + size * n + ptr + 1] << 8
        low = min(low, p)
        n += 1
    return n


def first_name(d, off):
    """The bytes of the name at off, to the 0x50 that ends it (a trainer's group's first), or None."""
    if off is None or off >= len(d):
        return None
    e = d.find(b'\x50', off, off + 16)
    return d[off:e + 1] if e >= 0 else None


def names_list(d, off, count):
    out = []
    for _ in range(count):
        e = d.index(b'\x50', off)
        out.append(d[off:e])
        off = e + 1
    return out


# --- Porting a game ---

class Port:
    def __init__(self, name, g, sym, base_rom, base_game):
        self.name, self.g, self.sym, self.base, self.bg = name, g, sym, base_rom, base_game
        self.sites = {}

    def symbol(self, key):
        a = self.g['asks'].get(key)
        if a is None:
            return None
        if key.startswith('const.'):
            return a[0][1]
        s = a[0][0]
        if s not in self.sym:
            fail('%s:%d: %s isn\'t in the base\'s symbols' % (LIST, a[1], s))
        return s

    def base_value(self, key):
        """The base's own value for a key: its symbol's address, or the base game's constant."""
        kind = key.split('.')[0]
        s = self.symbol(key)
        if s is None:
            return None
        if kind == 'const':
            c = self.bg['const'].get(key[6:])
            return c['value'] if c else None
        bank, addr = self.sym[s]
        if kind == 'ram':
            return (1 if 0xD000 <= addr <= 0xDFFF else 0, addr)
        return (bank if addr >= 0x4000 else 0, addr)

    def one(self, key, rom, values):
        """A key's value in one build, with how it was found."""
        kind = key.split('.')[0]
        what = '%s %s' % (self.name, key)
        m = self.g['methods'].get(key)
        if m is None:
            s = self.symbol(key)
            bank, addr = self.sym[s]
            if s not in self.sites:
                self.sites[s] = vote_sites(self.base, bank, addr)
            return decide(votes(self.base, rom, self.sites[s], bank, addr), '%s (%s)' % (what, s))
        method, args, n = m
        if method == 'port.data':
            if len(args) != 1 or not args[0].isdigit():
                fail('%s:%d: port.data <key> <length>' % (LIST, n))
            bank, addr = self.sym[self.symbol(key)]
            o = file_off(bank, addr)
            pat = self.base[o:o + int(args[0])]
            found = [x.start() for x in re.finditer(re.escape(pat), rom)]
            if len(found) != 1:
                raise PortError('%s: the base\'s %s bytes found %d times' % (what, args[0], len(found)))
            b, a = where(found[0])
            return ((b, a) if kind == 'rom' else (1 if 0xD000 <= a <= 0xDFFF else 0, a)), 'data %s' % args[0]
        if method == 'port.code':
            pats = compile_pattern(args, n)
            caps = find_code(pats, rom, kind == 'rom', what)
            if caps is None:
                raise PortError('%s: the pattern isn\'t found' % what)
            v = code_value(kind, caps, what)
            # (the pattern in the base: the code that uses the symbol only if it gives the base's own value there)
            bcaps = find_code(pats, self.base, kind == 'rom', what + ' in the base')
            if bcaps is None:
                return v, 'code'
            bv, own = code_value(kind, bcaps, what + ' in the base'), self.base_value(key)
            if own is None:
                return v, 'code'
            if bv != own:
                raise PortError('%s: the pattern gives %s in the base, not its %s' % (what, show(bv), show(own)))
            return v, 'code, the base\'s too'
        if method == 'port.count':
            if len(args) != 3 or not args[0].startswith('rom.') or not args[1].isdigit() or not args[2].isdigit():
                fail('%s:%d: port.count const.<key> rom.<table> <entry size> <pointer offset>' % (LIST, n))
            size, ptr = int(args[1]), int(args[2])
            if args[0] not in values:
                fail('%s:%d: %s comes after %s in the list' % (LIST, n, key, args[0]))
            t = values[args[0]][0]
            got = table_count(rom, file_off(*t), size, ptr)
            bt = self.base_value(args[0])
            own = self.base_value(key)
            if table_count(self.base, file_off(*bt), size, ptr) != own:
                raise PortError('%s: the count gives the base %d, not its %s' %
                                (what, table_count(self.base, file_off(*bt), size, ptr), own))
            return got, 'count'
        if method == 'port.same':
            if len(args) != 1 or args[0] not in self.g['methods'] or self.g['methods'][args[0]][0] != 'port.data':
                fail('%s:%d: port.same const.<key> rom.<table found by port.data>' % (LIST, n))
            own = self.base_value(key)
            if own is None:
                fail('%s:%d: the base game has no %s' % (LIST, n, key))
            return own, 'the base\'s'
        if method == 'port.fixed':
            if len(args) < 2 or not args[0].isdigit():
                fail('%s:%d: port.fixed const.<key> <value> <pattern>' % (LIST, n))
            pats = compile_pattern(args[1:], n)
            if any(caps for _, caps in pats):
                fail('%s:%d: a port.fixed pattern captures nothing' % (LIST, n))
            if find_code(pats, rom, False, what) is None:
                raise PortError('%s: the pattern of its code isn\'t found' % what)
            return int(args[0]), 'fixed by its code'
        if method == 'port.trainer':
            if len(args) != 2 or not args[0].startswith('rom.') or args[1] not in self.sym:
                fail('%s:%d: port.trainer const.<key> rom.<groups> <the base\'s groups symbol>' % (LIST, n))
            if any(k not in values for k in (args[0], 'const.numTrainerClasses', 'rom.trainerClassNames')):
                fail('%s:%d: %s comes after %s, const.numTrainerClasses and rom.trainerClassNames in the list' %
                     (LIST, n, key, args[0]))
            own = self.base_value(key)
            if own is None:
                fail('%s:%d: the base game has no %s' % (LIST, n, key))
            bank, addr = self.sym[args[1]]
            p = file_off(bank, addr) + 2 * (own - 1)
            ptr = self.base[p] | self.base[p + 1] << 8
            name = first_name(self.base, file_off(bank, ptr))
            if not name:
                raise PortError('%s: the base\'s class %d has no first trainer\'s name' % (what, own))
            cls = names_list(self.base, file_off(*self.sym[self.symbol('rom.trainerClassNames')]), own)[own - 1]
            t, count = file_off(*values[args[0]][0]), values['const.numTrainerClasses'][0]
            classes = names_list(rom, file_off(*values['rom.trainerClassNames'][0]), count)
            hits = [c + 1 for c in range(count) if classes[c] == cls and
                    first_name(rom, file_off(rom[t + 3 * c], rom[t + 3 * c + 1] | rom[t + 3 * c + 2] << 8)) == name]
            if len(hits) != 1:
                raise PortError('%s: the base\'s class %d, by its name and its first trainer\'s, is %d classes' %
                                (what, own, len(hits)))
            return hits[0], 'by its class\'s and first trainer\'s names'
        fail('%s:%d: unknown method %s' % (LIST, n, method))

    def keys(self):
        out = [k for k in self.g['asks']] + [k for k in self.g['methods'] if k not in self.g['asks']]
        # (counts after the tables they count)
        return sorted(out, key=lambda k: self.g['methods'].get(k, ('',))[0] in ('port.count', 'port.same', 'port.trainer'))

    def build(self, rom):
        values, bad = {}, []
        for key in self.keys():
            try:
                values[key] = self.one(key, rom, values)
            except PortError as e:
                bad.append(str(e))
        return values, bad


def items_types(port, roms, extra_games):
    """`items from` and `types from`: the base game's, its items by their names in each ROM."""
    out = {}
    g = port.g
    for kind in ('items', 'types'):
        if kind not in g['from']:
            fail('%s: %s has no %s line' % (LIST, port.name, kind))
        src, n = g['from'][kind]
        if src not in extra_games:
            fail('%s:%d: %s isn\'t in %s' % (LIST, n, src, EXTRA))
        sg = extra_games[src]
        if kind == 'types':
            m = g['methods'].get('rom.typeChart')
            bank, addr = port.sym[port.symbol('rom.typeChart')]
            o, end = file_off(bank, addr), file_off(bank, addr)
            while port.base[end] != 0xFF:
                end += 1 if port.base[end] == 0xFE else 3
            if not m or m[0] != 'port.data' or int(m[1][0]) < end - o + 1:
                fail('%s:%d: types from %s need rom.typeChart found by port.data over its %d bytes' %
                     (LIST, n, src, end - o + 1))
            out.update({'types': sg['types'], 'special': sg['special']})
            continue
        bank, addr = port.sym[port.symbol('rom.itemNames')]
        bnames = names_list(port.base, file_off(bank, addr), 255)
        items, got = {}, None
        for build, (rom, values) in roms.items():
            hnames = names_list(rom, file_off(*values['rom.itemNames'][0]), 255)
            this = {}
            for name, (i, gen4) in sg['items'].items():
                hits = [k + 1 for k, x in enumerate(hnames) if x == bnames[i - 1]]
                if len(hits) != 1:
                    fail('%s: %s\'s %s is named %d times in %s\'s ItemNames' % (port.name, src, name, len(hits), build))
                this[name] = [hits[0], gen4]
            if got is not None and this != got:
                fail('%s: its builds disagree on the items\' ids' % port.name)
            got = this
        items = got
        out['items'] = items
        out['balls'] = sorted(items[k][0] for k, (i, _) in sg['items'].items() if i in sg['balls'])
    return out


def port_game(name, g, pret, roms_by_sha1, extra_games):
    repo, bld = g['base']
    sym = pe.parse_sym(pret.read(repo, 'symbols:%s.sym' % bld))
    bsha = None
    for line in pret.read(repo, 'roms.sha1').splitlines():
        h, _, f = line.partition(' ')
        if f.lstrip('*') == bld + '.gbc':
            bsha = h.lower()
    if not bsha or bsha not in roms_by_sha1:
        fail('the base %s.gbc (sha1 %s) isn\'t in the ROMs\' folder' % (bld, bsha))
    base_game = [k for k, x in extra_games.items() if any(r.get('sha1') == bsha for r in x['releases'])]
    if len(base_game) != 1:
        fail('%s: %s\'s release %s isn\'t one game\'s in %s' % (name, repo, bld, EXTRA))
    port = Port(name, g, sym, roms_by_sha1[bsha][1], extra_games[base_game[0]])
    results, problems, titles = {}, [], set()
    for b in g['builds']:
        if b['sha1'] not in roms_by_sha1:
            fail('%s %s (sha1 %s) isn\'t in the ROMs\' folder' % (name, b['name'], b['sha1']))
        rom = roms_by_sha1[b['sha1']][1]
        values, bad = port.build(rom)
        problems += ['%s: %s' % (b['name'], x) for x in bad]
        results[b['name']] = (rom, values)
        titles.add(pe.rom_header(rom))
        b['code'] = pe.rom_header(rom)[1]
    if problems:
        fail('%s: %d values not found:\n  %s' % (name, len(problems), '\n  '.join(problems)))
    if len(titles) != 1:
        fail('%s\'s builds have two header titles: %s' % (name, titles))
    # a version's builds agree; the versions' differences are each release's own
    versions = {}
    for b in g['builds']:
        versions.setdefault(b['name'].split()[0], []).append(b['name'])
    out = {'header_title': titles.pop()[0], 'releases': [], 'ram': {}, 'rom': {}, 'const': {}}
    report = []
    for key in port.keys():
        kind, k = key.split('.', 1)
        per = {}
        for ver, names in versions.items():
            got = {results[x][1][key][0] for x in names}
            if len(got) != 1:
                fail('%s %s: the %s builds disagree: %s' % (name, key, ver, ', '.join(
                    '%s %s' % (x, show(results[x][1][key][0])) for x in names)))
            per[ver] = got.pop()
        how = sorted({results[x][1][key][1] for x in results}, key=lambda s: (len(s), s))[0]
        s = port.symbol(key)

        def entry(v):
            e = {'value': v} if kind == 'const' else {'bank': v[0], 'addr': pe.hexaddr(v[1])}
            if s:
                e['symbol'] = s
            e['how'] = how
            return e
        report.append('%-26s %-36s %s  (%s)' % (key, s or '-', ' | '.join('%s %s' % (ver, show(v)) for ver, v in per.items())
                      if len(set(per.values())) > 1 else show(next(iter(per.values()))), how))
        if len(set(per.values())) == 1:
            out[kind][k] = entry(next(iter(per.values())))
        else:
            for b in g['builds']:
                b.setdefault(kind, {})[k] = entry(per[b['name'].split()[0]])
    for b in g['builds']:
        rel = {'name': b['name'], 'sha1': b['sha1']}
        if b.get('code'):
            rel['code'] = b['code']
        for kind in ('ram', 'rom', 'const'):
            if kind in b:
                rel[kind] = b[kind]
        out['releases'].append(rel)
    out.update(items_types(port, {k: (r, v) for k, (r, v) in results.items()}, extra_games))
    out['files'] = {k: 'from %s' % g['from'][k][0] for k in ('items', 'types')}
    return out, report, bsha, base_game[0]


# --- The ROMs ---

def find_roms(folder):
    """The .gb and .gbc files in the folder and its subfolders one level down: sha1 -> (path, bytes)."""
    out = {}
    if not os.path.isdir(folder):
        fail('no folder %s' % folder)
    paths = []
    for e in sorted(os.listdir(folder)):
        p = os.path.join(folder, e)
        if os.path.isdir(p):
            paths += [os.path.join(p, x) for x in sorted(os.listdir(p))]
        else:
            paths.append(p)
    for p in paths:
        if os.path.isfile(p) and p.lower().endswith(('.gb', '.gbc')):
            d = open(p, 'rb').read()
            out.setdefault(hashlib.sha1(d).hexdigest(), (p, d))
    return out


# --- The self-test ---

def self_test():
    """The methods on made-up ROMs: a use of $D000 found in a build that moved it by 0x40 with the bytes around changed;
    the same with the use rewritten fails with the symbol's name; a pattern and a table found."""
    rnd = random.Random(30)

    def filler(n):
        return bytes(rnd.choice([0x00, 0x23, 0x77, 0x47, 0x4F, 0x05, 0x0D, 0x3C]) for _ in range(n))
    use = bytes.fromhex('f5 c5 cd 00 31 3e 07 fa 00 d0 fe 03 20 04 af e0 9f c9')     # ld a,[$D000] among code
    sites_code = filler(0x100) + use + filler(0x80) + use[:6] + bytes.fromhex('ea 00 d0 a7 28 02 3c c9 01 02') + filler(0x60)
    base = bytearray(0x8000)
    base[0x200:0x200 + len(sites_code)] = sites_code
    moved = sites_code.replace(b'\x00\xd0', b'\x40\xd0').replace(b'\xcd\x00\x31', b'\xcd\x80\x32')
    hack = bytearray(0x8000)
    hack[0x240:0x240 + len(moved)] = moved
    hack[0x100:0x140] = filler(0x40)
    sites = vote_sites(bytes(base), 0, 0xD000)
    v, how = decide(votes(bytes(base), bytes(hack), sites, 0, 0xD000), 'wMadeUp')
    assert v == (1, 0xD040) and how == 'votes 2', (v, how)
    rewritten = bytes(hack).replace(bytes.fromhex('fa 40 d0'), bytes.fromhex('21 40 d0')).replace(
        bytes.fromhex('ea 40 d0'), bytes.fromhex('21 40 d0'))
    try:
        decide(votes(bytes(base), rewritten, sites, 0, 0xD000), 'wMadeUp')
        raise AssertionError('a rewritten use was ported')
    except PortError as e:
        assert 'wMadeUp' in str(e), e
    pats = compile_pattern('3e 07 fa {addr} fe {byte}'.split(), 0)
    caps = find_code(pats, bytes(rewritten).replace(bytes.fromhex('21 40 d0 fe'), bytes.fromhex('fa 40 d0 fe'), 1), False, 'x')
    assert caps and code_value('ram', caps, 'x') == (1, 0xD040) and caps['byte'] == 3, caps
    try:
        find_code(compile_pattern('c5 cd .. .. 3e {byte}'.split(), 0), bytes(hack), False, 'x')
        raise AssertionError('a pattern found twice passed')
    except PortError as e:
        assert 'found 2 times' in str(e), e
    # (a table of 4-byte entries whose pointers point just past it: three)
    t = bytearray(0x8000)
    for i, p in enumerate((0x400C, 0x4010, 0x4012)):
        t[0x4000 + 4 * i + 2:0x4000 + 4 * i + 4] = bytes([p & 0xFF, p >> 8])
    assert table_count(bytes(t), 0x4000, 4, 2) == 3
    # (a fixed value: its code's pattern once, else none)
    fixed = Port('made-up', {'methods': {'const.groupSize': ('port.fixed', '6 21 .. .. 19 19 19 19 19 19 cd'.split(), 0)},
                             'asks': {}}, {}, bytes(base), {})
    six = bytes(hack[:0x300]) + bytes.fromhex('21 25 7d 19 19 19 19 19 19 cd') + bytes(hack[0x30A:])
    assert fixed.one('const.groupSize', six, {}) == (6, 'fixed by its code')
    try:
        fixed.one('const.groupSize', bytes(hack), {})
        raise AssertionError('a fixed value without its code passed')
    except PortError as e:
        assert 'const.groupSize' in str(e), e
    # (a trainer class by its name and its first trainer's: the base's class 2, the build's 3, its classes one fewer
    # before it and a bank and a pointer each; its class 1, of another name, has the same first trainer; a class that
    # two of the build's classes are isn't one)
    tb = bytearray(0x8000 * 2)
    tb[0x200:0x20B] = b'X\x50LEADER\x50Y\x50'
    for c, nm in enumerate((b'ANNA\x50', b'BOB\x50', b'CY\x50')):
        tb[0x100 + 2 * c:0x102 + 2 * c] = bytes([0x00, 0x60 + c])
        tb[0x2000 + 0x100 * c:0x2000 + 0x100 * c + len(nm)] = nm
    th = bytearray(0x8000 * 2)
    th[0x4800:0x4812] = b'BOSS\x50Z\x50LEADER\x50ANY\x50'
    for c, nm in enumerate((b'BOB\x50', b'CY\x50', b'BOB\x50', b'ANNA\x50')):
        th[0x4000 + 3 * c:0x4003 + 3 * c] = bytes([1, 0x00, 0x50 + c])
        th[0x5000 + 0x100 * c:0x5000 + 0x100 * c + len(nm)] = nm
    tp = Port('made-up', {'methods': {'const.bob': ('port.trainer', ['rom.groups', 'Groups'], 0)}, 'asks': {}},
              {'Groups': (0, 0x100), 'Classes': (0, 0x200)}, bytes(tb), {'const': {'bob': {'value': 2}}})
    tp.symbol = lambda key: 'Classes'
    vals = {'rom.groups': ((1, 0x4000), ''), 'const.numTrainerClasses': (4, ''),
            'rom.trainerClassNames': ((1, 0x4800), '')}
    assert tp.one('const.bob', bytes(th), vals) == (3, 'by its class\'s and first trainer\'s names')
    th[0x4800:0x4812] = b'LEADER\x50Z\x50LEADER\x50A\x50'
    try:
        tp.one('const.bob', bytes(th), vals)
        raise AssertionError('a class two of the build\'s are passed')
    except PortError as e:
        assert 'const.bob' in str(e) and '2 classes' in str(e), e
    print('gbpk-port: self-test passed')


# --- main ---

def main(argv):
    if argv == ['--self-test']:
        self_test()
        return
    check = '--check' in argv
    args = [a for a in argv if a != '--check']
    if len(args) not in (0, 2) or any(a.startswith('-') for a in args):
        sys.exit('usage: gbpk-port.py <pret folder> <roms folder> [--check] | --check | --self-test')
    listing = open(os.path.join(REPO, LIST), encoding='utf-8').read()
    ported = read_port_list(listing)
    if not args:
        if not check:
            fail('give the pret folder and the ROMs\' folder, or --check')
        n = pe.verify_sources(SECTION, [LIST, EXTRA, OUT])
        out = json.load(open(os.path.join(REPO, OUT), encoding='utf-8'))
        values = 0
        for name, g in ported.items():
            og = out['games'].get(name)
            if og is None:
                fail('no %s in %s' % (name, OUT))
            if sorted(r['sha1'] for r in og['releases']) != sorted(b['sha1'] for b in g['builds']):
                fail('%s\'s releases in %s aren\'t its port lines\'' % (name, OUT))
            for key in set(g['asks']) | set(g['methods']):
                kind, k = key.split('.', 1)
                if k not in og[kind] and not all(k in r.get(kind, {}) for r in og['releases']):
                    fail('%s %s: no answer in %s' % (name, key, OUT))
                values += 1
            for k in ('items', 'balls', 'types', 'special'):
                if k not in og:
                    fail('%s: no %s in %s' % (name, k, OUT))
        print('gbpk-port: %d files match SOURCES; %d values of %s answered in %s' % (n, values, LIST, OUT))
        return
    pret = pe.Pret(os.path.abspath(args[0]))
    roms = find_roms(os.path.abspath(args[1]))
    extra = json.load(open(os.path.join(REPO, EXTRA), encoding='utf-8'))
    data = {'schema': 1, 'kind': 'gb-pokemon-port', 'games': {}, 'base': {}}
    body, reports = [], []
    for name in sorted(ported):
        game, report, bsha, bgame = port_game(name, ported[name], pret, roms, extra['games'])
        data['games'][name] = game
        repo, bld = ported[name]['base']
        data['base'][name] = {'game': bgame, 'repo': repo, 'build': bld, 'sha1': bsha}
        reports += ['%s, from %s %s:' % (name, repo, bld)] + ['  ' + r for r in report]
        body += ['rom %s  %s %s.gbc' % (bsha, repo, bld)]
        body += ['rom %s  %s %s' % (b['sha1'], name, b['name']) for b in ported[name]['builds']]
    out = pe.dump(data)
    src_path = os.path.join(REPO, SOURCES)
    old_sources = open(src_path, encoding='utf-8').read()
    body = ['commit %s %s symbols %s' % (r, v['commit'], v['symbols']) for r, v in sorted(pret.repos.items())] + body
    body += ['in  %s  %s' % (h, w) for h, w in sorted(pret.inputs, key=lambda x: x[1])]
    body += ['in  %s  %s' % (pe.sha256(listing.encode('utf-8')), LIST)]
    body += ['in  %s  %s' % (pe.sha256(open(os.path.join(REPO, EXTRA), 'rb').read()), EXTRA)]
    body += ['out %s  %s' % (pe.sha256(out.encode('utf-8')), OUT)]
    sources = pe.write_sources(old_sources, SECTION, body)
    if check:
        bad = []
        cur_path = os.path.join(REPO, OUT)
        if not os.path.exists(cur_path) or open(cur_path, encoding='utf-8').read() != out:
            bad.append(OUT)
        if pe.read_sources(old_sources).get(SECTION) != pe.read_sources(sources)[SECTION]:
            bad.append('%s [%s]' % (SOURCES, SECTION))
        if bad:
            with tempfile.NamedTemporaryFile('w', suffix='.json', delete=False, encoding='utf-8') as t:
                t.write(out)
            fail('the ROMs give another %s (this run\'s %s is in %s)' % (' and '.join(bad), OUT, t.name))
        print('gbpk-port: %s and SOURCES\' [port] are the ROMs\' (%s)' % (OUT, ', '.join(sorted(ported))))
        return
    print('\n'.join(reports))
    open(os.path.join(REPO, OUT), 'w', encoding='utf-8').write(out)
    open(src_path, 'w', encoding='utf-8').write(sources)
    print('gbpk-port: wrote %s (%s) and SOURCES\' [port]' % (OUT, ', '.join(sorted(ported))))


if __name__ == '__main__':
    main(sys.argv[1:])
