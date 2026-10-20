# 0001 Tailscale: investigation

Issue: #1 · Branch: `k/0001-tailscale` · Status: refined

## The request

Join the handheld to a Tailscale network (a tailnet) from Settings > Network, so it can reach services on the tailnet
(planned uses: netplay, ssh out, services at home) and be reached from anywhere (SSH, File sharing).

- Logging in shows the login link and a QR code of it, so the device is approved from a phone instead of typing
  the link into a browser.
- One tailnet at a time.
- Once connected, the menu lists the tailnet's devices: name and address.
- When Tailscale is on, it starts again by itself after a power-on or a restart.

Decided while discussing the request:

- Kernel mode (a `tailscale0` interface), so programs on the handheld reach the tailnet, not only the other way.
- Tailscale built from source with Buildroot's Go: 1.98.8, the newest that builds with Go 1.26.3 (see *Options*).
- tailscaled running all the time while Tailscale is on, games included, is acceptable.
- Off by default (upstream's behaviour).
- Tailscale's log upload off (`--no-logs-no-support`).
- Besides the link and the QR, an auth key written to the card on a computer logs in without them.
- The device's name on the tailnet is a setting: `banana-rgds` on the RG DS and `banana-rgds-plus` on the RG DS Plus
  by default.
- Settings > Network gets one `Tailscale` row showing the state; A on it opens Tailscale's own submenu (switch it on
  or off, the devices, the name, log out, and later options). Information to read sits on the top panel; what can be
  moved through or changed sits on the bottom panel.

Out of scope: MagicDNS on the handheld itself (the handheld resolving tailnet names: `/etc/resolv.conf` is on the
read-only root and udhcpc rewrites it), its own issue. Other devices reaching the handheld by its MagicDNS name works
without it (a tailnet-wide setting in the admin console). Also out: exit nodes, subnet routes, Tailscale SSH (dropbear
is there), Taildrop, Funnel/Serve, and Headscale (a self-hosted control server, `--login-server`: postponed).

## Oversights in the requirements

1. **The kernel has no TUN.** Upstream turns off `CONFIG_TUN`, `CONFIG_NETFILTER` and `CONFIG_WIREGUARD` on purpose
   (`buildroot/board/rgds/linux-banana.fragment:47-57`: "nothing on the network path"). Kernel mode needs TUN.
   *Proposed*: `CONFIG_TUN=m` in `kiwami/linux.fragment`, a module loaded only when Tailscale starts (as upstream
   loads the SMB server). Netfilter stays off and tailscaled runs with `--netfilter-mode=off`; what that gives up is
   Tailscale's own firewall rules, which matter for subnet routers and exit nodes, and drop packets spoofing tailnet
   addresses from other interfaces. Banana has no firewall at all today. WireGuard's kernel module isn't needed:
   Tailscale does WireGuard in its own process on Linux. Policy routing, which tailscaled uses for its routes, is
   already in the kernel (`CONFIG_IP_MULTIPLE_TABLES=y`, `CONFIG_IPV6_MULTIPLE_TABLES=y` from ROCKNIX's config).

2. **Buildroot's own `tailscale` package can't be used, nor its name.** It is 1.78.1 (Dec 2024), and its
   `LINUX_CONFIG_FIXUPS` switch on netfilter and nftables in the kernel. A package of the fork's own can't be called
   `tailscale` either: `BR2_PACKAGE_TAILSCALE` is Buildroot's. *Proposed*: `kiwami/package/kiwami-tailscale/`.

3. **DNS has to be kept away from `/etc/resolv.conf`.** tailscaled's default (`--accept-dns=true`) rewrites
   `/etc/resolv.conf` and writes a backup beside it, on the read-only root: errors in its log, and on-device DNS is
   out of scope anyway. *Proposed*: `--accept-dns=false`, always.

4. **Device approval.** A tailnet can require an admin to approve each new device after the login
   (tailscaled's state `NeedsMachineAuth`). The page then shows that it is waiting for the admin, not the QR.

5. **Logins expire.** Node keys expire after 180 days by default; tailscaled goes back to `NeedsLogin` and the device
   is off the tailnet until it logs in again. *Proposed*: the Tailscale row and page say "Log in again" and show a new
   QR; the page's help suggests disabling key expiry for the device in the admin console. A login link left unused
   also expires: the page asks tailscaled for a new one.

6. **The clock.** The login and the control connection use TLS and need the right time. The clock comes from the RTC
   (`hwclock -s` in `rcS`) and from NTP once Wi-Fi connects (`wifi`, on `CONNECTED`). A handheld whose RTC lost the
   time can't log in until NTP ran. *Proposed*: tailscaled retries by itself; if it reports a certificate/time error,
   the page says to connect Wi-Fi so the clock is set.

7. **Wi-Fi off, and sleep.** On the RG DS the Wi-Fi module is unloaded before every suspend and loaded after
   (`system/rootfs/etc/banana/sleep.d/10-wifi`), so `wlan0` disappears and comes back. tailscaled should notice the
   link change and reconnect in a few seconds; only the device can show it (manual test). With Wi-Fi off and
   Tailscale on, tailscaled stays running and idle; the row says "Waiting for Wi-Fi". tailscaled itself is never
   stopped around a sleep (it is frozen with everything else during the suspend).

8. **Shutdown.** `rcK` stops sharing, SSH and Wi-Fi, then unmounts `/data`. tailscaled keeps files on the card
   (its state) and must be stopped before the unmount, or `/data` is only remounted read-only.

9. **The login state is on the card, and the card is shared.** The only writable persistent storage is the exFAT
   `/data`, so the state goes to `/data/banana/tailscale/` (it survives A/B updates, which is good). But:
   - anyone holding the card can copy the device's identity (the same as `wifi.conf`'s passwords today);
   - **File sharing shares all of `/data`**: the state file would be readable over SMB by anyone on the network. The
     share script keeps secrets out with `veto files` (`system/rootfs/usr/libexec/banana/share`), which doesn't know
     Tailscale's. *Proposed*: `kiwami/post-build.sh` adds the state file and the auth key (oversight 13) to that list
     in the image's copy (no change to upstream's script in the repo);
   - exFAT has no permissions: tailscaled writing and renaming its state there must be checked on the device (dropbear
     needed a copy in `/run` for its keys for the same reason, `ssh` script).

10. **File sharing and SSH over the tailnet.** With Tailscale on, the guest-access SMB share and dropbear are
    reachable from the whole tailnet, including people a device was shared with. Mostly the point, but the Tailscale
    page's help says so, as File sharing's does for the local network.

11. **The device's name.** The system's hostname is `banana` on every card (`rcS:25`); left as it is (SMB's
    `BANANA` and syslog use it). The tailnet name is Tailscale's own (`tailscale up --hostname`, changed later with
    `tailscale set --hostname`), a setting of the fork's:
    - The default comes from the handheld that is running, not the image: `/proc/device-tree/model` is
      `Anbernic RG DS` or `Anbernic RG DS Plus`, so a `universal` card gets the right one on each.
    - A name is a DNS label: lowercase letters, digits and hyphens, up to 63, not starting or ending with a hyphen.
      The keyboard turns spaces into hyphens and capitals into lowercase, and refuses the rest.
    - Two handhelds with the same name: the second gets `-1` appended by Tailscale.
    - A device renamed in the admin console stops following its own name there (the console's "Auto-generate from
      OS hostname" turns off), so a later change on the handheld doesn't show. The name row's help says so.
    - The keyboard is upstream's (`startEntry`); a new purpose for it is a value in upstream's `Entry` enum and a
      branch in `finishEntry()`: two more one-line hooks.

12. **Tailscale's log upload.** By default tailscaled sends its logs to Tailscale's log server (for their support)
    and buffers them on disk, here on the card. Decided: `--no-logs-no-support`, no upload and no buffer files.
    tailscaled's own output goes to syslog (`/run/banana/messages`, in RAM), and the fork's helper writes its few
    lines (started, stopped, state changes, logins) to `/data/banana/logs/tailscale.log`.

13. **The auth key.** A key from the admin console, in `banana/tailscale/authkey` on the card (written on a
    computer, or over File sharing). Typing one on the handheld isn't offered: it is about 60 random characters.
    - It is used when Tailscale needs a login and the file is there: on switching it on, at boot, after a logout or an
      expiry. The QR is shown only when there is no key.
    - Once the login succeeds, the file is deleted (a reusable key left on the card would let anyone holding it add
      devices). A key that is refused is renamed to `authkey.refused`, the reason goes to the log, and the page shows
      it and falls back to the QR.
    - Like the state file, it is kept out of File sharing (`veto files`).
    - A key tagged in the admin console makes a device whose key doesn't expire; the help mentions it.

14. **First boot and no card.** Nothing special: with no `tailscale = on` in `settings.conf` nothing starts, and
    `/data/banana/tailscale/` is made when Tailscale is first switched on.

15. **Both boards and both panel sizes.** Nothing board-specific (same kernel and Wi-Fi). The QR has to read well at
    640x480 and 1024x768: a login URL (~45 characters) is a version 4 QR, 33 modules plus a 4-module quiet zone each
    side, so 41 modules: 8 px each on a 640x480 panel (328 px), 12 or more at 1024x768.

## Suggestions

Decided: the auth key on the card (oversight 13), the name setting (oversight 11), Headscale postponed.

- **The device list shows a little more**: online or offline, the OS, and the selected device's MagicDNS name and
  addresses on the top panel. This device's own top panel shows the `ssh root@<address>` and `\\<address>\banana`
  lines, so SSH and File sharing over the tailnet are one look away.
- **A generic page hook offered upstream**: the fork's page needs about a dozen one-line hooks in `settings.cpp` (see
  *What has to change*). A small interface for extra settings pages would be worth offering upstream later
  (`docs/FORK.md`, *Upstreaming*); not part of this feature.
- **Leave out**: on-device MagicDNS (own issue), exit nodes and subnet routes (they need netfilter), Taildrop (a good
  later feature: ROMs sent to the handheld from a phone), Tailscale SSH.

## Current state

**Settings screen** (`ui/src/settings.cpp`, `settings.h`):

- The Network group is built in `Settings::build()` (`settings.cpp:392-395`): `Wi-Fi` (On/Off, `settings.conf`
  key `wifi`), `Network` (a `PAGE`: the networks around), `File sharing` (On/Off, key `share`).
- A choice is written by `save()` (`settings.cpp:~630-656`), which then starts the matching helper in the background:
  `spawn("/usr/libexec/banana/wifi" | ".../ssh" | ".../share", "up" | "down")`.
- A `PAGE` row opens its page in `change()` (`settings.cpp:665`). Pages are an `enum Page { LIST, NETWORKS, KEYBOARD,
  UPDATES, SETUP, GAME }` (`settings.h`), dispatched in `press()` (`:711`), `touch()` (`:773`), `render()` (`:1177`),
  `update()` (`:1319`) and `pageWake()` (`:1392`). A page draws its top panel (title, help) and bottom panel (list) in
  `render*()` with the screen's private helpers (`box`, `text`, `wrapped`, `icon`, `face`...).
- Background commands: `wifi::Job` (`ui/src/wifi.h`) runs a command on a thread and hands back its output; `sync`
  runs it at once in the headless test. The Network page polls `wifi status` only while it is open (`update()`,
  `pageWake()`), and `open()` asks once for the row's value (`settings.cpp:595`).
- A row's value text (`settings.cpp:~1247`) and help (`:~1200`) are computed per key.
- `Settings` has no extension point: every row, page and dispatch is in upstream's code.

**Helpers and boot** (`system/rootfs/`):

- `usr/libexec/banana/{wifi,ssh,share}`: `up` / `down` scripts, pid files in `/run/banana/`.
- `etc/init.d/rcS:64-86`: a background subshell after the menu starts; Wi-Fi up if configured, then
  `share up` and `ssh up` if their setting is `on` (`grep -q '^ *ssh *= *on' settings.conf`).
- `etc/init.d/rcK`: stops share, SSH, Wi-Fi, writes the boot log, unmounts `/data`.
- `etc/banana/sleep.d/10-wifi`: unloads and reloads the RG DS's Wi-Fi around a suspend.
- `syslogd -O /run/banana/messages -s 256 -b 1` (`rcS:26`): syslog in RAM, 256 KB, one rotation.

**Tests**: `ui/tests/run.sh` renders headless at 640x480 and 1024x768 with a stand-in Wi-Fi script
(`BANANA_WIFI=ui/tests/fake-wifi.sh`) and checks banana-ui's log lines.

**The fork**: the build tree is in place (`kiwami/`: external tree, `configure.sh`, `linux.fragment`, rootfs overlay,
`post-build.sh`, `tests/ci.sh`; `docs/FORK.md` *How the build takes in the fork*). `ui/src/kiwami/` doesn't exist
yet, nor a hook building it.

**Upstream (`main`)** has nothing for Tailscale or VPNs.

**ROCKNIX** (for comparison, `ROCKNIX/distribution` `projects/ROCKNIX/packages/network/tailscale/` and
`ROCKNIX/emulationstation-next` `GuiMenu.cpp:~5915`): Tailscale's prebuilt 1.98.8 binaries, kernel mode with
`TUN=m`, `WIREGUARD=m` and netfilter on; tailscaled started at boot by systemd when `tailscale.up=1`, state in
`/storage/.cache/tailscale`. The menu's switch runs `tailscale up --timeout=7s`, reads `tailscale status`, and shows
the `Log in at:` URL in a message box when it says `Logged out.` No QR, no device list, no waiting for the approval
(the switch is saved as off while the login is pending).

## What has to change

New files of the fork's:

- `kiwami/package/kiwami-tailscale/` (`Config.in`, `kiwami-tailscale.mk`, `.hash`): Tailscale 1.98.8 from source
  (`golang-package`), one `tailscaled` binary with the CLI built in (`ts_include_cli`) and `tailscale` a symlink to
  it, unused features left out with `ts_omit_*` build tags, stripped. Sourced from `kiwami/Config.in`.
- `kiwami/configs/kiwami.fragment`: `BR2_PACKAGE_KIWAMI_TAILSCALE=y`, and `BR2_PACKAGE_HOST_GO_BIN=y` (Go's prebuilt
  compiler, same 1.26.3, instead of building Go from source through five bootstrap stages).
- `kiwami/linux.fragment`: `CONFIG_TUN=m`.
- `kiwami/rootfs/usr/libexec/kiwami/tailscale`: the helper.
  - `boot`: `up` if `tailscale = on`.
  - `up`: load `tun`, start tailscaled with `--statedir=/data/banana/tailscale --no-logs-no-support`, then
    `tailscale up --accept-dns=false --netfilter-mode=off --hostname=<name>` in the background, with
    `--auth-key=file:/data/banana/tailscale/authkey` when that file is there (deleted once logged in, renamed to
    `authkey.refused` when refused).
  - `down`: stop tailscaled, unload `tun`. The login stays.
  - `logout`: `tailscale logout` (forgets the tailnet; the next `up` asks for a login).
  - `name <name>`: `tailscale set --hostname=<name>` while it runs (the next `up` uses it anyway).
  - `status`: `tailscale status --json`.
  - Its own log in `/data/banana/logs/tailscale.log`.
- `kiwami/post-build.sh`: the state file and the auth key added to the share script's `veto files`.
- `ui/src/kiwami/`: the `Tailscale` row in the Network list, its submenu and the devices page (*Options*, *The
  submenu*), the name typed on upstream's keyboard, a small JSON reader for `tailscale status --json`, and Nayuki's
  QR Code generator (MIT, one `.cpp`/`.hpp`, vendored).
- `kiwami/tests/`: a stand-in `tailscale` helper (each state: off, starting, needs login with a URL, needs approval,
  running with peers, logged out), headless renders of the page at both sizes, checks on the log lines, and the JSON
  and QR code checked on their own. Run from `kiwami/tests/ci.sh` once banana-ui is built.
- `kiwami/DEPENDENCIES.md`: Tailscale 1.98.8 and qrcodegen.

Hooks in upstream's files (each one line, marked `kiwami:`):

- `ui/Makefile`: `SRC += $(wildcard src/kiwami/*.cpp)` (the fork's banana-ui sources).
- `ui/src/settings.h`: a `friend` for the fork's class (its page draws with the screen's own private helpers, so it
  looks the same) and a pointer to it.
- `ui/src/settings.cpp`, at each place a page or row is dispatched: `build()` (the rows), `open()` (the row's state,
  once), `change()` (the submenu opened), `press()`, `touch()`, `render()`, `update()`, `pageWake()`, the row's value
  text, and `finishEntry()` (the name typed on the keyboard). About ten lines. They can't be avoided: `Settings` has
  no way to add a page from outside.
- `ui/src/settings.h`, also: a value in the `Entry` enum (the keyboard's purpose: the Tailscale name).
- `system/rootfs/etc/init.d/rcS`: `/usr/libexec/kiwami/tailscale boot` in the background subshell, after SSH.
- `system/rootfs/etc/init.d/rcK`: `/usr/libexec/kiwami/tailscale down` before the unmount.

Settings, in `/data/banana/settings.conf`:

- `tailscale = off | on`, default `off` (upstream's behaviour: nothing installed runs, the module isn't loaded, the
  menu shows only the extra row).
- `tailscale_name = <name>`, absent by default: then `banana-rgds` or `banana-rgds-plus` from the device tree's model.

## Options

**How Tailscale gets into the image**

| | Version | Cost |
|---|---|---|
| Buildroot's package | 1.78.1 | Too old, switches netfilter on. Ruled out. |
| **Source, Buildroot's Go 1.26.3** (chosen) | 1.98.8 (30 Jun 2026) | One trimmed binary, ~10-15 MB in the squashfs (to be measured). Newer versions wait for a newer Go in Buildroot (1.100 needs 1.26.4, 1.102.3 needs 1.26.6, 1.104 needs 1.27). |
| Source, the fork's own newer Go | 1.104.1 | A Go toolchain package to maintain beside Buildroot's. |
| Tailscale's prebuilt binaries | 1.104.1 and later | ~40 MB in the squashfs (72 MB raw), nothing left out; every update package 40 MB bigger. |

**Reading tailscaled's state**

- *`tailscale status --json` parsed in banana-ui* (recommended): the JSON is Tailscale's stable interface
  (`BackendState`, `AuthURL`, `Self`, `Peer` with `HostName`, `DNSName`, `TailscaleIPs`, `OS`, `Online`). Costs a
  small JSON reader in `ui/src/kiwami/` (upstream's in `theme.cpp` is private to that file and skips arrays).
- *`tailscale status` text, parsed in the helper*: no JSON code, but the text is for people and changes between
  versions, and the login URL and state need the JSON anyway.

**Where the QR code comes from**

- *Nayuki's qrcodegen vendored in `ui/src/kiwami/`* (recommended): MIT, one file, nothing added to the rootfs,
  rendered by banana-ui so the headless tests see it.
- *libqrencode* (Buildroot has it): a library in the rootfs and a dependency for banana-ui's package.
- *`tailscale up --qr`*: prints the QR as text for a terminal; parsing it back is worse than encoding it.

**The login flow**

- *Recommended*: switching Tailscale on starts tailscaled and runs `tailscale up` in the background (it returns at
  once with `--timeout`, or keeps waiting for the login, detached). While the Tailscale page is open, it asks for
  `status --json` every couple of seconds: `NeedsLogin` with an `AuthURL` shows the QR and the link; `NeedsMachineAuth`
  says it is waiting for an admin; `Running` shows the devices. Nothing is polled while the page is closed.
- *ROCKNIX's*: a blocking `tailscale up --timeout=7s` from the switch and a message box. Freezes the menu for the
  timeout and never notices the approval.

**The submenu** (decided: information to read on the top panel, what can be moved through or changed on the bottom)

Settings > Network gets a `Tailscale` row after `File sharing`. Its value is the state: `Off`, `Starting`,
`Log in`, `Waiting for approval`, `Waiting for Wi-Fi`, or this device's address. A opens the submenu; B goes back to
the Network list.

The submenu's rows, on the bottom panel, in the Settings list's look:

| Row | Kind | Shown |
|---|---|---|
| `Tailscale` | ◀ On / Off ▶ | always |
| `Devices` | A opens the devices page; value: how many are online | logged in |
| `Device name` | A opens the keyboard; value: the name | always |
| `Log out` | press A twice (like `Restart`) | logged in |

Later options (on-device MagicDNS, issue to come) become rows here.

The top panel shows the state, whichever row is selected, with the selected row's help under it:

- Off: what Tailscale is, and that turning it on starts it at every boot.
- Starting / Waiting for Wi-Fi: that line.
- Needs login: the QR, large, with the link beside or under it: "Scan with your phone, log in, and approve
  banana-rgds. Or put an auth key in banana/tailscale/authkey on the card." When the login is done the top panel
  turns into the connected state by itself.
- Waiting for approval: "Logged in. Waiting for your tailnet's admin to approve this device."
- Connected: this device's name, address and MagicDNS name, and how to reach it: `ssh root@100.x.y.z`,
  `\\100.x.y.z\banana`.
- An error (a refused auth key, the clock wrong): what happened and what to do.

```
 top panel                                  bottom panel
+------------------------------------+     +------------------------------------+
| TAILSCALE                Log in    |     |  Tailscale                ◀ On ▶   |
|  +---------+  Scan with your phone,|     |  Device name         banana-rgds   |
|  |  QR     |  log in and approve   |     |                                    |
|  |  code   |  banana-rgds.         |     |                                    |
|  +---------+  login.tailscale.com/ |     |                                    |
|               a/1a2b3c4d5e6f       |     |                                    |
|  Or an auth key in                 |     |                                    |
|  banana/tailscale/authkey          |     |                                    |
+------------------------------------+     |  (B) Back              (A) Choose  |
                                           +------------------------------------+
```

The devices page: the bottom panel lists the tailnet's devices, this one first, each with its name, its IPv4
address and online/offline. The top panel shows the selected device in full: MagicDNS name, IPv4 and IPv6 addresses,
OS, and when it was last seen if offline. B goes back to the submenu.

While the submenu or the devices page is open, banana-ui asks for `status --json` every couple of seconds; nothing
is polled while they are closed. Opening Settings asks once, for the row's value.

## Risks

- **Performance and footprint**:
  - Memory: tailscaled is estimated at 25-50 MB resident on a small tailnet, growing with the number of devices.
    The whole system idles at 95 MB (measured in QEMU, `docs/ARCHITECTURE.md` *What it costs*), so a large share of
    that, while Tailscale is on. Measured in QEMU (plan, step 2): 25 MB, and 15 MB more for the `tailscale up` that
    waits for a login, only while it waits. Not measured on the device (M14 skipped, plan *Deviations* 2026-10-20).
  - CPU while idle: estimated well under 1% on average; keepalives with the relay and peers every few seconds to tens
    of seconds. These also wake the Wi-Fi radio: the battery cost is likely larger than the CPU's. Not measured (M14,
    M15 skipped).
  - Under traffic: WireGuard runs on the CPU; a big transfer keeps a good share of one A55 core busy (the Wi-Fi limits
    the speed first).
  - Frame time: none outside the Tailscale page; the page draws the QR once into a texture (the login page draws in
    0.06 ms, headless).
  - Rootfs: measured (plan, step 1): `tailscaled` is 15.7 MB stripped; the squashfs grows by 7.26 MB of the 256 MB
    slot (161.97 to 169.23 MB), the update package by 7.25 MB, the `.img.gz` by 6.2 MB. `tun.ko` is 77 KB.
  - Boot: tailscaled starts in `rcS`'s background subshell, beside banana-ui's start; nothing waits for it.
    Measured on the RG DS Plus (plan, M5): `Running` about 4-5 s after the menu's first frame, most of it waiting for
    Wi-Fi.
  - Build: Go's prebuilt compiler and Tailscale's modules are downloaded; `kiwami-tailscale` itself builds in 17 s
    once they are.
- **Conflicts with upstream**: `settings.cpp` changes often upstream and gets ~10 hook lines; conflicts there are
  likely but each is one line to put back (rerere remembers). `settings.h`, `ui/Makefile`, `rcS`, `rcK` one line each.
  The share script's veto list is patched at build time, so an upstream change to that line must be checked by hand
  after a merge (`docs/FORK.md`, *What the fork shadows*).
- **Hardware and boards**: the same on `rgds` and `rgds-plus`. Only the device can show: reconnection after a sleep
  (Wi-Fi unloaded), tailscaled's state on exFAT, memory and battery cost, the QR scanned from each panel.
- **Go and Tailscale versions**: 1.98.8 is three stable releases behind and only a newer Go moves it. Tailscale's
  servers keep accepting older clients for a long time, but security fixes in newer releases wait for Buildroot's Go.
- **Kernel module**: `tun` built as a module must load from the image's modules (`modprobe tun`); a kernel built
  without the fragment applied (an output dir not reconfigured) would lack it. The helper reports that in its log.
