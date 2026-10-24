#!/bin/sh
# RetroArch's master behind BR2_PACKAGE_KIWAMI_RETROARCH_MASTER, as Buildroot resolves it in outputs configured with the
# fork's options (kiwami/tests/ci.sh's): on, the pin (kiwami/local.mk), only the fork's hash file, the fork's patches
# instead of upstream's (the patch step's folders, from make -n: nothing is downloaded or built) and the fork's
# configure options after upstream's; off (a copy of the first output), upstream's RetroArch as it is. The output's own
# local.mk is read both ways. First, a guard: upstream's RetroArch (its version, its joypads' commit and its patches)
# is the one the pin was last checked against. Run by kiwami/tests/ci.sh:
#   kiwami/tests/retroarch-pin.sh <buildroot dir> <output dir>...
#   kiwami/tests/retroarch-pin.sh --self-test     the guard on copies of upstream's RetroArch, each move caught
set -e
[ "$1" = --self-test ] || { BR=$(cd "$1" && pwd); shift; }
cd "$(dirname "$0")/../.."
K=$PWD/kiwami
PIN=71f521e726c2b1886ee7a2484195ab63abc92127
OPTS='--disable-keychain --disable-mcp --disable-retronfs --disable-retrosmb --disable-smbclient --disable-rav1
    --disable-rvp9 --disable-rmp4 --disable-rwebm --disable-rmpeg1 --disable-raac --disable-ropus --disable-rvorbis
    --disable-rmp3 --disable-rmodtracker --disable-rrar --disable-gamemode'
OPTS=$(echo $OPTS)
HOTKEYS='video_filter_toggle headset_recenter laser_pointer_toggle headset_view_toggle'
fail() { echo "retroarch-pin: FAIL: $*" >&2; exit 1; }

# Upstream's RetroArch as it was when the fork's pin was last checked against it. When upstream moves one of these,
# the fork's pin hides it: look at what upstream changed, then rebase or drop kiwami/patches/retroarch/, move the
# pin (kiwami/local.mk) or turn it off, and update the stamp here.
UP_VERSION=v1.22.2
UP_JOYPADS=f3b62b847ed21939b668b897b18bbc1defbaae05
UP_PATCHES=0a6ab4470613449b3910ca1788386257a32381a6aae1efeef0ae909be1c7f94f
# guard: upstream's RetroArch in $KIWAMI_TREE (the repository by default) against the stamps
guard() {
    t=${KIWAMI_TREE:-.}/buildroot
    v=$(sed -n 's/^RETROARCH_VERSION = //p' "$t/package/retroarch/retroarch.mk")
    j=$(sed -n 's/^RETROARCH_JOYPADS_VERSION = //p' "$t/package/retroarch/retroarch.mk")
    [ "$v" = $UP_VERSION ] || fail "upstream's RETROARCH_VERSION is $v, the pin was checked against $UP_VERSION:" \
        "see what upstream changed (buildroot/package/retroarch/, buildroot/patches/retroarch/) and the stamps in $0"
    [ "$j" = $UP_JOYPADS ] || fail "upstream's RETROARCH_JOYPADS_VERSION is $j, the pin was checked against" \
        "$UP_JOYPADS: kiwami/patches/retroarch/retroarch.hash lists upstream's joypad tarball, so add the new one's" \
        "sha256 there, and see the stamps in $0"
    f=$(cd "$t" && { find patches/retroarch -type f; find package/retroarch -name '*.patch'; } | LC_ALL=C sort)
    [ "$(cd "$t" && echo "$f" | xargs sha256sum | sha256sum | cut -c1-64)" = $UP_PATCHES ] ||
        fail "upstream's RetroArch patches changed since the pin was checked (now $(echo $f)): the fork applies" \
            "kiwami/patches/retroarch/ instead, so see what upstream changed and the stamps in $0"
}

# --self-test: the guard on copies of upstream's RetroArch: an unchanged copy passes, and each move fails with its
# own message
if [ "$1" = --self-test ]; then
    T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
    copy() {
        rm -rf "$T/buildroot"; mkdir -p "$T/buildroot/package" "$T/buildroot/patches"
        cp -a buildroot/package/retroarch "$T/buildroot/package/"; cp -a buildroot/patches/retroarch "$T/buildroot/patches/"
    }
    # moved <what> <words the message has>: the guard on the changed copy fails, naming what moved
    moved() {
        if (KIWAMI_TREE=$T; guard) 2> "$T/err"; then fail "self-test: the guard missed $1"; fi
        grep -qF -- "$2" "$T/err" || fail "self-test: $1: the message doesn't say '$2': $(cat "$T/err")"
    }
    copy; (KIWAMI_TREE=$T; guard) || fail "self-test: the guard fails on an unchanged copy"
    copy; sed -i 's/^RETROARCH_VERSION = .*/RETROARCH_VERSION = v1.23.0/' "$T/buildroot/package/retroarch/retroarch.mk"
    moved "a new version" "RETROARCH_VERSION is v1.23.0"
    copy; sed -i 's/^RETROARCH_JOYPADS_VERSION = .*/RETROARCH_JOYPADS_VERSION = 0123456789abcdef0123456789abcdef01234567/' \
        "$T/buildroot/package/retroarch/retroarch.mk"
    moved "a new joypads commit" "RETROARCH_JOYPADS_VERSION is 0123456789abcdef0123456789abcdef01234567"
    copy; echo x > "$T/buildroot/patches/retroarch/0004-x.patch"
    moved "a new patch" "patches/retroarch/0004-x.patch"
    copy; echo x > "$T/buildroot/package/retroarch/x.patch"
    moved "a patch in the package's folder" "package/retroarch/x.patch"
    echo "retroarch-pin: self-test: the guard caught all 4 moves"
    exit 0
fi
guard
echo "retroarch-pin: upstream's RetroArch is the one the pin was checked against"
VARS='RETROARCH_VERSION RETROARCH_HASH_FILES RETROARCH_CONF_OPTS RETROARCH_TARGET_PATCH KIWAMI_TEST_LOCAL_MK
    RETROARCH_POST_INSTALL_TARGET_HOOKS KIWAMI_RETROARCH_HOTKEYS'
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
        # FLAC stays: master's CHD reader decodes CD-audio hunks with it
        case " $c " in *" --disable-rflac "*) fail "${O##*/}: RETROARCH_CONF_OPTS turns FLAC off: $c" ;; esac
        [ "$(dirs "$p")" = "$(printf '%s\n' "$PWD/buildroot/package/retroarch" "$K/patches/retroarch" | sort)" ] ||
            fail "${O##*/}: the patch step applies $(dirs "$p" | tr '\n' ' ')"
        [ "$(hashes "$p")" = "$K/patches/retroarch/retroarch.hash" ] || fail "${O##*/}: the downloads are checked against $(hashes "$p" | tr '\n' ' ')"
        # master's four hotkeys, written for run-retroarch at RetroArch's install
        [ "$(var KIWAMI_RETROARCH_HOTKEYS)" = "$HOTKEYS" ] || fail "${O##*/}: KIWAMI_RETROARCH_HOTKEYS is '$(var KIWAMI_RETROARCH_HOTKEYS)'"
        case " $(var RETROARCH_POST_INSTALL_TARGET_HOOKS) " in *" KIWAMI_RETROARCH_HOTKEYS_INSTALL "*) ;;
        *) fail "${O##*/}: no hotkeys hook in RETROARCH_POST_INSTALL_TARGET_HOOKS" ;; esac
    else
        [ "$v" = v1.22.2 ] || fail "off: RETROARCH_VERSION is '$v', not upstream's v1.22.2"
        [ "$h" = "$PWD/buildroot/package/retroarch/retroarch.hash $PWD/buildroot/patches/retroarch/retroarch.hash" ] ||
            fail "off: RETROARCH_HASH_FILES is '$h', not Buildroot's default"
        case " $c " in *" --disable-keychain "*) fail "off: RETROARCH_CONF_OPTS has the fork's options: $c" ;; esac
        [ "$ON_OPTS" = "$c $OPTS" ] || fail "off: RETROARCH_CONF_OPTS isn't the on one's without the fork's: $c"
        [ "$(dirs "$p")" = "$(printf '%s\n' "$PWD/buildroot/package/retroarch" "$PWD/buildroot/patches/retroarch" | sort)" ] ||
            fail "off: the patch step applies $(dirs "$p" | tr '\n' ' ')"
        case " $(var RETROARCH_POST_INSTALL_TARGET_HOOKS) " in *" KIWAMI_RETROARCH_HOTKEYS_INSTALL "*)
            fail "off: the hotkeys hook is in RETROARCH_POST_INSTALL_TARGET_HOOKS" ;; esac
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
