#!/usr/bin/env python3
# The Game Boy Pokémon companion's facts from pret's disassemblies (pokecrystal, pokegold, pokered, pokeyellow) and
# the ROM hacks' forks of them: the addresses, ROM offsets and constants kiwami/tools/gbpk-symbols.txt asks for, looked
# up in each release's symbol file, each game's items, key items, types and physical/special split from its constants,
# and the releases' SHA-1s and ROM headers, written to kiwami/tools/gbpk-extra.json. Only facts about the ROMs are
# taken, none of the disassemblies' code or tables (kiwami/GB-POKEMON.md).
#
#   pret-extract.py <pret folder>            write gbpk-extra.json and SOURCES' [pret] section
#   pret-extract.py <pret folder> --check    write them to a temporary folder and compare with what's committed
#   pret-extract.py --check                  without the checkouts: the committed files against SOURCES' hashes,
#                                            and every line of gbpk-symbols.txt answered in gbpk-extra.json
#
# <pret folder> holds a clone of each repository named in the list. pret's have their symbols branch (git fetch origin
# symbols:symbols): the .sym files are read from that branch with git show, the constants, Makefile and roms.sha1
# from the checkout's HEAD. A ROM hack's (a build line) is checked out at its release's tag and built there by the
# user: its ROM and .sym are the working tree's, its constants its HEAD's. Python 3, standard library only.
import hashlib
import json
import os
import re
import subprocess
import sys
import tempfile
from typing import NoReturn

REPO = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..'))
LIST = 'kiwami/tools/gbpk-symbols.txt'
OUT = 'kiwami/tools/gbpk-extra.json'
SOURCES = 'kiwami/rootfs/usr/share/banana/companions/gb-pokemon/SOURCES'
PATCHES = 'kiwami/tests/gbpk/roms.patches'   # the ROM hacks read as the game they patch (a hack line's check)
SECTION = 'pret'
PROG = 'pret-extract'       # (pokedaisy-extract.py sets its own name when it uses these helpers)


def fail(msg) -> NoReturn:
    sys.exit(PROG + ': ' + msg)


def sha256(data):
    return hashlib.sha256(data).hexdigest()


# --- The list ---

# the fork's names of the types (the data files' "types": nds::Type's order, then the fork's own past it), by the type
# constant's name in a game's constants/type_constants.asm; BIRD and CURSE_TYPE have no type of their own
TYPE_NAMES = {'NORMAL': 'normal', 'FIGHTING': 'fighting', 'FLYING': 'flying', 'POISON': 'poison', 'GROUND': 'ground',
              'ROCK': 'rock', 'BUG': 'bug', 'GHOST': 'ghost', 'STEEL': 'steel', 'FIRE': 'fire', 'WATER': 'water',
              'GRASS': 'grass', 'ELECTRIC': 'electric', 'PSYCHIC_TYPE': 'psychic', 'ICE': 'ice', 'DRAGON': 'dragon',
              'DARK': 'dark', 'TYPELESS': 'typeless', 'CRYSTAL': 'crystal', 'BONEMERANG_TYPE': 'bonemerang',
              'TRI': 'tri', 'FLOATING': 'floating', 'MAGMA': 'magma'}
NOT_TYPES = ('BIRD', 'CURSE_TYPE')
SPLITS = ('physical', 'special', 'stats')


def ported_games(text):
    """The games gbpk-port.py finds in their builds (a port line each): their lines are its, not this script's."""
    return {l.split()[0] for l in (x.split('#', 1)[0] for x in text.splitlines())
            if len(l.split()) > 1 and (l.split()[1] == 'port' or l.split()[1].startswith('port.'))}


def read_list(text):
    """The lines of gbpk-symbols.txt: (line number, games, kind, key, args), the ported games left out."""
    out = []
    ported = ported_games(text)
    for n, line in enumerate(text.splitlines(), 1):
        line = line.split('#', 1)[0].strip()
        if not line:
            continue
        parts = line.split(None, 2)
        if len(parts) < 3:
            fail('%s:%d: too short: %s' % (LIST, n, line))
        if parts[0] != 'item' and ported:
            games = [g for g in parts[0].split(',') if g not in ported]
            if not games:
                continue
            parts[0] = ','.join(games)
            line = ' '.join(parts)
        if parts[0] == 'item':
            f = line.split()
            if len(f) not in (3, 4) or not f[2].isdigit() or not int(f[2]) <= 65535 or f[3:] not in ([], ['ball']) or \
                    (int(f[2]) == 0 and f[3:] != ['ball']):
                fail('%s:%d: an item is item <NAME> <Gen IV id> [ball] (0: a ball Gen IV hasn\'t)' % (LIST, n))
            out.append((n, None, 'item', f[1], [int(f[2]), f[3:] == ['ball']]))
            continue
        games = parts[0].split(',')
        if parts[1] in ('items', 'keyitems', 'types'):
            a = parts[2].split()
            if len(a) != 1:
                fail('%s:%d: %s takes <file>' % (LIST, n, parts[1]))
            out.append((n, games, parts[1], None, a))
            continue
        if parts[1] == 'split':
            a = parts[2].split()
            if len(a) != 2 or a[0] not in TYPE_NAMES or a[1] not in SPLITS:
                fail('%s:%d: a split is <games> split <TYPE> %s' % (LIST, n, '|'.join(SPLITS)))
            out.append((n, games, 'split', None, a))
            continue
        if parts[1] == 'release':
            a = parts[2].split(None, 2)
            if len(a) != 3:
                fail('%s:%d: a release is <game> release <repo> <build> <name>' % (LIST, n))
            out.append((n, games, 'release', None, a))
            continue
        if parts[1] == 'build':
            a = parts[2].split(None, 3)
            if len(a) != 4 or not re.fullmatch(r'[0-9a-f]{40}', a[2]):
                fail('%s:%d: a build is <game> build <folder> <build> <sha1> <name>' % (LIST, n))
            out.append((n, games, 'build', None, a))
            continue
        if parts[1] == 'hack':
            a = parts[2].split(None, 1)
            if len(a) != 2 or not re.fullmatch(r'[0-9a-f]{40}', a[0]):
                fail('%s:%d: a hack is <game> hack <sha1> <name>' % (LIST, n))
            out.append((n, games, 'hack', None, a))
            continue
        kind, dot, key = parts[1].partition('.')
        if kind not in ('ram', 'rom', 'const') or not dot or not key:
            fail('%s:%d: unknown kind %s' % (LIST, n, parts[1]))
        a = parts[2].split()
        if (kind == 'const') != (len(a) == 2) or len(a) not in (1, 2):
            fail('%s:%d: %s takes %s' % (LIST, n, kind, '<file> <name>' if kind == 'const' else '<symbol>'))
        out.append((n, games, kind, key, a))
    return out


# --- rgbds' symbol files and constants ---

def parse_sym(text):
    """A .sym file's symbols: name -> (bank, address)."""
    syms = {}
    for line in text.splitlines():
        m = re.match(r'([0-9a-fA-F]+):([0-9a-fA-F]+) (\S+)$', line.strip())
        if m:
            syms.setdefault(m.group(3), (int(m.group(1), 16), int(m.group(2), 16)))
    return syms


class Consts:
    """Enough of rgbds' assembler to evaluate a constants file: DEF and REDEF (EQU, =, +=, -=; before rgbds 0.6, which
    the older ROM hacks build with, NAME EQU value and NAME = value, SET as =, without DEF), a name with a number
    in it ({02d:NAME}), the const macros of macros/const.asm (const_def, const, const_skip, const_next, shift_const), a structure's offsets
    (rsreset, rsset, NAME rb/rw/rl [count], _RS)
    and the file's own MACROs (MACRO name, or name: MACRO before rgbds 0.6). An ASSERT, which defines nothing, is passed over,
    and a string's EQUS is set aside as below. Anything else stops the run, so a
    value is never computed from a line that wasn't understood. A DEF whose expression names a symbol the file
    doesn't define (one from hardware.inc, which it doesn't read, or a move's id in an item's alias) is set aside:
    asking for that name stops the run with the reason, any other line using it too. So is a DEF whose name is made
    with such a symbol (toggle_constants.asm's TOGGLEMAP{\\1}_ID, by a map's id): no other line can name it."""

    ASSIGN = re.compile(r'(?:(?:RE)?DEF\s+)?(\w+)\s*(EQU\b|SET\b|=|\+=|-=)\s*(.+)$')
    TOKEN = re.compile(r'\s*(?:(\$[0-9A-Fa-f]+)|(%[01]+)|(\d+)|([A-Za-z_][A-Za-z0-9_@#.]*)|(<<|>>|[-+*/%&|^~()]))')

    def __init__(self, name):
        self.name = name
        self.consts = {}   # the names the const macro gave, in order: an enumeration's members (not a DEF's alias)
        self.v = {'const_value': 0, 'const_inc': 1, '_RS': 0}
        self.macros = {}
        self.unknown = {}  # a DEF set aside: name -> why

    class Undefined(Exception):
        pass

    def expr(self, s, where):
        out, pos = [], 0
        s = s.strip()
        while pos < len(s):
            m = self.TOKEN.match(s, pos)
            if not m or m.end() == pos:
                fail('%s: an expression it can\'t read: %s' % (where, s))
            pos = m.end()
            hx, bn, dec, ident, op = m.groups()
            if hx:
                out.append(str(int(hx[1:], 16)))
            elif bn:
                out.append(str(int(bn[1:], 2)))
            elif dec:
                out.append(str(int(dec)))     # (05 is 5 to rgbds; Python refuses the leading 0)
            elif ident:
                if ident not in self.v:
                    raise Consts.Undefined('%s: %s isn\'t defined' % (where, self.unknown.get(ident, ident)))
                out.append('(%d)' % self.v[ident])
            else:
                out.append('//' if op == '/' else op)
        return int(eval(' '.join(out), {'__builtins__': {}}, {}))

    def run(self, text, args=None, depth=0):
        if depth > 8:
            fail('%s: macros nested too deep' % self.name)
        lines = text.splitlines()
        i = 0
        while i < len(lines):
            n, line = i + 1, lines[i]
            i += 1
            line = line.split(';', 1)[0].strip()
            if args is not None:
                for k in range(9, 0, -1):
                    line = line.replace('\\%d' % k, args[k - 1] if k <= len(args) else '')
            if not line:
                continue
            where = '%s:%d' % (self.name, n) if depth == 0 else '%s (macro)' % self.name
            m = re.match(r'MACRO\??\s+(\w+)$', line) or re.match(r'(\w+):\s*MACRO$', line)
            if m:
                body = []
                while i < len(lines) and lines[i].split(';', 1)[0].strip() != 'ENDM':
                    body.append(lines[i])
                    i += 1
                if i == len(lines):
                    fail('%s: MACRO %s has no ENDM' % (where, m.group(1)))
                i += 1
                self.macros[m.group(1)] = '\n'.join(body)
                continue
            if re.match(r'ASSERT\b', line):
                continue
            # (a string equate defines no number: set aside, as a DEF it can't evaluate)
            m = re.match(r'(?:(?:RE)?DEF\s+)?(\w+)\s+EQUS\b', line)
            if m:
                self.v.pop(m.group(1), None)
                self.unknown[m.group(1)] = '%s (a string)' % m.group(1)
                continue
            # (a symbol's name made with a number: {02d:HM_VALUE} is HM_VALUE's, two digits)
            try:
                line = re.sub(r'\{(?:0?(\d*)d:)?(\w+)\}', lambda x: '%0*d' % (int(x.group(1) or 0), self.expr(x.group(2), where)), line)
            except Consts.Undefined:
                if re.match(r'(?:RE)?DEF\s', line) or self.ASSIGN.match(line):
                    continue
                raise
            m = self.ASSIGN.match(line)
            if m and m.group(1) in ('const', 'shift_const', 'const_def', 'const_skip', 'const_next'):
                m = None
            if m and m.group(2) in ('EQU', 'SET', '='):
                try:
                    self.expr(m.group(3), where)
                except Consts.Undefined as e:
                    self.v.pop(m.group(1), None)
                    self.unknown[m.group(1)] = '%s (%s)' % (m.group(1), e)
                    continue
            if m:
                name, op, val = m.group(1), m.group(2), self.expr(m.group(3), where)
                if op in ('EQU', 'SET', '='):
                    self.v[name] = val
                    self.unknown.pop(name, None)
                elif name not in self.v:
                    fail('%s: %s %s before it was defined' % (where, name, op))
                else:
                    self.v[name] += val if op == '+=' else -val
                continue
            m = re.match(r'(?:DEF\s+)?(\w+):?\s+(RB|RW|RL)\b\s*(.*)$', line, re.I)
            if m:
                self.v[m.group(1)] = self.v['_RS']
                self.v['_RS'] += {'rb': 1, 'rw': 2, 'rl': 4}[m.group(2).lower()] * (self.expr(m.group(3), where) if m.group(3) else 1)
                continue
            word, _, rest = line.partition(' ')
            a = [x.strip() for x in rest.split(',')] if rest.strip() else []
            if word.lower() == 'rsreset' and not a:
                self.v['_RS'] = 0
                continue
            if word.lower() == 'rsset' and len(a) == 1:
                self.v['_RS'] = self.expr(a[0], where)
                continue
            if word == 'const_def':
                self.v['const_value'] = self.expr(a[0], where) if a else 0
                self.v['const_inc'] = self.expr(a[1], where) if len(a) > 1 else 1
            elif word == 'const' and len(a) == 1:
                self.v[a[0]] = self.consts[a[0]] = self.v['const_value']
                self.v['const_value'] += self.v['const_inc']
            elif word == 'shift_const' and len(a) == 1:
                self.v[a[0]] = 1 << self.v['const_value']
                self.v[a[0] + '_F'] = self.v['const_value']
                self.v['const_value'] += self.v['const_inc']
            elif word == 'const_skip':
                self.v['const_value'] += self.v['const_inc'] * (self.expr(a[0], where) if a else 1)
            elif word == 'const_next' and len(a) == 1:
                nxt = self.expr(a[0], where)
                if self.v['const_value'] > 0 and nxt < self.v['const_value']:
                    fail('%s: const_next goes backwards' % where)
                self.v['const_value'] = nxt
            elif word in self.macros:
                self.run(self.macros[word], a, depth + 1)
            else:
                fail('%s: a line it can\'t read: %s' % (where, line))


def rgbfix_flags(makefile, build):
    """The rgbfix flags a build gets: the Makefile's 'RGBFIXFLAGS +=' lines for every build, then <build>.gbc's."""
    flags = []
    for line in makefile.splitlines():
        m = re.match(r'(?:(\S+)\.gbc:\s*)?RGBFIXFLAGS\s*\+=\s*(.*)$', line.strip())
        if m and m.group(1) in (None, build):
            flags += re.findall(r'"[^"]*"|\S+', m.group(2))
    out = {}
    for f, v in zip(flags, flags[1:]):
        if f in ('-t', '-i'):
            out[f] = v.strip('"')
    return out


def rom_header(rom):
    """A ROM's header title and game code, as rgbfix wrote them: the title at 0x134 up to its first 0, 15 bytes at most
    on a Game Boy Color cart (0x143 its flag), 16 on another; the code at 0x13F when the title ends before it and the
    four bytes are capitals and digits (None without one)."""
    if len(rom) < 0x150:
        fail('a ROM of %d bytes has no header' % len(rom))
    color = rom[0x143] & 0x80
    t = rom[0x134:0x143 if color else 0x144]
    title = t.split(b'\0', 1)[0].decode('ascii', 'replace')
    code = rom[0x13F:0x143]
    if color and len(title) <= 11 and re.fullmatch(rb'[A-Z0-9]{4}', code):
        return title, code.decode('ascii')
    return title, None


# --- Reading the checkouts ---

def git(repo, *args):
    r = subprocess.run(['git', '-C', repo] + list(args), stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    if r.returncode:
        fail('git %s in %s: %s' % (' '.join(args), repo, r.stderr.decode().strip()))
    return r.stdout


class Pret:
    def __init__(self, folder):
        self.folder = folder
        self.inputs = []                # (sha256, what) of every file read
        self.repos = {}
        self.cache = {}
        self.built = set()              # the ROM hacks' checkouts (build lines): a tag, no symbols branch

    def repo(self, name):
        if name not in self.repos:
            path = os.path.join(self.folder, name)
            if not os.path.isdir(path):
                fail('no checkout of %s in %s' % (name, self.folder))
            head = git(path, 'rev-parse', 'HEAD').decode().strip()
            if name in self.built:
                r = subprocess.run(['git', '-C', path, 'describe', '--tags', '--exact-match', 'HEAD'],
                                   stdout=subprocess.PIPE, stderr=subprocess.PIPE)
                if r.returncode:
                    fail('%s\'s HEAD %s isn\'t a tag: check out the release\'s' % (path, head[:8]))
                self.repos[name] = {'commit': head, 'tag': r.stdout.decode().strip()}
            else:
                sym = git(path, 'rev-parse', 'symbols').decode().strip()
                self.repos[name] = {'commit': head, 'symbols': sym}
        return os.path.join(self.folder, name)

    def built_file(self, repo, what, record=True):
        """A file the user's build wrote in a ROM hack's working tree (its ROM, its .sym)."""
        self.built.add(repo)
        path = os.path.join(self.repo(repo), what)
        if not os.path.isfile(path):
            fail('no %s: build the release in %s' % (path, os.path.join(self.folder, repo)))
        data = open(path, 'rb').read()
        if record:
            self.inputs.append((sha256(data), '%s %s' % (repo, what)))
        return data

    def read(self, repo, what):
        """A file of the checkout's HEAD, or 'symbols:<file>' from the symbols branch."""
        k = (repo, what)
        if k not in self.cache:
            path = self.repo(repo)
            if what.startswith('symbols:'):
                data = git(path, 'show', what)
            else:
                data = git(path, 'show', 'HEAD:' + what)
            self.cache[k] = data
            self.inputs.append((sha256(data), '%s %s' % (repo, what)))
        return self.cache[k].decode('utf-8')


def hexaddr(a):
    return '0x%04X' % a


def read_patches():
    """roms.patches' hacks: the hack's SHA-1 -> its retail ROM's."""
    out = {}
    for line in open(os.path.join(REPO, PATCHES), encoding='utf-8').read().splitlines():
        f = line.split('#', 1)[0].split()
        if len(f) >= 2:
            out[f[0]] = f[1]
    return out


def build(pret, lines):
    games = {}
    # the releases first: they say which repository and symbol files a game's values come from. A ROM hack's builds
    # may be in several folders (one checkout a version, each at its tag): its constants must agree in all of them,
    # and an address its versions disagree on is each release's own (below)
    for n, gs, kind, key, a in lines:
        if kind not in ('release', 'build'):
            continue
        if len(gs) != 1:
            fail('%s:%d: a release names one game' % (LIST, n))
        repo, bld, name = a[0], a[1], a[-1]
        g = games.setdefault(gs[0], {'repo': repo, 'releases': [], 'ram': {}, 'rom': {}, 'const': {}})
        if g.setdefault('kind', kind) != kind:
            fail('%s:%d: %s has release and build lines' % (LIST, n, gs[0]))
        if g['repo'] != repo and kind != 'build':
            fail('%s:%d: %s\'s releases are in two repositories' % (LIST, n, gs[0]))
        if repo not in g.setdefault('folders', []):
            g['folders'].append(repo)
        if kind == 'build':
            # a ROM hack built by the user at its release's tag: the build must be the release (its SHA-1 the line's;
            # the hack's own roms.sha1 is pret's, left over), its header read from its bytes
            if repo in pret.repos and repo not in pret.built:
                fail('%s:%d: %s is pret\'s and a hack\'s' % (LIST, n, repo))
            rom = pret.built_file(repo, bld + '.gbc', record=False)
            got = hashlib.sha1(rom).hexdigest()
            if got != a[2]:
                fail('%s/%s.gbc isn\'t %s\'s release: sha1 %s, not %s' % (repo, bld, name, got, a[2]))
            title, code = rom_header(rom)
            if g.setdefault('header_title', title) != title:
                fail('%s:%d: %s\'s releases have two header titles' % (LIST, n, gs[0]))
            rel = {'build': bld, 'name': name, 'sha1': a[2]}
            if code:
                rel['code'] = code
            g['releases'].append(rel)
            g.setdefault('syms', []).append(parse_sym(pret.built_file(repo, bld + '.sym').decode('utf-8')))
            continue
        if repo in pret.built:
            fail('%s:%d: %s is pret\'s and a hack\'s' % (LIST, n, repo))
        sha1 = None
        for line in pret.read(repo, 'roms.sha1').splitlines():
            h, _, f = line.partition(' ')
            if f.lstrip('*') == bld + '.gbc':
                sha1 = h.lower()
        if not sha1:
            fail('%s:%d: %s.gbc isn\'t in %s\'s roms.sha1' % (LIST, n, bld, repo))
        fix = rgbfix_flags(pret.read(repo, 'Makefile'), bld)
        if '-t' not in fix:
            fail('%s:%d: no rgbfix title for %s.gbc in %s\'s Makefile' % (LIST, n, bld, repo))
        if g.setdefault('header_title', fix['-t']) != fix['-t']:
            fail('%s:%d: %s\'s releases have two header titles' % (LIST, n, gs[0]))
        rel = {'build': bld, 'name': name, 'sha1': sha1}
        if '-i' in fix:
            rel['code'] = fix['-i']
        g['releases'].append(rel)
        g.setdefault('syms', []).append(parse_sym(pret.read(repo, 'symbols:%s.sym' % bld)))
    # the ROM hacks read as the game they patch: roms.patches must list each as a patch of one of the game's releases
    # (kiwami/tests/gbpk-roms.sh checks the patch moves nothing the companion reads); no symbols of their own
    patches = read_patches()
    for n, gs, kind, key, a in lines:
        if kind != 'hack':
            continue
        if len(gs) != 1 or gs[0] not in games:
            fail('%s:%d: a hack names one game with a release line' % (LIST, n))
        g = games[gs[0]]
        if patches.get(a[0]) not in {r['sha1'] for r in g['releases']}:
            fail('%s:%d: %s isn\'t in %s as a patch of a %s release' % (LIST, n, a[0], PATCHES, gs[0]))
        g['releases'].append({'hack': True, 'name': a[1], 'sha1': a[0]})
    consts = {}

    def constants(g, f, repo=None):
        # (f may be several files joined by +, read in that order as the game INCLUDEs them: a file that names
        # another's constants, as PureRGB's pokemon_data_constants.asm does battle_constants.asm's NUM_STATS)
        ck = (repo or g['repo'], f)
        if ck not in consts:
            c = Consts('%s/%s' % ck)
            try:
                for part in f.split('+'):
                    c.name = '%s/%s' % (ck[0], part)
                    c.run(pret.read(ck[0], part))
            except Consts.Undefined as e:
                fail(str(e))
            consts[ck] = c
        return consts[ck]

    for n, gs, kind, key, a in lines:
        if kind in ('release', 'build', 'hack', 'item', 'items', 'keyitems', 'types', 'split'):
            continue
        for game in gs:
            if game not in games:
                fail('%s:%d: %s has no release line' % (LIST, n, game))
            g = games[game]
            if key in g[kind]:
                fail('%s:%d: %s %s.%s given twice' % (LIST, n, game, kind, key))
            if kind == 'const':
                f, name = a
                values = set()
                for repo in g['folders']:
                    c = constants(g, f, repo)
                    if name in c.unknown:
                        fail('%s:%d: %s/%s: %s' % (LIST, n, repo, f, c.unknown[name]))
                    if name not in c.v:
                        fail('%s:%d: %s isn\'t in %s/%s' % (LIST, n, name, repo, f))
                    values.add(c.v[name])
                if len(values) != 1:
                    fail('%s:%d: %s\'s versions disagree on %s: %s' % (LIST, n, game, name, ', '.join(map(str, sorted(values)))))
                g['const'][key] = {'value': values.pop(), 'symbol': name, 'file': f}
                continue
            found = {}
            for rel, syms in zip([r for r in g['releases'] if not r.get('hack')], g['syms']):
                if a[0] not in syms:
                    fail('%s:%d: %s isn\'t in %s.sym' % (LIST, n, a[0], rel['build']))
                found.setdefault(syms[a[0]], []).append(rel)
            # (a ROM hack's versions, each rebuilt, may each have the table elsewhere: each release's own value then;
            # pret's releases must agree)
            if len(found) != 1 and g['kind'] != 'build':
                fail('%s:%d: %s\'s releases disagree on %s: %s' % (LIST, n, game, a[0],
                     ', '.join('%02X:%04X' % b for b in sorted(found))))
            for (bank, addr), rels in found.items():
                if kind == 'ram':
                    if not (0xC000 <= addr <= 0xDFFF or 0xFF80 <= addr <= 0xFFFE):
                        fail('%s:%d: %s at %04X isn\'t in WRAM or HRAM' % (LIST, n, a[0], addr))
                    # (Gen I's symbols give D000-DFFF bank 0, the Game Boy having no other: the bank the cores map there)
                    bank = max(bank, 1) if 0xD000 <= addr <= 0xDFFF else 0
                elif addr >= 0x8000:
                    fail('%s:%d: %s at %04X isn\'t in the ROM' % (LIST, n, a[0], addr))
                v = {'bank': bank, 'addr': hexaddr(addr), 'symbol': a[0]}
                if len(found) == 1:
                    g[kind][key] = v
                else:
                    for rel in rels:
                        rel.setdefault(kind, {})[key] = v
    tables(pret, lines, games, constants)
    used = sorted({r for g in games.values() for r in g['folders']})
    for g in games.values():
        del g['syms'], g['folders'], g['kind']
    return {'schema': 1, 'kind': 'gb-pokemon-pret', 'games': games,
            'repos': {r: pret.repos[r] for r in used}}


def tables(pret, lines, games, constants):
    """The items, key items, types and physical/special split lines: each game's tables from its constants files."""
    items = [(n, key, a) for n, gs, kind, key, a in lines if kind == 'item']
    if len({key for n, key, a in items}) != len(items):
        fail('%s: an item is listed twice' % LIST)
    found = set()
    order = {'items': 0, 'types': 0, 'keyitems': 1, 'split': 1}    # (key items and splits read the others)
    for step in (0, 1):
        for n, gs, kind, key, a in lines:
            if order.get(kind) != step:
                continue
            for game in gs:
                if game not in games:
                    fail('%s:%d: %s has no release line' % (LIST, n, game))
                g = games[game]
                files = g.setdefault('files', {})
                if kind != 'split' and kind in files:
                    fail('%s:%d: %s\'s %s given twice' % (LIST, n, game, kind))
                if kind in ('items', 'types', 'keyitems'):
                    # (a ROM hack's versions in several folders: each folder's table, which must be the same)
                    got = [table(pret, g, game, kind, n, a[0], files, items, constants, repo) for repo in g['folders']]
                    if any(x != got[0] for x in got):
                        fail('%s:%d: %s\'s versions disagree on its %s (%s)' % (LIST, n, game, kind, ', '.join(g['folders'])))
                    g.update(got[0])
                    if kind == 'items':
                        found.update(got[0]['items'])
                else:   # split
                    if 'types' not in files:
                        fail('%s:%d: %s\'s split before its types line' % (LIST, n, game))
                    ids = [int(i) for i, t in g['types'].items() if t == TYPE_NAMES[a[0]]]
                    if len(ids) != 1:
                        fail('%s:%d: %s has no type %s' % (LIST, n, game, a[0]))
                    i = ids[0]
                    if a[1] == 'stats':
                        g.setdefault('split_stats', []).append(i)
                    elif a[1] == 'special' and i not in g['special']:
                        g['special'] = sorted(g['special'] + [i])
                    elif a[1] == 'physical' and i in g['special']:
                        g['special'].remove(i)
                    continue
                files[kind] = a[0]
    for ln, name, a in items:
        if name not in found:
            fail('%s:%d: item %s is in no game\'s item constants' % (LIST, ln, name))


def table(pret, g, game, kind, n, f, files, items, constants, repo):
    """An items, types or keyitems line's table for a game, from the constants in one of its folders."""
    out = {}
    if kind == 'items':
        # (by the const macro's names only: pokecrystal's SAFARI_BALL is a DEF of MOON_STONE's id)
        c = constants(g, f, repo)
        out['items'], out['balls'], taken = {}, [], {}
        for ln, name, (gen4, ball) in items:
            if name not in c.consts:
                continue
            i = c.consts[name]
            if not 1 <= i <= 255 or i in taken:
                fail('%s:%d: %s\'s %s is id %d (%s)' % (LIST, ln, game, name, i, taken.get(i, 'not an item\'s')))
            taken[i] = name
            out['items'][name] = [i, gen4]
            if ball:
                out['balls'].append(i)
        out['balls'].sort()
    elif kind == 'types':
        c = constants(g, f, repo)
        if 'SPECIAL' not in c.v:
            fail('%s:%d: %s/%s has no SPECIAL' % (LIST, n, repo, f))
        out['types'], out['special'] = {}, []
        for name, i in c.consts.items():
            if name in NOT_TYPES:
                continue
            if name not in TYPE_NAMES:
                fail('%s:%d: %s/%s: %s isn\'t a type the fork knows' % (LIST, n, repo, f, name))
            out['types'][str(i)] = TYPE_NAMES[name]
            if i >= c.v['SPECIAL']:
                out['special'].append(i)
    else:   # keyitems
        if 'items' not in files:
            fail('%s:%d: %s\'s key items before its items line' % (LIST, n, game))
        names = {i: k for k, i in constants(g, files['items'], repo).consts.items()}
        text = pret.read(repo, f)
        flags = re.findall(r'^\s*dbit\s+(TRUE|FALSE)\s*;\s*(\w+)', text, re.M)
        if not flags:
            fail('%s:%d: no dbit lines in %s/%s' % (LIST, n, repo, f))
        out['key_items'] = []
        for i, (on, name) in enumerate(flags, 1):
            if names.get(i) != name:
                fail('%s:%d: %s/%s: item %d is %s, not %s' % (LIST, n, repo, f, i, names.get(i), name))
            if on == 'TRUE':
                out['key_items'].append(i)
    return out


# --- SOURCES ---

def read_sources(text):
    """SOURCES' sections: name -> its lines (the header comment kept under None)."""
    secs: dict = {None: []}
    cur = None
    for line in text.splitlines():
        m = re.match(r'\[(\w+)\]$', line)
        if m:
            cur = m.group(1)
            secs[cur] = []
        else:
            secs[cur].append(line)
    return secs


HEADER = '''# Where the files of the Game Boy Pokémon companion come from, written by the three scripts that make them:
# kiwami/tools/pokedaisy-extract.py ([pokedaisy]), kiwami/tools/pret-extract.py ([pret]) and kiwami/tools/gbpk-port.py
# ([port]). Each section names the commits read (and [port] the ROMs, by their SHA-1s), then a line a file: "in" for
# what was read, "out" for what was written, with its SHA-256. Paths are the repository's, or the checkout's after its
# name. Each script's --check, without the checkouts, verifies the "out" files and the repository's own "in" files
# against these hashes.'''


def write_sources(text, name, body):
    secs: dict = read_sources(text) if text else {None: []}
    secs[name] = body
    out = [HEADER]
    for k in sorted(s for s in secs if s):
        while secs[k] and not secs[k][-1].strip():
            secs[k].pop()
        out += ['', '[%s]' % k] + secs[k]
    return '\n'.join(out) + '\n'


def verify_sources(name, files_in_repo):
    """--check without the checkouts: the section's out files, and the in files that are the repository's."""
    path = os.path.join(REPO, SOURCES)
    if not os.path.exists(path):
        fail('no %s' % SOURCES)
    sec = read_sources(open(path, encoding='utf-8').read()).get(name)
    if not sec:
        fail('%s has no [%s] section' % (SOURCES, name))
    checked = 0
    for line in sec:
        m = re.match(r'(in|out)\s+([0-9a-f]{64})\s+(.+)$', line)
        if not m:
            continue
        f = m.group(3)
        if m.group(1) == 'in' and f not in files_in_repo:
            continue
        p = os.path.join(REPO, f)
        if not os.path.exists(p):
            fail('%s is missing (%s, [%s])' % (f, SOURCES, name))
        if sha256(open(p, 'rb').read()) != m.group(2):
            fail('%s isn\'t the file %s\'s [%s] records: run the script again' % (f, SOURCES, name))
        checked += 1
    for f in files_in_repo:
        if not any(re.match(r'(in|out)\s+[0-9a-f]{64}\s+%s$' % re.escape(f), l) for l in sec):
            fail('%s\'s [%s] doesn\'t list %s' % (SOURCES, name, f))
    return checked


def dump(obj):
    return json.dumps(obj, indent=2, sort_keys=True, ensure_ascii=False) + '\n'


def main(argv):
    check = '--check' in argv
    args = [a for a in argv if a != '--check']
    if len(args) > 1 or any(a.startswith('-') for a in args):
        sys.exit('usage: pret-extract.py [<pret folder>] [--check]')
    listing = open(os.path.join(REPO, LIST), encoding='utf-8').read()
    lines = read_list(listing)
    if not args:
        if not check:
            fail('give the folder of pret\'s checkouts, or --check')
        n = verify_sources(SECTION, [LIST, OUT, PATCHES])
        extra = json.load(open(os.path.join(REPO, OUT), encoding='utf-8'))
        values = 0
        for ln, gs, kind, key, a in lines:
            if kind == 'item':
                if not any(key in g.get('items', {}) and g['items'][key][1] == a[0] for g in extra['games'].values()):
                    fail('%s:%d: no answer in %s' % (LIST, ln, OUT))
                values += 1
                continue
            for game in gs:
                g = extra['games'].get(game)
                if g is None:
                    fail('%s:%d: no %s in %s' % (LIST, ln, game, OUT))
                if kind == 'release':
                    ok = any(r.get('build') == a[1] and r['name'] == a[2] and len(r.get('sha1', '')) == 40
                             for r in g['releases'])
                elif kind == 'build':
                    ok = any(r.get('build') == a[1] and r['sha1'] == a[2] and r['name'] == a[3] for r in g['releases'])
                elif kind == 'hack':
                    ok = any(r.get('hack') and r['sha1'] == a[0] and r['name'] == a[1] for r in g['releases'])
                elif kind in ('items', 'keyitems', 'types'):
                    ok = g.get('files', {}).get(kind) == a[0]
                elif kind == 'split':
                    ids = [int(i) for i, t in g.get('types', {}).items() if t == TYPE_NAMES[a[0]]]
                    ok = len(ids) == 1 and (ids[0] in g.get('split_stats', []) if a[1] == 'stats' else
                                            (ids[0] in g.get('special', [])) == (a[1] == 'special'))
                else:
                    v = g.get(kind, {}).get(key)
                    # (or each release's own, where a ROM hack's versions disagree)
                    own = [r.get(kind, {}).get(key) for r in g['releases'] if not r.get('hack')]
                    ok = (v is not None and v.get('symbol') == a[-1]) or (
                        v is None and all(x is not None and x.get('symbol') == a[-1] for x in own))
                if not ok:
                    fail('%s:%d: no answer in %s' % (LIST, ln, OUT))
                values += 1
        print('pret-extract: %d files match SOURCES; %d values of %s answered in %s' % (n, values, LIST, OUT))
        return
    pret = Pret(os.path.abspath(args[0]))
    data = build(pret, lines)
    out = dump(data)
    src_path = os.path.join(REPO, SOURCES)
    old_sources = open(src_path, encoding='utf-8').read() if os.path.exists(src_path) else ''
    body = ['commit %s %s %s' % (r, v['commit'], 'tag %s' % v['tag'] if 'tag' in v else 'symbols %s' % v['symbols'])
            for r, v in sorted(data['repos'].items())]
    body += ['in  %s  %s' % (h, w) for h, w in sorted(pret.inputs, key=lambda x: x[1])]
    body += ['in  %s  %s' % (sha256(listing.encode('utf-8')), LIST)]
    body += ['in  %s  %s' % (sha256(open(os.path.join(REPO, PATCHES), 'rb').read()), PATCHES)]
    body += ['out %s  %s' % (sha256(out.encode('utf-8')), OUT)]
    sources = write_sources(old_sources, SECTION, body)
    if check:
        bad = []
        cur = open(os.path.join(REPO, OUT), encoding='utf-8').read() if os.path.exists(os.path.join(REPO, OUT)) else ''
        if cur != out:
            bad.append(OUT)
        if read_sources(old_sources).get(SECTION) != read_sources(sources)[SECTION]:
            bad.append('%s [%s]' % (SOURCES, SECTION))
        if bad:
            with tempfile.NamedTemporaryFile('w', suffix='.json', delete=False, encoding='utf-8') as t:
                t.write(out)
            fail('the checkouts give another %s (this run\'s %s is in %s)' % (' and '.join(bad), OUT, t.name))
        print('pret-extract: %s and SOURCES\' [pret] are the checkouts\' (%s)' %
              (OUT, ', '.join('%s %s' % (r, v['commit'][:8]) for r, v in sorted(data['repos'].items()))))
        return
    open(os.path.join(REPO, OUT), 'w', encoding='utf-8').write(out)
    os.makedirs(os.path.dirname(src_path), exist_ok=True)
    open(src_path, 'w', encoding='utf-8').write(sources)
    print('pret-extract: wrote %s (%d games) and SOURCES\' [pret]' % (OUT, len(data['games'])))


if __name__ == '__main__':
    main(sys.argv[1:])
