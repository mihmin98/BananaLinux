#!/usr/bin/env python3
# The Game Boy Pokémon companion's data files from PokeDaisy (https://github.com/lidor30/pokedaisy, GPL-3.0 with the
# 7(b) attribution term; kiwami/GB-POKEMON.md): each game's RAM addresses, read from PokeDaisy's Kotlin readers, with
# the releases' SHA-1s from its GameTitles.kt, merged with what pret's disassemblies add (kiwami/tools/gbpk-extra.json,
# written by pret-extract.py), into kiwami/rootfs/usr/share/banana/companions/gb-pokemon/<game>.ram.json.
#
#   pokedaisy-extract.py <pokedaisy checkout> [--any-commit]   write the data files, LICENSE and SOURCES' [pokedaisy]
#   pokedaisy-extract.py <pokedaisy checkout> --check          write them to a temporary folder, compare with the repo's
#   pokedaisy-extract.py --check                               without the checkout: the files against SOURCES' hashes
#
# A checkout at another commit than PIN is refused unless --any-commit is given. PokeDaisy's hand-written configs
# (Gen2Config(...) with named arguments, wramx(bank, addr), .copy(...) of another config; Gen2DexTables(...) with
# bank to address pairs) are read by a small evaluator: a field it can't read is listed under "unread:" and fails the
# run; the fields that are the games' own tables (SKIP, DEX_SKIP) are left out by name. A value both sources give must
# agree. Python 3, standard library only.
import importlib.util
import json
import os
import re
import sys
import tempfile
from typing import NoReturn

PIN = '135ac809a597230c1fe3ad6359ae0fe81e233583'      # 1.5.0.1
DATA = 'app/src/main/kotlin/com/pokedaisy/app/companion/data'
GEN2 = DATA + '/Gen2Reader.kt'
GEN2_DEX = DATA + '/Gen2Dex.kt'
CRYSTAL_GUIDE = DATA + '/GuideCrystalGen.kt'
TITLES = 'app/src/main/kotlin/com/pokedaisy/app/GameTitles.kt'

# pret-extract.py's SOURCES helpers (one file, two sections)
_spec = importlib.util.spec_from_file_location('pret_extract',
                                               os.path.join(os.path.dirname(os.path.abspath(__file__)), 'pret-extract.py'))
assert _spec and _spec.loader
pe = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(pe)
REPO, SOURCES, EXTRA = pe.REPO, pe.SOURCES, pe.OUT
FOLDER = os.path.dirname(SOURCES)
SECTION = 'pokedaisy'

# The games: their key, generation, title, PokeDaisy's config, its ROM tables' config (Gen2Dex.kt) and the title
# GameTitles.kt gives their releases
GAMES = [
    ('crystal', 2, 'Crystal', 'GEN2_CRYSTAL', 'GEN2_DEX_CRYSTAL', 'Pokémon Crystal'),
    ('gold', 2, 'Gold', 'GEN2_GOLD', 'GEN2_DEX_GOLD_SILVER', 'Pokémon Gold'),
    ('silver', 2, 'Silver', 'GEN2_SILVER', 'GEN2_DEX_GOLD_SILVER', 'Pokémon Silver'),
]

# Gen2Config's fields that are the games' own tables, or PokeDaisy's features outside the companion: left out
SKIP = {
    'landmarks': 'a table of the game\'s (map to landmark), read from the ROM instead',
    'genderRatios': 'a table of the game\'s, read from the ROM instead',
    'tmItems': 'a table of the game\'s, read from the ROM instead',
    'pokedex': 'the ROM tables of PokeDaisy\'s Pokédex pages (later steps read their own)',
    'mailItems': 'item ids of the game\'s',
    'growthRates': 'a table of the game\'s, read from the ROM instead',
    'cursorTile': 'a tile id for PokeDaisy\'s battle buttons, not used',
}

# Gen2DexTables' fields: the ROM tables the data files' rom section takes (by its key), the numbers its sizes take,
# and what is left out (the sprites' come with them, later; the rest are PokeDaisy's own or the games' tables)
DEX_ROM = {'baseData': 'baseData', 'dexEntryPointers': 'dexEntryPointers', 'dexEntryBanks': 'dexEntryBanks',
           'itemDescriptions': 'itemDescriptions', 'tmHmMoves': 'tmHmMoves', 'moveDescriptions': 'moveDescriptions'}
DEX_SIZES = {'firstTm': 'firstTm'}
DEX_SKIP = {
    'picPointers': 'the sprites\' tables, read with them', 'unownPicPointers': 'the sprites\' tables, read with them',
    'picsBanks': 'the sprites\' tables, read with them', 'palettes': 'the sprites\' tables, read with them',
    'picBankFixes': 'the sprites\' tables, read with them',
    'tmItems': 'a table of the game\'s (the TM items\' ids)',
    'dexEntryFirstBank': 'checked against pret\'s rom.dexEntryFirst instead',
    'textDict': 'a ROM hack\'s', 'packed': 'a ROM hack\'s', 'dexOrder': 'a ROM hack\'s', 'baseDataSize': 'a ROM hack\'s',
    'picBankDirect': 'a ROM hack\'s', 'unown': 'the sprites\'', 'lastItem': 'firstTm - 1 in the retail games',
    'dexEntryFar': 'a ROM hack\'s', 'polished': 'a ROM hack\'s',
}
# (Gold's and Silver's dexEntryBanks is PokeDaisy's placeholder: their entries' bank is counted from the first's)
DEX_UNUSED = {('GEN2_DEX_GOLD_SILVER', 'dexEntryBanks')}

# GuideCrystalGen.kt's RAM constants (WRAM bank 1), by the data file's key
CRYSTAL_GUIDE_RAM = {'CR_EVENT_FLAGS': 'eventFlags', 'CR_TIME_OF_DAY': 'timeOfDay', 'CR_BADGES': 'johtoBadges',
                     'CR_TRADE_FLAGS': 'tradeFlags'}
CRYSTAL_GUIDE_SIZES = {'CR_EVENT_FLAG_BYTES': 'eventFlagBytes', 'CR_TRADE_FLAG_BYTES': 'tradeFlagBytes'}


def fail(msg) -> NoReturn:
    sys.exit('pokedaisy-extract: ' + msg)


setattr(pe, 'PROG', 'pokedaisy-extract')


# --- A little Kotlin ---

def strip_comments(src):
    """The source without // and /* */ comments (nested, as Kotlin's), strings and characters kept as they are."""
    out, i, n = [], 0, len(src)
    while i < n:
        c = src[i]
        if c in '"\'':
            j = i + 1
            while j < n and src[j] != c:
                j += 2 if src[j] == '\\' else 1
            out.append(src[i:j + 1])
            i = j + 1
        elif src.startswith('//', i):
            while i < n and src[i] != '\n':
                i += 1
        elif src.startswith('/*', i):
            depth, i = 1, i + 2
            while i < n and depth:
                if src.startswith('/*', i):
                    depth, i = depth + 1, i + 2
                elif src.startswith('*/', i):
                    depth, i = depth - 1, i + 2
                else:
                    i += 1
            out.append(' ')
        else:
            out.append(c)
            i += 1
    return ''.join(out)


def closing(s, i):
    """The index of the bracket that closes the one at s[i]."""
    depth, j = 0, i
    while j < len(s):
        if s[j] == '"':
            j += 1
            while j < len(s) and s[j] != '"':
                j += 2 if s[j] == '\\' else 1
        elif s[j] in '([{':
            depth += 1
        elif s[j] in ')]}':
            depth -= 1
            if depth == 0:
                return j
        j += 1
    fail('no closing bracket for %s' % s[i:i + 60])


def split_args(s, angles=False):
    """A list's items at the top level (with angles, < > nest too: a field's type, Map<Int, Int>)."""
    opens, closes = ('([{<', ')]}>') if angles else ('([{', ')]}')
    out, depth, cur = [], 0, []
    for c in s:
        if c in opens:
            depth += 1
        elif c in closes:
            depth -= 1
        if c == ',' and depth == 0:
            out.append(''.join(cur).strip())
            cur = []
        else:
            cur.append(c)
    if ''.join(cur).strip():
        out.append(''.join(cur).strip())
    return out


def evaluate(expr):
    """A config's value: ('addr', bank, address), an int, a bool, or None when it isn't one of those."""
    e = expr.strip()
    m = re.fullmatch(r'wramx\(\s*(\d+)\s*,\s*(0x[0-9A-Fa-f]+|\d+)\s*\)', e)
    if m:
        return ('addr', int(m.group(1)), int(m.group(2), 0))
    m = re.fullmatch(r'(-?)(0x[0-9A-Fa-f]+|\d+)L?', e)
    if m:
        return int(m.group(1) + str(int(m.group(2), 0)))
    if e in ('true', 'false'):
        return e == 'true'
    return None


def read_configs(src):
    """Gen2Config's fields (name -> (type, default text)) and every 'val X = Gen2Config(...)' or '= Y.copy(...)'."""
    s = strip_comments(src)
    m = re.search(r'data class Gen2Config\(', s)
    if not m:
        fail('no data class Gen2Config in %s' % GEN2)
    params = s[m.end():closing(s, m.end() - 1)]
    fields = {}
    for p in split_args(params, angles=True):
        pm = re.fullmatch(r'val\s+(\w+)\s*:\s*([^=]+?)\s*(?:=\s*(.+))?', p, re.S)
        if not pm:
            fail('a Gen2Config field it can\'t read: %s' % p)
        fields[pm.group(1)] = (pm.group(2).strip(), pm.group(3))
    configs = {}
    for m in re.finditer(r'\bval\s+(\w+)\s*=\s*(Gen2Config|\w+\.copy)\(', s):
        body = s[m.end():closing(s, m.end() - 1)]
        args = {}
        for a in split_args(body):
            am = re.fullmatch(r'(\w+)\s*=\s*(.+)', a, re.S)
            if not am:
                fail('%s: an argument it can\'t read: %s' % (m.group(1), a))
            args[am.group(1)] = am.group(2).strip()
        base = None if m.group(2) == 'Gen2Config' else m.group(2)[:-len('.copy')]
        configs[m.group(1)] = (base, args)
    return fields, configs


def resolve(name, fields, configs):
    """A config's arguments over its base's, over Gen2Config's defaults."""
    if name not in configs:
        fail('no config %s in %s' % (name, GEN2))
    base, args = configs[name]
    if base:
        out = resolve(base, fields, configs)
    else:
        out = {k: d for k, (t, d) in fields.items()}
    for k, v in args.items():
        if k not in fields:
            fail('%s sets %s, which Gen2Config doesn\'t have' % (name, k))
        out[k] = v
    return out


def addr_value(v):
    """(bank, address) of an evaluated address: a plain one in D000-DFFF is bank 1's, as the cores map it."""
    if isinstance(v, tuple):
        return v[1], v[2]
    return (1 if 0xD000 <= v <= 0xDFFF else 0), v


def config_values(name, fields, configs):
    ram, sizes, unread = {}, {}, []
    for k, text in resolve(name, fields, configs).items():
        if k in SKIP:
            continue
        typ = fields[k][0]
        if text is None:
            unread.append('%s.%s (no value)' % (name, k))
            continue
        v = evaluate(text)
        if typ == 'Long' and v is not None and not isinstance(v, bool):
            if v == -1:
                continue
            bank, a = addr_value(v)
            if not (0xC000 <= a <= 0xDFFF or 0xFF80 <= a <= 0xFFFE):
                unread.append('%s.%s (%s isn\'t in WRAM or HRAM)' % (name, k, text))
                continue
            ram[k] = (bank, a)
        elif typ == 'Int' and isinstance(v, int) and not isinstance(v, bool):
            if v:
                sizes[k] = v
        elif typ == 'Boolean' and isinstance(v, bool):
            if v:
                unread.append('%s.%s (true: a layout the companion doesn\'t read)' % (name, k))
        else:
            unread.append('%s.%s = %s' % (name, k, text))
    return ram, sizes, unread


def read_dex(src, name):
    """A Gen2DexTables config's fields: name -> (bank, address) for a pair, an int, or the text it couldn't read."""
    s = strip_comments(src)
    m = re.search(r'\bval\s+%s\s*=\s*Gen2DexTables\(' % name, s)
    if not m:
        fail('no %s = Gen2DexTables(...) in %s' % (name, GEN2_DEX))
    out = {}
    for a in split_args(s[m.end():closing(s, m.end() - 1)]):
        am = re.fullmatch(r'(\w+)\s*=\s*(.+)', a, re.S)
        if not am:
            fail('%s: an argument it can\'t read: %s' % (name, a))
        v = am.group(2).strip()
        pm = re.fullmatch(r'(0x[0-9A-Fa-f]+|\d+)\s+to\s+(0x[0-9A-Fa-f]+|\d+)', v)
        out[am.group(1)] = (int(pm.group(1), 0), int(pm.group(2), 0)) if pm else evaluate(v) if evaluate(v) is not None else v
    return out


def guide_consts(src):
    return {m.group(1): int(m.group(2), 0)
            for m in re.finditer(r'const val (\w+)\s*=\s*(0x[0-9A-Fa-f]+|\d+)\b', strip_comments(src))}


def retail_sha1s(src):
    s = strip_comments(src)
    m = re.search(r'private val RETAIL\s*:[^=]*=\s*mapOf\(', s)
    if not m:
        fail('no RETAIL map in %s' % TITLES)
    body = s[m.end():closing(s, m.end() - 1)]
    out = {}
    for h, title in re.findall(r'"([0-9a-f]{40})"\s+to\s+"([^"]+)"', body):
        out.setdefault(title, set()).add(h)
    return out


# --- The data files ---

def build(checkout):
    inputs = {}

    def read(rel):
        data = open(os.path.join(checkout, rel), 'rb').read()
        inputs['pokedaisy ' + rel] = pe.sha256(data)
        return data.decode('utf-8')

    fields, configs = read_configs(read(GEN2))
    dex_src = read(GEN2_DEX)
    guide = guide_consts(read(CRYSTAL_GUIDE))
    titles = retail_sha1s(read(TITLES))
    extra_text = open(os.path.join(REPO, EXTRA), encoding='utf-8').read()
    inputs[EXTRA] = pe.sha256(extra_text.encode('utf-8'))
    extra = json.loads(extra_text)
    files, unread = {}, []
    for key, gen, title, cfg, dex_cfg, pd_title in GAMES:
        ram, sizes, u = config_values(cfg, fields, configs)
        unread += u
        rom, rom_src = {}, {}
        dex = read_dex(dex_src, dex_cfg)
        for k, v in dex.items():
            if (dex_cfg, k) in DEX_UNUSED or k in DEX_SKIP:
                continue
            if k in DEX_ROM and isinstance(v, tuple):
                rom[DEX_ROM[k]], rom_src[DEX_ROM[k]] = v, 'pokedaisy'
            elif k in DEX_SIZES and isinstance(v, int) and not isinstance(v, bool):
                sizes[DEX_SIZES[k]] = v
            else:
                unread.append('%s.%s = %s' % (dex_cfg, k, v))
        src = {k: 'pokedaisy' for k in ram}
        if key == 'crystal':
            for c, k in CRYSTAL_GUIDE_RAM.items():
                if c not in guide:
                    unread.append('%s: no %s' % (CRYSTAL_GUIDE, c))
                    continue
                v = addr_value(('addr', 1, guide[c]))
                if k in ram and ram[k] != v:
                    fail('crystal ram.%s: %s says %02X:%04X, %s %02X:%04X' % (k, GEN2, ram[k][0], ram[k][1], c, v[0], v[1]))
                ram[k], src[k] = v, 'pokedaisy'
            for c, k in CRYSTAL_GUIDE_SIZES.items():
                if c not in guide:
                    unread.append('%s: no %s' % (CRYSTAL_GUIDE, c))
                    continue
                sizes[k] = guide[c]
        g = extra['games'].get(key)
        if g is None:
            fail('%s has no %s' % (EXTRA, key))
        for k, v in g['ram'].items():
            pv = (v['bank'], int(v['addr'], 16))
            if k in ram and ram[k] != pv:
                fail('%s ram.%s: PokeDaisy says %02X:%04X, pret\'s %s %02X:%04X' %
                     (key, k, ram[k][0], ram[k][1], v['symbol'], pv[0], pv[1]))
            src[k] = 'pokedaisy pret (%s)' % v['symbol'] if k in ram else 'pret (%s)' % v['symbol']
            ram[k] = pv
        for k, v in g.get('rom', {}).items():
            pv = (v['bank'], int(v['addr'], 16))
            if k in rom and rom[k] != pv:
                fail('%s rom.%s: PokeDaisy says %02X:%04X, pret\'s %s %02X:%04X' %
                     (key, k, rom[k][0], rom[k][1], v['symbol'], pv[0], pv[1]))
            rom_src[k] = 'pokedaisy pret (%s)' % v['symbol'] if k in rom else 'pret (%s)' % v['symbol']
            rom[k] = pv
        if 'dexEntryFirst' in rom and dex.get('dexEntryFirstBank') != rom['dexEntryFirst'][0]:
            fail('%s: PokeDaisy\'s dexEntryFirstBank %s isn\'t pret\'s first Pokédex entry\'s bank %02X' %
                 (key, dex.get('dexEntryFirstBank'), rom['dexEntryFirst'][0]))
        for k, v in g.get('const', {}).items():
            if k != 'numEvents':
                sizes[k] = v['value']
        if 'numEvents' in g.get('const', {}):
            n = g['const']['numEvents']['value']
            if n % 8:
                fail('%s: NUM_EVENTS %d isn\'t whole bytes' % (key, n))
            if sizes.get('eventFlagBytes', n // 8) != n // 8:
                fail('%s: PokeDaisy has %d bytes of event flags, pret %d' % (key, sizes['eventFlagBytes'], n // 8))
            sizes['eventFlagBytes'] = n // 8
        pret_sha1 = {r['sha1'] for r in g['releases']}
        if titles.get(pd_title, set()) != pret_sha1:
            fail('%s: GameTitles.kt\'s "%s" SHA-1s %s aren\'t pret\'s %s' %
                 (key, pd_title, sorted(titles.get(pd_title, ())), sorted(pret_sha1)))
        releases = [{k: r[k] for k in ('sha1', 'name', 'code') if k in r} for r in g['releases']]
        doc = {
            'schema': 1, 'kind': 'gb-pokemon-ram', 'game': key, 'gen': gen, 'title': title,
            'header_title': g['header_title'], 'releases': releases,
            'ram': {k: {'bank': b, 'addr': pe.hexaddr(a), 'src': src[k]} for k, (b, a) in ram.items()},
            'sizes': sizes, 'flags': [],
            'rom': {k: {'bank': b, 'addr': pe.hexaddr(a), 'src': rom_src[k]} for k, (b, a) in rom.items()},
        }
        files['%s/%s.ram.json' % (FOLDER, key)] = pe.dump(doc).encode('utf-8')
    if unread:
        print('unread:', file=sys.stderr)
        for u in unread:
            print('  ' + u, file=sys.stderr)
        fail('%d values unread' % len(unread))
    files[FOLDER + '/LICENSE'] = open(os.path.join(checkout, 'LICENSE'), 'rb').read()
    inputs['pokedaisy LICENSE'] = pe.sha256(files[FOLDER + '/LICENSE'])
    return files, inputs


def main(argv):
    check = '--check' in argv
    any_commit = '--any-commit' in argv
    args = [a for a in argv if a not in ('--check', '--any-commit')]
    if len(args) > 1 or any(a.startswith('-') for a in args):
        sys.exit('usage: pokedaisy-extract.py [<pokedaisy checkout>] [--check] [--any-commit]')
    outs = ['%s/%s.ram.json' % (FOLDER, g[0]) for g in GAMES] + [FOLDER + '/LICENSE']
    if not args:
        if not check:
            fail('give PokeDaisy\'s checkout, or --check')
        n = pe.verify_sources(SECTION, outs + [EXTRA])
        print('pokedaisy-extract: %d files match SOURCES\' [pokedaisy]' % n)
        return
    checkout = os.path.abspath(args[0])
    head = pe.git(checkout, 'rev-parse', 'HEAD').decode().strip()
    if head != PIN and not any_commit:
        fail('%s is at %s, not PokeDaisy %s (--any-commit to read it anyway)' % (checkout, head[:8], PIN[:8]))
    files, inputs = build(checkout)
    body = ['commit pokedaisy %s' % head]
    body += ['in  %s  %s' % (h, w) for w, h in sorted(inputs.items())]
    body += ['out %s  %s' % (pe.sha256(d), f) for f, d in sorted(files.items())]
    src_path = os.path.join(REPO, SOURCES)
    old = open(src_path, encoding='utf-8').read() if os.path.exists(src_path) else ''
    sources = pe.write_sources(old, SECTION, body)
    if check:
        bad = [f for f, d in sorted(files.items())
               if not os.path.exists(os.path.join(REPO, f)) or open(os.path.join(REPO, f), 'rb').read() != d]
        if pe.read_sources(old).get(SECTION) != pe.read_sources(sources)[SECTION]:
            bad.append('%s [%s]' % (SOURCES, SECTION))
        if bad:
            t = tempfile.mkdtemp(prefix='pokedaisy-extract.')
            for f, d in files.items():
                open(os.path.join(t, os.path.basename(f)), 'wb').write(d)
            fail('the checkout gives other files: %s (this run\'s are in %s)' % (', '.join(bad), t))
        print('pokedaisy-extract: the data files and SOURCES\' [pokedaisy] are PokeDaisy %s\'s' % head[:8])
        return
    os.makedirs(os.path.join(REPO, FOLDER), exist_ok=True)
    for f, d in files.items():
        open(os.path.join(REPO, f), 'wb').write(d)
    open(src_path, 'w', encoding='utf-8').write(sources)
    print('pokedaisy-extract: wrote %s' % ', '.join(os.path.basename(f) for f in sorted(files)))


if __name__ == '__main__':
    main(sys.argv[1:])
