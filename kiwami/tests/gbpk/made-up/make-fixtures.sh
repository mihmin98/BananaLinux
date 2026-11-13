#!/bin/sh
# The made-up Game Boy ROMs kiwami/tests/gbpk-units.cpp reads through archives, made once by hand and committed:
#   kiwami/tests/gbpk/made-up/make-fixtures.sh        (needs python3, and bsdtar, libarchive's)
# rom.gbc: 32 KB of a counting pattern with a Color game's header, titled PM_CRYSTAL with the code BYTE (Crystal's),
# so it is told as Crystal by its title and refused by its SHA-1; other.gb: the same titled TETRIS, a game the
# companion doesn't know. game.zip holds readme.txt and rom.gbc; game.7z (solid, LZMA2) readme.txt, other.gb and
# rom.gbc, whose name comes after other.gb's, so the test sees the first game in name order taken.
set -e
cd "$(dirname "$0")"
T=$(mktemp -d)
trap 'rm -rf "$T"' EXIT
python3 - "$T" <<'PY'
import sys
def rom(title, code, cgb):
    r = bytearray((i * 13 + 7) & 0xFF for i in range(0x8000))
    r[0x134:0x144] = bytes(16)
    r[0x134:0x134 + len(title)] = title
    if code:
        r[0x13F:0x143] = code
    r[0x143] = cgb
    return bytes(r)
open('rom.gbc', 'wb').write(rom(b'PM_CRYSTAL', b'BYTE', 0xC0))
open(sys.argv[1] + '/other.gb', 'wb').write(rom(b'TETRIS', b'', 0x00))
PY
printf 'not a game\n' > "$T/readme.txt"
cp rom.gbc "$T/"
rm -f game.zip game.7z
bsdtar -cf game.zip --format zip -C "$T" readme.txt rom.gbc
bsdtar -cf game.7z --format 7zip --options 7zip:compression=lzma2 -C "$T" readme.txt rom.gbc other.gb
