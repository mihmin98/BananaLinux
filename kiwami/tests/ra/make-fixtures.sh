#!/bin/sh
# The game files kiwami/tests/units.cpp hashes, made once by hand and committed (64 KB at most each):
#   kiwami/tests/ra/make-fixtures.sh        (needs python3 and bsdtar, libarchive's)
# A PlayStation disc: SYSTEM.CNF naming its boot executable, SLUS_000.01, a PS-X EXE of 4 KB (its header says 2048
# bytes of code after the header's 2048). As an ISO 9660 image (psx.iso), a cue on it as MODE1/2048
# (psx-mode1.cue), the same sectors as raw MODE2/2352 (psx.bin, psx-mode2.cue), and an .m3u naming that cue (psx.m3u).
# rcheevos hashes the executable's name and its bytes: the test computes that from SLUS_000.01, kept beside them.
# Cartridges in archives, each hashed on the game in it: a GBA game (cart.gba, 4 KB) as cart.zip, cart-lzma.7z and
# cart-lzma2.7z; an NES game with its 16-byte header (cart.nes) as cart-nes.7z; several.zip and several.7z (solid)
# holding readme.txt, B.gba and a.gba in that order (a.gba is cart.gba: RetroArch takes the first game in name order,
# letters' case aside); readme.zip and readme.7z with no game; big.7z, cart-lzma.7z with its header saying the game
# is 65 MB.
set -e
cd "$(dirname "$0")"
T=$(mktemp -d)
trap 'rm -rf "$T"' EXIT
python3 - "$T" <<'PY'
import struct, sys
h = bytearray(2048)
h[0:8] = b"PS-X EXE"
h[28:32] = struct.pack("<I", 2048)
open(sys.argv[1] + "/SLUS_000.01", "wb").write(bytes(h) + bytes((i * 7 + 3) % 251 for i in range(2048)))
PY
printf 'BOOT = cdrom:\\SLUS_000.01;1\r\nTCB = 4\r\nEVENT = 10\r\nSTACK = 801FFFF0\r\n' > "$T/SYSTEM.CNF"
cp "$T/SLUS_000.01" SLUS_000.01
bsdtar -cf psx.iso --format iso9660 --options 'iso9660:!pad,!rockridge,!joliet' -C "$T" SYSTEM.CNF SLUS_000.01
# each 2048-byte sector as a raw one: sync, header (its address from 00:02:00, mode 2), Form 1 subheader, the data,
# EDC and ECC left zero (rcheevos reads the data only)
python3 - <<'PY'
iso = open("psx.iso", "rb").read()
bcd = lambda v: (v // 10) << 4 | v % 10
out = bytearray()
for i in range(len(iso) // 2048):
    lba = i + 150
    head = bytes([bcd(lba // 4500), bcd(lba // 75 % 60), bcd(lba % 75), 2])
    out += bytes([0] + [0xFF] * 10 + [0]) + head + bytes([0, 0, 8, 0, 0, 0, 8, 0]) + iso[i * 2048:(i + 1) * 2048] + bytes(280)
open("psx.bin", "wb").write(out)
PY
printf 'FILE "psx.iso" BINARY\n  TRACK 01 MODE1/2048\n    INDEX 01 00:00:00\n' > psx-mode1.cue
printf 'FILE "psx.bin" BINARY\n  TRACK 01 MODE2/2352\n    INDEX 01 00:00:00\n' > psx-mode2.cue
printf 'psx-mode2.cue\n' > psx.m3u
python3 - "$T" <<'PY'
import sys
game = bytes((i * 13 + 1) % 251 for i in range(4096))
open("cart.gba", "wb").write(game)
open("cart.nes", "wb").write(b"NES\x1a" + bytes([1, 1]) + bytes(10) + bytes((i * 7 + 2) % 251 for i in range(4096)))
open(sys.argv[1] + "/a.gba", "wb").write(game)
open(sys.argv[1] + "/B.gba", "wb").write(bytes((i * 5 + 9) % 251 for i in range(4096)))
open(sys.argv[1] + "/readme.txt", "w").write("Not a game.\n")
PY
rm -f cart.zip cart-lzma.7z cart-lzma2.7z cart-nes.7z several.zip several.7z readme.zip readme.7z big.7z
bsdtar -cf cart.zip --format zip cart.gba
bsdtar -cf cart-lzma.7z --format 7zip --options 7zip:compression=lzma1 cart.gba
bsdtar -cf cart-lzma2.7z --format 7zip --options 7zip:compression=lzma2 cart.gba
bsdtar -cf cart-nes.7z --format 7zip --options 7zip:compression=lzma2 cart.nes
bsdtar -cf several.zip --format zip -C "$T" readme.txt B.gba a.gba
bsdtar -cf several.7z --format 7zip --options 7zip:compression=lzma2 -C "$T" readme.txt B.gba a.gba
bsdtar -cf readme.zip --format zip -C "$T" readme.txt
bsdtar -cf readme.7z --format 7zip -C "$T" readme.txt
# big.7z: bsdtar writes the header unpacked, so its coder's unpacked size (kCodersUnpackSize, 0x0C, then 4096 as
# 0x90 0x00) is rewritten as 65 MB (0xE4 0x00 0x00 0x10), and the start header's size and CRCs written again
python3 - <<'PY'
import struct, zlib
a = open("cart-lzma.7z", "rb").read()
off, size = struct.unpack_from("<QQ", a, 12)
head = a[32 + off:32 + off + size]
assert head.count(b"\x0c\x90\x00") == 1, "cart-lzma.7z's header isn't the one expected"
head = head.replace(b"\x0c\x90\x00", b"\x0c\xe4\x00\x00\x10")
start = struct.pack("<QQI", off, len(head), zlib.crc32(head))
open("big.7z", "wb").write(a[:8] + struct.pack("<I", zlib.crc32(start)) + start + a[32:32 + off] + head)
PY
ls -l SLUS_000.01 psx.iso psx-mode1.cue psx.bin psx-mode2.cue psx.m3u cart.gba cart.nes cart.zip cart-lzma.7z \
    cart-lzma2.7z cart-nes.7z several.zip several.7z readme.zip readme.7z big.7z
