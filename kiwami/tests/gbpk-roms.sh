#!/bin/sh
# The Game Boy Pokémon companion's ROM tests on the player's own ROMs (kiwami/GB-POKEMON.md, "The ROMs for the tests").
# None is in the repository: kiwami/tests/gbpk/roms.sha1 lists what the tests expect, by SHA-1, and this checks the
# folders against it. GB_ROMS (default ~/gb-companion/roms) holds the Game Boy and Game Boy Color games, unpacked (.gb,
# .gbc); without that folder everything is skipped and the check succeeds (as in CI). Each listed file is a line: ok,
# ok (named <its name>) when it's there under another name, missing, or wrong file: sha1 <...> when a file has its name
# and other bytes (a European release, a hack, a bad dump). A required one missing or wrong fails the check. Files the
# list hasn't are named and left alone. A ROM hack read as the retail game it patches (roms.patches, beside the list)
# is compared with that game's ROM when both are there: the same size, and every byte that differs inside the patch's
# ranges, or the check fails. Then the Game Boy Advance and DS games are looked for the same way in GBA_ROMS (default
# ~/gba-companion/roms, .gba) and NDS_ROMS (default ~/nds-companion/roms, .nds), none of them failing it.
# Then the companion's ROM reader reads each listed release there and checks what it read (gbpk-roms.cpp), and writes
# the pictures and maps it decoded to GBPK_ROMS_OUT (default ui/tests/out/gbpk-roms/) as PNGs for looking at.
# Run by kiwami/tests/ci.sh, or by hand:
#   kiwami/tests/gbpk-roms.sh [<pokedaisy checkout>]   (GBPK_ROMS_LIST: another list, and its .patches, for ci.sh's
#                                                      made-up folders)
set -e
cd "$(dirname "$0")/../.."
LIST=${GBPK_ROMS_LIST:-$PWD/kiwami/tests/gbpk/roms.sha1}
PATCHES=${LIST%.sha1}.patches
GB=${GB_ROMS:-$HOME/gb-companion/roms}
GBA=${GBA_ROMS:-$HOME/gba-companion/roms}
NDS=${NDS_ROMS:-$HOME/nds-companion/roms}
if [ ! -d "$GB" ]; then
    echo "gbpk-roms: skipped, no ROMs in $GB (GB_ROMS)"
    exit 0
fi
T=$(mktemp -d)
trap 'rm -rf "$T"' EXIT

# hashes <folder> <extension>...: "<sha1> <file name>" for each file there with one of the extensions (any case)
hashes() {
    d=$1; shift
    for f in "$d"/*; do
        [ -f "$f" ] || continue
        e=$(printf '%s' "${f##*.}" | tr 'A-Z' 'a-z')
        for x in "$@"; do
            if [ "$e" = "$x" ]; then h=$(sha1sum < "$f"); printf '%s %s\n' "${h%% *}" "${f##*/}"; fi
        done
    done
}

# check <system> <folder> <variable> <strict> <extension>...: a line for each of the list's files of that system and
# for each file there it hasn't; with strict 1, the number of required files not ok in $T/bad
check() {
    sys=$1 dir=$2 var=$3 strict=$4; shift 4
    echo 0 > "$T/bad"
    if [ ! -d "$dir" ]; then
        echo "gbpk-roms: no folder $dir ($var): its $(awk -v s="$sys" '!/^#/ && NF >= 5 && $2 == s' "$LIST" | wc -l) files not looked for"
        return 0
    fi
    hashes "$dir" "$@" > "$T/hashes"
    awk -v sys="$sys" -v strict="$strict" -v hashes="$T/hashes" -v bad="$T/bad" -v folder="$dir" '
        BEGIN {
            while ((getline l < hashes) > 0) {
                h = substr(l, 1, 40); n = substr(l, 42)
                files[n] = h
                if (!(h in byhash)) byhash[h] = n
            }
        }
        /^#/ || NF < 5 || $2 != sys { next }
        {
            name = $0
            for (i = 1; i <= 4; i++) sub(/^[^ ]+ +/, "", name)
            listed[$1] = 1
            if ($1 in byhash) {
                if (byhash[$1] == name) st = "ok"; else st = "ok (named " byhash[$1] ")"
                if ($3 == "required") ok++
            } else {
                if (name in files) { st = "wrong file: sha1 " files[name]; named[name] = 1 } else st = "missing"
                if ($3 == "required" && strict) nok++
            }
            if ($3 == "required") req++
            print "gbpk-roms: " name ": " st
        }
        END {
            for (n in files) if (!(files[n] in listed) && !(n in named)) print "gbpk-roms: not listed, left alone: " n " (sha1 " files[n] ")"
            if (strict) print "gbpk-roms: " (ok + 0) " of " (req + 0) " required files ok in " folder
            print (nok + 0) > bad
        }' "$LIST"
}

# patches: each hack of $PATCHES that is in GB_ROMS against its retail ROM there (GB_ROMS' hashes in $T/hashes); one
# that differs elsewhere, or has no retail ROM to compare with, adds to nok
patches() {
    [ -f "$PATCHES" ] || return 0
    while read -r hack base ranges; do
        case $hack in ''|'#'*) continue ;; esac
        hf=$(awk -v h="$hack" 'substr($0, 1, 40) == h { print substr($0, 42); exit }' "$T/hashes")
        [ -n "$hf" ] || continue
        bf=$(awk -v h="$base" 'substr($0, 1, 40) == h { print substr($0, 42); exit }' "$T/hashes")
        if [ -z "$bf" ]; then
            echo "gbpk-roms: $hf: no ROM with sha1 $base to compare it with"; nok=$((nok + 1)); continue
        fi
        if [ "$(wc -c < "$GB/$hf")" != "$(wc -c < "$GB/$bf")" ]; then
            echo "gbpk-roms: $hf: not the size of $bf"; nok=$((nok + 1)); continue
        fi
        dr=
        for r in $ranges; do dr="$dr $((0x${r%-*})) $((0x${r#*-}))"; done
        res=$(cmp -l "$GB/$bf" "$GB/$hf" | awk -v r="$dr" '
            BEGIN { n = split(r, a, " ") }
            {
                o = $1 - 1; in_r = 0
                for (i = 1; i < n; i += 2) if (o >= a[i] + 0 && o < a[i + 1] + 0) in_r = 1
                if (!in_r && !out) out = sprintf("0x%04x", o)
                d++
            }
            END { print (d + 0) " " (out ? out : "-") }') || true
        if [ "${res#* }" = - ]; then
            echo "gbpk-roms: $hf: differs from $bf only in its patch (${res%% *} bytes in $ranges)"
        else
            echo "gbpk-roms: $hf: differs from $bf outside its patch, first at ${res#* }"; nok=$((nok + 1))
        fi
    done < "$PATCHES"
}

check gb "$GB" GB_ROMS 1 gb gbc
nok=$(cat "$T/bad")
patches
check gba "$GBA" GBA_ROMS 0 gba
check nds "$NDS" NDS_ROMS 0 nds
if [ "$nok" != 0 ]; then
    echo "gbpk-roms: FAIL $nok required files missing or wrong, or hacks not their patch, in $GB (${LIST#"$PWD"/} lists them)" >&2
    exit 1
fi

# the ROM reader on them (gbpk-roms.cpp), with the host's g++ and the vendored LZMA SDK as ci.sh builds them; with a
# PokeDaisy checkout as the argument, every species' gender ratio and growth rate against its tables too
make -s -C ui kiwami-c
${CXX:-g++} -std=c++17 -O2 -Wall -Werror -I ui/src -o "$T/gbpk-roms" kiwami/tests/gbpk-roms.cpp ui/src/kiwami/gbpk/*.cpp \
    ui/src/kiwami/json.cpp ui/src/kiwami/ra7z.cpp ui/src/zip.cpp ui/src/zipdir.cpp ui/src/nds/nitro.cpp ui/src/nds/model.cpp \
    ui/src/nds/logic.cpp ui/src/pokemon.cpp ui/src/pokemap.cpp $(find ui/src/kiwami/third_party/lzma -name '*.kiwami.o') -lz -lpthread
# (the pictures and maps it decodes, as PNGs for looking at: GBPK_ROMS_OUT, by default ui/tests/out/gbpk-roms/)
OUT=${GBPK_ROMS_OUT:-ui/tests/out/gbpk-roms}
mkdir -p "$OUT"
GBPK_ROMS_OUT=$OUT "$T/gbpk-roms" kiwami/rootfs/usr/share/banana/companions/gb-pokemon "$LIST" "$GB" ${1:+"$1"}
