#!/bin/sh
# A stand-in for banana-sync in the fork's headless tests of its RomM pages (BANANA_SYNC names it): the kiwami verbs
# answer here, everything else goes to upstream's ui/tests/fake-sync.sh. Each kiwami call's arguments go to
# $FAKE_SYNC_LOG, as upstream's does with its own.
#   kiwami bios list         prints $FAKE_BIOS (printf's escapes: \t, \n), its last line the result ("ok <n>" or "error <why>")
#   kiwami bios get <id>...  prints $FAKE_BIOS_GET, else "ok <the ids' count> 0 0"
upstream=$(dirname "$0")/../../ui/tests/fake-sync.sh
# the verb and what follows it: the options before it skipped, with their values
skip= verb= rest=
for a; do
    if [ -n "$skip" ]; then skip=; continue; fi
    if [ -z "$verb" ]; then
        case $a in --conf|--run|--auto|--status|--report|--root|--settings|--rom) skip=1; continue ;; esac
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
    if [ -n "${FAKE_BIOS_GET:-}" ]; then printf "$FAKE_BIOS_GET\n"; else echo "ok $# 0 0"; fi ;;
*) echo "usage: banana-sync kiwami bios list | bios get <id>..." >&2; exit 2 ;;
esac
