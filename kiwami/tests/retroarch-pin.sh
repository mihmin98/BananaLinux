#!/bin/sh
# RetroArch's master behind BR2_PACKAGE_KIWAMI_RETROARCH_MASTER, as Buildroot resolves it in outputs configured with the
# fork's options (kiwami/tests/ci.sh's): on, the pin (kiwami/local.mk), only the fork's hash file, the fork's patches
# instead of upstream's (the patch step's folders, from make -n: nothing is downloaded or built) and the fork's
# configure options after upstream's; off (a copy of the first output), upstream's RetroArch as it is. The output's own
# local.mk is read both ways. Run by kiwami/tests/ci.sh:
#   kiwami/tests/retroarch-pin.sh <buildroot dir> <output dir>...
set -e
BR=$(cd "$1" && pwd); shift
cd "$(dirname "$0")/../.."
K=$PWD/kiwami
PIN=71f521e726c2b1886ee7a2484195ab63abc92127
OPTS='--disable-keychain'
fail() { echo "retroarch-pin: FAIL: $*" >&2; exit 1; }
VARS='RETROARCH_VERSION RETROARCH_HASH_FILES RETROARCH_CONF_OPTS RETROARCH_TARGET_PATCH KIWAMI_TEST_LOCAL_MK'
# resolve <output>: the variables as Buildroot resolves them, read once (each make parses all of Buildroot, ~10 s),
# and the patch step's commands, its downloads' and its patches' (make -n on its stamp)
resolve() {
    R=$(make -s -C "$BR" O="$1" printvars VARS="$VARS")
    P=$(make -n -s -C "$BR" O="$1" "$(var RETROARCH_TARGET_PATCH)" 2>&1)
}
var() { echo "$R" | sed -n "s/^$1=//p"; }
# the folders the patch step applies, and the hash files the downloads are checked against, one a line
dirs() { echo "$1" | grep 'apply-patches\.sh' | sed -n 's/.* \([^ ]*\) \\\*\.patch.*/\1/p' | sort -u; }
hashes() { echo "$1" | grep -o "dl-wrapper.*" | grep -o " -H '[^']*'" | sed "s/ -H '\(.*\)'/\1/" | sort -u; }

# check <output> on|off: the output's own local.mk (Buildroot's default override file) read too
check() {
    O=$1
    echo 'KIWAMI_TEST_LOCAL_MK = 1' > "$O/local.mk"
    resolve "$O"
    rm -f "$O/local.mk"
    v=$(var RETROARCH_VERSION) h=$(var RETROARCH_HASH_FILES) c=$(var RETROARCH_CONF_OPTS) p=$P
    [ "$(var KIWAMI_TEST_LOCAL_MK)" = 1 ] || fail "${O##*/} ($2): the output's own local.mk isn't read"
    if [ $2 = on ]; then
        [ "$v" = $PIN ] || fail "${O##*/}: RETROARCH_VERSION is '$v', not $PIN"
        [ "$h" = "$K/patches/retroarch/retroarch.hash" ] || fail "${O##*/}: RETROARCH_HASH_FILES is '$h'"
        case " $c" in *" $OPTS") ;; *) fail "${O##*/}: RETROARCH_CONF_OPTS doesn't end with the fork's '$OPTS': $c" ;; esac
        [ "$(dirs "$p")" = "$(printf '%s\n' "$PWD/buildroot/package/retroarch" "$K/patches/retroarch" | sort)" ] ||
            fail "${O##*/}: the patch step applies $(dirs "$p" | tr '\n' ' ')"
        [ "$(hashes "$p")" = "$K/patches/retroarch/retroarch.hash" ] || fail "${O##*/}: the downloads are checked against $(hashes "$p" | tr '\n' ' ')"
    else
        [ "$v" = v1.22.2 ] || fail "off: RETROARCH_VERSION is '$v', not upstream's v1.22.2"
        [ "$h" = "$PWD/buildroot/package/retroarch/retroarch.hash $PWD/buildroot/patches/retroarch/retroarch.hash" ] ||
            fail "off: RETROARCH_HASH_FILES is '$h', not Buildroot's default"
        case " $c " in *" --disable-keychain "*) fail "off: RETROARCH_CONF_OPTS has the fork's options: $c" ;; esac
        [ "$ON_OPTS" = "$c $OPTS" ] || fail "off: RETROARCH_CONF_OPTS isn't the on one's without the fork's: $c"
        [ "$(dirs "$p")" = "$(printf '%s\n' "$PWD/buildroot/package/retroarch" "$PWD/buildroot/patches/retroarch" | sort)" ] ||
            fail "off: the patch step applies $(dirs "$p" | tr '\n' ' ')"
    fi
}

for O in "$@"; do
    O=$(cd "$O" && pwd)
    check "$O" on
    echo "retroarch-pin: ${O##*/} pinned"
done
# off: the first output again, the option taken out
O=$(cd "$1" && pwd)
OFF=$O-off
rm -rf "$OFF"; cp -a "$O" "$OFF"
grep -qx 'BR2_PACKAGE_KIWAMI_RETROARCH_MASTER=y' "$OFF/.config" || fail "${O##*/} doesn't have the option on"
sed -i 's/^BR2_PACKAGE_KIWAMI_RETROARCH_MASTER=y$/# BR2_PACKAGE_KIWAMI_RETROARCH_MASTER is not set/' "$OFF/.config"
make -s -C "$BR" O="$OFF" olddefconfig > /dev/null
ON_OPTS=$c
check "$OFF" off
rm -rf "$OFF"
echo "retroarch-pin: off is upstream's"
