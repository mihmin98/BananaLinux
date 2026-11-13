#!/bin/sh
# The fork's checks, as a step in CI after upstream's (which cloned Buildroot), or by hand:
#   kiwami/tests/ci.sh <buildroot dir>
set -e
BR=$(cd "$1" && pwd)
cd "$(dirname "$0")/../.."

# the fork's shell scripts parse
for f in $(find kiwami -name '*.sh') $(find kiwami/rootfs -type f \( -path '*/init.d/*' -o -path '*/bin/*' -o -path '*/libexec/*' -o -path '*/sleep.d/*' \)) kiwami/tests/bin/*; do
    sh -n "$f"
done

# the Tailscale helper, against stand-ins
kiwami/tests/tailscale-helper.sh

# the fork's cores' RetroArch overrides, in a made-up config folder, and run-retroarch's hook
kiwami/tests/core-overrides.sh

# RetroArch master's credentials file folded back into retroarch.cfg, on a made-up card, and ra-secrets' hook
kiwami/tests/ra-keychain.sh

# the vendored C libraries are each their pinned version's files, unchanged
for d in ui/src/kiwami/third_party/*/; do (cd "$d" && sha256sum -c --quiet SHA256SUMS) || { echo "third_party: $d differs from its SHA256SUMS" >&2; exit 1; }; done
echo "third_party: $(ls -d ui/src/kiwami/third_party/*/ | wc -l) vendored libraries checked"

# banana-ui's code of the fork's that needs no screen (the JSON reader, Tailscale's status, the name, the QR code,
# Settings' grid of groups, RetroAchievements' replies, its account on the card and its games' hashes), with the
# host's g++ and the vendored C built by banana-ui's own rules
make -s -C ui kiwami-c
T=$(mktemp -d)
${CXX:-g++} -std=c++17 -Wall -Werror -I ui/src -o "$T/units" kiwami/tests/units.cpp ui/src/kiwami/json.cpp ui/src/kiwami/tsstatus.cpp \
    ui/src/kiwami/qrcodegen.cpp ui/src/kiwami/grid.cpp ui/src/kiwami/raclient.cpp ui/src/kiwami/rastore.cpp ui/src/kiwami/rahash.cpp \
    ui/src/kiwami/ra7z.cpp ui/src/kiwami/rachd.cpp ui/src/zip.cpp ui/src/zipdir.cpp \
    $(find ui/src/kiwami/third_party -name '*.kiwami.o') -ldl -lz
"$T/units" kiwami/tests/tailscale kiwami/tests/ra
# the Game Boy Pokémon companion's (ui/src/kiwami/gbpk/, GPL-3.0: kiwami/GB-POKEMON.md), its own binary: which game a
# ROM is (the made-up ones in kiwami/tests/gbpk/made-up/, plain and in archives), its data files and the list of ROMs
# its ROM tests expect (kiwami/tests/gbpk/roms.sha1), its text, its ROM reader on a made-up ROM, its memory reader
# replayed over PokeDaisy's captures (kiwami/tests/gbpk/fixtures/), Gen I's and II's rules through upstream's logic
# and its hooks, and the guides through upstream's loader. Its sources but the game panel's (game.cpp, host.cpp,
# panel.cpp: Settings' and the pages', built only into banana-ui, gbpk-ui.sh below)
gbpk_data() { ls ui/src/kiwami/gbpk/*.cpp | grep -Ev '/(game|host|panel)\.cpp$'; }
${CXX:-g++} -std=c++17 -Wall -Werror -I ui/src -o "$T/gbpk-units" kiwami/tests/gbpk-units.cpp $(gbpk_data) \
    ui/src/kiwami/json.cpp ui/src/kiwami/ra7z.cpp ui/src/zip.cpp ui/src/zipdir.cpp ui/src/nds/nitro.cpp ui/src/nds/model.cpp \
    ui/src/nds/logic.cpp ui/src/nds/guide.cpp ui/src/json.cpp ui/src/pokemon.cpp ui/src/pokemap.cpp \
    $(find ui/src/kiwami/third_party/lzma -name '*.kiwami.o') -lz -lpthread
"$T/gbpk-units" kiwami/rootfs/usr/share/banana/companions/gb-pokemon kiwami/tests/gbpk
# the stand-in RetroAchievements server the UI tests run against (its bytecode in $T, not beside it)
PYTHONPYCACHEPREFIX="$T" python3 -m py_compile kiwami/tests/ra-server.py
rm -rf "$T"

# banana-ui compiled one file per call (ui/src/kiwami/kiwami.mk) is byte for byte upstream's one g++ call, with the
# host's g++, and a change rebuilds what it should; when banana-ui's libraries are there, as make ui needs (the
# compilers' warnings, upstream's code's, shown only on a failure)
if pkg-config --exists freetype2 libdrm zlib; then
    T=$(mktemp -d)
    kiwami/tests/ui-objects.sh > "$T/log" 2>&1 || { tail -n 40 "$T/log" >&2; exit 1; }
    grep '^ui-objects' "$T/log"
    rm -rf "$T"
else
    echo "ui-objects: skipped, no freetype2, libdrm or zlib (pkg-config)"
fi

# the comparison of two builds' images (kiwami/tests/image-compare.sh), its rules on made-up data: each kind of noise
# excused, each planted difference reported
python3 -I kiwami/tests/image-compare.py --self-test

# the audit of the libraries packages link against what they declare (kiwami/tests/deps-audit.py), on a made-up output
# built with the host's cc: an undeclared library reported, a declared, virtual or allowed one not, lists read right
if command -v cc >/dev/null && command -v readelf >/dev/null; then
    python3 -I kiwami/tests/deps-audit.py --self-test
else
    echo "deps-audit: skipped, no cc or readelf"
fi

# the Game Boy Pokémon companion's data (kiwami/GB-POKEMON.md): the extraction scripts' committed outputs as SOURCES
# records them (neither PokeDaisy nor pret needed), every value gbpk-symbols.txt asks for answered, and each data file
# a gb-pokemon-ram file whose addresses are all WRAM or HRAM, beside PokeDaisy's LICENSE and NOTICE
python3 -I kiwami/tools/pret-extract.py --check
python3 -I kiwami/tools/pokedaisy-extract.py --check
G=kiwami/rootfs/usr/share/banana/companions/gb-pokemon
for f in LICENSE NOTICE SOURCES; do [ -s "$G/$f" ] || { echo "gb-pokemon: no $f in $G" >&2; exit 1; }; done
grep -qF 'Based on PokeDaisy by Lidor Itzhari - https://github.com/lidor30/pokedaisy' "$G/NOTICE" ||
    { echo "gb-pokemon: $G/NOTICE hasn't PokeDaisy's attribution line" >&2; exit 1; }
python3 -I - "$G"/*.ram.json <<'EOF'
import json, sys
for f in sys.argv[1:]:
    d = json.load(open(f, encoding='utf-8'))
    if d.get('kind') != 'gb-pokemon-ram' or d.get('schema') != 1:
        sys.exit('gb-pokemon: %s isn\'t a gb-pokemon-ram file' % f)
    if not d.get('releases') or any(len(r.get('sha1', '')) != 40 for r in d['releases']) or not d.get('ram'):
        sys.exit('gb-pokemon: %s has no releases or no ram' % f)
    for k, v in d['ram'].items():
        a = int(v['addr'], 16)
        if not (0xC000 <= a <= 0xDFFF or 0xFF80 <= a <= 0xFFFE) or not 0 <= v['bank'] <= 7:
            sys.exit('gb-pokemon: %s: ram.%s at %d:%s is neither WRAM nor HRAM' % (f, k, v['bank'], v['addr']))
print('gb-pokemon: %d data files checked' % (len(sys.argv) - 1))
EOF

# PokeDaisy's reference data (kiwami/data/pokedaisy/, written by pokedaisy-extract.py --reference, checked against its
# SOURCES above): its LICENSE and NOTICE, every file JSON, none of it in the image's overlay, and none of twenty of the
# games' species, move and item names (in capitals and in title case) in it or in the image's data, but where a guide
# names its bosses and places, and in text.json, PokeDaisy's own words, which name what they talk about
P=kiwami/data/pokedaisy
for f in LICENSE NOTICE README.md SOURCES; do [ -s "$P/$f" ] || { echo "pokedaisy: no $f in $P" >&2; exit 1; }; done
grep -qF 'Based on PokeDaisy by Lidor Itzhari - https://github.com/lidor30/pokedaisy' "$P/NOTICE" ||
    { echo "pokedaisy: $P/NOTICE hasn't PokeDaisy's attribution line" >&2; exit 1; }
[ -z "$(find kiwami/rootfs -path '*pokedaisy*')" ] || { echo "pokedaisy: the reference data is in the overlay" >&2; exit 1; }
python3 -I - "$P"/*/*.json "$G"/*.json <<'EOF'
import json, os, re, sys
NAMES = ['BULBASAUR', 'CHARMANDER', 'PIKACHU', 'MEWTWO', 'CHIKORITA', 'LUGIA', 'TREECKO', 'RAYQUAZA', 'TURTWIG', 'SNIVY',
         'THUNDERBOLT', 'FLAMETHROWER', 'EARTHQUAKE', 'HYPER BEAM', 'TACKLE', 'MASTER BALL', 'RARE CANDY', 'LEFTOVERS',
         'FULL RESTORE', 'NUGGET']
NAMES += [n.title() for n in NAMES]
FOUND = re.compile(r'\b(%s)\b' % '|'.join(re.escape(n) for n in NAMES))
GUIDE_NAMES = {'title', 'where', 'name', 'foe', 'places'}     # a guide's bosses and places


def walk(v, path, guide):
    if isinstance(v, dict):
        for k, x in v.items():
            if not (guide and k in GUIDE_NAMES):
                walk(x, path + '.' + k, guide)
    elif isinstance(v, list):
        for i, x in enumerate(v):
            walk(x, '%s[%d]' % (path, i), guide)
    elif isinstance(v, str) and FOUND.search(v):
        sys.exit('pokedaisy: %s%s names %s, one of the games\' own' % (f, path, FOUND.search(v).group()))


n = 0
for f in sys.argv[1:]:
    d = json.load(open(f, encoding='utf-8'))
    base = os.path.basename(f)
    if base == 'text.json':
        continue
    walk(d, '', base == 'guide.json' or d.get('kind') == 'nds-pokemon-guide')
    n += 1
print('pokedaisy: %d files parse, none names the games\' content (%d text.json kept whole)' %
      (n, sum(os.path.basename(f) == 'text.json' for f in sys.argv[1:])))
EOF

# ... and the check of the player's ROMs, then the ROM reader on them (kiwami/tests/gbpk-roms.sh; skipped without
# ~/gb-companion/roms, as here), then the check on made-up folders: none is skipped; an empty one fails naming the six required files; a file
# with a listed name and other bytes is a wrong file, and six of them can't pass; and, with a made-up list, a file
# found by its SHA-1 under its name or another, an optional one missing, an unlisted file named, the GBA folder's
# files looked for without failing the check
kiwami/tests/gbpk-roms.sh
T=$(mktemp -d)
rr() { GB_ROMS=$T/$1 GBA_ROMS=$T/${2:-none} NDS_ROMS=$T/none kiwami/tests/gbpk-roms.sh > "$T/out" 2>&1; }
has() { grep -qxF -- "gbpk-roms: $1" "$T/out" || { echo "gbpk-roms: no line '$1' in: $(cat "$T/out")" >&2; exit 1; }; }
rr none || { echo "gbpk-roms: failed with no folder" >&2; exit 1; }
has "skipped, no ROMs in $T/none (GB_ROMS)"
mkdir "$T/empty" "$T/six" "$T/ok" "$T/gba"
! rr empty || { echo "gbpk-roms: an empty folder passed" >&2; exit 1; }
awk '!/^#/ && $2 == "gb" && $3 == "required" { n = $0; for (i = 1; i <= 4; i++) sub(/^[^ ]+ +/, "", n); print n }' \
    kiwami/tests/gbpk/roms.sha1 > "$T/required"
[ "$(wc -l < "$T/required")" = 6 ] || { echo "gbpk-roms: roms.sha1 hasn't six required files" >&2; exit 1; }
while IFS= read -r n; do has "$n: missing"; done < "$T/required"
has "0 of 6 required files ok in $T/empty"
has "no folder $T/none (GBA_ROMS): its 11 files not looked for"
has "no folder $T/none (NDS_ROMS): its 7 files not looked for"
while IFS= read -r n; do echo "$n" > "$T/six/$n"; done < "$T/required"
! rr six || { echo "gbpk-roms: six made-up files passed" >&2; exit 1; }
while IFS= read -r n; do has "$n: wrong file: sha1 $(echo "$n" | sha1sum | cut -c1-40)"; done < "$T/required"
printf a > "$T/ok/Made Up (USA).gb"; printf b > "$T/ok/b.GBC"; printf c > "$T/ok/c.gbc"; printf d > "$T/ok/readme.txt"
printf e > "$T/gba/e.gba"; printf f > "$T/gba/Made Up GBA (USA).gba"
h() { printf "$1" | sha1sum | cut -c1-40; }
{ echo '# made up'
  echo "$(h a) gb required made Made Up (USA).gb"
  echo "$(h b) gb required made Made Up (Europe).gbc"
  echo "$(h z) gb optional made Made Up (Japan).gbc"
  echo "$(h e) gba optional made Made Up Advance (USA).gba"
  echo "$(h z) gba optional made Made Up GBA (USA).gba"; } > "$T/list"
GBPK_ROMS_LIST=$T/list rr ok gba || { echo "gbpk-roms: the made-up list's files failed: $(cat "$T/out")" >&2; exit 1; }
has "Made Up (USA).gb: ok"
has "Made Up (Europe).gbc: ok (named b.GBC)"
has "Made Up (Japan).gbc: missing"
has "not listed, left alone: c.gbc (sha1 $(h c))"
has "2 of 2 required files ok in $T/ok"
has "Made Up Advance (USA).gba: ok (named e.gba)"
has "Made Up GBA (USA).gba: wrong file: sha1 $(h f)"
! grep -q 'readme' "$T/out" || { echo "gbpk-roms: looked at readme.txt" >&2; exit 1; }
# a ROM hack (the list's .patches) against its retail ROM: changed only inside its ranges passes; changed outside them,
# another size, or no retail ROM to compare with fails
mkdir "$T/hack"
mk() { python3 -I -c 'import sys; b = bytearray(256); [b.__setitem__(int(o, 16), 0xFF) for o in sys.argv[2:]]; open(sys.argv[1], "wb").write(b)' "$@"; }
mk "$T/hack/retail.gb"; mk "$T/hack/fast.gb" 10 11 1f 80; mk "$T/hack/bad.gb" 10 20; head -c 255 "$T/hack/retail.gb" > "$T/hack/short.gb"
f() { sha1sum < "$T/hack/$1" | cut -c1-40; }
# hk <hack file>: the list with retail.gb and that file, and that file as a hack of retail.gb, then run
hk() {
    { echo "$(f retail.gb) gb required made Retail.gb"; echo "$(f $1) gb optional made Fast.gb"; } > "$T/hack.sha1"
    echo "$(f $1) $(f retail.gb) 0010-0020 0080-0081" > "$T/hack.patches"
    GBPK_ROMS_LIST=$T/hack.sha1 rr hack
}
hk fast.gb || { echo "gbpk-roms: a hack inside its patch failed: $(cat "$T/out")" >&2; exit 1; }
has "fast.gb: differs from retail.gb only in its patch (4 bytes in 0010-0020 0080-0081)"
! hk bad.gb || { echo "gbpk-roms: a hack changed outside its patch passed" >&2; exit 1; }
has "bad.gb: differs from retail.gb outside its patch, first at 0x0020"
! hk short.gb || { echo "gbpk-roms: a hack of another size passed" >&2; exit 1; }
has "short.gb: not the size of retail.gb"
mv "$T/hack/retail.gb" "$T/retail.gb"
! GBPK_ROMS_LIST=$T/hack.sha1 rr hack || { echo "gbpk-roms: a hack with no retail ROM passed" >&2; exit 1; }
has "short.gb: no ROM with sha1 $(sha1sum < "$T/retail.gb" | cut -c1-40) to compare it with"
rm -rf "$T"
echo "gbpk-roms: checked on made-up folders"

# RomM's game list in banana-sync: 200 a page and its progress, each game kept small, against a large made-up library
kiwami/tests/romm-list.sh

# Home server > Show sync times: banana-sync's phases, its times file and the report's times
kiwami/tests/sync-times.sh

# Show sync times in banana-ui's Settings, headless, when banana-ui is built: the line under Sync now's bar and the row
if [ -x ui/banana-ui ]; then kiwami/tests/sync-times-ui.sh; else echo "sync-times-ui: skipped, no ui/banana-ui (make ui)"; fi

# Tailscale in banana-ui's Settings, headless, when banana-ui is built (CI builds it before)
if [ -x ui/banana-ui ]; then kiwami/tests/tailscale-ui.sh; else echo "tailscale-ui: skipped, no ui/banana-ui (make ui)"; fi

# .7z games on the cartridge systems' shelves, headless, when banana-ui is built
if [ -x ui/banana-ui ]; then kiwami/tests/7z-ui.sh; else echo "7z-ui: skipped, no ui/banana-ui (make ui)"; fi

# the fork's cores in Settings > Other systems, headless, when banana-ui is built
if [ -x ui/banana-ui ]; then kiwami/tests/cores-ui.sh; else echo "cores-ui: skipped, no ui/banana-ui (make ui)"; fi

# the RetroAchievements group in Settings and the grid scrolling to it, headless, when banana-ui is built
if [ -x ui/banana-ui ]; then kiwami/tests/ra-ui.sh; else echo "ra-ui: skipped, no ui/banana-ui (make ui)"; fi

# the Game Boy Pokémon companion in banana-ui, headless, when banana-ui is built: its Settings rows, the game panel's
# pages for Crystal fed PokeDaisy's captures, a ROM it doesn't know, upstream's panel after it
if [ -x ui/banana-ui ]; then kiwami/tests/gbpk-ui.sh; else echo "gbpk-ui: skipped, no ui/banana-ui (make ui)"; fi

# post-build.sh vetoes tailscaled's state and RetroArch master's credentials file in upstream's share script, each
# once and the same on a second run, and stops the build when the line isn't there
T=$(mktemp -d)
mkdir -p "$T/usr/libexec/banana" "$T/etc/banana"
cp system/rootfs/usr/libexec/banana/share "$T/usr/libexec/banana/"
cp system/rootfs/etc/banana/systems.conf system/rootfs/etc/banana/credits.txt "$T/etc/banana/"
sh kiwami/post-build.sh "$T" rgds
SH=$T/usr/libexec/banana/share
for v in '\*tailscaled\.state' '\*retroarch-keychain\.\*'; do
    grep -q "^	veto files = /.*/$v/" "$SH" || { echo "post-build: no veto for $v" >&2; exit 1; }
    [ "$(grep '^	veto files = ' "$SH" | grep -o "$v/" | wc -l)" = 1 ] || { echo "post-build: $v vetoed more than once" >&2; exit 1; }
done
cp "$SH" "$T/share.once"

# ... and adds 7z to N64's ext line, so that it and upstream's six cartridge systems each list it once, and nowhere
# else, the same on a second run, and stops the build when N64 has no ext line
C=$T/etc/banana/systems.conf
ext() { awk -v s="[$1]" '/^\[/ { in_s = ($0 == s) } in_s && /^ext *=/ { print }' "$C"; }
for s in gba gb gbc nes snes megadrive n64; do
    ext $s | grep -q ' 7z$' || { echo "post-build: no 7z on [$s]'s ext line: $(ext $s)" >&2; exit 1; }
    [ "$(ext $s | tr ' ' '\n' | grep -cx 7z)" = 1 ] || { echo "post-build: 7z more than once on [$s]: $(ext $s)" >&2; exit 1; }
done
for s in nds psx; do
    ! ext $s | grep -q '7z' || { echo "post-build: 7z on [$s]: $(ext $s)" >&2; exit 1; }
done
cp "$C" "$T/systems.once"
sh kiwami/post-build.sh "$T" rgds
cmp -s "$C" "$T/systems.once" || { echo "post-build: a second run changed systems.conf" >&2; exit 1; }
cmp -s "$SH" "$T/share.once" || { echo "post-build: a second run changed the share script" >&2; exit 1; }
awk '/^\[/ { in_s = ($0 == "[n64]") } !(in_s && /^ext *=/)' system/rootfs/etc/banana/systems.conf > "$C"
! sh kiwami/post-build.sh "$T" rgds 2>"$T/err" || { echo "post-build: no failure without [n64]'s ext line" >&2; exit 1; }
grep -q '\[n64\]' "$T/err" || { echo "post-build: the failure doesn't name [n64]: $(cat "$T/err")" >&2; exit 1; }
[ ! -e "$C.tmp" ] || { echo "post-build: left its temporary behind" >&2; exit 1; }
cp system/rootfs/etc/banana/systems.conf "$C"

# ... and adds PokeDaisy's credit (its GPL-3.0 7(b) line, kiwami/GB-POKEMON.md) to the credits, as the last name of
# upstream's [Thanks], once (the same after the second run above), and stops the build when there's no [Thanks]
K=$T/etc/banana/credits.txt
P='PokeDaisy / Lidor Itzhari = Game Boy Pokémon companion. Based on PokeDaisy by Lidor Itzhari - https://github.com/lidor30/pokedaisy'
[ "$(grep -cxF "$P" "$K")" = 1 ] || { echo "post-build: PokeDaisy's credit $(grep -cxF "$P" "$K") times in credits.txt" >&2; exit 1; }
[ "$(awk '/^\[/ { in_t = ($0 == "[Thanks]") } in_t && NF && !/^[#[]/ { last = $0 } END { print last }' "$K")" = "$P" ] ||
    { echo "post-build: PokeDaisy's credit isn't [Thanks]' last name" >&2; exit 1; }
grep -vxF "$P" "$K" | cmp -s - system/rootfs/etc/banana/credits.txt || { echo "post-build: credits.txt changed beyond PokeDaisy's line" >&2; exit 1; }
sed '/^\[Thanks\]$/d' system/rootfs/etc/banana/credits.txt > "$K"
! sh kiwami/post-build.sh "$T" rgds 2>"$T/err" || { echo "post-build: no failure without [Thanks]" >&2; exit 1; }
grep -q 'no \[Thanks\] section' "$T/err" || { echo "post-build: the failure doesn't name [Thanks]: $(cat "$T/err")" >&2; exit 1; }
[ ! -e "$K.tmp" ] || { echo "post-build: left the credits' temporary behind" >&2; exit 1; }
cp system/rootfs/etc/banana/credits.txt "$K"

sed -i '/veto files/d' "$T/usr/libexec/banana/share"
! sh kiwami/post-build.sh "$T" rgds 2>/dev/null || { echo "post-build: no failure without the veto line" >&2; exit 1; }
rm -rf "$T"
echo "post-build: veto, 7z and PokeDaisy's credit checked"

# ... and, with BR2_PACKAGE_KIWAMI_OTA_OFF in the board's .config, sends the update script's check, download and
# beta-login to ota-off (once, the same on a second run, nothing without the option, a stop without upstream's case
# line); ota-off then says why where Settings shows each one's answer (list and apply, the card's packages, stay upstream's)
T=$(mktemp -d)
mkdir -p "$T/usr/libexec/banana" "$T/usr/libexec/kiwami" "$T/etc/banana" "$T/data/updates" "$T/run"
cp system/rootfs/usr/libexec/banana/share system/rootfs/usr/libexec/banana/update "$T/usr/libexec/banana/"
cp kiwami/rootfs/usr/libexec/kiwami/ota-off "$T/usr/libexec/kiwami/"
cp system/rootfs/etc/banana/systems.conf system/rootfs/etc/banana/credits.txt "$T/etc/banana/"
U=$T/usr/libexec/banana/update
echo '# BR2_PACKAGE_KIWAMI_OTA_OFF is not set' > "$T/off.config"
echo 'BR2_PACKAGE_KIWAMI_OTA_OFF=y' > "$T/on.config"
BR2_CONFIG=$T/off.config sh kiwami/post-build.sh "$T" rgds
cmp -s "$U" system/rootfs/usr/libexec/banana/update || { echo "ota-off: the update script changed with the option off" >&2; exit 1; }
BR2_CONFIG=$T/on.config sh kiwami/post-build.sh "$T" rgds
BR2_CONFIG=$T/on.config sh kiwami/post-build.sh "$T" rgds
[ "$(grep -c 'ota-off' "$U")" = 1 ] || { echo "ota-off: hooked $(grep -c 'ota-off' "$U") times into the update script" >&2; exit 1; }
[ "$(grep -A1 '^case \$1 in$' "$U" | tail -n 1 | cut -c1-26)" = 'check|download|beta-login)' ] ||
    { echo "ota-off: its case isn't the first: $(grep -A1 '^case \$1 in$' "$U" | tail -n 1)" >&2; exit 1; }
sh -n "$U"
# (the image's paths, here in $T)
sed -i "s|/usr/libexec/kiwami/ota-off|$T/usr/libexec/kiwami/ota-off|" "$U"
upd() { BANANA_DATA=$T/data BANANA_RUN=$T/run BANANA_BOARD=rgds BANANA_VERSION=0.5.0.1 sh "$U" "$@"; }
! upd check > "$T/out" 2>&1 || { echo "ota-off: the check succeeded" >&2; exit 1; }
grep -qx 'state=error' "$T/data/updates/.check" && grep -q '^error=updates over the air are off in this build of the kiwami fork' "$T/data/updates/.check" ||
    { echo "ota-off: the check's answer: $(cat "$T/data/updates/.check")" >&2; exit 1; }
rm -f "$T/data/updates/.check"
upd check --daily || { echo "ota-off: the daily check failed" >&2; exit 1; }
[ ! -e "$T/data/updates/.check" ] || { echo "ota-off: the daily check wrote an answer" >&2; exit 1; }
! upd download 2>/dev/null || { echo "ota-off: the download succeeded" >&2; exit 1; }
grep -q '^failed: updates over the air are off' "$T/run/update-download" || { echo "ota-off: the download's answer: $(cat "$T/run/update-download")" >&2; exit 1; }
! echo secret | upd beta-login > "$T/out" || { echo "ota-off: the beta login succeeded" >&2; exit 1; }
grep -q 'banana-rgds-<version>.update.tar' "$T/out" || { echo "ota-off: the beta login's answer: $(cat "$T/out")" >&2; exit 1; }
cp system/rootfs/usr/libexec/banana/update "$U"; sed -i 's/^case \$1 in$/case "$1" in/' "$U"
! BR2_CONFIG=$T/on.config sh kiwami/post-build.sh "$T" rgds 2>/dev/null || { echo "ota-off: no failure without upstream's case line" >&2; exit 1; }
rm -rf "$T"
echo "post-build: over-the-air updates off checked"

# the top Makefile's board build (kiwami/build.sh) in each mode, against made-up outputs
kiwami/tests/build-mode.sh

# every board configures with the fork's tree and options over upstream's, keeping all of the fork's options, its
# packages built in parallel among them; and with KIWAMI_SERIAL=1, without per-package directories, as upstream's
for b in rgds rgds_plus universal; do
    O=${TMPDIR:-/tmp}/kiwami-config-$b
    make -s -C "$BR" BR2_EXTERNAL="$PWD/buildroot:$PWD/kiwami" O="$O" banana_${b}_defconfig
    KIWAMI_SERIAL= kiwami/configure.sh "$BR" "$O"
    grep -qx 'BR2_PER_PACKAGE_DIRECTORIES=y' "$O/.config" || { echo "$b: no per-package directories" >&2; exit 1; }
    echo "$b: configured"
done
O=${TMPDIR:-/tmp}/kiwami-config-rgds_plus-serial
make -s -C "$BR" BR2_EXTERNAL="$PWD/buildroot:$PWD/kiwami" O="$O" banana_rgds_plus_defconfig
KIWAMI_SERIAL=1 kiwami/configure.sh "$BR" "$O"
! grep -q '^BR2_PER_PACKAGE_DIRECTORIES=y' "$O/.config" || { echo "rgds_plus: per-package directories with KIWAMI_SERIAL=1" >&2; exit 1; }
echo "rgds_plus: configured with KIWAMI_SERIAL=1, without per-package directories"
# (a bad value stops, taking the defconfig's .config with it)
! KIWAMI_SERIAL=yes kiwami/configure.sh "$BR" "$O" 2>/dev/null || { echo "configure.sh: KIWAMI_SERIAL=yes accepted" >&2; exit 1; }
[ ! -e "$O/.config" ] || { echo "configure.sh: KIWAMI_SERIAL=yes left $O/.config" >&2; exit 1; }
rm -rf "$O"

# RetroArch's master as each board resolves it with the fork's options (the pin, the fork's hash file, patches and
# options), and upstream's v1.22.2 with the option off; first the guard, on copies of upstream's RetroArch (each move
# caught), then on upstream's own (it hasn't moved under the pin)
kiwami/tests/retroarch-pin.sh --self-test
kiwami/tests/retroarch-pin.sh "$BR" ${TMPDIR:-/tmp}/kiwami-config-rgds ${TMPDIR:-/tmp}/kiwami-config-rgds_plus \
    ${TMPDIR:-/tmp}/kiwami-config-universal
