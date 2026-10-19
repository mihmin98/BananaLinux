#!/bin/sh
# The fork's options over a board's .config, once upstream's defconfig has made it: the top Makefile runs this right
# after `make banana_<board>_defconfig`, and so does kiwami/tests/ci.sh.
#   configure.sh <buildroot dir> <output dir>
# The lists (rootfs overlays, post-build scripts, kernel fragments) get the fork's entry after upstream's, so
# upstream's own entries stay as its defconfig sets them. Other options come from configs/kiwami.fragment and replace
# upstream's value. Buildroot then resolves the dependencies, and an option of the fork's that didn't survive that
# fails here, as upstream's CI fails for its defconfigs.
set -e
K=$(cd "$(dirname "$0")" && pwd)
BR=$1 O=$2
C=$O/.config
[ -f "$C" ] || { echo "configure.sh: no $C (run the board's defconfig first)" >&2; exit 1; }

drop() { sed -i "/^$1=/d; /^# $1 is not set\$/d" "$C"; }
list() { sed -n "s/^$1=\"\(.*\)\"\$/\1/p" "$C"; }
# append <option> <entry>: the entry after the list's own, once
append() {
    old=$(list "$1")
    case " $old " in *" $2 "*) return 0 ;; esac
    drop "$1"
    echo "$1=\"${old:+$old }$2\"" >> "$C"
}
LISTS='BR2_ROOTFS_OVERLAY $(BR2_EXTERNAL_KIWAMI_PATH)/rootfs
BR2_ROOTFS_POST_BUILD_SCRIPT $(BR2_EXTERNAL_KIWAMI_PATH)/post-build.sh
BR2_LINUX_KERNEL_CONFIG_FRAGMENT_FILES $(BR2_EXTERNAL_KIWAMI_PATH)/linux.fragment'
echo "$LISTS" | while read -r o e; do append "$o" "$e"; done

OPTS=$(grep -E '^BR2_|^# BR2_.* is not set$' "$K/configs/kiwami.fragment" || true)
[ -z "$OPTS" ] || echo "$OPTS" | while IFS= read -r l; do
    o=${l#\# }; o=${o%%[= ]*}
    drop "$o"
    echo "$l" >> "$C"
done

make -s -C "$BR" O="$(cd "$O" && pwd)" olddefconfig

# every option of the fork's still there
echo "$LISTS" | while read -r o e; do
    case " $(list "$o") " in *" $e "*) ;; *) echo "configure.sh: $o lost $e" >&2; exit 1 ;; esac
done
[ -z "$OPTS" ] || echo "$OPTS" | while IFS= read -r l; do
    grep -qxF "$l" "$C" || { echo "configure.sh: Buildroot drops $l (a missing dependency?)" >&2; exit 1; }
done
