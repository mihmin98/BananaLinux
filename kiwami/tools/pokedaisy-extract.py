#!/usr/bin/env python3
# The Game Boy Pokémon companion's data files from PokeDaisy (https://github.com/lidor30/pokedaisy, GPL-3.0 with the
# 7(b) attribution term; kiwami/GB-POKEMON.md): each game's RAM addresses, read from PokeDaisy's Kotlin readers, with
# the releases' SHA-1s from its GameTitles.kt, merged with what pret's disassemblies add (kiwami/tools/gbpk-extra.json,
# written by pret-extract.py), into kiwami/rootfs/usr/share/banana/companions/gb-pokemon/<game>.ram.json.
#
#   pokedaisy-extract.py <pokedaisy checkout> [--any-commit]   write the data files, LICENSE and SOURCES' [pokedaisy]
#   pokedaisy-extract.py <pokedaisy checkout> --reference      write the reference data, kiwami/data/pokedaisy/ (below)
#   pokedaisy-extract.py <pokedaisy checkout> --check          write both to a temporary folder, compare with the repo's
#   pokedaisy-extract.py --check                               without the checkout: both against their SOURCES' hashes
#
# A checkout at another commit than PIN is refused unless --any-commit is given. PokeDaisy's hand-written configs
# (Gen2Config(...) with named arguments, wramx(bank, addr), .copy(...) of another config; Gen2DexTables(...) and
# Gen2ArtTables(...) with bank to address pairs, the latter also by position over another's, X.let { ... it.field };
# Gen1ArtTables(...) also as a function's, val X = f(...) with fun f(...) = Gen1ArtTables(...))
# are read by a small evaluator: a field it can't read is listed under "unread:" and fails the
# run; the fields that are the games' own tables (SKIP, DEX_SKIP) are left out by name. A value both sources give must
# agree. The reference data, every mainline game PokeDaisy supports, is evaluated by a little Kotlin interpreter
# instead (its section below), with Gen III's gamedata/ TOML. Python 3.11 or later (tomllib), standard library only.
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
GEN1_GUIDE = DATA + '/GuideGen1Gen.kt'
GEN1_RULES = DATA + '/GuideGen1.kt'
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

# Red's, Blue's and Yellow's guides (GuideGen1.kt's rules over GuideGen1Gen.kt's tables): each game's names there (its
# constants' prefix, its RAM's and its world's), and the guide file's code (the fork's own: the carts have none,
# ui/src/kiwami/gbpk/guide-games.inc)
GEN1_GUIDES = {'red': ('G1R', 'RED', 'PKRD'), 'blue': ('G1B', 'BLUE', 'PKBL'), 'yellow': ('G1Y', 'YELLOW', 'PKYL')}
# GuideGen1Gen.kt's RAM constants (<prefix>_<name>, Gen I's D000-DFFF the data files' bank 1), by the data file's key,
# and its sizes; what is left out
GEN1_GUIDE_RAM = {'EVENT_FLAGS': 'eventFlags', 'MISSABLE_FLAGS': 'toggleFlags', 'BADGES': 'badges'}
GEN1_GUIDE_SIZES = {'EVENT_FLAG_BYTES': 'eventFlagBytes', 'MISSABLE_FLAG_BYTES': 'toggleFlagBytes'}
GEN1_GUIDE_SKIP = {
    'HIDDEN_ITEM_FLAGS': 'the hidden items are listed whether found or not', 'HIDDEN_ITEM_FLAG_BYTES': 'the same',
    'TRADE_FLAGS': 'the trades are notes, not followed', 'TRADE_FLAG_BYTES': 'the same',
    'TEAM_VAR': 'the Champion\'s teams are the ROM\'s, every one his milestone\'s',
}
# GuideGen1.kt's flag ids past the event flags: a bank each, by the id's 0x10000s (its readGen1Progress)
GEN1_BANKS = {'G1_MISSABLE_FLAG': 0x10000, 'G1_HIDDEN_ITEM_FLAG': 0x20000, 'G1_TRADE_FLAG': 0x30000,
              'G1_BADGE_FLAG': 0x40000}
# the Champion (PokeDaisy's "CHAMPION, YOUR RIVAL"): the class his three teams have in the ROM (pret's TrainerNames,
# RIVAL3), by which the guide's resolver finds them, and who he is in the advice's sentences
GEN1_CHAMPION = ('CHAMPION', 'CHAMPION, YOUR RIVAL', 'Rival3', 'your rival')


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
    """A name as the companion writes it: "LT. SURGE" Lt. Surge, "POKéMON LEAGUE" Pokémon League, a floor as the game
    writes it ("GATE 2F" Gate 2F)."""
    return ' '.join(w if re.fullmatch(r'B?\d+F', w) else w[:1].upper() + w[1:].lower() for w in s.split())


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


# --- Red's, Blue's and Yellow's guides ---

def raw_table(src, name, where):
    """A generated table, val <name>: List<String> = listOf(\"\"\"...\"\"\"), as its lines."""
    m = re.search(r'\bval %s\s*:[^=]*=\s*listOf\(\s*"""(.*?)"""\s*,?\s*\)' % name, src, re.S)
    if not m:
        fail('no %s (one string) in %s' % (name, where))
    return [line for line in m.group(1).splitlines() if line.strip()]


def gen1_rules(rules_src, prefix, world):
    """What GuideGen1.kt does with a game's tables, as the conversion takes it: a boss is done when any of its flags is
    set, the flag ids past the event flags are GEN1_BANKS' banks of the save (the badges 8 bits of wObtainedBadges),
    and the game's guide is its world's over its own constants. Returns what doesn't hold."""
    s = ' '.join(strip_comments(rules_src).split())
    consts = guide_consts(rules_src)
    out = ['%s isn\'t 0x%X' % (k, v) for k, v in GEN1_BANKS.items() if consts.get(k) != v]
    for shape in ('doneIf = { p -> first.done.any { p.flag(it) } }',
                  'SaveProgress( read(ram.eventFlags, ram.eventBytes), ByteArray(0),',
                  'SaveBank(G1_MISSABLE_FLAG, 0, ram.missableBytes * 8) to read(ram.missable, ram.missableBytes)',
                  'SaveBank(G1_BADGE_FLAG, 0, 8) to read(ram.badges, 1)',
                  'private val %s_GUIDE_RAM = Gen1GuideRam( %s_EVENT_FLAGS, %s_EVENT_FLAG_BYTES, %s_MISSABLE_FLAGS, '
                  '%s_MISSABLE_FLAG_BYTES,' % ((world,) + (prefix,) * 4),
                  ' %s_BADGES, %s_TEAM_VAR, )' % (prefix, prefix),
                  'cfg === GEN1_%s -> %s_GUIDE_RAM' % (world, world),
                  'internal val %s_WORLD = Gen1World(%s_WILD_RAW, %s_BOSSES_RAW)' % (world, prefix, prefix),
                  'internal val GUIDE_%s = gen1Guide(%s_WORLD)' % (world, world)):
        if shape not in s:
            out.append('no "%s"' % shape)
    return out


def gen1_guide(gen_src, key, prefix, code, title, sizes, cave_guy, first_derived):
    """The guide file of Red, Blue or Yellow (nds-pokemon-guide, schema 1) and the bits derived from the save's flags its
    milestones read: each boss of GuideGen1Gen.kt's <prefix>_BOSSES_RAW in its order (a key's lines one boss, its
    teams the Champion's by his starter), a gym done by its badge, the others by a derived bit when any of their flags
    is set (the event flags from bit 0, the toggleable objects' after them); the in-game trades as the Pokédex's notes
    on the Pokémon given, by their place. The item balls, gifts and key items of <prefix>_AREAS_RAW are left out, and
    its hidden items are the ROM's. Returns (guide, derived, unread)."""
    unread, groups = [], {}
    events, toggles = 8 * sizes['eventFlagBytes'], 8 * sizes['toggleFlagBytes']
    for line in raw_table(gen_src, prefix + '_BOSSES_RAW', GEN1_GUIDE):
        f = line.split('|')
        teams = [t.split() for t in f[5].split(';')] if len(f) == 6 else []
        if not teams or not all(len(t) >= 3 and t[0].isdigit() for t in teams):
            unread.append('%s_BOSSES_RAW: %s' % (prefix, line))
            continue
        groups.setdefault(f[0], []).append((f[1], f[2], f[3], f[4], max(int(t[0]) for t in teams)))
    places, milestones, derived, gyms = {}, [], [], 0
    for bkey, lines in groups.items():
        title_, where, done, _, ace = lines[0]
        # (one boss a key: its lines differ only by the team, which the Champion's var picks; every team's top level
        # the same, which the resolver finds them by)
        if any(x[:3] != lines[0][:3] or x[4] != ace for x in lines) or \
                (len(lines) > 1) != all(re.fullmatch(r'0x10000=\d+', x[3]) for x in lines):
            unread.append('%s_BOSSES_RAW: %s (its lines)' % (prefix, bkey))
            continue
        flags = [int(x, 0) for x in done.split('/') if x]
        leader = None
        if title_.startswith('LEADER ') and where.endswith(' GYM'):
            kind, foe, where = 'gym', name_case(title_[len('LEADER '):]), where[:-len(' GYM')]
        elif title_.startswith('ELITE FOUR '):
            kind, foe = 'elite4', name_case(title_[len('ELITE FOUR '):])
        elif (bkey, title_) == GEN1_CHAMPION[:2]:
            kind, foe, leader = 'champion', GEN1_CHAMPION[2], GEN1_CHAMPION[3]
        else:
            unread.append('%s_BOSSES_RAW: %s (its kind)' % (prefix, bkey))
            continue
        place_name = name_case(where)
        places[slug(place_name)] = {'name': place_name}
        # done: a gym by its badge (the badges' bank, bit n: badge:n + 1); the others by any of their flags
        bits = []
        for n in flags:
            if n < events:
                bits.append(n)
            elif GEN1_BANKS['G1_MISSABLE_FLAG'] <= n < GEN1_BANKS['G1_MISSABLE_FLAG'] + toggles:
                if n - GEN1_BANKS['G1_MISSABLE_FLAG'] != cave_guy:
                    fail('%s: %s is done by toggleable object %d, not pret\'s TOGGLE_CERULEAN_CAVE_GUY %d' %
                         (key, bkey, n - GEN1_BANKS['G1_MISSABLE_FLAG'], cave_guy))
                bits.append(events + n - GEN1_BANKS['G1_MISSABLE_FLAG'])
            else:
                bits = None
                break
        if kind == 'gym':
            gyms += 1
            if flags != [GEN1_BANKS['G1_BADGE_FLAG'] + gyms - 1]:
                unread.append('%s_BOSSES_RAW: %s (not its badge, %d)' % (prefix, bkey, gyms))
                continue
            mid, name, done_when = 'gym-%d-%s' % (gyms, slug(foe)), 'Gym %d: %s' % (gyms, foe), 'badge:%d' % gyms
        else:
            if not bits:
                unread.append('%s_BOSSES_RAW: %s (its flags)' % (prefix, bkey))
                continue
            label = 'Elite Four' if kind == 'elite4' else 'Champion'
            mid = '%s-%s' % ('e4' if kind == 'elite4' else kind, slug(leader or foe))
            name = '%s: %s' % (label, leader or foe)
            if len(bits) == 1:
                done_when = 'flag:%d' % bits[0]
            else:
                bit = first_derived + len(derived)
                derived.append({'bit': bit, 'any': bits, 'for': mid})
                done_when = 'flag:%d' % bit
        ms = {'id': mid, 'name': name, 'kind': kind, 'place': slug(place_name), 'level_cap': ace,
              'foe': {'name': foe, 'ace_level': ace}, 'done_when': done_when, 'quality': 'reference-only',
              'source': 'PokeDaisy GuideGen1.kt, GuideGen1Gen.kt (%s_BOSSES_RAW %s)' % (prefix, bkey)}
        if leader:
            ms['leader'] = leader
        milestones.append(ms)
    # the in-game trades: map|TRADE|the Pokémon given|its flag|count|the Pokémon wanted|the place
    evolutions = []
    for line in raw_table(gen_src, prefix + '_AREAS_RAW', GEN1_GUIDE):
        f = line.split('|')
        if len(f) < 2 or f[1] != 'TRADE':
            continue
        if len(f) != 7 or not f[2].isdigit() or not 1 <= int(f[2]) <= 151 or not f[6].strip():
            unread.append('%s_AREAS_RAW: %s' % (prefix, line))
            continue
        evolutions.append({'species': int(f[2]), 'text': 'From an in-game trade in the %s.' % name_case(f[6]),
                           'quality': 'reference-only', 'source': 'PokeDaisy GuideGen1Gen.kt (%s_AREAS_RAW)' % prefix})
    guide = {
        'schema': 1, 'kind': 'nds-pokemon-guide', 'game': key, 'code': code, 'title': title, 'revision': 1,
        'sources': ['PokeDaisy by Lidor Itzhari, %s (GPL-3.0): GuideGen1.kt\'s rules; GuideGen1Gen.kt\'s bosses, their '
                    'order and flags, and the in-game trades, generated there from pret/pokered and pret/pokeyellow'
                    % PIN[:8],
                    'The player\'s ROM: the trainers\' teams, the places\' maps and the hidden items, read when the '
                    'game starts (kiwami/GB-POKEMON.md)'],
        'places': places, 'milestones': milestones, 'evolutions': evolutions,
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
    gen1_guide_src = read(GEN1_GUIDE)
    gen1_guide_consts = guide_consts(gen1_guide_src)
    gen1_rules_src = read(GEN1_RULES)
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
        if key in GEN1_GUIDES:
            prefix = GEN1_GUIDES[key][0]
            for c, k in GEN1_GUIDE_RAM.items():
                if prefix + '_' + c not in gen1_guide_consts:
                    unread.append('%s: no %s_%s' % (GEN1_GUIDE, prefix, c))
                    continue
                v = addr_value(gen1_guide_consts[prefix + '_' + c])
                if k in ram and ram[k] != v:
                    fail('%s ram.%s: %s says %02X:%04X, %s_%s %02X:%04X' % (key, k, GEN1, ram[k][0], ram[k][1], prefix, c, v[0], v[1]))
                ram[k], src[k] = v, 'pokedaisy'
            for c, k in GEN1_GUIDE_SIZES.items():
                if prefix + '_' + c not in gen1_guide_consts:
                    unread.append('%s: no %s_%s' % (GEN1_GUIDE, prefix, c))
                    continue
                sizes[k] = gen1_guide_consts[prefix + '_' + c]
            for c in gen1_guide_consts:
                if c.startswith(prefix + '_') and c[len(prefix) + 1:] not in {**GEN1_GUIDE_RAM, **GEN1_GUIDE_SIZES, **GEN1_GUIDE_SKIP}:
                    unread.append('%s: %s' % (GEN1_GUIDE, c))
            unread += ['%s: %s' % (GEN1_RULES, u) for u in gen1_rules(gen1_rules_src, prefix, GEN1_GUIDES[key][1])]
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
        elif key in GEN1_GUIDES:
            ev = sizes['eventFlagBytes']
            flags = [{'ram': 'eventFlags', 'bytes': ev, 'first': 0},
                     {'ram': 'toggleFlags', 'bytes': sizes['toggleFlagBytes'], 'first': 8 * ev}]
            gd, derived, u = gen1_guide(gen1_guide_src, key, prefix, GEN1_GUIDES[key][2], pd_title, sizes,
                                        sizes['toggleCeruleanCaveGuy'], 8 * (ev + sizes['toggleFlagBytes']))
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


# --- The reference data (--reference): a little Kotlin interpreter ---
#
# kiwami/data/pokedaisy/ holds, for every mainline game PokeDaisy supports (English releases), the facts its sources
# give: the reader's configs (RAM), the ROM tables' records, the guides' bosses and areas by ids and flags, and
# PokeDaisy's own words. Its configs and guides are Kotlin values built by constructors, .copy(), helper functions with
# defaults, if/else on a parameter, string templates and "...".format(); they are evaluated here rather than matched
# by patterns. What can't be evaluated (code run on the save, a when, a name in a file not read) is a Bad value, which
# the output leaves out and README.md lists under "Not extracted", as it does what the content rules leave out.

class Unread(Exception):
    pass


class Bad:
    """A value that couldn't be worked out: kept in its place so the rest of a record still reads."""
    def __init__(self, reason):
        self.reason = reason


class Hex(int):
    """An int written in hex in the source (or a TOML long): written back as "0x..."."""
    def _wrap(self, v, other):
        return Hex(v) if isinstance(v, int) and not isinstance(v, bool) else v

    def __add__(self, o): return self._wrap(int(self) + o, o)
    __radd__ = __add__
    def __sub__(self, o): return self._wrap(int(self) - o, o)
    def __rsub__(self, o): return self._wrap(o - int(self), o)
    def __mul__(self, o): return self._wrap(int(self) * o, o)
    __rmul__ = __mul__


class Obj:
    def __init__(self, cls, fields, given=()):
        self.cls, self.fields, self.given = cls, fields, set(given)


class Table:
    """A named collection: config output names it rather than writing it (PokeDaisy's id tables aren't extracted);
    guide output takes its value."""
    def __init__(self, name, get):
        self.name, self._get, self._v = name, get, None

    def value(self):
        if self._v is None:
            self._v = self._get()
        return self._v


class Lambda:
    def __init__(self, params, body, scope):
        self.params, self.body, self.scope = params, body, scope


class Fn:
    def __init__(self, name, params, body, scope):
        self.name, self.params, self.body, self.scope = name, params, body, scope


class Namespace:
    """An object's or an enum's name: its members by name (an enum's entries are "Enum.ENTRY")."""
    def __init__(self, name, members=None):
        self.name, self.members = name, members


TOKEN = re.compile(r'''(?P<ws>[ \t\r]+)|(?P<nl>\n)|(?P<raw>""".*?""")|(?P<str>"(?:[^"\\]|\\.)*")
    |(?P<chr>'(?:[^'\\]|\\.)')
    |(?P<num>0[xX][0-9A-Fa-f_]+[uU]?[lL]?|\d[\d_]*(?:\.\d+)?[fFdDlLuU]*)
    |(?P<id>[A-Za-z_][A-Za-z0-9_]*|`[^`]+`)
    |(?P<op>\?\.|\?:|!!|::|->|===|==|!=|>=|<=|&&|\|\||\.\.|[-+*/%<>=!.,()\[\]{}:;?@])''', re.S | re.X)
INFIX = ('to', 'shl', 'shr', 'ushr', 'and', 'or', 'xor', 'until', 'downTo', 'step')
CONTINUE = ('.', '?.', '?:', '&&', '||')
KEYWORDS = ('if', 'else', 'when', 'val', 'var', 'fun', 'return', 'null', 'true', 'false', 'object', 'is', 'in')


def tokenize(src):
    out, nl = [], False
    for m in TOKEN.finditer(src):
        k = m.lastgroup
        if k == 'ws':
            continue
        if k == 'nl':
            nl = True
            continue
        out.append((k, m.group(), nl))
        nl = False
    out.append(('end', '', True))
    return out


class Parser:
    """Kotlin expressions and statements, as far as PokeDaisy's configs and guides use them, into tuples."""

    def __init__(self, toks, i=0):
        self.t, self.i, self.nl_stack = toks, i, [True]

    def peek(self, n=0):
        return self.t[self.i + n]

    def at(self, v, n=0):
        k, s, _ = self.t[self.i + n]
        return s == v and k in ('op', 'id')

    def take(self, v=None):
        tok = self.t[self.i]
        if v is not None and not (tok[1] == v and tok[0] in ('op', 'id')):
            raise Unread('expected %s, found %s' % (v, tok[1] or 'the end'))
        self.i += 1
        return tok

    def newline(self):
        return self.nl_stack[-1] and self.t[self.i][2]

    def skip_type(self, stops):
        """Over a type (Map<Int, Pair<Int, Int>>?, ((SaveProgress) -> Boolean)?) up to one of stops at depth 0."""
        depth = 0
        while True:
            k, s, _ = self.peek()
            if k == 'end':
                return
            if depth == 0 and s in stops and k == 'op':
                return
            if s in ('<', '(', '['):
                depth += 1
            elif s in ('>', ')', ']'):
                if depth == 0:
                    return
                depth -= 1
            elif s == '->' and depth > 0:
                pass
            self.i += 1

    def skip_generic(self):
        """listOf<Int>(...) and emptyList<Boss>(): the type arguments, when a call follows them."""
        if not self.at('<'):
            return
        j, depth = self.i, 0
        while True:
            k, s, _ = self.t[j]
            if s == '<':
                depth += 1
            elif s == '>':
                depth -= 1
                if depth == 0:
                    break
            elif not (k == 'id' or s in (',', '?', '.', '(', ')', '->', '*')):
                return
            j += 1
        if self.t[j + 1][1] == '(':
            self.i = j + 1

    def params(self):
        self.take('(')
        out = []
        while not self.at(')'):
            star = ''
            while self.at('val') or self.at('var') or self.at('vararg') or self.at('noinline') or self.at('crossinline'):
                star = '*' if self.take()[1] == 'vararg' else star
            name = star + self.take()[1]
            default = None
            if self.at(':'):
                self.take()
                self.skip_type((',', '=', ')'))
            if self.at('='):
                self.take()
                self.nl_stack.append(False)
                default = self.expr()
                self.nl_stack.pop()
            out.append((name, default))
            if self.at(','):
                self.take()
        self.take(')')
        return out

    # statements: a block's lines; a top-level or local declaration's body
    def block(self):
        self.take('{')
        self.nl_stack.append(True)
        body = self.statements('}')
        self.nl_stack.pop()
        self.take('}')
        return body

    def statements(self, end):
        out = []
        while not self.at(end) and self.peek()[0] != 'end':
            if self.at(';'):
                self.take()
                continue
            out.append(self.statement())
        return out

    def statement(self):
        while self.peek()[1] in ('private', 'internal', 'public', 'const', 'inline', 'override', 'operator'):
            self.take()
        if self.at('val') or self.at('var'):
            self.take()
            name = self.take()[1]
            if self.at(':'):
                self.take()
                self.skip_type(('=', 'by'))
            if self.at('by'):
                self.take('by')
                self.take('lazy')
                return ('val', name, ('lazy', self.block()))
            self.take('=')
            return ('val', name, self.expr())
        if self.at('fun'):
            self.take()
            name = self.take()[1]
            ps = self.params()
            if self.at(':'):
                self.take()
                self.skip_type(('=', '{'))
            if self.at('='):
                self.take()
                return ('fun', name, ps, [('return', self.expr())])
            return ('fun', name, ps, self.block())
        if self.at('return'):
            self.take()
            return ('return', self.expr())
        return ('expr', self.expr())

    # expressions, by Kotlin's precedence
    def expr(self):
        return self.binary(0)

    LEVELS = [('||',), ('&&',), ('==', '!=', '==='), ('<', '>', '<=', '>='), ('in', '!in', 'is'), ('?:',), INFIX,
              ('..',), ('+', '-'), ('*', '/', '%')]

    def binary(self, level):
        if level == len(self.LEVELS):
            return self.prefix()
        left = self.binary(level + 1)
        while True:
            k, s, nl = self.peek()
            if s not in self.LEVELS[level] or k not in ('op', 'id'):
                return left
            if nl and self.nl_stack[-1] and s not in CONTINUE:
                return left
            self.take()
            left = ('bin', s, left, self.binary(level + 1))

    def prefix(self):
        if self.at('-') or self.at('!') or self.at('+'):
            op = self.take()[1]
            return ('un', op, self.prefix())
        return self.postfix(self.primary())

    def args(self):
        self.take('(')
        self.nl_stack.append(False)
        out = []
        while not self.at(')'):
            name = None
            if self.peek()[0] == 'id' and self.at('=', 1) and not self.at('==', 1):
                name = self.take()[1]
                self.take('=')
            out.append((name, self.expr()))
            if self.at(','):
                self.take()
        self.nl_stack.pop()
        self.take(')')
        return out

    def postfix(self, e):
        while True:
            k, s, nl = self.peek()
            if s == '(' and k == 'op' and not nl:
                args = self.args()
                lam = self.lambda_() if self.at('{') and not self.peek()[2] else None
                e = ('call', e, args, lam)
            elif s == '{' and k == 'op' and not nl and e[0] in ('id', 'get'):
                e = ('call', e, [], self.lambda_())
            elif s in ('.', '?.') and k == 'op':
                self.take()
                name = self.take()[1]
                self.skip_generic()
                e = ('get', e, name)
            elif s == '!!' and k == 'op':
                self.take()
            elif s == '[' and k == 'op' and not nl:
                self.take()
                self.nl_stack.append(False)
                ix = self.expr()
                self.nl_stack.pop()
                self.take(']')
                e = ('idx', e, ix)
            elif s == '::' and k == 'op':
                self.take()
                e = ('bad', 'a function reference (::%s)' % self.take()[1])
            else:
                return e

    def lambda_(self):
        self.take('{')
        j, depth, params = self.i, 0, None
        while self.t[j][0] != 'end':
            s = self.t[j][1]
            if s in ('{', '(', '['):
                depth += 1
            elif s in ('}', ')', ']'):
                if depth == 0:
                    break
                depth -= 1
            elif s == '->' and depth == 0:
                params = [t[1] for t in self.t[self.i:j] if t[0] == 'id' and t[1] != '_']
                self.i = j + 1
                break
            elif depth == 0 and not (self.t[j][0] == 'id' or s in (',', ':', '<', '>', '?', '.')):
                break
            j += 1
        self.nl_stack.append(True)
        body = self.statements('}')
        self.nl_stack.pop()
        self.take('}')
        return ('lambda', params, body)

    def primary(self):
        k, s, _ = self.take()
        if k == 'num':
            t = s.replace('_', '').rstrip('lLuUfFdD') if not s.lower().startswith('0x') else s.replace('_', '').rstrip('lLuU')
            if t.lower().startswith('0x'):
                return ('num', Hex(int(t, 16)))
            return ('num', float(t) if '.' in t else int(t))
        if k == 'str':
            return ('str', s[1:-1], False)
        if k == 'raw':
            return ('str', s[3:-3], True)
        if k == 'chr':
            return ('num', s[1:-1].encode().decode('unicode_escape'))
        if s == '(' and k == 'op':
            self.nl_stack.append(False)
            e = self.expr()
            self.nl_stack.pop()
            self.take(')')
            return e
        if s == '{' and k == 'op':
            self.i -= 1
            return self.lambda_()
        if k == 'id':
            if s == 'true' or s == 'false':
                return ('num', s == 'true')
            if s == 'null':
                return ('num', None)
            if s == 'if':
                self.take('(')
                self.nl_stack.append(False)
                c = self.expr()
                self.nl_stack.pop()
                self.take(')')
                a = ('block', self.block()) if self.at('{') else self.expr()
                b = ('num', None)
                save = self.i
                if self.at('else') or (self.peek()[2] and self.at('else')):
                    self.take()
                    b = ('block', self.block()) if self.at('{') else self.expr()
                else:
                    self.i = save
                return ('if', c, a, b)
            if s in ('when', 'object', 'try'):
                depth = 0
                while True:
                    k2, s2, _ = self.take()
                    if k2 == 'end':
                        break
                    if s2 == '{':
                        depth += 1
                    elif s2 == '}':
                        depth -= 1
                        if depth == 0:
                            break
                return ('bad', 'a %s expression' % s)
            self.skip_generic()
            return ('id', s.strip('`'))
        raise Unread('an expression it can\'t read at %r' % s)


def parse_expr(src):
    p = Parser(tokenize(src))
    e = p.expr()
    return e


# --- declarations: what each file defines at its top level and in its objects ---

class Decl:
    def __init__(self, kind, name, toks, start, file, private, ns):
        self.kind, self.name, self.toks, self.start, self.file, self.private, self.ns = kind, name, toks, start, file, private, ns
        self.secondary = []


def scan_decls(toks, i, end, file, ns, out):
    """The declarations between toks[i] and toks[end] at their own depth: vals, funs, classes, objects and enums."""
    depth, private = 0, False
    while i < end:
        k, s, _ = toks[i]
        if s in ('{', '(', '[') and k == 'op':
            depth += 1
        elif s in ('}', ')', ']') and k == 'op':
            depth -= 1
        elif depth == 0 and k == 'id':
            if s in ('private', 'internal'):
                private = private or s == 'private'
            elif s in ('val', 'var', 'fun'):
                j = i + 1
                if toks[j][1] == '<':
                    j = skip_angles(toks, j)
                name = toks[j][1]
                if toks[j + 1][1] == '.':      # an extension (fun Foo.bar): not a value of its own
                    private = False
                else:
                    start = i
                    while toks[start - 1][1] in ('private', 'internal', 'public', 'const', 'inline', 'override') \
                            and start - 1 >= 0:
                        start -= 1
                    out.append(Decl('val' if s != 'fun' else 'fun', name, toks, start, file, private, ns))
                    private = False
            elif s in ('class', 'interface', 'object') and toks[i - 1][1] != '::':
                name = toks[i + 1][1]
                enum = toks[i - 1][1] == 'enum'
                if s == 'class' and toks[i + 2][1] == '(':
                    out.append(Decl('class', name, toks, i + 2, file, private, ns))
                j = i + 2
                if toks[j][1] == '(':
                    j = match(toks, j) + 1
                while toks[j][1] not in ('{', 'end') and not toks[j][2] and toks[j][0] != 'end':
                    if toks[j][1] == '(':
                        j = match(toks, j)
                    j += 1
                if toks[j][1] == '{':
                    close = match(toks, j)
                    if s == 'class' and out and out[-1].kind == 'class' and out[-1].name == name:
                        # its secondary constructors: constructor(params) : this(args)
                        out[-1].secondary = [t for t in range(j + 1, close) if toks[t][1] == 'constructor']
                    if enum:
                        entries = []
                        for t in range(j + 1, close):
                            if toks[t][1] == ';':
                                break
                            if toks[t][0] == 'id' and toks[t - 1][1] in ('{', ','):
                                entries.append(toks[t][1])
                        out.append(Decl('enum', name, entries, 0, file, private, ns))
                    elif s != 'class' and name != '{':     # an object's or an interface's members (a class's are its own)
                        out.append(Decl('object', name, None, 0, file, private, ns))
                        scan_decls(toks, j + 1, close, file, (ns + '.' if ns else '') + name, out)
                    i = close
                private = False
            else:
                if toks[i][2] and s not in ('data', 'enum', 'sealed', 'abstract', 'open', 'const', 'inline', 'companion'):
                    private = False
        i += 1


def skip_angles(toks, j):
    depth = 0
    while True:
        if toks[j][1] == '<':
            depth += 1
        elif toks[j][1] == '>':
            depth -= 1
            if depth == 0:
                return j + 1
        j += 1


def match(toks, j):
    depth = 0
    while toks[j][0] != 'end':
        if toks[j][0] == 'op' and toks[j][1] in ('(', '[', '{'):
            depth += 1
        elif toks[j][0] == 'op' and toks[j][1] in (')', ']', '}'):
            depth -= 1
            if depth == 0:
                return j
        j += 1
    raise Unread('no closing bracket')


class Kotlin:
    """PokeDaisy's files read, their declarations, and the evaluator."""

    def __init__(self, sources):
        self.globals, self.privates, self.classes, self.enums, self.objects = {}, {}, {}, {}, set()
        self.cache, self.busy, self.extern = {}, set(), None
        for file, src in sources.items():
            toks = tokenize(strip_comments(src))
            decls = []
            scan_decls(toks, 0, len(toks) - 1, file, '', decls)
            for d in decls:
                key = (d.ns + '.' if d.ns else '') + d.name
                if d.kind == 'class':
                    self.classes[d.name] = d
                elif d.kind == 'enum':
                    self.enums[d.name] = d.toks
                elif d.kind == 'object':
                    self.objects.add(key)
                elif d.private and not d.ns:
                    self.privates.setdefault(file, {})[d.name] = d
                else:
                    self.globals[key] = d

    def top(self, name):
        """A value the build names: a top-level or object member's (Gen1Art.RED), or one file's private one."""
        d = self.globals.get(name)
        if not d:
            found = [ps[name] for ps in self.privates.values() if name in ps]
            if len(found) != 1:
                raise Unread('%s isn\'t in the files read%s' % (name, ' (in several)' if found else ''))
            d = found[0]
        return self.decl_value(d)

    # values of declarations
    def decl_value(self, d):
        key = (d.file, d.ns, d.name)
        if key in self.cache:
            return self.cache[key]
        if key in self.busy:
            raise Unread('%s refers to itself' % d.name)
        self.busy.add(key)
        try:
            p = Parser(d.toks, d.start)
            st = p.statement()
            scope = {'__file__': d.file, '__ns__': d.ns}
            if st[0] == 'fun':
                v = Fn(st[1], st[2], st[3], scope)
            else:
                e = st[2]
                if e[0] == 'call' and e[1][0] == 'id' and e[1][1] in COLLECTIONS or e[0] == 'lazy':
                    v = Table(d.name, lambda e=e, scope=scope: self.eval(e, scope))
                else:
                    v = self.eval(e, scope)
        except Unread as u:
            v = Bad('%s: %s' % (d.name, u))
        finally:
            self.busy.discard(key)
        self.cache[key] = v
        return v

    def lookup(self, name, scope):
        s = scope
        while s is not None:
            if name in s:
                return s[name]
            s = s.get('__parent__')
        file, ns = scope_get(scope, '__file__'), scope_get(scope, '__ns__')
        while ns:
            d = self.globals.get(ns + '.' + name)
            if d:
                return self.decl_value(d)
            ns = ns.rpartition('.')[0]
        d = self.privates.get(file, {}).get(name) or self.globals.get(name)
        if d:
            return self.decl_value(d)
        if name in self.classes:
            return ('class', name)
        if name in self.enums:
            return Namespace(name)
        if name in self.objects:
            return Namespace(name, name)
        if name in BUILTINS:
            return ('builtin', name)
        if self.extern:
            v = self.extern(name)
            if v is not None:
                return v
        raise Unread('%s isn\'t in the files read' % name)

    def eval(self, e, scope):
        k = e[0]
        if k == 'num':
            return e[1]
        if k == 'str':
            return self.string(e[1], e[2], scope)
        if k == 'id':
            return self.lookup(e[1], scope)
        if k == 'bad':
            raise Unread(e[1])
        if k == 'lambda':
            return Lambda(e[1], e[2], scope)
        if k == 'block':
            return self.run(e[1], {'__parent__': scope})
        if k == 'lazy':
            return self.run(e[1], {'__parent__': scope})
        if k == 'if':
            c = self.eval(e[1], scope)
            if not isinstance(c, bool):
                raise Unread('a condition that depends on the save or the game')
            return self.eval(e[2] if c else e[3], scope)
        if k == 'un':
            v = self.eval(e[2], scope)
            check(v)
            return (not v) if e[1] == '!' else -v if e[1] == '-' else v
        if k == 'bin':
            return self.binop(e[1], e[2], e[3], scope)
        if k == 'get':
            return self.get(self.eval(e[1], scope), e[2])
        if k == 'idx':
            v, ix = self.eval(e[1], scope), self.eval(e[2], scope)
            v = v.value() if isinstance(v, Table) else v
            check(v)
            return v[ix]
        if k == 'call':
            return self.call(e, scope)
        raise Unread('a %s it can\'t evaluate' % k)

    def string(self, text, raw, scope):
        out, i = [], 0
        while i < len(text):
            c = text[i]
            if c == '\\' and not raw:
                n = text[i + 1]
                if n == 'u':
                    out.append(chr(int(text[i + 2:i + 6], 16)))
                    i += 6
                    continue
                out.append({'n': '\n', 't': '\t', 'r': '\r', 'b': '\b'}.get(n, n))
                i += 2
            elif c == '$' and i + 1 < len(text) and text[i + 1] == '{':
                j = i + 2
                depth = 1
                while depth:
                    depth += {'{': 1, '}': -1}.get(text[j], 0)
                    j += 1
                out.append(text_of(self.eval(parse_expr(text[i + 2:j - 1]), scope)))
                i = j
            elif c == '$' and i + 1 < len(text) and (text[i + 1].isalpha() or text[i + 1] == '_'):
                m = re.match(r'[A-Za-z_]\w*', text[i + 1:])
                out.append(text_of(self.lookup(m.group(), scope)))
                i += 1 + m.end()
            else:
                out.append(c)
                i += 1
        return ''.join(out)

    def binop(self, op, a, b, scope):
        if op == '&&' or op == '||':
            x = self.eval(a, scope)
            check(x)
            if not isinstance(x, bool):
                raise Unread('a condition that depends on the save')
            if (op == '&&' and not x) or (op == '||' and x):
                return x
            return self.eval(b, scope)
        x = self.eval(a, scope)
        if op == '?:':
            return self.eval(b, scope) if x is None else x
        y = self.eval(b, scope)
        if op == 'to':
            return (x, y)
        check(x)
        check(y)
        if op == '+' and (isinstance(x, str) or isinstance(y, str)):
            return text_of(x) + text_of(y)
        if op == '==':
            return x == y
        if op == '!=':
            return x != y
        if not all(isinstance(v, (int, float)) for v in (x, y)):
            raise Unread('%s on what isn\'t a number' % op)
        if op == '/':
            return Hex(int(x / y)) if isinstance(x, Hex) else int(x / y) if isinstance(x, int) and isinstance(y, int) else x / y
        f = {'+': lambda: x + y, '-': lambda: x - y, '*': lambda: x * y, '%': lambda: x % y,
             'shl': lambda: x << y, 'shr': lambda: x >> y, 'and': lambda: x & y, 'or': lambda: x | y,
             'xor': lambda: x ^ y, '<': lambda: x < y, '>': lambda: x > y, '<=': lambda: x <= y, '>=': lambda: x >= y,
             '..': lambda: list(range(x, y + 1)), 'until': lambda: list(range(x, y))}.get(op)
        if not f:
            raise Unread('the operator %s' % op)
        v = f()
        return Hex(v) if isinstance(v, int) and not isinstance(v, bool) and (isinstance(x, Hex) or isinstance(y, Hex)) else v

    def get(self, v, name):
        if isinstance(v, Namespace):
            if v.members is None:
                return '%s.%s' % (v.name, name)
            d = self.globals.get(v.members + '.' + name)
            if d:
                return self.decl_value(d)
            if name in self.classes:
                return ('class', name)
            raise Unread('%s.%s isn\'t in the files read' % (v.name, name))
        if isinstance(v, tuple) and v and v[0] == 'class':
            if name in self.classes:
                return ('class', name)
            return '%s.%s' % (v[1], name)
        check(v)
        if isinstance(v, Table):
            v = v.value()
        if isinstance(v, Obj) and name in v.fields:
            f = v.fields[name]
            check(f)
            return f
        if name in ('size',) and isinstance(v, (list, str, dict)):
            return len(v)
        if name in ('first', 'second') and isinstance(v, tuple):
            return v[0 if name == 'first' else 1]
        return ('method', v, name)

    def call(self, e, scope):
        _, callee, args, lam = e
        f = self.eval(callee, scope)
        if lam is not None:
            args = args + [('__trailing__', lam)]
        vals = []
        for name, a in args:
            try:
                vals.append((name, self.eval(a, scope)))
            except Unread as u:
                vals.append((name, Bad(str(u))))
        return self.apply(f, vals)

    def apply(self, f, vals):
        if isinstance(f, Fn) or isinstance(f, Lambda):
            return self.invoke(f, vals)
        if isinstance(f, tuple) and f[0] == 'class':
            return self.construct(f[1], vals)
        if isinstance(f, tuple) and f[0] == 'builtin':
            return BUILTINS[f[1]](self, [v for n, v in vals if n in (None, '__trailing__')], dict((n, v) for n, v in vals if n and n != '__trailing__'))
        if isinstance(f, tuple) and f[0] == 'method':
            return self.method(f[1], f[2], vals)
        check(f)
        raise Unread('a call on what isn\'t a function')

    def invoke(self, f, vals):
        params = f.params if isinstance(f, Fn) else [(p, None) for p in (f.params if f.params is not None else ['it'])]
        local = {'__parent__': f.scope}
        bind(params, vals, local, lambda d: self.eval(d, local), getattr(f, 'name', 'a lambda'))
        return self.run(f.body, local)

    def run(self, body, local):
        result = None
        for st in body:
            if st[0] == 'val':
                try:
                    local[st[1]] = self.eval(st[2], local)
                except Unread as u:
                    local[st[1]] = Bad(str(u))
            elif st[0] == 'fun':
                local[st[1]] = Fn(st[1], st[2], st[3], local)
            elif st[0] == 'return':
                return self.eval(st[1], local)
            else:
                result = self.eval(st[1], local)
        return result

    def construct(self, cls, vals):
        d = self.classes[cls]
        params = Parser(d.toks, d.start).params()
        scope = {'__file__': d.file, '__ns__': d.ns}
        fields = {}
        try:
            given = bind(params, vals, fields, lambda x: self.eval(x, {'__parent__': fields, **scope}), cls)
        except Unread:
            for t in d.secondary:
                p = Parser(d.toks, t + 1)
                local = dict(scope)
                try:
                    bind(p.params(), vals, local, lambda x: self.eval(x, local), cls)
                    p.take(':')
                    p.take('this')
                    args = [(n, self.eval(a, local)) for n, a in p.args()]
                    return self.construct(cls, args)
                except Unread:
                    continue
            raise
        return Obj(cls, fields, given)

    def method(self, v, name, vals):
        pos = [x for n, x in vals if n in (None, '__trailing__')]
        for x in pos:
            check(x)
        if isinstance(v, Obj) and name == 'copy':
            fields = dict(v.fields)
            for n, x in vals:
                if n not in fields:
                    raise Unread('%s.copy sets %s, which it hasn\'t' % (v.cls, n))
                fields[n] = x
            return Obj(v.cls, fields, v.given | {n for n, _ in vals})
        if isinstance(v, Obj) and name == 'trainer' and 'bossesRaw' in v.fields and pos and isinstance(pos[0], str):
            raw = v.fields['bossesRaw']
            for line in raw_lines(raw.value() if isinstance(raw, Table) else raw):
                f = line.split('|')
                if f[0] == pos[0] and len(f) > 1 and f[1].isdigit():
                    return int(f[1])
            raise Unread('no boss %s in its world\'s table' % pos[0])
        if name == 'let' and pos and isinstance(pos[0], Lambda):
            return self.invoke(pos[0], [(None, v)])
        if isinstance(v, str) and name == 'format':
            for x in pos:
                check(x)
            return re.sub(r'%(0?\d*)d', r'%\1d', v) % tuple(int(x) if isinstance(x, int) else x for x in pos)
        if isinstance(v, str) and name == 'trimIndent':
            lines = v.split('\n')
            if lines and not lines[0].strip():
                lines = lines[1:]
            if lines and not lines[-1].strip():
                lines = lines[:-1]
            ind = min((len(l) - len(l.lstrip()) for l in lines if l.strip()), default=0)
            return '\n'.join(l[ind:] for l in lines)
        if name in ('toLong', 'toInt', 'toShort') and isinstance(v, int):
            return v
        if name in ('toList', 'toIntArray', 'toLongArray', 'toSet', 'orEmpty') and isinstance(v, list):
            return v
        if name in ('uppercase', 'lowercase') and isinstance(v, str):
            return v.upper() if name == 'uppercase' else v.lower()
        raise Unread('.%s(...) on a %s' % (name, type(v).__name__ if not isinstance(v, Obj) else v.cls))


def scope_get(scope, key):
    while scope is not None:
        if key in scope:
            return scope[key]
        scope = scope.get('__parent__')
    return None


def bind(params, vals, into, default, what):
    names = [p.lstrip('*') for p, _ in params]
    star = [p for p, _ in params if p.startswith('*')]
    pos = 0
    given = {}
    for n, v in vals:
        if n == '__trailing__':     # a trailing lambda: the last parameter
            n = names[-1]
        if n:
            if n not in names:
                raise Unread('%s has no parameter %s' % (what, n))
            given[n] = v
        elif star and names[pos] == star[0][1:]:
            given.setdefault(names[pos], []).append(v)
        else:
            if pos >= len(names):
                raise Unread('more arguments than %s takes' % what)
            given[names[pos]] = v
            pos += 1
    params = [(p.lstrip('*'), d if not p.startswith('*') else ('num', [])) for p, d in params]
    for p, d in params:
        if p in given:
            into[p] = given[p]
        elif d is not None:
            try:
                into[p] = default(d)
            except Unread as u:
                into[p] = Bad(str(u))
        else:
            into[p] = Bad('%s: no value for %s' % (what, p))
    return set(given)


def check(v):
    if isinstance(v, Bad):
        raise Unread(v.reason)


def has_bad(v):
    if isinstance(v, Bad):
        return True
    if isinstance(v, (list, tuple)):
        return any(has_bad(x) for x in v)
    if isinstance(v, dict):
        return any(has_bad(x) for x in v.values())
    return False


def text_of(v):
    check(v)
    if isinstance(v, bool):
        return 'true' if v else 'false'
    if isinstance(v, (int, float, str)):
        return str(int(v)) if isinstance(v, int) else str(v)
    raise Unread('a %s in a string' % type(v).__name__)


def raw_lines(v):
    """A generated table's lines: its strings (one or more raw strings), split into lines."""
    out = []
    for s in (v if isinstance(v, list) else [v]):
        check(s)
        out += [l.strip() for l in str(s).split('\n') if l.strip()]
    return out


def _collection(kind):
    def f(k, pos, named):
        if kind == 'map':
            return dict(pos)
        return list(pos)
    return f


COLLECTIONS = ('listOf', 'mutableListOf', 'arrayOf', 'intArrayOf', 'longArrayOf', 'setOf', 'mutableSetOf', 'mapOf',
               'mutableMapOf', 'hashMapOf', 'byteArrayOf', 'shortArrayOf')
BUILTINS = {n: _collection('map' if 'Map' in n or n == 'mapOf' else 'list') for n in COLLECTIONS}
BUILTINS.update({
    'emptyList': lambda k, p, n: [], 'emptySet': lambda k, p, n: [], 'emptyMap': lambda k, p, n: {},
    'Pair': lambda k, p, n: (p[0], p[1]),
    'wramx': lambda k, p, n: {'bank': p[0], 'addr': Hex(p[1])},
    'hexBytes': lambda k, p, n: Bad('bytes copied from the ROM'),
    'IntArray': lambda k, p, n: [0] * p[0] if len(p) == 1 else Bad('an array made by code'),
    'listOfNotNull': lambda k, p, n: [x for x in p if x is not None],
})


# --- The reference data: what is read for each game, and the rules for what is left out ---

REF = 'kiwami/data/pokedaisy'
GAMEDATA = 'gamedata/gen3'
REF_KT = [  # PokeDaisy's Kotlin the reference data evaluates (its data folder's files)
    'Pokedex.kt', 'Guide.kt', 'GuideAreas.kt', 'Telemetry.kt', 'Gen1Charset.kt', 'Gen3Mon.kt', 'NativeReader.kt', 'GuideRom.kt', 'RomItemText.kt', 'TrainerCard.kt',
    'Gen1Reader.kt', 'Gen1Dex.kt', 'Gen1Art.kt', 'Gen2Reader.kt', 'Gen2Dex.kt', 'Gen2Art.kt',
    'GuideGen1Gen.kt', 'GuideGen1.kt', 'GuideCrystalGen.kt', 'GuideCrystal.kt', 'ItemDescriptionsYellow.kt',
    'GuideEmerald.kt', 'GuideFireRed.kt', 'GuideRubySapphire.kt', 'GuideAreasEmeraldGen.kt', 'GuideAreasFireRedGen.kt',
    'GuideAreasLeafGreenGen.kt', 'GuideAreasRubySapphireGen.kt',
    'Gen4Reader.kt', 'Gen5Reader.kt', 'Gen4Dex.kt', 'Gen5DexBlack.kt', 'GuideGen4.kt', 'GuideGen5.kt',
    'GuidePlatinum.kt', 'GuideHeartGold.kt', 'GuideBlack.kt', 'GuideBlack2.kt', 'GuideGen4PlatinumGen.kt',
    'GuideGen4HeartGoldGen.kt', 'GuideGen5BlackGen.kt', 'GuideGen5WhiteGen.kt', 'GuideGen5Black2Gen.kt',
    'GuideGen5White2Gen.kt',
]
REF_TOML = ['retail/ruby_sapphire.toml', 'retail/emerald.toml', 'retail/firered.toml', 'retail/leafgreen.toml',
            'records/emerald.toml', 'records/firered.toml']
# The raw tables' columns (the generated guides' lines): a boss's team and an area's sub-map name are left out
BOSS_COLS = {'gen1': ('key', 'title', 'where', 'done', 'cond'), 'crystal': ('key', 'class', 'party', 'name'),
             'ds': ('key', 'trainer')}
AREA_COLS = ('place', 'kind', 'id', 'flag', 'qty', 'wants')
# A game a folder: its PokeDaisy titles (GameTitles.kt's English releases), the reader's configs (ram.json), the ROM
# tables' records (rom.json: GB's Dex and Art tables; a Gen III config's record fields and the family's TOML records;
# a DS config's Pokédex record), the hand-written guides (GuideId -> val), the generated bosses' and areas' tables,
# the guides' constants (a file, and a prefix where one file holds several games), and PokeDaisy's own words beyond
# the guides
REF_GAMES = [
    ('red-blue', 'Red and Blue', ['Pokémon Red', 'Pokémon Blue'], dict(
        ram=['GEN1_RED', 'GEN1_BLUE'], rom=['GEN1_DEX_RED', 'Gen1Art.RED', 'Gen1Art.BLUE'],
        guides={'RED': 'GUIDE_RED', 'BLUE': 'GUIDE_BLUE'},
        bosses={'G1R_BOSSES_RAW': 'gen1', 'G1B_BOSSES_RAW': 'gen1'}, areas=['G1R_AREAS_RAW', 'G1B_AREAS_RAW'],
        consts=[('GuideGen1Gen.kt', 'G1R_'), ('GuideGen1Gen.kt', 'G1B_'), ('GuideGen1.kt', None)])),
    ('yellow', 'Yellow', ['Pokémon Yellow'], dict(
        ram=['GEN1_YELLOW'], rom=['GEN1_DEX_YELLOW', 'Gen1Art.YELLOW'], guides={'YELLOW': 'GUIDE_YELLOW'},
        bosses={'G1Y_BOSSES_RAW': 'gen1'}, areas=['G1Y_AREAS_RAW'],
        consts=[('GuideGen1Gen.kt', 'G1Y_'), ('GuideGen1.kt', None)], words={'itemDescriptions': 'itemDescriptionsYellow'})),
    ('gold-silver', 'Gold and Silver', ['Pokémon Gold', 'Pokémon Silver'], dict(
        ram=['GEN2_GOLD', 'GEN2_SILVER'], rom=['GEN2_DEX_GOLD_SILVER', 'Gen2Art.GOLD', 'Gen2Art.SILVER'])),
    ('crystal', 'Crystal', ['Pokémon Crystal'], dict(
        ram=['GEN2_CRYSTAL'], rom=['GEN2_DEX_CRYSTAL', 'Gen2Art.CRYSTAL'], guides={'CRYSTAL': 'GUIDE_CRYSTAL'},
        bosses={'CR_BOSSES_RAW': 'crystal'}, areas=['GUIDE_AREAS_CRYSTAL_RAW'],
        consts=[('GuideCrystalGen.kt', 'CR_'), ('GuideCrystal.kt', None)])),
    ('ruby-sapphire', 'Ruby and Sapphire', ['Pokémon Ruby', 'Pokémon Sapphire'], dict(
        toml=['ruby', 'sapphire'], records=[], guides={'RUBY': 'GUIDE_RUBY', 'SAPPHIRE': 'GUIDE_SAPPHIRE'},
        areas=['GUIDE_AREAS_RUBY_SAPPHIRE_RAW'], consts=[('GuideRubySapphire.kt', None)])),
    ('emerald', 'Emerald', ['Pokémon Emerald'], dict(
        toml=['emerald', 'emerald_retail'], records=['records/emerald.toml'], guides={'EMERALD': 'GUIDE_EMERALD'},
        areas=['GUIDE_AREAS_EMERALD_RAW'], consts=[('GuideEmerald.kt', None)])),
    ('firered-leafgreen', 'FireRed and LeafGreen', ['Pokémon FireRed', 'Pokémon LeafGreen'], dict(
        toml=['firered_rev0', 'firered_rev1', 'leafgreen_rev0', 'leafgreen_rev1'], records=['records/firered.toml'],
        guides={'FIRERED': 'GUIDE_FIRERED', 'LEAFGREEN': 'GUIDE_LEAFGREEN'},
        areas=['GUIDE_AREAS_FIRERED_RAW', 'GUIDE_AREAS_LEAFGREEN_RAW'], consts=[('GuideFireRed.kt', None)])),
    ('platinum', 'Platinum', ['Pokémon Platinum'], dict(
        ram=['GEN4_PLATINUM_US'], guides={'PLATINUM': 'GUIDE_PLATINUM'}, bosses={'PT_BOSSES_RAW': 'ds'},
        areas=['PT_AREAS_RAW'], consts=[('GuidePlatinum.kt', None), ('GuideGen4PlatinumGen.kt', None)])),
    ('heartgold-soulsilver', 'HeartGold and SoulSilver', ['Pokémon HeartGold', 'Pokémon SoulSilver'], dict(
        ram=['GEN4_HEARTGOLD_US', 'GEN4_SOULSILVER_US'], guides={'HEARTGOLD': 'GUIDE_HEARTGOLD', 'SOULSILVER': 'GUIDE_SOULSILVER'},
        bosses={'HG_BOSSES_RAW': 'ds'}, areas=['HG_AREAS_RAW'],
        consts=[('GuideHeartGold.kt', None), ('GuideGen4HeartGoldGen.kt', None)])),
    ('black-white', 'Black and White', ['Pokémon Black', 'Pokémon White'], dict(
        ram=['GEN5_BLACK', 'GEN5_WHITE'], guides={'BLACK': 'GUIDE_BLACK', 'WHITE': 'GUIDE_WHITE'},
        bosses={'BLACK_BOSSES_RAW': 'ds', 'WHITE_BOSSES_RAW': 'ds'}, areas=['BLACK_AREAS_RAW', 'WHITE_AREAS_RAW'],
        consts=[('GuideBlack.kt', None)])),
    ('black2-white2', 'Black 2 and White 2', ['Pokémon Black 2', 'Pokémon White 2'], dict(
        ram=['GEN5_BLACK2', 'GEN5_WHITE2'], guides={'BLACK2': 'GUIDE_BLACK2', 'WHITE2': 'GUIDE_WHITE2'},
        bosses={'BLACK2_BOSSES_RAW': 'ds', 'WHITE2_BOSSES_RAW': 'ds'}, areas=['BLACK2_AREAS_RAW', 'WHITE2_AREAS_RAW'],
        consts=[('GuideBlack2.kt', None)])),
]
# A config's field that is a ROM table's record (rom.json, not ram.json)
ROM_FIELDS = ('pokedex', 'pokedexTables', 'guideTables', 'trainerCard', 'fieldItems', 'itemDescs')
# The text a config may keep: PokeDaisy's own identifiers (a preset, a constant's or an enum entry's name, a language
# letter); any other string is the games' text (a region, a boss, a place) or PokeDaisy's file names, left out
TEXT_OK = re.compile(r'[a-z][a-z0-9_]*|[A-Z]|[A-Za-z]+\.[A-Z][A-Z0-9_]*|[A-Z][A-Z0-9]*(?:_[A-Z0-9]+)+')
# Left out by rule, whatever the file (a game's own content, PokeDaisy's features outside a companion's facts)
REF_SKIP = {
    'internalToDex': 'the game\'s species order (a table of the game\'s)',
    'landmarks': 'a table of the game\'s (map to landmark)', 'genderRatios': 'a table of the game\'s',
    'growthRates': 'a table of the game\'s', 'tmItems': 'a table of the game\'s', 'tmMoves': 'a table of the game\'s',
    'mailItems': 'a table of the game\'s', 'probeName': 'a boss\'s name, the game\'s text',
    'regionalDex': 'a table of the game\'s (its regional Pokédex order)',
}


def ref_walk(v, path, notes, k, mode):
    """A value as JSON: in 'config' mode (ram.json, rom.json) a named table, a string of the games' and anything
    unevaluated are noted and left out; in 'guide' mode (guide.json, text.json) tables are written and strings kept, an
    object writes only the fields its source sets. None is left out in both."""
    if isinstance(v, Bad):
        notes.append((path, v.reason))
        return None
    if isinstance(v, bool) or v is None or isinstance(v, float):
        return v
    if isinstance(v, Hex):
        return '0x%X' % v if v >= 0 else int(v)
    if isinstance(v, int):
        return v
    if isinstance(v, str):
        if mode == 'config' and not TEXT_OK.fullmatch(v) and any(c.isalpha() for c in v):
            notes.append((path, 'text (%s)' % (v if len(v) < 40 else v[:37] + '...')))
            return None
        return v
    if isinstance(v, Table):
        if mode == 'config':
            notes.append((path, 'a table of PokeDaisy\'s (%s), not extracted' % v.name))
            return None
        try:
            return ref_walk(v.value(), path, notes, k, mode)
        except Unread as u:
            notes.append((path, str(u)))
            return None
    if isinstance(v, (list, tuple)):
        out = [ref_walk(x, '%s[%d]' % (path, i), notes, k, mode) for i, x in enumerate(v)]
        return [x for x in out if x is not None]
    if isinstance(v, dict):
        out = {}
        for key, x in v.items():
            y = ref_walk(x, '%s[%s]' % (path, key), notes, k, mode)
            if y is not None:
                out[str(int(key)) if isinstance(key, int) else str(key)] = y
        return out
    if isinstance(v, Obj):
        out = {}
        for f, x in v.fields.items():
            if f in REF_SKIP:
                notes.append(('%s.%s' % (path, f), REF_SKIP[f]))
                continue
            if mode == 'guide' and f not in v.given:
                continue
            if isinstance(x, Lambda):
                try:
                    x = k.invoke(x, [])
                    if isinstance(x, (Lambda, Fn)) or has_bad(x):
                        raise Unread('a function')
                except Unread:
                    if mode == 'guide':
                        out.setdefault('by_code', []).append(f)
                    else:
                        notes.append(('%s.%s' % (path, f), 'code run on the game\'s state'))
                    continue
            y = ref_walk(x, '%s.%s' % (path, f), notes, k, mode)
            if y is not None and not (mode == 'guide' and y == []):
                out[f] = y
        return out
    if isinstance(v, (Lambda, Fn)):
        notes.append((path, 'code run on the game\'s state'))
        return None
    if isinstance(v, Namespace):
        notes.append((path, 'a name of PokeDaisy\'s code (%s)' % v.name))
        return None
    notes.append((path, 'a value it can\'t write (%s)' % type(v).__name__))
    return None


def toml_records(tomls):
    """Every [config.X], [record.X] and [list.X] of the TOML files read, by name: (kind, file, table)."""
    out = {}
    for f, d in tomls.items():
        for kind in ('config', 'record', 'list'):
            for name, t in d.get(kind, {}).items():
                out[name] = (kind, f, t)
    return out


def toml_value(name, recs, schemas, k, seen=()):
    """A TOML config or record over its base's, with its schema's defaults under a config; each string that names a
    record, a list or one of PokeDaisy's Kotlin values is that value."""
    if name in seen:
        raise Unread('%s is its own base' % name)
    kind, f, t = recs[name]
    if kind == 'list':
        return [ref_toml_str(x, recs, schemas, k, seen + (name,)) for x in t.get('items', [])]
    schema = schemas.get('gen3_config' if kind == 'config' else t.get('schema', ''), {})
    out = {}
    if t.get('base'):
        base = t['base']
        out = dict(toml_value(base, recs, schemas, k, seen + (name,)) if base in recs else
                   ref_kotlin_fields(k, base))
    elif kind == 'config':
        for fn, fd in schema.get('field', {}).items():
            if 'default' in fd:
                out[fn] = ref_toml_field(fd['default'], fd, recs, schemas, k, seen + (name,))
    for fn, v in t.items():
        if fn in ('base', 'schema', 'private', 'lazy', 'port'):
            continue
        out[fn] = ref_toml_field(v, schema.get('field', {}).get(fn, {}), recs, schemas, k, seen + (name,))
    return out


def ref_kotlin_fields(k, name):
    try:
        v = k.top(name)
    except Unread as u:
        return {'(base)': Bad(str(u))}
    return dict(v.fields) if isinstance(v, Obj) else {'(base)': Bad('%s isn\'t a record' % name)}


def ref_toml_field(v, fd, recs, schemas, k, seen):
    if isinstance(v, dict):
        if 'kotlin' in v:
            return Bad('Kotlin code (%s)' % v['kotlin'])
        if 'ref' in v and len(v) == 1:
            return ref_toml_str(v['ref'], recs, schemas, k, seen)
        sub = dict(v)
        if fd.get('record') and isinstance(sub.get('base'), str):
            base = sub.pop('base')
            out = toml_value(base, recs, schemas, k, seen) if base in recs else ref_kotlin_fields(k, base)
            out = dict(out)
            out.update({a: ref_toml_field(b, {}, recs, schemas, k, seen) for a, b in sub.items()})
            return out
        return {a: ref_toml_field(b, {}, recs, schemas, k, seen) for a, b in sub.items()}
    if isinstance(v, list):
        return [ref_toml_field(x, {}, recs, schemas, k, seen) for x in v]
    if isinstance(v, str):
        if v == 'none':
            return None
        if fd.get('type') in ('ref',) or v in recs:
            return ref_toml_str(v, recs, schemas, k, seen)
        if fd.get('type') == 'long' and re.fullmatch(r'0[xX][0-9A-Fa-f]+|\d+', v):
            return Hex(int(v, 0))
        return v
    if isinstance(v, int) and not isinstance(v, bool) and fd.get('type') == 'long':
        return Hex(v)
    if isinstance(v, int) and not isinstance(v, bool) and v >= 0x02000000:
        return Hex(v)
    return v


def ref_toml_str(v, recs, schemas, k, seen):
    if v in recs:
        return toml_value(v, recs, schemas, k, seen)
    if TEXT_OK.fullmatch(v) and not re.fullmatch(r'[A-Z][A-Z0-9]*(?:_[A-Z0-9]+)+', v):
        return v
    try:
        return k.top(v)
    except Unread:
        return v if TEXT_OK.fullmatch(v) else Bad('%s isn\'t in the files read' % v)


def ref_raw(k, name, cols):
    """A generated table's lines, its columns by name; the columns after them (a boss's team, an area's sub-map) left
    out."""
    v = k.top(name)
    lines = raw_lines(v.value() if isinstance(v, Table) else v)
    out = []
    for line in lines:
        f = line.split('|')
        row = {}
        for c, x in zip(cols, f):
            if re.fullmatch(r'-?\d+', x):
                row[c] = int(x)
            elif re.fullmatch(r'\d+(,\d+)+', x):
                row[c] = [int(y) for y in x.split(',')]
            elif x != '':
                row[c] = x
        out.append(row)
    return out


def ref_consts(k, file, prefix, src):
    """A guide file's int constants (flags, items, vars, RAM): those its source writes as const vals."""
    out = {}
    for m in re.finditer(r'\bconst\s+val\s+(\w+)', strip_comments(src)):
        name = m.group(1)
        if prefix and not name.startswith(prefix):
            continue
        d = k.privates.get(file, {}).get(name) or k.globals.get(name)
        v = k.decl_value(d) if d else None
        if isinstance(v, int) and not isinstance(v, bool):
            out[name] = '0x%X' % v if isinstance(v, Hex) else int(v)
    return out


def build_reference(checkout):
    import tomllib
    inputs, files = {}, {}

    def read(rel):
        data = open(os.path.join(checkout, rel), 'rb').read()
        inputs['pokedaisy ' + rel] = pe.sha256(data)
        return data

    kt = {f: read(DATA + '/' + f).decode('utf-8') for f in REF_KT}
    k = Kotlin(kt)
    tomls = {f: tomllib.loads(read(GAMEDATA + '/' + f).decode('utf-8')) for f in REF_TOML}
    schema_dir = os.path.join(checkout, 'gamedata/schema')
    schemas = {os.path.splitext(f)[0]: tomllib.loads(read('gamedata/schema/' + f).decode('utf-8'))
               for f in sorted(os.listdir(schema_dir)) if f.endswith('.toml')}
    recs = toml_records(tomls)
    k.extern = lambda name: toml_value(name, recs, schemas, k) if name in recs else None
    titles = retail_sha1s(read(TITLES).decode('utf-8'))
    index, unread = [], []
    for folder, title, pd_titles, spec in REF_GAMES:
        notes, out = [], {}
        rel = [{'sha1': h, 'title': t} for t in pd_titles for h in sorted(titles.get(t, ()))]
        if not rel:
            fail('GameTitles.kt has no release of %s' % title)
        out['releases'] = {'releases': rel}
        ram, rom = {}, {}
        for name in spec.get('ram', []):
            v = k.top(name)
            if isinstance(v, Obj):
                for f in ROM_FIELDS:
                    if f in v.fields and v.fields[f] is not None:
                        rom['%s.%s' % (name, f)] = ref_walk(v.fields[f], '%s.%s' % (name, f), notes, k, 'config')
                v = Obj(v.cls, {f: x for f, x in v.fields.items() if f not in ROM_FIELDS})
            ram[name] = ref_walk(v, name, notes, k, 'config')
        for name in spec.get('rom', []):
            rom[name] = ref_walk(k.top(name), name, notes, k, 'config')
        for name in spec.get('toml', []):
            v = toml_value(name, recs, schemas, k)
            for f in ROM_FIELDS:
                if v.get(f) is not None:
                    rom['%s.%s' % (name, f)] = ref_walk(v[f], '%s.%s' % (name, f), notes, k, 'config')
            ram[name] = ref_walk({f: x for f, x in v.items() if f not in ROM_FIELDS}, name, notes, k, 'config')
        for f in spec.get('records', []):
            for name, (kind, rf, t) in recs.items():
                if rf == f and kind == 'record' and 'LEGACY' not in name:
                    rom[name] = ref_walk(toml_value(name, recs, schemas, k), name, notes, k, 'config')
        out['ram'] = {'configs': ram}
        if rom:
            out['rom'] = {'tables': rom}
        guide, text = {}, {}
        for gid, name in spec.get('guides', {}).items():
            g = k.top(name)
            if not isinstance(g, Obj):
                check(g)
            bosses = ref_walk(g.fields.get('bosses'), '%s.bosses' % name, notes, k, 'guide')
            if bosses:
                guide.setdefault('guides', {})[gid] = {'bosses': bosses, 'verified': g.fields.get('verified')}
            pages = ref_walk(g.fields.get('pages'), '%s.pages' % name, notes, k, 'guide')
            if pages:
                text.setdefault('guides', {})[gid] = {'pages': pages}
        for name, cols in spec.get('bosses', {}).items():
            guide.setdefault('bossTables', {})[name] = ref_raw(k, name, BOSS_COLS[cols])
        for name in spec.get('areas', []):
            guide.setdefault('areas', {})[name] = ref_raw(k, name, AREA_COLS)
        for f, prefix in spec.get('consts', []):
            c = ref_consts(k, f, prefix, kt[f])
            if c:
                guide.setdefault('consts', {}).update(c)
        for key, name in spec.get('words', {}).items():
            text[key] = ref_walk(k.top(name), name, notes, k, 'guide')
        if guide:
            out['guide'] = guide
        if text:
            out['text'] = text
        for kind, doc in out.items():
            doc.update({'schema': 1, 'kind': 'pokedaisy-reference-' + kind, 'game': folder, 'title': title})
            files['%s/%s/%s.json' % (REF, folder, kind)] = pe.dump(doc).encode('utf-8')
        index.append((folder, title, sorted(out), spec))
        unread += [(folder, p, r) for p, r in notes]
    files[REF + '/LICENSE'] = read('LICENSE')
    files[REF + '/README.md'] = ref_readme(index, unread).encode('utf-8')
    return files, inputs


REF_KINDS = {
    'releases': 'the English releases PokeDaisy knows: SHA-1 and its title (GameTitles.kt)',
    'ram': 'the reader\'s configs, by PokeDaisy\'s name: RAM addresses, offsets, sizes and layouts',
    'rom': 'the ROM tables\' records: where the Pokédex, the pictures, the guide\'s tables and the rest are',
    'guide': 'the guides\' structure by ids: the bosses (title, place, the flag that marks them beaten, trainer ids), '
             'the generated boss tables without their teams, the areas\' items, gifts and trades by place, id, flag '
             'and count, and the guides\' constants (flags, items, vars)',
    'text': 'PokeDaisy\'s own words: its guide pages (the tips, and WHERE IS: each entry titled by what it is about, '
            'with PokeDaisy\'s answer and hint), and Yellow\'s item descriptions it wrote itself',
}


def ref_readme(index, unread):
    out = ['# PokeDaisy\'s reference data', '',
           'Based on PokeDaisy by Lidor Itzhari - https://github.com/lidor30/pokedaisy', '',
           'The facts PokeDaisy %s holds about every mainline game it supports, English releases only, as JSON: what '
           'its readers read from the games\' memory and ROM, its guides\' structure by ids and flags, and its own '
           'words. Nothing here is copied into the image (the image\'s overlay is `kiwami/rootfs/`), and nothing of '
           'the games themselves is here: no species, move or item names, no Pokédex or item text, no wild tables, '
           'trainer teams, base stats, pictures or maps. Place and boss names are kept only where a guide names its '
           'bosses; PokeDaisy\'s own words are kept whole, though they name what they talk about.' % PIN[:8], '',
           'Written by `kiwami/tools/pokedaisy-extract.py <checkout> --reference` (see `kiwami/GB-POKEMON.md`); '
           '`SOURCES` records PokeDaisy\'s commit and the SHA-256 of every file read and written, and the script\'s '
           '`--check` verifies them. Licence: `LICENSE` (GPL-3.0) and `NOTICE` (its 7(b) attribution term).', '',
           '## The games', '']
    for folder, title, kinds, spec in index:
        out.append('- **%s** (`%s/`): %s.' % (title, folder, ', '.join('`%s.json`' % x for x in kinds)))
    out += ['', 'Each file:', '']
    out += ['- `%s.json`: %s.' % (x, d) for x, d in REF_KINDS.items()]
    out += ['', 'Numbers PokeDaisy writes in hex are written as hex strings ("0x020244EC"). A guide\'s `by_code` names '
            'the fields PokeDaisy works out by code from the save (a team chosen by the starter, a boss done when any '
            'of several flags is set): its source has the rule.', '',
            '## Not extracted', '',
            'What the script met and left out: by rule (a table of the game\'s, its text, a table of PokeDaisy\'s own '
            'ids), or because it is code rather than a value. Paths are PokeDaisy\'s names. A name "not in the files '
            'read" is one of PokeDaisy\'s values in a file the script doesn\'t read: its tables of the games\' content '
            '(map places, item pockets, fly spots, town maps), its art (NdsArt) or its telemetry.', '']
    for folder, title, kinds, spec in index:
        rows = [(p, r) for f, p, r in unread if f == folder]
        if rows:
            out += ['### %s' % title, '']
            out += ['- `%s`: %s' % (p, r.replace('|', '\\|')) for p, r in sorted(set(rows))]
            out.append('')
    out += ['Left out whole: PokeDaisy\'s hacks, its other languages\' releases and texts (`*TextDeGen.kt`, '
            '`GameText*Gen.kt`, `gamedata/gen3/ports/`, `gamedata/nds/ports.toml`), and its tables of the games\' '
            'content (species, move and item names and data, Pokédex and item text, wild Pokémon, trainer teams, '
            'base stats, gender ratios, growth rates, type charts, town maps).', '']
    return '\n'.join(out)


REF_SOURCES = REF + '/SOURCES'
REF_HEADER = '''# Where kiwami/data/pokedaisy/ comes from, written by kiwami/tools/pokedaisy-extract.py <checkout> --reference: the
# PokeDaisy commit read, then a line a file: "in" for what was read, "out" for what was written, with its SHA-256.
# --check without the checkout verifies the "out" files against these hashes.'''


def ref_sources_text(head, files, inputs):
    body = [REF_HEADER, '', 'commit pokedaisy %s' % head]
    body += ['in  %s  %s' % (h, w) for w, h in sorted(inputs.items())]
    body += ['out %s  %s' % (pe.sha256(d), f) for f, d in sorted(files.items())]
    return '\n'.join(body) + '\n'


def ref_verify():
    path = os.path.join(REPO, REF_SOURCES)
    if not os.path.exists(path):
        fail('no %s' % REF_SOURCES)
    n, listed = 0, set()
    for line in open(path, encoding='utf-8').read().splitlines():
        m = re.match(r'out\s+([0-9a-f]{64})\s+(.+)$', line)
        if not m:
            continue
        p = os.path.join(REPO, m.group(2))
        listed.add(m.group(2))
        if not os.path.exists(p):
            fail('%s is missing (%s)' % (m.group(2), REF_SOURCES))
        if pe.sha256(open(p, 'rb').read()) != m.group(1):
            fail('%s isn\'t the file %s records: run the script again' % (m.group(2), REF_SOURCES))
        n += 1
    for root, _, fs in os.walk(os.path.join(REPO, REF)):
        for f in fs:
            r = os.path.relpath(os.path.join(root, f), REPO)
            if r not in listed and r not in (REF_SOURCES, REF + '/NOTICE'):
                fail('%s isn\'t in %s: a file the script didn\'t write' % (r, REF_SOURCES))
    return n


def reference(checkout, head, check):
    """--reference writes kiwami/data/pokedaisy/ and its SOURCES; --check with the checkout compares them."""
    files, inputs = build_reference(checkout)
    sources = ref_sources_text(head, files, inputs)
    if check:
        bad = [f for f, d in sorted(files.items())
               if not os.path.exists(os.path.join(REPO, f)) or open(os.path.join(REPO, f), 'rb').read() != d]
        sp = os.path.join(REPO, REF_SOURCES)
        if not os.path.exists(sp) or open(sp, encoding='utf-8').read() != sources:
            bad.append(REF_SOURCES)
        if bad:
            t = tempfile.mkdtemp(prefix='pokedaisy-reference.')
            for f, d in files.items():
                os.makedirs(os.path.dirname(os.path.join(t, f)), exist_ok=True)
                open(os.path.join(t, f), 'wb').write(d)
            fail('the checkout gives other reference files: %s (this run\'s are in %s)' % (', '.join(bad), t))
        ref_verify()
        print('pokedaisy-extract: the reference data and its SOURCES are PokeDaisy %s\'s' % head[:8])
        return
    old = set()
    for root, _, fs in os.walk(os.path.join(REPO, REF)):
        old |= {os.path.relpath(os.path.join(root, f), REPO) for f in fs if f.endswith('.json')}
    for f in sorted(old - set(files)):
        os.remove(os.path.join(REPO, f))
    for f, d in files.items():
        os.makedirs(os.path.dirname(os.path.join(REPO, f)), exist_ok=True)
        open(os.path.join(REPO, f), 'wb').write(d)
    open(os.path.join(REPO, REF_SOURCES), 'w', encoding='utf-8').write(sources)
    print('pokedaisy-extract: wrote %d reference files in %s' % (len(files), REF))


def main(argv):
    check = '--check' in argv
    any_commit = '--any-commit' in argv
    ref = '--reference' in argv
    args = [a for a in argv if a not in ('--check', '--any-commit', '--reference')]
    if len(args) > 1 or any(a.startswith('-') for a in args) or (ref and check):
        sys.exit('usage: pokedaisy-extract.py [<pokedaisy checkout>] [--check | --reference] [--any-commit]')
    outs = ['%s/%s.ram.json' % (FOLDER, g[0]) for g in GAMES] + [FOLDER + '/LICENSE'] + \
        ['%s/%s.json' % (FOLDER, k) for k in ['crystal'] + list(GEN1_GUIDES)]
    if not args:
        if not check:
            fail('give PokeDaisy\'s checkout, or --check')
        n = pe.verify_sources(SECTION, outs + [EXTRA])
        print('pokedaisy-extract: %d files match SOURCES\' [pokedaisy]' % n)
        n = ref_verify()
        print('pokedaisy-extract: %d reference files match %s' % (n, REF_SOURCES))
        return
    checkout = os.path.abspath(args[0])
    head = pe.git(checkout, 'rev-parse', 'HEAD').decode().strip()
    if head != PIN and not any_commit:
        fail('%s is at %s, not PokeDaisy %s (--any-commit to read it anyway)' % (checkout, head[:8], PIN[:8]))
    if ref:
        reference(checkout, head, False)
        return
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
        reference(checkout, head, True)
        return
    os.makedirs(os.path.join(REPO, FOLDER), exist_ok=True)
    for f, d in files.items():
        open(os.path.join(REPO, f), 'wb').write(d)
    open(src_path, 'w', encoding='utf-8').write(sources)
    print('pokedaisy-extract: wrote %s' % ', '.join(os.path.basename(f) for f in sorted(files)))


if __name__ == '__main__':
    main(sys.argv[1:])
