#!/bin/sh
# RetroArch master's credentials file folded back into retroarch.cfg (kiwami/rootfs/usr/libexec/kiwami/ra-keychain)
# on a made-up card, alone and through upstream's ra-secrets scrub with the fork's hook: the keychain's lines once
# each, its value winning, the files gone, the password and token in accounts.conf in the end; with no keychain, what
# upstream's scrub leaves, byte for byte. Run by kiwami/tests/ci.sh:
#   kiwami/tests/ra-keychain.sh
set -e
cd "$(dirname "$0")/../.."
SEC=system/rootfs/usr/libexec/banana/ra-secrets
HOOK='    [ -x /usr/libexec/kiwami/ra-keychain ] && /usr/libexec/kiwami/ra-keychain $CFG	# kiwami: RetroArch master'"'"'s credentials file back into retroarch.cfg'
X=$(mktemp -d "${TMPDIR:-/tmp}/kiwami-kc.XXXXXX")
R=$X/data/retroarch
CFG=$R/retroarch.cfg
K=$R/retroarch-keychain.cfg
A=$X/data/banana/accounts.conf
n=0
fail() { echo "ra-keychain: FAIL: $*" >&2; [ -d "$X" ] && find "$X/data" -type f | sed 's/^/  file: /' >&2; exit 1; }
ok() { n=$((n + 1)); }
# the scripts with /data and /usr under $X, in one pass (a TMPDIR under /usr isn't rewritten again): ra-secrets with
# the hook, which reaches the copy of the fork's script, and upstream's, the hook taken out
rw() { sed "s#\(^\|[^A-Za-z0-9_.]\)/\(data\|usr\)/#\1$X/\2/#g"; }
mkdir -p "$X/usr/libexec/kiwami"
cp kiwami/rootfs/usr/libexec/kiwami/ra-keychain "$X/usr/libexec/kiwami/"
rw < $SEC > "$X/hooked"
grep -vxF "$HOOK" $SEC | rw > "$X/upstream"
! grep -q ra-keychain "$X/upstream" || fail "upstream's copy of ra-secrets still has the hook"
fresh() { rm -rf "$X/data"; mkdir -p "$R" "${A%/*}"; }
# fold / scrub / upstream: the fork's script alone, the hooked scrub, upstream's scrub; stderr in $X/err
fold() { "$X/usr/libexec/kiwami/ra-keychain" "$CFG" 2> "$X/err" || fail "ra-keychain exited $?"; }
scrub() { sh "$X/hooked" scrub 2> "$X/err" || fail "the hooked scrub exited $?"; }
upstream() { sh "$X/upstream" scrub 2> "$X/err" || fail "upstream's scrub exited $?"; }
LOG='kiwami: ra-keychain: retroarch-keychain.cfg folded into retroarch.cfg'
# retroarch.cfg as master leaves it (its sensitive keys taken out), and the keychain file it writes beside it
MASTER_CFG='video_driver = "gl"
audio_latency = "64"
cheevos_enable = "true"'
KC='cheevos_username = "alice"
cheevos_password = "pw1"
cheevos_token = "tok1"
netplay_password = ""
kiosk_mode_password = ""'
master() { printf '%s\n' "$MASTER_CFG" > "$CFG"; printf '%s\n' "$1" > "$K"; : > "$R/retroarch-keychain.key"; }
has() { grep -qxF "$2" "$1" || fail "no '$2' in ${1#$X/}: $(cat "$1" 2>/dev/null)"; }
count() { [ "$(grep -c "$2" "$1")" = "$3" ] || fail "'$2' is $(grep -c "$2" "$1") times in ${1#$X/}, not $3"; }
gone() { for f in "$R"/retroarch-keychain.*; do [ ! -e "$f" ] || fail "${f#$X/} left"; done; }

# no keychain: the hooked scrub leaves what upstream's does, and the fold logs nothing (A7)
for pass in hooked upstream; do
    fresh
    printf 'video_driver = "gl"\ncheevos_username = "alice"\ncheevos_password = "pw1"\n' > "$CFG"
    printf 'ra_user=alice\nra_password=pw0\n' > "$A"
    if [ $pass = hooked ]; then scrub; else upstream; fi
    cp "$CFG" "$X/cfg.$pass"; cp "$A" "$X/acct.$pass"; cp "$X/err" "$X/err.$pass"
done
cmp -s "$X/cfg.hooked" "$X/cfg.upstream" || fail "no keychain: retroarch.cfg differs from upstream's scrub"; ok
cmp -s "$X/acct.hooked" "$X/acct.upstream" || fail "no keychain: accounts.conf differs from upstream's scrub"; ok
[ ! -s "$X/err.hooked" ] || fail "no keychain: logged '$(cat "$X/err.hooked")'"; ok
# ...and the fold alone on a retroarch.cfg with no secrets: byte-identical
fresh; printf '%s\n' "$MASTER_CFG" > "$CFG"; cp "$CFG" "$X/before"; fold
cmp -s "$CFG" "$X/before" || fail "no keychain: the fold changed retroarch.cfg"; ok
[ ! -s "$X/err" ] || fail "no keychain: the fold logged '$(cat "$X/err")'"; ok

# folded: the keychain's five lines once each, in its order, after retroarch.cfg's own; both files gone; one log line
# with no value (A5)
fresh; master "$KC"; fold
[ "$(cat "$CFG")" = "$(printf '%s\n%s' "$MASTER_CFG" "$KC")" ] || fail "folded: retroarch.cfg is $(cat "$CFG")"; ok
gone; ok
[ "$(cat "$X/err")" = "$LOG" ] || fail "folded: logged '$(cat "$X/err")'"; ok
! grep -q 'pw1\|tok1' "$X/err" || fail "folded: a value in the log"; ok

# through the scrub: the password and token to accounts.conf, the username stays in retroarch.cfg, nothing left (A6)
fresh; master "$KC"; scrub
for l in ra_user=alice ra_password=pw1 ra_retroarch_token=tok1; do has "$A" $l; done; ok
has "$CFG" 'cheevos_username = "alice"'; ok
! grep -q '^cheevos_\(password\|token\) = ' "$CFG" || fail "scrub: a password or token in retroarch.cfg: $(cat "$CFG")"; ok
gone; ok
# ...again: nothing changes, nothing logged
cp "$CFG" "$X/cfg.once"; cp "$A" "$X/acct.once"; scrub
cmp -s "$CFG" "$X/cfg.once" && cmp -s "$A" "$X/acct.once" || fail "a second scrub changed the card"; ok
[ ! -s "$X/err" ] || fail "a second scrub logged '$(cat "$X/err")'"; ok

# another account in the keychain than accounts.conf's: the last login wins (A6)
fresh; master "$(printf 'cheevos_username = "bob"\ncheevos_password = "pw2"\ncheevos_token = "tok2"')"
printf 'ra_user=alice\nra_password=pw0\nra_retroarch_token=tok0\n' > "$A"; scrub
[ "$(sort "$A")" = "$(printf 'ra_password=pw2\nra_retroarch_token=tok2\nra_user=bob')" ] || fail "another account: accounts.conf is $(cat "$A")"; ok
has "$CFG" 'cheevos_username = "bob"'; ok

# the keychain wins: a key in both files ends up with the keychain's line only (A5)
fresh; printf 'cheevos_username = "alice"\nvideo_driver = "gl"\ncheevos_username = "alice"\n' > "$CFG"
printf 'cheevos_username = "bob"\n' > "$K"; fold
count "$CFG" '^cheevos_username = ' 1; ok
has "$CFG" 'cheevos_username = "bob"'; ok
has "$CFG" 'video_driver = "gl"'; ok

# special characters: & | \ carried byte for byte, through to accounts.conf (A5)
fresh; master "$(printf 'cheevos_username = "alice"\ncheevos_password = "%s"' 'p&w|d\x')"; scrub
has "$A" 'ra_password=p&w|d\x'; ok

# no final newline on retroarch.cfg: its last line kept whole, the keychain's after it (A5)
fresh; printf 'video_driver = "gl"\naudio_latency = "64"' > "$CFG"; printf 'cheevos_username = "alice"' > "$K"; fold
[ "$(cat "$CFG")" = "$(printf 'video_driver = "gl"\naudio_latency = "64"\ncheevos_username = "alice"')" ] || fail "no final newline: $(cat "$CFG")"; ok

# an empty keychain file: retroarch.cfg kept as it is, a key twice and a line of another shape too (not read as the
# keychain), the file gone
fresh; printf '%s\n# a comment\ncheevos_enable = "false"\n' "$MASTER_CFG" > "$CFG"; cp "$CFG" "$X/before"; : > "$K"; fold
cmp -s "$CFG" "$X/before" || fail "an empty keychain changed retroarch.cfg: $(cat "$CFG")"; ok
gone; ok

# no retroarch.cfg: the keychain stays as it is (file sharing vetoes it), nothing made, exit 0, through the scrub too (A5)
fresh; printf '%s\n' "$KC" > "$K"; cp "$K" "$X/kc"; fold; scrub
cmp -s "$K" "$X/kc" || fail "no retroarch.cfg: the keychain changed"; ok
[ ! -e "$CFG" ] && [ ! -e "$CFG.kiwami" ] || fail "no retroarch.cfg: made $(ls "$R")"; ok

# a failure (the folder not writable): both files as they were, no temporary, the scrub carries on (not as root,
# which writes anyway)
if [ "$(id -u)" != 0 ]; then
    fresh; master "$KC"; rm -f "$R/retroarch-keychain.key"; cp "$CFG" "$X/before"; cp "$K" "$X/kc"
    chmod a-w "$R"; fold; chmod u+w "$R"
    cmp -s "$CFG" "$X/before" && cmp -s "$K" "$X/kc" || fail "a failed fold changed the files"; ok
    [ ! -e "$CFG.kiwami" ] || fail "a failed fold left its temporary"; ok
    [ ! -s "$X/err" ] || ! grep -qF "$LOG" "$X/err" || fail "a failed fold logged it folded"; ok
fi

# the hook: the first line of upstream's scrub)
[ "$(grep -A1 '^scrub)$' $SEC | tail -n 1)" = "$HOOK" ] || fail "ra-secrets has no ra-keychain hook as the first line of scrub)"; ok

rm -rf "$X"
echo "ra-keychain: all $n checks passed"
