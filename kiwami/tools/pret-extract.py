#!/usr/bin/env python3
# The Game Boy Pokémon companion's facts from pret's disassemblies (pokecrystal, pokegold, pokered, pokeyellow): the
# addresses, ROM offsets and constants kiwami/tools/gbpk-symbols.txt asks for, looked up in each release's symbol
# file, and the releases' SHA-1s and ROM headers, written to kiwami/tools/gbpk-extra.json. Only facts about the retail
# ROMs are taken, none of pret's code or tables (kiwami/GB-POKEMON.md).
#
#   pret-extract.py <pret folder>            write gbpk-extra.json and SOURCES' [pret] section
#   pret-extract.py <pret folder> --check    write them to a temporary folder and compare with what's committed
#   pret-extract.py --check                  without the checkouts: the committed files against SOURCES' hashes,
#                                            and every line of gbpk-symbols.txt answered in gbpk-extra.json
#
# <pret folder> holds a clone of each repository named in the list, with its symbols branch (git fetch origin
# symbols:symbols): the .sym files are read from that branch with git show, the constants, Makefile and roms.sha1
# from the checkout's HEAD. Python 3, standard library only.
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

def read_list(text):
    """The lines of gbpk-symbols.txt: (line number, games, kind, key, args)."""
    out = []
    for n, line in enumerate(text.splitlines(), 1):
        line = line.split('#', 1)[0].strip()
        if not line:
            continue
        parts = line.split(None, 2)
        if len(parts) < 3:
            fail('%s:%d: too short: %s' % (LIST, n, line))
        games = parts[0].split(',')
        if parts[1] == 'release':
            a = parts[2].split(None, 2)
            if len(a) != 3:
                fail('%s:%d: a release is <game> release <repo> <build> <name>' % (LIST, n))
            out.append((n, games, 'release', None, a))
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
    """Enough of rgbds' assembler to evaluate a constants file: DEF and REDEF (EQU, =, +=, -=), a name with a number
    in it ({02d:NAME}), the const macros of macros/const.asm (const_def, const, const_skip, const_next, shift_const)
    and the file's own MACROs. An ASSERT, which defines nothing, is passed over. Anything else stops the run, so a
    value is never computed from a line that wasn't understood. A DEF whose expression names a symbol the file
    doesn't define (one from hardware.inc, which it doesn't read, or a move's id in an item's alias) is set aside:
    asking for that name stops the run with the reason, any other line using it too."""

    TOKEN = re.compile(r'\s*(?:(\$[0-9A-Fa-f]+)|(%[01]+)|(\d+)|([A-Za-z_][A-Za-z0-9_@#.]*)|(<<|>>|[-+*/%&|^~()]))')

    def __init__(self, name):
        self.name = name
        self.v = {'const_value': 0, 'const_inc': 1}
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
                out.append(dec)
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
            m = re.match(r'MACRO\??\s+(\w+)$', line)
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
            # (a symbol's name made with a number: {02d:HM_VALUE} is HM_VALUE's, two digits)
            line = re.sub(r'\{(?:0?(\d*)d:)?(\w+)\}', lambda x: '%0*d' % (int(x.group(1) or 0), self.expr(x.group(2), where)), line)
            m = re.match(r'(?:RE)?DEF\s+(\w+)\s*(EQU|=|\+=|-=)\s*(.+)$', line)
            if m and m.group(2) in ('EQU', '='):
                try:
                    self.expr(m.group(3), where)
                except Consts.Undefined as e:
                    self.v.pop(m.group(1), None)
                    self.unknown[m.group(1)] = '%s (%s)' % (m.group(1), e)
                    continue
            if m:
                name, op, val = m.group(1), m.group(2), self.expr(m.group(3), where)
                if op in ('EQU', '='):
                    self.v[name] = val
                    self.unknown.pop(name, None)
                elif name not in self.v:
                    fail('%s: %s %s before it was defined' % (where, name, op))
                else:
                    self.v[name] += val if op == '+=' else -val
                continue
            word, _, rest = line.partition(' ')
            a = [x.strip() for x in rest.split(',')] if rest.strip() else []
            if word == 'const_def':
                self.v['const_value'] = self.expr(a[0], where) if a else 0
                self.v['const_inc'] = self.expr(a[1], where) if len(a) > 1 else 1
            elif word == 'const' and len(a) == 1:
                self.v[a[0]] = self.v['const_value']
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

    def repo(self, name):
        if name not in self.repos:
            path = os.path.join(self.folder, name)
            if not os.path.isdir(path):
                fail('no checkout of %s in %s' % (name, self.folder))
            head = git(path, 'rev-parse', 'HEAD').decode().strip()
            sym = git(path, 'rev-parse', 'symbols').decode().strip()
            self.repos[name] = {'commit': head, 'symbols': sym}
        return os.path.join(self.folder, name)

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
    # the releases first: they say which repository and symbol files a game's values come from
    for n, gs, kind, key, a in lines:
        if kind != 'release':
            continue
        if len(gs) != 1:
            fail('%s:%d: a release names one game' % (LIST, n))
        repo, bld, name = a
        g = games.setdefault(gs[0], {'repo': repo, 'releases': [], 'ram': {}, 'rom': {}, 'const': {}})
        if g['repo'] != repo:
            fail('%s:%d: %s\'s releases are in two repositories' % (LIST, n, gs[0]))
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
    for n, gs, kind, key, a in lines:
        if kind in ('release', 'hack'):
            continue
        for game in gs:
            if game not in games:
                fail('%s:%d: %s has no release line' % (LIST, n, game))
            g = games[game]
            if key in g[kind]:
                fail('%s:%d: %s %s.%s given twice' % (LIST, n, game, kind, key))
            if kind == 'const':
                f, name = a
                ck = (g['repo'], f)
                if ck not in consts:
                    c = Consts('%s/%s' % ck)
                    try:
                        c.run(pret.read(g['repo'], f))
                    except Consts.Undefined as e:
                        fail(str(e))
                    consts[ck] = c
                if name in consts[ck].unknown:
                    fail('%s:%d: %s/%s: %s' % (LIST, n, g['repo'], f, consts[ck].unknown[name]))
                if name not in consts[ck].v:
                    fail('%s:%d: %s isn\'t in %s/%s' % (LIST, n, name, g['repo'], f))
                g['const'][key] = {'value': consts[ck].v[name], 'symbol': name, 'file': f}
                continue
            found = set()
            for rel, syms in zip([r for r in g['releases'] if not r.get('hack')], g['syms']):
                if a[0] not in syms:
                    fail('%s:%d: %s isn\'t in %s.sym' % (LIST, n, a[0], rel['build']))
                found.add(syms[a[0]])
            if len(found) != 1:
                fail('%s:%d: %s\'s releases disagree on %s: %s' % (LIST, n, game, a[0],
                     ', '.join('%02X:%04X' % b for b in sorted(found))))
            bank, addr = found.pop()
            if kind == 'ram':
                if not (0xC000 <= addr <= 0xDFFF or 0xFF80 <= addr <= 0xFFFE):
                    fail('%s:%d: %s at %04X isn\'t in WRAM or HRAM' % (LIST, n, a[0], addr))
                # (Gen I's symbols give D000-DFFF bank 0, the Game Boy having no other: the bank the cores map there)
                bank = max(bank, 1) if 0xD000 <= addr <= 0xDFFF else 0
            elif addr >= 0x8000:
                fail('%s:%d: %s at %04X isn\'t in the ROM' % (LIST, n, a[0], addr))
            g[kind][key] = {'bank': bank, 'addr': hexaddr(addr), 'symbol': a[0]}
    for g in games.values():
        del g['syms']
    used = sorted({g['repo'] for g in games.values()})
    return {'schema': 1, 'kind': 'gb-pokemon-pret', 'games': games,
            'repos': {r: pret.repos[r] for r in used}}


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


HEADER = '''# Where the files of the Game Boy Pokémon companion come from, written by the two scripts that make them:
# kiwami/tools/pokedaisy-extract.py ([pokedaisy]) and kiwami/tools/pret-extract.py ([pret]). Each section names the
# commits read, then a line a file: "in" for what was read, "out" for what was written, with its SHA-256. Paths are
# the repository's, or the checkout's after its name. Each script's --check, without the checkouts, verifies the
# "out" files and the repository's own "in" files against these hashes.'''


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
            for game in gs:
                g = extra['games'].get(game)
                if g is None:
                    fail('%s:%d: no %s in %s' % (LIST, ln, game, OUT))
                if kind == 'release':
                    ok = any(r.get('build') == a[1] and r['name'] == a[2] and len(r.get('sha1', '')) == 40
                             for r in g['releases'])
                elif kind == 'hack':
                    ok = any(r.get('hack') and r['sha1'] == a[0] and r['name'] == a[1] for r in g['releases'])
                else:
                    v = g.get(kind, {}).get(key)
                    ok = v is not None and v.get('symbol') == a[-1]
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
    body = ['commit %s %s symbols %s' % (r, v['commit'], v['symbols']) for r, v in sorted(data['repos'].items())]
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
