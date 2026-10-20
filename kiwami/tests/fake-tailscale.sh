#!/bin/sh
# A stand-in for the Tailscale helper (/usr/libexec/kiwami/tailscale) in banana-ui's headless test
# (kiwami/tests/tailscale-ui.sh, as KIWAMI_TAILSCALE): records each call in $KT_REC, and `status` answers as the helper
# would in the state $KT_STATE: off, error (tun missing), starting, refused (an auth key refused, then a login by QR),
# or a capture in kiwami/tests/tailscale/ by its name (needs-login, needs-approval, running, expired, clock, ...).
C=$(cd "$(dirname "$0")" && pwd)/tailscale
[ -n "$KT_REC" ] && echo "$*" >> "$KT_REC"
case $1 in
status)
    case $KT_STATE in
    off) echo "state off" ;;
    error) echo "state off"; echo "error the tun module could not be loaded" ;;
    starting) echo "state starting" ;;
    refused) echo "error auth key refused: backend error: invalid key: API key does not exist"; cat "$C/needs-login.json" ;;
    *) cat "$C/$KT_STATE.json" ;;
    esac
    ;;
esac
exit 0
