#!/bin/sh
# The game files kiwami/tests/units.cpp hashes, made once by hand and committed (64 KB at most each):
#   kiwami/tests/ra/make-fixtures.sh        (needs python3 and bsdtar, libarchive's)
# A PlayStation disc: SYSTEM.CNF naming its boot executable, SLUS_000.01, a PS-X EXE of 4 KB (its header says 2048
# bytes of code after the header's 2048). As an ISO 9660 image (psx.iso), a cue on it as MODE1/2048
# (psx-mode1.cue), the same sectors as raw MODE2/2352 (psx.bin, psx-mode2.cue), and an .m3u naming that cue (psx.m3u).
# rcheevos hashes the executable's name and its bytes: the test computes that from SLUS_000.01, kept beside them.
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
ls -l SLUS_000.01 psx.iso psx-mode1.cue psx.bin psx-mode2.cue psx.m3u
