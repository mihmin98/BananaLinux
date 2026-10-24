#!/bin/sh
# RetroArch master's hotkeys that v1.22.2 doesn't have, for Settings > Other systems > RetroArch hotkeys Off: run at
# RetroArch's install (a post-install hook in external.mk, with BR2_PACKAGE_KIWAMI_RETROARCH_MASTER), it writes the
# names for run-retroarch (a kiwami: hook adds them to upstream's HOTKEYS list).
#   retroarch-hotkeys.sh <configuration.c> <run-retroarch> <out file> <name>...
# Off promises no F + button shortcut but the two needed to get out (F + X, F + Start), so a hotkey of RetroArch's that
# neither upstream's list nor the names cover would still work there. The build stops instead, naming it: when master
# has a hotkey neither names, when a name is no hotkey of this RetroArch's, or when upstream's list has one already.
set -e
SRC=$1 RUN=$2 OUT=$3; shift 3
# RetroArch's hotkeys, but the three Off keeps (enable_hotkey, menu_toggle, exit_emulator)
ra=$(sed -n 's/^ *DECLARE_META_BIND( *[0-9]*, *\([a-z0-9_]*\),.*/\1/p' "$SRC" |
    grep -vxE 'enable_hotkey|menu_toggle|exit_emulator' | sort -u)
up=$(sed -n '/^HOTKEYS="/,/"$/p' "$RUN" | sed 's/^HOTKEYS=//' | tr -d '"' | tr ' ' '\n' | grep . | sort -u)
[ -n "$ra" ] || { echo "kiwami/retroarch-hotkeys.sh: no DECLARE_META_BIND hotkeys in $SRC" >&2; exit 1; }
[ -n "$up" ] || { echo "kiwami/retroarch-hotkeys.sh: no HOTKEYS list in $RUN" >&2; exit 1; }
bad=
for h in $ra; do
    case " $* " in *" $h "*) continue ;; esac
    if echo "$up" | grep -qx "$h"; then continue; fi
    echo "kiwami/retroarch-hotkeys.sh: master's hotkey $h isn't in run-retroarch's list or KIWAMI_RETROARCH_HOTKEYS: add it to KIWAMI_RETROARCH_HOTKEYS in kiwami/external.mk" >&2
    bad=1
done
for h; do
    if ! echo "$ra" | grep -qx "$h"; then
        echo "kiwami/retroarch-hotkeys.sh: $h is no hotkey of this RetroArch's: drop it from KIWAMI_RETROARCH_HOTKEYS" >&2
        bad=1
    fi
    if echo "$up" | grep -qx "$h"; then
        echo "kiwami/retroarch-hotkeys.sh: run-retroarch lists $h now: drop it from KIWAMI_RETROARCH_HOTKEYS" >&2
        bad=1
    fi
done
[ -z "$bad" ] || exit 1
mkdir -p "${OUT%/*}"
printf '%s\n' "$@" > "$OUT.tmp"
mv "$OUT.tmp" "$OUT"
