#!/bin/sh
# RetroArch master's device measurements (plan.md, Manual tests), the same on the v1.22.2 image and master's. Not part
# of the image: copied to the card as /data/banana/measure.sh and run over SSH (exFAT has no exec bit):
#   sh /data/banana/measure.sh run <name> [seconds]   during a game: clocks and RetroArch's CPU share each second, then
#                                                     its average share, VmHWM and VmRSS (default 30 s)
#   sh /data/banana/measure.sh powersave              during a game: the powersave clocks (the next game gets its own)
#   sh /data/banana/measure.sh reinit                 during a game: AUDIO_REINIT on RetroArch's stdin, 3 times, 5 s apart
#   sh /data/banana/measure.sh hotkeys <name>         during a game: what RetroArch hotkeys Off gave RetroArch
#   sh /data/banana/measure.sh log <name>             after a game: the game panel's retroarch.log, secrets masked
#   sh /data/banana/measure.sh card <name>            after a game: what the card holds, secrets as <set> or <empty>
# Everything goes to /data/banana/logs/ra-<name>.* (and the screen). No secret's value is written: RetroAchievements'
# password, token and username show as <set> or <empty>, and retroarch.log's are masked.
# RA_PID (a process to measure instead of retroarch), RA_DATA (/data), RA_RUN (/run/banana) and RA_LAUNCHER
# (run-retroarch, for its hotkey list) are for trying it on a computer.
D=${RA_DATA:-/data}
RUN=${RA_RUN:-/run/banana}
LAUNCHER=${RA_LAUNCHER:-/usr/libexec/banana/run-retroarch}
R=$D/retroarch
CFG=$R/retroarch.cfg
LOGS=$D/banana/logs
CPU=/sys/devices/system/cpu/cpufreq/policy0
# master's hotkeys that v1.22.2 doesn't have, and run-retroarch's list (the three Off keeps aren't in it)
FOUR="video_filter_toggle headset_recenter laser_pointer_toggle headset_view_toggle"
HOTKEYS="$(sed -n '/^HOTKEYS="/,/"$/p' "$LAUNCHER" 2>/dev/null | sed 's/^HOTKEYS=//' | tr -d '"') $FOUR"
alt() { echo $* | tr ' ' '|'; }

usage() { sed -n 's/^#   sh /  sh /p' "$0" >&2; exit 1; }
need_name() { [ -n "$1" ] || usage; }
mkdir -p "$LOGS"

# P: the running RetroArch's pid, or the script stops
game() {
    P=${RA_PID:-$(pidof retroarch 2>/dev/null | cut -d ' ' -f 1)}
    [ -n "$P" ] && [ -d /proc/$P ] || { echo "measure: no retroarch running: start the game first" >&2; exit 1; }
}
val() { cat "$1" 2>/dev/null || echo -; }
gpu() { for g in /sys/class/devfreq/*.gpu; do [ -e "$g/cur_freq" ] && { cat "$g/cur_freq"; return; }; done; echo -; }
# ticks: RetroArch's utime + stime, all its threads (fields 14 and 15 of /proc/<pid>/stat, counted after the name)
ticks() {
    s=$(cat /proc/$P/stat 2>/dev/null) || return 1
    s=${s##*) }
    set -- $s
    echo $((${12} + ${13}))
}
# cs: the time since boot in hundredths of a second
cs() { read -r u _ < /proc/uptime; echo $((${u%.*} * 100 + 1${u#*.} - 100)); }
# share <ticks> <hundredths>: the CPU share in percent, one decimal (over 100 when it uses more than one core)
share() { t=$(($1 * 100000 / (HZ * $2))); echo $((t / 10)).$((t % 10)); }
# masked <file> <key prefix>: each key starting with the prefix, its lines' values as <set> or <empty>, never the value
# (key = "value" in retroarch.cfg, key=value in accounts.conf)
masked() {
    [ -f "$1" ] || { echo "  no ${1#$D/}"; return; }
    awk -v p="$2" 'index($0, p) == 1 && match($0, /^[A-Za-z0-9_]+ *=/) {
            k = substr($0, 1, RLENGTH); sub(/ *=$/, "", k)
            v = substr($0, RLENGTH + 1); sub(/^ */, "", v)
            if (v ~ /^".*"$/) v = substr(v, 2, length(v) - 2)
            if (!(k in n)) o[++c] = k
            n[k]++; s[k] = s[k] (v == "" ? " <empty>" : " <set>")
        }
        END { if (!c) print "  none"; for (i = 1; i <= c; i++) printf "  %s:%s (%d line%s)\n", o[i], s[o[i]], n[o[i]], (n[o[i]] > 1 ? "s" : "") }' "$1"
}

case $1 in
run)
    need_name "$2"; game
    N=${3:-30}
    HZ=$(getconf CLK_TCK 2>/dev/null); [ -n "$HZ" ] || HZ=100
    {
        echo "== $2, $(date '+%Y-%m-%d %H:%M:%S')"
        echo "version: $(val /etc/banana/version)"
        echo "retroarch --version:"
        if command -v retroarch > /dev/null; then retroarch --version 2>&1 | head -n 5 | sed 's/^/  /'; else echo "  no retroarch"; fi
        echo "governor: $(val $CPU/scaling_governor), scaling_max_freq: $(val $CPU/scaling_max_freq)"
        for f in $CFG $R/config/Mesen/Mesen.cfg $R/config/SameBoy/SameBoy.cfg; do
            v=$(sed -n 's/^video_threaded = "\(.*\)"$/\1/p' "$f" 2>/dev/null | tail -n 1)
            echo "video_threaded in ${f#$R/}: ${v:-none}"
        done
        echo "pid $P, $N s, CLK_TCK $HZ"
        echo "  s    cpu_khz     gpu_hz  share%"
        t0=$(ticks); c0=$(cs); t=$t0; c=$c0; i=0
        while [ $i -lt $N ]; do
            sleep 1
            t1=$(ticks) || { echo "retroarch ended after $i s"; break; }
            c1=$(cs); i=$((i + 1))
            printf '%3d %10s %10s %7s\n' $i "$(val $CPU/scaling_cur_freq)" "$(gpu)" "$(share $((t1 - t)) $((c1 - c)))"
            t=$t1; c=$c1
        done
        [ $c -gt $c0 ] && echo "average share%: $(share $((t - t0)) $((c - c0)))"
        grep -E '^(VmHWM|VmRSS):' /proc/$P/status 2>/dev/null | tr -s ' \t' ' '
    } 2>&1 | tee "$LOGS/ra-$2.txt"
    ;;
powersave)
    game
    banana-perf powersave && echo "powersave: governor $(val $CPU/scaling_governor), scaling_max_freq $(val $CPU/scaling_max_freq)"
    ;;
reinit)
    # the FIFO run-retroarch (Second screen off) and banana-ui (the game panel) give RetroArch as its stdin
    F=$RUN/retroarch.cmd
    [ -p $F ] || { echo "measure: no $F: start the game first" >&2; exit 1; }
    {
        echo "== $(date '+%Y-%m-%d %H:%M:%S'), $(val /etc/banana/version)"
        for i in 1 2 3; do
            [ $i = 1 ] || sleep 5
            echo AUDIO_REINIT > $F && echo "$(date +%T) AUDIO_REINIT $i"
        done
    } 2>&1 | tee -a "$LOGS/ra-reinit.txt"
    ;;
hotkeys)
    need_name "$2"
    HK=$RUN/retroarch-hotkeys.cfg
    AC=$RUN/ra-autoconfig/udev
    {
        echo "== $2, $(date '+%Y-%m-%d %H:%M:%S'), $(val /etc/banana/version)"
        echo "-- the Off config, master's four and save_state ($HK)"
        if [ -f $HK ]; then grep -E "^input_($(alt $FOUR save_state))_(btn|axis) = " $HK; else echo "no Off config"; fi
        echo "-- the profiles' copy, hotkey lines left in it ($AC)"
        if [ -d $AC ]; then
            for f in $AC/*.cfg; do
                [ -f "$f" ] || continue
                k=$(grep -oE "^input_($(alt $HOTKEYS))_(btn|axis)" "$f" | sed 's/^input_//' | tr '\n' ' ')
                echo "${f##*/}: ${k:-none}"
            done
        else
            echo "no profiles' copy"
        fi
        echo "-- the lines kept for after the game ($R/.banana-hotkeys.keep)"
        cat $R/.banana-hotkeys.keep 2>/dev/null || echo "none"
    } 2>&1 | tee "$LOGS/ra-$2-hotkeys.txt"
    ;;
log)
    need_name "$2"
    L=$RUN/retroarch.log
    [ -f $L ] || { echo "measure: no $L: it's the game panel's (Second screen: Game panel), after a game" >&2; exit 1; }
    sed -E 's/((password|token)"?[ ]*[=:][ ]*"?)[^"& ]*/\1<masked>/g; s/(^|[?& ])((t|p)=)[^& "]*/\1\2<masked>/g' $L > "$LOGS/ra-$2.log" &&
        echo "$LOGS/ra-$2.log: $(wc -l < "$LOGS/ra-$2.log") lines"
    ;;
card)
    need_name "$2"
    {
        echo "== $2, $(date '+%Y-%m-%d %H:%M:%S'), $(val /etc/banana/version)"
        echo "-- ls -la $R"; ls -la $R
        echo "-- retroarch.cfg's cheevos_*"; masked $CFG cheevos_
        echo "-- banana/accounts.conf's ra_*"; masked $D/banana/accounts.conf ra_
        echo "-- files"
        for f in retroarch-keychain.cfg retroarch-keychain.key .banana-hotkeys.keep; do
            [ -e $R/$f ] && echo "  $f: there" || echo "  $f: none"
        done
        echo "-- retroarch.cfg's log_to_file, joypad_autoconfig_dir and hotkey lines"
        grep -E "^(log_to_file|joypad_autoconfig_dir|input_($(alt $HOTKEYS enable_hotkey menu_toggle exit_emulator))_(btn|axis)) = " $CFG 2>/dev/null
        for b in $R/backup-*; do
            [ -d "$b" ] || continue
            echo "-- ls -laR ${b#$D/}"; ls -laR "$b"
            echo "-- ${b#$D/}/retroarch.cfg's cheevos_*"; masked "$b/retroarch.cfg" cheevos_
        done
        echo "-- file sharing's veto line ($RUN/ksmbd.conf)"
        grep 'veto files' $RUN/ksmbd.conf 2>/dev/null || echo "file sharing off"
    } 2>&1 | tee "$LOGS/ra-$2-card.txt"
    ;;
*) usage ;;
esac
