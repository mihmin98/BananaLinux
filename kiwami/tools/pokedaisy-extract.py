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
# (Gen2Config(...) with named arguments, wramx(bank, addr), .copy(...) of another config; Gen2DexTables(...) and
# Gen2ArtTables(...) with bank to address pairs, the latter also by position over another's, X.let { ... it.field };
# Gen1ArtTables(...) also as a function's, val X = f(...) with fun f(...) = Gen1ArtTables(...))
# are read by a small evaluator: a field it can't read is listed under "unread:" and fails the
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
GEN1 = DATA + '/Gen1Reader.kt'
GEN1_DEX = DATA + '/Gen1Dex.kt'
GEN2 = DATA + '/Gen2Reader.kt'
GEN2_DEX = DATA + '/Gen2Dex.kt'
GEN2_ART = DATA + '/Gen2Art.kt'
GEN1_ART = DATA + '/Gen1Art.kt'
CRYSTAL_GUIDE = DATA + '/GuideCrystalGen.kt'
CRYSTAL_RULES = DATA + '/GuideCrystal.kt'
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

# The games: their key, generation, title, PokeDaisy's config, its ROM tables' config (Gen2Dex.kt, Gen1Dex.kt), its
# art's (Gen2Art.kt, Gen1Art.kt) and the title GameTitles.kt gives their releases
GAMES = [
    ('crystal', 2, 'Crystal', 'GEN2_CRYSTAL', 'GEN2_DEX_CRYSTAL', 'CRYSTAL', 'Pokémon Crystal'),
    ('gold', 2, 'Gold', 'GEN2_GOLD', 'GEN2_DEX_GOLD_SILVER', 'GOLD', 'Pokémon Gold'),
    ('silver', 2, 'Silver', 'GEN2_SILVER', 'GEN2_DEX_GOLD_SILVER', 'SILVER', 'Pokémon Silver'),
    ('red', 1, 'Red', 'GEN1_RED', 'GEN1_DEX_RED', 'RED', 'Pokémon Red'),
    ('blue', 1, 'Blue', 'GEN1_BLUE', 'GEN1_DEX_RED', 'BLUE', 'Pokémon Blue'),
    ('yellow', 1, 'Yellow', 'GEN1_YELLOW', 'GEN1_DEX_YELLOW', 'YELLOW', 'Pokémon Yellow'),
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

# Gen1Config's fields that are the games' own tables, or PokeDaisy's features outside the companion: left out
# (language is checked to be English's instead)
GEN1_SKIP = {
    'internalToDex': 'a table of the game\'s (its species\' internal order), read from the ROM instead',
    'pokedex': 'the ROM tables of PokeDaisy\'s Pokédex pages (later steps read their own)',
    'language': 'English\'s (checked)',
    'gameCode': 'the European Yellows\' code',
    'battleMenuLeftX': 'a cursor column for PokeDaisy\'s battle buttons, not used',
    'battleMenuRightX': 'a cursor column for PokeDaisy\'s battle buttons, not used',
    'battlePartyMenu': 'a menu id for PokeDaisy\'s battle switch, not used',
}

# Gen2DexTables' fields: the ROM tables the data files' rom section takes (by its key), the numbers its sizes take,
# and what is left out (PokeDaisy's own or the games' tables)
DEX_ROM = {'baseData': 'baseData', 'dexEntryPointers': 'dexEntryPointers', 'dexEntryBanks': 'dexEntryBanks',
           'itemDescriptions': 'itemDescriptions', 'tmHmMoves': 'tmHmMoves', 'moveDescriptions': 'moveDescriptions',
           'picPointers': 'picPointers', 'unownPicPointers': 'unownPicPointers', 'picsBanks': 'picsBanks',
           'picBankFixes': 'picBankFixes', 'palettes': 'monPalettes'}
DEX_SIZES = {'firstTm': 'firstTm'}
DEX_SKIP = {
    'tmItems': 'a table of the game\'s (the TM items\' ids)',
    'dexEntryFirstBank': 'checked against pret\'s rom.dexEntryFirst instead',
    'textDict': 'a ROM hack\'s', 'packed': 'a ROM hack\'s', 'dexOrder': 'a ROM hack\'s', 'baseDataSize': 'a ROM hack\'s',
    'picBankDirect': 'a ROM hack\'s', 'unown': 'Unown\'s species, 201 in the retail games', 'lastItem': 'firstTm - 1 in the retail games',
    'dexEntryFar': 'a ROM hack\'s', 'polished': 'a ROM hack\'s',
}
# (Gold's and Silver's dexEntryBanks and picsBanks are PokeDaisy's placeholders: their entries' bank is counted from
# the first's, and their pictures' banks are fixed by picBankFixes' pairs)
DEX_UNUSED = {('GEN2_DEX_GOLD_SILVER', 'dexEntryBanks'), ('GEN2_DEX_GOLD_SILVER', 'picsBanks')}

# Gen1DexTables' fields: the ROM tables the data files' rom section takes (by its key), the Pokédex's RAM, which
# pret's must agree with, the front pictures' banks (checked against pret's: DEX1_PICS), the fields whose default is
# the English retail games' (checked), and what is left out
DEX1_ROM = {'baseStats': 'baseData', 'dexEntryPointers': 'dexEntryPointers', 'mewBaseStats': 'mewBaseData',
            'monsterPalettes': 'monPaletteIds', 'cgbPalettes': 'palettes'}
DEX1_RAM = {'owned': 'dexCaught', 'seen': 'dexSeen'}
DEX1_PICS = ('picsBank', 'picsBankBreaks', 'mewPicsBank')
DEX1_DEFAULT = ('metric', 'charset', 'baseStatsSize', 'picBankOffset', 'dexEntryData', 'categoryMapping',
                'categoryStrings', 'textFar')
DEX1_SKIP = {'internalToDex': 'a table of the game\'s (its species\' internal order), read from the ROM instead'}
DEX1_SKIP.update({k: 'the museum\'s fossil picture, not a species\'' for k in ('fossilKabutops', 'fossilKabutopsBank')})

# Gen2ArtTables' fields: the Pokégear's maps the rom section takes (townMaps: Johto's, then Kanto's), and what is left
# out (PokeDaisy's own art: the font, the party icons, the player on the map)
ART_ROM = {'townMapGfx': 'townMapGfx', 'townMapPalMap': 'townMapPalMap', 'pokegearPals': 'pokegearPals'}
ART_LISTS = {'townMaps': ('johtoMap', 'kantoMap')}
ART_SKIP = {k: 'PokeDaisy\'s own art, not the companion\'s' for k in (
    'font', 'fontPath', 'icons', 'monMenuIcons', 'eggIcon', 'heldItemIcons', 'partyMenuObPals', 'iconsDir',
    'holdIconsPath', 'playerSprites', 'objectPals', 'playerPaths')}
ART_SKIP.update({'name': 'PokeDaisy\'s name for the art\'s files', 'sha1s': 'the releases, from GameTitles.kt instead',
                 'townMapPaths': 'PokeDaisy\'s files'})
# Gen1ArtTables' fields: the town map's tables the rom section takes, and what is left out (PokeDaisy's own art, and
# the colours it sampled from an emulator: the companion takes the palette from the ROM)
ART1_ROM = {'townMap': 'townMap', 'townMapTiles': 'townMapTiles'}
ART1_SKIP = {k: 'PokeDaisy\'s own art, not the companion\'s' for k in (
    'font', 'fontPath', 'iconPointers', 'iconPointerCount', 'monPartyData', 'iconsDir', 'iconColors', 'playerSprite',
    'playerPath', 'playerColors', 'iconSheet', 'iconSheet2')}
ART1_SKIP.update({'sha1': 'the releases, from GameTitles.kt instead', 'dexCount': '151, the species read from the ROM',
                  'townMapPath': 'PokeDaisy\'s file', 'townMapColors': 'colours sampled from an emulator; the ROM\'s instead',
                  'charset': 'English\'s, as the data files\' text'})

# GuideCrystalGen.kt's RAM constants (WRAM bank 1), by the data file's key
CRYSTAL_GUIDE_RAM = {'CR_EVENT_FLAGS': 'eventFlags', 'CR_TIME_OF_DAY': 'timeOfDay', 'CR_BADGES': 'johtoBadges',
                     'CR_TRADE_FLAGS': 'tradeFlags'}
CRYSTAL_GUIDE_SIZES = {'CR_EVENT_FLAG_BYTES': 'eventFlagBytes', 'CR_TRADE_FLAG_BYTES': 'tradeFlagBytes'}

# Crystal's guide (GuideCrystal.kt's bosses and tips over GuideCrystalGen.kt's flags and teams): the guide file's
# milestones by the title's first words, and the places PokeDaisy names that aren't a landmark's name in the game
GUIDE_KINDS = [('LEADER ', 'gym', 'Gym', 'Leader'), ('ELITE FOUR ', 'elite4', 'Elite Four', 'Elite Four'),
               ('CHAMPION ', 'champion', 'Champion', 'Champion'), ('', 'postgame', None, None)]
PLACE_ALIAS = {'POKéMON LEAGUE': 'INDIGO PLATEAU', 'MT. SILVER': 'SILVER CAVE'}


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
    """A list's items at the top level (with angles, < > nest too: a field's type, Map<Int, Int>); a string's commas
    and brackets are its own."""
    opens, closes = ('([{<', ')]}>') if angles else ('([{', ')]}')
    out, depth, cur, quoted, escaped = [], 0, [], False, False
    for c in s:
        if quoted:
            quoted = escaped or c != '"'
            escaped = not escaped and c == '\\'
            cur.append(c)
            continue
        if c == '"':
            quoted = True
        elif c in opens:
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


def read_configs(src, cls='Gen2Config', where=GEN2):
    """A config class's fields (name -> (type, default text)) and every 'val X = <cls>(...)' or '= Y.copy(...)'."""
    s = strip_comments(src)
    m = re.search(r'data class %s\(' % cls, s)
    if not m:
        fail('no data class %s in %s' % (cls, where))
    params = s[m.end():closing(s, m.end() - 1)]
    fields = {}
    for p in split_args(params, angles=True):
        pm = re.fullmatch(r'val\s+(\w+)\s*:\s*([^=]+?)\s*(?:=\s*(.+))?', p, re.S)
        if not pm:
            fail('a %s field it can\'t read: %s' % (cls, p))
        fields[pm.group(1)] = (pm.group(2).strip(), pm.group(3))
    configs = {}
    for m in re.finditer(r'\bval\s+(\w+)\s*=\s*(%s|\w+\.copy)\(' % cls, s):
        body = s[m.end():closing(s, m.end() - 1)]
        args = {}
        for a in split_args(body):
            am = re.fullmatch(r'(\w+)\s*=\s*(.+)', a, re.S)
            if not am:
                fail('%s: an argument it can\'t read: %s' % (m.group(1), a))
            args[am.group(1)] = am.group(2).strip()
        base = None if m.group(2) == cls else m.group(2)[:-len('.copy')]
        configs[m.group(1)] = (base, args)
    return fields, configs


def resolve(name, fields, configs):
    """A config's arguments over its base's, over Gen2Config's defaults."""
    if name not in configs:
        fail('no config %s in PokeDaisy\'s readers' % name)
    base, args = configs[name]
    if base:
        out = resolve(base, fields, configs)
    else:
        out = {k: d for k, (t, d) in fields.items()}
    for k, v in args.items():
        if k not in fields:
            fail('%s sets %s, which its config class doesn\'t have' % (name, k))
        out[k] = v
    return out


def addr_value(v):
    """(bank, address) of an evaluated address: a plain one in D000-DFFF is bank 1's, as the cores map it."""
    if isinstance(v, tuple):
        return v[1], v[2]
    return (1 if 0xD000 <= v <= 0xDFFF else 0), v


def config_values(name, fields, configs, skip=None):
    ram, sizes, unread = {}, {}, []
    for k, text in resolve(name, fields, configs).items():
        if k in (SKIP if skip is None else skip):
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


def gen1_dex(name, fields, configs):
    """A Gen1DexTables config's ROM tables (rom key -> (bank, address)), its RAM (ram key -> (bank, address)), its front
    pictures' banks (DEX1_PICS: a number, or picsBankBreaks' list) and what it couldn't read: a field it doesn't know,
    or a default of the English retail games' that the config changes."""
    rom, ram, pics, unread = {}, {}, {}, []
    for k, text in resolve(name, fields, configs).items():
        if k in DEX1_SKIP or (k in DEX1_ROM and text == 'null'):
            continue
        pm = re.fullmatch(r'(0x[0-9A-Fa-f]+|\d+)\s+to\s+(0x[0-9A-Fa-f]+|\d+)', text or '')
        lm = re.fullmatch(r'intArrayOf\(([^)]*)\)', text or '')
        v = evaluate(text or '')
        if k in DEX1_ROM and pm:
            rom[DEX1_ROM[k]] = (int(pm.group(1), 0), int(pm.group(2), 0))
        elif k in DEX1_RAM and isinstance(v, int) and not isinstance(v, bool):
            ram[DEX1_RAM[k]] = addr_value(v)
        elif k in DEX1_PICS and isinstance(v, int) and not isinstance(v, bool):
            pics[k] = v
        elif k in DEX1_PICS and lm and all(isinstance(evaluate(x), int) for x in split_args(lm.group(1))):
            pics[k] = [evaluate(x) for x in split_args(lm.group(1))]
        elif k in DEX1_DEFAULT and text == fields[k][1]:
            continue
        else:
            unread.append('%s.%s = %s' % (name, k, text))
    unread += ['%s.%s (no value)' % (name, k) for k in DEX1_PICS if k not in pics]
    return rom, ram, pics, unread


def read_art(src, name, cls='Gen2ArtTables', where=GEN2_ART):
    """A Gen2ArtTables (or cls) config's fields: name -> (bank, address), a list of them, or the text it couldn't read. A
    config made from another (X.let { Gen2ArtTables(...) }) is read by position, its 'it.field' the other's field; one
    made by a function (val X = f(...), fun f(...) = Gen1ArtTables(...)) is the function's arguments."""
    s = strip_comments(src)
    m = re.search(r'class %s\(' % cls, s)
    if not m:
        fail('no class %s in %s' % (cls, where))
    order = []
    for p in split_args(s[m.end():closing(s, m.end() - 1)], angles=True):
        pm = re.match(r'val\s+(\w+)\s*:', p)
        if not pm:
            fail('a %s field it can\'t read: %s' % (cls, p))
        order.append(pm.group(1))

    def value(v):
        v = v.strip()
        pm = re.fullmatch(r'(0x[0-9A-Fa-f]+|\d+)\s+to\s+(0x[0-9A-Fa-f]+|\d+)', v)
        if pm:
            return int(pm.group(1), 0), int(pm.group(2), 0)
        lm = re.fullmatch(r'listOf\((.*)\)', v, re.S)
        if lm:
            items = [value(x) for x in split_args(lm.group(1))]
            return items if all(isinstance(x, tuple) for x in items) else v
        return v

    m = re.search(r'\bval\s+%s\s*=\s*%s\(' % (name, cls), s)
    fm = re.search(r'\bval\s+%s\s*=\s*(\w+)\(' % name, s)
    if not m and fm:
        m = re.search(r'\bfun\s+%s\([^)]*\)\s*=\s*%s\(' % (fm.group(1), cls), s)
    if m:
        out = {}
        for a in split_args(s[m.end():closing(s, m.end() - 1)]):
            am = re.fullmatch(r'(\w+)\s*=\s*(.+)', a, re.S)
            if not am:
                fail('%s: an argument it can\'t read: %s' % (name, a))
            out[am.group(1)] = value(am.group(2))
        return out
    m = re.search(r'\bval\s+%s\s*=\s*(\w+)\.let\s*\{\s*%s\(' % (name, cls), s)
    if not m:
        fail('no %s = %s(...) in %s' % (name, cls, where))
    base = read_art(src, m.group(1), cls, where)
    out = {}
    for i, a in enumerate(split_args(s[m.end():closing(s, m.end() - 1)])):
        if i >= len(order):
            fail('%s: more arguments than %s has' % (name, cls))
        im = re.fullmatch(r'it\.(\w+)', a.strip())
        if im and im.group(1) != order[i]:
            fail('%s: %s is given %s' % (name, order[i], a))
        out[order[i]] = base.get(order[i], a) if im else value(a)
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


# --- Crystal's guide ---

def kotlin_string(expr):
    """A string made of literals joined by +, as its text; None for anything else."""
    lit = r'"((?:[^"\\$]|\\.)*)"'
    parts = re.findall(lit, expr)
    if not parts or re.sub(lit, '', expr).replace('+', '').strip():
        return None
    return ''.join(re.sub(r'\\(.)', r'\1', p) for p in parts)


def braced(s, start):
    """The { ... } that starts at or after s[start]."""
    i = s.index('{', start)
    return s[i:closing(s, i) + 1]


def flag_rule(lam, consts, params=None):
    """A rule over the save's flags, { p -> p.flag(A) || p.flag(B) } or with &&: ('any' or 'all', [flag numbers]);
    None for another shape."""
    m = re.fullmatch(r'\{\s*p\s*->\s*(.*?)\s*\}', lam.strip(), re.S)
    if not m:
        return None
    for op, kind in (('||', 'any'), ('&&', 'all')):
        terms = [t.strip() for t in m.group(1).split(op)]
        if len(terms) < 2:
            continue
        out = []
        for t in terms:
            tm = re.fullmatch(r'p\.flag\((\w+)\)', t)
            n = tm and (params or {}).get(tm.group(1), consts.get(tm.group(1)))
            if not isinstance(n, int):
                return None
            out.append(n)
        return kind, out
    return None


def name_case(s):
    """A name as the companion writes it: "LT. SURGE" Lt. Surge, "POKéMON LEAGUE" Pokémon League."""
    return ' '.join(w[:1].upper() + w[1:].lower() for w in s.split())


def slug(s):
    return re.sub(r'[^a-z0-9]+', '-', s.lower().replace('é', 'e')).strip('-')


def crystal_guide(rules_src, gen_src, first_derived):
    """The guide file of Crystal (nds-pokemon-guide, schema 1) and the bits derived from the save's flags its milestones
    read: each boss of GuideCrystal.kt in its order, done by its flag, or by a rule over two (crLeague's, a lambda's)
    as a derived bit from first_derived on; its team's trainer id and top level from GuideCrystalGen.kt; the tips as
    notes. Returns (guide, derived, unread)."""
    consts = guide_consts(gen_src)
    unread = []
    m = re.search(r'\bval CR_BOSSES_RAW\s*:[^=]*=\s*listOf\(\s*"""(.*?)"""', gen_src, re.S)
    if not m:
        fail('no CR_BOSSES_RAW in %s' % CRYSTAL_GUIDE)
    teams = {}
    for line in m.group(1).splitlines():
        if not line.strip():
            continue
        f = line.split('|')
        if len(f) != 5 or not f[1].isdigit() or not f[2].isdigit():
            fail('%s: a boss line it can\'t read: %s' % (CRYSTAL_GUIDE, line))
        teams[f[0]] = (int(f[1]) << 8 | int(f[2]), max(int(x.split()[0]) for x in f[4].split(';')))
    s = strip_comments(rules_src)
    # (the shapes the conversion takes: crBoss is a Boss of its title, place, flag and rule, its team the key's)
    if not re.search(r'fun crBoss\(title: String, where: String, done: Int, key: String, doneIf: \(\(SaveProgress\) -> Boolean\)\? = null\)\s*=\s*'
                     r'Boss\(title, where, done, doneIf = doneIf\)\s*\{\s*CrystalWorld\.trainer\(key\)\s*\}', s):
        fail('%s: crBoss isn\'t Boss(title, where, done, doneIf) with CrystalWorld\'s team' % CRYSTAL_RULES)
    gm = re.search(r'\bval GUIDE_CRYSTAL\s*=\s*GameGuide\(', s)
    gbody = gm and s[gm.end():closing(s, gm.end() - 1)]
    if not gbody or not re.search(r'bosses\s*=\s*BOSSES_CRYSTAL\b', gbody) or not re.search(r'pages\s*=\s*listOf\(CRYSTAL_TIPS\)', gbody):
        fail('%s: GUIDE_CRYSTAL isn\'t BOSSES_CRYSTAL with CRYSTAL_TIPS' % CRYSTAL_RULES)
    lm = re.search(r'fun crLeague\(own: Int\)[^=]*=', s)
    league = lm and braced(s, lm.end())
    bm = re.search(r'\bval BOSSES_CRYSTAL\s*=\s*listOf\(', s)
    if not bm:
        fail('no BOSSES_CRYSTAL in %s' % CRYSTAL_RULES)
    places, milestones, derived, gyms = {}, [], [], 0
    for it in split_args(s[bm.end():closing(s, bm.end() - 1)]):
        if not it.startswith('crBoss('):
            unread.append('BOSSES_CRYSTAL: %s' % it)
            continue
        c = closing(it, len('crBoss'))
        args, rest = split_args(it[len('crBoss('):c]), it[c + 1:].strip()
        title, where, key = (kotlin_string(args[i]) for i in (0, 1, 3)) if len(args) >= 4 else (None, None, None)
        if not title or not where or key not in teams or len(args) > 5:
            unread.append('BOSSES_CRYSTAL: %s' % it)
            continue
        rule = None
        if len(args) == 5:
            cm = re.fullmatch(r'crLeague\((\w+)\)', args[4])
            if cm and league and cm.group(1) in consts:
                rule = flag_rule(league, consts, {'own': consts[cm.group(1)]})
        elif rest:
            rule = flag_rule(rest, consts)
        elif args[2] in consts:
            rule = ('flag', consts[args[2]])
        if not rule or (args[2] != '0' and len(args) + bool(rest) > 4):
            unread.append('BOSSES_CRYSTAL: %s (its rule)' % title)
            continue
        prefix, kind, label, cls = next(k for k in GUIDE_KINDS if title.startswith(k[0]))
        foe = name_case(title[len(prefix):])
        if kind == 'gym':
            where = re.sub(r' GYM$', '', where)
        place_name = name_case(PLACE_ALIAS.get(where, where))
        places[slug(place_name)] = {'name': place_name}
        if kind == 'gym':
            gyms += 1
            mid, name = 'gym-%d-%s' % (gyms, slug(foe)), 'Gym %d: %s' % (gyms, foe)
        elif label:
            mid, name = '%s-%s' % ('e4' if kind == 'elite4' else kind, slug(foe)), '%s: %s' % (label, foe)
        else:
            mid, name = slug(foe), '%s (%s)' % (foe, place_name)
        if rule[0] == 'flag':
            done = 'flag:%d' % rule[1]
        else:
            bit = first_derived + len(derived)
            derived.append({'bit': bit, rule[0]: rule[1], 'for': mid})
            done = 'flag:%d' % bit
        tid, ace = teams[key]
        ms = {'id': mid, 'name': name, 'kind': kind, 'place': slug(place_name), 'level_cap': ace,
              'foe': {'name': foe, 'ace_level': ace}, 'trainer_ids': [tid], 'done_when': done,
              'quality': 'reference-only', 'source': 'PokeDaisy GuideCrystal.kt, GuideCrystalGen.kt (%s)' % key}
        if cls:
            ms['foe']['class'] = cls
        milestones.append(ms)
    notes = []
    tm = re.search(r'\bval CRYSTAL_TIPS\s*=\s*page\(', s)
    if not tm:
        fail('no CRYSTAL_TIPS in %s' % CRYSTAL_RULES)
    tips = s[tm.end():closing(s, tm.end() - 1)]
    for em in re.finditer(r'\bentry\(', tips):
        args = split_args(tips[em.end():closing(tips, em.end() - 1)])
        q, a = (kotlin_string(args[0]), kotlin_string(args[1])) if len(args) >= 2 else (None, None)
        # (an entry's hint, the answer's first words in PokeDaisy's quiz, is left out: the notes show the answer)
        if not q or not a or any(not re.match(r'hint\s*=', x) for x in args[2:]):
            unread.append('CRYSTAL_TIPS: %s' % ', '.join(args))
            continue
        notes.append({'id': 'note-tip-%d' % (len(notes) + 1), 'kind': 'tip', 'title': q, 'text': a,
                      'quality': 'reference-only', 'source': 'PokeDaisy GuideCrystal.kt (CRYSTAL_TIPS)'})
    guide = {
        'schema': 1, 'kind': 'nds-pokemon-guide', 'game': 'crystal', 'code': 'BYTE', 'title': 'Pokémon Crystal',
        'revision': 1,
        'sources': ['PokeDaisy by Lidor Itzhari, %s (GPL-3.0): GuideCrystal.kt\'s bosses, their order and rules, and '
                    'its tips; GuideCrystalGen.kt\'s flags and teams, generated there from pret/pokecrystal' % PIN[:8],
                    'The player\'s ROM: the trainers\' teams, the places\' maps and the hidden items, read when the '
                    'game starts (kiwami/GB-POKEMON.md)'],
        'places': places, 'milestones': milestones, 'notes': notes,
    }
    return guide, derived, unread


# --- The data files ---

def build(checkout):
    inputs = {}

    def read(rel):
        data = open(os.path.join(checkout, rel), 'rb').read()
        inputs['pokedaisy ' + rel] = pe.sha256(data)
        return data.decode('utf-8')

    fields, configs = read_configs(read(GEN2))
    fields1, configs1 = read_configs(read(GEN1), 'Gen1Config', GEN1)
    dex1_fields, dex1_configs = read_configs(read(GEN1_DEX), 'Gen1DexTables', GEN1_DEX)
    dex_src = read(GEN2_DEX)
    art_src = read(GEN2_ART)
    art1_src = read(GEN1_ART)
    guide_src = read(CRYSTAL_GUIDE)
    guide = guide_consts(guide_src)
    rules_src = read(CRYSTAL_RULES)
    titles = retail_sha1s(read(TITLES))
    extra_text = open(os.path.join(REPO, EXTRA), encoding='utf-8').read()
    inputs[EXTRA] = pe.sha256(extra_text.encode('utf-8'))
    extra = json.loads(extra_text)
    files, unread = {}, []
    for key, gen, title, cfg, dex_cfg, art_cfg, pd_title in GAMES:
        if gen == 1:
            ram, sizes, u = config_values(cfg, fields1, configs1, GEN1_SKIP)
            if resolve(cfg, fields1, configs1).get('language') != "'E'":
                unread.append('%s.language isn\'t English\'s' % cfg)
        else:
            ram, sizes, u = config_values(cfg, fields, configs)
        unread += u
        rom, rom_src, pics = {}, {}, {}
        if gen == 1:
            dex_rom, dex_ram, pics, u = gen1_dex(dex_cfg, dex1_fields, dex1_configs)
            unread += u
            rom.update(dex_rom)
            rom_src.update({k: 'pokedaisy' for k in dex_rom})
            ram.update(dex_ram)
        dex = read_dex(dex_src, dex_cfg) if gen == 2 else {}
        for k, v in dex.items():
            if (dex_cfg, k) in DEX_UNUSED or k in DEX_SKIP:
                continue
            if k in DEX_ROM and isinstance(v, tuple):
                rom[DEX_ROM[k]], rom_src[DEX_ROM[k]] = v, 'pokedaisy'
            elif k in DEX_SIZES and isinstance(v, int) and not isinstance(v, bool):
                sizes[DEX_SIZES[k]] = v
            else:
                unread.append('%s.%s = %s' % (dex_cfg, k, v))
        art = read_art(art_src, art_cfg) if gen == 2 else read_art(art1_src, art_cfg, 'Gen1ArtTables', GEN1_ART)
        art_rom, art_skip = (ART_ROM, ART_SKIP) if gen == 2 else (ART1_ROM, ART1_SKIP)
        for k, v in art.items():
            if k in art_skip:
                continue
            if k in art_rom and isinstance(v, tuple):
                rom[art_rom[k]], rom_src[art_rom[k]] = v, 'pokedaisy'
            elif k in ART_LISTS and isinstance(v, list) and len(v) == len(ART_LISTS[k]):
                for rk, rv in zip(ART_LISTS[k], v):
                    rom[rk], rom_src[rk] = rv, 'pokedaisy'
            else:
                unread.append('%s.%s = %s' % (art_cfg, k, v))
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
        # (Gen I's front pictures: PokeDaisy's first bank and the internal indexes where the next begins, as
        # UncompressMonSprite counts them, and Red's and Blue's Mew's bank, against pret's sections and constants)
        if pics:
            c, r = g.get('const', {}), g.get('rom', {})
            want = {'picsBank': r['pics1']['bank'],
                    'picsBankBreaks': [c['picsTangela']['value'] + 1, c['picsMoltres']['value'] + 1,
                                       c['picsBeedrill']['value'] + 2, c['picsStarmie']['value'] + 1],
                    'mewPicsBank': r['mewPicFront']['bank'] if 'mewPicFront' in r else 0}
            for k, v in want.items():
                if pics[k] != v:
                    fail('%s: PokeDaisy\'s %s is %s, pret\'s %s' % (key, k, pics[k], v))
            if [r['pics%d' % n]['bank'] for n in range(1, 6)] != [pics['picsBank'] + n for n in range(5)]:
                fail('%s: pret\'s "Pics 1" .. "Pics 5" aren\'t the banks from PokeDaisy\'s picsBank on' % key)
        if 'dexEntryFirst' in rom and dex.get('dexEntryFirstBank') != rom['dexEntryFirst'][0]:
            fail('%s: PokeDaisy\'s dexEntryFirstBank %s isn\'t pret\'s first Pokédex entry\'s bank %02X' %
                 (key, dex.get('dexEntryFirstBank'), rom['dexEntryFirst'][0]))
        for k, v in g.get('const', {}).items():
            if k != 'numEvents':
                if k in sizes and sizes[k] != v['value']:
                    fail('%s sizes.%s: PokeDaisy says %d, pret\'s %s %d' % (key, k, sizes[k], v['symbol'], v['value']))
                sizes[k] = v['value']
        if 'numEvents' in g.get('const', {}):
            n = g['const']['numEvents']['value']
            if n % 8:
                fail('%s: NUM_EVENTS %d isn\'t whole bytes' % (key, n))
            if sizes.get('eventFlagBytes', n // 8) != n // 8:
                fail('%s: PokeDaisy has %d bytes of event flags, pret %d' % (key, sizes['eventFlagBytes'], n // 8))
            sizes['eventFlagBytes'] = n // 8
        # (the ROM hacks read as the game they patch aren't PokeDaisy's: gbpk-symbols.txt's hack lines)
        pret_sha1 = {r['sha1'] for r in g['releases'] if not r.get('hack')}
        if titles.get(pd_title, set()) != pret_sha1:
            fail('%s: GameTitles.kt\'s "%s" SHA-1s %s aren\'t pret\'s %s' %
                 (key, pd_title, sorted(titles.get(pd_title, ())), sorted(pret_sha1)))
        releases = [{k: r[k] for k in ('sha1', 'name', 'code', 'hack') if k in r} for r in g['releases']]
        # the flag banks its guide reads (Live::flags: each bank's bits from its first, the derived bits after them)
        flags, derived = [], []
        if key == 'crystal':
            flags = [{'ram': 'eventFlags', 'bytes': sizes['eventFlagBytes'], 'first': 0}]
            gd, derived, u = crystal_guide(rules_src, guide_src, 8 * sizes['eventFlagBytes'])
            unread += u
            files['%s/%s.json' % (FOLDER, key)] = pe.dump(gd).encode('utf-8')
        doc = {
            'schema': 1, 'kind': 'gb-pokemon-ram', 'game': key, 'gen': gen, 'title': title,
            'header_title': g['header_title'], 'releases': releases,
            'ram': {k: {'bank': b, 'addr': pe.hexaddr(a), 'src': src[k]} for k, (b, a) in ram.items()},
            'sizes': sizes, 'flags': flags,
            'rom': {k: {'bank': b, 'addr': pe.hexaddr(a), 'src': rom_src[k]} for k, (b, a) in rom.items()},
        }
        if derived:
            doc['derived'] = derived
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
    outs = ['%s/%s.ram.json' % (FOLDER, g[0]) for g in GAMES] + [FOLDER + '/crystal.json', FOLDER + '/LICENSE']
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
