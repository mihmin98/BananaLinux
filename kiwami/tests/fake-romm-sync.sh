#!/bin/sh
# A stand-in for banana-sync in the fork's headless tests of its RomM pages (BANANA_SYNC names it): the kiwami verbs
# answer here, everything else goes to upstream's ui/tests/fake-sync.sh. Each kiwami call's arguments go to
# $FAKE_SYNC_LOG, as upstream's does with its own.
#   kiwami bios list         prints $FAKE_BIOS (printf's escapes: \t, \n), its last line the result ("ok <n>" or "error <why>")
#   kiwami bios get <id>...  prints $FAKE_BIOS_GET, else "ok <the ids' count> 0 0"; with $FAKE_BIOS_PACE (seconds), first
#                            writes banana-sync's progress for each id to <--run>/kiwami-bios.status ("getting <i> <n>
#                            <bytes done> <bytes in all> <name>", the names and sizes $FAKE_BIOS's), that far apart, and
#                            removes it before it answers
#   kiwami installs on|off   prints $FAKE_INSTALLS, else "ok"
upstream=$(dirname "$0")/../../ui/tests/fake-sync.sh
# the verb and what follows it: the options before it skipped, with their values
skip= verb= rest= run=
for a; do
    if [ -n "$skip" ]; then [ "$skip" = --run ] && run=$a; skip=; continue; fi
    if [ -z "$verb" ]; then
        case $a in --conf|--run|--auto|--status|--report|--root|--settings|--rom) skip=$a; continue ;; esac
        verb=$a; continue
    fi
    rest="$rest $a"
done
[ "$verb" = kiwami ] || exec "$upstream" "$@"
echo "$*" >> "$FAKE_SYNC_LOG"
set -- $rest
case "$1 $2" in
"bios list")
    printf "${FAKE_BIOS:-ok 0}\n"
    case ${FAKE_BIOS:-ok 0} in *error*) exit 1 ;; esac ;;
"bios get")
    shift 2
    if [ -n "${FAKE_BIOS_PACE:-}" ]; then
        # (each id's name and size from the list; the bytes before it of the ids asked for)
        field() { printf "${FAKE_BIOS:-}\n" | awk -F'\t' -v id="$1" -v f="$2" '$1 == id { print $f }'; }
        all=0; for id; do all=$((all + $(field "$id" 4))); done
        i=0 done=0
        for id; do
            i=$((i + 1))
            echo "getting $i $# $done $all $(field "$id" 3)" > "$run/kiwami-bios.status"
            done=$((done + $(field "$id" 4)))
            sleep "$FAKE_BIOS_PACE"
        done
        rm -f "$run/kiwami-bios.status"
    fi
    if [ -n "${FAKE_BIOS_GET:-}" ]; then printf "$FAKE_BIOS_GET\n"; else echo "ok $# 0 0"; fi ;;
"installs on"|"installs off")
    printf "${FAKE_INSTALLS:-ok}\n"
    case ${FAKE_INSTALLS:-ok} in *error*) exit 1 ;; esac ;;
*) echo "usage: banana-sync kiwami bios list | bios get <id>... | installs on|off" >&2; exit 2 ;;
esac
